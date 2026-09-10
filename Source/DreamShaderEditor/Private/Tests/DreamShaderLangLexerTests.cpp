// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShaderLang 2.0 lexer tests (milestone M1, agent A).
//
// Covers the four files of the lexer layer:
//   Source/DreamShaderLang/Private/Lang/LangSource.cpp      -- FLangSourceText, the line table, file kinds
//   Source/DreamShaderLang/Private/Lang/LangDiagnostic.cpp  -- FLangDiagnosticSink, ToWireString
//   Source/DreamShaderLang/Private/Lang/LangToken.cpp       -- keyword table, spellings, LexToString
//   Source/DreamShaderLang/Private/Lang/LangLexer.cpp       -- LexDreamShaderLang
//
// Three conventions run through every test here:
//
//   1. NEVER ASSERT ON MESSAGE TEXT. Diagnostics are asserted by their DSHnnnn code (and, where the
//      position is the point, by their span). The messages are LOCTEXT and the editor is localised.
//
//   2. NEVER USE A MULTI-LINE RAW STRING LITERAL FOR A FIXTURE. Whether a `R"(...)"` spanning
//      physical lines yields `\n` or `\r\n` is a property of the compiler's source mapping, and this
//      file asserts exact offsets, line numbers and columns. Every fixture is built by JoinLines()
//      with the terminator spelled out, so the same test can be run over LF and CRLF text and the
//      difference is the thing under test rather than an accident of the build. Single-line raw
//      strings (no newline inside) are used freely -- they are the readable way to write a fixture
//      that contains quotes and backslashes.
//
//   3. SPANS ARE CHECKED BY SLICING, NOT BY COUNTING. Every expectation carries the text the token's
//      span must cover; Source.Slice(Span) reproducing it pins Offset AND Length exactly, without a
//      table of hand-counted offsets that rots the moment a fixture line changes. Line and column are
//      asserted directly wherever they are the point (line endings, error positions).
//
// FString::operator== is case-INSENSITIVE in Unreal, so every string comparison below goes through
// TestEqualSensitive.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderTestCommon.h"

#include "Lang/LangDiagnostic.h"
#include "Lang/LangLexer.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Text.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::LangLexerTests
{
	using namespace UE::DreamShader::Lang;

	/** The path every fixture claims to come from. Nothing touches the disk. */
	static const TCHAR* const GFixturePath = TEXT("/DreamShaderTests/Lang/Lexer.dss");

	// ---------------------------------------------------------------------------------------------
	// Fixture plumbing
	// ---------------------------------------------------------------------------------------------

	/**
	 * Joins lines with an EXPLICIT terminator and adds none at the end.
	 *
	 * See the header comment: this exists so a fixture's line endings are data, not a property of how
	 * the C++ compiler mapped this source file.
	 */
	static FString JoinLines(const TArray<FString>& Lines, const TCHAR* Terminator = TEXT("\n"))
	{
		FString Result;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result.Append(Terminator);
			}
			Result.Append(Lines[Index]);
		}
		return Result;
	}

	/** One source text, its tokens and its diagnostics, lexed on construction. */
	struct FLexFixture
	{
		FLangSourceText Source;
		FLangDiagnosticSink Diagnostics;
		TArray<FLangToken> Tokens;
		bool bLexSucceeded = false;

		explicit FLexFixture(const FString& InText, const FLangLexOptions& InOptions = FLangLexOptions())
			: Source(FString(GFixturePath), InText)
			, Diagnostics(FString(GFixturePath))
		{
			bLexSucceeded = LexDreamShaderLang(Source, InOptions, Tokens, Diagnostics);
		}

		int32 Num() const { return Tokens.Num(); }

		/** The text the token's span covers -- the whole point of checking spans by slicing. */
		FString SliceOf(int32 Index) const
		{
			return Tokens.IsValidIndex(Index) ? Source.Slice(Tokens[Index].Span) : FString();
		}

		FString FirstErrorCode() const
		{
			const FLangDiagnostic* Error = Diagnostics.FirstError();
			return (Error != nullptr) ? Error->Code : FString();
		}

		/** Every diagnostic code in raise order, comma separated, so a whole run asserts in one line. */
		FString AllCodes() const
		{
			FString Result;
			for (const FLangDiagnostic& Diagnostic : Diagnostics.GetDiagnostics())
			{
				if (!Result.IsEmpty())
				{
					Result.Append(TEXT(","));
				}
				Result.Append(Diagnostic.Code);
			}
			return Result;
		}

		const FLangDiagnostic* DiagnosticAt(int32 Index) const
		{
			const TArray<FLangDiagnostic>& All = Diagnostics.GetDiagnostics();
			return All.IsValidIndex(Index) ? &All[Index] : nullptr;
		}
	};

	/** One expected token. Text and Slice are optional (nullptr skips that check). */
	struct FExpectedToken
	{
		ELangTokenKind Kind = ELangTokenKind::EndOfFile;
		/** What FLangToken::Text must hold. */
		const TCHAR* Text = nullptr;
		/** What Source.Slice(Token.Span) must return. */
		const TCHAR* Slice = nullptr;
		/** What FLangToken::Keyword must hold; None for everything that is not a keyword. */
		ELangKeyword Keyword = ELangKeyword::None;
	};

	static FExpectedToken Punct(const ELangTokenKind Kind)
	{
		// Text is empty for punctuation by contract; the spelling comes from the module itself, so a
		// wrong entry in GetLangTokenSpelling shows up as a slice mismatch rather than passing.
		return FExpectedToken{ Kind, TEXT(""), GetLangTokenSpelling(Kind) };
	}

	static FExpectedToken Ident(const TCHAR* Name)
	{
		return FExpectedToken{ ELangTokenKind::Identifier, Name, Name };
	}

	/** Also asserts the keyword spelling table: the expected lexeme IS LexToString(Keyword). */
	static FExpectedToken Word(const ELangKeyword Keyword)
	{
		return FExpectedToken{ ELangTokenKind::Keyword, LexToString(Keyword), LexToString(Keyword), Keyword };
	}

	static FExpectedToken IntTok(const TCHAR* Lexeme)
	{
		return FExpectedToken{ ELangTokenKind::IntLiteral, Lexeme, Lexeme };
	}

	static FExpectedToken FloatTok(const TCHAR* Lexeme)
	{
		return FExpectedToken{ ELangTokenKind::FloatLiteral, Lexeme, Lexeme };
	}

	static FExpectedToken StrTok(const TCHAR* ResolvedValue, const TCHAR* Lexeme)
	{
		return FExpectedToken{ ELangTokenKind::StringLiteral, ResolvedValue, Lexeme };
	}

	static FExpectedToken DocTok(const TCHAR* Payload, const TCHAR* Lexeme)
	{
		return FExpectedToken{ ELangTokenKind::DocComment, Payload, Lexeme };
	}

	static FExpectedToken DirTok(const TCHAR* Payload, const TCHAR* Lexeme)
	{
		return FExpectedToken{ ELangTokenKind::Directive, Payload, Lexeme };
	}

	static FExpectedToken UnknownTok(const TCHAR* Lexeme)
	{
		return FExpectedToken{ ELangTokenKind::Unknown, Lexeme, Lexeme };
	}

	static FExpectedToken EndTok()
	{
		return FExpectedToken{ ELangTokenKind::EndOfFile, TEXT(""), TEXT("") };
	}

	/** Asserts the whole token stream against a table, reporting the index of the first mismatch. */
	static bool CheckTokens(FAutomationTestBase& Test, const FLexFixture& Fixture, const TArray<FExpectedToken>& Expected)
	{
		bool bOk = Test.TestEqual(TEXT("token count"), Fixture.Tokens.Num(), Expected.Num());

		const int32 Count = FMath::Min(Fixture.Tokens.Num(), Expected.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FLangToken& Token = Fixture.Tokens[Index];
			const FExpectedToken& Want = Expected[Index];

			const FString What = FString::Printf(TEXT("token %d"), Index);

			bOk &= Test.TestEqualSensitive(*(What + TEXT(" kind")),
				FString(LexToString(Token.Kind)), FString(LexToString(Want.Kind)));

			bOk &= Test.TestEqualSensitive(*(What + TEXT(" keyword")),
				FString(LexToString(Token.Keyword)), FString(LexToString(Want.Keyword)));

			if (Want.Text != nullptr)
			{
				bOk &= Test.TestEqualSensitive(*(What + TEXT(" text")), Token.Text, FString(Want.Text));
			}

			if (Want.Slice != nullptr)
			{
				bOk &= Test.TestEqualSensitive(*(What + TEXT(" span slice")),
					Fixture.SliceOf(Index), FString(Want.Slice));
			}
		}

		return bOk;
	}

	static bool CheckLineColumn(FAutomationTestBase& Test, const TCHAR* What, const FLangSpan& Span, const int32 Line, const int32 Column)
	{
		bool bOk = Test.TestEqual(*(FString(What) + TEXT(" line")), Span.Line, Line);
		bOk &= Test.TestEqual(*(FString(What) + TEXT(" column")), Span.Column, Column);
		return bOk;
	}

	/**
	 * FAutomationTestBase::TestEqual has no bool overload -- a raw `TestEqual(What, bA, bB)` picks
	 * whichever numeric overload wins the promotion, which is legal but reads as an accident. Every
	 * flag comparison below goes through this instead.
	 */
	static bool CheckFlag(FAutomationTestBase& Test, const TCHAR* What, const bool bActual, const bool bExpected)
	{
		return bExpected ? Test.TestTrue(What, bActual) : Test.TestFalse(What, bActual);
	}
}

