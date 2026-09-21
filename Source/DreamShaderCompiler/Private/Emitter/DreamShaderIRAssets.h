// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Asset identity for an IR product: where it lands, how it is created or reused, and how its
// metadata is stamped.
//
// None of this is new. The 1.x generator already owns the destination rules
// (ResolveDreamShaderAssetDestination + ApplyDefaultRootFromSourceFile), the create/reuse paths with
// their ownership guard, the source-hash skip, the digest and the save, and all of that is reached
// through DreamShaderMaterialGeneratorPrivate.h. What this file adds is the adapter: those functions
// speak FTextShaderDefinition / FTextShaderMaterialFunctionDefinition, so an FIRProduct's Name /
// AssetPathOverride / Settings are folded into the minimal definitions they read, and nothing else
// about the 1.x structs is filled in.
//
// Persistence: a material and a material function always save. A ThinCustom instance follows the compile
// request's EThinCustomPersistence { Ephemeral, Materialized }, which the pipeline forwards through
// FIREmitContext since the compiler relocation; storage still decides, so an instance whose package is on disk stays Materialized.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderCompilerInterface.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderTypes.h"
#include "IR/IR.h"

class UMaterial;
class UMaterialFunction;
class UDreamShaderMaterialInstance;
class UMaterialInstanceConstant;

namespace UE::DreamShader::Editor::Compiler
{
	/**
	 * Builds the minimal FTextShaderDefinition the 1.x asset factory and ApplySettings read:
	 * Name, Root and Settings, and nothing else.
	 *
	 * Naming follows CONTRACT §2 "Product naming". A bare `Name` is the leaf and the root rules
	 * apply, which for a source under a plugin source root means the plugin's own mount point
	 * (ApplyDefaultRootFromSourceFile). An `AssetPathOverride` that names a full package path is
	 * split at its last separator: everything before it becomes Root -- so `/Game`, `/Plugin.X` and
	 * a bare relative path are resolved by exactly the same 1.x code -- and the leaf becomes Name.
	 *
	 * Settings keys are lower-cased on the way in, because FTextShaderDefinition::TryGetSetting and
	 * ApplySettings both look them up through NormalizeSettingKey. Backend is already stripped by
	 * the binder (it lives on FIRProduct::Backend), so anything left here is a real UMaterial
	 * property path.
	 */
	void BuildDefinitionForIRProduct(
		const IR::FIRProduct& Product,
		const FString& SourceFilePath,
		FTextShaderDefinition& OutDefinition);

	/**
	 * The function-asset half of the same adapter. Takes the definition
	 * BuildDefinitionForIRProduct already resolved (so the Root default is applied once) and adds
	 * the kind, which is what decides UMaterialFunction vs UMaterialFunctionMaterialLayer vs
	 * UMaterialFunctionMaterialLayerBlend, plus the library exposure the `/// @library` doc tag asks
	 * for.
	 */
	void BuildFunctionDefinitionForIRProduct(
		const IR::FIRProduct& Product,
		const FTextShaderDefinition& ResolvedDefinition,
		FTextShaderMaterialFunctionDefinition& OutDefinition);

	/** The object path the product will occupy, without creating anything. Used for diagnostics and for the local-function table. */
	bool ResolveIRProductObjectPath(
		const IR::FIRProduct& Product,
		const FString& SourceFilePath,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutAssetLeafName,
		FDreamShaderError& OutError);

	/**
	 * Thin wrappers over the asset factory. A material and a material function always persist; the ThinCustom
	 * instance takes the compile request's persistence, and storage still decides (an instance whose package
	 * exists on disk stays Materialized).
	 */
	bool CreateOrReuseIRMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FDreamShaderError& OutError);
	bool CreateOrReuseIRMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FDreamShaderError& OutError);
	bool CreateOrReuseIRThinCustomInstance(
		const FTextShaderDefinition& Definition,
		UDreamShaderMaterialInstance*& OutInstance,
		FDreamShaderError& OutError,
		::UE::DreamShader::EThinCustomPersistence Persistence);

	/**
	 * The MaterialInstance product of a `.dsi`: a PLAIN UMaterialInstanceConstant, never the ThinCustom class and its IsAsset
	 * trick (research-instance section 3.6 step 2). Refuses an existing asset of another class, the ThinCustom one included
	 * (DSH8241), and a saved asset DreamShader did not generate (DSH8242); DSH8253 when the package or the object cannot be
	 * created. Always persisted: an instance has no memory-only state.
	 */
	bool CreateOrReuseIRMaterialInstance(const FTextShaderDefinition& Definition, UMaterialInstanceConstant*& OutInstance, FDreamShaderError& OutError);

	/**
	 * The hidden base UMaterial a ThinCustom instance parents to, ported from the 1.x EnsureThinCustomBaseMaterial with both
	 * of its halves:
	 *
	 * - Materialized: a subobject of the instance, so the pair shares one package and one file on disk.
	 * - Ephemeral (bEphemeral): a base in the transient package, for an instance that hides itself and is never saved; named
	 *   after the instance's package, so two instances never share one, and RF_Transient, so nothing serializes it by accident.
	 *
	 * Both carry RF_Standalone: RecompileMaterial runs a full GC pass before the instance parents to the base.
	 */
	bool EnsureIRThinCustomBaseMaterial(
		UDreamShaderMaterialInstance* Instance,
		bool bEphemeral,
		UMaterial*& OutBase,
		FDreamShaderError& OutError);

	/**
	 * Registers a freshly SAVED generated asset with the asset registry so the Content Browser sees it and its folder without
	 * waiting for a rescan. The creation-time publish, Ephemeral half included (AssetViewUtils::OnAlwaysShowPath), is the asset
	 * factory's own (Assets/DreamShaderAssetFactory.cpp, PublishGeneratedAsset); this covers an instance that was memory-only
	 * when it was created, whose AssetCreated then did nothing because IsAsset() answered false.
	 */
	void PublishGeneratedIRAsset(UObject* GeneratedAsset);

	/**
	 * Sets one package-metadata value on an asset.
	 *
	 * Copied from the file-static SetSourceMetadataValue in
	 * MaterialAssetGeneration/DreamShaderGeneratedAssetMetadata.cpp (~45), engine guard included:
	 * UPackage::GetMetaData() returns FMetaData& from 5.6 and UMetaData* before it.
	 */
	void SetDreamShaderAssetMetadata(UObject* Asset, const TCHAR* Key, const FString& Value);
}
