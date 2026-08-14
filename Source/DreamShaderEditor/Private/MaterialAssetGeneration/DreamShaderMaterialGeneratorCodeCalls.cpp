#include "DreamShaderMaterialGeneratorCodeShared.h"

namespace UE::DreamShader::Editor::Private
{
	bool ApplyFunctionCallOutputType(
		UMaterialExpressionMaterialFunctionCall* FunctionCall,
		const int32 FunctionOutputIndex,
		int32& InOutComponentCount,
		bool& bInOutIsTextureObject,
		bool& bInOutIsSubstrateMaterial)
	{
		if (!FunctionCall || !FunctionCall->FunctionOutputs.IsValidIndex(FunctionOutputIndex))
		{
			return false;
		}

		// MaterialFunctionCall::GetOutputValueType can report scalar for function outputs
		// even when the referenced FunctionOutput is a vector. Keep the source declaration
		// authoritative so vector outputs are not expanded through invalid AppendVector nodes.
		if (InOutComponentCount > 0 || bInOutIsTextureObject || bInOutIsSubstrateMaterial || IsMaterialAttributesComponentType(InOutComponentCount, bInOutIsTextureObject, bInOutIsSubstrateMaterial))
		{
			return true;
		}

		int32 ResolvedComponentCount = 0;
		bool bResolvedIsTextureObject = false;
		const EMaterialValueType OutputValueType = GetDreamShaderExpressionOutputValueType(FunctionCall, FunctionOutputIndex);
		if (IsSubstrateMaterialValueType(OutputValueType))
		{
			InOutComponentCount = 0;
			bInOutIsTextureObject = false;
			bInOutIsSubstrateMaterial = true;
			return true;
		}
		if (TryResolveMaterialValueType(OutputValueType, ResolvedComponentCount, bResolvedIsTextureObject))
		{
			InOutComponentCount = ResolvedComponentCount;
			bInOutIsTextureObject = bResolvedIsTextureObject;
			bInOutIsSubstrateMaterial = false;
			return true;
		}

		const FFunctionExpressionOutput& FunctionOutput = FunctionCall->FunctionOutputs[FunctionOutputIndex];
		if (FunctionOutput.Output.Mask)
		{
			const int32 MaskComponentCount =
				(FunctionOutput.Output.MaskR ? 1 : 0)
				+ (FunctionOutput.Output.MaskG ? 1 : 0)
				+ (FunctionOutput.Output.MaskB ? 1 : 0)
				+ (FunctionOutput.Output.MaskA ? 1 : 0);
			if (MaskComponentCount > 0)
			{
				InOutComponentCount = MaskComponentCount;
				bInOutIsTextureObject = false;
				bInOutIsSubstrateMaterial = false;
				return true;
			}
		}

		return false;
	}

	FString BuildFunctionSourceArgumentList(const FTextShaderFunctionDefinition& Function, const TArray<FString>& ResultVariableNames)
	{
		TArray<FString> Parameters;
		for (const FTextShaderFunctionParameter& Input : Function.Inputs)
		{
			Parameters.Add(Input.Name);
		}
		for (const FString& ResultVariableName : ResultVariableNames)
		{
			Parameters.Add(ResultVariableName);
		}
		return FString::Join(Parameters, TEXT(", "));
	}

