#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DreamPassSubsystem.h"
#include "DreamPassTypes.h"

#include "DreamPassComponent.generated.h"

class UDreamPassPipeline;

/** Which views a UDreamPassComponent runs its pipeline in. */
UENUM(BlueprintType)
enum class EDreamPassComponentScope : uint8
{
	/** Every view of the world, wherever the owner is. */
	World,
	/** Only the views whose view target is the owner: the player looking through it, and anyone spectating it. */
	ViewTarget,
};

/**
 * Runs a Custom Pass pipeline from any actor: in every view of its world, or only in the views of the players whose view
 * target the actor is -- a pawn's own effect, seen by whoever plays or spectates it. Its overrides blend with every other
 * activation of the same pipeline in ascending Priority, each by its weight (UDreamPassSubsystem::ResolveView).
 *
 * A source of the world's UDreamPassSubsystem while it is registered -- in editor worlds too -- and asked about every view
 * on the game thread each frame. Nothing is cached: a property set from a Blueprint takes effect on the next frame.
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class DREAMSHADERPASS_API UDreamPassComponent : public UActorComponent, public IDreamPassActivationSource
{
	GENERATED_BODY()

public:
	/** The pipeline the component runs. None: the component does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	TObjectPtr<UDreamPassPipeline> Pipeline = nullptr;

	/** Off: the component applies nowhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass")
	bool bEnabled = true;

	/** Where the component's overrides sit among every other activation of the same pipeline: higher wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	float Priority = 0.0f;

	/**
	 * 0: no effect, 1: full effect. The strength the overrides blend in with, and the pipeline's weight in the view unless
	 * another source gives it more.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Weight = 1.0f;

	/** Every view of the world, or only the views whose view target is the owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	EDreamPassComponentScope Scope = EDreamPassComponentScope::World;

	/**
	 * Only that local player's views, numbered as FSceneView::PlayerIndex numbers them (the controller id).
	 * -1: any view, editor viewports included.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass", meta = (ClampMin = "-1"))
	int32 PlayerIndex = -1;

	/** The parameter values the component runs the pipeline with. One left out keeps what lower priorities give it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass")
	TArray<FDreamPassParameterOverride> Overrides;

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void SetEnabled(bool bInEnabled);

	/** Clamped to 0..1. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void SetWeight(float InWeight);

	/** Sets, or adds, one parameter override. False when the pipeline has no such parameter, or not of the value's type. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool SetParameter(FName Name, const FDreamPassParameterValue& Value);

	/** A Float parameter. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool SetFloatParameter(FName Name, float Value);

	/** A Float4 parameter (RGBA), or a Float3 one (RGB), whichever the pipeline declares. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool SetColorParameter(FName Name, FLinearColor Value);

	//~ IDreamPassActivationSource
	virtual void GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const override;

protected:
	//~ UActorComponent
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};
