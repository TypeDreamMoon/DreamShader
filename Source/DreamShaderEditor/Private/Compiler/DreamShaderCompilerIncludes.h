// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The `#include` resolver the 2.0 binder is handed (FBindOptions::IncludeResolver).
//
// The binder knows nothing about files. It asks for a path and gets back a parsed FModule, or null
// and a diagnostic. Everything between those two -- finding the file under the source roots,
// running the conditional-compilation preprocessor over it, parsing it, and remembering both the
// result and what it cost -- is here.
//
// Three things this owns that the binder must not:
//
//   * LIFETIME. FBoundModule keeps raw pointers into every included FModule (FBoundModule::Included,
//     and every FBoundExpr keyed on a node address inside one). So the resolver owns the modules and
//     must outlive the bound module, not just the bind call. It is a stack local of the pipeline for
//     exactly the span in which the bound module is alive.
//   * FILE-LOCAL `#define`. Each header is preprocessed ON ITS OWN, against the compile's define
//     table, and its `#define`s do not escape into the file that included it. That is the 1.9 rule
//     (DreamShaderPreprocessor.h, and the comment at DreamShaderMaterialGeneratorSourceLoading.cpp
//     ~164): the preprocessor runs before anything is inlined, so cross-file macro state never
//     exists to leak. 2.0 keeps it, which is why this resolver preprocesses per file rather than
//     carrying a table forward.
//   * THE BUILD KEY. Every define any header read, and every path that resolved, accumulate here
//     and are read back by the pipeline: the defines fold into BuildSourceHash, the paths into the
//     dependency registration and the asset's include list. A header whose `#if` the compile read
//     but whose value never reached the hash is a silently stale asset.
//
// Import cycles are NOT detected here. The binder owns that (DSH4211): this resolver is asked once
// per distinct path and answers from its cache thereafter, so a cycle reaches it as a repeat
// lookup, never as recursion.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderDefineTable.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Resolves, preprocesses, parses and caches the headers one compile includes.
	 *
	 * One per compile. Not copyable, not movable: the modules it owns are pointed at from a bound
	 * module that outlives neither.
	 */
	class FDreamShaderIncludeResolver
	{
	public:
		/**
		 * @param InDefines  The table this compile resolved ONCE, by reference. Resolving per header
		 *                   would let a provider delegate answer differently between two files of one
		 *                   compile, and nothing downstream could tell -- the same reason
		 *                   LoadPreparedDreamShaderSource resolves once for its whole recursion.
		 *                   It must outlive this resolver.
		 */
		explicit FDreamShaderIncludeResolver(const UE::DreamShader::FDreamShaderDefineTable& InDefines);

		FDreamShaderIncludeResolver(const FDreamShaderIncludeResolver&) = delete;
		FDreamShaderIncludeResolver& operator=(const FDreamShaderIncludeResolver&) = delete;

		/**
		 * The callable for FBindOptions::IncludeResolver.
		 *
		 * Captures `this`. The returned TFunction must not outlive the resolver, which in the
		 * pipeline it cannot: both are locals of one compile.
		 */
		UE::DreamShader::Lang::FLangIncludeResolver MakeBinderResolver();

		/** Every path that resolved, in first-seen order, absolute and normalized. */
		const TArray<FString>& GetResolvedIncludePaths() const { return ResolvedIncludePaths; }

		/** The union, over every header, of the defines its preprocessing read. Build-key material. */
		const UE::DreamShader::FDreamShaderDefineValueMap& GetTouchedDefines() const { return TouchedDefines; }

		/** True when any header carried a preprocessor directive, taken or not (the Adopt gate). */
		bool AnyIncludeHadDirectives() const { return bAnyIncludeHadDirectives; }

		/**
		 * The concatenated preprocessed text of every header, each wrapped in the 1.x
		 * `// Begin/End DreamShader source:` markers, in first-seen order.
		 *
		 * Not used for parsing -- each header is parsed on its own, which is the whole point of this
		 * class. It exists so the source hash covers the headers' CONTENT and not merely their paths:
		 * editing a `.dsh` must invalidate every asset built from a `.dss` that includes it, and the
		 * `.dss`'s own text does not change when the header does.
		 */
		const FString& GetIncludedSourceDigestText() const { return IncludedSourceDigestText; }

	private:
		const UE::DreamShader::Lang::FModule* Resolve(
			const FString& IncludePath,
			const FString& FromFile,
			UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

		const UE::DreamShader::FDreamShaderDefineTable& Defines;

		/** Resolved absolute path -> the parsed module. Owns them; see the class comment on lifetime. */
		TMap<FString, TUniquePtr<UE::DreamShader::Lang::FModule>> ModulesByPath;

		/**
		 * The source texts the modules were parsed from, kept alive alongside them.
		 *
		 * The AST carries spans as values, so nothing in it dereferences these -- but a span is only
		 * useful against the text it indexes, and the language service, the printer and any future
		 * quoted diagnostic all want to slice it. Holding them costs one copy of each header per
		 * compile and removes a whole class of "the offsets point at nothing" bug.
		 */
		TArray<TUniquePtr<UE::DreamShader::Lang::FLangSourceText>> SourceTexts;

		/** Paths already reported as unresolvable/unreadable/unparsable; reported once, not per include. */
		TSet<FString> FailedPaths;

		TArray<FString> ResolvedIncludePaths;
		UE::DreamShader::FDreamShaderDefineValueMap TouchedDefines;
		FString IncludedSourceDigestText;
		bool bAnyIncludeHadDirectives = false;
	};

	/**
	 * Resolves one include specifier to an absolute file path. Public because `check` and the
	 * language-service tools want the answer without binding anything.
	 *
	 * The rules, in order:
	 *   1. An absolute path that exists is itself.
	 *   2. A specifier starting with `/` is ROOT-ANCHORED: it is never relative to the including
	 *      file. `/Game/` at the front is dropped -- that prefix is the engine's content-root
	 *      spelling, which a `.dsh` under `DShader/` has no counterpart for, and the 2.0 syntax
	 *      examples write it (CONTRACT §6.11). The remainder is tried against the including file's
	 *      own root first, then every other root, `Directory` before `PackagesDirectory`.
	 *   3. Anything else goes through the 1.x rules --
	 *      FDreamShaderDependencyGraphService::ResolveImportPath -- which cover the root qualifier
	 *      (`Plugin.MoonToon:Shared/Common.dsh`), the including file's own directory, its root, and
	 *      its Packages tree, and which refuse to leave the owning root without a qualifier.
	 *
	 * A specifier with no extension gains `.dsh`, exactly as an `import` does.
	 */
	bool ResolveDreamShaderIncludePath(
		const FString& IncludeSpecifier,
		const FString& FromFile,
		FString& OutResolvedPath,
		FString& OutError);
}
