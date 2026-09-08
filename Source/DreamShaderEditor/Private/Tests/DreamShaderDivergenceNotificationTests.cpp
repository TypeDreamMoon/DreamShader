// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Contract tests for the decision half of the divergence notification
// (Bridge/DreamShaderDivergenceNotice.h, Docs/generation/divergence.md).
//
// The toast itself is Slate and cannot be raised, let alone clicked, in a -nullrhi automation run.
// Everything that DECIDES what the toast says is therefore kept out of the widget code, and this
// file is what makes that separation worth having: message recognition, which buttons an asset can
// be offered, and the per-round suppression that stops a fifty-asset rebuild from stacking fifty
// notifications. No Slate, no UObjects, no file I/O -- runs in microseconds.
//
// The one thing this file deliberately does NOT assert is that the generator's real DSH8115 message
// matches the parser's anchors, because reproducing the format string here would only prove that
// this file agrees with itself. That assertion lives in
// DreamShader.Compiler.Divergence.DivergedAssetIsNotRebuilt, against a message an actual refusal
// produced.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Bridge/DreamShaderDivergenceNotice.h"

#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::Tests::DivergenceNotice
{
	/**
	 * A refusal message shaped exactly the way the generator emits it, wrapper prefix and all.
	 *
	 * The prefix matters: CheckGeneratedAssetNotDiverged raises the bare sentence, and the material
	 * and instance entry points then wrap it as `<source>: <sentence>` under their own diagnostic
	 * code. The parser has to survive that, and any further wrapping a caller adds.
	 */
	inline FString MakeRefusal(const TCHAR* AssetPath, const TCHAR* SourceFile, const TCHAR* Prefix = TEXT(""))
	{
		return FString::Printf(
			TEXT("%sAsset '%s' was edited by hand since DreamShader generated it from '%s', so it was NOT rebuilt (rebuilding would destroy those edits). ")
			TEXT("Use the notification, or right-click the asset > DreamShader, and choose one: Revert to Source, Adopt Into Source, or Detach From DreamShader."),
			Prefix, AssetPath, SourceFile);
	}

	inline FString MakeAssetPath(int32 Index)
	{
		return FString::Printf(TEXT("/Game/Materials/M_Diverged%d.M_Diverged%d"), Index, Index);
	}
}

