// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Compiler2.Provenance.* -- the headless cores of the provenance actions (Provenance/
// DreamShaderProvenanceActions.h) in 2.0: Adopt writes 2.0 text through the 2.0 decompiler, a 1.x source
// adopts as a migration, a `.dsi` adopts as a splice, and a ThinCustom instance's tweaks go either into the source's
// defaults or into a `.dsi` of their own.
//
// Every case asserts the two halves of a refusal too: nothing was written, and no backup was made.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderMaterialInstance.h"
#include "Provenance/DreamShaderProvenanceActions.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::Provenance2Tests
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	inline void ExpectFixtureNoise(FAutomationTestBase& Test, const FDreamShaderCompile2Fixture& Fixture)
	{
		Test.AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
	}

	inline bool Compile(FAutomationTestBase& Test, const FString& SourceFilePath)
	{
		UE::DreamShader::FDreamShaderError Error;
		if (!CompileDreamShaderTestAssets(SourceFilePath, Error, /*bForce*/ true, /*bEphemeralThinCustom*/ false))
		{
			ExpectDreamShaderTestCompile(Test, TEXT("the provenance fixture compiles"), false, Error);
			return false;
		}
		return true;
	}

	inline FString Describe(const FDreamShaderProvenanceOutcome& Outcome)
	{
		return FString::Printf(TEXT("%s %s | compiled=%d %s"), *Outcome.Code, *ToInvariantWireString(Outcome.Message), Outcome.bCompiled ? 1 : 0, *Outcome.CompileMessage);
	}

	inline FString LoadText(const FString& FilePath)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *FilePath);
		return Text;
	}
}

