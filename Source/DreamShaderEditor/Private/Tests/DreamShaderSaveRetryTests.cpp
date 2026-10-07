#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamPassPipeline.h"
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderMaterialInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "FileHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "UObject/Package.h"

namespace UE::DreamShader::Editor::Private::SaveRetryTests
{
	using namespace Tests;

	class FScopedSaveFailure
	{
	public:
		explicit FScopedSaveFailure(FDreamShaderAssetSaveFailurePredicate Predicate)
			: Saved(MoveTemp(GetDreamShaderAssetSaveFailureOverride()))
		{
			GetDreamShaderAssetSaveFailureOverride() = MoveTemp(Predicate);
		}
		~FScopedSaveFailure() { GetDreamShaderAssetSaveFailureOverride() = MoveTemp(Saved); }
	private:
		FDreamShaderAssetSaveFailurePredicate Saved;
	};

	bool Compile(const FDreamShaderCompile2Fixture& Fixture, FAutomationTestBase& Test, bool bExpected)
	{
		FDreamShaderError Error;
		const bool bCompiled = CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ false, /*bEphemeralThinCustom*/ false);
		Test.TestEqual(FString::Printf(TEXT("compile returns %s (%s: %s)"), bExpected ? TEXT("success") : TEXT("failure"), *Error.Code, *Error.Message), bCompiled, bExpected);
		return bCompiled;
	}

	TArray<uint8> ReadAssetFile(FAutomationTestBase& Test, UObject* Asset)
	{
		TArray<uint8> Bytes;
		const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		Test.TestTrue(TEXT("the saved package can be read"), FFileHelper::LoadFileToArray(Bytes, *Filename));
		return Bytes;
	}

	bool Reload(FAutomationTestBase& Test, const TArray<UPackage*>& Packages)
	{
		FText Error;
		return Test.TestTrue(TEXT("reload the fixture's saved packages"),
			UPackageTools::ReloadPackages(Packages, Error, EReloadPackagesInteractionMode::AssumePositive));
	}
}

IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(
	FDreamShaderSaveRetryAssetsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.SaveRetry.Assets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FDreamShaderSaveRetryAssetsTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const TCHAR* Kind : { TEXT("Graph"), TEXT("ThinCustom"), TEXT("Function"), TEXT("Instance") })
	{
		OutBeautifiedNames.Add(Kind);
		OutTestCommands.Add(Kind);
	}
}