// -------------------------------------------------------------------------------------------------
// Recognising the refusal.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDivergenceNoticeParseTest,
	"DreamShader.Compiler.DivergenceNotification.RefusalCarriesAssetAndSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDivergenceNoticeParseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests::DivergenceNotice;

	{
		FDreamShaderDivergenceReport Report;
		TestTrue(
			TEXT("A bare refusal is recognised"),
			TryParseDivergenceRefusal(
				MakeRefusal(TEXT("/Game/Materials/M_Emissive.M_Emissive"), TEXT("DShader/Materials/M_Emissive.dsm")),
				Report));
		TestEqual(TEXT("...and names the asset"), Report.AssetPath, FString(TEXT("/Game/Materials/M_Emissive.M_Emissive")));
		TestEqual(TEXT("...and names the source"), Report.SourceFilePath, FString(TEXT("DShader/Materials/M_Emissive.dsm")));
		TestTrue(TEXT("...and is a usable report"), Report.IsValid());
	}

	{
		// What the material and instance entry points actually hand the bridge.
		FDreamShaderDivergenceReport Report;
		TestTrue(
			TEXT("A wrapped refusal is recognised"),
			TryParseDivergenceRefusal(
				MakeRefusal(
					TEXT("/Game/Materials/M_Emissive.M_Emissive"),
					TEXT("DShader/Materials/M_Emissive.dsm"),
					TEXT("I:/Project/DShader/Materials/M_Emissive.dsm: ")),
				Report));
		TestEqual(TEXT("...and still names the asset"), Report.AssetPath, FString(TEXT("/Game/Materials/M_Emissive.M_Emissive")));
	}

	{
		// The reason the scan runs backwards from the marker rather than forwards from the start: a
		// wrapping path is arbitrary text and may contain the asset prefix itself.
		FDreamShaderDivergenceReport Report;
		TestTrue(
			TEXT("A refusal wrapped in a path containing the anchor is recognised"),
			TryParseDivergenceRefusal(
				MakeRefusal(
					TEXT("/Game/Materials/M_Emissive.M_Emissive"),
					TEXT("DShader/Materials/M_Emissive.dsm"),
					TEXT("I:/Asset 'Store'/DShader/M_Emissive.dsm: ")),
				Report));
		TestEqual(TEXT("...and names the right asset"), Report.AssetPath, FString(TEXT("/Game/Materials/M_Emissive.M_Emissive")));
	}

	{
		// The ThinCustom case the whole feature exists for: an instance with no tile to right-click.
		FDreamShaderDivergenceReport Report;
		TestTrue(
			TEXT("An in-memory instance refusal is recognised"),
			TryParseDivergenceRefusal(
				MakeRefusal(TEXT("/Game/DreamShader/MI_Cloud.MI_Cloud"), TEXT("DShader/MI_Cloud.dsm")),
				Report));
		TestEqual(TEXT("...and names the instance"), Report.AssetPath, FString(TEXT("/Game/DreamShader/MI_Cloud.MI_Cloud")));
	}

	// Every other failure is a source edit away from being fixed and must stay in the log.
	const TCHAR* const NotRefusals[] =
	{
		TEXT(""),
		TEXT("Asset '/Game/M_X.M_X' is open in an asset editor, so it was NOT rebuilt."),
		TEXT("DShader/M_X.dsm(12,5): Unknown type 'vec5'."),
		TEXT("Asset '/Game/M_X.M_X' was edited by hand"),
		TEXT("was edited by hand since DreamShader generated it from 'DShader/M_X.dsm'"),
	};
	for (const TCHAR* const NotRefusal : NotRefusals)
	{
		FDreamShaderDivergenceReport Report;
		TestFalse(
			FString::Printf(TEXT("Not a divergence refusal: '%s'"), NotRefusal),
			TryParseDivergenceRefusal(NotRefusal, Report));
	}

	{
		// An asset whose stamp is empty still deserves a notification -- the bridge fills the source
		// in from the file it was compiling. What it must not do is fail to recognise the refusal.
		FDreamShaderDivergenceReport Report;
		TestTrue(
			TEXT("A refusal with an empty source is still recognised"),
			TryParseDivergenceRefusal(MakeRefusal(TEXT("/Game/M_X.M_X"), TEXT("")), Report));
		TestEqual(TEXT("...and names the asset"), Report.AssetPath, FString(TEXT("/Game/M_X.M_X")));
		TestTrue(TEXT("...with an empty source for the caller to fill in"), Report.SourceFilePath.IsEmpty());
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Which buttons the toast carries.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDivergenceNoticeButtonsTest,
	"DreamShader.Compiler.DivergenceNotification.ButtonsFollowTheAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDivergenceNoticeButtonsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	{
		// The three resolutions all act on a live UObject. Offering a button that cannot do anything
		// is worse than offering none, so an unresolvable asset gets an informational toast.
		FDreamShaderDivergenceAssetFacts Facts;
		Facts.bAssetResolved = false;
		Facts.bHiddenInMemoryInstance = false;
		const FDreamShaderDivergenceNoticeButtons Buttons = DecideDivergenceNoticeButtons(Facts);
		TestEqual(TEXT("An unresolved asset gets no action buttons"), Buttons.Num(), 0);
		TestFalse(TEXT("...and reports that it has none"), Buttons.HasAnyAction());
	}

	{
		FDreamShaderDivergenceAssetFacts Facts;
		Facts.bAssetResolved = true;
		Facts.bHiddenInMemoryInstance = false;
		const FDreamShaderDivergenceNoticeButtons Buttons = DecideDivergenceNoticeButtons(Facts);
		TestTrue(TEXT("A saved asset is offered Revert"), Buttons.bRevert);
		TestTrue(TEXT("A saved asset is offered Adopt"), Buttons.bAdopt);
		TestTrue(TEXT("A saved asset is offered Detach"), Buttons.bDetach);
		TestFalse(TEXT("A visible asset is NOT offered the visibility toggle"), Buttons.bShowInMemoryMaterials);
		TestEqual(TEXT("...three buttons in total"), Buttons.Num(), 3);
	}

	{
		// Issue #31's actual dead end: the asset is hidden, so "right-click the asset" names
		// something that is not on screen at all.
		FDreamShaderDivergenceAssetFacts Facts;
		Facts.bAssetResolved = true;
		Facts.bHiddenInMemoryInstance = true;
		const FDreamShaderDivergenceNoticeButtons Buttons = DecideDivergenceNoticeButtons(Facts);
		TestTrue(TEXT("A hidden in-memory instance keeps the three resolutions"),
			Buttons.bRevert && Buttons.bAdopt && Buttons.bDetach);
		TestTrue(TEXT("...and gains Show In-Memory Materials"), Buttons.bShowInMemoryMaterials);
		TestEqual(TEXT("...four buttons in total"), Buttons.Num(), 4);
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// The suppression key.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDivergenceNoticeKeyTest,
	"DreamShader.Compiler.DivergenceNotification.SuppressionKeyIdentifiesTheAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDivergenceNoticeKeyTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	TestEqual(
		TEXT("Two spellings of one asset path are one key"),
		MakeDivergenceNoticeKey(TEXT("/Game/M_X.M_X")),
		MakeDivergenceNoticeKey(TEXT("  /Game/m_x.M_X  ")));
	TestNotEqual(
		TEXT("Two different assets are two keys"),
		MakeDivergenceNoticeKey(TEXT("/Game/M_X.M_X")),
		MakeDivergenceNoticeKey(TEXT("/Game/M_Y.M_Y")));
	TestTrue(
		TEXT("An empty path makes an empty key rather than a crash"),
		MakeDivergenceNoticeKey(TEXT("")).IsEmpty());

	return true;
}

