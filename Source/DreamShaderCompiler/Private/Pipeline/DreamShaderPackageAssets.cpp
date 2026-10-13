// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Pipeline/DreamShaderPackageAssets.h"

#include "DreamShaderDefineTable.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "DreamShaderPreprocessor.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DreamShader.PackageAssets"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		using namespace UE::DreamShader::Lang;

		/** What a `#pragma generate(...)` sets for the functions below it. */
		struct FGenerateTemplate
		{
			FString Functions;
			FString Library;
			FString Description;
			FLangSpan Span;
		};

		/** The value of a `///` directive written on one line: a `@desc` must not end the comment it is written into. */
		FString SingleLine(const FString& Text)
		{
			FString Result = Text.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
			Result.TrimStartAndEndInline();
			return Result;
		}

		/** Why the function cannot be a material function, or empty when it can. */
		FText FindUngeneratableReason(const FFunctionDecl& Function)
		{
			if (Function.Linkage == EFunctionLinkage::Export)
			{
				return LOCTEXT("ReasonExport", "an 'export' function already builds an asset of its own");
			}
			if (Function.Linkage == EFunctionLinkage::Extern)
			{
				return LOCTEXT("ReasonExtern", "an 'extern' prototype names an asset that exists, and has no body to generate from");
			}
			if (!Function.Body.IsValid() && !Function.bOpaqueBody)
			{
				return LOCTEXT("ReasonNoBody", "it has no body");
			}
			for (const FParam& Param : Function.Params)
			{
				if (Param.ArrayDimensions.Num() > 0)
				{
					return FText::Format(LOCTEXT("ReasonArray", "its parameter '{0}' is an array, and a material function has no array pins"), FText::FromString(Param.Name));
				}
			}
			return FText::GetEmpty();
		}
	}

	void CollectDreamShaderPackageGenerateRequests(
		const UE::DreamShader::Lang::FModule& Header,
		TArray<FDreamShaderPackageGenerateRequest>& OutRequests,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace UE::DreamShader::Lang;

		OutRequests.Reset();

		FGenerateTemplate Template;
		TMap<FString, FLangSpan> SeenAssets;

		for (const FDeclPtr& DeclPtr : Header.Declarations)
		{
			const FDecl* Decl = DeclPtr.Get();
			if (!Decl)
			{
				continue;
			}

			if (const FPragmaDecl* Pragma = Decl->As<FPragmaDecl>())
			{
				if (Pragma->PragmaKind != EPragmaKind::Generate)
				{
					continue;
				}
				// A later pragma replaces the earlier one whole; `#pragma generate()` ends the template.
				Template = FGenerateTemplate();
				Template.Span = Pragma->Span;
				for (const FPragmaArgument& Argument : Pragma->Arguments)
				{
					if (Argument.Key.Equals(TEXT("Functions"), ESearchCase::IgnoreCase))
					{
						Template.Functions = Argument.Value.TrimStartAndEnd();
					}
					else if (Argument.Key.Equals(TEXT("Library"), ESearchCase::IgnoreCase))
					{
						Template.Library = Argument.Value.TrimStartAndEnd();
					}
					else if (Argument.Key.Equals(TEXT("Desc"), ESearchCase::IgnoreCase) || Argument.Key.Equals(TEXT("Description"), ESearchCase::IgnoreCase))
					{
						Template.Description = Argument.Value.TrimStartAndEnd();
					}
					else
					{
						Diagnostics.Warning(
							TEXT("DSH8276"),
							Argument.Span.Length > 0 ? Argument.Span : Pragma->Span,
							FText::Format(
								LOCTEXT("PragmaGenerateKey", "'{0}' is not a key of '#pragma generate', and was ignored; it takes Functions, Library and Desc."),
								FText::FromString(Argument.Key)));
					}
				}
				continue;
			}

			const FFunctionDecl* Function = Decl->As<FFunctionDecl>();
			if (!Function)
			{
				continue;
			}

			FString AssetName;
			FString Library;
			FString Description;
			FLangSpan Span = Function->NameSpan;
			bool bFromTemplate = false;

			if (const FDocDirective* Own = Function->Doc.Find(Directive::Generate))
			{
				AssetName = Own->Value.TrimStartAndEnd();
				Span = Own->Span.Length > 0 ? Own->Span : Function->NameSpan;
				if (AssetName.Equals(TEXT("none"), ESearchCase::IgnoreCase))
				{
					continue;
				}
			}
			else if (!Template.Functions.IsEmpty() && Function->Linkage == EFunctionLinkage::Internal && (Function->Body.IsValid() || Function->bOpaqueBody))
			{
				// The template reaches the helpers only: an `export` or an `extern` below it is not asking for anything.
				AssetName = Template.Functions.Replace(TEXT("{Name}"), *Function->Name, ESearchCase::CaseSensitive);
				Library = Template.Library;
				Description = Template.Description;
				Span = Template.Span;
				bFromTemplate = true;
			}
			if (AssetName.IsEmpty())
			{
				continue;
			}

			if (AssetName.Contains(TEXT("{")) || AssetName.Contains(TEXT("}")))
			{
				Diagnostics.Error(
					TEXT("DSH8273"),
					Span,
					FText::Format(
						LOCTEXT("GeneratePlaceholder", "'{0}' holds a placeholder other than '`{Name`}', the only one a generate template has."),
						FText::FromString(AssetName)));
				continue;
			}

			const FText Reason = FindUngeneratableReason(*Function);
			if (!Reason.IsEmpty())
			{
				Diagnostics.Error(
					TEXT("DSH8274"),
					Function->NameSpan,
					FText::Format(
						LOCTEXT("GenerateNotPossible", "'{0}' cannot be generated as the material function '{1}': {2}."),
						FText::FromString(Function->Name),
						FText::FromString(AssetName),
						Reason));
				continue;
			}

			// The function's own directives win over the template's.
			if (const FDocDirective* OwnLibrary = Function->Doc.Find(Directive::Library))
			{
				Library = OwnLibrary->Value.TrimStartAndEnd();
			}
			if (const FDocDirective* OwnDescription = Function->Doc.Find(Directive::Desc))
			{
				Description = OwnDescription->Value.TrimStartAndEnd();
			}
			else if (!bFromTemplate && Description.IsEmpty() && Function->Doc.FreeText.Num() > 0)
			{
				// `@desc` falls back to the free text of the `///` block, as it does for every declaration.
				Description = FString::Join(Function->Doc.FreeText, TEXT(" "));
			}

			const FString Key = AssetName.ToLower();
			if (const FLangSpan* First = SeenAssets.Find(Key))
			{
				Diagnostics.Error(
					TEXT("DSH8275"),
					Span,
					FText::Format(
						LOCTEXT("GenerateTwice", "'{0}' is generated a second time here; the line {1} already generates it from another function."),
						FText::FromString(AssetName),
						FText::AsNumber(First->Line)));
				continue;
			}
			SeenAssets.Add(Key, Span);

			FDreamShaderPackageGenerateRequest& Request = OutRequests.AddDefaulted_GetRef();
			Request.Function = Function;
			Request.AssetName = AssetName;
			Request.Library = Library;
			Request.Description = Description;
			Request.Span = Span;
		}
	}

	FString BuildDreamShaderPackageWrapperSource(
		const FString& HeaderPath,
		const FString& AssetRoot,
		const TArray<FDreamShaderPackageGenerateRequest>& Requests)
	{
		using namespace UE::DreamShader::Lang;

		FString Out;
		Out += FString::Printf(TEXT("// Written by DreamShader for %s: one material function per function it asks to generate.\n"), *FPaths::GetCleanFilename(HeaderPath)); /* I18N-EXEMPT: generated source text */
		Out += FString::Printf(TEXT("#include \"%s\"\n"), *FPaths::GetCleanFilename(HeaderPath)); /* I18N-EXEMPT: generated source text */

		for (int32 Index = 0; Index < Requests.Num(); ++Index)
		{
			const FDreamShaderPackageGenerateRequest& Request = Requests[Index];
			const FFunctionDecl& Function = *Request.Function;

			Out += TEXT("\n");
			// A full path: the asset goes exactly where the package says, under its root, whatever folder the header is in.
			Out += FString::Printf(TEXT("/// @name %s/%s\n"), *AssetRoot, *Request.AssetName); /* I18N-EXEMPT: generated source text */
			if (!Request.Library.IsEmpty())
			{
				Out += FString::Printf(TEXT("/// @library %s\n"), *SingleLine(Request.Library)); /* I18N-EXEMPT: generated source text */
			}
			if (!Request.Description.IsEmpty())
			{
				Out += FString::Printf(TEXT("/// @desc %s\n"), *SingleLine(Request.Description)); /* I18N-EXEMPT: generated source text */
			}
			// What the function says about its own pins carries over: a static switch stays one, a pin keeps its tooltip.
			for (const FDocDirective& Directive : Function.Doc.Directives)
			{
				if (Directive.Key.Equals(Lang::Directive::Static, ESearchCase::CaseSensitive) || Directive.Key.Equals(Lang::Directive::Param, ESearchCase::CaseSensitive))
				{
					Out += FString::Printf(TEXT("/// @%s %s\n"), *Directive.Key, *SingleLine(Directive.Value)); /* I18N-EXEMPT: generated source text */
				}
			}

			TArray<FString> Parameters;
			TArray<FString> Arguments;
			for (const FParam& Param : Function.Params)
			{
				FString Parameter;
				if (Param.Direction == EParamDirection::Out)
				{
					Parameter += TEXT("out ");
				}
				else if (Param.Direction == EParamDirection::InOut)
				{
					Parameter += TEXT("inout ");
				}
				Parameter += PrintDreamShaderLangType(Param.Type);
				Parameter += TEXT(" ");
				Parameter += Param.Name;
				if (Param.Direction == EParamDirection::In && Param.Default.IsValid())
				{
					Parameter += TEXT(" = ");
					Parameter += PrintDreamShaderLangExpr(*Param.Default);
				}
				Parameters.Add(MoveTemp(Parameter));
				Arguments.Add(Param.Name);
			}

			// The wrapper's own name is never seen (`@name` names the asset); it only has to stay clear of the header's names.
			const bool bVoid = Function.ReturnType.Category == ETypeCategory::Void;
			Out += FString::Printf(
				TEXT("export %s DreamShaderGenerated_%d(%s)\n{\n    %s%s(%s);\n}\n"), /* I18N-EXEMPT: generated source text */
				bVoid ? TEXT("void") : *PrintDreamShaderLangType(Function.ReturnType),
				Index,
				*FString::Join(Parameters, TEXT(", ")),
				bVoid ? TEXT("") : TEXT("return "),
				*Function.Name,
				*FString::Join(Arguments, TEXT(", ")));
		}
		return Out;
	}

	bool BuildDreamShaderPackageAssetUnit(
		const FString& HeaderPath,
		const FString& RawHeaderText,
		const UE::DreamShader::FDreamShaderDefineTable& Defines,
		FString& OutUnitText,
		UE::DreamShader::Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace UE::DreamShader::Lang;

		UE::DreamShader::FDreamShaderPreprocessResult Preprocessed;
		UE::DreamShader::FDreamShaderTextError PreprocessError;
		if (!UE::DreamShader::PreprocessDreamShaderSource(
			RawHeaderText,
			HeaderPath,
			Defines,
			Preprocessed,
			PreprocessError,
			UE::DreamShader::EDreamShaderPreprocessDialect::Mixed))
		{
			Diagnostics.Error(TEXT("DSH8291"), FLangSpan(), FText::Format(
				LOCTEXT("HeaderPreprocessFailed", "'{0}' failed conditional compilation: {1}: {2}"),
				FText::FromString(HeaderPath),
				FText::FromString(PreprocessError.Code),
				PreprocessError.Message));
			return false;
		}

		const FLangSourceText Source(HeaderPath, Preprocessed.Text);
		FLangParseResult Parse = ParseDreamShaderLang(Source);
		const bool bParsed = Parse.Succeeded();
		Diagnostics.Append(MoveTemp(Parse.Diagnostics));
		if (!bParsed)
		{
			return false;
		}

		TArray<FDreamShaderPackageGenerateRequest> Requests;
		CollectDreamShaderPackageGenerateRequests(*Parse.Module, Requests, Diagnostics);
		if (Diagnostics.HasErrors())
		{
			return false;
		}

		OutUnitText = BuildDreamShaderPackageWrapperSource(HeaderPath, UE::DreamShader::GetPackageAssetRoot(HeaderPath), Requests);
		return true;
	}

	FString GetDreamShaderPackageAssetUnitPath(const FString& HeaderPath)
	{
		return HeaderPath + TEXT(".generated.dss");
	}
}

#undef LOCTEXT_NAMESPACE
