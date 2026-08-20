#include "Bridge/DreamShaderEditorBridge.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"
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

		if (ShouldSkipDreamShaderEditorBridge())
		{
			return;
		}

		Bridge = MakeShared<UE::DreamShader::Editor::Private::FDreamShaderEditorBridge, ESPMode::ThreadSafe>();
		GDreamShaderEditorBridge = Bridge;
		Bridge->Startup();

		UE::DreamShader::Editor::Private::FDreamShaderMaterialBrowser::Register();
	}

	virtual void ShutdownModule() override
	{
		if (CookPostEngineInitHandle.IsValid())
		{
			DREAMSHADER_POST_ENGINE_INIT_DELEGATE().Remove(CookPostEngineInitHandle);
			CookPostEngineInitHandle.Reset();
		}

		UE::DreamShader::Editor::Private::FDreamShaderMaterialBrowser::Unregister();

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
		UE::DreamShader::Editor::Private::FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);

		if (SourceFiles.IsEmpty())
		{
			return;
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader cook: generating %d source file(s) as persistent assets..."), SourceFiles.Num());

		// Writing the .uasset is not enough for the cooker to find it. A cook request is resolved
		// through IAssetRegistry::DoesPackageExistOnDisk, which consults only the registry's in-memory
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

		int32 FailureCount = 0;
		for (const FString& SourceFile : SourceFiles)
		{
			const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			if (UE::DreamShader::IsDreamShaderHeaderFile(NormalizedPath))
			{
				continue;
			}

			FString Message;
			const bool bSuccess = UE::DreamShader::Editor::FMaterialGenerator::GenerateAssetsFromFile(NormalizedPath, Message, true, false);
			if (bSuccess)
			{
				UE_LOG(LogDreamShader, Display, TEXT("  [Cook] %s"), *Message);
			}
			else
			{
				UE_LOG(LogDreamShader, Error, TEXT("  [Cook] Failed: %s"), *Message);
				++FailureCount;
			}
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
			// DreamShader material. A cook that could not generate every source file must not succeed.
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
