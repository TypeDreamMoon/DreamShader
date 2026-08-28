// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UObject;

namespace UE::DreamShader::Editor::Private
{
	/**
	 * The three answers to a divergence report -- a generated asset that no longer matches what
	 * DreamShader last wrote into it. Each one decides which copy is the truth: Revert says the source
	 * is and rebuilds over the asset, Adopt says the asset is and rewrites the source from it, Detach
	 * says neither and takes the asset out of DreamShader's hands for good. All three take UObject
	 * because they are offered on materials, material functions and the ThinCustom instance alike.
	 *
	 * Each one confirms with a dialog, closes (and afterwards reopens) any asset editor on the asset,
	 * toasts its result, and compiles through the bridge so the diagnostics store follows. See
	 * Docs/generation/divergence.md.
	 */
	void RevertGeneratedAssetToSource(TWeakObjectPtr<UObject> Asset);
	void AdoptGeneratedAssetIntoSource(TWeakObjectPtr<UObject> Asset);
	void DetachGeneratedAssetFromDreamShader(TWeakObjectPtr<UObject> Asset);

	/** Absolute, normalized path of the source an asset was generated from, resolved from its stamp. */
	bool TryResolveGeneratedAssetSourceFile(UObject* Asset, FString& OutSourceFilePath, FString& OutError);

	/**
	 * Close any asset editor on this asset before acting on it, and reopen it afterwards. Used only by
	 * the provenance actions -- a compile refuses instead, because it must never pop a dialog or close
	 * a window on its own. See the definitions.
	 */
	bool TryCloseAssetEditorsFor(UObject* Asset, bool& bOutWasOpen, FString& OutError);
	void ReopenAssetEditorFor(UObject* Asset, bool bWasOpen);
}
