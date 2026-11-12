// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "MCP/DreamShaderMCPActions.h"

#include "MCP/DreamShaderMCPEndpoint.h"
#include "MCP/DreamShaderMCPPreviewStats.h"
#include "MCP/DreamShaderMCPResult.h"

#include "Bridge/DreamShaderEditorBridge.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "DreamShaderBuiltinCatalog.h"
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderCompilerService.h"
#include "DreamShaderModule.h"
#include "DreamShaderSettings.h"
#include "DreamShaderSourceFileUtils.h"
#include "DreamShaderTextWireUtils.h"
#include "DreamShaderVersionCompat.h"
#include "IR/IRCatalog.h"
#include "Preview/DreamShaderPreviewRenderer.h"
#include "Tools/DreamShaderCompilerTools.h"
#include "Tools/DreamShaderDecompileTools.h"
#include "Tools/DreamShaderShaderCheck.h"

#include "AssetCompilingManager.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialShared.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialInterface.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace UE::DreamShader::Editor::MCP
{
	namespace
	{
		using UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord;

		// Every summary and next-step sentence here is wire text for a model, never Slate: invariant English, written
		// as literals. A message that comes from the compiler arrives as an FText and goes out through
		// ToInvariantWireString, inside MakeDiagnostic.

		/** The diagnostics of the last check of each file through this endpoint, keyed by the normalized path. */
		TMap<FString, TArray<FDiagnostic>>& GetCheckedDiagnostics()
		{
			static TMap<FString, TArray<FDiagnostic>> Checked;
			return Checked;
		}

		FString Rel(const FString& Path)
		{
			return MakeProjectRelativePath(Path);
		}

		TSharedRef<FJsonValueString> JsonString(const FString& Text)
		{
			return MakeShared<FJsonValueString>(Text);
		}

		// ------------------------------------------------------------------------------ argument checks

		/** Resolves `file`; a refusal when it names nothing that exists. */
		bool ResolveExistingFile(const FActionCall& Call, FString& OutPath, FResult& OutRefusal)
		{
			const FString Argument = Call.GetString(TEXT("file"));
			OutPath = ResolveProjectFileArgument(Argument);
			if (OutPath.IsEmpty() || !IFileManager::Get().FileExists(*OutPath))
			{
				OutRefusal = FResult::MakeRefused(TEXT("file-not-found"),
					FString::Printf(TEXT("There is no file '%s'."), *Argument), // I18N-EXEMPT: wire text
					TEXT("Pass a path relative to the project root, such as DShader/Materials/M_Rock.dss."));
				return false;
			}
			return true;
		}

		/** A file the pipeline compiles on its own: `.dss`, `.dsi`, `.dsp`, `.dsm`, `.dsf`. */
		bool RequireCompilable(const FString& Path, FResult& OutRefusal)
		{
			if (Compiler::IsDreamShaderLang2Source(Path))
			{
				return true;
			}
			OutRefusal = FResult::MakeRefused(TEXT("not-a-source"),
				UE::DreamShader::IsDreamShaderHeaderFile(Path)
					? FString::Printf(TEXT("%s is a header; a header is compiled through the sources that include it."), *Rel(Path)) // I18N-EXEMPT: wire text
					: FString::Printf(TEXT("%s is not a DreamShader source (.dss, .dsi, .dsp, .dsm or .dsf)."), *Rel(Path)), // I18N-EXEMPT: wire text
				UE::DreamShader::IsDreamShaderHeaderFile(Path)
					? FString(TEXT("Check or compile a .dss that includes it."))
					: FString());
			return false;
		}

		FResult MakeCancelled()
		{
			return FResult::MakeRefused(TEXT("cancelled"), TEXT("Cancelled on request."));
		}

		// --------------------------------------------------------------------------------- what was written

		/**
		 * The packages saved while it is in scope, and whether each existed before: `created` or `saved`. Asked of the
		 * engine's own save events, so it is true for every route that saves -- the compiler service, the bridge, the
		 * pipeline -- without any of them reporting it.
		 */
		class FScopedWriteCapture
		{
		public:
			FScopedWriteCapture()
			{
				PreSaveHandle = UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FScopedWriteCapture::HandlePreSave);
				SavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FScopedWriteCapture::HandleSaved);
			}

			~FScopedWriteCapture()
			{
				UPackage::PreSavePackageWithContextEvent.Remove(PreSaveHandle);
				UPackage::PackageSavedWithContextEvent.Remove(SavedHandle);
			}

			void AppendTo(TArray<FWrite>& OutWrites) const
			{
				for (const FString& Name : SavedPackages)
				{
					const bool* bExisted = ExistedBefore.Find(Name);
					FWrite Write;
					Write.Asset = Name;
					Write.State = (bExisted && *bExisted) ? TEXT("saved") : TEXT("created");
					OutWrites.Add(Write);
				}
			}

			bool WasSaved(const FString& PackageName) const { return SavedPackages.Contains(PackageName); }

		private:
			void HandlePreSave(UPackage* Package, FObjectPreSaveContext)
			{
				if (Package)
				{
					const FString Name = Package->GetName();
					if (!ExistedBefore.Contains(Name))
					{
						ExistedBefore.Add(Name, FPackageName::DoesPackageExist(Name));
					}
				}
			}

			void HandleSaved(const FString&, UPackage* Package, FObjectPostSaveContext)
			{
				if (Package)
				{
					SavedPackages.AddUnique(Package->GetName());
				}
			}

			FDelegateHandle PreSaveHandle;
			FDelegateHandle SavedHandle;
			TMap<FString, bool> ExistedBefore;
			TArray<FString> SavedPackages;
		};

		/**
		 * After a successful build: the products the source declares, as `data.products`, and the ones that live in memory
		 * only (an Ephemeral ThinCustom material in the editor) as `memory` writes. Resolution only -- nothing is built.
		 */
		void DescribeProducts(const FString& SourceFile, const FScopedWriteCapture& Capture, FResult& Result)
		{
			Compiler::FDreamShaderProductResolution Resolution;
			if (!Compiler::ResolveDreamShaderSourceProducts(SourceFile, Resolution))
			{
				return;
			}
			TArray<TSharedPtr<FJsonValue>> Products;
			for (const Compiler::FDreamShaderResolvedProduct& Product : Resolution.Products)
			{
				TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
				Object->SetStringField(TEXT("asset"), Product.ObjectPath);
				Object->SetStringField(TEXT("kind"), UE::DreamShader::IR::LexToString(Product.Kind));
				Object->SetStringField(TEXT("backend"), UE::DreamShader::IR::LexToString(Product.Backend));
				const bool bOnDisk = FPackageName::DoesPackageExist(Product.PackageName);
				Object->SetBoolField(TEXT("onDisk"), bOnDisk);
				Products.Add(MakeShared<FJsonValueObject>(Object));

				if (!bOnDisk && !Capture.WasSaved(Product.PackageName) && FindObject<UObject>(nullptr, *Product.ObjectPath))
				{
					FWrite Write;
					Write.Asset = Product.PackageName;
					Write.State = TEXT("memory");
					Result.Writes.Add(Write);
				}
			}
			if (!Result.Data.IsValid())
			{
				Result.Data = MakeShared<FJsonObject>();
			}
			Result.Data->SetArrayField(TEXT("products"), Products);
		}

		/** A failed compile's outcome: a hand-edited asset is a refusal, a diagnostic error the source's, anything else the engine's. */
		void ClassifyFailure(const FString& SourceFile, const FString& Message, FResult& Result)
		{
			const bool bHandEdited = Result.Diagnostics.ContainsByPredicate([](const FDiagnostic& Diagnostic) { return Diagnostic.Code == TEXT("DSH8115"); })
				|| Message.Contains(TEXT("DSH8115"));
			if (bHandEdited)
			{
				Result.Outcome = EOutcome::Refused;
				Result.Reason = TEXT("asset-hand-edited");
				Result.Summary = FString::Printf(TEXT("%s was not rebuilt: an asset it builds was edited by hand in the editor since it was generated."), *Rel(SourceFile)); // I18N-EXEMPT: wire text
				Result.Next.Add(TEXT("Ask the user to choose, on the asset in the editor (right-click > DreamShader): Revert to Source, Adopt Into Source, or Detach. force does not get past this."));
				return;
			}

			// A compile route that leaves no structured records (the preview's material-only compile) still reports its
			// errors in the located wire form; the diagnostics store's own parser reads them back.
			if (CountSeverity(Result.Diagnostics, TEXT("error")) == 0 && !Message.IsEmpty())
			{
				const int32 First = Result.Diagnostics.Num();
				AppendDiagnostics(Result.Diagnostics, Private::FDreamShaderDiagnosticsStore::BuildGenerateErrorDiagnostics(SourceFile, Message), SourceFile);
				for (int32 Index = First; Index < Result.Diagnostics.Num(); ++Index)
				{
					// That parser keeps the code inside the message (`DSH4200: ...`); the result carries it apart.
					FDiagnostic& Diagnostic = Result.Diagnostics[Index];
					FString Code;
					FString Rest;
					if (Diagnostic.Message.Split(TEXT(": "), &Code, &Rest) && !MakeDiagnosticDocUrl(Code).IsEmpty())
					{
						Diagnostic.Code = Code;
						Diagnostic.Message = Rest;
						Diagnostic.Doc = MakeDiagnosticDocUrl(Code);
						Diagnostic.Stage = Compiler::Lang2StageForCode(Code);
					}
				}
				Result.Diagnostics.RemoveAll([](const FDiagnostic& Diagnostic) { return !Diagnostic.Code.StartsWith(TEXT("DSH")) && Diagnostic.Line <= 1; });
			}

			const int32 Errors = CountSeverity(Result.Diagnostics, TEXT("error"));
			if (Errors > 0)
			{
				Result.Outcome = EOutcome::SourceError;
				Result.Summary = FString::Printf(TEXT("%s has %d error(s)."), *Rel(SourceFile), Errors); // I18N-EXEMPT: wire text
				Result.Next.Add(TEXT("Fix the errors at the lines given, then compile again."));
				return;
			}

			Result.Outcome = EOutcome::EngineError;
			Result.Reason = TEXT("compile-failed");
			FString FirstLine;
			Message.Split(TEXT("\n"), &FirstLine, nullptr);
			Result.Summary = FString::Printf(TEXT("%s did not compile and no diagnostic says why: %s"), *Rel(SourceFile), // I18N-EXEMPT: wire text
				*(FirstLine.IsEmpty() ? Message : FirstLine).Replace(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("")));
			Result.Next.Add(TEXT("Read the engine log (dream_engine logs) for the line that failed."));
		}

		// --------------------------------------------------------------------------------------------- check

		TUniquePtr<IJob> StartCheck(const FActionCall& Call)
		{
			struct FState
			{
				bool bResolved = false;
				TArray<FString> Files;
				int32 Next = 0;
				FResult Result;
				TArray<TSharedPtr<FJsonValue>> FileSummaries;
				int32 Errors = 0;
				int32 Warnings = 0;
			};
			TSharedRef<FState> State = MakeShared<FState>();
			const FActionCall CallCopy = Call;

			return MakeUnique<FFunctionJob>([State, CallCopy](FJobContext& Context, FResult& OutResult) -> bool
			{
				if (!State->bResolved)
				{
					State->bResolved = true;
					const bool bAll = CallCopy.GetBool(TEXT("all"), false);
					if (bAll)
					{
						Compiler::FindProjectDreamShaderLang2Sources(State->Files);
					}
					else if (CallCopy.Has(TEXT("file")))
					{
						FString File;
						FResult Refusal;
						if (!ResolveExistingFile(CallCopy, File, Refusal) || !RequireCompilable(File, Refusal))
						{
							OutResult = Refusal;
							return true;
						}
						State->Files.Add(File);
					}
					else
					{
						OutResult = FResult::MakeRefused(TEXT("bad-args"), TEXT("check needs a file, or all=true."),
							TEXT("Pass file (relative to the project root), or all=true for every source."));
						return true;
					}
					Context.SetProgress(TEXT("check"), 0, State->Files.Num());
					return false;
				}

				if (Context.IsCancelRequested())
				{
					OutResult = MakeCancelled();
					return true;
				}

				if (State->Next < State->Files.Num())
				{
					// One file per tick: every request that arrives meanwhile is answered between two of them.
					const FString& File = State->Files[State->Next++];
					Compiler::FDreamShaderLang2PipelineOptions Options;
					Options.bEmitAssets = false;
					Compiler::FDreamShaderLang2PipelineResult Pipeline;
					Compiler::RunDreamShaderLang2Pipeline(File, Options, Pipeline);

					TArray<FDiagnostic> FileDiagnostics;
					AppendDiagnostics(FileDiagnostics, Pipeline.Diagnostics, File);
					const int32 FileErrors = CountSeverity(FileDiagnostics, TEXT("error"));
					const int32 FileWarnings = CountSeverity(FileDiagnostics, TEXT("warning"));
					State->Errors += FileErrors;
					State->Warnings += FileWarnings;
					GetCheckedDiagnostics().Add(UE::DreamShader::NormalizeSourceFilePath(File), FileDiagnostics);
					State->Result.Diagnostics.Append(FileDiagnostics);

					TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
					Summary->SetStringField(TEXT("file"), Rel(File));
					Summary->SetNumberField(TEXT("errors"), FileErrors);
					Summary->SetNumberField(TEXT("warnings"), FileWarnings);
					State->FileSummaries.Add(MakeShared<FJsonValueObject>(Summary));
					Context.SetProgress(TEXT("check"), State->Next, State->Files.Num());
					return false;
				}

				FResult& Result = State->Result;
				Result.Data = MakeShared<FJsonObject>();
				Result.Data->SetNumberField(TEXT("checked"), State->Files.Num());
				Result.Data->SetNumberField(TEXT("errors"), State->Errors);
				Result.Data->SetNumberField(TEXT("warnings"), State->Warnings);
				Result.Data->SetArrayField(TEXT("files"), State->FileSummaries);
				const FString What = State->Files.Num() == 1 ? Rel(State->Files[0]) : FString::Printf(TEXT("%d sources"), State->Files.Num()); // I18N-EXEMPT: wire text
				if (State->Errors > 0)
				{
					Result.Outcome = EOutcome::SourceError;
					Result.Summary = FString::Printf(TEXT("Checked %s: %d error(s), %d warning(s)."), *What, State->Errors, State->Warnings); // I18N-EXEMPT: wire text
					Result.Next.Add(TEXT("Fix the errors at the lines given, then check again."));
				}
				else
				{
					Result.Summary = FString::Printf(TEXT("Checked %s: no errors, %d warning(s). Nothing was built."), *What, State->Warnings); // I18N-EXEMPT: wire text
					if (State->Files.Num() == 1)
					{
						Result.Next.Add(TEXT("compile builds the assets; preview renders a material."));
					}
				}
				OutResult = Result;
				return true;
			});
		}

		// ------------------------------------------------------------------------------------------- compile

		TUniquePtr<IJob> StartCompile(const FActionCall& Call)
		{
			const FActionCall CallCopy = Call;
			return MakeUnique<FFunctionJob>([CallCopy](FJobContext& Context, FResult& OutResult) -> bool
			{
				FString File;
				if (!ResolveExistingFile(CallCopy, File, OutResult) || !RequireCompilable(File, OutResult))
				{
					return true;
				}
				if (Context.IsCancelRequested())
				{
					OutResult = MakeCancelled();
					return true;
				}
				Context.SetProgress(TEXT("compile"), 0, 1);

				const bool bForce = CallCopy.GetBool(TEXT("force"), false);
				const bool bShaders = CallCopy.GetBool(TEXT("shaders"), false);

				FResult Result;
				FScopedWriteCapture Capture;
				bool bSucceeded = false;
				FString Message;

				if (bShaders)
				{
					// `check -Shaders`'s route: a shader compile needs a real material, so the products are built and
					// saved as the commandlet saves them, and then their shaders are compiled and read back.
					Compiler::FDreamShaderLang2PipelineOptions Options;
					Options.bForce = bForce;
					Options.bEmitAssets = true;
					Options.bIncludeNoGeneration = true;
					Compiler::FDreamShaderLang2PipelineResult Pipeline;
					bSucceeded = Compiler::RunDreamShaderLang2Pipeline(File, Options, Pipeline);
					if (bSucceeded)
					{
						Compiler::FDreamShaderShaderCheckOptions ShaderOptions;
						Compiler::FDreamShaderShaderCheckStats Stats;
						bSucceeded = Compiler::CheckDreamShaderShaders(Pipeline, ShaderOptions, Pipeline.Diagnostics, Stats);
						Result.Data = MakeShared<FJsonObject>();
						Result.Data->SetNumberField(TEXT("materialsChecked"), Stats.MaterialsChecked);
						Result.Data->SetNumberField(TEXT("shaderErrors"), Stats.ShaderErrorsReported);
						Result.Data->SetBoolField(TEXT("timedOut"), Stats.bTimedOut);
					}
					AppendDiagnostics(Result.Diagnostics, Pipeline.Diagnostics, File);
				}
				else
				{
					Private::FDreamShaderEditorBridge* const Bridge = CallCopy.Mode == EEndpointMode::Editor ? Private::GetDreamShaderEditorBridge() : nullptr;
					if (Bridge)
					{
						// The editor's own compile, so the diagnostics store, the extensions and the Material Content
						// Browser all see this one as they see a save; a ThinCustom product stays Ephemeral as it would.
						bSucceeded = Bridge->CompileSourceFile(File, bForce, Message, /*bIncludeNoGeneration*/ true);
					}
					else if (::UE::DreamShader::IDreamShaderCompiler* const CompilerService = ::UE::DreamShader::GetDreamShaderCompiler())
					{
						// No editor session to keep an Ephemeral product alive in: saved, as the commandlet saves it.
						::UE::DreamShader::FDreamShaderCompileRequest Request;
						Request.SourceFilePath = File;
						Request.bForce = bForce;
						Request.ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Materialized;
						Request.bIncludeNoGeneration = true;
						const ::UE::DreamShader::FDreamShaderCompileResult Compiled = CompilerService->CompileAssets(Request);
						bSucceeded = Compiled.bSucceeded;
						Message = Private::ToInvariantWireString(Compiled.Message);
					}
					else
					{
						OutResult = FResult::MakeEngineError(TEXT("compiler-unavailable"), TEXT("The DreamShader compiler module is not loaded in this process."));
						return true;
					}

					TArray<FLang2DiagnosticRecord> Records;
					if (Compiler::GetDreamShaderLastCompileDiagnostics(File, Records))
					{
						AppendDiagnostics(Result.Diagnostics, Records, File);
					}
				}

				Capture.AppendTo(Result.Writes);
				GetCheckedDiagnostics().Add(UE::DreamShader::NormalizeSourceFilePath(File), Result.Diagnostics);

				if (!bSucceeded)
				{
					ClassifyFailure(File, Message, Result);
					OutResult = Result;
					return true;
				}

				DescribeProducts(File, Capture, Result);
				const int32 Warnings = CountSeverity(Result.Diagnostics, TEXT("warning"));
				if (Result.Writes.IsEmpty())
				{
					Result.Summary = FString::Printf(TEXT("%s is unchanged since its assets were built; nothing was rebuilt."), *Rel(File)); // I18N-EXEMPT: wire text
					Result.Next.Add(TEXT("Pass force=true to rebuild anyway."));
				}
				else
				{
					Result.Summary = FString::Printf(TEXT("Built %s: %d asset(s) written, %d warning(s)."), *Rel(File), Result.Writes.Num(), Warnings); // I18N-EXEMPT: wire text
					Result.Next.Add(TEXT("preview renders the material."));
				}
				OutResult = Result;
				return true;
			});
		}

		// --------------------------------------------------------------------------------------- diagnostics

		FResult RunDiagnostics(const FActionCall& Call)
		{
			FResult Result;
			Result.Data = MakeShared<FJsonObject>();
			Private::FDreamShaderEditorBridge* const Bridge = Call.Mode == EEndpointMode::Editor ? Private::GetDreamShaderEditorBridge() : nullptr;

			struct FFileCounts
			{
				FString File;
				int32 Errors = 0;
				int32 Warnings = 0;
				int32 Infos = 0;
			};
			TArray<FFileCounts> Counts;
			const auto AddFile = [&Result, &Counts](const FString& File, const TArray<FDiagnostic>& Diagnostics)
			{
				Result.Diagnostics.Append(Diagnostics);
				FFileCounts& Count = Counts.AddDefaulted_GetRef();
				Count.File = Rel(File);
				Count.Errors = CountSeverity(Diagnostics, TEXT("error"));
				Count.Warnings = CountSeverity(Diagnostics, TEXT("warning"));
				Count.Infos = CountSeverity(Diagnostics, TEXT("info"));
			};
			// The files with errors first, then warnings, then infos: the order the summary names them in, and the order
			// the result's diagnostics come in (errors first; FResult::ToJson).
			const auto ByWhatTheyHave = [](const FFileCounts& A, const FFileCounts& B)
			{
				if (A.Errors != B.Errors) { return A.Errors > B.Errors; }
				if (A.Warnings != B.Warnings) { return A.Warnings > B.Warnings; }
				return A.Infos > B.Infos;
			};
			const auto FilesJson = [&Counts, &ByWhatTheyHave]()
			{
				TArray<FFileCounts> Sorted = Counts;
				Sorted.StableSort(ByWhatTheyHave);
				TArray<TSharedPtr<FJsonValue>> Files;
				for (const FFileCounts& Count : Sorted)
				{
					TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
					Summary->SetStringField(TEXT("file"), Count.File);
					Summary->SetNumberField(TEXT("errors"), Count.Errors);
					Summary->SetNumberField(TEXT("warnings"), Count.Warnings);
					Summary->SetNumberField(TEXT("infos"), Count.Infos);
					Files.Add(MakeShared<FJsonValueObject>(Summary));
				}
				return Files;
			};
			// "2 errors, 1 warning": what a file has, in words.
			const auto DescribeCounts = [](const FFileCounts& Count)
			{
				TArray<FString> Parts;
				const auto Add = [&Parts](int32 N, const TCHAR* One, const TCHAR* Many)
				{
					if (N > 0)
					{
						Parts.Add(FString::Printf(TEXT("%d %s"), N, N == 1 ? One : Many)); // I18N-EXEMPT: wire text
					}
				};
				Add(Count.Errors, TEXT("error"), TEXT("errors"));
				Add(Count.Warnings, TEXT("warning"), TEXT("warnings"));
				Add(Count.Infos, TEXT("info"), TEXT("infos"));
				return FString::Join(Parts, TEXT(", "));
			};

			if (Call.Has(TEXT("file")))
			{
				const FString File = UE::DreamShader::NormalizeSourceFilePath(ResolveProjectFileArgument(Call.GetString(TEXT("file"))));
				// The editor's store is the freshest: every save it compiled is in it, not only what came through here.
				const TArray<Private::FDreamShaderDiagnosticRecord>* Stored = Bridge ? Bridge->GetDiagnosticsForSource(File) : nullptr;
				const TArray<FDiagnostic>* Checked = GetCheckedDiagnostics().Find(File);
				TArray<FDiagnostic> Diagnostics;
				FString From;
				if (Stored)
				{
					AppendDiagnostics(Diagnostics, *Stored, File);
					From = TEXT("the editor's last compile");
				}
				else if (Checked)
				{
					Diagnostics = *Checked;
					From = TEXT("the last check or compile through this endpoint");
				}
				else
				{
					TArray<FLang2DiagnosticRecord> Records;
					if (Compiler::GetDreamShaderLastCompileDiagnostics(File, Records))
					{
						AppendDiagnostics(Diagnostics, Records, File);
						From = TEXT("the last compile in this process");
					}
				}

				Result.Data->SetBoolField(TEXT("known"), !From.IsEmpty());
				if (From.IsEmpty())
				{
					Result.Summary = FString::Printf(TEXT("No diagnostics are known for %s in this process yet."), *Rel(File)); // I18N-EXEMPT: wire text
					Result.Next.Add(TEXT("Run check on it."));
					Result.Data->SetArrayField(TEXT("files"), FilesJson());
					return Result;
				}
				AddFile(File, Diagnostics);
				Result.Data->SetStringField(TEXT("from"), From);
				const FString Described = DescribeCounts(Counts.Last());
				Result.Outcome = Counts.Last().Errors > 0 ? EOutcome::SourceError : EOutcome::Ok;
				Result.Summary = FString::Printf(TEXT("%s has %s, from %s."), *Rel(File), // I18N-EXEMPT: wire text
					Described.IsEmpty() ? TEXT("no diagnostics") : *Described, *From);
			}
			else
			{
				TSet<FString> Files;
				if (Bridge)
				{
					for (const FString& File : Bridge->GetDiagnosticsStore().GetFilePaths())
					{
						Files.Add(File);
						TArray<FDiagnostic> Diagnostics;
						if (const TArray<Private::FDreamShaderDiagnosticRecord>* Stored = Bridge->GetDiagnosticsForSource(File))
						{
							AppendDiagnostics(Diagnostics, *Stored, File);
						}
						AddFile(File, Diagnostics);
					}
				}
				for (const TPair<FString, TArray<FDiagnostic>>& Pair : GetCheckedDiagnostics())
				{
					if (!Files.Contains(Pair.Key) && Pair.Value.Num() > 0)
					{
						Files.Add(Pair.Key);
						AddFile(Pair.Key, Pair.Value);
					}
				}
				// Only an error makes it a source error: a file whose findings are infos or warnings builds.
				const int32 Errors = CountSeverity(Result.Diagnostics, TEXT("error"));
				Result.Outcome = Errors > 0 ? EOutcome::SourceError : EOutcome::Ok;
				Result.Data->SetBoolField(TEXT("known"), true);

				// The summary names each file with what it has, the ones with errors first, so it says where to look.
				TArray<FFileCounts> WithFindings = Counts.FilterByPredicate([](const FFileCounts& Count)
				{
					return Count.Errors + Count.Warnings + Count.Infos > 0;
				});
				WithFindings.StableSort(ByWhatTheyHave);
				constexpr int32 MaxNamed = 10;
				TArray<FString> Named;
				for (int32 Index = 0; Index < FMath::Min(WithFindings.Num(), MaxNamed); ++Index)
				{
					Named.Add(FString::Printf(TEXT("%s has %s"), *WithFindings[Index].File, *DescribeCounts(WithFindings[Index]))); // I18N-EXEMPT: wire text
				}
				if (WithFindings.Num() > MaxNamed)
				{
					Named.Add(FString::Printf(TEXT("%d more"), WithFindings.Num() - MaxNamed)); // I18N-EXEMPT: wire text
				}
				if (WithFindings.IsEmpty())
				{
					Result.Summary = Counts.IsEmpty()
						? FString(TEXT("No file has diagnostics in this process yet."))
						: FString::Printf(TEXT("None of the %d known file(s) has diagnostics."), Counts.Num()); // I18N-EXEMPT: wire text
				}
				else
				{
					Result.Summary = FString::Printf(TEXT("%d file(s) have diagnostics: %s."), WithFindings.Num(), *FString::Join(Named, TEXT("; "))); // I18N-EXEMPT: wire text
				}
				if (Errors > 0)
				{
					Result.Next.Add(FString::Printf(TEXT("Fix the errors first. diagnostics with file=%s lists that file's alone."), *WithFindings[0].File)); // I18N-EXEMPT: wire text
				}
				if (!Bridge)
				{
					Result.Next.Add(TEXT("This process only knows the files checked or compiled in it; check with all=true covers every source."));
				}
			}
			Result.Data->SetArrayField(TEXT("files"), FilesJson());
			return Result;
		}

		// -------------------------------------------------------------------------------------------- lookup

		FString QualifiedName(const UE::DreamShader::IR::FCatalogExpression& Expression)
		{
			return Expression.Namespace + TEXT(".") + Expression.ShortName;
		}

		FResult RunLookup(const FActionCall& Call)
		{
			using namespace UE::DreamShader::IR;
			const FBuiltinCatalog& Catalog = Compiler::GetDreamShaderBuiltinCatalog();

			FString Query = Call.GetString(TEXT("name")).TrimStartAndEnd();
			const int32 Limit = FMath::Clamp(static_cast<int32>(Call.GetNumber(TEXT("limit"), 20.0)), 1, 200);
			FString Namespace;
			FString Name = Query;
			if (Query.StartsWith(TEXT("UE."), ESearchCase::CaseSensitive))
			{
				Namespace = TEXT("UE");
				Name = Query.RightChop(3);
			}
			else if (Query.StartsWith(TEXT("Substrate."), ESearchCase::CaseSensitive))
			{
				Namespace = TEXT("Substrate");
				Name = Query.RightChop(10);
			}
			else if (Query.StartsWith(TEXT("m."), ESearchCase::CaseSensitive))
			{
				Name = Query.RightChop(2);
			}

			// Exact first: the name as the language spells it, in the namespace asked for or either.
			int32 Exact = INDEX_NONE;
			if (!Namespace.IsEmpty())
			{
				Exact = Catalog.FindExpression(Namespace, Name);
			}
			else
			{
				Exact = Catalog.FindExpression(TEXT("UE"), Name);
				if (Exact == INDEX_NONE)
				{
					Exact = Catalog.FindExpression(TEXT("Substrate"), Name);
				}
				if (Exact == INDEX_NONE)
				{
					Exact = Catalog.FindExpressionByClass(Name);
				}
			}
			const int32 ExactAttribute = Catalog.FindMaterialAttribute(Name);

			FBuiltinCatalog Matches;
			Matches.EngineVersion = Catalog.EngineVersion;
			TArray<FString> AttributeNames;
			bool bExact = false;
			int32 Total = 0;
			if (Exact != INDEX_NONE || ExactAttribute != INDEX_NONE)
			{
				bExact = true;
				if (Exact != INDEX_NONE)
				{
					Matches.Expressions.Add(Catalog.Expressions[Exact]);
				}
				if (ExactAttribute != INDEX_NONE)
				{
					Matches.MaterialAttributes.Add(Catalog.MaterialAttributes[ExactAttribute]);
				}
				Total = Matches.Expressions.Num() + Matches.MaterialAttributes.Num();
			}
			else
			{
				const auto Contains = [&Name](const FString& Text) { return Text.Contains(Name, ESearchCase::IgnoreCase); };
				for (const FCatalogExpression& Expression : Catalog.Expressions)
				{
					if (!Namespace.IsEmpty() && Expression.Namespace != Namespace)
					{
						continue;
					}
					if (Contains(Expression.ShortName) || Contains(Expression.ClassName) || Expression.Aliases.ContainsByPredicate(Contains))
					{
						++Total;
						if (Matches.Expressions.Num() + Matches.MaterialAttributes.Num() < Limit)
						{
							Matches.Expressions.Add(Expression);
						}
					}
				}
				if (Namespace.IsEmpty())
				{
					for (const FCatalogMaterialAttribute& Attribute : Catalog.MaterialAttributes)
					{
						if (Contains(Attribute.Name) || Attribute.Aliases.ContainsByPredicate(Contains))
						{
							++Total;
							if (Matches.Expressions.Num() + Matches.MaterialAttributes.Num() < Limit)
							{
								Matches.MaterialAttributes.Add(Attribute);
							}
						}
					}
				}
			}

			// The manifest writer is the catalog's own serializer: one entry spelled the way export-catalog spells it.
			FResult Result;
			Result.Data = MakeShared<FJsonObject>();
			Result.Data->SetBoolField(TEXT("exact"), bExact);
			Result.Data->SetNumberField(TEXT("total"), Total);
			if (const TSharedPtr<FJsonObject> Serialized = ParseJsonObject(SaveBuiltinCatalogToJson(Matches)))
			{
				const TArray<TSharedPtr<FJsonValue>>* Expressions = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Attributes = nullptr;
				if (bExact)
				{
					Result.Data->SetArrayField(TEXT("expressions"), Serialized->TryGetArrayField(TEXT("expressions"), Expressions) && Expressions ? *Expressions : TArray<TSharedPtr<FJsonValue>>());
					Result.Data->SetArrayField(TEXT("attributes"), Serialized->TryGetArrayField(TEXT("materialAttributes"), Attributes) && Attributes ? *Attributes : TArray<TSharedPtr<FJsonValue>>());
				}
				else
				{
					// A list of names is what a search wants; the whole entry is one exact lookup away.
					TArray<TSharedPtr<FJsonValue>> Names;
					for (const FCatalogExpression& Expression : Matches.Expressions)
					{
						TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
						Entry->SetStringField(TEXT("name"), QualifiedName(Expression));
						Entry->SetStringField(TEXT("className"), Expression.ClassName);
						TArray<TSharedPtr<FJsonValue>> Inputs;
						for (const FCatalogPin& Pin : Expression.Inputs)
						{
							Inputs.Add(JsonString(Pin.Name));
						}
						Entry->SetArrayField(TEXT("inputs"), Inputs);
						Names.Add(MakeShared<FJsonValueObject>(Entry));
					}
					Result.Data->SetArrayField(TEXT("expressions"), Names);
					TArray<TSharedPtr<FJsonValue>> AttributeValues;
					for (const FCatalogMaterialAttribute& Attribute : Matches.MaterialAttributes)
					{
						AttributeValues.Add(JsonString(TEXT("m.") + Attribute.Name));
					}
					Result.Data->SetArrayField(TEXT("attributes"), AttributeValues);
				}
			}

			if (Total == 0)
			{
				Result.Summary = FString::Printf(TEXT("Nothing in the node catalog matches '%s'."), *Query); // I18N-EXEMPT: wire text
				const int32 Near = Catalog.FindExpressionIgnoreCase(Namespace.IsEmpty() ? FString(TEXT("UE")) : Namespace, Name);
				if (Near != INDEX_NONE)
				{
					Result.Next.Add(FString::Printf(TEXT("Did you mean %s? Names are case-sensitive."), *QualifiedName(Catalog.Expressions[Near]))); // I18N-EXEMPT: wire text
				}
				else
				{
					Result.Next.Add(TEXT("Search for a shorter part of the name."));
				}
			}
			else if (bExact)
			{
				Result.Summary = Matches.Expressions.Num() > 0
					? FString::Printf(TEXT("%s: %d input(s), %d output(s), %d propert(ies)."), *QualifiedName(Matches.Expressions[0]), // I18N-EXEMPT: wire text
						Matches.Expressions[0].Inputs.Num(), Matches.Expressions[0].Outputs.Num(), Matches.Expressions[0].Properties.Num())
					: FString::Printf(TEXT("m.%s is a material attribute."), *Matches.MaterialAttributes[0].Name); // I18N-EXEMPT: wire text
			}
			else
			{
				Result.Summary = FString::Printf(TEXT("%d match(es) for '%s'%s."), Total, *Query, // I18N-EXEMPT: wire text
					Total > Limit ? *FString::Printf(TEXT(", the first %d listed"), Limit) : TEXT("")); // I18N-EXEMPT: wire text
				Result.Next.Add(TEXT("Look up one exact name for its inputs, outputs and properties."));
			}
			return Result;
		}

		// ------------------------------------------------------------------------------------------- preview

		/** The material's resource for the platform this process renders with: what the preview draws from. */
		const FMaterialResource* GetRenderResource(const UMaterialInterface* Material)
		{
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
			return Material ? Material->GetMaterialResource(GMaxRHIShaderPlatform) : nullptr;
#else
			return Material ? Material->GetMaterialResource(GMaxRHIFeatureLevel) : nullptr;
#endif
		}

		/**
		 * Whether the shaders the preview will draw with are still compiling. A ThinCustom instance compiles through its
		 * hidden base, so the base is asked too. 5.4 gave materials the async-compilation query; before it, the resource
		 * of this platform is asked whether its compile has finished.
		 */
		bool IsStillCompiling(UMaterialInterface* Material)
		{
			if (!Material)
			{
				return false;
			}
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
			if (Material->IsCompiling())
			{
				return true;
			}
			const UMaterial* const Base = Material->GetMaterial();
			return Base && Base != Material && Base->IsCompiling();
#else
			const FMaterialResource* Resource = GetRenderResource(Material);
			if (!Resource)
			{
				Resource = GetRenderResource(Material->GetMaterial());
			}
			return Resource && !Resource->IsCompilationFinished();
#endif
		}

		/**
		 * The meshes the preview scene can draw on (UThumbnailManager loads them when the editor starts). One that is
		 * still building gets no render state (UStaticMeshComponent::ShouldCreateRenderState), and a preview drawn then
		 * is empty -- right after a host editor starts, they may well be: the preview waits for them as it waits for
		 * the material's shaders.
		 */
		void CollectPreviewMeshes(TArray<TWeakObjectPtr<UObject>>& OutAssets)
		{
			UThumbnailManager& Thumbnails = UThumbnailManager::Get();
			OutAssets.Add(Thumbnails.EditorCube.Get());
			OutAssets.Add(Thumbnails.EditorSphere.Get());
			OutAssets.Add(Thumbnails.EditorCylinder.Get());
			OutAssets.Add(Thumbnails.EditorPlane.Get());
			OutAssets.Add(Thumbnails.EditorSkySphere.Get());
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)
			OutAssets.Add(Thumbnails.EditorShaderBall.Get());
