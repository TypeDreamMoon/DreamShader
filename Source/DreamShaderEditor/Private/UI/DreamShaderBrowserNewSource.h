// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Model/DreamShaderBrowserEntry.h"

namespace UE::DreamShader::Editor::Private
{
	// Creating a new .dsm / .dsf / .dsh / .dsi from the plugin's templates (Resources/Templates). The file
	// lands in a writable source root; the bridge's watcher then lists and compiles it like any
	// other save. Split so the dialog's logic is testable without Slate.

	struct FNewSourceRequest
	{
		EBrowserSourceKind Kind = EBrowserSourceKind::Material;
		FString Directory; // absolute; must be under a writable source root
		FString FileStem;  // without extension
		// Instance only: the `#pragma instance` Parent -- an asset path (`/Game/Materials/M_Base`) or the
		// name of a product under the same source root. Quotes in it are dropped.
		FString ParentReference;
	};

	// The template text with {NAME}, {FILENAME}, {ASSETPATH} and {PARENT} filled in. {NAME} is the block's
	// Name= -- the directory's path relative to its root plus the stem, which is what makes the
	// asset land next to its neighbours' in /Game. {PARENT} is ParentReference (the Instance template
	// has no Name=; its asset path follows the file). Fails when the template is missing.
	bool RenderNewSourceTemplate(const FNewSourceRequest& Request, FString& OutText, FString& OutError);

	// Writes the rendered template. Refuses an existing file, a directory outside every writable root,
	// a stem that is not a valid identifier, and an Instance without a parent. OutFilePath is the
	// absolute, normalized result.
	bool CreateNewSourceFile(const FNewSourceRequest& Request, FString& OutFilePath, FString& OutError);

	// The modal dialog. OnCreated receives the absolute path of the file that was written. An Instance
	// gets a Parent row, prefilled with DefaultParent.
	void OpenNewSourceDialog(EBrowserSourceKind Kind, const FString& DefaultDirectory, TFunction<void(const FString&)> OnCreated, const FString& DefaultParent = FString());

	const TCHAR* GetSourceKindExtension(EBrowserSourceKind Kind);
}
