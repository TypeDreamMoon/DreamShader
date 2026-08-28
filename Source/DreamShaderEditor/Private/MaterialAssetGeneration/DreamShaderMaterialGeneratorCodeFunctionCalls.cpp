// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FCodeGraphBuilder material-function / virtual-function call handling: evaluate & execute calls to
// UMaterialFunction assets and VirtualFunction declarations, wiring a MaterialFunctionCall node and
// propagating output types. Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeCalls.cpp;
// member declarations stay in the FCodeGraphBuilder class header. Cross-TU dependencies are the
// now-exposed ApplyFunctionCallOutputType / IsSubstrateTypeUnsupportedForEngine (private header).

#include "DreamShaderMaterialGeneratorCodeShared.h"
#include "DreamShaderMaterialGeneratorPrivate.h"

namespace UE::DreamShader::Editor::Private
{
	bool FCodeGraphBuilder::EvaluateMaterialFunctionCall(
		const FTextShaderMaterialFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FDreamShaderError& OutError)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Function.Name, Function.Root, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		return EvaluateMaterialFunctionCallAsset(
			UE::DreamShader::LexToString(Function.Kind),
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
		FDreamShaderError& OutError)
	{
		FString ObjectPath;
		if (!TryResolveDreamShaderAssetReference(Function.Asset, ObjectPath, OutError))
		{
			return WrapError(OutError, FString::Printf(TEXT("VirtualFunction '%s' asset reference is invalid: %s"), *Function.Name, *OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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

	bool FCodeGraphBuilder::ExecuteMaterialFunctionCall(
		const FTextShaderMaterialFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FDreamShaderError& OutError)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Function.Name, Function.Root, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		return ExecuteMaterialFunctionCallAsset(
			UE::DreamShader::LexToString(Function.Kind),
			Function.Name,
			ObjectPath,
			Function.Inputs,
			Function.Outputs,
			Arguments,
			OutError);
	}

	bool FCodeGraphBuilder::ExecuteVirtualFunctionCall(
		const FTextShaderVirtualFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FDreamShaderError& OutError)
	{
		FString ObjectPath;
		if (!TryResolveDreamShaderAssetReference(Function.Asset, ObjectPath, OutError))
		{
			return WrapError(OutError, FString::Printf(TEXT("VirtualFunction '%s' asset reference is invalid: %s"), *Function.Name, *OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		return ExecuteMaterialFunctionCallAsset(
			TEXT("VirtualFunction"),
			Function.Name,
			ObjectPath,
			Function.Inputs,
			Function.Outputs,
			Arguments,
			OutError);
	}

	bool FCodeGraphBuilder::CreateAndConnectMaterialFunctionCallAsset(
		const FString& CallKind,
		const FString& FunctionName,
		const FString& ObjectPath,
		const TArray<FTextShaderFunctionParameter>& Inputs,
		const TArray<FTextShaderFunctionParameter>& Outputs,
		const TArray<FCodeCallArgument>& InputArguments,
		UMaterialExpressionMaterialFunctionCall*& OutFunctionCall,
		FDreamShaderError& OutError)
	{
		OutFunctionCall = nullptr;
		if (Outputs.IsEmpty())
		{
			return FailWith(OutError, TEXT("DSH6001"), FString::Printf(TEXT("%s '%s' must declare at least one output."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		UMaterialFunction* MaterialFunctionAsset = LoadObject<UMaterialFunction>(nullptr, *ObjectPath);
		if (!MaterialFunctionAsset)
		{
			return FailWith(OutError, TEXT("DSH6002"), FString::Printf(TEXT("%s '%s' could not load MaterialFunction asset '%s'."), *CallKind, *FunctionName, *ObjectPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		auto* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(
			CreateExpression(UMaterialExpressionMaterialFunctionCall::StaticClass(), 640, ConsumeNodeY()));
		if (!FunctionCall)
		{
			return FailWith(OutError, TEXT("DSH6003"), FString::Printf(TEXT("Failed to create a MaterialFunctionCall node for '%s'."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		if (!FunctionCall->SetMaterialFunction(MaterialFunctionAsset))
		{
			return FailWith(OutError, TEXT("DSH6004"), FString::Printf(TEXT("Failed to assign material function '%s' to the generated call node."), *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		TArray<const FCodeCallArgument*> PositionalArguments;
		bool bHasNamedArgument = false;
		for (const FCodeCallArgument& Argument : InputArguments)
		{
			if (Argument.bIsNamed)
			{
				bHasNamedArgument = true;
			}
			else
			{
				PositionalArguments.Add(&Argument);
			}
		}

		if (bHasNamedArgument && PositionalArguments.Num() > 0)
		{
			return FailWith(OutError, TEXT("DSH6005"), FString::Printf(TEXT("%s '%s' input arguments cannot mix positional and named forms."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		int32 PositionalArgumentIndex = 0;
		for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
		{
			const FTextShaderFunctionParameter& InputDefinition = Inputs[InputIndex];
			const FCodeCallArgument* InputArgument = FindNamedArgument(InputArguments, *InputDefinition.Name);
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
				return FailWith(OutError, TEXT("DSH6006"), FString::Printf(TEXT("%s '%s' input '%s' does not exist on MaterialFunction asset '%s'."), *CallKind, *FunctionName, *InputDefinition.Name, *ObjectPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (!InputArgument)
			{
				if (InputDefinition.bOptional)
				{
					continue;
				}

				return FailWith(OutError, TEXT("DSH6007"), FString::Printf(TEXT("%s '%s' is missing required input '%s'."), *CallKind, *FunctionName, *InputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (IsDefaultArgument(InputArgument->Expression))
			{
				if (InputDefinition.bOptional)
				{
					continue;
				}

				return FailWith(OutError, TEXT("DSH6008"), FString::Printf(TEXT("%s '%s' input '%s' is not optional and cannot use default."), *CallKind, *FunctionName, *InputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(InputArgument->Expression, InputValue, OutError))
			{
				return WrapError(OutError, FString::Printf(TEXT("%s '%s' input '%s': %s"), *CallKind, *FunctionName, *InputDefinition.Name, *OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			int32 ExpectedComponentCount = 1;
			bool bExpectedTexture = false;
			bool bExpectedSubstrate = false;
			ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
			if (!TryResolveCodeDeclaredType(InputDefinition.Type, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate))
			{
				if (IsSubstrateTypeUnsupportedForEngine(InputDefinition.Type))
				{
					return FailWith(OutError, TEXT("DSH6009"), FString::Printf(TEXT("%s '%s' input '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *CallKind, *FunctionName, *InputDefinition.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
				return FailWith(OutError, TEXT("DSH6010"), FString::Printf(TEXT("%s '%s' input '%s' uses unsupported type '%s'."), *CallKind, *FunctionName, *InputDefinition.Name, *InputDefinition.Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FCodeValue CoercedValue;
			if (!CoerceValueToType(InputValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedValue, OutError))
			{
				return WrapError(OutError, FString::Printf(TEXT("%s '%s' input '%s': %s"), *CallKind, *FunctionName, *InputDefinition.Name, *OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			ConnectCodeValueToInput(FunctionCall->FunctionInputs[FunctionInputIndex].Input, CoercedValue);
		}

		if (PositionalArgumentIndex < PositionalArguments.Num())
		{
			return FailWith(OutError, TEXT("DSH6011"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s '%s' received %d positional input argument(s), but only %d input(s) are declared."), *CallKind, *FunctionName, PositionalArguments.Num(), Inputs.Num()));
		}

		for (const FCodeCallArgument& Argument : InputArguments)
		{
			if (!Argument.bIsNamed)
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
				return FailWith(OutError, TEXT("DSH6012"), FString::Printf(TEXT("%s '%s' does not have an input named '%s'."), *CallKind, *FunctionName, *Argument.Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
		}

		OutFunctionCall = FunctionCall;
		return true;
	}

	bool FCodeGraphBuilder::ExecuteMaterialFunctionCallAsset(
		const FString& CallKind,
		const FString& FunctionName,
		const FString& ObjectPath,
		const TArray<FTextShaderFunctionParameter>& Inputs,
		const TArray<FTextShaderFunctionParameter>& Outputs,
		const TArray<FCodeCallArgument>& Arguments,
		FDreamShaderError& OutError)
	{
		if (!Values)
		{
			return FailWith(OutError, TEXT("DSH6013"), FString::Printf(TEXT("%s '%s' statement call requires an active Graph build context."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		if (Outputs.IsEmpty())
		{
			return FailWith(OutError, TEXT("DSH6014"), FString::Printf(TEXT("%s '%s' must declare at least one output."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed)
			{
				return FailWith(OutError, TEXT("DSH6015"), FString::Printf(TEXT("%s '%s' statement calls currently use positional arguments only."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
		}

		if (Arguments.Num() < Outputs.Num())
		{
			return FailWith(OutError, TEXT("DSH6016"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s '%s' expects output target arguments after its inputs, but got %d total argument(s) for %d output(s)."), *CallKind, *FunctionName, Arguments.Num(), Outputs.Num()));
		}

		const int32 InputArgumentCount = Arguments.Num() - Outputs.Num();
		if (InputArgumentCount > Inputs.Num())
		{
			return FailWith(OutError, TEXT("DSH6017"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s '%s' expects at most %d input argument(s) followed by %d output target(s), but got %d input argument(s)."), *CallKind, *FunctionName, Inputs.Num(), Outputs.Num(), InputArgumentCount));
		}

		for (int32 InputIndex = InputArgumentCount; InputIndex < Inputs.Num(); ++InputIndex)
		{
			if (!Inputs[InputIndex].bOptional)
			{
				return FailWith(OutError, TEXT("DSH6018"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s '%s' is missing required input '%s'."), *CallKind, *FunctionName, *Inputs[InputIndex].Name));
			}
		}

		TArray<FCodeCallArgument> InputArguments;
		InputArguments.Reserve(InputArgumentCount);
		for (int32 InputIndex = 0; InputIndex < InputArgumentCount; ++InputIndex)
		{
			InputArguments.Add(Arguments[InputIndex]);
		}

		TArray<FString> OutputTargetNames;
		OutputTargetNames.Reserve(Outputs.Num());
		TSet<FString> SeenTargetNames;
		for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
		{
			const FCodeCallArgument& Argument = Arguments[InputArgumentCount + OutputIndex];
			if (!Argument.Expression || Argument.Expression->Kind != ECodeExpressionKind::Name)
			{
				return FailWith(OutError, TEXT("DSH6019"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("%s '%s' output argument %d must be a plain variable name."), *CallKind, *FunctionName, OutputIndex + 1));
			}

			const FString TargetName = Argument.Expression->Text.TrimStartAndEnd();
			if (TargetName.IsEmpty())
			{
				return FailWith(OutError, TEXT("DSH6020"), FString::Printf(TEXT("%s '%s' has an empty output target name."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			if (SeenTargetNames.Contains(TargetName))
			{
				return FailWith(OutError, TEXT("DSH6021"), FString::Printf(TEXT("%s '%s' cannot write multiple outputs into '%s' in the same call."), *CallKind, *FunctionName, *TargetName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			SeenTargetNames.Add(TargetName);
			OutputTargetNames.Add(TargetName);
		}

		UMaterialExpressionMaterialFunctionCall* FunctionCall = nullptr;
		if (!CreateAndConnectMaterialFunctionCallAsset(
			CallKind,
			FunctionName,
			ObjectPath,
			Inputs,
			Outputs,
			InputArguments,
			FunctionCall,
			OutError))
		{
			return false;
		}

		for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
		{
			int32 OutputComponents = 0;
			bool bIsTextureObject = false;
			ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
			bool bIsSubstrateMaterial = false;
			if (!TryResolveCodeDeclaredType(Outputs[OutputIndex].Type, OutputComponents, bIsTextureObject, TextureType, bIsSubstrateMaterial))
			{
				if (IsSubstrateTypeUnsupportedForEngine(Outputs[OutputIndex].Type))
				{
					return FailWith(OutError, TEXT("DSH6022"), FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
				return FailWith(OutError, TEXT("DSH6023"), FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name, *Outputs[OutputIndex].Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH6024"), FString::Printf(TEXT("%s '%s' output '%s' does not exist on MaterialFunction asset '%s'."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name, *ObjectPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			ApplyFunctionCallOutputType(FunctionCall, FunctionOutputIndex, OutputComponents, bIsTextureObject, bIsSubstrateMaterial);

			FCodeValue OutputValue;
			OutputValue.Expression = FunctionCall;
			OutputValue.OutputIndex = FunctionOutputIndex;
			OutputValue.ComponentCount = OutputComponents;
			OutputValue.bIsTextureObject = bIsTextureObject;
			OutputValue.TextureType = TextureType;
			OutputValue.bIsMaterialAttributes = IsMaterialAttributesComponentType(OutputComponents, bIsTextureObject, bIsSubstrateMaterial);
			OutputValue.bIsSubstrateMaterial = bIsSubstrateMaterial;
			(*Values).Add(OutputTargetNames[OutputIndex], OutputValue);
		}

		return true;
	}

	bool FCodeGraphBuilder::EvaluateMaterialFunctionCallAsset(
		const FString& CallKind,
		const FString& FunctionName,
		const FString& ObjectPath,
		const TArray<FTextShaderFunctionParameter>& Inputs,
		const TArray<FTextShaderFunctionParameter>& Outputs,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FDreamShaderError& OutError)
	{
		auto TryInlineBreakOutFunction = [&]() -> bool
		{
			if (!CallKind.Equals(TEXT("VirtualFunction"), ESearchCase::IgnoreCase)
				|| !(FunctionName.Equals(TEXT("BreakOutFloat2Components"), ESearchCase::IgnoreCase)
					|| FunctionName.Equals(TEXT("BreakOutFloat3Components"), ESearchCase::IgnoreCase)
					|| FunctionName.Equals(TEXT("BreakOutFloat4Components"), ESearchCase::IgnoreCase)))
			{
				return false;
			}

			const FCodeCallArgument* InputArgument = FindPositionalArgument(Arguments, 0);
			if (!InputArgument && !Inputs.IsEmpty())
			{
				InputArgument = FindNamedArgument(Arguments, *Inputs[0].Name);
			}
			if (!InputArgument || IsDefaultArgument(InputArgument->Expression))
			{
				return false;
			}

			const FCodeCallArgument* OutputArgument = FindNamedArgument(Arguments, TEXT("Output"));
			const FCodeCallArgument* OutputIndexOnlyArgument = FindNamedArgument(Arguments, TEXT("OutputIndex"));
			if (OutputArgument && OutputIndexOnlyArgument)
			{
				return false;
			}

			bool bOutputIndexArgument = false;
			if (!OutputArgument)
			{
				OutputArgument = FindNamedArgument(Arguments, TEXT("OutputName"));
				if (OutputArgument && OutputIndexOnlyArgument)
				{
					return false;
				}
			}
			if (!OutputArgument)
			{
				OutputArgument = OutputIndexOnlyArgument;
				bOutputIndexArgument = OutputArgument != nullptr;
			}
			if (!OutputArgument)
			{
				return false;
			}

			int32 OutputChannelIndex = INDEX_NONE;
			if (bOutputIndexArgument)
			{
				if (!TryExtractIntegerLiteral(OutputArgument->Expression, OutputChannelIndex)
					|| !Outputs.IsValidIndex(OutputChannelIndex))
				{
					return false;
				}
			}
			else
			{
				FString OutputText;
				if (!TryExtractLiteralText(OutputArgument->Expression, OutputText))
				{
					return false;
				}
				OutputText.TrimStartAndEndInline();

				if (ParseIntegerLiteral(OutputText, OutputChannelIndex))
				{
					if (!Outputs.IsValidIndex(OutputChannelIndex))
					{
						return false;
					}
				}
				else
				{
					// Map the requested output to a swizzle channel by its semantic NAME (R/G/B/A or
					// X/Y/Z/W), not by its position in the declared Outputs list. A list-position
					// mapping disagrees with the real-asset path (which resolves by name) whenever the
					// outputs are declared in a non-canonical order, e.g. [B,G,R]. If the matched output
					// name is not a recognized channel letter, leave OutputChannelIndex == INDEX_NONE so
					// the bounds check below returns false and we fall back to the name-resolved path.
					for (int32 CandidateIndex = 0; CandidateIndex < Outputs.Num(); ++CandidateIndex)
					{
						const FString& CandidateName = Outputs[CandidateIndex].Name;
						if (CandidateName.Equals(OutputText, ESearchCase::IgnoreCase))
						{
							if (CandidateName.Len() == 1)
							{
								switch (FChar::ToLower(CandidateName[0]))
								{
								case TCHAR('x'): case TCHAR('r'): OutputChannelIndex = 0; break;
								case TCHAR('y'): case TCHAR('g'): OutputChannelIndex = 1; break;
								case TCHAR('z'): case TCHAR('b'): OutputChannelIndex = 2; break;
								case TCHAR('w'): case TCHAR('a'): OutputChannelIndex = 3; break;
								default: break;
								}
							}
							break;
						}
					}
				}
			}

			static const TCHAR* Swizzles[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
			if (OutputChannelIndex < 0 || OutputChannelIndex >= UE_ARRAY_COUNT(Swizzles))
			{
				return false;
			}

			FCodeValue InputValue;
			if (!EvaluateExpression(InputArgument->Expression, InputValue, OutError))
			{
				return true;
			}
			if (!CreateSwizzleExpression(InputValue, Swizzles[OutputChannelIndex], OutValue, OutError))
			{
				return true;
			}
			return true;
		};

		if (TryInlineBreakOutFunction())
		{
			return OutError.IsEmpty();
		}

		const FCodeCallArgument* OutputNameArgument = FindNamedArgument(Arguments, TEXT("Output"));
		if (!OutputNameArgument)
		{
			OutputNameArgument = FindNamedArgument(Arguments, TEXT("OutputName"));
		}
		const FCodeCallArgument* OutputIndexArgument = FindNamedArgument(Arguments, TEXT("OutputIndex"));
		if (OutputNameArgument && OutputIndexArgument)
		{
			return FailWith(OutError, TEXT("DSH6025"), FString::Printf(TEXT("%s '%s' cannot use OutputName/Output together with OutputIndex."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		TArray<FCodeCallArgument> InputArguments;
		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed)
			{
				const FString NormalizedName = UE::DreamShader::NormalizeSettingKey(Argument.Name);
				if (NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("Output"))
					|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputName"))
					|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputIndex")))
				{
					continue;
				}
			}

			InputArguments.Add(Argument);
		}

		FString FunctionCallReuseKey;
		FString OutputReuseKey;
		if (TryBuildReusableCallKey(CallKind, FunctionName, InputArguments, FunctionCallReuseKey))
		{
			FunctionCallReuseKey = FString::Printf(TEXT("%s|Asset=%s"), *FunctionCallReuseKey, *ObjectPath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			if (OutputNameArgument)
			{
				FString OutputNameText;
				if (TryExtractLiteralText(OutputNameArgument->Expression, OutputNameText))
				{
					OutputReuseKey = FunctionCallReuseKey + FString::Printf(TEXT("|OutputName=%s"), *NormalizeCodeReuseLiteralText(OutputNameText)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
			}
			else if (OutputIndexArgument)
			{
				FString OutputIndexText;
				if (TryExtractLiteralText(OutputIndexArgument->Expression, OutputIndexText))
				{
					OutputReuseKey = FunctionCallReuseKey + FString::Printf(TEXT("|OutputIndex=%s"), *NormalizeCodeReuseLiteralText(OutputIndexText)); /* I18N-EXEMPT: deferred codegen or compatibility path */
				}
			}
			else if (Outputs.Num() == 1)
			{
				OutputReuseKey = FunctionCallReuseKey + TEXT("|OutputIndex=0");
			}
			if (TryFindReusableExpressionValue(OutputReuseKey, OutValue))
			{
				return true;
			}
		}

		UMaterialExpressionMaterialFunctionCall* FunctionCall = nullptr;
		FCodeValue ReusableFunctionCallValue;
		if (TryFindReusableExpressionValue(FunctionCallReuseKey, ReusableFunctionCallValue))
		{
			FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(ReusableFunctionCallValue.Expression);
		}
		if (!FunctionCall
			&& !CreateAndConnectMaterialFunctionCallAsset(
				CallKind,
				FunctionName,
				ObjectPath,
				Inputs,
				Outputs,
				InputArguments,
				FunctionCall,
				OutError))
		{
			return false;
		}
		if (!FunctionCallReuseKey.IsEmpty() && FunctionCall)
		{
			FCodeValue FunctionCallValue;
			FunctionCallValue.Expression = FunctionCall;
			FunctionCallValue.OutputIndex = 0;
			FunctionCallValue.ComponentCount = 0;
			AddReusableExpressionValue(FunctionCallReuseKey, FunctionCallValue);
		}

		int32 OutputIndex = 0;
		if (OutputIndexArgument)
		{
			if (!TryExtractIntegerLiteral(OutputIndexArgument->Expression, OutputIndex)
				|| OutputIndex < 0
				|| !Outputs.IsValidIndex(OutputIndex))
			{
				return FailWith(OutError, TEXT("DSH6026"), FString::Printf(TEXT("%s '%s' OutputIndex is out of range."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
		}
		else if (OutputNameArgument)
		{
			FString OutputName;
			if (!TryExtractLiteralText(OutputNameArgument->Expression, OutputName))
			{
				return FailWith(OutError, TEXT("DSH6027"), FString::Printf(TEXT("%s '%s' OutputName must be a literal value."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				return FailWith(OutError, TEXT("DSH6028"), FString::Printf(TEXT("%s '%s' does not expose an output named '%s'."), *CallKind, *FunctionName, *OutputName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
		}
		else if (Outputs.Num() != 1)
		{
			return FailWith(OutError, TEXT("DSH6029"), FString::Printf(TEXT("%s '%s' exposes multiple outputs. Specify Output=\"Name\" or OutputIndex=N."), *CallKind, *FunctionName)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		int32 OutputComponents = 0;
		bool bIsTextureObject = false;
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		bool bIsSubstrateMaterial = false;
		if (!TryResolveCodeDeclaredType(Outputs[OutputIndex].Type, OutputComponents, bIsTextureObject, TextureType, bIsSubstrateMaterial))
		{
			if (IsSubstrateTypeUnsupportedForEngine(Outputs[OutputIndex].Type))
			{
				return FailWith(OutError, TEXT("DSH6030"), FString::Printf(TEXT("%s '%s' output '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name)); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			return FailWith(OutError, TEXT("DSH6031"), FString::Printf(TEXT("%s '%s' output '%s' uses unsupported type '%s'."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name, *Outputs[OutputIndex].Type)); /* I18N-EXEMPT: deferred codegen or compatibility path */
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
			return FailWith(OutError, TEXT("DSH6032"), FString::Printf(TEXT("%s '%s' output '%s' does not exist on MaterialFunction asset '%s'."), *CallKind, *FunctionName, *Outputs[OutputIndex].Name, *ObjectPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}
		ApplyFunctionCallOutputType(FunctionCall, FunctionOutputIndex, OutputComponents, bIsTextureObject, bIsSubstrateMaterial);

		OutValue.Expression = FunctionCall;
		OutValue.OutputIndex = FunctionOutputIndex;
		OutValue.ComponentCount = OutputComponents;
		OutValue.bIsTextureObject = bIsTextureObject;
		OutValue.TextureType = TextureType;
		OutValue.bIsMaterialAttributes = IsMaterialAttributesComponentType(OutputComponents, bIsTextureObject, bIsSubstrateMaterial);
		OutValue.bIsSubstrateMaterial = bIsSubstrateMaterial;
		AddReusableExpressionValue(OutputReuseKey, OutValue);
		return true;
	}
}
