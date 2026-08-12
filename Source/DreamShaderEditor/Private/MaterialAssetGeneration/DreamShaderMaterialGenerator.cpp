#include "DreamShaderMaterialGenerator.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderMaterialGeneratorCodeShared.h"
#include "DreamShaderMaterialGeneratorDiagnostics.h"
#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderMaterialGeneratorSourceLoading.h"

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CoreGlobals.h"
#include "Engine/Texture.h"
#include "HAL/FileManager.h"
#include "RenderUtils.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialShared.h"
#include "Misc/PackageName.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "MaterialSceneTextureId.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "DreamShader.Generator"

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

		static bool ContainsIdentifierReference(const FString& Text, const FString& Identifier)
		{
			if (Identifier.IsEmpty())
			{
				return false;
			}

			bool bInString = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;
			for (int32 Index = 0; Index < Text.Len(); ++Index)
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

				if ((FChar::IsAlpha(Char) || Char == TCHAR('_')) && IsIdentifierBoundary(Text, Index - 1))
				{
					const int32 Start = Index++;
					while (Text.IsValidIndex(Index) && (FChar::IsAlnum(Text[Index]) || Text[Index] == TCHAR('_')))
					{
						++Index;
					}

					if (Text.Mid(Start, Index - Start).Equals(Identifier, ESearchCase::CaseSensitive))
					{
						return true;
					}
					--Index;
				}
			}

			return false;
		}

		static FString FindRegionNameForStatement(
			const TArray<FTextShaderGraphRegion>& Regions,
			const Private::FCodeStatement& Statement)
		{
			if (!Statement.bHasSourceLocation)
			{
				return FString();
			}

			for (const FTextShaderGraphRegion& Region : Regions)
			{
				if (Statement.SourceLine >= Region.StartLine && Statement.SourceLine <= Region.EndLine)
				{
					return Region.Name;
				}
			}

			return FString();
		}

		static void ApplyStatementRegionsRecursive(
			TArray<Private::FCodeStatement>& Statements,
			const TArray<FTextShaderGraphRegion>& Regions)
		{
			for (Private::FCodeStatement& Statement : Statements)
			{
				Statement.RegionName = FindRegionNameForStatement(Regions, Statement);
				ApplyStatementRegionsRecursive(Statement.ThenStatements, Regions);
				ApplyStatementRegionsRecursive(Statement.ElseStatements, Regions);
			}
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
			const ETextShaderTextureType TextureType,
			const TArray<FTextShaderPropertyDefinition>* LocalProperties,
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

			const bool bIsSubstrateMaterial = Private::IsSubstrateMaterialType(InputDefinition.Type);
			const bool bIsMaterialAttributes = ComponentCount == 0 && !bIsTextureObject && !bIsSubstrateMaterial;
			FVector4f PreviewValue;
			if (!bIsTextureObject && !bIsMaterialAttributes && !bIsSubstrateMaterial && TryParseFunctionInputPreviewLiteral(InputDefinition.DefaultValueText, ComponentCount, PreviewValue))
			{
				InputExpression->PreviewValue = PreviewValue;
				return true;
			}

			Private::FCodeGraphBuilder PreviewGraphBuilder(
				nullptr,
				MaterialFunction,
				RootDefinition,
				SourceFilePath,
				Private::BuildGeneratedIncludeVirtualPath(SourceFilePath),
				LocalProperties);
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

			if (PreviewExpressionValue.bIsTextureObject != bIsTextureObject
				|| (bIsTextureObject && PreviewExpressionValue.TextureType != TextureType)
				|| PreviewExpressionValue.bIsMaterialAttributes != bIsMaterialAttributes
				|| PreviewExpressionValue.bIsSubstrateMaterial != bIsSubstrateMaterial
				|| PreviewExpressionValue.ComponentCount != ComponentCount)
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("Input '%s' default expression '%s' does not match declared type '%s'."),
					*InputDefinition.Name,
					*InputDefinition.DefaultValueText,
					*InputDefinition.Type);
				return false;
			}

			Private::ConnectCodeValueToInput(InputExpression->Preview, PreviewExpressionValue);
			return true;
		}

		static bool SeedMaterialAttributesGraphValue(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const FString& ValueName,
			TMap<FString, Private::FCodeValue>& InOutGeneratedValues,
			int32& InOutPositionY,
			FString& OutError)
		{
			if (ValueName.IsEmpty() || InOutGeneratedValues.Contains(ValueName))
			{
				return true;
			}

			auto* Expression = Cast<UMaterialExpressionMakeMaterialAttributes>(
				UMaterialEditingLibrary::CreateMaterialExpressionEx(
					Material,
					MaterialFunction,
					UMaterialExpressionMakeMaterialAttributes::StaticClass(),
					nullptr,
					120,
					InOutPositionY,
					false));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create a MakeMaterialAttributes node for '%s'."), *ValueName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			Private::FCodeValue Value;
			Value.Expression = Expression;
			Value.OutputIndex = 0;
			Value.ComponentCount = 0;
			Value.bIsTextureObject = false;
			Value.bIsMaterialAttributes = true;
			Value.bIsSubstrateMaterial = false;
			InOutGeneratedValues.Add(ValueName, Value);
			InOutPositionY += 220;
			return true;
		}

		static const FTextShaderPropertyDefinition* FindPropertyByName(
			const TArray<FTextShaderPropertyDefinition>& Properties,
			const FString& Name)
		{
			for (const FTextShaderPropertyDefinition& Property : Properties)
			{
				if (Property.Name.Equals(Name, ESearchCase::IgnoreCase))
				{
					return &Property;
				}
			}

			return nullptr;
		}

		static bool CreateReferencedPropertyExpression(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const TArray<FTextShaderPropertyDefinition>& Properties,
			const FTextShaderPropertyDefinition& Property,
			TMap<FString, UMaterialExpression*>& InOutGeneratedPropertyExpressions,
			TSet<FString>& InOutCreatingPropertyNames,
			int32& InOutPositionY,
			UMaterialExpression*& OutExpression,
			FString& OutError)
		{
			if (UMaterialExpression* const* ExistingExpression = InOutGeneratedPropertyExpressions.Find(Property.Name))
			{
				OutExpression = *ExistingExpression;
				return true;
			}

			for (const FString& CreatingName : InOutCreatingPropertyNames)
			{
				if (CreatingName.Equals(Property.Name, ESearchCase::IgnoreCase))
				{
					OutError = FString::Printf(TEXT("Property '%s' has a recursive UE builtin dependency."), *Property.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
			}

			InOutCreatingPropertyNames.Add(Property.Name);
			if (Property.Source == ETextShaderPropertySource::UEBuiltin)
			{
				for (const TPair<FString, FString>& Argument : Property.UEBuiltinArguments)
				{
					const FTextShaderPropertyDefinition* Dependency = FindPropertyByName(Properties, Argument.Value.TrimStartAndEnd());
					if (!Dependency)
					{
						continue;
					}

					UMaterialExpression* IgnoredDependencyExpression = nullptr;
					if (!CreateReferencedPropertyExpression(
						Material,
						MaterialFunction,
						Properties,
						*Dependency,
						InOutGeneratedPropertyExpressions,
						InOutCreatingPropertyNames,
						InOutPositionY,
						IgnoredDependencyExpression,
						OutError))
					{
						InOutCreatingPropertyNames.Remove(Property.Name);
						return false;
					}
				}
			}

			FString PropertyExpressionError;
			OutExpression = Private::CreatePropertyExpression(
				Material,
				MaterialFunction,
				Property,
				InOutGeneratedPropertyExpressions,
				InOutPositionY,
				PropertyExpressionError);
			if (!OutExpression)
			{
				OutError = PropertyExpressionError;
				InOutCreatingPropertyNames.Remove(Property.Name);
				return false;
			}

			InOutGeneratedPropertyExpressions.Add(Property.Name, OutExpression);
			InOutPositionY += 220;
			InOutCreatingPropertyNames.Remove(Property.Name);
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
					OutError = FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("Function '%s' parameter '%s' uses unsupported qualifier '%s'. Supported qualifiers are in and out."),
						*FunctionName,
						*Parameter,
						*Qualifier);
					return false;
				}

				if (TypeToken.IsEmpty() || NameToken.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				const FString Statement = FString::Printf(TEXT("        %s %s;\n"), *TypeToken, *NameToken); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				OutError = FString::Printf(TEXT("Function '%s' must declare at least one out parameter."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

				if (Output.MaskR && Output.MaskG && Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("RGB")))
				{
					OutIndex = OutputIndex;
					return true;
				}
				if (Output.MaskR && Output.MaskG && !Output.MaskB && !Output.MaskA && DesiredOutput == FName(TEXT("RG")))
				{
					OutIndex = OutputIndex;
					return true;
				}
				if (Output.MaskR && Output.MaskG && Output.MaskB && Output.MaskA && DesiredOutput == FName(TEXT("RGBA")))
				{
					OutIndex = OutputIndex;
					return true;
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

		int32 GetPreferredOutputIndexForProperty(const FTextShaderPropertyDefinition& Property, const UMaterialExpression* Expression)
		{
			if (Property.Type == ETextShaderPropertyType::Vector && !Property.bConst)
			{
				static const TCHAR* ComponentOutputs[] = { TEXT(""), TEXT("R"), TEXT("RG"), TEXT("RGB"), TEXT("RGBA") };
				int32 OutputIndex = 0;
				if (Property.ComponentCount > 0
					&& Property.ComponentCount < UE_ARRAY_COUNT(ComponentOutputs)
					&& TryResolveExpressionOutputIndexByName(Expression, ComponentOutputs[Property.ComponentCount], OutputIndex))
				{
					return OutputIndex;
				}
			}

			return 0;
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
				OutError = FString::Printf(TEXT("Output target '%s' could not resolve MaterialExpression class '%s'."), *Binding.TargetText, *Binding.ExpressionClass); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			OutExpression = Cast<UMaterialExpression>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, 1200, InOutPositionY));
			if (!OutExpression)
			{
				OutError = FString::Printf(TEXT("Output target '%s' failed to create '%s'."), *Binding.TargetText, *ExpressionClass->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					OutError = FString::Printf(TEXT("Output target '%s': '%s' is not a property on '%s'."), *Binding.TargetText, *Argument.Key, *ExpressionClass->GetName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				if (Private::IsMaterialExpressionInputProperty(BoundProperty))
				{
					OutError = FString::Printf(TEXT("Output target '%s': inline input property '%s' is not supported yet. Bind through .Pin[index] instead."), *Binding.TargetText, *Argument.Key); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				FString LiteralError;
				if (!Private::SetMaterialExpressionLiteralProperty(OutExpression, BoundProperty, Argument.Value, LiteralError))
				{
					OutError = FString::Printf(TEXT("Output target '%s': %s"), *Binding.TargetText, *LiteralError); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

			const FString PinKey = BuildOutputTargetCacheKey(Binding) + FString::Printf(TEXT("#%d"), Binding.ExpressionPinIndex); /* I18N-EXEMPT: deferred codegen or compatibility path */
			if (BoundPins.Contains(PinKey))
			{
				OutError = FString::Printf(TEXT("Output target pin '%s' is bound more than once."), *Binding.TargetText); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FExpressionInput* TargetInput = TargetExpression->GetInput(Binding.ExpressionPinIndex);
			if (!TargetInput)
			{
				OutError = FString::Printf(TEXT("Output target '%s' does not have Pin[%d]."), *Binding.TargetText, Binding.ExpressionPinIndex); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			Private::FCodeValue SourceValue;
			SourceValue.Expression = SourceExpression;
			SourceValue.OutputIndex = SourceOutputIndex;
			Private::FCodeValue RoutedValue = Private::CreateOutputRerouteValue(
				SourceExpression->Material,
				SourceExpression->Function,
				SourceValue,
				Binding.TargetText,
				Binding.ExpressionPinIndex);
			Private::ConnectCodeValueToInput(*TargetInput, RoutedValue);
			BoundPins.Add(PinKey);
			return true;
		}

		const TCHAR* GetMaterialFunctionBlockKindText(const ETextShaderMaterialFunctionKind Kind)
		{
			return UE::DreamShader::LexToString(Kind);
		}

		EMaterialFunctionUsage GetUnrealMaterialFunctionUsage(const ETextShaderMaterialFunctionKind Kind)
		{
			switch (Kind)
			{
			case ETextShaderMaterialFunctionKind::MaterialLayer:
				return EMaterialFunctionUsage::MaterialLayer;
			case ETextShaderMaterialFunctionKind::MaterialLayerBlend:
				return EMaterialFunctionUsage::MaterialLayerBlend;
			case ETextShaderMaterialFunctionKind::ShaderFunction:
			default:
				return EMaterialFunctionUsage::Default;
			}
		}

		static constexpr int32 DreamShaderAcceptableLayerMaterialAttributesInputs = 1;
		static constexpr int32 DreamShaderAcceptableBlendMaterialAttributesInputs = 2;

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
		EBlendInputRelevance ResolveMaterialLayerBlendInputRelevance(
			const FTextShaderMaterialFunctionDefinition& FunctionDefinition,
			const FTextShaderFunctionParameter& InputDefinition,
			const int32 MaterialAttributesInputIndex)
		{
			if (FunctionDefinition.Kind != ETextShaderMaterialFunctionKind::MaterialLayerBlend
				|| !Private::IsMaterialAttributesType(InputDefinition.Type))
			{
				return EBlendInputRelevance::General;
			}

			FString NormalizedInputName = InputDefinition.Name;
			NormalizedInputName.ReplaceInline(TEXT(" "), TEXT(""));
			NormalizedInputName.ReplaceInline(TEXT("_"), TEXT(""));
			NormalizedInputName.ReplaceInline(TEXT("-"), TEXT(""));
			if (NormalizedInputName.Equals(TEXT("Top"), ESearchCase::IgnoreCase)
				|| NormalizedInputName.Equals(TEXT("TopLayer"), ESearchCase::IgnoreCase)
				|| InputDefinition.Name.Equals(TopMaterialBlendInputName, ESearchCase::IgnoreCase))
			{
				return EBlendInputRelevance::Top;
			}
			if (NormalizedInputName.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase)
				|| NormalizedInputName.Equals(TEXT("BottomLayer"), ESearchCase::IgnoreCase)
				|| NormalizedInputName.Equals(TEXT("Base"), ESearchCase::IgnoreCase)
				|| NormalizedInputName.Equals(TEXT("BaseLayer"), ESearchCase::IgnoreCase)
				|| InputDefinition.Name.Equals(BottomMaterialBlendInputName, ESearchCase::IgnoreCase))
			{
				return EBlendInputRelevance::Bottom;
			}

			return MaterialAttributesInputIndex == 0
				? EBlendInputRelevance::Bottom
				: EBlendInputRelevance::Top;
		}
#endif

		bool ValidateMaterialLayerFunctionDefinition(const FTextShaderMaterialFunctionDefinition& FunctionDefinition, FString& OutError)
		{
			const TCHAR* BlockKind = GetMaterialFunctionBlockKindText(FunctionDefinition.Kind);
			if (FunctionDefinition.Kind == ETextShaderMaterialFunctionKind::ShaderFunction)
			{
				return true;
			}

			if (FunctionDefinition.Outputs.Num() != 1 || !Private::IsMaterialAttributesType(FunctionDefinition.Outputs[0].Type))
			{
				OutError = FString::Printf(TEXT("%s '%s' must declare exactly one MaterialAttributes output."), BlockKind, *FunctionDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			int32 MaterialAttributesInputCount = 0;
			for (const FTextShaderFunctionParameter& InputDefinition : FunctionDefinition.Inputs)
			{
				if (Private::IsMaterialAttributesType(InputDefinition.Type))
				{
					++MaterialAttributesInputCount;
				}
			}

			if (FunctionDefinition.Kind == ETextShaderMaterialFunctionKind::MaterialLayer
				&& (FunctionDefinition.Inputs.Num() > DreamShaderAcceptableLayerMaterialAttributesInputs
					|| MaterialAttributesInputCount != FunctionDefinition.Inputs.Num()))
			{
				OutError = FString::Printf(TEXT("ShaderLayer '%s' must declare at most one input, and it must be MaterialAttributes. Use Properties for layer controls."), *FunctionDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			if (FunctionDefinition.Kind == ETextShaderMaterialFunctionKind::MaterialLayerBlend)
			{
				if (FunctionDefinition.Inputs.Num() != DreamShaderAcceptableBlendMaterialAttributesInputs
					|| MaterialAttributesInputCount != DreamShaderAcceptableBlendMaterialAttributesInputs)
				{
					OutError = FString::Printf(TEXT("ShaderLayerBlend '%s' must declare exactly two inputs, both MaterialAttributes. Use Properties for blend controls."), *FunctionDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
			}

			return true;
		}

		void CacheMaterialFunctionInterfaceIds(
			const UMaterialFunction* MaterialFunction,
			TMap<FName, FGuid>& OutInputIdsByName,
			TMap<FName, FGuid>& OutOutputIdsByName)
		{
			OutInputIdsByName.Reset();
			OutOutputIdsByName.Reset();
			if (!MaterialFunction)
			{
				return;
			}

			for (UMaterialExpression* Expression : MaterialFunction->GetExpressions())
			{
				if (const UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(Expression))
				{
					if (!InputExpression->InputName.IsNone() && InputExpression->Id.IsValid())
					{
						OutInputIdsByName.Add(InputExpression->InputName, InputExpression->Id);
					}
				}
				else if (const UMaterialExpressionFunctionOutput* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(Expression))
				{
					if (!OutputExpression->OutputName.IsNone() && OutputExpression->Id.IsValid())
					{
						OutOutputIdsByName.Add(OutputExpression->OutputName, OutputExpression->Id);
					}
				}
			}
		}

		void RestoreOrGenerateFunctionInputId(
			UMaterialExpressionFunctionInput* InputExpression,
			const TMap<FName, FGuid>& InputIdsByName)
		{
			if (!InputExpression)
			{
				return;
			}

			if (const FGuid* ExistingId = InputIdsByName.Find(InputExpression->InputName))
			{
				InputExpression->Id = *ExistingId;
			}

			InputExpression->ConditionallyGenerateId(false);
		}

		void RestoreOrGenerateFunctionOutputId(
			UMaterialExpressionFunctionOutput* OutputExpression,
			const TMap<FName, FGuid>& OutputIdsByName)
		{
			if (!OutputExpression)
			{
				return;
			}

			if (const FGuid* ExistingId = OutputIdsByName.Find(OutputExpression->OutputName))
			{
				OutputExpression->Id = *ExistingId;
			}

			OutputExpression->ConditionallyGenerateId(false);
		}

		bool AppendInitializedOutputStatements(
			const TArray<FTextShaderVariableDeclaration>& OutputDeclarations,
			TArray<Private::FCodeStatement>& InOutStatements,
			FString& OutError)
		{
			for (const FTextShaderVariableDeclaration& OutputDeclaration : OutputDeclarations)
			{
				if (!OutputDeclaration.bHasDefaultValue)
				{
					continue;
				}

				Private::FCodeStatement Statement;
				if (!Private::MakeCodeDeclarationStatement(
					OutputDeclaration.Type,
					OutputDeclaration.Name,
					OutputDeclaration.DefaultValueText,
					Statement,
					OutError))
				{
					OutError = FString::Printf(TEXT("Output '%s': %s"), *OutputDeclaration.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				InOutStatements.Add(Statement);
			}

			return true;
		}

		bool GenerateMaterialFunctionAsset(
			const FString& SourceFilePath,
			const FString& PreparedSource,
			const FString& SourceHash,
			const FTextShaderDefinition& RootDefinition,
			const FTextShaderMaterialFunctionDefinition& FunctionDefinition,
			const bool bForce,
			const bool bTransient,
			FString& OutGeneratedAssetPath,
			FString& OutError)
		{
		FScopedSlowTask FunctionSlowTask(
			10.0f,
			FText::Format(
				LOCTEXT("GeneratingDreamShaderFunction", "Generating DreamShader function '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
			if (!IsRunningCommandlet())
			{
				FunctionSlowTask.MakeDialogDelayed(0.25f);
			}

		const TCHAR* BlockKind = GetMaterialFunctionBlockKindText(FunctionDefinition.Kind);
		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ValidatingDreamShaderFunction", "Validating {0} '{1}'..."),
				FText::FromString(BlockKind),
				FText::FromString(FunctionDefinition.Name)));
			if (FunctionDefinition.Outputs.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s '%s' must declare at least one output."), BlockKind, *FunctionDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			if (!ValidateMaterialLayerFunctionDefinition(FunctionDefinition, OutError))
			{
				return false;
			}

			if (FunctionDefinition.Code.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s '%s' must provide a Graph block."), BlockKind, *FunctionDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			UMaterialFunction* MaterialFunction = nullptr;
			if (!Private::CreateOrReuseMaterialFunction(FunctionDefinition, MaterialFunction, OutError, bTransient) || !MaterialFunction)
			{
				return false;
			}

			const EMaterialFunctionUsage ExpectedUsage = GetUnrealMaterialFunctionUsage(FunctionDefinition.Kind);
			if (!bForce
				&& Private::IsGeneratedAssetSourceCurrent(MaterialFunction, SourceFilePath, SourceHash)
				&& MaterialFunction->GetMaterialFunctionUsage() == ExpectedUsage)
			{
				OutGeneratedAssetPath = MaterialFunction->GetPathName();
				return true;
			}

			MaterialFunction->Modify();
			MaterialFunction->SetMaterialFunctionUsage(ExpectedUsage);
			TMap<FName, FGuid> ExistingInputIdsByName;
			TMap<FName, FGuid> ExistingOutputIdsByName;
			CacheMaterialFunctionInterfaceIds(MaterialFunction, ExistingInputIdsByName, ExistingOutputIdsByName);
		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ClearingOldFunctionGraph", "Clearing old function graph '{0}'..."),
				FText::FromString(MaterialFunction->GetName())));
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
					OutError = FString::Printf(TEXT("%s '%s': ExposeToLibrary must be true or false."), BlockKind, *FunctionDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			TMap<FString, UMaterialExpression*> GeneratedExpressionsByVariable;
			TMap<FString, FString> RegionByVariable;
			TSet<FString> SeenPropertyNames;
			for (const FTextShaderPropertyDefinition& Property : FunctionDefinition.Properties)
			{
				bool bNameConflict = false;
				for (const FString& ExistingPropertyName : SeenPropertyNames)
				{
					if (ExistingPropertyName.Equals(Property.Name, ESearchCase::IgnoreCase))
					{
						bNameConflict = true;
						break;
					}
				}

				for (const FTextShaderFunctionParameter& InputDefinition : FunctionDefinition.Inputs)
				{
					if (InputDefinition.Name.Equals(Property.Name, ESearchCase::IgnoreCase))
					{
						bNameConflict = true;
						break;
					}
				}

				if (bNameConflict)
				{
					OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("%s '%s' property '%s' conflicts with another property or input name."),
						BlockKind,
						*FunctionDefinition.Name,
						*Property.Name);
					return false;
				}

				SeenPropertyNames.Add(Property.Name);
			}

			TMap<FString, UMaterialExpressionFunctionInput*> GeneratedInputExpressions;
			int32 InputPositionY = -260;
			int32 MaterialAttributesInputIndex = 0;
		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("CreatingInputsForFunction", "Creating inputs for '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
			for (int32 InputIndex = 0; InputIndex < FunctionDefinition.Inputs.Num(); ++InputIndex)
			{
				const FTextShaderFunctionParameter& InputDefinition = FunctionDefinition.Inputs[InputIndex];

				int32 ComponentCount = 0;
				bool bIsTextureObject = false;
				bool bIsSubstrateMaterial = false;
				ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
				int32 FunctionInputTypeValue = 0;
				if (!Private::TryResolveMaterialFunctionParameterType(
					InputDefinition.Type,
					ComponentCount,
					bIsTextureObject,
					FunctionInputTypeValue,
					bIsSubstrateMaterial))
				{
					if (Private::IsSubstrateMaterialType(InputDefinition.Type))
					{
						OutError = FString::Printf(TEXT("%s '%s' input '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
					OutError = FString::Printf(TEXT("%s '%s' input '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name, *InputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
				if (bIsTextureObject)
				{
					verify(Private::TryResolveCodeDeclaredType(InputDefinition.Type, ComponentCount, bIsTextureObject, TextureType));
				}

				auto* InputExpression = Cast<UMaterialExpressionFunctionInput>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionFunctionInput::StaticClass(), -800, InputPositionY));
				if (!InputExpression)
				{
					OutError = FString::Printf(TEXT("%s '%s' failed to create input '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				InputExpression->InputName = FName(*InputDefinition.Name);
				InputExpression->InputType = static_cast<EFunctionInputType>(FunctionInputTypeValue);
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
				InputExpression->BlendInputRelevance = ResolveMaterialLayerBlendInputRelevance(
					FunctionDefinition,
					InputDefinition,
					MaterialAttributesInputIndex);
#endif
				InputExpression->Description = InputDefinition.Metadata.Description;
				InputExpression->SortPriority = InputDefinition.Metadata.bHasSortPriority
					? InputDefinition.Metadata.SortPriority
					: InputIndex;
				RestoreOrGenerateFunctionInputId(InputExpression, ExistingInputIdsByName);
				if (Private::IsMaterialAttributesType(InputDefinition.Type))
				{
					++MaterialAttributesInputIndex;
				}

				Private::FCodeValue InputValue;
				InputValue.Expression = InputExpression;
				InputValue.ComponentCount = ComponentCount;
				InputValue.bIsTextureObject = bIsTextureObject;
				InputValue.TextureType = TextureType;
				InputValue.bIsMaterialAttributes = ComponentCount == 0 && !bIsTextureObject && !bIsSubstrateMaterial;
				InputValue.bIsSubstrateMaterial = bIsSubstrateMaterial;
				GeneratedValues.Add(InputDefinition.Name, InputValue);
				GeneratedInputExpressions.Add(InputDefinition.Name, InputExpression);
				GeneratedExpressionsByVariable.Add(InputDefinition.Name, InputExpression);
				InputPositionY += 180;
			}

			for (const FTextShaderFunctionParameter& InputDefinition : FunctionDefinition.Inputs)
			{
				UMaterialExpressionFunctionInput** InputExpressionPtr = GeneratedInputExpressions.Find(InputDefinition.Name);
				const Private::FCodeValue* InputValue = GeneratedValues.Find(InputDefinition.Name);
				if (!InputExpressionPtr || !*InputExpressionPtr || !InputValue)
				{
					OutError = FString::Printf(TEXT("%s '%s' failed to resolve generated input '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					InputValue->TextureType,
					&FunctionDefinition.Properties,
					GeneratedValues,
					PreviewError))
				{
					OutError = FString::Printf(TEXT("%s '%s' input '%s': %s"), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name, *PreviewError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
			}

			int32 MaterialAttributesSeedPositionY = 260;
			for (const FTextShaderFunctionParameter& OutputDefinition : FunctionDefinition.Outputs)
			{
				if (Private::IsMaterialAttributesType(OutputDefinition.Type))
				{
					FString SeedError;
					if (!SeedMaterialAttributesGraphValue(
						nullptr,
						MaterialFunction,
						OutputDefinition.Name,
						GeneratedValues,
						MaterialAttributesSeedPositionY,
						SeedError))
					{
						OutError = FString::Printf(TEXT("%s '%s' output '%s': %s"), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *SeedError); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
				}
				else if (Private::IsSubstrateMaterialType(OutputDefinition.Type) && !OutputDefinition.bHasDefaultValue)
				{
					continue;
				}
			}

			if (!FunctionDefinition.Code.IsEmpty())
			{
		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ParsingFunctionGraphBlock", "Parsing Graph block for '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
				FString CodeSourceFilePath;
				int32 CodeStartLine = 1;
				int32 CodeStartColumn = 1;
				ResolveCodeBlockLocation(SourceFilePath, PreparedSource, FunctionDefinition.CodeStartIndex, CodeSourceFilePath, CodeStartLine, CodeStartColumn);
				TArray<Private::FCodeStatement> CodeStatements;
				FString CodeParseError;
				int32 CodeParseErrorLine = 0;
				int32 CodeParseErrorColumn = 0;
				if (!Private::ParseCodeStatements(FunctionDefinition.Code, CodeStatements, CodeParseError, &CodeParseErrorLine, &CodeParseErrorColumn))
				{
					OutError = FormatCodeBlockError(
						SourceFilePath,
						CodeSourceFilePath,
						CodeStartLine,
						CodeStartColumn,
						FString::Printf(TEXT("%s '%s': %s"), BlockKind, *FunctionDefinition.Name, *CodeParseError), /* I18N-EXEMPT: deferred codegen or compatibility path */
						CodeParseErrorLine,
						CodeParseErrorColumn);
					return false;
				}
				ApplyStatementRegionsRecursive(CodeStatements, FunctionDefinition.GraphRegions);

				Private::FCodeGraphBuilder CodeGraphBuilder(
					nullptr,
					MaterialFunction,
					RootDefinition,
					SourceFilePath,
					Private::BuildGeneratedIncludeVirtualPath(SourceFilePath),
					&FunctionDefinition.Properties,
					CodeSourceFilePath,
					CodeStartLine,
					CodeStartColumn);
				FString CodeBuildError;
		FunctionSlowTask.EnterProgressFrame(
			2.0f,
			FText::Format(
				LOCTEXT("CreatingFunctionGraphNodes", "Creating Graph nodes for '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
				if (!CodeGraphBuilder.Build(CodeStatements, GeneratedValues, CodeBuildError))
				{
					OutError = CodeBuildError;
					return false;
				}
				GeneratedExpressionsByVariable.Append(CodeGraphBuilder.GetGeneratedExpressionsByVariable());
				RegionByVariable = CodeGraphBuilder.GetRegionByVariable();
			}
			else
			{
				const FTextShaderFunctionParameter& PrimaryOutput = FunctionDefinition.Outputs[0];
				ECustomMaterialOutputType OutputType = CMOT_Float1;
				if (Private::IsSubstrateMaterialType(PrimaryOutput.Type))
				{
					OutError = Private::IsSubstrateMaterialTypeSupported()
						? FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use a Graph block and Substrate.* nodes."), BlockKind, *FunctionDefinition.Name, *PrimaryOutput.Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
						: FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), BlockKind, *FunctionDefinition.Name, *PrimaryOutput.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
				if (!Private::TryResolveCustomOutputType(PrimaryOutput.Type, OutputType))
				{
					OutError = FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *PrimaryOutput.Name, *PrimaryOutput.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				auto* CustomExpression = Cast<UMaterialExpressionCustom>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionCustom::StaticClass(), 120, 0));
				if (!CustomExpression)
				{
					OutError = FString::Printf(TEXT("%s '%s' failed to create the function Custom node."), BlockKind, *FunctionDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				CustomExpression->Description = FunctionDefinition.Name;
				CustomExpression->OutputType = OutputType;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
				CustomExpression->ShowCode = false;
#endif
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
					OutError = FString::Printf(TEXT("%s '%s': %s"), BlockKind, *FunctionDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
				CustomExpression->Code = Private::EnsureTopLevelReturn(PreparedCustomCode);

				if (bUsesGeneratedInclude)
				{
					CustomExpression->IncludeFilePaths.Add(Private::BuildGeneratedIncludeVirtualPath(SourceFilePath));
				}

				TMap<FString, UMaterialExpression*> GeneratedPropertyExpressions;
				TSet<FString> CreatingPropertyNames;
				int32 PropertyPositionY = -620;
				for (const FTextShaderFunctionParameter& InputDefinition : FunctionDefinition.Inputs)
				{
					const Private::FCodeValue* InputValue = GeneratedValues.Find(InputDefinition.Name);
					if (!InputValue || !InputValue->Expression)
					{
						OutError = FString::Printf(TEXT("%s '%s' failed to resolve generated input '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}

					FCustomInput Input;
					Input.InputName = FName(*InputDefinition.Name);
					CustomExpression->Inputs.Add(Input);
					Private::ConnectCodeValueToInput(CustomExpression->Inputs.Last().Input, *InputValue);
				}

				for (const FTextShaderPropertyDefinition& Property : FunctionDefinition.Properties)
				{
					if (!ContainsIdentifierReference(PreparedCustomCode, Property.Name))
					{
						continue;
					}

					FString PropertyExpressionError;
					UMaterialExpression* PropertyExpression = nullptr;
					if (!CreateReferencedPropertyExpression(
						nullptr,
						MaterialFunction,
						FunctionDefinition.Properties,
						Property,
						GeneratedPropertyExpressions,
						CreatingPropertyNames,
						PropertyPositionY,
						PropertyExpression,
						PropertyExpressionError))
					{
						OutError = FString::Printf(TEXT("%s '%s' property '%s': %s"), BlockKind, *FunctionDefinition.Name, *Property.Name, *PropertyExpressionError); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}

					FCustomInput Input;
					Input.InputName = FName(*Property.Name);
					CustomExpression->Inputs.Add(Input);
					CustomExpression->Inputs.Last().Input.Connect(GetPreferredOutputIndexForProperty(Property, PropertyExpression), PropertyExpression);
				}

				for (int32 OutputIndex = 1; OutputIndex < FunctionDefinition.Outputs.Num(); ++OutputIndex)
				{
					const FTextShaderFunctionParameter& OutputDefinition = FunctionDefinition.Outputs[OutputIndex];
					ECustomMaterialOutputType AdditionalOutputType = CMOT_Float1;
					if (Private::IsSubstrateMaterialType(OutputDefinition.Type))
					{
						OutError = Private::IsSubstrateMaterialTypeSupported()
							? FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use a Graph block and Substrate.* nodes."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
							: FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
					if (!Private::TryResolveCustomOutputType(OutputDefinition.Type, AdditionalOutputType))
					{
						OutError = FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}

					FCustomOutput Output;
					Output.OutputName = FName(*OutputDefinition.Name);
					Output.OutputType = AdditionalOutputType;
					CustomExpression->AdditionalOutputs.Add(Output);
				}

				Private::RebuildDreamShaderCustomOutputs(CustomExpression);

				Private::FCodeValue PrimaryOutputValue;
				PrimaryOutputValue.Expression = CustomExpression;
				PrimaryOutputValue.ComponentCount = 0;
				PrimaryOutputValue.bIsTextureObject = false;
				verify(Private::TryResolveCodeDeclaredType(PrimaryOutput.Type, PrimaryOutputValue.ComponentCount, PrimaryOutputValue.bIsTextureObject, PrimaryOutputValue.TextureType, PrimaryOutputValue.bIsSubstrateMaterial));
				PrimaryOutputValue.bIsMaterialAttributes = PrimaryOutputValue.ComponentCount == 0 && !PrimaryOutputValue.bIsTextureObject && !PrimaryOutputValue.bIsSubstrateMaterial;
				GeneratedValues.Add(PrimaryOutput.Name, PrimaryOutputValue);

				for (int32 OutputIndex = 1; OutputIndex < FunctionDefinition.Outputs.Num(); ++OutputIndex)
				{
					const FTextShaderFunctionParameter& OutputDefinition = FunctionDefinition.Outputs[OutputIndex];
					Private::FCodeValue OutputValue;
					OutputValue.Expression = CustomExpression;
					OutputValue.OutputIndex = OutputIndex;
					verify(Private::TryResolveCodeDeclaredType(OutputDefinition.Type, OutputValue.ComponentCount, OutputValue.bIsTextureObject, OutputValue.TextureType, OutputValue.bIsSubstrateMaterial));
					OutputValue.bIsMaterialAttributes = OutputValue.ComponentCount == 0 && !OutputValue.bIsTextureObject && !OutputValue.bIsSubstrateMaterial;
					GeneratedValues.Add(OutputDefinition.Name, OutputValue);
				}
			}

			int32 OutputPositionY = -120;
		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ConnectingFunctionOutputs", "Connecting outputs for '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
			for (int32 OutputIndex = 0; OutputIndex < FunctionDefinition.Outputs.Num(); ++OutputIndex)
			{
				const FTextShaderFunctionParameter& OutputDefinition = FunctionDefinition.Outputs[OutputIndex];
				const Private::FCodeValue* OutputValue = GeneratedValues.Find(OutputDefinition.Name);
				if (!OutputValue || !OutputValue->Expression)
				{
					OutError = FString::Printf(TEXT("%s '%s' output '%s' was never assigned an expression."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				int32 ExpectedComponentCount = 0;
				bool bExpectedTexture = false;
				bool bExpectedSubstrate = false;
				int32 IgnoredFunctionInputType = 0;
				if (!Private::TryResolveMaterialFunctionParameterType(
					OutputDefinition.Type,
					ExpectedComponentCount,
					bExpectedTexture,
					IgnoredFunctionInputType,
					bExpectedSubstrate))
				{
					if (Private::IsSubstrateMaterialType(OutputDefinition.Type) && !Private::IsSubstrateMaterialTypeSupported())
					{
						OutError = FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
					OutError = FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
				if (bExpectedTexture)
				{
					verify(Private::TryResolveCodeDeclaredType(OutputDefinition.Type, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType));
				}

				if (bExpectedTexture != OutputValue->bIsTextureObject
					|| (bExpectedTexture && ExpectedTextureType != OutputValue->TextureType)
					|| bExpectedSubstrate != OutputValue->bIsSubstrateMaterial
					|| ((ExpectedComponentCount == 0 && !bExpectedTexture && !bExpectedSubstrate) != OutputValue->bIsMaterialAttributes)
					|| (!bExpectedTexture && ExpectedComponentCount != OutputValue->ComponentCount))
				{
					OutError = FString::Printf(TEXT("%s '%s' output '%s' does not match its declared type '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				auto* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionFunctionOutput::StaticClass(), 900, OutputPositionY));
				if (!OutputExpression)
				{
					OutError = FString::Printf(TEXT("%s '%s' failed to create output '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				OutputExpression->OutputName = FName(*OutputDefinition.Name);
				OutputExpression->Description = OutputDefinition.Metadata.Description;
				OutputExpression->SortPriority = OutputDefinition.Metadata.bHasSortPriority
					? OutputDefinition.Metadata.SortPriority
					: OutputIndex;
				RestoreOrGenerateFunctionOutputId(OutputExpression, ExistingOutputIdsByName);
				const Private::FCodeValue RoutedOutputValue = Private::CreateOutputRerouteValue(
					nullptr,
					MaterialFunction,
					*OutputValue,
					OutputDefinition.Name,
					OutputIndex);
				Private::ConnectCodeValueToInput(OutputExpression->A, RoutedOutputValue);
				GeneratedExpressionsByVariable.Add(OutputDefinition.Name, OutputExpression);
				OutputPositionY += 180;
			}

			if (bTransient)
			{
				FunctionSlowTask.EnterProgressFrame(1.0f);
			}
			else
			{
		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("LayingOutFunction", "Laying out '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
				Private::LayoutGeneratedExpressions(
					nullptr,
					MaterialFunction,
					&FunctionDefinition.Layout,
					GeneratedExpressionsByVariable.IsEmpty() ? nullptr : &GeneratedExpressionsByVariable,
					RegionByVariable.IsEmpty() ? nullptr : &RegionByVariable);
			}
		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("UpdatingFunction", "Updating '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
			UMaterialEditingLibrary::UpdateMaterialFunction(MaterialFunction, nullptr);
			MaterialFunction->PostEditChange();

			if (bTransient)
			{
				FunctionSlowTask.EnterProgressFrame(1.0f);
				// Modify()/PostEditChange dirtied the in-memory package; clear it so no save-all or
				// exit prompt can silently persist an in-memory material function.
				MaterialFunction->GetPackage()->SetDirtyFlag(false);
			}
			else
			{
				MaterialFunction->MarkPackageDirty();
				Private::ApplySourceMetadata(MaterialFunction, SourceFilePath, SourceHash);

		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("SavingFunction", "Saving '{0}'..."),
				FText::FromString(FunctionDefinition.Name)));
				FString SaveError;
				if (!Private::SaveAssetPackage(MaterialFunction, SaveError))
				{
					OutError = SaveError;
					return false;
				}
			}

			OutGeneratedAssetPath = MaterialFunction->GetPathName();
			return true;
		}
	}

	bool FMaterialGenerator::GenerateAssetsFromFile(const FString& InSourceFilePath, FString& OutMessage, const bool bForce, const bool bTransient)
	{
		const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		FScopedSlowTask SourceSlowTask(
			6.0f,
			FText::Format(
				LOCTEXT("CompilingDreamShaderSource", "Compiling DreamShader source '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (!IsRunningCommandlet())
		{
			SourceSlowTask.MakeDialogDelayed(0.35f);
		}

		SourceSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ReadingDreamShaderSource", "Reading DreamShader source '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (UE::DreamShader::IsDreamShaderHeaderFile(SourceFilePath))
		{
			OutMessage = FString::Printf(TEXT("DreamShader header '%s' does not generate assets directly. Recompile dependent .dsm or .dsf files instead."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
		SourceSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ParsingDreamShaderSource", "Parsing DreamShader source '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (!FTextShaderParser::Parse(SourceText, Definition, ParseError))
		{
			OutMessage = FormatParseErrorWithSourceLocation(SourceFilePath, SourceText, ParseError);
			return false;
		}

		FString RootFallbackReason;
		Private::ApplyDefaultRootFromSourceFile(SourceFilePath, Definition, &RootFallbackReason);
		if (!RootFallbackReason.IsEmpty())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("%s"), *RootFallbackReason);
		}

		const FString SourceHash = Private::BuildSourceHash(SourceText);

		if (UE::DreamShader::IsDreamShaderFunctionFile(SourceFilePath) && !Definition.Name.IsEmpty())
		{
			OutMessage = FString::Printf(TEXT("%s: .dsf files cannot define top-level Shader blocks."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		bool bGeneratedHelperInclude = false;
		SourceSlowTask.EnterProgressFrame(1.0f, LOCTEXT("PreparingDreamShaderGeneratedAssets", "Preparing DreamShader generated assets..."));
		if (!Definition.Functions.IsEmpty())
		{
			FString IncludeWriteError;
			if (!Private::WriteGeneratedInclude(SourceFilePath, Definition, IncludeWriteError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *IncludeWriteError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			bGeneratedHelperInclude = true;
		}

		TArray<FString> GeneratedAssetMessages;
		SourceSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("GeneratingDreamShaderFunctionAssets", "Generating {0} DreamShader function asset{1}..."),
				FText::AsNumber(Definition.MaterialFunctions.Num()),
				FText::FromString(Definition.MaterialFunctions.Num() == 1 ? TEXT("") : TEXT("s"))));
		for (const FTextShaderMaterialFunctionDefinition& FunctionDefinition : Definition.MaterialFunctions)
		{
			FString GeneratedAssetPath;
			FString FunctionError;
			if (!GenerateMaterialFunctionAsset(SourceFilePath, SourceText, SourceHash, Definition, FunctionDefinition, bForce, bTransient, GeneratedAssetPath, FunctionError))
			{
				OutMessage = FormatGenerateError(SourceFilePath, FunctionError);
				return false;
			}

			GeneratedAssetMessages.Add(FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("Generated %s %s from %s."),
				GetMaterialFunctionBlockKindText(FunctionDefinition.Kind),
				*GeneratedAssetPath,
				*SourceFilePath));
		}

		if (!Definition.Name.IsEmpty())
		{
			FString MaterialMessage;
		SourceSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("GeneratingDreamShaderMaterial", "Generating DreamShader material '{0}'..."),
				FText::FromString(Definition.Name)));
			if (!GenerateMaterialFromFile(SourceFilePath, MaterialMessage, bForce, bTransient))
			{
				OutMessage = MaterialMessage;
				return false;
			}

			GeneratedAssetMessages.Add(MaterialMessage);
		}

		SourceSlowTask.EnterProgressFrame(1.0f, LOCTEXT("FinishingDreamShaderCompile", "Finishing DreamShader compile..."));
		if (GeneratedAssetMessages.IsEmpty())
		{
			if (bGeneratedHelperInclude)
			{
				OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("Generated DreamShader helper include '%s' from %s."),
					*Private::BuildGeneratedIncludeVirtualPath(SourceFilePath),
					*SourceFilePath);
				return true;
			}

			if (!Definition.VirtualFunctions.IsEmpty())
			{
				OutMessage = FString::Printf(TEXT("DreamShader file '%s' contains VirtualFunction declarations only; no assets were generated."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return true;
			}

			if (!Definition.GraphFunctions.IsEmpty())
			{
				OutMessage = FString::Printf(TEXT("DreamShader file '%s' contains GraphFunction declarations only; no assets were generated."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return true;
			}

			OutMessage = FString::Printf(TEXT("DreamShader file '%s' did not contain any material, ShaderFunction, ShaderLayer, or ShaderLayerBlend assets to generate."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		OutMessage = FString::Join(GeneratedAssetMessages, TEXT("\n"));
		if (!Definition.Warnings.IsEmpty())
		{
			OutMessage += TEXT("\nWarnings:\n");
			OutMessage += FString::Join(Definition.Warnings, TEXT("\n"));
		}
		return true;
	}

	namespace Private
	{
		static FString GetHlslTypeForCustomOutput(const ECustomMaterialOutputType OutputType)
		{
			switch (OutputType)
			{
			case CMOT_Float1: return TEXT("float");
			case CMOT_Float2: return TEXT("float2");
			case CMOT_Float3: return TEXT("float3");
			default:          return TEXT("float4");
			}
		}

		enum class EResolvedBackend : uint8 { Graph, ThinCustom };

		// Decide which backend materializes the file: an explicit Settings = { Backend = "..." } wins;
		// otherwise the project's DefaultBackend applies.
		//
		// STAGE 6 FLIP: "Instance" -- both the explicit Backend setting and the project default -- is
		// a deprecation-window ALIAS for ThinCustom. ThinCustom reaches full Instance parity (textures,
		// UI/PostProcess domains, scene reads, MaterialAttributes, the state-read builtin wave) with
		// bit-identical SM6 rendering, so existing Instance sources keep generating unchanged -- they
		// just get the real-graph hidden base + thin instance instead of the graphless host + injected
		// resource. The legacy Instance generator is no longer reachable and is deleted in Stage 7.
		static bool ResolveRequestedBackend(const FTextShaderDefinition& Definition, EResolvedBackend& OutBackend, bool& bOutExplicit, FString& OutError)
		{
			OutBackend = EResolvedBackend::Graph;
			bOutExplicit = false;
			FString Value;
			if (!Definition.TryGetSetting(TEXT("Backend"), Value))
			{
				if (const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>())
				{
					switch (Settings->DefaultBackend)
					{
					case EDreamShaderDefaultBackend::Instance:   OutBackend = EResolvedBackend::ThinCustom; break;
					case EDreamShaderDefaultBackend::ThinCustom: OutBackend = EResolvedBackend::ThinCustom; break;
					default:                                     OutBackend = EResolvedBackend::Graph; break;
					}
				}
				return true;
			}

			bOutExplicit = true;
			const FString Trimmed = Value.TrimStartAndEnd().TrimQuotes();
			if (Trimmed.Equals(TEXT("Instance"), ESearchCase::IgnoreCase))
			{
				OutBackend = EResolvedBackend::ThinCustom;
				return true;
			}
			if (Trimmed.Equals(TEXT("ThinCustom"), ESearchCase::IgnoreCase))
			{
				OutBackend = EResolvedBackend::ThinCustom;
				return true;
			}
			if (Trimmed.Equals(TEXT("Graph"), ESearchCase::IgnoreCase) || Trimmed.IsEmpty())
			{
				return true;
			}

			OutError = FString::Printf(TEXT("Unsupported Backend '%s'. Supported values: Graph, Instance, ThinCustom."), *Value); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		// The single committed parent material every instance material derives from. Created on first
		// use and saved into the plugin's Content mount; its graph stays empty because the instance
		// resource overrides property compilation for every DSL-bound output.
		// Parameter rows in the material instance editor are only marked visible when the parent
		// material opts into the new HLSL generator (GetVisibleMaterialParameters trusts the
		// enumerated parameter list instead of walking the — here nonexistent — expression graph).
		// Actual translation stays on the legacy path: FDreamShaderInstanceResource overrides
		// IsUsingNewHLSLGenerator to false. Requires r.Material.Translator.EnableNew=1.
		static void EnsureHostParameterVisibilityFlag(UMaterial* Host)
		{
			if (!Host->bEnableNewHLSLGenerator)
			{
				Host->bEnableNewHLSLGenerator = true;
				Host->MarkPackageDirty();
				FString SaveError;
				if (!SaveAssetPackage(Host, SaveError))
				{
					UE_LOG(LogDreamShader, Warning,
						TEXT("Failed to persist the parameter-visibility flag on the instance host material: %s"), *SaveError);
				}
			}
		}

	}

	// Shared Graph-backend construction: applies settings, builds the node graph (decomposed Graph
	// block or whole-surface single Custom node), lays out and recompiles. Extracted so both the
	// visible Graph material path and the ThinCustom-on-hidden-base path build an identical graph
	// onto their target UMaterial. Caller owns creation, source-hash skip, and persistence.
	static bool PopulateMaterialGraphFromDefinition(
		UMaterial* Material,
		const FTextShaderDefinition& Definition,
		const FString& SourceFilePath,
		const FString& SourceText,
		const TArray<Private::FResolvedNamedOutput>& NamedOutputs,
		bool bUsesReturn,
		ECustomMaterialOutputType ReturnOutputType,
		bool bReturnIsSubstrateMaterial,
		bool bUsesFrontMaterial,
		bool bTransient,
		FScopedSlowTask& MaterialSlowTask,
		FString& OutMessage)
	{
		Material->Modify();
		MaterialSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Clearing old material graph '%s'..."), *Material->GetName()))); /* I18N-EXEMPT: deferred codegen or compatibility path */
		Private::ClearMaterialExpressions(Material);
		Private::ResetMaterialToDefaults(Material);

		FString SettingsError;
		if (!Private::ApplySettings(Material, Definition, SettingsError))
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SettingsError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}
		if (bUsesFrontMaterial)
		{
			FString ShadingModelValue;
			if (Definition.TryGetSetting(TEXT("ShadingModel"), ShadingModelValue)
				&& !ShadingModelValue.TrimStartAndEnd().Equals(TEXT("Substrate"), ESearchCase::IgnoreCase)
				&& !ShadingModelValue.TrimStartAndEnd().Equals(TEXT("Strata"), ESearchCase::IgnoreCase))
			{
				OutMessage = FString::Printf(TEXT("%s: Base.FrontMaterial requires ShadingModel=\"Substrate\" or no explicit ShadingModel setting."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			Material->SetShadingModel(MSM_Strata);
		}

		TMap<FString, UMaterialExpression*> GeneratedOutputTargetExpressions;
		TSet<FString> BoundOutputTargetPins;
		TMap<FString, Private::FCodeValue> GeneratedCodeValues;
		TMap<FString, UMaterialExpression*> GeneratedExpressionsByVariable;
		TMap<FString, FString> RegionByVariable;
		int32 OutputTargetPositionY = 200;
		TSet<FString> SeenPropertyNames;
		for (const FTextShaderPropertyDefinition& Property : Definition.Properties)
		{
			bool bNameConflict = false;
			for (const FString& ExistingPropertyName : SeenPropertyNames)
			{
				if (ExistingPropertyName.Equals(Property.Name, ESearchCase::IgnoreCase))
				{
					bNameConflict = true;
					break;
				}
			}

			if (bNameConflict)
			{
				OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("%s: Property '%s' is declared more than once. Property names must be unique."),
					*SourceFilePath,
					*Property.Name);
				return false;
			}

			SeenPropertyNames.Add(Property.Name);
		}

		int32 MaterialAttributesSeedPositionY = OutputTargetPositionY;
		for (const FTextShaderVariableDeclaration& OutputDeclaration : Definition.OutputDeclarations)
		{
			if (!OutputDeclaration.bHasDefaultValue && Private::IsMaterialAttributesType(OutputDeclaration.Type))
			{
				FString SeedError;
				if (!SeedMaterialAttributesGraphValue(
					Material,
					nullptr,
					OutputDeclaration.Name,
					GeneratedCodeValues,
					MaterialAttributesSeedPositionY,
					SeedError))
				{
					OutMessage = FString::Printf(TEXT("%s: Output '%s': %s"), *SourceFilePath, *OutputDeclaration.Name, *SeedError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
			}
		}
		OutputTargetPositionY = FMath::Max(OutputTargetPositionY, MaterialAttributesSeedPositionY);

		bool bHasInitializedOutput = false;
		for (const FTextShaderVariableDeclaration& OutputDeclaration : Definition.OutputDeclarations)
		{
			if (OutputDeclaration.bHasDefaultValue)
			{
				bHasInitializedOutput = true;
				break;
			}
		}

		if (!Definition.Code.IsEmpty() || bHasInitializedOutput)
		{
			if (bUsesReturn)
			{
				OutMessage = FString::Printf(TEXT("%s: Graph blocks do not support binding Outputs to the reserved name 'return'."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			TArray<Private::FCodeStatement> CodeStatements;
			FString CodeParseError;
		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ParsingMaterialGraphBlock", "Parsing Graph block for '{0}'..."),
				FText::FromString(Definition.Name)));
			if (!AppendInitializedOutputStatements(Definition.OutputDeclarations, CodeStatements, CodeParseError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *CodeParseError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FString CodeSourceFilePath;
			int32 CodeStartLine = 1;
			int32 CodeStartColumn = 1;
			ResolveCodeBlockLocation(SourceFilePath, SourceText, Definition.CodeStartIndex, CodeSourceFilePath, CodeStartLine, CodeStartColumn);
			TArray<Private::FCodeStatement> GraphStatements;
			int32 CodeParseErrorLine = 0;
			int32 CodeParseErrorColumn = 0;
			if (!Definition.Code.IsEmpty() && !Private::ParseCodeStatements(Definition.Code, GraphStatements, CodeParseError, &CodeParseErrorLine, &CodeParseErrorColumn))
			{
				OutMessage = FormatCodeBlockError(
					SourceFilePath,
					CodeSourceFilePath,
					CodeStartLine,
					CodeStartColumn,
					CodeParseError,
					CodeParseErrorLine,
					CodeParseErrorColumn);
				return false;
			}
			ApplyStatementRegionsRecursive(GraphStatements, Definition.GraphRegions);
			CodeStatements.Append(GraphStatements);

			Private::FCodeGraphBuilder CodeGraphBuilder(
				Material,
				nullptr,
				Definition,
				SourceFilePath,
				Private::BuildGeneratedIncludeVirtualPath(SourceFilePath),
				nullptr,
				CodeSourceFilePath,
				CodeStartLine,
				CodeStartColumn);
			FString CodeBuildError;
		MaterialSlowTask.EnterProgressFrame(
			2.0f,
			FText::Format(
				LOCTEXT("CreatingMaterialGraphNodes", "Creating Graph nodes for '{0}'..."),
				FText::FromString(Definition.Name)));
			if (!CodeGraphBuilder.Build(CodeStatements, GeneratedCodeValues, CodeBuildError))
			{
				OutMessage = FormatGenerateError(SourceFilePath, CodeBuildError);
				return false;
			}
			GeneratedExpressionsByVariable = CodeGraphBuilder.GetGeneratedExpressionsByVariable();
			RegionByVariable = CodeGraphBuilder.GetRegionByVariable();

		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ConnectingMaterialOutputs", "Connecting material outputs for '{0}'..."),
				FText::FromString(Definition.Name)));
			for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
			{
				Private::FCodeValue OutputValue;
				FString OutputExpressionError;
				if (!CodeGraphBuilder.EvaluateOutputExpression(Binding.SourceText, OutputValue, OutputExpressionError)
					|| !OutputValue.Expression)
				{
					OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("%s: %s"),
						*SourceFilePath,
						*OutputExpressionError);
					return false;
				}

				int32 DeclaredComponents = 0;
				bool bDeclaredTexture = false;
				ETextShaderTextureType DeclaredTextureType = ETextShaderTextureType::Texture2D;
				bool bDeclaredSubstrate = false;
				if (Private::TryResolveOutputVariableComponentCount(Definition, Binding.SourceText, DeclaredComponents, bDeclaredTexture, DeclaredTextureType, bDeclaredSubstrate))
				{
					const bool bDeclaredMaterialAttributes = DeclaredComponents == 0 && !bDeclaredTexture && !bDeclaredSubstrate;
					if (bDeclaredTexture
						|| OutputValue.bIsTextureObject
						|| bDeclaredSubstrate != OutputValue.bIsSubstrateMaterial
						|| bDeclaredMaterialAttributes != OutputValue.bIsMaterialAttributes
						|| DeclaredComponents != OutputValue.ComponentCount)
					{
						OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					if (ResolvedProperty.bIsSubstrateMaterial)
					{
						if (!OutputValue.bIsSubstrateMaterial)
						{
							OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
								TEXT("%s: Material output '%s' expects a Substrate value."),
								*SourceFilePath,
								*Binding.MaterialProperty);
							return false;
						}
					}
					else if (ResolvedProperty.OutputType == CMOT_MaterialAttributes)
					{
						if (!OutputValue.bIsMaterialAttributes)
						{
							OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
								TEXT("%s: Material output '%s' expects a MaterialAttributes value."),
								*SourceFilePath,
								*Binding.MaterialProperty);
							return false;
						}
						Material->bUseMaterialAttributes = true;
					}
					else if (OutputValue.bIsSubstrateMaterial)
					{
						OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
							TEXT("%s: Material output '%s' expects a numeric value, but got Substrate."),
							*SourceFilePath,
							*Binding.MaterialProperty);
						return false;
					}

					FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(ResolvedProperty.Property);
					if (!MaterialInput)
					{
						OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
							TEXT("%s: Failed to find material property '%s' while connecting Graph output '%s'."),
							*SourceFilePath,
							*Binding.MaterialProperty,
							*Binding.SourceText);
						return false;
					}

					const Private::FCodeValue RoutedOutputValue = Private::CreateOutputRerouteValue(
						Material,
						nullptr,
						OutputValue,
						Binding.MaterialProperty,
						static_cast<int32>(ResolvedProperty.Property));
					Private::ConnectCodeValueToInput(*MaterialInput, RoutedOutputValue);
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
						OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *TargetError); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
				}
			}
		}
		else
		{
			if (bReturnIsSubstrateMaterial)
			{
				OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("%s: Base.FrontMaterial expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes."),
					*SourceFilePath);
				return false;
			}
			for (const Private::FResolvedNamedOutput& OutputDefinition : NamedOutputs)
			{
				if (OutputDefinition.bIsSubstrateMaterial)
				{
					OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("%s: Output '%s' is declared as Substrate and cannot be generated by a material Custom node. Use a Graph block and Substrate.* nodes."),
						*SourceFilePath,
						*OutputDefinition.Name);
					return false;
				}
			}

		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("CreatingMaterialCustomNode", "Creating Custom node for '{0}'..."),
				FText::FromString(Definition.Name)));
			auto* CustomExpression = Cast<UMaterialExpressionCustom>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCustom::StaticClass(), 120, 0));
			if (!CustomExpression)
			{
				OutMessage = FString::Printf(TEXT("%s: Failed to create the material Custom node."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			CustomExpression->Description = Definition.Name;
			CustomExpression->OutputType = bUsesReturn ? ReturnOutputType : CMOT_Float1;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
			CustomExpression->ShowCode = false;
#endif
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
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *OutMessage); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			CustomExpression->Code = Private::EnsureTopLevelReturn(PreparedCustomCode);

			if (bUsesGeneratedInclude)
			{
				CustomExpression->IncludeFilePaths.Add(Private::BuildGeneratedIncludeVirtualPath(SourceFilePath));
			}

			TMap<FString, UMaterialExpression*> GeneratedPropertyExpressions;
			TSet<FString> CreatingPropertyNames;
			int32 ParameterPositionY = -300;
			for (const FTextShaderPropertyDefinition& Property : Definition.Properties)
			{
				if (!ContainsIdentifierReference(PreparedCustomCode, Property.Name))
				{
					continue;
				}

				FString PropertyExpressionError;
				UMaterialExpression* PropertyExpression = nullptr;
				if (!CreateReferencedPropertyExpression(
					Material,
					nullptr,
					Definition.Properties,
					Property,
					GeneratedPropertyExpressions,
					CreatingPropertyNames,
					ParameterPositionY,
					PropertyExpression,
					PropertyExpressionError))
				{
					OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *PropertyExpressionError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				FCustomInput Input;
				Input.InputName = FName(*Property.Name);
				CustomExpression->Inputs.Add(Input);
				CustomExpression->Inputs.Last().Input.Connect(GetPreferredOutputIndexForProperty(Property, PropertyExpression), PropertyExpression);
			}

			for (const Private::FResolvedNamedOutput& OutputDefinition : NamedOutputs)
			{
				FCustomOutput Output;
				Output.OutputName = FName(*OutputDefinition.Name);
				Output.OutputType = OutputDefinition.OutputType;
				CustomExpression->AdditionalOutputs.Add(Output);
			}

			Private::RebuildDreamShaderCustomOutputs(CustomExpression);

			for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
			{
				int32 SourceOutputIndex = 0;
				if (!Binding.SourceText.Equals(TEXT("return"), ESearchCase::IgnoreCase)
					&& !TryResolveExpressionOutputIndexByName(CustomExpression, Binding.SourceText, SourceOutputIndex))
				{
					OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("%s: Failed to resolve Custom output '%s'."),
						*SourceFilePath,
						*Binding.SourceText);
					return false;
				}

				if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty)
				{
					Private::FResolvedMaterialProperty ResolvedProperty;
					verify(Private::ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty));
					if (ResolvedProperty.bIsSubstrateMaterial)
					{
						OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
							TEXT("%s: Material output '%s' expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes."),
							*SourceFilePath,
							*Binding.MaterialProperty);
						return false;
					}
					if (ResolvedProperty.OutputType == CMOT_MaterialAttributes)
					{
						Material->bUseMaterialAttributes = true;
					}

					FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(ResolvedProperty.Property);
					if (!MaterialInput)
					{
						OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
							TEXT("%s: Failed to find material property '%s' while connecting '%s'."),
							*SourceFilePath,
							*Binding.MaterialProperty,
							*Binding.SourceText);
						return false;
					}

					Private::FCodeValue OutputValue;
					OutputValue.Expression = CustomExpression;
					OutputValue.OutputIndex = SourceOutputIndex;
					const Private::FCodeValue RoutedOutputValue = Private::CreateOutputRerouteValue(
						Material,
						nullptr,
						OutputValue,
						Binding.MaterialProperty,
						static_cast<int32>(ResolvedProperty.Property));
					Private::ConnectCodeValueToInput(*MaterialInput, RoutedOutputValue);
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
						OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *TargetError); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
				}
			}
		}

		if (bTransient)
		{
			MaterialSlowTask.EnterProgressFrame(1.0f);
		}
		else
		{
		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("LayingOutMaterialGraph", "Laying out material graph '{0}'..."),
				FText::FromString(Material->GetName())));
			Private::LayoutGeneratedExpressions(
				Material,
				nullptr,
				&Definition.Layout,
				GeneratedExpressionsByVariable.IsEmpty() ? nullptr : &GeneratedExpressionsByVariable,
				RegionByVariable.IsEmpty() ? nullptr : &RegionByVariable);
		}
		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("CompilingMaterial", "Compiling material '{0}'..."),
				FText::FromString(Material->GetName())));
		UMaterialEditingLibrary::RecompileMaterial(Material);
		Material->PostEditChange();
		return true;
	}

	// The hidden base UMaterial that carries the ThinCustom whole-surface graph (the instance parents
	// to it). Hosting splits on bTransient:
	//
	// - TRANSIENT (the editor's in-memory default): the base lives in the transient package. Objects
	//   whose outermost is GetTransientPackage() are excluded from IsAsset() and every content-browser
	//   / asset-registry enumeration, so the base is naturally hidden with no override needed (the
	//   instance hides itself via its own IsAsset()). Nothing is ever saved.
	//
	// - PERSIST (the cook director / materialize path): the base is created as a hidden SUBOBJECT of the
	//   instance (Outer = the instance, not a package). It therefore serializes into -- and cooks and
	//   loads with -- the instance's own package as a plain export: there is no separate
	//   MB_DreamThinBase_* sibling asset to leak into the Content Browser, and no fragile cross-package
	//   parent import to lose on a future load or at cook. A non-package outer makes IsAsset() false with
	//   no override needed, so the base stays invisible while the instance remains the one browsable asset.
	static bool EnsureThinCustomBaseMaterial(
		const FTextShaderDefinition& Definition,
		const bool bTransient,
		UDreamShaderMaterialInstance* Instance,
		UMaterial*& OutBase,
		FString& OutError)
	{
		if (bTransient)
		{
			// Definition.Name is a slash-delimited logical path; sanitize it into a flat object name.
			// Slashes in an FName break FindObject reuse (they read as subobject-path separators), so an
			// unsanitized name would leak a new transient base on every regeneration.
			const FName BaseName(*FString::Printf(TEXT("MB_DreamThinBase_%s"), *UE::DreamShader::SanitizeIdentifier(Definition.Name))); /* I18N-EXEMPT: deferred codegen or compatibility path */
			OutBase = FindObject<UMaterial>(GetTransientPackage(), *BaseName.ToString());
			if (!OutBase)
			{
				// RF_Standalone is mandatory, not cosmetic: RecompileMaterial runs a full GC pass
				// (BuildTextureStreamingData -> CollectGarbage), and in the editor GARBAGE_COLLECTION_KEEPFLAGS
				// is RF_Standalone alone. Until the instance parents to this base, the base is only reachable
				// through a local pointer, so without RF_Standalone the GC collects it mid-recompile.
				OutBase = NewObject<UMaterial>(GetTransientPackage(), BaseName, RF_Public | RF_Standalone | RF_Transient);
			}
			if (!OutBase)
			{
				OutError = FString::Printf(TEXT("Failed to create ThinCustom base material for '%s'."), *Definition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			return true;
		}

		// Persist: the base is a hidden subobject of the instance -- one asset, one package on disk.
		if (!Instance)
		{
			OutError = FString::Printf(TEXT("Cannot create a persisted ThinCustom base without an instance for '%s'."), *Definition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		const FName BaseName(*FString::Printf(TEXT("MB_DreamThinBase_%s"), *Instance->GetName())); /* I18N-EXEMPT: deferred codegen or compatibility path */

		// Reuse the instance's existing base on regeneration: prefer the subobject found by name, then
		// fall back to the current parent if it is already one of our subobjects (tolerates a name drift).
		// An instance loaded from an older build still parents to a separate MB_ sibling package; that is
		// not a subobject of this instance, so it is deliberately not reused here -- a fresh subobject base
		// is created and the stale sibling package is left orphaned (harmless, safely deletable).
		OutBase = FindObject<UMaterial>(Instance, *BaseName.ToString());
		if (!OutBase)
		{
			if (UMaterial* ExistingParent = Cast<UMaterial>(Instance->Parent);
				ExistingParent && ExistingParent->GetOuter() == Instance)
			{
				OutBase = ExistingParent;
			}
		}
		if (!OutBase)
		{
			// RF_Standalone for the same GC reason as the transient path (RecompileMaterial GCs before the
			// instance parents to the base). No RF_Transient: the base must serialize into the instance's
			// saved package as an export.
			OutBase = NewObject<UMaterial>(Instance, BaseName, RF_Public | RF_Standalone);
		}
		if (!OutBase)
		{
			OutError = FString::Printf(TEXT("Failed to create ThinCustom base material for instance '%s'."), *Instance->GetPathName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}
		return true;
	}

	// THE backend: build the material graph onto a hidden per-material base UMaterial (reusing the
	// Graph construction wholesale) and emit a lightweight UDreamShaderMaterialInstance of it. The
	// instance is a plain thin MIC -- parameters, settings, domains, and scene reads all live on the
	// base as ordinary nodes and properties, enumerated and compiled natively by the engine. What the
	// instance adds is the in-memory hiding (IsAsset) and root shader-map ownership
	// (HasOverridenBaseProperties, since the parent is a UMaterial).
	static bool GenerateThinCustomMaterialAsInstance(
		const FString& SourceFilePath,
		const FString& SourceHash,
		const FTextShaderDefinition& Definition,
		const FString& SourceText,
		const TArray<Private::FResolvedNamedOutput>& NamedOutputs,
		bool bUsesReturn,
		ECustomMaterialOutputType ReturnOutputType,
		bool bReturnIsSubstrateMaterial,
		bool bUsesFrontMaterial,
		FString& OutMessage,
		const bool bForce,
		const bool bTransient)
	{
		UDreamShaderMaterialInstance* Instance = nullptr;
		FString InstanceError;
		if (!Private::CreateOrReuseInstanceMaterial(Definition, Instance, InstanceError, bTransient) || !Instance)
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *InstanceError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (!bForce && Private::IsGeneratedAssetSourceCurrent(Instance, SourceFilePath, SourceHash))
		{
			OutMessage = FString::Printf(TEXT("Skipped %s from %s; source hash is unchanged."), *Instance->GetPathName(), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;
		}

		// After the skip check on purpose: a hash-skip must not create (or ownership-check) a base.
		UMaterial* BaseMaterial = nullptr;
		FString BaseError;
		if (!EnsureThinCustomBaseMaterial(Definition, bTransient, Instance, BaseMaterial, BaseError) || !BaseMaterial)
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *BaseError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		// Build the whole-surface Custom graph onto the base. bTransient is threaded through: the
		// persist path lays out the base's graph (it becomes a real inspectable asset on disk), the
		// transient path skips layout. Scope a self-contained slow-task for the build so its progress
		// frames account against their own budget -- the caller already entered one coarse frame for
		// the whole thin-custom generation, so threading that same task through here would overflow it
		// (SlowTask.cpp "Work overflow" ensure).
		{
		FScopedSlowTask GraphBuildTask(
			11.0f,
			FText::Format(
				LOCTEXT("BuildingMaterialGraph", "Building material graph for '{0}'..."),
				FText::FromString(Definition.Name)));
			if (!PopulateMaterialGraphFromDefinition(
					BaseMaterial, Definition, SourceFilePath, SourceText, NamedOutputs,
					bUsesReturn, ReturnOutputType, bReturnIsSubstrateMaterial, bUsesFrontMaterial,
					bTransient, GraphBuildTask, OutMessage))
			{
				return false;
			}
		}

		// Deferred recache: the single shader recache happens in UpdateStaticPermutation below, after
		// the parent's graph is in place.
		Instance->SetParentEditorOnly(BaseMaterial, /*RecacheShader*/ false);
		Instance->ClearParameterValuesEditorOnly();
		Instance->SourceFilePath = SourceFilePath;
		Instance->SourceHash = SourceHash;

		Instance->UpdateStaticPermutation();
		Instance->PostEditChange();
		Private::ApplySourceMetadata(Instance, SourceFilePath, SourceHash);

		if (bTransient)
		{
			// The editor-only setters and PostEditChange dirtied the in-memory package; clear it so no
			// save-all or exit prompt can silently persist a virtual instance material.
			Instance->GetPackage()->SetDirtyFlag(false);
		}
		else
		{
			// The base is a hidden subobject of the instance, so the pair shares one package: stamping and
			// dirtying the instance is enough, and a single save writes both (the base rides along as an
			// export). The instance is the on-disk ownership anchor -- its source metadata guards
			// regeneration; the base is stamped too only to keep the "generated objects carry source
			// metadata" invariant.
			Private::ApplySourceMetadata(BaseMaterial, SourceFilePath, SourceHash);

			Instance->MarkPackageDirty();

			// Commit the package to disk. The editor save path drops any package UPackage::IsEmptyPackage()
			// reports as empty, and that check counts only objects whose IsAsset() is true. While the
			// package is PKG_NewlyCreated, UDreamShaderMaterialInstance::IsAsset() returns false (its
			// in-memory-hiding rule) and the base is a non-asset subobject, so the package looks empty and
			// is silently skipped. Editor bookkeeping on a memory-only instance (MarkPackageDirty above)
			// re-asserts PKG_NewlyCreated, so clear it as the very LAST step before saving -- now that we
			// are persisting it -- to make the instance report as a real asset. Restore the flag if the
			// save fails so a failed persist does not leave a memory-only instance masquerading as saved.
			UPackage* InstancePackage = Instance->GetOutermost();
			const bool bWasNewlyCreated = InstancePackage->HasAnyPackageFlags(PKG_NewlyCreated);
			InstancePackage->ClearPackageFlags(PKG_NewlyCreated);

			FString SaveError;
			if (!Private::SaveAssetPackages({ Instance }, SaveError))
			{
				if (bWasNewlyCreated)
				{
					InstancePackage->SetPackageFlags(PKG_NewlyCreated);
				}
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SaveError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			// A newly persisted instance was memory-only at creation, so its create-time AssetCreated
			// no-op'd (IsAsset() was false). Now that the save committed it (clearing PKG_NewlyCreated for
			// good), register it so the Content Browser reflects it immediately without waiting for a
			// rescan. A regenerated (already on-disk) instance is already registered -- don't re-add it.
			if (bWasNewlyCreated)
			{
				FAssetRegistryModule::AssetCreated(Instance);
			}
		}

		OutMessage = FString::Printf(TEXT("Generated DreamShader thin-custom material %s from %s."), *Instance->GetPathName(), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
		return true;
	}

	bool FMaterialGenerator::GenerateMaterialFromFile(const FString& InSourceFilePath, FString& OutMessage, const bool bForce, const bool bTransient)
	{
		const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		FScopedSlowTask MaterialSlowTask(
			11.0f,
			FText::Format(
				LOCTEXT("GeneratingDreamShaderMaterialFromSource", "Generating DreamShader material from '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (!IsRunningCommandlet())
		{
			MaterialSlowTask.MakeDialogDelayed(0.25f);
		}

		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ReadingMaterialSource", "Reading material source '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (UE::DreamShader::IsDreamShaderHeaderFile(SourceFilePath) || UE::DreamShader::IsDreamShaderFunctionFile(SourceFilePath))
		{
			OutMessage = FString::Printf(TEXT("DreamShader source '%s' cannot generate a material asset directly."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ParsingMaterialSource", "Parsing material source '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (!FTextShaderParser::Parse(SourceText, Definition, ParseError))
		{
			OutMessage = FormatParseErrorWithSourceLocation(SourceFilePath, SourceText, ParseError);
			return false;
		}

		FString RootFallbackReason;
		Private::ApplyDefaultRootFromSourceFile(SourceFilePath, Definition, &RootFallbackReason);
		if (!RootFallbackReason.IsEmpty())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("%s"), *RootFallbackReason);
		}

		const FString SourceHash = Private::BuildSourceHash(SourceText);

		if (Definition.Name.IsEmpty())
		{
			OutMessage = FString::Printf(TEXT("%s: This file does not define a top-level Shader block."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (Definition.Outputs.IsEmpty())
		{
			OutMessage = FString::Printf(TEXT("%s: Outputs block is required."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		TArray<Private::FResolvedNamedOutput> NamedOutputs;
		bool bUsesReturn = false;
		ECustomMaterialOutputType ReturnOutputType = CMOT_Float1;
		bool bReturnIsSubstrateMaterial = false;
		FString ValidationError;
		if (!Private::ValidateSettings(Definition, ValidationError)
			|| !Private::ValidateOutputs(Definition, NamedOutputs, bUsesReturn, ReturnOutputType, bReturnIsSubstrateMaterial, ValidationError))
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *ValidationError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		bool bUsesFrontMaterial = false;
		bool bUsesMaterialAttributesOutput = false;
		for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
		{
			if (Binding.TargetKind != FTextShaderOutputBinding::ETargetKind::MaterialProperty)
			{
				continue;
			}

			Private::FResolvedMaterialProperty ResolvedProperty;
			if (!Private::ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty))
			{
				continue;
			}

			bUsesFrontMaterial |= ResolvedProperty.bIsSubstrateMaterial;
			bUsesMaterialAttributesOutput |= ResolvedProperty.OutputType == CMOT_MaterialAttributes;
		}
		if (bUsesFrontMaterial && bUsesMaterialAttributesOutput)
		{
			OutMessage = FString::Printf(TEXT("%s: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader."), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		// Both backends consume the imported-functions include, so it is written before the
		// backend split.
		if (!Definition.Functions.IsEmpty())
		{
			FString IncludeWriteError;
			if (!Private::WriteGeneratedInclude(SourceFilePath, Definition, IncludeWriteError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *IncludeWriteError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		Private::EResolvedBackend RequestedBackend = Private::EResolvedBackend::Graph;
		bool bExplicitBackend = false;
		FString BackendError;
		if (!Private::ResolveRequestedBackend(Definition, RequestedBackend, bExplicitBackend, BackendError))
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *BackendError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (RequestedBackend == Private::EResolvedBackend::ThinCustom)
		{
			// Convergence path: build the whole surface onto a hidden base UMaterial and emit a
			// lightweight instance of it. It reuses the Graph construction, so it is as capable as the
			// Graph backend (return-style Outputs included) -- no capability gate or fallback needed.
		MaterialSlowTask.EnterProgressFrame(
			8.0f,
			FText::Format(
				LOCTEXT("GeneratingThinCustomMaterial", "Generating thin-custom material for '{0}'..."),
				FText::FromString(Definition.Name)));
			return GenerateThinCustomMaterialAsInstance(
				SourceFilePath, SourceHash, Definition, SourceText, NamedOutputs,
				bUsesReturn, ReturnOutputType, bReturnIsSubstrateMaterial, bUsesFrontMaterial,
				OutMessage, bForce, bTransient);
		}


		UMaterial* Material = nullptr;
		FString MaterialError;
		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("PreparingMaterialAsset", "Preparing material asset '{0}'..."),
				FText::FromString(Definition.Name)));
		if (!Private::CreateOrReuseMaterial(Definition, Material, MaterialError, bTransient) || !Material)
		{
			OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *MaterialError); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (!bForce && Private::IsGeneratedAssetSourceCurrent(Material, SourceFilePath, SourceHash))
		{
			OutMessage = FString::Printf(TEXT("Skipped %s from %s; source hash is unchanged."), *Material->GetPathName(), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;
		}

		if (!PopulateMaterialGraphFromDefinition(
				Material, Definition, SourceFilePath, SourceText, NamedOutputs,
				bUsesReturn, ReturnOutputType, bReturnIsSubstrateMaterial, bUsesFrontMaterial,
				bTransient, MaterialSlowTask, OutMessage))
		{
			return false;
		}

		if (bTransient)
		{
			MaterialSlowTask.EnterProgressFrame(1.0f);
			// Modify()/PostEditChange dirtied the in-memory package; clear it so no save-all or
			// exit prompt can silently persist an in-memory material and fork the source of truth.
			Material->GetPackage()->SetDirtyFlag(false);
		}
		else
		{
			Material->MarkPackageDirty();
			Private::ApplySourceMetadata(Material, SourceFilePath, SourceHash);

		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("SavingMaterial", "Saving material '{0}'..."),
				FText::FromString(Material->GetName())));
			FString SaveError;
			if (!Private::SaveAssetPackage(Material, SaveError))
			{
				OutMessage = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SaveError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		OutMessage = FString::Printf(TEXT("Generated %s from %s.%s"), *Material->GetPathName(), *SourceFilePath, bTransient ? TEXT(" (virtual)") : TEXT("")); /* I18N-EXEMPT: deferred codegen or compatibility path */
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
