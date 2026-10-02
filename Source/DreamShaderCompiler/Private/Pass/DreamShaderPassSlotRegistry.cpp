// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderPassSlotRegistry.h.
//
// Diagnostics owned by this file: DSH8315-DSH8329.
//
// THE PRE-CHECK, and why it is built the way it is (DreamShader_Plan/04 section 3, SP-3).
//
// A global shader that fails to compile is fatal: ProcessCompiledGlobalShaders offers a retry box and then exits, and a
// teammate who pulls a registry that does not compile cannot start the editor. So a slot's new snapshot is compiled
// BEFORE anything is written, through a path that only returns errors:
//
//   * the job is our own FShaderCompileJob, never handed to GShaderCompilingManager -- PrepareShaderCompileJob would
//     register it, and SubmitJobs would route a failure to the global shader map's error handling;
//   * it is filled the way FGlobalShaderTypeCompiler::BeginCompileShader fills a global shader's
//     (E/Private/ShaderCompiler/ShaderCompilerEditor.cpp, PrepareGlobalShaderCompileJob): the type's own
//     SetupCompileEnvironment for the slot's permutation, then GlobalBeginCompileShader -- so the defines, the uniform
//     buffer declarations and the compiler flags are the real compile's;
//   * the source is in memory: the root is a copy of the slot shader (DreamPassCompute.usf / DreamPassPixel.usf) whose
//     registry include points at an in-memory registry holding this slot's candidate section, which includes the
//     candidate snapshot, all under `/DreamPassUser/Precheck/`. An in-memory root has no bulk preprocess dependencies
//     (RC/Private/ShaderCore.cpp, GetShaderPreprocessDependencies answers false), so every include goes through the
//     preprocessor's slow path, which consults FShaderCompilerEnvironment::IncludeVirtualPathToContentsMap first
//     (Developer/ShaderPreprocessor/Private/ShaderPreprocessor.cpp, StbResolveInclude / StbLoadFile). Overriding the
//     real root's registry in that map would do nothing: a file in the bulk dependencies is read from disk first;
//   * it is preprocessed and compiled in-process with RenderCore's PreprocessShader and CompileShader
//     (RC/Public/ShaderCompilerCore.h), the functions a shader compile worker runs, through the platform's
//     IShaderFormat. Errors land in the job's Output.Errors; nothing is cached, nothing reaches a shader map;
//   * CFLAG_DisableSourceStripping keeps the `#line` directives, so an error names the in-memory path and the line in
//     it, which maps back to the user's file -- no remapping table to replay.
//
// Its limits, said here and in the docs: only the formats this machine has a compiler for are checked (the others are
// warned about); an include of a live user path is compiled as it is now and can change later; and a uniform buffer
// that no slot shader referenced before is declared for the real compile (UpdateReferencedUniformBufferNames runs
// before it) but not for the pre-check, which then refuses a valid shader -- the safe direction.

#include "Pass/DreamShaderPassSlotRegistry.h"

#include "DreamPassPipeline.h"
#include "DreamPassTypes.h"
#include "DreamShaderPassModule.h"

#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
// GetTargetPlatformManager / GetTargetPlatformManagerRef.
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if DREAMSHADER_WITH_CUSTOM_PASS
#include "GlobalShader.h"
#include "Interfaces/IShaderFormat.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"
#include "RHIStrings.h"
#include "Shader.h"
#include "ShaderCompiler.h"
#include "ShaderCompilerCore.h"
#include "ShaderCompilerJobTypes.h"
#include "ShaderCore.h"
#endif

#define LOCTEXT_NAMESPACE "DreamShader.Pass.Slots"

namespace UE::DreamShader::Editor::Compiler
{
	namespace DreamPassSlotRegistryDetail
	{
		constexpr int32 RegistrySchemaVersion = 1;
		const TCHAR* const RegistrySchemaName = TEXT("dreamshader-pass-registry");

		/** The types Render/DreamPassGlobalShaders.cpp registers, at file scope so their names are exactly these. */
		const TCHAR* const ComputeShaderTypeName = TEXT("FDreamPassCS");
		const TCHAR* const PixelShaderTypeName = TEXT("FDreamPassPS");

		/** The fixed entries a slot renames a pass's entry to (Shaders/Pass/DreamPassCompute.usf / DreamPassPixel.usf). */
		const TCHAR* GetMainEntryName(const bool bCompute)
		{
			return bCompute ? TEXT("DreamPassMainCS") : TEXT("DreamPassMainPS");
		}

		/** `/DreamPassUser/RegistryCompute.ush`: the include the slot shaders spell. */
		FString GetRegistryVirtualPath(const bool bCompute)
		{
			return ::UE::DreamPass::GetUserShaderVirtualDirectory() / FPaths::GetCleanFilename(::UE::DreamPass::GetRegistryFilePath(bCompute));
		}

		/** Where the pre-check's in-memory files live: under the user directory, beside nothing that exists on disk. */
		FString GetPrecheckVirtualDirectory()
		{
			return ::UE::DreamPass::GetUserShaderVirtualDirectory() / TEXT("Precheck");
		}

		bool IsIdentifier(const FString& Name)
		{
			if (Name.IsEmpty())
			{
				return false;
			}
			for (int32 Index = 0; Index < Name.Len(); ++Index)
			{
				const TCHAR Character = Name[Index];
				const bool bLetter = (Character >= TCHAR('A') && Character <= TCHAR('Z')) || (Character >= TCHAR('a') && Character <= TCHAR('z')) || Character == TCHAR('_');
				const bool bDigit = Character >= TCHAR('0') && Character <= TCHAR('9');
				if (!bLetter && !(bDigit && Index > 0))
				{
					return false;
				}
			}
			return true;
		}

		/** `(DP_Params[2].zw)`, `asint(DP_Params[0].y)`, `(asuint(DP_Params[1].x) != 0)`: one packed `param`, read back. */
		FString MakeParamExpression(const FDreamPassSlotParamLocation& Location)
		{
			static const TCHAR* const Components = TEXT("xyzw");
			FString Swizzle;
			switch (Location.Width)
			{
			case 1: Swizzle = FString::Printf(TEXT(".%c"), Components[FMath::Clamp(Location.Component, 0, 3)]); break;
			case 2: Swizzle = Location.Component == 0 ? TEXT(".xy") : TEXT(".zw"); break;
			case 3: Swizzle = TEXT(".xyz"); break;
			default: break;
			}

			const FString Vector = FString::Printf(TEXT("DP_Params[%d]%s"), Location.Vector, *Swizzle); /* I18N-EXEMPT: HLSL */
			switch (Location.Type)
			{
			case EDreamPassParameterType::Int:
				return FString::Printf(TEXT("asint(%s)"), *Vector); /* I18N-EXEMPT: HLSL */
			case EDreamPassParameterType::Bool:
				return FString::Printf(TEXT("(asuint(%s) != 0)"), *Vector); /* I18N-EXEMPT: HLSL */
			default:
				return FString::Printf(TEXT("(%s)"), *Vector); /* I18N-EXEMPT: HLSL */
			}
		}

		/** The name a binding is known by in the pass's HLSL: its slot name, or the buffer's when it has none. */
		FString GetBindingName(const FDreamPassBufferBinding& Binding)
		{
			return Binding.Slot.IsNone() ? Binding.Buffer.ToString() : Binding.Slot.ToString();
		}

