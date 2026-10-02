// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The `.dsp` vocabulary, and the few rules both of its readers have to apply the same way: the binder
// (Semantic/LangBinderPipeline.cpp), which turns a file into a payload, and the text side
// (Lang/LangPipelineSource.cpp), which turns a payload back into a file, splices one into a file and compares two.
// What "the default" of a key is has to be one answer for both -- otherwise a decompiled file states what the binder
// would have filled in anyway, or leaves out what it would not, and the round trip `.dsp` -> asset -> `.dsp` moves.
//
// Every spelling is the canonical `.dsp` one and compares case-sensitively. The two tables the runtime keeps too
// (injection points, buffer formats) are in the order of its enums (DreamShaderPass/Public/DreamPassTypes.h,
// EDreamPassInjection and EDreamPassBufferFormat); GetDreamShaderPassInjectionNames / GetDreamShaderPassFormatNames
// hand them out, and a test holds them equal to UE::DreamPass::LexToString.
//
// Core-only. Everything here is inline: two translation units include it and the module builds as one unity blob.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"

namespace UE::DreamShader::Lang::Private::PipelineVocabulary
{
	// ------------------------------------------------------------------------------------ tables

	/** EDreamPassInjection, in its order -- which is the frame order. */
	inline constexpr const TCHAR* InjectionNames[] =
	{
		TEXT("BeginView"),
		TEXT("BeforeBasePass"),
		TEXT("AfterBasePass"),
		TEXT("AfterOpaque"),
		TEXT("BeforePostProcess"),
		TEXT("PostProcess.BeforeDOF"),
		TEXT("PostProcess.AfterDOF"),
		TEXT("PostProcess.TranslucencyAfterDOF"),
		TEXT("PostProcess.ReplaceTonemapper"),
		TEXT("PostProcess.AfterMotionBlur"),
		TEXT("PostProcess.AfterTonemap"),
		TEXT("PostProcess.AfterFXAA"),
		TEXT("EndOfView"),
	};

	/** Indices into InjectionNames, for the rules that name a point. */
	namespace Injection
	{
		inline constexpr int32 BeginView = 0;
		inline constexpr int32 BeforeBasePass = 1;
		inline constexpr int32 AfterBasePass = 2;
		inline constexpr int32 AfterOpaque = 3;
		inline constexpr int32 BeforePostProcess = 4;
		inline constexpr int32 PostProcessBeforeDOF = 5;
		inline constexpr int32 PostProcessAfterDOF = 6;
		inline constexpr int32 PostProcessTranslucencyAfterDOF = 7;
		inline constexpr int32 PostProcessReplaceTonemapper = 8;
		inline constexpr int32 PostProcessAfterMotionBlur = 9;
		inline constexpr int32 PostProcessAfterTonemap = 10;
		inline constexpr int32 PostProcessAfterFXAA = 11;
		inline constexpr int32 EndOfView = 12;
	}

	/** `#pragma pipeline(Injection = ...)` when it is not written (FIRPassPipeline::DefaultInjection). */
	inline constexpr const TCHAR* DefaultInjectionName = TEXT("BeforePostProcess");

	/** EDreamPassBufferFormat, in its order. */
	inline constexpr const TCHAR* FormatNames[] =
	{
		TEXT("R8"),
		TEXT("RG8"),
		TEXT("RGBA8"),
		TEXT("R16F"),
		TEXT("RG16F"),
		TEXT("RGBA16F"),
		TEXT("R32F"),
		TEXT("RG32F"),
		TEXT("RGBA32F"),
		TEXT("R32U"),
		TEXT("RG32U"),
		TEXT("Depth32"),
	};

	inline constexpr const TCHAR* PassKinds[] = { TEXT("fullscreen"), TEXT("compute"), TEXT("mesh"), TEXT("clear"), TEXT("copy") };
	namespace Kind
	{
		inline constexpr int32 Fullscreen = 0;
		inline constexpr int32 Compute = 1;
		inline constexpr int32 Mesh = 2;
		inline constexpr int32 Clear = 3;
		inline constexpr int32 Copy = 4;
	}

