// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderBuiltinCatalog.h. The cache half of the former DreamShaderCatalogManifest.cpp, moved
// verbatim in the compiler relocation; the manifest export stayed in the editor module.

#include "DreamShaderBuiltinCatalog.h"

#include "DreamShaderModule.h"

#include "Modules/ModuleManager.h"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		TUniquePtr<UE::DreamShader::IR::FBuiltinCatalog> GCachedCatalog;

		/**
		 * Bumped whenever a module loads or unloads. That is the coarse but honest trigger: a
		 * plugin mounting brings UMaterialExpression classes with it, and a catalog built before it
		 * reports every one of them as an unknown name with a "did you mean" that names nothing.
		 * Reflection is cheap enough that over-invalidating costs a second at worst.
		 */
		uint32 GCatalogRevision = 0;
		uint32 GCachedCatalogRevision = MAX_uint32;
		bool bGModulesDelegateRegistered = false;

		void EnsureModulesChangedHook()
		{
			if (bGModulesDelegateRegistered)
			{
				return;
			}

			bGModulesDelegateRegistered = true;
			FModuleManager::Get().OnModulesChanged().AddLambda([](FName, EModuleChangeReason)
			{
				++GCatalogRevision;
			});
		}
	}

	const UE::DreamShader::IR::FBuiltinCatalog& GetDreamShaderBuiltinCatalog()
	{
		EnsureModulesChangedHook();

		if (!GCachedCatalog.IsValid() || GCachedCatalogRevision != GCatalogRevision)
		{
			GCachedCatalog = MakeUnique<UE::DreamShader::IR::FBuiltinCatalog>();
			BuildBuiltinCatalogFromReflection(*GCachedCatalog);
			GCachedCatalogRevision = GCatalogRevision;

			UE_LOG(
				LogDreamShader,
				Verbose,
				TEXT("DreamShader built the builtin expression catalog from reflection: %d expression(s), %d material attribute(s)."),
				GCachedCatalog->Expressions.Num(),
				GCachedCatalog->MaterialAttributes.Num());
		}

		return *GCachedCatalog;
	}

	void InvalidateDreamShaderBuiltinCatalog()
	{
		++GCatalogRevision;
	}
}
