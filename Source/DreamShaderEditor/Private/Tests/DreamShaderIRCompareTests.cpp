// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.IRCompare.* -- AreDreamShaderIRModulesEquivalent (IR/IRCompare.h), the question `dsc migrate` and the
// decompile round trip both ask: do these two modules build the same assets, whatever order their nodes were made in
// and whatever their variables were called.
//
// The comparer is only as good as what it refuses to call equal, so most of this file is pairs that differ in exactly
// one thing, each with the difference text looked at: a message that says "they differ" and not where is what makes a
// DSH9098 useless to the person reading it.
//
// Core only: sources are lowered with the hand-built test catalog through the shared IR runner.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRCompare.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::IRCompareTests
{
	using namespace UE::DreamShader::IR;

	using FIRRun = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRun;

	inline bool Lower(FAutomationTestBase& Test, FIRRun& Run, const TCHAR* Text, const TCHAR* FileName = TEXT("Compare.dss"))
	{
		UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRunOptions Options;
		UE::DreamShader::Editor::Private::Tests::RunDreamShaderIRPipeline(FileName, Text, Options, Run);
		if (!Run.Module.IsValid() || !Run.Succeeded())
		{
			Test.AddError(FString::Printf(TEXT("a comparer fixture does not build: %s"), *Run.ErrorText()));
			return false;
		}
		return true;
	}

	/** Lowers both texts and compares them with the default options unless the caller changes them. */
	inline bool Compare(FAutomationTestBase& Test, const TCHAR* TextA, const TCHAR* TextB, FString& OutDifference, const FIRCompareOptions& Options = FIRCompareOptions())
	{
		FIRRun A;
		FIRRun B;
		if (!Lower(Test, A, TextA) || !Lower(Test, B, TextB))
		{
			OutDifference = TEXT("(did not build)");
			return false;
		}
		return AreDreamShaderIRModulesEquivalent(*A.Module, *B.Module, Options, OutDifference);
	}

	static const TCHAR* const GBaseline = TEXT(
		"uniform float A = 1;\n"
		"uniform float B = 2;\n"
		"export void M_Compare(inout material m)\n"
		"{\n"
		"    float Sum = A + B;\n"
		"    m.Opacity = Sum * 0.5;\n"
		"    m.Roughness = A;\n"
		"}\n");
}

