// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Contract tests for the conditional-compilation preprocessor: PreprocessDreamShaderSource,
// DreamShaderSourceHasPreprocessorDirectives, BuildDreamShaderDefineKeyFragment, and the parts of
// FDreamShaderDefineTable that can be exercised without the process-global registry.
//
// These are written against the FROZEN headers (Public/DreamShaderPreprocessor.h,
// Public/DreamShaderDefineTable.h) and Plan/preprocessor-conditionals.md -- deliberately not
// against the implementation. Everything asserted here is a promise the feature makes to its
// callers, so a failure means either the implementation or the contract is wrong, never that the
// test needs "updating to match".
//
// Two rules this file follows throughout:
//
//   * Diagnostics are asserted by CODE, never by message text. The code is the published identity
//     (DreamShaderDiagnostic.h); the message is free to be reworded and is gathered for zh-Hans.
//   * Every successful preprocess asserts line-count conservation, even when that is not what the
//     case is about. It costs nothing and it is the invariant that silently corrupts every
//     diagnostic location in the file when it breaks.
//
// Pure functions only: no world, no asset, no file I/O. Runs in milliseconds.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderDefineTable.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderPreprocessor.h"

#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::Tests::Preprocessor
{
	/** Stand-in path for diagnostics. Nothing is read from disk. */
	static const TCHAR* const TestFilePath = TEXT("C:/Proj/DShader/PreprocessorTest.dsm");

	/**
	 * THE measure of line conservation.
	 *
	 * Counting '\n' rather than comparing split-line arrays is deliberate: it is well defined for
	 * empty input, for input with no trailing terminator, and for CRLF input, and it is exactly the
	 * quantity the generator's diagnostics mapper counts when it walks a
	 * `// Begin/End DreamShader source:` block back to a physical line number.
	 */
	inline int32 CountLineFeeds(const FString& Text)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			if (Text[Index] == TEXT('\n'))
			{
				++Count;
			}
		}
		return Count;
	}

	/** Splits on '\n' and drops a trailing '\r', so a CRLF fixture indexes the same as an LF one. */
	inline TArray<FString> SplitPreprocessorLines(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), /*InCullEmpty*/ false);
		for (FString& Line : Lines)
		{
			Line.RemoveFromEnd(TEXT("\r"));
		}
		return Lines;
	}

	/** One (name, value) row for a fixture table. An aggregate, so braced table literals work. */
	struct FDefineSpec
	{
		const TCHAR* Name;
		const TCHAR* Value;
	};

	/**
	 * Builds a table through the public Set(). Source is Settings -- any non-Builtin tier would do,
	 * but Settings is the one a project actually ships, so a fixture built this way is a table the
	 * feature really sees.
	 */
	inline FDreamShaderDefineTable MakeDefineTable(const TArray<FDefineSpec>& Specs)
	{
		FDreamShaderDefineTable Table;
		for (const FDefineSpec& Spec : Specs)
		{
			Table.Set(Spec.Name, Spec.Value, EDreamShaderDefineSource::Settings, TEXT("PreprocessorTests"));
		}
		return Table;
	}

	/**
	 * The fixture every expression case is evaluated against.
	 *
	 * No name here starts with `DS_`: that prefix is reserved and Set() refuses it from a non-Builtin
	 * tier, so a fixture using it would silently be an EMPTY table and half these cases would pass
	 * for the wrong reason.
	 */
	inline FDreamShaderDefineTable MakeExpressionFixtureTable()
	{
		return MakeDefineTable({
			{ TEXT("PP_ONE"),    TEXT("1")       },
			{ TEXT("PP_TWO"),    TEXT("2")       },
			{ TEXT("PP_ZERO"),   TEXT("0")       },
			{ TEXT("PP_EMPTY"),  TEXT("")        },  // bare marker: defined() is true, arithmetic reads 1
			{ TEXT("PP_HEX"),    TEXT("0x20")    },
			// A value is an integer when, after trimming, an optional sign plus decimal or hex digits
			// consumes the whole string. These three are the cases a naive "is it all digits" check
			// gets wrong, and getting them wrong turns an integer define into a string, which under
			// the strict string rules below is then a hard DSH1040 rather than a quiet misread.
			{ TEXT("PP_NEG"),    TEXT("-3")      },
			{ TEXT("PP_PADDED"), TEXT("  7  ")   },
			{ TEXT("PP_HEXUP"),  TEXT("+0x1F")   },
			{ TEXT("PP_TEXT"),   TEXT("Windows") },  // the one genuinely non-numeric value
			// PP_MISSING is intentionally never defined, and neither is PP_SC.
		});
	}

	inline FDreamShaderDefineValueMap MakeTouchedMap(const TArray<FDefineSpec>& Specs)
	{
		FDreamShaderDefineValueMap Map;
		for (const FDefineSpec& Spec : Specs)
		{
			Map.Add(FString(Spec.Name), FString(Spec.Value));
		}
		return Map;
	}

	/**
	 * Runs the preprocessor, requires success, and asserts line-count conservation.
	 *
	 * Returns false when preprocessing failed, so a caller can skip assertions that would otherwise
	 * report a second, misleading failure against a default-constructed result.
	 */
	inline bool RunPreprocessorExpectingSuccess(
		FAutomationTestBase& Test,
		const FString& What,
		const FString& Source,
		const FDreamShaderDefineTable& Defines,
		FDreamShaderPreprocessResult& OutResult)
	{
		OutResult = FDreamShaderPreprocessResult();

		FDreamShaderTextError Error;
		if (!PreprocessDreamShaderSource(Source, TestFilePath, Defines, OutResult, Error))
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] preprocessing should SUCCEED but failed with %s: %s"),
				*What, *Error.Code, *Error.Message.ToString()));
			return false;
		}

		Test.TestEqual(
			FString::Printf(TEXT("[%s] output line-feed count equals input"), *What),
			CountLineFeeds(OutResult.Text),
			CountLineFeeds(Source));
		return true;
	}

	/** Runs the preprocessor, requires failure, and asserts the DSHnnnn code (never the message). */
	inline void RunPreprocessorExpectingCode(
		FAutomationTestBase& Test,
		const FString& What,
		const FString& Source,
		const FDreamShaderDefineTable& Defines,
		const FString& ExpectedCode)
	{
		FDreamShaderPreprocessResult Result;
		FDreamShaderTextError Error;
		const bool bOk = PreprocessDreamShaderSource(Source, TestFilePath, Defines, Result, Error);

		Test.TestFalse(FString::Printf(TEXT("[%s] preprocessing should FAIL"), *What), bOk);
		Test.TestEqual(
			FString::Printf(TEXT("[%s] diagnostic code (message was: %s)"), *What, *Error.Message.ToString()),
			Error.Code,
			ExpectedCode);
	}

	/**
	 * The standard single-condition probe. KEEP_ME survives iff the condition is true, CUT_ME iff it
	 * is false. BOTH markers are asserted, so an implementation that emits everything -- or nothing --
	 * fails instead of half-passing.
	 */
	inline FString MakeConditionProbe(const FString& Expression)
	{
		return FString::Printf(TEXT("#if %s\nKEEP_ME\n#else\nCUT_ME\n#endif\n"), *Expression);
	}

	inline void TestBranchTaken(FAutomationTestBase& Test, const FString& What, const FString& Text, bool bExpectTrue)
	{
		Test.TestTrue(
			FString::Printf(TEXT("[%s] KEEP_ME %s in the output"), *What, bExpectTrue ? TEXT("is") : TEXT("is NOT")),
			Text.Contains(TEXT("KEEP_ME")) == bExpectTrue);
		Test.TestTrue(
			FString::Printf(TEXT("[%s] CUT_ME %s in the output"), *What, bExpectTrue ? TEXT("is NOT") : TEXT("is")),
			Text.Contains(TEXT("CUT_ME")) == !bExpectTrue);
	}

	inline void TestTouchedValue(
		FAutomationTestBase& Test,
		const FString& What,
		const FDreamShaderPreprocessResult& Result,
		const TCHAR* Name,
		const TCHAR* ExpectedValue)
	{
		const FString* Found = Result.TouchedDefines.Find(FString(Name));
		if (Found == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("[%s] '%s' should be in the touched set but is not."), *What, Name));
			return;
		}
		Test.TestEqual(
			FString::Printf(TEXT("[%s] touched value of '%s'"), *What, Name),
			*Found,
			FString(ExpectedValue));
	}

	inline void TestNotTouched(
		FAutomationTestBase& Test,
		const FString& What,
		const FDreamShaderPreprocessResult& Result,
		const TCHAR* Name)
	{
		Test.TestFalse(
			FString::Printf(TEXT("[%s] '%s' must NOT be in the touched set"), *What, Name),
			Result.TouchedDefines.Contains(FString(Name)));
	}
}

/**
 * Base for the tests whose cases deliberately fail. A refusal is allowed to log, and an incidental
 * log line must not be mistaken for a test failure. The success-only tests deliberately do NOT use
 * this, so an unexpected error log on a happy path still surfaces.
 *
 * At file scope rather than inside the namespace above, matching the existing
 * FDreamShaderQuietAutomationTestBase: IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST names the base in both
 * a base-clause and a mem-initializer, and an unqualified name is the shape that is known to work.
 */
class FDreamShaderPreprocessorQuietTestBase : public FAutomationTestBase
{
public:
	FDreamShaderPreprocessorQuietTestBase(const FString& InName, bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	virtual bool SuppressLogErrors() override { return true; }
	virtual bool SuppressLogWarnings() override { return true; }
};

// -------------------------------------------------------------------------------------------------
// Line-count conservation.
//
// The load-bearing invariant of the whole feature, and the reason the design is line-oriented at all.
// LoadPreparedDreamShaderSourceRecursive blanks its own Import lines for the same reason: the
// generator's diagnostics mapper recovers physical line numbers by COUNTING lines inside a source
// block. Lose one line here and every error below it in the file -- in every source that ever uses a
// conditional -- points at the wrong place, silently and forever.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorLineCountTest,
	"DreamShader.Lang.Preprocessor.LineCountConservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorLineCountTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FLineCase
	{
		const TCHAR* What;
		const TCHAR* Source;
	};

	const FLineCase Cases[] =
	{
		{ TEXT("empty input"),                    TEXT("") },
		{ TEXT("one empty line"),                 TEXT("\n") },
		{ TEXT("only blank lines"),               TEXT("\n\n\n") },
		{ TEXT("no directives, trailing NL"),     TEXT("alpha\nbeta\ngamma\n") },
		// The classic off-by-one: an implementation that appends a terminator after every line turns
		// a file with no final newline into one that has it, and every downstream count shifts.
		{ TEXT("no directives, no trailing NL"),  TEXT("alpha\nbeta") },
		{ TEXT("single line, no newline at all"), TEXT("alpha") },

		{ TEXT("everything active"),              TEXT("#if 1\nKEEP_ME\n#endif\n") },
		{ TEXT("everything elided"),              TEXT("#if 0\nCUT_ME\n#endif\n") },
		{ TEXT("if/else, true"),                  TEXT("#if 1\nKEEP_ME\n#else\nCUT_ME\n#endif\n") },
		{ TEXT("if/else, false"),                 TEXT("#if 0\nCUT_ME\n#else\nKEEP_ME\n#endif\n") },
		{ TEXT("elif chain"),                     TEXT("#if 0\nA\n#elif 0\nB\n#elif 1\nKEEP_ME\n#else\nD\n#endif\n") },

		// A directive on the very first line and on the very last line, with and without a final
		// newline -- both ends are where a "blank it out" loop tends to lose or gain one.
		{ TEXT("directive first and last"),       TEXT("#if 1\nKEEP_ME\n#endif") },
		{ TEXT("directive last, no content"),     TEXT("#if 0\n#endif") },
		{ TEXT("only directives"),                TEXT("#if 1\n#else\n#endif\n") },
		{ TEXT("only directives, no trailing NL"),TEXT("#if 1\n#else\n#endif") },

		{ TEXT("nested, outer true"),             TEXT("#if 1\nA\n#if 0\nB\n#if 1\nC\n#endif\n#endif\nKEEP_ME\n#endif\n") },
		{ TEXT("nested, outer false"),            TEXT("#if 0\nA\n#if 1\nB\n#endif\n#endif\n") },
		{ TEXT("nested in the else group"),       TEXT("#if 0\nA\n#else\n#if 1\nKEEP_ME\n#endif\n#endif\n") },

		{ TEXT("define and undef lines"),         TEXT("#define PP_X 1\n#if PP_X\nKEEP_ME\n#endif\n#undef PP_X\n") },

		// CRLF: the newline count is the invariant, whichever terminator the file uses.
		{ TEXT("CRLF, elided"),                   TEXT("#if 0\r\nCUT_ME\r\n#endif\r\n") },
		{ TEXT("CRLF, active"),                   TEXT("#if 1\r\nKEEP_ME\r\n#endif\r\n") },
		{ TEXT("CRLF, no directives"),            TEXT("alpha\r\nbeta\r\n") },

		{ TEXT("region pass-through"),            TEXT("\t\t#Region \"R\"\n\t\tKEEP_ME\n\t\t#EndRegion\n") },
		{ TEXT("indented directives"),            TEXT("    #if 1\n    KEEP_ME\n    #endif\n") },
	};

	for (const FLineCase& Case : Cases)
	{
		FDreamShaderPreprocessResult Result;
		// RunPreprocessorExpectingSuccess asserts the newline count for every one of these.
		RunPreprocessorExpectingSuccess(*this, Case.What, Case.Source, Defines, Result);
	}

	// A source with no directives at all must come back byte-identical. Preprocessing is supposed to
	// be invisible to the sources that do not use it -- rewriting line terminators or appending a
	// final newline would make every existing .dsm silently different text from the file on disk.
	{
		const FString Untouched = TEXT("Shader(Name=\"M_Foo\", Root=\"Game\")\n{\n\tSettings = {\n\t}\n}\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("no-directive identity"), Untouched, Defines, Result))
		{
			TestEqual(TEXT("a source with no directives passes through unchanged"), Result.Text, Untouched);
			TestFalse(TEXT("no directives -> bHadDirectives false"), Result.bHadDirectives);
			TestEqual(TEXT("no directives -> nothing touched"), Result.TouchedDefines.Num(), 0);
		}
	}

	// Empty input must not gain a line.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("empty identity"), FString(), Defines, Result))
		{
			TestTrue(TEXT("empty input yields empty output"), Result.Text.IsEmpty());
			TestFalse(TEXT("empty input -> bHadDirectives false"), Result.bHadDirectives);
		}
	}

