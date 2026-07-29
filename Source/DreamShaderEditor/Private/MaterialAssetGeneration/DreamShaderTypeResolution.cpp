// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShaderLang type-resolution helpers (output-type component counts, MaterialAttributes /
// Substrate type predicates, code-declared type resolution, and output validation), extracted from
// DreamShaderMaterialGeneratorSupport.cpp. Header-declared in DreamShaderMaterialGeneratorPrivate.h
// and shared across the generation translation units.

#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Materials/MaterialExpressionFunctionInput.h"

namespace UE::DreamShader::Editor::Private
{
	ETextShaderTextureType ResolveEffectiveTextureType(const FTextShaderPropertyDefinition& Property)
	{
		// Type tokens that name the dimension win outright -- declaring `Texture2D` and pointing it at a
		// Texture2DArray stays an error. Tokens like TextureObjectParameter map to one Unreal node class
		// that carries any dimension, so the assigned asset is the only thing that knows which it is.
		if (Property.bHasExplicitTextureType || Property.TextureDefaultObjectPath.IsEmpty())
		{
			return Property.TextureType;
		}

		const UTexture* DefaultTexture = LoadObject<UTexture>(nullptr, *Property.TextureDefaultObjectPath);
		if (!DefaultTexture)
		{
			return Property.TextureType;
		}

		if (DefaultTexture->IsA<UTextureCube>())
		{
			return ETextShaderTextureType::TextureCube;
		}
		if (DefaultTexture->IsA<UTexture2DArray>())
		{
			return ETextShaderTextureType::Texture2DArray;
		}
		if (DefaultTexture->IsA<UVolumeTexture>())
		{
			return ETextShaderTextureType::VolumeTexture;
		}

		return ETextShaderTextureType::Texture2D;
	}

	bool TryGetComponentCountForOutputType(const ECustomMaterialOutputType OutputType, int32& OutComponentCount)
	{
		switch (OutputType)
		{
		case CMOT_Float1:
			OutComponentCount = 1;
			return true;
		case CMOT_Float2:
			OutComponentCount = 2;
			return true;
		case CMOT_Float3:
			OutComponentCount = 3;
			return true;
		case CMOT_Float4:
			OutComponentCount = 4;
			return true;
		case CMOT_MaterialAttributes:
			OutComponentCount = 0;
			return true;
		default:
			return false;
		}
	}

