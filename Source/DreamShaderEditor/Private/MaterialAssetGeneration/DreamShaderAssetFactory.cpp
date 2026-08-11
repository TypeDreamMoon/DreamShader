#include "DreamShaderMaterialGeneratorPrivate.h"

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetViewUtils.h"
#include "Factories/MaterialFactoryNew.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/Package.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		bool AppendSanitizedPackageSegment(
			const FString& RawSegment,
			const FString& ErrorContext,
			FString& InOutPackagePath,
			FString& OutError)
		{
			FString Segment = RawSegment.TrimStartAndEnd();
			if (Segment.IsEmpty())
			{
				return true;
			}

			const FString FolderName = ObjectTools::SanitizeObjectName(Segment);
			if (FolderName.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s contains an invalid folder segment."), *ErrorContext);
				return false;
			}

			InOutPackagePath += TEXT("/");
			InOutPackagePath += FolderName;
			return true;
		}

		bool ResolveProjectContentPluginPackageRoot(
			const FString& Root,
			const FString& PluginName,
			FString& OutPackagePath,
			FString& OutError)
		{
			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
			if (!Plugin.IsValid())
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but no enabled plugin with that name was found."), *Root, *PluginName);
				return false;
			}

			const FString PluginBaseDir = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
			const FString ProjectPluginsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
			if (Plugin->GetType() != EPluginType::Project || !FPaths::IsUnderDirectory(PluginBaseDir, ProjectPluginsDir))
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' must reference a project plugin under '%s'."), *Root, *ProjectPluginsDir);
				return false;
			}

			if (!Plugin->IsEnabled())
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but the plugin is not enabled."), *Root, *PluginName);
				return false;
			}

			if (!Plugin->CanContainContent())
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but the plugin cannot contain content."), *Root, *PluginName);
				return false;
			}

			const FString ContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());
			if (!IFileManager::Get().DirectoryExists(*ContentDir))
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but its Content directory does not exist: '%s'."), *Root, *PluginName, *ContentDir);
				return false;
			}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			if (!Plugin->IsMounted())
			{
				OutError = FString::Printf(TEXT("DreamShader Root '%s' references project plugin '%s', but the plugin content is not mounted."), *Root, *PluginName);
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

		bool ResolveDreamShaderRootPackagePath(
			const FString& Root,
			FString& OutPackagePath,
			FString& OutError)
		{
			FString Normalized = Root;
			Normalized.TrimStartAndEndInline();
			Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));

			if (Normalized.IsEmpty())
			{
				OutPackagePath = TEXT("/Game");
				return true;
			}

			const bool bHadLeadingSlash = Normalized.StartsWith(TEXT("/"));
			while (Normalized.StartsWith(TEXT("/")))
			{
				Normalized.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
			while (Normalized.EndsWith(TEXT("/")))
			{
				Normalized.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}

			if (Normalized.IsEmpty())
			{
				OutPackagePath = TEXT("/Game");
				return true;
			}

			TArray<FString> Segments;
			Normalized.ParseIntoArray(Segments, TEXT("/"), true);
			if (Segments.IsEmpty())
			{
				OutPackagePath = TEXT("/Game");
				return true;
			}

			const FString RootSegment = Segments[0].TrimStartAndEnd();
			int32 FirstFolderSegmentIndex = 1;
			if (RootSegment.Equals(TEXT("Game"), ESearchCase::IgnoreCase))
			{
				OutPackagePath = TEXT("/Game");
			}
			else if (RootSegment.StartsWith(TEXT("Plugin."), ESearchCase::IgnoreCase)
				|| RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase))
			{
				const int32 PluginPrefixLength = RootSegment.StartsWith(TEXT("Plugins."), ESearchCase::IgnoreCase) ? 8 : 7;
				FString PluginName = RootSegment.Mid(PluginPrefixLength).TrimStartAndEnd();
				if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
				{
					OutError = FString::Printf(TEXT("DreamShader Root '%s' has an invalid plugin name."), *Root);
					return false;
				}

				if (!ResolveProjectContentPluginPackageRoot(Root, PluginName, OutPackagePath, OutError))
				{
					return false;
				}
			}
			else if ((RootSegment.Equals(TEXT("Plugin"), ESearchCase::IgnoreCase)
				|| RootSegment.Equals(TEXT("Plugins"), ESearchCase::IgnoreCase)) && Segments.IsValidIndex(1))
			{
				FString PluginName = Segments[1].TrimStartAndEnd();
				if (PluginName.IsEmpty() || ObjectTools::SanitizeObjectName(PluginName) != PluginName)
				{
					OutError = FString::Printf(TEXT("DreamShader Root '%s' has an invalid plugin name."), *Root);
					return false;
				}

				if (!ResolveProjectContentPluginPackageRoot(Root, PluginName, OutPackagePath, OutError))
				{
					return false;
				}
				FirstFolderSegmentIndex = 2;
			}
			else if (bHadLeadingSlash)
			{
				if (RootSegment.IsEmpty() || ObjectTools::SanitizeObjectName(RootSegment) != RootSegment)
				{
					OutError = FString::Printf(TEXT("DreamShader Root '%s' has an invalid package root."), *Root);
					return false;
				}

				OutPackagePath = TEXT("/") + RootSegment;
			}
			else
			{
				OutPackagePath = TEXT("/Game");
				FirstFolderSegmentIndex = 0;
			}

			for (int32 Index = FirstFolderSegmentIndex; Index < Segments.Num(); ++Index)
			{
				if (!AppendSanitizedPackageSegment(Segments[Index], FString::Printf(TEXT("DreamShader Root '%s'"), *Root), OutPackagePath, OutError))
				{
					return false;
				}
			}

			return true;
		}
	}

	void ApplyDefaultRootFromSourceFile(
		const FString& SourceFilePath,
		FTextShaderDefinition& Definition,
		FString* OutFallbackReason)
	{
		if (OutFallbackReason != nullptr)
		{
			OutFallbackReason->Reset();
		}

		const UE::DreamShader::FDreamShaderSourceRoot* SourceRoot =
			UE::DreamShader::FindSourceRootForFile(SourceFilePath);
		if (SourceRoot == nullptr || SourceRoot->PluginName.IsEmpty())
		{
			return;
		}

		auto DeclaresNoRoot = [](const FString& Root)
		{
			// Only an absent (or whitespace-only) Root counts. `Root="/"` resolves to /Game just as
			// well and is the documented way for a plugin-root file to opt back out of the default.
			return Root.TrimStartAndEnd().IsEmpty();
		};

		const bool bNeedsDefault = DeclaresNoRoot(Definition.Root)
			|| Definition.MaterialFunctions.ContainsByPredicate(
				[&DeclaresNoRoot](const FTextShaderMaterialFunctionDefinition& FunctionDefinition)
				{
					return DeclaresNoRoot(FunctionDefinition.Root);
				});
		if (!bNeedsDefault)
		{
			return;
		}

		const FString InferredRoot = FString::Printf(TEXT("Plugin.%s"), *SourceRoot->PluginName);

		// Resolved before it is handed out: a plugin that cannot host content has no mount point to
		// default to, and /Game -- what every file did before source roots existed -- is the only
		// answer left. Silently emitting an unresolvable Root would turn a missing attribute into a
		// compile error the author never wrote.
		FString ResolvedPackagePath;
		FString ResolveError;
		if (!ResolveProjectContentPluginPackageRoot(InferredRoot, SourceRoot->PluginName, ResolvedPackagePath, ResolveError))
		{
			if (OutFallbackReason != nullptr)
			{
				*OutFallbackReason = FString::Printf(
					TEXT("'%s' declares no Root= and plugin '%s' cannot host generated content, so its assets go to /Game. %s"),
					*SourceFilePath,
					*SourceRoot->PluginName,
					*ResolveError);
			}
			return;
		}

		if (DeclaresNoRoot(Definition.Root))
		{
			Definition.Root = InferredRoot;
		}
		for (FTextShaderMaterialFunctionDefinition& FunctionDefinition : Definition.MaterialFunctions)
		{
			if (DeclaresNoRoot(FunctionDefinition.Root))
			{
				FunctionDefinition.Root = InferredRoot;
			}
		}
	}

	bool ResolveDreamShaderAssetDestination(
		const FString& AssetName,
		const FString& Root,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutAssetLeafName,
		FString& OutError)
	{
		FString Normalized = AssetName;
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

		if (Normalized.IsEmpty())
		{
			OutError = TEXT("DreamShader asset name must resolve to a non-empty asset path.");
			return false;
		}

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.IsEmpty())
		{
			OutError = TEXT("DreamShader asset name must resolve to a non-empty asset path.");
			return false;
		}

		OutAssetLeafName = ObjectTools::SanitizeObjectName(Segments.Last());
		if (OutAssetLeafName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("DreamShader asset name '%s' produced an invalid asset name."), *AssetName);
			return false;
		}

		FString PackagePath;
		if (!ResolveDreamShaderRootPackagePath(Root, PackagePath, OutError))
		{
			return false;
		}

		for (int32 Index = 0; Index < Segments.Num() - 1; ++Index)
		{
			if (!AppendSanitizedPackageSegment(
				Segments[Index],
				FString::Printf(TEXT("DreamShader asset name '%s'"), *AssetName),
				PackagePath,
				OutError))
			{
				return false;
			}
		}

		OutPackageName = PackagePath + TEXT("/") + OutAssetLeafName;
		OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *OutAssetLeafName);
		FText PathError;
		if (!FPackageName::IsValidObjectPath(OutObjectPath, &PathError))
		{
			const FString PathErrorText = PathError.ToString();
			OutError = PathErrorText.IsEmpty()
				? FString::Printf(TEXT("DreamShader asset path '%s' is not a valid Unreal object path."), *OutObjectPath)
				: PathErrorText;
			return false;
		}

		return true;
	}

	// In in-memory material mode a stale saved asset at the target path wins over in-memory
	// regeneration (the reuse path below loads and mutates it, unsaved). Surface that loudly so
	// users understand why an "in-memory" material still shows up as an on-disk asset.
	static void WarnIfInMemoryModeShadowedByDiskAsset(const bool bTransient, const FString& PackageName, const FString& ObjectPath)
	{
		if (bTransient && FPackageName::DoesPackageExist(PackageName))
		{
			UE_LOG(LogDreamShader, Warning,
				TEXT("In-memory material mode: '%s' already exists as a saved asset, which shadows in-memory regeneration. Delete the saved asset to make it fully in-memory."),
				*ObjectPath);
		}
	}

	// Publish a freshly created generated asset to the editor's discovery surfaces.
	//
	// This runs for IN-MEMORY generation too, not just the persist path. The Content Browser's folder
	// tree and the registry's recursive path expansion both read the registry path tree, and for a
	// package with no file on disk the only thing that populates it is AssetCreated ->
	// FAssetRegistryImpl::AddAssetPath. Skipping it meant any Shader(Name=...) naming a folder that no
	// on-disk asset already occupied generated a material that exists and renders but that the browser
	// has no route to: the folder never appeared, so nothing ever queried that package path, so the
	// live-object enumeration that would have found it (it only tests IsAsset() plus a package-path
	// match) was never asked. That is why the same source worked verbatim under an existing folder and
	// vanished the moment a new folder segment was inserted. AssetCreated also broadcasts AssetAdded,
	// so the tile appears immediately instead of on the next re-enumeration.
	//
	// AssetCreated alone is NOT enough for an in-memory folder. It adds the path and broadcasts, but it
	// never calls AddAssetData -- in-memory assets never enter the registry's State, which is the
	// on-disk cache. IAssetRegistry::HasAssets reads State, so a folder holding only in-memory
	// materials reports empty forever and the Content Browser drops it the moment "Show Empty Folders"
	// is off (UContentBrowserAssetDataSource::HideFolderIfEmpty). Measured, not inferred: an on-disk
	// material reports STATE=true while a generated in-memory material reports STATE=false at the same
	// moment its folder and tile are on screen. AssetViewUtils::OnAlwaysShowPath is the engine's own
	// answer to exactly this -- it stamps EContentBrowserFolderAttributes::AlwaysVisible, which
	// IsFolderVisible short-circuits on before it ever consults emptiness, and it walks up to the
	// parents itself so one call covers a whole new chain of folders. It is the same call the Content
	// Browser makes for a folder the user creates by hand, which is likewise empty at creation.
	//
	// Ordering is safe: generation runs off FCoreDelegates::OnPostEngineInit, which fires after the
	// Default loading phase, and UContentBrowserAssetDataSource subscribes to the delegate in its
	// Default-phase StartupModule -- so the listener is always bound before the first broadcast.
	//
	// In-memory hiding still wins where it applies: AssetCreated early-outs on !IsAsset(), so a hidden
	// memory-only UDreamShaderMaterialInstance registers nothing, and the IsAsset() test here keeps us
	// from force-showing a folder whose only occupant is invisible.
	static void PublishGeneratedAsset(UObject* GeneratedAsset, const FString& PackageName, const bool bTransient)
	{
		if (!GeneratedAsset)
		{
			return;
		}

		FAssetRegistryModule::AssetCreated(GeneratedAsset);

		if (bTransient && GeneratedAsset->IsAsset())
		{
			AssetViewUtils::OnAlwaysShowPath().Broadcast(FPackageName::GetLongPackagePath(PackageName));
		}
	}

	bool CreateOrReuseMaterial(const FTextShaderDefinition& Definition, UMaterial*& OutMaterial, FString& OutError, const bool bTransient)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		if (UObject* ExistingObject = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			OutMaterial = Cast<UMaterial>(ExistingObject);
			if (!OutMaterial)
			{
				OutError = FString::Printf(TEXT("Asset '%s' already exists and is not a Material."), *ObjectPath);
				return false;
			}

			// Ownership guard: never clear + overwrite a saved asset DreamShader did not generate. A
			// hand-authored material at this path (a real package on disk without our source metadata)
			// is refused; our own in-memory regeneration (no disk package) and persisted generated
			// materials (which carry the metadata) both pass.
			if (FPackageName::DoesPackageExist(PackageName) && !HasDreamShaderSourceMetadata(ExistingObject))
			{
				OutError = FString::Printf(
					TEXT("Asset '%s' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating."),
					*ObjectPath);
				return false;
			}

			WarnIfInMemoryModeShadowedByDiskAsset(bTransient, PackageName, ObjectPath);
			return true;
		}

		UPackage* MaterialPackage = CreatePackage(*PackageName);
		if (!MaterialPackage)
		{
			OutError = FString::Printf(TEXT("Failed to create package '%s'."), *PackageName);
			return false;
		}

		if (bTransient)
		{
			MaterialPackage->SetPackageFlags(PKG_NewlyCreated);
			OutMaterial = NewObject<UMaterial>(MaterialPackage, FName(*AssetName), RF_Public | RF_Standalone);
		}
		else
		{
			UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
			OutMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
				UMaterial::StaticClass(),
				MaterialPackage,
				FName(*AssetName),
				RF_Public | RF_Standalone,
				nullptr,
				GWarn));
		}

		if (!OutMaterial)
		{
			OutError = FString::Printf(TEXT("Failed to create material '%s'."), *ObjectPath);
			return false;
		}

		// A Graph-backend UMaterial has no IsAsset() override, so it was already visible under existing
		// folders; publishing only makes the registry and the folder tree agree with that.
		PublishGeneratedAsset(OutMaterial, PackageName, bTransient);
		return true;
	}

	bool CreateOrReuseInstanceMaterial(const FTextShaderDefinition& Definition, UDreamShaderMaterialInstance*& OutInstance, FString& OutError, const bool bTransient)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		if (UObject* ExistingObject = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			OutInstance = Cast<UDreamShaderMaterialInstance>(ExistingObject);
			if (!OutInstance)
			{
				OutError = FString::Printf(
					TEXT("Asset '%s' already exists and is not a DreamShader instance material. Delete it (or remove Backend=\"Instance\") before switching backends."),
					*ObjectPath);
				return false;
			}

			WarnIfInMemoryModeShadowedByDiskAsset(bTransient, PackageName, ObjectPath);
			return true;
		}

		UPackage* InstancePackage = CreatePackage(*PackageName);
		if (!InstancePackage)
		{
			OutError = FString::Printf(TEXT("Failed to create package '%s'."), *PackageName);
			return false;
		}

		if (bTransient)
		{
			InstancePackage->SetPackageFlags(PKG_NewlyCreated);
		}

		// Material instances don't support undo/redo (the shader map desyncs), so no RF_Transactional.
		OutInstance = NewObject<UDreamShaderMaterialInstance>(InstancePackage, FName(*AssetName), RF_Public | RF_Standalone);
		if (!OutInstance)
		{
			OutError = FString::Printf(TEXT("Failed to create instance material '%s'."), *ObjectPath);
			return false;
		}

		// Self-limiting rather than redundant here: UDreamShaderMaterialInstance::IsAsset() is false
		// while the package is PKG_NewlyCreated and Show In-Memory Materials is off, so a hidden
		// instance publishes neither itself nor its folder. Both appear together when the toggle flips
		// and re-runs AssetCreated (FDreamShaderEditorBridge::ToggleShowInMemoryMaterialsInContentBrowser).
		PublishGeneratedAsset(OutInstance, PackageName, bTransient);
		return true;
	}

	bool CreateOrReuseMaterialFunction(const FTextShaderMaterialFunctionDefinition& Definition, UMaterialFunction*& OutFunction, FString& OutError, const bool bTransient)
	{
		FString PackageName;
		FString ObjectPath;
		FString AssetName;
		if (!ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, ObjectPath, AssetName, OutError))
		{
			return false;
		}

		UClass* ExpectedClass = UMaterialFunction::StaticClass();
		const TCHAR* ExpectedKindText = UE::DreamShader::LexToString(Definition.Kind);
		if (Definition.Kind == ETextShaderMaterialFunctionKind::MaterialLayer)
		{
			ExpectedClass = UMaterialFunctionMaterialLayer::StaticClass();
		}
		else if (Definition.Kind == ETextShaderMaterialFunctionKind::MaterialLayerBlend)
		{
			ExpectedClass = UMaterialFunctionMaterialLayerBlend::StaticClass();
		}

		if (UObject* ExistingObject = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			const bool bClassMatches = Definition.Kind == ETextShaderMaterialFunctionKind::ShaderFunction
				? ExistingObject->GetClass() == ExpectedClass
				: ExistingObject->IsA(ExpectedClass);
			if (!bClassMatches)
			{
				OutError = FString::Printf(
					TEXT("Asset '%s' already exists as '%s', but %s generation requires '%s'. Delete or move the existing asset and regenerate it."),
					*ObjectPath,
					*ExistingObject->GetClass()->GetName(),
					ExpectedKindText,
					*ExpectedClass->GetName());
				return false;
			}

			OutFunction = Cast<UMaterialFunction>(ExistingObject);
			if (!OutFunction)
			{
				OutError = FString::Printf(TEXT("Asset '%s' already exists and is not a MaterialFunction asset."), *ObjectPath);
				return false;
			}

			// Ownership guard: never clear + overwrite a saved function asset DreamShader did not
			// generate (see CreateOrReuseMaterial for the rationale).
			if (FPackageName::DoesPackageExist(PackageName) && !HasDreamShaderSourceMetadata(ExistingObject))
			{
				OutError = FString::Printf(
					TEXT("Asset '%s' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating."),
					*ObjectPath);
				return false;
			}

			WarnIfInMemoryModeShadowedByDiskAsset(bTransient, PackageName, ObjectPath);
			return true;
		}

		UPackage* FunctionPackage = CreatePackage(*PackageName);
		if (!FunctionPackage)
		{
			OutError = FString::Printf(TEXT("Failed to create package '%s'."), *PackageName);
			return false;
		}

		const EObjectFlags ObjectFlags = RF_Public | RF_Standalone;

		if (bTransient)
		{
			FunctionPackage->SetPackageFlags(PKG_NewlyCreated);
		}

		OutFunction = Cast<UMaterialFunction>(NewObject<UObject>(
			FunctionPackage,
			ExpectedClass,
			FName(*AssetName),
			ObjectFlags));

		if (!OutFunction)
		{
			OutError = FString::Printf(TEXT("Failed to create material function '%s'."), *ObjectPath);
			return false;
		}

		// An in-memory function whose Name points at a not-yet-existing folder is otherwise unreachable
		// from the Content Browser, same as a material.
		PublishGeneratedAsset(OutFunction, PackageName, bTransient);
		return true;
	}
}
