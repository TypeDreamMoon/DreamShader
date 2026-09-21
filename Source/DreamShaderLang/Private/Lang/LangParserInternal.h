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
//     Codes are allocated per translation unit and must not be reused across units.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

namespace UE::DreamShader::Lang::Private
{
	/** One 1.x `Properties` entry as the legacy parser collected it, before it became a uniform or was expanded at its uses. */
	struct FLegacyProperty
	{
		FString Name;
		FString NodeType;
		FExprPtr Default;
		FString DefaultText;
		/** Synthesized, canonical keys. */
		TArray<FDocDirective> Metadata;
		/** As written, for FLegacyParameterDeclaration. */
		TArray<TPair<FString, FString>> RawMetadata;
		bool bConst = false;
		bool bExpandAtUse = false;
		FLangSpan Span;
	};

	/** The 1.x block being parsed: its header attributes and what its sections have collected. */
	struct FLegacyBlockContext
	{
		FString BlockWord;
		FString Name;
		FString Root;
		bool bHasRoot = false;
		bool bMaterial = false;
		TArray<FLegacyProperty> Properties;
		/** Null when the caller did not ask for migration information. */
		FLegacyMigrationInfo* Info = nullptr;
	};

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

		// ------------------------------------------------------ legacy top level (LangLegacyParser.cpp)

		/** The legacy module loop, to EndOfFile. Info may be null (no migration information asked for). */
		TUniquePtr<FModule> ParseLegacyModule(FLegacyMigrationInfo* Info);
		/** One legacy top-level construct; it may yield several declarations. */
		bool ParseLegacyTopLevel(TArray<FDeclPtr>& OutDecls);
		/** At a block word (`Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`): the block and its sections. */
		bool ParseLegacyBlock(TArray<FDeclPtr>& OutDecls);
		/** At `(`: the `Key = value, ...` of a block header. */
		bool ParseLegacyAttributes(TArray<FPragmaArgument>& OutAttributes);
		/** At `Function` / `GraphFunction`; NamespaceName is empty outside a `Namespace`. */
		FDeclPtr ParseLegacyFunction(const FString& NamespaceName);
		/**
		 * The `UE.Name(...)` calls of Function.RawBody parsed into Function.HoistedCalls, each one an input of the
		 * function's Custom node (rule L8 for a 1.x GraphFunction; the same lift for a `/// @custom` function).
		 * The text stays in RawBody; the code builder replaces it by the input's name. BodyContentOffset is where
		 * RawBody starts in the file, InputNames the names a lifted input may not take.
		 */
		void LiftCallsOutOfOpaqueBody(FFunctionDecl& Function, const FString& QualifiedName, int32 BodyContentOffset, const TArray<FString>& InputNames);
		/** At `Namespace`. */
		bool ParseLegacyNamespace(TArray<FDeclPtr>& OutDecls);
		/** At `VirtualFunction`. */
		FDeclPtr ParseLegacyVirtualFunction();

		// ------------------------------------------------------ legacy sections (LangLegacySections.cpp)

		bool ParseLegacyProperties(FLegacyBlockContext& Block, TArray<FDeclPtr>& OutDecls);
		bool ParseLegacySettings(FLegacyBlockContext& Block, TArray<FPragmaArgument>& OutSettings);
		bool ParseLegacyOutputs(FLegacyBlockContext& Block, TArray<FStmtPtr>& OutHead, TArray<FStmtPtr>& OutTail);
		bool ParseLegacyParams(FLegacyBlockContext& Block, EParamDirection Direction, TArray<FParam>& OutParams);
		bool ParseLegacyLayout(TArray<FDeclPtr>& OutDecls);

		// -------------------------------------------------- legacy statements (LangLegacyStatements.cpp)

		TUniquePtr<FBlockStmt> ParseLegacyGraphBody(FLegacyBlockContext& Block);
		/** The restriction pass over a parsed legacy body: reports what 1.x silently truncated; never rewrites. */
		void ValidateLegacyBody(const FBlockStmt& Body);

		// ------------------------------------------------ legacy expressions (LangLegacyExpressions.cpp)

		/** The parse-time rewrites of 1.x call spellings onto 2.0 shapes. Block may be null. */
		FExprPtr RewriteLegacyCall(FLegacyBlockContext* Block, TUniquePtr<FCallExpr> Call);
		/** Classifies a 1.x type spelling (case-insensitive; `vec*`, `MaterialAttributes`, `StaticBool`) and writes the 2.0 spelling into Name. */
		static void ClassifyLegacyTypeName(const FString& Spelling, FTypeRef& InOutType);

		// ------------------------------------------------ FE additions: trivia (LangParser.cpp)

