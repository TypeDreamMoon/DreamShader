// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The compile pipeline. Implements DreamShaderCompilePipeline.h: RunDreamShaderLang2Pipeline, the one entry every
// compile goes through, ResolveDreamShaderSourceProducts, which runs its front half without emitting, and the thin
// CompileDreamShaderLang2File.
//
// The shape of the run, and who owns each step:
//
//   read            this file        FFileHelper::LoadFileToString                         DSH8290
//   preprocess      DreamShaderLang  PreprocessDreamShaderSource, dialect by file kind     DSH8291
//   parse           front end        ParseDreamShaderLang, front end by file kind (Auto)
//   catalog         emitter          GetDreamShaderBuiltinCatalog (cached reflection)
//   bind            binder           BindDreamShaderLang (+ this module's include resolver)
//   lower           IR builder       BuildDreamShaderIR, stamping source paths project-relative (B5)
//   passes          IR               RunDreamShaderIRPasses
//   validate        IR               ValidateDreamShaderIR
//   emit            emitter          EmitDreamShaderIRProduct, once per product, in dependency order
//
// One pipeline for every compilable kind since the compiler relocation (CONTRACT section 2.1, agreement A4): `.dss` and `.dsi` take the
// 2.0 front end, `.dsm` and `.dsf` the legacy one, and a `.dsh` included by any of them is parsed declaration by
// declaration. The parser's Auto front end makes that choice from the path; this file makes the matching choice of
// preprocessor dialect, here and in the include resolver, and everything after the parse is one chain.
//
// Two things this file deliberately does NOT do, both of which look like omissions until you look at what does them:
//
//   * It does not broadcast OnDreamShaderSourceGenerated or drain the RaiseGenerationWarning collector. The compiler
//     service does both, around the whole run, for the outermost compile only (DreamShaderCompilerService.cpp).
//     Doing it here as well would fire the event twice for every compile, and the Browser model rebuilds its view
//     on every broadcast.
//   * It does not check the source hash itself. FIREmitContext carries bForce and the emitter owns
//     IsGeneratedAssetSourceCurrent per product, because only the emitter knows which asset a product resolves to.
//     The hash IS computed here -- the emitter cannot, it never sees the text. What the emitter decided comes back
//     as an Info on the product's span (DSH8237 for a current hash, DSH8209 for another editor owning the write), and
//     WordDreamShaderPipelineReport words that product's result line from it in the 1.x words, so a skip never reads
//     as `Generated`.

#include "DreamShaderCompilePipeline.h"

#include "Pipeline/DreamShaderCompilePipelineInternal.h"
// A 1.x texture default's `Class'...'` shell, judged for every declaration as the 1.x parser judged it.
#include "Pipeline/DreamShaderLegacyTextureDefaults.h"

// CompileDreamShaderSourceFile, which CompileDreamShaderLang2File is a spelling of.
#include "DreamShaderCompilerServiceInternal.h"

// `.dsi`: parent resolution through the product index, and the parameter schema's two producers.
#include "DreamShaderInstanceSchema.h"
#include "DreamShaderProductIndex.h"
#include "IR/IRInstanceSchema.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

#include "DreamShaderBuiltinCatalog.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderCompilerIncludes.h"

// FIREmitContext, EmitDreamShaderIRProduct; ResolveIRProductObjectPath for product resolution.
#include "DreamShaderIREmitter.h"
#include "Emitter/DreamShaderIRAssets.h"
// The IR builder, passes and validator.
#include "IR/IRBuilder.h"
#include "IR/IRPasses.h"
#include "IR/IRValidator.h"

#include "DreamShaderDefineResolution.h"
#include "DreamShaderModule.h"
#include "DreamShaderPreprocessor.h"
#include "DreamShaderSettings.h"
#include "Lang/LangParser.h"
#include "DreamShaderGenerationProgress.h"
#include "DreamShaderGeneratedAssets.h"
#include "Semantic/LangBound.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "DreamShader.Pipeline"

namespace UE::DreamShader::Editor::Compiler
{
	// No namespace-scope `using namespace` in this file: the compiler module builds as one unity blob, and a directive
	// here would reach every later `Editor::Compiler` block of every other file in it (research-relocation section 4.7).
	// The function bodies that call into the front end and the IR open the two namespaces locally instead.

	bool IsDreamShaderLang2Source(const FString& SourceFilePath)
	{
		// `.dsh` answers false on purpose: a header is compiled only as part of the source that includes it, and
		// handing one to this pipeline directly would produce no product at all. This is the one place it is decided.
		switch (Lang::GetLangFileKindFromPath(SourceFilePath))
		{
		case Lang::ELangFileKind::Dss:
		case Lang::ELangFileKind::Dsi:
		case Lang::ELangFileKind::Dsm:
		case Lang::ELangFileKind::Dsf:
			return true;

		case Lang::ELangFileKind::Dsh:
		case Lang::ELangFileKind::Unknown:
			return false;
		}

		return false;
	}

	UE::DreamShader::EDreamShaderPreprocessDialect GetDreamShaderPreprocessDialectForFile(const FString& FilePath)
	{
		switch (Lang::GetLangFileKindFromPath(FilePath))
		{
		case Lang::ELangFileKind::Dsm:
		case Lang::ELangFileKind::Dsf:
			return UE::DreamShader::EDreamShaderPreprocessDialect::Legacy;

		case Lang::ELangFileKind::Dsh:
			// Both languages' opaque-body triggers: the front end decides per declaration which one a header uses.
			return UE::DreamShader::EDreamShaderPreprocessDialect::Mixed;

		case Lang::ELangFileKind::Dss:
		case Lang::ELangFileKind::Dsi:
		case Lang::ELangFileKind::Unknown:
			return UE::DreamShader::EDreamShaderPreprocessDialect::Lang2;
		}

		return UE::DreamShader::EDreamShaderPreprocessDialect::Lang2;
	}

