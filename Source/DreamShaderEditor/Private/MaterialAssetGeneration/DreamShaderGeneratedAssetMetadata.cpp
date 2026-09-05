#include "DreamShaderMaterialGeneratorPrivate.h"

#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderModule.h"
// BuildDreamShaderDefineKeyFragment: the sorted fold of the touched-define set into the build key.
#include "DreamShaderPreprocessor.h"
#include "DreamShaderSettings.h"

#include "Interfaces/IPluginManager.h"
#include "DreamShaderVersionCompat.h"

#include "FileHelpers.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		FString GetSourceMetadataValue(UObject* Asset, const TCHAR* Key)
		{
			if (!Asset)
			{
				return FString();
			}

			UPackage* Package = Asset->GetOutermost();
			if (!Package)
			{
				return FString();
			}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			return Package->GetMetaData().GetValue(Asset, Key);
#else
			if (UMetaData* MetaData = Package->GetMetaData())
			{
				return MetaData->GetValue(Asset, Key);
			}
			return FString();
#endif
		}

		void SetSourceMetadataValue(UObject* Asset, const TCHAR* Key, const FString& Value)
		{
			if (!Asset)
			{
				return;
			}

			UPackage* Package = Asset->GetOutermost();
			if (!Package)
			{
				return;
			}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			Package->GetMetaData().SetValue(Asset, Key, *Value);
#else
			if (UMetaData* MetaData = Package->GetMetaData())
			{
				MetaData->SetValue(Asset, Key, *Value);
			}
#endif
		}

		void RemoveSourceMetadataValue(UObject* Asset, const TCHAR* Key)
		{
			if (!Asset)
			{
				return;
			}

			UPackage* Package = Asset->GetOutermost();
			if (!Package)
			{
				return;
			}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
			Package->GetMetaData().RemoveValue(Asset, Key);
#else
			if (UMetaData* MetaData = Package->GetMetaData())
			{
				MetaData->RemoveValue(Asset, Key);
			}
#endif
		}

		// Project-relative, forward-slashed source path. Stamped into asset metadata so a checkout on a
		// different machine (or a moved project directory) records the same identity instead of an
		// absolute path that differs per machine. Sources outside the project (rare) stay absolute.
		FString MakeProjectRelativeSourcePath(const FString& SourceFilePath)
		{
			FString Path = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
			const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			FPaths::MakePathRelativeTo(Path, *ProjectDir);
			return Path;
		}
	}

	namespace
	{
		// Bump when a change to the generator makes previously generated assets wrong for their source.
		// A released plugin gets that for free from its version; this is what covers the window between
		// releases, where the version has not moved but the output has.
		// DSK2: component masks are now emitted in a form the material graph editor can round-trip
		// (see FCodeGraphBuilder::ApplyChannelMaskToValue). Assets stamped DSK1 can carry an inline mask
		// that the editor rewrites into a different output on the first Apply.
		// DSK3: the key now folds in the preprocessor defines the source read. A DSK2 stamp was computed
		// from post-cut text with no record of WHY it was cut, so an asset generated before this could
		// not be told apart from the same asset generated under a different define set.
		constexpr const TCHAR* BuildKeyVersion = TEXT("DSK3");

		FString GetDreamShaderPluginVersion()
		{
			if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
			{
				return Plugin->GetDescriptor().VersionName;
			}
			return TEXT("unknown"); // I18N-EXEMPT: build key material, never displayed
		}

		// The settings that change what a given source compiles INTO. Sorted, because a TMap's
		// iteration order is not something to hash.
		void AppendSettingsToBuildKey(FString& InOutKey)
		{
			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
			if (!Settings)
			{
				return;
			}

			InOutKey += FString::Printf(TEXT("Backend=%d|"), static_cast<int32>(Settings->DefaultBackend));

			auto AppendMapping = [&InOutKey](const TCHAR* Label, const auto& Mapping)
			{
				TArray<FString> Entries;
				Entries.Reserve(Mapping.Num());
				for (const auto& Pair : Mapping)
				{
					Entries.Add(FString::Printf(TEXT("%s=%d"), *Pair.Key, static_cast<int32>(Pair.Value.GetValue())));
				}
				Entries.Sort();
				InOutKey += FString::Printf(TEXT("%s=%s|"), Label, *FString::Join(Entries, TEXT(";")));
			};

			AppendMapping(TEXT("ShadingModelMappings"), Settings->ShadingModelMappings);
			AppendMapping(TEXT("BlendModeMappings"), Settings->BlendModeMappings);
			AppendMapping(TEXT("MaterialDomainMappings"), Settings->MaterialDomainMappings);
		}
	}

	// The value stamped as DreamShader.SourceHash, and the whole of what the regeneration skip compares.
	//
	// It is a hash of the source IN THE CONTEXT THAT COMPILES IT, not of the source alone. Everything
	// folded in below is an input that changes what a given source produces, and leaving any of it out
	// makes the skip check answer "still current" about an asset that is not:
	//
	//   - the prepared source text, imports already inlined (so a changed .dsh or a called .dsf is
	//     covered transitively, and needs nothing else here);
	//   - the default backend, which decides whether a Shader block becomes a UMaterial or a thin
	//     instance -- the setting whose change used to need its own forced sweep to take effect;
	//   - the mapping tables, which decide what a Settings key resolves to;
	//   - the plugin version and a hand-bumped format tag, so upgrading the generator invalidates what
	//     the old generator wrote;
	//   - the engine version, because what is generable moves with it (Substrate, for one);
	//   - the preprocessor defines the source read, which is the one input the text CANNOT carry.
	//
	// That last one needs its own paragraph, because it is the only entry here that is invisible in
	// the hashed text. SourceText arrives post-cut: the branches a `#if` discarded are already gone,
	// so flipping a define changes the text and would be caught -- but flipping it BACK produces text
	// identical to what some earlier stamp hashed, and the asset on disk was generated under whichever
	// define set was live at the time. Without the defines in the key, "the text is the same" is not
	// the same claim as "the asset is current", and the difference is a silently stale asset.
	//
	// Only the defines the preprocessor actually READ are folded in, not the whole table, and that is
	// exact rather than approximate. Preprocessing is deterministic and reads a define only to
	// evaluate a condition; the position of the k-th condition depends only on the results of the k-1
	// before it. So if every value in the touched set is unchanged, the evaluated conditions and their
	// results are unchanged, and the output text is byte-identical. A define named only inside a dead
	// branch is correctly absent: it cannot affect the output until the condition that killed the
	// branch changes, and THAT condition's defines are in the set. A name read while undefined is in
	// the set too, under a sentinel value -- otherwise defining it later would not move the hash.
	//
	// Changing the composition invalidates every existing stamp, which costs one rebuild per asset and
	// is exactly the intended effect.
	FString BuildSourceHash(const FString& SourceText, const UE::DreamShader::FDreamShaderDefineValueMap& TouchedDefines)
	{
		FString BuildKey = FString::Printf( // I18N-EXEMPT: build key material, never displayed
			TEXT("%s|Plugin=%s|Engine=%d.%d|"),
			BuildKeyVersion,
			*GetDreamShaderPluginVersion(),
			DREAMSHADER_UE_MAJOR,
			DREAMSHADER_UE_MINOR);
		AppendSettingsToBuildKey(BuildKey);
		// Sorted by the fragment builder, for the same reason AppendSettingsToBuildKey sorts: TMap
		// iteration order is not stable, and an unsorted fold would hash the same inputs differently
		// from one run to the next -- which reads as "every asset is stale, every time".
		BuildKey += FString::Printf( // I18N-EXEMPT: build key material, never displayed
			TEXT("Defines=%s|"),
			*UE::DreamShader::BuildDreamShaderDefineKeyFragment(TouchedDefines));
		// A separator the source text cannot supply on its own, so no source can spell a prefix that
		// collides with a different context's.
		BuildKey += TEXT("\n--\n");
		BuildKey += SourceText;

		return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*BuildKey)); /* I18N-EXEMPT: deferred codegen or compatibility path */
	}

	bool IsGeneratedAssetSourceCurrent(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash)
	{
		if (!Asset || SourceHash.IsEmpty())
		{
			return false;
		}

		const FString ExistingSourceFile = GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceFile"));
		if (ExistingSourceFile.IsEmpty())
		{
			return false;
		}

		const FString ExistingSourceHash = GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceHash"));

		// Compare the project-relative source path (as stamped by ApplySourceMetadata): a different
		// machine or a moved project recognizes its own generated assets instead of regenerating them.
		// The content hash is the primary identity; the path disambiguates two sources that hash alike.
		return ExistingSourceFile.Equals(MakeProjectRelativeSourcePath(SourceFilePath), ESearchCase::IgnoreCase)
			&& ExistingSourceHash.Equals(SourceHash, ESearchCase::CaseSensitive);
	}

	bool HasDreamShaderSourceMetadata(UObject* Asset)
	{
		// DreamShader stamps DreamShader.SourceFile on every asset it generates; its presence is the
		// ownership marker used to decide whether an existing asset is safe to regenerate over.
		return !GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceFile")).IsEmpty();
	}

	FString GetGeneratedAssetSourceFile(UObject* Asset)
	{
		return GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceFile"));
	}

	FString GetOutputDigestMetadata(UObject* Asset)
	{
		return GetSourceMetadataValue(Asset, TEXT("DreamShader.OutputDigest"));
	}

	FString GetGeneratedAssetSourceHash(UObject* Asset)
	{
		return GetSourceMetadataValue(Asset, TEXT("DreamShader.SourceHash"));
	}

	void ApplyOutputDigestMetadata(UObject* Asset)
	{
		const FString Digest = BuildOutputDigest(Asset);
		if (Digest.IsEmpty())
		{
			// An asset class the digest does not cover. Leave any previous stamp alone rather than
			// writing an empty one: a stale stamp is still readable as "schema mismatch", whereas an
			// empty one is indistinguishable from never having been stamped.
			return;
		}

		SetSourceMetadataValue(Asset, TEXT("DreamShader.OutputDigest"), Digest);
	}

	EDreamShaderDigestState ClassifyGeneratedAsset(UObject* Asset)
	{
		if (!HasDreamShaderSourceMetadata(Asset))
		{
			return EDreamShaderDigestState::Foreign;
		}

		const FString StampedDigest = GetOutputDigestMetadata(Asset);
		if (StampedDigest.IsEmpty())
		{
			return EDreamShaderDigestState::Unstamped;
		}

		// Schema first: a stamp written by a different digest format or a different engine cannot be
		// compared against one written now, and reporting the difference as divergence would flag
		// every generated asset in the project the moment either moves.
		const FString SchemaTag = MakeDigestSchemaTag();
		if (!StampedDigest.StartsWith(SchemaTag + TEXT(":"), ESearchCase::CaseSensitive))
		{
			return EDreamShaderDigestState::Unstamped;
		}

		const FString CurrentDigest = BuildOutputDigest(Asset);
		if (CurrentDigest.IsEmpty())
		{
			return EDreamShaderDigestState::Unstamped;
		}

		return CurrentDigest.Equals(StampedDigest, ESearchCase::CaseSensitive)
			? EDreamShaderDigestState::Generated
			: EDreamShaderDigestState::Diverged;
	}

	void ClearDreamShaderMetadata(UObject* Asset)
	{
		RemoveSourceMetadataValue(Asset, TEXT("DreamShader.SourceFile"));
		RemoveSourceMetadataValue(Asset, TEXT("DreamShader.SourceHash"));
		RemoveSourceMetadataValue(Asset, TEXT("DreamShader.OutputDigest"));
	}

	namespace
	{
		// Deliberately not a bForce parameter. bForce means "ignore the source hash", and the editor's
		// own startup sweep passes it on every file -- so honouring it here would leave the gate dead
		// in the one mode that matters most. Overwriting a hand edit is a decision only a person can
		// make, and this is how that decision reaches the generator without threading a second flag
		// through every compile entry point.
		int32 GRevertDivergedAssetsDepth = 0;
	}

	FScopedDreamShaderRevertDiverged::FScopedDreamShaderRevertDiverged()
	{
		++GRevertDivergedAssetsDepth;
	}

	FScopedDreamShaderRevertDiverged::~FScopedDreamShaderRevertDiverged()
	{
		--GRevertDivergedAssetsDepth;
	}

	bool IsRevertingDivergedAssets()
	{
		return GRevertDivergedAssetsDepth > 0;
	}

	bool CheckGeneratedAssetNotDiverged(UObject* Asset, FDreamShaderError& OutError)
	{
		if (IsRevertingDivergedAssets() || ClassifyGeneratedAsset(Asset) != EDreamShaderDigestState::Diverged)
		{
			return true;
		}

		const FString SourceFile = GetGeneratedAssetSourceFile(Asset);
		return FailWith(OutError, TEXT("DSH8115"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */ TEXT("Asset '%s' has been edited by hand since DreamShader generated it from '%s', so it was NOT rebuilt (rebuilding would destroy those edits). ") TEXT("Right-click the asset > DreamShader and choose one: 'Revert to Source' discards the edits and rebuilds, ") TEXT("'Adopt Into Source' rewrites '%s' from the edited asset, ") TEXT("'Detach From DreamShader' hands the asset over to you and stops managing it."), *Asset->GetPathName(), *SourceFile, *SourceFile));
	}

	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath)
	{
		ApplySourceMetadata(Asset, SourceFilePath, FString());
	}

	void ApplySourceMetadata(UObject* Asset, const FString& SourceFilePath, const FString& SourceHash)
	{
		if (!Asset)
		{
			return;
		}

		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			return;
		}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
		FMetaData& MetaData = Package->GetMetaData();
		MetaData.SetValue(Asset, TEXT("DreamShader.SourceFile"), *MakeProjectRelativeSourcePath(SourceFilePath));
		if (!SourceHash.IsEmpty())
		{
			MetaData.SetValue(Asset, TEXT("DreamShader.SourceHash"), *SourceHash);
		}
