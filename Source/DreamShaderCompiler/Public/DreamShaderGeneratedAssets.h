// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The asset layer every compile builds on: destinations and the source-root default, create/reuse behind
// the ownership guard, source metadata and the build key, the divergence gate, saving, reflection and
// literal writes, material settings, graph support and layout.
//
// Renamed from MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h and trimmed in the M4
// relocation (research-relocation §2.1). Everything that served only the 1.x graph builder is gone: the
// code tokens and values, FCodeGraphBuilder, the property and literal expression factories, the HLSL
// function codegen. The rest is exported, because the editor module -- bridge, browser, provenance,
// preview, decompiler, tests -- calls it directly.
//
// FTextShaderDefinition and friends (runtime DreamShaderTypes.h) stay in these signatures: the asset
// factory, ApplySettings and the layout still read the 1.x definition shape, which the IR assets adapter
// fills (research-relocation §9 Q2).

#pragma once

#include "CoreMinimal.h"
#include "DreamShaderTypes.h"

// FDreamShaderError: every failure below carries a DSHnnnn code alongside its message, so a wrap as the
// stack unwinds keeps the code the innermost raise set.
#include "DreamShaderDefineTable.h"
#include "DreamShaderDiagnostic.h"

// EDreamShaderDigestState, returned by the provenance helpers declared below.
#include "DreamShaderGeneratedAssetDigest.h"

// EThinCustomPersistence, which CreateOrReuseInstanceMaterial takes.
#include "DreamShaderCompilerInterface.h"

// EBlendMode / EMaterialShadingModel, used by the resolver declarations below. Unity builds happened
// to pull these in through a neighbouring TU; a non-unity compile of this header does not.
#include "Engine/EngineTypes.h"

// EMaterialDomain, used by GetDefaultUsedWithVolumetricCloud below. It lives here rather than in
// SceneTypes.h, and nothing else this header includes pulls it in on a non-unity compile.
#include "MaterialDomain.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "SceneTypes.h"

class UMaterial;
class UMaterialFunction;
class UMaterialExpression;
class UDreamShaderMaterialInstance;
class UClass;
class UEnum;
class FProperty;

namespace UE::DreamShader::Editor::Private
{
	// Estimated Slate-unit footprint of one material graph node. See EstimateMaterialNodeSize.
	struct FLayoutNodeSize
	{
		int32 Width = 0;
		int32 Height = 0;
	};

	DREAMSHADERCOMPILER_API bool TryResolveCustomOutputType(const FString& InTypeName, ECustomMaterialOutputType& OutOutputType);

