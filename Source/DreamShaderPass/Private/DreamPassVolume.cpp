#include "DreamPassVolume.h"

#include "DreamPassPipeline.h"

#include "Components/BrushComponent.h"
#include "CoreGlobals.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

ADreamPassVolume::ADreamPassVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set up as APostProcessVolume's brush is (E/Private/PostProcessVolume.cpp:13-24): no collision, but a physics body all
	// the same, because AVolume::EncompassesPoint measures the view's distance against the brush's body and finds none
	// without one (E/Private/Volume.cpp:84-117); movable, so the body follows a volume moved at runtime. (No local for the
	// component: ABrush has a member called Brush, and a shadowing local is an error in UE builds.)
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->bAlwaysCreatePhysicsState = true;
	GetBrushComponent()->Mobility = EComponentMobility::Movable;
}

float ADreamPassVolume::GetWeightAt(const FVector& Location) const
{
	// A post-process volume's arithmetic (E/Private/World.cpp:10705-10742, DoPostProcessVolume): BlendWeight inside -- the
	// distance to a convex body is 0 there (E/Classes/Components/PrimitiveComponent.h:2351-2360) -- fading linearly to 0
	// over BlendRadius outside. A distance below 0 means there is no body to measure against: a brush without simple
	// collision, or one whose physics state does not exist yet.
	const float Weight = FMath::Clamp(BlendWeight, 0.0f, 1.0f);
	if (bUnbound)
	{
		return Weight;
	}

	float DistanceToPoint = -1.0f;
	EncompassesPoint(Location, 0.0f, &DistanceToPoint);

	// BlendRadius is writable from Blueprints, where nothing clamps it.
	const float Radius = FMath::Max(BlendRadius, 0.0f);
	if (DistanceToPoint < 0.0f || DistanceToPoint > Radius)
	{
		return 0.0f;
	}

	// The engine divides only by a radius of 1 cm or more; a smaller one is a hard edge.
	if (Radius >= 1.0f)
	{
		return FMath::Clamp(Weight * (1.0f - DistanceToPoint / Radius), 0.0f, 1.0f);
	}
	return Weight;
}

void ADreamPassVolume::GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const
{
	if (!bEnabled || !Pipeline)
	{
		return;
	}

#if WITH_EDITOR
	// As APostProcessVolume::IsPPVEnabled (E/Private/PostProcessVolume.cpp:101-113): outside a game world the volume follows
	// its editor visibility, so hiding it in the outliner shows the viewport without its pipeline. A game world -- PIE
	// included -- ignores it; a volume is always hidden in game, being a brush.
	const UWorld* World = GetWorld();
	const bool bGameWorld = World && World->UsesGameHiddenFlags();
	if (!bGameWorld && (!GIsEditor || IsHiddenEd()))
	{
		return;
	}
#endif

	const float Weight = GetWeightAt(View.ViewLocation);
	if (Weight <= 0.0f)
	{
		return;
	}

	FDreamPassActivation& Activation = OutActivations.AddDefaulted_GetRef();
	Activation.Pipeline = Pipeline.Get();
	Activation.Priority = Priority;
	Activation.Weight = Weight;
	Activation.Overrides = Overrides;
}

void ADreamPassVolume::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

void ADreamPassVolume::SetBlendWeight(float InBlendWeight)
{
	BlendWeight = FMath::Clamp(InBlendWeight, 0.0f, 1.0f);
}

bool ADreamPassVolume::SetParameter(FName Name, const FDreamPassParameterValue& Value)
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

void ADreamPassVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// Called in editor and game worlds alike once every component is registered (E/Classes/GameFramework/Actor.h:3234-3238),
	// and undone by PostUnregisterAllComponents (E/Private/Actor.cpp:5977-5984), which can also come without it -- a removal
	// that finds nothing is harmless. The pair APostProcessVolume enters and leaves its world's volume list from
	// (E/Private/PostProcessVolume.cpp:56-99). Registering twice, after a re-register, only updates the entry. A world
	// without the subsystem -- an editor preview -- has no Custom Pass at all.
	if (UDreamPassSubsystem* Subsystem = UDreamPassSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterSource(this, this);
	}
}

void ADreamPassVolume::PostUnregisterAllComponents()
{
	// The world is null during the exit purge (E/Private/PostProcessVolume.cpp:75-79), when its subsystem is going anyway.
	if (UDreamPassSubsystem* Subsystem = UDreamPassSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterSource(this);
	}

	Super::PostUnregisterAllComponents();
}

#if WITH_EDITOR
void ADreamPassVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// An unbound volume applies everywhere, so World Partition must not stream it in and out with the cell it sits in --
	// as APostProcessVolume does (E/Private/PostProcessVolume.cpp:168-174, E/Classes/Engine/PostProcessVolume.h:85). Every
	// other property is read live by GatherDreamPassActivations; nothing needs registering again.
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ADreamPassVolume, bUnbound) && bUnbound)
	{
		bIsSpatiallyLoaded = false;
	}
}

bool ADreamPassVolume::CanChangeIsSpatiallyLoadedFlag() const
{
	return !bUnbound && Super::CanChangeIsSpatiallyLoadedFlag();
}
#endif
