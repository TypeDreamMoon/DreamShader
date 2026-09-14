// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderShaderCheck.h for the design and for why the errors are collected twice over.
//
// EVERY ENGINE API THIS FILE USES, with the header it comes from, gathered here so a signature that
// moved in another engine version fails at one named place rather than thirty:
//
//   Materials/Material.h            UMaterial::BeginCacheForCookedPlatformData(const ITargetPlatform*)
//                                   UMaterial::IsCachedCookedPlatformDataLoaded(const ITargetPlatform*)
//                                   UMaterial::ClearAllCachedCookedPlatformData()
//                                   UMaterial::CacheShaders(EMaterialShaderPrecompileMode)
//                                   UMaterial::GetMaterialResource(EShaderPlatform, EMaterialQualityLevel::Type)
//   MaterialShared.h                FMaterialResource : FMaterial
//                                   FMaterial::GetCompileErrors() const   [WITH_EDITOR, inline]
//                                   FMaterial::GetErrorExpressions() const [WITH_EDITOR, inline]
//   MaterialShaderPrecompileMode.h  EMaterialShaderPrecompileMode::{None, Background, Synchronous, Default}
//   ShaderCompiler.h                GShaderCompilingManager
//                                   FShaderCompilingManager::ProcessAsyncResults(bool, bool)
//                                   FShaderCompilingManager::IsCompiling() const
//                                   FShaderCompilingManager::FinishAllCompilation()
//   SceneTypes.h                    EMaterialQualityLevel::{Low, High, Medium, Epic, Num}  -- NOT in quality order
//                                   LexToString(EMaterialQualityLevel::Type)
//   RHIStrings.h                    ShaderFormatToLegacyShaderPlatform(FName)
//                                   LegacyShaderPlatformToShaderFormat(EShaderPlatform)
//   RHIGlobals.h                    GMaxRHIShaderPlatform, GMaxRHIFeatureLevel
//   Interfaces/ITargetPlatform*.h   GetTargetPlatformManager(), ITargetPlatformManagerModule::
//                                   {GetActiveTargetPlatforms, FindTargetPlatform, FindTargetPlatformWithSupport},
//                                   ITargetPlatform(Settings)::GetAllTargetedShaderFormats(TArray<FName>&)
//   Misc/App.h                      FApp::CanEverRender()
//   Misc/OutputDevice.h             FOutputDevice::Serialize(const TCHAR*, ELogVerbosity::Type, const FName&)
//
// The TargetPlatform module is reached through UnrealEd, which lists it in
// PublicDependencyModuleNames -- so no Build.cs change is needed for the includes below. If that
// ever stops being true, add "TargetPlatform" to DreamShaderEditor.Build.cs.

#include "DreamShaderShaderCheck.h"

// Unit N's parsed `DreamShader.SourceSpans` table. Reused rather than re-parsed here: the guid key
// format (EGuidFormats::DigitsWithHyphens) and the field names are unit E's choice, and a second
// reader of that JSON is a second place for them to drift.
#include "DreamShaderSourceNavigation.h"

#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"

#include "HAL/CriticalSection.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialShared.h"
#include "MaterialShaderPrecompileMode.h"
#include "Misc/App.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "RHIGlobals.h"
#include "RHIStrings.h"
#include "SceneTypes.h"
#include "ShaderCompiler.h"

#define LOCTEXT_NAMESPACE "DreamShader.Tools"

namespace UE::DreamShader::Editor::Compiler
{
	using UE::DreamShader::Lang::FLangDiagnosticSink;
	using UE::DreamShader::Lang::FLangSpan;

	namespace
	{
		/** Where a shader error was traced back to. Line/Column are 1-based; Length may be 0. */
		struct FShaderErrorLocation
		{
			FString FilePath;
			int32 Line = 1;
			int32 Column = 1;
			int32 Length = 0;
			bool bResolved = false;
		};

