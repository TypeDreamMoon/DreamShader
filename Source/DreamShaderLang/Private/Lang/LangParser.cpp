// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Front-end selection, the token cursor, error recovery and the module loop.
//
// The two public entry points both do the same three things -- build the token stream, hand it to
// one FLangParser, hand back whatever survived -- and differ only in what they ask the parser for.
// Everything below the entry points is the machinery the three parser translation units share:
// a clamped cursor that can never run off the end (the last token is always EndOfFile, and
// Advance() refuses to move past it, so a parse method may Peek without bounds-checking), the
// Expect/Describe pair every "expected X, found Y" message goes through, and the two skip
// routines that let one broken declaration cost exactly one declaration.

#include "Lang/LangParser.h"

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLexer.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.Parser"

namespace UE::DreamShader::Lang::Private
{
	/**
	 * Defined in LangParserDeclarations.cpp.
	 *
	 * `uniform float a, b;` is one FVariableDecl per name, but ParseDeclaration() can only hand
	 * back one node, so ParseFunctionOrVariableDecl() stops at the `,` and leaves the rest of the
	 * list to the module loop, which is the only place that can append several declarations at
	 * once. Consumes through the terminating `;`.
	 */
	bool ParseTrailingVariableDeclarators(FLangParser& Parser, const FVariableDecl& First, TArray<FDeclPtr>& OutDecls);

	namespace
	{
		/** The leading word of a directive payload, so a message says `#pragma` and not the whole line. */
		FString FirstWordOf(const FString& Text)
		{
			int32 End = 0;
			while (End < Text.Len() && !FChar::IsWhitespace(Text[End]))
			{
				++End;
			}
			return Text.Left(End);
		}