#endif
		}

		/** A preview mesh still building: nothing is drawn with it until it is done. */
		bool IsAnyStillCompiling(const TArray<TWeakObjectPtr<UObject>>& Assets)
		{
			for (const TWeakObjectPtr<UObject>& Asset : Assets)
			{
				const UStaticMesh* const Mesh = Cast<UStaticMesh>(Asset.Get());
				if (Mesh && Mesh->IsCompiling())
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * The two reference materials a preview is measured against, both unlit, masked and magenta: the silhouette
		 * (opacity mask 1) draws wherever the mesh is, whatever the previewed material does there; the background
		 * (opacity mask 0) draws nothing, which leaves the scene behind the mesh. Two-sided, so the silhouette covers a
		 * face the previewed material culls -- a see-through pixel, as far as the picture goes. Constants rather than
		 * one material with a parameter: an instance made before its parent's first compile can draw with the default
		 * value. Built once per process, the first time a preview needs them.
		 */
		struct FPreviewMaskMaterials
		{
			TStrongObjectPtr<UMaterial> Silhouette;
			TStrongObjectPtr<UMaterial> Background;
		};

		FPreviewMaskMaterials& GetPreviewMaskMaterials()
		{
			static FPreviewMaskMaterials Materials;
			return Materials;
		}

		UMaterial* MakePreviewMaskMaterial(const TCHAR* BaseName, float Coverage)
		{
			UPackage* const Package = GetTransientPackage();
			UMaterial* const Material = NewObject<UMaterial>(Package, MakeUniqueObjectName(Package, UMaterial::StaticClass(), BaseName), RF_Transient);
			Material->BlendMode = BLEND_Masked;
			Material->SetShadingModel(MSM_Unlit);
			Material->TwoSided = true;

			UMaterialExpressionConstant3Vector* const Color = NewObject<UMaterialExpressionConstant3Vector>(Material);
			Color->Constant = FLinearColor(1.0f, 0.0f, 1.0f);
			UMaterialExpressionConstant* const Mask = NewObject<UMaterialExpressionConstant>(Material);
			Mask->R = Coverage;
			Material->GetExpressionCollection().AddExpression(Color);
			Material->GetExpressionCollection().AddExpression(Mask);
			Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Color);
			Material->GetEditorOnlyData()->OpacityMask.Connect(0, Mask);
			Material->PostEditChange();
			return Material;
		}

		FPreviewMaskMaterials& EnsurePreviewMaskMaterials()
		{
			FPreviewMaskMaterials& Materials = GetPreviewMaskMaterials();
			if (!Materials.Silhouette.IsValid())
			{
				Materials.Silhouette.Reset(MakePreviewMaskMaterial(TEXT("DreamMCP_PreviewSilhouette"), 1.0f));
			}
			if (!Materials.Background.IsValid())
			{
				Materials.Background.Reset(MakePreviewMaskMaterial(TEXT("DreamMCP_PreviewBackground"), 0.0f));
			}
			return Materials;
		}

		/** `tone`: editor (what the editor's thumbnails show), fixed (exposure 1, the filmic curve), linear (exposure 1, no curve). */
		Private::FDreamShaderPreviewFrameOptions MakeFrameOptions(const FString& Tone)
		{
			Private::FDreamShaderPreviewFrameOptions Options;
			Options.bFixedExposure = Tone == TEXT("fixed") || Tone == TEXT("linear");
			Options.bToneMap = Tone != TEXT("linear");
			return Options;
		}

		const TCHAR* DescribeTone(const FString& Tone)
		{
			// The figures are measured: Unlit emissive greys on the plugin's test host, which has default exposure settings.
			if (Tone == TEXT("linear"))
			{
				return TEXT("Exposure 1, no tone curve, no bloom: a pixel is the material's output with gamma 2.2, clipped at 1. Emissive (1,0,0) is #FF0000; 0.5 is 186 of 255.");
			}
			if (Tone == TEXT("fixed"))
			{
				return TEXT("Tone-mapped at exposure 1. The filmic curve compresses values, so nothing clips and bright colours drift toward white: emissive 0.18 reads 117 of 255, 0.5 reads 178, 1 reads 208, 4 reads 240. tone=\"linear\" shows the values.");
			}
			return TEXT("Tone-mapped as the engine's thumbnails are: the project's exposure and the filmic curve. The curve compresses values, so nothing clips and bright colours drift toward white. With default exposure settings, emissive 0.18 reads 117 of 255, 0.5 reads 178, 1 reads 208, 4 reads 240. tone=\"linear\" shows the values.");
		}

		/** The widest strip a multi-frame preview writes. */
		constexpr int32 MaxPreviewStripWidth = 8192;

		TUniquePtr<IJob> StartPreview(const FActionCall& Call)
		{
			struct FState
			{
				int32 Stage = 0;
				FString File;
				FString Tone;
				Private::FDreamShaderPreviewRequest Request;
				Private::FDreamShaderPreviewResult Preview;
				TStrongObjectPtr<UMaterialInterface> Material;
				TArray<TWeakObjectPtr<UObject>> Meshes;
				TUniquePtr<Private::FDreamShaderPreviewRenderContext> Renderer;
				double StartTime = 0.0;
				double Duration = 1.0;
				int32 FrameCount = 1;
				TArray<FColor> BackgroundPixels;
				TArray<FColor> SilhouettePixels;
				TArray<TArray<FColor>> Frames;
				double WaitStarted = 0.0;
				FResult Result;

				double GetFrameTime(int32 Index) const
				{
					return FrameCount > 1 ? StartTime + Duration * double(Index) / double(FrameCount) : StartTime;
				}
			};
			TSharedRef<FState> State = MakeShared<FState>();
			const FActionCall CallCopy = Call;

			return MakeUnique<FFunctionJob>([State, CallCopy](FJobContext& Context, FResult& OutResult) -> bool
			{
				if (Context.IsCancelRequested())
				{
					OutResult = MakeCancelled();
					return true;
				}

				// Progress: compile, shaders, background, then one step per frame (the count is known after the first step).
				const int32 TotalSteps = 3 + State->FrameCount;

				if (State->Stage == 0)
				{
					FString File;
					if (!ResolveExistingFile(CallCopy, File, OutResult))
					{
						return true;
					}
					if (!UE::DreamShader::IsDreamShaderMaterialFile(File) && !UE::DreamShader::IsDreamShaderLang2File(File)
						&& !UE::DreamShader::IsDreamShaderInstanceFile(File))
					{
						OutResult = FResult::MakeRefused(TEXT("not-previewable"),
							FString::Printf(TEXT("%s builds no material; preview takes a .dss, .dsi or .dsm."), *Rel(File))); // I18N-EXEMPT: wire text
						return true;
					}

					State->FrameCount = FMath::Clamp(static_cast<int32>(CallCopy.GetNumber(TEXT("frames"), 1.0)), 1, 16);
					State->StartTime = CallCopy.GetNumber(TEXT("time"), 0.0);
					State->Duration = CallCopy.GetNumber(TEXT("duration"), 1.0);
					State->Tone = CallCopy.Has(TEXT("tone")) ? CallCopy.GetString(TEXT("tone")) : FString(TEXT("editor"));
					// A strip of frames is small frames side by side; one image is the full default size.
					const double DefaultSize = State->FrameCount > 1 ? 256.0 : 512.0;

					State->File = File;
					State->Request.SourceFilePath = File;
					State->Request.Mesh = CallCopy.Has(TEXT("mesh")) ? CallCopy.GetString(TEXT("mesh")) : FString(TEXT("sphere"));
					State->Request.Width = FMath::Clamp(static_cast<int32>(CallCopy.GetNumber(TEXT("width"), DefaultSize)), 64, 2048);
					State->Request.Height = FMath::Clamp(static_cast<int32>(CallCopy.GetNumber(TEXT("height"), DefaultSize)), 64, 2048);
					State->Request.OrbitYaw = static_cast<float>(CallCopy.GetNumber(TEXT("yaw"), -157.5));
					State->Request.OrbitPitch = static_cast<float>(CallCopy.GetNumber(TEXT("pitch"), -11.25));
					State->Request.bForceRecompile = CallCopy.GetBool(TEXT("force"), false);

					const int32 StripWidth = State->Request.Width * State->FrameCount;
					if (StripWidth > MaxPreviewStripWidth)
					{
						OutResult = FResult::MakeRefused(TEXT("strip-too-wide"),
							FString::Printf(TEXT("%d frames of %d px make a strip %d px wide; the limit is %d."), // I18N-EXEMPT: wire text
								State->FrameCount, State->Request.Width, StripWidth, MaxPreviewStripWidth),
							TEXT("Lower width or frames."));
						return true;
					}

					Context.SetProgress(TEXT("compile"), 0, 3 + State->FrameCount);
					CollectPreviewMeshes(State->Meshes);

					// The preview's own compile: interactive, so a ThinCustom product stays in memory.
					FScopedWriteCapture Capture;
					UMaterialInterface* Material = nullptr;
					const bool bResolved = Private::FDreamShaderPreviewRenderer::ResolvePreviewMaterial(State->Request, State->Preview, Material);
					Capture.AppendTo(State->Result.Writes);
					TArray<FLang2DiagnosticRecord> Records;
					if (Compiler::GetDreamShaderLastCompileDiagnostics(File, Records))
					{
						AppendDiagnostics(State->Result.Diagnostics, Records, File);
					}
					if (!bResolved || !Material)
					{
						FResult Failed = State->Result;
						ClassifyFailure(File, Private::ToInvariantWireString(State->Preview.Message), Failed);
						if (Failed.Outcome == EOutcome::EngineError)
						{
							Failed.Reason = TEXT("preview-compile-failed");
						}
						OutResult = Failed;
						return true;
					}
					State->Material.Reset(Material);

					EnsurePreviewMaskMaterials();
					State->WaitStarted = FPlatformTime::Seconds();
					State->Stage = 1;
					Context.SetProgress(TEXT("shaders"), 1, TotalSteps);
					return false;
				}

				if (State->Stage == 1)
				{
					// The shaders and the meshes build in the background; waiting a tick at a time keeps the endpoint answering.
					const bool bMaterialCompiling = IsStillCompiling(State->Material.Get())
						|| IsStillCompiling(GetPreviewMaskMaterials().Silhouette.Get()) || IsStillCompiling(GetPreviewMaskMaterials().Background.Get());
					const bool bMeshCompiling = IsAnyStillCompiling(State->Meshes);
					if (bMaterialCompiling || bMeshCompiling)
					{
						if (FPlatformTime::Seconds() - State->WaitStarted < 120.0)
						{
							Context.SetWaiting();
							return false;
						}
						UE_LOG(LogDreamShader, Warning, TEXT("DreamMCP: preview %s stopped waiting after 120 s; still compiling:%s%s. It renders what there is."),
							*Context.GetJobId(), bMaterialCompiling ? TEXT(" the material's shaders") : TEXT(""), bMeshCompiling ? TEXT(" a preview mesh") : TEXT(""));
					}
					State->Stage = 2;
					Context.SetProgress(TEXT("warm-up"), 2, TotalSteps);
					return false;
				}

				UMaterialInterface* const Material = State->Material.Get();
				FPreviewMaskMaterials& Masks = GetPreviewMaskMaterials();
				if (!Material || !Masks.Silhouette.IsValid() || !Masks.Background.IsValid())
				{
					OutResult = FResult::MakeEngineError(TEXT("preview-material-lost"), TEXT("The compiled material was collected before it could be rendered."));
					return true;
				}
				const int32 Width = State->Request.Width;
				const int32 Height = State->Request.Height;

				// The scene puts a material that asks for a plane (UI, sprites) on a plane seen level, whatever the request
				// says; the two reference renders have to frame the same mesh the same way.
				UMaterial* const Base = Material->GetBaseMaterial();
				const bool bForcedPlane = Base ? Base->ShouldForcePlanePreview() : Material->ShouldForcePlanePreview();
				const FString MaskMesh = bForcedPlane ? FString(TEXT("plane")) : State->Request.Mesh;
				const float MaskPitch = bForcedPlane ? 0.0f : State->Request.OrbitPitch;

				if (State->Stage == 2)
				{
					// A warm-up draw of all three. A material that has never been drawn may not have asked for its shaders
					// yet -- it does when it is first drawn, and until they are in, it draws as the engine's default
					// material. Measured on a fresh ThinCustom material: the first preview of each came out flat grey
					// whatever its emissive, and the first job of a process had no silhouette. The draws here are thrown
					// away; the next stage waits for what they asked for.
					State->Renderer = MakeUnique<Private::FDreamShaderPreviewRenderContext>();
					Private::FDreamShaderPreviewFrameOptions Options = MakeFrameOptions(State->Tone);
					Options.TimeSeconds = State->StartTime;
					State->Renderer->SetFrameOptions(Options);

					TArray<FColor> Discarded;
					FString RenderError;
					if (!State->Renderer->RenderFramePixels(Material, Width, Height, State->Request.Mesh, State->Request.OrbitYaw,
							State->Request.OrbitPitch, Discarded, RenderError)
						|| !State->Renderer->RenderFramePixels(Masks.Background.Get(), Width, Height, MaskMesh, State->Request.OrbitYaw, MaskPitch,
							Discarded, RenderError)
						|| !State->Renderer->RenderFramePixels(Masks.Silhouette.Get(), Width, Height, MaskMesh, State->Request.OrbitYaw, MaskPitch,
							Discarded, RenderError))
					{
						OutResult = FResult::MakeEngineError(TEXT("render-failed"), FString::Printf(TEXT("The preview did not render: %s"), *RenderError)); // I18N-EXEMPT: wire text
						return true;
					}
					State->WaitStarted = FPlatformTime::Seconds();
					State->Stage = 3;
					return false;
				}

				if (State->Stage == 3)
				{
					const bool bCompiling = IsStillCompiling(Material) || IsStillCompiling(Masks.Silhouette.Get()) || IsStillCompiling(Masks.Background.Get());
					if (bCompiling)
					{
						if (FPlatformTime::Seconds() - State->WaitStarted < 120.0)
						{
							Context.SetWaiting();
							return false;
						}
						UE_LOG(LogDreamShader, Warning, TEXT("DreamMCP: preview %s stopped waiting after 120 s for shaders the first draw asked for. It renders what there is."),
							*Context.GetJobId());
					}

					TArray<UObject*> Objects;
					Objects.Add(Material);
					Objects.Add(Masks.Silhouette.Get());
					Objects.Add(Masks.Background.Get());
					for (const TWeakObjectPtr<UObject>& Mesh : State->Meshes)
					{
						if (UObject* const Loaded = Mesh.Get())
						{
							Objects.Add(Loaded);
						}
					}
					FAssetCompilingManager::Get().FinishCompilationForObjects(Objects);

					FString RenderError;
					if (!State->Renderer->RenderFramePixels(Masks.Background.Get(), Width, Height, MaskMesh, State->Request.OrbitYaw, MaskPitch,
							State->BackgroundPixels, RenderError)
						|| !State->Renderer->RenderFramePixels(Masks.Silhouette.Get(), Width, Height, MaskMesh, State->Request.OrbitYaw, MaskPitch,
							State->SilhouettePixels, RenderError))
					{
						OutResult = FResult::MakeEngineError(TEXT("render-failed"), FString::Printf(TEXT("The preview did not render: %s"), *RenderError)); // I18N-EXEMPT: wire text
						return true;
					}
					State->Stage = 4;
					Context.SetProgress(TEXT("render"), 3, TotalSteps);
					return false;
				}

				if (State->Stage == 4)
				{
					// One frame a step: a strip of sixteen does not hold the endpoint for its whole length.
					const int32 Index = State->Frames.Num();
					Private::FDreamShaderPreviewFrameOptions Options = MakeFrameOptions(State->Tone);
					Options.TimeSeconds = State->GetFrameTime(Index);
					State->Renderer->SetFrameOptions(Options);

					TArray<FColor> Pixels;
					FString RenderError;
					if (!State->Renderer->RenderFramePixels(Material, Width, Height, State->Request.Mesh, State->Request.OrbitYaw,
							State->Request.OrbitPitch, Pixels, RenderError))
					{
						OutResult = FResult::MakeEngineError(TEXT("render-failed"), FString::Printf(TEXT("The preview did not render: %s"), *RenderError)); // I18N-EXEMPT: wire text
						return true;
					}
					State->Frames.Add(MoveTemp(Pixels));
					Context.SetProgress(TEXT("render"), 3 + State->Frames.Num(), TotalSteps);
					if (State->Frames.Num() < State->FrameCount)
					{
						return false;
					}
					State->Renderer.Reset();
					State->Stage = 5;
				}

				// Stitch, encode, save, measure.
				const int32 FrameCount = State->Frames.Num();
				const int32 StripWidth = Width * FrameCount;
				TArray<FColor> Strip;
				Strip.SetNumUninitialized(StripWidth * Height);
				for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
				{
					const TArray<FColor>& Frame = State->Frames[FrameIndex];
					for (int32 Row = 0; Row < Height; ++Row)
					{
						FMemory::Memcpy(&Strip[Row * StripWidth + FrameIndex * Width], &Frame[Row * Width], Width * sizeof(FColor));
					}
				}
				for (FColor& Pixel : Strip)
				{
					Pixel.A = 255;
				}
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(StripWidth, Height, TArrayView64<const FColor>(Strip.GetData(), Strip.Num()), Png);

				const FString ImagePath = FPaths::Combine(FEndpoint::GetArtifactDirectory(), Context.GetJobId() + TEXT(".png"));
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(ImagePath), true);
				if (Png.IsEmpty() || !FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(Png.GetData(), Png.Num()), *ImagePath))
				{
					OutResult = FResult::MakeEngineError(TEXT("artifact-not-written"), FString::Printf(TEXT("The preview image could not be written to %s."), *Rel(ImagePath))); // I18N-EXEMPT: wire text
					return true;
				}

				FResult& Result = State->Result;
				FArtifact Image;
				Image.Kind = TEXT("image");
				Image.Path = Rel(ImagePath);
				Image.Title = FrameCount > 1
					? FString::Printf(TEXT("%s on a %s, %d frames side by side"), *State->Preview.AssetPath, *State->Preview.Mesh, FrameCount) // I18N-EXEMPT: wire text
					: FString::Printf(TEXT("%s on a %s"), *State->Preview.AssetPath, *State->Preview.Mesh); // I18N-EXEMPT: wire text
				Result.Artifacts.Add(Image);

				// A shader that did not compile renders as the engine's default material: the picture alone would hide it.
				TArray<FString> ShaderErrors;
				const FMaterialResource* Resource = GetRenderResource(Material);
				if (!Resource)
				{
					Resource = GetRenderResource(Material->GetMaterial());
				}
				if (Resource)
				{
					ShaderErrors = Resource->GetCompileErrors();
				}
				for (const FString& Error : ShaderErrors)
				{
					FDiagnostic Diagnostic;
					Diagnostic.Severity = TEXT("error");
					Diagnostic.Stage = TEXT("shader");
					Diagnostic.File = Rel(State->File);
					Diagnostic.Line = 1;
					Diagnostic.Column = 1;
					Diagnostic.Message = Error.TrimStartAndEnd().Replace(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT(""));
					Result.Diagnostics.Add(Diagnostic);
				}

				// What the pixels show: each frame alone, and all of them together.
				FPreviewStats Overall;
				TArray<FPreviewStats> PerFrame;
				TArray<double> Times;
				TArray<TSharedPtr<FJsonValue>> FrameValues;
				for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
				{
					FPreviewStats& Stats = PerFrame.AddDefaulted_GetRef();
					Stats.Add(State->Frames[FrameIndex], State->BackgroundPixels, State->SilhouettePixels);
					Overall.Add(State->Frames[FrameIndex], State->BackgroundPixels, State->SilhouettePixels);
					Times.Add(State->GetFrameTime(FrameIndex));
					if (FrameCount > 1)
					{
						TSharedRef<FJsonObject> FrameJson = MakeShared<FJsonObject>();
						FrameJson->SetNumberField(TEXT("index"), FrameIndex);
						FrameJson->SetNumberField(TEXT("time"), FMath::RoundToDouble(Times.Last() * 10000.0) / 10000.0);
						FrameJson->SetObjectField(TEXT("stats"), Stats.ToJson());
						FrameValues.Add(MakeShared<FJsonValueObject>(FrameJson));
					}
				}
				const bool bLinear = State->Tone == TEXT("linear");

				Result.Data = MakeShared<FJsonObject>();
				Result.Data->SetStringField(TEXT("asset"), State->Preview.AssetPath);
				Result.Data->SetStringField(TEXT("mesh"), State->Preview.Mesh);
				Result.Data->SetNumberField(TEXT("width"), Width);
				Result.Data->SetNumberField(TEXT("height"), Height);
				Result.Data->SetStringField(TEXT("tone"), State->Tone);
				Result.Data->SetStringField(TEXT("toneNote"), DescribeTone(State->Tone));
				Result.Data->SetNumberField(TEXT("time"), State->StartTime);
				Result.Data->SetNumberField(TEXT("shaderErrors"), ShaderErrors.Num());
				Result.Data->SetObjectField(TEXT("stats"), Overall.ToJson());
				FPreviewAnimation Animation;
				if (FrameCount > 1)
				{
					Animation = AnalysePreviewAnimation(State->Frames, PerFrame);
					Result.Data->SetObjectField(TEXT("animation"), PreviewAnimationToJson(Animation, PerFrame, Times, State->Duration));
					Result.Data->SetArrayField(TEXT("frames"), FrameValues);
				}

				if (ShaderErrors.Num() > 0)
				{
					Result.Outcome = EOutcome::SourceError;
					Result.Summary = FString::Printf(TEXT("Rendered %s, but its shaders did not compile (%d error(s)); the image shows the default material."), // I18N-EXEMPT: wire text
						*State->Preview.AssetPath, ShaderErrors.Num());
					Result.Next.Add(TEXT("compile with shaders=true maps the shader errors to source lines."));
				}
				else
				{
					// A strip is told by how it changes, never by the mean of its frames: that can be a colour no frame shows.
					const FString Finding = FrameCount > 1
						? DescribePreviewAnimation(Animation, Overall, PerFrame, Times, State->Duration, bLinear, Result.Next)
						: DescribePreview(Overall, bLinear, Result.Next);
					Result.Summary = FString::Printf(TEXT("%s. Image: %s (%s, %dx%d%s)."), *Finding, *Rel(ImagePath), // I18N-EXEMPT: wire text
						*State->Preview.Mesh, Width, Height, FrameCount > 1 ? *FString::Printf(TEXT(", %d frames side by side"), FrameCount) : TEXT("")); // I18N-EXEMPT: wire text
				}
				GetCheckedDiagnostics().Add(UE::DreamShader::NormalizeSourceFilePath(State->File), Result.Diagnostics);
				OutResult = Result;
				return true;
			});
		}

		// ---------------------------------------------------------------------------------------------- info

		FResult RunInfo(const FActionCall& Call)
		{
			FResult Result;
			Result.Data = MakeShared<FJsonObject>();
			FString PluginVersion;
			if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader")))
			{
				PluginVersion = Plugin->GetDescriptor().VersionName;
			}
			const FString EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Patch);
			Result.Data->SetStringField(TEXT("plugin"), PluginVersion);
			Result.Data->SetStringField(TEXT("engine"), EngineVersion);
			Result.Data->SetStringField(TEXT("project"), FApp::GetProjectName());
			Result.Data->SetStringField(TEXT("mode"), LexEndpointMode(Call.Mode));
			Result.Data->SetBoolField(TEXT("render"), Call.bCanRender);
			Result.Data->SetBoolField(TEXT("customPass"), DREAMSHADER_WITH_CUSTOM_PASS != 0);
			Result.Data->SetBoolField(TEXT("substrate"), DREAMSHADER_WITH_SUBSTRATE_BUILTINS != 0);
			Result.Data->SetStringField(TEXT("sourceDirectory"), Rel(UE::DreamShader::GetSourceShaderDirectory()));

			const UDreamShaderSettings* const Settings = GetDefault<UDreamShaderSettings>();
			Result.Data->SetStringField(TEXT("defaultBackend"), Settings && Settings->DefaultBackend == EDreamShaderDefaultBackend::Graph ? TEXT("Graph") : TEXT("ThinCustom"));
			Result.Data->SetBoolField(TEXT("autoCompileOnSave"), Settings && Settings->bAutoCompileOnSave);

			TArray<TSharedPtr<FJsonValue>> Roots;
			for (const UE::DreamShader::FDreamShaderSourceRoot& Root : UE::DreamShader::GetSourceShaderRoots())
			{
				TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
				Object->SetStringField(TEXT("name"), Root.DisplayName);
				Object->SetStringField(TEXT("directory"), Rel(Root.Directory));
				Object->SetBoolField(TEXT("writable"), Root.bWritable);
				Object->SetBoolField(TEXT("project"), Root.bIsProjectRoot);
				if (!Root.PluginName.IsEmpty())
				{
					Object->SetStringField(TEXT("plugin"), Root.PluginName);
				}
				Roots.Add(MakeShared<FJsonValueObject>(Object));
			}
			Result.Data->SetArrayField(TEXT("roots"), Roots);

			if (Call.Mode == EEndpointMode::Editor)
			{
				if (const Private::FDreamShaderEditorBridge* Bridge = Private::GetDreamShaderEditorBridge())
				{
					Result.Data->SetBoolField(TEXT("bridgeOwner"), Bridge->IsBridgeOwner());
				}
			}

			Result.Summary = FString::Printf(TEXT("DreamShader %s on UE %s, %s process%s; %d source root(s), sources under %s."), // I18N-EXEMPT: wire text
				*PluginVersion, *EngineVersion, LexEndpointMode(Call.Mode), Call.bCanRender ? TEXT(" that renders") : TEXT(" without a renderer"),
				Roots.Num(), *Rel(UE::DreamShader::GetSourceShaderDirectory()));
			return Result;
		}

		// ----------------------------------------------------------------------------------------- decompile

		TUniquePtr<IJob> StartDecompile(const FActionCall& Call)
		{
			const FActionCall CallCopy = Call;
			return MakeUnique<FFunctionJob>([CallCopy](FJobContext& Context, FResult& OutResult) -> bool
			{
				if (Context.IsCancelRequested())
				{
					OutResult = MakeCancelled();
					return true;
				}
				Context.SetProgress(TEXT("decompile"), 0, 1);

				const FString AssetArgument = CallCopy.GetString(TEXT("asset")).TrimStartAndEnd();
				FString ObjectPath;
				UObject* const Asset = Private::LoadDreamShaderDecompileAsset(AssetArgument, ObjectPath);
				if (!Asset)
				{
					OutResult = FResult::MakeRefused(TEXT("asset-not-found"),
						FString::Printf(TEXT("No asset '%s' could be found or loaded."), *AssetArgument), // I18N-EXEMPT: wire text
						TEXT("Pass an object or package path such as /Game/Materials/M_Rock."));
					return true;
				}

				FString OutputPath;
				if (CallCopy.Has(TEXT("out")))
				{
					OutputPath = ResolveProjectFileArgument(CallCopy.GetString(TEXT("out")));
				}

				Editor::FDreamShaderDecompileRequest Request;
				Request.Asset = Asset;
				Request.OutputFilePath = OutputPath;
				Request.Format = Editor::EDreamShaderDecompileFormat::Auto;
				Request.bReadable = CallCopy.GetBool(TEXT("readable"), false);
				const Editor::FDreamShaderDecompileResult Decompiled = Private::RunDreamShaderDecompileRequest(Request);

				FResult Result;
				const FString DiagnosticsFile = Decompiled.OutputFilePath.IsEmpty() ? ObjectPath : Decompiled.OutputFilePath;
				TArray<FLang2DiagnosticRecord> Records;
				Private::BuildDreamShaderDecompileDiagnosticRecords(Decompiled, DiagnosticsFile, Records);
				AppendDiagnostics(Result.Diagnostics, Records, DiagnosticsFile);

				if (!Decompiled.bSucceeded)
				{
					Result.Outcome = EOutcome::Refused;
					Result.Reason = TEXT("not-decompilable");
					Result.Summary = FString::Printf(TEXT("%s could not be decompiled: %s"), *ObjectPath, // I18N-EXEMPT: wire text
						*Private::DescribeDreamShaderDecompileFailure(Decompiled).Replace(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("")));
					OutResult = Result;
					return true;
				}

				if (IFileManager::Get().FileExists(*Decompiled.OutputFilePath) && !CallCopy.GetBool(TEXT("overwrite"), false))
				{
					OutResult = FResult::MakeRefused(TEXT("file-exists"),
						FString::Printf(TEXT("%s already exists; nothing was written."), *Rel(Decompiled.OutputFilePath)), // I18N-EXEMPT: wire text
						TEXT("Pass overwrite=true to replace it, or out with another path."));
					return true;
				}

				FString SaveError;
				if (!Private::FDecompiledSourceWriter::Save(Decompiled, SaveError))
				{
					OutResult = FResult::MakeEngineError(TEXT("source-not-written"),
						FString::Printf(TEXT("%s could not be written."), *Rel(Decompiled.OutputFilePath))); // I18N-EXEMPT: wire text
					return true;
				}

				FArtifact Source;
				Source.Kind = TEXT("source");
				Source.Path = Rel(Decompiled.OutputFilePath);
				Source.Title = FString::Printf(TEXT("%s as DreamShader source"), *ObjectPath); // I18N-EXEMPT: wire text
				Result.Artifacts.Add(Source);
				Result.Data = MakeShared<FJsonObject>();
				Result.Data->SetStringField(TEXT("file"), Source.Path);
				Result.Data->SetStringField(TEXT("asset"), ObjectPath);
				Result.Data->SetNumberField(TEXT("warnings"), CountSeverity(Result.Diagnostics, TEXT("warning")));
				Result.Summary = FString::Printf(TEXT("Wrote %s from %s (%d warning(s)). No asset was changed."), *Source.Path, *ObjectPath, // I18N-EXEMPT: wire text
					CountSeverity(Result.Diagnostics, TEXT("warning")));
				Result.Next.Add(FString::Printf(TEXT("check %s to confirm the text compiles."), *Source.Path)); // I18N-EXEMPT: wire text
				OutResult = Result;
				return true;
			});
		}
	}

	void RegisterDreamShaderMCPActions(FEndpoint& Endpoint)
	{
		FActionHandler Check;
		Check.Start = &StartCheck;
		Endpoint.RegisterAction(TEXT("check"), MoveTemp(Check));

		FActionHandler Compile;
		Compile.Start = &StartCompile;
		Endpoint.RegisterAction(TEXT("compile"), MoveTemp(Compile));

		FActionHandler Diagnostics;
		Diagnostics.Run = &RunDiagnostics;
		Endpoint.RegisterAction(TEXT("diagnostics"), MoveTemp(Diagnostics));

		FActionHandler Lookup;
		Lookup.Run = &RunLookup;
		Endpoint.RegisterAction(TEXT("lookup"), MoveTemp(Lookup));

		FActionHandler Preview;
		Preview.Start = &StartPreview;
		Endpoint.RegisterAction(TEXT("preview"), MoveTemp(Preview));

		FActionHandler Info;
		Info.Run = &RunInfo;
		Endpoint.RegisterAction(TEXT("info"), MoveTemp(Info));

		FActionHandler Decompile;
		Decompile.Start = &StartDecompile;
		Endpoint.RegisterAction(TEXT("decompile"), MoveTemp(Decompile));
	}

	void ResetDreamShaderMCPActionState()
	{
		GetCheckedDiagnostics().Reset();
		GetPreviewMaskMaterials().Silhouette.Reset();
		GetPreviewMaskMaterials().Background.Reset();
	}
}
