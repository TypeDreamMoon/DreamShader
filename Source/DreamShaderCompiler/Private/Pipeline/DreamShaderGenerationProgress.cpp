// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The one out-of-line piece of DreamShaderGenerationProgress.h: the cancel override's storage.
//
// It is here, in the compiler module's DLL, and nowhere else on purpose. The declaration used to be an
// inline function with a function-local static, and an inline function gets one such static per DLL that
// instantiates it: an automation test in the editor module would arm its own copy while the pipeline in
// this module read another, and the cancel test would stop cancelling without failing to build
// (research-relocation section 4.7).

#include "DreamShaderGenerationProgress.h"

namespace UE::DreamShader::Editor::Private
{
	FDreamShaderGenerationCancelPredicate& GetDreamShaderGenerationCancelOverride()
	{
		static FDreamShaderGenerationCancelPredicate Override;
		return Override;
	}
}