		/**
		 * Captures the engine's own shader-error log for the duration of one check.
		 *
		 * Needed because the COOK path's FMaterialResources live in UMaterial's private
		 * CachedMaterialResourcesForCooking and there is no accessor: their errors reach the log and
		 * nowhere else. Registered on GLog, so Serialize is called from the shader compiler's worker
		 * threads as well as the game thread -- hence the lock.
		 */
		class FDreamShaderShaderLogCapture : public FOutputDevice
		{
		public:
			FDreamShaderShaderLogCapture()
			{
				if (GLog)
				{
					GLog->AddOutputDevice(this);
				}
			}

			virtual ~FDreamShaderShaderLogCapture() override
			{
				if (GLog)
				{
					GLog->RemoveOutputDevice(this);
				}
			}

			FDreamShaderShaderLogCapture(const FDreamShaderShaderLogCapture&) = delete;
			FDreamShaderShaderLogCapture& operator=(const FDreamShaderShaderLogCapture&) = delete;

			virtual void Serialize(const TCHAR* V, const ELogVerbosity::Type Verbosity, const FName& Category) override
			{
				// Masked: a verbosity arrives with BreakOnLog / SetColor flags ORed in, and an
				// unmasked comparison silently drops every line that carries one.
				if ((Verbosity & ELogVerbosity::VerbosityMask) > ELogVerbosity::Warning || V == nullptr)
				{
					return;
				}

				static const FName LogMaterialName(TEXT("LogMaterial"));
				static const FName LogShaderCompilersName(TEXT("LogShaderCompilers"));
				static const FName LogShadersName(TEXT("LogShaders"));
				if (Category != LogMaterialName && Category != LogShaderCompilersName && Category != LogShadersName)
				{
					return;
				}

				FScopeLock Lock(&Mutex);
				Lines.AddUnique(FString(V).TrimStartAndEnd());
			}

			// A log device must not be flushed away with the rest of them mid-run.
			virtual bool CanBeUsedOnAnyThread() const override { return true; }
			virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

			TArray<FString> TakeLines()
			{
				FScopeLock Lock(&Mutex);
				TArray<FString> Result = MoveTemp(Lines);
				Lines.Reset();
				return Result;
			}

		private:
			FCriticalSection Mutex;
			TArray<FString> Lines;
		};

		bool TryParseQualityToken(const FString& Token, EMaterialQualityLevel::Type& OutQuality)
		{
			// Spelled out rather than looped over LexToString: the enumerator ORDER is
			// Low, High, Medium, Epic (SceneTypes.h), so anything that assumes the numeric order is
			// the quality order silently maps "Medium" to High.
			const FString Trimmed = Token.TrimStartAndEnd();
			if (Trimmed.Equals(TEXT("Low"), ESearchCase::IgnoreCase))    { OutQuality = EMaterialQualityLevel::Low;    return true; }
			if (Trimmed.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) { OutQuality = EMaterialQualityLevel::Medium; return true; }
			if (Trimmed.Equals(TEXT("High"), ESearchCase::IgnoreCase))   { OutQuality = EMaterialQualityLevel::High;   return true; }
			if (Trimmed.Equals(TEXT("Epic"), ESearchCase::IgnoreCase))   { OutQuality = EMaterialQualityLevel::Epic;   return true; }
			return false;
		}

		/** `SM6` -> `PCD3D_SM6`; anything unrecognised is passed through as a shader-format name. */
		FName ShaderFormatNameForToken(const FString& Token)
		{
			const FString Trimmed = Token.TrimStartAndEnd();
			if (Trimmed.Equals(TEXT("SM6"), ESearchCase::IgnoreCase))   { return FName(TEXT("PCD3D_SM6")); }
			if (Trimmed.Equals(TEXT("SM5"), ESearchCase::IgnoreCase))   { return FName(TEXT("PCD3D_SM5")); }
			if (Trimmed.Equals(TEXT("ES3_1"), ESearchCase::IgnoreCase)
				|| Trimmed.Equals(TEXT("ES31"), ESearchCase::IgnoreCase)) { return FName(TEXT("PCD3D_ES3_1")); }
			if (Trimmed.Equals(TEXT("VULKAN_SM6"), ESearchCase::IgnoreCase)) { return FName(TEXT("SF_VULKAN_SM6")); }
			if (Trimmed.Equals(TEXT("VULKAN_SM5"), ESearchCase::IgnoreCase)) { return FName(TEXT("SF_VULKAN_SM5")); }
			if (Trimmed.Equals(TEXT("METAL_SM5"), ESearchCase::IgnoreCase))  { return FName(TEXT("SF_METAL_SM5")); }
			return FName(*Trimmed);
		}

