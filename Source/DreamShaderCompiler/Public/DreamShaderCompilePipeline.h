// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The compile pipeline's public surface: preprocess -> parse -> bind -> lower -> validate -> emit.
//
// Merged in the compiler relocation from two headers of the editor module:
//
//   * the frozen DreamShaderCompilerPipeline.h -- IsDreamShaderLang2Source and
//     CompileDreamShaderLang2File, the thin entry the 1.x generator dispatched a `.dss` through;
//   * part 1 of DreamShaderCompilerTools.h -- FDreamShaderLang2PipelineOptions / Result and
//     RunDreamShaderLang2Pipeline, the same driver with its intermediate products handed back instead
//     of dropped: `check` wants to stop before the emit, `dump-ir` wants the IR module, `index` wants
//     the bound module, `check --shaders` wants the assets that came out, `dump-graph` wants their paths.
//
// Plus one addition, ResolveDreamShaderSourceProducts: which assets a source builds, and under which
// build key, WITHOUT building them. The Material Content Browser, the preview and dump-graph used to
// answer that by running the 1.x parser and loader over the source; they ask here instead.
//
// The commandlet verbs that shared DreamShaderCompilerTools.h stay in the editor module
// (Tools/DreamShaderCompilerTools.h). Everything declared here is implemented in
// Private/Pipeline/DreamShaderCompilePipeline.cpp.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilerIncludes.h"
#include "DreamShaderDefineTable.h"
// FDreamShaderError, CompileDreamShaderLang2File's OutError.
#include "DreamShaderDiagnostic.h"
// EThinCustomPersistence, which FDreamShaderLang2PipelineOptions forwards to the emitter.
#include "DreamShaderCompilerInterface.h"
#include "IR/IR.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"
#include "UObject/WeakObjectPtr.h"

class UObject;

namespace UE::DreamShader::Lang
{
	struct FLegacyMigrationInfo;
}

namespace UE::DreamShader::Editor::Compiler
{
	// ------------------------------------------------------------------------ the thin entry

	/**
	 * True for every file the pipeline compiles on its own: `.dss` and `.dsi` through the 2.0 front end, `.dsm`
	 * and `.dsf` through the legacy one (the name is older than that: once only `.dss` answered true). A `.dsh` header
	 * answers false: it is compiled only as part of the source that includes it.
	 */
	DREAMSHADERCOMPILER_API bool IsDreamShaderLang2Source(const FString& SourceFilePath);

	/**
	 * Compiles one source (see IsDreamShaderLang2Source) into its assets: one material, one instance, or one
	 * asset per exported function. The same compile as the compiler service's CompileAssets with a Materialized
	 * request -- notice and warnings included -- spelled with an FDreamShaderError for callers that want the code
	 * and the text apart. bForce rebuilds even when the source hash says the assets are current. On failure
	 * OutError carries the first error in the wire form and the diagnostics store has all of them.
	 */
	DREAMSHADERCOMPILER_API bool CompileDreamShaderLang2File(const FString& SourceFilePath, bool bForce, UE::DreamShader::FDreamShaderError& OutError);

	// ------------------------------------------------------------------------ the full run

	struct FDreamShaderLang2PipelineOptions
	{
		/** Rebuild even when a product's stamped source hash says its asset is current. */
		bool bForce = false;

		/**
		 * False stops after ValidateDreamShaderIR: everything the front end can say has been said and
		 * nothing was written. That is `check`, and it is the reason the emit is the last frame of
		 * the run rather than interleaved with validation.
		 */
		bool bEmitAssets = true;

		/**
		 * The state a ThinCustom product of this run ends in; every other product kind always saves. Materialized
		 * by default, so a tool that runs the pipeline itself (check, dump-ir, dump-graph) writes what it builds.
		 * The compiler service forwards its request's value.
		 */
		::UE::DreamShader::EThinCustomPersistence ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Materialized;
	};

	/**
	 * Everything one run produced, owned.
	 *
	 * The member ORDER is load-bearing. FBoundModule holds raw pointers into the parsed module and
	 * into every module the include resolver parsed, and FIRModule is built from it; members are
	 * destroyed in reverse declaration order, so IR dies first, then Bound, then the two things
	 * Bound points into, then the define table the resolver holds by reference. Reordering these
	 * declarations is a use-after-free with no other symptom.
	 */
	struct FDreamShaderLang2PipelineResult
	{
		/** Absolute, normalized. */
		FString SourceFilePath;

		/** Every diagnostic of the whole run, front end and emitter together. */
		UE::DreamShader::Lang::FLangDiagnosticSink Diagnostics;

		/** The build key: the preprocessed text of the file AND of every header, plus the defines read. */
		FString SourceHash;

		/** Resolved `#include` paths in first-seen order. Feeds the dependency graph and the asset's include list. */
		TArray<FString> IncludePaths;

