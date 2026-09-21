// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// How anything outside the compiler asks for a DreamShader source to be compiled into its assets.
//
// It lives in the RUNTIME module on purpose. The compiler (DreamShaderCompiler) is an Editor-type
// module: it is absent from game targets, and only the editor module above it may depend on it at build
// time. This header needs nothing but Core, so every other caller includes it instead. The compiler
// module implements IDreamShaderCompiler and registers the implementation when it starts;
// GetDreamShaderCompiler() hands it back -- loading the compiler module first when nothing has loaded it
// yet -- and answers null wherever no compiler exists: a game target, or an engine that is shutting down.
//
// Namespace UE::DreamShader, not UE::DreamShader::Compiler: inside UE::DreamShader::Editor a bare
// `Compiler::` names UE::DreamShader::Editor::Compiler, so a nested namespace here would be hidden from
// exactly the callers that use it most (CONTRACT m2m3 §6.13 #52a).
//
// Moved here in the compiler relocation from DreamShaderCompiler/Public/DreamShaderCompilerInterfaces.h. The
// wrapper FDreamShaderCompileService and the editor's compile adapter are gone; callers use the interface.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader
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

	/** One compile request. */
	struct DREAMSHADER_API FDreamShaderCompileRequest
	{
		/** A `.dss`, `.dsm` or `.dsf` source; the compiler normalizes it. A `.dsh` header is not a compile unit. */
		FString SourceFilePath;

		/** Rebuild even when a product's stamped build key says its asset is current. */
		bool bForce = false;

		/**
		 * Which state a ThinCustom product this compile touches should end in. Ignored by the Graph and
		 * material-function backends, which always save. Materialized is the default so a caller that
		 * does not care -- the commandlet, a cook -- writes assets to disk as it always has; the
		 * interactive editor paths ask for Ephemeral explicitly.
		 */
		EThinCustomPersistence ThinCustomPersistence = EThinCustomPersistence::Materialized;
	};

	/** What one compile did. */
	struct DREAMSHADER_API FDreamShaderCompileResult
	{
		bool bSucceeded = false;

		/**
		 * The compile's report in the wire shape the bridge, the extensions and `dsc.ps1` parse. On
		 * success, one `Generated <Kind> <ObjectPath> from <Source>.` (or `Skipped ...`) line per product
		 * and an optional `Warnings:` block. On failure, the first error as
		 * `<file>(<line>,<column>): DSHnnnn: <message>`, then every other diagnostic on its own line.
		 * A caller that writes it to a wire passes it through ToInvariantWireString.
		 */
		FText Message;

		/**
		 * The DSHnnnn code of the first error; empty on success. Carried beside Message so a caller that
		 * keys on codes -- the diagnostics store, a test -- does not have to parse it back out of the text.
		 */
		FString Code;
	};

	/** Implemented once, by the compiler module's service (DreamShaderCompilerService.h). */
	class DREAMSHADER_API IDreamShaderCompiler
	{
	public:
		virtual ~IDreamShaderCompiler() = default;

		/** Every product of the source: its material and every exported function, layer and blend. */
		virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) = 0;

		/** The source's material product only: the preview's route. */
		virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) = 0;
	};

	/**
	 * The registered compiler, or null.
	 *
	 * In an editor build the compiler module is loaded on demand when nothing has registered yet, so a
	 * caller that runs before the module's own start still gets it: both modules are Default-phase, and
	 * start order within a phase is not a contract. Null in a game target, where the module does not
	 * exist, and while the engine is shutting down. Every caller must handle null.
	 *
	 * Game thread only whenever it may have to load the module.
	 */
	DREAMSHADER_API IDreamShaderCompiler* GetDreamShaderCompiler();

	/**
	 * For the compiler module: its service from StartupModule, nullptr from ShutdownModule. The registry
	 * does not own the object.
	 */
	DREAMSHADER_API void RegisterDreamShaderCompiler(IDreamShaderCompiler* Compiler);
}
