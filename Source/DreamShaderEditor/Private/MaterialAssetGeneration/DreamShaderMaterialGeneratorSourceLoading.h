// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Source loading for the DreamShader material generator: read a .dsm/.dsf and recursively inline its
// imports into a single prepared source string (with "// Begin/End DreamShader source:" markers used
// by the diagnostics line mapping), detecting import cycles and enforcing header/function-file rules.

#pragma once

#include "CoreMinimal.h"
#include "DreamShaderDiagnostic.h"

namespace UE::DreamShader::Editor
{
	bool LoadPreparedDreamShaderSource(const FString& SourceFilePath, FString& OutSourceText, FDreamShaderError& OutError);
}
