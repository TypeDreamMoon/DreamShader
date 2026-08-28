// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The actions the Material Content Browser offers on an entry, in one place so the inspector's
// buttons, the row context menu and the toolbar all run the same code and the model is the only
// thing that gets told about the result. Every action toasts its own outcome.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;
	struct FBrowserEntry;

	struct FDreamShaderBrowserActions
	{
		// Force-recompile the entry's source in memory. Refreshes the entry's status on success and
		// pins the failure message on it otherwise. Returns whether the compile succeeded.
		static bool Compile(FDreamShaderBrowserModel& Model, const TSharedPtr<FBrowserEntry>& Entry);

		// Force-recompile every scanned source except headers (their dependents are in the set
		// already), behind a modal slow task.
		static void CompileAll(FDreamShaderBrowserModel& Model);

		// Open the entry's source file in the preferred text editor, at a line/column when given.
		static void OpenSource(const FBrowserEntry& Entry, int32 Line = 1, int32 Column = 1);

		// Open the entry's material in its asset editor.
		static void OpenMaterial(const FBrowserEntry& Entry);

		// Open the create-instance dialog on the entry's material, compiling it first if it has
		// never been generated.
		static void CreateInstance(FDreamShaderBrowserModel& Model, const TSharedPtr<FBrowserEntry>& Entry);

		// Write the entry's memory-only material (and its base) to disk. Returns the persisted
		// material, or null on failure (already toasted).
		static UMaterialInterface* Materialize(FDreamShaderBrowserModel& Model, const TSharedPtr<FBrowserEntry>& Entry);

		static void Notify(const FText& Message, bool bSuccess);
	};
}