	/** EDreamPassViewFlags, in bit order. */
	inline constexpr const TCHAR* ViewNames[] =
	{
		TEXT("Game"), TEXT("Editor"), TEXT("SceneCapture"), TEXT("PlanarReflection"), TEXT("ReflectionCapture"), TEXT("Thumbnail"),
	};
	/** `Views` when it is not written; an empty FIRPassPipeline::Views means this. */
	inline constexpr const TCHAR* DefaultViews[] = { TEXT("Game"), TEXT("Editor") };

	/** EDreamPassRequirementFlags, in bit order. */
	inline constexpr const TCHAR* RequirementNames[] = { TEXT("PostProcess"), TEXT("SceneResolve"), TEXT("CustomStencil") };

	/** What `Resolution = ...` may say; `Fixed` comes from `Size = int2(w, h)` and is never written. */
	inline constexpr const TCHAR* WrittenResolutionNames[] = { TEXT("Render"), TEXT("Output") };
	inline constexpr const TCHAR* ResolutionRender = TEXT("Render");
	inline constexpr const TCHAR* ResolutionOutput = TEXT("Output");
	inline constexpr const TCHAR* ResolutionFixed = TEXT("Fixed");

	inline constexpr const TCHAR* MeshModes[] = { TEXT("Override"), TEXT("Own"), TEXT("OwnOrOverride") };
	inline constexpr const TCHAR* DepthModes[] = { TEXT("TestScene"), TEXT("None"), TEXT("Own") };
	inline constexpr const TCHAR* CullModes[] = { TEXT("Auto"), TEXT("Back"), TEXT("Front"), TEXT("None") };
	inline constexpr const TCHAR* BlendModes[] = { TEXT("Replace"), TEXT("Add"), TEXT("Max"), TEXT("Min"), TEXT("AlphaBlend") };
	inline constexpr const TCHAR* NanitePolicies[] = { TEXT("Skip"), TEXT("StencilMask"), TEXT("AssignStencil") };

	/** EDreamPassMeshUsageFlags, in bit order. */
	inline constexpr const TCHAR* UsageNames[] =
	{
		TEXT("StaticMesh"), TEXT("InstancedStaticMeshes"), TEXT("SkeletalMesh"), TEXT("Landscape"), TEXT("SplineMesh"), TEXT("GeometryCache"),
	};
	/** `Usage` when it is not written (FDreamPassMeshSettings::Usage's default); an empty FIRPass::Usage means this. */
	inline constexpr const TCHAR* DefaultUsage[] = { TEXT("StaticMesh"), TEXT("InstancedStaticMeshes"), TEXT("SkeletalMesh") };

	/** The textures a pass binds without declaring them (UE::DreamPass::BuiltinBuffers). */
	inline constexpr const TCHAR* BuiltinBufferNames[] =
	{
		TEXT("SceneColor"), TEXT("SceneDepth"), TEXT("CustomDepth"), TEXT("CustomStencil"),
		TEXT("GBufferA"), TEXT("GBufferB"), TEXT("GBufferC"), TEXT("GBufferD"), TEXT("GBufferE"), TEXT("GBufferF"),
		TEXT("Velocity"), TEXT("Translucency"),
	};
	inline constexpr const TCHAR* SceneColorName = TEXT("SceneColor");
	inline constexpr const TCHAR* SceneDepthName = TEXT("SceneDepth");

	/** What a pipeline `uniform` may be declared as; FIRPassParameter::Type. */
	inline constexpr const TCHAR* ParameterTypeNames[] =
	{
		TEXT("float"), TEXT("float2"), TEXT("float3"), TEXT("float4"), TEXT("int"), TEXT("bool"), TEXT("Texture2D"),
	};

	/** The pipeline's total weight in a view: a `param` source, and never a declared name (UE::DreamPass::WeightParameterName). */
	inline constexpr const TCHAR* WeightParameterName = TEXT("DreamPassWeight");

