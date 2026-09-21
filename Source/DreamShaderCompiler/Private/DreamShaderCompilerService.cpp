// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderCompilerService.h and DreamShaderCompilerServiceInternal.h.
//
// Re-homed here in the compiler relocation (research-relocation sections 3 and 6):
//
//   * from MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp -- the OnDreamShaderSourceGenerated
//     delegate and its outermost-only notice (1.x FScopedGenerationNotice), the warnings collector
//     behind RaiseGenerationWarning and its drain into the `Warnings:` block (1.x AppendGenerationWarnings),
//     and the DSH9011 / DSH9012 helpers (1.x FShaderCompileStageWatch, ReportCustomNodeLoopHeuristic);
//   * from UI/DreamShaderInstanceFactory.cpp -- IsMemoryOnlyMaterial and MaterializeDreamShaderMaterial,
//     whose LOCTEXT namespace and keys are kept so their translations survive the move.
//
// Nothing of the 1.x DSH9010 cancel path came along: a cancelled 2.0 compile is DSH8298, raised by the
// pipeline.

#include "DreamShaderCompilerService.h"

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerServiceInternal.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderGenerationProgress.h"
#include "Pipeline/DreamShaderCompilePipelineInternal.h"

#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"

#include "HAL/PlatformTime.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::DreamShader::Editor
{
	namespace DreamShaderCompilerServiceDetail
	{
		FOnDreamShaderSourceGenerated GDreamShaderSourceGeneratedDelegate;

		/** How many compiles are open on the stack. Game thread only, like everything that compiles. */
		int32 GDreamShaderCompileNoticeDepth = 0;

		/**
		 * Warnings raised while the current outermost compile runs. They are raised deep inside the emit, where
		 * the report being built is several frames up and out of reach, so they collect here and join the
		 * report's `Warnings:` block when the outermost compile words it.
		 */
		TArray<FString>& GetCollectedWarningLines()
		{
			static TArray<FString> Lines;
			return Lines;
		}

		/** Sources a nested compile finished inside the current outermost one, in finish order, one entry per source. */
		TArray<TPair<FString, bool>>& GetPendingNestedNotices()
		{
			static TArray<TPair<FString, bool>> Pending;
			return Pending;
		}

		/** Each source's most recent compile as store records, by normalized path; FString keys compare case-insensitively, as the file system does. */
		TMap<FString, TArray<::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord>>& GetLastCompileDiagnosticRecords()
		{
			static TMap<FString, TArray<::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord>> Records;
			return Records;
		}

		/**
		 * One compile's notice. Fires OnDreamShaderSourceGenerated for the outermost compile only -- a compile
		 * nested inside another (a `.dsi` building its parent first) queues its source instead, and the queue is
		 * broadcast when the outermost scope closes. Firing inside the pipeline as well would double every
		 * notification, and the Material Content Browser rebuilds its view on each one.
		 */
		struct FScopedDreamShaderCompileNotice
		{
			explicit FScopedDreamShaderCompileNotice(const FString& InSourceFilePath)
				: SourceFilePath(::UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath))
			{
				if (++GDreamShaderCompileNoticeDepth == 1)
				{
					// One compile, one warning list: the outermost scope owns it, so a compile never
					// inherits the previous file's findings.
					GetCollectedWarningLines().Reset();
					GetPendingNestedNotices().Reset();
				}
			}

			FScopedDreamShaderCompileNotice(const FScopedDreamShaderCompileNotice&) = delete;
			FScopedDreamShaderCompileNotice& operator=(const FScopedDreamShaderCompileNotice&) = delete;

			bool IsOutermost() const
			{
				return GDreamShaderCompileNoticeDepth == 1;
			}

			void Report(const bool bInSucceeded)
			{
				bSucceeded = bInSucceeded;
			}

			~FScopedDreamShaderCompileNotice()
			{
				if (--GDreamShaderCompileNoticeDepth > 0)
				{
					// Source paths compare case-insensitively on purpose, as the file system does.
					TArray<TPair<FString, bool>>& Pending = GetPendingNestedNotices();
					Pending.RemoveAll([this](const TPair<FString, bool>& Entry)
					{
						return Entry.Key.Equals(SourceFilePath, ESearchCase::IgnoreCase);
					});
					Pending.Emplace(SourceFilePath, bSucceeded);
					return;
				}

				// Taken out before broadcasting: a subscriber may start a compile of its own, which opens a
				// fresh outermost scope and resets the queue.
				TArray<TPair<FString, bool>> Nested = MoveTemp(GetPendingNestedNotices());
				GetPendingNestedNotices().Reset();
				for (const TPair<FString, bool>& Entry : Nested)
				{
					if (!Entry.Key.Equals(SourceFilePath, ESearchCase::IgnoreCase))
					{
						GDreamShaderSourceGeneratedDelegate.Broadcast(Entry.Key, Entry.Value);
					}
				}
				GDreamShaderSourceGeneratedDelegate.Broadcast(SourceFilePath, bSucceeded);
			}

			const FString SourceFilePath;
			bool bSucceeded = false;
		};
	}

	FOnDreamShaderSourceGenerated& OnDreamShaderSourceGenerated()
	{
		return DreamShaderCompilerServiceDetail::GDreamShaderSourceGeneratedDelegate;
	}
}

