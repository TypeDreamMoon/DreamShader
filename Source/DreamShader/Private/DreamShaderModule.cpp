#include "DreamShaderModule.h"

#include "DreamShaderSettings.h"

#include "HAL/FileManager.h"
#include "CoreGlobals.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Char.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "UObject/UObjectBase.h"

DEFINE_LOG_CATEGORY(LogDreamShader);

namespace UE::DreamShader
{
	namespace Private
	{
		static const FString GeneratedShaderVirtualDirectory = TEXT("/DreamShaderGenerated");

		struct FConfiguredDirectories
		{
			FString Source;
			FString Package;
			FString Generated;
			bool bInitialized = false;
		};

		static FConfiguredDirectories ConfiguredDirectories;

		static FString ResolveProjectDirectory(const FString& ConfiguredPath, const FString& DefaultPath)
		{
			FString PathText = ConfiguredPath;
			PathText.TrimStartAndEndInline();
			if (PathText.IsEmpty())
			{
				PathText = DefaultPath;
			}

			FString ResolvedPath = FPaths::IsRelative(PathText)
				? FPaths::Combine(FPaths::ProjectDir(), PathText)
				: PathText;
			FPaths::NormalizeFilename(ResolvedPath);
			FPaths::MakeStandardFilename(ResolvedPath);
			return ResolvedPath;
		}

		static bool CanReadSettingsObject()
		{
			return UObjectInitialized() && !GExitPurge && !IsEngineExitRequested();
		}

		/**
		 * The settings object is not constructible when this module starts: it loads at
		 * PostConfigInit (see StartupModule for why), which is before the UObject system is up.
		 * The ini is readable by then, so the two directory settings are taken straight from it
		 * -- the same values the settings object will report once it exists, since that is where
		 * it loads them from. An FDirectoryPath is written as (Path="...").
		 */
		static FString ReadDirectorySettingFromConfig(const TCHAR* Key)
		{
			FString Value;
			if (GConfig && GConfig->GetString(TEXT("/Script/DreamShader.DreamShaderSettings"), Key, Value, GEngineIni))
			{
				FString Path;
				if (FParse::Value(*Value, TEXT("Path="), Path))
				{
					return Path;
				}
			}
			return FString();
		}

		static void RefreshConfiguredDirectories()
		{
			const UDreamShaderSettings* Settings = CanReadSettingsObject()
				? GetDefault<UDreamShaderSettings>()
				: nullptr;

			ConfiguredDirectories.Source = ResolveProjectDirectory(
				Settings ? Settings->SourceDirectory.Path : ReadDirectorySettingFromConfig(TEXT("SourceDirectory")),
				TEXT("DShader"));
			ConfiguredDirectories.Package = FPaths::Combine(ConfiguredDirectories.Source, TEXT("Packages"));
			ConfiguredDirectories.Generated = ResolveProjectDirectory(
				Settings ? Settings->GeneratedShaderDirectory.Path : ReadDirectorySettingFromConfig(TEXT("GeneratedShaderDirectory")),
				TEXT("Intermediate/DreamShader/GeneratedShaders"));
			ConfiguredDirectories.bInitialized = true;
		}

		static const FConfiguredDirectories& GetConfiguredDirectories()
		{
			if (!ConfiguredDirectories.bInitialized || CanReadSettingsObject())
			{
				RefreshConfiguredDirectories();
			}

			return ConfiguredDirectories;
		}

		/**
		 * The root list is cached because building it stats one directory per enabled plugin, and
		 * discovery calls this in loops. The two inputs that can change mid-session without a plugin
		 * mount -- the configured source directory and the scan toggle -- are snapshotted and compared,
		 * so a Project Settings edit still takes effect on the next call.
		 */
		struct FSourceRootCache
		{
			TArray<FDreamShaderSourceRoot> Roots;
			FString ProjectDirectory;
			bool bScannedPlugins = false;
			bool bInitialized = false;
		};

		static FSourceRootCache SourceRootCache;

		static FString CanonicalizeRootDirectory(const FString& InDirectory)
		{
			FString Result = NormalizeSourceFilePath(InDirectory);
			Result.RemoveFromEnd(TEXT("/"));
			return Result;
		}

