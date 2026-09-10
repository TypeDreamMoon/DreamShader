// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Printer tests for the DreamShaderLang 2.0 front end -- DreamShader.Lang2.Printer.*
//
// The printer's contract (Public/Lang/LangPrinter.h) is STRUCTURAL fidelity, not textual: the tree
// that comes out of parse(print(parse(X))) is the tree parse(X) produced. That is not directly
// observable without a tree comparison, so what is asserted here is the observable consequence --
// the printer reaches a FIXED POINT after one iteration:
//
//     print(parse(X)) == print(parse(print(parse(X))))     byte for byte
//
// Anything the printer normalises (an inserted parenthesis, a `///` block re-ordered into free text
// then directives, a single-statement branch body given braces, `uniform float a, b;` split into
// two declarations) changes the text on the FIRST print and never again. A second print that
// differs from the first means the printer emitted something it cannot read back -- exactly the
// class of bug that would silently rewrite a user's material on the first editor round trip.
//
// Three layers:
//   RoundTrip.*   fixed point on inline sources: the proposal's section 7 material and section 8
//                 function, a struct, an opaque `/// @custom` body, control flow, precedence.
//   Expressions   spot checks on the exact spelling of a printed expression (the parenthesis rules).
//   Corpus        every non-".bad." fixture under Tests/Corpus/Lang reaches the same fixed point.
//
// The tests cannot run until the whole DreamShaderLang module compiles (M1's combined build pass).

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderTestCommon.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

namespace UE::DreamShader::Editor::Private::LangPrinterTests
{
	using namespace UE::DreamShader::Lang;

	// ---------------------------------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------------------------------

	/** `DSHnnnn: message` for the first error of a sink, or a placeholder when there is none. */
	static FString DescribeFirstError(const FLangDiagnosticSink& Diagnostics)
	{
		if (const FLangDiagnostic* Error = Diagnostics.FirstError())
		{
			return FLangDiagnosticSink::ToWireString(*Error);
		}
		return TEXT("<no diagnostic>");
	}

	/** The line where two printed texts first differ, rendered for a failure message. */
	static FString DescribeFirstDifference(const FString& Left, const FString& Right)
	{
		const int32 Common = FMath::Min(Left.Len(), Right.Len());

		int32 Index = 0;
		while (Index < Common && Left[Index] == Right[Index])
		{
			++Index;
		}

		int32 Line = 1;
		int32 LineStart = 0;
		for (int32 Scan = 0; Scan < Index; ++Scan)
		{
			if (Left[Scan] == TEXT('\n'))
			{
				++Line;
				LineStart = Scan + 1;
			}
		}

		auto LineAt = [](const FString& Text, const int32 Start)
		{
			if (Start >= Text.Len())
			{
				return FString(TEXT("<end of text>"));
			}
			int32 End = Start;
			while (End < Text.Len() && Text[End] != TEXT('\n'))
			{
				++End;
			}
			return Text.Mid(Start, End - Start).Replace(TEXT("\r"), TEXT(""));
		};

		return FString::Printf(
			TEXT("first difference at line %d (offset %d):\n    first  print: %s\n    second print: %s"),
			Line,
			Index,
			*LineAt(Left, LineStart),
			*LineAt(Right, LineStart));
	}

