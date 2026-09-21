// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The compiler service: the one implementation of UE::DreamShader::IDreamShaderCompiler, plus the pieces
// that belong to "a compile" as a whole rather than to any stage inside it.
//
//   * FDreamShaderCompilerService -- registered by FDreamShaderCompilerModule::StartupModule through
//     RegisterDreamShaderCompiler, reached through GetDreamShaderCompiler(). It picks the front end by
//     extension, forwards the request's ThinCustom persistence, and words the result.
//   * OnDreamShaderSourceGenerated -- fired once per OUTERMOST compile of a source, by the service and
//     never by the pipeline: the pipeline runs inside the service, so firing in both would double every
//     notice.
//   * IsMemoryOnlyMaterial / MaterializeDreamShaderMaterial -- moved from UI/DreamShaderInstanceFactory.cpp
//     in the compiler relocation, because materializing IS a compile with a Materialized request.
//   * GetDreamShaderLastCompileDiagnostics -- each source's most recent compile as structured records, which
//     the bridge files into its diagnostics store instead of re-parsing a result's text.
//
// Also the service's, though declared in DreamShaderGeneratedAssets.h so that its call sites stay
// untouched: RaiseGenerationWarning, whose collector the service drains into a result's `Warnings:` block.
//
// Replaces the 1.x FMaterialGenerator facade (MaterialAssetGeneration/DreamShaderMaterialGenerator.h),
// FDreamShaderCompileService and the editor's compile adapter.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilerInterface.h"
// FLang2DiagnosticRecord: what GetDreamShaderLastCompileDiagnostics hands back.
#include "DreamShaderCompilerDiagnostics.h"

#include "Delegates/Delegate.h"

class UMaterialInterface;

namespace UE::DreamShader::Editor
{
	/**
	 * Fired once per outermost compile of a source file, after it succeeded or failed, with the
	 * normalized source path. Every compile route -- the bridge's watcher, a commandlet, the Material
	 * Content Browser, a provenance action, a test -- ends up in the compiler service, so this is the one
	 * place a "this source was just (re)built" signal is complete. A compile nested inside another (the
	 * material half of CompileAssets) does not fire; only the outer call does.
	 *
	 * The same declaration the 1.x generator header carried, moved: subscribers compile unchanged.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDreamShaderSourceGenerated, const FString& /*SourceFilePath*/, bool /*bSucceeded*/);
	DREAMSHADERCOMPILER_API FOnDreamShaderSourceGenerated& OnDreamShaderSourceGenerated();
}

namespace UE::DreamShader::Editor::Private
{
	/**
	 * True when Material's package has never been saved (PKG_NewlyCreated): a memory-only DreamShader
	 * product that materializing would move to disk.
	 */
	DREAMSHADERCOMPILER_API bool IsMemoryOnlyMaterial(UMaterialInterface* Material);

	/**
	 * Persists a memory-only DreamShader material -- the ThinCustom instance and its hidden base -- by
	 * compiling its source again with bForce and EThinCustomPersistence::Materialized, then reloading it
	 * by object path; returns the on-disk material. A material that is not memory-only is returned
	 * unchanged. Returns null with OutError when the material is not a UDreamShaderMaterialInstance with a
	 * source file, when the compile fails, or when the reload finds nothing at the object path.
	 */
	DREAMSHADERCOMPILER_API UMaterialInterface* MaterializeDreamShaderMaterial(UMaterialInterface* Material, FString& OutError);
}

namespace UE::DreamShader::Editor::Compiler
{
	/** The service. Game thread only, like the pipeline under it. */
	class DREAMSHADERCOMPILER_API FDreamShaderCompilerService final : public ::UE::DreamShader::IDreamShaderCompiler
	{
	public:
		/** The process-wide instance the module registers. */
		static FDreamShaderCompilerService& Get();

		virtual ::UE::DreamShader::FDreamShaderCompileResult CompileAssets(const ::UE::DreamShader::FDreamShaderCompileRequest& Request) override;
		virtual ::UE::DreamShader::FDreamShaderCompileResult CompileMaterial(const ::UE::DreamShader::FDreamShaderCompileRequest& Request) override;
	};

	/**
	 * Every diagnostic the most recent compile of SourceFilePath raised -- errors, warnings and infos, front end and
	 * emitter, the headers it includes as well -- as diagnostics-store records that keep their DSHnnnn code, stage,
	 * severity and span length. The bridge files these instead of re-parsing a result's Message, which keeps only a
	 * location and a text per line.
	 *
	 * Each compile of the source replaces them, a nested one included: a `.dsi` that builds its stale parent first
	 * leaves the parent's records under the parent's path. False, with OutRecords empty, for a source that has not been
	 * compiled in this session. Game thread only.
	 */
	DREAMSHADERCOMPILER_API bool GetDreamShaderLastCompileDiagnostics(const FString& SourceFilePath, TArray<FLang2DiagnosticRecord>& OutRecords);
}
