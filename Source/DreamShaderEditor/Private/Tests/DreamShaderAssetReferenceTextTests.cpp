// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Content Browser's "Copy Reference" spelling -- Class'/Game/Folder/Asset.Asset' -- end to end.
//
// Three layers, deliberately:
//
//   * the shared helper (Public/DreamShaderAssetReferenceText.h) as a pure function, because it is
//     the one piece BOTH resolvers and the rename-sync service depend on, and a pure test is the
//     only one that can enumerate the malformed spellings cheaply;
//   * the texture-default resolver through FTextShaderParser::Parse, which is where the shelled,
//     quoted and Path(...) spellings have to agree on one resolved object path;
//   * generation, where the resolved path has to end up on the actual UMaterialExpression.
//
// The fixtures reference /Engine/EngineResources/DefaultTexture, which ships with the engine, so
// nothing here needs a project asset or a content fixture.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderAssetReferenceText.h"
#include "DreamShaderParser.h"
#include "DreamShaderTypes.h"

#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	// Fixture helpers shared with DreamShaderAutomationTests.cpp (defined there, external linkage).
	FString MakeUniqueTestAssetName(const TCHAR* Prefix);
	FString MakeAutomationObjectPath(const FString& AssetName);
	bool WriteAutomationSourceFile(FAutomationTestBase& Test, const FString& FileName, const FString& SourceText, FString& OutSourceFilePath);
	void AddExpectedNewAssetProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath);
	void AddExpectedAutomationCleanupWarnings(FAutomationTestBase& Test);
	void DeleteSourceFileForAutomation(const FString& SourceFilePath);
	void DeleteAssetForAutomation(const FString& ObjectPath);

	namespace ShellReference
	{
		/** An engine texture, so the fixtures need no project content. */
		static const TCHAR* const EngineTexturePath = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");

		/** An engine volume texture, for the dimension-mismatch case. */
		static const TCHAR* const EngineVolumeTexturePath = TEXT("/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture");

		/**
		 * Local mirror of the artifacts guard in DreamShaderAutomationTests.cpp: that one is a
		 * file-local class there, so it cannot be forward-declared, but the delete helpers it drives
		 * can be and are.
		 */
		class FScopedShellReferenceArtifacts
		{
		public:
			void AddSourceFile(const FString& SourceFilePath) { SourceFilePaths.Add(SourceFilePath); }
			void AddObjectPath(const FString& ObjectPath) { ObjectPaths.Add(ObjectPath); }

			~FScopedShellReferenceArtifacts()
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

		/** Find one parsed property by name. */
		static const FTextShaderPropertyDefinition* FindProperty(const FTextShaderDefinition& Definition, const TCHAR* Name)
		{
			return Definition.Properties.FindByPredicate(
				[Name](const FTextShaderPropertyDefinition& Candidate) { return Candidate.Name == Name; });
		}
	}
}

/**
 * Base for the cases that are supposed to fail. A refusal is allowed to log, and an incidental log
 * line must not be read as a test failure. At file scope, matching the other test files: the
 * IMPLEMENT_CUSTOM_* macro names the base unqualified in two places.
 */
class FDreamShaderShellReferenceQuietTestBase : public FAutomationTestBase
{
public:
	FDreamShaderShellReferenceQuietTestBase(const FString& InName, bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	virtual bool SuppressLogErrors() override { return true; }
	virtual bool SuppressLogWarnings() override { return true; }
};

// -------------------------------------------------------------------------------------------------
// The helper, as a pure function.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderReferenceShellStrippingTest,
	"DreamShader.Lang.AssetReferences.ShellStripping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderReferenceShellStrippingTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;

	struct FStripCase
	{
		const TCHAR* What;
		const TCHAR* Input;
		bool bStripped;
		const TCHAR* ClassName;   // expected class name when bStripped
		const TCHAR* ObjectPath;  // expected object path when bStripped
	};

