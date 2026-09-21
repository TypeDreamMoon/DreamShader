// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The three answers to a divergence report -- a generated asset that no longer matches what
// DreamShader last wrote into it. Each one decides which copy is the truth: Revert says the source is
// and rebuilds over the asset, Adopt says the asset is and rewrites the source from it, Detach says
// neither and takes the asset out of DreamShader's hands for good. Offered on materials, material
// functions, the ThinCustom instance and a `.dsi` material instance alike, from the Content Browser
// context menu and the Material Content Browser.
//
// Plus the two answers to a Tweaked ThinCustom instance: Adopt Tweaks writes the
// instance's parameter overrides into the `.dss` as uniform defaults, Extract Tweaks writes them into a
// new `.dsi`.
//
// Every action is a UI shell (confirm, close editors, toast) around a headless core (the header says
// what each core does); the automation tests drive the cores.

#include "Provenance/DreamShaderProvenanceActions.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "DreamShaderCompilerInterface.h"
// FDecompiledSourceWriter, and the decompile request / result the Adopt of a `.dss` / `.dsm` / `.dsf` runs.
#include "Decompiler/DreamShaderDecompileService.h"
// DecompileMaterialInstance: the overrides a `.dsi` Adopt and the two tweak actions write back.
#include "Decompiler/DreamShaderInstanceDecompiler.h"
#include "DreamShaderTextWireUtils.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderGeneratedAssets.h"
// ResolveDreamShaderSourceProducts (the Adopt gate) and RunDreamShaderLang2Pipeline (the parsed and bound file the
// span-splice rewriters need).
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerDiagnostics.h"
// RewriteDreamShaderInstanceSource, RewriteDreamShaderUniformDefaults, PrintDreamShaderInstance.
#include "Lang/LangInstanceSource.h"
#include "Tools/DreamShaderDecompileTools.h"

