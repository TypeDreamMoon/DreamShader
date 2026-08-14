#include "Commandlet/DreamShaderCommandletRunner.h"
#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "DreamShaderMaterialInstance.h"
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "DreamShaderSettings.h"
#include "DreamShaderTestCommon.h"
#include "DreamShaderTypes.h"
#include "DreamShaderVersionCompat.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"
// EstimateMaterialNodeSize / FLayoutNodeSize, so the layout test measures placed nodes by the same
// rule the placement pass used.
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"
#include "Preview/DreamShaderPreviewRenderer.h"

#include "AssetCompilingManager.h"
#include "Engine/Texture.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameters.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialEditingLibrary.h"
#include "Engine/Texture.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::DreamShader::Editor::Private::Tests
{
	FString MakeUniqueTestAssetName(const TCHAR* Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	FString GetAutomationSourceDirectory()
	{
		return FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), TEXT("Tests"), TEXT("Automation"));
	}

	FString MakeAutomationSourcePath(const FString& FileName)
	{
		return UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(GetAutomationSourceDirectory(), FileName));
	}

	FString MakeAutomationObjectPath(const FString& AssetName)
	{
		return FString::Printf(TEXT("/Game/DreamShaderTests/Automation/%s.%s"), *AssetName, *AssetName);
	}

	bool WriteAutomationSourceFile(
		FAutomationTestBase& Test,
		const FString& FileName,
		const FString& SourceText,
		FString& OutSourceFilePath)
	{
		OutSourceFilePath = MakeAutomationSourcePath(FileName);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutSourceFilePath), true);
		if (!FFileHelper::SaveStringToFile(SourceText, *OutSourceFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Test.AddError(FString::Printf(TEXT("Failed to write DreamShader automation source file '%s'."), *OutSourceFilePath));
			return false;
		}

		return true;
	}

	void AddExpectedNewAssetProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath)
	{
		// Negative occurrence count = suppress these new-asset probe messages if they fire, but do
		// not require them: UE 5.8 / Moon does not always emit the SkipPackage probe warning, and a
		// hard "expected 1, found 0" requirement makes the generate tests spuriously fail.
		Test.AddExpectedError(
			FString::Printf(TEXT("SkipPackage: %s"), *FPackageName::ObjectPathToPackageName(ObjectPath)),
			EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(
			FString::Printf(TEXT("%s"), *ObjectPath),
			EAutomationExpectedErrorFlags::Contains, -1);
	}

	void AddExpectedAutomationCleanupWarnings(FAutomationTestBase& Test)
	{
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
	}

	void DeleteSourceFileForAutomation(const FString& SourceFilePath)
	{
		if (!SourceFilePath.IsEmpty())
		{
			IFileManager::Get().Delete(*SourceFilePath, false, true);
		}
	}

	void DeleteAssetForAutomation(const FString& ObjectPath)
	{
		if (ObjectPath.IsEmpty())
		{
			return;
		}

		UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
		if (!Object)
		{
			return;
		}

		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(Object);
		ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
	}

	class FScopedDreamShaderAutomationArtifacts
	{
	public:
		void AddSourceFile(const FString& SourceFilePath)
		{
			SourceFilePaths.Add(SourceFilePath);
		}

		void AddObjectPath(const FString& ObjectPath)
		{
			ObjectPaths.Add(ObjectPath);
		}

		~FScopedDreamShaderAutomationArtifacts()
		{
			for (const FString& ObjectPath : ObjectPaths)
			{
				DeleteAssetForAutomation(ObjectPath);
			}

			for (const FString& SourceFilePath : SourceFilePaths)
			{
				DeleteSourceFileForAutomation(SourceFilePath);
			}
		}

	private:
		TArray<FString> SourceFilePaths;
		TArray<FString> ObjectPaths;
	};

	FString MakeMinimalMaterialSource(const FString& AssetName)
	{
		return FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
)"), *AssetName);
	}

	FString MakeSharedHeaderSource()
	{
		return TEXT(R"(
Function ApplyAutomationTint(in vec3 color, in vec3 tint, out vec3 result) {
    result = color * tint;
}
)");
	}

	FString MakeImportedFunctionSource(const FString& HeaderFileName, const FString& AssetName)
	{
		return FString::Printf(TEXT(R"(
import "%s";

ShaderFunction(Name="DreamShaderTests/Automation/%s")
{
    Inputs = {
        vec3 InColor;
        vec3 InTint;
    }

    Outputs = {
        vec3 OutColor;
    }

    Graph = {
        ApplyAutomationTint(InColor, InTint, OutColor);
    }
}
)"), *HeaderFileName, *AssetName);
	}

	FString MakeSubstrateMaterialSource(const FString& AssetName)
	{
		return FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Outputs = {
        Substrate Surface;
        Base.FrontMaterial = Surface;
    }

    Graph = {
        Surface = Substrate.Unlit(EmissiveColor=vec3(0.1, 0.6, 1.0));
    }
}
)"), *AssetName);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderParserMinimalMaterialTest,
	"DreamShader.Compiler.Parser.MinimalMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderParserMinimalMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FTextShaderDefinition Definition;
	FString ParseError;
	const bool bParsed = FTextShaderParser::Parse(MakeMinimalMaterialSource(TEXT("M_ParserMinimal")), Definition, ParseError);
	if (!TestTrue(FString::Printf(TEXT("Parser succeeds: %s"), *ParseError), bParsed))
	{
		return false;
	}

	TestEqual(TEXT("Shader name"), Definition.Name, FString(TEXT("DreamShaderTests/Automation/M_ParserMinimal")));
	FString Domain;
	TestTrue(TEXT("Domain setting exists"), Definition.TryGetSetting(TEXT("Domain"), Domain));
	TestEqual(TEXT("Domain setting"), Domain, FString(TEXT("UI")));
	TestEqual(TEXT("Output declaration count"), Definition.OutputDeclarations.Num(), 1);
	TestEqual(TEXT("Output binding count"), Definition.Outputs.Num(), 1);
	TestFalse(TEXT("Graph code is captured"), Definition.Code.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateMinimalMaterialTest,
	"DreamShader.Compiler.Generate.MinimalMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderGenerateMinimalMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoMinimal"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeMinimalMaterialSource(AssetName), SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true);
	if (!TestTrue(FString::Printf(TEXT("Material generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UMaterial* GeneratedMaterial = LoadObject<UMaterial>(nullptr, *ObjectPath);
	TestNotNull(FString::Printf(TEXT("Generated material loads from '%s'."), *ObjectPath), GeneratedMaterial);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateDsfWithImportTest,
	"DreamShader.Compiler.Generate.DsfWithImport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderGenerateDsfWithImportTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString FunctionName = MakeUniqueTestAssetName(TEXT("F_AutoImport"));
	const FString HeaderFileName = FunctionName + TEXT("_Shared.dsh");
	const FString ObjectPath = MakeAutomationObjectPath(FunctionName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString HeaderFilePath;
	if (!WriteAutomationSourceFile(*this, HeaderFileName, MakeSharedHeaderSource(), HeaderFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(HeaderFilePath);

	FString FunctionFilePath;
	if (!WriteAutomationSourceFile(*this, FunctionName + TEXT(".dsf"), MakeImportedFunctionSource(HeaderFileName, FunctionName), FunctionFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(FunctionFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateAssetsFromFile(FunctionFilePath, Message, true);
	if (!TestTrue(FString::Printf(TEXT("Imported .dsf generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UMaterialFunction* GeneratedFunction = LoadObject<UMaterialFunction>(nullptr, *ObjectPath);
	TestNotNull(FString::Printf(TEXT("Generated material function loads from '%s'."), *ObjectPath), GeneratedFunction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateSubstrateMaterialTest,
	"DreamShader.Compiler.Generate.SubstrateMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderGenerateSubstrateMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoSubstrate"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeSubstrateMaterialSource(AssetName), SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true);
	if (!TestTrue(FString::Printf(TEXT("Substrate material generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UMaterial* GeneratedMaterial = LoadObject<UMaterial>(nullptr, *ObjectPath);
	TestNotNull(FString::Printf(TEXT("Generated Substrate material loads from '%s'."), *ObjectPath), GeneratedMaterial);
#else
	AddInfo(TEXT("DreamShader Substrate builtins are not available for this Unreal Engine version; skipping generation test."));
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSourceHashSkipTest,
	"DreamShader.Compiler.SourceHash.SkipUnchangedMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSourceHashSkipTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoHash"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeMinimalMaterialSource(AssetName), SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString FirstMessage;
	if (!TestTrue(
		FString::Printf(TEXT("Initial material generation succeeds: %s"), *FirstMessage),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, FirstMessage, true)))
	{
		return false;
	}

	FString SecondMessage;
	const bool bSkipped = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, SecondMessage, false);
	if (!TestTrue(FString::Printf(TEXT("Unchanged material generation succeeds: %s"), *SecondMessage), bSkipped))
	{
		return false;
	}
	TestTrue(
		FString::Printf(TEXT("Unchanged source is skipped: %s"), *SecondMessage),
		SecondMessage.Contains(TEXT("source hash is unchanged"), ESearchCase::IgnoreCase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCommandletCompileSingleSourceSmokeTest,
	"DreamShader.Commandlet.Compile.SingleSourceSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCommandletCompileSingleSourceSmokeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoCommandlet"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeMinimalMaterialSource(AssetName), SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	TArray<FString> Tokens;
	TArray<FString> Switches;
	Switches.Add(TEXT("Force"));

	TMap<FString, FString> Params;
	Params.Add(TEXT("Source"), SourceFilePath);

	const bool bCompiled = RunDreamShaderCompileCommandlet(Tokens, Switches, Params);
	if (!TestTrue(TEXT("DreamShader compile commandlet runner succeeds."), bCompiled))
	{
		return false;
	}

	UMaterial* GeneratedMaterial = LoadObject<UMaterial>(nullptr, *ObjectPath);
	TestNotNull(FString::Printf(TEXT("Commandlet generated material loads from '%s'."), *ObjectPath), GeneratedMaterial);
	return true;
}

// Suppresses incidental log errors during generation. Laying out an if-statement material trips a
// benign engine SlowTask progress-frame ensure in LayoutGeneratedExpressions (Support.cpp ~4826)
// which would otherwise fail this test even though the generated graph is correct.
class FDreamShaderQuietAutomationTestBase : public FAutomationTestBase
{
public:
	FDreamShaderQuietAutomationTestBase(const FString& InName, bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	virtual bool SuppressLogErrors() override { return true; }
	virtual bool SuppressLogWarnings() override { return true; }
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTruthyConditionWiringTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Gen.Wiring.TruthyCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTruthyConditionWiringTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Asserts Graph-backend node shape (UMaterialExpressionIf), so pin the Graph backend — the
	// if/else source is expressible in the Instance backend and would otherwise route there.
	FScopedDreamShaderGraphBackendPin BackendPin;
	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_Truthy"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        ScalarParameter Sign = -1.0;
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec3 Color = vec3(0.0, 0.0, 0.0);
        if (Sign) {
            Color = vec3(1.0, 0.0, 0.0);
        } else {
            Color = vec3(0.0, 1.0, 0.0);
        }
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("Material generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated material loads"), Material))
	{
		return false;
	}

	UMaterialExpressionIf* IfExpression = nullptr;
	for (auto&& ExpressionPtr : Material->GetExpressions())
	{
		if (UMaterialExpressionIf* Candidate = Cast<UMaterialExpressionIf>(ExpressionPtr))
		{
			IfExpression = Candidate;
			break;
		}
	}
	if (!TestNotNull(TEXT("Material contains a Material If node"), IfExpression))
	{
		return false;
	}

	// `if (Sign)` is truthy == (Sign != 0): the then-branch must be selected for any non-zero Sign
	// (both Sign > 0 and Sign < 0), so the If node wires the SAME (then) value to AGreaterThanB and
	// ALessThanB. Before the fix, truthy was treated as Sign > 0 and ALessThanB pointed at the else
	// value, so negative conditions wrongly selected the else branch.
	TestTrue(TEXT("If A>B is connected"), IfExpression->AGreaterThanB.Expression != nullptr);
	TestTrue(TEXT("If A<B is connected"), IfExpression->ALessThanB.Expression != nullptr);
	TestTrue(
		TEXT("truthy: A<B selects the same then-value as A>B (non-zero -> then-branch)"),
		IfExpression->ALessThanB.Expression == IfExpression->AGreaterThanB.Expression);
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderRoundTripMaterialTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Roundtrip.MaterialDecompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderRoundTripMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// These assertions describe graph-backend node shapes; pin the backend against reroutes.
	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_RoundTrip"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        VectorParameter BaseColor = float4(0.8, 0.4, 0.2, 1.0) [
            Group="Surface";
        ];
        ScalarParameter Roughness = 0.55;
    }

    Settings = {
        Domain = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode = "Opaque";
    }

    Outputs = {
        float3 Color;
        float Rough;
        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        Color = BaseColor.rgb;
        Rough = Roughness;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("Material generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated material loads"), Material))
	{
		return false;
	}

	// Decompile the generated material back to DreamShaderLang source.
	FString DecompiledSource;
	FString DecompileError;
	const bool bDecompiled = GetGraphDecompiler().DecompileMaterial(
		Material, TEXT("Decompiled/M_RoundTrip"), DecompiledSource, DecompileError);
	if (!TestTrue(FString::Printf(TEXT("Decompile succeeds: %s"), *DecompileError), bDecompiled))
	{
		return false;
	}
	TestFalse(TEXT("Decompiled source is non-empty"), DecompiledSource.IsEmpty());

	// The round-trip property: the decompiled output must be valid DreamShaderLang that re-parses.
	// This is the only automated coverage of the decompiler's parameter-reuse and material-setting
	// emission paths, so a malformed export (e.g. duplicate parameter declarations) is caught here.
	FTextShaderDefinition ReparsedDefinition;
	FString ReparseError;
	TestTrue(
		FString::Printf(TEXT("Decompiled source re-parses: %s"), *ReparseError),
		FTextShaderParser::Parse(DecompiledSource, ReparsedDefinition, ReparseError));
	TestTrue(TEXT("Decompiled source declares a parameter"), DecompiledSource.Contains(TEXT("Parameter")));
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderRoundTripSubstrateMaterialTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Roundtrip.SubstrateMaterialRegenerates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A Substrate material's shading is driven by its FrontMaterial tree, so the UMaterial's ShadingModel
// enum stays at its default (DefaultLit) -- it does not describe the surface. The decompiler used to
// emit that enum verbatim, producing ShadingModel="DefaultLit" alongside Base.FrontMaterial, which the
// generator rejects ("Base.FrontMaterial requires ShadingModel=Substrate or none"). So a decompiled
// Substrate material would not round-trip. The existing round-trip test only re-PARSES the decompiled
// source; this failure is at GENERATE time, so it needs a re-generate. (Requires r.Substrate=True.)
bool FDreamShaderRoundTripSubstrateMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Substrate materials generate through the Graph backend (the decomposed node path).
	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_SubstrateSrc"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeSubstrateMaterialSource(AssetName), SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("Substrate material generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated Substrate material loads"), Material))
	{
		return false;
	}

	// Decompile to a fresh automation-path name so the re-generated asset is registered for cleanup.
	const FString ReAssetName = MakeUniqueTestAssetName(TEXT("M_SubstrateRoundTrip"));
	const FString ReObjectPath = MakeAutomationObjectPath(ReAssetName);
	Artifacts.AddObjectPath(ReObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ReObjectPath);

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("Substrate decompile succeeds: %s"), *DecompileError),
		GetGraphDecompiler().DecompileMaterial(Material, FString::Printf(TEXT("DreamShaderTests/Automation/%s"), *ReAssetName), DecompiledSource, DecompileError)))
	{
		return false;
	}

	// The fix: a FrontMaterial material decompiles to ShadingModel="Substrate", not the misleading
	// DefaultLit enum, so it satisfies the generator's Base.FrontMaterial rule.
	TestTrue(TEXT("Decompiled Substrate material binds Base.FrontMaterial."), DecompiledSource.Contains(TEXT("Base.FrontMaterial")));
	TestTrue(TEXT("Decompiled Substrate ShadingModel is \"Substrate\"."), DecompiledSource.Contains(TEXT("ShadingModel = \"Substrate\"")));
	TestFalse(TEXT("Decompiled Substrate ShadingModel is not the material's default DefaultLit enum."), DecompiledSource.Contains(TEXT("ShadingModel = \"DefaultLit\"")));

	// The real round-trip: the decompiled source must re-GENERATE. This is the check the reported bug
	// failed -- a re-parse alone would have passed while generation still errored.
	FString ReSourceFilePath;
	if (!WriteAutomationSourceFile(*this, ReAssetName + TEXT(".dsm"), DecompiledSource, ReSourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(ReSourceFilePath);

	FString ReMessage;
	TestTrue(
		FString::Printf(TEXT("Decompiled Substrate source re-generates: %s"), *ReMessage),
		FMaterialGenerator::GenerateMaterialFromFile(ReSourceFilePath, ReMessage, true));

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderRoundTripSwitchTypedAppendTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Roundtrip.SwitchTypedAppendRegenerates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Three decompiler sizing/escaping bugs that only show up at re-GENERATE time, all reachable from one
// graph (they were found together on LGUI's LexUI_RectBlock / LexUI_ImageAndFont):
//   1. Switch-style nodes report no output value type, so the "assume float4" fallback oversized them.
//      An Append of such a value plus one more component was emitted as `float5(...)` -- not a type.
//   2. VertexColor was typed from output 0 (the RGB pin) while the alpha pin's mask was appended as a
//      swizzle, producing `.a` on a three-component value.
//   3. Comment / #Region text carrying a newline was written raw into a "..." literal, splitting the
//      directive across two lines.
bool FDreamShaderRoundTripSwitchTypedAppendTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_SwitchAppendSrc"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Settings = {
        Domain = "Surface";
        ShadingModel = "Unlit";
        BlendMode = "Translucent";
    }

    Outputs = {
        float3 Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity = Alpha;
    }

    Graph = {
        float3 VertexRgb = (UE.Expression(Class="VertexColor", OutputType="float4")).rgb;
        float VertexAlpha = (UE.Expression(Class="VertexColor", OutputType="float4")).a;
        float3 Dimmed = (VertexRgb * 0.5);
        float3 Selected = UE.Expression(Class="StaticSwitch", OutputType="float3", True=Dimmed, False=VertexRgb, Value=true);
        float4 Combined = float4(Selected, VertexAlpha);
        Color = Combined.rgb;
        Alpha = Combined.a;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("Switch/append material generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated switch/append material loads"), Material))
	{
		return false;
	}

	// Stand in for a comment authored in the material editor (the decompiler skips its own
	// "DreamShader: " layout chrome, so only a hand-authored comment exercises the emission path).
	// Multi-line comment text is the common case and used to be written raw into the "..." literal.
	if (UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Material, NAME_None, RF_Transactional))
	{
		Comment->Text = TEXT("Vertex color split\nacross two lines");
		Comment->MaterialExpressionEditorX = -600;
		Comment->MaterialExpressionEditorY = -200;
		Comment->SizeX = 900;
		Comment->SizeY = 600;
		Material->GetExpressionCollection().AddComment(Comment);
	}

	const FString ReAssetName = MakeUniqueTestAssetName(TEXT("M_SwitchAppendRoundTrip"));
	const FString ReObjectPath = MakeAutomationObjectPath(ReAssetName);
	Artifacts.AddObjectPath(ReObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ReObjectPath);

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("Switch/append decompile succeeds: %s"), *DecompileError),
		GetGraphDecompiler().DecompileMaterial(Material, FString::Printf(TEXT("DreamShaderTests/Automation/%s"), *ReAssetName), DecompiledSource, DecompileError)))
	{
		return false;
	}

	TestFalse(TEXT("Decompiled source has no float5 constructor."), DecompiledSource.Contains(TEXT("float5(")));
	TestTrue(TEXT("Decompiled VertexColor is typed float4 so its alpha swizzle is valid."), DecompiledSource.Contains(TEXT("Class=\"VertexColor\", OutputType=\"float4\"")));
	TestTrue(TEXT("Decompiled comment text escapes its newline."), DecompiledSource.Contains(TEXT("Comment(Name=\"Vertex color split\\nacross two lines\"")));

	FString ReSourceFilePath;
	if (!WriteAutomationSourceFile(*this, ReAssetName + TEXT(".dsm"), DecompiledSource, ReSourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(ReSourceFilePath);

	FString ReMessage;
	TestTrue(
		FString::Printf(TEXT("Decompiled switch/append source re-generates: %s"), *ReMessage),
		FMaterialGenerator::GenerateMaterialFromFile(ReSourceFilePath, ReMessage, true));

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderRoundTripCustomAdditionalOutputTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Roundtrip.CustomAdditionalOutputRegenerates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Unreal turns a Custom node's additional outputs into `inout` parameters of the generated function,
// so the code body may only assign to outputs the node declares. A node feeding two outputs is
// emitted once per output, and only the emission carrying Output="..." used to declare the extra one
// -- the other node compiled to "use of undeclared identifier" at shader-compile time, well after
// generation reported success. Every emission must carry the full declaration.
bool FDreamShaderRoundTripCustomAdditionalOutputTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_CustomOutputsSrc"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"DSM(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Settings = {
        Domain = "Surface";
        ShadingModel = "Unlit";
        BlendMode = "Translucent";
    }

    Outputs = {
        float3 Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity = Alpha;
    }

    Graph = {
        float4 Packed = UE.Expression(
            Class="Custom",
            OutputType="float4",
            Code="Extra = UV.x; return float4(UV, 0.0, 1.0);",
            Description="Packer",
            AdditionalOutputs="((OutputName=\"Extra\",OutputType=CMOT_Float1))",
            UV=UE.Expression(Class="TextureCoordinate", OutputType="float2"));
        float Extra = UE.Expression(
            Class="Custom",
            OutputType="float4",
            Code="Extra = UV.x; return float4(UV, 0.0, 1.0);",
            Description="Packer",
            Output="Extra",
            AdditionalOutputs="((OutputName=\"Extra\",OutputType=CMOT_Float1))",
            UV=UE.Expression(Class="TextureCoordinate", OutputType="float2"));
        Color = Packed.rgb;
        Alpha = Extra;
    }
}
)DSM"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("Custom additional-output material generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated Custom additional-output material loads"), Material))
	{
		return false;
	}

	const FString ReAssetName = MakeUniqueTestAssetName(TEXT("M_CustomOutputsRoundTrip"));
	const FString ReObjectPath = MakeAutomationObjectPath(ReAssetName);
	Artifacts.AddObjectPath(ReObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ReObjectPath);

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("Custom additional-output decompile succeeds: %s"), *DecompileError),
		GetGraphDecompiler().DecompileMaterial(Material, FString::Printf(TEXT("DreamShaderTests/Automation/%s"), *ReAssetName), DecompiledSource, DecompileError)))
	{
		return false;
	}

	TestTrue(TEXT("Decompiled Custom node declares its additional output."), DecompiledSource.Contains(TEXT("OutputName=\\\"Extra\\\",OutputType=CMOT_Float1")));
	TestFalse(TEXT("Reading the secondary output does not rewrite the node's own OutputType."), DecompiledSource.Contains(TEXT("Class=\"Custom\", OutputType=\"float\"")));

	FString ReSourceFilePath;
	if (!WriteAutomationSourceFile(*this, ReAssetName + TEXT(".dsm"), DecompiledSource, ReSourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(ReSourceFilePath);

	FString ReMessage;
	if (!TestTrue(
		FString::Printf(TEXT("Decompiled Custom additional-output source re-generates: %s"), *ReMessage),
		FMaterialGenerator::GenerateMaterialFromFile(ReSourceFilePath, ReMessage, true)))
	{
		return false;
	}

	UMaterial* RegeneratedMaterial = LoadObject<UMaterial>(nullptr, *ReObjectPath);
	if (!TestNotNull(TEXT("Regenerated Custom additional-output material loads"), RegeneratedMaterial))
	{
		return false;
	}

	int32 CustomNodeCount = 0;
	for (auto&& ExpressionPtr : RegeneratedMaterial->GetExpressions())
	{
		UMaterialExpressionCustom* CustomExpression = Cast<UMaterialExpressionCustom>(ExpressionPtr);
		if (!CustomExpression)
		{
			continue;
		}

		++CustomNodeCount;
		TestEqual(TEXT("Regenerated Custom node keeps its own float4 return type."), static_cast<int32>(CustomExpression->OutputType.GetValue()), static_cast<int32>(CMOT_Float4));
		if (TestEqual(TEXT("Regenerated Custom node declares exactly one additional output."), CustomExpression->AdditionalOutputs.Num(), 1))
		{
			TestEqual(TEXT("Regenerated additional output keeps its name."), CustomExpression->AdditionalOutputs[0].OutputName.ToString(), FString(TEXT("Extra")));
			TestEqual(TEXT("Regenerated additional output keeps its own type."), static_cast<int32>(CustomExpression->AdditionalOutputs[0].OutputType.GetValue()), static_cast<int32>(CMOT_Float1));
		}
	}
	TestTrue(TEXT("Regenerated material has at least one Custom node."), CustomNodeCount > 0);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderRoundTripStaticBoolFunctionInputTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Roundtrip.StaticBoolFunctionInputRegenerates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A StaticBool function input with a default: Unreal ignores PreviewValue for static-bool inputs and
// compiles whatever is wired to the Preview pin, so the default has to materialize as a StaticBool
// node. The decompiler also has to keep the type token distinct -- "bool" declares a scalar pin, which
// would silently turn the input into a float and reject every static-bool value passed to it.
bool FDreamShaderRoundTripStaticBoolFunctionInputTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString FunctionName = MakeUniqueTestAssetName(TEXT("F_StaticBoolSrc"));
	const FString ObjectPath = MakeAutomationObjectPath(FunctionName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(
ShaderFunction(Name="DreamShaderTests/Automation/%s")
{
    Inputs = {
        float3 Color;
        opt StaticBool UseDimmed = false;
    }

    Outputs = {
        float3 Result;
    }

    Graph = {
        float3 Dimmed = (Color * 0.5);
        Result = UE.Expression(Class="StaticSwitch", OutputType="float3", True=Dimmed, False=Color, Value=UseDimmed);
    }
}
)"), *FunctionName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, FunctionName + TEXT(".dsf"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("StaticBool function generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateAssetsFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterialFunction* GeneratedFunction = LoadObject<UMaterialFunction>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated StaticBool function loads"), GeneratedFunction))
	{
		return false;
	}

	const FString ReFunctionName = MakeUniqueTestAssetName(TEXT("F_StaticBoolRoundTrip"));
	const FString ReObjectPath = MakeAutomationObjectPath(ReFunctionName);
	Artifacts.AddObjectPath(ReObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ReObjectPath);

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("StaticBool function decompile succeeds: %s"), *DecompileError),
		GetGraphDecompiler().DecompileFunction(
			GeneratedFunction,
			FString::Printf(TEXT("DreamShaderTests/Automation/%s"), *ReFunctionName),
			EDreamShaderDecompiledFunctionKind::Function,
			DecompiledSource,
			DecompileError)))
	{
		return false;
	}

	TestTrue(TEXT("Decompiled input keeps the StaticBool type token."), DecompiledSource.Contains(TEXT("opt StaticBool UseDimmed = false")));
	TestFalse(TEXT("Decompiled input is not downgraded to a scalar bool pin."), DecompiledSource.Contains(TEXT("opt bool UseDimmed")));

	FString ReSourceFilePath;
	if (!WriteAutomationSourceFile(*this, ReFunctionName + TEXT(".dsf"), DecompiledSource, ReSourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(ReSourceFilePath);

	FString ReMessage;
	if (!TestTrue(
		FString::Printf(TEXT("Decompiled StaticBool function re-generates: %s"), *ReMessage),
		FMaterialGenerator::GenerateAssetsFromFile(ReSourceFilePath, ReMessage, true)))
	{
		return false;
	}

	// The default has to survive as a node on the Preview pin, not just as PreviewValue.
	UMaterialFunction* RegeneratedFunction = LoadObject<UMaterialFunction>(nullptr, *ReObjectPath);
	if (!TestNotNull(TEXT("Regenerated StaticBool function loads"), RegeneratedFunction))
	{
		return false;
	}

	bool bFoundStaticBoolInput = false;
	for (auto&& ExpressionPtr : RegeneratedFunction->GetExpressions())
	{
		UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(ExpressionPtr);
		if (!FunctionInput || FunctionInput->InputName != FName(TEXT("UseDimmed")))
		{
			continue;
		}

		bFoundStaticBoolInput = true;
		TestEqual(TEXT("Regenerated input is a StaticBool function input."), static_cast<int32>(FunctionInput->InputType.GetValue()), static_cast<int32>(FunctionInput_StaticBool));
		TestTrue(TEXT("Regenerated StaticBool input keeps its default."), FunctionInput->bUsePreviewValueAsDefault != 0);
		TestNotNull(TEXT("Regenerated StaticBool default is a node on the Preview pin."), FunctionInput->Preview.GetTracedInput().Expression);
	}
	TestTrue(TEXT("Regenerated function exposes the UseDimmed input."), bFoundStaticBoolInput);

	return true;
}

namespace UE::DreamShader::Editor::Private::Tests
{
	template <typename TExpressionClass>
	int32 CountMaterialExpressionsOfClass(UMaterial* Material)
	{
		int32 Count = 0;
		if (Material)
		{
			for (auto&& ExpressionPtr : Material->GetExpressions())
			{
				if (Cast<TExpressionClass>(ExpressionPtr))
				{
					++Count;
				}
			}
		}
		return Count;
	}

	// Count by exact class name -- lets a data-driven test assert across many parameter node types
	// without pulling in a header per UMaterialExpression* subclass.
	int32 CountMaterialExpressionsOfClassName(UMaterial* Material, const TCHAR* ClassName)
	{
		int32 Count = 0;
		if (Material && ClassName)
		{
			for (auto&& ExpressionPtr : Material->GetExpressions())
			{
				if (ExpressionPtr && ExpressionPtr->GetClass()->GetName() == ClassName)
				{
					++Count;
				}
			}
		}
		return Count;
	}

	// True when the named input pin of the first node of ClassName is connected to another expression.
	bool IsNamedInputConnected(UMaterial* Material, const TCHAR* ClassName, const TCHAR* InputName)
	{
		if (!Material || !ClassName || !InputName)
		{
			return false;
		}
		for (auto&& ExpressionPtr : Material->GetExpressions())
		{
			UMaterialExpression* Expression = ExpressionPtr;
			if (!Expression || Expression->GetClass()->GetName() != ClassName)
			{
				continue;
			}
			for (int32 InputIndex = 0; InputIndex < 32; ++InputIndex)
			{
				FExpressionInput* Input = Expression->GetInput(InputIndex);
				if (!Input)
				{
					break;
				}
				if (Expression->GetInputName(InputIndex).ToString().Equals(InputName, ESearchCase::IgnoreCase))
				{
					return Input->Expression != nullptr;
				}
			}
		}
		return false;
	}

	bool GenerateAndLoadMaterial(
		FAutomationTestBase& Test,
		FScopedDreamShaderAutomationArtifacts& Artifacts,
		const FString& AssetName,
		const FString& Source,
		UMaterial*& OutMaterial)
	{
		// This helper loads the result as a UMaterial, which only the Graph backend produces. Pin it so
		// graph-shape tests do not depend on a source's domain/content happening to be inexpressible in
		// the Instance backend (which would route it to a UMaterialInstance and return null here).
		FScopedDreamShaderGraphBackendPin GraphPin;

		const FString ObjectPath = MakeAutomationObjectPath(AssetName);
		Artifacts.AddObjectPath(ObjectPath);
		AddExpectedNewAssetProbeWarnings(Test, ObjectPath);
		AddExpectedAutomationCleanupWarnings(Test);

		FString SourceFilePath;
		if (!WriteAutomationSourceFile(Test, AssetName + TEXT(".dsm"), Source, SourceFilePath))
		{
			return false;
		}
		Artifacts.AddSourceFile(SourceFilePath);

		FString Message;
		if (!Test.TestTrue(
			FString::Printf(TEXT("Material generation succeeds: %s"), *Message),
			UE::DreamShader::Editor::FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
		{
			return false;
		}

		OutMaterial = LoadObject<UMaterial>(nullptr, *ObjectPath);
		return Test.TestNotNull(TEXT("Generated material loads"), OutMaterial);
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderArithmeticNodeTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Gen.Graph.ArithmeticNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderArithmeticNodeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_Arith"));
	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        vec3 Tint = vec3(0.8, 0.4, 0.2);
        float A = 0.5;
        float B = 0.25;
    }

    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float Sum = A + B;
        Color = Tint * Sum;
    }
}
)"), *AssetName);

	UMaterial* Material = nullptr;
	if (!GenerateAndLoadMaterial(*this, Artifacts, AssetName, Source, Material))
	{
		return false;
	}

	TestTrue(TEXT("'+' generates an Add node"), CountMaterialExpressionsOfClass<UMaterialExpressionAdd>(Material) >= 1);
	TestTrue(TEXT("'*' generates a Multiply node"), CountMaterialExpressionsOfClass<UMaterialExpressionMultiply>(Material) >= 1);
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMathBuiltinNodeTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Gen.Graph.MathBuiltinNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderMathBuiltinNodeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_Math"));
	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        float Phase = 0.5;
    }

    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float Wave = sin(Phase);
        Color = vec3(Wave, Wave, Wave);
    }
}
)"), *AssetName);

	UMaterial* Material = nullptr;
	if (!GenerateAndLoadMaterial(*this, Artifacts, AssetName, Source, Material))
	{
		return false;
	}

	TestTrue(TEXT("'sin' generates a Sine node"), CountMaterialExpressionsOfClass<UMaterialExpressionSine>(Material) >= 1);
	return true;
}

// Generation half of the parameter-expression coverage (the parse half lives in
// DreamShaderParameterTests.cpp). Declares parameters with AND without inline defaults, references
// them in the graph, and asserts the matching UMaterialExpression*Parameter nodes are created --
// pinning both "the parameter generates its node" and the "default is optional" contract end to end.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderParameterNodeGenerationTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Gen.Parameters.NodeCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderParameterNodeGenerationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	// These assertions describe graph-backend node shapes; pin the backend against reroutes.
	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_Params"));
	// Scal and Vec are declared WITHOUT `= value` on purpose (optional-default contract); Dyn carries
	// an inline default. All three are referenced so lazy materialization actually creates the nodes.
	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        ScalarParameter Scal [Group="Gen"; SortPriority=10;];
        VectorParameter Vec [Group="Gen"; SortPriority=20;];
        DynamicParameter Dyn = float4(0.1, 0.2, 0.3, 1.0) [Group="Gen"; SortPriority=30;];
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Vec.rgb * Scal + Dyn.rgb;
    }
}
)"), *AssetName);

	UMaterial* Material = nullptr;
	if (!GenerateAndLoadMaterial(*this, Artifacts, AssetName, Source, Material))
	{
		return false;
	}

	TestEqual(TEXT("ScalarParameter declared without a default generates exactly one node"),
		CountMaterialExpressionsOfClass<UMaterialExpressionScalarParameter>(Material), 1);
	TestEqual(TEXT("VectorParameter declared without a default generates exactly one node"),
		CountMaterialExpressionsOfClass<UMaterialExpressionVectorParameter>(Material), 1);
	TestEqual(TEXT("DynamicParameter generates exactly one node"),
		CountMaterialExpressionsOfClass<UMaterialExpressionDynamicParameter>(Material), 1);
	return true;
}

