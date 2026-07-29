// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder property / parameter materialization: resolve a property name to its
// FTextShaderPropertyDefinition, lazily create the parameter expression node for a referenced
// property (FindPropertyDefinition / TryCreatePropertyValue), and evaluate StaticSwitchParameter
// calls. Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp; member
// declarations stay in the FCodeGraphBuilder class header, so call sites are unchanged.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopeExit.h"
#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
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

	bool FCodeGraphBuilder::TryCreatePropertyValue(const FString& Name, FCodeValue& OutValue, FString& OutError)
	{
		if (!Values)
		{
			return false;
		}

		const FTextShaderPropertyDefinition* Property = FindPropertyDefinition(Name);
		if (!Property)
		{
			return false;
		}

		if (Property->ParameterNodeType.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		for (const FString& CreatingName : CreatingPropertyNames)
		{
			if (CreatingName.Equals(Property->Name, ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(TEXT("Property '%s' has a recursive UE builtin dependency."), *Property->Name);
				return true;
			}
		}

		CreatingPropertyNames.Add(Property->Name);

		if (Property->Source == ETextShaderPropertySource::UEBuiltin)
		{
			for (const TPair<FString, FString>& Argument : Property->UEBuiltinArguments)
			{
				const FString DependencyName = Argument.Value.TrimStartAndEnd();
				if (DependencyName.IsEmpty() || FindValue(DependencyName))
				{
					continue;
				}

				FCodeValue IgnoredDependencyValue;
				FString DependencyError;
				if (TryCreatePropertyValue(DependencyName, IgnoredDependencyValue, DependencyError) && !DependencyError.IsEmpty())
				{
					OutError = DependencyError;
					CreatingPropertyNames.Remove(Property->Name);
					return true;
				}
			}
		}

		FString PropertyExpressionError;
		UMaterialExpression* PropertyExpression = CreatePropertyExpression(
			Material,
			MaterialFunction,
			*Property,
			GeneratedPropertyExpressions,
			NextPropertyNodeY,
			PropertyExpressionError);
		if (!PropertyExpression)
		{
			OutError = FString::Printf(TEXT("Property '%s': %s"), *Property->Name, *PropertyExpressionError);
			CreatingPropertyNames.Remove(Property->Name);
			return true;
		}

		GeneratedPropertyExpressions.Add(Property->Name, PropertyExpression);
		GeneratedExpressionsByVariable.Add(Property->Name, PropertyExpression);

		OutValue = FCodeValue{};
		OutValue.Expression = PropertyExpression;
		OutValue.OutputIndex = 0;
		if (Property->Type == ETextShaderPropertyType::Vector && !Property->bConst)
		{
			static const TCHAR* ComponentOutputs[] = { TEXT(""), TEXT("R"), TEXT("RG"), TEXT("RGB"), TEXT("RGBA") };
			int32 OutputIndex = 0;
			if (Property->ComponentCount > 0
				&& Property->ComponentCount < UE_ARRAY_COUNT(ComponentOutputs)
				&& TryResolveExpressionOutputIndex(PropertyExpression, ComponentOutputs[Property->ComponentCount], OutputIndex))
			{
				OutValue.OutputIndex = OutputIndex;
			}
		}
		OutValue.ComponentCount = Property->Type == ETextShaderPropertyType::Texture2D
			? 0
			: Property->ComponentCount;
		OutValue.bIsTextureObject = Property->Type == ETextShaderPropertyType::Texture2D;
		OutValue.TextureType = ResolveEffectiveTextureType(*Property);
		OutValue.bIsMaterialAttributes = false;
		Values->Add(Property->Name, OutValue);
		NextPropertyNodeY += 220;
		CreatingPropertyNames.Remove(Property->Name);
		return true;
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
		if (TrueValue.bIsSubstrateMaterial || FalseValue.bIsSubstrateMaterial)
		{
			OutError = FString::Printf(TEXT("StaticSwitchParameter '%s' cannot switch Substrate values."), *Property.Name);
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
				CreateExpression(UMaterialExpressionStaticSwitchParameter::StaticClass(), 520, ConsumeNodeY()));
		}

		if (!SwitchExpression)
		{
			OutError = FString::Printf(TEXT("Failed to create StaticSwitchParameter node '%s'."), *Property.Name);
			return false;
		}

		SwitchExpression->ParameterName = FName(*Property.Name);
		const bool bDefaultValue = Property.bHasDefaultValue && Property.ScalarDefaultValue != 0.0;
		SwitchExpression->DefaultValue = bDefaultValue ? 1U : 0U;
		if (!SwitchExpression->ExpressionGUID.IsValid())
		{
			SwitchExpression->ExpressionGUID = FGuid::NewGuid();
		}
		if (Material)
		{
			Material->SetStaticSwitchParameterValueEditorOnly(SwitchExpression->ParameterName, bDefaultValue, SwitchExpression->ExpressionGUID);
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
		OutValue.bIsSubstrateMaterial = false;
		return true;
	}

	bool FCodeGraphBuilder::ParameterTypeAcceptsInputArguments(const FString& ParameterNodeType) const
	{
		// Parameter node types that own input pins the author may want to wire from the Graph.
		static const TCHAR* const InputBearingTypes[] = {
			TEXT("ChannelMaskParameter"),
			TEXT("StaticComponentMaskParameter"),
			TEXT("TextureSampleParameter2D"),
			TEXT("TextureSampleParameter2DArray"),
			TEXT("TextureSampleParameterCube"),
			TEXT("TextureSampleParameterCubeArray"),
			TEXT("TextureSampleParameterVolume"),
			TEXT("TextureSampleParameterSubUV"),
			TEXT("RuntimeVirtualTextureSampleParameter"),
			TEXT("SparseVolumeTextureSampleParameter"),
		};
		for (const TCHAR* Type : InputBearingTypes)
		{
			if (ParameterNodeType.Equals(Type, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool FCodeGraphBuilder::EvaluateConfigurableParameterCall(
		const FTextShaderPropertyDefinition& Property,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		// Materialise the parameter node (and its base output) exactly as a bare reference does, then
		// wire each named argument onto the matching input pin. The node is cached by name, so a later
		// bare reference to the same parameter shares this configured node.
		OutError.Reset();
		if (!TryCreatePropertyValue(Property.Name, OutValue, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Could not resolve parameter '%s' for configuration."), *Property.Name);
			}
			return false;
		}
		if (!OutError.IsEmpty())
		{
			return false;
		}

		UMaterialExpression* Expression = OutValue.Expression;
		if (!Expression)
		{
			OutError = FString::Printf(TEXT("Parameter '%s' did not produce an expression node."), *Property.Name);
			return false;
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (!Argument.bIsNamed)
			{
				OutError = FString::Printf(
					TEXT("Parameter '%s' must be called with named arguments wiring its input pins (e.g. %s(Coordinates=...) or %s(Input=...))."),
					*Property.Name, *Property.Name, *Property.Name);
				return false;
			}

			const FString NormalizedArgumentName = UE::DreamShader::NormalizeSettingKey(Argument.Name);
			FExpressionInput* BoundInput = nullptr;
			for (int32 InputIndex = 0; InputIndex < GetDreamShaderExpressionInputCount(Expression); ++InputIndex)
			{
				FExpressionInput* CandidateInput = Expression->GetInput(InputIndex);
				const FName InputName = Expression->GetInputName(InputIndex);
				if (CandidateInput
					&& !InputName.IsNone()
					&& UE::DreamShader::NormalizeSettingKey(InputName.ToString()) == NormalizedArgumentName)
				{
					BoundInput = CandidateInput;
					break;
				}
			}

			if (!BoundInput)
			{
				OutError = FString::Printf(
					TEXT("Parameter '%s' (%s) has no input pin named '%s'. Asset slots (Texture/Curve/Font/...) are set via [%s=Path(...)] metadata, not call arguments."),
					*Property.Name, *Property.ParameterNodeType, *Argument.Name, *Argument.Name);
				return false;
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(Argument.Expression, InputValue, OutError))
			{
				OutError = FString::Printf(TEXT("Parameter '%s' input '%s': %s"), *Property.Name, *Argument.Name, *OutError);
				return false;
			}
			if (InputValue.bIsTextureObject || InputValue.bIsMaterialAttributes || InputValue.bIsSubstrateMaterial)
			{
				OutError = FString::Printf(TEXT("Parameter '%s' input '%s' must be a numeric value."), *Property.Name, *Argument.Name);
				return false;
			}

			ConnectCodeValueToInput(*BoundInput, InputValue);
		}

		return true;
	}
}
