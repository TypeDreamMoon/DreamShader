// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderThinCustomParameterOverrides.h for why a rebuild carries these values across.
//
// Two rules shape this file. First, kinds are addressed by INDEX, never by a hand-written list of
// enumerators: the set of parameter kinds grows between engine versions, and a list that goes out of
// date would drop a user's value without ever mentioning it. Second, the names to restore come from
// the instance's own override arrays rather than from the parent's cached parameter set -- the arrays
// are the override, and reading them needs no cached expression data to be current.

#include "DreamShaderThinCustomParameterOverrides.h"

#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderModule.h"

#include "Engine/Font.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/UnrealType.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		// Every parameter name the instance itself overrides, whatever kind it is.
		//
		// Read off the override arrays by reflection, the same sweep the divergence digest and the
		// Adopt gate use: the arrays ARE the set of overrides, so nothing about the parent has to be
		// current for this to be exact. The element struct's `ParameterInfo` field is the only part
		// this needs to understand, and every override struct in the engine has it.
		void GatherOverriddenParameterInfos(UMaterialInstance* Instance, TArray<FMaterialParameterInfo>& OutInfos)
		{
			for (TFieldIterator<FProperty> It(Instance->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*It);
				if (!ArrayProperty || !ArrayProperty->GetName().EndsWith(TEXT("ParameterValues"), ESearchCase::CaseSensitive))
				{
					continue;
				}

				const FStructProperty* ElementProperty = CastField<FStructProperty>(ArrayProperty->Inner);
				if (!ElementProperty || !ElementProperty->Struct)
				{
					continue;
				}

				const FStructProperty* InfoProperty =
					CastField<FStructProperty>(ElementProperty->Struct->FindPropertyByName(TEXT("ParameterInfo")));
				if (!InfoProperty || InfoProperty->Struct != FMaterialParameterInfo::StaticStruct())
				{
					continue;
				}

				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Instance));
				for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
				{
					if (const uint8* Element = ArrayHelper.GetRawPtr(ElementIndex))
					{
						OutInfos.AddUnique(*InfoProperty->ContainerPtrToValuePtr<FMaterialParameterInfo>(Element));
					}
				}
			}

			// The static parameters live in their own set rather than in a *ParameterValues array, so
			// the sweep above cannot see them -- and a static switch is exactly the kind of override
			// whose loss changes what the material COMPILES to, not merely what it looks like.
			const FStaticParameterSet StaticParameters = Instance->GetStaticParameters();
			for (const FStaticSwitchParameter& Switch : StaticParameters.StaticSwitchParameters)
			{
				OutInfos.AddUnique(Switch.ParameterInfo);
			}
			for (const FStaticComponentMaskParameter& Mask : StaticParameters.EditorOnly.StaticComponentMaskParameters)
			{
				OutInfos.AddUnique(Mask.ParameterInfo);
			}
		}

		// Whatever an override points at, so the capture can hold it against a GC that runs while the
		// instance is cleared. AsTextureObject covers the texture-ish kinds; a font parameter also
		// needs the UFont itself, because AsTextureObject hands back the page, not the font.
		void HoldOverrideReferencedObjects(const FMaterialParameterValue& Value, TArray<TStrongObjectPtr<UObject>>& InOutReferenced)
		{
			if (UObject* Referenced = Value.AsTextureObject())
			{
				InOutReferenced.Emplace(Referenced);
			}
			if (Value.Type == EMaterialParameterType::Font && Value.Font.Value)
			{
				// TStrongObjectPtr<UObject> only converts from UObject*, not from a derived pointer.
				UObject* FontObject = Value.Font.Value;
				InOutReferenced.Emplace(FontObject);
			}
		}

		// Write one captured value back onto the instance. False means "this build cannot express that
		// kind as an editor-only set", which the caller reports exactly like a parameter the source
		// dropped -- both end with the value gone, and both deserve to be said out loud.
		//
		// The static kinds go through the editor-only setters, which write into the instance's static
		// parameter set; the caller's UpdateStaticPermutation is what turns that into a shader map.
		bool ApplyCapturedOverride(UMaterialInstanceConstant* Instance, const FDreamShaderCapturedParameterOverride& Override)
		{
			const FMaterialParameterValue& Value = Override.Meta.Value;
			switch (Value.Type)
			{
			case EMaterialParameterType::Scalar:
				Instance->SetScalarParameterValueEditorOnly(Override.Info, Value.AsScalar());
				return true;
			case EMaterialParameterType::Vector:
				Instance->SetVectorParameterValueEditorOnly(Override.Info, Value.AsLinearColor());
				return true;
			case EMaterialParameterType::DoubleVector:
				Instance->SetDoubleVectorParameterValueEditorOnly(Override.Info, Value.AsVector4d());
				return true;
			case EMaterialParameterType::Texture:
				Instance->SetTextureParameterValueEditorOnly(Override.Info, Value.Texture);
				return true;
			case EMaterialParameterType::RuntimeVirtualTexture:
				Instance->SetRuntimeVirtualTextureParameterValueEditorOnly(Override.Info, Value.RuntimeVirtualTexture);
				return true;
			case EMaterialParameterType::SparseVolumeTexture:
				Instance->SetSparseVolumeTextureParameterValueEditorOnly(Override.Info, Value.SparseVolumeTexture);
				return true;
			case EMaterialParameterType::Font:
				Instance->SetFontParameterValueEditorOnly(Override.Info, Value.Font.Value, Value.Font.Page);
				return true;
			case EMaterialParameterType::StaticSwitch:
				Instance->SetStaticSwitchParameterValueEditorOnly(Override.Info, Value.AsStaticSwitch());
				return true;
			case EMaterialParameterType::StaticComponentMask:
				Instance->SetStaticComponentMaskParameterValueEditorOnly(Override.Info, Value.AsStaticComponentMask());
				return true;
			default:
				// Texture collections, parameter collections, and whatever a later engine adds. The
				// capture above still SEES them (it walks kinds by index), so they are named in the
				// report rather than vanishing quietly; deliberately not spelled out here, because
				// naming an enumerator that an older supported engine does not have would not compile.
				return false;
			}
		}
	}

	void CaptureThinCustomParameterOverrides(UMaterialInstance* Instance, FDreamShaderCapturedParameterOverrides& OutCaptured)
	{
		OutCaptured.Overrides.Reset();
		OutCaptured.ReferencedObjects.Reset();

		if (!Instance || !IsThinCustomInstancePair(Instance))
		{
			return;
		}

		TArray<FMaterialParameterInfo> OverriddenInfos;
		GatherOverriddenParameterInfos(Instance, OverriddenInfos);
		if (OverriddenInfos.Num() == 0)
		{
			return;
		}

		// One probe per (name, kind). GetParameterOverrideValue answers from the override arrays alone
		// and only for entries actually flagged as overrides, so this both classifies the name and
		// confirms it. A name that somehow exists under two kinds is captured under both.
		for (const FMaterialParameterInfo& Info : OverriddenInfos)
		{
			for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
			{
				const EMaterialParameterType Type = static_cast<EMaterialParameterType>(TypeIndex);
				FMaterialParameterMetadata Meta;
				if (!Instance->GetParameterOverrideValue(Type, Info, Meta))
				{
					continue;
				}

				FDreamShaderCapturedParameterOverride& Captured = OutCaptured.Overrides.AddDefaulted_GetRef();
				Captured.Type = Type;
				Captured.Info = Info;
				Captured.Meta = Meta;
				HoldOverrideReferencedObjects(Meta.Value, OutCaptured.ReferencedObjects);
			}
		}
	}

	void RestoreThinCustomParameterOverrides(
		UMaterialInstanceConstant* Instance,
		UMaterial* BaseMaterial,
		const FDreamShaderCapturedParameterOverrides& Captured,
		const FString& SourceFilePath)
	{
		if (!Instance || !BaseMaterial || Captured.IsEmpty())
		{
			return;
		}

		// What the rebuilt base declares now, asked once per kind rather than once per override. The
		// base has been through PostEditChange by this point, so its cached parameter set is the new
		// one -- which is the whole question: does the name the user tuned still exist?
		TMap<int32, TSet<FMaterialParameterInfo>> DeclaredByType;
		TArray<FString> DroppedNames;

		for (const FDreamShaderCapturedParameterOverride& Override : Captured.Overrides)
		{
			const int32 TypeIndex = static_cast<int32>(Override.Type);
			if (TypeIndex < 0 || TypeIndex >= NumMaterialParameterTypes)
			{
				continue;
			}

			TSet<FMaterialParameterInfo>* Declared = DeclaredByType.Find(TypeIndex);
			if (!Declared)
			{
				TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Parameters;
				BaseMaterial->GetAllParametersOfType(Override.Type, Parameters);
				Declared = &DeclaredByType.Add(TypeIndex);
				Declared->Reserve(Parameters.Num());
				for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Parameter : Parameters)
				{
					Declared->Add(Parameter.Key);
				}
			}

			if (!Declared->Contains(Override.Info) || !ApplyCapturedOverride(Instance, Override))
			{
				DroppedNames.AddUnique(Override.Info.Name.ToString());
			}
		}

		if (DroppedNames.Num() == 0)
		{
			return;
		}

		// One line for the whole rebuild, sorted, because a source that removes a parameter block
		// removes several at once and one line per name buries everything around it. A generation
		// warning rather than a bare log line: it joins the `Warnings:` block of the compile result,
		// so the Material Content Browser and the VSCode extension show it next to the source that
		// caused it instead of leaving it to the log.
		DroppedNames.Sort();
		RaiseGenerationWarning(TEXT("DSH8155"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("rebuilding '%s' from '%s' dropped %d parameter override(s) the rebuilt material no longer declares: %s."),
			*Instance->GetPathName(),
			*SourceFilePath,
			DroppedNames.Num(),
			*FString::Join(DroppedNames, TEXT(", "))));
	}
}
