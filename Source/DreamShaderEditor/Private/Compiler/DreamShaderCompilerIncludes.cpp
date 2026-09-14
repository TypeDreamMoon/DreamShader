// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderCompilerIncludes.h.

#include "DreamShaderCompilerIncludes.h"

#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderDiagnostic.h"
#include "DreamShaderModule.h"
#include "DreamShaderPreprocessor.h"
#include "Lang/LangParser.h"

#include "HAL/FileManager.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DreamShader.Pipeline"

namespace UE::DreamShader::Editor::Compiler
{
	using UE::DreamShader::Lang::ELangFileKind;
	using UE::DreamShader::Lang::FLangDiagnosticSink;
	using UE::DreamShader::Lang::FLangParseResult;
	using UE::DreamShader::Lang::FLangSourceText;
	using UE::DreamShader::Lang::FLangSpan;
	using UE::DreamShader::Lang::FModule;

	namespace
	{
		/** `/Game/Shared/Common.dsh` -> `Shared/Common.dsh`; a plain leading `/` is just dropped. */
		FString StripRootAnchor(const FString& Specifier)
		{
			FString Remainder = Specifier;
			if (Remainder.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
			{
				Remainder.RightChopInline(6, DREAMSHADER_ALLOW_SHRINKING_NO);
				return Remainder;
			}

			while (Remainder.StartsWith(TEXT("/")))
			{
				Remainder.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
			return Remainder;
		}

		bool TryExistingFile(const FString& Candidate, FString& OutResolvedPath)
		{
			const FString Normalized = UE::DreamShader::NormalizeSourceFilePath(Candidate);
			if (!IFileManager::Get().FileExists(*Normalized))
			{
				return false;
			}

			OutResolvedPath = Normalized;
			return true;
		}
	}

	bool ResolveDreamShaderIncludePath(
		const FString& IncludeSpecifier,
		const FString& FromFile,
		FString& OutResolvedPath,
		FString& OutError)
	{
		OutError.Reset();

		// NormalizeImportSpecifier, not a hand-rolled trim: it is what decides that a specifier with
		// no extension means `.dsh`, and having two answers to that question in one plugin is how a
		// header resolves for `import` and not for `#include`.
		const FString Normalized =
			Private::FDreamShaderDependencyGraphService::NormalizeImportSpecifier(IncludeSpecifier);
		if (Normalized.IsEmpty())
		{
			OutError = TEXT("the include path is empty"); /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
			return false;
		}

		// 1. An absolute path is itself. A test fixture and a `-Source` outside every root both
		//    arrive this way, and refusing them would make the resolver less capable than `import`.
		if (!FPaths::IsRelative(Normalized) && TryExistingFile(Normalized, OutResolvedPath))
		{
			return true;
		}

		// 2. Root-anchored. Deliberately never relative to the including file: `/x` and `x` mean
		//    different things, which is the only reason to spell the slash at all.
		if (Normalized.StartsWith(TEXT("/")))
		{
			const FString Remainder = StripRootAnchor(Normalized);
			if (Remainder.IsEmpty())
			{
				OutError = TEXT("the include path names no file"); /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				return false;
			}

			// The including file's own root first, so a plugin's `/Shared/X.dsh` means that plugin's
			// Shared, not the project's -- the same non-shadowing rule an unqualified `import` has.
			TArray<const UE::DreamShader::FDreamShaderSourceRoot*> Roots;
			if (const UE::DreamShader::FDreamShaderSourceRoot* Owning = UE::DreamShader::FindSourceRootForFile(FromFile))
			{
				Roots.Add(Owning);
			}
			for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
			{
				Roots.AddUnique(&Root);
			}

			for (const UE::DreamShader::FDreamShaderSourceRoot* Root : Roots)
			{
				if (TryExistingFile(FPaths::Combine(Root->Directory, Remainder), OutResolvedPath)
					|| TryExistingFile(FPaths::Combine(Root->PackagesDirectory, Remainder), OutResolvedPath))
				{
					return true;
				}
			}

			OutError = FString::Printf( /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
				TEXT("no source root contains '%s'"),
				*Remainder);
			return false;
		}

		// 3. The 1.x rules, unchanged, for everything else.
		FString ResolveError;
		if (Private::FDreamShaderDependencyGraphService::ResolveImportPath(
			FromFile,
			IncludeSpecifier,
			OutResolvedPath,
			&ResolveError))
		{
			OutResolvedPath = UE::DreamShader::NormalizeSourceFilePath(OutResolvedPath);
			return true;
		}

		OutError = ResolveError.IsEmpty()
			? FString::Printf(TEXT("no file matched it beside '%s' or under its source root"), *FPaths::GetCleanFilename(FromFile)) /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
			: ResolveError;
		return false;
	}

	FDreamShaderIncludeResolver::FDreamShaderIncludeResolver(const UE::DreamShader::FDreamShaderDefineTable& InDefines)
		: Defines(InDefines)
	{
	}

	UE::DreamShader::Lang::FLangIncludeResolver FDreamShaderIncludeResolver::MakeBinderResolver()
	{
		return [this](const FString& IncludePath, const FString& FromFile, FLangDiagnosticSink& Diagnostics) -> const FModule*
		{
			return Resolve(IncludePath, FromFile, Diagnostics);
		};
	}

	const FModule* FDreamShaderIncludeResolver::Resolve(
		const FString& IncludePath,
		const FString& FromFile,
		FLangDiagnosticSink& Diagnostics)
	{
		// The span the binder gave us is not available here -- FLangIncludeResolver passes paths, not
		// nodes -- so an include failure is reported against the top of the including file. The
		// binder raises its own located DSH4xxx for the directive itself; this names the file system
		// reason, which is the half the binder cannot know.
		const FLangSpan NoSpan;

		FString ResolvedPath;
		FString ResolveError;
		if (!ResolveDreamShaderIncludePath(IncludePath, FromFile, ResolvedPath, ResolveError))
		{
			Diagnostics.Error(TEXT("DSH8292"), NoSpan, FText::Format(
				LOCTEXT("IncludeUnresolved", "'{0}', included from '{1}', could not be resolved: {2}."),
				FText::FromString(IncludePath),
				FText::FromString(FPaths::GetCleanFilename(FromFile)),
				FText::FromString(ResolveError)));
			return nullptr;
		}

		if (const TUniquePtr<FModule>* Cached = ModulesByPath.Find(ResolvedPath))
		{
			return Cached->Get();
		}

		if (FailedPaths.Contains(ResolvedPath))
		{
			// Already reported. Answering null again without a second diagnostic keeps one broken
			// header from producing one error per file that includes it.
			return nullptr;
		}

		const ELangFileKind Kind = UE::DreamShader::Lang::GetLangFileKindFromPath(ResolvedPath);
		if (Kind != ELangFileKind::Dsh && Kind != ELangFileKind::Dss)
		{
			FailedPaths.Add(ResolvedPath);
			Diagnostics.Error(TEXT("DSH8295"), NoSpan, FText::Format(
				LOCTEXT("IncludeWrongKind", "'{0}' is not a DreamShader header; an include names a '.dsh' (or a '.dss'), not a '{1}' file."),
				FText::FromString(ResolvedPath),
				FText::FromString(FPaths::GetExtension(ResolvedPath))));
			return nullptr;
		}

		FString RawText;
		if (!FFileHelper::LoadFileToString(RawText, *ResolvedPath))
		{
			FailedPaths.Add(ResolvedPath);
			Diagnostics.Error(TEXT("DSH8293"), NoSpan, FText::Format(
				LOCTEXT("IncludeUnreadable", "'{0}', included from '{1}', resolved but could not be read."),
				FText::FromString(ResolvedPath),
				FText::FromString(FPaths::GetCleanFilename(FromFile))));
			return nullptr;
		}

		// Preprocessed ON ITS OWN, against the compile's table. See the class comment: a `#define`
		// in a header is not visible to the file that included it, and that is the rule, not a gap.
		UE::DreamShader::FDreamShaderPreprocessResult PreprocessResult;
		UE::DreamShader::FDreamShaderTextError PreprocessError;
		if (!UE::DreamShader::PreprocessDreamShaderSource(RawText, ResolvedPath, Defines, PreprocessResult, PreprocessError, UE::DreamShader::EDreamShaderPreprocessDialect::Lang2))
		{
			FailedPaths.Add(ResolvedPath);
			// The preprocessor's own DSH103x code is carried in the message rather than replacing
			// this site's code: the two say different things -- which header, and what was wrong in
			// it -- and losing either makes the failure harder to act on.
			Diagnostics.Error(TEXT("DSH8291"), NoSpan, FText::Format(
				LOCTEXT("IncludePreprocessFailed", "'{0}' failed conditional compilation: {1}: {2}"),
				FText::FromString(ResolvedPath),
				FText::FromString(PreprocessError.Code),
				PreprocessError.Message));
			return nullptr;
		}

		for (const TPair<FString, FString>& Pair : PreprocessResult.TouchedDefines)
		{
			// First writer wins, matching MergeTouchedDefines in the 1.x loader: the table is fixed
			// for the compile, so a second value for one name cannot be legitimate, and the first is
			// deterministic because the include order is.
			if (!TouchedDefines.Contains(Pair.Key))
			{
				TouchedDefines.Add(Pair.Key, Pair.Value);
			}
		}
		bAnyIncludeHadDirectives |= PreprocessResult.bHadDirectives;

		TUniquePtr<FLangSourceText> Source = MakeUnique<FLangSourceText>(ResolvedPath, PreprocessResult.Text);
		FLangParseResult ParseResult = UE::DreamShader::Lang::ParseDreamShaderLang(*Source);

		// Appended whatever the outcome: a header's warnings belong to the compile that read it, and
		// a header that parsed with errors still has a tree the binder can work through.
		Diagnostics.Append(MoveTemp(ParseResult.Diagnostics));

		if (!ParseResult.Module.IsValid())
		{
			FailedPaths.Add(ResolvedPath);
			Diagnostics.Error(TEXT("DSH8294"), NoSpan, FText::Format(
				LOCTEXT("IncludeUnparsable", "'{0}', included from '{1}', could not be parsed; its own errors are above."),
				FText::FromString(ResolvedPath),
				FText::FromString(FPaths::GetCleanFilename(FromFile))));
			return nullptr;
		}

		// The markers are the 1.x ones on purpose. Nothing parses this text -- it exists only to be
		// hashed -- but the diagnostics mapper and the shader-error mapper both read blocks in this
		// shape, so writing a second shape here would be a second thing to keep in step.
		IncludedSourceDigestText += FString::Printf(TEXT("// Begin DreamShader source: %s\n"), *ResolvedPath); /* I18N-EXEMPT: build-key material, never displayed */
		IncludedSourceDigestText += PreprocessResult.Text;
		IncludedSourceDigestText += FString::Printf(TEXT("\n// End DreamShader source: %s\n\n"), *ResolvedPath); /* I18N-EXEMPT: build-key material, never displayed */

		ResolvedIncludePaths.AddUnique(ResolvedPath);
		SourceTexts.Add(MoveTemp(Source));

		const TUniquePtr<FModule>& Stored = ModulesByPath.Add(ResolvedPath, MoveTemp(ParseResult.Module));
		return Stored.Get();
	}
}

#undef LOCTEXT_NAMESPACE
