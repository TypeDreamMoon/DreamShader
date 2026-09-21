// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.Trivia.* -- comments and blank lines through the front end and back out of the printer:
// Comment tokens, the attach pass behind FLangParseOptions::bKeepTrivia, FDocBlock::Order,
// declarations that share one statement, `#pragma instance`, and the printer reproducing all of it.
//
// `dsc migrate`, Adopt and the VirtualFunction sync all print a parsed file back, so "a comment survives" is not a
// nicety here: it is the DSH9092 invariant, and these tests are the smallest place it can fail.
//
// Core only.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Lang/LangLexer.h"
#include "Lang/LangToken.h"
// CollectDreamShaderComments: how the comment invariant reads a text.
#include "Migrate/LangMigrate.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::LangTriviaTests
{
	using namespace UE::DreamShader::Lang;

	inline FLangParseResult ParseWithTrivia(const TCHAR* FileName, const FString& Text)
	{
		FLangParseOptions Options;
		Options.bKeepTrivia = true;
		return ParseDreamShaderLang(FLangSourceText(FileName, Text), Options);
	}

	inline TArray<FString> ErrorsOf(const FLangParseResult& Result)
	{
		return UE::DreamShader::Editor::Private::Tests::GatherDreamShaderLangDiagnostics(Result.Diagnostics, ELangSeverity::Error);
	}

	inline bool HasLeading(const FLangTrivia* Trivia, const TCHAR* Text)
	{
		return Trivia && Trivia->Leading.ContainsByPredicate([Text](const FLangComment& Comment) { return Comment.Text.Equals(Text, ESearchCase::CaseSensitive); });
	}

	inline bool HasTrailing(const FLangTrivia* Trivia, const TCHAR* Text)
	{
		return Trivia && Trivia->Trailing.IsSet() && Trivia->Trailing->Text.Equals(Text, ESearchCase::CaseSensitive);
	}

	inline const FFunctionDecl* FindFunction(const FModule& Module, const TCHAR* Name)
	{
		for (const FDeclPtr& Decl : Module.Declarations)
		{
			const FFunctionDecl* Function = Decl.IsValid() ? Decl->As<FFunctionDecl>() : nullptr;
			if (Function && Function->Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Function;
			}
		}
		return nullptr;
	}

	inline const FVariableDecl* FindVariable(const FModule& Module, const TCHAR* Name)
	{
		for (const FDeclPtr& Decl : Module.Declarations)
		{
			const FVariableDecl* Variable = Decl.IsValid() ? Decl->As<FVariableDecl>() : nullptr;
			if (Variable && Variable->Declarator.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Variable;
			}
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------------------------------------
// The lexer's Comment tokens
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangTriviaLexerTest,
	"DreamShader.Lang2.Trivia.LexerComments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangTriviaLexerTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;

	const FLangSourceText Source(TEXT("Comments.dss"), TEXT(
		"// line\n"
		"//// ruler ////\n"
		"/* block */\n"
		"#pragma region R // trailing\n"
		"/// doc\n"));

	// With comments: every one is a token, delimiters included, and the one behind a directive is its own token.
	{
		FLangLexOptions Options;
		Options.bEmitComments = true;
		TArray<FLangToken> Tokens;
		FLangDiagnosticSink Sink;
		LexDreamShaderLang(Source, Options, Tokens, Sink);

		TArray<FString> Kinds;
		TArray<FString> Comments;
		for (const FLangToken& Token : Tokens)
		{
			Kinds.Add(LexToString(Token.Kind));
			if (Token.Kind == ELangTokenKind::Comment)
			{
				Comments.Add(Token.Text);
			}
		}
		TestEqual(FString::Printf(TEXT("four comment tokens (kinds: %s)"), *FString::Join(Kinds, TEXT(" "))), Comments.Num(), 4);
		TestTrue(TEXT("a line comment, verbatim"), Comments.Contains(TEXT("// line")));
		TestTrue(TEXT("`////` is a comment, not documentation"), Comments.Contains(TEXT("//// ruler ////")));
		TestTrue(TEXT("a block comment, verbatim"), Comments.Contains(TEXT("/* block */")));
		TestTrue(TEXT("the comment behind a directive"), Comments.Contains(TEXT("// trailing")));

		const FLangToken* Directive = Tokens.FindByPredicate([](const FLangToken& Token) { return Token.Kind == ELangTokenKind::Directive; });
		if (TestNotNull(TEXT("the directive token"), Directive))
		{
			TestTrue(FString::Printf(TEXT("the directive's text ends before its comment ('%s')"), *Directive->Text), Directive->Text.Equals(TEXT("pragma region R"), ESearchCase::CaseSensitive));
		}
		TestTrue(TEXT("`///` is still a DocComment"), Tokens.ContainsByPredicate([](const FLangToken& Token) { return Token.Kind == ELangTokenKind::DocComment; }));
	}

	// Doc comments off, comments on: a `///` line is then just a comment.
	{
		FLangLexOptions Options;
		Options.bEmitComments = true;
		Options.bEmitDocComments = false;
		TArray<FLangToken> Tokens;
		FLangDiagnosticSink Sink;
		LexDreamShaderLang(Source, Options, Tokens, Sink);
		TestTrue(TEXT("`/// doc` as a Comment token"), Tokens.ContainsByPredicate([](const FLangToken& Token)
		{
			return Token.Kind == ELangTokenKind::Comment && Token.Text.Equals(TEXT("/// doc"), ESearchCase::CaseSensitive);
		}));
		TestFalse(TEXT("and no DocComment token"), Tokens.ContainsByPredicate([](const FLangToken& Token) { return Token.Kind == ELangTokenKind::DocComment; }));
	}

	// Without comments the stream is what it always was.
	{
		TArray<FLangToken> Tokens;
		FLangDiagnosticSink Sink;
		LexDreamShaderLang(Source, FLangLexOptions(), Tokens, Sink);
		TestFalse(TEXT("no Comment token by default"), Tokens.ContainsByPredicate([](const FLangToken& Token) { return Token.Kind == ELangTokenKind::Comment; }));
		TestEqual(TEXT("Directive, DocComment, EndOfFile"), Tokens.Num(), 3);
	}

	// An unterminated block comment is still a token: the text to the end of the file is what the author wrote.
	{
		FLangLexOptions Options;
		Options.bEmitComments = true;
		TArray<FLangToken> Tokens;
		FLangDiagnosticSink Sink;
		LexDreamShaderLang(FLangSourceText(TEXT("Open.dss"), TEXT("uniform float A = 1;\n/* never closed\n")), Options, Tokens, Sink);
		TestTrue(TEXT("DSH2102"), Sink.GetDiagnostics().ContainsByPredicate([](const FLangDiagnostic& Diagnostic) { return Diagnostic.Code == TEXT("DSH2102"); }));
		TestTrue(TEXT("and the comment token"), Tokens.ContainsByPredicate([](const FLangToken& Token)
		{
			return Token.Kind == ELangTokenKind::Comment && Token.Text.StartsWith(TEXT("/* never closed"));
		}));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The attach pass
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangTriviaAttachTest,
	"DreamShader.Lang2.Trivia.Attach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangTriviaAttachTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangTriviaTests;

	const FLangParseResult Parsed = ParseWithTrivia(TEXT("Attach.dss"), TEXT(
		"// above A\n"
		"uniform float A = 1; // after A\n"
		"\n"
		"\n"
		"\n"
		"// above entry\n"
		"export void M_Attach(inout material m /* in the list */)\n"
		"{\n"
		"    // above X\n"
		"    float X = A; // after X\n"
		"\n"
		"    m.Opacity = X;\n"
		"    // inner tail\n"
		"}\n"
		"// module tail\n"));
	if (!TestTrue(FString::Printf(TEXT("the source parses (%s)"), *FString::Join(ErrorsOf(Parsed), TEXT(" | "))), Parsed.Succeeded()))
	{
		return false;
	}
	const FModule& Module = *Parsed.Module;

	const FVariableDecl* A = FindVariable(Module, TEXT("A"));
	const FFunctionDecl* Entry = FindFunction(Module, TEXT("M_Attach"));
	if (!TestNotNull(TEXT("uniform A"), A) || !TestNotNull(TEXT("the entry"), Entry) || !TestTrue(TEXT("the entry has a body"), Entry->Body.IsValid()))
	{
		return false;
	}

	TestTrue(TEXT("an own-line comment above a declaration is its Leading"), HasLeading(Module.Trivia.Find(A), TEXT("// above A")));
	TestTrue(TEXT("a comment after the `;` on the same line is its Trailing"), HasTrailing(Module.Trivia.Find(A), TEXT("// after A")));

	const FLangTrivia* EntryTrivia = Module.Trivia.Find(Entry);
	TestTrue(TEXT("the entry's Leading"), HasLeading(EntryTrivia, TEXT("// above entry")));
	TestTrue(TEXT("a comment inside the parameter list moves to the function's Leading"), HasLeading(EntryTrivia, TEXT("/* in the list */")));
	// Three blank lines above the comment run: counted, and the count is of the gap ABOVE the comments.
	TestEqual(TEXT("BlankLinesBefore counts the gap above the comments"), EntryTrivia ? EntryTrivia->BlankLinesBefore : -1, 3);

	if (TestEqual(TEXT("two statements"), Entry->Body->Statements.Num(), 2))
	{
		const FLangTrivia* First = Module.Trivia.Find(Entry->Body->Statements[0].Get());
		const FLangTrivia* Second = Module.Trivia.Find(Entry->Body->Statements[1].Get());
		TestTrue(TEXT("a statement's Leading"), HasLeading(First, TEXT("// above X")));
		TestTrue(TEXT("a statement's Trailing"), HasTrailing(First, TEXT("// after X")));
		TestEqual(TEXT("one blank line above the second statement"), Second ? Second->BlankLinesBefore : -1, 1);
	}

	const FLangTrivia* BodyTrivia = Module.Trivia.Find(Entry->Body.Get());
	TestTrue(TEXT("a comment after the last statement is the block's Inner"), BodyTrivia && BodyTrivia->Inner.ContainsByPredicate([](const FLangComment& Comment) { return Comment.Text == TEXT("// inner tail"); }));
	TestTrue(TEXT("a comment after the last declaration is the module's"), Module.TrailingComments.ContainsByPredicate([](const FLangComment& Comment) { return Comment.Text == TEXT("// module tail"); }));

	// Without the option there is no side table at all.
	const FLangParseResult Plain = ParseDreamShaderLang(FLangSourceText(TEXT("Attach.dss"), TEXT("// c\nuniform float A = 1;\n")), FLangParseOptions());
	TestTrue(TEXT("no trivia unless asked"), Plain.Module.IsValid() && Plain.Module->Trivia.Num() == 0 && Plain.Module->TrailingComments.Num() == 0);
	return true;
}

// ---------------------------------------------------------------------------------------------
// The printer
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangTriviaPrintTest,
	"DreamShader.Lang2.Trivia.Print",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangTriviaPrintTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangTriviaTests;

	const FString Source = TEXT(
		"// above A\n"
		"uniform float A = 1; // after A\n"
		"\n"
		"\n"
		"\n"
		"/* a block\n"
		"   over two lines */\n"
		"export void M_Print(inout material m)\n"
		"{\n"
		"    // above X\n"
		"    float X = A; // after X\n"
		"\n"
		"    m.Opacity = X;\n"
		"    // inner tail\n"
		"}\n"
		"// module tail\n");

	const FLangParseResult Parsed = ParseWithTrivia(TEXT("Print.dss"), Source);
	if (!TestTrue(TEXT("the source parses"), Parsed.Succeeded()))
	{
		return false;
	}

	const FString Printed = PrintDreamShaderLang(*Parsed.Module);
	for (const TCHAR* Comment : { TEXT("// above A"), TEXT("// after A"), TEXT("/* a block"), TEXT("   over two lines */"), TEXT("// above X"), TEXT("// after X"), TEXT("// inner tail"), TEXT("// module tail") })
	{
		TestTrue(FString::Printf(TEXT("'%s' is printed\n%s"), Comment, *Printed), Printed.Contains(Comment, ESearchCase::CaseSensitive));
	}
	TestTrue(TEXT("a trailing comment stays on its line"), Printed.Contains(TEXT("uniform float A = 1; // after A")));
	TestFalse(TEXT("three blank lines print as one"), Printed.Contains(TEXT("\n\n\n")));
	TestTrue(TEXT("a blank line between statements is kept"), Printed.Contains(TEXT("// after X\n\n")));

	// A fixed point after one print.
	const FLangParseResult Again = ParseWithTrivia(TEXT("Print.dss"), Printed);
	if (TestTrue(FString::Printf(TEXT("the printed text parses (%s)"), *FString::Join(ErrorsOf(Again), TEXT(" | "))), Again.Succeeded()))
	{
		const FString PrintedAgain = PrintDreamShaderLang(*Again.Module);
		TestTrue(
			FString::Printf(TEXT("print(parse(print(parse(X)))) == print(parse(X)): %s"),
				*UE::DreamShader::Editor::Private::Tests::DescribeDreamShaderTextDifference(PrintedAgain, Printed)),
			PrintedAgain.Equals(Printed, ESearchCase::CaseSensitive));
	}

	// bPrintTrivia off: the canonical layout, as if the parse had kept nothing.
	{
		FLangPrintOptions Options;
		Options.bPrintTrivia = false;
		const FString Bare = PrintDreamShaderLang(*Parsed.Module, Options);
		const FLangParseResult Plain = ParseDreamShaderLang(FLangSourceText(TEXT("Print.dss"), Source), FLangParseOptions());
		TestFalse(TEXT("no comment without bPrintTrivia"), Bare.Contains(TEXT("// above A")));
		TestTrue(TEXT("and the text is the one a parse without trivia prints"), Plain.Module.IsValid() && Bare.Equals(PrintDreamShaderLang(*Plain.Module), ESearchCase::CaseSensitive));
	}

	// Another line ending: a block comment's own line breaks follow it.
	{
		FLangPrintOptions Options;
		Options.NewLine = TEXT("\r\n");
		const FString Crlf = PrintDreamShaderLang(*Parsed.Module, Options);
		FString WithoutCrlf = Crlf;
		WithoutCrlf.ReplaceInline(TEXT("\r\n"), TEXT(""));
		TestFalse(TEXT("no bare LF is left under NewLine = CRLF"), WithoutCrlf.Contains(TEXT("\n")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Doc blocks in their written order; declarations that share a statement; `#pragma instance`
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangTriviaDeclarationsTest,
	"DreamShader.Lang2.Trivia.Declarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangTriviaDeclarationsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::LangTriviaTests;

	// FDocBlock::Order.
	{
		const FString Source = TEXT(
			"/// The gain @group Look @sort 3\n"
			"/// second line\n"
			"uniform float Gain = 1;\n");
		const FLangParseResult Parsed = ParseWithTrivia(TEXT("Doc.dss"), Source);
		const FVariableDecl* Gain = Parsed.Module.IsValid() ? FindVariable(*Parsed.Module, TEXT("Gain")) : nullptr;
		if (TestNotNull(TEXT("uniform Gain"), Gain))
		{
			const FDocBlock& Doc = Gain->Doc;
			TestEqual(TEXT("two free-text lines"), Doc.FreeText.Num(), 2);
			TestEqual(TEXT("two directives"), Doc.Directives.Num(), 2);
			if (TestEqual(TEXT("four items in written order"), Doc.Order.Num(), 4))
			{
				TestTrue(TEXT("item 0: free text 0 on line 0"), Doc.Order[0].Kind == FDocItem::EKind::FreeText && Doc.Order[0].Index == 0 && Doc.Order[0].Line == 0);
				TestTrue(TEXT("item 1: directive 0 on line 0"), Doc.Order[1].Kind == FDocItem::EKind::Directive && Doc.Order[1].Index == 0 && Doc.Order[1].Line == 0);
				TestTrue(TEXT("item 2: directive 1 on line 0"), Doc.Order[2].Kind == FDocItem::EKind::Directive && Doc.Order[2].Index == 1 && Doc.Order[2].Line == 0);
				TestTrue(TEXT("item 3: free text 1 on line 1"), Doc.Order[3].Kind == FDocItem::EKind::FreeText && Doc.Order[3].Index == 1 && Doc.Order[3].Line == 1);
			}

			const FString Printed = PrintDreamShaderLang(*Parsed.Module);
			TestTrue(FString::Printf(TEXT("the first line keeps its three pieces together\n%s"), *Printed), Printed.Contains(TEXT("/// The gain")) && Printed.Contains(TEXT("@group Look")) && Printed.Contains(TEXT("@sort 3")));
			const int32 FirstLineEnd = Printed.Find(TEXT("\n"));
			TestTrue(TEXT("on ONE line"), FirstLineEnd != INDEX_NONE && Printed.Left(FirstLineEnd).Contains(TEXT("@sort 3")) && Printed.Left(FirstLineEnd).Contains(TEXT("The gain")));
		}
	}

	// `uniform float a, b = 2, c;`
	{
		const FLangParseResult Parsed = ParseWithTrivia(TEXT("Grouped.dss"), TEXT("uniform float a, b = 2, c;\nuniform float d = 4;\n"));
		if (TestTrue(TEXT("grouped declarators parse"), Parsed.Succeeded()))
		{
			const FVariableDecl* VarA = FindVariable(*Parsed.Module, TEXT("a"));
			const FVariableDecl* VarB = FindVariable(*Parsed.Module, TEXT("b"));
			const FVariableDecl* VarC = FindVariable(*Parsed.Module, TEXT("c"));
			const FVariableDecl* VarD = FindVariable(*Parsed.Module, TEXT("d"));
			if (TestTrue(TEXT("one declaration per name"), VarA && VarB && VarC && VarD))
			{
				TestFalse(TEXT("`a` opens the statement"), VarA->bSharesDeclarationWithPrevious);
				TestTrue(TEXT("`b` shares it"), VarB->bSharesDeclarationWithPrevious);
				TestTrue(TEXT("`c` shares it"), VarC->bSharesDeclarationWithPrevious);
				TestFalse(TEXT("`d` is a statement of its own"), VarD->bSharesDeclarationWithPrevious);
			}
			const FString Printed = PrintDreamShaderLang(*Parsed.Module);
			TestTrue(FString::Printf(TEXT("printed back as one statement\n%s"), *Printed), Printed.Contains(TEXT("uniform float a, b = 2, c;")));
			TestTrue(TEXT("and the unrelated declaration is not joined to it"), Printed.Contains(TEXT("uniform float d = 4;")));
		}
	}

	// `#pragma instance`.
	{
		const FLangParseResult Parsed = ParseWithTrivia(TEXT("Instance.dsi"), TEXT("#pragma instance(Parent = \"/Game/M\", BlendMode = Translucent) // kept\nuniform float Gain = 2.0;\n"));
		if (TestTrue(FString::Printf(TEXT("a `.dsi` parses (%s)"), *FString::Join(ErrorsOf(Parsed), TEXT(" | "))), Parsed.Succeeded()))
		{
			const FPragmaDecl* Pragma = Parsed.Module->Declarations.Num() > 0 ? Parsed.Module->Declarations[0]->As<FPragmaDecl>() : nullptr;
			if (TestNotNull(TEXT("the pragma"), Pragma))
			{
				TestTrue(TEXT("EPragmaKind::Instance"), Pragma->PragmaKind == EPragmaKind::Instance);
				if (TestEqual(TEXT("two keyed arguments"), Pragma->Arguments.Num(), 2))
				{
					TestTrue(TEXT("Parent is quoted"), Pragma->Arguments[0].Key == TEXT("Parent") && Pragma->Arguments[0].bQuoted && Pragma->Arguments[0].Value == TEXT("/Game/M"));
					TestTrue(TEXT("BlendMode is bare"), Pragma->Arguments[1].Key == TEXT("BlendMode") && !Pragma->Arguments[1].bQuoted && Pragma->Arguments[1].Value == TEXT("Translucent"));
				}
			}
			const FString Printed = PrintDreamShaderLang(*Parsed.Module);
			TestTrue(FString::Printf(TEXT("the pragma prints back with its comment\n%s"), *Printed), Printed.Contains(TEXT("#pragma instance(Parent = \"/Game/M\", BlendMode = Translucent) // kept")));
		}

		const FLangParseResult Positional = ParseWithTrivia(TEXT("Bad.dsi"), TEXT("#pragma instance(Foo)\n"));
		TestTrue(TEXT("a positional argument is DSH3202"), ErrorsOf(Positional).ContainsByPredicate([](const FString& Line) { return Line.StartsWith(TEXT("DSH3202")); }));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The comment invariant over the corpora
// ---------------------------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FDreamShaderLangCommentInvariantTest,
	"DreamShader.Lang2.Trivia.CommentInvariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderLangCommentInvariantTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Every fixture that parses, in both languages: `.dss` / `.dsh` of the 2.0 corpora and the 1.x ones of Parse and
	// Legacy/Parse. One sub-test per corpus directory keeps the report readable at a few hundred files.
	for (const TCHAR* SubDir : { TEXT("Lang"), TEXT("IR"), TEXT("Compile"), TEXT("Parse"), TEXT("Legacy/Parse"), TEXT("Migrate") })
	{
		OutBeautifiedNames.Add(FString(SubDir).Replace(TEXT("/"), TEXT(".")));
		OutTestCommands.Add(SubDir);
	}
}

bool FDreamShaderLangCommentInvariantTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::LangTriviaTests;

	TArray<FCorpusCase> Cases;
	LoadDreamShaderCorpusCases(Parameters, Cases);

	int32 Checked = 0;
	for (const FCorpusCase& Case : Cases)
	{
		FString Source;
		if (Case.bBadByName || !FFileHelper::LoadFileToString(Source, *Case.SourcePath))
		{
			continue;
		}
		const FLangParseResult Parsed = ParseWithTrivia(*Case.SourcePath, Source);
		if (!Parsed.Succeeded())
		{
			// A fixture that does not parse is some other test's subject.
			continue;
		}
		++Checked;

		// Every comment of the source is in what its tree prints: in a Trivia entry, among the module's trailing
		// comments, or inside an opaque body that is printed verbatim. (A legacy source prints as 2.0 text, so the
		// texts are compared as CollectDreamShaderComments reads them: without their fences.)
		const FString Printed = PrintDreamShaderLang(*Parsed.Module);
		const FString PrintedPath = MakeDreamShaderCorpusCase(Case.SourcePath).SourcePath + TEXT(".printed.dss");

		TArray<FString> Before;
		TArray<FString> After;
		CollectDreamShaderComments(FLangSourceText(Case.SourcePath, Source), /*bIncludeDocComments*/ true, Before);
		CollectDreamShaderComments(FLangSourceText(PrintedPath, Printed), /*bIncludeDocComments*/ true, After);

		TArray<FString> Lost;
		for (const FString& Comment : Before)
		{
			const int32 Found = After.IndexOfByPredicate([&Comment](const FString& Candidate) { return Candidate.Equals(Comment, ESearchCase::CaseSensitive); });
			if (Found == INDEX_NONE)
			{
				Lost.Add(Comment);
			}
			else
			{
				After.RemoveAtSwap(Found);
			}
		}
		if (Lost.Num() > 0)
		{
			AddError(FString::Printf(TEXT("[%s] %d comment(s) are not in the printed text: %s"), *Case.SourcePath, Lost.Num(), *FString::Join(Lost, TEXT(" | "))));
		}
	}

	AddInfo(FString::Printf(TEXT("%s: %d fixture(s) checked."), *Parameters, Checked));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
