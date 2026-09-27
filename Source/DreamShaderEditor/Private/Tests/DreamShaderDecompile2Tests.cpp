// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Compiler2.Decompile.* -- the 2.0 decompiler through the editor's one seam onto it
// (Tools/DreamShaderDecompileTools.h -> the decompile service -> GetIRDecompiler / the instance decompiler).
//
// The round trip proper -- source, assets, text, the same assets again -- is data (Tests/Corpus/Roundtrip, run by
// DreamShader.Compiler2.Roundtrip.Corpus). What is here is what a corpus cannot say: which request gets which answer.
// A ThinCustom pair read through its hidden base, `/// @name` written only when asked and only when needed, a plain
// instance answered with a `.dsi`, and the refusals with their codes.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Decompiler/DreamShaderDecompileService.h"
#include "Tools/DreamShaderDecompileTools.h"

#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialInstanceConstant.h"

// Nodes an engine may not have: without them there is nothing of the kind to decompile.
#if __has_include("Materials/MaterialExpressionConvert.h")
#include "Materials/MaterialExpressionConvert.h"
#define DREAMSHADER_DECOMPILE2_TESTS_WITH_CONVERT 1
#else
#define DREAMSHADER_DECOMPILE2_TESTS_WITH_CONVERT 0
#endif
#if __has_include("Materials/MaterialExpressionSwitch.h")
#include "Materials/MaterialExpressionSwitch.h"
#define DREAMSHADER_DECOMPILE2_TESTS_WITH_SWITCH 1
#else
#define DREAMSHADER_DECOMPILE2_TESTS_WITH_SWITCH 0
#endif

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::Decompile2Tests
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	using FRequest = ::UE::DreamShader::Editor::FDreamShaderDecompileRequest;
	using FResult = ::UE::DreamShader::Editor::FDreamShaderDecompileResult;
	using EFormat = ::UE::DreamShader::Editor::EDreamShaderDecompileFormat;

	inline bool HasCode(const FResult& Result, const TCHAR* Code)
	{
		return Result.Diagnostics.ContainsByPredicate([Code](const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive);
		});
	}

	inline FString Describe(const FResult& Result)
	{
		TArray<FString> Lines;
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Result.Diagnostics)
		{
			Lines.Add(UE::DreamShader::Lang::FLangDiagnosticSink::ToWireString(Diagnostic));
		}
		return Result.Error + TEXT(" | ") + FString::Join(Lines, TEXT(" | "));
	}

	/** Writes and compiles Source into the fixture, and hands back the asset named AssetName. */
	inline UObject* CompileAndLoad(FAutomationTestBase& Test, FDreamShaderCompile2Fixture& Fixture, const FString& Source, const TCHAR* AssetName)
	{
		Test.AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

		if (!Fixture.WriteSource(Test, Source))
		{
			return nullptr;
		}
		UE::DreamShader::FDreamShaderError Error;
		if (!CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ true, /*bEphemeralThinCustom*/ false))
		{
			ExpectDreamShaderTestCompile(Test, TEXT("the decompile fixture compiles"), false, Error);
			return nullptr;
		}
		const FString ObjectPath = Fixture.MakeObjectPath(AssetName);
		Fixture.TrackObjectPath(ObjectPath);
		UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
		if (!Asset)
		{
			Test.AddError(FString::Printf(TEXT("the compile did not make '%s'."), *ObjectPath));
		}
		return Asset;
	}

	/** The text parses as 2.0 source of the kind its file name says. */
	inline bool ParsesAs(FAutomationTestBase& Test, const FString& FileName, const FString& Text)
	{
		using namespace UE::DreamShader::Lang;
		const FLangParseResult Parsed = ParseDreamShaderLang(FLangSourceText(FileName, Text), FLangParseOptions());
		const TArray<FString> Errors = GatherDreamShaderLangDiagnostics(Parsed.Diagnostics, ELangSeverity::Error);
		Test.TestTrue(FString::Printf(TEXT("the text parses as '%s' (%s)\n%s"), *FileName, *FString::Join(Errors, TEXT(" | ")), *Text), Parsed.Succeeded());
		return Parsed.Succeeded();
	}

	static const TCHAR* const GMaterialSource = TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"\n"
		"/// @group Look @desc The tint\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"/// @slider 0 4\n"
		"uniform float Gain = 0.5;\n"
		"\n"
		"export void {NAME}(inout material m)\n"
		"{\n"
		"    #pragma region Maths\n"
		"    float3 Scaled = Tint * Gain;\n"
		"    #pragma endregion\n"
		"    m.EmissiveColor = Scaled;\n"
		"}\n");

	/** FString::Printf takes a literal format only, so the fixture's one hole is filled by name. */
	inline FString MakeMaterialSource(const TCHAR* EntryName)
	{
		return FString(GMaterialSource).Replace(TEXT("{NAME}"), EntryName, ESearchCase::CaseSensitive);
	}

	/** Every needle in the text, each one said on its own. */
	inline void ExpectSnippets(FAutomationTestBase& Test, const TCHAR* What, const FString& Text, std::initializer_list<const TCHAR*> Needles)
	{
		for (const TCHAR* Needle : Needles)
		{
			Test.TestTrue(FString::Printf(TEXT("%s: '%s'\n%s"), What, Needle, *Text), Text.Contains(Needle, ESearchCase::CaseSensitive));
		}
	}
}

