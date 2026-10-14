// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.NoGeneration.* -- sources that only a compile naming them builds (Docs/generation/no-generation.md):
//
//   Syntax          `#pragma nogeneration`, `/// @nogeneration`, the 1.x `NoGeneration = true`, and their four mistakes.
//   Compile         an unnamed compile leaves a NoGeneration export alone, a same-file caller uses the asset on disk, and
//                   one that was never built is DSH8278 for itself and DSH8279 for its caller.
//   InstanceParent  a `.dsi` is built against a NoGeneration parent's asset, and is DSH8279 while it has none.
//
// DreamShader.DisableDiagnostics.* -- `disable_warning` / `disable_error` (Docs/language-v2/disable-diagnostics.md):
//
//   Scan            the four spellings read off a text, and the words that are not codes.
//   Compile         a disabled warning is not reported, a disabled error does not fail, the lists reach a 1.x file.
//
// Every test writes its sources into a package of its own under the project root's Packages tree (whose Assets folder
// every build path takes) and deletes it, and every asset under its asset root, on the way out.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "Lang/LangDiagnosticSuppression.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	// Fixture helpers shared with DreamShaderAutomationTests.cpp (defined there, external linkage).
	void AddExpectedNewAssetProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath);
	void AddExpectedAutomationCleanupWarnings(FAutomationTestBase& Test);
}

namespace UE::DreamShader::Editor::Private::NoGenerationTests
{
	/** `Packages/@dstest/<unique>` with its own asset root; both deleted on destruction. */
	class FScopedNoGenerationPackage
	{
	public:
		FScopedNoGenerationPackage()
		{
			Name = FString(TEXT("ng")) + FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
			Directory = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(UE::DreamShader::GetPackageShaderDirectory(), TEXT("@dstest"), Name));
			AssetRoot = FString::Printf(TEXT("/Game/DreamShaderPackages/dstest/%s"), *Name);
			IFileManager::Get().MakeDirectory(*Directory, true);
		}

		~FScopedNoGenerationPackage()
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			TArray<FString> ScanPaths;
			ScanPaths.Add(AssetRoot);
			AssetRegistryModule.Get().ScanPathsSynchronous(ScanPaths, /*bForceRescan*/ true);

			FARFilter Filter;
			Filter.PackagePaths.Add(FName(*AssetRoot));
			Filter.bRecursivePaths = true;
			TArray<FAssetData> Remaining;
			AssetRegistryModule.Get().GetAssets(Filter, Remaining);

			TArray<UObject*> ObjectsToDelete;
			for (const FAssetData& AssetData : Remaining)
			{
				if (UObject* Asset = AssetData.GetAsset())
				{
					ObjectsToDelete.Add(Asset);
				}
			}
			if (ObjectsToDelete.Num() > 0)
			{
				ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
			}

