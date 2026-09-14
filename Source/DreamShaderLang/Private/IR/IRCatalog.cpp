// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FBuiltinCatalog: the lookups, and the manifest form the tools read.
//
// Everything here is case-SENSITIVE by default. That is the point of the catalog: 2.0 is a
// case-sensitive language, `ue.texcoord` is a typo, and this is the one place that can still tell.
// The IgnoreCase variants exist only to put a "did you mean" on the end of a diagnostic and must
// never be used to resolve a name.
//
// The JSON half is hand-written (IRJson.h) because this module may not depend on the Json module
// (CONTRACT §0.9). The schema is the one spelled in IRCatalog.h and the writer is canonical: every
// optional field is omitted when it holds its default, so SaveBuiltinCatalogToJson(load(x)) == x
// byte for byte for any x this writer produced.

#include "IR/IRCatalog.h"

#include "IRJson.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"

#include "Misc/CString.h"

namespace UE::DreamShader::IR
{
	namespace Private
	{
		static const TCHAR* const GCatalogSchemaName = TEXT("dreamshader-builtin-catalog");
		static constexpr int32 GCatalogSchemaVersion = 1;

		/** Every ECatalogValueType spelling, in enum order, for the parser's second pass. */
		static const ECatalogValueType GCatalogValueTypes[] =
		{
			ECatalogValueType::Unknown,
			ECatalogValueType::Numeric,
			ECatalogValueType::Float1,
			ECatalogValueType::Float2,
			ECatalogValueType::Float3,
			ECatalogValueType::Float4,
			ECatalogValueType::Bool,
			ECatalogValueType::Int,
			ECatalogValueType::Texture,
			ECatalogValueType::SamplerState,
			ECatalogValueType::MaterialAttributes,
			ECatalogValueType::Substrate,
			ECatalogValueType::String,
			ECatalogValueType::Name,
			ECatalogValueType::Enum,
			ECatalogValueType::Object,
			ECatalogValueType::StaticBool,
		};

