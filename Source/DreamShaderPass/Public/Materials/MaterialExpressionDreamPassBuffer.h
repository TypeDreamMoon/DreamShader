#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialExpression.h"
#include "UObject/ObjectPtr.h"

#include "MaterialExpressionDreamPassBuffer.generated.h"

class UDreamPassPipeline;
class UTextureRenderTarget2D;

/**
 * Reads a buffer a DreamShader Custom Pass pipeline exports (`Export = true` in its `.dsp`): the render target the
 * pipeline copies the buffer into after its last writer, sampled as a float4. Without Coordinates it reads at the
 * viewport UV, where a buffer of the view's size lines up with the screen. In a vertex shader (World Position Offset)
 * it reads mip 0.
 *
 * What a material sees is the latest picture of the buffer: this frame's when the pipeline wrote it earlier in the
 * frame than the material renders, the previous frame's otherwise.
 * `UE.DreamPassBuffer(Pipeline = ..., Buffer = "...", Coordinates = ...)` in a `.dss`.
 */
UCLASS(MinimalAPI, collapsecategories, hidecategories = Object)
class UMaterialExpressionDreamPassBuffer : public UMaterialExpression
{
	GENERATED_BODY()

public:
	UMaterialExpressionDreamPassBuffer(const FObjectInitializer& ObjectInitializer);

	/** Where to read, in the render target's UV. Defaults to the viewport UV. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput Coordinates;

	/**
	 * The pipeline that exports the buffer. A hard reference: loading the material loads the pipeline and its render targets,
	 * so the one it samples always resolves; the material lists that render target among its textures, which cooks it too.
	 */
	UPROPERTY(EditAnywhere, Category = "Dream Pass Buffer")
	TObjectPtr<UDreamPassPipeline> Pipeline;

	/** A buffer of Pipeline declared with `Export = true`. */
	UPROPERTY(EditAnywhere, Category = "Dream Pass Buffer")
	FName Buffer;

	/**
	 * Where the sampler comes from. From Texture Asset, the default as on a Texture Sample node, uses the render target's own
	 * address and filter settings, the ones Blueprints, UMG and Niagara read it with. A shared sampler (Wrap or Clamp) does
	 * not use up one of the material's sampler slots.
	 */
	UPROPERTY(EditAnywhere, Category = "Dream Pass Buffer")
	TEnumAsByte<ESamplerSourceMode> SamplerSource;

	/**
	 * The render target buffer InBuffer of InPipeline is exported to, or null with the reason in OutProblem: no pipeline, no
	 * buffer named, a built-in buffer, a buffer the pipeline does not have, one that is not exported, one of a format a
	 * material cannot sample (integer or depth), or an export the pipeline has no render target for. Compile reports
	 * OutProblem as the material error; DreamShader's `.dss` checks can call this too, so the two cannot disagree.
	 */
	static DREAMSHADERPASS_API UTextureRenderTarget2D* ResolveExportTarget(const UDreamPassPipeline* InPipeline, FName InBuffer, FText* OutProblem = nullptr);

	/**
	 * ResolveExportTarget's refusal of a built-in buffer (SceneColor, CustomDepth, ...), in its words, for a caller that has
	 * no pipeline asset to hand it yet -- DreamShader's `.dss` check reading the pipeline's `.dsp` instead -- so that both
	 * paths say the same thing. PipelineName is the pipeline asset's name.
	 */
	static DREAMSHADERPASS_API FText DescribeBuiltinBufferRead(FName InBuffer, const FText& PipelineName);

	// Legacy translator only. There is no Build(MIR::FEmitter&): MIR::FEmitter is not exported
	// (E/Public/Materials/MaterialIREmitter.h:308), so a material that turns the new translator on gets the base class's
	// "Unsupported material expression." (E/Private/Materials/MaterialExpressionsToMIR.cpp:332-335). DreamShader refuses
	// bEnableNewHLSLGenerator on a material that uses this node before it gets that far.

#if WITH_EDITOR
	//~ Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FText GetKeywords() const override;
#if DREAMSHADER_WITH_CUSTOM_PASS
	virtual EMaterialValueType GetInputValueType(int32 InputIndex) override;
	virtual EMaterialValueType GetOutputValueType(int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface

#if DREAMSHADER_WITH_CUSTOM_PASS
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
#endif
#endif

#if DREAMSHADER_WITH_CUSTOM_PASS
	//~ Begin UMaterialExpression Interface
	/** The export target, so the material's list of referenced textures holds it: Compiler->Texture finds it there. */
	virtual UObject* GetReferencedTexture() const override;
	virtual bool CanReferenceTexture() const override { return true; }
	//~ End UMaterialExpression Interface
#endif
};
