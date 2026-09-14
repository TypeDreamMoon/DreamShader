#pragma once

#include "CoreMinimal.h"

#ifndef DREAMSHADERCOMPILER_API
#define DREAMSHADERCOMPILER_API
#endif

namespace UE::DreamShader::Compiler
{
	/**
	 * The two states a generated ThinCustom product can be in (architecture plan v2 §5).
	 *
	 * ThinCustom is the one backend with a memory-only form, because it is the one whose visible half
	 * -- UDreamShaderMaterialInstance -- can lie about IsAsset(). Graph materials and .dsf material
	 * functions are plain engine assets with no way to opt out of enumeration, so they have no second
	 * state and nothing here applies to them.
	 *
	 * Only two transitions exist: Materialize (an explicit Materialize action, a cook, or creating a
	 * child instance of the product) and Make Ephemeral (deleting the package on disk). A product that
	 * already has a package on disk stays Materialized no matter what a request asks for -- storage
	 * decides, which is why nothing here can resurrect the class of accident where a stale file on
	 * disk won over a freshly built copy in memory.
	 */
	enum class EThinCustomPersistence : uint8
	{
		/** No package on disk: the base material lives in the transient package and the instance hides itself. */
		Ephemeral,
		/** A package on disk holds the instance, with the base material as a subobject export of it. */
		Materialized,
	};

	struct DREAMSHADERCOMPILER_API FDreamShaderCompileRequest
	{
		FString SourceFilePath;
		bool bForce = false;

		/**
		 * Which state a ThinCustom product this compile touches should end in. Ignored by the Graph and
		 * material-function backends, which always save. Materialized is the default so a caller that
		 * does not care -- the commandlet, a cook -- writes assets to disk as it always has; the
		 * interactive editor paths ask for Ephemeral explicitly.
		 */
		EThinCustomPersistence ThinCustomPersistence = EThinCustomPersistence::Materialized;
	};

	struct DREAMSHADERCOMPILER_API FDreamShaderCompileResult
	{
		bool bSucceeded = false;
		FText Message;
	};

	class DREAMSHADERCOMPILER_API IDreamShaderCompiler
	{
	public:
		virtual ~IDreamShaderCompiler() = default;

		virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) = 0;
		virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) = 0;
	};
}
