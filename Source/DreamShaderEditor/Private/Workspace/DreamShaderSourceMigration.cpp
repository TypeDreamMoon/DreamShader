#include "Workspace/DreamShaderSourceMigration.h"

#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealEdMisc.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShaderSourceMigration"

namespace UE::DreamShader::Editor::Private
{
	namespace SourceMigrationLocal
	{
		FDelegateHandle GStartupHandle;
		TWeakPtr<SNotificationItem> GNotification;

		const TCHAR* const ConfigSection = TEXT("DreamSource");
		const TCHAR* const HideKey = TEXT("bHideShaderMigrationNotice");
		const TCHAR* const WorkspaceFileName = TEXT("DreamShader.code-workspace");

		bool IsHidden()
		{
			bool bHidden = false;
			GConfig->GetBool(ConfigSection, HideKey, bHidden, GEditorPerProjectIni);
			return bHidden;
		}

		void Hide()
		{
			GConfig->SetBool(ConfigSection, HideKey, true, GEditorPerProjectIni);
			GConfig->Flush(/*bRemoveFromCache*/false, GEditorPerProjectIni);
		}

		void Dismiss()
		{
			if (const TSharedPtr<SNotificationItem> Item = GNotification.Pin())
			{
				Item->SetExpireDuration(0.0f);
				Item->SetFadeOutDuration(0.5f);
				Item->ExpireAndFadeout();
			}
			GNotification.Reset();
		}

		bool CanNotify()
		{
			return FSlateApplication::IsInitialized()
				&& !IsRunningCommandlet()
				&& !GIsAutomationTesting
				&& !FApp::IsUnattended();
		}

		void ShowResult(const FText& InText, bool bSuccess)
		{
			FNotificationInfo Info(InText);
			Info.bUseLargeFont = false;
			if (bSuccess)
			{
				// Nothing about the move takes effect until the editor starts again: offer it right here.
				Info.bFireAndForget = false;
				Info.ExpireDuration = 0.0f;
				Info.ButtonDetails.Add(FNotificationButtonInfo(
					LOCTEXT("RestartNow", "Restart Now"),
					FText::GetEmpty(),
					FSimpleDelegate::CreateLambda([] { Dismiss(); FUnrealEdMisc::Get().RestartEditor(/*bWarn*/true); }),
					SNotificationItem::CS_Success));
				Info.ButtonDetails.Add(FNotificationButtonInfo(
					LOCTEXT("RestartLater", "Later"),
					FText::GetEmpty(),
					FSimpleDelegate::CreateLambda([] { Dismiss(); }),
					SNotificationItem::CS_Success));
			}
			else
			{
				Info.ExpireDuration = 12.0f;
			}
			if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
			{
				Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
				if (bSuccess)
				{
					GNotification = Item;
				}
			}
		}

