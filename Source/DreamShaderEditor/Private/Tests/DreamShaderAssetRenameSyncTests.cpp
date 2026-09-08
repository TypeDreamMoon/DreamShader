// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The rewrite half of the asset-rename sync service, as a pure function: source text in, source text
// out. No asset registry, no editor, no files.
//
// The two tests that matter most are the negative ones. A false hit inside a `Function` body would
// splice an asset path into HLSL that is handed straight to the shader compiler, and a prefix match
// rather than an exact one would rewrite `/Game/FooBar` every time somebody renamed `/Game/Foo`.
// Both failures are silent, both are on a path that runs unattended, and both would be found by a
// user rather than by a build.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SourceFiles/DreamShaderAssetRenameSyncService.h"

#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	namespace
	{
		/**
		 * A stand-in for the generator's `TryResolveDreamShaderAssetReference`.
		 *
		 * It knows the root spellings and nothing else -- no plugin manager, no object-path validator,
		 * no mounted content. That is the point: what these tests assert is the rewrite, and a rewrite
		 * that could only be exercised with a live editor would not be exercised at all.
		 */
		bool ResolveTestPathExpression(const FString& InPathExpression, FString& OutObjectPath)
		{
			const FString Text = InPathExpression.TrimStartAndEnd();
			if (!Text.StartsWith(TEXT("Path("), ESearchCase::IgnoreCase) || !Text.EndsWith(TEXT(")")))
			{
				return false;
			}

			FString RootText;
			FString PathText;
			if (!Text.Mid(5, Text.Len() - 6).Split(TEXT(","), &RootText, &PathText))
			{
				return false;
			}

			auto Clean = [](FString& InOutText)
			{
				InOutText.TrimStartAndEndInline();
				if (InOutText.Len() >= 2 && InOutText.StartsWith(TEXT("\"")) && InOutText.EndsWith(TEXT("\"")))
				{
					InOutText = InOutText.Mid(1, InOutText.Len() - 2);
				}
			};
			Clean(RootText);
			Clean(PathText);
			if (RootText.IsEmpty() || PathText.IsEmpty())
			{
				return false;
			}

			RootText.ReplaceInline(TEXT("\\"), TEXT("/"));
			TArray<FString> Segments;
			RootText.ParseIntoArray(Segments, TEXT("/"), true);
			if (Segments.IsEmpty())
			{
				return false;
			}

			FString Base;
			int32 Consumed = 1;
			const FString& First = Segments[0];
			if (First.Equals(TEXT("Game"), ESearchCase::IgnoreCase))
			{
				Base = TEXT("/Game");
			}
			else if (First.Equals(TEXT("Engine"), ESearchCase::IgnoreCase))
			{
				Base = TEXT("/Engine");
			}
			else if (First.StartsWith(TEXT("Plugin."), ESearchCase::IgnoreCase))
			{
				Base = TEXT("/") + First.RightChop(7);
			}
			else if (First.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase))
			{
				Base = TEXT("/") + First.RightChop(8);
			}
			else if ((First.Equals(TEXT("Plugin"), ESearchCase::IgnoreCase)
					|| First.Equals(TEXT("Plugins"), ESearchCase::IgnoreCase))
				&& Segments.Num() >= 2)
			{
				Base = TEXT("/") + Segments[1];
				Consumed = 2;
			}
			else
			{
				return false;
			}

			for (int32 Index = Consumed; Index < Segments.Num(); ++Index)
			{
				Base += TEXT("/") + Segments[Index];
			}

			FString Joined = Base + TEXT("/") + PathText;
			int32 SlashIndex = INDEX_NONE;
			if (Joined.FindLastChar(TCHAR('/'), SlashIndex))
			{
				const FString Tail = Joined.Mid(SlashIndex + 1);
				if (!Tail.Contains(TEXT(".")))
				{
					Joined += TEXT(".") + Tail;
				}
			}

			OutObjectPath = Joined;
			return true;
		}

		FDreamShaderAssetRename MakeRename(const TCHAR* InOld, const TCHAR* InNew)
		{
			FDreamShaderAssetRename Rename;
			Rename.OldObjectPath = InOld;
			Rename.NewObjectPath = InNew;
			return Rename;
		}

		FString Rewrite(
			const FString& InText,
			const TArray<FDreamShaderAssetRename>& InRenames,
			FDreamShaderAssetRenameRewriteResult& OutResult)
		{
			auto ResolverBody = [](const FString& InExpression, FString& OutObjectPath)
			{
				return ResolveTestPathExpression(InExpression, OutObjectPath);
			};

			FString Output;
			FDreamShaderAssetRenameSyncService::RewriteSourceText(
				InText,
				InRenames,
				FDreamShaderAssetRenameSyncService::FPathExpressionResolver(ResolverBody),
				Output,
				OutResult);
			return Output;
		}

		/**
		 * The whole assertion, in one call.
		 *
		 * Written as a helper rather than as a bare `TestEqual` at each site so that every argument
		 * arrives as an `FString`: the overload set has both `(FString, FString)` and
		 * `(FStringView, FStringView)` in it, and a mixed `FString` / `const TCHAR*` pair is one
		 * implicit conversion away from being ambiguous.
		 */
		void CheckRewrite(
			FAutomationTestBase& InTest,
			const TCHAR* InWhat,
			const FString& InSource,
			const TArray<FDreamShaderAssetRename>& InRenames,
			const FString& InExpected)
		{
			FDreamShaderAssetRenameRewriteResult Result;
			InTest.TestEqual(InWhat, Rewrite(InSource, InRenames, Result), InExpected);
		}

		/** The one rename most of the fixtures below use. */
		TArray<FDreamShaderAssetRename> RenameTextureX()
		{
			return { MakeRename(TEXT("/Game/Textures/T_X.T_X"), TEXT("/Game/Textures/T_Y.T_Y")) };
		}
	}
}

