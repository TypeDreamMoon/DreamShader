#include "DreamShaderDiagnosticsStore.h"

#include "DreamShaderEditorPersistenceUtils.h"
#include "DreamShaderModule.h"
#include "DreamShaderTextWireUtils.h"
#include "DreamShaderVersionCompat.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		// The wire format (diagnostics.json / diagnostics/*.json / bridge.db) is parsed by VSCode and
		// Rider extensions and MUST stay English (culture-invariant) regardless of the editor culture.
		// ToInvariantWireString returns the source-language string for plain LOCTEXT/FromString text
		// and re-renders FText::Format results with the invariant culture, so a zh-Hans editor can
		// never leak localized text into the wire format (see DreamShaderTextWireUtils.h).
		FString DiagnosticTextToJsonString(const FText& Text)
		{
			return ToInvariantWireString(Text);
		}

		void AddDiagnosticJson(TArray<TSharedPtr<FJsonValue>>& OutDiagnostics, const FDreamShaderDiagnosticRecord& Diagnostic)
		{
			TSharedRef<FJsonObject> DiagnosticObject = MakeShared<FJsonObject>();
			DiagnosticObject->SetStringField(TEXT("message"), DiagnosticTextToJsonString(Diagnostic.Message));
			if (!Diagnostic.Detail.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("detail"), DiagnosticTextToJsonString(Diagnostic.Detail));
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

		FString MakeStableFileKey(const FString& FilePath)
		{
			return FMD5::HashAnsiString(*UE::DreamShader::NormalizeSourceFilePath(FilePath));
		}

		TSharedRef<FJsonObject> BuildDiagnosticsFileObject(
			const FString& FilePath,
			const TArray<FDreamShaderDiagnosticRecord>& Diagnostics)
		{
			TSharedRef<FJsonObject> FileObject = MakeShared<FJsonObject>();
			FileObject->SetStringField(TEXT("path"), FilePath);

			TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
			for (const FDreamShaderDiagnosticRecord& Diagnostic : Diagnostics)
			{
				AddDiagnosticJson(DiagnosticValues, Diagnostic);
			}

			FileObject->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
			return FileObject;
		}

		void RemoveDiagnosticsOwnedBySource(
			TMap<FString, TArray<FDreamShaderDiagnosticRecord>>& InOutDiagnosticsByFile,
			const FString& OwnerSourceFilePath)
		{
			TArray<FString> EmptyFiles;
			for (TPair<FString, TArray<FDreamShaderDiagnosticRecord>>& Pair : InOutDiagnosticsByFile)
			{
				Pair.Value.RemoveAll([&OwnerSourceFilePath](const FDreamShaderDiagnosticRecord& Diagnostic)
				{
					return Diagnostic.OwnerSourceFilePath.Equals(OwnerSourceFilePath, ESearchCase::IgnoreCase);
				});

				if (Pair.Value.IsEmpty())
				{
					EmptyFiles.Add(Pair.Key);
				}
			}

			for (const FString& EmptyFile : EmptyFiles)
			{
				InOutDiagnosticsByFile.Remove(EmptyFile);
			}
		}
	}

	void FDreamShaderDiagnosticsStore::Reset()
	{
		DiagnosticsByFile.Reset();
	}

	void FDreamShaderDiagnosticsStore::SetDiagnostics(
		const FString& SourceFilePath,
		TArray<FDreamShaderDiagnosticRecord>&& Diagnostics)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		RemoveDiagnosticsOwnedBySource(DiagnosticsByFile, NormalizedPath);
		if (Diagnostics.IsEmpty())
		{
			return;
		}

		for (FDreamShaderDiagnosticRecord& Diagnostic : Diagnostics)
		{
			const FString DiagnosticFilePath = Diagnostic.FilePath.IsEmpty()
				? NormalizedPath
				: UE::DreamShader::NormalizeSourceFilePath(Diagnostic.FilePath);
			Diagnostic.FilePath.Reset();
			Diagnostic.OwnerSourceFilePath = NormalizedPath;
			DiagnosticsByFile.FindOrAdd(DiagnosticFilePath).Add(MoveTemp(Diagnostic));
		}
	}

	void FDreamShaderDiagnosticsStore::ClearDiagnostics(const FString& SourceFilePath)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		DiagnosticsByFile.Remove(NormalizedPath);
		RemoveDiagnosticsOwnedBySource(DiagnosticsByFile, NormalizedPath);
	}

	const TArray<FDreamShaderDiagnosticRecord>* FDreamShaderDiagnosticsStore::FindDiagnostics(const FString& SourceFilePath) const
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		return DiagnosticsByFile.Find(NormalizedPath);
	}

	void FDreamShaderDiagnosticsStore::WriteToFile(const FString& OutputFilePath) const
	{
		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetNumberField(TEXT("version"), 1);
		RootObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());

		TArray<TSharedPtr<FJsonValue>> FileEntries;
		for (const TPair<FString, TArray<FDreamShaderDiagnosticRecord>>& Pair : DiagnosticsByFile)
		{
			FileEntries.Add(MakeShared<FJsonValueObject>(BuildDiagnosticsFileObject(Pair.Key, Pair.Value)));
		}

		RootObject->SetArrayField(TEXT("files"), FileEntries);

		FString OutputText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputText);
		FJsonSerializer::Serialize(RootObject, Writer);
		FFileHelper::SaveStringToFile(OutputText, *OutputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	void FDreamShaderDiagnosticsStore::WriteToDirectory(const FString& OutputDirectory) const
	{
		IFileManager::Get().MakeDirectory(*OutputDirectory, true);

		TSet<FString> ActiveFileKeys;
		TArray<TSharedPtr<FJsonValue>> FileIndexValues;
		for (const TPair<FString, TArray<FDreamShaderDiagnosticRecord>>& Pair : DiagnosticsByFile)
		{
			const FString FileKey = MakeStableFileKey(Pair.Key);
			ActiveFileKeys.Add(FileKey);

			TSharedRef<FJsonObject> FileObject = BuildDiagnosticsFileObject(Pair.Key, Pair.Value);
			FString FileText;
			SerializeJsonObject(FileObject, FileText);
			FFileHelper::SaveStringToFile(
				FileText,
				*FPaths::Combine(OutputDirectory, FileKey + TEXT(".json")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

			TSharedRef<FJsonObject> IndexObject = MakeShared<FJsonObject>();
			IndexObject->SetStringField(TEXT("path"), Pair.Key);
			IndexObject->SetStringField(TEXT("file"), FileKey + TEXT(".json"));
			IndexObject->SetNumberField(TEXT("count"), Pair.Value.Num());
			FileIndexValues.Add(MakeShared<FJsonValueObject>(IndexObject));
		}

		TArray<FString> ExistingFiles;
		IFileManager::Get().FindFiles(ExistingFiles, *FPaths::Combine(OutputDirectory, TEXT("*.json")), true, false);
		for (const FString& ExistingFile : ExistingFiles)
		{
			const FString BaseName = FPaths::GetBaseFilename(ExistingFile);
			if (!BaseName.Equals(TEXT("index"), ESearchCase::IgnoreCase) && !ActiveFileKeys.Contains(BaseName))
			{
				IFileManager::Get().Delete(*FPaths::Combine(OutputDirectory, ExistingFile), false, true, true);
			}
		}

		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetNumberField(TEXT("version"), 1);
		RootObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		RootObject->SetArrayField(TEXT("files"), FileIndexValues);

		FString IndexText;
		SerializeJsonObject(RootObject, IndexText);
		FFileHelper::SaveStringToFile(
			IndexText,
			*FPaths::Combine(OutputDirectory, TEXT("index.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	void FDreamShaderDiagnosticsStore::WriteToDatabase(const FString& DatabaseFilePath) const
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(DatabaseFilePath), true);

		FSQLiteDatabase Database;
		if (!Database.Open(*DatabaseFilePath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
		{
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to open DreamShader bridge database for diagnostics: %s"), *DatabaseFilePath);
			return;
		}

		Database.Execute(TEXT("PRAGMA journal_mode=WAL;"));
		Database.Execute(TEXT("PRAGMA synchronous=NORMAL;"));
		Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS diagnostics(path TEXT PRIMARY KEY, json TEXT NOT NULL, updated_at_utc TEXT NOT NULL);"));
		Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);"));
		Database.Execute(TEXT("BEGIN TRANSACTION;"));
		Database.Execute(TEXT("DELETE FROM diagnostics;"));

		const FString UpdatedAtUtc = FDateTime::UtcNow().ToIso8601();
		{
			FSQLitePreparedStatement Statement(
				Database,
				TEXT("INSERT OR REPLACE INTO diagnostics(path, json, updated_at_utc) VALUES(?1, ?2, ?3);"));
			if (Statement.IsValid())
			{
				for (const TPair<FString, TArray<FDreamShaderDiagnosticRecord>>& Pair : DiagnosticsByFile)
				{
					FString JsonText;
					SerializeJsonObject(BuildDiagnosticsFileObject(Pair.Key, Pair.Value), JsonText);
					Statement.SetBindingValueByIndex(1, Pair.Key);
					Statement.SetBindingValueByIndex(2, JsonText);
					Statement.SetBindingValueByIndex(3, UpdatedAtUtc);
					BindAndExecute(Statement);
				}
			}
		}

		{
			FSQLitePreparedStatement MetaStatement(
				Database,
				TEXT("INSERT OR REPLACE INTO meta(key, value) VALUES('diagnostics.updatedAt', ?1);"));
			if (MetaStatement.IsValid())
			{
				MetaStatement.SetBindingValueByIndex(1, UpdatedAtUtc);
				BindAndExecute(MetaStatement);
			}
		}

		Database.Execute(TEXT("COMMIT;"));
		Database.Close();
	}

	bool FDreamShaderDiagnosticsStore::TryParseErrorLocation(
		const FString& Line,
		FDreamShaderDiagnosticLocation& OutLocation)
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

		OutLocation.Line = FMath::Max(1, FCString::Atoi(*LineText));
		OutLocation.Column = FMath::Max(1, FCString::Atoi(*ColumnText));
		OutLocation.FilePath = UE::DreamShader::NormalizeSourceFilePath(Line.Left(OpenMarkerIndex));
		// The message is the tail of a dynamic UE compile error line; FText::FromString marks it as
		// dynamic text (correct for gather) and its source string is the raw English error text.
		OutLocation.Message = FText::FromString(Line.Mid(CloseMarkerIndex + 3).TrimStartAndEnd());
		return !OutLocation.FilePath.IsEmpty() && !OutLocation.Message.IsEmpty();
	}

		TArray<FDreamShaderDiagnosticRecord> FDreamShaderDiagnosticsStore::BuildGenerateErrorDiagnostics(
			const FString& SourceFilePath,
			const FText& ErrorMessage)
	{
		TArray<FDreamShaderDiagnosticRecord> Diagnostics;

		const FString ErrorMessageWire = ToInvariantWireString(ErrorMessage);
		TArray<FString> Lines;
		ErrorMessageWire.ParseIntoArrayLines(Lines, false);
		if (Lines.IsEmpty())
		{
			Lines.Add(ErrorMessageWire);
		}

		const FString PathPrefix = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath) + TEXT(": ");
		for (FString Line : Lines)
		{
			FDreamShaderDiagnosticLocation Location;
			if (TryParseErrorLocation(Line, Location))
			{
				FDreamShaderDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
				Diagnostic.FilePath = Location.FilePath;
				Diagnostic.Message = Location.Message;
				Diagnostic.Detail = FText::FromString(Line);
				Diagnostic.Stage = TEXT("generate");
				Diagnostic.Code = TEXT("generate-error");
				Diagnostic.Source = TEXT("DreamShader Generate");
				Diagnostic.Line = Location.Line;
				Diagnostic.Column = Location.Column;
				continue;
			}

			if (Line.StartsWith(PathPrefix))
			{
				Line.RightChopInline(PathPrefix.Len(), DREAMSHADER_ALLOW_SHRINKING_NO);
			}

			FDreamShaderDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Message = FText::FromString(Line);
			Diagnostic.Detail = FText::FromString(Line);
			Diagnostic.Stage = TEXT("generate");
			Diagnostic.Code = TEXT("generate-error");
			Diagnostic.Source = TEXT("DreamShader Generate");
		}

		return Diagnostics;
	}
}
