// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Tests for the `dump-graph` commandlet verb -- the canonical JSON fingerprint of a generated graph.
//
// The dump exists to be a parity oracle for the 2.0 compiler, so the property that matters is not
// "is the JSON right" but "is the JSON the SAME". Determinism is therefore the first test and the
// only one that could not be replaced by reading the code: each of the leaks it catches -- a fresh
// FGuid on a named-reroute declaration, the random colour that reroute seeds from its own path name,
// the numeric suffix the engine appends when it recreates an expression -- is invisible in a single
// capture and fatal in a comparison. Generating the same source twice and diffing the bytes is the
// only thing that sees them.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Commandlet/DreamShaderGraphDump.h"
#include "DreamShaderModule.h"
#include "DreamShaderTestCommon.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"

#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"

// A private namespace of its own rather than the shared ...::Tests one: the module builds as a unity
// blob, so a second definition of a helper that DreamShaderAutomationTests.cpp already exports under
// that namespace would be a redefinition rather than a local convenience.
namespace UE::DreamShader::Editor::Private::DumpGraphTests
{
	/**
	 * The tiny material every case here uses. Three nodes, and each one is load-bearing:
	 * the Constant3Vector the literal becomes, plus the named-reroute declaration/usage pair the
	 * generator always inserts between a value and the material property it drives -- which is what
	 * puts a per-generation FGuid and a path-name-seeded colour into the graph, i.e. the two things
	 * the determinism case is looking for.
	 */
	inline FString MakeDumpGraphSource(const FString& AssetName)
	{
		return FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/DumpGraph/%s")
{
    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = vec3(1.0, 0.0, 0.0);
    }
}
)"), *AssetName);
	}

	inline FString MakeDumpGraphAssetName()
	{
		return FString::Printf(TEXT("M_DumpGraph_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	inline FString MakeDumpGraphObjectPath(const FString& AssetName)
	{
		return FString::Printf(TEXT("/Game/DreamShaderTests/DumpGraph/%s.%s"), *AssetName, *AssetName);
	}

	/**
	 * The source file, the asset it produces and the scratch output tree, all removed on the way out.
	 * The source has to live under the project's DShader root so the dump's `source` object resolves
	 * a real root name instead of falling back to "External".
	 */
	class FDumpGraphFixture
	{
	public:
		explicit FDumpGraphFixture(const FString& InAssetName)
			: AssetName(InAssetName)
			, ObjectPath(MakeDumpGraphObjectPath(InAssetName))
			, SourceFilePath(UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(
				UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("Tests"),
				TEXT("DumpGraph"),
				InAssetName + TEXT(".dsm"))))
			, OutputDirectory(FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("DreamShader"),
				TEXT("GraphDumpTests"),
				InAssetName)))
		{
		}

		~FDumpGraphFixture()
		{
			if (UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath))
			{
				TArray<UObject*> ObjectsToDelete;
				ObjectsToDelete.Add(Asset);
				ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
			}

			IFileManager::Get().Delete(*SourceFilePath, false, true);
			IFileManager::Get().DeleteDirectory(*OutputDirectory, false, true);
		}

		bool WriteSource(FAutomationTestBase& Test) const
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(SourceFilePath), true);
			if (!FFileHelper::SaveStringToFile(
					MakeDumpGraphSource(AssetName),
					*SourceFilePath,
					FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddError(TEXT("Failed to write the dump-graph test source file."));
				return false;
			}
			return true;
		}

		/** Dump into `<OutputDirectory>/<Pass>` and hand back the single JSON the source produced. */
		bool DumpOnce(FAutomationTestBase& Test, const int32 Pass, FString& OutJson, int32& OutNodeCount) const
		{
			TArray<FDreamShaderGraphDumpEntry> Entries;
			UE::DreamShader::FDreamShaderError DumpError;

			// The same guard the commandlet installs, exercised here for the same reason it exists:
			// a test that quietly wrote /Game assets would be a test that changed the working tree.
			FScopedDreamShaderGraphDumpWriteGuard WriteGuard;
			const FString PassDirectory = FPaths::Combine(OutputDirectory, FString::Printf(TEXT("Pass%d"), Pass));
			if (!DumpDreamShaderGraphsForSource(SourceFilePath, PassDirectory, Entries, DumpError))
			{
				// The message goes through AddInfo, never AddError: it very likely quotes the asset's
				// object path, which AddExpectedDumpGraphProbeWarnings has registered as an expected
				// error -- so an AddError carrying it would be swallowed and the test would pass.
				Test.AddInfo(FString::Printf(TEXT("dump-graph reported: %s"), *DumpError.Message));
				Test.AddError(FString::Printf(TEXT("dump-graph pass %d failed; see the info line above."), Pass));
				return false;
			}

			if (Entries.Num() != 1)
			{
				Test.AddError(FString::Printf(TEXT("dump-graph pass %d produced %d dump(s); expected exactly 1."), Pass, Entries.Num()));
				return false;
			}

			if (!FFileHelper::LoadFileToString(OutJson, *Entries[0].OutputFilePath))
			{
				Test.AddError(FString::Printf(TEXT("dump-graph pass %d wrote a file that could not be read back."), Pass));
				return false;
			}

			OutNodeCount = Entries[0].NodeCount;
			return true;
		}

		const FString& GetObjectPath() const
		{
			return ObjectPath;
		}

	private:
		FString AssetName;
		FString ObjectPath;
		FString SourceFilePath;
		FString OutputDirectory;
	};

	/**
	 * Negative occurrence counts: suppress the new-asset probe messages if they fire without
	 * requiring them, exactly as the generate tests do. Never name the object path in an assertion
	 * message afterwards -- it is registered as an expected error here, and the harness would
	 * swallow a real failure that happened to quote it.
	 */
	inline void AddExpectedDumpGraphProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath)
	{
		Test.AddExpectedError(
			FString::Printf(TEXT("SkipPackage: %s"), *FPackageName::ObjectPathToPackageName(ObjectPath)),
			EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(ObjectPath, EAutomationExpectedErrorFlags::Contains, -1);
	}
}