bool FDreamShaderSaveRetryAssetsTest::RunTest(const FString& Kind)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::SaveRetryTests;
	const FString Name = TEXT("M_SaveRetry_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const bool bInstance = Kind == TEXT("Instance");
	FDreamShaderCompile2Fixture Fixture(Name, TEXT("SaveRetry"), bInstance ? TEXT("dsi") : TEXT("dss"));
	const FString ObjectPath = Fixture.MakeObjectPath(Name);
	Fixture.TrackObjectPath(ObjectPath);
	const FString ParentName = Name + TEXT("_Parent");
	if (bInstance)
	{
		FString ParentSourcePath;
		if (!Fixture.WriteSiblingSource(*this, ParentName + TEXT(".dss"), FString::Printf(TEXT(
			"#pragma material(Backend = Graph, ShadingModel = Unlit)\nuniform float Gain = 1.0;\n"
			"export void %s(inout material m) { m.EmissiveColor = float3(Gain, 0, 0); }\n"), *ParentName), ParentSourcePath))
		{
			return false;
		}
		Fixture.TrackObjectPath(Fixture.MakeObjectPath(ParentName));
	}
	auto Source = [&](const TCHAR* Value)
	{
		if (bInstance)
		{
			return FString::Printf(TEXT("#pragma instance(Parent = \"%s\")\nuniform float Gain = %s;\n"), *ParentName, Value);
		}
		if (Kind == TEXT("Function"))
		{
			return FString::Printf(TEXT("export float %s(float Input) { return Input * %s; }\n"), *Name, Value);
		}
		return FString::Printf(TEXT("#pragma material(Backend = %s, ShadingModel = Unlit)\n"
			"export void %s(inout material m) { m.EmissiveColor = float3(%s, 0, 0); }\n"), *Kind, *Name, Value);
	};
	if (!Fixture.WriteSource(*this, Source(TEXT("0.25"))) || !Compile(Fixture, *this, true))
	{
		return false;
	}
	UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the initial generated asset exists"), Asset))
	{
		return false;
	}
	const TArray<uint8> OriginalFile = ReadAssetFile(*this, Asset);
	const FString OriginalDigest = BuildOutputDigest(Asset);
	bool bFailSave = true;
	int32 SaveAttempts = 0;
	FScopedSaveFailure Failure([&](const TArray<UPackage*>& Packages)
	{
		if (Packages.Contains(Asset->GetOutermost()))
		{
			++SaveAttempts;
			return bFailSave;
		}
		return false;
	});
	if (!Fixture.WriteSource(*this, Source(TEXT("0.5"))))
	{
		return false;
	}
	Compile(Fixture, *this, false);
	TestEqual(TEXT("the failure reached saving"), SaveAttempts, 1);
	TestTrue(TEXT("failed saving leaves the original file intact"), ReadAssetFile(*this, Asset) == OriginalFile);
	TestTrue(TEXT("a failed save disables the source-hash skip"), GetGeneratedAssetSourceHash(Asset).IsEmpty());
	TestTrue(TEXT("the source owner is retained"), HasDreamShaderSourceMetadata(Asset));
	const FString UpdatedDigest = BuildOutputDigest(Asset);
	TestTrue(TEXT("the new graph remains in memory"), UpdatedDigest != OriginalDigest);
	TestTrue(TEXT("its divergence baseline is retained"), GetOutputDigestMetadata(Asset) == UpdatedDigest);

	bFailSave = false;
	if (!Compile(Fixture, *this, true))
	{
		return false;
	}
	TestEqual(TEXT("an unchanged-source retry actually saves"), SaveAttempts, 2);
	TestFalse(TEXT("the successful retry stamps the source hash again"), GetGeneratedAssetSourceHash(Asset).IsEmpty());
	TestTrue(TEXT("the saved file changed"), ReadAssetFile(*this, Asset) != OriginalFile);
	if (!Reload(*this, { Asset->GetOutermost() }))
	{
		return false;
	}
	Asset = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the retried asset reloads"), Asset))
	{
		return false;
	}
	TestTrue(TEXT("reloading keeps the rebuilt content"), BuildOutputDigest(Asset) == UpdatedDigest);
	TestFalse(TEXT("reloading keeps the successful source stamp"), GetGeneratedAssetSourceHash(Asset).IsEmpty());

	// Retry must still respect edits made after the unsuccessful compile; invalidating all metadata would lose that guard.
	bFailSave = true;
	if (!Fixture.WriteSource(*this, Source(TEXT("0.75"))))
	{
		return false;
	}
	Compile(Fixture, *this, false);
	if (UMaterial* Material = Cast<UMaterial>(Asset))
	{
		Material->TwoSided = true;
	}
	else if (UDreamShaderMaterialInstance* Thin = Cast<UDreamShaderMaterialInstance>(Asset))
	{
		CastChecked<UMaterial>(Thin->Parent)->TwoSided = true;
	}
	else if (UMaterialFunction* Function = Cast<UMaterialFunction>(Asset))
	{
		Function->Description = TEXT("User edit after failed save");
	}
	else if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset))
	{
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Gain")), 9.0f);
	}
	const FString HandEditedDigest = BuildOutputDigest(Asset);
	TestTrue(TEXT("a later hand edit is still classified as divergent"), ClassifyGeneratedAsset(Asset) == EDreamShaderDigestState::Diverged);
	const int32 AttemptsBeforeRefusal = SaveAttempts;
	bFailSave = false;
	Compile(Fixture, *this, false);
	TestEqual(TEXT("the divergence gate stops before saving"), SaveAttempts, AttemptsBeforeRefusal);
	TestTrue(TEXT("retry preserves the hand edit"), BuildOutputDigest(Asset) == HandEditedDigest);
	return true;
}

