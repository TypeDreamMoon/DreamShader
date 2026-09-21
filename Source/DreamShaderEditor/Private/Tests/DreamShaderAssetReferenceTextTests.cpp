// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Content Browser's "Copy Reference" spelling -- Class'/Game/Folder/Asset.Asset' -- end to end.
//
// Three layers, deliberately:
//
//   * the shared helper (Public/DreamShaderAssetReferenceText.h) as a pure function, because it is
//     the one piece both the resolver and the rename-sync service depend on, and a pure test is the
//     only one that can enumerate the malformed spellings cheaply;
//   * the asset-reference resolver the compiler uses (TryResolveDreamShaderAssetReference), fed the default text
//     the legacy front end carries unresolved for a 1.x property. That is where the shelled, quoted and Path(...)
//     spellings have to agree on one resolved object path. The 1.x runtime parser, which resolved them at
//     parse time, is gone; the legacy front end keeps the text and the emitter resolves it;
//   * compilation, where the resolved path has to end up on the actual UMaterialExpression.
//
// The fixtures reference /Engine/EngineResources/DefaultTexture, which ships with the engine, so
// nothing here needs a project asset or a content fixture.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderAssetReferenceText.h"
// TryResolveDreamShaderAssetReference: the resolver every compile-side reference goes through.
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderTypes.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/VolumeTexture.h"
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

		/** A one-property 1.x Shader around one Properties declaration. */
		static FString MakeShellReferenceSource(const FString& Declaration)
		{
			return FString::Printf(TEXT(
				"Shader(Name=\"DreamShaderTests/Params/M_ShellForms\", Root=\"Game\")\n"
				"{\n"
				"    Properties { %s }\n"
				"    Settings { Domain = \"Surface\"; ShadingModel = \"Unlit\"; BlendMode = \"Opaque\"; }\n"
				"    Outputs { vec3 Color; Base.EmissiveColor = Color; }\n"
				"    Graph { Color = vec3(0.5, 0.5, 0.5); }\n"
				"}\n"), *Declaration);
		}

		/**
		 * The default text a 1.x source gave the texture-sample parameter `Name`, as the legacy front end carries it: a
		 * texture-sample parameter is expanded at its uses, so it is recorded as a FLegacyParameterDeclaration whose
		 * DefaultText is the source text verbatim, and the emitter resolves that text. Records a failure and answers
		 * false when the source does not parse or the declaration is missing.
		 */
		static bool GetLegacyTextureDefaultText(FAutomationTestBase& Test, const FString& Source, const TCHAR* Name, FString& OutText)
		{
			const UE::DreamShader::Lang::FLangSourceText Text(TEXT("M_ShellForms.dsm"), Source);
			const UE::DreamShader::Lang::FLangParseResult Result = UE::DreamShader::Lang::ParseDreamShaderLang(Text, UE::DreamShader::Lang::FLangParseOptions());
			if (!Test.TestTrue(
					*FString::Printf(TEXT("property %s: the legacy front end parses its source (%s)"), Name,
						*FString::Join(GatherDreamShaderLangDiagnostics(Result.Diagnostics, UE::DreamShader::Lang::ELangSeverity::Error), TEXT(" | "))),
					Result.Succeeded() && Result.Legacy.IsValid()))
			{
				return false;
			}

			const UE::DreamShader::Lang::FLegacyParameterDeclaration* Declaration = Result.Legacy->ParameterDeclarations.FindByPredicate(
				[Name](const UE::DreamShader::Lang::FLegacyParameterDeclaration& Candidate)
				{
					return Candidate.Name.Equals(Name, ESearchCase::CaseSensitive);
				});
			if (!Test.TestNotNull(*FString::Printf(TEXT("property %s is recorded with its default"), Name), Declaration))
			{
				return false;
			}

			OutText = Declaration->DefaultText;
			return Test.TestFalse(*FString::Printf(TEXT("property %s keeps its default text"), Name), OutText.IsEmpty());
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
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests::ShellReference;

	struct FFormCase
	{
		const TCHAR* What;
		const TCHAR* Default;
	};

	// Every spelling below names the same asset. The point of the feature is that they agree, so they are asserted
	// against one expected path rather than against each other. One source per spelling, so a spelling the legacy
	// front end does not carry is reported on its own.
	static const FFormCase Cases[] =
	{
		{ TEXT("A: quoted /Script/ shell"),     TEXT("\"/Script/Engine.Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\"") },
		{ TEXT("B: quoted bare-class shell"),   TEXT("\"Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\"") },
		{ TEXT("C: unquoted shell"),            TEXT("Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'") },
		{ TEXT("D: Path(shell)"),               TEXT("Path(\"Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\")") },
		{ TEXT("E: Path(root, shell)"),         TEXT("Path(Game, \"Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\")") },
		{ TEXT("F: Path(root, absolute path)"), TEXT("Path(Game, \"/Engine/EngineResources/DefaultTexture\")") },
		{ TEXT("G: quoted package path"),       TEXT("\"/Engine/EngineResources/DefaultTexture\"") },
		{ TEXT("H: Path(Engine, relative)"),    TEXT("Path(Engine, \"EngineResources/DefaultTexture\")") },
	};

	for (const FFormCase& Case : Cases)
	{
		FString DefaultText;
		if (!GetLegacyTextureDefaultText(
				*this,
				MakeShellReferenceSource(FString::Printf(TEXT("TextureSampleParameter2D P = %s;"), Case.Default)),
				TEXT("P"),
				DefaultText))
		{
			AddInfo(FString::Printf(TEXT("[%s] was not carried through the legacy front end."), Case.What));
			continue;
		}

		FString ObjectPath;
		UE::DreamShader::FDreamShaderError Error;
		const bool bResolved = TryResolveDreamShaderAssetReference(DefaultText, ObjectPath, Error, UTexture::StaticClass());
		if (!TestTrue(*FString::Printf(TEXT("[%s] '%s' resolves (%s %s)"), Case.What, *DefaultText, *Error.Code, *Error.Message), bResolved))
		{
			continue;
		}

		TestEqual(*FString::Printf(TEXT("[%s] resolves to the engine texture"), Case.What), ObjectPath, FString(EngineTexturePath));
	}

	return true;
}

