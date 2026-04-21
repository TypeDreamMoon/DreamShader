#include "DreamShaderParserInternal.h"

#include "DreamShaderModule.h"

namespace UE::DreamShader::Private
{
	static bool TryResolveExplicitOutputSignature(
		const TMap<FString, FString>& Arguments,
		ETextShaderPropertyType& OutType,
		int32& OutComponentCount)
	{
		const FString* TypeText = Arguments.Find(NormalizeSettingKey(TEXT("OutputType")));
		if (!TypeText)
		{
			TypeText = Arguments.Find(NormalizeSettingKey(TEXT("ResultType")));
		}

		if (!TypeText)
		{
			return false;
		}

		FString Value = TypeText->TrimStartAndEnd();
		Value.ToLowerInline();
		Value.ReplaceInline(TEXT(" "), TEXT(""));

		if (Value == TEXT("float")
			|| Value == TEXT("float1")
			|| Value == TEXT("half")
			|| Value == TEXT("half1")
			|| Value == TEXT("int")
			|| Value == TEXT("uint")
			|| Value == TEXT("bool"))
		{
			OutType = ETextShaderPropertyType::Scalar;
			OutComponentCount = 1;
			return true;
		}

		if (Value == TEXT("float2")
			|| Value == TEXT("half2")
			|| Value == TEXT("vec2")
			|| Value == TEXT("int2")
			|| Value == TEXT("uint2")
			|| Value == TEXT("bool2")
			|| Value == TEXT("ivec2")
			|| Value == TEXT("uvec2")
			|| Value == TEXT("bvec2"))
		{
			OutType = ETextShaderPropertyType::Vector;
			OutComponentCount = 2;
			return true;
		}

		if (Value == TEXT("float3")
			|| Value == TEXT("half3")
			|| Value == TEXT("vec3")
			|| Value == TEXT("int3")
			|| Value == TEXT("uint3")
			|| Value == TEXT("bool3")
			|| Value == TEXT("ivec3")
			|| Value == TEXT("uvec3")
			|| Value == TEXT("bvec3"))
		{
			OutType = ETextShaderPropertyType::Vector;
			OutComponentCount = 3;
			return true;
		}

		if (Value == TEXT("float4")
			|| Value == TEXT("half4")
			|| Value == TEXT("vec4")
			|| Value == TEXT("int4")
			|| Value == TEXT("uint4")
			|| Value == TEXT("bool4")
			|| Value == TEXT("ivec4")
			|| Value == TEXT("uvec4")
			|| Value == TEXT("bvec4"))
		{
			OutType = ETextShaderPropertyType::Vector;
			OutComponentCount = 4;
			return true;
		}

		if (Value == TEXT("texture2d")
			|| Value == TEXT("texturecube")
			|| Value == TEXT("texture2darray"))
		{
			OutType = ETextShaderPropertyType::Texture2D;
			OutComponentCount = 0;
			return true;
		}

		return false;
	}

	bool TryResolveUEBuiltinOutputSignature(
		const FString& InFunctionName,
		ETextShaderPropertyType& OutType,
		int32& OutComponentCount)
	{
		const auto Matches = [&InFunctionName](const TCHAR* Candidate)
		{
			return InFunctionName.Equals(Candidate, ESearchCase::IgnoreCase);
		};

		if (Matches(TEXT("TexCoord")) || Matches(TEXT("Panner")))
		{
			OutType = ETextShaderPropertyType::Vector;
			OutComponentCount = 2;
			return true;
		}

		if (Matches(TEXT("Time")))
		{
			OutType = ETextShaderPropertyType::Scalar;
			OutComponentCount = 1;
			return true;
		}

		if (Matches(TEXT("WorldPosition")) || Matches(TEXT("CameraVectorWS")) || Matches(TEXT("ObjectPositionWS")))
		{
			OutType = ETextShaderPropertyType::Vector;
			OutComponentCount = 3;
			return true;
		}

		if (Matches(TEXT("ScreenPosition")) || Matches(TEXT("VertexColor")))
		{
			OutType = ETextShaderPropertyType::Vector;
			OutComponentCount = 4;
			return true;
		}

		return false;
	}

