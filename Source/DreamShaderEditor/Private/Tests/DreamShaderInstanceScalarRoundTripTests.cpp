// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Decompiler/DreamShaderInstanceDecompiler.h"
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderVersionCompat.h"
#include "Provenance/DreamShaderProvenanceActions.h"

#include "Materials/MaterialInstanceConstant.h"
#if DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER
#include "Materials/MaterialParameters.h"
#else
#include "MaterialTypes.h"
#endif

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceScalarRoundTripTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Provenance.ScalarRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceScalarRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	namespace Compiler = UE::DreamShader::Editor::Compiler;
	namespace Lang = UE::DreamShader::Lang;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("MI_ScalarRoundTrip"), TEXT("ScalarRoundTrip"), TEXT("dsi"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("M_ScalarRoundTripParent")));
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MI_ScalarRoundTrip")));

	FString ParentPath;
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_ScalarRoundTripParent.dss"), TEXT(
		"#pragma material(Backend = Graph, ShadingModel = Unlit)\n"
		"uniform bool Flag = true;\n"
		"uniform int Count = 1;\n"
		"uniform uint Unsigned = 1;\n"
		"uniform int Overflow = 1;\n"
		"uniform bool BoolControl = false;\n"
		"uniform int IntControl = 2;\n"
		"uniform uint UIntControl = 3;\n"
		"export void M_ScalarRoundTripParent(inout material m)\n"
		"{ m.EmissiveColor = float3(Flag ? 1.0 : 0.0, Count + IntControl + Unsigned + Overflow + UIntControl, BoolControl ? 1.0 : 0.0); }\n"), ParentPath)
		|| !Fixture.WriteSource(*this, TEXT(
			"#pragma instance(Parent = \"M_ScalarRoundTripParent\")\n"
			"uniform bool Flag = true; // retain flag comment\n"
			"uniform int Count = 1; // retain count comment\n"
			"uniform uint Unsigned = 1;\n"
			"uniform int Overflow = 1;\n"
			"uniform bool BoolControl = false;\n"
			"uniform int IntControl = 2;\n"
			"uniform uint UIntControl = 3;\n")))
	{
		return false;
	}
	UE::DreamShader::FDreamShaderError CompileError;
	if (!ExpectDreamShaderTestCompile(*this, TEXT("the scalar fixture compiles"),
		CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), CompileError, /*bForce*/ true, /*bEphemeralThinCustom*/ false), CompileError))
	{
		return false;
	}
	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_ScalarRoundTrip")));
	if (!TestNotNull(TEXT("the instance exists"), Instance))
	{
		return false;
	}
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Flag")), 2.0f);
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Count")), 0.5f);
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Unsigned")), -2.0f);
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Overflow")), 2147483648.0f);

	Compiler::FDreamShaderLang2PipelineOptions CheckOptions;
	CheckOptions.bEmitAssets = false;
	Compiler::FDreamShaderLang2PipelineResult Checked;
	if (!TestTrue(TEXT("the source checks and supplies its parent's declared types"),
		Compiler::RunDreamShaderLang2Pipeline(Fixture.GetSourceFilePath(), CheckOptions, Checked)))
	{
		return false;
	}
	FInstanceDecompileOptions Options;
	Options.Filter = EInstanceDecompileFilter::OverriddenOnly;
	Options.ParentSchema = Checked.ParentSchema.Get();
	FString Exported;
	Lang::FLangDiagnosticSink Diagnostics(Fixture.GetSourceFilePath());
	if (TestTrue(TEXT("the live scalar overrides decompile"), DecompileMaterialInstanceToText(Instance, Options, Exported, Diagnostics)))
	{
		TestTrue(FString::Printf(TEXT("a non-boolean scalar exports without coercion\n%s"), *Exported), Exported.Contains(TEXT("uniform float Flag = 2.0;")));
		TestTrue(FString::Printf(TEXT("a fractional scalar exports without rounding\n%s"), *Exported), Exported.Contains(TEXT("uniform float Count = 0.5;")));
		TestTrue(TEXT("a negative uint scalar exports as float"), Exported.Contains(TEXT("uniform float Unsigned = -2.0;")));
		TestTrue(TEXT("a scalar beyond int range exports as float"), Exported.Contains(TEXT("uniform float Overflow = 2147483648.0;")));
	}

	const FDreamShaderProvenanceOutcome Adopted = AdoptInstanceIntoSource(Instance, Fixture.GetSourceFilePath(), /*bWriteBackup*/ false);
	TestTrue(FString::Printf(TEXT("Adopt succeeds (%s | %s)"), *ToInvariantWireString(Adopted.Message), *Adopted.CompileMessage), Adopted.bSucceeded && Adopted.bCompiled);
	Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_ScalarRoundTrip")));
	if (!TestNotNull(TEXT("the instance still exists after Adopt"), Instance))
	{
		return false;
	}
	float Value = 0.0f;
	TestTrue(TEXT("Flag remains a scalar override"), Instance->GetScalarParameterValue(FName(TEXT("Flag")), Value));
	TestEqual(TEXT("Adopt preserves the exact non-boolean scalar"), Value, 2.0f);
	TestTrue(TEXT("Count remains a scalar override"), Instance->GetScalarParameterValue(FName(TEXT("Count")), Value));
	TestEqual(TEXT("Adopt preserves the fractional scalar"), Value, 0.5f);
	TestTrue(TEXT("Unsigned remains a scalar override"), Instance->GetScalarParameterValue(FName(TEXT("Unsigned")), Value));
	TestEqual(TEXT("Adopt preserves the negative unsigned override"), Value, -2.0f);
	TestTrue(TEXT("Overflow remains a scalar override"), Instance->GetScalarParameterValue(FName(TEXT("Overflow")), Value));
	TestEqual(TEXT("Adopt preserves the scalar beyond int range"), Value, 2147483648.0f);
	TestTrue(TEXT("the boolean control remains a scalar"), Instance->GetScalarParameterValue(FName(TEXT("BoolControl")), Value));
	TestEqual(TEXT("the representable boolean control stays false"), Value, 0.0f);
	TestTrue(TEXT("the integer control remains a scalar"), Instance->GetScalarParameterValue(FName(TEXT("IntControl")), Value));
	TestEqual(TEXT("the representable integer control stays two"), Value, 2.0f);
	TestTrue(TEXT("the uint control remains a scalar"), Instance->GetScalarParameterValue(FName(TEXT("UIntControl")), Value));
	TestEqual(TEXT("the representable uint control stays three"), Value, 3.0f);

	FString Text;
	TestTrue(TEXT("the adopted source can be read"), FFileHelper::LoadFileToString(Text, *Fixture.GetSourceFilePath()));
	TestTrue(TEXT("the boolean control keeps its declaration"), Text.Contains(TEXT("uniform bool BoolControl = false;")));
	TestTrue(TEXT("the integer control keeps its declaration"), Text.Contains(TEXT("uniform int IntControl = 2;")));
	TestTrue(TEXT("the uint control keeps its declaration"), Text.Contains(TEXT("uniform uint UIntControl = 3;")));
	TestTrue(TEXT("the flag comment survives"), Text.Contains(TEXT("// retain flag comment")));
	TestTrue(TEXT("the count comment survives"), Text.Contains(TEXT("// retain count comment")));
	return true;
}

#endif
