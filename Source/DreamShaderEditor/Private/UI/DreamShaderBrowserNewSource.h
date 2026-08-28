// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Model/DreamShaderBrowserEntry.h"

namespace UE::DreamShader::Editor::Private
{
	// Creating a new .dsm / .dsf / .dsh from the plugin's templates (Resources/Templates). The file
	// lands in a writable source root; the bridge's watcher then lists and compiles it like any
	// other save. Split so the dialog's logic is testable without Slate.

	struct FNewSourceRequest
	{
		EBrowserSourceKind Kind = EBrowserSourceKind::Material;
		FString Directory; // absolute; must be under a writable source root
		FString FileStem;  // without extension
	};

	// The template text with {NAME}, {FILENAME} and {ASSETPATH} filled in. {NAME} is the block's
	// Name= -- the directory's path relative to its root plus the stem, which is what makes the
	// asset land next to its neighbours' in /Game. Fails when the template is missing.
	bool RenderNewSourceTemplate(const FNewSourceRequest& Request, FString& OutText, FString& OutError);

	// Writes the rendered template. Refuses an existing file, a directory outside every writable root,
	// and a stem that is not a valid identifier. OutFilePath is the absolute, normalized result.
	bool CreateNewSourceFile(const FNewSourceRequest& Request, FString& OutFilePath, FString& OutError);

	// The modal dialog. OnCreated receives the absolute path of the file that was written.
	void OpenNewSourceDialog(EBrowserSourceKind Kind, const FString& DefaultDirectory, TFunction<void(const FString&)> OnCreated);

	const TCHAR* GetSourceKindExtension(EBrowserSourceKind Kind);
}
