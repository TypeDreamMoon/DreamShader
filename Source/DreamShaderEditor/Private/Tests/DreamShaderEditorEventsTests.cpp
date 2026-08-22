// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The editor-side events the Material Content Browser follows instead of polling: the generator's
// per-source notice, and the bridge's compile route that feeds the diagnostics store.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Bridge/DreamShaderEditorBridge.h"
#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"

#include "Misc/AutomationTest.h"

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
		FString MakeEventsMaterialSource(const FString& AssetName, const TCHAR* GraphBody)
		{
			return FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Backend = "Graph";
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        %s
    }
}
)"), *AssetName, GraphBody);
		}

		struct FScopedEventsArtifacts
		{
			TArray<FString> SourceFiles;
			TArray<FString> ObjectPaths;
			~FScopedEventsArtifacts()
			{
				for (const FString& ObjectPath : ObjectPaths) { DeleteAssetForAutomation(ObjectPath); }
				for (const FString& SourceFile : SourceFiles) { DeleteSourceFileForAutomation(SourceFile); }
			}
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSourceGeneratedEventTest,
	"DreamShader.Browser.Events.OneNoticePerOutermostGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// GenerateAssetsFromFile generates the material through GenerateMaterialFromFile; a listener must
// hear one notice per source, not one per layer, and must hear failures too.
bool FDreamShaderSourceGeneratedEventTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedEventsArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoGenEvent"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.ObjectPaths.Add(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeEventsMaterialSource(AssetName, TEXT("Color = Tint;")), SourceFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(SourceFilePath);

	TArray<TPair<FString, bool>> Notices;
	const FDelegateHandle Handle = OnDreamShaderSourceGenerated().AddLambda(
		[&Notices](const FString& Path, bool bOk) { Notices.Emplace(Path, bOk); });
	ON_SCOPE_EXIT { OnDreamShaderSourceGenerated().Remove(Handle); };

	FString Message;
	TestTrue(FString::Printf(TEXT("Assets generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateAssetsFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true));
	if (TestEqual(TEXT("One notice for GenerateAssetsFromFile, not one per layer"), Notices.Num(), 1))
	{
		TestEqual(TEXT("The notice names the normalized source path"), Notices[0].Key, SourceFilePath);
		TestTrue(TEXT("The notice reports success"), Notices[0].Value);
	}

	Notices.Reset();
	TestTrue(FString::Printf(TEXT("Material generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true));
	TestEqual(TEXT("One notice for GenerateMaterialFromFile on its own"), Notices.Num(), 1);

	// A failing source: the notice still fires, reporting failure.
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeEventsMaterialSource(AssetName, TEXT("Color = NoSuchIdentifier;")), SourceFilePath))
	{
		return false;
	}
	Notices.Reset();
	AddExpectedError(TEXT("NoSuchIdentifier"), EAutomationExpectedErrorFlags::Contains, -1);
	TestFalse(TEXT("A broken source fails to generate"),
		FMaterialGenerator::GenerateAssetsFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true));
	if (TestEqual(TEXT("One notice for the failed generation"), Notices.Num(), 1))
	{
		TestFalse(TEXT("The notice reports failure"), Notices[0].Value);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderBridgeCompileFeedsDiagnosticsTest,
	"DreamShader.Browser.Events.BridgeCompileFeedsDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A compile through the bridge is what puts a failure into the diagnostics store and takes it back
// out on the next success -- the difference between the browser's buttons and a bare generator call.
bool FDreamShaderBridgeCompileFeedsDiagnosticsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge();
	if (!Bridge)
	{
		AddInfo(TEXT("No editor bridge in this process (-NoDreamShaderEditorBridge?); nothing to test."));
		return true;
	}

	FScopedEventsArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoBridgeDiag"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.ObjectPaths.Add(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeEventsMaterialSource(AssetName, TEXT("Color = NoSuchIdentifier;")), SourceFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(SourceFilePath);

	int32 DiagnosticsCommits = 0;
	const FDelegateHandle Handle = Bridge->OnDiagnosticsChanged().AddLambda([&DiagnosticsCommits]() { ++DiagnosticsCommits; });
	ON_SCOPE_EXIT { Bridge->OnDiagnosticsChanged().Remove(Handle); };

	AddExpectedError(TEXT("NoSuchIdentifier"), EAutomationExpectedErrorFlags::Contains, -1);
	FString Message;
	TestFalse(TEXT("The broken source fails through the bridge"), Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, /*bInMemory*/ true, Message));
	TestFalse(TEXT("The failure message comes back"), Message.IsEmpty());
	TestEqual(TEXT("The failure committed the diagnostics store once"), DiagnosticsCommits, 1);

	const TArray<FDreamShaderDiagnosticRecord>* Records = Bridge->GetDiagnosticsForSource(SourceFilePath);
	if (TestNotNull(TEXT("The store holds records for the failed source"), Records))
	{
		TestTrue(TEXT("At least one record"), Records->Num() >= 1);
	}

	// Fix it: the success must clear the file's records, which a bare generator call never did.
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeEventsMaterialSource(AssetName, TEXT("Color = Tint;")), SourceFilePath))
	{
		return false;
	}
	TestTrue(FString::Printf(TEXT("The fixed source compiles through the bridge: %s"), *Message),
		Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, /*bInMemory*/ true, Message));
	TestEqual(TEXT("The success committed the diagnostics store again"), DiagnosticsCommits, 2);
	Records = Bridge->GetDiagnosticsForSource(SourceFilePath);
	TestTrue(TEXT("The success cleared the file's records"), Records == nullptr || Records->Num() == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
