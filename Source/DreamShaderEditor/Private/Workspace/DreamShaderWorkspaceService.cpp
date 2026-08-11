#include "DreamShaderWorkspaceService.h"

#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderEditorPersistenceUtils.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderVersionCompat.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Materials/MaterialExpression.h"
#include "MaterialValueType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"
#include "UObject/UObjectIterator.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		FString QuoteProcessArgument(const FString& Argument)
		{
			FString Escaped = Argument;
			Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
			return FString::Printf(TEXT("\"%s\""), *Escaped);
		}

		FString GetMaterialExpressionShortName(const UClass* Class)
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

		FString GetReflectedPropertyTypeName(const FProperty* Property)
		{
			if (!Property)
			{
				return TEXT("unknown");
			}

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (StructProperty->Struct && StructProperty->Struct->GetFName() == NAME_ExpressionInput)
				{
					return TEXT("input");
				}
				return StructProperty->Struct ? StructProperty->Struct->GetName() : TEXT("struct");
			}
			if (CastField<FBoolProperty>(Property))
			{
				return TEXT("bool");
			}
			if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
			{
				if (NumericProperty->IsFloatingPoint())
				{
					return TEXT("float");
				}
				if (NumericProperty->IsInteger())
				{
					return TEXT("int");
				}
				return TEXT("number");
			}
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				return EnumProperty->GetEnum() ? EnumProperty->GetEnum()->GetName() : TEXT("enum");
			}
			if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				return ByteProperty->Enum ? ByteProperty->Enum->GetName() : TEXT("byte");
			}
			if (CastField<FNameProperty>(Property))
			{
				return TEXT("name");
			}
			if (CastField<FStrProperty>(Property) || CastField<FTextProperty>(Property))
			{
				return TEXT("string");
			}
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				return ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetName() : TEXT("object");
			}
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				return FString::Printf(TEXT("array<%s>"), *GetReflectedPropertyTypeName(ArrayProperty->Inner));
			}

			return Property->GetCPPType();
		}

		bool IsExportedMaterialExpressionProperty(const FProperty* Property)
		{
			if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient | CPF_DuplicateTransient))
			{
				return false;
			}

			return UE::DreamShader::Editor::Private::IsMaterialExpressionInputProperty(Property)
				|| Property->HasAnyPropertyFlags(CPF_Edit);
		}

		struct FSubstrateBuiltinParameterManifestEntry
		{
			const TCHAR* Type = TEXT("value");
			const TCHAR* Name = TEXT("");
			const TCHAR* Placeholder = TEXT("");
		};

		struct FSubstrateBuiltinManifestEntry
		{
			const TCHAR* Name = TEXT("");
			const TCHAR* ClassName = TEXT("");
			const TCHAR* Detail = TEXT("");
			const TCHAR* Example = TEXT("");
			bool bIsSubstrateOutput = true;
			TArray<FSubstrateBuiltinParameterManifestEntry> Parameters;
		};

		struct FDreamShaderBridgeMappingEntry
		{
			FString Kind;
			FString Alias;
			int64 Value = 0;
			FString Name;
			FString DisplayName;
			FString Source;
		};

		struct FDreamShaderBridgeMaterialExpressionEntry
		{
			FString Name;
			FString ClassName;
			FString PathName;
			FString DefaultOutputType;
			FString JsonText;
		};

		struct FDreamShaderBridgeSubstrateBuiltinEntry
		{
			FString Name;
			FString QualifiedName;
			FString ClassName;
			FString OutputType;
			bool bIsSubstrateOutput = false;
			FString Detail;
			FString Example;
			FString Snippet;
			FString JsonText;
		};

		void AddParameterJson(TArray<TSharedPtr<FJsonValue>>& OutValues, const FSubstrateBuiltinParameterManifestEntry& Parameter)
		{
			TSharedRef<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
			ParameterObject->SetStringField(TEXT("qualifier"), TEXT("in"));
			ParameterObject->SetStringField(TEXT("type"), Parameter.Type);
			ParameterObject->SetStringField(TEXT("name"), Parameter.Name);
			if (FCString::Strlen(Parameter.Placeholder) > 0)
			{
				ParameterObject->SetStringField(TEXT("placeholder"), Parameter.Placeholder);
			}
			OutValues.Add(MakeShared<FJsonValueObject>(ParameterObject));
		}

		FString BuildSubstrateSnippet(const FSubstrateBuiltinManifestEntry& Builtin)
		{
			FString Snippet = FString::Printf(TEXT("Substrate.%s("), Builtin.Name);
			for (int32 Index = 0; Index < Builtin.Parameters.Num(); ++Index)
			{
				if (Index > 0)
				{
					Snippet += TEXT(", ");
				}

				const FSubstrateBuiltinParameterManifestEntry& Parameter = Builtin.Parameters[Index];
				const FString Placeholder = FCString::Strlen(Parameter.Placeholder) > 0
					? FString(Parameter.Placeholder)
					: FString(Parameter.Name);
				Snippet += FString::Printf(TEXT("%s=${%d:%s}"), Parameter.Name, Index + 1, *Placeholder);
			}
			Snippet += TEXT(")");
			return Snippet;
		}

		TArray<FSubstrateBuiltinManifestEntry> BuildSubstrateBuiltinManifestEntries()
		{
			return {
				{ TEXT("ShadingModels"), TEXT("MaterialExpressionSubstrateShadingModels"), TEXT("Creates a Substrate shading-model node."), TEXT("Substrate.ShadingModels()"), true, {} },
				{ TEXT("Slab"), TEXT("MaterialExpressionSubstrateSlabBSDF"), TEXT("Creates a Substrate slab BSDF."), TEXT("Substrate.Slab(DiffuseAlbedo=Color, F0=float3(0.04, 0.04, 0.04), Roughness=0.45)"), true, {
					{ TEXT("value"), TEXT("DiffuseAlbedo"), TEXT("Color") },
					{ TEXT("value"), TEXT("F0"), TEXT("float3(0.04, 0.04, 0.04)") },
					{ TEXT("value"), TEXT("Roughness"), TEXT("0.45") },
					{ TEXT("value"), TEXT("Normal"), TEXT("Normal") }
				} },
				{ TEXT("SimpleClearCoat"), TEXT("MaterialExpressionSubstrateSimpleClearCoatBSDF"), TEXT("Creates a Substrate simple clear coat BSDF."), TEXT("Substrate.SimpleClearCoat(DiffuseAlbedo=Color, F0=float3(0.04, 0.04, 0.04), Roughness=0.35)"), true, {
					{ TEXT("value"), TEXT("DiffuseAlbedo"), TEXT("Color") },
					{ TEXT("value"), TEXT("F0"), TEXT("float3(0.04, 0.04, 0.04)") },
					{ TEXT("value"), TEXT("Roughness"), TEXT("0.35") },
					{ TEXT("value"), TEXT("ClearCoatCoverage"), TEXT("1.0") },
					{ TEXT("value"), TEXT("ClearCoatRoughness"), TEXT("0.1") },
					{ TEXT("value"), TEXT("Normal"), TEXT("Normal") }
				} },
				{ TEXT("VolumetricFogCloud"), TEXT("MaterialExpressionSubstrateVolumetricFogCloudBSDF"), TEXT("Creates a Substrate volumetric fog/cloud BSDF."), TEXT("Substrate.VolumetricFogCloud(Albedo=Albedo, Extinction=Extinction, EmissiveColor=EmissiveColor)"), true, {
					{ TEXT("value"), TEXT("Albedo"), TEXT("Albedo") },
					{ TEXT("value"), TEXT("Extinction"), TEXT("Extinction") },
					{ TEXT("value"), TEXT("EmissiveColor"), TEXT("EmissiveColor") },
					{ TEXT("value"), TEXT("AmbientOcclusion"), TEXT("1.0") }
				} },
				{ TEXT("Unlit"), TEXT("MaterialExpressionSubstrateUnlitBSDF"), TEXT("Creates a Substrate unlit BSDF."), TEXT("Substrate.Unlit(EmissiveColor=Color)"), true, {
					{ TEXT("value"), TEXT("EmissiveColor"), TEXT("Color") }
				} },
				{ TEXT("Hair"), TEXT("MaterialExpressionSubstrateHairBSDF"), TEXT("Creates a Substrate hair BSDF."), TEXT("Substrate.Hair(BaseColor=Color, Roughness=0.35)"), true, {
					{ TEXT("value"), TEXT("BaseColor"), TEXT("Color") },
					{ TEXT("value"), TEXT("Scatter"), TEXT("0.0") },
					{ TEXT("value"), TEXT("Specular"), TEXT("0.5") },
					{ TEXT("value"), TEXT("Roughness"), TEXT("0.35") },
					{ TEXT("value"), TEXT("Backlit"), TEXT("0.0") },
					{ TEXT("value"), TEXT("Tangent"), TEXT("Tangent") },
					{ TEXT("value"), TEXT("EmissiveColor"), TEXT("EmissiveColor") }
				} },
				{ TEXT("Eye"), TEXT("MaterialExpressionSubstrateEyeBSDF"), TEXT("Creates a Substrate eye BSDF."), TEXT("Substrate.Eye(DiffuseColor=Color, Roughness=0.2)"), true, {
					{ TEXT("value"), TEXT("DiffuseColor"), TEXT("Color") },
					{ TEXT("value"), TEXT("Roughness"), TEXT("0.2") },
					{ TEXT("value"), TEXT("CorneaNormal"), TEXT("CorneaNormal") },
					{ TEXT("value"), TEXT("IrisNormal"), TEXT("IrisNormal") },
					{ TEXT("value"), TEXT("IrisPlaneNormal"), TEXT("IrisPlaneNormal") },
					{ TEXT("value"), TEXT("IrisMask"), TEXT("IrisMask") },
					{ TEXT("value"), TEXT("IrisDistance"), TEXT("IrisDistance") },
					{ TEXT("value"), TEXT("EmissiveColor"), TEXT("EmissiveColor") }
				} },
				{ TEXT("SingleLayerWater"), TEXT("MaterialExpressionSubstrateSingleLayerWaterBSDF"), TEXT("Creates a Substrate single-layer water BSDF."), TEXT("Substrate.SingleLayerWater(BaseColor=Color, Roughness=0.05)"), true, {
					{ TEXT("value"), TEXT("BaseColor"), TEXT("Color") },
					{ TEXT("value"), TEXT("Metallic"), TEXT("0.0") },
					{ TEXT("value"), TEXT("Specular"), TEXT("0.5") },
					{ TEXT("value"), TEXT("Roughness"), TEXT("0.05") },
					{ TEXT("value"), TEXT("Normal"), TEXT("Normal") },
					{ TEXT("value"), TEXT("EmissiveColor"), TEXT("EmissiveColor") },
					{ TEXT("value"), TEXT("TopMaterialOpacity"), TEXT("1.0") },
					{ TEXT("value"), TEXT("WaterAlbedo"), TEXT("WaterAlbedo") },
					{ TEXT("value"), TEXT("WaterExtinction"), TEXT("WaterExtinction") },
					{ TEXT("value"), TEXT("WaterPhaseG"), TEXT("WaterPhaseG") },
					{ TEXT("value"), TEXT("ColorScaleBehindWater"), TEXT("ColorScaleBehindWater") }
				} },
				{ TEXT("LightFunction"), TEXT("MaterialExpressionSubstrateLightFunction"), TEXT("Creates a Substrate light function material."), TEXT("Substrate.LightFunction(Color=Color)"), true, {
					{ TEXT("value"), TEXT("Color"), TEXT("Color") }
				} },
				{ TEXT("PostProcess"), TEXT("MaterialExpressionSubstratePostProcess"), TEXT("Creates a Substrate post-process material."), TEXT("Substrate.PostProcess(Color=Color, Opacity=1.0)"), true, {
					{ TEXT("value"), TEXT("Color"), TEXT("Color") },
					{ TEXT("value"), TEXT("Opacity"), TEXT("1.0") }
				} },
				{ TEXT("UI"), TEXT("MaterialExpressionSubstrateUI"), TEXT("Creates a Substrate UI material."), TEXT("Substrate.UI(Color=Color, Opacity=1.0)"), true, {
					{ TEXT("value"), TEXT("Color"), TEXT("Color") },
					{ TEXT("value"), TEXT("Opacity"), TEXT("1.0") }
				} },
				{ TEXT("ConvertMaterialAttributes"), TEXT("MaterialExpressionSubstrateConvertMaterialAttributes"), TEXT("Converts MaterialAttributes to a Substrate material."), TEXT("Substrate.ConvertMaterialAttributes(MaterialAttributes=Attrs)"), true, {
					{ TEXT("MaterialAttributes"), TEXT("MaterialAttributes"), TEXT("Attrs") },
					{ TEXT("value"), TEXT("WaterScatteringCoefficients"), TEXT("WaterScatteringCoefficients") },
					{ TEXT("value"), TEXT("WaterAbsorptionCoefficients"), TEXT("WaterAbsorptionCoefficients") },
					{ TEXT("value"), TEXT("WaterPhaseG"), TEXT("WaterPhaseG") },
					{ TEXT("value"), TEXT("ColorScaleBehindWater"), TEXT("ColorScaleBehindWater") }
				} },
				{ TEXT("ConvertToDecal"), TEXT("MaterialExpressionSubstrateConvertToDecal"), TEXT("Converts a Substrate material to decal output."), TEXT("Substrate.ConvertToDecal(DecalMaterial=Surface, Coverage=1.0)"), true, {
					{ TEXT("Substrate"), TEXT("DecalMaterial"), TEXT("Surface") },
					{ TEXT("value"), TEXT("Coverage"), TEXT("1.0") }
				} },
				{ TEXT("HorizontalMix"), TEXT("MaterialExpressionSubstrateHorizontalMixing"), TEXT("Horizontally mixes two Substrate materials."), TEXT("Substrate.HorizontalMix(Background=Background, Foreground=Foreground, Mix=Mix)"), true, {
					{ TEXT("Substrate"), TEXT("Background"), TEXT("Background") },
					{ TEXT("Substrate"), TEXT("Foreground"), TEXT("Foreground") },
					{ TEXT("value"), TEXT("Mix"), TEXT("Mix") }
				} },
				{ TEXT("HorizontalMixing"), TEXT("MaterialExpressionSubstrateHorizontalMixing"), TEXT("Alias of Substrate.HorizontalMix."), TEXT("Substrate.HorizontalMixing(Background=Background, Foreground=Foreground, Mix=Mix)"), true, {
					{ TEXT("Substrate"), TEXT("Background"), TEXT("Background") },
					{ TEXT("Substrate"), TEXT("Foreground"), TEXT("Foreground") },
					{ TEXT("value"), TEXT("Mix"), TEXT("Mix") }
				} },
				{ TEXT("VerticalLayer"), TEXT("MaterialExpressionSubstrateVerticalLayering"), TEXT("Layers one Substrate material over another."), TEXT("Substrate.VerticalLayer(Top=TopLayer, Base=BaseLayer, Thickness=0.01)"), true, {
					{ TEXT("Substrate"), TEXT("Top"), TEXT("Top") },
					{ TEXT("Substrate"), TEXT("Base"), TEXT("Base") },
					{ TEXT("value"), TEXT("Thickness"), TEXT("0.01") }
				} },
				{ TEXT("VerticalLayering"), TEXT("MaterialExpressionSubstrateVerticalLayering"), TEXT("Alias of Substrate.VerticalLayer."), TEXT("Substrate.VerticalLayering(Top=TopLayer, Base=BaseLayer, Thickness=0.01)"), true, {
					{ TEXT("Substrate"), TEXT("Top"), TEXT("Top") },
					{ TEXT("Substrate"), TEXT("Base"), TEXT("Base") },
					{ TEXT("value"), TEXT("Thickness"), TEXT("0.01") }
				} },
				{ TEXT("Add"), TEXT("MaterialExpressionSubstrateAdd"), TEXT("Adds two Substrate materials."), TEXT("Substrate.Add(A=A, B=B)"), true, {
					{ TEXT("Substrate"), TEXT("A"), TEXT("A") },
					{ TEXT("Substrate"), TEXT("B"), TEXT("B") }
				} },
				{ TEXT("Weight"), TEXT("MaterialExpressionSubstrateWeight"), TEXT("Weights a Substrate material."), TEXT("Substrate.Weight(A=Surface, Weight=1.0)"), true, {
					{ TEXT("Substrate"), TEXT("A"), TEXT("Surface") },
					{ TEXT("value"), TEXT("Weight"), TEXT("1.0") }
				} },
				{ TEXT("Select"), TEXT("MaterialExpressionSubstrateSelect"), TEXT("Selects between Substrate materials."), TEXT("Substrate.Select(A=A, B=B, SelectValue=SelectValue)"), true, {
					{ TEXT("Substrate"), TEXT("A"), TEXT("A") },
					{ TEXT("Substrate"), TEXT("B"), TEXT("B") },
					{ TEXT("value"), TEXT("SelectValue"), TEXT("SelectValue") }
				} },
				{ TEXT("TransmittanceToMFP"), TEXT("MaterialExpressionSubstrateTransmittanceToMFP"), TEXT("Converts transmittance to mean free path values."), TEXT("Substrate.TransmittanceToMFP(TransmittanceColor=Color, Thickness=1.0)"), false, {
					{ TEXT("value"), TEXT("TransmittanceColor"), TEXT("Color") },
					{ TEXT("value"), TEXT("Thickness"), TEXT("1.0") }
				} },
				{ TEXT("MetalnessToDiffuseAlbedoF0"), TEXT("MaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0"), TEXT("Converts metalness workflow values to diffuse albedo and F0."), TEXT("Substrate.MetalnessToDiffuseAlbedoF0(BaseColor=Color, Metallic=Metallic, Specular=0.5)"), false, {
					{ TEXT("value"), TEXT("BaseColor"), TEXT("Color") },
					{ TEXT("value"), TEXT("Metallic"), TEXT("Metallic") },
					{ TEXT("value"), TEXT("Specular"), TEXT("0.5") }
				} },
				{ TEXT("HazinessToSecondaryRoughness"), TEXT("MaterialExpressionSubstrateHazinessToSecondaryRoughness"), TEXT("Converts haziness to secondary roughness."), TEXT("Substrate.HazinessToSecondaryRoughness(BaseRoughness=Roughness, Haziness=0.0)"), false, {
					{ TEXT("value"), TEXT("BaseRoughness"), TEXT("Roughness") },
					{ TEXT("value"), TEXT("Haziness"), TEXT("0.0") }
				} },
				{ TEXT("ThinFilm"), TEXT("MaterialExpressionSubstrateThinFilm"), TEXT("Creates Substrate thin-film interference helper output."), TEXT("Substrate.ThinFilm(Thickness=500.0, IOR=1.4)"), false, {
					{ TEXT("value"), TEXT("Normal"), TEXT("Normal") },
					{ TEXT("value"), TEXT("F0"), TEXT("F0") },
					{ TEXT("value"), TEXT("F90"), TEXT("F90") },
					{ TEXT("value"), TEXT("Thickness"), TEXT("500.0") },
					{ TEXT("value"), TEXT("IOR"), TEXT("1.4") }
				} }
			};
		}

		int32 GetExpressionOutputComponentCount(const FExpressionOutput& Output)
		{
			const int32 MaskCount =
				(Output.MaskR ? 1 : 0)
				+ (Output.MaskG ? 1 : 0)
				+ (Output.MaskB ? 1 : 0)
				+ (Output.MaskA ? 1 : 0);
			return MaskCount > 0 ? MaskCount : 1;
		}

		FString GetOutputTypeNameFromComponentCount(const int32 ComponentCount)
		{
			if (ComponentCount <= 1)
			{
				return TEXT("float1");
			}
			if (ComponentCount == 2)
			{
				return TEXT("float2");
			}
			if (ComponentCount == 3)
			{
				return TEXT("float3");
			}
			return TEXT("float4");
		}

		// Translates the EMaterialValueType bitmask an input pin reports (via GetInputType /
		// GetInputValueType) into the same friendly type vocabulary used elsewhere in the manifest.
		// Most expressions never override this and inherit UMaterialExpression's base default of
		// MCT_Float ("any float1-4, auto-promoted"), which is reported as the generic "float" rather
		// than falling through to the uninformative "value" placeholder.
		FString GetFriendlyNameForMaterialInputValueType(const uint64 ValueTypeMask)
		{
			switch (ValueTypeMask)
			{
			case MCT_Float1: return TEXT("float1");
			case MCT_Float2: return TEXT("float2");
			case MCT_Float3: return TEXT("float3");
			case MCT_Float4: return TEXT("float4");
			case MCT_StaticBool:
			case MCT_Bool: return TEXT("bool");
			case MCT_Texture2D: return TEXT("Texture2D");
			case MCT_TextureCube: return TEXT("TextureCube");
			case MCT_Texture2DArray: return TEXT("Texture2DArray");
			case MCT_TextureCubeArray: return TEXT("TextureCubeArray");
			case MCT_VolumeTexture: return TEXT("VolumeTexture");
			case MCT_MaterialAttributes: return TEXT("MaterialAttributes");
			default:
				break;
			}

			if (ValueTypeMask != 0 && (ValueTypeMask & ~static_cast<uint64>(MCT_Float)) == 0)
			{
				return TEXT("float");
			}
			if (ValueTypeMask != 0 && (ValueTypeMask & ~static_cast<uint64>(MCT_Texture)) == 0)
			{
				return TEXT("Texture");
			}

			return TEXT("value");
		}

		template<typename EnumType>
		void AddSettingsMappingEntries(
			const FString& Kind,
			TArray<FDreamShaderBridgeMappingEntry>& OutEntries,
			const TMap<FString, TEnumAsByte<EnumType>>& Mappings,
			const UEnum* Enum,
			const FString& Source,
			TSet<FString>& InOutNormalizedAliases,
			const TSet<int64>* ExcludedEnumValues = nullptr)
		{
			TArray<FString> Aliases;
			Mappings.GetKeys(Aliases);
			Aliases.Sort([](const FString& Left, const FString& Right)
			{
				return Left < Right;
			});

			for (const FString& Alias : Aliases)
			{
				const TEnumAsByte<EnumType>* Value = Mappings.Find(Alias);
				if (!Value)
				{
					continue;
				}

				const int64 EnumValue = static_cast<int64>(Value->GetValue());
				if (ExcludedEnumValues && ExcludedEnumValues->Contains(EnumValue))
				{
					continue;
				}

				const FString NormalizedAlias = UDreamShaderSettings::NormalizeMappingKey(Alias);
				if (NormalizedAlias.IsEmpty() || InOutNormalizedAliases.Contains(NormalizedAlias))
				{
					continue;
				}

				InOutNormalizedAliases.Add(NormalizedAlias);

				FDreamShaderBridgeMappingEntry& Entry = OutEntries.AddDefaulted_GetRef();
				Entry.Kind = Kind;
				Entry.Alias = Alias;
				Entry.Value = EnumValue;
				Entry.Name = Enum ? Enum->GetNameStringByValue(EnumValue) : FString::FromInt(EnumValue);
				Entry.DisplayName = Enum ? Enum->GetDisplayNameTextByValue(EnumValue).ToString() : FString();
				Entry.Source = Source;
			}
		}

		TArray<TSharedPtr<FJsonValue>> BuildSettingsMappingJsonValues(
			const TArray<FDreamShaderBridgeMappingEntry>& Entries,
			const FString& Kind)
		{
			TArray<TSharedPtr<FJsonValue>> MappingValues;
			for (const FDreamShaderBridgeMappingEntry& Entry : Entries)
			{
				if (Entry.Kind != Kind)
				{
					continue;
				}

				TSharedRef<FJsonObject> MappingObject = MakeShared<FJsonObject>();
				MappingObject->SetStringField(TEXT("alias"), Entry.Alias);
				MappingObject->SetNumberField(TEXT("value"), static_cast<double>(Entry.Value));
				MappingObject->SetStringField(TEXT("name"), Entry.Name);
				MappingObject->SetStringField(TEXT("displayName"), Entry.DisplayName);
				MappingObject->SetStringField(TEXT("source"), Entry.Source);
				MappingValues.Add(MakeShared<FJsonValueObject>(MappingObject));
			}
			return MappingValues;
		}

		bool OpenBridgeDatabase(FSQLiteDatabase& Database)
		{
			const FString DatabasePath = FDreamShaderWorkspaceService::GetBridgeDatabaseFilePath();
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(DatabasePath), true);
			if (!Database.Open(*DatabasePath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
			{
				UE_LOG(LogDreamShader, Warning, TEXT("Failed to open DreamShader bridge database: %s"), *DatabasePath);
				return false;
			}

			Database.Execute(TEXT("PRAGMA journal_mode=WAL;"));
			Database.Execute(TEXT("PRAGMA synchronous=NORMAL;"));
			Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);"));
			Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS settings_mappings(kind TEXT NOT NULL, alias TEXT NOT NULL, normalized_alias TEXT NOT NULL, value INTEGER NOT NULL, name TEXT, display_name TEXT, source TEXT NOT NULL, PRIMARY KEY(kind, normalized_alias));"));
			Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS material_expressions(name TEXT PRIMARY KEY, class_name TEXT NOT NULL, path_name TEXT NOT NULL, default_output_type TEXT NOT NULL, json TEXT NOT NULL);"));
			Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS substrate_builtins(name TEXT PRIMARY KEY, qualified_name TEXT NOT NULL, class_name TEXT NOT NULL, output_type TEXT NOT NULL, is_substrate_output INTEGER NOT NULL, detail TEXT, example TEXT, snippet TEXT, json TEXT NOT NULL);"));
			Database.Execute(TEXT("CREATE TABLE IF NOT EXISTS diagnostics(path TEXT PRIMARY KEY, json TEXT NOT NULL, updated_at_utc TEXT NOT NULL);"));
			Database.Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_settings_mappings_kind_alias ON settings_mappings(kind, alias);"));
			return true;
		}

		void SetBridgeDatabaseMeta(FSQLiteDatabase& Database, const TCHAR* Key, const FString& Value)
		{
			FSQLitePreparedStatement Statement(Database, TEXT("INSERT OR REPLACE INTO meta(key, value) VALUES(?1, ?2);"));
			if (Statement.IsValid())
			{
				Statement.SetBindingValueByIndex(1, Key);
				Statement.SetBindingValueByIndex(2, Value);
				BindAndExecute(Statement);
			}
		}

		void WriteSettingsMappingsToBridgeDatabase(const TArray<FDreamShaderBridgeMappingEntry>& Entries)
		{
			FSQLiteDatabase Database;
			if (!OpenBridgeDatabase(Database))
			{
				return;
			}

			Database.Execute(TEXT("BEGIN TRANSACTION;"));
			Database.Execute(TEXT("DELETE FROM settings_mappings;"));
			{
				FSQLitePreparedStatement Statement(
					Database,
					TEXT("INSERT OR REPLACE INTO settings_mappings(kind, alias, normalized_alias, value, name, display_name, source) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7);"));
				if (Statement.IsValid())
				{
					for (const FDreamShaderBridgeMappingEntry& Entry : Entries)
					{
						Statement.SetBindingValueByIndex(1, Entry.Kind);
						Statement.SetBindingValueByIndex(2, Entry.Alias);
						Statement.SetBindingValueByIndex(3, UDreamShaderSettings::NormalizeMappingKey(Entry.Alias));
						Statement.SetBindingValueByIndex(4, Entry.Value);
						Statement.SetBindingValueByIndex(5, Entry.Name);
						Statement.SetBindingValueByIndex(6, Entry.DisplayName);
						Statement.SetBindingValueByIndex(7, Entry.Source);
						BindAndExecute(Statement);
					}
				}
			}
			SetBridgeDatabaseMeta(Database, TEXT("settings.generatedAt"), FDateTime::UtcNow().ToIso8601());
			Database.Execute(TEXT("COMMIT;"));
			Database.Close();
		}

		void WriteMaterialExpressionsToBridgeDatabase(
			const TArray<FDreamShaderBridgeMaterialExpressionEntry>& Entries)
		{
			FSQLiteDatabase Database;
			if (!OpenBridgeDatabase(Database))
			{
				return;
			}

			Database.Execute(TEXT("BEGIN TRANSACTION;"));
			Database.Execute(TEXT("DELETE FROM material_expressions;"));
			{
				FSQLitePreparedStatement Statement(
					Database,
					TEXT("INSERT OR REPLACE INTO material_expressions(name, class_name, path_name, default_output_type, json) VALUES(?1, ?2, ?3, ?4, ?5);"));
				if (Statement.IsValid())
				{
					for (const FDreamShaderBridgeMaterialExpressionEntry& Entry : Entries)
					{
						Statement.SetBindingValueByIndex(1, Entry.Name);
						Statement.SetBindingValueByIndex(2, Entry.ClassName);
						Statement.SetBindingValueByIndex(3, Entry.PathName);
						Statement.SetBindingValueByIndex(4, Entry.DefaultOutputType);
						Statement.SetBindingValueByIndex(5, Entry.JsonText);
						BindAndExecute(Statement);
					}
				}
			}
			SetBridgeDatabaseMeta(Database, TEXT("materialExpressions.generatedAt"), FDateTime::UtcNow().ToIso8601());
			Database.Execute(TEXT("COMMIT;"));
			Database.Close();
		}

		void WriteSubstrateBuiltinsToBridgeDatabase(const TArray<FDreamShaderBridgeSubstrateBuiltinEntry>& Entries)
		{
			FSQLiteDatabase Database;
			if (!OpenBridgeDatabase(Database))
			{
				return;
			}

			Database.Execute(TEXT("BEGIN TRANSACTION;"));
			Database.Execute(TEXT("DELETE FROM substrate_builtins;"));
			{
				FSQLitePreparedStatement Statement(
					Database,
					TEXT("INSERT OR REPLACE INTO substrate_builtins(name, qualified_name, class_name, output_type, is_substrate_output, detail, example, snippet, json) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);"));
				if (Statement.IsValid())
				{
					for (const FDreamShaderBridgeSubstrateBuiltinEntry& Entry : Entries)
					{
						Statement.SetBindingValueByIndex(1, Entry.Name);
						Statement.SetBindingValueByIndex(2, Entry.QualifiedName);
						Statement.SetBindingValueByIndex(3, Entry.ClassName);
						Statement.SetBindingValueByIndex(4, Entry.OutputType);
						Statement.SetBindingValueByIndex(5, Entry.bIsSubstrateOutput ? 1 : 0);
						Statement.SetBindingValueByIndex(6, Entry.Detail);
						Statement.SetBindingValueByIndex(7, Entry.Example);
						Statement.SetBindingValueByIndex(8, Entry.Snippet);
						Statement.SetBindingValueByIndex(9, Entry.JsonText);
						BindAndExecute(Statement);
					}
				}
			}
			SetBridgeDatabaseMeta(Database, TEXT("substrateBuiltins.generatedAt"), FDateTime::UtcNow().ToIso8601());
			Database.Execute(TEXT("COMMIT;"));
			Database.Close();
		}

		void AddExistingFileCandidate(TArray<FString>& OutCandidates, const FString& Candidate)
		{
			if (!Candidate.IsEmpty() && FPaths::FileExists(Candidate))
			{
				OutCandidates.AddUnique(UE::DreamShader::NormalizeSourceFilePath(Candidate));
			}
		}

		TArray<FString> FindVSCodeExecutableCandidates()
		{
			TArray<FString> Candidates;

			auto AddFromEnvironmentDirectory = [&Candidates](const TCHAR* VariableName, const TCHAR* RelativePath)
			{
				const FString Directory = FPlatformMisc::GetEnvironmentVariable(VariableName);
				if (!Directory.IsEmpty())
				{
					AddExistingFileCandidate(Candidates, FPaths::Combine(Directory, RelativePath));
				}
			};

			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code/Code.exe"));
			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code/bin/code.cmd"));
			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code Insiders/Code - Insiders.exe"));
			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code Insiders/bin/code-insiders.cmd"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles"), TEXT("Microsoft VS Code/Code.exe"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles"), TEXT("Microsoft VS Code/bin/code.cmd"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles(x86)"), TEXT("Microsoft VS Code/Code.exe"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles(x86)"), TEXT("Microsoft VS Code/bin/code.cmd"));

			const FString PathEnvironment = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
			TArray<FString> PathEntries;
			PathEnvironment.ParseIntoArray(PathEntries, TEXT(";"), true);
			for (FString PathEntry : PathEntries)
			{
				PathEntry.TrimStartAndEndInline();
				if (PathEntry.IsEmpty())
				{
					continue;
				}

				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("code.cmd")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("code.exe")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("Code.exe")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("code-insiders.cmd")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("Code - Insiders.exe")));
			}

			return Candidates;
		}
	}

	bool FDreamShaderEditorLaunchUtils::LaunchVSCodeWorkspace(const FString& WorkspaceFilePath)
	{
		for (const FString& Candidate : FindVSCodeExecutableCandidates())
		{
			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();

			FProcHandle ProcessHandle;
			if (Candidate.EndsWith(TEXT(".cmd"), ESearchCase::IgnoreCase)
				|| Candidate.EndsWith(TEXT(".bat"), ESearchCase::IgnoreCase))
			{
				FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
				if (CmdExe.IsEmpty())
				{
					CmdExe = TEXT("C:/Windows/System32/cmd.exe");
				}

				FString Parameters = FString::Printf(
					TEXT("/C \"\"%s\" %s %s\""),
					*Candidate,
					((Settings && !Settings->bOpenInNewWindow) ? TEXT(" --reuse-window") : TEXT("")),
					*QuoteProcessArgument(WorkspaceFilePath));
				ProcessHandle = FPlatformProcess::CreateProc(*CmdExe, *Parameters, true, true, true, nullptr, 0, nullptr, nullptr);
			}
			else
			{
				const FString Parameters = FString::Printf(
					TEXT("%s %s"),
					((Settings && !Settings->bOpenInNewWindow) ? TEXT(" --reuse-window") : TEXT("")),
					*QuoteProcessArgument(WorkspaceFilePath));
				ProcessHandle = FPlatformProcess::CreateProc(*Candidate, *Parameters, true, false, false, nullptr, 0, nullptr, nullptr);
			}

			if (ProcessHandle.IsValid())
			{
				FPlatformProcess::CloseProc(ProcessHandle);
				return true;
			}
		}

		return false;
	}

	bool FDreamShaderEditorLaunchUtils::LaunchVSCodeFile(const FString& FilePath, const int32 Line, const int32 Column)
	{
		const FString GotoArgument = FString::Printf(
			TEXT("%s:%d:%d"),
			*FilePath,
			FMath::Max(1, Line),
			FMath::Max(1, Column));

		for (const FString& Candidate : FindVSCodeExecutableCandidates())
		{
			FProcHandle ProcessHandle;
			if (Candidate.EndsWith(TEXT(".cmd"), ESearchCase::IgnoreCase)
				|| Candidate.EndsWith(TEXT(".bat"), ESearchCase::IgnoreCase))
			{
				FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
				if (CmdExe.IsEmpty())
				{
					CmdExe = TEXT("C:/Windows/System32/cmd.exe");
				}

				const FString Parameters = FString::Printf(
					TEXT("/C \"\"%s\" --reuse-window -g %s\""),
					*Candidate,
					*QuoteProcessArgument(GotoArgument));
				ProcessHandle = FPlatformProcess::CreateProc(*CmdExe, *Parameters, true, true, true, nullptr, 0, nullptr, nullptr);
			}
			else
			{
				const FString Parameters = FString::Printf(TEXT("--reuse-window -g %s"), *QuoteProcessArgument(GotoArgument));
				ProcessHandle = FPlatformProcess::CreateProc(*Candidate, *Parameters, true, false, false, nullptr, 0, nullptr, nullptr);
			}

			if (ProcessHandle.IsValid())
			{
				FPlatformProcess::CloseProc(ProcessHandle);
				return true;
			}
		}

		return false;
	}

	bool FDreamShaderEditorLaunchUtils::LaunchTextFileWithNotepad(const FString& FilePath)
	{
		TArray<FString> Candidates;
		const FString SystemRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot"));
		AddExistingFileCandidate(Candidates, FPaths::Combine(SystemRoot, TEXT("System32/notepad.exe")));
		Candidates.Add(TEXT("notepad.exe"));

		for (const FString& Candidate : Candidates)
		{
			const FString Parameters = QuoteProcessArgument(FilePath);
			FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*Candidate, *Parameters, true, false, false, nullptr, 0, nullptr, nullptr);
			if (ProcessHandle.IsValid())
			{
				FPlatformProcess::CloseProc(ProcessHandle);
				return true;
			}
		}

		return false;
	}

	bool FDreamShaderEditorLaunchUtils::LaunchTextFileInPreferredEditor(const FString& FilePath, const int32 Line, const int32 Column)
	{
		if (LaunchVSCodeFile(FilePath, Line, Column))
		{
			return true;
		}
		if (FPlatformProcess::LaunchFileInDefaultExternalApplication(*FilePath, nullptr, ELaunchVerb::Edit, false))
		{
			return true;
		}
		return LaunchTextFileWithNotepad(FilePath);
	}

	FString FDreamShaderWorkspaceService::GetMaterialExpressionManifestFilePath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader/Bridge/material-expressions.json"));
	}

	FString FDreamShaderWorkspaceService::GetDreamShaderSettingsManifestFilePath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader/Bridge/settings.json"));
	}

	FString FDreamShaderWorkspaceService::GetSubstrateBuiltinsManifestFilePath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader/Bridge/substrate-builtins.json"));
	}

	FString FDreamShaderWorkspaceService::GetBridgeDatabaseFilePath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader/Bridge/bridge.db"));
	}

	void FDreamShaderWorkspaceService::ResetBridgeDatabase()
	{
		const FString DatabasePath = GetBridgeDatabaseFilePath();
		IFileManager::Get().Delete(*DatabasePath, false, true, true);
		IFileManager::Get().Delete(*(DatabasePath + TEXT("-wal")), false, true, true);
		IFileManager::Get().Delete(*(DatabasePath + TEXT("-shm")), false, true, true);
		IFileManager::Get().DeleteDirectory(
			*FPaths::Combine(FPaths::GetPath(DatabasePath), TEXT("diagnostics")),
			false,
			true);
	}

	void FDreamShaderWorkspaceService::ExportSubstrateBuiltinsManifest()
	{
		const FString ManifestPath = GetSubstrateBuiltinsManifestFilePath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);

		TArray<TSharedPtr<FJsonValue>> BuiltinValues;
		TArray<FDreamShaderBridgeSubstrateBuiltinEntry> DatabaseEntries;
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		for (const FSubstrateBuiltinManifestEntry& Builtin : BuildSubstrateBuiltinManifestEntries())
		{
			TSharedRef<FJsonObject> BuiltinObject = MakeShared<FJsonObject>();
			const FString BuiltinName(Builtin.Name);
			const FString QualifiedName = FString::Printf(TEXT("Substrate.%s"), Builtin.Name);
			const FString OutputType = Builtin.bIsSubstrateOutput ? TEXT("Substrate") : TEXT("auto");
			const FString Snippet = BuildSubstrateSnippet(Builtin);
			BuiltinObject->SetStringField(TEXT("name"), BuiltinName);
			BuiltinObject->SetStringField(TEXT("qualifiedName"), QualifiedName);
			BuiltinObject->SetStringField(TEXT("className"), Builtin.ClassName);
			BuiltinObject->SetStringField(TEXT("outputType"), OutputType);
			BuiltinObject->SetBoolField(TEXT("isSubstrateOutput"), Builtin.bIsSubstrateOutput);
			BuiltinObject->SetStringField(TEXT("detail"), Builtin.Detail);
			BuiltinObject->SetStringField(TEXT("example"), Builtin.Example);
			BuiltinObject->SetStringField(TEXT("snippet"), Snippet);

			TArray<TSharedPtr<FJsonValue>> ParameterValues;
			for (const FSubstrateBuiltinParameterManifestEntry& Parameter : Builtin.Parameters)
			{
				AddParameterJson(ParameterValues, Parameter);
			}
			BuiltinObject->SetArrayField(TEXT("parameters"), ParameterValues);
			BuiltinValues.Add(MakeShared<FJsonValueObject>(BuiltinObject));

			FDreamShaderBridgeSubstrateBuiltinEntry& DatabaseEntry = DatabaseEntries.AddDefaulted_GetRef();
			DatabaseEntry.Name = BuiltinName;
			DatabaseEntry.QualifiedName = QualifiedName;
			DatabaseEntry.ClassName = Builtin.ClassName;
			DatabaseEntry.OutputType = OutputType;
			DatabaseEntry.bIsSubstrateOutput = Builtin.bIsSubstrateOutput;
			DatabaseEntry.Detail = Builtin.Detail;
			DatabaseEntry.Example = Builtin.Example;
			DatabaseEntry.Snippet = Snippet;
			SerializeJsonObject(BuiltinObject, DatabaseEntry.JsonText);
		}
