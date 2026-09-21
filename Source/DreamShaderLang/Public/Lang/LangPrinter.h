// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"

namespace UE::DreamShader::Lang
{
	struct FLangPrintOptions
	{
		/** One indentation level. Four spaces. */
		FString Indent = TEXT("    ");
		/** Line terminator. */
		FString NewLine = TEXT("\n");
		/** One blank line between top-level declarations. */
		bool bBlankLineBetweenDeclarations = true;
		/** Print FModule::Trivia and FModule::TrailingComments. */
		bool bPrintTrivia = true;
	};

	/**
	 * Prints a module back as `.dss` text.
	 *
	 * The contract is structural fidelity, not textual: parse(print(parse(X))) yields the same tree
	 * as parse(X). Parentheses are emitted where FParenExpr recorded them and additionally wherever
	 * precedence requires; `///` blocks are re-emitted one directive per line after the free text;
	 * an opaque body is written verbatim from RawBody; pragmas and includes keep their spelling.
	 * Comments and blank lines are reproduced from FModule::Trivia when the parse kept them and Options.bPrintTrivia is
	 * set (at most one blank line in a row); a module without trivia gets the canonical layout.
	 */
	DREAMSHADERLANG_API FString PrintDreamShaderLang(const FModule& Module, const FLangPrintOptions& Options = FLangPrintOptions());

	DREAMSHADERLANG_API FString PrintDreamShaderLangDecl(const FDecl& Decl, const FLangPrintOptions& Options = FLangPrintOptions());
	DREAMSHADERLANG_API FString PrintDreamShaderLangStmt(const FStmt& Stmt, const FLangPrintOptions& Options = FLangPrintOptions(), int32 IndentLevel = 0);
	DREAMSHADERLANG_API FString PrintDreamShaderLangExpr(const FExpr& Expr);
	DREAMSHADERLANG_API FString PrintDreamShaderLangType(const FTypeRef& Type);
}
