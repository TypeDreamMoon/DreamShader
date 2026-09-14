// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The builtin catalog: every engine fact the front end needs, as plain data.
//
// DreamShaderLang depends on Core alone, yet `UE.TexCoord(Index = 0)` has to be typed, its
// arguments matched to pins and properties, and `m.BaseColor` recognised as a material attribute of
// three components. All of that is engine knowledge -- UMaterialExpression classes, their
// FExpressionInput members, their reflected properties, the material attribute table. It enters
// the front end through this struct and only through it.
//
// Two producers fill it: the editor, from reflection (Compiler side, BuildBuiltinCatalogFromReflection),
// and the tools, from the JSON the editor exports for the language service (LoadBuiltinCatalogFromJson).
// Both must produce the same content for the same engine; the JSON round-trips through
// SaveBuiltinCatalogToJson so that can be tested.
//
// FROZEN for batch 1 (M2+M3).

#pragma once

#include "CoreMinimal.h"
#include "IR/IRTypes.h"

namespace UE::DreamShader::IR
{
	/** What a pin carries or a property holds, as coarsely as the binder needs to know it. */
	enum class ECatalogValueType : uint8
	{
		Unknown,
		/** Any float1..4; the pin does not constrain the width. */
		Numeric,
		Float1,
		Float2,
		Float3,
		Float4,
		Bool,
		Int,
		Texture,
		SamplerState,
		MaterialAttributes,
		Substrate,
		/** Literal property kinds. */
		String,
		Name,
		Enum,
		/** An asset reference (a texture, a material function, a parameter collection). */
		Object,
		/** A StaticBool or a static-switch style input: connect a static bool expression. */
		StaticBool,
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(ECatalogValueType Type);
	DREAMSHADERLANG_API bool TryParseCatalogValueType(const FString& Text, ECatalogValueType& OutType);

	/** An input or output pin of a reflected expression class. */
	struct FCatalogPin
	{
		FString Name;
		ECatalogValueType Type = ECatalogValueType::Numeric;
		/** Inputs only: the pin must be connected (the engine errors on an empty one). */
		bool bRequired = false;
		/** Inputs only: the pin is an FExpressionInput that may also be given as a literal property (a "Const*" twin). */
		FString ConstPropertyName;
		/** The 1.x argument spellings that still name this pin (`UV` for `Coordinates`), case-sensitive. */
		TArray<FString> Aliases;
	};

	/** A reflected literal property of an expression class. */
	struct FCatalogProperty
	{
		FString Name;
		ECatalogValueType Type = ECatalogValueType::Unknown;
		/** Type == Enum: the accepted enumerator spellings, without the enum's prefix. */
		TArray<FString> EnumValues;
		/** As the engine spells the default, for the language service; may be empty. */
		FString DefaultText;
		/** The 1.x argument spellings that still name this property (`Index` for `CoordinateIndex`), case-sensitive. */
		TArray<FString> Aliases;
	};

	struct DREAMSHADERLANG_API FCatalogExpression
	{
		/** "UE" or "Substrate": the prefix the language uses. */
		FString Namespace;
		/** What follows the prefix: `UE.TextureCoordinate`. Case-sensitive. */
		FString ShortName;
		/** The UClass name without its U: "MaterialExpressionTextureCoordinate". */
		FString ClassName;
		/** "/Script/Engine.MaterialExpressionTextureCoordinate". */
		FString ClassPathName;
		/** The 1.x spellings that still resolve here, e.g. "TexCoord". Case-sensitive. */
		TArray<FString> Aliases;
		TArray<FCatalogPin> Inputs;
		/** Outputs[0] is the default output. A single unnamed output is one entry with an empty Name. */
		TArray<FCatalogPin> Outputs;
		TArray<FCatalogProperty> Properties;
		/**
		 * The canonical order for positional arguments (`UE.TexCoord(0)`): names of inputs and/or
		 * properties. Empty means the class takes named arguments only.
		 */
		TArray<FString> PositionalParameters;
		/** A parameter expression (Scalar/Vector/Texture/StaticBool...). */
		bool bIsParameter = false;
		/** A custom-output node (VolumetricAdvanced..., ClearCoatNormal...): a statement, no value. */
		bool bIsCustomOutput = false;
		/** The class is abstract or otherwise not creatable; listed so the language service can explain. */
		bool bIsAbstract = false;

