// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The 2.0 compile pipeline. Implements the frozen DreamShaderCompilerPipeline.h, plus the richer
// entry point the tools need (RunDreamShaderLang2Pipeline, declared in DreamShaderCompilerTools.h).
//
// The shape of the run, and who owns each step:
//
//   read            this file        FFileHelper::LoadFileToString                     DSH8290
//   preprocess      DreamShaderLang  PreprocessDreamShaderSource (line count conserved) DSH8291
//   parse           M1               ParseDreamShaderLang
//   catalog         E                BuildBuiltinCatalogFromReflection (cached, DreamShaderCatalogManifest)
//   bind            S                BindDreamShaderLang  (+ this file's include resolver)
//   lower           I2               BuildDreamShaderIR
//   passes          I2               RunDreamShaderIRPasses
//   validate        I1               ValidateDreamShaderIR
//   emit            E                EmitDreamShaderIRProduct, once per product, in dependency order
//
// Two things this file deliberately does NOT do, both of which look like omissions until you look
// at what already does them:
//
//   * It does not broadcast OnDreamShaderSourceGenerated. FMaterialGenerator::GenerateAssetsFromFile
//     and ::GenerateMaterialFromFile wrap their dispatch into CompileDreamShaderLang2File in
//     FScopedGenerationNotice, whose destructor broadcasts once per outermost generation. Doing it
//     here as well would fire the event twice for every `.dss` compiled the normal way, and the
//     Browser model rebuilds its view on every broadcast.
//   * It does not check the source hash itself. FIREmitContext carries bForce and the emitter owns
//     IsGeneratedAssetSourceCurrent per product, because only the emitter knows which asset a
//     product resolves to. The hash IS computed here -- the emitter cannot, it never sees the text.
//     What the emitter decided comes back as an Info on the product's span (DSH8237 for a current
//     hash, DSH8209 for another editor owning the write), and CompileDreamShaderLang2File words that
//     product's result line from it in the 1.x words, so a skip never reads as `Generated`.

#include "DreamShaderCompilerPipeline.h"

#include "DreamShaderCatalogManifest.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderCompilerIncludes.h"
#include "DreamShaderCompilerTools.h"

// Unit E's entry points. FIREmitContext, EmitDreamShaderIRProduct.
#include "DreamShaderIREmitter.h"
// Unit I1's validator and dump; unit I2's builder and passes.
#include "IR/IRBuilder.h"
#include "IR/IRPasses.h"
#include "IR/IRValidator.h"

#include "DreamShaderDefineResolution.h"
#include "DreamShaderModule.h"
#include "DreamShaderPreprocessor.h"
#include "DreamShaderSettings.h"
#include "Lang/LangParser.h"
#include "MaterialAssetGeneration/DreamShaderGenerationProgress.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "Semantic/LangBound.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "DreamShader.Pipeline"

namespace UE::DreamShader::Editor::Compiler
{
	// Deliberate, and narrow in effect: the exact namespace each cross-unit entry point ends up in
	// (UE::DreamShader::Lang or UE::DreamShader::IR) is a choice the other units make, and the
	// contract's signatures are written in a shorthand that does not pin it. Every call below is
	// unqualified, so ordinary lookup (which walks out to UE::DreamShader) plus ADL on the IR and
	// Lang argument types plus these two directives cover every spelling any of them can pick. If a
	// name is still not found, the error names one line in this file, not thirty across the plugin.
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;

	bool IsDreamShaderLang2Source(const FString& SourceFilePath)
	{
		// `.dsh` answers false on purpose: a header is compiled only as part of the `.dss` that
		// includes it, and handing one to this pipeline directly would produce no product at all.
		// The frozen header says so; this is the one place it is decided.
		return GetLangFileKindFromPath(SourceFilePath) == ELangFileKind::Dss;
	}

	namespace
	{
		/** The 1.x `// Begin/End DreamShader source:` wrapper, for the build-key digest only. */
		FString MakeDigestBlock(const FString& FilePath, const FString& Text)
		{
			FString Block;
			Block += FString::Printf(TEXT("// Begin DreamShader source: %s\n"), *FilePath); /* I18N-EXEMPT: build-key material, never displayed */
			Block += Text;
			Block += FString::Printf(TEXT("\n// End DreamShader source: %s\n\n"), *FilePath); /* I18N-EXEMPT: build-key material, never displayed */
			return Block;
		}

