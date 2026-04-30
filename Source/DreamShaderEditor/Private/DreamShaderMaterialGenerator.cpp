#include "DreamShaderMaterialGenerator.h"

#include "DreamShaderMaterialGeneratorPrivate.h"

#include "DreamShaderModule.h"
#include "DreamShaderParser.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialFunction.h"
#include "Misc/FileHelper.h"

namespace UE::DreamShader::Editor
{
	namespace
	{
		static bool IsIdentifierBoundary(const FString& Text, const int32 Index)
		{
			if (!Text.IsValidIndex(Index))
			{
				return true;
			}

			const TCHAR Char = Text[Index];
			return !(FChar::IsAlnum(Char) || Char == TCHAR('_'));
		}

		static bool TryConsumeKeywordAt(const FString& Text, const int32 Index, const TCHAR* Keyword)
		{
			const int32 KeywordLength = FCString::Strlen(Keyword);
			if (!Text.Mid(Index, KeywordLength).Equals(Keyword, ESearchCase::CaseSensitive))
			{
				return false;
			}

			return IsIdentifierBoundary(Text, Index - 1) && IsIdentifierBoundary(Text, Index + KeywordLength);
		}

		static bool TryParseFunctionInputPreviewLiteral(
			const FString& InText,
			const int32 ComponentCount,
			FVector4f& OutPreviewValue)
		{
			if (ComponentCount <= 1)
			{
				double ScalarValue = 0.0;
				if (!Private::ParseScalarLiteral(InText, ScalarValue))
				{
					return false;
				}

				const float Value = static_cast<float>(ScalarValue);
				OutPreviewValue = FVector4f(Value, Value, Value, Value);
				return true;
			}

			FString Candidate = InText.TrimStartAndEnd();
			const int32 OpenParenIndex = Candidate.Find(TEXT("("));
			const int32 CloseParenIndex = Candidate.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (OpenParenIndex == INDEX_NONE || CloseParenIndex == INDEX_NONE || CloseParenIndex <= OpenParenIndex)
			{
				return false;
			}

			const FString ValueBlock = Candidate.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1);
			TArray<FString> Parts;
			ValueBlock.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.IsEmpty() || Parts.Num() > 4)
			{
				return false;
			}

			float Parsed[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			for (int32 Index = 0; Index < Parts.Num(); ++Index)
			{
				double ParsedValue = 0.0;
				if (!Private::ParseScalarLiteral(Parts[Index], ParsedValue))
				{
					return false;
				}
				Parsed[Index] = static_cast<float>(ParsedValue);
			}

			if (Parts.Num() == 1)
			{
				Parsed[1] = Parsed[0];
				Parsed[2] = Parsed[0];
				Parsed[3] = Parsed[0];
			}

			OutPreviewValue = FVector4f(Parsed[0], Parsed[1], Parsed[2], Parsed[3]);
			return true;
		}

		static bool ApplyFunctionInputPreviewDefault(
			UMaterialFunction* MaterialFunction,
			const FString& SourceFilePath,
			const FTextShaderDefinition& RootDefinition,
			const FTextShaderFunctionParameter& InputDefinition,
			UMaterialExpressionFunctionInput* InputExpression,
			const int32 ComponentCount,
			const bool bIsTextureObject,
			TMap<FString, Private::FCodeValue>& GeneratedValues,
			FString& OutError)
		{
			if (!InputExpression || (!InputDefinition.bOptional && !InputDefinition.bHasDefaultValue))
			{
				return true;
			}

			InputExpression->bUsePreviewValueAsDefault = InputDefinition.bOptional ? 1U : 0U;
			if (!InputDefinition.bHasDefaultValue)
			{
				return true;
			}

			if (bIsTextureObject)
			{
				OutError = FString::Printf(TEXT("Input '%s' texture defaults must be connected through the FunctionInput Preview pin in Unreal for now."), *InputDefinition.Name);
				return false;
			}

			FVector4f PreviewValue;
			if (TryParseFunctionInputPreviewLiteral(InputDefinition.DefaultValueText, ComponentCount, PreviewValue))
			{
				InputExpression->PreviewValue = PreviewValue;
				return true;
			}

			Private::FCodeGraphBuilder PreviewGraphBuilder(
				nullptr,
				MaterialFunction,
				RootDefinition,
				SourceFilePath,
				Private::BuildGeneratedIncludeVirtualPath(SourceFilePath));
			TArray<Private::FCodeStatement> EmptyStatements;
			FString BuildError;
			if (!PreviewGraphBuilder.Build(EmptyStatements, GeneratedValues, BuildError))
			{
				OutError = BuildError;
				return false;
			}

			Private::FCodeValue PreviewExpressionValue;
			if (!PreviewGraphBuilder.EvaluateOutputExpression(InputDefinition.DefaultValueText, PreviewExpressionValue, OutError))
			{
				return false;
			}

			if (PreviewExpressionValue.bIsTextureObject || PreviewExpressionValue.ComponentCount != ComponentCount)
			{
				OutError = FString::Printf(
					TEXT("Input '%s' default expression '%s' does not match declared type '%s'."),
					*InputDefinition.Name,
					*InputDefinition.DefaultValueText,
					*InputDefinition.Type);
				return false;
			}

			InputExpression->Preview.Connect(PreviewExpressionValue.OutputIndex, PreviewExpressionValue.Expression);
			return true;
		}

		static bool FindMatchingDelimiter(
			const FString& Text,
			const int32 OpenIndex,
			const TCHAR OpenChar,
			const TCHAR CloseChar,
			int32& OutCloseIndex)
		{
			if (!Text.IsValidIndex(OpenIndex) || Text[OpenIndex] != OpenChar)
			{
				return false;
			}

			int32 Depth = 1;
			bool bInString = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;

			for (int32 Index = OpenIndex + 1; Index < Text.Len(); ++Index)
			{
				const TCHAR Char = Text[Index];
				const TCHAR Next = Text.IsValidIndex(Index + 1) ? Text[Index + 1] : TCHAR('\0');

				if (bInLineComment)
				{
					if (Char == TCHAR('\n'))
					{
						bInLineComment = false;
					}
					continue;
				}

				if (bInBlockComment)
				{
					if (Char == TCHAR('*') && Next == TCHAR('/'))
					{
						bInBlockComment = false;
						++Index;
					}
					continue;
				}

				if (bInString)
				{
					if (Char == TCHAR('\\') && Text.IsValidIndex(Index + 1))
					{
						++Index;
					}
					else if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					continue;
				}

				if (Char == TCHAR('"'))
				{
					bInString = true;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('/'))
				{
					bInLineComment = true;
					++Index;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('*'))
				{
					bInBlockComment = true;
					++Index;
					continue;
				}

				if (Char == OpenChar)
				{
					++Depth;
				}
				else if (Char == CloseChar)
				{
					--Depth;
					if (Depth == 0)
					{
						OutCloseIndex = Index;
						return true;
					}
				}
			}

			return false;
		}