	namespace
	{
		/** The 1.x `// Begin/End DreamShader source:` wrapper, for the build-key digest only. */
		FString MakeDigestBlock(const FString& FilePath, const FString& Text)
		{
			// The ABSOLUTE path, as the 1.x prepared-source digest hashed it
			// (MaterialAssetGeneration/DreamShaderMaterialGeneratorSourceLoading.cpp): the build key is compared on
			// this machine only, and keeping its input identical keeps a migrated source's key comparable (debt B5 (d)).
			FString Block;
			Block += FString::Printf(TEXT("// Begin DreamShader source: %s\n"), *FilePath); /* I18N-EXEMPT: build-key material, never displayed */
			Block += Text;
			Block += FString::Printf(TEXT("\n// End DreamShader source: %s\n\n"), *FilePath); /* I18N-EXEMPT: build-key material, never displayed */
			return Block;
		}

		/**
		 * Counts line terminators the way FString::ParseIntoArrayLines splits on them.
		 *
		 * A copy of the 1.x CountSourceLineTerminators, which was file-static in
		 * DreamShaderMaterialGeneratorSourceLoading.cpp. It is eight lines and it guards a cross-module invariant that
		 * nothing else notices when it breaks (see the ensureMsgf below), so duplicating it beat not checking.
		 */
		int32 CountSourceLineTerminators(const FString& InText)
		{
			int32 Count = 0;
			for (int32 Index = 0; Index < InText.Len(); ++Index)
			{
				const TCHAR Character = InText[Index];
				if (Character == TCHAR('\n'))
				{
					++Count;
				}
				else if (Character == TCHAR('\r'))
				{
					++Count;
					if (Index + 1 < InText.Len() && InText[Index + 1] == TCHAR('\n'))
					{
						++Index;
					}
				}
			}

			return Count;
		}

		/**
		 * True when the user pressed Cancel on the slow-task dialog.
		 *
		 * Reads the automation override first, so a test can exercise the cancel path -- FSlowTask::ShouldCancel is
		 * gated on GIsSlowTask, which only a real dialog sets. The override's storage lives in this module
		 * (DreamShaderGenerationProgress.cpp), so a test in the editor module arms the copy this reads.
		 */
		bool IsPipelineCancelled(const FScopedSlowTask& SlowTask)
		{
			if (const Private::FDreamShaderGenerationCancelPredicate& Override = Private::GetDreamShaderGenerationCancelOverride())
			{
				return Override();
			}

			return SlowTask.ShouldCancel();
		}

		IR::EIRBackend ResolveDefaultBackend()
		{
			if (const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>())
			{
				switch (Settings->DefaultBackend)
				{
				case EDreamShaderDefaultBackend::Instance:   return IR::EIRBackend::ThinCustom;
				case EDreamShaderDefaultBackend::ThinCustom: return IR::EIRBackend::ThinCustom;
				default:                                     return IR::EIRBackend::Graph;
				}
			}

			return IR::EIRBackend::Graph;
		}

		/**
		 * The order products must be emitted in: a product another product calls comes first.
		 *
		 * The edge is Prop::LocalFunction -- the property the IR builder writes on a FunctionCall node that targets an
		 * exported function of the SAME file, holding that function's product index. The emitter binds a
		 * MaterialFunctionCall against the live UMaterialFunction asset, reading its pins off the object, so a caller
		 * emitted first binds against last build's interface -- the same failure
		 * FDreamShaderDependencyGraphService::SortByDependencyOrder exists to prevent between files, one level down.
		 *
		 * Returns false on a cycle, naming the products involved.
		 */
		bool ComputeProductEmitOrder(const IR::FIRModule& Module, TArray<int32>& OutOrder, TArray<int32>& OutCycle)
		{
			const int32 ProductCount = Module.Products.Num();

			TArray<TSet<int32>> Dependencies;
			Dependencies.SetNum(ProductCount);
			for (int32 ProductIndex = 0; ProductIndex < ProductCount; ++ProductIndex)
			{
				for (const IR::FIRNode& Node : Module.Products[ProductIndex].Graph.Nodes)
				{
					if (Node.Op != IR::EIROp::FunctionCall)
					{
						continue;
					}

					const IR::FIRProperty* Local = Node.FindProperty(IR::Prop::LocalFunction);
					if (!Local || Local->Value.Kind != IR::EIRPropertyKind::Int)
					{
						continue;
					}

					const int32 Target = static_cast<int32>(Local->Value.I);
					if (Target >= 0 && Target < ProductCount && Target != ProductIndex)
					{
						Dependencies[ProductIndex].Add(Target);
					}
				}
			}

			// Kahn, taking products in declaration order among the ready ones, so the emit order is stable run to run
			// and a diff of two logs is readable.
			TArray<int32> RemainingCount;
			RemainingCount.SetNum(ProductCount);
			for (int32 ProductIndex = 0; ProductIndex < ProductCount; ++ProductIndex)
			{
				RemainingCount[ProductIndex] = Dependencies[ProductIndex].Num();
			}

			OutOrder.Reset();
			OutOrder.Reserve(ProductCount);

			TArray<bool> Emitted;
			Emitted.Init(false, ProductCount);

			bool bProgress = true;
			while (OutOrder.Num() < ProductCount && bProgress)
			{
				bProgress = false;
				for (int32 ProductIndex = 0; ProductIndex < ProductCount; ++ProductIndex)
				{
					if (Emitted[ProductIndex] || RemainingCount[ProductIndex] > 0)
					{
						continue;
					}

					Emitted[ProductIndex] = true;
					OutOrder.Add(ProductIndex);
					bProgress = true;

					for (int32 Dependent = 0; Dependent < ProductCount; ++Dependent)
					{
						if (!Emitted[Dependent] && Dependencies[Dependent].Contains(ProductIndex))
						{
							--RemainingCount[Dependent];
						}
					}
				}
			}

			if (OutOrder.Num() == ProductCount)
			{
				return true;
			}

			OutCycle.Reset();
			for (int32 ProductIndex = 0; ProductIndex < ProductCount; ++ProductIndex)
			{
				if (!Emitted[ProductIndex])
				{
					OutCycle.Add(ProductIndex);
				}
			}
			return false;
		}

		FText DescribeProducts(const IR::FIRModule& Module, const TArray<int32>& Indices)
		{
			TArray<FString> Names;
			for (const int32 Index : Indices)
			{
				if (Module.Products.IsValidIndex(Index))
				{
					Names.Add(Module.Products[Index].Name);
				}
			}
			return FText::FromString(FString::Join(Names, TEXT(", ")));
		}

