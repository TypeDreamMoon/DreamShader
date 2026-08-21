// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder vector/integer constructor handling: classify constructor names
// (int/int2.., float2/vec3..), resolve their component counts, and build the AppendVector graph for
// a constructor call. Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp;
// the member declarations stay in the FCodeGraphBuilder class header, so call sites are unchanged.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	bool FCodeGraphBuilder::IsIntegerConstructorName(const FString& InName)
	{
		return InName.Equals(TEXT("int"), ESearchCase::IgnoreCase)
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
			|| InName.Equals(TEXT("uvec4"), ESearchCase::IgnoreCase);
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
		FDreamShaderError& OutError)
	{
		const int32 ExpectedComponents = GetConstructorComponentCount(ConstructorName);

		// Constant-fold: when every component is a numeric literal, emit a single ConstantNVector node
		// instead of N scalar Constant nodes + (N-1) AppendVector nodes. This keeps generated graphs
		// compact for the very common cases (vec3(0,0,0), vec4(0.8,0.3,0.1,1), clamp bounds, etc.).
		// Integer constructors are excluded so their integer semantics are not collapsed into floats.
		if (ExpectedComponents >= 2 && !IsIntegerConstructorName(ConstructorName))
		{
			TArray<double> ConstantComponents;
			bool bAllConstantScalars = true;
			for (const FCodeCallArgument& Argument : Arguments)
			{
				double Scalar = 0.0;
				if (Argument.bIsNamed || !TryExtractScalarLiteral(Argument.Expression, Scalar))
				{
					bAllConstantScalars = false;
					break;
				}
				ConstantComponents.Add(Scalar);
			}

			// Replicate a single literal across all channels: vec3(0.5) -> (0.5, 0.5, 0.5).
			if (bAllConstantScalars && ConstantComponents.Num() == 1 && ExpectedComponents > 1)
			{
				ConstantComponents.Init(ConstantComponents[0], ExpectedComponents);
			}

			if (bAllConstantScalars && ConstantComponents.Num() == ExpectedComponents)
			{
				TArray<FString> ReuseKeyParts;
				ReuseKeyParts.Reserve(ConstantComponents.Num());
				for (const double Component : ConstantComponents)
				{
					ReuseKeyParts.Add(FString::SanitizeFloat(Component));
				}
				const FString ReuseKey = FString::Printf(TEXT("constvec%d|%s"), ExpectedComponents, *FString::Join(ReuseKeyParts, TEXT("|"))); /* I18N-EXEMPT: deferred codegen or compatibility path */
				if (TryFindReusableExpressionValue(ReuseKey, OutValue))
				{
					return true;
				}

				UMaterialExpression* VectorExpression = CreateVectorLiteralExpression(
					Material, MaterialFunction, ConstantComponents, ExpectedComponents, ConsumeNodeY());
				if (!VectorExpression)
				{
					OutError = FString::Printf(TEXT("Failed to create a constant float%d node for constructor '%s'."), ExpectedComponents, *ConstructorName); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				OutValue = FCodeValue{};
				OutValue.Expression = VectorExpression;
				OutValue.OutputIndex = 0;
				OutValue.ComponentCount = ExpectedComponents;
				OutValue.bHasAuthoritativeComponentCount = true;
				AddReusableExpressionValue(ReuseKey, OutValue);
				return true;
			}
		}

		TArray<FCodeValue> Parts;

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' does not accept named arguments."), *ConstructorName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue EvaluatedArgument;
			if (!EvaluateExpression(Argument.Expression, EvaluatedArgument, OutError))
			{
				return false;
			}

			if (EvaluatedArgument.bIsTextureObject)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' cannot use Texture2D arguments."), *ConstructorName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (EvaluatedArgument.bIsMaterialAttributes)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' cannot use MaterialAttributes arguments."), *ConstructorName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (EvaluatedArgument.bIsSubstrateMaterial)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' cannot use Substrate arguments."), *ConstructorName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			Parts.Add(EvaluatedArgument);
		}

		if (ExpectedComponents == 1)
		{
			if (Parts.Num() != 1 || Parts[0].ComponentCount != 1)
			{
				OutError = FString::Printf(TEXT("Constructor '%s' expects a single scalar input."), *ConstructorName); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

		if (Parts.Num() == 1 && Parts[0].ComponentCount == ExpectedComponents)
		{
			OutValue = Parts[0];
			return true;
		}

		int32 TotalComponents = 0;
		for (const FCodeValue& Part : Parts)
		{
			TotalComponents += Part.ComponentCount;
		}

		if (TotalComponents != ExpectedComponents)
		{
			OutError = FString::Printf(TEXT("Constructor '%s' expects %d total components but got %d."), *ConstructorName, ExpectedComponents, TotalComponents); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (!AppendValues(Parts, OutValue, OutError))
		{
			return false;
		}

		OutValue.ComponentCount = ExpectedComponents;
		return true;
	}
}