	/**
	 * parse -> print -> parse -> print, asserting the two printed texts are byte-identical and that
	 * neither parse reported an error. OutFirstPrint receives the first print so a caller can assert
	 * on the layout itself (indentation, `in` omission, one directive per line, ...).
	 */
	static bool RunRoundTrip(
		FAutomationTestBase& Test,
		const FString& Label,
		const FString& Path,
		const FString& SourceText,
		FString* OutFirstPrint = nullptr)
	{
		const FLangSourceText FirstSource(Path, SourceText);
		FLangParseResult FirstParse = ParseDreamShaderLang(FirstSource);

		if (!FirstParse.Module.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("[%s] the first parse produced no module."), *Label));
			return false;
		}
		if (FirstParse.Diagnostics.HasErrors())
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] the source must parse cleanly, but the first parse failed: %s"),
				*Label,
				*DescribeFirstError(FirstParse.Diagnostics)));
			return false;
		}

		const FString FirstPrint = PrintDreamShaderLang(*FirstParse.Module);
		if (OutFirstPrint)
		{
			*OutFirstPrint = FirstPrint;
		}

		Test.TestTrue(
			FString::Printf(TEXT("[%s] the printed text is not empty"), *Label),
			!FirstPrint.IsEmpty() || FirstParse.Module->Declarations.Num() == 0);

		const FLangSourceText SecondSource(Path, FirstPrint);
		FLangParseResult SecondParse = ParseDreamShaderLang(SecondSource);

		if (!SecondParse.Module.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("[%s] the printed text produced no module."), *Label));
			return false;
		}
		if (SecondParse.Diagnostics.HasErrors())
		{
			// The printer wrote something it cannot read back: show the text, not just the code.
			Test.AddError(FString::Printf(
				TEXT("[%s] the printed text does not parse: %s\n--- printed ---\n%s--- end ---"),
				*Label,
				*DescribeFirstError(SecondParse.Diagnostics),
				*FirstPrint));
			return false;
		}

		const FString SecondPrint = PrintDreamShaderLang(*SecondParse.Module);

		const bool bIdentical = SecondPrint.Equals(FirstPrint, ESearchCase::CaseSensitive);
		Test.TestTrue(
			FString::Printf(TEXT("[%s] print(parse(print(x))) is byte-identical to print(parse(x))"), *Label),
			bIdentical);
		if (!bIdentical)
		{
			Test.AddError(FString::Printf(TEXT("[%s] %s"), *Label, *DescribeFirstDifference(FirstPrint, SecondPrint)));
		}

		Test.TestEqual(
			FString::Printf(TEXT("[%s] the declaration count survives the round trip"), *Label),
			SecondParse.Module->Declarations.Num(),
			FirstParse.Module->Declarations.Num());

		return bIdentical;
	}

	/**
	 * TestTrue that Printed contains Needle, case-sensitively, with the haystack in the message.
	 *
	 * `\r` is stripped first: the printer's own line terminator is `\n`, but an opaque `/// @custom`
	 * body is a verbatim slice of the source, so it carries whatever the source used -- and whether
	 * a raw string literal in THIS file keeps its CRLF is a compiler detail, not a printer rule.
	 */
	static void TestContains(FAutomationTestBase& Test, const FString& Label, const FString& Printed, const TCHAR* Needle)
	{
		const FString Normalized = Printed.Replace(TEXT("\r"), TEXT(""));
		const bool bFound = Normalized.Contains(Needle, ESearchCase::CaseSensitive);
		Test.TestTrue(FString::Printf(TEXT("[%s] printed text contains \"%s\""), *Label, Needle), bFound);
		if (!bFound)
		{
			Test.AddError(FString::Printf(TEXT("[%s] --- printed ---\n%s--- end ---"), *Label, *Printed));
		}
	}

	/**
	 * Parses one expression, prints it, and asserts the exact spelling -- then prints it a second
	 * time through a re-parse, because an expression printer that adds a parenthesis must add the
	 * same one twice.
	 */
	static void TestExpression(FAutomationTestBase& Test, const TCHAR* SourceText, const TCHAR* Expected)
	{
		const FString Path(TEXT("Inline/Expression.dss"));

		FLangDiagnosticSink Diagnostics(Path);
		const FLangSourceText Source(Path, FString(SourceText));
		const FExprPtr Expression = ParseDreamShaderLangExpression(Source, Diagnostics);

		if (!Expression.IsValid() || Diagnostics.HasErrors())
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] the expression must parse cleanly: %s"),
				SourceText,
				*DescribeFirstError(Diagnostics)));
			return;
		}

		const FString Printed = PrintDreamShaderLangExpr(*Expression);
		Test.TestTrue(
			FString::Printf(TEXT("print(parse(\"%s\")) == \"%s\" (actual: \"%s\")"), SourceText, Expected, *Printed),
			Printed.Equals(Expected, ESearchCase::CaseSensitive));

		FLangDiagnosticSink ReparseDiagnostics(Path);
		const FLangSourceText Reparsed(Path, Printed);
		const FExprPtr Again = ParseDreamShaderLangExpression(Reparsed, ReparseDiagnostics);

		if (!Again.IsValid() || ReparseDiagnostics.HasErrors())
		{
			Test.AddError(FString::Printf(
				TEXT("[%s] the printed expression \"%s\" does not parse back: %s"),
				SourceText,
				*Printed,
				*DescribeFirstError(ReparseDiagnostics)));
			return;
		}

		Test.TestTrue(
			FString::Printf(TEXT("[%s] the expression printer is idempotent"), SourceText),
			PrintDreamShaderLangExpr(*Again).Equals(Printed, ESearchCase::CaseSensitive));
	}

	// ---------------------------------------------------------------------------------------------
	// Inline sources
	// ---------------------------------------------------------------------------------------------

	/** The proposal's section 7 material, verbatim (Plan/syntax-v2-proposal.md). */
	static FString MakeMaterialSource()
	{
		return TEXT(R"DSS(// Soft radial glow with a tighter hot core and a slow breathing pulse.

#pragma material(ShadingModel = Unlit, BlendMode = Additive, bUsedWithNiagaraSprites = true)

/// @group Glow|Look   @desc Multiplied on top of the particle colour
uniform float4 Tint = float4(1, 1, 1, 1);

/// @group Glow|Look   @desc Overall emissive gain
uniform float Intensity = 0.7;

/// @group Glow|Motion @desc Amplitude of the slow breathing pulse
uniform float Breathe = 0.10;

float GlowMask(float2 UV, float Time, float Amount)
{
    float2 P  = UV * 2.0 - 1.0;
    float R2  = dot(P, P);
    float Glow  = pow(saturate(1.0 - R2), 2.4);
    float Hot   = pow(saturate(1.0 - R2 * 2.6), 5.0) * 0.9;
    float Pulse = 1.0 - Amount + Amount * sin(Time * 2.1);
    return (Glow + Hot) * Pulse;
}

export void M_TeleportGlow(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float4 VC = UE.VertexColor();

    m.EmissiveColor = Tint.rgb * VC.rgb * GlowMask(UV, UE.Time(), Breathe) * Intensity;
}
)DSS");
	}

	/**
	 * The proposal's section 8 material function, plus the `out` parameter form, an `extern`
	 * prototype, both include spellings and a `///` block with a paragraph break (a bare `///`
	 * line, which the parser keeps as an empty free-text line).
	 */
	static FString MakeFunctionSource()
	{
		return TEXT(R"DSS(// Shared UV construction: pick a UV channel, scale it, offset it.

#include "/Game/Shared/Common.dsh"

import "/MoonToon/Shared/Toon.dsh";

/// @asset /MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch
extern float2 MF_UVChannelSwitch(float UVChannelIndex);

/// UV channel select, then scale and offset.
///
/// ScaleOffset is xy = scale, zw = offset.
/// @library MoonToon|Shared
/// @param UVChannel    Which UV channel to read.
/// @param ScaleOffset  xy scales the UV, zw is added afterwards.
export float2 MF_ToonUV(float UVChannel = 0.0, float4 ScaleOffset = float4(1, 1, 0, 0))
{
    float2 Selected = MF_UVChannelSwitch(UVChannel);
    return Selected * ScaleOffset.xy + ScaleOffset.zw;
}

export void SampleTinted(Texture2D Tex, float2 UV, float3 InTint,
                         out float3 RGB, out float Alpha)
{
    float4 Texel = Tex.Sample(UV);
    RGB   = Texel.rgb * InTint;
    Alpha = Texel.a;
}
)DSS");
	}

	/** Structs: per-field `///` blocks, array dimensions after the name, `};` to close. */
	static FString MakeStructSource()
	{
		return TEXT(R"DSS(/// @desc Everything the toon shading step needs.
struct ToonInputs
{
    /// @desc Albedo in linear space.
    float3 Albedo;
    float  NdotL;
    /// @group Ramp
    float2 RampUV[4];
    int    Flags[2][3];
};

struct Ramp
{
    Texture2D Tex;
    float2    UV;
};

static const float ToonBias = 0.05;

ToonInputs Gather(float2 UV)
{
    ToonInputs Result;
    Result.Albedo = float3(UV.x, UV.y, 0.0);
    Result.NdotL  = 1.0;
    return Result;
}
)DSS");
	}

	/**
	 * A `/// @custom` body is captured verbatim: braces inside strings, comments and `#` lines must
	 * come back byte for byte, not re-indented and not re-lexed.
	 */
	static FString MakeCustomBodySource()
	{
		return TEXT(R"DSS(/// @custom
float3 AtmosphereLightDir()
{
    return View.AtmosphereLightDirection[0].xyz;
}

/// @custom selfcontained
/// @desc Braces inside strings, comments and directives must not end the raw body.
float4 RawBodyTorture(float2 UV)
{
    #include "/Engine/Private/Common.ush"
    #pragma message("an unbalanced } inside a directive")
    // a line comment with a closing brace } and an opening one {
    /* a block comment: } { */
    float4 Result = float4(0.0, 0.0, 0.0, 0.0);
    if (UV.x > 0.5) { Result.x = 1.0; }
    printf("a closing brace } inside a string literal");
    return Result;
}
)DSS");
	}

	/** Nested branches and loops, including the single-statement bodies the printer braces. */
	static FString MakeControlFlowSource()
	{
		return TEXT(R"DSS(export float3 ControlFlow(float3 Base, int Count, bool bEnable)
{
    float3 Result = Base;
    float Weights[2] = { 0.5, 0.5 };

    for (int i = 0; i < Count; ++i)
    {
        if (i % 2 == 0)
        {
            Result += Base * Weights[0];
        }
        else if (i % 3 == 0)
        {
            Result -= Base * Weights[1];
        }
        else
        {
            Result = Result * 0.9;
        }
    }

    for (;;)
    {
        break;
    }

    int Guard = 0;
    while (Guard < Count)
    {
        Guard++;
        if (Guard > 8)
            break;
        if (Guard == 3)
            continue;
    }

    do
    {
        Guard--;
    }
    while (Guard > 0);

    {
        float Scoped = 1.0;
        Result *= Scoped;
    }

    ;

    if (!bEnable)
    {
        discard;
        return float3(0, 0, 0);
    }

    return Result;
}
)DSS");
	}

	/** Precedence with and without author parentheses; the printed spelling is asserted below. */
	static FString MakePrecedenceSource()
	{
		return TEXT(R"DSS(export float Precedence(float A, float B, float C, int I, int J)
{
    float NoParens = A + B * C - A / B;
    float Author   = (A + B) * (C - A);
    float Right    = A - (B - C);
    float Flat     = A - B - C;
    float Mixed    = ((A + B) * C) / (A - B);
    bool  Logic    = A > B && B > C || C > A && !(A == B);
    int   Bits     = (I & J) | (I ^ J) << 2;
    float Cond     = Logic ? A : B > C ? B : C;
    float Cast     = (float)((I << 2) + (J >> 1));
    float Unary    = -A * -B + +C;
    float Twice    = - -A;
    float Post     = A++ + --B;

    float Chain;
    float Other;
    Chain = Other = A + B;

    return NoParens + Author + Right + Flat + Mixed + Cond + Cast + Unary + Twice + Post + Chain + Other + (float)Bits;
}
)DSS");
	}
}

