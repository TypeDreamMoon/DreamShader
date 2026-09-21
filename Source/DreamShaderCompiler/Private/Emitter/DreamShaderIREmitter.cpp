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

#include "DreamShaderIREmitter.h"

#include "Emitter/DreamShaderIRAssets.h"
#include "Emitter/DreamShaderIREmitterInternal.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "Assets/DreamShaderGraphRollback.h"
#include "DreamShaderGeneratedAssets.h"
#include "Assets/DreamShaderThinCustomParameterOverrides.h"
// FDreamShaderShaderCompileStallWatch (DSH9011).
#include "DreamShaderCompilerServiceInternal.h"
// The graph debug table the probe preview and breakpoints read (agreement A1).
#include "DreamShaderGraphDebugInfo.h"
// `.dsi`: the instance product (IsMemoryOnlyMaterial / MaterializeDreamShaderMaterial, the schema, the keys).
#include "DreamShaderCompilerService.h"
#include "DreamShaderInstanceSchema.h"
#include "DreamShaderInstanceSettings.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameters.h"
#include "Misc/PackageName.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "VT/RuntimeVirtualTexture.h"
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
#include "Engine/TextureCollection.h"
#include "Materials/MaterialParameterCollection.h"
#endif

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
					if (Hint.bHasColor)
					{
						// `#pragma layout(Comment, ..., Color = "r g b a")`; without it the layout pass's own default stays.
						Comment.Color = FLinearColor(Hint.Color[0], Hint.Color[1], Hint.Color[2], Hint.Color[3]);
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

		/** Whether a statement binding lies in the compiled file itself, in either spelling the builder may have used for it. */
		bool IsDreamShaderBindingInCompiledFile(const IR::FIRSourceRef& Source, const FString& SourceFilePath, const FString& StampedSourceFilePath)
		{
			if (Source.File.IsEmpty())
			{
				return true;
			}

			// Case-insensitive, as the file system is. The pipeline stamps project-relative paths (debt B5); a module built
			// without a stamper -- a test, a tool -- carries the absolute one.
			return Source.File.Equals(StampedSourceFilePath, ESearchCase::IgnoreCase)
				|| Source.File.Equals(SourceFilePath, ESearchCase::IgnoreCase);
		}

		/**
		 * The graph debug table of one built material (agreement A1): one probe per statement that bound a named variable in
		 * the compiled file, pointing at the expression output its value became. A binding in an included header is not a
		 * probe of this file, and 2.0 never puts an inline mask on a wire, so no probe carries one.
		 */
		void BuildDreamShaderGraphProbes(
			const IR::FIRProduct& Product,
			const FIREmitter& Emitter,
			const FString& SourceFilePath,
			TArray<Private::FDreamShaderGraphProbe>& OutProbes)
		{
			OutProbes.Reset();

			const FString StampedSourceFilePath = Private::MakeProjectRelativeSourcePath(SourceFilePath);
			for (const IR::FIRStatementBinding& Binding : Product.Graph.StatementBindings)
			{
				if (Binding.Name.IsEmpty()
					|| !Product.Graph.IsValidValue(Binding.Value)
					|| !IsDreamShaderBindingInCompiledFile(Binding.Source, SourceFilePath, StampedSourceFilePath))
				{
					continue;
				}

				FEmittedValue Value;
				if (!Emitter.TryGetEmittedValue(Binding.Value, Value))
				{
					// A value that became no expression of its own, such as a write into the material sink.
					continue;
				}

				const IR::FIRType& Type = Product.Graph.TypeOf(Binding.Value);

				Private::FDreamShaderGraphProbe& Probe = OutProbes.AddDefaulted_GetRef();
				Probe.Line = Binding.Source.Span.Line;
				Probe.Column = Binding.Source.Span.Column;
				Probe.Name = Binding.Name;
				Probe.Expression = Value.Expression;
				Probe.OutputIndex = Value.OutputIndex;
				Probe.ComponentCount = FMath::Max(1, Type.GraphComponentCount());
				Probe.bIsTextureObject = Type.IsTexture();
				Probe.bIsMaterialAttributes = Type.IsMaterial();
				Probe.bIsSubstrateMaterial = Type.Kind == IR::EIRTypeKind::Substrate;
				Probe.bIsStatementTarget = true;
			}
		}

		/**
		 * `DreamShader.DecompileHints`, version 1 (agreement A6; research-decompiler section 6.5): what the graph itself cannot
		 * say about where its nodes came from. See WriteDreamShaderDecompileHints for the shape.
		 */
		FString BuildDreamShaderDecompileHintsJson(const IR::FIRGraph& Graph, const FIREmitter& Emitter)
		{
			FString Json;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("version"), 1);

			Writer->WriteObjectStart(TEXT("names"));
			for (const TPair<FGuid, FString>& Pair : Emitter.GetDecompileNames())
			{
				Writer->WriteValue(Pair.Key.ToString(EGuidFormats::DigitsWithHyphens), Pair.Value);
			}
			Writer->WriteObjectEnd();

			Writer->WriteObjectStart(TEXT("nodeRegions"));
			for (const TPair<FGuid, int32>& Pair : Emitter.GetDecompileNodeRegions())
			{
				Writer->WriteValue(Pair.Key.ToString(EGuidFormats::DigitsWithHyphens), Pair.Value);
			}
			Writer->WriteObjectEnd();

			Writer->WriteArrayStart(TEXT("regions"));
			for (const IR::FIRRegion& Region : Graph.Regions)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), Region.Name);
				Writer->WriteValue(TEXT("parent"), Region.Parent);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();

			Writer->WriteArrayStart(TEXT("comments"));
			for (const IR::FIRLayoutHint& Hint : Graph.LayoutHints)
			{
				if (!Hint.Kind.Equals(TEXT("Comment"), ESearchCase::CaseSensitive))
				{
					continue;
				}

				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), Hint.Name);
				Writer->WriteValue(TEXT("x"), Hint.X);
				Writer->WriteValue(TEXT("y"), Hint.Y);
				// Only what the pragma said, so a decompile prints back exactly the pragma it came from.
				if (Hint.bHasSize)
				{
					Writer->WriteValue(TEXT("w"), Hint.W);
					Writer->WriteValue(TEXT("h"), Hint.H);
				}
				if (Hint.bHasColor)
				{
					Writer->WriteArrayStart(TEXT("color"));
					for (int32 Channel = 0; Channel < 4; ++Channel)
					{
						Writer->WriteValue(static_cast<double>(Hint.Color[Channel]));
					}
					Writer->WriteArrayEnd();
				}
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();

			Writer->WriteObjectEnd();
			Writer->Close();
			return Json;
		}

		/**
		 * Whether a build of Asset would end in a file, when writing is refused (dump-graph's write guard, a second
		 * editor that does not own the bridge). An asset that HAS a file is the write owner's to rebuild and is left as
		 * it stands (DSH8209). One that has none is built in memory and left there, which is what the 1.x generator
		 * did and what `dump-graph` of a source nobody compiled yet reads: nothing is written either way, and the
		 * alternative is the empty shell CreateOrReuse just made. bOutSaveToDisk says which of the two this build is.
		 */
		bool WouldBuildPersist(UObject* Asset, bool& bOutSaveToDisk)
		{
			bOutSaveToDisk = Private::MayWriteGeneratedAssetsToDisk();
			return bOutSaveToDisk || Private::IsGeneratedAssetPersisted(Asset);
		}

		/** DSH8298 when the pipeline says the user cancelled; the caller returns false and its rollback does the rest. */
		bool IsEmitCancelled(const FIREmitContext& Context, const IR::FIRProduct& Product, Lang::FLangDiagnosticSink& Diagnostics)
		{
			if (!Context.IsCancelled || !Context.IsCancelled())
			{
				return false;
			}
			Diagnostics.Error(TEXT("DSH8298"), SpanOf(Product.Source), FText::Format(
				LOCTEXT("EmitCancelled", "Building '{0}' was cancelled; the asset is as it was before this compile."),
				FText::FromString(Product.Name)));
			return true;
		}

		/** The end of a build that was told not to write: nothing is saved, and no save-all may do it later. */
		void FinishMemoryOnlyBuild(UObject* Asset)
		{
			if (UPackage* Package = Asset ? Asset->GetPackage() : nullptr)
			{
				Package->SetDirtyFlag(false);
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
			const bool bWouldPersist,
			Lang::FLangDiagnosticSink& Diagnostics,
			bool& bOutSkip)
		{
			bOutSkip = false;

			// bWouldPersist is false only for an Ephemeral ThinCustom build, which writes nothing to disk and so is not another
			// editor's to own.
			FDreamShaderError DeferMessage;
			if (Private::ShouldDeferPersistedAssetToWriteOwner(Asset, bWouldPersist, DeferMessage))
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
			TMap<FGuid, IR::FIRSourceRef>& OutSpans,
			FString& OutDecompileHints)
		{
			Material->Modify();

			// First of all (agreement A1): a probe preview material shares this material's expression collection and has to
			// let go of it before a single node below is detached or destroyed.
			Private::FDreamShaderGraphDebugRegistry::Get().NotifyGraphMaterialAboutToReset(Material);

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

			// The graph is gone and the rollback holds it: from here a cancel is a failed rebuild like any other.
			if (IsEmitCancelled(Context, Product, Diagnostics))
			{
				return false;
			}

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
			if (!Emitter.EmitGraph() || IsEmitCancelled(Context, Product, Diagnostics))
			{
				return false;
			}

			// The project's Graph Layout Style: one of the styles computed on the IR, or Classic, the 1.x layout.
			if (!ApplyDreamShaderIRLayout(Material, nullptr, Product, Emitter))
			{
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
			}

			OutSpans = Emitter.GetSourceSpans();
			OutDecompileHints = BuildDreamShaderDecompileHintsJson(Product.Graph, Emitter);

			// The graph is complete and nothing below can fail, so the old one is finally
			// expendable. Before the recompile, not after: holding a detached copy of the old graph
			// across the compile would keep every node of it alive for no reason.
			Rollback.Commit();

			// The finished graph is addressable by (line, name) now. Published before the recompile, as 1.x did, so a probe
			// preview that re-wires on publish gets its own compile queued alongside this one.
			{
				TArray<Private::FDreamShaderGraphProbe> Probes;
				BuildDreamShaderGraphProbes(Product, Emitter, Context.SourceFilePath, Probes);
				Private::FDreamShaderGraphDebugRegistry::Get().Publish(Context.SourceFilePath, Material, MoveTemp(Probes));
			}

			{
				// DSH9011. Everything above is graph construction and takes milliseconds; this is the stage
				// that can take minutes, and naming it is what keeps a working compile from reading as a hung
				// plugin (issue #29).
				FDreamShaderShaderCompileStallWatch ShaderCompileWatch(Material->GetName());
				UMaterialEditingLibrary::RecompileMaterial(Material);
				Material->PostEditChange();
			}
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

			bool bSaveToDisk = true;
			const bool bWouldPersist = WouldBuildPersist(Material, bSaveToDisk);

			bool bSkip = false;
			if (!CheckRebuildPreconditions(Material, Context, Product, /*bAllowHashSkip*/ true, bWouldPersist, Diagnostics, bSkip))
			{
				return false;
			}
			if (bSkip)
			{
				return true;
			}

			TMap<FGuid, IR::FIRSourceRef> Spans;
			FString DecompileHints;
			if (!PopulateMaterialFromProduct(Material, Definition, Product, Context, Diagnostics, Spans, DecompileHints))
			{
				return false;
			}

			Private::ApplySourceMetadata(Material, Context.SourceFilePath, Context.SourceHash);
			Private::ApplyOutputDigestMetadata(Material);
			WriteDreamShaderSourceSpans(Material, Spans);
			WriteDreamShaderDecompileHints(Material, DecompileHints);
			if (!bSaveToDisk)
			{
				FinishMemoryOnlyBuild(Material);
				return true;
			}
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
			if (!CreateOrReuseIRThinCustomInstance(Definition, Instance, CreateError, Context.ThinCustomPersistence) || !Instance)
			{
				Diagnostics.Error(TEXT("DSH8203"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("CreateInstanceFailed", "The ThinCustom instance for '{0}' could not be created or reused. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(CreateError)));
				return false;
			}

			OutAsset = Instance;

			// THE Ephemeral/Materialized decision for this product (architecture plan v2 section 5.1), made as 1.x made it: the
			// request asks, and storage decides -- an instance whose package exists on disk is maintained on disk whatever the
			// request said. Asking the instance answers for the pair, because a Materialized base is its subobject.
			const bool bEphemeral =
				Context.ThinCustomPersistence == ::UE::DreamShader::EThinCustomPersistence::Ephemeral
				&& !Private::IsGeneratedAssetPersisted(Instance);

			// One gate for the pair: the base belongs to the instance, so the instance's
			// digest covers both the graph and the instance's own overrides.
			bool bSkip = false;
			if (!CheckRebuildPreconditions(Instance, Context, Product, /*bAllowHashSkip*/ true, /*bWouldPersist*/ !bEphemeral, Diagnostics, bSkip))
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
			if (!EnsureIRThinCustomBaseMaterial(Instance, bEphemeral, BaseMaterial, BaseError) || !BaseMaterial)
			{
				Diagnostics.Error(TEXT("DSH8230"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("CreateThinBaseFailed", "The hidden base material for '{0}' could not be created. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(BaseError)));
				return false;
			}

			TMap<FGuid, IR::FIRSourceRef> Spans;
			FString DecompileHints;
			if (!PopulateMaterialFromProduct(BaseMaterial, Definition, Product, Context, Diagnostics, Spans, DecompileHints))
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

			{
				// DSH9011: the permutation update is where the instance's own shader map compiles.
				FDreamShaderShaderCompileStallWatch ShaderCompileWatch(Instance->GetName());
				Instance->UpdateStaticPermutation();
				Instance->PostEditChange();
			}

			Private::ApplySourceMetadata(Instance, Context.SourceFilePath, Context.SourceHash);
			// After UpdateStaticPermutation and PostEditChange: those settle the static parameter set
			// the digest reads, so stamping ahead of them would fingerprint a state the asset is not
			// left in.
			Private::ApplyOutputDigestMetadata(Instance);
			WriteDreamShaderSourceSpans(BaseMaterial, Spans);
			WriteDreamShaderDecompileHints(BaseMaterial, DecompileHints);

			if (bEphemeral)
			{
				// Memory-only: nothing is saved. The editor-only setters and PostEditChange dirtied the unsaved package, so the
				// flag is cleared -- no save-all or exit prompt may silently persist a virtual instance material. Materialize is
				// the one way to disk (MaterializeDreamShaderMaterial).
				Instance->GetPackage()->SetDirtyFlag(false);
				return true;
			}

			// The base is stamped too, only to keep the "generated objects carry source metadata"
			// invariant; the instance is the on-disk ownership anchor. Never an Ephemeral base: it lives in the
			// transient package, whose metadata this would write instead.
			Private::ApplySourceMetadata(BaseMaterial, Context.SourceFilePath, Context.SourceHash);

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
			bool bSaveToDisk = true;
			const bool bWouldPersist = WouldBuildPersist(Function, bSaveToDisk);
			if (!CheckRebuildPreconditions(Function, Context, Product, /*bAllowHashSkip*/ bUsageMatches, bWouldPersist, Diagnostics, bSkip))
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
			// The rollback above holds the old graph: a cancel that lands here is a failed rebuild like any other.
			if (IsEmitCancelled(Context, Product, Diagnostics) || !Emitter.EmitGraph() || IsEmitCancelled(Context, Product, Diagnostics))
			{
				return false;
			}

			if (!ApplyDreamShaderIRLayout(nullptr, Function, Product, Emitter))
			{
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
			}

			// Before UpdateMaterialFunction, because that reaches ForceRecompileForRendering, which
			// is one of the two places that REBUILD DependentFunctionExpressionCandidates -- and
			// Commit() resets it. The other order would throw away the list the recompile had just
			// populated.
			Rollback.Commit();

			{
				// DSH9011: UpdateMaterialFunction recompiles every material that calls this function, so this
				// one call can outlast the whole graph build.
				FDreamShaderShaderCompileStallWatch ShaderCompileWatch(Function->GetName());
				UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);
				Function->PostEditChange();
			}

			Private::ApplySourceMetadata(Function, Context.SourceFilePath, Context.SourceHash);
			Private::ApplyOutputDigestMetadata(Function);
			WriteDreamShaderSourceSpans(Function, Emitter.GetSourceSpans());
			WriteDreamShaderDecompileHints(Function, BuildDreamShaderDecompileHintsJson(Product.Graph, Emitter));
			if (!bSaveToDisk)
			{
				FinishMemoryOnlyBuild(Function);
				return true;
			}
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

		/** An override's FIRInstanceOverride::Value as the engine value of its kind; false with OutError when it is not one. */
		bool MakeDreamShaderInstanceEngineValue(const IR::FIRInstanceOverride& Override, FMaterialParameterMetadata& OutMeta, FString& OutError)
		{
			const IR::FIRPropertyValue& Value = Override.Value;

			// An object override: "" is an explicit None, anything else a reference the asset layer resolves.
			const auto LoadReference = [&Value, &OutError](UClass* ExpectedClass, UObject*& OutObject) -> bool
			{
				OutObject = nullptr;
				const FString Reference = Value.S.TrimStartAndEnd();
				if (Reference.IsEmpty())
				{
					return true;
				}

				FString ObjectPath;
				FDreamShaderError ReferenceError;
				if (!Private::TryResolveDreamShaderAssetReference(Reference, ObjectPath, ReferenceError, ExpectedClass))
				{
					OutError = ReferenceError.Message;
					return false;
				}

				OutObject = StaticLoadObject(ExpectedClass, nullptr, *ObjectPath);
				if (!OutObject)
				{
					OutError = FString::Printf(TEXT("nothing of class %s loads from '%s'."), *ExpectedClass->GetName(), *ObjectPath); /* I18N-EXEMPT: wrapped by DSH8252 */
					return false;
				}
				return true;
			};

			UObject* Object = nullptr;
			switch (Override.Kind)
			{
			case IR::EIRParameterKind::Scalar:
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(static_cast<float>(Value.V[0])));
				return true;

			case IR::EIRParameterKind::Vector:
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(FLinearColor(
					static_cast<float>(Value.V[0]),
					static_cast<float>(Value.V[1]),
					static_cast<float>(Value.V[2]),
					static_cast<float>(Value.V[3]))));
				return true;

			case IR::EIRParameterKind::DoubleVector:
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(FVector4d(Value.V[0], Value.V[1], Value.V[2], Value.V[3])));
				return true;

			case IR::EIRParameterKind::StaticSwitch:
			{
				const bool bSwitch = Value.Kind == IR::EIRPropertyKind::Bool ? Value.B : Value.V[0] != 0.0;
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(bSwitch));
				return true;
			}

			case IR::EIRParameterKind::StaticComponentMask:
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(Value.V[0] != 0.0, Value.V[1] != 0.0, Value.V[2] != 0.0, Value.V[3] != 0.0));
				return true;

			case IR::EIRParameterKind::Texture:
				if (!LoadReference(UTexture::StaticClass(), Object))
				{
					return false;
				}
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(Cast<UTexture>(Object)));
				return true;

			case IR::EIRParameterKind::RuntimeVirtualTexture:
				if (!LoadReference(URuntimeVirtualTexture::StaticClass(), Object))
				{
					return false;
				}
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(Cast<URuntimeVirtualTexture>(Object)));
				return true;

			case IR::EIRParameterKind::SparseVolumeTexture:
				if (!LoadReference(USparseVolumeTexture::StaticClass(), Object))
				{
					return false;
				}
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(Cast<USparseVolumeTexture>(Object)));
				return true;

			case IR::EIRParameterKind::Font:
				if (!LoadReference(UFont::StaticClass(), Object))
				{
					return false;
				}
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(Cast<UFont>(Object), Override.FontPage));
				return true;

			case IR::EIRParameterKind::TextureCollection:
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
				if (!LoadReference(UTextureCollection::StaticClass(), Object))
				{
					return false;
				}
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(static_cast<const UTextureCollection*>(Cast<UTextureCollection>(Object))));
				return true;