namespace UE::DreamShader::Editor::Private
{
	void RaiseGenerationWarning(const TCHAR* Code, const FString& Message)
	{
		// Logged and queued. Never fails a compile: these are advisory by definition.
		const FString Line = FString::Printf(TEXT("%s: %s"), Code, *Message); /* I18N-EXEMPT: wire form `DSHnnnn: message` */
		UE_LOG(LogDreamShader, Warning, TEXT("%s"), *Line);
		::UE::DreamShader::Editor::DreamShaderCompilerServiceDetail::GetCollectedWarningLines().AddUnique(Line);
	}
}

namespace UE::DreamShader::Editor::Compiler
{
	const TArray<FString>& GetDreamShaderCollectedGenerationWarnings()
	{
		return ::UE::DreamShader::Editor::DreamShaderCompilerServiceDetail::GetCollectedWarningLines();
	}

	bool GetDreamShaderLastCompileDiagnostics(const FString& SourceFilePath, TArray<FLang2DiagnosticRecord>& OutRecords)
	{
		OutRecords.Reset();
		const TArray<FLang2DiagnosticRecord>* const Found =
			::UE::DreamShader::Editor::DreamShaderCompilerServiceDetail::GetLastCompileDiagnosticRecords().Find(
				::UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
		if (!Found)
		{
			return false;
		}
		OutRecords = *Found;
		return true;
	}

	bool CompileDreamShaderSourceFile(
		const FString& SourceFilePath,
		const bool bForce,
		const ::UE::DreamShader::EThinCustomPersistence Persistence,
		::UE::DreamShader::FDreamShaderError& OutError)
	{
		::UE::DreamShader::Editor::DreamShaderCompilerServiceDetail::FScopedDreamShaderCompileNotice Notice(SourceFilePath);

		FDreamShaderLang2PipelineOptions Options;
		Options.bForce = bForce;
		Options.bEmitAssets = true;
		Options.ThinCustomPersistence = Persistence;

		bool bSucceeded = false;
		{
			// Scoped so the run's owned state -- the parsed and bound modules and the IR -- is gone before the
			// notice fires: a subscriber reads assets, never this run's intermediate products.
			FDreamShaderLang2PipelineResult Result;
			const bool bPipelineSucceeded = RunDreamShaderLang2Pipeline(SourceFilePath, Options, Result);

			// Kept as records for GetDreamShaderLastCompileDiagnostics before the report below flattens them into text:
			// each keeps its code, stage, severity and span.
			TArray<FLang2DiagnosticRecord> Records;
			BuildLang2DiagnosticRecords(Result.Diagnostics, Result.SourceFilePath.IsEmpty() ? SourceFilePath : Result.SourceFilePath, Records);
			::UE::DreamShader::Editor::DreamShaderCompilerServiceDetail::GetLastCompileDiagnosticRecords().Add(
				::UE::DreamShader::NormalizeSourceFilePath(SourceFilePath),
				MoveTemp(Records));

			static const TArray<FString> NoCollectedWarnings;
			bSucceeded = WordDreamShaderPipelineReport(
				Result,
				bPipelineSucceeded,
				Notice.IsOutermost() ? GetDreamShaderCollectedGenerationWarnings() : NoCollectedWarnings,
				OutError);
		}

		Notice.Report(bSucceeded);
		return bSucceeded;
	}

	// ------------------------------------------------------------------------------------ DSH9011

	FDreamShaderShaderCompileStallWatch::FDreamShaderShaderCompileStallWatch(FString InLabel)
		: Label(MoveTemp(InLabel))
		, StartSeconds(FPlatformTime::Seconds())
	{
	}

	FDreamShaderShaderCompileStallWatch::~FDreamShaderShaderCompileStallWatch()
	{
		const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
		if (!Private::ShouldWarnOnShaderCompileStall(ElapsedSeconds, bWarned))
		{
			return;
		}

		bWarned = true;
		Private::RaiseGenerationWarning(TEXT("DSH9011"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("Compiling shaders for '%s' took %.0f seconds. A stall of this length is almost always a Custom node whose loop bound is an input (a 'for' or 'while' whose limit is not a literal or a #define) combined with implicit-mip texture sampling -- Texture2DSample / Texture3DSample / .Sample inside divergent flow -- which forces the compiler to fully unroll an iteration count it cannot know. To confirm it is still working rather than hung, check whether ShaderCompileWorker.exe is busy in Task Manager. To fix it, bound the loop with a literal or a #define, or switch the samples to SampleLevel."),
			*Label,
			ElapsedSeconds));
	}

	// ------------------------------------------------------------------------------------ DSH9012

	void ReportDreamShaderCustomNodeLoopHeuristic(const UMaterialExpressionCustom* CustomExpression, const FString& NodeLabel)
	{
		if (!CustomExpression)
		{
			return;
		}

		TArray<FString> InputNames;
		InputNames.Reserve(CustomExpression->Inputs.Num());
		for (const FCustomInput& Input : CustomExpression->Inputs)
		{
			InputNames.Add(Input.InputName.ToString());
		}

		Private::FDreamShaderDynamicLoopSampleFinding Finding;
		if (!Private::ScanCustomCodeForDynamicLoopSampling(CustomExpression->Code, InputNames, Finding))
		{
			return;
		}

		Private::RaiseGenerationWarning(TEXT("DSH9012"), FString::Printf( /* I18N-EXEMPT: deferred codegen or compatibility path */
			TEXT("'%s' loops on the input '%s' and samples with '%s', which takes its mip level from screen-space derivatives. The shader compiler cannot know how many iterations to expect, so it fully unrolls the loop to keep the derivatives defined, and compilation can take minutes. Bound the loop with a literal or a #define, or call SampleLevel / SampleGrad instead."),
			*NodeLabel,
			*Finding.LoopBoundName,
			*Finding.SampleCall));
	}

	// ------------------------------------------------------------------------------------ the service

	namespace
	{
		::UE::DreamShader::FDreamShaderCompileResult RunDreamShaderCompilerServiceRequest(const ::UE::DreamShader::FDreamShaderCompileRequest& Request)
		{
			::UE::DreamShader::FDreamShaderError Error;

			::UE::DreamShader::FDreamShaderCompileResult Result;
			Result.bSucceeded = CompileDreamShaderSourceFile(Request.SourceFilePath, Request.bForce, Request.ThinCustomPersistence, Error);
			Result.Message = FText::FromString(Error.Message);
			if (!Result.bSucceeded)
			{
				Result.Code = Error.Code;
			}
			return Result;
		}
	}

	FDreamShaderCompilerService& FDreamShaderCompilerService::Get()
	{
		static FDreamShaderCompilerService Service;
		return Service;
	}

	::UE::DreamShader::FDreamShaderCompileResult FDreamShaderCompilerService::CompileAssets(const ::UE::DreamShader::FDreamShaderCompileRequest& Request)
	{
		return RunDreamShaderCompilerServiceRequest(Request);
	}

	::UE::DreamShader::FDreamShaderCompileResult FDreamShaderCompilerService::CompileMaterial(const ::UE::DreamShader::FDreamShaderCompileRequest& Request)
	{
		// The same compile as CompileAssets. A 2.0 source is one compile unit: its material may call the
		// functions the same file exports, which are emitted first, and a legacy `.dsm` has one product. The
		// 1.x facade already routed both entry points of a `.dss` to the one compile; the split survives for
		// the preview, which asks for "the material" and reads it back through product resolution.
		return RunDreamShaderCompilerServiceRequest(Request);
	}
}

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	bool IsMemoryOnlyMaterial(UMaterialInterface* Material)
	{
		// PKG_NewlyCreated alone is not "never saved": FAssetRegistryModule::AssetCreated sets it on whatever package it
		// is handed, a saved one included (PublishGeneratedIRAsset puts that right, and this is the belt to its braces).
		// The file is what decides.
		UPackage* Package = Material ? Material->GetPackage() : nullptr;
		return Package && Package->HasAnyPackageFlags(PKG_NewlyCreated) && !FPackageName::DoesPackageExist(Package->GetName());
	}

