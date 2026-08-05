#include "UI/DreamShaderGeneratedAssetPath.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderParser.h"
#include "DreamShaderTypes.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Misc/FileHelper.h"

namespace UE::DreamShader::Editor::Private
{
	bool ResolveGeneratedAssetObjectPath(const FString& SourceFilePath, FString& OutObjectPath, FString& OutError)
	{
		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *SourceFilePath))
		{
			OutError = FString::Printf(TEXT("Failed to read DreamShader source '%s'."), *SourceFilePath);
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
		FString ParseError;
		if (!FTextShaderParser::Parse(SourceText, Definition, ParseError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *ParseError);
			return false;
		}

		if (Definition.Name.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s: this file does not define a top-level Shader block."), *SourceFilePath);
			return false;
		}

		// Same default the generator applies, so the path shown here is the path that gets written.
		ApplyDefaultRootFromSourceFile(SourceFilePath, Definition);

		FString PackageName;
		FString AssetName;
		return ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, OutObjectPath, AssetName, OutError);
	}
}