// ---------------------------------------------------------------------------------------------
// A material, and what is written about where it lives
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2MaterialTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.Material",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDecompile2MaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("M_DcMaterial"), TEXT("Decompile2"));
	UObject* Asset = CompileAndLoad(*this, Fixture, MakeMaterialSource(TEXT("M_DcMaterial")), TEXT("M_DcMaterial"));
	if (!Asset)
	{
		return false;
	}

	// Auto with no output file is 2.0 text, at the decompiled-sources default.
	{
		FRequest Request;
		Request.Asset = Asset;
		const FResult Result = RunDreamShaderDecompileRequest(Request);
		TestTrue(FString::Printf(TEXT("the decompile succeeds (%s)"), *Describe(Result)), Result.bSucceeded);
		TestTrue(TEXT("the default output is a `.dss`"), Result.OutputFilePath.EndsWith(TEXT(".dss"), ESearchCase::IgnoreCase));
		if (Result.bSucceeded)
		{
			ParsesAs(*this, TEXT("M_DcMaterial.dss"), Result.SourceText);
			TestTrue(TEXT("the entry"), Result.SourceText.Contains(TEXT("export void M_DcMaterial(inout material")));
			const bool bUniforms = Result.SourceText.Contains(TEXT("uniform float3 Tint")) && Result.SourceText.Contains(TEXT("@group Look")) && Result.SourceText.Contains(TEXT("@slider 0.0 4.0"));
			TestTrue(TEXT("the uniforms, with their metadata"), bUniforms);
			if (!bUniforms)
			{
				AddInfo(FString::Printf(TEXT("decompiled text:\n%s"), *Result.SourceText));
			}
			TestTrue(TEXT("the settings"), Result.SourceText.Contains(TEXT("ShadingModel = Unlit")));
			// The hints the emitter left say what the variable and the region were called.
			TestTrue(FString::Printf(TEXT("the named local comes back under its name\n%s"), *Result.SourceText), Result.SourceText.Contains(TEXT("float3 Scaled")));
			TestTrue(TEXT("the region comes back"), Result.SourceText.Contains(TEXT("#pragma region Maths")));
			// Not asked to keep the path: the text does not pin the asset anywhere.
			TestFalse(TEXT("no `@name` without bKeepAssetPath"), Result.SourceText.Contains(TEXT("@name /Game")));
		}
	}

	// bKeepAssetPath, into a file that would derive another package: `/// @name` with the package the asset has.
	{
		FRequest Request;
		Request.Asset = Asset;
		Request.bKeepAssetPath = true;
		Request.OutputFilePath = FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), TEXT("DreamShaderTests"), TEXT("Decompile2"), TEXT("Elsewhere"), TEXT("M_DcMaterial.dss"));
		const FResult Result = RunDreamShaderDecompileRequest(Request);
		TestTrue(FString::Printf(TEXT("keep path: succeeds (%s)"), *Describe(Result)), Result.bSucceeded);
		TestTrue(
			FString::Printf(TEXT("keep path: `@name` names the asset's package\n%s"), *Result.SourceText),
			Result.SourceText.Contains(FString::Printf(TEXT("@name %s/M_DcMaterial"), *Fixture.GetPackagePath())));
	}

	// bKeepAssetPath into the file that derives the asset's own package: nothing to say.
	{
		FRequest Request;
		Request.Asset = Asset;
		Request.bKeepAssetPath = true;
		Request.OutputFilePath = Fixture.GetSourceFilePath();
		const FResult Result = RunDreamShaderDecompileRequest(Request);
		TestTrue(FString::Printf(TEXT("keep path, own place: succeeds (%s)"), *Describe(Result)), Result.bSucceeded);
		TestFalse(FString::Printf(TEXT("keep path, own place: no `@name`\n%s"), *Result.SourceText), Result.SourceText.Contains(TEXT("@name /Game")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// A ThinCustom pair
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2ThinCustomTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.ThinCustomPair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDecompile2ThinCustomTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FDreamShaderCompile2Fixture Fixture(TEXT("M_DcThinCustom"), TEXT("Decompile2"));
	UObject* Asset = CompileAndLoad(*this, Fixture, TEXT(
		"#pragma material(Backend = ThinCustom, ShadingModel = Unlit, BlendMode = Opaque)\n"
		// An alpha that is not 1: a float4 read only as `.rgb`, whose alpha is the 1 a float3 default is padded with,
		// decompiles as the float3 it cannot be told from.
		"uniform float4 Tint = float4(1, 0.5, 0.25, 0.5);\n"
		"uniform float Boost = 0.5;\n"
		"export void M_DcThinCustom(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Tint.rgb * Boost;\n"
		"}\n"), TEXT("M_DcThinCustom"));
	if (!Asset)
	{
		return false;
	}
	TestNotNull(TEXT("the addressable half of the pair is a material instance"), Cast<UMaterialInstanceConstant>(Asset));

	FRequest Request;
	Request.Asset = Asset;
	Request.Format = EFormat::Dss;
	const FResult Result = RunDreamShaderDecompileRequest(Request);
	TestTrue(FString::Printf(TEXT("the pair decompiles as its source, not as a `.dsi` (%s)"), *Describe(Result)), Result.bSucceeded);
	if (Result.bSucceeded)
	{
		TestTrue(TEXT("a `.dss`"), Result.OutputFilePath.EndsWith(TEXT(".dss"), ESearchCase::IgnoreCase));
		ParsesAs(*this, TEXT("M_DcThinCustom.dss"), Result.SourceText);
		TestTrue(FString::Printf(TEXT("the backend is said\n%s"), *Result.SourceText), Result.SourceText.Contains(TEXT("Backend = ThinCustom")));
		TestTrue(TEXT("the product is named after the pair, not after the hidden base"), Result.SourceText.Contains(TEXT("export void M_DcThinCustom(")));
		TestTrue(FString::Printf(TEXT("the uniforms\n%s"), *Result.SourceText), Result.SourceText.Contains(TEXT("uniform float4 Tint")) && Result.SourceText.Contains(TEXT("uniform float Boost")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Every product of a source in one text; a plain instance as a `.dsi`
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2SourceProductsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.SourceProducts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDecompile2SourceProductsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FDreamShaderCompile2Fixture Fixture(TEXT("DcLibrary"), TEXT("Decompile2"));
	UObject* Inner = CompileAndLoad(*this, Fixture, TEXT(
		"export void MF_DcInner(float4 V, out float3 Head, out float Tail)\n"
		"{\n"
		"    Head = V.rgb;\n"
		"    Tail = V.a;\n"
		"}\n"
		"export float3 MF_DcOuter(float4 V, float Gain = 2.0)\n"
		"{\n"
		"    float3 H;\n"
		"    float T;\n"
		"    MF_DcInner(V, H, T);\n"
		"    return H * T * Gain;\n"
		"}\n"), TEXT("MF_DcInner"));
	if (!Inner)
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MF_DcOuter")));

	FRequest Request;
	Request.Asset = Inner;
	Request.Format = EFormat::Dss;
	Request.SourceFilePath = Fixture.GetSourceFilePath();
	const FResult Result = RunDreamShaderDecompileRequest(Request);
	TestTrue(FString::Printf(TEXT("the request succeeds (%s)"), *Describe(Result)), Result.bSucceeded);
	if (Result.bSucceeded)
	{
		ParsesAs(*this, TEXT("DcLibrary.dss"), Result.SourceText);
		TestTrue(TEXT("both exports are in the one text"), Result.SourceText.Contains(TEXT("MF_DcInner(")) && Result.SourceText.Contains(TEXT("export float3 MF_DcOuter(")));
		// The call between them is a call to the function of the file, not an extern to the asset the file makes.
		TestFalse(FString::Printf(TEXT("no extern to a product of the file\n%s"), *Result.SourceText), Result.SourceText.Contains(TEXT("extern")));
	}

	// One asset alone: the other export is an asset it calls, so it comes back as an extern with the whole interface.
	{
		UObject* Outer = LoadObject<UObject>(nullptr, *Fixture.MakeObjectPath(TEXT("MF_DcOuter")));
		if (TestNotNull(TEXT("the second export exists"), Outer))
		{
			FRequest Single;
			Single.Asset = Outer;
			Single.Format = EFormat::Dss;
			const FResult Alone = RunDreamShaderDecompileRequest(Single);
			TestTrue(FString::Printf(TEXT("one asset alone decompiles (%s)"), *Describe(Alone)), Alone.bSucceeded);
			TestTrue(FString::Printf(TEXT("and declares what it calls\n%s"), *Alone.SourceText), Alone.SourceText.Contains(TEXT("extern")) && Alone.SourceText.Contains(TEXT("@asset")));
			TestFalse(TEXT("from the asset's own interface, not inferred: no DSH9081"), HasCode(Alone, TEXT("DSH9081")));
		}
	}
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2InstanceTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.InstanceToDsi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDecompile2InstanceTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("MI_DcInstance"), TEXT("Decompile2"), TEXT("dsi"));

	FString ParentPath;
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_DcParent.dss"), TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"uniform float Gain = 0.5;\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"/// @static\n"
		"uniform bool UseWarm = true;\n"
		"export void M_DcParent(inout material m)\n"
		"{\n"
		"    float3 Colour = float3(0, 0.5, 1);\n"
		"    if (UseWarm)\n"
		"    {\n"
		"        Colour = float3(1, 0.5, 0);\n"
		"    }\n"
		"    m.EmissiveColor = Colour * Tint * Gain;\n"
		"}\n"), ParentPath))
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("M_DcParent")));

	UObject* Asset = CompileAndLoad(*this, Fixture, TEXT(
		"#pragma instance(Parent = \"M_DcParent\")\n"
		"uniform float Gain = 2.0;\n"
		"/// @static\n"
		"uniform bool UseWarm = false;\n"), TEXT("MI_DcInstance"));
	if (!Asset)
	{
		return false;
	}

	FRequest Request;
	Request.Asset = Asset;
	const FResult Result = RunDreamShaderDecompileRequest(Request);
	TestTrue(FString::Printf(TEXT("a plain instance decompiles (%s)"), *Describe(Result)), Result.bSucceeded);
	if (Result.bSucceeded)
	{
		TestTrue(TEXT("as a `.dsi`"), Result.OutputFilePath.EndsWith(TEXT(".dsi"), ESearchCase::IgnoreCase));
		ParsesAs(*this, TEXT("MI_DcInstance.dsi"), Result.SourceText);
		TestTrue(FString::Printf(TEXT("the parent is named\n%s"), *Result.SourceText), Result.SourceText.Contains(TEXT("#pragma instance(Parent = ")) && Result.SourceText.Contains(TEXT("M_DcParent")));
		TestTrue(TEXT("the scalar override"), Result.SourceText.Contains(TEXT("uniform float Gain = 2.0")));
		TestTrue(TEXT("the static switch override, with its directive"), Result.SourceText.Contains(TEXT("@static")) && Result.SourceText.Contains(TEXT("uniform bool UseWarm = false")));
		// What equals the parent is not an override worth writing.
		TestFalse(TEXT("an untouched parameter is not written"), Result.SourceText.Contains(TEXT("Tint")));
	}

	// The same asset asked for as 1.x text is a contradiction the service refuses before any decompiler runs.
	{
		FRequest Legacy;
		Legacy.Asset = Asset;
		Legacy.Format = EFormat::Dss;
		Legacy.OutputFilePath = FPaths::Combine(FPaths::GetPath(Fixture.GetSourceFilePath()), TEXT("MI_DcInstance.dss"));
		const FResult Refused = RunDreamShaderDecompileRequest(Legacy);
		TestFalse(TEXT("an instance into a `.dss` is refused"), Refused.bSucceeded);
		TestTrue(FString::Printf(TEXT("with DSH9085 (%s)"), *Describe(Refused)), HasCode(Refused, TEXT("DSH9085")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// A Convert node (Make / Break FloatN), whose pins no call can name
// ---------------------------------------------------------------------------------------------

#if DREAMSHADER_DECOMPILE2_TESTS_WITH_CONVERT
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2ConvertTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.ConvertNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The reflected call read a Convert back as `UE.Convert()`: its inputs live in ConvertInputs, which no call can wire, and
// the catalog lists the one output the class default has, so a read of the second output became a read of the first.
// A material that broke a float4 of derivatives into two float2 pins decompiled to `UE.Convert(), UE.Convert()`. What
// each output computes is said instead, and the text is proved by building it and reading the build back.
bool FDreamShaderDecompile2ConvertTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	// The probe's name is the entry its text declares, and the product the rebuild makes.
	const FName ProbeName = MakeUniqueObjectName(GetTransientPackage(), UMaterial::StaticClass(), TEXT("M_DcConvert"));
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), ProbeName, RF_Transient);
	if (!TestNotNull(TEXT("the probe material"), Material))
	{
		return false;
	}

	UMaterialExpressionVectorParameter* Pairs = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass()));
	UMaterialExpressionScalarParameter* Gain = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass()));
	UMaterialExpressionDotProduct* Dot = Cast<UMaterialExpressionDotProduct>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionDotProduct::StaticClass()));
	if (!TestNotNull(TEXT("the parameters"), Pairs) || !TestNotNull(TEXT("the parameters"), Gain) || !TestNotNull(TEXT("the dot product"), Dot))
	{
		return false;
	}
	Pairs->ParameterName = TEXT("DXY");
	Gain->ParameterName = TEXT("Gain");
	// VectorParameter's outputs are RGB, R, G, B, A and then RGBA.
	constexpr int32 RGBA = 5;

	using EConvertType = EMaterialExpressionConvertType;
	const auto MakeConvert = [this, Material](
		std::initializer_list<EConvertType> Inputs,
		std::initializer_list<EConvertType> Outputs,
		std::initializer_list<FMaterialExpressionConvertMapping> Mappings) -> UMaterialExpressionConvert*
	{
		UMaterialExpressionConvert* Convert = Cast<UMaterialExpressionConvert>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConvert::StaticClass()));
		if (!TestNotNull(TEXT("a Convert node"), Convert))
		{
			return nullptr;
		}
		for (const EConvertType Type : Inputs)
		{
			Convert->ConvertInputs.AddDefaulted_GetRef().Type = Type;
		}
		for (const EConvertType Type : Outputs)
		{
			Convert->ConvertOutputs.AddDefaulted_GetRef().Type = Type;
		}
		Convert->ConvertMappings = Mappings;
		// The pins follow the arrays only once asked for; a wire to output 1 needs them.
		Convert->GetOutputs();
		return Convert;
	};

	// Mappings are (input, input channel, output, output channel).
	// The report's shape: a float4 of two derivative pairs broken into its float2 halves.
	UMaterialExpressionConvert* Break = MakeConvert(
		{ EConvertType::Vector4 }, { EConvertType::Vector2, EConvertType::Vector2 },
		{ { 0, 0, 0, 0 }, { 0, 1, 0, 1 }, { 0, 2, 1, 0 }, { 0, 3, 1, 1 } });
	// A wired scalar, an unwired input's default, and a channel no mapping fills: the output's default.
	UMaterialExpressionConvert* Make = MakeConvert(
		{ EConvertType::Scalar, EConvertType::Scalar }, { EConvertType::Vector3 },
		{ { 0, 0, 0, 0 }, { 1, 0, 0, 1 } });
	// Channels out of order: selections put together, as `v.wxy` is.
	UMaterialExpressionConvert* Shuffle = MakeConvert(
		{ EConvertType::Vector4 }, { EConvertType::Vector3 },
		{ { 0, 3, 0, 0 }, { 0, 0, 0, 1 }, { 0, 1, 0, 2 } });
	// A scalar is each of its channels.
	UMaterialExpressionConvert* Splat = MakeConvert({ EConvertType::Vector3 }, { EConvertType::Scalar }, { { 0, 2, 0, 0 } });
	// Nothing mapped at all: the output is its default.
	UMaterialExpressionConvert* Fixed = MakeConvert({}, { EConvertType::Scalar }, {});
	if (!Break || !Make || !Shuffle || !Splat || !Fixed)
	{
		return false;
	}
	Make->ConvertInputs[1].DefaultValue = FLinearColor(0.25f, 0.0f, 0.0f, 0.0f);
	Make->ConvertOutputs[0].DefaultValue = FLinearColor(0.0f, 0.0f, 0.75f, 0.0f);
	Fixed->ConvertOutputs[0].DefaultValue = FLinearColor(0.375f, 0.0f, 0.0f, 0.0f);

	Break->ConvertInputs[0].ExpressionInput.Connect(RGBA, Pairs);
	Make->ConvertInputs[0].ExpressionInput.Connect(0, Gain);
	Shuffle->ConvertInputs[0].ExpressionInput.Connect(RGBA, Pairs);
	Splat->ConvertInputs[0].ExpressionInput.Connect(0, Gain);
	Dot->A.Connect(0, Break);
	Dot->B.Connect(1, Break);
	Material->GetExpressionInputForProperty(MP_Roughness)->Connect(0, Dot);
	Material->GetExpressionInputForProperty(MP_EmissiveColor)->Connect(0, Make);
	Material->GetExpressionInputForProperty(MP_BaseColor)->Connect(0, Shuffle);
	Material->GetExpressionInputForProperty(MP_Metallic)->Connect(0, Splat);
	Material->GetExpressionInputForProperty(MP_Specular)->Connect(0, Fixed);

	const auto ExpectConvertRead = [this](const TCHAR* What, const FString& Text)
	{
		ExpectSnippets(*this, What, Text, {
			TEXT("m.Roughness = dot(DXY.xy, DXY.zw);"),
			TEXT("m.EmissiveColor = float3(Gain, 0.25, 0.75);"),
			TEXT("m.BaseColor = float3(DXY.w, DXY.xy);"),
			TEXT("m.Metallic = Gain;"),
			TEXT("m.Specular = 0.375;") });
		TestFalse(FString::Printf(TEXT("%s: no reflected Convert\n%s"), What, *Text), Text.Contains(TEXT("UE.Convert")));
	};

	FRequest Request;
	Request.Asset = Material;
	Request.Format = EFormat::Dss;
	const FResult Result = RunDreamShaderDecompileRequest(Request);
	TestTrue(FString::Printf(TEXT("the probe decompiles (%s)"), *Describe(Result)), Result.bSucceeded);
	if (!Result.bSucceeded)
	{
		return false;
	}
	ParsesAs(*this, ProbeName.ToString() + TEXT(".dss"), Result.SourceText);
	ExpectConvertRead(TEXT("the probe"), Result.SourceText);
	TestTrue(FString::Printf(TEXT("the rebuild is said to differ in its nodes: DSH9073 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9073")));
	TestFalse(FString::Printf(TEXT("no wire is dropped: no DSH9070 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9070")));
	TestFalse(FString::Printf(TEXT("no output is read as another: no DSH9072 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9072")));
	TestFalse(FString::Printf(TEXT("nothing is refused: no DSH9063 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9063")));

	// The proof: the text builds, and the build reads back as the same text.
	FDreamShaderCompile2Fixture Fixture(ProbeName.ToString(), TEXT("Decompile2"));
	UObject* Rebuilt = CompileAndLoad(*this, Fixture, Result.SourceText, *ProbeName.ToString());
	if (!Rebuilt)
	{
		return false;
	}
	FRequest Again;
	Again.Asset = Rebuilt;
	Again.Format = EFormat::Dss;
	const FResult Second = RunDreamShaderDecompileRequest(Again);
	TestTrue(FString::Printf(TEXT("the rebuild decompiles (%s)"), *Describe(Second)), Second.bSucceeded);
	if (Second.bSucceeded)
	{
		ExpectConvertRead(TEXT("the rebuild"), Second.SourceText);
	}
	return true;
}
#endif // DREAMSHADER_DECOMPILE2_TESTS_WITH_CONVERT