#endif

		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetStringField(TEXT("schema"), TEXT("DreamShader.SubstrateBuiltins"));
		RootObject->SetNumberField(TEXT("version"), 1);
		RootObject->SetStringField(TEXT("generatedAt"), FDateTime::UtcNow().ToIso8601());
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		RootObject->SetBoolField(TEXT("supported"), true);
#else
		RootObject->SetBoolField(TEXT("supported"), false);
		RootObject->SetStringField(TEXT("unsupportedReason"), TEXT("Substrate builtins require Unreal Engine 5.4 or newer."));
#endif
		RootObject->SetArrayField(TEXT("builtins"), BuiltinValues);

		FString ManifestText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ManifestText);
		FJsonSerializer::Serialize(RootObject, Writer);

		if (FFileHelper::SaveStringToFile(ManifestText, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDreamShader, Display, TEXT("Wrote DreamShader Substrate builtin manifest: %s"), *ManifestPath);
		}
		else
		{
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write DreamShader Substrate builtin manifest: %s"), *ManifestPath);
		}

		WriteSubstrateBuiltinsToBridgeDatabase(DatabaseEntries);
	}

	void FDreamShaderWorkspaceService::ExportDreamShaderSettingsManifest()
	{
		const FString ManifestPath = GetDreamShaderSettingsManifestFilePath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);

		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		if (!Settings)
		{
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to read DreamShader settings for Bridge manifest."));
			return;
		}

		TSet<int64> ExcludedShadingModels;
