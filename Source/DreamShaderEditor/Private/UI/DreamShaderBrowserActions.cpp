// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/DreamShaderBrowserActions.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "DreamShaderModule.h"
#include "DreamShaderCompilerService.h"
#include "UI/DreamShaderInstanceFactory.h"
#include "UI/Model/DreamShaderBrowserModel.h"
#include "Workspace/DreamShaderWorkspaceService.h"

#include "DreamPassPipeline.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopedSlowTask.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		// Through the bridge when there is one, so the result lands in the diagnostics store (and from
		// there in diagnostics.json and the VSCode extension) and a previous error on the file is
		// cleared on success. A direct generator call does neither. Forced, and a ThinCustom product
		// stays Ephemeral, like the buttons always were.
		bool CompileSource(const FString& SourceFilePath, FString& OutMessage)
		{
			if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
			{
				return Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, OutMessage);
			}
			::UE::DreamShader::IDreamShaderCompiler* const Compiler = ::UE::DreamShader::GetDreamShaderCompiler();
			if (!Compiler)
			{
				OutMessage = LOCTEXT("CompilerUnavailable", "The DreamShader compiler module is not available, so nothing was compiled.").ToString();
				return false;
			}
			::UE::DreamShader::FDreamShaderCompileRequest Request;
			Request.SourceFilePath = SourceFilePath;
			Request.bForce = true;
			Request.ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Ephemeral;
			const ::UE::DreamShader::FDreamShaderCompileResult Result = Compiler->CompileAssets(Request);
			OutMessage = Result.Message.ToString();
			return Result.bSucceeded;
		}
	}

	void FDreamShaderBrowserActions::Notify(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 3.5f;
		Info.bFireAndForget = true;
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	bool FDreamShaderBrowserActions::Compile(FDreamShaderBrowserModel& Model, const TSharedPtr<FBrowserEntry>& Entry)
	{
		if (!Entry.IsValid() || !Entry->Source.IsSet())
		{
			return false;
		}

		FString Message;
		const bool bSuccess = CompileSource(Entry->Source->FilePath, Message);

		Notify(
			FText::Format(
				bSuccess ? LOCTEXT("CompileOk", "Compiled {0}") : LOCTEXT("CompileFail", "Failed to compile {0}"),
				FText::FromString(Entry->Source->DisplayName)),
			bSuccess);

		if (bSuccess)
		{
			Model.RefreshEntry(Entry);
		}
		else
		{
			UE_LOG(LogDreamShader, Error, TEXT("Material Content Browser compile failed: %s"), *Message);
			Model.MarkCompileFailed(Entry, Message);
		}
		return bSuccess;
	}

	void FDreamShaderBrowserActions::CompileAll(FDreamShaderBrowserModel& Model)
	{
		TArray<TSharedPtr<FBrowserEntry>> Targets = Model.GetEntries().FilterByPredicate(
			[](const TSharedPtr<FBrowserEntry>& Entry)
			{
				// Headers do not generate assets directly; their dependents recompile below.
				return Entry->Source.IsSet() && Entry->Source->Kind != EBrowserSourceKind::Header;
			});

		FScopedSlowTask SlowTask(static_cast<float>(Targets.Num()), LOCTEXT("CompilingAll", "Compiling all DreamShader sources..."));
		SlowTask.MakeDialog();

		int32 FailureCount = 0;
		for (const TSharedPtr<FBrowserEntry>& Entry : Targets)
		{
			SlowTask.EnterProgressFrame(1.0f, FText::FromString(Entry->Source->DisplayName));
			FString Message;
			if (CompileSource(Entry->Source->FilePath, Message))
			{
				Model.RefreshEntry(Entry);
			}
			else
			{
				Model.MarkCompileFailed(Entry, Message);
				++FailureCount;
			}
		}

		Notify(
			FText::Format(LOCTEXT("CompiledAll", "Compiled {0} source(s), {1} failed"), FText::AsNumber(Targets.Num()), FText::AsNumber(FailureCount)),
			FailureCount == 0);
	}

	void FDreamShaderBrowserActions::OpenSource(const FBrowserEntry& Entry, int32 Line, int32 Column)
	{
		if (Entry.Source.IsSet())
		{
			FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(Entry.Source->FilePath, Line, Column);
		}
	}

	void FDreamShaderBrowserActions::OpenMaterial(const FBrowserEntry& Entry)
	{
		// A `.dsp`'s asset is a pass pipeline, whose editor is its details panel.
		UObject* Asset = Entry.ResolveMaterial();
		if (!Asset)
		{
			Asset = Entry.ResolvePipeline();
		}
		if (Asset && GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
		}
	}

	void FDreamShaderBrowserActions::CreateInstance(FDreamShaderBrowserModel& Model, const TSharedPtr<FBrowserEntry>& Entry)
	{
		if (!Entry.IsValid() || Entry->IsLibrary() || Entry->IsPipeline())
		{
			return;
		}

		// Make sure the source has been generated so there is a parent to instance from.
		UMaterialInterface* Material = Entry->ResolveMaterial();
		if (!Material && Entry->Source.IsSet())
		{
			FString Message;
			if (CompileSource(Entry->Source->FilePath, Message))
			{
				Model.RefreshEntry(Entry);
				Material = Entry->ResolveMaterial();
			}
		}

		if (Material)
		{
			// A `.dsi` for a DreamShader product, an ordinary instance asset for anything else.
			OpenCreateInstanceDialogForParent(Material);
		}
		else
		{
			Notify(FText::Format(LOCTEXT("InstanceNeedsCompile", "Compile {0} first."), FText::FromString(Entry->GetDisplayName())), false);
		}
	}

	UMaterialInterface* FDreamShaderBrowserActions::Materialize(FDreamShaderBrowserModel& Model, const TSharedPtr<FBrowserEntry>& Entry)
	{
		if (!Entry.IsValid())
		{
			return nullptr;
		}
		// Re-resolve by path (never a captured pointer) so a delete/GC between building the panel
		// and the click cannot dangle.
		UMaterialInterface* Material = Entry->ResolveMaterial();
		if (!Material)
		{
			return nullptr;
		}

		FString Error;
		UMaterialInterface* Persisted = MaterializeDreamShaderMaterial(Material, Error);
		if (Persisted)
		{
			Notify(FText::Format(LOCTEXT("Materialized", "Materialized {0} to disk"), FText::FromString(Material->GetName())), true);
			Model.RefreshEntry(Entry);
		}
		else
		{
			Notify(FText::FromString(Error), false);
		}
		return Persisted;
	}
}

#undef LOCTEXT_NAMESPACE
