// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Packages.* -- a package generating material functions (Docs/tools/packages.md):
//
//   Paths               the package directory, its asset root and the Assets-relative folder of a file.
//   Enumeration         which package files the build paths take: the Assets sources and the asset headers, nothing else.
//   LegacyAttributes    `Function(Generate = ...)` / `Namespace(..., Generate = ...)` become @generate directives.
//   PragmaRoundTrip     `#pragma generate(...)` prints back as written.
//   HeaderRequests      what a 2.0 asset header asks for, and the four mistakes it can make.
//   CompileLegacyHeader a 1.x Namespace with a Generate template builds one material function per member, into the
//                       package's asset root, named after the header.
//   CompileAssetsFolder a `.dss` under Assets builds into the asset root plus its Assets-relative folder.
//   GenerateOutsidePackage @generate anywhere else is DSH7234 and builds nothing.
//   AdoptRefused        Adopt Into Source on an asset a package built is DSH8133.
//
// Every test makes its own package under the project root's Packages tree and deletes it, and every asset under its
// asset root, on the way out.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"
#include "DreamShaderSourceFileUtils.h"
#include "Lang/LangParser.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Provenance/DreamShaderProvenanceActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialFunction.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	// Fixture helpers shared with DreamShaderAutomationTests.cpp (defined there, external linkage).
	void AddExpectedNewAssetProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath);
	void AddExpectedAutomationCleanupWarnings(FAutomationTestBase& Test);
}

namespace UE::DreamShader::Editor::Private::PackageAssetsTests
{
	/** A package of the project root, `Packages/@dstest/<unique>`, with its own asset root; both are deleted on destruction. */
	class FScopedTestPackage
	{
	public:
		FScopedTestPackage()
		{
			Name = FString(TEXT("p")) + FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
			Directory = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(UE::DreamShader::GetPackageShaderDirectory(), TEXT("@dstest"), Name));
			AssetRoot = FString::Printf(TEXT("/Game/DreamShaderPackages/dstest/%s"), *Name);
			IFileManager::Get().MakeDirectory(*Directory, true);
		}