	/** The keys, each table in the order the canonical form writes them (DreamShader_Plan/05 s3, s4, s5.1, s5.2). */
	inline constexpr const TCHAR* PragmaKeys[] = { TEXT("Order"), TEXT("Injection"), TEXT("Views"), TEXT("Requires"), TEXT("Enabled") };
	inline constexpr const TCHAR* BufferKeys[] = { TEXT("Scale"), TEXT("Size"), TEXT("Resolution"), TEXT("Clear"), TEXT("Mips"), TEXT("History"), TEXT("Export") };
	inline constexpr const TCHAR* CommonPassKeys[] = { TEXT("Injection"), TEXT("Enabled") };
	inline constexpr const TCHAR* FullscreenKeys[] = { TEXT("Material"), TEXT("Shader"), TEXT("Entry") };
	inline constexpr const TCHAR* ComputeKeys[] = { TEXT("Shader"), TEXT("Entry"), TEXT("Threads"), TEXT("Dispatch") };
	inline constexpr const TCHAR* MeshKeys[] =
	{
		TEXT("Filter"), TEXT("Material"), TEXT("Mode"), TEXT("Depth"), TEXT("Cull"), TEXT("Blend"), TEXT("Usage"), TEXT("Nanite"), TEXT("NaniteValue"),
	};
	inline constexpr const TCHAR* ClearKeys[] = { TEXT("Value") };

	/** The filter terms of a mesh pass. */
	inline constexpr const TCHAR* FilterStencil = TEXT("Stencil");
	inline constexpr const TCHAR* FilterLayer = TEXT("Layer");
	inline constexpr const TCHAR* FilterList = TEXT("List");

	// ------------------------------------------------------------------------------------ limits

	/** The fixed parameter block of the HLSL slots (UE::DreamPass::MaxSlotInputs, MaxSlotOutputs, MaxSlotParamVectors). */
	inline constexpr int32 MaxSlotInputs = 8;
	inline constexpr int32 MaxSlotOutputs = 4;
	inline constexpr int32 MaxSlotParamVectors = 16;
	/** UE.DreamPassOutput's outputs (UE::DreamPass::MaxMeshOutputs). */
	inline constexpr int32 MaxMeshOutputs = 4;
	/**
	 * The post-process material input slots a fullscreen material pass shares with the material's own SceneTexture
	 * nodes: five, starting at 0, the engine's rule (DreamShader_Plan/09, "UserSceneTexture slots start at 0").
	 */
	inline constexpr int32 MaterialInputSlots = 5;
	/** The project settings' pass layer table (D-7). */
	inline constexpr int32 MaxPassLayers = 32;

	// ------------------------------------------------------------------------------------ lookups

