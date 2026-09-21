// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.Format.* -- FormatDreamShaderLangSource (Lang/LangFormat.h), the engine-free half of `dsc fmt`.
//
// The formatter is the printer, and the printer has its own tests. What is tested here is the part that is the
// formatter's alone: what it answers for each kind of file, that a second format changes nothing, that the source's
// line terminator and comments survive, and that the files it must not touch are left alone with a reason.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Lang/LangFormat.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::LangFormatTests
{
	using namespace UE::DreamShader::Lang;

	struct FFormatRun
	{
		ELangFormatOutcome Outcome = ELangFormatOutcome::Failed;
		FString Text;
		FLangDiagnosticSink Diagnostics;

		bool HasCode(const TCHAR* Code) const
		{
			for (const FLangDiagnostic& Diagnostic : Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
			return false;
		}
	};

	inline void Format(FFormatRun& Run, const TCHAR* Path, const FString& Text)
	{
		Run.Outcome = FormatDreamShaderLangSource(FLangSourceText(Path, Text), FLangFormatOptions(), Run.Text, Run.Diagnostics);
	}

	/** Written the way nobody formats: no indentation, several statements to a line, spaces nowhere and everywhere. */
	static const TCHAR* const GMessy = TEXT(
		"// A tint, and how much of it.\n"
		"uniform   float3 Tint=float3(1,0.5,0.25);\n"
		"uniform float Gain   = 0.5 ;   // per material\n"
		"\n"
		"\n"
		"\n"
		"float Half( float x ){return x*0.5;}\n"
		"export void M_Format(inout material m)\n"
		"{\n"
		"float3 Color=Tint*Half(Gain);  /* the look */\n"
		"      if(Gain>0.5){Color=Color*2.0;}\n"
		"m.EmissiveColor=Color;\n"
		"}\n");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLangFormatOutcomesTest,
	"DreamShader.Lang2.Format.Outcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLangFormatOutcomesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangFormatTests;

	// ----- a messy file is rewritten, once
	FFormatRun First;
	Format(First, TEXT("Format.dss"), GMessy);
	if (!TestEqual(TEXT("a messy file is Changed"), LexToString(First.Outcome), LexToString(ELangFormatOutcome::Changed)))
	{
		for (const FLangDiagnostic& Diagnostic : First.Diagnostics.GetDiagnostics())
		{
			AddInfo(FLangDiagnosticSink::ToWireString(Diagnostic));
		}
		return false;
	}
	TestFalse(TEXT("...and says nothing about it"), First.Diagnostics.HasErrors());
	TestTrue(TEXT("the comment above a declaration is kept"), First.Text.Contains(TEXT("// A tint, and how much of it.")));
	TestTrue(TEXT("a trailing comment is kept"), First.Text.Contains(TEXT("// per material")));
	TestTrue(TEXT("a block comment inside a body is kept"), First.Text.Contains(TEXT("/* the look */")));
	TestTrue(TEXT("a body is indented"), First.Text.Contains(TEXT("\n    float3 Color = Tint * Half(Gain);")));
	TestFalse(TEXT("three blank lines are one"), First.Text.Contains(TEXT("\n\n\n")));
	AddInfo(First.Text);

	FFormatRun Second;
	Format(Second, TEXT("Format.dss"), First.Text);
	TestEqual(TEXT("formatting the result changes nothing"), LexToString(Second.Outcome), LexToString(ELangFormatOutcome::Unchanged));
	TestEqual(TEXT("...and hands the same text back"), Second.Text, First.Text);

	// ----- the source's own line terminator
	FFormatRun Crlf;
	Format(Crlf, TEXT("Format.dss"), FString(GMessy).Replace(TEXT("\n"), TEXT("\r\n")));
	if (TestEqual(TEXT("a CRLF file is Changed too"), LexToString(Crlf.Outcome), LexToString(ELangFormatOutcome::Changed)))
	{
		TestTrue(TEXT("...and stays CRLF"), Crlf.Text.Contains(TEXT("\r\n")));
		TestFalse(TEXT("...on every line"), Crlf.Text.Replace(TEXT("\r\n"), TEXT("")).Contains(TEXT("\n")));
		TestEqual(TEXT("...with the same text otherwise"), Crlf.Text.Replace(TEXT("\r\n"), TEXT("\n")), First.Text);
	}

	// ----- an instance source is 2.0 text like any other
	FFormatRun Instance;
	Format(Instance, TEXT("MI_Format.dsi"), TEXT("#pragma instance(Parent=\"/Game/M_Parent\")\nuniform float   Gain=0.25;\n"));
	TestEqual(TEXT("a .dsi is formatted"), LexToString(Instance.Outcome), LexToString(ELangFormatOutcome::Changed));

	// ----- what is left alone, and why
	FFormatRun Legacy;
	Format(Legacy, TEXT("M_Legacy.dsm"), TEXT("Shader(Name=\"M_Legacy\")\n{\n    Outputs = { vec3 C; Base.EmissiveColor = C; }\n    Graph = { C = vec3(1.0, 0.0, 0.0); }\n}\n"));
	TestEqual(TEXT("a 1.x source is Skipped"), LexToString(Legacy.Outcome), LexToString(ELangFormatOutcome::Skipped));
	TestTrue(TEXT("...as migrate's business (DSH9042)"), Legacy.HasCode(TEXT("DSH9042")));
	TestFalse(TEXT("...which is no error"), Legacy.Diagnostics.HasErrors());
	TestTrue(TEXT("...and hands no text back"), Legacy.Text.IsEmpty());

	FFormatRun Conditional;
	Format(Conditional, TEXT("Format.dss"), TEXT("#if DS_SUBSTRATE\nuniform float A = 1;\n#else\nuniform float A = 2;\n#endif\n"));
	TestEqual(TEXT("a file that uses #if is Skipped"), LexToString(Conditional.Outcome), LexToString(ELangFormatOutcome::Skipped));
	TestTrue(TEXT("...because of the preprocessor (DSH9043)"), Conditional.HasCode(TEXT("DSH9043")));
	TestFalse(TEXT("...which is no error either"), Conditional.Diagnostics.HasErrors());

	// A `#` line inside a custom body is the body's own, and the file is formatted around it.
	FFormatRun Custom;
	Format(Custom, TEXT("Format.dss"), TEXT(
		"/// @custom\n"
		"float Pick(float x)\n"
		"{\n"
		"#if 1\n"
		"    return x;\n"
		"#else\n"
		"    return 0;\n"
		"#endif\n"
		"}\n"
		"export   void M_Custom(inout material m){m.Opacity=Pick(1.0);}\n"));
	if (TestEqual(TEXT("a #if inside a custom body does not stop the format"), LexToString(Custom.Outcome), LexToString(ELangFormatOutcome::Changed)))
	{
		TestTrue(TEXT("...and the body is written as it was"), Custom.Text.Contains(TEXT("#if 1\n    return x;\n#else\n    return 0;\n#endif")));
	}

	FFormatRun Broken;
	Format(Broken, TEXT("Format.dss"), TEXT("uniform float A = ;\n"));
	TestEqual(TEXT("a file that does not parse is Failed"), LexToString(Broken.Outcome), LexToString(ELangFormatOutcome::Failed));
	TestTrue(TEXT("...with the parser's own error"), Broken.Diagnostics.HasErrors() && !Broken.HasCode(TEXT("DSH9044")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