// A root written beside an absolute path is IGNORED, not prepended -- the behaviour the asset-reference resolver
// has always had, and since 1.9.0 the texture defaults too. Before 1.9.0 this produced '/Game/Engine/...' and then
// failed to load. Kept as its own test because it is a deliberate behaviour CHANGE, not new syntax.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTextureDefaultRootIgnoredTest,
	"DreamShader.Lang.AssetReferences.RootIgnoredForAbsolutePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTextureDefaultRootIgnoredTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests::ShellReference;

	const FString Source = MakeShellReferenceSource(TEXT(
		"TextureSampleParameter2D A = Path(Game, \"/Engine/EngineResources/DefaultTexture\");\n"
		"        TextureSampleParameter2D B = Path(Plugin.DreamShader, \"/Engine/EngineResources/DefaultTexture\");"));

	for (const TCHAR* const Name : { TEXT("A"), TEXT("B") })
	{
		FString DefaultText;
		if (!GetLegacyTextureDefaultText(*this, Source, Name, DefaultText))
		{
			continue;
		}

		FString ObjectPath;
		UE::DreamShader::FDreamShaderError Error;
		if (!TestTrue(
				*FString::Printf(TEXT("property %s resolves (%s %s)"), Name, *Error.Code, *Error.Message),
				TryResolveDreamShaderAssetReference(DefaultText, ObjectPath, Error, UTexture::StaticClass())))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("property %s ignores the root and keeps the absolute path"), Name),
			ObjectPath,
			FString(EngineTexturePath));
	}

	return true;
}

