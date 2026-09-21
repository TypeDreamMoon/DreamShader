// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// What the compiler service shares with the rest of this module, and with nothing outside it.
//
//   * CompileDreamShaderSourceFile -- the one routine behind every compile entry: the service's
//     CompileAssets / CompileMaterial, CompileDreamShaderLang2File, and a compile nested inside another
//     (a `.dsi` building its missing or stale parent first). It owns the outermost-only generation notice
//     and the warnings collector, so no entry point can forget either.
//   * FDreamShaderShaderCompileStallWatch (DSH9011) and ReportDreamShaderCustomNodeLoopHeuristic (DSH9012)
//     -- the two advisory warnings the 1.x generator raised around shader compilation, ported for the
//     emitter, which is where 2.0 compiles shaders.
//
// Defined in DreamShaderCompilerService.cpp. The module builds as one unity blob, so every name here is
// spelled to be unique in it.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilerInterface.h"
#include "DreamShaderDiagnostic.h"

class UMaterialExpressionCustom;

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Compiles one source file into its assets and words the result in the wire shape
	 * FDreamShaderCompileResult::Message documents.
	 *
	 * Opens a generation-notice scope. OnDreamShaderSourceGenerated fires when the OUTERMOST scope closes:
	 * first once for every other source a nested compile finished inside it (last result wins), then once for
	 * its own source. Warnings raised through RaiseGenerationWarning are collected across the whole outermost
	 * compile and appended to the outermost report only; a nested compile's report carries its own
	 * diagnostics and none of the collected lines, or the outer report would print them twice.
	 *
	 * Returns true when every product is current afterwards. OutError always holds the report; on failure its
	 * Code is the first error's.
	 */
	bool CompileDreamShaderSourceFile(
		const FString& SourceFilePath,
		bool bForce,
		::UE::DreamShader::EThinCustomPersistence Persistence,
		::UE::DreamShader::FDreamShaderError& OutError);

	/** The lines RaiseGenerationWarning collected since the current outermost compile began, in raise order, without duplicates. */
	const TArray<FString>& GetDreamShaderCollectedGenerationWarnings();

	/**
	 * DSH9011. Times one asset's shader-compilation stage from construction to destruction and, once it has
	 * run past GDreamShaderShaderCompileStallSeconds, raises the advisory warning that says what a stall of
	 * that length almost always is. Put one on the stack right before the call that compiles shaders.
	 * Reports from the destructor, so every exit path is covered.
	 */
	class FDreamShaderShaderCompileStallWatch
	{
	public:
		explicit FDreamShaderShaderCompileStallWatch(FString InLabel);
		~FDreamShaderShaderCompileStallWatch();

		FDreamShaderShaderCompileStallWatch(const FDreamShaderShaderCompileStallWatch&) = delete;
		FDreamShaderShaderCompileStallWatch& operator=(const FDreamShaderShaderCompileStallWatch&) = delete;

	private:
		FString Label;
		double StartSeconds = 0.0;
		bool bWarned = false;
	};

	/**
	 * DSH9012. Reads a finished, wired Custom node -- its code and its input pin names together -- and raises
	 * the advisory warning when the two spell a loop bounded by an input next to implicit-mip sampling, the
	 * shape that pins ShaderCompileWorker for minutes. NodeLabel names the node in the message.
	 */
	void ReportDreamShaderCustomNodeLoopHeuristic(const UMaterialExpressionCustom* CustomExpression, const FString& NodeLabel);
}
