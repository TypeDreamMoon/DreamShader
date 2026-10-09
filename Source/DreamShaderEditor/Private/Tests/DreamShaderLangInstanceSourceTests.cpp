// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.InstanceSource.* -- `.dsi` text without the compiler (Lang/LangInstanceSource.h):
// the text of an instance payload, which the decompiler writes for a MaterialInstanceConstant, and the
// in-place rewrites behind Adopt of a `.dsi` and "Adopt tweaks as source defaults" on a `.dss`.
//
// A rewrite is a splice over the parsed file, never a reprint. So the assertion that matters in every success case is
// the same one: applying the edits to the original gives the text, the edits do not overlap, and every byte outside
// them is the author's.
//
// Core only.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Lang/LangInstanceSource.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::InstanceSourceTests
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;

	using FIRRun = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRun;

	/** A `.dsi` (or `.dss`) taken as far as its IR; the instance payload of a `.dsi` is Products[0].Instance. */
	inline bool Lower(FAutomationTestBase& Test, FIRRun& Run, const TCHAR* FileName, const FString& Text)
	{
		UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRunOptions Options;
		UE::DreamShader::Editor::Private::Tests::RunDreamShaderIRPipeline(FileName, Text, Options, Run);
		if (!Run.Module.IsValid() || !Run.Bind.Bound.IsValid() || Run.Errors.Num() > 0)
		{
			Test.AddError(FString::Printf(TEXT("[%s] does not build: %s"), FileName, *Run.ErrorText()));
			return false;
		}
		return true;
	}

	inline FIRInstanceOverride MakeScalar(const TCHAR* Name, const double Value, const FIRType& DeclaredType = FIRType::Float(1))
	{
		FIRInstanceOverride Override;
		Override.ParameterName = Name;
		Override.VariableName = Name;
		Override.Kind = EIRParameterKind::Scalar;
		const double V[4] = { Value, 0.0, 0.0, 0.0 };
		Override.Value = FIRPropertyValue::MakeFloat4(V, 1);
		Override.DeclaredType = DeclaredType;
		return Override;
	}

	/** Applies sorted, non-overlapping edits to Original; false when they are not that. */
	inline bool ApplyEdits(const FString& Original, const TArray<FLangSourceEdit>& Edits, FString& OutText)
	{
		OutText.Reset();
		int32 Cursor = 0;
		for (const FLangSourceEdit& Edit : Edits)
		{
			if (Edit.Span.Offset < Cursor || Edit.Span.End() > Original.Len())
			{
				return false;
			}
			OutText += Original.Mid(Cursor, Edit.Span.Offset - Cursor);
			OutText += Edit.NewText;
			Cursor = Edit.Span.End();
		}
		OutText += Original.Mid(Cursor);
		return true;
	}

	inline bool HasCode(const FLangDiagnosticSink& Sink, const TCHAR* Code)
	{
		return Sink.GetDiagnostics().ContainsByPredicate([Code](const FLangDiagnostic& Diagnostic) { return Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive); });
	}

	static const TCHAR* const GInstanceSource = TEXT(
		"// An instance with comments the rewrite has to leave alone.\n"
		"#pragma instance(Parent = \"/Game/M_Parent\", BlendMode = Translucent) // the pragma's own comment\n"
		"\n"
		"// the gain\n"
		"uniform float Gain = 2.0; // trailing\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"/// @static\n"
		"uniform bool UseWarm = false;\n"
		"/// @default /Game/T_Old\n"
		"uniform Texture2D Source;\n");
}