// Generation coverage for the "miscellaneous" parameter node types beyond Scalar/Vector/Dynamic.
// Each type is declared (with or without an inline default) and referenced so lazy materialization
// creates its node; the test asserts the matching UMaterialExpression*Parameter class is generated.
// This pins the parameter-cluster fixes (CurveAtlasRow scalar-default no longer aborts; the
// TextureSample family seeds a default texture) and guards the rest from regression.
// Texture-OBJECT and asset-required types (TextureObject/Collection/SparseVolume*, RVT) are out of
// scope here: they output a texture object or need a bound asset to be usable, which the
// node-creation axis cannot exercise cleanly -- they are tracked as follow-ups in the findings doc.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderOtherParameterNodeGenerationTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Gen.Parameters.OtherNodeCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderOtherParameterNodeGenerationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	// These assertions describe graph-backend node shapes; pin the backend against reroutes.
	FScopedDreamShaderGraphBackendPin BackendPin;

	struct FOtherParameterCase
	{
		const TCHAR* NodeType;        // DSL keyword
		const TCHAR* Default;         // inline default literal, or nullptr to declare without one
		const TCHAR* RefExpr;         // graph expression that references the parameter and yields a vec3
		const TCHAR* ExpectedClass;   // expected UMaterialExpression subclass name
	};

	static const FOtherParameterCase Cases[] = {
		{ TEXT("DoubleVectorParameter"),        TEXT("float4(1, 2, 3, 4)"),    TEXT("P.rgb"),   TEXT("MaterialExpressionDoubleVectorParameter") },
		{ TEXT("CurveAtlasRowParameter"),       TEXT("float3(0.5, 0.5, 0.5)"), TEXT("P"),       TEXT("MaterialExpressionCurveAtlasRowParameter") },
		{ TEXT("ChannelMaskParameter"),         nullptr,                       TEXT("vec3(P)"), TEXT("MaterialExpressionChannelMaskParameter") },
		{ TEXT("StaticComponentMaskParameter"), TEXT("float4(1, 1, 0, 0)"),    TEXT("P.rgb"),   TEXT("MaterialExpressionStaticComponentMaskParameter") },
		{ TEXT("TextureSampleParameter2D"),     nullptr,                       TEXT("P.rgb"),   TEXT("MaterialExpressionTextureSampleParameter2D") },
		{ TEXT("FontSampleParameter"),          nullptr,                       TEXT("P.rgb"),   TEXT("MaterialExpressionFontSampleParameter") },
	};

	FScopedDreamShaderAutomationArtifacts Artifacts;
	for (const FOtherParameterCase& Case : Cases)
	{
		const FString AssetName = MakeUniqueTestAssetName(TEXT("M_OtherParam"));
		const FString DefaultClause = Case.Default ? FString::Printf(TEXT(" = %s"), Case.Default) : FString();
		const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = { %s P%s [Group="Gen";]; }
    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = { Color = %s; }
}
)"), *AssetName, Case.NodeType, *DefaultClause, Case.RefExpr);

		UMaterial* Material = nullptr;
		if (!GenerateAndLoadMaterial(*this, Artifacts, AssetName, Source, Material))
		{
			// GenerateAndLoadMaterial already recorded the failure; keep going so every type is covered.
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s generates exactly one %s node"), Case.NodeType, Case.ExpectedClass),
			CountMaterialExpressionsOfClassName(Material, Case.ExpectedClass),
			1);
	}

	return true;
}