		/**
		 * FIRType::ToString() read back. The material attribute table is the only place a full type
		 * crosses the manifest, and it is always one of `float1..4`, `bool`, `material` or
		 * `Substrate` -- but the parser accepts everything ToString() can produce so the two stay
		 * exact inverses.
		 */
		static bool TryParseIRTypeString(const FString& Text, FIRType& OutType)
		{
			if (Text.IsEmpty())
			{
				return false;
			}

			struct FFixedSpelling
			{
				const TCHAR* Spelling;
				FIRType (*Make)();
			};

			static const FFixedSpelling FixedSpellings[] =
			{
				{ TEXT("void"),            []() { return FIRType::Void(); } },
				{ TEXT("<error>"),         []() { return FIRType::Error(); } },
				{ TEXT("material"),        []() { return FIRType::Material(); } },
				{ TEXT("Substrate"),       []() { return FIRType::Substrate(); } },
				{ TEXT("SamplerState"),    []() { return FIRType::Sampler(); } },
				{ TEXT("Texture"),         []() { return FIRType::TextureOf(Lang::ETextureKind::None); } },
				{ TEXT("Texture2D"),       []() { return FIRType::TextureOf(Lang::ETextureKind::Texture2D); } },
				{ TEXT("TextureCube"),     []() { return FIRType::TextureOf(Lang::ETextureKind::TextureCube); } },
				{ TEXT("Texture2DArray"),  []() { return FIRType::TextureOf(Lang::ETextureKind::Texture2DArray); } },
				{ TEXT("Texture3D"),       []() { return FIRType::TextureOf(Lang::ETextureKind::Texture3D); } },
				{ TEXT("VolumeTexture"),   []() { return FIRType::TextureOf(Lang::ETextureKind::VolumeTexture); } },
			};

			for (const FFixedSpelling& Fixed : FixedSpellings)
			{
				if (Text.Equals(Fixed.Spelling, ESearchCase::CaseSensitive))
				{
					OutType = Fixed.Make();
					return true;
				}
			}

			if (Text.StartsWith(TEXT("struct#"), ESearchCase::CaseSensitive))
			{
				OutType = FIRType::Struct(FCString::Atoi(*Text.Mid(7)));
				return true;
			}
			if (Text.StartsWith(TEXT("node#"), ESearchCase::CaseSensitive))
			{
				OutType = FIRType::Node(FCString::Atoi(*Text.Mid(5)));
				return true;
			}

			struct FNumericSpelling
			{
				const TCHAR* Prefix;
				EIRTypeKind Kind;
			};

			// `double` before `d`-nothing, and `float` before nothing else that starts with f --
			// no prefix here is a prefix of another, so the order does not matter, but the list is
			// kept in the same order as EIRTypeKind for readability.
			static const FNumericSpelling NumericSpellings[] =
			{
				{ TEXT("bool"),   EIRTypeKind::Bool },
				{ TEXT("int"),    EIRTypeKind::Int },
				{ TEXT("uint"),   EIRTypeKind::UInt },
				{ TEXT("float"),  EIRTypeKind::Float },
				{ TEXT("half"),   EIRTypeKind::Half },
				{ TEXT("double"), EIRTypeKind::Double },
			};

			for (const FNumericSpelling& Numeric : NumericSpellings)
			{
				const int32 PrefixLength = FCString::Strlen(Numeric.Prefix);
				if (!Text.StartsWith(Numeric.Prefix, ESearchCase::CaseSensitive))
				{
					continue;
				}

				const FString Suffix = Text.Mid(PrefixLength);
				if (Suffix.IsEmpty())
				{
					OutType = FIRType::Scalar(Numeric.Kind);
					return true;
				}

				int32 SeparatorIndex = INDEX_NONE;
				if (Suffix.FindChar(TEXT('x'), SeparatorIndex))
				{
					const FString RowsText = Suffix.Left(SeparatorIndex);
					const FString ColsText = Suffix.Mid(SeparatorIndex + 1);
					if (RowsText.IsEmpty() || ColsText.IsEmpty() || !RowsText.IsNumeric() || !ColsText.IsNumeric())
					{
						return false;
					}
					OutType = FIRType::Matrix(Numeric.Kind, FCString::Atoi(*RowsText), FCString::Atoi(*ColsText));
					return true;
				}

				if (!Suffix.IsNumeric())
				{
					return false;
				}
				OutType = FIRType::Vector(Numeric.Kind, FCString::Atoi(*Suffix));
				return true;
			}

			return false;
		}

		// Key order follows the schema comment in IRCatalog.h exactly -- name, type, required,
		// constProperty, aliases -- and every optional key is omitted when it holds its default, so
		// Save(Load(x)) == x byte for byte.
		static void SaveCatalogPin(FIRJsonWriter& Writer, const FCatalogPin& Pin, const bool bIsInput)
		{
			Writer.BeginObject();
			Writer.KeyString(TEXT("name"), Pin.Name);
			Writer.KeyString(TEXT("type"), LexToString(Pin.Type));
			if (bIsInput && Pin.bRequired)
			{
				Writer.KeyBool(TEXT("required"), true);
			}
			if (bIsInput && !Pin.ConstPropertyName.IsEmpty())
			{
				Writer.KeyString(TEXT("constProperty"), Pin.ConstPropertyName);
			}
			// Inputs only: the schema gives outputs just `name` and `type`, because the 1.x
			// spellings an alias list carries were all ARGUMENT names.
			if (bIsInput && Pin.Aliases.Num() > 0)
			{
				Writer.KeyStringArray(TEXT("aliases"), Pin.Aliases);
			}
			Writer.EndObject();
		}

		static void LoadCatalogPin(const FIRJsonValue& Object, FCatalogPin& OutPin)
		{
			OutPin.Name = Object.GetString(TEXT("name"));
			if (!TryParseCatalogValueType(Object.GetString(TEXT("type")), OutPin.Type))
			{
				// An unreadable spelling is Unknown rather than a hard failure: a newer exporter
				// may know a pin kind this build does not, and one strange pin must not cost the
				// tools the whole catalog.
				OutPin.Type = ECatalogValueType::Unknown;
			}
			OutPin.bRequired = Object.GetBool(TEXT("required"), false);
			OutPin.ConstPropertyName = Object.GetString(TEXT("constProperty"));
			Object.GetStringArray(TEXT("aliases"), OutPin.Aliases);
		}
	}

