// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Producer B of the parameter schema a `.dsi` binds against: read off a LOADED parent asset. Used for a parent
// that no DreamShader source under the roots builds (a hand-made material or instance), and by the instance
// emitter's drift check (DSH8246), which asks the asset because a source-derived schema can be ahead of a stale
// parent.
//
// Producer A, from the parent's IR, is engine-free and lives in DreamShaderLang (IR/IRInstanceSchema.h). Both
// encode FIRParameterSchemaEntry::ParentValue the way FIRInstanceOverride::Value documents, so the binder cannot
// tell which one it was handed. Design: Plan/m4m5/research-instance.md sections 3.4 and 7 (IN-E).

#pragma once

#include "CoreMinimal.h"

#include "IR/IR.h"

class UMaterialInterface;
enum class EMaterialParameterType : uint8;

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Every parameter of Parent, all kinds and associations, walked by type index over [0, NumMaterialParameterTypes)
	 * with UMaterialInterface::GetAllParametersOfType -- never by a list of enumerators, which would go out of date
	 * with the engine. Origin "asset", ParentObjectPath = Parent's path, bParentIsInstance for a UMaterialInstance;
	 * DeclaredType stays Error (an asset does not know what a source spelled). A kind the IR has no name for is
	 * skipped. Returns false, with bValid false, for a null parent.
	 */
	DREAMSHADERCOMPILER_API bool BuildParameterSchemaFromAsset(const UMaterialInterface* Parent, IR::FIRParameterSchema& OutSchema);

	/** The engine parameter type of an IR parameter kind; false when this engine has no such type. */
	DREAMSHADERCOMPILER_API bool TryGetEngineParameterTypeForIRKind(IR::EIRParameterKind Kind, EMaterialParameterType& OutType);
}
