#include "DreamShaderMaterialGeneratorPrivate.h"

#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "FileHelpers.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		FString GetSourceMetadataValue(UObject* Asset, const TCHAR* Key)
		{
			if (!Asset)
			{
				return FString();
			}

			UPackage* Package = Asset->GetOutermost();
			if (!Package)
			{
				return FString();
			}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			return Package->GetMetaData().GetValue(Asset, Key);
#else
			if (UMetaData* MetaData = Package->GetMetaData())
			{
				return MetaData->GetValue(Asset, Key);
			}
			return FString();
#endif
		}

		// Project-relative, forward-slashed source path. Stamped into asset metadata so a checkout on a
		// different machine (or a moved project directory) records the same identity instead of an
		// absolute path that differs per machine. Sources outside the project (rare) stay absolute.
		FString MakeProjectRelativeSourcePath(const FString& SourceFilePath)
		{
			FString Path = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
			const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			FPaths::MakePathRelativeTo(Path, *ProjectDir);
			return Path;
		}
	}

	FString BuildSourceHash(const FString& SourceText)
	{
		return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*SourceText)); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	bool IsGeneratedAssetSourceCurrent(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash)
	{
		if (!Asset || SourceHash.IsEmpty())
		{
			return false;
		}

		const FString ExistingSourceFile = GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceFile"));
		if (ExistingSourceFile.IsEmpty())
		{
			return false;
		}

		const FString ExistingSourceHash = GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceHash"));

		// Compare the project-relative source path (as stamped by ApplySourceMetadata): a different
		// machine or a moved project recognizes its own generated assets instead of regenerating them.
		// The content hash is the primary identity; the path disambiguates two sources that hash alike.
		return ExistingSourceFile.Equals(MakeProjectRelativeSourcePath(SourceFilePath), ESearchCase::IgnoreCase)
			&& ExistingSourceHash.Equals(SourceHash, ESearchCase::CaseSensitive);
	}

	bool HasDreamShaderSourceMetadata(UObject* Asset)
	{
		// DreamShader stamps DreamShader.SourceFile on every asset it generates; its presence is the
		// ownership marker used to decide whether an existing asset is safe to regenerate over.
		return !GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceFile")).IsEmpty();
	}

	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath)
	{
		ApplySourceMetadata(Asset, SourceFilePath, FString());
	}

	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash)
	{
		if (!Asset)
		{
			return;
		}

		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			return;
		}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		FMetaData& MetaData = Package->GetMetaData();
		MetaData.SetValue(Asset, TEXT("DreamShader.SourceFile"), *MakeProjectRelativeSourcePath(SourceFilePath));
		if (!SourceHash.IsEmpty())
		{
			MetaData.SetValue(Asset, TEXT("DreamShader.SourceHash"), *SourceHash);
		}
#else
		UMetaData* MetaData = Package->GetMetaData();
		if (!MetaData)
		{
			return;
		}
		MetaData->SetValue(Asset, TEXT("DreamShader.SourceFile"), *MakeProjectRelativeSourcePath(SourceFilePath));
		if (!SourceHash.IsEmpty())
		{
			MetaData->SetValue(Asset, TEXT("DreamShader.SourceHash"), *SourceHash);
		}
#endif
	}

	bool SaveAssetPackage(UObject* Asset, FString& OutError)
	{
		check(Asset);

		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(Asset->GetOutermost());
		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			OutError = FString::Printf(TEXT("Generated DreamShader asset '%s' could not be saved."), *Asset->GetPathName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		return true;
	}

	bool SaveAssetPackages(const TArray<UObject*>& Assets, FString& OutError)
	{
		// One SavePackages call for a dependent asset PAIR (e.g. a ThinCustom base material and the
		// instance parented to it): SavePackages saves exactly the packages passed in -- it does NOT
		// gather referenced (parent) packages -- so both must be listed explicitly, and both must be
		// dirty (bOnlyDirty=true).
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Reserve(Assets.Num());
		for (UObject* Asset : Assets)
		{
			check(Asset);
			PackagesToSave.AddUnique(Asset->GetOutermost());
		}

		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			OutError = TEXT("Generated DreamShader asset packages could not be saved.");
			for (UObject* Asset : Assets)
			{
				OutError += FString::Printf(TEXT(" '%s'"), *Asset->GetPathName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			return false;
		}

		return true;
	}
}