		int32 FindInput(const FString& Name) const;
		int32 FindOutput(const FString& Name) const;
		int32 FindProperty(const FString& Name) const;
	};

	/** One entry of the material attribute table: what `m.BaseColor` refers to. */
	struct FCatalogMaterialAttribute
	{
		/** The spelling the language uses: "BaseColor", "EmissiveColor", "FrontMaterial"... */
		FString Name;
		/** The EMaterialProperty enumerator name: "MP_BaseColor". */
		FString PropertyName;
		/** The value the pin carries. */
		FIRType ValueType;
		/** Other spellings that resolve to this attribute ("Emissive" for EmissiveColor), case-sensitive. */
		TArray<FString> Aliases;
	};

	struct DREAMSHADERLANG_API FBuiltinCatalog
	{
		TArray<FCatalogExpression> Expressions;
		TArray<FCatalogMaterialAttribute> MaterialAttributes;
		/** Where it came from, for diagnostics: "reflection" or the manifest path. */
		FString Source;
		/** The engine version the reflection was taken from, "5.8", for the manifest header. */
		FString EngineVersion;

		/**
		 * `UE.TexCoord` -> the TextureCoordinate entry. Matches ShortName first, then Aliases,
		 * both case-sensitive: the 2.0 language is case-sensitive everywhere and this is where a
		 * `ue.texcoord` typo is caught rather than accepted. INDEX_NONE when nothing matches.
		 */
		int32 FindExpression(const FString& Namespace, const FString& Name) const;
		/** `UE.Expression(Class = "...")`: matches ShortName, ClassName or ClassPathName in any namespace. */
		int32 FindExpressionByClass(const FString& ClassSpecifier) const;
		/** Name or alias, case-sensitive. */
		int32 FindMaterialAttribute(const FString& Name) const;

		/** Case-insensitive lookups for the "did you mean" half of a diagnostic. INDEX_NONE if nothing is close. */
		int32 FindExpressionIgnoreCase(const FString& Namespace, const FString& Name) const;
		int32 FindMaterialAttributeIgnoreCase(const FString& Name) const;

		bool IsEmpty() const { return Expressions.Num() == 0; }
	};

	/**
	 * The manifest form. Schema, so the editor exporter and this loader agree:
	 *
	 *   { "schema": "dreamshader-builtin-catalog", "version": 1, "engine": "5.8",
	 *     "expressions": [ { "namespace", "shortName", "className", "classPathName", "aliases": [],
	 *                        "inputs":  [ { "name", "type", "required", "constProperty", "aliases": [] } ],
	 *                        "outputs": [ { "name", "type" } ],
	 *                        "properties": [ { "name", "type", "enumValues": [], "default", "aliases": [] } ],
	 *                        "positional": [], "isParameter", "isCustomOutput", "isAbstract" } ],
	 *     "materialAttributes": [ { "name", "property", "type": "float3", "aliases": [] } ] }
	 *
	 * The `type` strings are LexToString(ECatalogValueType); a material attribute's `type` is FIRType::ToString().
	 */
	DREAMSHADERLANG_API bool LoadBuiltinCatalogFromJson(const FString& Json, FBuiltinCatalog& OutCatalog, FString& OutError);
	DREAMSHADERLANG_API FString SaveBuiltinCatalogToJson(const FBuiltinCatalog& Catalog);

	/** The type a pin of the given catalog type is bound as; Numeric becomes float1 with bAnyWidth true. */
	DREAMSHADERLANG_API FIRType TypeFromCatalogValueType(ECatalogValueType Type, bool* bOutAnyWidth = nullptr);
}
