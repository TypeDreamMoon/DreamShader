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
// The one asymmetry worth knowing: every product this file creates is Materialized. 2.0 has no
// request for anything else (plan §5) -- materials and material functions always save -- so the
// 1.x factory's persistence flag is passed false at every call site below and is never a choice
// the 2.0 pipeline makes. That flag survives only because the 1.x signatures still carry it
// until M4 deletes them; the persistence a ThinCustom product can actually have is
// EThinCustomPersistence { Ephemeral, Materialized }, and 2.0 only ever asks for Materialized.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderDiagnostic.h"
#include "DreamShaderTypes.h"
#include "IR/IR.h"

class UMaterial;
class UMaterialFunction;
class UDreamShaderMaterialInstance;

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

	/** Thin wrappers over the 1.x factory. Always Materialized: the product is created on disk. */
	bool CreateOrReuseIRMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FDreamShaderError& OutError);
	bool CreateOrReuseIRMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FDreamShaderError& OutError);
	bool CreateOrReuseIRThinCustomInstance(const FTextShaderDefinition& Definition, UDreamShaderMaterialInstance*& OutInstance, FDreamShaderError& OutError);

	/**
	 * The hidden base UMaterial a ThinCustom instance parents to: a subobject of the instance, so
	 * the pair shares one package and one file on disk. Mirrors the persist half of the 1.x
	 * EnsureThinCustomBaseMaterial (DreamShaderMaterialGenerator.cpp ~3156); its Ephemeral half --
	 * the base built into the transient package for an instance that hides itself -- has no 2.0
	 * caller, because every 2.0 product is Materialized.
	 */
	bool EnsureIRThinCustomBaseMaterial(
		UDreamShaderMaterialInstance* Instance,
		UMaterial*& OutBase,
		FDreamShaderError& OutError);

	/**
	 * Registers a freshly created generated asset with the asset registry so the Content Browser
	 * sees it and its folder without waiting for a rescan.
	 *
	 * Copied from the file-static PublishGeneratedAsset in
	 * MaterialAssetGeneration/DreamShaderAssetFactory.cpp (~468); its Ephemeral half
	 * (AssetViewUtils::OnAlwaysShowPath) is dropped because every 2.0 product is Materialized.
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
