// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::DreamShader::IR::GeneratedCoreCode
{
	// Exact code and pin names shared by the emitter and graph importer. The versioned marker
	// identifies the implementation, not just a Custom node with a similar caption.
	inline constexpr const TCHAR* Round = TEXT("// DreamShader core round v1\nreturn round(Input);");
	inline constexpr const TCHAR* RoundInput = TEXT("Input");
	inline constexpr const TCHAR* RoundDescription = TEXT("DreamShader round (nearest even)");
}
