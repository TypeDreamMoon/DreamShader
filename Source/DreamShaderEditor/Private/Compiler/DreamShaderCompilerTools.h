// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Two things live here, and the seam between them is the point of the file.
//
// PART 1 is the pipeline's rich entry point. The frozen DreamShaderCompilerPipeline.h publishes
// exactly what the 1.x generator needs -- a path in, a bool and one error out -- and nothing more,
// because that is the whole of what the 1.x side is allowed to know. Every other caller wants more
// than that: `check` wants to stop before the emit, `dump-ir` wants the IR module, `index` wants the
// bound module, `check --shaders` wants the assets that came out, `dump-graph` wants their paths.
// RunDreamShaderLang2Pipeline is the same driver with its intermediate products handed back instead
// of dropped, and CompileDreamShaderLang2File is a thin wrapper over it. Both are implemented in
// DreamShaderCompilerPipeline.cpp.
//
// PART 2 is the four commandlet verbs -- `check`, `dump-ir`, `index`, `export-catalog` -- and the
// source selection they share. They take the same (Tokens, Switches, Params) triple UCommandlet
// hands its Main and that every existing verb already takes, so the dispatcher stays a dispatcher.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilerIncludes.h"
#include "DreamShaderDefineTable.h"
#include "IR/IR.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"
#include "UObject/WeakObjectPtr.h"

class UObject;

namespace UE::DreamShader::Editor::Compiler
{
	// ---------------------------------------------------------------------------------- part 1

	struct FDreamShaderLang2PipelineOptions
	{
		/** Rebuild even when a product's stamped source hash says its asset is current. */
		bool bForce = false;

		/**
		 * False stops after ValidateDreamShaderIR: everything the front end can say has been said and
		 * nothing was written. That is `check`, and it is the reason the emit is the last frame of
		 * the run rather than interleaved with validation.
		 */
		bool bEmitAssets = true;
	};

	/**
	 * Everything one run produced, owned.
	 *
	 * The member ORDER is load-bearing. FBoundModule holds raw pointers into the parsed module and
	 * into every module the include resolver parsed, and FIRModule is built from it; members are
	 * destroyed in reverse declaration order, so IR dies first, then Bound, then the two things
	 * Bound points into, then the define table the resolver holds by reference. Reordering these
	 * declarations is a use-after-free with no other symptom.
	 */
	struct FDreamShaderLang2PipelineResult
	{
		/** Absolute, normalized. */
		FString SourceFilePath;

		/** Every diagnostic of the whole run, front end and emitter together. */
		UE::DreamShader::Lang::FLangDiagnosticSink Diagnostics;

		/** The build key: the preprocessed text of the file AND of every header, plus the defines read. */
		FString SourceHash;

		/** Resolved `#include` paths in first-seen order. Feeds the dependency graph and the asset's include list. */
		TArray<FString> IncludePaths;

		/** The defines the preprocessor actually read, over the file and every header. Folded into SourceHash. */
		UE::DreamShader::FDreamShaderDefineValueMap TouchedDefines;

		/** True when the file or any header carried a preprocessor directive, taken or not (the Adopt gate). */
		bool bSourceHadPreprocessorDirectives = false;

		/** Product indices in the order they were emitted; parallel to ProductAssets / ProductAssetPaths. */
		TArray<int32> ProductOrder;
		TArray<TWeakObjectPtr<UObject>> ProductAssets;
		TArray<FString> ProductAssetPaths;

		bool bSucceeded = false;
		/** True when the user pressed Cancel. Distinct from a failure: nothing was written either way, but nothing is WRONG. */
		bool bCancelled = false;

		// --- owned state, in destruction-safe order; see the struct comment ---
		TUniquePtr<UE::DreamShader::FDreamShaderDefineTable> Defines;
		TUniquePtr<UE::DreamShader::Lang::FLangSourceText> Source;
		TUniquePtr<UE::DreamShader::Lang::FModule> Module;
		TUniquePtr<FDreamShaderIncludeResolver> Includes;
		TUniquePtr<UE::DreamShader::Lang::FBoundModule> Bound;
		TUniquePtr<UE::DreamShader::IR::FIRModule> IR;
	};

	/**
	 * The whole run: read, preprocess, parse, bind, lower, run the passes, validate, and -- unless
	 * Options says otherwise -- emit every product in dependency order.
	 *
	 * Returns false for any failure, including cancellation; OutResult.Diagnostics always says why.
	 * OutResult keeps whatever it got as far as, so a caller that only wants the AST (a language
	 * service on a file that does not bind) gets it from a false return.
	 *
	 * Game thread only: the emit half creates UObjects.
	 */
	bool RunDreamShaderLang2Pipeline(
		const FString& SourceFilePath,
		const FDreamShaderLang2PipelineOptions& Options,
		FDreamShaderLang2PipelineResult& OutResult);

	// ---------------------------------------------------------------------------------- part 2

	/**
	 * Every `.dss` under every source root, minus the `Packages` trees, sorted.
	 *
	 * A separate scan from FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles, which
	 * enumerates `*.dsm`, `*.dsh` and `*.dsf` only and is not this unit's to change. Until it learns
	 * the extension, `compile -All` does not see a `.dss` at all -- recorded in the unit report as
	 * the one-line change that fixes it for every verb at once.
	 */
	void FindProjectDreamShaderLang2Sources(TArray<FString>& OutSourceFiles);

	/**
	 * The `-Source` / `-File` / `-All` triple for the 2.0 verbs, resolved the way `compile` resolves
	 * it, over `.dss` files.
	 *
	 * Returns false when none of the three was given -- the usage-banner case. An empty list with a
	 * true return is the legitimate "this project has no 2.0 sources yet" answer.
	 */
	bool ResolveDreamShaderLang2CommandletSourceFiles(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		TArray<FString>& OutSourceFiles);

	/** The usage lines for the four 2.0 verbs, appended to the commandlet's own banner. */
	const TCHAR* GetDreamShaderLang2CommandletUsage();

	/**
	 * `check <file|-All> [-Shaders] [-Platform=SM6[,SM5]] [-Quality=High] [-Timeout=120] [-DiagnosticsOut=<file>]`
	 *
	 * Runs the pipeline to IR validation and writes NOTHING -- that is the gate, and it is safe to
	 * run against a tree you do not want touched.
	 *
	 * `-Shaders` is the exception, and it is a real one: a shader compile needs a real material, and
	 * 2.0 has no transient asset to build one into (plan §5), so `check -Shaders` builds and saves
	 * the products exactly as `compile` does before compiling their shaders. It says so in the log
	 * before it starts. See DreamShaderShaderCheck.h and plan §13.3.
	 */
	bool RunDreamShaderCheckCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `dump-ir <file|-All> [-Out=<dir>] [-Json]` -- DumpDreamShaderIRText, and `.json` as well with `-Json`. */
	bool RunDreamShaderDumpIRCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `index <file|-All> [-Out=<dir>]` -- BuildDreamShaderSymbolIndexJson per file (plan §13.4). */
	bool RunDreamShaderIndexCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);

	/** `export-catalog [-Out=<path>]` -- the builtin catalog as JSON, beside the other manifests. */
	bool RunDreamShaderExportCatalogCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);
}
