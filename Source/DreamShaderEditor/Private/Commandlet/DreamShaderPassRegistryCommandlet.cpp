// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderPassRegistryCommandlet.h.
//
// Diagnostics owned by this file: DSH9200-DSH9209.
//
// The verbs only orchestrate: judging, collecting and rewriting the registry are the compiler's
// (DreamShaderPassPipelines.h), and `-Rebuild` compiles through the same entry `compile` does. What this file adds is the
// one judgement the compiler cannot make alone -- after a rebuild, a pipeline whose source did not compile still points its
// passes at the slots it had, and if the registry gave one of them away, that pass would run another pipeline's shader.
// Those are reported pass by pass.

#include "Commandlet/DreamShaderPassRegistryCommandlet.h"

#include "Commandlet/DreamShaderCommandletRunner.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "DreamShaderPassPipelines.h"
#include "DreamShaderProductIndex.h"
#include "DreamShaderSourceFileUtils.h"
#include "DreamShaderTextWireUtils.h"
#include "Lang/LangDiagnostic.h"

#include "DreamPassPipeline.h"
#include "DreamPassTypes.h"

#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShader.PassRegistryCommandlet"

namespace UE::DreamShader::Editor::Private
{
	namespace DreamShaderPassRegistryCommandletDetail
	{
		using ::UE::DreamShader::Editor::Compiler::EDreamPassSlotState;
		using ::UE::DreamShader::Editor::Compiler::FDreamPassRegistryReport;
		using ::UE::DreamShader::Editor::Compiler::FDreamPassSlotReport;
		using ::UE::DreamShader::Lang::FLangDiagnosticSink;
		using ::UE::DreamShader::Lang::FLangSpan;

		/** `-Gc`, `--gc`, `-Gc=true`: the runner's flag rule (its HasCommandletFlag is file-static there). */
		bool HasPassRegistryFlag(const TArray<FString>& Tokens, const TArray<FString>& Switches, const TCHAR* Name)
		{
			auto Matches = [Name](const FString& Text) -> bool
			{
				FString Key;
				FString Value;
				if (!TrySplitCommandletAssignment(Text, Key, Value))
				{
					return NormalizeCommandletKey(Text).Equals(Name, ESearchCase::IgnoreCase);
				}
				if (!Key.Equals(Name, ESearchCase::IgnoreCase))
				{
					return false;
				}
				const FString Lower = Value.ToLower();
				return !(Lower == TEXT("0") || Lower == TEXT("false") || Lower == TEXT("no") || Lower == TEXT("off"));
			};
			return Switches.ContainsByPredicate(Matches) || Tokens.ContainsByPredicate(Matches);
		}

		void LogPassRegistrySummary(const bool bSucceeded, const FString& Line)
		{
			const FString Full = FString::Printf(TEXT("%s RESULT=%s"), *Line, bSucceeded ? TEXT("OK") : TEXT("FAILED")); /* I18N-EXEMPT: machine-readable verdict line */
			if (bSucceeded)
			{
				UE_LOG(LogDreamShader, Display, TEXT("%s"), *Full);
			}
			else
			{
				UE_LOG(LogDreamShader, Error, TEXT("%s"), *Full);
			}
		}

		FText TableWord(const bool bCompute)
		{
			return bCompute ? LOCTEXT("ComputeWord", "compute") : LOCTEXT("PixelWord", "pixel");
		}

		FString MakeSlotLabel(const FDreamPassSlotReport& Slot)
		{
			return FString::Printf(TEXT("%s%02d"), Slot.bCompute ? TEXT("C") : TEXT("P"), Slot.Slot); /* I18N-EXEMPT: slot label */
		}

		/** One line per slot, columns for the eye; the states are the words `pass-registry` documents. */
		void LogSlotTable(const FDreamPassRegistryReport& Report)
		{
			for (const FDreamPassSlotReport& Slot : Report.Slots)
			{
				UE_LOG(LogDreamShader, Display, TEXT("  %s  %-15s  %s  pass '%s'  %s : %s  [%s]"), /* I18N-EXEMPT: machine-readable listing */
					*MakeSlotLabel(Slot),
					::UE::DreamShader::Editor::Compiler::LexDreamPassSlotState(Slot.State),
					*Slot.Pipeline,
					*Slot.Pass,
					Slot.Shader.IsEmpty() ? TEXT("-") : *Slot.Shader,
					Slot.Entry.IsEmpty() ? TEXT("-") : *Slot.Entry,
					*FString::Join(Slot.Formats, TEXT(", ")));
			}
		}

