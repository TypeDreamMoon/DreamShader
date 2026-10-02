// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderPipelineReferences.h.
//
// Diagnostics owned by this file: DSH8330-DSH8334 (the `.dsp` reference stage) and DSH5315-DSH5326 (the `.dss`
// checks of UE.DreamPassBuffer that need the engine: the pipeline, its buffer and its export).

#include "Pipeline/DreamShaderPipelineReferences.h"

#include "DreamShaderBuiltinCatalog.h"
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerServiceInternal.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "DreamShaderProductIndex.h"
#include "Emitter/DreamShaderIRAssets.h"
#include "Pass/DreamShaderPassShaderText.h"
#include "Pipeline/DreamShaderCompilePipelineInternal.h"

#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "Materials/MaterialExpressionDreamPassBuffer.h"
#include "Materials/MaterialExpressionDreamPassOutput.h"

#include "Lang/LangPipelineSource.h"

#include "Engine/BlendableInterface.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if DREAMSHADER_WITH_CUSTOM_PASS
#include "Materials/MaterialExpressionUserSceneTexture.h"
#endif

#define LOCTEXT_NAMESPACE "DreamShader.Pipeline.References"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamShaderPipelineReferencesDetail
	{
		/** The sources whose cross references are being resolved right now, outermost first: the cycle guard. */
		TArray<FString>& GetCrossReferenceChain()
		{
			static TArray<FString> Chain;
			return Chain;
		}

		bool IsInCrossReferenceChain(const FString& SourceFile)
		{
			return GetCrossReferenceChain().ContainsByPredicate([&SourceFile](const FString& Link)
			{
				return Link.Equals(SourceFile, ESearchCase::IgnoreCase);
			});
		}

		struct FScopedCrossReference
		{
			explicit FScopedCrossReference(const FString& SourceFile)
			{
				GetCrossReferenceChain().Add(SourceFile);
			}
			~FScopedCrossReference()
			{
				GetCrossReferenceChain().Pop();
			}
			FScopedCrossReference(const FScopedCrossReference&) = delete;
			FScopedCrossReference& operator=(const FScopedCrossReference&) = delete;
		};

		/** The span of `"<Reference>"` in the text, so a message about a reference lands on it; empty when it is not found. */
		Lang::FLangSpan FindReferenceSpan(const Lang::FLangSourceText& Source, const FString& Reference)
		{
			const FString Quoted = FString::Printf(TEXT("\"%s\""), *Reference);
			const int32 Offset = Source.GetText().Find(Quoted, ESearchCase::CaseSensitive);
			return Offset == INDEX_NONE ? Lang::FLangSpan() : Source.MakeSpan(Offset, Quoted.Len());
		}

		/** A reference written as a path rather than as a bare product name -- the `.dsi` Parent rule. */
		bool IsPathSpelling(const FString& Reference)
		{
			return Reference.StartsWith(TEXT("/"))
				|| Reference.StartsWith(TEXT("Path("), ESearchCase::IgnoreCase)
				|| Reference.EndsWith(TEXT("'"));
		}

		UObject* FindOrLoadAsset(const FString& ObjectPath)
		{
			UObject* Asset = FindObject<UObject>(nullptr, *ObjectPath);
			if (!Asset && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ObjectPath)))
			{
				Asset = LoadObject<UObject>(nullptr, *ObjectPath);
			}
			return Asset;
		}

		/** `MD_PostProcess` -> `PostProcess`. */
		FString EnumNameWithoutPrefix(const UEnum* Enum, const int64 Value, const TCHAR* Prefix)
		{
			if (!Enum)
			{
				return FString();
			}
			FString Name = Enum->GetNameStringByValue(Value);
			Name.RemoveFromStart(Prefix);
			return Name;
		}

		enum class EReferenceOutcome : uint8
		{
			Found,
			/** A bare name nothing under the root builds. */
			NotFound,
			/** A bare name several products share. */
			Ambiguous,
			/** A path that does not parse. */
			Malformed,
		};

		struct FResolvedReference
		{
			FString ObjectPath;
			/** The DreamShader source that builds it; empty for an asset no source under the roots builds. */
			FString SourceFile;
			TArray<FString> AmbiguousSources;
			FString PathError;
		};

		/** A material or pipeline reference, by the `.dsi` Parent rule: a path is an asset path, a bare name a product under the referrer's root. */
		EReferenceOutcome ResolveProductReference(
			const FString& InReference,
			const FString& ReferringSource,
			const TFunctionRef<bool(IR::EIRProductKind)> AcceptsKind,
			const UClass* ExpectedClass,
			FResolvedReference& Out)
		{
			const FString Reference = InReference.TrimStartAndEnd();
			FDreamShaderProductIndex& Index = FDreamShaderProductIndex::Get();
			Index.Refresh();

			if (IsPathSpelling(Reference))
			{
				FDreamShaderError Error;
				if (!Private::TryResolveDreamShaderAssetReference(Reference, Out.ObjectPath, Error, ExpectedClass))
				{
					Out.PathError = Error.HasCode()
						? FString::Printf(TEXT("%s: %s"), *Error.Code, *Error.Message) /* I18N-EXEMPT: quotes an asset-layer message verbatim */
						: Error.Message;
					return EReferenceOutcome::Malformed;
				}
				if (const FDreamShaderProductRecord* Record = Index.FindByObjectPath(Out.ObjectPath))
				{
					if (AcceptsKind(Record->Kind))
					{
						Out.ObjectPath = Record->ObjectPath;
						Out.SourceFile = Record->SourceFilePath;
					}
				}
				return EReferenceOutcome::Found;
			}

			const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(ReferringSource);
			TArray<const FDreamShaderProductRecord*> Matches;
			Index.FindByName(Root ? Root->Directory : FString(), Reference, Matches);
			Matches.RemoveAll([&ReferringSource, &AcceptsKind](const FDreamShaderProductRecord* Candidate)
			{
				return !AcceptsKind(Candidate->Kind) || Candidate->SourceFilePath.Equals(ReferringSource, ESearchCase::IgnoreCase);
			});

			if (Matches.Num() == 0)
			{
				return EReferenceOutcome::NotFound;
			}
			if (Matches.Num() > 1)
			{
				for (const FDreamShaderProductRecord* Match : Matches)
				{
					Out.AmbiguousSources.AddUnique(FPaths::GetCleanFilename(Match->SourceFilePath));
				}
				return EReferenceOutcome::Ambiguous;
			}

			Out.ObjectPath = Matches[0]->ObjectPath;
			Out.SourceFile = Matches[0]->SourceFilePath;
			return EReferenceOutcome::Found;
		}

		bool IsMaterialProductKind(const IR::EIRProductKind Kind)
		{
			return Kind == IR::EIRProductKind::Material || Kind == IR::EIRProductKind::MaterialInstance;
		}

		bool IsPipelineProductKind(const IR::EIRProductKind Kind)
		{
			return Kind == IR::EIRProductKind::PassPipeline;
		}

		/**
		 * The settings half of a material's facts, off a UMaterial (the asset's own base material, or a scratch one the
		 * source's settings went onto), with the usage flags asked of UsageSource -- the material or instance the pass names.
		 */
		void ReadMaterialSettingFacts(const UMaterial& Material, const UMaterialInterface& UsageSource, Lang::FPipelineMaterialInfo& Info)
		{
			Info.Domain = EnumNameWithoutPrefix(StaticEnum<EMaterialDomain>(), static_cast<int64>(Material.MaterialDomain.GetValue()), TEXT("MD_"));
			Info.BlendableLocation = EnumNameWithoutPrefix(StaticEnum<EBlendableLocation>(), static_cast<int64>(Material.BlendableLocation.GetValue()), TEXT("BL_"));

#if DREAMSHADER_WITH_CUSTOM_PASS
			Info.bDisablePreExposureScale = Material.bDisablePreExposureScale != 0;
			Info.bUsesNewTranslator = Material.bEnableNewHLSLGenerator != 0;

			// EDreamPassMeshUsageFlags spellings. Landscape has no usage flag: the landscape draws whatever material it is
			// given, so it is never what a pass's `Usage` waits for.
			struct FUsageSpelling
			{
				EMaterialUsage Usage;
				const TCHAR* Name;
			};
			static const FUsageSpelling Usages[] =
			{
				{ MATUSAGE_StaticMesh,            TEXT("StaticMesh") },
				{ MATUSAGE_InstancedStaticMeshes, TEXT("InstancedStaticMeshes") },
				{ MATUSAGE_SkeletalMesh,          TEXT("SkeletalMesh") },
				{ MATUSAGE_SplineMesh,            TEXT("SplineMesh") },
				{ MATUSAGE_GeometryCache,         TEXT("GeometryCache") },
			};
			Info.Usages.Reset();
			for (const FUsageSpelling& Spelling : Usages)
			{
				if (UsageSource.GetUsageByFlag(Spelling.Usage))
				{
					Info.Usages.AddUnique(Spelling.Name);
				}
			}
			Info.Usages.AddUnique(TEXT("Landscape"));
#else
			(void)UsageSource;
#endif
		}

		/** Every fact, off an existing material or material instance (its base material's graph and settings). */
		void ReadMaterialFactsFromAsset(UMaterialInterface& MaterialInterface, Lang::FPipelineMaterialInfo& Info)
		{
			UMaterial* Material = MaterialInterface.GetMaterial();
			if (!Material)
			{
				return;
			}

			// The usage flags asked of what the pass names: an instance answers for itself (today with its parent's flags).
			ReadMaterialSettingFacts(*Material, MaterialInterface, Info);

#if DREAMSHADER_WITH_CUSTOM_PASS
			// The inputs a pass binds by name: every UserSceneTexture node, the material's functions included -- under the name
			// the pass names: an instance may rename an input (UMaterialInstance::UserSceneTextureOverrides), and the runtime
			// matches a `read` against the renamed one (Render/DreamPassFullscreen.cpp, GetUserSceneTextureOverride).
			TArray<UMaterialExpressionUserSceneTexture*> UserSceneTextures;
			Material->GetAllExpressionsInMaterialAndFunctionsOfType(UserSceneTextures);
			for (const UMaterialExpressionUserSceneTexture* Expression : UserSceneTextures)
			{
				if (Expression && !Expression->UserSceneTexture.IsNone())
				{
					FName InputName = Expression->UserSceneTexture;
					FName Renamed = InputName;
					// As the render proxy reads an override: one that names nothing leaves the input as it is.
					if (MaterialInterface.GetUserSceneTextureOverride(Renamed) && !Renamed.IsNone())
					{
						InputName = Renamed;
					}
					Info.UserSceneTextureInputs.AddUnique(InputName.ToString());
				}
			}

			// The post-process input slots SceneTexture nodes take, which the named inputs then skip.
			TArray<UMaterialExpressionSceneTexture*> SceneTextures;
			Material->GetAllExpressionsInMaterialAndFunctionsOfType(SceneTextures);
			for (const UMaterialExpressionSceneTexture* Expression : SceneTextures)
			{
				const int32 Id = Expression ? static_cast<int32>(Expression->SceneTextureId.GetValue()) : INDEX_NONE;
				if (Id >= static_cast<int32>(PPI_PostProcessInput0) && Id <= static_cast<int32>(PPI_PostProcessInput6))
				{
					Info.PostProcessInputsUsed |= 1u << (Id - static_cast<int32>(PPI_PostProcessInput0));
				}
			}

			// UE.DreamPassOutput counts only in the material's own graph: the engine compiles no custom output from a function.
			for (UMaterialExpression* Expression : Material->GetExpressions())
			{
				const UMaterialExpressionDreamPassOutput* Output = Cast<UMaterialExpressionDreamPassOutput>(Expression);
				if (!Output)
				{
					continue;
				}
				Info.bHasPassOutput = true;
				const FExpressionInput* Pins[] = { &Output->Output0, &Output->Output1, &Output->Output2, &Output->Output3 };
				for (int32 Pin = 0; Pin < UE_ARRAY_COUNT(Pins); ++Pin)
				{
					if (Pins[Pin]->Expression)
					{
						Info.PassOutputsConnected |= 1u << Pin;
					}
				}
			}
#endif
		}

		/**
		 * Every fact, off the material's source, for a material not built yet (a `check` of a `.dsp` on a fresh checkout):
		 * the settings through a scratch UMaterial and the very ApplySettings a build uses, the graph facts off the IR of its
		 * own graph. Nodes inside the functions it calls are not seen; a build reads them off the asset.
		 */
		bool ReadMaterialFactsFromSource(const FString& MaterialSource, const FString& ObjectPath, Lang::FPipelineMaterialInfo& Info)
		{
			FDreamShaderLang2PipelineResult Run;
			if (!RunDreamShaderPipelineToIR(MaterialSource, Run) || !Run.IR.IsValid())
			{
				return false;
			}

			const IR::FIRProduct* Product = nullptr;
			for (const IR::FIRProduct& Candidate : Run.IR->Products)
			{
				FString PackageName;
				FString CandidatePath;
				FString LeafName;
				FDreamShaderError DestinationError;
				if (Candidate.Kind == IR::EIRProductKind::Material
					&& ResolveIRProductObjectPath(Candidate, Run.SourceFilePath, PackageName, CandidatePath, LeafName, DestinationError)
					&& CandidatePath.Equals(ObjectPath, ESearchCase::IgnoreCase))
				{
					Product = &Candidate;
					break;
				}
			}
			if (!Product)
			{
				return false;
			}

			FTextShaderDefinition Definition;
			BuildDefinitionForIRProduct(*Product, Run.SourceFilePath, Definition);
			UMaterial* Scratch = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
			FDreamShaderError SettingsError;
			// A setting the material's own compile refuses is that compile's error to report; the facts are best effort.
			Private::ApplySettings(Scratch, Definition, SettingsError);
			ReadMaterialSettingFacts(*Scratch, *Scratch, Info);
			Scratch->MarkAsGarbage();

			const IR::FBuiltinCatalog& Catalog = GetDreamShaderBuiltinCatalog();
			for (const IR::FIRNode& Node : Product->Graph.Nodes)
			{
				if (Node.Op != IR::EIROp::Reflected || !Catalog.Expressions.IsValidIndex(Node.CatalogIndex))
				{
					continue;
				}
				const FString& ClassName = Catalog.Expressions[Node.CatalogIndex].ClassName;
				if (ClassName.Equals(TEXT("MaterialExpressionUserSceneTexture"), ESearchCase::CaseSensitive))
				{
					if (const IR::FIRProperty* Name = Node.FindProperty(TEXT("UserSceneTexture")))
					{
						const FString Texture = Name->Value.S.TrimStartAndEnd();
						if (!Texture.IsEmpty() && !Texture.Equals(TEXT("None")))
						{
							Info.UserSceneTextureInputs.AddUnique(Texture);
						}
					}
				}
				else if (ClassName.Equals(TEXT("MaterialExpressionSceneTexture"), ESearchCase::CaseSensitive))
				{
					if (const IR::FIRProperty* Id = Node.FindProperty(TEXT("SceneTextureId")))
					{
						FString Spelling = Id->Value.S;
						Spelling.RemoveFromStart(TEXT("PPI_"));
						if (Spelling.RemoveFromStart(TEXT("PostProcessInput")) && Spelling.IsNumeric())
						{
							const int32 Input = FCString::Atoi(*Spelling);
							if (Input >= 0 && Input <= 6)
							{
								Info.PostProcessInputsUsed |= 1u << Input;
							}
						}
					}
				}
				else if (ClassName.Equals(TEXT("MaterialExpressionDreamPassOutput"), ESearchCase::CaseSensitive))
				{
					Info.bHasPassOutput = true;
					for (int32 Pin = 0; Pin < ::UE::DreamPass::MaxMeshOutputs; ++Pin)
					{
						const IR::FIRInput* Input = Node.FindInput(FString::Printf(TEXT("Output%d"), Pin));
						if (Input && Input->Value.IsValid())
						{
							Info.PassOutputsConnected |= 1u << Pin;
						}
					}
				}
			}
			return true;
		}

		/** The facts as build-key text: a `.dsp` is rebuilt when what its passes were checked against moves. */
		FString DescribeMaterialFacts(const Lang::FPipelineMaterialInfo& Info)
		{
			return FString::Printf(
				TEXT("%s|%s|%d|%s|%u|%d|%u|%s|%d"), /* I18N-EXEMPT: build-key material, never displayed */
				*Info.Domain,
				*Info.BlendableLocation,
				Info.bDisablePreExposureScale ? 1 : 0,
				*FString::Join(Info.UserSceneTextureInputs, TEXT(",")),
				Info.PostProcessInputsUsed,
				Info.bHasPassOutput ? 1 : 0,
				Info.PassOutputsConnected,
				*FString::Join(Info.Usages, TEXT(",")),
				Info.bUsesNewTranslator ? 1 : 0);
		}

		/**
		 * In an emitting run: compiles SourceFile first when its asset is missing or older than it (the `.dsi` DSH8264 rule).
		 * False with diagnostics when that compile failed. Never re-enters a source being resolved further up.
		 */
		bool CompileReferencedSourceIfStale(
			const FString& SourceFile,
			const FString& ObjectPath,
			const FString& Reference,
			const Lang::FLangSpan& Span,
			const bool bMaterial,
			Lang::FLangDiagnosticSink& Diagnostics)
		{
			FDreamShaderLang2PipelineResult Run;
			const bool bLowered = RunDreamShaderPipelineToIR(SourceFile, Run);

			UObject* Asset = FindOrLoadAsset(ObjectPath);
			if (Asset && bLowered && Private::IsGeneratedAssetSourceCurrent(Asset, Run.SourceFilePath, Run.SourceHash))
			{
				return true;
			}

			// One pair of codes per stage, so a material's and a pipeline's compile-first read apart.
			if (bMaterial)
			{
				Diagnostics.Info(TEXT("DSH8332"), Span, FText::Format(
					LOCTEXT("MaterialCompiledFirst", "'{0}' was missing or older than its source, so '{1}' was compiled first."),
					FText::FromString(ObjectPath),
					FText::FromString(FPaths::GetCleanFilename(SourceFile))));
			}
			else
			{
				Diagnostics.Info(TEXT("DSH5320"), Span, FText::Format(
					LOCTEXT("PipelineCompiledFirst", "The pipeline '{0}' was missing or older than its source, so '{1}' was compiled first."),
					FText::FromString(ObjectPath),
					FText::FromString(FPaths::GetCleanFilename(SourceFile))));
			}

			FDreamShaderError CompileError;
			if (CompileDreamShaderSourceFile(SourceFile, /*bForce*/ false, ::UE::DreamShader::EThinCustomPersistence::Materialized, CompileError))
			{
				return true;
			}

			if (bMaterial)
			{
				return Diagnostics.Error(TEXT("DSH8331"), Span, FText::Format(
					LOCTEXT("MaterialCompileFailed", "The material '{0}' comes from '{1}', which failed to compile, so this pipeline has no material to check its pass against. {2}"),
					FText::FromString(Reference),
					FText::FromString(FPaths::GetCleanFilename(SourceFile)),
					FText::FromString(CompileError.Message)));
			}
			return Diagnostics.Error(TEXT("DSH5319"), Span, FText::Format(
				LOCTEXT("PipelineCompileFailed", "The pipeline '{0}' comes from '{1}', which failed to compile, so the buffer this material reads cannot be checked. {2}"),
				FText::FromString(Reference),
				FText::FromString(FPaths::GetCleanFilename(SourceFile)),
				FText::FromString(CompileError.Message)));
		}

		bool IsPassBufferNode(const IR::FIRNode& Node, const IR::FBuiltinCatalog& Catalog)
		{
			return Node.Op == IR::EIROp::Reflected
				&& Catalog.Expressions.IsValidIndex(Node.CatalogIndex)
				&& Catalog.Expressions[Node.CatalogIndex].ClassName.Equals(TEXT("MaterialExpressionDreamPassBuffer"), ESearchCase::CaseSensitive);
		}

		/** What one pipeline reference of a `.dss` resolved to, kept so several nodes naming it resolve (and compile) it once. */
		struct FResolvedPassPipeline
		{
			bool bResolved = false;
			FString ObjectPath;
			FString SourceFile;
			bool bCycle = false;
			TWeakObjectPtr<UDreamPassPipeline> Asset;
			bool bHasPayload = false;
			IR::FIRPassPipeline Payload;
		};
	}

	// ------------------------------------------------------------------------------------------- the `.dsp` stage

	void ResolveDreamShaderPipelineReferencesForPipeline(
		const Lang::FModule& Module,
		const Lang::FLangSourceText& Source,
		const FString& SourceFilePath,
		const bool bCompileStaleMaterials,
		Lang::FPipelineReferences& OutReferences,
		FString& OutBuildKeyText,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamShaderPipelineReferencesDetail;

		OutReferences = Lang::FPipelineReferences();
		OutReferences.bCustomPassAvailable = DREAMSHADER_WITH_CUSTOM_PASS != 0;
		for (const FName Layer : UDreamPassSettings::Get().LayerNames)
		{
			OutReferences.LayerNames.Add(Layer.ToString());
		}

		// The layer table maps names to bits: moving one re-targets the pipeline, so the table belongs to its key.
		OutBuildKeyText = FString::Printf(
			TEXT("PassLayers=%s\nCustomPass=%d\n"), /* I18N-EXEMPT: build-key material, never displayed */
			*FString::Join(OutReferences.LayerNames, TEXT(",")),
			OutReferences.bCustomPassAvailable ? 1 : 0);

		TArray<FString> MaterialReferences;
		TArray<FString> ShaderReferences;
		Lang::CollectDreamShaderPipelineReferences(Module, MaterialReferences, ShaderReferences);

		const FScopedCrossReference Guard(SourceFilePath);

		// ------------------------------------------------------------------------------------------------ materials
		for (const FString& Reference : MaterialReferences)
		{
			Lang::FPipelineMaterialInfo& Info = OutReferences.Materials.AddDefaulted_GetRef();
			Info.Reference = Reference;
			const Lang::FLangSpan Span = FindReferenceSpan(Source, Reference);

			FResolvedReference Resolved;
			switch (ResolveProductReference(Reference, SourceFilePath, [](const IR::EIRProductKind Kind) { return IsMaterialProductKind(Kind); }, UMaterialInterface::StaticClass(), Resolved))
			{
			case EReferenceOutcome::Malformed:
				Diagnostics.Error(TEXT("DSH8334"), Span, FText::Format(
					LOCTEXT("MaterialReferenceMalformed", "The material '{0}' does not resolve to an asset path. {1}"),
					FText::FromString(Reference),
					FText::FromString(Resolved.PathError)));
				continue;

			case EReferenceOutcome::Ambiguous:
				Diagnostics.Error(TEXT("DSH8330"), Span, FText::Format(
					LOCTEXT("MaterialReferenceAmbiguous", "'{0}' names more than one material under this source root ({1}); write the material's asset path instead."),
					FText::FromString(Reference),
					FText::FromString(FString::Join(Resolved.AmbiguousSources, TEXT(", ")))));
				continue;

			case EReferenceOutcome::NotFound:
				// The binder words it, with the pass that named it.
				OutBuildKeyText += FString::Printf(TEXT("Material=%s|-\n"), *Reference); /* I18N-EXEMPT: build-key material, never displayed */
				continue;

			case EReferenceOutcome::Found:
				break;
			}

			Info.ObjectPath = Resolved.ObjectPath;

			const bool bCycle = !Resolved.SourceFile.IsEmpty() && IsInCrossReferenceChain(Resolved.SourceFile);
			if (bCycle)
			{
				Diagnostics.Warning(TEXT("DSH8333"), Span, FText::Format(
					LOCTEXT("MaterialReferenceCycle", "'{0}' is built by '{1}', which is being compiled already further up this compile (it reads this pipeline's buffers), so the material is checked as it stands on disk."),
					FText::FromString(Reference),
					FText::FromString(FPaths::GetCleanFilename(Resolved.SourceFile))));
			}
			else if (bCompileStaleMaterials && !Resolved.SourceFile.IsEmpty())
			{
				if (!CompileReferencedSourceIfStale(Resolved.SourceFile, Resolved.ObjectPath, Reference, Span, /*bMaterial*/ true, Diagnostics))
				{
					continue;
				}
			}

			UObject* Asset = FindOrLoadAsset(Resolved.ObjectPath);
			if (Asset && !Asset->IsA<UMaterialInterface>())
			{
				Diagnostics.Error(TEXT("DSH8334"), Span, FText::Format(
					LOCTEXT("MaterialReferenceNotMaterial", "'{0}' is a {1}, not a material or a material instance."),
					FText::FromString(Reference),
					FText::FromString(Asset->GetClass()->GetName())));
				continue;
			}

			if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset))
			{
				Info.bFound = true;
				ReadMaterialFactsFromAsset(*Material, Info);
			}
			else if (!Resolved.SourceFile.IsEmpty() && !bCycle)
			{
				Info.bFound = ReadMaterialFactsFromSource(Resolved.SourceFile, Resolved.ObjectPath, Info);
			}

			// The material's path, the build key it was last built under and the facts checked: any of them moving rebuilds
			// the pipeline, which is what lets the bridge's dependents queue skip an unaffected one on its hash.
			OutBuildKeyText += FString::Printf(
				TEXT("Material=%s|%s|%s|%s\n"), /* I18N-EXEMPT: build-key material, never displayed */
				*Reference,
				*Info.ObjectPath,
				Asset ? *Private::GetGeneratedAssetSourceHash(Asset) : TEXT("-"),
				Info.bFound ? *DescribeMaterialFacts(Info) : TEXT("-"));
		}

		// -------------------------------------------------------------------------------------------------- shaders
		for (const FString& Reference : ShaderReferences)
		{
			Lang::FPipelineShaderInfo& Info = OutReferences.Shaders.AddDefaulted_GetRef();
			Info.Reference = Reference;
			ResolveDreamPassShaderReference(Reference, SourceFilePath, Info.VirtualPath, Info.FilePath);
			Info.bExists = !Info.FilePath.IsEmpty() && IFileManager::Get().FileExists(*Info.FilePath);

			FString ContentHash = TEXT("-");
			if (Info.bExists)
			{
				// The entry may be defined in a file the shader includes by a relative path: the snapshot takes those along,
				// so they count as the shader's own.
				FDreamPassShaderClosure Closure;
				if (CollectDreamPassShaderClosure(Info.FilePath, Closure))
				{
					for (const FDreamPassShaderClosureFile& File : Closure.Files)
					{
						TMap<FString, FIntVector> ComputeEntries;
						TArray<FString> Functions;
						ScanDreamPassShaderFunctions(StripDreamPassShaderComments(File.Text), ComputeEntries, Functions);
						for (const TPair<FString, FIntVector>& Entry : ComputeEntries)
						{
							if (!Info.ComputeEntries.Contains(Entry.Key))
							{
								Info.ComputeEntries.Add(Entry.Key, Entry.Value);
							}
						}
						for (const FString& Function : Functions)
						{
							if (!Info.ComputeEntries.Contains(Function))
							{
								Info.Functions.AddUnique(Function);
							}
						}
					}
					// The snapshot's inputs: an edit of the `.usf`, or of anything it includes, is an edit of the pipeline.
					ContentHash = Closure.ComputeContentHash();
				}
			}

			OutBuildKeyText += FString::Printf(
				TEXT("Shader=%s|%s|%s\n"), /* I18N-EXEMPT: build-key material, never displayed */
				*Reference,
				*Info.VirtualPath,
				*ContentHash);
		}
	}

	void FillDreamShaderPipelinePayloadReferences(IR::FIRModule& Module, const Lang::FPipelineReferences& References)
	{
		for (IR::FIRProduct& Product : Module.Products)
		{
			if (Product.Kind != IR::EIRProductKind::PassPipeline)
			{
				continue;
			}
			for (IR::FIRPass& Pass : Product.PassPipeline.Passes)
			{
				if (!Pass.MaterialReference.IsEmpty() && Pass.MaterialObjectPath.IsEmpty())
				{
					if (const Lang::FPipelineMaterialInfo* Material = References.FindMaterial(Pass.MaterialReference))
					{
						Pass.MaterialObjectPath = Material->ObjectPath;
					}
				}
				if (!Pass.ShaderReference.IsEmpty())
				{
					if (const Lang::FPipelineShaderInfo* Shader = References.FindShader(Pass.ShaderReference))
					{
						if (Pass.ShaderVirtualPath.IsEmpty())
						{
							Pass.ShaderVirtualPath = Shader->VirtualPath;
						}
						if (Pass.ShaderFilePath.IsEmpty())
						{
							Pass.ShaderFilePath = Shader->FilePath;
						}
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------- the `.dss` stage

	void CollectDreamShaderPassBufferPipelineReferences(const IR::FIRModule& Module, const IR::FBuiltinCatalog& Catalog, TArray<FString>& OutReferences)
	{
		using namespace DreamShaderPipelineReferencesDetail;

		OutReferences.Reset();
		for (const IR::FIRProduct& Product : Module.Products)
		{
			for (const IR::FIRNode& Node : Product.Graph.Nodes)
			{
				if (!IsPassBufferNode(Node, Catalog))
				{
					continue;
				}
				if (const IR::FIRProperty* Pipeline = Node.FindProperty(TEXT("Pipeline")))
				{
					const FString Reference = Pipeline->Value.S.TrimStartAndEnd();
					if (!Reference.IsEmpty())
					{
						OutReferences.AddUnique(Reference);
					}
				}
			}
		}
	}

	void ResolveDreamShaderPassBufferReads(
		IR::FIRModule& Module,
		const IR::FBuiltinCatalog& Catalog,
		const Lang::FLangSourceText& Source,
		const FString& SourceFilePath,
		const bool bCompileStalePipelines,
		FString& OutBuildKeyText,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamShaderPipelineReferencesDetail;

		(void)Source;
		if (FDreamShaderProductIndex::Get().IsRefreshing())
		{
			return;
		}

		const FScopedCrossReference Guard(SourceFilePath);
		TMap<FString, FResolvedPassPipeline> Resolved;

		for (IR::FIRProduct& Product : Module.Products)
		{
			for (IR::FIRNode& Node : Product.Graph.Nodes)
			{
				if (!IsPassBufferNode(Node, Catalog))
				{
					continue;
				}

				const Lang::FLangSpan Span = Node.Source.Span;
				IR::FIRProperty* PipelineProperty = Node.FindProperty(TEXT("Pipeline"));
				const IR::FIRProperty* BufferProperty = Node.FindProperty(TEXT("Buffer"));
				const FString Reference = PipelineProperty ? PipelineProperty->Value.S.TrimStartAndEnd() : FString();
				const FString BufferName = BufferProperty ? BufferProperty->Value.S.TrimStartAndEnd() : FString();

				if (Reference.IsEmpty())
				{
					Diagnostics.Error(TEXT("DSH5315"), Span, LOCTEXT("PassBufferNoPipeline",
						"UE.DreamPassBuffer names no Pipeline: write the `.dsp`'s name (Pipeline = \"CP_Highlight\") or the pipeline asset's path."));
					continue;
				}
				if (BufferName.IsEmpty() || BufferName.Equals(TEXT("None")))
				{
					Diagnostics.Error(TEXT("DSH5324"), Span, FText::Format(
						LOCTEXT("PassBufferNoBuffer", "UE.DreamPassBuffer names no Buffer of '{0}': write the exported buffer's name (Buffer = \"Blurred\")."),
						FText::FromString(Reference)));
					continue;
				}

				FResolvedPassPipeline* Entry = Resolved.Find(Reference);
				if (!Entry)
				{
					Entry = &Resolved.Add(Reference);

					FResolvedReference FoundPipeline;
					switch (ResolveProductReference(Reference, SourceFilePath, [](const IR::EIRProductKind Kind) { return IsPipelineProductKind(Kind); }, UDreamPassPipeline::StaticClass(), FoundPipeline))
					{
					case EReferenceOutcome::Malformed:
						Diagnostics.Error(TEXT("DSH5315"), Span, FText::Format(
							LOCTEXT("PassBufferPipelineMalformed", "The pipeline '{0}' does not resolve to an asset path. {1}"),
							FText::FromString(Reference),
							FText::FromString(FoundPipeline.PathError)));
						continue;

					case EReferenceOutcome::NotFound:
					{
						const UE::DreamShader::FDreamShaderSourceRoot* Root = UE::DreamShader::FindSourceRootForFile(SourceFilePath);
						Diagnostics.Error(TEXT("DSH5316"), Span, FText::Format(
							LOCTEXT("PassBufferPipelineNotFound", "No pipeline named '{0}' is built by a .dsp under '{1}'; write the pipeline asset's path, or check the name."),
							FText::FromString(Reference),
							FText::FromString(Root ? Root->Directory : FString())));
						continue;
					}

					case EReferenceOutcome::Ambiguous:
						Diagnostics.Error(TEXT("DSH5317"), Span, FText::Format(
							LOCTEXT("PassBufferPipelineAmbiguous", "'{0}' names more than one pipeline under this source root ({1}); write the pipeline asset's path instead."),
							FText::FromString(Reference),
							FText::FromString(FString::Join(FoundPipeline.AmbiguousSources, TEXT(", ")))));
						continue;

					case EReferenceOutcome::Found:
						break;
					}

					Entry->ObjectPath = FoundPipeline.ObjectPath;
					Entry->SourceFile = FoundPipeline.SourceFile;
					Entry->bCycle = !Entry->SourceFile.IsEmpty() && IsInCrossReferenceChain(Entry->SourceFile);

					if (Entry->bCycle)
					{
						Diagnostics.Warning(TEXT("DSH5325"), Span, FText::Format(
							LOCTEXT("PassBufferPipelineCycle", "The pipeline '{0}' is being compiled already further up this compile (this material is one of its pass materials), so its buffer is checked against the pipeline asset as it stands."),
							FText::FromString(Reference)));
					}
					else if (bCompileStalePipelines && !Entry->SourceFile.IsEmpty())
					{
						if (!CompileReferencedSourceIfStale(Entry->SourceFile, Entry->ObjectPath, Reference, Span, /*bMaterial*/ false, Diagnostics))
						{
							continue;
						}
					}

					UObject* Asset = FindOrLoadAsset(Entry->ObjectPath);
					if (Asset && !Asset->IsA<UDreamPassPipeline>())
					{
						Diagnostics.Error(TEXT("DSH5326"), Span, FText::Format(
							LOCTEXT("PassBufferNotPipeline", "'{0}' is a {1}, not a DreamShader pass pipeline."),
							FText::FromString(Reference),
							FText::FromString(Asset->GetClass()->GetName())));
						continue;
					}
					Entry->Asset = Cast<UDreamPassPipeline>(Asset);

					if (!Entry->Asset.IsValid() && !Entry->SourceFile.IsEmpty() && !Entry->bCycle)
					{
						// Not built yet (a `check` on a fresh checkout): the buffers as the `.dsp` declares them.
						FDreamShaderLang2PipelineResult Run;
						if (RunDreamShaderPipelineToIR(Entry->SourceFile, Run) && Run.IR.IsValid())
						{
							for (const IR::FIRProduct& PipelineProduct : Run.IR->Products)
							{
								if (PipelineProduct.Kind == IR::EIRProductKind::PassPipeline)
								{
									Entry->Payload = PipelineProduct.PassPipeline;
									Entry->bHasPayload = true;
									break;
								}
							}
						}
					}

					if (!Entry->Asset.IsValid() && !Entry->bHasPayload && !Entry->bCycle)
					{
						Diagnostics.Error(TEXT("DSH5318"), Span, FText::Format(
							LOCTEXT("PassBufferPipelineMissing", "The pipeline '{0}' names nothing: no pipeline asset exists at '{1}', and no .dsp under the source roots builds it."),
							FText::FromString(Reference),
							FText::FromString(Entry->ObjectPath)));
						continue;
					}
					Entry->bResolved = true;
				}

				if (!Entry->bResolved)
				{
					continue;
				}

				// ---- the buffer: the node's own rule when the asset is there, so this and the material error agree
				FString Exported = TEXT("-");
				FString Format = TEXT("-");
				FString TargetPath = TEXT("-");
				if (const UDreamPassPipeline* Pipeline = Entry->Asset.Get())
				{
					FText Problem;
					UTextureRenderTarget2D* Target = UMaterialExpressionDreamPassBuffer::ResolveExportTarget(Pipeline, FName(*BufferName), &Problem);
					if (!Target)
					{
						Diagnostics.Error(TEXT("DSH5321"), Span, Problem);
						continue;
					}
					if (const FDreamPassBufferDesc* Desc = Pipeline->FindBuffer(FName(*BufferName)))
					{
						Exported = Desc->bExport ? TEXT("1") : TEXT("0");
						Format = ::UE::DreamPass::LexToString(Desc->Format);
					}
					TargetPath = Target->GetPathName();
				}
				else if (Entry->bHasPayload)
				{
					const IR::FIRPassBuffer* Desc = Entry->Payload.Buffers.FindByPredicate([&BufferName](const IR::FIRPassBuffer& Buffer)
					{
						return Buffer.Name.Equals(BufferName, ESearchCase::CaseSensitive);
					});
					if (!Desc)
					{
						Diagnostics.Error(TEXT("DSH5322"), Span, FText::Format(
							LOCTEXT("PassBufferUnknownInSource", "The pipeline '{0}' declares no buffer '{1}'."),
							FText::FromString(Reference),
							FText::FromString(BufferName)));
						continue;
					}
					if (!Desc->bExport)
					{
						Diagnostics.Error(TEXT("DSH5322"), Span, FText::Format(
							LOCTEXT("PassBufferNotExportedInSource", "Buffer '{1}' of the pipeline '{0}' is not exported. Declare it with Export = true in the .dsp."),
							FText::FromString(Reference),
							FText::FromString(BufferName)));
						continue;
					}
					if (Desc->Format == TEXT("R32U") || Desc->Format == TEXT("RG32U") || Desc->Format == TEXT("Depth32"))
					{
						Diagnostics.Error(TEXT("DSH5323"), Span, FText::Format(
							LOCTEXT("PassBufferNotSampleable", "Buffer '{1}' of the pipeline '{0}' is {2}, which a material cannot sample. Export a float or normalized buffer instead."),
							FText::FromString(Reference),
							FText::FromString(BufferName),
							FText::FromString(Desc->Format)));
						continue;
					}
					Exported = TEXT("1");
					Format = Desc->Format;
				}

				// The reflected-property writer loads the Pipeline property by path: a bare name becomes the object path here.
				if (PipelineProperty)
				{
					PipelineProperty->Value = IR::FIRPropertyValue::MakeObject(Entry->ObjectPath);
				}

				// The export facts this material was built against: a `.dsp` that moves them rebuilds it.
				OutBuildKeyText += FString::Printf(
					TEXT("PassBuffer=%s|%s|%s|%s|%s\n"), /* I18N-EXEMPT: build-key material, never displayed */
					*Entry->ObjectPath,
					*BufferName,
					*Exported,
					*Format,
					*TargetPath);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
