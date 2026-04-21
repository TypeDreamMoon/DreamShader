#include "DreamShaderEditorBridge.h"

#include "DreamShaderMaterialGenerator.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"

#include "Async/Async.h"
#include "DirectoryWatcherModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Commands/UIAction.h"
#include "HAL/FileManager.h"
#include "IDirectoryWatcher.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "DreamShaderEditorBridge"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		bool IsPathUnderDirectory(const FString& InPath, const FString& InDirectory)
		{
			const FString Path = UE::DreamShader::NormalizeSourceFilePath(InPath);
			FString Directory = UE::DreamShader::NormalizeSourceFilePath(InDirectory);
			Directory.RemoveFromEnd(TEXT("/"));

			return Path.Equals(Directory, ESearchCase::IgnoreCase)
				|| Path.StartsWith(Directory + TEXT("/"), ESearchCase::IgnoreCase);
		}

		bool IsPackageMaterialFile(const FString& InPath)
		{
			return UE::DreamShader::IsDreamShaderMaterialFile(InPath)
				&& IsPathUnderDirectory(InPath, UE::DreamShader::GetPackageShaderDirectory());
		}

		bool TryExtractImportPathFromLine(const FString& Line, FString& OutPath)
		{
			FString TrimmedLine = Line.TrimStartAndEnd();
			if (TrimmedLine.StartsWith(TEXT("//"))
				|| !TrimmedLine.StartsWith(TEXT("import"), ESearchCase::CaseSensitive))
			{
				return false;
			}

			TrimmedLine.RightChopInline(6, EAllowShrinking::No);
			TrimmedLine.TrimStartInline();
			if (TrimmedLine.Len() < 2 || (TrimmedLine[0] != TCHAR('"') && TrimmedLine[0] != TCHAR('\'')))
			{
				return false;
			}

			const TCHAR Quote = TrimmedLine[0];
			int32 ClosingQuoteIndex = INDEX_NONE;
			for (int32 Index = 1; Index < TrimmedLine.Len(); ++Index)
			{
				if (TrimmedLine[Index] == Quote)
				{
					ClosingQuoteIndex = Index;
					break;
				}
			}

			if (ClosingQuoteIndex == INDEX_NONE)
			{
				return false;
			}

			OutPath = TrimmedLine.Mid(1, ClosingQuoteIndex - 1).TrimStartAndEnd();
			return !OutPath.IsEmpty();
		}

		FString NormalizeDreamShaderImportSpecifier(const FString& ImportSpecifier)
		{
			FString Normalized = ImportSpecifier.TrimStartAndEnd();
			Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (Normalized.StartsWith(TEXT("./")))
			{
				Normalized.RightChopInline(2, EAllowShrinking::No);
			}

			if (FPaths::GetExtension(Normalized, true).IsEmpty())
			{
				Normalized += TEXT(".dsh");
			}

			return Normalized;
		}

		bool ResolveDreamShaderImportPath(const FString& CurrentFilePath, const FString& ImportSpecifier, FString& OutResolvedPath)
		{
			const FString NormalizedImport = NormalizeDreamShaderImportSpecifier(ImportSpecifier);
			if (NormalizedImport.IsEmpty())
			{
				return false;
			}

			const TArray<FString> Candidates =
			{
				FPaths::Combine(FPaths::GetPath(CurrentFilePath), NormalizedImport),
				FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), NormalizedImport),
				FPaths::Combine(UE::DreamShader::GetPackageShaderDirectory(), NormalizedImport),
				FPaths::Combine(UE::DreamShader::GetBuiltinShaderLibraryDirectory(), NormalizedImport)
			};

			for (const FString& Candidate : Candidates)
			{
				const FString NormalizedCandidate = UE::DreamShader::NormalizeSourceFilePath(Candidate);
				if (IFileManager::Get().FileExists(*NormalizedCandidate))
				{
					OutResolvedPath = NormalizedCandidate;
					return true;
				}
			}

			return false;
		}

		void FindProjectMaterialSourceFiles(TArray<FString>& OutSourceFiles)
		{
			IFileManager::Get().FindFilesRecursive(
				OutSourceFiles,
				*UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("*.dsm"),
				true,
				false,
				false);

			for (FString& SourceFile : OutSourceFiles)
			{
				SourceFile = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			}

			OutSourceFiles.RemoveAll([](const FString& SourceFile)
			{
				return IsPackageMaterialFile(SourceFile);
			});
		}

		void CollectHeaderDependenciesRecursive(
			const FString& SourceFilePath,
			TSet<FString>& OutHeaders,
			TSet<FString>& InOutVisitedFiles)
		{
			const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
			if (InOutVisitedFiles.Contains(NormalizedPath))
			{
				return;
			}
			InOutVisitedFiles.Add(NormalizedPath);

			FString SourceText;
			if (!FFileHelper::LoadFileToString(SourceText, *NormalizedPath))
			{
				return;
			}

			TArray<FString> Lines;
			SourceText.ParseIntoArrayLines(Lines, false);
			for (const FString& Line : Lines)
			{
				FString ImportPath;
				if (!TryExtractImportPathFromLine(Line, ImportPath))
				{
					continue;
				}

				FString ResolvedImportPath;
				if (!ResolveDreamShaderImportPath(NormalizedPath, ImportPath, ResolvedImportPath))
				{
					continue;
				}

				if (UE::DreamShader::IsDreamShaderHeaderFile(ResolvedImportPath))
				{
					OutHeaders.Add(ResolvedImportPath);
				}

				CollectHeaderDependenciesRecursive(ResolvedImportPath, OutHeaders, InOutVisitedFiles);
			}
		}

		bool TryParseErrorLocation(
			const FString& Line,
			FString& OutFilePath,
			int32& OutLine,
			int32& OutColumn,
			FString& OutMessage)
		{
			const int32 CloseMarkerIndex = Line.Find(TEXT("): "));
			if (CloseMarkerIndex == INDEX_NONE)
			{
				return false;
			}

			const int32 OpenMarkerIndex = Line.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromEnd, CloseMarkerIndex);
			if (OpenMarkerIndex == INDEX_NONE || OpenMarkerIndex >= CloseMarkerIndex)
			{
				return false;
			}

			const FString LocationText = Line.Mid(OpenMarkerIndex + 1, CloseMarkerIndex - OpenMarkerIndex - 1);
			FString LineText;
			FString ColumnText;
			if (!LocationText.Split(TEXT(","), &LineText, &ColumnText))
			{
				return false;
			}

			LineText.TrimStartAndEndInline();
			ColumnText.TrimStartAndEndInline();
			if (!LineText.IsNumeric() || !ColumnText.IsNumeric())
			{
				return false;
			}

			OutLine = FMath::Max(1, FCString::Atoi(*LineText));
			OutColumn = FMath::Max(1, FCString::Atoi(*ColumnText));

			OutFilePath = UE::DreamShader::NormalizeSourceFilePath(Line.Left(OpenMarkerIndex));
			OutMessage = Line.Mid(CloseMarkerIndex + 3).TrimStartAndEnd();
			return !OutFilePath.IsEmpty() && !OutMessage.IsEmpty();
		}

		void AddDiagnosticJson(TArray<TSharedPtr<FJsonValue>>& OutDiagnostics, const FDreamShaderEditorBridge::FDiagnosticRecord& Diagnostic)
		{
			TSharedRef<FJsonObject> DiagnosticObject = MakeShared<FJsonObject>();
			DiagnosticObject->SetStringField(TEXT("message"), Diagnostic.Message);
			DiagnosticObject->SetNumberField(TEXT("line"), Diagnostic.Line);
			DiagnosticObject->SetNumberField(TEXT("column"), Diagnostic.Column);
			DiagnosticObject->SetStringField(TEXT("severity"), Diagnostic.Severity);
			DiagnosticObject->SetStringField(TEXT("source"), Diagnostic.Source);
			OutDiagnostics.Add(MakeShared<FJsonValueObject>(DiagnosticObject));
		}
	}

	FString FDreamShaderEditorBridge::GetBridgeDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Bridge"));
	}

	FString FDreamShaderEditorBridge::GetRequestDirectory()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("Requests"));
	}

	FString FDreamShaderEditorBridge::GetDiagnosticsFilePath()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("diagnostics.json"));
	}

	FString FDreamShaderEditorBridge::GetSourceFileMetadata(UObject* Asset)
	{
		if (!Asset)
		{
			return FString();
		}

		if (UPackage* Package = Asset->GetOutermost())
		{
			return Package->GetMetaData().GetValue(Asset, TEXT("DreamShader.SourceFile"));
		}

		return FString();
	}

	void FDreamShaderEditorBridge::Startup()
	{
		IFileManager::Get().MakeDirectory(*GetBridgeDirectory(), true);
		IFileManager::Get().MakeDirectory(*GetRequestDirectory(), true);

		QueueFullScan();
		UpdateDiagnosticsFile();

		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				UE::DreamShader::GetSourceShaderDirectory(),
				IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FDreamShaderEditorBridge::OnDirectoryChanged),
				DirectoryWatcherHandle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
		}

		MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddRaw(
			this,
			&FDreamShaderEditorBridge::OnMaterialCompilationFinished);

		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDreamShaderEditorBridge::RegisterMenus));

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FDreamShaderEditorBridge::Tick),
			0.1f);
	}

	void FDreamShaderEditorBridge::Shutdown()
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}

		if (MaterialCompilationFinishedHandle.IsValid())
		{
			UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
			MaterialCompilationFinishedHandle.Reset();
		}

		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);

		if (DirectoryWatcherHandle.IsValid())
		{
			if (FDirectoryWatcherModule* DirectoryWatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
			{
				if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule->Get())
				{
					DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(UE::DreamShader::GetSourceShaderDirectory(), DirectoryWatcherHandle);
				}
			}

			DirectoryWatcherHandle.Reset();
		}

		PendingFiles.Reset();
		DiagnosticsByFile.Reset();
	}

	void FDreamShaderEditorBridge::QueueFullScan()
	{
		TArray<FString> SourceFiles;
		FindProjectMaterialSourceFiles(SourceFiles);
		RebuildDependencyGraph();

		const double Now = FPlatformTime::Seconds();
		for (FString& SourceFile : SourceFiles)
		{
			PendingFiles.Add(UE::DreamShader::NormalizeSourceFilePath(SourceFile), Now);
		}
	}

	void FDreamShaderEditorBridge::QueueSourceFile(const FString& SourceFilePath)
	{
		PendingFiles.Add(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath), FPlatformTime::Seconds());
	}

	void FDreamShaderEditorBridge::QueueDependentMaterialsForHeader(const FString& HeaderFilePath)
	{
		const FString NormalizedHeaderPath = UE::DreamShader::NormalizeSourceFilePath(HeaderFilePath);
		TSet<FString> MaterialsToQueue;

		if (const TSet<FString>* ExistingDependents = HeaderDependentsByFile.Find(NormalizedHeaderPath))
		{
			for (const FString& Dependent : *ExistingDependents)
			{
				MaterialsToQueue.Add(Dependent);
			}
		}

		RebuildDependencyGraph();

		if (const TSet<FString>* RebuiltDependents = HeaderDependentsByFile.Find(NormalizedHeaderPath))
		{
			for (const FString& Dependent : *RebuiltDependents)
			{
				MaterialsToQueue.Add(Dependent);
			}
		}

		const double Now = FPlatformTime::Seconds();
		for (const FString& MaterialFile : MaterialsToQueue)
		{
			PendingFiles.Add(MaterialFile, Now);
		}

		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		if (Settings && Settings->bVerboseLogs)
		{
			UE_LOG(
				LogDreamShader,
				Display,
				TEXT("DreamShader queued %d dependent .dsm file(s) for header '%s'."),
				MaterialsToQueue.Num(),
				*NormalizedHeaderPath);
		}
	}

	void FDreamShaderEditorBridge::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
	{
		TArray<FFileChangeData> ChangesCopy = FileChanges;
		AsyncTask(ENamedThreads::GameThread, [this, Changes = MoveTemp(ChangesCopy)]()
		{
			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
			if (Settings && !Settings->bAutoCompileOnSave)
			{
				return;
			}

			for (const FFileChangeData& FileChange : Changes)
			{
				if (FileChange.Action == FFileChangeData::FCA_RescanRequired)
				{
					QueueFullScan();
					continue;
				}

				if (!UE::DreamShader::IsDreamShaderSourceFile(FileChange.Filename))
				{
					continue;
				}

				if (FileChange.Action == FFileChangeData::FCA_Added || FileChange.Action == FFileChangeData::FCA_Modified)
				{
					if (UE::DreamShader::IsDreamShaderHeaderFile(FileChange.Filename))
					{
						QueueDependentMaterialsForHeader(FileChange.Filename);
					}
					else if (IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						RebuildDependencyGraph();
						QueueSourceFile(FileChange.Filename);
					}
				}
				else if (FileChange.Action == FFileChangeData::FCA_Removed)
				{
					const FString SourceFile = UE::DreamShader::NormalizeSourceFilePath(FileChange.Filename);
					if (UE::DreamShader::IsDreamShaderHeaderFile(FileChange.Filename))
					{
						QueueDependentMaterialsForHeader(FileChange.Filename);
					}
					else if (IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						PendingFiles.Remove(SourceFile);
						ClearDiagnosticsForSourceAndDependencies(SourceFile);
						RebuildDependencyGraph();
						UpdateDiagnosticsFile();
					}
					UE_LOG(LogDreamShader, Display, TEXT("DreamShader source removed, existing generated assets were left untouched: %s"), *FileChange.Filename);
				}
			}
		});
	}

	bool FDreamShaderEditorBridge::Tick(float DeltaSeconds)
	{
		(void)DeltaSeconds;

		ProcessRequestFiles();
		ProcessReadyFiles();
		return true;
	}

	void FDreamShaderEditorBridge::ProcessRequestFiles()
	{
		TArray<FString> RequestFiles;
		IFileManager::Get().FindFiles(RequestFiles, *FPaths::Combine(GetRequestDirectory(), TEXT("*.json")), true, false);

		for (const FString& RequestFileName : RequestFiles)
		{
			const FString RequestPath = FPaths::Combine(GetRequestDirectory(), RequestFileName);

			FString RequestText;
			if (!FFileHelper::LoadFileToString(RequestText, *RequestPath))
			{
				IFileManager::Get().Delete(*RequestPath);
				continue;
			}

			TSharedPtr<FJsonObject> RequestObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestText);
			if (FJsonSerializer::Deserialize(Reader, RequestObject) && RequestObject.IsValid())
			{
				FString Action;
				FString Scope;
				RequestObject->TryGetStringField(TEXT("action"), Action);
				RequestObject->TryGetStringField(TEXT("scope"), Scope);
				if (Action.Equals(TEXT("recompile"), ESearchCase::IgnoreCase))
				{
					if (Scope.Equals(TEXT("all"), ESearchCase::IgnoreCase))
					{
						RequestRecompileAll();
					}
					else if (Scope.Equals(TEXT("file"), ESearchCase::IgnoreCase))
					{
						FString SourceFilePath;
						if (RequestObject->TryGetStringField(TEXT("sourceFile"), SourceFilePath) && !SourceFilePath.IsEmpty())
						{
							QueueSourceFile(SourceFilePath);
						}
					}
				}
			}

			IFileManager::Get().Delete(*RequestPath);
		}
	}

	void FDreamShaderEditorBridge::ProcessReadyFiles()
	{
		const double Now = FPlatformTime::Seconds();
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		const double SaveDebounceSeconds = Settings ? FMath::Clamp(static_cast<double>(Settings->SaveDebounceSeconds), 0.05, 10.0) : 0.25;
		TArray<FString> ReadyFiles;
		for (const TPair<FString, double>& PendingFile : PendingFiles)
		{
			if (Now - PendingFile.Value >= SaveDebounceSeconds)
			{
				ReadyFiles.Add(PendingFile.Key);
			}
		}

		for (const FString& ReadyFile : ReadyFiles)
		{
			PendingFiles.Remove(ReadyFile);
			if (IFileManager::Get().FileExists(*ReadyFile))
			{
				ProcessSourceFile(ReadyFile);
			}
		}
	}

	void FDreamShaderEditorBridge::ProcessSourceFile(const FString& SourceFilePath)
	{
		FString Message;
		if (FMaterialGenerator::GenerateAssetsFromFile(SourceFilePath, Message))
		{
			ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
			UpdateDiagnosticsFile();
			UE_LOG(LogDreamShader, Display, TEXT("%s"), *Message);
			return;
		}

		TArray<FDiagnosticRecord> Diagnostics = BuildErrorDiagnostics(SourceFilePath, Message);
		ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
		UE_LOG(LogDreamShader, Error, TEXT("%s"), *Message);
	}

	void FDreamShaderEditorBridge::OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface)
	{
		UMaterial* Material = Cast<UMaterial>(MaterialInterface);
		if (!Material)
		{
			return;
		}

		const FString SourceFilePath = GetSourceFileMetadata(Material);
		if (SourceFilePath.IsEmpty())
		{
			return;
		}

		TArray<FDiagnosticRecord> Diagnostics;
		if (const FMaterialResource* MaterialResource = Material->GetMaterialResource(GMaxRHIShaderPlatform))
		{
			for (const FString& Error : MaterialResource->GetCompileErrors())
			{
				FDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
				Diagnostic.Message = Error;
			}
		}

		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
	}

	void FDreamShaderEditorBridge::RegisterMenus()
	{
		FToolMenuOwnerScoped MenuOwner(this);

		if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddMenuEntry(
				TEXT("DreamShader.RecompileAll"),
				LOCTEXT("DreamShaderRecompileLabel", "Recompile DSM"),
				LOCTEXT("DreamShaderRecompileTooltip", "Recompile all DreamShader .dsm files and refresh diagnostics."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
				FUIAction(FExecuteAction::CreateRaw(this, &FDreamShaderEditorBridge::RequestRecompileAll)));
		}

		if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar")))
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("DreamShader.RecompileAllToolbar"),
				FUIAction(FExecuteAction::CreateRaw(this, &FDreamShaderEditorBridge::RequestRecompileAll)),
				LOCTEXT("DreamShaderRecompileToolbarLabel", "DSM"),
				LOCTEXT("DreamShaderRecompileToolbarTooltip", "Recompile all DreamShader .dsm files."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh"))));
		}
	}

	void FDreamShaderEditorBridge::RequestRecompileAll()
	{
		QueueFullScan();
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader queued a full .dsm recompile scan."));
	}

	void FDreamShaderEditorBridge::RebuildDependencyGraph()
	{
		HeaderDependentsByFile.Reset();

		TArray<FString> MaterialFiles;
		FindProjectMaterialSourceFiles(MaterialFiles);
		for (const FString& MaterialFile : MaterialFiles)
		{
			TSet<FString> Dependencies;
			TSet<FString> VisitedFiles;
			CollectHeaderDependenciesRecursive(MaterialFile, Dependencies, VisitedFiles);
			for (const FString& HeaderFile : Dependencies)
			{
				HeaderDependentsByFile.FindOrAdd(HeaderFile).Add(MaterialFile);
			}
		}
	}

	void FDreamShaderEditorBridge::SetDiagnostics(const FString& SourceFilePath, TArray<FDiagnosticRecord>&& Diagnostics)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		DiagnosticsByFile.Remove(NormalizedPath);
		if (Diagnostics.IsEmpty())
		{
			return;
		}

		for (FDiagnosticRecord& Diagnostic : Diagnostics)
		{
			const FString DiagnosticFilePath = Diagnostic.FilePath.IsEmpty()
				? NormalizedPath
				: UE::DreamShader::NormalizeSourceFilePath(Diagnostic.FilePath);
			Diagnostic.FilePath.Reset();
			DiagnosticsByFile.FindOrAdd(DiagnosticFilePath).Add(MoveTemp(Diagnostic));
		}
	}

	void FDreamShaderEditorBridge::ClearDiagnostics(const FString& SourceFilePath)
	{
		DiagnosticsByFile.Remove(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
	}

	void FDreamShaderEditorBridge::ClearDiagnosticsForSourceAndDependencies(const FString& SourceFilePath)
	{
		ClearDiagnostics(SourceFilePath);

		TSet<FString> Dependencies;
		TSet<FString> VisitedFiles;
		CollectHeaderDependenciesRecursive(SourceFilePath, Dependencies, VisitedFiles);
		for (const FString& HeaderFile : Dependencies)
		{
			ClearDiagnostics(HeaderFile);
		}
	}

	void FDreamShaderEditorBridge::UpdateDiagnosticsFile() const
	{
		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetNumberField(TEXT("version"), 1);
		RootObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());

		TArray<TSharedPtr<FJsonValue>> FileEntries;
		for (const TPair<FString, TArray<FDiagnosticRecord>>& Pair : DiagnosticsByFile)
		{
			TSharedRef<FJsonObject> FileObject = MakeShared<FJsonObject>();
			FileObject->SetStringField(TEXT("path"), Pair.Key);

			TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
			for (const FDiagnosticRecord& Diagnostic : Pair.Value)
			{
				AddDiagnosticJson(DiagnosticValues, Diagnostic);
			}

			FileObject->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
			FileEntries.Add(MakeShared<FJsonValueObject>(FileObject));
		}

		RootObject->SetArrayField(TEXT("files"), FileEntries);

		FString OutputText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputText);
		FJsonSerializer::Serialize(RootObject, Writer);
		FFileHelper::SaveStringToFile(OutputText, *GetDiagnosticsFilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	TArray<FDreamShaderEditorBridge::FDiagnosticRecord> FDreamShaderEditorBridge::BuildErrorDiagnostics(
		const FString& SourceFilePath,
		const FString& ErrorMessage) const
	{
		TArray<FDiagnosticRecord> Diagnostics;

		TArray<FString> Lines;
		ErrorMessage.ParseIntoArrayLines(Lines, false);
		if (Lines.IsEmpty())
		{
			Lines.Add(ErrorMessage);
		}

		const FString PathPrefix = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath) + TEXT(": ");
		for (FString Line : Lines)
		{
			FString DiagnosticFilePath;
			int32 DiagnosticLine = 1;
			int32 DiagnosticColumn = 1;
			FString DiagnosticMessage;
			if (TryParseErrorLocation(Line, DiagnosticFilePath, DiagnosticLine, DiagnosticColumn, DiagnosticMessage))
			{
				FDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
				Diagnostic.FilePath = DiagnosticFilePath;
				Diagnostic.Message = DiagnosticMessage;
				Diagnostic.Line = DiagnosticLine;
				Diagnostic.Column = DiagnosticColumn;
				continue;
			}

			if (Line.StartsWith(PathPrefix))
			{
				Line.RightChopInline(PathPrefix.Len(), EAllowShrinking::No);
			}

			FDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Message = Line;
		}

		return Diagnostics;
	}
}

#undef LOCTEXT_NAMESPACE