#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShaderEditorBridge"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		void ShowDreamShaderProvenanceNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
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
		// service otherwise. Always forced: every caller has just decided the asset must be rebuilt.
		bool CompileSourceForProvenance(const FString& SourceFilePath, bool bAllowEphemeralThinCustom, FString& OutMessage)
		{
			if (FDreamShaderEditorBridge* Bridge = GetDreamShaderEditorBridge())
			{
				// The bridge is always the interactive path, which leaves a ThinCustom product Ephemeral.
				return Bridge->CompileSourceFile(SourceFilePath, /*bForce*/ true, OutMessage);
			}
			::UE::DreamShader::IDreamShaderCompiler* const Compiler = ::UE::DreamShader::GetDreamShaderCompiler();
			if (!Compiler)
			{
				OutMessage = LOCTEXT("ProvenanceCompilerUnavailable", "The DreamShader compiler module is not available, so nothing was rebuilt.").ToString();
				return false;
			}
			::UE::DreamShader::FDreamShaderCompileRequest Request;
			Request.SourceFilePath = SourceFilePath;
			Request.bForce = true;
			Request.ThinCustomPersistence = bAllowEphemeralThinCustom
				? ::UE::DreamShader::EThinCustomPersistence::Ephemeral
				: ::UE::DreamShader::EThinCustomPersistence::Materialized;
			const ::UE::DreamShader::FDreamShaderCompileResult Result = Compiler->CompileAssets(Request);
			OutMessage = ToInvariantWireString(Result.Message);
			return Result.bSucceeded;
		}

		FDreamShaderProvenanceOutcome MakeProvenanceRefusal(const FString& Code, const FText& Message)
		{
			FDreamShaderProvenanceOutcome Outcome;
			Outcome.Code = Code;
			Outcome.Message = Message;
			return Outcome;
		}

		/** A refusal carrying the first error of a sink, in the located wire form and with its code; FallbackMessage when the sink holds no error. */
		FDreamShaderProvenanceOutcome MakeProvenanceRefusalFromSink(
			const ::UE::DreamShader::Lang::FLangDiagnosticSink& Sink,
			const FString& FallbackFilePath,
			const FText& FallbackMessage)
		{
			if (const ::UE::DreamShader::Lang::FLangDiagnostic* const FirstError = Sink.FirstError())
			{
				return MakeProvenanceRefusal(
					FirstError->Code,
					FText::FromString(::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(*FirstError, FallbackFilePath)));
			}
			return MakeProvenanceRefusal(FString(), FallbackMessage);
		}

		/**
		 * The first diagnostic of an instance decompile a write-back must not go past, whatever its severity: an error, or
		 * one of the three kinds of state a `.dsi` has no spelling for -- overrides of layer or blend parameters (DSH9101),
		 * a UsageFlags override (DSH9104), a scalar atlas or curve override (DSH9105). Writing the rest back and rebuilding
		 * would silently drop them from the asset. Null when there is none.
		 */
		const ::UE::DreamShader::Lang::FLangDiagnostic* FindProvenanceFatalInstanceDiagnostic(const ::UE::DreamShader::Lang::FLangDiagnosticSink& Sink)
		{
			for (const ::UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				if (Diagnostic.Severity == ::UE::DreamShader::Lang::ELangSeverity::Error
					|| Diagnostic.Code.Equals(TEXT("DSH9101"), ESearchCase::CaseSensitive)
					|| Diagnostic.Code.Equals(TEXT("DSH9104"), ESearchCase::CaseSensitive)
					|| Diagnostic.Code.Equals(TEXT("DSH9105"), ESearchCase::CaseSensitive))
				{
					return &Diagnostic;
				}
			}
			return nullptr;
		}

		/** The refusal for an instance decompile that failed or met state a `.dsi` cannot state. */
		FDreamShaderProvenanceOutcome MakeProvenanceInstanceDecompileRefusal(
			const ::UE::DreamShader::Lang::FLangDiagnosticSink& Sink,
			const UObject* Instance,
			const FString& FallbackFilePath)
		{
			const ::UE::DreamShader::Lang::FLangDiagnostic* const Fatal = FindProvenanceFatalInstanceDiagnostic(Sink);
			return MakeProvenanceRefusal(
				Fatal ? Fatal->Code : FString(),
				FText::Format(
					LOCTEXT("DreamShaderProvenanceInstanceDecompileRefused", "'{0}' holds state a .dsi cannot state, so nothing was written: {1}"),
					FText::FromString(Instance ? Instance->GetPathName() : FString()),
					Fatal
						? FText::FromString(::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(*Fatal, FallbackFilePath))
						: LOCTEXT("DreamShaderProvenanceInstanceDecompileNoReason", "the decompiler gave no reason")));
		}

		/** The Parent spelling a hand-written `.dsi` uses: the package path when the object is named after its package, else the object path. */
		FString MakeProvenanceParentReference(const UObject* Object)
		{
			const FString ObjectPath = Object->GetPathName();
			const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
			return FPackageName::GetShortName(PackageName).Equals(Object->GetName(), ESearchCase::CaseSensitive) ? PackageName : ObjectPath;
		}

		/**
		 * Drops every parameter override the instance itself carries, statics included, through one update context -- the
		 * permutation is updated once, when the context closes (parameter updates).
		 */
		void ClearProvenanceInstanceTweaks(UMaterialInstance* Instance)
		{
			if (Instance)
			{
				FMaterialInstanceParameterUpdateContext UpdateContext(Instance, EMaterialInstanceClearParameterFlag::All);
			}
		}

		/** `<Source>.bak`, overwriting an older one. False when the copy failed. */
		bool BackUpProvenanceSourceFile(const FString& SourceFilePath, FString& OutBackupFilePath)
		{
			OutBackupFilePath = SourceFilePath + TEXT(".bak");
			return IFileManager::Get().Copy(*OutBackupFilePath, *SourceFilePath, /*bReplace*/ true) == COPY_OK;
		}

		/** Loads a source for a span splice, and runs the compile's front half over it: the parsed and bound tree the rewriters read. */
		bool RunProvenanceSourceCheck(
			const FString& SourceFilePath,
			FString& OutRawText,
			::UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineResult& OutRun)
		{
			OutRawText.Reset();
			if (!FFileHelper::LoadFileToString(OutRawText, *SourceFilePath))
			{
				return false;
			}
			::UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineOptions Options;
			// Stops after validation: nothing is emitted, and a `.dsi`'s parent is not compiled first.
			Options.bEmitAssets = false;
			::UE::DreamShader::Editor::Compiler::RunDreamShaderLang2Pipeline(SourceFilePath, Options, OutRun);
			return true;
		}

		/** The tree was parsed from exactly the file's bytes. False for a preprocessed text, whose spans do not address the file. */
		bool IsProvenanceSpliceTextExact(const FString& RawText, const ::UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineResult& Run)
		{
			return !Run.bSourceHadPreprocessorDirectives
				&& Run.Source.IsValid()
				&& Run.Source->GetText().Equals(RawText, ESearchCase::CaseSensitive);
		}
	}

	/**
	 * Close any asset editor open on this asset, and report whether one was.
	 *
	 * A compile refuses outright when an editor is open (CheckGeneratedAssetNotOpenInEditor) -- an
	 * automatic compile must never pop a dialog or close a window somebody is working in. The provenance
	 * actions are the opposite case: the user just clicked them, quite possibly from that very editor's
	 * toolbar, so refusing would make the menu item permanently dead exactly where it is most likely to
	 * be used.
	 *
	 * The engine's own save prompt may appear here, and for Adopt it is load-bearing rather than noise:
	 * "this asset's current contents" is what gets written back into the source, and unapplied editor
	 * changes are not part of those contents until the prompt is answered. Which is why this runs
	 * BEFORE the work, not after -- and why every headless core refuses outright when an editor is still open.
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
		AbsolutePath = ::UE::DreamShader::NormalizeSourceFilePath(AbsolutePath);

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

	bool IsGeneratedInstanceTweaked(UObject* Asset)
	{
		const UDreamShaderMaterialInstance* const Instance = Cast<UDreamShaderMaterialInstance>(Asset);
		return Instance != nullptr
			&& IsThinCustomInstancePair(Instance)
			&& ClassifyGeneratedAsset(Asset) == EDreamShaderDigestState::Tweaked;
	}

	bool CanAdoptTweaksIntoSourceDefaults(UObject* Asset)
	{
		if (!IsGeneratedInstanceTweaked(Asset))
		{
			return false;
		}
		FString SourceFilePath;
		FString Error;
		return TryResolveGeneratedAssetSourceFile(Asset, SourceFilePath, Error)
			&& ::UE::DreamShader::IsDreamShaderLang2File(SourceFilePath)
			&& ::UE::DreamShader::IsWritableSourceFilePath(SourceFilePath);
	}

	FString MakeDefaultTweaksInstanceSourcePath(UMaterialInstance* Instance, const FString& SourceFilePath)
	{
		// Beside the source, so the instance's asset lands beside the material's; the project root when that folder ships
		// with a plugin and is read-only.
		const ::UE::DreamShader::FDreamShaderSourceRoot* const Root = ::UE::DreamShader::FindSourceRootForFile(SourceFilePath);
		const FString Directory = (Root && Root->bWritable)
			? FPaths::GetPath(::UE::DreamShader::NormalizeSourceFilePath(SourceFilePath))
			: ::UE::DreamShader::NormalizeSourceFilePath(::UE::DreamShader::GetSourceShaderDirectory());

		const FString BaseStem = FString::Printf( /* I18N-EXEMPT: file name */
			TEXT("MI_%s"),
			*::UE::DreamShader::SanitizeIdentifier(Instance ? Instance->GetName() : FString(TEXT("Instance"))));
		FString Candidate = FPaths::Combine(Directory, BaseStem + TEXT(".dsi"));
		for (int32 Suffix = 2; IFileManager::Get().FileExists(*Candidate); ++Suffix)
		{
			Candidate = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%d.dsi"), *BaseStem, Suffix)); /* I18N-EXEMPT: file name */
		}
		return ::UE::DreamShader::NormalizeSourceFilePath(Candidate);
	}

	// ---------------------------------------------------------------------------------------- Revert

	void RevertGeneratedAssetToSource(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		FString SourceFilePath;
		FString Error;
		if (!TryResolveGeneratedAssetSourceFile(AssetObject, SourceFilePath, Error))
		{
			ShowDreamShaderProvenanceNotification(FText::FromString(Error), SNotificationItem::CS_Fail);
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
			ShowDreamShaderProvenanceNotification(FText::FromString(CloseError), SNotificationItem::CS_Fail);
			return;
		}

		// Rebuild in whichever world this asset lives in. Reverting a saved asset in memory only would
		// leave the hand edits on disk and report success, and the next session would read the same
		// divergence back off the package. A `.dsi` instance always saves, whatever this says.
		const bool bPersisted = FPackageName::DoesPackageExist(AssetObject->GetOutermost()->GetName());

		// The only place this scope is taken for a discard: the user just confirmed a dialog that says the
		// edits will be discarded, which is the one authorization the divergence gate accepts.
		bool bReverted = false;
		{
			FScopedDreamShaderRevertDiverged RevertScope;
			bReverted = CompileSourceForProvenance(SourceFilePath, /*bAllowEphemeralThinCustom*/ !bPersisted, RevertMessage);
		}

		ReopenAssetEditorFor(AssetObject, bEditorWasOpen);

		ShowDreamShaderProvenanceNotification(
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

	// ----------------------------------------------------------------------------------------- Adopt

	FDreamShaderProvenanceOutcome AdoptGeneratedAssetIntoSourceCore(UObject* Asset, const FString& InSourceFilePath, const bool bWriteBackup)
	{
		if (!Asset)
		{
			return MakeProvenanceRefusal(FString(), LOCTEXT("DreamShaderProvenanceNoAsset", "DreamShader could not find the selected asset."));
		}

		const FString SourceFilePath = ::UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		if (::UE::DreamShader::IsDreamShaderInstanceFile(SourceFilePath))
		{
			// A `.dsi` builds one plain material instance, never the ThinCustom class.
			UMaterialInstanceConstant* const Instance = Cast<UMaterialInstanceConstant>(Asset);
			if (!Instance || Asset->IsA<UDreamShaderMaterialInstance>())
			{
				return MakeProvenanceRefusal(FString(), FText::Format(
					LOCTEXT("DreamShaderAdoptInstanceNotMic", "'{0}' is built from the instance file '{1}' but is not a plain material instance, so nothing was adopted."),
					FText::FromString(Asset->GetPathName()),
					FText::FromString(SourceFilePath)));
			}
			return AdoptInstanceIntoSource(Instance, SourceFilePath, bWriteBackup);
		}

		// Conditional compilation and Adopt are mutually exclusive, and this is where that is decided.
		//
		// Adopt's entire mechanism is "decompile the asset, write the result over the source". A
		// generated asset only ever holds the POST-CUT graph -- the one branch that was taken for the
		// define set that built it -- so the text written back can describe that branch and has no way
		// to spell the others. The `#if`, the `#else` and everything inside them would be gone, from a
		// file the user asked to have UPDATED rather than rewritten, with no failure anywhere to say so.
		//
		// Product resolution answers both of this gate's questions with the compile's own front half:
		// whether the file or any header it includes carries a directive, taken or not, and how many
		// assets the file builds. The directive flag is asked first because it is set as soon as the
		// preprocessor has run, so a source whose parse or bind fails still answers it. A source whose
		// preprocessor fails -- a half-written `#if`, the state a user is most likely to be in when they
		// reach for Adopt -- does not get that far and is refused below with the preprocessor's error;
		// nothing is written either way.
		::UE::DreamShader::Editor::Compiler::FDreamShaderProductResolution Resolution;
		const bool bResolved = ::UE::DreamShader::Editor::Compiler::ResolveDreamShaderSourceProducts(SourceFilePath, Resolution);

		if (Resolution.bSourceHadPreprocessorDirectives)
		{
			// Raised through FailWith even though nothing here propagates an error struct:
			// .skill/gen-diagnostics.ps1 discovers every DSHnnnn by scanning for exactly this shape, so
			// a code raised any other way would exist in the source and nowhere in the docs. The FText
			// carrier is the one that takes LOCTEXT, which keeps this message in the localization
			// gather like the refusals below it.
			FDreamShaderTextError ConditionalError;
			FailWith(
				ConditionalError,
				TEXT("DSH8149"),
				FText::Format(
					LOCTEXT("DreamShaderAdoptConditionalSource", "DSH8149: '{0}' uses conditional compilation, and '{1}' holds only the branch that was taken -- adopting it would write that one branch back over the file and delete the rest. Move the change into the matching branch of the source by hand, or use DreamShader > Detach first if this asset should stop being generated from it."),
					FText::FromString(SourceFilePath),
					FText::FromString(Asset->GetPathName())));

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
				*Asset->GetPathName());
			return MakeProvenanceRefusal(ConditionalError.Code, ConditionalError.Message);
		}

		if (!bResolved)
		{
			// `<file>(<line>,<col>): DSHnnnn: <message>`, the form every other DreamShader message takes.
			return MakeProvenanceRefusalFromSink(
				Resolution.Diagnostics,
				SourceFilePath,
				FText::Format(
					LOCTEXT("DreamShaderAdoptUnresolved", "'{0}' could not be resolved to the assets it builds, so nothing was adopted."),
					FText::FromString(SourceFilePath)));
		}

		if (Resolution.Products.IsEmpty())
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptNoProducts", "'{0}' declares no asset any more, so '{1}' cannot be adopted into it."),
				FText::FromString(SourceFilePath),
				FText::FromString(Asset->GetPathName())));
		}

		// A Tweaked ThinCustom instance holds exactly what the last build wrote, plus the user's parameter overrides --
		// which live on the instance, not in the graph a decompile reads. Adopting would rewrite the source to what it
		// already says; the overrides have their own two actions.
		if (IsGeneratedInstanceTweaked(Asset))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptTweakedInstance", "'{0}' still matches its source and only carries parameter overrides, so there is nothing to adopt; use DreamShader > Adopt Tweaks as Source Defaults or Extract Tweaks to .dsi instead."),
				FText::FromString(Asset->GetPathName())));
		}

		if (IsGeneratedAssetOpenInEditor(Asset))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptOpenInEditor", "'{0}' is open in an asset editor, whose copy a decompile cannot see, so nothing was adopted; save and close the editor, then adopt again."),
				FText::FromString(Asset->GetPathName())));
		}

		// A 1.x source adopts as a migration: the 2.0 text is written beside it, and the 1.x file leaves the source tree,
		// or both would build the same asset.
		const bool bMigration = ::UE::DreamShader::IsDreamShaderMaterialFile(SourceFilePath) || ::UE::DreamShader::IsDreamShaderFunctionFile(SourceFilePath);
		const FString OutputFilePath = bMigration
			? ::UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(FPaths::GetPath(SourceFilePath), FPaths::GetBaseFilename(SourceFilePath) + TEXT(".dss")))
			: SourceFilePath;
		if (bMigration && IFileManager::Get().FileExists(*OutputFilePath))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptMigrationTargetExists", "'{0}' already exists, so '{1}' cannot be adopted as a migration into it; move or delete that file first."),
				FText::FromString(OutputFilePath),
				FText::FromString(SourceFilePath)));
		}

		// Decompile before the backup: a decompiler failure must not leave a .bak lying next to an untouched source, which
		// reads as "something happened here" when nothing did. Every product of the stamped source goes into the one
		// module (SourceFilePath), each keeps its own asset path (bKeepAssetPath), and the text is class-exact 2.0.
		::UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
		Request.Asset = Asset;
		Request.OutputFilePath = OutputFilePath;
		Request.Format = ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Dss;
		Request.bKeepAssetPath = true;
		Request.SourceFilePath = SourceFilePath;
		const ::UE::DreamShader::Editor::FDreamShaderDecompileResult Result = RunDreamShaderDecompileRequest(Request);
		if (!Result.bSucceeded)
		{
			const ::UE::DreamShader::Lang::FLangDiagnostic* const FirstError = Result.Diagnostics.FindByPredicate(
				[](const ::UE::DreamShader::Lang::FLangDiagnostic& Diagnostic)
				{
					return Diagnostic.Severity == ::UE::DreamShader::Lang::ELangSeverity::Error;
				});
			return MakeProvenanceRefusal(
				FirstError ? FirstError->Code : FString(),
				FText::Format(
					LOCTEXT("DreamShaderAdoptDecompileFailed", "DreamShader could not decompile '{0}', so nothing was written: {1}"),
					FText::FromString(Asset->GetPathName()),
					FText::FromString(DescribeDreamShaderDecompileFailure(Result))));
		}

		FDreamShaderProvenanceOutcome Outcome;
		if (!bMigration && bWriteBackup && !BackUpProvenanceSourceFile(SourceFilePath, Outcome.BackupFilePath))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptBackupFailed", "Could not back up '{0}' to '{1}'; nothing was written."),
				FText::FromString(SourceFilePath),
				FText::FromString(Outcome.BackupFilePath)));
		}

		FString SaveError;
		if (!FDecompiledSourceWriter::Save(Result, SaveError))
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader adopt failed to write '%s': %s"), *Result.OutputFilePath, *SaveError);
			return MakeProvenanceRefusal(FString(), FText::FromString(SaveError));
		}
		Outcome.WrittenFiles.Add(Result.OutputFilePath);

		if (bMigration)
		{
			const FString LegacyBackupFilePath = SourceFilePath + TEXT(".bak");
			const bool bMovedAside = bWriteBackup
				? IFileManager::Get().Move(*LegacyBackupFilePath, *SourceFilePath, /*bReplace*/ true)
				: IFileManager::Get().Delete(*SourceFilePath);
			if (!bMovedAside)
			{
				// Two files building one asset is worse than no migration: take the new one back out.
				IFileManager::Get().Delete(*Result.OutputFilePath);
				return MakeProvenanceRefusal(FString(), FText::Format(
					LOCTEXT("DreamShaderAdoptMigrationMoveFailed", "Could not move '{0}' out of the source tree, so the new '{1}' was removed again and nothing changed."),
					FText::FromString(SourceFilePath),
					FText::FromString(Result.OutputFilePath)));
			}
			if (bWriteBackup)
			{
				Outcome.BackupFilePath = LegacyBackupFilePath;
			}
		}
		Outcome.bSucceeded = true;

		// The watcher would pick the rewritten file up on its own, but only after the debounce window, and it would compile
		// it WITHOUT force -- which the just-stamped source hash would skip, leaving the digest describing the pre-adopt
		// asset. Compiling here closes the loop now.
		//
		// The scope is needed even though nothing is being discarded: the asset is still diverged from the digest of the
		// PREVIOUS generation, and the gate has no way to know the new source was just written from that very asset.
		{
			FScopedDreamShaderRevertDiverged RevertScope;
			const bool bPersisted = FPackageName::DoesPackageExist(Asset->GetOutermost()->GetName());
			Outcome.bCompiled = CompileSourceForProvenance(Result.OutputFilePath, /*bAllowEphemeralThinCustom*/ !bPersisted, Outcome.CompileMessage);
		}

		const FText BackupText = Outcome.BackupFilePath.IsEmpty()
			? LOCTEXT("DreamShaderAdoptNoBackup", "none")
			: FText::FromString(Outcome.BackupFilePath);
		Outcome.Message = bMigration
			? FText::Format(
				LOCTEXT("DreamShaderAdoptMigrationResult", "Adopted '{0}' as a migration into '{1}' (the 1.x source's backup: '{2}'). {3}"),
				FText::FromString(Asset->GetPathName()),
				FText::FromString(Result.OutputFilePath),
				BackupText,
				FText::FromString(Outcome.CompileMessage))
			: FText::Format(
				LOCTEXT("DreamShaderAdoptResult", "Adopted '{0}' into '{1}' (backup: '{2}'). {3}"),
				FText::FromString(Asset->GetPathName()),
				FText::FromString(Result.OutputFilePath),
				BackupText,
				FText::FromString(Outcome.CompileMessage));
		return Outcome;
	}

	FDreamShaderProvenanceOutcome AdoptInstanceIntoSource(UMaterialInstanceConstant* Instance, const FString& InSourceFilePath, const bool bWriteBackup)
	{
		if (!Instance)
		{
			return MakeProvenanceRefusal(FString(), LOCTEXT("DreamShaderProvenanceNoAsset", "DreamShader could not find the selected asset."));
		}

		const FString SourceFilePath = ::UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		if (!::UE::DreamShader::IsDreamShaderInstanceFile(SourceFilePath))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptInstanceNotDsi", "'{0}' is not a .dsi instance file, so the overrides of '{1}' cannot be spliced into it."),
				FText::FromString(SourceFilePath),
				FText::FromString(Instance->GetPathName())));
		}

		// The compile's own front half, stopped after validation: the parsed tree and the bound module the splice reads,
		// bound against the same parent schema a build uses. It compiles nothing, the parent included.
		FString RawText;
		::UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineResult Run;
		if (!RunProvenanceSourceCheck(SourceFilePath, RawText, Run))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptInstanceUnreadable", "'{0}' could not be read, so nothing was adopted."),
				FText::FromString(SourceFilePath)));
		}

		// Every span of the tree addresses the text the parser saw. With a directive in the file that text is the
		// preprocessed one, and a splice at its offsets would write the cut branches back as blank lines.
		if (!IsProvenanceSpliceTextExact(RawText, Run))
		{
			FDreamShaderTextError ConditionalError;
			FailWith(
				ConditionalError,
				TEXT("DSH8149"),
				FText::Format(
					LOCTEXT("DreamShaderAdoptInstanceConditionalSource", "DSH8149: '{0}' uses conditional compilation, so the overrides of '{1}' cannot be spliced into it without deleting the branches this build did not take; move the change into the source by hand."),
					FText::FromString(SourceFilePath),
					FText::FromString(Instance->GetPathName())));
			return MakeProvenanceRefusal(ConditionalError.Code, ConditionalError.Message);
		}

		// The pipeline's parent resolution raises DSH8260-8265 (unresolvable, missing, no such product, ambiguous, a cycle,
		// too deep). Any of them means the file's Parent names nothing a rewrite could be checked against.
		const ::UE::DreamShader::Lang::FLangDiagnostic* const ParentFailure = Run.Diagnostics.GetDiagnostics().FindByPredicate(
			[](const ::UE::DreamShader::Lang::FLangDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == ::UE::DreamShader::Lang::ELangSeverity::Error
					&& Diagnostic.Code.StartsWith(TEXT("DSH826"), ESearchCase::CaseSensitive);
			});
		if (ParentFailure)
		{
			FDreamShaderTextError ParentError;
			FailWith(
				ParentError,
				TEXT("DSH9103"),
				FText::Format(
					LOCTEXT("DreamShaderAdoptInstanceParentUnresolved", "DSH9103: The Parent of '{0}' no longer resolves, so the overrides of '{1}' cannot be written back into it: {2}"),
					FText::FromString(SourceFilePath),
					FText::FromString(Instance->GetPathName()),
					FText::FromString(::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(*ParentFailure, SourceFilePath))));
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("DreamShader adopt refused (%s): the Parent of '%s' no longer resolves."),
				*ParentError.Code,
				*SourceFilePath);
			return MakeProvenanceRefusal(ParentError.Code, ParentError.Message);
		}

		if (!Run.bSucceeded || !Run.Module.IsValid() || !Run.Bound.IsValid() || !Run.Bound->Instance.bIsInstance)
		{
			return MakeProvenanceRefusalFromSink(
				Run.Diagnostics,
				SourceFilePath,
				FText::Format(
					LOCTEXT("DreamShaderAdoptInstanceUnbound", "'{0}' does not check, so the overrides of '{1}' cannot be spliced into it; fix the file first."),
					FText::FromString(SourceFilePath),
					FText::FromString(Instance->GetPathName())));
		}

		if (IsGeneratedAssetOpenInEditor(Instance))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderAdoptOpenInEditor", "'{0}' is open in an asset editor, whose copy a decompile cannot see, so nothing was adopted; save and close the editor, then adopt again."),
				FText::FromString(Instance->GetPathName())));
		}

		// Every override the instance itself carries, equal to the parent or not: an override the author pinned to the
		// parent's value must stay pinned.
		FInstanceDecompileOptions DecompileOptions;
		DecompileOptions.Filter = EInstanceDecompileFilter::OverriddenOnly;
		DecompileOptions.TargetSourceFilePath = SourceFilePath;
		DecompileOptions.bPreferBareParentName = false;
		DecompileOptions.ParentSchema = Run.ParentSchema.Get();
		::UE::DreamShader::IR::FIRInstance Desired;
		::UE::DreamShader::Lang::FLangDiagnosticSink DecompileDiagnostics(SourceFilePath);
		const bool bDecompiled = DecompileMaterialInstance(Instance, DecompileOptions, Desired, DecompileDiagnostics);
		if (!bDecompiled || FindProvenanceFatalInstanceDiagnostic(DecompileDiagnostics))
		{
			return MakeProvenanceInstanceDecompileRefusal(DecompileDiagnostics, Instance, SourceFilePath);
		}

		// An unchanged parent keeps the author's spelling: the rewrite compares Desired's parent with the pragma and would
		// otherwise reprint it (FE report, contract change 6).
		const UMaterialInterface* const CurrentParent = Instance->Parent;
		if (CurrentParent && !Run.ParentObjectPath.IsEmpty() && CurrentParent->GetPathName().Equals(Run.ParentObjectPath, ESearchCase::IgnoreCase))
		{
			Desired.ParentReference = Run.Bound->Instance.ParentReference;
			Desired.ParentObjectPath = Run.ParentObjectPath;
		}

		TArray<::UE::DreamShader::Lang::FLangSourceEdit> Edits;
		FString NewText;
		::UE::DreamShader::Lang::FLangDiagnosticSink RewriteDiagnostics(SourceFilePath);
		const bool bRewritten = ::UE::DreamShader::Lang::RewriteDreamShaderInstanceSource(*Run.Source, *Run.Module, *Run.Bound, Desired, Edits, NewText, RewriteDiagnostics);
		if (!bRewritten || RewriteDiagnostics.HasErrors())
		{
			return MakeProvenanceRefusalFromSink(
				RewriteDiagnostics,
				SourceFilePath,
				FText::Format(
					LOCTEXT("DreamShaderAdoptInstanceRewriteRefused", "The overrides of '{0}' could not be spliced into '{1}', so nothing was written."),
					FText::FromString(Instance->GetPathName()),
					FText::FromString(SourceFilePath)));
		}

		FDreamShaderProvenanceOutcome Outcome;
		const bool bChanged = !NewText.Equals(RawText, ESearchCase::CaseSensitive);
		if (bChanged)
		{
			if (bWriteBackup && !BackUpProvenanceSourceFile(SourceFilePath, Outcome.BackupFilePath))
			{
				return MakeProvenanceRefusal(FString(), FText::Format(
					LOCTEXT("DreamShaderAdoptBackupFailed", "Could not back up '{0}' to '{1}'; nothing was written."),
					FText::FromString(SourceFilePath),
					FText::FromString(Outcome.BackupFilePath)));
			}
			if (!FFileHelper::SaveStringToFile(NewText, *SourceFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				return MakeProvenanceRefusal(FString(), FText::Format(
					LOCTEXT("DreamShaderProvenanceWriteFailed", "Could not write '{0}'."),
					FText::FromString(SourceFilePath)));
			}
			Outcome.WrittenFiles.Add(SourceFilePath);
		}
		Outcome.bSucceeded = true;

		// Rebuilt even when the text did not change: the instance is still diverged from the previous build's digest, and
		// a rebuild inside the revert scope is what makes the two agree again.
		{
			FScopedDreamShaderRevertDiverged RevertScope;
			const bool bPersisted = FPackageName::DoesPackageExist(Instance->GetOutermost()->GetName());
			Outcome.bCompiled = CompileSourceForProvenance(SourceFilePath, /*bAllowEphemeralThinCustom*/ !bPersisted, Outcome.CompileMessage);
		}

		Outcome.Message = bChanged
			? FText::Format(
				LOCTEXT("DreamShaderAdoptInstanceResult", "Wrote the overrides of '{0}' into '{1}' ({2} edit(s), backup: '{3}'). {4}"),
				FText::FromString(Instance->GetPathName()),
				FText::FromString(SourceFilePath),
				FText::AsNumber(Edits.Num()),
				Outcome.BackupFilePath.IsEmpty() ? LOCTEXT("DreamShaderAdoptNoBackup", "none") : FText::FromString(Outcome.BackupFilePath),
				FText::FromString(Outcome.CompileMessage))
			: FText::Format(
				LOCTEXT("DreamShaderAdoptInstanceUnchanged", "'{0}' already states every override of '{1}', so only the instance was rebuilt. {2}"),
				FText::FromString(SourceFilePath),
				FText::FromString(Instance->GetPathName()),
				FText::FromString(Outcome.CompileMessage));
		return Outcome;
	}

	void AdoptGeneratedAssetIntoSource(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		FString SourceFilePath;
		FString Error;
		if (!TryResolveGeneratedAssetSourceFile(AssetObject, SourceFilePath, Error))
		{
			ShowDreamShaderProvenanceNotification(FText::FromString(Error), SNotificationItem::CS_Fail);
			return;
		}

		const bool bInstanceSource = ::UE::DreamShader::IsDreamShaderInstanceFile(SourceFilePath);
		const bool bMigration = ::UE::DreamShader::IsDreamShaderMaterialFile(SourceFilePath) || ::UE::DreamShader::IsDreamShaderFunctionFile(SourceFilePath);

		// The dialog wants to say how many assets are rewritten. When the source does not get that far -- a directive, an
		// error, nothing declared -- the core is asked straight away instead: it refuses with the reason and touches
		// nothing, so there is nothing to confirm and no editor to close.
		FText ConfirmText;
		if (IsGeneratedInstanceTweaked(AssetObject))
		{
			ConfirmText = FText::GetEmpty();
		}
		else if (bInstanceSource)
		{
			ConfirmText = FText::Format(
				LOCTEXT("DreamShaderAdoptInstanceConfirm", "Write the parameter overrides and instance settings of '{0}' back into '{1}'?\n\nThe existing file is copied to '{2}' first. Only the declarations whose values changed are rewritten, so comments and the order of the file are kept."),
				FText::FromString(AssetObject->GetPathName()),
				FText::FromString(SourceFilePath),
				FText::FromString(SourceFilePath + TEXT(".bak")));
		}
		else
		{
			::UE::DreamShader::Editor::Compiler::FDreamShaderProductResolution Resolution;
			const bool bResolved = ::UE::DreamShader::Editor::Compiler::ResolveDreamShaderSourceProducts(SourceFilePath, Resolution);
			if (bResolved && !Resolution.bSourceHadPreprocessorDirectives && !Resolution.Products.IsEmpty())
			{
				ConfirmText = bMigration
					? FText::Format(
						LOCTEXT("DreamShaderAdoptMigrationConfirm", "Adopt '{0}' as a migration of '{1}' to 2.0?\n\nThe asset is decompiled into '{2}', and the 1.x source is moved to '{3}'. The new file is the decompiler's own form, so comments and formatting of the old file are not carried over."),
						FText::FromString(AssetObject->GetPathName()),
						FText::FromString(SourceFilePath),
						FText::FromString(FPaths::Combine(FPaths::GetPath(SourceFilePath), FPaths::GetBaseFilename(SourceFilePath) + TEXT(".dss"))),
						FText::FromString(SourceFilePath + TEXT(".bak")))
					: FText::Format(
						LOCTEXT("DreamShaderAdoptConfirmDss", "Rewrite '{0}' from the current contents of the {1} asset(s) it builds, '{2}' among them?\n\nThe existing source is copied to '{3}' first. The rewritten file is the decompiler's own form, so hand-written comments, helper functions and formatting in it are replaced."),
						FText::FromString(SourceFilePath),
						FText::AsNumber(Resolution.Products.Num()),
						FText::FromString(AssetObject->GetPathName()),
						FText::FromString(SourceFilePath + TEXT(".bak")));
			}
		}

		bool bEditorWasOpen = false;
		if (!ConfirmText.IsEmpty())
		{
			if (FMessageDialog::Open(EAppMsgType::YesNo, ConfirmText) != EAppReturnType::Yes)
			{
				return;
			}

			// Before the decompile, and that ordering is the point: what gets written into the source is "this asset's
			// current contents", and unapplied changes sitting in an open editor are not part of those contents until the
			// engine's save prompt has been answered.
			FString CloseError;
			if (!TryCloseAssetEditorsFor(AssetObject, bEditorWasOpen, CloseError))
			{
				ShowDreamShaderProvenanceNotification(FText::FromString(CloseError), SNotificationItem::CS_Fail);
				return;
			}
		}

		const FDreamShaderProvenanceOutcome Outcome = AdoptGeneratedAssetIntoSourceCore(AssetObject, SourceFilePath, /*bWriteBackup*/ true);

		ReopenAssetEditorFor(AssetObject, bEditorWasOpen);

		ShowDreamShaderProvenanceNotification(
			Outcome.Message,
			Outcome.bSucceeded && Outcome.bCompiled ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		// English under a localized editor, with the code as its own field when the refusal has one.
		const FString CodeNote = Outcome.Code.IsEmpty() ? FString() : FString::Printf(TEXT(" (%s)"), *Outcome.Code);
		const FString WireMessage = ToInvariantWireString(Outcome.Message);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader adopt of '%s' into '%s'%s: %s"),
			*AssetObject->GetPathName(),
			*SourceFilePath,
			*CodeNote,
			*WireMessage);
	}

	// --------------------------------------------------------------------------------------- Tweaks

	FDreamShaderProvenanceOutcome AdoptTweaksIntoSourceDefaultsCore(UMaterialInstance* Instance, const FString& InSourceFilePath, const bool bWriteBackup)
	{
		if (!Instance)
		{
			return MakeProvenanceRefusal(FString(), LOCTEXT("DreamShaderProvenanceNoAsset", "DreamShader could not find the selected asset."));
		}

		const FString SourceFilePath = ::UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		if (!IsGeneratedInstanceTweaked(Instance))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksNotTweaked", "'{0}' is not a generated instance that only carries parameter overrides, so there are no tweaks to write back."),
				FText::FromString(Instance->GetPathName())));
		}
		if (!::UE::DreamShader::IsDreamShaderLang2File(SourceFilePath))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksNeedDss", "'{0}' is not a .dss source, and tweaks are written into 2.0 uniform declarations; migrate the source first, or use Extract Tweaks to .dsi."),
				FText::FromString(SourceFilePath)));
		}

		FString RawText;
		::UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineResult Run;
		if (!RunProvenanceSourceCheck(SourceFilePath, RawText, Run))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksUnreadable", "'{0}' could not be read, so no tweak was written."),
				FText::FromString(SourceFilePath)));
		}
		if (!IsProvenanceSpliceTextExact(RawText, Run))
		{
			FDreamShaderTextError ConditionalError;
			FailWith(
				ConditionalError,
				TEXT("DSH8149"),
				FText::Format(
					LOCTEXT("DreamShaderTweaksConditionalSource", "DSH8149: '{0}' uses conditional compilation, so the tweaks of '{1}' cannot be spliced into its uniforms without deleting the branches this build did not take; set the defaults by hand."),
					FText::FromString(SourceFilePath),
					FText::FromString(Instance->GetPathName())));
			return MakeProvenanceRefusal(ConditionalError.Code, ConditionalError.Message);
		}
		if (!Run.bSucceeded || !Run.Module.IsValid() || !Run.Bound.IsValid())
		{
			return MakeProvenanceRefusalFromSink(
				Run.Diagnostics,
				SourceFilePath,
				FText::Format(
					LOCTEXT("DreamShaderTweaksSourceUnchecked", "'{0}' does not check, so the tweaks of '{1}' cannot be spliced into it; fix the source first."),
					FText::FromString(SourceFilePath),
					FText::FromString(Instance->GetPathName())));
		}

		if (IsGeneratedAssetOpenInEditor(Instance))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksOpenInEditor", "'{0}' is open in an asset editor, whose copy is not what a decompile reads, so no tweak was written; save and close the editor, then try again."),
				FText::FromString(Instance->GetPathName())));
		}

		// The overrides the instance itself carries. The parent is the hidden base material, whose parameters the schema is
		// read from (no ParentSchema: producer B inside the decompiler).
		FInstanceDecompileOptions DecompileOptions;
		DecompileOptions.Filter = EInstanceDecompileFilter::OverriddenOnly;
		DecompileOptions.TargetSourceFilePath = SourceFilePath;
		DecompileOptions.bPreferBareParentName = false;
		DecompileOptions.ParentSchema = nullptr;
		::UE::DreamShader::IR::FIRInstance Tweaks;
		::UE::DreamShader::Lang::FLangDiagnosticSink DecompileDiagnostics(SourceFilePath);
		const bool bDecompiled = DecompileMaterialInstance(Cast<UMaterialInstanceConstant>(Instance), DecompileOptions, Tweaks, DecompileDiagnostics);
		if (!bDecompiled || FindProvenanceFatalInstanceDiagnostic(DecompileDiagnostics))
		{
			return MakeProvenanceInstanceDecompileRefusal(DecompileDiagnostics, Instance, SourceFilePath);
		}
		if (Tweaks.Overrides.IsEmpty())
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksNone", "'{0}' carries no parameter override a source default can state, so nothing was written."),
				FText::FromString(Instance->GetPathName())));
		}

		TArray<::UE::DreamShader::Lang::FLangSourceEdit> Edits;
		FString NewText;
		::UE::DreamShader::Lang::FLangDiagnosticSink RewriteDiagnostics(SourceFilePath);
		const bool bRewritten = ::UE::DreamShader::Lang::RewriteDreamShaderUniformDefaults(*Run.Source, *Run.Module, *Run.Bound, Tweaks.Overrides, Edits, NewText, RewriteDiagnostics);
		if (!bRewritten || RewriteDiagnostics.HasErrors())
		{
			return MakeProvenanceRefusalFromSink(
				RewriteDiagnostics,
				SourceFilePath,
				FText::Format(
					LOCTEXT("DreamShaderTweaksRewriteRefused", "The tweaks of '{0}' could not be spliced into the uniforms of '{1}', so nothing was written."),
					FText::FromString(Instance->GetPathName()),
					FText::FromString(SourceFilePath)));
		}

		FDreamShaderProvenanceOutcome Outcome;
		if (!NewText.Equals(RawText, ESearchCase::CaseSensitive))
		{
			if (bWriteBackup && !BackUpProvenanceSourceFile(SourceFilePath, Outcome.BackupFilePath))
			{
				return MakeProvenanceRefusal(FString(), FText::Format(
					LOCTEXT("DreamShaderAdoptBackupFailed", "Could not back up '{0}' to '{1}'; nothing was written."),
					FText::FromString(SourceFilePath),
					FText::FromString(Outcome.BackupFilePath)));
			}
			if (!FFileHelper::SaveStringToFile(NewText, *SourceFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				return MakeProvenanceRefusal(FString(), FText::Format(
					LOCTEXT("DreamShaderProvenanceWriteFailed", "Could not write '{0}'."),
					FText::FromString(SourceFilePath)));
			}
			Outcome.WrittenFiles.Add(SourceFilePath);
		}
		Outcome.bSucceeded = true;

		// Cleared BEFORE the rebuild, and that order is the point: the ThinCustom rebuild captures every override on the
		// instance and puts it back afterwards (T3-B), so a tweak still on the instance would sit on top of the new
		// default it just became. After the source write, so a failed write leaves the tweaks where they were.
		ClearProvenanceInstanceTweaks(Instance);
		const bool bPersisted = FPackageName::DoesPackageExist(Instance->GetOutermost()->GetName());
		Outcome.bCompiled = CompileSourceForProvenance(SourceFilePath, /*bAllowEphemeralThinCustom*/ !bPersisted, Outcome.CompileMessage);

		Outcome.Message = FText::Format(
			LOCTEXT("DreamShaderTweaksAdoptResult", "Wrote {0} tweak(s) of '{1}' into '{2}' as uniform defaults and cleared them from the instance (backup: '{3}'). {4}"),
			FText::AsNumber(Tweaks.Overrides.Num()),
			FText::FromString(Instance->GetPathName()),
			FText::FromString(SourceFilePath),
			Outcome.BackupFilePath.IsEmpty() ? LOCTEXT("DreamShaderAdoptNoBackup", "none") : FText::FromString(Outcome.BackupFilePath),
			FText::FromString(Outcome.CompileMessage));
		return Outcome;
	}

	FDreamShaderProvenanceOutcome ExtractTweaksToInstanceSourceCore(UMaterialInstance* Instance, const FString& InTargetFilePath)
	{
		if (!Instance)
		{
			return MakeProvenanceRefusal(FString(), LOCTEXT("DreamShaderProvenanceNoAsset", "DreamShader could not find the selected asset."));
		}
		if (!IsGeneratedInstanceTweaked(Instance))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksNotTweaked", "'{0}' is not a generated instance that only carries parameter overrides, so there are no tweaks to write back."),
				FText::FromString(Instance->GetPathName())));
		}

		FString SourceFilePath;
		FString ResolveError;
		if (!TryResolveGeneratedAssetSourceFile(Instance, SourceFilePath, ResolveError))
		{
			return MakeProvenanceRefusal(FString(), FText::FromString(ResolveError));
		}

		const FString TargetFilePath = ::UE::DreamShader::NormalizeSourceFilePath(
			InTargetFilePath.IsEmpty() ? MakeDefaultTweaksInstanceSourcePath(Instance, SourceFilePath) : InTargetFilePath);
		if (!::UE::DreamShader::IsDreamShaderInstanceFile(TargetFilePath))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksTargetNotDsi", "'{0}' is not a .dsi file name, so the tweaks were not extracted."),
				FText::FromString(TargetFilePath)));
		}
		const ::UE::DreamShader::FDreamShaderSourceRoot* const TargetRoot = ::UE::DreamShader::FindSourceRootForFile(TargetFilePath);
		if (!TargetRoot || !TargetRoot->bWritable)
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksTargetReadOnly", "'{0}' is not under a writable source root, where the watcher would find it, so the tweaks were not extracted."),
				FText::FromString(TargetFilePath)));
		}
		if (IFileManager::Get().FileExists(*TargetFilePath))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksTargetExists", "'{0}' already exists, so the tweaks were not extracted into it."),
				FText::FromString(TargetFilePath)));
		}

		FInstanceDecompileOptions DecompileOptions;
		DecompileOptions.Filter = EInstanceDecompileFilter::OverriddenOnly;
		DecompileOptions.TargetSourceFilePath = TargetFilePath;
		DecompileOptions.bPreferBareParentName = false;
		DecompileOptions.ParentSchema = nullptr;
		::UE::DreamShader::IR::FIRInstance Payload;
		::UE::DreamShader::Lang::FLangDiagnosticSink DecompileDiagnostics(TargetFilePath);
		const bool bDecompiled = DecompileMaterialInstance(Cast<UMaterialInstanceConstant>(Instance), DecompileOptions, Payload, DecompileDiagnostics);
		if (!bDecompiled || FindProvenanceFatalInstanceDiagnostic(DecompileDiagnostics))
		{
			return MakeProvenanceInstanceDecompileRefusal(DecompileDiagnostics, Instance, TargetFilePath);
		}
		if (Payload.Overrides.IsEmpty())
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderTweaksNone", "'{0}' carries no parameter override a source default can state, so nothing was written."),
				FText::FromString(Instance->GetPathName())));
		}

		// The decompile named the instance's own parent (the hidden base). The new file's parent is the instance itself,
		// and a tweak is a parameter override only: instance settings on a ThinCustom pair would read as a hand edit.
		Payload.ParentReference = MakeProvenanceParentReference(Instance);
		Payload.ParentObjectPath = Instance->GetPathName();
		Payload.Settings.Reset();

		const FString Text = ::UE::DreamShader::Lang::PrintDreamShaderInstance(Payload, TargetFilePath, FString());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetFilePath), /*Tree*/ true);
		if (!FFileHelper::SaveStringToFile(Text, *TargetFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return MakeProvenanceRefusal(FString(), FText::Format(
				LOCTEXT("DreamShaderProvenanceWriteFailed", "Could not write '{0}'."),
				FText::FromString(TargetFilePath)));
		}

		FDreamShaderProvenanceOutcome Outcome;
		Outcome.WrittenFiles.Add(TargetFilePath);

		// The instance's path, not the pointer: compiling a `.dsi` whose parent is memory-only materializes the parent
		// first (DSH8244), which rebuilds it.
		const FString InstanceObjectPath = Instance->GetPathName();
		Outcome.bCompiled = CompileSourceForProvenance(TargetFilePath, /*bAllowEphemeralThinCustom*/ true, Outcome.CompileMessage);
		if (!Outcome.bCompiled)
		{
			// The file stays for the user to fix; the tweaks stay on the instance, so nothing is lost either way.
			Outcome.Message = FText::Format(
				LOCTEXT("DreamShaderTweaksExtractCompileFailed", "Wrote '{0}', but it did not compile, so the tweaks stay on '{1}'. {2}"),
				FText::FromString(TargetFilePath),
				FText::FromString(InstanceObjectPath),
				FText::FromString(Outcome.CompileMessage));
			return Outcome;
		}

		FText SaveNote = FText::GetEmpty();
		if (UMaterialInstance* const LiveInstance = FindObject<UMaterialInstance>(nullptr, *InstanceObjectPath))
		{
			ClearProvenanceInstanceTweaks(LiveInstance);
			if (FPackageName::DoesPackageExist(LiveInstance->GetOutermost()->GetName()))
			{
				FDreamShaderError SaveError;
				if (!SaveAssetPackage(LiveInstance, SaveError))
				{
					SaveNote = FText::Format(
						LOCTEXT("DreamShaderTweaksExtractSaveFailed", " Clearing the tweaks could not be saved: {0}"),
						FText::FromString(SaveError.Message));
				}
			}
		}
		Outcome.bSucceeded = true;
		Outcome.Message = FText::Format(
			LOCTEXT("DreamShaderTweaksExtractResult", "Extracted {0} tweak(s) of '{1}' into '{2}' and cleared them from the instance; anything that uses '{1}' shows the untuned material until it is pointed at the new instance.{3} {4}"),
			FText::AsNumber(Payload.Overrides.Num()),
			FText::FromString(InstanceObjectPath),
			FText::FromString(TargetFilePath),
			SaveNote,
			FText::FromString(Outcome.CompileMessage));
		return Outcome;
	}

	void AdoptTweaksIntoSourceDefaults(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		FString SourceFilePath;
		FString Error;
		if (!TryResolveGeneratedAssetSourceFile(AssetObject, SourceFilePath, Error))
		{
			ShowDreamShaderProvenanceNotification(FText::FromString(Error), SNotificationItem::CS_Fail);
			return;
		}

		if (FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
					LOCTEXT("DreamShaderTweaksAdoptConfirm", "Write the parameter overrides of '{0}' into '{1}' as the defaults of its uniforms?\n\nThe existing source is copied to '{2}' first, only the initializers and @default values that change are rewritten, and the overrides are then cleared from the instance."),
					FText::FromString(AssetObject->GetPathName()),
					FText::FromString(SourceFilePath),
					FText::FromString(SourceFilePath + TEXT(".bak")))) != EAppReturnType::Yes)
		{
			return;
		}

		bool bEditorWasOpen = false;
		FString CloseError;
		if (!TryCloseAssetEditorsFor(AssetObject, bEditorWasOpen, CloseError))
		{
			ShowDreamShaderProvenanceNotification(FText::FromString(CloseError), SNotificationItem::CS_Fail);
			return;
		}

		const FDreamShaderProvenanceOutcome Outcome = AdoptTweaksIntoSourceDefaultsCore(Cast<UMaterialInstance>(AssetObject), SourceFilePath, /*bWriteBackup*/ true);

		ReopenAssetEditorFor(AssetObject, bEditorWasOpen);

		ShowDreamShaderProvenanceNotification(
			Outcome.Message,
			Outcome.bSucceeded && Outcome.bCompiled ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader adopt tweaks of '%s' into '%s': %s"),
			*AssetObject->GetPathName(),
			*SourceFilePath,
			*ToInvariantWireString(Outcome.Message));
	}

	void ExtractTweaksToInstanceSource(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		UMaterialInstance* const Instance = Cast<UMaterialInstance>(AssetObject);
		FString SourceFilePath;
		FString Error;
		if (!Instance || !TryResolveGeneratedAssetSourceFile(AssetObject, SourceFilePath, Error))
		{
			ShowDreamShaderProvenanceNotification(
				Error.IsEmpty() ? LOCTEXT("DreamShaderProvenanceNoAsset", "DreamShader could not find the selected asset.") : FText::FromString(Error),
				SNotificationItem::CS_Fail);
			return;
		}

		const FString TargetFilePath = MakeDefaultTweaksInstanceSourcePath(Instance, SourceFilePath);
		if (FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
					LOCTEXT("DreamShaderTweaksExtractConfirm", "Create '{0}' holding the parameter overrides of '{1}', compile it, and clear the overrides from '{1}'?\n\nMeshes and materials that use '{1}' lose the tuned look until you point them at the new instance."),
					FText::FromString(TargetFilePath),
					FText::FromString(AssetObject->GetPathName()))) != EAppReturnType::Yes)
		{
			return;
		}

		bool bEditorWasOpen = false;
		FString CloseError;
		if (!TryCloseAssetEditorsFor(AssetObject, bEditorWasOpen, CloseError))
		{
			ShowDreamShaderProvenanceNotification(FText::FromString(CloseError), SNotificationItem::CS_Fail);
			return;
		}

		const FDreamShaderProvenanceOutcome Outcome = ExtractTweaksToInstanceSourceCore(Instance, TargetFilePath);

		ReopenAssetEditorFor(AssetObject, bEditorWasOpen);

		ShowDreamShaderProvenanceNotification(
			Outcome.Message,
			Outcome.bSucceeded ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader extract tweaks of '%s' into '%s': %s"),
			*AssetObject->GetPathName(),
			*TargetFilePath,
			*ToInvariantWireString(Outcome.Message));
	}

	// ---------------------------------------------------------------------------------------- Detach

	void DetachGeneratedAssetFromDreamShader(TWeakObjectPtr<UObject> Asset)
	{
		UObject* AssetObject = Asset.Get();
		if (!AssetObject)
		{
			ShowDreamShaderProvenanceNotification(
				LOCTEXT("DreamShaderDetachNoAsset", "DreamShader could not find the selected asset."),
				SNotificationItem::CS_Fail);
			return;
		}

		if (!HasDreamShaderSourceMetadata(AssetObject))
		{
			ShowDreamShaderProvenanceNotification(
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

		ShowDreamShaderProvenanceNotification(
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
