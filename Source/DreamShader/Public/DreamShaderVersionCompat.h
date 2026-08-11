#pragma once

#include "Misc/CoreDelegates.h"
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

// UE 5.8 added FCoreDelegates::GetOnPostEngineInit() and deprecated the OnPostEngineInit data member
// it replaces; 5.7 and earlier only have the member, so naming either one directly breaks the other
// engine (a hard compile error going back, a deprecation warning going forward). One accessor keeps
// the version test here instead of at every subscribe/unsubscribe site.
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)
#define DREAMSHADER_POST_ENGINE_INIT_DELEGATE() FCoreDelegates::GetOnPostEngineInit()
#else
#define DREAMSHADER_POST_ENGINE_INIT_DELEGATE() FCoreDelegates::OnPostEngineInit
#endif

// EThumbnailPrimType gained TPT_ShaderBall in UE 5.8. Earlier engines stop at TPT_Cylinder, so the
// shaderball preview falls back to the sphere -- the same shape ResolvePreviewPrimitiveType already
// uses for an unrecognized mesh selector.
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)
#define DREAMSHADER_THUMBNAIL_PRIM_SHADERBALL TPT_ShaderBall
#else
#define DREAMSHADER_THUMBNAIL_PRIM_SHADERBALL TPT_Sphere
#endif

// UE 5.7's MaterialValueTypeToMaterialAggregateAttributeType (MaterialAggregate.cpp) has no case for
// MCT_Substrate and checkf(false)s on its default branch. 5.8 added the missing case, so this is a
// 5.7-and-earlier engine defect, not something the caller can validate around.
#define DREAMSHADER_MATERIAL_AGGREGATE_HANDLES_SUBSTRATE DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)