		static TArray<FString> SplitTopLevelParameters(const FString& ParameterBlock)
		{
			TArray<FString> Parameters;
			FString Current;
			int32 ParenthesisDepth = 0;
			bool bInString = false;

			for (int32 Index = 0; Index < ParameterBlock.Len(); ++Index)
			{
				const TCHAR Char = ParameterBlock[Index];

				if (bInString)
				{
					Current.AppendChar(Char);
					if (Char == TCHAR('\\') && ParameterBlock.IsValidIndex(Index + 1))
					{
						Current.AppendChar(ParameterBlock[++Index]);
					}
					else if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					continue;
				}

				if (Char == TCHAR('"'))
				{
					bInString = true;
					Current.AppendChar(Char);
					continue;
				}

				if (Char == TCHAR('('))
				{
					++ParenthesisDepth;
					Current.AppendChar(Char);
					continue;
				}

				if (Char == TCHAR(')'))
				{
					ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
					Current.AppendChar(Char);
					continue;
				}

				if (Char == TCHAR(',') && ParenthesisDepth == 0)
				{
					Current.TrimStartAndEndInline();
					if (!Current.IsEmpty())
					{
						Parameters.Add(Current);
					}
					Current.Reset();
					continue;
				}

				Current.AppendChar(Char);
			}

			Current.TrimStartAndEndInline();
			if (!Current.IsEmpty())
			{
				Parameters.Add(Current);
			}

			return Parameters;
		}

		static FString NormalizeShaderTypeToken(const FString& InTypeToken)
		{
			FString TypeToken = InTypeToken.TrimStartAndEnd();
			FString Lower = TypeToken;
			Lower.ToLowerInline();

			if (Lower == TEXT("vec2")) return TEXT("float2");
			if (Lower == TEXT("vec3")) return TEXT("float3");
			if (Lower == TEXT("vec4")) return TEXT("float4");
			if (Lower == TEXT("ivec2")) return TEXT("int2");
			if (Lower == TEXT("ivec3")) return TEXT("int3");
			if (Lower == TEXT("ivec4")) return TEXT("int4");
			if (Lower == TEXT("uvec2")) return TEXT("uint2");
			if (Lower == TEXT("uvec3")) return TEXT("uint3");
			if (Lower == TEXT("uvec4")) return TEXT("uint4");
			if (Lower == TEXT("bvec2")) return TEXT("bool2");
			if (Lower == TEXT("bvec3")) return TEXT("bool3");
			if (Lower == TEXT("bvec4")) return TEXT("bool4");
			if (Lower == TEXT("mat2")) return TEXT("float2x2");
			if (Lower == TEXT("mat3")) return TEXT("float3x3");
			if (Lower == TEXT("mat4")) return TEXT("float4x4");

			return TypeToken;
		}

		static FString NormalizeShaderLanguageText(const FString& InCode)
		{
			static const TMap<FString, FString> IdentifierMap = {
				{ TEXT("vec2"), TEXT("float2") },
				{ TEXT("vec3"), TEXT("float3") },
				{ TEXT("vec4"), TEXT("float4") },
				{ TEXT("ivec2"), TEXT("int2") },
				{ TEXT("ivec3"), TEXT("int3") },
				{ TEXT("ivec4"), TEXT("int4") },
				{ TEXT("uvec2"), TEXT("uint2") },
				{ TEXT("uvec3"), TEXT("uint3") },
				{ TEXT("uvec4"), TEXT("uint4") },
				{ TEXT("bvec2"), TEXT("bool2") },
				{ TEXT("bvec3"), TEXT("bool3") },
				{ TEXT("bvec4"), TEXT("bool4") },
				{ TEXT("mat2"), TEXT("float2x2") },
				{ TEXT("mat3"), TEXT("float3x3") },
				{ TEXT("mat4"), TEXT("float4x4") },
				{ TEXT("mix"), TEXT("lerp") },
				{ TEXT("fract"), TEXT("frac") },
				{ TEXT("mod"), TEXT("fmod") }
			};

			FString OutCode;
			OutCode.Reserve(InCode.Len() + 32);

			bool bInString = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;

			for (int32 Index = 0; Index < InCode.Len();)
			{
				const TCHAR Char = InCode[Index];
				const TCHAR Next = InCode.IsValidIndex(Index + 1) ? InCode[Index + 1] : TCHAR('\0');

				if (bInLineComment)
				{
					OutCode.AppendChar(Char);
					if (Char == TCHAR('\n'))
					{
						bInLineComment = false;
					}
					++Index;
					continue;
				}

				if (bInBlockComment)
				{
					OutCode.AppendChar(Char);
					if (Char == TCHAR('*') && Next == TCHAR('/'))
					{
						OutCode.AppendChar(Next);
						bInBlockComment = false;
						Index += 2;
					}
					else
					{
						++Index;
					}
					continue;
				}

				if (bInString)
				{
					OutCode.AppendChar(Char);
					if (Char == TCHAR('\\') && InCode.IsValidIndex(Index + 1))
					{
						OutCode.AppendChar(InCode[Index + 1]);
						Index += 2;
					}
					else
					{
						if (Char == TCHAR('"'))
						{
							bInString = false;
						}
						++Index;
					}
					continue;
				}

				if (Char == TCHAR('"'))
				{
					bInString = true;
					OutCode.AppendChar(Char);
					++Index;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('/'))
				{
					bInLineComment = true;
					OutCode.AppendChar(Char);
					OutCode.AppendChar(Next);
					Index += 2;
					continue;
				}

				if (Char == TCHAR('/') && Next == TCHAR('*'))
				{
					bInBlockComment = true;
					OutCode.AppendChar(Char);
					OutCode.AppendChar(Next);
					Index += 2;
					continue;
				}

				if (FChar::IsAlpha(Char) || Char == TCHAR('_'))
				{
					const int32 Start = Index++;
					while (InCode.IsValidIndex(Index) && (FChar::IsAlnum(InCode[Index]) || InCode[Index] == TCHAR('_')))
					{
						++Index;
					}

					const FString Identifier = InCode.Mid(Start, Index - Start);
					if (const FString* Replacement = IdentifierMap.Find(Identifier.ToLower()))
					{
						OutCode += *Replacement;
					}
					else
					{
						OutCode += Identifier;
					}
					continue;
				}

				OutCode.AppendChar(Char);
				++Index;
			}

			return OutCode;
		}

		static bool ParseModernFunctionSignature(
			const FString& FunctionName,
			const FString& ParameterBlock,
			FString& OutInputsBlock,
			FString& OutResultsBlock,
			FString& OutError)
		{
			OutInputsBlock.Reset();
			OutResultsBlock.Reset();

			int32 ResultCount = 0;
			for (const FString& RawParameter : SplitTopLevelParameters(ParameterBlock))
			{
				const FString Parameter = RawParameter.TrimStartAndEnd();
				if (Parameter.IsEmpty())
				{
					continue;
				}

				TArray<FString> Parts;
				Parameter.ParseIntoArrayWS(Parts);
				if (Parts.Num() < 2 || Parts.Num() > 3)
				{
					OutError = FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter);
					return false;
				}

				FString Qualifier = TEXT("in");
				FString TypeToken;
				FString NameToken;
				if (Parts.Num() == 2)
				{
					TypeToken = Parts[0];
					NameToken = Parts[1];
				}
				else
				{
					Qualifier = Parts[0];
					TypeToken = Parts[1];
					NameToken = Parts[2];
				}

				Qualifier = Qualifier.TrimStartAndEnd();
				Qualifier.ToLowerInline();
				TypeToken = NormalizeShaderTypeToken(TypeToken);
				NameToken = NameToken.TrimStartAndEnd();

				if (!Qualifier.Equals(TEXT("in")) && !Qualifier.Equals(TEXT("out")))
				{
					OutError = FString::Printf(
						TEXT("Function '%s' parameter '%s' uses unsupported qualifier '%s'. Supported qualifiers are in and out."),
						*FunctionName,
						*Parameter,
						*Qualifier);
					return false;
				}

				if (TypeToken.IsEmpty() || NameToken.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter);
					return false;
				}

