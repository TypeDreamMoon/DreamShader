// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "MCP/DreamShaderMCPResult.h"

#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderDiagnosticRecord.h"
#include "DreamShaderModule.h"
#include "DreamShaderTextWireUtils.h"
#include "Lang/LangDiagnostic.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace UE::DreamShader::Editor::MCP
{
	const TCHAR* LexOutcome(const EOutcome Outcome)
	{
		switch (Outcome)
		{
		case EOutcome::Ok:          return TEXT("ok");
		case EOutcome::SourceError: return TEXT("source-error");
		case EOutcome::Refused:     return TEXT("refused");
		case EOutcome::EngineError: return TEXT("engine-error");
		}
		return TEXT("engine-error");
	}

	FResult FResult::MakeOk(const FString& InSummary)
	{
		FResult Result;
		Result.Summary = InSummary;
		return Result;
	}

	FResult FResult::MakeRefused(const FString& InReason, const FString& InSummary, const FString& InNext)
	{
		FResult Result;
		Result.Outcome = EOutcome::Refused;
		Result.Reason = InReason;
		Result.Summary = InSummary;
		if (!InNext.IsEmpty())
		{
			Result.Next.Add(InNext);
		}
		return Result;
	}

	FResult FResult::MakeEngineError(const FString& InReason, const FString& InSummary, const FString& InNext)
	{
		FResult Result = MakeRefused(InReason, InSummary, InNext);
		Result.Outcome = EOutcome::EngineError;
		return Result;
	}

	TSharedRef<FJsonObject> FResult::ToJson() const
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("ok"), Outcome == EOutcome::Ok);
		Root->SetStringField(TEXT("outcome"), LexOutcome(Outcome));
		if (Reason.IsEmpty())
		{
			Root->SetField(TEXT("reason"), MakeShared<FJsonValueNull>());
		}
		else
		{
			Root->SetStringField(TEXT("reason"), Reason);
		}
		Root->SetStringField(TEXT("summary"), Summary);

		TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
		for (const FDiagnostic& Diagnostic : Diagnostics)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			if (!Diagnostic.Code.IsEmpty())
			{
				Object->SetStringField(TEXT("code"), Diagnostic.Code);
			}
			Object->SetStringField(TEXT("severity"), Diagnostic.Severity);
			if (!Diagnostic.File.IsEmpty())
			{
				Object->SetStringField(TEXT("file"), Diagnostic.File);
			}
			if (Diagnostic.Line > 0)
			{
				Object->SetNumberField(TEXT("line"), Diagnostic.Line);
				Object->SetNumberField(TEXT("column"), FMath::Max(1, Diagnostic.Column));
			}
			if (Diagnostic.Length > 0)
			{
				Object->SetNumberField(TEXT("length"), Diagnostic.Length);
			}
			Object->SetStringField(TEXT("message"), Diagnostic.Message);
			if (!Diagnostic.Hint.IsEmpty())
			{
				Object->SetStringField(TEXT("hint"), Diagnostic.Hint);
			}
			if (!Diagnostic.Stage.IsEmpty())
			{
				Object->SetStringField(TEXT("stage"), Diagnostic.Stage);
			}
			if (!Diagnostic.Doc.IsEmpty())
			{
				Object->SetStringField(TEXT("doc"), Diagnostic.Doc);
			}
			DiagnosticValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("diagnostics"), DiagnosticValues);

		TArray<TSharedPtr<FJsonValue>> WriteValues;
		for (const FWrite& Write : Writes)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("asset"), Write.Asset);
			Object->SetStringField(TEXT("state"), Write.State);
			WriteValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("writes"), WriteValues);

		TArray<TSharedPtr<FJsonValue>> ArtifactValues;
		for (const FArtifact& Artifact : Artifacts)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("kind"), Artifact.Kind);
			Object->SetStringField(TEXT("path"), Artifact.Path);
			Object->SetStringField(TEXT("title"), Artifact.Title);
			ArtifactValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("artifacts"), ArtifactValues);

		Root->SetObjectField(TEXT("data"), Data.IsValid() ? Data : MakeShared<FJsonObject>());

		TArray<TSharedPtr<FJsonValue>> NextValues;
		for (const FString& Step : Next)
		{
			NextValues.Add(MakeShared<FJsonValueString>(Step));
		}
		Root->SetArrayField(TEXT("next"), NextValues);
		return Root;
	}

	FString SerializeJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Object, Writer);
		return Text;
	}

	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			return nullptr;
		}
		return Object;
	}

	FString MakeProjectRelativePath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return FString();
		}

		FString Absolute = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Absolute);
		FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(ProjectDirectory);

		FString Relative = Absolute;
		if (!FPaths::MakePathRelativeTo(Relative, *(ProjectDirectory + TEXT("/"))))
		{
			// A path on another drive has no relative spelling. Its file name is all that may leave the process.
			Relative = FPaths::GetCleanFilename(Absolute);
		}
		Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Relative;
	}

	FString ResolveProjectFileArgument(const FString& Argument)
	{
		FString Value = Argument.TrimStartAndEnd();
		Value.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (Value.IsEmpty())
		{
			return FString();
		}
		if (!FPaths::IsRelative(Value))
		{
			return UE::DreamShader::NormalizeSourceFilePath(Value);
		}

		const FString UnderProject = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(FPaths::ProjectDir(), Value));
		if (IFileManager::Get().FileExists(*UnderProject))
		{
			return UnderProject;
		}
		const FString UnderSourceDirectory = UE::DreamShader::NormalizeSourceFilePath(
			FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), Value));
		if (IFileManager::Get().FileExists(*UnderSourceDirectory))
		{
			return UnderSourceDirectory;
		}
		return UnderProject;
	}

	FString MakeDiagnosticDocUrl(const FString& Code)
	{
		// DSHnnnn, and nothing else: the page is the code's thousand (DSH7xxx), the anchor the code itself.
		if (Code.Len() != 7 || !Code.StartsWith(TEXT("DSH"), ESearchCase::CaseSensitive))
		{
			return FString();
		}
		for (int32 Index = 3; Index < 7; ++Index)
		{
			if (!FChar::IsDigit(Code[Index]))
			{
				return FString();
			}
		}

		FString Site = TEXT("https://shader.toolchain.64hz.cn/");
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
		{
			if (!Plugin->GetDescriptor().DocsURL.IsEmpty())
			{
				Site = Plugin->GetDescriptor().DocsURL;
			}
		}
		if (!Site.EndsWith(TEXT("/")))
		{
			Site += TEXT("/");
		}
		const FString Lower = Code.ToLower();
		return FString::Printf(TEXT("%sen/docs/plugin/diagnostics/%sxxx/#%s"), *Site, *Lower.Left(4), *Lower); // I18N-EXEMPT: a URL
	}

	FString ScrubAbsolutePaths(const FString& Text)
	{
		FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(ProjectDirectory);
		FString Scrubbed = Text.Replace(*(ProjectDirectory + TEXT("/")), TEXT(""), ESearchCase::IgnoreCase);
		FString Backslashed = ProjectDirectory + TEXT("/");
		Backslashed.ReplaceInline(TEXT("/"), TEXT("\\"));
		return Scrubbed.Replace(*Backslashed, TEXT(""), ESearchCase::IgnoreCase);
	}

	FDiagnostic MakeDiagnostic(const Private::FDreamShaderDiagnosticRecord& Record, const int32 Length, const FString& FallbackFile)
	{
		FDiagnostic Diagnostic;
		Diagnostic.Code = Record.Code;
		Diagnostic.Severity = Record.Severity.IsEmpty() ? FString(TEXT("error")) : Record.Severity.ToLower();
		Diagnostic.File = MakeProjectRelativePath(Record.FilePath.IsEmpty() ? FallbackFile : Record.FilePath);
		Diagnostic.Line = Record.Line;
		Diagnostic.Column = Record.Column;
		Diagnostic.Length = Length;
		Diagnostic.Message = ScrubAbsolutePaths(Private::ToInvariantWireString(Record.Message));
		// A record's detail is often the located wire line again -- `<file>(<line>,<col>): DSHnnnn: <message>` -- which
		// says nothing the fields do not, and names the file absolutely. Only a detail that adds something is a hint.
		const FString Detail = ScrubAbsolutePaths(Private::ToInvariantWireString(Record.Detail));
		Diagnostic.Hint = Detail.EndsWith(Diagnostic.Message) ? FString() : Detail;
		Diagnostic.Stage = Record.Stage;
		Diagnostic.Doc = MakeDiagnosticDocUrl(Record.Code);
		return Diagnostic;
	}

	void AppendDiagnostics(TArray<FDiagnostic>& Out, const TArray<Compiler::FLang2DiagnosticRecord>& Records, const FString& FallbackFile)
	{
		for (const Compiler::FLang2DiagnosticRecord& Record : Records)
		{
			Out.Add(MakeDiagnostic(Record.Record, Record.Length, FallbackFile));
		}
	}

	void AppendDiagnostics(TArray<FDiagnostic>& Out, const TArray<Private::FDreamShaderDiagnosticRecord>& Records, const FString& FallbackFile)
	{
		for (const Private::FDreamShaderDiagnosticRecord& Record : Records)
		{
			Out.Add(MakeDiagnostic(Record, 0, FallbackFile));
		}
	}

	void AppendDiagnostics(TArray<FDiagnostic>& Out, const UE::DreamShader::Lang::FLangDiagnosticSink& Sink, const FString& SourceFile)
	{
		TArray<Compiler::FLang2DiagnosticRecord> Records;
		Compiler::BuildLang2DiagnosticRecords(Sink, SourceFile, Records);
		AppendDiagnostics(Out, Records, SourceFile);
	}

	int32 CountSeverity(const TArray<FDiagnostic>& Diagnostics, const TCHAR* Severity)
	{
		int32 Count = 0;
		for (const FDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.Severity.Equals(Severity, ESearchCase::IgnoreCase))
			{
				++Count;
			}
		}
		return Count;
	}
}
