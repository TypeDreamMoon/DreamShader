#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionCustomOutput.h"

#include "MaterialExpressionDreamPassOutput.generated.h"

/**
 * The values a DreamShader Custom Pass mesh pass writes for this material: a pass's `write Output<i> = <buffer>;`
 * writes pin Output<i> into that buffer. Each pin takes a float1 to float4 and writes a float4; an open pin writes 0.
 * A mesh pass that draws objects with their own material skips a material without this node.
 *
 * At most one per material, in the material's own graph: the engine compiles no custom output inside a material function.
 * `UE.DreamPassOutput(Output0 = ..., Output1 = ...);` in a `.dss`.
 */
UCLASS(MinimalAPI, collapsecategories, hidecategories = Object)
class UMaterialExpressionDreamPassOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_BODY()

public:
	UMaterialExpressionDreamPassOutput(const FObjectInitializer& ObjectInitializer);

	/** Written by a mesh pass's `write Output0 = <buffer>;`. A float1 is replicated, a float2 or float3 padded with zeros. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput Output0;

	/** Written by a mesh pass's `write Output1 = <buffer>;`. A float1 is replicated, a float2 or float3 padded with zeros. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput Output1;

	/** Written by a mesh pass's `write Output2 = <buffer>;`. A float1 is replicated, a float2 or float3 padded with zeros. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput Output2;

	/** Written by a mesh pass's `write Output3 = <buffer>;`. A float1 is replicated, a float2 or float3 padded with zeros. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput Output3;

	// The pins are found by reflection: UMaterialExpression's constructor caches every FExpressionInput property in
	// declaration order for the default GetInput (E/Private/Materials/MaterialExpressions.cpp:818-831, 1816-1819), and the
	// default GetInputName walks the same properties (:1821-1856), so pin i is Output<i>, named "Output<i>" -- the same i
	// Compile is called with for it.
	//
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
	virtual void GetShaderTags(TArray<FName>& ShaderTagsOut) override;
#endif
	//~ End UMaterialExpression Interface
#endif

#if DREAMSHADER_WITH_CUSTOM_PASS
	virtual bool IsAllowedIn(const UObject* MaterialOrFunction) const override;
#endif

	//~ Begin UMaterialExpressionCustomOutput Interface
	virtual int32 GetNumOutputs() const override;
	virtual FString GetFunctionName() const override;
#if DREAMSHADER_WITH_CUSTOM_PASS
	virtual int32 GetMaxOutputs() const override;
	virtual FString GetDisplayName() const override;
#if WITH_EDITOR
	virtual EShaderFrequency GetShaderFrequency(uint32 OutputIndex) override;
#endif
#endif
	//~ End UMaterialExpressionCustomOutput Interface

private:
	/** Output<OutputIndex>, or null past Output3. */
	FExpressionInput* GetPassInput(int32 OutputIndex);
};
