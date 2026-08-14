#include "DreamShaderVirtualFunctionSyncService.h"

#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"

#include "Materials/MaterialFunction.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "DreamShaderVirtualFunctionSyncService"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		bool IsIdentifierCharacter(TCHAR Character)
		{
			return FChar::IsAlnum(Character) || Character == TCHAR('_');
		}

		bool IsValidStringIndex(const FString& Text, int32 Index)
		{
			return Index >= 0 && Index < Text.Len();
		}

		bool IsKeywordAt(const FString& Text, int32 Index, const TCHAR* Keyword)
		{
			const int32 KeywordLength = FCString::Strlen(Keyword);
			if (Index < 0 || Index + KeywordLength > Text.Len())
			{
				return false;
			}

			if (Index > 0 && IsIdentifierCharacter(Text[Index - 1]))
			{
				return false;
			}

			if (Index + KeywordLength < Text.Len() && IsIdentifierCharacter(Text[Index + KeywordLength]))
			{
				return false;
			}

			return Text.Mid(Index, KeywordLength).Equals(Keyword, ESearchCase::CaseSensitive);
		}

		void SkipQuotedString(const FString& Text, int32& Index)
		{
			if (!IsValidStringIndex(Text, Index) || (Text[Index] != TCHAR('"') && Text[Index] != TCHAR('\'')))
			{
				return;
			}

			const TCHAR Quote = Text[Index];
			++Index;
			bool bEscaped = false;
			while (Index < Text.Len())
			{
				const TCHAR Character = Text[Index++];
				if (bEscaped)
				{
					bEscaped = false;
					continue;
				}
				if (Character == TCHAR('\\'))
				{
					bEscaped = true;
					continue;
				}
				if (Character == Quote)
				{
					return;
				}
			}
		}

		bool TrySkipComment(const FString& Text, int32& Index)
		{
			if (Index + 1 >= Text.Len() || Text[Index] != TCHAR('/'))
			{
				return false;
			}

			if (Text[Index + 1] == TCHAR('/'))
			{
				Index += 2;
				while (Index < Text.Len() && Text[Index] != TCHAR('\n'))
				{
					++Index;
				}
				return true;
			}

			if (Text[Index + 1] == TCHAR('*'))
			{
				Index += 2;
				while (Index + 1 < Text.Len())
				{
					if (Text[Index] == TCHAR('*') && Text[Index + 1] == TCHAR('/'))
					{
						Index += 2;
						return true;
					}
					++Index;
				}
				Index = Text.Len();
				return true;
			}

			return false;
		}

		void SkipIgnoredText(const FString& Text, int32& Index)
		{
			while (Index < Text.Len())
			{
				if (FChar::IsWhitespace(Text[Index]))
				{
					++Index;
					continue;
				}
				if (TrySkipComment(Text, Index))
				{
					continue;
				}
				return;
			}
		}

		bool TryExtractBalancedRange(
			const FString& Text,
			int32 OpenIndex,
			TCHAR OpenCharacter,
			TCHAR CloseCharacter,
			int32& OutEndIndex)
		{
			OutEndIndex = INDEX_NONE;
			if (!IsValidStringIndex(Text, OpenIndex) || Text[OpenIndex] != OpenCharacter)
			{
				return false;
			}

			int32 Depth = 0;
			int32 Index = OpenIndex;
			while (Index < Text.Len())
			{
				if (Text[Index] == TCHAR('"') || Text[Index] == TCHAR('\''))
				{
					SkipQuotedString(Text, Index);
					continue;
				}
				if (TrySkipComment(Text, Index))
				{
					continue;
				}

				if (Text[Index] == OpenCharacter)
				{
					++Depth;
				}
				else if (Text[Index] == CloseCharacter)
				{
					--Depth;
					if (Depth == 0)
					{
						OutEndIndex = Index + 1;
						return true;
					}
				}
				++Index;
			}

			return false;
		}

		void CalculateLineColumnForIndex(const FString& Text, int32 Position, int32& OutLine, int32& OutColumn)
		{
			OutLine = 1;
			OutColumn = 1;
			const int32 ClampedPosition = FMath::Clamp(Position, 0, Text.Len());
			for (int32 Index = 0; Index < ClampedPosition; ++Index)
			{
				if (Text[Index] == TCHAR('\n'))
				{
					++OutLine;
					OutColumn = 1;
				}
				else if (Text[Index] != TCHAR('\r'))
				{
					++OutColumn;
				}
			}
		}

		FString NormalizeVirtualFunctionDefinitionText(FString Text)
		{
			Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
			Text.ReplaceInline(TEXT("\r"), TEXT("\n"));
			Text.TrimStartAndEndInline();
			return Text;
		}

		FDreamShaderDiagnosticRecord MakeVirtualFunctionDiagnostic(
			const FString& SourceFilePath,
			const FText& Message,
			const FText& Detail,
			const FString& AssetPath,
			int32 Line,
			int32 Column)
		{
			FDreamShaderDiagnosticRecord Diagnostic;
			Diagnostic.FilePath = SourceFilePath;
			Diagnostic.Message = Message;
			Diagnostic.Detail = Detail;
			Diagnostic.Stage = TEXT("virtualFunctionSync");
			Diagnostic.AssetPath = AssetPath;
			Diagnostic.Code = TEXT("virtual-function-sync");
			Diagnostic.Line = FMath::Max(1, Line);
			Diagnostic.Column = FMath::Max(1, Column);
			Diagnostic.Source = TEXT("DreamShader VirtualFunction");
			return Diagnostic;
		}

		bool TryParseVirtualFunctionBlock(
			const FString& BlockText,
			FTextShaderVirtualFunctionDefinition& OutFunction,
			FText& OutError)
		{
			FTextShaderDefinition ParsedDefinition;
			FText ParseError;
			if (!FTextShaderParser::Parse(BlockText, ParsedDefinition, ParseError))
			{
				OutError = ParseError;
				return false;
			}

			if (ParsedDefinition.VirtualFunctions.Num() != 1)
			{
				OutError = FText::FromString(TEXT("Expected exactly one VirtualFunction block."));
				return false;
			}

			OutFunction = ParsedDefinition.VirtualFunctions[0];
			return true;
		}

		void CollectDefinitionLocationsFromFile(
			const FString& SourceFilePath,
			TArray<FDreamShaderVirtualFunctionDefinitionLocation>& OutLocations,
			FString* OutSourceText = nullptr,
			TArray<FDreamShaderDiagnosticRecord>* OutDiagnostics = nullptr)
		{
			OutLocations.Reset();

			FString SourceText;
			if (!FFileHelper::LoadFileToString(SourceText, *SourceFilePath))
			{
				if (OutDiagnostics)
				{
					OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
						SourceFilePath,
						FText::Format(
							LOCTEXT("ReadSourceFileFailed", "DreamShader could not read VirtualFunction source file '{0}'."),
							FText::FromString(SourceFilePath)),
						FText(),
						FString(),
						1,
						1));
				}
				return;
			}

			if (OutSourceText)
			{
				*OutSourceText = SourceText;
			}

			int32 Index = 0;
			while (Index < SourceText.Len())
			{
				if (SourceText[Index] == TCHAR('"') || SourceText[Index] == TCHAR('\''))
				{
					SkipQuotedString(SourceText, Index);
					continue;
				}
				if (TrySkipComment(SourceText, Index))
				{
					continue;
				}
				if (!IsKeywordAt(SourceText, Index, TEXT("VirtualFunction")))
				{
					++Index;
					continue;
				}

				const int32 StartIndex = Index;
				Index += FCString::Strlen(TEXT("VirtualFunction"));
				SkipIgnoredText(SourceText, Index);
				if (!IsValidStringIndex(SourceText, Index) || SourceText[Index] != TCHAR('('))
				{
					Index = StartIndex + 1;
					continue;
				}

				int32 AttributesEndIndex = INDEX_NONE;
				if (!TryExtractBalancedRange(SourceText, Index, TCHAR('('), TCHAR(')'), AttributesEndIndex))
				{
					int32 Line = 1;
					int32 Column = 1;
					CalculateLineColumnForIndex(SourceText, StartIndex, Line, Column);
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							LOCTEXT("MissingClosingParenthesis", "VirtualFunction attributes are missing a closing ')'."),
							FText(),
							FString(),
							Line,
							Column));
					}
					Index = StartIndex + 1;
					continue;
				}

				Index = AttributesEndIndex;
				SkipIgnoredText(SourceText, Index);
				if (!IsValidStringIndex(SourceText, Index) || SourceText[Index] != TCHAR('{'))
				{
					Index = StartIndex + 1;
					continue;
				}

				int32 BodyEndIndex = INDEX_NONE;
				if (!TryExtractBalancedRange(SourceText, Index, TCHAR('{'), TCHAR('}'), BodyEndIndex))
				{
					int32 Line = 1;
					int32 Column = 1;
					CalculateLineColumnForIndex(SourceText, StartIndex, Line, Column);
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							LOCTEXT("MissingClosingBrace", "VirtualFunction body is missing a closing '}'."),
							FText(),
							FString(),
							Line,
							Column));
					}
					Index = StartIndex + 1;
					continue;
				}

				int32 EndIndex = BodyEndIndex;
				int32 AfterBodyIndex = EndIndex;
				SkipIgnoredText(SourceText, AfterBodyIndex);
				if (IsValidStringIndex(SourceText, AfterBodyIndex) && SourceText[AfterBodyIndex] == TCHAR(';'))
				{
					EndIndex = AfterBodyIndex + 1;
				}

				const FString BlockText = SourceText.Mid(StartIndex, EndIndex - StartIndex);
				int32 Line = 1;
				int32 Column = 1;
				CalculateLineColumnForIndex(SourceText, StartIndex, Line, Column);

				FTextShaderVirtualFunctionDefinition ParsedFunction;
				FText ParseError;
				if (!TryParseVirtualFunctionBlock(BlockText, ParsedFunction, ParseError))
				{
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							FText::Format(
								LOCTEXT("InvalidDeclaration", "VirtualFunction declaration is invalid: {0}"),
								ParseError),
							ParseError,
							FString(),
							Line,
							Column));
					}
					Index = EndIndex;
					continue;
				}

				FString ObjectPath;
				FString ResolveError;
				if (!TryResolveDreamShaderAssetReference(ParsedFunction.Asset, ObjectPath, ResolveError))
				{
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							FText::Format(
								LOCTEXT("InvalidAssetReference", "VirtualFunction '{0}' asset reference is invalid: {1}"),
								FText::FromString(ParsedFunction.Name),
								FText::FromString(ResolveError)),
							FText::FromString(ResolveError),
							ParsedFunction.Asset,
							Line,
							Column));
					}
					Index = EndIndex;
					continue;
				}

				FDreamShaderVirtualFunctionDefinitionLocation& Location = OutLocations.AddDefaulted_GetRef();
				Location.SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
				Location.FunctionName = ParsedFunction.Name;
				Location.AssetObjectPath = ObjectPath;
				Location.CurrentText = BlockText;
				Location.Inputs = ParsedFunction.Inputs;
				Location.Outputs = ParsedFunction.Outputs;
				Location.StartIndex = StartIndex;
				Location.EndIndex = EndIndex;
				Location.Line = Line;
				Location.Column = Column;

				Index = EndIndex;
			}
		}
	}

	bool FDreamShaderVirtualFunctionSyncService::FindDefinitionForMaterialFunction(
		const UMaterialFunction* MaterialFunction,
		FDreamShaderVirtualFunctionDefinitionLocation& OutLocation)
	{
		if (!MaterialFunction)
		{
			return false;
		}

		const FString TargetObjectPath = MaterialFunction->GetPathName();
		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);
		for (const FString& SourceFile : SourceFiles)
		{
			TArray<FDreamShaderVirtualFunctionDefinitionLocation> Locations;
			CollectDefinitionLocationsFromFile(SourceFile, Locations);
			for (const FDreamShaderVirtualFunctionDefinitionLocation& Location : Locations)
			{
				if (Location.AssetObjectPath.Equals(TargetObjectPath, ESearchCase::IgnoreCase))
				{
					OutLocation = Location;
					return true;
				}
			}
		}

		return false;
	}

	FDreamShaderVirtualFunctionSyncResult FDreamShaderVirtualFunctionSyncService::SyncDefinitions(FDefinitionBuilder DefinitionBuilder)
	{
		struct FVirtualFunctionReplacement
		{
			int32 StartIndex = INDEX_NONE;
			int32 EndIndex = INDEX_NONE;
			FString DefinitionText;
		};

		FDreamShaderVirtualFunctionSyncResult Result;

		TArray<FString> SourceFiles;
		FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);
		for (const FString& SourceFile : SourceFiles)
		{
			// Sync rewrites the file in place. A plugin ships its VirtualFunction definitions as
			// authored, so a read-only root is skipped outright rather than scanned and reported as
			// out of date on every sync.
			if (!UE::DreamShader::IsWritableSourceFilePath(SourceFile))
			{
				continue;
			}

			FString SourceText;
			TArray<FDreamShaderVirtualFunctionDefinitionLocation> Locations;
			TArray<FDreamShaderDiagnosticRecord> Diagnostics;
			CollectDefinitionLocationsFromFile(SourceFile, Locations, &SourceText, &Diagnostics);

			TArray<FVirtualFunctionReplacement> Replacements;
			for (const FDreamShaderVirtualFunctionDefinitionLocation& Location : Locations)
			{
				++Result.ScannedDefinitionCount;

				UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *Location.AssetObjectPath);
				if (!Function)
				{
					Diagnostics.Add(MakeVirtualFunctionDiagnostic(
						SourceFile,
						FText::Format(
							LOCTEXT("MissingMaterialFunction", "VirtualFunction '{0}' references missing MaterialFunction '{1}'."),
							FText::FromString(Location.FunctionName),
							FText::FromString(Location.AssetObjectPath)),
						FText(),
						Location.AssetObjectPath,
						Location.Line,
						Location.Column));
					continue;
				}

				FString GeneratedDefinition;
				FString Error;
				if (!DefinitionBuilder(Function, GeneratedDefinition, Error))
				{
					Diagnostics.Add(MakeVirtualFunctionDiagnostic(
						SourceFile,
						FText::Format(
							LOCTEXT("RefreshFailed", "VirtualFunction '{0}' could not be refreshed from MaterialFunction '{1}': {2}"),
							FText::FromString(Location.FunctionName),
							FText::FromString(Location.AssetObjectPath),
							FText::FromString(Error)),
						FText::FromString(Error),
						Location.AssetObjectPath,
						Location.Line,
						Location.Column));
					continue;
				}

				if (NormalizeVirtualFunctionDefinitionText(Location.CurrentText)
					!= NormalizeVirtualFunctionDefinitionText(GeneratedDefinition))
				{
					FVirtualFunctionReplacement& Replacement = Replacements.AddDefaulted_GetRef();
					Replacement.StartIndex = Location.StartIndex;
					Replacement.EndIndex = Location.EndIndex;
					Replacement.DefinitionText = GeneratedDefinition;
				}
			}

			if (!Replacements.IsEmpty())
			{
				FString UpdatedSourceText = SourceText;
				Replacements.Sort([](const FVirtualFunctionReplacement& A, const FVirtualFunctionReplacement& B)
				{
					return A.StartIndex > B.StartIndex;
				});

				for (const FVirtualFunctionReplacement& Replacement : Replacements)
				{
					UpdatedSourceText =
						UpdatedSourceText.Left(Replacement.StartIndex)
						+ Replacement.DefinitionText
						+ UpdatedSourceText.Mid(Replacement.EndIndex);
				}

				if (!FFileHelper::SaveStringToFile(UpdatedSourceText, *SourceFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					Diagnostics.Add(MakeVirtualFunctionDiagnostic(
						SourceFile,
						FText::Format(
							LOCTEXT("UpdateSourceFileFailed", "DreamShader failed to update VirtualFunction source file '{0}'."),
							FText::FromString(SourceFile)),
						FText(),
						FString(),
						1,
						1));
				}
				else
				{
					Result.UpdatedDefinitionCount += Replacements.Num();
				}
			}

			Result.ErrorCount += Diagnostics.Num();

			if (!Locations.IsEmpty() || !Diagnostics.IsEmpty())
			{
				FDreamShaderVirtualFunctionSyncFileResult& FileResult = Result.Files.AddDefaulted_GetRef();
				FileResult.SourceFilePath = SourceFile;
				FileResult.DefinitionCount = Locations.Num();
				FileResult.UpdatedDefinitionCount = Replacements.Num();
				FileResult.Diagnostics = MoveTemp(Diagnostics);
			}
		}

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