	/** Index of Text in Names, case-sensitive; INDEX_NONE when absent. */
	inline int32 FindName(TConstArrayView<const TCHAR*> Names, const FString& Text)
	{
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			if (Text.Equals(Names[Index], ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** The one entry that differs from Text in case only, for "did you mean"; empty when there is none. */
	inline FString FindNameIgnoringCase(TConstArrayView<const TCHAR*> Names, const FString& Text)
	{
		for (const TCHAR* Name : Names)
		{
			if (Text.Equals(Name, ESearchCase::IgnoreCase) && !Text.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Name;
			}
		}
		return FString();
	}

	inline bool HasName(TConstArrayView<const TCHAR*> Names, const FString& Text)
	{
		return FindName(Names, Text) != INDEX_NONE;
	}

	/** `A, B and C` -- for a message listing the accepted spellings. */
	inline FString JoinNames(TConstArrayView<const TCHAR*> Names)
	{
		FString Result;
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += (Index + 1 == Names.Num()) ? TEXT(" and ") : TEXT(", ");
			}
			Result += Names[Index];
		}
		return Result;
	}

	inline int32 FindInjection(const FString& Name)
	{
		return FindName(InjectionNames, Name);
	}

	/**
	 * Whether a point sees the output (post-upscale) view rect: the post-process chain from the tonemapper replacement
	 * on, and the end of the view (UE::DreamPass::IsOutputResolutionInjection).
	 */
	inline bool IsOutputResolutionInjection(const int32 InjectionIndex)
	{
		return InjectionIndex >= Injection::PostProcessReplaceTonemapper;
	}

	inline bool IsPostProcessInjection(const int32 InjectionIndex)
	{
		return InjectionIndex >= Injection::PostProcessBeforeDOF && InjectionIndex <= Injection::PostProcessAfterFXAA;
	}

	/**
	 * Whether scene colour is tonemapped (or being tonemapped) at a point: what decides the BlendableLocation a post-process
	 * material run there should be compiled for (POST_PROCESS_MATERIAL_BEFORE_TONEMAP is 0 for SceneColorAfterTonemapping
	 * and ReplacingTonemapper, R/Private/PostProcess/PostProcessMaterial.cpp:265).
	 */
	inline bool IsAfterTonemapInjection(const int32 InjectionIndex)
	{
		return InjectionIndex == Injection::PostProcessReplaceTonemapper || InjectionIndex >= Injection::PostProcessAfterTonemap;
	}

	inline bool IsBuiltinBuffer(const FString& Name)
	{
		return HasName(BuiltinBufferNames, Name);
	}

	inline bool IsDepthFormat(const FString& Format)
	{
		return Format.Equals(TEXT("Depth32"), ESearchCase::CaseSensitive);
	}

	inline bool IsIntegerFormat(const FString& Format)
	{
		return Format.Equals(TEXT("R32U"), ESearchCase::CaseSensitive) || Format.Equals(TEXT("RG32U"), ESearchCase::CaseSensitive);
	}

	/** The keys of one pass kind (CommonPassKeys excluded); empty for `copy` and for an unknown kind. */
	inline TConstArrayView<const TCHAR*> KeysOfKind(const int32 KindIndex)
	{
		switch (KindIndex)
		{
		case Kind::Fullscreen: return MakeArrayView(FullscreenKeys);
		case Kind::Compute:    return MakeArrayView(ComputeKeys);
		case Kind::Mesh:       return MakeArrayView(MeshKeys);
		case Kind::Clear:      return MakeArrayView(ClearKeys);
		default:               return TConstArrayView<const TCHAR*>();
		}
	}

	// ----------------------------------------------------------------------------- frame and defaults

	inline int32 InjectionIndexOf(const IR::FIRPass& Pass)
	{
		return FindInjection(Pass.Injection);
	}

	/** Pass indices in frame order: by injection point, then in declaration order. An unknown point sorts last. */
	inline TArray<int32> PassesInFrameOrder(const IR::FIRPassPipeline& Pipeline)
	{
		TArray<int32> Order;
		Order.Reserve(Pipeline.Passes.Num());
		for (int32 Index = 0; Index < Pipeline.Passes.Num(); ++Index)
		{
			Order.Add(Index);
		}
		const auto Rank = [&Pipeline](const int32 PassIndex)
		{
			const int32 InjectionIndex = InjectionIndexOf(Pipeline.Passes[PassIndex]);
			return InjectionIndex == INDEX_NONE ? 1000 : InjectionIndex;
		};
		Order.StableSort([&Rank](const int32 A, const int32 B) { return Rank(A) < Rank(B); });
		return Order;
	}

	/** Whether Pass writes the buffer: one of its `write`s, or its own depth (`Depth = Own(Buffer)`). */
	inline bool PassWritesBuffer(const IR::FIRPass& Pass, const FString& Buffer)
	{
		for (const IR::FIRPassBinding& Binding : Pass.Writes)
		{
			if (Binding.Buffer.Equals(Buffer, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return Pass.Kind.Equals(PassKinds[Kind::Mesh], ESearchCase::CaseSensitive)
			&& Pass.Depth.Equals(TEXT("Own"), ESearchCase::CaseSensitive)
			&& Pass.DepthBuffer.Equals(Buffer, ESearchCase::CaseSensitive);
	}

	/**
	 * The resolution a buffer has when it neither says `Resolution` nor `Size`: the one its first writer's injection point
	 * draws at (Output from PostProcess.ReplaceTonemapper on, Render before). Render when nothing writes it.
	 */
	inline FString InferBufferResolution(const IR::FIRPassPipeline& Pipeline, const FString& BufferName)
	{
		for (const int32 PassIndex : PassesInFrameOrder(Pipeline))
		{
			const IR::FIRPass& Pass = Pipeline.Passes[PassIndex];
			if (PassWritesBuffer(Pass, BufferName))
			{
				const int32 InjectionIndex = InjectionIndexOf(Pass);
				return (InjectionIndex != INDEX_NONE && IsOutputResolutionInjection(InjectionIndex)) ? FString(ResolutionOutput) : FString(ResolutionRender);
			}
		}
		return ResolutionRender;
	}

	inline bool PassHasMaterial(const IR::FIRPass& Pass)
	{
		return !Pass.MaterialReference.IsEmpty() || !Pass.MaterialObjectPath.IsEmpty();
	}

	inline bool PassHasShader(const IR::FIRPass& Pass)
	{
		return !Pass.ShaderReference.IsEmpty() || !Pass.ShaderVirtualPath.IsEmpty();
	}

	/** A mesh pass's `Mode` when it is not written: Override with a `Material`, Own without (05 s5.2). */
	inline FString DefaultMeshMode(const IR::FIRPass& Pass)
	{
		return PassHasMaterial(Pass) ? FString(TEXT("Override")) : FString(TEXT("Own"));
	}

	inline FString EffectiveMeshMode(const IR::FIRPass& Pass)
	{
		return Pass.MeshMode.IsEmpty() ? DefaultMeshMode(Pass) : Pass.MeshMode;
	}

	inline bool FilterUsesStencil(const IR::FIRPass& Pass)
	{
		for (const IR::FIRPassFilterClause& Clause : Pass.Filter)
		{
			for (const IR::FIRPassFilterTerm& Term : Clause.AllOf)
			{
				if (Term.Kind.Equals(FilterStencil, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
		}
		return false;
	}

	/** `Nanite` when it is not written: StencilMask for a filter with a Stencil term, Skip otherwise (03 s5.5). */
	inline FString DefaultNanitePolicy(const IR::FIRPass& Pass)
	{
		return FilterUsesStencil(Pass) ? FString(TEXT("StencilMask")) : FString(TEXT("Skip"));
	}

	/** The usage flags a mesh pass checks for, canonical order: an empty FIRPass::Usage is the default set. */
	inline TArray<FString> EffectiveUsage(const IR::FIRPass& Pass)
	{
		TArray<FString> Result;
		if (Pass.Usage.Num() == 0)
		{
			for (const TCHAR* Name : DefaultUsage)
			{
				Result.Add(Name);
			}
			return Result;
		}
		for (const TCHAR* Name : UsageNames)
		{
			if (Pass.Usage.ContainsByPredicate([Name](const FString& Written) { return Written.Equals(Name, ESearchCase::CaseSensitive); }))
			{
				Result.Add(Name);
			}
		}
		return Result;
	}

	/** The views a pipeline runs in, canonical order: an empty FIRPassPipeline::Views is Game | Editor. */
	inline TArray<FString> EffectiveViews(const IR::FIRPassPipeline& Pipeline)
	{
		TArray<FString> Result;
		if (Pipeline.Views.Num() == 0)
		{
			for (const TCHAR* Name : DefaultViews)
			{
				Result.Add(Name);
			}
			return Result;
		}
		for (const TCHAR* Name : ViewNames)
		{
			if (Pipeline.Views.ContainsByPredicate([Name](const FString& Written) { return Written.Equals(Name, ESearchCase::CaseSensitive); }))
			{
				Result.Add(Name);
			}
		}
		return Result;
	}

	/** A compute pass's `Dispatch` when it is not written: one thread per texel of its first write. */
	inline FString DefaultDispatchBuffer(const IR::FIRPass& Pass)
	{
		return Pass.Writes.Num() > 0 ? Pass.Writes[0].Buffer : FString();
	}

	/**
	 * A pass whose code is HLSL: compute, or fullscreen without a material -- with `Shader =`, or with its code in the `.dsp`
	 * (FIRPass::HlslSource, DreamShader_Plan/10). Its bindings are HLSL names.
	 */
	inline bool IsHlslPass(const IR::FIRPass& Pass)
	{
		return Pass.Kind.Equals(PassKinds[Kind::Compute], ESearchCase::CaseSensitive)
			|| (Pass.Kind.Equals(PassKinds[Kind::Fullscreen], ESearchCase::CaseSensitive) && !PassHasMaterial(Pass)
				&& (PassHasShader(Pass) || IR::PassHlslSource::IsInline(Pass.HlslSource)));
	}

	/**
	 * A pass that runs in a global shader slot, with its fixed parameter block (MaxSlotInputs, MaxSlotOutputs,
	 * MaxSlotParamVectors): compute (the compute slot) and a fullscreen `.usf` (the pixel slot, FDreamPassPS) whatever its
	 * output count, 1 to 4. A fullscreen material pass runs no slot. No material is ever generated around a `.usf`
	 * (DreamShader_Plan/09, deviation table; supersedes 03 s4.1 and 04 s1): every HLSL pass is a slot pass.
	 */
	inline bool IsSlotPass(const IR::FIRPass& Pass)
	{
		return IsHlslPass(Pass);
	}

	/** How many float4 components a value of a parameter type spelling takes in DP_Params; 0 for a texture (no slot form). */
	inline int32 SlotWidthOfType(const FString& Type)
	{
		if (Type.Equals(TEXT("float"), ESearchCase::CaseSensitive) || Type.Equals(TEXT("int"), ESearchCase::CaseSensitive) || Type.Equals(TEXT("bool"), ESearchCase::CaseSensitive))
		{
			return 1;
		}
		if (Type.Equals(TEXT("float2"), ESearchCase::CaseSensitive))
		{
			return 2;
		}
		if (Type.Equals(TEXT("float3"), ESearchCase::CaseSensitive))
		{
			return 3;
		}
		if (Type.Equals(TEXT("float4"), ESearchCase::CaseSensitive))
		{
			return 4;
		}
		return 0;
	}

	/**
	 * The packing rule of UE::DreamPass::LayoutSlotParameters (DreamShaderPass/Private/DreamPassPipeline.cpp), over widths
	 * in declaration order: a float4 takes a vector of its own, a float3 the xyz of an empty vector, a float2 the xy or
	 * zw half that is free, a scalar the next free component. False when MaxSlotParamVectors are not enough, or a
	 * width is not 1..4. Kept in step with the runtime by hand; a test feeds both the same widths.
	 */
	inline bool FitSlotParameters(TConstArrayView<int32> Widths)
	{
		uint8 UsedMask[MaxSlotParamVectors] = {};
		for (const int32 Width : Widths)
		{
			if (Width < 1 || Width > 4)
			{
				return false;
			}
			int32 FoundVector = INDEX_NONE;
			int32 FoundComponent = 0;
			for (int32 Vector = 0; Vector < MaxSlotParamVectors && FoundVector == INDEX_NONE; ++Vector)
			{
				const uint8 Mask = UsedMask[Vector];
				if (Width >= 3)
				{
					if (Mask == 0)
					{
						FoundVector = Vector;
						FoundComponent = 0;
					}
				}
				else if (Width == 2)
				{
					if ((Mask & 0x3) == 0)
					{
						FoundVector = Vector;
						FoundComponent = 0;
					}
					else if ((Mask & 0xC) == 0)
					{
						FoundVector = Vector;
						FoundComponent = 2;
					}
				}
				else
				{
					for (int32 Component = 0; Component < 4; ++Component)
					{
						if ((Mask & (1 << Component)) == 0)
						{
							FoundVector = Vector;
							FoundComponent = Component;
							break;
						}
					}
				}
			}
			if (FoundVector == INDEX_NONE)
			{
				return false;
			}
			UsedMask[FoundVector] |= uint8(((1 << Width) - 1) << FoundComponent);
		}
		return true;
	}

	// ----------------------------------------------------------------------------- values

	/** Two doubles that are one float32: what the asset stores, so what a round trip can promise. */
	inline bool SameFloat(const double A, const double B)
	{
		return static_cast<float>(A) == static_cast<float>(B);
	}

	inline bool SameFloat4(const double A[4], const double B[4])
	{
		return SameFloat(A[0], B[0]) && SameFloat(A[1], B[1]) && SameFloat(A[2], B[2]) && SameFloat(A[3], B[3]);
	}

	inline bool IsZero4(const double V[4])
	{
		return V[0] == 0.0 && V[1] == 0.0 && V[2] == 0.0 && V[3] == 0.0;
	}

	/** Whether Text is an identifier the 2.0 lexer reads as one word (keywords are the caller's question). */
	inline bool IsPipelineIdentifierText(const FString& Text)
	{
		if (Text.IsEmpty())
		{
			return false;
		}
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];
			const bool bLetter = (Char >= TEXT('A') && Char <= TEXT('Z')) || (Char >= TEXT('a') && Char <= TEXT('z')) || Char == TEXT('_');
			const bool bDigit = Char >= TEXT('0') && Char <= TEXT('9');
			if (!(bLetter || (bDigit && Index > 0)))
			{
				return false;
			}
		}
		return true;
	}
}