		/**
		 * What the listing and `-Gc` say about slots that need a hand, each once. A rebuild leaves the missing snapshots out:
		 * its last step turns those slots into reserved ones and says so itself (DSH8337).
		 */
		void ReportSlotProblems(const FDreamPassRegistryReport& Report, const bool bWarnMissingSnapshots, FLangDiagnosticSink& Sink)
		{
			const FLangSpan NoSpan;
			for (const FDreamPassSlotReport& Slot : Report.Slots)
			{
				switch (Slot.State)
				{
				case EDreamPassSlotState::PipelineGone:
					if (Slot.bPipelineAssetExists)
					{
						Sink.Warning(TEXT("DSH9201"), NoSpan, FText::Format(
							LOCTEXT("PipelineAssetOrphaned", "No .dsp builds '{0}' any more, but the asset is still there and its pass '{1}' points at {2} slot {3}. Delete the asset or restore its source: once the slot is collected and given to another pass, that pass's shader is what this one would run."),
							FText::FromString(Slot.Pipeline),
							FText::FromString(Slot.Pass),
							TableWord(Slot.bCompute),
							FText::AsNumber(Slot.Slot)));
					}
					break;

				case EDreamPassSlotState::SnapshotMissing:
					if (!bWarnMissingSnapshots)
					{
						break;
					}
					Sink.Warning(TEXT("DSH9208"), NoSpan, FText::Format(
						LOCTEXT("SnapshotMissing", "The snapshot of {0} slot {1} (pass '{2}' of '{3}') is not on disk, and the registry file includes it: the global shaders fail to compile at the next start. Compile '{4}', or run 'dsc pass-registry --rebuild'; commit the Slots folder with the registry."),
						TableWord(Slot.bCompute),
						FText::AsNumber(Slot.Slot),
						FText::FromString(Slot.Pass),
						FText::FromString(Slot.Pipeline),
						FText::FromString(Slot.Source)));
					break;

				case EDreamPassSlotState::Unknown:
					Sink.Warning(TEXT("DSH9209"), NoSpan, FText::Format(
						LOCTEXT("SlotUnknown", "'{0}' does not compile far enough to tell whether it still runs pass '{1}' in {2} slot {3}, so the slot is kept. Fix the source and compile it."),
						FText::FromString(Slot.Source.IsEmpty() ? Slot.Pipeline : Slot.Source),
						FText::FromString(Slot.Pass),
						TableWord(Slot.bCompute),
						FText::AsNumber(Slot.Slot)));
					break;

				case EDreamPassSlotState::Live:
				case EDreamPassSlotState::Reserved:
				case EDreamPassSlotState::PassGone:
					break;
				}
			}
		}

