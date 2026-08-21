// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShaderLang Path(...) asset-reference resolution: parses Game/Engine/Plugin asset references
// into validated Unreal long object paths. Self-contained string + package/plugin path logic.
// Extracted from DreamShaderMaterialGeneratorSupport.cpp; the public entry
// TryResolveDreamShaderAssetReference stays declared in the private header.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"

namespace UE::DreamShader::Editor::Private
{
	static bool SplitSimpleArgumentList(const FString& InText, TArray<FString>& OutArguments)
	{
		OutArguments.Reset();

		FString Current;
		bool bInString = false;
		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Character = InText[Index];
			if (bInString && Character == TCHAR('\\') && InText.IsValidIndex(Index + 1))
			{
				Current.AppendChar(Character);
				Current.AppendChar(InText[++Index]);
				continue;
			}

			if (Character == TCHAR('"'))
			{
				bInString = !bInString;
				Current.AppendChar(Character);
				continue;
			}

			if (!bInString && Character == TCHAR(','))
			{
				OutArguments.Add(Current.TrimStartAndEnd());
				Current.Reset();
				continue;
			}

			Current.AppendChar(Character);
		}

		if (bInString)
		{
			return false;
		}

		OutArguments.Add(Current.TrimStartAndEnd());
		return true;
	}

	static FString UnescapeDreamShaderStringLiteral(const FString& InText)
	{
		FString Unescaped;
		Unescaped.Reserve(InText.Len());
		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Character = InText[Index];
			if (Character != TCHAR('\\') || !InText.IsValidIndex(Index + 1))
			{
				Unescaped.AppendChar(Character);
				continue;
			}

			const TCHAR EscapedCharacter = InText[++Index];
			switch (EscapedCharacter)
			{
			case TCHAR('n'):
				Unescaped.AppendChar(TCHAR('\n'));
				break;
			case TCHAR('r'):
				Unescaped.AppendChar(TCHAR('\r'));
				break;
			case TCHAR('t'):
				Unescaped.AppendChar(TCHAR('\t'));
				break;
			case TCHAR('"'):
				Unescaped.AppendChar(TCHAR('"'));
				break;
			case TCHAR('\\'):
				Unescaped.AppendChar(TCHAR('\\'));
				break;
			default:
				Unescaped.AppendChar(EscapedCharacter);
				break;
			}
		}

		return Unescaped;
	}

	static FString TrimMatchingQuotes(const FString& InText)
	{
		FString Result = InText.TrimStartAndEnd();
		if (Result.Len() >= 2 && Result.StartsWith(TEXT("\"")) && Result.EndsWith(TEXT("\"")))
		{
			return UnescapeDreamShaderStringLiteral(Result.Mid(1, Result.Len() - 2));
		}

		return Result;
	}

	static bool ResolveContentPluginAssetReferenceRoot(
		const FString& Root,
		const FString& PluginName,
		FString& OutPackagePath,
		FDreamShaderError& OutError)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but no enabled plugin with that name was found."), *Root, *PluginName); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (!Plugin->IsEnabled())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but the plugin is not enabled."), *Root, *PluginName); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (!Plugin->CanContainContent())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but the plugin cannot contain content."), *Root, *PluginName); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		const FString ContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());
		if (!IFileManager::Get().DirectoryExists(*ContentDir))
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but its Content directory does not exist: '%s'."), *Root, *PluginName, *ContentDir); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		if (!Plugin->IsMounted())
		{
			OutError = FString::Printf(TEXT("Asset Path root '%s' references plugin '%s', but the plugin content is not mounted."), *Root, *PluginName); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}