#if DREAMSHADER_WITH_CUSTOM_PASS
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSaveRetryPipelineTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.SaveRetry.PipelineExports",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSaveRetryPipelineTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::SaveRetryTests;
	const FString Name = TEXT("CP_SaveRetry_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FDreamShaderCompile2Fixture Fixture(Name, TEXT("SaveRetry"), TEXT("dsp"));
	const FString PipelinePath = Fixture.MakeObjectPath(Name);
	const FString TargetPath = Fixture.MakeObjectPath(Name + TEXT("_Glow"));
	Fixture.TrackObjectPath(PipelinePath);
	Fixture.TrackObjectPath(TargetPath);
	auto Source = [](int32 Size)
	{
		return FString::Printf(TEXT("buffer Glow : RGBA16F(Size = int2(%d, %d), Export = true);\n"
			"pass Fill : clear { Injection = AfterOpaque; write Glow; }\n"), Size, Size);
	};
	if (!Fixture.WriteSource(*this, Source(16)) || !Compile(Fixture, *this, true))
	{
		return false;
	}
	UDreamPassPipeline* Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *PipelinePath);
	UTextureRenderTarget2D* Target = LoadObject<UTextureRenderTarget2D>(nullptr, *TargetPath);
	if (!TestNotNull(TEXT("generated pipeline"), Pipeline) || !TestNotNull(TEXT("generated export"), Target))
	{
		return false;
	}
	bool bFailSave = true;
	bool bSavePipelineBeforeFailure = false;
	TSet<FString> AttemptedPackages;
	FScopedSaveFailure Failure([&](const TArray<UPackage*>& Packages)
	{
		if (!Packages.Contains(Pipeline->GetOutermost()))
		{
			return false;
		}
		for (UPackage* Package : Packages)
		{
			AttemptedPackages.Add(Package->GetName());
		}
		if (bFailSave && bSavePipelineBeforeFailure)
		{
			// Simulate the package saver succeeding for the pipeline, then failing on its dependent export.
			TestTrue(TEXT("the first package of a partial batch reaches disk"), UEditorLoadingAndSavingUtils::SavePackages({ Pipeline->GetOutermost() }, true));
		}
		return bFailSave;
	});
	Fixture.WriteSource(*this, Source(32));
	Compile(Fixture, *this, false);
	TestEqual(TEXT("failed saving retains the updated in-memory export"), Target->SizeX, 32);
	TestTrue(TEXT("the failed pipeline cannot hash-skip"), GetGeneratedAssetSourceHash(Pipeline).IsEmpty());
	AttemptedPackages.Reset();
	bFailSave = false;
	Compile(Fixture, *this, true);
	TestTrue(TEXT("retry includes the unchanged-but-unsaved export"), AttemptedPackages.Contains(Target->GetOutermost()->GetName()));
	if (!Reload(*this, { Pipeline->GetOutermost(), Target->GetOutermost() }))
	{
		return false;
	}
	Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *PipelinePath);
	Target = LoadObject<UTextureRenderTarget2D>(nullptr, *TargetPath);
	if (!TestNotNull(TEXT("reloaded pipeline"), Pipeline) || !TestNotNull(TEXT("reloaded export"), Target))
	{
		return false;
	}
	TestEqual(TEXT("retry persisted the export's new size"), Target->SizeX, 32);

	Fixture.WriteSource(*this, Source(64));
	bFailSave = true;
	bSavePipelineBeforeFailure = true;
	Compile(Fixture, *this, false);
	// Discard only these fixtures' unsaved state, as restarting after a partial save would do.
	Pipeline->GetOutermost()->SetDirtyFlag(false);
	Target->GetOutermost()->SetDirtyFlag(false);
	if (!Reload(*this, { Pipeline->GetOutermost(), Target->GetOutermost() }))
	{
		return false;
	}
	Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *PipelinePath);
	Target = LoadObject<UTextureRenderTarget2D>(nullptr, *TargetPath);
	if (!TestNotNull(TEXT("partially saved pipeline reloads"), Pipeline) || !TestNotNull(TEXT("old export reloads"), Target))
	{
		return false;
	}
	TestFalse(TEXT("the partially saved pipeline carries the new source hash on disk"), GetGeneratedAssetSourceHash(Pipeline).IsEmpty());
	TestEqual(TEXT("the export still carries the old size on disk"), Target->SizeX, 32);
	bFailSave = false;
	AttemptedPackages.Reset();
	Compile(Fixture, *this, true);
	TestTrue(TEXT("the mismatched export prevents a hash skip after restart"), AttemptedPackages.Contains(Target->GetOutermost()->GetName()));
	TestEqual(TEXT("retry repairs the export after a partial save"), Target->SizeX, 64);
	return true;
}
#endif

#endif
