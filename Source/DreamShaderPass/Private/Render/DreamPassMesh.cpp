#include "Render/DreamPassMesh.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "DreamPassSubsystem.h"
#include "DreamShaderPassModule.h"
#include "Render/DreamPassFrame.h"

#include "Components/PrimitiveComponent.h"
#include "ConvexVolume.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "MaterialShaderType.h"
#include "MaterialShared.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshMaterialShader.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "PixelShaderUtils.h"
#include "PrimitiveSceneInfo.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RenderCore.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneRendererInterface.h"
#include "SceneRenderTargetParameters.h"
#include "SceneTexturesConfig.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"
#include "SimpleMeshDrawCommandPass.h"
#include "StaticMeshBatch.h"
#include "UnrealEngine.h"

/**
 * Mesh passes: selected primitives drawn again into the pass's buffers, with an override material or their own.
 *
 * Everything here goes through the renderer's public interface (01 §8): the primitives' static mesh batches
 * (FPrimitiveSceneInfo::StaticMeshes, R/Public/PrimitiveSceneInfo.h), a mesh pass processor of our own built with the
 * name constructor -- EMeshPass has no slot for a plugin, so its commands are never cached and are built every frame
 * -- and AddSimpleMeshPass with no instance culling manager, which a plugin cannot reach: every instance of a selected
 * primitive is drawn. The precedents are Landscape's physical material pass
 * (Runtime/Landscape/Private/LandscapePhysicalMaterial.cpp:197-254) and RenderTrace (Plugins/Runtime/RenderTrace).
 *
 * Nanite primitives have no batches to draw (E/Private/Rendering/NaniteResources.cpp:691-693): the StencilMask and
 * AssignStencil policies fill the pass's outputs from the custom stencil instead. Primitives that only have dynamic
 * relevance (CPU skinning, procedural meshes) are not reachable without the renderer's private mesh collector and are
 * skipped (D-6).
 */

// --- shaders ----------------------------------------------------------------------------------------------------------

/**
 * The mesh pass's material shaders (Shaders/Pass/DreamPassMesh.usf). Compiled only for Surface materials that carry
 * UE.DreamPassOutput, whose GetShaderTags gives its material the DreamPass tag (E/Public/Materials/MaterialExpression.h,
 * GetShaderTags; E/Public/MaterialShared.h, FMaterialShaderParameters::MaterialShaderTags). Without that gate every
 * material of the project would compile them for every vertex factory it is used with.
 */
class FDreamMeshPassShader : public FMeshMaterialShader
{
public:
	FDreamMeshPassShader() = default;

	FDreamMeshPassShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		// Deliberately not bIsDefaultMaterial: the default material would then compile these for every vertex factory,
		// and block startup on it. A pass therefore never falls back to the engine's default material; a primitive whose
		// materials have none of these shaders is skipped.
		return Parameters.MaterialParameters.MaterialDomain == MD_Surface
			&& Parameters.MaterialParameters.MaterialShaderTags.Contains(UE::DreamPass::MaterialShaderTag)
			&& IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

class FDreamMeshPassVS : public FDreamMeshPassShader
{
	DECLARE_SHADER_TYPE(FDreamMeshPassVS, MeshMaterial);

public:
	FDreamMeshPassVS() = default;

	FDreamMeshPassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDreamMeshPassShader(Initializer)
	{
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FDreamMeshPassVS, TEXT("/Plugin/DreamShader/Pass/DreamPassMesh.usf"), TEXT("MainVS"), SF_Vertex);

/** Per draw command: the mesh material data, and where the pass's target pixels lie in the view. */
class FDreamMeshPassShaderElementData : public FMeshMaterialShaderElementData
{
public:
	/** DreamPassSvPositionScaleBias of DreamPassMesh.usf: from the pass's target pixels to the view's. */
	FVector4f SvPositionScaleBias = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);
};

class FDreamMeshPassPS : public FDreamMeshPassShader
{
	DECLARE_SHADER_TYPE(FDreamMeshPassPS, MeshMaterial);

public:
	FDreamMeshPassPS() = default;

	FDreamMeshPassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDreamMeshPassShader(Initializer)
	{
		SvPositionScaleBiasParameter.Bind(Initializer.ParameterMap, TEXT("DreamPassSvPositionScaleBias"));
	}

	/** Called by BuildMeshDrawCommands (R/Public/MeshPassProcessor.inl) with the pass's element data. */
	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FDreamMeshPassShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);
		ShaderBindings.Add(SvPositionScaleBiasParameter, ShaderElementData.SvPositionScaleBias);
	}

private:
	LAYOUT_FIELD(FShaderParameter, SvPositionScaleBiasParameter);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FDreamMeshPassPS, TEXT("/Plugin/DreamShader/Pass/DreamPassMesh.usf"), TEXT("MainPS"), SF_Pixel);

/** The Nanite policies' fill: the pass's outputs receive NaniteValue where the custom stencil is selected (Shaders/Pass/DreamPassStencilMask.usf). */
class FDreamMeshPassStencilMaskPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDreamMeshPassStencilMaskPS);
	SHADER_USE_PARAMETER_STRUCT(FDreamMeshPassStencilMaskPS, FGlobalShader);

	/** The pass has a depth target: the fill writes the custom depth, so it is tested like a drawn mesh. */
	class FOutputDepth : SHADER_PERMUTATION_BOOL("DREAMPASS_OUTPUT_DEPTH");
	using FPermutationDomain = TShaderPermutationDomain<FOutputDepth>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER(FVector4f, TargetToSourceScaleBias)
		SHADER_PARAMETER(FIntPoint, SourceMin)
		SHADER_PARAMETER(FIntPoint, SourceMax)
		SHADER_PARAMETER(FVector4f, FillValue)
		SHADER_PARAMETER_ARRAY(FUintVector4, StencilMatch, [2])
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDreamMeshPassStencilMaskPS, "/Plugin/DreamShader/Pass/DreamPassStencilMask.usf", "StencilMaskPS", SF_Pixel);

/** `Depth = TestScene` for targets that do not share the scene depth's pixels: the scene depth brought into theirs. */
class FDreamMeshPassCopyDepthPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDreamMeshPassCopyDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FDreamMeshPassCopyDepthPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceDepth)
		SHADER_PARAMETER(FVector4f, TargetToSourceScaleBias)
		SHADER_PARAMETER(FIntPoint, SourceMin)
		SHADER_PARAMETER(FIntPoint, SourceMax)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDreamMeshPassCopyDepthPS, "/Plugin/DreamShader/Pass/DreamPassStencilMask.usf", "CopyDepthPS", SF_Pixel);