		void ShowNotice()
		{
			const FDreamShaderProjectSourceDirectory Project = ::UE::DreamShader::ResolveProjectSourceDirectory();
			if (!Project.bUsingLegacy && !Project.bLegacyIgnored)
			{
				return;
			}

			FNotificationInfo Info(Project.bUsingLegacy
				? FText::Format(LOCTEXT("UsingLegacy",
					"DreamShader: '{0}' is deprecated.\nMove it to '{1}' -- Dream sources are managed together under DreamSource/.\nIt keeps working until then."),
					FText::FromString(Project.LegacyDirectory), FText::FromString(Project.ConfiguredDirectory))
				: FText::Format(LOCTEXT("LegacyIgnored",
					"DreamShader: '{0}' is ignored because '{1}' exists.\nMerge what it still holds into '{1}', then delete it."),
					FText::FromString(Project.LegacyDirectory), FText::FromString(Project.ConfiguredDirectory)));
			Info.bFireAndForget = false;
			Info.ExpireDuration = 0.0f;
			Info.bUseLargeFont = false;
			if (Project.bUsingLegacy)
			{
				Info.ButtonDetails.Add(FNotificationButtonInfo(
					LOCTEXT("Migrate", "Migrate"),
					LOCTEXT("MigrateTip", "Move the folder now, then restart the editor. Close VS Code windows that have files in it open first."),
					FSimpleDelegate::CreateLambda([]
					{
						Dismiss();
						FString Error;
						if (FDreamShaderSourceMigration::Migrate(Error))
						{
							ShowResult(FText::Format(LOCTEXT("Migrated",
								"DreamShader: sources moved to '{0}'.\nRestart the editor to use the new place."),
								FText::FromString(::UE::DreamShader::ResolveProjectSourceDirectory().ConfiguredDirectory)), true);
						}
						else
						{
							ShowResult(FText::Format(LOCTEXT("MigrateFailed", "DreamShader: could not migrate: {0}"),
								FText::FromString(Error)), false);
						}
					}),
					SNotificationItem::CS_None));
			}
			Info.ButtonDetails.Add(FNotificationButtonInfo(
				LOCTEXT("DontShowAgain", "Don't show again"),
				LOCTEXT("DontShowAgainTip", "Stop showing this notice on this machine. The Output Log still says it."),
				FSimpleDelegate::CreateLambda([] { Hide(); Dismiss(); }),
				SNotificationItem::CS_None));
			Info.ButtonDetails.Add(FNotificationButtonInfo(
				LOCTEXT("Later", "Later"),
				FText::GetEmpty(),
				FSimpleDelegate::CreateLambda([] { Dismiss(); }),
				SNotificationItem::CS_None));

			GNotification = FSlateNotificationManager::Get().AddNotification(Info);
		}

		void DeleteTree(const FString& InDirectory)
		{
			if (IFileManager::Get().DirectoryExists(*InDirectory)
				&& !IFileManager::Get().DeleteDirectory(*InDirectory, /*RequireExists*/false, /*Tree*/true))
			{
				UE_LOG(LogDreamShader, Warning,
					TEXT("DreamShader: could not delete '%s', which the DreamSource layout replaced; delete it once nothing holds it open."),
					*InDirectory);
			}
		}
	}

	void FDreamShaderSourceMigration::Register(bool bInShowNotice)
	{
		using namespace SourceMigrationLocal;
		// After engine init: the settings CDOs, the plugin list and Slate all have to be up.
		GStartupHandle = DREAMSHADER_POST_ENGINE_INIT_DELEGATE().AddLambda([bInShowNotice]
		{
			::UE::DreamShader::LogDeprecatedSourceDirectoriesOnce();
			if (bInShowNotice && CanNotify() && !IsHidden())
			{
				ShowNotice();
			}
		});
	}

	void FDreamShaderSourceMigration::Unregister()
	{
		using namespace SourceMigrationLocal;
		if (GStartupHandle.IsValid())
		{
			DREAMSHADER_POST_ENGINE_INIT_DELEGATE().Remove(GStartupHandle);
			GStartupHandle.Reset();
		}
		GNotification.Reset();
	}

