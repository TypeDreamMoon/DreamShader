#include "DreamShaderParserInternal.h"
#include "Internationalization/Text.h"

#include "DreamShaderModule.h"

#define LOCTEXT_NAMESPACE "DreamShader.Parser.Sections"

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
			|| Value == TEXT("texture2darray")
			|| Value == TEXT("texture3d")
			|| Value == TEXT("volumetexture"))
		{
			OutType = ETextShaderPropertyType::Texture2D;
			OutComponentCount = 0;
			return true;
		}

		return false;
	}

	static bool IsParameterNodeType(const FString& InTypeToken, const TCHAR* Candidate)
	{
		return InTypeToken.Equals(Candidate, ESearchCase::IgnoreCase);
	}

	static bool IsIdentifierToken(const FString& InText)
	{
		const FString Trimmed = InText.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}

		if (!(FChar::IsAlpha(Trimmed[0]) || Trimmed[0] == TCHAR('_')))
		{
			return false;
		}

		for (int32 Index = 1; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR Char = Trimmed[Index];
			if (!(FChar::IsAlnum(Char) || Char == TCHAR('_')))
			{
				return false;
			}
		}

		return true;
	}

	static bool IsStaticSwitchParameterType(const FString& InTypeToken)
	{
		return IsParameterNodeType(InTypeToken, TEXT("StaticSwitchParameter"));
	}

	static bool IsStaticBoolParameterType(const FString& InTypeToken)
	{
		return IsParameterNodeType(InTypeToken, TEXT("StaticBoolParameter"));
	}

	static bool IsScalarParameterType(const FString& InTypeToken)
	{
		return IsParameterNodeType(InTypeToken, TEXT("ScalarParameter"))
			|| IsStaticBoolParameterType(InTypeToken)
			|| IsStaticSwitchParameterType(InTypeToken);
	}

	static bool IsVectorParameterType(const FString& InTypeToken)
	{
		return IsParameterNodeType(InTypeToken, TEXT("VectorParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("DoubleVectorParameter"));
	}

	static bool IsTextureObjectParameterType(const FString& InTypeToken)
	{
		return IsParameterNodeType(InTypeToken, TEXT("TextureObjectParameter"));
	}

	static bool IsTextureSampleParameterType(const FString& InTypeToken)
	{
		return IsParameterNodeType(InTypeToken, TEXT("TextureSampleParameter2D"))
			|| IsParameterNodeType(InTypeToken, TEXT("TextureSampleParameter2DArray"))
			|| IsParameterNodeType(InTypeToken, TEXT("TextureSampleParameterCube"))
			|| IsParameterNodeType(InTypeToken, TEXT("TextureSampleParameterCubeArray"))
			|| IsParameterNodeType(InTypeToken, TEXT("TextureSampleParameterVolume"))
			|| IsParameterNodeType(InTypeToken, TEXT("TextureSampleParameterSubUV"))
			|| IsParameterNodeType(InTypeToken, TEXT("RuntimeVirtualTextureSampleParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("SparseVolumeTextureSampleParameter"));
	}

	static bool IsKnownParameterNodeType(const FString& InTypeToken)
	{
		return IsScalarParameterType(InTypeToken)
			|| IsVectorParameterType(InTypeToken)
			|| IsTextureObjectParameterType(InTypeToken)
			|| IsTextureSampleParameterType(InTypeToken)
			|| IsParameterNodeType(InTypeToken, TEXT("ChannelMaskParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("StaticComponentMaskParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("TextureCollectionParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("CurveAtlasRowParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("DynamicParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("FontSampleParameter"))
			|| IsParameterNodeType(InTypeToken, TEXT("SpriteTextureSampler"))
			|| IsParameterNodeType(InTypeToken, TEXT("SparseVolumeTextureObjectParameter"));
	}

	static bool ParseTrailingMetadata(FString& InOutStatement, FTextShaderMetadata& OutMetadata, FDreamShaderTextError& OutError)
	{
		FString Statement = InOutStatement.TrimStartAndEnd();
		if (!Statement.EndsWith(TEXT("]")))
		{
			InOutStatement = Statement;
			return true;
		}

		int32 MetadataStart = INDEX_NONE;
		int32 ParenthesisDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;
		for (int32 Index = 0; Index < Statement.Len(); ++Index)
		{
			const TCHAR Char = Statement[Index];
			if (bInString)
			{
				if (Char == TCHAR('\\') && Statement.IsValidIndex(Index + 1))
				{
					++Index;
				}
				else if (Char == TCHAR('"'))
				{
					bInString = false;
				}
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				continue;
			}
			if (Char == TCHAR('('))
			{
				++ParenthesisDepth;
				continue;
			}
			if (Char == TCHAR(')'))
			{
				ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
				continue;
			}
			if (Char == TCHAR('['))
			{
				if (ParenthesisDepth == 0 && BracketDepth == 0)
				{
					MetadataStart = Index;
				}
				++BracketDepth;
				continue;
			}
			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				continue;
			}
		}

		if (MetadataStart == INDEX_NONE)
		{
			InOutStatement = Statement;
			return true;
		}

		const FString MetadataBlock = Statement.Mid(MetadataStart + 1, Statement.Len() - MetadataStart - 2).TrimStartAndEnd();
		FString Prefix = Statement.Left(MetadataStart).TrimStartAndEnd();
		if (Prefix.IsEmpty())
		{
			FailWith(OutError, TEXT("DSH7001"), LOCTEXT("MetadataMustFollowADeclaration", "Metadata must follow a declaration."));
			return false;
		}

		auto SplitMetadataEntries = [](const FString& Input)
		{
			TArray<FString> Entries;
			FString Current;
			int32 ParenthesisDepth = 0;
			int32 BracketDepth = 0;
			bool bInString = false;

			for (int32 Index = 0; Index < Input.Len(); ++Index)
			{
				const TCHAR Char = Input[Index];

				if (bInString)
				{
					Current.AppendChar(Char);
					if (Char == TCHAR('\\') && Input.IsValidIndex(Index + 1))
					{
						Current.AppendChar(Input[++Index]);
					}
					else if (Char == TCHAR('"'))
					{
						bInString = false;
					}
					continue;
				}

				if (Char == TCHAR('"'))
				{
					bInString = true;
					Current.AppendChar(Char);
					continue;
				}

				if (Char == TCHAR('('))
				{
					++ParenthesisDepth;
					Current.AppendChar(Char);
					continue;
				}

				if (Char == TCHAR(')'))
				{
					ParenthesisDepth = FMath::Max(0, ParenthesisDepth - 1);
					Current.AppendChar(Char);
					continue;
				}

				if (Char == TCHAR('['))
				{
					++BracketDepth;
					Current.AppendChar(Char);
					continue;
				}

				if (Char == TCHAR(']'))
				{
					BracketDepth = FMath::Max(0, BracketDepth - 1);
					Current.AppendChar(Char);
					continue;
				}

				if ((Char == TCHAR(';') || Char == TCHAR(',')) && ParenthesisDepth == 0 && BracketDepth == 0)
				{
					Current.TrimStartAndEndInline();
					if (!Current.IsEmpty())
					{
						Entries.Add(Current);
					}
					Current.Reset();
					continue;
				}

				Current.AppendChar(Char);
			}

			Current.TrimStartAndEndInline();
			if (!Current.IsEmpty())
			{
				Entries.Add(Current);
			}

			return Entries;
		};

		for (const FString& Entry : SplitMetadataEntries(MetadataBlock))
		{
			// Slider(min, max) shorthand -> SliderMin / SliderMax reflected properties. It has no '='
			// so it must be handled before the Key=Value split below.
			const FString TrimmedEntry = Entry.TrimStartAndEnd();
			if (TrimmedEntry.StartsWith(TEXT("Slider("), ESearchCase::IgnoreCase) && TrimmedEntry.EndsWith(TEXT(")")))
			{
				const FString Inner = TrimmedEntry.Mid(7, TrimmedEntry.Len() - 8);
				const TArray<FString> Bounds = SplitTopLevelDelimited(Inner, TCHAR(','));
				double SliderMinValue = 0.0;
				double SliderMaxValue = 0.0;
				if (Bounds.Num() != 2
					|| !ParseScalarLiteral(Bounds[0].TrimStartAndEnd(), SliderMinValue)
					|| !ParseScalarLiteral(Bounds[1].TrimStartAndEnd(), SliderMaxValue))
				{
					FailWith(OutError, TEXT("DSH7002"), FText::Format(LOCTEXT("MetadataSliderMinMaxRequiresExactly", "Metadata 'Slider(min, max)' requires exactly two numeric bounds: '{0}'."),
					FText::FromString(TrimmedEntry)));
					return false;
				}

				const FString SliderMinKey = NormalizeSettingKey(TEXT("SliderMin"));
				const FString SliderMaxKey = NormalizeSettingKey(TEXT("SliderMax"));
				if (OutMetadata.ReflectedProperties.Contains(SliderMinKey) || OutMetadata.ReflectedProperties.Contains(SliderMaxKey))
				{
					FailWith(OutError, TEXT("DSH7003"), FText::Format(LOCTEXT("MetadataSliderMinSliderMaxIsDeclaredMore", "Metadata SliderMin/SliderMax is declared more than once (entry '{0}')."),
					FText::FromString(TrimmedEntry)));
					return false;
				}
				OutMetadata.ReflectedProperties.Add(SliderMinKey, FString::SanitizeFloat(SliderMinValue));
				OutMetadata.ReflectedProperties.Add(SliderMaxKey, FString::SanitizeFloat(SliderMaxValue));
				continue;
			}

			FString Key;
			FString Value;
			if (!SplitTopLevelAssignment(Entry, Key, Value))
			{
				FailWith(OutError, TEXT("DSH7004"), FText::Format(LOCTEXT("MetadataEntrySMustUseKey", "Metadata entry '{0}' must use Key=Value syntax."),
					FText::FromString(Entry)));
				return false;
			}

			const FString OriginalKey = Key.TrimStartAndEnd();
			Key = NormalizeSettingKey(Key);
			Value = Unquote(Value).TrimStartAndEnd();
			if (Key.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH7005"), FText::Format(LOCTEXT("InvalidMetadataEntryS", "Invalid metadata entry '{0}'."),
					FText::FromString(Entry)));
				return false;
			}

			if (OutMetadata.ReflectedProperties.Contains(Key))
			{
				FailWith(OutError, TEXT("DSH7006"), FText::Format(LOCTEXT("MetadataKeySIsDeclaredMore", "Metadata key '{0}' is declared more than once."),
					FText::FromString(OriginalKey)));
				return false;
			}
			OutMetadata.ReflectedProperties.Add(Key, Value);

			if (Key == NormalizeSettingKey(TEXT("Group")) || Key == NormalizeSettingKey(TEXT("Category")))
			{
				OutMetadata.Group = Value;
			}
			else if (Key == NormalizeSettingKey(TEXT("Description")) || Key == NormalizeSettingKey(TEXT("Desc")) || Key == NormalizeSettingKey(TEXT("Tooltip")))
			{
				OutMetadata.Description = Value;
			}
			else if (Key == NormalizeSettingKey(TEXT("SortPriority")) || Key == NormalizeSettingKey(TEXT("Sort")))
			{
				int32 SortPriority = 32;
				if (!ParseIntegerLiteral(Value, SortPriority))
				{
					FailWith(OutError, TEXT("DSH7007"), FText::Format(LOCTEXT("MetadataSortPriorityValueSIsNot", "Metadata SortPriority value '{0}' is not an integer."),
					FText::FromString(Value)));
					return false;
				}
				OutMetadata.bHasSortPriority = true;
				OutMetadata.SortPriority = SortPriority;
			}
		}

		InOutStatement = Prefix;
		return true;
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

		if (Matches(TEXT("WorldPosition"))
			|| Matches(TEXT("CameraVectorWS"))
			|| Matches(TEXT("ObjectPositionWS"))
			|| Matches(TEXT("VertexNormalWS"))
			|| Matches(TEXT("VertexTangentWS")))
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
		FDreamShaderTextError& OutError)
	{
		FString CallSpec = InTypeToken.TrimStartAndEnd();
		if (!CallSpec.StartsWith(TEXT("UE."), ESearchCase::IgnoreCase))
		{
			return false;
		}

		CallSpec.RightChopInline(3, DREAMSHADER_ALLOW_SHRINKING_NO);
		CallSpec.TrimStartAndEndInline();
		if (CallSpec.IsEmpty())
		{
			FailWith(OutError, TEXT("DSH7010"), LOCTEXT("UEBuiltinPropertyDeclarationsMustSpecify", "UE builtin property declarations must specify a function name, for example UE.TexCoord UV."));
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
				FailWith(OutError, TEXT("DSH7011"), FText::Format(LOCTEXT("InvalidUEBuiltinDeclarationS", "Invalid UE builtin declaration '{0}'."),
					FText::FromString(InTypeToken)));
				return false;
			}

			if (!CallSpec.Mid(CloseParenIndex + 1).TrimStartAndEnd().IsEmpty())
			{
				FailWith(OutError, TEXT("DSH7012"), FText::Format(LOCTEXT("UnexpectedCharactersAfterUEBuiltinArgument", "Unexpected characters after UE builtin argument list in '{0}'."),
					FText::FromString(InTypeToken)));
				return false;
			}

			FunctionName = CallSpec.Left(OpenParenIndex).TrimStartAndEnd();
			ArgumentBlock = CallSpec.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1).TrimStartAndEnd();
		}

		if (FunctionName.IsEmpty())
		{
			FailWith(OutError, TEXT("DSH7011"), FText::Format(LOCTEXT("InvalidUEBuiltinDeclarationS2", "Invalid UE builtin declaration '{0}'."),
					FText::FromString(InTypeToken)));
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
				FailWith(OutError, TEXT("DSH7013"), FText::Format(LOCTEXT("UEBuiltinArgumentSMustUse", "UE builtin argument '{0}' must use named syntax like Key=Value in '{1}'."),
					FText::FromString(ArgumentStatement),
					FText::FromString(InTypeToken)));
				return false;
			}

			ArgumentName = NormalizeSettingKey(ArgumentName);
			ArgumentValue = Unquote(ArgumentValue).TrimStartAndEnd();
			if (ArgumentName.IsEmpty() || ArgumentValue.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH7014"), FText::Format(LOCTEXT("InvalidUEBuiltinArgumentSIn", "Invalid UE builtin argument '{0}' in '{1}'."),
					FText::FromString(ArgumentStatement),
					FText::FromString(InTypeToken)));
				return false;
			}

			if (OutProperty.UEBuiltinArguments.Contains(ArgumentName))
			{
				FailWith(OutError, TEXT("DSH7015"), FText::Format(LOCTEXT("UEBuiltinArgumentSIsDeclared", "UE builtin argument '{0}' is declared more than once in '{1}'."),
					FText::FromString(ArgumentName),
					FText::FromString(InTypeToken)));
				return false;
			}

			OutProperty.UEBuiltinArguments.Add(ArgumentName, ArgumentValue);
		}

		ETextShaderPropertyType BuiltinType = ETextShaderPropertyType::Scalar;
		int32 BuiltinComponentCount = 1;
		if (TryResolveExplicitOutputSignature(OutProperty.UEBuiltinArguments, BuiltinType, BuiltinComponentCount)
			|| TryResolveUEBuiltinOutputSignature(FunctionName, BuiltinType, BuiltinComponentCount))
		{
			OutProperty.Type = BuiltinType;
			OutProperty.ComponentCount = BuiltinComponentCount;
			return true;
		}

		if (FunctionName.Equals(TEXT("CollectionParam"), ESearchCase::IgnoreCase)
			|| FunctionName.Equals(TEXT("CollectionParameter"), ESearchCase::IgnoreCase))
		{
			OutProperty.Type = ETextShaderPropertyType::Scalar;
			OutProperty.ComponentCount = 1;
			return true;
		}

		FailWith(OutError, TEXT("DSH7016"), FText::Format(LOCTEXT("UnsupportedUEBuiltinFunctionSUse", "Unsupported UE builtin function '{0}'. Use OutputType=\\\"float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture\\\" for generic MaterialExpression calls."),
					FText::FromString(FunctionName)));
		return false;
	}

	// Parse a single property declaration statement (no trailing ';') into Property. Hoisted out of
	// ParsePropertyStatements so the flat path and the Group(...) scope walker share identical parsing.
	bool ParseSinglePropertyStatement(const FString& Statement, FTextShaderPropertyDefinition& Property, FDreamShaderTextError& OutError)
	{
		{
			FString Trimmed = Statement.TrimStartAndEnd();

			if (!ParseTrailingMetadata(Trimmed, Property.Metadata, OutError))
			{
				return false;
			}

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
				FailWith(OutError, TEXT("DSH7020"), FText::Format(LOCTEXT("InvalidPropertyDeclarationS", "Invalid property declaration '{0}'."),
					FText::FromString(Statement)));
				return false;
			}

			TypeToken.TrimStartAndEndInline();
			NameToken.TrimStartAndEndInline();
			if (NameToken.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH7021"), FText::Format(LOCTEXT("MissingPropertyNameInDeclarationS", "Missing property name in declaration '{0}'."),
					FText::FromString(Statement)));
				return false;
			}

			if (TypeToken.Len() >= 5
				&& TypeToken.Left(5).Equals(TEXT("const"), ESearchCase::IgnoreCase)
				&& (TypeToken.Len() == 5 || FChar::IsWhitespace(TypeToken[5])))
			{
				Property.bConst = true;
				TypeToken.RightChopInline(5, DREAMSHADER_ALLOW_SHRINKING_NO);
				TypeToken.TrimStartAndEndInline();
				if (TypeToken.IsEmpty())
				{
					FailWith(OutError, TEXT("DSH7022"), FText::Format(LOCTEXT("MissingPropertyTypeAfterConstIn", "Missing property type after const in declaration '{0}'."),
					FText::FromString(Statement)));
					return false;
				}
			}

			Property.Name = NameToken;

			if (IsKnownParameterNodeType(TypeToken))
			{
				Property.ParameterNodeType = TypeToken;

				if (IsScalarParameterType(TypeToken))
				{
					Property.Type = ETextShaderPropertyType::Scalar;
					Property.ComponentCount = 1;
					if (Property.bHasDefaultValue)
					{
						if (IsStaticBoolParameterType(TypeToken) || IsStaticSwitchParameterType(TypeToken))
						{
							bool bDefaultValue = false;
							if (!ParseBooleanLiteral(Right, bDefaultValue))
							{
								FailWith(OutError, TEXT("DSH7023"), FText::Format(LOCTEXT("InvalidBooleanDefaultValueSFor", "Invalid boolean default value '{0}' for property '{1}'."),
					FText::FromString(Right),
					FText::FromString(Property.Name)));
								return false;
							}
							Property.ScalarDefaultValue = bDefaultValue ? 1.0 : 0.0;
						}
						else if (!ParseScalarLiteral(Right, Property.ScalarDefaultValue))
						{
							FailWith(OutError, TEXT("DSH7024"), FText::Format(LOCTEXT("InvalidScalarDefaultValueSFor", "Invalid scalar default value '{0}' for property '{1}'."),
					FText::FromString(Right),
					FText::FromString(Property.Name)));
							return false;
						}
					}
				}
				else if (IsVectorParameterType(TypeToken)
					|| IsParameterNodeType(TypeToken, TEXT("ChannelMaskParameter"))
					|| IsParameterNodeType(TypeToken, TEXT("StaticComponentMaskParameter"))
					|| IsParameterNodeType(TypeToken, TEXT("DynamicParameter"))
					|| IsParameterNodeType(TypeToken, TEXT("FontSampleParameter"))
					|| IsParameterNodeType(TypeToken, TEXT("CurveAtlasRowParameter"))
					|| IsParameterNodeType(TypeToken, TEXT("SpriteTextureSampler")))
				{
					Property.Type = ETextShaderPropertyType::Vector;
					Property.ComponentCount = IsParameterNodeType(TypeToken, TEXT("ChannelMaskParameter"))
						? 1
						: (IsParameterNodeType(TypeToken, TEXT("CurveAtlasRowParameter")) ? 3 : 4);
					if (Property.bHasDefaultValue && !ParseVectorLiteral(Right, Property.VectorDefaultValue))
					{
						FailWith(OutError, TEXT("DSH7025"), FText::Format(LOCTEXT("InvalidVectorDefaultValueSFor", "Invalid vector default value '{0}' for property '{1}'."),
					FText::FromString(Right),
					FText::FromString(Property.Name)));
						return false;
					}
				}
				else if (IsTextureObjectParameterType(TypeToken)
					|| IsParameterNodeType(TypeToken, TEXT("TextureCollectionParameter"))
					|| IsParameterNodeType(TypeToken, TEXT("SparseVolumeTextureObjectParameter")))
				{
					Property.Type = ETextShaderPropertyType::Texture2D;
					Property.ComponentCount = 0;
					if (Property.bHasDefaultValue && !ParseTextureAssetReference(Right, Property.TextureDefaultObjectPath, OutError))
					{
						FailWith(OutError, TEXT("DSH7026"), FText::Format(LOCTEXT("InvalidTextureDefaultValueSFor", "Invalid texture default value '{0}' for property '{1}'. {2}"),
					FText::FromString(Right),
					FText::FromString(Property.Name),
					OutError.Message));
						return false;
					}
				}
				else if (IsTextureSampleParameterType(TypeToken))
				{
					Property.Type = ETextShaderPropertyType::Vector;
					Property.ComponentCount = 4;
					Property.bHasExplicitTextureType = true;
					if (TypeToken.Contains(TEXT("Cube"), ESearchCase::IgnoreCase))
					{
						Property.TextureType = ETextShaderTextureType::TextureCube;
					}
					else if (TypeToken.Contains(TEXT("Array"), ESearchCase::IgnoreCase))
					{
						Property.TextureType = ETextShaderTextureType::Texture2DArray;
					}
					else if (TypeToken.Contains(TEXT("Volume"), ESearchCase::IgnoreCase))
					{
						Property.TextureType = ETextShaderTextureType::VolumeTexture;
					}
					if (Property.bHasDefaultValue && !ParseTextureAssetReference(Right, Property.TextureDefaultObjectPath, OutError))
					{
						FailWith(OutError, TEXT("DSH7027"), FText::Format(LOCTEXT("InvalidTextureSampleDefaultValueS", "Invalid texture sample default value '{0}' for property '{1}'. {2}"),
					FText::FromString(Right),
					FText::FromString(Property.Name),
					OutError.Message));
						return false;
					}
				}
				else
				{
					FailWith(OutError, TEXT("DSH7028"), FText::Format(LOCTEXT("ParameterNodeTypeSIsRecognized", "Parameter node type '{0}' is recognized but not supported as a plain Properties declaration yet. Use UE.{1}(OutputType=\\\"float4\\\", ...) for reflected node creation."),
					FText::FromString(TypeToken),
					FText::FromString(TypeToken)));
					return false;
				}
			}
			else if (TypeToken.StartsWith(TEXT("UE."), ESearchCase::IgnoreCase))
			{
				if (!ParseUEBuiltinPropertyType(TypeToken, Property, OutError))
				{
					return false;
				}

				if (Property.bHasDefaultValue)
				{
					FailWith(OutError, TEXT("DSH7029"), FText::Format(LOCTEXT("UEBuiltinPropertySDoesNot", "UE builtin property '{0}' does not support inline defaults. Put arguments inside UE.{1}(...)."),
					FText::FromString(Property.Name),
					FText::FromString(Property.UEBuiltinFunctionName)));
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
					FailWith(OutError, TEXT("DSH7024"), FText::Format(LOCTEXT("InvalidScalarDefaultValueSFor2", "Invalid scalar default value '{0}' for property '{1}'."),
					FText::FromString(Right),
					FText::FromString(Property.Name)));
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
					FailWith(OutError, TEXT("DSH7025"), FText::Format(LOCTEXT("InvalidVectorDefaultValueSFor2", "Invalid vector default value '{0}' for property '{1}'."),
					FText::FromString(Right),
					FText::FromString(Property.Name)));
					return false;
				}
			}
			else if (TypeToken.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("Texture3D"), ESearchCase::IgnoreCase)
				|| TypeToken.Equals(TEXT("VolumeTexture"), ESearchCase::IgnoreCase))
			{
				Property.Type = ETextShaderPropertyType::Texture2D;
				Property.bHasExplicitTextureType = true;
				if (TypeToken.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase))
				{
					Property.TextureType = ETextShaderTextureType::TextureCube;
				}
				else if (TypeToken.Equals(TEXT("Texture2DArray"), ESearchCase::IgnoreCase))
				{
					Property.TextureType = ETextShaderTextureType::Texture2DArray;
				}
				else if (TypeToken.Equals(TEXT("Texture3D"), ESearchCase::IgnoreCase)
					|| TypeToken.Equals(TEXT("VolumeTexture"), ESearchCase::IgnoreCase))
				{
					Property.TextureType = ETextShaderTextureType::VolumeTexture;
				}
				else
				{
					Property.TextureType = ETextShaderTextureType::Texture2D;
				}

				if (Property.bHasDefaultValue)
				{
					if (!ParseTextureAssetReference(Right, Property.TextureDefaultObjectPath, OutError))
					{
						FailWith(OutError, TEXT("DSH7026"), FText::Format(LOCTEXT("InvalidTextureDefaultValueSFor2", "Invalid texture default value '{0}' for property '{1}'. {2}"),
					FText::FromString(Right),
					FText::FromString(Property.Name),
					OutError.Message));
						return false;
					}
				}
			}
			else
			{
				FailWith(OutError, TEXT("DSH7030"), FText::Format(LOCTEXT("UnsupportedPropertyTypeS", "Unsupported property type '{0}'."),
					FText::FromString(TypeToken)));
				return false;
			}

		}

		return true;
	}

	bool TryMatchGroupHead(const FString& Head, FString& OutGroupName)
	{
		const FString Trimmed = Head.TrimStartAndEnd();
		if (Trimmed.Len() < 5 || !Trimmed.Left(5).Equals(TEXT("Group"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		const FString Rest = Trimmed.RightChop(5).TrimStartAndEnd();
		if (!Rest.StartsWith(TEXT("(")) || !Rest.EndsWith(TEXT(")")))
		{
			return false;
		}
		const FString Inner = Rest.Mid(1, Rest.Len() - 2).TrimStartAndEnd();
		if (!Inner.StartsWith(TEXT("\"")))
		{
			return false;
		}
		OutGroupName = Unquote(Inner).TrimStartAndEnd();
		return true;
	}

	void StampGroupedProperty(FTextShaderPropertyDefinition& Property, const FString& InheritedGroup, int32& InOutNextAutoSort)
	{
		if (InheritedGroup.IsEmpty())
		{
			// Top-level (ungrouped) statements keep today's behavior: no inherited group, no auto-sort.
			return;
		}

		if (!Property.Metadata.ReflectedProperties.Contains(NormalizeSettingKey(TEXT("Group")))
			&& !Property.Metadata.ReflectedProperties.Contains(NormalizeSettingKey(TEXT("Category"))))
		{
			Property.Metadata.Group = InheritedGroup;
		}

		// Auto-number group members by declaration order; an explicit SortPriority/Sort wins and does
		// not consume an auto slot.
		if (!Property.Metadata.bHasSortPriority)
		{
			Property.Metadata.bHasSortPriority = true;
			Property.Metadata.SortPriority = InOutNextAutoSort;
			InOutNextAutoSort += 10;
		}
	}

	// Recursively parse a Properties body that may contain Group("Name") { ... } scope blocks. Content
	// must already be comment-stripped. Brace-aware: tracks () [] "" depth like SplitStatements, plus
	// {} so Group blocks (whose bodies contain ';'-separated statements) are not mis-split.
	bool ParsePropertyBlock(
		const FString& Content,
		const FString& InheritedGroup,
		int32& InOutNextAutoSort,
		TArray<FTextShaderPropertyDefinition>& OutProperties,
		FDreamShaderTextError& OutError)
	{
		auto FlushStatement = [&](FString& Buffer) -> bool
		{
			const FString Statement = Buffer.TrimStartAndEnd();
			Buffer.Reset();
			if (Statement.IsEmpty())
			{
				return true;
			}
			FTextShaderPropertyDefinition Property;
			if (!ParseSinglePropertyStatement(Statement, Property, OutError))
			{
				return false;
			}
			StampGroupedProperty(Property, InheritedGroup, InOutNextAutoSort);
			OutProperties.Add(Property);
			return true;
		};

		int32 ParenDepth = 0;
		int32 BracketDepth = 0;
		bool bInString = false;
		FString Buffer;

		int32 Index = 0;
		while (Index < Content.Len())
		{
			const TCHAR Char = Content[Index];

			if (bInString)
			{
				Buffer.AppendChar(Char);
				if (Char == TCHAR('\\') && Index + 1 < Content.Len())
				{
					Buffer.AppendChar(Content[Index + 1]);
					Index += 2;
					continue;
				}
				if (Char == TCHAR('"'))
				{
					bInString = false;
				}
				++Index;
				continue;
			}

			if (Char == TCHAR('"'))
			{
				bInString = true;
				Buffer.AppendChar(Char);
				++Index;
				continue;
			}
			if (Char == TCHAR('('))
			{
				++ParenDepth;
				Buffer.AppendChar(Char);
				++Index;
				continue;
			}
			if (Char == TCHAR(')'))
			{
				ParenDepth = FMath::Max(0, ParenDepth - 1);
				Buffer.AppendChar(Char);
				++Index;
				continue;
			}
			if (Char == TCHAR('['))
			{
				++BracketDepth;
				Buffer.AppendChar(Char);
				++Index;
				continue;
			}
			if (Char == TCHAR(']'))
			{
				BracketDepth = FMath::Max(0, BracketDepth - 1);
				Buffer.AppendChar(Char);
				++Index;
				continue;
			}

			if (ParenDepth == 0 && BracketDepth == 0)
			{
				if (Char == TCHAR(';'))
				{
					if (!FlushStatement(Buffer))
					{
						return false;
					}
					++Index;
					continue;
				}

				if (Char == TCHAR('{'))
				{
					FString GroupName;
					if (!TryMatchGroupHead(Buffer, GroupName))
					{
					FailWith(OutError, TEXT("DSH3130"), FText::Format(
						LOCTEXT("UnexpectedInPropertiesNearSOnly", "Unexpected '{{' in Properties near '{0}'. Only Group(\"Name\") {{ ... }} may open a brace here."),
						FText::FromString(Buffer.TrimStartAndEnd())));
						return false;
					}
					if (GroupName.IsEmpty())
					{
						FailWith(OutError, TEXT("DSH3131"), LOCTEXT("GroupRequiresANonEmptyName", "Group(...) requires a non-empty name."));
						return false;
					}

					int32 BraceDepth = 0;
					bool bInBlockString = false;
					int32 InnerStart = INDEX_NONE;
					int32 InnerEnd = INDEX_NONE;
					for (int32 Scan = Index; Scan < Content.Len(); ++Scan)
					{
						const TCHAR BlockChar = Content[Scan];
						if (bInBlockString)
						{
							if (BlockChar == TCHAR('\\'))
							{
								++Scan;
								continue;
							}
							if (BlockChar == TCHAR('"'))
							{
								bInBlockString = false;
							}
							continue;
						}
						if (BlockChar == TCHAR('"'))
						{
							bInBlockString = true;
							continue;
						}
						if (BlockChar == TCHAR('{'))
						{
							if (BraceDepth == 0)
							{
								InnerStart = Scan + 1;
							}
							++BraceDepth;
							continue;
						}
						if (BlockChar == TCHAR('}'))
						{
							--BraceDepth;
							if (BraceDepth == 0)
							{
								InnerEnd = Scan;
								break;
							}
						}
					}

					if (InnerStart == INDEX_NONE || InnerEnd == INDEX_NONE)
					{
						FailWith(OutError, TEXT("DSH3132"), FText::Format(
							LOCTEXT("UnterminatedGroupBlock", "Unterminated Group(\"{0}\") {{ ... }} block."),
							FText::FromString(GroupName)));
						return false;
					}

					const FString Inner = Content.Mid(InnerStart, InnerEnd - InnerStart);
					Buffer.Reset();
					// Nested Group("Outer") { Group("Inner") { ... } } composes into "Outer|Inner", matching
					// Unreal's native '|' sub-category syntax for the Group/Category property.
					const FString ComposedGroup = InheritedGroup.IsEmpty()
						? GroupName
						: InheritedGroup + TEXT("|") + GroupName;
					if (!ParsePropertyBlock(Inner, ComposedGroup, InOutNextAutoSort, OutProperties, OutError))
					{
						return false;
					}

					Index = InnerEnd + 1;
					while (Index < Content.Len() && FChar::IsWhitespace(Content[Index]))
					{
						++Index;
					}
					if (Index < Content.Len() && Content[Index] == TCHAR(';'))
					{
						++Index;
					}
					continue;
				}
			}

			Buffer.AppendChar(Char);
			++Index;
		}

		return FlushStatement(Buffer);
	}

	bool ParsePropertyStatements(const FString& BlockContent, TArray<FTextShaderPropertyDefinition>& OutProperties, FDreamShaderTextError& OutError)
	{
		int32 NextAutoSort = 0;
		return ParsePropertyBlock(RemoveComments(BlockContent), FString(), NextAutoSort, OutProperties, OutError);
	}

	bool ParseSettingStatements(const FString& BlockContent, TMap<FString, FString>& OutSettings, FDreamShaderTextError& OutError)
	{
		const TArray<FString> Statements = SplitStatements(RemoveComments(BlockContent));
		for (const FString& Statement : Statements)
		{
			FString Key;
			FString Value;
			// Use the quote/paren-aware top-level split (as the 6 other Key=Value sites do) so a setting
			// whose value is a struct/quoted literal containing '=' (e.g. (R=1,G=0,B=0)) splits on the
			// assignment, not the first inner '='.
			if (!SplitTopLevelAssignment(Statement, Key, Value))
			{
				FailWith(OutError, TEXT("DSH7040"), FText::Format(LOCTEXT("InvalidSettingDeclarationS", "Invalid setting declaration '{0}'."),
					FText::FromString(Statement)));
				return false;
			}

			Key = NormalizeSettingKey(Key);
			Value = Unquote(Value);
			if (Key.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH7041"), FText::Format(LOCTEXT("InvalidEmptySettingKeyInS", "Invalid empty setting key in '{0}'."),
					FText::FromString(Statement)));
				return false;
			}

			OutSettings.Add(Key, Value);
		}

		return true;
	}

	bool ParseTypedDeclarationStatement(const FString& Statement, FTextShaderVariableDeclaration& OutDeclaration, FDreamShaderTextError& OutError)
	{
		const FString Trimmed = Statement.TrimStartAndEnd();
		const int32 LastSpaceIndex = Trimmed.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastSpaceIndex == INDEX_NONE)
		{
			FailWith(OutError, TEXT("DSH3070"), FText::Format(LOCTEXT("InvalidTypedDeclarationS", "Invalid typed declaration '{0}'."),
					FText::FromString(Statement)));
			return false;
		}

		OutDeclaration.Type = Trimmed.Left(LastSpaceIndex).TrimStartAndEnd();
		OutDeclaration.Name = Trimmed.Mid(LastSpaceIndex + 1).TrimStartAndEnd();

		if (OutDeclaration.Type.IsEmpty() || !IsIdentifierToken(OutDeclaration.Name))
		{
			FailWith(OutError, TEXT("DSH3070"), FText::Format(LOCTEXT("InvalidTypedDeclarationS2", "Invalid typed declaration '{0}'."),
					FText::FromString(Statement)));
			return false;
		}

		return true;
	}

	bool ParseOutputStatements(
		const FString& BlockContent,
		TArray<FTextShaderVariableDeclaration>& OutOutputDeclarations,
		TArray<FTextShaderOutputBinding>& OutOutputs,
		FDreamShaderTextError& OutError)
	{
		const auto ParseOutputTarget = [&OutError](const FString& InTargetText, FTextShaderOutputBinding& OutBinding) -> bool
		{
			OutBinding.TargetText = InTargetText.TrimStartAndEnd();
			if (OutBinding.TargetText.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH3080"), LOCTEXT("OutputBindingTargetCannotBeEmpty", "Output binding target cannot be empty."));
				return false;
			}

			FString TargetText = OutBinding.TargetText;
			if (TargetText.StartsWith(TEXT("Base."), ESearchCase::IgnoreCase))
			{
				TargetText.RightChopInline(5, DREAMSHADER_ALLOW_SHRINKING_NO);
				TargetText.TrimStartAndEndInline();
				OutBinding.TargetKind = FTextShaderOutputBinding::ETargetKind::MaterialProperty;
				OutBinding.MaterialProperty = TargetText;
				if (OutBinding.MaterialProperty.IsEmpty())
				{
					FailWith(OutError, TEXT("DSH3081"), FText::Format(LOCTEXT("OutputBindingTargetSIsEmpty", "Output binding target '{0}' is empty."),
					FText::FromString(InTargetText)));
					return false;
				}
				return true;
			}

			if (!TargetText.StartsWith(TEXT("Expression"), ESearchCase::IgnoreCase))
			{
				FailWith(OutError, TEXT("DSH3082"), FText::Format(LOCTEXT("OutputBindingTargetSMustStart", "Output binding target '{0}' must start with Base. for material outputs or Expression(...) for output nodes."),
					FText::FromString(InTargetText)));
				return false;
			}

			const int32 OpenParenIndex = TargetText.Find(TEXT("("));
			const int32 CloseParenIndex = TargetText.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (OpenParenIndex == INDEX_NONE || CloseParenIndex == INDEX_NONE || CloseParenIndex <= OpenParenIndex)
			{
				FailWith(OutError, TEXT("DSH3083"), FText::Format(LOCTEXT("InvalidOutputExpressionTargetS", "Invalid output expression target '{0}'."),
					FText::FromString(InTargetText)));
				return false;
			}

			const FString ExpressionKeyword = TargetText.Left(OpenParenIndex).TrimStartAndEnd();
			if (!ExpressionKeyword.Equals(TEXT("Expression"), ESearchCase::IgnoreCase))
			{
				FailWith(OutError, TEXT("DSH3084"), FText::Format(LOCTEXT("UnsupportedOutputTargetS", "Unsupported output target '{0}'."),
					FText::FromString(InTargetText)));
				return false;
			}

			const FString ArgumentBlock = TargetText.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1).TrimStartAndEnd();
			const FString Suffix = TargetText.Mid(CloseParenIndex + 1).TrimStartAndEnd();
			if (!Suffix.StartsWith(TEXT("."), ESearchCase::CaseSensitive))
			{
				FailWith(OutError, TEXT("DSH3085"), FText::Format(LOCTEXT("ExpressionOutputTargetSMustSelect", "Expression output target '{0}' must select a pin with .Pin[index]."),
					FText::FromString(InTargetText)));
				return false;
			}

			FString PinSpecifier = Suffix.Mid(1).TrimStartAndEnd();
			if (!PinSpecifier.StartsWith(TEXT("Pin["), ESearchCase::IgnoreCase) || !PinSpecifier.EndsWith(TEXT("]")))
			{
				FailWith(OutError, TEXT("DSH3086"), FText::Format(LOCTEXT("ExpressionOutputTargetSMustUse", "Expression output target '{0}' must use .Pin[index] syntax."),
					FText::FromString(InTargetText)));
				return false;
			}

			const FString PinIndexText = PinSpecifier.Mid(4, PinSpecifier.Len() - 5).TrimStartAndEnd();
			if (!ParseIntegerLiteral(PinIndexText, OutBinding.ExpressionPinIndex) || OutBinding.ExpressionPinIndex < 0)
			{
				FailWith(OutError, TEXT("DSH3087"), FText::Format(LOCTEXT("ExpressionOutputTargetSHasAn", "Expression output target '{0}' has an invalid pin index."),
					FText::FromString(InTargetText)));
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
					FailWith(OutError, TEXT("DSH3088"), FText::Format(LOCTEXT("ExpressionOutputTargetArgumentSMust", "Expression output target argument '{0}' must use Key=Value syntax."),
					FText::FromString(ArgumentStatement)));
					return false;
				}

				ArgumentName = NormalizeSettingKey(ArgumentName);
				ArgumentValue = Unquote(ArgumentValue).TrimStartAndEnd();
				if (ArgumentName.IsEmpty() || ArgumentValue.IsEmpty())
				{
					FailWith(OutError, TEXT("DSH3089"), FText::Format(LOCTEXT("InvalidExpressionOutputTargetArgumentS", "Invalid expression output target argument '{0}'."),
					FText::FromString(ArgumentStatement)));
					return false;
				}

				if (OutBinding.ExpressionArguments.Contains(ArgumentName))
				{
					FailWith(OutError, TEXT("DSH3090"), FText::Format(LOCTEXT("ExpressionOutputTargetArgumentSIs", "Expression output target argument '{0}' is declared more than once."),
					FText::FromString(ArgumentName)));
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
				FailWith(OutError, TEXT("DSH3091"), FText::Format(LOCTEXT("ExpressionOutputTargetSMustSpecify", "Expression output target '{0}' must specify Class=\\\"...\\\"."),
					FText::FromString(InTargetText)));
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
				FTextShaderVariableDeclaration Declaration;
				if (ParseTypedDeclarationStatement(LeftSide, Declaration, OutError))
				{
					Declaration.bHasDefaultValue = true;
					Declaration.DefaultValueText = RightSide.TrimStartAndEnd();
					if (Declaration.DefaultValueText.IsEmpty())
					{
						FailWith(OutError, TEXT("DSH3092"), FText::Format(LOCTEXT("InvalidOutputDeclarationInitializerS", "Invalid output declaration initializer '{0}'."),
					FText::FromString(Statement)));
						return false;
					}

					OutOutputDeclarations.Add(Declaration);
					continue;
				}

				Binding.SourceText = RightSide.TrimStartAndEnd();
				if (Binding.SourceText.IsEmpty())
				{
					FailWith(OutError, TEXT("DSH3093"), FText::Format(LOCTEXT("InvalidOutputBindingS", "Invalid output binding '{0}'."),
					FText::FromString(Statement)));
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

	static bool ParseLayoutCallStatement(
		const FString& Statement,
		FString& OutCallName,
		TMap<FString, FString>& OutArguments,
		FDreamShaderTextError& OutError)
	{
		const FString Trimmed = Statement.TrimStartAndEnd();
		const int32 OpenParenIndex = Trimmed.Find(TEXT("("));
		const int32 CloseParenIndex = Trimmed.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (OpenParenIndex == INDEX_NONE || CloseParenIndex == INDEX_NONE || CloseParenIndex <= OpenParenIndex)
		{
			FailWith(OutError, TEXT("DSH3100"), FText::Format(LOCTEXT("InvalidLayoutStatementS", "Invalid Layout statement '{0}'."),
					FText::FromString(Statement)));
			return false;
		}

		if (!Trimmed.Mid(CloseParenIndex + 1).TrimStartAndEnd().IsEmpty())
		{
			FailWith(OutError, TEXT("DSH3101"), FText::Format(LOCTEXT("UnexpectedTextAfterLayoutStatementS", "Unexpected text after Layout statement '{0}'."),
					FText::FromString(Statement)));
			return false;
		}

		OutCallName = Trimmed.Left(OpenParenIndex).TrimStartAndEnd();
		if (!IsIdentifierToken(OutCallName))
		{
			FailWith(OutError, TEXT("DSH3102"), FText::Format(LOCTEXT("InvalidLayoutStatementNameInS", "Invalid Layout statement name in '{0}'."),
					FText::FromString(Statement)));
			return false;
		}

		OutArguments.Reset();
		const FString ArgumentBlock = Trimmed.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1).TrimStartAndEnd();
		for (const FString& ArgumentStatement : SplitTopLevelDelimited(ArgumentBlock, TCHAR(',')))
		{
			FString ArgumentName;
			FString ArgumentValue;
			if (!SplitTopLevelAssignment(ArgumentStatement, ArgumentName, ArgumentValue))
			{
				FailWith(OutError, TEXT("DSH3103"), FText::Format(LOCTEXT("LayoutArgumentSMustUseKey", "Layout argument '{0}' must use Key=Value syntax."),
					FText::FromString(ArgumentStatement)));
				return false;
			}

			ArgumentName = NormalizeSettingKey(ArgumentName);
			ArgumentValue = Unquote(ArgumentValue).TrimStartAndEnd();
			if (ArgumentName.IsEmpty() || ArgumentValue.IsEmpty())
			{
				FailWith(OutError, TEXT("DSH3104"), FText::Format(LOCTEXT("InvalidLayoutArgumentS", "Invalid Layout argument '{0}'."),
					FText::FromString(ArgumentStatement)));
				return false;
			}

			if (OutArguments.Contains(ArgumentName))
			{
				FailWith(OutError, TEXT("DSH3105"), FText::Format(LOCTEXT("LayoutArgumentSIsDeclaredMore", "Layout argument '{0}' is declared more than once."),
					FText::FromString(ArgumentName)));
				return false;
			}

			OutArguments.Add(ArgumentName, ArgumentValue);
		}

		return true;
	}

	static bool TryGetRequiredLayoutTextArgument(
		const TMap<FString, FString>& Arguments,
		const TCHAR* Name,
		FString& OutValue,
		FDreamShaderTextError& OutError)
	{
		const FString* Value = Arguments.Find(NormalizeSettingKey(Name));
		if (!Value || Value->TrimStartAndEnd().IsEmpty())
		{
			FailWith(OutError, TEXT("DSH3106"), FText::Format(LOCTEXT("LayoutArgumentSIsRequired", "Layout argument '{0}' is required."),
					FText::FromString(Name)));
			return false;
		}

		OutValue = Value->TrimStartAndEnd();
		return true;
	}

	static bool TryGetRequiredLayoutIntArgument(
		const TMap<FString, FString>& Arguments,
		const TCHAR* Name,
		int32& OutValue,
		FDreamShaderTextError& OutError)
	{
		const FString* Value = Arguments.Find(NormalizeSettingKey(Name));
		if (!Value || !ParseIntegerLiteral(*Value, OutValue))
		{
			FailWith(OutError, TEXT("DSH3107"), FText::Format(LOCTEXT("LayoutArgumentSMustBeAn", "Layout argument '{0}' must be an integer."),
					FText::FromString(Name)));
			return false;
		}

		return true;
	}

	bool ParseLayoutStatements(const FString& BlockContent, FTextShaderLayout& OutLayout, FDreamShaderTextError& OutError)
	{
		OutLayout = FTextShaderLayout{};
		const TArray<FString> Statements = SplitStatements(RemoveComments(BlockContent));
		for (const FString& Statement : Statements)
		{
			const FString Trimmed = Statement.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}

			FString CallName;
			TMap<FString, FString> Arguments;
			if (!ParseLayoutCallStatement(Trimmed, CallName, Arguments, OutError))
			{
				return false;
			}

			if (CallName.Equals(TEXT("Node"), ESearchCase::IgnoreCase))
			{
				FTextShaderLayoutNode Node;
				if (!TryGetRequiredLayoutTextArgument(Arguments, TEXT("Var"), Node.Var, OutError)
					|| !TryGetRequiredLayoutIntArgument(Arguments, TEXT("X"), Node.X, OutError)
					|| !TryGetRequiredLayoutIntArgument(Arguments, TEXT("Y"), Node.Y, OutError))
				{
					FailWith(OutError, TEXT("DSH3108"), FText::Format(LOCTEXT("InvalidLayoutNodeStatementSS", "Invalid Layout Node statement '{0}'. {1}"),
					FText::FromString(Trimmed),
					OutError.Message));
					return false;
				}

				OutLayout.Nodes.Add(Node);
				continue;
			}

			if (CallName.Equals(TEXT("Comment"), ESearchCase::IgnoreCase))
			{
				FTextShaderLayoutComment Comment;
				if (!TryGetRequiredLayoutTextArgument(Arguments, TEXT("Name"), Comment.Name, OutError)
					|| !TryGetRequiredLayoutIntArgument(Arguments, TEXT("X"), Comment.X, OutError)
					|| !TryGetRequiredLayoutIntArgument(Arguments, TEXT("Y"), Comment.Y, OutError)
					|| !TryGetRequiredLayoutIntArgument(Arguments, TEXT("W"), Comment.W, OutError)
					|| !TryGetRequiredLayoutIntArgument(Arguments, TEXT("H"), Comment.H, OutError))
				{
					FailWith(OutError, TEXT("DSH3109"), FText::Format(LOCTEXT("InvalidLayoutCommentStatementSS", "Invalid Layout Comment statement '{0}'. {1}"),
					FText::FromString(Trimmed),
					OutError.Message));
					return false;
				}

				if (const FString* ColorText = Arguments.Find(NormalizeSettingKey(TEXT("Color"))))
				{
					if (!ParseVectorLiteral(*ColorText, Comment.Color))
					{
						FailWith(OutError, TEXT("DSH3110"), FText::Format(LOCTEXT("LayoutCommentColorMustBeA", "Layout Comment Color must be a float4 literal in '{0}'."),
					FText::FromString(Trimmed)));
						return false;
					}
				}

				OutLayout.Comments.Add(Comment);
				continue;
			}

			FailWith(OutError, TEXT("DSH3111"), FText::Format(LOCTEXT("UnknownLayoutStatementS", "Unknown Layout statement '{0}'."),
					FText::FromString(CallName)));
			return false;
		}

		return true;
	}

	static FString ParseRegionDirectiveName(const FString& Line, const TCHAR* Directive)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		Trimmed.RightChopInline(FCString::Strlen(Directive), DREAMSHADER_ALLOW_SHRINKING_NO);
		Trimmed.TrimStartAndEndInline();
		return Unquote(Trimmed).TrimStartAndEnd();
	}

	static bool IsGraphDirective(const FString& TrimmedLine, const TCHAR* Directive)
	{
		const int32 DirectiveLength = FCString::Strlen(Directive);
		return TrimmedLine.Left(DirectiveLength).Equals(Directive, ESearchCase::IgnoreCase)
			&& (TrimmedLine.Len() == DirectiveLength || FChar::IsWhitespace(TrimmedLine[DirectiveLength]) || TrimmedLine[DirectiveLength] == TCHAR('"'));
	}

	bool ExtractGraphRegions(
		const FString& InCode,
		FString& OutCode,
		TArray<FTextShaderGraphRegion>& OutRegions,
		FDreamShaderTextError& OutError)
	{
		OutCode.Reset();
		OutRegions.Reset();

		struct FOpenRegion
		{
			FString Name;
			int32 StartLine = 1;
		};

		TArray<FOpenRegion> OpenRegions;
		TArray<FString> Lines;
		InCode.ParseIntoArrayLines(Lines, false);
		if (Lines.IsEmpty() && !InCode.IsEmpty())
		{
			Lines.Add(InCode);
		}

		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const int32 LineNumber = LineIndex + 1;
			const FString& Line = Lines[LineIndex];
			const FString Trimmed = Line.TrimStartAndEnd();
			if (IsGraphDirective(Trimmed, TEXT("#Region")))
			{
				const FString Name = ParseRegionDirectiveName(Trimmed, TEXT("#Region"));
				if (Name.IsEmpty())
				{
					FailWith(OutError, TEXT("DSH3120"), FText::Format(LOCTEXT("GraphRegionOnLineDMust", "Graph #Region on line {0} must include a name."),
					FText::AsNumber(LineNumber)));
					return false;
				}

				OpenRegions.Add({ Name, LineNumber + 1 });
				OutCode += FString::ChrN(Line.Len(), TCHAR(' '));
			}
			else if (IsGraphDirective(Trimmed, TEXT("#EndRegion")))
			{
				if (OpenRegions.IsEmpty())
				{
					FailWith(OutError, TEXT("DSH3121"), FText::Format(LOCTEXT("GraphEndRegionOnLineDHas", "Graph #EndRegion on line {0} has no matching #Region."),
					FText::AsNumber(LineNumber)));
					return false;
				}

				FOpenRegion OpenRegion = OpenRegions.Pop(DREAMSHADER_ALLOW_SHRINKING_NO);
				FTextShaderGraphRegion& Region = OutRegions.AddDefaulted_GetRef();
				Region.Name = OpenRegion.Name;
				Region.StartLine = OpenRegion.StartLine;
				Region.EndLine = FMath::Max(OpenRegion.StartLine, LineNumber - 1);
				OutCode += FString::ChrN(Line.Len(), TCHAR(' '));
			}
			else
			{
				OutCode += Line;
			}

			if (LineIndex + 1 < Lines.Num() || InCode.EndsWith(TEXT("\n")))
			{
				OutCode += TEXT("\n");
			}
		}

		if (!OpenRegions.IsEmpty())
		{
			FailWith(OutError, TEXT("DSH3122"), FText::Format(LOCTEXT("GraphRegionSIsMissingEndRegion", "Graph #Region '{0}' is missing #EndRegion."),
					FText::FromString(OpenRegions.Last().Name)));
			return false;
		}

		return true;
	}

	bool ParseTypedParameterStatements(const FString& BlockContent, TArray<FTextShaderFunctionParameter>& OutParameters, FDreamShaderTextError& OutError)
	{
		const TArray<FString> Statements = SplitStatements(RemoveComments(BlockContent));
		for (const FString& Statement : Statements)
		{
			FString Trimmed = Statement.TrimStartAndEnd();
			FTextShaderMetadata Metadata;
			if (!ParseTrailingMetadata(Trimmed, Metadata, OutError))
			{
				return false;
			}

			bool bOptional = false;
			if (Trimmed.StartsWith(TEXT("opt "), ESearchCase::IgnoreCase)
				|| Trimmed.Equals(TEXT("opt"), ESearchCase::IgnoreCase))
			{
				bOptional = true;
				Trimmed.RightChopInline(3, DREAMSHADER_ALLOW_SHRINKING_NO);
				Trimmed.TrimStartAndEndInline();
			}

			FString Left = Trimmed;
			FString Right;
			const bool bHasDefaultValue = SplitTopLevelAssignment(Trimmed, Left, Right);
			if (Left.TrimStartAndEnd().IsEmpty())
			{
				FailWith(OutError, TEXT("DSH3070"), FText::Format(LOCTEXT("InvalidTypedDeclarationS3", "Invalid typed declaration '{0}'."),
					FText::FromString(Statement)));
				return false;
			}

			FTextShaderVariableDeclaration Declaration;
			if (!ParseTypedDeclarationStatement(Left, Declaration, OutError))
			{
				return false;
			}

			FTextShaderFunctionParameter Parameter;
			Parameter.Type = Declaration.Type;
			Parameter.Name = Declaration.Name;
			Parameter.bOptional = bOptional;
			Parameter.bHasDefaultValue = bHasDefaultValue;
			Parameter.DefaultValueText = Right.TrimStartAndEnd();
			Parameter.Metadata = Metadata;
			OutParameters.Add(Parameter);
		}

		return true;
	}

	static int32 GetTrimStartDelta(const FString& Text)
	{
		int32 Index = 0;
		while (Text.IsValidIndex(Index) && FChar::IsWhitespace(Text[Index]))
		{
			++Index;
		}
		return Index;
	}

	static int32 CountLinesBeforeIndex(const FString& Text, const int32 TargetIndex)
	{
		int32 LineCount = 0;
		for (int32 Index = 0; Index < TargetIndex && Text.IsValidIndex(Index); ++Index)
		{
			if (Text[Index] == TCHAR('\n'))
			{
				++LineCount;
			}
		}
		return LineCount;
	}

	static void AdjustRegionsForTrim(TArray<FTextShaderGraphRegion>& Regions, const int32 TrimLineDelta)
	{
		if (TrimLineDelta <= 0)
		{
			return;
		}

		for (FTextShaderGraphRegion& Region : Regions)
		{
			Region.StartLine = FMath::Max(1, Region.StartLine - TrimLineDelta);
			Region.EndLine = FMath::Max(Region.StartLine, Region.EndLine - TrimLineDelta);
		}
	}

	bool ParseShaderBody(const FString& BodyContent, const int32 BodyContentStartIndex, FTextShaderDefinition& OutDefinition, FDreamShaderTextError& OutError)
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

			// The '=' between a section name and its { } block is optional sugar:
			// `Properties { ... }` and `Properties = { ... }` parse identically.
			Scanner.TryConsume(TCHAR('='));

			FString SectionBody;
			int32 SectionBodyStartIndex = INDEX_NONE;
			if (!Scanner.ExtractBalancedBlock(SectionBody, SectionBodyStartIndex, OutError))
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
			else if (SectionName.Equals(TEXT("Graph"), ESearchCase::IgnoreCase))
			{
				FDreamShaderTextError RegionError;
				FString CodeWithRegionDirectivesRemoved;
				if (!ExtractGraphRegions(SectionBody, CodeWithRegionDirectivesRemoved, OutDefinition.GraphRegions, RegionError))
				{
					OutError = RegionError;
					return false;
				}

				const int32 TrimDelta = GetTrimStartDelta(CodeWithRegionDirectivesRemoved);
				const int32 TrimLineDelta = CountLinesBeforeIndex(CodeWithRegionDirectivesRemoved, TrimDelta);
				AdjustRegionsForTrim(OutDefinition.GraphRegions, TrimLineDelta);

				OutDefinition.Code = CodeWithRegionDirectivesRemoved.TrimStartAndEnd();
				OutDefinition.CodeStartIndex = BodyContentStartIndex + SectionBodyStartIndex + TrimDelta;
			}
			else if (SectionName.Equals(TEXT("Layout"), ESearchCase::IgnoreCase))
			{
				if (!ParseLayoutStatements(SectionBody, OutDefinition.Layout, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
			{
				FailWith(OutError, TEXT("DSH3065"), LOCTEXT("ShaderGraphCodeDeprecated", "Shader graph sections now use Graph = { ... }. Function Code = { ... } is still supported."));
				return false;
			}
			else
			{
				FailWith(OutError, TEXT("DSH3060"), FText::Format(LOCTEXT("UnknownShaderSectionS", "Unknown shader section '{0}'."),
					FText::FromString(SectionName)));
				return false;
			}

			Scanner.TryConsume(TCHAR(';'));
		}
	}

	bool ParseFunctionBody(const FString& BodyContent, FTextShaderFunctionDefinition& OutFunction, FDreamShaderTextError& OutError)
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

			// The '=' between a section name and its { } block is optional sugar:
			// `Properties { ... }` and `Properties = { ... }` parse identically.
			Scanner.TryConsume(TCHAR('='));

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
			else if (SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase)
				|| SectionName.Equals(TEXT("Graph"), ESearchCase::IgnoreCase))
			{
				// `Graph` is the unified body keyword; `Code` stays accepted as a legacy alias.
				OutFunction.HLSL = SectionBody.TrimStartAndEnd();
			}
			else
			{
				FailWith(OutError, TEXT("DSH3061"), FText::Format(LOCTEXT("UnknownShaderFunctionSectionS", "Unknown shader function section '{0}'."),
					FText::FromString(SectionName)));
				return false;
			}

			Scanner.TryConsume(TCHAR(';'));
		}
	}

	bool ParseMaterialFunctionBody(const FString& BodyContent, const int32 BodyContentStartIndex, FTextShaderMaterialFunctionDefinition& OutFunction, FDreamShaderTextError& OutError)
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

			// The '=' between a section name and its { } block is optional sugar:
			// `Properties { ... }` and `Properties = { ... }` parse identically.
			Scanner.TryConsume(TCHAR('='));

			FString SectionBody;
			int32 SectionBodyStartIndex = INDEX_NONE;
			if (!Scanner.ExtractBalancedBlock(SectionBody, SectionBodyStartIndex, OutError))
			{
				return false;
			}

			if (SectionName.Equals(TEXT("Properties"), ESearchCase::IgnoreCase))
			{
				if (!ParsePropertyStatements(SectionBody, OutFunction.Properties, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Inputs"), ESearchCase::IgnoreCase))
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
			else if (SectionName.Equals(TEXT("Graph"), ESearchCase::IgnoreCase))
			{
				FDreamShaderTextError RegionError;
				FString CodeWithRegionDirectivesRemoved;
				if (!ExtractGraphRegions(SectionBody, CodeWithRegionDirectivesRemoved, OutFunction.GraphRegions, RegionError))
				{
					OutError = RegionError;
					return false;
				}

				const int32 TrimDelta = GetTrimStartDelta(CodeWithRegionDirectivesRemoved);
				const int32 TrimLineDelta = CountLinesBeforeIndex(CodeWithRegionDirectivesRemoved, TrimDelta);
				AdjustRegionsForTrim(OutFunction.GraphRegions, TrimLineDelta);

				OutFunction.Code = CodeWithRegionDirectivesRemoved.TrimStartAndEnd();
				OutFunction.CodeStartIndex = BodyContentStartIndex + SectionBodyStartIndex + TrimDelta;
			}
			else if (SectionName.Equals(TEXT("Layout"), ESearchCase::IgnoreCase))
			{
				if (!ParseLayoutStatements(SectionBody, OutFunction.Layout, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
			{
				FailWith(OutError, TEXT("DSH3066"), LOCTEXT("MaterialFunctionGraphCodeDeprecated", "ShaderFunction, ShaderLayer, and ShaderLayerBlend graph sections now use Graph = { ... }. Function Code = { ... } is still supported."));
				return false;
			}
			else
			{
				FailWith(OutError, TEXT("DSH3062"), FText::Format(LOCTEXT("UnknownMaterialFunctionSectionS", "Unknown material function section '{0}'."),
					FText::FromString(SectionName)));
				return false;
			}

			Scanner.TryConsume(TCHAR(';'));
		}
	}

	bool ParseVirtualFunctionBody(const FString& BodyContent, FTextShaderVirtualFunctionDefinition& OutFunction, FDreamShaderTextError& OutError)
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

			// The '=' between a section name and its { } block is optional sugar:
			// `Properties { ... }` and `Properties = { ... }` parse identically.
			Scanner.TryConsume(TCHAR('='));

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
			else if (SectionName.Equals(TEXT("Options"), ESearchCase::IgnoreCase)
				|| SectionName.Equals(TEXT("Settings"), ESearchCase::IgnoreCase))
			{
				if (!ParseSettingStatements(SectionBody, OutFunction.Options, OutError))
				{
					return false;
				}
			}
			else if (SectionName.Equals(TEXT("Graph"), ESearchCase::IgnoreCase)
				|| SectionName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
			{
				FailWith(OutError, TEXT("DSH3064"), LOCTEXT("VirtualFunctionNoGraphOrCode", "VirtualFunction declares an existing MaterialFunction asset and does not support Graph or Code sections."));
				return false;
			}
			else
			{
				FailWith(OutError, TEXT("DSH3063"), FText::Format(LOCTEXT("UnknownVirtualFunctionSectionS", "Unknown VirtualFunction section '{0}'."),
					FText::FromString(SectionName)));
				return false;
			}

			Scanner.TryConsume(TCHAR(';'));
		}
	}
}

#undef LOCTEXT_NAMESPACE
