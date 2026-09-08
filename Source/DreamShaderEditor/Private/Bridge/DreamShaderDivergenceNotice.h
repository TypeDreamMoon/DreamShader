// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The decision half of the divergence notification -- everything the toast has to work out before a
// single Slate widget exists, kept here so it can be asserted headlessly
// (Tests/DreamShaderDivergenceNotificationTests.cpp). See Docs/generation/divergence.md.
//
// A rebuild that is refused because the asset was hand-edited (DSH8115) used to reach the user only
// as prose in the log, pointing at a Content Browser submenu -- which for a hidden in-memory
// ThinCustom instance does not exist, because neither does the tile you would right-click. The
// bridge now raises an actionable notification instead. Three questions have to be answered first,
// and all three are pure:
//
//   1. Is this compile failure a divergence refusal at all, and which asset and source file is it
//      about?  -> TryParseDivergenceRefusal
//   2. Which actions can this asset actually be offered?  -> DecideDivergenceNoticeButtons
//   3. Has this asset already been reported in this rebuild round, and has the round grown big
//      enough that fifty toasts must collapse into one?  -> FDreamShaderDivergenceNoticeRound
//
// Nothing in this file touches Slate, UObjects or the file system.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	/**
	 * How many distinct assets may raise their own actionable toast in one rebuild round before the
	 * round stops toasting and collapses the rest into a single summary.
	 *
	 * A full scan re-compiles every source in the project, so one round legitimately reports fifty
	 * diverged assets -- and fifty stacked notifications are not fifty offers of help, they are a
	 * wall that hides the editor. The budget is per ROUND rather than per second because that is the
	 * unit the user perceives as one action ("I pressed Recompile DSM").
	 */
	inline constexpr int32 DreamShaderDivergenceNoticeCollapseThreshold = 5;

	/** The two payloads DSH8115 carries, recovered from a compile result's message. */
	struct FDreamShaderDivergenceReport
	{
		/** Full object path of the asset that was refused, e.g. `/Game/M_X.M_X`. */
		FString AssetPath;
		/** The source file it was generated from, exactly as the stamp spells it. May be empty. */
		FString SourceFilePath;

		bool IsValid() const { return !AssetPath.IsEmpty(); }
	};

	/**
	 * The two fixed phrases of the DSH8115 message that this file reads back.
	 *
	 * They are a contract with the raise site in
	 * MaterialAssetGeneration/DreamShaderGeneratedAssetMetadata.cpp: the code that formats the
	 * message and the code that parses it are in different modules of the pipeline and there is no
	 * structured channel between them -- FDreamShaderCompileResult carries a bool and an FText, and
	 * the wrapper codes the generator puts on the refusal (DSH8074, DSH8086) differ per asset type,
	 * so the code alone would not identify it either. Reword the message and this stops matching,
	 * which is why both ends carry a comment pointing at the other.
	 */
	namespace DreamShaderDivergenceNoticeAnchors
	{
		inline const TCHAR* const AssetPrefix = TEXT("Asset '");
		inline const TCHAR* const EditedByHand = TEXT("' was edited by hand since DreamShader generated it from '");
	}

	/**
	 * Recognise a divergence refusal in a compile message and pull the asset and source out of it.
	 *
	 * Tolerates any amount of wrapping: the generator prefixes the refusal with `<source>: ` before
	 * it leaves, and the bridge may add more. Returns false for every other failure message.
	 */
	inline bool TryParseDivergenceRefusal(const FString& Message, FDreamShaderDivergenceReport& OutReport)
	{
		OutReport = FDreamShaderDivergenceReport();

		const FString AssetPrefix(DreamShaderDivergenceNoticeAnchors::AssetPrefix);
		const FString Marker(DreamShaderDivergenceNoticeAnchors::EditedByHand);

		const int32 MarkerIndex = Message.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (MarkerIndex == INDEX_NONE)
		{
			return false;
		}

		// Scanned backwards from the marker rather than forwards from the start: the wrapping prefix
		// is a file path, and a path is free to contain anything at all -- including the literal
		// "Asset '". The LAST occurrence before the marker is the one the raise site wrote.
		int32 AssetPrefixIndex = INDEX_NONE;
		for (int32 Index = MarkerIndex - AssetPrefix.Len(); Index >= 0; --Index)
		{
			if (FCString::Strncmp(*Message + Index, *AssetPrefix, AssetPrefix.Len()) == 0)
			{
				AssetPrefixIndex = Index;
				break;
			}
		}
		if (AssetPrefixIndex == INDEX_NONE)
		{
			return false;
		}

		const int32 AssetStart = AssetPrefixIndex + AssetPrefix.Len();
		OutReport.AssetPath = Message.Mid(AssetStart, MarkerIndex - AssetStart);
		if (OutReport.AssetPath.IsEmpty())
		{
			return false;
		}

		const int32 SourceStart = MarkerIndex + Marker.Len();
		const int32 SourceEnd = Message.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SourceStart);
		OutReport.SourceFilePath = (SourceEnd == INDEX_NONE)
			? Message.Mid(SourceStart)
			: Message.Mid(SourceStart, SourceEnd - SourceStart);
		return true;
	}

	/**
	 * The suppression identity of one diverged asset.
	 *
	 * Lower-cased because the same asset must map to one key however the message spelled it, and a
	 * duplicate key is a duplicate toast -- the exact thing the round budget exists to prevent.
	 */
	inline FString MakeDivergenceNoticeKey(const FString& AssetPath)
	{
		return AssetPath.TrimStartAndEnd().ToLower();
	}

	/** What the notification is allowed to know about the asset, reduced to two booleans. */
	struct FDreamShaderDivergenceAssetFacts
	{
		/** The asset named by the refusal was found in this process. */
		bool bAssetResolved = false;
		/**
		 * ...and it is a memory-only ThinCustom instance that the Content Browser is currently
		 * hiding, so "right-click the asset" names something the user cannot see.
		 */
		bool bHiddenInMemoryInstance = false;
	};

	/** Which buttons the toast carries. */
	struct FDreamShaderDivergenceNoticeButtons
	{
		bool bRevert = false;
		bool bAdopt = false;
		bool bDetach = false;
		bool bShowInMemoryMaterials = false;

		int32 Num() const
		{
			return (bRevert ? 1 : 0) + (bAdopt ? 1 : 0) + (bDetach ? 1 : 0) + (bShowInMemoryMaterials ? 1 : 0);
		}
		bool HasAnyAction() const { return Num() > 0; }
	};

	/**
	 * The three resolutions need a live UObject to act on, so an asset that could not be resolved
	 * gets an informational toast and no actions at all -- offering a button that would do nothing is
	 * worse than not offering it. The fourth button is offered only when the asset is invisible,
	 * because that is the only case where it helps: turning the setting on for an asset already on
	 * screen just adds noise to the Content Browser.
	 */
	inline FDreamShaderDivergenceNoticeButtons DecideDivergenceNoticeButtons(const FDreamShaderDivergenceAssetFacts& Facts)
	{
		FDreamShaderDivergenceNoticeButtons Buttons;
		if (!Facts.bAssetResolved)
		{
			return Buttons;
		}

		Buttons.bRevert = true;
		Buttons.bAdopt = true;
		Buttons.bDetach = true;
		Buttons.bShowInMemoryMaterials = Facts.bHiddenInMemoryInstance;
		return Buttons;
	}

	/** What one reported divergence gets. */
	enum class EDreamShaderDivergenceNoticeDecision : uint8
	{
		/** Give this asset its own actionable toast. */
		Show,
		/** Say nothing: already reported in this round, or its previous toast is still on screen. */
		Suppress,
		/** Over the round's toast budget -- counted into the round's single summary instead. */
		Collapse,
	};

	/**
	 * One rebuild round's notification bookkeeping.
	 *
	 * A round is one drain of the bridge's pending queue -- one watcher batch, or one full scan --
	 * or a single compile that was asked for directly. Enter/Leave nest, so the compile calls inside
	 * a drain belong to the drain's round rather than starting fifty of their own.
	 */
	struct FDreamShaderDivergenceNoticeRound
	{
		/** Increments on every round actually opened. Only useful for diagnostics and tests. */
		int32 RoundId = 0;
		/** Every distinct asset reported in this round, whether it toasted or not. */
		TSet<FString> SeenKeys;
		/** The subset that got its own toast. Bounded by the collapse threshold. */
		TSet<FString> ToastedKeys;
		/** How many were held back for the summary. */
		int32 CollapsedCount = 0;

		/** Opens a round. Nested opens join the round in progress. True when a NEW round started. */
		bool Enter()
		{
			if (Depth++ > 0)
			{
				return false;
			}

			++RoundId;
			SeenKeys.Reset();
			ToastedKeys.Reset();
			CollapsedCount = 0;
			return true;
		}

		/** Closes one nesting level. True when the outermost one closed, i.e. the round is over. */
		bool Leave()
		{
			if (Depth <= 0)
			{
				return false;
			}
			return --Depth == 0;
		}

		bool IsInRound() const { return Depth > 0; }

		/**
		 * @param Key                          MakeDivergenceNoticeKey of the refused asset.
		 * @param bPreviousNoticeStillOnScreen this asset's last toast has not been answered or faded.
		 */
		EDreamShaderDivergenceNoticeDecision Decide(const FString& Key, bool bPreviousNoticeStillOnScreen)
		{
			bool bAlreadySeen = false;
			SeenKeys.Add(Key, &bAlreadySeen);
			if (bAlreadySeen)
			{
				// One notification per asset per round, unconditionally. A source that declares a
				// material and its function reports the same asset twice within one compile.
				return EDreamShaderDivergenceNoticeDecision::Suppress;
			}

			if (ToastedKeys.Num() >= DreamShaderDivergenceNoticeCollapseThreshold)
			{
				++CollapsedCount;
				return EDreamShaderDivergenceNoticeDecision::Collapse;
			}

			if (bPreviousNoticeStillOnScreen)
			{
				// Deliberately does NOT consume budget: nothing was added to the screen, so nothing
				// should be taken away from the four assets that still could be.
				return EDreamShaderDivergenceNoticeDecision::Suppress;
			}

			ToastedKeys.Add(Key);
			return EDreamShaderDivergenceNoticeDecision::Show;
		}

		/** True when the round must close with one summary toast standing in for the rest. */
		bool ShouldShowSummary() const { return CollapsedCount > 0; }

		/** Every diverged asset the round saw -- what the summary counts. */
		int32 GetDivergedAssetCount() const { return SeenKeys.Num(); }
		int32 GetToastedAssetCount() const { return ToastedKeys.Num(); }

	private:
		int32 Depth = 0;
	};
}
