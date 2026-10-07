// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
#pragma once

#include "CoreMinimal.h"

class FObjectPostSaveContext;
class UPackage;

namespace UE::DreamShader::Editor::Private
{
	/** Records this commandlet's saves, not unrelated files that appear in Content while it runs. */
	class FDreamShaderAssetWriteManifest
	{
	public:
		~FDreamShaderAssetWriteManifest();
		bool Start(const FString& InPath, const FString& InRunId);
		bool Finish();

	private:
		void OnPackageSaved(const FString& Filename, UPackage* Package, FObjectPostSaveContext Context);

		FString Path;
		FString RunId;
		FDelegateHandle SavedHandle;
		TMap<FString, FString> SavedFiles;
	};
}