		/** The defines the preprocessor actually read, over the file and every header. Folded into SourceHash. */
		UE::DreamShader::FDreamShaderDefineValueMap TouchedDefines;

		/** True when the file or any header carried a preprocessor directive, taken or not (the Adopt gate). */
		bool bSourceHadPreprocessorDirectives = false;

		/** Product indices in the order they were emitted; parallel to ProductAssets / ProductAssetPaths. */
		TArray<int32> ProductOrder;
		TArray<TWeakObjectPtr<UObject>> ProductAssets;
		TArray<FString> ProductAssetPaths;

		bool bSucceeded = false;
		/** True when the user pressed Cancel. Distinct from a failure: nothing was written either way, but nothing is WRONG. */
		bool bCancelled = false;

		/** `.dsi` only: the parent's resolved object path. Part of the build key; the schema is not. */
		FString ParentObjectPath;

		// --- owned state, in destruction-safe order; see the struct comment ---
		TUniquePtr<UE::DreamShader::FDreamShaderDefineTable> Defines;
		TUniquePtr<UE::DreamShader::Lang::FLangSourceText> Source;
		TUniquePtr<UE::DreamShader::Lang::FModule> Module;
		TUniquePtr<FDreamShaderIncludeResolver> Includes;
		/** `.dsi` only: the schema the instance was bound against. FBoundModule::ParentSchema points into it, so it is declared before Bound. */
		TUniquePtr<UE::DreamShader::IR::FIRParameterSchema> ParentSchema;
		TUniquePtr<UE::DreamShader::Lang::FBoundModule> Bound;
		TUniquePtr<UE::DreamShader::IR::FIRModule> IR;
	};

	/**
	 * The whole run: read, preprocess, parse, bind, lower, run the passes, validate, and -- unless
	 * Options says otherwise -- emit every product in dependency order.
	 *
	 * Returns false for any failure, including cancellation; OutResult.Diagnostics always says why.
	 * OutResult keeps whatever it got as far as, so a caller that only wants the AST (a language
	 * service on a file that does not bind) gets it from a false return.
	 *
	 * Game thread only: the emit half creates UObjects.
	 */
	DREAMSHADERCOMPILER_API bool RunDreamShaderLang2Pipeline(
		const FString& SourceFilePath,
		const FDreamShaderLang2PipelineOptions& Options,
		FDreamShaderLang2PipelineResult& OutResult);

	// ------------------------------------------------------------------------ product resolution

	/** One product a source builds, located without building it. */
	struct FDreamShaderResolvedProduct
	{
		/** Index into FIRModule::Products: the same index FDreamShaderLang2PipelineResult::ProductOrder holds. */
		int32 ProductIndex = INDEX_NONE;

		UE::DreamShader::IR::EIRProductKind Kind = UE::DreamShader::IR::EIRProductKind::Material;

		/**
		 * For a ThinCustom material, ObjectPath below is the UDreamShaderMaterialInstance's: the
		 * addressable half of the pair, the same object the emitting run hands back.
		 */
		UE::DreamShader::IR::EIRBackend Backend = UE::DreamShader::IR::EIRBackend::Graph;

		/** `/Game/FX/M_Glow`. What FPackageName::DoesPackageExist takes. */
		FString PackageName;

		/** `/Game/FX/M_Glow.M_Glow`. What FindObject / LoadObject take, and where the emitted asset lands. */
		FString ObjectPath;
	};

	/** Everything ResolveDreamShaderSourceProducts found out about one source. */
	struct FDreamShaderProductResolution
	{
		/** Absolute, normalized. */
		FString SourceFilePath;

		/** Every diagnostic of the resolution. Never an emitter diagnostic: nothing is emitted. */
		UE::DreamShader::Lang::FLangDiagnosticSink Diagnostics;

		/**
		 * The build key, computed by the same code and from the same inputs as
		 * FDreamShaderLang2PipelineResult::SourceHash, so it equals the DreamShader.SourceHash a
		 * current product was stamped with. Compare it with IsGeneratedAssetSourceCurrent; never hash
		 * the source text again to get it. Set as soon as the bind has run, even when a later stage
		 * fails.
		 */
		FString SourceHash;

		/** Resolved `#include` paths in first-seen order, as the emitting run records them. */
		TArray<FString> IncludePaths;

		/** The defines the preprocessor read, over the file and every header. Folded into SourceHash. */
		UE::DreamShader::FDreamShaderDefineValueMap TouchedDefines;

		/** True when the file or any header carried a preprocessor directive, taken or not (the Adopt gate). */
		bool bSourceHadPreprocessorDirectives = false;

		/** In FIRModule::Products order, which is declaration order and NOT emit order. */
		TArray<FDreamShaderResolvedProduct> Products;