// ---------------------------------------------------------------------------------------------
// Literals and identifiers
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceSourceSpellingsTest,
	"DreamShader.Lang2.InstanceSource.Spellings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceSourceSpellingsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;

	struct FFloatCase { double Value; const TCHAR* Text; };
	const FFloatCase Floats[] = {
		{ 0.0, TEXT("0.0") }, { 1.0, TEXT("1.0") }, { 4.0, TEXT("4.0") }, { -4.0, TEXT("-4.0") },
		{ 0.5, TEXT("0.5") }, { 0.1, TEXT("0.1") }, { 1.0 / 3.0, TEXT("0.33333334") }, { 16777217.0, TEXT("16777216.0") },
	};
	for (const FFloatCase& Case : Floats)
	{
		TestEqual(FString::Printf(TEXT("FormatDreamShaderFloatLiteral(%g)"), Case.Value), FormatDreamShaderFloatLiteral(Case.Value), FString(Case.Text));
	}
	TestEqual(TEXT("NaN is written as zero"), FormatDreamShaderFloatLiteral(NAN), FString(TEXT("0.0")));
	TestEqual(TEXT("infinity is written as zero"), FormatDreamShaderFloatLiteral(INFINITY), FString(TEXT("0.0")));

	// The shortest text that reads back as the same float32, for a spread of bit patterns.
	uint32 State = 0x9E3779B9u;
	for (int32 Index = 0; Index < 512; ++Index)
	{
		State = State * 1664525u + 1013904223u;
		float Value = 0.0f;
		FMemory::Memcpy(&Value, &State, sizeof(Value));
		if (!FMath::IsFinite(Value))
		{
			continue;
		}
		const FString Text = FormatDreamShaderFloatLiteral(Value);
		const float ReadBack = static_cast<float>(FCString::Atod(*Text));
		if (ReadBack != Value)
		{
			AddError(FString::Printf(TEXT("'%s' reads back as %.9g, and was written for %.9g"), *Text, ReadBack, Value));
			break;
		}
	}

	struct FNameCase { const TCHAR* Name; const TCHAR* Identifier; };
	const FNameCase Names[] = {
		{ TEXT("Base Color"), TEXT("Base_Color") }, { TEXT("2Side"), TEXT("_2Side") }, { TEXT(""), TEXT("Parameter") },
		{ TEXT("float"), TEXT("float_") }, { TEXT("UE"), TEXT("UE_") }, { TEXT("Tint-A"), TEXT("Tint_A") },
	};
	for (const FNameCase& Case : Names)
	{
		TestEqual(FString::Printf(TEXT("MakeDreamShaderIdentifier('%s')"), Case.Name), MakeDreamShaderIdentifier(Case.Name), FString(Case.Identifier));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The text of a payload
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceSourcePrintTest,
	"DreamShader.Lang2.InstanceSource.Print",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceSourcePrintTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::InstanceSourceTests;

	FIRInstance Instance;
	Instance.ParentReference = TEXT("/Game/Materials/M_Parent");
	Instance.Settings.Emplace(TEXT("BlendMode"), TEXT("Translucent"));
	Instance.Settings.Emplace(TEXT("OpacityMaskClipValue"), TEXT("0.25"));

	Instance.Overrides.Add(MakeScalar(TEXT("Gain"), 2.0));
	Instance.Overrides.Add(MakeScalar(TEXT("Steps"), 4.0, FIRType::Scalar(EIRTypeKind::Int)));

	{
		FIRInstanceOverride Vector;
		Vector.ParameterName = TEXT("Base Color");
		Vector.VariableName = TEXT("Base_Color");
		Vector.Kind = EIRParameterKind::Vector;
		const double V[4] = { 1.0, 0.5, 0.25, 1.0 };
		Vector.Value = FIRPropertyValue::MakeFloat4(V, 4);
		Vector.DeclaredType = FIRType::Float(3);
		Instance.Overrides.Add(Vector);
	}
	{
		FIRInstanceOverride Switch;
		Switch.ParameterName = TEXT("UseWarm");
		Switch.VariableName = TEXT("UseWarm");
		Switch.Kind = EIRParameterKind::StaticSwitch;
		Switch.Value = FIRPropertyValue::MakeBool(true);
		Switch.DeclaredType = FIRType::Bool(1);
		Instance.Overrides.Add(Switch);
	}
	{
		FIRInstanceOverride Texture;
		Texture.ParameterName = TEXT("Source");
		Texture.VariableName = TEXT("Source");
		Texture.Kind = EIRParameterKind::Texture;
		Texture.Value = FIRPropertyValue::MakeObject(TEXT("/Game/Textures/T_X"));
		Instance.Overrides.Add(Texture);

		FIRInstanceOverride None = Texture;
		None.ParameterName = TEXT("Mask");
		None.VariableName = TEXT("Mask");
		None.Value = FIRPropertyValue::MakeObject(FString());
		Instance.Overrides.Add(None);
	}
	{
		FIRInstanceOverride Font;
		Font.ParameterName = TEXT("Glyphs");
		Font.VariableName = TEXT("Glyphs");
		Font.Kind = EIRParameterKind::Font;
		Font.Value = FIRPropertyValue::MakeObject(TEXT("/Game/Fonts/F_X"));
		Font.FontPage = 2;
		Instance.Overrides.Add(Font);
	}

	const FString Text = PrintDreamShaderInstance(Instance, TEXT("MI_Print.dsi"), TEXT("/Game/Instances/MI_Print"));

	TestTrue(FString::Printf(TEXT("`@name` above the pragma\n%s"), *Text), Text.Contains(TEXT("@name /Game/Instances/MI_Print")));
	TestTrue(TEXT("the pragma: a path is quoted, a word and a number are bare"), Text.Contains(TEXT("#pragma instance(Parent = \"/Game/Materials/M_Parent\", BlendMode = Translucent, OpacityMaskClipValue = 0.25)")));
	TestTrue(TEXT("a scalar"), Text.Contains(TEXT("uniform float Gain = 2.0;")));
	TestTrue(TEXT("a scalar declared int"), Text.Contains(TEXT("uniform int Steps = 4;")));
	TestTrue(TEXT("a name that is no identifier: `@name`, and the variable sanitised"), Text.Contains(TEXT("@name Base Color")) && Text.Contains(TEXT("uniform float3 Base_Color = float3(1.0, 0.5, 0.25);")));
	TestTrue(TEXT("a static switch"), Text.Contains(TEXT("@static")) && Text.Contains(TEXT("uniform bool UseWarm = true;")));
	TestTrue(TEXT("a texture by `@default`"), Text.Contains(TEXT("@default /Game/Textures/T_X")) && Text.Contains(TEXT("uniform Texture2D Source;")));
	TestTrue(TEXT("an explicit None"), Text.Contains(TEXT("@default None")));
	TestTrue(TEXT("a font with its page"), Text.Contains(TEXT("@page 2")) && Text.Contains(TEXT("uniform Font Glyphs;")));

	// What is written is a `.dsi` the front end reads.
	const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(TEXT("MI_Print.dsi"), Text), FLangParseOptions());
	TestTrue(
		FString::Printf(TEXT("the text parses as a `.dsi` (%s)"),
			*FString::Join(UE::DreamShader::Editor::Private::Tests::GatherDreamShaderLangDiagnostics(Parsed.Diagnostics, ELangSeverity::Error), TEXT(" | "))),
		Parsed.Succeeded());
	return true;
}