		FString ShaderPlatformLabel(const EShaderPlatform Platform)
		{
			return LegacyShaderPlatformToShaderFormat(Platform).ToString();
		}

		/** One requested platform, resolved as far as it could be. */
		struct FResolvedShaderTarget
		{
			FString Token;
			FName ShaderFormat;
			EShaderPlatform ShaderPlatform = SP_NumPlatforms;
			/** The cook target that owns this format, when one could be found. Null means "read-only check". */
			ITargetPlatform* TargetPlatform = nullptr;
		};

		ITargetPlatform* FindTargetPlatformForFormat(const FName ShaderFormat, const FString& Token)
		{
			ITargetPlatformManagerModule* Manager = GetTargetPlatformManager(/*bFailOnInitErrors*/ false);
			if (!Manager)
			{
				return nullptr;
			}

			// A token that names a platform outright ("Windows") wins: it is unambiguous and it is
			// what somebody typing a platform name meant.
			if (ITargetPlatform* Named = Manager->FindTargetPlatform(*Token))
			{
				return Named;
			}

			// Otherwise the first active platform that targets this shader format. Active rather than
			// all, so a check does not silently start compiling for a platform the project does not
			// build for.
			for (ITargetPlatform* Platform : Manager->GetActiveTargetPlatforms())
			{
				if (!Platform)
				{
					continue;
				}

				TArray<FName> Formats;
				Platform->GetAllTargetedShaderFormats(Formats);
				if (Formats.Contains(ShaderFormat))
				{
					return Platform;
				}
			}

			return nullptr;
		}

		/**
		 * Maps a 1-based line of one Custom node's code back to the source it came from, by counting
		 * lines inside each `// Begin DreamShader source: <path>` ... `// End DreamShader source:`
		 * block -- the same walk DreamShaderMaterialGeneratorDiagnostics.cpp does over a prepared
		 * source, and the reason both the preprocessor and the import inliner must conserve lines.
		 */
		bool MapCustomCodeLineToSource(const FString& Code, const int32 CodeLine, FShaderErrorLocation& OutLocation)
		{
			static const TCHAR* const BeginMarker = TEXT("// Begin DreamShader source: ");
			static const TCHAR* const EndMarker = TEXT("// End DreamShader source: ");

			TArray<FString> Lines;
			Code.ParseIntoArrayLines(Lines, false);

			FString CurrentFile;
			int32 CurrentSourceLine = 0;
			for (int32 Index = 0; Index < Lines.Num(); ++Index)
			{
				const FString Trimmed = Lines[Index].TrimStart();
				if (Trimmed.StartsWith(BeginMarker))
				{
					CurrentFile = Trimmed.RightChop(FCString::Strlen(BeginMarker)).TrimStartAndEnd();
					CurrentSourceLine = 1;
					continue;
				}
				if (Trimmed.StartsWith(EndMarker))
				{
					CurrentFile.Reset();
					CurrentSourceLine = 0;
					continue;
				}

				// A marker line never advances the count: it does not exist in the file it names.
				if (Index + 1 == CodeLine && !CurrentFile.IsEmpty())
				{
					OutLocation.FilePath = CurrentFile;
					OutLocation.Line = FMath::Max(1, CurrentSourceLine);
					OutLocation.Column = 1;
					OutLocation.Length = 0;
					OutLocation.bResolved = true;
					return true;
				}

				if (!CurrentFile.IsEmpty())
				{
					++CurrentSourceLine;
				}
			}

			return false;
		}