		/** Set by ParseDreamShaderLang when the parse keeps trivia; the skips below then record what they drop. */
		void SetKeepTrivia(bool bInKeepTrivia) { bKeepTrivia = bInKeepTrivia; }
		bool KeepsTrivia() const { return bKeepTrivia; }
		/** A `///` token the grammar skipped (inside a body, or lexed where 1.x reads it as a comment): kept as a comment. */
		void RecordSkippedDocComment(const FLangToken& Token);
		/** A `///` block that was parsed and then discarded (DSH3221): one comment per line. */
		void RecordSkippedDocBlock(const FDocBlock& Doc);
		/** Everything recorded so far, in recording order; empties the list. */
		TArray<FLangComment> TakeSkippedComments() { return MoveTemp(SkippedComments); }

		// ------------------------------------------ FE additions: legacy state (LangLegacyParser.cpp)

		/** The migration record legacy constructs report into; null when none was asked for. */
		void SetLegacyInfo(FLegacyMigrationInfo* InInfo) { LegacyInfo = InInfo; }
		FLegacyMigrationInfo* GetLegacyInfo() const { return LegacyInfo; }
		/** True while parsing 1.x text: a legacy module, or a legacy declaration inside a `.dsh`. */
		bool IsLegacyMode() const { return Frontend == ELangFrontend::Legacy || LegacyScopeDepth > 0; }
		/** At a `.dsh` legacy word (`Function`, `GraphFunction`, `Namespace`, `VirtualFunction`, a block word): one legacy construct. Doc is the `///` block above it. */
		bool ParseLegacyDeclarationInHeader(FDocBlock&& Doc, TArray<FDeclPtr>& OutDecls);
		/** After a failed legacy top-level construct: skip to the next line that starts a top-level word, or past the `}` closing the construct. */
		void SkipToLegacyTopLevelBoundary();
		/** A `.dsm`/`.dsf`/`.dsh` top-level word the legacy front end owns (exact case). */
		static bool IsLegacyTopLevelWord(const FString& Text);
		/** `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `MaterialLayer`, `MaterialLayerBlend` (exact case). */
		static bool IsLegacyAssetBlockWord(const FString& Text);

		// ------------------------------------ FE additions: legacy expressions (LangLegacyExpressions.cpp)

		/** Legacy classification of a type spelling; true for every builtin the 1.x type tokens name. */
		static bool IsLegacyBuiltinTypeName(const FString& Name);
		/** In legacy mode, at `Ident :: Ident ...`: the flattened `N_F` identifier (1.x SanitizeIdentifier). */
		FExprPtr ParseLegacyQualifiedName();
		/** In legacy mode, a bare read of a property expanded at its uses (TextureSampleParameter2D, ChannelMaskParameter, ...); null when Name is none. */
		FExprPtr TryExpandLegacyPropertyRead(const FString& Name, const FLangSpan& Span);
		/** Records one rename into the migration info (no-op without one). */
		void RecordLegacyRename(FLegacyRename::EKind Kind, const FString& From, const FString& To, const FLangSpan& Span);
		/** The 1.x typed zero for a declaration without an initializer; null for a type 1.x refused to zero (texture, Substrate, sampler, named). */
		static FExprPtr MakeLegacyZeroInitializer(const FTypeRef& Type, const FLangSpan& Span);

		// -------------------------------------- FE additions: legacy statements (LangLegacyStatements.cpp)

		/** In legacy mode, at a Directive token inside a Graph body: `#Region "Name"` / `#EndRegion` as an FPragmaStmt. */
		FStmtPtr ParseLegacyRegionDirective();
		/**
		 * The inputs of the block being read whose 1.x type was `StaticBool`: the type alias table reads the token as
		 * `bool`, and the block's function says `/// @static <Name>` for each, which is what keeps the pin a StaticBool one.
		 * Filled by ParseLegacyParamsWithDocs, taken by the block once its function exists.
		 */
		TArray<FString> LegacyStaticBoolInputs;

		/** Typed zero initializers for every declarator of Body that has none (1.x semantics), recorded as synthesized. */
		void SynthesizeLegacyInitializers(FBlockStmt& Body);
		/**
		 * 1.x read `T x = {a, b, c};` as `T x = T(a, b, c);` and `T x = {};` as the zero of T (EvaluateBraceInitializer).
		 * Rewritten before the restriction pass, which would otherwise refuse the list (DSH2214): a braced ASSIGNMENT
		 * still is, because the type it constructs is the target's and the parser does not know it.
		 */
		void RewriteLegacyBraceInitializers(FBlockStmt& Body);
		/** The restriction pass over one expression in value position (an Outputs binding source, a section default). */
		void ValidateLegacyValue(const FExpr& Expr);

		// ---------------------------------------- FE additions: legacy sections (LangLegacySections.cpp)

