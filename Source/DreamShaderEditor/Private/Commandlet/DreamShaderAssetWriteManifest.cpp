// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
#include "Commandlet/DreamShaderAssetWriteManifest.h"

#include "DreamShaderModule.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

namespace UE::DreamShader::Editor::Private
{
	FDreamShaderAssetWriteManifest::~FDreamShaderAssetWriteManifest()
	{
		UPackage::PackageSavedWithContextEvent.Remove(SavedHandle);
	}

	bool FDreamShaderAssetWriteManifest::Start(const FString& InPath, const FString& InRunId)
	{
		if (InPath.IsEmpty())
		{
			return true;
		}
		Path = FPaths::ConvertRelativePathToFull(InPath);
		RunId = InRunId;
		if (RunId.IsEmpty() || IFileManager::Get().FileExists(*Path))
		{
			UE_LOG(LogDreamShader, Error, TEXT("AssetWritesManifest requires an unused output path and AssetWritesRunId."));
			return false;
		}
		SavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FDreamShaderAssetWriteManifest::OnPackageSaved);
		return true;
	}

	void FDreamShaderAssetWriteManifest::OnPackageSaved(const FString& Filename, UPackage*, FObjectPostSaveContext Context)
	{
		if (!Context.SaveSucceeded() || !FPaths::GetExtension(Filename).Equals(TEXT("uasset"), ESearchCase::IgnoreCase))
		{
			return;
		}
		FString FullPath = FPaths::ConvertRelativePathToFull(Filename);
		FPaths::NormalizeFilename(FullPath);
		const FMD5Hash Hash = FMD5Hash::HashFile(*FullPath);
		// An unreadable save is never eligible for cleanup; overwrite any earlier hash of this path too.
		SavedFiles.Add(FullPath, Hash.IsValid() ? LexToString(Hash) : FString());
	}

	bool FDreamShaderAssetWriteManifest::Finish()
	{
		if (Path.IsEmpty())
		{
			return true;
		}
		UPackage::WaitForAsyncFileWrites();
		UPackage::PackageSavedWithContextEvent.Remove(SavedHandle);
		SavedHandle.Reset();

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema"), TEXT("dreamshader-saved-assets"));
		Root->SetNumberField(TEXT("version"), 1);
		Root->SetStringField(TEXT("runId"), RunId);
		TArray<FString> Filenames;
		SavedFiles.GetKeys(Filenames);
		Filenames.Sort();
		TArray<TSharedPtr<FJsonValue>> Files;
		for (const FString& Filename : Filenames)
		{
			const TSharedRef<FJsonObject> File = MakeShared<FJsonObject>();
			File->SetStringField(TEXT("path"), Filename);
			File->SetStringField(TEXT("md5"), SavedFiles[Filename]);
			Files.Add(MakeShared<FJsonValueObject>(File));
		}
		Root->SetArrayField(TEXT("files"), Files);

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		if (!FJsonSerializer::Serialize(Root, Writer)
			|| !IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true)
			|| !FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDreamShader, Error, TEXT("Could not write asset save manifest '%s'; cleanup must leave all assets intact."), *Path);
			return false;
		}
		return true;
	}
}
