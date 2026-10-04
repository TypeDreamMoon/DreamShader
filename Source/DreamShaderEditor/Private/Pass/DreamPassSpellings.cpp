// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamPassSpellings.h.
//
// Diagnostics owned by this file: none.

#include "Pass/DreamPassSpellings.h"

namespace UE::DreamShader::Editor::Private
{
	namespace PassSpelling
	{
		namespace
		{
			// Indexed by the enumerator's value: every one of these enums counts from 0 without a gap. The asserts hold the
			// tables to the enums' lengths, so a new enumerator is a compile error here instead of a "?" in a decompile.
			const TCHAR* const KindNames[] = { TEXT("fullscreen"), TEXT("compute"), TEXT("mesh"), TEXT("clear"), TEXT("copy") };
			static_assert(UE_ARRAY_COUNT(KindNames) == int32(EDreamPassKind::Copy) + 1, "Every pass kind needs a spelling.");

			const TCHAR* const ResolutionNames[] = { TEXT("Render"), TEXT("Output"), TEXT("Fixed") };
			static_assert(UE_ARRAY_COUNT(ResolutionNames) == int32(EDreamPassBufferResolution::Fixed) + 1, "Every buffer resolution needs a spelling.");

			const TCHAR* const ParameterTypeNames[] = { TEXT("float"), TEXT("float2"), TEXT("float3"), TEXT("float4"), TEXT("int"), TEXT("bool"), TEXT("Texture2D") };
			static_assert(UE_ARRAY_COUNT(ParameterTypeNames) == int32(EDreamPassParameterType::Texture) + 1, "Every parameter type needs a spelling.");

			const TCHAR* const ParamSourceNames[] = { TEXT("Parameter"), TEXT("Constant"), TEXT("Weight") };
			static_assert(UE_ARRAY_COUNT(ParamSourceNames) == int32(EDreamPassParamSource::Weight) + 1, "Every param source needs a spelling.");

			const TCHAR* const FilterKindNames[] = { TEXT("Stencil"), TEXT("Layer"), TEXT("List") };
			static_assert(UE_ARRAY_COUNT(FilterKindNames) == int32(EDreamPassFilterKind::List) + 1, "Every filter kind needs a spelling.");

			const TCHAR* const MeshModeNames[] = { TEXT("Override"), TEXT("Own"), TEXT("OwnOrOverride") };
			static_assert(UE_ARRAY_COUNT(MeshModeNames) == int32(EDreamPassMeshMode::OwnOrOverride) + 1, "Every mesh mode needs a spelling.");

			const TCHAR* const DepthNames[] = { TEXT("TestScene"), TEXT("None"), TEXT("Own") };
			static_assert(UE_ARRAY_COUNT(DepthNames) == int32(EDreamPassDepthMode::Own) + 1, "Every depth mode needs a spelling.");

			const TCHAR* const CullNames[] = { TEXT("Auto"), TEXT("Back"), TEXT("Front"), TEXT("None") };
			static_assert(UE_ARRAY_COUNT(CullNames) == int32(EDreamPassCullMode::None) + 1, "Every cull mode needs a spelling.");

			const TCHAR* const BlendNames[] = { TEXT("Replace"), TEXT("Add"), TEXT("Max"), TEXT("Min"), TEXT("AlphaBlend") };
			static_assert(UE_ARRAY_COUNT(BlendNames) == int32(EDreamPassBlendMode::AlphaBlend) + 1, "Every blend mode needs a spelling.");

			const TCHAR* const NaniteNames[] = { TEXT("Skip"), TEXT("StencilMask"), TEXT("AssignStencil") };
			static_assert(UE_ARRAY_COUNT(NaniteNames) == int32(EDreamPassNanitePolicy::AssignStencil) + 1, "Every Nanite policy needs a spelling.");

			const TCHAR* const DispatchModeNames[] = { TEXT("Buffer"), TEXT("Fixed") };
			static_assert(UE_ARRAY_COUNT(DispatchModeNames) == int32(EDreamPassDispatchMode::Fixed) + 1, "Every dispatch mode needs a spelling.");

			const FFlagSpelling ViewFlagNames[] =
			{
				{ int32(EDreamPassViewFlags::Game),              TEXT("Game") },
				{ int32(EDreamPassViewFlags::Editor),            TEXT("Editor") },
				{ int32(EDreamPassViewFlags::SceneCapture),      TEXT("SceneCapture") },
				{ int32(EDreamPassViewFlags::PlanarReflection),  TEXT("PlanarReflection") },
				{ int32(EDreamPassViewFlags::ReflectionCapture), TEXT("ReflectionCapture") },
				{ int32(EDreamPassViewFlags::Thumbnail),         TEXT("Thumbnail") },
			};

			const FFlagSpelling RequirementFlagNames[] =
			{
				{ int32(EDreamPassRequirementFlags::PostProcess),   TEXT("PostProcess") },
				{ int32(EDreamPassRequirementFlags::SceneResolve),  TEXT("SceneResolve") },
				{ int32(EDreamPassRequirementFlags::CustomStencil), TEXT("CustomStencil") },
			};