		void CountTables(const FDreamPassRegistryReport& Report, int32& OutCompute, int32& OutPixel, int32& OutGarbage)
		{
			OutCompute = 0;
			OutPixel = 0;
			OutGarbage = 0;
			for (const FDreamPassSlotReport& Slot : Report.Slots)
			{
				++(Slot.bCompute ? OutCompute : OutPixel);
				if (Slot.State == EDreamPassSlotState::PipelineGone || Slot.State == EDreamPassSlotState::PassGone)
				{
					++OutGarbage;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------ the verbs

		bool RunList(FLangDiagnosticSink& Sink)
		{
			const FLangSpan NoSpan;
			FDreamPassRegistryReport Report;
			FString LoadError;
			if (!::UE::DreamShader::Editor::Compiler::DescribeDreamPassRegistry(/*bClassify*/ true, Report, LoadError))
			{
				Sink.Error(TEXT("DSH9200"), NoSpan, FText::Format(
					LOCTEXT("RegistryUnreadable", "The Custom Pass slot registry cannot be read: {0}. 'dsc pass-registry --rebuild' moves it aside and gives every HLSL pass a slot again."),
					FText::FromString(LoadError)));
				::UE::DreamShader::Editor::Compiler::LogLang2Diagnostics(Sink, FString());
				LogPassRegistrySummary(false, TEXT("DreamShader pass-registry: the registry could not be read.")); /* I18N-EXEMPT: machine-readable verdict line */
				return false;
			}

			UE_LOG(LogDreamShader, Display, TEXT("DreamShader pass-registry: %s"), *Report.RegistryJsonPath); /* I18N-EXEMPT: machine-readable listing */
			LogSlotTable(Report);
			ReportSlotProblems(Report, /*bWarnMissingSnapshots*/ true, Sink);
			::UE::DreamShader::Editor::Compiler::LogLang2Diagnostics(Sink, Report.RegistryJsonPath);

			int32 Compute = 0;
			int32 Pixel = 0;
			int32 Garbage = 0;
			CountTables(Report, Compute, Pixel, Garbage);
			LogPassRegistrySummary(true, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
				TEXT("DreamShader pass-registry: %d of %d compute slot(s), %d of %d pixel slot(s) taken; %d to collect with --gc."),
				Compute,
				Report.ComputeSlotCount,
				Pixel,
				Report.PixelSlotCount,
				Garbage));
			return true;
		}

		/** `-Gc`, alone or as the third step of `-Rebuild`. Raises one Info per freed slot. */
		bool CollectGarbage(const bool bWarnMissingSnapshots, FLangDiagnosticSink& Sink, int32& OutFreed)
		{
			const FLangSpan NoSpan;
			OutFreed = 0;

			// The warnings first, from the same judgement the collection makes: an orphaned asset is worth saying before its
			// slot goes.
			FDreamPassRegistryReport Before;
			FString LoadError;
			if (::UE::DreamShader::Editor::Compiler::DescribeDreamPassRegistry(/*bClassify*/ true, Before, LoadError))
			{
				ReportSlotProblems(Before, bWarnMissingSnapshots, Sink);
			}

			TArray<FDreamPassSlotReport> Freed;
			if (!::UE::DreamShader::Editor::Compiler::CollectDreamPassRegistryGarbage(Freed, Sink))
			{
				return false;
			}
			for (const FDreamPassSlotReport& Slot : Freed)
			{
				Sink.Info(TEXT("DSH9202"), NoSpan, FText::Format(
					LOCTEXT("SlotCollected", "{0} slot {1} was freed: {2}"),
					TableWord(Slot.bCompute),
					FText::AsNumber(Slot.Slot),
					Slot.State == EDreamPassSlotState::PipelineGone
						? FText::Format(LOCTEXT("CollectedPipelineGone", "no .dsp builds '{0}' any more."), FText::FromString(Slot.Pipeline))
						: FText::Format(LOCTEXT("CollectedPassGone", "'{0}' no longer runs pass '{1}' in HLSL there."), FText::FromString(Slot.Pipeline), FText::FromString(Slot.Pass))));
			}
			OutFreed = Freed.Num();
			return true;
		}

		bool RunGc(FLangDiagnosticSink& Sink)
		{
			int32 Freed = 0;
			const bool bOk = CollectGarbage(/*bWarnMissingSnapshots*/ true, Sink, Freed);
			::UE::DreamShader::Editor::Compiler::LogLang2Diagnostics(Sink, FString());
			LogPassRegistrySummary(bOk, FString::Printf(TEXT("DreamShader pass-registry --gc: %d slot(s) freed."), Freed)); /* I18N-EXEMPT: machine-readable verdict line */
			return bOk;
		}

		/**
		 * After a rebuild: every pass of a pipeline whose source did not compile, checked against the registry. A slot the
		 * registry records for another pass, or for none, is one that pass must not run in.
		 */
		void ReportStaleSlots(const TArray<FString>& FailedSources, FLangDiagnosticSink& Sink)
		{
			if (FailedSources.IsEmpty())
			{
				return;
			}

			const FLangSpan NoSpan;
			FDreamPassRegistryReport Report;
			FString LoadError;
			if (!::UE::DreamShader::Editor::Compiler::DescribeDreamPassRegistry(/*bClassify*/ false, Report, LoadError))
			{
				return;
			}

			// Copied out of the index before anything loads: a refresh invalidates every record pointer.
			TArray<TPair<FString, FString>> Pipelines;
			{
				::UE::DreamShader::Editor::Compiler::FDreamShaderProductIndex& Index = ::UE::DreamShader::Editor::Compiler::FDreamShaderProductIndex::Get();
				Index.Refresh();
				for (const FString& Source : FailedSources)
				{
					TArray<const ::UE::DreamShader::Editor::Compiler::FDreamShaderProductRecord*> Records;
					Index.FindBySource(Source, Records);
					for (const ::UE::DreamShader::Editor::Compiler::FDreamShaderProductRecord* Record : Records)
					{
						if (Record && Record->Kind == ::UE::DreamShader::IR::EIRProductKind::PassPipeline)
						{
							Pipelines.Emplace(Record->ObjectPath, Source);
						}
					}
				}
			}

			for (const TPair<FString, FString>& Entry : Pipelines)
			{
				const FString PackageName = FPackageName::ObjectPathToPackageName(Entry.Key);
				UDreamPassPipeline* Pipeline = FindObject<UDreamPassPipeline>(nullptr, *Entry.Key);
				if (!Pipeline && FPackageName::DoesPackageExist(PackageName))
				{
					Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *Entry.Key);
				}
				if (!Pipeline)
				{
					continue;
				}

				for (const FDreamPassDesc& Pass : Pipeline->Passes)
				{
					bool bCompute = false;
					int32 SlotNumber = INDEX_NONE;
					if (Pass.Kind == EDreamPassKind::Compute)
					{
						bCompute = true;
						SlotNumber = Pass.Compute.Slot;
					}
					else if (Pass.Kind == EDreamPassKind::Fullscreen)
					{
						SlotNumber = Pass.Fullscreen.PixelSlot;
					}
					if (SlotNumber == INDEX_NONE)
					{
						continue;
					}

					const FDreamPassSlotReport* Recorded = Report.Slots.FindByPredicate([bCompute, SlotNumber](const FDreamPassSlotReport& Slot)
					{
						return Slot.bCompute == bCompute && Slot.Slot == SlotNumber;
					});
					const bool bOwn = Recorded
						&& Recorded->Pipeline.Equals(Entry.Key, ESearchCase::IgnoreCase)
						&& Recorded->Pass.Equals(Pass.Name.ToString(), ESearchCase::CaseSensitive);
					if (bOwn)
					{
						continue;
					}
					Sink.Warning(TEXT("DSH9205"), NoSpan, FText::Format(
						LOCTEXT("StaleSlot", "Pass '{1}' of '{0}' points at {2} slot {3}, which the registry {4}. Its source '{5}' did not compile, so the asset was not updated: fix the source and compile it, or that pass runs whatever the slot holds."),
						FText::FromString(Entry.Key),
						FText::FromName(Pass.Name),
						TableWord(bCompute),
						FText::AsNumber(SlotNumber),
						Recorded
							? FText::Format(LOCTEXT("StaleSlotTaken", "gives to pass '{0}' of '{1}'"), FText::FromString(Recorded->Pass), FText::FromString(Recorded->Pipeline))
							: LOCTEXT("StaleSlotFree", "leaves free"),
						FText::FromString(Entry.Value)));
				}
			}
		}

		bool RunRebuild(FLangDiagnosticSink& Sink)
		{
			const FLangSpan NoSpan;
			bool bOk = true;

			// 1. A registry that does not parse cannot be planned against: aside with it, and every slot is assigned afresh.
			FString MovedTo;
			if (!::UE::DreamShader::Editor::Compiler::ResetUnreadableDreamPassRegistry(MovedTo, Sink))
			{
				::UE::DreamShader::Editor::Compiler::LogLang2Diagnostics(Sink, FString());
				LogPassRegistrySummary(false, TEXT("DreamShader pass-registry --rebuild: the unreadable registry could not be reset.")); /* I18N-EXEMPT: machine-readable verdict line */
				return false;
			}
			if (!MovedTo.IsEmpty())
			{
				Sink.Warning(TEXT("DSH9203"), NoSpan, FText::Format(
					LOCTEXT("RegistryReset", "Registry.json did not parse and was moved aside to '{0}'; the compiles that follow give every HLSL pass a slot afresh."),
					FText::FromString(MovedTo)));
			}

			// 2. Every `.dsp`, forced: each one replans its slots against the registry and writes the snapshots it is missing.
			// Through the compiler service, as `compile` goes: its report names every asset written, which dsc.ps1 lists.
			::UE::DreamShader::IDreamShaderCompiler* const Compiler = ::UE::DreamShader::GetDreamShaderCompiler();
			TArray<FString> Sources;
			FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(Sources);
			Sources.RemoveAll([](const FString& Source)
			{
				return !::UE::DreamShader::IsDreamShaderPipelineFile(Source);
			});
			Sources.Sort([](const FString& Left, const FString& Right)
			{
				return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
			});
			if (Sources.IsEmpty())
			{
				Sink.Info(TEXT("DSH9206"), NoSpan, LOCTEXT("NoPipelines", "No .dsp under the source roots; the registry is rewritten from Registry.json as it stands."));
			}

			int32 Compiled = 0;
			TArray<FString> Failed;
			for (const FString& Source : Sources)
			{
				FString Report;
				bool bCompiled = false;
				if (Compiler)
				{
					// Materialized: a commandlet has no editor session to keep an Ephemeral product alive in.
					::UE::DreamShader::FDreamShaderCompileRequest Request;
					Request.SourceFilePath = Source;
					Request.bForce = true;
					Request.ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Materialized;
					const ::UE::DreamShader::FDreamShaderCompileResult Result = Compiler->CompileAssets(Request);
					Report = ToInvariantWireString(Result.Message);
					bCompiled = Result.bSucceeded;
				}
				else
				{
					Report = TEXT("the DreamShaderCompiler module is not available in this process"); /* I18N-EXEMPT: wrapped by DSH9204 */
				}

				if (bCompiled)
				{
					++Compiled;
					UE_LOG(LogDreamShader, Display, TEXT("%s"), *Report);
					continue;
				}
				Failed.Add(Source);
				bOk = Sink.Error(TEXT("DSH9204"), NoSpan, FText::Format(
					LOCTEXT("RebuildCompileFailed", "'{0}' did not compile, so its pipeline keeps the slots it had: {1}"),
					FText::FromString(Source),
					FText::FromString(Report)));
			}

			// 3. The slots of passes and pipelines that are gone.
			int32 Freed = 0;
			if (!CollectGarbage(/*bWarnMissingSnapshots*/ false, Sink, Freed))
			{
				bOk = false;
			}

			// 4. The registry files from Registry.json, with a slot whose snapshot is missing turned into a reserved one.
			int32 Reserved = 0;
			if (!::UE::DreamShader::Editor::Compiler::RewriteDreamPassRegistryFiles(Reserved, Sink))
			{
				bOk = false;
			}

			// 5. What the failed sources left pointing at slots that are not theirs.
			ReportStaleSlots(Failed, Sink);

			::UE::DreamShader::Editor::Compiler::LogLang2Diagnostics(Sink, FString());
			LogPassRegistrySummary(bOk, FString::Printf( /* I18N-EXEMPT: machine-readable verdict line */
				TEXT("DreamShader pass-registry --rebuild: %d .dsp compiled, %d failed, %d slot(s) freed, %d slot(s) reserved for a missing snapshot."),
				Compiled,
				Failed.Num(),
				Freed,
				Reserved));
			return bOk;
		}
	}

	bool RunDreamShaderPassRegistryCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		using namespace DreamShaderPassRegistryCommandletDetail;
		(void)Params;

		FLangDiagnosticSink Sink;
		if (HasPassRegistryFlag(Tokens, Switches, TEXT("Rebuild")))
		{
			// The rebuild collects the garbage as its third step; -Gc beside it adds nothing.
			return RunRebuild(Sink);
		}
		if (HasPassRegistryFlag(Tokens, Switches, TEXT("Gc")))
		{
			return RunGc(Sink);
		}
		return RunList(Sink);
	}
}

#undef LOCTEXT_NAMESPACE