// -------------------------------------------------------------------------------------------------
// The written forms
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderAssetRenameSyncSpellingsTest,
	"DreamShader.AssetRenameSync.RewritesEverySpelling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderAssetRenameSyncSpellingsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	const TArray<FDreamShaderAssetRename> Renames = RenameTextureX();

	// Path(root, "relative") -- the canonical form. The root still contains the new path, so the
	// relative spelling survives and only the tail moves.
	CheckRewrite(
		*this,
		TEXT("Path(root, relative) keeps the relative spelling"),
		TEXT("Texture2D A = Path(Game, \"Textures/T_X\");"),
		Renames,
		TEXT("Texture2D A = Path(Game, \"Textures/T_Y\");"));

	// Path("absolute").
	CheckRewrite(
		*this,
		TEXT("Path(\"absolute\")"),
		TEXT("Texture2D B = Path(\"/Game/Textures/T_X\");"),
		Renames,
		TEXT("Texture2D B = Path(\"/Game/Textures/T_Y\");"));

	// A bare quoted absolute path.
	CheckRewrite(
		*this,
		TEXT("Bare quoted absolute path"),
		TEXT("Texture2D C = \"/Game/Textures/T_X\";"),
		Renames,
		TEXT("Texture2D C = \"/Game/Textures/T_Y\";"));

	// The shelled form, quoted -- what Copy Reference pastes into a string.
	CheckRewrite(
		*this,
		TEXT("Quoted Class'/Path' form keeps its class prefix"),
		TEXT("Texture2D D = \"Texture2D'/Game/Textures/T_X.T_X'\";"),
		Renames,
		TEXT("Texture2D D = \"Texture2D'/Game/Textures/T_Y.T_Y'\";"));

	// The shelled form with a full class path, unquoted, in a metadata entry.
	CheckRewrite(
		*this,
		TEXT("Unquoted /Script/Engine.Class'/Path' form"),
		TEXT("[Texture = /Script/Engine.Texture2D'/Game/Textures/T_X.T_X']"),
		Renames,
		TEXT("[Texture = /Script/Engine.Texture2D'/Game/Textures/T_Y.T_Y']"));

	return true;
}