	bool IsMaterialAttributesType(const FString& InTypeName)
	{
		FString TypeName = InTypeName;
		TypeName.TrimStartAndEndInline();
		TypeName.ReplaceInline(TEXT(" "), TEXT(""));
		return TypeName.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase);
	}

	bool IsSubstrateMaterialType(const FString& InTypeName)
	{
		FString TypeName = InTypeName;
		TypeName.TrimStartAndEndInline();
		TypeName.ReplaceInline(TEXT(" "), TEXT(""));
		return TypeName.Equals(TEXT("Substrate"), ESearchCase::IgnoreCase);
	}

	bool IsSubstrateMaterialTypeSupported()
	{
		return !!DREAMSHADER_WITH_SUBSTRATE_BUILTINS;
	}

	bool TryResolveCodeDeclaredType(
		const FString& InTypeName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		ETextShaderTextureType& OutTextureType,
		bool& bOutIsSubstrateMaterial)
	{
		bOutIsTexture = false;
		bOutIsSubstrateMaterial = false;
		OutTextureType = ETextShaderTextureType::Texture2D;

		if (IsSubstrateMaterialType(InTypeName))
		{
			if (!IsSubstrateMaterialTypeSupported())
			{
				return false;
			}
			OutComponentCount = 0;
			bOutIsSubstrateMaterial = true;
			return true;
		}

		// StaticBool is a one-component compile-time value: it needs to resolve here so call sites can
		// type-check arguments passed to a StaticBool function input (TryResolveMaterialFunctionParameterType
		// maps the same token to FunctionInput_StaticBool when the input node itself is created).
		if (InTypeName.Equals(TEXT("StaticBool"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("StaticBoolParameter"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 1;
			return true;
		}

		ECustomMaterialOutputType OutputType = CMOT_Float1;
		if (TryResolveCustomOutputType(InTypeName, OutputType) && TryGetComponentCountForOutputType(OutputType, OutComponentCount))
		{
			return true;
		}

		if (InTypeName.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("SamplerState"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 0;
			bOutIsTexture = true;
			OutTextureType = ETextShaderTextureType::Texture2D;
			return true;
		}
		if (InTypeName.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 0;
			bOutIsTexture = true;
			OutTextureType = ETextShaderTextureType::TextureCube;
			return true;
		}
		if (InTypeName.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 0;
			bOutIsTexture = true;
			OutTextureType = ETextShaderTextureType::Texture2DArray;
			return true;
		}
		if (InTypeName.Equals(TEXT("Texture3D"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("VolumeTexture"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 0;
			bOutIsTexture = true;
			OutTextureType = ETextShaderTextureType::VolumeTexture;
			return true;
		}

		return false;
	}

	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture, ETextShaderTextureType& OutTextureType)
	{
		bool bIsSubstrateMaterial = false;
		return TryResolveCodeDeclaredType(InTypeName, OutComponentCount, bOutIsTexture, OutTextureType, bIsSubstrateMaterial);
	}

	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture, bool& bOutIsSubstrateMaterial)
	{
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		return TryResolveCodeDeclaredType(InTypeName, OutComponentCount, bOutIsTexture, TextureType, bOutIsSubstrateMaterial);
	}

	bool TryResolveCodeDeclaredType(const FString& InTypeName, int32& OutComponentCount, bool& bOutIsTexture)
	{
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		return TryResolveCodeDeclaredType(InTypeName, OutComponentCount, bOutIsTexture, TextureType);
	}

	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		ETextShaderTextureType& OutTextureType,
		bool& bOutIsSubstrateMaterial)
	{
		bOutIsSubstrateMaterial = false;
		for (const FTextShaderVariableDeclaration& Declaration : Definition.OutputDeclarations)
		{
			if (Declaration.Name.Equals(VariableName, ESearchCase::IgnoreCase))
			{
				return TryResolveCodeDeclaredType(Declaration.Type, OutComponentCount, bOutIsTexture, OutTextureType, bOutIsSubstrateMaterial);
			}
		}

		return false;
	}

	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		ETextShaderTextureType& OutTextureType)
	{
		bool bIsSubstrateMaterial = false;
		return TryResolveOutputVariableComponentCount(Definition, VariableName, OutComponentCount, bOutIsTexture, OutTextureType, bIsSubstrateMaterial);
	}

	bool TryResolveOutputVariableComponentCount(
		const FTextShaderDefinition& Definition,
		const FString& VariableName,
		int32& OutComponentCount,
		bool& bOutIsTexture)
	{
		ETextShaderTextureType TextureType = ETextShaderTextureType::Texture2D;
		return TryResolveOutputVariableComponentCount(Definition, VariableName, OutComponentCount, bOutIsTexture, TextureType);
	}

	bool TryResolveMaterialFunctionParameterType(
		const FString& InTypeName,
		int32& OutComponentCount,
		bool& bOutIsTexture,
		int32& OutFunctionInputTypeValue,
		bool& bOutIsSubstrateMaterial)
	{
		bOutIsSubstrateMaterial = false;
		if (InTypeName.Equals(TEXT("StaticBool"), ESearchCase::IgnoreCase)
			|| InTypeName.Equals(TEXT("StaticBoolParameter"), ESearchCase::IgnoreCase))
		{
			OutComponentCount = 1;
			bOutIsTexture = false;
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_StaticBool);
			return true;
		}

		if (IsMaterialAttributesType(InTypeName))
		{
			OutComponentCount = 0;
			bOutIsTexture = false;
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_MaterialAttributes);
			return true;
		}

		if (IsSubstrateMaterialType(InTypeName))
		{
			if (!IsSubstrateMaterialTypeSupported())
			{
				return false;
			}
			OutComponentCount = 0;
			bOutIsTexture = false;
			bOutIsSubstrateMaterial = true;
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Substrate);
			return true;
		}

		if (!TryResolveCodeDeclaredType(InTypeName, OutComponentCount, bOutIsTexture, bOutIsSubstrateMaterial))
		{
			return false;
		}

		if (bOutIsTexture)
		{
			if (InTypeName.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase))
			{
				OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_TextureCube);
			}
			else if (InTypeName.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase))
			{
				OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Texture2DArray);
			}
			else if (InTypeName.Equals(TEXT("Texture3D"), ESearchCase::IgnoreCase)
				|| InTypeName.Equals(TEXT("VolumeTexture"), ESearchCase::IgnoreCase))
			{
				OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_VolumeTexture);
			}
			else
			{
				OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Texture2D);
			}
			return true;
		}

		switch (OutComponentCount)
		{
		case 1:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Scalar);
			return true;
		case 2:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Vector2);
			return true;
		case 3:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Vector3);
			return true;
		case 4:
			OutFunctionInputTypeValue = static_cast<int32>(FunctionInput_Vector4);
			return true;
		default:
			return false;
		}
	}

	bool ValidateOutputs(
		const FTextShaderDefinition& Definition,
		TArray<FResolvedNamedOutput>& OutNamedOutputs,
		bool& bOutUsesReturn,
		ECustomMaterialOutputType& OutReturnType,
		bool& bOutReturnIsSubstrateMaterial,
		FString& OutError)
	{
		const auto IsSimpleOutputReference = [](const FString& InText) -> bool
		{
			const FString Candidate = InText.TrimStartAndEnd();
			if (Candidate.IsEmpty())
			{
				return false;
			}

			for (int32 Index = 0; Index < Candidate.Len(); ++Index)
			{
				const TCHAR Char = Candidate[Index];
				if (Index == 0)
				{
					if (!(FChar::IsAlpha(Char) || Char == TCHAR('_')))
					{
						return false;
					}
				}
				else if (!(FChar::IsAlnum(Char) || Char == TCHAR('_')))
				{
					return false;
				}
			}

			return true;
		};

		OutNamedOutputs.Reset();
		bOutUsesReturn = false;
		OutReturnType = CMOT_Float1;
		bOutReturnIsSubstrateMaterial = false;

		TMap<FString, ECustomMaterialOutputType> DeclaredOutputTypes;
		TMap<FString, FString> DeclaredOutputTypeTexts;
		TMap<FString, bool> DeclaredOutputSubstrateTypes;
		for (const FTextShaderVariableDeclaration& Declaration : Definition.OutputDeclarations)
		{
			if (Declaration.Name.Equals(TEXT("return"), ESearchCase::IgnoreCase))
			{
				OutError = TEXT("Outputs declarations cannot use the reserved name 'return'.");
				return false;
			}

			ECustomMaterialOutputType DeclaredType = CMOT_Float1;
			const bool bDeclaredSubstrate = IsSubstrateMaterialType(Declaration.Type);
			if (bDeclaredSubstrate && !IsSubstrateMaterialTypeSupported())
			{
				OutError = FString::Printf(TEXT("Output '%s' uses Substrate, which requires Unreal Engine 5.4 or newer."), *Declaration.Name);
				return false;
			}
			if (!bDeclaredSubstrate && !TryResolveCustomOutputType(Declaration.Type, DeclaredType))
			{
				OutError = FString::Printf(TEXT("Unsupported output type '%s' for '%s'."), *Declaration.Type, *Declaration.Name);
				return false;
			}

			if (const ECustomMaterialOutputType* ExistingType = DeclaredOutputTypes.Find(Declaration.Name))
			{
				const bool bExistingSubstrate = DeclaredOutputSubstrateTypes.FindRef(Declaration.Name);
				if (*ExistingType != DeclaredType || bExistingSubstrate != bDeclaredSubstrate)
				{
					OutError = FString::Printf(TEXT("Output variable '%s' is declared with conflicting types."), *Declaration.Name);
					return false;
				}
			}
			else
			{
				DeclaredOutputTypes.Add(Declaration.Name, DeclaredType);
				DeclaredOutputTypeTexts.Add(Declaration.Name, Declaration.Type);
				DeclaredOutputSubstrateTypes.Add(Declaration.Name, bDeclaredSubstrate);
			}
		}

		TMap<FString, int32> OutputOrder;
		for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
		{
			const FString SourceName = Binding.SourceText.TrimStartAndEnd();
			const bool bIsSimpleSourceReference = IsSimpleOutputReference(SourceName);
			ECustomMaterialOutputType BindingOutputType = CMOT_Float1;
			bool bBindingIsSubstrateMaterial = false;
			bool bHasImplicitTypeFromTarget = false;
			if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::MaterialProperty)
			{
				FResolvedMaterialProperty ResolvedProperty;
				if (!ResolveMaterialProperty(Binding.MaterialProperty, ResolvedProperty))
				{
					if (Binding.MaterialProperty.TrimStartAndEnd().Equals(TEXT("FrontMaterial"), ESearchCase::IgnoreCase))
					{
						OutError = TEXT("Base.FrontMaterial requires Unreal Engine 5.4 or newer.");
						return false;
					}
					OutError = FString::Printf(TEXT("Unsupported material output '%s'."), *Binding.MaterialProperty);
					return false;
				}

				BindingOutputType = ResolvedProperty.OutputType;
				bBindingIsSubstrateMaterial = ResolvedProperty.bIsSubstrateMaterial;
				bHasImplicitTypeFromTarget = true;
			}

			if (SourceName.Equals(TEXT("return"), ESearchCase::IgnoreCase))
			{
				if (Binding.TargetKind != FTextShaderOutputBinding::ETargetKind::MaterialProperty)
				{
					OutError = TEXT("The reserved output name 'return' can only bind to Base material properties.");
					return false;
				}

				if (!bOutUsesReturn)
				{
					bOutUsesReturn = true;
					OutReturnType = BindingOutputType;
					bOutReturnIsSubstrateMaterial = bBindingIsSubstrateMaterial;
				}
				else if (OutReturnType != BindingOutputType || bOutReturnIsSubstrateMaterial != bBindingIsSubstrateMaterial)
				{
					OutError = TEXT("The return value is bound to material properties with incompatible types.");
					return false;
				}

				continue;
			}

			if (!bIsSimpleSourceReference)
			{
				continue;
			}

			if (const int32* ExistingIndex = OutputOrder.Find(SourceName))
			{
				if ((OutNamedOutputs[*ExistingIndex].OutputType != BindingOutputType
					|| OutNamedOutputs[*ExistingIndex].bIsSubstrateMaterial != bBindingIsSubstrateMaterial)
					&& bHasImplicitTypeFromTarget)
				{
					OutError = FString::Printf(TEXT("Output variable '%s' is bound to incompatible material properties."), *SourceName);
					return false;
				}
			}
			else
			{
				FResolvedNamedOutput& Output = OutNamedOutputs.AddDefaulted_GetRef();
				Output.Name = SourceName;
				Output.OutputType = BindingOutputType;
				Output.bIsSubstrateMaterial = bBindingIsSubstrateMaterial;

				if (const ECustomMaterialOutputType* DeclaredType = DeclaredOutputTypes.Find(SourceName))
				{
					const bool bDeclaredSubstrate = DeclaredOutputSubstrateTypes.FindRef(SourceName);
					if (bHasImplicitTypeFromTarget && (*DeclaredType != BindingOutputType || bDeclaredSubstrate != bBindingIsSubstrateMaterial))
					{
						OutError = FString::Printf(
							TEXT("Output variable '%s' is declared as '%s' but bound material property '%s' expects a different type."),
							*SourceName,
							*DeclaredOutputTypeTexts.FindChecked(SourceName),
							*Binding.MaterialProperty);
						return false;
					}

					Output.OutputType = *DeclaredType;
					Output.bIsSubstrateMaterial = bDeclaredSubstrate;
				}
				else if (!bHasImplicitTypeFromTarget)
				{
					OutError = FString::Printf(
						TEXT("Output variable '%s' must declare an explicit type before binding to expression target '%s'."),
						*SourceName,
						*Binding.TargetText);
					return false;
				}

				OutputOrder.Add(SourceName, OutNamedOutputs.Num() - 1);
			}
		}

		return true;
	}
}
