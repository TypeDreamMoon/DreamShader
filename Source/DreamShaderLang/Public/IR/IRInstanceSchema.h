// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The parameter schema a `.dsi` binds against, produced from the parent's IR.
//
// When the parent of an instance is a DreamShader product its parameters are known without the
// engine: they are what its source lowers to. This is that producer (producer A); the other one reads
// the schema off a loaded parent asset and lives on the editor side. The schema is not part of the
// build key; the resolved parent object path is (CONTRACT section 2.3).
//
// Design: Plan/m4m5/research-instance.md section 6.4.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"

namespace UE::DreamShader::Lang
{
	struct FBoundModule;
}

namespace UE::DreamShader::IR
{
	/**
	 * Producer A: the parameter schema a child of ParentModule.Products[ProductIndex] binds against. ParentBound supplies
	 * declared types and spans (may be null: kinds only). For a MaterialInstance product the product's own
	 * ParentSchema is copied with ParentValue overlaid by its overrides (instance-of-instance).
	 */
	DREAMSHADERLANG_API bool BuildParameterSchemaFromIR(
		const FIRModule& ParentModule,
		int32 ProductIndex,
		const Lang::FBoundModule* ParentBound,
		FIRParameterSchema& OutSchema);
}
