// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Compiler/DreamShaderBuiltinCatalogReflection.h"

#include "Compiler/DreamShaderIREmitter.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
// TryResolveKnownExpressionOutputComponentCount: 1.x's table of the output widths the engine's own
// reflection does not report. An inline in this header, so it is reused rather than re-derived.
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeShared.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionParameter.h"
#include "MaterialValueType.h"
#include "Misc/EngineVersion.h"
#include "SceneTypes.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
#include "Materials/MaterialExpressionSubstrate.h"
#endif

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		/**
		 * The 1.x `Substrate.*` table, ported from the file-static FindSubstrateBuiltinDescriptor in
		 * MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeUE.cpp (~21).
		 *
		 * Two things come out of it: which classes live in the `Substrate` namespace rather than
		 * `UE`, and the 1.x spellings that must keep resolving -- `Substrate.HorizontalMix` and
		 * `Substrate.HorizontalMixing` are the same node, and a source that says either has to keep
		 * compiling after M4 deletes the 1.x front end.
		 *
		 * Only the CLASS list is authoritative here. A Substrate class this table does not name
		 * still lands in the catalog, in the Substrate namespace, under its reflected short name;
		 * the table only adds the aliases and pins the 1.x spelling.
		 */
		struct FSubstrateCatalogAlias
		{
			const TCHAR* ClassName;
			const TCHAR* Alias;
		};

		const FSubstrateCatalogAlias SubstrateCatalogAliases[] =
		{
			{ TEXT("MaterialExpressionSubstrateShadingModels"),                TEXT("ShadingModels") },
			{ TEXT("MaterialExpressionSubstrateSlabBSDF"),                     TEXT("Slab") },
			{ TEXT("MaterialExpressionSubstrateSimpleClearCoatBSDF"),          TEXT("SimpleClearCoat") },
			{ TEXT("MaterialExpressionSubstrateVolumetricFogCloudBSDF"),       TEXT("VolumetricFogCloud") },
			{ TEXT("MaterialExpressionSubstrateUnlitBSDF"),                    TEXT("Unlit") },
			{ TEXT("MaterialExpressionSubstrateHairBSDF"),                     TEXT("Hair") },
			{ TEXT("MaterialExpressionSubstrateEyeBSDF"),                      TEXT("Eye") },
			{ TEXT("MaterialExpressionSubstrateSingleLayerWaterBSDF"),         TEXT("SingleLayerWater") },
			{ TEXT("MaterialExpressionSubstrateLightFunction"),                TEXT("LightFunction") },
			{ TEXT("MaterialExpressionSubstratePostProcess"),                  TEXT("PostProcess") },
			{ TEXT("MaterialExpressionSubstrateUI"),                           TEXT("UI") },
			{ TEXT("MaterialExpressionSubstrateConvertMaterialAttributes"),    TEXT("ConvertMaterialAttributes") },
			{ TEXT("MaterialExpressionSubstrateConvertToDecal"),               TEXT("ConvertToDecal") },
			{ TEXT("MaterialExpressionSubstrateHorizontalMixing"),             TEXT("HorizontalMix") },
			{ TEXT("MaterialExpressionSubstrateHorizontalMixing"),             TEXT("HorizontalMixing") },
			{ TEXT("MaterialExpressionSubstrateVerticalLayering"),             TEXT("VerticalLayer") },
			{ TEXT("MaterialExpressionSubstrateVerticalLayering"),             TEXT("VerticalLayering") },
			{ TEXT("MaterialExpressionSubstrateAdd"),                          TEXT("Add") },
			{ TEXT("MaterialExpressionSubstrateWeight"),                       TEXT("Weight") },
			{ TEXT("MaterialExpressionSubstrateSelect"),                       TEXT("Select") },
			{ TEXT("MaterialExpressionSubstrateTransmittanceToMFP"),           TEXT("TransmittanceToMFP") },
			{ TEXT("MaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0"),   TEXT("MetalnessToDiffuseAlbedoF0") },
			{ TEXT("MaterialExpressionSubstrateHazinessToSecondaryRoughness"), TEXT("HazinessToSecondaryRoughness") },
			{ TEXT("MaterialExpressionSubstrateThinFilm"),                     TEXT("ThinFilm") },
		};

		/**
		 * The 1.x `UE.*` alias table, ported from the registered-builtin list in
		 * MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeUE.cpp (~416 onward).
		 *
		 * Every entry is a spelling that resolved to a class whose reflected short name is
		 * something else -- `UE.TexCoord` for TextureCoordinate, the Instance backend's suffix-less
		 * `UE.ObjectPosition` for ObjectPositionWS, `UE.ViewportUV` for the ScreenPosition node
		 * whose output 0 is the viewport UV. Spellings that already equal the short name
		 * (`UE.VertexColor`, `UE.PixelDepth`, ...) are not listed: they resolve on ShortName alone.
		 *
		 * `UE.TranslatedWorldPosition` is deliberately absent. In 1.x it was WorldPosition with
		 * WorldPositionShaderOffset forced to WPT_CameraRelative -- a class plus a property, not an
		 * alias -- and an alias here would silently give it the node's ABSOLUTE world position
		 * default. It is recorded in the report as a follow-up for the Substrate/sugar milestone
		 * (M7), where a spelling that carries a property belongs.
		 */
		struct FUECatalogAlias
		{
			const TCHAR* ClassName;
			const TCHAR* Alias;
		};

		const FUECatalogAlias UECatalogAliases[] =
		{
			{ TEXT("MaterialExpressionTextureCoordinate"),  TEXT("TexCoord") },
			{ TEXT("MaterialExpressionObjectPositionWS"),   TEXT("ObjectPosition") },
			{ TEXT("MaterialExpressionCameraVectorWS"),     TEXT("CameraVector") },
			{ TEXT("MaterialExpressionCameraPositionWS"),   TEXT("CameraPosition") },
			{ TEXT("MaterialExpressionReflectionVectorWS"), TEXT("ReflectionVector") },
			{ TEXT("MaterialExpressionScreenPosition"),     TEXT("ViewportUV") },
			{ TEXT("MaterialExpressionTransform"),          TEXT("TransformVector") },
			{ TEXT("MaterialExpressionLinearInterpolate"),  TEXT("Lerp") },
			{ TEXT("MaterialExpressionComponentMask"),      TEXT("Mask") },
			{ TEXT("MaterialExpressionMaterialFunctionCall"), TEXT("FunctionCall") },
		};

		/**
		 * The classes that take positional arguments, and in what order.
		 *
		 * Deliberately small. CONTRACT §6.10 makes "no positional parameters" the default and has S
		 * refuse a positional argument with DSH5220, so an entry missing here costs an author one
		 * `Name =` and an entry added here wrongly silently binds an argument to the wrong pin. 1.x
		 * itself accepted positional form for exactly two classes (Transform and TransformPosition,
		 * index 0 only); the rest below are the constructors and one-argument nodes where the
		 * positional reading is the only one anybody writes.
		 */
		struct FPositionalCatalogEntry
		{
			const TCHAR* ClassName;
			const TCHAR* Parameters[6];
		};

		const FPositionalCatalogEntry PositionalCatalogEntries[] =
		{
			{ TEXT("MaterialExpressionTextureCoordinate"), { TEXT("Index"), nullptr } },
			{ TEXT("MaterialExpressionConstant"),          { TEXT("R"), nullptr } },
			{ TEXT("MaterialExpressionConstant2Vector"),   { TEXT("R"), TEXT("G"), nullptr } },
			{ TEXT("MaterialExpressionConstant3Vector"),   { TEXT("Constant"), nullptr } },
			{ TEXT("MaterialExpressionConstant4Vector"),   { TEXT("Constant"), nullptr } },
			{ TEXT("MaterialExpressionTransform"),         { TEXT("Input"), TEXT("TransformSourceType"), TEXT("TransformType"), nullptr } },
			{ TEXT("MaterialExpressionTransformPosition"), { TEXT("Input"), TEXT("TransformSourceType"), TEXT("TransformType"), nullptr } },
			{ TEXT("MaterialExpressionPanner"),            { TEXT("Coordinate"), TEXT("Time"), TEXT("Speed"), nullptr } },
			{ TEXT("MaterialExpressionComponentMask"),     { TEXT("Input"), nullptr } },
			{ TEXT("MaterialExpressionTime"),              { TEXT("Period"), nullptr } },
		};

		/**
		 * Material attribute aliases, ported from ResolveMaterialProperty in
		 * MaterialAssetGeneration/DreamShaderMaterialValueParsing.cpp (~86).
		 *
		 * FMaterialAttributeDefinitionMap supplies the canonical names; these are the extra
		 * spellings 1.x accepted, which a source written against the 1.x front end still uses.
		 * `ClearCoat` and `ClearCoatRoughness` are aliases of CustomData0 / CustomData1 -- the
		 * engine has no separate attribute for them -- and the Mooa* spellings are the pre-rename
		 * Moon engine ones.
		 */
		struct FAttributeAlias
		{
			const TCHAR* PropertyName;
			const TCHAR* Alias;
		};

		const FAttributeAlias AttributeAliases[] =
		{
			{ TEXT("MP_MaterialAttributes"),   TEXT("Attributes") },
			{ TEXT("MP_EmissiveColor"),        TEXT("Emissive") },
			{ TEXT("MP_AmbientOcclusion"),     TEXT("AO") },
			{ TEXT("MP_WorldPositionOffset"),  TEXT("WPO") },
			{ TEXT("MP_PixelDepthOffset"),     TEXT("PDO") },
			{ TEXT("MP_CustomData0"),          TEXT("ClearCoat") },
			{ TEXT("MP_CustomData1"),          TEXT("ClearCoatRoughness") },
			{ TEXT("MP_CustomizedUVs0"),       TEXT("CustomizedUV0") },
			{ TEXT("MP_CustomizedUVs1"),       TEXT("CustomizedUV1") },
			{ TEXT("MP_CustomizedUVs2"),       TEXT("CustomizedUV2") },
			{ TEXT("MP_CustomizedUVs3"),       TEXT("CustomizedUV3") },
			{ TEXT("MP_CustomizedUVs4"),       TEXT("CustomizedUV4") },
			{ TEXT("MP_CustomizedUVs5"),       TEXT("CustomizedUV5") },
			{ TEXT("MP_CustomizedUVs6"),       TEXT("CustomizedUV6") },
			{ TEXT("MP_CustomizedUVs7"),       TEXT("CustomizedUV7") },
#if DREAMSHADER_WITH_MOON_ENGINE
			{ TEXT("MP_MoonEncodedAttribute0"), TEXT("MooaEncodedAttribute0") },
			{ TEXT("MP_MoonEncodedAttribute1"), TEXT("MooaEncodedAttribute1") },
			{ TEXT("MP_MoonEncodedAttribute2"), TEXT("MooaEncodedAttribute2") },
			{ TEXT("MP_MoonEncodedAttribute3"), TEXT("MooaEncodedAttribute3") },
			{ TEXT("MP_MoonEncodedAttribute4"), TEXT("MooaEncodedAttribute4") },
#endif
		};

		/**
		 * The argument spellings 1.x accepted for a pin or property whose engine name is something
		 * else (CONTRACT §6.13 #35).
		 *
		 * `UE.TexCoord(Index = 0)` is the spelling of every 1.x Graph line and of all three
		 * `Lang/Examples/*.dss`, and the engine property is `CoordinateIndex` -- so without these the
		 * examples do not bind at all. Ported from the `AllowedArguments` tables in
		 * `DreamShaderExpressionFactory.cpp` (~925 onward) and the named-argument handling in
		 * `DreamShaderMaterialGeneratorCodeUE.cpp`.
		 *
		 * Only the mappings no general rule below derives. `IgnorePause` -> `bIgnorePause` and
		 * `ShaderOffsets` -> `WorldPositionShaderOffset` are NOT here: the first falls out of the
		 * b-prefix rule and the second out of the DisplayName rule, both of which generalise to
		 * classes nobody has written a table row for.
		 */
		struct FMemberAlias
		{
			const TCHAR* ClassName;
			const TCHAR* MemberName;
			const TCHAR* Alias;
		};

		const FMemberAlias MemberAliases[] =
		{
			{ TEXT("MaterialExpressionTextureCoordinate"),  TEXT("CoordinateIndex"),     TEXT("Index") },
			{ TEXT("MaterialExpressionObjectPositionWS"),   TEXT("OriginType"),          TEXT("Origin") },
			{ TEXT("MaterialExpressionTransform"),          TEXT("TransformSourceType"), TEXT("Source") },
			{ TEXT("MaterialExpressionTransform"),          TEXT("TransformType"),       TEXT("Destination") },
			{ TEXT("MaterialExpressionTransformPosition"),  TEXT("TransformSourceType"), TEXT("Source") },
			{ TEXT("MaterialExpressionTransformPosition"),  TEXT("TransformType"),       TEXT("Destination") },
			{ TEXT("MaterialExpressionCollectionParameter"),TEXT("Collection"),          TEXT("Asset") },
			{ TEXT("MaterialExpressionCollectionParameter"),TEXT("ParameterName"),       TEXT("Parameter") },
			// The sugar the binder normalises `Tex.Sample(UV)` through; a hand-written
			// `UE.TextureSample(UV = ...)` should reach the same pin.
			{ TEXT("MaterialExpressionTextureSample"),      TEXT("Coordinates"),         TEXT("UV") },
		};

		/**
		 * Every spelling other than the engine name that should resolve to this member.
		 *
		 * Two general rules and one table. The b-prefix rule mirrors the second pass of 1.x's
		 * `FindMaterialExpressionArgumentProperty`, which strips a leading `b` from a bool property
		 * so an author writes `FractionalPart` rather than `bFractionalPart`. The DisplayName rule
		 * catches the properties the engine renames for the Details panel -- `WorldPositionShaderOffset`
		 * is shown as "Shader Offsets", which is where 1.x's `ShaderOffsets` argument came from.
		 */
		void CollectMemberAliases(
			const FString& ClassName,
			const FProperty* Property,
			const FString& MemberName,
			TArray<FString>& OutAliases)
		{
			OutAliases.Reset();

			if (CastField<FBoolProperty>(Property) != nullptr
				&& MemberName.Len() > 1
				&& MemberName[0] == TCHAR('b')
				&& FChar::IsUpper(MemberName[1]))
			{
				OutAliases.AddUnique(MemberName.RightChop(1));
			}

			static const FName DisplayNameKey(TEXT("DisplayName"));
			if (Property != nullptr && Property->HasMetaData(DisplayNameKey))
			{
				FString DisplayName = Property->GetMetaData(DisplayNameKey);
				DisplayName.ReplaceInline(TEXT(" "), TEXT(""));
				if (!DisplayName.IsEmpty() && !DisplayName.Equals(MemberName, ESearchCase::CaseSensitive))
				{
					OutAliases.AddUnique(DisplayName);
				}
			}

			for (const FMemberAlias& Alias : MemberAliases)
			{
				if (ClassName.Equals(Alias.ClassName, ESearchCase::CaseSensitive)
					&& MemberName.Equals(Alias.MemberName, ESearchCase::CaseSensitive))
				{
					OutAliases.AddUnique(Alias.Alias);
				}
			}
		}

		/**
		 * Drops any alias that collides with a real member name on the same class.
		 *
		 * A generated alias is a convenience; a real name is the contract. If a class ever grew both
		 * `Index` and `CoordinateIndex`, the alias has to lose, or one spelling would silently mean
		 * two different members depending on which entry the binder reached first.
		 */
		void PruneCollidingAliases(IR::FCatalogExpression& Entry)
		{
			TSet<FString> RealNames;
			for (const IR::FCatalogPin& Pin : Entry.Inputs)
			{
				RealNames.Add(Pin.Name);
			}
			for (const IR::FCatalogProperty& Property : Entry.Properties)
			{
				RealNames.Add(Property.Name);
			}

			const auto Prune = [&RealNames](TArray<FString>& Aliases)
			{
				Aliases.RemoveAll([&RealNames](const FString& Alias)
				{
					return RealNames.Contains(Alias);
				});
			};

			for (IR::FCatalogPin& Pin : Entry.Inputs)
			{
				Prune(Pin.Aliases);
			}
			for (IR::FCatalogProperty& Property : Entry.Properties)
			{
				Prune(Property.Aliases);
			}
		}

		/**
		 * Whether asking this class's CDO for a pin's value type is safe.
		 *
		 * Copied from the file-static CanQueryExpressionInputValueTypes in
		 * Workspace/DreamShaderWorkspaceService.cpp (~1408). Below 5.8,
		 * UMaterialExpressionAggregate::GetInputValueType dereferences state the CDO does not have
		 * and takes the editor down -- measured, not inferred: a Substrate project on 5.7 died on
		 * launch with the plugin enabled and started fine without it. The cost of skipping is that
		 * those pins keep the coarse `Numeric` type instead of the engine's own.
		 */
		bool CanQueryExpressionInputValueTypes(const UClass* Class)
		{
			if constexpr (DREAMSHADER_MATERIAL_AGGREGATE_HANDLES_SUBSTRATE)
			{
				return true;
			}
			else
			{
				static const UClass* AggregateExpressionClass =
					FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionAggregate"));
				return !AggregateExpressionClass || !Class->IsChildOf(AggregateExpressionClass);
			}
		}

		/**
		 * Whether this class answers value-type questions from per-instance state a CDO never has.
		 *
		 * Convert, Composite and PinBase build their pins from instance arrays (ConvertInputs /
		 * ConvertOutputs, ReroutePins) and NeuralNetworkOutput answers from its index-type setting;
		 * each override ends in checkNoEntry() or check(false) for an index those arrays lack, which on
		 * a CDO is every index. Measured, 09-14: UMaterialExpressionConvert::GetOutputValueType took
		 * the editor down on the first `.dss` compile of a test run. Found by scanning 5.8 for a check
		 * inside every GetInputValueType / GetOutputValueType override (37 overrides, 30 classes); the
		 * other 26 classes only assert past a fixed pin list, which the GetInput guard and the mask
		 * rule at the two call sites stay inside. The cost of skipping is the coarse `Numeric` type.
		 */
		bool HasInstanceDependentValueTypes(const UClass* Class)
		{
			static const TCHAR* const ClassPaths[] = {
				TEXT("/Script/Engine.MaterialExpressionConvert"),
				TEXT("/Script/Engine.MaterialExpressionComposite"),
				TEXT("/Script/Engine.MaterialExpressionPinBase"),
				TEXT("/Script/Engine.MaterialExpressionNeuralNetworkOutput"),
			};
			for (const TCHAR* ClassPath : ClassPaths)
			{
				const UClass* InstanceDependentClass = FindObject<UClass>(nullptr, ClassPath);
				if (InstanceDependentClass && Class->IsChildOf(InstanceDependentClass))
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Output widths the engine's own reflection does not report.
		 *
		 * `UMaterialExpression::GetOutputType` answers `MCT_Float` for ANY unmasked output, and only
		 * 56 of the ~276 expression classes override it -- so `TextureCoordinate` and
		 * `ReflectionVectorWS` both report "a float of some width" and land on `Numeric`, which
		 * TypeFromCatalogValueType reads as float1-that-broadcasts. `float2 UV = UE.TexCoord(0)` then
		 * binds as a float1 and S reports a type error on a line that is perfectly correct
		 * (CONTRACT §6.13 #32).
		 *
		 * The masked outputs need none of this: a mask IS the width, and the base class narrows by
		 * it already. This table is only for the unmasked ones the engine leaves at `MCT_Float`.
		 * Keyed by (class, output index) rather than by class, because a multi-output node can mix
		 * widths -- SceneTexture's Color is a masked float4 while its Size and InvSize are unmasked
		 * float2s.
		 *
		 * Seeded from the two places 1.x already knew these widths:
		 * `TryResolveKnownExpressionOutputComponentCount` (CodeShared.h, consulted directly below so
		 * a fix there is inherited) and the `OutputComponents` field of the `UE.*` builtin
		 * descriptors (CodeUE.cpp ~416+), which covered classes that helper did not.
		 */
		struct FKnownOutputWidth
		{
			const TCHAR* ClassName;
			int32 OutputIndex;
			int32 Components;
		};

		const FKnownOutputWidth KnownOutputWidths[] =
		{
			{ TEXT("MaterialExpressionTime"),                 0, 1 },
			{ TEXT("MaterialExpressionSceneDepth"),           0, 1 },
			{ TEXT("MaterialExpressionObjectRadius"),         0, 1 },
			{ TEXT("MaterialExpressionPerInstanceRandom"),    0, 1 },
			{ TEXT("MaterialExpressionPerInstanceFadeAmount"),0, 1 },
			{ TEXT("MaterialExpressionObjectBounds"),         0, 3 },
			{ TEXT("MaterialExpressionCameraPositionWS"),     0, 3 },
			{ TEXT("MaterialExpressionReflectionVectorWS"),   0, 3 },
			// SceneTexture output 0 ("Color") is masked RGBA and narrows on its own; 1 and 2 are the
			// unmasked Size / InvSize pair, which is the float2 the 1.x decompiler had to special-case.
			{ TEXT("MaterialExpressionSceneTexture"),         1, 2 },
			{ TEXT("MaterialExpressionSceneTexture"),         2, 2 },
		};

		bool TryGetKnownOutputWidth(const FString& ClassName, const int32 OutputIndex, int32& OutComponents)
		{
			for (const FKnownOutputWidth& Known : KnownOutputWidths)
			{
				if (Known.OutputIndex == OutputIndex && ClassName.Equals(Known.ClassName, ESearchCase::CaseSensitive))
				{
					OutComponents = Known.Components;
					return true;
				}
			}
			return false;
		}

		IR::ECatalogValueType FloatTypeForComponents(const int32 Components)
		{
			switch (Components)
			{
			case 1:  return IR::ECatalogValueType::Float1;
			case 2:  return IR::ECatalogValueType::Float2;
			case 3:  return IR::ECatalogValueType::Float3;
			case 4:  return IR::ECatalogValueType::Float4;
			default: return IR::ECatalogValueType::Numeric;
			}
		}

		/** EMaterialValueType -> the catalog's coarse vocabulary. */
		IR::ECatalogValueType TranslateMaterialValueType(const EMaterialValueType ValueType)
		{
			if (ValueType == MCT_MaterialAttributes)
			{
				return IR::ECatalogValueType::MaterialAttributes;
			}
			if (ValueType == MCT_StaticBool)
			{
				return IR::ECatalogValueType::StaticBool;
			}
			if (ValueType == MCT_Bool)
			{
				return IR::ECatalogValueType::Bool;
			}
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
			if (ValueType == MCT_Substrate)
			{
				return IR::ECatalogValueType::Substrate;
			}
#else
			if (ValueType == MCT_Strata)
			{
				return IR::ECatalogValueType::Substrate;
			}
#endif
			switch (ValueType)
			{
			case MCT_Texture:
			case MCT_Texture2D:
			case MCT_TextureCube:
			case MCT_Texture2DArray:
			case MCT_TextureExternal:
			case MCT_VolumeTexture:
				return IR::ECatalogValueType::Texture;
			case MCT_Float1:
			case MCT_LWCScalar:
				return IR::ECatalogValueType::Float1;
			case MCT_Float2:
			case MCT_LWCVector2:
				return IR::ECatalogValueType::Float2;
			case MCT_Float3:
			case MCT_LWCVector3:
				return IR::ECatalogValueType::Float3;
			case MCT_Float4:
			case MCT_LWCVector4:
				return IR::ECatalogValueType::Float4;
			case MCT_Float:
				// The base-class default: "any float1-4, promoted as needed". Numeric is exactly
				// that, and reporting Float1 instead would make every unoverridden pin reject a
				// float3 the graph accepts happily.
				return IR::ECatalogValueType::Numeric;
			default:
				break;
			}

			return IR::ECatalogValueType::Numeric;
		}

		/** The catalog's coarse vocabulary for a reflected literal property. */
		IR::ECatalogValueType TranslatePropertyType(const FProperty* Property, TArray<FString>& OutEnumValues)
		{
			OutEnumValues.Reset();
			if (!Property)
			{
				return IR::ECatalogValueType::Unknown;
			}

			const auto CollectEnum = [&OutEnumValues](const UEnum* Enum)
			{
				if (!Enum)
				{
					return;
				}
				for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
				{
					if (Enum->HasMetaData(TEXT("Hidden"), Index))
					{
						continue;
					}
					FString Name = Enum->GetNameStringByIndex(Index);
					// The enumerator spelling WITHOUT its prefix, which is what IR.h fixes as the
					// Enum property form ("SAMPLERTYPE_Normal" is written "Normal").
					int32 SeparatorIndex = INDEX_NONE;
					if (Name.FindChar(TCHAR('_'), SeparatorIndex))
					{
						Name.RightChopInline(SeparatorIndex + 1, DREAMSHADER_ALLOW_SHRINKING_NO);
					}
					if (!Name.IsEmpty() && !Name.Equals(TEXT("MAX"), ESearchCase::CaseSensitive))
					{
						OutEnumValues.AddUnique(Name);
					}
				}
			};

			if (CastField<FBoolProperty>(Property))
			{
				return IR::ECatalogValueType::Bool;
			}
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				CollectEnum(EnumProperty->GetEnum());
				return IR::ECatalogValueType::Enum;
			}
			if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				if (ByteProperty->Enum)
				{
					CollectEnum(ByteProperty->Enum);
					return IR::ECatalogValueType::Enum;
				}
				return IR::ECatalogValueType::Int;
			}
			if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
			{
				return NumericProperty->IsFloatingPoint() ? IR::ECatalogValueType::Float1 : IR::ECatalogValueType::Int;
			}
			if (CastField<FNameProperty>(Property))
			{
				return IR::ECatalogValueType::Name;
			}
			if (CastField<FStrProperty>(Property) || CastField<FTextProperty>(Property))
			{
				return IR::ECatalogValueType::String;
			}
			if (CastField<FObjectPropertyBase>(Property))
			{
				return IR::ECatalogValueType::Object;
			}

			return IR::ECatalogValueType::Unknown;
		}

		bool IsCatalogInputProperty(const FProperty* Property)
		{
			return Private::IsMaterialExpressionInputProperty(Property);
		}

		/**
		 * Whether a property belongs in the catalog at all.
		 *
		 * Same rule as the 1.x manifest's IsExportedMaterialExpressionProperty
		 * (Workspace/DreamShaderWorkspaceService.cpp ~111): editable, not deprecated, not transient
		 * -- plus the inputs, which are not CPF_Edit but are the whole point.
		 */
		bool IsCatalogProperty(const FProperty* Property)
		{
			if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient | CPF_DuplicateTransient))
			{
				return false;
			}

			return IsCatalogInputProperty(Property) || Property->HasAnyPropertyFlags(CPF_Edit);
		}

		/** `RequiredInput` metadata, defaulting to true -- the engine's own rule in UMaterialExpression::IsInputConnectionRequired. */
		bool IsInputRequired(const FProperty* Property)
		{
			static const FName RequiredInputMetaData(TEXT("RequiredInput"));
			if (Property && Property->HasMetaData(RequiredInputMetaData))
			{
				return Property->GetBoolMetaData(RequiredInputMetaData);
			}
			return true;
		}

		FString MakeInputPinName(const FProperty* Property, const int32 ArrayIndex)
		{
			// One pin per array element for the handful of classes that declare their inputs as C
			// arrays (QualitySwitch, FeatureLevelSwitch, ShadingPathSwitch, MakeMaterialAttributes'
			// CustomizedUVs[8]); the emitter parses the `[i]` back off.
			if (Property->ArrayDim <= 1)
			{
				return Property->GetName();
			}
			return FString::Printf(TEXT("%s[%d]"), *Property->GetName(), ArrayIndex); /* I18N-EXEMPT: pin identifier, not display text */
		}
	}

	FString GetMaterialExpressionCatalogShortName(const UClass* Class)
	{
		if (!Class)
		{
			return FString();
		}

		FString Name = Class->GetName();
		Name.RemoveFromStart(TEXT("U"), ESearchCase::CaseSensitive);
		Name.RemoveFromStart(TEXT("MaterialExpression"), ESearchCase::CaseSensitive);
		return Name;
	}

	const TCHAR* GetMaterialExpressionCatalogNamespace(const UClass* Class)
	{
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		if (Class
			&& (Class->IsChildOf(UMaterialExpressionSubstrateBSDF::StaticClass())
				|| Class->IsChildOf(UMaterialExpressionSubstrateUtilityBase::StaticClass())))
		{
			return TEXT("Substrate");
		}
#else
		(void)Class;
#endif
		return TEXT("UE");
	}

	void BuildBuiltinCatalogFromReflection(IR::FBuiltinCatalog& OutCatalog)
	{
		OutCatalog = IR::FBuiltinCatalog{};
		OutCatalog.Source = TEXT("reflection");
		OutCatalog.EngineVersion = FString::Printf(
			TEXT("%d.%d"), /* I18N-EXEMPT: version string, not display text */
			FEngineVersion::Current().GetMajor(),
			FEngineVersion::Current().GetMinor());

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class
				|| !Class->IsChildOf(UMaterialExpression::StaticClass())
				|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			const FString ShortName = GetMaterialExpressionCatalogShortName(Class);
			if (ShortName.IsEmpty())
			{
				continue;
			}

			IR::FCatalogExpression Entry;
			Entry.Namespace = GetMaterialExpressionCatalogNamespace(Class);
			Entry.ShortName = ShortName;
			Entry.ClassName = Class->GetName();
			Entry.ClassPathName = Class->GetPathName();
			Entry.bIsAbstract = false;

			// Non-const: GetInputValueType / GetOutputValueType are non-const virtuals.
			UMaterialExpression* DefaultExpression = Cast<UMaterialExpression>(Class->GetDefaultObject(false));
			const bool bCanQueryValueTypes =
				DefaultExpression != nullptr && CanQueryExpressionInputValueTypes(Class) && !HasInstanceDependentValueTypes(Class);

			Entry.bIsCustomOutput = Class->IsChildOf(UMaterialExpressionCustomOutput::StaticClass());
			Entry.bIsParameter = Class->IsChildOf(UMaterialExpressionParameter::StaticClass())
				|| (DefaultExpression && DefaultExpression->HasAParameterName());

			// Inputs and properties in one pass, in TFieldIterator order -- which is also the order
			// UMaterialExpression::GetInput counts pins in, so the Nth input pin here is the Nth pin
			// the engine reports.
			int32 NextInputIndex = 0;
			for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (!IsCatalogProperty(Property))
				{
					continue;
				}

				if (IsCatalogInputProperty(Property))
				{
					for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
					{
						IR::FCatalogPin Pin;
						Pin.Name = MakeInputPinName(Property, ArrayIndex);
						Pin.bRequired = IsInputRequired(Property);
						Pin.Type = IR::ECatalogValueType::Numeric;
						// Only where the engine itself asks: UMaterialGraphNode::CreateInputPins walks GetInput until
						// it answers null and queries the type of those pins alone. Past that, the fixed-list overrides
						// (IfThenElse, the Substrate BSDFs, the custom outputs) end in check(false).
						if (bCanQueryValueTypes && DefaultExpression->GetInput(NextInputIndex) != nullptr)
						{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
							Pin.Type = TranslateMaterialValueType(DefaultExpression->GetInputValueType(NextInputIndex));
#else
							PRAGMA_DISABLE_DEPRECATION_WARNINGS
							Pin.Type = TranslateMaterialValueType(static_cast<EMaterialValueType>(DefaultExpression->GetInputType(NextInputIndex)));
							PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
						}

						// The `Const*` twin: Multiply's A pin has a ConstA property that decides what
						// an unconnected pin means. §6.10 lets the IR builder prefer the property
						// when the value is constant, which is what 1.x emits.
						const FString ConstName = FString::Printf(TEXT("Const%s"), *Property->GetName()); /* I18N-EXEMPT: reflected property name */
						if (Property->ArrayDim <= 1 && Class->FindPropertyByName(FName(*ConstName)) != nullptr)
						{
							Pin.ConstPropertyName = ConstName;
						}

						// Aliases go on the base name, not the `[i]` spelling: an indexed pin is one
						// property, and an alias for element 3 alone would mean nothing.
						if (Property->ArrayDim <= 1)
						{
							CollectMemberAliases(Entry.ClassName, Property, Pin.Name, Pin.Aliases);
						}

						Entry.Inputs.Add(MoveTemp(Pin));
						++NextInputIndex;
					}
					continue;
				}

				IR::FCatalogProperty CatalogProperty;
				CatalogProperty.Name = Property->GetName();
				CatalogProperty.Type = TranslatePropertyType(Property, CatalogProperty.EnumValues);
				CollectMemberAliases(Entry.ClassName, Property, CatalogProperty.Name, CatalogProperty.Aliases);
				if (DefaultExpression)
				{
					Property->ExportTextItem_InContainer(CatalogProperty.DefaultText, DefaultExpression, nullptr, nullptr, PPF_None);
				}
				Entry.Properties.Add(MoveTemp(CatalogProperty));
			}

			// Outputs off the CDO's own Outputs array rather than GetOutputs(), which some classes
			// rebuild from state a CDO does not have (Custom's AdditionalOutputs, for one).
			if (DefaultExpression)
			{
				for (int32 OutputIndex = 0; OutputIndex < DefaultExpression->Outputs.Num(); ++OutputIndex)
				{
					const FExpressionOutput& Output = DefaultExpression->Outputs[OutputIndex];
					IR::FCatalogPin Pin;
					// FName::ToString answers the text None for an unnamed output, and the channel naming below and
					// IRCatalog.h both spell an unnamed output as the EMPTY name.
					Pin.Name = Output.OutputName.IsNone() ? FString() : Output.OutputName.ToString();
					Pin.Type = IR::ECatalogValueType::Numeric;
					// A masked output's width is its mask (below), so asking would only produce an answer that gets
					// overruled -- one fewer engine virtual called on a CDO for nothing. The classes whose output
					// list is rebuilt from instance state are excluded from bCanQueryValueTypes; for every other
					// asserting override (the Substrate helpers, SparseVolumeTextureSample) the constructor's
					// Outputs are exactly the indices its switch answers, checked against 5.8 on 09-14.
					const bool bMaskedOutput = Output.Mask != 0 && (Output.MaskR || Output.MaskG || Output.MaskB || Output.MaskA);
					if (bCanQueryValueTypes && !bMaskedOutput)
					{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
						Pin.Type = TranslateMaterialValueType(DefaultExpression->GetOutputValueType(OutputIndex));
#else
						PRAGMA_DISABLE_DEPRECATION_WARNINGS
						Pin.Type = TranslateMaterialValueType(static_cast<EMaterialValueType>(DefaultExpression->GetOutputType(OutputIndex)));
						PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
					}

					// A masked output publishes a component view of the value; the mask IS the width,
					// and it beats anything GetOutputValueType said.
					const int32 MaskCount =
						(Output.MaskR ? 1 : 0) + (Output.MaskG ? 1 : 0) + (Output.MaskB ? 1 : 0) + (Output.MaskA ? 1 : 0);
					if (Output.Mask != 0 && MaskCount > 0)
					{
						Pin.Type = FloatTypeForComponents(MaskCount);
					}
					else if (Pin.Type == IR::ECatalogValueType::Numeric || Pin.Type == IR::ECatalogValueType::Unknown)
					{
						// Unmasked and the engine would only say "a float": either the width really
						// does follow the inputs (Add, Multiply -- Numeric is the right answer) or the
						// class simply never overrode GetOutputType and the width is fixed and known.
						// Nothing in reflection tells those two apart, so the known-width tables do.
						int32 KnownComponents = 0;
						const bool bKnown =
							(DefaultExpression->Outputs.Num() == 1
								&& Private::TryResolveKnownExpressionOutputComponentCount(DefaultExpression, OutputIndex, KnownComponents)
								&& KnownComponents > 0)
							|| TryGetKnownOutputWidth(Entry.ClassName, OutputIndex, KnownComponents);
						if (bKnown)
						{
							Pin.Type = FloatTypeForComponents(KnownComponents);
						}
					}

					Entry.Outputs.Add(MoveTemp(Pin));
				}
			}

			// An unnamed masked output is a channel view, and it is named by the channels it keeps -- `RGB`,
			// `R`, `A` -- which is how the engine names the views it does name (TextureSample, ParticleColor,
			// SceneColor) and what 1.x's TryResolveExpressionOutputIndex answers to for the ones it does not
			// (VertexColor, DynamicParameter). Three things need that name. An author selecting the view:
			// `UE.VertexColor().A` has no other spelling. The binder: a node whose outputs are all channel
			// views is ONE value (1.x's TryRetargetChannelMaskToOutput rule), a node of different values is
			// not, and the name is the only place the catalog can say which. And the emitter's slot map,
			// which resolves an EMPTY name to output 0 whichever slot asked. A single output keeps its empty
			// name: it is the value, and IRCatalog.h spells it that way.
			if (DefaultExpression && Entry.Outputs.Num() > 1)
			{
				for (int32 OutputIndex = 0; OutputIndex < Entry.Outputs.Num() && OutputIndex < DefaultExpression->Outputs.Num(); ++OutputIndex)
				{
					const FExpressionOutput& Output = DefaultExpression->Outputs[OutputIndex];
					if (!Entry.Outputs[OutputIndex].Name.IsEmpty() || Output.Mask == 0)
					{
						continue;
					}

					FString ChannelName;
					if (Output.MaskR)
					{
						ChannelName.AppendChar(TCHAR('R'));
					}
					if (Output.MaskG)
					{
						ChannelName.AppendChar(TCHAR('G'));
					}
					if (Output.MaskB)
					{
						ChannelName.AppendChar(TCHAR('B'));
					}
					if (Output.MaskA)
					{
						ChannelName.AppendChar(TCHAR('A'));
					}

					// Never shadow a name the class already has. The emitter looks an output up by FName,
					// which ignores case, so a clash in case alone is a clash.
					const bool bNameTaken = ChannelName.IsEmpty()
						|| Entry.Outputs.ContainsByPredicate([&ChannelName](const IR::FCatalogPin& Existing)
						{
							return Existing.Name.Equals(ChannelName, ESearchCase::IgnoreCase);
						});
					if (!bNameTaken)
					{
						Entry.Outputs[OutputIndex].Name = ChannelName;
					}
				}
			}

			// A class with no output entry at all still has a default output in the graph; one
			// unnamed entry says so, and keeps FindOutput/Outputs[0] meaningful for every class.
			if (Entry.Outputs.IsEmpty() && !Entry.bIsCustomOutput)
			{
				IR::FCatalogPin Pin;
				Pin.Type = IR::ECatalogValueType::Numeric;
				Entry.Outputs.Add(MoveTemp(Pin));
			}

			// After both loops, so it sees every real pin AND property name of the class.
			PruneCollidingAliases(Entry);

			for (const FSubstrateCatalogAlias& Alias : SubstrateCatalogAliases)
			{
				if (Entry.ClassName.Equals(Alias.ClassName, ESearchCase::CaseSensitive)
					&& !Entry.ShortName.Equals(Alias.Alias, ESearchCase::CaseSensitive))
				{
					Entry.Aliases.AddUnique(Alias.Alias);
				}
			}
			for (const FUECatalogAlias& Alias : UECatalogAliases)
			{
				if (Entry.ClassName.Equals(Alias.ClassName, ESearchCase::CaseSensitive)
					&& !Entry.ShortName.Equals(Alias.Alias, ESearchCase::CaseSensitive))
				{
					Entry.Aliases.AddUnique(Alias.Alias);
				}
			}
			for (const FPositionalCatalogEntry& Positional : PositionalCatalogEntries)
			{
				if (!Entry.ClassName.Equals(Positional.ClassName, ESearchCase::CaseSensitive))
				{
					continue;
				}
				for (const TCHAR* Parameter : Positional.Parameters)
				{
					if (Parameter == nullptr)
					{
						break;
					}
					Entry.PositionalParameters.Add(Parameter);
				}
			}

			OutCatalog.Expressions.Add(MoveTemp(Entry));
		}

		// Sorted by class name, not by short name: two classes can share a short name only if they
		// share a class name, so this is the one key with no ties -- and unit P exports this as JSON
		// where a reordering would read as a diff on every export.
		OutCatalog.Expressions.Sort([](const IR::FCatalogExpression& Left, const IR::FCatalogExpression& Right)
		{
			return Left.ClassName.Compare(Right.ClassName, ESearchCase::CaseSensitive) < 0;
		});

		// ------------------------------------------------------------- material attributes

		const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
		TArray<TPair<FString, FGuid>> NameToIdList;
		FMaterialAttributeDefinitionMap::GetAttributeNameToIDList(NameToIdList);

		for (const TPair<FString, FGuid>& NameToId : NameToIdList)
		{
			const EMaterialProperty Property = FMaterialAttributeDefinitionMap::GetProperty(NameToId.Value);
			if (!PropertyEnum)
			{
				break;
			}

			const FString PropertyName = PropertyEnum->GetNameStringByValue(static_cast<int64>(Property));
			if (PropertyName.IsEmpty() || PropertyName.Equals(TEXT("MP_MAX"), ESearchCase::CaseSensitive))
			{
				// A custom output attribute registered through AddCustomAttribute has no
				// EMaterialProperty of its own; it is reachable as a custom-output node instead, so
				// it is not a `m.<Attribute>` the language can write.
				continue;
			}

			IR::FCatalogMaterialAttribute Attribute;
			Attribute.Name = NameToId.Key;
			Attribute.PropertyName = PropertyName;

			const EMaterialValueType ValueType = FMaterialAttributeDefinitionMap::GetValueType(NameToId.Value);
			switch (ValueType)
			{
			case MCT_Float:
			case MCT_Float1:
			case MCT_LWCScalar:
				Attribute.ValueType = IR::FIRType::Float(1);
				break;
			case MCT_Float2:
			case MCT_LWCVector2:
				Attribute.ValueType = IR::FIRType::Float(2);
				break;
			case MCT_Float3:
			case MCT_LWCVector3:
				Attribute.ValueType = IR::FIRType::Float(3);
				break;
			case MCT_Float4:
			case MCT_LWCVector4:
				Attribute.ValueType = IR::FIRType::Float(4);
				break;
			case MCT_MaterialAttributes:
				Attribute.ValueType = IR::FIRType::Material();
				break;
			default:
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
				Attribute.ValueType = (ValueType == MCT_Substrate) ? IR::FIRType::Substrate() : IR::FIRType::Float(1);
#else
				Attribute.ValueType = (ValueType == MCT_Strata) ? IR::FIRType::Substrate() : IR::FIRType::Float(1);
#endif
				break;
			}

			for (const FAttributeAlias& Alias : AttributeAliases)
			{
				if (Attribute.PropertyName.Equals(Alias.PropertyName, ESearchCase::CaseSensitive)
					&& !Attribute.Name.Equals(Alias.Alias, ESearchCase::CaseSensitive))
				{
					Attribute.Aliases.AddUnique(Alias.Alias);
				}
			}

			OutCatalog.MaterialAttributes.Add(MoveTemp(Attribute));
		}

		// MP_MaterialAttributes is not in the attribute table -- it is the whole set rather than one
		// of them -- but `m` itself and a MaterialAttributes pin are spelled with it, so it has to
		// be addressable.
		if (OutCatalog.FindMaterialAttribute(TEXT("MaterialAttributes")) == INDEX_NONE)
		{
			IR::FCatalogMaterialAttribute Attribute;
			Attribute.Name = TEXT("MaterialAttributes");
			Attribute.PropertyName = TEXT("MP_MaterialAttributes");
			Attribute.ValueType = IR::FIRType::Material();
			Attribute.Aliases.Add(TEXT("Attributes"));
			OutCatalog.MaterialAttributes.Add(MoveTemp(Attribute));
		}

		OutCatalog.MaterialAttributes.Sort([](const IR::FCatalogMaterialAttribute& Left, const IR::FCatalogMaterialAttribute& Right)
		{
			return Left.Name.Compare(Right.Name, ESearchCase::CaseSensitive) < 0;
		});

		if (OutCatalog.Expressions.IsEmpty())
		{
			// No DSHnnnn here on purpose. BuildBuiltinCatalogFromReflection takes no diagnostic sink
			// (its signature is frozen by CONTRACT 5) and a code that only ever appears inside a log
			// string is invisible to .skill/gen-diagnostics.ps1 -- it would exist in the binary and
			// in no document. The condition IS reported with a code: the pipeline asks
			// FBuiltinCatalog::IsEmpty() straight after this and raises DSH8297 into the sink, which
			// is the one a user sees. This line is the breadcrumb for a log read after the fact.
			UE_LOG(LogDreamShader, Warning,
				TEXT("Reflection produced no material expression classes; the DreamShader builtin catalog is empty."));
		}
	}
}
