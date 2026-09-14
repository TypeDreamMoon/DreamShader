// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Compiler/DreamShaderIRAssets.h"
#include "Misc/Paths.h"

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		/**
		 * Splits `/Game/Some/Path/M_X` into Root `/Game/Some/Path` and Name `M_X`.
		 *
		 * This is what makes CONTRACT §2's "a full path in @name is AssetPathOverride" land on the
		 * 1.x root rules rather than beside them: ResolveDreamShaderAssetDestination treats its
		 * AssetName as relative to Root and its Root's first segment as the mount point, so handing
		 * it the whole path as a name would produce `/Game/Game/Some/Path/M_X`. Splitting at the
		 * last separator instead means `/Game`, `/Plugin.Foo/...` and a bare relative folder chain
		 * are all resolved by ResolveDreamShaderRootPackagePath exactly as a 1.x `Root=` would be.
		 *
		 * An override with no separator is a bare leaf: it replaces the name and leaves the root
		 * rules alone, which is the same sentence the contract writes for a bare `@name`.
		 */
		void SplitAssetPathOverride(const FString& Override, FString& OutRoot, FString& OutLeaf)
		{
			FString Normalized = Override;
			Normalized.TrimStartAndEndInline();
			Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (Normalized.EndsWith(TEXT("/")))
			{
				Normalized.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}

			FString Left;
			FString Right;
			if (Normalized.Split(TEXT("/"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
				&& !Right.IsEmpty())
			{
				OutRoot = Left;
				OutLeaf = Right;
				return;
			}

			OutRoot.Reset();
			OutLeaf = Normalized;
		}

		/**
		 * `DreamShaderTests/Compile2/C_Minimal` for `<root>/DreamShaderTests/Compile2/C_Minimal/C_Minimal.dss`:
		 * the source's folder relative to the source root that owns it, `/`-separated. Empty when the
		 * file sits in the root itself or under no root at all.
		 */
		FString GetSourceRootRelativeDirectory(const FString& SourceFilePath)
		{
			const UE::DreamShader::FDreamShaderSourceRoot* SourceRoot = UE::DreamShader::FindSourceRootForFile(SourceFilePath);
			if (!SourceRoot)
			{
				return FString();
			}

			FString Directory = FPaths::GetPath(FPaths::ConvertRelativePathToFull(SourceFilePath));
			FPaths::NormalizeDirectoryName(Directory);
			FString RootDirectory = SourceRoot->Directory;
			FPaths::NormalizeDirectoryName(RootDirectory);

			const FString Prefix = RootDirectory + TEXT("/");
			if (!Directory.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return FString();
			}
			return Directory.RightChop(Prefix.Len());
		}
	}

	void BuildDefinitionForIRProduct(
		const IR::FIRProduct& Product,
		const FString& SourceFilePath,
		FTextShaderDefinition& OutDefinition)
	{
		OutDefinition = FTextShaderDefinition{};

		if (!Product.AssetPathOverride.IsEmpty())
		{
			SplitAssetPathOverride(Product.AssetPathOverride, OutDefinition.Root, OutDefinition.Name);
		}
		else
		{
			OutDefinition.Name = Product.Name;
		}

		if (OutDefinition.Name.IsEmpty())
		{
			OutDefinition.Name = Product.Name;
		}

		// Lower-cased keys: FTextShaderDefinition::TryGetSetting and ApplySettings both look up
		// through NormalizeSettingKey, so a `#pragma material(TwoSided = true)` written in any case
		// has to be stored the way those lookups spell it. Backend is not here -- the binder moved
		// it onto FIRProduct::Backend -- so everything left is a real UMaterial property path.
		for (const TPair<FString, FString>& Setting : Product.Settings)
		{
			OutDefinition.Settings.Add(UE::DreamShader::NormalizeSettingKey(Setting.Key), Setting.Value);
		}

		// Only when the product declared no root of its own: a file under a plugin source root
		// defaults to that plugin's mount point instead of /Game. Must run before
		// ResolveDreamShaderAssetDestination at every site that turns a source file into an asset
		// path, which is why it is here rather than at one of the three call sites below.
		if (OutDefinition.Root.IsEmpty())
		{
			Private::ApplyDefaultRootFromSourceFile(SourceFilePath, OutDefinition);

			// The 2.0 default, as the plan and the syntax proposal fix it (source-root-relative folder
			// plus the product name): `DShader/FX/Glow.dss` exporting `M_Glow` is `/Game/FX/M_Glow`, and
			// the same file under a plugin's root lands under that plugin's mount point. A bare `@name`
			// still replaces only the leaf; a full-path one never reaches here. The 1.x rule stopped at
			// the root and put every asset directly in it, which is what the first 2.0 test run did --
			// into /Game, where no fixture's cleanup looked.
			const FString RelativeDirectory = GetSourceRootRelativeDirectory(SourceFilePath);
			if (!RelativeDirectory.IsEmpty())
			{
				OutDefinition.Root = OutDefinition.Root.IsEmpty()
					? FString::Printf(TEXT("Game/%s"), *RelativeDirectory) /* I18N-EXEMPT: package path, not display text */
					: FString::Printf(TEXT("%s/%s"), *OutDefinition.Root, *RelativeDirectory); /* I18N-EXEMPT: package path, not display text */
			}
		}
	}

	void BuildFunctionDefinitionForIRProduct(
		const IR::FIRProduct& Product,
		const FTextShaderDefinition& ResolvedDefinition,
		FTextShaderMaterialFunctionDefinition& OutDefinition)
	{
		OutDefinition = FTextShaderMaterialFunctionDefinition{};
		OutDefinition.Name = ResolvedDefinition.Name;
		OutDefinition.Root = ResolvedDefinition.Root;
		OutDefinition.Settings = ResolvedDefinition.Settings;

		switch (Product.Kind)
		{
		case IR::EIRProductKind::MaterialLayer:
			OutDefinition.Kind = ETextShaderMaterialFunctionKind::MaterialLayer;
			break;
		case IR::EIRProductKind::MaterialLayerBlend:
			OutDefinition.Kind = ETextShaderMaterialFunctionKind::MaterialLayerBlend;
			break;
		case IR::EIRProductKind::Material:
		case IR::EIRProductKind::MaterialFunction:
		default:
			OutDefinition.Kind = ETextShaderMaterialFunctionKind::ShaderFunction;
			break;
		}

		// `/// @desc` and `/// @library Cat|Sub` are product fields in the IR, but the 1.x function
		// path reads them out of the Settings map (Description / ExposeToLibrary /
		// LibraryCategories), so they are folded in here rather than applied twice.
		if (!Product.Description.IsEmpty())
		{
			OutDefinition.Settings.Add(UE::DreamShader::NormalizeSettingKey(TEXT("Description")), Product.Description);
		}
		if (!Product.LibraryPath.IsEmpty())
		{
			// Verbatim: a bar nests categories and a comma separates them, which is the 1.x LibraryCategories
			// spelling already; replacing the bar split one nested category into two top-level ones.
			const FString& Categories = Product.LibraryPath;
			OutDefinition.Settings.Add(UE::DreamShader::NormalizeSettingKey(TEXT("ExposeToLibrary")), TEXT("true"));
			OutDefinition.Settings.Add(UE::DreamShader::NormalizeSettingKey(TEXT("LibraryCategories")), Categories);
		}
	}

	bool ResolveIRProductObjectPath(
		const IR::FIRProduct& Product,
		const FString& SourceFilePath,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutAssetLeafName,
		FDreamShaderError& OutError)
	{
		FTextShaderDefinition Definition;
		BuildDefinitionForIRProduct(Product, SourceFilePath, Definition);
		return Private::ResolveDreamShaderAssetDestination(
			Definition.Name,
			Definition.Root,
			OutPackageName,
			OutObjectPath,
			OutAssetLeafName,
			OutError);
	}

	bool CreateOrReuseIRMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FDreamShaderError& OutError)
	{
		return Private::CreateOrReuseMaterial(Definition, OutMaterial, OutError, /*bTransient*/ false);
	}

	bool CreateOrReuseIRMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FDreamShaderError& OutError)
	{
		return Private::CreateOrReuseMaterialFunction(Definition, OutFunction, OutError, /*bTransient*/ false);
	}

	bool CreateOrReuseIRThinCustomInstance(const FTextShaderDefinition& Definition, UDreamShaderMaterialInstance*& OutInstance, FDreamShaderError& OutError)
	{
		return Private::CreateOrReuseInstanceMaterial(Definition, OutInstance, OutError, /*bTransient*/ false);
	}

	bool EnsureIRThinCustomBaseMaterial(
		UDreamShaderMaterialInstance* Instance,
		UMaterial*& OutBase,
		FDreamShaderError& OutError)
	{
		// Mirrors the persist half of the 1.x EnsureThinCustomBaseMaterial
		// (MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp ~3184, file-static), including
		// its two reuse fallbacks and the RF_Standalone that keeps the base alive across the GC
		// RecompileMaterial trips before the instance parents to it.
		OutBase = nullptr;
		if (!Instance)
		{
			return FailWith(OutError, TEXT("DSH8230"), TEXT("Cannot create a ThinCustom base material without an instance."));
		}

		const FName BaseName(*FString::Printf(TEXT("MB_DreamThinBase_%s"), *Instance->GetName())); /* I18N-EXEMPT: object name, not user-facing text */

		OutBase = FindObject<UMaterial>(Instance, *BaseName.ToString());
		if (!OutBase)
		{
			if (UMaterial* ExistingParent = Cast<UMaterial>(Instance->Parent);
				ExistingParent && ExistingParent->GetOuter() == Instance)
			{
				OutBase = ExistingParent;
			}
		}
		if (!OutBase)
		{
			// RF_Standalone: RecompileMaterial runs a full GC pass and the editor's
			// GARBAGE_COLLECTION_KEEPFLAGS is RF_Standalone alone, so until the instance parents to
			// this base it is only reachable through a local pointer. No RF_Transient: the base must
			// serialize into the instance's saved package as an export.
			OutBase = NewObject<UMaterial>(Instance, BaseName, RF_Public | RF_Standalone);
		}
		if (!OutBase)
		{
			return FailWith(OutError, TEXT("DSH8230"), FString::Printf(TEXT("Failed to create the ThinCustom base material for instance '%s'."), *Instance->GetPathName())); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		return true;
	}

	void PublishGeneratedIRAsset(UObject* GeneratedAsset)
	{
		// Copied from the file-static PublishGeneratedAsset in
		// MaterialAssetGeneration/DreamShaderAssetFactory.cpp (~468). AssetCreated adds the package
		// path to the registry's path tree and broadcasts AssetAdded, which is what makes a brand
		// new folder reachable in the Content Browser at all. The Ephemeral half of the original
		// (AssetViewUtils::OnAlwaysShowPath, to force-show a folder whose only products are
		// Ephemeral) has no 2.0 caller: every generated asset here is Materialized.
		if (!GeneratedAsset)
		{
			return;
		}

		FAssetRegistryModule::AssetCreated(GeneratedAsset);
	}

	void SetDreamShaderAssetMetadata(UObject* Asset, const TCHAR* Key, const FString& Value)
	{
		// Copied from the file-static SetSourceMetadataValue in
		// MaterialAssetGeneration/DreamShaderGeneratedAssetMetadata.cpp (~45).
		if (!Asset)
		{
			return;
		}

		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			return;
		}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		Package->GetMetaData().SetValue(Asset, Key, *Value);
#else
		if (UMetaData* MetaData = Package->GetMetaData())
		{
			MetaData->SetValue(Asset, Key, *Value);
		}
#endif
	}
}