// ---------------------------------------------------------------------------------------------
// A Switch node, whose cases no call can name
// ---------------------------------------------------------------------------------------------

#if DREAMSHADER_DECOMPILE2_TESTS_WITH_SWITCH
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2SwitchTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.SwitchNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The cases of a Switch are an array (Inputs), so the reflected call read one back with none -- `UE.Switch(SwitchValue =
// ..)`, which builds a Switch that has nothing to switch between. It is written as the branches the engine makes of it.
bool FDreamShaderDecompile2SwitchTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FName ProbeName = MakeUniqueObjectName(GetTransientPackage(), UMaterial::StaticClass(), TEXT("M_DcSwitch"));
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), ProbeName, RF_Transient);
	if (!TestNotNull(TEXT("the probe material"), Material))
	{
		return false;
	}

	const auto Create = [Material](UClass* Class) { return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class); };
	UMaterialExpressionScalarParameter* Index = Cast<UMaterialExpressionScalarParameter>(Create(UMaterialExpressionScalarParameter::StaticClass()));
	UMaterialExpressionVectorParameter* TintA = Cast<UMaterialExpressionVectorParameter>(Create(UMaterialExpressionVectorParameter::StaticClass()));
	UMaterialExpressionVectorParameter* TintB = Cast<UMaterialExpressionVectorParameter>(Create(UMaterialExpressionVectorParameter::StaticClass()));
	UMaterialExpressionConstant* Low = Cast<UMaterialExpressionConstant>(Create(UMaterialExpressionConstant::StaticClass()));
	UMaterialExpressionConstant* High = Cast<UMaterialExpressionConstant>(Create(UMaterialExpressionConstant::StaticClass()));
	UMaterialExpressionSwitch* Pick = Cast<UMaterialExpressionSwitch>(Create(UMaterialExpressionSwitch::StaticClass()));
	UMaterialExpressionSwitch* Fixed = Cast<UMaterialExpressionSwitch>(Create(UMaterialExpressionSwitch::StaticClass()));
	if (!TestTrue(TEXT("the probe's nodes"), Index && TintA && TintB && Low && High && Pick && Fixed))
	{
		return false;
	}
	Index->ParameterName = TEXT("Index");
	TintA->ParameterName = TEXT("TintA");
	TintB->ParameterName = TEXT("TintB");
	Low->R = 0.25f;
	High->R = 0.75f;

	// A new Switch comes with one unnamed case, which an unwired graph would leave open (and neither translator compiles).
	Pick->Inputs.Reset();
	Fixed->Inputs.Reset();
	const auto AddCase = [](UMaterialExpressionSwitch* Switch, const TCHAR* Name, UMaterialExpression* From)
	{
		FSwitchCustomInput& Case = Switch->Inputs.AddDefaulted_GetRef();
		Case.InputName = Name;
		Case.Input.Connect(0, From);
	};

	// Wired to a parameter: the branches, the default (ConstDefault, a scalar among float3s) last.
	AddCase(Pick, TEXT("A"), TintA);
	AddCase(Pick, TEXT("B"), TintB);
	Pick->SwitchValue.Connect(0, Index);
	Pick->ConstDefault = 0.5f;
	Material->GetExpressionInputForProperty(MP_EmissiveColor)->Connect(0, Pick);

	// Nothing wired to SwitchValue: its number picks the case once and for all (floor(1.5) = 1).
	AddCase(Fixed, TEXT("Low"), Low);
	AddCase(Fixed, TEXT("High"), High);
	Fixed->ConstSwitchValue = 1.5f;
	Material->GetExpressionInputForProperty(MP_Roughness)->Connect(0, Fixed);

	const auto ExpectSwitchRead = [this](const TCHAR* What, const FString& Text)
	{
		ExpectSnippets(*this, What, Text, {
			TEXT("floor(Index)"),
			TEXT("0.0 == "),
			TEXT("1.0 == "),
			TEXT("m.Roughness = 0.75;") });
		TestFalse(FString::Printf(TEXT("%s: no reflected Switch\n%s"), What, *Text), Text.Contains(TEXT("UE.Switch")));
	};

	FRequest Request;
	Request.Asset = Material;
	Request.Format = EFormat::Dss;
	const FResult Result = RunDreamShaderDecompileRequest(Request);
	TestTrue(FString::Printf(TEXT("the probe decompiles (%s)"), *Describe(Result)), Result.bSucceeded);
	if (!Result.bSucceeded)
	{
		return false;
	}
	ParsesAs(*this, ProbeName.ToString() + TEXT(".dss"), Result.SourceText);
	ExpectSwitchRead(TEXT("the probe"), Result.SourceText);
	TestTrue(FString::Printf(TEXT("the branches are said: DSH9073 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9073")));
	TestTrue(FString::Printf(TEXT("the fixed pick is said: DSH9069 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9069")));
	TestFalse(FString::Printf(TEXT("no case is dropped: no DSH9070 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9070")));

	FDreamShaderCompile2Fixture Fixture(ProbeName.ToString(), TEXT("Decompile2"));
	UObject* Rebuilt = CompileAndLoad(*this, Fixture, Result.SourceText, *ProbeName.ToString());
	if (!Rebuilt)
	{
		return false;
	}
	FRequest Again;
	Again.Asset = Rebuilt;
	Again.Format = EFormat::Dss;
	const FResult Second = RunDreamShaderDecompileRequest(Again);
	TestTrue(FString::Printf(TEXT("the rebuild decompiles (%s)"), *Describe(Second)), Second.bSucceeded);
	if (Second.bSucceeded)
	{
		ExpectSwitchRead(TEXT("the rebuild"), Second.SourceText);
	}
	return true;
}
#endif // DREAMSHADER_DECOMPILE2_TESTS_WITH_SWITCH