		~FScopedTestPackage()
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
				Test.AddError(FString::Printf(TEXT("Could not write the test package file '%s'."), *Path));
			}
			return Path;
		}

		FString ObjectPath(const FString& RelativeAssetPath) const
		{
			return FString::Printf(TEXT("%s/%s.%s"), *AssetRoot, *RelativeAssetPath, *FPaths::GetCleanFilename(RelativeAssetPath));
		}

		FString Name;
		FString Directory;
		FString AssetRoot;
	};

	bool HasLine(const TArray<FString>& Lines, const TCHAR* Needle)
	{
		return Lines.ContainsByPredicate([Needle](const FString& Line) { return Line.Contains(Needle); });
	}

	/** Every diagnostic of a resolution, as `DSHnnnn: message` lines. */
	TArray<FString> GatherLines(const UE::DreamShader::Lang::FLangDiagnosticSink& Sink)
	{
		TArray<FString> Lines;
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			Lines.Add(Diagnostic.Code + TEXT(": ") + Diagnostic.Message.ToString());
		}
		return Lines;
	}

	FString JoinLines(const TArray<FString>& Lines)
	{
		return FString::Join(Lines, TEXT("\n"));
	}

	UE::DreamShader::FDreamShaderCompileResult Compile(const FString& SourceFile)
	{
		UE::DreamShader::FDreamShaderCompileRequest Request;
		Request.SourceFilePath = SourceFile;
		Request.bForce = true;
		Request.ThinCustomPersistence = UE::DreamShader::EThinCustomPersistence::Materialized;
		UE::DreamShader::IDreamShaderCompiler* const Compiler = UE::DreamShader::GetDreamShaderCompiler();
		return Compiler ? Compiler->CompileAssets(Request) : UE::DreamShader::FDreamShaderCompileResult();
	}

	/** The names of the function's input and output pins, in order. */
	void GetPinNames(UMaterialFunction* Function, TArray<FString>& OutInputs, TArray<FString>& OutOutputs)
	{
		TArray<FFunctionExpressionInput> Inputs;
		TArray<FFunctionExpressionOutput> Outputs;
		Function->GetInputsAndOutputs(Inputs, Outputs);
		for (const FFunctionExpressionInput& Input : Inputs)
		{
			OutInputs.Add(Input.ExpressionInput ? Input.ExpressionInput->InputName.ToString() : FString());
		}
		for (const FFunctionExpressionOutput& Output : Outputs)
		{
			OutOutputs.Add(Output.ExpressionOutput ? Output.ExpressionOutput->OutputName.ToString() : FString());
		}
	}

	const TCHAR* const LegacyHeader = TEXT(
		"// A 1.x library header that asks for a material function per member.\n"
		"Namespace(Name = \"T\", Generate = \"MF_T_{Name}\", Library = \"DSTest|Packages\")\n"
		"{\n"
		"    Function Mix(in vec3 a, in vec3 b, out vec3 r)\n"
		"    {\n"
		"        r = lerp(a, b, 0.25);\n"
		"    }\n"
		"\n"
		"    GraphFunction Scale(in float x, out float y)\n"
		"    {\n"
		"        y = x * 2.0;\n"
		"    }\n"
		"\n"
		"    Function(Generate = \"\") Private(in float x, out float y)\n"
		"    {\n"
		"        y = x;\n"
		"    }\n"
		"\n"
		"    Function(Generate = \"Extra/MF_T_Renamed\", Description = \"Renamed one\") Named(in float x, out float y)\n"
		"    {\n"
		"        y = x + 1.0;\n"
		"    }\n"
		"}\n");
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesPathsTest,
	"DreamShader.Packages.Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesPathsTest::RunTest(const FString& Parameters)
{
	const FString Packages = UE::DreamShader::NormalizeSourceFilePath(UE::DreamShader::GetPackageShaderDirectory());

	const FString Scoped = Packages + TEXT("/@scope/my-lib/Assets/Noise/MF_X.dss");
	TestEqual(TEXT("a scoped package is two folders"), UE::DreamShader::FindPackageDirectoryForFile(Scoped), Packages + TEXT("/@scope/my-lib"));
	TestTrue(TEXT("a .dss under Assets is an Assets source"), UE::DreamShader::IsPackageAssetsSourceFile(Scoped));
	TestEqual(TEXT("the asset root drops '@'"), UE::DreamShader::GetPackageAssetRoot(Scoped), FString(TEXT("/Game/DreamShaderPackages/scope/my-lib")));
	TestEqual(TEXT("the folder under Assets"), UE::DreamShader::GetPackageAssetsRelativeDirectory(Scoped), FString(TEXT("Noise")));

	const FString Plain = Packages + TEXT("/lib/Library/Noise.dsh");
	TestEqual(TEXT("an unscoped package is one folder"), UE::DreamShader::FindPackageDirectoryForFile(Plain), Packages + TEXT("/lib"));
	TestFalse(TEXT("a header is never an Assets source"), UE::DreamShader::IsPackageAssetsSourceFile(Plain));
	TestEqual(TEXT("no Assets folder, no relative folder"), UE::DreamShader::GetPackageAssetsRelativeDirectory(Plain), FString());

	TestTrue(TEXT("a .dsp under Assets is not built"), !UE::DreamShader::IsPackageAssetsSourceFile(Packages + TEXT("/lib/Assets/P_X.dsp")));
	TestEqual(TEXT("a file directly in Packages is in no package"), UE::DreamShader::FindPackageDirectoryForFile(Packages + TEXT("/Loose.dsh")), FString());
	TestEqual(TEXT("a file directly in a scope is in no package"), UE::DreamShader::FindPackageDirectoryForFile(Packages + TEXT("/@scope/Loose.dsh")), FString());
	TestFalse(TEXT("a project file is no package file"),
		UE::DreamShader::IsPackageFilePath(UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), TEXT("Materials/M.dss")))));
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesEnumerationTest,
	"DreamShader.Packages.Enumeration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesEnumerationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::PackageAssetsTests;

	FScopedTestPackage Package;
	const FString AssetsSource = Package.Write(*this, TEXT("Assets/MF_A.dss"), TEXT("export float MF_A(float x) { return x; }\n"));
	const FString AssetHeader = Package.Write(*this, TEXT("Library/Gen.dsh"), TEXT("/// @generate MF_Gen\nfloat Gen(float x) { return x * 2.0; }\n"));
	const FString PlainHeader = Package.Write(*this, TEXT("Library/Plain.dsh"), TEXT("float Plain(float x) { return x; }\n"));
	const FString Example = Package.Write(*this, TEXT("Examples/M_Example.dss"), TEXT("export void M_Example(inout material m) { }\n"));
	const FString Loose = Package.Write(*this, TEXT("Other/M_Loose.dss"), TEXT("export void M_Loose(inout material m) { }\n"));
	const FString ExampleHeader = Package.Write(*this, TEXT("Examples/ExampleGen.dsh"), TEXT("/// @generate MF_Example\nfloat ExampleGen(float x) { return x; }\n"));

	TestTrue(TEXT("an Assets source is buildable"), FDreamShaderSourceFileUtils::IsBuildablePackageFile(AssetsSource));
	TestTrue(TEXT("an annotated header is an asset header"), FDreamShaderSourceFileUtils::IsPackageAssetHeader(AssetHeader));
	TestFalse(TEXT("a plain header is not"), FDreamShaderSourceFileUtils::IsPackageAssetHeader(PlainHeader));
	TestFalse(TEXT("an annotated header under Examples is not"), FDreamShaderSourceFileUtils::IsPackageAssetHeader(ExampleHeader));
	TestTrue(TEXT("an asset header is a compile unit"), FDreamShaderSourceFileUtils::IsCompileUnit(AssetHeader));
	TestFalse(TEXT("a plain header is not a compile unit"), FDreamShaderSourceFileUtils::IsCompileUnit(PlainHeader));
	TestTrue(TEXT("a package .dss outside Assets is excluded"), FDreamShaderSourceFileUtils::IsExcludedPackageFile(Loose));
	TestTrue(TEXT("an example is excluded"), FDreamShaderSourceFileUtils::IsExcludedPackageFile(Example));

	TArray<FString> Building;
	FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(Building, EDreamShaderPackageSources::IncludeBuildable);
	TestTrue(TEXT("the build enumeration takes the Assets source"), Building.Contains(AssetsSource));
	TestTrue(TEXT("the build enumeration takes the asset header"), Building.Contains(AssetHeader));
	TestFalse(TEXT("the build enumeration leaves the plain header"), Building.Contains(PlainHeader));
	TestFalse(TEXT("the build enumeration leaves the example"), Building.Contains(Example));
	TestFalse(TEXT("the build enumeration leaves the loose source"), Building.Contains(Loose));

	TArray<FString> Rewriting;
	FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(Rewriting);
	TestFalse(TEXT("the default enumeration takes nothing from a package"), Rewriting.ContainsByPredicate([&Package](const FString& File)
	{
		return File.StartsWith(Package.Directory, ESearchCase::IgnoreCase);
	}));

	TArray<FString> Materials;
	FDreamShaderSourceFileUtils::FindProjectMaterialSourceFiles(Materials);
	TestTrue(TEXT("the material enumeration takes the Assets source"), Materials.Contains(AssetsSource));
	TestTrue(TEXT("the material enumeration takes the asset header"), Materials.Contains(AssetHeader));
	TestFalse(TEXT("the material enumeration leaves the loose source (it used to build it in the editor only)"), Materials.Contains(Loose));
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesLegacyAttributesTest,
	"DreamShader.Packages.LegacyAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesLegacyAttributesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::PackageAssetsTests;

	const FLangSourceText Source(TEXT("C:/Header.dsh"), FString(LegacyHeader));
	const FLangParseResult Parse = ParseDreamShaderLang(Source);
	if (!TestTrue(FString::Printf(TEXT("the header parses (%s)"), *JoinLines(GatherLines(Parse.Diagnostics))), Parse.Succeeded()))
	{
		return false;
	}

	TMap<FString, const FFunctionDecl*> Functions;
	for (const FDeclPtr& Decl : Parse.Module->Declarations)
	{
		if (const FFunctionDecl* Function = Decl.IsValid() ? Decl->As<FFunctionDecl>() : nullptr)
		{
			Functions.Add(Function->Name, Function);
		}
	}

	auto GenerateOf = [&Functions](const TCHAR* Name) -> FString
	{
		const FFunctionDecl* const* Function = Functions.Find(Name);
		const FDocDirective* Directive = Function ? (*Function)->Doc.Find(TEXT("generate")) : nullptr;
		return Directive ? Directive->Value : FString(TEXT("<none>"));
	};
	TestEqual(TEXT("the template names a member by its own name"), GenerateOf(TEXT("T_Mix")), FString(TEXT("MF_T_Mix")));
	TestEqual(TEXT("a GraphFunction member too"), GenerateOf(TEXT("T_Scale")), FString(TEXT("MF_T_Scale")));
	TestEqual(TEXT("Generate = \"\" opts out"), GenerateOf(TEXT("T_Private")), FString(TEXT("<none>")));
	TestEqual(TEXT("a member's own Generate wins"), GenerateOf(TEXT("T_Named")), FString(TEXT("Extra/MF_T_Renamed")));

	const FFunctionDecl* const* Mix = Functions.Find(TEXT("T_Mix"));
	const FDocDirective* Library = Mix ? (*Mix)->Doc.Find(TEXT("library")) : nullptr;
	TestEqual(TEXT("the namespace's Library reaches the member"), Library ? Library->Value : FString(), FString(TEXT("DSTest|Packages")));
	const FFunctionDecl* const* Named = Functions.Find(TEXT("T_Named"));
	const FDocDirective* Description = Named ? (*Named)->Doc.Find(TEXT("desc")) : nullptr;
	TestEqual(TEXT("a member's Description becomes @desc"), Description ? Description->Value : FString(), FString(TEXT("Renamed one")));

	{
		const FLangSourceText Bad(TEXT("C:/Bad.dsh"), FString(TEXT(
			"Function(Genrate = \"MF_X\") A(in float x, out float y) { y = x; }\n"
			"Namespace(Name = \"N\", Generate = \"MF_{Nme}\") { Function B(in float x, out float y) { y = x; } }\n")));
		const FLangParseResult BadParse = ParseDreamShaderLang(Bad);
		const TArray<FString> Lines = GatherLines(BadParse.Diagnostics);
		TestTrue(FString::Printf(TEXT("a misspelt key is DSH6331 (%s)"), *JoinLines(Lines)), HasLine(Lines, TEXT("DSH6331")));
		TestTrue(FString::Printf(TEXT("an unknown placeholder is DSH6332 (%s)"), *JoinLines(Lines)), HasLine(Lines, TEXT("DSH6332")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesPragmaRoundTripTest,
	"DreamShader.Packages.PragmaRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesPragmaRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;

	const FLangSourceText Source(TEXT("C:/Gen.dsh"), FString(TEXT(
		"#pragma generate(Functions = \"MF_{Name}\", Library = \"A|B\")\n"
		"/// @generate none\n"
		"float Skip(float x) { return x; }\n")));
	const FLangParseResult Parse = ParseDreamShaderLang(Source);
	if (!TestTrue(TEXT("the header parses"), Parse.Succeeded()))
	{
		return false;
	}
	const FDecl* First = Parse.Module->Declarations.Num() > 0 ? Parse.Module->Declarations[0].Get() : nullptr;
	const FPragmaDecl* Pragma = First ? First->As<FPragmaDecl>() : nullptr;
	TestTrue(TEXT("the pragma is a generate pragma"), Pragma && Pragma->PragmaKind == EPragmaKind::Generate);
	const FString Printed = PrintDreamShaderLang(*Parse.Module);
	TestTrue(FString::Printf(TEXT("the printer writes it back:\n%s"), *Printed), Printed.Contains(TEXT("#pragma generate(Functions = \"MF_{Name}\", Library = \"A|B\")")));
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesHeaderRequestsTest,
	"DreamShader.Packages.HeaderRequests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesHeaderRequestsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Compiler;
	using namespace UE::DreamShader::Editor::Private::PackageAssetsTests;

	FScopedTestPackage Package;

	{
		const FString Header = Package.Write(*this, TEXT("Library/Good.dsh"), TEXT(
			"/// @generate Shapes/MF_Circle\n"
			"/// @library DSTest\n"
			"float Circle(float2 uv, float radius = 0.5) { return step(length(uv - 0.5), radius); }\n"
			"\n"
			"#pragma generate(Functions = \"MF_{Name}\")\n"
			"float Ring(float2 uv) { return Circle(uv, 0.4) - Circle(uv, 0.3); }\n"
			"/// @generate none\n"
			"float Hidden(float x) { return x; }\n"
			"float3 Tint(float3 c, out float luma) { luma = dot(c, float3(0.3, 0.6, 0.1)); return c; }\n"));
		FDreamShaderProductResolution Resolution;
		const bool bResolved = ResolveDreamShaderSourceProducts(Header, Resolution);
		const TArray<FString> Lines = GatherLines(Resolution.Diagnostics);
		TestTrue(FString::Printf(TEXT("the header resolves (%s)"), *JoinLines(Lines)), bResolved);
		TestFalse(TEXT("@library beside @generate is not DSH7224: it describes the generated function"), HasLine(Lines, TEXT("DSH7224")));

		TArray<FString> Paths;
		for (const FDreamShaderResolvedProduct& Product : Resolution.Products)
		{
			Paths.Add(Product.ObjectPath);
		}
		const FString Listed = FString::Join(Paths, TEXT(", "));
		TestEqual(FString::Printf(TEXT("three functions are generated (%s)"), *Listed), Paths.Num(), 3);
		TestTrue(TEXT("@generate with a folder"), Paths.Contains(Package.ObjectPath(TEXT("Shapes/MF_Circle"))));
		TestTrue(TEXT("the template's {Name}"), Paths.Contains(Package.ObjectPath(TEXT("MF_Ring"))));
		TestTrue(TEXT("a function with a return value"), Paths.Contains(Package.ObjectPath(TEXT("MF_Tint"))));
		TestFalse(TEXT("@generate none opts out"), Paths.Contains(Package.ObjectPath(TEXT("MF_Hidden"))));
	}

	{
		const FString Header = Package.Write(*this, TEXT("Library/Bad.dsh"), TEXT(
			"#pragma generate(Functions = \"MF_{Nam}\", Colour = \"Red\")\n"
			"float A(float x) { return x; }\n"
			"#pragma generate()\n"
			"/// @generate MF_Same\n"
			"float B(float x) { return x; }\n"
			"/// @generate MF_Same\n"
			"float C(float x) { return x; }\n"
			"/// @generate MF_Array\n"
			"float D(float x[2]) { return x[0]; }\n"));
		FDreamShaderProductResolution Resolution;
		ResolveDreamShaderSourceProducts(Header, Resolution);
		const TArray<FString> Lines = GatherLines(Resolution.Diagnostics);
		TestTrue(FString::Printf(TEXT("an unknown placeholder is DSH8273 (%s)"), *JoinLines(Lines)), HasLine(Lines, TEXT("DSH8273")));
		TestTrue(TEXT("an array parameter is DSH8274"), HasLine(Lines, TEXT("DSH8274")));
		TestTrue(TEXT("one asset twice is DSH8275"), HasLine(Lines, TEXT("DSH8275")));
		TestTrue(TEXT("an unknown pragma key is DSH8276"), HasLine(Lines, TEXT("DSH8276")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesCompileLegacyHeaderTest,
	"DreamShader.Packages.CompileLegacyHeader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesCompileLegacyHeaderTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::PackageAssetsTests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FScopedTestPackage Package;
	AddExpectedError(Package.AssetRoot, EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedAutomationCleanupWarnings(*this);
	for (const TCHAR* Asset : { TEXT("MF_T_Mix"), TEXT("MF_T_Scale"), TEXT("Extra/MF_T_Renamed") })
	{
		AddExpectedNewAssetProbeWarnings(*this, Package.ObjectPath(Asset));
	}

	const FString Header = Package.Write(*this, TEXT("Library/Tools.dsh"), LegacyHeader);
	const UE::DreamShader::FDreamShaderCompileResult Result = Compile(Header);
	if (!TestTrue(FString::Printf(TEXT("the asset header compiles: %s"), *Result.Message.ToString()), Result.bSucceeded))
	{
		return false;
	}

	UMaterialFunction* Mix = LoadObject<UMaterialFunction>(nullptr, *Package.ObjectPath(TEXT("MF_T_Mix")));
	UMaterialFunction* Scale = LoadObject<UMaterialFunction>(nullptr, *Package.ObjectPath(TEXT("MF_T_Scale")));
	UMaterialFunction* Renamed = LoadObject<UMaterialFunction>(nullptr, *Package.ObjectPath(TEXT("Extra/MF_T_Renamed")));
	TestNotNull(TEXT("MF_T_Mix is in the package's asset root"), Mix);
	TestNotNull(TEXT("MF_T_Scale (a GraphFunction) too"), Scale);
	TestNotNull(TEXT("a member's own Generate with a folder"), Renamed);
	TestNull(TEXT("the member that opted out builds nothing"), LoadObject<UMaterialFunction>(nullptr, *Package.ObjectPath(TEXT("MF_T_Private"))));
	if (!Mix)
	{
		return false;
	}

	TArray<FString> Inputs;
	TArray<FString> Outputs;
	GetPinNames(Mix, Inputs, Outputs);
	TestEqual(TEXT("the inputs are the function's in parameters"), FString::Join(Inputs, TEXT(",")), FString(TEXT("a,b")));
	TestEqual(TEXT("the output is its out parameter"), FString::Join(Outputs, TEXT(",")), FString(TEXT("r")));
	TestTrue(TEXT("Library exposes it to the function library"), Mix->bExposeToLibrary != 0);
	// A bar nests a category in the palette and stays inside the text; a comma would separate two.
	TestTrue(TEXT("with its nested category"), Mix->LibraryCategoriesText.ContainsByPredicate([](const FText& Category) { return Category.ToString() == TEXT("DSTest|Packages"); }));
	if (Renamed)
	{
		TestEqual(TEXT("Description becomes the function's description"), Renamed->Description, FString(TEXT("Renamed one")));
	}

	const FString Stamped = UE::DreamShader::Editor::Private::GetGeneratedAssetSourceFile(Mix);
	TestTrue(FString::Printf(TEXT("the asset names the header as its source (%s)"), *Stamped), Stamped.EndsWith(TEXT("Library/Tools.dsh")));
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesCompileAssetsFolderTest,
	"DreamShader.Packages.CompileAssetsFolder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesCompileAssetsFolderTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::PackageAssetsTests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FScopedTestPackage Package;
	AddExpectedError(Package.AssetRoot, EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedAutomationCleanupWarnings(*this);
	AddExpectedNewAssetProbeWarnings(*this, Package.ObjectPath(TEXT("Noise/MF_Wave")));
	AddExpectedNewAssetProbeWarnings(*this, Package.ObjectPath(TEXT("MF_Legacy")));

	Package.Write(*this, TEXT("Library/Wave.dsh"), TEXT("float WaveOf(float x) { return sin(x) * 0.5 + 0.5; }\n"));
	const FString Dss = Package.Write(*this, TEXT("Assets/Noise/MF_Wave.dss"), TEXT(
		"#include \"../../Library/Wave.dsh\"\n"
		"export float MF_Wave(float x) { return WaveOf(x); }\n"));
	const FString Dsf = Package.Write(*this, TEXT("Assets/MF_Legacy.dsf"), TEXT(
		"ShaderFunction(Name = \"MF_Legacy\")\n"
		"{\n"
		"    Inputs = { float x; }\n"
		"    Outputs = { float y; }\n"
		"    Graph = { y = x * 3.0; }\n"
		"}\n"));

	const UE::DreamShader::FDreamShaderCompileResult DssResult = Compile(Dss);
	TestTrue(FString::Printf(TEXT("the Assets .dss compiles: %s"), *DssResult.Message.ToString()), DssResult.bSucceeded);
	TestNotNull(TEXT("into the asset root plus its folder under Assets"), LoadObject<UMaterialFunction>(nullptr, *Package.ObjectPath(TEXT("Noise/MF_Wave"))));

	const UE::DreamShader::FDreamShaderCompileResult DsfResult = Compile(Dsf);
	TestTrue(FString::Printf(TEXT("the Assets .dsf compiles: %s"), *DsfResult.Message.ToString()), DsfResult.bSucceeded);
	TestNotNull(TEXT("a 1.x Name= with no Root is under the asset root"), LoadObject<UMaterialFunction>(nullptr, *Package.ObjectPath(TEXT("MF_Legacy"))));
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesGenerateOutsidePackageTest,
	"DreamShader.Packages.GenerateOutsidePackage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesGenerateOutsidePackageTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Compiler;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::PackageAssetsTests;

	FDreamShaderCompile2Fixture Fixture(TEXT("GenerateOutsidePackage"), TEXT("Packages"), TEXT("dss"));
	if (!Fixture.WriteSource(*this, FString::Printf(TEXT(
		"/// @generate MF_Nope\n"
		"float Helper(float x) { return x; }\n"
		"/// @name %s/M_Outside\n"
		"export void M_Outside(inout material m) { m.EmissiveColor = float3(Helper(0.5), 0, 0); }\n"), *Fixture.GetPackagePath())))
	{
		return false;
	}

	FDreamShaderProductResolution Resolution;
	const bool bResolved = ResolveDreamShaderSourceProducts(Fixture.GetSourceFilePath(), Resolution);
	const TArray<FString> Lines = GatherLines(Resolution.Diagnostics);
	TestTrue(FString::Printf(TEXT("the source still resolves (%s)"), *JoinLines(Lines)), bResolved);
	TestTrue(TEXT("@generate outside a package is DSH7234"), HasLine(Lines, TEXT("DSH7234")));
	TestEqual(TEXT("and generates nothing: one product, the material"), Resolution.Products.Num(), 1);
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPackagesAdoptRefusedTest,
	"DreamShader.Packages.AdoptRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPackagesAdoptRefusedTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::PackageAssetsTests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FScopedTestPackage Package;
	AddExpectedError(Package.AssetRoot, EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("DSH8133"), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedAutomationCleanupWarnings(*this);
	AddExpectedNewAssetProbeWarnings(*this, Package.ObjectPath(TEXT("MF_Adopt")));

	const FString Header = Package.Write(*this, TEXT("Library/Adopt.dsh"), TEXT("/// @generate MF_Adopt\nfloat Adopt(float x) { return x; }\n"));
	const FString Before = [&Header]() { FString Text; FFileHelper::LoadFileToString(Text, *Header); return Text; }();
	if (!TestTrue(TEXT("the asset header compiles"), Compile(Header).bSucceeded))
	{
		return false;
	}
	UMaterialFunction* Asset = LoadObject<UMaterialFunction>(nullptr, *Package.ObjectPath(TEXT("MF_Adopt")));
	if (!TestNotNull(TEXT("the function exists"), Asset))
	{
		return false;
	}

	const FDreamShaderProvenanceOutcome Outcome = AdoptGeneratedAssetIntoSourceCore(Asset, Header, /*bWriteBackup*/ false);
	TestFalse(TEXT("Adopt is refused"), Outcome.bSucceeded);
	TestEqual(TEXT("with DSH8133"), Outcome.Code, FString(TEXT("DSH8133")));
	FString After;
	FFileHelper::LoadFileToString(After, *Header);
	TestEqual(TEXT("and the header is untouched"), After, Before);
	return true;
}

#endif
