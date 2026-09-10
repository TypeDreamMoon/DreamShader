// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The recursive-descent parser for DreamShaderLang 2.0, split across three translation units
// that share this one class:
//
//   LangParser.cpp              entry points, the token cursor, recovery, the module loop
//   LangParserDeclarations.cpp  file-scope declarations, types, `///` blocks, `#pragma`, `#include`, raw bodies
//   LangParserStatements.cpp    statements and blocks
//   LangParserExpressions.cpp   expressions (precedence climbing over LangOperators.h)
//
// Conventions every method follows:
//   - A parse method that returns a pointer returns null on failure AFTER having reported the
//     error; it never reports twice for one failure. The caller decides whether to recover.
//   - A parse method that returns bool has already reported on false.
//   - Spans: every node's Span runs from its first token to its last; use SpanFrom(StartIndex).
//   - Diagnostics carry a literal code: `Diagnostics.Error(TEXT("DSH2151"), Span, LOCTEXT(...))`.
//     Codes are allocated per translation unit -- see Plan/m1/CONTRACT.md -- and must not be
//     reused across units.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

namespace UE::DreamShader::Lang::Private
{
	class FLangParser
	{
	public:
		FLangParser(
			const FLangSourceText& InSource,
			TArray<FLangToken>&& InTokens,
			ELangFrontend InFrontend,
			ELangFileKind InFileKind,
			FLangDiagnosticSink& InDiagnostics);

		// ---------------------------------------------------------------- entry (LangParser.cpp)

		/** Parses to EndOfFile. Never null: a module with whatever declarations survived. */
		TUniquePtr<FModule> ParseModule();
		/** Parses one expression and requires EndOfFile after it. */
		FExprPtr ParseStandaloneExpression();

		// ---------------------------------------------------------------- cursor (LangParser.cpp)

		const FLangToken& Peek(int32 Ahead = 0) const;
		const FLangToken& Current() const { return Peek(0); }
		/** The last consumed token; the first token before anything was consumed. */
		const FLangToken& Previous() const;
		bool AtEnd() const;
		/** Consumes and returns the current token. Does not move past EndOfFile. */
		const FLangToken& Advance();

		bool Check(ELangTokenKind Kind) const { return Current().Kind == Kind; }
		bool CheckKeyword(ELangKeyword Keyword) const { return Current().IsKeyword(Keyword); }
		bool CheckIdentifier(const TCHAR* Name) const { return Current().IsIdentifier(Name); }
		/** Consumes the token when it matches. */
		bool Match(ELangTokenKind Kind);
		bool MatchKeyword(ELangKeyword Keyword);
		/**
		 * Consumes Kind, or reports Code with "Expected <What>, found <token>" and returns false
		 * without consuming. What is the human name of the thing expected, e.g. LOCTEXT("...", "';'").
		 */
		bool Expect(ELangTokenKind Kind, const TCHAR* Code, const FText& What);
		bool ExpectIdentifier(FString& OutName, FLangSpan& OutSpan, const TCHAR* Code, const FText& What);

		int32 GetTokenIndex() const { return Index; }
		/** Backtracking for the few two-token lookaheads that need it (cast vs parenthesised expression). */
		void SetTokenIndex(int32 InIndex) { Index = FMath::Clamp(InIndex, 0, Tokens.Num() - 1); }
		/** Span from the token at StartIndex to the end of Previous(). */
		FLangSpan SpanFrom(int32 StartIndex) const;
		/** The source text a span covers. */
		FString Slice(const FLangSpan& Span) const { return Source.Slice(Span); }
		/** "identifier 'foo'", "'{'", "number '1.5'", "end of file" -- for messages. */
		FText DescribeToken(const FLangToken& Token) const;
		/** Reports DSH2150 (unexpected end of file) at the current token and returns false. */
		bool FailAtEnd(const FText& WhileParsing);

		const FLangSourceText& GetSource() const { return Source; }
		FLangDiagnosticSink& GetDiagnostics() { return Diagnostics; }
		ELangFileKind GetFileKind() const { return FileKind; }

		// ---------------------------------------------------------------- recovery (LangParser.cpp)

		/**
		 * After a failed declaration: skip until just past the next `;` at brace depth zero, or
		 * just past the `}` that closes the outermost open brace, whichever comes first. Stops at
		 * EndOfFile. A DocComment or Directive token at line start also ends the skip (it starts the
		 * next declaration) and is NOT consumed.
		 */
		void SkipToDeclarationBoundary();
		/** After a failed statement: skip until just past the next `;` at depth zero, or stop BEFORE the `}` that closes the enclosing block. */
		void SkipToStatementBoundary();
		/** At a `{`: consumes through the matching `}`. False (with DSH2150) when the file ends first. */
		bool SkipBalancedBraces();