#if !DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		ExcludedShadingModels.Add(static_cast<int64>(MSM_Strata));
#endif
		TArray<FDreamShaderBridgeMappingEntry> MappingEntries;
		TSet<FString> ShadingModelAliases;
		TSet<FString> BlendModeAliases;
		TSet<FString> MaterialDomainAliases;
		AddSettingsMappingEntries(
			TEXT("ShadingModel"),
			MappingEntries,
			Settings->ShadingModelMappings,
			StaticEnum<EMaterialShadingModel>(),
			TEXT("user"),
			ShadingModelAliases,
			&ExcludedShadingModels);
		AddSettingsMappingEntries(
			TEXT("BlendMode"),
			MappingEntries,
			Settings->BlendModeMappings,
			StaticEnum<EBlendMode>(),
			TEXT("user"),
			BlendModeAliases);
		AddSettingsMappingEntries(
			TEXT("MaterialDomain"),
			MappingEntries,
			Settings->MaterialDomainMappings,
			StaticEnum<EMaterialDomain>(),
			TEXT("user"),
			MaterialDomainAliases);

		TMap<FString, TEnumAsByte<EMaterialShadingModel>> DefaultShadingModelMappings;
		TMap<FString, TEnumAsByte<EBlendMode>> DefaultBlendModeMappings;
		TMap<FString, TEnumAsByte<EMaterialDomain>> DefaultMaterialDomainMappings;
		UDreamShaderSettings::BuildDefaultShadingModelMappings(DefaultShadingModelMappings);
		UDreamShaderSettings::BuildDefaultBlendModeMappings(DefaultBlendModeMappings);
		UDreamShaderSettings::BuildDefaultMaterialDomainMappings(DefaultMaterialDomainMappings);
		AddSettingsMappingEntries(
			TEXT("ShadingModel"),
			MappingEntries,
			DefaultShadingModelMappings,
			StaticEnum<EMaterialShadingModel>(),
			TEXT("builtin"),
			ShadingModelAliases,
			&ExcludedShadingModels);
		AddSettingsMappingEntries(
			TEXT("BlendMode"),
			MappingEntries,
			DefaultBlendModeMappings,
			StaticEnum<EBlendMode>(),
			TEXT("builtin"),
			BlendModeAliases);
		AddSettingsMappingEntries(
			TEXT("MaterialDomain"),
			MappingEntries,
			DefaultMaterialDomainMappings,
			StaticEnum<EMaterialDomain>(),
			TEXT("builtin"),
			MaterialDomainAliases);

		TSharedRef<FJsonObject> MappingsObject = MakeShared<FJsonObject>();
		MappingsObject->SetArrayField(TEXT("ShadingModel"), BuildSettingsMappingJsonValues(MappingEntries, TEXT("ShadingModel")));
		MappingsObject->SetArrayField(TEXT("BlendMode"), BuildSettingsMappingJsonValues(MappingEntries, TEXT("BlendMode")));
		MappingsObject->SetArrayField(TEXT("MaterialDomain"), BuildSettingsMappingJsonValues(MappingEntries, TEXT("MaterialDomain")));

		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetStringField(TEXT("schema"), TEXT("DreamShader.Settings"));
		RootObject->SetNumberField(TEXT("version"), 1);
		RootObject->SetStringField(TEXT("generatedAt"), FDateTime::UtcNow().ToIso8601());
		RootObject->SetObjectField(TEXT("mappings"), MappingsObject);

		FString ManifestText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ManifestText);
		FJsonSerializer::Serialize(RootObject, Writer);

		if (FFileHelper::SaveStringToFile(ManifestText, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDreamShader, Display, TEXT("Wrote DreamShader settings manifest: %s"), *ManifestPath);
		}
		else
		{
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write DreamShader settings manifest: %s"), *ManifestPath);
		}

		WriteSettingsMappingsToBridgeDatabase(MappingEntries);
	}

	// Whether it is safe to ask this expression class's CDO for its input value types.
	//
	// UMaterialExpressionAggregate::GetInputValueType routes pin 0 through
	// UMaterialAggregate::GetMaterialAttributes(), a lazy static that builds one aggregate attribute
	// per material property. With Substrate enabled that list includes MP_FrontMaterial, whose value
	// type is MCT_Substrate -- and before UE 5.8 MaterialValueTypeToMaterialAggregateAttributeType has
	// no case for it and checkf(false)s on its default branch. The check fires inside the engine call,
	// so there is no return value to inspect and nothing to catch: the only way to survive it is not to
	// make the call. This manifest is exported during editor startup, which is why a Substrate project
	// on 5.7 died on launch with the plugin enabled and started fine without it.
	//
	// Cost of skipping: those inputs keep their reflected property type name instead of the engine's
	// value-type name, for one expression class, on engines that predate the fix.
	//
	// Looked up by path rather than StaticClass() because the class does not exist on every engine
	// version this plugin supports, and a null result correctly means "nothing to avoid".
	static bool CanQueryExpressionInputValueTypes(const UClass* Class)
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

	void FDreamShaderWorkspaceService::ExportMaterialExpressionManifest()
	{
		const FString ManifestPath = GetMaterialExpressionManifestFilePath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);

		TArray<TSharedPtr<FJsonValue>> ExpressionValues;
		TArray<FDreamShaderBridgeMaterialExpressionEntry> DatabaseEntries;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class
				|| !Class->IsChildOf(UMaterialExpression::StaticClass())
				|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			const FString ShortName = GetMaterialExpressionShortName(Class);
			if (ShortName.IsEmpty())
			{
				continue;
			}

			TSharedRef<FJsonObject> ExpressionObject = MakeShared<FJsonObject>();
			ExpressionObject->SetStringField(TEXT("name"), ShortName);
			ExpressionObject->SetStringField(TEXT("className"), Class->GetName());
			ExpressionObject->SetStringField(TEXT("pathName"), Class->GetPathName());

			// Non-const: GetInputType/GetInputValueType are non-const virtuals on UMaterialExpression.
			UMaterialExpression* DefaultExpression = Cast<UMaterialExpression>(Class->GetDefaultObject(false));

			// Asking some CDOs for an input type takes the editor down rather than returning something
			// we could validate, so the decision has to be made per class, before the call.
			const bool bCanQueryInputValueTypes = CanQueryExpressionInputValueTypes(Class);

			TArray<TSharedPtr<FJsonValue>> PropertyValues;
			TArray<TSharedPtr<FJsonValue>> InputValues;
			int32 NextInputIndex = 0;
			for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (!IsExportedMaterialExpressionProperty(Property))
				{
					continue;
				}

				const bool bIsInput = UE::DreamShader::Editor::Private::IsMaterialExpressionInputProperty(Property);
				FString PropertyType = GetReflectedPropertyTypeName(Property);
				if (bIsInput && DefaultExpression && bCanQueryInputValueTypes)
				{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
					const uint64 ValueTypeMask = static_cast<uint64>(DefaultExpression->GetInputValueType(NextInputIndex));
#else
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					const uint64 ValueTypeMask = static_cast<uint64>(DefaultExpression->GetInputType(NextInputIndex));
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
					PropertyType = GetFriendlyNameForMaterialInputValueType(ValueTypeMask);
					++NextInputIndex;
				}

				TSharedRef<FJsonObject> PropertyObject = MakeShared<FJsonObject>();
				PropertyObject->SetStringField(TEXT("name"), Property->GetName());
				PropertyObject->SetStringField(TEXT("type"), PropertyType);
				PropertyObject->SetBoolField(TEXT("isInput"), bIsInput);

				PropertyValues.Add(MakeShared<FJsonValueObject>(PropertyObject));
				if (bIsInput)
				{
					InputValues.Add(MakeShared<FJsonValueObject>(PropertyObject));
				}
			}
			ExpressionObject->SetArrayField(TEXT("properties"), PropertyValues);
			ExpressionObject->SetArrayField(TEXT("inputs"), InputValues);

			TArray<TSharedPtr<FJsonValue>> OutputValues;
			if (DefaultExpression)
			{
				for (int32 OutputIndex = 0; OutputIndex < DefaultExpression->Outputs.Num(); ++OutputIndex)
				{
					const FExpressionOutput& Output = DefaultExpression->Outputs[OutputIndex];
					const int32 ComponentCount = GetExpressionOutputComponentCount(Output);

					TSharedRef<FJsonObject> OutputObject = MakeShared<FJsonObject>();
					OutputObject->SetNumberField(TEXT("index"), OutputIndex);
					OutputObject->SetStringField(TEXT("name"), Output.OutputName.ToString());
					OutputObject->SetNumberField(TEXT("componentCount"), ComponentCount);
					OutputObject->SetStringField(TEXT("outputType"), GetOutputTypeNameFromComponentCount(ComponentCount));
					OutputValues.Add(MakeShared<FJsonValueObject>(OutputObject));
				}
			}
			ExpressionObject->SetArrayField(TEXT("outputs"), OutputValues);
			ExpressionObject->SetStringField(
				TEXT("defaultOutputType"),
				OutputValues.IsEmpty()
					? TEXT("float1")
					: OutputValues[0]->AsObject()->GetStringField(TEXT("outputType")));

			ExpressionValues.Add(MakeShared<FJsonValueObject>(ExpressionObject));

			FDreamShaderBridgeMaterialExpressionEntry& DatabaseEntry = DatabaseEntries.AddDefaulted_GetRef();
			DatabaseEntry.Name = ShortName;
			DatabaseEntry.ClassName = Class->GetName();
			DatabaseEntry.PathName = Class->GetPathName();
			DatabaseEntry.DefaultOutputType = ExpressionObject->GetStringField(TEXT("defaultOutputType"));
			SerializeJsonObject(ExpressionObject, DatabaseEntry.JsonText);
		}

		ExpressionValues.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
		{
			const TSharedPtr<FJsonObject> LeftObject = Left.IsValid() ? Left->AsObject() : nullptr;
			const TSharedPtr<FJsonObject> RightObject = Right.IsValid() ? Right->AsObject() : nullptr;
			return LeftObject.IsValid()
				&& RightObject.IsValid()
				&& LeftObject->GetStringField(TEXT("name")) < RightObject->GetStringField(TEXT("name"));
		});

		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetStringField(TEXT("schema"), TEXT("DreamShader.MaterialExpressions"));
		RootObject->SetNumberField(TEXT("version"), 1);
		RootObject->SetStringField(TEXT("generatedAt"), FDateTime::UtcNow().ToIso8601());
		RootObject->SetArrayField(TEXT("expressions"), ExpressionValues);

		FString ManifestText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ManifestText);
		FJsonSerializer::Serialize(RootObject, Writer);

		if (FFileHelper::SaveStringToFile(ManifestText, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDreamShader, Display, TEXT("Wrote DreamShader MaterialExpression manifest: %s"), *ManifestPath);
		}
		else
		{
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write DreamShader MaterialExpression manifest: %s"), *ManifestPath);
		}

		WriteMaterialExpressionsToBridgeDatabase(DatabaseEntries);
	}

	bool FDreamShaderWorkspaceService::WriteDreamShaderWorkspaceFile(FString& OutWorkspaceFilePath, FString& OutError)
	{
		const FString SourceDirectory = UE::DreamShader::NormalizeSourceFilePath(UE::DreamShader::GetSourceShaderDirectory());
		if (SourceDirectory.IsEmpty())
		{
			OutError = TEXT("DreamShader source directory is empty.");
			return false;
		}

		if (!IFileManager::Get().MakeDirectory(*SourceDirectory, true))
		{
			OutError = FString::Printf(TEXT("Failed to create DreamShader source directory '%s'."), *SourceDirectory);
			return false;
		}

		const FString WorkspaceFilePath = UE::DreamShader::NormalizeSourceFilePath(
			FPaths::Combine(SourceDirectory, TEXT("DreamShader.code-workspace")));

		FString WorkspaceText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&WorkspaceText);
		Writer->WriteObjectStart();
		Writer->WriteArrayStart(TEXT("folders"));
		for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
		{
			FString FolderPath;
			if (Root.bIsProjectRoot)
			{
				// The workspace file lives in the project root, so this stays "." -- it keeps VSCode's
				// folder identity (and therefore per-folder settings) stable across the upgrade.
				FolderPath = TEXT(".");
			}
			else
			{
				FolderPath = Root.Directory;
				// A root on another drive -- an engine plugin, with the engine on a different volume --
				// has no relative form on Windows. An absolute path is still a valid workspace folder.
				if (!FPaths::MakePathRelativeTo(FolderPath, *(SourceDirectory + TEXT("/"))))
				{
					FolderPath = Root.Directory;
				}
			}

			Writer->WriteObjectStart();
			Writer->WriteValue(
				TEXT("name"),
				Root.bIsProjectRoot
					? FString(TEXT("DreamShader Source"))
					: FString::Printf(TEXT("Plugin: %s"), *Root.DisplayName));
			Writer->WriteValue(TEXT("path"), FolderPath);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectStart(TEXT("settings"));
		Writer->WriteObjectStart(TEXT("files.associations"));
		Writer->WriteValue(TEXT("*.dsm"), TEXT("dreamshaderlang"));
		Writer->WriteValue(TEXT("*.dsh"), TEXT("dreamshaderlang"));
		Writer->WriteValue(TEXT("*.dsf"), TEXT("dreamshaderlang"));
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
		Writer->Close();

		if (!FFileHelper::SaveStringToFile(WorkspaceText, *WorkspaceFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("Failed to write DreamShader workspace file '%s'."), *WorkspaceFilePath);
			return false;
		}

		OutWorkspaceFilePath = WorkspaceFilePath;
		return true;
	}
}
