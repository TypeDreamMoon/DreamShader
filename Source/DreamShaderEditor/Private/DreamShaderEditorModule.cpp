#include "Bridge/DreamShaderEditorBridge.h"
#include "Navigation/DreamShaderSourceNavigation.h"
#include "Pass/DreamPassPipelineCustomization.h"
#include "DreamShaderCompilerService.h"
#include "SourceFiles/DreamShaderAssetRenameSyncService.h"
#include "SourceFiles/DreamShaderBatchCompile.h"
#include "DreamShaderSourceFileUtils.h"
#include "UI/DreamShaderMaterialBrowser.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "CoreGlobals.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

namespace
{
	TWeakPtr<UE::DreamShader::Editor::Private::FDreamShaderEditorBridge, ESPMode::ThreadSafe> GDreamShaderEditorBridge;

	bool ShouldSkipDreamShaderEditorBridge()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("NoDreamShaderEditorBridge"));
	}

	bool IsCookCommandlet()
	{
		FString CommandletName;
		return FParse::Value(FCommandLine::Get(), TEXT("-run="), CommandletName) && CommandletName.Contains(TEXT("Cook"));
	}

	bool IsCookWorkerProcess()
	{
		// Multiprocess cook: the director launches every worker with -cookworker (see CookDirector).
		// A worker's -run= still contains "Cook", so IsCookCommandlet() is true for it too. Only the
		// director should materialize the source files as persistent assets; the workers then load what
		// the director saved. Without this gate every process regenerates and races to save the same
		// packages.
		return FParse::Param(FCommandLine::Get(), TEXT("cookworker"));
	}
}

class FDreamShaderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet())
		{
			if (IsCookCommandlet() && !IsCookWorkerProcess())
			{
				// MaterialEditingLibrary accesses editor subsystems that are not ready while startup
				// modules are loading. Generate after engine initialization but before commandlet Main.
				CookPostEngineInitHandle = DREAMSHADER_POST_ENGINE_INIT_DELEGATE().AddRaw(
					this,
					&FDreamShaderEditorModule::HandlePostEngineInitForCook);
			}
			return;
		}

		// The details panel of a pass pipeline reads the asset alone, so it is there with or without the bridge.
		UE::DreamShader::Editor::Private::FDreamPassPipelineCustomization::Register();

		if (ShouldSkipDreamShaderEditorBridge())
		{
			return;
		}

		Bridge = MakeShared<UE::DreamShader::Editor::Private::FDreamShaderEditorBridge, ESPMode::ThreadSafe>();
		GDreamShaderEditorBridge = Bridge;
		Bridge->Startup();

		UE::DreamShader::Editor::Private::FDreamShaderMaterialBrowser::Register();

		// Node <-> source navigation: the material editor graph-node context menu entry. Gated with
		// every other menu, so -NoDreamShaderEditorBridge and a commandlet run see none of it.
		UE::DreamShader::Editor::Private::FDreamShaderSourceNavigationMenu::Register();

		// After the bridge, because ownership decides whether this process may rewrite sources at all.
		UE::DreamShader::Editor::Private::FDreamShaderAssetRenameSyncService::Startup();
	}

	virtual void ShutdownModule() override
	{
		if (CookPostEngineInitHandle.IsValid())
		{
			DREAMSHADER_POST_ENGINE_INIT_DELEGATE().Remove(CookPostEngineInitHandle);
			CookPostEngineInitHandle.Reset();
		}

		UE::DreamShader::Editor::Private::FDreamShaderAssetRenameSyncService::Shutdown();

		UE::DreamShader::Editor::Private::FDreamShaderSourceNavigationMenu::Unregister();

		UE::DreamShader::Editor::Private::FDreamShaderMaterialBrowser::Unregister();

		UE::DreamShader::Editor::Private::FDreamPassPipelineCustomization::Unregister();

		if (Bridge)
		{
			Bridge->Shutdown();
			Bridge.Reset();
		}

		GDreamShaderEditorBridge.Reset();
	}