		/** What the emit did with a product it did not fail. */
		enum class EDreamShaderLang2EmitOutcome : uint8
		{
			/** The graph was rebuilt and the asset saved. */
			Built,
			/** The stamped source hash matched and bForce was not set: DSH8237. */
			SkippedSourceCurrent,
			/** Another editor owns writing this project's generated assets: DSH8209. */
			DeferredToWriteOwner,
		};

		/**
		 * What the emit did with one product, read back from the Info the emitter records for a skip.
		 *
		 * EmitDreamShaderIRProduct answers true for a build and for both kinds of skip, and its frozen signature
		 * carries nothing else, so the Info IS the record: DSH8237 for a current source hash, DSH8209 for a deferral,
		 * each raised once at the product's own declaration span (DreamShaderIREmitter.cpp, CheckRebuildPreconditions).
		 * Code and span are matched together: codes are the stable half of a diagnostic, and no two products of one
		 * file share a declaration, so one product's skip is never read as another's. The codes are only compared
		 * here, never raised; .skill/gen-diagnostics.ps1 attributes a code to its raise site.
		 */
		EDreamShaderLang2EmitOutcome ClassifyLang2ProductEmitOutcome(const Lang::FLangDiagnosticSink& Diagnostics, const IR::FIRProduct& Product)
		{
			const Lang::FLangSpan& ProductSpan = Product.Source.Span;
			for (const Lang::FLangDiagnostic& Diagnostic : Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Severity != Lang::ELangSeverity::Info
					|| Diagnostic.Span.Offset != ProductSpan.Offset
					|| Diagnostic.Span.Length != ProductSpan.Length
					|| Diagnostic.Span.Line != ProductSpan.Line
					|| Diagnostic.Span.Column != ProductSpan.Column)
				{
					continue;
				}

				if (Diagnostic.Code.Equals(TEXT("DSH8237"), ESearchCase::CaseSensitive))
				{
					return EDreamShaderLang2EmitOutcome::SkippedSourceCurrent;
				}
				if (Diagnostic.Code.Equals(TEXT("DSH8209"), ESearchCase::CaseSensitive))
				{
					return EDreamShaderLang2EmitOutcome::DeferredToWriteOwner;
				}
			}

			return EDreamShaderLang2EmitOutcome::Built;
		}

		/** How far RunDreamShaderPipelineStages goes. */
		enum class EDreamShaderPipelineStop : uint8
		{
			/** Front end, bind and IR build: ResolveDreamShaderSourceProducts. Nothing after the builder runs. */
			AfterLower,
			/** Plus the passes and the validator: `check`, a run with bEmitAssets false. */
			AfterValidate,
			/** The whole run. */
			AfterEmit,
		};

		bool RunDreamShaderPipelineStages(
			const FString& InSourceFilePath,
			const FDreamShaderLang2PipelineOptions& Options,
			EDreamShaderPipelineStop Stop,
			bool bShowProgress,
			FDreamShaderLang2PipelineResult& OutResult);

		/** How many instances deep a Parent chain may go before the pipeline calls it a runaway (DSH8265). */
		constexpr int32 DreamShaderInstanceChainLimit = 16;

		/** The `.dsi` sources whose parent is being resolved right now, outermost first: the cycle and depth guard. */
		TArray<FString>& GetDreamShaderInstanceParentChain()
		{
			static TArray<FString> Chain;
			return Chain;
		}