#endif

		FString MountedAssetPath = Plugin->GetMountedAssetPath();
		MountedAssetPath.TrimStartAndEndInline();
		MountedAssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (MountedAssetPath.EndsWith(TEXT("/")))
		{
			MountedAssetPath.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
		}
		if (!MountedAssetPath.StartsWith(TEXT("/")))
		{
			MountedAssetPath = TEXT("/") + MountedAssetPath;
		}
		if (MountedAssetPath.IsEmpty() || MountedAssetPath == TEXT("/"))
		{
			MountedAssetPath = TEXT("/") + Plugin->GetName();
		}

		OutPackagePath = MountedAssetPath;
		return true;
	}

	static bool ResolveAssetReferenceRootPackagePath(
		const FString& Root,
		FString& OutPackagePath,
		FDreamShaderError& OutError)
	{
		FString Normalized = Root;
		Normalized.TrimStartAndEndInline();
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Normalized.StartsWith(TEXT("/")))
		{
			Normalized.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
		}
		while (Normalized.EndsWith(TEXT("/")))
		{
			Normalized.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
		}

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.IsEmpty())
		{
			OutError = TEXT("Relative asset Path(...) references require a root such as Game, Engine, or Plugin.PluginName.");
			return false;
		}

		const FString RootSegment = Segments[0].TrimStartAndEnd();
		int32 FirstFolderSegmentIndex = 1;
		if (RootSegment.Equals(TEXT("Game"), ESearchCase::IgnoreCase))
		{
			OutPackagePath = TEXT("/Game");
		}
		else if (RootSegment.Equals(TEXT("Engine"), ESearchCase::IgnoreCase))
		{
			OutPackagePath = TEXT("/Engine");
		}
		else if (RootSegment.StartsWith(TEXT("Plugin."), ESearchCase::IgnoreCase)
			|| RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase))
		{
			const int32 PluginPrefixLength = RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase) ? 8 : 7;
			const FString PluginName = RootSegment.Mid(PluginPrefixLength).TrimStartAndEnd();
			if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
			{
				OutError = FString::Printf(TEXT("Asset Path root '%s' has an invalid plugin name."), *Root); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (!ResolveContentPluginAssetReferenceRoot(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
		}
		else if ((RootSegment.Equals(TEXT("Plugin"), ESearchCase::IgnoreCase)
			|| RootSegment.Equals(TEXT("Plugins"), ESearchCase::IgnoreCase)) && Segments.IsValidIndex(1))
		{
			const FString PluginName = Segments[1].TrimStartAndEnd();
			if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
			{
				OutError = FString::Printf(TEXT("Asset Path root '%s' has an invalid plugin name."), *Root); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}
			if (!ResolveContentPluginAssetReferenceRoot(Root, PluginName, OutPackagePath, OutError))
			{
				return false;
			}
			FirstFolderSegmentIndex = 2;
		}
		else
		{
			OutError = FString::Printf(TEXT("Unsupported asset Path root '%s'. Use Game, Engine, or Plugin.PluginName."), *Root); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		for (int32 Index = FirstFolderSegmentIndex; Index < Segments.Num(); ++Index)
		{
			OutPackagePath += TEXT("/");
			OutPackagePath += Segments[Index].TrimStartAndEnd();
		}

		return true;
	}

	bool TryResolveDreamShaderAssetReference(const FString& InText, FString& OutObjectPath, FDreamShaderError& OutError)
	{
		OutObjectPath.Reset();

		FString Candidate = InText.TrimStartAndEnd();
		if (Candidate.IsEmpty())
		{
			OutError = TEXT("Asset reference cannot be empty.");
			return false;
		}

		FString RootName;
		FString AssetPath;
		if (Candidate.StartsWith(TEXT("Path("), ESearchCase::IgnoreCase))
		{
			if (!Candidate.EndsWith(TEXT(")")))
			{
				OutError = TEXT("Asset Path(...) reference is missing a closing ')'.");
				return false;
			}

			const FString ArgumentBlock = Candidate.Mid(5, Candidate.Len() - 6);
			TArray<FString> Arguments;
			if (!SplitSimpleArgumentList(ArgumentBlock, Arguments))
			{
				OutError = TEXT("Asset Path(...) contains an unterminated string literal.");
				return false;
			}

			if (Arguments.Num() == 1)
			{
				AssetPath = Arguments[0];
			}
			else if (Arguments.Num() == 2)
			{
				RootName = Arguments[0];
				AssetPath = Arguments[1];
			}
			else
			{
				OutError = TEXT("Asset Path(...) expects either 1 argument (/Game/... path) or 2 arguments (Game|Engine|Plugin.PluginName, asset path).");
				return false;
			}
		}
		else
		{
			AssetPath = Candidate;
		}

		RootName = TrimMatchingQuotes(RootName);
		AssetPath = TrimMatchingQuotes(AssetPath);
		AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (AssetPath.IsEmpty())
		{
			OutError = TEXT("Asset reference requires a non-empty path.");
			return false;
		}

		FString LongObjectPath;
		if (AssetPath.StartsWith(TEXT("/")))
		{
			LongObjectPath = AssetPath;
		}
		else
		{
			FString PackageRoot;
			if (!ResolveAssetReferenceRootPackagePath(RootName, PackageRoot, OutError))
			{
				return false;
			}
			LongObjectPath = PackageRoot + TEXT("/") + AssetPath;
		}

		const int32 LastSlashIndex = LongObjectPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		const int32 LastDotIndex = LongObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastSlashIndex == INDEX_NONE || LastSlashIndex >= LongObjectPath.Len() - 1)
		{
			OutError = FString::Printf(TEXT("Invalid asset path '%s'."), *LongObjectPath); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		if (LastDotIndex == INDEX_NONE || LastDotIndex < LastSlashIndex)
		{
			const FString AssetName = LongObjectPath.Mid(LastSlashIndex + 1);
			LongObjectPath += TEXT(".") + AssetName;
		}

		FText PathError;
		if (!FPackageName::IsValidObjectPath(LongObjectPath, &PathError))
		{
			OutError = PathError.ToString();
			return false;
		}

		OutObjectPath = LongObjectPath;
		return true;
	}

}
