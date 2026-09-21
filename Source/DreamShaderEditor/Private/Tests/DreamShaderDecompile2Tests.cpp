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
#include "Materials/MaterialInstanceConstant.h"

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