		/** `<path>(<line>,<column>): <rest>` -- the shape both the engine and DreamShader emit. */
		bool TryParseLocatedError(const FString& Error, FString& OutPath, int32& OutLine, int32& OutColumn)
		{
			int32 Separator = INDEX_NONE;
			if (!Error.FindChar(TCHAR(')'), Separator))
			{
				return false;
			}

			const int32 Open = Error.Left(Separator).Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (Open == INDEX_NONE)
			{
				return false;
			}

			const FString Path = Error.Left(Open).TrimStartAndEnd();
			const FString Inside = Error.Mid(Open + 1, Separator - Open - 1);
			if (Path.IsEmpty() || Inside.IsEmpty())
			{
				return false;
			}

			FString LineText;
			FString ColumnText;
			if (!Inside.Split(TEXT(","), &LineText, &ColumnText))
			{
				LineText = Inside;
				ColumnText = TEXT("1");
			}

			LineText.TrimStartAndEndInline();
			ColumnText.TrimStartAndEndInline();
			if (!LineText.IsNumeric() || !ColumnText.IsNumeric())
			{
				return false;
			}

			OutPath = Path;
			OutLine = FMath::Max(1, FCString::Atoi(*LineText));
			OutColumn = FMath::Max(1, FCString::Atoi(*ColumnText));
			return true;
		}

		/** Best-effort: the span table, then the custom-code markers, then the source at line 1. */
		FShaderErrorLocation LocateShaderError(
			const FString& RawError,
			UMaterial* Material,
			UMaterialExpression* ErrorExpression,
			const Private::FDreamShaderSourceSpanTable& Spans,
			const FString& FallbackSourceFilePath)
		{
			if (ErrorExpression)
			{
				if (const Private::FDreamShaderSourceSpan* Found = Spans.FindForExpression(ErrorExpression))
				{
					FShaderErrorLocation Location;
					Location.FilePath = Found->GetAbsoluteFilePath();
					Location.Line = Found->Line;
					Location.Column = Found->Column;
					Location.Length = Found->Length;
					Location.bResolved = Found->IsValid();
					return Location;
				}
			}

			FString ParsedPath;
			int32 ParsedLine = 1;
			int32 ParsedColumn = 1;
			const bool bLocated = TryParseLocatedError(RawError, ParsedPath, ParsedLine, ParsedColumn);

			// An error the engine already attributed to a DreamShader file needs nothing from us.
			if (bLocated && UE::DreamShader::IsDreamShaderSourceFile(ParsedPath))
			{
				FShaderErrorLocation Location;
				Location.FilePath = UE::DreamShader::NormalizeSourceFilePath(ParsedPath);
				Location.Line = ParsedLine;
				Location.Column = ParsedColumn;
				Location.bResolved = true;
				return Location;
			}

			if (bLocated && Material)
			{
				// The line is a line of generated HLSL, so this only lands when the generated file IS
				// a custom node's code -- which is the case the `@custom` author cares about.
				for (UMaterialExpression* Expression : Material->GetExpressions())
				{
					const UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
					if (!Custom || Custom->Code.IsEmpty())
					{
						continue;
					}

					FShaderErrorLocation Location;
					if (MapCustomCodeLineToSource(Custom->Code, ParsedLine, Location))
					{
						Location.Column = ParsedColumn;
						return Location;
					}
				}
			}

			FShaderErrorLocation Fallback;
			Fallback.FilePath = FallbackSourceFilePath;
			Fallback.Line = 1;
			Fallback.Column = 1;
			Fallback.bResolved = false;
			return Fallback;
		}
	}