// -------------------------------------------------------------------------------------------------
// Object path with and without `.Name`, and the styles that must survive
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderAssetRenameSyncStyleTest,
	"DreamShader.AssetRenameSync.PreservesWrittenStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderAssetRenameSyncStyleTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	const TArray<FDreamShaderAssetRename> Renames = RenameTextureX();

	// A package path written without `.Name` comes back without one...
	CheckRewrite(
		*this,
		TEXT("Package-path spelling stays a package path"),
		TEXT("\"/Game/Textures/T_X\""),
		Renames,
		TEXT("\"/Game/Textures/T_Y\""));

	// ...and one written with `.Name` keeps it, carrying the new asset's name.
	CheckRewrite(
		*this,
		TEXT("Object-path spelling stays an object path"),
		TEXT("\"/Game/Textures/T_X.T_X\""),
		Renames,
		TEXT("\"/Game/Textures/T_Y.T_Y\""));

	// The same distinction inside a rooted relative reference.
	CheckRewrite(
		*this,
		TEXT("Relative object-path spelling is rebuilt as an object path"),
		TEXT("Path(Game, \"Textures/T_X.T_X\")"),
		Renames,
		TEXT("Path(Game, \"Textures/T_Y.T_Y\")"));

	// Folder segments carried by the root are part of the root, so only the tail is rewritten.
	CheckRewrite(
		*this,
		TEXT("Extra root segments are left alone"),
		TEXT("Path(\"Game/Textures\", \"T_X\")"),
		Renames,
		TEXT("Path(\"Game/Textures\", \"T_Y\")"));

	// Whatever separated the two arguments is not this service's to normalize.
	CheckRewrite(
		*this,
		TEXT("Argument spacing survives"),
		TEXT("Path(Game,\"Textures/T_X\")"),
		Renames,
		TEXT("Path(Game,\"Textures/T_Y\")"));

	// Moved out from under its own root: no relative spelling is left, so the call collapses to the
	// single-argument absolute form rather than being left pointing at nothing.
	CheckRewrite(
		*this,
		TEXT("A move out of the root collapses to the absolute form"),
		TEXT("Path(\"Game/Textures\", \"T_X\")"),
		{ MakeRename(TEXT("/Game/Textures/T_X.T_X"), TEXT("/Game/Props/T_Y.T_Y")) },
		TEXT("Path(\"/Game/Props/T_Y\")"));

	return true;
}

// -------------------------------------------------------------------------------------------------
// What must NOT change
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderAssetRenameSyncFunctionBodyTest,
	"DreamShader.AssetRenameSync.SkipsFunctionBodies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderAssetRenameSyncFunctionBodyTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// A Function body is raw HLSL handed to the shader compiler. Nothing in it is an asset reference,
	// and a string that merely looks like one must come back exactly as the author wrote it.
	const FString Source =
		TEXT("Shader(Name=\"M_Probe\")\n")
		TEXT("{\n")
		TEXT("    Properties = { Texture2D A = Path(Game, \"Textures/T_X\"); }\n")
		TEXT("\n")
		TEXT("    Function Probe(float2 UV, out float3 Result)\n")
		TEXT("    {\n")
		TEXT("        // \"/Game/Textures/T_X\" is a comment in HLSL, not a reference.\n")
		TEXT("        float3 Marker = float3(0, 0, 0); // \"/Game/Textures/T_X\"\n")
		TEXT("        Result = float3(UV, 0) + Marker;\n")
		TEXT("    }\n")
		TEXT("\n")
		TEXT("    Properties = { Texture2D B = \"/Game/Textures/T_X\"; }\n")
		TEXT("}\n");

	const FString Expected =
		TEXT("Shader(Name=\"M_Probe\")\n")
		TEXT("{\n")
		TEXT("    Properties = { Texture2D A = Path(Game, \"Textures/T_Y\"); }\n")
		TEXT("\n")
		TEXT("    Function Probe(float2 UV, out float3 Result)\n")
		TEXT("    {\n")
		TEXT("        // \"/Game/Textures/T_X\" is a comment in HLSL, not a reference.\n")
		TEXT("        float3 Marker = float3(0, 0, 0); // \"/Game/Textures/T_X\"\n")
		TEXT("        Result = float3(UV, 0) + Marker;\n")
		TEXT("    }\n")
		TEXT("\n")
		TEXT("    Properties = { Texture2D B = \"/Game/Textures/T_Y\"; }\n")
		TEXT("}\n");

	FDreamShaderAssetRenameRewriteResult Result;
	const FString Actual = Rewrite(Source, RenameTextureX(), Result);

	TestEqual(TEXT("The Function body is untouched and the declarations around it are not"), Actual, Expected);
	TestEqual(TEXT("Exactly two references were rewritten"), Result.Rewrites.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderAssetRenameSyncSharedPrefixTest,
	"DreamShader.AssetRenameSync.IgnoresSharedPrefixes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderAssetRenameSyncSharedPrefixTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// `/Game/Foo` is not a prefix match for `/Game/FooBar`, and it is not one for `/Game/Foo/T_X`
	// either. Matching is exact equality on the whole path, in either spelling.
	const TArray<FDreamShaderAssetRename> Renames =
	{
		MakeRename(TEXT("/Game/Foo.Foo"), TEXT("/Game/Renamed.Renamed"))
	};

	const FString Source =
		TEXT("A = \"/Game/FooBar\";\n")
		TEXT("B = \"/Game/FooBar.FooBar\";\n")
		TEXT("C = \"/Game/Foo/T_X\";\n")
		TEXT("D = \"/Game/Foo\";\n");

	const FString Expected =
		TEXT("A = \"/Game/FooBar\";\n")
		TEXT("B = \"/Game/FooBar.FooBar\";\n")
		TEXT("C = \"/Game/Foo/T_X\";\n")
		TEXT("D = \"/Game/Renamed\";\n");

	FDreamShaderAssetRenameRewriteResult Result;
	TestEqual(TEXT("Only the exact path is rewritten"), Rewrite(Source, Renames, Result), Expected);
	TestEqual(TEXT("One reference, not four"), Result.Rewrites.Num(), 1);

	// Nothing matched at all: the text comes back byte for byte, and nothing is reported as changed.
	const FString Untouched = TEXT("A = \"/Game/Somewhere/T_Q\";\r\nB = Path(Game, \"Other/T_Q\");\r\n");
	FDreamShaderAssetRenameRewriteResult MissResult;
	TestEqual(TEXT("A batch that hits nothing changes nothing"), Rewrite(Untouched, Renames, MissResult), Untouched);
	TestFalse(TEXT("...and does not report a change"), MissResult.bChanged);

	return true;
}

