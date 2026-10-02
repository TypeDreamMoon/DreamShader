#include "DreamPassLayerComponent.h"

#include "DreamPassSettings.h"
#include "DreamPassSubsystem.h"
#include "DreamShaderPassModule.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace UE::DreamPass::Private
{
	/** Layer names already reported as unknown: once per name for the session, however many components give it. */
	static TSet<FName> ReportedUnknownLayers;
}

UDreamPassLayerComponent::UDreamPassLayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDreamPassLayerComponent::SetLayers(const TArray<FName>& NewLayers)
{
	Layers = NewLayers;
	Refresh();
}

void UDreamPassLayerComponent::Refresh()
{
	ReleaseLayers();
	if (IsRegistered())
	{
		ApplyLayers();
	}
}

void UDreamPassLayerComponent::OnRegister()
{
	Super::OnRegister();
	ApplyLayers();
}

void UDreamPassLayerComponent::OnUnregister()
{
	ReleaseLayers();
	Super::OnUnregister();
}

#if WITH_EDITOR
void UDreamPassLayerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Refresh();
}
#endif

TArray<FName> UDreamPassLayerComponent::GetLayerNameOptions() const
{
	// The bit of LayerNames[i] is 1 << i, so only the first MaxLayers names are layers at all.
	const TArray<FName>& Names = UDreamPassSettings::Get().LayerNames;
	TArray<FName> Options;
	for (int32 Index = 0; Index < Names.Num() && Index < UDreamPassSettings::MaxLayers; ++Index)
	{
		if (!Names[Index].IsNone())
		{
			Options.Add(Names[Index]);
		}
	}
	return Options;
}

void UDreamPassLayerComponent::ApplyLayers()
{
	ReleaseLayers();

	// A world without the subsystem -- an asset editor's preview -- runs no pipeline, so its primitives need no layers.
	UDreamPassSubsystem* Subsystem = UDreamPassSubsystem::Get(GetWorld());
	AActor* Owner = GetOwner();
	if (!Subsystem || !Owner)
	{
		return;
	}

	TArray<FName> Unknown;
	const uint32 Mask = UDreamPassSettings::Get().MakeLayerMask(Layers, &Unknown);
	for (const FName Name : Unknown)
	{
		bool bAlreadyReported = false;
		UE::DreamPass::Private::ReportedUnknownLayers.Add(Name, &bAlreadyReported);
		if (!bAlreadyReported)
		{
			UE_LOG(LogDreamPass, Warning, TEXT("DreamPass: %s gives the pass layer '%s', which Project Settings > DreamShader Custom Pass > Layer Names does not have; it is ignored."),
				*GetPathName(), *Name.ToString());
		}
	}

	if (Mask == 0 || (!bAllPrimitives && ComponentTag.IsNone()))
	{
		return;
	}

	TArray<UPrimitiveComponent*> Primitives;
	Owner->GetComponents<UPrimitiveComponent>(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (!Primitive || (!bAllPrimitives && !Primitive->ComponentHasTag(ComponentTag)))
		{
			continue;
		}

		// Only the bits this component adds are remembered, so taking them away again leaves what another source gave.
		const uint32 Existing = Subsystem->GetPrimitiveLayers(Primitive);
		const uint32 Added = Mask & ~Existing;
		if (Added == 0)
		{
			continue;
		}
		Subsystem->SetPrimitiveLayers(Primitive, Existing | Added);
		Applied.Add({ Primitive, Added });
	}
}

void UDreamPassLayerComponent::ReleaseLayers()
{
	if (Applied.IsEmpty())
	{
		return;
	}

	// At world teardown the subsystem may be gone before this component unregisters; its tables went with it.
	if (UDreamPassSubsystem* Subsystem = UDreamPassSubsystem::Get(GetWorld()))
	{
		for (const FAppliedLayers& Entry : Applied)
		{
			if (UPrimitiveComponent* Primitive = Entry.Primitive.Get())
			{
				Subsystem->SetPrimitiveLayers(Primitive, Subsystem->GetPrimitiveLayers(Primitive) & ~Entry.Mask);
			}
		}
	}
	Applied.Reset();
}