	/**
	 * Raise an advisory DSHnnnn warning from inside a generation. Logged at Warning level and queued
	 * into the compile result's `Warnings:` block by whichever entry point is outermost; never fails
	 * the compile. The code is spelled `TEXT("DSHnnnn")` at the call site so `.skill/gen-diagnostics.ps1`
	 * can find it, the same way it finds FailWith.
	 *
	 * Defined by the compiler service, which owns the collector (DreamShaderCompilerService.h).
	 */
	DREAMSHADERCOMPILER_API void RaiseGenerationWarning(const TCHAR* Code, const FString& Message);
	DREAMSHADERCOMPILER_API bool ParseScalarLiteral(const FString& InText, double& OutValue);
	DREAMSHADERCOMPILER_API bool ParseBooleanLiteral(const FString& InText, bool& OutValue);
	DREAMSHADERCOMPILER_API bool ParseIntegerLiteral(const FString& InText, int32& OutValue);
	DREAMSHADERCOMPILER_API bool ParseUnsignedInteger32Literal(const FString& InText, uint32& OutValue);
	DREAMSHADERCOMPILER_API FString NormalizeEnumLookupKey(const FString& InKey);
	DREAMSHADERCOMPILER_API bool TryResolveEnumLiteral(UEnum* Enum, const FString& InValue, int64& OutEnumValue);
	DREAMSHADERCOMPILER_API bool ResolveDreamShaderAssetDestination(
		const FString& AssetName,
		const FString& Root,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutAssetLeafName,
		FDreamShaderError& OutError);
	/**
	 * Fills in the `Root` of every block in `Definition` that declared none, when the source file
	 * lives under a plugin source root: the asset then defaults to the plugin that ships the source
	 * instead of to `/Game`. Files under the project root are left alone, as is any block with an
	 * explicit `Root=` — including `Root="/"`, which is how you ask a plugin-root file for `/Game`.
	 *
	 * No-op when the owning plugin cannot host generated content; `OutFallbackReason`, if given,
	 * then explains why the block fell back to `/Game`.
	 *
	 * Must run before ResolveDreamShaderAssetDestination at *every* site that turns a source file
	 * into an asset path, or the Gen page and the preview would name a different asset than the one
	 * generation actually writes.
	 */
	DREAMSHADERCOMPILER_API void ApplyDefaultRootFromSourceFile(
		const FString& SourceFilePath,
		FTextShaderDefinition& Definition,
		FString* OutFallbackReason = nullptr);
	/**
	 * The asset-reference resolver. `ExpectedClass`, when given, is only used to judge the class
	 * written in a `Class'/Game/...'` shell -- a reference that carries no shell, or one whose class
	 * DreamShader cannot resolve, is never rejected on those grounds. Loading and type-checking the
	 * asset itself remains the caller's job.
	 */
	DREAMSHADERCOMPILER_API bool TryResolveDreamShaderAssetReference(
		const FString& InText,
		FString& OutObjectPath,
		FDreamShaderError& OutError,
		const UClass* ExpectedClass = nullptr);
	// Thin wrapper over UMaterialEditingLibrary::CreateMaterialExpressionEx, shared by the emitter, the
	// output reroutes below and graph layout (reroute/comment nodes).
	DREAMSHADERCOMPILER_API UMaterialExpression* CreateOwnedMaterialExpression(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		UClass* ExpressionClass,
		int32 PositionX,
		int32 PositionY);
	// Tearing a graph down is FDreamShaderGraphRollback's job (Private/Assets/DreamShaderGraphRollback.h):
	// it detaches the old nodes instead of destroying them, so a build that fails partway can put them back.
	DREAMSHADERCOMPILER_API void EnsureExpressionCanBeDeleted(UMaterialExpression* Expression);
	DREAMSHADERCOMPILER_API void ClearDreamShaderGeneratedComments(UMaterial* Material, UMaterialFunction* MaterialFunction);
	/**
	 * The name of the named-reroute pair CreateOutputRerouteValue puts in front of an output:
	 * `DS_<Name>_<RouteIndex>`, where <Name> is RouteName trimmed and run through SanitizeIdentifier,
	 * `Output` stands in for a blank RouteName, and a negative RouteIndex drops the `_<RouteIndex>`
	 * suffix. Exposed as the one spelling of the rule: the emitter names every output reroute with it,
	 * and the graph importer (M5) recognises a generated reroute by it.
	 */
	DREAMSHADERCOMPILER_API FString MakeDreamShaderOutputRerouteName(const FString& RouteName, int32 RouteIndex);
	/**
	 * Puts a named reroute -- a declaration named by MakeDreamShaderOutputRerouteName and a usage bound to
	 * it -- between an output value and the pin it drives, so that the pin reads as the output's own name
	 * and the wire from wherever the value was computed does not cross the whole canvas. The declaration's
	 * input is connected to (SourceExpression, SourceOutputIndex) with no inline mask.
	 *
	 * Returns true with OutExpression = the usage node and OutOutputIndex = 0: what the consumer connects
	 * to instead of the source. Returns false, with OutExpression / OutOutputIndex set to the source
	 * itself, when there is no source expression, no owning Material or MaterialFunction, or the two
	 * reroute nodes could not be created; the caller then wires the source directly.
	 */
	DREAMSHADERCOMPILER_API bool CreateOutputRerouteValue(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		UMaterialExpression* SourceExpression,
		int32 SourceOutputIndex,
		const FString& RouteName,
		int32 RouteIndex,
		UMaterialExpression*& OutExpression,
		int32& OutOutputIndex);
	// The footprint the layout pass places a node by. There is no widget to measure at generation
	// time, so this is derived from what SGraphNodeMaterialBase assembles -- title bar, one row per
	// pin, and the expression preview when ShouldShowPreview(). Errs high on purpose: over-estimating
	// only loosens the graph, under-estimating overlaps nodes. Exposed so a layout test can assert
	// against the same measure the placement used.
	DREAMSHADERCOMPILER_API FLayoutNodeSize EstimateMaterialNodeSize(UMaterialExpression* Expression);
	DREAMSHADERCOMPILER_API void LayoutGeneratedExpressions(UMaterial* Material, UMaterialFunction* MaterialFunction);
	// RegionByVariable names the region a node was made in. A region nested in another is named by its whole path, the
	// names joined by GDreamShaderRegionPathSeparator from the outermost in: once everything is placed, the layout
	// draws one comment box per region around its nodes, each inside its parent's. Regions move no node.
	inline const TCHAR* const GDreamShaderRegionPathSeparator = TEXT("\x1f");
	// bQuiet drops the per-node slow-task frames. An interactive compile lays out on every save, where
	// formatting one progress string per node costs more than the placement itself.
	DREAMSHADERCOMPILER_API void LayoutGeneratedExpressions(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const FTextShaderLayout* Layout,
		const TMap<FString, UMaterialExpression*>* ExpressionsByVariable,
		const TMap<FString, FString>* RegionByVariable,
		bool bQuiet);
	DREAMSHADERCOMPILER_API void ResetMaterialToDefaults(UMaterial* Material);
	/**
	 * The value ApplySettings gives bUsedWithVolumetricCloud when the source's Settings block does
	 * not name it. Decompile calls this too, so that the value a generated material would get
	 * anyway is the one it may leave out of the Settings block it writes.
	 */
	DREAMSHADERCOMPILER_API bool GetDefaultUsedWithVolumetricCloud(const EMaterialDomain Domain);
	DREAMSHADERCOMPILER_API bool ValidateSettings(const FTextShaderDefinition& Definition, FDreamShaderError& OutError);
	DREAMSHADERCOMPILER_API bool ApplySettings(UMaterial* Material, const FTextShaderDefinition& Definition, FDreamShaderError& OutError);
	/**
	 * Creates or reuses the asset a product lands on. A Graph material and a material function always
	 * persist, so these two have no persistence parameter: the 1.x `bTransient` flag went with the
	 * transient Graph build.
	 */
	DREAMSHADERCOMPILER_API bool CreateOrReuseMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FDreamShaderError& OutError);
	DREAMSHADERCOMPILER_API bool CreateOrReuseMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FDreamShaderError& OutError);
	/**
	 * The ThinCustom instance. Persistence is the compile request's, forwarded; no default, so every
	 * caller states it. An instance whose package already exists on disk stays Materialized whatever the
	 * request asks for (storage decides, see EThinCustomPersistence).
	 */
	DREAMSHADERCOMPILER_API bool CreateOrReuseInstanceMaterial(
		const FTextShaderDefinition& Definition,
		::UDreamShaderMaterialInstance*& OutInstance,
		FDreamShaderError& OutError,
		::UE::DreamShader::EThinCustomPersistence Persistence);
	/**
	 * The regeneration stamp: hashes the prepared source together with everything else that decides
	 * what that source compiles into.
	 *
	 * TouchedDefines is not optional and has no default. It is what the preprocessor read over the file
	 * and every header (FDreamShaderLang2PipelineResult::TouchedDefines), and passing an empty map is not
	 * "I have nothing to add" -- it is a positive claim that the source read no preprocessor defines,
	 * which for a conditional source pins its hash to a value that never moves when its defines do. That
	 * is a silently stale asset, so the argument is required and every caller has to have gone and got it.
	 */
	DREAMSHADERCOMPILER_API FString BuildSourceHash(const FString& SourceText, const UE::DreamShader::FDreamShaderDefineValueMap& TouchedDefines);
	DREAMSHADERCOMPILER_API bool IsGeneratedAssetSourceCurrent(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash);
	DREAMSHADERCOMPILER_API bool HasDreamShaderSourceMetadata(UObject* Asset);
	DREAMSHADERCOMPILER_API void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath);
	DREAMSHADERCOMPILER_API void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash);
	/**
	 * The project-relative, forward-slashed spelling of a source path: what DreamShader.SourceFile stamps, and
	 * what the pipeline hands the IR builder as FIRBuildOptions::StampSourcePath (debt B5), so neither the
	 * metadata nor a Custom node's code changes with the machine or the checkout. A file outside the project
	 * directory keeps its absolute path. Readers resolve a relative answer against the project directory.
	 */
	DREAMSHADERCOMPILER_API FString MakeProjectRelativeSourcePath(const FString& SourceFilePath);
	/**
	 * The second rebuild precondition, alongside CheckGeneratedAssetNotDiverged.
	 *
	 * An open asset editor does not edit the asset: FMaterialEditor duplicates it into a transient
	 * UPreviewMaterial and copies that duplicate back on Apply or Save (UpdateOriginalMaterial), and
	 * the material instance editor writes back through a UMaterialEditorInstanceConstant wrapper. So
	 * an editor that was open across a rebuild holds a pre-rebuild copy, and the next Apply silently
	 * reverts everything the rebuild did. Refusing is the only safe answer available: whether that
	 * copy has unsaved edits in it is FMaterialEditor::bMaterialDirty, which is private to the
	 * MaterialEditor module -- so "close it if it is clean" is not a question this plugin can ask, and
	 * closing it blindly would pop a save prompt in the middle of a compile-on-save.
	 */
	DREAMSHADERCOMPILER_API bool IsGeneratedAssetOpenInEditor(UObject* Asset);
	DREAMSHADERCOMPILER_API bool CheckGeneratedAssetNotOpenInEditor(UObject* Asset, FDreamShaderError& OutError);

	/**
	 * Whether this process is allowed to write generated assets to disk.
	 *
	 * Only one process per project may, and the bridge's ownership lock is what decides which. Two
	 * editors open on the same project would otherwise both rebuild and both SavePackage the same file
	 * -- and a save that loses that race is not a merge, it is a corrupted package or a dead editor.
	 * True by default, because a commandlet has no bridge, no second process, and exactly one job.
	 */
	DREAMSHADERCOMPILER_API void SetMayWriteGeneratedAssetsToDisk(bool bMayWrite);
	DREAMSHADERCOMPILER_API bool MayWriteGeneratedAssetsToDisk();
	/**
	 * True when a compile that would land on disk has to leave the asset alone because another process
	 * owns writing it. Reported as a skip rather than a failure: a second editor is a legitimate way to
	 * work, its own unsaved materials still compile, and only the shared file is off limits.
	 */
	DREAMSHADERCOMPILER_API bool ShouldDeferPersistedAssetToWriteOwner(UObject* Asset, bool bWouldPersist, FDreamShaderError& OutMessage);

	// Whether the asset has a file behind it. The generation paths ask this right after creating or
	// reusing their target and downgrade an Ephemeral request to a Materialized one when it answers yes:
	// where the asset lives decides how it is rebuilt, so there is never a memory-only copy of an
	// on-disk asset disagreeing with the file underneath it.
	DREAMSHADERCOMPILER_API bool IsGeneratedAssetPersisted(UObject* Asset);
	// The project-relative source path stamped at generation time -- the asset's answer to "which file
	// am I built from", which the Adopt action needs in order to know what to rewrite.
	DREAMSHADERCOMPILER_API FString GetGeneratedAssetSourceFile(UObject* Asset);
	// The stamped source hash. Only a persisted build writes one -- a memory-only build stamps the path
	// alone, deliberately, so the skip check stays off -- which makes this the plainest reading of
	// which of the two paths a compile actually took.
	DREAMSHADERCOMPILER_API FString GetGeneratedAssetSourceHash(UObject* Asset);
	// Content fingerprint bookkeeping. See DreamShaderGeneratedAssetDigest.h: the source hash says
	// whether the SOURCE moved, these say whether the ASSET did.
	DREAMSHADERCOMPILER_API FString GetOutputDigestMetadata(UObject* Asset);
	DREAMSHADERCOMPILER_API void ApplyOutputDigestMetadata(UObject* Asset);
	DREAMSHADERCOMPILER_API EDreamShaderDigestState ClassifyGeneratedAsset(UObject* Asset);
	// Detach: drops every DreamShader stamp, which makes the asset Foreign and takes it out of the
	// generator's hands for good (the ownership guard refuses to touch it afterwards).
	DREAMSHADERCOMPILER_API void ClearDreamShaderMetadata(UObject* Asset);
	// The divergence gate. Returns false -- with a message naming the three ways out -- when the asset
	// no longer holds what DreamShader last generated into it. Must be called BEFORE anything clears
	// the graph; that ordering is the whole point.
	//
	// Note what it does NOT take: a force flag. See FScopedDreamShaderRevertDiverged.
	DREAMSHADERCOMPILER_API bool CheckGeneratedAssetNotDiverged(UObject* Asset, FDreamShaderError& OutError);

	/**
	 * The one thing that lets a compile overwrite a hand-edited asset. Held by the Revert action for
	 * the duration of the rebuild it asked for, and by nothing else.
	 *
	 * Kept separate from bForce on purpose. bForce answers "is the source hash stale", which the
	 * editor's startup sweep asserts unconditionally for every file it regenerates in memory; if the
	 * gate honoured it, every restart would quietly rebuild over saved hand edits -- the exact failure
	 * the gate exists to stop, in the mode the editor spends all its time in.
	 */
	struct DREAMSHADERCOMPILER_API FScopedDreamShaderRevertDiverged
	{
		FScopedDreamShaderRevertDiverged();
		~FScopedDreamShaderRevertDiverged();

		FScopedDreamShaderRevertDiverged(const FScopedDreamShaderRevertDiverged&) = delete;
		FScopedDreamShaderRevertDiverged& operator=(const FScopedDreamShaderRevertDiverged&) = delete;
	};
	DREAMSHADERCOMPILER_API bool IsRevertingDivergedAssets();
	DREAMSHADERCOMPILER_API bool SaveAssetPackage(UObject* Asset, FDreamShaderError& OutError);
	DREAMSHADERCOMPILER_API bool SaveAssetPackages(const TArray<UObject*>& Assets, FDreamShaderError& OutError);
	DREAMSHADERCOMPILER_API UClass* ResolveMaterialExpressionClass(const FString& ClassSpecifier);
	DREAMSHADERCOMPILER_API FProperty* FindMaterialExpressionArgumentProperty(UClass* ExpressionClass, const FString& ArgumentName);
	DREAMSHADERCOMPILER_API bool IsMaterialExpressionInputProperty(const FProperty* Property);
	DREAMSHADERCOMPILER_API bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, const FString& ValueText, FDreamShaderError& OutError);
	DREAMSHADERCOMPILER_API bool SetMaterialExpressionLiteralProperty(UObject* Target, FProperty* Property, void* ValuePtr, const FString& ValueText, FDreamShaderError& OutError);
}