// =================================================================================================
// A small program that exercises every rule in LangLexer.h at once.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerSmallProgramTest,
	"DreamShader.Lang2.Lexer.SmallProgram",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerSmallProgramTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	// 1  #pragma material(Name = "M//A") // tail
	// 2  /// @group Look
	// 3  uniform float3 Tint = float3(1.0f, .5, 0x1Fu);
	// 4  export void Main(inout material M)
	// 5  {
	// 6      /* block
	// 7         comment */ M.BaseColor = Tint * 2.0h; // note
	// 8      if (M.x >= 0.5) { M.y <<= 1; } else { discard; }
	// 9      import "sub.dsh";
	// 10 }
	const FString Text = JoinLines({
		TEXT(R"DSL(#pragma material(Name = "M//A") // tail)DSL"),
		TEXT("/// @group Look"),
		TEXT("uniform float3 Tint = float3(1.0f, .5, 0x1Fu);"),
		TEXT("export void Main(inout material M)"),
		TEXT("{"),
		TEXT("    /* block"),
		TEXT("       comment */ M.BaseColor = Tint * 2.0h; // note"),
		TEXT("    if (M.x >= 0.5) { M.y <<= 1; } else { discard; }"),
		TEXT(R"DSL(    import "sub.dsh";)DSL"),
		TEXT("}")
	});

	const FLexFixture Fixture(Text);

	TestTrue(TEXT("a clean program lexes without errors"), Fixture.bLexSucceeded);
	TestEqual(TEXT("a clean program raises no diagnostics at all"), Fixture.Diagnostics.Num(), 0);

	const TArray<FExpectedToken> Expected = {
		// The directive payload keeps the `//` inside the quoted value and loses the trailing comment.
		DirTok(TEXT(R"DSL(pragma material(Name = "M//A"))DSL"), TEXT(R"DSL(#pragma material(Name = "M//A") // tail)DSL")),
		DocTok(TEXT("@group Look"), TEXT("/// @group Look")),

		Word(ELangKeyword::Uniform),
		Ident(TEXT("float3")),                    // a type name is an IDENTIFIER, never a keyword
		Ident(TEXT("Tint")),
		Punct(ELangTokenKind::Assign),
		Ident(TEXT("float3")),
		Punct(ELangTokenKind::LeftParen),
		FloatTok(TEXT("1.0f")),
		Punct(ELangTokenKind::Comma),
		FloatTok(TEXT(".5")),
		Punct(ELangTokenKind::Comma),
		IntTok(TEXT("0x1Fu")),
		Punct(ELangTokenKind::RightParen),
		Punct(ELangTokenKind::Semicolon),

		Word(ELangKeyword::Export),
		Ident(TEXT("void")),
		Ident(TEXT("Main")),
		Punct(ELangTokenKind::LeftParen),
		Word(ELangKeyword::InOut),
		Ident(TEXT("material")),                  // `material` is an identifier too
		Ident(TEXT("M")),
		Punct(ELangTokenKind::RightParen),
		Punct(ELangTokenKind::LeftBrace),

		// The multi-line block comment is skipped entirely.
		Ident(TEXT("M")),
		Punct(ELangTokenKind::Dot),
		Ident(TEXT("BaseColor")),
		Punct(ELangTokenKind::Assign),
		Ident(TEXT("Tint")),
		Punct(ELangTokenKind::Star),
		FloatTok(TEXT("2.0h")),
		Punct(ELangTokenKind::Semicolon),

		Word(ELangKeyword::If),
		Punct(ELangTokenKind::LeftParen),
		Ident(TEXT("M")),
		Punct(ELangTokenKind::Dot),
		Ident(TEXT("x")),
		Punct(ELangTokenKind::GreaterEqual),
		FloatTok(TEXT("0.5")),
		Punct(ELangTokenKind::RightParen),
		Punct(ELangTokenKind::LeftBrace),
		Ident(TEXT("M")),
		Punct(ELangTokenKind::Dot),
		Ident(TEXT("y")),
		Punct(ELangTokenKind::LessLessAssign),
		IntTok(TEXT("1")),
		Punct(ELangTokenKind::Semicolon),
		Punct(ELangTokenKind::RightBrace),
		Word(ELangKeyword::Else),
		Punct(ELangTokenKind::LeftBrace),
		Word(ELangKeyword::Discard),
		Punct(ELangTokenKind::Semicolon),
		Punct(ELangTokenKind::RightBrace),

		Word(ELangKeyword::Import),
		StrTok(TEXT("sub.dsh"), TEXT(R"DSL("sub.dsh")DSL")),
		Punct(ELangTokenKind::Semicolon),

		Punct(ELangTokenKind::RightBrace),
		EndTok()
	};

	if (!CheckTokens(*this, Fixture, Expected))
	{
		return false;
	}

	// Positions of a few landmarks: the directive on line 1, the declaration on line 3, the statement
	// that follows a multi-line block comment on line 7, the import on line 9.
	CheckLineColumn(*this, TEXT("directive"), Fixture.Tokens[0].Span, 1, 1);
	CheckLineColumn(*this, TEXT("doc comment"), Fixture.Tokens[1].Span, 2, 1);
	CheckLineColumn(*this, TEXT("uniform"), Fixture.Tokens[2].Span, 3, 1);
	CheckLineColumn(*this, TEXT("export"), Fixture.Tokens[15].Span, 4, 1);
	CheckLineColumn(*this, TEXT("M after the block comment"), Fixture.Tokens[24].Span, 7, 19);
	CheckLineColumn(*this, TEXT("import"), Fixture.Tokens[53].Span, 9, 5);
	CheckLineColumn(*this, TEXT("closing brace"), Fixture.Tokens[56].Span, 10, 1);

	// bAtLineStart: true for the directive, the doc comment and every declaration head; false for a
	// token a block comment shares its line with.
	TestTrue(TEXT("the directive is at line start"), Fixture.Tokens[0].bAtLineStart);
	TestTrue(TEXT("the doc comment is at line start"), Fixture.Tokens[1].bAtLineStart);
	TestTrue(TEXT("`uniform` is at line start"), Fixture.Tokens[2].bAtLineStart);
	TestFalse(TEXT("`float3` after `uniform` is not at line start"), Fixture.Tokens[3].bAtLineStart);
	TestFalse(TEXT("a token after a block comment is not at line start"), Fixture.Tokens[24].bAtLineStart);
	TestTrue(TEXT("`import` is at line start"), Fixture.Tokens[53].bAtLineStart);

	// Literal payloads.
	TestEqual(TEXT("0x1Fu value"), static_cast<int64>(Fixture.Tokens[12].Integer), static_cast<int64>(31));
	TestTrue(TEXT("0x1Fu is unsigned"), Fixture.Tokens[12].bUnsigned);
	TestEqual(TEXT("1.0f value"), Fixture.Tokens[8].Real, 1.0);
	TestEqual(TEXT(".5 value"), Fixture.Tokens[10].Real, 0.5);
	TestEqual(TEXT("2.0h value"), Fixture.Tokens[30].Real, 2.0);
	TestEqual(TEXT("literal `1` value"), static_cast<int64>(Fixture.Tokens[45].Integer), static_cast<int64>(1));
	TestFalse(TEXT("literal `1` is not unsigned"), Fixture.Tokens[45].bUnsigned);

	// The EndOfFile token sits AT the end of the text with an empty span.
	const FLangToken& End = Fixture.Tokens.Last();
	TestEqual(TEXT("EndOfFile offset is the end of the text"), End.Span.Offset, Text.Len());
	TestEqual(TEXT("EndOfFile span is empty"), End.Span.Length, 0);
	CheckLineColumn(*this, TEXT("EndOfFile"), End.Span, 10, 2);

	return true;
}

// =================================================================================================
// Operators and punctuation: the whole table, and longest-match with no separating whitespace.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerOperatorsTest,
	"DreamShader.Lang2.Lexer.OperatorsLongestMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerOperatorsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	// Every punctuation and operator kind the token enum declares, separated by spaces.
	{
		const FString Text = TEXT("<<= >>= << >> <= >= == != && || ++ -- += -= *= /= %= &= |= ^= = < > + - * / % & | ^ ~ ! ? : . , ; ( ) { } [ ]");
		const FLexFixture Fixture(Text);

		TestTrue(TEXT("the operator table lexes cleanly"), Fixture.bLexSucceeded);

		const TArray<FExpectedToken> Expected = {
			Punct(ELangTokenKind::LessLessAssign),
			Punct(ELangTokenKind::GreaterGreaterAssign),
			Punct(ELangTokenKind::LessLess),
			Punct(ELangTokenKind::GreaterGreater),
			Punct(ELangTokenKind::LessEqual),
			Punct(ELangTokenKind::GreaterEqual),
			Punct(ELangTokenKind::EqualEqual),
			Punct(ELangTokenKind::BangEqual),
			Punct(ELangTokenKind::AmpersandAmpersand),
			Punct(ELangTokenKind::PipePipe),
			Punct(ELangTokenKind::PlusPlus),
			Punct(ELangTokenKind::MinusMinus),
			Punct(ELangTokenKind::PlusAssign),
			Punct(ELangTokenKind::MinusAssign),
			Punct(ELangTokenKind::StarAssign),
			Punct(ELangTokenKind::SlashAssign),
			Punct(ELangTokenKind::PercentAssign),
			Punct(ELangTokenKind::AmpersandAssign),
			Punct(ELangTokenKind::PipeAssign),
			Punct(ELangTokenKind::CaretAssign),
			Punct(ELangTokenKind::Assign),
			Punct(ELangTokenKind::Less),
			Punct(ELangTokenKind::Greater),
			Punct(ELangTokenKind::Plus),
			Punct(ELangTokenKind::Minus),
			Punct(ELangTokenKind::Star),
			Punct(ELangTokenKind::Slash),
			Punct(ELangTokenKind::Percent),
			Punct(ELangTokenKind::Ampersand),
			Punct(ELangTokenKind::Pipe),
			Punct(ELangTokenKind::Caret),
			Punct(ELangTokenKind::Tilde),
			Punct(ELangTokenKind::Bang),
			Punct(ELangTokenKind::Question),
			Punct(ELangTokenKind::Colon),
			Punct(ELangTokenKind::Dot),
			Punct(ELangTokenKind::Comma),
			Punct(ELangTokenKind::Semicolon),
			Punct(ELangTokenKind::LeftParen),
			Punct(ELangTokenKind::RightParen),
			Punct(ELangTokenKind::LeftBrace),
			Punct(ELangTokenKind::RightBrace),
			Punct(ELangTokenKind::LeftBracket),
			Punct(ELangTokenKind::RightBracket),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
	}

	// Longest match with nothing to separate the operator from its operands -- where a table in the
	// wrong order turns `a <<= b` into `a < < = b`.
	{
		const FString Text = TEXT("a<<=b;a>>=b;a<<b;a>>b;a<b;a>b;a<=b;a/=b;a/b;a++;--a;");
		const FLexFixture Fixture(Text);

		TestTrue(TEXT("the unspaced operators lex cleanly"), Fixture.bLexSucceeded);

		const TArray<FExpectedToken> Expected = {
			Ident(TEXT("a")), Punct(ELangTokenKind::LessLessAssign),       Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::GreaterGreaterAssign), Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::LessLess),             Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::GreaterGreater),       Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::Less),                 Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::Greater),              Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::LessEqual),            Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::SlashAssign),          Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::Slash),                Ident(TEXT("b")), Punct(ELangTokenKind::Semicolon),
			Ident(TEXT("a")), Punct(ELangTokenKind::PlusPlus),             Punct(ELangTokenKind::Semicolon),
			Punct(ELangTokenKind::MinusMinus), Ident(TEXT("a")),           Punct(ELangTokenKind::Semicolon),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
	}

	// GetLangTokenSpelling answers only for punctuation; everything else has its spelling in Text.
	TestEqualSensitive(TEXT("<<= spelling"), FString(GetLangTokenSpelling(ELangTokenKind::LessLessAssign)), FString(TEXT("<<=")));
	TestEqualSensitive(TEXT(">>= spelling"), FString(GetLangTokenSpelling(ELangTokenKind::GreaterGreaterAssign)), FString(TEXT(">>=")));
	TestEqualSensitive(TEXT("identifier has no fixed spelling"), FString(GetLangTokenSpelling(ELangTokenKind::Identifier)), FString(TEXT("")));
	TestEqualSensitive(TEXT("keyword has no fixed spelling"), FString(GetLangTokenSpelling(ELangTokenKind::Keyword)), FString(TEXT("")));
	TestEqualSensitive(TEXT("EndOfFile has no fixed spelling"), FString(GetLangTokenSpelling(ELangTokenKind::EndOfFile)), FString(TEXT("")));
	TestEqualSensitive(TEXT("Unknown has no fixed spelling"), FString(GetLangTokenSpelling(ELangTokenKind::Unknown)), FString(TEXT("")));

	return true;
}