	static const FStripCase Cases[] =
	{
		// The two spellings the Content Browser produces, old and new.
		{ TEXT("/Script/ prefixed"),        TEXT("/Script/Engine.Texture2D'/Game/Cloud/T_V_03.T_V_03'"), true,  TEXT("Texture2D"),    TEXT("/Game/Cloud/T_V_03.T_V_03") },
		{ TEXT("bare class name"),          TEXT("Texture2D'/Game/Cloud/T_V_03.T_V_03'"),                true,  TEXT("Texture2D"),    TEXT("/Game/Cloud/T_V_03.T_V_03") },
		{ TEXT("volume texture"),           TEXT("/Script/Engine.VolumeTexture'/Game/V.V'"),             true,  TEXT("VolumeTexture"), TEXT("/Game/V.V") },
		// A class from some other module keeps only its short name.
		{ TEXT("non-Engine module"),        TEXT("/Script/Niagara.NiagaraSystem'/Game/N.N'"),            true,  TEXT("NiagaraSystem"), TEXT("/Game/N.N") },
		// Whitespace, inside and out.
		{ TEXT("outer whitespace"),         TEXT("   Texture2D'/Game/A/T_X.T_X'  "),                     true,  TEXT("Texture2D"),    TEXT("/Game/A/T_X.T_X") },
		{ TEXT("whitespace around path"),   TEXT("Texture2D' /Game/A/T_X.T_X '"),                        true,  TEXT("Texture2D"),    TEXT("/Game/A/T_X.T_X") },
		{ TEXT("space before the quote"),   TEXT("Texture2D '/Game/A/T_X.T_X'"),                         true,  TEXT("Texture2D"),    TEXT("/Game/A/T_X.T_X") },
		// A path whose folders and asset name carry dots -- the class name is taken from the LAST dot
		// of the PREFIX, so the dots inside the quotes must not reach it.
		{ TEXT("dots in the path"),         TEXT("Texture2D'/Game/A.B/T_X.T_X'"),                        true,  TEXT("Texture2D"),    TEXT("/Game/A.B/T_X.T_X") },
		// No class at all is still a shell: Unreal writes this form too.
		{ TEXT("quotes with no prefix"),    TEXT("'/Game/A/T_X.T_X'"),                                   true,  TEXT(""),             TEXT("/Game/A/T_X.T_X") },

		// Not shelled: every one of these has to come back untouched so the caller's own grammar and
		// its own diagnostics still see the text it was given.
		{ TEXT("plain absolute path"),      TEXT("/Game/A/T_X.T_X"),                                     false, nullptr, nullptr },
		{ TEXT("quoted absolute path"),     TEXT("\"/Game/A/T_X.T_X\""),                                 false, nullptr, nullptr },
		{ TEXT("Path(...) call"),           TEXT("Path(Game, \"Textures/T_X\")"),                        false, nullptr, nullptr },
		{ TEXT("empty"),                    TEXT(""),                                                    false, nullptr, nullptr },
		{ TEXT("whitespace only"),          TEXT("   "),                                                 false, nullptr, nullptr },
		{ TEXT("a lone quote"),             TEXT("'"),                                                   false, nullptr, nullptr },
		{ TEXT("unterminated shell"),       TEXT("Texture2D'/Game/A/T_X.T_X"),                           false, nullptr, nullptr },
		{ TEXT("no opening quote"),         TEXT("Texture2D/Game/A/T_X.T_X'"),                           false, nullptr, nullptr },
		{ TEXT("empty inside the quotes"),  TEXT("Texture2D''"),                                         false, nullptr, nullptr },
		{ TEXT("whitespace in the quotes"), TEXT("Texture2D'   '"),                                      false, nullptr, nullptr },
		// Nested quotes: ambiguous, so it is refused rather than guessed at.
		{ TEXT("nested quotes"),            TEXT("Texture2D'/Game/A'B.C'"),                              false, nullptr, nullptr },
	};

