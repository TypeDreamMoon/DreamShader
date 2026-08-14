// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder AST literal / qualified-name extraction: flatten dotted names and pull scalar /
// integer / boolean / text / asset-reference literals (and the `default` sentinel) out of parsed
// FCodeExpression nodes. Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp;
// the member declarations stay in the FCodeGraphBuilder class header, so all call sites are unchanged.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
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
				Parts.Add(FString::Printf(TEXT("\"%s\""), *Escaped)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				continue;
			}

			FString LiteralText;
			if (!TryExtractLiteralText(Argument.Expression, LiteralText))
			{
				return false;
			}
			Parts.Add(LiteralText);
		}

		OutText = FString::Printf(TEXT("Path(%s)"), *FString::Join(Parts, TEXT(", "))); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
}
