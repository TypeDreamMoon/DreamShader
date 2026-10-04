#pragma once

#include "CoreMinimal.h"

// FDreamShaderDiagnosticRecord / FDreamShaderDiagnosticLocation. They live in the compiler module since
// the compiler relocation, so the compiler can build records without depending on this store.
#include "DreamShaderDiagnosticRecord.h"

namespace UE::DreamShader::Editor::Private
{
	/**
	 * Which run of a source filed a set of records. A record belongs to the source whose run produced it and to that
	 * run: a run replaces what the same run of the same source filed before, wherever it filed it, and nothing else.
	 * So recompiling A.dsm never touches what B.dsm filed against a header both include, and a material's shader
	 * compile, which finishes ticks after the compile that generated the material, never erases that compile's warnings.
	 */
	enum class EDreamShaderDiagnosticsProducer : uint8
	{
		/** The compile of the source: its own records, or the generate-error fallback. */
		Compile,
		/** The shader compile of one material the source generated. The scope is the material's object path. */
		MaterialCompile,
		/** The startup VirtualFunction declaration scan of the source. */
		VirtualFunctionSync,
	};

	class FDreamShaderDiagnosticsStore
	{
	public:
		void Reset();
		/**
		 * Replaces what this producer last filed for SourceFilePath, against the source or any other file, with
		 * Diagnostics; an empty array only removes. A record with an empty FilePath is filed against SourceFilePath.
		 * What another source or another producer filed stays, against the same files too.
		 */
		void SetDiagnostics(
			const FString& SourceFilePath,
			EDreamShaderDiagnosticsProducer Producer,
			TArray<FDreamShaderDiagnosticRecord>&& Diagnostics,
			const FString& Scope = FString());
		/**
		 * The file is gone: removes what it owns, from every producer, and every record filed against it, whoever
		 * filed it, since there is no file left to show those in.
		 */
		void ClearDiagnostics(const FString& SourceFilePath);
		/** Every record filed against FilePath, whoever owns it. FilePath is empty in these: the key is the file. */
		const TArray<FDreamShaderDiagnosticRecord>* FindDiagnostics(const FString& FilePath) const;
		/** What one producer filed for SourceFilePath, each record's FilePath naming the file it is filed against. */
		const TArray<FDreamShaderDiagnosticRecord>* FindOwnedDiagnostics(
			const FString& SourceFilePath,
			EDreamShaderDiagnosticsProducer Producer,
			const FString& Scope = FString()) const;
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
		/** One run of one source. Held here rather than in the record, which the compiler module builds too. */
		struct FOwner
		{
			FString SourceFilePath;
			EDreamShaderDiagnosticsProducer Producer = EDreamShaderDiagnosticsProducer::Compile;
			FString Scope;

			bool operator==(const FOwner& Other) const
			{
				return Producer == Other.Producer && SourceFilePath == Other.SourceFilePath && Scope == Other.Scope;
			}

			friend uint32 GetTypeHash(const FOwner& Owner)
			{
				return HashCombineFast(
					HashCombineFast(GetTypeHash(Owner.SourceFilePath), GetTypeHash(Owner.Scope)),
					static_cast<uint32>(Owner.Producer));
			}
		};

		/** DiagnosticsByFile again from DiagnosticsByOwner, after every change to it. */
		void RebuildDiagnosticsByFile();

		/** What each run filed, each record's FilePath set. The truth; DiagnosticsByFile is derived from it. */
		TMap<FOwner, TArray<FDreamShaderDiagnosticRecord>> DiagnosticsByOwner;
		/** The same records keyed by the file they are filed against: what the wire files publish. */
		TMap<FString, TArray<FDreamShaderDiagnosticRecord>> DiagnosticsByFile;
	};
}
