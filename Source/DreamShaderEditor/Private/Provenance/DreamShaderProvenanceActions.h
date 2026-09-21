// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UObject;
class UMaterialInstance;
class UMaterialInstanceConstant;

namespace UE::DreamShader::Editor::Private
{
	/**
	 * The three answers to a divergence report -- a generated asset that no longer matches what
	 * DreamShader last wrote into it. Each one decides which copy is the truth: Revert says the source
	 * is and rebuilds over the asset, Adopt says the asset is and rewrites the source from it, Detach
	 * says neither and takes the asset out of DreamShader's hands for good. All three take UObject
	 * because they are offered on materials, material functions, the ThinCustom instance and a `.dsi`
	 * material instance alike.
	 *
	 * Each one confirms with a dialog, closes (and afterwards reopens) any asset editor on the asset,
	 * toasts its result, and compiles through the bridge so the diagnostics store follows. See
	 * Docs/generation/divergence.md.
	 *
	 * Adopt by the stamped source's kind:
	 *   `.dss`         every product of the file is decompiled back into it as one module (a backup first);
	 *   `.dsm`/`.dsf`  a migration: the product is decompiled to `<stem>.dss` beside it, and the 1.x file moves to `.bak`;
	 *   `.dsi`         the instance's overrides are spliced into the file (AdoptInstanceIntoSource), comments kept.
	 */
	void RevertGeneratedAssetToSource(TWeakObjectPtr<UObject> Asset);
	void AdoptGeneratedAssetIntoSource(TWeakObjectPtr<UObject> Asset);
	void DetachGeneratedAssetFromDreamShader(TWeakObjectPtr<UObject> Asset);

	/**
	 * The two answers to a Tweaked ThinCustom instance -- one whose generated content still matches and which carries
	 * parameter overrides (CONTRACT section 2.3). Adopt Tweaks writes the overrides into the `.dss` as the defaults of its
	 * uniforms and clears them from the instance; Extract Tweaks writes them into a new `.dsi` whose Parent is the
	 * instance, compiles it, and clears them from the instance. Both confirm, close editors, toast and log.
	 */
	void AdoptTweaksIntoSourceDefaults(TWeakObjectPtr<UObject> Asset);
	void ExtractTweaksToInstanceSource(TWeakObjectPtr<UObject> Asset);

	/** A generated ThinCustom instance in the Tweaked state: the asset both tweak actions accept. */
	bool IsGeneratedInstanceTweaked(UObject* Asset);

	/** IsGeneratedInstanceTweaked, and its stamped source is a `.dss` under a writable root: Adopt Tweaks applies. */
	bool CanAdoptTweaksIntoSourceDefaults(UObject* Asset);

	/** Absolute, normalized path of the source an asset was generated from, resolved from its stamp. */
	bool TryResolveGeneratedAssetSourceFile(UObject* Asset, FString& OutSourceFilePath, FString& OutError);

	/**
	 * Close any asset editor on this asset before acting on it, and reopen it afterwards. Used only by
	 * the provenance actions -- a compile refuses instead, because it must never pop a dialog or close
	 * a window on its own. See the definitions.
	 */
	bool TryCloseAssetEditorsFor(UObject* Asset, bool& bOutWasOpen, FString& OutError);
	void ReopenAssetEditorFor(UObject* Asset, bool bWasOpen);

	// ------------------------------------------------------------------------------------ headless cores
	//
	// What the UI actions above run after their dialogs: no dialog, no editor is closed, no toast. Every core refuses
	// when the asset is open in an asset editor (the editor holds a copy the decompile cannot see), and every refusal
	// writes nothing. The cores are what the automation tests drive.

	/** What one core did. */
	struct FDreamShaderProvenanceOutcome
	{
		/** The source now states the asset (or the new `.dsi` exists and compiled, for Extract). False for every refusal. */
		bool bSucceeded = false;
		/** The DSHnnnn of a refusal that has one -- DSH8149, DSH9103, or the decompiler's, rewriter's or compiler's own; empty otherwise. */
		FString Code;
		/** What happened, localised, for a toast; ToInvariantWireString of it for a log line. */
		FText Message;
		/** The source files written, in the order they were written. */
		TArray<FString> WrittenFiles;
		/** The backup made before the first write; empty when none was. */
		FString BackupFilePath;
		/** True when the rebuild after the write ran and succeeded; CompileMessage is its report either way. */
		bool bCompiled = false;
		FString CompileMessage;
	};

	/**
	 * Adopt over the source Asset was generated from (`.dss`, `.dsm`, `.dsf`; a `.dsi` goes to AdoptInstanceIntoSource).
	 * Refuses a source with preprocessor directives (DSH8149), a source that does not resolve, a Tweaked ThinCustom
	 * instance, and a migration whose `.dss` already exists. bWriteBackup copies the `.dss` to `.bak` before writing; for
	 * a migration it moves the 1.x file to `.bak`, and without it the 1.x file is deleted.
	 */
	FDreamShaderProvenanceOutcome AdoptGeneratedAssetIntoSourceCore(UObject* Asset, const FString& SourceFilePath, bool bWriteBackup);

	/**
	 * Adopt of a `.dsi` (research-instance section 4.4): the overrides the instance itself carries are decompiled
	 * (OverriddenOnly), spliced into the file by RewriteDreamShaderInstanceSource -- only declarations whose values changed
	 * are touched, so `//` comments and order survive -- and the instance is rebuilt inside the revert scope. DSH9103 when
	 * the file's Parent no longer resolves; refused for state a `.dsi` cannot state (layer or blend overrides, UsageFlags).
	 */
	FDreamShaderProvenanceOutcome AdoptInstanceIntoSource(UMaterialInstanceConstant* Instance, const FString& SourceFilePath, bool bWriteBackup);

	/**
	 * Adopt Tweaks: the Tweaked instance's overrides become the initializers / `@default` values of the `.dss` uniforms
	 * (RewriteDreamShaderUniformDefaults), the overrides are cleared from the instance, and the source is rebuilt forced.
	 */
	FDreamShaderProvenanceOutcome AdoptTweaksIntoSourceDefaultsCore(UMaterialInstance* Instance, const FString& SourceFilePath, bool bWriteBackup);

	/**
	 * Extract Tweaks: writes TargetFilePath (MakeDefaultTweaksInstanceSourcePath when empty) as a `.dsi` with Parent = the
	 * instance and one override per tweak, compiles it, and clears the tweaks from the instance only when that compile
	 * succeeded. Refuses an existing target and a target outside every writable root.
	 */
	FDreamShaderProvenanceOutcome ExtractTweaksToInstanceSourceCore(UMaterialInstance* Instance, const FString& TargetFilePath);

	/** `<source directory>/MI_<instance name>.dsi` (the project root when the source's root is read-only), numbered until it does not exist. */
	FString MakeDefaultTweaksInstanceSourcePath(UMaterialInstance* Instance, const FString& SourceFilePath);
}