// =================================================================================================
// Line endings. The same logical text as LF, CRLF, lone CR and a mixture must report the same
// line/column for every token -- only the offsets may differ.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerLineEndingsTest,
	"DreamShader.Lang2.Lexer.LineEndings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerLineEndingsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	struct FEndingCase
	{
		const TCHAR* What;
		const TCHAR* Terminator;
		int32 SecondLineOffset;
		int32 ThirdLineOffset;
		int32 EndOffset;
	};

	// "a<T>bb<T>ccc" -- one, two and three characters, so an off-by-one in the line table shows up as
	// a wrong column rather than a wrong line.
	const FEndingCase Cases[] = {
		{ TEXT("LF"),   TEXT("\n"),   2, 5, 8 },
		{ TEXT("CRLF"), TEXT("\r\n"), 3, 7, 10 },
		{ TEXT("CR"),   TEXT("\r"),   2, 5, 8 },
	};

	for (const FEndingCase& Case : Cases)
	{
		const FString Text = JoinLines({ TEXT("a"), TEXT("bb"), TEXT("ccc") }, Case.Terminator);
		const FLexFixture Fixture(Text);

		const FString Prefix = FString(Case.What) + TEXT(": ");

		if (!TestEqual(*(Prefix + TEXT("token count")), Fixture.Num(), 4))
		{
			continue;
		}

		CheckLineColumn(*this, *(Prefix + TEXT("a")), Fixture.Tokens[0].Span, 1, 1);
		CheckLineColumn(*this, *(Prefix + TEXT("bb")), Fixture.Tokens[1].Span, 2, 1);
		CheckLineColumn(*this, *(Prefix + TEXT("ccc")), Fixture.Tokens[2].Span, 3, 1);
		CheckLineColumn(*this, *(Prefix + TEXT("EndOfFile")), Fixture.Tokens[3].Span, 3, 4);

		// Offsets are the terminator-dependent half: they are what the printer and the raw-body
		// capture slice with, so they must follow the physical text.
		TestEqual(*(Prefix + TEXT("bb offset")), Fixture.Tokens[1].Span.Offset, Case.SecondLineOffset);
		TestEqual(*(Prefix + TEXT("ccc offset")), Fixture.Tokens[2].Span.Offset, Case.ThirdLineOffset);
		TestEqual(*(Prefix + TEXT("EndOfFile offset")), Fixture.Tokens[3].Span.Offset, Case.EndOffset);

		TestTrue(*(Prefix + TEXT("a is at line start")), Fixture.Tokens[0].bAtLineStart);
		TestTrue(*(Prefix + TEXT("bb is at line start")), Fixture.Tokens[1].bAtLineStart);
		TestTrue(*(Prefix + TEXT("ccc is at line start")), Fixture.Tokens[2].bAtLineStart);

		TestEqualSensitive(*(Prefix + TEXT("line 2 text")), Fixture.Source.GetLineText(2), FString(TEXT("bb")));
		TestEqual(*(Prefix + TEXT("line count")), Fixture.Source.GetLineCount(), 3);
	}

	// A file that mixes all three terminators still counts one line per break.
	{
		const FString Text = FString(TEXT("a\r\nbb\nccc\rd"));
		const FLexFixture Fixture(Text);

		if (TestEqual(TEXT("mixed: token count"), Fixture.Num(), 5))
		{
			CheckLineColumn(*this, TEXT("mixed: a"), Fixture.Tokens[0].Span, 1, 1);
			CheckLineColumn(*this, TEXT("mixed: bb"), Fixture.Tokens[1].Span, 2, 1);
			CheckLineColumn(*this, TEXT("mixed: ccc"), Fixture.Tokens[2].Span, 3, 1);
			CheckLineColumn(*this, TEXT("mixed: d"), Fixture.Tokens[3].Span, 4, 1);
			CheckLineColumn(*this, TEXT("mixed: EndOfFile"), Fixture.Tokens[4].Span, 4, 2);
			TestEqual(TEXT("mixed: d offset"), Fixture.Tokens[3].Span.Offset, 10);
		}
	}

	// Doc comments and directives report the same line whatever the terminator is.
	{
		const TArray<FString> Lines = { TEXT("#pragma a"), TEXT("/// doc"), TEXT("int x;") };
		const FEndingCase DirectiveCases[] = {
			{ TEXT("directive/doc LF"),   TEXT("\n"),   0, 0, 0 },
			{ TEXT("directive/doc CRLF"), TEXT("\r\n"), 0, 0, 0 },
		};

		for (const FEndingCase& Case : DirectiveCases)
		{
			const FLexFixture Fixture(JoinLines(Lines, Case.Terminator));
			const FString Prefix = FString(Case.What) + TEXT(": ");

			if (TestEqual(*(Prefix + TEXT("token count")), Fixture.Num(), 6))
			{
				CheckLineColumn(*this, *(Prefix + TEXT("directive")), Fixture.Tokens[0].Span, 1, 1);
				CheckLineColumn(*this, *(Prefix + TEXT("doc comment")), Fixture.Tokens[1].Span, 2, 1);
				CheckLineColumn(*this, *(Prefix + TEXT("int")), Fixture.Tokens[2].Span, 3, 1);
				TestEqualSensitive(*(Prefix + TEXT("directive payload")), Fixture.Tokens[0].Text, FString(TEXT("pragma a")));
				TestEqualSensitive(*(Prefix + TEXT("doc payload")), Fixture.Tokens[1].Text, FString(TEXT("doc")));
			}
		}
	}

	return true;
}

