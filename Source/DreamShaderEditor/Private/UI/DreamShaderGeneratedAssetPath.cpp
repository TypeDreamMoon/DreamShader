#include "UI/DreamShaderGeneratedAssetPath.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderParser.h"
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
		return ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, OutObjectPath, AssetName, OutError);
	}
}

#undef LOCTEXT_NAMESPACE