	bool ParseDreamShaderShaderCheckOptions(
		const FString& PlatformValue,
		const FString& QualityValue,
		const FString& TimeoutValue,
		FDreamShaderShaderCheckOptions& OutOptions,
		FLangDiagnosticSink& Diagnostics)
	{
		const FLangSpan NoSpan;
		bool bOk = true;

		if (!PlatformValue.IsEmpty())
		{
			PlatformValue.ParseIntoArray(OutOptions.PlatformTokens, TEXT(","), true);
			for (FString& Token : OutOptions.PlatformTokens)
			{
				Token.TrimStartAndEndInline();
				if (ShaderFormatToLegacyShaderPlatform(ShaderFormatNameForToken(Token)) == SP_NumPlatforms)
				{
					bOk = Diagnostics.Error(TEXT("DSH9026"), NoSpan, FText::Format(
						LOCTEXT("UnknownShaderPlatform", "'{0}' is not a shader platform this engine knows. Write SM6, SM5, ES3_1, or a shader format name such as PCD3D_SM6."),
						FText::FromString(Token)));
				}
			}
		}

		if (!QualityValue.IsEmpty())
		{
			TArray<FString> Tokens;
			QualityValue.ParseIntoArray(Tokens, TEXT(","), true);
			for (FString& Token : Tokens)
			{
				Token.TrimStartAndEndInline();
				EMaterialQualityLevel::Type Quality = EMaterialQualityLevel::Num;
				if (!TryParseQualityToken(Token, Quality))
				{
					bOk = Diagnostics.Error(TEXT("DSH9027"), NoSpan, FText::Format(
						LOCTEXT("UnknownQualityLevel", "'{0}' is not a material quality level. Write Low, Medium, High or Epic."),
						FText::FromString(Token)));
					continue;
				}
				OutOptions.QualityTokens.Add(Token);
			}
		}

		if (!TimeoutValue.IsEmpty())
		{
			const double Seconds = FCString::Atod(*TimeoutValue);
			if (Seconds > 0.0)
			{
				OutOptions.TimeoutSeconds = Seconds;
			}
		}

		return bOk;
	}

	bool CheckDreamShaderShaders(
		const FDreamShaderLang2PipelineResult& Compiled,
		const FDreamShaderShaderCheckOptions& Options,
		FLangDiagnosticSink& Diagnostics,
		FDreamShaderShaderCheckStats& OutStats)
	{
		const FLangSpan NoSpan;

		// ------------------------------------------------------------------ resolve the targets

		TArray<FResolvedShaderTarget> Targets;
		for (const FString& Token : Options.PlatformTokens)
		{
			FResolvedShaderTarget Target;
			Target.Token = Token;
			Target.ShaderFormat = ShaderFormatNameForToken(Token);
			Target.ShaderPlatform = ShaderFormatToLegacyShaderPlatform(Target.ShaderFormat);
			Target.TargetPlatform = FindTargetPlatformForFormat(Target.ShaderFormat, Token);
			Targets.Add(MoveTemp(Target));
		}

		if (Targets.IsEmpty())
		{
			// No -Platform: the host's own shader platform, which is what an editor would compile.
			FResolvedShaderTarget Target;
			Target.ShaderPlatform = GMaxRHIShaderPlatform;
			Target.ShaderFormat = LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform);
			Target.Token = Target.ShaderFormat.ToString();
			Target.TargetPlatform = FindTargetPlatformForFormat(Target.ShaderFormat, Target.Token);
			Targets.Add(MoveTemp(Target));
		}

		OutStats.PlatformsRequested = Targets.Num();

		TArray<EMaterialQualityLevel::Type> Qualities;
		for (const FString& Token : Options.QualityTokens)
		{
			EMaterialQualityLevel::Type Quality = EMaterialQualityLevel::Num;
			if (TryParseQualityToken(Token, Quality))
			{
				Qualities.AddUnique(Quality);
			}
		}
		if (Qualities.IsEmpty())
		{
			// Num means "the project's current scalability level" to GetMaterialResource, which is
			// what an editor shows and therefore the right default for a gate.
			Qualities.Add(EMaterialQualityLevel::Num);
		}

		// ------------------------------------------------------------------ collect the materials

		TArray<UMaterial*> Materials;
		for (const TWeakObjectPtr<UObject>& Weak : Compiled.ProductAssets)
		{
			if (UMaterial* Material = Cast<UMaterial>(Weak.Get()))
			{
				Materials.Add(Material);
			}
		}

		if (Materials.IsEmpty())
		{
			Diagnostics.Info(TEXT("DSH9037"), NoSpan, FText::Format(
				LOCTEXT("NoMaterialToCheck", "'{0}' produced no material, so there are no shaders to compile; a function library is checked by the material that calls it."),
				FText::FromString(FPaths::GetCleanFilename(Compiled.SourceFilePath))));
			return true;
		}

		OutStats.MaterialsChecked = Materials.Num();

		// --------------------------------------------------------------------------- compile

		FDreamShaderShaderLogCapture LogCapture;
		const bool bCanReadRenderingResources = FApp::CanEverRender();