		/**
		 * Counts line terminators the way FString::ParseIntoArrayLines splits on them.
		 *
		 * A copy of the 1.x CountSourceLineTerminators, which is file-static in
		 * DreamShaderMaterialGeneratorSourceLoading.cpp and therefore not reachable from here. It is
		 * eight lines and it guards a cross-module invariant that nothing else notices when it breaks
		 * (see the ensureMsgf below), so duplicating it beats not checking.
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
		 * Reads the same automation override the 1.x generator reads, so a test can exercise the
		 * cancel path here exactly as it does there -- FSlowTask::ShouldCancel is gated on GIsSlowTask,
		 * which only a real dialog sets.
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
		 * The edge is Prop::LocalFunction -- the property I2 writes on a FunctionCall node that
		 * targets an exported function of the SAME file, holding that function's product index
		 * (CONTRACT §2, "Helper vs export vs extern calls"). The emitter binds a MaterialFunctionCall
		 * against the live UMaterialFunction asset, reading its pins off the object, so a caller
		 * emitted first binds against last build's interface -- the same failure
		 * FDreamShaderDependencyGraphService::SortByDependencyOrder exists to prevent between files,
		 * one level down.
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

			// Kahn, taking products in declaration order among the ready ones, so the emit order is
			// stable run to run and a diff of two logs is readable.
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
		 * EmitDreamShaderIRProduct answers true for a build and for both kinds of skip, and its frozen
		 * signature carries nothing else, so the Info IS the record: DSH8237 for a current source hash,
		 * DSH8209 for a deferral, each raised once at the product's own declaration span
		 * (DreamShaderIREmitter.cpp, CheckRebuildPreconditions). Code and span are matched together:
		 * codes are the stable half of a diagnostic (CONTRACT 6.12), and no two products of one file
		 * share a declaration, so one product's skip is never read as another's. The codes are only
		 * compared here, never raised; .skill/gen-diagnostics.ps1 attributes a code to its raise site.
		 */
		EDreamShaderLang2EmitOutcome ClassifyLang2ProductEmitOutcome(const FLangDiagnosticSink& Diagnostics, const IR::FIRProduct& Product)
		{
			const FLangSpan& ProductSpan = Product.Source.Span;
			for (const FLangDiagnostic& Diagnostic : Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Severity != ELangSeverity::Info
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
	}

	bool RunDreamShaderLang2Pipeline(
		const FString& InSourceFilePath,
		const FDreamShaderLang2PipelineOptions& Options,
		FDreamShaderLang2PipelineResult& OutResult)
	{
		const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		OutResult.SourceFilePath = SourceFilePath;

		// The sink is given the file it is about. ValidateDreamShaderIR stamps every diagnostic it
		// raises with the sink's own path (it has no other way to know one), so a default-constructed
		// sink would produce validator errors that belong to no file.
		OutResult.Diagnostics = FLangDiagnosticSink(SourceFilePath);

		// An empty span: the failures below are about the file, not about a position in it, and a
		// fabricated span would put a squiggle on a line that has nothing to do with the problem.
		const FLangSpan FileSpan;

		if (!IsDreamShaderLang2Source(SourceFilePath))
		{
			OutResult.Diagnostics.Error(TEXT("DSH8296"), FileSpan, FText::Format(
				LOCTEXT("NotALang2Source", "'{0}' is not a 2.0 source; the 2.0 pipeline compiles '.dss' files, and a '.dsh' header is compiled only through the '.dss' that includes it."),
				FText::FromString(SourceFilePath)));
			return false;
		}

		// Six frames: read, parse, bind, lower, validate, emit. The same staged shape the 1.x
		// generator uses, and the cancel button matters for the same reason -- the emit half can
		// take minutes once shader compilation starts behind it.
		FScopedSlowTask SlowTask(
			6.0f,
			FText::Format(
				LOCTEXT("CompilingLang2Source", "Compiling DreamShader source '{0}'..."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (!IsRunningCommandlet())
		{
			SlowTask.MakeDialogDelayed(0.35f, /*bShowCancelButton*/ true);
		}

		auto Cancelled = [&OutResult, &FileSpan, &SourceFilePath]() -> bool
		{
			OutResult.bCancelled = true;
			OutResult.Diagnostics.Error(TEXT("DSH8298"), FileSpan, FText::Format(
				LOCTEXT("CompileCancelled", "Compiling '{0}' was cancelled; nothing was written."),
				FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
			return false;
		};

		// ------------------------------------------------------------------------------ read

		SlowTask.EnterProgressFrame(1.0f, FText::Format(
			LOCTEXT("Lang2Reading", "Reading '{0}'..."),
			FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (IsPipelineCancelled(SlowTask)) { return Cancelled(); }

		FString RawText;
		if (!FFileHelper::LoadFileToString(RawText, *SourceFilePath))
		{
			OutResult.Diagnostics.Error(TEXT("DSH8290"), FileSpan, FText::Format(
				LOCTEXT("SourceUnreadable", "'{0}' could not be read."),
				FText::FromString(SourceFilePath)));
			return false;
		}

		// Resolved ONCE for the whole compile and handed down by reference, exactly as
		// LoadPreparedDreamShaderSource does: one table for one compile is what makes the touched
		// set coherent and the build key provable. A provider delegate that answered differently
		// between the `.dss` and one of its headers would otherwise produce an asset whose halves
		// were compiled against different define sets, with nothing downstream able to tell.
		OutResult.Defines = MakeUnique<UE::DreamShader::FDreamShaderDefineTable>(UE::DreamShader::ResolveDreamShaderDefines());

		UE::DreamShader::FDreamShaderPreprocessResult PreprocessResult;
		UE::DreamShader::FDreamShaderTextError PreprocessError;
		if (!UE::DreamShader::PreprocessDreamShaderSource(RawText, SourceFilePath, *OutResult.Defines, PreprocessResult, PreprocessError, UE::DreamShader::EDreamShaderPreprocessDialect::Lang2))
		{
			OutResult.Diagnostics.Error(TEXT("DSH8291"), FileSpan, FText::Format(
				LOCTEXT("SourcePreprocessFailed", "'{0}' failed conditional compilation: {1}: {2}"),
				FText::FromString(SourceFilePath),
				FText::FromString(PreprocessError.Code),
				PreprocessError.Message));
			return false;
		}

		// The same cross-module check the 1.x loader makes, for the same reason: break line-count
		// conservation and nothing FAILS -- every diagnostic below the first directive simply points
		// at the wrong line, in every conditional source, and the bug is misattributed to the
		// diagnostics mapper for a long time. ensureMsgf rather than checkf because the damage is
		// misreported positions, not a bad asset, and this runs inside an artist's open editor.
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

		SlowTask.EnterProgressFrame(1.0f, FText::Format(
			LOCTEXT("Lang2Parsing", "Parsing '{0}'..."),
			FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (IsPipelineCancelled(SlowTask)) { return Cancelled(); }

		OutResult.Source = MakeUnique<FLangSourceText>(SourceFilePath, PreprocessResult.Text);
		FLangParseResult ParseResult = ParseDreamShaderLang(*OutResult.Source);
		const bool bParsed = ParseResult.Succeeded();
		OutResult.Module = MoveTemp(ParseResult.Module);
		OutResult.Diagnostics.Append(MoveTemp(ParseResult.Diagnostics));
		if (!bParsed)
		{
			// The tree is kept even here -- a language service wants navigation on a broken file most
			// of all, and `index` reads OutResult.Module without caring that the compile failed.
			return false;
		}

		// ---------------------------------------------------------------------------- bind

		SlowTask.EnterProgressFrame(1.0f, FText::Format(
			LOCTEXT("Lang2Binding", "Resolving names in '{0}'..."),
			FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (IsPipelineCancelled(SlowTask)) { return Cancelled(); }

		const IR::FBuiltinCatalog& Catalog = GetDreamShaderBuiltinCatalog();
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

		FLangBindResult BindResult = BindDreamShaderLang(*OutResult.Module, BindOptions);
		const bool bBound = BindResult.Succeeded();
		OutResult.Bound = MoveTemp(BindResult.Bound);
		OutResult.Diagnostics.Append(MoveTemp(BindResult.Diagnostics));

		// Recorded whether or not the bind succeeded: a compile that failed still read those headers,
		// and the watcher needs to know which files to rebuild this one from when they change.
		OutResult.IncludePaths = OutResult.Includes->GetResolvedIncludePaths();
		for (const TPair<FString, FString>& Pair : OutResult.Includes->GetTouchedDefines())
		{
			if (!OutResult.TouchedDefines.Contains(Pair.Key))
			{
				OutResult.TouchedDefines.Add(Pair.Key, Pair.Value);
			}
		}
		OutResult.bSourceHadPreprocessorDirectives |= OutResult.Includes->AnyIncludeHadDirectives();

		// The build key covers the headers' CONTENT, not merely their paths: editing a `.dsh` must
		// invalidate every asset built from a `.dss` that includes it, and the `.dss`'s own text does
		// not change when the header does. Headers first, then the file, matching the order the 1.x
		// inliner emits them in so the two hashes are comparable by eye during the migration.
		{
			FString DigestText = OutResult.Includes->GetIncludedSourceDigestText();
			DigestText += MakeDigestBlock(SourceFilePath, PreprocessResult.Text);
			OutResult.SourceHash = Private::BuildSourceHash(DigestText, OutResult.TouchedDefines);
		}

		if (!bBound)
		{
			return false;
		}

		// --------------------------------------------------------------------------- lower

		SlowTask.EnterProgressFrame(1.0f, FText::Format(
			LOCTEXT("Lang2Lowering", "Lowering '{0}' to IR..."),
			FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (IsPipelineCancelled(SlowTask)) { return Cancelled(); }

		FIRBuildOptions BuildOptions;
		// The SAME catalog the bind ran against. FBoundModule keys reflected calls and material
		// attributes by catalog INDEX and does not carry the table, so the builder has to be handed
		// it or every `UE.*` call lowers to DSH4352.
		BuildOptions.Catalog = &Catalog;
		OutResult.IR = BuildDreamShaderIR(*OutResult.Bound, BuildOptions, OutResult.Diagnostics);
		if (!OutResult.IR.IsValid() || OutResult.Diagnostics.HasErrors())
		{
			return false;
		}

		FIRPassOptions PassOptions;
		RunDreamShaderIRPasses(*OutResult.IR, PassOptions, OutResult.Diagnostics);
		if (OutResult.Diagnostics.HasErrors())
		{
			return false;
		}

		// ------------------------------------------------------------------------ validate

		SlowTask.EnterProgressFrame(1.0f, FText::Format(
			LOCTEXT("Lang2Validating", "Validating the IR of '{0}'..."),
			FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (IsPipelineCancelled(SlowTask)) { return Cancelled(); }

		if (!ValidateDreamShaderIR(*OutResult.IR, Catalog, OutResult.Diagnostics))
		{
			return false;
		}

		if (!Options.bEmitAssets)
		{
			// `check`: everything the front end can say has been said, and nothing was written.
			OutResult.bSucceeded = true;
			return true;
		}

		// ---------------------------------------------------------------------------- emit

		SlowTask.EnterProgressFrame(1.0f, FText::Format(
			LOCTEXT("Lang2Emitting", "Building the graph for '{0}'..."),
			FText::FromString(FPaths::GetCleanFilename(SourceFilePath))));
		if (IsPipelineCancelled(SlowTask)) { return Cancelled(); }

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

		for (const int32 ProductIndex : EmitOrder)
		{
			if (IsPipelineCancelled(SlowTask)) { return Cancelled(); }

			UObject* Asset = nullptr;
			if (!EmitDreamShaderIRProduct(*OutResult.IR, ProductIndex, EmitContext, Asset, OutResult.Diagnostics))
			{
				return false;
			}

			const FString AssetPath = Asset ? Asset->GetPathName() : FString();

			// Filled AFTER each product and read by the next: a FunctionCall to a same-file export
			// carries only the product index, and the emitter turns that into a real asset
			// reference by looking the index up here. This is why the order above is not cosmetic.
			EmitContext.EmittedProductAssetPaths.Add(ProductIndex, AssetPath);

			OutResult.ProductAssets.Add(Asset);
			OutResult.ProductAssetPaths.Add(AssetPath);
			OutResult.ProductOrder.Add(ProductIndex);
		}

		OutResult.bSucceeded = true;
		return true;
	}

	bool CompileDreamShaderLang2File(const FString& SourceFilePath, const bool bForce, UE::DreamShader::FDreamShaderError& OutError)
	{
		FDreamShaderLang2PipelineOptions Options;
		Options.bForce = bForce;
		Options.bEmitAssets = true;

		FDreamShaderLang2PipelineResult Result;
		const bool bSucceeded = RunDreamShaderLang2Pipeline(SourceFilePath, Options, Result);

		if (!bSucceeded)
		{
			if (!BuildLang2CompileError(Result.Diagnostics, Result.SourceFilePath, OutError))
			{
				// A false return with no error in the sink is an internal fault, not a source
				// problem: some stage refused without saying why. Naming it as such beats reporting
				// an empty message, which reads as a success to every caller that only prints
				// OutError. DSH9039 rather than an 829x: the 1.x convention puts an invariant
				// failure in DSH9xxx, and this one says nothing about the source it was given.
				//
				// Through FailWith, not by assigning OutError.Code: .skill/gen-diagnostics.ps1 finds
				// raise sites by the literal-code shape, and a plain assignment is invisible to it --
				// the code would exist in the binary and in no document.
				FailWith(OutError, TEXT("DSH9039"), FString::Printf( /* I18N-EXEMPT: internal invariant report */
					TEXT("%s: DSH9039: the DreamShader 2.0 pipeline failed without raising a diagnostic."),
					*Result.SourceFilePath));
			}
			return false;
		}

		// The success message is the 1.x shape on purpose, one line per product: `.skill/dsc.ps1` greps
		// `Generated <Kind> <ObjectPath> from <Source>.` to list the assets a run WROTE, and the bridge
		// logs the message verbatim. So only a product that was actually built says Generated; one the
		// emitter skipped says what the 1.x generator says for the same skip, word for word, which is
		// what makes a `.dss` read exactly like the `.dsm` it replaces.
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
				// The 1.x hash skip (DreamShaderMaterialGenerator.cpp, the Graph and ThinCustom paths).
				Lines.Add(FString::Printf( /* I18N-EXEMPT: wire form, mirrors the 1.x hash-skip line */
					TEXT("Skipped %s from %s; source hash is unchanged (build key %s)."),
					*AssetPath,
					*Result.SourceFilePath,
					*Result.SourceHash));
				break;

			case EDreamShaderLang2EmitOutcome::DeferredToWriteOwner:
				// The 1.x deferral, as ShouldDeferPersistedAssetToWriteOwner writes it
				// (DreamShaderAssetFactory.cpp) and Docs/tools/bridge.md quotes it.
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

		// Warnings ride under the same `Warnings:` header the 1.x generator uses, so the bridge, the
		// extension and dsc.ps1 all keep parsing one shape. They are NOT also logged here: every
		// caller of this function logs OutError.Message, and logging them twice is how the 1.x
		// AppendGenerationWarnings depth guard came to exist.
		TArray<FString> Warnings;
		for (const FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
		{
			if (Diagnostic.Severity == ELangSeverity::Warning)
			{
				Warnings.AddUnique(FormatLang2DiagnosticWireLine(Diagnostic, Result.SourceFilePath));
			}
		}

		OutError.Reset();
		OutError.Message = FString::Join(Lines, TEXT("\n"));
		if (!Warnings.IsEmpty())
		{
			OutError.Message += TEXT("\nWarnings:\n"); /* I18N-EXEMPT: deferred codegen or compatibility path */
			OutError.Message += FString::Join(Warnings, TEXT("\n"));
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