// ---------------------------------------------------------------------------------------------
// Adopt of a `.dsi`: the splice
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceSourceRewriteTest,
	"DreamShader.Lang2.InstanceSource.RewriteInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceSourceRewriteTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::InstanceSourceTests;

	const FString Original = GInstanceSource;
	FIRRun Run;
	if (!Lower(*this, Run, TEXT("MI_Rewrite.dsi"), Original) || !TestTrue(TEXT("one instance product"), Run.Module->Products.Num() == 1))
	{
		return false;
	}
	const FLangSourceText OriginalSource(TEXT("MI_Rewrite.dsi"), Original);
	const FIRInstance Payload = Run.Module->Products[0].Instance;

	const auto Rewrite = [this, &OriginalSource, &Run](const FIRInstance& Desired, TArray<FLangSourceEdit>& OutEdits, FString& OutText, FLangDiagnosticSink& OutSink)
	{
		return RewriteDreamShaderInstanceSource(OriginalSource, *Run.Parse.Module, *Run.Bind.Bound, Desired, OutEdits, OutText, OutSink);
	};
	const auto ExpectSplice = [this, &Original](const TCHAR* What, const TArray<FLangSourceEdit>& Edits, const FString& Text)
	{
		FString Applied;
		TestTrue(FString::Printf(TEXT("%s: the edits are sorted and do not overlap"), What), ApplyEdits(Original, Edits, Applied));
		TestTrue(FString::Printf(TEXT("%s: applying the edits to the original gives the text"), What), Applied.Equals(Text, ESearchCase::CaseSensitive));
	};

	// The payload the file already states: nothing to do.
	{
		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestTrue(TEXT("unchanged: succeeds"), Rewrite(Payload, Edits, Text, Sink));
		TestEqual(TEXT("unchanged: no edits"), Edits.Num(), 0);
		TestTrue(TEXT("unchanged: the text is the original"), Text.Equals(Original, ESearchCase::CaseSensitive));
	}

	// One scalar changes: one initializer changes, and nothing else.
	{
		FIRInstance Desired = Payload;
		for (FIRInstanceOverride& Override : Desired.Overrides)
		{
			if (Override.ParameterName == TEXT("Gain"))
			{
				const double V[4] = { 3.5, 0.0, 0.0, 0.0 };
				Override.Value = FIRPropertyValue::MakeFloat4(V, 1);
			}
		}
		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestTrue(TEXT("scalar: succeeds"), Rewrite(Desired, Edits, Text, Sink));
		TestEqual(TEXT("scalar: one edit"), Edits.Num(), 1);
		ExpectSplice(TEXT("scalar"), Edits, Text);
		TestTrue(FString::Printf(TEXT("scalar: only the initializer\n%s"), *Text), Text.Contains(TEXT("uniform float Gain = 3.5; // trailing")));
		TestTrue(TEXT("scalar: the comments are the author's"), Text.Contains(TEXT("// the gain")) && Text.Contains(TEXT("// the pragma's own comment")));
	}

	// A texture changes: only the `@default` value.
	{
		FIRInstance Desired = Payload;
		for (FIRInstanceOverride& Override : Desired.Overrides)
		{
			if (Override.ParameterName == TEXT("Source"))
			{
				Override.Value = FIRPropertyValue::MakeObject(TEXT("/Game/T_New"));
			}
		}
		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestTrue(TEXT("texture: succeeds"), Rewrite(Desired, Edits, Text, Sink));
		ExpectSplice(TEXT("texture"), Edits, Text);
		TestTrue(TEXT("texture: the new default"), Text.Contains(TEXT("/// @default /Game/T_New")) && !Text.Contains(TEXT("T_Old")));
	}

	// An override goes away, another one arrives.
	{
		FIRInstance Desired = Payload;
		Desired.Overrides.RemoveAll([](const FIRInstanceOverride& Override) { return Override.ParameterName == TEXT("Tint"); });
		Desired.Overrides.Add(MakeScalar(TEXT("Added"), 7.0));

		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestTrue(TEXT("add and remove: succeeds"), Rewrite(Desired, Edits, Text, Sink));
		ExpectSplice(TEXT("add and remove"), Edits, Text);
		TestFalse(TEXT("add and remove: the removed declaration is gone"), Text.Contains(TEXT("Tint")));
		TestTrue(TEXT("add and remove: the new one is written"), Text.Contains(TEXT("uniform float Added = 7.0;")));
		TestTrue(TEXT("add and remove: after the last uniform"), Text.Find(TEXT("uniform float Added")) > Text.Find(TEXT("uniform Texture2D Source")));
	}

	// Another parent: the pragma is reprinted, and keeps the comment behind it.
	{
		FIRInstance Desired = Payload;
		Desired.ParentReference = TEXT("/Game/M_OtherParent");
		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestTrue(TEXT("parent: succeeds"), Rewrite(Desired, Edits, Text, Sink));
		ExpectSplice(TEXT("parent"), Edits, Text);
		TestTrue(FString::Printf(TEXT("parent: rewritten, with its comment\n%s"), *Text), Text.Contains(TEXT("Parent = \"/Game/M_OtherParent\"")) && Text.Contains(TEXT("// the pragma's own comment")));
	}

	// A value inside a declaration shared with other names cannot be spliced: refused whole, nothing edited.
	{
		const FString Shared = TEXT("#pragma instance(Parent = \"/Game/M_Parent\")\nuniform float A = 1, B = 2;\n");
		FIRRun SharedRun;
		if (Lower(*this, SharedRun, TEXT("MI_Shared.dsi"), Shared))
		{
			FIRInstance Desired = SharedRun.Module->Products[0].Instance;
			if (Desired.Overrides.Num() > 0)
			{
				const double V[4] = { 9.0, 0.0, 0.0, 0.0 };
				Desired.Overrides[0].Value = FIRPropertyValue::MakeFloat4(V, 1);
			}
			TArray<FLangSourceEdit> Edits;
			FString Text;
			FLangDiagnosticSink Sink;
			const bool bRewritten = RewriteDreamShaderInstanceSource(
				FLangSourceText(TEXT("MI_Shared.dsi"), Shared), *SharedRun.Parse.Module, *SharedRun.Bind.Bound, Desired, Edits, Text, Sink);
			TestFalse(TEXT("shared declaration: refused"), bRewritten);
			TestTrue(TEXT("shared declaration: DSH9107"), HasCode(Sink, TEXT("DSH9107")));
			TestEqual(TEXT("shared declaration: no edits at all"), Edits.Num(), 0);
			TestTrue(TEXT("shared declaration: the text is the original"), Text.Equals(Shared, ESearchCase::CaseSensitive));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// "Adopt tweaks as source defaults" on a `.dss`
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceSourceUniformDefaultsTest,
	"DreamShader.Lang2.InstanceSource.RewriteUniformDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceSourceUniformDefaultsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::InstanceSourceTests;

	const FString Original = TEXT(
		"// a material with defaults\n"
		"uniform float Roughness = 0.5; // keep me\n"
		"uniform float NoInitializer;\n"
		"uniform float3 Tint = float3(1, 1, 1);\n"
		"\n"
		"export void M_Defaults(inout material m)\n"
		"{\n"
		"    m.Roughness = Roughness + NoInitializer;\n"
		"    m.BaseColor = Tint;\n"
		"}\n");
	FIRRun Run;
	if (!Lower(*this, Run, TEXT("M_Defaults.dss"), Original))
	{
		return false;
	}
	const FLangSourceText Source(TEXT("M_Defaults.dss"), Original);

	{
		TArray<FIRInstanceOverride> Defaults;
		Defaults.Add(MakeScalar(TEXT("Roughness"), 0.25));
		Defaults.Add(MakeScalar(TEXT("NoInitializer"), 1.5));

		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestTrue(TEXT("defaults: succeeds"), RewriteDreamShaderUniformDefaults(Source, *Run.Parse.Module, *Run.Bind.Bound, Defaults, Edits, Text, Sink));
		FString Applied;
		TestTrue(TEXT("defaults: the edits splice"), ApplyEdits(Original, Edits, Applied) && Applied.Equals(Text, ESearchCase::CaseSensitive));
		TestTrue(FString::Printf(TEXT("defaults: the initializer changes and its comment stays\n%s"), *Text), Text.Contains(TEXT("uniform float Roughness = 0.25; // keep me")));
		TestTrue(TEXT("defaults: a missing initializer is added"), Text.Contains(TEXT("uniform float NoInitializer = 1.5;")));
		TestTrue(TEXT("defaults: the rest is untouched"), Text.Contains(TEXT("uniform float3 Tint = float3(1, 1, 1);")) && Text.Contains(TEXT("// a material with defaults")));
	}

	// A parameter the file does not declare: nothing to write the value onto, and then nothing is written at all.
	{
		TArray<FIRInstanceOverride> Defaults;
		Defaults.Add(MakeScalar(TEXT("Roughness"), 0.75));
		Defaults.Add(MakeScalar(TEXT("Nowhere"), 1.0));

		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestFalse(TEXT("unknown parameter: refused"), RewriteDreamShaderUniformDefaults(Source, *Run.Parse.Module, *Run.Bind.Bound, Defaults, Edits, Text, Sink));
		TestTrue(TEXT("unknown parameter: DSH9109"), HasCode(Sink, TEXT("DSH9109")));
		TestEqual(TEXT("unknown parameter: no edits, not even the one that would have worked"), Edits.Num(), 0);
	}

	// A texture value onto a float: a uniform of another kind.
	{
		FIRInstanceOverride WrongKind;
		WrongKind.ParameterName = TEXT("Roughness");
		WrongKind.VariableName = TEXT("Roughness");
		WrongKind.Kind = EIRParameterKind::Texture;
		WrongKind.Value = FIRPropertyValue::MakeObject(TEXT("/Game/T_X"));

		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestFalse(TEXT("kind mismatch: refused"), RewriteDreamShaderUniformDefaults(Source, *Run.Parse.Module, *Run.Bind.Bound, { WrongKind }, Edits, Text, Sink));
		TestTrue(TEXT("kind mismatch: DSH9109"), HasCode(Sink, TEXT("DSH9109")));
	}

	// A .dss uniform's type is part of the shader program. Values that require a type change
	// must fail atomically, instead of silently coercing the tweak or changing callers' semantics.
	const FString TypedOriginal = TEXT(
		"uniform bool Flag = true;\n"
		"uniform int Count = 1;\n"
		"uniform uint Index = 0;\n"
		"export void M_Typed(inout material m) { m.EmissiveColor = float3(Flag ? 1 : 0, Count, Index); }\n");
	FIRRun TypedRun;
	if (!Lower(*this, TypedRun, TEXT("M_Typed.dss"), TypedOriginal)) { return false; }
	const FLangSourceText TypedSource(TEXT("M_Typed.dss"), TypedOriginal);
	for (const FIRInstanceOverride& Bad : { MakeScalar(TEXT("Flag"), 2), MakeScalar(TEXT("Count"), 0.5), MakeScalar(TEXT("Index"), -1) })
	{
		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestFalse(TEXT("a scalar requiring a uniform type change is refused"), RewriteDreamShaderUniformDefaults(
			TypedSource, *TypedRun.Parse.Module, *TypedRun.Bind.Bound, { MakeScalar(TEXT("Count"), 3), Bad }, Edits, Text, Sink));
		TestTrue(TEXT("the unrepresentable scalar reports DSH9109"), HasCode(Sink, TEXT("DSH9109")));
		TestEqual(TEXT("no partial edit survives the refused scalar"), Edits.Num(), 0);
		TestEqual(TEXT("a refused scalar leaves the source unchanged"), Text, TypedOriginal);
	}
	{
		TArray<FLangSourceEdit> Edits;
		FString Text;
		FLangDiagnosticSink Sink;
		TestTrue(TEXT("representable typed scalar defaults still rewrite"), RewriteDreamShaderUniformDefaults(
			TypedSource, *TypedRun.Parse.Module, *TypedRun.Bind.Bound,
			{ MakeScalar(TEXT("Flag"), 0), MakeScalar(TEXT("Count"), 2), MakeScalar(TEXT("Index"), 3) }, Edits, Text, Sink));
		TestTrue(TEXT("the boolean uniform keeps its type"), Text.Contains(TEXT("uniform bool Flag = false;")));
		TestTrue(TEXT("the integer uniform keeps its type"), Text.Contains(TEXT("uniform int Count = 2;")));
		TestTrue(TEXT("the unsigned uniform keeps its type"), Text.Contains(TEXT("uniform uint Index = 3;")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceSourceDoublePrecisionTest,
	"DreamShader.Lang2.InstanceSource.DoublePrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceSourceDoublePrecisionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::InstanceSourceTests;

	FIRInstance Desired;
	Desired.ParentReference = TEXT("/Game/M_Parent");
	FIRInstanceOverride Vector;
	Vector.ParameterName = Vector.VariableName = TEXT("Position");
	Vector.Kind = EIRParameterKind::DoubleVector;
	Vector.DeclaredType = FIRType::Vector(EIRTypeKind::Double, 4);
	const double Values[4] = { 16777217.0, 1.0000000000009095, 1.0e40, 1.0e-50 };
	Vector.Value = FIRPropertyValue::MakeFloat4(Values, 4);
	Desired.Overrides.Add(Vector);

	const FString Printed = PrintDreamShaderInstance(Desired, TEXT("MI_Double.dsi"), FString());
	FIRRun PrintedRun;
	if (Lower(*this, PrintedRun, TEXT("MI_Double.dsi"), Printed)
		&& TestEqual(TEXT("printed override count"), PrintedRun.Module->Products[0].Instance.Overrides.Num(), 1))
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			TestTrue(FString::Printf(TEXT("printed component %d keeps all double precision: %s"), Index, *Printed),
				PrintedRun.Module->Products[0].Instance.Overrides[0].Value.V[Index] == Values[Index]);
		}
	}

	// These two values collapse to the same float32. Adopt must still replace the initializer.
	const FString Original = TEXT("#pragma instance(Parent = \"/Game/M_Parent\")\n"
		"uniform double4 Position = double4(16777216.0, 0.0, 0.0, 0.0); // keep\n");
	FIRRun Run;
	if (!Lower(*this, Run, TEXT("MI_AdoptDouble.dsi"), Original))
	{
		return false;
	}
	const double AdoptValues[4] = { 16777217.0, 0.0, 0.0, 0.0 };
	Desired.Overrides[0].Value = FIRPropertyValue::MakeFloat4(AdoptValues, 4);
	TArray<FLangSourceEdit> Edits;
	FString Rewritten;
	FLangDiagnosticSink Sink;
	TestTrue(TEXT("double adopt succeeds"), RewriteDreamShaderInstanceSource(
		FLangSourceText(TEXT("MI_AdoptDouble.dsi"), Original), *Run.Parse.Module, *Run.Bind.Bound, Desired, Edits, Rewritten, Sink));
	TestEqual(TEXT("double adopt notices a change below float32 precision"), Edits.Num(), 1);
	TestTrue(TEXT("double adopt preserves the trailing comment"), Rewritten.Contains(TEXT("; // keep")));
	FIRRun RewrittenRun;
	if (Lower(*this, RewrittenRun, TEXT("MI_AdoptDouble.dsi"), Rewritten))
	{
		TestTrue(TEXT("adopted double survives source reload exactly"),
			RewrittenRun.Module->Products[0].Instance.Overrides[0].Value.V[0] == AdoptValues[0]);
		TArray<FLangSourceEdit> RepeatEdits;
		FString RepeatText;
		FLangDiagnosticSink RepeatSink;
		TestTrue(TEXT("repeating double adopt succeeds"), RewriteDreamShaderInstanceSource(
			FLangSourceText(TEXT("MI_AdoptDouble.dsi"), Rewritten), *RewrittenRun.Parse.Module, *RewrittenRun.Bind.Bound,
			Desired, RepeatEdits, RepeatText, RepeatSink));
		TestEqual(TEXT("repeating double adopt changes nothing"), RepeatEdits.Num(), 0);
	}

	// The parent schema may identify a float4 declaration as DoubleVector. Storage kind,
	// not source spelling, decides the precision of an adopted value.
	const FString FloatSpelling = Original.Replace(TEXT("double4"), TEXT("float4"));
	FIRRun FloatRun;
	if (Lower(*this, FloatRun, TEXT("MI_FloatSpelling.dsi"), FloatSpelling))
	{
		Desired.Overrides[0].DeclaredType = FIRType::Float(4);
		TArray<FLangSourceEdit> FloatEdits;
		FString FloatText;
		FLangDiagnosticSink FloatSink;
		TestTrue(TEXT("float4 spelling of a double parameter can be adopted"), RewriteDreamShaderInstanceSource(
			FLangSourceText(TEXT("MI_FloatSpelling.dsi"), FloatSpelling), *FloatRun.Parse.Module, *FloatRun.Bind.Bound,
			Desired, FloatEdits, FloatText, FloatSink));
		TestEqual(TEXT("double parameter comparison ignores float4 spelling"), FloatEdits.Num(), 1);
		TestTrue(TEXT("double parameter printing preserves precision and the author's type spelling"),
			FloatText.Contains(TEXT("uniform float4 Position = float4(16777217.0, 0.0, 0.0, 0.0); // keep")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
