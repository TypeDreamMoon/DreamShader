// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Complete coverage for the parameter-expression Properties surface of a 1.x source, as the legacy front end
// reads it. The 1.x runtime parser is gone, so the parse axis is now "which 2.0 form does each
// parameter node type become":
//
//   * a `uniform` declaration: ScalarParameter -> `float`, StaticBoolParameter -> `/// @static` `bool`,
//     VectorParameter -> `float4`, TextureObjectParameter -> `Texture2D` whose default is a `/// @default`;
//   * no declaration but one reflected call per use, recorded in FLegacyMigrationInfo::ParameterDeclarations: the
//     static switch, the channel mask, the static component mask and every texture-sample parameter;
//   * DSH3253, a node type 2.0 has no spelling for: DoubleVector, CurveAtlasRow, Dynamic, FontSample,
//     SpriteTextureSampler, TextureCollection, SparseVolumeTextureObject.
//
// Also pins the "default is optional" contract and the Group / SortPriority / Slider metadata the directives come
// from. The generate axis (the node really appears in the graph) is DreamShader.Gen.Parameters.* in
// DreamShaderAutomationTests.cpp.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderTestCommon.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"

#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::ParameterTests
{
	/** The 2.0 form the legacy front end gives a 1.x property type. */
	enum class EParameterTestForm : uint8
	{
		/** A `uniform` declaration of UniformType. */
		Uniform,
		/** No declaration: FLegacyParameterDeclaration, expanded at every use. */
		ExpandAtUse,
		/** DSH3253. */
		Unsupported,
	};

	struct FParameterCase
	{
		const TCHAR* NodeType;          // the 1.x keyword
		const TCHAR* Default;           // inline default literal, or nullptr for "declare without default"
		EParameterTestForm Form;
		const TCHAR* UniformType;       // Form == Uniform: the declaration's FTypeRef::Name
	};

	// One row per parameter node type 1.x accepted in a plain Properties declaration.
	static const FParameterCase GParameterCases[] = {
		// Declarations
		{ TEXT("ScalarParameter"),                      TEXT("0.55"),                                EParameterTestForm::Uniform,     TEXT("float") },
		{ TEXT("ScalarParameter"),                      nullptr,                                     EParameterTestForm::Uniform,     TEXT("float") }, // optional default
		{ TEXT("StaticBoolParameter"),                  TEXT("true"),                                EParameterTestForm::Uniform,     TEXT("bool") },
		{ TEXT("VectorParameter"),                      TEXT("float4(0.1, 0.2, 0.3, 1.0)"),          EParameterTestForm::Uniform,     TEXT("float4") },
		{ TEXT("VectorParameter"),                      nullptr,                                     EParameterTestForm::Uniform,     TEXT("float4") }, // optional default
		{ TEXT("TextureObjectParameter"),               TEXT("Path(Game, \"Probe/T_Default\")"),     EParameterTestForm::Uniform,     TEXT("Texture2D") },
		{ TEXT("TextureObjectParameter"),               nullptr,                                     EParameterTestForm::Uniform,     TEXT("Texture2D") }, // optional default
		// Expanded at every use
		{ TEXT("StaticSwitchParameter"),                TEXT("false"),                               EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("ChannelMaskParameter"),                 TEXT("float4(1, 0, 0, 0)"),                  EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("StaticComponentMaskParameter"),         TEXT("float4(1, 1, 0, 0)"),                  EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("TextureSampleParameter2D"),             nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("TextureSampleParameter2DArray"),        nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("TextureSampleParameterCube"),           nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("TextureSampleParameterCubeArray"),      nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("TextureSampleParameterVolume"),         nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("TextureSampleParameterSubUV"),          nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("RuntimeVirtualTextureSampleParameter"), nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		{ TEXT("SparseVolumeTextureSampleParameter"),   nullptr,                                     EParameterTestForm::ExpandAtUse, nullptr },
		// No 2.0 spelling
		{ TEXT("DoubleVectorParameter"),                TEXT("float4(1, 2, 3, 4)"),                  EParameterTestForm::Unsupported, nullptr },
		{ TEXT("CurveAtlasRowParameter"),               TEXT("float3(0.5, 0.5, 0.5)"),               EParameterTestForm::Unsupported, nullptr },
		{ TEXT("DynamicParameter"),                     TEXT("float4(0, 0, 0, 0)"),                  EParameterTestForm::Unsupported, nullptr },
		{ TEXT("FontSampleParameter"),                  nullptr,                                     EParameterTestForm::Unsupported, nullptr },
		{ TEXT("SpriteTextureSampler"),                 nullptr,                                     EParameterTestForm::Unsupported, nullptr },
		{ TEXT("TextureCollectionParameter"),           nullptr,                                     EParameterTestForm::Unsupported, nullptr },
		{ TEXT("SparseVolumeTextureObjectParameter"),   nullptr,                                     EParameterTestForm::Unsupported, nullptr },
	};

	// A Shader source declaring one property per listed case, named P<index>.
	static FString BuildParameterTestSource(const TArray<int32>& CaseIndices)
	{
		FString Properties;
		for (const int32 Index : CaseIndices)
		{
			const FParameterCase& Case = GParameterCases[Index];
			if (Case.Default)
			{
				Properties += FString::Printf(TEXT("        %s P%d = %s [Group=\"Params\"; SortPriority=%d;];\n"), Case.NodeType, Index, Case.Default, Index);
			}
			else
			{
				Properties += FString::Printf(TEXT("        %s P%d [Group=\"Params\"; SortPriority=%d;];\n"), Case.NodeType, Index, Index);
			}
		}

		return FString::Printf(TEXT(
			"Shader(Name=\"DreamShaderTests/Params/M_AllParameterTypes\", Root=\"Game\")\n"
			"{\n"
			"    Properties = {\n%s    }\n"
			"    Settings = { Domain = \"Surface\"; ShadingModel = \"Unlit\"; BlendMode = \"Opaque\"; }\n"
			"    Outputs = { vec3 Color; Base.EmissiveColor = Color; }\n"
			"    Graph = { Color = vec3(0.5, 0.5, 0.5); }\n"
			"}\n"), *Properties);
	}

	/** One legacy parse: the file name only picks the front end (Auto: `.dsm` -> legacy). */
	static UE::DreamShader::Lang::FLangParseResult ParseParameterTestSource(const FString& Text, const TCHAR* FileName = TEXT("M_AllParameterTypes.dsm"))
	{
		const UE::DreamShader::Lang::FLangSourceText Source(FileName, Text);
		return UE::DreamShader::Lang::ParseDreamShaderLang(Source, UE::DreamShader::Lang::FLangParseOptions());
	}

	static FString DescribeParameterTestErrors(const UE::DreamShader::Lang::FLangParseResult& Result)
	{
		return FString::Join(
			UE::DreamShader::Editor::Private::Tests::GatherDreamShaderLangDiagnostics(Result.Diagnostics, UE::DreamShader::Lang::ELangSeverity::Error),
			TEXT(" | "));
	}

	static bool HasParameterTestError(const UE::DreamShader::Lang::FLangParseResult& Result, const TCHAR* Code)
	{
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
		{
			if (Diagnostic.Severity == UE::DreamShader::Lang::ELangSeverity::Error && Diagnostic.Code.Equals(Code, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** The file-scope `uniform` (or `static const`) declaration of that name, or null. */
	static const UE::DreamShader::Lang::FVariableDecl* FindParameterTestVariable(const UE::DreamShader::Lang::FModule& Module, const FString& Name)
	{
		for (const UE::DreamShader::Lang::FDeclPtr& Decl : Module.Declarations)
		{
			const UE::DreamShader::Lang::FVariableDecl* Variable = Decl.IsValid() ? Decl->As<UE::DreamShader::Lang::FVariableDecl>() : nullptr;
			if (Variable && Variable->Declarator.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Variable;
			}
		}
		return nullptr;
	}

	/** The expanded-at-use parameter-node declaration of that name, or null. */
	static const UE::DreamShader::Lang::FLegacyParameterDeclaration* FindParameterTestDeclaration(
		const UE::DreamShader::Lang::FLegacyMigrationInfo& Info,
		const FString& Name)
	{
		return Info.ParameterDeclarations.FindByPredicate([&Name](const UE::DreamShader::Lang::FLegacyParameterDeclaration& Candidate)
		{
			return Candidate.Name.Equals(Name, ESearchCase::CaseSensitive);
		});
	}

	/** The value of a `///` directive the legacy front end wrote on a declaration, or an empty optional. */
	static TOptional<FString> GetParameterTestDirective(const UE::DreamShader::Lang::FVariableDecl& Variable, const TCHAR* Key)
	{
		if (const UE::DreamShader::Lang::FDocDirective* Directive = Variable.Doc.Find(Key))
		{
			return Directive->Value;
		}
		return TOptional<FString>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderParameterParseAllTest,
	"DreamShader.Lang.ParameterExpressions.ParseAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderParameterParseAllTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::ParameterTests;

	TArray<int32> WithForm;
	TArray<int32> WithoutForm;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(GParameterCases); ++Index)
	{
		(GParameterCases[Index].Form == EParameterTestForm::Unsupported ? WithoutForm : WithForm).Add(Index);
	}

	const FLangParseResult Result = ParseParameterTestSource(BuildParameterTestSource(WithForm));
	if (!TestTrue(
			FString::Printf(TEXT("every parameter type with a 2.0 form parses through the legacy front end: %s"), *DescribeParameterTestErrors(Result)),
			Result.Succeeded() && Result.Legacy.IsValid()))
	{
		return false;
	}

	for (const int32 Index : WithForm)
	{
		const FParameterCase& Case = GParameterCases[Index];
		const FString Name = FString::Printf(TEXT("P%d"), Index);
		const FString Label = FString::Printf(TEXT("%s (%s)"), *Name, Case.NodeType);

		const FVariableDecl* Variable = FindParameterTestVariable(*Result.Module, Name);
		const FLegacyParameterDeclaration* Declaration = FindParameterTestDeclaration(*Result.Legacy, Name);

		if (Case.Form == EParameterTestForm::Uniform)
		{
			TestTrue(*FString::Printf(TEXT("%s is not recorded as an expanded parameter node"), *Label), Declaration == nullptr);
			if (!TestNotNull(*FString::Printf(TEXT("%s is a declaration"), *Label), Variable))
			{
				continue;
			}

			TestEqual(*FString::Printf(TEXT("%s storage is uniform"), *Label), static_cast<int32>(Variable->Storage), static_cast<int32>(EStorageClass::Uniform));
			TestTrue(
				*FString::Printf(TEXT("%s type is '%s' (actual '%s')"), *Label, Case.UniformType, *Variable->Type.Name),
				Variable->Type.Name.Equals(Case.UniformType, ESearchCase::CaseSensitive));
			TestTrue(*FString::Printf(TEXT("%s is marked legacy"), *Label), Variable->bLegacy);

			// A texture default is a `/// @default` directive; a value default is the initializer.
			const bool bHasDefault = Variable->Type.IsTexture()
				? Variable->Doc.Has(TEXT("default"))
				: Variable->Declarator.Initializer.IsValid();
			TestEqual(*FString::Printf(TEXT("%s carries a default exactly when one was written"), *Label), bHasDefault, Case.Default != nullptr);

			if (FCString::Strcmp(Case.NodeType, TEXT("StaticBoolParameter")) == 0)
			{
				TestTrue(*FString::Printf(TEXT("%s is a /// @static uniform"), *Label), Variable->Doc.Has(TEXT("static")));
			}

			const TOptional<FString> Group = GetParameterTestDirective(*Variable, TEXT("group"));
			TestTrue(
				*FString::Printf(TEXT("%s Group=\"Params\" becomes /// @group Params"), *Label),
				Group.IsSet() && Group.GetValue().Equals(TEXT("Params"), ESearchCase::CaseSensitive));
			const TOptional<FString> Sort = GetParameterTestDirective(*Variable, TEXT("sort"));
			TestTrue(
				*FString::Printf(TEXT("%s SortPriority=%d becomes /// @sort %d"), *Label, Index, Index),
				Sort.IsSet() && Sort.GetValue().Equals(FString::FromInt(Index), ESearchCase::CaseSensitive));
		}
		else
		{
			TestTrue(*FString::Printf(TEXT("%s makes no declaration"), *Label), Variable == nullptr);
			if (!TestNotNull(*FString::Printf(TEXT("%s is recorded as a parameter node expanded at its uses"), *Label), Declaration))
			{
				continue;
			}

			TestTrue(
				*FString::Printf(TEXT("%s NodeType is '%s' (actual '%s')"), *Label, Case.NodeType, *Declaration->NodeType),
				Declaration->NodeType.Equals(Case.NodeType, ESearchCase::CaseSensitive));
			TestEqual(
				*FString::Printf(TEXT("%s keeps its default text exactly when one was written"), *Label),
				!Declaration->DefaultText.IsEmpty(),
				Case.Default != nullptr);
			TestTrue(
				*FString::Printf(TEXT("%s keeps its 1.x metadata"), *Label),
				Declaration->Metadata.ContainsByPredicate([](const TPair<FString, FString>& Entry)
				{
					return Entry.Key.Equals(TEXT("Group"), ESearchCase::IgnoreCase);
				}));
		}
	}

	// A type with no 2.0 spelling is refused on its own, so one refusal cannot hide another.
	for (const int32 Index : WithoutForm)
	{
		const FParameterCase& Case = GParameterCases[Index];
		const FLangParseResult Refused = ParseParameterTestSource(BuildParameterTestSource({ Index }));
		TestTrue(
			*FString::Printf(TEXT("%s has no 2.0 spelling: DSH3253 (actual: %s)"), Case.NodeType, *DescribeParameterTestErrors(Refused)),
			HasParameterTestError(Refused, TEXT("DSH3253")));
	}

	return true;
}

// Group("X") { ... } Properties scope: stamps the group + an auto-incrementing SortPriority (step 10, global
// counter; explicit values win and don't consume a slot); a loose parameter gets the synthesized `@sort 32` that
// stands for 1.x's "no SortPriority written". Also pins the Slider(min,max) shorthand and
// asset-in-= (a bare quoted absolute path) on an expanded texture-sample parameter.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPropertyGroupScopeTest,
	"DreamShader.Lang.ParameterExpressions.GroupScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPropertyGroupScopeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::ParameterTests;

	const FString Source = TEXT(R"(
Shader(Name="DreamShaderTests/Params/M_GroupScope", Root="Game")
{
    Properties {
        Group("Surface") {
            ScalarParameter A = 0.5 [Slider(0, 1)];
            VectorParameter B = float4(1, 1, 1, 1);
        }
        Group("Detail") {
            ScalarParameter C = 1.0 [SortPriority=99;];
            ScalarParameter D = 2.0;
        }
        ScalarParameter Loose = 3.0;
        TextureSampleParameter2D Tex = "/Engine/EngineResources/WhiteSquareTexture";
    }
    Settings { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs { vec3 Color; Base.EmissiveColor = Color; }
    Graph { Color = vec3(A, A, A); }
}
)");

	const FLangParseResult Result = ParseParameterTestSource(Source, TEXT("M_GroupScope.dsm"));
	if (!TestTrue(FString::Printf(TEXT("Group-scope source parses: %s"), *DescribeParameterTestErrors(Result)), Result.Succeeded() && Result.Legacy.IsValid()))
	{
		return false;
	}

	const FVariableDecl* A = FindParameterTestVariable(*Result.Module, TEXT("A"));
	const FVariableDecl* B = FindParameterTestVariable(*Result.Module, TEXT("B"));
	const FVariableDecl* C = FindParameterTestVariable(*Result.Module, TEXT("C"));
	const FVariableDecl* D = FindParameterTestVariable(*Result.Module, TEXT("D"));
	const FVariableDecl* Loose = FindParameterTestVariable(*Result.Module, TEXT("Loose"));
	const FLegacyParameterDeclaration* Tex = FindParameterTestDeclaration(*Result.Legacy, TEXT("Tex"));
	if (!TestNotNull(TEXT("A"), A) || !TestNotNull(TEXT("B"), B) || !TestNotNull(TEXT("C"), C)
		|| !TestNotNull(TEXT("D"), D) || !TestNotNull(TEXT("Loose"), Loose) || !TestNotNull(TEXT("Tex"), Tex))
	{
		return false;
	}

	const auto DirectiveIs = [](const FVariableDecl& Variable, const TCHAR* Key, const TCHAR* Expected) -> bool
	{
		const TOptional<FString> Value = GetParameterTestDirective(Variable, Key);
		return Value.IsSet() && Value.GetValue().Equals(Expected, ESearchCase::CaseSensitive);
	};

	// Group stamping.
	TestTrue(TEXT("A inherits /// @group Surface"), DirectiveIs(*A, TEXT("group"), TEXT("Surface")));
	TestTrue(TEXT("B inherits /// @group Surface"), DirectiveIs(*B, TEXT("group"), TEXT("Surface")));
	TestTrue(TEXT("C inherits /// @group Detail"), DirectiveIs(*C, TEXT("group"), TEXT("Detail")));
	TestTrue(TEXT("D inherits /// @group Detail"), DirectiveIs(*D, TEXT("group"), TEXT("Detail")));
	TestFalse(TEXT("the loose parameter keeps no group"), Loose->Doc.Has(TEXT("group")));

	// Auto SortPriority: global counter, step 10; the explicit value (C) wins and does not consume a slot.
	TestTrue(TEXT("A /// @sort 0"), DirectiveIs(*A, TEXT("sort"), TEXT("0")));
	TestTrue(TEXT("B /// @sort 10"), DirectiveIs(*B, TEXT("sort"), TEXT("10")));
	TestTrue(TEXT("C keeps the explicit /// @sort 99"), DirectiveIs(*C, TEXT("sort"), TEXT("99")));
	TestTrue(TEXT("D /// @sort 20 (the explicit C did not consume the counter)"), DirectiveIs(*D, TEXT("sort"), TEXT("20")));

	// No SortPriority written and no group: 1.x left the engine default, which the legacy front end spells out.
	TestTrue(TEXT("the loose parameter gets the synthesized /// @sort 32"), DirectiveIs(*Loose, TEXT("sort"), TEXT("32")));
	TestTrue(
		TEXT("and the synthesized directive is recorded for migrate"),
		Result.Legacy->SynthesizedDirectives.ContainsByPredicate([Loose](const FLegacySynthesizedDirective& Directive)
		{
			return Directive.Decl == Loose
				&& Directive.Key.Equals(TEXT("sort"), ESearchCase::CaseSensitive)
				&& Directive.Value.Equals(TEXT("32"), ESearchCase::CaseSensitive);
		}));

	// Slider(0, 1) shorthand -> one /// @slider directive with both bounds.
	TestTrue(TEXT("Slider(0, 1) becomes /// @slider 0 1"), DirectiveIs(*A, TEXT("slider"), TEXT("0 1")));

	// asset-in-= via a bare quoted absolute path, on a parameter node expanded at its uses.
	TestTrue(
		FString::Printf(TEXT("Tex keeps the texture path it was declared with (actual '%s')"), *Tex->DefaultText),
		Tex->DefaultText.Contains(TEXT("/Engine/EngineResources/WhiteSquareTexture")));

	return true;
}

// Nested Group("Outer") { Group("Inner") { ... } } composes into "Outer|Inner", matching Unreal's native '|'
// sub-category syntax; a sibling statement directly inside the outer group keeps just the outer name, and
// Group("A|B") typed as a single literal name is passed through unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderPropertyNestedGroupScopeTest,
	"DreamShader.Lang.ParameterExpressions.NestedGroupScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderPropertyNestedGroupScopeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::ParameterTests;

	const FString Source = TEXT(R"(
Shader(Name="DreamShaderTests/Params/M_NestedGroupScope", Root="Game")
{
    Properties {
        Group("Surface") {
            Group("SS") {
                ScalarParameter Test = 0.5 [Slider(0, 1)];
            }
            ScalarParameter Rough = 0.2;
        }
        Group("Manual|Literal") {
            ScalarParameter Explicit = 1.0;
        }
    }
    Settings { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs { vec3 Color; Base.EmissiveColor = Color; }
    Graph { Color = vec3(Rough, Rough, Rough); }
}
)");

	const FLangParseResult Result = ParseParameterTestSource(Source, TEXT("M_NestedGroupScope.dsm"));
	if (!TestTrue(FString::Printf(TEXT("Nested-group-scope source parses: %s"), *DescribeParameterTestErrors(Result)), Result.Succeeded()))
	{
		return false;
	}

	const FVariableDecl* Test = FindParameterTestVariable(*Result.Module, TEXT("Test"));
	const FVariableDecl* Rough = FindParameterTestVariable(*Result.Module, TEXT("Rough"));
	const FVariableDecl* Explicit = FindParameterTestVariable(*Result.Module, TEXT("Explicit"));
	if (!TestNotNull(TEXT("Test"), Test) || !TestNotNull(TEXT("Rough"), Rough) || !TestNotNull(TEXT("Explicit"), Explicit))
	{
		return false;
	}

	const auto GroupOf = [](const FVariableDecl& Variable) -> FString
	{
		const TOptional<FString> Value = GetParameterTestDirective(Variable, TEXT("group"));
		return Value.IsSet() ? Value.GetValue() : FString(TEXT("<none>"));
	};

	TestEqual(TEXT("Test (nested Group(\"SS\") inside Group(\"Surface\")) composes to 'Surface|SS'"), GroupOf(*Test), FString(TEXT("Surface|SS")));
	TestEqual(TEXT("Rough (direct child of Group(\"Surface\")) keeps just 'Surface'"), GroupOf(*Rough), FString(TEXT("Surface")));
	TestEqual(TEXT("Explicit (single literal 'Manual|Literal' name) passes through unchanged"), GroupOf(*Explicit), FString(TEXT("Manual|Literal")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
