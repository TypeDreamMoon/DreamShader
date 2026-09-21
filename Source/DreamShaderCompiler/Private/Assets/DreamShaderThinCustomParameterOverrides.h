// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The parameter overrides a user set on a generated ThinCustom instance, carried across a rebuild.
//
// The ThinCustom instance exists to be tuned: the graph lives on a hidden base UMaterial, and the
// instance is the thin, cheap surface a designer drags sliders on. A rebuild has to clear those
// overrides -- ClearParameterValuesEditorOnly is what makes the instance agree with a base whose
// parameter set may have changed -- and for a long time it simply destroyed them, which is why an
// override used to count as a hand edit and lock the .dsm out of rebuilding entirely.
//
// So: capture before the rebuild, put back after it. Restoration is by (kind, name), because a name
// is the only thing that survives a graph the generator tore down and rebuilt from scratch. A name
// the rebuilt base no longer declares cannot come back and is reported (DSH8155) rather than
// silently swallowed -- the source removed the parameter, and that is the source's decision.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderVersionCompat.h"

// FMaterialParameterInfo / FMaterialParameterMetadata / EMaterialParameterType: moved into
// Materials/MaterialParameters.h in UE 5.7; MaterialTypes.h before it (and a deprecation stub after),
// so neither spelling covers the whole supported range on its own.
#if DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER
#include "Materials/MaterialParameters.h"
#else
#include "MaterialTypes.h"
#endif

#include "UObject/StrongObjectPtr.h"

class UMaterial;
class UMaterialInstance;
class UMaterialInstanceConstant;

namespace UE::DreamShader::Editor::Private
{
	/** One override read off the instance: which kind of parameter, which name, and the value. */
	struct FDreamShaderCapturedParameterOverride
	{
		EMaterialParameterType Type = EMaterialParameterType::None;
		FMaterialParameterInfo Info;
		FMaterialParameterMetadata Meta;
	};

	/**
	 * Everything one instance overrode, plus strong references to whatever those overrides point at.
	 *
	 * The strong references are not decoration. Between the capture and the restore the instance is
	 * cleared, so for a texture nothing else in the project references, the instance held the last
	 * reference -- and generation can trip a GC on its way through (BuildTextureStreamingData
	 * collects). Without these the restore would put back a pointer to a collected object.
	 */
	struct FDreamShaderCapturedParameterOverrides
	{
		TArray<FDreamShaderCapturedParameterOverride> Overrides;
		TArray<TStrongObjectPtr<UObject>> ReferencedObjects;

		bool IsEmpty() const { return Overrides.Num() == 0; }
	};

	/**
	 * Read every parameter the instance itself overrides, across every kind this engine has.
	 *
	 * Kinds are walked by index rather than by name so a kind added in a later engine version is
	 * captured without this file knowing about it. Only the ThinCustom pair is captured; any other
	 * instance shape has no rebuild to survive, and its overrides remain ordinary digest content.
	 *
	 * Call BEFORE the rebuild touches the base material, while the old parameter set is still live.
	 */
	void CaptureThinCustomParameterOverrides(UMaterialInstance* Instance, FDreamShaderCapturedParameterOverrides& OutCaptured);

	/**
	 * Put the captured overrides back on an instance whose parent has just been rebuilt.
	 *
	 * Only names the rebuilt base still declares under the same kind are restored; the rest are logged
	 * once, together, as DSH8155. Does NOT update the static permutation -- the caller does that right
	 * after, and doing it here would recache the shader map twice.
	 *
	 * Call on the SUCCESS path only. A failed rebuild returns with the instance's overrides never
	 * cleared (the atomic rollback puts the base's graph back), so there is nothing to restore and
	 * restoring anyway would duplicate what is already there.
	 */
	void RestoreThinCustomParameterOverrides(
		UMaterialInstanceConstant* Instance,
		UMaterial* BaseMaterial,
		const FDreamShaderCapturedParameterOverrides& Captured,
		const FString& SourceFilePath);
}