				const FString Statement = FString::Printf(TEXT("        %s %s;\n"), *TypeToken, *NameToken);
				if (Qualifier.Equals(TEXT("out")))
				{
					OutResultsBlock += Statement;
					++ResultCount;
				}
				else
				{
					OutInputsBlock += Statement;
				}
			}

			if (ResultCount == 0)
			{
				OutError = FString::Printf(TEXT("Function '%s' must declare at least one out parameter."), *FunctionName);
				return false;
			}

			return true;
		}

		static bool TransformModernFunctionSyntax(const FString& InSourceText, FString& OutSourceText, FString& OutError)
		{
			OutError.Reset();
			OutSourceText = InSourceText;
			return true;
		}

		static bool TryExtractImportPathFromLine(const FString& Line, FString& OutImportPath)
		{
			FString Trimmed = Line.TrimStartAndEnd();
			if (!Trimmed.StartsWith(TEXT("import"), ESearchCase::IgnoreCase))
			{
				return false;
			}

			const int32 ImportKeywordLength = 6;
			if (Trimmed.Len() > ImportKeywordLength
				&& !FChar::IsWhitespace(Trimmed[ImportKeywordLength]))
			{
				return false;
			}

			Trimmed.RightChopInline(ImportKeywordLength, EAllowShrinking::No);
			Trimmed.TrimStartAndEndInline();
			if (Trimmed.Len() < 2 || (Trimmed[0] != TCHAR('"') && Trimmed[0] != TCHAR('\'')))
			{
				return false;
			}

			const TCHAR Quote = Trimmed[0];
			int32 ClosingQuoteIndex = INDEX_NONE;
			bool bEscaped = false;
			for (int32 Index = 1; Index < Trimmed.Len(); ++Index)
			{
				const TCHAR Character = Trimmed[Index];
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
					ClosingQuoteIndex = Index;
					break;
				}
			}

			if (ClosingQuoteIndex == INDEX_NONE)
			{
				return false;
			}

			FString TrailingText = Trimmed.Mid(ClosingQuoteIndex + 1).TrimStartAndEnd();
			if (TrailingText.StartsWith(TEXT(";")))
			{
				TrailingText.RightChopInline(1, EAllowShrinking::No);
				TrailingText.TrimStartAndEndInline();
			}
			if (!TrailingText.IsEmpty() && !TrailingText.StartsWith(TEXT("//")))
			{
				return false;
			}

			OutImportPath = Trimmed.Mid(1, ClosingQuoteIndex - 1).TrimStartAndEnd();
			return !OutImportPath.IsEmpty();
		}

		static FString NormalizeDreamShaderImportSpecifier(const FString& InImportPath)
		{
			FString Result = InImportPath;
			Result.TrimStartAndEndInline();
			Result.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (!Result.EndsWith(TEXT(".dsh"), ESearchCase::IgnoreCase))
			{
				Result += TEXT(".dsh");
			}
			return Result;
		}

		static bool ResolveDreamShaderImportPath(
			const FString& CurrentFilePath,
			const FString& ImportSpecifier,
			FString& OutResolvedPath,
			FString& OutError)
		{
			const FString NormalizedImport = NormalizeDreamShaderImportSpecifier(ImportSpecifier);
			const FString RelativeCandidate = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(FPaths::GetPath(CurrentFilePath), NormalizedImport));
			if (IFileManager::Get().FileExists(*RelativeCandidate))
			{
				OutResolvedPath = RelativeCandidate;
				return true;
			}

			const FString RootCandidate = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), NormalizedImport));
			if (IFileManager::Get().FileExists(*RootCandidate))
			{
				OutResolvedPath = RootCandidate;
				return true;
			}

			const FString PackageCandidate = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(UE::DreamShader::GetPackageShaderDirectory(), NormalizedImport));
			if (IFileManager::Get().FileExists(*PackageCandidate))
			{
				OutResolvedPath = PackageCandidate;
				return true;
			}

			const FString BuiltinCandidate = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(UE::DreamShader::GetBuiltinShaderLibraryDirectory(), NormalizedImport));
			if (IFileManager::Get().FileExists(*BuiltinCandidate))
			{
				OutResolvedPath = BuiltinCandidate;
				return true;
			}

			OutError = FString::Printf(
				TEXT("DreamShader import '%s' referenced from '%s' could not be resolved."),
				*ImportSpecifier,
				*CurrentFilePath);
			return false;
		}

		static bool TryExtractPreparedSourceIndexFromError(const FString& Error, int32& OutIndex)
		{
			const FString Needle = TEXT("near index ");
			const int32 NeedleIndex = Error.Find(Needle, ESearchCase::IgnoreCase);
			if (NeedleIndex == INDEX_NONE)
			{
				return false;
			}

			int32 Cursor = NeedleIndex + Needle.Len();
			while (Cursor < Error.Len() && FChar::IsWhitespace(Error[Cursor]))
			{
				++Cursor;
			}

			const int32 NumberStart = Cursor;
			while (Cursor < Error.Len() && FChar::IsDigit(Error[Cursor]))
			{
				++Cursor;
			}

			if (Cursor == NumberStart)
			{
				return false;
			}

			OutIndex = FCString::Atoi(*Error.Mid(NumberStart, Cursor - NumberStart));
			return OutIndex >= 0;
		}

		static bool TryMapPreparedSourceIndexToLocation(
			const FString& PreparedSource,
			const int32 SourceIndex,
			FString& OutFilePath,
			int32& OutLine,
			int32& OutColumn)
		{
			const FString BeginMarker = TEXT("// Begin DreamShader source: ");
			const FString EndMarker = TEXT("// End DreamShader source: ");

			FString CurrentFilePath;
			int32 CurrentSourceLine = 1;
			int32 Cursor = 0;
			while (Cursor <= PreparedSource.Len())
			{
				int32 LineEnd = PreparedSource.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
				if (LineEnd == INDEX_NONE)
				{
					LineEnd = PreparedSource.Len();
				}

				const FString LineText = PreparedSource.Mid(Cursor, LineEnd - Cursor);
				if (LineText.StartsWith(BeginMarker))
				{
					CurrentFilePath = LineText.RightChop(BeginMarker.Len()).TrimStartAndEnd();
					CurrentSourceLine = 1;
				}
				else if (LineText.StartsWith(EndMarker))
				{
					CurrentFilePath.Reset();
					CurrentSourceLine = 1;
				}
				else
				{
					const int32 LineEndInclusive = LineEnd < PreparedSource.Len() ? LineEnd + 1 : LineEnd;
					if (SourceIndex >= Cursor && SourceIndex <= LineEndInclusive && !CurrentFilePath.IsEmpty())
					{
						OutFilePath = CurrentFilePath;
						OutLine = FMath::Max(1, CurrentSourceLine);
						OutColumn = FMath::Max(1, SourceIndex - Cursor + 1);
						return true;
					}

					if (!CurrentFilePath.IsEmpty())
					{
						++CurrentSourceLine;
					}
				}

				if (LineEnd >= PreparedSource.Len())
				{
					break;
				}
				Cursor = LineEnd + 1;
			}

			return false;
		}

		static FString FormatParseErrorWithSourceLocation(
			const FString& FallbackSourceFilePath,
			const FString& PreparedSource,
			const FString& ParseError)
		{
			int32 SourceIndex = INDEX_NONE;
			FString MappedFilePath;
			int32 MappedLine = 1;
			int32 MappedColumn = 1;
			if (TryExtractPreparedSourceIndexFromError(ParseError, SourceIndex)
				&& TryMapPreparedSourceIndexToLocation(PreparedSource, SourceIndex, MappedFilePath, MappedLine, MappedColumn))
			{
				return FString::Printf(TEXT("%s(%d,%d): %s"), *MappedFilePath, MappedLine, MappedColumn, *ParseError);
			}

			return FString::Printf(TEXT("%s: %s"), *FallbackSourceFilePath, *ParseError);
		}

		static bool LoadPreparedDreamShaderSourceRecursive(
			const FString& SourceFilePath,
			TSet<FString>& InOutVisitedFiles,
			TSet<FString>& InOutActiveStack,
			FString& OutSourceText,
			FString& OutError)
		{
			const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
			if (InOutVisitedFiles.Contains(NormalizedPath))
			{
				return true;
			}

			if (InOutActiveStack.Contains(NormalizedPath))
			{
				OutError = FString::Printf(TEXT("DreamShader import cycle detected at '%s'."), *NormalizedPath);
				return false;
			}

			FString SourceText;
			if (!FFileHelper::LoadFileToString(SourceText, *NormalizedPath))
			{
				OutError = FString::Printf(TEXT("DreamShader could not read '%s'."), *NormalizedPath);
				return false;
			}

			InOutActiveStack.Add(NormalizedPath);

			TArray<FString> Lines;
			SourceText.ParseIntoArrayLines(Lines, false);

			FString SanitizedSourceText;
			SanitizedSourceText.Reserve(SourceText.Len());

			for (const FString& Line : Lines)
			{
				FString ImportPath;
				if (TryExtractImportPathFromLine(Line, ImportPath))
				{
					FString ResolvedImportPath;
					if (!ResolveDreamShaderImportPath(NormalizedPath, ImportPath, ResolvedImportPath, OutError))
					{
						return false;
					}

					if (!LoadPreparedDreamShaderSourceRecursive(ResolvedImportPath, InOutVisitedFiles, InOutActiveStack, OutSourceText, OutError))
					{
						return false;
					}
					continue;
				}

				SanitizedSourceText += Line;
				SanitizedSourceText += TEXT("\n");
			}

			if (UE::DreamShader::IsDreamShaderHeaderFile(NormalizedPath)
				&& (SanitizedSourceText.Contains(TEXT("Shader("), ESearchCase::IgnoreCase)
					|| SanitizedSourceText.Contains(TEXT("ShaderFunction("), ESearchCase::IgnoreCase)))
			{
				OutError = FString::Printf(TEXT("DreamShader header '%s' may only declare Function/Namespace blocks and imports."), *NormalizedPath);
				return false;
			}

			OutSourceText += FString::Printf(TEXT("// Begin DreamShader source: %s\n"), *NormalizedPath);
			OutSourceText += SanitizedSourceText;
			OutSourceText += FString::Printf(TEXT("\n// End DreamShader source: %s\n\n"), *NormalizedPath);

			InOutActiveStack.Remove(NormalizedPath);
			InOutVisitedFiles.Add(NormalizedPath);
			return true;
		}

		static bool LoadPreparedDreamShaderSource(const FString& SourceFilePath, FString& OutSourceText, FString& OutError)
		{
			OutSourceText.Reset();
			TSet<FString> VisitedFiles;
			TSet<FString> ActiveStack;
			return LoadPreparedDreamShaderSourceRecursive(SourceFilePath, VisitedFiles, ActiveStack, OutSourceText, OutError);
		}

		bool TryResolveExpressionOutputIndexByName(const UMaterialExpression* Expression, const FString& OutputSpecifier, int32& OutIndex)
		{
			if (!Expression || Expression->Outputs.Num() == 0)
			{
				return false;
			}

			const FName DesiredOutput(*OutputSpecifier.TrimStartAndEnd());
			if (DesiredOutput.IsNone())
			{
				OutIndex = 0;
				return true;
			}

			for (int32 OutputIndex = 0; OutputIndex < Expression->Outputs.Num(); ++OutputIndex)
			{
				const FExpressionOutput& Output = Expression->Outputs[OutputIndex];
				if (!Output.OutputName.IsNone())
				{
					if (Output.OutputName == DesiredOutput)
					{
						OutIndex = OutputIndex;
						return true;
					}
					continue;
				}

				if (Output.MaskR && !Output.MaskG && !Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("R")))
				{
					OutIndex = OutputIndex;
					return true;
				}
				if (!Output.MaskR && Output.MaskG && !Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("G")))
				{
					OutIndex = OutputIndex;
					return true;
				}
				if (!Output.MaskR && !Output.MaskG && Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("B")))
				{
					OutIndex = OutputIndex;
					return true;
				}
				if (!Output.MaskR && !Output.MaskG && !Output.MaskB && Output.MaskA && DesiredOutput == FName(TEXT("A")))
				{
					OutIndex = OutputIndex;
					return true;
				}
			}

			return false;
		}

		FString BuildOutputTargetCacheKey(const FTextShaderOutputBinding& Binding)
		{
			TArray<FString> Parts;
			Parts.Reserve(Binding.ExpressionArguments.Num() + 1);
			Parts.Add(UE::DreamShader::NormalizeSettingKey(Binding.ExpressionClass));

			TArray<FString> ArgumentKeys;
			Binding.ExpressionArguments.GetKeys(ArgumentKeys);
			ArgumentKeys.Sort();
			for (const FString& Key : ArgumentKeys)
			{
				Parts.Add(Key + TEXT("=") + Binding.ExpressionArguments.FindChecked(Key));
			}

			return FString::Join(Parts, TEXT("|"));
		}

		bool CreateOrReuseOutputTargetExpression(
			UMaterial* Material,
			const FTextShaderOutputBinding& Binding,
			TMap<FString, UMaterialExpression*>& InOutExpressions,
			int32& InOutPositionY,
			UMaterialExpression*& OutExpression,
			FString& OutError)
		{
			const FString CacheKey = BuildOutputTargetCacheKey(Binding);
			if (UMaterialExpression* const* ExistingExpression = InOutExpressions.Find(CacheKey))
			{
				OutExpression = *ExistingExpression;
				return true;
			}

			UClass* ExpressionClass = Private::ResolveMaterialExpressionClass(Binding.ExpressionClass);
			if (!ExpressionClass)
			{
				OutError = FString::Printf(TEXT("Output target '%s' could not resolve MaterialExpression class '%s'."), *Binding.TargetText, *Binding.ExpressionClass);
				return false;
			}

			OutExpression = Cast<UMaterialExpression>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, 1200, InOutPositionY));
			if (!OutExpression)
			{
				OutError = FString::Printf(TEXT("Output target '%s' failed to create '%s'."), *Binding.TargetText, *ExpressionClass->GetName());
				return false;
			}
			InOutPositionY += 220;

			for (const TPair<FString, FString>& Argument : Binding.ExpressionArguments)
			{
				if (Argument.Key == UE::DreamShader::NormalizeSettingKey(TEXT("Class")))
				{
					continue;
				}

				FProperty* BoundProperty = Private::FindMaterialExpressionArgumentProperty(ExpressionClass, Argument.Key);
				if (!BoundProperty)
				{
					OutError = FString::Printf(TEXT("Output target '%s': '%s' is not a property on '%s'."), *Binding.TargetText, *Argument.Key, *ExpressionClass->GetName());
					return false;
				}

				if (Private::IsMaterialExpressionInputProperty(BoundProperty))
				{
					OutError = FString::Printf(TEXT("Output target '%s': inline input property '%s' is not supported yet. Bind through .Pin[index] instead."), *Binding.TargetText, *Argument.Key);
					return false;
				}

				FString LiteralError;
				if (!Private::SetMaterialExpressionLiteralProperty(OutExpression, BoundProperty, Argument.Value, LiteralError))
				{
					OutError = FString::Printf(TEXT("Output target '%s': %s"), *Binding.TargetText, *LiteralError);
					return false;
				}
			}

			InOutExpressions.Add(CacheKey, OutExpression);
			return true;
		}

		bool ConnectExpressionSourceToTargetPin(
			UMaterialExpression* SourceExpression,
			int32 SourceOutputIndex,
			const FString& SourceDebugName,
			const FTextShaderOutputBinding& Binding,
			UMaterialExpression* TargetExpression,
			TSet<FString>& BoundPins,
			FString& OutError)
		{
			if (!SourceExpression || !TargetExpression)
			{
				OutError = TEXT("Invalid output source or target expression.");
				return false;
			}

			const FString PinKey = BuildOutputTargetCacheKey(Binding) + FString::Printf(TEXT("#%d"), Binding.ExpressionPinIndex);
			if (BoundPins.Contains(PinKey))
			{
				OutError = FString::Printf(TEXT("Output target pin '%s' is bound more than once."), *Binding.TargetText);
				return false;
			}

			FExpressionInput* TargetInput = TargetExpression->GetInput(Binding.ExpressionPinIndex);
			if (!TargetInput)
			{
				OutError = FString::Printf(TEXT("Output target '%s' does not have Pin[%d]."), *Binding.TargetText, Binding.ExpressionPinIndex);
				return false;
			}

			TargetInput->Connect(SourceOutputIndex, SourceExpression);
			BoundPins.Add(PinKey);
			return true;
		}

		bool GenerateMaterialFunctionAsset(
			const FString& SourceFilePath,
			const FString& SourceHash,
			const FTextShaderDefinition& RootDefinition,
			const FTextShaderMaterialFunctionDefinition& FunctionDefinition,
			FString& OutGeneratedAssetPath,
			FString& OutError)
		{
			if (FunctionDefinition.Outputs.IsEmpty())
			{
				OutError = FString::Printf(TEXT("ShaderFunction '%s' must declare at least one output."), *FunctionDefinition.Name);
				return false;
			}

			if (FunctionDefinition.Code.IsEmpty())
			{
				OutError = FString::Printf(TEXT("ShaderFunction '%s' must provide a Graph block."), *FunctionDefinition.Name);
				return false;
			}

			UMaterialFunction* MaterialFunction = nullptr;
			if (!Private::CreateOrReuseMaterialFunction(FunctionDefinition, MaterialFunction, OutError) || !MaterialFunction)
			{
				return false;
			}

			if (Private::IsGeneratedAssetSourceCurrent(MaterialFunction, SourceFilePath, SourceHash))
			{
				OutGeneratedAssetPath = MaterialFunction->GetPathName();
				return true;
			}

			MaterialFunction->Modify();
			Private::ClearMaterialFunctionExpressions(MaterialFunction);

			if (const FString* Description = FunctionDefinition.Settings.Find(UE::DreamShader::NormalizeSettingKey(TEXT("Description"))))
			{
				MaterialFunction->Description = *Description;
			}
			else
			{
				MaterialFunction->Description.Reset();
			}

			if (const FString* Caption = FunctionDefinition.Settings.Find(UE::DreamShader::NormalizeSettingKey(TEXT("UserExposedCaption"))))
			{
				MaterialFunction->UserExposedCaption = *Caption;
			}
			else
			{
				MaterialFunction->UserExposedCaption.Reset();
			}

			if (const FString* ExposeToLibraryText = FunctionDefinition.Settings.Find(UE::DreamShader::NormalizeSettingKey(TEXT("ExposeToLibrary"))))
			{
				bool bExposeToLibrary = false;
				if (!Private::ParseBooleanLiteral(*ExposeToLibraryText, bExposeToLibrary))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s': ExposeToLibrary must be true or false."), *FunctionDefinition.Name);
					return false;
				}
				MaterialFunction->bExposeToLibrary = bExposeToLibrary ? 1U : 0U;
			}
			else
			{
				MaterialFunction->bExposeToLibrary = 0U;
			}

			MaterialFunction->LibraryCategoriesText.Reset();
			if (const FString* CategoriesText = FunctionDefinition.Settings.Find(UE::DreamShader::NormalizeSettingKey(TEXT("LibraryCategories"))))
			{
				TArray<FString> Categories;
				CategoriesText->ParseIntoArray(Categories, TEXT(","), true);
				for (const FString& Category : Categories)
				{
					const FString TrimmedCategory = Category.TrimStartAndEnd();
					if (!TrimmedCategory.IsEmpty())
					{
						MaterialFunction->LibraryCategoriesText.Add(FText::FromString(TrimmedCategory));
					}
				}
			}

			TMap<FString, Private::FCodeValue> GeneratedValues;
			TMap<FString, UMaterialExpressionFunctionInput*> GeneratedInputExpressions;
			int32 InputPositionY = -260;
			for (int32 InputIndex = 0; InputIndex < FunctionDefinition.Inputs.Num(); ++InputIndex)
			{
				const FTextShaderFunctionParameter& InputDefinition = FunctionDefinition.Inputs[InputIndex];

				int32 ComponentCount = 0;
				bool bIsTextureObject = false;
				int32 FunctionInputTypeValue = 0;
				if (!Private::TryResolveMaterialFunctionParameterType(
					InputDefinition.Type,
					ComponentCount,
					bIsTextureObject,
					FunctionInputTypeValue))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' input '%s' uses unsupported type '%s'."), *FunctionDefinition.Name, *InputDefinition.Name, *InputDefinition.Type);
					return false;
				}

				auto* InputExpression = Cast<UMaterialExpressionFunctionInput>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionFunctionInput::StaticClass(), -800, InputPositionY));
				if (!InputExpression)
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' failed to create input '%s'."), *FunctionDefinition.Name, *InputDefinition.Name);
					return false;
				}

				InputExpression->InputName = FName(*InputDefinition.Name);
				InputExpression->InputType = static_cast<EFunctionInputType>(FunctionInputTypeValue);
				InputExpression->Description = InputDefinition.Metadata.Description;
				InputExpression->SortPriority = InputDefinition.Metadata.bHasSortPriority
					? InputDefinition.Metadata.SortPriority
					: InputIndex;
				InputExpression->ConditionallyGenerateId(false);

				Private::FCodeValue InputValue;
				InputValue.Expression = InputExpression;
				InputValue.ComponentCount = ComponentCount;
				InputValue.bIsTextureObject = bIsTextureObject;
				GeneratedValues.Add(InputDefinition.Name, InputValue);
				GeneratedInputExpressions.Add(InputDefinition.Name, InputExpression);
				InputPositionY += 180;
			}

			for (const FTextShaderFunctionParameter& InputDefinition : FunctionDefinition.Inputs)
			{
				UMaterialExpressionFunctionInput** InputExpressionPtr = GeneratedInputExpressions.Find(InputDefinition.Name);
				const Private::FCodeValue* InputValue = GeneratedValues.Find(InputDefinition.Name);
				if (!InputExpressionPtr || !*InputExpressionPtr || !InputValue)
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' failed to resolve generated input '%s'."), *FunctionDefinition.Name, *InputDefinition.Name);
					return false;
				}

				FString PreviewError;
				if (!ApplyFunctionInputPreviewDefault(
					MaterialFunction,
					SourceFilePath,
					RootDefinition,
					InputDefinition,
					*InputExpressionPtr,
					InputValue->ComponentCount,
					InputValue->bIsTextureObject,
					GeneratedValues,
					PreviewError))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' input '%s': %s"), *FunctionDefinition.Name, *InputDefinition.Name, *PreviewError);
					return false;
				}
			}

			if (!FunctionDefinition.Code.IsEmpty())
			{
				TArray<Private::FCodeStatement> CodeStatements;
				FString CodeParseError;
				if (!Private::ParseCodeStatements(FunctionDefinition.Code, CodeStatements, CodeParseError))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s': %s"), *FunctionDefinition.Name, *CodeParseError);
					return false;
				}

				Private::FCodeGraphBuilder CodeGraphBuilder(
					nullptr,
					MaterialFunction,
					RootDefinition,
					SourceFilePath,
					Private::BuildGeneratedIncludeVirtualPath(SourceFilePath));
				FString CodeBuildError;
				if (!CodeGraphBuilder.Build(CodeStatements, GeneratedValues, CodeBuildError))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s': %s"), *FunctionDefinition.Name, *CodeBuildError);
					return false;
				}
			}
			else
			{
				const FTextShaderFunctionParameter& PrimaryOutput = FunctionDefinition.Outputs[0];
				ECustomMaterialOutputType OutputType = CMOT_Float1;
				if (!Private::TryResolveCustomOutputType(PrimaryOutput.Type, OutputType))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' output '%s' uses unsupported type '%s'."), *FunctionDefinition.Name, *PrimaryOutput.Name, *PrimaryOutput.Type);
					return false;
				}

				auto* CustomExpression = Cast<UMaterialExpressionCustom>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionCustom::StaticClass(), 0, 0));
				if (!CustomExpression)
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' failed to create the function Custom node."), *FunctionDefinition.Name);
					return false;
				}

				CustomExpression->Description = FunctionDefinition.Name;
				CustomExpression->OutputType = OutputType;
				CustomExpression->ShowCode = true;
				CustomExpression->Inputs.Reset();
				CustomExpression->AdditionalOutputs.Reset();
				CustomExpression->IncludeFilePaths.Reset();

				FString PreparedCustomCode;
				bool bUsesGeneratedInclude = false;
				if (!Private::PrepareCustomNodeCode(
					RootDefinition,
					FunctionDefinition.HLSL,
					TArray<FString>(),
					FunctionDefinition.Name,
					PreparedCustomCode,
					bUsesGeneratedInclude,
					OutError))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s': %s"), *FunctionDefinition.Name, *OutError);
					return false;
				}
				CustomExpression->Code = Private::EnsureTopLevelReturn(PreparedCustomCode);

				if (bUsesGeneratedInclude)
				{
					CustomExpression->IncludeFilePaths.Add(Private::BuildGeneratedIncludeVirtualPath(SourceFilePath));
				}

				for (const FTextShaderFunctionParameter& InputDefinition : FunctionDefinition.Inputs)
				{
					const Private::FCodeValue* InputValue = GeneratedValues.Find(InputDefinition.Name);
					if (!InputValue || !InputValue->Expression)
					{
						OutError = FString::Printf(TEXT("ShaderFunction '%s' failed to resolve generated input '%s'."), *FunctionDefinition.Name, *InputDefinition.Name);
						return false;
					}

					FCustomInput Input;
					Input.InputName = FName(*InputDefinition.Name);
					CustomExpression->Inputs.Add(Input);
					CustomExpression->Inputs.Last().Input.Connect(InputValue->OutputIndex, InputValue->Expression);
				}

				for (int32 OutputIndex = 1; OutputIndex < FunctionDefinition.Outputs.Num(); ++OutputIndex)
				{
					const FTextShaderFunctionParameter& OutputDefinition = FunctionDefinition.Outputs[OutputIndex];
					ECustomMaterialOutputType AdditionalOutputType = CMOT_Float1;
					if (!Private::TryResolveCustomOutputType(OutputDefinition.Type, AdditionalOutputType))
					{
						OutError = FString::Printf(TEXT("ShaderFunction '%s' output '%s' uses unsupported type '%s'."), *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type);
						return false;
					}

					FCustomOutput Output;
					Output.OutputName = FName(*OutputDefinition.Name);
					Output.OutputType = AdditionalOutputType;
					CustomExpression->AdditionalOutputs.Add(Output);
				}

				CustomExpression->RebuildOutputs();

				Private::FCodeValue PrimaryOutputValue;
				PrimaryOutputValue.Expression = CustomExpression;
				PrimaryOutputValue.ComponentCount = 0;
				PrimaryOutputValue.bIsTextureObject = false;
				verify(Private::TryResolveCodeDeclaredType(PrimaryOutput.Type, PrimaryOutputValue.ComponentCount, PrimaryOutputValue.bIsTextureObject));
				GeneratedValues.Add(PrimaryOutput.Name, PrimaryOutputValue);

				for (int32 OutputIndex = 1; OutputIndex < FunctionDefinition.Outputs.Num(); ++OutputIndex)
				{
					const FTextShaderFunctionParameter& OutputDefinition = FunctionDefinition.Outputs[OutputIndex];
					Private::FCodeValue OutputValue;
					OutputValue.Expression = CustomExpression;
					OutputValue.OutputIndex = OutputIndex;
					verify(Private::TryResolveCodeDeclaredType(OutputDefinition.Type, OutputValue.ComponentCount, OutputValue.bIsTextureObject));
					GeneratedValues.Add(OutputDefinition.Name, OutputValue);
				}
			}

			int32 OutputPositionY = -120;
			for (int32 OutputIndex = 0; OutputIndex < FunctionDefinition.Outputs.Num(); ++OutputIndex)
			{
				const FTextShaderFunctionParameter& OutputDefinition = FunctionDefinition.Outputs[OutputIndex];
				const Private::FCodeValue* OutputValue = GeneratedValues.Find(OutputDefinition.Name);
				if (!OutputValue || !OutputValue->Expression)
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' output '%s' was never assigned an expression."), *FunctionDefinition.Name, *OutputDefinition.Name);
					return false;
				}

				int32 ExpectedComponentCount = 0;
				bool bExpectedTexture = false;
				int32 IgnoredFunctionInputType = 0;
				if (!Private::TryResolveMaterialFunctionParameterType(
					OutputDefinition.Type,
					ExpectedComponentCount,
					bExpectedTexture,
					IgnoredFunctionInputType))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' output '%s' uses unsupported type '%s'."), *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type);
					return false;
				}

				if (bExpectedTexture != OutputValue->bIsTextureObject
					|| (!bExpectedTexture && ExpectedComponentCount != OutputValue->ComponentCount))
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' output '%s' does not match its declared type '%s'."), *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type);
					return false;
				}

				auto* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionFunctionOutput::StaticClass(), 900, OutputPositionY));
				if (!OutputExpression)
				{
					OutError = FString::Printf(TEXT("ShaderFunction '%s' failed to create output '%s'."), *FunctionDefinition.Name, *OutputDefinition.Name);
					return false;
				}

				OutputExpression->OutputName = FName(*OutputDefinition.Name);
				OutputExpression->Description = OutputDefinition.Metadata.Description;
				OutputExpression->SortPriority = OutputDefinition.Metadata.bHasSortPriority
					? OutputDefinition.Metadata.SortPriority
					: OutputIndex;
				OutputExpression->ConditionallyGenerateId(false);
				OutputExpression->A.Connect(OutputValue->OutputIndex, OutputValue->Expression);
				OutputPositionY += 180;
			}

			UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(MaterialFunction);
			UMaterialEditingLibrary::UpdateMaterialFunction(MaterialFunction, nullptr);
			MaterialFunction->PostEditChange();
			MaterialFunction->MarkPackageDirty();
			Private::ApplySourceMetadata(MaterialFunction, SourceFilePath, SourceHash);

			FString SaveError;
			if (!Private::SaveAssetPackage(MaterialFunction, SaveError))
			{
				OutError = SaveError;
				return false;
			}

			OutGeneratedAssetPath = MaterialFunction->GetPathName();
			return true;
		}
	}

	bool FMaterialGenerator::GenerateAssetsFromFile(const FString& InSourceFilePath, FString& OutMessage)
	{
		const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		if (UE::DreamShader::IsDreamShaderHeaderFile(SourceFilePath))
		{
			OutMessage = FString::Printf(TEXT("DreamShader header '%s' does not generate assets directly. Recompile dependent .dsm files instead."), *SourceFilePath);
			return false;
		}

		FString SourceText;
		FString PreparedSourceError;
		if (!LoadPreparedDreamShaderSource(SourceFilePath, SourceText, PreparedSourceError))
		{
			OutMessage = PreparedSourceError;
			return false;
		}

		FTextShaderDefinition Definition;
		FString ParseError;
		if (!FTextShaderParser::Parse(SourceText, Definition, ParseError))
		{
			OutMessage = FormatParseErrorWithSourceLocation(SourceFilePath, SourceText, ParseError);
			return false;
		}

		const FString SourceHash = Private::BuildSourceHash(SourceText);

		bool bGeneratedHelperInclude = false;
		if (!Definition.Functions.IsEmpty())
		{
			FString IncludeWriteError;
			if (!Private::WriteGeneratedInclude(SourceFilePath, Definition, IncludeWriteError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *IncludeWriteError);
				return false;
			}

			bGeneratedHelperInclude = true;
		}

		TArray<FString> GeneratedAssetMessages;
		for (const FTextShaderMaterialFunctionDefinition& FunctionDefinition : Definition.MaterialFunctions)
		{
			FString GeneratedAssetPath;
			FString FunctionError;
			if (!GenerateMaterialFunctionAsset(SourceFilePath, SourceHash, Definition, FunctionDefinition, GeneratedAssetPath, FunctionError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *FunctionError);
				return false;
			}

			GeneratedAssetMessages.Add(FString::Printf(TEXT("Generated %s from %s."), *GeneratedAssetPath, *SourceFilePath));
		}

		if (!Definition.Name.IsEmpty())
		{
			FString MaterialMessage;
			if (!GenerateMaterialFromFile(SourceFilePath, MaterialMessage))
			{
				OutMessage = MaterialMessage;
				return false;
			}

			GeneratedAssetMessages.Add(MaterialMessage);
		}

		if (GeneratedAssetMessages.IsEmpty())
		{
			if (bGeneratedHelperInclude)
			{
				OutMessage = FString::Printf(
					TEXT("Generated DreamShader helper include '%s' from %s."),
					*Private::BuildGeneratedIncludeVirtualPath(SourceFilePath),
					*SourceFilePath);
				return true;
			}

			if (!Definition.VirtualFunctions.IsEmpty())
			{
				OutMessage = FString::Printf(TEXT("DreamShader file '%s' contains VirtualFunction declarations only; no assets were generated."), *SourceFilePath);
				return true;
			}

			OutMessage = FString::Printf(TEXT("DreamShader file '%s' did not contain any material or ShaderFunction assets to generate."), *SourceFilePath);
			return false;
		}

		OutMessage = FString::Join(GeneratedAssetMessages, TEXT("\n"));
		return true;
	}

	bool FMaterialGenerator::GenerateMaterialFromFile(const FString& InSourceFilePath, FString& OutMessage)
	{
		const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		if (UE::DreamShader::IsDreamShaderHeaderFile(SourceFilePath))
		{
			OutMessage = FString::Printf(TEXT("DreamShader header '%s' cannot generate a material asset directly."), *SourceFilePath);
			return false;
		}

		FString SourceText;
		FString PreparedSourceError;
		if (!LoadPreparedDreamShaderSource(SourceFilePath, SourceText, PreparedSourceError))
		{
			OutMessage = PreparedSourceError;
			return false;
		}

		FTextShaderDefinition Definition;
		FString ParseError;
		if (!FTextShaderParser::Parse(SourceText, Definition, ParseError))
		{
			OutMessage = FormatParseErrorWithSourceLocation(SourceFilePath, SourceText, ParseError);
			return false;
		}

		const FString SourceHash = Private::BuildSourceHash(SourceText);

		if (Definition.Name.IsEmpty())
		{
			OutMessage = FString::Printf(TEXT("%s: This file does not define a top-level Shader block."), *SourceFilePath);
			return false;
		}

		if (Definition.Outputs.IsEmpty())
		{
			OutMessage = FString::Printf(TEXT("%s: Outputs block is required."), *SourceFilePath);
			return false;
		}

		TArray<Private::FResolvedNamedOutput> NamedOutputs;
		bool bUsesReturn = false;
		ECustomMaterialOutputType ReturnOutputType = CMOT_Float1;
		FString ValidationError;
		if (!Private::ValidateSettings(Definition, ValidationError)
			|| !Private::ValidateOutputs(Definition, NamedOutputs, bUsesReturn, ReturnOutputType, ValidationError))
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *ValidationError);
			return false;
		}

		if (!Definition.Functions.IsEmpty())
		{
			FString IncludeWriteError;
			if (!Private::WriteGeneratedInclude(SourceFilePath, Definition, IncludeWriteError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *IncludeWriteError);
				return false;
			}
		}

		UMaterial* Material = nullptr;
		FString MaterialError;
		if (!Private::CreateOrReuseMaterial(Definition, Material, MaterialError) || !Material)
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *MaterialError);
			return false;
		}

		if (Private::IsGeneratedAssetSourceCurrent(Material, SourceFilePath, SourceHash))
		{
			OutMessage = FString::Printf(TEXT("Skipped %s from %s; source hash is unchanged."), *Material->GetPathName(), *SourceFilePath);
			return true;
		}

		Material->Modify();
		Private::ClearMaterialExpressions(Material);
		Private::ResetMaterialToDefaults(Material);

		FString SettingsError;
		if (!Private::ApplySettings(Material, Definition, SettingsError))
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SettingsError);
			return false;
		}

		TMap<FString, UMaterialExpression*> GeneratedPropertyExpressions;
		TMap<FString, UMaterialExpression*> GeneratedOutputTargetExpressions;
		TSet<FString> BoundOutputTargetPins;
		TMap<FString, Private::FCodeValue> GeneratedCodeValues;
		int32 ParameterPositionY = -300;
		int32 OutputTargetPositionY = 200;
		for (const FTextShaderPropertyDefinition& Property : Definition.Properties)
		{
			bool bNameConflict = false;
			for (const TPair<FString, UMaterialExpression*>& ExistingProperty : GeneratedPropertyExpressions)
			{
				if (ExistingProperty.Key.Equals(Property.Name, ESearchCase::IgnoreCase))
				{
					bNameConflict = true;
					break;
				}
			}

			if (bNameConflict)
			{
				OutMessage = FString::Printf(
					TEXT("%s: Property '%s' is declared more than once. Property names must be unique."),
					*SourceFilePath,
					*Property.Name);
				return false;
			}

			FString PropertyExpressionError;
			UMaterialExpression* PropertyExpression =
				Private::CreatePropertyExpression(Material, Property, GeneratedPropertyExpressions, ParameterPositionY, PropertyExpressionError);
			if (!PropertyExpression)
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *PropertyExpressionError);
				return false;
			}

			GeneratedPropertyExpressions.Add(Property.Name, PropertyExpression);
			Private::FCodeValue PropertyValue;
			PropertyValue.Expression = PropertyExpression;
			PropertyValue.ComponentCount = Property.Type == ETextShaderPropertyType::Texture2D ? 0 : Property.ComponentCount;
			PropertyValue.bIsTextureObject = Property.Type == ETextShaderPropertyType::Texture2D;
			GeneratedCodeValues.Add(Property.Name, PropertyValue);
			ParameterPositionY += 220;
		}

		if (!Definition.Code.IsEmpty())
		{
			if (bUsesReturn)
			{
				OutMessage = FString::Printf(TEXT("%s: Graph blocks do not support binding Outputs to the reserved name 'return'."), *SourceFilePath);
				return false;
			}

			TArray<Private::FCodeStatement> CodeStatements;
			FString CodeParseError;
			if (!Private::ParseCodeStatements(Definition.Code, CodeStatements, CodeParseError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *CodeParseError);
				return false;
			}

			Private::FCodeGraphBuilder CodeGraphBuilder(
				Material,
				nullptr,
				Definition,
				SourceFilePath,
				Private::BuildGeneratedIncludeVirtualPath(SourceFilePath));
			FString CodeBuildError;
			if (!CodeGraphBuilder.Build(CodeStatements, GeneratedCodeValues, CodeBuildError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *CodeBuildError);
				return false;
			}

			for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
			{
				Private::FCodeValue OutputValue;
				FString OutputExpressionError;
				if (!CodeGraphBuilder.EvaluateOutputExpression(Binding.SourceText, OutputValue, OutputExpressionError)
					|| !OutputValue.Expression)
				{
					OutMessage = FString::Printf(
						TEXT("%s: %s"),
						*SourceFilePath,
						*OutputExpressionError);
					return false;
				}

				int32 DeclaredComponents = 0;
				bool bDeclaredTexture = false;
				if (Private::TryResolveOutputVariableComponentCount(Definition, Binding.SourceText, DeclaredComponents, bDeclaredTexture))
				{
					if (bDeclaredTexture || OutputValue.bIsTextureObject || DeclaredComponents != OutputValue.ComponentCount)
					{
						OutMessage = FString::Printf(
							TEXT("%s: Graph output '%s' does not match its declared type."),
							*SourceFilePath,
							*Binding.SourceText);
						return false;
					}
				}

				if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty)
				{
					Private::FResolvedMaterialProperty ResolvedProperty;
					verify(Private::ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty));

					FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(ResolvedProperty.Property);
					if (!MaterialInput)
					{
						OutMessage = FString::Printf(
							TEXT("%s: Failed to find material property '%s' while connecting Graph output '%s'."),
							*SourceFilePath,
							*Binding.MaterialProperty,
							*Binding.SourceText);
						return false;
					}

					MaterialInput->Connect(OutputValue.OutputIndex, OutputValue.Expression);
				}
				else
				{
					UMaterialExpression* TargetExpression = nullptr;
					FString TargetError;
					if (!CreateOrReuseOutputTargetExpression(
						Material,
						Binding,
						GeneratedOutputTargetExpressions,
						OutputTargetPositionY,
						TargetExpression,
						TargetError)
						|| !ConnectExpressionSourceToTargetPin(
							OutputValue.Expression,
							OutputValue.OutputIndex,
							Binding.SourceText,
							Binding,
							TargetExpression,
							BoundOutputTargetPins,
							TargetError))
					{
						OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *TargetError);
						return false;
					}
				}
			}
		}
		else
		{
			auto* CustomExpression = Cast<UMaterialExpressionCustom>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCustom::StaticClass(), 0, 0));
			if (!CustomExpression)
			{
				OutMessage = FString::Printf(TEXT("%s: Failed to create the material Custom node."), *SourceFilePath);
				return false;
			}

			CustomExpression->Description = Definition.Name;
			CustomExpression->OutputType = bUsesReturn ? ReturnOutputType : CMOT_Float1;
			CustomExpression->ShowCode = true;
			CustomExpression->Inputs.Reset();
			CustomExpression->AdditionalOutputs.Reset();
			CustomExpression->IncludeFilePaths.Reset();

			FString PreparedCustomCode;
			bool bUsesGeneratedInclude = false;
			if (!Private::PrepareCustomNodeCode(
				Definition,
				Definition.HLSL,
				TArray<FString>(),
				Definition.Name,
				PreparedCustomCode,
				bUsesGeneratedInclude,
				OutMessage))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *OutMessage);
				return false;
			}
			CustomExpression->Code = Private::EnsureTopLevelReturn(PreparedCustomCode);

			if (bUsesGeneratedInclude)
			{
				CustomExpression->IncludeFilePaths.Add(Private::BuildGeneratedIncludeVirtualPath(SourceFilePath));
			}

			for (const FTextShaderPropertyDefinition& Property : Definition.Properties)
			{
				FCustomInput Input;
				Input.InputName = FName(*Property.Name);
				CustomExpression->Inputs.Add(Input);
				CustomExpression->Inputs.Last().Input.Expression = GeneratedPropertyExpressions.FindChecked(Property.Name);
			}

			for (const Private::FResolvedNamedOutput& OutputDefinition : NamedOutputs)
			{
				FCustomOutput Output;
				Output.OutputName = FName(*OutputDefinition.Name);
				Output.OutputType = OutputDefinition.OutputType;
				CustomExpression->AdditionalOutputs.Add(Output);
			}

			CustomExpression->RebuildOutputs();

			for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
			{
				int32 SourceOutputIndex = 0;
				if (!Binding.SourceText.Equals(TEXT("return"), ESearchCase::IgnoreCase)
					&& !TryResolveExpressionOutputIndexByName(CustomExpression, Binding.SourceText, SourceOutputIndex))
				{
					OutMessage = FString::Printf(
						TEXT("%s: Failed to resolve Custom output '%s'."),
						*SourceFilePath,
						*Binding.SourceText);
					return false;
				}

				if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty)
				{
					Private::FResolvedMaterialProperty ResolvedProperty;
					verify(Private::ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty));

					FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(ResolvedProperty.Property);
					if (!MaterialInput)
					{
						OutMessage = FString::Printf(
							TEXT("%s: Failed to find material property '%s' while connecting '%s'."),
							*SourceFilePath,
							*Binding.MaterialProperty,
							*Binding.SourceText);
						return false;
					}

					MaterialInput->Connect(SourceOutputIndex, CustomExpression);
				}
				else
				{
					UMaterialExpression* TargetExpression = nullptr;
					FString TargetError;
					if (!CreateOrReuseOutputTargetExpression(
						Material,
						Binding,
						GeneratedOutputTargetExpressions,
						OutputTargetPositionY,
						TargetExpression,
						TargetError)
						|| !ConnectExpressionSourceToTargetPin(
							CustomExpression,
							SourceOutputIndex,
							Binding.SourceText,
							Binding,
							TargetExpression,
							BoundOutputTargetPins,
							TargetError))
					{
						OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *TargetError);
						return false;
					}
				}
			}
		}

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		Material->PostEditChange();
		Material->MarkPackageDirty();
		Private::ApplySourceMetadata(Material, SourceFilePath, SourceHash);

		FString SaveError;
		if (!Private::SaveAssetPackage(Material, SaveError))
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SaveError);
			return false;
		}

		OutMessage = FString::Printf(TEXT("Generated %s from %s."), *Material->GetPathName(), *SourceFilePath);
		return true;
	}
}