	// Directive lines become EMPTY lines, not deleted lines and not surviving text.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("directive lines blanked"), TEXT("#if 1\n#endif\n"), Defines, Result))
		{
			const TArray<FString> Lines = SplitPreprocessorLines(Result.Text);
			if (TestEqual(TEXT("two lines out"), Lines.Num(), 2))
			{
				TestTrue(TEXT("the #if line is blank"), Lines[0].TrimStartAndEnd().IsEmpty());
				TestTrue(TEXT("the #endif line is blank"), Lines[1].TrimStartAndEnd().IsEmpty());
			}
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Line POSITIONS, not just the count.
//
// Equal newline counts are necessary but not sufficient: the mapper needs surviving content to sit on
// its ORIGINAL physical line. An implementation that emitted all the blanks at the end would satisfy
// the count and still misreport every location.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorLinePositionTest,
	"DreamShader.Lang.Preprocessor.SurvivingLinesKeepTheirPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorLinePositionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	//   index 0: Shader(...)
	//         1: {
	//         2: #if 0
	//         3:     CUT_ME
	//         4: #else
	//         5:     KEEP_ME          <- must still be line index 5 on the way out
	//         6: #endif
	//         7: }
	const FString Source =
		TEXT("Shader(Name=\"M_Foo\")\n")
		TEXT("{\n")
		TEXT("#if 0\n")
		TEXT("    CUT_ME\n")
		TEXT("#else\n")
		TEXT("    KEEP_ME\n")
		TEXT("#endif\n")
		TEXT("}\n");

	FDreamShaderPreprocessResult Result;
	if (!RunPreprocessorExpectingSuccess(*this, TEXT("line positions"), Source, Defines, Result))
	{
		return false;
	}

	const TArray<FString> Lines = SplitPreprocessorLines(Result.Text);
	if (!TestEqual(TEXT("eight lines out"), Lines.Num(), 8))
	{
		return false;
	}

	TestEqual(TEXT("line 0 untouched"), Lines[0], FString(TEXT("Shader(Name=\"M_Foo\")")));
	TestEqual(TEXT("line 1 untouched"), Lines[1], FString(TEXT("{")));
	TestTrue(TEXT("line 2 (#if) is blank"), Lines[2].TrimStartAndEnd().IsEmpty());
	TestTrue(TEXT("line 3 (elided content) is blank"), Lines[3].TrimStartAndEnd().IsEmpty());
	TestTrue(TEXT("line 4 (#else) is blank"), Lines[4].TrimStartAndEnd().IsEmpty());
	// Indentation survives too: columns in a diagnostic have to keep meaning something.
	TestEqual(TEXT("line 5 keeps its content AND its indentation"), Lines[5], FString(TEXT("    KEEP_ME")));
	TestTrue(TEXT("line 6 (#endif) is blank"), Lines[6].TrimStartAndEnd().IsEmpty());
	TestEqual(TEXT("line 7 untouched"), Lines[7], FString(TEXT("}")));

	// No directive may survive into the parser's input.
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FString Trimmed = Lines[Index].TrimStartAndEnd();
		TestFalse(
			FString::Printf(TEXT("line %d does not still carry a directive: %s"), Index, *Trimmed),
			Trimmed.StartsWith(TEXT("#if"), ESearchCase::CaseSensitive)
			|| Trimmed.StartsWith(TEXT("#else"), ESearchCase::CaseSensitive)
			|| Trimmed.StartsWith(TEXT("#elif"), ESearchCase::CaseSensitive)
			|| Trimmed.StartsWith(TEXT("#endif"), ESearchCase::CaseSensitive));
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// The touched set.
//
// TouchedDefines is folded into the asset's build key, so it decides what gets rebuilt when a define
// changes. Every omission here is a MISSED rebuild -- an asset that keeps a graph generated from a
// branch its source no longer takes -- and every spurious addition is a rebuild storm. Neither is
// visible until someone notices the material is wrong.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorTouchedDefinesTest,
	"DreamShader.Lang.Preprocessor.TouchedDefines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorTouchedDefinesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	// A name that is read gets its value recorded.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("plain read"), MakeConditionProbe(TEXT("PP_ONE")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("plain read"), Result, TEXT("PP_ONE"), TEXT("1"));
			TestEqual(TEXT("[plain read] exactly one name touched"), Result.TouchedDefines.Num(), 1);
		}
	}

	// THE ONE THAT ROTS SILENTLY.
	//
	// A name read while undefined must be recorded with GDreamShaderUndefinedDefineSentinel, not
	// skipped. If it is skipped, the map for `#if PP_MISSING` is empty, so the build key of every
	// asset compiled from that source is the same whether PP_MISSING exists or not. Someone later
	// adds PP_MISSING to the project settings, the source starts wanting the other branch -- and
	// nothing rebuilds, because the hash did not move. The asset keeps a graph its source no longer
	// describes, and the only symptom is a material that renders wrong.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("undefined read"), MakeConditionProbe(TEXT("PP_MISSING")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("undefined read"), Result, TEXT("PP_MISSING"), GDreamShaderUndefinedDefineSentinel);
			TestEqual(TEXT("[undefined read] exactly one name touched"), Result.TouchedDefines.Num(), 1);
		}
	}

	// defined(X) reads X. It does not read X's VALUE, but it absolutely reads whether X exists, and
	// that is what decides the output -- so X has to be in the map with the sentinel. Leaving it out
	// is the same silent-miss as above, one step less obvious.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("defined() on a missing name"), MakeConditionProbe(TEXT("defined(PP_MISSING)")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("defined() on a missing name"), Result, TEXT("PP_MISSING"), GDreamShaderUndefinedDefineSentinel);
		}
	}
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("defined() records the value it had"), MakeConditionProbe(TEXT("defined(PP_ZERO)")), Defines, Result))
		{
			// "mapped to the value it had" -- even though defined() never looked at it. Recording "0"
			// rather than the sentinel is what makes a later edit of PP_ZERO's value rebuild this
			// asset, which it must: the source could read PP_ZERO for its value elsewhere tomorrow.
			TestTouchedValue(*this, TEXT("defined() records the value it had"), Result, TEXT("PP_ZERO"), TEXT("0"));
		}
	}

	// A define that exists with an EMPTY value must not be recorded as the sentinel. "#define FOO"
	// and "FOO is absent" are different states that produce different output (`#if FOO` is true for
	// the first and false for the second), so they must produce different build keys.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("bare marker is not the sentinel"), MakeConditionProbe(TEXT("PP_EMPTY")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("bare marker is not the sentinel"), Result, TEXT("PP_EMPTY"), TEXT(""));
		}
	}

	// String values are recorded verbatim.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("string value"), MakeConditionProbe(TEXT("PP_TEXT == \"Windows\"")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("string value"), Result, TEXT("PP_TEXT"), TEXT("Windows"));
		}
	}

	// Reading the same name twice is one entry, not two and not a changed value.
	{
		const FString Source = TEXT("#if PP_ONE\nA\n#endif\n#if PP_ONE == 1\nB\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("repeated read"), Source, Defines, Result))
		{
			TestTouchedValue(*this, TEXT("repeated read"), Result, TEXT("PP_ONE"), TEXT("1"));
			TestEqual(TEXT("[repeated read] one entry, not two"), Result.TouchedDefines.Num(), 1);
		}
	}

	// A define mentioned only inside a branch that was cut is NOT touched -- and the paired positive
	// control proves the assertion discriminates. Without the control, an implementation whose
	// touched set is always empty would pass the negative half.
	{
		const FString Dead = TEXT("#if PP_ZERO\n#if PP_DEAD_INNER\nA\n#endif\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("dead nested #if"), Dead, Defines, Result))
		{
			TestTouchedValue(*this, TEXT("dead nested #if"), Result, TEXT("PP_ZERO"), TEXT("0"));
			TestNotTouched(*this, TEXT("dead nested #if"), Result, TEXT("PP_DEAD_INNER"));
		}
	}
	{
		// Same shape, live outer branch: now the inner name IS read.
		const FString Live = TEXT("#if PP_ONE\n#if PP_DEAD_INNER\nA\n#endif\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("live nested #if (control)"), Live, Defines, Result))
		{
			TestTouchedValue(*this, TEXT("live nested #if (control)"), Result, TEXT("PP_DEAD_INNER"), GDreamShaderUndefinedDefineSentinel);
		}
	}

	// Once a branch is taken, the conditions of the #elif groups after it are never evaluated, so
	// their names are not touched. Same argument as the dead-branch case: those conditions cannot
	// change the output until the condition that already won changes, and THAT one is in the map.
	// (It also means a later #elif is free to hold an expression that would not even evaluate --
	// see the InactiveBranchIsNotValidated test.)
	{
		const FString Source = TEXT("#if PP_ONE\nKEEP_ME\n#elif PP_LATER\nCUT_ME\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#elif after a taken branch"), Source, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("#elif after a taken branch"), Result.Text, /*bExpectTrue*/ true);
			TestTouchedValue(*this, TEXT("#elif after a taken branch"), Result, TEXT("PP_ONE"), TEXT("1"));
			TestNotTouched(*this, TEXT("#elif after a taken branch"), Result, TEXT("PP_LATER"));
		}
	}
	{
		// Control: when the #if is false, the #elif condition IS the deciding read.
		const FString Source = TEXT("#if PP_ZERO\nCUT_ME\n#elif PP_LATER\nA\n#else\nKEEP_ME\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#elif after a failed branch (control)"), Source, Defines, Result))
		{
			TestTouchedValue(*this, TEXT("#elif after a failed branch (control)"), Result, TEXT("PP_ZERO"), TEXT("0"));
			TestTouchedValue(*this, TEXT("#elif after a failed branch (control)"), Result, TEXT("PP_LATER"), GDreamShaderUndefinedDefineSentinel);
		}
	}

	// A source with no conditions touches nothing at all -- an empty map, not a map of everything the
	// table happened to hold.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("no conditions"), TEXT("alpha\nbeta\n"), Defines, Result))
		{
			TestEqual(TEXT("[no conditions] touched set is empty"), Result.TouchedDefines.Num(), 0);
		}
	}

	// #ifdef / #ifndef read a name the same way defined() does, so they record the same way: the
	// value when there is one, the sentinel when there is not.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifdef read"), TEXT("#ifdef PP_ZERO\nA\n#endif\n"), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("#ifdef read"), Result, TEXT("PP_ZERO"), TEXT("0"));
		}
	}
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifndef read"), TEXT("#ifndef PP_MISSING\nA\n#endif\n"), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("#ifndef read"), Result, TEXT("PP_MISSING"), GDreamShaderUndefinedDefineSentinel);
		}
	}

	// ...and a name reached only from a dead branch stays out, whichever spelling asked for it.
	{
		const FString Source = TEXT("#if PP_ZERO\n#ifdef PP_DEAD_IFDEF\nA\n#endif\n#ifndef PP_DEAD_IFNDEF\nB\n#endif\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("dead #ifdef / #ifndef"), Source, Defines, Result))
		{
			TestTouchedValue(*this, TEXT("dead #ifdef / #ifndef"), Result, TEXT("PP_ZERO"), TEXT("0"));
			TestNotTouched(*this, TEXT("dead #ifdef / #ifndef"), Result, TEXT("PP_DEAD_IFDEF"));
			TestNotTouched(*this, TEXT("dead #ifdef / #ifndef"), Result, TEXT("PP_DEAD_IFNDEF"));
			TestEqual(TEXT("[dead #ifdef / #ifndef] only the live condition is recorded"), Result.TouchedDefines.Num(), 1);
		}
	}

	// The touched map is case-sensitive on the same terms as the define table, now that both use the
	// same key funcs. Merging Foo with FOO here would not be untidiness, it would be a build-key
	// soundness hole: two distinct names collapse to one entry, only one value ever reaches the hash,
	// and editing the other rebuilds nothing.
	{
		const FDreamShaderDefineTable CaseDefines = MakeDefineTable({
			{ TEXT("Foo"), TEXT("1") },
			{ TEXT("FOO"), TEXT("2") },
			{ TEXT("foo"), TEXT("3") },
		});

		const FString Source = TEXT("#if Foo\nA\n#endif\n#if FOO\nB\n#endif\n#if foo\nC\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("case-sensitive touched set"), Source, CaseDefines, Result))
		{
			TestEqual(TEXT("[case-sensitive touched set] three separate entries"), Result.TouchedDefines.Num(), 3);
			TestTouchedValue(*this, TEXT("case-sensitive touched set"), Result, TEXT("Foo"), TEXT("1"));
			TestTouchedValue(*this, TEXT("case-sensitive touched set"), Result, TEXT("FOO"), TEXT("2"));
			TestTouchedValue(*this, TEXT("case-sensitive touched set"), Result, TEXT("foo"), TEXT("3"));
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Short circuit.
//
// Not an optimization: it is part of the build-key contract. A name the evaluator never had to read
// cannot change the output, so folding it into the key would rebuild every asset in the tree each
// time an unrelated define moves. Conversely, an operand that IS read must be recorded.
//
// Every case below is paired with a positive control that reaches the SAME name through the
// non-short-circuiting side of the same operator. Without the controls, an implementation that
// simply never records right-hand operands would pass the whole test.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorShortCircuitTest,
	"DreamShader.Lang.Preprocessor.TouchedDefinesShortCircuit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorShortCircuitTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FShortCircuitCase
	{
		const TCHAR* Expression;
		bool bExpectTouched;
	};

	// PP_SC is never defined, so "touched" always means "recorded with the sentinel".
	const FShortCircuitCase Cases[] =
	{
		// && with a false left operand never reads the right one...
		{ TEXT("0 && PP_SC"),           false },
		{ TEXT("PP_ZERO && PP_SC"),     false },
		// ...but with a true left operand it must.
		{ TEXT("1 && PP_SC"),           true  },
		{ TEXT("PP_ONE && PP_SC"),      true  },

		// || with a true left operand never reads the right one...
		{ TEXT("1 || PP_SC"),           false },
		{ TEXT("PP_ONE || PP_SC"),      false },
		{ TEXT("!0 || PP_SC"),          false },
		// ...but with a false left operand it must.
		{ TEXT("0 || PP_SC"),           true  },
		{ TEXT("PP_ZERO || PP_SC"),     true  },

		// The cut applies to whole parenthesised subexpressions, not just to a bare identifier.
		{ TEXT("0 && (PP_SC || 1)"),    false },
		{ TEXT("(0 && PP_SC) || 1"),    false },
		{ TEXT("1 || (PP_SC && 1)"),    false },
		// ...and a live subexpression still reads through.
		{ TEXT("1 && (0 || PP_SC)"),    true  },
		{ TEXT("0 || (1 && PP_SC)"),    true  },
	};

	for (const FShortCircuitCase& Case : Cases)
	{
		const FString What = FString::Printf(TEXT("short circuit: #if %s"), Case.Expression);
		FDreamShaderPreprocessResult Result;
		if (!RunPreprocessorExpectingSuccess(*this, What, MakeConditionProbe(Case.Expression), Defines, Result))
		{
			continue;
		}

		if (Case.bExpectTouched)
		{
			TestTouchedValue(*this, What, Result, TEXT("PP_SC"), GDreamShaderUndefinedDefineSentinel);
		}
		else
		{
			TestNotTouched(*this, What, Result, TEXT("PP_SC"));
		}
	}

	// The sharpest proof that the right operand is genuinely not evaluated: a short-circuited
	// division by zero must not raise DSH1041. An implementation that evaluates both sides and then
	// discards one would fail here even though its touched set happened to look right.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("short-circuited divide by zero"), MakeConditionProbe(TEXT("0 && (1 / 0)")), Defines, Result))
		{
			TestBranchTaken(*this, TEXT("short-circuited divide by zero"), Result.Text, /*bExpectTrue*/ false);
		}
	}
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("short-circuited type error"), MakeConditionProbe(TEXT("1 || (\"a\" < \"b\")")), Defines, Result))
		{
			TestBranchTaken(*this, TEXT("short-circuited type error"), Result.Text, /*bExpectTrue*/ true);
		}
	}
	{
		// A string operand is a hard DSH1040 in every position -- unless the operator never reaches
		// it. `#if 1 && PP_TEXT` is in the diagnostics table for exactly this contrast.
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("short-circuited string operand"), MakeConditionProbe(TEXT("0 && PP_TEXT")), Defines, Result))
		{
			TestBranchTaken(*this, TEXT("short-circuited string operand"), Result.Text, /*bExpectTrue*/ false);
			TestNotTouched(*this, TEXT("short-circuited string operand"), Result, TEXT("PP_TEXT"));
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// The touched set records only the reads the INJECTED TABLE answered.
//
// A name the file has already #define'd or #undef'd is answered by the source text, and the source
// text is hashed in its own right -- so recording that read would be redundant at best. At worst it is
// wrong: the map holds ONE value per name, and a file that reads a name on both sides of a #define
// produces two different answers for it, only one of which can be stored.
//
// Dropping the file-local reads is what makes one-value-per-name sufficient, because every read that
// survives resolved against a table that is fixed for the whole run. This is the closing step of the
// soundness argument in 5.2, not an optimisation, so both directions are pinned.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorShadowedTouchTest,
	"DreamShader.Lang.Preprocessor.TouchedDefinesFileLocalShadowing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorShadowedTouchTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	// Control: with no local definition in the way, a read resolves against the table and is recorded.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("table read (control)"), TEXT("#if PP_ONE\nA\n#endif\n"), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("table read (control)"), Result, TEXT("PP_ONE"), TEXT("1"));
		}
	}

	// Shadowed by a local #define: the answer came from this file, so it is not recorded.
	{
		const FString Source = TEXT("#define PP_ONE 5\n#if PP_ONE == 5\nA\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("read after a local #define"), Source, Defines, Result))
		{
			TestNotTouched(*this, TEXT("read after a local #define"), Result, TEXT("PP_ONE"));
			TestEqual(TEXT("[read after a local #define] nothing else crept in"), Result.TouchedDefines.Num(), 0);
		}
	}

	// Both reads in one file. The first resolved against the table and is recorded WITH THE TABLE'S
	// VALUE; the second resolved against the #define and contributes nothing -- so the single stored
	// value is unambiguous even though the file saw two.
	{
		const FString Source =
			TEXT("#if PP_ONE\n")
			TEXT("A\n")
			TEXT("#endif\n")
			TEXT("#define PP_ONE 5\n")
			TEXT("#if PP_ONE == 5\n")
			TEXT("B\n")
			TEXT("#endif\n");

		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("read on both sides of a #define"), Source, Defines, Result))
		{
			TestTouchedValue(*this, TEXT("read on both sides of a #define"), Result, TEXT("PP_ONE"), TEXT("1"));
			TestEqual(TEXT("[read on both sides of a #define] one entry, not two"), Result.TouchedDefines.Num(), 1);
		}
	}

	// A name the table never held, invented and read locally: not a table read, so not recorded.
	{
		const FString Source = TEXT("#define PP_LOCAL_ONLY 1\n#if PP_LOCAL_ONLY\nA\n#endif\n#ifdef PP_LOCAL_ONLY\nB\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("locally invented name"), Source, Defines, Result))
		{
			TestNotTouched(*this, TEXT("locally invented name"), Result, TEXT("PP_LOCAL_ONLY"));
			TestEqual(TEXT("[locally invented name] touched set is empty"), Result.TouchedDefines.Num(), 0);
		}
	}

	// #undef puts a name under the file's control just as #define does.
	{
		const FString Source = TEXT("#undef PP_ONE\n#if defined(PP_ONE)\nA\n#else\nB\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("read after a local #undef"), Source, Defines, Result))
		{
			TestNotTouched(*this, TEXT("read after a local #undef"), Result, TEXT("PP_ONE"));
		}
	}

	// ...including the vacuous case, where the #undef removed nothing. Adding PP_MISSING to the
	// project later still cannot change this file's output, because the #undef would erase it again.
	{
		const FString Source = TEXT("#undef PP_MISSING\n#if PP_MISSING\nA\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("read after a vacuous #undef"), Source, Defines, Result))
		{
			TestNotTouched(*this, TEXT("read after a vacuous #undef"), Result, TEXT("PP_MISSING"));
		}
	}

	// A read BEFORE the #undef is still a table read and is still recorded.
	{
		const FString Source = TEXT("#if PP_ONE\nA\n#endif\n#undef PP_ONE\n#if defined(PP_ONE)\nB\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("read before a local #undef"), Source, Defines, Result))
		{
			TestTouchedValue(*this, TEXT("read before a local #undef"), Result, TEXT("PP_ONE"), TEXT("1"));
			TestEqual(TEXT("[read before a local #undef] one entry"), Result.TouchedDefines.Num(), 1);
		}
	}

	// #define then #undef leaves the name file-controlled, not handed back to the table.
	{
		const FString Source = TEXT("#define PP_TWO 9\n#undef PP_TWO\n#if defined(PP_TWO)\nA\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#define then #undef"), Source, Defines, Result))
		{
			TestNotTouched(*this, TEXT("#define then #undef"), Result, TEXT("PP_TWO"));
		}
	}

	// The rule is about who answered, not about which directive asked: #ifdef obeys it too.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifdef before a local #define"), TEXT("#ifdef PP_TWO\nA\n#endif\n#define PP_TWO 9\n"), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("#ifdef before a local #define"), Result, TEXT("PP_TWO"), TEXT("2"));
		}
	}
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifdef after a local #define"), TEXT("#define PP_TWO 9\n#ifdef PP_TWO\nA\n#endif\n"), Defines, Result))
		{
			TestNotTouched(*this, TEXT("#ifdef after a local #define"), Result, TEXT("PP_TWO"));
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Constant-expression evaluation.
//
// Table-driven against the grammar in Plan/preprocessor-conditionals.md 4.1. Every case is spelled as
// a condition that must be TRUE or FALSE, so a value assertion ("2*3 is 6") is written as an equality
// the evaluator has to get right ("2 * 3 == 6"). Precedence and associativity cases are chosen so the
// WRONG grouping gives the opposite answer -- a case both groupings agree on proves nothing.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorExpressionTest,
	"DreamShader.Lang.Preprocessor.ExpressionEvaluation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorExpressionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FExpressionCase
	{
		const TCHAR* Expression;
		bool bExpectTrue;
	};

	const FExpressionCase Cases[] =
	{
		// -- Literals and truthiness -------------------------------------------------------------
		{ TEXT("1"),                                  true  },
		{ TEXT("0"),                                  false },
		{ TEXT("2"),                                  true  },
		{ TEXT("-1"),                                 true  },  // non-zero is true, sign irrelevant
		{ TEXT("(0)"),                                false },
		{ TEXT("((((1))))"),                          true  },

		// -- Unary -------------------------------------------------------------------------------
		{ TEXT("!0"),                                 true  },
		{ TEXT("!1"),                                 false },
		{ TEXT("!5"),                                 false },  // '!' is logical, not bitwise
		{ TEXT("!!5"),                                true  },
		{ TEXT("-3 == 0 - 3"),                        true  },
		{ TEXT("+3 == 3"),                            true  },
		{ TEXT("!0 && 1"),                            true  },  // unary binds tighter than '&&'
		{ TEXT("!(0 && 1)"),                          true  },
		{ TEXT("!PP_MISSING"),                        true  },

		// -- Integer literals: decimal and 0x hex ------------------------------------------------
		{ TEXT("0x10 == 16"),                         true  },
		{ TEXT("0xff == 255"),                        true  },
		{ TEXT("0x0 == 0"),                           true  },
		{ TEXT("0x7fffffff == 2147483647"),           true  },
		{ TEXT("0X10 == 16"),                         true  },  // the 0x prefix is case-insensitive
		{ TEXT("0XFF == 255"),                        true  },

		// -- Arithmetic precedence and LEFT associativity ----------------------------------------
		{ TEXT("1 + 2 * 3 == 7"),                     true  },  // '*' over '+' (the other reading is 9)
		{ TEXT("(1 + 2) * 3 == 9"),                   true  },  // parentheses override it
		{ TEXT("10 - 3 - 4 == 3"),                    true  },  // left assoc; right assoc would be 11
		{ TEXT("100 / 10 / 2 == 5"),                  true  },  // left assoc; right assoc would be 20
		{ TEXT("7 % 3 == 1"),                         true  },
		{ TEXT("8 / 3 == 2"),                         true  },  // integer division truncates
		// C semantics for a negative operand: TRUNCATE TOWARD ZERO, with the remainder taking the
		// sign of the dividend. An evaluator that reaches for a floor-division helper silently
		// returns -4 and 1 instead, and the only symptom is a branch flipping in whichever material
		// happened to divide by something negative.
		{ TEXT("-7 / 2 == -3"),                       true  },  // not -4
		{ TEXT("-7 % 2 == -1"),                       true  },  // not 1
		{ TEXT("7 / -2 == -3"),                       true  },
		{ TEXT("7 % -2 == 1"),                        true  },  // the remainder follows the DIVIDEND's sign
		{ TEXT("2 * -3 == 0 - 6"),                    true  },
		{ TEXT("1 + 2 % 2 == 1"),                     true  },  // '%' over '+'

		// -- Relational binds tighter than equality ----------------------------------------------
		{ TEXT("1 < 2 == 1"),                         true  },  // (1<2)==1; the other reading is 1<(2==1) -> false
		{ TEXT("1 < 2"),                              true  },
		{ TEXT("2 < 2"),                              false },
		{ TEXT("2 <= 2"),                             true  },
		{ TEXT("3 > 2"),                              true  },
		{ TEXT("2 >= 3"),                             false },
		{ TEXT("1 + 1 < 3"),                          true  },  // arithmetic over relational

		// -- Equality ----------------------------------------------------------------------------
		{ TEXT("2 == 2"),                             true  },
		{ TEXT("2 == 3"),                             false },
		{ TEXT("2 != 3"),                             true  },
		{ TEXT("2 != 2"),                             false },

		// -- '==' over '&&' over '||' ------------------------------------------------------------
		{ TEXT("1 == 1 && 2 == 2"),                   true  },  // the other reading collapses to false
		{ TEXT("1 || 0 && 0"),                        true  },  // 1 || (0&&0); (1||0)&&0 would be false
		{ TEXT("0 && 0 || 1"),                        true  },  // (0&&0) || 1; 0&&(0||1) would be false
		{ TEXT("0 || 0"),                             false },
		{ TEXT("1 && 0"),                             false },
		{ TEXT("1 && 1 && 1"),                        true  },
		{ TEXT("0 || 0 || 1"),                        true  },

		// -- defined(), with and without parentheses ---------------------------------------------
		{ TEXT("defined(PP_ONE)"),                    true  },
		{ TEXT("defined PP_ONE"),                     true  },  // 4.1 allows the bare form
		{ TEXT("defined(PP_ZERO)"),                   true  },  // defined() does NOT look at the value
		{ TEXT("defined(PP_EMPTY)"),                  true  },  // ...not even an empty one
		{ TEXT("defined(PP_MISSING)"),                false },
		{ TEXT("defined PP_MISSING"),                 false },
		{ TEXT("!defined(PP_MISSING)"),               true  },
		{ TEXT("defined(PP_ONE) && defined(PP_TWO)"), true  },
		{ TEXT("defined(PP_ONE) == 1"),               true  },  // defined() yields 0/1, an integer

		// -- Identifier values -------------------------------------------------------------------
		{ TEXT("PP_ONE"),                             true  },
		{ TEXT("PP_ZERO"),                            false },
		{ TEXT("PP_MISSING"),                         false },  // an undefined identifier reads 0 (C semantics)
		{ TEXT("PP_MISSING == 0"),                    true  },
		{ TEXT("PP_TWO == 2"),                        true  },
		{ TEXT("PP_ONE + PP_TWO == 3"),               true  },
		{ TEXT("PP_HEX == 32"),                       true  },  // a value that parses as a hex literal is an integer
		// A value is an integer when, after trimming, an optional sign plus decimal or hex digits
		// consumes the whole string. Miss any of these three and the value falls through to the
		// string branch -- where, under the strict string rules, every use below becomes a hard
		// DSH1040. Loud, but loud in the user's source rather than here.
		{ TEXT("PP_NEG == -3"),                       true  },  // a leading sign belongs to the value
		{ TEXT("PP_NEG < 0"),                         true  },
		{ TEXT("PP_NEG"),                             true  },  // non-zero, so true
		{ TEXT("PP_PADDED == 7"),                     true  },  // surrounding whitespace is trimmed off first
		{ TEXT("PP_HEXUP == 31"),                     true  },  // a sign and an uppercase 0X together
		// A define with an empty value is a bare marker: arithmetic reads it as 1. That is
		// DreamShaderDefineTable.h's rule, and it is what makes `-D FOO` behave like `-D FOO=1`.
		// It is also the FIRST rule applied: an empty value consumes no digits, so testing the
		// integer shape first would classify it as a string and make every use below a DSH1040.
		{ TEXT("PP_EMPTY"),                           true  },
		{ TEXT("PP_EMPTY == 1"),                      true  },

		// -- Strings: '==' and '!=' only ---------------------------------------------------------
		{ TEXT("\"a\" == \"a\""),                     true  },
		{ TEXT("\"a\" == \"b\""),                     false },
		{ TEXT("\"a\" != \"b\""),                     true  },
		{ TEXT("\"\" == \"\""),                       true  },
		{ TEXT("PP_TEXT == \"Windows\""),             true  },
		{ TEXT("PP_TEXT != \"Linux\""),               true  },
		// String comparison is CASE-SENSITIVE, matching the case sensitivity of the names. FString's
		// operator== defaults to IgnoreCase in UE, so this is the case an implementation gets wrong
		// by writing the obvious thing -- and getting it wrong means `DS_PLATFORM == "windows"`
		// quietly matches on every platform whose name differs only in case.
		{ TEXT("PP_TEXT == \"windows\""),             false },
		{ TEXT("PP_TEXT != \"windows\""),             true  },

		// -- A trailing // comment on the directive line is allowed (3.1) -------------------------
		{ TEXT("1 // this branch is taken"),          true  },
		{ TEXT("0 // this branch is not"),            false },
		{ TEXT("PP_ONE && PP_TWO // both defined"),   true  },
	};

	for (const FExpressionCase& Case : Cases)
	{
		const FString What = FString::Printf(TEXT("#if %s"), Case.Expression);
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, What, MakeConditionProbe(Case.Expression), Defines, Result))
		{
			TestBranchTaken(*this, What, Result.Text, Case.bExpectTrue);
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// #ifdef / #ifndef.
//
// Sugar with exact definitions: `#ifdef NAME` is `#if defined(NAME)`, `#ifndef NAME` is
// `#if !defined(NAME)`. They exist because HLSL has them and muscle memory produces them whether or
// not they are supported -- and a directive that is silently unrecognised is a far worse outcome than
// one that works.
//
// "Exact" is the load-bearing word, so the equivalence is asserted directly: the same output text AND
// the same touched set as the long spelling, not merely the same branch.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorIfdefTest,
	"DreamShader.Lang.Preprocessor.IfdefAndIfndef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorIfdefTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	auto MakeIfdefProbe = [](const TCHAR* OpeningLine)
	{
		return FString::Printf(TEXT("%s\nKEEP_ME\n#else\nCUT_ME\n#endif\n"), OpeningLine);
	};

	struct FIfdefCase
	{
		const TCHAR* OpeningLine;
		bool bExpectTrue;
	};

	const FIfdefCase Cases[] =
	{
		{ TEXT("#ifdef PP_ONE"),                 true  },
		{ TEXT("#ifdef PP_ZERO"),                true  },  // defined-ness only; the value is never read
		{ TEXT("#ifdef PP_EMPTY"),               true  },  // ...not even an empty one
		// A string-valued define is DEFINED, so #ifdef answers true -- even though `#if PP_TEXT` is a
		// DSH1040. Defined-ness never type-checks, which is half the reason these two directives are
		// worth having.
		{ TEXT("#ifdef PP_TEXT"),                true  },
		{ TEXT("#ifdef PP_MISSING"),             false },

		{ TEXT("#ifndef PP_ONE"),                false },
		{ TEXT("#ifndef PP_ZERO"),               false },
		{ TEXT("#ifndef PP_TEXT"),               false },
		{ TEXT("#ifndef PP_MISSING"),            true  },

		// The same whitespace and comment tolerance as every other directive.
		{ TEXT("   #ifdef PP_ONE"),              true  },
		{ TEXT("#  ifdef PP_ONE"),               true  },
		{ TEXT("#\tifndef PP_MISSING"),          true  },
		{ TEXT("#ifdef PP_ONE // present"),      true  },
		{ TEXT("#ifndef PP_MISSING   "),         true  },
	};

	for (const FIfdefCase& Case : Cases)
	{
		const FString What = FString::Printf(TEXT("%s"), Case.OpeningLine);
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, What, MakeIfdefProbe(Case.OpeningLine), Defines, Result))
		{
			TestBranchTaken(*this, What, Result.Text, Case.bExpectTrue);
			TestTrue(FString::Printf(TEXT("[%s] bHadDirectives"), *What), Result.bHadDirectives);
		}
	}

	// Both spellings read the name, so both record it -- with its value, or with the sentinel.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifdef records the value"), MakeIfdefProbe(TEXT("#ifdef PP_ONE")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("#ifdef records the value"), Result, TEXT("PP_ONE"), TEXT("1"));
			TestEqual(TEXT("[#ifdef records the value] one entry"), Result.TouchedDefines.Num(), 1);
		}
	}
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifdef records the sentinel"), MakeIfdefProbe(TEXT("#ifdef PP_MISSING")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("#ifdef records the sentinel"), Result, TEXT("PP_MISSING"), GDreamShaderUndefinedDefineSentinel);
		}
	}
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifndef records the sentinel"), MakeIfdefProbe(TEXT("#ifndef PP_MISSING")), Defines, Result))
		{
			TestTouchedValue(*this, TEXT("#ifndef records the sentinel"), Result, TEXT("PP_MISSING"), GDreamShaderUndefinedDefineSentinel);
		}
	}

	// The equivalence itself, on both the text and the touched set. Asserting only "same branch"
	// would let a spelling that forgot to record its name pass.
	{
		const TCHAR* const Names[] = { TEXT("PP_ONE"), TEXT("PP_ZERO"), TEXT("PP_EMPTY"), TEXT("PP_TEXT"), TEXT("PP_MISSING") };

		auto AssertEquivalent = [this, &Defines](const FString& What, const FString& SugarSource, const FString& LongSource)
		{
			FDreamShaderPreprocessResult Sugar;
			FDreamShaderPreprocessResult Long;
			if (!RunPreprocessorExpectingSuccess(*this, What + TEXT(" (sugar)"), SugarSource, Defines, Sugar)
				|| !RunPreprocessorExpectingSuccess(*this, What + TEXT(" (long)"), LongSource, Defines, Long))
			{
				return;
			}

			TestEqual(FString::Printf(TEXT("[%s] same output text"), *What), Sugar.Text, Long.Text);
			TestEqual(FString::Printf(TEXT("[%s] same touched count"), *What), Sugar.TouchedDefines.Num(), Long.TouchedDefines.Num());
			for (const TPair<FString, FString>& Pair : Long.TouchedDefines)
			{
				const FString* Found = Sugar.TouchedDefines.Find(Pair.Key);
				if (Found == nullptr)
				{
					AddError(FString::Printf(TEXT("[%s] '%s' is touched by the long spelling but not the sugar."), *What, *Pair.Key));
					continue;
				}
				TestEqual(FString::Printf(TEXT("[%s] touched value of '%s'"), *What, *Pair.Key), *Found, Pair.Value);
			}
		};

		for (const TCHAR* Name : Names)
		{
			AssertEquivalent(
				FString::Printf(TEXT("#ifdef %s == #if defined(%s)"), Name, Name),
				FString::Printf(TEXT("#ifdef %s\nKEEP_ME\n#else\nCUT_ME\n#endif\n"), Name),
				FString::Printf(TEXT("#if defined(%s)\nKEEP_ME\n#else\nCUT_ME\n#endif\n"), Name));

			AssertEquivalent(
				FString::Printf(TEXT("#ifndef %s == #if !defined(%s)"), Name, Name),
				FString::Printf(TEXT("#ifndef %s\nKEEP_ME\n#else\nCUT_ME\n#endif\n"), Name),
				FString::Printf(TEXT("#if !defined(%s)\nKEEP_ME\n#else\nCUT_ME\n#endif\n"), Name));
		}
	}

	// They chain and nest like any other group opener.
	{
		struct FChainCase { const TCHAR* What; const TCHAR* Source; };
		const FChainCase Chains[] =
		{
			{ TEXT("#ifdef with #elif"),      TEXT("#ifdef PP_MISSING\nCUT_ME\n#elif PP_ONE\nKEEP_ME\n#else\nCUT_ME\n#endif\n") },
			{ TEXT("#ifndef with #else"),     TEXT("#ifndef PP_ONE\nCUT_ME\n#else\nKEEP_ME\n#endif\n") },
			{ TEXT("#ifdef inside #if"),      TEXT("#if 1\n#ifdef PP_ONE\nKEEP_ME\n#endif\n#endif\n") },
			{ TEXT("#if inside #ifndef"),     TEXT("#ifndef PP_MISSING\n#if 1\nKEEP_ME\n#endif\n#endif\n") },
			{ TEXT("#ifdef inside #ifndef"),  TEXT("#ifndef PP_MISSING\n#ifdef PP_ONE\nKEEP_ME\n#endif\n#endif\n") },
			{ TEXT("dead #ifdef group"),      TEXT("#ifdef PP_MISSING\nCUT_ME\n#endif\nKEEP_ME\n") },
		};

		for (const FChainCase& Case : Chains)
		{
			FDreamShaderPreprocessResult Result;
			if (RunPreprocessorExpectingSuccess(*this, Case.What, Case.Source, Defines, Result))
			{
				TestBranchTaken(*this, Case.What, Result.Text, /*bExpectTrue*/ true);
			}
		}
	}

	// The directive line is blanked in place, like every other directive line.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#ifdef line is blanked"), TEXT("#ifdef PP_ONE\nKEEP_ME\n#endif\n"), Defines, Result))
		{
			const TArray<FString> Lines = SplitPreprocessorLines(Result.Text);
			if (TestEqual(TEXT("[#ifdef line is blanked] three lines out"), Lines.Num(), 3))
			{
				TestTrue(TEXT("[#ifdef line is blanked] line 0 is blank"), Lines[0].TrimStartAndEnd().IsEmpty());
				TestEqual(TEXT("[#ifdef line is blanked] line 1 survives"), Lines[1], FString(TEXT("KEEP_ME")));
				TestTrue(TEXT("[#ifdef line is blanked] line 2 is blank"), Lines[2].TrimStartAndEnd().IsEmpty());
			}
		}
	}

	TestTrue(TEXT("#ifdef counts as a preprocessor directive"), DreamShaderSourceHasPreprocessorDirectives(TEXT("#ifdef PP_ONE\n#endif\n")));
	TestTrue(TEXT("#ifndef counts as a preprocessor directive"), DreamShaderSourceHasPreprocessorDirectives(TEXT("#ifndef PP_ONE\n#endif\n")));

	return true;
}

