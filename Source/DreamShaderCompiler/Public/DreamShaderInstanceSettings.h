// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The `#pragma instance(...)` keys other than Parent: what each one sets on a material instance, in both directions.
//
// A key is either
//   (a) a value field of FMaterialInstanceBasePropertyOverrides: the value is written and the flag named by that
//       field's EditCondition metadata is set -- never a flag guessed from the name, because the engine's flag names
//       do not follow the value names (bOverride_CastDynamicShadowAsMasked guards bCastDynamicShadowAsMasked); or
//   (b) a row of a fixed table of material-instance properties, each with its own override flag where it has one
//       (PhysMaterial + bOverridePhysMaterial, SubsurfaceProfile + bOverrideSubsurfaceProfile, ...).
// Anything else is refused. Keys a `.dsi` no longer names revert to "not overridden" on every build: the base
// overrides start from a default struct, and every table row is reset to its class default first.
//
// Design: Plan/m4m5/research-instance.md sections 3.1 (keys), 3.6 (apply, digest) and 5 (read back).

#pragma once

#include "CoreMinimal.h"

#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"

class UMaterialInstance;
class UMaterialInstanceConstant;
struct FMaterialInstanceBasePropertyOverrides;

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Resets Instance's table rows, then applies Settings, in source order, to InOutBase and to those rows. Unknown key
	 * DSH8249, bad value DSH8250, refused key DSH8251, each at Source.Span. Returns false when any key failed; every
	 * good key is still applied, so a caller that must not leave a half-applied asset runs it on a scratch instance
	 * first.
	 */
	DREAMSHADERCOMPILER_API bool ApplyInstanceSettings(
		UMaterialInstanceConstant* Instance,
		const TArray<TPair<FString, FString>>& Settings,
		FMaterialInstanceBasePropertyOverrides& InOutBase,
		const IR::FIRSourceRef& Source,
		Lang::FLangDiagnosticSink& Diagnostics);

	/**
	 * The decompile direction: the keys Instance sets, in GetInstanceSettingKeys order, values in the spelling
	 * ApplyInstanceSettings reads back (enums without their prefix, objects as paths or None). OutUnsupported names
	 * the overridden state a `.dsi` cannot spell (UsageFlags).
	 */
	DREAMSHADERCOMPILER_API void ReadInstanceSettings(
		const UMaterialInstanceConstant* Instance,
		TArray<TPair<FString, FString>>& OutSettings,
		TArray<FString>& OutUnsupported);

	/** Every key: the base-override fields in struct order, then the table. For the bridge `directives` manifest and the language service. */
	DREAMSHADERCOMPILER_API const TArray<FString>& GetInstanceSettingKeys();

	/**
	 * The digest lines of an instance that is not a ThinCustom pair (research-instance section 3.6 step 8):
	 * `MI Base <BasePropertyOverrides as text>` and one `MI Key <Property>=<value>` per table property and flag the
	 * class has, so a hand edit of BlendMode or PhysMaterial on a `.dsi` instance reads as Diverged.
	 */
	DREAMSHADERCOMPILER_API void AppendInstanceSettingsDigestLines(const UMaterialInstance* Instance, FString& InOutText);
}