	for (const FStripCase& Case : Cases)
	{
		FDreamShaderReferenceShell Shell;
		const bool bStripped = TryStripDreamShaderReferenceShell(Case.Input, Shell);
		if (!TestEqual(*FString::Printf(TEXT("[%s] strips or not"), Case.What), (int32)bStripped, (int32)Case.bStripped))
		{
			continue;
		}

		if (!bStripped)
		{
			continue;
		}

		TestEqual(*FString::Printf(TEXT("[%s] class name"), Case.What), Shell.ClassName, FString(Case.ClassName));
		TestEqual(*FString::Printf(TEXT("[%s] object path"), Case.What), Shell.ObjectPath, FString(Case.ObjectPath));
	}

	// The full class path is kept beside the short name: the resolver needs it to look the class up
	// in the module it was actually written for.
	{
		FDreamShaderReferenceShell Shell;
		TestTrue(TEXT("class path is kept verbatim"),
			TryStripDreamShaderReferenceShell(TEXT("/Script/Engine.Texture2D'/Game/A.A'"), Shell));
		TestEqual(TEXT("ClassPath is the prefix as written"), Shell.ClassPath, FString(TEXT("/Script/Engine.Texture2D")));
	}

	// Class classification: the tables that decide whether a written class contradicts the slot.
	{
		ETextShaderTextureType TextureType = ETextShaderTextureType::TextureCube;
		TestTrue(TEXT("Texture2D maps to a dimension"), TryGetDreamShaderReferenceTextureType(TEXT("Texture2D"), TextureType));
		TestEqual(TEXT("Texture2D -> Texture2D"), (int32)TextureType, (int32)ETextShaderTextureType::Texture2D);

		TestTrue(TEXT("class names are matched case-insensitively"), TryGetDreamShaderReferenceTextureType(TEXT("volumetexture"), TextureType));
		TestEqual(TEXT("volumetexture -> VolumeTexture"), (int32)TextureType, (int32)ETextShaderTextureType::VolumeTexture);

		TestTrue(TEXT("a render target counts as its own dimension"), TryGetDreamShaderReferenceTextureType(TEXT("TextureRenderTarget2D"), TextureType));
		TestEqual(TEXT("TextureRenderTarget2D -> Texture2D"), (int32)TextureType, (int32)ETextShaderTextureType::Texture2D);

		TestFalse(TEXT("a project class DreamShader has never heard of has no dimension"),
			TryGetDreamShaderReferenceTextureType(TEXT("MyProjectTexture"), TextureType));

		TestTrue(TEXT("SparseVolumeTexture is a texture with no modelled dimension"),
			IsDreamShaderTextureReferenceClass(TEXT("SparseVolumeTexture")));
		TestFalse(TEXT("...and is therefore not judged by dimension"),
			TryGetDreamShaderReferenceTextureType(TEXT("SparseVolumeTexture"), TextureType));

		TestTrue(TEXT("MaterialFunction is known not to be a texture"),
			IsKnownDreamShaderNonTextureReferenceClass(TEXT("MaterialFunction")));
		TestTrue(TEXT("CurveLinearColor is known not to be a texture"),
			IsKnownDreamShaderNonTextureReferenceClass(TEXT("CurveLinearColor")));
		TestFalse(TEXT("Texture2D is not on the not-a-texture list"),
			IsKnownDreamShaderNonTextureReferenceClass(TEXT("Texture2D")));
		// RuntimeVirtualTexture is not a UTexture, but RuntimeVirtualTextureSampleParameter takes one
		// where a texture would go, so it must not be refused as "not a texture".
		TestFalse(TEXT("RuntimeVirtualTexture is not refused in a texture slot"),
			IsKnownDreamShaderNonTextureReferenceClass(TEXT("RuntimeVirtualTexture")));
		TestFalse(TEXT("an unknown class is never on the not-a-texture list"),
			IsKnownDreamShaderNonTextureReferenceClass(TEXT("MyProjectThing")));
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// The texture-default resolver: every spelling of one reference resolves to one object path.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTextureDefaultShellFormsTest,
	"DreamShader.Lang.AssetReferences.TextureDefaultForms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTextureDefaultShellFormsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::ShellReference;

	const FString Source = TEXT(R"(
Shader(Name="DreamShaderTests/Params/M_ShellForms", Root="Game")
{
    Properties {
        TextureSampleParameter2D A = "/Script/Engine.Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'";
        TextureSampleParameter2D B = "Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'";
        TextureSampleParameter2D C = Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture';
        TextureSampleParameter2D D = Path("Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'");
        TextureSampleParameter2D E = Path(Game, "Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'");
        TextureSampleParameter2D F = Path(Game, "/Engine/EngineResources/DefaultTexture");
        TextureSampleParameter2D G = "/Engine/EngineResources/DefaultTexture";
        TextureSampleParameter2D H = Path(Engine, "EngineResources/DefaultTexture");
    }
    Settings { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs { vec3 Color; Base.EmissiveColor = Color; }
    Graph { Color = A.rgb; }
}
)");

	FTextShaderDefinition Definition;
	FDreamShaderTextError ParseError;
	// Parse first: reading the error inside the assertion's message argument would format it before
	// Parse has run.
	const bool bParsed = FTextShaderParser::Parse(Source, Definition, ParseError);
	if (!TestTrue(
		FString::Printf(TEXT("shelled texture defaults parse [%s] %s"), *ParseError.Code, *ParseError.Message.ToString()),
		bParsed))
	{
		return false;
	}

	// Every spelling above names the same asset. The point of the feature is that they agree, so they
	// are asserted against one expected path rather than against each other.
	static const TCHAR* const Names[] = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F"), TEXT("G"), TEXT("H") };
	for (const TCHAR* const Name : Names)
	{
		const FTextShaderPropertyDefinition* Property = FindProperty(Definition, Name);
		if (!TestNotNull(*FString::Printf(TEXT("property %s parsed"), Name), Property))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("property %s resolves to the engine texture"), Name),
			Property->TextureDefaultObjectPath,
			FString(EngineTexturePath));
	}