		// ------------------------------------------------ declarations (LangParserDeclarations.cpp)

		/** One file-scope declaration; consumes the `///` block that precedes it. Null after reporting. */
		FDeclPtr ParseDeclaration();
		/** Consumes consecutive DocComment tokens into OutDoc. Returns true when any was consumed. */
		bool ParseDocBlock(FDocBlock& OutDoc);
		/** Splits one `///` line into directives (`@key value ... @key value`) and free text. Shared with struct fields. */
		static void ParseDocLine(const FString& Line, const FLangSpan& LineSpan, FDocBlock& InOutDoc);
		/** At a Directive token: `#pragma ...` or `#include ...`; anything else is DSH3201. */
		FDeclPtr ParseDirective(FDocBlock&& Doc);
		/** At `import`: `import "path";`. */
		FDeclPtr ParseImport(FDocBlock&& Doc);
		/** At `struct`. */
		FDeclPtr ParseStructDecl(FDocBlock&& Doc);
		/** At `[uniform|static|const|extern|export]* Type Name ...` -- decides between a function and a variable after the name. */
		FDeclPtr ParseFunctionOrVariableDecl(FDocBlock&& Doc);
		/** After the function name's `(`: parameters through `)`. */
		bool ParseParameterList(TArray<FParam>& OutParams);
		/**
		 * A type spelling: one identifier, classified by ClassifyTypeName. Reports DSH3204 when the
		 * current token is not an identifier. Does NOT consume array dimensions -- they belong to
		 * the declarator (`float a[4]`), never to the type in HLSL.
		 */
		bool ParseType(FTypeRef& OutType);
		/** Zero or more `[expr]` / `[]` after a declarator name. */
		bool ParseArrayDimensions(TArray<FExprPtr>& OutDimensions);
		/**
		 * At a `{`: captures the text between the braces verbatim (string- and comment-aware brace
		 * matching over the TOKENS, so a `}` inside a string or a comment does not end the body) and
		 * consumes through the closing `}`. OutSpan covers both braces.
		 */
		bool CaptureRawBody(FString& OutRaw, FLangSpan& OutSpan);
		/** Fills Category / Scalar / Texture / Rows / Cols from a spelling. Unknown names become Named. */
		static void ClassifyTypeName(const FString& Name, FTypeRef& InOutType);
		static bool IsBuiltinTypeName(const FString& Name);
		/**
		 * True when the tokens at the cursor read as the start of a declaration: an optional storage
		 * keyword, then an identifier (the type) followed by an identifier (the name). `Foo(x);` is a
		 * call, `Foo x;` and `Foo x = ...;` are declarations. Used by the statement parser too.
		 */
		bool LooksLikeDeclarationStart() const;

		// -------------------------------------------------- statements (LangParserStatements.cpp)

		FStmtPtr ParseStatement();
		/** At `{`: a block. */
		TUniquePtr<FBlockStmt> ParseBlock();
		/** At `[static] [const] Type name [dims] [= init] {, name ...} ;` */
		TUniquePtr<FVarDeclStmt> ParseLocalVarDecl();

		// ------------------------------------------------ expressions (LangParserExpressions.cpp)

		/** The full expression grammar: assignment level, right-associative. */
		FExprPtr ParseExpression();
		FExprPtr ParseAssignment();
		FExprPtr ParseConditional();
		/** Precedence climbing over GetBinaryPrecedence; MinPrecedence 1 parses everything binary. */
		FExprPtr ParseBinary(int32 MinPrecedence);
		FExprPtr ParseUnary();
		/** Applies `.member`, `[index]`, `(args)`, `++`, `--` to an already parsed primary. */
		FExprPtr ParsePostfix(FExprPtr Primary);
		FExprPtr ParsePrimary();
		/**
		 * After a `(`: arguments through `)`. `Ident = expr` at the top level of an argument is a
		 * NAMED argument (so `UE.TexCoord(Index = 0)` keeps its 1.x spelling); an assignment as a
		 * positional argument must be written in parentheses. Named arguments may not be followed
		 * by positional ones (DSH2158).
		 */
		bool ParseArguments(TArray<FArgument>& OutArguments);
		/** At `{`: `{ a, b, ... }`. */
		FExprPtr ParseInitializerList();
		/** True when the cursor is at `(` Type `)` -- a cast -- rather than a parenthesised expression. */
		bool IsCastStart() const;

	private:
		const FLangSourceText& Source;
		TArray<FLangToken> Tokens;
		int32 Index = 0;
		ELangFrontend Frontend;
		ELangFileKind FileKind;
		FLangDiagnosticSink& Diagnostics;
	};
}
