// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The PassPipeline product of a `.dsp`: one UDreamPassPipeline (the DreamShaderPass module), the render targets of its
// exported buffers next to it, and the HLSL slots of its HLSL passes (Pass/DreamShaderPassSlotRegistry.h).
//
// The order is the material emitter's, for the same reasons (DreamShaderIREmitter.cpp): the destination and the
// ownership guard, the rebuild gates -- write owner, source hash, open editor, divergence -- and only then anything that
// writes. What is new is that a `.dsp` writes outside its package: the slot registry and the snapshots. So everything is
// staged first -- the pipeline's data onto a transient copy, the slots planned and pre-checked -- and the first write
// happens only once nothing is left that can fail: a pre-check failure leaves the asset, the registry and the snapshots
// exactly as they were, and the previous version keeps running.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderIREmitter.h"
#include "DreamShaderTypes.h"
#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"
#include "Pass/DreamShaderPassSlotRegistry.h"

class UDreamPassPipeline;
class UObject;

namespace UE::DreamShader::Editor::Compiler
{
	/** EmitDreamShaderIRProduct's PassPipeline branch. Same contract: true when the asset is current afterwards, skips included. */
	bool EmitPassPipelineProduct(
		const IR::FIRProduct& Product,
		const FTextShaderDefinition& Definition,
		const FIREmitContext& Context,
		UObject*& OutAsset,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * A PassPipeline product's payload as a pipeline asset's data, onto Target: every canonical spelling mapped onto the
	 * DreamShaderPass enums, layer names kept AND turned into the project's layer bits (the names are what the decompiler
	 * writes back; the bits alone go stale when the layer table is reordered), materials and texture defaults loaded, shader
	 * paths kept as the `.dsp` writes them. Leaves ExportTargets, the slots and the source fields alone. False with
	 * diagnostics when a value does not map or an asset does not load.
	 */
	bool BuildDreamPassPipelineFromPayload(const IR::FIRProduct& Product, UDreamPassPipeline& Target, Lang::FLangDiagnosticSink& Diagnostics);

	/** The HLSL passes of a pipeline built from Payload, as slot candidates (Payload and Staged hold the passes in the same order). */
	void MakeDreamPassSlotCandidates(const IR::FIRPassPipeline& Payload, const UDreamPassPipeline& Staged, TArray<FDreamPassSlotCandidate>& OutCandidates);

	/**
	 * Where the render target of an exported buffer lives: `<pipeline's folder>/<Pipeline>_<Buffer>`, sanitized. The one
	 * rule the emitter creates them by and product resolution names them by. Returns the object path.
	 */
	FString MakeDreamPassExportTargetPath(
		const FString& PipelinePackageName,
		const FString& PipelineAssetName,
		const FString& BufferName,
		FString* OutPackageName = nullptr,
		FString* OutAssetName = nullptr);

	/** Whether a buffer format (its `.dsp` spelling) can be exported: a float or normalized one; never R32U, RG32U, Depth32. */
	bool IsDreamPassExportableBufferFormat(const FString& FormatSpelling);

	/** The emitter's own rebuild gates (DreamShaderIREmitter.cpp, CheckRebuildPreconditions), for a product emitted outside that file. */
	bool CheckDreamShaderIRRebuildPreconditions(
		UObject* Asset,
		const FIREmitContext& Context,
		const IR::FIRProduct& Product,
		bool bAllowHashSkip,
		bool bWouldPersist,
		Lang::FLangDiagnosticSink& Diagnostics,
		bool& bOutSkip);

	/** DreamShaderIREmitter.cpp's WouldBuildPersist: whether this build ends in a file, and whether it may write one. */
	bool WouldDreamShaderIRBuildPersist(UObject* Asset, bool& bOutSaveToDisk);
}