// Parameter input-wiring call form: a parameter that owns input pins can be configured from the Graph
// with Param(InputPin = expr). Generation only succeeds if each named argument resolves to a real
// input pin and is wired, so this both exercises and pins that path (ChannelMask Input + texture
// sample Coordinates), asserting the pins end up connected.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderParameterInputWiringTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Gen.Parameters.InputWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderParameterInputWiringTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	// These assertions describe graph-backend node shapes; pin the backend against reroutes.
	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_ParamInputs"));
	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        VectorParameter Col = float4(0.8, 0.4, 0.2, 1.0) [Group="T";];
        ChannelMaskParameter Msk [Group="T";];
        TextureSampleParameterCube TexCube [Group="T";];
    }
    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = {
        float3 dir = float3(0.0, 0.0, 1.0);
        Color = vec3(Msk(Input = Col)) + TexCube(Coordinates = dir).rgb;
    }
}
)"), *AssetName);

	UMaterial* Material = nullptr;
	if (!GenerateAndLoadMaterial(*this, Artifacts, AssetName, Source, Material))
	{
		return false;
	}

	TestTrue(TEXT("ChannelMaskParameter Input pin is wired by Msk(Input=...)"),
		IsNamedInputConnected(Material, TEXT("MaterialExpressionChannelMaskParameter"), TEXT("Input")));
	TestTrue(TEXT("TextureSampleParameterCube Coordinates pin is wired by TexCube(Coordinates=...)"),
		IsNamedInputConnected(Material, TEXT("MaterialExpressionTextureSampleParameterCube"), TEXT("Coordinates")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateInstanceBackendTest,
	"DreamShader.Compiler.Generate.InstanceAlias",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 6 (the flip): Backend="Instance" is a deprecation-window ALIAS for ThinCustom. The legacy
// graphless-host generator is unreachable; an explicit Instance source now generates the thin chain
// (hidden base carrying the real graph + thin instance with an empty Instance-backend model), and
// parameters/settings resolve natively through the base.
bool FDreamShaderGenerateInstanceBackendTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoInstanceAlias"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        ScalarParameter Boost = 0.5;
        VectorParameter Tint = float4(1.0, 0.5, 0.25, 1.0);
        const float K = 2.0;
    }

    Settings = {
        Backend = "Instance";
        ShadingModel = "Unlit";
        BlendMode = "Opaque";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint.rgb * Boost * K;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("Instance-alias generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Instance-alias material exists in memory."), Instance))
	{
		return false;
	}

	// The thin chain, not the legacy graphless host: empty Instance-backend model, parented to the
	// per-material hidden base.
	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to a real base UMaterial."), Base))
	{
		TestTrue(TEXT("Parent is the ThinCustom hidden base."), Base->GetName().StartsWith(TEXT("MB_DreamThinBase_")));
		TestEqual(TEXT("Blend mode lands on the base."), Base->BlendMode, BLEND_Opaque);
		TestTrue(TEXT("Shading model lands on the base."), Base->GetShadingModels().HasShadingModel(MSM_Unlit));
	}

	float BoostValue = 0.0f;
	TestTrue(TEXT("Scalar parameter resolves natively through the chain."),
		Instance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Boost")), BoostValue) && FMath::IsNearlyEqual(BoostValue, 0.5f));
	FLinearColor TintValue = FLinearColor::Black;
	TestTrue(TEXT("Vector parameter resolves natively through the chain."),
		Instance->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), TintValue));
	float KValue = 0.0f;
	TestFalse(TEXT("Const property does not become a parameter."),
		Instance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("K")), KValue));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateThinCustomBackendTest,
	"DreamShader.Compiler.Generate.ThinCustomBackend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderGenerateThinCustomBackendTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoThinCustom"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	// Persist-mode generation (bTransient defaults false below) hides the base as a subobject INSIDE the
	// instance's package -- there is no separate sibling asset. BaseSiblingObjectPath is the path such a
	// sibling WOULD have had before convergence; the test asserts below that it never reaches disk.
	const FString BaseSiblingObjectPath = MakeAutomationObjectPath(FString::Printf(TEXT("MB_DreamThinBase_%s"), *AssetName));
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        ScalarParameter Boost = 0.5;
        VectorParameter Tint = float4(1.0, 0.5, 0.25, 1.0);
    }

    Settings = {
        Backend = "ThinCustom";
        ShadingModel = "Unlit";
        BlendMode = "Opaque";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint.rgb * Boost;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true);
	if (!TestTrue(FString::Printf(TEXT("ThinCustom material generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = LoadObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(FString::Printf(TEXT("Generated ThinCustom instance loads from '%s'."), *ObjectPath), Instance))
	{
		return false;
	}

	// The instance is a THIN material instance of a real base UMaterial: its own Instance-backend
	// model stays empty (no per-property Custom injection, no synthesized parameter data), so the
	// engine compiles and enumerates the base's real graph natively. This is what distinguishes the
	// convergence path from the Instance backend.
	TestTrue(TEXT("Instance forces a static permutation (root of its own shader map)."), Instance->HasOverridenBaseProperties());

	UMaterial* Base = Cast<UMaterial>(Instance->Parent);
	if (TestNotNull(TEXT("Instance is parented to a real base UMaterial."), Base))
	{
		TestTrue(TEXT("Parent is the ThinCustom hidden base."), Base->GetName().StartsWith(TEXT("MB_DreamThinBase_")));
		TestTrue(TEXT("The base carries a real material graph (Multiply from the Graph block)."),
			CountMaterialExpressionsOfClass<UMaterialExpressionMultiply>(Base) >= 1);

		// Convergence: the base is a hidden subobject of the instance -- it shares the instance's
		// package (so it cooks and loads with it, with no cross-package parent import to lose), is not
		// an independently browsable asset, and leaves NO separate MB_ sibling behind on disk.
		TestEqual(TEXT("Base is a subobject of the instance."),
			Base->GetOuter(), static_cast<UObject*>(Instance));
		TestEqual(TEXT("Base shares the instance's package."),
			Base->GetOutermost(), Instance->GetOutermost());
		TestFalse(TEXT("Base is not an independently browsable asset."), Base->IsAsset());
		TestTrue(TEXT("Instance package reached disk."),
			FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ObjectPath)));
		TestFalse(TEXT("No separate base sibling asset was written to disk."),
			FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(BaseSiblingObjectPath)));
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderThinCustomVsGraphParityTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Render.ThinCustomVsGraphParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage-1 acceptance gate for the ThinCustom convergence backend: the SAME shader body generated
// through the Graph backend (a visible UMaterial) and through ThinCustom (hidden base + thin
// instance, rendering through the instance's own static-permutation shader map) must produce
// pixel-identical frames on the real RHI. Anything short of that -- a broken fall-through in the
// instance resource, a parameter that failed to bind by name, a shading-model/blend-mode mismatch on
// the base -- shows up as a pixel difference here.
bool FDreamShaderThinCustomVsGraphParityTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (GUsingNullRHI || !FApp::CanEverRender())
	{
		AddInfo(TEXT("Skipping ThinCustom-vs-Graph render parity: no usable RHI (-nullrhi). Run without -nullrhi for the full pixel comparison."));
		return true;
	}

	FScopedDreamShaderAutomationArtifacts Artifacts;
	AddExpectedAutomationCleanupWarnings(*this);

	// One shared body per case, generated twice: only the Backend setting differs between the twins.
	// FlatParams exercises by-name scalar/vector parameter binding on an Unlit surface; UvTexture adds
	// UE.TexCoord + a texture-object parameter + SampleTexture2D so interpolator allocation and the
	// texture-sampling path must also match (Stage 2); LitAttributes routes the whole surface through
	// Base.MaterialAttributes on a DefaultLit material so the lit pipeline and the
	// MakeMaterialAttributes path must also match (Stage 5).
	struct FParityCase
	{
		const TCHAR* CaseName;
		const TCHAR* PropertiesBlock;
		const TCHAR* SettingsBlock;
		const TCHAR* OutputsBlock;
		const TCHAR* GraphBlock;
	};
	const FParityCase ParityCases[] =
	{
		{
			TEXT("FlatParams"),
			TEXT("        ScalarParameter Boost = 0.75;\n        VectorParameter Tint = float4(0.1, 0.9, 0.35, 1.0);"),
			TEXT("        ShadingModel = \"Unlit\";\n        BlendMode = \"Opaque\";"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;"),
			TEXT("        Color = Tint.rgb * Boost;")
		},
		{
			TEXT("UvTexture"),
			TEXT("        TextureObjectParameter BaseMap = \"/Engine/EngineResources/DefaultTexture\";\n        ScalarParameter Boost = 1.0;"),
			TEXT("        ShadingModel = \"Unlit\";\n        BlendMode = \"Opaque\";"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;"),
			TEXT("        float2 uv = UE.TexCoord(Index=0);\n        float4 texel = SampleTexture2D(BaseMap, uv);\n        Color = vec3(0.08 * (uv.x + texel.r), 0.55 + 0.35 * uv.y, 0.15) * Boost;")
		},
		{
			TEXT("LitAttributes"),
			TEXT("        VectorParameter Tint = float4(0.15, 0.8, 0.3, 1.0);\n        ScalarParameter Rough = 0.4;"),
			TEXT("        ShadingModel = \"DefaultLit\";\n        BlendMode = \"Opaque\";"),
			TEXT("        MaterialAttributes Attrs;\n        Base.MaterialAttributes = Attrs;"),
			TEXT("        Attrs.BaseColor = Tint.rgb;\n        Attrs.Roughness = Rough;")
		},
	};

	const auto MakeParitySource = [](const FString& AssetName, const TCHAR* Backend, const FParityCase& Case)
	{
		return FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
%s
    }

    Settings = {
        Backend = "%s";
%s
    }

    Outputs = {
%s
    }

    Graph = {
%s
    }
}
)"), *AssetName, Case.PropertiesBlock, Backend, Case.SettingsBlock, Case.OutputsBlock, Case.GraphBlock);
	};

	const int32 RenderSize = 256;
	const TCHAR* RenderMesh = TEXT("sphere");
	const float RenderYaw = -157.5f;
	const float RenderPitch = -11.25f;
	UE::DreamShader::Editor::Private::FDreamShaderPreviewRenderContext RenderContext;

	for (const FParityCase& Case : ParityCases)
	{
		UMaterialInterface* Twins[2] = { nullptr, nullptr };
		const TCHAR* TwinBackends[2] = { TEXT("Graph"), TEXT("ThinCustom") };
		const FString TwinAssetNames[2] =
		{
			MakeUniqueTestAssetName(*FString::Printf(TEXT("M_Parity%sGraph"), Case.CaseName)),
			MakeUniqueTestAssetName(*FString::Printf(TEXT("M_Parity%sThin"), Case.CaseName)),
		};

		for (int32 TwinIndex = 0; TwinIndex < 2; ++TwinIndex)
		{
			const FString& AssetName = TwinAssetNames[TwinIndex];
			const FString ObjectPath = MakeAutomationObjectPath(AssetName);
			Artifacts.AddObjectPath(ObjectPath);
			AddExpectedNewAssetProbeWarnings(*this, ObjectPath);

			FString SourceFilePath;
			if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), MakeParitySource(AssetName, TwinBackends[TwinIndex], Case), SourceFilePath))
			{
				return false;
			}
			Artifacts.AddSourceFile(SourceFilePath);

			FString Message;
			if (!TestTrue(
				FString::Printf(TEXT("[%s] %s twin generation succeeds: %s"), Case.CaseName, TwinBackends[TwinIndex], *Message),
				FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true)))
			{
				return false;
			}

			Twins[TwinIndex] = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
			if (!TestNotNull(FString::Printf(TEXT("[%s] %s twin loads from '%s'."), Case.CaseName, TwinBackends[TwinIndex], *ObjectPath), Twins[TwinIndex]))
			{
				return false;
			}
		}

		// The ThinCustom twin must actually be the thin instance chain -- otherwise the comparison
		// would silently degrade into Graph-vs-Graph.
		UDreamShaderMaterialInstance* ThinInstance = Cast<UDreamShaderMaterialInstance>(Twins[1]);
		if (!TestNotNull(FString::Printf(TEXT("[%s] ThinCustom twin is a UDreamShaderMaterialInstance."), Case.CaseName), ThinInstance))
		{
			return false;
		}
		TestNotNull(FString::Printf(TEXT("[%s] ThinCustom twin is parented to its hidden base."), Case.CaseName), Cast<UMaterial>(ThinInstance->Parent.Get()));

		// Both shader maps must be fully compiled before pixels mean anything.
		const auto FinishTwinCompilation = [&Twins]()
		{
			TArray<UObject*> CompileObjects;
			CompileObjects.Add(Twins[0]);
			CompileObjects.Add(Twins[1]);
			FAssetCompilingManager::Get().FinishCompilationForObjects(CompileObjects);
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			FlushRenderingCommands();
		};
		FinishTwinCompilation();

		// Render both twins through the identical offscreen path. A throwaway warm-up render per twin
		// first: material resources/shader maps are allocated lazily on first render, and the finish-
		// compile after the warm-up drains whatever that first render queued, so the measured frame
		// renders the real shader instead of the default-material fallback.
		TArray<FColor> TwinPixels[2];
		for (int32 TwinIndex = 0; TwinIndex < 2; ++TwinIndex)
		{
			FString RenderError;
			TArray<FColor> WarmupPixels;
			if (!TestTrue(
				FString::Printf(TEXT("[%s] %s twin warm-up render succeeds: %s"), Case.CaseName, TwinBackends[TwinIndex], *RenderError),
				RenderContext.RenderFramePixels(Twins[TwinIndex], RenderSize, RenderSize, RenderMesh, RenderYaw, RenderPitch, WarmupPixels, RenderError)))
			{
				return false;
			}

			FinishTwinCompilation();

			if (!TestTrue(
				FString::Printf(TEXT("[%s] %s twin render succeeds: %s"), Case.CaseName, TwinBackends[TwinIndex], *RenderError),
				RenderContext.RenderFramePixels(Twins[TwinIndex], RenderSize, RenderSize, RenderMesh, RenderYaw, RenderPitch, TwinPixels[TwinIndex], RenderError)))
			{
				return false;
			}
		}

		if (!TestEqual(FString::Printf(TEXT("[%s] Both twins produced the same pixel count."), Case.CaseName), TwinPixels[1].Num(), TwinPixels[0].Num()))
		{
			return false;
		}

		// Sanity before parity: the frame must actually contain our green-dominant emissive. This is
		// what separates "both twins render the same broken default-material checkerboard" (would pass
		// a naive diff) from "both twins render the intended shader".
		{
			int32 GreenDominantPixels = 0;
			uint64 SumR = 0, SumG = 0, SumB = 0;
			for (const FColor& Pixel : TwinPixels[0])
			{
				SumR += Pixel.R;
				SumG += Pixel.G;
				SumB += Pixel.B;
				if (Pixel.G > Pixel.R + 30 && Pixel.G > Pixel.B + 30)
				{
					++GreenDominantPixels;
				}
			}
			const int32 PixelCount = FMath::Max(TwinPixels[0].Num(), 1);
			const FColor CenterPixel = TwinPixels[0][(RenderSize / 2) * RenderSize + (RenderSize / 2)];
			AddInfo(FString::Printf(
				TEXT("[%s] Graph twin frame stats: avg RGB=(%d,%d,%d), center=(%d,%d,%d), %d green-dominant pixels."),
				Case.CaseName,
				int32(SumR / PixelCount), int32(SumG / PixelCount), int32(SumB / PixelCount),
				CenterPixel.R, CenterPixel.G, CenterPixel.B,
				GreenDominantPixels));

			const bool bShowsEmissive = GreenDominantPixels > (TwinPixels[0].Num() / 20);
			if (!bShowsEmissive)
			{
				// Dump both frames for offline inspection before failing -- a bit-identical pair that
				// does not show the material means BOTH chains rendered the same wrong thing.
				for (int32 TwinIndex = 0; TwinIndex < 2; ++TwinIndex)
				{
					TArray<FColor> PngColors = TwinPixels[TwinIndex];
					for (FColor& Color : PngColors)
					{
						Color.A = 255;
					}
					TArray64<uint8> PngData;
					FImageUtils::PNGCompressImageArray(RenderSize, RenderSize, TArrayView64<const FColor>(PngColors.GetData(), PngColors.Num()), PngData);
					const FString DumpPath = FPaths::ProjectSavedDir() / TEXT("DreamShaderTests") / FString::Printf(TEXT("Parity_%s_%s.png"), Case.CaseName, TwinBackends[TwinIndex]);
					FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PngData.GetData(), PngData.Num()), *DumpPath);
					AddInfo(FString::Printf(TEXT("Dumped %s twin frame to '%s'."), TwinBackends[TwinIndex], *DumpPath));
				}
			}
			TestTrue(
				FString::Printf(TEXT("[%s] Graph twin frame shows the green emissive sphere (%d green-dominant pixels)."), Case.CaseName, GreenDominantPixels),
				bShowsEmissive);
		}

		// Pixel parity with a small tolerance: the two shader maps are equivalent but separately
		// translated/compiled, so allow LSB-level float/rounding differences while failing on anything
		// a human could see.
		{
			constexpr int32 ChannelTolerance = 2;
			int32 OffendingPixels = 0;
			int32 MaxChannelDifference = 0;
			for (int32 PixelIndex = 0; PixelIndex < TwinPixels[0].Num(); ++PixelIndex)
			{
				const FColor& A = TwinPixels[0][PixelIndex];
				const FColor& B = TwinPixels[1][PixelIndex];
				const int32 PixelDifference = FMath::Max3(
					FMath::Abs(int32(A.R) - int32(B.R)),
					FMath::Abs(int32(A.G) - int32(B.G)),
					FMath::Abs(int32(A.B) - int32(B.B)));
				MaxChannelDifference = FMath::Max(MaxChannelDifference, PixelDifference);
				if (PixelDifference > ChannelTolerance)
				{
					++OffendingPixels;
				}
			}

			const int32 AllowedOffenders = TwinPixels[0].Num() / 1000; // 0.1%
			TestTrue(
				FString::Printf(
					TEXT("[%s] ThinCustom renders identically to its Graph twin (%d of %d pixels beyond tolerance %d, max channel difference %d)."),
					Case.CaseName, OffendingPixels, TwinPixels[0].Num(), ChannelTolerance, MaxChannelDifference),
				OffendingPixels <= AllowedOffenders);
			AddInfo(FString::Printf(
				TEXT("[%s] ThinCustom-vs-Graph parity: %d/%d pixels beyond tolerance %d, max channel difference %d."),
				Case.CaseName, OffendingPixels, TwinPixels[0].Num(), ChannelTolerance, MaxChannelDifference));
		}
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderMTestToonRoundTripRenderTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Roundtrip.MTestToonRenderParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// End-to-end round-trip fidelity gate for the real hand-authored Substrate toon material M_Test_Toon:
// decompile it to a .dsm, regenerate a fresh twin from that source, and require the twin to render
// pixel-identically to the original through the same offscreen thumbnail path as the ThinCustom-vs-Graph
// parity test. This exercises both the ShadingModel="Substrate" fix and the renamed-channel swizzle fix
// on a genuinely complex asset. Self-contained: only depends on M_Test_Toon existing in this project;
// skips cleanly under -nullrhi or where the asset is absent (plugin dropped into another project).
bool FDreamShaderMTestToonRoundTripRenderTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	if (GUsingNullRHI || !FApp::CanEverRender())
	{
		AddInfo(TEXT("Skipping M_Test_Toon round-trip render parity: no usable RHI (run without -nullrhi)."));
		return true;
	}

	// "阿芙" via code points so this source file stays ASCII-clean.
	const FString CjkFolder = FString::Chr(TCHAR(0x963F)) + FString::Chr(TCHAR(0x8299));
	const FString OriginalPath = FString::Printf(TEXT("/Game/Render/Character/%s/NewToonMaterials/M_Test_Toon.M_Test_Toon"), *CjkFolder);
	UMaterial* OriginalMaterial = LoadObject<UMaterial>(nullptr, *OriginalPath);
	if (!OriginalMaterial)
	{
		AddInfo(FString::Printf(TEXT("Skipping M_Test_Toon round-trip: '%s' is not present in this project."), *OriginalPath));
		return true;
	}

	FScopedDreamShaderAutomationArtifacts Artifacts;
	AddExpectedAutomationCleanupWarnings(*this);

	// Decompile the real material, then regenerate a transient twin from the emitted .dsm -- the true
	// round trip. If the renamed-channel swizzle regressed, the ".m" suffix would re-appear and this
	// regeneration would fail with an invalid-swizzle error, so generation success is itself a guard.
	const FString ReAssetName = MakeUniqueTestAssetName(TEXT("M_MTestToonRoundTrip"));
	const FString ReObjectPath = MakeAutomationObjectPath(ReAssetName);
	Artifacts.AddObjectPath(ReObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ReObjectPath);

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("M_Test_Toon decompiles: %s"), *DecompileError),
		GetGraphDecompiler().DecompileMaterial(OriginalMaterial, FString::Printf(TEXT("DreamShaderTests/Automation/%s"), *ReAssetName), DecompiledSource, DecompileError)))
	{
		return false;
	}

	// Explicit regression assertions for the two decompiler fixes under test.
	TestTrue(TEXT("Decompiled M_Test_Toon binds Base.FrontMaterial with ShadingModel=\"Substrate\"."),
		DecompiledSource.Contains(TEXT("ShadingModel = \"Substrate\"")));
	TestFalse(TEXT("Decompiled M_Test_Toon emits no invalid renamed-channel swizzle (\".m\")."),
		DecompiledSource.Contains(TEXT(".m,")) || DecompiledSource.Contains(TEXT(".m ")) || DecompiledSource.Contains(TEXT(".m)")));

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, ReAssetName + TEXT(".dsm"), DecompiledSource, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("Decompiled M_Test_Toon re-generates (0 errors): %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true)))
	{
		return false;
	}

	UMaterialInterface* Materials[2] = { nullptr, nullptr };
	const TCHAR* Labels[2] = { TEXT("Original"), TEXT("RoundTripTwin") };
	Materials[0] = OriginalMaterial;
	Materials[1] = LoadObject<UMaterialInterface>(nullptr, *ReObjectPath);
	if (!TestNotNull(FString::Printf(TEXT("Round-trip twin loads from '%s'."), *ReObjectPath), Materials[1]))
	{
		return false;
	}

	const int32 RenderSize = 256;
	const TCHAR* RenderMesh = TEXT("sphere");
	const float RenderYaw = -157.5f;
	const float RenderPitch = -11.25f;
	UE::DreamShader::Editor::Private::FDreamShaderPreviewRenderContext RenderContext;

	const auto FinishCompile = [&Materials]()
	{
		TArray<UObject*> Objects;
		Objects.Add(Materials[0]);
		Objects.Add(Materials[1]);
		FAssetCompilingManager::Get().FinishCompilationForObjects(Objects);
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		FlushRenderingCommands();
	};
	FinishCompile();

	TArray<FColor> Pixels[2];
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FString RenderError;
		TArray<FColor> Warmup;
		if (!TestTrue(FString::Printf(TEXT("%s warm-up render succeeds: %s"), Labels[Index], *RenderError),
			RenderContext.RenderFramePixels(Materials[Index], RenderSize, RenderSize, RenderMesh, RenderYaw, RenderPitch, Warmup, RenderError)))
		{
			return false;
		}
		FinishCompile();
		if (!TestTrue(FString::Printf(TEXT("%s render succeeds: %s"), Labels[Index], *RenderError),
			RenderContext.RenderFramePixels(Materials[Index], RenderSize, RenderSize, RenderMesh, RenderYaw, RenderPitch, Pixels[Index], RenderError)))
		{
			return false;
		}
	}

	// Always dump both frames for offline / screenshot inspection.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		TArray<FColor> PngColors = Pixels[Index];
		for (FColor& Color : PngColors)
		{
			Color.A = 255;
		}
		TArray64<uint8> PngData;
		FImageUtils::PNGCompressImageArray(RenderSize, RenderSize, TArrayView64<const FColor>(PngColors.GetData(), PngColors.Num()), PngData);
		const FString DumpPath = FPaths::ProjectSavedDir() / TEXT("DreamShaderTests") / FString::Printf(TEXT("MTestToon_%s.png"), Labels[Index]);
		FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PngData.GetData(), PngData.Num()), *DumpPath);
		AddInfo(FString::Printf(TEXT("Dumped %s frame to '%s'."), Labels[Index], *DumpPath));
	}

	if (!TestEqual(TEXT("Both frames share a pixel count."), Pixels[1].Num(), Pixels[0].Num()))
	{
		return false;
	}

	// Sentinel: the frame must actually show the material sphere, not just the (44,44,48) clear color --
	// otherwise a pair of empty frames would sail through a naive diff (as a -run= commandlet does).
	{
		const FColor ClearColor(44, 44, 48);
		int32 ForegroundPixels = 0;
		for (const FColor& Pixel : Pixels[0])
		{
			const int32 D = FMath::Max3(
				FMath::Abs(int32(Pixel.R) - int32(ClearColor.R)),
				FMath::Abs(int32(Pixel.G) - int32(ClearColor.G)),
				FMath::Abs(int32(Pixel.B) - int32(ClearColor.B)));
			if (D > 8)
			{
				++ForegroundPixels;
			}
		}
		AddInfo(FString::Printf(TEXT("Original frame foreground pixels (>8 from clear color): %d/%d."), ForegroundPixels, Pixels[0].Num()));
		if (!TestTrue(
			FString::Printf(TEXT("Original frame actually shows the material sphere (%d foreground pixels)."), ForegroundPixels),
			ForegroundPixels > Pixels[0].Num() / 20))
		{
			return false;
		}
	}

	// Round-trip parity: original vs regenerated twin. The two are separately translated/compiled shader
	// maps, so allow LSB-level tolerance while failing on anything a human could see.
	{
		constexpr int32 ChannelTolerance = 2;
		int32 OffendingPixels = 0;
		int32 MaxChannelDifference = 0;
		for (int32 PixelIndex = 0; PixelIndex < Pixels[0].Num(); ++PixelIndex)
		{
			const FColor& A = Pixels[0][PixelIndex];
			const FColor& B = Pixels[1][PixelIndex];
			const int32 PixelDifference = FMath::Max3(
				FMath::Abs(int32(A.R) - int32(B.R)),
				FMath::Abs(int32(A.G) - int32(B.G)),
				FMath::Abs(int32(A.B) - int32(B.B)));
			MaxChannelDifference = FMath::Max(MaxChannelDifference, PixelDifference);
			if (PixelDifference > ChannelTolerance)
			{
				++OffendingPixels;
			}
		}

		const int32 AllowedOffenders = Pixels[0].Num() / 1000; // 0.1%
		AddInfo(FString::Printf(
			TEXT("M_Test_Toon round-trip parity: %d/%d pixels beyond tolerance %d, max channel difference %d."),
			OffendingPixels, Pixels[0].Num(), ChannelTolerance, MaxChannelDifference));
		TestTrue(
			FString::Printf(
				TEXT("Round-trip twin renders identically to the original (%d of %d pixels beyond tolerance %d, max channel difference %d)."),
				OffendingPixels, Pixels[0].Num(), ChannelTolerance, MaxChannelDifference),
			OffendingPixels <= AllowedOffenders);
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderDecompileRenamedChannelUsesMaskTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Roundtrip.RenamedChannelUsesMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Regression guard for the renamed-channel swizzle fix. A texture sample lets the author RELABEL its
// per-channel outputs (ChannelNames), but channel selection at compile/render time is driven SOLELY by
// the mask bits (FExpressionInput::Compile applies the connected output's mask; ApplyChannelNames only
// rewrites the cosmetic name). So a green-masked output relabeled with the swizzle letter "R" must
// decompile to ".g" (the mask), NOT ".r" (the misleading label) -- otherwise the regenerated material
// silently samples the wrong channel of a packed MRA/ORM texture. The M_Test_Toon happy path can't catch
// this because its relabel ("M") is not a valid swizzle letter and already falls through to the mask.
bool FDreamShaderDecompileRenamedChannelUsesMaskTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Transient probe material created."), Material))
	{
		return false;
	}

	UMaterialExpressionTextureSampleParameter2D* Tex = Cast<UMaterialExpressionTextureSampleParameter2D>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass()));
	if (!TestNotNull(TEXT("Texture sample parameter created."), Tex))
	{
		return false;
	}
	Tex->ParameterName = TEXT("T_MaskProbe");
	Tex->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture"));

	// Relabel with swizzle letters that DISAGREE with the physical masks: the red-masked output becomes
	// "Rd" and the GREEN-masked output becomes "R". If the decompiler trusted the label it would emit
	// ".r" (red) for the green channel; the mask is authoritative, so it must emit ".g".
	Tex->ChannelNames.R = FText::FromString(TEXT("Rd"));
	Tex->ChannelNames.G = FText::FromString(TEXT("R"));
	Tex->ApplyChannelNames();

	// Connect emissive to the output now labelled "R" -- the green-masked one (output index 2).
	if (!TestTrue(TEXT("Connected the relabeled green output to EmissiveColor."),
		UMaterialEditingLibrary::ConnectMaterialProperty(Tex, TEXT("R"), MP_EmissiveColor)))
	{
		return false;
	}

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("Renamed-channel probe material decompiles: %s"), *DecompileError),
		GetGraphDecompiler().DecompileMaterial(Material, TEXT("DreamShaderTests/Automation/M_RenamedChannelProbe"), DecompiledSource, DecompileError)))
	{
		return false;
	}

	// The green mask is authoritative, so the emissive must read the GREEN channel regardless of the "R" label.
	TestTrue(TEXT("Relabeled green channel decompiles to the mask-derived '.g' swizzle."),
		DecompiledSource.Contains(TEXT("T_MaskProbe.g")));
	TestFalse(TEXT("Relabeled green channel does NOT decompile to the misleading '.r' label."),
		DecompiledSource.Contains(TEXT("T_MaskProbe.r")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateThinCustomTextureTest,
	"DreamShader.Compiler.Generate.ThinCustomTexture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 2: texture parameters and state-read builtins reach ThinCustom as REAL nodes through the
// shared Graph construction -- no .ush lowering, no instance-side texture index space, no dummy
// interpolator chunks. The engine enumerates the base's texture parameter natively, which is what
// makes the MI editor and the cook's used-texture gather work with zero instance machinery.
bool FDreamShaderGenerateThinCustomTextureTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoThinCustomTexture"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        TextureObjectParameter BaseMap = "/Engine/EngineResources/DefaultTexture";
        ScalarParameter Intensity = 1.0;
    }

    Settings = {
        Backend = "ThinCustom";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float2 uv = UE.TexCoord(Index=0);
        float4 texel = SampleTexture2D(BaseMap, uv);
        Color = texel.rgb * Intensity;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("ThinCustom texture generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("ThinCustom texture instance exists in memory."), Instance))
	{
		return false;
	}

	// The Instance-backend model stays empty: no lowered .ush, no per-instance texture index space,
	// no dummy TexCoord side-effect inputs.

	// Native parameter enumeration through the base's real nodes -- the engine, not synthesized
	// cached data, resolves these.
	UTexture* BaseMapValue = nullptr;
	TestTrue(TEXT("BaseMap texture parameter resolves natively through the chain."),
		Instance->GetTextureParameterValue(FMaterialParameterInfo(TEXT("BaseMap")), BaseMapValue) && BaseMapValue != nullptr);
	float IntensityValue = 0.0f;
	TestTrue(TEXT("Intensity scalar parameter resolves natively through the chain."),
		Instance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Intensity")), IntensityValue) && FMath::IsNearlyEqual(IntensityValue, 1.0f));

	// The base carries the real nodes: a texture-object parameter and a real TexCoord node (the
	// stock translator allocates the interpolator from the node -- no dummy-chunk ceiling).
	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to the hidden base."), Base))
	{
		TestTrue(TEXT("Base has a real texture-object-parameter node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionTextureObjectParameter")) >= 1);
		TestTrue(TEXT("Base has a real TextureCoordinate node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionTextureCoordinate")) >= 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateThinCustomUITest,
	"DreamShader.Compiler.Generate.ThinCustomUI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 3: the UI domain through ThinCustom. Unlike the Instance backend (which needs a dedicated
// graphless per-domain host asset), the shared Graph construction sets MaterialDomain directly on
// the per-material hidden base -- no host infrastructure at all.
bool FDreamShaderGenerateThinCustomUITest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoThinCustomUI"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        VectorParameter BaseTint = float4(0.15, 0.55, 0.95, 1.0);
        ScalarParameter Opacity = 1.0;
    }

    Settings = {
        Backend = "ThinCustom";
        Domain = "UI";
    }

    Outputs = {
        vec3 Color;
        float A;
        Base.EmissiveColor = Color;
        Base.Opacity = A;
    }

    Graph = {
        float2 uv = UE.TexCoord(Index=0);
        float grad = lerp(0.85, 1.0, uv.y);
        Color = BaseTint.rgb * grad;
        A = Opacity;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("ThinCustom UI generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("ThinCustom UI instance exists in memory."), Instance))
	{
		return false;
	}

	FLinearColor BaseTintValue = FLinearColor::Black;
	TestTrue(TEXT("BaseTint vector parameter resolves natively through the chain."),
		Instance->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BaseTint")), BaseTintValue));

	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to the hidden base."), Base))
	{
		TestEqual(TEXT("The base carries the UI material domain."), Base->MaterialDomain, MD_UI);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateThinCustomPostProcessTest,
	"DreamShader.Compiler.Generate.ThinCustomPostProcess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 3: the PostProcess domain through ThinCustom. The scene read becomes a REAL
// UMaterialExpressionSceneTexture node on the hidden base -- replacing the Instance backend's named
// value-input machinery (FDreamShaderSceneRead / SceneTextureLookup chunks) for this path.
bool FDreamShaderGenerateThinCustomPostProcessTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoThinCustomPP"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        ScalarParameter Saturation = 1.4;
    }

    Settings = {
        Backend = "ThinCustom";
        Domain = "PostProcess";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float3 scene = UE.SceneTexture(Id="PostProcessInput0").rgb;
        float luma = dot(scene, float3(0.299, 0.587, 0.114));
        Color = lerp(float3(luma, luma, luma), scene, Saturation);
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("ThinCustom PostProcess generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("ThinCustom PostProcess instance exists in memory."), Instance))
	{
		return false;
	}

	// No named value-input scene reads: the scene texture is a real node on the base, not a
	// translator-injected chunk on the instance.

	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to the hidden base."), Base))
	{
		TestEqual(TEXT("The base carries the PostProcess material domain."), Base->MaterialDomain, MD_PostProcess);
		TestTrue(TEXT("The base carries a real SceneTexture node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionSceneTexture")) >= 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateThinCustomSceneReadsTest,
	"DreamShader.Compiler.Generate.ThinCustomSceneReads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 4: translucent scene reads through ThinCustom. UE.SceneDepth/UE.SceneColor/UE.PixelDepth
// become REAL nodes on the hidden base -- replacing the Instance backend's named value-input
// machinery (FDreamShaderSceneRead + SceneDepth/SceneColor compiler chunks) for this path. The
// engine's own "only translucent materials can read scene color" validation stands in for the
// Instance backend's hand-written pre-flight gate.
bool FDreamShaderGenerateThinCustomSceneReadsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoThinCustomSceneReads"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        ScalarParameter FadeDistance = 64.0;
        VectorParameter TintColor = float4(0.3, 0.7, 1.0, 1.0);
    }

    Settings = {
        Backend = "ThinCustom";
        ShadingModel = "Unlit";
        BlendMode = "Translucent";
    }

    Outputs = {
        vec3 Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity = Alpha;
    }

    Graph = {
        float sceneDepth = UE.SceneDepth();
        float pixelDepth = UE.PixelDepth();
        float fade = saturate((sceneDepth - pixelDepth) / FadeDistance);

        float4 behind = UE.SceneColor();
        Color = lerp(behind.rgb, TintColor.rgb, 0.35);
        Alpha = fade;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("ThinCustom scene-reads generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("ThinCustom scene-reads instance exists in memory."), Instance))
	{
		return false;
	}

	// The named value-input machinery stays entirely unused on this path.

	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to the hidden base."), Base))
	{
		TestEqual(TEXT("The base carries the translucent blend mode."), Base->BlendMode, BLEND_Translucent);
		TestTrue(TEXT("The base carries a real SceneDepth node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionSceneDepth")) >= 1);
		TestTrue(TEXT("The base carries a real SceneColor node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionSceneColor")) >= 1);
		TestTrue(TEXT("The base carries a real PixelDepth node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionPixelDepth")) >= 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateThinCustomMaterialAttributesTest,
	"DreamShader.Compiler.Generate.ThinCustomMaterialAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 5: a whole-MaterialAttributes bind through ThinCustom. The shared Graph construction seeds a
// real MakeMaterialAttributes node and flips bUseMaterialAttributes on the hidden base -- replacing
// the Instance backend's per-channel flattening (the __Attrs_Field local rewrite) for this path.
bool FDreamShaderGenerateThinCustomMaterialAttributesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoThinCustomMatAttrs"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        VectorParameter Tint = float4(0.6, 0.8, 1.0, 1.0);
        ScalarParameter Rough = 0.35;
        ScalarParameter Metal = 0.0;
    }

    Settings = {
        Backend = "ThinCustom";
        ShadingModel = "DefaultLit";
        BlendMode = "Opaque";
    }

    Outputs = {
        MaterialAttributes Attrs;
        Base.MaterialAttributes = Attrs;
    }

    Graph = {
        Attrs.BaseColor = Tint.rgb;
        Attrs.Roughness = Rough;
        Attrs.Metallic = Metal;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("ThinCustom MaterialAttributes generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("ThinCustom MaterialAttributes instance exists in memory."), Instance))
	{
		return false;
	}


	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to the hidden base."), Base))
	{
		TestTrue(TEXT("The base routes the surface through MaterialAttributes."), Base->bUseMaterialAttributes);
		TestTrue(TEXT("The base carries a real MakeMaterialAttributes node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionMakeMaterialAttributes")) >= 1);
		TestTrue(TEXT("The base carries the DefaultLit shading model."),
			Base->GetShadingModels().HasShadingModel(MSM_DefaultLit));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateInstanceBackendStateReadsTest,
	"DreamShader.Compiler.Generate.InstanceAliasStateReads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 6: the no-arg state-read wave through the Instance alias. Every name the Instance .ush
// lowering accepted is registered in the node-graph evaluator's UE.* table, so the same source
// produces real nodes on the hidden base (semantics pinned against the DS_ macros).
bool FDreamShaderGenerateInstanceBackendStateReadsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoInstanceStateReads"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Settings = {
        Backend = "Instance";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        float3 wp = UE.WorldPosition();
        float3 op = UE.ObjectPosition();
        float3 refl = UE.ReflectionVector();
        float2 vp = UE.ScreenPosition();
        float rnd = UE.PerInstanceRandom();
        Color = frac(wp * 0.001) * 0.5 + refl * 0.25 + float3(vp * rnd, op.z * 0.0);
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("State-read alias generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("State-read alias material exists in memory."), Instance))
	{
		return false;
	}


	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to the hidden base."), Base))
	{
		TestTrue(TEXT("UE.WorldPosition is a real node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionWorldPosition")) >= 1);
		TestTrue(TEXT("UE.ObjectPosition is a real node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionObjectPositionWS")) >= 1);
		TestTrue(TEXT("UE.ReflectionVector is a real node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionReflectionVectorWS")) >= 1);
		TestTrue(TEXT("UE.ScreenPosition is a real node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionScreenPosition")) >= 1);
		TestTrue(TEXT("UE.PerInstanceRandom is a real node."),
			CountMaterialExpressionsOfClassName(Base, TEXT("MaterialExpressionPerInstanceRandom")) >= 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateInstanceBackendImportedFunctionTest,
	"DreamShader.Compiler.Generate.InstanceAliasImportedFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 6: an out-param imported function through the Instance alias. The shared Graph construction
// materializes the import through its own function-call machinery (same as the Graph backend's
// .dsf/.dsh imports), so the out-param call form keeps working with no .ush call-site rewriting.
bool FDreamShaderGenerateInstanceBackendImportedFunctionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoInstanceImport"));
	const FString HeaderFileName = AssetName + TEXT("_Shared.dsh");
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	FString HeaderFilePath;
	if (!WriteAutomationSourceFile(*this, HeaderFileName, MakeSharedHeaderSource(), HeaderFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(HeaderFilePath);

	const FString Source = FString::Printf(TEXT(R"(import "%s";

Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Properties = {
        VectorParameter InColor = (0.5, 0.5, 0.5, 1.0);
        VectorParameter InTint = (1.0, 0.8, 0.6, 1.0);
    }

    Settings = { Backend = "Instance"; ShadingModel = "Unlit"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        ApplyAutomationTint(InColor.rgb, InTint.rgb, Color);
    }
}
)"), *HeaderFileName, *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("Imported-function alias generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Imported-function alias material exists in memory."), Instance))
	{
		return false;
	}

	FLinearColor InColorValue = FLinearColor::Black;
	TestTrue(TEXT("InColor resolves natively through the chain."),
		Instance->GetVectorParameterValue(FMaterialParameterInfo(TEXT("InColor")), InColorValue));
	TestNotNull(TEXT("Instance is parented to the hidden base."), Cast<UMaterial>(Instance->Parent.Get()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGenerateInstanceBackendBaseOverridesTest,
	"DreamShader.Compiler.Generate.InstanceAliasBaseOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Stage 6: long-tail material settings through the Instance alias. They land as plain UMaterial
// properties on the hidden base via the shared reflected settings applier -- the
// FMaterialInstanceBasePropertyOverrides route (and its bOverride_ bookkeeping) is retired with the
// legacy backend.
bool FDreamShaderGenerateInstanceBackendBaseOverridesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;

	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_AutoInstanceOverrides"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString Source = FString::Printf(TEXT(R"(Shader(Name="DreamShaderTests/Automation/%s", Root="Game")
{
    Settings = {
        Backend = "Instance";
        ShadingModel = "Unlit";
        BlendMode = "Masked";
        TwoSided = true;
        OpacityMaskClipValue = 0.7;
    }

    Outputs = {
        vec3 Color;
        float Mask;
        Base.EmissiveColor = Color;
        Base.OpacityMask = Mask;
    }

    Graph = {
        Color = vec3(1.0, 1.0, 1.0);
        Mask = 1.0;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	const bool bGenerated = FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, /*bForce*/ true, /*bTransient*/ true);
	if (!TestTrue(FString::Printf(TEXT("Base-overrides alias generation succeeds: %s"), *Message), bGenerated))
	{
		return false;
	}

	UDreamShaderMaterialInstance* Instance = FindObject<UDreamShaderMaterialInstance>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Base-overrides alias material exists in memory."), Instance))
	{
		return false;
	}

	UMaterial* Base = Cast<UMaterial>(Instance->Parent.Get());
	if (TestNotNull(TEXT("Instance is parented to the hidden base."), Base))
	{
		TestEqual(TEXT("BlendMode lands on the base."), Base->BlendMode, BLEND_Masked);
		TestTrue(TEXT("TwoSided lands on the base."), Base->TwoSided != 0);
		TestTrue(TEXT("OpacityMaskClipValue lands on the base."), FMath::IsNearlyEqual(Base->OpacityMaskClipValue, 0.7f));
	}

	// The legacy per-instance override route stays untouched: settings live on the base, inherited.
	TestFalse(TEXT("No BlendMode base-property override on the instance."), Instance->BasePropertyOverrides.bOverride_BlendMode != 0);
	TestFalse(TEXT("No ShadingModel base-property override on the instance."), Instance->BasePropertyOverrides.bOverride_ShadingModel != 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSourceRootImportIsolationTest,
	"DreamShader.Compiler.Imports.SourceRootIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Locks in the rule the source-root design rests on: an unqualified import never leaves its own
// root, and the only way across is the explicit `<root>:<path>` form. A regression here would not
// fail any other test -- it would just quietly let one root shadow another.
bool FDreamShaderSourceRootImportIsolationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// The DreamShader plugin ships no DShader folder, so creating one here manufactures a second
	// source root without depending on whatever plugins this project happens to have.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DreamShader"));
	if (!TestTrue(TEXT("DreamShader plugin is discoverable."), Plugin.IsValid()))
	{
		return false;
	}

	const FString PluginRootDirectory = UE::DreamShader::NormalizeSourceFilePath(
		FPaths::Combine(Plugin->GetBaseDir(), TEXT("DShader")));
	const bool bPluginRootPreexisted = IFileManager::Get().DirectoryExists(*PluginRootDirectory);

	const FString Probe = TEXT("Namespace(Name = \"RootIsolationProbe\") { Function Pass(in float a, out float b) { b = a; } }\n");
	const FString RelativeProbeName = FString::Printf(TEXT("RootIsolation/%s.dsh"), *MakeUniqueTestAssetName(TEXT("Probe")));

	FString ProjectProbePath;
	if (!WriteAutomationSourceFile(*this, RelativeProbeName, Probe, ProjectProbePath))
	{
		return false;
	}

	const FString PluginProbePath = UE::DreamShader::NormalizeSourceFilePath(
		FPaths::Combine(PluginRootDirectory, RelativeProbeName));
	const FString PluginImporterPath = UE::DreamShader::NormalizeSourceFilePath(
		FPaths::Combine(PluginRootDirectory, TEXT("RootIsolation"), TEXT("Importer.dsm")));

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PluginProbePath), true);
	const bool bWrotePluginProbe =
		FFileHelper::SaveStringToFile(Probe, *PluginProbePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		&& FFileHelper::SaveStringToFile(Probe, *PluginImporterPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// The root list is cached; the folder created above is invisible until it is dropped.
	UE::DreamShader::RefreshSourceShaderRoots();

	if (TestTrue(TEXT("Plugin probe files were written."), bWrotePluginProbe))
	{
		// The project probe's path *relative to the project root*, which is exactly the specifier a
		// plugin file would have to write if roots fell back to one another.
		FString ProjectRelativeSpecifier = ProjectProbePath;
		FPaths::MakePathRelativeTo(ProjectRelativeSpecifier, *(UE::DreamShader::GetSourceShaderDirectory() + TEXT("/")));

		FString Resolved;
		FString Error;

		TestFalse(
			TEXT("An unqualified specifier does not reach the project root from a plugin root."),
			FDreamShaderDependencyGraphService::ResolveImportPath(PluginImporterPath, ProjectRelativeSpecifier, Resolved, &Error));
		TestTrue(
			TEXT("An ordinary miss carries no root-specific error."),
			Error.IsEmpty());

		Resolved.Reset();
		TestTrue(
			TEXT("A Project: qualified specifier does reach the project root."),
			FDreamShaderDependencyGraphService::ResolveImportPath(
				PluginImporterPath,
				FString::Printf(TEXT("Project:%s"), *ProjectRelativeSpecifier),
				Resolved,
				&Error));
		TestEqual(TEXT("It resolves to the project probe."), Resolved, ProjectProbePath);

		Resolved.Reset();
		TestTrue(
			TEXT("A Plugin.<Name>: qualified specifier reaches that plugin's root from the project."),
			FDreamShaderDependencyGraphService::ResolveImportPath(
				ProjectProbePath,
				FString::Printf(TEXT("Plugin.DreamShader:%s"), *RelativeProbeName),
				Resolved,
				&Error));
		TestEqual(TEXT("It resolves to the plugin probe."), Resolved, PluginProbePath);

		Resolved.Reset();
		Error.Reset();
		TestFalse(
			TEXT("A qualifier naming no live root fails."),
			FDreamShaderDependencyGraphService::ResolveImportPath(
				ProjectProbePath,
				TEXT("Plugin.NotInstalled:Whatever.dsh"),
				Resolved,
				&Error));
		TestTrue(
			TEXT("...with the dedicated diagnostic."),
			Error.Contains(TEXT("is not a DreamShader source root")));

		// A Windows drive letter must not be mistaken for a qualifier, or an absolute path would stop
		// reporting the plain unresolved-import error it always reported.
		Resolved.Reset();
		Error.Reset();
		TestFalse(
			TEXT("An absolute path is not read as a root qualifier."),
			FDreamShaderDependencyGraphService::ResolveImportPath(ProjectProbePath, TEXT("C:/Nowhere/Probe.dsh"), Resolved, &Error));
		TestTrue(
			TEXT("...and reports no root-specific error."),
			Error.IsEmpty());
	}

	DeleteSourceFileForAutomation(ProjectProbePath);
	DeleteSourceFileForAutomation(PluginProbePath);
	DeleteSourceFileForAutomation(PluginImporterPath);
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PluginProbePath), false, true);
	if (!bPluginRootPreexisted)
	{
		IFileManager::Get().DeleteDirectory(*PluginRootDirectory, false, true);
	}
	UE::DreamShader::RefreshSourceShaderRoots();

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderGraphLayoutNoOverlapTest,
	FDreamShaderQuietAutomationTestBase,
	"DreamShader.Gen.Layout.NoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderGraphLayoutNoOverlapTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Layout places a UMaterial graph, so pin the Graph backend rather than letting this route to a
	// hidden ThinCustom base that the object path would not resolve to.
	FScopedDreamShaderGraphBackendPin BackendPin;
	FScopedDreamShaderAutomationArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_Layout"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	// Four connected outputs, so the pass builds several blocks; one subexpression every output
	// shares, so blocks bridge through DS_Shared_* reroutes; and edges spanning more than one rank.
	//
	// Two things make the overlap assertion mean anything. Balanced trees, because rank is distance
	// to the sink and a plain chain would put exactly one node in every column, with nothing ever
	// stacked to collide. And the pair of Blend8 call sites, because ordinary math nodes are short:
	// UMaterialExpression defaults bCollapsed to true, so an Add draws no preview and stands about
	// 124 tall, which any plausible fixed row pitch clears. A Function call site becomes a
	// UMaterialExpressionCustom with one pin per argument -- eight of them here, about 292 tall --
	// and the two sit at the same rank, which is the shape that actually collided.
	const FString Source = FString::Printf(TEXT(R"(
Function float Blend8(in float a, in float b, in float c, in float d, in float e, in float f, in float g, in float h)
{
    return a * 0.2 + b * 0.15 + c * 0.15 + d * 0.1 + e * 0.1 + f * 0.1 + g * 0.1 + h * 0.1;
}

Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        ScalarParameter A = 0.25;
        ScalarParameter B = 0.5;
        VectorParameter Tint = float4(1.0, 0.5, 0.25, 1.0);
    }

    Settings = {
        Domain = "Surface";
        ShadingModel = "DefaultLit";
    }

    Outputs = {
        float3 BaseColor;
        float  Metallic;
        float  Roughness;
        float3 Emissive;
        Base.BaseColor     = BaseColor;
        Base.Metallic      = Metallic;
        Base.Roughness     = Roughness;
        Base.EmissiveColor = Emissive;
    }

    Graph = {
        float2 uv = UE.TexCoord(Index=0);
        float Common = A * B + uv.x;

        float t0 = Common * 0.5 + A;
        float t1 = Common * 1.5 + B;
        float t2 = Common * 2.5 + A;
        float t3 = Common * 3.5 + B;
        float u0 = t0 + t1;
        float u1 = t2 + t3;
        float v0 = u0 + u1;

        float p = Blend8(A, B, uv.x, uv.y, v0, A + B, A * 2.0, B * 2.0);
        float q = Blend8(B, A, uv.y, uv.x, v0, A - B, A * 3.0, B * 3.0);
        float r = p + q;

        BaseColor = Tint.rgb * r;
        Metallic  = u0 * 0.5;
        Roughness = u1 * 0.25;
        Emissive  = Tint.rgb * p;
    }
}
)"), *AssetName);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("Material generation succeeds: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("Generated material loads"), Material))
	{
		return false;
	}

	struct FPlacedNode
	{
		UMaterialExpression* Expression = nullptr;
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;
	};

	TArray<FPlacedNode> PlacedNodes;
	for (auto&& ExpressionPtr : Material->GetExpressions())
	{
		UMaterialExpression* Expression = ExpressionPtr;
		if (!Expression)
		{
			continue;
		}

		// Measured by exactly the rule the placement used, so the assertion below is about where the
		// nodes were put rather than about the estimate itself.
		const FLayoutNodeSize Size = EstimateMaterialNodeSize(Expression);
		FPlacedNode& Placed = PlacedNodes.AddDefaulted_GetRef();
		Placed.Expression = Expression;
		Placed.MinX = Expression->MaterialExpressionEditorX;
		Placed.MinY = Expression->MaterialExpressionEditorY;
		Placed.MaxX = Placed.MinX + Size.Width;
		Placed.MaxY = Placed.MinY + Size.Height;
	}

	if (!TestTrue(TEXT("Generated graph has enough nodes to exercise layout."), PlacedNodes.Num() >= 8))
	{
		return false;
	}

	// The regression this guards: spacing used to be a fixed 220 rows / 420 columns against an assumed
	// 320 x 150 footprint. A node carrying an expression preview is already ~248 tall on its own, so
	// column neighbours overlapped each other.
	int32 OverlapCount = 0;
	FString FirstOverlap;
	for (int32 Left = 0; Left < PlacedNodes.Num(); ++Left)
	{
		for (int32 Right = Left + 1; Right < PlacedNodes.Num(); ++Right)
		{
			const FPlacedNode& First = PlacedNodes[Left];
			const FPlacedNode& Second = PlacedNodes[Right];
			const bool bOverlaps = First.MinX < Second.MaxX && Second.MinX < First.MaxX
				&& First.MinY < Second.MaxY && Second.MinY < First.MaxY;
			if (!bOverlaps)
			{
				continue;
			}

			++OverlapCount;
			if (FirstOverlap.IsEmpty())
			{
				FirstOverlap = FString::Printf(
					TEXT("%s (%d,%d)-(%d,%d) vs %s (%d,%d)-(%d,%d)"),
					*First.Expression->GetName(), First.MinX, First.MinY, First.MaxX, First.MaxY,
					*Second.Expression->GetName(), Second.MinX, Second.MinY, Second.MaxX, Second.MaxY);
			}
		}
	}

	TestEqual(
		*FString::Printf(TEXT("No two laid-out nodes overlap (first: %s)"), *FirstOverlap),
		OverlapCount,
		0);

	// Generated boxes carry bGroupMode, so a node poking out of the box it sits in is a node the box
	// will leave behind when it is dragged.
	int32 CommentCount = 0;
	int32 EscapingNodeCount = 0;
	for (auto&& CommentPtr : Material->GetEditorComments())
	{
		UMaterialExpressionComment* Comment = CommentPtr;
		if (!Comment || !Comment->Text.StartsWith(TEXT("DreamShader: ")))
		{
			continue;
		}

		++CommentCount;
		const int32 BoxMinX = Comment->MaterialExpressionEditorX;
		const int32 BoxMinY = Comment->MaterialExpressionEditorY;
		const int32 BoxMaxX = BoxMinX + Comment->SizeX;
		const int32 BoxMaxY = BoxMinY + Comment->SizeY;
		for (const FPlacedNode& Node : PlacedNodes)
		{
			const bool bIntersects = Node.MinX < BoxMaxX && BoxMinX < Node.MaxX
				&& Node.MinY < BoxMaxY && BoxMinY < Node.MaxY;
			const bool bContained = Node.MinX >= BoxMinX && Node.MaxX <= BoxMaxX
				&& Node.MinY >= BoxMinY && Node.MaxY <= BoxMaxY;
			if (bIntersects && !bContained)
			{
				++EscapingNodeCount;
			}
		}
	}

	TestTrue(TEXT("Layout produced at least one DreamShader comment box."), CommentCount > 0);
	TestEqual(TEXT("No node hangs out of the comment box it sits in."), EscapingNodeCount, 0);
	return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
