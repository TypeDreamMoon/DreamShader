// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `dsc migrate`, the engine-free half: a module the legacy (1.x) front end parsed, rewritten in place into one
// the 2.0 front end reads the same way, so that printing it gives the `.dss`.
//
// The legacy front end already lowers every 1.x construct onto 2.0 node kinds; what is left is what only the
// legacy RULES make work, and a `.dss` is bound without them:
//
//   import "x.dsh";                       -> #include "x.dsh"
//   Backend = "" / Backend = Instance     -> Graph / ThinCustom
//   /// @root, /// @name                  -> gone, or the `/// @name` the host asks for
//   mix / fract / mod                     -> lerp / frac / fmod                                      (rule L2)
//   F(a, b).Out, F(a, b)[k]               -> one call statement with a local per output            (rule L3b)
//   x = F(a, b) with F's outs left out    -> the outs passed to locals nobody reads                (rule L3b)
//   x = UE.Node() with several outputs    -> x = UE.Node().FirstOutput                             (rule L3c)
//   a value wider than the place it goes  -> its leading swizzle, `.rgb`                           (rule L22)
//   UE.Node(...)[k]                       -> UE.Node(...).OutputName, where the name is a word
//   UE.Expression(Class = "X", ...)       -> UE.X(...), where the language names the class
//   F(a, b, R, O) with R the return value -> R = F(a, b, O)                                        (rule L5)
//   F(a, default, c)                      -> F(a, C = c), the input left out                       (rule L25)
//   an out target nobody declared         -> declared in front of the call                         (rule L5)
//   x = value with x declared nowhere     -> T x = value                                           (rule L26)
//   SamplerType = SAMPLERTYPE_Normal      -> the catalog's spelling                                (rule L12)
//   a name in the wrong case              -> the declaration's spelling                            (rule L19)
//   a block named like a callable function -> the block's function renamed, `X_Asset`              (rule L23)
//   a default the node class cannot hold  -> dropped, as 1.x dropped it                            (rule L21)
//   `opt` input without a default         -> the zero of its type as the default
//   the comment above a 1.x block         -> above the first declaration the block became
//   a `UE.` call in a GraphFunction body  -> the same call in the 2.0 spelling, in its place         (rule L8)
//   a function of a Namespace block       -> `/// @name N::F` (its node's title), body re-indented
//   Description = "a\r\nb"                -> the doc block's free text, line for line
//   T x = UE.Expression(.., OutputType=T) -> x declared with the type the node really makes
//
// The bound module says what each 1.x spelling resolved to. Nothing here decides whether the result means the same:
// the host builds the printed text with the 2.0 front end and compares (Editor: Commandlet/DreamShaderMigrate.cpp).
//
// Design: Plan/m4m5/research-decompiler.md section 5. Diagnostics: DSH9091, DSH9094.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

namespace UE::DreamShader::Lang
{
	/** What one product declaration is to be called, decided by the host: it knows where a `.dss` puts its assets. */
	struct FLangMigrateProductName
	{
		/** The FLegacyBlock's declaration. */
		const FDecl* Decl = nullptr;
		/** `/// @name` with this; empty writes none, because the file's own place already names the asset. */
		FString Name;
	};

	struct FLangMigrateOptions
	{
		/** One per product block the host has an answer for; a block without one keeps the `/// @name` it has. */
		TArray<FLangMigrateProductName> ProductNames;
	};

	/**
	 * Rewrites Module in place. Bound was bound against Module and is only read; its node pointers are stale once this
	 * returns, so it must not be used afterwards. False after an error (DSH9091): the module is then not worth printing.
	 */
	DREAMSHADERLANG_API bool MigrateDreamShaderLegacyModule(
		FModule& Module,
		const FLegacyMigrationInfo& Legacy,
		const FBoundModule& Bound,
		const FLangMigrateOptions& Options,
		FLangDiagnosticSink& Diagnostics);

	/**
	 * Every comment of a text, trimmed, in order: `//` and block comments, and with bIncludeDocComments the `///` lines
	 * too. The host counts them before and after a migration; a comment the source had and the result does not have
	 * means the file is not written (DSH9092).
	 */
	DREAMSHADERLANG_API void CollectDreamShaderComments(const FLangSourceText& Source, bool bIncludeDocComments, TArray<FString>& OutComments);
}