// =================================================================================================
// FLangSourceText on its own: the line table at every boundary.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerSourceTextTest,
	"DreamShader.Lang2.Lexer.SourceTextBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerSourceTextTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	// A default-constructed text has no line table at all; every query must still answer.
	{
		const FLangSourceText Empty;
		int32 Line = 0;
		int32 Column = 0;

		TestEqual(TEXT("default text length"), Empty.Len(), 0);
		TestEqual(TEXT("default text line count"), Empty.GetLineCount(), 0);
		TestEqualSensitive(TEXT("default text line 1"), Empty.GetLineText(1), FString(TEXT("")));

		Empty.GetLineAndColumn(0, Line, Column);
		TestEqual(TEXT("default text line at 0"), Line, 1);
		TestEqual(TEXT("default text column at 0"), Column, 1);

		Empty.GetLineAndColumn(50, Line, Column);
		TestEqual(TEXT("default text clamps a wild offset to line 1"), Line, 1);
		TestEqual(TEXT("default text clamps a wild offset to column 1"), Column, 1);

		const FLangSpan Span = Empty.MakeSpan(10, 10);
		TestEqual(TEXT("default text span offset clamps"), Span.Offset, 0);
		TestEqual(TEXT("default text span length clamps"), Span.Length, 0);
		TestEqualSensitive(TEXT("default text slice"), Empty.Slice(Span), FString(TEXT("")));
	}

	// LF text with a trailing terminator: the empty last line is a real line.
	{
		const FLangSourceText Source(FString(TEXT("p.dss")), FString(TEXT("a\nbb\n")));
		int32 Line = 0;
		int32 Column = 0;

		TestEqual(TEXT("LF line count includes the empty last line"), Source.GetLineCount(), 3);
		TestEqualSensitive(TEXT("LF line 0 is out of range"), Source.GetLineText(0), FString(TEXT("")));
		TestEqualSensitive(TEXT("LF line 1"), Source.GetLineText(1), FString(TEXT("a")));
		TestEqualSensitive(TEXT("LF line 2"), Source.GetLineText(2), FString(TEXT("bb")));
		TestEqualSensitive(TEXT("LF line 3 is empty"), Source.GetLineText(3), FString(TEXT("")));
		TestEqualSensitive(TEXT("LF line 4 is out of range"), Source.GetLineText(4), FString(TEXT("")));

		Source.GetLineAndColumn(0, Line, Column);
		TestEqual(TEXT("LF offset 0 line"), Line, 1);
		TestEqual(TEXT("LF offset 0 column"), Column, 1);

		// The terminator itself still belongs to the line it ends.
		Source.GetLineAndColumn(1, Line, Column);
		TestEqual(TEXT("LF offset 1 line"), Line, 1);
		TestEqual(TEXT("LF offset 1 column"), Column, 2);

		Source.GetLineAndColumn(2, Line, Column);
		TestEqual(TEXT("LF offset 2 line"), Line, 2);
		TestEqual(TEXT("LF offset 2 column"), Column, 1);

		// AT the end of the text, and past it: both are the last position.
		Source.GetLineAndColumn(5, Line, Column);
		TestEqual(TEXT("LF end-of-text line"), Line, 3);
		TestEqual(TEXT("LF end-of-text column"), Column, 1);

		Source.GetLineAndColumn(999, Line, Column);
		TestEqual(TEXT("LF past-the-end line"), Line, 3);
		TestEqual(TEXT("LF past-the-end column"), Column, 1);

		TestEqualSensitive(TEXT("LF slice of line 2"), Source.Slice(Source.MakeSpan(2, 2)), FString(TEXT("bb")));

		const FLangSpan Clamped = Source.MakeSpan(4, 100);
		TestEqual(TEXT("LF over-long span length clamps"), Clamped.Length, 1);
	}

	// CRLF answers exactly the same line texts; only the offsets shift.
	{
		const FLangSourceText Source(FString(TEXT("p.dss")), FString(TEXT("a\r\nbb\r\n")));
		int32 Line = 0;
		int32 Column = 0;

		TestEqual(TEXT("CRLF line count"), Source.GetLineCount(), 3);
		TestEqualSensitive(TEXT("CRLF line 1 drops its terminator"), Source.GetLineText(1), FString(TEXT("a")));
		TestEqualSensitive(TEXT("CRLF line 2 drops its terminator"), Source.GetLineText(2), FString(TEXT("bb")));
		TestEqualSensitive(TEXT("CRLF line 3 is empty"), Source.GetLineText(3), FString(TEXT("")));

		Source.GetLineAndColumn(3, Line, Column);
		TestEqual(TEXT("CRLF start of line 2 line"), Line, 2);
		TestEqual(TEXT("CRLF start of line 2 column"), Column, 1);
	}

	// A lone CR is one break too.
	{
		const FLangSourceText Source(FString(TEXT("p.dss")), FString(TEXT("a\rbb")));
		int32 Line = 0;
		int32 Column = 0;

		TestEqual(TEXT("CR line count"), Source.GetLineCount(), 2);
		TestEqualSensitive(TEXT("CR line 2"), Source.GetLineText(2), FString(TEXT("bb")));

		Source.GetLineAndColumn(2, Line, Column);
		TestEqual(TEXT("CR start of line 2 line"), Line, 2);
		TestEqual(TEXT("CR start of line 2 column"), Column, 1);
	}

	// Text with no terminator at all is one line.
	{
		const FLangSourceText Source(FString(TEXT("p.dss")), FString(TEXT("abc")));
		TestEqual(TEXT("single line count"), Source.GetLineCount(), 1);
		TestEqualSensitive(TEXT("single line text"), Source.GetLineText(1), FString(TEXT("abc")));
		TestEqualSensitive(TEXT("single line path is kept verbatim"), Source.GetPath(), FString(TEXT("p.dss")));
		TestEqualSensitive(TEXT("single line text is kept verbatim"), Source.GetText(), FString(TEXT("abc")));
	}

	return true;
}

// =================================================================================================
// File kinds -- the other half of LangSource.cpp.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerFileKindTest,
	"DreamShader.Lang2.Lexer.FileKind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerFileKindTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;

	struct FKindCase
	{
		const TCHAR* Path;
		ELangFileKind Kind;
	};

	const FKindCase Cases[] = {
		{ TEXT("A.dss"),                      ELangFileKind::Dss },
		{ TEXT("A.DSS"),                      ELangFileKind::Dss },     // extensions are case-insensitive
		{ TEXT("A.dsh"),                      ELangFileKind::Dsh },
		{ TEXT("A.DsH"),                      ELangFileKind::Dsh },
		{ TEXT("C:/x/y/A.dsm"),               ELangFileKind::Dsm },
		{ TEXT("/Game/DShader/A.dsf"),        ELangFileKind::Dsf },
		{ TEXT("A.txt"),                      ELangFileKind::Unknown },
		{ TEXT("A"),                          ELangFileKind::Unknown },
		{ TEXT(""),                           ELangFileKind::Unknown },
		// Only the REAL extension selects a front end: a backup copy is not a .dsm.
		{ TEXT("Foo.dsm.bak"),                ELangFileKind::Unknown },
	};

	for (const FKindCase& Case : Cases)
	{
		const FString What = FString(TEXT("kind of '")) + Case.Path + TEXT("'");
		TestEqualSensitive(*What,
			FString(LexToString(GetLangFileKindFromPath(FString(Case.Path)))),
			FString(LexToString(Case.Kind)));
	}

	TestEqualSensitive(TEXT("Dss name"), FString(LexToString(ELangFileKind::Dss)), FString(TEXT("Dss")));
	TestEqualSensitive(TEXT("Dsh name"), FString(LexToString(ELangFileKind::Dsh)), FString(TEXT("Dsh")));
	TestEqualSensitive(TEXT("Dsm name"), FString(LexToString(ELangFileKind::Dsm)), FString(TEXT("Dsm")));
	TestEqualSensitive(TEXT("Dsf name"), FString(LexToString(ELangFileKind::Dsf)), FString(TEXT("Dsf")));
	TestEqualSensitive(TEXT("Unknown name"), FString(LexToString(ELangFileKind::Unknown)), FString(TEXT("Unknown")));

	return true;
}

// =================================================================================================
// Doc comments: `///` yes, `//` and `////` no.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerDocCommentsTest,
	"DreamShader.Lang2.Lexer.DocComments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerDocCommentsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	const TArray<FString> Lines = {
		TEXT("/// first"),
		TEXT("//// separator ////"),   // four slashes: an ordinary comment, NOT a doc block
		TEXT("///// five"),            // and so is five
		TEXT("// plain"),
		TEXT("///no space"),           // no leading space to remove
		TEXT("///  two spaces"),       // exactly ONE leading space is removed
		TEXT("///"),                   // an empty doc line is still a doc line
		TEXT("int a; /// trailing")
	};

	{
		const FLexFixture Fixture(JoinLines(Lines));

		TestTrue(TEXT("doc comments lex cleanly"), Fixture.bLexSucceeded);

		const TArray<FExpectedToken> Expected = {
			DocTok(TEXT("first"), TEXT("/// first")),
			DocTok(TEXT("no space"), TEXT("///no space")),
			DocTok(TEXT(" two spaces"), TEXT("///  two spaces")),
			DocTok(TEXT(""), TEXT("///")),
			Ident(TEXT("int")),
			Ident(TEXT("a")),
			Punct(ELangTokenKind::Semicolon),
			DocTok(TEXT("trailing"), TEXT("/// trailing")),
			EndTok()
		};

		if (CheckTokens(*this, Fixture, Expected))
		{
			TestTrue(TEXT("a doc comment on its own line is at line start"), Fixture.Tokens[0].bAtLineStart);
			TestFalse(TEXT("a doc comment after code is not at line start"), Fixture.Tokens[7].bAtLineStart);
			CheckLineColumn(*this, TEXT("first doc comment"), Fixture.Tokens[0].Span, 1, 1);
			CheckLineColumn(*this, TEXT("empty doc comment"), Fixture.Tokens[3].Span, 7, 1);
			CheckLineColumn(*this, TEXT("trailing doc comment"), Fixture.Tokens[7].Span, 8, 8);
		}
	}

	// With bEmitDocComments off, a `///` line is skipped like any other comment.
	{
		FLangLexOptions Options;
		Options.bEmitDocComments = false;

		const FLexFixture Fixture(JoinLines(Lines), Options);

		const TArray<FExpectedToken> Expected = {
			Ident(TEXT("int")),
			Ident(TEXT("a")),
			Punct(ELangTokenKind::Semicolon),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
	}

	// A `///` at the very end of the file, with no terminator behind it, is still a doc comment.
	{
		const FLexFixture Fixture(FString(TEXT("///x")));
		const TArray<FExpectedToken> Expected = { DocTok(TEXT("x"), TEXT("///x")), EndTok() };
		CheckTokens(*this, Fixture, Expected);
	}

	// ... and `////` at the very end is still not one.
	{
		const FLexFixture Fixture(FString(TEXT("////x")));
		const TArray<FExpectedToken> Expected = { EndTok() };
		CheckTokens(*this, Fixture, Expected);
	}

	return true;
}

