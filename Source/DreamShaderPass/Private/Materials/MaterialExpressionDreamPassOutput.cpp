#include "Materials/MaterialExpressionDreamPassOutput.h"

#include "DreamPassTypes.h"

#include "Materials/MaterialFunctionInterface.h"

#if WITH_EDITOR
#include "MaterialCompiler.h"
#endif

#define LOCTEXT_NAMESPACE "DreamShader.Pass.OutputNode"

// Output0..Output3 are four properties, written out by hand; the mesh pass has as many colour targets.
static_assert(UE::DreamPass::MaxMeshOutputs == 4, "UMaterialExpressionDreamPassOutput declares one pin per mesh pass output.");

UMaterialExpressionDreamPassOutput::UMaterialExpressionDreamPassOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	struct FConstructorStatics
	{
		FText NAME_DreamShader;
		FConstructorStatics()
			: NAME_DreamShader(LOCTEXT("MenuCategory", "DreamShader"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_DreamShader);

	// A custom output is a root of the graph: pins in, nothing out.
	Outputs.Reset();
#endif
}

FExpressionInput* UMaterialExpressionDreamPassOutput::GetPassInput(int32 OutputIndex)
{
	switch (OutputIndex)
	{
	case 0: return &Output0;
	case 1: return &Output1;
	case 2: return &Output2;
	case 3: return &Output3;
	default: return nullptr;
	}
}

#if WITH_EDITOR

int32 UMaterialExpressionDreamPassOutput::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
#if DREAMSHADER_WITH_CUSTOM_PASS
	// The legacy translator calls this once per output, 0 to GetNumOutputs() - 1, in pixel frequency and with no material
	// property being compiled (E/Private/Materials/HLSLMaterialTranslator.cpp:916-932). CustomOutput turns each call into
	// `#define HAVE_GetDreamPassOutput<i> 1` and `MaterialFloat4 GetDreamPassOutput<i>(inout FMaterialPixelParameters)`
	// (:14474-14494, :14568-14576), which the mesh pass pixel shader calls; the same pass also emits
	// `#define NUM_MATERIAL_OUTPUTS_GETDREAMPASSOUTPUT 4` (:903-906).
	FExpressionInput* Input = GetPassInput(OutputIndex);
	if (!Input)
	{
		return INDEX_NONE;
	}

	// An open pin writes zero instead of leaving its function out: every output a mesh pass binds exists, whichever pins
	// the material wired, so `HAVE_GetDreamPassOutput<i>` in the pass only tells a material with this node from one
	// without it. Traced, so a dangling reroute counts as open, as the engine's own nodes count it.
	if (!Input->GetTracedInput().Expression)
	{
		return Compiler->CustomOutput(this, OutputIndex, Compiler->Constant4(0.0f, 0.0f, 0.0f, 0.0f));
	}

	const int32 Code = Input->Compile(Compiler);
	if (Code == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// ForceCast takes numbers only and its own error says nothing about where the value was going.
	const EMaterialValueType Type = Compiler->GetType(Code);
	if (!IsNumericType(Type) && Type != MCT_StaticBool)
	{
		return Compiler->Errorf(TEXT("%s"), *FText::Format(
			LOCTEXT("NotANumber", "Dream Pass Output: {0} takes a float1 to float4 value; a Substrate BSDF, material attributes or a texture cannot be written to a buffer."),
			FText::FromName(GetInputName(OutputIndex))).ToString());
	}

	// Always a float4: a scalar is replicated, a float2 or float3 is padded with zeros and an LWC value is demoted -- ForceCast
	// without MFCF_ExactMatch (HLSLMaterialTranslator.cpp:4764-4801). CustomOutput declares the function with the code's own
	// type (:14575), so this cast is also what gives every GetDreamPassOutput<i> the same MaterialFloat4 signature.
	return Compiler->CustomOutput(this, OutputIndex, Compiler->ForceCast(Code, MCT_Float4));
#else
	return Compiler->Errorf(TEXT("%s"), *LOCTEXT("NeedsEngine", "Dream Pass Output needs Unreal Engine 5.8 or later.").ToString());
#endif
}

void UMaterialExpressionDreamPassOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Dream Pass Output"));
}

