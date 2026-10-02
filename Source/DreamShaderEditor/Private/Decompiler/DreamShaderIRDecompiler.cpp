// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Decompiler/DreamShaderIRDecompiler.h"

#include "Decompile/IRToAst.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Decompiler/DreamShaderGraphImport.h"
#include "Decompiler/DreamShaderInstanceDecompiler.h"
// DecompileDreamPassPipelineToText: a UDreamPassPipeline comes back as `.dsp` text.
#include "Decompiler/DreamShaderPipelineDecompiler.h"
#include "DreamPassPipeline.h"
#include "DreamShaderBuiltinCatalog.h"
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderTextWireUtils.h"
#include "IR/IR.h"
#include "IR/IRPasses.h"
#include "IR/IRValidator.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"

#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShader.Decompiler.IR"

namespace UE::DreamShader::Editor::Private
{
	namespace IRDecompile
	{
		/** One product of the module to be: the graph that is read, and the asset the product is addressed by. */
		struct FSubject
		{
			/** A UMaterial or a UMaterialFunction. For a ThinCustom pair, the hidden base material. */
			UObject* Graph = nullptr;
			/** What the source's product is: the same object, or the pair's UDreamShaderMaterialInstance. */
			UObject* Addressed = nullptr;
			bool bThinCustom = false;
		};

		static UE::DreamShader::IR::EIRBackend GetProjectDefaultBackend()
		{
			// The pipeline's own reading of the setting (ResolveDefaultBackend): Instance is the old spelling of ThinCustom.
			if (const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>())
			{
				return Settings->DefaultBackend == EDreamShaderDefaultBackend::Graph
					? UE::DreamShader::IR::EIRBackend::Graph
					: UE::DreamShader::IR::EIRBackend::ThinCustom;
			}
			return UE::DreamShader::IR::EIRBackend::Graph;
		}

		static bool ShouldExportLayout()
		{
			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
			return !Settings || Settings->bExportDecompiledLayout;
		}

		/** The asset as a subject; false when it is of no kind a `.dss` can hold. */
		static bool MakeSubject(UObject* Asset, FSubject& OutSubject)
		{
			OutSubject = FSubject();
			OutSubject.Addressed = Asset;

			if (Cast<UMaterial>(Asset) || Cast<UMaterialFunction>(Asset))
			{
				OutSubject.Graph = Asset;
				return true;
			}

			// The ThinCustom pair: the instance is what the project sees, the hidden base material is where the graph is
			// (Adopt reads it the same way).
			if (const UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset))
			{
				if (IsThinCustomInstancePair(Instance) && Cast<UMaterial>(Instance->Parent))
				{
					OutSubject.Graph = Instance->Parent;
					OutSubject.bThinCustom = true;
					return true;
				}
			}
			return false;
		}

		static UObject* FindOrLoadObject(const FString& ObjectPath)
		{
			// In memory first: an Ephemeral product has no package to load.
			if (UObject* Found = FindObject<UObject>(nullptr, *ObjectPath))
			{
				return Found;
			}
			return LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		static void AppendSink(FDreamShaderDecompileResult& Result, const UE::DreamShader::Lang::FLangDiagnosticSink& Sink)
		{
			for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				UE::DreamShader::Lang::FLangDiagnostic Copy = Diagnostic;
				if (Copy.FilePath.IsEmpty())
				{
					Copy.FilePath = Result.OutputFilePath;
				}
				Result.Diagnostics.Add(MoveTemp(Copy));
			}
		}

		static void Fail(FDreamShaderDecompileResult& Result, const TCHAR* Code, const FText& Message)
		{
			UE::DreamShader::Lang::FLangDiagnostic Diagnostic;
			Diagnostic.Code = Code;
			Diagnostic.Severity = UE::DreamShader::Lang::ELangSeverity::Error;
			Diagnostic.Message = Message;
			Diagnostic.FilePath = Result.OutputFilePath;
			if (Result.Error.IsEmpty())
			{
				Result.Error = ::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(Diagnostic, Result.OutputFilePath);
			}
			Result.Diagnostics.Add(MoveTemp(Diagnostic));
			Result.bSucceeded = false;
		}

		static void FailFromSink(FDreamShaderDecompileResult& Result, const UE::DreamShader::Lang::FLangDiagnosticSink& Sink)
		{
			if (const UE::DreamShader::Lang::FLangDiagnostic* First = Sink.FirstError())
			{
				if (Result.Error.IsEmpty())
				{
					Result.Error = ::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(*First, Result.OutputFilePath);
				}
			}
			Result.bSucceeded = false;
		}

