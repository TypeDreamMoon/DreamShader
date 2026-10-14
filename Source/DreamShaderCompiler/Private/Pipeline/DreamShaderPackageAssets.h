// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// A package's asset header: a `.dsh` in `DShader/Packages/<scope>/<name>/` whose functions ask to be generated as
// material functions -- `/// @generate MF_X` on a function, `#pragma generate(Functions = "MF_{Name}")` above several, or
// the 1.x `Function(Generate = ...)` / `Namespace(..., Generate = ...)`, which the legacy front end turns into the same
// `@generate` directives.
//
// The header is compiled as a `.dss` the compiler writes for it: an `#include` of the header and one `export` wrapper per
// function that calls it, named into the package's asset root with `@name`. Everything after that -- the bind, the IR, the
// emitter, the build key, divergence, the cook -- is the ordinary compile of an exported function. The assets name the
// header as their source, so Revert to Source and every rebuild come back here. See Docs/tools/packages.md.

#pragma once

#include "CoreMinimal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader
{
	class FDreamShaderDefineTable;
}

namespace UE::DreamShader::Editor::Compiler
{
	/** One material function an asset header asks for. */
	struct FDreamShaderPackageGenerateRequest
	{
		const UE::DreamShader::Lang::FFunctionDecl* Function = nullptr;
		/** `MF_BL_TexNoise3D` or `Noise/MF_BL_TexNoise3D`, relative to the package's asset root. */
		FString AssetName;
		FString Library;
		FString Description;
		UE::DreamShader::Lang::FLangSpan Span;
	};

	/**
	 * What a parsed header asks to generate, in declaration order: a function's own `@generate` (`none` opts out of a
	 * template), else the template of the last `#pragma generate(Functions = ...)` above it. Errors: a template placeholder
	 * other than `{Name}` (DSH8273), a function that cannot become a material function (DSH8274), two requests for one asset
	 * (DSH8275); an unknown `#pragma generate` key is a warning (DSH8276).
	 */
	void CollectDreamShaderPackageGenerateRequests(
		const UE::DreamShader::Lang::FModule& Header,
		TArray<FDreamShaderPackageGenerateRequest>& OutRequests,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/** The `.dss` the header is compiled as: an `#include` of the header, then one `export` wrapper per request named under AssetRoot. */
	FString BuildDreamShaderPackageWrapperSource(
		const FString& HeaderPath,
		const FString& AssetRoot,
		const TArray<FDreamShaderPackageGenerateRequest>& Requests);

	/**
	 * The whole header half of the compile: preprocess and parse the header's raw text, collect its requests and write the
	 * wrapper `.dss`. False with the reasons in Diagnostics when the header does not parse or a request is in error.
	 */
	bool BuildDreamShaderPackageAssetUnit(
		const FString& HeaderPath,
		const FString& RawHeaderText,
		const UE::DreamShader::FDreamShaderDefineTable& Defines,
		FString& OutUnitText,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/** The path the wrapper `.dss` is parsed under: beside the header, so its `#include` resolves, and never on disk. */
	FString GetDreamShaderPackageAssetUnitPath(const FString& HeaderPath);
}