		/**
		 * One slot's section, without its `#include` line: the head (`#if`, the marker, the defines) and the tail (the
		 * `#undef`s, `#endif`). The #include is spelled by the caller, because the pre-check includes the same snapshot at
		 * another path.
		 */
		bool BuildSlotSection(
			const FDreamPassSlotCandidate& Candidate,
			const UDreamPassPipeline& Staged,
			const FString& PipelineName,
			FString& OutHead,
			FString& OutTail,
			FText& OutError)
		{
			const FDreamPassDesc& Pass = Staged.Passes[Candidate.PassIndex];
			const bool bCompute = Candidate.bCompute;

			TArray<TPair<FString, FString>> Defines;
			auto Define = [&Defines, &OutError](const FString& Name, const FString& Value) -> bool
			{
				if (!IsIdentifier(Name) || Name.StartsWith(TEXT("DP_"), ESearchCase::CaseSensitive))
				{
					OutError = FText::Format(
						LOCTEXT("SlotNameInvalid", "'{0}' cannot name something in an HLSL slot: a slot name has to be an identifier, and one starting with DP_ would hide one of the slot's fixed parameters."),
						FText::FromString(Name));
					return false;
				}
				// Case-sensitive, as HLSL macros are and as the binder's own check of these names is: a TSet<FString> would take
				// `mask` and `Mask` for one name and refuse a pass that compiles.
				const bool bAlreadyTaken = Defines.ContainsByPredicate([&Name](const TPair<FString, FString>& Existing)
				{
					return Existing.Key.Equals(Name, ESearchCase::CaseSensitive);
				});
				if (bAlreadyTaken)
				{
					OutError = FText::Format(
						LOCTEXT("SlotNameTwice", "'{0}' would be defined twice in the pass's HLSL slot: every read, write and param needs its own name, and a read or a write also takes the names {0}Size and {0}UVRect."),
						FText::FromString(Name));
					return false;
				}
				Defines.Emplace(Name, Value);
				return true;
			};

			if (Pass.Reads.Num() > ::UE::DreamPass::MaxSlotInputs)
			{
				OutError = FText::Format(LOCTEXT("SlotTooManyReads", "The pass reads {0} buffers; an HLSL slot has {1} inputs."), FText::AsNumber(Pass.Reads.Num()), FText::AsNumber(::UE::DreamPass::MaxSlotInputs));
				return false;
			}
			if (Pass.Writes.Num() > ::UE::DreamPass::MaxSlotOutputs)
			{
				OutError = FText::Format(LOCTEXT("SlotTooManyWrites", "The pass writes {0} buffers; an HLSL slot has {1} outputs."), FText::AsNumber(Pass.Writes.Num()), FText::AsNumber(::UE::DreamPass::MaxSlotOutputs));
				return false;
			}

			for (int32 Index = 0; Index < Pass.Reads.Num(); ++Index)
			{
				const FString Name = GetBindingName(Pass.Reads[Index]);
				if (!Define(Name, FString::Printf(TEXT("DP_Input%d"), Index))
					|| !Define(Name + TEXT("Size"), FString::Printf(TEXT("DP_InputSize[%d]"), Index))
					|| !Define(Name + TEXT("UVRect"), FString::Printf(TEXT("DP_InputUVRect[%d]"), Index)))
				{
					return false;
				}
			}

			for (int32 Index = 0; Index < Pass.Writes.Num(); ++Index)
			{
				const FString Name = GetBindingName(Pass.Writes[Index]);
				// A pixel slot's outputs are SV_Target0..3 of the entry; only their sizes are parameters.
				if (bCompute && !Define(Name, FString::Printf(TEXT("DP_Output%d"), Index)))
				{
					return false;
				}
				if (!Define(Name + TEXT("Size"), FString::Printf(TEXT("DP_OutputSize[%d]"), Index)))
				{
					return false;
				}
			}

			if (Pass.Params.Num() > 0)
			{
				// The runtime packs the values with the same call and the same types (DreamPassSnapshot.cpp), so the two
				// cannot disagree about where a value is.
				TArray<FName> Names;
				TArray<EDreamPassParameterType> Types;
				for (const FDreamPassParamBinding& Param : Pass.Params)
				{
					Names.Add(Param.Target);
					Types.Add(::UE::DreamPass::GetParamBindingType(Staged, Param));
				}

				TArray<FDreamPassSlotParamLocation> Locations;
				if (!::UE::DreamPass::LayoutSlotParameters(Names, Types, Locations))
				{
					OutError = FText::Format(
						LOCTEXT("SlotParamLayout", "The pass's params do not fit an HLSL slot: {0} float4 vectors hold every param, a float4 takes one of its own, and a texture cannot be a param of a slot."),
						FText::AsNumber(::UE::DreamPass::MaxSlotParamVectors));
					return false;
				}
				for (const FDreamPassSlotParamLocation& Location : Locations)
				{
					if (!Define(Location.Name.ToString(), MakeParamExpression(Location)))
					{
						return false;
					}
				}
			}

			const FString MainEntry = GetMainEntryName(bCompute);
			if (!Candidate.Entry.Equals(MainEntry, ESearchCase::CaseSensitive) && !Define(Candidate.Entry, MainEntry))
			{
				return false;
			}

			// At BeginView the view uniform buffer does not exist yet; a use of `View` is an undeclared name, not a crash.
			if (Pass.Injection == EDreamPassInjection::BeginView && !Define(TEXT("View"), TEXT("DP_NoViewAtBeginView")))
			{
				return false;
			}

			OutHead = FString::Printf(TEXT("#if DP_SLOT == %d\n#define DP_SLOT_DEFINED 1\n"), Candidate.Slot); /* I18N-EXEMPT: HLSL */
			OutHead += FString::Printf(TEXT("// %s.%s -- %s\n"), *PipelineName, *Candidate.PassName, *Candidate.ShaderReference); /* I18N-EXEMPT: HLSL comment */
			for (const TPair<FString, FString>& Pair : Defines)
			{
				OutHead += FString::Printf(TEXT("#define %s %s\n"), *Pair.Key, *Pair.Value); /* I18N-EXEMPT: HLSL */
			}

			OutTail.Reset();
			for (const TPair<FString, FString>& Pair : Defines)
			{
				OutTail += FString::Printf(TEXT("#undef %s\n"), *Pair.Key); /* I18N-EXEMPT: HLSL */
			}
			// DP_SLOT_DEFINED stays defined: it is what keeps the stub entry after the registry out of this permutation.
			OutTail += TEXT("#endif\n");
			return true;
		}

		FString MakeIncludeLine(const FString& VirtualPath)
		{
			return FString::Printf(TEXT("#include \"%s\"\n"), *VirtualPath); /* I18N-EXEMPT: HLSL */
		}

		TSharedRef<FJsonObject> SlotToJson(const FDreamPassRegistrySlot& Slot)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetNumberField(TEXT("slot"), Slot.Slot);
			Object->SetStringField(TEXT("pipeline"), Slot.Pipeline);
			Object->SetStringField(TEXT("pass"), Slot.Pass);
			Object->SetStringField(TEXT("kind"), Slot.Kind);
			Object->SetStringField(TEXT("source"), Slot.Source);
			Object->SetStringField(TEXT("shader"), Slot.Shader);
			Object->SetStringField(TEXT("entry"), Slot.Entry);
			Object->SetStringField(TEXT("hash"), Slot.Hash);

			TArray<TSharedPtr<FJsonValue>> Formats;
			for (const FString& Format : Slot.Formats)
			{
				Formats.Add(MakeShared<FJsonValueString>(Format));
			}
			Object->SetArrayField(TEXT("formats"), Formats);

			TArray<TSharedPtr<FJsonValue>> Files;
			for (const FString& File : Slot.Files)
			{
				Files.Add(MakeShared<FJsonValueString>(File));
			}
			Object->SetArrayField(TEXT("files"), Files);

			Object->SetStringField(TEXT("section"), Slot.Section);
			return Object;
		}