// The class in the shell is checked against the slot, before anything is loaded. In 2.0 the check is the resolver's,
// judged against the class the receiving slot declares: related classes pass in either direction, an unrelated one is
// DSH1045, and a class name DreamShader cannot place is never an error.
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderTextureDefaultShellClassCheckTest,
	FDreamShaderShellReferenceQuietTestBase,
	"DreamShader.Lang.AssetReferences.ShellClassCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderTextureDefaultShellClassCheckTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;

	struct FClassCase
	{
		const TCHAR* What;
		const TCHAR* Reference;
		UClass* SlotClass;
		bool bResolves;
		const TCHAR* MessageContains;  // asserted only when bResolves is false
	};

	const FClassCase Cases[] =
	{
		// The dimension the slot declares wins over the class the paste happened to carry.
		{ TEXT("Texture2D into a Volume slot"),
		  TEXT("\"Texture2D'/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture'\""),
		  UVolumeTexture::StaticClass(), false, TEXT("VolumeTexture") },
		{ TEXT("VolumeTexture into a 2D slot"),
		  TEXT("\"VolumeTexture'/Engine/EngineResources/DefaultTexture.DefaultTexture'\""),
		  UTexture2D::StaticClass(), false, TEXT("VolumeTexture") },
		// A class that plainly cannot be a texture, whatever the slot's dimension.
		{ TEXT("MaterialFunction into a texture slot"),
		  TEXT("\"MaterialFunction'/Engine/EngineResources/DefaultTexture.DefaultTexture'\""),
		  UTexture2D::StaticClass(), false, TEXT("MaterialFunction") },

		// Accepted: the class agrees, or DreamShader has no opinion about it.
		{ TEXT("matching dimension"),
		  TEXT("\"Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\""),
		  UTexture2D::StaticClass(), true, nullptr },
		// A render target is a UTexture but not a UTexture2D: the slot that takes one is a UTexture slot, as the
		// Texture property of every texture-sample node is.
		{ TEXT("render target into a texture slot"),
		  TEXT("\"TextureRenderTarget2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'\""),
		  UTexture::StaticClass(), true, nullptr },
		{ TEXT("unknown class is not an error"),
		  TEXT("\"MyProjectTexture'/Engine/EngineResources/DefaultTexture.DefaultTexture'\""),
		  UTexture2D::StaticClass(), true, nullptr },
		// A texture slot that declares no dimension of its own judges only the not-a-texture half.
		{ TEXT("dimensionless slot ignores the dimension"),
		  TEXT("\"VolumeTexture'/Engine/EngineResources/DefaultTexture.DefaultTexture'\""),
		  UTexture::StaticClass(), true, nullptr },
		{ TEXT("dimensionless slot still refuses a non-texture"),
		  TEXT("\"CurveLinearColor'/Engine/EngineResources/DefaultTexture.DefaultTexture'\""),
		  UTexture::StaticClass(), false, TEXT("CurveLinearColor") },
	};

	for (const FClassCase& Case : Cases)
	{
		FString ObjectPath;
		UE::DreamShader::FDreamShaderError Error;
		const bool bResolved = TryResolveDreamShaderAssetReference(Case.Reference, ObjectPath, Error, Case.SlotClass);

		if (!TestEqual(*FString::Printf(TEXT("[%s] resolves or not (%s %s)"), Case.What, *Error.Code, *Error.Message), (int32)bResolved, (int32)Case.bResolves))
		{
			continue;
		}

		if (Case.bResolves)
		{
			continue;
		}

		// The message names both the class that was written and what the slot wanted -- that precision
		// is the whole reason the check exists rather than waiting for the load to fail.
		TestTrue(
			*FString::Printf(TEXT("[%s] the refusal names the class. Got: %s"), Case.What, *Error.Message),
			Error.Message.Contains(Case.MessageContains));
		TestEqual(*FString::Printf(TEXT("[%s] the refusal is DSH1045"), Case.What), Error.Code, FString(TEXT("DSH1045")));
	}

	return true;
}

// -------------------------------------------------------------------------------------------------
// Compilation: the resolved path has to reach the material.
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
		FString::Printf(TEXT("a material whose texture defaults are pasted references compiles: %s"), *Message),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestMaterial(SourceFilePath, Message, true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("the compiled material loads"), Material))
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

// The class check refuses the paste before the compile gets anywhere near loading the asset.
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
	// The refusal is the expected outcome, but a compile that wrongly succeeds writes a real asset (a Graph material
	// always saves in 2.0), so the object path is cleaned up either way.
	const FString ObjectPath = MakeAutomationObjectPath(AssetName);
	Artifacts.AddObjectPath(ObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, ObjectPath);
	AddExpectedAutomationCleanupWarnings(*this);

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

	FString Message;
	TestFalse(
		TEXT("a texture default whose shell class contradicts the slot does not compile"),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestMaterial(SourceFilePath, Message, true, true));

	TestTrue(
		FString::Printf(TEXT("the failure names the class that was written. Got: %s"), *Message),
		Message.Contains(TEXT("Texture2D")));
	TestTrue(
		FString::Printf(TEXT("the failure names the type the property declares. Got: %s"), *Message),
		Message.Contains(TEXT("VolumeTexture")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
