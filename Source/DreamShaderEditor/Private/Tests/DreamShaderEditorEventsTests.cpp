// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The editor-side events the Material Content Browser follows instead of polling: the generator's
// per-source notice, and the bridge's compile route that feeds the diagnostics store -- including who
// owns what that store holds.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Bridge/DreamShaderEditorBridge.h"
#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "DreamShaderCompilerService.h"
#include "DreamShaderModule.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

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

		/** The 2.0 material of the shared-header test: includes the header and calls its broken helper. */
		FString MakeSharedHeaderMaterialSource(const FString& HeaderFileName, const FString& ProductName)
		{
			return FString::Printf(TEXT(R"(#include "%s"

export void %s(inout material m)
{
    m.Opacity = SharedBroken(0.5);
}
)"), *HeaderFileName, *ProductName);
		}

		int32 CountRecordsOwnedBy(const TArray<FDreamShaderDiagnosticRecord>* Records, const FString& OwnerSourceFilePath)
		{
			if (Records == nullptr)
			{
				return 0;
			}
			int32 Count = 0;
			for (const FDreamShaderDiagnosticRecord& Record : *Records)
			{
				Count += Record.OwnerSourceFilePath.Equals(OwnerSourceFilePath, ESearchCase::IgnoreCase) ? 1 : 0;
			}
			return Count;
		}

		FDreamShaderDiagnosticRecord MakeStoreTestRecord(const FString& FilePath, const TCHAR* Message, const TCHAR* Severity = TEXT("error"))
		{
			FDreamShaderDiagnosticRecord Record;
			Record.FilePath = FilePath;
			Record.Message = FText::FromString(Message);
			Record.Severity = Severity;
			return Record;
		}

		TArray<FDreamShaderDiagnosticRecord> MakeStoreTestRecords(std::initializer_list<FDreamShaderDiagnosticRecord> Records)
		{
			return TArray<FDreamShaderDiagnosticRecord>(Records);
		}

		/** The messages of what is filed against FilePath, in order, or "" when nothing is. */
		FString DescribeStoreFile(const FDreamShaderDiagnosticsStore& Store, const FString& FilePath)
		{
			TArray<FString> Messages;
			if (const TArray<FDreamShaderDiagnosticRecord>* Records = Store.FindDiagnostics(FilePath))
			{
				for (const FDreamShaderDiagnosticRecord& Record : *Records)
				{
					Messages.Add(Record.Message.ToString());
				}
			}
			return FString::Join(Messages, TEXT(","));
		}
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
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(SourceFilePath, Message, /*bForce*/ true, /*bEphemeralThinCustom*/ true));
	if (TestEqual(TEXT("One notice for GenerateAssetsFromFile, not one per layer"), Notices.Num(), 1))
	{
		TestEqual(TEXT("The notice names the normalized source path"), Notices[0].Key, SourceFilePath);
		TestTrue(TEXT("The notice reports success"), Notices[0].Value);
	}

	Notices.Reset();
	TestTrue(FString::Printf(TEXT("Material generation succeeds: %s"), *Message),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestMaterial(SourceFilePath, Message, /*bForce*/ true, /*bEphemeralThinCustom*/ true));
	TestEqual(TEXT("One notice for GenerateMaterialFromFile on its own"), Notices.Num(), 1);

	// A failing source: the notice still fires, reporting failure.
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeEventsMaterialSource(AssetName, TEXT("Color = NoSuchIdentifier;")), SourceFilePath))
	{
		return false;
	}
	Notices.Reset();
	AddExpectedError(TEXT("NoSuchIdentifier"), EAutomationExpectedErrorFlags::Contains, -1);
	TestFalse(TEXT("A broken source fails to generate"),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestAssets(SourceFilePath, Message, /*bForce*/ true, /*bEphemeralThinCustom*/ true));
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
	TestFalse(TEXT("The broken source fails through the bridge"), Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, Message));
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
		Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, Message));
	TestEqual(TEXT("The success committed the diagnostics store again"), DiagnosticsCommits, 2);
	Records = Bridge->GetDiagnosticsForSource(SourceFilePath);
	TestTrue(TEXT("The success cleared the file's records"), Records == nullptr || Records->Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSharedHeaderDiagnosticsOwnershipTest,
	"DreamShader.Browser.Events.SharedHeaderDiagnosticsStayWithTheirOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Two materials include one header and each compile files an error against it. Recompiling one
// replaces exactly what that one filed and leaves the other's record for the same header alone. The
// bridge used to clear every header the compiled source includes, whoever had filed against it.
bool FDreamShaderSharedHeaderDiagnosticsOwnershipTest::RunTest(const FString& Parameters)
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
	const FString Prefix = MakeUniqueTestAssetName(TEXT("AutoHeaderOwner"));
	const FString HeaderFileName = Prefix + TEXT("_Shared.dsh");
	// Every compile below fails before anything is generated, so no asset is ever made.
	AddExpectedError(TEXT("MissingSharedGain"), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("MissingLocalGain"), EAutomationExpectedErrorFlags::Contains, -1);

	FString HeaderFilePath;
	if (!WriteAutomationSourceFile(*this, HeaderFileName, TEXT(R"(// Both materials include this; each compile files the error below against it.
float SharedBroken(float x)
{
    return x * MissingSharedGain;
}
)"), HeaderFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(HeaderFilePath);

	FString FirstFilePath;
	FString SecondFilePath;
	if (!WriteAutomationSourceFile(*this, Prefix + TEXT("_A.dss"), MakeSharedHeaderMaterialSource(HeaderFileName, Prefix + TEXT("_A")), FirstFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(FirstFilePath);
	if (!WriteAutomationSourceFile(*this, Prefix + TEXT("_B.dss"), MakeSharedHeaderMaterialSource(HeaderFileName, Prefix + TEXT("_B")), SecondFilePath))
	{
		return false;
	}
	Artifacts.SourceFiles.Add(SecondFilePath);

	FString Message;
	TestFalse(TEXT("A fails on the header's undeclared name"), Bridge->CompileSourceFile(FirstFilePath, /*bForce*/ true, Message));
	TestFalse(TEXT("B fails on the header's undeclared name"), Bridge->CompileSourceFile(SecondFilePath, /*bForce*/ true, Message));

	const TArray<FDreamShaderDiagnosticRecord>* HeaderRecords = Bridge->GetDiagnosticsForSource(HeaderFilePath);
	const int32 FirstInHeader = CountRecordsOwnedBy(HeaderRecords, FirstFilePath);
	const int32 SecondInHeader = CountRecordsOwnedBy(HeaderRecords, SecondFilePath);
	TestTrue(FString::Printf(TEXT("A's compile filed against the header (%d record(s))"), FirstInHeader), FirstInHeader >= 1);
	if (!TestTrue(FString::Printf(TEXT("B's compile filed against the header (%d record(s))"), SecondInHeader), SecondInHeader >= 1))
	{
		return false;
	}
	TestTrue(TEXT("B's header record names the header's mistake"),
		HeaderRecords->ContainsByPredicate([&SecondFilePath](const FDreamShaderDiagnosticRecord& Record)
		{
			return Record.OwnerSourceFilePath.Equals(SecondFilePath, ESearchCase::IgnoreCase)
				&& Record.Message.ToString().Contains(TEXT("MissingSharedGain"));
		}));

	// A again, still including the header: the old clear-the-header compile wiped B's record here.
	TestFalse(TEXT("A fails again"), Bridge->CompileSourceFile(FirstFilePath, /*bForce*/ true, Message));
	HeaderRecords = Bridge->GetDiagnosticsForSource(HeaderFilePath);
	TestEqual(TEXT("Recompiling A left B's records for the header alone"), CountRecordsOwnedBy(HeaderRecords, SecondFilePath), SecondInHeader);
	TestEqual(TEXT("Recompiling A replaced its header records rather than doubling them"), CountRecordsOwnedBy(HeaderRecords, FirstFilePath), FirstInHeader);

	// A stops including the header and fails on a mistake of its own instead.
	if (!WriteAutomationSourceFile(*this, Prefix + TEXT("_A.dss"), FString::Printf(TEXT(R"(export void %s_A(inout material m)
{
    m.Opacity = MissingLocalGain;
}
)"), *Prefix), FirstFilePath))
	{
		return false;
	}
	TestFalse(TEXT("A fails on its own undeclared name"), Bridge->CompileSourceFile(FirstFilePath, /*bForce*/ true, Message));

	HeaderRecords = Bridge->GetDiagnosticsForSource(HeaderFilePath);
	TestEqual(TEXT("A's compile without the header took A's records out of it"), CountRecordsOwnedBy(HeaderRecords, FirstFilePath), 0);
	TestEqual(TEXT("and left B's alone"), CountRecordsOwnedBy(HeaderRecords, SecondFilePath), SecondInHeader);
	TestTrue(TEXT("A's new error is filed against A"), CountRecordsOwnedBy(Bridge->GetDiagnosticsForSource(FirstFilePath), FirstFilePath) >= 1);

	// B again, unchanged: its records are replaced, not added to.
	TestFalse(TEXT("B still fails"), Bridge->CompileSourceFile(SecondFilePath, /*bForce*/ true, Message));
	TestEqual(TEXT("Recompiling B replaced its header records rather than doubling them"),
		CountRecordsOwnedBy(Bridge->GetDiagnosticsForSource(HeaderFilePath), SecondFilePath), SecondInHeader);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDiagnosticsProducersTest,
	"DreamShader.Browser.Events.DiagnosticsProducersReplaceOnlyTheirOwn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The store on its own: each run of each source replaces only what that run filed. A material's
// shader compile finishes ticks after the compile that generated it, and used to take that compile's
// warnings with it; a compile used to take the material's shader errors.
bool FDreamShaderDiagnosticsProducersTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using EProducer = EDreamShaderDiagnosticsProducer;

	const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("DShader/StoreTest"));
	const FString First = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(Directory, TEXT("A.dss")));
	const FString Second = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(Directory, TEXT("B.dss")));
	const FString Header = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(Directory, TEXT("Shared.dsh")));
	const FString FirstMaterial = TEXT("/Game/StoreTest/A.A");
	const FString OtherMaterial = TEXT("/Game/StoreTest/A_Variant.A_Variant");

	FDreamShaderDiagnosticsStore Store;

	// Two compiles file against one header; a record with no file is the compiled source's.
	Store.SetDiagnostics(First, EProducer::Compile, MakeStoreTestRecords({
		MakeStoreTestRecord(FString(), TEXT("a-warn"), TEXT("warning")),
		MakeStoreTestRecord(Header, TEXT("a-header")) }));
	Store.SetDiagnostics(Second, EProducer::Compile, MakeStoreTestRecords({ MakeStoreTestRecord(Header, TEXT("b-header")) }));
	TestEqual(TEXT("both compiles' records are filed against the header, by source"), DescribeStoreFile(Store, Header), FString(TEXT("a-header,b-header")));
	TestEqual(TEXT("a record with no file is filed against its source"), DescribeStoreFile(Store, First), FString(TEXT("a-warn")));

	if (const TArray<FDreamShaderDiagnosticRecord>* Owned = Store.FindOwnedDiagnostics(First, EProducer::Compile))
	{
		TestEqual(TEXT("A's compile owns two records"), Owned->Num(), 2);
		if (Owned->Num() == 2)
		{
			TestEqual(TEXT("an owned record names the file it is filed against"), (*Owned)[1].FilePath, Header);
			TestEqual(TEXT("an owned record names its owner"), (*Owned)[1].OwnerSourceFilePath, First);
		}
	}
	else
	{
		AddError(TEXT("A's compile records are not found by owner"));
	}
	if (const TArray<FDreamShaderDiagnosticRecord>* HeaderRecords = Store.FindDiagnostics(Header))
	{
		TestTrue(TEXT("a record filed against a file leaves FilePath to the key"), HeaderRecords->Num() > 0 && (*HeaderRecords)[0].FilePath.IsEmpty());
	}

	// A's shader compile, clean and then not: the compile's warning stands either way.
	Store.SetDiagnostics(First, EProducer::MaterialCompile, {}, FirstMaterial);
	TestEqual(TEXT("a clean shader compile leaves the compile's warning"), DescribeStoreFile(Store, First), FString(TEXT("a-warn")));
	Store.SetDiagnostics(First, EProducer::MaterialCompile, MakeStoreTestRecords({ MakeStoreTestRecord(FString(), TEXT("a-shader")) }), FirstMaterial);
	Store.SetDiagnostics(First, EProducer::MaterialCompile, MakeStoreTestRecords({ MakeStoreTestRecord(FString(), TEXT("a-variant-shader")) }), OtherMaterial);
	TestEqual(TEXT("shader errors join the compile's records, which come first"), DescribeStoreFile(Store, First), FString(TEXT("a-warn,a-shader,a-variant-shader")));
	Store.SetDiagnostics(First, EProducer::MaterialCompile, {}, OtherMaterial);
	TestEqual(TEXT("one material's shader compile replaces only that material's records"), DescribeStoreFile(Store, First), FString(TEXT("a-warn,a-shader")));

	// A compiles again with nothing to say: its compile records go, wherever they were, and only those.
	Store.SetDiagnostics(First, EProducer::Compile, {});
	TestEqual(TEXT("the material's shader error outlives a clean compile"), DescribeStoreFile(Store, First), FString(TEXT("a-shader")));
	TestEqual(TEXT("B's record for the header outlives A's clean compile"), DescribeStoreFile(Store, Header), FString(TEXT("b-header")));
	TestNull(TEXT("A's compile owns nothing now"), Store.FindOwnedDiagnostics(First, EProducer::Compile));

	// The VirtualFunction scan's records for the header belong to the header itself.
	Store.SetDiagnostics(Header, EProducer::VirtualFunctionSync, MakeStoreTestRecords({ MakeStoreTestRecord(FString(), TEXT("h-scan")) }));
	TestEqual(TEXT("the scan's record joins B's, after it"), DescribeStoreFile(Store, Header), FString(TEXT("b-header,h-scan")));
	Store.SetDiagnostics(Header, EProducer::VirtualFunctionSync, {});
	TestEqual(TEXT("retiring the scan's records leaves B's"), DescribeStoreFile(Store, Header), FString(TEXT("b-header")));

	// Deleting the header drops everything filed against it; deleting A drops everything A owns.
	Store.SetDiagnostics(Second, EProducer::Compile, MakeStoreTestRecords({
		MakeStoreTestRecord(Header, TEXT("b-header")),
		MakeStoreTestRecord(FString(), TEXT("b-own")) }));
	Store.ClearDiagnostics(Header);
	TestNull(TEXT("nothing is filed against a deleted header"), Store.FindDiagnostics(Header));
	TestEqual(TEXT("B keeps what it filed against itself"), DescribeStoreFile(Store, Second), FString(TEXT("b-own")));
	Store.ClearDiagnostics(First);
	TestNull(TEXT("a deleted source owns nothing, shader errors included"), Store.FindDiagnostics(First));
	TestNull(TEXT("nor any material's shader compile"), Store.FindOwnedDiagnostics(First, EProducer::MaterialCompile, FirstMaterial));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
