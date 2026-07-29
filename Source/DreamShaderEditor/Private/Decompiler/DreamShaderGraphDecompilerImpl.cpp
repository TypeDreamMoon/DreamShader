// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Out-of-line member definitions for FDreamShaderGraphDecompiler (DreamShaderGraphDecompilerImpl.h),
// hoisted from DreamShaderGraphDecompiler.cpp.

#include "DreamShaderGraphDecompilerImpl.h"

namespace UE::DreamShader::Editor::Private
{
	bool FDreamShaderGraphDecompiler::DecompileMaterial(UMaterial* Material, const FString& DecompiledName, FString& OutSourceText, FString& OutError)
	{
		Reset();
		if (!Material)
		{
			OutError = TEXT("No Material asset was provided.");
			return false;
		}

		FScopedSlowTask DecompileSlowTask(
			FMath::Max(4.0f, static_cast<float>(Material->GetExpressions().Num()) + 4.0f),
			FText::FromString(FString::Printf(TEXT("Decompiling Material '%s'..."), *Material->GetName())));
		if (!IsRunningCommandlet())
		{
			DecompileSlowTask.MakeDialogDelayed(0.25f);
		}
		ActiveDecompileSlowTask = &DecompileSlowTask;
		ON_SCOPE_EXIT
		{
			ActiveDecompileSlowTask = nullptr;
		};
		CollectLayoutComments(Material);

		DecompileSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Scanning material outputs for '%s'..."), *Material->GetName())));
		TArray<FString> OutputDeclarations;
		TArray<FString> OutputBindings;
		TArray<FString> OutputAssignments;
		bool bUsesFrontMaterial = false;

		struct FMaterialOutputBinding
		{
			EMaterialProperty Property;
			const TCHAR* Name;
			const TCHAR* Type;
			const TCHAR* Target;
			const TCHAR* DefaultValue;
		};

		const FMaterialOutputBinding Bindings[] =
		{
			{ MP_EmissiveColor, TEXT("EmissiveColor"), TEXT("float3"), TEXT("Base.EmissiveColor"), TEXT("float3(0, 0, 0)") },
			{ MP_BaseColor, TEXT("BaseColor"), TEXT("float3"), TEXT("Base.BaseColor"), TEXT("float3(0.8, 0.8, 0.8)") },
			{ MP_Metallic, TEXT("Metallic"), TEXT("float"), TEXT("Base.Metallic"), TEXT("0.0") },
			{ MP_Specular, TEXT("Specular"), TEXT("float"), TEXT("Base.Specular"), TEXT("0.5") },
			{ MP_Roughness, TEXT("Roughness"), TEXT("float"), TEXT("Base.Roughness"), TEXT("0.5") },
			{ MP_Anisotropy, TEXT("Anisotropy"), TEXT("float"), TEXT("Base.Anisotropy"), TEXT("0.0") },
			{ MP_Opacity, TEXT("Opacity"), TEXT("float"), TEXT("Base.Opacity"), TEXT("1.0") },
			{ MP_OpacityMask, TEXT("OpacityMask"), TEXT("float"), TEXT("Base.OpacityMask"), TEXT("1.0") },
			{ MP_Normal, TEXT("Normal"), TEXT("float3"), TEXT("Base.Normal"), TEXT("float3(0, 0, 1)") },
			{ MP_Tangent, TEXT("Tangent"), TEXT("float3"), TEXT("Base.Tangent"), TEXT("float3(1, 0, 0)") },
			{ MP_WorldPositionOffset, TEXT("WorldPositionOffset"), TEXT("float3"), TEXT("Base.WorldPositionOffset"), TEXT("float3(0, 0, 0)") },
			{ MP_SubsurfaceColor, TEXT("SubsurfaceColor"), TEXT("float3"), TEXT("Base.SubsurfaceColor"), TEXT("float3(1, 1, 1)") },
			{ MP_CustomData0, TEXT("CustomData0"), TEXT("float"), TEXT("Base.CustomData0"), TEXT("0.0") },
			{ MP_CustomData1, TEXT("CustomData1"), TEXT("float"), TEXT("Base.CustomData1"), TEXT("0.0") },
			{ MP_AmbientOcclusion, TEXT("AmbientOcclusion"), TEXT("float"), TEXT("Base.AmbientOcclusion"), TEXT("1.0") },
			{ MP_Refraction, TEXT("Refraction"), TEXT("float"), TEXT("Base.Refraction"), TEXT("0.0") },
			{ MP_PixelDepthOffset, TEXT("PixelDepthOffset"), TEXT("float"), TEXT("Base.PixelDepthOffset"), TEXT("0.0") },
			{ MP_MaterialAttributes, TEXT("MaterialAttributes"), TEXT("MaterialAttributes"), TEXT("Base.MaterialAttributes"), TEXT("MaterialAttributes()") },
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
			{ MP_FrontMaterial, TEXT("FrontMaterial"), TEXT("Substrate"), TEXT("Base.FrontMaterial"), TEXT("default") },
#endif
		};

		for (const FMaterialOutputBinding& Binding : Bindings)
		{
			FExpressionInput* MaterialInput = GetMaterialInputForDecompile(Material, Binding.Property);
			if (!MaterialInput || !MaterialInput->IsConnected())
			{
				continue;
			}

			ReservedNames.Add(Binding.Name);
			OutputDeclarations.Add(FString::Printf(TEXT("\t\t%s %s;"), Binding.Type, Binding.Name));
			OutputBindings.Add(FString::Printf(TEXT("\t\t%s = %s;"), Binding.Target, Binding.Name));
			if (Binding.Property == MP_FrontMaterial)
			{
				bUsesFrontMaterial = true;
			}
		}

		for (const FMaterialOutputBinding& Binding : Bindings)
		{
			FExpressionInput* MaterialInput = GetMaterialInputForDecompile(Material, Binding.Property);
			if (!MaterialInput || !MaterialInput->IsConnected())
			{
				continue;
			}

			OutputAssignments.Add(FormatGraphSetStatement(Binding.Name, CompileInput(*MaterialInput, Binding.DefaultValue)));
		}
		FinalizeGraphLayoutMetadata();

		TArray<FString> Lines;
		DecompileSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Formatting DSM source for '%s'..."), *Material->GetName())));
		Lines.Add(FString::Printf(TEXT("// Decompiled from %s"), *Material->GetPathName()));
		if (!Warnings.IsEmpty())
		{
			for (const FString& Warning : Warnings)
			{
				Lines.Add(FString::Printf(TEXT("// Warning: %s"), *Warning));
			}
		}
		if (!VirtualFunctionDefinitions.IsEmpty())
		{
			Lines.Add(TEXT(""));
			Lines.Append(VirtualFunctionDefinitions);
		}

		Lines.Add(FString::Printf(TEXT("Shader(Name=\"%s\")"), *EscapeDreamShaderString(DecompiledName)));
		Lines.Add(TEXT("{"));
		AppendSection(Lines, TEXT("Properties"), PropertyDeclarations);
		Lines.Add(TEXT("\tSettings = {"));
		Lines.Add(FString::Printf(TEXT("\t\tDomain = \"%s\";"), *GetMaterialDomainText(Material->MaterialDomain)));
		// A Substrate material's shading is driven by its FrontMaterial tree, so the UMaterial's
		// ShadingModel enum stays at its default (usually DefaultLit) and does NOT describe the surface.
		// Emit "Substrate" whenever we decompiled a Base.FrontMaterial binding, matching the generator's
		// rule (Base.FrontMaterial requires ShadingModel="Substrate" or none) so the .dsm round-trips.
		const FString ShadingModelText = bUsesFrontMaterial ? FString(TEXT("Substrate")) : GetShadingModelText(Material);
		Lines.Add(FString::Printf(TEXT("\t\tShadingModel = \"%s\";"), *ShadingModelText));
		Lines.Add(FString::Printf(TEXT("\t\tBlendMode = \"%s\";"), *GetBlendModeText(Material->BlendMode)));
		AppendAdditionalMaterialSettings(Lines, Material);
		Lines.Add(TEXT("\t}"));
		Lines.Add(TEXT(""));
		AppendSection(Lines, TEXT("Outputs"), OutputDeclarations, OutputBindings);
		AppendSection(Lines, TEXT("Graph"), BuildRegionizedGraphLines(GraphLines), OutputAssignments);
		if (!LayoutLines.IsEmpty())
		{
			AppendSection(Lines, TEXT("Layout"), LayoutLines);
		}
		Lines.Add(TEXT("}"));

		OutSourceText = FString::Join(Lines, TEXT("\n"));
		return true;
	}

	bool FDreamShaderGraphDecompiler::DecompileFunction( UMaterialFunction* MaterialFunction, const FString& DecompiledName, EDreamShaderDecompiledFunctionKind FunctionKind, FString& OutSourceText, FString& OutError)
	{
		Reset();
		if (!MaterialFunction)
		{
			OutError = TEXT("No MaterialFunction asset was provided.");
			return false;
		}

		FScopedSlowTask DecompileSlowTask(
			FMath::Max(4.0f, static_cast<float>(MaterialFunction->GetExpressions().Num()) + 4.0f),
			FText::FromString(FString::Printf(TEXT("Decompiling Material Function '%s'..."), *MaterialFunction->GetName())));
		if (!IsRunningCommandlet())
		{
			DecompileSlowTask.MakeDialogDelayed(0.25f);
		}
		ActiveDecompileSlowTask = &DecompileSlowTask;
		ON_SCOPE_EXIT
		{
			ActiveDecompileSlowTask = nullptr;
		};
		CollectLayoutComments(MaterialFunction);

		DecompileSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Scanning function inputs and outputs for '%s'..."), *MaterialFunction->GetName())));
		TArray<FFunctionExpressionInput> Inputs;
		TArray<FFunctionExpressionOutput> Outputs;
		MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);
		if (Outputs.IsEmpty())
		{
			OutError = FString::Printf(TEXT("MaterialFunction '%s' does not expose any outputs."), *MaterialFunction->GetName());
			return false;
		}

		TArray<FString> InputDeclarations;
		for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
		{
			const FFunctionExpressionInput& Input = Inputs[InputIndex];
			UMaterialExpressionFunctionInput* InputExpression = Input.ExpressionInput;
			const FString InputName = InputExpression
				? InputExpression->InputName.ToString()
				: Input.Input.InputName.ToString();
			const EFunctionInputType InputType = InputExpression
				? InputExpression->InputType.GetValue()
				: FunctionInput_Vector4;
			const bool bOptional = InputExpression && InputExpression->bUsePreviewValueAsDefault != 0;
			const FString DefaultText = bOptional && InputExpression
				? MakePreviewValueText(InputType, InputExpression->PreviewValue)
				: FString();
			const FString DefaultSuffix = DefaultText.IsEmpty()
				? FString()
				: FString::Printf(TEXT(" = %s"), *DefaultText);
			const FString MetadataSuffix = InputExpression
				? MakeFunctionParameterMetadataSuffix(InputExpression->Description, InputExpression->SortPriority, InputIndex)
				: FString();
			const FString DeclarationName = MakeDreamShaderDeclarationName(InputName, TEXT("Input"), InputIndex);
			if (InputExpression)
			{
				FunctionInputNames.Add(InputExpression, DeclarationName);
				RegisterExpressionName(InputExpression, DeclarationName);
			}
			InputDeclarations.Add(FString::Printf(
				TEXT("\t\t%s%s %s%s%s;"),
				bOptional ? TEXT("opt ") : TEXT(""),
				*GetDreamShaderTypeForFunctionInput(InputType),
				*DeclarationName,
				*DefaultSuffix,
				*MetadataSuffix));
		}

		TArray<FString> OutputDeclarations;
		TArray<FString> OutputAssignments;
		for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
		{
			const FFunctionExpressionOutput& Output = Outputs[OutputIndex];
			UMaterialExpressionFunctionOutput* OutputExpression = Output.ExpressionOutput;
			const FString OutputName = OutputExpression
				? OutputExpression->OutputName.ToString()
				: Output.Output.OutputName.ToString();
			const FString DeclarationName = MakeDreamShaderDeclarationName(OutputName, TEXT("Output"), OutputIndex);
			ReservedNames.Add(DeclarationName);
		}

		for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
		{
			const FFunctionExpressionOutput& Output = Outputs[OutputIndex];
			UMaterialExpressionFunctionOutput* OutputExpression = Output.ExpressionOutput;
			const FString OutputName = OutputExpression
				? OutputExpression->OutputName.ToString()
				: Output.Output.OutputName.ToString();
			const FString DeclarationName = MakeDreamShaderDeclarationName(OutputName, TEXT("Output"), OutputIndex);
			const FString MetadataSuffix = OutputExpression
				? MakeFunctionParameterMetadataSuffix(OutputExpression->Description, OutputExpression->SortPriority, OutputIndex)
				: FString();
			OutputDeclarations.Add(FString::Printf(
				TEXT("\t\t%s %s%s;"),
				*GetDreamShaderTypeForFunctionOutput(OutputExpression),
				*DeclarationName,
				*MetadataSuffix));

			if (OutputExpression && OutputExpression->A.IsConnected())
			{
				OutputAssignments.Add(FormatGraphSetStatement(DeclarationName, CompileInput(OutputExpression->A, TEXT("0.0"))));
			}
			if (OutputExpression)
			{
				RegisterExpressionName(OutputExpression, DeclarationName);
			}
		}
		FinalizeGraphLayoutMetadata();