	return true;
}

// A root written beside an absolute path is IGNORED, not prepended -- the behaviour the
// asset-reference resolver has always had, which the texture-default resolver now matches. Before
// 1.9.0 this produced '/Game/Engine/...' and then failed to load. Kept as its own test because it is
// a deliberate behaviour CHANGE, not new syntax.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTextureDefaultRootIgnoredTest,
	"DreamShader.Lang.AssetReferences.RootIgnoredForAbsolutePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTextureDefaultRootIgnoredTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::ShellReference;

	const FString Source = TEXT(R"(
Shader(Name="DreamShaderTests/Params/M_RootIgnored", Root="Game")
{
    Properties {
        TextureSampleParameter2D A = Path(Game, "/Engine/EngineResources/DefaultTexture");
        TextureSampleParameter2D B = Path(Plugin.DreamShader, "/Engine/EngineResources/DefaultTexture");
    }
    Settings { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs { vec3 Color; Base.EmissiveColor = Color; }
    Graph { Color = A.rgb; }
}
)");

	FTextShaderDefinition Definition;
	FDreamShaderTextError ParseError;
	const bool bParsed = FTextShaderParser::Parse(Source, Definition, ParseError);
	if (!TestTrue(
		FString::Printf(TEXT("root + absolute path parses [%s] %s"), *ParseError.Code, *ParseError.Message.ToString()),
		bParsed))
	{
		return false;
	}

	for (const TCHAR* const Name : { TEXT("A"), TEXT("B") })
	{
		const FTextShaderPropertyDefinition* Property = FindProperty(Definition, Name);
		if (!TestNotNull(*FString::Printf(TEXT("property %s parsed"), Name), Property))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("property %s ignores the root and keeps the absolute path"), Name),
			Property->TextureDefaultObjectPath,
			FString(EngineTexturePath));
	}

	return true;
}