		bool SlotFromJson(const FJsonObject& Object, FDreamPassRegistrySlot& OutSlot)
		{
			double SlotNumber = -1.0;
			if (!Object.TryGetNumberField(TEXT("slot"), SlotNumber) || SlotNumber < 0.0)
			{
				return false;
			}
			OutSlot.Slot = FMath::RoundToInt(SlotNumber);
			Object.TryGetStringField(TEXT("pipeline"), OutSlot.Pipeline);
			Object.TryGetStringField(TEXT("pass"), OutSlot.Pass);
			Object.TryGetStringField(TEXT("kind"), OutSlot.Kind);
			Object.TryGetStringField(TEXT("source"), OutSlot.Source);
			Object.TryGetStringField(TEXT("shader"), OutSlot.Shader);
			Object.TryGetStringField(TEXT("entry"), OutSlot.Entry);
			Object.TryGetStringField(TEXT("hash"), OutSlot.Hash);
			Object.TryGetStringField(TEXT("section"), OutSlot.Section);

			const TArray<TSharedPtr<FJsonValue>>* Formats = nullptr;
			if (Object.TryGetArrayField(TEXT("formats"), Formats))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Formats)
				{
					FString Text;
					if (Value.IsValid() && Value->TryGetString(Text))
					{
						OutSlot.Formats.Add(Text);
					}
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
			if (Object.TryGetArrayField(TEXT("files"), Files))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Files)
				{
					FString Text;
					if (Value.IsValid() && Value->TryGetString(Text))
					{
						OutSlot.Files.Add(Text);
					}
				}
			}
			return !OutSlot.Pipeline.IsEmpty() && !OutSlot.Pass.IsEmpty();
		}

		bool ReadTable(const FJsonObject& Root, const TCHAR* Field, TArray<FDreamPassRegistrySlot>& OutTable)
		{
			const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
			if (!Root.TryGetArrayField(Field, Entries))
			{
				return true;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Entries)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !Object->IsValid())
				{
					return false;
				}
				FDreamPassRegistrySlot Slot;
				if (!SlotFromJson(**Object, Slot))
				{
					return false;
				}
				OutTable.Add(MoveTemp(Slot));
			}
			return true;
		}

		/** Writes Text unless the file already holds exactly that; a registry file whose timestamp moves recompiles the slot shaders. */
		bool WriteIfChanged(const FString& FilePath, const FString& Text, FString& OutError)
		{
			FString Existing;
			if (FFileHelper::LoadFileToString(Existing, *FilePath) && Existing.Equals(Text, ESearchCase::CaseSensitive))
			{
				return true;
			}
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), /*Tree*/ true);
			if (!FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				OutError = FilePath;
				return false;
			}
			return true;
		}

		bool SnapshotFilesExist(const bool bCompute, const FDreamPassRegistrySlot& Slot)
		{
			if (Slot.Files.IsEmpty())
			{
				return false;
			}
			const FString Directory = ::UE::DreamPass::GetSlotDirectory(bCompute, Slot.Slot);
			for (const FString& File : Slot.Files)
			{
				if (!IFileManager::Get().FileExists(*FPaths::Combine(Directory, File)))
				{
					return false;
				}
			}
			return true;
		}

		/** The lowest slot of the table no entry holds, or INDEX_NONE. */
		int32 FindFreeSlot(const TArray<FDreamPassRegistrySlot>& Table, const TSet<int32>& Reserved, const int32 SlotCount)
		{
			for (int32 Slot = 0; Slot < SlotCount; ++Slot)
			{
				if (Reserved.Contains(Slot))
				{
					continue;
				}
				if (!Table.ContainsByPredicate([Slot](const FDreamPassRegistrySlot& Entry) { return Entry.Slot == Slot; }))
				{
					return Slot;
				}
			}
			return INDEX_NONE;
		}

#if DREAMSHADER_WITH_CUSTOM_PASS
		const FGlobalShaderType* FindSlotShaderType(const bool bCompute)
		{
			const FShaderType* Type = FindShaderTypeByName(FHashedName(bCompute ? ComputeShaderTypeName : PixelShaderTypeName));
			return Type ? Type->GetGlobalShaderType() : nullptr;
		}

		/** `12`, or `12,4`: the line, and the column when there is one. */
		void ParseErrorLine(const FString& LineString, int32& OutLine, int32& OutColumn)
		{
			FString Line = LineString;
			FString Column;
			if (!LineString.Split(TEXT(","), &Line, &Column))
			{
				Column.Reset();
			}
			OutLine = FMath::Max(1, FCString::Atoi(*Line.TrimStartAndEnd()));
			OutColumn = Column.IsEmpty() ? 1 : FMath::Max(1, FCString::Atoi(*Column.TrimStartAndEnd()));
		}

		/** The text of a slot shader with its registry include pointed at the pre-check's in-memory registry. */
		bool MakePrecheckRoot(const FGlobalShaderType& ShaderType, const bool bCompute, FString& OutText)
		{
			FString FilePath;
			if (!MapDreamPassShaderVirtualPathToFile(ShaderType.GetShaderFilename(), FilePath) || !FFileHelper::LoadFileToString(OutText, *FilePath))
			{
				return false;
			}

			const FString RealInclude = FString::Printf(TEXT("\"%s\""), *GetRegistryVirtualPath(bCompute));
			const FString PrecheckInclude = FString::Printf(TEXT("\"%s\""), *(GetPrecheckVirtualDirectory() / FPaths::GetCleanFilename(GetRegistryVirtualPath(bCompute))));
			if (!OutText.Contains(RealInclude, ESearchCase::IgnoreCase))
			{
				return false;
			}
			OutText.ReplaceInline(*RealInclude, *PrecheckInclude, ESearchCase::IgnoreCase);
			return true;
		}

		/** An in-memory include must not be empty: the preprocessor checks it (StbLoadFile). */
		FString NonEmpty(const FString& Text)
		{
			return Text.IsEmpty() ? FString(TEXT("\n")) : Text;
		}

		struct FPrecheckError
		{
			FString File;
			int32 Line = 1;
			int32 Column = 1;
			FString Message;
			/** The error is in the user's file (or a file of the snapshot), not in the slot's generated text. */
			bool bInUserFile = false;
		};

		/**
		 * Compiles one candidate for one shader platform. False with OutErrors when it did not compile; true, possibly with
		 * warnings in OutErrors, when it did. bOutSkipped: the slot is not compiled on this platform at all.
		 */
		bool RunPrecheckJob(
			const FGlobalShaderType& ShaderType,
			const FDreamPassSlotCandidate& Candidate,
			const FString& RootText,
			const EShaderPlatform Platform,
			TArray<FPrecheckError>& OutErrors,
			bool& bOutSkipped)
		{
			bOutSkipped = false;
			OutErrors.Reset();

			FPlatformTypeLayoutParameters LayoutParameters;
			LayoutParameters.InitializeForPlatform(nullptr);
			const EShaderPermutationFlags PermutationFlags = GetShaderPermutationFlags(LayoutParameters);

			// One dimension, SHADER_PERMUTATION_RANGE_INT("DP_SLOT", 0, N): the permutation id is the slot.
			const int32 PermutationId = Candidate.Slot;
			if (PermutationId < 0 || PermutationId >= ShaderType.GetPermutationCount()
				|| !ShaderType.ShouldCompilePermutation(Platform, PermutationId, PermutationFlags))
			{
				bOutSkipped = true;
				return true;
			}

			const bool bCompute = Candidate.bCompute;
			const FString PrecheckDirectory = GetPrecheckVirtualDirectory();
			const FString RootPath = PrecheckDirectory / FPaths::GetCleanFilename(ShaderType.GetShaderFilename());
			const FString RegistryPath = PrecheckDirectory / FPaths::GetCleanFilename(GetRegistryVirtualPath(bCompute));
			const FString SlotDirectory = PrecheckDirectory / TEXT("Slots") / MakeDreamPassSlotLeaf(bCompute, Candidate.Slot);

			TRefCountPtr<FShaderCompileJob> Job = new FShaderCompileJob(
				0u,
				0u,
				EShaderCompileJobPriority::High,
				FShaderCompileJobKey(&ShaderType, Platform, nullptr, PermutationId));

			// As PrepareGlobalShaderCompileJob fills a global shader's job, minus the manager.
			ShaderType.SetupCompileEnvironment(Platform, PermutationId, PermutationFlags, Job->Input.Environment);
			Job->bErrorsAreLikelyToBeCode = true;
			Job->bIsGlobalShader = true;
			GlobalBeginCompileShader(
				TEXT("Global"),
				nullptr,
				&ShaderType,
				nullptr,
				PermutationId,
				*RootPath,
				ShaderType.GetFunctionName(),
				FShaderTarget(ShaderType.GetFrequency(), Platform),
				Job->Input);

			// After GlobalBeginCompileShader on purpose: it validates the map's paths under DO_CHECK, and allows only the
			// root and `.ush` files there, while a snapshot holds `.usf` files. The preprocessor itself takes any.
			TMap<FString, FString>& InMemory = Job->Input.Environment.IncludeVirtualPathToContentsMap;
			InMemory.Add(RootPath, NonEmpty(RootText));
			InMemory.Add(RegistryPath, NonEmpty(Candidate.SectionHead + MakeIncludeLine(SlotDirectory / Candidate.IncludeRelativePath) + Candidate.SectionTail));
			for (const FDreamPassShaderClosureFile& File : Candidate.Closure.Files)
			{
				InMemory.Add(SlotDirectory / File.RelativePath, NonEmpty(File.Text));
			}
			Job->Input.Environment.CompilerFlags.Add(CFLAG_DisableSourceStripping);

			const bool bPreprocessed = PreprocessShader(Job.GetReference());
			if (bPreprocessed)
			{
				CompileShader(GetTargetPlatformManagerRef().GetShaderFormats(), *Job);
			}
			const bool bSucceeded = bPreprocessed && Job->Output.bSucceeded;

			for (FShaderCompilerError& Error : Job->Output.Errors)
			{
				if (Error.ErrorVirtualFilePath.IsEmpty())
				{
					Error.ExtractSourceLocation();
				}

				FPrecheckError& Out = OutErrors.AddDefaulted_GetRef();
				Out.Message = Error.StrippedErrorMessage.TrimStartAndEnd();
				ParseErrorLine(Error.ErrorLineString, Out.Line, Out.Column);

				FString ErrorPath = Error.ErrorVirtualFilePath;
				ErrorPath.ReplaceInline(TEXT("\\"), TEXT("/"));
				if (ErrorPath.StartsWith(SlotDirectory + TEXT("/"), ESearchCase::IgnoreCase))
				{
					const FString Relative = ErrorPath.RightChop(SlotDirectory.Len() + 1);
					if (const FDreamPassShaderClosureFile* File = Candidate.Closure.FindByRelativePath(Relative))
					{
						Out.File = File->FilePath;
						Out.bInUserFile = true;
						continue;
					}
				}
				// The section or the slot shader: the pass's own declaration answers for it. Anything else -- an engine
				// header -- keeps its path in the message.
				if (!ErrorPath.IsEmpty() && !ErrorPath.StartsWith(PrecheckDirectory + TEXT("/"), ESearchCase::IgnoreCase))
				{
					Out.Message = FString::Printf(TEXT("%s(%d): %s"), *ErrorPath, Out.Line, *Out.Message); /* I18N-EXEMPT: quotes a shader compiler message */
				}
			}

			return bSucceeded;
		}