		TArray<FString> Lines;
		DecompileSlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Formatting DSF source for '%s'..."), *MaterialFunction->GetName())));
		Lines.Add(FString::Printf(TEXT("// Decompiled from %s"), *MaterialFunction->GetPathName()));
		if (!Warnings.IsEmpty())
		{
			for (const FString& Warning : Warnings)
			{
				Lines.Add(FString::Printf(TEXT("// Warning: %s"), *Warning));
			}
		}
		if (!VirtualFunctionDefinitions.IsEmpty())
		{
			Lines.Add(TEXT(""));
			Lines.Append(VirtualFunctionDefinitions);
		}

		const TCHAR* BlockName = TEXT("ShaderFunction");
		if (FunctionKind == EDreamShaderDecompiledFunctionKind::MaterialLayer)
		{
			BlockName = TEXT("ShaderLayer");
		}
		else if (FunctionKind == EDreamShaderDecompiledFunctionKind::MaterialLayerBlend)
		{
			BlockName = TEXT("ShaderLayerBlend");
		}

		Lines.Add(FString::Printf(TEXT("%s(Name=\"%s\")"), BlockName, *EscapeDreamShaderString(DecompiledName)));
		Lines.Add(TEXT("{"));
		AppendSection(Lines, TEXT("Properties"), PropertyDeclarations);
		AppendSection(Lines, TEXT("Inputs"), InputDeclarations);
		AppendSection(Lines, TEXT("Outputs"), OutputDeclarations);
		AppendSection(Lines, TEXT("Graph"), BuildRegionizedGraphLines(GraphLines), OutputAssignments);
		if (!LayoutLines.IsEmpty())
		{
			AppendSection(Lines, TEXT("Layout"), LayoutLines);
		}
		Lines.Add(TEXT("}"));