	bool IsSubstrateTypeUnsupportedForEngine(const FString& TypeName)
	{
		return IsSubstrateMaterialType(TypeName) && !IsSubstrateMaterialTypeSupported();
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
			OutError = FString::Printf(TEXT("Unknown Graph function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (Function->Results.Num() != 1)
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader Function '%s' has %d outputs and must be called with explicit out variables, for example %s(..., ResultA, ResultB)."),
				*FunctionName,
				Function->Results.Num(),
				*FunctionName);
			return false;
		}

		if (Arguments.Num() != Function->Inputs.Num())
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader Function '%s' returns one value and expects %d input argument(s) when used as a value expression, but got %d."),
				*FunctionName,
				Function->Inputs.Num(),
				Arguments.Num());
			return false;
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed)
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' currently uses positional arguments only."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		ECustomMaterialOutputType ResultOutputType = CMOT_Float1;
		if (IsSubstrateMaterialType(Function->Results[0].Type))
		{
			OutError = IsSubstrateMaterialTypeSupported()
				? FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead."), *FunctionName, *Function->Results[0].Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
				: FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *FunctionName, *Function->Results[0].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}
		if (!TryResolveCustomOutputType(Function->Results[0].Type, ResultOutputType))
		{
			OutError = FString::Printf(TEXT("DreamShader Function '%s' has unsupported result type '%s'."), *FunctionName, *Function->Results[0].Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		int32 ResultComponents = 1;
		verify(TryGetComponentCountForOutputType(ResultOutputType, ResultComponents));

		TArray<FCodeValue> InputValues;
		InputValues.Reserve(Function->Inputs.Num());
		for (int32 InputIndex = 0; InputIndex < Function->Inputs.Num(); ++InputIndex)
		{
			const FTextShaderFunctionParameter& InputDefinition = Function->Inputs[InputIndex];
			FCodeValue InputValue;
			if (!EvaluateExpression(Arguments[InputIndex].Expression, InputValue, OutError))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s': %s"), *FunctionName, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			int32 ExpectedComponentCount = 1;
			bool bExpectedTexture = false;
			bool bExpectedSubstrate = false;
			ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
			if (!TryResolveCodeDeclaredType(InputDefinition.Type, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate))
			{
				if (IsSubstrateTypeUnsupportedForEngine(InputDefinition.Type))
				{
					OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *FunctionName, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s' uses unsupported type '%s'."), *FunctionName, *InputDefinition.Name, *InputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (bExpectedSubstrate)
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead."), *FunctionName, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue CoercedValue;
			if (!CoerceValueToType(InputValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedValue, OutError))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s': %s"), *FunctionName, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			InputValues.Add(CoercedValue);
		}

		auto* CustomExpression = Cast<UMaterialExpressionCustom>(
			CreateExpression(UMaterialExpressionCustom::StaticClass(), 640, ConsumeNodeY()));
		if (!CustomExpression)
		{
			OutError = FString::Printf(TEXT("Failed to create a Custom node for DreamShader Function '%s'."), *FunctionName); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		CustomExpression->Description = Function->Name;
		CustomExpression->OutputType = ResultOutputType;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
		CustomExpression->ShowCode = false;
#endif
		CustomExpression->Inputs.Reset();
		CustomExpression->AdditionalOutputs.Reset();
		CustomExpression->IncludeFilePaths.Reset();

		for (int32 InputIndex = 0; InputIndex < Function->Inputs.Num(); ++InputIndex)
		{
			FCustomInput Input;
			Input.InputName = FName(*Function->Inputs[InputIndex].Name);
			CustomExpression->Inputs.Add(Input);
			ConnectCodeValueToInput(CustomExpression->Inputs.Last().Input, InputValues[InputIndex]);
		}

		const FString CustomCode = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("return %s(%s);"),
			*BuildGeneratedFunctionSymbolName(*Function),
			*(Function->bSelfContained
				? BuildFunctionSourceArgumentList(*Function, TArray<FString>())
				: BuildFunctionArgumentList(*Function, TArray<FString>())));

		if (Function->bSelfContained)
		{
			TArray<FString> EmbeddedFunctionNames;
			EmbeddedFunctionNames.Add(Function->Name);

			FString PreparedCustomCode;
			bool bUsesGeneratedInclude = false;
			if (!PrepareCustomNodeCode(
				Definition,
				CustomCode,
				EmbeddedFunctionNames,
				Function->Name,
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

		RebuildDreamShaderCustomOutputs(CustomExpression);

		OutValue.Expression = CustomExpression;
		OutValue.OutputIndex = 0;
		OutValue.ComponentCount = ResultComponents;
		OutValue.bIsTextureObject = false;
		OutValue.bIsMaterialAttributes = IsMaterialAttributesComponentType(ResultComponents, false);
		return true;
	}

	bool FCodeGraphBuilder::EvaluateGraphFunctionCall(
		const FTextShaderFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (!Values)
		{
			OutError = TEXT("GraphFunction value call requires an active Graph build context.");
			return false;
		}

		if (Function.Results.Num() != 1)
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader GraphFunction '%s' has %d outputs and must be called with explicit out variables, for example %s(..., ResultA, ResultB)."),
				*Function.Name,
				Function.Results.Num(),
				*Function.Name);
			return false;
		}

		if (Arguments.Num() != Function.Inputs.Num())
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader GraphFunction '%s' returns one value and expects %d input argument(s) when used as a value expression, but got %d."),
				*Function.Name,
				Function.Inputs.Num(),
				Arguments.Num());
			return false;
		}

		for (const FCodeCallArgument& Argument : Arguments)
		{
			if (Argument.bIsNamed)
			{
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' currently uses positional arguments only."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		FString TempResultName;
		for (int32 Attempt = 0; Attempt < 1024; ++Attempt)
		{
			TempResultName = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("__ds_%s_value%d"),
				*UE::DreamShader::SanitizeIdentifier(Function.Name),
				Values->Num() + Attempt);
			if (!FindValue(TempResultName))
			{
				break;
			}
		}

		TArray<FCodeCallArgument> ExpandedArguments = Arguments;
		FCodeCallArgument ResultArgument;
		ResultArgument.Expression = MakeShared<FCodeExpression>();
		ResultArgument.Expression->Kind = ECodeExpressionKind::Name;
		ResultArgument.Expression->Text = TempResultName;
		ExpandedArguments.Add(ResultArgument);

		if (!ExecuteGraphFunctionCall(Function, ExpandedArguments, OutError))
		{
			return false;
		}

		if (const FCodeValue* ResultValue = FindValue(TempResultName))
		{
			OutValue = *ResultValue;
			return true;
		}

		OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' did not produce a value result."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
		return false;
	}

	bool FCodeGraphBuilder::ExecuteCustomFunctionCall(
		const FTextShaderFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FString& OutError)
	{
		if (Function.Results.IsEmpty())
		{
			OutError = FString::Printf(TEXT("DreamShader Function '%s' must declare at least one out result."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		const int32 ExpectedArgumentCount = Function.Inputs.Num() + Function.Results.Num();
		if (Arguments.Num() != ExpectedArgumentCount)
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
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
				OutError = FString::Printf(TEXT("DreamShader Function '%s' currently uses positional arguments only."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		ECustomMaterialOutputType PrimaryOutputType = CMOT_Float1;
		if (IsSubstrateMaterialType(Function.Results[0].Type))
		{
			OutError = IsSubstrateMaterialTypeSupported()
				? FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead."), *Function.Name, *Function.Results[0].Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
				: FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *Function.Results[0].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}
		if (!TryResolveCustomOutputType(Function.Results[0].Type, PrimaryOutputType))
		{
			OutError = FString::Printf(TEXT("DreamShader Function '%s' has unsupported result type '%s'."), *Function.Name, *Function.Results[0].Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		int32 PrimaryOutputComponents = 1;
		verify(TryGetComponentCountForOutputType(PrimaryOutputType, PrimaryOutputComponents));

		TArray<FCodeValue> InputValues;
		InputValues.Reserve(Function.Inputs.Num());
		for (int32 InputIndex = 0; InputIndex < Function.Inputs.Num(); ++InputIndex)
		{
			const FTextShaderFunctionParameter& InputDefinition = Function.Inputs[InputIndex];
			FCodeValue InputValue;
			if (!EvaluateExpression(Arguments[InputIndex].Expression, InputValue, OutError))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s': %s"), *Function.Name, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			int32 ExpectedComponentCount = 1;
			bool bExpectedTexture = false;
			bool bExpectedSubstrate = false;
			ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
			if (!TryResolveCodeDeclaredType(InputDefinition.Type, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate))
			{
				if (IsSubstrateTypeUnsupportedForEngine(InputDefinition.Type))
				{
					OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s' uses unsupported type '%s'."), *Function.Name, *InputDefinition.Name, *InputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (bExpectedSubstrate)
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead."), *Function.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue CoercedValue;
			if (!CoerceValueToType(InputValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedValue, OutError))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' input '%s': %s"), *Function.Name, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			InputValues.Add(CoercedValue);
		}

		TArray<FString> ResultTargetNames;
		ResultTargetNames.Reserve(Function.Results.Num());
		TSet<FString> SeenTargetNames;
		for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			const FCodeCallArgument& Argument = Arguments[Function.Inputs.Num() + ResultIndex];
			if (!Argument.Expression || Argument.Expression->Kind != ECodeExpressionKind::Name)
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader Function '%s' out argument %d must be a plain variable name."),
					*Function.Name,
					ResultIndex + 1);
				return false;
			}

			const FString TargetName = Argument.Expression->Text.TrimStartAndEnd();
			if (TargetName.IsEmpty())
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' has an empty out target name."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			if (SeenTargetNames.Contains(TargetName))
			{
				OutError = FString::Printf(TEXT("DreamShader Function '%s' cannot write multiple out results into '%s' in the same call."), *Function.Name, *TargetName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			SeenTargetNames.Add(TargetName);
			ResultTargetNames.Add(TargetName);
		}

		auto* CustomExpression = Cast<UMaterialExpressionCustom>(
			CreateExpression(UMaterialExpressionCustom::StaticClass(), 640, ConsumeNodeY()));
		if (!CustomExpression)
		{
			OutError = FString::Printf(TEXT("Failed to create a Custom node for DreamShader Function '%s'."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		CustomExpression->Description = Function.Name;
		CustomExpression->OutputType = PrimaryOutputType;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
		CustomExpression->ShowCode = false;
#endif
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
			const FString TempName = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("__ds_%s_out%d"),
				*UE::DreamShader::SanitizeIdentifier(Function.Name),
				ResultIndex);
			ResultVariableNames.Add(TempName);
			CustomCode += FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
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

		if (Function.bSelfContained)
		{
			CustomCode += FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("%s(%s);\n"),
				*BuildGeneratedFunctionSymbolName(Function),
				*BuildFunctionSourceArgumentList(Function, ResultVariableNames));
		}
		else
		{
			CustomCode += FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("%s = %s(%s);\n"),
				*ResultVariableNames[0],
				*BuildGeneratedFunctionSymbolName(Function),
				*BuildFunctionArgumentList(Function, SecondaryResultVariables));
		}

		for (int32 ResultIndex = 1; ResultIndex < ResultVariableNames.Num(); ++ResultIndex)
		{
			CustomCode += FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("%s = %s;\n"),
				*ResultTargetNames[ResultIndex],
				*ResultVariableNames[ResultIndex]);
		}

		CustomCode += FString::Printf(TEXT("return %s;"), *ResultVariableNames[0]); /* I18N-EXEMPT: deferred codegen or compatibility path */

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
			if (IsSubstrateMaterialType(Function.Results[ResultIndex].Type))
			{
				OutError = IsSubstrateMaterialTypeSupported()
					? FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead."), *Function.Name, *Function.Results[ResultIndex].Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
					: FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *Function.Results[ResultIndex].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (!TryResolveCustomOutputType(Function.Results[ResultIndex].Type, AdditionalOutputType))
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
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

		RebuildDreamShaderCustomOutputs(CustomExpression);

		for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			ECustomMaterialOutputType ResultOutputType = CMOT_Float1;
			if (IsSubstrateMaterialType(Function.Results[ResultIndex].Type))
			{
				OutError = IsSubstrateMaterialTypeSupported()
					? FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which is not supported by HLSL Custom node functions. Use GraphFunction or ShaderFunction instead."), *Function.Name, *Function.Results[ResultIndex].Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
					: FString::Printf(TEXT("DreamShader Function '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *Function.Results[ResultIndex].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (!TryResolveCustomOutputType(Function.Results[ResultIndex].Type, ResultOutputType))
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
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

	bool FCodeGraphBuilder::ExecuteGraphFunctionCall(
		const FTextShaderFunctionDefinition& Function,
		const TArray<FCodeCallArgument>& Arguments,
		FString& OutError)
	{
		if (!Values)
		{
			OutError = TEXT("GraphFunction call requires an active Graph build context.");
			return false;
		}

		if (Function.Results.IsEmpty())
		{
			OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' must declare at least one out result."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		static TArray<FString> ActiveGraphFunctionStack;
		if (ActiveGraphFunctionStack.ContainsByPredicate([&Function](const FString& ActiveName)
			{
				return ActiveName.Equals(Function.Name, ESearchCase::IgnoreCase);
			}))
		{
			TArray<FString> Cycle = ActiveGraphFunctionStack;
			Cycle.Add(Function.Name);
			OutError = FString::Printf(TEXT("GraphFunction cycle detected: %s."), *FString::Join(Cycle, TEXT(" -> "))); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		struct FActiveGraphFunctionGuard
		{
			TArray<FString>& Stack;
			explicit FActiveGraphFunctionGuard(TArray<FString>& InStack, const FString& Name)
				: Stack(InStack)
			{
				Stack.Add(Name);
			}
			~FActiveGraphFunctionGuard()
			{
				Stack.Pop(DREAMSHADER_ALLOW_SHRINKING_NO);
			}
		};
		FActiveGraphFunctionGuard ActiveGuard(ActiveGraphFunctionStack, Function.Name);

		const int32 ExpectedArgumentCount = Function.Inputs.Num() + Function.Results.Num();
		if (Arguments.Num() != ExpectedArgumentCount)
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader GraphFunction '%s' expects %d arguments (%d inputs, %d out targets) but got %d."),
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
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' currently uses positional arguments only."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
		}

		if (Function.HLSL.IsEmpty())
		{
			TArray<FCodeValue> InputValues;
			InputValues.Reserve(Function.Inputs.Num());
			TMap<FString, FCodeValue> LocalValues = *Values;
			for (int32 InputIndex = 0; InputIndex < Function.Inputs.Num(); ++InputIndex)
			{
				const FTextShaderFunctionParameter& InputDefinition = Function.Inputs[InputIndex];
				FCodeValue InputValue;
				if (!EvaluateExpression(Arguments[InputIndex].Expression, InputValue, OutError))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s': %s"), *Function.Name, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				int32 ExpectedComponentCount = 1;
				bool bExpectedTexture = false;
				bool bExpectedSubstrate = false;
				ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
				if (!TryResolveCodeDeclaredType(InputDefinition.Type, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate))
				{
					if (IsSubstrateTypeUnsupportedForEngine(InputDefinition.Type))
					{
						OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s' uses unsupported type '%s'."), *Function.Name, *InputDefinition.Name, *InputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				FCodeValue CoercedValue;
				if (!CoerceValueToType(InputValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedValue, OutError))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s': %s"), *Function.Name, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				LocalValues.Add(InputDefinition.Name, CoercedValue);
				InputValues.Add(CoercedValue);
			}

			TArray<FString> ResultTargetNames;
			ResultTargetNames.Reserve(Function.Results.Num());
			TSet<FString> SeenTargetNames;
			for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
			{
				const FCodeCallArgument& Argument = Arguments[Function.Inputs.Num() + ResultIndex];
				if (!Argument.Expression || Argument.Expression->Kind != ECodeExpressionKind::Name)
				{
					OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
						TEXT("DreamShader GraphFunction '%s' out argument %d must be a plain variable name."),
						*Function.Name,
						ResultIndex + 1);
					return false;
				}

				const FString TargetName = Argument.Expression->Text.TrimStartAndEnd();
				if (TargetName.IsEmpty())
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' has an empty out target name."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				if (SeenTargetNames.Contains(TargetName))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' cannot write multiple out results into '%s' in the same call."), *Function.Name, *TargetName); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				SeenTargetNames.Add(TargetName);
				ResultTargetNames.Add(TargetName);
			}

			TMap<FString, FCodeValue>* PreviousValues = Values;
			struct FScopedGraphFunctionValues
			{
				TMap<FString, FCodeValue>*& ValuesRef;
				TMap<FString, FCodeValue>* Previous;
				FScopedGraphFunctionValues(TMap<FString, FCodeValue>*& InValuesRef, TMap<FString, FCodeValue>* NewValues)
					: ValuesRef(InValuesRef)
					, Previous(InValuesRef)
				{
					ValuesRef = NewValues;
				}
				~FScopedGraphFunctionValues()
				{
					ValuesRef = Previous;
				}
			};

			TArray<FCodeStatement> Statements;
			FString ParseError;
			if (!ParseCodeStatements(Function.HLSL, Statements, ParseError))
			{
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' Graph body is invalid: %s"), *Function.Name, *ParseError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			{
				FScopedGraphFunctionValues ScopedValues(Values, &LocalValues);
				for (const FCodeStatement& Statement : Statements)
				{
					if (!ExecuteStatement(Statement, OutError))
					{
						OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s': %s"), *Function.Name, *FormatStatementError(Statement, OutError)); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
				}
			}

			if (PreviousValues)
			{
				for (const TPair<FString, FCodeValue>& Pair : LocalValues)
				{
					if (!PreviousValues->Contains(Pair.Key) && FindPropertyDefinition(Pair.Key))
					{
						PreviousValues->Add(Pair.Key, Pair.Value);
					}
				}
			}

			for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
			{
				const FTextShaderFunctionParameter& ResultDefinition = Function.Results[ResultIndex];
				const FCodeValue* ResultValue = LocalValues.Find(ResultDefinition.Name);
				if (!ResultValue || !ResultValue->Expression)
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' was never assigned."), *Function.Name, *ResultDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				int32 ExpectedComponentCount = 1;
				bool bExpectedTexture = false;
				bool bExpectedSubstrate = false;
				ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
				if (!TryResolveCodeDeclaredType(ResultDefinition.Type, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate))
				{
					if (IsSubstrateTypeUnsupportedForEngine(ResultDefinition.Type))
					{
						OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *ResultDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
						return false;
					}
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses unsupported type '%s'."), *Function.Name, *ResultDefinition.Name, *ResultDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				FCodeValue CoercedResult;
				if (!CoerceValueToType(*ResultValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedResult, OutError))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s': %s"), *Function.Name, *ResultDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				PreviousValues->Add(ResultTargetNames[ResultIndex], CoercedResult);
			}

			return true;
		}

		ECustomMaterialOutputType PrimaryOutputType = CMOT_Float1;
		if (IsSubstrateMaterialType(Function.Results[0].Type))
		{
			OutError = IsSubstrateMaterialTypeSupported()
				? FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses Substrate, which is only supported by GraphFunction Graph blocks."), *Function.Name, *Function.Results[0].Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
				: FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *Function.Results[0].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}
		if (!TryResolveCustomOutputType(Function.Results[0].Type, PrimaryOutputType))
		{
			OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' has unsupported result type '%s'."), *Function.Name, *Function.Results[0].Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		int32 PrimaryOutputComponents = 1;
		verify(TryGetComponentCountForOutputType(PrimaryOutputType, PrimaryOutputComponents));

		TArray<FCodeValue> InputValues;
		InputValues.Reserve(Function.Inputs.Num());
		TMap<FString, FCodeValue> LocalValues = *Values;
		for (int32 InputIndex = 0; InputIndex < Function.Inputs.Num(); ++InputIndex)
		{
			const FTextShaderFunctionParameter& InputDefinition = Function.Inputs[InputIndex];
			FCodeValue InputValue;
			if (!EvaluateExpression(Arguments[InputIndex].Expression, InputValue, OutError))
			{
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s': %s"), *Function.Name, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			int32 ExpectedComponentCount = 1;
			bool bExpectedTexture = false;
			bool bExpectedSubstrate = false;
			ETextShaderTextureType ExpectedTextureType = ETextShaderTextureType::Texture2D;
			if (!TryResolveCodeDeclaredType(InputDefinition.Type, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate))
			{
				if (IsSubstrateTypeUnsupportedForEngine(InputDefinition.Type))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s' uses unsupported type '%s'."), *Function.Name, *InputDefinition.Name, *InputDefinition.Type); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (bExpectedSubstrate)
			{
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s' uses Substrate, which is only supported by GraphFunction Graph blocks."), *Function.Name, *InputDefinition.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue CoercedValue;
			if (!CoerceValueToType(InputValue, ExpectedComponentCount, bExpectedTexture, ExpectedTextureType, bExpectedSubstrate, CoercedValue, OutError))
			{
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' input '%s': %s"), *Function.Name, *InputDefinition.Name, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			LocalValues.Add(InputDefinition.Name, CoercedValue);
			InputValues.Add(CoercedValue);
		}

		TArray<FString> ResultTargetNames;
		ResultTargetNames.Reserve(Function.Results.Num());
		TSet<FString> SeenTargetNames;
		for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			const FCodeCallArgument& Argument = Arguments[Function.Inputs.Num() + ResultIndex];
			if (!Argument.Expression || Argument.Expression->Kind != ECodeExpressionKind::Name)
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader GraphFunction '%s' out argument %d must be a plain variable name."),
					*Function.Name,
					ResultIndex + 1);
				return false;
			}

			const FString TargetName = Argument.Expression->Text.TrimStartAndEnd();
			if (TargetName.IsEmpty())
			{
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' has an empty out target name."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			if (SeenTargetNames.Contains(TargetName))
			{
				OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' cannot write multiple out results into '%s' in the same call."), *Function.Name, *TargetName); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			SeenTargetNames.Add(TargetName);
			ResultTargetNames.Add(TargetName);
		}

		auto* CustomExpression = Cast<UMaterialExpressionCustom>(
			CreateExpression(UMaterialExpressionCustom::StaticClass(), 640, ConsumeNodeY()));
		if (!CustomExpression)
		{
			OutError = FString::Printf(TEXT("Failed to create a Custom node for DreamShader GraphFunction '%s'."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		CustomExpression->Description = Function.Name;
		CustomExpression->OutputType = PrimaryOutputType;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
		CustomExpression->ShowCode = false;
#endif
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

		TSet<FString> CustomInputNames;
		for (const FTextShaderFunctionParameter& InputDefinition : Function.Inputs)
		{
			CustomInputNames.Add(UE::DreamShader::NormalizeSettingKey(InputDefinition.Name));
		}

		auto AddCustomInputValue = [&CustomExpression, &CustomInputNames](const FString& BaseName, const FCodeValue& Value) -> FString
		{
			FString InputName = UE::DreamShader::SanitizeIdentifier(BaseName);
			if (InputName.IsEmpty())
			{
				InputName = TEXT("__ds_input");
			}

			const FString OriginalInputName = InputName;
			for (int32 Attempt = 0; CustomInputNames.Contains(UE::DreamShader::NormalizeSettingKey(InputName)); ++Attempt)
			{
				InputName = FString::Printf(TEXT("%s_%d"), *OriginalInputName, Attempt + 1); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}

			FCustomInput Input;
			Input.InputName = FName(*InputName);
			CustomExpression->Inputs.Add(Input);
			ConnectCodeValueToInput(CustomExpression->Inputs.Last().Input, Value);
			CustomInputNames.Add(UE::DreamShader::NormalizeSettingKey(InputName));
			return InputName;
		};

		TMap<FString, FCodeValue>* PreviousValues = Values;
		struct FScopedCodeValues
		{
			TMap<FString, FCodeValue>*& ValuesRef;
			TMap<FString, FCodeValue>* Previous;
			FScopedCodeValues(TMap<FString, FCodeValue>*& InValuesRef, TMap<FString, FCodeValue>* NewValues)
				: ValuesRef(InValuesRef)
				, Previous(InValuesRef)
			{
				ValuesRef = NewValues;
			}
			~FScopedCodeValues()
			{
				ValuesRef = Previous;
			}
		};

		auto RewriteUEInputs = [this, &Function, &AddCustomInputValue, &LocalValues, &OutError](const FString& SourceCode, FString& OutRewrittenCode) -> bool
		{
			OutRewrittenCode.Reset();
			OutRewrittenCode.Reserve(SourceCode.Len());

			FScopedCodeValues ScopedValues(Values, &LocalValues);
			bool bInString = false;
			bool bInChar = false;
			bool bInLineComment = false;
			bool bInBlockComment = false;
			int32 AutoInputIndex = 0;

			for (int32 Index = 0; Index < SourceCode.Len(); ++Index)
			{
				const TCHAR Char = SourceCode[Index];

				if (bInLineComment)
				{
					OutRewrittenCode.AppendChar(Char);
					if (Char == TCHAR('\n'))
					{
						bInLineComment = false;
					}
					continue;
				}

				if (bInBlockComment)
				{
					OutRewrittenCode.AppendChar(Char);
					if (Char == TCHAR('*') && SourceCode.IsValidIndex(Index + 1) && SourceCode[Index + 1] == TCHAR('/'))
					{
						OutRewrittenCode.AppendChar(SourceCode[++Index]);
						bInBlockComment = false;
					}
					continue;
				}

				if (bInString || bInChar)
				{
					OutRewrittenCode.AppendChar(Char);
					if (Char == TCHAR('\\') && SourceCode.IsValidIndex(Index + 1))
					{
						OutRewrittenCode.AppendChar(SourceCode[++Index]);
						continue;
					}
					if (bInString && Char == TCHAR('"'))
					{
						bInString = false;
					}
					else if (bInChar && Char == TCHAR('\''))
					{
						bInChar = false;
					}
					continue;
				}

				if (Char == TCHAR('/') && SourceCode.IsValidIndex(Index + 1))
				{
					if (SourceCode[Index + 1] == TCHAR('/'))
					{
						OutRewrittenCode.AppendChar(Char);
						OutRewrittenCode.AppendChar(SourceCode[++Index]);
						bInLineComment = true;
						continue;
					}
					if (SourceCode[Index + 1] == TCHAR('*'))
					{
						OutRewrittenCode.AppendChar(Char);
						OutRewrittenCode.AppendChar(SourceCode[++Index]);
						bInBlockComment = true;
						continue;
					}
				}

				if (Char == TCHAR('"'))
				{
					OutRewrittenCode.AppendChar(Char);
					bInString = true;
					continue;
				}

				if (Char == TCHAR('\''))
				{
					OutRewrittenCode.AppendChar(Char);
					bInChar = true;
					continue;
				}

				const bool bCanStartUECall =
					FChar::ToUpper(Char) == TCHAR('U')
					&& SourceCode.IsValidIndex(Index + 2)
					&& FChar::ToUpper(SourceCode[Index + 1]) == TCHAR('E')
					&& SourceCode[Index + 2] == TCHAR('.')
					&& IsIdentifierBoundary(SourceCode, Index - 1);
				if (!bCanStartUECall)
				{
					OutRewrittenCode.AppendChar(Char);
					continue;
				}

				int32 Cursor = Index + 3;
				if (!SourceCode.IsValidIndex(Cursor) || !(FChar::IsAlpha(SourceCode[Cursor]) || SourceCode[Cursor] == TCHAR('_')))
				{
					OutRewrittenCode.AppendChar(Char);
					continue;
				}

				++Cursor;
				while (SourceCode.IsValidIndex(Cursor) && (FChar::IsAlnum(SourceCode[Cursor]) || SourceCode[Cursor] == TCHAR('_')))
				{
					++Cursor;
				}

				SkipWhitespace(SourceCode, Cursor);
				if (!SourceCode.IsValidIndex(Cursor) || SourceCode[Cursor] != TCHAR('('))
				{
					OutRewrittenCode.AppendChar(Char);
					continue;
				}

				int32 CloseIndex = INDEX_NONE;
				if (!FindMatchingDelimiter(SourceCode, Cursor, TCHAR('('), TCHAR(')'), CloseIndex))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' contains an unterminated UE.* call."), *Function.Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				const FString CallText = SourceCode.Mid(Index, CloseIndex - Index + 1);
				TSharedPtr<FCodeExpression> Expression;
				if (!ParseCodeExpression(CallText, Expression, OutError))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' UE input '%s': %s"), *Function.Name, *CallText, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				FCodeValue UEValue;
				if (!EvaluateExpression(Expression, UEValue, OutError))
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' UE input '%s': %s"), *Function.Name, *CallText, *OutError); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				if (UEValue.bIsTextureObject || UEValue.bIsMaterialAttributes || UEValue.bIsSubstrateMaterial)
				{
					OutError = FString::Printf(TEXT("DreamShader GraphFunction '%s' UE input '%s' cannot be passed into a Custom node input."), *Function.Name, *CallText); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}

				const FString InputName = AddCustomInputValue(
					FString::Printf(TEXT("__ds_%s_UE%d"), *UE::DreamShader::SanitizeIdentifier(Function.Name), AutoInputIndex++), /* I18N-EXEMPT: deferred codegen or compatibility path */
					UEValue);
				OutRewrittenCode += InputName;
				Index = CloseIndex;
			}

			return true;
		};

		FString RewrittenHLSL;
		if (!RewriteUEInputs(Function.HLSL, RewrittenHLSL))
		{
			return false;
		}

		if (PreviousValues)
		{
			for (const TPair<FString, FCodeValue>& Pair : LocalValues)
			{
				if (!PreviousValues->Contains(Pair.Key) && FindPropertyDefinition(Pair.Key))
				{
					PreviousValues->Add(Pair.Key, Pair.Value);
				}
			}
		}

		FString CustomCode;
		for (const FTextShaderFunctionParameter& ResultDefinition : Function.Results)
		{
			CustomCode += FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("%s %s = (%s)0;\n"),
				*ResultDefinition.Type,
				*ResultDefinition.Name,
				*ResultDefinition.Type);
		}

		CustomCode += RewrittenHLSL;
		if (!CustomCode.EndsWith(TEXT("\n")))
		{
			CustomCode += TEXT("\n");
		}

		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			CustomCode += FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("%s = %s;\n"),
				*ResultTargetNames[ResultIndex],
				*Function.Results[ResultIndex].Name);
		}

		CustomCode += FString::Printf(TEXT("return %s;"), *Function.Results[0].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */

		FString PreparedCustomCode;
		bool bUsesGeneratedInclude = false;
		if (!PrepareCustomNodeCode(
			Definition,
			CustomCode,
			TArray<FString>(),
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

		for (int32 ResultIndex = 1; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			ECustomMaterialOutputType AdditionalOutputType = CMOT_Float1;
			if (IsSubstrateMaterialType(Function.Results[ResultIndex].Type))
			{
				OutError = IsSubstrateMaterialTypeSupported()
					? FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses Substrate, which is only supported by GraphFunction Graph blocks."), *Function.Name, *Function.Results[ResultIndex].Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
					: FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *Function.Results[ResultIndex].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (!TryResolveCustomOutputType(Function.Results[ResultIndex].Type, AdditionalOutputType))
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader GraphFunction '%s' has unsupported result type '%s'."),
					*Function.Name,
					*Function.Results[ResultIndex].Type);
				return false;
			}

			FCustomOutput Output;
			Output.OutputName = FName(*ResultTargetNames[ResultIndex]);
			Output.OutputType = AdditionalOutputType;
			CustomExpression->AdditionalOutputs.Add(Output);
		}

		RebuildDreamShaderCustomOutputs(CustomExpression);

		for (int32 ResultIndex = 0; ResultIndex < Function.Results.Num(); ++ResultIndex)
		{
			ECustomMaterialOutputType ResultOutputType = CMOT_Float1;
			if (IsSubstrateMaterialType(Function.Results[ResultIndex].Type))
			{
				OutError = IsSubstrateMaterialTypeSupported()
					? FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses Substrate, which is only supported by GraphFunction Graph blocks."), *Function.Name, *Function.Results[ResultIndex].Name) /* I18N-EXEMPT: deferred codegen or compatibility path */
					: FString::Printf(TEXT("DreamShader GraphFunction '%s' result '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Function.Name, *Function.Results[ResultIndex].Name); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (!TryResolveCustomOutputType(Function.Results[ResultIndex].Type, ResultOutputType))
			{
				OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
					TEXT("DreamShader GraphFunction '%s' has unsupported result type '%s'."),
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
			PreviousValues->Add(ResultTargetNames[ResultIndex], ResultValue);
		}

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
}