			IFileManager::Get().DeleteDirectory(*Directory, false, true);
			FString AssetDirectory;
			if (FPackageName::TryConvertLongPackageNameToFilename(AssetRoot + TEXT("/"), AssetDirectory))
			{
				IFileManager::Get().DeleteDirectory(*AssetDirectory, false, true);
			}
		}

		FString Write(FAutomationTestBase& Test, const FString& RelativePath, const FString& Text) const
		{
			const FString Path = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(Directory, RelativePath));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			if (!FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddError(FString::Printf(TEXT("Could not write the test source '%s'."), *Path));
			}
			return Path;
		}

		/** `<AssetRoot>/<Leaf>.<Leaf>`: an asset built from a file directly under Assets. */
		FString ObjectPath(const FString& Leaf) const
		{
			return FString::Printf(TEXT("%s/%s.%s"), *AssetRoot, *Leaf, *Leaf);
		}

		FString Name;
		FString Directory;
		FString AssetRoot;
	};

	UE::DreamShader::FDreamShaderCompileResult Compile(const FString& SourceFile, const bool bNamed, const bool bForce = false)
	{
		UE::DreamShader::FDreamShaderCompileRequest Request;
		Request.SourceFilePath = SourceFile;
		Request.bForce = bForce;
		Request.ThinCustomPersistence = UE::DreamShader::EThinCustomPersistence::Materialized;
		Request.bIncludeNoGeneration = bNamed;
		UE::DreamShader::IDreamShaderCompiler* const Compiler = UE::DreamShader::GetDreamShaderCompiler();
		return Compiler ? Compiler->CompileAssets(Request) : UE::DreamShader::FDreamShaderCompileResult();
	}

	/** The run up to validation: the products and every diagnostic, nothing written. */
	bool Check(const FString& SourceFile, UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineResult& OutResult)
	{
		UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineOptions Options;
		Options.bEmitAssets = false;
		return UE::DreamShader::Editor::Compiler::RunDreamShaderLang2Pipeline(SourceFile, Options, OutResult);
	}

	TArray<FString> GatherCodes(const UE::DreamShader::Lang::FLangDiagnosticSink& Sink)
	{
		TArray<FString> Codes;
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			Codes.Add(Diagnostic.Code);
		}
		return Codes;
	}

	FString Describe(const UE::DreamShader::Lang::FLangDiagnosticSink& Sink)
	{
		TArray<FString> Lines;
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			Lines.Add(Diagnostic.Code + TEXT(": ") + Diagnostic.Message.ToString());
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	/** bNoGeneration of the product named Name, or -1 when there is none. */
	int32 NoGenerationOf(const UE::DreamShader::Editor::Compiler::FDreamShaderLang2PipelineResult& Result, const TCHAR* Name)
	{
		if (!Result.IR.IsValid())
		{
			return -1;
		}
		for (const UE::DreamShader::IR::FIRProduct& Product : Result.IR->Products)
		{
			if (Product.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Product.bNoGeneration ? 1 : 0;
			}
		}
		return -1;
	}

	UObject* FindOrLoad(const FString& ObjectPath)
	{
		if (UObject* Found = FindObject<UObject>(nullptr, *ObjectPath))
		{
			return Found;
		}
		return FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ObjectPath)) ? LoadObject<UObject>(nullptr, *ObjectPath) : nullptr;
	}
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderNoGenerationSyntaxTest,
	"DreamShader.NoGeneration.Syntax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderNoGenerationSyntaxTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Compiler;
	using namespace UE::DreamShader::Editor::Private::NoGenerationTests;

	FScopedNoGenerationPackage Package;

	{
		const FString File = Package.Write(*this, TEXT("Assets/MF_Whole.dss"), TEXT(
			"#pragma nogeneration\n"
			"export float MF_Whole_A(float x) { return x; }\n"
			"export float MF_Whole_B(float x) { return x * 2.0; }\n"));
		FDreamShaderLang2PipelineResult Result;
		TestTrue(FString::Printf(TEXT("the file checks (%s)"), *Describe(Result.Diagnostics)), Check(File, Result));
		TestEqual(TEXT("the pragma marks the first export"), NoGenerationOf(Result, TEXT("MF_Whole_A")), 1);
		TestEqual(TEXT("and the second"), NoGenerationOf(Result, TEXT("MF_Whole_B")), 1);
	}

	{
		const FString File = Package.Write(*this, TEXT("Assets/MF_One.dss"), TEXT(
			"/// @nogeneration\n"
			"export float MF_One_A(float x) { return x; }\n"
			"export float MF_One_B(float x) { return x * 2.0; }\n"
			"/// @nogeneration\n"
			"float Helper(float x) { return x; }\n"));
		FDreamShaderLang2PipelineResult Result;
		Check(File, Result);
		TestEqual(TEXT("@nogeneration marks its export"), NoGenerationOf(Result, TEXT("MF_One_A")), 1);
		TestEqual(TEXT("and only that one"), NoGenerationOf(Result, TEXT("MF_One_B")), 0);
		TestTrue(FString::Printf(TEXT("on a helper it is DSH7224 (%s)"), *Describe(Result.Diagnostics)), GatherCodes(Result.Diagnostics).Contains(TEXT("DSH7224")));
	}

	{
		const FString File = Package.Write(*this, TEXT("Assets/MF_Legacy.dsf"), TEXT(
			"ShaderFunction(Name = \"MF_Legacy\", NoGeneration = true)\n"
			"{\n"
			"    Inputs = { float x; }\n"
			"    Outputs = { float y; }\n"
			"    Graph = { y = x; }\n"
			"}\n"
			"ShaderFunction(Name = \"MF_Legacy_Typo\", NoGeneration = maybe)\n"
			"{\n"
			"    Inputs = { float x; }\n"
			"    Outputs = { float y; }\n"
			"    Graph = { y = x; }\n"
			"}\n"));
		FDreamShaderLang2PipelineResult Result;
		TestTrue(FString::Printf(TEXT("the 1.x file checks (%s)"), *Describe(Result.Diagnostics)), Check(File, Result));
		TestEqual(TEXT("NoGeneration = true marks the 1.x block"), NoGenerationOf(Result, TEXT("MF_Legacy")), 1);
		TestEqual(TEXT("a value that is no boolean does not"), NoGenerationOf(Result, TEXT("MF_Legacy_Typo")), 0);
		TestTrue(TEXT("and is DSH2260"), GatherCodes(Result.Diagnostics).Contains(TEXT("DSH2260")));
	}

	{
		const FString File = Package.Write(*this, TEXT("Assets/MF_Args.dss"), TEXT(
			"#pragma nogeneration(false)\n"
			"export float MF_Args(float x) { return x; }\n"));
		FDreamShaderLang2PipelineResult Result;
		Check(File, Result);
		TestTrue(TEXT("arguments are DSH3219"), GatherCodes(Result.Diagnostics).Contains(TEXT("DSH3219")));
		TestEqual(TEXT("and the file is still marked"), NoGenerationOf(Result, TEXT("MF_Args")), 1);
	}

	{
		Package.Write(*this, TEXT("Assets/Marked.dsh"), TEXT("#pragma nogeneration\nfloat Twice(float x) { return x * 2.0; }\n"));
		const FString File = Package.Write(*this, TEXT("Assets/MF_Includer.dss"), TEXT(
			"#include \"Marked.dsh\"\n"
			"export float MF_Includer(float x) { return Twice(x); }\n"));
		FDreamShaderLang2PipelineResult Result;
		Check(File, Result);
		TestTrue(FString::Printf(TEXT("the pragma in a header is DSH7235 (%s)"), *Describe(Result.Diagnostics)), GatherCodes(Result.Diagnostics).Contains(TEXT("DSH7235")));
		TestEqual(TEXT("and does not mark the includer"), NoGenerationOf(Result, TEXT("MF_Includer")), 0);
	}

	{
		using namespace UE::DreamShader::Lang;
		const FLangSourceText Source(TEXT("C:/M.dss"), FString(TEXT("#pragma nogeneration\n#pragma disable_warning(DSH7224, DSH8237)\nexport float F(float x) { return x; }\n")));
		const FLangParseResult Parse = ParseDreamShaderLang(Source);
		if (TestTrue(TEXT("the pragmas parse"), Parse.Succeeded()))
		{
			const FString Printed = PrintDreamShaderLang(*Parse.Module);
			TestTrue(FString::Printf(TEXT("nogeneration prints back:\n%s"), *Printed), Printed.Contains(TEXT("#pragma nogeneration\n")));
			TestTrue(FString::Printf(TEXT("a disable list prints back as written:\n%s"), *Printed), Printed.Contains(TEXT("#pragma disable_warning(DSH7224, DSH8237)")));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderNoGenerationCompileTest,
	"DreamShader.NoGeneration.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderNoGenerationCompileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Compiler;
	using namespace UE::DreamShader::Editor::Private::NoGenerationTests;

	FScopedNoGenerationPackage Package;
	const FString File = Package.Write(*this, TEXT("Assets/MF_NG.dss"), TEXT(
		"/// @nogeneration\n"
		"export float MF_NG_A(float x) { return x * 2.0; }\n"
		"export float MF_NG_B(float x) { return MF_NG_A(x) + 1.0; }\n"));
	const FString PathA = Package.ObjectPath(TEXT("MF_NG_A"));
	const FString PathB = Package.ObjectPath(TEXT("MF_NG_B"));
	UE::DreamShader::Editor::Private::Tests::AddExpectedNewAssetProbeWarnings(*this, PathA);
	UE::DreamShader::Editor::Private::Tests::AddExpectedNewAssetProbeWarnings(*this, PathB);
	UE::DreamShader::Editor::Private::Tests::AddExpectedAutomationCleanupWarnings(*this);

	{
		const UE::DreamShader::FDreamShaderCompileResult Result = Compile(File, /*bNamed*/ false);
		const FString Message = Result.Message.ToString();
		TestFalse(FString::Printf(TEXT("an unnamed compile of a caller of an export that was never built fails (%s)"), *Message), Result.bSucceeded);
		TestEqual(TEXT("with DSH8279"), Result.Code, FString(TEXT("DSH8279")));
		TestNull(TEXT("and builds nothing for the NoGeneration export"), FindOrLoad(PathA));
	}

	{
		const UE::DreamShader::FDreamShaderCompileResult Result = Compile(File, /*bNamed*/ true);
		TestTrue(FString::Printf(TEXT("a named compile builds both (%s)"), *Result.Message.ToString()), Result.bSucceeded);
		TestNotNull(TEXT("the NoGeneration export"), FindOrLoad(PathA));
		TestNotNull(TEXT("its caller"), FindOrLoad(PathB));
	}

	// The source changes: an unnamed compile rebuilds the caller and leaves the NoGeneration export as it was.
	Package.Write(*this, TEXT("Assets/MF_NG.dss"), TEXT(
		"/// @nogeneration\n"
		"export float MF_NG_A(float x) { return x * 3.0; }\n"
		"export float MF_NG_B(float x) { return MF_NG_A(x) + 2.0; }\n"));
	{
		const UE::DreamShader::FDreamShaderCompileResult Result = Compile(File, /*bNamed*/ false);
		const FString Message = Result.Message.ToString();
		TestTrue(FString::Printf(TEXT("the unnamed compile succeeds (%s)"), *Message), Result.bSucceeded);
		TestTrue(TEXT("and says the export was skipped for NoGeneration"), Message.Contains(TEXT("marks it NoGeneration")));

		FDreamShaderProductResolution Resolution;
		ResolveDreamShaderSourceProducts(File, Resolution);
		TestFalse(TEXT("the export still carries the old build key"),
			UE::DreamShader::Editor::Private::IsGeneratedAssetSourceCurrent(FindOrLoad(PathA), Resolution.SourceFilePath, Resolution.SourceHash));
		TestTrue(TEXT("its caller carries the new one"),
			UE::DreamShader::Editor::Private::IsGeneratedAssetSourceCurrent(FindOrLoad(PathB), Resolution.SourceFilePath, Resolution.SourceHash));
	}

	{
		const UE::DreamShader::FDreamShaderCompileResult Result = Compile(File, /*bNamed*/ true);
		TestTrue(FString::Printf(TEXT("a named compile rebuilds the export (%s)"), *Result.Message.ToString()), Result.bSucceeded);
		FDreamShaderProductResolution Resolution;
		ResolveDreamShaderSourceProducts(File, Resolution);
		TestTrue(TEXT("which then carries the new build key"),
			UE::DreamShader::Editor::Private::IsGeneratedAssetSourceCurrent(FindOrLoad(PathA), Resolution.SourceFilePath, Resolution.SourceHash));
	}

	{
		const FString Missing = Package.Write(*this, TEXT("Assets/MF_NG_Alone.dss"), TEXT("#pragma nogeneration\nexport float MF_NG_Alone(float x) { return x; }\n"));
		const UE::DreamShader::FDreamShaderCompileResult Result = Compile(Missing, /*bNamed*/ false);
		const FString Message = Result.Message.ToString();
		TestTrue(FString::Printf(TEXT("a NoGeneration file alone compiles (%s)"), *Message), Result.bSucceeded);
		TestTrue(TEXT("and warns that its asset was never built"), Message.Contains(TEXT("DSH8278")));
		TestTrue(TEXT("and reports the skip"), Message.Contains(TEXT("it has never been built")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderNoGenerationInstanceParentTest,
	"DreamShader.NoGeneration.InstanceParent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderNoGenerationInstanceParentTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::NoGenerationTests;

	FScopedNoGenerationPackage Package;
	const FString Parent = Package.Write(*this, TEXT("Assets/M_NGP.dss"), TEXT(
		"#pragma nogeneration\n"
		"uniform float Rough = 0.5;\n"
		"export void M_NGP(inout material m) { m.Roughness = Rough; }\n"));
	const FString Instance = Package.Write(*this, TEXT("Assets/MI_NGP.dsi"), FString::Printf(TEXT(
		"#pragma instance(Parent = \"%s/M_NGP\")\n"
		"uniform float Rough = 0.25;\n"), *Package.AssetRoot));
	UE::DreamShader::Editor::Private::Tests::AddExpectedNewAssetProbeWarnings(*this, Package.ObjectPath(TEXT("M_NGP")));
	UE::DreamShader::Editor::Private::Tests::AddExpectedNewAssetProbeWarnings(*this, Package.ObjectPath(TEXT("MI_NGP")));
	UE::DreamShader::Editor::Private::Tests::AddExpectedAutomationCleanupWarnings(*this);

	{
		const UE::DreamShader::FDreamShaderCompileResult Result = Compile(Instance, /*bNamed*/ true);
		TestFalse(FString::Printf(TEXT("an instance of a NoGeneration parent that was never built fails (%s)"), *Result.Message.ToString()), Result.bSucceeded);
		TestEqual(TEXT("with DSH8279"), Result.Code, FString(TEXT("DSH8279")));
		TestNull(TEXT("and the parent was not built for it"), FindOrLoad(Package.ObjectPath(TEXT("M_NGP"))));
	}

	TestTrue(TEXT("the parent builds when it is named"), Compile(Parent, /*bNamed*/ true).bSucceeded);
	{
		const UE::DreamShader::FDreamShaderCompileResult Result = Compile(Instance, /*bNamed*/ false);
		TestTrue(FString::Printf(TEXT("then the instance builds against it (%s)"), *Result.Message.ToString()), Result.bSucceeded);
		TestNotNull(TEXT("into its asset"), Cast<UMaterialInstanceConstant>(FindOrLoad(Package.ObjectPath(TEXT("MI_NGP")))));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDisableDiagnosticsScanTest,
	"DreamShader.DisableDiagnostics.Scan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDisableDiagnosticsScanTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;

	FLangDisabledDiagnostics Lists;
	ScanLangDisabledDiagnostics(FString(TEXT(
		"#pragma disable_warning(DSH7224, dsh8237)\n"     // 1
		"#pragma disable_error(DSH4210) // why\n"         // 2
		"float a = 1; // disable_warning DSH5251\n"       // 3
		"// disable_error DSH4211, DSH4212\n"             // 4
		"\n"                                              // 5
		"// an ordinary comment\n"                        // 6
		"float b = 2;\n"                                  // 7
		"// disable_warning DSH7224\n"                    // 8
		"/// @library X\n"                                // 9
		"string s = \"// disable_warning DSH1111\";\n"    // 10
		"// disable_warning DSH72 nope\n"                 // 11
		"float c = 3;\n")),                               // 12
		Lists);

	TestTrue(TEXT("a file-wide warning"), Lists.FileWarnings.Contains(TEXT("DSH7224")));
	TestTrue(TEXT("codes are upper-cased"), Lists.FileWarnings.Contains(TEXT("DSH8237")));
	TestTrue(TEXT("a file-wide error, a trailing comment after the pragma notwithstanding"), Lists.FileErrors.Contains(TEXT("DSH4210")));
	TestTrue(TEXT("a trailing comment covers its own line"), Lists.LineWarnings.Contains(3) && Lists.LineWarnings[3].Contains(TEXT("DSH5251")));
	TestTrue(TEXT("a comment alone covers the next line that holds code"), Lists.LineErrors.Contains(7) && Lists.LineErrors[7].Contains(TEXT("DSH4212")));
	TestTrue(TEXT("a doc line holds code"), Lists.LineWarnings.Contains(9) && Lists.LineWarnings[9].Contains(TEXT("DSH7224")));
	TestFalse(TEXT("a '//' inside a string is no comment"), Lists.LineWarnings.Contains(10));
	TestEqual(TEXT("two words that are not codes"), Lists.Malformed.Num(), 2);
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDisableDiagnosticsCompileTest,
	"DreamShader.DisableDiagnostics.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderDisableDiagnosticsCompileTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Compiler;
	using namespace UE::DreamShader::Editor::Private::NoGenerationTests;

	FScopedNoGenerationPackage Package;
	const TCHAR* const Body = TEXT(
		"/// @library Misplaced\n"
		"float Helper(float x) { return x; }\n"
		"export float MF_DD(float x) { return Helper(x); }\n");

	auto CodesOf = [this, &Package](const TCHAR* Leaf, const FString& Text, bool& bOutChecked, FDreamShaderLang2PipelineResult& Result)
	{
		const FString File = Package.Write(*this, FString::Printf(TEXT("Assets/%s.dss"), Leaf), Text);
		bOutChecked = Check(File, Result);
		return GatherCodes(Result.Diagnostics);
	};

	{
		bool bChecked = false;
		FDreamShaderLang2PipelineResult Result;
		TestTrue(TEXT("the misplaced @library is DSH7224"), CodesOf(TEXT("MF_DD"), Body, bChecked, Result).Contains(TEXT("DSH7224")));
	}
	{
		bool bChecked = false;
		FDreamShaderLang2PipelineResult Result;
		const TArray<FString> Codes = CodesOf(TEXT("MF_DD"), FString(TEXT("#pragma disable_warning(DSH7224)\n")) + Body, bChecked, Result);
		TestFalse(TEXT("the pragma disables it"), Codes.Contains(TEXT("DSH7224")));
		TestTrue(TEXT("and keeps it aside"), Result.Diagnostics.GetSuppressedDiagnostics().ContainsByPredicate([](const UE::DreamShader::Lang::FLangDiagnostic& D) { return D.Code == TEXT("DSH7224"); }));
	}
	{
		bool bChecked = false;
		FDreamShaderLang2PipelineResult Result;
		TestFalse(TEXT("a comment on the line above disables it"),
			CodesOf(TEXT("MF_DD"), FString(TEXT("// disable_warning DSH7224\n")) + Body, bChecked, Result).Contains(TEXT("DSH7224")));
	}
	{
		bool bChecked = false;
		FDreamShaderLang2PipelineResult Result;
		TestTrue(TEXT("a comment on another line does not"),
			CodesOf(TEXT("MF_DD"), FString(Body) + TEXT("float Other = 1.0; // disable_warning DSH7224\n"), bChecked, Result).Contains(TEXT("DSH7224")));
	}

	{
		const TCHAR* const ErrorBody = TEXT(
			"/// @sort many\n"
			"uniform float Rough = 0.5;\n"
			"export float MF_DE(float x) { return x * Rough; }\n");
		bool bChecked = false;
		FDreamShaderLang2PipelineResult Result;
		const TArray<FString> Codes = CodesOf(TEXT("MF_DE"), ErrorBody, bChecked, Result);
		TestTrue(TEXT("a malformed @sort is DSH7221"), Codes.Contains(TEXT("DSH7221")));
		TestFalse(TEXT("and fails the check"), bChecked);

		FDreamShaderLang2PipelineResult Disabled;
		const TArray<FString> DisabledCodes = CodesOf(TEXT("MF_DE"), FString(TEXT("#pragma disable_error(DSH7221)\n")) + ErrorBody, bChecked, Disabled);
		TestFalse(TEXT("disable_error hides it"), DisabledCodes.Contains(TEXT("DSH7221")));
		TestTrue(FString::Printf(TEXT("and the check passes (%s)"), *Describe(Disabled.Diagnostics)), bChecked);
	}

	{
		bool bChecked = false;
		FDreamShaderLang2PipelineResult Result;
		const TArray<FString> Codes = CodesOf(TEXT("MF_DM"), FString(TEXT("#pragma disable_warning(DSH72, Foo)\n")) + Body, bChecked, Result);
		TestEqual(TEXT("each word that is not a code is DSH7236"), Codes.FilterByPredicate([](const FString& Code) { return Code == TEXT("DSH7236"); }).Num(), 2);
	}

	{
		const FString File = Package.Write(*this, TEXT("Assets/MF_DL.dsf"), TEXT(
			"#pragma disable_warning(DSH2260)\n"
			"ShaderFunction(Name = \"MF_DL\", NoGeneration = maybe)\n"
			"{\n"
			"    Inputs = { float x; }\n"
			"    Outputs = { float y; }\n"
			"    Graph = { y = x; }\n"
			"}\n"));
		FDreamShaderLang2PipelineResult Result;
		const bool bChecked = Check(File, Result);
		TestTrue(FString::Printf(TEXT("a 1.x file takes the pragma (%s)"), *Describe(Result.Diagnostics)), bChecked);
		TestFalse(TEXT("and it disables a 1.x parser warning"), GatherCodes(Result.Diagnostics).Contains(TEXT("DSH2260")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