// ---------------------------------------------------------------------------------------------
// Adopt over a `.dss` with several products
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderProvenance2AdoptDssTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Provenance.AdoptDss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderProvenance2AdoptDssTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Provenance2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("PvLibrary"), TEXT("Provenance2"));
	ExpectFixtureNoise(*this, Fixture);

	if (!Fixture.WriteSource(*this, TEXT(
		"// A library of two.\n"
		"export float MF_PvScale(float Value, float Scale = 2.0)\n"
		"{\n"
		"    return Value * Scale;\n"
		"}\n"
		"\n"
		"export float3 MF_PvTint(float3 Colour)\n"
		"{\n"
		"    return Colour * float3(1, 0.5, 0.25);\n"
		"}\n")) || !Compile(*this, Fixture.GetSourceFilePath()))
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MF_PvScale")));
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MF_PvTint")));

	UMaterialFunction* Scale = LoadObject<UMaterialFunction>(nullptr, *Fixture.MakeObjectPath(TEXT("MF_PvScale")));
	if (!TestNotNull(TEXT("the function asset"), Scale))
	{
		return false;
	}

	// The hand edit: the optional input's preview value, which is its default in source.
	bool bEdited = false;
	for (const TObjectPtr<UMaterialExpression>& Expression : Scale->GetExpressions())
	{
		UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression.Get());
		if (Input && Input->InputName == FName(TEXT("Scale")))
		{
			Input->PreviewValue = FVector4f(3.0f, 3.0f, 3.0f, 3.0f);
			bEdited = true;
		}
	}
	if (!TestTrue(TEXT("the Scale input was found and edited"), bEdited))
	{
		return false;
	}
	TestTrue(TEXT("the edit reads as Diverged"), ClassifyGeneratedAsset(Scale) == EDreamShaderDigestState::Diverged);

	const FDreamShaderProvenanceOutcome Outcome = AdoptGeneratedAssetIntoSourceCore(Scale, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
	TestTrue(FString::Printf(TEXT("Adopt succeeds (%s)"), *Describe(Outcome)), Outcome.bSucceeded);
	TestTrue(TEXT("and rebuilt"), Outcome.bCompiled);
	TestTrue(TEXT("a backup was made"), !Outcome.BackupFilePath.IsEmpty() && IFileManager::Get().FileExists(*Outcome.BackupFilePath));

	const FString Adopted = LoadText(Fixture.GetSourceFilePath());
	TestTrue(FString::Printf(TEXT("the source states the edit\n%s"), *Adopted), Adopted.Contains(TEXT("Scale = 3.0")));
	TestTrue(TEXT("and still declares the product nobody touched"), Adopted.Contains(TEXT("MF_PvTint(")));

	Scale = LoadObject<UMaterialFunction>(nullptr, *Fixture.MakeObjectPath(TEXT("MF_PvScale")));
	TestTrue(TEXT("the adopted asset reads as Generated"), Scale && ClassifyGeneratedAsset(Scale) == EDreamShaderDigestState::Generated);
	TestNotNull(TEXT("the other product is still at its path"), LoadObject<UMaterialFunction>(nullptr, *Fixture.MakeObjectPath(TEXT("MF_PvTint"))));

	if (!Outcome.BackupFilePath.IsEmpty())
	{
		IFileManager::Get().Delete(*Outcome.BackupFilePath, false, true, true);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Adopt over a 1.x source is a migration
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderProvenance2AdoptMigrationTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Provenance.AdoptMigratesLegacySource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderProvenance2AdoptMigrationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Provenance2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("M_PvLegacy"), TEXT("Provenance2"), TEXT("dsm"));
	ExpectFixtureNoise(*this, Fixture);

	const FString Source = FString::Printf(TEXT(
		"Shader(Name=\"%s\", Root=\"Game\")\n"
		"{\n"
		"    Properties = { float Gain = 0.5; }\n"
		"    Settings = { ShadingModel = \"Unlit\"; Backend = \"Graph\"; }\n"
		"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
		"    Graph = { Color = vec3(Gain, Gain, Gain); }\n"
		"}\n"), *Fixture.MakeLegacyAssetName(TEXT("M_PvLegacy")));
	if (!Fixture.WriteSource(*this, Source) || !Compile(*this, Fixture.GetSourceFilePath()))
	{
		return false;
	}
	const FString ObjectPath = Fixture.MakeObjectPath(TEXT("M_PvLegacy"));
	Fixture.TrackObjectPath(ObjectPath);
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the 1.x source's material"), Material))
	{
		return false;
	}

	const FString DssPath = FPaths::ChangeExtension(Fixture.GetSourceFilePath(), TEXT("dss"));

	// A `.dss` in the way: refused, nothing moved.
	{
		FFileHelper::SaveStringToFile(FString(TEXT("// in the way\n")), *DssPath);
		const FDreamShaderProvenanceOutcome Refused = AdoptGeneratedAssetIntoSourceCore(Material, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
		TestFalse(TEXT("an existing `.dss` refuses the migration"), Refused.bSucceeded);
		TestEqual(TEXT("nothing was written"), Refused.WrittenFiles.Num(), 0);
		TestTrue(TEXT("no backup was made"), Refused.BackupFilePath.IsEmpty());
		TestTrue(TEXT("the `.dsm` is where it was"), IFileManager::Get().FileExists(*Fixture.GetSourceFilePath()));
		IFileManager::Get().Delete(*DssPath, false, true, true);
	}

	const FDreamShaderProvenanceOutcome Outcome = AdoptGeneratedAssetIntoSourceCore(Material, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
	TestTrue(FString::Printf(TEXT("Adopt migrates (%s)"), *Describe(Outcome)), Outcome.bSucceeded && Outcome.bCompiled);
	TestTrue(TEXT("the `.dss` is written"), IFileManager::Get().FileExists(*DssPath));
	TestFalse(TEXT("the `.dsm` is gone"), IFileManager::Get().FileExists(*Fixture.GetSourceFilePath()));
	TestTrue(TEXT("and is kept as a backup"), !Outcome.BackupFilePath.IsEmpty() && IFileManager::Get().FileExists(*Outcome.BackupFilePath));
	TestNotNull(TEXT("the rebuild made the same object path"), LoadObject<UMaterial>(nullptr, *ObjectPath));

	IFileManager::Get().Delete(*DssPath, false, true, true);
	if (!Outcome.BackupFilePath.IsEmpty())
	{
		IFileManager::Get().Delete(*Outcome.BackupFilePath, false, true, true);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Adopt of a `.dsi`
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderProvenance2AdoptInstanceTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Provenance.AdoptInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderProvenance2AdoptInstanceTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Provenance2Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("MI_PvInstance"), TEXT("Provenance2"), TEXT("dsi"));
	ExpectFixtureNoise(*this, Fixture);

	FString ParentPath;
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_PvParent.dss"), TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"uniform float Gain = 0.5;\n"
		"uniform float Bias = 0.25;\n"
		"export void M_PvParent(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = float3(Gain, Bias, 0);\n"
		"}\n"), ParentPath))
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("M_PvParent")));

	const FString Original = TEXT(
		"// the instance\n"
		"#pragma instance(Parent = \"M_PvParent\")\n"
		"\n"
		"// tuned by hand\n"
		"uniform float Gain = 2.0; // keep me\n"
		"uniform float Bias = 0.75;\n");
	if (!Fixture.WriteSource(*this, Original) || !Compile(*this, Fixture.GetSourceFilePath()))
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MI_PvInstance")));

	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_PvInstance")));
	if (!TestNotNull(TEXT("the instance"), Instance))
	{
		return false;
	}

	// Nothing changed: nothing written, no backup, and still rebuilt.
	{
		const FDreamShaderProvenanceOutcome Unchanged = AdoptInstanceIntoSource(Instance, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
		TestTrue(FString::Printf(TEXT("unchanged: succeeds (%s)"), *Describe(Unchanged)), Unchanged.bSucceeded);
		TestEqual(TEXT("unchanged: no file written"), Unchanged.WrittenFiles.Num(), 0);
		TestTrue(TEXT("unchanged: no backup"), Unchanged.BackupFilePath.IsEmpty());
		TestTrue(TEXT("unchanged: the text is the author's"), LoadText(Fixture.GetSourceFilePath()).Equals(Original, ESearchCase::CaseSensitive));
	}

	Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_PvInstance")));
	if (!TestNotNull(TEXT("the instance, again"), Instance))
	{
		return false;
	}
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Gain"), 3.5f);

	const FDreamShaderProvenanceOutcome Outcome = AdoptInstanceIntoSource(Instance, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
	TestTrue(FString::Printf(TEXT("Adopt succeeds (%s)"), *Describe(Outcome)), Outcome.bSucceeded && Outcome.bCompiled);

	const FString Adopted = LoadText(Fixture.GetSourceFilePath());
	const FString Expected = Original.Replace(TEXT("Gain = 2.0;"), TEXT("Gain = 3.5;"), ESearchCase::CaseSensitive);
	TestTrue(FString::Printf(TEXT("only the initializer changed\n%s"), *Adopted), Adopted.Equals(Expected, ESearchCase::CaseSensitive));
	TestTrue(TEXT("a backup was made"), !Outcome.BackupFilePath.IsEmpty() && IFileManager::Get().FileExists(*Outcome.BackupFilePath));

	Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_PvInstance")));
	TestTrue(TEXT("the adopted instance reads as Generated"), Instance && ClassifyGeneratedAsset(Instance) == EDreamShaderDigestState::Generated);

	if (!Outcome.BackupFilePath.IsEmpty())
	{
		IFileManager::Get().Delete(*Outcome.BackupFilePath, false, true, true);
	}

	// A `.dsi` with a directive: the splice would write the untaken branch back as blank lines.
	{
		const FString Conditional = FString(TEXT("#if DS_NEVER_DEFINED_FOR_TESTS\n#endif\n")) + Adopted;
		if (Fixture.WriteSource(*this, Conditional) && Instance)
		{
			const FDreamShaderProvenanceOutcome Refused = AdoptInstanceIntoSource(Instance, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
			TestFalse(TEXT("a `.dsi` with `#if` is refused"), Refused.bSucceeded);
			TestEqual(TEXT("with DSH8149"), Refused.Code, FString(TEXT("DSH8149")));
			TestTrue(TEXT("and nothing was written"), Refused.WrittenFiles.Num() == 0 && Refused.BackupFilePath.IsEmpty());
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The two answers to a Tweaked ThinCustom instance
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderProvenance2TweaksTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Provenance.Tweaks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderProvenance2TweaksTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Provenance2Tests;

	const TCHAR* Source = TEXT(
		"#pragma material(Backend = ThinCustom, ShadingModel = Unlit, BlendMode = Opaque)\n"
		"// the roughness an artist tunes\n"
		"uniform float Roughness = 0.5; // keep me\n"
		"export void {NAME}(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = float3(Roughness, Roughness, Roughness);\n"
		"}\n");

	// Adopt Tweaks: the override becomes the source's default.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("M_PvTweakAdopt"), TEXT("Provenance2"));
		ExpectFixtureNoise(*this, Fixture);
		if (!Fixture.WriteSource(*this, FString(Source).Replace(TEXT("{NAME}"), TEXT("M_PvTweakAdopt"))) || !Compile(*this, Fixture.GetSourceFilePath()))
		{
			return false;
		}
		const FString ObjectPath = Fixture.MakeObjectPath(TEXT("M_PvTweakAdopt"));
		Fixture.TrackObjectPath(ObjectPath);
		UDreamShaderMaterialInstance* Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
		if (!TestNotNull(TEXT("the ThinCustom instance"), Instance))
		{
			return false;
		}
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.25f);
		TestTrue(TEXT("a tuned instance reads as Tweaked"), ClassifyGeneratedAsset(Instance) == EDreamShaderDigestState::Tweaked);

		// A Tweaked instance does not Adopt as a graph: the message sends the user to the two tweak actions.
		const FDreamShaderProvenanceOutcome Graph = AdoptGeneratedAssetIntoSourceCore(Instance, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
		TestFalse(TEXT("Adopt Into Source refuses a Tweaked instance"), Graph.bSucceeded);
		TestTrue(TEXT("and wrote nothing"), Graph.WrittenFiles.Num() == 0 && Graph.BackupFilePath.IsEmpty());

		const FDreamShaderProvenanceOutcome Outcome = AdoptTweaksIntoSourceDefaultsCore(Instance, Fixture.GetSourceFilePath(), /*bWriteBackup*/ true);
		TestTrue(FString::Printf(TEXT("Adopt Tweaks succeeds (%s)"), *Describe(Outcome)), Outcome.bSucceeded && Outcome.bCompiled);

		const FString Adopted = LoadText(Fixture.GetSourceFilePath());
		TestTrue(FString::Printf(TEXT("the default is the tweak, and the line's comment stays\n%s"), *Adopted), Adopted.Contains(TEXT("uniform float Roughness = 0.25; // keep me")));
		TestTrue(TEXT("the comment above it stays"), Adopted.Contains(TEXT("// the roughness an artist tunes")));

		Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
		if (TestNotNull(TEXT("the rebuilt instance"), Instance))
		{
			FMaterialParameterMetadata Meta;
			TestFalse(TEXT("the instance carries no override any more"), Instance->GetParameterOverrideValue(EMaterialParameterType::Scalar, FMaterialParameterInfo(TEXT("Roughness")), Meta));
			TestTrue(TEXT("and reads as Generated"), ClassifyGeneratedAsset(Instance) == EDreamShaderDigestState::Generated);
		}
		if (!Outcome.BackupFilePath.IsEmpty())
		{
			IFileManager::Get().Delete(*Outcome.BackupFilePath, false, true, true);
		}
	}

	// Extract Tweaks: the override moves into a `.dsi` whose parent is the instance.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("M_PvTweakExtract"), TEXT("Provenance2"));
		ExpectFixtureNoise(*this, Fixture);
		if (!Fixture.WriteSource(*this, FString(Source).Replace(TEXT("{NAME}"), TEXT("M_PvTweakExtract"))) || !Compile(*this, Fixture.GetSourceFilePath()))
		{
			return false;
		}
		const FString ObjectPath = Fixture.MakeObjectPath(TEXT("M_PvTweakExtract"));
		Fixture.TrackObjectPath(ObjectPath);
		UDreamShaderMaterialInstance* Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
		if (!TestNotNull(TEXT("the ThinCustom instance"), Instance))
		{
			return false;
		}
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.75f);

		const FString DefaultPath = MakeDefaultTweaksInstanceSourcePath(Instance, Fixture.GetSourceFilePath());
		TestTrue(FString::Printf(TEXT("the default target is `MI_<name>.dsi` beside the source ('%s')"), *DefaultPath), DefaultPath.EndsWith(TEXT("/MI_M_PvTweakExtract.dsi"), ESearchCase::IgnoreCase));

		const FDreamShaderProvenanceOutcome Outcome = ExtractTweaksToInstanceSourceCore(Instance, FString());
		TestTrue(FString::Printf(TEXT("Extract Tweaks succeeds (%s)"), *Describe(Outcome)), Outcome.bSucceeded && Outcome.bCompiled);
		if (Outcome.WrittenFiles.Num() == 1)
		{
			const FString Written = LoadText(Outcome.WrittenFiles[0]);
			TestTrue(FString::Printf(TEXT("the `.dsi` names the instance as its parent\n%s"), *Written), Written.Contains(TEXT("#pragma instance(Parent = ")) && Written.Contains(TEXT("M_PvTweakExtract")));
			TestTrue(TEXT("and states the tweak"), Written.Contains(TEXT("uniform float Roughness = 0.75;")));
			Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MI_M_PvTweakExtract")));

			// The tweak moved: the ThinCustom instance is what its source says again.
			Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
			if (TestNotNull(TEXT("the ThinCustom instance after the extract"), Instance))
			{
				FMaterialParameterMetadata Meta;
				TestFalse(TEXT("the tweak left the ThinCustom instance"), Instance->GetParameterOverrideValue(EMaterialParameterType::Scalar, FMaterialParameterInfo(TEXT("Roughness")), Meta));

				// A second extract finds the file in its way, and keeps the new tweak where it is.
				UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.9f);
				const FDreamShaderProvenanceOutcome Second = ExtractTweaksToInstanceSourceCore(Instance, Outcome.WrittenFiles[0]);
				TestFalse(TEXT("an existing target is refused"), Second.bSucceeded);
				TestTrue(TEXT("and the tweak stays on the instance"), Instance->GetParameterOverrideValue(EMaterialParameterType::Scalar, FMaterialParameterInfo(TEXT("Roughness")), Meta));
			}
		}
		else
		{
			AddError(FString::Printf(TEXT("Extract Tweaks wrote %d file(s), expected one"), Outcome.WrittenFiles.Num()));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
