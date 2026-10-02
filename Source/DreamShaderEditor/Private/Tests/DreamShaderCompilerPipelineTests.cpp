// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Compiler2.Pipeline.* -- a `.dsp` through the compiler and the editor:
//
//   Asset            the UDreamPassPipeline and the render targets of its exports: every field, the render target
//                    settings, the stamps, the build-key skip, reuse in place, a stale export target
//   OwnershipGuard   an asset of another class, or one DreamShader did not make, at the pipeline's or a target's path
//   Dependencies     `.dsp` -> `.dss` (its pass materials, compiled first) and `.dss` -> `.dsp` (UE.DreamPassBuffer): the
//                    product index, the dependency sort, the build keys, and the `.dss` checks DSH5315-DSH5326
//   SelfRead         a pass material reading its own pipeline's export, in either compile order (DSH8333 / DSH5325)
//   Layers           only the first UDreamPassSettings::MaxLayers names of the layer table are layers (DSH4412)
//   Roundtrip        `.dsp` -> asset -> DecompileDreamPassPipelineToText -> bind: the same payload, no DSH9221-DSH9223
//   RegistryPrecheck CheckDreamShaderPipelineSlots: a pass that compiles, one that does not (DSH8322 at its file and
//                    line), a pipeline without HLSL passes (DSH8339); nothing written         [NonNullRHI]
//   Registry         a compile that commits slots: the section text, slot stability across an edit, the slot freed,
//                    the garbage of a deleted pipeline, an unreadable Registry.json           [NonNullRHI]
//
// Every fixture is a directory of its own under <DShader>/DreamShaderTests/Pipeline2 (FDreamShaderCompile2Fixture), and
// everything compiled into its package path is deleted on the way out. The Registry test commits to the project's
// slot registry (<DShader>/.dreampass): it copies that folder away first and puts it back in every case
// (FScopedDreamPassRegistryBackup). Every other test leaves Registry.json as it found it, and Asset and RegistryPrecheck
// say so.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Decompiler/DreamShaderPipelineDecompiler.h"
#include "DreamPassPipeline.h"
#include "DreamPassSettings.h"
#include "DreamPassTypes.h"
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderDependencyGraphService.h"
#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderPassModule.h"
#include "DreamShaderPassPipelines.h"
#include "DreamShaderProductIndex.h"
#include "DreamShaderVersionCompat.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IR/IR.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangPipelineSource.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::CompilerPipelineTests
{
	namespace Lang = ::UE::DreamShader::Lang;
	namespace IR = ::UE::DreamShader::IR;
	namespace Compiler = ::UE::DreamShader::Editor::Compiler;
	namespace Tests = ::UE::DreamShader::Editor::Private::Tests;

	/** The scratch area every fixture of this file lives in. */
	inline const TCHAR* ScratchArea() { return TEXT("Pipeline2"); }

	// =============================================================================================
	// Text and diagnostics
	// =============================================================================================

	/** A raw string as a source text: `\n` line terminators, and the line break right after `R"(` dropped. */
	inline FString Src(const TCHAR* Text)
	{
		FString Result = FString(Text).Replace(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
		if (Result.StartsWith(TEXT("\n"), ESearchCase::CaseSensitive))
		{
			Result.RightChopInline(1);
		}
		return Result;
	}

	/** The 1-based line of the first occurrence of Marker; INDEX_NONE when it is not there. */
	inline int32 LineOf(const FString& Text, const FString& Marker)
	{
		const int32 Offset = Text.Find(Marker, ESearchCase::CaseSensitive);
		if (Offset == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		int32 Line = 1;
		for (int32 Index = 0; Index < Offset; ++Index)
		{
			Line += Text[Index] == TEXT('\n') ? 1 : 0;
		}
		return Line;
	}

	inline const Lang::FLangDiagnostic* FindCode(const Lang::FLangDiagnosticSink& Sink, const TCHAR* Code)
	{
		return Sink.GetDiagnostics().FindByPredicate([Code](const Lang::FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive);
		});
	}

	inline bool HasCode(const Lang::FLangDiagnosticSink& Sink, const TCHAR* Code)
	{
		return FindCode(Sink, Code) != nullptr;
	}

	inline bool HasCode(const Lang::FLangDiagnosticSink& Sink, const TCHAR* Code, const Lang::ELangSeverity Severity)
	{
		return Sink.GetDiagnostics().ContainsByPredicate([Code, Severity](const Lang::FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == Severity && Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive);
		});
	}

	/** Whether Code is raised, or quoted inside another diagnostic's message (a compile-first failure nests the inner one). */
	inline bool HasCodeOrNested(const Lang::FLangDiagnosticSink& Sink, const TCHAR* Code)
	{
		return Sink.GetDiagnostics().ContainsByPredicate([Code](const Lang::FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive) || Diagnostic.Message.ToString().Contains(Code, ESearchCase::CaseSensitive);
		});
	}

	inline FString Describe(const Lang::FLangDiagnosticSink& Sink)
	{
		TArray<FString> Entries;
		for (const Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			Entries.Add(FString::Printf(TEXT("[%s] %s"), Lang::LexToString(Diagnostic.Severity), *Lang::FLangDiagnosticSink::ToWireString(Diagnostic)));
		}
		return Entries.Num() > 0 ? FString::Join(Entries, TEXT(" | ")) : FString(TEXT("<none>"));
	}

	inline FString ReadText(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		return Text;
	}

	/** A case-sensitive string compare that says both sides (FAutomationTestBase::TestEqual on FString ignores case). */
	inline bool ExpectString(FAutomationTestBase& Test, const FString& What, const FString& Actual, const FString& Expected)
	{
		const bool bEqual = Actual.Equals(Expected, ESearchCase::CaseSensitive);
		Test.TestTrue(FString::Printf(TEXT("%s: '%s' (expected '%s')"), *What, *Actual, *Expected), bEqual);
		return bEqual;
	}

	// =============================================================================================
	// Runs
	// =============================================================================================

	/** One run of the pipeline: a check (bEmitAssets false) or a build. */
	inline bool RunPipeline(const FString& SourceFilePath, Compiler::FDreamShaderLang2PipelineResult& OutResult, const bool bEmitAssets, const bool bForce = false)
	{
		Compiler::FDreamShaderLang2PipelineOptions Options;
		Options.bEmitAssets = bEmitAssets;
		Options.bForce = bForce;
		return Compiler::RunDreamShaderLang2Pipeline(SourceFilePath, Options, OutResult);
	}

	/** The PassPipeline payload of a run, as the IR carries it; null when the run did not get that far. */
	inline const IR::FIRPassPipeline* FindPassPipelinePayload(const Compiler::FDreamShaderLang2PipelineResult& Run)
	{
		if (!Run.IR.IsValid())
		{
			return nullptr;
		}
		for (const IR::FIRProduct& Product : Run.IR->Products)
		{
			if (Product.Kind == IR::EIRProductKind::PassPipeline)
			{
				return &Product.PassPipeline;
			}
		}
		return nullptr;
	}

	inline bool ExpectSamePipeline(FAutomationTestBase& Test, const FString& What, const IR::FIRPassPipeline& A, const IR::FIRPassPipeline& B)
	{
		TArray<FString> Differences;
		const bool bSame = Lang::CompareDreamShaderPipelines(A, B, &Differences);
		for (const FString& Difference : Differences)
		{
			Test.AddInfo(FString::Printf(TEXT("%s: %s"), *What, *Difference));
		}
		Test.TestTrue(What, bSame);
		return bSame;
	}

	/** The facade compile, every product; false with the wire message in OutMessage. */
	inline bool Compile(const FString& SourceFilePath, FString& OutMessage, const bool bForce = false)
	{
		::UE::DreamShader::FDreamShaderError Error;
		const bool bCompiled = Tests::CompileDreamShaderTestAssets(SourceFilePath, Error, bForce);
		OutMessage = FString::Printf(TEXT("%s: %s"), *Error.Code, *Error.Message);
		return bCompiled;
	}

	inline FString GetAssetMetadata(UObject* Asset, const TCHAR* Key)
	{
		UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
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

	/** Takes an object a test made in memory out of the way: no longer findable at its path, and collectable. */
	inline void DiscardInMemoryObject(UObject* Object)
	{
		if (!Object)
		{
			return;
		}
		Object->ClearFlags(RF_Public | RF_Standalone);
		Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
		Object->MarkAsGarbage();
	}

	/** The project's pass layers replaced for one scope (UDreamPassSettings::LayerNames is what the host hands the binder). */
	struct FScopedPassLayers
	{
		TArray<FName> Saved;

		explicit FScopedPassLayers(const TArray<FName>& Layers)
		{
			UDreamPassSettings* Settings = GetMutableDefault<UDreamPassSettings>();
			Saved = Settings->LayerNames;
			Settings->LayerNames = Layers;
		}

		~FScopedPassLayers()
		{
			GetMutableDefault<UDreamPassSettings>()->LayerNames = Saved;
		}

		FScopedPassLayers(const FScopedPassLayers&) = delete;
		FScopedPassLayers& operator=(const FScopedPassLayers&) = delete;
	};

	/**
	 * The project's slot registry folder (<DShader>/.dreampass) copied away, and put back however the scope is left. The
	 * slot shaders the editor has loaded follow the files: with a renderer, RewriteDreamPassRegistryFiles writes the
	 * registry files from the restored Registry.json and hot reloads them, and the copy is laid over them once more so that
	 * every file is byte for byte what it was.
	 */
	struct FScopedDreamPassRegistryBackup
	{
		FString Directory;
		FString Backup;
		bool bHadDirectory = false;
		bool bBackedUp = false;

		FScopedDreamPassRegistryBackup()
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			Directory = FPaths::ConvertRelativePathToFull(::UE::DreamPass::GetUserShaderDirectory());
			Backup = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("DreamShaderTests"),
				FString::Printf(TEXT("DreamPassBackup-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			bHadDirectory = PlatformFile.DirectoryExists(*Directory);
			if (bHadDirectory)
			{
				PlatformFile.CreateDirectoryTree(*Backup);
				bBackedUp = PlatformFile.CopyDirectoryTree(*Backup, *Directory, /*bOverwriteAllExisting*/ true);
			}
		}

		/** False when the folder exists and could not be copied: nothing may be committed then. */
		bool IsValid() const { return !bHadDirectory || bBackedUp; }

		~FScopedDreamPassRegistryBackup()
		{
			if (!IsValid())
			{
				return;
			}
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.DeleteDirectoryRecursively(*Directory);
			if (bHadDirectory)
			{
				PlatformFile.CreateDirectoryTree(*Directory);
				PlatformFile.CopyDirectoryTree(*Directory, *Backup, /*bOverwriteAllExisting*/ true);
				if (FApp::CanEverRender())
				{
					int32 Reserved = 0;
					Lang::FLangDiagnosticSink Ignored;
					Compiler::RewriteDreamPassRegistryFiles(Reserved, Ignored);
					PlatformFile.CopyDirectoryTree(*Directory, *Backup, /*bOverwriteAllExisting*/ true);
				}
				PlatformFile.DeleteDirectoryRecursively(*Backup);
			}
		}

		FScopedDreamPassRegistryBackup(const FScopedDreamPassRegistryBackup&) = delete;
		FScopedDreamPassRegistryBackup& operator=(const FScopedDreamPassRegistryBackup&) = delete;
	};

	/** Registry.json's text, or empty when there is none. */
	inline FString ReadRegistryJson()
	{
		return ReadText(FPaths::Combine(::UE::DreamPass::GetUserShaderDirectory(), TEXT("Registry.json")));
	}

	/** The slot a pass of a pipeline has in the registry, or INDEX_NONE. */
	inline int32 FindSlot(const Compiler::FDreamPassRegistryReport& Report, const FString& PipelineObjectPath, const TCHAR* Pass, const bool bCompute)
	{
		const Compiler::FDreamPassSlotReport* Slot = Report.Slots.FindByPredicate([&PipelineObjectPath, Pass, bCompute](const Compiler::FDreamPassSlotReport& Candidate)
		{
			return Candidate.bCompute == bCompute
				&& Candidate.Pass.Equals(Pass, ESearchCase::CaseSensitive)
				&& Candidate.Pipeline.Equals(PipelineObjectPath, ESearchCase::IgnoreCase);
		});
		return Slot ? Slot->Slot : INDEX_NONE;
	}

	inline int32 CountSlotsOf(const Compiler::FDreamPassRegistryReport& Report, const FString& PipelineObjectPath)
	{
		int32 Count = 0;
		for (const Compiler::FDreamPassSlotReport& Slot : Report.Slots)
		{
			Count += Slot.Pipeline.Equals(PipelineObjectPath, ESearchCase::IgnoreCase) ? 1 : 0;
		}
		return Count;
	}

	/** One slot's section of a registry file: from `#if DP_SLOT == N` to its `#endif`, both included; empty when absent. */
	inline FString FindRegistrySection(const bool bCompute, const int32 Slot)
	{
		const FString Text = ReadText(::UE::DreamPass::GetRegistryFilePath(bCompute)).Replace(TEXT("\r\n"), TEXT("\n"));
		const FString Head = FString::Printf(TEXT("#if DP_SLOT == %d\n"), Slot);
		const int32 Start = Text.Find(Head, ESearchCase::CaseSensitive);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		const int32 End = Text.Find(TEXT("#endif\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		return End == INDEX_NONE ? FString() : Text.Mid(Start, End + 7 - Start);
	}

	/** The expression a slot's registry gives one param (BuildSlotSection's spelling of a UE::DreamPass::LayoutSlotParameters location). */
	inline FString MakeSlotParamExpression(const FDreamPassSlotParamLocation& Location)
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
		const FString Vector = FString::Printf(TEXT("DP_Params[%d]%s"), Location.Vector, *Swizzle);
		switch (Location.Type)
		{
		case EDreamPassParameterType::Int:  return FString::Printf(TEXT("asint(%s)"), *Vector);
		case EDreamPassParameterType::Bool: return FString::Printf(TEXT("(asuint(%s) != 0)"), *Vector);
		default:                            return FString::Printf(TEXT("(%s)"), *Vector);
		}
	}

	// =============================================================================================
	// Sources
	// =============================================================================================

	/** A material a mesh pass draws: Unlit, the usages of the default `Usage` set, Output0 of UE.DreamPassOutput connected. */
	inline FString MakeMaskMaterial(const FString& Name, const TCHAR* Value = TEXT("1.0"))
	{
		return FString::Printf(TEXT(
			"#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)\n"
			"\n"
			"export void %s(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = float3(0.0, 0.0, 0.0);\n"
			"    UE.DreamPassOutput(Output0 = float4(%s, 0.0, 0.0, 0.0));\n"
			"}\n"), *Name, Value);
	}

	/** A Post Process material reading the UserSceneTexture "Mask", with the params a composite pass sets. */
	inline FString MakeCompositeMaterial(const FString& Name)
	{
		return FString::Printf(TEXT(
			"#pragma material(Domain = PostProcess, BlendableLocation = SceneColorAfterDOF)\n"
			"\n"
			"uniform float4 Color = float4(1.0, 0.6, 0.0, 1.0);\n"
			"uniform float Width = 2.0;\n"
			"uniform float DreamPassWeight = 1.0;\n"
			"\n"
			"export void %s(inout material m)\n"
			"{\n"
			"    float2 UV = UE.ScreenPosition().ViewportUV;\n"
			"    float3 Scene = UE.SceneTexture(SceneTextureId = PostProcessInput0, Coordinates = UV).Color.rgb;\n"
			"    float Inside = UE.UserSceneTexture(UserSceneTexture = \"Mask\", Coordinates = UV).Color.r;\n"
			"    m.EmissiveColor = lerp(Scene, Color.rgb, Inside * Color.a * DreamPassWeight * saturate(Width));\n"
			"}\n"), *Name);
	}

	/** A material that reads one buffer of a pipeline through UE.DreamPassBuffer. */
	inline FString MakeReaderMaterial(const FString& Name, const TCHAR* Pipeline, const TCHAR* Buffer)
	{
		return FString::Printf(TEXT(
			"export void %s(inout material m)\n"
			"{\n"
			"    float Glow = UE.DreamPassBuffer(Pipeline = \"%s\", Buffer = \"%s\").r;\n"
			"    m.EmissiveColor = float3(Glow, Glow, Glow);\n"
			"}\n"), *Name, Pipeline, Buffer);
	}

	/** CP_PL2Asset.dsp: a mesh pass, two clears, a composite; two exported buffers, one of a fixed size. */
	inline FString MakeAssetPipeline(const TCHAR* FieldSize, const TCHAR* BlurredExport)
	{
		return FString::Printf(TEXT(
			"#pragma pipeline(Order = 7, Views = Game | SceneCapture)\n"
			"\n"
			"/// @group Look\n"
			"uniform float4 Tint = float4(1.0, 0.5, 0.25, 1.0);\n"
			"uniform float Width = 2.0;\n"
			"\n"
			"buffer Mask : R8(Clear = 0);\n"
			"buffer Blurred : RG16F(Scale = 0.5, Export = %s);\n"
			"buffer Field : RGBA16F(Size = %s, Export = true);\n"
			"\n"
			"pass DrawMask : mesh\n"
			"{\n"
			"    Injection = AfterOpaque;\n"
			"    Filter = Stencil(1);\n"
			"    Material = \"M_PL2AssetMask\";\n"
			"    Depth = None;\n"
			"    write Output0 = Mask;\n"
			"}\n"
			"\n"
			"pass FillBlurred : clear\n"
			"{\n"
			"    Injection = AfterOpaque;\n"
			"    Value = 0.25;\n"
			"    write Blurred;\n"
			"}\n"
			"\n"
			"pass FillField : clear\n"
			"{\n"
			"    Injection = AfterOpaque;\n"
			"    Value = float4(0.5, 0.5, 0.5, 1.0);\n"
			"    write Field;\n"
			"}\n"
			"\n"
			"pass Composite : fullscreen\n"
			"{\n"
			"    Material = \"PP_PL2AssetComp\";\n"
			"    read Mask;\n"
			"    write SceneColor;\n"
			"    param Color = Tint;\n"
			"    param Width = Width;\n"
			"}\n"), BlurredExport, FieldSize);
	}

	/**
	 * A pipeline that draws a mask with Material and exports one buffer, Glow, which a clear fills (so that its format can
	 * change without touching another pass): the `.dsp` half of the dependency tests.
	 */
	inline FString MakeExportingPipeline(const TCHAR* Material, const TCHAR* GlowFormat)
	{
		return FString::Printf(TEXT(
			"buffer Mask : R8(Clear = 0, Export = true);\n"
			"buffer Glow : %s(Export = true);\n"
			"\n"
			"pass DrawMask : mesh\n"
			"{\n"
			"    Injection = AfterOpaque;\n"
			"    Filter = Stencil(1);\n"
			"    Material = \"%s\";\n"
			"    Depth = None;\n"
			"    write Output0 = Mask;\n"
			"}\n"
			"\n"
			"pass Keep : clear\n"
			"{\n"
			"    Injection = AfterOpaque;\n"
			"    Value = 1.0;\n"
			"    write Glow;\n"
			"}\n"), GlowFormat, Material);
	}

	/** A pipeline whose only pass is a clear of one exported buffer. */
	inline FString MakeClearPipeline(const TCHAR* Buffers, const TCHAR* Written)
	{
		return FString::Printf(TEXT("%s\npass Fill : clear\n{\n    Injection = AfterOpaque;\n    write %s;\n}\n"), Buffers, Written);
	}

	inline const TCHAR* GoodComputeShader()
	{
		return TEXT(
			"#include \"/Plugin/DreamShader/Pass/DreamPass.ush\"\n"
			"\n"
			"[numthreads(8, 8, 1)]\n"
			"void GoodCS(uint3 Id : SV_DispatchThreadID)\n"
			"{\n"
			"    if (any(Id.xy >= DP_DispatchSize.xy))\n"
			"    {\n"
			"        return;\n"
			"    }\n"
			"    Result[Id.xy] = float4(0.5, 0.0, 0.0, 1.0);\n"
			"}\n");
	}

	/** Line 4 does not compile: NotAName is declared nowhere. */
	inline const TCHAR* BrokenComputeShader()
	{
		return TEXT(
			"#include \"/Plugin/DreamShader/Pass/DreamPass.ush\"\n"
			"\n"
			"[numthreads(8, 8, 1)]\n"
			"void BrokenCS(uint3 Id : SV_DispatchThreadID) { Result[Id.xy] = NotAName; }\n");
	}

	inline FString MakeSingleComputePipeline(const TCHAR* Shader, const TCHAR* Entry)
	{
		return FString::Printf(TEXT(
			"buffer Grey : R16F(Size = int2(64, 64), Export = true);\n"
			"\n"
			"pass Run : compute\n"
			"{\n"
			"    Injection = AfterOpaque;\n"
			"    Shader = \"%s\";\n"
			"    Entry = %s;\n"
			"    write Result = Grey;\n"
			"}\n"), Shader, Entry);
	}
}

#if DREAMSHADER_WITH_CUSTOM_PASS

// ---------------------------------------------------------------------------------------------
// Asset
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineAssetTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.Asset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompilerPipelineAssetTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	FDreamShaderCompile2Fixture Fixture(TEXT("Asset/CP_PL2Asset"), ScratchArea(), TEXT("dsp"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	FString Written;
	if (!Fixture.WriteSource(*this, MakeAssetPipeline(TEXT("int2(64, 32)"), TEXT("true")))
		|| !Fixture.WriteSiblingSource(*this, TEXT("M_PL2AssetMask.dss"), MakeMaskMaterial(TEXT("M_PL2AssetMask")), Written)
		|| !Fixture.WriteSiblingSource(*this, TEXT("PP_PL2AssetComp.dss"), MakeCompositeMaterial(TEXT("PP_PL2AssetComp")), Written))
	{
		return false;
	}
	Compiler::FDreamShaderProductIndex::Get().Refresh();

	const FString RegistryBefore = ReadRegistryJson();
	const FString PipelinePath = Fixture.MakeObjectPath(TEXT("CP_PL2Asset"));
	const FString BlurredPath = Fixture.MakeObjectPath(TEXT("CP_PL2Asset_Blurred"));
	const FString FieldPath = Fixture.MakeObjectPath(TEXT("CP_PL2Asset_Field"));

	// ---- the build: its pass materials first (DSH8332), then the pipeline
	{
		Compiler::FDreamShaderLang2PipelineResult Run;
		const bool bBuilt = RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true);
		if (!TestTrue(FString::Printf(TEXT("the .dsp builds (%s)"), *Describe(Run.Diagnostics)), bBuilt))
		{
			return false;
		}
		TestTrue(TEXT("a pass material missing on disk is compiled first, and said so (DSH8332)"), HasCode(Run.Diagnostics, TEXT("DSH8332"), Lang::ELangSeverity::Info));
	}

	UDreamPassPipeline* Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *PipelinePath);
	if (!TestNotNull(TEXT("the .dsp made a UDreamPassPipeline named after its file, beside it"), Pipeline))
	{
		return false;
	}

	// ---- the data
	TestEqual(TEXT("Order"), Pipeline->Order, 7);
	TestEqual(TEXT("Views"), Pipeline->Views, int32(EDreamPassViewFlags::Game | EDreamPassViewFlags::SceneCapture));
	TestTrue(TEXT("the default injection point"), Pipeline->DefaultInjection == EDreamPassInjection::BeforePostProcess);
	if (TestEqual(TEXT("two parameters"), Pipeline->Parameters.Num(), 2))
	{
		TestTrue(TEXT("in source order"), Pipeline->Parameters[0].Name == FName(TEXT("Tint")) && Pipeline->Parameters[1].Name == FName(TEXT("Width")));
		TestTrue(TEXT("with their group"), Pipeline->Parameters[0].Group.Equals(TEXT("Look"), ESearchCase::CaseSensitive));
	}
	if (TestEqual(TEXT("three buffers"), Pipeline->Buffers.Num(), 3))
	{
		const FDreamPassBufferDesc* Blurred = Pipeline->FindBuffer(FName(TEXT("Blurred")));
		const FDreamPassBufferDesc* Field = Pipeline->FindBuffer(FName(TEXT("Field")));
		if (TestNotNull(TEXT("Blurred"), Blurred))
		{
			TestTrue(TEXT("Blurred: RG16F at half the render resolution, exported"),
				Blurred->Format == EDreamPassBufferFormat::RG16F && Blurred->Resolution == EDreamPassBufferResolution::Render && FMath::IsNearlyEqual(Blurred->Scale, 0.5f) && Blurred->bExport);
		}
		if (TestNotNull(TEXT("Field"), Field))
		{
			TestTrue(TEXT("Field: a fixed 64 x 32"), Field->Resolution == EDreamPassBufferResolution::Fixed && Field->FixedSize == FIntPoint(64, 32));
		}
	}
	if (TestEqual(TEXT("four passes"), Pipeline->Passes.Num(), 4))
	{
		const FDreamPassDesc& Draw = Pipeline->Passes[0];
		TestTrue(TEXT("DrawMask is a mesh pass at AfterOpaque"), Draw.Kind == EDreamPassKind::Mesh && Draw.Injection == EDreamPassInjection::AfterOpaque);
		TestTrue(TEXT("drawing with the material its .dss built"),
			Draw.Mesh.OverrideMaterial && Draw.Mesh.OverrideMaterial->GetPathName().Equals(Fixture.MakeObjectPath(TEXT("M_PL2AssetMask")), ESearchCase::IgnoreCase));
		const FDreamPassDesc& Composite = Pipeline->Passes[3];
		TestTrue(TEXT("Composite is a fullscreen pass drawing its material"),
			Composite.Kind == EDreamPassKind::Fullscreen && Composite.Fullscreen.Material
			&& Composite.Fullscreen.Material->GetPathName().Equals(Fixture.MakeObjectPath(TEXT("PP_PL2AssetComp")), ESearchCase::IgnoreCase));
		TestEqual(TEXT("with its two params"), Composite.Params.Num(), 2);
	}
	{
		TArray<FText> Problems;
		const bool bValid = Pipeline->Validate(&Problems);
		for (const FText& Problem : Problems)
		{
			AddInfo(Problem.ToString());
		}
		TestTrue(TEXT("the asset validates"), bValid);
	}

	// ---- the render targets of the exports
	UTextureRenderTarget2D* BlurredTarget = Pipeline->GetExportTarget(FName(TEXT("Blurred")));
	UTextureRenderTarget2D* FieldTarget = Pipeline->GetExportTarget(FName(TEXT("Field")));
	TestEqual(TEXT("one render target per exported buffer"), Pipeline->ExportTargets.Num(), 2);
	if (TestNotNull(TEXT("Blurred has its render target"), BlurredTarget))
	{
		TestTrue(TEXT("named <Pipeline>_<Buffer>, beside the pipeline"), BlurredTarget->GetPathName().Equals(BlurredPath, ESearchCase::IgnoreCase));
		TestTrue(TEXT("in the buffer's format"), BlurredTarget->RenderTargetFormat == RTF_RG16f);
		TestTrue(TEXT("clamped, a view-sized buffer"), BlurredTarget->AddressX == TA_Clamp && BlurredTarget->AddressY == TA_Clamp);
		TestTrue(TEXT("64 x 64 until the runtime sizes it"), BlurredTarget->SizeX == 64 && BlurredTarget->SizeY == 64);
		TestTrue(TEXT("linear, without mips"), BlurredTarget->bForceLinearGamma && !BlurredTarget->bAutoGenerateMips);
		TestTrue(TEXT("cleared to the buffer's clear value"), BlurredTarget->ClearColor.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)));
		TestTrue(TEXT("stamped as DreamShader's"), HasDreamShaderSourceMetadata(BlurredTarget));
		TestTrue(TEXT("and as its pipeline's"), GetAssetMetadata(BlurredTarget, TEXT("DreamShader.PassPipeline")).Equals(PipelinePath, ESearchCase::IgnoreCase));
	}
	if (TestNotNull(TEXT("Field has its render target"), FieldTarget))
	{
		TestTrue(TEXT("a fixed size wraps"), FieldTarget->AddressX == TA_Wrap && FieldTarget->AddressY == TA_Wrap);
		TestTrue(TEXT("and has that size"), FieldTarget->SizeX == 64 && FieldTarget->SizeY == 32);
		TestTrue(TEXT("RGBA16F"), FieldTarget->RenderTargetFormat == RTF_RGBA16f);
	}

	// ---- the stamps, and what a resolution without a build says
	const FString StampedHash = GetGeneratedAssetSourceHash(Pipeline);
	TestFalse(TEXT("the pipeline carries its build key"), StampedHash.IsEmpty());
	TestFalse(TEXT("and an output digest"), GetOutputDigestMetadata(Pipeline).IsEmpty());
	TestTrue(TEXT("which says it is as generated"), ClassifyGeneratedAsset(Pipeline) == EDreamShaderDigestState::Generated);
	TestTrue(TEXT("its source file is the .dsp"), GetGeneratedAssetSourceFile(Pipeline).EndsWith(TEXT("CP_PL2Asset.dsp"), ESearchCase::IgnoreCase));
	{
		Compiler::FDreamShaderProductResolution Resolution;
		if (TestTrue(FString::Printf(TEXT("the .dsp resolves (%s)"), *Describe(Resolution.Diagnostics)), Compiler::ResolveDreamShaderSourceProducts(Fixture.GetSourceFilePath(), Resolution)))
		{
			ExpectString(*this, TEXT("the resolution's build key is the one stamped"), Resolution.SourceHash, StampedHash);
			if (TestEqual(TEXT("one product"), Resolution.Products.Num(), 1))
			{
				const Compiler::FDreamShaderResolvedProduct& Product = Resolution.Products[0];
				TestTrue(TEXT("a PassPipeline"), Product.Kind == IR::EIRProductKind::PassPipeline);
				TestTrue(TEXT("at the pipeline's path"), Product.ObjectPath.Equals(PipelinePath, ESearchCase::IgnoreCase));
				TestTrue(TEXT("with the render targets of its exports, in buffer order"),
					Product.ExportTargetObjectPaths.Num() == 2
					&& Product.ExportTargetObjectPaths[0].Equals(BlurredPath, ESearchCase::IgnoreCase)
					&& Product.ExportTargetObjectPaths[1].Equals(FieldPath, ESearchCase::IgnoreCase));
			}
		}
	}
	TestTrue(TEXT("a pipeline without HLSL passes leaves Registry.json as it was"), ReadRegistryJson().Equals(RegistryBefore, ESearchCase::CaseSensitive));

	// ---- the build-key skip, and a forced build
	{
		const uint32 Revision = Pipeline->GetRevision();
		FString Message;
		TestTrue(FString::Printf(TEXT("an unchanged .dsp compiles (%s)"), *Message), Compile(Fixture.GetSourceFilePath(), Message));
		TestEqual(TEXT("and is skipped on its build key: the asset did not change"), Pipeline->GetRevision(), Revision);
		TestTrue(FString::Printf(TEXT("a forced build compiles (%s)"), *Message), Compile(Fixture.GetSourceFilePath(), Message, /*bForce*/ true));
		TestNotEqual(TEXT("and changes the asset"), Pipeline->GetRevision(), Revision);
	}

	// ---- an edit: the same asset and the same render targets, set up again
	{
		const TWeakObjectPtr<UTextureRenderTarget2D> OldBlurred = BlurredTarget;
		const TWeakObjectPtr<UTextureRenderTarget2D> OldField = FieldTarget;
		if (!Fixture.WriteSource(*this, MakeAssetPipeline(TEXT("int2(128, 16)"), TEXT("true"))))
		{
			return false;
		}
		FString Message;
		TestTrue(FString::Printf(TEXT("the edited .dsp compiles (%s)"), *Message), Compile(Fixture.GetSourceFilePath(), Message));
		TestTrue(TEXT("the pipeline is the same object"), LoadObject<UDreamPassPipeline>(nullptr, *PipelinePath) == Pipeline);
		TestTrue(TEXT("the render targets are reused in place"),
			OldBlurred.IsValid() && OldField.IsValid()
			&& Pipeline->GetExportTarget(FName(TEXT("Blurred"))) == OldBlurred.Get()
			&& Pipeline->GetExportTarget(FName(TEXT("Field"))) == OldField.Get());
		TestTrue(TEXT("and the fixed one has its new size"), OldField.IsValid() && OldField->SizeX == 128 && OldField->SizeY == 16);
	}

	// ---- an export that goes: its render target is deleted, or kept and said so when something still reads it
	{
		if (!Fixture.WriteSource(*this, MakeAssetPipeline(TEXT("int2(128, 16)"), TEXT("false"))))
		{
			return false;
		}
		Compiler::FDreamShaderLang2PipelineResult Run;
		TestTrue(FString::Printf(TEXT("a .dsp that stops exporting a buffer compiles (%s)"), *Describe(Run.Diagnostics)), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true));
		TestFalse(TEXT("the buffer has no render target in the asset any more"), Pipeline->ExportTargets.Contains(FName(TEXT("Blurred"))));
		TestTrue(TEXT("the stale render target was deleted (DSH8312) or kept with a warning (DSH8310)"),
			HasCode(Run.Diagnostics, TEXT("DSH8312"), Lang::ELangSeverity::Info) || HasCode(Run.Diagnostics, TEXT("DSH8310"), Lang::ELangSeverity::Warning));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// OwnershipGuard
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineOwnershipGuardTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.OwnershipGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompilerPipelineOwnershipGuardTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	const FString PipelineSource = MakeClearPipeline(TEXT("buffer Glow : R8(Export = true);\n"), TEXT("Glow"));

	// ---- the pipeline's own path
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("Guard/CP_PL2Guard"), ScratchArea(), TEXT("dsp"));
		AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
		if (!Fixture.WriteSource(*this, PipelineSource))
		{
			return false;
		}
		const FString PackageName = Fixture.GetPackagePath() + TEXT("/CP_PL2Guard");

		// Another class where the pipeline goes.
		{
			UPackage* Package = CreatePackage(*PackageName);
			UTextureRenderTarget2D* Squatter = NewObject<UTextureRenderTarget2D>(Package, FName(TEXT("CP_PL2Guard")), RF_Public | RF_Standalone);
			ON_SCOPE_EXIT
			{
				DiscardInMemoryObject(Squatter);
			};
			Compiler::FDreamShaderLang2PipelineResult Run;
			TestFalse(TEXT("a .dsp does not build over an asset of another class"), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true));
			const Lang::FLangDiagnostic* Failure = FindCode(Run.Diagnostics, TEXT("DSH8301"));
			TestTrue(FString::Printf(TEXT("DSH8301, wrapping DSH8302 (%s)"), *Describe(Run.Diagnostics)), Failure && Failure->Message.ToString().Contains(TEXT("DSH8302")));
		}

		// An asset of the class DreamShader did not make (its stamps cleared, its package on disk).
		{
			FString Message;
			if (!TestTrue(FString::Printf(TEXT("the .dsp builds where nothing is in the way (%s)"), *Message), Compile(Fixture.GetSourceFilePath(), Message, /*bForce*/ true)))
			{
				return false;
			}
			UDreamPassPipeline* Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *Fixture.MakeObjectPath(TEXT("CP_PL2Guard")));
			if (TestNotNull(TEXT("the pipeline"), Pipeline))
			{
				ClearDreamShaderMetadata(Pipeline);
				Compiler::FDreamShaderLang2PipelineResult Run;
				TestFalse(TEXT("a pipeline DreamShader did not make is not taken over"), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true));
				const Lang::FLangDiagnostic* Failure = FindCode(Run.Diagnostics, TEXT("DSH8301"));
				TestTrue(FString::Printf(TEXT("DSH8301, wrapping DSH8303 (%s)"), *Describe(Run.Diagnostics)), Failure && Failure->Message.ToString().Contains(TEXT("DSH8303")));
			}
		}
	}

	// ---- an exported buffer's render target path
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("Guard/CP_PL2GuardRT"), ScratchArea(), TEXT("dsp"));
		AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		if (!Fixture.WriteSource(*this, PipelineSource))
		{
			return false;
		}
		const FString TargetName = TEXT("CP_PL2GuardRT_Glow");

		{
			UPackage* Package = CreatePackage(*(Fixture.GetPackagePath() + TEXT("/") + TargetName));
			UDreamPassPipeline* Squatter = NewObject<UDreamPassPipeline>(Package, FName(*TargetName), RF_Public | RF_Standalone);
			ON_SCOPE_EXIT
			{
				DiscardInMemoryObject(Squatter);
			};
			Compiler::FDreamShaderLang2PipelineResult Run;
			TestFalse(TEXT("an export does not take over an asset of another class"), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true));
			const Lang::FLangDiagnostic* Failure = FindCode(Run.Diagnostics, TEXT("DSH8309"));
			TestTrue(FString::Printf(TEXT("DSH8309, wrapping DSH8313 (%s)"), *Describe(Run.Diagnostics)), Failure && Failure->Message.ToString().Contains(TEXT("DSH8313")));
		}
		{
			FString Message;
			if (!TestTrue(FString::Printf(TEXT("the .dsp builds where nothing is in the way (%s)"), *Message), Compile(Fixture.GetSourceFilePath(), Message, /*bForce*/ true)))
			{
				return false;
			}
			UTextureRenderTarget2D* Target = LoadObject<UTextureRenderTarget2D>(nullptr, *Fixture.MakeObjectPath(TargetName));
			if (TestNotNull(TEXT("the render target"), Target))
			{
				ClearDreamShaderMetadata(Target);
				Compiler::FDreamShaderLang2PipelineResult Run;
				TestFalse(TEXT("a render target DreamShader did not make is not taken over"), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true));
				const Lang::FLangDiagnostic* Failure = FindCode(Run.Diagnostics, TEXT("DSH8309"));
				TestTrue(FString::Printf(TEXT("DSH8309, wrapping DSH8313 (%s)"), *Describe(Run.Diagnostics)), Failure && Failure->Message.ToString().Contains(TEXT("DSH8313")));
			}
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Dependencies
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineDependenciesTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.Dependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompilerPipelineDependenciesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	FDreamShaderCompile2Fixture Fixture(TEXT("Dependencies/CP_PL2Dep"), ScratchArea(), TEXT("dsp"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	FString MaskPath;
	FString ReaderPath;
	FString FreshPath;
	FString BrokenPath;
	FString BadPath;
	FString BadMaskPath;
	if (!Fixture.WriteSource(*this, MakeExportingPipeline(TEXT("M_PL2DepMask"), TEXT("R8")))
		|| !Fixture.WriteSiblingSource(*this, TEXT("M_PL2DepMask.dss"), MakeMaskMaterial(TEXT("M_PL2DepMask")), MaskPath)
		|| !Fixture.WriteSiblingSource(*this, TEXT("M_PL2DepReader.dss"), MakeReaderMaterial(TEXT("M_PL2DepReader"), TEXT("CP_PL2Dep"), TEXT("Glow")), ReaderPath)
		// Never compiled: the `.dss` checks read its buffers off the source.
		|| !Fixture.WriteSiblingSource(*this, TEXT("CP_PL2DepFresh.dsp"), MakeClearPipeline(TEXT("buffer Shown : R8(Export = true);\nbuffer Hidden : R8;\n"), TEXT("Shown")), FreshPath)
		// Does not bind: R7 is no format.
		|| !Fixture.WriteSiblingSource(*this, TEXT("CP_PL2DepBroken.dsp"), MakeClearPipeline(TEXT("buffer Glow : R7(Export = true);\n"), TEXT("Glow")), BrokenPath)
		// Its pass material does not compile. It binds and lowers -- the product index, whose refresh skips the
		// UE.DreamPassBuffer check, knows its product -- and fails that check when it is run (no CP_PL2Nowhere: DSH5316).
		|| !Fixture.WriteSiblingSource(*this, TEXT("CP_PL2DepBad.dsp"), MakeExportingPipeline(TEXT("M_PL2DepBadMask"), TEXT("R8")), BadPath)
		|| !Fixture.WriteSiblingSource(*this, TEXT("M_PL2DepBadMask.dss"), TEXT(
			"#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)\n"
			"\n"
			"export void M_PL2DepBadMask(inout material m)\n"
			"{\n"
			"    float Glow = UE.DreamPassBuffer(Pipeline = \"CP_PL2Nowhere\", Buffer = \"Glow\").r;\n"
			"    m.EmissiveColor = float3(0.0, 0.0, 0.0);\n"
			"    UE.DreamPassOutput(Output0 = float4(Glow, 0.0, 0.0, 0.0));\n"
			"}\n"), BadMaskPath))
	{
		return false;
	}
	const FString DspPath = Fixture.GetSourceFilePath();
	Compiler::FDreamShaderProductIndex::Get().Refresh();

	const auto ContainsPath = [](const TArray<FString>& Paths, const FString& Path)
	{
		return Paths.ContainsByPredicate([&Path](const FString& Candidate) { return FPaths::IsSamePath(Candidate, Path); });
	};

	// ---- the edges, before anything is built
	{
		TArray<FString> Materials;
		Compiler::FindPipelineMaterialSourceFiles(DspPath, Materials);
		TestTrue(TEXT("a .dsp depends on the .dss of its pass material"), ContainsPath(Materials, MaskPath));
		TArray<FString> Pipelines;
		Compiler::FindPassBufferPipelineSourceFiles(ReaderPath, Pipelines);
		TestTrue(TEXT("a .dss depends on the .dsp whose buffer it reads"), ContainsPath(Pipelines, DspPath));

		TArray<FString> Order = { ReaderPath, DspPath, MaskPath };
		FDreamShaderDependencyGraphService::SortByDependencyOrder(Order);
		const int32 Mask = Order.IndexOfByPredicate([&MaskPath](const FString& Path) { return FPaths::IsSamePath(Path, MaskPath); });
		const int32 Dsp = Order.IndexOfByPredicate([&DspPath](const FString& Path) { return FPaths::IsSamePath(Path, DspPath); });
		const int32 Reader = Order.IndexOfByPredicate([&ReaderPath](const FString& Path) { return FPaths::IsSamePath(Path, ReaderPath); });
		TestTrue(FString::Printf(TEXT("the sort puts the material, the pipeline, the reader in that order (%d, %d, %d)"), Mask, Dsp, Reader), Mask < Dsp && Dsp < Reader);
	}

	// ---- the build: the pass material compiled first
	{
		Compiler::FDreamShaderLang2PipelineResult Run;
		if (!TestTrue(FString::Printf(TEXT("the .dsp builds (%s)"), *Describe(Run.Diagnostics)), RunPipeline(DspPath, Run, /*bEmitAssets*/ true, /*bForce*/ true)))
		{
			return false;
		}
		TestTrue(TEXT("its pass material was compiled first (DSH8332)"), HasCode(Run.Diagnostics, TEXT("DSH8332"), Lang::ELangSeverity::Info));
		Compiler::FDreamShaderProductIndex::Get().Refresh();

		TArray<FString> Pipelines;
		Compiler::CollectPipelineDependents(MaskPath, Pipelines);
		TestTrue(TEXT("a rebuilt pass material queues its pipeline"), ContainsPath(Pipelines, DspPath));
		TArray<FString> Readers;
		Compiler::CollectPassBufferDependents(DspPath, Readers);
		TestTrue(TEXT("a rebuilt pipeline queues the materials reading its exports"), ContainsPath(Readers, ReaderPath));
	}

	// ---- the reader, built against the export; an export that changes makes it stale
	{
		FString Message;
		if (!TestTrue(FString::Printf(TEXT("the reader builds (%s)"), *Message), Compile(ReaderPath, Message, /*bForce*/ true)))
		{
			return false;
		}
		UObject* ReaderAsset = LoadObject<UObject>(nullptr, *Fixture.MakeObjectPath(TEXT("M_PL2DepReader")));
		if (TestNotNull(TEXT("the reader's material"), ReaderAsset))
		{
			Compiler::FDreamShaderProductResolution Before;
			Compiler::ResolveDreamShaderSourceProducts(ReaderPath, Before);
			ExpectString(*this, TEXT("the reader is current"), Before.SourceHash, GetGeneratedAssetSourceHash(ReaderAsset));

			if (!Fixture.WriteSource(*this, MakeExportingPipeline(TEXT("M_PL2DepMask"), TEXT("RG16F"))))
			{
				return false;
			}
			Compiler::FDreamShaderLang2PipelineResult Run;
			TestTrue(FString::Printf(TEXT("the .dsp with a new export format builds (%s)"), *Describe(Run.Diagnostics)), RunPipeline(DspPath, Run, /*bEmitAssets*/ true));
			Compiler::FDreamShaderProductResolution After;
			Compiler::ResolveDreamShaderSourceProducts(ReaderPath, After);
			TestFalse(TEXT("the export's format is in the reader's build key: it is stale now"), After.SourceHash.Equals(GetGeneratedAssetSourceHash(ReaderAsset), ESearchCase::CaseSensitive));
		}
	}

	// ---- a pass material that changes moves the pipeline's build key
	{
		Compiler::FDreamShaderProductResolution Before;
		Compiler::ResolveDreamShaderSourceProducts(DspPath, Before);
		FString Written;
		FString Message;
		if (!Fixture.WriteSiblingSource(*this, TEXT("M_PL2DepMask.dss"), MakeMaskMaterial(TEXT("M_PL2DepMask"), TEXT("0.5")), Written))
		{
			return false;
		}
		TestTrue(FString::Printf(TEXT("the edited pass material builds (%s)"), *Message), Compile(MaskPath, Message));
		Compiler::FDreamShaderProductResolution After;
		Compiler::ResolveDreamShaderSourceProducts(DspPath, After);
		TestFalse(TEXT("the material's own build key is in the pipeline's"), After.SourceHash.Equals(Before.SourceHash, ESearchCase::CaseSensitive));
	}

	// ---- the `.dss` checks of UE.DreamPassBuffer, in a check run (nothing built)
	struct FReaderCase
	{
		const TCHAR* Name;
		const TCHAR* Pipeline;
		const TCHAR* Buffer;
		/** The code expected at the UE.DreamPassBuffer line; null: the reader checks clean. */
		const TCHAR* Code;
	};
	const FReaderCase Cases[] =
	{
		// The built asset's path: the export target found by FName.
		{ TEXT("M_PL2DepNoBuffer"), TEXT("CP_PL2Dep"), TEXT("Nope"), TEXT("DSH5321") },
		{ TEXT("M_PL2DepCase"), TEXT("CP_PL2Dep"), TEXT("glow"), nullptr },
		// No `.dsp` builds it.
		{ TEXT("M_PL2DepNowhere"), TEXT("CP_PL2Nowhere"), TEXT("Glow"), TEXT("DSH5316") },
		// The source's path: a `.dsp` never compiled, read as it declares its buffers.
		{ TEXT("M_PL2DepFreshMissing"), TEXT("CP_PL2DepFresh"), TEXT("Missing"), TEXT("DSH5322") },
		{ TEXT("M_PL2DepFreshHidden"), TEXT("CP_PL2DepFresh"), TEXT("Hidden"), TEXT("DSH5322") },
		{ TEXT("M_PL2DepFreshCase"), TEXT("CP_PL2DepFresh"), TEXT("shown"), nullptr },
		// A `.dsp` that is there and does not compile: said as that (DSH5319), not as a pipeline nobody builds.
		{ TEXT("M_PL2DepBrokenRead"), TEXT("CP_PL2DepBroken"), TEXT("Glow"), TEXT("DSH5319") },
	};
	for (const FReaderCase& Case : Cases)
	{
		const FString Text = MakeReaderMaterial(Case.Name, Case.Pipeline, Case.Buffer);
		FString Path;
		if (!Fixture.WriteSiblingSource(*this, FString(Case.Name) + TEXT(".dss"), Text, Path))
		{
			return false;
		}
		Compiler::FDreamShaderLang2PipelineResult Run;
		const bool bChecked = RunPipeline(Path, Run, /*bEmitAssets*/ false);
		if (!Case.Code)
		{
			TestTrue(FString::Printf(TEXT("%s: Buffer = \"%s\" of %s checks clean -- buffer names compare ignoring case (%s)"), Case.Name, Case.Buffer, Case.Pipeline, *Describe(Run.Diagnostics)), bChecked && !Run.Diagnostics.HasErrors());
			continue;
		}
		const int32 Line = LineOf(Text, TEXT("UE.DreamPassBuffer("));
		const bool bThere = Run.Diagnostics.GetDiagnostics().ContainsByPredicate([&Case, Line](const Lang::FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == Lang::ELangSeverity::Error && Diagnostic.Code.Equals(Case.Code, ESearchCase::CaseSensitive) && Diagnostic.Span.Line == Line;
		});
		TestTrue(FString::Printf(TEXT("%s: %s on line %d (%s)"), Case.Name, Case.Code, Line, *Describe(Run.Diagnostics)), bThere);
		if (FCString::Strcmp(Case.Code, TEXT("DSH5319")) == 0)
		{
			TestFalse(TEXT("a .dsp that does not compile is not reported as a pipeline nobody builds (DSH5316, DSH5318)"),
				HasCode(Run.Diagnostics, TEXT("DSH5316")) || HasCode(Run.Diagnostics, TEXT("DSH5318")));
		}
	}

	// ---- a pass material whose `.dss` does not compile, in a check run of its pipeline
	{
		Compiler::FDreamShaderLang2PipelineResult Run;
		TestFalse(TEXT("a pipeline whose pass material does not compile does not check"), RunPipeline(BadPath, Run, /*bEmitAssets*/ false));
		TestTrue(FString::Printf(TEXT("said as a material source that does not compile (DSH8331) (%s)"), *Describe(Run.Diagnostics)), HasCode(Run.Diagnostics, TEXT("DSH8331"), Lang::ELangSeverity::Error));
		TestFalse(TEXT("not as a material nobody builds (DSH4406)"), HasCode(Run.Diagnostics, TEXT("DSH4406")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// SelfRead: a pass material reading its own pipeline's export
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineSelfReadTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.SelfRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompilerPipelineSelfReadTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	// The material of the mesh pass samples the pipeline's own export: the two can be built in no order. Each order on a
	// pair of its own, so that each starts from nothing built.
	const auto MakeLoopMaterial = [](const FString& Name, const FString& Pipeline)
	{
		return FString::Printf(TEXT(
			"#pragma material(ShadingModel = Unlit, bUsedWithSkeletalMesh = true, bUsedWithInstancedStaticMeshes = true)\n"
			"\n"
			"export void %s(inout material m)\n"
			"{\n"
			"    float Glow = UE.DreamPassBuffer(Pipeline = \"%s\", Buffer = \"Glow\").r;\n"
			"    m.EmissiveColor = float3(0.0, 0.0, 0.0);\n"
			"    UE.DreamPassOutput(Output0 = float4(Glow, 0.0, 0.0, 0.0));\n"
			"}\n"), *Name, *Pipeline);
	};

	// ---- the pipeline first: its pass material is compiled first, and refuses to read the export (DSH5325 inside DSH8331)
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("SelfRead/CP_PL2LoopA"), ScratchArea(), TEXT("dsp"));
		AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		FString Written;
		if (!Fixture.WriteSource(*this, MakeExportingPipeline(TEXT("M_PL2LoopAMask"), TEXT("R8")))
			|| !Fixture.WriteSiblingSource(*this, TEXT("M_PL2LoopAMask.dss"), MakeLoopMaterial(TEXT("M_PL2LoopAMask"), TEXT("CP_PL2LoopA")), Written))
		{
			return false;
		}
		Compiler::FDreamShaderProductIndex::Get().Refresh();
		Compiler::FDreamShaderLang2PipelineResult Run;
		TestFalse(TEXT("a pipeline whose pass material reads its export does not build"), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true));
		TestTrue(FString::Printf(TEXT("its material failed to compile (DSH8331) (%s)"), *Describe(Run.Diagnostics)), HasCode(Run.Diagnostics, TEXT("DSH8331"), Lang::ELangSeverity::Error));
		TestTrue(TEXT("because it reads its own pipeline's export (DSH5325, an error)"), HasCodeOrNested(Run.Diagnostics, TEXT("DSH5325")));
	}

	// ---- the material first: its pipeline is compiled first, and refuses the material (DSH8333 inside DSH5319)
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("SelfRead/CP_PL2LoopB"), ScratchArea(), TEXT("dsp"));
		AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		FString MaterialPath;
		if (!Fixture.WriteSource(*this, MakeExportingPipeline(TEXT("M_PL2LoopBMask"), TEXT("R8")))
			|| !Fixture.WriteSiblingSource(*this, TEXT("M_PL2LoopBMask.dss"), MakeLoopMaterial(TEXT("M_PL2LoopBMask"), TEXT("CP_PL2LoopB")), MaterialPath))
		{
			return false;
		}
		Compiler::FDreamShaderProductIndex::Get().Refresh();
		Compiler::FDreamShaderLang2PipelineResult Run;
		TestFalse(TEXT("a pass material that reads its own pipeline's export does not build"), RunPipeline(MaterialPath, Run, /*bEmitAssets*/ true, /*bForce*/ true));
		TestTrue(FString::Printf(TEXT("its pipeline failed to compile (DSH5319) (%s)"), *Describe(Run.Diagnostics)), HasCode(Run.Diagnostics, TEXT("DSH5319"), Lang::ELangSeverity::Error));
		TestTrue(TEXT("because its pass material reads its export (DSH8333, an error)"), HasCodeOrNested(Run.Diagnostics, TEXT("DSH8333")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Layers: the first UDreamPassSettings::MaxLayers names only
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineLayersTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.Layers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompilerPipelineLayersTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	TArray<FName> Layers;
	for (int32 Index = 0; Index < UDreamPassSettings::MaxLayers; ++Index)
	{
		Layers.Add(FName(*FString::Printf(TEXT("PL2Layer%02d"), Index)));
	}
	Layers.Add(FName(TEXT("PL2Beyond")));
	const FScopedPassLayers ScopedLayers(Layers);

	FDreamShaderCompile2Fixture Fixture(TEXT("Layers/CP_PL2Layers"), ScratchArea(), TEXT("dsp"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);

	const auto Check = [this, &Fixture](const TCHAR* Layer, Compiler::FDreamShaderLang2PipelineResult& Run) -> bool
	{
		const FString Text = FString::Printf(TEXT(
			"buffer Mask : R8(Export = true);\n"
			"\n"
			"pass Draw : mesh\n"
			"{\n"
			"    Injection = AfterOpaque;\n"
			"    Filter = Layer(%s);\n"
			"    write Output0 = Mask;\n"
			"}\n"), Layer);
		if (!Fixture.WriteSource(*this, Text))
		{
			return false;
		}
		RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ false);
		return true;
	};

	{
		Compiler::FDreamShaderLang2PipelineResult Run;
		if (Check(*FString::Printf(TEXT("PL2Layer%02d"), UDreamPassSettings::MaxLayers - 1), Run))
		{
			TestFalse(FString::Printf(TEXT("the last of the first %d names is a layer (%s)"), UDreamPassSettings::MaxLayers, *Describe(Run.Diagnostics)), HasCode(Run.Diagnostics, TEXT("DSH4412")));
		}
	}
	{
		Compiler::FDreamShaderLang2PipelineResult Run;
		if (Check(TEXT("PL2Beyond"), Run))
		{
			TestTrue(FString::Printf(TEXT("a name past them has no bit, and is no layer (DSH4412) (%s)"), *Describe(Run.Diagnostics)), HasCode(Run.Diagnostics, TEXT("DSH4412"), Lang::ELangSeverity::Error));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Roundtrip: `.dsp` -> asset -> `.dsp`
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineRoundtripTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompilerPipelineRoundtripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	FDreamShaderCompile2Fixture Fixture(TEXT("Roundtrip/CP_PL2Round"), ScratchArea(), TEXT("dsp"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	// Every kind of pass but the HLSL ones, keys off their defaults, and numbers that are not float32 exactly.
	const FString Source = Src(TEXT(R"DSP(
#pragma pipeline(Order = -2, Injection = AfterOpaque, Views = Game | Editor | SceneCapture, Requires = PostProcess | CustomStencil, Enabled = On)

uniform bool On = true;
/// @group Look
/// @slider 0.1 3.3
uniform float Width = 1.1;
/// The colour of the outline.
/// Two lines of it.
uniform float4 Tint = float4(0.1, 0.2, 0.3, 0.4);
uniform int Taps = 3;

buffer Mask : R8(Scale = 0.3, Clear = 0.3);
/// @desc The mask, copied.
buffer Soft : R8(Scale = 0.3, Export = true);
buffer Field : RGBA16F(Size = int2(32, 16), Clear = None, History = true, Export = true);

pass DrawMask : mesh
{
    Filter = Stencil(3, 0x0F);
    Material = "M_PL2RoundMask";
    Depth = None;
    Cull = Front;
    Blend = Max;
    Nanite = StencilMask;
    NaniteValue = float4(0.7, 0.0, 0.0, 1.0);
    write Output0 = Mask;
}

pass Soften : copy
{
    read Mask;
    write Soft;
}

pass Fill : clear
{
    Enabled = On;
    Value = float4(0.1, 0.2, 0.3, 0.4);
    write Field;
}

/// @desc Draws the outline.
pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material = "PP_PL2RoundComp";
    read Mask;
    write SceneColor;
    param Color = Tint;
    param Width = Width * 0.3 + 0.1;
}
)DSP"));

	FString Written;
	if (!Fixture.WriteSource(*this, Source)
		|| !Fixture.WriteSiblingSource(*this, TEXT("M_PL2RoundMask.dss"), MakeMaskMaterial(TEXT("M_PL2RoundMask")), Written)
		|| !Fixture.WriteSiblingSource(*this, TEXT("PP_PL2RoundComp.dss"), MakeCompositeMaterial(TEXT("PP_PL2RoundComp")), Written))
	{
		return false;
	}
	Compiler::FDreamShaderProductIndex::Get().Refresh();

	FString Message;
	if (!TestTrue(FString::Printf(TEXT("the .dsp builds (%s)"), *Message), Compile(Fixture.GetSourceFilePath(), Message, /*bForce*/ true)))
	{
		return false;
	}
	UDreamPassPipeline* Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *Fixture.MakeObjectPath(TEXT("CP_PL2Round")));
	if (!TestNotNull(TEXT("the pipeline"), Pipeline))
	{
		return false;
	}

	Compiler::FDreamShaderLang2PipelineResult SourceRun;
	RunPipeline(Fixture.GetSourceFilePath(), SourceRun, /*bEmitAssets*/ false);
	const IR::FIRPassPipeline* SourcePayload = FindPassPipelinePayload(SourceRun);
	if (!TestNotNull(FString::Printf(TEXT("the source's payload (%s)"), *Describe(SourceRun.Diagnostics)), SourcePayload))
	{
		return false;
	}

	const FString BackPath = FPaths::Combine(FPaths::GetPath(Fixture.GetSourceFilePath()), TEXT("CP_PL2Round_Back.dsp"));
	FPipelineDecompileOptions Options;
	Options.TargetSourceFilePath = BackPath;

	// ---- the asset read back as a payload
	{
		IR::FIRPassPipeline Decompiled;
		Lang::FLangDiagnosticSink Diagnostics;
		if (TestTrue(FString::Printf(TEXT("the asset decompiles (%s)"), *Describe(Diagnostics)), DecompileDreamPassPipeline(Pipeline, Options, Decompiled, Diagnostics)))
		{
			ExpectSamePipeline(*this, TEXT("the asset holds the source's payload"), *SourcePayload, Decompiled);
		}
	}

	// ---- and as text, which binds to the same payload
	FString Text;
	{
		Lang::FLangDiagnosticSink Diagnostics;
		if (!TestTrue(FString::Printf(TEXT("the asset decompiles to text (%s)"), *Describe(Diagnostics)), DecompileDreamPassPipelineToText(Pipeline, Options, Text, Diagnostics)))
		{
			return false;
		}
		for (const TCHAR* Code : { TEXT("DSH9221"), TEXT("DSH9222"), TEXT("DSH9223") })
		{
			TestFalse(FString::Printf(TEXT("its own check passes: no %s (%s)"), Code, *Describe(Diagnostics)), HasCode(Diagnostics, Code));
		}
		TestFalse(TEXT("no error"), Diagnostics.HasErrors());
		TestNull(TEXT("nothing blocks writing it back"), FindPipelineWriteBackBlocker(Diagnostics));
	}
	if (!Fixture.WriteSiblingSource(*this, TEXT("CP_PL2Round_Back.dsp"), Text, Written))
	{
		return false;
	}
	{
		Compiler::FDreamShaderLang2PipelineResult BackRun;
		const bool bChecked = RunPipeline(Written, BackRun, /*bEmitAssets*/ false);
		TestTrue(FString::Printf(TEXT("the decompiled text checks (%s)"), *Describe(BackRun.Diagnostics)), bChecked);
		if (const IR::FIRPassPipeline* BackPayload = FindPassPipelinePayload(BackRun))
		{
			ExpectSamePipeline(*this, TEXT("source -> asset -> text -> bind gives the source's payload"), *SourcePayload, *BackPayload);
		}
		if (HasAnyErrors())
		{
			AddInfo(FString::Printf(TEXT("The decompiled text:\n%s"), *Text));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// RegistryPrecheck: CheckDreamShaderPipelineSlots, nothing written
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineRegistryPrecheckTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.RegistryPrecheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderCompilerPipelineRegistryPrecheckTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	FDreamShaderCompile2Fixture Fixture(TEXT("Precheck/CP_PL2Check"), ScratchArea(), TEXT("dsp"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);

	FString GoodShader;
	FString BrokenShader;
	FString BrokenPath;
	FString NonePath;
	if (!Fixture.WriteSource(*this, MakeSingleComputePipeline(TEXT("PL2Good.usf"), TEXT("GoodCS")))
		|| !Fixture.WriteSiblingSource(*this, TEXT("PL2Good.usf"), GoodComputeShader(), GoodShader)
		|| !Fixture.WriteSiblingSource(*this, TEXT("PL2Broken.usf"), BrokenComputeShader(), BrokenShader)
		|| !Fixture.WriteSiblingSource(*this, TEXT("CP_PL2CheckBroken.dsp"), MakeSingleComputePipeline(TEXT("PL2Broken.usf"), TEXT("BrokenCS")), BrokenPath)
		|| !Fixture.WriteSiblingSource(*this, TEXT("CP_PL2CheckNone.dsp"), MakeClearPipeline(TEXT("buffer Glow : R8(Export = true);\n"), TEXT("Glow")), NonePath))
	{
		return false;
	}
	Compiler::FDreamShaderProductIndex::Get().Refresh();
	const FString RegistryBefore = ReadRegistryJson();

	// An engine or a project that cannot pre-check here: said, and nothing asserted.
	const auto CannotCheck = [this](const Lang::FLangDiagnosticSink& Diagnostics)
	{
		if (HasCode(Diagnostics, TEXT("DSH8325")) || HasCode(Diagnostics, TEXT("DSH8323")))
		{
			AddWarning(FString::Printf(TEXT("No shader format or slot shader type to pre-check with here; the pre-check cases were skipped (%s)."), *Describe(Diagnostics)));
			return true;
		}
		return false;
	};

	{
		Lang::FLangDiagnosticSink Diagnostics;
		int32 Checked = 0;
		const bool bOk = Compiler::CheckDreamShaderPipelineSlots(Fixture.GetSourceFilePath(), TArray<FName>(), Diagnostics, Checked);
		if (CannotCheck(Diagnostics))
		{
			return true;
		}
		TestTrue(FString::Printf(TEXT("a pass whose .usf compiles passes the pre-check (%s)"), *Describe(Diagnostics)), bOk);
		TestEqual(TEXT("one HLSL pass checked"), Checked, 1);
	}
	{
		Lang::FLangDiagnosticSink Diagnostics;
		int32 Checked = 0;
		TestFalse(TEXT("a pass whose .usf does not compile fails the pre-check"), Compiler::CheckDreamShaderPipelineSlots(BrokenPath, TArray<FName>(), Diagnostics, Checked));
		const bool bAtTheLine = Diagnostics.GetDiagnostics().ContainsByPredicate([](const Lang::FLangDiagnostic& Diagnostic)
		{
			return Diagnostic.Code.Equals(TEXT("DSH8322"), ESearchCase::CaseSensitive)
				&& FPaths::GetCleanFilename(Diagnostic.FilePath).Equals(TEXT("PL2Broken.usf"), ESearchCase::IgnoreCase)
				&& Diagnostic.Span.Line == 4;
		});
		TestTrue(FString::Printf(TEXT("DSH8322 at the user's file and line (PL2Broken.usf:4) (%s)"), *Describe(Diagnostics)), bAtTheLine);
	}
	{
		Lang::FLangDiagnosticSink Diagnostics;
		int32 Checked = -1;
		Compiler::CheckDreamShaderPipelineSlots(NonePath, TArray<FName>(), Diagnostics, Checked);
		TestTrue(FString::Printf(TEXT("a pipeline without HLSL passes says there was nothing to check (DSH8339) (%s)"), *Describe(Diagnostics)), HasCode(Diagnostics, TEXT("DSH8339"), Lang::ELangSeverity::Info));
		TestEqual(TEXT("and checked none"), Checked, 0);
	}
	TestTrue(TEXT("a pre-check writes nothing: Registry.json is as it was"), ReadRegistryJson().Equals(RegistryBefore, ESearchCase::CaseSensitive));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Registry: a compile that commits slots. Backs the registry folder up and restores it.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineRegistryTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FDreamShaderCompilerPipelineRegistryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	// Declared first, destroyed last: the registry is restored after the fixture has deleted the assets and the sources.
	FScopedDreamPassRegistryBackup RegistryBackup;
	if (!TestTrue(TEXT("the slot registry folder was copied away before anything is committed to it"), RegistryBackup.IsValid()))
	{
		return false;
	}

	FDreamShaderCompile2Fixture Fixture(TEXT("Registry/CP_PL2Reg"), ScratchArea(), TEXT("dsp"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	const FString Parameters1 = TEXT(
		"uniform float Gain = 1.0;\n"
		"uniform float3 Tint = float3(1.0, 0.5, 0.25);\n"
		"uniform int Count = 2;\n"
		"uniform bool On = true;\n"
		"uniform float2 Pair = float2(0.25, 0.75);\n"
		"\n"
		"buffer Grey : R16F(Size = int2(64, 64));\n"
		"buffer Soft : RGBA16F;\n"
		"buffer Late : RGBA16F(Export = true);\n"
		"\n");
	const FString BlurPass = TEXT(
		"pass Blur : compute\n"
		"{\n"
		"    Injection = AfterOpaque;\n"
		"    Shader = \"PL2Blur.usf\";\n"
		"    Entry = BlurCS;\n"
		"    read Source = Grey;\n"
		"    write Result = Soft;\n"
		"    param Gain = Gain;\n"
		"    param Tint = Tint;\n"
		"    param Count = Count;\n"
		"    param On = On;\n"
		"    param Pair = Pair;\n"
		"}\n"
		"\n");
	const FString TonePass = TEXT(
		"pass Tone : fullscreen\n"
		"{\n"
		"    Injection = BeforePostProcess;\n"
		"    Shader = \"PL2Tone.usf\";\n"
		"    Entry = TonePS;\n"
		"    read In = Soft;\n"
		"    write Out = SceneColor;\n"
		"}\n");
	const FString SeedCompute = TEXT(
		"pass Seed : compute\n"
		"{\n"
		"    Injection = BeginView;\n"
		"    Shader = \"PL2Seed.usf\";\n"
		"    Entry = SeedCS;\n"
		"    write Result = Grey;\n"
		"}\n"
		"\n");
	const FString SeedClear = TEXT(
		"pass Seed : clear\n"
		"{\n"
		"    Injection = BeginView;\n"
		"    Value = 0.5;\n"
		"    write Grey;\n"
		"}\n"
		"\n");
	const FString SmoothPass = TEXT(
		"pass Smooth : compute\n"
		"{\n"
		"    Injection = AfterOpaque;\n"
		"    Shader = \"PL2Blur.usf\";\n"
		"    Entry = BlurCS;\n"
		"    read Source = Soft;\n"
		"    write Result = Late;\n"
		"    param Gain = Gain;\n"
		"    param Tint = Tint;\n"
		"    param Count = Count;\n"
		"    param On = On;\n"
		"    param Pair = Pair;\n"
		"}\n"
		"\n");

	FString Written;
	if (!Fixture.WriteSource(*this, Parameters1 + SeedCompute + BlurPass + TonePass)
		|| !Fixture.WriteSiblingSource(*this, TEXT("PL2Seed.usf"), TEXT(
			"#include \"/Plugin/DreamShader/Pass/DreamPass.ush\"\n"
			"\n"
			"[numthreads(8, 8, 1)]\n"
			"void SeedCS(uint3 Id : SV_DispatchThreadID)\n"
			"{\n"
			"    if (any(Id.xy >= DP_DispatchSize.xy))\n"
			"    {\n"
			"        return;\n"
			"    }\n"
			"    Result[Id.xy] = float4(frac(DP_Time.x), 0.0, 0.0, 1.0);\n"
			"}\n"), Written)
		|| !Fixture.WriteSiblingSource(*this, TEXT("PL2Blur.usf"), TEXT(
			"#include \"/Plugin/DreamShader/Pass/DreamPass.ush\"\n"
			"\n"
			"[numthreads(8, 8, 1)]\n"
			"void BlurCS(uint3 Id : SV_DispatchThreadID)\n"
			"{\n"
			"    if (any(Id.xy >= DP_DispatchSize.xy))\n"
			"    {\n"
			"        return;\n"
			"    }\n"
			"    const float2 UV = (Id.xy + 0.5) * ResultSize.zw;\n"
			"    const float Base = Source.SampleLevel(DP_LinearClamp, UV, 0).r;\n"
			"    const float Extra = (On ? 1.0 : 0.0) + (float)Count * 0.0 + Pair.x * 0.0;\n"
			"    Result[Id.xy] = float4(Base * Gain * Tint + Extra, 1.0);\n"
			"}\n"), Written)
		|| !Fixture.WriteSiblingSource(*this, TEXT("PL2Tone.usf"), TEXT(
			"#include \"/Plugin/DreamShader/Pass/DreamPass.ush\"\n"
			"\n"
			"void TonePS(float4 SvPosition : SV_POSITION, out float4 OutColor0 : SV_Target0)\n"
			"{\n"
			"    const float2 UV = (SvPosition.xy - DP_ViewRect.xy) * OutSize.zw;\n"
			"    OutColor0 = In.SampleLevel(DP_LinearClamp, UV, 0);\n"
			"}\n"), Written))
	{
		return false;
	}
	Compiler::FDreamShaderProductIndex::Get().Refresh();
	const FString PipelinePath = Fixture.MakeObjectPath(TEXT("CP_PL2Reg"));

	// ---- v1: three HLSL passes take three slots
	{
		Compiler::FDreamShaderLang2PipelineResult Run;
		const bool bBuilt = RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true);
		if (HasCode(Run.Diagnostics, TEXT("DSH8325")) || HasCode(Run.Diagnostics, TEXT("DSH8323")))
		{
			AddWarning(FString::Printf(TEXT("No shader format or slot shader type to pre-check with here; the registry cases were skipped (%s)."), *Describe(Run.Diagnostics)));
			return true;
		}
		if (!TestTrue(FString::Printf(TEXT("v1 builds and commits its slots (%s)"), *Describe(Run.Diagnostics)), bBuilt))
		{
			return false;
		}
	}

	Compiler::FDreamPassRegistryReport Report;
	FString ReportError;
	if (!TestTrue(FString::Printf(TEXT("Registry.json reads (%s)"), *ReportError), Compiler::DescribeDreamPassRegistry(/*bClassify*/ false, Report, ReportError)))
	{
		return false;
	}
	const int32 SeedSlot = FindSlot(Report, PipelinePath, TEXT("Seed"), /*bCompute*/ true);
	const int32 BlurSlot = FindSlot(Report, PipelinePath, TEXT("Blur"), /*bCompute*/ true);
	const int32 ToneSlot = FindSlot(Report, PipelinePath, TEXT("Tone"), /*bCompute*/ false);
	TestTrue(FString::Printf(TEXT("Seed and Blur have compute slots, Tone a pixel one (%d, %d, %d)"), SeedSlot, BlurSlot, ToneSlot),
		SeedSlot != INDEX_NONE && BlurSlot != INDEX_NONE && ToneSlot != INDEX_NONE && SeedSlot != BlurSlot);
	{
		UDreamPassPipeline* Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *PipelinePath);
		if (TestNotNull(TEXT("the pipeline"), Pipeline))
		{
			const int32 Blur = Pipeline->FindPassIndex(FName(TEXT("Blur")));
			const int32 Tone = Pipeline->FindPassIndex(FName(TEXT("Tone")));
			TestTrue(TEXT("the asset's passes point at their slots"),
				Pipeline->Passes.IsValidIndex(Blur) && Pipeline->Passes[Blur].Compute.Slot == BlurSlot
				&& Pipeline->Passes.IsValidIndex(Tone) && Pipeline->Passes[Tone].Fullscreen.PixelSlot == ToneSlot);
		}
	}

	// ---- the sections: every name a pass binds as a #define of the slot's parameters, the params packed as the runtime packs them
	if (BlurSlot != INDEX_NONE)
	{
		const FString Section = FindRegistrySection(/*bCompute*/ true, BlurSlot);
		TArray<FName> Names = { FName(TEXT("Gain")), FName(TEXT("Tint")), FName(TEXT("Count")), FName(TEXT("On")), FName(TEXT("Pair")) };
		TArray<EDreamPassParameterType> Types = { EDreamPassParameterType::Float, EDreamPassParameterType::Float3, EDreamPassParameterType::Int, EDreamPassParameterType::Bool, EDreamPassParameterType::Float2 };
		TArray<FDreamPassSlotParamLocation> Locations;
		TestTrue(TEXT("the five params fit a slot"), UE::DreamPass::LayoutSlotParameters(Names, Types, Locations));

		TArray<TPair<FString, FString>> Defines;
		Defines.Emplace(FString(TEXT("Source")), FString(TEXT("DP_Input0")));
		Defines.Emplace(FString(TEXT("SourceSize")), FString(TEXT("DP_InputSize[0]")));
		Defines.Emplace(FString(TEXT("SourceUVRect")), FString(TEXT("DP_InputUVRect[0]")));
		Defines.Emplace(FString(TEXT("Result")), FString(TEXT("DP_Output0")));
		Defines.Emplace(FString(TEXT("ResultSize")), FString(TEXT("DP_OutputSize[0]")));
		for (const FDreamPassSlotParamLocation& Location : Locations)
		{
			Defines.Emplace(Location.Name.ToString(), MakeSlotParamExpression(Location));
		}
		Defines.Emplace(FString(TEXT("BlurCS")), FString(TEXT("DreamPassMainCS")));

		FString DefineBlock;
		FString UndefBlock;
		for (const TPair<FString, FString>& Define : Defines)
		{
			DefineBlock += FString::Printf(TEXT("#define %s %s\n"), *Define.Key, *Define.Value);
			UndefBlock += FString::Printf(TEXT("#undef %s\n"), *Define.Key);
		}

		TestTrue(TEXT("Blur's section opens its slot"), Section.StartsWith(FString::Printf(TEXT("#if DP_SLOT == %d\n#define DP_SLOT_DEFINED 1\n// CP_PL2Reg.Blur -- "), BlurSlot), ESearchCase::CaseSensitive));
		TestTrue(TEXT("and defines its names in binding order, then the params, then the entry"), Section.Contains(DefineBlock, ESearchCase::CaseSensitive));
		TestTrue(TEXT("includes its snapshot from the slot's directory"),
			Section.Contains(FString::Printf(TEXT("#include \"%s/"), *UE::DreamPass::GetSlotVirtualDirectory(/*bCompute*/ true, BlurSlot)), ESearchCase::CaseSensitive));
		TestTrue(TEXT("and undefines every name before it closes"), Section.EndsWith(UndefBlock + TEXT("#endif\n"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("its snapshot is on disk"), IFileManager::Get().DirectoryExists(*UE::DreamPass::GetSlotDirectory(/*bCompute*/ true, BlurSlot)));
		if (HasAnyErrors())
		{
			AddInfo(FString::Printf(TEXT("Blur's section:\n%s\nexpected the defines:\n%s"), *Section, *DefineBlock));
		}
	}
	if (SeedSlot != INDEX_NONE)
	{
		TestTrue(TEXT("a pass at BeginView makes `View` a name that does not compile"),
			FindRegistrySection(/*bCompute*/ true, SeedSlot).Contains(TEXT("#define View DP_NoViewAtBeginView\n"), ESearchCase::CaseSensitive));
	}
	if (ToneSlot != INDEX_NONE)
	{
		const FString Section = FindRegistrySection(/*bCompute*/ false, ToneSlot);
		TestTrue(TEXT("a pixel pass's read has its three names"), Section.Contains(TEXT("#define In DP_Input0\n#define InSize DP_InputSize[0]\n#define InUVRect DP_InputUVRect[0]\n"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("its write only a size: the output is SV_Target0"), Section.Contains(TEXT("#define OutSize DP_OutputSize[0]\n"), ESearchCase::CaseSensitive) && !Section.Contains(TEXT("#define Out DP_Output0"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("its entry is the pixel slot's"), Section.Contains(TEXT("#define TonePS DreamPassMainPS\n"), ESearchCase::CaseSensitive));
	}

	// ---- a registry file that no longer says what Registry.json says is written again by the next commit
	FString ToneSection;
	if (ToneSlot != INDEX_NONE)
	{
		ToneSection = FindRegistrySection(/*bCompute*/ false, ToneSlot);
		const FString PixelFile = UE::DreamPass::GetRegistryFilePath(/*bCompute*/ false);
		const FString Tampered = ReadText(PixelFile).Replace(TEXT("\r\n"), TEXT("\n")).Replace(*ToneSection, TEXT(""), ESearchCase::CaseSensitive);
		FFileHelper::SaveStringToFile(Tampered, *PixelFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	// ---- v2: Seed is a clear now, and a new HLSL pass comes after Blur
	{
		if (!Fixture.WriteSource(*this, Parameters1 + SeedClear + BlurPass + SmoothPass + TonePass))
		{
			return false;
		}
		Compiler::FDreamShaderLang2PipelineResult Run;
		TestTrue(FString::Printf(TEXT("v2 builds (%s)"), *Describe(Run.Diagnostics)), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true));
		TestTrue(TEXT("the slot Seed left is freed, and said so (DSH8327)"), HasCode(Run.Diagnostics, TEXT("DSH8327"), Lang::ELangSeverity::Info));

		Compiler::FDreamPassRegistryReport After;
		Compiler::DescribeDreamPassRegistry(/*bClassify*/ false, After, ReportError);
		TestEqual(TEXT("Blur keeps its slot across the edit"), FindSlot(After, PipelinePath, TEXT("Blur"), true), BlurSlot);
		TestEqual(TEXT("Tone keeps its slot"), FindSlot(After, PipelinePath, TEXT("Tone"), false), ToneSlot);
		TestEqual(TEXT("Seed has none any more"), FindSlot(After, PipelinePath, TEXT("Seed"), true), int32(INDEX_NONE));
		TestNotEqual(TEXT("Smooth has one"), FindSlot(After, PipelinePath, TEXT("Smooth"), true), int32(INDEX_NONE));
		if (ToneSlot != INDEX_NONE)
		{
			TestTrue(TEXT("the pixel registry file holds Tone's section again"), FindRegistrySection(/*bCompute*/ false, ToneSlot).Equals(ToneSection, ESearchCase::CaseSensitive));
		}
	}

	// ---- the decompiler reads the slotted pipeline back without a word
	{
		UDreamPassPipeline* Pipeline = LoadObject<UDreamPassPipeline>(nullptr, *PipelinePath);
		FPipelineDecompileOptions Options;
		Options.TargetSourceFilePath = Fixture.GetSourceFilePath();
		FString Text;
		Lang::FLangDiagnosticSink Diagnostics;
		TestTrue(FString::Printf(TEXT("the slotted pipeline decompiles (%s)"), *Describe(Diagnostics)), Pipeline && DecompileDreamPassPipelineToText(Pipeline, Options, Text, Diagnostics));
		TestFalse(TEXT("and its text passes its own check"), HasCode(Diagnostics, TEXT("DSH9221")) || HasCode(Diagnostics, TEXT("DSH9222")) || HasCode(Diagnostics, TEXT("DSH9223")));
	}

	// ---- the source gone: its slots are garbage, and -Gc frees them
	{
		IFileManager::Get().Delete(*Fixture.GetSourceFilePath(), false, true);
		Compiler::FDreamShaderProductIndex::Get().Refresh();
		Compiler::FDreamPassRegistryReport Classified;
		Compiler::DescribeDreamPassRegistry(/*bClassify*/ true, Classified, ReportError);
		const bool bGone = Classified.Slots.ContainsByPredicate([&PipelinePath](const Compiler::FDreamPassSlotReport& Slot)
		{
			return Slot.Pipeline.Equals(PipelinePath, ESearchCase::IgnoreCase) && Slot.State == Compiler::EDreamPassSlotState::PipelineGone;
		});
		TestTrue(TEXT("a slot of a pipeline no .dsp builds is PipelineGone"), bGone);

		TArray<Compiler::FDreamPassSlotReport> Freed;
		Lang::FLangDiagnosticSink Diagnostics;
		TestTrue(FString::Printf(TEXT("the garbage is collected (%s)"), *Describe(Diagnostics)), Compiler::CollectDreamPassRegistryGarbage(Freed, Diagnostics));
		TestTrue(TEXT("our slots among what was freed"), Freed.ContainsByPredicate([&PipelinePath](const Compiler::FDreamPassSlotReport& Slot) { return Slot.Pipeline.Equals(PipelinePath, ESearchCase::IgnoreCase); }));
		Compiler::FDreamPassRegistryReport AfterGc;
		Compiler::DescribeDreamPassRegistry(/*bClassify*/ false, AfterGc, ReportError);
		TestEqual(TEXT("and none of them is left"), CountSlotsOf(AfterGc, PipelinePath), 0);
	}

	// ---- an unreadable Registry.json: nothing is planned over it, and -Rebuild moves it aside
	{
		FString CheckPath;
		FString GoodShader;
		if (!Fixture.WriteSiblingSource(*this, TEXT("PL2Good.usf"), GoodComputeShader(), GoodShader)
			|| !Fixture.WriteSiblingSource(*this, TEXT("CP_PL2RegCheck.dsp"), MakeSingleComputePipeline(TEXT("PL2Good.usf"), TEXT("GoodCS")), CheckPath))
		{
			return false;
		}
		Compiler::FDreamShaderProductIndex::Get().Refresh();
		const FString JsonPath = FPaths::Combine(UE::DreamPass::GetUserShaderDirectory(), TEXT("Registry.json"));
		FFileHelper::SaveStringToFile(TEXT("{ \"compute\": [ <<<<<<< merge conflict\n"), *JsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

		Lang::FLangDiagnosticSink Diagnostics;
		int32 Checked = 0;
		TestFalse(TEXT("a pre-check against an unreadable registry fails"), Compiler::CheckDreamShaderPipelineSlots(CheckPath, TArray<FName>(), Diagnostics, Checked));
		TestTrue(FString::Printf(TEXT("DSH8315 (%s)"), *Describe(Diagnostics)), HasCode(Diagnostics, TEXT("DSH8315"), Lang::ELangSeverity::Error));

		FString MovedTo;
		Lang::FLangDiagnosticSink ResetDiagnostics;
		TestTrue(FString::Printf(TEXT("the unreadable registry is reset (%s)"), *Describe(ResetDiagnostics)), Compiler::ResetUnreadableDreamPassRegistry(MovedTo, ResetDiagnostics));
		TestTrue(FString::Printf(TEXT("moved aside to Registry.json.unreadable ('%s')"), *MovedTo), MovedTo.EndsWith(TEXT("Registry.json.unreadable"), ESearchCase::IgnoreCase) && IFileManager::Get().FileExists(*MovedTo));
		Compiler::FDreamPassRegistryReport Empty;
		TestTrue(TEXT("and an empty registry reads"), Compiler::DescribeDreamPassRegistry(/*bClassify*/ false, Empty, ReportError) && Empty.Slots.Num() == 0);
	}
	return true;
}

#else // DREAMSHADER_WITH_CUSTOM_PASS

// ---------------------------------------------------------------------------------------------
// An engine without the Custom Pass runtime: a `.dsp` binds, and builds nothing
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCompilerPipelineNeedsCustomPassTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Pipeline.NeedsCustomPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCompilerPipelineNeedsCustomPassTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::CompilerPipelineTests;
	using UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture;

	FDreamShaderCompile2Fixture Fixture(TEXT("Old/CP_PL2Old"), ScratchArea(), TEXT("dsp"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	if (!Fixture.WriteSource(*this, MakeClearPipeline(TEXT("buffer Glow : R8(Export = true);\n"), TEXT("Glow"))))
	{
		return false;
	}
	Compiler::FDreamShaderLang2PipelineResult Run;
	TestFalse(TEXT("a .dsp builds nothing without the Custom Pass runtime"), RunPipeline(Fixture.GetSourceFilePath(), Run, /*bEmitAssets*/ true, /*bForce*/ true));
	TestTrue(FString::Printf(TEXT("DSH8300 (%s)"), *Describe(Run.Diagnostics)), HasCode(Run.Diagnostics, TEXT("DSH8300"), Lang::ELangSeverity::Error));
	return true;
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS

#endif // WITH_DEV_AUTOMATION_TESTS
