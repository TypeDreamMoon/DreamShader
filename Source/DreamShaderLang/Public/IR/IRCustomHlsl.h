// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The HLSL a `/// @custom` function becomes: the `Code` string of one UMaterialExpressionCustom.
//
// A `@custom` body is never parsed as DreamShaderLang -- the parser captured it verbatim into
// FFunctionDecl::RawBody -- so nothing upstream of here has looked inside it. This is the one
// place that does, and it does it as text: it hoists the leading `#include`s onto the node,
// expands the `Tex.Sample(UV)` sampler sugar, embeds the other `@custom` functions the body calls
// as members of a local struct (HLSL has no nested free functions, and a Custom node's code IS a
// function body), rewrites those call sites, and makes sure the result ends in a `return`.
//
// It is pure text transformation over FBoundModule: no engine, no UObject, no file system. What
// the emitter needs beyond the string -- the include list and which functions got swallowed --
// comes back in FCustomNodeCode.
//
// This is the 2.0 home of the rules that lived in the 1.x
// `MaterialAssetGeneration/DreamShaderHlslFunctionCodegen.cpp`. The shapes that survive verbatim:
// the `DreamShaderFn_` symbol prefix, the `<Param>Sampler` pairing, the `generated_wrapper_*`
// struct, `EnsureTopLevelReturn`, and leading-`#include` hoisting. What changed and why is in
// `Plan/m2m3/H-report.md`.

#pragma once

#include "CoreMinimal.h"

#include "Lang/LangDiagnostic.h"
#include "Semantic/LangBound.h"

namespace UE::DreamShader::IR
{
	/**
	 * Everything one `Custom` node needs that is not in the function's signature.
	 *
	 * I2 owns the rest of the node: the input pins (the function's `in` parameters, by name -- a
	 * texture parameter is ONE pin carrying the texture object; the engine synthesises the
	 * companion `SamplerState <Pin>Sampler` itself), `Prop::OutputType` from the return type, and
	 * `Prop::AdditionalOutputs` from the `out` parameters in declaration order.
	 */
	struct FCustomNodeCode
	{
		/** `Prop::Code`. Never empty when the call returned true. LF line endings. */
		FString Code;

		/**
		 * `Prop::IncludeFilePaths`, in first-seen order, deduplicated: the `#include "..."` lines
		 * hoisted out of the top of the emitted bodies -- the embedded helpers' first, in the
		 * order they are embedded, then this function's own.
		 */
		TArray<FString> IncludeFilePaths;

		/**
		 * Indices into `FBoundModule::Functions` of the OTHER `@custom` functions whose bodies were
		 * embedded into Code, dependency-first. Never contains FunctionIndex itself. I2 uses it to
		 * know which functions this node already accounts for; P uses it for the dependency graph.
		 */
		TArray<int32> InlinedFunctions;
	};

	/**
	 * The comment markers `Code` carries so a shader-compile error can be mapped back to a source
	 * line (plan §13.3). One Begin/End pair wraps each verbatim body -- the embedded helpers' and
	 * this function's -- so a body that came from an `#include`d header maps to that header.
	 *
	 * The pair is spelled exactly as the 1.x prepared-source markers
	 * (`DreamShaderMaterialGeneratorSourceLoading.cpp`), so a `StartsWith` scanner written for
	 * those reads the file path unchanged. The line a block starts at cannot be carried by that
	 * form -- 1.x always started at line 1 -- so it rides on a second line:
	 *
	 *     // Begin DreamShader source: D:/Proj/DShader/M_X.dss
	 *     // DreamShader custom: GlowMask line 42
	 *     <the body, verbatim, one emitted line per source line, no added indent>
	 *     // End DreamShader source: D:/Proj/DShader/M_X.dss
	 *
	 * Reading rule: on Begin, take the file; on the `custom` line, set the current source line to
	 * N; every following line that is not a marker maps to the current line and then increments it;
	 * on End, the block is over. Columns map 1:1 except on the first line of a body that shares its
	 * line with the opening brace.
	 */
	namespace CustomCodeMarker
	{
		inline const TCHAR* const BeginPrefix = TEXT("// Begin DreamShader source: ");
		inline const TCHAR* const EndPrefix = TEXT("// End DreamShader source: ");
		inline const TCHAR* const BodyPrefix = TEXT("// DreamShader custom: ");
		inline const TCHAR* const BodyLineInfix = TEXT(" line ");
	}

	/**
	 * Reads one `// DreamShader custom: <Name> line <N>` marker. Returns false for any other line,
	 * leaving the outputs untouched. Spelled here so the writer and every reader share one grammar.
	 */
	DREAMSHADERLANG_API bool TryParseCustomCodeBodyMarker(const FString& Line, FString& OutFunctionName, int32& OutSourceLine);

	/**
	 * Builds the Custom-node code for `Bound.Functions[FunctionIndex]`, which must be a
	 * `EBoundFunctionKind::Custom` function with a verbatim body.
	 *
	 * Returns false after reporting when the function cannot become a Custom node at all. Like the
	 * rest of the front end it keeps going after a recoverable problem, so a false return may carry
	 * several diagnostics, and `Out` is still filled with the best code it could build (useful for
	 * `dump-ir`, never for an asset).
	 *
	 * I2 calls this once per Custom node it makes. Two calls for the same function produce the same
	 * string, so the dedupe key (§6.7) over `Prop::Code` behaves.
	 */
	DREAMSHADERLANG_API bool BuildDreamShaderCustomNodeCode(
		const Lang::FBoundModule& Bound,
		int32 FunctionIndex,
		FCustomNodeCode& Out,
		Lang::FLangDiagnosticSink& Diagnostics);
}