/**
 * The mesh pass's render graph parameters, as LandscapePhysicalMaterial.cpp:197-202 has them: the view, a scene uniform
 * buffer of our own (GetSceneUniformBufferRef, R/Public/SceneRendererInterface.h:70) and the InstanceCullingDrawParams
 * member AddSimpleMeshPass fills. The scene textures are bound too, for a material that samples them (a depth fade, the
 * custom stencil): a material shader that reads them without them bound has nothing at their static slot.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FDreamMeshPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

namespace UE::DreamPass::Private::MeshPass
{
	// --- drawing -------------------------------------------------------------------------------------------------------

	/** One static mesh batch the pass draws, with the material and shaders it is drawn with. */
	struct FDrawItem
	{
		const FMeshBatch* MeshBatch = nullptr;
		const FPrimitiveSceneProxy* Proxy = nullptr;
		int32 StaticMeshId = INDEX_NONE;
		const FMaterialRenderProxy* MaterialProxy = nullptr;
		const FMaterial* Material = nullptr;
		TShaderRef<FDreamMeshPassVS> VertexShader;
		TShaderRef<FDreamMeshPassPS> PixelShader;
		ERasterizerFillMode FillMode = FM_Solid;
		ERasterizerCullMode CullMode = CM_None;
	};

	/** The pass's settings that decide how a batch is drawn. */
	struct FDrawRules
	{
		EDreamPassMeshMode Mode = EDreamPassMeshMode::Override;
		EDreamPassCullMode Cull = EDreamPassCullMode::Auto;

		/** The override material's proxy; null in Own mode, or when OwnOrOverride has none. */
		const FMaterialRenderProxy* OverrideProxy = nullptr;

		/** `<Pipeline>.<Pass>`, for the log. */
		FString Label;
	};

	/**
	 * Whether a primitive's own material changes what its silhouette is -- it masks or dithers pixels away, or moves
	 * vertices -- so that drawing it with another material would draw a different shape. The engine's custom depth pass
	 * keeps the primitive's own material for exactly these (R/Private/CustomDepthRendering.cpp:560-591); its
	 * DoMaterialAndPrimitiveModifyMeshPosition is renderer-private (R/Private/SceneRendering.cpp:6685-6694), so the
	 * position half is mirrored here.
	 */
	static bool ChangesSilhouette(const FMaterial& Material, const FMeshBatch& MeshBatch, const FPrimitiveSceneProxy* Proxy)
	{
		const bool bMasked = !Material.WritesEveryPixel(false, MeshBatch.VertexFactory->SupportsNullPixelShader());
		const bool bPrimitiveEvaluatesOffset = !ShouldOptimizedWPOAffectNonNaniteShaderSelection() || (Proxy && Proxy->EvaluateWorldPositionOffset());
		const bool bMovesVertices = (Material.MaterialModifiesMeshPosition_RenderThread() && bPrimitiveEvaluatesOffset) || (Proxy && Proxy->IsFirstPerson());
		return bMasked || bMovesVertices;
	}

	/**
	 * The mesh pass processor. Built with the name constructor (R/Public/MeshPassProcessor.h:2273), which leaves
	 * MeshPassType at EMeshPass::Num: EMeshPass has no slot for a plugin, so these commands are never cached.
	 *
	 * Batches are resolved -- material and shaders picked -- before the render graph pass is added, so a pass that ends
	 * up drawing nothing adds none; BuildItem then turns them into draw commands inside AddSimpleMeshPass.
	 */
	class FProcessor final : public FMeshPassProcessor
	{
	public:
		FProcessor(const FScene* InScene, const FSceneView& InView, const FDrawRules& InRules)
			: FMeshPassProcessor(TEXT("DreamPass"), InScene, InView.GetFeatureLevel(), &InView, nullptr)
			, Rules(InRules)
		{
		}

		virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
		{
			FDrawItem Item;
			if (ResolveBatch(MeshBatch, PrimitiveSceneProxy, StaticMeshId, Item))
			{
				BuildItem(Item, BatchElementMask);
			}
		}

		/** Picks the material and shaders a batch is drawn with under the pass's rules; false when it is not drawn. */
		bool ResolveBatch(const FMeshBatch& MeshBatch, const FPrimitiveSceneProxy* Proxy, int32 StaticMeshId, FDrawItem& OutItem) const
		{
			const FMaterialRenderProxy* OwnProxy = MeshBatch.MaterialRenderProxy;
			if (!OwnProxy || !MeshBatch.VertexFactory)
			{
				return false;
			}

			const FVertexFactoryType* VertexFactoryType = MeshBatch.VertexFactory->GetType();

			// The primitive's own material, compiled or not, decides the fill and cull modes and the silhouette rule.
			const FMaterial& Original = OwnProxy->GetIncompleteMaterialWithFallback(FeatureLevel);

			bool bResolved = false;
			switch (Rules.Mode)
			{
			case EDreamPassMeshMode::Own:
				bResolved = TryMaterial(OwnProxy, VertexFactoryType, OutItem);
				break;

			case EDreamPassMeshMode::OwnOrOverride:
				bResolved = TryMaterial(OwnProxy, VertexFactoryType, OutItem) || TryOverride(VertexFactoryType, OutItem);
				break;

			default:
				// D-9, as the engine's custom depth: a primitive whose own material masks or moves vertices keeps it when it
				// has the pass's output, and is drawn with the override -- as a solid, unmoved shape -- when it has not.
				if (ChangesSilhouette(Original, MeshBatch, Proxy))
				{
					bResolved = TryMaterial(OwnProxy, VertexFactoryType, OutItem);
					if (!bResolved)
					{
						WarnOnce(Rules.Label + TEXT(".Mesh.Silhouette"),
							FString::Printf(TEXT("DreamPass: %s: a selected primitive's material masks, dithers or moves vertices and has no UE.DreamPassOutput; it is drawn with the override material, as a solid shape without its mask or vertex offset (reported once)."), *Rules.Label));
					}
				}
				if (!bResolved)
				{
					bResolved = TryOverride(VertexFactoryType, OutItem);
				}
				break;
			}

			if (!bResolved)
			{
				return false;
			}

			// The cull mode comes from the original material's two-sidedness and the primitive's mirroring whatever material
			// is drawn, as the custom depth pass computes it (R/Private/CustomDepthRendering.cpp:633-635). The view's own
			// reverse culling is applied later, to every command (ApplyViewOverridesToMeshDrawCommands).
			const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
			OutItem.FillMode = ComputeMeshFillMode(Original, OverrideSettings);
			OutItem.CullMode = ComputeCullMode(Original, OverrideSettings);
			OutItem.MeshBatch = &MeshBatch;
			OutItem.Proxy = Proxy;
			OutItem.StaticMeshId = StaticMeshId;
			return true;
		}

		/** Builds the draw commands of a resolved batch into the draw list context SetDrawListContext gave. */
		void BuildItem(const FDrawItem& Item, uint64 BatchElementMask = ~0ull)
		{
			TMeshProcessorShaders<FDreamMeshPassVS, FDreamMeshPassPS> PassShaders;
			PassShaders.VertexShader = Item.VertexShader;
			PassShaders.PixelShader = Item.PixelShader;

			// A dithered LOD transition takes the fade the view chose for this static mesh, as the base pass does
			// (R/Private/ShaderBaseClasses.cpp:27-58). No stencil dithering: the pass has no stencil to dither with.
			FDreamMeshPassShaderElementData ShaderElementData;
			ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, Item.Proxy, *Item.MeshBatch, Item.StaticMeshId, false);
			ShaderElementData.SvPositionScaleBias = SvPositionScaleBias;

			const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

			BuildMeshDrawCommands(
				*Item.MeshBatch,
				BatchElementMask,
				Item.Proxy,
				*Item.MaterialProxy,
				*Item.Material,
				RenderState,
				PassShaders,
				Item.FillMode,
				Item.CullMode,
				SortKey,
				EMeshPassFeatures::Default,
				ShaderElementData);
		}

		FMeshPassProcessorRenderState RenderState;
		FVector4f SvPositionScaleBias = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);

	private:
		/**
		 * The first material of a proxy's fallback chain that has the pass's shaders for the vertex factory, as RenderTrace
		 * walks it (Plugins/Runtime/RenderTrace/Source/Private/RenderTrace.cpp:256-316). The chain ends at the default
		 * material, whose fallback is null (E/Private/Materials/Material.cpp:441-450) and which never has these shaders;
		 * the step limit only guards against a proxy that falls back in a circle.
		 */
		bool TryMaterial(const FMaterialRenderProxy* First, const FVertexFactoryType* VertexFactoryType, FDrawItem& OutItem) const
		{
			FMaterialShaderTypes ShaderTypes;
			ShaderTypes.AddShaderType<FDreamMeshPassVS>();
			ShaderTypes.AddShaderType<FDreamMeshPassPS>();

			int32 Steps = 0;
			for (const FMaterialRenderProxy* Candidate = First; Candidate && Steps < 8; Candidate = Candidate->GetFallback(FeatureLevel), ++Steps)
			{
				const FMaterial* Material = Candidate->GetMaterialNoFallback(FeatureLevel);
				FMaterialShaders Shaders;
				if (Material && Material->TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
				{
					Shaders.TryGetVertexShader(OutItem.VertexShader);
					Shaders.TryGetPixelShader(OutItem.PixelShader);
					OutItem.MaterialProxy = Candidate;
					OutItem.Material = Material;
					return OutItem.VertexShader.IsValid() && OutItem.PixelShader.IsValid();
				}
			}
			return false;
		}

		bool TryOverride(const FVertexFactoryType* VertexFactoryType, FDrawItem& OutItem) const
		{
			if (!Rules.OverrideProxy)
			{
				return false;
			}
			if (TryMaterial(Rules.OverrideProxy, VertexFactoryType, OutItem))
			{
				return true;
			}

			// The override material has finished compiling (ExecuteMeshPass waits for it), so a vertex factory it has no
			// shaders for is one it is not used with: its usage flag is off. Reported per vertex factory, the batch skipped.
			const TCHAR* FactoryName = VertexFactoryType ? VertexFactoryType->GetName() : TEXT("?");
			WarnOnce(Rules.Label + TEXT(".Mesh.Usage.") + FactoryName,
				FString::Printf(TEXT("DreamPass: %s: the override material %s has no mesh pass shaders for the vertex factory %s -- its bUsedWith... flag for that kind of mesh is off (`.dsp` Usage =). Those primitives are skipped."),
					*Rules.Label, *Rules.OverrideProxy->GetMaterialName(), FactoryName));
			return false;
		}

		ERasterizerCullMode ComputeCullMode(const FMaterial& Original, const FMeshDrawingPolicyOverrideSettings& OverrideSettings) const
		{
			// A mirrored primitive winds its triangles the other way, so a forced Back or Front follows it.
			const bool bReverse = EnumHasAnyFlags(OverrideSettings.MeshOverrideFlags, EDrawingPolicyOverrideFlags::ReverseCullMode);
			switch (Rules.Cull)
			{
			case EDreamPassCullMode::Back:  return bReverse ? CM_CCW : CM_CW;
			case EDreamPassCullMode::Front: return bReverse ? CM_CW : CM_CCW;
			case EDreamPassCullMode::None:  return CM_None;
			default:                        return ComputeMeshCullMode(Original, OverrideSettings);
			}
		}

		const FDrawRules& Rules;
	};

	// --- states --------------------------------------------------------------------------------------------------------

	/** One blend for every colour target the pass can have (UE.DreamPassOutput has four outputs). */
	template<EBlendOperation ColorOp, EBlendFactor ColorSource, EBlendFactor ColorDest, EBlendOperation AlphaOp, EBlendFactor AlphaSource, EBlendFactor AlphaDest>
	static FRHIBlendState* GetEveryTargetBlendState()
	{
		return TStaticBlendState<
			CW_RGBA, ColorOp, ColorSource, ColorDest, AlphaOp, AlphaSource, AlphaDest,
			CW_RGBA, ColorOp, ColorSource, ColorDest, AlphaOp, AlphaSource, AlphaDest,
			CW_RGBA, ColorOp, ColorSource, ColorDest, AlphaOp, AlphaSource, AlphaDest,
			CW_RGBA, ColorOp, ColorSource, ColorDest, AlphaOp, AlphaSource, AlphaDest>::GetRHI();
	}

	static FRHIBlendState* GetPassBlendState(EDreamPassBlendMode Blend)
	{
		switch (Blend)
		{
		case EDreamPassBlendMode::Add:
			return GetEveryTargetBlendState<BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>();
		case EDreamPassBlendMode::Max:
			return GetEveryTargetBlendState<BO_Max, BF_One, BF_One, BO_Max, BF_One, BF_One>();
		case EDreamPassBlendMode::Min:
			return GetEveryTargetBlendState<BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>();
		case EDreamPassBlendMode::AlphaBlend:
			// Colour over what is there by the output's alpha; alpha accumulates as coverage does.
			return GetEveryTargetBlendState<BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>();
		default:
			return GetEveryTargetBlendState<BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>();
		}
	}

	/** From the pixels of TargetRect to those of ViewRect: xy scale, zw bias. */
	static FVector4f MakeScaleBias(const FIntRect& TargetRect, const FIntRect& ViewRect)
	{
		const float ScaleX = float(FMath::Max(ViewRect.Width(), 1)) / float(FMath::Max(TargetRect.Width(), 1));
		const float ScaleY = float(FMath::Max(ViewRect.Height(), 1)) / float(FMath::Max(TargetRect.Height(), 1));
		return FVector4f(
			ScaleX,
			ScaleY,
			float(ViewRect.Min.X) - float(TargetRect.Min.X) * ScaleX,
			float(ViewRect.Min.Y) - float(TargetRect.Min.Y) * ScaleY);
	}

	// --- writes and targets --------------------------------------------------------------------------------------------

	static FName GetOutputSlotName(int32 Index)
	{
		static const FName Names[MaxMeshOutputs] = { FName(TEXT("Output0")), FName(TEXT("Output1")), FName(TEXT("Output2")), FName(TEXT("Output3")) };
		return Names[Index];
	}

	/** The pass's `write` bindings by colour target: Output<i> is SV_Target<i>. */
	struct FWrites
	{
		const FDreamPassBufferBinding* BySlot[MaxMeshOutputs] = {};
		int32 Count = 0;
		int32 HighestSlot = INDEX_NONE;
	};

	static bool IsIntegerFormat(EDreamPassBufferFormat Format)
	{
		return Format == EDreamPassBufferFormat::R32U || Format == EDreamPassBufferFormat::RG32U;
	}

	static bool ParseWrites(const FExecuteContext& Context, const FString& Label, FWrites& Out)
	{
		const TArray<FDreamPassBufferBinding>& Writes = Context.Pass.Writes;
		for (int32 WriteIndex = 0; WriteIndex < Writes.Num(); ++WriteIndex)
		{
			const FDreamPassBufferBinding& Write = Writes[WriteIndex];

			int32 Slot = INDEX_NONE;
			if (Write.Slot.IsNone() && Writes.Num() == 1)
			{
				Slot = 0;
			}
			for (int32 Index = 0; Index < MaxMeshOutputs && Slot == INDEX_NONE; ++Index)
			{
				if (Write.Slot == GetOutputSlotName(Index))
				{
					Slot = Index;
				}
			}
			if (Slot == INDEX_NONE || Out.BySlot[Slot])
			{
				WarnOnce(Label + TEXT(".Mesh.Slot.") + Write.Slot.ToString(),
					FString::Printf(TEXT("DreamPass: %s: a mesh pass writes Output0..Output3, each once; '%s' is not one of them. The pass is skipped."), *Label, *Write.Slot.ToString()));
				return false;
			}

			// One texture cannot be two colour targets of the same draw.
			for (int32 Other = 0; Other < WriteIndex; ++Other)
			{
				if (Writes[Other].Buffer == Write.Buffer)
				{
					WarnOnce(Label + TEXT(".Mesh.SameBuffer"),
						FString::Printf(TEXT("DreamPass: %s: two outputs write '%s'; a buffer can take one output of a pass. The pass is skipped."), *Label, *Write.Buffer.ToString()));
					return false;
				}
			}

			if (!IsBuiltinBuffer(Write.Buffer))
			{
				const int32 BufferIndex = Context.Pipeline.FindBuffer(Write.Buffer);
				if (!Context.Pipeline.Buffers.IsValidIndex(BufferIndex))
				{
					WarnOnce(Label + TEXT(".Mesh.NoBuffer.") + Write.Buffer.ToString(),
						FString::Printf(TEXT("DreamPass: %s writes '%s', which is no buffer of the pipeline; the pass is skipped."), *Label, *Write.Buffer.ToString()));
					return false;
				}

				// UE.DreamPassOutput is float4, which an integer target would take as raw bits.
				const EDreamPassBufferFormat Format = Context.Pipeline.Buffers[BufferIndex].Desc.Format;
				if (IsIntegerFormat(Format) || Format == EDreamPassBufferFormat::Depth32)
				{
					WarnOnce(Label + TEXT(".Mesh.Format.") + Write.Buffer.ToString(),
						FString::Printf(TEXT("DreamPass: %s: '%s' is %s; a mesh pass writes float colour buffers only (a Depth32 buffer is its `Depth = Own(...)`). The pass is skipped."),
							*Label, *Write.Buffer.ToString(), LexToString(Format)));
					return false;
				}
			}

			Out.BySlot[Slot] = &Write;
			++Out.Count;
			Out.HighestSlot = FMath::Max(Out.HighestSlot, Slot);
		}
		return true;
	}

	/** A pass that draws nothing still leaves its own buffers as created -- cleared -- so an exported one shows that. */
	static void TouchOwnWrites(FExecuteContext& Context, const FWrites& Writes)
	{
		for (const FDreamPassBufferBinding* Write : Writes.BySlot)
		{
			const int32 BufferIndex = Write && !IsBuiltinBuffer(Write->Buffer) ? Context.Pipeline.FindBuffer(Write->Buffer) : INDEX_NONE;
			if (Context.Pipeline.Buffers.IsValidIndex(BufferIndex))
			{
				GetOrCreateBuffer(Context, BufferIndex);
			}
		}
	}

	/** The colour targets, packed by slot as the render graph wants them, all of one extent and one view rect. */
	struct FTargets
	{
		FRDGTextureRef Textures[MaxMeshOutputs] = {};
		ERenderTargetLoadAction LoadActions[MaxMeshOutputs] = { ERenderTargetLoadAction::ENoAction, ERenderTargetLoadAction::ENoAction, ERenderTargetLoadAction::ENoAction, ERenderTargetLoadAction::ENoAction };
		int32 Num = 0;

		FIntPoint Extent = FIntPoint::ZeroValue;
		FIntRect Rect;

		/** The slot whose target is the scene colour's scratch texture, which CommitSceneColor brings back. */
		int32 SceneColorSlot = INDEX_NONE;
		FScreenPassRenderTarget SceneColor;

		bool HasColor() const { return Num > 0; }

		void Bind(FRenderTargetBindingSlots& Slots) const
		{
			for (int32 Slot = 0; Slot < Num; ++Slot)
			{
				Slots[Slot] = FRenderTargetBinding(Textures[Slot], LoadActions[Slot]);
			}
		}
	};

	static bool ResolveTargets(FExecuteContext& Context, const FString& Label, const FWrites& Writes, FTargets& Out)
	{
		bool bHaveShape = false;
		for (int32 Slot = 0; Slot <= Writes.HighestSlot; ++Slot)
		{
			const FDreamPassBufferBinding* Write = Writes.BySlot[Slot];
			if (!Write)
			{
				continue;
			}

			const FScreenPassRenderTarget Target = ResolveWrite(Context, *Write);
			if (!Target.IsValid())
			{
				return false;
			}

			// One viewport for every target: they must hold the view at the same pixels.
			if (!bHaveShape)
			{
				Out.Extent = Target.Texture->Desc.Extent;
				Out.Rect = Target.ViewRect;
				bHaveShape = true;
			}
			else if (Target.Texture->Desc.Extent != Out.Extent || Target.ViewRect != Out.Rect)
			{
				WarnOnce(Label + TEXT(".Mesh.Shape"),
					FString::Printf(TEXT("DreamPass: %s: a mesh pass's outputs must all have one size and hold the view at the same place; '%s' does not match the others. The pass is skipped."),
						*Label, *Write->Buffer.ToString()));
				return false;
			}

			Out.Textures[Slot] = Target.Texture;
			Out.LoadActions[Slot] = Target.LoadAction;
			if (IsSceneColorWrite(Context, *Write))
			{
				Out.SceneColorSlot = Slot;
				Out.SceneColor = Target;
			}
		}

		Out.Num = Writes.HighestSlot + 1;

		// Render targets are bound packed (RenderCore/Private/RenderGraphValidation.cpp:911-915, "Render targets must be
		// packed"): an output the pass skips below one it writes gets a throwaway target, whose writes nobody reads.
		// Outputs above the last one written are left unbound; the shader's writes to them are discarded.
		for (int32 Slot = 0; Slot < Out.Num; ++Slot)
		{
			if (!Out.Textures[Slot])
			{
				Out.Textures[Slot] = Context.GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(Out.Extent, PF_R8, FClearValueBinding::Black, TexCreate_RenderTargetable),
					TEXT("DreamPass.MeshUnusedOutput"));
				Out.LoadActions[Slot] = ERenderTargetLoadAction::ENoAction;
			}
		}
		return true;
	}

	/** The depth the pass tests against or writes, and how. */
	struct FDepth
	{
		FDepthStencilBinding Binding;
		FExclusiveDepthStencil::Type Access = FExclusiveDepthStencil::DepthNop_StencilNop;
		FRHIDepthStencilState* State = nullptr;
		bool bHasTarget = false;
	};

	/**
	 * The scene depth copied into the targets' pixels, for `Depth = TestScene` targets that do not hold the view where
	 * the scene depth does -- an own buffer of the second view in split screen, a buffer at another resolution. The
	 * copy is point-sampled; where the scales differ, the test is as exact as the coarser of the two.
	 */
	static FRDGTextureRef AddCopyDepthPass(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef SceneDepth, const FIntRect& DepthRect, const FTargets& Targets, const FVector4f& TargetToDepth)
	{
		FRDGTextureRef Copy = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(Targets.Extent, SceneDepth->Desc.Format, FClearValueBinding::DepthFar, TexCreate_DepthStencilTargetable | TexCreate_ShaderResource),
			TEXT("DreamPass.MeshSceneDepth"));

		FDreamMeshPassCopyDepthPS::FParameters* Parameters = GraphBuilder.AllocParameters<FDreamMeshPassCopyDepthPS::FParameters>();
		Parameters->SourceDepth = SceneDepth;
		Parameters->TargetToSourceScaleBias = TargetToDepth;
		Parameters->SourceMin = DepthRect.Min;
		Parameters->SourceMax = DepthRect.Max;
		Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(Copy, ERenderTargetLoadAction::EClear, FExclusiveDepthStencil::DepthWrite_StencilNop);

		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
		TShaderMapRef<FDreamMeshPassCopyDepthPS> PixelShader(ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			ShaderMap,
			RDG_EVENT_NAME("DreamPass.MeshSceneDepth"),
			PixelShader,
			Parameters,
			Targets.Rect,
			nullptr,
			nullptr,
			TStaticDepthStencilState<true, CF_Always>::GetRHI());
		return Copy;
	}

	static bool ResolveDepth(FExecuteContext& Context, const FString& Label, FTargets& Targets, FDepth& Out)
	{
		const FDreamPassMeshSettings& Mesh = Context.Pass.Mesh;
		const FSceneView& View = Context.GetView();

		switch (Mesh.Depth)
		{
		case EDreamPassDepthMode::None:
			Out.State = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			return true;

		case EDreamPassDepthMode::Own:
		{
			const int32 BufferIndex = Context.Pipeline.FindBuffer(Mesh.DepthBuffer);
			if (!Context.Pipeline.Buffers.IsValidIndex(BufferIndex) || Context.Pipeline.Buffers[BufferIndex].Desc.Format != EDreamPassBufferFormat::Depth32)
			{
				WarnOnce(Label + TEXT(".Mesh.OwnDepth"),
					FString::Printf(TEXT("DreamPass: %s: `Depth = Own(%s)` needs a Depth32 buffer of the pipeline by that name; the pass is skipped."), *Label, *Mesh.DepthBuffer.ToString()));
				return false;
			}

			// Cleared to the far plane when it is created (Render/DreamPassBuffers.cpp, reverse Z).
			FRDGTextureRef DepthTexture = GetOrCreateBuffer(Context, BufferIndex);
			if (!Targets.HasColor())
			{
				// A depth-only pass: it builds the depth a later pass of the pipeline tests against.
				Targets.Extent = DepthTexture->Desc.Extent;
				Targets.Rect = FIntRect(FIntPoint::ZeroValue, Targets.Extent);
			}
			else if (DepthTexture->Desc.Extent.X < Targets.Rect.Max.X || DepthTexture->Desc.Extent.Y < Targets.Rect.Max.Y)
			{
				WarnOnce(Label + TEXT(".Mesh.OwnDepthSize"),
					FString::Printf(TEXT("DreamPass: %s: the depth buffer %s is smaller than the outputs it is drawn with; give it their size. The pass is skipped."), *Label, *Mesh.DepthBuffer.ToString()));
				return false;
			}

			Out.Access = FExclusiveDepthStencil::DepthWrite_StencilNop;
			Out.Binding = FDepthStencilBinding(DepthTexture, ERenderTargetLoadAction::ELoad, Out.Access);
			Out.State = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
			Out.bHasTarget = true;
			return true;
		}

		default:
		{
			FRDGTextureRef SceneDepth = Context.Injection.SceneDepth;
			if (!SceneDepth)
			{
				WarnOnce(Label + TEXT(".Mesh.NoSceneDepth"),
					FString::Printf(TEXT("DreamPass: %s: `Depth = TestScene` needs the scene depth, which %s does not have; the pass is skipped."), *Label, LexToString(Context.Injection.Injection)));
				return false;
			}
			if (SceneDepth->Desc.NumSamples > 1)
			{
				WarnOnce(Label + TEXT(".Mesh.MSAA"),
					FString::Printf(TEXT("DreamPass: %s: the scene depth is multisampled here, which a mesh pass cannot test against; the pass is skipped."), *Label));
				return false;
			}

			// The scene depth holds the view at the render-resolution view rect (UE::FXRenderingUtils::GetRawViewRectUnsafe).
			// Targets that hold it at the same pixels test against it directly, read only, with the comparison the custom
			// depth pass draws with (R/Private/CustomDepthRendering.cpp:510); a larger depth texture is fine, the viewport
			// is inside both. Others test against a copy brought into their pixels.
			const FIntRect DepthRect = GetRenderViewRect(View);
			const bool bSamePixels = Targets.Rect == DepthRect
				&& SceneDepth->Desc.Extent.X >= DepthRect.Max.X
				&& SceneDepth->Desc.Extent.Y >= DepthRect.Max.Y;

			FRDGTextureRef DepthTexture = bSamePixels
				? SceneDepth
				: AddCopyDepthPass(Context.GraphBuilder, View, SceneDepth, DepthRect, Targets, MakeScaleBias(Targets.Rect, DepthRect));

			// A depth test only: nothing a later pass reads is written (01 §8.4; reverse Z, so nearer is greater).
			Out.Access = FExclusiveDepthStencil::DepthRead_StencilNop;
			Out.Binding = FDepthStencilBinding(DepthTexture, ERenderTargetLoadAction::ELoad, Out.Access);
			Out.State = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
			Out.bHasTarget = true;
			return true;
		}
		}
	}

	static bool SceneTexturesHold(const FSceneTextureUniformParameters& SceneTextures, FRDGTextureRef Texture)
	{
		return Texture == SceneTextures.SceneColorTexture
			|| Texture == SceneTextures.SceneDepthTexture
			|| Texture == SceneTextures.ScenePartialDepthTexture
			|| Texture == SceneTextures.GBufferATexture
			|| Texture == SceneTextures.GBufferBTexture
			|| Texture == SceneTextures.GBufferCTexture
			|| Texture == SceneTextures.GBufferDTexture
			|| Texture == SceneTextures.GBufferETexture
			|| Texture == SceneTextures.GBufferFTexture
			|| Texture == SceneTextures.GBufferVelocityTexture
			|| Texture == SceneTextures.GBufferSGGXTexture
			|| Texture == SceneTextures.ScreenSpaceAOTexture
			|| Texture == SceneTextures.CustomDepthTexture;
	}

	/**
	 * The scene textures the pass's materials and the Nanite fill read. The injection point's own, unless the pass writes
	 * one of them -- a GBuffer target at AfterBasePass -- which one pass cannot both sample and render to: then a set with
	 * the depths only. The scene colour is never written in place (Render/DreamPassBuffers.cpp, ResolveWrite).
	 */
	static TRDGUniformBufferRef<FSceneTextureUniformParameters> GetPassSceneTextures(FExecuteContext& Context, const FTargets& Targets)
	{
		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures = Context.Injection.SceneTextures;
		if (SceneTextures)
		{
			const FSceneTextureUniformParameters& Contents = *SceneTextures->GetContents();
			for (int32 Slot = 0; Slot < Targets.Num; ++Slot)
			{
				if (SceneTexturesHold(Contents, Targets.Textures[Slot]))
				{
					SceneTextures = nullptr;
					break;
				}
			}
		}
		if (!SceneTextures)
		{
			SceneTextures = CreateSceneTextureUniformBuffer(Context.GraphBuilder, Context.GetView(), ESceneTextureSetupMode::SceneDepth | ESceneTextureSetupMode::CustomDepth);
		}
		return SceneTextures;
	}

	// --- selection -----------------------------------------------------------------------------------------------------

	/**
	 * The value the custom depth pass leaves in the stencil for a primitive: its stencil value under its write mask
	 * (R/Private/CustomDepthRendering.cpp, GetCustomDepthStencilState: SM_Default and SM_255 write every bit, SM_1..SM_128 one).
	 */
	static uint32 GetWrittenStencil(const FPrimitiveSceneProxy& Proxy)
	{
		const EStencilMask WriteMask = Proxy.GetStencilWriteMask();
		const uint32 Bits = (WriteMask >= SM_1 && WriteMask <= SM_128) ? (1u << uint32(WriteMask - SM_1)) : 0xFFu;
		return uint32(Proxy.GetCustomDepthStencilValue()) & Bits;
	}

	static bool StencilTermMatches(const FDreamPassFilterTerm& Term, uint32 Stencil)
	{
		const uint32 Mask = uint32(Term.StencilMask) & 0xFFu;
		return (Stencil & Mask) == (uint32(Term.StencilValue) & Mask);
	}

	/**
	 * The scene's primitives that render custom depth, by packed index: scanned once per family and shared by every
	 * mesh pass of it that selects by stencil (03 §5.2). The packed indices hold for the whole family: the scene is
	 * updated before a renderer starts, never while it renders.
	 */
	static const TArray<uint32>& GetCustomDepthPrimitives(FFamilyState& Family, const FSceneInterface& Scene)
	{
		if (!Family.bCustomDepthPrimitivesCollected)
		{
			Family.bCustomDepthPrimitivesCollected = true;
			Family.CustomDepthPrimitiveIndices.Reset();

			const TConstArrayView<FPrimitiveSceneProxy*> Proxies = Scene.GetPrimitiveSceneProxies();
			for (int32 Index = 0; Index < Proxies.Num(); ++Index)
			{
				if (Proxies[Index] && Proxies[Index]->ShouldRenderCustomDepth())
				{
					Family.CustomDepthPrimitiveIndices.Add(uint32(Index));
				}
			}
		}
		return Family.CustomDepthPrimitiveIndices;
	}

	/** The term a clause's candidates are drawn from: the narrowest kind it has -- a list, then a layer, then a stencil. */
	static const FDreamPassFilterTerm* PickGenerator(const FDreamPassFilterClause& Clause)
	{
		const FDreamPassFilterTerm* Best = nullptr;
		for (const FDreamPassFilterTerm& Term : Clause.AllOf)
		{
			const int32 Rank = Term.Kind == EDreamPassFilterKind::List ? 0 : Term.Kind == EDreamPassFilterKind::Layer ? 1 : 2;
			const int32 BestRank = !Best ? 3 : Best->Kind == EDreamPassFilterKind::List ? 0 : Best->Kind == EDreamPassFilterKind::Layer ? 1 : 2;
			if (Rank < BestRank)
			{
				Best = &Term;
			}
		}
		return Best;
	}

	/**
	 * Every primitive the pass's filter selects, by packed scene index, each once. The filter is a disjunction of
	 * clauses, each a conjunction of terms; each clause draws its candidates from its narrowest term and keeps those that
	 * satisfy all of its terms. Render thread: only render-side data is read -- proxies, scene infos and the snapshot's
	 * id tables -- never a component.
	 */
	static void SelectPrimitives(FExecuteContext& Context, const FSceneInterface& Scene, const FString& Label, TArray<int32>& OutIndices)
	{
		const FDreamPassMeshFilter& Filter = Context.Pass.Mesh.Filter;
		const FFamilySnapshot& Snapshot = *Context.Family.Snapshot;

		const TConstArrayView<FPrimitiveSceneProxy*> Proxies = Scene.GetPrimitiveSceneProxies();
		const TConstArrayView<FPrimitiveComponentId> ComponentIds = Scene.GetScenePrimitiveComponentIds();
		const int32 NumPrimitives = FMath::Min(Proxies.Num(), ComponentIds.Num());

		// A list term that does not generate a clause's candidates tests them: by set rather than by scanning the list.
		TMap<FName, TSet<uint32>> ListSets;
		for (const FDreamPassFilterClause& Clause : Filter.AnyOf)
		{
			for (const FDreamPassFilterTerm& Term : Clause.AllOf)
			{
				if (Term.Kind == EDreamPassFilterKind::List && !ListSets.Contains(Term.ListName))
				{
					TSet<uint32>& Set = ListSets.Add(Term.ListName);
					if (const TArray<uint32>* Members = Snapshot.ListMembers.Find(Term.ListName))
					{
						Set.Append(*Members);
					}
				}
			}
		}

		auto TermHolds = [&](const FDreamPassFilterTerm& Term, int32 Index) -> bool
		{
			const uint32 Id = ComponentIds[Index].PrimIDValue;
			switch (Term.Kind)
			{
			case EDreamPassFilterKind::Stencil:
				return Proxies[Index]->ShouldRenderCustomDepth() && StencilTermMatches(Term, GetWrittenStencil(*Proxies[Index]));
			case EDreamPassFilterKind::Layer:
			{
				const uint32* Layers = Snapshot.LayersByPrimitiveId.Find(Id);
				return Layers && (*Layers & uint32(Term.LayerMask)) != 0;
			}
			case EDreamPassFilterKind::List:
			{
				const TSet<uint32>* Set = ListSets.Find(Term.ListName);
				return Set && Set->Contains(Id);
			}
			}
			return false;
		};

		TBitArray<> Selected(false, NumPrimitives);
		auto Consider = [&](const FDreamPassFilterClause& Clause, int32 Index)
		{
			if (Index < 0 || Index >= NumPrimitives || Selected[Index] || !Proxies[Index])
			{
				return;
			}
			for (const FDreamPassFilterTerm& Term : Clause.AllOf)
			{
				if (!TermHolds(Term, Index))
				{
					return;
				}
			}
			Selected[Index] = true;
			OutIndices.Add(Index);
		};
		auto ConsiderComponent = [&](const FDreamPassFilterClause& Clause, uint32 IdValue)
		{
			// The game thread recorded component ids; the scene finds their primitives (E/Public/SceneInterface.h:186).
			FPrimitiveComponentId Id;
			Id.PrimIDValue = IdValue;
			const FPrimitiveSceneInfo* SceneInfo = Scene.GetPrimitiveSceneInfo(Id);
			if (SceneInfo && SceneInfo->IsIndexValid())
			{
				Consider(Clause, SceneInfo->GetIndex());
			}
		};

		for (const FDreamPassFilterClause& Clause : Filter.AnyOf)
		{
			const FDreamPassFilterTerm* Generator = PickGenerator(Clause);
			if (!Generator)
			{
				WarnOnce(Label + TEXT(".Mesh.EmptyClause"),
					FString::Printf(TEXT("DreamPass: %s: a filter clause without terms selects nothing."), *Label));
				continue;
			}

			switch (Generator->Kind)
			{
			case EDreamPassFilterKind::List:
				if (const TArray<uint32>* Members = Snapshot.ListMembers.Find(Generator->ListName))
				{
					for (const uint32 IdValue : *Members)
					{
						ConsiderComponent(Clause, IdValue);
					}
				}
				break;

			case EDreamPassFilterKind::Layer:
				for (const TPair<uint32, uint32>& Pair : Snapshot.LayersByPrimitiveId)
				{
					if ((Pair.Value & uint32(Generator->LayerMask)) != 0)
					{
						ConsiderComponent(Clause, Pair.Key);
					}
				}
				break;

			case EDreamPassFilterKind::Stencil:
				for (const uint32 Index : GetCustomDepthPrimitives(Context.Family, Scene))
				{
					Consider(Clause, int32(Index));
				}
				break;
			}
		}
	}

	/** What the pass culls by, worked out once per execution. */
	struct FCullSettings
	{
		int32 ForcedLODLevel = 0;
		float LODScale = 1.0f;
		float DistanceScale = 1.0f;
	};

	static FCullSettings MakeCullSettings(const FSceneView& View)
	{
		FCullSettings Settings;

		// As the renderer's visibility (R/Private/SceneVisibility.cpp:1243-1250, 3880).
		Settings.ForcedLODLevel = View.Family->EngineShowFlags.LOD ? GetCVarForceLOD() : 0;

		static const TConsoleVariableData<float>* LODDistanceScale = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.StaticMeshLODDistanceScale"));
		Settings.LODScale = (LODDistanceScale ? LODDistanceScale->GetValueOnRenderThread() : 1.0f) * View.LODDistanceFactor;

		const FCachedSystemScalabilityCVars& Scalability = GetCachedScalabilityCVars();
		Settings.DistanceScale = Scalability.ViewDistanceScale * Scalability.CalculateFieldOfViewDistanceScale(View.DesiredFOV);
		return Settings;
	}

	/**
	 * View-level culling with public data only (01 §8.4): shown in this view, not hidden by it, inside its frustum, within
	 * the primitive's draw distances. No occlusion culling and no per-instance culling (R-8).
	 */
	static bool IsPrimitiveVisible(const FSceneView& View, const FPrimitiveSceneProxy& Proxy, FPrimitiveComponentId Id, const FCullSettings& Settings)
	{
		if (!Proxy.IsShown(&View))
		{
			return false;
		}
		if (View.HiddenPrimitives.Contains(Id) || (View.ShowOnlyPrimitives.IsSet() && !View.ShowOnlyPrimitives.GetValue().Contains(Id)))
		{
			return false;
		}

		const FBoxSphereBounds& Bounds = Proxy.GetBounds();
		if (!View.GetCullingFrustum().IntersectBox(Bounds.Origin, Bounds.BoxExtent))
		{
			return false;
		}

		// Distance to the bounding sphere's edge, as r.DistanceCullToSphereEdge does by default
		// (R/Private/SceneVisibility.cpp:720-728, 990-1027).
		if (View.Family->EngineShowFlags.DistanceCulledPrimitives && !Proxy.IsDetailMesh())
		{
			return true;
		}
		const float MaxDrawDistance = Proxy.GetMaxDrawDistance();
		const float MinDrawDistance = Proxy.GetMinDrawDistance();
		const bool bHasMaxDrawDistance = MaxDrawDistance < FLT_MAX;
		const bool bHasMinDrawDistance = MinDrawDistance > 0.0f;
		if (bHasMaxDrawDistance || bHasMinDrawDistance)
		{
			const float Distance = float((Bounds.Origin - View.CullingOrigin).Length());
			const float Radius = float(Bounds.SphereRadius);
			const float ClosestSquared = Distance >= Radius ? FMath::Square(Distance - Radius) : 0.0f;
			const float FurthestSquared = FMath::Square(Distance + Radius);
			if (bHasMaxDrawDistance && ClosestSquared > FMath::Square(MaxDrawDistance * Settings.DistanceScale))
			{
				return false;
			}
			if (bHasMinDrawDistance && FurthestSquared < FMath::Square(MinDrawDistance * Settings.DistanceScale))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * The LODs of a primitive the view draws, as ComputePrimitiveMeshLODs works them out (R/Private/SceneVisibility.cpp:1494-1517)
	 * less its scene culling query, which needs the renderer's private culling state. Without an instance culling
	 * manager there is no per-instance LOD on the GPU either, so the primitive is drawn at one LOD (two while it dithers
	 * between them), never at a range of them.
	 */
	static FLODMask ComputeLODToRender(const FSceneView& View, const FPrimitiveSceneProxy& Proxy, const FPrimitiveSceneInfo& SceneInfo, const FCullSettings& Settings)
	{
		const FDesiredLODLevel DesiredLODLevel = Proxy.GetDesiredLODLevel_RenderThread(&View);
		if (DesiredLODLevel.IsFixed())
		{
			FLODMask LODToRender;
			LODToRender.SetLOD(DesiredLODLevel.LOD);
			return LODToRender;
		}

		const FBoxSphereBounds& Bounds = Proxy.GetBounds();
		float ScreenRadiusSquared = 0.0f;
		return ComputeLODForMeshes(SceneInfo.StaticMeshRelevances, View, Bounds.Origin, float(Bounds.SphereRadius), Settings.ForcedLODLevel, ScreenRadiusSquared, int8(DesiredLODLevel.LOD), Settings.LODScale);
	}

	/** The selection, culled and resolved into draws. */
	struct FCollected
	{
		TArray<FDrawItem> Items;

		/** A selected Nanite primitive is visible in the view: the Nanite policy may have something to fill. */
		bool bAnyNanite = false;
	};

	static void CollectDraws(FExecuteContext& Context, const FSceneInterface& Scene, const FProcessor& Processor, const FString& Label, FCollected& Out)
	{
		const FSceneView& View = Context.GetView();
		const FDreamPassMeshSettings& Mesh = Context.Pass.Mesh;

		TArray<int32> Indices;
		SelectPrimitives(Context, Scene, Label, Indices);
		if (Indices.IsEmpty())
		{
			return;
		}

		const TConstArrayView<FPrimitiveSceneProxy*> Proxies = Scene.GetPrimitiveSceneProxies();
		const TConstArrayView<FPrimitiveComponentId> ComponentIds = Scene.GetScenePrimitiveComponentIds();
		const FCullSettings Settings = MakeCullSettings(View);

		for (const int32 Index : Indices)
		{
			const FPrimitiveSceneProxy* Proxy = Proxies[Index];
			const FPrimitiveSceneInfo* SceneInfo = Scene.GetPrimitiveSceneInfo(Index);
			if (!Proxy || !SceneInfo || !IsPrimitiveVisible(View, *Proxy, ComponentIds[Index], Settings))
			{
				continue;
			}

			const FPrimitiveViewRelevance Relevance = Proxy->GetViewRelevance(&View);
			if (!Relevance.bDrawRelevance)
			{
				continue;
			}

			// Nanite primitives have no mesh batch to draw again (E/Private/Rendering/NaniteResources.cpp:691-693).
			if (Proxy->IsNaniteMesh())
			{
				Out.bAnyNanite = true;
				if (Mesh.Nanite == EDreamPassNanitePolicy::Skip)
				{
					WarnOnce(Label + TEXT(".Mesh.NaniteSkipped"),
						FString::Printf(TEXT("DreamPass: %s: Nanite primitives cannot be drawn again by a mesh pass and are skipped (`Nanite = StencilMask` fills their pixels from the custom stencil instead). Reported once."), *Label));
				}
				continue;
			}

			// Static relevance is what has batches here; GPU-skinned skeletal meshes have it too (01 §8.3). What only has
			// dynamic relevance is built per view by the renderer's private mesh collector (D-6).
			if (!Relevance.bStaticRelevance)
			{
				if (Relevance.bDynamicRelevance)
				{
					WarnOnce(Label + TEXT(".Mesh.Dynamic"),
						FString::Printf(TEXT("DreamPass: %s: a selected primitive is dynamic only (CPU skinning, a procedural mesh, particles), which a mesh pass cannot draw yet; it is skipped. Reported once."), *Label));
				}
				continue;
			}

			const FLODMask LODToRender = ComputeLODToRender(View, *Proxy, *SceneInfo, Settings);
			const int32 NumMeshes = FMath::Min(SceneInfo->StaticMeshRelevances.Num(), SceneInfo->StaticMeshes.Num());
			for (int32 MeshIndex = 0; MeshIndex < NumMeshes; ++MeshIndex)
			{
				const FStaticMeshBatchRelevance& MeshRelevance = SceneInfo->StaticMeshRelevances[MeshIndex];

				// The batches the base pass draws (R/Public/StaticMeshBatch.h): not depth-only or shadow-only ones, nor those
				// for runtime virtual textures, the water info texture, Lumen's card capture or an overlay material.
				if (!MeshRelevance.bUseForMaterial
					|| MeshRelevance.bRenderToVirtualTexture
					|| MeshRelevance.bUseForWaterInfoTextureDepth
					|| MeshRelevance.bUseForLumenSceneCapture
					|| MeshRelevance.bOverlayMaterial
					|| !LODToRender.ContainsLOD(MeshRelevance.GetLODIndex()))
				{
					continue;
				}

				const FStaticMeshBatch& StaticMesh = SceneInfo->StaticMeshes[MeshIndex];
				FDrawItem Item;
				if (Processor.ResolveBatch(StaticMesh, Proxy, StaticMesh.Id, Item))
				{
					Out.Items.Add(Item);
				}
			}
		}
	}

	// --- Nanite --------------------------------------------------------------------------------------------------------

	static bool FilterHasMembershipTerms(const FDreamPassMeshFilter& Filter)
	{
		for (const FDreamPassFilterClause& Clause : Filter.AnyOf)
		{
			for (const FDreamPassFilterTerm& Term : Clause.AllOf)
			{
				if (Term.Kind != EDreamPassFilterKind::Stencil)
				{
					return true;
				}
			}
		}
		return false;
	}

	/**
	 * The custom stencil values the Nanite fill selects, as 256 bits: every Stencil term of the filter, and the value
	 * AssignStencil gives the Nanite members of its layers and lists. A pixel only knows its stencil, so a clause's other
	 * terms do not narrow the fill. False when no value is selected.
	 */
	static bool BuildStencilMatch(const FDreamPassMeshSettings& Mesh, uint32 (&OutWords)[8])
	{
		FMemory::Memzero(OutWords);
		bool bAny = false;

		auto AddValue = [&OutWords, &bAny](uint32 Value, uint32 Mask)
		{
			for (uint32 Stencil = 0; Stencil < 256u; ++Stencil)
			{
				if ((Stencil & Mask) == (Value & Mask))
				{
					OutWords[Stencil >> 5u] |= 1u << (Stencil & 31u);
				}
			}
			bAny = true;
		};

		for (const FDreamPassFilterClause& Clause : Mesh.Filter.AnyOf)
		{
			for (const FDreamPassFilterTerm& Term : Clause.AllOf)
			{
				if (Term.Kind == EDreamPassFilterKind::Stencil)
				{
					AddValue(uint32(Term.StencilValue) & 0xFFu, uint32(Term.StencilMask) & 0xFFu);
				}
			}
		}

		if (Mesh.Nanite == EDreamPassNanitePolicy::AssignStencil && FilterHasMembershipTerms(Mesh.Filter))
		{
			AddValue(uint32(FMath::Clamp(Mesh.AssignedStencilValue, 1, 255)), 0xFFu);
		}
		return bAny;
	}

	/** Whether the Nanite fill can run for this pass here; says once why not when it cannot. */
	static bool PrepareNaniteFill(const FExecuteContext& Context, const FString& Label, uint32 (&OutWords)[8])
	{
		const FDreamPassMeshSettings& Mesh = Context.Pass.Mesh;
		if (Mesh.Nanite == EDreamPassNanitePolicy::Skip)
		{
			return false;
		}

		if (Mesh.Nanite == EDreamPassNanitePolicy::StencilMask && FilterHasMembershipTerms(Mesh.Filter))
		{
			WarnOnce(Label + TEXT(".Mesh.NaniteMembers"),
				FString::Printf(TEXT("DreamPass: %s: `Nanite = StencilMask` reaches Nanite primitives through their custom stencil only; the Nanite members of the filter's layers and lists are not filled. `Nanite = AssignStencil(...)` gives them a stencil value."), *Label));
		}

		if (!BuildStencilMatch(Mesh, OutWords))
		{
			return false;
		}

		if (!Context.Family.Snapshot->bCustomStencilWritten)
		{
			WarnOnce(Label + TEXT(".Mesh.NaniteNoStencil"),
				FString::Printf(TEXT("DreamPass: %s: Nanite primitives are filled from the custom stencil, which only r.CustomDepth=3 writes; they are skipped."), *Label));
			return false;
		}

		// PreRenderBasePass comes before the custom depth pass in every order (01 §2, CustomDepth timing).
		if (Context.Injection.Injection == EDreamPassInjection::BeforeBasePass)
		{
			WarnOnce(Label + TEXT(".Mesh.NaniteTooEarly"),
				FString::Printf(TEXT("DreamPass: %s: at BeforeBasePass the custom depth is not rendered yet; Nanite primitives are skipped."), *Label));
			return false;
		}
		return true;
	}

	/**
	 * Fills the outputs with NaniteValue where the custom stencil holds a selected value, at the custom depth tested as
	 * a drawn mesh would be. Runs before the meshes are drawn: a non-Nanite primitive that matches the stencil as well is
	 * then drawn over its fill, which Replace (the default blend) makes exact; other blend modes add the two.
	 */
	static void AddNaniteFillPass(FExecuteContext& Context, const FTargets& Targets, const FDepth& Depth, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures, FRHIBlendState* BlendState, const FVector4f& TargetToView, const uint32 (&Words)[8])
	{
		const FSceneView& View = Context.GetView();
		const FDreamPassMeshSettings& Mesh = Context.Pass.Mesh;
		const FIntRect ViewRect = GetRenderViewRect(View);

		FDreamMeshPassStencilMaskPS::FParameters* Parameters = Context.GraphBuilder.AllocParameters<FDreamMeshPassStencilMaskPS::FParameters>();
		Parameters->SceneTextures = SceneTextures;
		Parameters->TargetToSourceScaleBias = TargetToView;
		Parameters->SourceMin = ViewRect.Min;
		Parameters->SourceMax = ViewRect.Max;
		Parameters->FillValue = FVector4f(Mesh.NaniteValue.R, Mesh.NaniteValue.G, Mesh.NaniteValue.B, Mesh.NaniteValue.A);
		Parameters->StencilMatch[0] = FUintVector4(Words[0], Words[1], Words[2], Words[3]);
		Parameters->StencilMatch[1] = FUintVector4(Words[4], Words[5], Words[6], Words[7]);
		Targets.Bind(Parameters->RenderTargets);
		if (Depth.bHasTarget)
		{
			Parameters->RenderTargets.DepthStencil = Depth.Binding;
		}

		FDreamMeshPassStencilMaskPS::FPermutationDomain Permutation;
		Permutation.Set<FDreamMeshPassStencilMaskPS::FOutputDepth>(Depth.bHasTarget);

		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
		TShaderMapRef<FDreamMeshPassStencilMaskPS> PixelShader(ShaderMap, Permutation);
		FPixelShaderUtils::AddFullscreenPass(
			Context.GraphBuilder,
			ShaderMap,
			RDG_EVENT_NAME("DreamPass.MeshNaniteStencilMask"),
			PixelShader,
			Parameters,
			Targets.Rect,
			BlendState,
			nullptr,
			Depth.State);
	}
}

namespace UE::DreamPass
{
	bool ExecuteMeshPass(FExecuteContext& Context)
	{
		using namespace Private::MeshPass;

		const FSnapshotPass& Pass = Context.Pass;
		const FDreamPassMeshSettings& Mesh = Pass.Mesh;
		const FSceneView& View = Context.GetView();
		FRDGBuilder& GraphBuilder = Context.GraphBuilder;
		const FString Label = Context.Pipeline.DebugName + TEXT(".") + Pass.Name.ToString();

		// At BeginView the renderer has not set the scene up for the frame -- no GPU scene upload, no scene textures.
		if (Context.Injection.Injection == EDreamPassInjection::BeginView)
		{
			WarnOnce(Label + TEXT(".Mesh.BeginView"),
				FString::Printf(TEXT("DreamPass: %s: a mesh pass cannot run at BeginView, before the renderer has set the scene up for the frame; the pass is skipped."), *Label));
			return false;
		}

		FSceneInterface* SceneInterface = View.Family ? View.Family->Scene : nullptr;
		const FScene* Scene = SceneInterface ? SceneInterface->GetRenderScene() : nullptr;
		if (!Scene)
		{
			return false;
		}

		FWrites Writes;
		if (!ParseWrites(Context, Label, Writes))
		{
			return false;
		}
		if (Writes.Count == 0 && Mesh.Depth != EDreamPassDepthMode::Own)
		{
			WarnOnce(Label + TEXT(".Mesh.NoWrites"),
				FString::Printf(TEXT("DreamPass: %s: a mesh pass writes at least one of Output0..Output3, or a depth with `Depth = Own(...)`; the pass is skipped."), *Label));
			return false;
		}

		FDrawRules Rules;
		Rules.Mode = Mesh.Mode;
		Rules.Cull = Mesh.Cull;
		Rules.OverrideProxy = Mesh.Mode == EDreamPassMeshMode::Own ? nullptr : Pass.OverrideMaterialProxy;
		Rules.Label = Label;

		if (Mesh.Mode == EDreamPassMeshMode::Override && !Rules.OverrideProxy)
		{
			WarnOnce(Label + TEXT(".Mesh.NoOverride"),
				FString::Printf(TEXT("DreamPass: %s: `Mode = Override` needs a material (`Material =`); the pass is skipped."), *Label));
			return false;
		}
		if (Rules.OverrideProxy)
		{
			// While the override compiles, every primitive drawn with it would be skipped as if its usage flag were off;
			// the pass waits instead, as a fullscreen pass waits for its material. No primitive draws with the override,
			// so this asking is also what starts its compile.
			if (!IsMaterialReadyToDraw(*Rules.OverrideProxy, View.GetFeatureLevel()))
			{
				WarnOnce(Label + TEXT(".Mesh.Compiling"),
					FString::Printf(TEXT("DreamPass: %s waits for its override material to finish compiling (reported once)."), *Label));
				return false;
			}
		}

		// Select, cull and resolve before anything is added to the graph, so a pass with nothing to draw adds nothing.
		FProcessor Processor(Scene, View, Rules);
		FCollected Collected;
		CollectDraws(Context, *SceneInterface, Processor, Label, Collected);

		uint32 StencilMatchWords[8] = {};
		const bool bFillNanite = Collected.bAnyNanite && PrepareNaniteFill(Context, Label, StencilMatchWords);

		if (Collected.Items.IsEmpty() && !bFillNanite)
		{
			TouchOwnWrites(Context, Writes);
			return true;
		}

		TRDGUniformBufferRef<FSceneUniformParameters> SceneUniforms = nullptr;
		if (!Collected.Items.IsEmpty())
		{
			SceneUniforms = GetSceneUniformBufferRef(GraphBuilder, View);
			if (!SceneUniforms)
			{
				WarnOnce(Label + TEXT(".Mesh.NoSceneUniforms"),
					FString::Printf(TEXT("DreamPass: %s: the view has no scene renderer to draw meshes with; the pass is skipped."), *Label));
				return false;
			}
		}

		FTargets Targets;
		if (!ResolveTargets(Context, Label, Writes, Targets))
		{
			return false;
		}

		// The viewport is the targets' view rect, which for the scene colour or a GBuffer target is the raw view rect
		// (UE::FXRenderingUtils::GetRawViewRectUnsafe) and for an own buffer its whole extent. Where the two differ, the
		// material and the fullscreen helpers map the targets' pixels back onto the view's.
		FDepth Depth;
		if (!ResolveDepth(Context, Label, Targets, Depth))
		{
			return false;
		}
		const FVector4f TargetToView = MakeScaleBias(Targets.Rect, GetRenderViewRect(View));

		const TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures = GetPassSceneTextures(Context, Targets);
		FRHIBlendState* BlendState = GetPassBlendState(Mesh.Blend);

		if (bFillNanite)
		{
			AddNaniteFillPass(Context, Targets, Depth, SceneTextures, BlendState, TargetToView, StencilMatchWords);
		}

		if (!Collected.Items.IsEmpty())
		{
			FDreamMeshPassParameters* PassParameters = GraphBuilder.AllocParameters<FDreamMeshPassParameters>();
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->Scene = SceneUniforms;
			PassParameters->SceneTextures = SceneTextures;
			Targets.Bind(PassParameters->RenderTargets);
			if (Depth.bHasTarget)
			{
				PassParameters->RenderTargets.DepthStencil = Depth.Binding;
			}

			Processor.RenderState.SetBlendState(BlendState);
			Processor.RenderState.SetDepthStencilState(Depth.State);
			Processor.RenderState.SetDepthStencilAccess(Depth.Access);
			Processor.SvPositionScaleBias = TargetToView;

			const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();
			const TArray<FDrawItem>& Items = Collected.Items;

			// No instance culling manager -- the renderer's is a local a plugin cannot reach -- so every instance of a
			// selected primitive is drawn (R/Private/InstanceCulling/InstanceCullingContext.cpp:192, 749-750).
			AddSimpleMeshPass(GraphBuilder, PassParameters, Scene, View, nullptr, RDG_EVENT_NAME("DreamPass.Mesh (%d batches)", Items.Num()), Targets.Rect,
				[&Processor, &Items, &GraphBuilder, FeatureLevel](FDynamicPassMeshDrawListContext* DrawListContext)
				{
					Processor.SetDrawListContext(DrawListContext);

					// A draw command binds the material's uniform buffer as it is when the command is built: bring it up to date
					// first, once per material, as the precedents do (LandscapePhysicalMaterial.cpp:260, RenderTrace.cpp:421).
					TSet<const FMaterialRenderProxy*> Updated;
					for (const FDrawItem& Item : Items)
					{
						bool bAlreadyUpdated = false;
						Updated.Add(Item.MaterialProxy, &bAlreadyUpdated);
						if (!bAlreadyUpdated)
						{
							Item.MaterialProxy->UpdateUniformExpressionCacheIfNeeded(GraphBuilder.RHICmdList, FeatureLevel);
						}
						Processor.BuildItem(Item);
					}
				});
		}

		if (Targets.SceneColorSlot != INDEX_NONE)
		{
			CommitSceneColor(Context, Targets.SceneColor);
		}
		return true;
	}

	// --- AssignStencil (game thread) -------------------------------------------------------------------------------------

	void UpdateNaniteStencilAssignments(UDreamPassSubsystem& Subsystem, const FFamilySnapshot& Snapshot)
	{
		check(IsInGameThread());

		const UWorld* World = Subsystem.GetWorld();
		const bool bGameWorld = World && World->IsGameWorld();

		for (const FViewSnapshot& View : Snapshot.Views)
		{
			for (const FSnapshotPipeline& Pipeline : View.Pipelines)
			{
				for (const FSnapshotPass& Pass : Pipeline.Passes)
				{
					if (Pass.Kind != EDreamPassKind::Mesh || Pass.Mesh.Nanite != EDreamPassNanitePolicy::AssignStencil)
					{
						continue;
					}

					const FString Label = Pipeline.DebugName + TEXT(".") + Pass.Name.ToString();

					// The assignment changes the components' own custom depth settings, which a level saves: an editor world
					// would keep them in its package. Game and PIE worlds are thrown away.
					if (!bGameWorld)
					{
						WarnOnce(Label + TEXT(".Mesh.AssignStencilEditor"),
							FString::Printf(TEXT("DreamPass: %s: `Nanite = AssignStencil` changes the custom depth settings of components, which an editor world would save; it applies in game and PIE worlds only."), *Label));
						continue;
					}

					TArray<UPrimitiveComponent*> NaniteMembers;
					auto Gather = [&NaniteMembers](UPrimitiveComponent& Primitive)
					{
						// What the scene draws: the component's proxy, created on this thread, says whether it is Nanite
						// (the engine asks it the same way, E/Private/Components/StaticMeshComponent.cpp:2602). Without a proxy
						// the component is not in the scene and has no custom depth to write.
						const FPrimitiveSceneProxy* Proxy = Primitive.GetSceneProxy();
						if (Proxy && Proxy->IsNaniteMesh())
						{
							NaniteMembers.Add(&Primitive);
						}
					};

					for (const FDreamPassFilterClause& Clause : Pass.Mesh.Filter.AnyOf)
					{
						for (const FDreamPassFilterTerm& Term : Clause.AllOf)
						{
							if (Term.Kind == EDreamPassFilterKind::Layer)
							{
								Subsystem.ForEachPrimitiveInLayers(uint32(Term.LayerMask), Gather);
							}
							else if (Term.Kind == EDreamPassFilterKind::List)
							{
								Subsystem.ForEachPrimitiveInList(Term.ListName, Gather);
							}
						}
					}

					// A primitive two terms reach is asked for twice with the same value, which changes nothing.
					const int32 Value = FMath::Clamp(Pass.Mesh.AssignedStencilValue, 1, 255);
					for (UPrimitiveComponent* Primitive : NaniteMembers)
					{
						if (!Subsystem.RequestNaniteStencil(*Primitive, Value))
						{
							WarnOnce(Label + TEXT(".Mesh.AssignStencilConflict"),
								FString::Printf(TEXT("DreamPass: %s: a Nanite primitive is assigned different stencil values by two passes; the first keeps it."), *Label));
						}
					}
				}
			}
		}
	}
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
