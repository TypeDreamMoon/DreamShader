// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The three answers to a divergence report -- a generated asset that no longer matches what
// DreamShader last wrote into it. Each one decides which copy is the truth: Revert says the source is
// and rebuilds over the asset, Adopt says the asset is and rewrites the source from it, Detach says
// neither and takes the asset out of DreamShader's hands for good. Offered on materials, material
// functions and the ThinCustom instance alike, from the Content Browser context menu and the Material
// Content Browser.

#include "Provenance/DreamShaderProvenanceActions.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "Compile/DreamShaderEditorCompileAdapter.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "DreamShaderCompileService.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "DreamShaderPreprocessor.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorSourceLoading.h"

#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShaderEditorBridge"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		// Any parameter override at all, across every override array the engine version has. Read by
		// reflection rather than from a hand-written list of the arrays, because that list grows
		// between engine versions (texture collections, sparse volume textures) and a missed array
		// here would be an override the Adopt action silently drops.
		bool HasAnyParameterOverride(UMaterialInstance* Instance)
		{
			if (!Instance)
			{
				return false;
			}

			for (TFieldIterator<FProperty> It(Instance->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*It);
				if (!ArrayProperty || !ArrayProperty->GetName().EndsWith(TEXT("ParameterValues"), ESearchCase::CaseSensitive))
				{
					continue;
				}

				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Instance));
				if (ArrayHelper.Num() > 0)
				{
					return true;
				}
			}

			const FStaticParameterSet& StaticParameters = Instance->GetStaticParameters();
			return StaticParameters.StaticSwitchParameters.Num() > 0
				|| StaticParameters.EditorOnly.StaticComponentMaskParameters.Num() > 0;
		}

		void ShowDreamShaderNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
		{
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 4.0f;
			Info.bUseLargeFont = false;
			if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
			{
				Notification->SetCompletionState(CompletionState);
			}
		}

		// Through the bridge when there is one, so the diagnostics store (and everything fed from it:
		// diagnostics.json, the VSCode extension, the browser) sees the result; straight to the compile
		// service otherwise. Always forced: both callers have just decided the asset must be rebuilt.
		bool CompileSourceForProvenance(const FString& SourceFilePath, bool bInMemory, FString& OutMessage)
		{
			if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
			{
				return Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, bInMemory, OutMessage);
			}
			UE::DreamShader::Compiler::FDreamShaderCompileService CompileService(UE::DreamShader::Editor::GetEditorCompileAdapter());
			const UE::DreamShader::Compiler::FDreamShaderCompileResult Result =
				CompileService.CompileAssets(SourceFilePath, /*bForce*/ true, bInMemory);
			OutMessage = ToInvariantWireString(Result.Message);
			return Result.bSucceeded;
		}
	}

	/**
	 * Close any asset editor open on this asset, and report whether one was.
	 *
	 * A compile refuses outright when an editor is open (CheckGeneratedAssetNotOpenInEditor) -- an
	 * automatic compile must never pop a dialog or close a window somebody is working in. The two
	 * provenance actions are the opposite case: the user just clicked them, quite possibly from that
	 * very editor's toolbar, so refusing would make the menu item permanently dead exactly where it is
	 * most likely to be used.
	 *
	 * The engine's own save prompt may appear here, and for Adopt it is load-bearing rather than noise:
	 * "this asset's current contents" is what gets written back into the source, and unapplied editor
	 * changes are not part of those contents until the prompt is answered. Which is why this runs
	 * BEFORE the work, not after.
	 */
	bool TryCloseAssetEditorsFor(UObject* Asset, bool& bOutWasOpen, FString& OutError)
	{
		bOutWasOpen = false;
		if (!Asset || !IsGeneratedAssetOpenInEditor(Asset))
		{
			return true;
		}

		bOutWasOpen = true;
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
		}

		// Cancelling the save prompt cancels the close, and going ahead with an editor still holding a
		// pre-rebuild copy is the very thing being guarded against.
		if (IsGeneratedAssetOpenInEditor(Asset))
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("'%s' is still open in an asset editor, so nothing was done. Close it and try again."),
				*Asset->GetPathName());
			return false;
		}

		return true;
	}

	void ReopenAssetEditorFor(UObject* Asset, const bool bWasOpen)
	{
		if (bWasOpen && Asset && GEditor)
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				AssetEditorSubsystem->OpenEditorForAsset(Asset);
			}
		}
	}

	bool TryResolveGeneratedAssetSourceFile(UObject* Asset, FString& OutSourceFilePath, FString& OutError)
	{
		if (!Asset)
		{
			OutError = LOCTEXT("DreamShaderProvenanceNoAsset", "DreamShader could not find the selected asset.").ToString();
			return false;
		}

		const FString StampedPath = GetGeneratedAssetSourceFile(Asset);
		if (StampedPath.IsEmpty())
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("'%s' carries no DreamShader source stamp, so it was not generated by DreamShader."),
				*Asset->GetPathName());
			return false;
		}

		// Stamps are project-relative so a checkout elsewhere still recognizes its own assets; only a
		// source outside the project directory is stored absolute.
		FString AbsolutePath = StampedPath;
		if (FPaths::IsRelative(AbsolutePath))
		{
			AbsolutePath = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), StampedPath);
		}
		AbsolutePath = UE::DreamShader::NormalizeSourceFilePath(AbsolutePath);

		if (!IFileManager::Get().FileExists(*AbsolutePath))
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("'%s' was generated from '%s', which no longer exists."),
				*Asset->GetPathName(),
				*AbsolutePath);
			return false;
		}

		OutSourceFilePath = AbsolutePath;
		return true;
	}

	void RevertGeneratedAssetToSource(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		FString SourceFilePath;
		FString Error;
		if (!TryResolveGeneratedAssetSourceFile(AssetObject, SourceFilePath, Error))
		{
			ShowDreamShaderNotification(FText::FromString(Error), SNotificationItem::CS_Fail);
			return;
		}

		if (FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
					LOCTEXT("DreamShaderRevertConfirm", "Rebuild '{0}' from '{1}'?\n\nEvery hand edit in the asset is discarded. The source file is not modified."),
					FText::FromString(AssetObject->GetPathName()),
					FText::FromString(SourceFilePath))) != EAppReturnType::Yes)
		{
			return;
		}

		FString RevertMessage;

		// After the confirmation, so no window is closed for an action the user then cancels.
		bool bEditorWasOpen = false;
		FString CloseError;
		if (!TryCloseAssetEditorsFor(AssetObject, bEditorWasOpen, CloseError))
		{
			ShowDreamShaderNotification(FText::FromString(CloseError), SNotificationItem::CS_Fail);
			return;
		}

		// Rebuild in whichever world this asset lives in. Reverting a saved asset in memory only would
		// leave the hand edits on disk and report success, and the next session would read the same
		// divergence back off the package.
		const bool bPersisted = FPackageName::DoesPackageExist(AssetObject->GetOutermost()->GetName());

		// The only place this scope is taken: the user just confirmed a dialog that says the edits
		// will be discarded, which is the one authorization the divergence gate accepts.
		bool bReverted = false;
		{
			FScopedDreamShaderRevertDiverged RevertScope;
			bReverted = CompileSourceForProvenance(SourceFilePath, /*bInMemory*/ !bPersisted, RevertMessage);
		}

		ReopenAssetEditorFor(AssetObject, bEditorWasOpen);

		ShowDreamShaderNotification(
			FText::FromString(RevertMessage),
			bReverted ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader revert of '%s' from '%s': %s"),
			*AssetObject->GetPathName(),
			*SourceFilePath,
			*RevertMessage);
	}

	void AdoptGeneratedAssetIntoSource(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		FString SourceFilePath;
		FString Error;
		if (!TryResolveGeneratedAssetSourceFile(AssetObject, SourceFilePath, Error))
		{
			ShowDreamShaderNotification(FText::FromString(Error), SNotificationItem::CS_Fail);
			return;
		}

		// Conditional compilation and Adopt are mutually exclusive, and this is where that is decided.
		//
		// Adopt's entire mechanism is "decompile the asset, write the result over the source". A
		// generated asset only ever holds the POST-CUT graph -- the one branch that was taken for the
		// define set that built it -- so the text written back can describe that branch and has no way
		// to spell the others. The `#if`, the `#else` and everything inside them would be gone, from a
		// file the user asked to have UPDATED rather than rewritten, with no failure anywhere to say so.
		//
		// Read straight off disk, and that is the load-bearing detail. LoadPreparedDreamShaderSource
		// below hands back the PREPARED text, which is post-preprocessor by construction: its directive
		// lines have already been blanked out, so asking the prepared text this question always gets
		// "no" and the gate would never fire. Reading the file again costs one I/O on a path the user
		// just clicked a menu item on.
		//
		// The cheap scanner is used rather than a real preprocessor run for a second reason: it still
		// answers for a source whose conditions would FAIL to evaluate, and a half-written `#if` is
		// exactly the state a user is most likely to be in when they reach for Adopt.
		FString RawSourceText;
		if (!FFileHelper::LoadFileToString(RawSourceText, *SourceFilePath))
		{
			ShowDreamShaderNotification(
				FText::Format(
					LOCTEXT("DreamShaderAdoptSourceUnreadable", "Could not read '{0}'."),
					FText::FromString(SourceFilePath)),
				SNotificationItem::CS_Fail);
			return;
		}

		if (UE::DreamShader::DreamShaderSourceHasPreprocessorDirectives(RawSourceText))
		{
			// Raised through FailWith even though nothing here propagates an error struct:
			// .skill/gen-diagnostics.ps1 discovers every DSHnnnn by scanning for exactly this shape, so
			// a code raised any other way would exist in the source and nowhere in the docs. The FText
			// carrier is the one that takes LOCTEXT, which keeps this message in the localization
			// gather like the two refusals below it.
			FDreamShaderTextError ConditionalError;
			FailWith(
				ConditionalError,
				TEXT("DSH8149"),
				FText::Format(
					LOCTEXT("DreamShaderAdoptConditionalSource", "DSH8149: '{0}' uses conditional compilation, and '{1}' holds only the branch that was taken -- adopting it would write that one branch back over the file and delete the rest. Move the change into the matching branch of the source by hand, or use DreamShader > Detach first if this asset should stop being generated from it."),
					FText::FromString(SourceFilePath),
					FText::FromString(AssetObject->GetPathName())));

			ShowDreamShaderNotification(ConditionalError.Message, SNotificationItem::CS_Fail);

			// Spelled out again rather than logging ConditionalError.Message, so the log line stays
			// English under a localized editor and carries the code as its own field -- which is how
			// every other consumer of a DSHnnnn (the diagnostics store, diagnostics.json, the
			// extensions) reads one. A four-second toast is not a record; this is.
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("DreamShader adopt refused (%s): '%s' contains preprocessor directives, and '%s' holds only the branch they selected."),
				*ConditionalError.Code,
				*SourceFilePath,
				*AssetObject->GetPathName());
			return;
		}

		// Adopt rewrites the whole file, so it is only safe when the file produces exactly one asset.
		// A source that declares several (a Shader plus its ShaderFunctions, or one that imports a
		// .dsf) would lose everything the decompiled text does not reproduce, and the decompiler emits
		// one block, not a translation unit.
		FString PreparedSource;
		FDreamShaderError LoadError;
		if (!UE::DreamShader::Editor::LoadPreparedDreamShaderSource(SourceFilePath, PreparedSource, LoadError))
		{
			ShowDreamShaderNotification(FText::FromString(LoadError), SNotificationItem::CS_Fail);
			return;
		}

		UE::DreamShader::FTextShaderDefinition Definition;
		FString ParseError;
		if (!UE::DreamShader::FTextShaderParser::Parse(PreparedSource, Definition, ParseError))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not parse '%s': %s"), *SourceFilePath, *ParseError)), // I18N-EXEMPT
				SNotificationItem::CS_Fail);
			return;
		}

		const int32 DeclaredAssetCount = (Definition.Name.IsEmpty() ? 0 : 1) + Definition.MaterialFunctions.Num();
		if (DeclaredAssetCount != 1)
		{
			ShowDreamShaderNotification(
				FText::Format(
					LOCTEXT("DreamShaderAdoptMultiAsset", "'{0}' declares {1} assets, so adopting one of them would overwrite the others. Use DreamShader > Export DSM and merge the result by hand."),
					FText::FromString(SourceFilePath),
					FText::AsNumber(DeclaredAssetCount)),
				SNotificationItem::CS_Fail);
			return;
		}

		const FString BackupFilePath = SourceFilePath + TEXT(".bak");
		if (FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
					LOCTEXT("DreamShaderAdoptConfirm", "Rewrite '{0}' from the current contents of '{1}'?\n\nThe existing source is copied to '{2}' first. The rewritten file is the decompiler's own form, so hand-written comments, imports and formatting in it are replaced."),
					FText::FromString(SourceFilePath),
					FText::FromString(AssetObject->GetPathName()),
					FText::FromString(BackupFilePath))) != EAppReturnType::Yes)
		{
			return;
		}

		// Before the decompile, and that ordering is the point: what gets written into the source is
		// "this asset's current contents", and unapplied changes sitting in an open editor are not part
		// of those contents until the engine's save prompt has been answered.
		bool bEditorWasOpen = false;
		FString CloseError;
		if (!TryCloseAssetEditorsFor(AssetObject, bEditorWasOpen, CloseError))
		{
			ShowDreamShaderNotification(FText::FromString(CloseError), SNotificationItem::CS_Fail);
			return;
		}

		// A ThinCustom instance is not itself decompilable -- the graph lives on the hidden base
		// UMaterial, which is -- so the base is what gets written back. But the instance's own
		// parameter overrides live nowhere in that graph, so adopting an instance that carries any
		// would write a source describing everything EXCEPT the edit the user most likely made, and
		// the recompile right after would clear it. Refusing is the only honest answer.
		UObject* DecompileSubject = AssetObject;
		if (UMaterialInstance* Instance = Cast<UMaterialInstance>(AssetObject))
		{
			if (HasAnyParameterOverride(Instance))
			{
				ShowDreamShaderNotification(
					FText::Format(
						LOCTEXT("DreamShaderAdoptInstanceOverrides", "'{0}' has parameter overrides set on the generated instance, and those cannot be written back into '{1}' -- adopting would drop them. Move the values into the source as Properties defaults (or override them on a child material instance instead), then Revert."),
						FText::FromString(AssetObject->GetPathName()),
						FText::FromString(SourceFilePath)),
					SNotificationItem::CS_Fail);
				return;
			}

			DecompileSubject = Instance->Parent;
			if (!Cast<UMaterial>(DecompileSubject))
			{
				ShowDreamShaderNotification(
					FText::Format(
						LOCTEXT("DreamShaderAdoptInstanceNoBase", "'{0}' has no base material to decompile."),
						FText::FromString(AssetObject->GetPathName())),
					SNotificationItem::CS_Fail);
				return;
			}
		}

		// Decompile before the backup: a decompiler failure must not leave a .bak lying next to an
		// untouched source, which reads as "something happened here" when nothing did.
		FDreamShaderDecompileService DecompileService(GetGraphDecompiler());
		UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
		Request.Asset = DecompileSubject;
		Request.OutputFilePath = SourceFilePath;
		const UE::DreamShader::Editor::FDreamShaderDecompileResult Result = DecompileService.DecompileAsset(Request);
		if (!Result.bSucceeded)
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not decompile '%s': %s"), *AssetObject->GetPathName(), *Result.Error)), // I18N-EXEMPT
				SNotificationItem::CS_Fail);
			return;
		}

		if (IFileManager::Get().Copy(*BackupFilePath, *SourceFilePath, true) != COPY_OK)
		{
			ShowDreamShaderNotification(
				FText::Format(
					LOCTEXT("DreamShaderAdoptBackupFailed", "Could not back up '{0}' to '{1}'; nothing was written."),
					FText::FromString(SourceFilePath),
					FText::FromString(BackupFilePath)),
				SNotificationItem::CS_Fail);
			return;
		}

		FString SaveError;
		if (!FDecompiledSourceWriter::Save(Result, SaveError))
		{
			ShowDreamShaderNotification(FText::FromString(SaveError), SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader adopt failed to write '%s': %s"), *SourceFilePath, *SaveError);
			return;
		}

		// The watcher will pick the rewritten file up on its own, but only after the debounce window,
		// and it would compile it WITHOUT force -- which the just-stamped source hash would skip,
		// leaving the digest describing the pre-adopt asset. Compiling here closes the loop now.
		//
		// The scope is needed even though nothing is being discarded: the asset is still diverged from
		// the digest of the PREVIOUS generation, and the gate has no way to know the new source was
		// just written from that very asset. Rebuilding it here is what makes the two agree again.
		FScopedDreamShaderRevertDiverged RevertScope;
		const bool bPersisted = FPackageName::DoesPackageExist(AssetObject->GetOutermost()->GetName());
		FString CompileMessage;
		const bool bCompiled = CompileSourceForProvenance(SourceFilePath, /*bInMemory*/ !bPersisted, CompileMessage);

		ReopenAssetEditorFor(AssetObject, bEditorWasOpen);

		ShowDreamShaderNotification(
			FText::Format(
				LOCTEXT("DreamShaderAdoptResult", "Adopted '{0}' into '{1}' (backup: '{2}'). {3}"),
				FText::FromString(AssetObject->GetPathName()),
				FText::FromString(SourceFilePath),
				FText::FromString(BackupFilePath),
				FText::FromString(CompileMessage)),
			bCompiled ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader adopted '%s' into '%s' (backup '%s'): %s"),
			*AssetObject->GetPathName(),
			*SourceFilePath,
			*BackupFilePath,
			*CompileMessage);
	}

	void DetachGeneratedAssetFromDreamShader(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		if (!AssetObject)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderDetachNoAsset", "DreamShader could not find the selected asset."),
				SNotificationItem::CS_Fail);
			return;
		}

		if (!HasDreamShaderSourceMetadata(AssetObject))
		{
			ShowDreamShaderNotification(
				FText::Format(
					LOCTEXT("DreamShaderDetachNotGenerated", "'{0}' is not a DreamShader-generated asset."),
					FText::FromString(AssetObject->GetPathName())),
				SNotificationItem::CS_Fail);
			return;
		}

		const FString SourceFilePath = GetGeneratedAssetSourceFile(AssetObject);
		if (FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
					LOCTEXT("DreamShaderDetachConfirm", "Stop managing '{0}'?\n\nIt keeps its current contents and becomes an ordinary asset. DreamShader will never rebuild it again, and compiling '{1}' afterwards fails with an ownership error until you move or rename one of them."),
					FText::FromString(AssetObject->GetPathName()),
					FText::FromString(SourceFilePath))) != EAppReturnType::Yes)
		{
			return;
		}

		ClearDreamShaderMetadata(AssetObject);
		AssetObject->MarkPackageDirty();

		ShowDreamShaderNotification(
			FText::Format(
				LOCTEXT("DreamShaderDetachResult", "'{0}' is no longer managed by DreamShader. Save it to keep the change."),
				FText::FromString(AssetObject->GetPathName())),
			SNotificationItem::CS_Success);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader detached '%s' (was generated from '%s')."),
			*AssetObject->GetPathName(),
			*SourceFilePath);
	}
}

#undef LOCTEXT_NAMESPACE