// -------------------------------------------------------------------------------------------------
// Determinism
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDumpGraphDeterminismTest,
	"DreamShader.DumpGraph.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDumpGraphDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::DumpGraphTests;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// The dump has to answer for both backends, but only the Graph backend puts the nodes this
	// source describes directly on a UMaterial; under the project default the same source becomes a
	// thin instance over a hidden base and the shapes below would be about a different graph.
	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = MakeDumpGraphAssetName();
	FDumpGraphFixture Fixture(AssetName);
	AddExpectedDumpGraphProbeWarnings(*this, Fixture.GetObjectPath());

	if (!Fixture.WriteSource(*this))
	{
		return false;
	}

	FString FirstJson;
	int32 FirstNodeCount = 0;
	if (!Fixture.DumpOnce(*this, 1, FirstJson, FirstNodeCount))
	{
		return false;
	}

	// A second FULL generation, not a re-read: the graph is torn down and rebuilt, every expression
	// object is new, and the named reroute gets a fresh VariableGuid and a fresh colour. Anything
	// that leaked one of those into the dump shows up here and nowhere else.
	FString SecondJson;
	int32 SecondNodeCount = 0;
	if (!Fixture.DumpOnce(*this, 2, SecondJson, SecondNodeCount))
	{
		return false;
	}

	TestEqual(TEXT("Both passes dump the same number of nodes"), SecondNodeCount, FirstNodeCount);

	const bool bIdentical = FirstJson.Equals(SecondJson, ESearchCase::CaseSensitive);
	TestTrue(TEXT("Two generations of one source produce byte-identical graph dumps"), bIdentical);
	if (!bIdentical)
	{
		// The first differing line, so a failure names the leaked property instead of dumping two
		// whole files into the log.
		TArray<FString> FirstLines;
		TArray<FString> SecondLines;
		FirstJson.ParseIntoArrayLines(FirstLines, false);
		SecondJson.ParseIntoArrayLines(SecondLines, false);
		for (int32 LineIndex = 0; LineIndex < FMath::Max(FirstLines.Num(), SecondLines.Num()); ++LineIndex)
		{
			const FString& Left = FirstLines.IsValidIndex(LineIndex) ? FirstLines[LineIndex] : FString();
			const FString& Right = SecondLines.IsValidIndex(LineIndex) ? SecondLines[LineIndex] : FString();
			if (!Left.Equals(Right, ESearchCase::CaseSensitive))
			{
				AddInfo(FString::Printf(TEXT("First difference at line %d: '%s' vs '%s'"), LineIndex + 1, *Left, *Right));
				break;
			}
		}
	}

	// The file contract, checked on the bytes that were actually written rather than on the builder.
	TestTrue(TEXT("The dump ends with a single trailing newline"), FirstJson.EndsWith(TEXT("\n"), ESearchCase::CaseSensitive));
	TestFalse(TEXT("The dump has no CR anywhere -- LF endings only"), FirstJson.Contains(TEXT("\r"), ESearchCase::CaseSensitive));
	return true;
}

