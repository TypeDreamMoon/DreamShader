// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Tweaked: a generated ThinCustom instance carrying parameter overrides.
//
// The instance exists to be tuned, so tuning it must not be judged a hand edit -- which is what it
// used to be, and which permanently refused the .dsm's rebuild. These tests pin the three halves of
// the answer: the rebuild goes through, the values come back, and a real edit to the hidden base's
// graph is still divergence.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderVersionCompat.h"
#include "MaterialAssetGeneration/DreamShaderGeneratedAssetDigest.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
// FMaterialParameterInfo / FMaterialParameterMetadata: Materials/MaterialParameters.h from UE 5.7,
// MaterialTypes.h before it. MaterialTypes.h survives on 5.7 as a deprecation stub, so neither
// spelling covers both.
#if DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER
#include "Materials/MaterialParameters.h"
#else
#include "MaterialTypes.h"
#endif
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	// Fixture helpers shared with DreamShaderAutomationTests.cpp (defined there, external linkage).
	FString MakeUniqueTestAssetName(const TCHAR* Prefix);
	FString MakeAutomationObjectPath(const FString& AssetName);
	bool WriteAutomationSourceFile(FAutomationTestBase& Test, const FString& FileName, const FString& SourceText, FString& OutSourceFilePath);
	void AddExpectedNewAssetProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath);
	void AddExpectedAutomationCleanupWarnings(FAutomationTestBase& Test);
	void DeleteSourceFileForAutomation(const FString& SourceFilePath);
	void DeleteAssetForAutomation(const FString& ObjectPath);

	namespace
	{
		struct FScopedTweakedArtifacts
		{
			TArray<FString> SourceFiles;
			TArray<FString> ObjectPaths;
			~FScopedTweakedArtifacts()
			{
				for (const FString& ObjectPath : ObjectPaths) { DeleteAssetForAutomation(ObjectPath); }
				for (const FString& SourceFile : SourceFiles) { DeleteSourceFileForAutomation(SourceFile); }
			}
		};

		// One scalar parameter and one static switch parameter, both reaching the emissive output so
		// they are real parameters on the generated base rather than dead declarations. Backend is
		// spelled out: these tests want the instance, not a UMaterial, whatever the project default is.
		FString MakeTweakedMaterialSourceWithBoost(const FString& AssetName, const TCHAR* BoostDefault, const TCHAR* TrueBranch)
		{
			const FString Properties = FString::Printf(TEXT("        ScalarParameter Boost = %s;\n"), BoostDefault);
			return FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
%s    }

    Settings = {
        Backend = "ThinCustom";
        Domain = "Surface";
        ShadingModel = "Unlit";
        BlendMode = "Opaque";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec3 Tint = UE.StaticSwitchParameter(Name = "Flag", True = %s, False = vec3(0.0, 0.0, 1.0), Default = false);
        Color = Tint * Boost;
    }
}
)"), *AssetName, *Properties, TrueBranch);
		}

		// The same material with `Boost` renamed away: a source that stopped declaring the parameter
		// somebody had tuned. Another parameter takes its place so the shape of the file is otherwise
		// unchanged and the switch override still has something to survive alongside.
		FString MakeTweakedMaterialSourceWithoutBoost(const FString& AssetName, const TCHAR* TrueBranch)
		{
			return FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        ScalarParameter Gain = 0.5;
    }

    Settings = {
        Backend = "ThinCustom";
        Domain = "Surface";
        ShadingModel = "Unlit";
        BlendMode = "Opaque";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec3 Tint = UE.StaticSwitchParameter(Name = "Flag", True = %s, False = vec3(0.0, 0.0, 1.0), Default = false);
        Color = Tint * Gain;
    }
}
)"), *AssetName, TrueBranch);
		}

		// The two tweaks under test: a numeric override and a static one. The static switch matters
		// most -- it changes what the material COMPILES to, so losing it silently changes the picture.
		void ApplyTestOverrides(UDreamShaderMaterialInstance* Instance, float BoostValue, bool bFlagValue)
		{
			UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Boost"), BoostValue);
			Instance->SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Flag")), bFlagValue);
			Instance->UpdateStaticPermutation();
			Instance->PostEditChange();
		}

		bool TryGetScalarOverride(UMaterialInstance* Instance, const TCHAR* Name, float& OutValue)
		{
			FMaterialParameterMetadata Meta;
			if (!Instance->GetParameterOverrideValue(EMaterialParameterType::Scalar, FMaterialParameterInfo(Name), Meta))
			{
				return false;
			}
			OutValue = Meta.Value.AsScalar();
			return true;
		}

		bool TryGetStaticSwitchOverride(UMaterialInstance* Instance, const TCHAR* Name, bool& OutValue)
		{
			FMaterialParameterMetadata Meta;
			if (!Instance->GetParameterOverrideValue(EMaterialParameterType::StaticSwitch, FMaterialParameterInfo(Name), Meta))
			{
				return false;
			}
			OutValue = Meta.Value.AsStaticSwitch();
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTweakedOverridesSurviveRebuildTest,
	"DreamShader.Compiler.Tweaked.OverridesSurviveARebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The whole point of the Tweaked state: a moved source rebuilds a tuned instance instead of refusing,
// and the tuning is still there afterwards -- including the static switch, whose loss would silently
// change which permutation the material renders with.
bool FDreamShaderTweakedOverridesSurviveRebuildTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedTweakedArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoTweakSurvive"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.ObjectPaths.Add(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(
			*this,
			AssetName + TEXT(".dsm"),
			MakeTweakedMaterialSourceWithBoost(AssetName, TEXT("0.5"), TEXT("vec3(1.0, 0.0, 0.0)")),
			SourceFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(SourceFilePath);

	FString Message;
	if (!TestTrue(
			FString::Printf(TEXT("ThinCustom generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true)))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated ThinCustom instance loads"), Instance))
	{
		return false;
	}

	TestEqual(
		TEXT("A freshly generated instance classifies as Generated"),
		static_cast<int32>(ClassifyGeneratedAsset(Instance)),
		static_cast<int32>(EDreamShaderDigestState::Generated));

	ApplyTestOverrides(Instance, 0.9f, /*bFlagValue*/ true);
	TestEqual(
		TEXT("Overriding parameters on the instance classifies as Tweaked, not Diverged"),
		static_cast<int32>(ClassifyGeneratedAsset(Instance)),
		static_cast<int32>(EDreamShaderDigestState::Tweaked));

	// Move the source. Without this the compile is skipped by the source hash and never reaches the
	// gate, and the test would prove nothing at all.
	if (!WriteAutomationSourceFile(
			*this,
			AssetName + TEXT(".dsm"),
			MakeTweakedMaterialSourceWithBoost(AssetName, TEXT("0.25"), TEXT("vec3(0.0, 1.0, 0.0)")),
			SourceFilePath))
	{
		return false;
	}

	const bool bRebuilt = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ false);
	if (!TestTrue(FString::Printf(TEXT("A changed source rebuilds a tweaked instance: %s"), *Message), bRebuilt))
	{
		return false;
	}
	TestFalse(
		FString::Printf(TEXT("The rebuild is not refused as a hand edit: %s"), *Message),
		Message.Contains(TEXT("edited by hand")));

	Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("The rebuilt instance loads"), Instance))
	{
		return false;
	}

	float BoostValue = 0.0f;
	if (TestTrue(TEXT("The scalar override is still set after the rebuild"), TryGetScalarOverride(Instance, TEXT("Boost"), BoostValue)))
	{
		// 0.9, not the source's new 0.25 default: restored, rather than merely re-defaulted.
		TestEqual(TEXT("The scalar override kept its value"), BoostValue, 0.9f);
	}

	bool bFlagValue = false;
	if (TestTrue(TEXT("The static switch override is still set after the rebuild"), TryGetStaticSwitchOverride(Instance, TEXT("Flag"), bFlagValue)))
	{
		TestTrue(TEXT("The static switch override kept its value"), bFlagValue);
	}

	TestEqual(
		TEXT("The rebuilt instance is Tweaked again, not Diverged"),
		static_cast<int32>(ClassifyGeneratedAsset(Instance)),
		static_cast<int32>(EDreamShaderDigestState::Tweaked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTweakedRemovedParameterIsDroppedTest,
	"DreamShader.Compiler.Tweaked.RemovedParameterOverrideIsDropped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The other half of restore: an override whose parameter the source no longer declares cannot come
// back, and must not turn the rebuild into a failure. It is reported (DSH8155) and dropped.
bool FDreamShaderTweakedRemovedParameterIsDroppedTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedTweakedArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoTweakDropped"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.ObjectPaths.Add(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(
			*this,
			AssetName + TEXT(".dsm"),
			MakeTweakedMaterialSourceWithBoost(AssetName, TEXT("0.5"), TEXT("vec3(1.0, 0.0, 0.0)")),
			SourceFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(SourceFilePath);

	FString Message;
	if (!TestTrue(
			FString::Printf(TEXT("ThinCustom generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true)))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated ThinCustom instance loads"), Instance))
	{
		return false;
	}

	ApplyTestOverrides(Instance, 0.9f, /*bFlagValue*/ true);

	// The source drops Boost entirely and keeps the switch.
	if (!WriteAutomationSourceFile(
			*this,
			AssetName + TEXT(".dsm"),
			MakeTweakedMaterialSourceWithoutBoost(AssetName, TEXT("vec3(1.0, 0.0, 0.0)")),
			SourceFilePath))
	{
		return false;
	}

	const bool bRebuilt = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ false);
	if (!TestTrue(FString::Printf(TEXT("Dropping an overridden parameter is not an error: %s"), *Message), bRebuilt))
	{
		return false;
	}

	Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("The rebuilt instance loads"), Instance))
	{
		return false;
	}

	float BoostValue = 0.0f;
	TestFalse(
		TEXT("An override whose parameter the source removed is dropped"),
		TryGetScalarOverride(Instance, TEXT("Boost"), BoostValue));

	bool bFlagValue = false;
	if (TestTrue(TEXT("The parameter that survived keeps its override"), TryGetStaticSwitchOverride(Instance, TEXT("Flag"), bFlagValue)))
	{
		TestTrue(TEXT("The surviving override kept its value"), bFlagValue);
	}

	AddInfo(FString::Printf(TEXT("Rebuild message: %s"), *Message));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTweakedBaseGraphEditDivergesTest,
	"DreamShader.Compiler.Tweaked.BaseGraphEditStillDiverges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The guard on the exclusion. Taking the instance's overrides out of the digest must not take the
