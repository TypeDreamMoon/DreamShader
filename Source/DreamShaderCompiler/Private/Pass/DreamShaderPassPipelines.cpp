// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderPassPipelines.h.
//
// Diagnostics owned by this file: DSH8335-DSH8339.
//
// The registry verbs are built on the slot registry's own reading and writing (Pass/DreamShaderPassSlotRegistry.h), and
// the pre-check `check -Shaders` runs is the emitter's, minus the writing: the same staging, the same plan, the same
// in-memory compile. What is new here is judging a slot, and that needs care in one direction only -- a slot freed while
// its pipeline still runs the pass would hand that slot to the next new pass, whose shader the old pipeline would then
// dispatch with its own bindings. So a slot is garbage only on positive evidence: no source builds its pipeline any more,
// or its pipeline's source compiles and no longer has the pass. A source that does not compile keeps every slot it has.

#include "DreamShaderPassPipelines.h"

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "DreamShaderProductIndex.h"
#include "Emitter/DreamShaderIRAssets.h"
#include "Emitter/DreamShaderIREmitterPassPipeline.h"
#include "Pass/DreamShaderPassShaderText.h"
#include "Pass/DreamShaderPassSlotRegistry.h"
#include "Pipeline/DreamShaderCompilePipelineInternal.h"