#else
		UMetaData* MetaData = Package->GetMetaData();
		if (!MetaData)
		{
			return;
		}
		MetaData->SetValue(Asset, TEXT("DreamShader.SourceFile"), *MakeProjectRelativeSourcePath(SourceFilePath));
		if (!SourceHash.IsEmpty())
		{
			MetaData->SetValue(Asset, TEXT("DreamShader.SourceHash"), *SourceHash);
		}
#endif
	}

	bool SaveAssetPackage(UObject* Asset, FDreamShaderError& OutError)
	{
		check(Asset);

		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(Asset->GetOutermost());
		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			return FailWith(OutError, TEXT("DSH8116"), FString::Printf(TEXT("Generated DreamShader asset '%s' could not be saved."), *Asset->GetPathName())); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		return true;
	}

	bool SaveAssetPackages(const TArray<UObject*>& Assets, FDreamShaderError& OutError)
	{
		// One SavePackages call for a dependent asset PAIR (e.g. a ThinCustom base material and the
		// instance parented to it): SavePackages saves exactly the packages passed in -- it does NOT
		// gather referenced (parent) packages -- so both must be listed explicitly, and both must be
		// dirty (bOnlyDirty=true).
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Reserve(Assets.Num());
		for (UObject* Asset : Assets)
		{
			check(Asset);
			PackagesToSave.AddUnique(Asset->GetOutermost());
		}

		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			FString FailedAssetList;
			for (UObject* Asset : Assets)
			{
				FailedAssetList += FString::Printf(TEXT(" '%s'"), *Asset->GetPathName()); /* I18N-EXEMPT: deferred codegen or compatibility path */
			}
			return FailWith(OutError, TEXT("DSH8117"), FString::Printf(TEXT("Generated DreamShader asset packages could not be saved.%s"), *FailedAssetList)); /* I18N-EXEMPT: deferred codegen or compatibility path */
		}

		return true;
	}
}