// hidden base's graph out with them: an edit made there is still work a rebuild would destroy, and
// it still has to register against the instance, which is where the ownership metadata lives.
bool FDreamShaderTweakedBaseGraphEditDivergesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedTweakedArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoTweakBaseEdit"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.ObjectPaths.Add(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(
			*this,
			AssetName + TEXT(".dsm"),
			MakeTweakedMaterialSourceWithBoost(AssetName, TEXT("0.5"), TEXT("vec3(1.0, 0.0, 0.0)")),
			SourceFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(SourceFilePath);

	FString Message;
	if (!TestTrue(
			FString::Printf(TEXT("ThinCustom generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true)))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated ThinCustom instance loads"), Instance))
	{
		return false;
	}

	UMaterial* BaseMaterial = Cast<UMaterial>(Instance->Parent);
	if (!TestNotNull(TEXT("The instance has a hidden base material"), BaseMaterial))
	{
		return false;
	}

	// Retuning a generated parameter's DEFAULT is an edit to the graph, not an override on the
	// instance -- the one is content, the other is tuning, and only the first is divergence.
	UMaterialExpressionScalarParameter* BoostParameter = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expression : BaseMaterial->GetExpressions())
	{
		if (UMaterialExpressionScalarParameter* Candidate = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			if (Candidate->ParameterName == TEXT("Boost"))
			{
				BoostParameter = Candidate;
				break;
			}
		}
	}
	if (!TestNotNull(TEXT("The hidden base carries the Boost parameter node"), BoostParameter))
	{
		return false;
	}

	BoostParameter->DefaultValue = 0.125f;
	TestEqual(
		TEXT("Editing the hidden base's graph by hand still reads as divergence"),
		static_cast<int32>(ClassifyGeneratedAsset(Instance)),
		static_cast<int32>(EDreamShaderDigestState::Diverged));

	// And that divergence still refuses the rebuild, which is the behaviour the exclusion had to
	// leave standing.
	UE::DreamShader::FDreamShaderError DivergenceError;
	TestFalse(
		TEXT("A hand-edited base still fails the divergence gate"),
		CheckGeneratedAssetNotDiverged(Instance, DivergenceError));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