#else
				OutError = TEXT("this engine has no texture collection parameters.");
				return false;
#endif

			case IR::EIRParameterKind::ParameterCollection:
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
				if (!LoadReference(UMaterialParameterCollection::StaticClass(), Object))
				{
					return false;
				}
				OutMeta = FMaterialParameterMetadata(FMaterialParameterValue(static_cast<const UMaterialParameterCollection*>(Cast<UMaterialParameterCollection>(Object))));
				return true;
#else
				OutError = TEXT("this engine has no parameter collection parameters.");
				return false;
#endif
			}

			OutError = TEXT("the parameter kind is unknown.");
			return false;
		}

		/**
		 * The MaterialInstance product of a `.dsi`: one plain UMaterialInstanceConstant of its parent, with its overrides
		 * and keys set through one FMaterialInstanceParameterUpdateContext (research-instance section 3.6). The asset is
		 * not touched until every check has passed: the gates, the parent, the cycle, the drift against the loaded
		 * parent, every value and every key.
		 */
		bool EmitMaterialInstanceProduct(
			const IR::FIRProduct& Product,
			const FTextShaderDefinition& Definition,
			const FIREmitContext& Context,
			UObject*& OutAsset,
			Lang::FLangDiagnosticSink& Diagnostics)
		{
			const IR::FIRInstance& InstanceData = Product.Instance;

			UMaterialInstanceConstant* Instance = nullptr;
			FDreamShaderError CreateError;
			if (!CreateOrReuseIRMaterialInstance(Definition, Instance, CreateError) || !Instance)
			{
				Diagnostics.Error(TEXT("DSH8240"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("CreateMaterialInstanceFailed", "The material instance for '{0}' could not be created or reused. {1}"),
					FText::FromString(Product.Name),
					WrapLegacyError(CreateError)));
				return false;
			}

			OutAsset = Instance;

			bool bSkip = false;
			bool bSaveToDisk = true;
			const bool bWouldPersist = WouldBuildPersist(Instance, bSaveToDisk);
			if (!CheckRebuildPreconditions(Instance, Context, Product, /*bAllowHashSkip*/ true, bWouldPersist, Diagnostics, bSkip))
			{
				return false;
			}
			if (bSkip)
			{
				return true;
			}

			const FString ParentLabel = InstanceData.ParentObjectPath.IsEmpty() ? InstanceData.ParentReference : InstanceData.ParentObjectPath;
			UMaterialInterface* Parent = InstanceData.ParentObjectPath.IsEmpty()
				? nullptr
				: LoadObject<UMaterialInterface>(nullptr, *InstanceData.ParentObjectPath);
			if (!Parent)
			{
				Diagnostics.Error(TEXT("DSH8243"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("InstanceParentDoesNotLoad", "The parent '{0}' of '{1}' does not load; compile the source that builds it, or correct the Parent key."),
					FText::FromString(ParentLabel),
					FText::FromString(Product.Name)));
				return false;
			}

			// A saved instance cannot reference a memory-only parent: the reference would be a dangling import.
			if (Private::IsMemoryOnlyMaterial(Parent))
			{
				const UDreamShaderMaterialInstance* DreamParent = Cast<UDreamShaderMaterialInstance>(Parent);
				if (!DreamParent || DreamParent->SourceFilePath.IsEmpty())
				{
					Diagnostics.Error(TEXT("DSH8248"), SpanOf(Product.Source), FText::Format(
						LOCTEXT("InstanceParentMemoryOnlyForeign", "The parent '{0}' exists only in memory and no DreamShader source builds it, so '{1}' cannot be saved against it; save the parent first."),
						FText::FromString(Parent->GetPathName()),
						FText::FromString(Product.Name)));
					return false;
				}

				const FString EphemeralParentPath = Parent->GetPathName();
				FString MaterializeError;
				UMaterialInterface* Materialized = Private::MaterializeDreamShaderMaterial(Parent, MaterializeError);
				if (!Materialized)
				{
					Diagnostics.Error(TEXT("DSH8243"), SpanOf(Product.Source), FText::Format(
						LOCTEXT("InstanceParentMaterializeFailed", "The parent '{0}' of '{1}' exists only in memory and could not be saved first. {2}"),
						FText::FromString(EphemeralParentPath),
						FText::FromString(Product.Name),
						FText::FromString(MaterializeError)));
					return false;
				}

				Diagnostics.Info(TEXT("DSH8244"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("InstanceParentMaterialized", "'{0}' was memory-only, so it was saved to disk first: '{1}' parents to it."),
					FText::FromString(EphemeralParentPath),
					FText::FromString(Product.Name)));
				UE_LOG(LogDreamShader, Display, TEXT("[Ephemeral] Materialized '%s' because instance '%s' parents to it."), *EphemeralParentPath, *Instance->GetPathName());
				Parent = Materialized;
			}

			const UMaterialInstance* ParentInstance = Cast<UMaterialInstance>(Parent);
			if (Parent == Instance || (ParentInstance && ParentInstance->IsChildOf(Instance)))
			{
				Diagnostics.Error(TEXT("DSH8245"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("InstanceParentCycle", "'{0}' cannot parent to '{1}': that parent already descends from this instance."),
					FText::FromString(Product.Name),
					FText::FromString(Parent->GetPathName())));
				return false;
			}

			// Drift: the schema the binder checked came from the parent's SOURCE, which can be ahead of the parent asset.
			{
				IR::FIRParameterSchema AssetSchema;
				BuildParameterSchemaFromAsset(Parent, AssetSchema);
				TArray<FString> Missing;
				for (const IR::FIRInstanceOverride& Override : InstanceData.Overrides)
				{
					const int32 EntryIndex = AssetSchema.Find(Override.ParameterName, Override.Association, Override.AssociationIndex);
					if (EntryIndex == INDEX_NONE || AssetSchema.Parameters[EntryIndex].Kind != Override.Kind)
					{
						Missing.Add(Override.ParameterName);
					}
				}
				if (Missing.Num() > 0)
				{
					Diagnostics.Error(TEXT("DSH8246"), SpanOf(Product.Source), FText::Format(
						LOCTEXT("InstanceParentDrift", "The parent asset '{0}' has no parameter {1} of the kind this instance overrides; the asset is older than its source. Compile the parent source first."),
						FText::FromString(Parent->GetPathName()),
						FText::FromString(FString::Join(Missing, TEXT(", ")))));
					return false;
				}
			}

			// Every value before the asset changes.
			TArray<TPair<FMaterialParameterInfo, FMaterialParameterMetadata>> Values;
			Values.Reserve(InstanceData.Overrides.Num());
			bool bValuesOk = true;
			for (const IR::FIRInstanceOverride& Override : InstanceData.Overrides)
			{
				FMaterialParameterMetadata Meta;
				FString ValueError;
				if (!MakeDreamShaderInstanceEngineValue(Override, Meta, ValueError))
				{
					Diagnostics.Error(TEXT("DSH8252"), SpanOf(Override.Source), FText::Format(
						LOCTEXT("InstanceValueNotApplied", "The override of '{0}' could not be applied: {1}"),
						FText::FromString(Override.ParameterName),
						FText::FromString(ValueError)));
					bValuesOk = false;
					continue;
				}
				Values.Emplace(FMaterialParameterInfo(FName(*Override.ParameterName)), MoveTemp(Meta));
			}
			if (!bValuesOk)
			{
				return false;
			}

			// And every key, on a scratch instance: a bad key reports here and leaves the real asset untouched.
			{
				Lang::FLangDiagnosticSink ScratchDiagnostics(Context.SourceFilePath);
				UMaterialInstanceConstant* Scratch = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient);
				FMaterialInstanceBasePropertyOverrides ScratchBase;
				const bool bKeysOk = ApplyInstanceSettings(Scratch, InstanceData.Settings, ScratchBase, Product.Source, ScratchDiagnostics);
				Scratch->MarkAsGarbage();
				if (!bKeysOk)
				{
					Diagnostics.Append(MoveTemp(ScratchDiagnostics));
					return false;
				}
			}

			Instance->Modify();
			Instance->SetParentEditorOnly(Parent, /*RecacheShader*/ false);
			{
				// DSH9011 first, so it is destroyed last: the update context's destructor is the permutation update.
				FDreamShaderShaderCompileStallWatch ShaderCompileWatch(Instance->GetName());

				// Clears every override, statics included, and runs ONE static permutation update when it goes out of
				// scope (MaterialInstance.h, FMaterialInstanceParameterUpdateContext): an override deleted from the file does
				// not survive the rebuild, and the shader map is rebuilt once.
				FMaterialInstanceParameterUpdateContext Update(Instance, EMaterialInstanceClearParameterFlag::All);
				for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Value : Values)
				{
					Update.SetParameterValueEditorOnly(Value.Key, Value.Value);
				}

				FMaterialInstanceBasePropertyOverrides BaseOverrides;
				Lang::FLangDiagnosticSink KeyDiagnostics(Context.SourceFilePath);
				ApplyInstanceSettings(Instance, InstanceData.Settings, BaseOverrides, Product.Source, KeyDiagnostics);
				Update.SetBasePropertyOverrides(BaseOverrides);
			}

			// ValidateStaticPermutationAllowed nulls the parent of an instance whose static overrides the parent disallows.
			if (Instance->Parent != Parent)
			{
				Diagnostics.Error(TEXT("DSH8247"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("InstanceParentDropped", "The engine dropped '{0}' as the parent of '{1}' while applying the static overrides; that parent does not allow them."),
					FText::FromString(Parent->GetPathName()),
					FText::FromString(Product.Name)));
				return false;
			}

			Instance->PostEditChange();

			// After PostEditChange, as the ThinCustom path orders it: the digest reads the settled static parameter set.
			Private::ApplySourceMetadata(Instance, Context.SourceFilePath, Context.SourceHash);
			Private::ApplyOutputDigestMetadata(Instance);
			if (!bSaveToDisk)
			{
				FinishMemoryOnlyBuild(Instance);
				return true;
			}
			Instance->MarkPackageDirty();

			const bool bWasOnDisk = FPackageName::DoesPackageExist(Instance->GetOutermost()->GetName());
			FDreamShaderError SaveError;
			if (!Private::SaveAssetPackage(Instance, SaveError))
			{
				Diagnostics.Error(TEXT("DSH8229"), SpanOf(Product.Source), FText::Format(
					LOCTEXT("SaveMaterialInstanceFailed", "'{0}' was built but could not be saved. {1}"),
					FText::FromString(Instance->GetPathName()),
					WrapLegacyError(SaveError)));
				return false;
			}

			if (!bWasOnDisk)
			{
				PublishGeneratedIRAsset(Instance);
			}
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

		case IR::EIRProductKind::MaterialInstance:
			return EmitMaterialInstanceProduct(Product, Definition, Context, OutAsset, Diagnostics);

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

	void WriteDreamShaderDecompileHints(UObject* Asset, const FString& Json)
	{
		if (!Asset)
		{
			return;
		}

		// Written on every build, even with nothing to hint: the key describes the graph the asset holds NOW, like the span
		// table, and a stale one would hand the decompiler names for nodes that no longer exist.
		SetDreamShaderAssetMetadata(Asset, TEXT("DreamShader.DecompileHints"), Json);
	}
}

#undef LOCTEXT_NAMESPACE
