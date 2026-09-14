// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The top of the emit: destination, create/reuse, the three gates, the atomic rollback, the graph
// walk, settings, layout, metadata, save, publish.
//
// The order below is not arbitrary and matches the 1.x generator's step for step, because every
// step of it exists to stop a specific way a rebuild used to lose work:
//
//   destination + create/reuse   -- the ownership guard refuses a saved asset we did not generate
//   source-hash skip             -- an unchanged source does not touch the asset at all
//   write-owner check            -- a second editor open on the same project does not race the save
//   open-in-editor gate          -- an open asset editor would write its pre-rebuild copy back
//   divergence gate              -- a hand-edited asset is refused rather than silently overwritten
//   clear comments               -- BEFORE the snapshot, or it would capture dead comment pointers
//   arm the rollback             -- from here every failure restores the graph instead of emptying it
//   ... build ...
//   commit                       -- the old graph is finally expendable; nothing below can fail
//   recompile / save / publish
//
// Diagnostics: DSH8200-8289.

#include "Compiler/DreamShaderIREmitter.h"

#include "Compiler/DreamShaderIRAssets.h"
#include "Compiler/DreamShaderIREmitterInternal.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "MaterialAssetGeneration/DreamShaderGraphRollback.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "MaterialAssetGeneration/DreamShaderThinCustomParameterOverrides.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialFunction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "DreamShader.Emitter"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		Lang::FLangSpan SpanOf(const IR::FIRSourceRef& Source)
		{
			return Source.Span;
		}

		/** `DSH8101: Asset ... is open in an asset editor` -- the 1.x code kept inside the 2.0 message. */
		FText WrapLegacyError(const FDreamShaderError& Error)
		{
			if (Error.HasCode())
			{
				return FText::FromString(FString::Printf(TEXT("%s: %s"), *Error.Code, *Error.Message)); /* I18N-EXEMPT: quotes a 1.x generator message verbatim */
			}
			return FText::FromString(Error.Message);
		}

		/**
		 * Pin ids of the FunctionInput / FunctionOutput nodes on the live asset.
		 *
		 * Copied from the file-static CacheMaterialFunctionInterfaceIds in
		 * MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp (~1299). Must run BEFORE the
		 * rollback is armed: the rollback detaches those nodes, and caching afterwards would find an
		 * empty graph and lose every call site's wiring (UMaterialExpressionMaterialFunctionCall
		 * matches its stored input GUIDs first and falls back to declaration order only when they
		 * do not resolve).
		 */
		void CacheInterfaceIds(
			const UMaterialFunction* Function,
			TMap<FName, FGuid>& OutInputIdsByName,
			TMap<FName, FGuid>& OutOutputIdsByName)
		{
			OutInputIdsByName.Reset();
			OutOutputIdsByName.Reset();
			if (!Function)
			{
				return;
			}

			for (UMaterialExpression* Expression : Function->GetExpressions())
			{
				if (const UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expression))
				{
					if (!Input->InputName.IsNone() && Input->Id.IsValid())
					{
						OutInputIdsByName.Add(Input->InputName, Input->Id);
					}
				}
				else if (const UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expression))
				{
					if (!Output->OutputName.IsNone() && Output->Id.IsValid())
					{
						OutOutputIdsByName.Add(Output->OutputName, Output->Id);
					}
				}
			}
		}

		/** `#pragma layout(...)` hints in the shape the 1.x layout pass reads them. */
		void BuildLayoutFromHints(const IR::FIRGraph& Graph, FTextShaderLayout& OutLayout)
		{
			for (const IR::FIRLayoutHint& Hint : Graph.LayoutHints)
			{
				if (Hint.Kind.Equals(TEXT("Comment"), ESearchCase::CaseSensitive))
				{
					FTextShaderLayoutComment Comment;
					Comment.Name = Hint.Name;
					Comment.X = Hint.X;
					Comment.Y = Hint.Y;
					if (Hint.bHasSize)
					{
						Comment.W = Hint.W;
						Comment.H = Hint.H;
					}
					OutLayout.Comments.Add(MoveTemp(Comment));
					continue;
				}

				FTextShaderLayoutNode Node;
				Node.Var = Hint.Var;
				Node.X = Hint.X;
				Node.Y = Hint.Y;
				OutLayout.Nodes.Add(MoveTemp(Node));
			}
		}

		/**
		 * The three preconditions a rebuild has to pass, in the order the 1.x generator asks them.
		 *
		 * Answers false with a diagnostic when the asset must not be touched. bOutSkip is set when
		 * there is nothing to do and that is a success: the source hash is current, or another
		 * editor owns writing this project's generated assets. Each skip records an Info at the
		 * product's span -- DSH8237 for the hash, DSH8209 for the write owner -- and that Info is how
		 * the driver tells a skip from a build: EmitDreamShaderIRProduct answers true for all three,
		 * and its signature has no other channel. Keep each code on its one branch.
		 *
		 * bAllowHashSkip is how a caller says the hash is not the only question. A material function
		 * whose asset carries the wrong usage has to be rebuilt even from an unchanged source, and
		 * the gates below must still run for that rebuild -- taking the hash skip first would walk
		 * past the open-editor and divergence checks on the way to editing the asset anyway.
		 */
		bool CheckRebuildPreconditions(
			UObject* Asset,
			const FIREmitContext& Context,
			const IR::FIRProduct& Product,
			const bool bAllowHashSkip,
			Lang::FLangDiagnosticSink& Diagnostics,
			bool& bOutSkip)
		{
			bOutSkip = false;

			FDreamShaderError DeferMessage;
			if (Private::ShouldDeferPersistedAssetToWriteOwner(Asset, /*bWouldPersist*/ true, DeferMessage))
			{
				// Info, not an error: a second editor open on the same project is a legitimate way to
				// work, and only the shared file is off limits. Recorded rather than logged alone so
				// the driver can tell a skip from a build in the diagnostics store.
				Diagnostics.Info(TEXT("DSH8209"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("DeferredToWriteOwner", "'{0}' was left alone: another editor owns writing this project's generated assets to disk."),
					FText::FromString(Asset->GetPathName())));
				bOutSkip = true;
				return true;
			}

			if (bAllowHashSkip && !Context.bForce && Private::IsGeneratedAssetSourceCurrent(Asset, Context.SourceFilePath, Context.SourceHash))
			{
				// Info, like the deferral above, and read by the driver the same way: it is what turns
				// this product's result line into 1.x's "Skipped ...; source hash is unchanged" rather
				// than a "Generated" for a build that never happened.
				Diagnostics.Info(TEXT("DSH8237"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("SourceHashCurrent", "'{0}' was left alone: its source hash is unchanged since it was last built."),
					FText::FromString(Asset->GetPathName())));
				bOutSkip = true;
				return true;
			}

			// Before anything edits the asset: a function that fails a gate must come out of the
			// compile completely untouched, because its call sites read their pins from the live
			// asset and a half-updated function breaks all of them.
			FDreamShaderError EditorOpenError;
			if (!Private::CheckGeneratedAssetNotOpenInEditor(Asset, EditorOpenError))
			{
				Diagnostics.Error(TEXT("DSH8206"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("AssetOpenInEditor", "'{0}' is open in an asset editor, so it was not rebuilt. {1}"),
					FText::FromString(Asset->GetPathName()),
					WrapLegacyError(EditorOpenError)));
				return false;
			}

			FDreamShaderError DivergenceError;
			if (!Private::CheckGeneratedAssetNotDiverged(Asset, DivergenceError))
			{
				Diagnostics.Error(TEXT("DSH8207"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("AssetDiverged", "'{0}' no longer holds what DreamShader generated into it, so it was not rebuilt. {1}"),
					FText::FromString(Asset->GetPathName()),
					WrapLegacyError(DivergenceError)));
				return false;
			}

			return true;
		}

		/**
		 * Builds one product's graph onto a UMaterial and leaves the material recompiled.
		 *
		 * The Graph backend calls this on the visible material; the ThinCustom backend calls it on
		 * the hidden base. Identical either way, which is the whole point of the 1.x
		 * PopulateMaterialGraphFromDefinition this mirrors: there is one graph construction, and the
		 * backend only decides what it is built onto.
		 *
		 * Callers own creation, the gates, metadata and persistence.
		 */
		bool PopulateMaterialFromProduct(
			UMaterial* Material,
			const FTextShaderDefinition& Definition,
			const IR::FIRProduct& Product,
			const FIREmitContext& Context,
			Lang::FLangDiagnosticSink& Diagnostics,
			TMap<FGuid, IR::FIRSourceRef>& OutSpans)
		{
			Material->Modify();

			// Ahead of the snapshot, and it must stay there: this destroys the generated comment
			// boxes, so a snapshot taken first would capture pointers to comments already garbage.
			Private::ClearDreamShaderGeneratedComments(Material, nullptr);

			// Everything from here until Commit() is reversible. Every `return false` below rolls
			// the material back to what it held on the line above.
			Private::FDreamShaderGraphRollback Rollback(Material);
			if (!Rollback.IsArmed())
			{
				Diagnostics.Warning(TEXT("DSH8208"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("RollbackNotArmed", "'{0}' could not be snapshotted before rebuilding it, so a failed rebuild will not be rolled back."),
					FText::FromString(Material->GetPathName())));
			}

			Private::ResetMaterialToDefaults(Material);

			FDreamShaderError SettingsError;
			if (!Private::ApplySettings(Material, Definition, SettingsError))
			{
				Diagnostics.Error(TEXT("DSH8215"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("SettingsRefused", "A material setting on '{0}' was refused. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(SettingsError)));
				return false;
			}

			FIREmitter Emitter(Material, nullptr, Product, Context, Diagnostics);
			if (!Emitter.EmitGraph())
			{
				return false;
			}

			FTextShaderLayout Layout;
			BuildLayoutFromHints(Product.Graph, Layout);
			const TMap<FString, UMaterialExpression*>& ExpressionsByVariable = Emitter.GetExpressionsByVariable();
			const TMap<FString, FString>& RegionByVariable = Emitter.GetRegionByVariable();
			Private::LayoutGeneratedExpressions(
				Material,
				nullptr,
				&Layout,
				ExpressionsByVariable.IsEmpty() ? nullptr : &ExpressionsByVariable,
				RegionByVariable.IsEmpty() ? nullptr : &RegionByVariable,
				/*bQuiet*/ false);

			OutSpans = Emitter.GetSourceSpans();

			// The graph is complete and nothing below can fail, so the old one is finally
			// expendable. Before the recompile, not after: holding a detached copy of the old graph
			// across the compile would keep every node of it alive for no reason.
			Rollback.Commit();

			UMaterialEditingLibrary::RecompileMaterial(Material);
			Material->PostEditChange();
			return true;
		}

		/** The Graph backend: one visible UMaterial, saved. */
		bool EmitMaterialProduct(
			const IR::FIRProduct& Product,
			const FTextShaderDefinition& Definition,
			const FIREmitContext& Context,
			UObject*& OutAsset,
			Lang::FLangDiagnosticSink& Diagnostics)
		{
			UMaterial* Material = nullptr;
			FDreamShaderError CreateError;
			if (!CreateOrReuseIRMaterial(Definition, Material, CreateError) || !Material)
			{
				Diagnostics.Error(TEXT("DSH8201"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("CreateMaterialFailed", "The material for '{0}' could not be created or reused. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(CreateError)));
				return false;
			}

			OutAsset = Material;

			bool bSkip = false;
			if (!CheckRebuildPreconditions(Material, Context, Product, /*bAllowHashSkip*/ true, Diagnostics, bSkip))
			{
				return false;
			}
			if (bSkip)
			{
				return true;
			}

			TMap<FGuid, IR::FIRSourceRef> Spans;
			if (!PopulateMaterialFromProduct(Material, Definition, Product, Context, Diagnostics, Spans))
			{
				return false;
			}

			Private::ApplySourceMetadata(Material, Context.SourceFilePath, Context.SourceHash);
			Private::ApplyOutputDigestMetadata(Material);
			WriteDreamShaderSourceSpans(Material, Spans);
			Material->MarkPackageDirty();

			FDreamShaderError SaveError;
			if (!Private::SaveAssetPackage(Material, SaveError))
			{
				Diagnostics.Error(TEXT("DSH8229"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("SaveMaterialFailed", "'{0}' was built but could not be saved. {1}"),
					FText::FromString(Material->GetPathName()),
					WrapLegacyError(SaveError)));
				return false;
			}

			PublishGeneratedIRAsset(Material);
			return true;
		}

		/**
		 * The ThinCustom backend: the graph goes onto a hidden base UMaterial that is a subobject of
		 * a UDreamShaderMaterialInstance, and the instance is the addressable asset.
		 *
		 * Mirrors the 1.x GenerateThinCustomMaterialAsInstance
		 * (MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp ~3227) with the transient half
		 * removed. The order of the four lines around the restore is the part that matters: capture
		 * before anything touches the base, clear after reparenting, restore between the clear and
		 * UpdateStaticPermutation, and stamp the digest only after PostEditChange has settled the
		 * static parameter set the digest reads.
		 */
		bool EmitThinCustomMaterialProduct(
			const IR::FIRProduct& Product,
			const FTextShaderDefinition& Definition,
			const FIREmitContext& Context,
			UObject*& OutAsset,
			Lang::FLangDiagnosticSink& Diagnostics)
		{
			UDreamShaderMaterialInstance* Instance = nullptr;
			FDreamShaderError CreateError;
			if (!CreateOrReuseIRThinCustomInstance(Definition, Instance, CreateError) || !Instance)
			{
				Diagnostics.Error(TEXT("DSH8203"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("CreateInstanceFailed", "The ThinCustom instance for '{0}' could not be created or reused. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(CreateError)));
				return false;
			}

			OutAsset = Instance;

			// One gate for the pair: the base is a subobject of the instance, so the instance's
			// digest covers both the graph and the instance's own overrides.
			bool bSkip = false;
			if (!CheckRebuildPreconditions(Instance, Context, Product, /*bAllowHashSkip*/ true, Diagnostics, bSkip))
			{
				return false;
			}
			if (bSkip)
			{
				return true;
			}

			// The user's tuning, read while the old parameter set is still live -- before anything
			// touches the base, because the rebuild both tears the base's graph down and clears
			// every override off the instance below.
			Private::FDreamShaderCapturedParameterOverrides CapturedOverrides;
			Private::CaptureThinCustomParameterOverrides(Instance, CapturedOverrides);

			// After the skip check on purpose: a hash-skip must not create a base.
			UMaterial* BaseMaterial = nullptr;
			FDreamShaderError BaseError;
			if (!EnsureIRThinCustomBaseMaterial(Instance, BaseMaterial, BaseError) || !BaseMaterial)
			{
				Diagnostics.Error(TEXT("DSH8230"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("CreateThinBaseFailed", "The hidden base material for '{0}' could not be created. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(BaseError)));
				return false;
			}

			TMap<FGuid, IR::FIRSourceRef> Spans;
			if (!PopulateMaterialFromProduct(BaseMaterial, Definition, Product, Context, Diagnostics, Spans))
			{
				return false;
			}

			// Deferred recache: the single shader recache happens in UpdateStaticPermutation below,
			// once the parent's graph is in place.
			Instance->SetParentEditorOnly(BaseMaterial, /*RecacheShader*/ false);
			Instance->ClearParameterValuesEditorOnly();
			Instance->SourceFilePath = Context.SourceFilePath;
			Instance->SourceHash = Context.SourceHash;

			// Between the clear and the permutation update on purpose: the clear is what the restore
			// undoes, and the permutation update is what turns a restored static switch into the
			// matching shader map. Names the source dropped are reported by the restore itself.
			Private::RestoreThinCustomParameterOverrides(Instance, BaseMaterial, CapturedOverrides, Context.SourceFilePath);

			Instance->UpdateStaticPermutation();
			Instance->PostEditChange();

			// The base is stamped too, only to keep the "generated objects carry source metadata"
			// invariant; the instance is the on-disk ownership anchor.
			Private::ApplySourceMetadata(BaseMaterial, Context.SourceFilePath, Context.SourceHash);
			Private::ApplySourceMetadata(Instance, Context.SourceFilePath, Context.SourceHash);
			// After UpdateStaticPermutation and PostEditChange: those settle the static parameter set
			// the digest reads, so stamping ahead of them would fingerprint a state the asset is not
			// left in.
			Private::ApplyOutputDigestMetadata(Instance);
			WriteDreamShaderSourceSpans(BaseMaterial, Spans);

			Instance->MarkPackageDirty();

			// The editor save path drops any package UPackage::IsEmptyPackage() reports as empty,
			// and that counts only objects whose IsAsset() is true. While PKG_NewlyCreated is set,
			// UDreamShaderMaterialInstance::IsAsset() is false and the base is a non-asset
			// subobject, so the package looks empty and is silently skipped. Clearing the flag is
			// therefore the very last step before saving -- and is put back if the save fails, so a
			// failed persist does not leave a memory-only instance masquerading as saved.
			UPackage* InstancePackage = Instance->GetOutermost();
			const bool bWasNewlyCreated = InstancePackage && InstancePackage->HasAnyPackageFlags(PKG_NewlyCreated);
			if (InstancePackage)
			{
				InstancePackage->ClearPackageFlags(PKG_NewlyCreated);
			}

			FDreamShaderError SaveError;
			if (!Private::SaveAssetPackages({ Instance }, SaveError))
			{
				if (bWasNewlyCreated && InstancePackage)
				{
					InstancePackage->SetPackageFlags(PKG_NewlyCreated);
				}
				Diagnostics.Error(TEXT("DSH8229"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("SaveInstanceFailed", "'{0}' was built but could not be saved. {1}"),
					FText::FromString(Instance->GetPathName()),
					WrapLegacyError(SaveError)));
				return false;
			}

			if (bWasNewlyCreated)
			{
				PublishGeneratedIRAsset(Instance);
			}
			return true;
		}

		/** MaterialFunction / MaterialLayer / MaterialLayerBlend: one UMaterialFunction asset, saved. */
		bool EmitFunctionProduct(
			const IR::FIRProduct& Product,
			const FTextShaderDefinition& Definition,
			const FIREmitContext& Context,
			UObject*& OutAsset,
			Lang::FLangDiagnosticSink& Diagnostics)
		{
			FTextShaderMaterialFunctionDefinition FunctionDefinition;
			BuildFunctionDefinitionForIRProduct(Product, Definition, FunctionDefinition);

			UMaterialFunction* Function = nullptr;
			FDreamShaderError CreateError;
			if (!CreateOrReuseIRMaterialFunction(FunctionDefinition, Function, CreateError) || !Function)
			{
				Diagnostics.Error(TEXT("DSH8202"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("CreateFunctionFailed", "The material function for '{0}' could not be created or reused. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(CreateError)));
				return false;
			}

			OutAsset = Function;

			const EMaterialFunctionUsage ExpectedUsage =
				Product.Kind == IR::EIRProductKind::MaterialLayer ? EMaterialFunctionUsage::MaterialLayer
				: Product.Kind == IR::EIRProductKind::MaterialLayerBlend ? EMaterialFunctionUsage::MaterialLayerBlend
				: EMaterialFunctionUsage::Default;

			// A usage that disagrees with the product kind means the asset is no longer what this
			// source describes -- an export that became a `@layer`, say -- so an unchanged source
			// hash must not skip the rebuild. Decided BEFORE the gates rather than after them: a
			// rebuild that goes ahead has to pass the open-editor and divergence checks like any
			// other, and reading the hash skip first would step over both on its way to editing the
			// asset anyway.
			const bool bUsageMatches = Function->GetMaterialFunctionUsage() == ExpectedUsage;

			bool bSkip = false;
			if (!CheckRebuildPreconditions(Function, Context, Product, /*bAllowHashSkip*/ bUsageMatches, Diagnostics, bSkip))
			{
				return false;
			}
			if (bSkip)
			{
				return true;
			}

			Function->Modify();

			// Ahead of the snapshot: this destroys the generated comment boxes.
			Private::ClearDreamShaderGeneratedComments(nullptr, Function);

			// Also ahead of the snapshot, for the opposite reason: this reads the pin GUIDs off the
			// live FunctionInput / FunctionOutput nodes and the rollback detaches those nodes.
			TMap<FName, FGuid> ExistingInputIds;
			TMap<FName, FGuid> ExistingOutputIds;
			CacheInterfaceIds(Function, ExistingInputIds, ExistingOutputIds);

			// The usage is set INSIDE the guarded region, which is why the snapshot is taken before
			// it rather than where the old teardown sat.
			Private::FDreamShaderGraphRollback Rollback(Function);
			if (!Rollback.IsArmed())
			{
				Diagnostics.Warning(TEXT("DSH8208"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("RollbackNotArmedFunction", "'{0}' could not be snapshotted before rebuilding it, so a failed rebuild will not be rolled back."),
					FText::FromString(Function->GetPathName())));
			}

			Function->SetMaterialFunctionUsage(ExpectedUsage);
			Function->Description = Product.Description;
			Function->bExposeToLibrary = Product.LibraryPath.IsEmpty() ? 0U : 1U;
			Function->LibraryCategoriesText.Reset();
			if (!Product.LibraryPath.IsEmpty())
			{
				TArray<FString> Categories;
				// A bar nests one category inside another in the editor palette (EdGraphSchema), so it stays
				// inside the category text, exactly as 1.x writes LibraryCategories; a comma separates two.
				Product.LibraryPath.ParseIntoArray(Categories, TEXT(","), true);
				for (const FString& Category : Categories)
				{
					const FString Trimmed = Category.TrimStartAndEnd();
					if (!Trimmed.IsEmpty())
					{
						Function->LibraryCategoriesText.Add(FText::FromString(Trimmed));
					}
				}
			}

			FIREmitter Emitter(nullptr, Function, Product, Context, Diagnostics);
			Emitter.SetExistingInterfaceIds(MoveTemp(ExistingInputIds), MoveTemp(ExistingOutputIds));
			if (!Emitter.EmitGraph())
			{
				return false;
			}

			FTextShaderLayout Layout;
			BuildLayoutFromHints(Product.Graph, Layout);
			const TMap<FString, UMaterialExpression*>& ExpressionsByVariable = Emitter.GetExpressionsByVariable();
			const TMap<FString, FString>& RegionByVariable = Emitter.GetRegionByVariable();
			Private::LayoutGeneratedExpressions(
				nullptr,
				Function,
				&Layout,
				ExpressionsByVariable.IsEmpty() ? nullptr : &ExpressionsByVariable,
				RegionByVariable.IsEmpty() ? nullptr : &RegionByVariable,
				/*bQuiet*/ false);

			// Before UpdateMaterialFunction, because that reaches ForceRecompileForRendering, which
			// is one of the two places that REBUILD DependentFunctionExpressionCandidates -- and
			// Commit() resets it. The other order would throw away the list the recompile had just
			// populated.
			Rollback.Commit();

			UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);
			Function->PostEditChange();

			Private::ApplySourceMetadata(Function, Context.SourceFilePath, Context.SourceHash);
			Private::ApplyOutputDigestMetadata(Function);
			WriteDreamShaderSourceSpans(Function, Emitter.GetSourceSpans());
			Function->MarkPackageDirty();

			FDreamShaderError SaveError;
			if (!Private::SaveAssetPackage(Function, SaveError))
			{
				Diagnostics.Error(TEXT("DSH8229"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("SaveFunctionFailed", "'{0}' was built but could not be saved. {1}"),
					FText::FromString(Function->GetPathName()),
					WrapLegacyError(SaveError)));
				return false;
			}

			PublishGeneratedIRAsset(Function);
			return true;
		}
	}

	bool EmitDreamShaderIRProduct(
		const IR::FIRModule& Module,
		const int32 ProductIndex,
		const FIREmitContext& Context,
		UObject*& OutAsset,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		OutAsset = nullptr;

		if (!Module.Products.IsValidIndex(ProductIndex))
		{
			Diagnostics.Error(TEXT("DSH8205"), Lang::FLangSpan(), FText::Format(
				LOCTEXT("BadProductIndex", "Product index {0} does not exist in this module, which has {1}."),
				FText::AsNumber(ProductIndex),
				FText::AsNumber(Module.Products.Num())));
			return false;
		}

		const IR::FIRProduct& Product = Module.Products[ProductIndex];

		if (Context.Catalog == nullptr || Context.Catalog->IsEmpty())
		{
			Diagnostics.Error(TEXT("DSH8204"), SpanOf(Product.Source), LOCTEXT("NoCatalog",
				"The emitter needs the builtin catalog the front end was bound against, but the emit context carries none."));
			return false;
		}

		FTextShaderDefinition Definition;
		BuildDefinitionForIRProduct(Product, Context.SourceFilePath, Definition);

		FString PackageName;
		FString ObjectPath;
		FString LeafName;
		FDreamShaderError DestinationError;
		if (!Private::ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, ObjectPath, LeafName, DestinationError))
		{
			Diagnostics.Error(TEXT("DSH8200"), SpanOf(Product.Source), FText::Format(
				LOCTEXT("DestinationFailed", "'{0}' does not resolve to a valid asset path. {1}"),
				FText::FromString(Product.Name),
				WrapLegacyError(DestinationError)));
			return false;
		}

		switch (Product.Kind)
		{
		case IR::EIRProductKind::Material:
			return Product.Backend == IR::EIRBackend::ThinCustom
				? EmitThinCustomMaterialProduct(Product, Definition, Context, OutAsset, Diagnostics)
				: EmitMaterialProduct(Product, Definition, Context, OutAsset, Diagnostics);

		case IR::EIRProductKind::MaterialFunction:
		case IR::EIRProductKind::MaterialLayer:
		case IR::EIRProductKind::MaterialLayerBlend:
			return EmitFunctionProduct(Product, Definition, Context, OutAsset, Diagnostics);

		default:
			break;
		}

		Diagnostics.Error(TEXT("DSH8232"), SpanOf(Product.Source), FText::Format(
			LOCTEXT("UnknownProductKind", "'{0}' has a product kind the emitter does not know how to materialize."),
			FText::FromString(Product.Name)));
		return false;
	}

	void WriteDreamShaderSourceSpans(UObject* Asset, const TMap<FGuid, IR::FIRSourceRef>& Spans)
	{
		if (!Asset)
		{
			return;
		}

		// An empty table still writes an empty object rather than leaving a stale one behind: the
		// key has to describe the graph the asset holds NOW, and a rebuild that produced no spans
		// (an empty graph) must not leave the previous build's line numbers pointing at nodes that
		// no longer exist.
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		Writer->WriteObjectStart();
		for (const TPair<FGuid, IR::FIRSourceRef>& Pair : Spans)
		{
			Writer->WriteObjectStart(Pair.Key.ToString(EGuidFormats::DigitsWithHyphens));
			Writer->WriteValue(TEXT("file"), Pair.Value.File);
			Writer->WriteValue(TEXT("line"), Pair.Value.Span.Line);
			Writer->WriteValue(TEXT("col"), Pair.Value.Span.Column);
			Writer->WriteValue(TEXT("len"), Pair.Value.Span.Length);
			if (Pair.Value.HasCallSite())
			{
				// callFile only when the call site is in another file -- a helper inlined from an
				// included .dsh has its Span in the header and its CallSite in the caller (CONTRACT
				// 6.13 #6). Omitted when the two agree, which is every same-file inline, so the
				// common row stays the shape it was and the reader falls back to "file".
				if (!Pair.Value.CallSiteFile.IsEmpty()
					&& !Pair.Value.CallSiteFile.Equals(Pair.Value.File, ESearchCase::CaseSensitive))
				{
					Writer->WriteValue(TEXT("callFile"), Pair.Value.CallSiteFile);
				}
				Writer->WriteValue(TEXT("callLine"), Pair.Value.CallSite.Line);
				Writer->WriteValue(TEXT("callCol"), Pair.Value.CallSite.Column);
			}
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectEnd();
		Writer->Close();

		SetDreamShaderAssetMetadata(Asset, TEXT("DreamShader.SourceSpans"), Json);
	}
}

#undef LOCTEXT_NAMESPACE