#include "DreamPassPipeline.h"
#include "DreamShaderPassModule.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShader.Pass.Pipelines"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamShaderPassPipelinesDetail
	{
		/** One HLSL pass a `.dsp` runs now. */
		struct FSourcePass
		{
			FString Pipeline;
			FString Pass;
			bool bCompute = true;
		};

		/** What one `.dsp`'s front end says: its HLSL passes, or that it did not get far enough to say. */
		struct FSourcePasses
		{
			bool bOk = false;
			TArray<FSourcePass> Passes;
		};

		/** The IR spells kinds canonically and case-sensitively; FString's operator== does not compare that way. */
		bool IsHlslPass(const IR::FIRPass& Pass, bool& bOutCompute)
		{
			if (Pass.Kind.Equals(TEXT("compute"), ESearchCase::CaseSensitive))
			{
				bOutCompute = true;
				return true;
			}
			if (Pass.Kind.Equals(TEXT("fullscreen"), ESearchCase::CaseSensitive) && Pass.MaterialReference.IsEmpty()
				&& (!Pass.ShaderReference.IsEmpty() || IR::PassHlslSource::IsInline(Pass.HlslSource)))
			{
				bOutCompute = false;
				return true;
			}
			return false;
		}

		/** Runs SourceFile to IR once per call site: nothing is built, nothing is written. */
		FSourcePasses GetSourcePasses(const FString& SourceFile, TMap<FString, FSourcePasses>& Cache)
		{
			if (const FSourcePasses* Cached = Cache.Find(SourceFile))
			{
				return *Cached;
			}

			FSourcePasses Result;
			FDreamShaderLang2PipelineResult Run;
			if (RunDreamShaderPipelineToIR(SourceFile, Run) && Run.IR.IsValid())
			{
				Result.bOk = true;
				for (const IR::FIRProduct& Product : Run.IR->Products)
				{
					if (Product.Kind != IR::EIRProductKind::PassPipeline)
					{
						continue;
					}
					FString PackageName;
					FString ObjectPath;
					FString LeafName;
					FDreamShaderError DestinationError;
					if (!ResolveIRProductObjectPath(Product, Run.SourceFilePath, PackageName, ObjectPath, LeafName, DestinationError))
					{
						Result.bOk = false;
						continue;
					}
					for (const IR::FIRPass& Pass : Product.PassPipeline.Passes)
					{
						bool bCompute = false;
						if (IsHlslPass(Pass, bCompute))
						{
							FSourcePass& Out = Result.Passes.AddDefaulted_GetRef();
							Out.Pipeline = ObjectPath;
							Out.Pass = Pass.Name;
							Out.bCompute = bCompute;
						}
					}
				}
			}
			Cache.Add(SourceFile, Result);
			return Result;
		}

		/** The registry's `source`, project-relative as stamped, as an absolute normalized path. */
		FString ResolveRecordedSource(const FString& Recorded)
		{
			if (Recorded.IsEmpty())
			{
				return FString();
			}
			const FString Absolute = FPaths::IsRelative(Recorded)
				? FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), Recorded)
				: Recorded;
			return UE::DreamShader::NormalizeSourceFilePath(Absolute);
		}

		/** What the index says about one slot's pipeline, copied out: a refresh invalidates every record pointer. */
		struct FPipelineOwner
		{
			/** The `.dsp` that builds the pipeline now; empty when none does. */
			FString SourceFile;
			/** No source builds it, but the one the registry recorded is still there and did not get indexed: say nothing. */
			bool bRecordedSourceUnindexed = false;
		};

		/** A shader file and what it pulls in, as the slot's snapshot would take it. */
		void AppendShaderClosureFiles(const FString& ShaderFile, TArray<FString>& InOutFiles)
		{
			const FString Root = NormalizeDreamPassShaderFilePath(ShaderFile);
			InOutFiles.AddUnique(Root);

			FDreamPassShaderClosure Closure;
			if (!CollectDreamPassShaderClosure(Root, Closure))
			{
				// Not there yet: it is still watched for, since creating it is what makes the pass compile.
				return;
			}
			for (const FDreamPassShaderClosureFile& File : Closure.Files)
			{
				InOutFiles.AddUnique(File.FilePath);
			}
			for (const FDreamPassLiveInclude& Live : Closure.LiveIncludes)
			{
				if (!Live.FilePath.IsEmpty())
				{
					InOutFiles.AddUnique(NormalizeDreamPassShaderFilePath(Live.FilePath));
				}
			}
			for (const FDreamPassMissingInclude& Missing : Closure.MissingIncludes)
			{
				InOutFiles.AddUnique(NormalizeDreamPassShaderFilePath(FPaths::Combine(FPaths::GetPath(Missing.IncludingFile), Missing.Path)));
			}
		}

		/** The shader files every `.dsp` names, per `.dsp`, copied out of the index after one refresh. */
		void CollectPipelineShaderRoots(TArray<TPair<FString, TArray<FString>>>& OutRoots)
		{
			OutRoots.Reset();
			FDreamShaderProductIndex& Index = FDreamShaderProductIndex::Get();
			Index.Refresh();

			TArray<const FDreamShaderProductRecord*> Pipelines;
			Index.FindPipelines(Pipelines);
			for (const FDreamShaderProductRecord* Record : Pipelines)
			{
				if (!Record)
				{
					continue;
				}
				TPair<FString, TArray<FString>>* Entry = OutRoots.FindByPredicate([Record](const TPair<FString, TArray<FString>>& Existing)
				{
					return Existing.Key.Equals(Record->SourceFilePath, ESearchCase::IgnoreCase);
				});
				if (!Entry)
				{
					Entry = &OutRoots.Emplace_GetRef(Record->SourceFilePath, TArray<FString>());
				}
				for (const FString& File : Record->ShaderFiles)
				{
					Entry->Value.AddUnique(File);
				}
			}
		}

		bool ReadsDifferently(const FString& FilePath, const FString& Text)
		{
			FString Existing;
			return !FFileHelper::LoadFileToString(Existing, *FilePath) || !Existing.Equals(Text, ESearchCase::CaseSensitive);
		}
	}

	const TCHAR* LexDreamPassSlotState(const EDreamPassSlotState State)
	{
		switch (State)
		{
		case EDreamPassSlotState::Live:            return TEXT("Live");
		case EDreamPassSlotState::Reserved:        return TEXT("Reserved");
		case EDreamPassSlotState::SnapshotMissing: return TEXT("SnapshotMissing");
		case EDreamPassSlotState::PipelineGone:    return TEXT("PipelineGone");
		case EDreamPassSlotState::PassGone:        return TEXT("PassGone");
		case EDreamPassSlotState::Unknown:         return TEXT("Unknown");
		}
		return TEXT("Unknown");
	}

	// ------------------------------------------------------------------------------------------------ the registry

	bool DescribeDreamPassRegistry(const bool bClassify, FDreamPassRegistryReport& OutReport, FString& OutError)
	{
		using namespace DreamShaderPassPipelinesDetail;

		OutReport = FDreamPassRegistryReport();
		OutReport.RegistryJsonPath = GetDreamPassRegistryJsonPath();
		OutReport.ComputeSlotCount = ::UE::DreamPass::GetComputeSlotCount();
		OutReport.PixelSlotCount = ::UE::DreamPass::GetPixelSlotCount();

		FDreamPassRegistry Registry;
		if (!LoadDreamPassRegistry(Registry, OutError))
		{
			return false;
		}

		// Every index question first, answered and copied: the front ends run below refresh the index themselves.
		TMap<FString, FPipelineOwner> Owners;
		if (bClassify)
		{
			FDreamShaderProductIndex& Index = FDreamShaderProductIndex::Get();
			Index.Refresh();
			for (const bool bCompute : { true, false })
			{
				for (const FDreamPassRegistrySlot& Slot : Registry.Table(bCompute))
				{
					if (Owners.Contains(Slot.Pipeline))
					{
						continue;
					}
					FPipelineOwner Owner;
					const FDreamShaderProductRecord* Record = Index.FindByObjectPath(Slot.Pipeline);
					if (Record && Record->Kind == IR::EIRProductKind::PassPipeline)
					{
						Owner.SourceFile = Record->SourceFilePath;
					}
					else
					{
						// Gone means gone on evidence: the file the registry recorded is not there any more, or it is and
						// builds another pipeline now. One that is there and was not indexed at all -- a preprocessor
						// error stops the index's parse -- proves nothing.
						const FString Recorded = ResolveRecordedSource(Slot.Source);
						if (!Recorded.IsEmpty() && IFileManager::Get().FileExists(*Recorded))
						{
							TArray<const FDreamShaderProductRecord*> RecordedProducts;
							Index.FindBySource(Recorded, RecordedProducts);
							Owner.bRecordedSourceUnindexed = RecordedProducts.IsEmpty();
						}
					}
					Owners.Add(Slot.Pipeline, MoveTemp(Owner));
				}
			}
		}

		TMap<FString, FSourcePasses> SourceCache;
		for (const bool bCompute : { true, false })
		{
			for (const FDreamPassRegistrySlot& Slot : Registry.Table(bCompute))
			{
				FDreamPassSlotReport& Out = OutReport.Slots.AddDefaulted_GetRef();
				Out.bCompute = bCompute;
				Out.Slot = Slot.Slot;
				Out.Pipeline = Slot.Pipeline;
				Out.Pass = Slot.Pass;
				Out.Source = Slot.Source;
				Out.Shader = Slot.Shader;
				Out.Entry = Slot.Entry;
				Out.Hash = Slot.Hash;
				Out.Formats = Slot.Formats;
				if (!Slot.HasSnapshot())
				{
					Out.State = EDreamPassSlotState::Reserved;
				}
				else if (!AreDreamPassSnapshotFilesPresent(bCompute, Slot))
				{
					Out.State = EDreamPassSlotState::SnapshotMissing;
				}

				if (!bClassify)
				{
					continue;
				}

				const FPipelineOwner& Owner = Owners.FindChecked(Slot.Pipeline);
				if (Owner.SourceFile.IsEmpty())
				{
					if (Owner.bRecordedSourceUnindexed)
					{
						Out.State = EDreamPassSlotState::Unknown;
						continue;
					}
					Out.State = EDreamPassSlotState::PipelineGone;
					Out.bPipelineAssetExists = FindObject<UObject>(nullptr, *Slot.Pipeline) != nullptr
						|| FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Slot.Pipeline));
					continue;
				}

				const FSourcePasses Passes = GetSourcePasses(Owner.SourceFile, SourceCache);
				if (!Passes.bOk)
				{
					if (Out.State == EDreamPassSlotState::Live)
					{
						Out.State = EDreamPassSlotState::Unknown;
					}
					continue;
				}
				const bool bStillRun = Passes.Passes.ContainsByPredicate([&Slot, bCompute](const FSourcePass& Pass)
				{
					return Pass.bCompute == bCompute
						&& Pass.Pipeline.Equals(Slot.Pipeline, ESearchCase::IgnoreCase)
						&& Pass.Pass.Equals(Slot.Pass, ESearchCase::CaseSensitive);
				});
				if (!bStillRun)
				{
					Out.State = EDreamPassSlotState::PassGone;
				}
			}
		}
		return true;
	}

	bool CollectDreamPassRegistryGarbage(TArray<FDreamPassSlotReport>& OutFreed, Lang::FLangDiagnosticSink& Diagnostics)
	{
		OutFreed.Reset();
		const Lang::FLangSpan NoSpan;

		FDreamPassRegistryReport Report;
		FString LoadError;
		FDreamPassRegistry Registry;
		if (!DescribeDreamPassRegistry(/*bClassify*/ true, Report, LoadError) || !LoadDreamPassRegistry(Registry, LoadError))
		{
			return Diagnostics.Error(TEXT("DSH8335"), NoSpan, FText::Format(
				LOCTEXT("RegistryUnreadableForTool", "The Custom Pass slot registry cannot be read: {0}. Nothing was changed; 'dsc pass-registry -Rebuild' replaces a registry that does not parse."),
				FText::FromString(LoadError)));
		}
		const FDreamPassRegistry Previous = Registry;

		bool bComputeChanged = false;
		bool bPixelChanged = false;
		for (const FDreamPassSlotReport& Slot : Report.Slots)
		{
			if (Slot.State != EDreamPassSlotState::PipelineGone && Slot.State != EDreamPassSlotState::PassGone)
			{
				continue;
			}
			Registry.Table(Slot.bCompute).RemoveAll([&Slot](const FDreamPassRegistrySlot& Entry)
			{
				return Entry.Slot == Slot.Slot;
			});
			(Slot.bCompute ? bComputeChanged : bPixelChanged) = true;
			OutFreed.Add(Slot);
		}

		if (!WriteDreamPassRegistry(Registry, &Previous, Diagnostics))
		{
			return false;
		}
		DeleteStrayDreamPassSlotDirectories(Registry);
		HotReloadDreamPassShaders(bComputeChanged, bPixelChanged);
		return true;
	}

	bool ResetUnreadableDreamPassRegistry(FString& OutMovedTo, Lang::FLangDiagnosticSink& Diagnostics)
	{
		OutMovedTo.Reset();

		FDreamPassRegistry Registry;
		FString LoadError;
		if (LoadDreamPassRegistry(Registry, LoadError))
		{
			return true;
		}

		using namespace DreamShaderPassPipelinesDetail;

		const Lang::FLangSpan NoSpan;
		const FString Path = GetDreamPassRegistryJsonPath();
		const FString Aside = Path + TEXT(".unreadable");

		// Asked before the move, as every registry write asks before it touches anything: moved aside and then refused the
		// empty registry files, the reset would leave no Registry.json beside the registry files it no longer matches.
		{
			TArray<FString> FilesToWrite;
			FilesToWrite.Add(Path);
			FilesToWrite.Add(Aside);
			for (const bool bCompute : { true, false })
			{
				const FString RegistryFile = ::UE::DreamPass::GetRegistryFilePath(bCompute);
				if (ReadsDifferently(RegistryFile, BuildDreamPassRegistryShaderText(FDreamPassRegistry(), bCompute)))
				{
					FilesToWrite.Add(RegistryFile);
				}
			}
			if (!CheckDreamPassRegistryWritable(FilesToWrite, TArray<FString>(), Diagnostics))
			{
				return false;
			}
		}

		if (!IFileManager::Get().Move(*Aside, *Path, /*Replace*/ true))
		{
			return Diagnostics.Error(TEXT("DSH8336"), NoSpan, FText::Format(
				LOCTEXT("RegistryMoveAsideFailed", "The Custom Pass slot registry cannot be read ({0}) and could not be moved aside to '{1}', so nothing was reset. Another process holding one of the two files open is the usual reason; a read-only one is reported before this."),
				FText::FromString(LoadError),
				FText::FromString(Aside)));
		}
		OutMovedTo = Aside;

		// Empty, the registry files included: the compiles that follow assign every slot afresh and write their snapshots,
		// and the rewrite that ends a rebuild deletes the slot directories none of them took.
		return WriteDreamPassRegistry(FDreamPassRegistry(), nullptr, Diagnostics);
	}

	bool RewriteDreamPassRegistryFiles(int32& OutReserved, Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamShaderPassPipelinesDetail;

		OutReserved = 0;
		const Lang::FLangSpan NoSpan;

		FDreamPassRegistry Registry;
		FString LoadError;
		if (!LoadDreamPassRegistry(Registry, LoadError))
		{
			return Diagnostics.Error(TEXT("DSH8335"), NoSpan, FText::Format(
				LOCTEXT("RegistryUnreadableForTool", "The Custom Pass slot registry cannot be read: {0}. Nothing was changed; 'dsc pass-registry -Rebuild' replaces a registry that does not parse."),
				FText::FromString(LoadError)));
		}
		const FDreamPassRegistry Previous = Registry;

		for (const bool bCompute : { true, false })
		{
			for (FDreamPassRegistrySlot& Slot : Registry.Table(bCompute))
			{
				if (!Slot.HasSnapshot() || AreDreamPassSnapshotFilesPresent(bCompute, Slot))
				{
					continue;
				}
				Diagnostics.Warning(TEXT("DSH8337"), NoSpan, FText::Format(
					LOCTEXT("SnapshotMissingReserved", "{0} slot {1} ({2}, pass '{3}') names snapshot files that are not on disk, so it is now reserved and compiles to the empty stub: a registry that includes a missing file fails the global shader compile. Compile '{4}' to give the pass its snapshot back, and commit the Slots folder with the registry."),
					bCompute ? LOCTEXT("ComputeSlotWord", "Compute") : LOCTEXT("PixelSlotWord", "Pixel"),
					FText::AsNumber(Slot.Slot),
					FText::FromString(Slot.Pipeline),
					FText::FromString(Slot.Pass),
					FText::FromString(Slot.Source)));
				Slot.Hash.Reset();
				Slot.Section.Reset();
				Slot.Formats.Reset();
				Slot.Files.Reset();
				++OutReserved;
			}
		}

		const bool bComputeChanged = ReadsDifferently(::UE::DreamPass::GetRegistryFilePath(true), BuildDreamPassRegistryShaderText(Registry, true));
		const bool bPixelChanged = ReadsDifferently(::UE::DreamPass::GetRegistryFilePath(false), BuildDreamPassRegistryShaderText(Registry, false));
		if (!WriteDreamPassRegistry(Registry, &Previous, Diagnostics))
		{
			return false;
		}
		DeleteStrayDreamPassSlotDirectories(Registry);
		HotReloadDreamPassShaders(bComputeChanged, bPixelChanged);
		return true;
	}

	// ------------------------------------------------------------------------------------------------- the check

	bool CheckDreamShaderPipelineSlots(
		const FString& SourceFilePath,
		const TArray<FName>& Formats,
		Lang::FLangDiagnosticSink& Diagnostics,
		int32& OutPassesChecked)
	{
		OutPassesChecked = 0;
		const Lang::FLangSpan NoSpan;

#if !DREAMSHADER_WITH_CUSTOM_PASS
		(void)SourceFilePath;
		(void)Formats;
		return Diagnostics.Error(TEXT("DSH8338"), NoSpan, LOCTEXT("SlotCheckNeedsCustomPass",
			"HLSL slots need Unreal Engine 5.8 or later; this engine has no Custom Pass runtime to pre-check them for."));
#else
		FDreamShaderLang2PipelineResult Run;
		if (!RunDreamShaderPipelineToIR(SourceFilePath, Run) || !Run.IR.IsValid())
		{
			const bool bExplained = Run.Diagnostics.HasErrors();
			Diagnostics.Append(MoveTemp(Run.Diagnostics));
			if (bExplained)
			{
				return false;
			}
			return Diagnostics.Error(TEXT("DSH8338"), NoSpan, FText::Format(
				LOCTEXT("SlotCheckNoIR", "'{0}' did not get as far as its pipeline, so its HLSL slots could not be pre-checked."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		}

		bool bOk = true;
		for (const IR::FIRProduct& Product : Run.IR->Products)
		{
			if (Product.Kind != IR::EIRProductKind::PassPipeline)
			{
				continue;
			}

			FString PackageName;
			FString ObjectPath;
			FString LeafName;
			FDreamShaderError DestinationError;
			if (!ResolveIRProductObjectPath(Product, Run.SourceFilePath, PackageName, ObjectPath, LeafName, DestinationError))
			{
				bOk = Diagnostics.Error(TEXT("DSH8338"), Product.Source.Span, FText::Format(
					LOCTEXT("SlotCheckNoDestination", "'{0}' does not resolve to an asset path ({1}), so its HLSL slots could not be pre-checked."),
					FText::FromString(Product.Name),
					FText::FromString(DestinationError.Message)));
				continue;
			}

			// The emitter's own staging, onto a transient pipeline nobody else keeps -- held strongly, as the emitter holds its own,
			// so nothing that collects garbage while the slots are checked can take it away.
			TStrongObjectPtr<UDreamPassPipeline> StagedHolder(NewObject<UDreamPassPipeline>(GetTransientPackage(), NAME_None, RF_Transient));
			UDreamPassPipeline* Staged = StagedHolder.Get();
			ON_SCOPE_EXIT
			{
				Staged->MarkAsGarbage();
				StagedHolder.Reset();
			};
			if (!BuildDreamPassPipelineFromPayload(Product, *Staged, Diagnostics))
			{
				bOk = false;
				continue;
			}

			TArray<FDreamPassSlotCandidate> Candidates;
			MakeDreamPassSlotCandidates(Product.PassPipeline, *Staged, Candidates);
			if (Candidates.IsEmpty())
			{
				Diagnostics.Info(TEXT("DSH8339"), Product.Source.Span, FText::Format(
					LOCTEXT("SlotCheckNothing", "'{0}' has no HLSL pass, so there is no slot to pre-check; its materials are checked by the sources that build them."),
					FText::FromString(Product.Name)));
				continue;
			}

			// Planned against the registry as it stands -- a pass that has a slot keeps it, a new one is checked in the slot it
			// would take -- and nothing of the plan is committed.
			FDreamPassSlotPlan Plan;
			if (!PlanDreamPassSlots(*Staged, ObjectPath, Run.SourceFilePath, MoveTemp(Candidates), Plan, Diagnostics))
			{
				bOk = false;
				continue;
			}
			OutPassesChecked += Plan.Candidates.Num();
			if (!PrecheckDreamPassSlots(Plan, Formats, /*bRecheckUnchanged*/ true, Diagnostics))
			{
				bOk = false;
			}
		}
		return bOk;
#endif
	}

	// --------------------------------------------------------------------------------------------- the shader files

	bool IsDreamShaderPassShaderFile(const FString& Path)
	{
		return IsDreamPassShaderFileExtension(Path);
	}

	void CollectDreamShaderPipelineShaderFiles(const FString& PipelineSourceFile, TArray<FString>& OutShaderFiles)
	{
		using namespace DreamShaderPassPipelinesDetail;

		OutShaderFiles.Reset();

		TArray<FString> Roots;
		{
			FDreamShaderProductIndex& Index = FDreamShaderProductIndex::Get();
			Index.Refresh();
			TArray<const FDreamShaderProductRecord*> Records;
			Index.FindBySource(PipelineSourceFile, Records);
			for (const FDreamShaderProductRecord* Record : Records)
			{
				if (Record && Record->Kind == IR::EIRProductKind::PassPipeline)
				{
					for (const FString& File : Record->ShaderFiles)
					{
						Roots.AddUnique(File);
					}
				}
			}
		}

		for (const FString& Root : Roots)
		{
			AppendShaderClosureFiles(Root, OutShaderFiles);
		}
	}

	void FindDreamShaderPipelinesUsingShaderFile(const FString& ShaderFile, TArray<FString>& OutPipelineSourceFiles)
	{
		using namespace DreamShaderPassPipelinesDetail;

		OutPipelineSourceFiles.Reset();
		const FString Wanted = NormalizeDreamPassShaderFilePath(ShaderFile);

		TArray<TPair<FString, TArray<FString>>> PipelineRoots;
		CollectPipelineShaderRoots(PipelineRoots);
		for (const TPair<FString, TArray<FString>>& Pipeline : PipelineRoots)
		{
			TArray<FString> Files;
			for (const FString& Root : Pipeline.Value)
			{
				AppendShaderClosureFiles(Root, Files);
			}
			if (Files.ContainsByPredicate([&Wanted](const FString& File) { return File.Equals(Wanted, ESearchCase::IgnoreCase); }))
			{
				OutPipelineSourceFiles.AddUnique(Pipeline.Key);
			}
		}
	}

	void CollectDreamShaderPipelineShaderDirectories(TArray<FString>& OutDirectories)
	{
		using namespace DreamShaderPassPipelinesDetail;

		OutDirectories.Reset();

		TArray<TPair<FString, TArray<FString>>> PipelineRoots;
		CollectPipelineShaderRoots(PipelineRoots);
		TArray<FString> Files;
		for (const TPair<FString, TArray<FString>>& Pipeline : PipelineRoots)
		{
			for (const FString& Root : Pipeline.Value)
			{
				AppendShaderClosureFiles(Root, Files);
			}
		}
		for (const FString& File : Files)
		{
			const FString Directory = FPaths::GetPath(File);
			if (!Directory.IsEmpty())
			{
				OutDirectories.AddUnique(Directory);
			}
		}
		OutDirectories.Sort([](const FString& Left, const FString& Right)
		{
			return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
		});
	}
}

#undef LOCTEXT_NAMESPACE
