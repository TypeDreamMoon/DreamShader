// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder::EvaluateMathBuiltinCall: lowers DreamShaderLang math builtins (abs/floor/ceil/
// frac/saturate/sin/cos/sqrt/normalize/lerp/clamp/min/max/pow/dot/step/smoothstep/length/cross/
// asin/acos/atan/atan2/reflect/refract/...) to the matching UMaterialExpression nodes. Extracted
// byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp; the member declaration stays in
// the FCodeGraphBuilder class header. Only cross-TU dependency is the now-exposed
// MakeCodeValueReuseToken (DreamShaderMaterialGeneratorPrivate.h).
//
// Every builtin here maps to exactly one node except reflect and refract, which the engine has no
// expression for and which are lowered to a small subgraph of arithmetic nodes.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	bool FCodeGraphBuilder::EvaluateMathBuiltinCall(
		const FString& FunctionName,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FDreamShaderError& OutError)
	{
		const auto ValidatePositionalArguments = [&]()
		{
			for (const FCodeCallArgument& Argument : Arguments)
			{
				if (Argument.bIsNamed)
				{
					return FailWith(OutError, TEXT("DSH5001"), FString::Printf(TEXT("Math function '%s' only accepts positional arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
			}
			return true;
		};

		const auto EvaluateArgument = [&](const int32 ArgumentIndex, FCodeValue& OutArgumentValue)
		{
			if (!Arguments.IsValidIndex(ArgumentIndex))
			{
				return FailWith(OutError, TEXT("DSH5002"), FString::Printf(TEXT("Math function '%s' is missing argument %d."), *FunctionName, ArgumentIndex + 1)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			if (!EvaluateExpression(Arguments[ArgumentIndex].Expression, OutArgumentValue, OutError))
			{
				return WrapError(OutError, FString::Printf(TEXT("Math function '%s' argument %d: %s"), *FunctionName, ArgumentIndex + 1, *OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			if (OutArgumentValue.bIsTextureObject || OutArgumentValue.bIsMaterialAttributes || OutArgumentValue.bIsSubstrateMaterial)
			{
				return FailWith(OutError, TEXT("DSH5003"), FString::Printf(TEXT("Math function '%s' only accepts numeric scalar/vector arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			return true;
		};

		// Wires one already-evaluated argument to a pin found by reflected property name. Shared by the
		// fixed-arity helpers below so every one of them reports the same two binding diagnostics.
		const auto BindNamedInput = [&](
			UMaterialExpression* Expression,
			const TCHAR* InputName,
			const FCodeValue& InputValue) -> bool
		{
			FProperty* InputProperty = FindMaterialExpressionArgumentProperty(Expression->GetClass(), InputName);
			if (!InputProperty || !IsMaterialExpressionInputProperty(InputProperty))
			{
				return FailWith(OutError, TEXT("DSH5004"), FString::Printf(TEXT("Math function '%s' could not bind input '%s'."), *FunctionName, InputName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FExpressionInput* Input = InputProperty->ContainerPtrToValuePtr<FExpressionInput>(Expression);
			if (!Input)
			{
				return FailWith(OutError, TEXT("DSH5005"), FString::Printf(TEXT("Math function '%s' failed to access input '%s'."), *FunctionName, InputName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			ConnectCodeValueToInput(*Input, InputValue);
			return true;
		};

		const auto EvaluateUnary = [&](
			const TSubclassOf<UMaterialExpression> ExpressionClass,
			const TCHAR* InputName,
			const int32 OutputComponentCount) -> bool
		{
			if (Arguments.Num() != 1 || !ValidatePositionalArguments())
			{
				return FailWith(OutError, TEXT("DSH5006"), FString::Printf(TEXT("Math function '%s' expects exactly 1 argument."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5007"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (!BindNamedInput(Expression, InputName, InputValue))
			{
				return false;
			}

			OutValue.Expression = Expression;
			OutValue.ComponentCount = OutputComponentCount > 0 ? OutputComponentCount : InputValue.ComponentCount;
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bHasAuthoritativeComponentCount =
				OutputComponentCount > 0 || InputValue.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		};

		// Two-argument counterpart of EvaluateUnary. The pin names are passed in because the engine is
		// not consistent about them: CrossProduct is A/B, Step and Arctangent2 are Y/X.
		const auto EvaluateBinary = [&](
			const TSubclassOf<UMaterialExpression> ExpressionClass,
			const TCHAR* FirstInputName,
			const TCHAR* SecondInputName,
			const int32 OutputComponentCount) -> bool
		{
			if (Arguments.Num() != 2 || !ValidatePositionalArguments())
			{
				return FailWith(OutError, TEXT("DSH5008"), FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FCodeValue FirstValue;
			FCodeValue SecondValue;
			if (!EvaluateArgument(0, FirstValue) || !EvaluateArgument(1, SecondValue))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-binary|%s|%s|%s|%s|%d"),
				*UE::DreamShader::NormalizeSettingKey(FunctionName),
				*ExpressionClass->GetName(),
				*MakeCodeValueReuseToken(FirstValue),
				*MakeCodeValueReuseToken(SecondValue),
				OutputComponentCount);
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			UMaterialExpression* Expression = CreateExpression(ExpressionClass, 360, ConsumeNodeY());
			if (!Expression)
			{
				return FailWith(OutError, TEXT("DSH5009"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (!BindNamedInput(Expression, FirstInputName, FirstValue)
				|| !BindNamedInput(Expression, SecondInputName, SecondValue))
			{
				return false;
			}

			OutValue.Expression = Expression;
			OutValue.ComponentCount = OutputComponentCount > 0
				? OutputComponentCount
				: FMath::Max(FirstValue.ComponentCount, SecondValue.ComponentCount);
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bHasAuthoritativeComponentCount = OutputComponentCount > 0
				|| FirstValue.bHasAuthoritativeComponentCount
				|| SecondValue.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		};

		// Three-argument counterpart. Only smoothstep uses it today.
		const auto EvaluateTernary = [&](
			const TSubclassOf<UMaterialExpression> ExpressionClass,
			const TCHAR* FirstInputName,
			const TCHAR* SecondInputName,
			const TCHAR* ThirdInputName) -> bool
		{
			if (Arguments.Num() != 3 || !ValidatePositionalArguments())
			{
				return FailWith(OutError, TEXT("DSH5010"), FString::Printf(TEXT("Math function '%s' expects exactly 3 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FCodeValue FirstValue;
			FCodeValue SecondValue;
			FCodeValue ThirdValue;
			if (!EvaluateArgument(0, FirstValue)
				|| !EvaluateArgument(1, SecondValue)
				|| !EvaluateArgument(2, ThirdValue))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-ternary|%s|%s|%s|%s|%s"),
				*UE::DreamShader::NormalizeSettingKey(FunctionName),
				*ExpressionClass->GetName(),
				*MakeCodeValueReuseToken(FirstValue),
				*MakeCodeValueReuseToken(SecondValue),
				*MakeCodeValueReuseToken(ThirdValue));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			UMaterialExpression* Expression = CreateExpression(ExpressionClass, 360, ConsumeNodeY());
			if (!Expression)
			{
				return FailWith(OutError, TEXT("DSH5011"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (!BindNamedInput(Expression, FirstInputName, FirstValue)
				|| !BindNamedInput(Expression, SecondInputName, SecondValue)
				|| !BindNamedInput(Expression, ThirdInputName, ThirdValue))
			{
				return false;
			}

			OutValue.Expression = Expression;
			OutValue.ComponentCount = FMath::Max3(
				FirstValue.ComponentCount, SecondValue.ComponentCount, ThirdValue.ComponentCount);
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bHasAuthoritativeComponentCount =
				FirstValue.bHasAuthoritativeComponentCount
				|| SecondValue.bHasAuthoritativeComponentCount
				|| ThirdValue.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		};

		// Creates one node for the reflect/refract lowerings below, which build a small subgraph rather
		// than a single node. Sets OutError and returns null on failure, so a lowering can create its
		// whole node set first and check once.
		const auto CreateLoweringNode = [&](const TSubclassOf<UMaterialExpression> ExpressionClass) -> UMaterialExpression*
		{
			UMaterialExpression* Expression = CreateExpression(ExpressionClass, 360, ConsumeNodeY());
			if (!Expression)
			{
				OutError = FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			return Expression;
		};

		if (FunctionName.Equals(TEXT("lerp"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("mix"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 3 || !ValidatePositionalArguments())
			{
				return FailWith(OutError, TEXT("DSH5012"), FString::Printf(TEXT("Math function '%s' expects exactly 3 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5013"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5014"), FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5015"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5016"), FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5017"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5018"), FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5019"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5020"), FString::Printf(TEXT("Math function '%s' expects exactly 3 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5021"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5022"), FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH5023"), FString::Printf(TEXT("Failed to create math function '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			ConnectCodeValueToInput(Expression->A, Dividend);
			ConnectCodeValueToInput(Expression->B, Divisor);
			OutValue.Expression = Expression;
			OutValue.ComponentCount = Dividend.ComponentCount;
			OutValue.bHasAuthoritativeComponentCount = Dividend.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		// `step(edge, x)` -> UMaterialExpressionStep, whose pins are (Y = edge, X = value) and whose
		// translator emits `X >= Y`. Argument order therefore maps to the pins reversed, matching HLSL.
		if (FunctionName.Equals(TEXT("step"), ESearchCase::IgnoreCase))
		{
			return EvaluateBinary(UMaterialExpressionStep::StaticClass(), TEXT("Y"), TEXT("X"), 0);
		}

		// `smoothstep(min, max, x)` -> UMaterialExpressionSmoothStep (Min/Max/Value), argument order
		// as in HLSL.
		if (FunctionName.Equals(TEXT("smoothstep"), ESearchCase::IgnoreCase))
		{
			return EvaluateTernary(
				UMaterialExpressionSmoothStep::StaticClass(), TEXT("Min"), TEXT("Max"), TEXT("Value"));
		}

		// Authoritative widths: length collapses to a scalar and cross is always 3 components,
		// whatever the arguments were. Both match TryResolveKnownExpressionOutputComponentCount, which
		// already reports these widths for the same classes reached through UE.Expression.
		if (FunctionName.Equals(TEXT("length"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionLength::StaticClass(), TEXT("Input"), 1);
		}
		if (FunctionName.Equals(TEXT("cross"), ESearchCase::IgnoreCase))
		{
			return EvaluateBinary(UMaterialExpressionCrossProduct::StaticClass(), TEXT("A"), TEXT("B"), 3);
		}

		if (FunctionName.Equals(TEXT("asin"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionArcsine::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("acos"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionArccosine::StaticClass(), TEXT("Input"), 0);
		}
		if (FunctionName.Equals(TEXT("atan"), ESearchCase::IgnoreCase))
		{
			return EvaluateUnary(UMaterialExpressionArctangent::StaticClass(), TEXT("Input"), 0);
		}
		// `atan2(y, x)` -> UMaterialExpressionArctangent2, whose pins are already named Y and X.
		if (FunctionName.Equals(TEXT("atan2"), ESearchCase::IgnoreCase))
		{
			return EvaluateBinary(UMaterialExpressionArctangent2::StaticClass(), TEXT("Y"), TEXT("X"), 0);
		}

		// No UMaterialExpression computes reflect(), so lower it to its HLSL definition,
		// I - 2 * dot(I, N) * N, as four nodes. The literal 2 rides Multiply's ConstB instead of
		// costing a separate Constant node.
		if (FunctionName.Equals(TEXT("reflect"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 2 || !ValidatePositionalArguments())
			{
				return FailWith(OutError, TEXT("DSH5024"), FString::Printf(TEXT("Math function '%s' expects exactly 2 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FCodeValue Incident;
			FCodeValue Normal;
			if (!EvaluateArgument(0, Incident) || !EvaluateArgument(1, Normal))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-reflect|%s|%s"),
				*MakeCodeValueReuseToken(Incident),
				*MakeCodeValueReuseToken(Normal));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			auto* IncidentDotNormal = Cast<UMaterialExpressionDotProduct>(
				CreateLoweringNode(UMaterialExpressionDotProduct::StaticClass()));
			auto* ScaledDot = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* ScaledNormal = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* Reflected = Cast<UMaterialExpressionSubtract>(
				CreateLoweringNode(UMaterialExpressionSubtract::StaticClass()));
			if (!IncidentDotNormal || !ScaledDot || !ScaledNormal || !Reflected)
			{
				return false;
			}

			ConnectCodeValueToInput(IncidentDotNormal->A, Incident);
			ConnectCodeValueToInput(IncidentDotNormal->B, Normal);

			ScaledDot->A.Connect(0, IncidentDotNormal);
			ScaledDot->ConstB = 2.0f;

			ScaledNormal->A.Connect(0, ScaledDot);
			ConnectCodeValueToInput(ScaledNormal->B, Normal);

			ConnectCodeValueToInput(Reflected->A, Incident);
			Reflected->B.Connect(0, ScaledNormal);

			OutValue.Expression = Reflected;
			OutValue.ComponentCount = FMath::Max(Incident.ComponentCount, Normal.ComponentCount);
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bHasAuthoritativeComponentCount =
				Incident.bHasAuthoritativeComponentCount || Normal.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		// refract() has no node either. Lowered to the same definition HLSL uses:
		//   k = 1 - eta*eta * (1 - dot(N, I)^2)
		//   k < 0 ? 0 : eta*I - (eta*dot(N, I) + sqrt(k)) * N
		// The total-internal-reflection branch is an If node, so both sides are translated and one is
		// selected -- the sqrt of a negative k lands only on the discarded side, exactly as in HLSL.
		// The zero branch is built as I * 0 rather than a Constant so its type always equals I's,
		// which the If node requires and which the tracked component count cannot always guarantee.
		if (FunctionName.Equals(TEXT("refract"), ESearchCase::IgnoreCase))
		{
			if (Arguments.Num() != 3 || !ValidatePositionalArguments())
			{
				return FailWith(OutError, TEXT("DSH5025"), FString::Printf(TEXT("Math function '%s' expects exactly 3 arguments."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FCodeValue Incident;
			FCodeValue Normal;
			FCodeValue Eta;
			if (!EvaluateArgument(0, Incident) || !EvaluateArgument(1, Normal) || !EvaluateArgument(2, Eta))
			{
				return false;
			}

			FString ReuseKey = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("math-refract|%s|%s|%s"),
				*MakeCodeValueReuseToken(Incident),
				*MakeCodeValueReuseToken(Normal),
				*MakeCodeValueReuseToken(Eta));
			if (TryFindReusableExpressionValue(ReuseKey, OutValue))
			{
				return true;
			}

			auto* NormalDotIncident = Cast<UMaterialExpressionDotProduct>(
				CreateLoweringNode(UMaterialExpressionDotProduct::StaticClass()));
			auto* NdotISquared = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* SinSquared = Cast<UMaterialExpressionSubtract>(
				CreateLoweringNode(UMaterialExpressionSubtract::StaticClass()));
			auto* EtaSquared = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* EtaSinSquared = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* K = Cast<UMaterialExpressionSubtract>(
				CreateLoweringNode(UMaterialExpressionSubtract::StaticClass()));
			auto* SqrtK = Cast<UMaterialExpressionSquareRoot>(
				CreateLoweringNode(UMaterialExpressionSquareRoot::StaticClass()));
			auto* EtaNdotI = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* NormalScale = Cast<UMaterialExpressionAdd>(
				CreateLoweringNode(UMaterialExpressionAdd::StaticClass()));
			auto* ScaledNormal = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* ScaledIncident = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* Refracted = Cast<UMaterialExpressionSubtract>(
				CreateLoweringNode(UMaterialExpressionSubtract::StaticClass()));
			auto* ZeroVector = Cast<UMaterialExpressionMultiply>(
				CreateLoweringNode(UMaterialExpressionMultiply::StaticClass()));
			auto* Selected = Cast<UMaterialExpressionIf>(
				CreateLoweringNode(UMaterialExpressionIf::StaticClass()));
			if (!NormalDotIncident || !NdotISquared || !SinSquared || !EtaSquared || !EtaSinSquared
				|| !K || !SqrtK || !EtaNdotI || !NormalScale || !ScaledNormal || !ScaledIncident
				|| !Refracted || !ZeroVector || !Selected)
			{
				return false;
			}

			ConnectCodeValueToInput(NormalDotIncident->A, Normal);
			ConnectCodeValueToInput(NormalDotIncident->B, Incident);

			NdotISquared->A.Connect(0, NormalDotIncident);
			NdotISquared->B.Connect(0, NormalDotIncident);

			SinSquared->ConstA = 1.0f;
			SinSquared->B.Connect(0, NdotISquared);

			ConnectCodeValueToInput(EtaSquared->A, Eta);
			ConnectCodeValueToInput(EtaSquared->B, Eta);

			EtaSinSquared->A.Connect(0, EtaSquared);
			EtaSinSquared->B.Connect(0, SinSquared);

			K->ConstA = 1.0f;
			K->B.Connect(0, EtaSinSquared);

			SqrtK->Input.Connect(0, K);

			ConnectCodeValueToInput(EtaNdotI->A, Eta);
			EtaNdotI->B.Connect(0, NormalDotIncident);

			NormalScale->A.Connect(0, EtaNdotI);
			NormalScale->B.Connect(0, SqrtK);

			ScaledNormal->A.Connect(0, NormalScale);
			ConnectCodeValueToInput(ScaledNormal->B, Normal);

			ConnectCodeValueToInput(ScaledIncident->A, Incident);
			ConnectCodeValueToInput(ScaledIncident->B, Eta);

			Refracted->A.Connect(0, ScaledIncident);
			Refracted->B.Connect(0, ScaledNormal);

			ConnectCodeValueToInput(ZeroVector->A, Incident);
			ZeroVector->ConstB = 0.0f;

			// k == 0 still satisfies the refraction formula (sqrt(0) == 0), so it takes the refracted
			// side; only k < 0 falls through to zero.
			Selected->A.Connect(0, K);
			Selected->ConstB = 0.0f;
			Selected->AGreaterThanB.Connect(0, Refracted);
			Selected->AEqualsB.Connect(0, Refracted);
			Selected->ALessThanB.Connect(0, ZeroVector);

			OutValue.Expression = Selected;
			OutValue.ComponentCount = FMath::Max(Incident.ComponentCount, Normal.ComponentCount);
			OutValue.bIsTextureObject = false;
			OutValue.bIsMaterialAttributes = false;
			OutValue.bHasAuthoritativeComponentCount =
				Incident.bHasAuthoritativeComponentCount || Normal.bHasAuthoritativeComponentCount;
			AddReusableExpressionValue(ReuseKey, OutValue);
			return true;
		}

		return false;
	}
}