private:
	void HandlePostEngineInitForCook()
	{
		// Materials are always memory-only in the editor, so the cook must materialize every
		// source file as a persistent asset for packaging. Director only -- workers load the
		// saved packages (see IsCookWorkerProcess).
		GenerateAllAssetsForCook();
	}

	void GenerateAllAssetsForCook()
	{
		TArray<FString> SourceFiles;
		UE::DreamShader::Editor::Private::FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles, UE::DreamShader::Editor::Private::EDreamShaderPackageSources::IncludeBuildable);

		if (SourceFiles.IsEmpty())
		{
			return;
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader cook: generating %d source file(s) as persistent assets..."), SourceFiles.Num());

		// Writing the .uasset is not enough for the cooker to find it. A cook request is resolved
		// through IAssetRegistry::DoesPackageExistOnDisk, which consults only the registry's cached
		// State and has no filesystem fallback -- so a package the registry never scanned is dropped
		// from the request list even though the file is right there on disk, and
		// DirectoriesToAlwaysCook silently skips it. Generation runs on post-engine-init, which is
		// after the registry enumerated the content directories, so every asset saved below lands in
		// exactly that blind spot. Record what the saves actually wrote and hand the filenames back to
		// the registry here, while there is still time: this runs before the commandlet's Main, and
		// therefore before the cooker collects its initial requests.
		TArray<FString> SavedPackageFilenames;
		const FDelegateHandle PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddLambda(
			[&SavedPackageFilenames](const FString& PackageFilename, UPackage*, FObjectPostSaveContext)
			{
				// ScanModifiedAssetFiles runs every entry through FilenameToLongPackageName, which
				// wants a path under a mounted root; SavePackage reports the engine-relative one.
				SavedPackageFilenames.AddUnique(FPaths::ConvertRelativePathToFull(PackageFilename));
			});

		TArray<FString> CompileUnits;
		for (const FString& SourceFile : SourceFiles)
		{
			const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			if (UE::DreamShader::Editor::Private::FDreamShaderSourceFileUtils::IsCompileUnit(NormalizedPath))
			{
				CompileUnits.Add(NormalizedPath);
			}
		}
		// Function sources first so the usual case builds in one pass; the batch retries whatever
		// still waited for a function another source generates (#72).
		UE::DreamShader::Editor::Private::SortDreamShaderSourcesForBatch(CompileUnits);

		int32 FailureCount = 0;
		int32 ForeignFailureCount = 0;
		::UE::DreamShader::IDreamShaderCompiler* const Compiler = ::UE::DreamShader::GetDreamShaderCompiler();
		if (Compiler == nullptr)
		{
			// Without the compiler nothing can be generated at all, whatever the file belongs to.
			for (const FString& SourceFile : CompileUnits)
			{
				UE_LOG(LogDreamShader, Error, TEXT("  [Cook] Failed: %s"),
					*FString::Printf(TEXT("%s: the DreamShader compiler module is not available."), *SourceFile)); /* I18N-EXEMPT: cook log line */
				++FailureCount;
			}
		}
		else
		{
			// Forced and Materialized: a cook packages what is on disk, so every product must be there and current.
			::UE::DreamShader::FDreamShaderCompileRequest RequestTemplate;
			RequestTemplate.bForce = true;
			RequestTemplate.ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Materialized;

			UE::DreamShader::Editor::Private::CompileDreamShaderSourcesInBatch(*Compiler, CompileUnits, RequestTemplate,
				[&FailureCount, &ForeignFailureCount](const UE::DreamShader::Editor::Private::FDreamShaderBatchCompileEntry& Entry)
				{
					const FString Message = Entry.Result.Message.ToString();
					if (Entry.Result.bSucceeded)
					{
						UE_LOG(LogDreamShader, Display, TEXT("  [Cook] %s"), *Message);
						return;
					}

					// Whether this file's products are this project's to build at all. A source under an
					// engine or marketplace plugin's DShader folder belongs to that plugin: gate 3 refuses
					// to generate into its mount (DSH8090), and the project may not edit the file either (a
					// plugin root ships its sources as-is). That plugin ships the assets its sources
					// describe, so a failure there is reported and the cook goes on -- the project's own
					// sources, and those of its own plugins under <Project>/Plugins, stay fatal.
					const UE::DreamShader::FDreamShaderSourceRoot* const OwnerRoot =
						UE::DreamShader::FindSourceRootForFile(Entry.SourceFile);
					const bool bOwnedByProject = OwnerRoot == nullptr
						|| OwnerRoot->bIsProjectRoot
						|| OwnerRoot->bIsProjectPlugin;
					if (!bOwnedByProject)
					{
						// Loud, not silent: the plugin's own build is the only place this source generates,
						// and a cook that said nothing here would read as if the file had been built.
						UE_LOG(LogDreamShader, Warning,
							TEXT("  [Cook] Not this project's to build (plugin '%s'): %s"),
							*OwnerRoot->DisplayName,
							*Message);
						++ForeignFailureCount;
					}
					else
					{
						UE_LOG(LogDreamShader, Error, TEXT("  [Cook] Failed: %s"), *Message);
						++FailureCount;
					}
				});
		}

		if (ForeignFailureCount > 0)
		{
			UE_LOG(LogDreamShader, Warning,
				TEXT("DreamShader cook: %d source file(s) belong to a plugin that is not a project plugin; those assets ship with that plugin, not with this project. See Docs/generation/asset-paths.md."),
				ForeignFailureCount);
		}

		UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);

		if (!SavedPackageFilenames.IsEmpty())
		{
			IAssetRegistry::GetChecked().ScanModifiedAssetFiles(SavedPackageFilenames);
			UE_LOG(LogDreamShader, Display,
				TEXT("DreamShader cook: registered %d generated package(s) with the AssetRegistry."),
				SavedPackageFilenames.Num());
		}

		if (FailureCount > 0)
		{
			// Fail the cook rather than package a build that is silently missing (or has a half-built)
			// DreamShader material. A cook that could not generate every source file this project owns
			// must not succeed. A source that belongs to another plugin is not counted here: its
			// products ship with that plugin, and no host project can generate or even edit it -- see
			// the [Cook] Not this project's to build entries above.
			UE_LOG(LogDreamShader, Fatal,
				TEXT("DreamShader cook generation failed for %d source file(s); aborting the cook. See the [Cook] Failed entries above."),
				FailureCount);
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader cook asset generation complete."));
	}

	TSharedPtr<UE::DreamShader::Editor::Private::FDreamShaderEditorBridge, ESPMode::ThreadSafe> Bridge;
	FDelegateHandle CookPostEngineInitHandle;
};

namespace UE::DreamShader::Editor::Private
{
	FDreamShaderEditorBridge* GetDreamShaderEditorBridge()
	{
		return GDreamShaderEditorBridge.Pin().Get();
	}
}

IMPLEMENT_MODULE(FDreamShaderEditorModule, DreamShaderEditor)
