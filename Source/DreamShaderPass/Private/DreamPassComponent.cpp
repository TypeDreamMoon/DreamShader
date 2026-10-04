#include "DreamPassComponent.h"

#include "DreamPassPipeline.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UDreamPassComponent::OnRegister()
{
	Super::OnRegister();

	// Components register in editor and game worlds alike, and the world is still set while OnUnregister runs
	// (E/Private/Components/ActorComponent.cpp:2144-2150), so the two calls pair up. Registering twice only updates the
	// entry. A world without the subsystem -- an editor preview, a Blueprint editor's viewport -- has no Custom Pass at
	// all.
	if (UDreamPassSubsystem* Subsystem = UDreamPassSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterSource(this, this);
	}
}

void UDreamPassComponent::OnUnregister()
{
	if (UDreamPassSubsystem* Subsystem = UDreamPassSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterSource(this);
	}

	Super::OnUnregister();
}

void UDreamPassComponent::GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const
{
	const float ClampedWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
	if (!bEnabled || !Pipeline || ClampedWeight <= 0.0f)
	{
		return;
	}

	if (PlayerIndex != INDEX_NONE && PlayerIndex != View.PlayerIndex)
	{
		return;
	}

	if (Scope == EDreamPassComponentScope::ViewTarget)
	{
		// The view's ViewActor is the player controller's view target (E/Private/LocalPlayer.cpp:874), and the query carries
		// its UObject id (E/Private/SceneView.cpp:780-793). An editor viewport or a scene capture has none, so it never
		// matches.
		const AActor* Owner = GetOwner();
		if (!Owner || View.ViewActorUniqueId == 0 || View.ViewActorUniqueId != Owner->GetUniqueID())
		{
			return;
		}
	}

	FDreamPassActivation& Activation = OutActivations.AddDefaulted_GetRef();
	Activation.Pipeline = Pipeline.Get();
	Activation.Priority = Priority;
	Activation.Weight = ClampedWeight;
	Activation.Overrides = Overrides;
}

void UDreamPassComponent::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

void UDreamPassComponent::SetWeight(float InWeight)
{
	Weight = FMath::Clamp(InWeight, 0.0f, 1.0f);
}

bool UDreamPassComponent::SetParameter(FName Name, const FDreamPassParameterValue& Value)
{
	// Checked against the pipeline as UDreamPassSubsystem::SetParameter checks it: an override of a parameter the pipeline
	// does not declare, or of another type, would never apply (ResolveView skips the one, BlendTowards the other).
	const FDreamPassParameterDesc* Desc = Pipeline ? Pipeline->FindParameter(Name) : nullptr;
	if (!Desc || Desc->Default.Type != Value.Type)
	{
		return false;
	}

	if (FDreamPassParameterOverride* Existing = Overrides.FindByPredicate([Name](const FDreamPassParameterOverride& Override) { return Override.Name == Name; }))
	{
		Existing->Value = Value;
	}
	else
	{
		Overrides.Add({ Name, Value });
	}
	return true;
}

bool UDreamPassComponent::SetFloatParameter(FName Name, float Value)
{
	return SetParameter(Name, FDreamPassParameterValue::MakeFloat(Value));
}

bool UDreamPassComponent::SetColorParameter(FName Name, FLinearColor Value)
{
	// A colour is a float3 in a `.dsp` as often as a float4; the value takes the type the pipeline declares, so one call
	// serves both. Any other type is refused by SetParameter.
	const FDreamPassParameterDesc* Desc = Pipeline ? Pipeline->FindParameter(Name) : nullptr;
	const EDreamPassParameterType Type = Desc && Desc->Default.Type == EDreamPassParameterType::Float3
		? EDreamPassParameterType::Float3
		: EDreamPassParameterType::Float4;
	return SetParameter(Name, FDreamPassParameterValue::MakeVector(Type, FVector4f(Value.R, Value.G, Value.B, Value.A)));
}
