// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Source loading for the DreamShader material generator (preprocessing + recursive import inlining +
// cycle detection + header/function-file rule checks). Extracted byte-for-byte from
// DreamShaderMaterialGenerator.cpp's anonymous namespace; LoadPreparedDreamShaderSource is promoted
// to external linkage (DreamShaderMaterialGeneratorSourceLoading.h) for the generator entry points.

#include "DreamShaderMaterialGeneratorSourceLoading.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderModule.h"
#include "DreamShaderPreprocessor.h"
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

	// Counts line terminators the way FString::ParseIntoArrayLines splits on them -- CRLF as one, and
	// a lone CR or LF as one each -- so the result is exactly one less than the number of lines the
	// inlining loop below will iterate over, whatever the file's line endings are.
	//
	// Its only caller is the line-count check across the preprocessor; that check is why this counts
	// terminators faithfully instead of just counting '\n', which would pass vacuously on a CR-only
	// file (0 == 0) -- the one shape where a preprocessor line bug would be least likely to be noticed
	// by anything else.
	static int32 CountSourceLineTerminators(const FString& InText)
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

	// Folds one file's touched-define set into the union carried up the recursion.
	//
	// A name arriving twice with two different values is an internal contradiction rather than a
	// source error: the define table is resolved once per compile (see LoadPreparedDreamShaderSource)
	// and handed unchanged to every file, so within one run a name cannot legitimately evaluate to two
	// values. It is logged and survived rather than asserted on. It is not corruption -- the key still
	// folds a real value, and first-writer-wins is deterministic because the recursion order is fixed
	// by the import graph, so the hash stays stable across runs rather than flipping with TMap order.
	// And this path runs inside `dsc` batch builds and inside an artist's open editor, where killing
	// the process over a build-key composition anomaly costs far more than a log line naming exactly
	// what broke.
	static void MergeTouchedDefines(
		UE::DreamShader::FDreamShaderDefineValueMap& InOutUnion,
		const UE::DreamShader::FDreamShaderDefineValueMap& InFileTouchedDefines,
		const FString& InFilePathForDiagnostics)
	{
		for (const TPair<FString, FString>& Pair : InFileTouchedDefines)
		{
			if (const FString* Existing = InOutUnion.Find(Pair.Key))
			{
				// Case-sensitive, matching the define table itself: `1` and `ON` are different values
				// exactly as `Foo` and `FOO` are different names.
				if (!Existing->Equals(Pair.Value, ESearchCase::CaseSensitive))
				{
					UE_LOG(LogDreamShader, Warning, /* I18N-EXEMPT: internal invariant report, never displayed */
						TEXT("DreamShader preprocessor reported define '%s' as both '%s' and '%s' while preparing '%s'. ")
						TEXT("The define table is fixed for the whole compile, so this can only mean the preprocessor or ")
						TEXT("the table changed underneath it. Keeping '%s' in the build key."),
						*Pair.Key,
						**Existing,
						*Pair.Value,
						*InFilePathForDiagnostics,
						**Existing);
				}
				continue;
			}

			InOutUnion.Add(Pair.Key, Pair.Value);
		}
	}

	static bool LoadPreparedDreamShaderSourceRecursive(
		const FString& SourceFilePath,
		const FDreamShaderDefineTable& InDefines,
		TSet<FString>& InOutVisitedFiles,
		TSet<FString>& InOutActiveStack,
		FString& OutSourceText,
		UE::DreamShader::FDreamShaderDefineValueMap& InOutTouchedDefines,
		bool& bInOutAnySourceHadDirectives,
		FDreamShaderError& OutError)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		if (InOutVisitedFiles.Contains(NormalizedPath))
		{
			// Already inlined once, so its defines are already in the union and its directives are
			// already reflected in the flag. Preprocessing it again would be redundant rather than
			// wrong: the define table is fixed for the compile and the preprocessor is deterministic,
			// so a second run over the same file necessarily produces the same text and the same
			// touched set.
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

		// Conditional compilation runs HERE: after the read, before anything looks at the text.
		//
		// The order is the feature. `#if` is allowed to wrap an `Import` line, so an import inside a
		// branch that was cut has to be gone before the loop below goes looking for imports -- run the
		// preprocessor after import extraction instead and the cut import gets inlined anyway, which is
		// the exact bug conditionals exist to prevent (a Substrate-only header pulled into a non-
		// Substrate build).
		//
		// Consequence worth naming: `#define` is therefore FILE-LOCAL. A definition in a .dsh is not
		// visible to the .dsm that imports it, because the .dsh is preprocessed on its own before its
		// text ever reaches the importer. That is the deliberate half of the trade -- C's cross-file
		// macro state is what makes include order load-bearing there -- and central switches belong in
		// the settings table or RegisterDreamShaderDefine instead.
		//
		// The dependency graph service deliberately does NOT come through here; see the comment on
		// FDreamShaderDependencyGraphService::TryExtractImportPathFromLine.
		{
			FDreamShaderPreprocessResult PreprocessResult;
			FDreamShaderTextError PreprocessError;
			if (!UE::DreamShader::PreprocessDreamShaderSource(SourceText, NormalizedPath, InDefines, PreprocessResult, PreprocessError))
			{
				// ToStringError, not a fresh FailWith: the preprocessor raises real DSH103x codes and
				// this is only a change of carrier (FText to FString) across the module boundary.
				// Re-raising here would replace a precise code with a vague one.
				OutError = UE::DreamShader::ToStringError(PreprocessError);
				return false;
			}

			// Line-count conservation is a cross-module contract, and the only cheap place to check it
			// is where the two modules meet. Break it and nothing fails: the generated asset is still
			// correct, but every diagnostic below the first directive in every conditional source
			// points at the wrong line, and that is a bug that gets misattributed to the diagnostics
			// mapper for a very long time. One O(n) scan over text we are about to walk character by
			// character anyway turns it into a named failure at the moment it appears.
			//
			// ensureMsgf rather than checkf: the damage is misreported line numbers, not a bad asset,
			// and this runs in `dsc` batch builds and in an artist's live editor. A crash there is a
			// far worse outcome than a loud log with a callstack, and ensure still breaks into an
			// attached debugger, which is where whoever is editing the preprocessor will be.
			const int32 LineTerminatorsBefore = CountSourceLineTerminators(SourceText);
			const int32 LineTerminatorsAfter = CountSourceLineTerminators(PreprocessResult.Text);
			ensureMsgf(
				LineTerminatorsBefore == LineTerminatorsAfter,
				TEXT("DreamShader preprocessor changed the line count of '%s' (%d -> %d). Directive and elided ")
				TEXT("lines must be emitted as empty lines, never removed: the diagnostics mapper recovers ")
				TEXT("physical line numbers by counting lines inside each file's Begin/End block."),
				*NormalizedPath,
				LineTerminatorsBefore + 1,
				LineTerminatorsAfter + 1);

			MergeTouchedDefines(InOutTouchedDefines, PreprocessResult.TouchedDefines, NormalizedPath);
			bInOutAnySourceHadDirectives |= PreprocessResult.bHadDirectives;

			SourceText = MoveTemp(PreprocessResult.Text);
		}

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

				if (!LoadPreparedDreamShaderSourceRecursive(
					ResolvedImportPath,
					InDefines,
					InOutVisitedFiles,
					InOutActiveStack,
					OutSourceText,
					InOutTouchedDefines,
					bInOutAnySourceHadDirectives,
					OutError))
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

	bool LoadPreparedDreamShaderSource(
		const FString& SourceFilePath,
		FString& OutSourceText,
		UE::DreamShader::FDreamShaderDefineValueMap& OutTouchedDefines,
		bool& bOutAnySourceHadDirectives,
		FDreamShaderError& OutError)
	{
		OutSourceText.Reset();
		OutTouchedDefines.Reset();
		bOutAnySourceHadDirectives = false;

		// Resolved ONCE for the whole recursion, then passed down by reference.
		//
		// Not for speed alone -- resolving walks the settings, the registry and every provider
		// delegate, so doing it per file would be quadratic in a deep import chain -- but for meaning:
		// one table for one compile is what makes the touched-define union coherent and the build key
		// provable. Re-resolving per file would let a provider that answers differently on its second
		// call produce a source whose halves were compiled against different define sets, and nothing
		// downstream could tell.
		const FDreamShaderDefineTable Defines = UE::DreamShader::ResolveDreamShaderDefines();

		TSet<FString> VisitedFiles;
		TSet<FString> ActiveStack;
		return LoadPreparedDreamShaderSourceRecursive(
			SourceFilePath,
			Defines,
			VisitedFiles,
			ActiveStack,
			OutSourceText,
			OutTouchedDefines,
			bOutAnySourceHadDirectives,
			OutError);
	}

	bool LoadPreparedDreamShaderSource(const FString& SourceFilePath, FString& OutSourceText, FDreamShaderError& OutError)
	{
		UE::DreamShader::FDreamShaderDefineValueMap DiscardedTouchedDefines;
		bool bDiscardedAnySourceHadDirectives = false;
		return LoadPreparedDreamShaderSource(
			SourceFilePath,
			OutSourceText,
			DiscardedTouchedDefines,
			bDiscardedAnySourceHadDirectives,
			OutError);
	}
}
