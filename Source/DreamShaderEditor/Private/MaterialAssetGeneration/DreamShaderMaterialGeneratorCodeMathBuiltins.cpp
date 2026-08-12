// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder::EvaluateMathBuiltinCall: lowers DreamShaderLang math builtins (abs/floor/ceil/
// frac/saturate/sin/cos/sqrt/normalize/lerp/clamp/min/max/pow/dot/...) to the matching
// UMaterialExpression nodes. Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp;
// the member declaration stays in the FCodeGraphBuilder class header. Only cross-TU dependency is the
// now-exposed MakeCodeValueReuseToken (DreamShaderMaterialGeneratorPrivate.h).

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	bool FCodeGraphBuilder::EvaluateMathBuiltinCall(
		const FString& FunctionName,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		const auto ValidatePositionalArguments = [&]()
		{
			for (const FCodeCallArgument& Argument : Arguments)
			{
				if (Argument.bIsNamed)
				{
					OutError = FString::Printf(TEXT("Math function '%s' only accepts positional arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
			}
			return true;
		};

		const auto EvaluateArgument = [&](const int32 ArgumentIndex, FCodeValue& OutArgumentValue)
		{
			if (!Arguments.IsValidIndex(ArgumentIndex))
			{
				OutError = FString::Printf(TEXT("Math function '%s' is missing argument %d."), *FunctionName, ArgumentIndex + 1); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (!EvaluateExpression(Arguments[ArgumentIndex].Expression, OutArgumentValue, OutError))
			{
				OutError = FString::Printf(TEXT("Math function '%s' argument %d: %s"), *FunctionName, ArgumentIndex + 1, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (OutArgumentValue.bIsTextureObject || OutArgumentValue.bIsMaterialAttributes || OutArgumentValue.bIsSubstrateMaterial)
			{
				OutError = FString::Printf(TEXT("Math function '%s' only accepts numeric scalar/vector arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			return true;
		};

		const auto EvaluateUnary = [&](
			const TSubclassOf<UMaterialExpression> ExpressionClass,
			const TCHAR* InputName,
			const int32 OutputComponentCount) -> bool
		{
			if (Arguments.Num() != 1 || !ValidatePositionalArguments())
			{
				OutError = FString::Printf(TEXT("Math function '%s' expects exactly 1 argument."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue InputValue;
			if (!EvaluateArgument(0, InputValue))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-unary|%s|%s|%s|%d"),
				*UE::DreamShader::NormalizeSettingKey(FunctionName),
				*ExpressionClass->GetName(),
				*MakeCodeValueReuseToken(InputValue),
				OutputComponentCount);
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			UMaterialExpression* Expression = CreateExpression(ExpressionClass, 360, ConsumeNodeY());
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FProperty* InputProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), InputName);
			if (!InputProperty || !IsMaterialExpressionInputProperty(InputProperty))
			{
				OutError = FString::Printf(TEXT("Math function '%s' could not bind input '%s'."), *FunctionName, InputName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FExpressionInput* Input = InputProperty->ContainerPtrToValuePtr<FExpressionInput>(Expression);
			if (!Input)
			{
				OutError = FString::Printf(TEXT("Math function '%s' failed to access input '%s'."), *FunctionName, InputName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			ConnectCodeValueToInput(*Input, InputValue);
			OutValue.Expression = Expression;
			OutValue.ComponentCount = OutputComponentCount > 0 ? OutputComponentCount : InputValue.ComponentCount;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bHasAuthoritativeComponentCount =
				OutputComponentCount > 0 || InputValue.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		};

		if (FunctionName.Equals(TEXT("lerp"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("mix"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 3 || !ValidatePositionalArguments())
			{
				OutError = FString::Printf(TEXT("Math function '%s' expects exactly 3 arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue A;
			FCodeValue B;
			FCodeValue Alpha;
			if (!EvaluateArgument(0, A) || !EvaluateArgument(1, B) || !EvaluateArgument(2, Alpha))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-lerp|%s|%s|%s"),
				*MakeCodeValueReuseToken(A),
				*MakeCodeValueReuseToken(B),
				*MakeCodeValueReuseToken(Alpha));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			auto* Expression = Cast<UMaterialExpressionLinearInterpolate>(
				CreateExpression(UMaterialExpressionLinearInterpolate::StaticClass(), 360, ConsumeNodeY()));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			ConnectCodeValueToInput(Expression->A, A);
			ConnectCodeValueToInput(Expression->B, B);
			ConnectCodeValueToInput(Expression->Alpha, Alpha);
			OutValue.Expression = Expression;
			OutValue.ComponentCount = FMath::Max(A.ComponentCount, B.ComponentCount);
			OutValue.bHasAuthoritativeComponentCount =
				A.bHasAuthoritativeComponentCount || B.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		if (FunctionName.Equals(TEXT("dot"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 2 || !ValidatePositionalArguments())
			{
				OutError = FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue A;
			FCodeValue B;
			if (!EvaluateArgument(0, A) || !EvaluateArgument(1, B))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-dot|%s|%s"),
				*MakeCodeValueReuseToken(A),
				*MakeCodeValueReuseToken(B));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			auto* Expression = Cast<UMaterialExpressionDotProduct>(
				CreateExpression(UMaterialExpressionDotProduct::StaticClass(), 360, ConsumeNodeY()));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			ConnectCodeValueToInput(Expression->A, A);
			ConnectCodeValueToInput(Expression->B, B);
			OutValue.Expression = Expression;
			OutValue.ComponentCount = 1;
			OutValue.bHasAuthoritativeComponentCount = true;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		if (FunctionName.Equals(TEXT("pow"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 2 || !ValidatePositionalArguments())
			{
				OutError = FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue Base;
			FCodeValue Exponent;
			if (!EvaluateArgument(0, Base) || !EvaluateArgument(1, Exponent))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-pow|%s|%s"),
				*MakeCodeValueReuseToken(Base),
				*MakeCodeValueReuseToken(Exponent));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			auto* Expression = Cast<UMaterialExpressionPower>(
				CreateExpression(UMaterialExpressionPower::StaticClass(), 360, ConsumeNodeY()));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			ConnectCodeValueToInput(Expression->Base, Base);
			ConnectCodeValueToInput(Expression->Exponent, Exponent);
			OutValue.Expression = Expression;
			OutValue.ComponentCount = Base.ComponentCount;
			OutValue.bHasAuthoritativeComponentCount = Base.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		if (FunctionName.Equals(TEXT("min"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("max"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 2 || !ValidatePositionalArguments())
			{
				OutError = FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue A;
			FCodeValue B;
			if (!EvaluateArgument(0, A) || !EvaluateArgument(1, B))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-%s|%s|%s"),
				*UE::DreamShader::NormalizeSettingKey(FunctionName),
				*MakeCodeValueReuseToken(A),
				*MakeCodeValueReuseToken(B));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			UMaterialExpression* RawExpression = FunctionName.Equals(TEXT("min"), ESearchCase::IgnoreCase)
				? CreateExpression(UMaterialExpressionMin::StaticClass(), 360, ConsumeNodeY())
				: CreateExpression(UMaterialExpressionMax::StaticClass(), 360, ConsumeNodeY());
			if (!RawExpression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			if (auto* MinExpression = Cast<UMaterialExpressionMin>(RawExpression))
			{
				ConnectCodeValueToInput(MinExpression->A, A);
				ConnectCodeValueToInput(MinExpression->B, B);
			}
			else if (auto* MaxExpression = Cast<UMaterialExpressionMax>(RawExpression))
			{
				ConnectCodeValueToInput(MaxExpression->A, A);
				ConnectCodeValueToInput(MaxExpression->B, B);
			}

			OutValue.Expression = RawExpression;
			OutValue.ComponentCount = FMath::Max(A.ComponentCount, B.ComponentCount);
			OutValue.bHasAuthoritativeComponentCount =
				A.bHasAuthoritativeComponentCount || B.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		if (FunctionName.Equals(TEXT("clamp"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 3 || !ValidatePositionalArguments())
			{
				OutError = FString::Printf(TEXT("Math function '%s' expects exactly 3 arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue Input;
			FCodeValue Min;
			FCodeValue Max;
			if (!EvaluateArgument(0, Input) || !EvaluateArgument(1, Min) || !EvaluateArgument(2, Max))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-clamp|%s|%s|%s"),
				*MakeCodeValueReuseToken(Input),
				*MakeCodeValueReuseToken(Min),
				*MakeCodeValueReuseToken(Max));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			auto* Expression = Cast<UMaterialExpressionClamp>(
				CreateExpression(UMaterialExpressionClamp::StaticClass(), 360, ConsumeNodeY()));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			ConnectCodeValueToInput(Expression->Input, Input);
			ConnectCodeValueToInput(Expression->Min, Min);
			ConnectCodeValueToInput(Expression->Max, Max);
			OutValue.Expression = Expression;
			OutValue.ComponentCount = Input.ComponentCount;
			OutValue.bHasAuthoritativeComponentCount = Input.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		if (FunctionName.Equals(TEXT("saturate"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionSaturate::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("sin"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionSine::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("cos"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionCosine::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("abs"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionAbs::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("floor"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionFloor::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("ceil"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionCeil::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("frac"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("fract"), ESearchCase::IgnoreCase))
		{
			// `fract` is the GLSL spelling of `frac`; accept both in Graph like the HLSL/Function path.
			return EvaluateUnary(UMaterialExpressionFrac::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("sqrt"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionSquareRoot::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("normalize"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionNormalize::StaticClass(), TEXT("VectorInput"), 0);
		}

		// `fmod`/`mod` -> UMaterialExpressionFmod, matching the HLSL/Function path (NormalizeShaderLanguageText
		// aliases GLSL `mod` to `fmod`). Brings the Graph builtin set in line with the function-body set.
		if (FunctionName.Equals(TEXT("fmod"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("mod"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 2 || !ValidatePositionalArguments())
			{
				OutError = FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue Dividend;
			FCodeValue Divisor;
			if (!EvaluateArgument(0, Dividend) || !EvaluateArgument(1, Divisor))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-fmod|%s|%s"),
				*MakeCodeValueReuseToken(Dividend),
				*MakeCodeValueReuseToken(Divisor));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			auto* Expression = Cast<UMaterialExpressionFmod>(
				CreateExpression(UMaterialExpressionFmod::StaticClass(), 360, ConsumeNodeY()));
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			ConnectCodeValueToInput(Expression->A, Dividend);
			ConnectCodeValueToInput(Expression->B, Divisor);
			OutValue.Expression = Expression;
			OutValue.ComponentCount = Dividend.ComponentCount;
			OutValue.bHasAuthoritativeComponentCount = Dividend.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		return false;
	}
}