	// --------------------------------------------------------------------------- value types

	const TCHAR* LexToString(const ECatalogValueType Type)
	{
		switch (Type)
		{
		case ECatalogValueType::Unknown:            return TEXT("Unknown");
		case ECatalogValueType::Numeric:            return TEXT("Numeric");
		case ECatalogValueType::Float1:             return TEXT("Float1");
		case ECatalogValueType::Float2:             return TEXT("Float2");
		case ECatalogValueType::Float3:             return TEXT("Float3");
		case ECatalogValueType::Float4:             return TEXT("Float4");
		case ECatalogValueType::Bool:               return TEXT("Bool");
		case ECatalogValueType::Int:                return TEXT("Int");
		case ECatalogValueType::Texture:            return TEXT("Texture");
		case ECatalogValueType::SamplerState:       return TEXT("SamplerState");
		case ECatalogValueType::MaterialAttributes: return TEXT("MaterialAttributes");
		case ECatalogValueType::Substrate:          return TEXT("Substrate");
		case ECatalogValueType::String:             return TEXT("String");
		case ECatalogValueType::Name:               return TEXT("Name");
		case ECatalogValueType::Enum:               return TEXT("Enum");
		case ECatalogValueType::Object:             return TEXT("Object");
		case ECatalogValueType::StaticBool:         return TEXT("StaticBool");
		}

		return TEXT("Unknown");
	}