// =================================================================================================
// Directives: `#` at line start only, payload trimmed, trailing comment stripped STRING-AWARE.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerDirectivesTest,
	"DreamShader.Lang2.Lexer.Directives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerDirectivesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	const TArray<FString> Lines = {
		// Leading whitespace still counts as "first thing on the line". The `//` INSIDE the quoted
		// value must survive; only the one outside is a comment.
		TEXT(R"DSL(  #pragma material(Name = "M//A") // strip me)DSL"),
		// A path with a doubled separator is the case this rule exists for.
		TEXT(R"DSL(#include "/Game/A//B.dsh")DSL"),
		TEXT("#if DS_FOO"),
		TEXT("#pragma region Look"),
		// A `#` that is not the first thing on its line is not a directive.
		TEXT("int a = 1; # bad"),
		// A bare `#` is an empty directive, not an error.
		TEXT("#")
	};

	{
		const FLexFixture Fixture(JoinLines(Lines));

		const TArray<FExpectedToken> Expected = {
			DirTok(TEXT(R"DSL(pragma material(Name = "M//A"))DSL"), TEXT(R"DSL(#pragma material(Name = "M//A") // strip me)DSL")),
			DirTok(TEXT(R"DSL(include "/Game/A//B.dsh")DSL"), TEXT(R"DSL(#include "/Game/A//B.dsh")DSL")),
			DirTok(TEXT("if DS_FOO"), TEXT("#if DS_FOO")),
			DirTok(TEXT("pragma region Look"), TEXT("#pragma region Look")),
			Ident(TEXT("int")),
			Ident(TEXT("a")),
			Punct(ELangTokenKind::Assign),
			IntTok(TEXT("1")),
			Punct(ELangTokenKind::Semicolon),
			UnknownTok(TEXT("#")),
			Ident(TEXT("bad")),
			DirTok(TEXT(""), TEXT("#")),
			EndTok()
		};

		if (CheckTokens(*this, Fixture, Expected))
		{
			// The whole line is the directive's span, so a parser diagnostic underlines the directive.
			CheckLineColumn(*this, TEXT("indented directive"), Fixture.Tokens[0].Span, 1, 3);
			TestTrue(TEXT("an indented directive is still at line start"), Fixture.Tokens[0].bAtLineStart);
			TestTrue(TEXT("#include is at line start"), Fixture.Tokens[1].bAtLineStart);
			TestFalse(TEXT("a mid-line # is not at line start"), Fixture.Tokens[9].bAtLineStart);
			CheckLineColumn(*this, TEXT("mid-line #"), Fixture.Tokens[9].Span, 5, 12);
		}

		// The only complaint is the misplaced `#`; lexing continues and `bad` is still an identifier.
		TestFalse(TEXT("a misplaced # makes the lex fail"), Fixture.bLexSucceeded);
		TestEqualSensitive(TEXT("codes raised"), Fixture.AllCodes(), FString(TEXT("DSH2106")));
		TestEqual(TEXT("error count"), Fixture.Diagnostics.NumErrors(), 1);
		if (const FLangDiagnostic* Diagnostic = Fixture.DiagnosticAt(0))
		{
			TestEqual(TEXT("DSH2106 span length"), Diagnostic->Span.Length, 1);
			TestEqual(TEXT("DSH2106 line"), Diagnostic->Span.Line, 5);
			TestEqualSensitive(TEXT("DSH2106 file path"), Diagnostic->FilePath, FString(GFixturePath));
		}
	}

	// With bEmitDirectives off, `#` lines vanish -- but a misplaced `#` is still a mistake.
	{
		FLangLexOptions Options;
		Options.bEmitDirectives = false;

		const FLexFixture Fixture(JoinLines(Lines), Options);

		const TArray<FExpectedToken> Expected = {
			Ident(TEXT("int")),
			Ident(TEXT("a")),
			Punct(ELangTokenKind::Assign),
			IntTok(TEXT("1")),
			Punct(ELangTokenKind::Semicolon),
			UnknownTok(TEXT("#")),
			Ident(TEXT("bad")),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
		TestEqualSensitive(TEXT("codes raised with directives off"), Fixture.AllCodes(), FString(TEXT("DSH2106")));
	}

	// A directive is the LAST thing on its line: the following line lexes as code again.
	{
		const FLexFixture Fixture(JoinLines({ TEXT("#pragma x"), TEXT("float y;") }));
		const TArray<FExpectedToken> Expected = {
			DirTok(TEXT("pragma x"), TEXT("#pragma x")),
			Ident(TEXT("float")),
			Ident(TEXT("y")),
			Punct(ELangTokenKind::Semicolon),
			EndTok()
		};
		CheckTokens(*this, Fixture, Expected);
	}

	return true;
}

// =================================================================================================
// Numbers: kind, lexeme, value, unsignedness.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerNumbersTest,
	"DreamShader.Lang2.Lexer.Numbers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerNumbersTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	const FString Text = TEXT("1 2u 3U 4l 5L 0x10 0xFF 0x1Fu 1.0f 2.0h .5 1. 1e3 1E-2 6F 7H 0");
	const FLexFixture Fixture(Text);

	TestTrue(TEXT("well-formed numbers lex cleanly"), Fixture.bLexSucceeded);
	TestEqual(TEXT("no diagnostics"), Fixture.Diagnostics.Num(), 0);

	const TArray<FExpectedToken> Expected = {
		IntTok(TEXT("1")),
		IntTok(TEXT("2u")),
		IntTok(TEXT("3U")),
		IntTok(TEXT("4l")),
		IntTok(TEXT("5L")),
		IntTok(TEXT("0x10")),
		IntTok(TEXT("0xFF")),
		IntTok(TEXT("0x1Fu")),
		FloatTok(TEXT("1.0f")),
		FloatTok(TEXT("2.0h")),
		FloatTok(TEXT(".5")),
		FloatTok(TEXT("1.")),
		FloatTok(TEXT("1e3")),
		FloatTok(TEXT("1E-2")),
		FloatTok(TEXT("6F")),
		FloatTok(TEXT("7H")),
		IntTok(TEXT("0")),
		EndTok()
	};

	if (!CheckTokens(*this, Fixture, Expected))
	{
		return false;
	}

	struct FIntCase
	{
		int32 Index;
		int64 Value;
		bool bUnsigned;
	};

	const FIntCase IntCases[] = {
		{ 0,   1, false },
		{ 1,   2, true  },   // `u`
		{ 2,   3, true  },   // `U`
		{ 3,   4, false },   // `l` is a size suffix, not a sign suffix
		{ 4,   5, false },   // `L`
		{ 5,  16, false },   // 0x10
		{ 6, 255, false },   // 0xFF -- `F` is a hex DIGIT here, not a float suffix
		{ 7,  31, true  },   // 0x1Fu
		{ 16,  0, false },
	};

	for (const FIntCase& Case : IntCases)
	{
		const FString What = FString::Printf(TEXT("integer %d"), Case.Index);
		TestEqual(*(What + TEXT(" value")), static_cast<int64>(Fixture.Tokens[Case.Index].Integer), Case.Value);
		CheckFlag(*this, *(What + TEXT(" unsigned")), Fixture.Tokens[Case.Index].bUnsigned, Case.bUnsigned);
	}

	struct FFloatCase
	{
		int32 Index;
		double Value;
	};

	const FFloatCase FloatCases[] = {
		{ 8,  1.0 },     // 1.0f
		{ 9,  2.0 },     // 2.0h
		{ 10, 0.5 },     // .5
		{ 11, 1.0 },     // 1.
		{ 12, 1000.0 },  // 1e3
		{ 13, 0.01 },    // 1E-2
		{ 14, 6.0 },     // 6F
		{ 15, 7.0 },     // 7H
	};

	for (const FFloatCase& Case : FloatCases)
	{
		TestEqual(*FString::Printf(TEXT("float %d value"), Case.Index), Fixture.Tokens[Case.Index].Real, Case.Value);
	}

	// A number glued to a member access is still a number followed by `.` only when the `.` cannot be
	// part of it: `x.5` is a member access on `x`, `2.5` is one float.
	{
		const FLexFixture Members(FString(TEXT("x.y 2.5")));
		const TArray<FExpectedToken> MemberExpected = {
			Ident(TEXT("x")),
			Punct(ELangTokenKind::Dot),
			Ident(TEXT("y")),
			FloatTok(TEXT("2.5")),
			EndTok()
		};
		CheckTokens(*this, Members, MemberExpected);
	}

	return true;
}

// =================================================================================================
// Strings: the six C escapes, the unknown-escape rule, and the never-across-a-line rule.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerStringsTest,
	"DreamShader.Lang2.Lexer.Strings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerStringsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	const FString Text = JoinLines({
		TEXT(R"DSL("a\\b")DSL"),
		TEXT(R"DSL("a\"b")DSL"),
		TEXT(R"DSL("a\nb")DSL"),
		TEXT(R"DSL("a\rb")DSL"),
		TEXT(R"DSL("a\tb")DSL"),
		TEXT(R"DSL("a\0b")DSL"),
		TEXT(R"DSL("a\qb")DSL"),          // unknown escape -- DSH2104, backslash kept
		TEXT(R"DSL("unterminated)DSL"),   // DSH2103, and the next line still lexes as code
		TEXT("next")
	});

	const FLexFixture Fixture(Text);

	const TArray<FExpectedToken> Expected = {
		StrTok(TEXT("a\\b"), TEXT(R"DSL("a\\b")DSL")),
		StrTok(TEXT("a\"b"), TEXT(R"DSL("a\"b")DSL")),
		StrTok(TEXT("a\nb"), TEXT(R"DSL("a\nb")DSL")),
		StrTok(TEXT("a\rb"), TEXT(R"DSL("a\rb")DSL")),
		StrTok(TEXT("a\tb"), TEXT(R"DSL("a\tb")DSL")),
		// `\0` resolves to nothing: FString::AppendChar drops a NUL, and no FString can carry one.
		StrTok(TEXT("ab"), TEXT(R"DSL("a\0b")DSL")),
		// The backslash is KEPT on an unknown escape so the value still shows what was written.
		StrTok(TEXT("a\\qb"), TEXT(R"DSL("a\qb")DSL")),
		// An unterminated literal stops at the end of its line rather than swallowing the file.
		StrTok(TEXT("unterminated"), TEXT(R"DSL("unterminated)DSL")),
		Ident(TEXT("next")),
		EndTok()
	};

	if (!CheckTokens(*this, Fixture, Expected))
	{
		return false;
	}

	TestFalse(TEXT("the bad strings make the lex fail"), Fixture.bLexSucceeded);
	TestEqualSensitive(TEXT("codes raised, in order"), Fixture.AllCodes(), FString(TEXT("DSH2104,DSH2103")));

	// DSH2104 underlines the two characters of the escape.
	if (const FLangDiagnostic* UnknownEscape = Fixture.DiagnosticAt(0))
	{
		TestEqualSensitive(TEXT("DSH2104 code"), UnknownEscape->Code, FString(TEXT("DSH2104")));
		TestEqual(TEXT("DSH2104 span length"), UnknownEscape->Span.Length, 2);
		TestEqual(TEXT("DSH2104 line"), UnknownEscape->Span.Line, 7);
		TestEqualSensitive(TEXT("DSH2104 underlines the escape"),
			Fixture.Source.Slice(UnknownEscape->Span), FString(TEXT("\\q")));
	}

	// DSH2103 points at the OPENING quote, which is where the fix goes.
	if (const FLangDiagnostic* Unterminated = Fixture.DiagnosticAt(1))
	{
		TestEqualSensitive(TEXT("DSH2103 code"), Unterminated->Code, FString(TEXT("DSH2103")));
		TestEqual(TEXT("DSH2103 span length"), Unterminated->Span.Length, 1);
		TestEqual(TEXT("DSH2103 line"), Unterminated->Span.Line, 8);
		TestEqual(TEXT("DSH2103 column"), Unterminated->Span.Column, 1);
		TestEqualSensitive(TEXT("DSH2103 underlines the opening quote"),
			Fixture.Source.Slice(Unterminated->Span), FString(TEXT("\"")));
	}

	// An empty string is a string.
	{
		const FLexFixture Empty(FString(TEXT(R"DSL("")DSL")));
		const TArray<FExpectedToken> EmptyExpected = { StrTok(TEXT(""), TEXT(R"DSL("")DSL")), EndTok() };
		CheckTokens(*this, Empty, EmptyExpected);
		TestTrue(TEXT("an empty string lexes cleanly"), Empty.bLexSucceeded);
	}

	return true;
}

// =================================================================================================
// Every lexer error code, and the promise that goes with each of them: the lexer never gives up.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerErrorRecoveryTest,
	"DreamShader.Lang2.Lexer.ErrorRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerErrorRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	// ---- DSH2101 unknown character ------------------------------------------------------------
	{
		const FLexFixture Fixture(FString(TEXT("int a = @ 1;")));

		const TArray<FExpectedToken> Expected = {
			Ident(TEXT("int")),
			Ident(TEXT("a")),
			Punct(ELangTokenKind::Assign),
			UnknownTok(TEXT("@")),
			IntTok(TEXT("1")),
			Punct(ELangTokenKind::Semicolon),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
		TestFalse(TEXT("DSH2101: lexing reports failure"), Fixture.bLexSucceeded);
		TestEqualSensitive(TEXT("DSH2101: codes"), Fixture.AllCodes(), FString(TEXT("DSH2101")));
		if (const FLangDiagnostic* Diagnostic = Fixture.DiagnosticAt(0))
		{
			TestEqual(TEXT("DSH2101: span length"), Diagnostic->Span.Length, 1);
			TestEqual(TEXT("DSH2101: column"), Diagnostic->Span.Column, 9);
		}
	}

	// ---- DSH2102 unterminated block comment ---------------------------------------------------
	{
		const FLexFixture Fixture(JoinLines({ TEXT("int a;"), TEXT("/* open"), TEXT("still inside") }));

		const TArray<FExpectedToken> Expected = {
			Ident(TEXT("int")),
			Ident(TEXT("a")),
			Punct(ELangTokenKind::Semicolon),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
		TestFalse(TEXT("DSH2102: lexing reports failure"), Fixture.bLexSucceeded);
		TestEqualSensitive(TEXT("DSH2102: codes"), Fixture.AllCodes(), FString(TEXT("DSH2102")));
		if (const FLangDiagnostic* Diagnostic = Fixture.DiagnosticAt(0))
		{
			// Reported at the OPENING `/*`, not at the end of the file.
			TestEqual(TEXT("DSH2102: line"), Diagnostic->Span.Line, 2);
			TestEqual(TEXT("DSH2102: column"), Diagnostic->Span.Column, 1);
			TestEqual(TEXT("DSH2102: span length"), Diagnostic->Span.Length, 2);
			TestEqualSensitive(TEXT("DSH2102: underlines the opener"),
				Fixture.Source.Slice(Diagnostic->Span), FString(TEXT("/*")));
		}
	}

	// A CLOSED block comment, including a nested-looking one, is fine and produces nothing.
	{
		const FLexFixture Fixture(FString(TEXT("a /* x /* y */ b")));
		const TArray<FExpectedToken> Expected = { Ident(TEXT("a")), Ident(TEXT("b")), EndTok() };
		CheckTokens(*this, Fixture, Expected);
		TestTrue(TEXT("a closed block comment lexes cleanly"), Fixture.bLexSucceeded);
	}

	// ---- DSH2103 unterminated string ----------------------------------------------------------
	{
		const FLexFixture Fixture(JoinLines({ TEXT(R"DSL("oops)DSL"), TEXT("int b;") }));

		const TArray<FExpectedToken> Expected = {
			StrTok(TEXT("oops"), TEXT(R"DSL("oops)DSL")),
			Ident(TEXT("int")),
			Ident(TEXT("b")),
			Punct(ELangTokenKind::Semicolon),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
		TestEqualSensitive(TEXT("DSH2103: codes"), Fixture.AllCodes(), FString(TEXT("DSH2103")));
	}

	// ---- DSH2104 unknown escape ---------------------------------------------------------------
	{
		const FLexFixture Fixture(FString(TEXT(R"DSL("a\qb" 7)DSL")));

		const TArray<FExpectedToken> Expected = {
			StrTok(TEXT("a\\qb"), TEXT(R"DSL("a\qb")DSL")),
			IntTok(TEXT("7")),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
		TestEqualSensitive(TEXT("DSH2104: codes"), Fixture.AllCodes(), FString(TEXT("DSH2104")));
		TestEqualSensitive(TEXT("DSH2104: first error code"), Fixture.FirstErrorCode(), FString(TEXT("DSH2104")));
	}

	// ---- DSH2105 malformed number -------------------------------------------------------------
	{
		// Every shape the lexer calls malformed, then a good number to prove the cursor is still
		// aligned: a malformed literal is ONE token, never a split that the parser could mistake for
		// a plausible expression.
		const FLexFixture Fixture(FString(TEXT("1.2.3 0x 1e 1fu 1.0u 0x1h 2abc 9")));

		const TArray<FExpectedToken> Expected = {
			FloatTok(TEXT("1.2.3")),
			IntTok(TEXT("0x")),
			FloatTok(TEXT("1e")),
			FloatTok(TEXT("1fu")),
			FloatTok(TEXT("1.0u")),
			IntTok(TEXT("0x1h")),
			IntTok(TEXT("2abc")),
			IntTok(TEXT("9")),
			EndTok()
		};

		CheckTokens(*this, Fixture, Expected);
		TestFalse(TEXT("DSH2105: lexing reports failure"), Fixture.bLexSucceeded);
		TestEqual(TEXT("DSH2105: one error per malformed literal"), Fixture.Diagnostics.NumErrors(), 7);
		TestEqualSensitive(TEXT("DSH2105: codes"), Fixture.AllCodes(),
			FString(TEXT("DSH2105,DSH2105,DSH2105,DSH2105,DSH2105,DSH2105,DSH2105")));

		// The good literal after seven bad ones is still a plain integer.
		TestEqual(TEXT("DSH2105: the following literal is intact"),
			static_cast<int64>(Fixture.Tokens[7].Integer), static_cast<int64>(9));
		TestFalse(TEXT("DSH2105: the following literal is not unsigned"), Fixture.Tokens[7].bUnsigned);
	}

	// ---- DSH2106 `#` that is not at line start ------------------------------------------------
	{
		const FLexFixture Fixture(FString(TEXT("int a = 1; # bad")));

		TestEqualSensitive(TEXT("DSH2106: codes"), Fixture.AllCodes(), FString(TEXT("DSH2106")));
		TestEqualSensitive(TEXT("DSH2106: first error code"), Fixture.FirstErrorCode(), FString(TEXT("DSH2106")));

		if (TestEqual(TEXT("DSH2106: token count"), Fixture.Num(), 8))
		{
			// Recovery: the word after the stray `#` is still an ordinary identifier.
			TestEqualSensitive(TEXT("DSH2106: the token after the # is `bad`"),
				FString(LexToString(Fixture.Tokens[6].Kind)), FString(LexToString(ELangTokenKind::Identifier)));
			TestEqualSensitive(TEXT("DSH2106: recovered identifier text"), Fixture.Tokens[6].Text, FString(TEXT("bad")));
		}
	}

	// ---- several different errors in one file, all recovered from ------------------------------
	{
		const FLexFixture Fixture(JoinLines({
			TEXT("int a = @;"),
			TEXT("float b = 1.2.3;"),
			TEXT(R"DSL(string c = "x\qy";)DSL"),
			TEXT("int d = 4;")
		}));

		TestEqualSensitive(TEXT("mixed: codes in raise order"), Fixture.AllCodes(),
			FString(TEXT("DSH2101,DSH2105,DSH2104")));
		TestEqualSensitive(TEXT("mixed: first error"), Fixture.FirstErrorCode(), FString(TEXT("DSH2101")));
		TestEqual(TEXT("mixed: error count"), Fixture.Diagnostics.NumErrors(), 3);

		// The last declaration, after three separate mistakes, is lexed exactly as if nothing had
		// happened -- the whole point of never giving up.
		const int32 Count = Fixture.Num();
		if (TestTrue(TEXT("mixed: enough tokens to check the tail"), Count >= 6))
		{
			TestEqualSensitive(TEXT("mixed: tail `int`"), Fixture.Tokens[Count - 6].Text, FString(TEXT("int")));
			TestEqualSensitive(TEXT("mixed: tail `d`"), Fixture.Tokens[Count - 5].Text, FString(TEXT("d")));
			TestEqualSensitive(TEXT("mixed: tail `4`"), Fixture.Tokens[Count - 3].Text, FString(TEXT("4")));
			TestEqual(TEXT("mixed: tail `4` value"),
				static_cast<int64>(Fixture.Tokens[Count - 3].Integer), static_cast<int64>(4));
			CheckLineColumn(*this, TEXT("mixed: tail `int`"), Fixture.Tokens[Count - 6].Span, 4, 1);
		}
	}

	return true;
}

// =================================================================================================
// bAtLineStart -- what makes a `///` block attach to a declaration and a `#` a directive.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerLineStartTest,
	"DreamShader.Lang2.Lexer.LineStartFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerLineStartTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	const FLexFixture Fixture(JoinLines({
		TEXT("int a;"),
		TEXT("   b"),          // leading whitespace does not clear the flag
		TEXT("/* c */ d"),     // a comment DOES: something has been seen on this line
		TEXT("// e"),
		TEXT("f"),             // ... but the flag is restored by the newline after the comment
		TEXT("/// g"),
		TEXT("h")
	}));

	const TArray<FExpectedToken> Expected = {
		Ident(TEXT("int")),
		Ident(TEXT("a")),
		Punct(ELangTokenKind::Semicolon),
		Ident(TEXT("b")),
		Ident(TEXT("d")),
		Ident(TEXT("f")),
		DocTok(TEXT("g"), TEXT("/// g")),
		Ident(TEXT("h")),
		EndTok()
	};

	if (!CheckTokens(*this, Fixture, Expected))
	{
		return false;
	}

	const bool ExpectedFlags[] = { true, false, false, true, false, true, true, true, false };
	if (TestEqual(TEXT("the flag table covers every token"), Fixture.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedFlags))))
	{
		for (int32 Index = 0; Index < Fixture.Num(); ++Index)
		{
			CheckFlag(*this, *FString::Printf(TEXT("token %d bAtLineStart"), Index),
				Fixture.Tokens[Index].bAtLineStart, ExpectedFlags[Index]);
		}
	}

	// The EndOfFile flag follows the same rule: false above (the text ends right after `h`), true
	// when the text ends with a terminator.
	{
		const FLexFixture Terminated(FString(TEXT("h\n")));
		TestTrue(TEXT("EndOfFile after a terminator is at line start"), Terminated.Tokens.Last().bAtLineStart);
		CheckLineColumn(*this, TEXT("EndOfFile after a terminator"), Terminated.Tokens.Last().Span, 2, 1);
	}

	return true;
}

// =================================================================================================
// Keywords. Case-sensitive, and NOTHING else is reserved -- type names, `material`, `Substrate` and
// the legacy section words all have to arrive as identifiers or the grammar collapses.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerKeywordsTest,
	"DreamShader.Lang2.Lexer.Keywords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerKeywordsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	// Every member of ELangKeyword except None, in declaration order.
	const ELangKeyword AllKeywords[] = {
		ELangKeyword::Uniform, ELangKeyword::Static, ELangKeyword::Const, ELangKeyword::Extern, ELangKeyword::Export,
		ELangKeyword::In, ELangKeyword::Out, ELangKeyword::InOut,
		ELangKeyword::Struct,
		ELangKeyword::If, ELangKeyword::Else, ELangKeyword::For, ELangKeyword::While, ELangKeyword::Do,
		ELangKeyword::Return, ELangKeyword::Break, ELangKeyword::Continue, ELangKeyword::Discard,
		ELangKeyword::True, ELangKeyword::False,
		ELangKeyword::Import
	};

	// If this fails, ELangKeyword grew and this test (and the table in LangToken.cpp) has to grow
	// with it -- Import is the last enumerator, so its value is the number of real keywords.
	TestEqual(TEXT("every ELangKeyword member is covered"),
		static_cast<int32>(ELangKeyword::Import), static_cast<int32>(UE_ARRAY_COUNT(AllKeywords)));

	// LexToString(ELangKeyword) is the SOURCE spelling, and it must round-trip through
	// TryGetLangKeyword -- the two read the same table, and this is what pins the table to the enum.
	for (const ELangKeyword Keyword : AllKeywords)
	{
		const FString Spelling = FString(LexToString(Keyword));
		const FString What = FString(TEXT("keyword '")) + Spelling + TEXT("'");

		TestFalse(*(What + TEXT(" has a spelling")), Spelling.IsEmpty());
		TestTrue(*(What + TEXT(" is not the None placeholder")), !Spelling.Equals(TEXT("None"), ESearchCase::CaseSensitive));

		ELangKeyword RoundTripped = ELangKeyword::None;
		if (TestTrue(*(What + TEXT(" round-trips through TryGetLangKeyword")), TryGetLangKeyword(Spelling, RoundTripped)))
		{
			TestEqualSensitive(*(What + TEXT(" round-trips to itself")),
				FString(LexToString(RoundTripped)), Spelling);
		}
	}

	TestEqualSensitive(TEXT("None has no spelling"), FString(LexToString(ELangKeyword::None)), FString(TEXT("None")));
	TestEqualSensitive(TEXT("InOut spells `inout`"), FString(LexToString(ELangKeyword::InOut)), FString(TEXT("inout")));

	// All of them lexed from one line.
	{
		FString Text;
		TArray<FExpectedToken> Expected;
		for (const ELangKeyword Keyword : AllKeywords)
		{
			if (!Text.IsEmpty())
			{
				Text.Append(TEXT(" "));
			}
			Text.Append(LexToString(Keyword));
			Expected.Add(Word(Keyword));
		}
		Expected.Add(EndTok());

		const FLexFixture Fixture(Text);
		CheckTokens(*this, Fixture, Expected);
		TestTrue(TEXT("the keyword line lexes cleanly"), Fixture.bLexSucceeded);
	}

	// Case matters, and only these words are reserved. `Import` and `Material` are the two that
	// actually occur: the 1.x front end capitalises its section words, and a user type may be called
	// anything at all.
	{
		const FString Text = TEXT("Import IMPORT import Material material Struct STRUCT Uniform Const True False ")
			TEXT("Substrate UE void float float3 float4x4 Texture2D SamplerState _private x1 In Out InOut");

		const FLexFixture Fixture(Text);
		TestTrue(TEXT("the identifier line lexes cleanly"), Fixture.bLexSucceeded);

		// Only `import` in the middle is a keyword; every other word is an identifier.
		for (int32 Index = 0; Index < Fixture.Num() - 1; ++Index)
		{
			const FLangToken& Token = Fixture.Tokens[Index];
			const FString What = FString::Printf(TEXT("word %d ('%s')"), Index, *Token.Text);

			if (Index == 2)
			{
				TestTrue(*(What + TEXT(" is the `import` keyword")), Token.IsKeyword(ELangKeyword::Import));
			}
			else
			{
				TestEqualSensitive(*(What + TEXT(" kind")),
					FString(LexToString(Token.Kind)), FString(LexToString(ELangTokenKind::Identifier)));
				TestEqualSensitive(*(What + TEXT(" carries no keyword")),
					FString(LexToString(Token.Keyword)), FString(TEXT("None")));
			}
		}

		// FLangToken::IsIdentifier is case-sensitive by construction; prove it here so nobody
		// "simplifies" it into FString::operator==.
		TestTrue(TEXT("IsIdentifier matches exactly"), Fixture.Tokens[0].IsIdentifier(TEXT("Import")));
		TestFalse(TEXT("IsIdentifier does not match a different case"), Fixture.Tokens[0].IsIdentifier(TEXT("import")));
	}

	// TryGetLangKeyword directly, on the cases that would break if the table went case-insensitive.
	{
		ELangKeyword Keyword = ELangKeyword::Discard;

		TestFalse(TEXT("`Import` is not a keyword"), TryGetLangKeyword(FString(TEXT("Import")), Keyword));
		TestEqualSensitive(TEXT("`Import` clears the out parameter"), FString(LexToString(Keyword)), FString(TEXT("None")));

		TestFalse(TEXT("`IMPORT` is not a keyword"), TryGetLangKeyword(FString(TEXT("IMPORT")), Keyword));
		TestFalse(TEXT("`Struct` is not a keyword"), TryGetLangKeyword(FString(TEXT("Struct")), Keyword));
		TestFalse(TEXT("`material` is not a keyword"), TryGetLangKeyword(FString(TEXT("material")), Keyword));
		TestFalse(TEXT("`float3` is not a keyword"), TryGetLangKeyword(FString(TEXT("float3")), Keyword));
		TestFalse(TEXT("an empty string is not a keyword"), TryGetLangKeyword(FString(), Keyword));
		// A prefix of a keyword is not the keyword.
		TestFalse(TEXT("`im` is not a keyword"), TryGetLangKeyword(FString(TEXT("im")), Keyword));
		TestFalse(TEXT("`imports` is not a keyword"), TryGetLangKeyword(FString(TEXT("imports")), Keyword));

		TestTrue(TEXT("`import` is a keyword"), TryGetLangKeyword(FString(TEXT("import")), Keyword));
		TestEqualSensitive(TEXT("`import` maps to Import"), FString(LexToString(Keyword)), FString(TEXT("import")));
	}

	// Identifiers may start with an underscore and carry digits, but never start with one.
	{
		const FLexFixture Fixture(FString(TEXT("_a a_1 _ 1a")));
		TestEqualSensitive(TEXT("`_a` is an identifier"),
			FString(LexToString(Fixture.Tokens[0].Kind)), FString(LexToString(ELangTokenKind::Identifier)));
		TestEqualSensitive(TEXT("`a_1` is an identifier"),
			FString(LexToString(Fixture.Tokens[1].Kind)), FString(LexToString(ELangTokenKind::Identifier)));
		TestEqualSensitive(TEXT("`_` is an identifier"),
			FString(LexToString(Fixture.Tokens[2].Kind)), FString(LexToString(ELangTokenKind::Identifier)));
		// `1a` is a malformed NUMBER, not an identifier -- one token, DSH2105.
		TestEqualSensitive(TEXT("`1a` is a malformed number"),
			FString(LexToString(Fixture.Tokens[3].Kind)), FString(LexToString(ELangTokenKind::IntLiteral)));
		TestEqualSensitive(TEXT("`1a` raises DSH2105"), Fixture.FirstErrorCode(), FString(TEXT("DSH2105")));
	}

	return true;
}

// =================================================================================================
// The EndOfFile token, and the promise that the array always ends with one.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerEndOfFileTest,
	"DreamShader.Lang2.Lexer.EndOfFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerEndOfFileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	struct FEndCase
	{
		const TCHAR* What;
		const TCHAR* Text;
		int32 TokenCount;
		int32 Line;
		int32 Column;
		bool bAtLineStart;
	};

	const FEndCase Cases[] = {
		{ TEXT("empty text"),        TEXT(""),        1, 1, 1, true  },
		{ TEXT("whitespace only"),   TEXT("   \n\t\n"), 1, 3, 1, true  },
		{ TEXT("comment only"),      TEXT("// x"),    1, 1, 5, false },
		{ TEXT("block comment only"),TEXT("/* x */"), 1, 1, 8, false },
		{ TEXT("one identifier"),    TEXT("a"),       2, 1, 2, false },
		{ TEXT("trailing newline"),  TEXT("a\n"),     2, 2, 1, true  },
	};

	for (const FEndCase& Case : Cases)
	{
		const FString Text = FString(Case.Text);
		const FLexFixture Fixture(Text);
		const FString Prefix = FString(Case.What) + TEXT(": ");

		TestTrue(*(Prefix + TEXT("lexes cleanly")), Fixture.bLexSucceeded);
		TestEqual(*(Prefix + TEXT("no diagnostics")), Fixture.Diagnostics.Num(), 0);

		if (!TestEqual(*(Prefix + TEXT("token count")), Fixture.Num(), Case.TokenCount))
		{
			continue;
		}

		const FLangToken& End = Fixture.Tokens.Last();
		TestEqualSensitive(*(Prefix + TEXT("last token is EndOfFile")),
			FString(LexToString(End.Kind)), FString(LexToString(ELangTokenKind::EndOfFile)));
		TestEqual(*(Prefix + TEXT("EndOfFile offset")), End.Span.Offset, Text.Len());
		TestEqual(*(Prefix + TEXT("EndOfFile span is empty")), End.Span.Length, 0);
		TestEqualSensitive(*(Prefix + TEXT("EndOfFile has no text")), End.Text, FString(TEXT("")));
		CheckLineColumn(*this, *Prefix, End.Span, Case.Line, Case.Column);
		CheckFlag(*this, *(Prefix + TEXT("EndOfFile bAtLineStart")), End.bAtLineStart, Case.bAtLineStart);
	}

	// The output array is reset, not appended to: a caller that reuses one buffer for several files
	// must not inherit the previous file's tokens.
	{
		const FLangSourceText Source(FString(GFixturePath), FString(TEXT("a")));
		FLangDiagnosticSink Diagnostics{ FString(GFixturePath) };
		const FLangLexOptions Options;

		TArray<FLangToken> Tokens;
		FLangToken Stale;
		Stale.Kind = ELangTokenKind::Unknown;
		Tokens.Add(Stale);
		Tokens.Add(Stale);
		Tokens.Add(Stale);

		LexDreamShaderLang(Source, Options, Tokens, Diagnostics);

		TestEqual(TEXT("a reused token array is reset"), Tokens.Num(), 2);
		if (Tokens.Num() == 2)
		{
			TestEqualSensitive(TEXT("the reused array holds the new tokens"),
				FString(LexToString(Tokens[0].Kind)), FString(LexToString(ELangTokenKind::Identifier)));
		}
	}

	// The return value reports what THIS call added, not what the shared sink already held.
	{
		const FLangSourceText Source(FString(GFixturePath), FString(TEXT("a")));
		FLangDiagnosticSink Diagnostics{ FString(GFixturePath) };
		Diagnostics.Error(TEXT("DSH9999"), Source.MakeSpan(0, 1), FText::FromString(TEXT("from an earlier stage")));

		TArray<FLangToken> Tokens;
		const FLangLexOptions Options;
		const bool bLexSucceeded = LexDreamShaderLang(Source, Options, Tokens, Diagnostics);

		TestTrue(TEXT("a clean lex succeeds even when the sink already had an error"), bLexSucceeded);
		TestTrue(TEXT("the pre-existing error is still there"), Diagnostics.HasErrors());
		TestEqual(TEXT("the pre-existing error was not duplicated"), Diagnostics.NumErrors(), 1);
	}

	return true;
}

// =================================================================================================
// FLangDiagnosticSink itself: counting, ordering, moving, and the invariant English wire form.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2LexerDiagnosticSinkTest,
	"DreamShader.Lang2.Lexer.DiagnosticSink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2LexerDiagnosticSinkTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangLexerTests;

	const FLangSourceText Source(FString(GFixturePath), FString(TEXT("abc")));
	const FLangSpan Span = Source.MakeSpan(1, 1);

	{
		FLangDiagnosticSink Sink{ FString(GFixturePath) };

		TestFalse(TEXT("a fresh sink has no errors"), Sink.HasErrors());
		TestNull(TEXT("a fresh sink has no first error"), Sink.FirstError());
		TestEqualSensitive(TEXT("the sink remembers its file"), Sink.GetFilePath(), FString(GFixturePath));

		// A warning and an info do not make the run fail.
		Sink.Warning(TEXT("DSH2107"), Span, FText::FromString(TEXT("a warning")));
		Sink.Info(TEXT("DSH2108"), Span, FText::FromString(TEXT("an info")));
		TestFalse(TEXT("warnings and infos are not errors"), Sink.HasErrors());
		TestEqual(TEXT("but they are diagnostics"), Sink.Num(), 2);
		TestEqual(TEXT("and the error count stays zero"), Sink.NumErrors(), 0);

		// Error() returns false so a raise site reads `return Sink.Error(...)`.
		const bool bReturned = Sink.Error(TEXT("DSH2101"), Span, FText::FromString(TEXT("an error")));
		TestFalse(TEXT("Error() returns false"), bReturned);
		TestTrue(TEXT("Error() sets HasErrors"), Sink.HasErrors());
		TestEqual(TEXT("Error() bumps NumErrors"), Sink.NumErrors(), 1);
		TestEqual(TEXT("Error() appends"), Sink.Num(), 3);

		// FirstError skips the warning and the info that came before it.
		if (const FLangDiagnostic* First = Sink.FirstError())
		{
			TestEqualSensitive(TEXT("FirstError finds the error, not the warning"), First->Code, FString(TEXT("DSH2101")));
			TestEqualSensitive(TEXT("a diagnostic carries the sink's file path"), First->FilePath, FString(GFixturePath));
			TestEqual(TEXT("a diagnostic carries its span"), First->Span.Offset, 1);
		}

		// Severity spellings are the lower-case wire form the 1.x extensions already match on.
		TestEqualSensitive(TEXT("error severity"), FString(LexToString(ELangSeverity::Error)), FString(TEXT("error")));
		TestEqualSensitive(TEXT("warning severity"), FString(LexToString(ELangSeverity::Warning)), FString(TEXT("warning")));
		TestEqualSensitive(TEXT("info severity"), FString(LexToString(ELangSeverity::Info)), FString(TEXT("info")));

		// Append moves everything over and leaves the source empty.
		FLangDiagnosticSink Target{ FString(TEXT("other.dss")) };
		Target.Error(TEXT("DSH2102"), Span, FText::FromString(TEXT("already here")));
		Target.Append(MoveTemp(Sink));

		TestEqual(TEXT("Append merges the counts"), Target.Num(), 4);
		TestEqual(TEXT("Append merges the error counts"), Target.NumErrors(), 2);
		TestEqual(TEXT("an appended sink is emptied"), Sink.Num(), 0);
		TestEqual(TEXT("an appended sink forgets its errors"), Sink.NumErrors(), 0);
		TestFalse(TEXT("an appended sink no longer reports errors"), Sink.HasErrors());
		if (const FLangDiagnostic* First = Target.FirstError())
		{
			TestEqualSensitive(TEXT("Append keeps the order"), First->Code, FString(TEXT("DSH2102")));
		}
	}

	// ToWireString: `DSHnnnn: <message>` with the message in its INVARIANT (English source) form.
	{
		FLangDiagnostic Plain;
		Plain.Code = TEXT("DSH2101");
		Plain.Message = FText::FromString(TEXT("Unexpected character '@' in source."));
		TestEqualSensitive(TEXT("a plain message goes onto the wire as written"),
			FLangDiagnosticSink::ToWireString(Plain),
			FString(TEXT("DSH2101: Unexpected character '@' in source.")));

		// A FORMATTED message is the interesting case: the display string of a format result is
		// localised, so ToWireString has to replay the pattern instead of reading it back.
		FFormatNamedArguments TextArguments;
		TextArguments.Add(TEXT("0"), FFormatArgumentValue(FText::FromString(TEXT("0x"))));

		FLangDiagnostic Formatted;
		Formatted.Code = TEXT("DSH2105");
		Formatted.Message = FText::Format(FTextFormat::FromString(TEXT("Malformed number literal '{0}'.")), TextArguments);
		TestEqualSensitive(TEXT("a formatted message is replayed onto the wire"),
			FLangDiagnosticSink::ToWireString(Formatted),
			FString(TEXT("DSH2105: Malformed number literal '0x'.")));

		// A NUMERIC argument must lose its grouping separators, or a five-digit offset reads as
		// "12,345" in en-US and something else again elsewhere.
		FFormatNamedArguments NumberArguments;
		NumberArguments.Add(TEXT("0"), FFormatArgumentValue(static_cast<int32>(12345)));

		FLangDiagnostic Numeric;
		Numeric.Code = TEXT("DSH2101");
		Numeric.Message = FText::Format(FTextFormat::FromString(TEXT("at offset {0}")), NumberArguments);
		TestEqualSensitive(TEXT("a numeric argument goes onto the wire ungrouped"),
			FLangDiagnosticSink::ToWireString(Numeric),
			FString(TEXT("DSH2101: at offset 12345")));
	}

	// The real thing, end to end: a lexer diagnostic's wire form starts with its code.
	{
		const FLexFixture Fixture(FString(TEXT("int a = @;")));
		if (const FLangDiagnostic* Diagnostic = Fixture.DiagnosticAt(0))
		{
			const FString Wire = FLangDiagnosticSink::ToWireString(*Diagnostic);
			TestTrue(TEXT("a lexer diagnostic's wire form starts with its code"),
				Wire.StartsWith(TEXT("DSH2101: "), ESearchCase::CaseSensitive));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
