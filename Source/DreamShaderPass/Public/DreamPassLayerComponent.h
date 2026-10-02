#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/WeakObjectPtr.h"

#include "DreamPassLayerComponent.generated.h"

class UPrimitiveComponent;

/**
 * Gives the primitives of its actor pass layers, which a mesh pass selects with `Filter = Layer(Name)`.
 *
 * The layers are the names of Project Settings > DreamShader Custom Pass > Layer Names. Every primitive component of
 * the actor receives them, or -- with All Primitives off -- only those that carry Component Tag. The layers are added
 * to whatever else gives a primitive layers, and taken away again when this component is unregistered, so two layer
 * components can share an actor; they should not share a layer.
 *
 * The primitives are gathered when the component registers and when Refresh is called: one added to the actor later
 * gets its layers from a Refresh.
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent, DisplayName = "Dream Pass Layers"))
class DREAMSHADERPASS_API UDreamPassLayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDreamPassLayerComponent();

	/** The pass layers the primitives are in: names from the project settings' Layer Names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass", meta = (GetOptions = "GetLayerNameOptions"))
	TArray<FName> Layers;

	/** Every primitive component of the actor. Off: only those with Component Tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass")
	bool bAllPrimitives = true;

	/** With All Primitives off, the component tag a primitive needs to receive the layers. None selects no primitive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Pass", meta = (EditCondition = "!bAllPrimitives"))
	FName ComponentTag;

	/** Replaces the layers and applies them at once. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void SetLayers(const TArray<FName>& NewLayers);

	/** Gathers the actor's primitives again and gives them the layers: after a primitive was added or a tag changed. */
	UFUNCTION(BlueprintCallable, Category = "DreamShader|Custom Pass")
	void Refresh();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** The details panel's choices for Layers. */
	UFUNCTION()
	TArray<FName> GetLayerNameOptions() const;

	void ApplyLayers();
	void ReleaseLayers();

	/** A primitive this component gave layers, and the bits it added, so exactly those are taken away again. */
	struct FAppliedLayers
	{
		TWeakObjectPtr<UPrimitiveComponent> Primitive;
		uint32 Mask = 0;
	};

	TArray<FAppliedLayers> Applied;
};
