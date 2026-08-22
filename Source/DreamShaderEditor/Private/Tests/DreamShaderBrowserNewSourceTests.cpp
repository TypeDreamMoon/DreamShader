// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The browser's New menu: the three templates under Resources/Templates must render with every
// placeholder filled and must compile as written, or the first thing a new user makes is broken.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/DreamShaderBrowserNewSource.h"

#include "DreamShaderModule.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	FString MakeUniqueTestAssetName(const TCHAR* Prefix);
	FString GetAutomationSourceDirectory();
	void AddExpectedNewAssetProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath);
	void AddExpectedAutomationCleanupWarnings(FAutomationTestBase& Test);
	void DeleteSourceFileForAutomation(const FString& SourceFilePath);
	void DeleteAssetForAutomation(const FString& ObjectPath);

	namespace
	{
		struct FScopedNewSourceArtifacts
		{
			TArray<FString> SourceFiles;
			TArray<FString> ObjectPaths;
			~FScopedNewSourceArtifacts()
			{
				for (const FString& ObjectPath : ObjectPaths) { DeleteAssetForAutomation(ObjectPath); }
				for (const FString& SourceFile : SourceFiles) { DeleteSourceFileForAutomation(SourceFile); }
			}
		};

		FString AutomationObjectPathFor(const FString& Stem)
		{
			return FString::Printf(TEXT("/Game/Tests/Automation/%s.%s"), *Stem, *Stem);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderNewSourceTemplatesTest,
	"DreamShader.Browser.NewSource.TemplatesRenderAndCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderNewSourceTemplatesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedNewSourceArtifacts Artifacts;
	AddExpectedAutomationCleanupWarnings(*this);
	const FString Directory = UE::DreamShader::NormalizeSourceFilePath(GetAutomationSourceDirectory());
	IFileManager::Get().MakeDirectory(*Directory, true);

	// Every template renders with no placeholder left behind.
	for (const EBrowserSourceKind Kind : { EBrowserSourceKind::Material, EBrowserSourceKind::Function, EBrowserSourceKind::Header })
	{
		FNewSourceRequest Request;
		Request.Kind = Kind;
		Request.Directory = Directory;
		Request.FileStem = TEXT("Probe");
		FString Text;
		FString Error;
		if (TestTrue(FString::Printf(TEXT("Template for .%s renders: %s"), GetSourceKindExtension(Kind), *Error), RenderNewSourceTemplate(Request, Text, Error)))
		{
			TestFalse(FString::Printf(TEXT("No placeholder left in the .%s template"), GetSourceKindExtension(Kind)), Text.Contains(TEXT("{NAME}")) || Text.Contains(TEXT("{FILENAME}")) || Text.Contains(TEXT("{ASSETPATH}")));
			if (Kind != EBrowserSourceKind::Header)
			{
				TestTrue(TEXT("The block's Name= is the directory relative to the root plus the stem"), Text.Contains(TEXT("Name=\"Tests/Automation/Probe\"")));
			}
		}
	}

	// The material template compiles.
	const FString MaterialStem = MakeUniqueTestAssetName(TEXT("M_AutoNewSource"));
	{
		FNewSourceRequest Request;
		Request.Kind = EBrowserSourceKind::Material;
		Request.Directory = Directory;
		Request.FileStem = MaterialStem;
		FString FilePath;
		FString Error;
		if (TestTrue(FString::Printf(TEXT("The material file is created: %s"), *Error), CreateNewSourceFile(Request, FilePath, Error)))
		{
			Artifacts.SourceFiles.Add(FilePath);
			const FString ObjectPath = AutomationObjectPathFor(MaterialStem);
			Artifacts.ObjectPaths.Add(ObjectPath);
			AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
			FString Message;
			TestTrue(FString::Printf(TEXT("The material template compiles: %s"), *Message),
				FMaterialGenerator::GenerateAssetsFromFile(FilePath, Message, /*bForce*/ true, /*bTransient*/ true));

			FString Unused;
			TestFalse(TEXT("Creating the same file again is refused"), CreateNewSourceFile(Request, Unused, Error));
			TestTrue(TEXT("The refusal names the file"), Error.Contains(FPaths::GetCleanFilename(FilePath)));
		}
	}

	// The function template compiles.
	const FString FunctionStem = MakeUniqueTestAssetName(TEXT("F_AutoNewSource"));
	{
		FNewSourceRequest Request;
		Request.Kind = EBrowserSourceKind::Function;
		Request.Directory = Directory;
		Request.FileStem = FunctionStem;
		FString FilePath;
		FString Error;
		if (TestTrue(FString::Printf(TEXT("The function file is created: %s"), *Error), CreateNewSourceFile(Request, FilePath, Error)))
		{
			Artifacts.SourceFiles.Add(FilePath);
			const FString ObjectPath = AutomationObjectPathFor(FunctionStem);
			Artifacts.ObjectPaths.Add(ObjectPath);
			AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
			FString Message;
			TestTrue(FString::Printf(TEXT("The function template compiles: %s"), *Message),
				FMaterialGenerator::GenerateAssetsFromFile(FilePath, Message, /*bForce*/ true, /*bTransient*/ true));
		}
	}

	// The header template compiles when a material imports it and calls its function.
	const FString HeaderStem = MakeUniqueTestAssetName(TEXT("H_AutoNewSource"));
	{
		FNewSourceRequest Request;
		Request.Kind = EBrowserSourceKind::Header;
		Request.Directory = Directory;
		Request.FileStem = HeaderStem;
		FString HeaderPath;
		FString Error;
		if (TestTrue(FString::Printf(TEXT("The header file is created: %s"), *Error), CreateNewSourceFile(Request, HeaderPath, Error)))
		{
			Artifacts.SourceFiles.Add(HeaderPath);

			const FString ImporterStem = MakeUniqueTestAssetName(TEXT("M_AutoNewSourceImporter"));
			const FString ImporterPath = Directory / (ImporterStem + TEXT(".dsm"));
			const FString ImporterText = FString::Printf(TEXT(R"(import "%s.dsh";
Shader(Name="Tests/Automation/%s")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.5, 0.25);
    }
    Settings = {
        Backend = "Graph";
        Domain = "UI";
        ShadingModel = "Unlit";
    }
    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }
    Graph = {
        Color = vec3(Luma(Tint));
    }
}
)"), *HeaderStem, *ImporterStem);
			if (TestTrue(TEXT("The importing material is written"), FFileHelper::SaveStringToFile(ImporterText, *ImporterPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))
			{
				Artifacts.SourceFiles.Add(ImporterPath);
				const FString ObjectPath = AutomationObjectPathFor(ImporterStem);
				Artifacts.ObjectPaths.Add(ObjectPath);
				AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
				FString Message;
				TestTrue(FString::Printf(TEXT("A material importing the header template compiles: %s"), *Message),
					FMaterialGenerator::GenerateAssetsFromFile(ImporterPath, Message, /*bForce*/ true, /*bTransient*/ true));
			}
		}
	}

	// Refusals.
	{
		FNewSourceRequest Request;
		Request.Kind = EBrowserSourceKind::Material;
		Request.Directory = Directory;
		Request.FileStem = TEXT("9lives");
		FString FilePath;
		FString Error;
		TestFalse(TEXT("A stem starting with a digit is refused"), CreateNewSourceFile(Request, FilePath, Error));
		Request.FileStem = TEXT("has space");
		TestFalse(TEXT("A stem with a space is refused"), CreateNewSourceFile(Request, FilePath, Error));
		Request.FileStem = TEXT("Fine");
		Request.Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		TestFalse(TEXT("A folder outside every writable root is refused"), CreateNewSourceFile(Request, FilePath, Error));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