		/**
		 * Parses tokens [First, End) of this parser as one legacy expression, all of them consumed (DSH3211 otherwise):
		 * a 1.x value sits inside section syntax (`float X = 1.0 [Group = "G"];`) that the expression grammar would
		 * misread past its end. Same source, same sink, same migration record and block context; the cursor of
		 * this parser is not moved. Null after reporting.
		 */
		FExprPtr ParseLegacyExpressionRange(int32 First, int32 End);
		/** ParseLegacyParams, plus the names in 1.x declaration order and each `[Description = ...]` as (name, text). */
		bool ParseLegacyParamsWithDocs(FLegacyBlockContext& Block, EParamDirection Direction, TArray<FParam>& OutParams, TArray<FString>& OutDeclaredNames, TArray<TPair<FString, FString>>& OutDocs);

	private:
		const FLangSourceText& Source;
		TArray<FLangToken> Tokens;
		int32 Index = 0;
		ELangFrontend Frontend;
		ELangFileKind FileKind;
		FLangDiagnosticSink& Diagnostics;

		// FE additions: trivia.
		bool bKeepTrivia = false;
		TArray<FLangComment> SkippedComments;

		// FE additions: legacy state.
		FLegacyMigrationInfo* LegacyInfo = nullptr;
		/** Above zero while a `.dsh` legacy declaration is being parsed by the 2.0 module loop. */
		int32 LegacyScopeDepth = 0;
		/** The block whose sections are being parsed; property calls and reads expand against it. Null outside a block. */
		FLegacyBlockContext* LegacyBlock = nullptr;
		/** The declaration renames are recorded against (a Function / GraphFunction being built); may be null. */
		const FDecl* LegacyRenameDecl = nullptr;
		/** Index expressions the rewrites synthesized (`[k]` selections): the restriction pass accepts these. */
		TSet<const FExpr*> LegacySynthesizedIndexExprs;
		/** Callee-and-argument keys of output-selecting calls, for FLegacyOutputSelection::Group (case-sensitive compare). */
		TArray<FString> LegacySelectionGroupKeys;
		/** Every entry, function and extern name the legacy front end produced in this module (collision suffixes). */
		TArray<FString> LegacyUsedNames;
		/** One `Shader` block per file (1.x DSH3030). */
		bool bLegacySawShaderBlock = false;
		/** Set when any legacy construct parsed (a legacy module with none is an error). */
		bool bLegacySawConstruct = false;
		/** The `///` block the 2.0 module loop read above a `.dsh` legacy declaration; the declaration takes it. */
		FDocBlock LegacyPendingDoc;
	};

	/** Shared by the four legacy translation units. Names are specific so the unity blob cannot collide with them. */
	namespace LegacyAst
	{
		FExprPtr MakeIdentifier(const FString& Name, const FLangSpan& Span);
		FExprPtr MakeStringLiteral(const FString& Value, const FLangSpan& Span);
		FExprPtr MakeIntLiteral(int64 Value, const FLangSpan& Span);
		FExprPtr MakeFloatLiteral(const FString& Lexeme, double Value, const FLangSpan& Span);
		FExprPtr MakeBoolLiteral(bool bValue, const FLangSpan& Span);
		/** `UE.<Name>` as a callee. */
		FExprPtr MakeReflectedCallee(const FString& Namespace, const FString& Name, const FLangSpan& Span);
		FArgument MakeNamedArgument(const FString& Name, FExprPtr Value, const FLangSpan& Span);
		/** `UE.Expression(Class = "<ClassName>")`, arguments to be appended. */
		TUniquePtr<FCallExpr> MakeExpressionCall(const FString& ClassName, const FLangSpan& Span);
		/** `Object.Member`. */
		FExprPtr MakeMember(FExprPtr Object, const FString& Member, const FLangSpan& Span);
		/** 1.x `SanitizeIdentifier`: every non-`[A-Za-z0-9_]` to `_`, runs of `_` collapsed, a digit start prefixed. */
		FString SanitizeIdentifier(const FString& Text);
		/** The literal a 1.x metadata or setting value spelled, from its text as written: `"..."` a string, a number, `true`/`false` a bool, a word an identifier, anything else the raw text as a string. */
		FExprPtr MakeValueExpressionFromText(const FString& WrittenText, const FLangSpan& Span);
		/** `Text` without surrounding quotes, 1.x escapes resolved (`\n \r \t \" \\`); unquoted text is only trimmed. */
		FString Unquote(const FString& Text);
		/** A canonical 2.0 vector literal for a 1.x vector default (1 value v,v,v,1; 2 values a,b,0,0; 3 values alpha 1); false when a part is not a number or bool. */
		bool TryNormalizeVectorLiteral(const FString& WrittenText, double OutValues[4]);
		/** `float4(r, g, b, a)` from four numbers, printed with the shortest round-trip text. */
		FExprPtr MakeFloat4Constructor(const double Values[4], const FLangSpan& Span);
	}
}
