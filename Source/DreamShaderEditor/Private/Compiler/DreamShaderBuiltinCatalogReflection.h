// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The reflection half of FBuiltinCatalog.
//
// DreamShaderLang depends on Core alone, so everything it knows about UMaterialExpression classes,
// their pins, their properties and the material attribute table arrives as plain data. This is the
// producer of that data in the editor; LoadBuiltinCatalogFromJson is the other one, for the tools
// and the language service, and the two have to agree for the same engine.
//
// The entry point is declared in DreamShaderIREmitter.h (CONTRACT §5 names it there). This header
// exists for the pieces unit P's catalog-manifest exporter may want to reuse.

#pragma once

#include "CoreMinimal.h"

#include "IR/IRCatalog.h"

class UClass;

namespace UE::DreamShader::Editor::Compiler
{
	/** `UMaterialExpressionTextureCoordinate` -> `TextureCoordinate`. Empty when the class is not one. */
	FString GetMaterialExpressionCatalogShortName(const UClass* Class);

	/** "Substrate" for the Substrate BSDF / utility classes, "UE" for everything else. */
	const TCHAR* GetMaterialExpressionCatalogNamespace(const UClass* Class);
}