	UMaterialInterface* MaterializeDreamShaderMaterial(UMaterialInterface* Material, FString& OutError)
	{
		if (!IsMemoryOnlyMaterial(Material))
		{
			return Material;
		}

		UDreamShaderMaterialInstance* DreamInstance = Cast<UDreamShaderMaterialInstance>(Material);
		if (!DreamInstance || DreamInstance->SourceFilePath.IsEmpty())
		{
			OutError = LOCTEXT("MaterializeNoSource", "This material is memory-only and has no DreamShader source file to materialize from.").ToString();
			return nullptr;
		}

		const FString ObjectPath = Material->GetPathName();

		// Materialized, and forced: this IS the Materialize action (architecture plan v2 section 5.1), and a
		// memory-only build never stamps the hash that would let the compile skip it anyway.
		::UE::DreamShader::FDreamShaderCompileRequest Request;
		Request.SourceFilePath = DreamInstance->SourceFilePath;
		Request.bForce = true;
		Request.ThinCustomPersistence = ::UE::DreamShader::EThinCustomPersistence::Materialized;

		const ::UE::DreamShader::FDreamShaderCompileResult Result =
			::UE::DreamShader::Editor::Compiler::FDreamShaderCompilerService::Get().CompileAssets(Request);
		if (!Result.bSucceeded)
		{
			OutError = FText::Format(LOCTEXT("FactoryMaterializeFailed", "Failed to materialize the material to disk: {0}"), Result.Message).ToString();
			return nullptr;
		}

		UMaterialInterface* Persisted = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
		if (!Persisted)
		{
			OutError = FText::Format(LOCTEXT("FactoryReloadFailed", "Materialized the material but could not reload it at {0}."), FText::FromString(ObjectPath)).ToString();
			return nullptr;
		}
		return Persisted;
	}
}

#undef LOCTEXT_NAMESPACE
