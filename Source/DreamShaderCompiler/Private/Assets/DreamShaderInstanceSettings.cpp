// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderInstanceSettings.h.

#include "DreamShaderInstanceSettings.h"

#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderSettings.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DreamShader.InstanceSettings"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamShaderInstanceSettingsDetail
	{
		/** One material-instance property a key sets, and the flag that marks it overridden (null for a property with none). */
		struct FDreamShaderInstanceKeyRow
		{
			const TCHAR* Key;
			const TCHAR* PropertyName;
			const TCHAR* FlagName;
		};

		/**
		 * The table (research-instance section 3.1 (b)). Looked up by name at run time, so a row whose property this
		 * engine does not have (a fork's own override, a property a later version renames) simply is not a key here.
		 */
		const FDreamShaderInstanceKeyRow DreamShaderInstanceKeyRows[] =
		{
			{ TEXT("PhysMaterial"),           TEXT("PhysMaterial"),              TEXT("bOverridePhysMaterial") },
			{ TEXT("PhysMaterialMask"),       TEXT("PhysMaterialMask"),          nullptr },
			{ TEXT("SubsurfaceProfile"),      TEXT("SubsurfaceProfile"),         TEXT("bOverrideSubsurfaceProfile") },
			{ TEXT("SpecularProfile"),        TEXT("SpecularProfileOverride"),   TEXT("bOverrideSpecularProfile") },
			{ TEXT("ToonProfile"),            TEXT("ToonProfileOverride"),       TEXT("bOverrideToonProfile") },
			{ TEXT("MoonToonProfile"),        TEXT("MoonToonProfileOverride"),   TEXT("bOverrideMoonToonProfile") },
			{ TEXT("BlendableLocation"),      TEXT("BlendableLocationOverride"), TEXT("bOverrideBlendableLocation") },
			{ TEXT("BlendablePriority"),      TEXT("BlendablePriorityOverride"), TEXT("bOverrideBlendablePriority") },
			{ TEXT("NaniteOverrideMaterial"), TEXT("NaniteOverrideMaterial"),    nullptr },
		};

		/** A value field of FMaterialInstanceBasePropertyOverrides and the flag its EditCondition names. */
		struct FDreamShaderBaseOverrideField
		{
			FProperty* Value = nullptr;
			FBoolProperty* Flag = nullptr;
		};

		/** The key spelling of a base field: its name, without the `b` of a bool (`bIsThinSurface` is `IsThinSurface`). */
		FString MakeDreamShaderBaseOverrideKey(const FProperty* Value)
		{
			const FString Name = Value->GetName();
			if (CastField<FBoolProperty>(Value) && Name.Len() > 1 && Name[0] == TCHAR('b') && FChar::IsUpper(Name[1]))
			{
				return Name.RightChop(1);
			}
			return Name;
		}

		const TArray<FDreamShaderBaseOverrideField>& GetDreamShaderBaseOverrideFields()
		{
			static const TArray<FDreamShaderBaseOverrideField> Fields = []()
			{
				TArray<FDreamShaderBaseOverrideField> Result;
				const UScriptStruct* Struct = FMaterialInstanceBasePropertyOverrides::StaticStruct();
				static const FName EditConditionKey(TEXT("EditCondition"));
				for (TFieldIterator<FProperty> It(Struct); It; ++It)
				{
					FProperty* Property = *It;
					if (!Property || !Property->HasMetaData(EditConditionKey))
					{
						continue;
					}

					// A negated or compound condition names no single flag; every field of the struct names one today.
					const FString Condition = Property->GetMetaData(EditConditionKey).TrimStartAndEnd();
					FBoolProperty* Flag = CastField<FBoolProperty>(Struct->FindPropertyByName(FName(*Condition)));
					if (!Flag)
					{
						continue;
					}

					FDreamShaderBaseOverrideField& Field = Result.AddDefaulted_GetRef();
					Field.Value = Property;
					Field.Flag = Flag;
				}
				return Result;
			}();
			return Fields;
		}

		const FDreamShaderBaseOverrideField* FindDreamShaderBaseOverrideField(const FString& Key)
		{
			for (const FDreamShaderBaseOverrideField& Field : GetDreamShaderBaseOverrideFields())
			{
				if (MakeDreamShaderBaseOverrideKey(Field.Value).Equals(Key, ESearchCase::IgnoreCase)
					|| Field.Value->GetName().Equals(Key, ESearchCase::IgnoreCase))
				{
					return &Field;
				}
			}
			return nullptr;
		}

		const FDreamShaderInstanceKeyRow* FindDreamShaderInstanceKeyRow(const UClass* InstanceClass, const FString& Key)
		{
			for (const FDreamShaderInstanceKeyRow& Row : DreamShaderInstanceKeyRows)
			{
				if (Key.Equals(Row.Key, ESearchCase::IgnoreCase) || Key.Equals(Row.PropertyName, ESearchCase::IgnoreCase))
				{
					return InstanceClass->FindPropertyByName(Row.PropertyName) != nullptr ? &Row : nullptr;
				}
			}
			return nullptr;
		}

		/** Why a key is refused outright, or empty when it is not. */
		FText GetDreamShaderRefusedInstanceKeyReason(const FString& Key)
		{
			if (Key.Equals(TEXT("Backend"), ESearchCase::IgnoreCase))
			{
				return LOCTEXT("RefusedBackend", "an instance has no backend of its own; it is a plain material instance of its parent.");
			}
			if (Key.Equals(TEXT("UsageFlags"), ESearchCase::IgnoreCase))
			{
				return LOCTEXT("RefusedUsageFlags", "usage flags are a bitmask the engine merges with the parent's, which one key value cannot express.");
			}
			if (Key.Equals(TEXT("BasePropertyOverrides"), ESearchCase::IgnoreCase))
			{
				return LOCTEXT("RefusedBaseStruct", "write the overrides as keys of their own, such as BlendMode or TwoSided.");
			}
			if (Key.StartsWith(TEXT("bOverride"), ESearchCase::IgnoreCase))
			{
				return LOCTEXT("RefusedFlag", "an override flag follows from the key that sets its value.");
			}
			if (Key.EndsWith(TEXT("ParameterValues"), ESearchCase::IgnoreCase))
			{
				return LOCTEXT("RefusedParameterArray", "parameter values are written as uniform overrides.");
			}
			return FText::GetEmpty();
		}

		/** A property value in the spelling ApplyInstanceSettings reads: enums without their prefix, objects as paths or None. */
		FString ExportDreamShaderInstanceSettingValue(const FProperty* Property, const void* ValuePtr)
		{
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				const UObject* Object = ObjectProperty->GetObjectPropertyValue(ValuePtr);
				return Object ? Object->GetPathName() : FString(TEXT("None"));
			}
			if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
			{
				return BoolProperty->GetPropertyValue(ValuePtr) ? TEXT("true") : TEXT("false");
			}

			UEnum* Enum = nullptr;
			int64 EnumValue = 0;
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
				EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
			}
			else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
			{
				Enum = ByteProperty->Enum;
				EnumValue = ByteProperty->GetPropertyValue(ValuePtr);
			}
			if (Enum)
			{
				FString Name = Enum->GetNameStringByValue(EnumValue);
				int32 Underscore = INDEX_NONE;
				if (Name.FindChar(TCHAR('_'), Underscore) && Underscore + 1 < Name.Len())
				{
					Name.RightChopInline(Underscore + 1);
				}
				return Name;
			}

			FString Text;
			Property->ExportTextItem_Direct(Text, ValuePtr, nullptr, nullptr, PPF_None);
			return Text;
		}

		/** A base field's value from text, with the blend mode and the shading model through the project's own resolvers. */
		bool ApplyDreamShaderBaseOverrideValue(
			UMaterialInstanceConstant* Instance,
			const FDreamShaderBaseOverrideField& Field,
			const FString& ValueText,
			FMaterialInstanceBasePropertyOverrides& InOutBase,
			FDreamShaderError& OutError)
		{
			const FString Trimmed = ValueText.TrimStartAndEnd();
			const FName FieldName = Field.Value->GetFName();

			// The two settings DreamShaderSettings maps: one spelling means the same blend mode in a `#pragma material`
			// and in a `#pragma instance`.
			if (FieldName == GET_MEMBER_NAME_CHECKED(FMaterialInstanceBasePropertyOverrides, BlendMode))
			{
				EBlendMode BlendMode = BLEND_Opaque;
				const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
				if (!Settings || !Settings->TryResolveBlendMode(Trimmed, BlendMode))
				{
					OutError.Message = FString::Printf(TEXT("'%s' is not a blend mode."), *Trimmed); /* I18N-EXEMPT: wrapped by DSH8250 */
					return false;
				}
				InOutBase.BlendMode = BlendMode;
			}
			else if (FieldName == GET_MEMBER_NAME_CHECKED(FMaterialInstanceBasePropertyOverrides, ShadingModel))
			{
				EMaterialShadingModel ShadingModel = MSM_DefaultLit;
				const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
				if (!Settings || !Settings->TryResolveShadingModel(Trimmed, ShadingModel))
				{
					OutError.Message = FString::Printf(TEXT("'%s' is not a shading model."), *Trimmed); /* I18N-EXEMPT: wrapped by DSH8250 */
					return false;
				}
				InOutBase.ShadingModel = ShadingModel;
			}
			else
			{
				void* ValuePtr = Field.Value->ContainerPtrToValuePtr<void>(&InOutBase);
				if (!Private::SetMaterialExpressionLiteralProperty(Instance, Field.Value, ValuePtr, Trimmed, OutError))
				{
					return false;
				}
			}

			Field.Flag->SetPropertyValue_InContainer(&InOutBase, true);
			return true;
		}

		bool ApplyDreamShaderInstanceKeyRow(
			UMaterialInstanceConstant* Instance,
			const FDreamShaderInstanceKeyRow& Row,
			const FString& ValueText,
			FDreamShaderError& OutError)
		{
			FProperty* Property = Instance->GetClass()->FindPropertyByName(Row.PropertyName);
			if (!Property)
			{
				OutError.Message = FString::Printf(TEXT("this engine has no '%s'."), Row.PropertyName); /* I18N-EXEMPT: wrapped by DSH8250 */
				return false;
			}

			const FString Trimmed = ValueText.TrimStartAndEnd();
			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Instance);
			const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
			if (ObjectProperty && (Trimmed.IsEmpty() || Trimmed.Equals(TEXT("None"), ESearchCase::IgnoreCase)))
			{
				ObjectProperty->SetObjectPropertyValue(ValuePtr, nullptr);
			}
			else if (!Private::SetMaterialExpressionLiteralProperty(Instance, Property, ValuePtr, Trimmed, OutError))
			{
				return false;
			}

			if (Row.FlagName)
			{
				if (FBoolProperty* Flag = CastField<FBoolProperty>(Instance->GetClass()->FindPropertyByName(Row.FlagName)))
				{
					Flag->SetPropertyValue_InContainer(Instance, true);
				}
			}
			return true;
		}

		/** Every table row back to the class default, flag included: a key the file no longer names is no longer overridden. */
		void ResetDreamShaderInstanceKeyRows(UMaterialInstanceConstant* Instance)
		{
			const UObject* Defaults = Instance->GetClass()->GetDefaultObject();
			for (const FDreamShaderInstanceKeyRow& Row : DreamShaderInstanceKeyRows)
			{
				if (const FProperty* Property = Instance->GetClass()->FindPropertyByName(Row.PropertyName))
				{
					Property->CopyCompleteValue_InContainer(Instance, Defaults);
				}
				if (Row.FlagName)
				{
					if (const FProperty* Flag = Instance->GetClass()->FindPropertyByName(Row.FlagName))
					{
						Flag->CopyCompleteValue_InContainer(Instance, Defaults);
					}
				}
			}
		}
	}

	bool ApplyInstanceSettings(
		UMaterialInstanceConstant* Instance,
		const TArray<TPair<FString, FString>>& Settings,
		FMaterialInstanceBasePropertyOverrides& InOutBase,
		const IR::FIRSourceRef& Source,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamShaderInstanceSettingsDetail;

		if (!Instance)
		{
			return false;
		}

		ResetDreamShaderInstanceKeyRows(Instance);

		bool bAllApplied = true;
		for (const TPair<FString, FString>& Setting : Settings)
		{
			const FString Key = Setting.Key.TrimStartAndEnd();

			const FText RefusedReason = GetDreamShaderRefusedInstanceKeyReason(Key);
			if (!RefusedReason.IsEmpty())
			{
				Diagnostics.Error(TEXT("DSH8251"), Source.Span, FText::Format(
					LOCTEXT("RefusedKey", "'{0}' cannot be set from an instance file: {1}"),
					FText::FromString(Key),
					RefusedReason));
				bAllApplied = false;
				continue;
			}

			FDreamShaderError ValueError;
			bool bApplied = false;
			bool bKnown = false;
			if (const FDreamShaderBaseOverrideField* Field = FindDreamShaderBaseOverrideField(Key))
			{
				bKnown = true;
				bApplied = ApplyDreamShaderBaseOverrideValue(Instance, *Field, Setting.Value, InOutBase, ValueError);
			}
			else if (const FDreamShaderInstanceKeyRow* Row = FindDreamShaderInstanceKeyRow(Instance->GetClass(), Key))
			{
				bKnown = true;
				bApplied = ApplyDreamShaderInstanceKeyRow(Instance, *Row, Setting.Value, ValueError);
			}

			if (!bKnown)
			{
				Diagnostics.Error(TEXT("DSH8249"), Source.Span, FText::Format(
					LOCTEXT("UnknownKey", "'{0}' is not an instance key; a key is a material property an instance can override, such as BlendMode, TwoSided, OpacityMaskClipValue or PhysMaterial."),
					FText::FromString(Key)));
				bAllApplied = false;
				continue;
			}

			if (!bApplied)
			{
				Diagnostics.Error(TEXT("DSH8250"), Source.Span, FText::Format(
					LOCTEXT("BadKeyValue", "'{0}' is not a valid value for the instance key '{1}'. {2}"),
					FText::FromString(Setting.Value),
					FText::FromString(Key),
					FText::FromString(ValueError.HasCode()
						? FString::Printf(TEXT("%s: %s"), *ValueError.Code, *ValueError.Message) /* I18N-EXEMPT: quotes an asset-layer message verbatim */
						: ValueError.Message)));
				bAllApplied = false;
			}
		}

		return bAllApplied;
	}

	void ReadInstanceSettings(
		const UMaterialInstanceConstant* Instance,
		TArray<TPair<FString, FString>>& OutSettings,
		TArray<FString>& OutUnsupported)
	{
		using namespace DreamShaderInstanceSettingsDetail;

		OutSettings.Reset();
		OutUnsupported.Reset();
		if (!Instance)
		{
			return;
		}

		const FMaterialInstanceBasePropertyOverrides& Base = Instance->BasePropertyOverrides;
		for (const FDreamShaderBaseOverrideField& Field : GetDreamShaderBaseOverrideFields())
		{
			if (!Field.Flag->GetPropertyValue_InContainer(&Base))
			{
				continue;
			}
			OutSettings.Emplace(
				MakeDreamShaderBaseOverrideKey(Field.Value),
				ExportDreamShaderInstanceSettingValue(Field.Value, Field.Value->ContainerPtrToValuePtr<void>(&Base)));
		}
		if (Base.bOverride_UsageFlags != 0)
		{
			OutUnsupported.Add(TEXT("UsageFlags"));
		}

		const UObject* Defaults = Instance->GetClass()->GetDefaultObject();
		for (const FDreamShaderInstanceKeyRow& Row : DreamShaderInstanceKeyRows)
		{
			const FProperty* Property = Instance->GetClass()->FindPropertyByName(Row.PropertyName);
			if (!Property)
			{
				continue;
			}

			bool bOverridden = false;
			if (Row.FlagName)
			{
				const FBoolProperty* Flag = CastField<FBoolProperty>(Instance->GetClass()->FindPropertyByName(Row.FlagName));
				bOverridden = Flag && Flag->GetPropertyValue_InContainer(Instance);
			}
			else
			{
				// No flag: set means different from the class default.
				bOverridden = !Property->Identical_InContainer(Instance, Defaults);
			}

			if (bOverridden)
			{
				OutSettings.Emplace(Row.Key, ExportDreamShaderInstanceSettingValue(Property, Property->ContainerPtrToValuePtr<void>(Instance)));
			}
		}
	}

	const TArray<FString>& GetInstanceSettingKeys()
	{
		using namespace DreamShaderInstanceSettingsDetail;

		static const TArray<FString> Keys = []()
		{
			TArray<FString> Result;
			for (const FDreamShaderBaseOverrideField& Field : GetDreamShaderBaseOverrideFields())
			{
				Result.AddUnique(MakeDreamShaderBaseOverrideKey(Field.Value));
			}
			for (const FDreamShaderInstanceKeyRow& Row : DreamShaderInstanceKeyRows)
			{
				if (UMaterialInstanceConstant::StaticClass()->FindPropertyByName(Row.PropertyName))
				{
					Result.AddUnique(Row.Key);
				}
			}
			return Result;
		}();
		return Keys;
	}

	void AppendInstanceSettingsDigestLines(const UMaterialInstance* Instance, FString& InOutText)
	{
		using namespace DreamShaderInstanceSettingsDetail;

		if (!Instance)
		{
			return;
		}

		// Exported through the property rather than field by field: the struct grows between engine versions, and a
		// field the digest missed would be an override a user can hand-edit and silently lose.
		if (const FStructProperty* BaseProperty = CastField<FStructProperty>(
			UMaterialInstance::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMaterialInstance, BasePropertyOverrides))))
		{
			FString BaseText;
			BaseProperty->ExportTextItem_Direct(BaseText, BaseProperty->ContainerPtrToValuePtr<void>(Instance), nullptr, nullptr, PPF_None);
			InOutText += FString::Printf(TEXT("MI Base %s\n"), *BaseText); /* I18N-EXEMPT: digest material, never displayed */
		}

		for (const FDreamShaderInstanceKeyRow& Row : DreamShaderInstanceKeyRows)
		{
			const TCHAR* const Names[] = { Row.PropertyName, Row.FlagName };
			for (const TCHAR* Name : Names)
			{
				if (!Name)
				{
					continue;
				}
				const FProperty* Property = Instance->GetClass()->FindPropertyByName(Name);
				if (!Property)
				{
					continue;
				}
				FString ValueText;
				Property->ExportTextItem_Direct(ValueText, Property->ContainerPtrToValuePtr<void>(Instance), nullptr, nullptr, PPF_None);
				InOutText += FString::Printf(TEXT("MI Key %s=%s\n"), Name, *ValueText); /* I18N-EXEMPT: digest material, never displayed */
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
