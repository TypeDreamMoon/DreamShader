#include "DreamShaderEditorBridge.h"

#include "DreamShaderMaterialGenerator.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"

#include "Async/Async.h"
#include "CoreGlobals.h"
#include "ContentBrowserMenuContexts.h"
#include "DirectoryWatcherModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "IDirectoryWatcher.h"
#include "IMaterialEditor.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "MaterialEditorContext.h"
#include "MaterialShared.h"
#include "MaterialValueType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCore.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShaderEditorBridge"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		static const FName DreamShaderToolMenuOwnerName(TEXT("DreamShaderEditor"));

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

		FString EscapeDreamShaderString(const FString& InText)
		{
			FString Result = InText;
			Result.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
			Result.ReplaceInline(TEXT("\""), TEXT("\\\""));
			return Result;
		}

		FString GetDreamShaderTypeForFunctionInput(EFunctionInputType InputType)
		{
			switch (InputType)
			{
			case FunctionInput_Scalar:
				return TEXT("float");
			case FunctionInput_Vector2:
				return TEXT("float2");
			case FunctionInput_Vector3:
				return TEXT("float3");
			case FunctionInput_Vector4:
				return TEXT("float4");
			case FunctionInput_Texture2D:
				return TEXT("Texture2D");
			case FunctionInput_TextureCube:
				return TEXT("TextureCube");
			case FunctionInput_Texture2DArray:
				return TEXT("Texture2DArray");
			case FunctionInput_StaticBool:
			case FunctionInput_Bool:
				return TEXT("bool");
			default:
				return TEXT("float4");
			}
		}

		FString GetDreamShaderTypeForMaterialValueType(EMaterialValueType ValueType)
		{
			switch (ValueType)
			{
			case MCT_Float:
			case MCT_Float1:
			case MCT_LWCScalar:
				return TEXT("float");
			case MCT_Float2:
			case MCT_LWCVector2:
				return TEXT("float2");
			case MCT_Float3:
			case MCT_LWCVector3:
				return TEXT("float3");
			case MCT_Float4:
			case MCT_LWCVector4:
				return TEXT("float4");
			case MCT_Texture2D:
				return TEXT("Texture2D");
			case MCT_TextureCube:
				return TEXT("TextureCube");
			case MCT_Texture2DArray:
				return TEXT("Texture2DArray");
			case MCT_StaticBool:
			case MCT_Bool:
				return TEXT("bool");
			default:
				return TEXT("float4");
			}
		}

		FString MakeDreamShaderDeclarationName(const FString& InName, const TCHAR* FallbackPrefix, int32 Index)
		{
			FString Result = UE::DreamShader::SanitizeIdentifier(InName.TrimStartAndEnd());
			if (Result.IsEmpty() || Result == TEXT("DreamShaderSymbol"))
			{
				Result = FString::Printf(TEXT("%s%d"), FallbackPrefix, Index + 1);
			}
			return Result;
		}

		bool TryMakeVirtualFunctionAssetLiteral(const UMaterialFunction* MaterialFunction, FString& OutLiteral, FString& OutError)
		{
			if (!MaterialFunction)
			{
				OutError = TEXT("No MaterialFunction asset was provided.");
				return false;
			}

			FString PackageName = MaterialFunction->GetOutermost() ? MaterialFunction->GetOutermost()->GetName() : FString();
			PackageName.TrimStartAndEndInline();
			PackageName.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (PackageName.IsEmpty() || !PackageName.StartsWith(TEXT("/")))
			{
				OutError = FString::Printf(TEXT("MaterialFunction '%s' does not have a valid package path."), *MaterialFunction->GetName());
				return false;
			}

			const auto BuildLiteral = [&OutLiteral](const TCHAR* RootName, const FString& RelativePath)
			{
				OutLiteral = FString::Printf(TEXT("Path(%s, \"%s\")"), RootName, *EscapeDreamShaderString(RelativePath));
			};

			if (PackageName.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
			{
				BuildLiteral(TEXT("Game"), PackageName.Mid(6));
				return true;
			}
			if (PackageName.StartsWith(TEXT("/Engine/"), ESearchCase::IgnoreCase))
			{
				BuildLiteral(TEXT("Engine"), PackageName.Mid(8));
				return true;
			}

			FString BestPluginName;
			FString BestMountedPath;
			for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
			{
				FString MountedPath = Plugin->GetMountedAssetPath();
				MountedPath.TrimStartAndEndInline();
				MountedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
				while (MountedPath.EndsWith(TEXT("/")))
				{
					MountedPath.LeftChopInline(1, EAllowShrinking::No);
				}
				if (!MountedPath.StartsWith(TEXT("/")))
				{
					MountedPath = TEXT("/") + MountedPath;
				}
				if (MountedPath.IsEmpty() || MountedPath == TEXT("/"))
				{
					MountedPath = TEXT("/") + Plugin->GetName();
				}

				if ((PackageName.Equals(MountedPath, ESearchCase::IgnoreCase)
					|| PackageName.StartsWith(MountedPath + TEXT("/"), ESearchCase::IgnoreCase))
					&& MountedPath.Len() > BestMountedPath.Len())
				{
					BestMountedPath = MountedPath;
					BestPluginName = Plugin->GetName();
				}
			}

			if (!BestPluginName.IsEmpty())
			{
				FString RelativePath = PackageName.Mid(BestMountedPath.Len());
				while (RelativePath.StartsWith(TEXT("/")))
				{
					RelativePath.RightChopInline(1, EAllowShrinking::No);
				}
				OutLiteral = FString::Printf(
					TEXT("Path(Plugins.%s, \"%s\")"),
					*BestPluginName,
					*EscapeDreamShaderString(RelativePath));
				return true;
			}

			OutLiteral = FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(MaterialFunction->GetPathName()));
			return true;
		}

		bool BuildVirtualFunctionDefinition(const UMaterialFunction* MaterialFunction, FString& OutDefinition, FString& OutError)
		{
			if (!MaterialFunction)
			{
				OutError = TEXT("No MaterialFunction asset was provided.");
				return false;
			}

			FString AssetLiteral;
			if (!TryMakeVirtualFunctionAssetLiteral(MaterialFunction, AssetLiteral, OutError))
			{
				return false;
			}

			TArray<FFunctionExpressionInput> Inputs;
			TArray<FFunctionExpressionOutput> Outputs;
			MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);

			if (Outputs.IsEmpty())
			{
				OutError = FString::Printf(TEXT("MaterialFunction '%s' does not expose any outputs."), *MaterialFunction->GetName());
				return false;
			}

			TArray<FString> Lines;
			Lines.Add(FString::Printf(
				TEXT("VirtualFunction(Name=\"%s\")"),
				*EscapeDreamShaderString(MakeDreamShaderDeclarationName(MaterialFunction->GetName(), TEXT("VirtualFunction"), 0))));
			Lines.Add(TEXT("{"));
			Lines.Add(TEXT("\tOptions = {"));
			Lines.Add(FString::Printf(TEXT("\t\tAsset = %s;"), *AssetLiteral));
			Lines.Add(FString::Printf(
				TEXT("\t\tDescription = \"Generated from %s\";"),
				*EscapeDreamShaderString(MaterialFunction->GetPathName())));
			Lines.Add(TEXT("\t}"));
			Lines.Add(TEXT(""));
			Lines.Add(TEXT("\tInputs = {"));
			for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
			{
				const FFunctionExpressionInput& Input = Inputs[InputIndex];
				const FString InputName = Input.ExpressionInput
					? Input.ExpressionInput->InputName.ToString()
					: Input.Input.InputName.ToString();
				const EFunctionInputType InputType = Input.ExpressionInput
					? Input.ExpressionInput->InputType.GetValue()
					: FunctionInput_Vector4;
				Lines.Add(FString::Printf(
					TEXT("\t\t%s %s;"),
					*GetDreamShaderTypeForFunctionInput(InputType),
					*MakeDreamShaderDeclarationName(InputName, TEXT("Input"), InputIndex)));
			}
			Lines.Add(TEXT("\t}"));
			Lines.Add(TEXT(""));
			Lines.Add(TEXT("\tOutputs = {"));
			for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
			{
				const FFunctionExpressionOutput& Output = Outputs[OutputIndex];
				const FString OutputName = Output.ExpressionOutput
					? Output.ExpressionOutput->OutputName.ToString()
					: Output.Output.OutputName.ToString();
				const EMaterialValueType OutputType = Output.ExpressionOutput
					? Output.ExpressionOutput->GetInputValueType(0)
					: MCT_Float4;
				Lines.Add(FString::Printf(
					TEXT("\t\t%s %s;"),
					*GetDreamShaderTypeForMaterialValueType(OutputType),
					*MakeDreamShaderDeclarationName(OutputName, TEXT("Output"), OutputIndex)));
			}
			Lines.Add(TEXT("\t}"));
			Lines.Add(TEXT("}"));

			OutDefinition = FString::Join(Lines, TEXT("\n"));
			return true;
		}

		bool BuildVirtualFunctionCallText(const UMaterialFunction* MaterialFunction, FString& OutCallText, FString& OutError)
		{
			if (!MaterialFunction)
			{
				OutError = TEXT("No MaterialFunction asset was provided.");
				return false;
			}

			TArray<FFunctionExpressionInput> Inputs;
			TArray<FFunctionExpressionOutput> Outputs;
			MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);

			if (Outputs.IsEmpty())
			{
				OutError = FString::Printf(TEXT("MaterialFunction '%s' does not expose any outputs."), *MaterialFunction->GetName());
				return false;
			}

			TArray<FString> Arguments;
			for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
			{
				const FFunctionExpressionInput& Input = Inputs[InputIndex];
				const FString InputName = Input.ExpressionInput
					? Input.ExpressionInput->InputName.ToString()
					: Input.Input.InputName.ToString();
				Arguments.Add(MakeDreamShaderDeclarationName(InputName, TEXT("Input"), InputIndex));
			}

			const FFunctionExpressionOutput& Output = Outputs[0];
			const FString OutputName = Output.ExpressionOutput
				? Output.ExpressionOutput->OutputName.ToString()
				: Output.Output.OutputName.ToString();
			Arguments.Add(FString::Printf(
				TEXT("Output=\"%s\""),
				*EscapeDreamShaderString(MakeDreamShaderDeclarationName(OutputName, TEXT("Output"), 0))));

			OutCallText = FString::Printf(
				TEXT("%s(%s)"),
				*MakeDreamShaderDeclarationName(MaterialFunction->GetName(), TEXT("VirtualFunction"), 0),
				*FString::Join(Arguments, TEXT(", ")));
			return true;
		}

		FString MakeUniqueVirtualFunctionDefinitionFilePath(const UMaterialFunction* MaterialFunction)
		{
			const FString DefinitionDirectory = FPaths::Combine(
				UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("VirtualFunctions"));
			const FString BaseName = MakeDreamShaderDeclarationName(
				MaterialFunction ? MaterialFunction->GetName() : FString(),
				TEXT("VirtualFunction"),
				0);

			FString Candidate = FPaths::Combine(DefinitionDirectory, BaseName + TEXT(".dsh"));
			for (int32 Suffix = 2; IFileManager::Get().FileExists(*Candidate); ++Suffix)
			{
				Candidate = FPaths::Combine(
					DefinitionDirectory,
					FString::Printf(TEXT("%s_%d.dsh"), *BaseName, Suffix));
			}

			return UE::DreamShader::NormalizeSourceFilePath(Candidate);
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
			if (!Diagnostic.Detail.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("detail"), Diagnostic.Detail);
			}
			if (!Diagnostic.Stage.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("stage"), Diagnostic.Stage);
			}
			if (!Diagnostic.AssetPath.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("assetPath"), Diagnostic.AssetPath);
			}
			if (!Diagnostic.ShaderPlatform.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("shaderPlatform"), Diagnostic.ShaderPlatform);
			}
			if (!Diagnostic.QualityLevel.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("qualityLevel"), Diagnostic.QualityLevel);
			}
			if (!Diagnostic.Code.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("code"), Diagnostic.Code);
			}
			DiagnosticObject->SetNumberField(TEXT("line"), Diagnostic.Line);
			DiagnosticObject->SetNumberField(TEXT("column"), Diagnostic.Column);
			DiagnosticObject->SetStringField(TEXT("severity"), Diagnostic.Severity);
			DiagnosticObject->SetStringField(TEXT("source"), Diagnostic.Source);
			OutDiagnostics.Add(MakeShared<FJsonValueObject>(DiagnosticObject));
		}

		FString GetShaderPlatformLabel(const EShaderPlatform ShaderPlatform)
		{
			const FName ShaderFormat = LegacyShaderPlatformToShaderFormat(ShaderPlatform);
			return ShaderFormat.IsNone()
				? FString::Printf(TEXT("Platform %d"), static_cast<int32>(ShaderPlatform))
				: ShaderFormat.ToString();
		}

		FString GetMaterialQualityLevelLabel(const EMaterialQualityLevel::Type QualityLevel)
		{
			return LexToString(QualityLevel);
		}

		FString GetFirstMeaningfulErrorLine(const FString& InError)
		{
			TArray<FString> Lines;
			InError.ParseIntoArrayLines(Lines, false);
			for (const FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					return Trimmed;
				}
			}
			return InError.TrimStartAndEnd();
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
		bIsShuttingDown = false;

		IFileManager::Get().MakeDirectory(*GetBridgeDirectory(), true);
		IFileManager::Get().MakeDirectory(*GetRequestDirectory(), true);

		QueueFullScan();
		UpdateDiagnosticsFile();

		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
		{
			WatchedSourceDirectory = UE::DreamShader::GetSourceShaderDirectory();
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				WatchedSourceDirectory,
				IDirectoryWatcher::FDirectoryChanged::CreateSP(AsShared(), &FDreamShaderEditorBridge::OnDirectoryChanged),
				DirectoryWatcherHandle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
		}

		MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddSP(
			AsShared(),
			&FDreamShaderEditorBridge::OnMaterialCompilationFinished);

		ToolMenusStartupCallbackHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::RegisterMenus));

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::Tick),
			0.1f);
	}

	void FDreamShaderEditorBridge::Shutdown()
	{
		bIsShuttingDown = true;

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

		if (ToolMenusStartupCallbackHandle.IsValid())
		{
			UToolMenus::UnRegisterStartupCallback(ToolMenusStartupCallbackHandle);
			ToolMenusStartupCallbackHandle.Reset();
		}

		if (!IsEngineExitRequested() && !GExitPurge)
		{
			UToolMenus::UnregisterOwner(DreamShaderToolMenuOwnerName);
		}

		if (DirectoryWatcherHandle.IsValid())
		{
			if (FDirectoryWatcherModule* DirectoryWatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
			{
				if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule->Get())
				{
					DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(WatchedSourceDirectory, DirectoryWatcherHandle);
				}
			}

			DirectoryWatcherHandle.Reset();
			WatchedSourceDirectory.Reset();
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
		TWeakPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> WeakBridge = AsWeak();
		AsyncTask(ENamedThreads::GameThread, [WeakBridge, Changes = MoveTemp(ChangesCopy)]()
		{
			TSharedPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> Bridge = WeakBridge.Pin();
			if (!Bridge.IsValid() || Bridge->bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
			{
				return;
			}

			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
			if (Settings && !Settings->bAutoCompileOnSave)
			{
				return;
			}

			for (const FFileChangeData& FileChange : Changes)
			{
				if (FileChange.Action == FFileChangeData::FCA_RescanRequired)
				{
					Bridge->QueueFullScan();
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
						Bridge->QueueDependentMaterialsForHeader(FileChange.Filename);
					}
					else if (IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						Bridge->RebuildDependencyGraph();
						Bridge->QueueSourceFile(FileChange.Filename);
					}
				}
				else if (FileChange.Action == FFileChangeData::FCA_Removed)
				{
					const FString SourceFile = UE::DreamShader::NormalizeSourceFilePath(FileChange.Filename);
					if (UE::DreamShader::IsDreamShaderHeaderFile(FileChange.Filename))
					{
						Bridge->QueueDependentMaterialsForHeader(FileChange.Filename);
					}
					else if (IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						Bridge->PendingFiles.Remove(SourceFile);
						Bridge->ClearDiagnosticsForSourceAndDependencies(SourceFile);
						Bridge->RebuildDependencyGraph();
						Bridge->UpdateDiagnosticsFile();
					}
					UE_LOG(LogDreamShader, Display, TEXT("DreamShader source removed, existing generated assets were left untouched: %s"), *FileChange.Filename);
				}
			}
		});
	}

	bool FDreamShaderEditorBridge::Tick(float DeltaSeconds)
	{
		(void)DeltaSeconds;

		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return false;
		}

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
				else if (Action.Equals(TEXT("cleanGeneratedShaders"), ESearchCase::IgnoreCase))
				{
					RequestCleanGeneratedShaders();
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
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

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
		const FString MaterialAssetPath = Material->GetPathName();
		TSet<FString> SeenDiagnosticKeys;
		for (int32 ShaderPlatformIndex = 0; ShaderPlatformIndex < EShaderPlatform::SP_NumPlatforms; ++ShaderPlatformIndex)
		{
			const EShaderPlatform ShaderPlatform = static_cast<EShaderPlatform>(ShaderPlatformIndex);
			for (int32 QualityLevelIndex = 0; QualityLevelIndex <= static_cast<int32>(EMaterialQualityLevel::Num); ++QualityLevelIndex)
			{
				const EMaterialQualityLevel::Type QualityLevel = static_cast<EMaterialQualityLevel::Type>(QualityLevelIndex);
				const FMaterialResource* MaterialResource = Material->GetMaterialResource(ShaderPlatform, QualityLevel);
				if (!MaterialResource)
				{
					continue;
				}

				const FString ShaderPlatformLabel = GetShaderPlatformLabel(ShaderPlatform);
				const FString QualityLabel = GetMaterialQualityLevelLabel(QualityLevel);
				for (const FString& Error : MaterialResource->GetCompileErrors())
				{
					const FString RawError = Error.TrimStartAndEnd();
					if (RawError.IsEmpty())
					{
						continue;
					}

					FString ParsedFilePath;
					int32 ParsedLine = 1;
					int32 ParsedColumn = 1;
					FString ParsedMessage;
					const bool bHasParsedLocation = TryParseErrorLocation(RawError, ParsedFilePath, ParsedLine, ParsedColumn, ParsedMessage);
					const bool bMapsToDreamShaderSource = bHasParsedLocation && UE::DreamShader::IsDreamShaderSourceFile(ParsedFilePath);

					const FString DisplayMessage = FString::Printf(
						TEXT("[%s / %s] %s"),
						*ShaderPlatformLabel,
						*QualityLabel,
						*(bHasParsedLocation ? ParsedMessage : GetFirstMeaningfulErrorLine(RawError)));

					const FString DeduplicationKey = FString::Printf(
						TEXT("%s|%s|%s|%s|%d|%d"),
						*SourceFilePath,
						*ShaderPlatformLabel,
						*QualityLabel,
						*DisplayMessage,
						bMapsToDreamShaderSource ? ParsedLine : 1,
						bMapsToDreamShaderSource ? ParsedColumn : 1);
					if (SeenDiagnosticKeys.Contains(DeduplicationKey))
					{
						continue;
					}
					SeenDiagnosticKeys.Add(DeduplicationKey);

					FDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
					Diagnostic.FilePath = bMapsToDreamShaderSource ? ParsedFilePath : SourceFilePath;
					Diagnostic.Message = DisplayMessage;
					Diagnostic.Detail = RawError;
					Diagnostic.Stage = TEXT("materialCompile");
					Diagnostic.AssetPath = MaterialAssetPath;
					Diagnostic.ShaderPlatform = ShaderPlatformLabel;
					Diagnostic.QualityLevel = QualityLabel;
					Diagnostic.Code = TEXT("material-compile");
					Diagnostic.Source = TEXT("DreamShader Material Compile");
					Diagnostic.Line = bMapsToDreamShaderSource ? ParsedLine : 1;
					Diagnostic.Column = bMapsToDreamShaderSource ? ParsedColumn : 1;
				}
			}
		}

		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
	}

	void FDreamShaderEditorBridge::RegisterMenus()
	{
		if (bIsShuttingDown || bMenusRegistered || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		bMenusRegistered = true;

		FToolMenuOwnerScoped MenuOwner(DreamShaderToolMenuOwnerName);

		if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddMenuEntry(
				TEXT("DreamShader.RecompileAll"),
				LOCTEXT("DreamShaderRecompileLabel", "Recompile DSM"),
				LOCTEXT("DreamShaderRecompileTooltip", "Recompile all DreamShader .dsm files and refresh diagnostics."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestRecompileAll)));
			Section.AddMenuEntry(
				TEXT("DreamShader.CleanGeneratedShaders"),
				LOCTEXT("DreamShaderCleanGeneratedShadersLabel", "Clean Generated Shaders"),
				LOCTEXT("DreamShaderCleanGeneratedShadersTooltip", "Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Delete")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestCleanGeneratedShaders)));
		}

		if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar")))
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("DreamShader.RecompileAllToolbar"),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestRecompileAll)),
				LOCTEXT("DreamShaderRecompileToolbarLabel", "DSM"),
				LOCTEXT("DreamShaderRecompileToolbarTooltip", "Recompile all DreamShader .dsm files."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh"))));
		}

		if (UToolMenu* MaterialFunctionAssetMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMaterialFunction::StaticClass()))
		{
			FToolMenuSection& Section = MaterialFunctionAssetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.VirtualFunctionAssetActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu));
		}

		if (UToolMenu* MaterialEditorToolbar = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.MaterialEditor.ToolBar")))
		{
			FToolMenuSection& Section = MaterialEditorToolbar->FindOrAddSection(TEXT("DreamShader"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.VirtualFunctionToolbarActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialFunctionEditorToolbar));
		}
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu(FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
		if (!Context || Context->SelectedAssets.Num() != 1 || !Context->SelectedAssets[0].IsInstanceOf(UMaterialFunction::StaticClass()))
		{
			return;
		}

		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Context->SelectedAssets[0].GetAsset());
		if (!MaterialFunction)
		{
			return;
		}

		InSection.AddSubMenu(
			TEXT("DreamShader.MaterialFunctionActions"),
			LOCTEXT("DreamShaderMaterialFunctionActionsLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialFunctionActionsTooltip", "DreamShader actions for this Material Function."),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu,
				TWeakObjectPtr<UMaterialFunction>(MaterialFunction)),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")));
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionEditorToolbar(FToolMenuSection& InSection)
	{
		const UMaterialEditorMenuContext* Context = InSection.FindContext<UMaterialEditorMenuContext>();
		TSharedPtr<IMaterialEditor> MaterialEditor = Context ? Context->MaterialEditor.Pin() : nullptr;
		if (!MaterialEditor.IsValid())
		{
			return;
		}

		UMaterialFunction* MaterialFunction = nullptr;
		const TArray<UObject*>* EditingObjects = MaterialEditor->GetObjectsCurrentlyBeingEdited();
		if (EditingObjects)
		{
			for (UObject* EditingObject : *EditingObjects)
			{
				MaterialFunction = Cast<UMaterialFunction>(EditingObject);
				if (MaterialFunction)
				{
					break;
				}
			}
		}

		if (!MaterialFunction)
		{
			return;
		}

		InSection.AddEntry(FToolMenuEntry::InitComboButton(
			TEXT("DreamShader.MaterialFunctionToolbarMenu"),
			FUIAction(),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu,
				TWeakObjectPtr<UMaterialFunction>(MaterialFunction)),
			LOCTEXT("DreamShaderMaterialFunctionToolbarMenuLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialFunctionToolbarMenuTooltip", "DreamShader actions for this Material Function."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings"))));
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		if (!InMenu || !MaterialFunction.IsValid())
		{
			return;
		}

		FToolMenuSection& Section = InMenu->AddSection(
			TEXT("DreamShader.VirtualFunctionActions"),
			LOCTEXT("DreamShaderVirtualFunctionActionsSection", "VirtualFunction"));
		Section.AddMenuEntry(
			TEXT("DreamShader.CopyVirtualFunction"),
			LOCTEXT("DreamShaderCopyVirtualFunctionLabel", "CopyVirtualFunction"),
			LOCTEXT("DreamShaderCopyVirtualFunctionTooltip", "Copy a complete DreamShader VirtualFunction declaration for this Material Function."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CopyVirtualFunctionDefinition,
				MaterialFunction)));
		Section.AddMenuEntry(
			TEXT("DreamShader.CreateVirtualFunction"),
			LOCTEXT("DreamShaderCreateVirtualFunctionLabel", "CreateVirtualFunction"),
			LOCTEXT("DreamShaderCreateVirtualFunctionTooltip", "Create a .dsh file containing the VirtualFunction declaration."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CreateVirtualFunctionDefinitionFile,
				MaterialFunction)));
		Section.AddMenuEntry(
			TEXT("DreamShader.CopyVirtualFunctionCall"),
			LOCTEXT("DreamShaderCopyVirtualFunctionCallLabel", "CopyVirtualFunctionCall"),
			LOCTEXT("DreamShaderCopyVirtualFunctionCallTooltip", "Copy a DreamShader Graph call example for this VirtualFunction."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CopyVirtualFunctionCall,
				MaterialFunction)));
	}

	void FDreamShaderEditorBridge::RequestRecompileAll()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		QueueFullScan();
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader queued a full .dsm recompile scan."));
	}

	void FDreamShaderEditorBridge::RequestCleanGeneratedShaders()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		CleanGeneratedShaderDirectory();
		QueueFullScan();
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader cleaned generated shader includes and queued a full .dsm recompile scan."));
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionDefinition(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FString DefinitionText;
		FString Error;
		if (!BuildVirtualFunctionDefinition(Function, DefinitionText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction definition for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*DefinitionText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction definition for %s."), *Function->GetName())),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Copied VirtualFunction definition for '%s'.\n%s"), *Function->GetPathName(), *DefinitionText);
	}

	void FDreamShaderEditorBridge::CreateVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCreateVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FString DefinitionText;
		FString Error;
		if (!BuildVirtualFunctionDefinition(Function, DefinitionText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction definition file for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		const FString DefinitionFilePath = MakeUniqueVirtualFunctionDefinitionFilePath(Function);
		const FString DefinitionDirectory = FPaths::GetPath(DefinitionFilePath);
		if (!IFileManager::Get().MakeDirectory(*DefinitionDirectory, true))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to create directory: %s"), *DefinitionDirectory)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to create VirtualFunction definition directory '%s'."), *DefinitionDirectory);
			return;
		}

		if (!FFileHelper::SaveStringToFile(DefinitionText, *DefinitionFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to write VirtualFunction file: %s"), *DefinitionFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write VirtualFunction definition file '%s'."), *DefinitionFilePath);
			return;
		}

		FPlatformProcess::LaunchFileInDefaultExternalApplication(*DefinitionFilePath, nullptr, ELaunchVerb::Edit);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Created VirtualFunction file: %s"), *DefinitionFilePath)),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Created VirtualFunction definition file '%s' for '%s'.\n%s"), *DefinitionFilePath, *Function->GetPathName(), *DefinitionText);
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionCall(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionCallNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FString CallText;
		FString Error;
		if (!BuildVirtualFunctionCallText(Function, CallText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction call: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction call for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*CallText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction call for %s."), *Function->GetName())),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Copied VirtualFunction call for '%s': %s"), *Function->GetPathName(), *CallText);
	}

	void FDreamShaderEditorBridge::CleanGeneratedShaderDirectory()
	{
		const FString GeneratedShaderDirectory = UE::DreamShader::GetGeneratedShaderDirectory();
		IFileManager& FileManager = IFileManager::Get();

		TArray<FString> GeneratedShaderFiles;
		FileManager.FindFilesRecursive(
			GeneratedShaderFiles,
			*GeneratedShaderDirectory,
			TEXT("*"),
			true,
			false,
			false);

		const int32 DeletedFileCount = GeneratedShaderFiles.Num();
		FileManager.DeleteDirectory(*GeneratedShaderDirectory, false, true);
		FileManager.MakeDirectory(*GeneratedShaderDirectory, true);

		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader deleted %d generated shader file(s) from '%s'."),
			DeletedFileCount,
			*GeneratedShaderDirectory);
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
				Diagnostic.Detail = Line;
				Diagnostic.Stage = TEXT("generate");
				Diagnostic.Code = TEXT("generate-error");
				Diagnostic.Source = TEXT("DreamShader Generate");
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
			Diagnostic.Detail = Line;
			Diagnostic.Stage = TEXT("generate");
			Diagnostic.Code = TEXT("generate-error");
			Diagnostic.Source = TEXT("DreamShader Generate");
		}

		return Diagnostics;
	}
}

#undef LOCTEXT_NAMESPACE
