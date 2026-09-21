#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceConstant;

namespace UE::DreamShader::Editor::Private
{
	struct FCreateInstanceResult
	{
		bool bSucceeded = false;
		FString Error;
		UMaterialInstanceConstant* Instance = nullptr;
	};

	// Creates a persisted UMaterialInstanceConstant parented to Parent. If Parent is a memory-only
	// DreamShader material, it is first materialized to disk (its transient base can't be a parent import),
	// so the child never references a transient object. The child shares the root's compiled shader map
	// (UDreamShaderMaterialInstance::HasOverridenBaseProperties), so many variants cost one shader map.
	// PackagePath is the destination folder (e.g. "/Game/Foo/Instances"); AssetName the leaf.
	// This is the UNMANAGED instance: no source file describes it. For a DreamShader parent the
	// browser writes a `.dsi` instead (CreateDreamShaderInstanceSource) and offers this as the
	// secondary action.
	FCreateInstanceResult CreateDreamShaderMaterialInstance(
		UMaterialInterface* Parent,
		const FString& AssetName,
		const FString& PackagePath,
		bool bOpenAfterCreate);

	// Computes the default destination for a new instance of Parent: <parentDir>/<InstanceSubfolder> and a
	// unique MI_<parentLeaf>_<n> asset name.
	void GetDefaultInstanceDestination(UMaterialInterface* Parent, FString& OutPackagePath, FString& OutAssetName);

	// Opens a modal dialog to configure and create a new (unmanaged) instance of Parent.
	void OpenCreateInstanceDialog(UMaterialInterface* Parent);

	struct FCreateInstanceSourceResult
	{
		bool bSucceeded = false;
		FString Error;
		// The `.dsi` that was written; set even when its compile failed, so the caller can open it at the error.
		FString SourceFilePath;
		// The compiled material instance; null when nothing was compiled.
		UMaterialInstanceConstant* Instance = nullptr;
	};

	// True when Parent is a DreamShader product -- it carries a source stamp that resolves to a compilable
	// source (`.dss`, `.dsi`, `.dsm`, `.dsf`) -- so an instance of it is written as a `.dsi` (CONTRACT
	// section 2.3, Material Browser Create instance).
	bool IsDreamShaderInstanceSourceParent(UMaterialInterface* Parent);

	// The default `.dsi` for an instance of Parent: <the parent source's folder>/<InstanceSubfolder> (the
	// project's DShader root instead when that folder is read-only), and a stem MI_<parentLeaf>[_n] that no
	// existing file takes.
	void GetDefaultInstanceSourceDestination(UMaterialInterface* Parent, FString& OutDirectory, FString& OutFileStem);

	// Writes <Directory>/<FileStem>.dsi from Resources/Templates/NewInstance.dsi with Parent = the parent's
	// path, compiles it (forced, through the bridge when there is one, so the diagnostics store follows), and
	// opens the instance when asked. Refuses what CreateNewSourceFile refuses.
	FCreateInstanceSourceResult CreateDreamShaderInstanceSource(
		UMaterialInterface* Parent,
		const FString& Directory,
		const FString& FileStem,
		bool bOpenAfterCreate);

	// Opens a modal dialog to write and compile a `.dsi` instance of Parent, with the unmanaged instance
	// (OpenCreateInstanceDialog) as its secondary action.
	void OpenCreateInstanceSourceDialog(UMaterialInterface* Parent);

	// Create instance as the Material Content Browser and the Content Browser offer it: the `.dsi` dialog for
	// a DreamShader product, the unmanaged-instance dialog for anything else.
	void OpenCreateInstanceDialogForParent(UMaterialInterface* Parent);
}