	bool TryParseCatalogValueType(const FString& Text, ECatalogValueType& OutType)
	{
		if (Text.IsEmpty())
		{
			return false;
		}

		for (const ECatalogValueType Candidate : Private::GCatalogValueTypes)
		{
			if (Text.Equals(LexToString(Candidate), ESearchCase::CaseSensitive))
			{
				OutType = Candidate;
				return true;
			}
		}

		// Second pass, case-insensitive. The manifest is machine-written so the first pass always
		// wins in practice; this only forgives a hand-edited file, and it never changes what the
		// LANGUAGE accepts -- these spellings are not user-visible identifiers.
		for (const ECatalogValueType Candidate : Private::GCatalogValueTypes)
		{
			if (Text.Equals(LexToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutType = Candidate;
				return true;
			}
		}

		return false;
	}

	FIRType TypeFromCatalogValueType(const ECatalogValueType Type, bool* bOutAnyWidth)
	{
		if (bOutAnyWidth != nullptr)
		{
			*bOutAnyWidth = false;
		}

		switch (Type)
		{
		case ECatalogValueType::Numeric:
			// The pin does not constrain the width, so the type is the narrowest thing that can
			// feed it and the caller is told not to check the width at all.
			if (bOutAnyWidth != nullptr)
			{
				*bOutAnyWidth = true;
			}
			return FIRType::Float(1);
		case ECatalogValueType::Float1:             return FIRType::Float(1);
		case ECatalogValueType::Float2:             return FIRType::Float(2);
		case ECatalogValueType::Float3:             return FIRType::Float(3);
		case ECatalogValueType::Float4:             return FIRType::Float(4);
		case ECatalogValueType::Bool:               return FIRType::Bool(1);
		// A static bool is a bool as far as the type system goes; whether the VALUE is static is a
		// property of the expression feeding it, which the binder tracks separately.
		case ECatalogValueType::StaticBool:         return FIRType::Bool(1);
		case ECatalogValueType::Int:                return FIRType::Scalar(EIRTypeKind::Int);
		// A texture pin the catalog could only describe as "a texture" accepts any texture kind;
		// ClassifyConversion knows that an unspecified ETextureKind::None target is a wildcard.
		case ECatalogValueType::Texture:            return FIRType::TextureOf(Lang::ETextureKind::None);
		case ECatalogValueType::SamplerState:       return FIRType::Sampler();
		case ECatalogValueType::MaterialAttributes: return FIRType::Material();
		case ECatalogValueType::Substrate:          return FIRType::Substrate();

		// String / Name / Enum / Object are LITERAL property kinds. They never carry a value, so
		// they have no FIRType: the binder matches the literal's shape against the property
		// directly and must not route these through ClassifyConversion.
		case ECatalogValueType::String:
		case ECatalogValueType::Name:
		case ECatalogValueType::Enum:
		case ECatalogValueType::Object:
		case ECatalogValueType::Unknown:
			return FIRType::Error();
		}

		return FIRType::Error();
	}

	// ---------------------------------------------------------------------------- expressions

	// The canonical name wins over every alias, which is why these are two passes and not one loop
	// with an inner alias check: `UE.TextureSample(Coordinates = ..)` must find the pin actually
	// called Coordinates even if some other pin of the same class carries `Coordinates` as a 1.x
	// alias. Same shape as FindExpression and FindMaterialAttribute.

	int32 FCatalogExpression::FindInput(const FString& Name) const
	{
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			if (Inputs[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}

		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			for (const FString& Alias : Inputs[Index].Aliases)
			{
				if (Alias.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Index;
				}
			}
		}

		return INDEX_NONE;
	}

	int32 FCatalogExpression::FindOutput(const FString& Name) const
	{
		// Outputs have no aliases: an output is selected by member access on a Node value, and the
		// 1.x spellings the alias lists carry were all ARGUMENT names.
		for (int32 Index = 0; Index < Outputs.Num(); ++Index)
		{
			if (Outputs[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FCatalogExpression::FindProperty(const FString& Name) const
	{
		for (int32 Index = 0; Index < Properties.Num(); ++Index)
		{
			if (Properties[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}

		for (int32 Index = 0; Index < Properties.Num(); ++Index)
		{
			for (const FString& Alias : Properties[Index].Aliases)
			{
				if (Alias.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Index;
				}
			}
		}

		return INDEX_NONE;
	}

	// -------------------------------------------------------------------------------- catalog

	int32 FBuiltinCatalog::FindExpression(const FString& Namespace, const FString& Name) const
	{
		if (Name.IsEmpty())
		{
			return INDEX_NONE;
		}

		// An empty Namespace means "any": the manifest exporter always fills one, but a caller
		// that has only a bare name (a `Class = "..."` specifier reaching this by another route)
		// should not have to guess between UE and Substrate.
		const bool bAnyNamespace = Namespace.IsEmpty();

		// ShortName wins over an alias everywhere, so a 1.x alias can never shadow a 2.0 name.
		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			const FCatalogExpression& Expression = Expressions[Index];
			if (!bAnyNamespace && !Expression.Namespace.Equals(Namespace, ESearchCase::CaseSensitive))
			{
				continue;
			}
			if (Expression.ShortName.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}

		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			const FCatalogExpression& Expression = Expressions[Index];
			if (!bAnyNamespace && !Expression.Namespace.Equals(Namespace, ESearchCase::CaseSensitive))
			{
				continue;
			}
			for (const FString& Alias : Expression.Aliases)
			{
				if (Alias.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Index;
				}
			}
		}

		return INDEX_NONE;
	}

	int32 FBuiltinCatalog::FindExpressionByClass(const FString& ClassSpecifier) const
	{
		const FString Specifier = ClassSpecifier.TrimStartAndEnd();
		if (Specifier.IsEmpty())
		{
			return INDEX_NONE;
		}

		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			if (Expressions[Index].ShortName.Equals(Specifier, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			if (Expressions[Index].ClassName.Equals(Specifier, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			if (Expressions[Index].ClassPathName.Equals(Specifier, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}

		// The two spellings 1.x also accepted (ResolveMaterialExpressionClass): the class name with
		// the `MaterialExpression` prefix left off, and the C++ name with its leading `U`.
		const FString Prefixed = FString(TEXT("MaterialExpression")) + Specifier;
		FString Unprefixed;
		if (Specifier.StartsWith(TEXT("UMaterialExpression"), ESearchCase::CaseSensitive))
		{
			Unprefixed = Specifier.Mid(1);
		}

		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			const FCatalogExpression& Expression = Expressions[Index];
			if (Expression.ClassName.Equals(Prefixed, ESearchCase::CaseSensitive))
			{
				return Index;
			}
			if (!Unprefixed.IsEmpty() && Expression.ClassName.Equals(Unprefixed, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}

		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			for (const FString& Alias : Expressions[Index].Aliases)
			{
				if (Alias.Equals(Specifier, ESearchCase::CaseSensitive))
				{
					return Index;
				}
			}
		}

		return INDEX_NONE;
	}

	int32 FBuiltinCatalog::FindMaterialAttribute(const FString& Name) const
	{
		if (Name.IsEmpty())
		{
			return INDEX_NONE;
		}

		for (int32 Index = 0; Index < MaterialAttributes.Num(); ++Index)
		{
			if (MaterialAttributes[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		for (int32 Index = 0; Index < MaterialAttributes.Num(); ++Index)
		{
			for (const FString& Alias : MaterialAttributes[Index].Aliases)
			{
				if (Alias.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Index;
				}
			}
		}

		return INDEX_NONE;
	}

	int32 FBuiltinCatalog::FindExpressionIgnoreCase(const FString& Namespace, const FString& Name) const
	{
		if (Name.IsEmpty())
		{
			return INDEX_NONE;
		}

		const bool bAnyNamespace = Namespace.IsEmpty();

		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			const FCatalogExpression& Expression = Expressions[Index];
			if (!bAnyNamespace && !Expression.Namespace.Equals(Namespace, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (Expression.ShortName.Equals(Name, ESearchCase::IgnoreCase))
			{
				return Index;
			}
			for (const FString& Alias : Expression.Aliases)
			{
				if (Alias.Equals(Name, ESearchCase::IgnoreCase))
				{
					return Index;
				}
			}
		}

		return INDEX_NONE;
	}

	int32 FBuiltinCatalog::FindMaterialAttributeIgnoreCase(const FString& Name) const
	{
		if (Name.IsEmpty())
		{
			return INDEX_NONE;
		}

		for (int32 Index = 0; Index < MaterialAttributes.Num(); ++Index)
		{
			const FCatalogMaterialAttribute& Attribute = MaterialAttributes[Index];
			if (Attribute.Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				return Index;
			}
			for (const FString& Alias : Attribute.Aliases)
			{
				if (Alias.Equals(Name, ESearchCase::IgnoreCase))
				{
					return Index;
				}
			}
		}

		return INDEX_NONE;
	}

	// ------------------------------------------------------------------------------- manifest

	bool LoadBuiltinCatalogFromJson(const FString& Json, FBuiltinCatalog& OutCatalog, FString& OutError)
	{
		OutCatalog = FBuiltinCatalog();
		OutError.Reset();

		Private::FIRJsonValue Root;
		FString ParseError;
		if (!Private::ParseIRJson(Json, Root, ParseError))
		{
			OutError = FString::Printf(TEXT("The builtin catalog is not valid JSON (%s)."), *ParseError); /* I18N-EXEMPT: OutError of a frozen FString-typed signature; not a sink diagnostic */
			return false;
		}
		if (!Root.IsObject())
		{
			OutError = TEXT("The builtin catalog must be a JSON object."); /* I18N-EXEMPT: OutError of a frozen FString-typed signature; not a sink diagnostic */
			return false;
		}

		const FString Schema = Root.GetString(TEXT("schema"));
		if (!Schema.Equals(Private::GCatalogSchemaName, ESearchCase::CaseSensitive))
		{
			OutError = FString::Printf( /* I18N-EXEMPT: OutError of a frozen FString-typed signature */
				TEXT("Expected schema '%s', found '%s'."),
				Private::GCatalogSchemaName,
				*Schema);
			return false;
		}

		const int64 Version = Root.GetInt(TEXT("version"), 0);
		if (Version != Private::GCatalogSchemaVersion)
		{
			OutError = FString::Printf( /* I18N-EXEMPT: OutError of a frozen FString-typed signature */
				TEXT("Expected schema version %d, found %lld."),
				Private::GCatalogSchemaVersion,
				Version);
			return false;
		}

		OutCatalog.EngineVersion = Root.GetString(TEXT("engine"));
		// The caller knows the path it read; all this can honestly say is "not reflection".
		OutCatalog.Source = TEXT("json");

		if (const Private::FIRJsonValue* Expressions = Root.FindArray(TEXT("expressions")))
		{
			OutCatalog.Expressions.Reserve(Expressions->Items.Num());
			for (const Private::FIRJsonValue& Item : Expressions->Items)
			{
				if (!Item.IsObject())
				{
					continue;
				}

				FCatalogExpression Expression;
				Expression.Namespace = Item.GetString(TEXT("namespace"));
				Expression.ShortName = Item.GetString(TEXT("shortName"));
				Expression.ClassName = Item.GetString(TEXT("className"));
				Expression.ClassPathName = Item.GetString(TEXT("classPathName"));
				Item.GetStringArray(TEXT("aliases"), Expression.Aliases);
				Item.GetStringArray(TEXT("positional"), Expression.PositionalParameters);
				Expression.bIsParameter = Item.GetBool(TEXT("isParameter"), false);
				Expression.bIsCustomOutput = Item.GetBool(TEXT("isCustomOutput"), false);
				Expression.bIsAbstract = Item.GetBool(TEXT("isAbstract"), false);

				if (const Private::FIRJsonValue* Inputs = Item.FindArray(TEXT("inputs")))
				{
					Expression.Inputs.Reserve(Inputs->Items.Num());
					for (const Private::FIRJsonValue& PinItem : Inputs->Items)
					{
						if (!PinItem.IsObject())
						{
							continue;
						}
						FCatalogPin Pin;
						Private::LoadCatalogPin(PinItem, Pin);
						Expression.Inputs.Add(MoveTemp(Pin));
					}
				}

				if (const Private::FIRJsonValue* Outputs = Item.FindArray(TEXT("outputs")))
				{
					Expression.Outputs.Reserve(Outputs->Items.Num());
					for (const Private::FIRJsonValue& PinItem : Outputs->Items)
					{
						if (!PinItem.IsObject())
						{
							continue;
						}
						FCatalogPin Pin;
						Private::LoadCatalogPin(PinItem, Pin);
						// Only an input can be required, carry a Const* twin or have argument
						// aliases; drop whatever an exporter may have written so a round-trip
						// normalises the file rather than growing it.
						Pin.bRequired = false;
						Pin.ConstPropertyName.Reset();
						Pin.Aliases.Reset();
						Expression.Outputs.Add(MoveTemp(Pin));
					}
				}

				if (const Private::FIRJsonValue* Properties = Item.FindArray(TEXT("properties")))
				{
					Expression.Properties.Reserve(Properties->Items.Num());
					for (const Private::FIRJsonValue& PropertyItem : Properties->Items)
					{
						if (!PropertyItem.IsObject())
						{
							continue;
						}
						FCatalogProperty Property;
						Property.Name = PropertyItem.GetString(TEXT("name"));
						if (!TryParseCatalogValueType(PropertyItem.GetString(TEXT("type")), Property.Type))
						{
							Property.Type = ECatalogValueType::Unknown;
						}
						PropertyItem.GetStringArray(TEXT("enumValues"), Property.EnumValues);
						Property.DefaultText = PropertyItem.GetString(TEXT("default"));
						PropertyItem.GetStringArray(TEXT("aliases"), Property.Aliases);
						Expression.Properties.Add(MoveTemp(Property));
					}
				}

				OutCatalog.Expressions.Add(MoveTemp(Expression));
			}
		}

		if (const Private::FIRJsonValue* Attributes = Root.FindArray(TEXT("materialAttributes")))
		{
			OutCatalog.MaterialAttributes.Reserve(Attributes->Items.Num());
			for (const Private::FIRJsonValue& Item : Attributes->Items)
			{
				if (!Item.IsObject())
				{
					continue;
				}

				FCatalogMaterialAttribute Attribute;
				Attribute.Name = Item.GetString(TEXT("name"));
				Attribute.PropertyName = Item.GetString(TEXT("property"));
				if (!Private::TryParseIRTypeString(Item.GetString(TEXT("type")), Attribute.ValueType))
				{
					Attribute.ValueType = FIRType::Error();
				}
				Item.GetStringArray(TEXT("aliases"), Attribute.Aliases);
				OutCatalog.MaterialAttributes.Add(MoveTemp(Attribute));
			}
		}

		return true;
	}

	FString SaveBuiltinCatalogToJson(const FBuiltinCatalog& Catalog)
	{
		Private::FIRJsonWriter Writer;

		Writer.BeginObject();
		Writer.KeyString(TEXT("schema"), Private::GCatalogSchemaName);
		Writer.KeyInt(TEXT("version"), Private::GCatalogSchemaVersion);
		Writer.KeyString(TEXT("engine"), Catalog.EngineVersion);

		Writer.Key(TEXT("expressions"));
		Writer.BeginArray();
		for (const FCatalogExpression& Expression : Catalog.Expressions)
		{
			Writer.BeginObject();
			Writer.KeyString(TEXT("namespace"), Expression.Namespace);
			Writer.KeyString(TEXT("shortName"), Expression.ShortName);
			Writer.KeyString(TEXT("className"), Expression.ClassName);
			Writer.KeyString(TEXT("classPathName"), Expression.ClassPathName);

			if (Expression.Aliases.Num() > 0)
			{
				Writer.KeyStringArray(TEXT("aliases"), Expression.Aliases);
			}

			if (Expression.Inputs.Num() > 0)
			{
				Writer.Key(TEXT("inputs"));
				Writer.BeginArray();
				for (const FCatalogPin& Pin : Expression.Inputs)
				{
					Private::SaveCatalogPin(Writer, Pin, /*bIsInput*/ true);
				}
				Writer.EndArray();
			}

			if (Expression.Outputs.Num() > 0)
			{
				Writer.Key(TEXT("outputs"));
				Writer.BeginArray();
				for (const FCatalogPin& Pin : Expression.Outputs)
				{
					Private::SaveCatalogPin(Writer, Pin, /*bIsInput*/ false);
				}
				Writer.EndArray();
			}

			if (Expression.Properties.Num() > 0)
			{
				Writer.Key(TEXT("properties"));
				Writer.BeginArray();
				for (const FCatalogProperty& Property : Expression.Properties)
				{
					Writer.BeginObject();
					Writer.KeyString(TEXT("name"), Property.Name);
					Writer.KeyString(TEXT("type"), LexToString(Property.Type));
					if (Property.EnumValues.Num() > 0)
					{
						Writer.KeyStringArray(TEXT("enumValues"), Property.EnumValues);
					}
					if (!Property.DefaultText.IsEmpty())
					{
						Writer.KeyString(TEXT("default"), Property.DefaultText);
					}
					if (Property.Aliases.Num() > 0)
					{
						Writer.KeyStringArray(TEXT("aliases"), Property.Aliases);
					}
					Writer.EndObject();
				}
				Writer.EndArray();
			}

			if (Expression.PositionalParameters.Num() > 0)
			{
				Writer.KeyStringArray(TEXT("positional"), Expression.PositionalParameters);
			}
			if (Expression.bIsParameter)
			{
				Writer.KeyBool(TEXT("isParameter"), true);
			}
			if (Expression.bIsCustomOutput)
			{
				Writer.KeyBool(TEXT("isCustomOutput"), true);
			}
			if (Expression.bIsAbstract)
			{
				Writer.KeyBool(TEXT("isAbstract"), true);
			}

			Writer.EndObject();
		}
		Writer.EndArray();

		Writer.Key(TEXT("materialAttributes"));
		Writer.BeginArray();
		for (const FCatalogMaterialAttribute& Attribute : Catalog.MaterialAttributes)
		{
			Writer.BeginObject();
			Writer.KeyString(TEXT("name"), Attribute.Name);
			Writer.KeyString(TEXT("property"), Attribute.PropertyName);
			Writer.KeyString(TEXT("type"), Attribute.ValueType.ToString());
			if (Attribute.Aliases.Num() > 0)
			{
				Writer.KeyStringArray(TEXT("aliases"), Attribute.Aliases);
			}
			Writer.EndObject();
		}
		Writer.EndArray();

		Writer.EndObject();
		return Writer.Release();
	}
}