// -------------------------------------------------------------------------------------------------
// Batches
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderAssetRenameSyncBatchTest,
	"DreamShader.AssetRenameSync.AppliesWholeBatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderAssetRenameSyncBatchTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	const TArray<FDreamShaderAssetRename> Renames =
	{
		MakeRename(TEXT("/Game/Textures/T_X.T_X"), TEXT("/Game/Textures/T_Y.T_Y")),
		MakeRename(TEXT("/Game/Curves/CV_Ramp.CV_Ramp"), TEXT("/Game/Curves/CV_Falloff.CV_Falloff"))
	};

	// CRLF, because the working tree is CRLF and a rewrite that normalized line endings would turn a
	// one-line change into a whole-file diff.
	const FString Source =
		TEXT("Texture2D A = Path(Game, \"Textures/T_X\");\r\n")
		TEXT("Curve = Path(Game, \"Curves/CV_Ramp\");\r\n")
		TEXT("Other = \"/Game/Textures/T_X.T_X\";\r\n");

	const FString Expected =
		TEXT("Texture2D A = Path(Game, \"Textures/T_Y\");\r\n")
		TEXT("Curve = Path(Game, \"Curves/CV_Falloff\");\r\n")
		TEXT("Other = \"/Game/Textures/T_Y.T_Y\";\r\n");

	FDreamShaderAssetRenameRewriteResult Result;
	TestEqual(TEXT("Both renames land in one pass, CRLF intact"), Rewrite(Source, Renames, Result), Expected);
	TestEqual(TEXT("Three references rewritten"), Result.Rewrites.Num(), 3);
	TestTrue(TEXT("The result reports a change"), Result.bChanged);

	// A rename chain inside one batch -- what a rename followed by a move produces -- is collapsed, so
	// a reference to the first path lands on the last one rather than on the intermediate, which by
	// then names nothing.
	CheckRewrite(
		*this,
		TEXT("A -> B -> C in one batch resolves to C"),
		TEXT("X = \"/Game/T_A.T_A\";"),
		{
			MakeRename(TEXT("/Game/T_A.T_A"), TEXT("/Game/T_B.T_B")),
			MakeRename(TEXT("/Game/T_B.T_B"), TEXT("/Game/Moved/T_B.T_B"))
		},
		TEXT("X = \"/Game/Moved/T_B.T_B\";"));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
