#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderDiagnosticRecord
	{
		FString FilePath;
		FText Message;
		FText Detail;
		FString Stage;
		FString AssetPath;
		FString ShaderPlatform;
		FString QualityLevel;
		FString Code;
		int32 Line = 1;
		int32 Column = 1;
		FString Severity = TEXT("error");
		FString Source = TEXT("DreamShader");
		FString OwnerSourceFilePath;
	};

	struct FDreamShaderDiagnosticLocation
	{
		FString FilePath;
		FText Message;
		int32 Line = 1;
		int32 Column = 1;
	};

	class FDreamShaderDiagnosticsStore
	{
	public:
		void Reset();
		void SetDiagnostics(const FString& SourceFilePath, TArray<FDreamShaderDiagnosticRecord>&& Diagnostics);
		void ClearDiagnostics(const FString& SourceFilePath);
		const TArray<FDreamShaderDiagnosticRecord>* FindDiagnostics(const FString& SourceFilePath) const;
		void WriteToFile(const FString& OutputFilePath) const;
		void WriteToDirectory(const FString& OutputDirectory) const;
		void WriteToDatabase(const FString& DatabaseFilePath) const;

		static bool TryParseErrorLocation(const FString& Line, FDreamShaderDiagnosticLocation& OutLocation);
		static TArray<FDreamShaderDiagnosticRecord> BuildGenerateErrorDiagnostics(
			const FString& SourceFilePath,
			const FText& ErrorMessage);
		static TArray<FDreamShaderDiagnosticRecord> BuildGenerateErrorDiagnostics(
			const FString& SourceFilePath,
			const FString& ErrorMessage)
		{
			return BuildGenerateErrorDiagnostics(SourceFilePath, FText::FromString(ErrorMessage));
		}

	private:
		TMap<FString, TArray<FDreamShaderDiagnosticRecord>> DiagnosticsByFile;
	};
}