		static void RebuildSourceRoots(const FString& InProjectDirectory, bool bInScanPlugins)
		{
			SourceRootCache.Roots.Reset();
			SourceRootCache.ProjectDirectory = InProjectDirectory;
			SourceRootCache.bScannedPlugins = bInScanPlugins;
			SourceRootCache.bInitialized = true;

			FDreamShaderSourceRoot& ProjectRoot = SourceRootCache.Roots.AddDefaulted_GetRef();
			ProjectRoot.Directory = CanonicalizeRootDirectory(InProjectDirectory);
			ProjectRoot.PackagesDirectory = CanonicalizeRootDirectory(
				FPaths::Combine(InProjectDirectory, TEXT("Packages")));
			ProjectRoot.DisplayName = TEXT("Project");
			ProjectRoot.bIsProjectRoot = true;
			ProjectRoot.bWritable = true;

			if (!bInScanPlugins)
			{
				return;
			}

			for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
			{
				const FString CandidateDirectory = CanonicalizeRootDirectory(
					FPaths::Combine(Plugin->GetBaseDir(), TEXT("DShader")));
				if (CandidateDirectory.IsEmpty() || !IFileManager::Get().DirectoryExists(*CandidateDirectory))
				{
					continue;
				}

				// Overlapping roots would hand the same file to two owners and make import resolution
				// depend on scan order. Happens when Source Directory is pointed at a plugin folder, or
				// at something that contains one.
				const FDreamShaderSourceRoot* OverlappingRoot = SourceRootCache.Roots.FindByPredicate(
					[&CandidateDirectory](const FDreamShaderSourceRoot& Existing)
					{
						return IsPathUnderSourceDirectory(CandidateDirectory, Existing.Directory)
							|| IsPathUnderSourceDirectory(Existing.Directory, CandidateDirectory);
					});
				if (OverlappingRoot != nullptr)
				{
					UE_LOG(
						LogDreamShader,
						Warning,
						TEXT("DreamShader ignored plugin source root '%s' because it overlaps the '%s' root at '%s'."),
						*CandidateDirectory,
						*OverlappingRoot->DisplayName,
						*OverlappingRoot->Directory);
					continue;
				}

				FDreamShaderSourceRoot& PluginRoot = SourceRootCache.Roots.AddDefaulted_GetRef();
				PluginRoot.Directory = CandidateDirectory;
				PluginRoot.PackagesDirectory = CanonicalizeRootDirectory(
					FPaths::Combine(CandidateDirectory, TEXT("Packages")));
				PluginRoot.DisplayName = Plugin->GetName();
				PluginRoot.PluginName = Plugin->GetName();
			}
		}

		static const TArray<FDreamShaderSourceRoot>& GetCachedSourceRoots()
		{
			const FString ProjectDirectory = GetConfiguredDirectories().Source;

			const UDreamShaderSettings* Settings = CanReadSettingsObject()
				? GetDefault<UDreamShaderSettings>()
				: nullptr;
			const bool bScanPlugins = Settings ? Settings->bScanPluginSourceDirectories : true;

			if (!SourceRootCache.bInitialized
				|| SourceRootCache.bScannedPlugins != bScanPlugins
				|| !SourceRootCache.ProjectDirectory.Equals(ProjectDirectory, ESearchCase::IgnoreCase))
			{
				RebuildSourceRoots(ProjectDirectory, bScanPlugins);
			}

			return SourceRootCache.Roots;
		}
	}

	FString GetSourceShaderDirectory()
	{
		return Private::GetConfiguredDirectories().Source;
	}

	FString GetPackageShaderDirectory()
	{
		return Private::GetConfiguredDirectories().Package;
	}

	// Game thread only: the returned reference points at a cache that a later call may rebuild.
	const TArray<FDreamShaderSourceRoot>& GetSourceShaderRoots()
	{
		return Private::GetCachedSourceRoots();
	}

	const FDreamShaderSourceRoot* FindSourceRootForFile(const FString& InPath)
	{
		// Roots never overlap (RebuildSourceRoots drops the ones that would), so the longest match is
		// only a defensive tie-break.
		const FDreamShaderSourceRoot* BestRoot = nullptr;
		for (const FDreamShaderSourceRoot& Root : GetSourceShaderRoots())
		{
			if (IsPathUnderSourceDirectory(InPath, Root.Directory)
				&& (BestRoot == nullptr || Root.Directory.Len() > BestRoot->Directory.Len()))
			{
				BestRoot = &Root;
			}
		}

		return BestRoot;
	}

	bool IsWritableSourceFilePath(const FString& InPath)
	{
		// A path outside every root is an ad-hoc source the caller named explicitly (a commandlet
		// -Source, a test fixture); it is not the plugin's to protect.
		const FDreamShaderSourceRoot* Root = FindSourceRootForFile(InPath);
		return Root == nullptr || Root->bWritable;
	}

	void RefreshSourceShaderRoots()
	{
		Private::SourceRootCache.bInitialized = false;
	}

	bool IsPathUnderSourceDirectory(const FString& InPath, const FString& InDirectory)
	{
		if (InPath.IsEmpty() || InDirectory.IsEmpty())
		{
			return false;
		}

		const FString Path = NormalizeSourceFilePath(InPath);
		FString Directory = NormalizeSourceFilePath(InDirectory);
		Directory.RemoveFromEnd(TEXT("/"));

		return Path.Equals(Directory, ESearchCase::IgnoreCase)
			|| Path.StartsWith(Directory + TEXT("/"), ESearchCase::IgnoreCase);
	}

	FString GetGeneratedShaderDirectory()
	{
		const FString VirtualDirectory = GetGeneratedShaderVirtualDirectory();
		if (const FString* MappedDirectory = AllShaderSourceDirectoryMappings().Find(VirtualDirectory))
		{
			return *MappedDirectory;
		}

		return Private::GetConfiguredDirectories().Generated;
	}