			const FFlagSpelling MeshUsageFlagNames[] =
			{
				{ int32(EDreamPassMeshUsageFlags::StaticMesh),            TEXT("StaticMesh") },
				{ int32(EDreamPassMeshUsageFlags::InstancedStaticMeshes), TEXT("InstancedStaticMeshes") },
				{ int32(EDreamPassMeshUsageFlags::SkeletalMesh),          TEXT("SkeletalMesh") },
				{ int32(EDreamPassMeshUsageFlags::Landscape),             TEXT("Landscape") },
				{ int32(EDreamPassMeshUsageFlags::SplineMesh),            TEXT("SplineMesh") },
				{ int32(EDreamPassMeshUsageFlags::GeometryCache),         TEXT("GeometryCache") },
			};

			template <typename TEnum, SIZE_T N>
			const TCHAR* Spell(const TCHAR* const (&Table)[N], const TEnum Value)
			{
				const int32 Index = int32(Value);
				return Index >= 0 && Index < int32(N) ? Table[Index] : TEXT("?");
			}
		}

		const TCHAR* Kind(const EDreamPassKind Value) { return Spell(KindNames, Value); }
		const TCHAR* Resolution(const EDreamPassBufferResolution Value) { return Spell(ResolutionNames, Value); }
		const TCHAR* ParameterType(const EDreamPassParameterType Value) { return Spell(ParameterTypeNames, Value); }
		const TCHAR* ParamSource(const EDreamPassParamSource Value) { return Spell(ParamSourceNames, Value); }
		const TCHAR* FilterKind(const EDreamPassFilterKind Value) { return Spell(FilterKindNames, Value); }
		const TCHAR* MeshMode(const EDreamPassMeshMode Value) { return Spell(MeshModeNames, Value); }
		const TCHAR* Depth(const EDreamPassDepthMode Value) { return Spell(DepthNames, Value); }
		const TCHAR* Cull(const EDreamPassCullMode Value) { return Spell(CullNames, Value); }
		const TCHAR* Blend(const EDreamPassBlendMode Value) { return Spell(BlendNames, Value); }
		const TCHAR* Nanite(const EDreamPassNanitePolicy Value) { return Spell(NaniteNames, Value); }
		const TCHAR* DispatchMode(const EDreamPassDispatchMode Value) { return Spell(DispatchModeNames, Value); }

		int32 ParameterWidth(const EDreamPassParameterType Type)
		{
			switch (Type)
			{
			case EDreamPassParameterType::Float2: return 2;
			case EDreamPassParameterType::Float3: return 3;
			case EDreamPassParameterType::Float4: return 4;
			case EDreamPassParameterType::Texture: return 0;
			default: return 1;
			}
		}

		TConstArrayView<const TCHAR*> AllKinds() { return KindNames; }
		TConstArrayView<const TCHAR*> AllResolutions() { return ResolutionNames; }
		TConstArrayView<const TCHAR*> AllParameterTypes() { return ParameterTypeNames; }
		TConstArrayView<const TCHAR*> AllParamSources() { return ParamSourceNames; }
		TConstArrayView<const TCHAR*> AllFilterKinds() { return FilterKindNames; }
		TConstArrayView<const TCHAR*> AllMeshModes() { return MeshModeNames; }
		TConstArrayView<const TCHAR*> AllDepthModes() { return DepthNames; }
		TConstArrayView<const TCHAR*> AllCullModes() { return CullNames; }
		TConstArrayView<const TCHAR*> AllBlendModes() { return BlendNames; }
		TConstArrayView<const TCHAR*> AllNanitePolicies() { return NaniteNames; }

		TConstArrayView<FFlagSpelling> ViewFlags() { return ViewFlagNames; }
		TConstArrayView<FFlagSpelling> RequirementFlags() { return RequirementFlagNames; }
		TConstArrayView<FFlagSpelling> MeshUsageFlags() { return MeshUsageFlagNames; }

		TArray<FString> FlagNames(const TConstArrayView<FFlagSpelling> Table, const int32 Mask)
		{
			TArray<FString> Names;
			for (const FFlagSpelling& Flag : Table)
			{
				if ((Mask & Flag.Bit) != 0)
				{
					Names.Add(Flag.Name);
				}
			}
			return Names;
		}

		TArray<FString> AllFlagNames(const TConstArrayView<FFlagSpelling> Table)
		{
			TArray<FString> Names;
			for (const FFlagSpelling& Flag : Table)
			{
				Names.Add(Flag.Name);
			}
			return Names;
		}

		FString JoinFlags(const TConstArrayView<FFlagSpelling> Table, const int32 Mask)
		{
			const TArray<FString> Names = FlagNames(Table, Mask);
			return Names.IsEmpty() ? FString(TEXT("None")) : FString::Join(Names, TEXT(" | "));
		}

		int32 DefaultMeshUsageMask()
		{
			// FDreamPassMeshSettings::Usage's initializer, read off a default-constructed struct rather than restated.
			return FDreamPassMeshSettings().Usage;
		}

		int32 DefaultViewMask()
		{
			// UDreamPassPipeline::Views' initializer.
			return int32(EDreamPassViewFlags::Game | EDreamPassViewFlags::Editor);
		}

		FString SlotLabel(const bool bCompute, const int32 Slot)
		{
			return FString::Printf(TEXT("%c%02d"), bCompute ? TCHAR('C') : TCHAR('P'), Slot); // I18N-EXEMPT: a slot's directory name
		}
	}
}