	bool ParseUEBuiltinPropertyType(
		const FString& InTypeToken,
		FTextShaderPropertyDefinition& OutProperty,
		FString& OutError)
	{
		FString CallSpec = InTypeToken.TrimStartAndEnd();
		if (!CallSpec.StartsWith(TEXT("UE."), ESearchCase::IgnoreCase))
		{
			return false;
		}

		CallSpec.RightChopInline(3, EAllowShrinking::No);
		CallSpec.TrimStartAndEndInline();
		if (CallSpec.IsEmpty())
		{
			OutError = TEXT("UE builtin property declarations must specify a function name, for example UE.TexCoord UV.");
			return false;
		}

		FString FunctionName = CallSpec;
		FString ArgumentBlock;
		const int32 OpenParenIndex = CallSpec.Find(TEXT("("));
		if (OpenParenIndex != INDEX_NONE)
		{
			const int32 CloseParenIndex = CallSpec.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (CloseParenIndex == INDEX_NONE || CloseParenIndex < OpenParenIndex)
			{
				OutError = FString::Printf(TEXT("Invalid UE builtin declaration '%s'."), *InTypeToken);
				return false;
			}

			if (!CallSpec.Mid(CloseParenIndex + 1).TrimStartAndEnd().IsEmpty())
			{
				OutError = FString::Printf(TEXT("Unexpected characters after UE builtin argument list in '%s'."), *InTypeToken);
				return false;
			}

			FunctionName = CallSpec.Left(OpenParenIndex).TrimStartAndEnd();
			ArgumentBlock = CallSpec.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1).TrimStartAndEnd();
		}

		if (FunctionName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Invalid UE builtin declaration '%s'."), *InTypeToken);
			return false;
		}

		OutProperty.Source = ETextShaderPropertySource::UEBuiltin;
		OutProperty.UEBuiltinFunctionName = FunctionName;
		OutProperty.UEBuiltinArguments.Reset();

		for (const FString& ArgumentStatement : SplitTopLevelDelimited(ArgumentBlock, TCHAR(',')))
		{
			FString ArgumentName;
			FString ArgumentValue;
			if (!SplitTopLevelAssignment(ArgumentStatement, ArgumentName, ArgumentValue))
			{
				OutError = FString::Printf(
					TEXT("UE builtin argument '%s' must use named syntax like Key=Value in '%s'."),
					*ArgumentStatement,
					*InTypeToken);
				return false;
			}

			ArgumentName = NormalizeSettingKey(ArgumentName);
			ArgumentValue = Unquote(ArgumentValue).TrimStartAndEnd();
			if (ArgumentName.IsEmpty() || ArgumentValue.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Invalid UE builtin argument '%s' in '%s'."), *ArgumentStatement, *InTypeToken);
				return false;
			}

			if (OutProperty.UEBuiltinArguments.Contains(ArgumentName))
			{
				OutError = FString::Printf(
					TEXT("UE builtin argument '%s' is declared more than once in '%s'."),
					*ArgumentName,
					*InTypeToken);
				return false;
			}

			OutProperty.UEBuiltinArguments.Add(ArgumentName, ArgumentValue);
		}

		ETextShaderPropertyType BuiltinType = ETextShaderPropertyType::Scalar;
		int32 BuiltinComponentCount = 1;
		if (TryResolveUEBuiltinOutputSignature(FunctionName, BuiltinType, BuiltinComponentCount)
			|| TryResolveExplicitOutputSignature(OutProperty.UEBuiltinArguments, BuiltinType, BuiltinComponentCount))
		{
			OutProperty.Type = BuiltinType;
			OutProperty.ComponentCount = BuiltinComponentCount;
			return true;
		}

