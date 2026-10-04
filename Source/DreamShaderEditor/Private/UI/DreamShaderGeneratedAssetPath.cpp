#include "UI/DreamShaderGeneratedAssetPath.h"

// Product resolution, and the wire form of its first diagnostic.
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerDiagnostics.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	bool ResolveGeneratedAssetProduct(
		const FString& SourceFilePath,
		const bool bMaterialOnly,
		FString& OutObjectPath,
		FString& OutSourceHash,
		FText& OutError)
	{
		using ::UE::DreamShader::Editor::Compiler::FDreamShaderProductResolution;
		using ::UE::DreamShader::Editor::Compiler::FDreamShaderResolvedProduct;

		OutObjectPath.Reset();
		OutSourceHash.Reset();
		OutError = FText::GetEmpty();

		// The 1.x recipe this replaces preprocessed, stripped imports and ran the 1.x parser to find Shader(Name, Root);
		// it answered nothing for a `.dss`. Product resolution runs the compile's own front half instead -- define set,
		// front end by extension, binder, IR builder and destination rules included -- so the answer cannot drift from
		// what a compile writes.
		FDreamShaderProductResolution Resolution;
		if (!::UE::DreamShader::Editor::Compiler::ResolveDreamShaderSourceProducts(SourceFilePath, Resolution))
		{
			if (const UE::DreamShader::Lang::FLangDiagnostic* FirstError = Resolution.Diagnostics.FirstError())
			{
				// `<file>(<line>,<col>): DSHnnnn: <message>`, the form every other DreamShader message takes.
				OutError = FText::FromString(::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(*FirstError, Resolution.SourceFilePath));
			}
			else
			{
				OutError = FText::Format(
					LOCTEXT("AssetPathUnresolved", "{0}: the source could not be resolved to the assets it builds."),
					FText::FromString(SourceFilePath));
			}
			return false;
		}

		const FDreamShaderResolvedProduct* Product = Resolution.FindMaterialProduct();
		if (!Product)
		{
			Product = Resolution.Products.FindByPredicate([](const FDreamShaderResolvedProduct& Candidate)
			{
				return Candidate.Kind == UE::DreamShader::IR::EIRProductKind::MaterialInstance;
			});
		}
		if (!Product && !bMaterialOnly)
		{
			// A `.dsp`: its one pipeline, which is the asset the browser shows for it -- and never a material to preview.
			Product = Resolution.Products.FindByPredicate([](const FDreamShaderResolvedProduct& Candidate)
			{
				return Candidate.Kind == UE::DreamShader::IR::EIRProductKind::PassPipeline;
			});
		}
		if (!Product && !bMaterialOnly && Resolution.Products.Num() > 0)
		{
			Product = &Resolution.Products[0];
		}

		if (!Product)
		{
			OutError = bMaterialOnly
				? FText::Format(LOCTEXT("AssetPathNoMaterial", "{0}: this file builds no material or material instance."), FText::FromString(SourceFilePath))
				: FText::Format(LOCTEXT("AssetPathNoProduct", "{0}: this file declares no material, instance or exported function."), FText::FromString(SourceFilePath));
			return false;
		}

		OutObjectPath = Product->ObjectPath;
		OutSourceHash = Resolution.SourceHash;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