		/**
		 * The `.dsi` half of a run, between the parse and the bind (research-instance sections 3.4 and 3.7): resolve the
		 * Parent, build the schema the binder checks the overrides against -- from the parent source's IR when a
		 * DreamShader source builds it (producer A), else from the loaded asset (producer B) -- and, in an emitting run,
		 * compile a missing or stale DreamShader parent first. Fills OutResult.ParentObjectPath and ParentSchema; every
		 * failure is a diagnostic.
		 */
		void ResolveDreamShaderInstanceParentForPipeline(
			const Lang::FModule& Module,
			const FString& SourceFilePath,
			const bool bCompileStaleParent,
			FDreamShaderLang2PipelineResult& OutResult)
		{
			using namespace ::UE::DreamShader::Lang;

			const FPragmaDecl* InstancePragma = nullptr;
			for (const FDeclPtr& Decl : Module.Declarations)
			{
				if (Decl && Decl->Kind == ENodeKind::PragmaDecl && static_cast<const FPragmaDecl&>(*Decl).PragmaKind == EPragmaKind::Instance)
				{
					InstancePragma = static_cast<const FPragmaDecl*>(Decl.Get());
					break;
				}
			}

			// No pragma, or no Parent: the binder reports it (DSH7250, DSH7252), and there is nothing to resolve.
			const FPragmaArgument* ParentArgument = InstancePragma ? InstancePragma->Find(TEXT("Parent")) : nullptr;
			if (!ParentArgument || ParentArgument->Value.TrimStartAndEnd().IsEmpty())
			{
				return;
			}

			TArray<FString>& Chain = GetDreamShaderInstanceParentChain();
			if (Chain.ContainsByPredicate([&SourceFilePath](const FString& Link) { return Link.Equals(SourceFilePath, ESearchCase::IgnoreCase); }))
			{
				OutResult.Diagnostics.Error(TEXT("DSH8263"), ParentArgument->Span, FText::Format(
					LOCTEXT("InstanceChainCycle", "'{0}' is its own ancestor: following Parent from it comes back to it ({1})."),
					FText::FromString(FPaths::GetCleanFilename(SourceFilePath)),
					FText::FromString(FString::Join(Chain, TEXT(" -> ")))));
				return;
			}
			if (Chain.Num() >= DreamShaderInstanceChainLimit)
			{
				OutResult.Diagnostics.Error(TEXT("DSH8265"), ParentArgument->Span, FText::Format(
					LOCTEXT("InstanceChainTooDeep", "The Parent chain above '{0}' is more than {1} instances deep; a chain that long is almost always a mistake in a Parent key."),
					FText::FromString(FPaths::GetCleanFilename(SourceFilePath)),
					FText::AsNumber(DreamShaderInstanceChainLimit)));
				return;
			}

			FString ParentObjectPath;
			FString ParentSourceFile;
			if (!ResolveInstanceParent(SourceFilePath, ParentArgument->Value, ParentArgument->Span, ParentObjectPath, ParentSourceFile, OutResult.Diagnostics))
			{
				return;
			}
			OutResult.ParentObjectPath = ParentObjectPath;

			TUniquePtr<IR::FIRParameterSchema> Schema = MakeUnique<IR::FIRParameterSchema>();
			if (!ParentSourceFile.IsEmpty())
			{
				// Producer A: the parent source through the front half, passes included -- they record the uniforms the
				// prune pass removed, which the schema lists so an override of one is told why it is unknown. Nothing is
				// emitted, and no dialog opens inside the compile that asked.
				FDreamShaderLang2PipelineOptions ParentOptions;
				ParentOptions.bEmitAssets = false;
				FDreamShaderLang2PipelineResult ParentRun;
				Chain.Add(SourceFilePath);
				const bool bParentLowered = RunDreamShaderPipelineStages(ParentSourceFile, ParentOptions, EDreamShaderPipelineStop::AfterValidate, /*bShowProgress*/ false, ParentRun);
				Chain.Pop(DREAMSHADER_ALLOW_SHRINKING_NO);

				int32 ParentProductIndex = INDEX_NONE;
				if (bParentLowered && ParentRun.IR.IsValid())
				{
					for (int32 ProductIndex = 0; ProductIndex < ParentRun.IR->Products.Num(); ++ProductIndex)
					{
						FString PackageName;
						FString ObjectPath;
						FString LeafName;
						FDreamShaderError DestinationError;
						if (ResolveIRProductObjectPath(ParentRun.IR->Products[ProductIndex], ParentRun.SourceFilePath, PackageName, ObjectPath, LeafName, DestinationError)
							&& ObjectPath.Equals(ParentObjectPath, ESearchCase::IgnoreCase))
						{
							ParentProductIndex = ProductIndex;
							break;
						}
					}
				}

				if (ParentProductIndex == INDEX_NONE)
				{
					OutResult.Diagnostics.Error(TEXT("DSH8260"), ParentArgument->Span, FText::Format(
						LOCTEXT("ParentSourceDoesNotBuild", "The parent '{0}' comes from '{1}', which does not compile, so the parameters this instance overrides cannot be checked; compile that source to see why."),
						FText::FromString(ParentObjectPath),
						FText::FromString(FPaths::GetCleanFilename(ParentSourceFile))));
					return;
				}

				IR::BuildParameterSchemaFromIR(*ParentRun.IR, ParentProductIndex, ParentRun.Bound.Get(), *Schema);
				Schema->ParentObjectPath = ParentObjectPath;

				// A missing or stale parent asset is compiled first, through the normal non-forced entry, so the
				// emitter's drift check (DSH8246) finds the parameters the source declares.
				if (bCompileStaleParent)
				{
					UObject* ParentAsset = FindObject<UObject>(nullptr, *ParentObjectPath);
					if (!ParentAsset && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ParentObjectPath)))
					{
						ParentAsset = LoadObject<UObject>(nullptr, *ParentObjectPath);
					}
					if (!ParentAsset || !Private::IsGeneratedAssetSourceCurrent(ParentAsset, ParentRun.SourceFilePath, ParentRun.SourceHash))
					{
						OutResult.Diagnostics.Info(TEXT("DSH8264"), ParentArgument->Span, FText::Format(
							LOCTEXT("ParentCompiledFirst", "'{0}' was missing or older than its source, so '{1}' was compiled first."),
							FText::FromString(ParentObjectPath),
							FText::FromString(FPaths::GetCleanFilename(ParentRun.SourceFilePath))));

						FDreamShaderError ParentError;
						Chain.Add(SourceFilePath);
						const bool bParentCompiled = CompileDreamShaderSourceFile(ParentRun.SourceFilePath, /*bForce*/ false, ::UE::DreamShader::EThinCustomPersistence::Materialized, ParentError);
						Chain.Pop(DREAMSHADER_ALLOW_SHRINKING_NO);
						if (!bParentCompiled)
						{
							OutResult.Diagnostics.Error(TEXT("DSH8260"), ParentArgument->Span, FText::Format(
								LOCTEXT("ParentCompileFailed", "The parent source '{0}' failed to compile, so this instance has no parent to build against. {1}"),
								FText::FromString(FPaths::GetCleanFilename(ParentRun.SourceFilePath)),
								FText::FromString(ParentError.Message)));
							return;
						}
					}
				}
			}
			else if (UMaterialInterface* ParentAsset = LoadObject<UMaterialInterface>(nullptr, *ParentObjectPath))
			{
				// Producer B: a parent no source under the roots builds, read off the loaded asset.
				BuildParameterSchemaFromAsset(ParentAsset, *Schema);
			}

			OutResult.ParentSchema = MoveTemp(Schema);
		}

		/**
		 * The run itself, shared by RunDreamShaderLang2Pipeline and ResolveDreamShaderSourceProducts so that both
		 * compute the build key -- and pick the front end, the dialect and the product destinations -- with exactly the
		 * same code (R0 handoff #8: "share it, do not copy it").
		 *
		 * bShowProgress opens the slow-task dialog and honours Cancel. Product resolution passes false: the Material
		 * Content Browser resolves every source it lists, and a dialog per row would flash, while the cancel override a
		 * test arms is meant for a compile, not for a status query.
		 */
		bool RunDreamShaderPipelineStages(
			const FString& InSourceFilePath,
			const FDreamShaderLang2PipelineOptions& Options,
			const EDreamShaderPipelineStop Stop,
			const bool bShowProgress,
			FDreamShaderLang2PipelineResult& OutResult)
		{
			using namespace ::UE::DreamShader::Lang;
			using namespace ::UE::DreamShader::IR;

			const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
			OutResult.SourceFilePath = SourceFilePath;

			// The sink is given the file it is about. ValidateDreamShaderIR stamps every diagnostic it raises with the
			// sink's own path (it has no other way to know one), so a default-constructed sink would produce validator
			// errors that belong to no file.
			OutResult.Diagnostics = FLangDiagnosticSink(SourceFilePath);

			// An empty span: the failures below are about the file, not about a position in it, and a fabricated span
			// would put a squiggle on a line that has nothing to do with the problem.
			const FLangSpan FileSpan;

			if (!IsDreamShaderLang2Source(SourceFilePath))
			{
				OutResult.Diagnostics.Error(TEXT("DSH8296"), FileSpan, FText::Format(
					LOCTEXT("NotACompilableSource", "'{0}' is not a source the compiler builds on its own; it builds '.dss', '.dsi', '.dsm' and '.dsf' files, and a '.dsh' header only through the source that includes it."),
					FText::FromString(SourceFilePath)));
				return false;
			}

			// Six frames: read, parse, bind, lower, validate, emit. The cancel button matters because the emit half can
			// take minutes once shader compilation starts behind it.
			TUniquePtr<FScopedSlowTask> SlowTask;
			if (bShowProgress)
			{
				SlowTask = MakeUnique<FScopedSlowTask>(
					6.0f,
					FText::Format(
						LOCTEXT("CompilingLang2Source", "Compiling DreamShader source '{0}'..."),
						FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
				if (!IsRunningCommandlet())
				{
					SlowTask->MakeDialogDelayed(0.35f, /*bShowCancelButton*/ true);
				}
			}

			// Answers true when the run must stop because the user cancelled; a run without a dialog never cancels.
			auto EnterFrameAndCheckCancel = [&SlowTask](const FText& FrameText) -> bool
			{
				if (!SlowTask.IsValid())
				{
					return false;
				}
				SlowTask->EnterProgressFrame(1.0f, FrameText);
				return IsPipelineCancelled(*SlowTask);
			};
			auto IsCancelledNow = [&SlowTask]() -> bool
			{
				return SlowTask.IsValid() && IsPipelineCancelled(*SlowTask);
			};
			auto ReportCancelled = [&OutResult, &FileSpan, &SourceFilePath]() -> bool
			{
				OutResult.bCancelled = true;
				OutResult.Diagnostics.Error(TEXT("DSH8298"), FileSpan, FText::Format(
					LOCTEXT("CompileCancelled", "Compiling '{0}' was cancelled; nothing was written."),
					FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
				return false;
			};

			// ------------------------------------------------------------------------------ read

			if (EnterFrameAndCheckCancel(FText::Format(
				LOCTEXT("Lang2Reading", "Reading '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath)))))
			{
				return ReportCancelled();
			}

			FString RawText;
			if (!FFileHelper::LoadFileToString(RawText, *SourceFilePath))
			{
				OutResult.Diagnostics.Error(TEXT("DSH8290"), FileSpan, FText::Format(
					LOCTEXT("SourceUnreadable", "'{0}' could not be read."),
					FText::FromString(SourceFilePath)));
				return false;
			}

			// Resolved ONCE for the whole compile and handed down by reference: one table for one compile is what makes
			// the touched set coherent and the build key provable. A provider delegate that answered differently between
			// the source and one of its headers would otherwise produce an asset whose halves were compiled against
			// different define sets, with nothing downstream able to tell.
			OutResult.Defines = MakeUnique<UE::DreamShader::FDreamShaderDefineTable>(UE::DreamShader::ResolveDreamShaderDefines());

			UE::DreamShader::FDreamShaderPreprocessResult PreprocessResult;
			UE::DreamShader::FDreamShaderTextError PreprocessError;
			if (!UE::DreamShader::PreprocessDreamShaderSource(
				RawText,
				SourceFilePath,
				*OutResult.Defines,
				PreprocessResult,
				PreprocessError,
				GetDreamShaderPreprocessDialectForFile(SourceFilePath)))
			{
				OutResult.Diagnostics.Error(TEXT("DSH8291"), FileSpan, FText::Format(
					LOCTEXT("SourcePreprocessFailed", "'{0}' failed conditional compilation: {1}: {2}"),
					FText::FromString(SourceFilePath),
					FText::FromString(PreprocessError.Code),
					PreprocessError.Message));
				return false;
			}

			// The same cross-module check the 1.x loader made, for the same reason: break line-count conservation and
			// nothing FAILS -- every diagnostic below the first directive simply points at the wrong line, in every
			// conditional source, and the bug is misattributed to the diagnostics mapper for a long time. ensureMsgf
			// rather than checkf because the damage is misreported positions, not a bad asset, and this runs inside an
			// artist's open editor.
			{
				const int32 Before = CountSourceLineTerminators(RawText);
				const int32 After = CountSourceLineTerminators(PreprocessResult.Text);
				ensureMsgf(
					Before == After,
					TEXT("DreamShader preprocessor changed the line count of '%s' (%d -> %d). Directive and elided ")
					TEXT("lines must be emitted as empty lines, never removed."),
					*SourceFilePath,
					Before + 1,
					After + 1);
			}

			OutResult.TouchedDefines = PreprocessResult.TouchedDefines;
			OutResult.bSourceHadPreprocessorDirectives = PreprocessResult.bHadDirectives;

			// --------------------------------------------------------------------------- parse

			if (EnterFrameAndCheckCancel(FText::Format(
				LOCTEXT("Lang2Parsing", "Parsing '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath)))))
			{
				return ReportCancelled();
			}

			OutResult.Source = MakeUnique<FLangSourceText>(SourceFilePath, PreprocessResult.Text);

			// Auto: the front end follows the file kind -- legacy for `.dsm` / `.dsf`, 2.0 for `.dss` / `.dsi` -- the same
			// decision GetDreamShaderPreprocessDialectForFile made above for the preprocessor (A4).
			FLangParseOptions ParseOptions;
			ParseOptions.Frontend = ELangFrontend::Auto;

			FLangParseResult ParseResult = ParseDreamShaderLang(*OutResult.Source, ParseOptions);
			const bool bParsed = ParseResult.Succeeded();
			OutResult.Module = MoveTemp(ParseResult.Module);
			OutResult.Diagnostics.Append(MoveTemp(ParseResult.Diagnostics));
			if (!bParsed)
			{
				// The tree is kept even here -- a language service wants navigation on a broken file most of all, and
				// `index` reads OutResult.Module without caring that the compile failed.
				return false;
			}

			// 1.x refused a texture default whose shell names the wrong class while it parsed, read or not (DSH1043,
			// DSH1044). ParseResult.Legacy points into the module, which OutResult now owns.
			if (ParseResult.Legacy.IsValid() && OutResult.Module.IsValid()
				&& !ValidateDreamShaderLegacyTextureDefaults(*OutResult.Module, *ParseResult.Legacy, OutResult.Diagnostics))
			{
				return false;
			}

			// `TextureObjectParameter T = <asset>` has the dimension of its asset, which only a host that can load one knows.
			if (ParseResult.Legacy.IsValid() && OutResult.Module.IsValid())
			{
				ResolveDreamShaderLegacyTextureTypes(*OutResult.Module, *ParseResult.Legacy);
			}

			// ------------------------------------------------------------------ instance parent (.dsi)

			const bool bIsInstanceSource = GetLangFileKindFromPath(SourceFilePath) == ELangFileKind::Dsi;
			if (bIsInstanceSource)
			{
				ResolveDreamShaderInstanceParentForPipeline(*OutResult.Module, SourceFilePath, Stop == EDreamShaderPipelineStop::AfterEmit, OutResult);
				if (OutResult.Diagnostics.HasErrors())
				{
					return false;
				}
			}

			// ---------------------------------------------------------------------------- bind

			if (EnterFrameAndCheckCancel(FText::Format(
				LOCTEXT("Lang2Binding", "Resolving names in '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath)))))
			{
				return ReportCancelled();
			}

			const FBuiltinCatalog& Catalog = GetDreamShaderBuiltinCatalog();
			if (Catalog.IsEmpty())
			{
				OutResult.Diagnostics.Error(TEXT("DSH8297"), FileSpan, LOCTEXT("CatalogEmpty",
					"The builtin expression catalog came back empty, so nothing that names a 'UE.*' node can be bound. Reflection found no UMaterialExpression classes, which normally means the Engine module is not loaded."));
				return false;
			}

			OutResult.Includes = MakeUnique<FDreamShaderIncludeResolver>(*OutResult.Defines);

			FBindOptions BindOptions;
			BindOptions.Catalog = &Catalog;
			BindOptions.IncludeResolver = OutResult.Includes->MakeBinderResolver();
			BindOptions.DefaultBackend = ResolveDefaultBackend();
			// `.dsi` only: what the binder checks the overrides against (null: shape checks only, DSH7263).
			BindOptions.ParentSchema = OutResult.ParentSchema.Get();
			BindOptions.ParentObjectPath = OutResult.ParentObjectPath;

			FLangBindResult BindResult = BindDreamShaderLang(*OutResult.Module, BindOptions);
			const bool bBound = BindResult.Succeeded();
			OutResult.Bound = MoveTemp(BindResult.Bound);
			OutResult.Diagnostics.Append(MoveTemp(BindResult.Diagnostics));

			// Recorded whether or not the bind succeeded: a compile that failed still read those headers, and the watcher
			// needs to know which files to rebuild this one from when they change.
			OutResult.IncludePaths = OutResult.Includes->GetResolvedIncludePaths();
			for (const TPair<FString, FString>& Pair : OutResult.Includes->GetTouchedDefines())
			{
				if (!OutResult.TouchedDefines.Contains(Pair.Key))
				{
					OutResult.TouchedDefines.Add(Pair.Key, Pair.Value);
				}
			}
			OutResult.bSourceHadPreprocessorDirectives |= OutResult.Includes->AnyIncludeHadDirectives();

			// The build key covers the headers' CONTENT, not merely their paths: editing a `.dsh` must invalidate every
			// asset built from a source that includes it, and the source's own text does not change when the header
			// does. Headers first, then the file, matching the order the 1.x inliner emitted them in.
			{
				FString DigestText = OutResult.Includes->GetIncludedSourceDigestText();
				DigestText += MakeDigestBlock(SourceFilePath, PreprocessResult.Text);
				if (bIsInstanceSource)
				{
					// The resolved parent is part of an instance's build key -- a bare-name Parent can re-resolve with no text
					// change -- and the parent's schema deliberately is not (research-instance section 3.7).
					DigestText += FString::Printf(TEXT("Parent=%s\n"), *OutResult.ParentObjectPath); /* I18N-EXEMPT: build-key material, never displayed */
				}
				OutResult.SourceHash = Private::BuildSourceHash(DigestText, OutResult.TouchedDefines);
			}

			if (!bBound)
			{
				return false;
			}

			// --------------------------------------------------------------------------- lower

			if (EnterFrameAndCheckCancel(FText::Format(
				LOCTEXT("Lang2Lowering", "Lowering '{0}' to IR..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath)))))
			{
				return ReportCancelled();
			}

			FIRBuildOptions BuildOptions;
			// The SAME catalog the bind ran against. FBoundModule keys reflected calls and material attributes by
			// catalog INDEX and does not carry the table, so the builder has to be handed it or every `UE.*` call
			// lowers to DSH4352.
			BuildOptions.Catalog = &Catalog;
			// Debt B5: every FIRSourceRef::File and every `// Begin/End DreamShader source:` marker in Custom node code
			// names the file project-relative, so neither the node code (and with it the shader keys) nor
			// DreamShader.SourceSpans changes with the machine or the checkout. Diagnostics keep the absolute path.
			BuildOptions.StampSourcePath = [](const FString& File)
			{
				return Private::MakeProjectRelativeSourcePath(File);
			};

			OutResult.IR = BuildDreamShaderIR(*OutResult.Bound, BuildOptions, OutResult.Diagnostics);
			if (bIsInstanceSource && OutResult.IR.IsValid())
			{
				for (FIRProduct& Product : OutResult.IR->Products)
				{
					if (Product.Kind == EIRProductKind::MaterialInstance && Product.Instance.ParentObjectPath.IsEmpty())
					{
						// FBoundInstance carries no resolved path of its own (SE report, contract change 1); the host fills it.
						Product.Instance.ParentObjectPath = OutResult.ParentObjectPath;
					}
				}
			}
			if (!OutResult.IR.IsValid() || OutResult.Diagnostics.HasErrors())
			{
				return false;
			}

			if (Stop == EDreamShaderPipelineStop::AfterLower)
			{
				OutResult.bSucceeded = true;
				return true;
			}

			FIRPassOptions PassOptions;
			RunDreamShaderIRPasses(*OutResult.IR, PassOptions, OutResult.Diagnostics);
			if (OutResult.Diagnostics.HasErrors())
			{
				return false;
			}

			// ------------------------------------------------------------------------ validate

			if (EnterFrameAndCheckCancel(FText::Format(
				LOCTEXT("Lang2Validating", "Validating the IR of '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath)))))
			{
				return ReportCancelled();
			}

			if (!ValidateDreamShaderIR(*OutResult.IR, Catalog, OutResult.Diagnostics))
			{
				return false;
			}

			if (Stop == EDreamShaderPipelineStop::AfterValidate)
			{
				// `check`: everything the front end can say has been said, and nothing was written.
				OutResult.bSucceeded = true;
				return true;
			}

			// ---------------------------------------------------------------------------- emit

			if (EnterFrameAndCheckCancel(FText::Format(
				LOCTEXT("Lang2Emitting", "Building the graph for '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath)))))
			{
				return ReportCancelled();
			}

			TArray<int32> EmitOrder;
			TArray<int32> Cycle;
			if (!ComputeProductEmitOrder(*OutResult.IR, EmitOrder, Cycle))
			{
				OutResult.Diagnostics.Error(TEXT("DSH8299"), FileSpan, FText::Format(
					LOCTEXT("ProductCycle", "The exported functions {0} call one another in a cycle, so there is no order in which they can be built; an exported function may call another only in one direction."),
					DescribeProducts(*OutResult.IR, Cycle)));
				return false;
			}

			FIREmitContext EmitContext;
			EmitContext.Catalog = &Catalog;
			EmitContext.SourceFilePath = SourceFilePath;
			EmitContext.SourceHash = OutResult.SourceHash;
			EmitContext.bForce = Options.bForce;
			EmitContext.ThinCustomPersistence = Options.ThinCustomPersistence;
			// The emit asks too: a cancel that lands while a graph is torn down is rolled back there (DSH8298).
			EmitContext.IsCancelled = IsCancelledNow;

			for (const int32 ProductIndex : EmitOrder)
			{
				if (IsCancelledNow())
				{
					return ReportCancelled();
				}

				UObject* Asset = nullptr;
				if (!EmitDreamShaderIRProduct(*OutResult.IR, ProductIndex, EmitContext, Asset, OutResult.Diagnostics))
				{
					OutResult.bCancelled = OutResult.bCancelled || IsCancelledNow();
					return false;
				}

				const FString AssetPath = Asset ? Asset->GetPathName() : FString();

				// Filled AFTER each product and read by the next: a FunctionCall to a same-file export carries only the
				// product index, and the emitter turns that into a real asset reference by looking the index up here.
				// This is why the order above is not cosmetic.
				EmitContext.EmittedProductAssetPaths.Add(ProductIndex, AssetPath);

				OutResult.ProductAssets.Add(Asset);
				OutResult.ProductAssetPaths.Add(AssetPath);
				OutResult.ProductOrder.Add(ProductIndex);
			}

			OutResult.bSucceeded = true;
			return true;
		}
	}

	bool RunDreamShaderLang2Pipeline(
		const FString& InSourceFilePath,
		const FDreamShaderLang2PipelineOptions& Options,
		FDreamShaderLang2PipelineResult& OutResult)
	{
		return RunDreamShaderPipelineStages(
			InSourceFilePath,
			Options,
			Options.bEmitAssets ? EDreamShaderPipelineStop::AfterEmit : EDreamShaderPipelineStop::AfterValidate,
			/*bShowProgress*/ true,
			OutResult);
	}

	bool ResolveDreamShaderProductDestination(
		const UE::DreamShader::IR::FIRProduct& Product,
		const FString& SourceFilePath,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutError)
	{
		FString AssetLeafName;
		FDreamShaderError DestinationError;
		if (ResolveIRProductObjectPath(Product, SourceFilePath, OutPackageName, OutObjectPath, AssetLeafName, DestinationError))
		{
			return true;
		}
		OutError = DestinationError.HasCode()
			? FString::Printf(TEXT("%s: %s"), *DestinationError.Code, *DestinationError.Message) /* I18N-EXEMPT: quotes an asset-layer message verbatim */
			: DestinationError.Message;
		return false;
	}

	bool ResolveDreamShaderSourceProducts(const FString& InSourceFilePath, FDreamShaderProductResolution& OutResult)
	{
		FDreamShaderLang2PipelineOptions Options;
		Options.bEmitAssets = false;

		// The run owns the parsed and bound modules and the IR; the resolution only copies facts out of it, so it
		// lives in this scope and dies here, in its load-bearing member order.
		FDreamShaderLang2PipelineResult Run;
		const bool bLowered = RunDreamShaderPipelineStages(InSourceFilePath, Options, EDreamShaderPipelineStop::AfterLower, /*bShowProgress*/ false, Run);

		OutResult.SourceFilePath = Run.SourceFilePath;
		OutResult.SourceHash = Run.SourceHash;
		OutResult.IncludePaths = Run.IncludePaths;
		OutResult.TouchedDefines = Run.TouchedDefines;
		OutResult.bSourceHadPreprocessorDirectives = Run.bSourceHadPreprocessorDirectives;
		OutResult.Products.Reset();

		bool bResolved = bLowered;
		if (bLowered && Run.IR.IsValid())
		{
			OutResult.Products.Reserve(Run.IR->Products.Num());
			for (int32 ProductIndex = 0; ProductIndex < Run.IR->Products.Num(); ++ProductIndex)
			{
				const IR::FIRProduct& Product = Run.IR->Products[ProductIndex];

				FDreamShaderResolvedProduct& Resolved = OutResult.Products.AddDefaulted_GetRef();
				Resolved.ProductIndex = ProductIndex;
				Resolved.Kind = Product.Kind;
				Resolved.Backend = Product.Backend;

				// The emitter's own destination rules (BuildDefinitionForIRProduct: the source-root default, `/// @name`,
				// the legacy Name=/Root= branch), so the path answered here is the path a compile writes.
				FString AssetLeafName;
				FDreamShaderError DestinationError;
				if (!ResolveIRProductObjectPath(Product, Run.SourceFilePath, Resolved.PackageName, Resolved.ObjectPath, AssetLeafName, DestinationError))
				{
					Run.Diagnostics.Error(TEXT("DSH8200"), Product.Source.Span, FText::Format(
						LOCTEXT("ResolveDestinationFailed", "'{0}' does not resolve to a valid asset path. {1}"),
						FText::FromString(Product.Name),
						FText::FromString(DestinationError.HasCode()
							? FString::Printf(TEXT("%s: %s"), *DestinationError.Code, *DestinationError.Message) /* I18N-EXEMPT: quotes an asset-layer message verbatim */
							: DestinationError.Message)));
					bResolved = false;
				}
			}
		}

		OutResult.Diagnostics = MoveTemp(Run.Diagnostics);
		return bResolved;
	}

	bool WordDreamShaderPipelineReport(
		const FDreamShaderLang2PipelineResult& Result,
		const bool bPipelineSucceeded,
		const TArray<FString>& ExtraWarnings,
		UE::DreamShader::FDreamShaderError& OutError)
	{
		using namespace ::UE::DreamShader::Lang;

		OutError.Reset();

		if (!bPipelineSucceeded)
		{
			if (!BuildLang2CompileError(Result.Diagnostics, Result.SourceFilePath, OutError))
			{
				// A false return with no error in the sink is an internal fault, not a source problem: some stage
				// refused without saying why. Naming it as such beats reporting an empty message, which reads as a
				// success to every caller that only prints OutError. DSH9039 rather than an 829x: the 1.x convention
				// puts an invariant failure in DSH9xxx, and this one says nothing about the source it was given.
				//
				// Through FailWith, not by assigning OutError.Code: .skill/gen-diagnostics.ps1 finds raise sites by the
				// literal-code shape, and a plain assignment is invisible to it.
				FailWith(OutError, TEXT("DSH9039"), FString::Printf( /* I18N-EXEMPT: internal invariant report */
					TEXT("%s: DSH9039: the DreamShader 2.0 pipeline failed without raising a diagnostic."),
					*Result.SourceFilePath));
			}
			return false;
		}

		// The success message is the 1.x shape on purpose, one line per product: `.skill/dsc.ps1` greps
		// `Generated <Kind> <ObjectPath> from <Source>.` to list the assets a run WROTE, and the bridge logs the message
		// verbatim. So only a product that was actually built says Generated; one the emitter skipped says what the 1.x
		// generator said for the same skip, word for word.
		TArray<FString> Lines;
		for (int32 Slot = 0; Slot < Result.ProductOrder.Num(); ++Slot)
		{
			const int32 ProductIndex = Result.ProductOrder[Slot];
			const IR::FIRProduct* Product = Result.IR.IsValid() && Result.IR->Products.IsValidIndex(ProductIndex)
				? &Result.IR->Products[ProductIndex]
				: nullptr;
			const FString& AssetPath = Result.ProductAssetPaths[Slot];

			const EDreamShaderLang2EmitOutcome Outcome = Product
				? ClassifyLang2ProductEmitOutcome(Result.Diagnostics, *Product)
				: EDreamShaderLang2EmitOutcome::Built;
			switch (Outcome)
			{
			case EDreamShaderLang2EmitOutcome::SkippedSourceCurrent:
				// The 1.x hash skip (the Graph and ThinCustom paths of the 1.x generator).
				Lines.Add(FString::Printf( /* I18N-EXEMPT: wire form, mirrors the 1.x hash-skip line */
					TEXT("Skipped %s from %s; source hash is unchanged (build key %s)."),
					*AssetPath,
					*Result.SourceFilePath,
					*Result.SourceHash));
				break;

			case EDreamShaderLang2EmitOutcome::DeferredToWriteOwner:
				// The 1.x deferral, as ShouldDeferPersistedAssetToWriteOwner writes it (DreamShaderAssetFactory.cpp) and
				// Docs/tools/bridge.md quotes it.
				Lines.Add(FString::Printf( /* I18N-EXEMPT: wire form, mirrors the 1.x write-owner line */
					TEXT("Skipped %s; another editor owns this project's DreamShader bridge, and only that one writes generated assets to disk."),
					*AssetPath));
				break;

			case EDreamShaderLang2EmitOutcome::Built:
				Lines.Add(FString::Printf( /* I18N-EXEMPT: wire form, parsed by dsc.ps1 */
					TEXT("Generated %s %s from %s."),
					Product ? LexToString(Product->Kind) : TEXT("Asset"),
					*AssetPath,
					*Result.SourceFilePath));
				break;
			}
		}

		if (Lines.IsEmpty())
		{
			Lines.Add(FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("Compiled %s; it declares no material and no exported function, so no asset was written."),
				*Result.SourceFilePath));
		}

		// Warnings ride under the same `Warnings:` header the 1.x generator used, so the bridge, the extension and
		// dsc.ps1 all keep parsing one shape: the run's own warning diagnostics, then what RaiseGenerationWarning
		// collected (DSH8155, DSH9011, DSH9012). They are NOT also logged here: every caller logs the message, and the
		// collector already logged its lines when they were raised.
		TArray<FString> Warnings;
		for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
		{
			if (Diagnostic.Severity == ELangSeverity::Warning)
			{
				Warnings.AddUnique(FormatLang2DiagnosticWireLine(Diagnostic, Result.SourceFilePath));
			}
		}
		for (const FString& Warning : ExtraWarnings)
		{
			Warnings.AddUnique(Warning);
		}

		OutError.Message = FString::Join(Lines, TEXT("\n"));
		if (!Warnings.IsEmpty())
		{
			OutError.Message += TEXT("\nWarnings:\n"); /* I18N-EXEMPT: deferred codegen or compatibility path */
			OutError.Message += FString::Join(Warnings, TEXT("\n"));
		}

		return true;
	}

	bool CompileDreamShaderLang2File(const FString& SourceFilePath, const bool bForce, UE::DreamShader::FDreamShaderError& OutError)
	{
		// The service's compile, notice and warnings included: every compile route ends in one routine.
		return CompileDreamShaderSourceFile(SourceFilePath, bForce, ::UE::DreamShader::EThinCustomPersistence::Materialized, OutError);
	}
}

#undef LOCTEXT_NAMESPACE
