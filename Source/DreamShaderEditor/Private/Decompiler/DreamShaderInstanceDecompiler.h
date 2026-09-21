// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// A UMaterialInstanceConstant read back into the payload a `.dsi` binds to (IR::FIRInstance), and from there into
// text by the Lang printer (PrintDreamShaderInstance). Both directions share that payload, so what a build writes
// into an instance and what a decompile reads out of it are one shape.
//
// Three callers, one reader: `dsc decompile` and the Content Browser export want what differs from the parent; Adopt
// of a `.dsi` and the two tweak actions of a ThinCustom pair want every override the instance itself carries, equal to
// the parent or not, because an override pinned to the parent's value has to stay pinned.
//
// Design: Plan/m4m5/research-instance.md section 5. Diagnostics: DSH9100-9102, DSH9104-9106 (DSH9103 is Adopt's).

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"

class UMaterialInstanceConstant;

namespace UE::DreamShader::Editor::Private
{
	enum class EInstanceDecompileFilter : uint8
	{
		/** Only parameters whose effective value differs from the parent's (`dsc decompile`). */
		DifferingFromParent,
		/** Every parameter the instance itself overrides, equal to the parent or not (Adopt). */
		OverriddenOnly,
	};

	struct FInstanceDecompileOptions
	{
		EInstanceDecompileFilter Filter = EInstanceDecompileFilter::DifferingFromParent;
		/** Where the `.dsi` will live: decides the default Parent spelling and whether `@name` is needed. */
		FString TargetSourceFilePath;
		/** Write `Parent = "M_Glow"` when the product index resolves it uniquely in the target's root. */
		bool bPreferBareParentName = false;
		/** Declared types of a DreamShader parent (producer A); null = kinds from the parent asset (producer B). */
		const UE::DreamShader::IR::FIRParameterSchema* ParentSchema = nullptr;
	};

	/** MIC -> instance payload. False (with errors) when the instance cannot be expressed; warnings for skipped state. */
	bool DecompileMaterialInstance(
		UMaterialInstanceConstant* Instance,
		const FInstanceDecompileOptions& Options,
		UE::DreamShader::IR::FIRInstance& OutInstance,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);

	/** DecompileMaterialInstance + Lang::PrintDreamShaderInstance. */
	bool DecompileMaterialInstanceToText(
		UMaterialInstanceConstant* Instance,
		const FInstanceDecompileOptions& Options,
		FString& OutText,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics);
}