		/** The first Material product, or null: what the Material Content Browser and the preview mean by "the asset of this source". */
		const FDreamShaderResolvedProduct* FindMaterialProduct() const
		{
			return Products.FindByPredicate([](const FDreamShaderResolvedProduct& Product)
			{
				return Product.Kind == UE::DreamShader::IR::EIRProductKind::Material;
			});
		}
	};

	/**
	 * Which assets a source builds and under which build key, WITHOUT building them.
	 *
	 * Runs the front end the emitting run would pick for the file's extension, then the binder and
	 * the IR builder, and resolves every product's destination by the rules the emitter uses (the
	 * source-root default, `/// @name`, ResolveDreamShaderAssetDestination). It does not run the
	 * passes, the validator or the emitter. It creates, loads and saves nothing, and it never opens a
	 * progress dialog: the Material Content Browser calls it for every source it lists.
	 *
	 * Replaces the 1.x recipes that answered the same question with the 1.x parser
	 * (UI/DreamShaderGeneratedAssetPath.cpp, Preview/DreamShaderPreviewRenderer.cpp,
	 * Commandlet/DreamShaderGraphDump.cpp) and the 1.x loader the Browser hashed with
	 * (UI/Model/DreamShaderBrowserModel.cpp).
	 *
	 * Returns false when the source does not get as far as its products: it cannot be read,
	 * preprocessed, parsed, bound or lowered, it is a header, or a destination does not resolve.
	 * OutResult.Diagnostics then says why, and whatever was established before the failure is kept.
	 * A source that declares no product answers true with an empty Products array.
	 *
	 * Game thread only: binding reads the builtin catalog, which is built from reflection.
	 */
	DREAMSHADERCOMPILER_API bool ResolveDreamShaderSourceProducts(
		const FString& SourceFilePath,
		FDreamShaderProductResolution& OutResult);

	/**
	 * Where ONE product would land if SourceFilePath declared it, by the rules the emitter uses: the source-root
	 * default (the plugin's mount point for a file under a plugin root), the root-relative folder, the product's name,
	 * and `/// @name` when Product.AssetPathOverride carries one. The file does not have to exist and nothing is read
	 * from it; only Product.Kind, Name, AssetPathOverride, bLegacyAssetPath and AssetRoot are looked at.
	 *
	 * The decompiler asks this before it writes a file, to find out whether the text needs a `/// @name` to keep the
	 * asset where it is. False, with OutError, when the destination does not resolve.
	 */
	DREAMSHADERCOMPILER_API bool ResolveDreamShaderProductDestination(
		const UE::DreamShader::IR::FIRProduct& Product,
		const FString& SourceFilePath,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutError);

	// ------------------------------------------------------------------------------ symbol index

	/**
	 * `<OutputDirectory>/<root>/<source path relative to that root>.index.json`, where the symbol index of a source
	 * lives. An empty OutputDirectory is the default, `<Project>/Saved/DreamShader/Index`. The root
	 * folder is what keeps two source roots that each hold a `Materials/M_Foo.dss` apart; a file under no root goes
	 * under `External` by its file name.
	 *
	 * One rule for both writers -- `dsc index` and the compiler service -- so that a language service reads one place
	 * whoever compiled last.
	 */
	DREAMSHADERCOMPILER_API FString GetDreamShaderSymbolIndexFilePath(const FString& SourceFilePath, const FString& OutputDirectory = FString());

	/**
	 * Writes BuildDreamShaderSymbolIndexJson(Bound) there. A file that already holds that text is left alone, so an
	 * unchanged compile does not wake whoever watches the folder. False, with OutError, when it could not be written.
	 */
	DREAMSHADERCOMPILER_API bool WriteDreamShaderSymbolIndex(
		const FString& SourceFilePath,
		const UE::DreamShader::Lang::FBoundModule& Bound,
		const FString& OutputDirectory,
		FString& OutIndexPath,
		FString& OutError);

	/**
	 * `TextureObjectParameter Font = Path(Plugins.DreamGUI, "Textures/FontArray")`: the type token is one node class that
	 * carries any dimension, and 1.x took the dimension from the asset (ResolveEffectiveTextureType). The front end reads
	 * such a declaration as Texture2D; this loads the default and retypes the declaration -- TextureCube, Texture2DArray,
	 * VolumeTexture -- before anything is bound, so that it fits the parameter it is passed to. A default that does not
	 * resolve or load is left for the emitter to report.
	 *
	 * The pipeline does this between the parse and the bind of a 1.x source, and so does whoever else binds one:
	 * `dsc migrate`, whose migrated text then declares the uniform with the type it really has.
	 */
	DREAMSHADERCOMPILER_API void ResolveDreamShaderLegacyTextureTypes(
		UE::DreamShader::Lang::FModule& Module,
		const UE::DreamShader::Lang::FLegacyMigrationInfo& Legacy);
}
