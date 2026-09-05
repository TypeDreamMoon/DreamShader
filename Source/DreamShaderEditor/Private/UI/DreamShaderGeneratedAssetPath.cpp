#include "UI/DreamShaderGeneratedAssetPath.h"
#include "DreamShaderDiagnostic.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderParser.h"
// PreprocessDreamShaderSource / ResolveDreamShaderDefines: this helper has to answer with the same
// cut the generator made, or it names an asset the source does not build. See the block below.
#include "DreamShaderPreprocessor.h"
#include "DreamShaderTypes.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	bool ResolveGeneratedAssetObjectPath(const FString& SourceFilePath, FString& OutObjectPath, FString& OutError)
	{
		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *SourceFilePath))
		{
			OutError = FText::Format(LOCTEXT("AssetPathReadFailed", "Failed to read DreamShader source '{0}'."), FText::FromString(SourceFilePath)).ToString();
			return false;
		}

		// Conditional compilation runs HERE, in the same order the generator uses: read, preprocess,
		// then look at imports. Two separate things go wrong without it.
		//
		// The first is loud: a `#if` line is not something the parser has a rule for, so every source
		// that uses conditionals would fail to resolve a path at all and the browser would list it as
		// Unresolved.
		//
		// The second is quiet, and it is the reason this has to be the SAME cut the generator made
		// rather than merely some cut. This function's contract is "name the asset that generation
		// wrote". `Shader(Name=..., Root=...)` may itself sit inside a branch, so a different define set
		// answers with a different object path -- and every consumer downstream (the browser's status
		// column, its up-to-date check, the preview) would then be talking about a material that is not
		// the one this source builds, with nothing anywhere reporting a mismatch.
		//
		// Order matters for the usual reason too: `#if` is allowed to wrap an `Import` line, so the
		// directives have to be resolved before anything goes looking for imports below.
		{
			FDreamShaderPreprocessResult PreprocessResult;
			FDreamShaderTextError PreprocessError;
			if (!UE::DreamShader::PreprocessDreamShaderSource(
				SourceText,
				SourceFilePath,
				UE::DreamShader::ResolveDreamShaderDefines(),
				PreprocessResult,
				PreprocessError))
			{
				// The DSH103x message already reads "<path>(<line>): ...", so it is not wrapped the way
				// the parse failure below is -- wrapping would print the file twice. Its code is lost
				// here because this function's out-parameter is a bare FString with nowhere to put one;
				// the same failure reaches the user with its code intact when the generator compiles
				// the file, which is the path that has an FDreamShaderError to carry it.
				OutError = PreprocessError.Message.ToString();
				return false;
			}

			SourceText = MoveTemp(PreprocessResult.Text);
		}

		// Import lines carry no top-level block and would confuse the parser's block detection; strip them
		// the same way ResolvePreviewMaterial does.
		TArray<FString> Lines;
		SourceText.ParseIntoArrayLines(Lines, false);
		SourceText.Reset();
		for (const FString& Line : Lines)
		{
			FString ImportPath;
			if (FDreamShaderDependencyGraphService::TryExtractImportPathFromLine(Line, ImportPath))
			{
				continue;
			}
			SourceText += Line;
			SourceText += TEXT("\n");
		}

		FTextShaderDefinition Definition;
		FText ParseError;
		if (!FTextShaderParser::Parse(SourceText, Definition, ParseError))
		{
			OutError = FText::Format(LOCTEXT("AssetPathParseError", "{0}: {1}"), FText::FromString(SourceFilePath), ParseError).ToString();
			return false;
		}

		if (Definition.Name.IsEmpty())
		{
			OutError = FText::Format(LOCTEXT("AssetPathNoShaderBlock", "{0}: this file does not define a top-level Shader block."), FText::FromString(SourceFilePath)).ToString();
			return false;
		}

		// Same default the generator applies, so the path shown here is the path that gets written.
		ApplyDefaultRootFromSourceFile(SourceFilePath, Definition);

		FString PackageName;
		FString AssetName;
		FDreamShaderError DestinationError;
		const bool bResolvedDestination = ResolveDreamShaderAssetDestination(
			Definition.Name, Definition.Root, PackageName, OutObjectPath, AssetName, DestinationError);
		OutError = DestinationError.Message;
		return bResolvedDestination;
	}
}

#undef LOCTEXT_NAMESPACE