		// Every material is STARTED before anything is pumped. The engine's own
		// CompileShadersTestBedCommandlet says why in as many words: pumping between two Begin calls
		// lets a half-built shader map be treated as finished and pushed to the DDC.
		for (UMaterial* Material : Materials)
		{
			if (bCanReadRenderingResources)
			{
				// The rendering half, which is the only one whose FMaterialResources are reachable
				// through a public accessor. Synchronous so the translation errors are in place
				// before the wait below rather than after it.
				Material->CacheShaders(EMaterialShaderPrecompileMode::Synchronous);
			}

			for (const FResolvedShaderTarget& Target : Targets)
			{
				if (Target.TargetPlatform)
				{
					Material->BeginCacheForCookedPlatformData(Target.TargetPlatform);
				}
			}
		}

		// ------------------------------------------------------------------------------ wait

		const double StartSeconds = FPlatformTime::Seconds();
		const double Deadline = StartSeconds + Options.TimeoutSeconds * static_cast<double>(Materials.Num());
		bool bTimedOut = false;

		for (;;)
		{
			bool bAllLoaded = true;
			for (UMaterial* Material : Materials)
			{
				for (const FResolvedShaderTarget& Target : Targets)
				{
					if (Target.TargetPlatform && !Material->IsCachedCookedPlatformDataLoaded(Target.TargetPlatform))
					{
						bAllLoaded = false;
						break;
					}
				}
				if (!bAllLoaded)
				{
					break;
				}
			}

			if (bAllLoaded && GShaderCompilingManager && !GShaderCompilingManager->IsCompiling())
			{
				break;
			}

			if (FPlatformTime::Seconds() > Deadline)
			{
				bTimedOut = true;
				break;
			}

			if (GShaderCompilingManager)
			{
				// bLimitExecutionTime false: this loop has nothing else to do and a time-sliced pump
				// would only make the wait longer. bBlockOnGlobalShaderCompletion false: the global
				// shader map is not this gate's business and blocking on it can deadlock a run that
				// never asked for one.
				GShaderCompilingManager->ProcessAsyncResults(/*bLimitExecutionTime*/ false, /*bBlockOnGlobalShaderCompletion*/ false);
			}
			else
			{
				break;
			}
		}

		if (!bTimedOut && GShaderCompilingManager)
		{
			// Only once the pump says the queue is drained: FinishAllCompilation blocks with no
			// deadline of its own, so calling it on a timed-out run would turn a reported timeout
			// back into the hang the timeout exists to prevent.
			GShaderCompilingManager->FinishAllCompilation();
		}

		// ---------------------------------------------------------------------------- report

		int32 ErrorCount = 0;
		TSet<FString> Seen;

		auto Report = [&Diagnostics, &ErrorCount, &Seen, &NoSpan](
			const FString& RawError,
			const FShaderErrorLocation& Location,
			const FString& PlatformLabel,
			const FString& QualityLabel,
			const FString& AssetPath)
		{
			const FString Key = FString::Printf(TEXT("%s|%s|%s|%s|%d"), *AssetPath, *PlatformLabel, *QualityLabel, *RawError, Location.Line);
			if (Seen.Contains(Key))
			{
				return;
			}
			Seen.Add(Key);
			++ErrorCount;

			// The span is fabricated from the mapped location rather than left empty: the sink keys
			// its records on FLangSpan, and a diagnostic that lands on line 1 of the right file is
			// far more use than one that lands nowhere.
			FLangSpan Span = NoSpan;
			Span.Line = Location.Line;
			Span.Column = Location.Column;
			Span.Length = Location.Length;

			Diagnostics.Error(TEXT("DSH9029"), Span, FText::Format(
				LOCTEXT("ShaderCompileError", "[{0} / {1}] {2}"),
				FText::FromString(PlatformLabel),
				FText::FromString(QualityLabel),
				FText::FromString(RawError)));
		};