// The class in the shell is checked against the slot, before anything is loaded.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTextureDefaultShellClassCheckTest,
	FDreamShaderShellReferenceQuietTestBase,
	"DreamShader.Lang.AssetReferences.ShellClassCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTextureDefaultShellClassCheckTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::Tests::ShellReference;

	struct FClassCase
	{
		const TCHAR* What;
		const TCHAR* Declaration;
		bool bParses;
		const TCHAR* MessageContains;  // asserted only when bParses is false
	};

	static const FClassCase Cases[] =
	{
		// The dimension the slot declares wins over the class the paste happened to carry.
		{ TEXT("Texture2D into a Volume slot"),
		  TEXT("TextureSampleParameterVolume P = \"Texture2D'/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture'\";"),
		  false, TEXT("VolumeTexture") },
		{ TEXT("VolumeTexture into a 2D slot"),
		  TEXT("TextureSampleParameter2D P = \"VolumeTexture'/Engine/EngineResources/DefaultTexture.DefaultTexture'\";"),
		  false, TEXT("VolumeTexture") },
		// A class that plainly cannot be a texture, whatever the slot's dimension.
		{ TEXT("MaterialFunction into a texture slot"),
		  TEXT("TextureSampleParameter2D P = \"MaterialFunction'/Engine/EngineResources/DefaultTexture.DefaultTexture'\";"),
		  false, TEXT("MaterialFunction") },

		// Accepted: the class agrees, or DreamShader has no opinion about it.
		{ TEXT("matching dimension"),
		  TEXT("TextureSampleParameter2D P = \"Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\";"),
		  true, nullptr },
		{ TEXT("render target of the right dimension"),
		  TEXT("TextureSampleParameter2D P = \"TextureRenderTarget2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\";"),
		  true, nullptr },
		{ TEXT("unknown class is not an error"),
		  TEXT("TextureSampleParameter2D P = \"MyProjectTexture'/Engine/EngineResources/DefaultTexture.DefaultTexture'\";"),
		  true, nullptr },
		// TextureObjectParameter declares no dimension of its own, so only the not-a-texture half of
		// the check applies to it.
		{ TEXT("dimensionless slot ignores the dimension"),
		  TEXT("TextureObjectParameter P = \"VolumeTexture'/Engine/EngineResources/DefaultTexture.DefaultTexture'\";"),
		  true, nullptr },
		{ TEXT("dimensionless slot still refuses a non-texture"),
		  TEXT("TextureObjectParameter P = \"CurveLinearColor'/Engine/EngineResources/DefaultTexture.DefaultTexture'\";"),
		  false, TEXT("CurveLinearColor") },
	};

	for (const FClassCase& Case : Cases)
	{
		const FString Source = FString::Printf(TEXT(
			"Shader(Name=\"DreamShaderTests/Params/M_ShellClass\", Root=\"Game\")\n"
			"{\n"
			"    Properties { %s }\n"
			"    Settings { Domain = \"Surface\"; ShadingModel = \"Unlit\"; BlendMode = \"Opaque\"; }\n"
			"    Outputs { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph { Color = vec3(0.5, 0.5, 0.5); }\n"
			"}\n"), Case.Declaration);

		FTextShaderDefinition Definition;
		FDreamShaderTextError ParseError;
		const bool bParsed = FTextShaderParser::Parse(Source, Definition, ParseError);
		const FString Message = ParseError.Message.ToString();

		if (!TestEqual(*FString::Printf(TEXT("[%s] parses or not (%s)"), Case.What, *Message), (int32)bParsed, (int32)Case.bParses))
		{
			continue;
		}

		if (Case.bParses)
		{
			continue;
		}

		// The message names both the class that was written and what the slot wanted -- that precision
		// is the whole reason the check exists rather than waiting for the load to fail.
		TestTrue(
			*FString::Printf(TEXT("[%s] the message names the written class. Got: %s"), Case.What, *Message),
			Message.Contains(Case.MessageContains));
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Generation: the resolved path has to reach the material.
// -------------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCopyReferenceShellDefaultsTest,
	FDreamShaderShellReferenceQuietTestBase,
	"DreamShader.Gen.Parameters.CopyReferenceShellDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCopyReferenceShellDefaultsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Tests::ShellReference;

	// This test loads the result as a UMaterial, which only the Graph backend produces.
	FScopedDreamShaderGraphBackendPin BackendPin;

	FScopedShellReferenceArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_ShellRef"));
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

	// Three spellings of one reference: Path(root, shelled), Path(shelled), and bare quoted shelled.
	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        TextureSampleParameter2D A = Path(Game, "Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'");
        TextureSampleParameter2D B = Path("Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'");
        TextureSampleParameter2D C = "/Script/Engine.Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'";
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = A.rgb + B.rgb + C.rgb;
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
		FString::Printf(TEXT("a material whose texture defaults are pasted references generates: %s"), *Message),
		UE::DreamShader::Editor::FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the generated material loads"), Material))
	{
		return false;
	}

	UTexture* ExpectedTexture = LoadObject<UTexture>(nullptr, EngineTexturePath);
	if (!TestNotNull(TEXT("the engine fixture texture loads"), ExpectedTexture))
	{
		return false;
	}

	int32 SamplerCount = 0;
	int32 BoundToExpectedCount = 0;
	for (auto&& ExpressionPtr : Material->GetExpressions())
	{
		if (const UMaterialExpressionTextureSampleParameter2D* Sampler =
			Cast<UMaterialExpressionTextureSampleParameter2D>(ExpressionPtr))
		{
			++SamplerCount;
			if (Sampler->Texture == ExpectedTexture)
			{
				++BoundToExpectedCount;
			}
		}
	}

	TestEqual(TEXT("all three declarations generate a sampler node"), SamplerCount, 3);
	TestEqual(TEXT("all three spellings bind the same engine texture"), BoundToExpectedCount, 3);
	return true;
}