// -------------------------------------------------------------------------------------------------
// Round trips
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterMaterialTest,
	"DreamShader.Lang2.Printer.RoundTrip.Material",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;

	const FString Label(TEXT("Material"));
	FString Printed;
	RunRoundTrip(*this, Label, TEXT("Inline/M_TeleportGlow.dss"), MakeMaterialSource(), &Printed);

	// The layout of section 7: a pragma written back with its spelling, one `///` directive per
	// line, a blank line between top-level declarations, Allman braces, four-space indentation,
	// `inout` kept and `in` omitted, and a named argument spelled `Name = value`.
	TestContains(*this, Label, Printed, TEXT("#pragma material(ShadingModel = Unlit, BlendMode = Additive, bUsedWithNiagaraSprites = true)"));
	TestContains(*this, Label, Printed, TEXT("/// @group Glow|Look\n/// @desc Multiplied on top of the particle colour\nuniform float4 Tint = float4(1, 1, 1, 1);"));
	TestContains(*this, Label, Printed, TEXT(";\n\n/// @group Glow|Motion"));
	TestContains(*this, Label, Printed, TEXT("float GlowMask(float2 UV, float Time, float Amount)\n{\n"));
	TestContains(*this, Label, Printed, TEXT("export void M_TeleportGlow(inout material m)\n{\n"));
	TestContains(*this, Label, Printed, TEXT("    float2 UV = UE.TexCoord(Index = 0);\n"));
	TestContains(*this, Label, Printed, TEXT("    return (Glow + Hot) * Pulse;\n"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterFunctionTest,
	"DreamShader.Lang2.Printer.RoundTrip.Function",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterFunctionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;

	const FString Label(TEXT("Function"));
	FString Printed;
	RunRoundTrip(*this, Label, TEXT("Inline/MF_ToonUV.dss"), MakeFunctionSource(), &Printed);

	// Both include spellings survive, an `extern` prototype ends in `;` with no body, parameter
	// defaults and `out` come back, and free text is re-emitted before the directives.
	TestContains(*this, Label, Printed, TEXT("#include \"/Game/Shared/Common.dsh\""));
	TestContains(*this, Label, Printed, TEXT("import \"/MoonToon/Shared/Toon.dsh\";"));
	TestContains(*this, Label, Printed, TEXT("/// @asset /MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch\nextern float2 MF_UVChannelSwitch(float UVChannelIndex);"));
	TestContains(*this, Label, Printed, TEXT("export float2 MF_ToonUV(float UVChannel = 0.0, float4 ScaleOffset = float4(1, 1, 0, 0))"));
	TestContains(*this, Label, Printed, TEXT("export void SampleTinted(Texture2D Tex, float2 UV, float3 InTint, out float3 RGB, out float Alpha)"));
	TestContains(*this, Label, Printed, TEXT("    RGB = Texel.rgb * InTint;\n"));

	// A blank `///` line is a paragraph break the parser records as an empty free-text line; the
	// printer has to write it back as a bare `///`, or the two paragraphs glue together.
	TestContains(*this, Label, Printed, TEXT("/// UV channel select, then scale and offset.\n///\n/// ScaleOffset is xy = scale, zw = offset.\n"));
	TestContains(*this, Label, Printed, TEXT("/// @param UVChannel    Which UV channel to read.\n"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterStructTest,
	"DreamShader.Lang2.Printer.RoundTrip.Struct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterStructTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;

	const FString Label(TEXT("Struct"));
	FString Printed;
	RunRoundTrip(*this, Label, TEXT("Inline/Struct.dss"), MakeStructSource(), &Printed);

	// `struct Name` / `{` / fields / `};`, a field `///` block indented with its field, dimensions
	// after the name, and the `static const` storage spelling.
	TestContains(*this, Label, Printed, TEXT("struct ToonInputs\n{\n"));
	TestContains(*this, Label, Printed, TEXT("    /// @desc Albedo in linear space.\n    float3 Albedo;\n"));
	TestContains(*this, Label, Printed, TEXT("    float2 RampUV[4];\n"));
	TestContains(*this, Label, Printed, TEXT("    int Flags[2][3];\n"));
	TestContains(*this, Label, Printed, TEXT("};\n"));
	TestContains(*this, Label, Printed, TEXT("static const float ToonBias = 0.05;"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterCustomBodyTest,
	"DreamShader.Lang2.Printer.RoundTrip.CustomBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterCustomBodyTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;

	const FString Label(TEXT("CustomBody"));
	FString Printed;
	RunRoundTrip(*this, Label, TEXT("Inline/CustomBody.dss"), MakeCustomBodySource(), &Printed);

	// The opaque body is written back verbatim: the `}` inside a string, the `{` inside a line
	// comment, the block comment and the `#` lines are all still there, unchanged and unindented.
	TestContains(*this, Label, Printed, TEXT("/// @custom\nfloat3 AtmosphereLightDir()\n{\n    return View.AtmosphereLightDirection[0].xyz;\n}\n"));
	TestContains(*this, Label, Printed, TEXT("    #include \"/Engine/Private/Common.ush\"\n"));
	TestContains(*this, Label, Printed, TEXT("    #pragma message(\"an unbalanced } inside a directive\")\n"));
	TestContains(*this, Label, Printed, TEXT("    // a line comment with a closing brace } and an opening one {\n"));
	TestContains(*this, Label, Printed, TEXT("    /* a block comment: } { */\n"));
	TestContains(*this, Label, Printed, TEXT("    printf(\"a closing brace } inside a string literal\");\n"));
	TestContains(*this, Label, Printed, TEXT("/// @custom selfcontained\n"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterControlFlowTest,
	"DreamShader.Lang2.Printer.RoundTrip.ControlFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterControlFlowTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;

	const FString Label(TEXT("ControlFlow"));
	FString Printed;
	RunRoundTrip(*this, Label, TEXT("Inline/ControlFlow.dss"), MakeControlFlowSource(), &Printed);

	// One statement per line, Allman braces at the statement's own indent, an `else if` chain that
	// stays flat, and a single-statement body that gets braces so a later edit cannot fall out of
	// the branch.
	TestContains(*this, Label, Printed, TEXT("    for (int i = 0; i < Count; ++i)\n    {\n"));
	TestContains(*this, Label, Printed, TEXT("        if (i % 2 == 0)\n        {\n"));
	TestContains(*this, Label, Printed, TEXT("        else if (i % 3 == 0)\n        {\n"));
	TestContains(*this, Label, Printed, TEXT("        else\n        {\n"));
	TestContains(*this, Label, Printed, TEXT("    for (;;)\n"));
	TestContains(*this, Label, Printed, TEXT("        if (Guard > 8)\n        {\n            break;\n        }\n"));
	TestContains(*this, Label, Printed, TEXT("    do\n    {\n        Guard--;\n    }\n    while (Guard > 0);\n"));
	TestContains(*this, Label, Printed, TEXT("    float Weights[2] = { 0.5, 0.5 };\n"));
	TestContains(*this, Label, Printed, TEXT("        discard;\n"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterPrecedenceTest,
	"DreamShader.Lang2.Printer.RoundTrip.Precedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterPrecedenceTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;

	const FString Label(TEXT("Precedence"));
	FString Printed;
	RunRoundTrip(*this, Label, TEXT("Inline/Precedence.dss"), MakePrecedenceSource(), &Printed);

	// Parentheses the author wrote come back where they were; parentheses precedence does not need
	// are not invented; spacing is one space around every binary and assignment operator and none
	// around a unary one.
	TestContains(*this, Label, Printed, TEXT("    float NoParens = A + B * C - A / B;\n"));
	TestContains(*this, Label, Printed, TEXT("    float Author = (A + B) * (C - A);\n"));
	TestContains(*this, Label, Printed, TEXT("    float Right = A - (B - C);\n"));
	TestContains(*this, Label, Printed, TEXT("    float Flat = A - B - C;\n"));
	TestContains(*this, Label, Printed, TEXT("    float Mixed = ((A + B) * C) / (A - B);\n"));
	TestContains(*this, Label, Printed, TEXT("    bool Logic = A > B && B > C || C > A && !(A == B);\n"));
	TestContains(*this, Label, Printed, TEXT("    int Bits = (I & J) | (I ^ J) << 2;\n"));
	TestContains(*this, Label, Printed, TEXT("    float Cond = Logic ? A : B > C ? B : C;\n"));
	TestContains(*this, Label, Printed, TEXT("    float Cast = (float)((I << 2) + (J >> 1));\n"));
	TestContains(*this, Label, Printed, TEXT("    float Unary = -A * -B + +C;\n"));
	// `--A` would lex back as one pre-decrement token, so the printer separates the two signs.
	TestContains(*this, Label, Printed, TEXT("    float Twice = - -A;\n"));
	TestContains(*this, Label, Printed, TEXT("    float Post = A++ + --B;\n"));
	TestContains(*this, Label, Printed, TEXT("    Chain = Other = A + B;\n"));

	return true;
}

// -------------------------------------------------------------------------------------------------
// Expression spot checks
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterExpressionsTest,
	"DreamShader.Lang2.Printer.Expressions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterExpressionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;

	// Parentheses: the author's are kept (FParenExpr prints its own and counts as an atom), and the
	// ones precedence requires are added -- the right operand of an equal-precedence left-associative
	// operator, the left operand of an equal-precedence right-associative one.
	TestExpression(*this, TEXT("a-(b-c)"), TEXT("a - (b - c)"));
	TestExpression(*this, TEXT("a-b-c"), TEXT("a - b - c"));
	TestExpression(*this, TEXT("(a+b)*c"), TEXT("(a + b) * c"));
	TestExpression(*this, TEXT("a+b*c"), TEXT("a + b * c"));
	TestExpression(*this, TEXT("a*(b+c)"), TEXT("a * (b + c)"));
	TestExpression(*this, TEXT("a||b&&c"), TEXT("a || b && c"));
	TestExpression(*this, TEXT("(a||b)&&c"), TEXT("(a || b) && c"));

	// Right-associative: assignment and the conditional operator.
	TestExpression(*this, TEXT("a = b = c"), TEXT("a = b = c"));
	TestExpression(*this, TEXT("a += b * 2"), TEXT("a += b * 2"));
	TestExpression(*this, TEXT("x ? y : z"), TEXT("x ? y : z"));
	TestExpression(*this, TEXT("a?b:c?d:e"), TEXT("a ? b : c ? d : e"));
	TestExpression(*this, TEXT("(a?b:c)?d:e"), TEXT("(a ? b : c) ? d : e"));

	// Unary, casts and postfix chains: tight spelling, and a cast that is not a constructor call.
	TestExpression(*this, TEXT("(float3)x"), TEXT("(float3)x"));
	TestExpression(*this, TEXT("float3(1, 2, 3)"), TEXT("float3(1, 2, 3)"));
	TestExpression(*this, TEXT("!a && b"), TEXT("!a && b"));
	TestExpression(*this, TEXT("-a"), TEXT("-a"));
	TestExpression(*this, TEXT("- -a"), TEXT("- -a"));
	TestExpression(*this, TEXT("a++"), TEXT("a++"));
	TestExpression(*this, TEXT("--a"), TEXT("--a"));
	TestExpression(*this, TEXT("Tex.Sample(UV).rgb"), TEXT("Tex.Sample(UV).rgb"));
	TestExpression(*this, TEXT("Data[I + 1].xy"), TEXT("Data[I + 1].xy"));

	// Named arguments keep the 1.x spelling; literals keep their lexeme; strings are re-escaped.
	TestExpression(*this, TEXT("f(a, Name = b)"), TEXT("f(a, Name = b)"));
	TestExpression(*this, TEXT("UE.TexCoord(Index = 0)"), TEXT("UE.TexCoord(Index = 0)"));
	TestExpression(*this, TEXT("g(1.0f, 0x10, 2u, true)"), TEXT("g(1.0f, 0x10, 2u, true)"));
	TestExpression(*this, TEXT("h(\"a \\\"b\\\" c\\n\")"), TEXT("h(\"a \\\"b\\\" c\\n\")"));

	return true;
}

// -------------------------------------------------------------------------------------------------
// Corpus
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2PrinterCorpusTest,
	"DreamShader.Lang2.Printer.Corpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2PrinterCorpusTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::LangPrinterTests;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// The corpus root is found the way DreamShaderTestCommon.h finds it: from the plugin's base
	// directory, never from a project-relative path.
	const FString CorpusRoot = GetDreamShaderCorpusRoot();
	if (CorpusRoot.IsEmpty())
	{
		AddError(TEXT("The DreamShader plugin was not found, so the Lang corpus cannot be located."));
		return false;
	}

	const FString LangDir = FPaths::Combine(CorpusRoot, TEXT("Lang"));
	IFileManager& FileManager = IFileManager::Get();

	TArray<FString> Files;
	FileManager.FindFilesRecursive(Files, *LangDir, TEXT("*.dss"), true, false, false);
	FileManager.FindFilesRecursive(Files, *LangDir, TEXT("*.dsh"), true, false, false);
	Files.Sort();

	int32 Checked = 0;
	for (const FString& File : Files)
	{
		// A ".bad." fixture is expected to fail to parse; there is no tree to print.
		if (FPaths::GetCleanFilename(File).Contains(TEXT(".bad."), ESearchCase::IgnoreCase))
		{
			continue;
		}

		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *File))
		{
			AddError(FString::Printf(TEXT("Cannot read corpus fixture '%s'."), *File));
			continue;
		}

		FString Relative = File;
		FPaths::MakePathRelativeTo(Relative, *(LangDir / TEXT("")));

		RunRoundTrip(*this, Relative, File, SourceText);
		++Checked;
	}

	// An empty enumeration is a broken test, not a passing one: the fixtures are checked in.
	TestTrue(
		FString::Printf(TEXT("the Lang corpus under '%s' holds at least one printable fixture"), *LangDir),
		Checked > 0);

	AddInfo(FString::Printf(TEXT("Round-tripped %d Lang corpus fixtures."), Checked));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