	bool FDreamShaderSourceMigration::RebaseWorkspaceFolders(
		const FString& InText, const FString& InOldDirectory, const FString& InNewDirectory, FString& OutText)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InText), Root) || !Root.IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Folders = nullptr;
		if (!Root->TryGetArrayField(TEXT("folders"), Folders))
		{
			return false;
		}

		OutText = InText;
		FString NewBase = InNewDirectory;
		FPaths::NormalizeDirectoryName(NewBase);
		NewBase += TEXT("/");
		for (const TSharedPtr<FJsonValue>& Folder : *Folders)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			FString Path;
			if (!Folder.IsValid() || !Folder->TryGetObject(Object) || !(*Object)->TryGetStringField(TEXT("path"), Path))
			{
				continue;
			}
			// "." is the source directory itself, wherever it is; an absolute path does not depend on where the file is.
			if (Path == TEXT(".") || !FPaths::IsRelative(Path))
			{
				continue;
			}
			FString Absolute = FPaths::ConvertRelativePathToFull(InOldDirectory, Path);
			FPaths::NormalizeDirectoryName(Absolute);
			FString Rebased = Absolute;
			if (!FPaths::MakePathRelativeTo(Rebased, *NewBase))
			{
				Rebased = Absolute;
			}
			// The text is edited, not re-serialized: the file is the author's too, and re-printing it would reflow it.
			OutText.ReplaceInline(
				*FString::Printf(TEXT("\"%s\""), *Path.ReplaceCharWithEscapedChar()),
				*FString::Printf(TEXT("\"%s\""), *Rebased.ReplaceCharWithEscapedChar()),
				ESearchCase::CaseSensitive);
		}
		return true;
	}

	bool FDreamShaderSourceMigration::Migrate(FString& OutError)
	{
		using namespace SourceMigrationLocal;

		const FDreamShaderProjectSourceDirectory Project = ::UE::DreamShader::ResolveProjectSourceDirectory();
		if (!Project.bUsingLegacy)
		{
			OutError = Project.bLegacyIgnored
				? FString::Printf(TEXT("'%s' already exists; merge '%s' into it by hand."), *Project.ConfiguredDirectory, *Project.LegacyDirectory)
				: FString::Printf(TEXT("'%s' is not in use, so there is nothing to move."), *Project.LegacyDirectory);
			return false;
		}
		if (IFileManager::Get().DirectoryExists(*Project.ConfiguredDirectory))
		{
			OutError = FString::Printf(TEXT("'%s' already exists; merge '%s' into it by hand."), *Project.ConfiguredDirectory, *Project.LegacyDirectory);
			return false;
		}

		IFileManager& FileManager = IFileManager::Get();
		FileManager.MakeDirectory(*FPaths::GetPath(Project.ConfiguredDirectory), /*Tree*/true);

		// A rename first: same volume, one step, nothing half-done if it fails. The editor's own directory watcher can
		// refuse it on Windows; then copy and delete, and if the delete is what fails, both folders exist and the
		// configured one wins after the restart -- the notice then says to delete the old one, and nothing was lost.
		if (!FileManager.Move(*Project.ConfiguredDirectory, *Project.LegacyDirectory, /*Replace*/false, /*EvenIfReadOnly*/true))
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (!PlatformFile.CopyDirectoryTree(*Project.ConfiguredDirectory, *Project.LegacyDirectory, /*bOverwriteAllExisting*/false))
			{
				FileManager.DeleteDirectory(*Project.ConfiguredDirectory, /*RequireExists*/false, /*Tree*/true);
				OutError = FString::Printf(TEXT("could not move or copy '%s' to '%s'."), *Project.LegacyDirectory, *Project.ConfiguredDirectory);
				return false;
			}
			if (!FileManager.DeleteDirectory(*Project.LegacyDirectory, /*RequireExists*/false, /*Tree*/true))
			{
				UE_LOG(LogDreamShader, Warning,
					TEXT("DreamShader: copied '%s' to '%s' but could not delete the old folder; delete it once nothing holds it open."),
					*Project.LegacyDirectory, *Project.ConfiguredDirectory);
			}
		}

		// The workspace file names the plugin roots relative to itself, and it is one level deeper now.
		const FString WorkspacePath = FPaths::Combine(Project.ConfiguredDirectory, WorkspaceFileName);
		FString WorkspaceText;
		if (FFileHelper::LoadFileToString(WorkspaceText, *WorkspacePath))
		{
			FString Rebased;
			if (RebaseWorkspaceFolders(WorkspaceText, Project.LegacyDirectory, Project.ConfiguredDirectory, Rebased)
				&& !Rebased.Equals(WorkspaceText, ESearchCase::CaseSensitive))
			{
				FFileHelper::SaveStringToFile(Rebased, *WorkspacePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}
		}

		// What the DreamSource layout replaced: the old bridge folder (its json manifests and bridge.db), the symbol index
		// folder, and the old generated-shader folder. Each is rebuilt in its new place by the next editor session.
		DeleteTree(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Bridge")));
		DeleteTree(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Index")));
		// Unless GeneratedShaderDirectory was set to somewhere inside it: that one is still in use.
		const FString OldIntermediate = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("DreamShader")));
		if (!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(::UE::DreamShader::GetGeneratedShaderDirectory()), OldIntermediate))
		{
			DeleteTree(OldIntermediate);
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader: moved '%s' to '%s'. Restart the editor to use it."),
			*Project.LegacyDirectory, *Project.ConfiguredDirectory);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