// -------------------------------------------------------------------------------------------------
// Per-round suppression, and the collapse.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDivergenceNoticeRoundTest,
	"DreamShader.Compiler.DivergenceNotification.OneNoticePerAssetPerRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDivergenceNoticeRoundTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests::DivergenceNotice;

	FDreamShaderDivergenceNoticeRound Round;
	const FString Key = MakeDivergenceNoticeKey(MakeAssetPath(1));
	const FString OtherKey = MakeDivergenceNoticeKey(MakeAssetPath(2));

	TestTrue(TEXT("The first Enter opens a round"), Round.Enter());
	TestTrue(TEXT("...and the round is open"), Round.IsInRound());
	TestEqual(
		TEXT("The first report of an asset toasts"),
		static_cast<int32>(Round.Decide(Key, /*bPreviousNoticeStillOnScreen*/ false)),
		static_cast<int32>(EDreamShaderDivergenceNoticeDecision::Show));

	// A source that declares a material and a function reports the same asset twice inside one
	// compile; a full scan can reach the same asset from two sources.
	TestEqual(
		TEXT("The second report of the same asset is suppressed"),
		static_cast<int32>(Round.Decide(Key, /*bPreviousNoticeStillOnScreen*/ false)),
		static_cast<int32>(EDreamShaderDivergenceNoticeDecision::Suppress));
	TestEqual(
		TEXT("A different asset in the same round still toasts"),
		static_cast<int32>(Round.Decide(OtherKey, /*bPreviousNoticeStillOnScreen*/ false)),
		static_cast<int32>(EDreamShaderDivergenceNoticeDecision::Show));
	TestEqual(TEXT("The round saw two diverged assets"), Round.GetDivergedAssetCount(), 2);
	TestEqual(TEXT("...and toasted both"), Round.GetToastedAssetCount(), 2);
	TestFalse(TEXT("Two is not enough to collapse"), Round.ShouldShowSummary());

	TestTrue(TEXT("Leaving the outermost scope closes the round"), Round.Leave());
	TestFalse(TEXT("...and the round is closed"), Round.IsInRound());

	// A new round is a new decision: the user pressed Recompile again, and the asset is still
	// diverged, so it is still worth saying so.
	TestTrue(TEXT("The next Enter opens a NEW round"), Round.Enter());
	TestEqual(
		TEXT("The same asset toasts again in the next round"),
		static_cast<int32>(Round.Decide(Key, /*bPreviousNoticeStillOnScreen*/ false)),
		static_cast<int32>(EDreamShaderDivergenceNoticeDecision::Show));
	TestEqual(TEXT("The new round counts only its own assets"), Round.GetDivergedAssetCount(), 1);
	Round.Leave();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDivergenceNoticeNestingTest,
	"DreamShader.Compiler.DivergenceNotification.NestedCompilesShareTheRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDivergenceNoticeNestingTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests::DivergenceNotice;

	// The bridge opens a round around the whole drain and another around each compile inside it. If
	// the inner one started a round of its own, every asset would be "the first in its round" and the
	// budget would never bind -- which is the fifty-toast bug the budget exists to prevent.
	FDreamShaderDivergenceNoticeRound Round;
	const FString Key = MakeDivergenceNoticeKey(MakeAssetPath(1));

	TestTrue(TEXT("The drain opens the round"), Round.Enter());
	const int32 DrainRoundId = Round.RoundId;

	TestFalse(TEXT("A compile inside the drain joins it"), Round.Enter());
	TestEqual(TEXT("...and does not start a new round"), Round.RoundId, DrainRoundId);
	Round.Decide(Key, false);
	TestFalse(TEXT("Leaving the inner scope does not close the round"), Round.Leave());
	TestTrue(TEXT("...and the round is still open"), Round.IsInRound());

	TestEqual(
		TEXT("The asset reported inside the nested compile is still remembered"),
		static_cast<int32>(Round.Decide(Key, false)),
		static_cast<int32>(EDreamShaderDivergenceNoticeDecision::Suppress));

	TestTrue(TEXT("Leaving the drain closes the round"), Round.Leave());
	TestFalse(TEXT("An unbalanced extra Leave closes nothing"), Round.Leave());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDivergenceNoticeLiveToastTest,
	"DreamShader.Compiler.DivergenceNotification.ALiveToastIsNotStacked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDivergenceNoticeLiveToastTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests::DivergenceNotice;

	FDreamShaderDivergenceNoticeRound Round;
	Round.Enter();

	// Save the same broken source twice inside a few seconds: two rounds, but the first toast is
	// still waiting for an answer, and a second one on top of it is not a second offer of help.
	TestEqual(
		TEXT("An asset whose previous toast is still up is suppressed"),
		static_cast<int32>(Round.Decide(MakeDivergenceNoticeKey(MakeAssetPath(1)), /*bPreviousNoticeStillOnScreen*/ true)),
		static_cast<int32>(EDreamShaderDivergenceNoticeDecision::Suppress));
	TestEqual(TEXT("...and it counts as a diverged asset"), Round.GetDivergedAssetCount(), 1);
	TestEqual(TEXT("...but consumed no toast budget"), Round.GetToastedAssetCount(), 0);

	// The point of not charging it: the assets that CAN still be shown must not lose their slots to
	// one that was never displayed.
	for (int32 Index = 2; Index <= 1 + DreamShaderDivergenceNoticeCollapseThreshold; ++Index)
	{
		TestEqual(
			FString::Printf(TEXT("Asset %d still toasts"), Index),
			static_cast<int32>(Round.Decide(MakeDivergenceNoticeKey(MakeAssetPath(Index)), false)),
			static_cast<int32>(EDreamShaderDivergenceNoticeDecision::Show));
	}
	TestEqual(
		TEXT("The whole budget was available to them"),
		Round.GetToastedAssetCount(),
		DreamShaderDivergenceNoticeCollapseThreshold);

	Round.Leave();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDivergenceNoticeCollapseTest,
	"DreamShader.Compiler.DivergenceNotification.LargeRoundCollapsesIntoOneSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDivergenceNoticeCollapseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests::DivergenceNotice;

	// Recompile DSM on a project where fifty assets were hand-edited. Fifty stacked notifications are
	// not fifty offers of help, they are a wall in front of the editor.
	constexpr int32 DivergedAssets = 50;

	FDreamShaderDivergenceNoticeRound Round;
	Round.Enter();

	int32 Shown = 0;
	int32 Collapsed = 0;
	for (int32 Index = 0; Index < DivergedAssets; ++Index)
	{
		switch (Round.Decide(MakeDivergenceNoticeKey(MakeAssetPath(Index)), false))
		{
		case EDreamShaderDivergenceNoticeDecision::Show:     ++Shown; break;
		case EDreamShaderDivergenceNoticeDecision::Collapse:  ++Collapsed; break;
		default: break;
		}
	}

	TestEqual(TEXT("Only the budget is toasted"), Shown, DreamShaderDivergenceNoticeCollapseThreshold);
	TestEqual(TEXT("The rest are collapsed"), Collapsed, DivergedAssets - DreamShaderDivergenceNoticeCollapseThreshold);
	TestTrue(TEXT("...into one summary"), Round.ShouldShowSummary());
	TestEqual(TEXT("The summary counts every diverged asset in the round"), Round.GetDivergedAssetCount(), DivergedAssets);
	TestTrue(
		TEXT("A fifty-asset round produces at most budget+1 notifications"),
		Shown + 1 <= DreamShaderDivergenceNoticeCollapseThreshold + 1);
	Round.Leave();

	// Exactly at the budget nothing collapses: the summary is for rounds the user could not have
	// answered one at a time, not for every round that reaches the limit.
	FDreamShaderDivergenceNoticeRound ExactRound;
	ExactRound.Enter();
	for (int32 Index = 0; Index < DreamShaderDivergenceNoticeCollapseThreshold; ++Index)
	{
		ExactRound.Decide(MakeDivergenceNoticeKey(MakeAssetPath(Index)), false);
	}
	TestFalse(TEXT("A round exactly at the budget shows no summary"), ExactRound.ShouldShowSummary());
	TestEqual(TEXT("...and toasted every one of them"), ExactRound.GetToastedAssetCount(), DreamShaderDivergenceNoticeCollapseThreshold);
	ExactRound.Leave();

	// A round with nothing diverged says nothing at all.
	FDreamShaderDivergenceNoticeRound QuietRound;
	QuietRound.Enter();
	TestFalse(TEXT("A clean round shows no summary"), QuietRound.ShouldShowSummary());
	TestEqual(TEXT("...and counts nothing"), QuietRound.GetDivergedAssetCount(), 0);
	QuietRound.Leave();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