		OutError = FString::Printf(
			TEXT("Unsupported UE builtin function '%s'. Use OutputType=\"float1/2/3/4/Texture2D\" for generic MaterialExpression calls."),
			*FunctionName);
		return false;
	}

	bool ParsePropertyStatements(const FString& BlockContent, TArray<FTextShaderPropertyDefinition>& OutProperties, FString& OutError)
	{
		const TArray<FString> Statements = SplitStatements(RemoveComments(BlockContent));

		for (const FString& Statement : Statements)
		{
			FString Trimmed = Statement.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}

			FTextShaderPropertyDefinition Property;

			FString Left = Trimmed;
			FString Right;
			if (SplitTopLevelAssignment(Trimmed, Left, Right))
			{
				Property.bHasDefaultValue = true;
			}

			FString TypeToken;
			FString NameToken;
			if (!SplitDeclarationTypeAndName(Left, TypeToken, NameToken))
			{
				OutError = FString::Printf(TEXT("Invalid property declaration '%s'."), *Statement);
				return false;
			}

			TypeToken.TrimStartAndEndInline();
			NameToken.TrimStartAndEndInline();
			if (NameToken.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Missing property name in declaration '%s'."), *Statement);
				return false;
			}

			Property.Name = NameToken;

			if (TypeToken.StartsWith(TEXT("UE."), ESearchCase::IgnoreCase))
			{
				if (!ParseUEBuiltinPropertyType(TypeToken, Property, OutError))
				{
					return false;
				}

				if (Property.bHasDefaultValue)
				{
					OutError = FString::Printf(
						TEXT("UE builtin property '%s' does not support inline defaults. Put arguments inside UE.%s(...)."),
						*Property.Name,
						*Property.UEBuiltinFunctionName);
					return false;
				}
			}
			else if (TypeToken.Equals(TEXT("float"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("float1"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("half"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("half1"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("int"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("uint"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
			{
				Property.Type = ETextShaderPropertyType::Scalar;
				Property.ComponentCount = 1;
				if (Property.bHasDefaultValue && !ParseScalarLiteral(Right, Property.ScalarDefaultValue))
				{
					OutError = FString::Printf(TEXT("Invalid scalar default value '%s' for property '%s'."), *Right, *Property.Name);
					return false;
				}
			}
			else if (TypeToken.Equals(TEXT("float2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("float3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("float4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("half2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("half3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("half4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("vec2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("vec3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("vec4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("int2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("int3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("int4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("uint2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("uint3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("uint4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("bool2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("bool3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("bool4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("ivec2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("ivec3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("ivec4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("uvec2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("uvec3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("uvec4"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("bvec2"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("bvec3"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("bvec4"), ESearchCase::IgnoreCase))
			{
				Property.Type = ETextShaderPropertyType::Vector;
				if (TypeToken.EndsWith(TEXT("2"), ESearchCase::IgnoreCase))
				{
					Property.ComponentCount = 2;
				}
				else if (TypeToken.EndsWith(TEXT("4"), ESearchCase::IgnoreCase))
				{
					Property.ComponentCount = 4;
				}
				else
				{
					Property.ComponentCount = 3;
				}

				if (Property.bHasDefaultValue && !ParseVectorLiteral(Right, Property.VectorDefaultValue))
				{
					OutError = FString::Printf(TEXT("Invalid vector default value '%s' for property '%s'."), *Right, *Property.Name);
					return false;
				}
			}
			else if (TypeToken.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase))
			{
				Property.Type = ETextShaderPropertyType::Texture2D;
				if (TypeToken.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase))
				{
					Property.TextureType = ETextShaderTextureType::TextureCube;
				}
				else if (TypeToken.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase))
				{
					Property.TextureType = ETextShaderTextureType::Texture2DArray;
				}
				else
				{
					Property.TextureType = ETextShaderTextureType::Texture2D;
				}

				if (Property.bHasDefaultValue)
				{
					if (!ParseTextureAssetReference(Right, Property.TextureDefaultObjectPath, OutError))
					{
						OutError = FString::Printf(
							TEXT("Invalid texture default value '%s' for property '%s'. %s"),
							*Right,
							*Property.Name,
							*OutError);
						return false;
					}
				}
			}
			else
			{
				OutError = FString::Printf(TEXT("Unsupported property type '%s'."), *TypeToken);
				return false;
			}

			OutProperties.Add(Property);
		}

		return true;
	}

	bool ParseSettingStatements(const FString& BlockContent, TMap<FString, FString>& OutSettings, FString& OutError)
	{
		const TArray<FString> Statements = SplitStatements(RemoveComments(BlockContent));
		for (const FString& Statement : Statements)
		{
			FString Key;
			FString Value;
			if (!Statement.Split(TEXT("="), &Key, &Value, ESearchCase::CaseSensitive))
			{
				OutError = FString::Printf(TEXT("Invalid setting declaration '%s'."), *Statement);
				return false;
			}

			Key = NormalizeSettingKey(Key);
			Value = Unquote(Value);
			if (Key.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Invalid empty setting key in '%s'."), *Statement);
				return false;
			}

			OutSettings.Add(Key, Value);
		}

		return true;
	}

	bool ParseTypedDeclarationStatement(const FString& Statement, FTextShaderVariableDeclaration& OutDeclaration, FString& OutError)
	{
		const FString Trimmed = Statement.TrimStartAndEnd();
		const int32 LastSpaceIndex = Trimmed.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastSpaceIndex == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Invalid typed declaration '%s'."), *Statement);
			return false;
		}

		OutDeclaration.Type = Trimmed.Left(LastSpaceIndex).TrimStartAndEnd();
		OutDeclaration.Name = Trimmed.Mid(LastSpaceIndex + 1).TrimStartAndEnd();

		if (OutDeclaration.Type.IsEmpty() || OutDeclaration.Name.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Invalid typed declaration '%s'."), *Statement);
			return false;
		}

		return true;
	}

	bool ParseOutputStatements(
		const FString& BlockContent,
		TArray<FTextShaderVariableDeclaration>& OutOutputDeclarations,
		TArray<FTextShaderOutputBinding>& OutOutputs,
		FString& OutError)
	{
		const auto ParseOutputTarget = [&OutError](const FString& InTargetText, FTextShaderOutputBinding& OutBinding) -> bool
		{
			OutBinding.TargetText = InTargetText.TrimStartAndEnd();
			if (OutBinding.TargetText.IsEmpty())
			{
				OutError = TEXT("Output binding target cannot be empty.");
				return false;
			}

			FString TargetText = OutBinding.TargetText;
			if (TargetText.StartsWith(TEXT("Base."), ESearchCase::IgnoreCase))
			{
				TargetText.RightChopInline(5, EAllowShrinking::No);
				TargetText.TrimStartAndEndInline();
				OutBinding.TargetKind = FTextShaderOutputBinding::ETargetKind::MaterialProperty;
				OutBinding.MaterialProperty = TargetText;
				if (OutBinding.MaterialProperty.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Output binding target '%s' is empty."), *InTargetText);
					return false;
				}
				return true;
			}

			if (!TargetText.StartsWith(TEXT("Expression"), ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(
					TEXT("Output binding target '%s' must start with Base. for material outputs or Expression(...) for output nodes."),
					*InTargetText);
				return false;
			}

			const int32 OpenParenIndex = TargetText.Find(TEXT("("));
			const int32 CloseParenIndex = TargetText.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (OpenParenIndex == INDEX_NONE || CloseParenIndex == INDEX_NONE || CloseParenIndex <= OpenParenIndex)
			{
				OutError = FString::Printf(TEXT("Invalid output expression target '%s'."), *InTargetText);
				return false;
			}

			const FString ExpressionKeyword = TargetText.Left(OpenParenIndex).TrimStartAndEnd();
			if (!ExpressionKeyword.Equals(TEXT("Expression"), ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(TEXT("Unsupported output target '%s'."), *InTargetText);
				return false;
			}

			const FString ArgumentBlock = TargetText.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1).TrimStartAndEnd();
			const FString Suffix = TargetText.Mid(CloseParenIndex + 1).TrimStartAndEnd();
			if (!Suffix.StartsWith(TEXT("."), ESearchCase::CaseSensitive))
			{
				OutError = FString::Printf(TEXT("Expression output target '%s' must select a pin with .Pin[index]."), *InTargetText);
				return false;
			}

			FString PinSpecifier = Suffix.Mid(1).TrimStartAndEnd();
			if (!PinSpecifier.StartsWith(TEXT("Pin["), ESearchCase::IgnoreCase) || !PinSpecifier.EndsWith(TEXT("]")))
			{
				OutError = FString::Printf(TEXT("Expression output target '%s' must use .Pin[index] syntax."), *InTargetText);
				return false;
			}

			const FString PinIndexText = PinSpecifier.Mid(4, PinSpecifier.Len() - 5).TrimStartAndEnd();
			if (!ParseIntegerLiteral(PinIndexText, OutBinding.ExpressionPinIndex) || OutBinding.ExpressionPinIndex < 0)
			{
				OutError = FString::Printf(TEXT("Expression output target '%s' has an invalid pin index."), *InTargetText);
				return false;
			}

			OutBinding.TargetKind = FTextShaderOutputBinding::ETargetKind::ExpressionInput;
			OutBinding.ExpressionArguments.Reset();
			for (const FString& ArgumentStatement : SplitTopLevelDelimited(ArgumentBlock, TCHAR(',')))
			{
				FString ArgumentName;
				FString ArgumentValue;
				if (!SplitTopLevelAssignment(ArgumentStatement, ArgumentName, ArgumentValue))
				{
					OutError = FString::Printf(TEXT("Expression output target argument '%s' must use Key=Value syntax."), *ArgumentStatement);
					return false;
				}

				ArgumentName = NormalizeSettingKey(ArgumentName);
				ArgumentValue = Unquote(ArgumentValue).TrimStartAndEnd();
				if (ArgumentName.IsEmpty() || ArgumentValue.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Invalid expression output target argument '%s'."), *ArgumentStatement);
					return false;
				}

				if (OutBinding.ExpressionArguments.Contains(ArgumentName))
				{
					OutError = FString::Printf(TEXT("Expression output target argument '%s' is declared more than once."), *ArgumentName);
					return false;
				}

				OutBinding.ExpressionArguments.Add(ArgumentName, ArgumentValue);
			}

			if (const FString* ClassName = OutBinding.ExpressionArguments.Find(NormalizeSettingKey(TEXT("Class"))))
			{
				OutBinding.ExpressionClass = *ClassName;
			}

			if (OutBinding.ExpressionClass.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Expression output target '%s' must specify Class=\"...\"."), *InTargetText);
				return false;
			}

			return true;
		};

		const TArray<FString> Statements = SplitStatements(RemoveComments(BlockContent));
		for (const FString& Statement : Statements)
		{
			const FString Trimmed = Statement.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}

			FTextShaderOutputBinding Binding;
			FString LeftSide;
			FString RightSide;
			if (SplitTopLevelAssignment(Trimmed, LeftSide, RightSide))
			{
				Binding.SourceText = RightSide.TrimStartAndEnd();
				if (Binding.SourceText.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Invalid output binding '%s'."), *Statement);
					return false;
				}
				if (!ParseOutputTarget(LeftSide, Binding))
				{
					return false;
				}

				OutOutputs.Add(Binding);
			}
			else
			{
				FTextShaderVariableDeclaration Declaration;
				if (!ParseTypedDeclarationStatement(Trimmed, Declaration, OutError))
				{
					return false;
				}

				OutOutputDeclarations.Add(Declaration);
			}
		}

		return true;
	}

	bool ParseTypedParameterStatements(const FString& BlockContent, TArray<FTextShaderFunctionParameter>& OutParameters, FString& OutError)
	{
		const TArray<FString> Statements = SplitStatements(RemoveComments(BlockContent));
		for (const FString& Statement : Statements)
		{
			const FString Trimmed = Statement.TrimStartAndEnd();
			const int32 LastSpaceIndex = Trimmed.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (LastSpaceIndex == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Invalid typed declaration '%s'."), *Statement);
				return false;
			}

			FTextShaderVariableDeclaration Declaration;
			if (!ParseTypedDeclarationStatement(Trimmed, Declaration, OutError))
			{
				return false;
			}

			FTextShaderFunctionParameter Parameter;
			Parameter.Type = Declaration.Type;
			Parameter.Name = Declaration.Name;
			OutParameters.Add(Parameter);
		}

		return true;
	}

	bool ParseShaderBody(const FString& BodyContent, FTextShaderDefinition& OutDefinition, FString& OutError)
	{
		FScanner Scanner(BodyContent);
		while (true)
		{
			Scanner.SkipIgnored();
			if (Scanner.IsAtEnd())
			{
				return true;
			}

			FString SectionName;
			if (!Scanner.ParseIdentifier(SectionName, OutError))
			{
				return false;
			}

			if (!Scanner.Expect(TCHAR('='), OutError))
			{
				return false;
			}

			FString SectionBody;
			if (!Scanner.ExtractBalancedBlock(SectionBody, OutError))
			{
				return false;
			}

			if (SectionName.Equals(TEXT("Properties"), ESearchCase::IgnoreCase))
			{
				if (!ParsePropertyStatements(SectionBody, OutDefinition.Properties, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Settings"), ESearchCase::IgnoreCase))
			{
				if (!ParseSettingStatements(SectionBody, OutDefinition.Settings, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Outputs"), ESearchCase::IgnoreCase))
			{
				if (!ParseOutputStatements(SectionBody, OutDefinition.OutputDeclarations, OutDefinition.Outputs, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
			{
				OutDefinition.Code = SectionBody.TrimStartAndEnd();
			}
			else
			{
				OutError = FString::Printf(TEXT("Unknown shader section '%s'."), *SectionName);
				return false;
			}

			Scanner.TryConsume(TCHAR(';'));
		}
	}

	bool ParseFunctionBody(const FString& BodyContent, FTextShaderFunctionDefinition& OutFunction, FString& OutError)
	{
		FScanner Scanner(BodyContent);
		while (true)
		{
			Scanner.SkipIgnored();
			if (Scanner.IsAtEnd())
			{
				return true;
			}

			FString SectionName;
			if (!Scanner.ParseIdentifier(SectionName, OutError))
			{
				return false;
			}

			if (!Scanner.Expect(TCHAR('='), OutError))
			{
				return false;
			}

			FString SectionBody;
			if (!Scanner.ExtractBalancedBlock(SectionBody, OutError))
			{
				return false;
			}

			if (SectionName.Equals(TEXT("Inputs"), ESearchCase::IgnoreCase)
				|| SectionName.Equals(TEXT("Properties"), ESearchCase::IgnoreCase))
			{
				if (!ParseTypedParameterStatements(SectionBody, OutFunction.Inputs, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Results"), ESearchCase::IgnoreCase) || SectionName.Equals(TEXT("Outputs"), ESearchCase::IgnoreCase))
			{
				if (!ParseTypedParameterStatements(SectionBody, OutFunction.Results, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
			{
				OutFunction.HLSL = SectionBody.TrimStartAndEnd();
			}
			else
			{
				OutError = FString::Printf(TEXT("Unknown shader function section '%s'."), *SectionName);
				return false;
			}

			Scanner.TryConsume(TCHAR(';'));
		}
	}

	bool ParseMaterialFunctionBody(const FString& BodyContent, FTextShaderMaterialFunctionDefinition& OutFunction, FString& OutError)
	{
		FScanner Scanner(BodyContent);
		while (true)
		{
			Scanner.SkipIgnored();
			if (Scanner.IsAtEnd())
			{
				return true;
			}

			FString SectionName;
			if (!Scanner.ParseIdentifier(SectionName, OutError))
			{
				return false;
			}

			if (!Scanner.Expect(TCHAR('='), OutError))
			{
				return false;
			}

			FString SectionBody;
			if (!Scanner.ExtractBalancedBlock(SectionBody, OutError))
			{
				return false;
			}

			if (SectionName.Equals(TEXT("Inputs"), ESearchCase::IgnoreCase)
				|| SectionName.Equals(TEXT("Properties"), ESearchCase::IgnoreCase))
			{
				if (!ParseTypedParameterStatements(SectionBody, OutFunction.Inputs, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Outputs"), ESearchCase::IgnoreCase)
				|| SectionName.Equals(TEXT("Results"), ESearchCase::IgnoreCase))
			{
				if (!ParseTypedParameterStatements(SectionBody, OutFunction.Outputs, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Settings"), ESearchCase::IgnoreCase))
			{
				if (!ParseSettingStatements(SectionBody, OutFunction.Settings, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
			{
				OutFunction.Code = SectionBody.TrimStartAndEnd();
			}
			else
			{
				OutError = FString::Printf(TEXT("Unknown ShaderFunction section '%s'."), *SectionName);
				return false;
			}

			Scanner.TryConsume(TCHAR(';'));
		}
	}
}