	FString GetGeneratedShaderVirtualDirectory()
	{
		return Private::GeneratedShaderVirtualDirectory;
	}

	FString SanitizeIdentifier(const FString& InText)
	{
		FString Result;
		Result.Reserve(InText.Len() + 1);

		for (TCHAR Char : InText)
		{
			if ((Char >= TCHAR('A') && Char <= TCHAR('Z'))
				|| (Char >= TCHAR('a') && Char <= TCHAR('z'))
				|| (Char >= TCHAR('0') && Char <= TCHAR('9'))
				|| Char == TCHAR('_'))
			{
				Result.AppendChar(Char);
			}
			else
			{
				Result.AppendChar(TEXT('_'));
			}
		}

		if (Result.IsEmpty())
		{
			Result = TEXT("DreamShaderSymbol");
		}

		bool bOnlyUnderscores = true;
		for (int32 Index = 0; Index < Result.Len(); ++Index)
		{
			if (Result[Index] != TCHAR('_'))
			{
				bOnlyUnderscores = false;
				break;
			}
		}
		if (bOnlyUnderscores)
		{
			Result = TEXT("DreamShaderSymbol");
		}

		if (!((Result[0] >= TCHAR('A') && Result[0] <= TCHAR('Z'))
			|| (Result[0] >= TCHAR('a') && Result[0] <= TCHAR('z'))
			|| Result[0] == TCHAR('_')))
		{
			Result.InsertAt(0, TCHAR('_'));
		}

		for (int32 Index = Result.Len() - 1; Index > 0; --Index)
		{
			if (Result[Index] == TCHAR('_') && Result[Index - 1] == TCHAR('_'))
			{
				Result.RemoveAt(Index, 1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
		}

		return Result;
	}

	FString NormalizeSourceFilePath(const FString& InPath)
	{
		FString Result = FPaths::ConvertRelativePathToFull(InPath);
		FPaths::NormalizeFilename(Result);
		FPaths::MakeStandardFilename(Result);
		return Result;
	}

	bool IsDreamShaderMaterialFile(const FString& InPath)
	{
		return FPaths::GetExtension(InPath, true).Equals(TEXT(".dsm"), ESearchCase::IgnoreCase);
	}

	bool IsDreamShaderHeaderFile(const FString& InPath)
	{
		return FPaths::GetExtension(InPath, true).Equals(TEXT(".dsh"), ESearchCase::IgnoreCase);
	}

	bool IsDreamShaderFunctionFile(const FString& InPath)
	{
		return FPaths::GetExtension(InPath, true).Equals(TEXT(".dsf"), ESearchCase::IgnoreCase);
	}

	bool IsDreamShaderSourceFile(const FString& InPath)
	{
		return IsDreamShaderMaterialFile(InPath) || IsDreamShaderHeaderFile(InPath) || IsDreamShaderFunctionFile(InPath);
	}
}

void FDreamShaderModule::StartupModule()
{
	// This module loads at PostConfigInit, not Default, because of the mapping below. The engine
	// validates every material's cached include paths as the material loads
	// (FMaterialCachedExpressionData, "Expression include file path ... is invalid"), and any
	// material reached during UMaterialInterface::InitDefaultMaterials -- M_DreamWindGrass, pulled
	// in by a PostConfigInit module -- loads before the Default-phase modules exist. With the
	// /DreamShaderGenerated mapping still unregistered at that point, the include was stripped
	// from the cached data, every permutation then failed with "File not found", and the material
	// rendered as the default material for the whole session (the startup regeneration skipped it,
	// its source hash being unchanged). Shader directory mappings belong to PostConfigInit for
	// exactly this reason; the cost is that the settings object is not available yet, which
	// RefreshConfiguredDirectories covers by reading the ini.
	IFileManager::Get().MakeDirectory(*UE::DreamShader::GetSourceShaderDirectory(), true);
	IFileManager::Get().MakeDirectory(*UE::DreamShader::GetPackageShaderDirectory(), true);
	IFileManager::Get().MakeDirectory(*UE::DreamShader::Private::GetConfiguredDirectories().Generated, true);

	const FString VirtualDirectory = UE::DreamShader::GetGeneratedShaderVirtualDirectory();
	const FString GeneratedShaderDirectory = UE::DreamShader::Private::GetConfiguredDirectories().Generated;
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualDirectory))
	{
		AddShaderSourceDirectoryMapping(VirtualDirectory, GeneratedShaderDirectory);
	}

	// Static plugin shaders (instance-backend builtin support header).
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
	{
		const FString PluginShaderVirtualDirectory = TEXT("/Plugin/DreamShader");
		if (!AllShaderSourceDirectoryMappings().Contains(PluginShaderVirtualDirectory))
		{
			AddShaderSourceDirectoryMapping(PluginShaderVirtualDirectory, FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
		}
	}
}

void FDreamShaderModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDreamShaderModule, DreamShader);
