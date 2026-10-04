// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The `.dsp` spelling of every DreamShaderPass enumeration the editor writes or shows, in one place: what the
// decompiler puts into a payload (IR::FIRPassPipeline carries every enumerated value as its canonical text), and what
// the graph dump, the details panel and the workspace's pass-keys.json print. The injection points and the buffer
// formats are the runtime's own tables (UE::DreamPass::LexToString); everything else is spelled here, the way the
// IR.h comments spell it. DreamShaderLang's binder keeps the same words, and a word that drifted reads as a re-parse
// difference (DSH9223) on the first decompile that uses it.
//
// Engine-free apart from the enums, so it builds on every engine the asset types build on.

#pragma once

#include "CoreMinimal.h"
#include "DreamPassTypes.h"

namespace UE::DreamShader::Editor::Private
{
	namespace PassSpelling
	{
		/** fullscreen, compute, mesh, clear, copy. */
		const TCHAR* Kind(EDreamPassKind Value);
		/** Render, Output, Fixed. */
		const TCHAR* Resolution(EDreamPassBufferResolution Value);
		/** float, float2, float3, float4, int, bool, Texture2D. */
		const TCHAR* ParameterType(EDreamPassParameterType Value);
		/** The number of components a parameter type has: 1..4, and 1 for int and bool; 0 for a texture. */
		int32 ParameterWidth(EDreamPassParameterType Value);
		/** Parameter, Constant, Weight. */
		const TCHAR* ParamSource(EDreamPassParamSource Value);
		/** Stencil, Layer, List. */
		const TCHAR* FilterKind(EDreamPassFilterKind Value);
		/** Override, Own, OwnOrOverride. */
		const TCHAR* MeshMode(EDreamPassMeshMode Value);
		/** TestScene, None, Own. */
		const TCHAR* Depth(EDreamPassDepthMode Value);
		/** Auto, Back, Front, None. */
		const TCHAR* Cull(EDreamPassCullMode Value);
		/** Replace, Add, Max, Min, AlphaBlend. */
		const TCHAR* Blend(EDreamPassBlendMode Value);
		/** Skip, StencilMask, AssignStencil. */
		const TCHAR* Nanite(EDreamPassNanitePolicy Value);
		/** Buffer, Fixed. */
		const TCHAR* DispatchMode(EDreamPassDispatchMode Value);

		/** Every value of one enumeration, in its declaration order: what pass-keys.json lists. */
		TConstArrayView<const TCHAR*> AllKinds();
		TConstArrayView<const TCHAR*> AllResolutions();
		TConstArrayView<const TCHAR*> AllParameterTypes();
		TConstArrayView<const TCHAR*> AllParamSources();
		TConstArrayView<const TCHAR*> AllFilterKinds();
		TConstArrayView<const TCHAR*> AllMeshModes();
		TConstArrayView<const TCHAR*> AllDepthModes();
		TConstArrayView<const TCHAR*> AllCullModes();
		TConstArrayView<const TCHAR*> AllBlendModes();
		TConstArrayView<const TCHAR*> AllNanitePolicies();

		/** One flag of a bitmask enumeration and its spelling. */
		struct FFlagSpelling
		{
			int32 Bit = 0;
			const TCHAR* Name = TEXT("");
		};

		/** Game, Editor, SceneCapture, PlanarReflection, ReflectionCapture, Thumbnail (EDreamPassViewFlags), in bit order. */
		TConstArrayView<FFlagSpelling> ViewFlags();
		/** PostProcess, SceneResolve, CustomStencil (EDreamPassRequirementFlags), in bit order. */
		TConstArrayView<FFlagSpelling> RequirementFlags();
		/** StaticMesh, InstancedStaticMeshes, SkeletalMesh, Landscape, SplineMesh, GeometryCache (EDreamPassMeshUsageFlags), in bit order. */
		TConstArrayView<FFlagSpelling> MeshUsageFlags();

		/** The spellings of the flags Mask has, in bit order. Bits no entry names are left out. */
		TArray<FString> FlagNames(TConstArrayView<FFlagSpelling> Table, int32 Mask);

		/** Every name of a table, in bit order. */
		TArray<FString> AllFlagNames(TConstArrayView<FFlagSpelling> Table);

		/** `Game | Editor`, the way a `.dsp` combines flags; `None` for an empty mask. */
		FString JoinFlags(TConstArrayView<FFlagSpelling> Table, int32 Mask);

		/** The mask a mesh pass checks when its source writes no `Usage` (the asset's default). */
		int32 DefaultMeshUsageMask();

		/** The views a pipeline runs in when its source writes no `Views` (the asset's default). */
		int32 DefaultViewMask();

		/** `C07` / `P03`: a slot as the slot directories name it (UE::DreamPass::GetSlotDirectory). */
		FString SlotLabel(bool bCompute, int32 Slot);
	}
}
