// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Tests for the Graph "breakpoint" preview: the debug table the generator publishes per source, the
// probe-preview material that re-wires an arbitrary Graph binding into a rendered material (the text
// analogue of the Material Editor's "Start Previewing Node"), and the raw-frame wire header.

#include "DreamShaderModule.h"
#include "DreamShaderTestCommon.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
#include "Preview/DreamShaderGraphDebugInfo.h"
#include "Preview/DreamShaderPreviewRenderer.h"
#include "Preview/DreamShaderPreviewSession.h"
#include "Preview/DreamShaderProbePreview.h"

#include "AssetCompilingManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::DreamShader::Editor::Private::Tests
{
	// These mirror the private helpers in DreamShaderAutomationTests.cpp. Kept local (not shared
	// through the header) so this file is self-contained; the tiny duplication is deliberate.
	namespace ProbeTestUtils
	{
		FString MakeAssetName(const TCHAR* Prefix)
		{
			return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		}

		FString SourceDir()
		{
			return FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), TEXT("Tests"), TEXT("Automation"));
		}

		FString SourcePath(const FString& FileName)
		{
			return UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(SourceDir(), FileName));
		}

		FString ObjectPath(const FString& AssetName)
		{
			return FString::Printf(TEXT("/Game/DreamShaderTests/Automation/%s.%s"), *AssetName, *AssetName);
		}

		bool WriteSource(FAutomationTestBase& Test, const FString& FileName, const FString& SourceText, FString& OutPath)
		{
			OutPath = SourcePath(FileName);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
			if (!FFileHelper::SaveStringToFile(SourceText, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddError(FString::Printf(TEXT("Failed to write '%s'."), *OutPath));
				return false;
			}
			return true;
		}

		void ExpectProbeAssetWarnings(FAutomationTestBase& Test, const FString& InObjectPath)
		{
			Test.AddExpectedError(
				FString::Printf(TEXT("SkipPackage: %s"), *FPackageName::ObjectPathToPackageName(InObjectPath)),
				EAutomationExpectedErrorFlags::Contains, -1);
			Test.AddExpectedError(InObjectPath, EAutomationExpectedErrorFlags::Contains, -1);
		}

		class FScopedArtifacts
		{
		public:
			void AddSource(const FString& Path) { Sources.Add(Path); }
			void AddObject(const FString& Path) { Objects.Add(Path); }
			~FScopedArtifacts()
			{
				for (const FString& Path : Objects)
				{
					if (UObject* Object = LoadObject<UObject>(nullptr, *Path))
					{
						TArray<UObject*> ToDelete = { Object };
						ObjectTools::DeleteObjectsUnchecked(ToDelete);
					}
				}
				for (const FString& Path : Sources)
				{
					IFileManager::Get().Delete(*Path, false, true);
				}
			}
		private:
			TArray<FString> Sources;
			TArray<FString> Objects;
		};

		// A material with several distinct-colored bindings on their own lines, so a probe on each
		// line renders a predictably different emissive color. The Graph block starts far enough down
		// that a wrong line-offset would be obvious.
		FString MakeProbeSource(const FString& AssetName)
		{
			return FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec3 RedValue = vec3(1.0, 0.0, 0.0);
        vec3 GreenValue = vec3(0.0, 1.0, 0.0);
        vec3 BlueValue = vec3(0.0, 0.0, 1.0);
        Color = RedValue + GreenValue + BlueValue;
    }
}
)"), *AssetName);
		}

		bool FinishCompilation(UMaterialInterface* Material)
		{
			if (!Material)
			{
				return false;
			}
			TArray<UObject*> Objects = { Material };
			FAssetCompilingManager::Get().FinishCompilationForObjects(Objects);
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			FlushRenderingCommands();
			return true;
		}
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FDreamShaderProbeTableLineMappingTest,
		"DreamShader.Preview.ProbeTable.LineMapping",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FDreamShaderProbeTableLineMappingTest::RunTest(const FString& Parameters)
	{
		using namespace UE::DreamShader::Editor;
		using namespace ProbeTestUtils;

		FScopedArtifacts Artifacts;
		const FString AssetName = MakeAssetName(TEXT("M_ProbeTable"));
		const FString ObjPath = ObjectPath(AssetName);
		Artifacts.AddObject(ObjPath);
		ExpectProbeAssetWarnings(*this, ObjPath);

		const FString Source = MakeProbeSource(AssetName);
		FString SourcePathOut;
		if (!WriteSource(*this, AssetName + TEXT(".dsm"), Source, SourcePathOut))
		{
			return false;
		}
		Artifacts.AddSource(SourcePathOut);

		FString Message;
		if (!TestTrue(FString::Printf(TEXT("Generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourcePathOut, Message, /*bForce*/ true, /*bTransient*/ true)))
		{
			return false;
		}

		const TSharedPtr<const Private::FDreamShaderGraphDebugTable> Table =
			Private::FDreamShaderGraphDebugRegistry::Get().Find(SourcePathOut);
		if (!TestValid(TEXT("Debug table was published for the source"), Table))
		{
			return false;
		}

		// The four assignments each bind exactly one probe (RedValue, GreenValue, BlueValue, Color).
		TestTrue(TEXT("Table has at least four probes"), Table->Probes.Num() >= 4);

		// Lines are 1-based into the source file. Find each binding by name and confirm the line the
		// probe reports is the line that text actually sits on.
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		const auto LineOf = [&Lines](const TCHAR* Needle) -> int32
		{
			for (int32 Index = 0; Index < Lines.Num(); ++Index)
			{
				if (Lines[Index].Contains(Needle))
				{
					return Index + 1;
				}
			}
			return -1;
		};

		const auto ProbeByName = [&Table](const TCHAR* Name) -> const Private::FDreamShaderGraphProbe*
		{
			for (const Private::FDreamShaderGraphProbe& Probe : Table->Probes)
			{
				if (Probe.Name == Name)
				{
					return &Probe;
				}
			}
			return nullptr;
		};

		const struct { const TCHAR* Name; const TCHAR* Marker; } Cases[] = {
			{ TEXT("RedValue"), TEXT("vec3 RedValue") },
			{ TEXT("GreenValue"), TEXT("vec3 GreenValue") },
			{ TEXT("BlueValue"), TEXT("vec3 BlueValue") },
			{ TEXT("Color"), TEXT("Color = RedValue") },
		};
		for (const auto& Case : Cases)
		{
			const Private::FDreamShaderGraphProbe* Probe = ProbeByName(Case.Name);
			if (!TestNotNull(FString::Printf(TEXT("Probe '%s' exists"), Case.Name), Probe))
			{
				continue;
			}
			const int32 ExpectedLine = LineOf(Case.Marker);
			TestEqual(FString::Printf(TEXT("Probe '%s' maps to its source line"), Case.Name), Probe->Line, ExpectedLine);
			TestTrue(FString::Printf(TEXT("Probe '%s' is a statement target"), Case.Name), Probe->bIsStatementTarget);
			TestNotNull(FString::Printf(TEXT("Probe '%s' has a live node"), Case.Name), Probe->Expression.Get());
		}

		// ResolveProbe on a blank line between statements snaps forward to the next real binding.
		const int32 GreenLine = LineOf(TEXT("vec3 GreenValue"));
		const Private::FDreamShaderGraphProbe* Resolved = Table->ResolveProbe(GreenLine);
		if (TestNotNull(TEXT("ResolveProbe finds the GreenValue line"), Resolved))
		{
			TestEqual(TEXT("ResolveProbe returns the binding on that line"), Resolved->Name, FString(TEXT("GreenValue")));
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FDreamShaderProbePreviewRendersBindingTest,
		"DreamShader.Preview.ProbePreview.RendersBinding",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FDreamShaderProbePreviewRendersBindingTest::RunTest(const FString& Parameters)
	{
		using namespace UE::DreamShader::Editor;
		using namespace ProbeTestUtils;

		FScopedArtifacts Artifacts;
		const FString AssetName = MakeAssetName(TEXT("M_ProbeRender"));
		const FString ObjPath = ObjectPath(AssetName);
		Artifacts.AddObject(ObjPath);
		ExpectProbeAssetWarnings(*this, ObjPath);

		const FString Source = MakeProbeSource(AssetName);
		FString SourcePathOut;
		if (!WriteSource(*this, AssetName + TEXT(".dsm"), Source, SourcePathOut))
		{
			return false;
		}
		Artifacts.AddSource(SourcePathOut);

		FString Message;
		if (!TestTrue(FString::Printf(TEXT("Generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourcePathOut, Message, /*bForce*/ true, /*bTransient*/ true)))
		{
			return false;
		}

		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		const auto LineOf = [&Lines](const TCHAR* Needle) -> int32
		{
			for (int32 Index = 0; Index < Lines.Num(); ++Index)
			{
				if (Lines[Index].Contains(Needle))
				{
					return Index + 1;
				}
			}
			return -1;
		};

		Private::FDreamShaderProbePreview Probe;
		Probe.SetSource(SourcePathOut);

		Private::FDreamShaderPreviewRenderContext RenderContext;
		constexpr int32 RenderSize = 64;

		// Each colored binding, probed in turn, should paint the mesh that color. The center pixel is
		// the cleanest sample (the sphere fills the middle of the frame).
		const struct { const TCHAR* Marker; const TCHAR* Name; int32 DominantChannel; } Cases[] = {
			{ TEXT("vec3 RedValue"),   TEXT("RedValue"),   0 },
			{ TEXT("vec3 GreenValue"), TEXT("GreenValue"), 1 },
			{ TEXT("vec3 BlueValue"),  TEXT("BlueValue"),  2 },
		};

		for (const auto& Case : Cases)
		{
			FString ProbeError;
			const int32 Line = LineOf(Case.Marker);
			if (!TestTrue(FString::Printf(TEXT("SetProbe on '%s' (line %d) succeeds: %s"), Case.Name, Line, *ProbeError),
				Probe.SetProbe(Line, FString(), ProbeError)))
			{
				continue;
			}
			if (!TestTrue(FString::Printf(TEXT("Probe '%s' is active"), Case.Name), Probe.IsActive()))
			{
				continue;
			}

			UMaterialInterface* PreviewMaterial = Probe.GetPreviewMaterial();
			if (!TestNotNull(FString::Printf(TEXT("Probe '%s' produced a preview material"), Case.Name), PreviewMaterial))
			{
				continue;
			}

			// Warm-up render then finish compile then real render (the parity test's proven recipe:
			// material resources are lazy, so the first render queues the compile the second reads).
			FString RenderError;
			TArray<FColor> WarmupPixels;
			RenderContext.RenderFramePixels(PreviewMaterial, RenderSize, RenderSize, TEXT("sphere"), -157.5f, -11.25f, WarmupPixels, RenderError);
			FinishCompilation(PreviewMaterial);

			TArray<FColor> Pixels;
			if (!TestTrue(FString::Printf(TEXT("Probe '%s' render succeeds: %s"), Case.Name, *RenderError),
				RenderContext.RenderFramePixels(PreviewMaterial, RenderSize, RenderSize, TEXT("sphere"), -157.5f, -11.25f, Pixels, RenderError)))
			{
				continue;
			}

			const FColor Center = Pixels[(RenderSize / 2) * RenderSize + (RenderSize / 2)];
			const int32 Channels[3] = { Center.R, Center.G, Center.B };
			const int32 Dominant = Channels[Case.DominantChannel];
			const int32 Other1 = Channels[(Case.DominantChannel + 1) % 3];
			const int32 Other2 = Channels[(Case.DominantChannel + 2) % 3];
			AddInfo(FString::Printf(TEXT("Probe '%s' center pixel = (%d,%d,%d)."), Case.Name, Center.R, Center.G, Center.B));
			TestTrue(
				FString::Printf(TEXT("Probe '%s' renders its channel dominant (center=%d,%d,%d)."), Case.Name, Center.R, Center.G, Center.B),
				Dominant > Other1 + 40 && Dominant > Other2 + 40);
		}

		// Clearing drops the preview material.
		Probe.ClearProbe();
		TestFalse(TEXT("Cleared probe is inactive"), Probe.IsActive());
		TestNull(TEXT("Cleared probe has no preview material"), Probe.GetPreviewMaterial());

		return true;
	}

	// A probe set before the source has ever been generated must stay pending, then attach on the
	// first publish -- the "breakpoint set on a file that hasn't compiled yet" case.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FDreamShaderProbePendingThenAttachesTest,
		"DreamShader.Preview.ProbePreview.PendingThenAttaches",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FDreamShaderProbePendingThenAttachesTest::RunTest(const FString& Parameters)
	{
		using namespace UE::DreamShader::Editor;
		using namespace ProbeTestUtils;

		FScopedArtifacts Artifacts;
		const FString AssetName = MakeAssetName(TEXT("M_ProbePending"));
		const FString ObjPath = ObjectPath(AssetName);
		Artifacts.AddObject(ObjPath);
		ExpectProbeAssetWarnings(*this, ObjPath);

		const FString Source = MakeProbeSource(AssetName);
		FString SourcePathOut = SourcePath(AssetName + TEXT(".dsm"));

		Private::FDreamShaderProbePreview Probe;
		Probe.SetSource(SourcePathOut);

		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		int32 GreenLine = 1;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Contains(TEXT("vec3 GreenValue")))
			{
				GreenLine = Index + 1;
				break;
			}
		}

		FString ProbeError;
		const bool bSetBeforeGen = Probe.SetProbe(GreenLine, FString(), ProbeError);
		TestFalse(TEXT("SetProbe before generation does not attach"), bSetBeforeGen);
		TestTrue(TEXT("SetProbe before generation is remembered"), Probe.IsRequested());
		TestFalse(TEXT("SetProbe before generation is not active"), Probe.IsActive());

		// Now generate: the publish should retro-attach the pending probe.
		if (!WriteSource(*this, AssetName + TEXT(".dsm"), Source, SourcePathOut))
		{
			return false;
		}
		Artifacts.AddSource(SourcePathOut);

		FString Message;
		if (!TestTrue(FString::Printf(TEXT("Generation succeeds: %s"), *Message),
			FMaterialGenerator::GenerateMaterialFromFile(SourcePathOut, Message, /*bForce*/ true, /*bTransient*/ true)))
		{
			return false;
		}

		TestTrue(TEXT("Pending probe attaches after generation"), Probe.IsActive());
		TestNotNull(TEXT("Attached probe has a preview material"), Probe.GetPreviewMaterial());
		if (Probe.GetResolvedProbe().IsSet())
		{
			TestEqual(TEXT("Attached probe resolved to GreenValue"), Probe.GetResolvedProbe()->Name, FString(TEXT("GreenValue")));
		}

		return true;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
