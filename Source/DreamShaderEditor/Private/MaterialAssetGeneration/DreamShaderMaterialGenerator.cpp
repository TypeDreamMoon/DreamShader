#include "DreamShaderMaterialGenerator.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderGraphRollback.h"
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
// UPackage, for the GetPackage()->SetDirtyFlag / HasAnyPackageFlags calls and for the
// GetTransientPackage() outer that FindObject/NewObject deduce against. Unity builds happened to
// pull it in through a neighbouring TU; a non-unity compile of this file -- which is what UBT's
// adaptive non-unity does to whatever you are currently editing -- failed with C2027.
#include "UObject/Package.h"
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
			FDreamShaderError& OutError)
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
			FDreamShaderError BuildError;
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
				return FailWith(OutError, TEXT("DSH8001"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("Input '%s' default expression '%s' does not match declared type '%s'."), *InputDefinition.Name, *InputDefinition.DefaultValueText, *InputDefinition.Type));
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
			FDreamShaderError& OutError)
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
				return FailWith(OutError, TEXT("DSH8002"), FString::Printf(TEXT("Failed to create a MakeMaterialAttributes node for '%s'."), *ValueName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			FDreamShaderError& OutError)
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
					return FailWith(OutError, TEXT("DSH8003"), FString::Printf(TEXT("Property '%s' has a recursive UE builtin dependency."), *Property.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

			FDreamShaderError PropertyExpressionError;
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
			FDreamShaderError& OutError)
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
					return FailWith(OutError, TEXT("DSH8004"), FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					return FailWith(OutError, TEXT("DSH8005"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("Function '%s' parameter '%s' uses unsupported qualifier '%s'. Supported qualifiers are in and out."), *FunctionName, *Parameter, *Qualifier));
				}

				if (TypeToken.IsEmpty() || NameToken.IsEmpty())
				{
					return FailWith(OutError, TEXT("DSH8006"), FString::Printf(TEXT("Function '%s' has an invalid parameter declaration '%s'."), *FunctionName, *Parameter)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH8007"), FString::Printf(TEXT("Function '%s' must declare at least one out parameter."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			return true;
		}

		static bool TransformModernFunctionSyntax(const FString& InSourceText, FString& OutSourceText, FDreamShaderError& OutError)
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
			FDreamShaderError& OutError)
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
				return FailWith(OutError, TEXT("DSH8008"), FString::Printf(TEXT("Output target '%s' could not resolve MaterialExpression class '%s'."), *Binding.TargetText, *Binding.ExpressionClass)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			OutExpression = Cast<UMaterialExpression>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, 1200, InOutPositionY));
			if (!OutExpression)
			{
				return FailWith(OutError, TEXT("DSH8009"), FString::Printf(TEXT("Output target '%s' failed to create '%s'."), *Binding.TargetText, *ExpressionClass->GetName())); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					return FailWith(OutError, TEXT("DSH8010"), FString::Printf(TEXT("Output target '%s': '%s' is not a property on '%s'."), *Binding.TargetText, *Argument.Key, *ExpressionClass->GetName())); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}

				if (Private::IsMaterialExpressionInputProperty(BoundProperty))
				{
					return FailWith(OutError, TEXT("DSH8011"), FString::Printf(TEXT("Output target '%s': inline input property '%s' is not supported yet. Bind through .Pin[index] instead."), *Binding.TargetText, *Argument.Key)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}

				FDreamShaderError LiteralError;
				if (!Private::SetMaterialExpressionLiteralProperty(OutExpression, BoundProperty, Argument.Value, LiteralError))
				{
					return FailWith(OutError, TEXT("DSH8012"), FString::Printf(TEXT("Output target '%s': %s"), *Binding.TargetText, *LiteralError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			FDreamShaderError& OutError)
		{
			if (!SourceExpression || !TargetExpression)
			{
				return FailWith(OutError, TEXT("DSH8013"), TEXT("Invalid output source or target expression."));
			}

			const FString PinKey = BuildOutputTargetCacheKey(Binding) + FString::Printf(TEXT("#%d"), Binding.ExpressionPinIndex); /* I18N-EXEMPT: deferred codegen or compatibility path */
			if (BoundPins.Contains(PinKey))
			{
				return FailWith(OutError, TEXT("DSH8014"), FString::Printf(TEXT("Output target pin '%s' is bound more than once."), *Binding.TargetText)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FExpressionInput* TargetInput = TargetExpression->GetInput(Binding.ExpressionPinIndex);
			if (!TargetInput)
			{
				return FailWith(OutError, TEXT("DSH8015"), FString::Printf(TEXT("Output target '%s' does not have Pin[%d]."), *Binding.TargetText, Binding.ExpressionPinIndex)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

		bool ValidateMaterialLayerFunctionDefinition(const FTextShaderMaterialFunctionDefinition& FunctionDefinition, FDreamShaderError& OutError)
		{
			const TCHAR* BlockKind = GetMaterialFunctionBlockKindText(FunctionDefinition.Kind);
			if (FunctionDefinition.Kind == ETextShaderMaterialFunctionKind::ShaderFunction)
			{
				return true;
			}

			if (FunctionDefinition.Outputs.Num() != 1 || !Private::IsMaterialAttributesType(FunctionDefinition.Outputs[0].Type))
			{
				return FailWith(OutError, TEXT("DSH8016"), FString::Printf(TEXT("%s '%s' must declare exactly one MaterialAttributes output."), BlockKind, *FunctionDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH8017"), FString::Printf(TEXT("ShaderLayer '%s' must declare at most one input, and it must be MaterialAttributes. Use Properties for layer controls."), *FunctionDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (FunctionDefinition.Kind == ETextShaderMaterialFunctionKind::MaterialLayerBlend)
			{
				if (FunctionDefinition.Inputs.Num() != DreamShaderAcceptableBlendMaterialAttributesInputs
					|| MaterialAttributesInputCount != DreamShaderAcceptableBlendMaterialAttributesInputs)
				{
					return FailWith(OutError, TEXT("DSH8018"), FString::Printf(TEXT("ShaderLayerBlend '%s' must declare exactly two inputs, both MaterialAttributes. Use Properties for blend controls."), *FunctionDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

		/**
		 * Dense, tie-free SortPriority values that keep the source's declaration order.
		 *
		 * UMaterialFunction::GetInputsAndOutputs sorts with TArray::Sort, which is introsort and not
		 * stable, so two parameters that share a SortPriority come out in an order decided by the
		 * pre-sort arrangement rather than by anything the author wrote. That order is load-bearing:
		 * UMaterialExpressionMaterialFunctionCall falls back to it whenever the stored input GUIDs do
		 * not match, so a tie lets a rebuild from unchanged source silently rewire every caller. The
		 * symptom surfaces in the calling material, not here -- a float that lands on a StaticBool pin
		 * is reported as "Cannot cast from static bool to float", naming the function but not the pin.
		 *
		 * Ranking (authored priority, declaration index) into 0..N-1 preserves the authored ordering,
		 * makes the source the tiebreak, and leaves no two values equal for the engine to reorder. The
		 * numbers themselves carry no meaning beyond that ordering, so densifying them costs nothing.
		 */
		void AssignDeterministicSortPriorities(
			const TArray<FTextShaderFunctionParameter>& Parameters,
			TArray<int32>& OutSortPriorities)
		{
			OutSortPriorities.SetNumZeroed(Parameters.Num());

			TArray<int32> DeclarationOrder;
			DeclarationOrder.Reserve(Parameters.Num());
			for (int32 Index = 0; Index < Parameters.Num(); ++Index)
			{
				DeclarationOrder.Add(Index);
			}

			// An unauthored priority ranks by declaration index, which is what the previous code used
			// directly; authored ones keep their relative order among themselves and against those.
			auto EffectivePriority = [&Parameters](int32 Index)
			{
				return Parameters[Index].Metadata.bHasSortPriority
					? Parameters[Index].Metadata.SortPriority
					: Index;
			};

			// StableSort, so equal priorities fall back to declaration order rather than to whatever
			// an unstable sort happens to produce -- the whole point of this pass.
			DeclarationOrder.StableSort([&EffectivePriority](int32 Left, int32 Right)
			{
				return EffectivePriority(Left) < EffectivePriority(Right);
			});

			for (int32 Rank = 0; Rank < DeclarationOrder.Num(); ++Rank)
			{
				OutSortPriorities[DeclarationOrder[Rank]] = Rank;
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
			FDreamShaderError& OutError)
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
					return WrapError(OutError, FString::Printf(TEXT("Output '%s': %s"), *OutputDeclaration.Name, *OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			FDreamShaderError& OutError)
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
				return FailWith(OutError, TEXT("DSH8019"), FString::Printf(TEXT("%s '%s' must declare at least one output."), BlockKind, *FunctionDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (!ValidateMaterialLayerFunctionDefinition(FunctionDefinition, OutError))
			{
				return false;
			}

			if (FunctionDefinition.Code.IsEmpty())
			{
				return FailWith(OutError, TEXT("DSH8020"), FString::Printf(TEXT("%s '%s' must provide a Graph block."), BlockKind, *FunctionDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			UMaterialFunction* MaterialFunction = nullptr;
			if (!Private::CreateOrReuseMaterialFunction(FunctionDefinition, MaterialFunction, OutError, bTransient) || !MaterialFunction)
			{
				return false;
			}

			// Storage decides from here, not the request. See IsGeneratedAssetPersisted.
			const bool bEffectiveTransient = bTransient && !Private::IsGeneratedAssetPersisted(MaterialFunction);

			FDreamShaderError DeferMessage;
			if (Private::ShouldDeferPersistedAssetToWriteOwner(MaterialFunction, !bEffectiveTransient, DeferMessage))
			{
				OutGeneratedAssetPath = MaterialFunction->GetPathName();
				UE_LOG(LogDreamShader, Display, TEXT("%s"), *DeferMessage);
				return true;
			}

			const EMaterialFunctionUsage ExpectedUsage = GetUnrealMaterialFunctionUsage(FunctionDefinition.Kind);
			if (!bForce
				&& Private::IsGeneratedAssetSourceCurrent(MaterialFunction, SourceFilePath, SourceHash)
				&& MaterialFunction->GetMaterialFunctionUsage() == ExpectedUsage)
			{
				OutGeneratedAssetPath = MaterialFunction->GetPathName();
				return true;
			}

			// Before Modify()/SetMaterialFunctionUsage, which are already edits to the asset: a function
			// that fails this gate must come out of the compile completely untouched, because its call
			// sites read its pins from the live asset and a half-updated function breaks all of them.
			FDreamShaderError FunctionEditorOpenError;
			if (!Private::CheckGeneratedAssetNotOpenInEditor(MaterialFunction, FunctionEditorOpenError))
			{
				OutError = FunctionEditorOpenError;
				return false;
			}

			FDreamShaderError FunctionDivergenceError;
			if (!Private::CheckGeneratedAssetNotDiverged(MaterialFunction, FunctionDivergenceError))
			{
				OutError = FunctionDivergenceError;
				return false;
			}

			MaterialFunction->Modify();

			// Ahead of the snapshot, and it must stay there: this destroys the generated comment
			// boxes, so a snapshot taken first would capture pointers to comments already garbage.
			Private::ClearDreamShaderGeneratedComments(nullptr, MaterialFunction);

			// Also ahead of the snapshot, but for the opposite reason: this reads the pin GUIDs off the
			// live FunctionInput/FunctionOutput nodes, and the rollback detaches those nodes. Caching
			// after arming it would find an empty graph and lose every call site's wiring.
			TMap<FName, FGuid> ExistingInputIdsByName;
			TMap<FName, FGuid> ExistingOutputIdsByName;
			CacheMaterialFunctionInterfaceIds(MaterialFunction, ExistingInputIdsByName, ExistingOutputIdsByName);

		FunctionSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ClearingOldFunctionGraph", "Clearing old function graph '{0}'..."),
				FText::FromString(MaterialFunction->GetName())));
			// Everything from here until Commit() is reversible -- including the usage below, which is
			// why the snapshot is taken before it rather than where the old teardown sat. A function
			// that fails to rebuild is the dangerous case: its call sites read their pins from the live
			// asset, so an emptied one used to take every material that called it down with it.
			Private::FDreamShaderGraphRollback Rollback(MaterialFunction);
			MaterialFunction->SetMaterialFunctionUsage(ExpectedUsage);

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
					return FailWith(OutError, TEXT("DSH8021"), FString::Printf(TEXT("%s '%s': ExposeToLibrary must be true or false."), BlockKind, *FunctionDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					return FailWith(OutError, TEXT("DSH8022"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s '%s' property '%s' conflicts with another property or input name."), BlockKind, *FunctionDefinition.Name, *Property.Name));
				}

				SeenPropertyNames.Add(Property.Name);
			}

			TMap<FString, UMaterialExpressionFunctionInput*> GeneratedInputExpressions;
			TArray<int32> InputSortPriorities;
			AssignDeterministicSortPriorities(FunctionDefinition.Inputs, InputSortPriorities);
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
						return FailWith(OutError, TEXT("DSH8023"), FString::Printf(TEXT("%s '%s' input '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
					}
					return FailWith(OutError, TEXT("DSH8024"), FString::Printf(TEXT("%s '%s' input '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name, *InputDefinition.Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
				if (bIsTextureObject)
				{
					verify(Private::TryResolveCodeDeclaredType(InputDefinition.Type, ComponentCount, bIsTextureObject, TextureType));
				}

				auto* InputExpression = Cast<UMaterialExpressionFunctionInput>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionFunctionInput::StaticClass(), -800, InputPositionY));
				if (!InputExpression)
				{
					return FailWith(OutError, TEXT("DSH8025"), FString::Printf(TEXT("%s '%s' failed to create input '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				InputExpression->SortPriority = InputSortPriorities[InputIndex];
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
					return FailWith(OutError, TEXT("DSH8026"), FString::Printf(TEXT("%s '%s' failed to resolve generated input '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}

				FDreamShaderError PreviewError;
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
					return FailWith(OutError, TEXT("DSH8027"), FString::Printf(TEXT("%s '%s' input '%s': %s"), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name, *PreviewError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
			}

			int32 MaterialAttributesSeedPositionY = 260;
			for (const FTextShaderFunctionParameter& OutputDefinition : FunctionDefinition.Outputs)
			{
				if (Private::IsMaterialAttributesType(OutputDefinition.Type))
				{
					FDreamShaderError SeedError;
					if (!SeedMaterialAttributesGraphValue(
						nullptr,
						MaterialFunction,
						OutputDefinition.Name,
						GeneratedValues,
						MaterialAttributesSeedPositionY,
						SeedError))
					{
						return FailWith(OutError, TEXT("DSH8028"), FString::Printf(TEXT("%s '%s' output '%s': %s"), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *SeedError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				FDreamShaderError CodeParseError;
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
				FDreamShaderError CodeBuildError;
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
					return FailWith(OutError, TEXT("DSH8029"), FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *PrimaryOutput.Name, *PrimaryOutput.Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}

				auto* CustomExpression = Cast<UMaterialExpressionCustom>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionCustom::StaticClass(), 120, 0));
				if (!CustomExpression)
				{
					return FailWith(OutError, TEXT("DSH8030"), FString::Printf(TEXT("%s '%s' failed to create the function Custom node."), BlockKind, *FunctionDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				TArray<FString> EmbeddedIncludePaths;
				if (!Private::PrepareCustomNodeCode(
					RootDefinition,
					FunctionDefinition.HLSL,
					TArray<FString>(),
					FunctionDefinition.Name,
					PreparedCustomCode,
					bUsesGeneratedInclude,
					EmbeddedIncludePaths,
					OutError))
				{
					return WrapError(OutError, FString::Printf(TEXT("%s '%s': %s"), BlockKind, *FunctionDefinition.Name, *OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
				CustomExpression->Code = Private::EnsureTopLevelReturn(PreparedCustomCode);

				for (const FString& IncludePath : EmbeddedIncludePaths)
				{
					CustomExpression->IncludeFilePaths.AddUnique(IncludePath);
				}
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
						return FailWith(OutError, TEXT("DSH8031"), FString::Printf(TEXT("%s '%s' failed to resolve generated input '%s'."), BlockKind, *FunctionDefinition.Name, *InputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

					FDreamShaderError PropertyExpressionError;
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
						return FailWith(OutError, TEXT("DSH8032"), FString::Printf(TEXT("%s '%s' property '%s': %s"), BlockKind, *FunctionDefinition.Name, *Property.Name, *PropertyExpressionError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
						return FailWith(OutError, TEXT("DSH8033"), FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			TArray<int32> OutputSortPriorities;
			AssignDeterministicSortPriorities(FunctionDefinition.Outputs, OutputSortPriorities);
			for (int32 OutputIndex = 0; OutputIndex < FunctionDefinition.Outputs.Num(); ++OutputIndex)
			{
				const FTextShaderFunctionParameter& OutputDefinition = FunctionDefinition.Outputs[OutputIndex];
				const Private::FCodeValue* OutputValue = GeneratedValues.Find(OutputDefinition.Name);
				if (!OutputValue || !OutputValue->Expression)
				{
					return FailWith(OutError, TEXT("DSH8034"), FString::Printf(TEXT("%s '%s' output '%s' was never assigned an expression."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
						return FailWith(OutError, TEXT("DSH8035"), FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
					}
					return FailWith(OutError, TEXT("DSH8036"), FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					return FailWith(OutError, TEXT("DSH8037"), FString::Printf(TEXT("%s '%s' output '%s' does not match its declared type '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name, *OutputDefinition.Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}

				auto* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(
					UMaterialEditingLibrary::CreateMaterialExpressionInFunction(MaterialFunction, UMaterialExpressionFunctionOutput::StaticClass(), 900, OutputPositionY));
				if (!OutputExpression)
				{
					return FailWith(OutError, TEXT("DSH8038"), FString::Printf(TEXT("%s '%s' failed to create output '%s'."), BlockKind, *FunctionDefinition.Name, *OutputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}

				OutputExpression->OutputName = FName(*OutputDefinition.Name);
				OutputExpression->Description = OutputDefinition.Metadata.Description;
				OutputExpression->SortPriority = OutputSortPriorities[OutputIndex];
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

			const bool bLayoutThisFunction = !bEffectiveTransient || GetDefault<UDreamShaderSettings>()->bLayoutInMemoryGraphs;
			if (bEffectiveTransient)
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
			}

			if (bLayoutThisFunction)
			{
				Private::LayoutGeneratedExpressions(
					nullptr,
					MaterialFunction,
					&FunctionDefinition.Layout,
					GeneratedExpressionsByVariable.IsEmpty() ? nullptr : &GeneratedExpressionsByVariable,
					RegionByVariable.IsEmpty() ? nullptr : &RegionByVariable,
					bEffectiveTransient);
			}

			// The graph is complete and nothing below can fail. Before UpdateMaterialFunction, because
			// that reaches ForceRecompileForRendering, which is one of the two places that REBUILD
			// DependentFunctionExpressionCandidates -- and Commit() resets it. The other order would
			// throw away the list the recompile had just populated.
			Rollback.Commit();

			FunctionSlowTask.EnterProgressFrame(
				1.0f,
				FText::Format(
					LOCTEXT("UpdatingFunction", "Updating '{0}'..."),
					FText::FromString(FunctionDefinition.Name)));
			UMaterialEditingLibrary::UpdateMaterialFunction(MaterialFunction, nullptr);
			MaterialFunction->PostEditChange();

			// Ahead of the transient dirty-flag reset below: stamping metadata dirties the package. Path
			// and hash both, in memory as on disk -- see the material path for what the hash buys here.
			if (bEffectiveTransient)
			{
				Private::ApplySourceMetadata(MaterialFunction, SourceFilePath, SourceHash);
			}
			Private::ApplyOutputDigestMetadata(MaterialFunction);

			if (bEffectiveTransient)
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
				FDreamShaderError SaveError;
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
		FDreamShaderError Error;
		const bool bResult = GenerateAssetsFromFile(InSourceFilePath, Error, bForce, bTransient);
		OutMessage = Error.Message;
		return bResult;
	}

	namespace
	{
		FOnDreamShaderSourceGenerated GOnDreamShaderSourceGenerated;

		// Fires OnDreamShaderSourceGenerated for the outermost generation only: GenerateAssetsFromFile
		// calls GenerateMaterialFromFile for the material half, and one source is one notice.
		struct FScopedGenerationNotice
		{
			static int32 Depth;
			const FString SourceFilePath;
			bool bSucceeded = false;

			explicit FScopedGenerationNotice(const FString& InSourceFilePath)
				: SourceFilePath(UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath))
			{
				++Depth;
			}

			bool Report(bool bResult)
			{
				bSucceeded = bResult;
				return bResult;
			}

			~FScopedGenerationNotice()
			{
				if (--Depth == 0)
				{
					GOnDreamShaderSourceGenerated.Broadcast(SourceFilePath, bSucceeded);
				}
			}
		};
		int32 FScopedGenerationNotice::Depth = 0;
	}

	FOnDreamShaderSourceGenerated& OnDreamShaderSourceGenerated()
	{
		return GOnDreamShaderSourceGenerated;
	}

	bool FMaterialGenerator::GenerateAssetsFromFile(const FString& InSourceFilePath, FDreamShaderError& OutMessage, const bool bForce, const bool bTransient)
	{
		FScopedGenerationNotice Notice(InSourceFilePath);
		return Notice.Report(GenerateAssetsFromFileInternal(InSourceFilePath, OutMessage, bForce, bTransient));
	}

	bool FMaterialGenerator::GenerateMaterialFromFile(const FString& InSourceFilePath, FDreamShaderError& OutMessage, const bool bForce, const bool bTransient)
	{
		FScopedGenerationNotice Notice(InSourceFilePath);
		return Notice.Report(GenerateMaterialFromFileInternal(InSourceFilePath, OutMessage, bForce, bTransient));
	}

	bool FMaterialGenerator::GenerateAssetsFromFileInternal(const FString& InSourceFilePath, FDreamShaderError& OutMessage, const bool bForce, const bool bTransient)
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
			return FailWith(OutMessage, TEXT("DSH8039"), FString::Printf(TEXT("DreamShader header '%s' does not generate assets directly. Recompile dependent .dsm or .dsf files instead."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		FString SourceText;
		// The defines this source (and everything it imports) actually read, for the build key below.
		// It has to travel with the text: SourceText arrives with its false branches already cut, so
		// nothing in it records which define set did the cutting.
		UE::DreamShader::FDreamShaderDefineValueMap TouchedDefines;
		// Whether any file in the inlined set carried a preprocessor directive. Read by the Adopt gate,
		// which cannot write a conditional source back from an asset that only holds the cut result.
		bool bSourceHadPreprocessorDirectives = false;
		FDreamShaderError PreparedSourceError;
		if (!LoadPreparedDreamShaderSource(
			SourceFilePath,
			SourceText,
			TouchedDefines,
			bSourceHadPreprocessorDirectives,
			PreparedSourceError))
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

		const FString SourceHash = Private::BuildSourceHash(SourceText, TouchedDefines);

		if (UE::DreamShader::IsDreamShaderFunctionFile(SourceFilePath) && !Definition.Name.IsEmpty())
		{
			return FailWith(OutMessage, TEXT("DSH8040"), FString::Printf(TEXT("%s: .dsf files cannot define top-level Shader blocks."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		bool bGeneratedHelperInclude = false;
		SourceSlowTask.EnterProgressFrame(1.0f, LOCTEXT("PreparingDreamShaderGeneratedAssets", "Preparing DreamShader generated assets..."));
		if (!Definition.Functions.IsEmpty())
		{
			FDreamShaderError IncludeWriteError;
			if (!Private::WriteGeneratedInclude(SourceFilePath, Definition, IncludeWriteError))
			{
				return FailWith(OutMessage, TEXT("DSH8041"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *IncludeWriteError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			FDreamShaderError FunctionError;
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
			FDreamShaderError MaterialMessage;
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

			return FailWith(OutMessage, TEXT("DSH8042"), FString::Printf(TEXT("DreamShader file '%s' did not contain any material, ShaderFunction, ShaderLayer, or ShaderLayerBlend assets to generate."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		FString CompletionMessage = FString::Join(GeneratedAssetMessages, TEXT("\n"));
		if (!Definition.Warnings.IsEmpty())
		{
			CompletionMessage += TEXT("\nWarnings:\n");
			CompletionMessage += FString::Join(Definition.Warnings, TEXT("\n"));
		}
		OutMessage = CompletionMessage;
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
		static bool ResolveRequestedBackend(const FTextShaderDefinition& Definition, EResolvedBackend& OutBackend, bool& bOutExplicit, FDreamShaderError& OutError)
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

			return FailWith(OutError, TEXT("DSH8043"), FString::Printf(TEXT("Unsupported Backend '%s'. Supported values: Graph, Instance, ThinCustom."), *Value)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				FDreamShaderError SaveError;
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
		FDreamShaderError& OutMessage)
	{
		Material->Modify();
		MaterialSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Clearing old material graph '%s'..."), *Material->GetName()))); /* I18N-EXEMPT: deferred codegen or compatibility path */
		// A live probe preview shares this material's expression collection (the Material Editor's
		// own "Start Previewing Node" arrangement); it must drop that share before the nodes below
		// are marked garbage.
		Private::FDreamShaderGraphDebugRegistry::Get().NotifyGraphMaterialAboutToReset(Material);
		// Ahead of the snapshot, and it must stay there: this destroys the generated comment boxes, so
		// a snapshot taken first would capture pointers to comments that are already garbage.
		Private::ClearDreamShaderGeneratedComments(Material, nullptr);
		// Everything from here until Commit() is reversible. Every `return false` below rolls the
		// material back to what it held on the line above, instead of leaving it emptied.
		Private::FDreamShaderGraphRollback Rollback(Material);
		Private::ResetMaterialToDefaults(Material);

		FDreamShaderError SettingsError;
		if (!Private::ApplySettings(Material, Definition, SettingsError))
		{
			return FailWith(OutMessage, TEXT("DSH8044"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SettingsError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}
		if (bUsesFrontMaterial)
		{
			FString ShadingModelValue;
			if (Definition.TryGetSetting(TEXT("ShadingModel"), ShadingModelValue)
				&& !ShadingModelValue.TrimStartAndEnd().Equals(TEXT("Substrate"), ESearchCase::IgnoreCase)
				&& !ShadingModelValue.TrimStartAndEnd().Equals(TEXT("Strata"), ESearchCase::IgnoreCase))
			{
				return FailWith(OutMessage, TEXT("DSH8045"), FString::Printf(TEXT("%s: Base.FrontMaterial requires ShadingModel=\"Substrate\" or no explicit ShadingModel setting."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			Material->SetShadingModel(MSM_Strata);
		}

		TMap<FString, UMaterialExpression*> GeneratedOutputTargetExpressions;
		TSet<FString> BoundOutputTargetPins;
		TMap<FString, Private::FCodeValue> GeneratedCodeValues;
		TMap<FString, UMaterialExpression*> GeneratedExpressionsByVariable;
		TMap<FString, FString> RegionByVariable;
		TArray<Private::FDreamShaderGraphProbe> GraphProbes;
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
				return FailWith(OutMessage, TEXT("DSH8046"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Property '%s' is declared more than once. Property names must be unique."), *SourceFilePath, *Property.Name));
			}

			SeenPropertyNames.Add(Property.Name);
		}

		int32 MaterialAttributesSeedPositionY = OutputTargetPositionY;
		for (const FTextShaderVariableDeclaration& OutputDeclaration : Definition.OutputDeclarations)
		{
			if (!OutputDeclaration.bHasDefaultValue && Private::IsMaterialAttributesType(OutputDeclaration.Type))
			{
				FDreamShaderError SeedError;
				if (!SeedMaterialAttributesGraphValue(
					Material,
					nullptr,
					OutputDeclaration.Name,
					GeneratedCodeValues,
					MaterialAttributesSeedPositionY,
					SeedError))
				{
					return FailWith(OutMessage, TEXT("DSH8047"), FString::Printf(TEXT("%s: Output '%s': %s"), *SourceFilePath, *OutputDeclaration.Name, *SeedError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutMessage, TEXT("DSH8048"), FString::Printf(TEXT("%s: Graph blocks do not support binding Outputs to the reserved name 'return'."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			TArray<Private::FCodeStatement> CodeStatements;
			FDreamShaderError CodeParseError;
		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ParsingMaterialGraphBlock", "Parsing Graph block for '{0}'..."),
				FText::FromString(Definition.Name)));
			if (!AppendInitializedOutputStatements(Definition.OutputDeclarations, CodeStatements, CodeParseError))
			{
				return FailWith(OutMessage, TEXT("DSH8049"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *CodeParseError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			FDreamShaderError CodeBuildError;
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
			GraphProbes = CodeGraphBuilder.TakeProbes();

			// A material's outputs are written from two places -- the Outputs block's declarative
			// bindings and the Graph block's `Base.<Attribute> = ...` statements -- and both end up
			// here as (property, value). Everything past this point treats them the same; the only
			// thing that cares which is which is the duplicate check, because one property driven from
			// both places has no defensible winner (the block is order-free, the statement is not).
			const TArray<Private::FMaterialOutputSinkWrite>& MaterialOutputSinkWrites =
				CodeGraphBuilder.GetMaterialOutputSinkWrites();
			for (const Private::FMaterialOutputSinkWrite& SinkWrite : MaterialOutputSinkWrites)
			{
				for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
				{
					Private::FResolvedMaterialProperty BoundProperty;
					if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty
						&& Private::ResolveMaterialProperty(Binding.MaterialProperty, BoundProperty)
						&& BoundProperty.Property == SinkWrite.Property)
					{
						return FailWith(OutMessage, TEXT("DSH8050"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Material output '%s' is written from the Graph block and bound in the Outputs block. Keep one of them."), *SourceFilePath, *SinkWrite.MemberName));
					}
				}
			}

			// Shared by both, so a value that would not fit its property is refused the same way
			// wherever it was written -- and refused here rather than surviving into HLSL translation,
			// where the same mistake reads as an error about generated code nobody wrote.
			const auto ConnectMaterialPropertyOutput =
				[&Material, &SourceFilePath, &OutMessage](
					const FString& PropertyName,
					const Private::FResolvedMaterialProperty& ResolvedProperty,
					const Private::FCodeValue& OutputValue) -> bool
			{
				if (ResolvedProperty.bIsSubstrateMaterial)
				{
					if (!OutputValue.bIsSubstrateMaterial)
					{
						return FailWith(OutMessage, TEXT("DSH8051"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Material output '%s' expects a Substrate value."), *SourceFilePath, *PropertyName));
					}
				}
				else if (ResolvedProperty.OutputType == CMOT_MaterialAttributes)
				{
					if (!OutputValue.bIsMaterialAttributes)
					{
						return FailWith(OutMessage, TEXT("DSH8052"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Material output '%s' expects a MaterialAttributes value."), *SourceFilePath, *PropertyName));
					}
					Material->bUseMaterialAttributes = true;
				}
				else if (OutputValue.bIsSubstrateMaterial)
				{
					return FailWith(OutMessage, TEXT("DSH8053"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Material output '%s' expects a numeric value, but got Substrate."), *SourceFilePath, *PropertyName));
				}
				else if (OutputValue.bIsMaterialAttributes || OutputValue.bIsTextureObject)
				{
					OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("%s: Material output '%s' expects a numeric value, but got %s."),
						*SourceFilePath,
						*PropertyName,
						OutputValue.bIsTextureObject ? TEXT("a texture object") : TEXT("MaterialAttributes")); /* I18N-EXEMPT */
					return false;
				}
				else
				{
					// Checks the value that was actually built, not a type someone declared for it, so
					// it holds for an Outputs binding and a Graph write alike -- neither has to have
					// been declared. Only an authoritative count is judged: an inferred one can be a
					// placeholder that the translator widens correctly on its own.
					int32 ExpectedComponentCount = 0;
					if (OutputValue.bHasAuthoritativeComponentCount
						&& Private::TryGetComponentCountForOutputType(ResolvedProperty.OutputType, ExpectedComponentCount)
						&& ExpectedComponentCount > 0
						&& OutputValue.ComponentCount > ExpectedComponentCount)
					{
						return FailWith(OutMessage, TEXT("DSH8054"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Material output '%s' expects %d component(s), but the value has %d."), *SourceFilePath, *PropertyName, ExpectedComponentCount, OutputValue.ComponentCount));
					}
				}

				FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(ResolvedProperty.Property);
				if (!MaterialInput)
				{
					return FailWith(OutMessage, TEXT("DSH8055"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Failed to find material property '%s' while connecting a Graph output."), *SourceFilePath, *PropertyName));
				}

				const Private::FCodeValue RoutedOutputValue = Private::CreateOutputRerouteValue(
					Material,
					nullptr,
					OutputValue,
					PropertyName,
					static_cast<int32>(ResolvedProperty.Property));
				Private::ConnectCodeValueToInput(*MaterialInput, RoutedOutputValue);
				return true;
			};

		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("ConnectingMaterialOutputs", "Connecting material outputs for '{0}'..."),
				FText::FromString(Definition.Name)));
			for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
			{
				Private::FCodeValue OutputValue;
				FDreamShaderError OutputExpressionError;
				if (!CodeGraphBuilder.EvaluateOutputExpression(Binding.SourceText, OutputValue, OutputExpressionError)
					|| !OutputValue.Expression)
				{
					return FailWith(OutMessage, TEXT("DSH8056"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: %s"), *SourceFilePath, *OutputExpressionError));
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
						return FailWith(OutMessage, TEXT("DSH8057"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Graph output '%s' does not match its declared type."), *SourceFilePath, *Binding.SourceText));
					}
				}

				if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty)
				{
					Private::FResolvedMaterialProperty ResolvedProperty;
					verify(Private::ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty));
					if (!ConnectMaterialPropertyOutput(Binding.MaterialProperty, ResolvedProperty, OutputValue))
					{
						return false;
					}
				}
				else
				{
					UMaterialExpression* TargetExpression = nullptr;
					FDreamShaderError TargetError;
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
						return FailWith(OutMessage, TEXT("DSH8058"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *TargetError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
					}
				}
			}

			// The Graph block's own writes. Their values were built while the statement ran, so unlike
			// a binding there is nothing left to evaluate -- only to connect.
			for (const Private::FMaterialOutputSinkWrite& SinkWrite : MaterialOutputSinkWrites)
			{
				Private::FResolvedMaterialProperty ResolvedProperty;
				verify(Private::ResolveMaterialProperty(SinkWrite.MemberName, ResolvedProperty));
				if (!SinkWrite.Value.Expression)
				{
					return FailWith(OutMessage, TEXT("DSH8059"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Material output '%s' was assigned a value that produced no expression."), *SourceFilePath, *SinkWrite.MemberName));
				}

				if (!ConnectMaterialPropertyOutput(SinkWrite.MemberName, ResolvedProperty, SinkWrite.Value))
				{
					return false;
				}
			}

			if (Definition.Outputs.IsEmpty() && MaterialOutputSinkWrites.IsEmpty())
			{
				return FailWith(OutMessage, TEXT("DSH8060"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: This material drives no outputs. Its Graph block computes values but never assigns one to 'Base.<Attribute>', and there is no Outputs block."), *SourceFilePath));
			}
		}
		else
		{
			if (bReturnIsSubstrateMaterial)
			{
				return FailWith(OutMessage, TEXT("DSH8061"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Base.FrontMaterial expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes."), *SourceFilePath));
			}
			for (const Private::FResolvedNamedOutput& OutputDefinition : NamedOutputs)
			{
				if (OutputDefinition.bIsSubstrateMaterial)
				{
					return FailWith(OutMessage, TEXT("DSH8062"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Output '%s' is declared as Substrate and cannot be generated by a material Custom node. Use a Graph block and Substrate.* nodes."), *SourceFilePath, *OutputDefinition.Name));
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
				return FailWith(OutMessage, TEXT("DSH8063"), FString::Printf(TEXT("%s: Failed to create the material Custom node."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			TArray<FString> EmbeddedIncludePaths;
			if (!Private::PrepareCustomNodeCode(
				Definition,
				Definition.HLSL,
				TArray<FString>(),
				Definition.Name,
				PreparedCustomCode,
				bUsesGeneratedInclude,
				EmbeddedIncludePaths,
				OutMessage))
			{
				return WrapError(OutMessage, FString::Printf(TEXT("%s: %s"), *SourceFilePath, *OutMessage)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			CustomExpression->Code = Private::EnsureTopLevelReturn(PreparedCustomCode);

			for (const FString& IncludePath : EmbeddedIncludePaths)
			{
				CustomExpression->IncludeFilePaths.AddUnique(IncludePath);
			}
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

				FDreamShaderError PropertyExpressionError;
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
					return FailWith(OutMessage, TEXT("DSH8064"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *PropertyExpressionError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					return FailWith(OutMessage, TEXT("DSH8065"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Failed to resolve Custom output '%s'."), *SourceFilePath, *Binding.SourceText));
				}

				if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty)
				{
					Private::FResolvedMaterialProperty ResolvedProperty;
					verify(Private::ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty));
					if (ResolvedProperty.bIsSubstrateMaterial)
					{
						return FailWith(OutMessage, TEXT("DSH8066"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Material output '%s' expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes."), *SourceFilePath, *Binding.MaterialProperty));
					}
					if (ResolvedProperty.OutputType == CMOT_MaterialAttributes)
					{
						Material->bUseMaterialAttributes = true;
					}

					FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(ResolvedProperty.Property);
					if (!MaterialInput)
					{
						return FailWith(OutMessage, TEXT("DSH8067"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: Failed to find material property '%s' while connecting '%s'."), *SourceFilePath, *Binding.MaterialProperty, *Binding.SourceText));
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
					FDreamShaderError TargetError;
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
						return FailWith(OutMessage, TEXT("DSH8068"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *TargetError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
					}
				}
			}
		}

		const bool bLayoutThisMaterial = !bTransient || GetDefault<UDreamShaderSettings>()->bLayoutInMemoryGraphs;
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
		}

		if (bLayoutThisMaterial)
		{
			Private::LayoutGeneratedExpressions(
				Material,
				nullptr,
				&Definition.Layout,
				GeneratedExpressionsByVariable.IsEmpty() ? nullptr : &GeneratedExpressionsByVariable,
				RegionByVariable.IsEmpty() ? nullptr : &RegionByVariable,
				bTransient);
		}

		// The graph is complete and nothing below can fail, so the old one is finally expendable.
		// Before the recompile, not after: the recompile is what publishes the new graph to the
		// renderer, and holding a detached copy of the old one across it would keep every node of it
		// alive through the compile for no reason.
		Rollback.Commit();

		// The finished graph is now addressable by (line, name). Publish before the recompile so a
		// probe preview that re-wires on publish gets its own compile queued alongside this one.
		Private::FDreamShaderGraphDebugRegistry::Get().Publish(SourceFilePath, Material, MoveTemp(GraphProbes));

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
		FDreamShaderError& OutError)
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
				return FailWith(OutError, TEXT("DSH8069"), FString::Printf(TEXT("Failed to create ThinCustom base material for '%s'."), *Definition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			return true;
		}

		// Persist: the base is a hidden subobject of the instance -- one asset, one package on disk.
		if (!Instance)
		{
			return FailWith(OutError, TEXT("DSH8070"), FString::Printf(TEXT("Cannot create a persisted ThinCustom base without an instance for '%s'."), *Definition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			return FailWith(OutError, TEXT("DSH8071"), FString::Printf(TEXT("Failed to create ThinCustom base material for instance '%s'."), *Instance->GetPathName())); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
		FDreamShaderError& OutMessage,
		const bool bForce,
		const bool bTransient)
	{
		UDreamShaderMaterialInstance* Instance = nullptr;
		FDreamShaderError InstanceError;
		if (!Private::CreateOrReuseInstanceMaterial(Definition, Instance, InstanceError, bTransient) || !Instance)
		{
			return FailWith(OutMessage, TEXT("DSH8072"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *InstanceError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		// Storage decides from here, not the request (see IsGeneratedAssetPersisted). Asking the
		// instance answers for the pair: the base is a subobject of it, so they share one package and
		// one file.
		const bool bEffectiveTransient = bTransient && !Private::IsGeneratedAssetPersisted(Instance);

		if (Private::ShouldDeferPersistedAssetToWriteOwner(Instance, !bEffectiveTransient, OutMessage))
		{
			return true;
		}

		if (!bForce && Private::IsGeneratedAssetSourceCurrent(Instance, SourceFilePath, SourceHash))
		{
			OutMessage = FString::Printf(TEXT("Skipped %s from %s; source hash is unchanged."), *Instance->GetPathName(), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;
		}

		// The ThinCustom pair has two ways to lose work -- ClearParameterValuesEditorOnly wipes every
		// override on the instance, and the base's graph is torn down below -- and the instance's
		// digest covers both, so one gate here guards the pair. A hand-authored instance that never
		// carried a stamp is a different case, and CreateOrReuseInstanceMaterial's ownership guard
		// (added alongside this) is what turns it away.
		FDreamShaderError EditorOpenError;
		if (!Private::CheckGeneratedAssetNotOpenInEditor(Instance, EditorOpenError))
		{
			return FailWith(OutMessage, TEXT("DSH8073"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *EditorOpenError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		FDreamShaderError DivergenceError;
		if (!Private::CheckGeneratedAssetNotDiverged(Instance, DivergenceError))
		{
			return FailWith(OutMessage, TEXT("DSH8074"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *DivergenceError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		// After the skip check on purpose: a hash-skip must not create (or ownership-check) a base.
		UMaterial* BaseMaterial = nullptr;
		FDreamShaderError BaseError;
		if (!EnsureThinCustomBaseMaterial(Definition, bEffectiveTransient, Instance, BaseMaterial, BaseError) || !BaseMaterial)
		{
			return FailWith(OutMessage, TEXT("DSH8075"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *BaseError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
					bEffectiveTransient, GraphBuildTask, OutMessage))
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
		// After UpdateStaticPermutation and PostEditChange: those settle the instance's static
		// parameter set, which the digest reads, so stamping ahead of them would fingerprint a state
		// the asset is not left in.
		Private::ApplyOutputDigestMetadata(Instance);

		if (bEffectiveTransient)
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

			FDreamShaderError SaveError;
			if (!Private::SaveAssetPackages({ Instance }, SaveError))
			{
				if (bWasNewlyCreated)
				{
					InstancePackage->SetPackageFlags(PKG_NewlyCreated);
				}
				return FailWith(OutMessage, TEXT("DSH8076"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SaveError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
		FDreamShaderError Error;
		const bool bResult = GenerateMaterialFromFile(InSourceFilePath, Error, bForce, bTransient);
		OutMessage = Error.Message;
		return bResult;
	}

	bool FMaterialGenerator::GenerateMaterialFromFileInternal(const FString& InSourceFilePath, FDreamShaderError& OutMessage, const bool bForce, const bool bTransient)
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
			return FailWith(OutMessage, TEXT("DSH8077"), FString::Printf(TEXT("DreamShader source '%s' cannot generate a material asset directly."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		FString SourceText;
		// The defines this source (and everything it imports) actually read, for the build key below.
		// It has to travel with the text: SourceText arrives with its false branches already cut, so
		// nothing in it records which define set did the cutting.
		UE::DreamShader::FDreamShaderDefineValueMap TouchedDefines;
		// Whether any file in the inlined set carried a preprocessor directive. Read by the Adopt gate,
		// which cannot write a conditional source back from an asset that only holds the cut result.
		bool bSourceHadPreprocessorDirectives = false;
		FDreamShaderError PreparedSourceError;
		if (!LoadPreparedDreamShaderSource(
			SourceFilePath,
			SourceText,
			TouchedDefines,
			bSourceHadPreprocessorDirectives,
			PreparedSourceError))
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

		const FString SourceHash = Private::BuildSourceHash(SourceText, TouchedDefines);

		if (Definition.Name.IsEmpty())
		{
			return FailWith(OutMessage, TEXT("DSH8078"), FString::Printf(TEXT("%s: This file does not define a top-level Shader block."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		// The Outputs block stopped being the only way to drive a material property -- a Graph block
		// writes them as `Base.<Attribute> = ...` -- so it is required only when there is no Graph
		// block to have written one. A Graph that turns out to write nothing is caught after it runs,
		// where the answer is known rather than guessed from the source text.
		if (Definition.Outputs.IsEmpty() && Definition.Code.IsEmpty())
		{
			return FailWith(OutMessage, TEXT("DSH8079"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s: This material drives no outputs. Add an Outputs block, or write them from Graph as 'Base.BaseColor = ...'."), *SourceFilePath));
		}

		TArray<Private::FResolvedNamedOutput> NamedOutputs;
		bool bUsesReturn = false;
		ECustomMaterialOutputType ReturnOutputType = CMOT_Float1;
		bool bReturnIsSubstrateMaterial = false;
		FDreamShaderError ValidationError;
		if (!Private::ValidateSettings(Definition, ValidationError)
			|| !Private::ValidateOutputs(Definition, NamedOutputs, bUsesReturn, ReturnOutputType, bReturnIsSubstrateMaterial, ValidationError))
		{
			return FailWith(OutMessage, TEXT("DSH8080"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *ValidationError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			return FailWith(OutMessage, TEXT("DSH8081"), FString::Printf(TEXT("%s: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader."), *SourceFilePath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		// Both backends consume the imported-functions include, so it is written before the
		// backend split.
		if (!Definition.Functions.IsEmpty())
		{
			FDreamShaderError IncludeWriteError;
			if (!Private::WriteGeneratedInclude(SourceFilePath, Definition, IncludeWriteError))
			{
				return FailWith(OutMessage, TEXT("DSH8082"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *IncludeWriteError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
		}

		Private::EResolvedBackend RequestedBackend = Private::EResolvedBackend::Graph;
		bool bExplicitBackend = false;
		FDreamShaderError BackendError;
		if (!Private::ResolveRequestedBackend(Definition, RequestedBackend, bExplicitBackend, BackendError))
		{
			return FailWith(OutMessage, TEXT("DSH8083"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *BackendError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
		FDreamShaderError MaterialError;
		MaterialSlowTask.EnterProgressFrame(
			1.0f,
			FText::Format(
				LOCTEXT("PreparingMaterialAsset", "Preparing material asset '{0}'..."),
				FText::FromString(Definition.Name)));
		if (!Private::CreateOrReuseMaterial(Definition, Material, MaterialError, bTransient) || !Material)
		{
			return FailWith(OutMessage, TEXT("DSH8084"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *MaterialError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		// From here on the asset's storage decides, not the request: an in-memory compile that landed on
		// an asset with a file behind it is a persisted compile. See IsGeneratedAssetPersisted.
		const bool bPersistThisMaterial = bTransient && Private::IsGeneratedAssetPersisted(Material);
		const bool bEffectiveTransient = bTransient && !bPersistThisMaterial;

		// Only one process per project writes these files. A second editor keeps its own in-memory
		// materials current and leaves the shared one to whoever owns the bridge.
		if (Private::ShouldDeferPersistedAssetToWriteOwner(Material, !bEffectiveTransient, OutMessage))
		{
			return true;
		}

		if (!bForce && Private::IsGeneratedAssetSourceCurrent(Material, SourceFilePath, SourceHash))
		{
			OutMessage = FString::Printf(TEXT("Skipped %s from %s; source hash is unchanged."), *Material->GetPathName(), *SourceFilePath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;
		}

		// Before the graph is touched: a rebuild is a destructive act on a hand-edited asset, and the
		// only moment it can still be refused is while the old graph is still there.
		//
		// The open-editor check comes first because it is the plainer obstacle -- close the window --
		// and because an editor that is open may still be about to change the divergence answer.
		FDreamShaderError EditorOpenError;
		if (!Private::CheckGeneratedAssetNotOpenInEditor(Material, EditorOpenError))
		{
			return FailWith(OutMessage, TEXT("DSH8085"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *EditorOpenError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		FDreamShaderError DivergenceError;
		if (!Private::CheckGeneratedAssetNotDiverged(Material, DivergenceError))
		{
			return FailWith(OutMessage, TEXT("DSH8086"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *DivergenceError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		if (!PopulateMaterialGraphFromDefinition(
				Material, Definition, SourceFilePath, SourceText, NamedOutputs,
				bUsesReturn, ReturnOutputType, bReturnIsSubstrateMaterial, bUsesFrontMaterial,
				bEffectiveTransient, MaterialSlowTask, OutMessage))
		{
			return false;
		}

		// Stamped for both modes, and ahead of the transient dirty-flag reset below because writing
		// package metadata dirties the package. An in-memory material is regenerated from scratch every
		// session, so its stamp only has to survive until the next compile in THIS one -- which is
		// exactly the window in which a hand edit to it can be destroyed.
		//
		// Path and hash both, in memory as on disk. The path is what makes the asset classify as ours
		// (without it the divergence gate is dead in the editor's default, memory-only mode); the hash
		// is what lets the skip check and the browser answer "is this asset current" truthfully -- a
		// memory-only material used to carry no hash and so read as stale forever. The skip this
		// switches on is the same one the thin-instance backend has always had: an unchanged source
		// is not rebuilt on save. Anything that MEANS "rebuild regardless" -- Recompile DSM, Clean
		// Generated Shaders, an explicit recompile request -- goes through the bridge's forced queue.
		if (bEffectiveTransient)
		{
			Private::ApplySourceMetadata(Material, SourceFilePath, SourceHash);
		}
		Private::ApplyOutputDigestMetadata(Material);

		if (bEffectiveTransient)
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
			FDreamShaderError SaveError;
			if (!Private::SaveAssetPackage(Material, SaveError))
			{
				return FailWith(OutMessage, TEXT("DSH8087"), FString::Printf(TEXT("%s: %s"), *SourceFilePath, *SaveError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
		}

		OutMessage = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("Generated %s from %s.%s"),
			*Material->GetPathName(),
			*SourceFilePath,
			bEffectiveTransient ? TEXT(" (virtual)") : (bPersistThisMaterial ? TEXT(" (saved; the asset exists on disk)") : TEXT("")));
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