// -------------------------------------------------------------------------------------------------
// The exclusion list
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDumpGraphExcludesEditorStateTest,
	"DreamShader.DumpGraph.ExcludesEditorState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDumpGraphExcludesEditorStateTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::DumpGraphTests;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = MakeDumpGraphAssetName();
	FDumpGraphFixture Fixture(AssetName);
	AddExpectedDumpGraphProbeWarnings(*this, Fixture.GetObjectPath());

	if (!Fixture.WriteSource(*this))
	{
		return false;
	}

	FString Json;
	int32 NodeCount = 0;
	if (!Fixture.DumpOnce(*this, 1, Json, NodeCount))
	{
		return false;
	}

	// Matched as KEYS (`"Name": `) rather than as bare words, so a property value that merely
	// mentions one of these cannot make the test pass or fail by accident.
	const TCHAR* ExcludedKeys[] =
	{
		TEXT("MaterialExpressionGuid"),
		TEXT("MaterialExpressionEditorX"),
		TEXT("MaterialExpressionEditorY"),
		TEXT("NodeColor"),
		TEXT("GraphNode"),
		TEXT("Desc"),
		TEXT("bCollapsed"),
		TEXT("bCommentBubbleVisible"),
		TEXT("VariableGuid"),
		TEXT("DeclarationGuid"),
		TEXT("SubgraphExpression")
	};

	for (const TCHAR* ExcludedKey : ExcludedKeys)
	{
		const FString Needle = FString::Printf(TEXT("\"%s\": "), ExcludedKey);
		TestFalse(
			FString::Printf(TEXT("The dump carries no '%s' key"), ExcludedKey),
			Json.Contains(Needle, ESearchCase::CaseSensitive));
	}

	// The positive half: excluding presentation must not have excluded behaviour with it.
	TestTrue(TEXT("The dump names its schema"), Json.Contains(TEXT("\"schema\": 1"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("The dump records the material property bindings"), Json.Contains(TEXT("\"MP_EmissiveColor\""), ESearchCase::CaseSensitive));
	TestTrue(TEXT("The dump records the material settings"), Json.Contains(TEXT("\"ShadingModel\": \"Unlit\""), ESearchCase::CaseSensitive));
	TestTrue(TEXT("The dump records the Graph backend"), Json.Contains(TEXT("\"backend\": \"Graph\""), ESearchCase::CaseSensitive));
	TestTrue(TEXT("The dump records the source root"), Json.Contains(TEXT("\"root\": \"Project\""), ESearchCase::CaseSensitive));
	return true;
}

// -------------------------------------------------------------------------------------------------
// Node count and node identity
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDumpGraphNodeCountTest,
	"DreamShader.DumpGraph.NodeCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDumpGraphNodeCountTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::DumpGraphTests;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString AssetName = MakeDumpGraphAssetName();
	FDumpGraphFixture Fixture(AssetName);
	AddExpectedDumpGraphProbeWarnings(*this, Fixture.GetObjectPath());

	if (!Fixture.WriteSource(*this))
	{
		return false;
	}

	FString Json;
	int32 NodeCount = 0;
	if (!Fixture.DumpOnce(*this, 1, Json, NodeCount))
	{
		return false;
	}

	// One literal plus the named-reroute declaration/usage pair the generator inserts in front of
	// every material property it drives.
	TestEqual(TEXT("The tiny source dumps three nodes"), NodeCount, 3);

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Fixture.GetObjectPath());
	if (!Material)
	{
		AddError(TEXT("The dump-graph test material did not load; the Graph backend pin may not have applied."));
		return false;
	}

	if (NodeCount != 3)
	{
		// So a shape change in the generator is a one-line fix rather than an investigation.
		for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
		{
			if (Expression)
			{
				AddInfo(FString::Printf(TEXT("Generated node class: %s"), *Expression->GetClass()->GetName()));
			}
		}
	}

	// The invariant behind the number: the traversal visits every expression in the graph exactly
	// once. A dropped subtree -- the classic way a sink-first walk goes wrong -- shows up here even
	// when the constant above has been updated for an unrelated generator change.
	TestEqual(
		TEXT("Every expression in the graph appears in the dump"),
		NodeCount,
		Material->GetExpressions().Num());

	// Ids are positional and dense, which is what makes them comparable between two compilers.
	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		const FString Needle = FString::Printf(TEXT("\"id\": \"n%d\""), Index);
		TestTrue(
			FString::Printf(TEXT("The dump assigns node id n%d"), Index),
			Json.Contains(Needle, ESearchCase::CaseSensitive));
	}
	TestFalse(
		TEXT("The dump assigns no id past the last node"),
		Json.Contains(FString::Printf(TEXT("\"id\": \"n%d\""), NodeCount), ESearchCase::CaseSensitive));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
