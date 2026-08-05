#pragma once

#include "Runtime/Launch/Resources/Version.h"

#ifndef DREAMSHADER_UE_MAJOR
#define DREAMSHADER_UE_MAJOR ENGINE_MAJOR_VERSION
#endif

#ifndef DREAMSHADER_UE_MINOR
#define DREAMSHADER_UE_MINOR ENGINE_MINOR_VERSION
#endif

#ifndef DREAMSHADER_UE_PATCH
#define DREAMSHADER_UE_PATCH ENGINE_PATCH_VERSION
#endif

#ifndef DREAMSHADER_WITH_SUBSTRATE_BUILTINS
#define DREAMSHADER_WITH_SUBSTRATE_BUILTINS (DREAMSHADER_UE_MAJOR > 5 || (DREAMSHADER_UE_MAJOR == 5 && DREAMSHADER_UE_MINOR >= 4))
#endif

// Set from DreamShader.Build.cs, which probes the engine's SceneTypes.h for MP_MoonEncodedAttribute0.
// This fallback only applies to translation units built outside that module's definitions (an IDE
// parsing a header on its own, for instance): off, i.e. stock Unreal, which is the safe assumption
// because the guarded code names enumerators only Moon Engine declares.
#ifndef DREAMSHADER_WITH_MOON_ENGINE
#define DREAMSHADER_WITH_MOON_ENGINE 0
#endif

#define DREAMSHADER_UE_VERSION_AT_LEAST(MajorVersion, MinorVersion) \
	(DREAMSHADER_UE_MAJOR > (MajorVersion) || (DREAMSHADER_UE_MAJOR == (MajorVersion) && DREAMSHADER_UE_MINOR >= (MinorVersion)))

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
#define DREAMSHADER_ALLOW_SHRINKING_NO EAllowShrinking::No
#else
#define DREAMSHADER_ALLOW_SHRINKING_NO false
#endif