// ---------------------------------------------------------------------------------------------
// What is the same
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRCompareEquivalentTest,
	"DreamShader.Lang2.IRCompare.Equivalent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRCompareEquivalentTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRCompareTests;

	FString Difference;

	// The same text twice.
	TestTrue(TEXT("a module is equivalent to itself"), Compare(*this, GBaseline, GBaseline, Difference));
	TestTrue(TEXT("and then there is no difference text"), Difference.IsEmpty());

	// Other variable names, another statement order, a value written inline instead of named: the same graph.
	TestTrue(
		FString::Printf(TEXT("names and statement order do not matter (%s)"), *Difference),
		Compare(*this, GBaseline, TEXT(
			"uniform float A = 1;\n"
			"uniform float B = 2;\n"
			"export void M_Compare(inout material m)\n"
			"{\n"
			"    m.Roughness = A;\n"
			"    float Renamed = A + B;\n"
			"    float Half = 0.5;\n"
			"    m.Opacity = Renamed * Half;\n"
			"}\n"), Difference));

	// Source positions: blank lines and comments move every span.
	TestTrue(
		FString::Printf(TEXT("source positions do not matter (%s)"), *Difference),
		Compare(*this, GBaseline, TEXT(
			"// a comment\n\n\n"
			"uniform float A = 1;\n"
			"uniform float B = 2;\n\n"
			"export void M_Compare(inout material m)\n"
			"{\n"
			"    float Sum = A + B; // trailing\n\n"
			"    m.Opacity = Sum * 0.5;\n"
			"    m.Roughness = A;\n"
			"}\n"), Difference));

	// Debug names are compared when asked.
	{
		FIRCompareOptions Options;
		Options.bCompareDebugNames = true;
		TestFalse(TEXT("with bCompareDebugNames a renamed local is a difference"), Compare(*this, GBaseline, TEXT(
			"uniform float A = 1;\n"
			"uniform float B = 2;\n"
			"export void M_Compare(inout material m)\n"
			"{\n"
			"    float Other = A + B;\n"
			"    m.Opacity = Other * 0.5;\n"
			"    m.Roughness = A;\n"
			"}\n"), Difference, Options));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// What is not
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRCompareDifferencesTest,
	"DreamShader.Lang2.IRCompare.Differences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRCompareDifferencesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRCompareTests;

	struct FCase
	{
		const TCHAR* What;
		const TCHAR* Text;
		/** A word the difference text has to contain, so a failure points somewhere. */
		const TCHAR* Needle;
	};

	const FCase Cases[] = {
		{ TEXT("a constant changed"),
			TEXT("uniform float A = 1;\nuniform float B = 2;\nexport void M_Compare(inout material m)\n{\n    float Sum = A + B;\n    m.Opacity = Sum * 0.25;\n    m.Roughness = A;\n}\n"),
			TEXT("Constant") },
		{ TEXT("an operator changed"),
			TEXT("uniform float A = 1;\nuniform float B = 2;\nexport void M_Compare(inout material m)\n{\n    float Sum = A - B;\n    m.Opacity = Sum * 0.5;\n    m.Roughness = A;\n}\n"),
			TEXT("Subtract") },
		{ TEXT("an operand swapped"),
			TEXT("uniform float A = 1;\nuniform float B = 2;\nexport void M_Compare(inout material m)\n{\n    float Sum = A + B;\n    m.Opacity = Sum * 0.5;\n    m.Roughness = B;\n}\n"),
			TEXT("Parameter") },
		{ TEXT("a parameter default changed"),
			TEXT("uniform float A = 1;\nuniform float B = 3;\nexport void M_Compare(inout material m)\n{\n    float Sum = A + B;\n    m.Opacity = Sum * 0.5;\n    m.Roughness = A;\n}\n"),
			TEXT("DefaultValue") },
		{ TEXT("an attribute more"),
			TEXT("uniform float A = 1;\nuniform float B = 2;\nexport void M_Compare(inout material m)\n{\n    float Sum = A + B;\n    m.Opacity = Sum * 0.5;\n    m.Roughness = A;\n    m.EmissiveColor = float3(1, 0, 0);\n}\n"),
			TEXT("MaterialSink") },
		{ TEXT("the asset renamed"),
			TEXT("uniform float A = 1;\nuniform float B = 2;\nexport void M_CompareRenamed(inout material m)\n{\n    float Sum = A + B;\n    m.Opacity = Sum * 0.5;\n    m.Roughness = A;\n}\n"),
			TEXT("M_CompareRenamed") },
		{ TEXT("a material setting changed"),
			TEXT("#pragma material(BlendMode = Translucent)\nuniform float A = 1;\nuniform float B = 2;\nexport void M_Compare(inout material m)\n{\n    float Sum = A + B;\n    m.Opacity = Sum * 0.5;\n    m.Roughness = A;\n}\n"),
			TEXT("blendmode") },
		// Read into a local nothing uses: lowered, then pruned, which is what PrunedParameters records. (A uniform nothing
		// reads at all is never lowered, and no IR can tell it was declared.)
		{ TEXT("a pruned parameter more"),
			TEXT("uniform float A = 1;\nuniform float B = 2;\nuniform float Unused = 3;\nexport void M_Compare(inout material m)\n{\n    float Dead = Unused;\n    float Sum = A + B;\n    m.Opacity = Sum * 0.5;\n    m.Roughness = A;\n}\n"),
			TEXT("Unused") },
	};

	for (const FCase& Case : Cases)
	{
		FString Difference;
		const bool bEquivalent = Compare(*this, GBaseline, Case.Text, Difference);
		TestFalse(FString::Printf(TEXT("%s: not equivalent"), Case.What), bEquivalent);
		TestTrue(
			FString::Printf(TEXT("%s: the difference names '%s' (actual: %s)"), Case.What, Case.Needle, *Difference),
			Difference.Contains(Case.Needle, ESearchCase::IgnoreCase));
	}

	// Destinations are a switch: migrate turns it off, because `@root` and `@name` spell one place two ways.
	{
		FIRCompareOptions Options;
		Options.bCompareDestinations = false;
		FString Difference;
		TestTrue(
			FString::Printf(TEXT("without bCompareDestinations a renamed asset is equivalent (%s)"), *Difference),
			Compare(*this, GBaseline, Cases[5].Text, Difference, Options));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Products
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRCompareProductsTest,
	"DreamShader.Lang2.IRCompare.Products",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRCompareProductsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRCompareTests;

	const TCHAR* Library = TEXT(
		"export float2 MF_Scale(float2 UV, float Scale = 1.0)\n"
		"{\n"
		"    return UV * Scale;\n"
		"}\n"
		"export void MF_Split(float4 V, out float3 Head, out float Tail)\n"
		"{\n"
		"    Head = V.rgb;\n"
		"    Tail = V.a;\n"
		"}\n");

	FString Difference;
	TestTrue(TEXT("a function library is equivalent to itself"), Compare(*this, Library, Library, Difference));

	TestFalse(TEXT("one product fewer is a difference"), Compare(*this, Library, TEXT(
		"export float2 MF_Scale(float2 UV, float Scale = 1.0)\n"
		"{\n"
		"    return UV * Scale;\n"
		"}\n"), Difference));
	TestTrue(FString::Printf(TEXT("and it is said as a product count (actual: %s)"), *Difference), Difference.Contains(TEXT("product")));

	// Outputs are compared IN ORDER: swapping two `out` parameters changes the pins of every caller.
	TestFalse(TEXT("function outputs in another order are a difference"), Compare(*this, Library, TEXT(
		"export float2 MF_Scale(float2 UV, float Scale = 1.0)\n"
		"{\n"
		"    return UV * Scale;\n"
		"}\n"
		"export void MF_Split(float4 V, out float Tail, out float3 Head)\n"
		"{\n"
		"    Head = V.rgb;\n"
		"    Tail = V.a;\n"
		"}\n"), Difference));
	TestTrue(FString::Printf(TEXT("and it is said as a function output (actual: %s)"), *Difference), Difference.Contains(TEXT("function output")));

	// An input's default is the pin's preview value: part of the asset.
	TestFalse(TEXT("another input default is a difference"), Compare(*this, Library, TEXT(
		"export float2 MF_Scale(float2 UV, float Scale = 2.0)\n"
		"{\n"
		"    return UV * Scale;\n"
		"}\n"
		"export void MF_Split(float4 V, out float3 Head, out float Tail)\n"
		"{\n"
		"    Head = V.rgb;\n"
		"    Tail = V.a;\n"
		"}\n"), Difference));
	return true;
}

// ---------------------------------------------------------------------------------------------
// The one normalisation that is the engine's: a FunctionInput's preview value
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRComparePreviewValueTest,
	"DreamShader.Lang2.IRCompare.PreviewValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRComparePreviewValueTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::IRCompareTests;

	// Two hand-made modules of one FunctionInput each: the property is what is under test, not the builder.
	const auto MakeModule = [](const TCHAR* InputType, const bool bWithPreview, const double X, const double Y, const double Z, const double W)
	{
		FIRModule Module;
		FIRProduct& Product = Module.Products.AddDefaulted_GetRef();
		Product.Kind = EIRProductKind::MaterialFunction;
		Product.Name = TEXT("MF_Preview");

		FIRNode Input;
		Input.Op = EIROp::FunctionInput;
		Input.Outputs.Add(FIRType::Float(1));
		Input.Properties.Add({ FString(Prop::InputName), FIRPropertyValue::MakeName(TEXT("In")) });
		Input.Properties.Add({ FString(Prop::InputType), FIRPropertyValue::MakeEnum(InputType) });
		Input.Properties.Add({ FString(Prop::IsOptional), FIRPropertyValue::MakeBool(true) });
		if (bWithPreview)
		{
			const double Preview[4] = { X, Y, Z, W };
			Input.Properties.Add({ FString(Prop::PreviewValue), FIRPropertyValue::MakeFloat4(Preview, 4) });
		}
		const int32 InputIndex = Product.Graph.AddNode(MoveTemp(Input));
		Product.Graph.FunctionInputs.Add(InputIndex);

		FIRNode Output;
		Output.Op = EIROp::FunctionOutput;
		Output.Operands.Add(FIRValue{ InputIndex, 0 });
		Output.Properties.Add({ FString(Prop::OutputName), FIRPropertyValue::MakeName(TEXT("Result")) });
		Product.Graph.FunctionOutputs.Add(Product.Graph.AddNode(MoveTemp(Output)));
		return Module;
	};

	FString Difference;
	const FIRCompareOptions Options;

	// A scalar input reads X only: (0,0,0,0), which a `= 0.0` default spreads to, is the engine's own (0,0,0,1).
	TestTrue(
		FString::Printf(TEXT("Scalar: a zero preview and no preview are the same input (%s)"), *Difference),
		AreDreamShaderIRModulesEquivalent(MakeModule(TEXT("Scalar"), false, 0, 0, 0, 0), MakeModule(TEXT("Scalar"), true, 0, 0, 0, 0), Options, Difference));
	TestFalse(
		TEXT("Scalar: a preview of one is not the missing preview"),
		AreDreamShaderIRModulesEquivalent(MakeModule(TEXT("Scalar"), false, 0, 0, 0, 0), MakeModule(TEXT("Scalar"), true, 1, 1, 1, 1), Options, Difference));

	// A four-wide input reads all four: the engine's alpha is one.
	TestFalse(
		TEXT("Vector4: (0,0,0,0) is not the missing preview, whose alpha is one"),
		AreDreamShaderIRModulesEquivalent(MakeModule(TEXT("Vector4"), false, 0, 0, 0, 0), MakeModule(TEXT("Vector4"), true, 0, 0, 0, 0), Options, Difference));
	TestTrue(
		FString::Printf(TEXT("Vector4: (0,0,0,1) is the missing preview (%s)"), *Difference),
		AreDreamShaderIRModulesEquivalent(MakeModule(TEXT("Vector4"), false, 0, 0, 0, 0), MakeModule(TEXT("Vector4"), true, 0, 0, 0, 1), Options, Difference));

	// Three-wide: alpha is not read.
	TestTrue(
		FString::Printf(TEXT("Vector3: the alpha of a preview is not part of the input (%s)"), *Difference),
		AreDreamShaderIRModulesEquivalent(MakeModule(TEXT("Vector3"), true, 1, 2, 3, 0), MakeModule(TEXT("Vector3"), true, 1, 2, 3, 1), Options, Difference));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Custom nodes: where the code came from is not the code
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRCompareCustomMarkersTest,
	"DreamShader.Lang2.IRCompare.CustomMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRCompareCustomMarkersTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::IRCompareTests;

	const TCHAR* Source = TEXT(
		"/// @custom\n"
		"float3 Posterise(float3 Colour, float Steps)\n"
		"{\n"
		"    return floor(Colour * Steps) / Steps;\n"
		"}\n"
		"export void M_Markers(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Posterise(float3(1, 0.5, 0.25), 4);\n"
		"}\n");

	// The same source under two file names and moved two lines down: the marker comments differ, the code does not.
	const FString Moved = FString(TEXT("\n\n")) + Source;
	FIRRun A;
	FIRRun B;
	if (!Lower(*this, A, Source, TEXT("First.dss")) || !Lower(*this, B, *Moved, TEXT("Second.dss")))
	{
		return false;
	}

	FString Difference;
	TestTrue(
		FString::Printf(TEXT("file and line markers are not part of the code (%s)"), *Difference),
		AreDreamShaderIRModulesEquivalent(*A.Module, *B.Module, FIRCompareOptions(), Difference));

	FIRCompareOptions Strict;
	Strict.bCompareCustomMarkers = true;
	TestFalse(TEXT("with bCompareCustomMarkers they are"), AreDreamShaderIRModulesEquivalent(*A.Module, *B.Module, Strict, Difference));

	// The body itself is compared.
	FIRRun C;
	if (!Lower(*this, C, TEXT(
		"/// @custom\n"
		"float3 Posterise(float3 Colour, float Steps)\n"
		"{\n"
		"    return ceil(Colour * Steps) / Steps;\n"
		"}\n"
		"export void M_Markers(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Posterise(float3(1, 0.5, 0.25), 4);\n"
		"}\n"), TEXT("First.dss")))
	{
		return false;
	}
	TestFalse(TEXT("another body is another node"), AreDreamShaderIRModulesEquivalent(*A.Module, *C.Module, FIRCompareOptions(), Difference));
	TestTrue(FString::Printf(TEXT("and the difference names the Custom node (actual: %s)"), *Difference), Difference.Contains(TEXT("Custom")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
