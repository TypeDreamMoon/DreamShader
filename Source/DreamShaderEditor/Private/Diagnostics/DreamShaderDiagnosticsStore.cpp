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
#include "DreamShaderDatabase.h"
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
	}

	void FDreamShaderDiagnosticsStore::Reset()
	{
		DiagnosticsByOwner.Reset();
		DiagnosticsByFile.Reset();
	}

	void FDreamShaderDiagnosticsStore::SetDiagnostics(
		const FString& SourceFilePath,
		const EDreamShaderDiagnosticsProducer Producer,
		TArray<FDreamShaderDiagnosticRecord>&& Diagnostics,
		const FString& Scope)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		const FOwner Owner{ NormalizedPath, Producer, Scope };
		if (Diagnostics.IsEmpty())
		{
			if (DiagnosticsByOwner.Remove(Owner) > 0)
			{
				RebuildDiagnosticsByFile();
			}
			return;
		}

		for (FDreamShaderDiagnosticRecord& Diagnostic : Diagnostics)
		{
			Diagnostic.FilePath = Diagnostic.FilePath.IsEmpty()
				? NormalizedPath
				: UE::DreamShader::NormalizeSourceFilePath(Diagnostic.FilePath);
			Diagnostic.OwnerSourceFilePath = NormalizedPath;
		}
		DiagnosticsByOwner.Add(Owner, MoveTemp(Diagnostics));
		RebuildDiagnosticsByFile();
	}

	void FDreamShaderDiagnosticsStore::ClearDiagnostics(const FString& SourceFilePath)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		for (auto It = DiagnosticsByOwner.CreateIterator(); It; ++It)
		{
			if (It.Key().SourceFilePath == NormalizedPath)
			{
				It.RemoveCurrent();
				continue;
			}

			It.Value().RemoveAll([&NormalizedPath](const FDreamShaderDiagnosticRecord& Diagnostic)
			{
				return Diagnostic.FilePath == NormalizedPath;
			});
			if (It.Value().IsEmpty())
			{
				It.RemoveCurrent();
			}
		}
		RebuildDiagnosticsByFile();
	}

	const TArray<FDreamShaderDiagnosticRecord>* FDreamShaderDiagnosticsStore::FindDiagnostics(const FString& FilePath) const
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(FilePath);
		return DiagnosticsByFile.Find(NormalizedPath);
	}

	TArray<FString> FDreamShaderDiagnosticsStore::GetFilePaths() const
	{
		TArray<FString> FilePaths;
		DiagnosticsByFile.GetKeys(FilePaths);
		FilePaths.Sort();
		return FilePaths;
	}

	const TArray<FDreamShaderDiagnosticRecord>* FDreamShaderDiagnosticsStore::FindOwnedDiagnostics(
		const FString& SourceFilePath,
		const EDreamShaderDiagnosticsProducer Producer,
		const FString& Scope) const
	{
		return DiagnosticsByOwner.Find(FOwner{ UE::DreamShader::NormalizeSourceFilePath(SourceFilePath), Producer, Scope });
	}

	void FDreamShaderDiagnosticsStore::RebuildDiagnosticsByFile()
	{
		// A file's records are listed compile first, then shader compile, then the VirtualFunction scan, and by source
		// within each: a stable order for the wire files, and the compile's first error is the one the browser shows.
		DiagnosticsByOwner.KeySort([](const FOwner& A, const FOwner& B)
		{
			if (A.Producer != B.Producer)
			{
				return A.Producer < B.Producer;
			}
			if (A.SourceFilePath != B.SourceFilePath)
			{
				return A.SourceFilePath < B.SourceFilePath;
			}
			return A.Scope < B.Scope;
		});

		DiagnosticsByFile.Reset();
		for (const TPair<FOwner, TArray<FDreamShaderDiagnosticRecord>>& Pair : DiagnosticsByOwner)
		{
			for (const FDreamShaderDiagnosticRecord& Diagnostic : Pair.Value)
			{
				TArray<FDreamShaderDiagnosticRecord>& FileDiagnostics = DiagnosticsByFile.FindOrAdd(Diagnostic.FilePath);
				FileDiagnostics.Add_GetRef(Diagnostic).FilePath.Reset();
			}
		}
	}

	bool FDreamShaderDiagnosticsStore::WriteToDatabase(const FString& DatabaseFilePath) const
	{
		// The whole table, every time, in one transaction: it holds this session's findings and nothing else, and one
		// commit is what keeps a reader from ever seeing half of it. A reader rebuilds the old diagnostics.json envelope
		// from the rows -- files = every row's json, updatedAtUtc = meta diagnostics_updated_utc.
		FDreamShaderDatabase Database;
		if (!Database.Open(DatabaseFilePath))
		{
			return false;
		}
		if (!Database.Begin())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader: %s"), *Database.DescribeError(TEXT("BEGIN")));
			return false;
		}

		const FString UpdatedAtUtc = FDateTime::UtcNow().ToIso8601();
		bool bOk = Database.Get().Execute(TEXT("DELETE FROM diagnostics;"));
		{
			FSQLitePreparedStatement Statement(
				Database.Get(),
				TEXT("INSERT OR REPLACE INTO diagnostics(path, json, updated_at_utc) VALUES(?1, ?2, ?3);"));
			bOk = bOk && Statement.IsValid();
			for (const TPair<FString, TArray<FDreamShaderDiagnosticRecord>>& Pair : DiagnosticsByFile)
			{
				if (!bOk)
				{
					break;
				}
				FString JsonText;
				SerializeJsonObject(BuildDiagnosticsFileObject(Pair.Key, Pair.Value), JsonText);
				Statement.Reset();
				Statement.ClearBindings();
				bOk = Statement.SetBindingValueByIndex(1, Pair.Key)
					&& Statement.SetBindingValueByIndex(2, JsonText)
					&& Statement.SetBindingValueByIndex(3, UpdatedAtUtc)
					&& Statement.Step() == ESQLitePreparedStatementStepResult::Done;
			}
		}
		if (!bOk || !Database.Bump(TEXT("diagnostics")) || !Database.Commit())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader: %s; the diagnostics external editors show are now stale."),
				*Database.DescribeError(TEXT("writing diagnostics")));
			Database.Rollback();
			return false;
		}
		return true;
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
