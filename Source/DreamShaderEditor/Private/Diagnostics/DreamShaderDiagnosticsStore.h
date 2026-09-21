#pragma once

#include "CoreMinimal.h"

// FDreamShaderDiagnosticRecord / FDreamShaderDiagnosticLocation. They live in the compiler module since
// the M4 relocation, so the compiler can build records without depending on this store.
#include "DreamShaderDiagnosticRecord.h"

namespace UE::DreamShader::Editor::Private
{
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
