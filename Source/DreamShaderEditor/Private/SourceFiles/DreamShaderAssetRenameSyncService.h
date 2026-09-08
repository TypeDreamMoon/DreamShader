// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Keeps the asset references written in .dsm / .dsf / .dsh sources pointing at the asset they named
// after somebody renames or moves it in the Content Browser.
//
// Renaming a texture used to break every source that referenced it, silently, until the next
// rebuild -- and the rebuild is where the failure finally surfaced, long after the action that
// caused it. This service closes that gap: the registry's rename event is coalesced into a batch,
// the batch rewrites the text, and the rewritten files trigger the ordinary compile-on-save path.
//
// Text first, rebuild second. The other order compiles a source that still names the old asset.

#pragma once

#include "CoreMinimal.h"

#include "Templates/Function.h"

namespace UE::DreamShader::Editor::Private
{
	/** One rename, as the asset registry reports it: two completed object paths. */
	struct FDreamShaderAssetRename
	{
		/** `/Game/Textures/T_X.T_X` -- the path the asset had. */
		FString OldObjectPath;
		/** `/Game/Props/T_Y.T_Y` -- the path it has now. */
		FString NewObjectPath;
	};

	/** One reference that changed, as it was written and as it is now. For the batch log. */
	struct FDreamShaderAssetReferenceRewrite
	{
		FString OldText;
		FString NewText;
	};

	struct FDreamShaderAssetRenameRewriteResult
	{
		bool bChanged = false;
		TArray<FDreamShaderAssetReferenceRewrite> Rewrites;
	};

	/** One file the batch wrote, for the log line and for the rebuild that follows it. */
	struct FDreamShaderAssetRenameSyncFileResult
	{
		FString SourceFilePath;
		FString BackupFilePath;
		TArray<FDreamShaderAssetReferenceRewrite> Rewrites;
	};

	struct FDreamShaderAssetRenameSyncResult
	{
		int32 ScannedFileCount = 0;
		int32 FailureCount = 0;
		TArray<FDreamShaderAssetRenameSyncFileResult> Files;
	};

	struct FDreamShaderAssetRenameSyncService
	{
		/**
		 * Resolves one `Path(...)` expression to a completed object path, exactly the way generation
		 * would. Injected rather than called directly so the rewrite below stays a pure function:
		 * the real resolver reaches for the plugin manager and Unreal's object-path validator, and a
		 * unit test that has to stand up either of those is not a unit test.
		 */
		using FPathExpressionResolver = TFunctionRef<bool(const FString& /*PathExpression*/, FString& /*OutObjectPath*/)>;

		/** Subscribes to `IAssetRegistry::OnAssetRenamed`. Idempotent. */
		static void Startup();
		/** Unsubscribes and drops whatever was still waiting for the coalescing window. */
		static void Shutdown();

		/**
		 * The pure half: source text in, source text out, using the generator's own resolver for the
		 * `Path(root, "relative")` form.
		 *
		 * Returns true when anything changed. `OutSourceText` is filled either way, so a caller may
		 * use it unconditionally.
		 */
		static bool RewriteSourceText(
			const FString& InSourceText,
			const TArray<FDreamShaderAssetRename>& InRenames,
			FString& OutSourceText,
			FDreamShaderAssetRenameRewriteResult& OutResult);

		/** Same, with the root resolution supplied by the caller. */
		static bool RewriteSourceText(
			const FString& InSourceText,
			const TArray<FDreamShaderAssetRename>& InRenames,
			FPathExpressionResolver InResolver,
			FString& OutSourceText,
			FDreamShaderAssetRenameRewriteResult& OutResult);

		/**
		 * Runs one batch now, synchronously: scan every writable project source once, back up each
		 * file that changes, write it, and report what was touched. Does NOT queue anything for
		 * rebuild -- that is the caller's second step, and it must be a second step.
		 */
		static FDreamShaderAssetRenameSyncResult SyncRenames(const TArray<FDreamShaderAssetRename>& InRenames);
	};
}
