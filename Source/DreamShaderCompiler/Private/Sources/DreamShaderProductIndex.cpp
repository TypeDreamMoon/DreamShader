// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderProductIndex.h.

#include "DreamShaderProductIndex.h"

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderDefineResolution.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "DreamShaderPreprocessor.h"
#include "DreamShaderSourceFileUtils.h"
#include "Emitter/DreamShaderIRAssets.h"
#include "Lang/LangParser.h"
#include "Pipeline/DreamShaderCompilePipelineInternal.h"

#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShader.ProductIndex"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamShaderProductIndexDetail
	{
		bool IsDreamShaderIndexedSource(const FString& Path)
		{
			const Lang::ELangFileKind Kind = Lang::GetLangFileKindFromPath(Path);
			return Kind == Lang::ELangFileKind::Dss || Kind == Lang::ELangFileKind::Dsi;
		}

		/** A Parent written as a path rather than as a bare product name. */
		bool IsDreamShaderParentPathSpelling(const FString& Reference)
		{
			return Reference.StartsWith(TEXT("/"))
				|| Reference.StartsWith(TEXT("Path("), ESearchCase::IgnoreCase)
				|| Reference.EndsWith(TEXT("'"));
		}

		/** `/Game/X/M.M` -> `/Game/X/M`; a package name is returned as it is. */
		FString GetDreamShaderPackagePart(const FString& Path)
		{
			FString Package;
			return Path.Split(TEXT("."), &Package, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Package : Path;
		}

		/** `/Game/X/M.M` -> `M`. */
		FString GetDreamShaderObjectName(const FString& ObjectPath)
		{
			FString ObjectName;
			if (ObjectPath.Split(TEXT("."), nullptr, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				return ObjectName;
			}
			return FPaths::GetBaseFilename(ObjectPath);
		}

		const Lang::FPragmaDecl* FindDreamShaderInstancePragma(const Lang::FModule& Module)
		{
			for (const Lang::FDeclPtr& Decl : Module.Declarations)
			{
				if (Decl && Decl->Kind == Lang::ENodeKind::PragmaDecl)
				{
					const Lang::FPragmaDecl& Pragma = static_cast<const Lang::FPragmaDecl&>(*Decl);
					if (Pragma.PragmaKind == Lang::EPragmaKind::Instance)
					{
						return &Pragma;
					}
				}
			}
			return nullptr;
		}

		/** A `.dsi`, parse only: its one product, named by the binder's rule (BuildInstanceProduct), and its Parent as written. */
		void IndexDreamShaderInstanceSource(const FString& SourceFilePath, TArray<FDreamShaderProductRecord>& OutRecords)
		{
			FString RawText;
			if (!FFileHelper::LoadFileToString(RawText, *SourceFilePath))
			{
				return;
			}

			const UE::DreamShader::FDreamShaderDefineTable Defines = UE::DreamShader::ResolveDreamShaderDefines();
			UE::DreamShader::FDreamShaderPreprocessResult Preprocessed;
			UE::DreamShader::FDreamShaderTextError PreprocessError;
			if (!UE::DreamShader::PreprocessDreamShaderSource(RawText, SourceFilePath, Defines, Preprocessed, PreprocessError, GetDreamShaderPreprocessDialectForFile(SourceFilePath)))
			{
				return;
			}

			const Lang::FLangSourceText Text(SourceFilePath, Preprocessed.Text);
			const Lang::FLangParseResult Parsed = Lang::ParseDreamShaderLang(Text);
			if (!Parsed.Module.IsValid())
			{
				return;
			}

			const Lang::FPragmaDecl* Pragma = FindDreamShaderInstancePragma(*Parsed.Module);

			IR::FIRProduct Product;
			Product.Kind = IR::EIRProductKind::MaterialInstance;
			Product.Name = FPaths::GetBaseFilename(SourceFilePath);
			if (Pragma)
			{
				// The `.dss` split rule, as the binder applies it to a `.dsi`: a full path overrides the destination, a bare
				// name replaces the leaf and lets the root rules place it.
				if (const Lang::FDocDirective* NameDirective = Pragma->Doc.Find(TEXT("name")))
				{
					const FString AssetName = NameDirective->Value.TrimStartAndEnd();
					if (AssetName.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
					{
						Product.AssetPathOverride = AssetName;
						int32 Slash = INDEX_NONE;
						if (AssetName.FindLastChar(TEXT('/'), Slash) && Slash + 1 < AssetName.Len())
						{
							Product.Name = AssetName.RightChop(Slash + 1);
						}
					}
					else if (!AssetName.IsEmpty())
					{
						Product.Name = AssetName;
					}
				}
			}

			FString PackageName;
			FString ObjectPath;
			FString LeafName;
			FDreamShaderError DestinationError;
			if (!ResolveIRProductObjectPath(Product, SourceFilePath, PackageName, ObjectPath, LeafName, DestinationError))
			{
				return;
			}

			FDreamShaderProductRecord& Record = OutRecords.AddDefaulted_GetRef();
			Record.SourceFilePath = SourceFilePath;
			Record.ProductName = GetDreamShaderObjectName(ObjectPath);
			Record.ObjectPath = ObjectPath;
			Record.Kind = IR::EIRProductKind::MaterialInstance;
			if (const Lang::FPragmaArgument* Parent = Pragma ? Pragma->Find(TEXT("Parent")) : nullptr)
			{
				Record.ParentReference = Parent->Value.TrimStartAndEnd();
			}
		}

		/** A `.dss`: its products as product resolution answers them, which is what a build writes. */
		void IndexDreamShaderLang2Source(const FString& SourceFilePath, TArray<FDreamShaderProductRecord>& OutRecords)
		{
			FDreamShaderProductResolution Resolution;
			ResolveDreamShaderSourceProducts(SourceFilePath, Resolution);
			for (const FDreamShaderResolvedProduct& Product : Resolution.Products)
			{
				if (Product.ObjectPath.IsEmpty())
				{
					continue;
				}
				FDreamShaderProductRecord& Record = OutRecords.AddDefaulted_GetRef();
				Record.SourceFilePath = Resolution.SourceFilePath;
				Record.ProductName = GetDreamShaderObjectName(Product.ObjectPath);
				Record.ObjectPath = Product.ObjectPath;
				Record.Kind = Product.Kind;
			}
		}

		bool CanDreamShaderProductBeInstanced(const IR::EIRProductKind Kind)
		{
			switch (Kind)
			{
			case IR::EIRProductKind::Material:
			case IR::EIRProductKind::MaterialInstance:
				return true;
			case IR::EIRProductKind::MaterialFunction:
			case IR::EIRProductKind::MaterialLayer:
			case IR::EIRProductKind::MaterialLayerBlend:
				return false;
			}
			return false;
		}

		/** The parent of an instance record, with no diagnostics: the dependency edges and the dependents query. */
		const FDreamShaderProductRecord* ResolveDreamShaderInstanceParentQuietly(const FDreamShaderProductIndex& Index, const FDreamShaderProductRecord& Instance)
		{
			if (Instance.ParentReference.IsEmpty())
			{
				return nullptr;
			}

			if (IsDreamShaderParentPathSpelling(Instance.ParentReference))
			{
				FString ObjectPath;
				FDreamShaderError Error;
				if (!Private::TryResolveDreamShaderAssetReference(Instance.ParentReference, ObjectPath, Error, UMaterialInterface::StaticClass()))
				{
					return nullptr;
				}
				const FDreamShaderProductRecord* Parent = Index.FindByObjectPath(ObjectPath);
				return Parent != &Instance ? Parent : nullptr;
			}

			const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(Instance.SourceFilePath);
			TArray<const FDreamShaderProductRecord*> Matches;
			Index.FindByName(Root ? Root->Directory : FString(), Instance.ParentReference, Matches);
			Matches.RemoveAll([&Instance](const FDreamShaderProductRecord* Candidate)
			{
				return Candidate == &Instance || !CanDreamShaderProductBeInstanced(Candidate->Kind);
			});
			return Matches.Num() == 1 ? Matches[0] : nullptr;
		}
	}

	// --------------------------------------------------------------------------------------- the index

	FDreamShaderProductIndex& FDreamShaderProductIndex::Get()
	{
		static FDreamShaderProductIndex Index;
		return Index;
	}

	void FDreamShaderProductIndex::Refresh()
	{
		using namespace DreamShaderProductIndexDetail;

		// Indexing a `.dss` runs product resolution, which never reaches the index again; the guard is for a caller
		// that refreshes from inside a refresh all the same.
		if (bRefreshing)
		{
			return;
		}
		TGuardValue<bool> RefreshGuard(bRefreshing, true);

		TArray<FString> Sources;
		Private::FDreamShaderSourceFileUtils::FindProjectMaterialSourceFiles(Sources);

		TSet<FString> Seen;
		for (const FString& Source : Sources)
		{
			if (!IsDreamShaderIndexedSource(Source))
			{
				continue;
			}
			Seen.Add(Source);

			const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*Source);
			const FIndexedFile* Existing = Files.Find(Source);
			if (Existing && Existing->Timestamp == Timestamp)
			{
				continue;
			}

			FIndexedFile& Entry = Files.FindOrAdd(Source);
			Entry.Timestamp = Timestamp;
			Entry.Records.Reset();
			if (Lang::GetLangFileKindFromPath(Source) == Lang::ELangFileKind::Dsi)
			{
				IndexDreamShaderInstanceSource(Source, Entry.Records);
			}
			else
			{
				IndexDreamShaderLang2Source(Source, Entry.Records);
			}
		}

		for (TMap<FString, FIndexedFile>::TIterator It = Files.CreateIterator(); It; ++It)
		{
			if (!Seen.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}
	}

	const FDreamShaderProductRecord* FDreamShaderProductIndex::FindByObjectPath(const FString& ObjectPath) const
	{
		using namespace DreamShaderProductIndexDetail;

		const FString Wanted = ObjectPath.TrimStartAndEnd();
		const bool bWantedIsPackage = !Wanted.Contains(TEXT("."));
		for (const TPair<FString, FIndexedFile>& Pair : Files)
		{
			for (const FDreamShaderProductRecord& Record : Pair.Value.Records)
			{
				if (Record.ObjectPath.Equals(Wanted, ESearchCase::IgnoreCase)
					|| (bWantedIsPackage && GetDreamShaderPackagePart(Record.ObjectPath).Equals(Wanted, ESearchCase::IgnoreCase)))
				{
					return &Record;
				}
			}
		}
		return nullptr;
	}

	void FDreamShaderProductIndex::FindByName(const FString& RootDirectory, const FString& ProductName, TArray<const FDreamShaderProductRecord*>& OutRecords) const
	{
		OutRecords.Reset();
		for (const TPair<FString, FIndexedFile>& Pair : Files)
		{
			if (!RootDirectory.IsEmpty() && !UE::DreamShader::IsPathUnderSourceDirectory(Pair.Key, RootDirectory))
			{
				continue;
			}
			for (const FDreamShaderProductRecord& Record : Pair.Value.Records)
			{
				if (Record.ProductName.Equals(ProductName, ESearchCase::CaseSensitive))
				{
					OutRecords.Add(&Record);
				}
			}
		}
	}

	void FDreamShaderProductIndex::FindBySource(const FString& SourceFilePath, TArray<const FDreamShaderProductRecord*>& OutRecords) const
	{
		OutRecords.Reset();
		if (const FIndexedFile* File = Files.Find(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath)))
		{
			for (const FDreamShaderProductRecord& Record : File->Records)
			{
				OutRecords.Add(&Record);
			}
		}
	}

	void FDreamShaderProductIndex::FindInstancesOfSource(const FString& ParentSourceFile, TArray<const FDreamShaderProductRecord*>& OutRecords) const
	{
		using namespace DreamShaderProductIndexDetail;

		OutRecords.Reset();
		const FString NormalizedParentSource = UE::DreamShader::NormalizeSourceFilePath(ParentSourceFile);
		for (const TPair<FString, FIndexedFile>& Pair : Files)
		{
			for (const FDreamShaderProductRecord& Record : Pair.Value.Records)
			{
				if (Record.Kind != IR::EIRProductKind::MaterialInstance)
				{
					continue;
				}
				const FDreamShaderProductRecord* Parent = ResolveDreamShaderInstanceParentQuietly(*this, Record);
				if (Parent && Parent->SourceFilePath.Equals(NormalizedParentSource, ESearchCase::IgnoreCase))
				{
					OutRecords.Add(&Record);
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------- resolution

	bool ResolveInstanceParent(
		const FString& InstanceSourceFile,
		const FString& ParentReference,
		const Lang::FLangSpan& ParentSpan,
		FString& OutObjectPath,
		FString& OutParentSourceFile,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamShaderProductIndexDetail;

		OutObjectPath.Reset();
		OutParentSourceFile.Reset();

		const FString Reference = ParentReference.TrimStartAndEnd();
		const FString InstanceSource = UE::DreamShader::NormalizeSourceFilePath(InstanceSourceFile);

		FDreamShaderProductIndex& Index = FDreamShaderProductIndex::Get();
		Index.Refresh();

		TArray<const FDreamShaderProductRecord*> OwnRecords;
		Index.FindBySource(InstanceSource, OwnRecords);
		const auto IsOwnProduct = [&OwnRecords](const FString& ObjectPath)
		{
			return OwnRecords.ContainsByPredicate([&ObjectPath](const FDreamShaderProductRecord* Record)
			{
				return Record->ObjectPath.Equals(ObjectPath, ESearchCase::IgnoreCase);
			});
		};

		if (IsDreamShaderParentPathSpelling(Reference))
		{
			FString ObjectPath;
			FDreamShaderError ReferenceError;
			if (!Private::TryResolveDreamShaderAssetReference(Reference, ObjectPath, ReferenceError, UMaterialInterface::StaticClass()))
			{
				Diagnostics.Error(TEXT("DSH8260"), ParentSpan, FText::Format(
					LOCTEXT("ParentReferenceUnresolved", "The parent '{0}' does not resolve to an asset path. {1}"),
					FText::FromString(Reference),
					FText::FromString(ReferenceError.HasCode()
						? FString::Printf(TEXT("%s: %s"), *ReferenceError.Code, *ReferenceError.Message) /* I18N-EXEMPT: quotes an asset-layer message verbatim */
						: ReferenceError.Message)));
				return false;
			}

			if (IsOwnProduct(ObjectPath))
			{
				Diagnostics.Error(TEXT("DSH8263"), ParentSpan, FText::Format(
					LOCTEXT("ParentIsSelf", "'{0}' names this instance itself as its parent; an instance needs a different material to instance."),
					FText::FromString(Reference)));
				return false;
			}

			if (const FDreamShaderProductRecord* Record = Index.FindByObjectPath(ObjectPath))
			{
				OutObjectPath = Record->ObjectPath;
				OutParentSourceFile = Record->SourceFilePath;
				return true;
			}

			// Foreign: no source under the roots builds it, so it has to exist already -- in memory or on disk.
			const bool bExists = FindObject<UMaterialInterface>(nullptr, *ObjectPath) != nullptr
				|| FPackageName::DoesPackageExist(GetDreamShaderPackagePart(ObjectPath));
			if (!bExists)
			{
				Diagnostics.Error(TEXT("DSH8260"), ParentSpan, FText::Format(
					LOCTEXT("ParentMissing", "The parent '{0}' names no material: nothing exists at '{1}', and no DreamShader source under the source roots builds it."),
					FText::FromString(Reference),
					FText::FromString(ObjectPath)));
				return false;
			}

			OutObjectPath = ObjectPath;
			return true;
		}

		// A bare product name: the products built under the instance's own source root.
		const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(InstanceSource);
		const FString RootDirectory = Root ? Root->Directory : FString();

		TArray<const FDreamShaderProductRecord*> Matches;
		Index.FindByName(RootDirectory, Reference, Matches);
		const int32 SelfMatches = Matches.RemoveAll([&InstanceSource](const FDreamShaderProductRecord* Candidate)
		{
			return Candidate->SourceFilePath.Equals(InstanceSource, ESearchCase::IgnoreCase);
		});
		Matches.RemoveAll([](const FDreamShaderProductRecord* Candidate)
		{
			return !CanDreamShaderProductBeInstanced(Candidate->Kind);
		});

		if (Matches.Num() == 0)
		{
			if (SelfMatches > 0)
			{
				Diagnostics.Error(TEXT("DSH8263"), ParentSpan, FText::Format(
					LOCTEXT("ParentNameIsSelf", "'{0}' is the name of this instance itself; an instance needs a different material to instance."),
					FText::FromString(Reference)));
				return false;
			}

			Diagnostics.Error(TEXT("DSH8261"), ParentSpan, FText::Format(
				LOCTEXT("ParentNameNotFound", "No material or instance named '{0}' is built by a source under '{1}'; write the parent's asset path, or check the name."),
				FText::FromString(Reference),
				FText::FromString(RootDirectory)));
			return false;
		}

		if (Matches.Num() > 1)
		{
			TArray<FString> Sources;
			for (const FDreamShaderProductRecord* Match : Matches)
			{
				Sources.AddUnique(FPaths::GetCleanFilename(Match->SourceFilePath));
			}
			Diagnostics.Error(TEXT("DSH8262"), ParentSpan, FText::Format(
				LOCTEXT("ParentNameAmbiguous", "'{0}' names more than one product under '{1}' ({2}); write the parent's asset path instead."),
				FText::FromString(Reference),
				FText::FromString(RootDirectory),
				FText::FromString(FString::Join(Sources, TEXT(", ")))));
			return false;
		}

		OutObjectPath = Matches[0]->ObjectPath;
		OutParentSourceFile = Matches[0]->SourceFilePath;
		return true;
	}

	void CollectInstanceDependents(const FString& SourceFilePath, TArray<FString>& OutInstanceSourceFiles)
	{
		OutInstanceSourceFiles.Reset();

		FDreamShaderProductIndex& Index = FDreamShaderProductIndex::Get();
		Index.Refresh();

		const FString Start = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		TSet<FString> Visited;
		Visited.Add(Start);

		TArray<FString> Pending;
		Pending.Add(Start);
		while (Pending.Num() > 0)
		{
			const FString Current = Pending.Pop(DREAMSHADER_ALLOW_SHRINKING_NO);

			TArray<const FDreamShaderProductRecord*> Instances;
			Index.FindInstancesOfSource(Current, Instances);
			for (const FDreamShaderProductRecord* Instance : Instances)
			{
				if (Visited.Contains(Instance->SourceFilePath))
				{
					continue;
				}
				Visited.Add(Instance->SourceFilePath);
				OutInstanceSourceFiles.Add(Instance->SourceFilePath);
				Pending.Add(Instance->SourceFilePath);
			}
		}
	}

	FString FindInstanceParentSourceFile(const FString& InstanceSourceFile)
	{
		using namespace DreamShaderProductIndexDetail;

		FDreamShaderProductIndex& Index = FDreamShaderProductIndex::Get();
		Index.Refresh();

		TArray<const FDreamShaderProductRecord*> Records;
		Index.FindBySource(InstanceSourceFile, Records);
		for (const FDreamShaderProductRecord* Record : Records)
		{
			if (Record->Kind != IR::EIRProductKind::MaterialInstance)
			{
				continue;
			}
			if (const FDreamShaderProductRecord* Parent = ResolveDreamShaderInstanceParentQuietly(Index, *Record))
			{
				return Parent->SourceFilePath;
			}
		}
		return FString();
	}
}

#undef LOCTEXT_NAMESPACE