// The class check refuses the paste before generation gets anywhere near loading the asset.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderCopyReferenceShellClassMismatchTest,
	FDreamShaderShellReferenceQuietTestBase,
	"DreamShader.Gen.Parameters.CopyReferenceShellClassMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderCopyReferenceShellClassMismatchTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::Tests::ShellReference;

	FScopedShellReferenceArtifacts Artifacts;
	const FString AssetName = MakeUniqueTestAssetName(TEXT("M_ShellRefBad"));

	// A VolumeTexture slot fed a reference whose shell says Texture2D. The asset behind the path is a
	// perfectly good volume texture: only the class written in the shell is wrong, which is exactly
	// what a paste from the wrong Content Browser row looks like.
	const FString Source = FString::Printf(TEXT(R"(
Shader(Name="DreamShaderTests/Automation/%s")
{
    Properties = {
        TextureSampleParameterVolume V = "Texture2D'%s'";
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = vec3(0.5, 0.5, 0.5);
    }
}
)"), *AssetName, EngineVolumeTexturePath);

	FString SourceFilePath;
	if (!WriteAutomationSourceFile(*this, AssetName + TEXT(".dsm"), Source, SourceFilePath))
	{
		return false;
	}
	Artifacts.AddSourceFile(SourceFilePath);

	// Transient: the source never gets far enough to write an asset, so nothing has to be cleaned up
	// in the content browser and no new-asset probe warning has to be expected.
	FString Message;
	TestFalse(
		TEXT("a texture default whose shell class contradicts the slot does not generate"),
		UE::DreamShader::Editor::FMaterialGenerator::GenerateMaterialFromFile(SourceFilePath, Message, true, true));

	TestTrue(
		FString::Printf(TEXT("the failure names the class that was written. Got: %s"), *Message),
		Message.Contains(TEXT("Texture2D")));
	TestTrue(
		FString::Printf(TEXT("the failure names the type the property declares. Got: %s"), *Message),
		Message.Contains(TEXT("VolumeTexture")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
