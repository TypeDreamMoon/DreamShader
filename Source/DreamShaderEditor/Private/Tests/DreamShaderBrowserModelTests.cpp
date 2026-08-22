// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Material Content Browser's model: the per-source status it computes and the asset -> source
// join. These are the facts the browser displays, so they are asserted here without any Slate.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Model/DreamShaderBrowserModel.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
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
		FString MakeBrowserMaterialSource(const FString& AssetName, const TCHAR* Backend, const TCHAR* TintExpression)
		{
			return FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        vec3 Tint = %s;
    }

    Settings = {
        Backend = "%s";
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
)"), *AssetName, TintExpression, Backend);
		}

		struct FScopedBrowserArtifacts
		{
			TArray<FString> SourceFiles;
			TArray<FString> ObjectPaths;
			~FScopedBrowserArtifacts()
			{
				for (const FString& ObjectPath : ObjectPaths) { DeleteAssetForAutomation(ObjectPath); }
				for (const FString& SourceFile : SourceFiles) { DeleteSourceFileForAutomation(SourceFile); }
			}
		};

		const TCHAR* StatusName(EBrowserSourceStatus Status)
		{
			switch (Status)
			{
			case EBrowserSourceStatus::NotCompiled: return TEXT("NotCompiled");
			case EBrowserSourceStatus::UpToDate: return TEXT("UpToDate");
			case EBrowserSourceStatus::Stale: return TEXT("Stale");
			case EBrowserSourceStatus::InMemoryUntracked: return TEXT("InMemoryUntracked");
			case EBrowserSourceStatus::Error: return TEXT("Error");
			case EBrowserSourceStatus::Library: return TEXT("Library");
			default: return TEXT("Unresolved");
			}
		}

		bool ExpectStatus(FAutomationTestBase& Test, const TCHAR* What, const TSharedPtr<FBrowserEntry>& Entry, EBrowserSourceStatus Expected)
		{
			if (!Test.TestTrue(FString::Printf(TEXT("%s: entry has a source half"), What), Entry.IsValid() && Entry->Source.IsSet()))
			{
				return false;
			}
			return Test.TestEqual(
				FString::Printf(TEXT("%s: status (detail: %s)"), What, *Entry->Source->StatusDetail.ToString()),
				FString(StatusName(Entry->Source->Status)),
				FString(StatusName(Expected)));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderBrowserFilterTest,
	"DreamShader.Browser.Filter.MatchesNameRootAndStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderBrowserFilterTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	FBrowserEntry Material;
	Material.Key = TEXT("/src/M_Emissive.dsm");
	FBrowserSourceInfo& MaterialSource = Material.Source.Emplace();
	MaterialSource.DisplayName = TEXT("M_Emissive.dsm");
	MaterialSource.RootDisplayName = TEXT("PluginA");
	MaterialSource.Kind = EBrowserSourceKind::Material;
	MaterialSource.Status = EBrowserSourceStatus::UpToDate;

	FBrowserEntry Broken = Material;
	Broken.Source->DisplayName = TEXT("M_Broken.dsm");
	Broken.Source->RootDisplayName.Reset();
	Broken.Source->Status = EBrowserSourceStatus::Error;

	FBrowserEntry Header = Material;
	Header.Source->DisplayName = TEXT("Common.dsh");
	Header.Source->Kind = EBrowserSourceKind::Header;
	Header.Source->Status = EBrowserSourceStatus::Library;

	FBrowserFilter Filter;
	TestTrue(TEXT("Empty filter matches everything"), Filter.Matches(Material) && Filter.Matches(Broken) && Filter.Matches(Header));

	Filter.bHideLibraries = true;
	TestTrue(TEXT("Hide libraries keeps materials"), Filter.Matches(Material));
	TestFalse(TEXT("Hide libraries drops headers"), Filter.Matches(Header));

	Filter = FBrowserFilter();
	Filter.bErrorsOnly = true;
	TestFalse(TEXT("Errors only drops an up-to-date material"), Filter.Matches(Material));
	TestTrue(TEXT("Errors only keeps a failed material"), Filter.Matches(Broken));
	TestFalse(TEXT("Errors only drops a library"), Filter.Matches(Header));

	Filter = FBrowserFilter();
	Filter.SearchText = TEXT("emiss");
	TestTrue(TEXT("Search matches the display name case-insensitively"), Filter.Matches(Material));
	TestFalse(TEXT("Search rejects a non-matching name"), Filter.Matches(Broken));

	Filter.SearchText = TEXT("plugina");
	TestTrue(TEXT("Search matches the root name, so a plugin's name filters to everything it ships"), Filter.Matches(Material));
	TestFalse(TEXT("A project-root file does not match a plugin's name"), Filter.Matches(Broken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderBrowserModelStatusTest,
	"DreamShader.Browser.Model.StatusFollowsTheAssetLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The states a .dsm moves through as the browser sees them: never compiled -> compiled in memory
// (which is NOT stale: a memory-only build stamps no hash) -> persisted and current -> stale once
// the source moves. Also the join from the generated asset back to its scanned source entry.
bool FDreamShaderBrowserModelStatusTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedBrowserArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoBrowserStatus"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.ObjectPaths.Add(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeBrowserMaterialSource(AssetName, TEXT("Graph"), TEXT("vec3(1.0, 0.2, 0.2)")), SourceFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(SourceFilePath);

	FDreamShaderBrowserModel Model;
	int32 ChangeCount = 0;
	Model.OnChanged.AddLambda([&ChangeCount]() { ++ChangeCount; });

	Model.RefreshAll();
	TestEqual(TEXT("RefreshAll broadcasts once"), ChangeCount, 1);

	TSharedPtr<FBrowserEntry> Entry = Model.FindBySourcePath(SourceFilePath);
	if (!TestTrue(TEXT("The scan lists the new source"), Entry.IsValid()))
	{
		return false;
	}
	ExpectStatus(*this, TEXT("Before any compile"), Entry, EBrowserSourceStatus::NotCompiled);
	TestEqual(TEXT("The resolved object path is where the compile will land"), Entry->Source->ResolvedObjectPath, ObjectPath);
	TestFalse(TEXT("No asset half before a compile"), Entry->Asset.IsSet());
	TestEqual(TEXT("A .dsm is a material, not a library"), static_cast<int32>(Entry->Source->Kind), static_cast<int32>(EBrowserSourceKind::Material));

	// 1. Memory-only compile: the path is stamped, the hash deliberately is not.
	FString Message;
	if (!TestTrue(FString::Printf(TEXT("In-memory generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true)))
	{
		return false;
	}
	Model.RefreshEntry(Entry);
	ExpectStatus(*this, TEXT("After an in-memory compile"), Entry, EBrowserSourceStatus::InMemoryUntracked);
	if (TestTrue(TEXT("The asset half is attached after a compile"), Entry->Asset.IsSet()))
	{
		TestEqual(TEXT("A memory-only build reads as in-memory storage"), static_cast<int32>(Entry->Asset->Storage), static_cast<int32>(EBrowserStorage::InMemory));
		TestEqual(TEXT("A fresh build classifies as Generated"), static_cast<int32>(Entry->Asset->Provenance), static_cast<int32>(EDreamShaderDigestState::Generated));
	}

	// The asset -> source join.
	UMaterialInterface* Material = Entry->ResolveMaterial();
	if (TestNotNull(TEXT("The entry resolves its material"), Material))
	{
		TSharedPtr<FBrowserEntry> FromAsset = Model.MakeEntryForAsset(Material);
		if (TestTrue(TEXT("An asset-centric entry is produced"), FromAsset.IsValid()))
		{
			TestTrue(TEXT("The asset joins back to its scanned source"), FromAsset->Source.IsSet());
			TestEqual(TEXT("The joined entry shares the source's key"), FromAsset->Key, Entry->Key);
			TestTrue(TEXT("The model hands out a snapshot, not the scanned entry itself"), FromAsset != Entry);
		}
	}

	// 2. Persisted compile: the hash is stamped, so currency can be judged.
	if (!TestTrue(FString::Printf(TEXT("Persisted generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ false)))
	{
		return false;
	}
	Model.RefreshEntry(Entry);
	ExpectStatus(*this, TEXT("After a persisted compile"), Entry, EBrowserSourceStatus::UpToDate);
	if (TestTrue(TEXT("The asset half survives the persist"), Entry->Asset.IsSet()))
	{
		TestEqual(TEXT("A persisted build reads as on-disk storage"), static_cast<int32>(Entry->Asset->Storage), static_cast<int32>(EBrowserStorage::OnDisk));
	}

	// 3. Move the source: the stamped hash no longer matches.
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeBrowserMaterialSource(AssetName, TEXT("Graph"), TEXT("vec3(0.2, 1.0, 0.2)")), SourceFilePath))
	{
		return false;
	}
	Model.RefreshEntry(Entry);
	ExpectStatus(*this, TEXT("After the source changed"), Entry, EBrowserSourceStatus::Stale);

	// 4. A compile failure pinned by the browser shows as an error with its message.
	Model.MarkCompileFailed(Entry, TEXT("synthetic failure"));
	ExpectStatus(*this, TEXT("After MarkCompileFailed"), Entry, EBrowserSourceStatus::Error);
	TestEqual(TEXT("The failure message is the status detail"), Entry->Source->StatusDetail.ToString(), FString(TEXT("synthetic failure")));

	TestTrue(TEXT("Every model mutation broadcast OnChanged"), ChangeCount >= 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderBrowserModelLibraryTest,
	"DreamShader.Browser.Model.LibrariesListTheirDependents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A header is a library with no status of its own; what the browser needs from it is who imports it.
bool FDreamShaderBrowserModelLibraryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedBrowserArtifacts Artifacts;
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString HeaderName = FString::Printf(TEXT("BrowserLib_%s.dsh"), *Suffix);
	const FString MaterialName = FString::Printf(TEXT("M_AutoBrowserImporter_%s"), *Suffix);

	FString HeaderPath;
	if (!WriteAutomationSourceFile(*this, HeaderName, TEXT("// browser model test header\n"), HeaderPath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(HeaderPath);

	const FString MaterialSource = FString::Printf(TEXT("import \"%s\";\n%s"), *HeaderName, *MakeBrowserMaterialSource(MaterialName, TEXT("Graph"), TEXT("vec3(1.0, 1.0, 1.0)")));
	FString MaterialPath;
	if (!WriteAutomationSourceFile(*this, MaterialName + TEXT(".dsm"), MaterialSource, MaterialPath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(MaterialPath);

	FDreamShaderBrowserModel Model;
	Model.RefreshAll();

	TSharedPtr<FBrowserEntry> Header = Model.FindBySourcePath(HeaderPath);
	if (!TestTrue(TEXT("The scan lists the header"), Header.IsValid()))
	{
		return false;
	}
	ExpectStatus(*this, TEXT("A header"), Header, EBrowserSourceStatus::Library);
	TestTrue(TEXT("A header is a library"), Header->IsLibrary());
	TestEqual(TEXT("A header has no object path"), Header->GetObjectPath(), FString());
	TestNull(TEXT("A header resolves no material"), Header->ResolveMaterial());
	TestTrue(
		FString::Printf(TEXT("The importing material is listed as a dependent (%d listed)"), Header->Source->Dependents.Num()),
		Header->Source->Dependents.Contains(MaterialPath));

	TSharedPtr<FBrowserEntry> Material = Model.FindBySourcePath(MaterialPath);
	if (TestTrue(TEXT("The scan lists the importer"), Material.IsValid()))
	{
		TestFalse(TEXT("A .dsm is not a library"), Material->IsLibrary());
		TestEqual(TEXT("A material lists no dependents"), Material->Source->Dependents.Num(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