// -------------------------------------------------------------------------------------------------
// Directive syntax: what counts as a directive line at all.
//
// The recogniser sits in front of everything else, so a mistake here does not produce an error -- it
// produces a file whose conditionals are quietly inert, or whose ordinary text quietly disappears.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorDirectiveSyntaxTest,
	"DreamShader.Lang.Preprocessor.DirectiveSyntax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorDirectiveSyntaxTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FSyntaxCase
	{
		const TCHAR* What;
		const TCHAR* Source;
		bool bExpectKeep;  // KEEP_ME survives; CUT_ME must not
	};

	const FSyntaxCase Cases[] =
	{
		{ TEXT("leading spaces before #"),   TEXT("    #if 1\nKEEP_ME\n    #else\nCUT_ME\n    #endif\n"),  true },
		{ TEXT("leading tab before #"),      TEXT("\t#if 1\nKEEP_ME\n\t#else\nCUT_ME\n\t#endif\n"),        true },
		{ TEXT("spaces after #"),            TEXT("#  if 1\nKEEP_ME\n#  else\nCUT_ME\n#  endif\n"),        true },
		{ TEXT("tab after #"),               TEXT("#\tif 0\nCUT_ME\n#\telse\nKEEP_ME\n#\tendif\n"),        true },
		{ TEXT("trailing comments"),         TEXT("#if 1 // take\nKEEP_ME\n#else // otherwise\nCUT_ME\n#endif // done\n"), true },
		{ TEXT("trailing whitespace"),       TEXT("#if 1   \nKEEP_ME\n#else   \nCUT_ME\n#endif   \n"),     true },
	};

	for (const FSyntaxCase& Case : Cases)
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, Case.What, Case.Source, Defines, Result))
		{
			TestBranchTaken(*this, Case.What, Result.Text, Case.bExpectKeep);
			TestTrue(FString::Printf(TEXT("[%s] bHadDirectives"), Case.What), Result.bHadDirectives);
		}
	}

	// A line that starts with `//` is not a directive (3.1). The conditional is inert, so the text
	// between the two commented lines must survive -- an over-eager recogniser would delete it.
	{
		const FString Source = TEXT("// #if 0\nKEEP_ME\n// #endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("commented-out directives"), Source, Defines, Result))
		{
			TestEqual(TEXT("[commented-out directives] text passes through unchanged"), Result.Text, Source);
			TestFalse(TEXT("[commented-out directives] bHadDirectives"), Result.bHadDirectives);
		}
	}

	// Only whitespace may precede the '#' (3.1). With code in front of it, this is a line of source,
	// not a directive -- and it is certainly not an unclosed #if.
	{
		const FString Source = TEXT("float x = 1; # if 2\nKEEP_ME\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("# not at line start"), Source, Defines, Result))
		{
			TestEqual(TEXT("[# not at line start] text passes through unchanged"), Result.Text, Source);
			TestFalse(TEXT("[# not at line start] bHadDirectives"), Result.bHadDirectives);
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Diagnostics DSH1030..DSH1041.
//
// The CODE is asserted and the message is not. The code is the published identity that the docs, the
// diagnose skill, the corpus goldens and the editor extensions all key off; the message is prose that
// is expected to be reworded and is gathered into the zh-Hans target, so asserting it would make this
// test fail on a translated editor for no reason. (DreamShaderPureFunctionTests' TextWireUtils test
// exists because of exactly that failure mode.)
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorDiagnosticsTest,
	FDreamShaderPreprocessorQuietTestBase,
	"DreamShader.Lang.Preprocessor.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorDiagnosticsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FDiagnosticCase
	{
		const TCHAR* Code;
		const TCHAR* Source;
	};

	const FDiagnosticCase Cases[] =
	{
		// DSH1030 -- an #if still open when the file ends.
		{ TEXT("DSH1030"), TEXT("#if 1\nKEEP_ME\n") },
		{ TEXT("DSH1030"), TEXT("#if 1\n#if 1\n#endif\n") },
		{ TEXT("DSH1030"), TEXT("#if 0\n#else\n") },
		{ TEXT("DSH1030"), TEXT("#if 1") },

		// DSH1031 -- an #endif with nothing open.
		{ TEXT("DSH1031"), TEXT("#endif\n") },
		{ TEXT("DSH1031"), TEXT("#if 1\n#endif\n#endif\n") },
		{ TEXT("DSH1031"), TEXT("A\nB\n#endif\n") },

		// DSH1032 -- an #elif or #else with nothing open.
		{ TEXT("DSH1032"), TEXT("#else\n") },
		{ TEXT("DSH1032"), TEXT("#elif 1\n") },
		{ TEXT("DSH1032"), TEXT("#if 1\n#endif\n#else\n#endif\n") },
		{ TEXT("DSH1032"), TEXT("#if 1\n#endif\n#elif 1\n#endif\n") },

		// DSH1033 -- an #elif or a second #else after the #else.
		{ TEXT("DSH1033"), TEXT("#if 1\n#else\n#else\n#endif\n") },
		{ TEXT("DSH1033"), TEXT("#if 1\n#else\n#elif 1\n#endif\n") },
		// Also when the #if was false, i.e. when the else group is the LIVE one. The structure is
		// wrong either way; whether the group happens to be taken must not decide it.
		{ TEXT("DSH1033"), TEXT("#if 0\n#else\n#elif 1\n#endif\n") },

		// DSH1034 -- the condition is not a well-formed expression: the parse never FINISHED. A
		// missing operand, a missing ')', or a token that cannot begin a primary where one was
		// required. Contrast DSH1042 further down, where the expression parsed cleanly and something
		// was left over afterwards -- that boundary, not the shape of the offending token, is what
		// separates the two codes.
		{ TEXT("DSH1034"), TEXT("#if 1 +\n#endif\n") },
		{ TEXT("DSH1034"), TEXT("#if (1\n#endif\n") },
		{ TEXT("DSH1034"), TEXT("#if &&\n#endif\n") },
		{ TEXT("DSH1034"), TEXT("#if 1 && \n#endif\n") },
		{ TEXT("DSH1034"), TEXT("#if defined()\n#endif\n") },
		{ TEXT("DSH1034"), TEXT("#if defined(1)\n#endif\n") },
		{ TEXT("DSH1034"), TEXT("#if \"unterminated\n#endif\n") },
		{ TEXT("DSH1034"), TEXT("#if 0\n#elif 1 +\n#endif\n") },  // an #elif that IS reached is checked

		// DSH1035 -- a directive that is not one of the six.
		{ TEXT("DSH1035"), TEXT("#pragma once\n") },
		{ TEXT("DSH1035"), TEXT("#include \"Shared/Common.dsh\"\n") },
		{ TEXT("DSH1035"), TEXT("#elseif 1\n#endif\n") },
		// #ifdef and #ifndef ARE part of the syntax (see the IfdefAndIfndef test), which is exactly
		// what makes a recogniser that matches directive names by PREFIX rather than by whole word
		// accept these three by accident.
		{ TEXT("DSH1035"), TEXT("#ifdefx PP_ONE\n#endif\n") },
		{ TEXT("DSH1035"), TEXT("#ifdefine PP_ONE\n#endif\n") },
		{ TEXT("DSH1035"), TEXT("#ifndefined PP_ONE\n#endif\n") },
		// A keyword in the wrong case is NOT the keyword. Passing these through instead would mean
		// `#IF FOO` silently does nothing -- a conditional that never fires, with no error and no
		// output difference to notice it by. Region is the one directive matched case-insensitively,
		// and only because the parser already accepts it that way (see RegionDirectivesPassThrough).
		{ TEXT("DSH1035"), TEXT("#IF PP_ONE\n#endif\n") },
		{ TEXT("DSH1035"), TEXT("#ENDIF\n") },
		{ TEXT("DSH1035"), TEXT("#Endif\n") },
		{ TEXT("DSH1035"), TEXT("#Else\n") },
		{ TEXT("DSH1035"), TEXT("#Elif 1\n") },
		{ TEXT("DSH1035"), TEXT("#Define PP_X 1\n") },
		{ TEXT("DSH1035"), TEXT("#UNDEF PP_ONE\n") },
		{ TEXT("DSH1035"), TEXT("#IfDef PP_ONE\n#endif\n") },
		{ TEXT("DSH1035"), TEXT("#IFNDEF PP_ONE\n#endif\n") },
		// ...and a capitalised word that is neither a keyword nor a region directive gets no
		// benefit of the doubt either.
		{ TEXT("DSH1035"), TEXT("#Foo\n") },
		{ TEXT("DSH1035"), TEXT("#Bar 1\n") },
		{ TEXT("DSH1035"), TEXT("#Regions \"X\"\n") },

		// DSH1036 -- #if / #elif with no condition at all.
		{ TEXT("DSH1036"), TEXT("#if\n#endif\n") },
		{ TEXT("DSH1036"), TEXT("#if   \n#endif\n") },
		{ TEXT("DSH1036"), TEXT("#if 0\n#elif\n#endif\n") },
		// #ifdef / #ifndef with no operand is the same failure: the directive needs something to
		// test and did not get it.
		{ TEXT("DSH1036"), TEXT("#ifdef\n#endif\n") },
		{ TEXT("DSH1036"), TEXT("#ifndef   \n#endif\n") },

		// DSH1038 -- #define / #undef given a name that is not [A-Za-z_][A-Za-z0-9_]*.
		{ TEXT("DSH1038"), TEXT("#define 1BAD 1\n") },
		{ TEXT("DSH1038"), TEXT("#define BAD$NAME 1\n") },
		{ TEXT("DSH1038"), TEXT("#undef 2BAD\n") },
		{ TEXT("DSH1038"), TEXT("#undef BAD-NAME\n") },
		// A missing name is a NAME problem, not a condition problem. DSH1036 is for a missing
		// conditional EXPRESSION, and #define has no expression to be missing.
		{ TEXT("DSH1038"), TEXT("#define\n") },
		{ TEXT("DSH1038"), TEXT("#define   \n") },
		{ TEXT("DSH1038"), TEXT("#undef\n") },
		// #ifdef / #ifndef take a name, so a malformed one raises the name diagnostic, not DSH1034.
		{ TEXT("DSH1038"), TEXT("#ifdef 1BAD\n#endif\n") },
		{ TEXT("DSH1038"), TEXT("#ifndef BAD$NAME\n#endif\n") },

		// DSH1039 -- writing a read-only builtin. The DS_ prefix is reserved wholesale, so an
		// unknown DS_ name is refused too: reserving a prefix rather than a fixed list is what stops
		// a builtin added next year from silently losing to a define some project already shipped.
		{ TEXT("DSH1039"), TEXT("#define DS_FOO 1\n") },
		{ TEXT("DSH1039"), TEXT("#define DS_SUBSTRATE 0\n") },
		// #undef is an override attempt too. If it were allowed, a source could erase DS_SUBSTRATE
		// for itself and every later read would silently answer 0 -- which is the read-only rule
		// broken by the back door.
		{ TEXT("DSH1039"), TEXT("#undef DS_SUBSTRATE\n") },

		// DSH1040 -- a string used anywhere but as an operand of '==' / '!=' against another
		// string. The rule is strict on purpose: there is no truthiness for strings and no
		// implicit conversion in either direction, so a mistake is an error on the line that made
		// it instead of a branch that quietly resolves the wrong way in a shipped asset.
		{ TEXT("DSH1040"), TEXT("#if \"a\" < \"b\"\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if \"a\" > 1\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if \"a\" + 1\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT * 2\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT - 1\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if -\"a\"\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT <= PP_TEXT\n#endif\n") },

		// A bare string in a BOOLEAN position. Left undecided, this is the one an implementation
		// settles by accident -- "non-empty is true" and "not a number is false" are both plausible
		// and both silent -- and `#if DS_PLATFORM` is what someone writes on their first day.
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if \"hello\"\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if !PP_TEXT\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT && 1\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if 1 && PP_TEXT\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT || 0\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if 0 || PP_TEXT\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if 0\n#elif PP_TEXT\n#endif\n") },

		// A string against an integer, even through '==' / '!='. Answering false would be worse
		// than useless: `#if DS_PLATFORM == 1` would compile, be permanently false, and the branch
		// it guards would never be built by anybody.
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT == 1\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if 1 == PP_TEXT\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if PP_TEXT != 0\n#endif\n") },
		{ TEXT("DSH1040"), TEXT("#if \"a\" == 1\n#endif\n") },
		// PP_ONE holds "1", which parses as an integer -- so this is int-vs-string as well.
		{ TEXT("DSH1040"), TEXT("#if PP_ONE == \"1\"\n#endif\n") },

		// DSH1041 -- division or modulo by zero.
		{ TEXT("DSH1041"), TEXT("#if 1 / 0\n#endif\n") },
		{ TEXT("DSH1041"), TEXT("#if 1 % 0\n#endif\n") },
		{ TEXT("DSH1041"), TEXT("#if 1 / PP_ZERO\n#endif\n") },
		{ TEXT("DSH1041"), TEXT("#if 1 % PP_MISSING\n#endif\n") },  // an undefined name reads 0

		// DSH1042 -- the directive was complete and there was still something after it.
		//
		// Not DSH1034: telling someone their #undef has a malformed CONDITIONAL EXPRESSION, when it
		// has no condition at all, sends them looking in the wrong place. `#ifdef A B` desugars to
		// `#if defined(A) B`, so what is actually wrong is a leftover token -- and that is what the
		// code should say.
		//
		// The split is by what the USER has to do, not by which component threw. Every case here has
		// the same fix -- "the directive already ended; delete the extra or comment it out" -- and it
		// is the same fix whether the leftover is `B`, `2` or `)`. Two codes pointing at one repair
		// would be the failure mode, so the line is drawn at "did the expression finish", which is
		// mechanically decidable and does not care what the leftover looks like.
		{ TEXT("DSH1042"), TEXT("#undef PP_ONE junk\n") },
		{ TEXT("DSH1042"), TEXT("#ifdef PP_ONE junk\n#endif\n") },
		{ TEXT("DSH1042"), TEXT("#ifndef PP_ONE junk\n#endif\n") },
		{ TEXT("DSH1042"), TEXT("#ifdef PP_ONE A B\n#endif\n") },
		// The same shape after a complete condition. The desugaring argument makes this row
		// unavoidable: `#if defined(A) B` and `#if 1 2` differ only in the expression.
		{ TEXT("DSH1042"), TEXT("#if 1 2\n#endif\n") },
		{ TEXT("DSH1042"), TEXT("#if 0\n#elif 1 2\n#endif\n") },
		// A leftover ')' is a leftover token like any other. What decides the code is whether the
		// EXPRESSION FINISHED -- not whether the leftover happens to be a parenthesis.
		{ TEXT("DSH1042"), TEXT("#if 1)\n#endif\n") },
		{ TEXT("DSH1042"), TEXT("#if (1))\n#endif\n") },
		// ...and its near-twin on the other side of that line: here the ')' turns up where the right
		// operand of '&&' was due, so the expression never completed and it is DSH1034. Two rows,
		// one character apart, opposite codes -- and only the criterion tells them apart.
		{ TEXT("DSH1034"), TEXT("#if 1 &&)\n#endif\n") },
		// #else and #endif take no operand at all, so anything after them is left over under the
		// same general rule.
		{ TEXT("DSH1042"), TEXT("#if 1\n#else junk\n#endif\n") },
		{ TEXT("DSH1042"), TEXT("#if 1\n#endif junk\n") },
	};

	for (const FDiagnosticCase& Case : Cases)
	{
		RunPreprocessorExpectingCode(
			*this,
			FString::Printf(TEXT("%s <- %s"), Case.Code, Case.Source),
			Case.Source,
			Defines,
			Case.Code);
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Nesting depth (DSH1037).
//
// The limit exists so a pathological or generated file cannot recurse the evaluator into the stack.
// Both sides of the boundary are pinned: "exceeds 64" means 64 is legal.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorNestingDepthTest,
	FDreamShaderPreprocessorQuietTestBase,
	"DreamShader.Lang.Preprocessor.NestingDepthBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorNestingDepthTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	auto BuildNested = [](int32 Depth, const TCHAR* OpeningLine)
	{
		FString Source;
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Source += FString::Printf(TEXT("%s\n"), OpeningLine);
		}
		Source += TEXT("KEEP_ME\n");
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Source += TEXT("#endif\n");
		}
		return Source;
	};

	// 64 is the documented limit, so 64 must work.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("64 levels"), BuildNested(64, TEXT("#if 1")), Defines, Result))
		{
			TestTrue(TEXT("[64 levels] the innermost content survives"), Result.Text.Contains(TEXT("KEEP_ME")));
		}
	}

	// 65 exceeds it.
	RunPreprocessorExpectingCode(*this, TEXT("65 levels"), BuildNested(65, TEXT("#if 1")), Defines, TEXT("DSH1037"));

	// The limit counts structural nesting, not evaluation: a stack of skipped groups is exactly as
	// deep as a stack of taken ones, and the scanner has to keep tracking it either way.
	RunPreprocessorExpectingCode(*this, TEXT("65 levels, all skipped"), BuildNested(65, TEXT("#if 0")), Defines, TEXT("DSH1037"));

	// #ifdef and #ifndef open a level exactly like #if, so they count against the same budget. A
	// scanner that tracks depth only for the literal "#if" spelling would let a file nest without
	// limit through the sugar.
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("64 levels of #ifdef"), BuildNested(64, TEXT("#ifdef PP_ONE")), Defines, Result))
		{
			TestTrue(TEXT("[64 levels of #ifdef] the innermost content survives"), Result.Text.Contains(TEXT("KEEP_ME")));
		}
	}
	RunPreprocessorExpectingCode(*this, TEXT("65 levels of #ifdef"), BuildNested(65, TEXT("#ifdef PP_ONE")), Defines, TEXT("DSH1037"));
	RunPreprocessorExpectingCode(*this, TEXT("65 levels of #ifndef"), BuildNested(65, TEXT("#ifndef PP_MISSING")), Defines, TEXT("DSH1037"));
	RunPreprocessorExpectingCode(*this, TEXT("65 levels of dead #ifdef"), BuildNested(65, TEXT("#ifdef PP_MISSING")), Defines, TEXT("DSH1037"));

	return true;
}