		OutSourceText = FString::Join(Lines, TEXT("\n"));
		return true;
	}

	void FDreamShaderGraphDecompiler::Reset()
	{
		PropertyDeclarations.Reset();
		PropertyNames.Reset();
		ReservedNames.Reset();
		FunctionInputNames.Reset();
		GraphLines.Reset();
		LayoutLines.Reset();
		ExpressionNames.Reset();
		ExpressionRegionNames.Reset();
		LayoutComments.Reset();
		ExpressionTemps.Reset();
		ExpressionValues.Reset();
		TempNames.Reset();
		VirtualFunctionDefinitions.Reset();
		VirtualFunctionNames.Reset();
		Warnings.Reset();
		NextTempIndex = 0;
		ActiveDecompileSlowTask = nullptr;
		ProgressVisitedExpressions.Reset();
		CompilingExpressionKeys.Reset();
		CompilingNamedRerouteDeclarations.Reset();
	}

	void FDreamShaderGraphDecompiler::AppendSection(TArray<FString>& Lines, const TCHAR* SectionName, const TArray<FString>& LinesA)
	{
		TArray<FString> EmptyLines;
		AppendSection(Lines, SectionName, LinesA, EmptyLines);
	}

	void FDreamShaderGraphDecompiler::AppendSection(TArray<FString>& Lines, const TCHAR* SectionName, const TArray<FString>& LinesA, const TArray<FString>& LinesB)
	{
		Lines.Add(FString::Printf(TEXT("\t%s = {"), SectionName));
		if (LinesA.IsEmpty() && LinesB.IsEmpty())
		{
			Lines.Add(TEXT("\t}"));
			Lines.Add(TEXT(""));
			return;
		}

		Lines.Append(LinesA);
		if (!LinesA.IsEmpty() && !LinesB.IsEmpty())
		{
			Lines.Add(TEXT(""));
		}
		Lines.Append(LinesB);
		Lines.Add(TEXT("\t}"));
		Lines.Add(TEXT(""));
	}

	FString FDreamShaderGraphDecompiler::MakeUniqueName(const FString& DesiredName, const TCHAR* FallbackPrefix)
	{
		FString BaseName = MakeDreamShaderDeclarationName(DesiredName, FallbackPrefix, NextTempIndex);
		FString Candidate = BaseName;
		int32 Suffix = 1;
		while (TempNames.Contains(Candidate)
			|| PropertyNames.Contains(Candidate)
			|| ReservedNames.Contains(Candidate)
			|| ContainsFunctionInputName(Candidate))
		{
			Candidate = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++);
		}

		TempNames.Add(Candidate);
		return Candidate;
	}

	bool FDreamShaderGraphDecompiler::ContainsFunctionInputName(const FString& Name) const
	{
		for (const TPair<const UMaterialExpressionFunctionInput*, FString>& Pair : FunctionInputNames)
		{
			if (Pair.Value.Equals(Name, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	void FDreamShaderGraphDecompiler::AddPropertyDeclaration(const FString& Name, const FString& Declaration)
	{
		if (PropertyNames.Contains(Name))
		{
			return;
		}

		PropertyNames.Add(Name);
		ReservedNames.Add(Name);
		PropertyDeclarations.Add(TEXT("\t\t") + Declaration);
	}

	FString FDreamShaderGraphDecompiler::MakeUniquePropertyName(const FString& DesiredName, const TCHAR* FallbackPrefix)
	{
		FString BaseName = MakeDreamShaderDeclarationName(DesiredName, FallbackPrefix, PropertyNames.Num());
		FString Candidate = BaseName;
		int32 Suffix = 1;
		while (PropertyNames.Contains(Candidate)
			|| ReservedNames.Contains(Candidate)
			|| TempNames.Contains(Candidate)
			|| ContainsFunctionInputName(Candidate))
		{
			Candidate = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++);
		}
		return Candidate;
	}

	void FDreamShaderGraphDecompiler::AddParameterNameMetadataIfNeeded(TArray<FString>& Entries, const FString& DeclarationName, const FName ParameterName)
	{
		if (!ParameterName.IsNone() && !ParameterName.ToString().Equals(DeclarationName, ESearchCase::CaseSensitive))
		{
			Entries.Add(FString::Printf(
				TEXT("ParameterName=\"%s\";"),
				*EscapeDreamShaderString(ParameterName.ToString())));
		}
	}

	FString FDreamShaderGraphDecompiler::BuildMetadataSuffix(const TArray<FString>& Entries)
	{
		if (Entries.IsEmpty())
		{
			return FString();
		}

		TArray<FString> Lines;
		Lines.Reserve(Entries.Num());
		for (const FString& Entry : Entries)
		{
			if (!Entry.TrimStartAndEnd().IsEmpty())
			{
				Lines.Add(TEXT("\t\t\t") + Entry.TrimStartAndEnd());
			}
		}

		return Lines.IsEmpty()
			? FString()
			: FString::Printf(TEXT(" [\n%s\n\t\t]"), *FString::Join(Lines, TEXT("\n")));
	}

	void FDreamShaderGraphDecompiler::AddStringMetadata(TArray<FString>& Entries, const TCHAR* Key, const FString& Value)
	{
		if (!Value.TrimStartAndEnd().IsEmpty())
		{
			Entries.Add(FString::Printf(TEXT("%s=\"%s\";"), Key, *EscapeDreamShaderString(Value.TrimStartAndEnd())));
		}
	}

	void FDreamShaderGraphDecompiler::AddIntMetadata(TArray<FString>& Entries, const TCHAR* Key, const int32 Value, const int32 DefaultValue)
	{
		if (Value != DefaultValue)
		{
			Entries.Add(FString::Printf(TEXT("%s=%d;"), Key, Value));
		}
	}

	void FDreamShaderGraphDecompiler::AddBoolMetadata(TArray<FString>& Entries, const TCHAR* Key, const bool bValue, const bool bDefaultValue)
	{
		if (bValue != bDefaultValue)
		{
			Entries.Add(FString::Printf(TEXT("%s=%s;"), Key, bValue ? TEXT("true") : TEXT("false")));
		}
	}

	void FDreamShaderGraphDecompiler::AddEnumMetadata(TArray<FString>& Entries, const TCHAR* Key, const UEnum* Enum, const int64 Value, const int64 DefaultValue)
	{
		if (Value != DefaultValue)
		{
			AddEnumMetadataAlways(Entries, Key, Enum, Value);
		}
	}

	void FDreamShaderGraphDecompiler::AddEnumMetadataAlways(TArray<FString>& Entries, const TCHAR* Key, const UEnum* Enum, const int64 Value)
	{
		Entries.Add(FString::Printf(TEXT("%s=\"%s\";"), Key, *EscapeDreamShaderString(GetEnumLiteralText(Enum, Value))));
	}

	FString FDreamShaderGraphDecompiler::BuildLiteralEnumArgument(const UEnum* Enum, const int64 Value)
	{
		return FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(GetEnumLiteralText(Enum, Value)));
	}

	void FDreamShaderGraphDecompiler::AddParameterMetadata(TArray<FString>& Entries, const UMaterialExpressionParameter* Parameter)
	{
		if (!Parameter)
		{
			return;
		}

		if (!Parameter->Group.IsNone())
		{
			AddStringMetadata(Entries, TEXT("Group"), Parameter->Group.ToString());
		}
		AddIntMetadata(Entries, TEXT("SortPriority"), Parameter->SortPriority, 32);
		AddStringMetadata(Entries, TEXT("Description"), Parameter->Desc);
	}

	void FDreamShaderGraphDecompiler::AddTextureParameterMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureSampleParameter* Parameter)
	{
		if (!Parameter)
		{
			return;
		}

		if (!Parameter->Group.IsNone())
		{
			AddStringMetadata(Entries, TEXT("Group"), Parameter->Group.ToString());
		}
		AddIntMetadata(Entries, TEXT("SortPriority"), Parameter->SortPriority, 32);
		AddStringMetadata(Entries, TEXT("Description"), Parameter->Desc);
	}

	void FDreamShaderGraphDecompiler::AddTextureParameterMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureObjectParameter* Parameter)
	{
		AddTextureParameterMetadata(Entries, static_cast<const UMaterialExpressionTextureSampleParameter*>(Parameter));
	}

	void FDreamShaderGraphDecompiler::AddTextureMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureBase* TextureExpression)
	{
		if (!TextureExpression)
		{
			return;
		}

		AddEnumMetadataAlways(
			Entries,
			TEXT("SamplerType"),
			StaticEnum<EMaterialSamplerType>(),
			TextureExpression->SamplerType.GetValue());
		AddBoolMetadata(Entries, TEXT("IsDefaultMeshpaintTexture"), TextureExpression->IsDefaultMeshpaintTexture != 0, false);
	}

	void FDreamShaderGraphDecompiler::AddTextureSampleMetadata(TArray<FString>& Entries, const UMaterialExpressionTextureSample* TextureSample)
	{
		if (!TextureSample)
		{
			return;
		}

		AddTextureMetadata(Entries, TextureSample);
		AddEnumMetadata(
			Entries,
			TEXT("SamplerSource"),
			StaticEnum<ESamplerSourceMode>(),
			TextureSample->SamplerSource.GetValue(),
			SSM_FromTextureAsset);
		AddEnumMetadata(
			Entries,
			TEXT("MipValueMode"),
			StaticEnum<ETextureMipValueMode>(),
			TextureSample->MipValueMode.GetValue(),
			TMVM_None);
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		AddEnumMetadata(
			Entries,
			TEXT("GatherMode"),
			StaticEnum<ETextureGatherMode>(),
			TextureSample->GatherMode.GetValue(),
			TGM_None);
#endif
		AddBoolMetadata(Entries, TEXT("AutomaticViewMipBias"), TextureSample->AutomaticViewMipBias != 0, true);
		AddIntMetadata(Entries, TEXT("ConstCoordinate"), TextureSample->ConstCoordinate, 0);
		AddIntMetadata(Entries, TEXT("ConstMipValue"), TextureSample->ConstMipValue, INDEX_NONE);
	}

	bool FDreamShaderGraphDecompiler::HasTextureSampleGraphInputs(const UMaterialExpressionTextureSample* TextureSample)
	{
		return TextureSample
			&& (TextureSample->Coordinates.IsConnected()
				|| TextureSample->TextureObject.IsConnected()
				|| TextureSample->MipValue.IsConnected()
				|| TextureSample->CoordinatesDX.IsConnected()
				|| TextureSample->CoordinatesDY.IsConnected()
				|| TextureSample->AutomaticViewMipBiasValue.IsConnected());
	}

	void FDreamShaderGraphDecompiler::AddTextureSampleExpressionArguments(UMaterialExpressionTextureSample* TextureSample, TArray<FExpressionCallArgument>& Arguments)
	{
		if (!TextureSample)
		{
			return;
		}

		if (TextureSample->Coordinates.IsConnected())
		{
			Arguments.Add({ TEXT("Coordinates"), CompileInput(TextureSample->Coordinates, TEXT("0.0")), true });
		}
		else if (TextureSample->ConstCoordinate != 0)
		{
			Arguments.Add({ TEXT("ConstCoordinate"), FString::Printf(TEXT("%d"), TextureSample->ConstCoordinate), false });
		}
		if (TextureSample->TextureObject.IsConnected())
		{
			Arguments.Add({ TEXT("TextureObject"), CompileInput(TextureSample->TextureObject, TEXT("default")), true });
		}
		else if (TextureSample->Texture)
		{
			Arguments.Add({ TEXT("Texture"), MakeDreamShaderObjectPathLiteral(TextureSample->Texture), false });
		}
		if (TextureSample->MipValue.IsConnected())
		{
			Arguments.Add({ TEXT("MipValue"), CompileInput(TextureSample->MipValue, TEXT("0.0")), true });
		}
		if (TextureSample->CoordinatesDX.IsConnected())
		{
			Arguments.Add({ TEXT("CoordinatesDX"), CompileInput(TextureSample->CoordinatesDX, TEXT("0.0")), true });
		}
		if (TextureSample->CoordinatesDY.IsConnected())
		{
			Arguments.Add({ TEXT("CoordinatesDY"), CompileInput(TextureSample->CoordinatesDY, TEXT("0.0")), true });
		}
		if (TextureSample->AutomaticViewMipBiasValue.IsConnected())
		{
			Arguments.Add({ TEXT("AutomaticViewMipBiasValue"), CompileInput(TextureSample->AutomaticViewMipBiasValue, TEXT("0.0")), true });
		}

		Arguments.Add({ TEXT("SamplerType"), BuildLiteralEnumArgument(StaticEnum<EMaterialSamplerType>(), TextureSample->SamplerType.GetValue()), false });
		if (TextureSample->SamplerSource.GetValue() != SSM_FromTextureAsset)
		{
			Arguments.Add({ TEXT("SamplerSource"), BuildLiteralEnumArgument(StaticEnum<ESamplerSourceMode>(), TextureSample->SamplerSource.GetValue()), false });
		}
		if (TextureSample->MipValueMode.GetValue() != TMVM_None)
		{
			Arguments.Add({ TEXT("MipValueMode"), BuildLiteralEnumArgument(StaticEnum<ETextureMipValueMode>(), TextureSample->MipValueMode.GetValue()), false });
		}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		if (TextureSample->GatherMode.GetValue() != TGM_None)
		{
			Arguments.Add({ TEXT("GatherMode"), BuildLiteralEnumArgument(StaticEnum<ETextureGatherMode>(), TextureSample->GatherMode.GetValue()), false });
		}
#endif
		if (!TextureSample->AutomaticViewMipBias)
		{
			Arguments.Add({ TEXT("AutomaticViewMipBias"), TEXT("false"), false });
		}
		if (TextureSample->ConstMipValue != INDEX_NONE)
		{
			Arguments.Add({ TEXT("ConstMipValue"), FString::Printf(TEXT("%d"), TextureSample->ConstMipValue), false });
		}
	}

	void FDreamShaderGraphDecompiler::AddTextureSampleParameterExpressionArguments( const UMaterialExpressionTextureSampleParameter* TextureParameter, TArray<FExpressionCallArgument>& Arguments)
	{
		if (!TextureParameter)
		{
			return;
		}

		if (!TextureParameter->ParameterName.IsNone())
		{
			Arguments.Add({
				TEXT("ParameterName"),
				FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(TextureParameter->ParameterName.ToString())),
				false
			});
		}
		if (!TextureParameter->Group.IsNone())
		{
			Arguments.Add({
				TEXT("Group"),
				FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(TextureParameter->Group.ToString())),
				false
			});
		}
		if (TextureParameter->SortPriority != 32)
		{
			Arguments.Add({ TEXT("SortPriority"), FString::Printf(TEXT("%d"), TextureParameter->SortPriority), false });
		}
		if (!TextureParameter->Desc.IsEmpty())
		{
			Arguments.Add({
				TEXT("Desc"),
				FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(TextureParameter->Desc)),
				false
			});
		}
	}

	int32 FDreamShaderGraphDecompiler::GetOutputComponentCount(const UMaterialExpression* Expression, const int32 OutputIndex)
	{
		return GetExpressionOutputComponentCount(const_cast<UMaterialExpression*>(Expression), OutputIndex);
	}

	int32 FDreamShaderGraphDecompiler::GetComponentCountForExpressionOutput(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		return GetExpressionOutputComponentCount(Expression, OutputIndex);
	}

	bool FDreamShaderGraphDecompiler::IsTextureObjectOutput(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		return Expression && GetDreamShaderTypeForExpressionOutput(Expression, OutputIndex).StartsWith(TEXT("Texture"));
	}

	int32 FDreamShaderGraphDecompiler::FindExpressionOutputIndexByName( const UMaterialExpression* Expression, const TCHAR* OutputName, const int32 FallbackIndex)
	{
		if (!Expression || !OutputName)
		{
			return FallbackIndex;
		}

		const FString DesiredOutputName(OutputName);
		for (int32 CandidateIndex = 0; CandidateIndex < Expression->Outputs.Num(); ++CandidateIndex)
		{
			const FExpressionOutput& Output = Expression->Outputs[CandidateIndex];
			if (!Output.OutputName.IsNone()
				&& Output.OutputName.ToString().Equals(DesiredOutputName, ESearchCase::IgnoreCase))
			{
				return CandidateIndex;
			}
		}

		return FallbackIndex;
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeValue( const FString& Text, const FString& Type, const int32 ComponentCount, const bool bIsSimple, const bool bIsTextureObject , const bool bIsMaterialAttributes , const bool bIsSubstrateMaterial )
	{
		FDecompiledValue Value;
		Value.Text = Text;
		Value.Type = Type;
		Value.ComponentCount = ComponentCount;
		Value.bIsSimple = bIsSimple;
		Value.bIsTextureObject = bIsTextureObject;
		Value.bIsMaterialAttributes = bIsMaterialAttributes;
		Value.bIsSubstrateMaterial = bIsSubstrateMaterial;
		return Value;
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeExpressionValue( UMaterialExpression* Expression, const int32 OutputIndex, const FString& Text, const bool bIsSimple)
	{
		const FString Type = GetDreamShaderTypeForExpressionOutput(Expression, OutputIndex);
		const bool bIsTextureObject = IsTextureObjectOutput(Expression, OutputIndex);
		const bool bIsSubstrateMaterial = Type.Equals(TEXT("Substrate"), ESearchCase::IgnoreCase);
		return MakeValue(
			Text,
			Type,
			bIsTextureObject ? 0 : GetComponentCountForExpressionOutput(Expression, OutputIndex),
			bIsSimple,
			bIsTextureObject,
			Type.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase),
			bIsSubstrateMaterial);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeDefaultValueForExpressionOutput(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		const FString Type = GetDreamShaderTypeForExpressionOutput(Expression, OutputIndex);
		const bool bIsTextureObject = IsTextureObjectOutput(Expression, OutputIndex);
		if (bIsTextureObject)
		{
			return MakeValue(TEXT("default"), Type, 0, true, true, false);
		}
		if (Type.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase))
		{
			return MakeValue(TEXT("MaterialAttributes()"), Type, 0, true, false, true);
		}
		if (Type.Equals(TEXT("Substrate"), ESearchCase::IgnoreCase))
		{
			return MakeValue(TEXT("default"), Type, 0, true, false, false, true);
		}

		const int32 ComponentCount = GetComponentCountForExpressionOutput(Expression, OutputIndex);
		if (ComponentCount <= 1)
		{
			return MakeValue(TEXT("0.0"), Type, 1, true);
		}

		TArray<FString> Components;
		Components.Init(TEXT("0.0"), ComponentCount);
		return MakeValue(
			FString::Printf(TEXT("%s(%s)"), *Type, *FString::Join(Components, TEXT(", "))),
			Type,
			ComponentCount,
			true);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeSwizzledValue(const FDecompiledValue& Source, const FString& SwizzleText)
	{
		if (SwizzleText.IsEmpty())
		{
			return Source;
		}
		if (Source.ComponentCount == 1 && IsSwizzleText(SwizzleText))
		{
			if (SwizzleText.Len() == 1)
			{
				return Source;
			}

			TArray<FDecompiledValue> Parts;
			Parts.Reserve(SwizzleText.Len());
			for (int32 Index = 0; Index < SwizzleText.Len(); ++Index)
			{
				Parts.Add(Source);
			}
			return MakeFunctionValue(
				TEXT("float") + FString::FromInt(SwizzleText.Len()),
				Parts,
				SwizzleText.Len());
		}

		const FString Text = MakeSwizzleExpression(Source.Text, SwizzleText);
		return MakeValue(
			Text,
			GetDreamShaderTypeForComponentCount(SwizzleText.Len()),
			SwizzleText.Len(),
			!Text.Contains(TEXT("\n")),
			false,
			false);
	}

	FString FDreamShaderGraphDecompiler::AddTemp(const FString& Type, const FString& ExpressionText, const FString& BaseName)
	{
		const FString Name = MakeUniqueName(BaseName, TEXT("Node"));
		GraphLines.Add(FormatGraphAssignment(Type, Name, ExpressionText));
		++NextTempIndex;
		return Name;
	}

	FString FDreamShaderGraphDecompiler::AddTempWithName(const FString& Type, const FString& ExpressionText, const FString& Name)
	{
		TempNames.Add(Name);
		ReservedNames.Add(Name);
		GraphLines.Add(FormatGraphAssignment(Type, Name, ExpressionText));
		++NextTempIndex;
		return Name;
	}

	void FDreamShaderGraphDecompiler::RegisterExpressionName(UMaterialExpression* Expression, const FString& Name)
	{
		if (Expression && !Name.TrimStartAndEnd().IsEmpty())
		{
			ExpressionNames.Add(Expression, Name);
		}
	}

	FString FDreamShaderGraphDecompiler::AddTempForExpression(UMaterialExpression* Expression, const FString& Type, const FString& ExpressionText, const FString& BaseName)
	{
		const FString Name = AddTemp(Type, ExpressionText, BaseName);
		RegisterExpressionName(Expression, Name);
		return Name;
	}

	FString FDreamShaderGraphDecompiler::AddTempWithNameForExpression(UMaterialExpression* Expression, const FString& Type, const FString& ExpressionText, const FString& Name)
	{
		const FString TempName = AddTempWithName(Type, ExpressionText, Name);
		RegisterExpressionName(Expression, TempName);
		return TempName;
	}

	FString FDreamShaderGraphDecompiler::FormatGraphAssignment(const FString& Type, const FString& Name, const FString& ExpressionText)
	{
		if (ExpressionText.Contains(TEXT("\n")))
		{
			return FString::Printf(TEXT("\t\t%s %s =\n%s;"), *Type, *Name, *IndentMultiline(ExpressionText, TEXT("\t\t\t")));
		}

		return FString::Printf(TEXT("\t\t%s %s = %s;"), *Type, *Name, *ExpressionText);
	}

	FString FDreamShaderGraphDecompiler::FormatGraphSetStatement(const FString& TargetName, const FString& ExpressionText)
	{
		if (ExpressionText.Contains(TEXT("\n")))
		{
			return FString::Printf(TEXT("\t\t%s =\n%s;"), *TargetName, *IndentMultiline(ExpressionText, TEXT("\t\t\t")));
		}

		return FString::Printf(TEXT("\t\t%s = %s;"), *TargetName, *ExpressionText);
	}

	FString FDreamShaderGraphDecompiler::IndentMultiline(const FString& Text, const TCHAR* Indent)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		for (FString& Line : Lines)
		{
			Line = FString(Indent) + Line;
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FDecompiledValue FDreamShaderGraphDecompiler::AddTempValue(const FDecompiledValue& Value, const FString& BaseName)
	{
		if (Value.bIsSimple)
		{
			return Value;
		}

		return MakeValue(
			AddTemp(Value.Type, Value.Text, BaseName),
			Value.Type,
			Value.ComponentCount,
			true,
			Value.bIsTextureObject,
			Value.bIsMaterialAttributes,
			Value.bIsSubstrateMaterial);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::AddTempValueWithName(const FDecompiledValue& Value, const FString& Name)
	{
		if (Value.bIsSimple)
		{
			return Value;
		}

		return MakeValue(
			AddTempWithName(Value.Type, Value.Text, Name),
			Value.Type,
			Value.ComponentCount,
			true,
			Value.bIsTextureObject,
			Value.bIsMaterialAttributes,
			Value.bIsSubstrateMaterial);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MaybeMaterializeValue(const FDecompiledValue& Value, const FString& BaseName)
	{
		if (Value.bIsSimple)
		{
			return Value;
		}

		const FString TrimmedText = Value.Text.TrimStartAndEnd();
		if (!Value.Text.Contains(TEXT("\n")) && !TrimmedText.StartsWith(TEXT("UE.Expression(")))
		{
			return Value;
		}

		return AddTempValue(Value, BaseName);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CacheExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value)
	{
		ExpressionTemps.Add(Key, Value.Text);
		ExpressionValues.Add(Key, Value);
		return Value;
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CacheTempExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value, const FString& BaseName)
	{
		if (Value.bIsSimple)
		{
			return CacheExpressionValue(Key, Value);
		}

		const FString Name = AddTempForExpression(Key.Expression ? const_cast<UMaterialExpression*>(Key.Expression) : nullptr, Value.Type, Value.Text, BaseName);
		return CacheExpressionValue(
			Key,
			MakeValue(
				Name,
				Value.Type,
				Value.ComponentCount,
				true,
				Value.bIsTextureObject,
				Value.bIsMaterialAttributes,
				Value.bIsSubstrateMaterial));
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CacheNamedTempExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value, const FString& Name)
	{
		if (Value.bIsSimple)
		{
			return CacheExpressionValue(Key, Value);
		}

		const FString TempName = AddTempWithNameForExpression(Key.Expression ? const_cast<UMaterialExpression*>(Key.Expression) : nullptr, Value.Type, Value.Text, Name);
		return CacheExpressionValue(
			Key,
			MakeValue(
				TempName,
				Value.Type,
				Value.ComponentCount,
				true,
				Value.bIsTextureObject,
				Value.bIsMaterialAttributes,
				Value.bIsSubstrateMaterial));
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CacheReusableExpressionValue(const FDecompiledExpressionKey& Key, const FDecompiledValue& Value, UMaterialExpression* Expression)
	{
		if (Value.bIsSimple)
		{
			return CacheExpressionValue(Key, Value);
		}

		return CacheTempExpressionValue(
			Key,
			Value,
			Expression ? GetMaterialExpressionShortName(Expression->GetClass()) : TEXT("Node"));
	}

	bool FDreamShaderGraphDecompiler::TracePlainReroutesForDecompile(const FExpressionInput& Input, FExpressionInput& OutInput)
	{
		OutInput = Input;

		TSet<const UMaterialExpressionReroute*> VisitedReroutes;
		while (UMaterialExpressionReroute* Reroute = Cast<UMaterialExpressionReroute>(OutInput.Expression))
		{
			if (VisitedReroutes.Contains(Reroute))
			{
				Warnings.AddUnique(FString::Printf(
					TEXT("Detected a recursive reroute dependency while decompiling node '%s'; emitted a default literal to avoid stack overflow."),
					*Reroute->GetName()));
				return false;
			}

			VisitedReroutes.Add(Reroute);
			OutInput = Reroute->Input;
		}

		return true;
	}

	FString FDreamShaderGraphDecompiler::CompileInput(const FExpressionInput& Input, const FString& DefaultText)
	{
		return CompileInputValue(Input, MakeValue(DefaultText, TEXT("float"), 1, true)).Text;
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CompileInputValue(const FExpressionInput& Input, const FDecompiledValue& DefaultValue)
	{
		FExpressionInput DecompileInput;
		if (!TracePlainReroutesForDecompile(Input, DecompileInput))
		{
			return DefaultValue;
		}

		if (UMaterialExpressionNamedRerouteUsage* NamedRerouteUsage = Cast<UMaterialExpressionNamedRerouteUsage>(DecompileInput.Expression))
		{
			if (!IsValid(NamedRerouteUsage->Declaration))
			{
				Warnings.AddUnique(FString::Printf(
					TEXT("Named reroute usage '%s' has no valid declaration; emitted its default value."),
					*NamedRerouteUsage->GetName()));
				return DefaultValue;
			}

			FDecompiledValue Value = CompileNamedRerouteDeclarationValue(NamedRerouteUsage->Declaration, DecompileInput.OutputIndex);
			const FString MaskSuffix = MakeInputMaskSuffix(Input);
			if (!MaskSuffix.IsEmpty())
			{
				Value = MakeSwizzledValue(Value, MaskSuffix);
			}
			return Value;
		}

		if (UMaterialExpressionNamedRerouteDeclaration* NamedRerouteDeclaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(DecompileInput.Expression))
		{
			FDecompiledValue Value = CompileNamedRerouteDeclarationValue(NamedRerouteDeclaration, DecompileInput.OutputIndex);
			const FString MaskSuffix = MakeInputMaskSuffix(Input);
			if (!MaskSuffix.IsEmpty())
			{
				Value = MakeSwizzledValue(Value, MaskSuffix);
			}
			return Value;
		}

		if (!DecompileInput.Expression)
		{
			return DefaultValue;
		}

		FDecompiledValue Value = CompileExpressionValue(DecompileInput.Expression, DecompileInput.OutputIndex);
		Value = ApplyConnectionInputMask(Value, DecompileInput.Expression, DecompileInput.OutputIndex, MakeInputMaskSuffix(Input));
		return MaybeMaterializeValue(Value, DecompileInput.Expression->GetName());
	}

	FString FDreamShaderGraphDecompiler::CompileConnectedOrLiteral(const FExpressionInput& Input, const FString& LiteralText)
	{
		return Input.IsConnected() ? CompileInput(Input, LiteralText) : LiteralText;
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CompileConnectedOrLiteralValue( const FExpressionInput& Input, const FString& LiteralText, const FString& Type, const int32 ComponentCount)
	{
		return Input.IsConnected()
			? CompileInputValue(Input, MakeValue(LiteralText, Type, ComponentCount, true))
			: MakeValue(LiteralText, Type, ComponentCount, true);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeBinaryValue( const FString& Operator, const FDecompiledValue& Left, const FDecompiledValue& Right)
	{
		const int32 ComponentCount = FMath::Max(Left.ComponentCount, Right.ComponentCount);
		return MakeValue(
			FString::Printf(TEXT("(%s %s %s)"), *Left.Text, *Operator, *Right.Text),
			GetDreamShaderTypeForComponentCount(ComponentCount),
			ComponentCount,
			false);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeFunctionValue(const FString& FunctionName, const TArray<FDecompiledValue>& Arguments, const int32 ComponentCount)
	{
		TArray<FString> ArgumentTexts;
		ArgumentTexts.Reserve(Arguments.Num());
		for (const FDecompiledValue& Argument : Arguments)
		{
			ArgumentTexts.Add(Argument.Text);
		}

		return MakeValue(
			FString::Printf(TEXT("%s(%s)"), *FunctionName, *FString::Join(ArgumentTexts, TEXT(", "))),
			GetDreamShaderTypeForComponentCount(ComponentCount),
			ComponentCount,
			false);
	}

	int32 FDreamShaderGraphDecompiler::GetCommonNumericComponentCount(const FDecompiledValue& A, const FDecompiledValue& B)
	{
		if (A.bIsMaterialAttributes || B.bIsMaterialAttributes || A.bIsSubstrateMaterial || B.bIsSubstrateMaterial)
		{
			return 0;
		}

		return FMath::Max(A.ComponentCount, B.ComponentCount);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeExpressionValueWithComponentCount( UMaterialExpression* Expression, const int32 OutputIndex, const FString& Text, const bool bIsSimple, const int32 ComponentCount)
	{
		if (ComponentCount <= 0)
		{
			return MakeExpressionValue(Expression, OutputIndex, Text, bIsSimple);
		}

		return MakeValue(
			Text,
			GetDreamShaderTypeForComponentCount(ComponentCount),
			ComponentCount,
			bIsSimple);
	}

	FString FDreamShaderGraphDecompiler::BuildUEExpressionCallWithOutputType( UMaterialExpression* Expression, const int32 OutputIndex, const FString& OutputType, const TArray<FExpressionCallArgument>& Arguments) const
	{
		TArray<FString> ArgumentTexts;
		ArgumentTexts.Add(FString::Printf(TEXT("Class=\"%s\""), *GetMaterialExpressionShortName(Expression ? Expression->GetClass() : nullptr)));
		ArgumentTexts.Add(FString::Printf(TEXT("OutputType=\"%s\""), *OutputType));
		// A name-based output selector (Output="...") and OutputIndex are mutually exclusive on a
		// UE.Expression -- the generator rejects emitting both ("cannot use OutputName/Output together
		// with OutputIndex"). Custom nodes select their secondary outputs by name, so when an
		// Output/OutputName argument is already present, suppress the redundant OutputIndex so the
		// decompiled call round-trips. OutputType is unaffected: it was resolved from the real
		// OutputIndex by the caller, so the named output still carries its correct type.
		const bool bHasNamedOutputSelector = Arguments.ContainsByPredicate(
			[](const FExpressionCallArgument& Argument)
			{
				return Argument.Name.Equals(TEXT("Output"), ESearchCase::IgnoreCase)
					|| Argument.Name.Equals(TEXT("OutputName"), ESearchCase::IgnoreCase);
			});
		if (OutputIndex > 0 && !bHasNamedOutputSelector)
		{
			ArgumentTexts.Add(FString::Printf(TEXT("OutputIndex=%d"), OutputIndex));
		}

		for (const FExpressionCallArgument& Argument : Arguments)
		{
			if (!Argument.Value.TrimStartAndEnd().IsEmpty() && !Argument.Value.Equals(TEXT("default"), ESearchCase::CaseSensitive))
			{
				ArgumentTexts.Add(FString::Printf(TEXT("%s=%s"), *Argument.Name, *Argument.Value));
			}
		}

		bool bUseMultiline = ArgumentTexts.Num() > 3;
		if (!bUseMultiline)
		{
			const FString SingleLine = FString::Printf(TEXT("UE.Expression(%s)"), *FString::Join(ArgumentTexts, TEXT(", ")));
			bUseMultiline = SingleLine.Len() > 120;
			if (!bUseMultiline)
			{
				return SingleLine;
			}
		}

		TArray<FString> Lines;
		Lines.Reserve(ArgumentTexts.Num());
		for (int32 ArgumentIndex = 0; ArgumentIndex < ArgumentTexts.Num(); ++ArgumentIndex)
		{
			Lines.Add(FString::Printf(
				TEXT("\t%s%s"),
				*ArgumentTexts[ArgumentIndex],
				ArgumentIndex + 1 < ArgumentTexts.Num() ? TEXT(",") : TEXT("")));
		}

		return FString::Printf(TEXT("UE.Expression(\n%s\n)"), *FString::Join(Lines, TEXT("\n")));
	}

	bool FDreamShaderGraphDecompiler::TryCombineAppendSwizzle( const FDecompiledValue& A, const FDecompiledValue& B, FDecompiledValue& OutValue)
	{
		FString BaseA;
		FString SwizzleA;
		FString BaseB;
		FString SwizzleB;
		if (!TrySplitTrailingSwizzle(A.Text, BaseA, SwizzleA)
			|| !TrySplitTrailingSwizzle(B.Text, BaseB, SwizzleB)
			|| !BaseA.Equals(BaseB, ESearchCase::CaseSensitive)
			|| SwizzleA.Len() != A.ComponentCount
			|| SwizzleB.Len() != B.ComponentCount)
		{
			return false;
		}

		const FString CombinedSwizzle = SwizzleA + SwizzleB;
		if (!IsSwizzleText(CombinedSwizzle))
		{
			return false;
		}

		const FString Text = MakeSwizzleExpression(BaseA, CombinedSwizzle);
		OutValue = MakeValue(
			Text,
			GetDreamShaderTypeForComponentCount(CombinedSwizzle.Len()),
			CombinedSwizzle.Len(),
			!Text.Contains(TEXT("\n")));
		return true;
	}

	FString FDreamShaderGraphDecompiler::CompileExpression(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		return CompileExpressionValue(Expression, OutputIndex).Text;
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CompileExpressionValue(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		if (!Expression)
		{
			return MakeValue(TEXT("0.0"), TEXT("float"), 1, true);
		}

		EnterExpressionProgressFrame(Expression);

		const FDecompiledExpressionKey Key{ Expression, OutputIndex };
		if (const FString* ExistingTemp = ExpressionTemps.Find(Key))
		{
			if (const FDecompiledValue* ExistingValue = ExpressionValues.Find(Key))
			{
				return *ExistingValue;
			}
			return MakeValue(*ExistingTemp, GetDreamShaderTypeForExpressionOutput(Expression, OutputIndex), GetComponentCountForExpressionOutput(Expression, OutputIndex), true);
		}
		if (CompilingExpressionKeys.Contains(Key))
		{
			Warnings.AddUnique(FString::Printf(
				TEXT("Detected a recursive graph dependency while decompiling node '%s'; emitted a default literal to avoid stack overflow."),
				*Expression->GetName()));
			return MakeDefaultValueForExpressionOutput(Expression, OutputIndex);
		}

		CompilingExpressionKeys.Add(Key);
		ON_SCOPE_EXIT
		{
			CompilingExpressionKeys.Remove(Key);
		};

		if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			if (const FString* InputName = FunctionInputNames.Find(FunctionInput))
			{
				return MakeValue(
					*InputName,
					GetDreamShaderTypeForFunctionInput(FunctionInput->InputType.GetValue()),
					FunctionInput->InputType == FunctionInput_Scalar ? 1 : GetComponentCountForExpressionOutput(Expression, OutputIndex),
					true);
			}
			return MakeValue(
				MakeDreamShaderDeclarationName(FunctionInput->InputName.ToString(), TEXT("Input"), 0),
				GetDreamShaderTypeForFunctionInput(FunctionInput->InputType.GetValue()),
				FunctionInput->InputType == FunctionInput_Scalar ? 1 : GetComponentCountForExpressionOutput(Expression, OutputIndex),
				true);
		}

		if (UMaterialExpressionNamedRerouteUsage* NamedRerouteUsage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
		{
			if (!IsValid(NamedRerouteUsage->Declaration))
			{
				Warnings.AddUnique(FString::Printf(
					TEXT("Named reroute usage '%s' has no valid declaration; emitted a default literal."),
					*NamedRerouteUsage->GetName()));
				return MakeDefaultValueForExpressionOutput(Expression, OutputIndex);
			}

			return CacheExpressionValue(Key, CompileNamedRerouteDeclarationValue(NamedRerouteUsage->Declaration, OutputIndex));
		}

		if (UMaterialExpressionNamedRerouteDeclaration* NamedRerouteDeclaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression))
		{
			return CacheExpressionValue(Key, CompileNamedRerouteDeclarationValue(NamedRerouteDeclaration, OutputIndex));
		}

		if (UMaterialExpressionCurveAtlasRowParameter* CurveAtlas = Cast<UMaterialExpressionCurveAtlasRowParameter>(Expression))
		{
			TArray<FExpressionCallArgument> Arguments;
			if (!CurveAtlas->ParameterName.IsNone())
			{
				Arguments.Add({
					TEXT("ParameterName"),
					FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(CurveAtlas->ParameterName.ToString())),
					false });
			}
			if (!CurveAtlas->Group.IsNone())
			{
				Arguments.Add({
					TEXT("Group"),
					FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(CurveAtlas->Group.ToString())),
					false });
			}
			if (CurveAtlas->SortPriority != 32)
			{
				Arguments.Add({ TEXT("SortPriority"), FString::FromInt(CurveAtlas->SortPriority), false });
			}
			if (!CurveAtlas->Desc.IsEmpty())
			{
				Arguments.Add({
					TEXT("Desc"),
					FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(CurveAtlas->Desc)),
					false });
			}
			Arguments.Add({ TEXT("DefaultValue"), FormatDreamShaderFloat(CurveAtlas->DefaultValue), false });
			if (CurveAtlas->Curve)
			{
				Arguments.Add({ TEXT("Curve"), MakeDreamShaderObjectPathLiteral(CurveAtlas->Curve.Get()), false });
			}
			if (CurveAtlas->Atlas)
			{
				Arguments.Add({ TEXT("Atlas"), MakeDreamShaderObjectPathLiteral(CurveAtlas->Atlas.Get()), false });
			}
			if (CurveAtlas->bUseCustomPrimitiveData)
			{
				Arguments.Add({ TEXT("UseCustomPrimitiveData"), TEXT("true"), false });
				Arguments.Add({ TEXT("PrimitiveDataIndex"), FString::FromInt(CurveAtlas->PrimitiveDataIndex), false });
			}
			if (CurveAtlas->InputTime.GetTracedInput().Expression)
			{
				Arguments.Add({
					TEXT("CurveTime"),
					CompileInputValue(CurveAtlas->InputTime, MakeValue(TEXT("0.0"), TEXT("float"), 1, true)).Text,
					true });
			}

			const FString BaseName = CurveAtlas->ParameterName.IsNone()
				? TEXT("CurveAtlas")
				: MakeDreamShaderDeclarationName(CurveAtlas->ParameterName.ToString(), TEXT("CurveAtlas"), 0);
			return CacheTempExpressionValue(
				Key,
				MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, Arguments), false),
				BaseName);
		}

		if (UMaterialExpressionScalarParameter* ScalarParameter = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			FString Name;
			if (const FString* ExistingName = ExpressionNames.Find(Expression))
			{
				Name = *ExistingName;
			}
			else
			{
				Name = MakeUniquePropertyName(ScalarParameter->ParameterName.ToString(), TEXT("Scalar"));
				TArray<FString> MetadataEntries;
				AddParameterNameMetadataIfNeeded(MetadataEntries, Name, ScalarParameter->ParameterName);
				AddParameterMetadata(MetadataEntries, ScalarParameter);
				AddPropertyDeclaration(
					Name,
					FString::Printf(
						TEXT("ScalarParameter %s = %s%s;"),
						*Name,
						*FormatDreamShaderFloat(ScalarParameter->DefaultValue),
						*BuildMetadataSuffix(MetadataEntries)));
				RegisterExpressionName(Expression, Name);
			}
			return CacheExpressionValue(Key, MakeValue(Name, TEXT("float"), 1, true));
		}

		if (UMaterialExpressionVectorParameter* VectorParameter = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			FString Name;
			if (const FString* ExistingName = ExpressionNames.Find(Expression))
			{
				Name = *ExistingName;
			}
			else
			{
				Name = MakeUniquePropertyName(VectorParameter->ParameterName.ToString(), TEXT("Vector"));
				TArray<FString> MetadataEntries;
				AddParameterNameMetadataIfNeeded(MetadataEntries, Name, VectorParameter->ParameterName);
				AddParameterMetadata(MetadataEntries, VectorParameter);
				AddPropertyDeclaration(
					Name,
					FString::Printf(
						TEXT("VectorParameter %s = %s%s;"),
						*Name,
						*FormatDreamShaderColor(VectorParameter->DefaultValue),
						*BuildMetadataSuffix(MetadataEntries)));
				RegisterExpressionName(Expression, Name);
			}
			return CacheExpressionValue(Key, MakeExpressionOutputValue(MakeExpressionValue(Expression, 0, Name, true), Expression, OutputIndex));
		}

		if (UMaterialExpressionTextureObjectParameter* TextureObjectParameter = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
		{
			FString Name;
			if (const FString* ExistingName = ExpressionNames.Find(Expression))
			{
				Name = *ExistingName;
			}
			else
			{
				Name = MakeUniquePropertyName(TextureObjectParameter->ParameterName.ToString(), TEXT("Texture"));
				const FString DefaultValue = TextureObjectParameter->Texture
					? FString::Printf(TEXT(" = %s"), *MakeDreamShaderObjectPathLiteral(TextureObjectParameter->Texture))
					: FString();
				TArray<FString> MetadataEntries;
				AddParameterNameMetadataIfNeeded(MetadataEntries, Name, TextureObjectParameter->ParameterName);
				AddTextureParameterMetadata(MetadataEntries, TextureObjectParameter);
				AddTextureSampleMetadata(MetadataEntries, TextureObjectParameter);
				AddPropertyDeclaration(
					Name,
					FString::Printf(
						TEXT("TextureObjectParameter %s%s%s;"),
						*Name,
						*DefaultValue,
						*BuildMetadataSuffix(MetadataEntries)));
				RegisterExpressionName(Expression, Name);
			}
			return CacheExpressionValue(Key, MakeValue(Name, TEXT("Texture2D"), 0, true, true));
		}

		if (UMaterialExpressionTextureSampleParameter2D* TextureParameter = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
		{
			if (HasTextureSampleGraphInputs(TextureParameter))
			{
				const FString Name = ExpressionNames.Find(Expression)
					? *ExpressionNames.Find(Expression)
					: MakeUniquePropertyName(TextureParameter->ParameterName.ToString(), TEXT("Texture"));
				TArray<FExpressionCallArgument> Arguments;
				AddTextureSampleParameterExpressionArguments(TextureParameter, Arguments);
				AddTextureSampleExpressionArguments(TextureParameter, Arguments);

				const int32 RgbaOutputIndex = FindExpressionOutputIndexByName(Expression, TEXT("RGBA"), OutputIndex);
				const FDecompiledExpressionKey RgbaKey{ Expression, RgbaOutputIndex };
				FDecompiledValue RgbaValue;
				if (const FDecompiledValue* ExistingValue = ExpressionValues.Find(RgbaKey))
				{
					RgbaValue = *ExistingValue;
				}
				else
				{
					const FString TempName = MakeUniqueName(Name, TEXT("Texture"));
					RgbaValue = CacheNamedTempExpressionValue(
						RgbaKey,
						MakeValue(BuildUEExpressionCall(Expression, RgbaOutputIndex, Arguments), TEXT("float4"), 4, false),
						TempName);
				}

				if (OutputIndex == RgbaOutputIndex)
				{
					return RgbaValue;
				}
				return MakeExpressionOutputValue(RgbaValue, Expression, OutputIndex);
			}

			FString Name;
			if (const FString* ExistingName = ExpressionNames.Find(Expression))
			{
				Name = *ExistingName;
			}
			else
			{
				Name = MakeUniquePropertyName(TextureParameter->ParameterName.ToString(), TEXT("Texture"));
				const FString DefaultValue = TextureParameter->Texture
					? FString::Printf(TEXT(" = %s"), *MakeDreamShaderObjectPathLiteral(TextureParameter->Texture))
					: FString();
				TArray<FString> MetadataEntries;
				AddParameterNameMetadataIfNeeded(MetadataEntries, Name, TextureParameter->ParameterName);
				AddTextureParameterMetadata(MetadataEntries, TextureParameter);
				AddTextureSampleMetadata(MetadataEntries, TextureParameter);
				AddPropertyDeclaration(
					Name,
					FString::Printf(
						TEXT("TextureSampleParameter2D %s%s%s;"),
						*Name,
						*DefaultValue,
						*BuildMetadataSuffix(MetadataEntries)));
				RegisterExpressionName(Expression, Name);
			}
			const int32 RgbaOutputIndex = FindExpressionOutputIndexByName(Expression, TEXT("RGBA"), 0);
			return CacheExpressionValue(Key, MakeExpressionOutputValue(MakeExpressionValue(Expression, RgbaOutputIndex, Name, true), Expression, OutputIndex));
		}

		if (UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(Expression))
		{
			return MakeValue(FormatDreamShaderFloat(Constant->R), TEXT("float"), 1, true);
		}

		if (UMaterialExpressionConstant2Vector* Constant2 = Cast<UMaterialExpressionConstant2Vector>(Expression))
		{
			return MakeValue(FormatDreamShaderVector2(Constant2->R, Constant2->G), TEXT("float2"), 2, true);
		}

		if (UMaterialExpressionConstant3Vector* Constant3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			return MakeValue(FormatDreamShaderVector3(Constant3->Constant.R, Constant3->Constant.G, Constant3->Constant.B), TEXT("float3"), 3, true);
		}

		if (UMaterialExpressionConstant4Vector* Constant4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
		{
			return MakeValue(FormatDreamShaderColor(Constant4->Constant), TEXT("float4"), 4, true);
		}

		if (UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Expression))
		{
			return CacheReusableExpressionValue(Key, MakeBinaryValue(
				TEXT("+"),
				CompileConnectedOrLiteralValue(Add->A, FormatDreamShaderFloat(Add->ConstA), TEXT("float"), 1),
				CompileConnectedOrLiteralValue(Add->B, FormatDreamShaderFloat(Add->ConstB), TEXT("float"), 1)), Expression);
		}

		if (UMaterialExpressionSubtract* Subtract = Cast<UMaterialExpressionSubtract>(Expression))
		{
			return CacheReusableExpressionValue(Key, MakeBinaryValue(
				TEXT("-"),
				CompileConnectedOrLiteralValue(Subtract->A, FormatDreamShaderFloat(Subtract->ConstA), TEXT("float"), 1),
				CompileConnectedOrLiteralValue(Subtract->B, FormatDreamShaderFloat(Subtract->ConstB), TEXT("float"), 1)), Expression);
		}

		if (UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Expression))
		{
			return CacheReusableExpressionValue(Key, MakeBinaryValue(
				TEXT("*"),
				CompileConnectedOrLiteralValue(Multiply->A, FormatDreamShaderFloat(Multiply->ConstA), TEXT("float"), 1),
				CompileConnectedOrLiteralValue(Multiply->B, FormatDreamShaderFloat(Multiply->ConstB), TEXT("float"), 1)), Expression);
		}

		if (UMaterialExpressionDivide* Divide = Cast<UMaterialExpressionDivide>(Expression))
		{
			return CacheReusableExpressionValue(Key, MakeBinaryValue(
				TEXT("/"),
				CompileConnectedOrLiteralValue(Divide->A, FormatDreamShaderFloat(Divide->ConstA), TEXT("float"), 1),
				CompileConnectedOrLiteralValue(Divide->B, FormatDreamShaderFloat(Divide->ConstB), TEXT("float"), 1)), Expression);
		}

		if (UMaterialExpressionLinearInterpolate* Lerp = Cast<UMaterialExpressionLinearInterpolate>(Expression))
		{
			FDecompiledValue A = CompileConnectedOrLiteralValue(Lerp->A, FormatDreamShaderFloat(Lerp->ConstA), TEXT("float"), 1);
			FDecompiledValue B = CompileConnectedOrLiteralValue(Lerp->B, FormatDreamShaderFloat(Lerp->ConstB), TEXT("float"), 1);
			FDecompiledValue Alpha = CompileConnectedOrLiteralValue(Lerp->Alpha, FormatDreamShaderFloat(Lerp->ConstAlpha), TEXT("float"), 1);
			return CacheReusableExpressionValue(
				Key,
				MakeFunctionValue(TEXT("lerp"), { A, B, Alpha }, GetCommonNumericComponentCount(A, B)),
				Expression);
		}

		if (UMaterialExpressionClamp* Clamp = Cast<UMaterialExpressionClamp>(Expression))
		{
			if (Clamp->ClampMode != CMODE_Clamp)
			{
				return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, {
					{ TEXT("Input"), CompileInput(Clamp->Input, TEXT("0.0")), true },
					{ TEXT("Min"), CompileConnectedOrLiteral(Clamp->Min, FormatDreamShaderFloat(Clamp->MinDefault)), true },
					{ TEXT("Max"), CompileConnectedOrLiteral(Clamp->Max, FormatDreamShaderFloat(Clamp->MaxDefault)), true },
					{ TEXT("ClampMode"), Clamp->ClampMode == CMODE_ClampMin ? TEXT("\"CMODE_ClampMin\"") : TEXT("\"CMODE_ClampMax\""), false },
				}), false);
			}
			FDecompiledValue Input = CompileInputValue(Clamp->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			FDecompiledValue Min = CompileConnectedOrLiteralValue(Clamp->Min, FormatDreamShaderFloat(Clamp->MinDefault), TEXT("float"), 1);
			FDecompiledValue Max = CompileConnectedOrLiteralValue(Clamp->Max, FormatDreamShaderFloat(Clamp->MaxDefault), TEXT("float"), 1);
			return MakeFunctionValue(TEXT("clamp"), { Input, Min, Max }, Input.ComponentCount);
		}

		if (UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expression))
		{
			FString Suffix;
			if (Mask->R)
			{
				Suffix += TEXT("r");
			}
			if (Mask->G)
			{
				Suffix += TEXT("g");
			}
			if (Mask->B)
			{
				Suffix += TEXT("b");
			}
			if (Mask->A)
			{
				Suffix += TEXT("a");
			}
			FDecompiledValue Source = CompileInputValue(Mask->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeSwizzledValue(Source, Suffix);
		}

		if (UMaterialExpressionStaticComponentMaskParameter* StaticComponentMask = Cast<UMaterialExpressionStaticComponentMaskParameter>(Expression))
		{
			TArray<FExpressionCallArgument> Arguments = {
				{ TEXT("Input"), CompileInput(StaticComponentMask->Input, TEXT("0.0")), true },
				{ TEXT("DefaultR"), StaticComponentMask->DefaultR ? TEXT("true") : TEXT("false"), false },
				{ TEXT("DefaultG"), StaticComponentMask->DefaultG ? TEXT("true") : TEXT("false"), false },
				{ TEXT("DefaultB"), StaticComponentMask->DefaultB ? TEXT("true") : TEXT("false"), false },
				{ TEXT("DefaultA"), StaticComponentMask->DefaultA ? TEXT("true") : TEXT("false"), false },
			};
			if (!StaticComponentMask->ParameterName.IsNone())
			{
				Arguments.Add({
					TEXT("ParameterName"),
					FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(StaticComponentMask->ParameterName.ToString())),
					false
				});
			}

			const int32 ComponentCount =
				(StaticComponentMask->DefaultR ? 1 : 0)
				+ (StaticComponentMask->DefaultG ? 1 : 0)
				+ (StaticComponentMask->DefaultB ? 1 : 0)
				+ (StaticComponentMask->DefaultA ? 1 : 0);
			const FString OutputType = GetDreamShaderTypeForComponentCount(ComponentCount > 0 ? ComponentCount : 1);
			return MakeExpressionValueWithComponentCount(
				Expression,
				OutputIndex,
				BuildUEExpressionCallWithOutputType(Expression, OutputIndex, OutputType, Arguments),
				false,
				ComponentCount > 0 ? ComponentCount : 1);
		}

		if (UMaterialExpressionAppendVector* Append = Cast<UMaterialExpressionAppendVector>(Expression))
		{
			FDecompiledValue A = CompileInputValue(Append->A, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			FDecompiledValue B = CompileInputValue(Append->B, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			FDecompiledValue CombinedValue;
			if (TryCombineAppendSwizzle(A, B, CombinedValue))
			{
				return CombinedValue;
			}

			// An append can never produce more than four components -- UE's own translator rejects it.
			// If the resolved counts say otherwise, one of them was inferred (some nodes do not report an
			// output value type), so mask the inputs down instead of emitting an impossible `float5(...)`
			// that the generator cannot parse back.
			if (A.ComponentCount + B.ComponentCount > 4)
			{
				const auto MaskValueDown = [](const FDecompiledValue& Value, const int32 ComponentCount)
				{
					if (Value.ComponentCount <= ComponentCount)
					{
						return Value;
					}

					return MakeValue(
						MakeSwizzleExpression(Value.Text, FString(TEXT("rgba")).Left(ComponentCount)),
						GetDreamShaderTypeForComponentCount(ComponentCount),
						ComponentCount,
						Value.bIsSimple);
				};

				const int32 ClampedComponentCountB = FMath::Clamp(B.ComponentCount, 1, 3);
				const int32 ClampedComponentCountA = FMath::Clamp(4 - ClampedComponentCountB, 1, 3);
				Warnings.AddUnique(FString::Printf(
					TEXT("Append node '%s' resolved to %d + %d components, which cannot fit a float4; masked its inputs down to %d + %d. Review the emitted swizzle."),
					*Expression->GetName(),
					A.ComponentCount,
					B.ComponentCount,
					ClampedComponentCountA,
					ClampedComponentCountB));

				A = MaskValueDown(A, ClampedComponentCountA);
				B = MaskValueDown(B, ClampedComponentCountB);
			}

			return MakeFunctionValue(TEXT("float") + FString::FromInt(A.ComponentCount + B.ComponentCount), { A, B }, A.ComponentCount + B.ComponentCount);
		}

		if (UMaterialExpressionOneMinus* OneMinus = Cast<UMaterialExpressionOneMinus>(Expression))
		{
			return MakeBinaryValue(TEXT("-"), MakeValue(TEXT("1.0"), TEXT("float"), 1, true), CompileInputValue(OneMinus->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true)));
		}

		if (UMaterialExpressionPower* Power = Cast<UMaterialExpressionPower>(Expression))
		{
			FDecompiledValue Base = CompileInputValue(Power->Base, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			FDecompiledValue Exponent = CompileConnectedOrLiteralValue(Power->Exponent, FormatDreamShaderFloat(Power->ConstExponent), TEXT("float"), 1);
			return MakeFunctionValue(TEXT("pow"), { Base, Exponent }, Base.ComponentCount);
		}

		if (UMaterialExpressionDotProduct* Dot = Cast<UMaterialExpressionDotProduct>(Expression))
		{
			return MakeFunctionValue(
				TEXT("dot"),
				{
					CompileInputValue(Dot->A, MakeValue(TEXT("0.0"), TEXT("float"), 1, true)),
					CompileInputValue(Dot->B, MakeValue(TEXT("0.0"), TEXT("float"), 1, true))
				},
				1);
		}

		if (UMaterialExpressionNormalize* Normalize = Cast<UMaterialExpressionNormalize>(Expression))
		{
			FDecompiledValue Input = CompileInputValue(Normalize->VectorInput, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("normalize"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionMin* Min = Cast<UMaterialExpressionMin>(Expression))
		{
			FDecompiledValue A = CompileConnectedOrLiteralValue(Min->A, FormatDreamShaderFloat(Min->ConstA), TEXT("float"), 1);
			FDecompiledValue B = CompileConnectedOrLiteralValue(Min->B, FormatDreamShaderFloat(Min->ConstB), TEXT("float"), 1);
			return MakeFunctionValue(TEXT("min"), { A, B }, FMath::Max(A.ComponentCount, B.ComponentCount));
		}

		if (UMaterialExpressionMax* Max = Cast<UMaterialExpressionMax>(Expression))
		{
			FDecompiledValue A = CompileConnectedOrLiteralValue(Max->A, FormatDreamShaderFloat(Max->ConstA), TEXT("float"), 1);
			FDecompiledValue B = CompileConnectedOrLiteralValue(Max->B, FormatDreamShaderFloat(Max->ConstB), TEXT("float"), 1);
			return MakeFunctionValue(TEXT("max"), { A, B }, FMath::Max(A.ComponentCount, B.ComponentCount));
		}

		if (UMaterialExpressionAbs* Abs = Cast<UMaterialExpressionAbs>(Expression))
		{
			FDecompiledValue Input = CompileInputValue(Abs->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("abs"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionSaturate* Saturate = Cast<UMaterialExpressionSaturate>(Expression))
		{
			FDecompiledValue Input = CompileInputValue(Saturate->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("saturate"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionFloor* Floor = Cast<UMaterialExpressionFloor>(Expression))
		{
			FDecompiledValue Input = CompileInputValue(Floor->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("floor"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionCeil* Ceil = Cast<UMaterialExpressionCeil>(Expression))
		{
			FDecompiledValue Input = CompileInputValue(Ceil->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("ceil"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionFrac* Frac = Cast<UMaterialExpressionFrac>(Expression))
		{
			FDecompiledValue Input = CompileInputValue(Frac->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("frac"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionSquareRoot* SquareRoot = Cast<UMaterialExpressionSquareRoot>(Expression))
		{
			FDecompiledValue Input = CompileInputValue(SquareRoot->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("sqrt"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionSine* Sine = Cast<UMaterialExpressionSine>(Expression))
		{
			if (!FMath::IsNearlyEqual(Sine->Period, 1.0f))
			{
				return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, {
					{ TEXT("Input"), CompileInput(Sine->Input, TEXT("0.0")), true },
					{ TEXT("Period"), FormatDreamShaderFloat(Sine->Period), false },
				}), false);
			}
			FDecompiledValue Input = CompileInputValue(Sine->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("sin"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionCosine* Cosine = Cast<UMaterialExpressionCosine>(Expression))
		{
			if (!FMath::IsNearlyEqual(Cosine->Period, 1.0f))
			{
				return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, {
					{ TEXT("Input"), CompileInput(Cosine->Input, TEXT("0.0")), true },
					{ TEXT("Period"), FormatDreamShaderFloat(Cosine->Period), false },
				}), false);
			}
			FDecompiledValue Input = CompileInputValue(Cosine->Input, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			return MakeFunctionValue(TEXT("cos"), { Input }, Input.ComponentCount);
		}

		if (UMaterialExpressionStaticSwitchParameter* StaticSwitchParameter = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			FDecompiledValue TrueValue = CompileInputValue(StaticSwitchParameter->A, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			FDecompiledValue FalseValue = CompileInputValue(StaticSwitchParameter->B, MakeValue(TEXT("0.0"), TEXT("float"), 1, true));
			const int32 ComponentCount = GetCommonNumericComponentCount(TrueValue, FalseValue);
			TArray<FExpressionCallArgument> Arguments = {
				{ TEXT("True"), TrueValue.Text, true },
				{ TEXT("False"), FalseValue.Text, true },
			};
			if (!StaticSwitchParameter->ParameterName.IsNone())
			{
				Arguments.Add({
					TEXT("ParameterName"),
					FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(StaticSwitchParameter->ParameterName.ToString())),
					false
				});
			}
			if (StaticSwitchParameter->DefaultValue)
			{
				Arguments.Add({ TEXT("DefaultValue"), TEXT("true"), false });
			}
			if (StaticSwitchParameter->DynamicBranch)
			{
				Arguments.Add({ TEXT("DynamicBranch"), TEXT("true"), false });
			}

			const FString OutputType = GetDreamShaderTypeForComponentCount(ComponentCount);
			return CacheReusableExpressionValue(
				Key,
				MakeExpressionValueWithComponentCount(
					Expression,
					OutputIndex,
					BuildUEExpressionCallWithOutputType(Expression, OutputIndex, OutputType, Arguments),
					false,
					ComponentCount),
				Expression);
		}

		if (UMaterialExpressionTextureCoordinate* TextureCoordinate = Cast<UMaterialExpressionTextureCoordinate>(Expression))
		{
			TArray<FExpressionCallArgument> Arguments;
			if (TextureCoordinate->CoordinateIndex != 0)
			{
				Arguments.Add({ TEXT("CoordinateIndex"), FString::Printf(TEXT("%d"), TextureCoordinate->CoordinateIndex), false });
			}
			if (!FMath::IsNearlyEqual(TextureCoordinate->UTiling, 1.0f))
			{
				Arguments.Add({ TEXT("UTiling"), FormatDreamShaderFloat(TextureCoordinate->UTiling), false });
			}
			if (!FMath::IsNearlyEqual(TextureCoordinate->VTiling, 1.0f))
			{
				Arguments.Add({ TEXT("VTiling"), FormatDreamShaderFloat(TextureCoordinate->VTiling), false });
			}
			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, Arguments), false);
		}

		if (UMaterialExpressionTime* Time = Cast<UMaterialExpressionTime>(Expression))
		{
			if (!Time->bIgnorePause && !Time->bOverride_Period)
			{
				return MakeValue(TEXT("UE.Time()"), TEXT("float"), 1, true);
			}

			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, {
				{ TEXT("bIgnorePause"), Time->bIgnorePause ? TEXT("true") : TEXT("false"), false },
				{ TEXT("bOverride_Period"), Time->bOverride_Period ? TEXT("true") : TEXT("false"), false },
				{ TEXT("Period"), FormatDreamShaderFloat(Time->Period), false },
			}), false);
		}

		if (UMaterialExpressionPanner* Panner = Cast<UMaterialExpressionPanner>(Expression))
		{
			TArray<FExpressionCallArgument> Arguments;
			if (Panner->Coordinate.IsConnected())
			{
				Arguments.Add({ TEXT("Coordinate"), CompileInput(Panner->Coordinate, TEXT("0.0")), true });
			}
			else if (Panner->ConstCoordinate != 0)
			{
				Arguments.Add({ TEXT("ConstCoordinate"), FString::Printf(TEXT("%d"), Panner->ConstCoordinate), false });
			}
			if (Panner->Time.IsConnected())
			{
				Arguments.Add({ TEXT("Time"), CompileInput(Panner->Time, TEXT("UE.Time()")), true });
			}
			if (Panner->Speed.IsConnected())
			{
				Arguments.Add({ TEXT("Speed"), CompileInput(Panner->Speed, FormatDreamShaderVector2(Panner->SpeedX, Panner->SpeedY)), true });
			}
			else
			{
				if (!FMath::IsNearlyZero(Panner->SpeedX))
				{
					Arguments.Add({ TEXT("SpeedX"), FormatDreamShaderFloat(Panner->SpeedX), false });
				}
				if (!FMath::IsNearlyZero(Panner->SpeedY))
				{
					Arguments.Add({ TEXT("SpeedY"), FormatDreamShaderFloat(Panner->SpeedY), false });
				}
			}
			if (Panner->bFractionalPart)
			{
				Arguments.Add({ TEXT("bFractionalPart"), TEXT("true"), false });
			}
			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, Arguments), false);
		}

		if (IsDreamShaderRotatorExpression(Expression))
		{
			const UMaterialExpressionRotator* Rotator = static_cast<const UMaterialExpressionRotator*>(Expression);
			TArray<FExpressionCallArgument> Arguments;
			if (Rotator->Coordinate.IsConnected())
			{
				Arguments.Add({ TEXT("Coordinate"), CompileInput(Rotator->Coordinate, TEXT("UE.TexCoord()")), true });
			}
			else if (Rotator->ConstCoordinate != 0)
			{
				Arguments.Add({ TEXT("ConstCoordinate"), FString::Printf(TEXT("%d"), Rotator->ConstCoordinate), false });
			}
			if (Rotator->Time.IsConnected())
			{
				Arguments.Add({ TEXT("Time"), CompileInput(Rotator->Time, TEXT("UE.Time()")), true });
			}
			if (!FMath::IsNearlyEqual(Rotator->CenterX, 0.5f))
			{
				Arguments.Add({ TEXT("CenterX"), FormatDreamShaderFloat(Rotator->CenterX), false });
			}
			if (!FMath::IsNearlyEqual(Rotator->CenterY, 0.5f))
			{
				Arguments.Add({ TEXT("CenterY"), FormatDreamShaderFloat(Rotator->CenterY), false });
			}
			if (!FMath::IsNearlyEqual(Rotator->Speed, 0.25f))
			{
				Arguments.Add({ TEXT("Speed"), FormatDreamShaderFloat(Rotator->Speed), false });
			}
			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, Arguments), false);
		}

		if (UMaterialExpressionWorldPosition* WorldPosition = Cast<UMaterialExpressionWorldPosition>(Expression))
		{
			TArray<FExpressionCallArgument> Arguments;
			if (WorldPosition->WorldPositionShaderOffset != WPT_Default)
			{
				Arguments.Add({
					TEXT("WorldPositionShaderOffset"),
					BuildLiteralEnumArgument(
						StaticEnum<EWorldPositionIncludedOffsets>(),
						WorldPosition->WorldPositionShaderOffset.GetValue()),
					false
				});
			}
			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, Arguments), false);
		}

		if (UMaterialExpressionCameraVectorWS* CameraVector = Cast<UMaterialExpressionCameraVectorWS>(Expression))
		{
			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, {}), false);
		}

		if (IsDreamShaderObjectPositionExpression(Expression))
		{
			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, {}), false);
		}

		if (IsDreamShaderScreenPositionExpression(Expression))
		{
			return MakeExpressionValue(Expression, OutputIndex, BuildUEExpressionCall(Expression, OutputIndex, {}), false);
		}

		if (UMaterialExpressionVertexColor* VertexColor = Cast<UMaterialExpressionVertexColor>(Expression))
		{
			// VertexColor compiles to the full float4 no matter which pin is used -- the pin only carries
			// the mask, which MakeExpressionOutputValue turns into the swizzle. Typing the base value
			// from output 0 (whose mask is RGB) would emit `float3 ... .a` for the alpha pin, and a
			// one-component swizzle of a three-component value does not generate back.
			return MakeExpressionOutputValue(
				MakeExpressionValueWithComponentCount(
					Expression,
					0,
					BuildUEExpressionCallWithOutputType(Expression, 0, TEXT("float4"), {}),
					false,
					4),
				Expression,
				OutputIndex);
		}

		if (UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression))
		{
			TArray<FExpressionCallArgument> Arguments;
			AddTextureSampleExpressionArguments(TextureSample, Arguments);

			const int32 RgbaOutputIndex = FindExpressionOutputIndexByName(Expression, TEXT("RGBA"), OutputIndex);
			const FDecompiledExpressionKey RgbaKey{ Expression, RgbaOutputIndex };
			FDecompiledValue RgbaValue;
			if (const FDecompiledValue* ExistingValue = ExpressionValues.Find(RgbaKey))
			{
				RgbaValue = *ExistingValue;
			}
			else
			{
				RgbaValue = CacheTempExpressionValue(
					RgbaKey,
					MakeValue(BuildUEExpressionCall(Expression, RgbaOutputIndex, Arguments), TEXT("float4"), 4, false),
					TextureSample->Desc.IsEmpty() ? TEXT("TextureSample") : TextureSample->Desc);
			}

			if (OutputIndex == RgbaOutputIndex)
			{
				return RgbaValue;
			}
			return MakeExpressionOutputValue(RgbaValue, Expression, OutputIndex);
		}

		if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			const FString FunctionCallText = BuildMaterialFunctionCall(FunctionCall, OutputIndex);
			return CacheTempExpressionValue(
				Key,
				MakeExpressionValue(Expression, OutputIndex, FunctionCallText, false),
				FunctionCall->MaterialFunction ? FunctionCall->MaterialFunction->GetName() : TEXT("Function"));
		}

		if (UMaterialExpressionCustom* CustomExpression = Cast<UMaterialExpressionCustom>(Expression))
		{
			return CacheTempExpressionValue(
				Key,
				MakeExpressionValue(Expression, OutputIndex, BuildCustomExpressionCall(CustomExpression, OutputIndex), false),
				CustomExpression->Description.IsEmpty() ? TEXT("Custom") : CustomExpression->Description);
		}

		Warnings.AddUnique(FString::Printf(
			TEXT("Exported '%s' as UE.Expression; review reflected literal properties if the node has editor-only state."),
			*Expression->GetClass()->GetName()));
		return CacheTempExpressionValue(
			Key,
			MakeExpressionValue(Expression, OutputIndex, BuildGenericExpressionCall(Expression, OutputIndex), false),
			GetMaterialExpressionShortName(Expression->GetClass()));
	}

	FDecompiledValue FDreamShaderGraphDecompiler::CompileNamedRerouteDeclarationValue( UMaterialExpressionNamedRerouteDeclaration* Declaration, const int32 OutputIndex)
	{
		if (!Declaration)
		{
			return MakeValue(TEXT("0.0"), TEXT("float"), 1, true);
		}

		const FDecompiledExpressionKey Key{ Declaration, OutputIndex };
		if (const FDecompiledValue* ExistingValue = ExpressionValues.Find(Key))
		{
			return *ExistingValue;
		}

		if (CompilingNamedRerouteDeclarations.Contains(Declaration))
		{
			Warnings.AddUnique(FString::Printf(
				TEXT("Detected a recursive named reroute dependency for '%s'; emitted a default literal to avoid stack overflow."),
				*Declaration->Name.ToString()));
			return MakeDefaultValueForExpressionOutput(Declaration, OutputIndex);
		}

		CompilingNamedRerouteDeclarations.Add(Declaration);
		ON_SCOPE_EXIT
		{
			CompilingNamedRerouteDeclarations.Remove(Declaration);
		};

		const FDecompiledValue Value = CompileInputValue(Declaration->Input, MakeDefaultValueForExpressionOutput(Declaration, OutputIndex));
		const FString TempName = MakeUniqueName(Declaration->Name.ToString(), TEXT("Reroute"));
		const FString CachedName = AddTempWithName(Value.Type, Value.Text, TempName);
		return CacheExpressionValue(
			Key,
			MakeValue(
				CachedName,
				Value.Type,
				Value.ComponentCount,
				true,
				Value.bIsTextureObject,
				Value.bIsMaterialAttributes,
				Value.bIsSubstrateMaterial));
	}

	FString FDreamShaderGraphDecompiler::MakeExpressionOutputMaskSuffix(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		if (!Expression || !Expression->Outputs.IsValidIndex(OutputIndex))
		{
			return FString();
		}

		const FExpressionOutput& Output = Expression->Outputs[OutputIndex];

		// Channel selection for an output connection is driven SOLELY by mask bits. The engine's
		// FExpressionInput::Compile applies Compiler->ComponentMask(result, MaskR, MaskG, MaskB, MaskA)
		// (Materials/MaterialShared.cpp), and UMaterialExpressionTextureSample::ApplyChannelNames only
		// rewrites the COSMETIC OutputName -- it never touches the mask. So a channel the author relabels
		// must decompile from its MASK, never its display name: a green-masked output labelled "M" would
		// otherwise emit an invalid ".m", and one labelled "R"/"A" (a swizzle-shaped label that denotes a
		// DIFFERENT channel, as in a packed MRA/ORM texture) would silently select the wrong channel. The
		// mask is authoritative in every case, so derive the swizzle from it and ignore the name.
		FString MaskSwizzle;
		if (Output.MaskR) MaskSwizzle += TEXT("r");
		if (Output.MaskG) MaskSwizzle += TEXT("g");
		if (Output.MaskB) MaskSwizzle += TEXT("b");
		if (Output.MaskA) MaskSwizzle += TEXT("a");

		// Empty mask (no component selection) or a full RGBA mask both mean "pass the whole value".
		return MaskSwizzle == TEXT("rgba") ? FString() : MaskSwizzle;
	}

	FString FDreamShaderGraphDecompiler::MakeExpressionOutputSelection(const FString& ExpressionText, UMaterialExpression* Expression, const int32 OutputIndex) const
	{
		const FString MaskSwizzle = MakeExpressionOutputMaskSuffix(Expression, OutputIndex);
		if (MaskSwizzle.IsEmpty())
		{
			return ExpressionText;
		}

		return MakeSwizzleExpression(ExpressionText, MaskSwizzle);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::ApplyConnectionInputMask(
		const FDecompiledValue& Value,
		UMaterialExpression* Expression,
		const int32 OutputIndex,
		const FString& InputMaskSuffix)
	{
		if (InputMaskSuffix.IsEmpty())
		{
			return Value;
		}

		// FExpressionInput::Connect copies the connected output's mask onto the input, and
		// FExpressionInput::Compile then applies ONLY the input's mask to the node's unmasked result. So an
		// input mask REPLACES the pin's mask rather than stacking on top of it. The two normally agree, but
		// a graph built programmatically (DreamShader's own generator does this) can wire the RGB pin and
		// carry an alpha mask -- valid in Unreal, yet stacking the suffixes emits `.rgb.a`, a swizzle of a
		// swizzle that no longer generates back.
		const FString OutputMaskSuffix = MakeExpressionOutputMaskSuffix(Expression, OutputIndex);
		if (OutputMaskSuffix.Equals(InputMaskSuffix, ESearchCase::IgnoreCase))
		{
			return Value;
		}

		FString BaseText;
		FString TrailingSwizzle;
		if (!OutputMaskSuffix.IsEmpty()
			&& TrySplitTrailingSwizzle(Value.Text, BaseText, TrailingSwizzle)
			&& TrailingSwizzle.Equals(OutputMaskSuffix, ESearchCase::IgnoreCase))
		{
			// Peel the pin's mask back off so the input mask applies to the node's own value. The peeled
			// value is that unmasked output, so it is at least as wide as either mask -- 4 keeps
			// MakeSwizzledValue off its scalar-splat path.
			const FDecompiledValue BaseValue = MakeValue(
				BaseText,
				GetDreamShaderTypeForComponentCount(4),
				4,
				Value.bIsSimple);
			return MakeSwizzledValue(BaseValue, InputMaskSuffix);
		}

		return MakeSwizzledValue(Value, InputMaskSuffix);
	}

	FDecompiledValue FDreamShaderGraphDecompiler::MakeExpressionOutputValue(FDecompiledValue Source, UMaterialExpression* Expression, const int32 OutputIndex) const
	{
		if (!Expression || !Expression->Outputs.IsValidIndex(OutputIndex))
		{
			return Source;
		}

		const FString Text = MakeExpressionOutputSelection(Source.Text, Expression, OutputIndex);
		const int32 ComponentCount = GetComponentCountForExpressionOutput(Expression, OutputIndex);
		const FString Type = GetDreamShaderTypeForExpressionOutput(Expression, OutputIndex);
		if (Type.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase)
			|| Type.Equals(TEXT("Substrate"), ESearchCase::IgnoreCase))
		{
			return MakeValue(
				Text,
				Type,
				0,
				!Text.Contains(TEXT("\n")),
				false,
				Type.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase),
				Type.Equals(TEXT("Substrate"), ESearchCase::IgnoreCase));
		}

		return MakeValue(
			Text,
			GetDreamShaderTypeForComponentCount(ComponentCount),
			ComponentCount,
			!Text.Contains(TEXT("\n")));
	}

	FString FDreamShaderGraphDecompiler::BuildUEExpressionCall(UMaterialExpression* Expression, const int32 OutputIndex, const TArray<FExpressionCallArgument>& Arguments) const
	{
		return BuildUEExpressionCallWithOutputType(
			Expression,
			OutputIndex,
			GetDreamShaderTypeForExpressionOutput(Expression, OutputIndex),
			Arguments);
	}

	bool FDreamShaderGraphDecompiler::IsControlExpressionArgumentName(const FString& Name)
	{
		const FString NormalizedName = UE::DreamShader::NormalizeSettingKey(Name);
		return NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("Class"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputType"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("ResultType"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("Output"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputName"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("OutputIndex"));
	}

	bool FDreamShaderGraphDecompiler::IsEditorOnlyExpressionPropertyName(const FString& Name)
	{
		const FString NormalizedName = UE::DreamShader::NormalizeSettingKey(Name);
		return NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("MaterialExpressionEditorX"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("MaterialExpressionEditorY"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("Desc"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bCommentBubbleVisible"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bShowOutputNameOnPin"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bHidePreviewWindow"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bCollapsed"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("bShaderInputData"))
			|| NormalizedName == UE::DreamShader::NormalizeSettingKey(TEXT("SortPriority"));
	}

	bool FDreamShaderGraphDecompiler::IsReflectedExpressionLiteralProperty(const FProperty* Property)
	{
		if (!Property
			|| Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient | CPF_DuplicateTransient)
			|| IsMaterialExpressionInputProperty(Property)
			|| !Property->HasAnyPropertyFlags(CPF_Edit)
			|| IsControlExpressionArgumentName(Property->GetName())
			|| IsEditorOnlyExpressionPropertyName(Property->GetName()))
		{
			return false;
		}

		return CastField<FBoolProperty>(Property) != nullptr
			|| CastField<FNumericProperty>(Property) != nullptr
			|| CastField<FEnumProperty>(Property) != nullptr
			|| CastField<FByteProperty>(Property) != nullptr
			|| CastField<FNameProperty>(Property) != nullptr
			|| CastField<FStrProperty>(Property) != nullptr
			|| CastField<FTextProperty>(Property) != nullptr
			|| CastField<FObjectPropertyBase>(Property) != nullptr;
	}

	bool FDreamShaderGraphDecompiler::IsReflectedPropertyDefaultValue(const UObject* Object, const FProperty* Property)
	{
		if (!Object || !Property)
		{
			return true;
		}

		UObject* DefaultObject = Object->GetClass() ? Object->GetClass()->GetDefaultObject(false) : nullptr;
		if (!DefaultObject)
		{
			return false;
		}

		return Property->Identical_InContainer(Object, DefaultObject);
	}

	bool FDreamShaderGraphDecompiler::TryBuildReflectedExpressionLiteralArgument( const UMaterialExpression* Expression, const FProperty* Property, FExpressionCallArgument& OutArgument)
	{
		if (!Expression || !Property || !IsReflectedExpressionLiteralProperty(Property) || IsReflectedPropertyDefaultValue(Expression, Property))
		{
			return false;
		}

		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Expression);
		if (!ValuePtr)
		{
			return false;
		}

		FString ValueText;
		if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			ValueText = BoolProperty->GetPropertyValue(ValuePtr) ? TEXT("true") : TEXT("false");
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProperty->GetEnum())
			{
				ValueText = BuildLiteralEnumArgument(Enum, EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr));
			}
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (UEnum* Enum = ByteProperty->Enum)
			{
				ValueText = BuildLiteralEnumArgument(Enum, ByteProperty->GetPropertyValue(ValuePtr));
			}
			else
			{
				ValueText = FString::FromInt(ByteProperty->GetPropertyValue(ValuePtr));
			}
		}
		else if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			if (NumericProperty->IsFloatingPoint())
			{
				ValueText = FormatDreamShaderFloat(NumericProperty->GetFloatingPointPropertyValue(ValuePtr));
			}
			else if (NumericProperty->IsInteger())
			{
				if (CastField<FUInt16Property>(Property)
					|| CastField<FUInt32Property>(Property)
					|| CastField<FUInt64Property>(Property))
				{
					ValueText = FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(NumericProperty->GetUnsignedIntPropertyValue(ValuePtr)));
				}
				else
				{
					ValueText = FString::Printf(TEXT("%lld"), static_cast<long long>(NumericProperty->GetSignedIntPropertyValue(ValuePtr)));
				}
			}
		}
		else if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			const FName NameValue = NameProperty->GetPropertyValue(ValuePtr);
			if (!NameValue.IsNone())
			{
				ValueText = FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(NameValue.ToString()));
			}
		}
		else if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			ValueText = FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(StringProperty->GetPropertyValue(ValuePtr)));
		}
		else if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			ValueText = FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(TextProperty->GetPropertyValue(ValuePtr).ToString()));
		}
		else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (const UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(ValuePtr))
			{
				ValueText = MakeDreamShaderObjectPathLiteral(ObjectValue);
			}
		}

		if (ValueText.TrimStartAndEnd().IsEmpty())
		{
			return false;
		}

		OutArgument = { Property->GetName(), ValueText, false };
		return true;
	}

	void FDreamShaderGraphDecompiler::AddReflectedExpressionLiteralArguments( const UMaterialExpression* Expression, TArray<FExpressionCallArgument>& Arguments) const
	{
		if (!Expression)
		{
			return;
		}

		TSet<FString> ExistingArgumentNames;
		for (const FExpressionCallArgument& Argument : Arguments)
		{
			ExistingArgumentNames.Add(UE::DreamShader::NormalizeSettingKey(Argument.Name));
		}

		for (TFieldIterator<FProperty> It(Expression->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property || ExistingArgumentNames.Contains(UE::DreamShader::NormalizeSettingKey(Property->GetName())))
			{
				continue;
			}

			FExpressionCallArgument ReflectedArgument;
			if (TryBuildReflectedExpressionLiteralArgument(Expression, Property, ReflectedArgument))
			{
				ExistingArgumentNames.Add(UE::DreamShader::NormalizeSettingKey(ReflectedArgument.Name));
				Arguments.Add(ReflectedArgument);
			}
		}
	}

	FString FDreamShaderGraphDecompiler::BuildGenericExpressionCall(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		TArray<FExpressionCallArgument> Arguments;
		if (Expression)
		{
			for (int32 InputIndex = 0; InputIndex < GetDreamShaderExpressionInputCount(Expression); ++InputIndex)
			{
				FExpressionInput* Input = Expression->GetInput(InputIndex);
				if (!Input || !Input->IsConnected())
				{
					continue;
				}

				const FName InputName = Expression->GetInputName(InputIndex);
				const FString ArgumentName = MakeDreamShaderDeclarationName(
					InputName.IsNone() ? FString::Printf(TEXT("Input%d"), InputIndex) : InputName.ToString(),
					TEXT("Input"),
					InputIndex);
				Arguments.Add({ ArgumentName, CompileInput(*Input, TEXT("0.0")), true });
			}

			AddReflectedExpressionLiteralArguments(Expression, Arguments);
		}

		return BuildUEExpressionCall(Expression, OutputIndex, Arguments);
	}

	FString FDreamShaderGraphDecompiler::BuildCustomExpressionCall(UMaterialExpressionCustom* CustomExpression, const int32 OutputIndex)
	{
		TArray<FExpressionCallArgument> Arguments;
		Arguments.Add({ TEXT("Code"), FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderCodeString(CustomExpression ? CustomExpression->Code : FString())), false });
		if (CustomExpression && !CustomExpression->Description.IsEmpty())
		{
			Arguments.Add({ TEXT("Description"), FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(CustomExpression->Description)), false });
		}
		if (CustomExpression && OutputIndex > 0)
		{
			const FString OutputName = GetCustomOutputName(CustomExpression, OutputIndex);
			if (!OutputName.IsEmpty())
			{
				Arguments.Add({ TEXT("Output"), FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(OutputName)), false });
			}
		}
		if (CustomExpression && CustomExpression->AdditionalOutputs.Num() > 0)
		{
			// Unreal turns each additional output into an `inout` parameter of the generated Custom
			// function, so the code body can only assign to one that the node actually declares. A node
			// is emitted once per output it feeds, and only the emission that selects the extra output
			// used to declare it -- the other one compiled to "use of undeclared identifier". Declare the
			// full set (names and per-output types) on every emission instead.
			const UEnum* CustomOutputTypeEnum = StaticEnum<ECustomMaterialOutputType>();
			TArray<FString> AdditionalOutputTexts;
			AdditionalOutputTexts.Reserve(CustomExpression->AdditionalOutputs.Num());
			for (const FCustomOutput& AdditionalOutput : CustomExpression->AdditionalOutputs)
			{
				AdditionalOutputTexts.Add(FString::Printf(
					TEXT("(OutputName=\"%s\",OutputType=%s)"),
					*AdditionalOutput.OutputName.ToString(),
					*GetEnumLiteralText(CustomOutputTypeEnum, static_cast<int64>(AdditionalOutput.OutputType))));
			}

			Arguments.Add({
				TEXT("AdditionalOutputs"),
				FString::Printf(
					TEXT("\"%s\""),
					*EscapeDreamShaderString(FString::Printf(TEXT("(%s)"), *FString::Join(AdditionalOutputTexts, TEXT(","))))),
				false });
		}
		if (CustomExpression)
		{
			for (const FCustomInput& CustomInput : CustomExpression->Inputs)
			{
				if (!CustomInput.Input.IsConnected())
				{
					continue;
				}
				const FString ArgumentName = MakeDreamShaderDeclarationName(CustomInput.InputName.ToString(), TEXT("Input"), Arguments.Num());
				Arguments.Add({ ArgumentName, CompileInput(CustomInput.Input, TEXT("0.0")), true });
			}
		}

		// OutputType on a Custom node declares the node's own return type, not the type of the output
		// this call reads -- an additional output is typed by its own AdditionalOutputs entry. Emitting
		// the selected output's type here would rewrite the node's return type (float4 -> float for the
		// alpha-ish secondary output) and drag the secondary output's type along with it.
		return BuildUEExpressionCallWithOutputType(
			CustomExpression,
			OutputIndex,
			GetDreamShaderTypeForCustomOutputType(CustomExpression ? CustomExpression->OutputType.GetValue() : CMOT_Float1),
			Arguments);
	}

	FString FDreamShaderGraphDecompiler::BuildMaterialFunctionCall(UMaterialExpressionMaterialFunctionCall* FunctionCall, const int32 OutputIndex)
	{
		if (!FunctionCall || !FunctionCall->MaterialFunction)
		{
			Warnings.AddUnique(TEXT("A MaterialFunctionCall had no function asset and was exported as a zero literal."));
			return TEXT("0.0");
		}

		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(FunctionCall->MaterialFunction);
		if (!MaterialFunction)
		{
			Warnings.AddUnique(FString::Printf(
				TEXT("MaterialFunctionCall '%s' is not a plain MaterialFunction; it was exported through UE.Expression."),
				*FunctionCall->MaterialFunction->GetPathName()));
			return BuildGenericExpressionCall(FunctionCall, OutputIndex);
		}

		const FString FunctionName = EnsureVirtualFunctionDefinition(MaterialFunction);
		TArray<FString> Arguments;
		for (int32 InputIndex = 0; InputIndex < FunctionCall->FunctionInputs.Num(); ++InputIndex)
		{
			const FFunctionExpressionInput& FunctionInput = FunctionCall->FunctionInputs[InputIndex];
			if (FunctionInput.Input.IsConnected())
			{
				Arguments.Add(CompileInput(FunctionInput.Input, TEXT("default")));
			}
			else
			{
				Arguments.Add(TEXT("default"));
			}
		}

		if (OutputIndex > 0 || FunctionCall->FunctionOutputs.Num() > 1)
		{
			Arguments.Add(FString::Printf(TEXT("OutputIndex=%d"), OutputIndex));
		}

		return FString::Printf(TEXT("%s(%s)"), *FunctionName, *FString::Join(Arguments, TEXT(", ")));
	}

	FString FDreamShaderGraphDecompiler::EnsureVirtualFunctionDefinition(UMaterialFunction* MaterialFunction)
	{
		if (!MaterialFunction)
		{
			return TEXT("MissingFunction");
		}

		if (const FString* ExistingName = VirtualFunctionNames.Find(MaterialFunction))
		{
			return *ExistingName;
		}

		FString DefinitionText;
		FString Error;
		const FString FunctionName = MakeDreamShaderDeclarationName(MaterialFunction->GetName(), TEXT("VirtualFunction"), VirtualFunctionNames.Num());
		if (BuildVirtualFunctionDefinition(MaterialFunction, DefinitionText, Error))
		{
			VirtualFunctionDefinitions.Add(DefinitionText);
		}
		else
		{
			Warnings.AddUnique(FString::Printf(
				TEXT("Failed to emit VirtualFunction for '%s': %s"),
				*MaterialFunction->GetPathName(),
				*Error));
		}

		VirtualFunctionNames.Add(MaterialFunction, FunctionName);
		return FunctionName;
	}

	void FDreamShaderGraphDecompiler::EnterExpressionProgressFrame(const UMaterialExpression* Expression)
	{
		if (!ActiveDecompileSlowTask || !Expression || ProgressVisitedExpressions.Contains(Expression))
		{
			return;
		}

		ProgressVisitedExpressions.Add(Expression);
		ActiveDecompileSlowTask->EnterProgressFrame(1.0f, FText::FromString(FString::Printf(
			TEXT("Decompiling node %d: %s"),
			ProgressVisitedExpressions.Num(),
			*GetMaterialExpressionShortName(Expression->GetClass()))));
	}

}
