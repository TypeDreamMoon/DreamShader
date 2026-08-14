// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder reusable-expression caching: build a stable key for a call/expression and
// look up / store the already-generated FCodeValue so identical sub-expressions reuse one node.
// Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp; member declarations
// stay in the class header. Only cross-TU dependency is the exposed MakeCodeValueReuseToken.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	bool FCodeGraphBuilder::TryBuildReusableCallKey(
		const FString& CallKind,
		const FString& FunctionName,
		const TArray<FCodeCallArgument>& Arguments,
		FString& OutKey) const
	{
		const TSet<FString> NoExcludedArguments;
		return TryBuildReusableCallKey(CallKind, FunctionName, Arguments, NoExcludedArguments, OutKey);
	}

	bool FCodeGraphBuilder::TryBuildReusableCallKey(
		const FString& CallKind,
		const FString& FunctionName,
		const TArray<FCodeCallArgument>& Arguments,
		const TSet<FString>& ExcludedNormalizedArgumentNames,
		FString& OutKey) const
	{
		TArray<FString> Parts;
		Parts.Add(UE::DreamShader::NormalizeSettingKey(CallKind));
		Parts.Add(UE::DreamShader::NormalizeSettingKey(FunctionName));

		for (int32 ArgumentIndex = 0; ArgumentIndex < Arguments.Num(); ++ArgumentIndex)
		{
			const FCodeCallArgument& Argument = Arguments[ArgumentIndex];
			const FString ArgumentName = Argument.bIsNamed
				? UE::DreamShader::NormalizeSettingKey(Argument.Name)
				: FString::Printf(TEXT("#%d"), ArgumentIndex); /* I18N-EXEMPT: deferred codegen or compatibility path */
			if (Argument.bIsNamed && ExcludedNormalizedArgumentNames.Contains(ArgumentName))
			{
				continue;
			}

			FString ArgumentToken;
			if (!BuildReusableExpressionToken(Argument.Expression, ArgumentToken))
			{
				return false;
			}
			Parts.Add(FString::Printf(TEXT("%s=%s"), *ArgumentName, *ArgumentToken)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		OutKey = FString::Join(Parts, TEXT("|"));
		return true;
	}

	bool FCodeGraphBuilder::BuildReusableExpressionToken(const TSharedPtr<FCodeExpression>& Expression, FString& OutToken) const
	{
		if (!Expression)
		{
			return false;
		}

		switch (Expression->Kind)
		{
		case ECodeExpressionKind::Name:
			if (const FCodeValue* ExistingValue = FindValue(Expression->Text))
			{
				OutToken = MakeCodeValueReuseToken(*ExistingValue);
				return true;
			}
			OutToken = FString::Printf(TEXT("name:%s"), *UE::DreamShader::NormalizeSettingKey(Expression->Text)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;

		case ECodeExpressionKind::NumberLiteral:
		case ECodeExpressionKind::StringLiteral:
			OutToken = FString::Printf(TEXT("literal:%s"), *NormalizeCodeReuseLiteralText(Expression->Text)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;

		case ECodeExpressionKind::Unary:
		{
			FString InnerToken;
			if (!BuildReusableExpressionToken(Expression->Left, InnerToken))
			{
				return false;
			}
			OutToken = FString::Printf(TEXT("unary:%s(%s)"), *Expression->Text, *InnerToken); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;
		}

		case ECodeExpressionKind::Binary:
		{
			FString LeftToken;
			FString RightToken;
			if (!BuildReusableExpressionToken(Expression->Left, LeftToken)
				|| !BuildReusableExpressionToken(Expression->Right, RightToken))
			{
				return false;
			}
			OutToken = FString::Printf(TEXT("binary:%s(%s,%s)"), *Expression->Text, *LeftToken, *RightToken); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;
		}

		case ECodeExpressionKind::MemberAccess:
		{
			FString LeftToken;
			if (!BuildReusableExpressionToken(Expression->Left, LeftToken))
			{
				return false;
			}
			OutToken = FString::Printf(TEXT("member:%s.%s"), *LeftToken, *UE::DreamShader::NormalizeSettingKey(Expression->Text)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return true;
		}

		case ECodeExpressionKind::Call:
		{
			FString CalleeName;
			if (!TryFlattenQualifiedName(Expression->Left, CalleeName))
			{
				return false;
			}
			return TryBuildReusableCallKey(TEXT("call"), CalleeName, Expression->Arguments, OutToken);
		}

		default:
			return false;
		}
	}

	bool FCodeGraphBuilder::TryFindReusableExpressionValue(const FString& Key, FCodeValue& OutValue) const
	{
		if (Key.IsEmpty())
		{
			return false;
		}

		if (const FCodeValue* ExistingValue = ReusableExpressionValues.Find(Key))
		{
			OutValue = *ExistingValue;
			return OutValue.Expression != nullptr;
		}

		return false;
	}

	void FCodeGraphBuilder::AddReusableExpressionValue(const FString& Key, const FCodeValue& Value)
	{
		if (!Key.IsEmpty() && Value.Expression)
		{
			ReusableExpressionValues.Add(Key, Value);
		}
	}
}