// ---------------------------------------------------------------------------------------------
// A custom-output class that is also a value: VertexInterpolator
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2VertexInterpolatorTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.VertexInterpolatorNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A custom-output node was read back with no output at all, the shape of `UE.ClearCoatNormalCustomOutput(...);`. A
// VertexInterpolator is one and hands its value on through its PS pin, so a material that read it did not decompile:
// the read pointed at an output the node did not have (DSH9088).
bool FDreamShaderDecompile2VertexInterpolatorTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FName ProbeName = MakeUniqueObjectName(GetTransientPackage(), UMaterial::StaticClass(), TEXT("M_DcInterpolator"));
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), ProbeName, RF_Transient);
	if (!TestNotNull(TEXT("the probe material"), Material))
	{
		return false;
	}

	UMaterialExpressionVectorParameter* Tint = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass()));
	UMaterialExpressionVertexInterpolator* Interpolator = Cast<UMaterialExpressionVertexInterpolator>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVertexInterpolator::StaticClass()));
	if (!TestTrue(TEXT("the probe's nodes"), Tint && Interpolator))
	{
		return false;
	}
	Tint->ParameterName = TEXT("Tint");
	// VectorParameter's output 0 is its RGB.
	Interpolator->Input.Connect(0, Tint);
	Material->GetExpressionInputForProperty(MP_EmissiveColor)->Connect(0, Interpolator);

	FRequest Request;
	Request.Asset = Material;
	Request.Format = EFormat::Dss;
	const FResult Result = RunDreamShaderDecompileRequest(Request);
	TestTrue(FString::Printf(TEXT("the probe decompiles (%s)"), *Describe(Result)), Result.bSucceeded);
	if (!Result.bSucceeded)
	{
		return false;
	}
	ParsesAs(*this, ProbeName.ToString() + TEXT(".dss"), Result.SourceText);
	ExpectSnippets(*this, TEXT("the probe"), Result.SourceText, { TEXT("m.EmissiveColor = UE.VertexInterpolator(") });
	TestFalse(FString::Printf(TEXT("no output is read as another: no DSH9072 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9072")));

	FDreamShaderCompile2Fixture Fixture(ProbeName.ToString(), TEXT("Decompile2"));
	UObject* Rebuilt = CompileAndLoad(*this, Fixture, Result.SourceText, *ProbeName.ToString());
	if (!Rebuilt)
	{
		return false;
	}
	FRequest Again;
	Again.Asset = Rebuilt;
	Again.Format = EFormat::Dss;
	const FResult Second = RunDreamShaderDecompileRequest(Again);
	TestTrue(FString::Printf(TEXT("the rebuild decompiles (%s)"), *Describe(Second)), Second.bSucceeded);
	if (Second.bSucceeded)
	{
		ExpectSnippets(*this, TEXT("the rebuild"), Second.SourceText, { TEXT("m.EmissiveColor = UE.VertexInterpolator(") });
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Refusals, and the tools seam's own small functions
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2RefusalsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Decompile.Refusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDecompile2RefusalsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("M_DcRefusals"), TEXT("Decompile2"));
	UObject* Asset = CompileAndLoad(*this, Fixture, MakeMaterialSource(TEXT("M_DcRefusals")), TEXT("M_DcRefusals"));
	if (!Asset)
	{
		return false;
	}

	// The format and the file's extension say different things.
	{
		FRequest Request;
		Request.Asset = Asset;
		Request.Format = EFormat::Dss;
		Request.OutputFilePath = FPaths::ChangeExtension(Fixture.GetSourceFilePath(), TEXT("dsm"));
		const FResult Result = RunDreamShaderDecompileRequest(Request);
		TestFalse(TEXT("Dss into a `.dsm` is refused"), Result.bSucceeded);
		TestTrue(FString::Printf(TEXT("with DSH9085 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9085")));
		TestFalse(TEXT("and the error line is filled"), Result.Error.IsEmpty() && Result.Diagnostics.Num() == 0);
	}

	// An asset no source makes.
	{
		FRequest Request;
		Request.Asset = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		Request.Format = EFormat::Dss;
		if (TestNotNull(TEXT("the engine's default texture loads"), Request.Asset))
		{
			const FResult Result = RunDreamShaderDecompileRequest(Request);
			TestFalse(TEXT("a texture is refused"), Result.bSucceeded);
			TestTrue(FString::Printf(TEXT("with DSH9060 or DSH9086 (%s)"), *Describe(Result)), HasCode(Result, TEXT("DSH9060")) || HasCode(Result, TEXT("DSH9086")));
		}
	}

	// No asset at all.
	{
		FRequest Request;
		Request.Format = EFormat::Dss;
		const FResult Result = RunDreamShaderDecompileRequest(Request);
		TestFalse(TEXT("no asset is refused"), Result.bSucceeded);
		TestFalse(TEXT("and says why"), DescribeDreamShaderDecompileFailure(Result).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompile2ToolsTest,
	"DreamShader.Compiler2.Decompile.Tools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDecompile2ToolsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Decompile2Tests;

	// Auto follows the output file; Dss and Legacy are what they say.
	TestTrue(TEXT("Auto + .dsm -> Legacy"), ResolveDreamShaderDecompileFormat(EFormat::Auto, TEXT("X/M.dsm")) == EFormat::Legacy);
	TestTrue(TEXT("Auto + .dsf -> Legacy"), ResolveDreamShaderDecompileFormat(EFormat::Auto, TEXT("X/MF.DSF")) == EFormat::Legacy);
	TestTrue(TEXT("Auto + .dss -> Dss"), ResolveDreamShaderDecompileFormat(EFormat::Auto, TEXT("X/M.dss")) == EFormat::Dss);
	TestTrue(TEXT("Auto + .dsi -> Dss"), ResolveDreamShaderDecompileFormat(EFormat::Auto, TEXT("X/MI.dsi")) == EFormat::Dss);
	TestTrue(TEXT("Auto + nothing -> Dss"), ResolveDreamShaderDecompileFormat(EFormat::Auto, FString()) == EFormat::Dss);
	TestTrue(TEXT("Dss stays Dss"), ResolveDreamShaderDecompileFormat(EFormat::Dss, TEXT("X/M.dsm")) == EFormat::Dss);
	TestTrue(TEXT("Legacy stays Legacy"), ResolveDreamShaderDecompileFormat(EFormat::Legacy, TEXT("X/M.dss")) == EFormat::Legacy);

	EFormat Parsed = EFormat::Auto;
	TestTrue(TEXT("`DSS` parses"), TryParseDreamShaderDecompileFormat(TEXT("DSS"), Parsed) && Parsed == EFormat::Dss);
	TestTrue(TEXT("`legacy` parses"), TryParseDreamShaderDecompileFormat(TEXT("legacy"), Parsed) && Parsed == EFormat::Legacy);
	TestTrue(TEXT("` auto ` parses"), TryParseDreamShaderDecompileFormat(TEXT(" auto "), Parsed) && Parsed == EFormat::Auto);
	Parsed = EFormat::Legacy;
	TestFalse(TEXT("`hlsl` does not"), TryParseDreamShaderDecompileFormat(TEXT("hlsl"), Parsed));
	TestTrue(TEXT("and leaves the format untouched"), Parsed == EFormat::Legacy);
	TestTrue(TEXT("the spelling reads back"), FString(LexDreamShaderDecompileFormat(EFormat::Dss)).Equals(TEXT("dss")));

	// Records keep code, severity, span and the fallback file.
	FResult Result;
	UE::DreamShader::Lang::FLangDiagnostic Diagnostic;
	Diagnostic.Code = TEXT("DSH9084");
	Diagnostic.Severity = UE::DreamShader::Lang::ELangSeverity::Warning;
	Diagnostic.Message = FText::FromString(TEXT("an input is unconnected"));
	Diagnostic.Span.Line = 7;
	Diagnostic.Span.Column = 3;
	Diagnostic.Span.Length = 5;
	Result.Diagnostics.Add(Diagnostic);

	TArray<::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord> Records;
	BuildDreamShaderDecompileDiagnosticRecords(Result, TEXT("C:/Fallback/M.dss"), Records);
	TestEqual(TEXT("one record per diagnostic"), Records.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
