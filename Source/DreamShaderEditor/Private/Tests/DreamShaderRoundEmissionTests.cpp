// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Decompiler/DreamShaderGraphImport.h"
#include "IR/IRGeneratedCoreCode.h"
#include "Preview/DreamShaderPreviewRenderer.h"
#include "Tools/DreamShaderDecompileTools.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionRound.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderingThread.h"
#include "RHIShaderPlatform.h"
#include "UObject/StrongObjectPtr.h"

namespace UE::DreamShader::Editor::Private::RoundEmissionTests
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	static UMaterial* Compile(FAutomationTestBase& Test, FDreamShaderCompile2Fixture& Fixture, const FString& Source)
	{
		if (!Fixture.WriteSource(Test, Source))
		{
			return nullptr;
		}
		UE::DreamShader::FDreamShaderError Error;
		if (!ExpectDreamShaderTestCompile(Test, TEXT("round material compiles"),
			CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, true, false), Error))
		{
			return nullptr;
		}
		const FString Path = Fixture.MakeObjectPath(TEXT("M_Round"));
		Fixture.TrackObjectPath(Path);
		UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
		Test.TestNotNull(TEXT("round material was emitted"), Material);
		return Material;
	}

	static int32 CountOp(FAutomationTestBase& Test, UMaterial* Material, IR::EIROp Op, const TCHAR* ClassName = nullptr)
	{
		IR::FIRModule Module;
		FGraphImportContext Context;
		Lang::FLangDiagnosticSink Diagnostics;
		if (!Test.TestTrue(TEXT("the actual material graph imports"),
			ImportDreamShaderGraphToIR(Material, FGraphImportOptions(), Module, Context, Diagnostics)))
		{
			return INDEX_NONE;
		}
		int32 Count = 0;
		for (const IR::FIRProduct& Product : Module.Products)
		{
			for (const IR::FIRNode& Node : Product.Graph.Nodes)
			{
				Count += Node.Op == Op && (!ClassName || Node.ClassName == ClassName) ? 1 : 0;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderRoundEmissionRoundtripTest,
	"DreamShader.Compiler2.Round.EmissionRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderRoundEmissionRoundtripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::RoundEmissionTests;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::IR;
	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("RoundEmission"));
	UMaterial* Material = Compile(*this, Fixture, TEXT(
		"#pragma material(ShadingModel = Unlit)\n"
		"uniform float Value = 0.5;\n"
		"export void M_Round(inout material m) {\n"
		" float4 values = float4(Value, Value, Value, Value);\n"
		" m.EmissiveColor = float3(round(Value), round(values.xy)) + round(values.xyz) + round(values).xyz;\n"
		" m.Roughness = UE.Round(Input = Value);\n"
		"}\n"));
	if (!Material)
	{
		return false;
	}
	TArray<UMaterialExpressionCustom*> Customs;
	int32 NativeRounds = 0;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (auto* Custom = Cast<UMaterialExpressionCustom>(Expression))
		{
			Customs.Add(Custom);
		}
		NativeRounds += Expression && Expression->IsA<UMaterialExpressionRound>() ? 1 : 0;
	}
	if (!TestEqual(TEXT("all four round widths use HLSL even for uniform inputs"), Customs.Num(), 4))
	{
		return false;
	}
	TestEqual(TEXT("only the explicit UE.Round stays native"), NativeRounds, 1);
	TestEqual(TEXT("generated rounds import as round"), CountOp(*this, Material, EIROp::Round), 4);
	TestEqual(TEXT("native Round keeps reflected semantics"), CountOp(*this, Material, EIROp::Reflected, TEXT("Round")), 1);

	UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
	Request.Asset = Material;
	Request.Format = UE::DreamShader::Editor::EDreamShaderDecompileFormat::Dss;
	Request.OutputFilePath = Fixture.GetSourceFilePath();
	const auto Decompiled = RunDreamShaderDecompileRequest(Request);
	if (!TestTrue(FString::Printf(TEXT("round decompiles: %s"), *Decompiled.Error), Decompiled.bSucceeded))
	{
		return false;
	}
	TestTrue(TEXT("source retains language round"), Decompiled.SourceText.Contains(TEXT("round(")));
	TestTrue(TEXT("source retains the native spelling"), Decompiled.SourceText.Contains(TEXT("UE.Round(")));
	TestFalse(TEXT("generated helper is not exposed as a user custom function"), Decompiled.SourceText.Contains(TEXT("@custom")));

	// A caption alone must not claim a user's implementation, and edited generated nodes must
	// preserve their extra behavior. Restore every property after each independent mutation.
	UMaterialExpressionCustom* Custom = Customs[0];
	const FString OriginalCode = Custom->Code;
	Custom->Code = TEXT("return round(Input);");
	TestEqual(TEXT("unmarked hand-written HLSL stays custom"), CountOp(*this, Material, EIROp::Round), 3);
	Custom->Code = OriginalCode;
	Custom->Code += TEXT("\n// user edit");
	TestEqual(TEXT("edited generated HLSL stays custom"), CountOp(*this, Material, EIROp::Round), 3);
	Custom->Code = OriginalCode;
	Custom->IncludeFilePaths.Add(TEXT("/Engine/Public/Platform.ush"));
	TestEqual(TEXT("extra includes prevent recovery"), CountOp(*this, Material, EIROp::Round), 3);
	Custom->IncludeFilePaths.Reset();
	Custom->Inputs.AddDefaulted_GetRef().InputName = TEXT("Extra");
	TestEqual(TEXT("extra input pins prevent recovery"), CountOp(*this, Material, EIROp::Round), 3);
	Custom->Inputs.SetNum(1);
	Custom->AdditionalOutputs.AddDefaulted_GetRef().OutputName = TEXT("Extra");
	TestEqual(TEXT("extra outputs prevent recovery"), CountOp(*this, Material, EIROp::Round), 3);
	Custom->AdditionalOutputs.Reset();
	const auto OriginalOutput = Custom->OutputType;
	Custom->OutputType = OriginalOutput == CMOT_Float4 ? CMOT_Float1 : CMOT_Float4;
	TestEqual(TEXT("changed output width prevents recovery"), CountOp(*this, Material, EIROp::Round), 3);
	Custom->OutputType = OriginalOutput;

	// Compile the actual decompiler output through the normal service, not just parse it.
	Material = Compile(*this, Fixture, Decompiled.SourceText);
	if (!Material)
	{
		return false;
	}
	TestEqual(TEXT("round survives graph-source-graph"), CountOp(*this, Material, EIROp::Round), 4);
	TestEqual(TEXT("UE.Round survives graph-source-graph"), CountOp(*this, Material, EIROp::Reflected, TEXT("Round")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderRoundEmissionRenderTest,
	"DreamShader.Compiler2.Round.RenderUniformAndVarying",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderRoundEmissionRenderTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::RoundEmissionTests;
	using namespace UE::DreamShader::Editor::Private::Tests;
	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("RoundRender"));
	UMaterial* Material = Compile(*this, Fixture, TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"uniform float Value = 0;\n"
		"uniform float Case = 0;\n"
		"export void M_Round(inout material m) {\n"
		" float reference = Case < 0.5 ? round(0.5) : Case < 1.5 ? round(1.5) : Case < 2.5 ? round(2.5)\n"
		"   : Case < 3.5 ? round(-0.5) : Case < 4.5 ? round(-1.5) : round(-2.5);\n"
		" float2 uv = UE.TextureCoordinate();\n"
		" float varying = Value + floor(uv.x);\n"
		" m.EmissiveColor = float3(abs(round(Value) - reference), 1, abs(round(varying) - reference));\n"
		"}\n"));
	if (!Material)
	{
		return false;
	}
	Material->EnsureIsComplete();
	FlushRenderingCommands();
	const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
	if (!TestTrue(TEXT("round material has complete shaders"), Resource && Resource->IsGameThreadShaderMapComplete())
		|| !TestTrue(TEXT("round shader compilation has no errors"), Resource && Resource->GetCompileErrors().IsEmpty()))
	{
		return false;
	}
	TStrongObjectPtr<UMaterialInstanceDynamic> Instance(UMaterialInstanceDynamic::Create(Material, GetTransientPackage()));
	FDreamShaderPreviewRenderContext Renderer;
	const float Inputs[] = { 0.5f, 1.5f, 2.5f, -0.5f, -1.5f, -2.5f };
	constexpr int32 Size = 64;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Inputs); ++Index)
	{
		Instance->SetScalarParameterValue(TEXT("Value"), Inputs[Index]);
		Instance->SetScalarParameterValue(TEXT("Case"), float(Index));
		TArray<FColor> Pixels;
		FString Error;
		// Reuse the preview's real scene draw and GPU readback. Green means uniform and
		// varying evaluation both match the independently folded literal. UV is in [0,1)
		// at the centre of the preview sphere, so the varying addend is zero there.
		const bool bDrew = Renderer.RenderFramePixels(Instance.Get(), Size, Size, TEXT("sphere"), -157.5f, -11.25f, Pixels, Error);
		if (!TestTrue(FString::Printf(TEXT("round(%g) draws: %s"), Inputs[Index], *Error), bDrew))
		{
			return false;
		}
		if (!TestEqual(TEXT("a complete frame reads back"), Pixels.Num(), Size * Size))
		{
			return false;
		}
		const FColor Center = Pixels[(Size / 2) * Size + Size / 2];
		TestTrue(FString::Printf(TEXT("round(%g) matches folded constant: pixel %s"), Inputs[Index], *Center.ToString()),
			Center.G > 150 && Center.R < 40 && Center.B < 40);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
