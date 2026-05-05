#include "DreamShaderMaterialGeneratorPrivate.h"

#include "DreamShaderModule.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	static void ConnectCodeValueToInput(FExpressionInput& Input, const FCodeValue& Value)
	{
		if (Value.Expression)
		{
			Input.Connect(Value.OutputIndex, Value.Expression);
		}
	}

	static bool TryResolveExpressionOutputIndex(const UMaterialExpression* Expression, const FString& OutputSpecifier, int32& OutIndex)
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

	static bool IsMaterialAttributesComponentType(const int32 ComponentCount, const bool bIsTextureObject)
	{
		return ComponentCount == 0 && !bIsTextureObject;
	}

	static bool TrySplitMemberTarget(const FString& TargetText, FString& OutBaseName, FString& OutMemberName)
	{
		FString Left;
		FString Right;
		if (!TargetText.TrimStartAndEnd().Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromStart))
		{
			return false;
		}

		OutBaseName = Left.TrimStartAndEnd();
		OutMemberName = Right.TrimStartAndEnd();
		return !OutBaseName.IsEmpty() && !OutMemberName.IsEmpty();
	}

	static bool ResolveTypeNameForComponentCount(const int32 ComponentCount, FString& OutTypeName)
	{
		switch (ComponentCount)
		{
		case 0: OutTypeName = TEXT("MaterialAttributes"); return true;
		case 1: OutTypeName = TEXT("float"); return true;
		case 2: OutTypeName = TEXT("float2"); return true;
		case 3: OutTypeName = TEXT("float3"); return true;
		case 4: OutTypeName = TEXT("float4"); return true;
		default:
			return false;
		}
	}

	static bool TryResolveMaterialAttributesBreakOutputIndex(const EMaterialProperty Property, int32& OutOutputIndex)
	{
		switch (Property)
		{
		case MP_BaseColor: OutOutputIndex = 0; return true;
		case MP_Metallic: OutOutputIndex = 1; return true;
		case MP_Specular: OutOutputIndex = 2; return true;
		case MP_Roughness: OutOutputIndex = 3; return true;
		case MP_Anisotropy: OutOutputIndex = 4; return true;
		case MP_EmissiveColor: OutOutputIndex = 5; return true;
		case MP_Opacity: OutOutputIndex = 6; return true;
		case MP_OpacityMask: OutOutputIndex = 7; return true;
		case MP_Normal: OutOutputIndex = 8; return true;
		case MP_Tangent: OutOutputIndex = 9; return true;
		case MP_WorldPositionOffset: OutOutputIndex = 10; return true;
		case MP_SubsurfaceColor: OutOutputIndex = 11; return true;
		case MP_CustomData0: OutOutputIndex = 12; return true;
		case MP_CustomData1: OutOutputIndex = 13; return true;
		case MP_AmbientOcclusion: OutOutputIndex = 14; return true;
		case MP_Refraction: OutOutputIndex = 15; return true;
		case MP_CustomizedUVs0: OutOutputIndex = 16; return true;
		case MP_CustomizedUVs1: OutOutputIndex = 17; return true;
		case MP_CustomizedUVs2: OutOutputIndex = 18; return true;
		case MP_CustomizedUVs3: OutOutputIndex = 19; return true;
		case MP_CustomizedUVs4: OutOutputIndex = 20; return true;
		case MP_CustomizedUVs5: OutOutputIndex = 21; return true;
		case MP_CustomizedUVs6: OutOutputIndex = 22; return true;
		case MP_CustomizedUVs7: OutOutputIndex = 23; return true;
		case MP_PixelDepthOffset: OutOutputIndex = 24; return true;
		case MP_Displacement: OutOutputIndex = 26; return true;
#ifdef MOON_ENGINE
		case MP_MooaEncodedAttribute0: OutOutputIndex = 27; return true;
		case MP_MooaEncodedAttribute1: OutOutputIndex = 28; return true;
		case MP_MooaEncodedAttribute2: OutOutputIndex = 29; return true;
		case MP_MooaEncodedAttribute3: OutOutputIndex = 30; return true;
		case MP_MooaEncodedAttribute4: OutOutputIndex = 31; return true;
#endif
		default:
			return false;
		}
	}

	static FString RemoveComments(const FString& Input)
	{
		FString Output;
		Output.Reserve(Input.Len());

		bool bInString = false;
		bool bInLineComment = false;
		bool bInBlockComment = false;

		for (int32 Index = 0; Index < Input.Len(); ++Index)
		{
			const TCHAR Char = Input[Index];
			const TCHAR Next = Input.IsValidIndex(Index + 1) ? Input[Index + 1] : TCHAR('\0');

			if (bInLineComment)
			{
				if (Char == TCHAR('\n'))
				{
					bInLineComment = false;
					Output.AppendChar(Char);
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
				Output.AppendChar(Char);
				if (Char == TCHAR('\\') && Input.IsValidIndex(Index + 1))
				{
					Output.AppendChar(Input[++Index]);
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
				Output.AppendChar(Char);
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

			Output.AppendChar(Char);
		}

		return Output;
	}

	static TArray<FString> SplitTopLevelSegments(const FString& InText, const TCHAR Delimiter)
	{
		TArray<FString> Segments;
		FString Current;
		int32 ParenthesisDepth = 0;
		int32 BraceDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;

		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Char = InText[Index];

			if (bInString)
			{
				Current.AppendChar(Char);
				if (Char == TCHAR('\\') && InText.IsValidIndex(Index + 1))
				{
					Current.AppendChar(InText[++Index]);
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

			if (Char == TCHAR('{'))
			{
				++BraceDepth;
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR('}'))
			{
				BraceDepth = FMath::Max(0, BraceDepth - 1);
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR('['))
			{
				++BracketDepth;
				Current.AppendChar(Char);
				continue;
			}

			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				Current.AppendChar(Char);
				continue;
			}

			if (Char == Delimiter && ParenthesisDepth == 0 && BraceDepth == 0 && BracketDepth == 0)
			{
				Current.TrimStartAndEndInline();
				if (!Current.IsEmpty())
				{
					Segments.Add(Current);
				}
				Current.Reset();
				continue;
			}

			Current.AppendChar(Char);
		}

		Current.TrimStartAndEndInline();
		if (!Current.IsEmpty())
		{
			Segments.Add(Current);
		}

		return Segments;
	}

	static bool IsIdentifierBoundary(const FString& Text, const int32 Index)
	{
		if (!Text.IsValidIndex(Index))
		{
			return true;
		}

		const TCHAR Char = Text[Index];
		return !(FChar::IsAlnum(Char) || Char == TCHAR('_'));
	}

	static bool MatchesKeywordAt(const FString& Text, const int32 Index, const TCHAR* Keyword)
	{
		const int32 KeywordLength = FCString::Strlen(Keyword);
		return Text.IsValidIndex(Index)
			&& Index + KeywordLength <= Text.Len()
			&& Text.Mid(Index, KeywordLength).Equals(Keyword, ESearchCase::CaseSensitive)
			&& IsIdentifierBoundary(Text, Index - 1)
			&& IsIdentifierBoundary(Text, Index + KeywordLength);
	}

	static void SkipWhitespace(const FString& Text, int32& InOutIndex)
	{
		while (Text.IsValidIndex(InOutIndex) && FChar::IsWhitespace(Text[InOutIndex]))
		{
			++InOutIndex;
		}
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

		int32 Depth = 0;
		bool bInString = false;
		for (int32 Index = OpenIndex; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];

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

			if (Char == OpenChar)
			{
				++Depth;
				continue;
			}

			if (Char == CloseChar)
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

	static bool FindIfStatementEnd(const FString& Text, const int32 IfIndex, int32& OutEndIndex, FString& OutError)
	{
		if (!MatchesKeywordAt(Text, IfIndex, TEXT("if")))
		{
			OutError = TEXT("Expected 'if'.");
			return false;
		}

		int32 Index = IfIndex + 2;
		SkipWhitespace(Text, Index);
		if (!Text.IsValidIndex(Index) || Text[Index] != TCHAR('('))
		{
			OutError = TEXT("Graph if statement is missing a condition block.");
			return false;
		}

		int32 ConditionCloseIndex = INDEX_NONE;
		if (!FindMatchingDelimiter(Text, Index, TCHAR('('), TCHAR(')'), ConditionCloseIndex))
		{
			OutError = TEXT("Graph if statement has an unterminated condition block.");
			return false;
		}

		Index = ConditionCloseIndex + 1;
		SkipWhitespace(Text, Index);
		if (!Text.IsValidIndex(Index) || Text[Index] != TCHAR('{'))
		{
			OutError = TEXT("Graph if statement is missing a '{ ... }' body.");
			return false;
		}

		int32 ThenCloseIndex = INDEX_NONE;
		if (!FindMatchingDelimiter(Text, Index, TCHAR('{'), TCHAR('}'), ThenCloseIndex))
		{
			OutError = TEXT("Graph if statement has an unterminated body block.");
			return false;
		}

		Index = ThenCloseIndex + 1;
		SkipWhitespace(Text, Index);
		if (MatchesKeywordAt(Text, Index, TEXT("else")))
		{
			Index += 4;
			SkipWhitespace(Text, Index);
			if (MatchesKeywordAt(Text, Index, TEXT("if")))
			{
				return FindIfStatementEnd(Text, Index, OutEndIndex, OutError);
			}

			if (!Text.IsValidIndex(Index) || Text[Index] != TCHAR('{'))
			{
				OutError = TEXT("Graph else statement is missing a '{ ... }' body.");
				return false;
			}

			int32 ElseCloseIndex = INDEX_NONE;
			if (!FindMatchingDelimiter(Text, Index, TCHAR('{'), TCHAR('}'), ElseCloseIndex))
			{
				OutError = TEXT("Graph else statement has an unterminated body block.");
				return false;
			}

			OutEndIndex = ElseCloseIndex + 1;
			return true;
		}

		OutEndIndex = Index;
		return true;
	}

	static bool SplitStatements(const FString& BlockContent, TArray<FString>& OutStatements, FString& OutError)
	{
		OutStatements.Reset();

		int32 Index = 0;
		while (Index < BlockContent.Len())
		{
			SkipWhitespace(BlockContent, Index);
			while (BlockContent.IsValidIndex(Index) && BlockContent[Index] == TCHAR(';'))
			{
				++Index;
				SkipWhitespace(BlockContent, Index);
			}

			if (Index >= BlockContent.Len())
			{
				break;
			}

			const int32 StatementStartIndex = Index;
			if (MatchesKeywordAt(BlockContent, Index, TEXT("if")))
			{
				int32 StatementEndIndex = INDEX_NONE;
				if (!FindIfStatementEnd(BlockContent, Index, StatementEndIndex, OutError))
				{
					return false;
				}

				FString Statement = BlockContent.Mid(StatementStartIndex, StatementEndIndex - StatementStartIndex).TrimStartAndEnd();
				if (!Statement.IsEmpty())
				{
					OutStatements.Add(Statement);
				}
				Index = StatementEndIndex;
				continue;
			}

			int32 ParenthesisDepth = 0;
			int32 BraceDepth = 0;
			int32 BracketDepth = 0;
			bool bInString = false;
			for (; Index < BlockContent.Len(); ++Index)
			{
				const TCHAR Char = BlockContent[Index];

				if (bInString)
				{
					if (Char == TCHAR('\\') && BlockContent.IsValidIndex(Index + 1))
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

				if (Char == TCHAR('('))
				{
					++ParenthesisDepth;
					continue;
				}
				if (Char == TCHAR(')'))
				{
					ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
					continue;
				}
				if (Char == TCHAR('{'))
				{
					++BraceDepth;
					continue;
				}
				if (Char == TCHAR('}'))
				{
					BraceDepth = FMath::Max(0, BraceDepth - 1);
					continue;
				}
				if (Char == TCHAR('['))
				{
					++BracketDepth;
					continue;
				}
				if (Char == TCHAR(']'))
				{
					BracketDepth = FMath::Max(0, BracketDepth - 1);
					continue;
				}

				if (Char == TCHAR(';') && ParenthesisDepth == 0 && BraceDepth == 0 && BracketDepth == 0)
				{
					break;
				}
			}

			FString Statement = BlockContent.Mid(StatementStartIndex, Index - StatementStartIndex).TrimStartAndEnd();
			if (!Statement.IsEmpty())
			{
				OutStatements.Add(Statement);
			}

			if (BlockContent.IsValidIndex(Index) && BlockContent[Index] == TCHAR(';'))
			{
				++Index;
			}
		}

		return true;
	}

	static bool SplitTopLevelAssignment(const FString& InText, FString& OutLeft, FString& OutRight)
	{
		int32 ParenthesisDepth = 0;
		int32 BraceDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;

		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Char = InText[Index];

			if (bInString)
			{
				if (Char == TCHAR('\\') && InText.IsValidIndex(Index + 1))
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

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				continue;
			}

			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				continue;
			}

			if (Char == TCHAR('{'))
			{
				++BraceDepth;
				continue;
			}

			if (Char == TCHAR('}'))
			{
				BraceDepth = FMath::Max(0, BraceDepth - 1);
				continue;
			}

			if (Char == TCHAR('['))
			{
				++BracketDepth;
				continue;
			}

			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				continue;
			}

			if (Char == TCHAR('=') && ParenthesisDepth == 0 && BraceDepth == 0 && BracketDepth == 0)
			{
				OutLeft = InText.Left(Index).TrimStartAndEnd();
				OutRight = InText.Mid(Index + 1).TrimStartAndEnd();
				return true;
			}
		}

		return false;
	}

	static bool SplitDeclarationTypeAndName(const FString& InText, FString& OutTypeToken, FString& OutNameToken)
	{
		const FString Trimmed = InText.TrimStartAndEnd();
		int32 ParenthesisDepth = 0;
		bool bInString = false;
		int32 LastWhitespaceIndex = INDEX_NONE;

		for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR Char = Trimmed[Index];

			if (bInString)
			{
				if (Char == TCHAR('\\') && Trimmed.IsValidIndex(Index + 1))
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

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				continue;
			}

			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				continue;
			}

			if (ParenthesisDepth == 0 && FChar::IsWhitespace(Char))
			{
				LastWhitespaceIndex = Index;
			}
		}

		if (LastWhitespaceIndex == INDEX_NONE)
		{
			return false;
		}

		OutTypeToken = Trimmed.Left(LastWhitespaceIndex).TrimStartAndEnd();
		OutNameToken = Trimmed.Mid(LastWhitespaceIndex + 1).TrimStartAndEnd();
		return !OutTypeToken.IsEmpty() && !OutNameToken.IsEmpty();
	}

	static bool IsBraceInitializerText(const FString& InText)
	{
		const FString Trimmed = InText.TrimStartAndEnd();
		return Trimmed.Len() >= 2 && Trimmed.StartsWith(TEXT("{")) && Trimmed.EndsWith(TEXT("}"));
	}

	static bool IsIdentifierToken(const FString& InText)
	{
		const FString Trimmed = InText.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}

		if (!(FChar::IsAlpha(Trimmed[0]) || Trimmed[0] == TCHAR('_')))
		{
			return false;
		}

		for (int32 Index = 1; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR Char = Trimmed[Index];
			if (!(FChar::IsAlnum(Char) || Char == TCHAR('_')))
			{
				return false;
			}
		}

		return true;
	}

	class FCodeExpressionParser
	{
	public:
		explicit FCodeExpressionParser(const FString& InSource)
			: Source(InSource)
		{
			Tokenize();
		}

		bool Parse(TSharedPtr<FCodeExpression>& OutExpression, FString& OutError)
		{
			OutExpression = ParseAdditive(OutError);
			if (!OutExpression)
			{
				return false;
			}

			if (Peek().Type != ECodeTokenType::End)
			{
				OutError = FString::Printf(TEXT("Unexpected token '%s' in Graph expression."), *Peek().Text);
				return false;
			}

			return true;
		}

	private:
		const FString& Source;
		TArray<FCodeToken> Tokens;
		int32 TokenIndex = 0;

		void Tokenize()
		{
			Tokens.Reset();

			int32 Index = 0;
			while (Index < Source.Len())
			{
				const TCHAR Char = Source[Index];
				if (FChar::IsWhitespace(Char))
				{
					++Index;
					continue;
				}

				if (FChar::IsAlpha(Char) || Char == TCHAR('_'))
				{
					const int32 Start = Index++;
					while (Index < Source.Len())
					{
						const TCHAR Inner = Source[Index];
						if (!(FChar::IsAlnum(Inner) || Inner == TCHAR('_')))
						{
							break;
						}
						++Index;
					}

					Tokens.Add({ ECodeTokenType::Identifier, Source.Mid(Start, Index - Start) });
					continue;
				}

				if (FChar::IsDigit(Char) || (Char == TCHAR('.') && Index + 1 < Source.Len() && FChar::IsDigit(Source[Index + 1])))
				{
					const int32 Start = Index++;
					bool bSeenExponent = false;
					while (Index < Source.Len())
					{
						const TCHAR Inner = Source[Index];
						if (FChar::IsDigit(Inner) || Inner == TCHAR('.'))
						{
							++Index;
							continue;
						}

						if (!bSeenExponent && (Inner == TCHAR('e') || Inner == TCHAR('E')))
						{
							bSeenExponent = true;
							++Index;
							if (Index < Source.Len() && (Source[Index] == TCHAR('+') || Source[Index] == TCHAR('-')))
							{
								++Index;
							}
							continue;
						}

						break;
					}

					Tokens.Add({ ECodeTokenType::Number, Source.Mid(Start, Index - Start) });
					continue;
				}

				if (Char == TCHAR('"'))
				{
					FString Value;
					++Index;
					while (Index < Source.Len() && Source[Index] != TCHAR('"'))
					{
						if (Source[Index] == TCHAR('\\') && Index + 1 < Source.Len())
						{
							++Index;
						}
						Value.AppendChar(Source[Index++]);
					}

					if (Index < Source.Len() && Source[Index] == TCHAR('"'))
					{
						++Index;
					}

					Tokens.Add({ ECodeTokenType::String, Value });
					continue;
				}

				const auto AddSingleToken = [this](const ECodeTokenType Type, const TCHAR CharValue)
				{
					Tokens.Add({ Type, FString(1, &CharValue) });
				};

				switch (Char)
				{
				case TCHAR('('): AddSingleToken(ECodeTokenType::LeftParen, Char); break;
				case TCHAR(')'): AddSingleToken(ECodeTokenType::RightParen, Char); break;
				case TCHAR(','): AddSingleToken(ECodeTokenType::Comma, Char); break;
				case TCHAR('.'): AddSingleToken(ECodeTokenType::Dot, Char); break;
				case TCHAR(':'):
					if (Index + 1 < Source.Len() && Source[Index + 1] == TCHAR(':'))
					{
						Tokens.Add({ ECodeTokenType::ScopeResolution, TEXT("::") });
						++Index;
					}
					else
					{
						AddSingleToken(ECodeTokenType::End, Char);
					}
					break;
				case TCHAR('+'): AddSingleToken(ECodeTokenType::Plus, Char); break;
				case TCHAR('-'): AddSingleToken(ECodeTokenType::Minus, Char); break;
				case TCHAR('*'): AddSingleToken(ECodeTokenType::Star, Char); break;
				case TCHAR('/'): AddSingleToken(ECodeTokenType::Slash, Char); break;
				case TCHAR('='): AddSingleToken(ECodeTokenType::Equals, Char); break;
				default: AddSingleToken(ECodeTokenType::End, Char); break;
				}

				++Index;
			}

			Tokens.Add({ ECodeTokenType::End, TEXT("") });
		}

		const FCodeToken& Peek(const int32 Offset = 0) const
		{
			const int32 Index = FMath::Clamp(TokenIndex + Offset, 0, Tokens.Num() - 1);
			return Tokens[Index];
		}

		bool Match(const ECodeTokenType Type)
		{
			if (Peek().Type == Type)
			{
				++TokenIndex;
				return true;
			}
			return false;
		}

		bool Expect(const ECodeTokenType Type, FString& OutError)
		{
			if (Match(Type))
			{
				return true;
			}

			OutError = FString::Printf(TEXT("Expected token type %d in Graph expression near '%s'."), static_cast<int32>(Type), *Peek().Text);
			return false;
		}

		TSharedPtr<FCodeExpression> ParseAdditive(FString& OutError)
		{
			TSharedPtr<FCodeExpression> Left = ParseMultiplicative(OutError);
			while (Left && (Peek().Type == ECodeTokenType::Plus || Peek().Type == ECodeTokenType::Minus))
			{
				const FString Operator = Peek().Text;
				++TokenIndex;

				TSharedPtr<FCodeExpression> Right = ParseMultiplicative(OutError);
				if (!Right)
				{
					return nullptr;
				}

				TSharedPtr<FCodeExpression> Expression = MakeShared<FCodeExpression>();
				Expression->Kind = ECodeExpressionKind::Binary;
				Expression->Text = Operator;
				Expression->Left = Left;
				Expression->Right = Right;
				Left = Expression;
			}

			return Left;
		}

		TSharedPtr<FCodeExpression> ParseMultiplicative(FString& OutError)
		{
			TSharedPtr<FCodeExpression> Left = ParseUnary(OutError);
			while (Left && (Peek().Type == ECodeTokenType::Star || Peek().Type == ECodeTokenType::Slash))
			{
				const FString Operator = Peek().Text;
				++TokenIndex;

				TSharedPtr<FCodeExpression> Right = ParseUnary(OutError);
				if (!Right)
				{
					return nullptr;
				}

				TSharedPtr<FCodeExpression> Expression = MakeShared<FCodeExpression>();
				Expression->Kind = ECodeExpressionKind::Binary;
				Expression->Text = Operator;
				Expression->Left = Left;
				Expression->Right = Right;
				Left = Expression;
			}

			return Left;
		}

		TSharedPtr<FCodeExpression> ParseUnary(FString& OutError)
		{
			if (Peek().Type == ECodeTokenType::Plus || Peek().Type == ECodeTokenType::Minus)
			{
				const FString Operator = Peek().Text;
				++TokenIndex;
				TSharedPtr<FCodeExpression> Operand = ParseUnary(OutError);
				if (!Operand)
				{
					return nullptr;
				}

				TSharedPtr<FCodeExpression> Expression = MakeShared<FCodeExpression>();
				Expression->Kind = ECodeExpressionKind::Unary;
				Expression->Text = Operator;
				Expression->Left = Operand;
				return Expression;
			}

			return ParsePostfix(OutError);
		}

		TSharedPtr<FCodeExpression> ParsePostfix(FString& OutError)
		{
			TSharedPtr<FCodeExpression> Expression = ParsePrimary(OutError);
			if (!Expression)
			{
				return nullptr;
			}

			while (true)
			{
				if (Match(ECodeTokenType::Dot) || Match(ECodeTokenType::ScopeResolution))
				{
					const bool bWasScopeResolution = Tokens[FMath::Max(0, TokenIndex - 1)].Type == ECodeTokenType::ScopeResolution;
					if (Peek().Type != ECodeTokenType::Identifier)
					{
						OutError = bWasScopeResolution ? TEXT("Expected function name after '::'.") : TEXT("Expected member name after '.'.");
						return nullptr;
					}

					TSharedPtr<FCodeExpression> Member = MakeShared<FCodeExpression>();
					Member->Kind = ECodeExpressionKind::MemberAccess;
					Member->Text = bWasScopeResolution ? FString(TEXT("::")) + Peek().Text : Peek().Text;
					Member->Left = Expression;
					Expression = Member;
					++TokenIndex;
					continue;
				}

				if (Match(ECodeTokenType::LeftParen))
				{
					TSharedPtr<FCodeExpression> Call = MakeShared<FCodeExpression>();
					Call->Kind = ECodeExpressionKind::Call;
					Call->Left = Expression;

					if (!Match(ECodeTokenType::RightParen))
					{
						while (true)
						{
							FCodeCallArgument Argument;
							if (Peek().Type == ECodeTokenType::Identifier && Peek(1).Type == ECodeTokenType::Equals)
							{
								Argument.bIsNamed = true;
								Argument.Name = Peek().Text;
								TokenIndex += 2;
							}

							Argument.Expression = ParseAdditive(OutError);
							if (!Argument.Expression)
							{
								return nullptr;
							}

							Call->Arguments.Add(Argument);

							if (Match(ECodeTokenType::RightParen))
							{
								break;
							}

							if (!Expect(ECodeTokenType::Comma, OutError))
							{
								return nullptr;
							}
						}
					}

					Expression = Call;
					continue;
				}

				break;
			}

			return Expression;
		}

		TSharedPtr<FCodeExpression> ParsePrimary(FString& OutError)
		{
			if (Peek().Type == ECodeTokenType::Identifier)
			{
				TSharedPtr<FCodeExpression> Expression = MakeShared<FCodeExpression>();
				Expression->Kind = ECodeExpressionKind::Name;
				Expression->Text = Peek().Text;
				++TokenIndex;
				return Expression;
			}

			if (Peek().Type == ECodeTokenType::Number)
			{
				TSharedPtr<FCodeExpression> Expression = MakeShared<FCodeExpression>();
				Expression->Kind = ECodeExpressionKind::NumberLiteral;
				Expression->Text = Peek().Text;
				++TokenIndex;
				return Expression;
			}

			if (Peek().Type == ECodeTokenType::String)
			{
				TSharedPtr<FCodeExpression> Expression = MakeShared<FCodeExpression>();
				Expression->Kind = ECodeExpressionKind::StringLiteral;
				Expression->Text = Peek().Text;
				++TokenIndex;
				return Expression;
			}

			if (Match(ECodeTokenType::LeftParen))
			{
				TSharedPtr<FCodeExpression> Expression = ParseAdditive(OutError);
				if (!Expression)
				{
					return nullptr;
				}

				if (!Expect(ECodeTokenType::RightParen, OutError))
				{
					return nullptr;
				}

				return Expression;
			}

			OutError = FString::Printf(TEXT("Unexpected token '%s' in Graph expression."), *Peek().Text);
			return nullptr;
		}
	};

	static bool SplitTopLevelComparison(const FString& InText, FString& OutLeft, FString& OutOperator, FString& OutRight)
	{
		static const TArray<FString> Operators = {
			TEXT(">="),
			TEXT("<="),
			TEXT("=="),
			TEXT("!="),
			TEXT(">"),
			TEXT("<"),
		};

		int32 ParenthesisDepth = 0;
		int32 BraceDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;

		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Char = InText[Index];

			if (bInString)
			{
				if (Char == TCHAR('\\') && InText.IsValidIndex(Index + 1))
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

			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				continue;
			}
			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				continue;
			}
			if (Char == TCHAR('{'))
			{
				++BraceDepth;
				continue;
			}
			if (Char == TCHAR('}'))
			{
				BraceDepth = FMath::Max(0, BraceDepth - 1);
				continue;
			}
			if (Char == TCHAR('['))
			{
				++BracketDepth;
				continue;
			}
			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				continue;
			}

			if (ParenthesisDepth != 0 || BraceDepth != 0 || BracketDepth != 0)
			{
				continue;
			}

			for (const FString& Operator : Operators)
			{
				if (Index + Operator.Len() <= InText.Len() && InText.Mid(Index, Operator.Len()) == Operator)
				{
					OutLeft = InText.Left(Index).TrimStartAndEnd();
					OutOperator = Operator;
					OutRight = InText.Mid(Index + Operator.Len()).TrimStartAndEnd();
					return !OutLeft.IsEmpty() && !OutRight.IsEmpty();
				}
			}
		}

		return false;
	}

	static bool ParseCodeCondition(const FString& ConditionText, FCodeCondition& OutCondition, FString& OutError)
	{
		FString LeftText;
		FString OperatorText;
		FString RightText;
		if (!SplitTopLevelComparison(ConditionText, LeftText, OperatorText, RightText))
		{
			LeftText = ConditionText.TrimStartAndEnd();
			OperatorText = TEXT("truthy");
		}

		if (LeftText.IsEmpty())
		{
			OutError = TEXT("Graph if condition is empty.");
			return false;
		}

		FCodeExpressionParser LeftParser(LeftText);
		if (!LeftParser.Parse(OutCondition.Left, OutError))
		{
			OutError = FString::Printf(TEXT("In Graph if condition '%s': %s"), *ConditionText, *OutError);
			return false;
		}

		OutCondition.Operator = OperatorText;
		if (OperatorText == TEXT("truthy"))
		{
			return true;
		}

		FCodeExpressionParser RightParser(RightText);
		if (!RightParser.Parse(OutCondition.Right, OutError))
		{
			OutError = FString::Printf(TEXT("In Graph if condition '%s': %s"), *ConditionText, *OutError);
			return false;
		}

		return true;
	}

	static bool ParseIfStatement(const FString& StatementText, FCodeStatement& OutStatement, FString& OutError)
	{
		int32 Index = 0;
		SkipWhitespace(StatementText, Index);
		if (!MatchesKeywordAt(StatementText, Index, TEXT("if")))
		{
			return false;
		}

		Index += 2;
		SkipWhitespace(StatementText, Index);
		int32 ConditionCloseIndex = INDEX_NONE;
		if (!StatementText.IsValidIndex(Index)
			|| StatementText[Index] != TCHAR('(')
			|| !FindMatchingDelimiter(StatementText, Index, TCHAR('('), TCHAR(')'), ConditionCloseIndex))
		{
			OutError = FString::Printf(TEXT("Invalid Graph if statement '%s'."), *StatementText);
			return false;
		}

		const FString ConditionText = StatementText.Mid(Index + 1, ConditionCloseIndex - Index - 1).TrimStartAndEnd();
		Index = ConditionCloseIndex + 1;
		SkipWhitespace(StatementText, Index);

		int32 ThenCloseIndex = INDEX_NONE;
		if (!StatementText.IsValidIndex(Index)
			|| StatementText[Index] != TCHAR('{')
			|| !FindMatchingDelimiter(StatementText, Index, TCHAR('{'), TCHAR('}'), ThenCloseIndex))
		{
			OutError = FString::Printf(TEXT("Invalid Graph if body in '%s'."), *StatementText);
			return false;
		}

		const FString ThenBody = StatementText.Mid(Index + 1, ThenCloseIndex - Index - 1);
		Index = ThenCloseIndex + 1;
		SkipWhitespace(StatementText, Index);

		FString ElseBody;
		if (MatchesKeywordAt(StatementText, Index, TEXT("else")))
		{
			Index += 4;
			SkipWhitespace(StatementText, Index);

			if (MatchesKeywordAt(StatementText, Index, TEXT("if")))
			{
				ElseBody = StatementText.Mid(Index).TrimStartAndEnd();
				Index = StatementText.Len();
			}
			else
			{
				int32 ElseCloseIndex = INDEX_NONE;
				if (!StatementText.IsValidIndex(Index)
					|| StatementText[Index] != TCHAR('{')
					|| !FindMatchingDelimiter(StatementText, Index, TCHAR('{'), TCHAR('}'), ElseCloseIndex))
				{
					OutError = FString::Printf(TEXT("Invalid Graph else body in '%s'."), *StatementText);
					return false;
				}

				ElseBody = StatementText.Mid(Index + 1, ElseCloseIndex - Index - 1);
				Index = ElseCloseIndex + 1;
			}

			SkipWhitespace(StatementText, Index);
		}

		if (Index < StatementText.Len())
		{
			OutError = FString::Printf(TEXT("Unexpected text after Graph if statement: '%s'."), *StatementText.Mid(Index).TrimStartAndEnd());
			return false;
		}

		OutStatement = FCodeStatement{};
		OutStatement.bIsIfStatement = true;
		if (!ParseCodeCondition(ConditionText, OutStatement.Condition, OutError))
		{
			return false;
		}

		if (!ParseCodeStatements(ThenBody, OutStatement.ThenStatements, OutError))
		{
			OutError = FString::Printf(TEXT("In Graph if body: %s"), *OutError);
			return false;
		}

		if (!ElseBody.IsEmpty() && !ParseCodeStatements(ElseBody, OutStatement.ElseStatements, OutError))
		{
			OutError = FString::Printf(TEXT("In Graph else body: %s"), *OutError);
			return false;
		}

		return true;
	}

	bool ParseCodeStatements(const FString& InCode, TArray<FCodeStatement>& OutStatements, FString& OutError)
	{
		OutStatements.Reset();
		TArray<FString> Statements;
		if (!SplitStatements(RemoveComments(InCode), Statements, OutError))
		{
			return false;
		}

		for (const FString& StatementText : Statements)
		{
			int32 StatementProbeIndex = 0;
			SkipWhitespace(StatementText, StatementProbeIndex);
			if (MatchesKeywordAt(StatementText, StatementProbeIndex, TEXT("if")))
			{
				FCodeStatement IfStatement;
				if (!ParseIfStatement(StatementText, IfStatement, OutError))
				{
					return false;
				}

				OutStatements.Add(IfStatement);
				continue;
			}

			const auto ParseDeclarationStatement =
				[&OutError](const FString& DeclaredType, const FString& TargetName, const FString* InitializerText, FCodeStatement& OutStatement) -> bool
			{
				OutStatement = FCodeStatement{};
				OutStatement.bIsDeclaration = true;
				OutStatement.DeclaredType = DeclaredType;
				OutStatement.TargetName = TargetName;

				if (!InitializerText)
				{
					return true;
				}

				OutStatement.InitializerText = *InitializerText;
				if (IsBraceInitializerText(*InitializerText))
				{
					OutStatement.bUsesBraceInitializer = true;
					return true;
				}

				FCodeExpressionParser ExpressionParser(*InitializerText);
				if (!ExpressionParser.Parse(OutStatement.Expression, OutError))
				{
					return false;
				}

				return true;
			};

			const TArray<FString> Declarators = SplitTopLevelSegments(StatementText, TCHAR(','));
			if (Declarators.Num() > 1)
			{
				FString FirstLeft;
				FString FirstRight;
				const bool bFirstHasAssignment = SplitTopLevelAssignment(Declarators[0], FirstLeft, FirstRight);
				const FString FirstDeclaratorText = bFirstHasAssignment ? FirstLeft : Declarators[0];

				FString SharedType;
				FString FirstName;
				if (SplitDeclarationTypeAndName(FirstDeclaratorText, SharedType, FirstName))
				{
					for (int32 DeclaratorIndex = 0; DeclaratorIndex < Declarators.Num(); ++DeclaratorIndex)
					{
						const FString& DeclaratorText = Declarators[DeclaratorIndex];
						FCodeStatement Statement;

						if (DeclaratorIndex == 0)
						{
							const FString* InitializerText = bFirstHasAssignment ? &FirstRight : nullptr;
							if (!ParseDeclarationStatement(SharedType, FirstName, InitializerText, Statement))
							{
								OutError = FString::Printf(TEXT("In Graph statement '%s': %s"), *StatementText, *OutError);
								return false;
							}
						}
						else
						{
							FString Left;
							FString Right;
							const bool bHasAssignment = SplitTopLevelAssignment(DeclaratorText, Left, Right);
							const FString NameToken = bHasAssignment ? Left : DeclaratorText;
							if (!IsIdentifierToken(NameToken))
							{
								OutError = FString::Printf(
									TEXT("In Graph statement '%s': '%s' is not a valid declarator in a comma-separated declaration."),
									*StatementText,
									*NameToken.TrimStartAndEnd());
								return false;
							}

							const FString TrimmedNameToken = NameToken.TrimStartAndEnd();
							const FString* InitializerText = bHasAssignment ? &Right : nullptr;
							if (!ParseDeclarationStatement(SharedType, TrimmedNameToken, InitializerText, Statement))
							{
								OutError = FString::Printf(TEXT("In Graph statement '%s': %s"), *StatementText, *OutError);
								return false;
							}
						}

						OutStatements.Add(Statement);
					}

					continue;
				}
			}

			FCodeStatement Statement;

			FString Left;
			FString Right;
			if (!SplitTopLevelAssignment(StatementText, Left, Right))
			{
				if (SplitDeclarationTypeAndName(StatementText, Statement.DeclaredType, Statement.TargetName))
				{
					Statement.bIsDeclaration = true;
					OutStatements.Add(Statement);
					continue;
				}

				FCodeExpressionParser ExpressionParser(StatementText);
				if (!ExpressionParser.Parse(Statement.Expression, OutError))
				{
					OutError = FString::Printf(TEXT("In Graph statement '%s': %s"), *StatementText, *OutError);
					return false;
				}

				Statement.bIsExpressionStatement = true;
				OutStatements.Add(Statement);
				continue;
			}

			Statement.bIsDeclaration = SplitDeclarationTypeAndName(Left, Statement.DeclaredType, Statement.TargetName);
			if (!Statement.bIsDeclaration)
			{
				Statement.TargetName = Left;
			}
			Statement.InitializerText = Right;

			if (IsBraceInitializerText(Right))
			{
				Statement.bUsesBraceInitializer = true;
				OutStatements.Add(Statement);
				continue;
			}

			FCodeExpressionParser ExpressionParser(Right);
			if (!ExpressionParser.Parse(Statement.Expression, OutError))
			{
				OutError = FString::Printf(TEXT("In Graph statement '%s': %s"), *StatementText, *OutError);
				return false;
			}

			OutStatements.Add(Statement);
		}

		return true;
	}

	FCodeGraphBuilder::FCodeGraphBuilder(
		UMaterial* InMaterial,
		UMaterialFunction* InMaterialFunction,
		const FTextShaderDefinition& InDefinition,
		const FString& InSourceFilePath,
		const FString& InIncludeVirtualPath,
		const TArray<FTextShaderPropertyDefinition>* InLocalProperties)
		: Material(InMaterial)
		, MaterialFunction(InMaterialFunction)
		, Definition(InDefinition)
		, LocalProperties(InLocalProperties)
		, SourceFilePath(InSourceFilePath)
		, IncludeVirtualPath(InIncludeVirtualPath)
	{
	}

	bool FCodeGraphBuilder::Build(
		const TArray<FCodeStatement>& Statements,
		TMap<FString, FCodeValue>& InOutValues,
		FString& OutError)
	{
		Values = &InOutValues;

		for (const FCodeStatement& Statement : Statements)
		{
			if (!ExecuteStatement(Statement, OutError))
			{
				return false;
			}
		}

		return true;
	}

	bool FCodeGraphBuilder::ExecuteStatement(const FCodeStatement& Statement, FString& OutError)
	{
		if (Statement.bIsIfStatement)
		{
			return ExecuteIfStatement(Statement, OutError);
		}

		if (!Statement.Expression && !Statement.bIsDeclaration && !Statement.bUsesBraceInitializer)
		{
			OutError = TEXT("Encountered an invalid empty Graph statement.");
			return false;
		}

		if (Statement.bIsExpressionStatement)
		{
			return ExecuteExpressionStatement(Statement.Expression, OutError);
		}

		if (Statement.TargetName.IsEmpty())
		{
			OutError = TEXT("Encountered a Graph assignment without a target variable.");
			return false;
		}

		if (!Statement.bIsDeclaration)
		{
			FString MemberBaseName;
			FString MemberName;
			if (TrySplitMemberTarget(Statement.TargetName, MemberBaseName, MemberName))
			{
				FCodeValue EvaluatedMemberValue;
				if (Statement.bUsesBraceInitializer)
				{
					FString TargetTypeName;
					if (!ResolveTargetTypeForAssignment(Statement, TargetTypeName, OutError)
						|| !EvaluateBraceInitializer(TargetTypeName, Statement.InitializerText, EvaluatedMemberValue, OutError))
					{
						OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
						return false;
					}
				}
				else if (Statement.Expression)
				{
					if (!EvaluateExpression(Statement.Expression, EvaluatedMemberValue, OutError))
					{
						OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
						return false;
					}
				}
				else
				{
					OutError = FString::Printf(TEXT("MaterialAttributes member assignment '%s' requires a value."), *Statement.TargetName);
					return false;
				}

				if (!AssignMaterialAttributesMember(Statement.TargetName, EvaluatedMemberValue, OutError))
				{
					OutError = FString::Printf(TEXT("Failed to assign Graph member '%s'. %s"), *Statement.TargetName, *OutError);
					return false;
				}

				return true;
			}
		}

		if (Statement.bIsDeclaration && FindValue(Statement.TargetName))
		{
			OutError = FString::Printf(TEXT("Graph variable '%s' is declared more than once."), *Statement.TargetName);
			return false;
		}

		FCodeValue EvaluatedValue;
		if (Statement.bUsesBraceInitializer)
		{
			FString TargetTypeName;
			if (!ResolveTargetTypeForAssignment(Statement, TargetTypeName, OutError)
				|| !EvaluateBraceInitializer(TargetTypeName, Statement.InitializerText, EvaluatedValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
				return false;
			}
		}
		else if (Statement.Expression)
		{
			if (!EvaluateExpression(Statement.Expression, EvaluatedValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to evaluate Graph assignment for '%s'. %s"), *Statement.TargetName, *OutError);
				return false;
			}
		}
		else if (Statement.bIsDeclaration)
		{
			if (!CreateDefaultValue(Statement.DeclaredType, EvaluatedValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to declare Graph variable '%s'. %s"), *Statement.TargetName, *OutError);
				return false;
			}
		}

		if (Statement.bIsDeclaration)
		{
			int32 ExpectedComponentCount = 1;
			bool bExpectedTexture = false;
			if (!TryResolveCodeDeclaredType(Statement.DeclaredType, ExpectedComponentCount, bExpectedTexture))
			{
				OutError = FString::Printf(TEXT("Unsupported Graph variable type '%s' for '%s'."), *Statement.DeclaredType, *Statement.TargetName);
				return false;
			}

			FCodeValue CoercedValue;
			if (!CoerceValueToType(EvaluatedValue, ExpectedComponentCount, bExpectedTexture, CoercedValue, OutError))
			{
				OutError = FString::Printf(
					TEXT("Graph variable '%s' is declared as '%s' but assigned an incompatible value. %s"),
					*Statement.TargetName,
					*Statement.DeclaredType,
					*OutError);
				return false;
			}

			EvaluatedValue = CoercedValue;
		}
		else if (const FCodeValue* ExistingValue = FindValue(Statement.TargetName))
		{
			FCodeValue CoercedValue;
			if (!CoerceValueToType(EvaluatedValue, ExistingValue->ComponentCount, ExistingValue->bIsTextureObject, CoercedValue, OutError))
			{
				OutError = FString::Printf(
					TEXT("Graph variable '%s' was previously assigned an incompatible value. %s"),
					*Statement.TargetName,
					*OutError);
				return false;
			}

			EvaluatedValue = CoercedValue;
		}

		(*Values).Add(Statement.TargetName, EvaluatedValue);
		return true;
	}

	static bool AreCodeValuesEquivalent(const FCodeValue& Left, const FCodeValue& Right)
	{
		return Left.Expression == Right.Expression
			&& Left.OutputIndex == Right.OutputIndex
			&& Left.ComponentCount == Right.ComponentCount
			&& Left.bIsTextureObject == Right.bIsTextureObject
			&& Left.bIsMaterialAttributes == Right.bIsMaterialAttributes;
	}

	static void CollectChangedValueNames(
		const TMap<FString, FCodeValue>& BaseValues,
		const TMap<FString, FCodeValue>& BranchValues,
		TSet<FString>& OutNames)
	{
		for (const TPair<FString, FCodeValue>& Pair : BranchValues)
		{
			const FCodeValue* BaseValue = BaseValues.Find(Pair.Key);
			if (!BaseValue || !AreCodeValuesEquivalent(*BaseValue, Pair.Value))
			{
				OutNames.Add(Pair.Key);
			}
		}
	}

	bool FCodeGraphBuilder::ExecuteIfStatement(const FCodeStatement& Statement, FString& OutError)
	{
		if (!Values)
		{
			OutError = TEXT("Graph builder is not initialized.");
			return false;
		}

		TMap<FString, FCodeValue>* OuterValues = Values;
		const TMap<FString, FCodeValue> BaseValues = *OuterValues;

		TMap<FString, FCodeValue> ThenValues = BaseValues;
		Values = &ThenValues;
		for (const FCodeStatement& ThenStatement : Statement.ThenStatements)
		{
			if (!ExecuteStatement(ThenStatement, OutError))
			{
				Values = OuterValues;
				OutError = FString::Printf(TEXT("In Graph if body: %s"), *OutError);
				return false;
			}
		}

		TMap<FString, FCodeValue> ElseValues = BaseValues;
		Values = &ElseValues;
		for (const FCodeStatement& ElseStatement : Statement.ElseStatements)
		{
			if (!ExecuteStatement(ElseStatement, OutError))
			{
				Values = OuterValues;
				OutError = FString::Printf(TEXT("In Graph else body: %s"), *OutError);
				return false;
			}
		}

		Values = OuterValues;

		TSet<FString> ChangedNames;
		CollectChangedValueNames(BaseValues, ThenValues, ChangedNames);
		CollectChangedValueNames(BaseValues, ElseValues, ChangedNames);

		for (const FString& Name : ChangedNames)
		{
			const FCodeValue* ThenValue = ThenValues.Find(Name);
			const FCodeValue* ElseValue = ElseValues.Find(Name);
			if (!ThenValue || !ElseValue)
			{
				OutError = FString::Printf(TEXT("Graph if statement could not resolve both branch values for '%s'."), *Name);
				return false;
			}

			int32 ExpectedComponentCount = ThenValue->ComponentCount;
			bool bExpectedTexture = ThenValue->bIsTextureObject;
			if (const FCodeValue* BaseValue = BaseValues.Find(Name))
			{
				ExpectedComponentCount = BaseValue->ComponentCount;
				bExpectedTexture = BaseValue->bIsTextureObject;
			}
			else
			{
				int32 OutputComponentCount = 0;
				bool bOutputIsTexture = false;
				if (TryResolveOutputVariableComponentCount(Definition, Name, OutputComponentCount, bOutputIsTexture))
				{
					ExpectedComponentCount = OutputComponentCount;
					bExpectedTexture = bOutputIsTexture;
				}
			}

			if (bExpectedTexture)
			{
				OutError = FString::Printf(TEXT("Graph if statement cannot select Texture2D value '%s'."), *Name);
				return false;
			}

			FCodeValue CoercedThenValue;
			FCodeValue CoercedElseValue;
			if (!CoerceValueToType(*ThenValue, ExpectedComponentCount, bExpectedTexture, CoercedThenValue, OutError)
				|| !CoerceValueToType(*ElseValue, ExpectedComponentCount, bExpectedTexture, CoercedElseValue, OutError))
			{
				OutError = FString::Printf(TEXT("Graph if branches assign incompatible values to '%s'. %s"), *Name, *OutError);
				return false;
			}

			FCodeValue ConditionalValue;
			if (!CreateConditionalValue(Statement.Condition, CoercedThenValue, CoercedElseValue, ConditionalValue, OutError))
			{
				OutError = FString::Printf(TEXT("Graph if statement failed to merge '%s'. %s"), *Name, *OutError);
				return false;
			}

			(*Values).Add(Name, ConditionalValue);
		}

		return true;
	}

	bool FCodeGraphBuilder::CreateConditionalValue(
		const FCodeCondition& Condition,
		const FCodeValue& TrueValue,
		const FCodeValue& FalseValue,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (TrueValue.bIsTextureObject || FalseValue.bIsTextureObject)
		{
			OutError = TEXT("Texture2D values cannot be selected by Graph if statements.");
			return false;
		}
		if (TrueValue.bIsMaterialAttributes != FalseValue.bIsMaterialAttributes)
		{
			OutError = TEXT("Graph if branches cannot mix MaterialAttributes and numeric values.");
			return false;
		}

		FCodeValue LeftValue;
		if (!EvaluateExpression(Condition.Left, LeftValue, OutError))
		{
			OutError = FString::Printf(TEXT("Failed to evaluate Graph if condition. %s"), *OutError);
			return false;
		}

		if (LeftValue.bIsTextureObject || LeftValue.bIsMaterialAttributes || LeftValue.ComponentCount != 1)
		{
			OutError = TEXT("Graph if condition left side must evaluate to a scalar value.");
			return false;
		}

		FCodeValue RightValue;
		if (Condition.Operator == TEXT("truthy"))
		{
			RightValue.Expression = CreateScalarLiteralNode(0.0, ConsumeNodeY());
			if (!RightValue.Expression)
			{
				OutError = TEXT("Failed to create a zero literal for Graph if condition.");
				return false;
			}
			RightValue.ComponentCount = 1;
		}
		else
		{
			if (!EvaluateExpression(Condition.Right, RightValue, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to evaluate Graph if condition. %s"), *OutError);
				return false;
			}
		}

		if (RightValue.bIsTextureObject || RightValue.bIsMaterialAttributes || RightValue.ComponentCount != 1)
		{
			OutError = TEXT("Graph if condition right side must evaluate to a scalar value.");
			return false;
		}

		auto* IfExpression = Cast<UMaterialExpressionIf>(
			CreateExpression(UMaterialExpressionIf::StaticClass(), 520, ConsumeNodeY()));
		if (!IfExpression)
		{
			OutError = TEXT("Failed to create a Material If node.");
			return false;
		}

		ConnectCodeValueToInput(IfExpression->A, LeftValue);
		ConnectCodeValueToInput(IfExpression->B, RightValue);

		const auto ConnectBranches = [&](const FCodeValue& GreaterValue, const FCodeValue& EqualValue, const FCodeValue& LessValue)
		{
			ConnectCodeValueToInput(IfExpression->AGreaterThanB, GreaterValue);
			ConnectCodeValueToInput(IfExpression->AEqualsB, EqualValue);
			ConnectCodeValueToInput(IfExpression->ALessThanB, LessValue);
		};

		if (Condition.Operator == TEXT("truthy") || Condition.Operator == TEXT(">"))
		{
			ConnectBranches(TrueValue, FalseValue, FalseValue);
		}
		else if (Condition.Operator == TEXT("<"))
		{
			ConnectBranches(FalseValue, FalseValue, TrueValue);
		}
		else if (Condition.Operator == TEXT(">="))
		{
			ConnectBranches(TrueValue, TrueValue, FalseValue);
		}
		else if (Condition.Operator == TEXT("<="))
		{
			ConnectBranches(FalseValue, TrueValue, TrueValue);
		}
		else if (Condition.Operator == TEXT("=="))
		{
			ConnectBranches(FalseValue, TrueValue, FalseValue);
		}
		else if (Condition.Operator == TEXT("!="))
		{
			ConnectBranches(TrueValue, FalseValue, TrueValue);
		}
		else
		{
			OutError = FString::Printf(TEXT("Unsupported Graph if comparison operator '%s'."), *Condition.Operator);
			return false;
		}

		OutValue.Expression = IfExpression;
		OutValue.OutputIndex = 0;
		OutValue.ComponentCount = TrueValue.ComponentCount;
		OutValue.bIsTextureObject = false;
		OutValue.bIsMaterialAttributes = TrueValue.bIsMaterialAttributes;
		return true;
	}

	bool FCodeGraphBuilder::EvaluateOutputExpression(const FString& ExpressionText, FCodeValue& OutValue, FString& OutError)
	{
		FCodeExpressionParser ExpressionParser(ExpressionText);
		TSharedPtr<FCodeExpression> ParsedExpression;
		if (!ExpressionParser.Parse(ParsedExpression, OutError))
		{
			OutError = FString::Printf(TEXT("In output expression '%s': %s"), *ExpressionText, *OutError);
			return false;
		}

		if (!EvaluateExpression(ParsedExpression, OutValue, OutError))
		{
			OutError = FString::Printf(TEXT("In output expression '%s': %s"), *ExpressionText, *OutError);
			return false;
		}

		return true;
	}

	FCodeValue* FCodeGraphBuilder::FindValue(const FString& Name) const
	{
		if (!Values)
		{
			return nullptr;
		}

		if (FCodeValue* ExactMatch = Values->Find(Name))
		{
			return ExactMatch;
		}

		for (TPair<FString, FCodeValue>& Pair : *Values)
		{
			if (Pair.Key.Equals(Name, ESearchCase::IgnoreCase))
			{
				return &Pair.Value;
			}
		}

		return nullptr;
	}

	int32 FCodeGraphBuilder::ConsumeNodeY()
	{
		const int32 Result = NextNodeY;
		NextNodeY += 180;
		return Result;
	}

	UMaterialExpression* FCodeGraphBuilder::CreateExpression(
		const TSubclassOf<UMaterialExpression> ExpressionClass,
		const int32 PositionX,
		const int32 PositionY) const
	{
		return UMaterialEditingLibrary::CreateMaterialExpressionEx(
			Material,
			MaterialFunction,
			ExpressionClass,
			nullptr,
			PositionX,
			PositionY);
	}

	UMaterialExpression* FCodeGraphBuilder::CreateScalarLiteralNode(const double Value, const int32 PositionY) const
	{
		auto* Expression = Cast<UMaterialExpressionConstant>(
			CreateExpression(UMaterialExpressionConstant::StaticClass(), -1120, PositionY));
		if (Expression)
		{
			Expression->R = static_cast<float>(Value);
		}
		return Expression;
	}

	bool FCodeGraphBuilder::CreateMaterialAttributesValue(FCodeValue& OutValue, FString& OutError)
	{
		auto* Expression = Cast<UMaterialExpressionMakeMaterialAttributes>(
			CreateExpression(UMaterialExpressionMakeMaterialAttributes::StaticClass(), 200, ConsumeNodeY()));
		if (!Expression)
		{
			OutError = TEXT("Failed to create a MakeMaterialAttributes node.");
			return false;
		}

		OutValue = FCodeValue{};
		OutValue.Expression = Expression;
		OutValue.OutputIndex = 0;
		OutValue.ComponentCount = 0;
		OutValue.bIsTextureObject = false;
		OutValue.bIsMaterialAttributes = true;
		return true;
	}

	bool FCodeGraphBuilder::CreateDefaultValue(const FString& DeclaredType, FCodeValue& OutValue, FString& OutError)
	{
		int32 ComponentCount = 1;
		bool bIsTexture = false;
		if (!TryResolveCodeDeclaredType(DeclaredType, ComponentCount, bIsTexture))
		{
			OutError = FString::Printf(TEXT("Unsupported Graph variable type '%s'."), *DeclaredType);
			return false;
		}

		if (IsMaterialAttributesComponentType(ComponentCount, bIsTexture))
		{
			return CreateMaterialAttributesValue(OutValue, OutError);
		}

		if (bIsTexture)
		{
			OutError = FString::Printf(TEXT("Graph variable type '%s' requires an explicit initializer."), *DeclaredType);
			return false;
		}

		FCodeValue ZeroScalar;
		ZeroScalar.Expression = CreateScalarLiteralNode(0.0, ConsumeNodeY());
		if (!ZeroScalar.Expression)
		{
			OutError = TEXT("Failed to create a default literal node.");
			return false;
		}
		ZeroScalar.ComponentCount = 1;

		if (ComponentCount == 1)
		{
			OutValue = ZeroScalar;
			return true;
		}

		TArray<FCodeValue> Parts;
		for (int32 Index = 0; Index < ComponentCount; ++Index)
		{
			Parts.Add(ZeroScalar);
		}

		if (!AppendValues(Parts, OutValue, OutError))
		{
			return false;
		}

		OutValue.ComponentCount = ComponentCount;
		return true;
	}

	bool FCodeGraphBuilder::CoerceValueToType(
		const FCodeValue& InValue,
		const int32 ExpectedComponentCount,
		const bool bExpectedTexture,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (IsMaterialAttributesComponentType(ExpectedComponentCount, bExpectedTexture))
		{
			if (!InValue.bIsMaterialAttributes)
			{
				OutError = TEXT("Expected a MaterialAttributes value.");
				return false;
			}

			OutValue = InValue;
			return true;
		}

		if (bExpectedTexture)
		{
			if (!InValue.bIsTextureObject)
			{
				OutError = TEXT("Expected a texture object value.");
				return false;
			}

			OutValue = InValue;
			return true;
		}

		if (InValue.bIsMaterialAttributes)
		{
			OutError = TEXT("MaterialAttributes values cannot be assigned to numeric outputs.");
			return false;
		}

		if (InValue.bIsTextureObject)
		{
			OutError = TEXT("Texture objects cannot be assigned to numeric outputs.");
			return false;
		}

		if (InValue.ComponentCount == ExpectedComponentCount)
		{
			OutValue = InValue;
			return true;
		}

		if (ExpectedComponentCount > 1 && InValue.ComponentCount == 1)
		{
			TArray<FCodeValue> ReplicatedParts;
			ReplicatedParts.Reserve(ExpectedComponentCount);
			for (int32 Index = 0; Index < ExpectedComponentCount; ++Index)
			{
				ReplicatedParts.Add(InValue);
			}

			if (!AppendValues(ReplicatedParts, OutValue, OutError))
			{
				return false;
			}

			OutValue.ComponentCount = ExpectedComponentCount;
			return true;
		}

		OutError = FString::Printf(
			TEXT("Expected %d component(s) but got %d."),
			ExpectedComponentCount,
			InValue.ComponentCount);
		return false;
	}

	bool FCodeGraphBuilder::EvaluateBraceInitializer(
		const FString& ConstructorType,
		const FString& InitializerText,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const FString Trimmed = InitializerText.TrimStartAndEnd();
		if (!Trimmed.StartsWith(TEXT("{")) || !Trimmed.EndsWith(TEXT("}")))
		{
			OutError = FString::Printf(TEXT("Initializer '%s' is not a valid brace initializer."), *InitializerText);
			return false;
		}

		const FString InnerText = Trimmed.Mid(1, Trimmed.Len() - 2).TrimStartAndEnd();
		if (InnerText.IsEmpty())
		{
			return CreateDefaultValue(ConstructorType, OutValue, OutError);
		}

		const FString ConstructorExpression = FString::Printf(TEXT("%s(%s)"), *ConstructorType, *InnerText);
		FCodeExpressionParser ExpressionParser(ConstructorExpression);
		TSharedPtr<FCodeExpression> ParsedExpression;
		if (!ExpressionParser.Parse(ParsedExpression, OutError))
		{
			OutError = FString::Printf(TEXT("Invalid brace initializer for type '%s'. %s"), *ConstructorType, *OutError);
			return false;
		}

		return EvaluateExpression(ParsedExpression, OutValue, OutError);
	}

	bool FCodeGraphBuilder::ResolveTargetTypeForAssignment(
		const FCodeStatement& Statement,
		FString& OutTypeName,
		FString& OutError) const
	{
		if (Statement.bIsDeclaration)
		{
			OutTypeName = Statement.DeclaredType;
			return true;
		}

		FString BaseName;
		FString MemberName;
		if (TrySplitMemberTarget(Statement.TargetName, BaseName, MemberName))
		{
			int32 MemberComponentCount = 0;
			return ResolveMaterialAttributesMemberType(MemberName, MemberComponentCount, OutTypeName, OutError);
		}

		if (const FCodeValue* ExistingValue = FindValue(Statement.TargetName))
		{
			if (ExistingValue->bIsTextureObject)
			{
				OutError = FString::Printf(TEXT("Brace initializer assignment is not supported for texture variable '%s'."), *Statement.TargetName);
				return false;
			}

			if (ResolveTypeNameForComponentCount(ExistingValue->ComponentCount, OutTypeName))
			{
				return true;
			}
		}

		int32 OutputComponentCount = 1;
		bool bOutputIsTexture = false;
		if (TryResolveOutputVariableComponentCount(Definition, Statement.TargetName, OutputComponentCount, bOutputIsTexture))
		{
			if (bOutputIsTexture)
			{
				OutError = FString::Printf(TEXT("Brace initializer assignment is not supported for texture output '%s'."), *Statement.TargetName);
				return false;
			}

			if (ResolveTypeNameForComponentCount(OutputComponentCount, OutTypeName))
			{
				return true;
			}
		}

		OutError = FString::Printf(TEXT("Brace initializer assignment for '%s' requires a declared scalar or vector target type."), *Statement.TargetName);
		return false;
	}

	bool FCodeGraphBuilder::ResolveMaterialAttributesMemberType(
		const FString& MemberName,
		int32& OutComponentCount,
		FString& OutTypeName,
		FString& OutError) const
	{
		FResolvedMaterialProperty ResolvedProperty;
		if (!ResolveMaterialProperty(MemberName, ResolvedProperty)
			|| ResolvedProperty.Property == MP_MaterialAttributes)
		{
			OutError = FString::Printf(TEXT("Unsupported MaterialAttributes member '%s'."), *MemberName);
			return false;
		}

		if (!TryGetComponentCountForOutputType(ResolvedProperty.OutputType, OutComponentCount)
			|| OutComponentCount <= 0
			|| !ResolveTypeNameForComponentCount(OutComponentCount, OutTypeName))
		{
			OutError = FString::Printf(TEXT("MaterialAttributes member '%s' does not have a numeric scalar/vector type."), *MemberName);
			return false;
		}

		return true;
	}

	bool FCodeGraphBuilder::AssignMaterialAttributesMember(const FString& TargetName, const FCodeValue& InValue, FString& OutError)
	{
		FString BaseName;
		FString MemberName;
		if (!TrySplitMemberTarget(TargetName, BaseName, MemberName))
		{
			OutError = FString::Printf(TEXT("Invalid MaterialAttributes member assignment target '%s'."), *TargetName);
			return false;
		}

		FCodeValue* BaseValue = FindValue(BaseName);
		if (!BaseValue)
		{
			OutError = FString::Printf(TEXT("Unknown MaterialAttributes variable '%s'."), *BaseName);
			return false;
		}
		if (!BaseValue->bIsMaterialAttributes)
		{
			OutError = FString::Printf(TEXT("Graph variable '%s' is not a MaterialAttributes value."), *BaseName);
			return false;
		}

		FResolvedMaterialProperty ResolvedProperty;
		if (!ResolveMaterialProperty(MemberName, ResolvedProperty)
			|| ResolvedProperty.Property == MP_MaterialAttributes)
		{
			OutError = FString::Printf(TEXT("Unsupported MaterialAttributes member '%s'."), *MemberName);
			return false;
		}

		int32 ExpectedComponentCount = 0;
		if (!TryGetComponentCountForOutputType(ResolvedProperty.OutputType, ExpectedComponentCount) || ExpectedComponentCount <= 0)
		{
			OutError = FString::Printf(TEXT("MaterialAttributes member '%s' cannot be assigned from Graph code."), *MemberName);
			return false;
		}

		FCodeValue CoercedValue;
		if (!CoerceValueToType(InValue, ExpectedComponentCount, false, CoercedValue, OutError))
		{
			OutError = FString::Printf(
				TEXT("MaterialAttributes member '%s' expects %d component(s). %s"),
				*MemberName,
				ExpectedComponentCount,
				*OutError);
			return false;
		}

		UMaterialExpressionSetMaterialAttributes* SetAttributes = Cast<UMaterialExpressionSetMaterialAttributes>(
			CreateExpression(UMaterialExpressionSetMaterialAttributes::StaticClass(), 320, ConsumeNodeY()));
		if (!SetAttributes)
		{
			OutError = TEXT("Failed to create a SetMaterialAttributes node.");
			return false;
		}

		if (!SetAttributes->ConnectInputAttribute(MP_MaterialAttributes, BaseValue->Expression, BaseValue->OutputIndex))
		{
			OutError = FString::Printf(TEXT("Failed to connect '%s' as the SetMaterialAttributes base value."), *BaseName);
			return false;
		}

		if (!SetAttributes->ConnectInputAttribute(ResolvedProperty.Property, CoercedValue.Expression, CoercedValue.OutputIndex))
		{
			OutError = FString::Printf(TEXT("Failed to connect MaterialAttributes member '%s'."), *MemberName);
			return false;
		}

		BaseValue->Expression = SetAttributes;
		BaseValue->OutputIndex = 0;
		BaseValue->ComponentCount = 0;
		BaseValue->bIsTextureObject = false;
		BaseValue->bIsMaterialAttributes = true;

		return true;
	}

	bool FCodeGraphBuilder::TryFlattenQualifiedName(const TSharedPtr<FCodeExpression>& Expression, FString& OutName)
	{
		if (!Expression)
		{
			return false;
		}

		if (Expression->Kind == ECodeExpressionKind::Name)
		{
			OutName = Expression->Text;
			return true;
		}

		if (Expression->Kind == ECodeExpressionKind::MemberAccess)
		{
			FString LeftName;
			if (!TryFlattenQualifiedName(Expression->Left, LeftName))
			{
				return false;
			}

			OutName = Expression->Text.StartsWith(TEXT("::"))
				? LeftName + Expression->Text
				: LeftName + TEXT(".") + Expression->Text;
			return true;
		}

		return false;
	}

	bool FCodeGraphBuilder::TryExtractTextLiteral(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const
	{
		if (!Expression)
		{
			return false;
		}

		if (Expression->Kind == ECodeExpressionKind::StringLiteral)
		{
			OutText = Expression->Text;
			return true;
		}

		if (TryFlattenQualifiedName(Expression, OutText))
		{
			return true;
		}

		return false;
	}

	bool FCodeGraphBuilder::TryExtractLiteralText(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const
	{
		if (!Expression)
		{
			return false;
		}

		if (Expression->Kind == ECodeExpressionKind::NumberLiteral)
		{
			OutText = Expression->Text;
			return true;
		}

		if (Expression->Kind == ECodeExpressionKind::Unary
			&& (Expression->Text == TEXT("+") || Expression->Text == TEXT("-")))
		{
			FString InnerText;
			if (!TryExtractLiteralText(Expression->Left, InnerText))
			{
				return false;
			}

			OutText = Expression->Text + InnerText;
			return true;
		}

		return TryExtractTextLiteral(Expression, OutText);
	}

	bool FCodeGraphBuilder::TryExtractAssetReferenceText(const TSharedPtr<FCodeExpression>& Expression, FString& OutText) const
	{
		if (!Expression)
		{
			return false;
		}

		if (TryExtractLiteralText(Expression, OutText))
		{
			return true;
		}

		if (Expression->Kind != ECodeExpressionKind::Call)
		{
			return false;
		}

		FString CalleeName;
		if (!TryFlattenQualifiedName(Expression->Left, CalleeName)
			|| !CalleeName.Equals(TEXT("Path"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		TArray<FString> Parts;
		for (const FCodeCallArgument& Argument : Expression->Arguments)
		{
			if (Argument.bIsNamed)
			{
				return false;
			}

			if (Argument.Expression && Argument.Expression->Kind == ECodeExpressionKind::StringLiteral)
			{
				FString Escaped = Argument.Expression->Text;
				Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
				Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
				Parts.Add(FString::Printf(TEXT("\"%s\""), *Escaped));
				continue;
			}

			FString LiteralText;
			if (!TryExtractLiteralText(Argument.Expression, LiteralText))
			{
				return false;
			}
			Parts.Add(LiteralText);
		}

		OutText = FString::Printf(TEXT("Path(%s)"), *FString::Join(Parts, TEXT(", ")));
		return true;
	}

	bool FCodeGraphBuilder::TryExtractScalarLiteral(const TSharedPtr<FCodeExpression>& Expression, double& OutValue) const
	{
		if (!Expression)
		{
			return false;
		}

		if (Expression->Kind == ECodeExpressionKind::NumberLiteral)
		{
			return ParseScalarLiteral(Expression->Text, OutValue);
		}

		if (Expression->Kind == ECodeExpressionKind::Unary
			&& (Expression->Text == TEXT("+") || Expression->Text == TEXT("-")))
		{
			double InnerValue = 0.0;
			if (!TryExtractScalarLiteral(Expression->Left, InnerValue))
			{
				return false;
			}

			OutValue = (Expression->Text == TEXT("-")) ? -InnerValue : InnerValue;
			return true;
		}

		FString TextValue;
		return TryExtractTextLiteral(Expression, TextValue) && ParseScalarLiteral(TextValue, OutValue);
	}

	bool FCodeGraphBuilder::TryExtractIntegerLiteral(const TSharedPtr<FCodeExpression>& Expression, int32& OutValue) const
	{
		if (!Expression)
		{
			return false;
		}

		if (Expression->Kind == ECodeExpressionKind::NumberLiteral)
		{
			return ParseIntegerLiteral(Expression->Text, OutValue);
		}

		if (Expression->Kind == ECodeExpressionKind::Unary
			&& (Expression->Text == TEXT("+") || Expression->Text == TEXT("-")))
		{
			int32 InnerValue = 0;
			if (!TryExtractIntegerLiteral(Expression->Left, InnerValue))
			{
				return false;
			}

			OutValue = (Expression->Text == TEXT("-")) ? -InnerValue : InnerValue;
			return true;
		}

		FString TextValue;
		return TryExtractTextLiteral(Expression, TextValue) && ParseIntegerLiteral(TextValue, OutValue);
	}

	bool FCodeGraphBuilder::TryExtractBooleanLiteral(const TSharedPtr<FCodeExpression>& Expression, bool& OutValue) const
	{
		if (!Expression)
		{
			return false;
		}

		FString TextValue;
		return TryExtractTextLiteral(Expression, TextValue) && ParseBooleanLiteral(TextValue, OutValue);
	}

	bool FCodeGraphBuilder::IsDefaultArgument(const TSharedPtr<FCodeExpression>& Expression)
	{
		FString Name;
		return TryFlattenQualifiedName(Expression, Name) && Name.Equals(TEXT("default"), ESearchCase::IgnoreCase);
	}

	const FCodeCallArgument* FCodeGraphBuilder::FindNamedArgument(const TArray<FCodeCallArgument>& Arguments, const TCHAR* Name) const
	{
		const FString Normalized = UE::DreamShader::NormalizeSettingKey(Name);
		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed && UE::DreamShader::NormalizeSettingKey(Argument.Name) == Normalized)
			{
				return &Argument;
			}
		}

		return nullptr;
	}

	const FCodeCallArgument* FCodeGraphBuilder::FindPositionalArgument(const TArray<FCodeCallArgument>& Arguments, const int32 PositionIndex) const
	{
		int32 CurrentIndex = 0;
		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (!Argument.bIsNamed)
			{
				if (CurrentIndex == PositionIndex)
				{
					return &Argument;
				}
				++CurrentIndex;
			}
		}

		return nullptr;
	}

	bool FCodeGraphBuilder::ExecuteExpressionStatement(const TSharedPtr<FCodeExpression>& Expression, FString& OutError)
	{
		if (!Expression || Expression->Kind != ECodeExpressionKind::Call)
		{
			OutError = TEXT("Graph expression statements currently support only Function calls with explicit out arguments.");
			return false;
		}

		FString CalleeName;
		if (!TryFlattenQualifiedName(Expression->Left, CalleeName))
		{
			OutError = TEXT("Graph expression statements must call a named Function.");
			return false;
		}

		const FTextShaderFunctionDefinition* Function = FindFunctionDefinition(CalleeName);
		if (!Function)
		{
			OutError = FString::Printf(
				TEXT("Graph expression statement '%s' is unsupported. Only DreamShader Function calls may use statement syntax."),
				*CalleeName);
			return false;
		}

		return ExecuteCustomFunctionCall(*Function, Expression->Arguments, OutError);
	}

	bool FCodeGraphBuilder::EvaluateExpression(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		if (!Expression)
		{
			OutError = TEXT("Empty Graph expression.");
			return false;
		}

		switch (Expression->Kind)
		{
		case ECodeExpressionKind::Name:
		{
			if (FCodeValue* ExistingValue = FindValue(Expression->Text))
			{
				OutValue = *ExistingValue;
				return true;
			}

			OutError = FString::Printf(TEXT("Unknown Graph identifier '%s'."), *Expression->Text);
			return false;
		}

		case ECodeExpressionKind::NumberLiteral:
		{
			double ParsedValue = 0.0;
			if (!ParseScalarLiteral(Expression->Text, ParsedValue))
			{
				OutError = FString::Printf(TEXT("Invalid numeric literal '%s'."), *Expression->Text);
				return false;
			}

			OutValue.Expression = CreateScalarLiteralNode(ParsedValue, ConsumeNodeY());
			OutValue.ComponentCount = 1;
			return OutValue.Expression != nullptr;
		}

		case ECodeExpressionKind::StringLiteral:
			OutError = TEXT("String literals can only be used in named UE builtin arguments.");
			return false;

		case ECodeExpressionKind::Unary:
			return EvaluateUnary(Expression, OutValue, OutError);

		case ECodeExpressionKind::Binary:
			return EvaluateBinary(Expression, OutValue, OutError);

		case ECodeExpressionKind::MemberAccess:
			return EvaluateMemberAccess(Expression, OutValue, OutError);

		case ECodeExpressionKind::Call:
			return EvaluateCall(Expression, OutValue, OutError);

		default:
			OutError = TEXT("Unsupported Graph expression kind.");
			return false;
		}
	}

	bool FCodeGraphBuilder::EvaluateUnary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FCodeValue Operand;
		if (!EvaluateExpression(Expression->Left, Operand, OutError))
		{
			return false;
		}

		if (Expression->Text == TEXT("+"))
		{
			OutValue = Operand;
			return true;
		}

		if (Expression->Text == TEXT("-"))
		{
			FCodeValue MinusOne;
			MinusOne.Expression = CreateScalarLiteralNode(-1.0, ConsumeNodeY());
			MinusOne.ComponentCount = 1;
			return CreateBinaryOperatorNode(TEXT("*"), Operand, MinusOne, OutValue, OutError);
		}

		OutError = FString::Printf(TEXT("Unsupported unary operator '%s'."), *Expression->Text);
		return false;
	}

	bool FCodeGraphBuilder::EvaluateBinary(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FCodeValue LeftValue;
		FCodeValue RightValue;
		if (!EvaluateExpression(Expression->Left, LeftValue, OutError)
			|| !EvaluateExpression(Expression->Right, RightValue, OutError))
		{
			return false;
		}

		return CreateBinaryOperatorNode(Expression->Text, LeftValue, RightValue, OutValue, OutError);
	}

	bool FCodeGraphBuilder::CreateBinaryOperatorNode(
		const FString& Operator,
		const FCodeValue& LeftValue,
		const FCodeValue& RightValue,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (LeftValue.bIsTextureObject || RightValue.bIsTextureObject)
		{
			OutError = TEXT("Arithmetic operators cannot be applied to Texture2D values.");
			return false;
		}
		if (LeftValue.bIsMaterialAttributes || RightValue.bIsMaterialAttributes)
		{
			OutError = TEXT("Arithmetic operators cannot be applied to MaterialAttributes values.");
			return false;
		}

		UMaterialExpression* Expression = nullptr;
		const int32 PositionY = ConsumeNodeY();

		if (Operator == TEXT("+"))
		{
			auto* AddExpression = Cast<UMaterialExpressionAdd>(
				CreateExpression(UMaterialExpressionAdd::StaticClass(), 200, PositionY));
			if (AddExpression)
			{
				ConnectCodeValueToInput(AddExpression->A, LeftValue);
				ConnectCodeValueToInput(AddExpression->B, RightValue);
				Expression = AddExpression;
			}
		}
		else if (Operator == TEXT("-"))
		{
			auto* SubtractExpression = Cast<UMaterialExpressionSubtract>(
				CreateExpression(UMaterialExpressionSubtract::StaticClass(), 200, PositionY));
			if (SubtractExpression)
			{
				ConnectCodeValueToInput(SubtractExpression->A, LeftValue);
				ConnectCodeValueToInput(SubtractExpression->B, RightValue);
				Expression = SubtractExpression;
			}
		}
		else if (Operator == TEXT("*"))
		{
			auto* MultiplyExpression = Cast<UMaterialExpressionMultiply>(
				CreateExpression(UMaterialExpressionMultiply::StaticClass(), 200, PositionY));
			if (MultiplyExpression)
			{
				ConnectCodeValueToInput(MultiplyExpression->A, LeftValue);
				ConnectCodeValueToInput(MultiplyExpression->B, RightValue);
				Expression = MultiplyExpression;
			}
		}
		else if (Operator == TEXT("/"))
		{
			auto* DivideExpression = Cast<UMaterialExpressionDivide>(
				CreateExpression(UMaterialExpressionDivide::StaticClass(), 200, PositionY));
			if (DivideExpression)
			{
				ConnectCodeValueToInput(DivideExpression->A, LeftValue);
				ConnectCodeValueToInput(DivideExpression->B, RightValue);
				Expression = DivideExpression;
			}
		}

		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Unsupported or failed binary operator '%s'."), *Operator);
			return false;
		}

		OutValue.Expression = Expression;
		OutValue.ComponentCount = FMath::Max(LeftValue.ComponentCount, RightValue.ComponentCount);
		return true;
	}

	bool FCodeGraphBuilder::EvaluateMemberAccess(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FCodeValue BaseValue;
		if (!EvaluateExpression(Expression->Left, BaseValue, OutError))
		{
			return false;
		}

		if (BaseValue.bIsMaterialAttributes)
		{
			FResolvedMaterialProperty ResolvedProperty;
			if (!ResolveMaterialProperty(Expression->Text, ResolvedProperty)
				|| ResolvedProperty.Property == MP_MaterialAttributes)
			{
				OutError = FString::Printf(TEXT("Unsupported MaterialAttributes member '%s'."), *Expression->Text);
				return false;
			}

			int32 OutputComponents = 0;
			if (!TryGetComponentCountForOutputType(ResolvedProperty.OutputType, OutputComponents) || OutputComponents <= 0)
			{
				OutError = FString::Printf(TEXT("MaterialAttributes member '%s' cannot be read as a numeric value."), *Expression->Text);
				return false;
			}

			auto* BreakAttributes = Cast<UMaterialExpressionBreakMaterialAttributes>(
				CreateExpression(UMaterialExpressionBreakMaterialAttributes::StaticClass(), 420, ConsumeNodeY()));
			if (!BreakAttributes)
			{
				OutError = TEXT("Failed to create a BreakMaterialAttributes node.");
				return false;
			}

			ConnectCodeValueToInput(BreakAttributes->MaterialAttributes, BaseValue);
			int32 OutputIndex = INDEX_NONE;
			if (!TryResolveMaterialAttributesBreakOutputIndex(ResolvedProperty.Property, OutputIndex)
				|| !BreakAttributes->Outputs.IsValidIndex(OutputIndex))
			{
				OutError = FString::Printf(TEXT("BreakMaterialAttributes does not expose member '%s'."), *Expression->Text);
				return false;
			}

			OutValue.Expression = BreakAttributes;
			OutValue.OutputIndex = OutputIndex;
			OutValue.ComponentCount = OutputComponents;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			return true;
		}

		if (BaseValue.bIsTextureObject)
		{
			OutError = TEXT("Texture2D values do not support swizzle/member access in Code.");
			return false;
		}

		return CreateSwizzleExpression(BaseValue, Expression->Text, OutValue, OutError);
	}

	bool FCodeGraphBuilder::CreateSingleChannelMask(
		const FCodeValue& BaseValue,
		const int32 ChannelIndex,
		FCodeValue& OutValue,
		FString& OutError)
	{
		auto* MaskExpression = Cast<UMaterialExpressionComponentMask>(
			CreateExpression(UMaterialExpressionComponentMask::StaticClass(), 400, ConsumeNodeY()));
		if (!MaskExpression)
		{
			OutError = TEXT("Failed to create a ComponentMask node.");
			return false;
		}

		ConnectCodeValueToInput(MaskExpression->Input, BaseValue);
		MaskExpression->R = ChannelIndex == 0;
		MaskExpression->G = ChannelIndex == 1;
		MaskExpression->B = ChannelIndex == 2;
		MaskExpression->A = ChannelIndex == 3;

		OutValue.Expression = MaskExpression;
		OutValue.ComponentCount = 1;
		return true;
	}

	bool FCodeGraphBuilder::AppendValues(const TArray<FCodeValue>& Parts, FCodeValue& OutValue, FString& OutError)
	{
		if (Parts.IsEmpty())
		{
			OutError = TEXT("Cannot build an empty vector.");
			return false;
		}
		for (const FCodeValue& Part : Parts)
		{
			if (Part.bIsTextureObject || Part.bIsMaterialAttributes)
			{
				OutError = TEXT("AppendVector inputs must be numeric scalar/vector values.");
				return false;
			}
		}

		FCodeValue Current = Parts[0];
		for (int32 Index = 1; Index < Parts.Num(); ++Index)
		{
			auto* AppendExpression = Cast<UMaterialExpressionAppendVector>(
				CreateExpression(UMaterialExpressionAppendVector::StaticClass(), 420, ConsumeNodeY()));
			if (!AppendExpression)
			{
				OutError = TEXT("Failed to create an AppendVector node.");
				return false;
			}

			ConnectCodeValueToInput(AppendExpression->A, Current);
			ConnectCodeValueToInput(AppendExpression->B, Parts[Index]);

			Current.Expression = AppendExpression;
			Current.ComponentCount += Parts[Index].ComponentCount;
		}

		OutValue = Current;
		return true;
	}

	bool FCodeGraphBuilder::CreateSwizzleExpression(
		const FCodeValue& BaseValue,
		const FString& Swizzle,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (Swizzle.IsEmpty() || Swizzle.Len() > 4)
		{
			OutError = FString::Printf(TEXT("Unsupported swizzle '%s'."), *Swizzle);
			return false;
		}

		TArray<FCodeValue> Channels;
		for (int32 Index = 0; Index < Swizzle.Len(); ++Index)
		{
			const TCHAR ChannelChar = FChar::ToLower(Swizzle[Index]);
			int32 ChannelIndex = INDEX_NONE;
			switch (ChannelChar)
			{
			case TCHAR('x'):
			case TCHAR('r'):
				ChannelIndex = 0;
				break;
			case TCHAR('y'):
			case TCHAR('g'):
				ChannelIndex = 1;
				break;
			case TCHAR('z'):
			case TCHAR('b'):
				ChannelIndex = 2;
				break;
			case TCHAR('w'):
			case TCHAR('a'):
				ChannelIndex = 3;
				break;
			default:
				break;
			}

			if (ChannelIndex == INDEX_NONE || ChannelIndex >= BaseValue.ComponentCount)
			{
				OutError = FString::Printf(TEXT("Swizzle '%s' is invalid for a value with %d components."), *Swizzle, BaseValue.ComponentCount);
				return false;
			}

			FCodeValue ChannelValue;
			if (!CreateSingleChannelMask(BaseValue, ChannelIndex, ChannelValue, OutError))
			{
				return false;
			}
			Channels.Add(ChannelValue);
		}

		if (Channels.Num() == 1)
		{
			OutValue = Channels[0];
			return true;
		}

		if (!AppendValues(Channels, OutValue, OutError))
		{
			return false;
		}

		OutValue.ComponentCount = Channels.Num();
		return true;
	}

	bool FCodeGraphBuilder::EvaluateCall(const TSharedPtr<FCodeExpression>& Expression, FCodeValue& OutValue, FString& OutError)
	{
		FString CalleeName;
		if (!TryFlattenQualifiedName(Expression->Left, CalleeName))
		{
			OutError = TEXT("Graph calls must target a named function.");
			return false;
		}

		if (IsVectorConstructorName(CalleeName))
		{
			return EvaluateVectorConstructor(CalleeName, Expression->Arguments, OutValue, OutError);
		}

		if (CalleeName.StartsWith(TEXT("UE."), ESearchCase::IgnoreCase))
		{
			return EvaluateUEBuiltinCall(CalleeName, Expression->Arguments, OutValue, OutError);
		}

		if (const FTextShaderPropertyDefinition* Property = FindPropertyDefinition(CalleeName))
		{
			if (Property->ParameterNodeType.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
			{
				return EvaluateStaticSwitchParameterCall(*Property, Expression->Arguments, OutValue, OutError);
			}
		}

		const FTextShaderFunctionDefinition* CustomFunction = FindFunctionDefinition(CalleeName);
		const FTextShaderMaterialFunctionDefinition* MaterialFunctionDefinition = FindMaterialFunctionDefinition(CalleeName);
		const FTextShaderVirtualFunctionDefinition* VirtualFunctionDefinition = FindVirtualFunctionDefinition(CalleeName);

		TArray<FString> MatchedKinds;
		if (CustomFunction)
		{
			MatchedKinds.Add(TEXT("Function"));
		}
		if (MaterialFunctionDefinition)
		{
			MatchedKinds.Add(TEXT("ShaderFunction"));
		}
		if (VirtualFunctionDefinition)
		{
			MatchedKinds.Add(TEXT("VirtualFunction"));
		}
		if (MatchedKinds.Num() > 1)
		{
			OutError = FString::Printf(
				TEXT("Graph call '%s' is ambiguous because multiple definitions use that name: %s."),
				*CalleeName,
				*FString::Join(MatchedKinds, TEXT(", ")));
			return false;
		}

		if (MaterialFunctionDefinition)
		{
			return EvaluateMaterialFunctionCall(*MaterialFunctionDefinition, Expression->Arguments, OutValue, OutError);
		}
		if (VirtualFunctionDefinition)
		{
			return EvaluateVirtualFunctionCall(*VirtualFunctionDefinition, Expression->Arguments, OutValue, OutError);
		}

		return EvaluateCustomFunctionCall(CalleeName, Expression->Arguments, OutValue, OutError);
	}

	bool FCodeGraphBuilder::IsVectorConstructorName(const FString& InName)
	{
		return InName.Equals(TEXT("float"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("float1"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("float2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("float3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("float4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("vec2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("vec3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("vec4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("half"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("half1"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("half2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("half3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("half4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("int"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("int2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("int3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("int4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("ivec2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("ivec3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("ivec4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("uint"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("uint2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("uint3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("uint4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("uvec2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("uvec3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("uvec4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("bool"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("bool2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("bool3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("bool4"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("bvec2"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("bvec3"), ESearchCase::IgnoreCase)
			|| InName.Equals(TEXT("bvec4"), ESearchCase::IgnoreCase);
	}

	int32 FCodeGraphBuilder::GetConstructorComponentCount(const FString& InName)
	{
		if (InName.EndsWith(TEXT("2")))
		{
			return 2;
		}
		if (InName.EndsWith(TEXT("3")))
		{
			return 3;
		}
		if (InName.EndsWith(TEXT("4")))
		{
			return 4;
		}
		return 1;
	}

	bool FCodeGraphBuilder::EvaluateVectorConstructor(
		const FString& ConstructorName,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const int32 ExpectedComponents = GetConstructorComponentCount(ConstructorName);
		TArray<FCodeValue> Parts;

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' does not accept named arguments."), *ConstructorName);
				return false;
			}

			FCodeValue EvaluatedArgument;
			if (!EvaluateExpression(Argument.Expression, EvaluatedArgument, OutError))
			{
				return false;
			}

			if (EvaluatedArgument.bIsTextureObject)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' cannot use Texture2D arguments."), *ConstructorName);
				return false;
			}
			if (EvaluatedArgument.bIsMaterialAttributes)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' cannot use MaterialAttributes arguments."), *ConstructorName);
				return false;
			}

			Parts.Add(EvaluatedArgument);
		}

		if (ExpectedComponents == 1)
		{
			if (Parts.Num() != 1 || Parts[0].ComponentCount != 1)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' expects a single scalar input."), *ConstructorName);
				return false;
			}

			OutValue = Parts[0];
			return true;
		}

		if (Parts.Num() == 1 && Parts[0].ComponentCount == 1)
		{
			TArray<FCodeValue> ReplicatedParts;
			for (int32 Index = 0; Index < ExpectedComponents; ++Index)
			{
				ReplicatedParts.Add(Parts[0]);
			}
			if (!AppendValues(ReplicatedParts, OutValue, OutError))
			{
				return false;
			}
			OutValue.ComponentCount = ExpectedComponents;
			return true;
		}

		int32 TotalComponents = 0;
		for (const FCodeValue& Part : Parts)
		{
			TotalComponents += Part.ComponentCount;
		}

		if (TotalComponents != ExpectedComponents)
		{
			OutError = FString::Printf(TEXT("Constructor '%s' expects %d total components but got %d."), *ConstructorName, ExpectedComponents, TotalComponents);
			return false;
		}

		if (!AppendValues(Parts, OutValue, OutError))
		{
			return false;
		}

		OutValue.ComponentCount = ExpectedComponents;
		return true;
	}

	const FTextShaderPropertyDefinition* FCodeGraphBuilder::FindPropertyDefinition(const FString& PropertyName) const
	{
		if (LocalProperties)
		{
			for (const FTextShaderPropertyDefinition& Property : *LocalProperties)
			{
				if (Property.Name.Equals(PropertyName, ESearchCase::IgnoreCase))
				{
					return &Property;
				}
			}
		}

		for (const FTextShaderPropertyDefinition& Property : Definition.Properties)
		{
			if (Property.Name.Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				return &Property;
			}
		}

		return nullptr;
	}

	bool FCodeGraphBuilder::EvaluateStaticSwitchParameterCall(
		const FTextShaderPropertyDefinition& Property,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const FCodeCallArgument* TrueArgument = FindNamedArgument(Arguments, TEXT("True"));
		if (!TrueArgument)
		{
			TrueArgument = FindNamedArgument(Arguments, TEXT("A"));
		}
		if (!TrueArgument)
		{
			TrueArgument = FindPositionalArgument(Arguments, 0);
		}

		const FCodeCallArgument* FalseArgument = FindNamedArgument(Arguments, TEXT("False"));
		if (!FalseArgument)
		{
			FalseArgument = FindNamedArgument(Arguments, TEXT("B"));
		}
		if (!FalseArgument)
		{
			FalseArgument = FindPositionalArgument(Arguments, 1);
		}

		if (!TrueArgument || !FalseArgument)
		{
			OutError = FString::Printf(TEXT("StaticSwitchParameter '%s' requires True=... and False=... inputs."), *Property.Name);
			return false;
		}

		FCodeValue TrueValue;
		if (!EvaluateExpression(TrueArgument->Expression, TrueValue, OutError))
		{
			OutError = FString::Printf(TEXT("StaticSwitchParameter '%s' True input: %s"), *Property.Name, *OutError);
			return false;
		}

		FCodeValue FalseValue;
		if (!EvaluateExpression(FalseArgument->Expression, FalseValue, OutError))
		{
			OutError = FString::Printf(TEXT("StaticSwitchParameter '%s' False input: %s"), *Property.Name, *OutError);
			return false;
		}

		if (TrueValue.bIsTextureObject || FalseValue.bIsTextureObject)
		{
			OutError = FString::Printf(TEXT("StaticSwitchParameter '%s' cannot switch Texture object values."), *Property.Name);
			return false;
		}
		if (TrueValue.bIsMaterialAttributes != FalseValue.bIsMaterialAttributes)
		{
			OutError = FString::Printf(TEXT("StaticSwitchParameter '%s' cannot mix MaterialAttributes and numeric branches."), *Property.Name);
			return false;
		}
		if (TrueValue.ComponentCount != FalseValue.ComponentCount)
		{
			OutError = FString::Printf(
				TEXT("StaticSwitchParameter '%s' branches must have the same component count, got %d and %d."),
				*Property.Name,
				TrueValue.ComponentCount,
				FalseValue.ComponentCount);
			return false;
		}

		UMaterialExpressionStaticSwitchParameter* SwitchExpression = nullptr;
		if (FCodeValue* ExistingValue = FindValue(Property.Name))
		{
			SwitchExpression = Cast<UMaterialExpressionStaticSwitchParameter>(ExistingValue->Expression);
		}

		if (!SwitchExpression)
		{
			SwitchExpression = Cast<UMaterialExpressionStaticSwitchParameter>(
				CreateExpression(UMaterialExpressionStaticSwitchParameter::StaticClass(), 600, ConsumeNodeY()));
		}

		if (!SwitchExpression)
		{
			OutError = FString::Printf(TEXT("Failed to create StaticSwitchParameter node '%s'."), *Property.Name);
			return false;
		}

		SwitchExpression->ParameterName = FName(*Property.Name);
		SwitchExpression->DefaultValue = Property.ScalarDefaultValue != 0.0 ? 1U : 0U;
		if (!SwitchExpression->ExpressionGUID.IsValid())
		{
			SwitchExpression->ExpressionGUID = FGuid::NewGuid();
		}
		if (!ApplyExpressionMetadata(SwitchExpression, Property.Metadata, OutError))
		{
			OutError = FString::Printf(TEXT("StaticSwitchParameter '%s': %s"), *Property.Name, *OutError);
			return false;
		}

		ConnectCodeValueToInput(SwitchExpression->A, TrueValue);
		ConnectCodeValueToInput(SwitchExpression->B, FalseValue);

		OutValue.Expression = SwitchExpression;
		OutValue.OutputIndex = 0;
		OutValue.ComponentCount = TrueValue.ComponentCount;
		OutValue.bIsTextureObject = false;
		OutValue.bIsMaterialAttributes = TrueValue.bIsMaterialAttributes;
		return true;
	}

	const FTextShaderFunctionDefinition* FCodeGraphBuilder::FindFunctionDefinition(const FString& FunctionName) const
	{
		for (const FTextShaderFunctionDefinition& Function : Definition.Functions)
		{
			if (Function.Name.Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				return &Function;
			}
		}

		return nullptr;
	}

	const FTextShaderMaterialFunctionDefinition* FCodeGraphBuilder::FindMaterialFunctionDefinition(const FString& FunctionName) const
	{
		for (const FTextShaderMaterialFunctionDefinition& Function : Definition.MaterialFunctions)
		{
			if (Function.Name.Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				return &Function;
			}

			FString ShortName = Function.Name;
			ShortName.ReplaceInline(TEXT("\\"), TEXT("/"));
			const int32 SlashIndex = ShortName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (SlashIndex != INDEX_NONE)
			{
				ShortName.RightChopInline(SlashIndex + 1, EAllowShrinking::No);
			}

			if (ShortName.Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				return &Function;
			}
		}

		return nullptr;
	}

	const FTextShaderVirtualFunctionDefinition* FCodeGraphBuilder::FindVirtualFunctionDefinition(const FString& FunctionName) const
	{
		for (const FTextShaderVirtualFunctionDefinition& Function : Definition.VirtualFunctions)
		{
			if (Function.Name.Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				return &Function;
			}

			FString ShortName = Function.Name;
			ShortName.ReplaceInline(TEXT("\\"), TEXT("/"));
			const int32 SlashIndex = ShortName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (SlashIndex != INDEX_NONE)
			{
				ShortName.RightChopInline(SlashIndex + 1, EAllowShrinking::No);
			}

			if (ShortName.Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				return &Function;
			}
		}

		return nullptr;
	}

	bool FCodeGraphBuilder::EvaluateCustomFunctionCall(
		const FString& FunctionName,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const FTextShaderFunctionDefinition* Function = FindFunctionDefinition(FunctionName);
		if (!Function)
		{
			OutError = FString::Printf(TEXT("Unknown Graph function '%s'."), *FunctionName);
			return false;
		}

		(void)Arguments;
		(void)OutValue;
		OutError = FString::Printf(
			TEXT("DreamShader Function '%s' must be called with explicit out variables, for example %s(..., Result);"),
			*FunctionName,
			*FunctionName);
		return false;
	}

	bool FCodeGraphBuilder::ExecuteCustomFunctionCall(
		const FTextShaderFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FString& OutError)
	{
		if (Function.Results.IsEmpty())
		{
			OutError = FString::Printf(TEXT("DreamShader Function '%s' must declare at least one out result."), *Function.Name);
			return false;
		}

		const int32 ExpectedArgumentCount = Function.Inputs.Num() + Function.Results.Num();
		if (Arguments.Num() != ExpectedArgumentCount)
		{
			OutError = FString::Printf(
				TEXT("DreamShader Function '%s' expects %d arguments (%d inputs, %d out targets) but got %d."),
				*Function.Name,
				ExpectedArgumentCount,
				Function.Inputs.Num(),
				Function.Results.Num(),
				Arguments.Num());
			return false;
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed)
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' currently uses positional arguments only."), *Function.Name);
				return false;
			}
		}

		ECustomMaterialOutputType PrimaryOutputType = CMOT_Float1;
		if (!TryResolveCustomOutputType(Function.Results[0].Type, PrimaryOutputType))
		{
			OutError = FString::Printf(TEXT("DreamShader Function '%s' has unsupported result type '%s'."), *Function.Name, *Function.Results[0].Type);
			return false;
		}

		int32 PrimaryOutputComponents = 1;
		verify(TryGetComponentCountForOutputType(PrimaryOutputType, PrimaryOutputComponents));

		TArray<FCodeValue> InputValues;
		InputValues.Reserve(Function.Inputs.Num());
		for (int32 InputIndex = 0; InputIndex < Function.Inputs.Num(); ++InputIndex)
		{
			FCodeValue InputValue;
			if (!EvaluateExpression(Arguments[InputIndex].Expression, InputValue, OutError))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s': %s"), *Function.Name, *Function.Inputs[InputIndex].Name, *OutError);
				return false;
			}
			InputValues.Add(InputValue);
		}

		TArray<FString> ResultTargetNames;
		ResultTargetNames.Reserve(Function.Results.Num());
		TSet<FString> SeenTargetNames;
		for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			const FCodeCallArgument& Argument = Arguments[Function.Inputs.Num() + ResultIndex];
			if (!Argument.Expression || Argument.Expression->Kind != ECodeExpressionKind::Name)
			{
				OutError = FString::Printf(
					TEXT("DreamShader Function '%s' out argument %d must be a plain variable name."),
					*Function.Name,
					ResultIndex + 1);
				return false;
			}

			const FString TargetName = Argument.Expression->Text.TrimStartAndEnd();
			if (TargetName.IsEmpty())
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' has an empty out target name."), *Function.Name);
				return false;
			}

			if (SeenTargetNames.Contains(TargetName))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' cannot write multiple out results into '%s' in the same call."), *Function.Name, *TargetName);
				return false;
			}

			SeenTargetNames.Add(TargetName);
			ResultTargetNames.Add(TargetName);
		}

		auto* CustomExpression = Cast<UMaterialExpressionCustom>(
			CreateExpression(UMaterialExpressionCustom::StaticClass(), 600, ConsumeNodeY()));
		if (!CustomExpression)
		{
			OutError = FString::Printf(TEXT("Failed to create a Custom node for DreamShader Function '%s'."), *Function.Name);
			return false;
		}

		CustomExpression->Description = Function.Name;
		CustomExpression->OutputType = PrimaryOutputType;
		CustomExpression->ShowCode = true;
		CustomExpression->Inputs.Reset();
		CustomExpression->AdditionalOutputs.Reset();
		CustomExpression->IncludeFilePaths.Reset();

		for (int32 InputIndex = 0; InputIndex < Function.Inputs.Num(); ++InputIndex)
		{
			FCustomInput Input;
			Input.InputName = FName(*Function.Inputs[InputIndex].Name);
			CustomExpression->Inputs.Add(Input);
			ConnectCodeValueToInput(CustomExpression->Inputs.Last().Input, InputValues[InputIndex]);
		}

		TArray<FString> ResultVariableNames;
		ResultVariableNames.Reserve(Function.Results.Num());

		FString CustomCode;
		for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			const FString TempName = FString::Printf(
				TEXT("__ds_%s_out%d"),
				*UE::DreamShader::SanitizeIdentifier(Function.Name),
				ResultIndex);
			ResultVariableNames.Add(TempName);
			CustomCode += FString::Printf(
				TEXT("%s %s = (%s)0;\n"),
				*Function.Results[ResultIndex].Type,
				*TempName,
				*Function.Results[ResultIndex].Type);
		}

		TArray<FString> SecondaryResultVariables;
		for (int32 ResultIndex = 1; ResultIndex < ResultVariableNames.Num(); ++ResultIndex)
		{
			SecondaryResultVariables.Add(ResultVariableNames[ResultIndex]);
		}

		CustomCode += FString::Printf(
			TEXT("%s = %s(%s);\n"),
			*ResultVariableNames[0],
			*BuildGeneratedFunctionSymbolName(Function),
			*BuildFunctionArgumentList(Function, SecondaryResultVariables));

		for (int32 ResultIndex = 1; ResultIndex < ResultVariableNames.Num(); ++ResultIndex)
		{
			CustomCode += FString::Printf(
				TEXT("%s = %s;\n"),
				*ResultTargetNames[ResultIndex],
				*ResultVariableNames[ResultIndex]);
		}

		CustomCode += FString::Printf(TEXT("return %s;"), *ResultVariableNames[0]);

		if (Function.bSelfContained)
		{
			TArray<FString> EmbeddedFunctionNames;
			EmbeddedFunctionNames.Add(Function.Name);

			FString PreparedCustomCode;
			bool bUsesGeneratedInclude = false;
			if (!PrepareCustomNodeCode(
				Definition,
				CustomCode,
				EmbeddedFunctionNames,
				Function.Name,
				PreparedCustomCode,
				bUsesGeneratedInclude,
				OutError))
			{
				return false;
			}

			CustomExpression->Code = PreparedCustomCode;
			if (bUsesGeneratedInclude)
			{
				CustomExpression->IncludeFilePaths.Add(IncludeVirtualPath);
			}
		}
		else
		{
			CustomExpression->Code = CustomCode;
			CustomExpression->IncludeFilePaths.Add(IncludeVirtualPath);
		}

		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			ECustomMaterialOutputType AdditionalOutputType = CMOT_Float1;
			if (!TryResolveCustomOutputType(Function.Results[ResultIndex].Type, AdditionalOutputType))
			{
				OutError = FString::Printf(
					TEXT("DreamShader Function '%s' has unsupported result type '%s'."),
					*Function.Name,
					*Function.Results[ResultIndex].Type);
				return false;
			}

			FCustomOutput Output;
			Output.OutputName = FName(*ResultTargetNames[ResultIndex]);
			Output.OutputType = AdditionalOutputType;
			CustomExpression->AdditionalOutputs.Add(Output);
		}

		CustomExpression->RebuildOutputs();

		for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			ECustomMaterialOutputType ResultOutputType = CMOT_Float1;
			if (!TryResolveCustomOutputType(Function.Results[ResultIndex].Type, ResultOutputType))
			{
				OutError = FString::Printf(
					TEXT("DreamShader Function '%s' has unsupported result type '%s'."),
					*Function.Name,
					*Function.Results[ResultIndex].Type);
				return false;
			}

			int32 ResultComponents = 1;
			verify(TryGetComponentCountForOutputType(ResultOutputType, ResultComponents));

			FCodeValue ResultValue;
			ResultValue.Expression = CustomExpression;
			ResultValue.OutputIndex = ResultIndex;
			ResultValue.ComponentCount = ResultComponents;
			ResultValue.bIsTextureObject = false;
			ResultValue.bIsMaterialAttributes = IsMaterialAttributesComponentType(ResultComponents, false);
			(*Values).Add(ResultTargetNames[ResultIndex], ResultValue);
		}

		return true;
	}

	bool FCodeGraphBuilder::EvaluateMaterialFunctionCall(
		const FTextShaderMaterialFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Function.Name, Function.Root, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		return EvaluateMaterialFunctionCallAsset(
			TEXT("ShaderFunction"),
			Function.Name,
			ObjectPath,
			Function.Inputs,
			Function.Outputs,
			Arguments,
			OutValue,
			OutError);
	}

	bool FCodeGraphBuilder::EvaluateVirtualFunctionCall(
		const FTextShaderVirtualFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		FString ObjectPath;
		if (!TryResolveDreamShaderAssetReference(Function.Asset, ObjectPath, OutError))
		{
			OutError = FString::Printf(TEXT("VirtualFunction '%s' asset reference is invalid: %s"), *Function.Name, *OutError);
			return false;
		}

		return EvaluateMaterialFunctionCallAsset(
			TEXT("VirtualFunction"),
			Function.Name,
			ObjectPath,
			Function.Inputs,
			Function.Outputs,
			Arguments,
			OutValue,
			OutError);
	}

	bool FCodeGraphBuilder::EvaluateMaterialFunctionCallAsset(
		const FString& CallKind,
		const FString& FunctionName,
		const FString& ObjectPath,
		const TArray<FTextShaderFunctionParameter>& Inputs,
		const TArray<FTextShaderFunctionParameter>& Outputs,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (Outputs.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s '%s' must declare at least one output."), *CallKind, *FunctionName);
			return false;
		}

		UMaterialFunction* MaterialFunctionAsset = LoadObject<UMaterialFunction>(nullptr, *ObjectPath);
		if (!MaterialFunctionAsset)
		{
			OutError = FString::Printf(TEXT("%s '%s' could not load MaterialFunction asset '%s'."), *CallKind, *FunctionName, *ObjectPath);
			return false;
		}

		auto* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(
			CreateExpression(UMaterialExpressionMaterialFunctionCall::StaticClass(), 600, ConsumeNodeY()));
		if (!FunctionCall)
		{
			OutError = FString::Printf(TEXT("Failed to create a MaterialFunctionCall node for '%s'."), *FunctionName);
			return false;
		}

		if (!FunctionCall->SetMaterialFunction(MaterialFunctionAsset))
		{
			OutError = FString::Printf(TEXT("Failed to assign material function '%s' to the generated call node."), *FunctionName);
			return false;
		}

		const FCodeCallArgument* OutputNameArgument = FindNamedArgument(Arguments, TEXT("Output"));
		if (!OutputNameArgument)
		{
			OutputNameArgument = FindNamedArgument(Arguments, TEXT("OutputName"));
		}
		const FCodeCallArgument* OutputIndexArgument = FindNamedArgument(Arguments, TEXT("OutputIndex"));
		if (OutputNameArgument && OutputIndexArgument)
		{
			OutError = FString::Printf(TEXT("%s '%s' cannot use OutputName/Output together with OutputIndex."), *CallKind, *FunctionName);
			return false;
		}

		TArray<const FCodeCallArgument*> PositionalArguments;
		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (!Argument.bIsNamed)
			{
				PositionalArguments.Add(&Argument);
			}
		}

		int32 PositionalArgumentIndex = 0;
		for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
		{
			const FTextShaderFunctionParameter& InputDefinition = Inputs[InputIndex];
			const FCodeCallArgument* InputArgument = FindNamedArgument(Arguments, *InputDefinition.Name);
			if (!InputArgument && PositionalArguments.IsValidIndex(PositionalArgumentIndex))
			{
				InputArgument = PositionalArguments[PositionalArgumentIndex++];
			}

			int32 FunctionInputIndex = INDEX_NONE;
			for (int32 CandidateIndex = 0; CandidateIndex < FunctionCall->FunctionInputs.Num(); ++CandidateIndex)
			{
				const FFunctionExpressionInput& CandidateInput = FunctionCall->FunctionInputs[CandidateIndex];
				const FName CandidateName = CandidateInput.ExpressionInput
					? CandidateInput.ExpressionInput->InputName
					: CandidateInput.Input.InputName;
				if (CandidateName.ToString().Equals(InputDefinition.Name, ESearchCase::IgnoreCase))
				{
					FunctionInputIndex = CandidateIndex;
					break;
				}
			}
			if (FunctionInputIndex == INDEX_NONE && FunctionCall->FunctionInputs.IsValidIndex(InputIndex))
			{
				FunctionInputIndex = InputIndex;
			}
			if (!FunctionCall->FunctionInputs.IsValidIndex(FunctionInputIndex))
			{
				OutError = FString::Printf(TEXT("%s '%s' input '%s' does not exist on MaterialFunction asset '%s'."), *CallKind, *FunctionName, *InputDefinition.Name, *ObjectPath);
				return false;
			}

			if (!InputArgument)
			{
				if (InputDefinition.bOptional)
				{
					continue;
				}

				OutError = FString::Printf(TEXT("%s '%s' is missing required input '%s'."), *CallKind, *FunctionName, *InputDefinition.Name);
				return false;
			}

			if (IsDefaultArgument(InputArgument->Expression))
			{
				if (InputDefinition.bOptional)
				{
					continue;
				}

				OutError = FString::Printf(TEXT("%s '%s' input '%s' is not optional and cannot use default."), *CallKind, *FunctionName, *InputDefinition.Name);
				return false;
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(InputArgument->Expression, InputValue, OutError))
			{
				OutError = FString::Printf(TEXT("%s '%s' input '%s': %s"), *CallKind, *FunctionName, *InputDefinition.Name, *OutError);
				return false;
			}

			ConnectCodeValueToInput(FunctionCall->FunctionInputs[FunctionInputIndex].Input, InputValue);
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (!Argument.bIsNamed)
			{
				continue;
			}

			const FString NormalizedName = UE::DreamShader::NormalizeSettingKey(Argument.Name);
			if (NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("Output"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputName"))
				|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputIndex")))
			{
				continue;
			}

			bool bMatchesInput = false;
			for (const FTextShaderFunctionParameter& Input : Inputs)
			{
				if (Input.Name.Equals(Argument.Name, ESearchCase::IgnoreCase))
				{
					bMatchesInput = true;
					break;
				}
			}

			if (!bMatchesInput)
			{
				OutError = FString::Printf(TEXT("%s '%s' does not have an input named '%s'."), *CallKind, *FunctionName, *Argument.Name);
				return false;
			}
		}

		int32 OutputIndex = 0;
		if (OutputIndexArgument)
		{
			if (!TryExtractIntegerLiteral(OutputIndexArgument->Expression, OutputIndex)
				|| OutputIndex < 0
				|| !Outputs.IsValidIndex(OutputIndex))
			{
				OutError = FString::Printf(TEXT("%s '%s' OutputIndex is out of range."), *CallKind, *FunctionName);
				return false;
			}
		}
		else if (OutputNameArgument)
		{
			FString OutputName;
			if (!TryExtractLiteralText(OutputNameArgument->Expression, OutputName))
			{
				OutError = FString::Printf(TEXT("%s '%s' OutputName must be a literal value."), *CallKind, *FunctionName);
				return false;
			}

			OutputIndex = INDEX_NONE;
			for (int32 CandidateIndex = 0; CandidateIndex < Outputs.Num(); ++CandidateIndex)
			{
				if (Outputs[CandidateIndex].Name.Equals(OutputName, ESearchCase::IgnoreCase))
				{
					OutputIndex = CandidateIndex;
					break;
				}
			}

			if (OutputIndex == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("%s '%s' does not expose an output named '%s'."), *CallKind, *FunctionName, *OutputName);
				return false;
			}
		}
		else if (Outputs.Num() != 1)
		{
			OutError = FString::Printf(TEXT("%s '%s' exposes multiple outputs. Specify Output=\"Name\" or OutputIndex=N."), *CallKind, *FunctionName);
			return false;
		}

		int32 OutputComponents = 0;
		bool bIsTextureObject = false;
		if (!TryResolveCodeDeclaredType(Outputs[OutputIndex].Type, OutputComponents, bIsTextureObject))
		{
			OutError = FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name, *Outputs[OutputIndex].Type);
			return false;
		}

		int32 FunctionOutputIndex = INDEX_NONE;
		for (int32 CandidateIndex = 0; CandidateIndex < FunctionCall->FunctionOutputs.Num(); ++CandidateIndex)
		{
			const FFunctionExpressionOutput& CandidateOutput = FunctionCall->FunctionOutputs[CandidateIndex];
			const FName CandidateName = CandidateOutput.ExpressionOutput
				? CandidateOutput.ExpressionOutput->OutputName
				: CandidateOutput.Output.OutputName;
			if (CandidateName.ToString().Equals(Outputs[OutputIndex].Name, ESearchCase::IgnoreCase))
			{
				FunctionOutputIndex = CandidateIndex;
				break;
			}
		}
		if (FunctionOutputIndex == INDEX_NONE && FunctionCall->FunctionOutputs.IsValidIndex(OutputIndex))
		{
			FunctionOutputIndex = OutputIndex;
		}
		if (!FunctionCall->FunctionOutputs.IsValidIndex(FunctionOutputIndex))
		{
			OutError = FString::Printf(TEXT("%s '%s' output '%s' does not exist on MaterialFunction asset '%s'."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name, *ObjectPath);
			return false;
		}

		OutValue.Expression = FunctionCall;
		OutValue.OutputIndex = FunctionOutputIndex;
		OutValue.ComponentCount = OutputComponents;
		OutValue.bIsTextureObject = bIsTextureObject;
		OutValue.bIsMaterialAttributes = IsMaterialAttributesComponentType(OutputComponents, bIsTextureObject);
		return true;
	}

	FString FCodeGraphBuilder::BuildFunctionArgumentList(const FTextShaderFunctionDefinition& Function, const TArray<FString>& ResultVariableNames)
	{
		TArray<FString> Parameters;
		for (const FTextShaderFunctionParameter& Input : Function.Inputs)
		{
			Parameters.Add(Input.Name);
			if (IsTextureFunctionParameterType(Input.Type))
			{
				Parameters.Add(Input.Name + TEXT("Sampler"));
			}
		}
		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			if (ResultVariableNames.IsValidIndex(ResultIndex - 1))
			{
				Parameters.Add(ResultVariableNames[ResultIndex - 1]);
			}
		}
		return FString::Join(Parameters, TEXT(", "));
	}

	bool FCodeGraphBuilder::TryResolveVectorTransformBasis(const FString& InText, EMaterialVectorCoordTransformSource& OutSource) const
	{
		FString Value = UE::DreamShader::NormalizeSettingKey(InText);
		if (Value == TEXT("tangent"))
		{
			OutSource = TRANSFORMSOURCE_Tangent;
			return true;
		}
		if (Value == TEXT("local"))
		{
			OutSource = TRANSFORMSOURCE_Local;
			return true;
		}
		if (Value == TEXT("world") || Value == TEXT("absoluteworld"))
		{
			OutSource = TRANSFORMSOURCE_World;
			return true;
		}
		if (Value == TEXT("view"))
		{
			OutSource = TRANSFORMSOURCE_View;
			return true;
		}
		if (Value == TEXT("camera"))
		{
			OutSource = TRANSFORMSOURCE_Camera;
			return true;
		}
		if (Value == TEXT("instance") || Value == TEXT("particle") || Value == TEXT("instanceparticle"))
		{
			OutSource = TRANSFORMSOURCE_Instance;
			return true;
		}
		return false;
	}

	bool FCodeGraphBuilder::TryResolveVectorTransformTarget(const FString& InText, EMaterialVectorCoordTransform& OutTarget) const
	{
		FString Value = UE::DreamShader::NormalizeSettingKey(InText);
		if (Value == TEXT("tangent"))
		{
			OutTarget = TRANSFORM_Tangent;
			return true;
		}
		if (Value == TEXT("local"))
		{
			OutTarget = TRANSFORM_Local;
			return true;
		}
		if (Value == TEXT("world") || Value == TEXT("absoluteworld"))
		{
			OutTarget = TRANSFORM_World;
			return true;
		}
		if (Value == TEXT("view"))
		{
			OutTarget = TRANSFORM_View;
			return true;
		}
		if (Value == TEXT("camera"))
		{
			OutTarget = TRANSFORM_Camera;
			return true;
		}
		if (Value == TEXT("instance") || Value == TEXT("particle") || Value == TEXT("instanceparticle"))
		{
			OutTarget = TRANSFORM_Instance;
			return true;
		}
		return false;
	}

	bool FCodeGraphBuilder::TryResolvePositionTransformBasis(const FString& InText, EMaterialPositionTransformSource& OutBasis) const
	{
		FString Value = UE::DreamShader::NormalizeSettingKey(InText);
		if (Value == TEXT("local"))
		{
			OutBasis = TRANSFORMPOSSOURCE_Local;
			return true;
		}
		if (Value == TEXT("world") || Value == TEXT("absoluteworld"))
		{
			OutBasis = TRANSFORMPOSSOURCE_World;
			return true;
		}
		if (Value == TEXT("periodicworld"))
		{
			OutBasis = TRANSFORMPOSSOURCE_PeriodicWorld;
			return true;
		}
		if (Value == TEXT("translatedworld") || Value == TEXT("camerarelativeworld"))
		{
			OutBasis = TRANSFORMPOSSOURCE_TranslatedWorld;
			return true;
		}
		if (Value == TEXT("firstperson") || Value == TEXT("firstpersontranslatedworld"))
		{
			OutBasis = TRANSFORMPOSSOURCE_FirstPersonTranslatedWorld;
			return true;
		}
		if (Value == TEXT("view"))
		{
			OutBasis = TRANSFORMPOSSOURCE_View;
			return true;
		}
		if (Value == TEXT("camera"))
		{
			OutBasis = TRANSFORMPOSSOURCE_Camera;
			return true;
		}
		if (Value == TEXT("instance") || Value == TEXT("particle") || Value == TEXT("instanceparticle"))
		{
			OutBasis = TRANSFORMPOSSOURCE_Instance;
			return true;
		}
		return false;
	}

	bool FCodeGraphBuilder::EvaluateUEBuiltinCall(
		const FString& CalleeName,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const FString FunctionName = CalleeName.Mid(3);

		auto HandleIntegerLiteralArgument = [&](const TCHAR* ArgumentName, int32& TargetField) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			if (!TryExtractIntegerLiteral(Argument->Expression, TargetField))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be an integer literal."), *FunctionName, ArgumentName);
				return false;
			}

			return true;
		};

		auto HandleScalarLiteralArgument = [&](const TCHAR* ArgumentName, float& TargetField) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			double ParsedValue = 0.0;
			if (!TryExtractScalarLiteral(Argument->Expression, ParsedValue))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be a numeric literal."), *FunctionName, ArgumentName);
				return false;
			}

			TargetField = static_cast<float>(ParsedValue);
			return true;
		};

		auto HandleBooleanLiteralArgument = [&](const TCHAR* ArgumentName, TFunctionRef<void(bool)> ApplyValue) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			bool bParsedValue = false;
			if (!TryExtractBooleanLiteral(Argument->Expression, bParsedValue))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be a boolean literal."), *FunctionName, ArgumentName);
				return false;
			}

			ApplyValue(bParsedValue);
			return true;
		};

		auto HandleOptionalTextLiteralArgument = [&](const TCHAR* ArgumentName, FString& TargetValue) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			if (!TryExtractTextLiteral(Argument->Expression, TargetValue))
			{
				OutError = FString::Printf(TEXT("UE.%s %s must be a text value."), *FunctionName, ArgumentName);
				return false;
			}

			return true;
		};

		auto HandleOptionalInputArgument = [&](const TCHAR* ArgumentName, FExpressionInput& TargetInput) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				return true;
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(Argument->Expression, InputValue, OutError))
			{
				return false;
			}

			ConnectCodeValueToInput(TargetInput, InputValue);
			return true;
		};

		auto HandleRequiredInputArgument = [&](const TCHAR* ArgumentName, const int32 PositionalIndex, FExpressionInput& TargetInput) -> bool
		{
			const FCodeCallArgument* Argument = FindNamedArgument(Arguments, ArgumentName);
			if (!Argument)
			{
				Argument = FindPositionalArgument(Arguments, PositionalIndex);
			}
			if (!Argument)
			{
				OutError = FString::Printf(TEXT("UE.%s requires parameter: %s"), *FunctionName, ArgumentName);
				return false;
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(Argument->Expression, InputValue, OutError))
			{
				return false;
			}

			ConnectCodeValueToInput(TargetInput, InputValue);
			return true;
		};

		if (FunctionName.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
		{
			const FCodeCallArgument* NameArgument = FindNamedArgument(Arguments, TEXT("Name"));
			if (!NameArgument)
			{
				NameArgument = FindNamedArgument(Arguments, TEXT("ParameterName"));
			}
			if (!NameArgument)
			{
				OutError = TEXT("UE.StaticSwitchParameter requires Name=\"ParameterName\".");
				return false;
			}

			FString ParameterName;
			if (!TryExtractTextLiteral(NameArgument->Expression, ParameterName) || ParameterName.TrimStartAndEnd().IsEmpty())
			{
				OutError = TEXT("UE.StaticSwitchParameter Name must be a text value.");
				return false;
			}

			FTextShaderPropertyDefinition SwitchProperty;
			SwitchProperty.Name = ParameterName.TrimStartAndEnd();
			SwitchProperty.ParameterNodeType = TEXT("StaticSwitchParameter");
			if (const FCodeCallArgument* DefaultArgument = FindNamedArgument(Arguments, TEXT("Default")))
			{
				bool bDefault = false;
				if (!TryExtractBooleanLiteral(DefaultArgument->Expression, bDefault))
				{
					OutError = TEXT("UE.StaticSwitchParameter Default must be true or false.");
					return false;
				}
				SwitchProperty.bHasDefaultValue = true;
				SwitchProperty.ScalarDefaultValue = bDefault ? 1.0 : 0.0;
			}
			if (const FCodeCallArgument* GroupArgument = FindNamedArgument(Arguments, TEXT("Group")))
			{
				(void)TryExtractTextLiteral(GroupArgument->Expression, SwitchProperty.Metadata.Group);
			}
			if (const FCodeCallArgument* DescriptionArgument = FindNamedArgument(Arguments, TEXT("Description")))
			{
				(void)TryExtractTextLiteral(DescriptionArgument->Expression, SwitchProperty.Metadata.Description);
			}
			if (const FCodeCallArgument* SortArgument = FindNamedArgument(Arguments, TEXT("SortPriority")))
			{
				int32 SortPriority = 32;
				if (!TryExtractIntegerLiteral(SortArgument->Expression, SortPriority))
				{
					OutError = TEXT("UE.StaticSwitchParameter SortPriority must be an integer literal.");
					return false;
				}
				SwitchProperty.Metadata.bHasSortPriority = true;
				SwitchProperty.Metadata.SortPriority = SortPriority;
			}

			return EvaluateStaticSwitchParameterCall(SwitchProperty, Arguments, OutValue, OutError);
		}

		if (FunctionName.Equals(TEXT("CollectionParam"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("CollectionParameter"), ESearchCase::IgnoreCase))
		{
			const FCodeCallArgument* CollectionArgument = FindNamedArgument(Arguments, TEXT("Collection"));
			if (!CollectionArgument)
			{
				CollectionArgument = FindNamedArgument(Arguments, TEXT("Asset"));
			}
			if (!CollectionArgument)
			{
				OutError = TEXT("UE.CollectionParam requires Collection=Path(...).");
				return false;
			}

			FString CollectionText;
			if (!TryExtractAssetReferenceText(CollectionArgument->Expression, CollectionText))
			{
				OutError = TEXT("UE.CollectionParam Collection must be Path(...) or an Unreal object path.");
				return false;
			}

			FString CollectionObjectPath;
			if (!TryResolveDreamShaderAssetReference(CollectionText, CollectionObjectPath, OutError))
			{
				OutError = FString::Printf(TEXT("UE.CollectionParam Collection is invalid: %s"), *OutError);
				return false;
			}

			UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *CollectionObjectPath);
			if (!Collection)
			{
				OutError = FString::Printf(TEXT("UE.CollectionParam could not load MaterialParameterCollection '%s'."), *CollectionObjectPath);
				return false;
			}

			const FCodeCallArgument* ParameterArgument = FindNamedArgument(Arguments, TEXT("Parameter"));
			if (!ParameterArgument)
			{
				ParameterArgument = FindNamedArgument(Arguments, TEXT("ParameterName"));
			}
			if (!ParameterArgument)
			{
				OutError = TEXT("UE.CollectionParam requires Parameter=\"Name\".");
				return false;
			}

			FString ParameterText;
			if (!TryExtractTextLiteral(ParameterArgument->Expression, ParameterText) || ParameterText.TrimStartAndEnd().IsEmpty())
			{
				OutError = TEXT("UE.CollectionParam Parameter must be a text value.");
				return false;
			}

			const FName ParameterName(*ParameterText.TrimStartAndEnd());
			const bool bIsScalarParameter = Collection->GetScalarParameterByName(ParameterName) != nullptr;
			const bool bIsVectorParameter = Collection->GetVectorParameterByName(ParameterName) != nullptr;
			if (!bIsScalarParameter && !bIsVectorParameter)
			{
				OutError = FString::Printf(
					TEXT("UE.CollectionParam collection '%s' does not contain parameter '%s'."),
					*CollectionObjectPath,
					*ParameterText);
				return false;
			}

			auto* Expression = Cast<UMaterialExpressionCollectionParameter>(
				CreateExpression(UMaterialExpressionCollectionParameter::StaticClass(), 600, ConsumeNodeY()));
			if (!Expression)
			{
				OutError = TEXT("Failed to create UE.CollectionParam node.");
				return false;
			}

			Expression->Collection = Collection;
			Expression->ParameterName = ParameterName;
			Expression->ParameterId = Collection->GetParameterId(ParameterName);
			if (!Expression->ExpressionGUID.IsValid())
			{
				Expression->ExpressionGUID = FGuid::NewGuid();
			}

			FString GroupText;
			if (HandleOptionalTextLiteralArgument(TEXT("Group"), GroupText) && !GroupText.IsEmpty())
			{
				Expression->Group = FName(*GroupText);
			}
			else if (!OutError.IsEmpty())
			{
				return false;
			}

			if (const FCodeCallArgument* SortArgument = FindNamedArgument(Arguments, TEXT("SortPriority")))
			{
				int32 SortPriority = 32;
				if (!TryExtractIntegerLiteral(SortArgument->Expression, SortPriority))
				{
					OutError = TEXT("UE.CollectionParam SortPriority must be an integer literal.");
					return false;
				}
				Expression->SortPriority = SortPriority;
			}

			FString DescriptionText;
			if (HandleOptionalTextLiteralArgument(TEXT("Description"), DescriptionText) && !DescriptionText.IsEmpty())
			{
				Expression->Desc = DescriptionText;
			}
			else if (!OutError.IsEmpty())
			{
				return false;
			}

			OutValue.Expression = Expression;
			OutValue.OutputIndex = 0;
			OutValue.ComponentCount = bIsVectorParameter ? 4 : 1;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			return true;
		}

		struct FUEBuiltinDescriptor
		{
			const TCHAR* Name = TEXT("");
			TSubclassOf<UMaterialExpression> ExpressionClass;
			int32 OutputComponents = 1;
			TFunction<bool(UMaterialExpression*, FString&)> Configure;
		};

		auto TryEvaluateRegisteredBuiltin = [&](const FUEBuiltinDescriptor& Builtin) -> bool
		{
			UMaterialExpression* Expression = CreateExpression(Builtin.ExpressionClass, 600, ConsumeNodeY());
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create UE.%s."), Builtin.Name);
				return false;
			}

			if (Builtin.Configure && !Builtin.Configure(Expression, OutError))
			{
				return false;
			}

			OutValue.Expression = Expression;
			OutValue.OutputIndex = 0;
			OutValue.ComponentCount = Builtin.OutputComponents;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			return true;
		};

		// Register explicit DreamShader UE.* sugar here. Anything not listed falls back
		// to the reflected generic MaterialExpression path below.
		TArray<FUEBuiltinDescriptor> Builtins;
		Builtins.Reserve(10);

		Builtins.Add({
			TEXT("TexCoord"),
			UMaterialExpressionTextureCoordinate::StaticClass(),
			2,
			[&](UMaterialExpression* RawExpression, FString&) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTextureCoordinate>(RawExpression);
				return HandleIntegerLiteralArgument(TEXT("Index"), Expression->CoordinateIndex)
					&& HandleScalarLiteralArgument(TEXT("UTiling"), Expression->UTiling)
					&& HandleScalarLiteralArgument(TEXT("VTiling"), Expression->VTiling)
					&& HandleBooleanLiteralArgument(TEXT("UnMirrorU"), [&](const bool bValue) { Expression->UnMirrorU = bValue ? 1U : 0U; })
					&& HandleBooleanLiteralArgument(TEXT("UnMirrorV"), [&](const bool bValue) { Expression->UnMirrorV = bValue ? 1U : 0U; });
			}
		});

		Builtins.Add({
			TEXT("Time"),
			UMaterialExpressionTime::StaticClass(),
			1,
			[&](UMaterialExpression* RawExpression, FString& Error) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTime>(RawExpression);
				if (const FCodeCallArgument* Argument = FindNamedArgument(Arguments, TEXT("Period")))
				{
					double Period = 0.0;
					if (!TryExtractScalarLiteral(Argument->Expression, Period))
					{
						Error = FString::Printf(TEXT("UE.%s Period must be a numeric literal."), *FunctionName);
						return false;
					}

					Expression->bOverride_Period = true;
					Expression->Period = static_cast<float>(Period);
				}

				return HandleBooleanLiteralArgument(TEXT("IgnorePause"), [&](const bool bValue) { Expression->bIgnorePause = bValue ? 1U : 0U; });
			}
		});

		Builtins.Add({
			TEXT("Panner"),
			UMaterialExpressionPanner::StaticClass(),
			2,
			[&](UMaterialExpression* RawExpression, FString&) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionPanner>(RawExpression);
				return HandleOptionalInputArgument(TEXT("Coordinate"), Expression->Coordinate)
					&& HandleOptionalInputArgument(TEXT("Time"), Expression->Time)
					&& HandleOptionalInputArgument(TEXT("Speed"), Expression->Speed)
					&& HandleScalarLiteralArgument(TEXT("SpeedX"), Expression->SpeedX)
					&& HandleScalarLiteralArgument(TEXT("SpeedY"), Expression->SpeedY)
					&& HandleBooleanLiteralArgument(TEXT("FractionalPart"), [&](const bool bValue) { Expression->bFractionalPart = bValue ? 1U : 0U; });
			}
		});

		Builtins.Add({ TEXT("WorldPosition"), UMaterialExpressionWorldPosition::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("ObjectPositionWS"), UMaterialExpressionObjectPositionWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("CameraVectorWS"), UMaterialExpressionCameraVectorWS::StaticClass(), 3, {} });
		Builtins.Add({ TEXT("ScreenPosition"), UMaterialExpressionScreenPosition::StaticClass(), 4, {} });
		Builtins.Add({ TEXT("VertexColor"), UMaterialExpressionVertexColor::StaticClass(), 4, {} });

		Builtins.Add({
			TEXT("TransformVector"),
			UMaterialExpressionTransform::StaticClass(),
			3,
			[&](UMaterialExpression* RawExpression, FString& Error) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTransform>(RawExpression);
				if (!HandleRequiredInputArgument(TEXT("Input"), 0, Expression->Input))
				{
					return false;
				}

				FString SourceText = TEXT("Tangent");
				if (!HandleOptionalTextLiteralArgument(TEXT("Source"), SourceText))
				{
					return false;
				}

				FString DestinationText = TEXT("World");
				if (!HandleOptionalTextLiteralArgument(TEXT("Destination"), DestinationText))
				{
					return false;
				}

				EMaterialVectorCoordTransformSource SourceBasis = TRANSFORMSOURCE_Tangent;
				EMaterialVectorCoordTransform DestinationBasis = TRANSFORM_World;
				if (!TryResolveVectorTransformBasis(SourceText, SourceBasis)
					|| !TryResolveVectorTransformTarget(DestinationText, DestinationBasis))
				{
					Error = TEXT("UE.TransformVector Source/Destination is invalid.");
					return false;
				}

				Expression->TransformSourceType = SourceBasis;
				Expression->TransformType = DestinationBasis;
				return true;
			}
		});

		Builtins.Add({
			TEXT("TransformPosition"),
			UMaterialExpressionTransformPosition::StaticClass(),
			3,
			[&](UMaterialExpression* RawExpression, FString& Error) -> bool
			{
				auto* Expression = CastChecked<UMaterialExpressionTransformPosition>(RawExpression);
				if (!HandleRequiredInputArgument(TEXT("Input"), 0, Expression->Input))
				{
					return false;
				}

				FString SourceText = TEXT("Local");
				if (!HandleOptionalTextLiteralArgument(TEXT("Source"), SourceText))
				{
					return false;
				}

				FString DestinationText = TEXT("World");
				if (!HandleOptionalTextLiteralArgument(TEXT("Destination"), DestinationText))
				{
					return false;
				}

				EMaterialPositionTransformSource SourceBasis = TRANSFORMPOSSOURCE_Local;
				EMaterialPositionTransformSource DestinationBasis = TRANSFORMPOSSOURCE_World;
				if (!TryResolvePositionTransformBasis(SourceText, SourceBasis)
					|| !TryResolvePositionTransformBasis(DestinationText, DestinationBasis))
				{
					Error = TEXT("UE.TransformPosition Source/Destination is invalid.");
					return false;
				}

				Expression->TransformSourceType = SourceBasis;
				Expression->TransformType = DestinationBasis;

				if (!HandleOptionalInputArgument(TEXT("PeriodicWorldTileSize"), Expression->PeriodicWorldTileSize))
				{
					return false;
				}

				return HandleOptionalInputArgument(TEXT("FirstPersonInterpolationAlpha"), Expression->FirstPersonInterpolationAlpha);
			}
		});

		for (const FUEBuiltinDescriptor& Builtin : Builtins)
		{
			if (FunctionName.Equals(Builtin.Name, ESearchCase::IgnoreCase))
			{
				return TryEvaluateRegisteredBuiltin(Builtin);
			}
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (!Argument.bIsNamed)
			{
				OutError = FString::Printf(TEXT("Generic UE.%s calls require named arguments."), *FunctionName);
				return false;
			}
		}

		const FCodeCallArgument* OutputTypeArgument = FindNamedArgument(Arguments, TEXT("OutputType"));
		if (!OutputTypeArgument)
		{
			OutputTypeArgument = FindNamedArgument(Arguments, TEXT("ResultType"));
		}
		if (!OutputTypeArgument)
		{
			OutError = FString::Printf(
				TEXT("Unsupported UE builtin call '%s' in Graph. For generic MaterialExpression calls, add OutputType=\"float1/2/3/4/Texture2D\"."),
				*CalleeName);
			return false;
		}

		FString OutputTypeText;
		if (!TryExtractLiteralText(OutputTypeArgument->Expression, OutputTypeText))
		{
			OutError = FString::Printf(TEXT("UE.%s OutputType must be a literal value."), *FunctionName);
			return false;
		}

		int32 OutputComponents = 0;
		bool bIsTextureObject = false;
		if (!TryResolveCodeDeclaredType(OutputTypeText, OutputComponents, bIsTextureObject))
		{
			OutError = FString::Printf(TEXT("UE.%s OutputType '%s' is not supported."), *FunctionName, *OutputTypeText);
			return false;
		}

		FString ClassSpecifier = FunctionName;
		if (const FCodeCallArgument* ClassArgument = FindNamedArgument(Arguments, TEXT("Class")))
		{
			if (!TryExtractLiteralText(ClassArgument->Expression, ClassSpecifier))
			{
				OutError = FString::Printf(TEXT("UE.%s Class must be a literal value."), *FunctionName);
				return false;
			}
		}
		else if (FunctionName.Equals(TEXT("Expression"), ESearchCase::IgnoreCase))
		{
			OutError = TEXT("UE.Expression requires Class=\"MaterialExpressionName\".");
			return false;
		}

		UClass* ExpressionClass = ResolveMaterialExpressionClass(ClassSpecifier);
		if (!ExpressionClass)
		{
			OutError = FString::Printf(TEXT("UE.%s could not resolve MaterialExpression class '%s'."), *FunctionName, *ClassSpecifier);
			return false;
		}

		auto* Expression = Cast<UMaterialExpression>(CreateExpression(ExpressionClass, 600, ConsumeNodeY()));
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("UE.%s failed to create '%s'."), *FunctionName, *ExpressionClass->GetName());
			return false;
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			const FString NormalizedArgumentName = UE::DreamShader::NormalizeSettingKey(Argument.Name);
			if (NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("Class"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputType"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("ResultType"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("Output"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputName"))
				|| NormalizedArgumentName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputIndex")))
			{
				continue;
			}

			FProperty* BoundProperty = FindMaterialExpressionArgumentProperty(ExpressionClass, Argument.Name);
			if (!BoundProperty)
			{
				OutError = FString::Printf(TEXT("UE.%s: '%s' is not a property on '%s'."), *FunctionName, *Argument.Name, *ExpressionClass->GetName());
				return false;
			}

			if (IsMaterialExpressionInputProperty(BoundProperty))
			{
				FCodeValue InputValue;
				if (!EvaluateExpression(Argument.Expression, InputValue, OutError))
				{
					OutError = FString::Printf(TEXT("UE.%s input '%s': %s"), *FunctionName, *Argument.Name, *OutError);
					return false;
				}

				FExpressionInput* Input = BoundProperty->ContainerPtrToValuePtr<FExpressionInput>(Expression);
				if (!Input)
				{
					OutError = FString::Printf(TEXT("UE.%s failed to bind input '%s'."), *FunctionName, *Argument.Name);
					return false;
				}

				ConnectCodeValueToInput(*Input, InputValue);
			}
			else
			{
				FString LiteralText;
				const bool bWantsAssetReference = CastField<FObjectPropertyBase>(BoundProperty) != nullptr;
				if (bWantsAssetReference)
				{
					if (!TryExtractAssetReferenceText(Argument.Expression, LiteralText))
					{
						OutError = FString::Printf(TEXT("UE.%s property '%s' must use Path(...) or an Unreal object path."), *FunctionName, *Argument.Name);
						return false;
					}
				}
				else if (!TryExtractLiteralText(Argument.Expression, LiteralText))
				{
					OutError = FString::Printf(TEXT("UE.%s property '%s' must use a literal value."), *FunctionName, *Argument.Name);
					return false;
				}

				FString LiteralError;
				if (!SetMaterialExpressionLiteralProperty(Expression, BoundProperty, LiteralText, LiteralError))
				{
					OutError = FString::Printf(TEXT("UE.%s property '%s': %s"), *FunctionName, *Argument.Name, *LiteralError);
					return false;
				}
			}
		}

		const FCodeCallArgument* OutputNameArgument = FindNamedArgument(Arguments, TEXT("Output"));
		if (!OutputNameArgument)
		{
			OutputNameArgument = FindNamedArgument(Arguments, TEXT("OutputName"));
		}
		const FCodeCallArgument* OutputIndexArgument = FindNamedArgument(Arguments, TEXT("OutputIndex"));
		if (OutputNameArgument && OutputIndexArgument)
		{
			OutError = FString::Printf(TEXT("UE.%s cannot use OutputName/Output together with OutputIndex."), *FunctionName);
			return false;
		}

		int32 ResolvedOutputIndex = 0;
		if (OutputIndexArgument)
		{
			if (!TryExtractIntegerLiteral(OutputIndexArgument->Expression, ResolvedOutputIndex)
				|| ResolvedOutputIndex < 0
				|| !Expression->Outputs.IsValidIndex(ResolvedOutputIndex))
			{
				OutError = FString::Printf(TEXT("UE.%s OutputIndex is out of range for '%s'."), *FunctionName, *ExpressionClass->GetName());
				return false;
			}
		}
		else if (OutputNameArgument)
		{
			FString OutputNameText;
			if (!TryExtractLiteralText(OutputNameArgument->Expression, OutputNameText))
			{
				OutError = FString::Printf(TEXT("UE.%s OutputName must be a literal value."), *FunctionName);
				return false;
			}

			if (!TryResolveExpressionOutputIndex(Expression, OutputNameText, ResolvedOutputIndex))
			{
				OutError = FString::Printf(TEXT("UE.%s output '%s' was not found on '%s'."), *FunctionName, *OutputNameText, *ExpressionClass->GetName());
				return false;
			}
		}
		else if (!Expression->Outputs.IsValidIndex(0))
		{
			OutError = FString::Printf(TEXT("UE.%s created '%s', but it has no material outputs."), *FunctionName, *ExpressionClass->GetName());
			return false;
		}

		OutValue.Expression = Expression;
		OutValue.OutputIndex = ResolvedOutputIndex;
		OutValue.ComponentCount = OutputComponents;
		OutValue.bIsTextureObject = bIsTextureObject;
		OutValue.bIsMaterialAttributes = IsMaterialAttributesComponentType(OutputComponents, bIsTextureObject);
		return true;
	}
}
