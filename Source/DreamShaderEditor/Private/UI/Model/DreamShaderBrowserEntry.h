// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Material Content Browser's unit of display: one entry per thing the browser can show, with a
// source half (a .dsm/.dsf/.dsh on disk) and an asset half (a UMaterialInterface / UMaterialFunction
// in the project), either of which may be absent. A source that has never been compiled has no asset
// half; a hand-authored material DreamShader never generated has no source half; a generated material
// has both, joined through the DreamShader.SourceFile stamp. The inspector renders whichever halves are
// present, which is what lets one panel serve the source-centric and the asset-centric views alike.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "MaterialAssetGeneration/DreamShaderGeneratedAssetDigest.h"

class UMaterialInterface;
class UObject;

namespace UE::DreamShader::Editor::Private
{
	enum class EBrowserSourceKind : uint8
	{
		Material, // .dsm
		Function, // .dsf
		Header,   // .dsh
	};

	// The source half's compile status. Library kinds (.dsf / .dsh) carry `Library`; the other values
	// describe the relationship between a .dsm and the asset it resolves to.
	enum class EBrowserSourceStatus : uint8
	{
		NotCompiled,       // no object at the resolved object path
		UpToDate,          // asset exists and its stamped source hash matches the current source
		Stale,             // asset exists but its stamped source hash differs from the current source
		InMemoryUntracked, // asset exists in memory and carries no source hash -- a memory-only build
		                   // deliberately stamps the path alone, so currency cannot be judged; this is
		                   // NOT stale, it is "compiled, freshness unknown"
		Error,             // the last compile (from this browser, or per the bridge's diagnostics) failed
		Library,           // .dsf / .dsh: a function library or header, not a top-level material
		Unresolved,        // could not read or parse the source, or it declares no top-level block
	};

	enum class EBrowserStorage : uint8
	{
		OnDisk,
		InMemory, // PKG_NewlyCreated: materializing would move it to disk
	};

	struct FBrowserSourceInfo
	{
		FString FilePath;        // absolute, normalized
		FString DisplayName;     // clean filename with extension
		FString RootDisplayName; // owning source root; empty for the project root
		bool bWritableRoot = true;
		EBrowserSourceKind Kind = EBrowserSourceKind::Material;
		EBrowserSourceStatus Status = EBrowserSourceStatus::NotCompiled;
		FText StatusDetail;      // error / parse message, or the object path
		FString ResolvedObjectPath; // where a .dsm compiles to; empty when unresolved or a library
		// Every diagnostic the bridge holds for this file, not just the first.
		TArray<FDreamShaderDiagnosticRecord> Diagnostics;
		// For libraries: the materials that import this file (absolute source paths).
		TArray<FString> Dependents;
		// For materials: the headers and functions this file imports, transitively (absolute paths).
		TArray<FString> Imports;

		bool IsLibrary() const { return Kind != EBrowserSourceKind::Material; }
	};

	struct FBrowserAssetInfo
	{
		FAssetData AssetData;
		FString ObjectPath;
		FString MountPoint; // "/Game", "/PluginName"
		EBrowserStorage Storage = EBrowserStorage::OnDisk;
		EDreamShaderDigestState Provenance = EDreamShaderDigestState::Foreign;
		// Open in an asset editor: a rebuild refuses while it is, because the editor holds a
		// pre-rebuild copy that its next Apply would write back.
		bool bOpenInEditor = false;
		bool bIsInstance = false;
		// Described from the asset registry alone, without loading the asset: storage is exact, the
		// provenance is provisional (Foreign until the object is loaded and its stamps can be read).
		bool bFromRegistryOnly = false;
	};

	struct FBrowserEntry
	{
		// Stable identity: the source path when there is one, else the asset object path.
		FString Key;
		TOptional<FBrowserSourceInfo> Source;
		TOptional<FBrowserAssetInfo> Asset;

		FString GetDisplayName() const;
		bool IsLibrary() const { return Source.IsSet() && Source->IsLibrary(); }
		// A material in the project that no scanned source generates: hand-authored, or an orphan whose
		// source is gone. Listed so the browser covers every material, not only DreamShader's.
		bool IsUnmanaged() const { return !Source.IsSet() && Asset.IsSet(); }
		FString GetObjectPath() const;
		// Loads (or finds) the material this entry stands for. Null for libraries and for sources
		// that have not been compiled.
		UMaterialInterface* ResolveMaterial() const;
	};
}