		for (UMaterial* Material : Materials)
		{
			// No sink passed: a malformed table is unit N's DSH9053 to raise when navigation needs
			// it, and a shader gate that failed because the SPAN TABLE was bad would be reporting
			// the wrong problem.
			const Private::FDreamShaderSourceSpanTable Spans =
				Private::FDreamShaderSourceSpanTable::LoadFromAsset(Material);
			const FString AssetPath = Material->GetPathName();

			for (const FResolvedShaderTarget& Target : Targets)
			{
				const FString PlatformLabel = Target.ShaderPlatform != SP_NumPlatforms
					? ShaderPlatformLabel(Target.ShaderPlatform)
					: Target.Token;

				for (const EMaterialQualityLevel::Type Quality : Qualities)
				{
					// `::` qualified: LexToString(EMaterialQualityLevel::Type) is the engine's, at
					// global scope (SceneTypes.h), and the IR namespace has LexToString overloads of
					// its own that an unqualified call could reach first through a using-directive.
					const FString QualityLabel = ::LexToString(Quality);

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 7)
					const FMaterialResource* Resource = Target.ShaderPlatform != SP_NumPlatforms
						? Material->GetMaterialResource(Target.ShaderPlatform, Quality)
						: nullptr;
#else
					const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIFeatureLevel, Quality);
#endif
					if (!Resource)
					{
						continue;
					}

#if WITH_EDITOR
					const TArray<FString>& CompileErrors = Resource->GetCompileErrors();
					const TArray<UMaterialExpression*>& ErrorExpressions = Resource->GetErrorExpressions();
					for (int32 Index = 0; Index < CompileErrors.Num(); ++Index)
					{
						const FString RawError = CompileErrors[Index].TrimStartAndEnd();
						if (RawError.IsEmpty())
						{
							continue;
						}

						// Index-parallel only for TRANSLATOR errors: a backend failure assigns the
						// whole CompileErrors array without touching ErrorExpressions, so the two
						// desync and the engine's own reader guards on the length exactly like this.
						UMaterialExpression* ErrorExpression =
							(ErrorExpressions.Num() == CompileErrors.Num()) ? ErrorExpressions[Index] : nullptr;

						Report(
							RawError,
							LocateShaderError(RawError, Material, ErrorExpression, Spans, Compiled.SourceFilePath),
							PlatformLabel,
							QualityLabel,
							AssetPath);
					}
#endif
				}
			}

			Material->ClearAllCachedCookedPlatformData();
		}

		// The log half. Only lines that name a material we were checking survive, so an unrelated
		// warning from another asset's background compile cannot fail this gate.
		for (const FString& Line : LogCapture.TakeLines())
		{
			bool bMentionsOurs = false;
			for (const UMaterial* Material : Materials)
			{
				if (Line.Contains(Material->GetName(), ESearchCase::CaseSensitive))
				{
					bMentionsOurs = true;
					break;
				}
			}

			if (!bMentionsOurs || !Line.Contains(TEXT("error"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			FShaderErrorLocation Location;
			Location.FilePath = Compiled.SourceFilePath;
			Report(Line, Location, TEXT("log"), TEXT("-"), FString());
		}

		if (bTimedOut)
		{
			OutStats.bTimedOut = true;
			Diagnostics.Error(TEXT("DSH9028"), NoSpan, FText::Format(
				LOCTEXT("ShaderCompileTimedOut", "Shader compilation for '{0}' did not finish within {1} seconds per material. A compile that never finishes is usually a dynamic loop or a texture read whose mip cannot be resolved in a divergent branch; move it into a '@custom' body with an explicit SampleLevel."),
				FText::FromString(FPaths::GetCleanFilename(Compiled.SourceFilePath)),
				FText::AsNumber(Options.TimeoutSeconds)));
		}

		if (!bCanReadRenderingResources && !Targets.ContainsByPredicate([](const FResolvedShaderTarget& Target) { return Target.TargetPlatform != nullptr; }))
		{
			OutStats.bErrorsUnreadable = true;
			Diagnostics.Warning(TEXT("DSH9038"), NoSpan, LOCTEXT("ShaderErrorsUnreadable",
				"Shader errors cannot be read in this configuration: '-nullrhi' switches the rendering shader maps off, and no cook target platform matched the requested platforms. Re-run without '-nullrhi', or pass a '-Platform=' an active target platform supports."));
		}

		OutStats.ShaderErrorsReported = ErrorCount;
		return ErrorCount == 0 && !bTimedOut;
	}
}

#undef LOCTEXT_NAMESPACE
