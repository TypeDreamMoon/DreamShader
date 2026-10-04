#pragma once

#include "CoreMinimal.h"
#include "DreamPassSubsystem.h"
#include "DreamPassTypes.h"
#include "GameFramework/Volume.h"

#include "DreamPassVolume.generated.h"

class UDreamPassPipeline;

/**
 * Runs a Custom Pass pipeline in the views inside it, the way a post-process volume applies its settings: BlendWeight
 * inside, fading to 0 over BlendRadius outside, everywhere when unbound. However many volumes and other sources ask for
 * one pipeline, it runs once per view; their overrides blend in ascending Priority, each by its weight there
 * (UDreamPassSubsystem::ResolveView).
 *
 * A source of the world's UDreamPassSubsystem while its components are registered -- in editor worlds too, so an editor
 * viewport inside the volume shows the pipeline -- and asked about every view on the game thread each frame. Nothing is
 * cached: an edit, a move or a Blueprint call takes effect on the next frame. The shape is the brush's: a volume spawned
 * at runtime has none, and applies only when unbound.
 */
UCLASS(hidecategories = (Advanced, Collision, Volume, Brush, Attachment))
class DREAMSHADERPASS_API ADreamPassVolume : public AVolume, public IDreamPassActivationSource
{
	GENERATED_BODY()

public:
	ADreamPassVolume(const FObjectInitializer& ObjectInitializer);

	/** The pipeline the volume runs. None: the volume does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	TObjectPtr<UDreamPassPipeline> Pipeline = nullptr;

	/** Off: the volume applies nowhere. In the editor, hiding the volume also takes it out of the editor viewports. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass")
	bool bEnabled = true;

	/** Applies to every view of the world, wherever it is, rather than to the views inside the volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass", meta = (DisplayName = "Infinite Extent (Unbound)"))
	bool bUnbound = false;

	/** Where the volume's overrides sit among every other activation of the same pipeline: higher wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass")
	float Priority = 0.0f;

	/** How far outside the volume, in cm, its weight fades from BlendWeight to 0. Below 1, the edge is hard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Pass", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6000.0", EditCondition = "!bUnbound"))
	float BlendRadius = 100.0f;

	/** 0: no effect, 1: full effect, inside the volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendWeight = 1.0f;

	/** The parameter values the volume runs the pipeline with. A parameter left out keeps what lower priorities give it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass")
	TArray<FDreamPassParameterOverride> Overrides;

	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void SetEnabled(bool bInEnabled);

	/** Clamped to 0..1. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void SetBlendWeight(float InBlendWeight);

	/** Sets, or adds, one parameter override. False when the pipeline has no such parameter, or not of the value's type. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	bool SetParameter(FName Name, const FDreamPassParameterValue& Value);

	/** The weight a view at Location gets from the volume, BlendWeight and BlendRadius included; bEnabled is not looked at. */
	float GetWeightAt(const FVector& Location) const;

	//~ IDreamPassActivationSource
	virtual void GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& OutActivations) const override;

	//~ AActor
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override;
#endif
};