// -------------------------------------------------------------------------------------------------
// An inactive branch is not validated (8.1) -- but its STRUCTURE still is.
//
// This is the one place the two halves pull in opposite directions, so it gets its own test. Cut is
// cut: whatever is inside a dead group is never parsed, never evaluated, and never diagnosed, exactly
// as in C. The price is that dead branches rot unnoticed, and that price was accepted.
//
// What is NOT skipped is the #if/#endif bookkeeping. The scanner has to keep counting through a dead
// region, or it cannot find where the region ends -- so an unbalanced file is still an error even
// when the imbalance is inside something that was cut.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorInactiveBranchTest,
	FDreamShaderPreprocessorQuietTestBase,
	"DreamShader.Lang.Preprocessor.InactiveBranchIsNotValidated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorInactiveBranchTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FToleratedCase
	{
		const TCHAR* What;
		const TCHAR* Source;
	};

	const FToleratedCase Tolerated[] =
	{
		// A nested condition that is not an expression at all.
		{ TEXT("garbage condition in a dead group"),  TEXT("#if 0\n#if @@@ !!! ###\nA\n#endif\n#endif\nKEEP_ME\n") },
		{ TEXT("empty condition in a dead group"),    TEXT("#if 0\n#if\nA\n#endif\n#endif\nKEEP_ME\n") },
		// A condition that WOULD raise a hard diagnostic if it were evaluated.
		{ TEXT("divide by zero in a dead group"),     TEXT("#if 0\n#if 1 / 0\nA\n#endif\n#endif\nKEEP_ME\n") },
		{ TEXT("type error in a dead group"),         TEXT("#if 0\n#if \"a\" < \"b\"\nA\n#endif\n#endif\nKEEP_ME\n") },
		// Directives that would otherwise be diagnosed on their own.
		{ TEXT("unknown directive in a dead group"),  TEXT("#if 0\n#totally_made_up\n#endif\nKEEP_ME\n") },
		{ TEXT("bad #define name in a dead group"),   TEXT("#if 0\n#define 1BAD 1\n#endif\nKEEP_ME\n") },
		{ TEXT("reserved #define in a dead group"),   TEXT("#if 0\n#define DS_FOO 1\n#endif\nKEEP_ME\n") },
		// The same, one level down in the #else group.
		{ TEXT("garbage in a dead else group"),       TEXT("#if 1\nKEEP_ME\n#else\n#if ((((\nA\n#endif\n#endif\n") },
		// ...and in an #elif group that can no longer be reached, for the same reason: the group's
		// condition is never evaluated once an earlier one has won, so it cannot fail either.
		{ TEXT("garbage in an unreachable #elif"),    TEXT("#if 1\nKEEP_ME\n#elif ((((\nA\n#endif\n") },
		// #ifdef / #ifndef inside a dead group are not checked either -- but they are still counted,
		// which the structural cases below hold them to.
		{ TEXT("bad #ifdef name in a dead group"),    TEXT("#if 0\n#ifdef 1BAD\nA\n#endif\n#endif\nKEEP_ME\n") },
		{ TEXT("nameless #ifndef in a dead group"),   TEXT("#if 0\n#ifndef\nA\n#endif\n#endif\nKEEP_ME\n") },
		{ TEXT("divide by zero in an unreachable #elif"), TEXT("#if 1\nKEEP_ME\n#elif 1 / 0\nA\n#endif\n") },
	};

	for (const FToleratedCase& Case : Tolerated)
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, Case.What, Case.Source, Defines, Result))
		{
			TestTrue(FString::Printf(TEXT("[%s] live content survives"), Case.What), Result.Text.Contains(TEXT("KEEP_ME")));
			TestFalse(FString::Printf(TEXT("[%s] dead content is gone"), Case.What), Result.Text.Contains(TEXT("\nA\n")));
		}
	}

	// ...but the nesting bookkeeping is not suspended inside a dead region.
	struct FStructuralCase
	{
		const TCHAR* Code;
		const TCHAR* Source;
	};

	const FStructuralCase Structural[] =
	{
		// The inner #if inside the dead group opened a level that is never closed.
		{ TEXT("DSH1030"), TEXT("#if 0\n#if 1\n#endif\n") },
		{ TEXT("DSH1030"), TEXT("#if 0\n#if @@@\n#endif\n") },
		// One #endif too many inside a dead group closes the outer group early, and the next one is
		// then stray -- the imbalance surfaces even though nothing in there was evaluated.
		{ TEXT("DSH1031"), TEXT("#if 0\n#endif\n#endif\n") },
		{ TEXT("DSH1031"), TEXT("#if 0\n#if 1\n#endif\n#endif\n#endif\n") },
		// The sugar opens and closes a level like anything else, dead group or not.
		{ TEXT("DSH1030"), TEXT("#if 0\n#ifdef PP_ONE\n#endif\n") },
		{ TEXT("DSH1030"), TEXT("#if 0\n#ifndef 1BAD\n#endif\n") },
		{ TEXT("DSH1031"), TEXT("#if 0\n#ifdef PP_ONE\n#endif\n#endif\n#endif\n") },
	};

	for (const FStructuralCase& Case : Structural)
	{
		RunPreprocessorExpectingCode(
			*this,
			FString::Printf(TEXT("structure still checked: %s"), Case.Source),
			Case.Source,
			Defines,
			Case.Code);
	}

	// A well-balanced dead region is fine, and its inner directives are blanked like any other.
	{
		const FString Source = TEXT("#if 0\n#if @@@ garbage\nA\n#endif\n#endif\nKEEP_ME\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("balanced dead region"), Source, Defines, Result))
		{
			const TArray<FString> Lines = SplitPreprocessorLines(Result.Text);
			if (TestEqual(TEXT("[balanced dead region] six lines out"), Lines.Num(), 6))
			{
				for (int32 Index = 0; Index < 5; ++Index)
				{
					TestTrue(
						FString::Printf(TEXT("[balanced dead region] line %d is blank"), Index),
						Lines[Index].TrimStartAndEnd().IsEmpty());
				}
				TestEqual(TEXT("[balanced dead region] live line survives"), Lines[5], FString(TEXT("KEEP_ME")));
			}
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Trailing tokens (DSH1042) -- and the shapes that look identical and are legal.
//
// The negative controls carry the weight here, which is why they get a test rather than a table row.
// `#define A B C` looks exactly like `#ifdef A B` and is perfectly legal: a #define's value runs to
// end of line, so `B C` IS the value. And a trailing `//` comment is never a trailing token, on any
// directive. An implementation that simply rejects "anything after the operand" passes every DSH1042
// row in the diagnostics table and breaks every case below.
//
// The other half of the boundary -- DSH1034 when the expression never finished, DSH1042 when it
// finished and something was left over -- is pinned by the adjacent `#if (1))` / `#if 1 &&)` pair in
// that table, where the two codes sit one character apart.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorTrailingTokenTest,
	FDreamShaderPreprocessorQuietTestBase,
	"DreamShader.Lang.Preprocessor.DirectiveTrailingTokens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorTrailingTokenTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	// The positives, kept beside their negatives so the contrast is readable in one place.
	RunPreprocessorExpectingCode(*this, TEXT("#undef with junk"), TEXT("#undef PP_ONE junk\n"), Defines, TEXT("DSH1042"));
	RunPreprocessorExpectingCode(*this, TEXT("#ifdef with junk"), TEXT("#ifdef PP_ONE junk\n#endif\n"), Defines, TEXT("DSH1042"));
	RunPreprocessorExpectingCode(*this, TEXT("#ifndef with junk"), TEXT("#ifndef PP_ONE junk\n#endif\n"), Defines, TEXT("DSH1042"));

	struct FLegalCase
	{
		const TCHAR* What;
		const TCHAR* Source;
	};

	const FLegalCase Legal[] =
	{
		// Two words after the name: the value is "B C", not an error.
		{ TEXT("#define with a two-word value"),  TEXT("#define PP_V B C\n#if PP_V == \"B C\"\nKEEP_ME\n#else\nCUT_ME\n#endif\n") },
		// A value is TEXT, not an expression: nothing in it is evaluated or expanded, so this is the
		// five-character string "1 + 1" and not the integer 2.
		{ TEXT("#define with an expression-shaped value"), TEXT("#define PP_SUM 1 + 1\n#if PP_SUM == \"1 + 1\"\nKEEP_ME\n#else\nCUT_ME\n#endif\n") },
		// A trailing comment is stripped before the value is taken, so this one is the integer 5.
		{ TEXT("#define with a trailing comment"), TEXT("#define PP_C 5 // five\n#if PP_C == 5\nKEEP_ME\n#else\nCUT_ME\n#endif\n") },
		// ...and on the directives that DO reject trailing tokens, a comment still is not one.
		{ TEXT("#undef with a trailing comment"),  TEXT("#undef PP_ONE // no longer wanted\n#if defined(PP_ONE)\nCUT_ME\n#else\nKEEP_ME\n#endif\n") },
		{ TEXT("#ifdef with a trailing comment"),  TEXT("#ifdef PP_ONE // present\nKEEP_ME\n#else\nCUT_ME\n#endif\n") },
		{ TEXT("#ifndef with a trailing comment"), TEXT("#ifndef PP_MISSING // absent\nKEEP_ME\n#else\nCUT_ME\n#endif\n") },
		{ TEXT("#else / #endif with comments"),    TEXT("#if 0\nCUT_ME\n#else // otherwise\nKEEP_ME\n#endif // done\n") },
	};

	for (const FLegalCase& Case : Legal)
	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, Case.What, Case.Source, Defines, Result))
		{
			TestBranchTaken(*this, Case.What, Result.Text, /*bExpectTrue*/ true);
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// #Region / #EndRegion pass through untouched.
//
// These are the decompiler's graph-layout annotations: PascalCase, handled by the parser, indented
// inside a Graph block -- which means they look exactly like a preprocessor directive to a scanner
// that only checks for a leading '#'. The preprocessor knows six lowercase keywords and nothing else,
// so a #Region must survive verbatim and must NOT be reported as an unknown directive (DSH1035).
//
// Getting this wrong breaks every decompiled .dsm in the project at once, because the decompiler
// emits #Region unconditionally.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorRegionPassThroughTest,
	"DreamShader.Lang.Preprocessor.RegionDirectivesPassThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorRegionPassThroughTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	// Shaped the way the decompiler actually writes it: two tabs of indent, a quoted name.
	const FString Source =
		TEXT("Shader(Name=\"M_Foo\")\n")
		TEXT("{\n")
		TEXT("\tGraph = {\n")
		TEXT("\t\t#Region \"Lighting\"\n")
		TEXT("\t\tfloat3 A = 1;\n")
		TEXT("\t\t#EndRegion\n")
		TEXT("\t}\n")
		TEXT("}\n");

	{
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("region only"), Source, Defines, Result))
		{
			TestEqual(TEXT("[region only] text passes through byte for byte"), Result.Text, Source);
			// A #Region is not a preprocessor directive, so a source that has only regions must not
			// trip the Adopt gate -- refusing to adopt every decompiled material would be the whole
			// feature made useless.
			TestFalse(TEXT("[region only] bHadDirectives is false"), Result.bHadDirectives);
			TestEqual(TEXT("[region only] nothing touched"), Result.TouchedDefines.Num(), 0);
		}
	}

	TestFalse(
		TEXT("a #Region-only source has no preprocessor directives"),
		DreamShaderSourceHasPreprocessorDirectives(Source));

	// The parser's IsGraphDirective compares with ESearchCase::IgnoreCase, so every spelling below is
	// legal in a Graph block TODAY. That is why `region` is matched case-insensitively while the eight
	// keywords are matched in lower case only -- this is a regression gate on shipped behaviour, not a
	// new tolerance being invented.
	//
	// The asymmetry is deliberate. Being case-insensitive about the KEYWORDS would let `#IF FOO`
	// through as inert text, and a conditional that silently never fires is worse than one that
	// errors: there is nothing to notice.
	{
		const TCHAR* const RegionSpellings[] =
		{
			TEXT("#Region \"X\""),
			TEXT("#region \"X\""),
			TEXT("#REGION \"X\""),
			TEXT("#ReGiOn \"X\""),
			TEXT("#Region\"X\""),
		};
		const TCHAR* const EndRegionSpellings[] =
		{
			TEXT("#EndRegion"),
			TEXT("#endregion"),
			TEXT("#ENDREGION"),
			TEXT("#EndREGION"),
			TEXT("#endRegion"),
		};

		const int32 SpellingCount = UE_ARRAY_COUNT(RegionSpellings);
		for (int32 Index = 0; Index < SpellingCount; ++Index)
		{
			const FString What = FString::Printf(TEXT("%s .. %s"), RegionSpellings[Index], EndRegionSpellings[Index]);
			const FString RegionSource = FString::Printf(
				TEXT("\t\t%s\n\t\tKEEP_ME\n\t\t%s\n"), RegionSpellings[Index], EndRegionSpellings[Index]);

			FDreamShaderPreprocessResult Result;
			if (RunPreprocessorExpectingSuccess(*this, What, RegionSource, Defines, Result))
			{
				TestEqual(FString::Printf(TEXT("[%s] passes through byte for byte"), *What), Result.Text, RegionSource);
				TestFalse(FString::Printf(TEXT("[%s] bHadDirectives stays false"), *What), Result.bHadDirectives);
				TestEqual(FString::Printf(TEXT("[%s] nothing touched"), *What), Result.TouchedDefines.Num(), 0);
			}

			TestFalse(
				FString::Printf(TEXT("[%s] cheap scan sees no directives"), *What),
				DreamShaderSourceHasPreprocessorDirectives(RegionSource));
		}
	}

	// Regions and conditionals coexist: the region lines survive their own physical lines while the
	// conditional around them does its cutting.
	{
		const FString Mixed =
			TEXT("#if 1\n")
			TEXT("\t\t#Region \"Kept\"\n")
			TEXT("\t\tKEEP_ME\n")
			TEXT("\t\t#EndRegion\n")
			TEXT("#else\n")
			TEXT("\t\t#Region \"Cut\"\n")
			TEXT("\t\tCUT_ME\n")
			TEXT("\t\t#EndRegion\n")
			TEXT("#endif\n");

		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("regions inside a conditional"), Mixed, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("regions inside a conditional"), Result.Text, /*bExpectTrue*/ true);
			TestTrue(TEXT("[regions inside a conditional] the live #Region survives"), Result.Text.Contains(TEXT("#Region \"Kept\"")));
			TestTrue(TEXT("[regions inside a conditional] the live #EndRegion survives"), Result.Text.Contains(TEXT("#EndRegion")));
			// The region in the dead branch is cut with everything else around it.
			TestFalse(TEXT("[regions inside a conditional] the dead #Region is gone"), Result.Text.Contains(TEXT("#Region \"Cut\"")));
			TestTrue(TEXT("[regions inside a conditional] bHadDirectives"), Result.bHadDirectives);
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Function / GraphFunction bodies are OPAQUE.
//
// A Function body is raw HLSL on its way to the shader compiler, and HLSL has its own `#if`, its own
// `#include` and its own `#pragma`. None of that belongs to this preprocessor: MF_MoonToonTranslucencyShadow.dsf
// branches on MATERIALBLENDING_SOLID, which is an engine macro this preprocessor has never heard of.
// Evaluate it here and it reads 0, every branch collapses to the #else, and the shipped material is
// silently wrong -- no error, no diagnostic, just a translucency path that stopped working.
//
// So inside a body every line passes through verbatim: not evaluated, not counted for nesting, not
// diagnosed, and not reported to the Adopt gate. That last part is what makes bHadDirectives and
// DreamShaderSourceHasPreprocessorDirectives usable at all -- almost every .dsf in the project has
// HLSL directives in a body, and a gate that fired on those would refuse Adopt for all of them.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorOpaqueBodyTest,
	FDreamShaderPreprocessorQuietTestBase,
	"DreamShader.Lang.Preprocessor.FunctionBodiesAreOpaque",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorOpaqueBodyTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FOpaqueCase
	{
		const TCHAR* What;
		const TCHAR* Source;
	};

	// Every one of these must come back byte-identical, with an empty touched set and
	// bHadDirectives false.
	const FOpaqueCase Cases[] =
	{
		// The shape MF_MoonToonTranslucencyShadow.dsf actually has: a signature spanning several
		// lines, the opening brace on its own, and an HLSL #if/#elif/#else chain inside.
		{
			TEXT("HLSL conditional in a multi-line Function"),
			TEXT("Function MoonToonTranslucencyShadow(\n")
			TEXT("\tin float3 Opaque,\n")
			TEXT("\tin float3 Masked,\n")
			TEXT("\tout float3 Result)\n")
			TEXT("{\n")
			TEXT("#if MATERIALBLENDING_SOLID\n")
			TEXT("\tResult = Opaque;\n")
			TEXT("#elif MATERIALBLENDING_MASKED\n")
			TEXT("\tResult = Masked;\n")
			TEXT("#else\n")
			TEXT("\tResult = 0;\n")
			TEXT("#endif\n")
			TEXT("}\n")
		},
		{
			TEXT("HLSL includes and pragmas in a Function"),
			TEXT("Function Noisy(in float2 uv, out float n)\n")
			TEXT("{\n")
			TEXT("#include \"/Engine/Private/Common.ush\"\n")
			TEXT("#pragma warning(disable : 3571)\n")
			TEXT("#define LOCAL_SCALE 4.0\n")
			TEXT("\tn = uv.x * LOCAL_SCALE;\n")
			TEXT("#undef LOCAL_SCALE\n")
			TEXT("}\n")
		},
		{
			TEXT("GraphFunction bodies are opaque too"),
			TEXT("GraphFunction WindPulse(in float2 uv, out float pulse)\n")
			TEXT("{\n")
			TEXT("#if WIND_ENABLED\n")
			TEXT("\tpulse = sin(uv.x);\n")
			TEXT("#endif\n")
			TEXT("}\n")
		},
		{
			TEXT("Function SelfContained bodies are opaque too"),
			TEXT("Function SelfContained Remap01(in float value, out float result)\n")
			TEXT("{\n")
			TEXT("#if 0\n")
			TEXT("\tresult = value;\n")
			TEXT("#endif\n")
			TEXT("\tresult = saturate(value * 0.5 + 0.5);\n")
			TEXT("}\n")
		},
		{
			TEXT("a Function with a return type"),
			TEXT("Function float Luma(in vec3 color)\n")
			TEXT("{\n")
			TEXT("#ifdef USE_REC709\n")
			TEXT("\treturn dot(color, float3(0.2126, 0.7152, 0.0722));\n")
			TEXT("#endif\n")
			TEXT("\treturn dot(color, float3(0.299, 0.587, 0.114));\n")
			TEXT("}\n")
		},
		// Nested braces inside the body: the tracker has to find the body's OWN closing brace, not
		// the first one it sees, or everything after an if-block leaves the opaque region early.
		{
			TEXT("nested braces inside a body"),
			TEXT("Function Branchy(in float x, out float y)\n")
			TEXT("{\n")
			TEXT("\tif (x > 0)\n")
			TEXT("\t{\n")
			TEXT("\t\ty = 1;\n")
			TEXT("\t}\n")
			TEXT("#if SOMETHING\n")
			TEXT("\ty = 2;\n")
			TEXT("#endif\n")
			TEXT("}\n")
		},
		// Spellings that are DSH1035 outside a body and nothing at all inside one.
		{
			TEXT("wrong-case and unknown directives in a body"),
			TEXT("Function Anything(out float y)\n")
			TEXT("{\n")
			TEXT("#IF UPPERCASE\n")
			TEXT("#Endif\n")
			TEXT("#totally_made_up\n")
			TEXT("#Foo\n")
			TEXT("\ty = 0;\n")
			TEXT("}\n")
		},
		// Unbalanced HLSL conditionals: an opaque region is not counted, so neither of these is a
		// DSH1030 or a DSH1031. A tracker that merely skips EVALUATION while still counting would
		// fail here, which is the whole point of the pair.
		{
			TEXT("unclosed HLSL #if in a body"),
			TEXT("Function Unclosed(out float y)\n")
			TEXT("{\n")
			TEXT("#if SOMETHING\n")
			TEXT("\ty = 0;\n")
			TEXT("}\n")
		},
		{
			TEXT("extra HLSL #endif in a body"),
			TEXT("Function Extra(out float y)\n")
			TEXT("{\n")
			TEXT("#if SOMETHING\n")
			TEXT("\ty = 0;\n")
			TEXT("#endif\n")
			TEXT("#endif\n")
			TEXT("}\n")
		},
	};

	for (const FOpaqueCase& Case : Cases)
	{
		const FString Source = Case.Source;
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, Case.What, Source, Defines, Result))
		{
			TestEqual(FString::Printf(TEXT("[%s] body passes through byte for byte"), Case.What), Result.Text, Source);
			TestFalse(FString::Printf(TEXT("[%s] bHadDirectives stays false"), Case.What), Result.bHadDirectives);
			TestEqual(FString::Printf(TEXT("[%s] nothing touched"), Case.What), Result.TouchedDefines.Num(), 0);
		}

		// The cheap scan uses the same classifier, so it has to agree -- it answers the same Adopt
		// gate, and the two disagreeing would mean Adopt behaved differently depending on which
		// entry point asked.
		TestFalse(
			FString::Printf(TEXT("[%s] cheap scan sees no directives"), Case.What),
			DreamShaderSourceHasPreprocessorDirectives(Source));
	}

	// A real conditional OUTSIDE the body still works, and the HLSL inside is carried along
	// untouched. This is the case that separates "opaque region" from "this file is exempt".
	{
		const FString Source =
			TEXT("#if PP_ONE\n")
			TEXT("Function Kept(out float y)\n")
			TEXT("{\n")
			TEXT("#if HLSL_ONLY\n")
			TEXT("\ty = 1;\n")
			TEXT("#endif\n")
			TEXT("}\n")
			TEXT("#endif\n");

		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("outer conditional keeps the Function"), Source, Defines, Result))
		{
			TestTrue(TEXT("[outer conditional] the Function survives"), Result.Text.Contains(TEXT("Function Kept")));
			TestTrue(TEXT("[outer conditional] its HLSL #if survives with it"), Result.Text.Contains(TEXT("#if HLSL_ONLY")));
			TestTrue(TEXT("[outer conditional] and its #endif"), Result.Text.Contains(TEXT("#endif")));
			TestTrue(TEXT("[outer conditional] bHadDirectives"), Result.bHadDirectives);
			// Only the outer name is a define read. HLSL_ONLY is body text, not a condition.
			TestTouchedValue(*this, TEXT("outer conditional"), Result, TEXT("PP_ONE"), TEXT("1"));
			TestNotTouched(*this, TEXT("outer conditional"), Result, TEXT("HLSL_ONLY"));
			TestEqual(TEXT("[outer conditional] one name touched"), Result.TouchedDefines.Num(), 1);
		}

		TestTrue(
			TEXT("the cheap scan does see the OUTER directive"),
			DreamShaderSourceHasPreprocessorDirectives(Source));
	}

	// ...and when the outer condition is false the whole Function goes, HLSL directives included.
	{
		const FString Source =
			TEXT("#if PP_ZERO\n")
			TEXT("Function Cut(out float y)\n")
			TEXT("{\n")
			TEXT("#if HLSL_ONLY\n")
			TEXT("\ty = 1;\n")
			TEXT("#endif\n")
			TEXT("}\n")
			TEXT("#endif\n")
			TEXT("KEEP_ME\n");

		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("outer conditional cuts the Function"), Source, Defines, Result))
		{
			TestFalse(TEXT("[outer cut] the Function is gone"), Result.Text.Contains(TEXT("Function Cut")));
			TestFalse(TEXT("[outer cut] its HLSL #if went with it"), Result.Text.Contains(TEXT("HLSL_ONLY")));
			TestTrue(TEXT("[outer cut] the live line survives"), Result.Text.Contains(TEXT("KEEP_ME")));
			TestNotTouched(*this, TEXT("outer cut"), Result, TEXT("HLSL_ONLY"));
		}
	}

	// Directives after the body closes are directives again -- the exemption ends with the brace.
	{
		const FString Source =
			TEXT("Function Body(out float y)\n")
			TEXT("{\n")
			TEXT("#if HLSL_ONLY\n")
			TEXT("\ty = 1;\n")
			TEXT("#endif\n")
			TEXT("}\n")
			TEXT("#if PP_ZERO\n")
			TEXT("CUT_ME\n")
			TEXT("#else\n")
			TEXT("KEEP_ME\n")
			TEXT("#endif\n");

		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("directives resume after the body"), Source, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("directives resume after the body"), Result.Text, /*bExpectTrue*/ true);
			TestTrue(TEXT("[after the body] the body's HLSL #if is still there"), Result.Text.Contains(TEXT("#if HLSL_ONLY")));
			TestTouchedValue(*this, TEXT("directives resume after the body"), Result, TEXT("PP_ZERO"), TEXT("0"));
			TestNotTouched(*this, TEXT("directives resume after the body"), Result, TEXT("HLSL_ONLY"));
		}
	}

	return true;
}
// -------------------------------------------------------------------------------------------------
// #define is FILE-LOCAL.
//
// The definition made by a source is visible to the rest of THAT source and to nothing else -- not to
// the file that imports it, and not to the next call. That is a deliberate divergence from C: it is
// the price of running before import inlining, which is what lets an #if wrap an Import line, and it
// drops C's include-order-dependent macro state on the floor along the way.
//
// At this layer the observable half is that the caller's table comes back untouched and that a second
// call starts from the same state as the first.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorFileLocalDefineTest,
	"DreamShader.Lang.Preprocessor.DefineIsFileLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorFileLocalDefineTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();
	const int32 EntriesBefore = Defines.Num();

	// A #define is visible to the rest of its own file.
	{
		const FString Source = TEXT("#define PP_NEW 7\n#if PP_NEW == 7\nKEEP_ME\n#else\nCUT_ME\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#define is visible below itself"), Source, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("#define is visible below itself"), Result.Text, /*bExpectTrue*/ true);
			TestTrue(TEXT("[#define is visible below itself] bHadDirectives"), Result.bHadDirectives);
		}
	}

	// ...but the caller's table is not modified by it.
	TestFalse(TEXT("the caller's table did not gain PP_NEW"), Defines.IsDefined(TEXT("PP_NEW")));
	TestEqual(TEXT("the caller's table has the same entry count"), Defines.Num(), EntriesBefore);

	// ...and the NEXT call starts clean. This is the assertion that catches a preprocessor that
	// caches its working table in a static: without it, whether a file compiles would depend on
	// which file was compiled before it.
	{
		const FString Source = TEXT("#if defined(PP_NEW)\nCUT_ME\n#else\nKEEP_ME\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("no leak into the next call"), Source, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("no leak into the next call"), Result.Text, /*bExpectTrue*/ true);
		}
	}

	// #undef of a table-provided name applies to this file only.
	{
		const FString Source = TEXT("#undef PP_ONE\n#if defined(PP_ONE)\nCUT_ME\n#else\nKEEP_ME\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#undef is file-local"), Source, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("#undef is file-local"), Result.Text, /*bExpectTrue*/ true);
		}
	}
	TestTrue(TEXT("the caller's table still defines PP_ONE"), Defines.IsDefined(TEXT("PP_ONE")));
	if (const FDreamShaderDefineEntry* Entry = Defines.Find(TEXT("PP_ONE")))
	{
		TestEqual(TEXT("the caller's PP_ONE still has its value"), Entry->Value, FString(TEXT("1")));
	}

	// Redefining a name the table already carries shadows it for this file. This is the mechanism a
	// single source uses to opt out of a project-wide default.
	{
		const FString Source = TEXT("#define PP_ONE 9\n#if PP_ONE == 9\nKEEP_ME\n#else\nCUT_ME\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#define shadows the table"), Source, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("#define shadows the table"), Result.Text, /*bExpectTrue*/ true);
		}
	}
	if (const FDreamShaderDefineEntry* Entry = Defines.Find(TEXT("PP_ONE")))
	{
		TestEqual(TEXT("the caller's PP_ONE survived the shadowing"), Entry->Value, FString(TEXT("1")));
	}

	// A valueless #define is a bare marker, exactly like `dsc -D FOO`: defined() sees it and
	// arithmetic reads 1.
	{
		const FString Source = TEXT("#define PP_BARE\n#if defined(PP_BARE) && PP_BARE == 1\nKEEP_ME\n#else\nCUT_ME\n#endif\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("valueless #define"), Source, Defines, Result))
		{
			TestBranchTaken(*this, TEXT("valueless #define"), Result.Text, /*bExpectTrue*/ true);
		}
	}

	// The directive lines themselves are blanked like every other directive.
	{
		const FString Source = TEXT("#define PP_X 1\n#undef PP_X\n");
		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, TEXT("#define / #undef lines are blanked"), Source, Defines, Result))
		{
			const TArray<FString> Lines = SplitPreprocessorLines(Result.Text);
			if (TestEqual(TEXT("[#define / #undef lines are blanked] two lines out"), Lines.Num(), 2))
			{
				TestTrue(TEXT("[#define / #undef lines are blanked] line 0"), Lines[0].TrimStartAndEnd().IsEmpty());
				TestTrue(TEXT("[#define / #undef lines are blanked] line 1"), Lines[1].TrimStartAndEnd().IsEmpty());
			}
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// bHadDirectives and DreamShaderSourceHasPreprocessorDirectives.
//
// Both answer the Adopt gate (DSH8149). Adopt rewrites a source from the graph in the generated
// asset, and the asset only ever holds the POST-CUT result -- so adopting a conditional source would
// silently delete its conditionals along with every branch that was not taken. A false negative here
// is unrecoverable data loss on the user's source file.
//
// The two entry points must also agree, which is why the same table drives both.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorHasDirectivesTest,
	FDreamShaderPreprocessorQuietTestBase,
	"DreamShader.Lang.Preprocessor.HasDirectives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorHasDirectivesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	const FDreamShaderDefineTable Defines = MakeExpressionFixtureTable();

	struct FHasDirectivesCase
	{
		const TCHAR* What;
		const TCHAR* Source;
		bool bHasDirectives;
		bool bPreprocessSucceeds;  // when true, bHadDirectives is asserted to agree
	};

	const FHasDirectivesCase Cases[] =
	{
		{ TEXT("empty"),                  TEXT(""),                               false, true  },
		{ TEXT("plain source"),           TEXT("Shader(Name=\"M\")\n{\n}\n"),     false, true  },
		{ TEXT("conditional"),            TEXT("#if 1\nA\n#endif\n"),             true,  true  },
		{ TEXT("conditional, false"),     TEXT("#if 0\nA\n#endif\n"),             true,  true  },
		// A directive is a directive whether or not its branch is taken -- "taken or not" is the
		// header's own wording, and the gate has to fire on a source whose #if is currently false
		// just as hard, because the branch it cut is exactly what Adopt would destroy.
		{ TEXT("#define only"),           TEXT("#define PP_X 1\n"),               true,  true  },
		{ TEXT("#undef only"),            TEXT("#undef PP_X\n"),                  true,  true  },
		{ TEXT("leading whitespace"),     TEXT("   #if 1\n#endif\n"),             true,  true  },
		{ TEXT("whitespace after #"),     TEXT("#  if 1\n#endif\n"),              true,  true  },
		{ TEXT("region only"),            TEXT("\t\t#Region \"R\"\n\t\t#EndRegion\n"), false, true },
		{ TEXT("region plus conditional"),TEXT("\t\t#Region \"R\"\n#if 1\n#endif\n"),  true,  true },
		{ TEXT("#ifdef"),                 TEXT("#ifdef PP_ONE\n#endif\n"),        true,  true  },
		{ TEXT("#ifndef"),                TEXT("#ifndef PP_ONE\n#endif\n"),       true,  true  },
		// The Adopt gate again: a Function body's HLSL directives belong to the shader compiler, not
		// to this preprocessor, so neither entry point may report them. Almost every .dsf in the
		// project has some, and a gate that fired on them would refuse Adopt for all of them.
		{ TEXT("HLSL directive in a Function body"), TEXT("Function F(out float y)\n{\n#if HLSL\n\ty = 1;\n#endif\n}\n"), false, true  },
		{ TEXT("wrong-case keyword"),      TEXT("#IF PP_ONE\n#ENDIF\n"),             true,  false },
		{ TEXT("commented-out directive"),TEXT("// #if 1\n// #endif\n"),          false, true  },
		{ TEXT("trailing comment only"),  TEXT("float x = 1; // #if 1\n"),        false, true  },
		{ TEXT("# not at line start"),    TEXT("float x = 1; # if 2\n"),          false, true  },

		// The cheap scan must answer for sources that would FAIL to preprocess -- that is the case
		// the header calls out by name. It never evaluates anything, so it cannot fail.
		{ TEXT("stray #endif"),           TEXT("#endif\n"),                       true,  false },
		{ TEXT("unclosed #if"),           TEXT("#if 1\nA\n"),                     true,  false },
		{ TEXT("unevaluable condition"),  TEXT("#if )))\n#endif\n"),              true,  false },
		{ TEXT("reserved #define"),       TEXT("#define DS_FOO 1\n"),             true,  false },
	};

	for (const FHasDirectivesCase& Case : Cases)
	{
		TestEqual(
			FString::Printf(TEXT("[%s] DreamShaderSourceHasPreprocessorDirectives"), Case.What),
			DreamShaderSourceHasPreprocessorDirectives(Case.Source) ? 1 : 0,
			Case.bHasDirectives ? 1 : 0);

		if (!Case.bPreprocessSucceeds)
		{
			continue;
		}

		FDreamShaderPreprocessResult Result;
		if (RunPreprocessorExpectingSuccess(*this, Case.What, Case.Source, Defines, Result))
		{
			TestEqual(
				FString::Printf(TEXT("[%s] bHadDirectives agrees with the cheap scan"), Case.What),
				Result.bHadDirectives ? 1 : 0,
				Case.bHasDirectives ? 1 : 0);
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// BuildDreamShaderDefineKeyFragment.
//
// This string goes into the asset's build key, so it has to be a FUNCTION of the map's content and of
// nothing else. Two failure modes, both silent:
//
//   * Not order-stable -> the same source hashes differently on the next run, every asset looks
//     stale, and the tree rebuilds itself forever.
//   * Not injective -> two DIFFERENT define sets collapse to one key, and an asset that should
//     rebuild does not. That is the exact data loss the sentinel exists to prevent, arriving by
//     another door.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorKeyFragmentTest,
	"DreamShader.Lang.Preprocessor.DefineKeyFragmentStability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorKeyFragmentTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	// Order independence. Enough keys that TMap's iteration order really does differ between the two
	// insertion orders -- with two or three entries it might coincide and prove nothing.
	{
		FDreamShaderDefineValueMap Forward;
		FDreamShaderDefineValueMap Reverse;
		for (int32 Index = 0; Index < 16; ++Index)
		{
			Forward.Add(FString::Printf(TEXT("PP_%02d"), Index), FString::Printf(TEXT("v%d"), Index));
		}
		for (int32 Index = 15; Index >= 0; --Index)
		{
			Reverse.Add(FString::Printf(TEXT("PP_%02d"), Index), FString::Printf(TEXT("v%d"), Index));
		}

		TestEqual(
			TEXT("insertion order does not change the fragment"),
			BuildDreamShaderDefineKeyFragment(Forward),
			BuildDreamShaderDefineKeyFragment(Reverse));
	}

	// Order independence when names differ ONLY by case. The map really does keep all three now that
	// FDreamShaderDefineValueMap compares keys case-sensitively -- but FString's own operators are
	// case-insensitive by default, so a plain Array.Sort() makes these three compare equal -- and
	// introsort is not stable, so their relative order would then follow the map's iteration order,
	// i.e. the insertion order. The fold has to break the tie itself.
	{
		const FDreamShaderDefineValueMap A = MakeTouchedMap({
			{ TEXT("Foo"), TEXT("1") },
			{ TEXT("FOO"), TEXT("2") },
			{ TEXT("foo"), TEXT("3") },
		});
		const FDreamShaderDefineValueMap B = MakeTouchedMap({
			{ TEXT("foo"), TEXT("3") },
			{ TEXT("FOO"), TEXT("2") },
			{ TEXT("Foo"), TEXT("1") },
		});

		TestEqual(
			TEXT("names differing only by case still fold deterministically"),
			BuildDreamShaderDefineKeyFragment(A),
			BuildDreamShaderDefineKeyFragment(B));
	}

	// Determinism within a run: same input, same output, twice.
	{
		const FDreamShaderDefineValueMap Map = MakeTouchedMap({ { TEXT("PP_A"), TEXT("1") }, { TEXT("PP_B"), TEXT("2") } });
		TestEqual(
			TEXT("the fragment is a pure function of the map"),
			BuildDreamShaderDefineKeyFragment(Map),
			BuildDreamShaderDefineKeyFragment(Map));
	}

	// Injectivity. Every pair below is two define sets that must NOT share a build key.
	auto AssertDistinct = [this](const TCHAR* Why, const FDreamShaderDefineValueMap& A, const FDreamShaderDefineValueMap& B)
	{
		const FString FragmentA = BuildDreamShaderDefineKeyFragment(A);
		const FString FragmentB = BuildDreamShaderDefineKeyFragment(B);
		TestTrue(
			FString::Printf(TEXT("fragments must differ (%s): '%s' vs '%s'"), Why, *FragmentA, *FragmentB),
			FragmentA != FragmentB);
	};

	AssertDistinct(TEXT("empty vs one entry"),
		MakeTouchedMap({}),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("1") } }));

	AssertDistinct(TEXT("same name, different value"),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("1") } }),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("2") } }));

	AssertDistinct(TEXT("different name, same value"),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("1") } }),
		MakeTouchedMap({ { TEXT("PP_B"), TEXT("1") } }));

	AssertDistinct(TEXT("an extra entry"),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("1") } }),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("1") }, { TEXT("PP_B"), TEXT("2") } }));

	// The sentinel's whole reason for existing: "read while undefined" is a different world state
	// from "never read", and the two must hash differently or the missed rebuild comes back.
	AssertDistinct(TEXT("sentinel entry vs no entry"),
		MakeTouchedMap({ { TEXT("PP_A"), GDreamShaderUndefinedDefineSentinel } }),
		MakeTouchedMap({}));

	// ...and "undefined" is not the same as "defined with an empty value", which is exactly the pair
	// a bare `dsc -D PP_A` flips between.
	AssertDistinct(TEXT("sentinel vs empty value"),
		MakeTouchedMap({ { TEXT("PP_A"), GDreamShaderUndefinedDefineSentinel } }),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("") } }));

	AssertDistinct(TEXT("empty value vs no entry"),
		MakeTouchedMap({ { TEXT("PP_A"), TEXT("") } }),
		MakeTouchedMap({}));

	// Names are case-sensitive, so two tables that differ only in a name's case are two tables.
	AssertDistinct(TEXT("names differ only by case"),
		MakeTouchedMap({ { TEXT("Foo"), TEXT("1") } }),
		MakeTouchedMap({ { TEXT("FOO"), TEXT("1") } }));

	// Separator injection. A define value may legitimately contain '=' and ';' -- `dsc -D A=1;B=2`
	// is a perfectly ordinary command line -- so a naive `Name + "=" + Value + ";"` fold collides
	// these two different define sets onto one key.
	AssertDistinct(TEXT("a value containing the separators"),
		MakeTouchedMap({ { TEXT("A"), TEXT("1") }, { TEXT("B"), TEXT("2") } }),
		MakeTouchedMap({ { TEXT("A"), TEXT("1;B=2") } }));

	// The same hazard with no separators at all: "AB" + "C" and "A" + "BC" both concatenate to "ABC".
	AssertDistinct(TEXT("name/value boundary is encoded"),
		MakeTouchedMap({ { TEXT("AB"), TEXT("C") } }),
		MakeTouchedMap({ { TEXT("A"), TEXT("BC") } }));

	return true;
}