#endif
	}

	// ------------------------------------------------------------------------------------------------ the registry

	const FDreamPassRegistrySlot* FDreamPassRegistry::Find(const bool bCompute, const FString& Pipeline, const FString& Pass) const
	{
		return Table(bCompute).FindByPredicate([&Pipeline, &Pass](const FDreamPassRegistrySlot& Slot)
		{
			return Slot.Pipeline.Equals(Pipeline, ESearchCase::IgnoreCase) && Slot.Pass.Equals(Pass, ESearchCase::CaseSensitive);
		});
	}

	const FDreamPassRegistrySlot* FDreamPassRegistry::FindSlot(const bool bCompute, const int32 Slot) const
	{
		return Table(bCompute).FindByPredicate([Slot](const FDreamPassRegistrySlot& Entry) { return Entry.Slot == Slot; });
	}

	void FDreamPassRegistry::FindPipeline(const FString& Pipeline, TArray<const FDreamPassRegistrySlot*>& OutSlots) const
	{
		OutSlots.Reset();
		for (const bool bCompute : { true, false })
		{
			for (const FDreamPassRegistrySlot& Slot : Table(bCompute))
			{
				if (Slot.Pipeline.Equals(Pipeline, ESearchCase::IgnoreCase))
				{
					OutSlots.Add(&Slot);
				}
			}
		}
	}

	void FDreamPassRegistry::Sort()
	{
		auto BySlot = [](const FDreamPassRegistrySlot& A, const FDreamPassRegistrySlot& B) { return A.Slot < B.Slot; };
		Compute.Sort(BySlot);
		Pixel.Sort(BySlot);
	}

	FString MakeDreamPassSlotLeaf(const bool bCompute, const int32 Slot)
	{
		return FPaths::GetCleanFilename(::UE::DreamPass::GetSlotDirectory(bCompute, Slot));
	}

	bool AreDreamPassSnapshotFilesPresent(const bool bCompute, const FDreamPassRegistrySlot& Slot)
	{
		return DreamPassSlotRegistryDetail::SnapshotFilesExist(bCompute, Slot);
	}

	int32 DeleteStrayDreamPassSlotDirectories(const FDreamPassRegistry& Registry)
	{
		IFileManager& FileManager = IFileManager::Get();
		const FString SlotsRoot = FPaths::GetPath(::UE::DreamPass::GetSlotDirectory(true, 0));
		if (!FileManager.DirectoryExists(*SlotsRoot))
		{
			return 0;
		}

		TArray<FString> Leaves;
		FileManager.FindFiles(Leaves, *(SlotsRoot / TEXT("*")), /*Files*/ false, /*Directories*/ true);

		int32 Deleted = 0;
		for (const FString& Leaf : Leaves)
		{
			// Recognised by its exact spelling, slot by slot: a directory that is not one this compiler writes is left alone.
			for (const bool bCompute : { true, false })
			{
				const int32 SlotCount = bCompute ? ::UE::DreamPass::GetComputeSlotCount() : ::UE::DreamPass::GetPixelSlotCount();
				for (int32 Slot = 0; Slot < SlotCount; ++Slot)
				{
					if (!Leaf.Equals(MakeDreamPassSlotLeaf(bCompute, Slot), ESearchCase::IgnoreCase))
					{
						continue;
					}
					if (!Registry.FindSlot(bCompute, Slot)
						&& FileManager.DeleteDirectory(*(SlotsRoot / Leaf), /*RequireExists*/ false, /*Tree*/ true))
					{
						++Deleted;
					}
				}
			}
		}
		return Deleted;
	}

	FString GetDreamPassRegistryJsonPath()
	{
		return FPaths::Combine(::UE::DreamPass::GetUserShaderDirectory(), TEXT("Registry.json"));
	}

	bool LoadDreamPassRegistry(FDreamPassRegistry& OutRegistry, FString& OutError)
	{
		using namespace DreamPassSlotRegistryDetail;

		OutRegistry = FDreamPassRegistry();
		OutError.Reset();

		const FString Path = GetDreamPassRegistryJsonPath();
		if (!IFileManager::Get().FileExists(*Path))
		{
			return true;
		}

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("'%s' could not be read"), *Path); /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = FString::Printf(TEXT("'%s' is not valid JSON -- a merge conflict left in it is the usual reason"), *Path); /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
			return false;
		}

		if (!ReadTable(*Root, TEXT("compute"), OutRegistry.Compute) || !ReadTable(*Root, TEXT("pixel"), OutRegistry.Pixel))
		{
			OutError = FString::Printf(TEXT("'%s' holds a slot entry without a slot number, a pipeline or a pass"), *Path); /* I18N-EXEMPT: wrapped by the caller's coded diagnostic */
			return false;
		}

		OutRegistry.Sort();
		return true;
	}

	FString SerializeDreamPassRegistry(const FDreamPassRegistry& Registry)
	{
		using namespace DreamPassSlotRegistryDetail;

		FDreamPassRegistry Sorted = Registry;
		Sorted.Sort();

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema"), RegistrySchemaName);
		Root->SetNumberField(TEXT("version"), RegistrySchemaVersion);
		for (const bool bCompute : { true, false })
		{
			TArray<TSharedPtr<FJsonValue>> Entries;
			for (const FDreamPassRegistrySlot& Slot : Sorted.Table(bCompute))
			{
				Entries.Add(MakeShared<FJsonValueObject>(SlotToJson(Slot)));
			}
			Root->SetArrayField(bCompute ? TEXT("compute") : TEXT("pixel"), Entries);
		}

		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		Json += TEXT("\n");
		return Json;
	}

	FString BuildDreamPassRegistryShaderText(const FDreamPassRegistry& Registry, const bool bCompute)
	{
		FDreamPassRegistry Sorted = Registry;
		Sorted.Sort();

		FString Text = FString::Printf(
			TEXT("// DreamShader Custom Pass -- %s slot registry.\n")
			TEXT("//\n")
			TEXT("// Generated by DreamShaderCompiler from Registry.json beside it, whenever a .dsp compile changes a slot.\n")
			TEXT("// Do not edit by hand: every section includes a snapshot that compiled in its pre-check, and an edit here\n")
			TEXT("// skips that check -- a global shader that fails to compile is fatal. `dsc pass-registry --rebuild` rewrites it.\n")
			TEXT("// Commit this file and the Slots folder next to it: the global shaders are built from them.\n"),
			bCompute ? TEXT("compute") : TEXT("pixel")); /* I18N-EXEMPT: generated HLSL comment */

		for (const FDreamPassRegistrySlot& Slot : Sorted.Table(bCompute))
		{
			Text += TEXT("\n");
			if (Slot.HasSnapshot() && DreamPassSlotRegistryDetail::SnapshotFilesExist(bCompute, Slot))
			{
				Text += Slot.Section;
			}
			else if (Slot.HasSnapshot())
			{
				// A section whose snapshot is not on disk (a Slots folder that was not committed) would fail the global shader
				// compile, which is fatal. The runtime takes such sections out at startup (DreamShaderPass,
				// DreamShaderPassModule.cpp, DropSectionsWithMissingSnapshots); a registry written here must not put them back
				// when another pipeline's compile rewrites the file. Registry.json keeps the record: the slot's own `.dsp`
				// writes the snapshot, and with it the section, the next time it compiles.
				Text += FString::Printf(
					TEXT("// Slot %d: %s.%s -- its snapshot is not on disk, so it compiles to the empty stub until its .dsp is compiled again.\n"), /* I18N-EXEMPT: generated HLSL comment */
					Slot.Slot,
					*FPaths::GetBaseFilename(Slot.Pipeline),
					*Slot.Pass);
			}
			else
			{
				Text += FString::Printf(
					TEXT("// Slot %d: %s.%s -- reserved; no snapshot of it has passed a pre-check yet, so it compiles to the empty stub.\n"), /* I18N-EXEMPT: generated HLSL comment */
					Slot.Slot,
					*FPaths::GetBaseFilename(Slot.Pipeline),
					*Slot.Pass);
			}
		}
		return Text;
	}

	// --------------------------------------------------------------------------------------------------- planning

	bool PlanDreamPassSlots(
		const UDreamPassPipeline& Staged,
		const FString& PipelineObjectPath,
		const FString& PipelineSourceFile,
		TArray<FDreamPassSlotCandidate>&& Candidates,
		FDreamPassSlotPlan& OutPlan,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamPassSlotRegistryDetail;

		OutPlan = FDreamPassSlotPlan();
		OutPlan.PipelineObjectPath = PipelineObjectPath;
		OutPlan.PipelineName = FPackageName::ObjectPathToObjectName(PipelineObjectPath);
		OutPlan.PipelineSourceFile = PipelineSourceFile;
		OutPlan.Candidates = MoveTemp(Candidates);

		const Lang::FLangSpan NoSpan;
		FString LoadError;
		if (!LoadDreamPassRegistry(OutPlan.Registry, LoadError))
		{
			return Diagnostics.Error(TEXT("DSH8315"), NoSpan, FText::Format(
				LOCTEXT("RegistryUnreadable", "The Custom Pass slot registry cannot be read: {0}. Nothing was written, because writing over it would lose every slot it records; fix the file, or run 'dsc pass-registry --rebuild'."),
				FText::FromString(LoadError)));
		}
		const FDreamPassRegistry Previous = OutPlan.Registry;

		// The slots this pipeline keeps: the same pass, in the same table.
		for (FDreamPassSlotCandidate& Candidate : OutPlan.Candidates)
		{
			if (const FDreamPassRegistrySlot* Existing = Previous.Find(Candidate.bCompute, PipelineObjectPath, Candidate.PassName))
			{
				Candidate.Slot = Existing->Slot;
			}
		}

		// The slots it frees: a pass it no longer has, or one that moved between the compute and the pixel table.
		for (const bool bCompute : { true, false })
		{
			TArray<FDreamPassRegistrySlot>& Table = OutPlan.Registry.Table(bCompute);
			for (int32 Index = Table.Num() - 1; Index >= 0; --Index)
			{
				const FDreamPassRegistrySlot& Entry = Table[Index];
				if (!Entry.Pipeline.Equals(PipelineObjectPath, ESearchCase::IgnoreCase))
				{
					continue;
				}
				const bool bStillUsed = OutPlan.Candidates.ContainsByPredicate([&Entry, bCompute](const FDreamPassSlotCandidate& Candidate)
				{
					return Candidate.bCompute == bCompute && Candidate.Slot == Entry.Slot && Candidate.PassName.Equals(Entry.Pass, ESearchCase::CaseSensitive);
				});
				if (!bStillUsed)
				{
					FDreamPassFreedSlot& Freed = OutPlan.FreedSlots.AddDefaulted_GetRef();
					Freed.bCompute = bCompute;
					Freed.Slot = Entry.Slot;
					Freed.Pass = Entry.Pass;
					(bCompute ? OutPlan.bComputeChanged : OutPlan.bPixelChanged) = true;
					Table.RemoveAt(Index);
				}
			}
		}

		// The new passes take the lowest free slots, in declaration order, so two runs of one source agree.
		bool bOk = true;
		for (FDreamPassSlotCandidate& Candidate : OutPlan.Candidates)
		{
			if (Candidate.Slot != INDEX_NONE)
			{
				continue;
			}
			TSet<int32> Planned;
			for (const FDreamPassSlotCandidate& Other : OutPlan.Candidates)
			{
				if (Other.bCompute == Candidate.bCompute && Other.Slot != INDEX_NONE)
				{
					Planned.Add(Other.Slot);
				}
			}
			const int32 SlotCount = Candidate.bCompute ? ::UE::DreamPass::GetComputeSlotCount() : ::UE::DreamPass::GetPixelSlotCount();
			Candidate.Slot = FindFreeSlot(OutPlan.Registry.Table(Candidate.bCompute), Planned, SlotCount);
			if (Candidate.Slot == INDEX_NONE)
			{
				bOk = Diagnostics.Error(TEXT("DSH8316"), Candidate.Span, FText::Format(
					LOCTEXT("SlotsUsedUp", "Pass '{0}' needs a {1} slot and all {2} are taken. Merge passes, run 'dsc pass-registry --gc' to free the slots of pipelines whose source is gone, or raise {3} in the project's Target.cs."),
					FText::FromString(Candidate.PassName),
					Candidate.bCompute ? LOCTEXT("ComputeSlotWord", "compute") : LOCTEXT("PixelSlotWord", "pixel"),
					FText::AsNumber(SlotCount),
					FText::FromString(Candidate.bCompute ? TEXT("DREAMSHADER_PASS_COMPUTE_SLOTS") : TEXT("DREAMSHADER_PASS_PIXEL_SLOTS"))));
			}
		}
		if (!bOk)
		{
			return false;
		}

		const FString ProjectRelativeSource = Private::MakeProjectRelativeSourcePath(PipelineSourceFile);
		for (FDreamPassSlotCandidate& Candidate : OutPlan.Candidates)
		{
			// ---- the snapshot, in memory
			if (Candidate.ShaderFilePath.IsEmpty() || !CollectDreamPassShaderClosure(Candidate.ShaderFilePath, Candidate.Closure))
			{
				bOk = Diagnostics.Error(TEXT("DSH8319"), Candidate.Span, FText::Format(
					LOCTEXT("SnapshotRootUnreadable", "The shader '{0}' of pass '{1}' could not be read, so there is nothing to snapshot into its slot."),
					FText::FromString(Candidate.ShaderReference),
					FText::FromString(Candidate.PassName)));
				continue;
			}
			for (const FDreamPassMissingInclude& Missing : Candidate.Closure.MissingIncludes)
			{
				Lang::FLangSpan Span;
				Span.Line = Missing.Line;
				bOk = Diagnostics.Error(TEXT("DSH8320"), Missing.IncludingFile, Span, FText::Format(
					LOCTEXT("SnapshotIncludeMissing", "'{0}' is included by a relative path and names no file, so the snapshot of pass '{1}' cannot be built."),
					FText::FromString(Missing.Path),
					FText::FromString(Candidate.PassName)));
			}
			for (const FDreamPassLiveInclude& Live : Candidate.Closure.LiveIncludes)
			{
				Lang::FLangSpan Span;
				Span.Line = Live.Line;
				Diagnostics.Warning(TEXT("DSH8321"), Live.IncludingFile, Span, FText::Format(
					LOCTEXT("SnapshotLiveInclude", "'{0}' is included by a virtual path that is neither /Engine/ nor /Plugin/, so the snapshot of pass '{1}' keeps including the live file: an edit of it later reaches the global shaders without a pre-check. Include it by a relative path to have it copied into the snapshot."),
					FText::FromString(Live.VirtualPath),
					FText::FromString(Candidate.PassName)));
			}

			// ---- the section
			FText SectionError;
			if (!BuildSlotSection(Candidate, Staged, OutPlan.PipelineName, Candidate.SectionHead, Candidate.SectionTail, SectionError))
			{
				bOk = Diagnostics.Error(TEXT("DSH8317"), Candidate.Span, FText::Format(
					LOCTEXT("SlotSectionFailed", "Pass '{0}' cannot be mapped onto its HLSL slot: {1}"),
					FText::FromString(Candidate.PassName),
					SectionError));
				continue;
			}

			const FDreamPassShaderClosureFile* Root = Candidate.Closure.GetRoot();
			Candidate.IncludeRelativePath = Root ? Root->RelativePath : FString();
			Candidate.Section = Candidate.SectionHead
				+ MakeIncludeLine(::UE::DreamPass::GetSlotVirtualDirectory(Candidate.bCompute, Candidate.Slot) / Candidate.IncludeRelativePath)
				+ Candidate.SectionTail;
			Candidate.Hash = HashDreamPassText(Candidate.Section + TEXT("\n") + Candidate.Closure.ComputeContentHash());

			// ---- the registry entry
			TArray<FDreamPassRegistrySlot>& Table = OutPlan.Registry.Table(Candidate.bCompute);
			FDreamPassRegistrySlot* Entry = Table.FindByPredicate([&Candidate](const FDreamPassRegistrySlot& Slot) { return Slot.Slot == Candidate.Slot; });
			Candidate.bUnchanged = Entry
				&& Entry->Pipeline.Equals(PipelineObjectPath, ESearchCase::IgnoreCase)
				&& Entry->Pass.Equals(Candidate.PassName, ESearchCase::CaseSensitive)
				&& Entry->Hash.Equals(Candidate.Hash, ESearchCase::CaseSensitive)
				&& Entry->Section.Equals(Candidate.Section, ESearchCase::CaseSensitive)
				&& SnapshotFilesExist(Candidate.bCompute, *Entry);
			if (Candidate.bUnchanged)
			{
				Candidate.CheckedFormats = Entry->Formats;
				continue;
			}

			if (!Entry)
			{
				Entry = &Table.AddDefaulted_GetRef();
				Entry->Slot = Candidate.Slot;
			}
			Entry->Pipeline = PipelineObjectPath;
			Entry->Pass = Candidate.PassName;
			Entry->Kind = Candidate.bCompute ? TEXT("compute") : TEXT("fullscreen");
			Entry->Source = ProjectRelativeSource;
			// The file, project-relative: the reference as written is relative to its `.dsp`, and a file outside every mapped
			// directory -- which a pass may name, since only the snapshot is ever compiled -- has no virtual path.
			Entry->Shader = Candidate.ShaderFilePath.IsEmpty() ? Candidate.ShaderReference : Private::MakeProjectRelativeSourcePath(Candidate.ShaderFilePath);
			Entry->Entry = Candidate.Entry;
			Entry->Hash = Candidate.Hash;
			Entry->Section = Candidate.Section;
			Entry->Formats.Reset();
			Entry->Files.Reset();
			for (const FDreamPassShaderClosureFile& File : Candidate.Closure.Files)
			{
				Entry->Files.Add(File.RelativePath);
			}
			(Candidate.bCompute ? OutPlan.bComputeChanged : OutPlan.bPixelChanged) = true;
		}

		OutPlan.Registry.Sort();
		return bOk;
	}

	// -------------------------------------------------------------------------------------------------- pre-check

	void ResolveDreamPassPrecheckFormats(TArray<FName>& OutFormats, TArray<FName>& OutUnavailable)
	{
		OutFormats.Reset();
		OutUnavailable.Reset();

#if DREAMSHADER_WITH_CUSTOM_PASS
		TArray<FName> Wanted;
		if (FApp::CanEverRender())
		{
			// What the hot reload compiles, and what this editor renders with.
			UMaterialInterface::IterateOverActiveFeatureLevels([&Wanted](const ERHIFeatureLevel::Type FeatureLevel)
			{
				const EShaderPlatform Platform = GShaderPlatformForFeatureLevel[FeatureLevel];
				if (Platform < SP_NumPlatforms)
				{
					Wanted.AddUnique(LegacyShaderPlatformToShaderFormat(Platform));
				}
			});
		}

		ITargetPlatformManagerModule* Manager = GetTargetPlatformManager(/*bFailOnInitErrors*/ false);
		if (Manager)
		{
			// What a cook compiles: every format the project's target platforms target.
			for (const ITargetPlatform* Platform : Manager->GetActiveTargetPlatforms())
			{
				if (!Platform)
				{
					continue;
				}
				TArray<FName> Formats;
				Platform->GetAllTargetedShaderFormats(Formats);
				for (const FName Format : Formats)
				{
					Wanted.AddUnique(Format);
				}
			}
		}

		Wanted.Sort([](const FName A, const FName B) { return A.LexicalLess(B); });
		for (const FName Format : Wanted)
		{
			if (Manager && Manager->FindShaderFormat(Format) && ShaderFormatToLegacyShaderPlatform(Format) != SP_NumPlatforms)
			{
				OutFormats.Add(Format);
			}
			else
			{
				OutUnavailable.Add(Format);
			}
		}
#endif
	}

	bool PrecheckDreamPassSlots(
		FDreamPassSlotPlan& Plan,
		const TArray<FName>& InFormats,
		const bool bRecheckUnchanged,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamPassSlotRegistryDetail;

		const Lang::FLangSpan NoSpan;

		const bool bAnyToCheck = Plan.Candidates.ContainsByPredicate([bRecheckUnchanged](const FDreamPassSlotCandidate& Candidate)
		{
			return bRecheckUnchanged || !Candidate.bUnchanged;
		});
		if (!bAnyToCheck)
		{
			return true;
		}

#if DREAMSHADER_WITH_CUSTOM_PASS
		TArray<FName> Formats = InFormats;
		if (Formats.IsEmpty())
		{
			TArray<FName> Unavailable;
			ResolveDreamPassPrecheckFormats(Formats, Unavailable);
			for (const FName Format : Unavailable)
			{
				Diagnostics.Warning(TEXT("DSH8324"), NoSpan, FText::Format(
					LOCTEXT("PrecheckFormatUnavailable", "The project targets the shader format {0}, which this machine has no shader compiler for, so the HLSL slots were not pre-checked for it; a cook for that platform compiles them unchecked."),
					FText::FromName(Format)));
			}
		}
		if (Formats.IsEmpty())
		{
			return Diagnostics.Error(TEXT("DSH8325"), NoSpan, LOCTEXT("PrecheckNoFormat",
				"There is no shader format to pre-check the HLSL slots with: no active feature level and no target platform has a shader compiler on this machine. Nothing was written, because a slot that never compiled must not reach the global shaders."));
		}

		ITargetPlatformManagerModule& Manager = GetTargetPlatformManagerRef();

		bool bOk = true;
		for (const bool bCompute : { true, false })
		{
			const bool bTableHasWork = Plan.Candidates.ContainsByPredicate([bCompute, bRecheckUnchanged](const FDreamPassSlotCandidate& Candidate)
			{
				return Candidate.bCompute == bCompute && (bRecheckUnchanged || !Candidate.bUnchanged);
			});
			if (!bTableHasWork)
			{
				continue;
			}

			const FGlobalShaderType* ShaderType = FindSlotShaderType(bCompute);
			FString RootText;
			if (!ShaderType || !MakePrecheckRoot(*ShaderType, bCompute, RootText))
			{
				bOk = Diagnostics.Error(TEXT("DSH8323"), NoSpan, FText::Format(
					LOCTEXT("PrecheckNoShaderType", "The {0} slot shader cannot be pre-checked: the global shader type {1} is not registered, or its source no longer includes '{2}'. The DreamShaderPass module is out of step with this compiler; nothing was written."),
					bCompute ? LOCTEXT("ComputeTypeWord", "compute") : LOCTEXT("PixelTypeWord", "pixel"),
					FText::FromString(bCompute ? ComputeShaderTypeName : PixelShaderTypeName),
					FText::FromString(GetRegistryVirtualPath(bCompute))));
				continue;
			}

			for (FDreamPassSlotCandidate& Candidate : Plan.Candidates)
			{
				if (Candidate.bCompute != bCompute || (Candidate.bUnchanged && !bRecheckUnchanged) || Candidate.Section.IsEmpty())
				{
					continue;
				}

				Candidate.CheckedFormats.Reset();
				for (const FName Format : Formats)
				{
					const EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(Format);
					if (Platform == SP_NumPlatforms || !Manager.FindShaderFormat(Format))
					{
						continue;
					}

					TArray<FPrecheckError> Errors;
					bool bSkipped = false;
					const bool bCompiled = RunPrecheckJob(*ShaderType, Candidate, RootText, Platform, Errors, bSkipped);
					if (bSkipped)
					{
						continue;
					}
					if (bCompiled)
					{
						Candidate.CheckedFormats.Add(Format.ToString());
						continue;
					}

					bOk = false;
					if (Errors.IsEmpty())
					{
						FPrecheckError& Unexplained = Errors.AddDefaulted_GetRef();
						Unexplained.Message = TEXT("the shader compiler failed without an error message"); /* I18N-EXEMPT: wrapped by DSH8322 */
					}
					for (const FPrecheckError& Error : Errors)
					{
						if (Error.bInUserFile)
						{
							Lang::FLangSpan Span;
							Span.Line = Error.Line;
							Span.Column = Error.Column;
							Diagnostics.Error(TEXT("DSH8322"), Error.File, Span, FText::Format(
								LOCTEXT("PrecheckErrorInFile", "[{0}] pass '{1}' does not compile in its HLSL slot: {2}"),
								FText::FromName(Format),
								FText::FromString(Candidate.PassName),
								FText::FromString(Error.Message)));
						}
						else
						{
							Diagnostics.Error(TEXT("DSH8322"), Candidate.Span, FText::Format(
								LOCTEXT("PrecheckErrorInSlot", "[{0}] pass '{1}' does not compile in its HLSL slot: {2} (a name this pass binds may clash with one the slot shader or the snapshot uses)"),
								FText::FromName(Format),
								FText::FromString(Candidate.PassName),
								FText::FromString(Error.Message)));
						}
					}
				}

				// The formats are part of the record: `dsc pass-registry` lists them, and a format added to the project later
				// shows as one this snapshot was never checked for.
				if (FDreamPassRegistrySlot* Entry = Plan.Registry.Table(bCompute).FindByPredicate([&Candidate](const FDreamPassRegistrySlot& Slot) { return Slot.Slot == Candidate.Slot; }))
				{
					if (!Candidate.bUnchanged)
					{
						Entry->Formats = Candidate.CheckedFormats;
					}
				}
			}
		}
		return bOk;
#else
		(void)InFormats;
		(void)bRecheckUnchanged;
		return Diagnostics.Error(TEXT("DSH8323"), NoSpan, LOCTEXT("PrecheckNoCustomPass",
			"HLSL slots need Unreal Engine 5.8 or later; this engine has no Custom Pass runtime to compile them for."));
#endif
	}

	// --------------------------------------------------------------------------------------------------- writing

	bool CommitDreamPassSlots(FDreamPassSlotPlan& Plan, Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamPassSlotRegistryDetail;

		if (!Plan.HasChanges())
		{
			return true;
		}

		const Lang::FLangSpan NoSpan;
		IFileManager& FileManager = IFileManager::Get();
		bool bOk = true;
		auto ReportWriteFailure = [&Diagnostics, &NoSpan, &bOk](const FString& What)
		{
			bOk = Diagnostics.Error(TEXT("DSH8326"), NoSpan, FText::Format(
				LOCTEXT("SlotWriteFailed", "'{0}' could not be written. The slot registry and its snapshots are committed files: a file that is read-only because it is not checked out is the usual reason."),
				FText::FromString(What)));
		};

		// Freed slots first: a slot a pass moved into this compile may be one another pass of it just left.
		for (const FDreamPassFreedSlot& Freed : Plan.FreedSlots)
		{
			Diagnostics.Info(TEXT("DSH8327"), NoSpan, FText::Format(
				LOCTEXT("SlotFreed", "{0} slot {1} of pass '{2}' was freed: the pipeline no longer runs that pass in HLSL there."),
				Freed.bCompute ? LOCTEXT("ComputeWord", "Compute") : LOCTEXT("PixelWord", "Pixel"),
				FText::AsNumber(Freed.Slot),
				FText::FromString(Freed.Pass)));

			const bool bTakenAgain = Plan.Candidates.ContainsByPredicate([&Freed](const FDreamPassSlotCandidate& Candidate)
			{
				return Candidate.bCompute == Freed.bCompute && Candidate.Slot == Freed.Slot;
			});
			if (!bTakenAgain)
			{
				FileManager.DeleteDirectory(*::UE::DreamPass::GetSlotDirectory(Freed.bCompute, Freed.Slot), /*RequireExists*/ false, /*Tree*/ true);
			}
		}

		// Then the snapshots, before the registry that includes them: a registry never names a file that is not there.
		for (const FDreamPassSlotCandidate& Candidate : Plan.Candidates)
		{
			if (Candidate.bUnchanged)
			{
				continue;
			}
			const FString Directory = ::UE::DreamPass::GetSlotDirectory(Candidate.bCompute, Candidate.Slot);
			FileManager.DeleteDirectory(*Directory, /*RequireExists*/ false, /*Tree*/ true);
			for (const FDreamPassShaderClosureFile& File : Candidate.Closure.Files)
			{
				const FString Target = FPaths::Combine(Directory, File.RelativePath);
				FileManager.MakeDirectory(*FPaths::GetPath(Target), /*Tree*/ true);
				if (!FFileHelper::SaveStringToFile(File.Text, *Target, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					ReportWriteFailure(Target);
				}
			}
		}
		if (!bOk)
		{
			return false;
		}

		FString FailedPath;
		if (!WriteIfChanged(GetDreamPassRegistryJsonPath(), SerializeDreamPassRegistry(Plan.Registry), FailedPath))
		{
			ReportWriteFailure(FailedPath);
			return false;
		}
		for (const bool bCompute : { true, false })
		{
			if (!(bCompute ? Plan.bComputeChanged : Plan.bPixelChanged))
			{
				continue;
			}
			if (!WriteIfChanged(::UE::DreamPass::GetRegistryFilePath(bCompute), BuildDreamPassRegistryShaderText(Plan.Registry, bCompute), FailedPath))
			{
				ReportWriteFailure(FailedPath);
			}
		}
		return bOk;
	}

	bool WriteDreamPassRegistry(const FDreamPassRegistry& Registry, const FDreamPassRegistry* Previous, Lang::FLangDiagnosticSink& Diagnostics)
	{
		using namespace DreamPassSlotRegistryDetail;

		const Lang::FLangSpan NoSpan;
		if (Previous)
		{
			for (const bool bCompute : { true, false })
			{
				for (const FDreamPassRegistrySlot& Old : Previous->Table(bCompute))
				{
					if (!Registry.FindSlot(bCompute, Old.Slot))
					{
						IFileManager::Get().DeleteDirectory(*::UE::DreamPass::GetSlotDirectory(bCompute, Old.Slot), /*RequireExists*/ false, /*Tree*/ true);
					}
				}
			}
		}

		bool bOk = true;
		FString FailedPath;
		if (!WriteIfChanged(GetDreamPassRegistryJsonPath(), SerializeDreamPassRegistry(Registry), FailedPath))
		{
			bOk = false;
		}
		for (const bool bCompute : { true, false })
		{
			if (bOk && !WriteIfChanged(::UE::DreamPass::GetRegistryFilePath(bCompute), BuildDreamPassRegistryShaderText(Registry, bCompute), FailedPath))
			{
				bOk = false;
			}
		}
		if (!bOk)
		{
			return Diagnostics.Error(TEXT("DSH8326"), NoSpan, FText::Format(
				LOCTEXT("RegistryWriteFailed", "'{0}' could not be written. The slot registry is a committed file: a file that is read-only because it is not checked out is the usual reason."),
				FText::FromString(FailedPath)));
		}
		return true;
	}

	void HotReloadDreamPassShaders(const bool bCompute, const bool bPixel)
	{
#if DREAMSHADER_WITH_CUSTOM_PASS
		using namespace DreamPassSlotRegistryDetail;

		// The editor only. A commandlet has no frame to show the change in, and the next editor start compiles the changed
		// registry anyway: the slot shaders' source hash moved, so their global shader map entry is out of date.
		if (!GIsEditor || IsRunningCommandlet() || !FApp::CanEverRender() || (!bCompute && !bPixel))
		{
			return;
		}

		TArray<const FShaderType*> Types;
		if (bCompute)
		{
			if (const FGlobalShaderType* Type = FindSlotShaderType(true))
			{
				Types.Add(Type);
			}
		}
		if (bPixel)
		{
			if (const FGlobalShaderType* Type = FindSlotShaderType(false))
			{
				Types.Add(Type);
			}
		}
		if (Types.IsEmpty())
		{
			return;
		}

		// The engine's own `recompileshaders changed`, narrowed to these two types (E/Private/ShaderCompiler/ShaderCompiler.cpp,
		// RecompileShaders): forget the cached sources, let the types see the uniform buffers the new snapshots reference,
		// then recompile per active feature level. Unchanged slots come back from the job cache.
		FlushShaderFileCache();
		FlushRenderingCommands();
		UpdateReferencedUniformBufferNames(Types, TArrayView<const FVertexFactoryType*>(), TArrayView<const FShaderPipelineType*>());

		const TArray<const FShaderPipelineType*> NoPipelines;
		UMaterialInterface::IterateOverActiveFeatureLevels([&Types, &NoPipelines](const ERHIFeatureLevel::Type FeatureLevel)
		{
			const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
			BeginRecompileGlobalShaders(Types, NoPipelines, ShaderPlatform);
			// Per platform, as the engine does it: the global shader compile job ids collide otherwise.
			FinishRecompileGlobalShaders();
		});

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader Custom Pass: recompiled the HLSL slot shaders (%s%s%s)."),
			bCompute ? TEXT("compute") : TEXT(""),
			bCompute && bPixel ? TEXT(", ") : TEXT(""),
			bPixel ? TEXT("pixel") : TEXT(""));
#else
		(void)bCompute;
		(void)bPixel;
#endif
	}

	bool IsDreamPassRegistryCurrentFor(const UDreamPassPipeline& Pipeline, const FString& PipelineObjectPath)
	{
		using namespace DreamPassSlotRegistryDetail;

		FDreamPassRegistry Registry;
		FString Error;
		if (!LoadDreamPassRegistry(Registry, Error))
		{
			return false;
		}

		int32 Expected = 0;
		for (const FDreamPassDesc& Pass : Pipeline.Passes)
		{
			bool bCompute = false;
			int32 Slot = INDEX_NONE;
			if (Pass.Kind == EDreamPassKind::Compute)
			{
				bCompute = true;
				Slot = Pass.Compute.Slot;
			}
			else if (Pass.Kind == EDreamPassKind::Fullscreen && !Pass.Fullscreen.Material && !Pass.Fullscreen.ShaderPath.IsEmpty())
			{
				Slot = Pass.Fullscreen.PixelSlot;
			}
			else
			{
				continue;
			}

			++Expected;
			const FDreamPassRegistrySlot* Entry = Registry.Find(bCompute, PipelineObjectPath, Pass.Name.ToString());
			if (!Entry || Entry->Slot != Slot || !Entry->HasSnapshot() || !SnapshotFilesExist(bCompute, *Entry))
			{
				return false;
			}
		}

		TArray<const FDreamPassRegistrySlot*> Owned;
		Registry.FindPipeline(PipelineObjectPath, Owned);
		return Owned.Num() == Expected;
	}
}

#undef LOCTEXT_NAMESPACE
