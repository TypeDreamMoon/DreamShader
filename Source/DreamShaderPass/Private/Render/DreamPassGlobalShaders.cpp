#include "Render/DreamPassFrame.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamShaderPassModule.h"

#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "SceneRenderTargetParameters.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"

/**
 * The HLSL pass slots: one global compute shader and one global pixel shader, each with a permutation per slot.
 *
 * A pass's own HLSL cannot get a global shader type of its own -- types register when this module loads and a late
 * one asserts (RC/Private/Shader.cpp:313-317) -- so every HLSL pass runs in a slot of these two, with the fixed
 * parameters of Shaders/Pass/DreamPass.ush, and the compiler's registry maps the pass's names onto them. The types
 * live at file scope so their registered names are exactly "FDreamPassCS" and "FDreamPassPS": the compiler finds them
 * by those names to recompile them after a registry change.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FDreamPassSlotParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input0)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input1)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input2)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input3)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input4)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input5)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input6)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DP_Input7)
	SHADER_PARAMETER_ARRAY(FVector4f, DP_InputSize, [8])
	SHADER_PARAMETER_ARRAY(FVector4f, DP_InputUVRect, [8])
	SHADER_PARAMETER_SAMPLER(SamplerState, DP_PointClamp)
	SHADER_PARAMETER_SAMPLER(SamplerState, DP_LinearClamp)
	SHADER_PARAMETER_ARRAY(FVector4f, DP_OutputSize, [4])
	SHADER_PARAMETER_ARRAY(FVector4f, DP_Params, [16])
	SHADER_PARAMETER(float, DP_Weight)
	SHADER_PARAMETER(FIntVector4, DP_ViewRect)
	SHADER_PARAMETER(FUintVector4, DP_DispatchSize)
	SHADER_PARAMETER(FVector4f, DP_Time)
END_SHADER_PARAMETER_STRUCT()

static_assert(UE::DreamPass::MaxSlotInputs == 8 && UE::DreamPass::MaxSlotOutputs == 4 && UE::DreamPass::MaxSlotParamVectors == 16,
	"FDreamPassSlotParameters and Shaders/Pass/DreamPass.ush hard-code these sizes.");

class FDreamPassCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDreamPassCS);
	SHADER_USE_PARAMETER_STRUCT(FDreamPassCS, FGlobalShader);

	class FSlot : SHADER_PERMUTATION_RANGE_INT("DP_SLOT", 0, DREAMSHADER_PASS_COMPUTE_SLOTS);
	using FPermutationDomain = TShaderPermutationDomain<FSlot>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FDreamPassSlotParameters, Common)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DP_Output0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DP_Output1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DP_Output2)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DP_Output3)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Every slot, used or not: the list must not depend on anything a build reads at startup, because cooked builds
		// evaluate it again and a permutation missing there is fatal (E/Private/ShaderCompiler/ShaderCompiler.cpp:4935-4975).
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

class FDreamPassPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDreamPassPS);
	SHADER_USE_PARAMETER_STRUCT(FDreamPassPS, FGlobalShader);

	class FSlot : SHADER_PERMUTATION_RANGE_INT("DP_SLOT", 0, DREAMSHADER_PASS_PIXEL_SLOTS);
	using FPermutationDomain = TShaderPermutationDomain<FSlot>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FDreamPassSlotParameters, Common)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDreamPassCS, "/Plugin/DreamShader/Pass/DreamPassCompute.usf", "DreamPassMainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDreamPassPS, "/Plugin/DreamShader/Pass/DreamPassPixel.usf", "DreamPassMainPS", SF_Pixel);

namespace UE::DreamPass
{
	namespace Private
	{
		static void SetSlotInput(FDreamPassSlotParameters& Parameters, int32 Index, FRDGTextureRef Texture)
		{
			switch (Index)
			{
			case 0: Parameters.DP_Input0 = Texture; break;
			case 1: Parameters.DP_Input1 = Texture; break;
			case 2: Parameters.DP_Input2 = Texture; break;
			case 3: Parameters.DP_Input3 = Texture; break;
			case 4: Parameters.DP_Input4 = Texture; break;
			case 5: Parameters.DP_Input5 = Texture; break;
			case 6: Parameters.DP_Input6 = Texture; break;
			case 7: Parameters.DP_Input7 = Texture; break;
			default: break;
			}
		}

		static void SetSlotOutput(FDreamPassCS::FParameters& Parameters, int32 Index, FRDGTextureUAVRef UAV)
		{
			switch (Index)
			{
			case 0: Parameters.DP_Output0 = UAV; break;
			case 1: Parameters.DP_Output1 = UAV; break;
			case 2: Parameters.DP_Output2 = UAV; break;
			case 3: Parameters.DP_Output3 = UAV; break;
			default: break;
			}
		}

		static FVector4f MakeSizeVector(FIntPoint Extent)
		{
			const float X = float(FMath::Max(Extent.X, 1));
			const float Y = float(FMath::Max(Extent.Y, 1));
			return FVector4f(X, Y, 1.0f / X, 1.0f / Y);
		}

		/** What every slot binds: inputs, outputs' sizes, parameters, weight, time, the view and the scene textures. */
		static void FillSlotParameters(FExecuteContext& Context, FDreamPassSlotParameters& Parameters, TConstArrayView<FScreenPassTexture> Inputs, TConstArrayView<FScreenPassRenderTarget> Outputs)
		{
			FRDGBuilder& GraphBuilder = Context.GraphBuilder;
			const FSceneView& View = Context.GetView();
			const FSnapshotPass& Pass = Context.Pass;

			// Null at BeginView, where the renderer has not created it yet; the registry makes any use of `View` in such
			// a pass a compile error, so nothing reads the empty binding.
			Parameters.View = View.ViewUniformBuffer;
			Parameters.SceneTextures = Context.Injection.SceneTextures
				? Context.Injection.SceneTextures
				: CreateSceneTextureUniformBuffer(GraphBuilder, nullptr, View.GetFeatureLevel(), ESceneTextureSetupMode::None);

			FRDGTextureRef Black = GSystemTextures.GetBlackDummy(GraphBuilder);
			for (int32 Index = 0; Index < MaxSlotInputs; ++Index)
			{
				const bool bBound = Inputs.IsValidIndex(Index) && Inputs[Index].IsValid();
				FRDGTextureRef Texture = bBound ? Inputs[Index].Texture : Black;
				SetSlotInput(Parameters, Index, Texture);

				const FIntPoint Extent = Texture->Desc.Extent;
				Parameters.DP_InputSize[Index] = MakeSizeVector(Extent);
				if (bBound)
				{
					const FIntRect Rect = Inputs[Index].ViewRect;
					const FVector2f InvExtent(1.0f / float(FMath::Max(Extent.X, 1)), 1.0f / float(FMath::Max(Extent.Y, 1)));
					Parameters.DP_InputUVRect[Index] = FVector4f(Rect.Min.X * InvExtent.X, Rect.Min.Y * InvExtent.Y, Rect.Max.X * InvExtent.X, Rect.Max.Y * InvExtent.Y);
				}
				else
				{
					Parameters.DP_InputUVRect[Index] = FVector4f(0.0f, 0.0f, 1.0f, 1.0f);
				}
			}

			Parameters.DP_PointClamp = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Parameters.DP_LinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			for (int32 Index = 0; Index < MaxSlotOutputs; ++Index)
			{
				Parameters.DP_OutputSize[Index] = Outputs.IsValidIndex(Index) && Outputs[Index].IsValid()
					? MakeSizeVector(Outputs[Index].Texture->Desc.Extent)
					: FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
			}

			for (int32 Index = 0; Index < MaxSlotParamVectors; ++Index)
			{
				Parameters.DP_Params[Index] = Pass.SlotParams.IsValidIndex(Index) ? Pass.SlotParams[Index] : FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
			}

			Parameters.DP_Weight = Pass.Weight;

			const FIntRect OutputRect = Outputs.Num() > 0 && Outputs[0].IsValid() ? Outputs[0].ViewRect : Context.Injection.SceneViewRect;
			Parameters.DP_ViewRect = FIntVector4(OutputRect.Min.X, OutputRect.Min.Y, OutputRect.Max.X, OutputRect.Max.Y);
			Parameters.DP_DispatchSize = FUintVector4(uint32(FMath::Max(OutputRect.Width(), 0)), uint32(FMath::Max(OutputRect.Height(), 0)), 1, 0);

			if (View.Family)
			{
				Parameters.DP_Time = FVector4f(
					float(View.Family->Time.GetWorldTimeSeconds()),
					View.Family->Time.GetDeltaWorldTimeSeconds(),
					float(View.Family->Time.GetRealTimeSeconds()),
					float(View.Family->FrameNumber));
			}
			else
			{
				Parameters.DP_Time = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}

		static bool ResolveSlotInputs(FExecuteContext& Context, TArray<FScreenPassTexture, TInlineAllocator<MaxSlotInputs>>& OutInputs)
		{
			for (const FDreamPassBufferBinding& Read : Context.Pass.Reads)
			{
				if (OutInputs.Num() == MaxSlotInputs)
				{
					break;
				}
				const FScreenPassTexture Texture = ResolveRead(Context, Read);
				if (!Texture.IsValid())
				{
					return false;
				}
				OutInputs.Add(Texture);
			}
			return true;
		}

		/** The number of threads a compute pass asks for: a buffer's extent times DispatchScale, or a fixed count. */
		static FIntVector GetDispatchThreads(FExecuteContext& Context, TConstArrayView<FScreenPassRenderTarget> Outputs)
		{
			const FSnapshotPass& Pass = Context.Pass;
			if (Pass.DispatchMode == EDreamPassDispatchMode::Fixed)
			{
				return FIntVector(FMath::Max(Pass.DispatchSize.X, 1), FMath::Max(Pass.DispatchSize.Y, 1), FMath::Max(Pass.DispatchSize.Z, 1));
			}

			FIntPoint Size = Outputs.Num() > 0 ? Outputs[0].ViewRect.Size() : Context.Injection.SceneViewRect.Size();
			if (!Pass.DispatchBuffer.IsNone())
			{
				FDreamPassBufferBinding Binding;
				Binding.Buffer = Pass.DispatchBuffer;
				const FScreenPassTexture Texture = ResolveRead(Context, Binding);
				if (Texture.IsValid())
				{
					Size = Texture.ViewRect.Size();
				}
			}
			return FIntVector(
				FMath::Max(FMath::CeilToInt(float(Size.X) * Pass.DispatchScale), 1),
				FMath::Max(FMath::CeilToInt(float(Size.Y) * Pass.DispatchScale), 1),
				1);
		}

		static void CommitSceneColorWrites(FExecuteContext& Context, TConstArrayView<FScreenPassRenderTarget> Outputs)
		{
			for (int32 Index = 0; Index < Context.Pass.Writes.Num() && Index < Outputs.Num(); ++Index)
			{
				if (IsSceneColorWrite(Context, Context.Pass.Writes[Index]))
				{
					CommitSceneColor(Context, Outputs[Index]);
				}
			}
		}
	}

	bool ExecuteComputePass(FExecuteContext& Context)
	{
		const FSnapshotPass& Pass = Context.Pass;
		const FSceneView& View = Context.GetView();
		FRDGBuilder& GraphBuilder = Context.GraphBuilder;

		if (Pass.Slot < 0 || Pass.Slot >= GetComputeSlotCount())
		{
			return false;
		}

		FDreamPassCS::FPermutationDomain Permutation;
		Permutation.Set<FDreamPassCS::FSlot>(Pass.Slot);
		TShaderMapRef<FDreamPassCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()), Permutation);
		if (!Shader.IsValid())
		{
			WarnOnce(Context.Pipeline.DebugName + TEXT(".") + Pass.Name.ToString() + TEXT(".NoShader"),
				FString::Printf(TEXT("DreamPass: %s.%s: compute slot %d has no compiled shader on this platform; the pass is skipped."), *Context.Pipeline.DebugName, *Pass.Name.ToString(), Pass.Slot));
			return false;
		}

		TArray<FScreenPassTexture, TInlineAllocator<MaxSlotInputs>> Inputs;
		if (!Private::ResolveSlotInputs(Context, Inputs))
		{
			return false;
		}

		TArray<FScreenPassRenderTarget, TInlineAllocator<MaxSlotOutputs>> Outputs;
		for (const FDreamPassBufferBinding& Write : Pass.Writes)
		{
			if (Outputs.Num() == MaxSlotOutputs)
			{
				break;
			}
			const FScreenPassRenderTarget Target = ResolveWrite(Context, Write);
			if (!Target.IsValid())
			{
				return false;
			}
			// A GBuffer target written in place, or a buffer some raster pass needed render-targetable, may not be
			// UAV-capable; compute writes need it.
			if (!EnumHasAnyFlags(Target.Texture->Desc.Flags, TexCreate_UAV))
			{
				WarnOnce(Context.Pipeline.DebugName + TEXT(".") + Pass.Name.ToString() + TEXT(".") + Write.Buffer.ToString() + TEXT(".NoUAV"),
					FString::Printf(TEXT("DreamPass: %s.%s: '%s' cannot be written by a compute shader here; the pass is skipped."), *Context.Pipeline.DebugName, *Pass.Name.ToString(), *Write.Buffer.ToString()));
				return false;
			}
			Outputs.Add(Target);
		}

		FDreamPassCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDreamPassCS::FParameters>();
		Private::FillSlotParameters(Context, Parameters->Common, Inputs, Outputs);
		for (int32 Index = 0; Index < Outputs.Num(); ++Index)
		{
			Private::SetSlotOutput(*Parameters, Index, GraphBuilder.CreateUAV(Outputs[Index].Texture));
		}

		const FIntVector Threads = Private::GetDispatchThreads(Context, Outputs);
		Parameters->Common.DP_DispatchSize = FUintVector4(uint32(Threads.X), uint32(Threads.Y), uint32(Threads.Z), 0);
		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Threads, Pass.ThreadGroupSize);

		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("DreamPass compute slot %d", Pass.Slot), Shader, Parameters, GroupCount);

		Private::CommitSceneColorWrites(Context, Outputs);
		return true;
	}

	bool ExecuteFullscreenSlotPass(FExecuteContext& Context)
	{
		const FSnapshotPass& Pass = Context.Pass;
		const FSceneView& View = Context.GetView();
		FRDGBuilder& GraphBuilder = Context.GraphBuilder;

		if (Pass.Slot < 0 || Pass.Slot >= GetPixelSlotCount())
		{
			return false;
		}

		FDreamPassPS::FPermutationDomain Permutation;
		Permutation.Set<FDreamPassPS::FSlot>(Pass.Slot);
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
		TShaderMapRef<FDreamPassPS> Shader(ShaderMap, Permutation);
		if (!Shader.IsValid())
		{
			WarnOnce(Context.Pipeline.DebugName + TEXT(".") + Pass.Name.ToString() + TEXT(".NoShader"),
				FString::Printf(TEXT("DreamPass: %s.%s: pixel slot %d has no compiled shader on this platform; the pass is skipped."), *Context.Pipeline.DebugName, *Pass.Name.ToString(), Pass.Slot));
			return false;
		}

		TArray<FScreenPassTexture, TInlineAllocator<MaxSlotInputs>> Inputs;
		if (!Private::ResolveSlotInputs(Context, Inputs))
		{
			return false;
		}

		TArray<FScreenPassRenderTarget, TInlineAllocator<MaxSlotOutputs>> Outputs;
		for (const FDreamPassBufferBinding& Write : Pass.Writes)
		{
			if (Outputs.Num() == MaxSlotOutputs)
			{
				break;
			}
			const FScreenPassRenderTarget Target = ResolveWrite(Context, Write);
			if (!Target.IsValid())
			{
				return false;
			}
			Outputs.Add(Target);
		}
		if (Outputs.IsEmpty())
		{
			return false;
		}

		FDreamPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FDreamPassPS::FParameters>();
		Private::FillSlotParameters(Context, Parameters->Common, Inputs, Outputs);
		for (int32 Index = 0; Index < Outputs.Num(); ++Index)
		{
			// The pass covers its viewport, so what was there before does not matter -- unless the target is shared with
			// another view's rect (the scene colour in split screen), which the viewport keeps out of reach anyway.
			Parameters->RenderTargets[Index] = FRenderTargetBinding(Outputs[Index].Texture, Outputs[Index].LoadAction);
		}

		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("DreamPass pixel slot %d", Pass.Slot), Shader, Parameters, Outputs[0].ViewRect);

		Private::CommitSceneColorWrites(Context, Outputs);
		return true;
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