// -------------------------------------------------------------------------------------------------
// FDreamShaderDefineTable and the two name predicates.
//
// Only the parts that need no process-global state: Set's refusal of reserved names, the name rules,
// the sorted order, and case sensitivity. Priority across the five tiers belongs with
// ResolveDreamShaderDefines and is not tested here.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorDefineTableTest,
	FDreamShaderPreprocessorQuietTestBase,
	"DreamShader.Lang.Preprocessor.DefineTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorDefineTableTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	// -- Set refuses a reserved name from every tier except Builtin ------------------------------
	//
	// The refusal lives in the table rather than in the resolver so that no caller can get around
	// the read-only rule simply by holding a table of its own -- which is exactly what a define
	// provider delegate is handed.
	{
		const EDreamShaderDefineSource NonBuiltinSources[] =
		{
			EDreamShaderDefineSource::Settings,
			EDreamShaderDefineSource::Registered,
			EDreamShaderDefineSource::Provider,
			EDreamShaderDefineSource::CommandLine,
		};

		for (const EDreamShaderDefineSource Source : NonBuiltinSources)
		{
			FDreamShaderDefineTable Table;
			const bool bSet = Table.Set(TEXT("DS_SUBSTRATE"), TEXT("1"), Source, TEXT("PreprocessorTests"));
			TestFalse(TEXT("Set refuses a reserved name from a non-builtin tier"), bSet);
			TestFalse(TEXT("the refused name was not added"), Table.IsDefined(TEXT("DS_SUBSTRATE")));
			TestEqual(TEXT("the table is untouched after a refusal"), Table.Num(), 0);
		}

		// An unknown DS_ name is refused too -- reserved is a prefix rule, not a list of the four
		// builtins that exist today.
		FDreamShaderDefineTable Table;
		TestFalse(TEXT("an unknown DS_ name is refused as well"),
			Table.Set(TEXT("DS_NOT_A_REAL_BUILTIN"), TEXT("1"), EDreamShaderDefineSource::CommandLine, TEXT("PreprocessorTests")));

		// Builtin is the one tier allowed to write them.
		TestTrue(TEXT("Builtin may set a reserved name"),
			Table.Set(TEXT("DS_SUBSTRATE"), TEXT("1"), EDreamShaderDefineSource::Builtin, TEXT("DreamShader")));
		TestTrue(TEXT("the builtin name is now defined"), Table.IsDefined(TEXT("DS_SUBSTRATE")));
	}

	// -- Bookkeeping ------------------------------------------------------------------------------
	{
		FDreamShaderDefineTable Table;
		TestTrue(TEXT("Set accepts an ordinary name"),
			Table.Set(TEXT("PP_A"), TEXT("1"), EDreamShaderDefineSource::Registered, TEXT("TagOne")));

		const FDreamShaderDefineEntry* Entry = Table.Find(TEXT("PP_A"));
		if (Entry == nullptr)
		{
			AddError(TEXT("PP_A should be findable after Set."));
			return false;
		}
		TestEqual(TEXT("value stored"), Entry->Value, FString(TEXT("1")));
		TestTrue(TEXT("source stored"), Entry->Source == EDreamShaderDefineSource::Registered);
		TestEqual(TEXT("source tag stored"), Entry->SourceTag, FString(TEXT("TagOne")));

		// Setting the same name again overwrites rather than duplicating.
		TestTrue(TEXT("Set overwrites"),
			Table.Set(TEXT("PP_A"), TEXT("2"), EDreamShaderDefineSource::CommandLine, TEXT("TagTwo")));
		TestEqual(TEXT("still one entry after overwrite"), Table.Num(), 1);
		if (const FDreamShaderDefineEntry* Overwritten = Table.Find(TEXT("PP_A")))
		{
			TestEqual(TEXT("overwritten value"), Overwritten->Value, FString(TEXT("2")));
			TestTrue(TEXT("overwritten source"), Overwritten->Source == EDreamShaderDefineSource::CommandLine);
			TestEqual(TEXT("overwritten tag"), Overwritten->SourceTag, FString(TEXT("TagTwo")));
		}

		TestTrue(TEXT("Find on a missing name is null"), Table.Find(TEXT("PP_NOPE")) == nullptr);
		TestFalse(TEXT("IsDefined on a missing name"), Table.IsDefined(TEXT("PP_NOPE")));

		Table.Remove(TEXT("PP_A"));
		TestEqual(TEXT("Remove drops the entry"), Table.Num(), 0);

		Table.Set(TEXT("PP_B"), TEXT("1"), EDreamShaderDefineSource::Settings, TEXT("Tag"));
		Table.Reset();
		TestEqual(TEXT("Reset empties the table"), Table.Num(), 0);
	}

	// -- Case sensitivity -------------------------------------------------------------------------
	//
	// `Foo` and `FOO` are two different defines, matching C and HLSL. This is the reason the key is
	// FString and not FName, and the reason the table pins its own case-sensitive key comparison
	// instead of inheriting whatever the engine's TMap<FString,...> currently does -- an inherited
	// answer would make the LANGUAGE's semantics an engine-version detail.
	{
		FDreamShaderDefineTable Table;
		Table.Set(TEXT("Foo"), TEXT("lower-ish"), EDreamShaderDefineSource::Settings, TEXT("Tag"));
		Table.Set(TEXT("FOO"), TEXT("upper"), EDreamShaderDefineSource::Settings, TEXT("Tag"));
		Table.Set(TEXT("foo"), TEXT("all-lower"), EDreamShaderDefineSource::Settings, TEXT("Tag"));

		TestEqual(TEXT("three names that differ only by case are three entries"), Table.Num(), 3);

		if (const FDreamShaderDefineEntry* Entry = Table.Find(TEXT("Foo")))
		{
			TestEqual(TEXT("Foo keeps its own value"), Entry->Value, FString(TEXT("lower-ish")));
		}
		if (const FDreamShaderDefineEntry* Entry = Table.Find(TEXT("FOO")))
		{
			TestEqual(TEXT("FOO keeps its own value"), Entry->Value, FString(TEXT("upper")));
		}
		if (const FDreamShaderDefineEntry* Entry = Table.Find(TEXT("foo")))
		{
			TestEqual(TEXT("foo keeps its own value"), Entry->Value, FString(TEXT("all-lower")));
		}

		Table.Remove(TEXT("FOO"));
		TestEqual(TEXT("Remove takes exactly one of the three"), Table.Num(), 2);
		TestTrue(TEXT("Remove left Foo alone"), Table.IsDefined(TEXT("Foo")));
		TestFalse(TEXT("Remove took FOO"), Table.IsDefined(TEXT("FOO")));
	}

	// -- GetSortedNames ---------------------------------------------------------------------------
	{
		FDreamShaderDefineTable Table;
		Table.Set(TEXT("Zeta"), TEXT("1"), EDreamShaderDefineSource::Settings, TEXT("Tag"));
		Table.Set(TEXT("Alpha"), TEXT("1"), EDreamShaderDefineSource::Settings, TEXT("Tag"));
		Table.Set(TEXT("Mu"), TEXT("1"), EDreamShaderDefineSource::Settings, TEXT("Tag"));
		Table.Set(TEXT("Beta"), TEXT("1"), EDreamShaderDefineSource::Settings, TEXT("Tag"));

		const TArray<FString> Names = Table.GetSortedNames();
		if (TestEqual(TEXT("all four names come back"), Names.Num(), 4))
		{
			TestEqual(TEXT("sorted[0]"), Names[0], FString(TEXT("Alpha")));
			TestEqual(TEXT("sorted[1]"), Names[1], FString(TEXT("Beta")));
			TestEqual(TEXT("sorted[2]"), Names[2], FString(TEXT("Mu")));
			TestEqual(TEXT("sorted[3]"), Names[3], FString(TEXT("Zeta")));
		}
	}
	{
		// "The only stable order" is the promise, so it has to hold for names a case-insensitive
		// comparator calls equal: those tie under FString's default operator<, and an unstable sort
		// then hands back whatever order the map iterated in.
		auto Build = [](bool bForward)
		{
			FDreamShaderDefineTable Table;
			const TCHAR* Names[] = { TEXT("Foo"), TEXT("FOO"), TEXT("foo"), TEXT("fOo") };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				const int32 Pick = bForward ? Index : (3 - Index);
				Table.Set(Names[Pick], TEXT("1"), EDreamShaderDefineSource::Settings, TEXT("Tag"));
			}
			return Table.GetSortedNames();
		};

		const TArray<FString> Forward = Build(true);
		const TArray<FString> Reverse = Build(false);

		if (TestEqual(TEXT("both orders yield four names"), Forward.Num(), Reverse.Num()))
		{
			for (int32 Index = 0; Index < Forward.Num(); ++Index)
			{
				TestEqual(
					FString::Printf(TEXT("GetSortedNames is insertion-order independent at [%d]"), Index),
					Forward[Index],
					Reverse[Index]);
			}
		}
	}

	// -- IsReservedDreamShaderDefineName -----------------------------------------------------------
	{
		struct FNameCase { const TCHAR* Name; bool bExpected; };
		const FNameCase Cases[] =
		{
			{ TEXT("DS_SUBSTRATE"),   true  },
			{ TEXT("DS_ANYTHING"),    true  },
			{ TEXT("DS_"),            true  },
			{ TEXT("DSX_FOO"),        false },  // the prefix is "DS_", not "DS"
			{ TEXT("DS"),             false },
			{ TEXT("D"),              false },
			{ TEXT("FOO_DS_BAR"),     false },  // a prefix rule, not a substring rule
			{ TEXT("_DS_FOO"),        false },
			// The prefix rule is CASE-SENSITIVE, like every other name comparison in this feature.
			// FString::StartsWith defaults to ESearchCase::IgnoreCase, so the obvious one-liner
			// reserves all three of these by accident -- and none of them can collide with a builtin,
			// every one of which is spelled in upper case.
			{ TEXT("ds_foo"),         false },
			{ TEXT("Ds_Foo"),         false },
			{ TEXT("dS_FOO"),         false },
			{ TEXT("PP_A"),           false },
			{ TEXT(""),               false },
		};

		for (const FNameCase& Case : Cases)
		{
			TestEqual(
				FString::Printf(TEXT("IsReservedDreamShaderDefineName('%s')"), Case.Name),
				IsReservedDreamShaderDefineName(Case.Name) ? 1 : 0,
				Case.bExpected ? 1 : 0);
		}
	}

	// -- IsValidDreamShaderDefineName --------------------------------------------------------------
	{
		struct FNameCase { const TCHAR* Name; bool bExpected; };
		const FNameCase Cases[] =
		{
			{ TEXT("A"),          true  },
			{ TEXT("_"),          true  },
			{ TEXT("_A"),         true  },
			{ TEXT("a1"),         true  },
			{ TEXT("FOO_BAR_9"),  true  },
			{ TEXT("Z9_"),        true  },
			{ TEXT("DS_FOO"),     true  },  // reserved, but syntactically valid: two separate rules
			{ TEXT(""),           false },
			{ TEXT("1A"),         false },  // may not start with a digit
			{ TEXT("9"),          false },
			{ TEXT("A-B"),        false },
			{ TEXT("A B"),        false },
			{ TEXT("A.B"),        false },
			{ TEXT("A$"),         false },
			{ TEXT("A#"),         false },
			{ TEXT(" A"),         false },  // leading whitespace is not trimmed away for you
			{ TEXT("A "),         false },
			{ TEXT("A\tB"),       false },
		};

		for (const FNameCase& Case : Cases)
		{
			TestEqual(
				FString::Printf(TEXT("IsValidDreamShaderDefineName('%s')"), Case.Name),
				IsValidDreamShaderDefineName(Case.Name) ? 1 : 0,
				Case.bExpected ? 1 : 0);
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// GetBuiltinDreamShaderDefines.
//
// Light coverage of the one rule that cannot be recovered from later: a builtin must be invariant for
// the lifetime of the process. Its value is evaluated once at generation time and then baked into a
// saved asset, so a builtin that can move mid-session makes both the build reproducible-in-name-only
// and the asset's recorded build key a lie about what produced it.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPreprocessorBuiltinDefinesTest,
	"DreamShader.Lang.Preprocessor.BuiltinDefines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPreprocessorBuiltinDefinesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::Preprocessor;

	FDreamShaderDefineTable First;
	GetBuiltinDreamShaderDefines(First);

	// It ADDS to the table it is handed rather than clearing it first. That is what lets
	// ResolveDreamShaderDefines lay the builtins down and then have each later tier write into the
	// same table -- a Reset in here would silently discard whatever the caller had already put in.
	{
		FDreamShaderDefineTable Merged;
		Merged.Set(TEXT("PP_PRE_EXISTING"), TEXT("keep me"), EDreamShaderDefineSource::Settings, TEXT("PreprocessorTests"));
		GetBuiltinDreamShaderDefines(Merged);

		TestTrue(TEXT("a pre-existing entry survives the call"), Merged.IsDefined(TEXT("PP_PRE_EXISTING")));
		if (const FDreamShaderDefineEntry* Entry = Merged.Find(TEXT("PP_PRE_EXISTING")))
		{
			TestEqual(TEXT("...with its value intact"), Entry->Value, FString(TEXT("keep me")));
		}
		TestTrue(TEXT("and the builtins were added alongside it"), Merged.IsDefined(TEXT("DS_PLATFORM")));
		TestEqual(TEXT("the merged table is the builtins plus the one that was there"), Merged.Num(), First.Num() + 1);
	}

	// The four documented facts (6.1). Asserted by presence, not by exhaustiveness, so adding a
	// fifth builtin later does not fail this test.
	const TCHAR* const Required[] =
	{
		TEXT("DS_ENGINE_MAJOR"),
		TEXT("DS_ENGINE_MINOR"),
		TEXT("DS_SUBSTRATE"),
		TEXT("DS_PLATFORM"),
		TEXT("DS_PLUGIN_VERSION"),
	};

	for (const TCHAR* Name : Required)
	{
		TestTrue(FString::Printf(TEXT("builtin '%s' exists"), Name), First.IsDefined(Name));
	}

	// Every builtin is under the reserved prefix and is tagged as Builtin -- otherwise Set would
	// have refused it, and a later tier could overwrite it.
	for (const FString& Name : First.GetSortedNames())
	{
		TestTrue(FString::Printf(TEXT("builtin '%s' is a reserved name"), *Name), IsReservedDreamShaderDefineName(Name));
		TestTrue(FString::Printf(TEXT("builtin '%s' is a valid name"), *Name), IsValidDreamShaderDefineName(Name));
		if (const FDreamShaderDefineEntry* Entry = First.Find(Name))
		{
			TestTrue(
				FString::Printf(TEXT("builtin '%s' is tagged Builtin"), *Name),
				Entry->Source == EDreamShaderDefineSource::Builtin);
		}
	}

	// Shapes the conditions in the design's own worked example rely on.
	if (const FDreamShaderDefineEntry* Entry = First.Find(TEXT("DS_SUBSTRATE")))
	{
		TestTrue(
			FString::Printf(TEXT("DS_SUBSTRATE is 0 or 1 (was '%s')"), *Entry->Value),
			Entry->Value == TEXT("0") || Entry->Value == TEXT("1"));
	}
	if (const FDreamShaderDefineEntry* Entry = First.Find(TEXT("DS_PLATFORM")))
	{
		TestFalse(TEXT("DS_PLATFORM is not empty"), Entry->Value.IsEmpty());
	}
	if (const FDreamShaderDefineEntry* Entry = First.Find(TEXT("DS_ENGINE_MAJOR")))
	{
		TestTrue(TEXT("DS_ENGINE_MAJOR is a decimal integer"), Entry->Value.IsNumeric() && !Entry->Value.IsEmpty());
	}
	if (const FDreamShaderDefineEntry* Entry = First.Find(TEXT("DS_ENGINE_MINOR")))
	{
		TestTrue(TEXT("DS_ENGINE_MINOR is a decimal integer"), Entry->Value.IsNumeric() && !Entry->Value.IsEmpty());
	}

	// THE HARD RULE: recomputing gives the same answer. A builtin that can move between two calls in
	// one process is a build that cannot be reproduced.
	FDreamShaderDefineTable Second;
	GetBuiltinDreamShaderDefines(Second);

	const TArray<FString> FirstNames = First.GetSortedNames();
	const TArray<FString> SecondNames = Second.GetSortedNames();
	if (TestEqual(TEXT("the builtin set has the same size on a second call"), FirstNames.Num(), SecondNames.Num()))
	{
		for (int32 Index = 0; Index < FirstNames.Num(); ++Index)
		{
			TestEqual(
				FString::Printf(TEXT("builtin name [%d] is unchanged"), Index),
				FirstNames[Index],
				SecondNames[Index]);

			const FDreamShaderDefineEntry* A = First.Find(FirstNames[Index]);
			const FDreamShaderDefineEntry* B = Second.Find(FirstNames[Index]);
			if (A != nullptr && B != nullptr)
			{
				TestEqual(
					FString::Printf(TEXT("builtin '%s' is unchanged"), *FirstNames[Index]),
					A->Value,
					B->Value);
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