FText UMaterialExpressionDreamPassOutput::GetKeywords() const
{
	return LOCTEXT("Keywords", "dream pass custom pass mesh pass output dreamshader");
}

#if DREAMSHADER_WITH_CUSTOM_PASS

EMaterialValueType UMaterialExpressionDreamPassOutput::GetInputValueType(int32 InputIndex)
{
	// What the pin is cast to, not all it accepts: the material editor connects any number to it
	// (CanConnectMaterialValueTypes, MaterialExpressions.cpp:554-589), and DreamShader hands a narrower vector to a typed
	// pin as it is. Compile does the widening. Never an assert: DreamShader's catalog asks the CDO for every pin GetInput reports.
	return InputIndex >= 0 && InputIndex < UE::DreamPass::MaxMeshOutputs ? MCT_Float4 : MCT_Unknown;
}

void UMaterialExpressionDreamPassOutput::GetShaderTags(TArray<FName>& ShaderTagsOut)
{
	// The tag is the mesh pass shaders' compile gate (FDreamMeshPassVS/PS::ShouldCompilePermutation): they are compiled
	// for a material only when it carries it. Tags are gathered from every expression the material reaches, the ones in
	// the material functions it calls included (E/Private/Materials/MaterialCachedData.cpp:430-458, 505-508), but custom
	// outputs are compiled from the material's own graph alone (Material.cpp:6623-6633, reached through
	// FMaterialResource::GatherCustomOutputExpressions, Material.cpp:331-333, from HLSLMaterialTranslator.cpp:1479-1481).
	// A node inside a function would tag a material that writes nothing -- mesh pass shaders compiled for nothing, and a
	// pass drawing objects with their own material taking it for one that has outputs. So the tag follows the data: none
	// from a function. The editor does not offer the node there (IsAllowedIn); DreamShader reports a `.dss` that puts it there.
	if (!GetTypedOuter<UMaterialFunctionInterface>())
	{
		ShaderTagsOut.AddUnique(UE::DreamPass::MaterialShaderTag);
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS

#endif // WITH_EDITOR

int32 UMaterialExpressionDreamPassOutput::GetNumOutputs() const
{
	// Always all four, wired or not (see Compile).
	return UE::DreamPass::MaxMeshOutputs;
}

FString UMaterialExpressionDreamPassOutput::GetFunctionName() const
{
	// The translator adds the output index and nothing else -- no "Get" -- so the engine's own custom outputs spell it into
	// the name (GetRenderTracePhysicalMaterial). Upper-cased, it is the NUM_MATERIAL_OUTPUTS_GETDREAMPASSOUTPUT define.
	return TEXT("GetDreamPassOutput");
}

#if DREAMSHADER_WITH_CUSTOM_PASS

int32 UMaterialExpressionDreamPassOutput::GetMaxOutputs() const
{
	return UE::DreamPass::MaxMeshOutputs;
}

FString UMaterialExpressionDreamPassOutput::GetDisplayName() const
{
	return TEXT("Dream Pass Output");
}

#if WITH_EDITOR
EShaderFrequency UMaterialExpressionDreamPassOutput::GetShaderFrequency(uint32 OutputIndex)
{
	// The 5.6 per-output overload (the argument-less one is deprecated): every output is read by the mesh pass pixel shader.
	return SF_Pixel;
}
#endif

bool UMaterialExpressionDreamPassOutput::IsAllowedIn(const UObject* MaterialOrFunction) const
{
	// In a material function the node would compile to nothing (see GetShaderTags), so the material editor neither offers
	// it there nor creates or pastes it there: the palette, node creation and paste all ask this with the function being
	// edited (Engine/Source/Editor/MaterialEditor/Private/MaterialEditorUtilities.cpp:724, MaterialEditor.cpp:5937;
	// E/Private/Materials/MaterialExpressions.cpp:893), and the exported IsAllowedExpressionType with the UMaterialFunction
	// CDO (:422-428). Both are UMaterialFunctionInterfaces.
	if (MaterialOrFunction && MaterialOrFunction->IsA<UMaterialFunctionInterface>())
	{
		return false;
	}
	return Super::IsAllowedIn(MaterialOrFunction);
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS

#undef LOCTEXT_NAMESPACE