		/**
		 * A `.dsh` is a shared header: it is included, never compiled into an asset, so an `export`
		 * in one has nowhere to put what it would produce. The declaration is kept -- a language
		 * service still wants the symbol -- but the file cannot succeed.
		 */
		void ReportExportInHeader(FLangDiagnosticSink& Diagnostics, ELangFileKind FileKind, const FDecl& Decl)
		{
			if (FileKind != ELangFileKind::Dsh)
			{
				return;
			}

			const FFunctionDecl* Function = Decl.As<FFunctionDecl>();
			if (!Function || Function->Linkage != EFunctionLinkage::Export)
			{
				return;
			}

			Diagnostics.Error(
				TEXT("DSH3210"),
				Function->NameSpan,
				FText::Format(
					LOCTEXT("ExportInHeader", "A '.dsh' header cannot export '{0}'; only a '.dss' file produces assets."),
					FText::FromString(Function->Name)));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Construction
	// ---------------------------------------------------------------------------------------------

	FLangParser::FLangParser(
		const FLangSourceText& InSource,
		TArray<FLangToken>&& InTokens,
		ELangFrontend InFrontend,
		ELangFileKind InFileKind,
		FLangDiagnosticSink& InDiagnostics)
		: Source(InSource)
		, Tokens(MoveTemp(InTokens))
		, Frontend(InFrontend)
		, FileKind(InFileKind)
		, Diagnostics(InDiagnostics)
	{
		// The lexer always terminates with EndOfFile; a caller that built the array by hand might
		// not. Every cursor operation below leans on "the last token is EndOfFile", so make it true
		// here once instead of testing for it in Peek().
		if (Tokens.Num() == 0 || Tokens.Last().Kind != ELangTokenKind::EndOfFile)
		{
			FLangToken EndToken;
			EndToken.Kind = ELangTokenKind::EndOfFile;
			EndToken.Span = Source.MakeSpan(Source.Len(), 0);
			EndToken.bAtLineStart = true;
			Tokens.Add(MoveTemp(EndToken));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Cursor
	// ---------------------------------------------------------------------------------------------

	const FLangToken& FLangParser::Peek(int32 Ahead) const
	{
		const int32 Last = Tokens.Num() - 1;
		const int32 Target = (Ahead >= Last - Index) ? Last : FMath::Max(0, Index + Ahead);
		return Tokens[Target];
	}

	const FLangToken& FLangParser::Previous() const
	{
		return Tokens[FMath::Clamp(Index - 1, 0, Tokens.Num() - 1)];
	}

	bool FLangParser::AtEnd() const
	{
		return Current().Kind == ELangTokenKind::EndOfFile;
	}

	const FLangToken& FLangParser::Advance()
	{
		const int32 Consumed = Index;
		if (Index < Tokens.Num() - 1)
		{
			++Index;
		}
		return Tokens[Consumed];
	}

	bool FLangParser::Match(ELangTokenKind Kind)
	{
		if (!Check(Kind))
		{
			return false;
		}
		Advance();
		return true;
	}

	bool FLangParser::MatchKeyword(ELangKeyword Keyword)
	{
		if (!CheckKeyword(Keyword))
		{
			return false;
		}
		Advance();
		return true;
	}

	bool FLangParser::Expect(ELangTokenKind Kind, const TCHAR* Code, const FText& What)
	{
		if (Check(Kind))
		{
			Advance();
			return true;
		}

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("What"), What);
		Arguments.Add(TEXT("Token"), DescribeToken(Current()));
		return Diagnostics.Error(
			Code,
			Current().Span,
			FText::Format(LOCTEXT("ExpectedFound", "Expected {What}, found {Token}."), Arguments));
	}

	bool FLangParser::ExpectIdentifier(FString& OutName, FLangSpan& OutSpan, const TCHAR* Code, const FText& What)
	{
		if (Check(ELangTokenKind::Identifier))
		{
			const FLangToken& Token = Advance();
			OutName = Token.Text;
			OutSpan = Token.Span;
			return true;
		}

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("What"), What);
		Arguments.Add(TEXT("Token"), DescribeToken(Current()));
		return Diagnostics.Error(
			Code,
			Current().Span,
			FText::Format(LOCTEXT("ExpectedFoundIdentifier", "Expected {What}, found {Token}."), Arguments));
	}

	FLangSpan FLangParser::SpanFrom(int32 StartIndex) const
	{
		const FLangSpan& StartSpan = Tokens[FMath::Clamp(StartIndex, 0, Tokens.Num() - 1)].Span;
		const FLangSpan& EndSpan = Previous().Span;
		if (EndSpan.End() <= StartSpan.Offset)
		{
			// Nothing was consumed since StartIndex; the start token alone is the best span there is.
			return StartSpan;
		}
		return FLangSpan::Join(StartSpan, EndSpan);
	}

	FText FLangParser::DescribeToken(const FLangToken& Token) const
	{
		switch (Token.Kind)
		{
		case ELangTokenKind::EndOfFile:
			return LOCTEXT("DescribeEndOfFile", "end of file");
		case ELangTokenKind::Identifier:
			return FText::Format(LOCTEXT("DescribeIdentifier", "identifier '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::Keyword:
			return FText::Format(LOCTEXT("DescribeKeyword", "keyword '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::IntLiteral:
		case ELangTokenKind::FloatLiteral:
			return FText::Format(LOCTEXT("DescribeNumber", "number '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::StringLiteral:
			return FText::Format(LOCTEXT("DescribeString", "string '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::DocComment:
			return LOCTEXT("DescribeDocComment", "a '///' comment");
		case ELangTokenKind::Directive:
			return FText::Format(LOCTEXT("DescribeDirective", "directive '#{0}'"), FText::FromString(FirstWordOf(Token.Text)));
		case ELangTokenKind::Unknown:
			return FText::Format(LOCTEXT("DescribeUnknown", "'{0}'"), FText::FromString(Token.Text));
		default:
			break;
		}

		const TCHAR* Spelling = GetLangTokenSpelling(Token.Kind);
		if (Spelling && Spelling[0] != TEXT('\0'))
		{
			return FText::Format(LOCTEXT("DescribeSpelling", "'{0}'"), FText::FromString(Spelling));
		}
		return FText::Format(LOCTEXT("DescribeKind", "'{0}'"), FText::FromString(LexToString(Token.Kind)));
	}

	bool FLangParser::FailAtEnd(const FText& WhileParsing)
	{
		return Diagnostics.Error(
			TEXT("DSH2150"),
			Current().Span,
			FText::Format(LOCTEXT("UnexpectedEndOfFile", "Unexpected end of file while parsing {0}."), WhileParsing));
	}

	// ---------------------------------------------------------------------------------------------
	// Recovery
	// ---------------------------------------------------------------------------------------------

	void FLangParser::SkipToDeclarationBoundary()
	{
		int32 Depth = 0;
		int32 ParenDepth = 0;
		while (!AtEnd())
		{
			const FLangToken& Token = Current();

			// Anything at the start of a line that can only be the start of the NEXT declaration
			// (a `///` block, a `#` line, `struct`, `import`, or `[prefix keywords] Type Name`)
			// is not part of the broken one; stop before it so it is not swallowed. Most broken
			// declarations fail at a token that has already consumed their own `;` or `}` (a
			// missing `;`, a prototype without `extern`, a body on an `extern`), and without this
			// rule the skip would run straight through the declaration that follows.
			if (Depth == 0 && ParenDepth == 0 && Token.bAtLineStart)
			{
				const bool bStructOrImport = Token.Kind == ELangTokenKind::Keyword
					&& (Token.Keyword == ELangKeyword::Struct || Token.Keyword == ELangKeyword::Import);
				if (Token.Kind == ELangTokenKind::DocComment
					|| Token.Kind == ELangTokenKind::Directive
					|| bStructOrImport
					|| LooksLikeDeclarationStart())
				{
					return;
				}
			}

			switch (Token.Kind)
			{
			case ELangTokenKind::LeftBrace:
				++Depth;
				Advance();
				break;

			case ELangTokenKind::RightBrace:
				Advance();
				if (Depth == 0)
				{
					// A stray `}` at file scope: it closes nothing, but it is still a boundary.
					return;
				}
				if (--Depth == 0)
				{
					return;
				}
				break;

			case ELangTokenKind::LeftParen:
				++ParenDepth;
				Advance();
				break;

			case ELangTokenKind::RightParen:
				// The skip may start inside a parameter list; a `)` it did not see opened is not a
				// reason to disable the line-start rule for the rest of the file.
				ParenDepth = FMath::Max(0, ParenDepth - 1);
				Advance();
				break;

			case ELangTokenKind::Semicolon:
				Advance();
				if (Depth == 0)
				{
					return;
				}
				break;

			default:
				Advance();
				break;
			}
		}
	}

	void FLangParser::SkipToStatementBoundary()
	{
		int32 Depth = 0;
		while (!AtEnd())
		{
			switch (Current().Kind)
			{
			case ELangTokenKind::Semicolon:
				Advance();
				if (Depth == 0)
				{
					return;
				}
				break;

			case ELangTokenKind::LeftBrace:
				++Depth;
				Advance();
				break;

			case ELangTokenKind::RightBrace:
				if (Depth == 0)
				{
					// The `}` that closes the enclosing block: stop BEFORE it, so the block parser
					// sees its own terminator.
					return;
				}
				--Depth;
				Advance();
				break;

			default:
				Advance();
				break;
			}
		}
	}

	bool FLangParser::SkipBalancedBraces()
	{
		if (!Check(ELangTokenKind::LeftBrace))
		{
			// Callers only reach here standing on a `{`; nothing to skip and nothing to report.
			return false;
		}

		int32 Depth = 0;
		do
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileParsingABlock", "a block"));
			}

			const ELangTokenKind Kind = Advance().Kind;
			if (Kind == ELangTokenKind::LeftBrace)
			{
				++Depth;
			}
			else if (Kind == ELangTokenKind::RightBrace)
			{
				--Depth;
			}
		}
		while (Depth > 0);

		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Entry
	// ---------------------------------------------------------------------------------------------

	TUniquePtr<FModule> FLangParser::ParseModule()
	{
		TUniquePtr<FModule> Module = MakeUnique<FModule>();
		Module->FilePath = Source.GetPath();
		Module->FileKind = FileKind;

		while (!AtEnd())
		{
			const int32 StartIndex = Index;

			FDeclPtr Declaration = ParseDeclaration();
			if (Declaration.IsValid())
			{
				ReportExportInHeader(Diagnostics, FileKind, *Declaration);

				// The node itself never moves (only the TUniquePtr does), so this stays valid
				// across the Add below and across the reallocation the extra names may cause.
				const FVariableDecl* First = Declaration->As<FVariableDecl>();
				Module->Declarations.Add(MoveTemp(Declaration));

				if (First && Check(ELangTokenKind::Comma))
				{
					TArray<FDeclPtr> Extra;
					ParseTrailingVariableDeclarators(*this, *First, Extra);
					for (FDeclPtr& Additional : Extra)
					{
						if (Additional.IsValid())
						{
							ReportExportInHeader(Diagnostics, FileKind, *Additional);
							Module->Declarations.Add(MoveTemp(Additional));
						}
					}
				}
			}
			else
			{
				SkipToDeclarationBoundary();
			}

			// The loop must always make progress: a declaration that failed without consuming
			// anything, and a recovery that had nothing to skip, would otherwise spin here.
			if (Index == StartIndex)
			{
				Advance();
			}
		}

		return Module;
	}

	FExprPtr FLangParser::ParseStandaloneExpression()
	{
		FExprPtr Expression = ParseExpression();
		if (!Expression)
		{
			return nullptr;
		}

		if (!AtEnd())
		{
			// Returning the partial expression here would be the 1.x silent-truncation bug wearing
			// a new hat (`a%b` parsed as `a`), so a trailing token loses the whole result.
			Diagnostics.Error(
				TEXT("DSH3211"),
				Current().Span,
				FText::Format(
					LOCTEXT("TrailingTokenAfterExpression", "Expected the end of the expression, found {0}."),
					DescribeToken(Current())));
			return nullptr;
		}

		return Expression;
	}
}

namespace UE::DreamShader::Lang
{
	namespace
	{
		/**
		 * The spans of a module's `/// @custom` bodies, braces included.
		 *
		 * A `@custom` body is opaque HLSL. The parser lexes it only far enough to find the brace
		 * that closes it and then slices the text straight out of the source, so what the lexer
		 * made of the characters in between was never used for anything. Its complaints about them
		 * -- a `@` in a macro, a `$`, a `'`, a `#` that is not the first thing on its line -- are
		 * therefore not facts about the program: they are this front end objecting to a language
		 * that is not its own. `dsc check --shaders` is what gets to complain about that text.
		 */
		TArray<FLangSpan> GatherOpaqueBodySpans(const FModule& Module)
		{
			TArray<FLangSpan> Spans;
			Module.ForEachDecl(ENodeKind::FunctionDecl, [&Spans](const FDecl& Decl)
			{
				const FFunctionDecl& Function = static_cast<const FFunctionDecl&>(Decl);
				if (Function.bOpaqueBody && Function.BodySpan.Length > 0)
				{
					Spans.Add(Function.BodySpan);
				}
			});
			return Spans;
		}

		bool StartsInsideAnyOf(const FLangSpan& Span, const TArray<FLangSpan>& Ranges)
		{
			for (const FLangSpan& Range : Ranges)
			{
				if (Span.Offset >= Range.Offset && Span.Offset < Range.End())
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Whether a lexical diagnostic may be dropped for falling inside an opaque body.
		 *
		 * Most may: they say "this character begins no token", which is not a fact about text that
		 * was never DreamShaderLang. The two unterminated-construct codes may not. Those mean the
		 * lexer ran past where it believed it was, and the brace matching that decided where the
		 * body ENDS ran over those same tokens -- so the span being suppressed against is itself
		 * suspect. Dropping the one clue would leave a body captured to the wrong brace with
		 * nothing at all said about why.
		 */
		bool MaySuppressInsideOpaqueBody(const FLangDiagnostic& Diagnostic)
		{
			return !Diagnostic.Code.Equals(TEXT("DSH2102"), ESearchCase::CaseSensitive)
				&& !Diagnostic.Code.Equals(TEXT("DSH2103"), ESearchCase::CaseSensitive);
		}

		void Emit(FLangDiagnosticSink& Sink, const FLangDiagnostic& Diagnostic)
		{
			switch (Diagnostic.Severity)
			{
			case ELangSeverity::Error:
				Sink.Error(*Diagnostic.Code, Diagnostic.Span, Diagnostic.Message);
				break;
			case ELangSeverity::Warning:
				Sink.Warning(*Diagnostic.Code, Diagnostic.Span, Diagnostic.Message);
				break;
			case ELangSeverity::Info:
				Sink.Info(*Diagnostic.Code, Diagnostic.Span, Diagnostic.Message);
				break;
			}
		}
	}

	FLangParseResult ParseDreamShaderLang(const FLangSourceText& Source, const FLangParseOptions& Options)
	{
		FLangParseResult Result;
		Result.Diagnostics = FLangDiagnosticSink(Source.GetPath());

		const ELangFileKind FileKind = GetLangFileKindFromPath(Source.GetPath());

		ELangFrontend Frontend = Options.Frontend;
		if (Frontend == ELangFrontend::Auto)
		{
			// `.dss`, `.dsh` and an unknown/absent extension take the 2.0 front end; only the two
			// frozen 1.x extensions ask for the legacy one.
			Frontend = (FileKind == ELangFileKind::Dsm || FileKind == ELangFileKind::Dsf)
				? ELangFrontend::Legacy
				: ELangFrontend::Dss;
		}

		if (Frontend == ELangFrontend::Legacy)
		{
			// M4 brings the second front end; until then the answer is one honest error rather
			// than a 2.0 parse of 1.x text, which would fail token by token and explain nothing.
			Result.Module = MakeUnique<FModule>();
			Result.Module->FilePath = Source.GetPath();
			Result.Module->FileKind = FileKind;

			Result.Diagnostics.Error(
				TEXT("DSH2199"),
				Source.MakeSpan(0, 0),
				LOCTEXT("LegacyFrontendUnavailable", "The 1.x front end is not available in this build."));
			return Result;
		}

		FLangLexOptions LexOptions;
		LexOptions.bEmitDocComments = true;
		LexOptions.bEmitDirectives = true;

		// The two stages report into their own sinks so they can be merged once the tree says which
		// stretches of the file were opaque HLSL (GatherOpaqueBodySpans). A lexing error is recorded
		// and the token stream is used anyway: the Unknown token it leaves behind fails at its own
		// spot, which is a better story than "the file is broken".
		FLangDiagnosticSink LexicalDiagnostics(Source.GetPath());
		FLangDiagnosticSink ParseDiagnostics(Source.GetPath());

		TArray<FLangToken> Tokens;
		LexDreamShaderLang(Source, LexOptions, Tokens, LexicalDiagnostics);

		Private::FLangParser Parser(Source, MoveTemp(Tokens), Frontend, FileKind, ParseDiagnostics);
		Result.Module = Parser.ParseModule();

		TArray<FLangSpan> OpaqueBodies;
		if (Result.Module.IsValid())
		{
			OpaqueBodies = GatherOpaqueBodySpans(*Result.Module);
		}

		// Both lists are already in source order, so one merge keeps them that way: whichever stage
		// noticed it, the first complaint about a file should be the first one reported.
		const TArray<FLangDiagnostic>& Lexical = LexicalDiagnostics.GetDiagnostics();
		const TArray<FLangDiagnostic>& Parsed = ParseDiagnostics.GetDiagnostics();
		int32 LexicalIndex = 0;
		int32 ParsedIndex = 0;
		while (LexicalIndex < Lexical.Num() || ParsedIndex < Parsed.Num())
		{
			const bool bTakeLexical = ParsedIndex >= Parsed.Num()
				|| (LexicalIndex < Lexical.Num() && Lexical[LexicalIndex].Span.Offset <= Parsed[ParsedIndex].Span.Offset);

			if (bTakeLexical)
			{
				const FLangDiagnostic& Diagnostic = Lexical[LexicalIndex++];
				if (!StartsInsideAnyOf(Diagnostic.Span, OpaqueBodies))
				{
					Emit(Result.Diagnostics, Diagnostic);
				}
			}
			else
			{
				Emit(Result.Diagnostics, Parsed[ParsedIndex++]);
			}
		}

		return Result;
	}

	FExprPtr ParseDreamShaderLangExpression(const FLangSourceText& Source, FLangDiagnosticSink& Diagnostics)
	{
		// An expression has no declarations to attach a `///` block to and no line for a `#`
		// directive to own, so both are lexed as plain comments here.
		FLangLexOptions LexOptions;
		LexOptions.bEmitDocComments = false;
		LexOptions.bEmitDirectives = false;

		TArray<FLangToken> Tokens;
		LexDreamShaderLang(Source, LexOptions, Tokens, Diagnostics);

		Private::FLangParser Parser(
			Source,
			MoveTemp(Tokens),
			ELangFrontend::Dss,
			GetLangFileKindFromPath(Source.GetPath()),
			Diagnostics);
		return Parser.ParseStandaloneExpression();
	}
}

#undef LOCTEXT_NAMESPACE