		/** `// Warning: DSH9064: ...` lines for the file's head, one per warning, single-line. */
		static void CollectWarningComments(const UE::DreamShader::Lang::FLangDiagnosticSink& Sink, TArray<FString>& OutLines)
		{
			for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
			{
				if (Diagnostic.Severity != UE::DreamShader::Lang::ELangSeverity::Warning)
				{
					continue;
				}
				FString Message = ToInvariantWireString(Diagnostic.Message);
				Message.ReplaceInline(TEXT("\r"), TEXT(" "));
				Message.ReplaceInline(TEXT("\n"), TEXT(" "));
				OutLines.Add(FString::Printf(TEXT("Warning: %s: %s"), *Diagnostic.Code, *Message)); // I18N-EXEMPT: a comment in a source file
			}
		}
	}

	class FDreamShaderIRDecompiler final : public UE::DreamShader::Editor::IDreamShaderDecompiler
	{
	public:
		virtual bool DecompileMaterial(UMaterial* Material, const FString& DecompiledName, FString& OutSourceText, FString& OutError) override
		{
			// The one-asset entry of the interface. The 2.0 text names its product after the asset, so the 1.x
			// `Decompiled/...` name the service makes up for the 1.x decompiler has no use here.
			(void)DecompiledName;
			return DecompileOne(Material, OutSourceText, OutError);
		}

		virtual bool DecompileFunction(
			UMaterialFunction* MaterialFunction,
			const FString& DecompiledName,
			EDreamShaderDecompiledFunctionKind FunctionKind,
			FString& OutSourceText,
			FString& OutError) override
		{
			(void)DecompiledName;
			(void)FunctionKind;
			return DecompileOne(MaterialFunction, OutSourceText, OutError);
		}

		virtual bool DecompileRequest(const FDreamShaderDecompileRequest& Request, FDreamShaderDecompileResult& OutResult) override;

	private:
		bool DecompileOne(UObject* Asset, FString& OutSourceText, FString& OutError)
		{
			FDreamShaderDecompileRequest Request;
			Request.Asset = Asset;
			Request.Format = EDreamShaderDecompileFormat::Dss;
			FDreamShaderDecompileResult Result;
			DecompileRequest(Request, Result);
			OutSourceText = MoveTemp(Result.SourceText);
			OutError = MoveTemp(Result.Error);
			return Result.bSucceeded;
		}

		void DecompileInstance(const FDreamShaderDecompileRequest& Request, UMaterialInstanceConstant* Instance, FDreamShaderDecompileResult& OutResult);
		void DecompilePipeline(const FDreamShaderDecompileRequest& Request, UDreamPassPipeline* Pipeline, FDreamShaderDecompileResult& OutResult);
	};

	void FDreamShaderIRDecompiler::DecompileInstance(const FDreamShaderDecompileRequest& Request, UMaterialInstanceConstant* Instance, FDreamShaderDecompileResult& OutResult)
	{
		OutResult.OutputFilePath = Request.OutputFilePath.IsEmpty()
			? FDecompiledAssetNaming::MakeInstanceFilePath(Instance)
			: UE::DreamShader::NormalizeSourceFilePath(Request.OutputFilePath);

		if (!FPaths::GetExtension(OutResult.OutputFilePath, /*bIncludeDot*/ false).Equals(TEXT("dsi"), ESearchCase::IgnoreCase))
		{
			IRDecompile::Fail(OutResult, TEXT("DSH9085"), FText::Format(
				LOCTEXT("InstanceNeedsDsi", "'{0}' is a material instance, which decompiles to a '.dsi', and '{1}' is not one."),
				FText::FromString(Instance->GetPathName()),
				FText::FromString(OutResult.OutputFilePath)));
			return;
		}

		// `dsc decompile`: what differs from the parent, under the parent's bare name where the target's root knows it.
		FInstanceDecompileOptions Options;
		Options.Filter = EInstanceDecompileFilter::DifferingFromParent;
		Options.TargetSourceFilePath = OutResult.OutputFilePath;
		Options.bPreferBareParentName = true;

		UE::DreamShader::Lang::FLangDiagnosticSink Sink(OutResult.OutputFilePath);
		FString Text;
		const bool bDecompiled = DecompileMaterialInstanceToText(Instance, Options, Text, Sink);
		IRDecompile::AppendSink(OutResult, Sink);
		if (!bDecompiled || Sink.HasErrors())
		{
			IRDecompile::FailFromSink(OutResult, Sink);
			return;
		}

		OutResult.SourceText = MoveTemp(Text);
		OutResult.bSucceeded = true;
	}

	void FDreamShaderIRDecompiler::DecompilePipeline(const FDreamShaderDecompileRequest& Request, UDreamPassPipeline* Pipeline, FDreamShaderDecompileResult& OutResult)
	{
		OutResult.OutputFilePath = Request.OutputFilePath.IsEmpty()
			? FDecompiledAssetNaming::MakePipelineFilePath(Pipeline)
			: UE::DreamShader::NormalizeSourceFilePath(Request.OutputFilePath);

		if (!UE::DreamShader::IsDreamShaderPipelineFile(OutResult.OutputFilePath))
		{
			IRDecompile::Fail(OutResult, TEXT("DSH9210"), FText::Format(
				LOCTEXT("PipelineNeedsDsp", "'{0}' is a pass pipeline, which decompiles to a '.dsp', and '{1}' is not one."),
				FText::FromString(Pipeline->GetPathName()),
				FText::FromString(OutResult.OutputFilePath)));
			return;
		}

		// Bare material names where the target's root builds them, the way `dsc decompile` writes a `.dsi`'s Parent. The
		// text is checked against the asset before it is handed back (the re-parse check, DSH9221-9223).
		FPipelineDecompileOptions Options;
		Options.TargetSourceFilePath = OutResult.OutputFilePath;
		Options.bPreferBareMaterialNames = true;
		Options.bKeepAssetPath = Request.bKeepAssetPath;
		if (Request.SourceFilePath.IsEmpty())
		{
			Options.HeaderComments.Add(FString::Printf(TEXT("Decompiled by DreamShader from %s"), *Pipeline->GetPathName())); // I18N-EXEMPT: a comment in a source file
		}

		UE::DreamShader::Lang::FLangDiagnosticSink Sink(OutResult.OutputFilePath);
		FString Text;
		const bool bDecompiled = DecompileDreamPassPipelineToText(Pipeline, Options, Text, Sink);
		IRDecompile::AppendSink(OutResult, Sink);
		if (!bDecompiled || Sink.HasErrors())
		{
			IRDecompile::FailFromSink(OutResult, Sink);
			return;
		}

		OutResult.SourceText = MoveTemp(Text);
		OutResult.bSucceeded = true;
	}

	bool FDreamShaderIRDecompiler::DecompileRequest(const FDreamShaderDecompileRequest& Request, FDreamShaderDecompileResult& OutResult)
	{
		namespace IR = UE::DreamShader::IR;
		namespace Lang = UE::DreamShader::Lang;

		OutResult = FDreamShaderDecompileResult();
		OutResult.OutputFilePath = Request.OutputFilePath.IsEmpty() ? FString() : UE::DreamShader::NormalizeSourceFilePath(Request.OutputFilePath);

		// ----- what is decompiled
		TArray<IRDecompile::FSubject> Subjects;
		if (!Request.SourceFilePath.IsEmpty())
		{
			// Every product of the source, in the source's own order, into one module.
			::UE::DreamShader::Editor::Compiler::FDreamShaderProductResolution Resolution;
			const bool bResolved = ::UE::DreamShader::Editor::Compiler::ResolveDreamShaderSourceProducts(Request.SourceFilePath, Resolution);
			if (!bResolved || Resolution.Products.Num() == 0)
			{
				IRDecompile::AppendSink(OutResult, Resolution.Diagnostics);
				IRDecompile::Fail(OutResult, TEXT("DSH9087"), FText::Format(
					LOCTEXT("SourceProductsUnresolved", "'{0}' does not resolve to the assets it builds, so there is nothing to decompile for it."),
					FText::FromString(Request.SourceFilePath)));
				return true;
			}

			for (const ::UE::DreamShader::Editor::Compiler::FDreamShaderResolvedProduct& Product : Resolution.Products)
			{
				UObject* Asset = IRDecompile::FindOrLoadObject(Product.ObjectPath);
				if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset))
				{
					if (Product.Kind == IR::EIRProductKind::MaterialInstance && !IsThinCustomInstancePair(Instance))
					{
						// A `.dsi`: one product, its own text.
						DecompileInstance(Request, Instance, OutResult);
						return true;
					}
				}
				if (UDreamPassPipeline* Pipeline = Cast<UDreamPassPipeline>(Asset))
				{
					if (Product.Kind == IR::EIRProductKind::PassPipeline)
					{
						// A `.dsp`: one product, its own text.
						DecompilePipeline(Request, Pipeline, OutResult);
						return true;
					}
				}

				IRDecompile::FSubject Subject;
				if (!Asset || !IRDecompile::MakeSubject(Asset, Subject))
				{
					IRDecompile::Fail(OutResult, TEXT("DSH9087"), FText::Format(
						LOCTEXT("SourceProductMissingAny", "'{0}' builds '{1}', and that asset does not exist or is of no kind a decompile reads; build the source first."),
						FText::FromString(Request.SourceFilePath),
						FText::FromString(Product.ObjectPath)));
					return true;
				}
				Subjects.Add(Subject);
			}
		}
		else
		{
			if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Request.Asset))
			{
				if (!IsThinCustomInstancePair(Instance))
				{
					DecompileInstance(Request, Instance, OutResult);
					return true;
				}
			}
			if (UDreamPassPipeline* Pipeline = Cast<UDreamPassPipeline>(Request.Asset))
			{
				DecompilePipeline(Request, Pipeline, OutResult);
				return true;
			}

			IRDecompile::FSubject Subject;
			if (!IRDecompile::MakeSubject(Request.Asset, Subject))
			{
				IRDecompile::Fail(OutResult, TEXT("DSH9086"), FText::Format(
					LOCTEXT("UnsupportedAssetClassWithPipeline", "'{0}' is a {1}, which no DreamShader source describes; a decompile takes a material, a material function, layer or blend, a material instance, or a pass pipeline."),
					FText::FromString(Request.Asset ? Request.Asset->GetPathName() : FString(TEXT("<null>"))),
					FText::FromString(Request.Asset ? Request.Asset->GetClass()->GetName() : FString(TEXT("null object")))));
				return true;
			}
			Subjects.Add(Subject);
		}

		if (OutResult.OutputFilePath.IsEmpty())
		{
			// Named after the asset the project addresses: the pair, not the hidden base nobody ever sees.
			OutResult.OutputFilePath = FDecompiledAssetNaming::MakeDssFilePath(Subjects[0].Addressed);
		}

		// ----- graph -> IR
		Lang::FLangDiagnosticSink Sink(OutResult.OutputFilePath);
		FGraphImportOptions ImportOptions;
		ImportOptions.bImportLayout = IRDecompile::ShouldExportLayout();
		FGraphImportContext ImportContext;

		IR::FIRModule Module;
		Module.SourceFilePath = OutResult.OutputFilePath;
		TArray<FString> ProductAssetPaths;
		for (const IRDecompile::FSubject& Subject : Subjects)
		{
			if (!ImportDreamShaderGraphToIR(Subject.Graph, ImportOptions, Module, ImportContext, Sink))
			{
				IRDecompile::AppendSink(OutResult, Sink);
				IRDecompile::FailFromSink(OutResult, Sink);
				return true;
			}

			IR::FIRProduct& Product = Module.Products.Last();
			// The product is the asset the project addresses: the pair, not its hidden base.
			Product.Name = Subject.Addressed->GetName();
			Product.Backend = Subject.bThinCustom ? IR::EIRBackend::ThinCustom : IR::EIRBackend::Graph;
			ProductAssetPaths.Add(Subject.Addressed->GetPathName());

			// An Ephemeral product lives in the transient package and has no path of its own to keep.
			if (Request.bKeepAssetPath && Subject.Addressed->GetOutermost() != GetTransientPackage())
			{
				// `/// @name` only where the file would put the asset somewhere else.
				const FString PackageName = Subject.Addressed->GetOutermost()->GetName();
				IR::FIRProduct Probe;
				Probe.Kind = Product.Kind;
				Probe.Name = Product.Name;
				FString DerivedPackage;
				FString DerivedObjectPath;
				FString DestinationError;
				const bool bDerived = ::UE::DreamShader::Editor::Compiler::ResolveDreamShaderProductDestination(
					Probe, OutResult.OutputFilePath, DerivedPackage, DerivedObjectPath, DestinationError);
				if (!bDerived || !DerivedPackage.Equals(PackageName, ESearchCase::IgnoreCase))
				{
					Product.AssetPathOverride = PackageName;
				}
			}
		}

		// ----- IR -> IR: what a build's module looks like
		IR::FIRPassOptions PassOptions;
		// A foreign graph keeps its node set: folding `Multiply(2, 3)` or merging two equal nodes would change it (RD-2).
		PassOptions.bFoldConstants = false;
		PassOptions.bDedupe = false;
		PassOptions.bPrune = true;
		IR::RunDreamShaderIRPasses(Module, PassOptions, Sink);

		const IR::FBuiltinCatalog& Catalog = ::UE::DreamShader::Editor::Compiler::GetDreamShaderBuiltinCatalog();
		if (!IR::ValidateDreamShaderIR(Module, Catalog, Sink) || Sink.HasErrors())
		{
			IRDecompile::AppendSink(OutResult, Sink);
			IRDecompile::FailFromSink(OutResult, Sink);
			IRDecompile::Fail(OutResult, TEXT("DSH9088"), FText::Format(
				LOCTEXT("ImportedModuleInvalid", "The graph of '{0}' did not read into a valid module; the errors above say where. This is a defect of the decompiler, not of the asset."),
				FText::FromString(Subjects[0].Addressed->GetPathName())));
			return true;
		}

		// The Substrate sugar (`A * w`, `Transmittance = ...` on a slab) only when the readable form is asked for: it is
		// graph-exact, but it names no node, and a search of the text for the node the graph shows finds nothing.
		Lang::RaiseDreamShaderIR(Module, Sink, Request.bReadable ? &Catalog : nullptr);

		// ----- IR -> AST -> text
		Lang::FIRToAstOptions AstOptions;
		AstOptions.bEmitLayout = ImportOptions.bImportLayout;
		AstOptions.DefaultBackend = IRDecompile::GetProjectDefaultBackend();
		// Adopt pins the backend: a plugin root has to say Graph whatever the project defaults to.
		AstOptions.bEmitBackend = !Request.SourceFilePath.IsEmpty() || Request.bKeepAssetPath;
		AstOptions.ExternInterfaces = ImportContext.ExternInterfaces;
		AstOptions.ProductAssetPaths = ProductAssetPaths;
		AstOptions.bReadable = Request.bReadable;
		if (Request.SourceFilePath.IsEmpty())
		{
			AstOptions.HeaderComments.Add(FString::Printf(TEXT("Decompiled by DreamShader from %s"), *Subjects[0].Addressed->GetPathName())); // I18N-EXEMPT: a comment in a source file
		}

		// Twice when there is something to warn about: the warnings of the build itself belong at the head of the file it
		// builds, and they are only known once it has been built. The second run reports into a sink nobody reads.
		TUniquePtr<Lang::FModule> Ast = Lang::BuildDreamShaderAstFromIR(Module, Catalog, AstOptions, Sink);
		TArray<FString> WarningComments;
		IRDecompile::CollectWarningComments(Sink, WarningComments);
		if (Ast.IsValid() && WarningComments.Num() > 0)
		{
			AstOptions.HeaderComments.Append(WarningComments);
			Lang::FLangDiagnosticSink Unread(OutResult.OutputFilePath);
			Ast = Lang::BuildDreamShaderAstFromIR(Module, Catalog, AstOptions, Unread);
		}

		IRDecompile::AppendSink(OutResult, Sink);
		if (!Ast.IsValid() || Sink.HasErrors())
		{
			IRDecompile::FailFromSink(OutResult, Sink);
			return true;
		}

		OutResult.SourceText = Lang::PrintDreamShaderLang(*Ast);

		// The text has to be a file the front end reads. A parse error here is the decompiler's, and says so.
		{
			const Lang::FLangSourceText Printed(OutResult.OutputFilePath, OutResult.SourceText);
			Lang::FLangParseOptions ParseOptions;
			ParseOptions.Frontend = Lang::ELangFrontend::Dss;
			const Lang::FLangParseResult Parsed = Lang::ParseDreamShaderLang(Printed, ParseOptions);
			if (!Parsed.Succeeded())
			{
				const Lang::FLangDiagnostic* First = Parsed.Diagnostics.FirstError();
				Lang::FLangDiagnostic Warning;
				Warning.Code = TEXT("DSH9089");
				Warning.Severity = Lang::ELangSeverity::Warning;
				Warning.FilePath = OutResult.OutputFilePath;
				if (First)
				{
					Warning.Span = First->Span;
				}
				Warning.Message = FText::Format(
					LOCTEXT("PrintedTextDoesNotParse", "The decompiled text does not parse back: {0}: {1}. It is written as it is; this is a defect of the decompiler."),
					FText::FromString(First ? First->Code : FString(TEXT("DSH0000"))),
					First ? First->Message : LOCTEXT("PrintedTextNoReason", "the parser gave no reason"));
				OutResult.Diagnostics.Add(MoveTemp(Warning));
			}
		}

		OutResult.bSucceeded = true;
		return true;
	}

	UE::DreamShader::Editor::IDreamShaderDecompiler& GetIRDecompiler()
	{
		static FDreamShaderIRDecompiler Decompiler;
		return Decompiler;
	}
}

#undef LOCTEXT_NAMESPACE
