// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The shell-class check of a 1.x texture default.
//
// A 1.x `Properties` default may be the engine's export form, `Texture2D'/Game/T.T'` (what "Copy Reference" puts on the
// clipboard), bare, quoted or inside `Path(...)`. The 1.x parser judged the class written in that shell against the
// property it was written on WHILE IT PARSED -- DSH1043 for a class that is not a texture, DSH1044 for a texture class
// of another dimension -- so a wrong paste failed whether or not the Graph ever read the property. The legacy front
// end carries such a default unresolved (the emitter resolves it, and only for a parameter something reads), so this
// is where the same judgement is made again, for every declaration, before anything is bound.
//
// Text only: nothing is loaded. The judgement is the 1.x one (ValidateTextureReferenceShellClass): no shell, no class,
// a class this plugin has never heard of and a texture class with no fixed dimension are all accepted.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Lang
{
	class FLangDiagnosticSink;
	struct FLegacyMigrationInfo;
	struct FModule;
}

namespace UE::DreamShader::Editor::Compiler
{
	/** Reports DSH1043 / DSH1044 into Diagnostics, at the default it judged. Returns false when it reported either. */
	bool ValidateDreamShaderLegacyTextureDefaults(
		const Lang::FModule& Module,
		const Lang::FLegacyMigrationInfo& Legacy,
		Lang::FLangDiagnosticSink& Diagnostics);

	// ResolveDreamShaderLegacyTextureTypes, the other half of this file, is declared in DreamShaderCompilePipeline.h:
	// `dsc migrate` binds a 1.x source too and needs it.
}
