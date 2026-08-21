// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Source loading for the DreamShader material generator (recursive import inlining + cycle
// detection + header/function-file rule checks). Extracted byte-for-byte from
// DreamShaderMaterialGenerator.cpp's anonymous namespace; LoadPreparedDreamShaderSource is promoted
// to external linkage (DreamShaderMaterialGeneratorSourceLoading.h) for the generator entry points.

#include "DreamShaderMaterialGeneratorSourceLoading.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderModule.h"
#include "Misc/FileHelper.h"

namespace UE::DreamShader::Editor
{
	static bool ResolveDreamShaderImportPath(
		const FString& CurrentFilePath,
		const FString& ImportSpecifier,
		FString& OutResolvedPath,
		FDreamShaderError& OutError)
	{
		FString ResolveError;
		if (Private::FDreamShaderDependencyGraphService::ResolveImportPath(
			CurrentFilePath,
			ImportSpecifier,
			OutResolvedPath,
			&ResolveError))
		{
			return true;
		}

		// A root-qualified import that named an unknown root says so precisely; everything else is an
		// ordinary miss, where naming the specifier and the importer is all there is to say.
		if (ResolveError.IsEmpty())
		{
			OutError = FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
				TEXT("DreamShader import '%s' referenced from '%s' could not be resolved."),
				*ImportSpecifier,
				*CurrentFilePath);
		}
		else
		{
			OutError = ResolveError;
		}
		return false;
	}

	static bool LoadPreparedDreamShaderSourceRecursive(
		const FString& SourceFilePath,
		TSet<FString>& InOutVisitedFiles,
		TSet<FString>& InOutActiveStack,
		FString& OutSourceText,
		FDreamShaderError& OutError)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		if (InOutVisitedFiles.Contains(NormalizedPath))
		{
			return true;
		}

		if (InOutActiveStack.Contains(NormalizedPath))
		{
			return FailWith(OutError, TEXT("DSH8133"), FString::Printf(TEXT("DreamShader import cycle detected at '%s'."), *NormalizedPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *NormalizedPath))
		{
			return FailWith(OutError, TEXT("DSH8134"), FString::Printf(TEXT("DreamShader could not read '%s'."), *NormalizedPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		InOutActiveStack.Add(NormalizedPath);

		TArray<FString> Lines;
		SourceText.ParseIntoArrayLines(Lines, false);

		FString SanitizedSourceText;
		SanitizedSourceText.Reserve(SourceText.Len());

		for (const FString& Line : Lines)
		{
			FString ImportPath;
			if (Private::FDreamShaderDependencyGraphService::TryExtractImportPathFromLine(Line, ImportPath))
			{
				FString ResolvedImportPath;
				if (!ResolveDreamShaderImportPath(NormalizedPath, ImportPath, ResolvedImportPath, OutError))
				{
					return false;
				}

				if (!LoadPreparedDreamShaderSourceRecursive(ResolvedImportPath, InOutVisitedFiles, InOutActiveStack, OutSourceText, OutError))
				{
					return false;
				}
				// Emit a blank placeholder line where the import directive was. The diagnostics mapper
				// counts emitted lines within each file's Begin/End block to recover physical line
				// numbers; dropping the import line outright would shift every subsequent line up by the
				// number of imports above it, so errors in this file would report the wrong line.
				SanitizedSourceText += TEXT("\n");
				continue;
			}

			SanitizedSourceText += Line;
			SanitizedSourceText += TEXT("\n");
		}

		if (UE::DreamShader::IsDreamShaderHeaderFile(NormalizedPath)
			&& (SanitizedSourceText.Contains(TEXT("Shader("), ESearchCase::IgnoreCase)
				|| SanitizedSourceText.Contains(TEXT("ShaderFunction("), ESearchCase::IgnoreCase)
				|| SanitizedSourceText.Contains(TEXT("ShaderLayer("), ESearchCase::IgnoreCase)
				|| SanitizedSourceText.Contains(TEXT("ShaderLayerBlend("), ESearchCase::IgnoreCase)
				|| SanitizedSourceText.Contains(TEXT("MaterialLayer("), ESearchCase::IgnoreCase)
				|| SanitizedSourceText.Contains(TEXT("MaterialLayerBlend("), ESearchCase::IgnoreCase)))
		{
			return FailWith(OutError, TEXT("DSH8135"), FString::Printf(TEXT("DreamShader header '%s' may only declare Function/Namespace/GraphFunction/VirtualFunction blocks and imports."), *NormalizedPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		if (UE::DreamShader::IsDreamShaderFunctionFile(NormalizedPath)
			&& SanitizedSourceText.Contains(TEXT("Shader("), ESearchCase::IgnoreCase))
		{
			return FailWith(OutError, TEXT("DSH8136"), FString::Printf(TEXT("DreamShader function file '%s' may only declare imports, Function/Namespace/GraphFunction/VirtualFunction blocks, and ShaderFunction/ShaderLayer/ShaderLayerBlend blocks."), *NormalizedPath)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		OutSourceText += FString::Printf(TEXT("// Begin DreamShader source: %s\n"), *NormalizedPath); /* I18N-EXEMPT: deferred codegen or compatibility path */
		OutSourceText += SanitizedSourceText;
		OutSourceText += FString::Printf(TEXT("\n// End DreamShader source: %s\n\n"), *NormalizedPath); /* I18N-EXEMPT: deferred codegen or compatibility path */

		InOutActiveStack.Remove(NormalizedPath);
		InOutVisitedFiles.Add(NormalizedPath);
		return true;
	}

	bool LoadPreparedDreamShaderSource(const FString& SourceFilePath, FString& OutSourceText, FDreamShaderError& OutError)
	{
		OutSourceText.Reset();
		TSet<FString> VisitedFiles;
		TSet<FString> ActiveStack;
		return LoadPreparedDreamShaderSourceRecursive(SourceFilePath, VisitedFiles, ActiveStack, OutSourceText, OutError);
	}
}
