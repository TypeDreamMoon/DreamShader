// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Outputs block form -- Expression(Class="...", args...) { Pin[0] = a; Pin[1] = b; } -- added in
// 1.9.0 for GitHub issues #30 / #33.
//
// The whole point of the form is a GUARANTEE that cannot be seen in the source alone: however many pins the
// block binds, the material ends up with exactly ONE terminal node. In 1.x it fell out of the parser lowering
// every pin to a binding with a byte-identical ExpressionClass + ExpressionArguments. The legacy front end
// keeps it in the tree: every pin whose head has an equal class and argument list becomes a
// `Pin[i] = source` argument (FArgument::PinIndex) of ONE `UE.Expression(Class = ...)` statement call, so the
// parse layer asserts the merge and the graph layer asserts the one node.
//
// Layers here:
//   DreamShader.Lang.OutputsBlock.*  parse only (the legacy front end), fast -- the merged call and the diagnostics.
//   DreamShader.Gen.Graph.*          compiles a material and counts nodes; needs the editor.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderTestCommon.h"

#include "Decompiler/DreamShaderDecompileService.h"
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "DreamShaderCompilerService.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"

#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private::OutputsBlockTests
{
	// The reflected class name, without the U prefix -- what GetClass()->GetName() answers and what
	// the decompiler writes into Class="...". See Docs/language/output-bindings.md.
	static const TCHAR* ThinTranslucentClassName = TEXT("MaterialExpressionThinTranslucentMaterialOutput");

	static FString MakeBlockFormSource()
	{
		return TEXT(R"(
Shader(Name="DreamShaderTests/OutputsBlock/M_Parse", Root="Game")
{
    Settings { Domain = "Volume"; BlendMode = "Additive"; }
    Outputs {
        vec3  Color;
        float PhaseG;
        float PhaseG2;
        float PhaseBlend;

        Base.EmissiveColor = Color;

        Expression(Class="VolumetricAdvancedMaterialOutput",
            PerSamplePhaseEvaluation="false",
            bGroundContribution="false")
        {
            Pin[0] = PhaseG;

            Pin[1] = PhaseG2;
            Pin[2] = PhaseBlend;
        };
    }
    Graph {
        Color = vec3(0.5, 0.6, 0.7);
        PhaseG = 0.5;
        PhaseG2 = 0.2;
        PhaseBlend = 0.1;
    }
}
)");
	}

	/** Wrap an Outputs body in the smallest Shader that still parses. */
	static FString WrapOutputs(const TCHAR* OutputsBody, const TCHAR* GraphBody)
	{
		return FString::Printf(TEXT(
			"Shader(Name=\"DreamShaderTests/OutputsBlock/M_Case\", Root=\"Game\")\n"
			"{\n"
			"    Settings { Domain = \"Surface\"; ShadingModel = \"Unlit\"; BlendMode = \"Translucent\"; }\n"
			"    Outputs {\n%s    }\n"
			"    Graph {\n%s    }\n"
			"}\n"), OutputsBody, GraphBody);
	}

	/** One parse through the legacy front end (Auto picks it for a `.dsm`). */
	static UE::DreamShader::Lang::FLangParseResult ParseOutputsBlockSource(const FString& Source)
	{
		const UE::DreamShader::Lang::FLangSourceText Text(TEXT("M_OutputsBlockCase.dsm"), Source);
		return UE::DreamShader::Lang::ParseDreamShaderLang(Text, UE::DreamShader::Lang::FLangParseOptions());
	}

	/** Parse and return the DSHnnnn code of the first error, or an empty string when it parsed. */
	static FString ParseForCode(const FString& Source, FString& OutMessage)
	{
		const UE::DreamShader::Lang::FLangParseResult Result = ParseOutputsBlockSource(Source);
		for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Result.Diagnostics.GetDiagnostics())
		{
			if (Diagnostic.Severity == UE::DreamShader::Lang::ELangSeverity::Error)
			{
				OutMessage = UE::DreamShader::Lang::FLangDiagnosticSink::ToWireString(Diagnostic);
				return Diagnostic.Code;
			}
		}

		OutMessage.Reset();
		return FString();
	}

	/** The material entry the legacy front end made of a Shader block, or null. */
	static const UE::DreamShader::Lang::FFunctionDecl* FindOutputsBlockEntry(const UE::DreamShader::Lang::FModule& Module)
	{
		for (const UE::DreamShader::Lang::FDeclPtr& Decl : Module.Declarations)
		{
			const UE::DreamShader::Lang::FFunctionDecl* Function = Decl.IsValid() ? Decl->As<UE::DreamShader::Lang::FFunctionDecl>() : nullptr;
			if (Function && Function->IsMaterialEntry() && Function->Body.IsValid())
			{
				return Function;
			}
		}
		return nullptr;
	}

	/** The `Class = "..."` string of a `UE.Expression(...)` call, or empty when the call is anything else. */
	static FString GetOutputsBlockExpressionClass(const UE::DreamShader::Lang::FCallExpr& Call)
	{
		using namespace UE::DreamShader::Lang;

		const FMemberExpr* Callee = Call.Callee.IsValid() ? Call.Callee->As<FMemberExpr>() : nullptr;
		const FIdentifierExpr* Namespace = (Callee && Callee->Object.IsValid()) ? Callee->Object->As<FIdentifierExpr>() : nullptr;
		if (!Namespace
			|| !Namespace->Name.Equals(TEXT("UE"), ESearchCase::CaseSensitive)
			|| !Callee->Member.Equals(TEXT("Expression"), ESearchCase::CaseSensitive))
		{
			return FString();
		}

		for (const FArgument& Argument : Call.Arguments)
		{
			if (Argument.Name.Equals(TEXT("Class"), ESearchCase::IgnoreCase) && Argument.Value.IsValid())
			{
				if (const FLiteralExpr* Literal = Argument.Value->As<FLiteralExpr>())
				{
					return Literal->Text;
				}
			}
		}
		return FString();
	}

	/** Every statement call of the entry body that targets `UE.Expression(Class = "<ClassName>")`. */
	static TArray<const UE::DreamShader::Lang::FCallExpr*> FindOutputsBlockTargetCalls(
		const UE::DreamShader::Lang::FFunctionDecl& Entry,
		const TCHAR* ClassName)
	{
		using namespace UE::DreamShader::Lang;

		TArray<const FCallExpr*> Calls;
		for (const FStmtPtr& Statement : Entry.Body->Statements)
		{
			const FExprStmt* ExpressionStatement = Statement.IsValid() ? Statement->As<FExprStmt>() : nullptr;
			const FCallExpr* Call = (ExpressionStatement && ExpressionStatement->Expression.IsValid())
				? ExpressionStatement->Expression->As<FCallExpr>()
				: nullptr;
			if (Call && GetOutputsBlockExpressionClass(*Call).Equals(ClassName, ESearchCase::CaseSensitive))
			{
				Calls.Add(Call);
			}
		}
		return Calls;
	}

	/** Every expression node of the given reflected class name that the material owns. */
	static TArray<UMaterialExpressionCustomOutput*> FindCustomOutputs(const UMaterial* Material, const TCHAR* ClassName)
	{
		TArray<UMaterialExpressionCustomOutput*> Found;
		if (!Material)
		{
			return Found;
		}

		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			UMaterialExpressionCustomOutput* CustomOutput = Cast<UMaterialExpressionCustomOutput>(Expression);
			if (CustomOutput && CustomOutput->GetClass()->GetName() == ClassName)
			{
				Found.Add(CustomOutput);
			}
		}
		return Found;
	}

	static FString GetGenerateCorpusFixturePath(const TCHAR* FileName)
	{
		const FString Root = UE::DreamShader::Editor::Private::Tests::GetDreamShaderCorpusRoot();
		// The Generate corpus is the Legacy compile layer now (Tests/Corpus/Legacy/Compile).
		return Root.IsEmpty() ? FString() : FPaths::Combine(Root, TEXT("Legacy"), TEXT("Compile"), TEXT("Material"), FileName);
	}

	/**
	 * Copy the M_OutputsBlock fixture into the test's scratch fixture, retargeted so its asset lands under the fixture's
	 * package path (a 1.x destination follows Name=, not the source folder), and write it as the fixture's main source.
	 */
	static bool WriteOutputsBlockFixture(
		FAutomationTestBase& Test,
		UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2Fixture& Fixture,
		const TCHAR* AssetName)
	{
		const FString FixturePath = GetGenerateCorpusFixturePath(TEXT("M_OutputsBlock.dsm"));
		FString FixtureText;
		if (!Test.TestTrue(TEXT("the M_OutputsBlock corpus fixture is readable"), !FixturePath.IsEmpty() && FFileHelper::LoadFileToString(FixtureText, *FixturePath)))
		{
			return false;
		}

		FString Retargeted;
		if (!Test.TestTrue(
				TEXT("the fixture's Shader block can be pointed at the scratch package path"),
				UE::DreamShader::Editor::Private::Tests::RetargetDreamShaderLegacyBlockName(FixtureText, Fixture.MakeLegacyAssetName(AssetName), Retargeted)))
		{
			return false;
		}
		return Fixture.WriteSource(Test, Retargeted);
	}
}

// ---------------------------------------------------------------------------------------------
// Parse layer
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderOutputsBlockLoweringTest,
	"DreamShader.Lang.OutputsBlock.LowersToOneNodeKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderOutputsBlockLoweringTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::OutputsBlockTests;

	const FLangParseResult Result = ParseOutputsBlockSource(MakeBlockFormSource());
	if (!TestTrue(
			FString::Printf(TEXT("block-form source parses: %s"),
				*FString::Join(UE::DreamShader::Editor::Private::Tests::GatherDreamShaderLangDiagnostics(Result.Diagnostics, ELangSeverity::Error), TEXT(" | "))),
			Result.Succeeded()))
	{
		return false;
	}

	const FFunctionDecl* Entry = FindOutputsBlockEntry(*Result.Module);
	if (!TestNotNull(TEXT("the Shader block became a material entry"), Entry))
	{
		return false;
	}

	// THE contract: one node key, one statement call, however many pins the block binds.
	const TArray<const FCallExpr*> Calls = FindOutputsBlockTargetCalls(*Entry, TEXT("VolumetricAdvancedMaterialOutput"));
	if (!TestEqual(TEXT("the block lowers to exactly one UE.Expression(Class = \"VolumetricAdvancedMaterialOutput\") call"), Calls.Num(), 1))
	{
		return false;
	}

	TArray<const FArgument*> Pins;
	TArray<FString> HeadArguments;
	for (const FArgument& Argument : Calls[0]->Arguments)
	{
		if (Argument.PinIndex != INDEX_NONE)
		{
			Pins.Add(&Argument);
		}
		else
		{
			HeadArguments.Add(Argument.Name);
		}
	}

	// The head is written once, on the call, not once per pin.
	TestEqual(TEXT("head arguments kept once (Class + two properties)"), HeadArguments.Num(), 3);
	TestTrue(TEXT("the head keeps PerSamplePhaseEvaluation"), HeadArguments.ContainsByPredicate([](const FString& Name) { return Name.Equals(TEXT("PerSamplePhaseEvaluation"), ESearchCase::CaseSensitive); }));
	TestTrue(TEXT("the head keeps bGroundContribution"), HeadArguments.ContainsByPredicate([](const FString& Name) { return Name.Equals(TEXT("bGroundContribution"), ESearchCase::CaseSensitive); }));

	if (!TestEqual(TEXT("one Pin[i] argument per pin"), Pins.Num(), 3))
	{
		return false;
	}

	static const TCHAR* const ExpectedSources[] = { TEXT("PhaseG"), TEXT("PhaseG2"), TEXT("PhaseBlend") };
	for (int32 Index = 0; Index < Pins.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("pin argument %d selects Pin[%d]"), Index, Index), Pins[Index]->PinIndex, Index);
		TestTrue(*FString::Printf(TEXT("pin argument %d is unnamed"), Index), Pins[Index]->Name.IsEmpty());

		const FIdentifierExpr* Source = Pins[Index]->Value.IsValid() ? Pins[Index]->Value->As<FIdentifierExpr>() : nullptr;
		TestTrue(
			*FString::Printf(TEXT("pin %d binds '%s' (sources bound in written order)"), Index, ExpectedSources[Index]),
			Source && Source->Name.Equals(ExpectedSources[Index], ESearchCase::CaseSensitive));
	}

	// Every diagnostic about a pin quotes the pin's own line, so each argument keeps the span of its `Pin[i] = x`.
	TestTrue(TEXT("the last pin's span is below the first pin's"), Pins[2]->Span.Line > Pins[0]->Span.Line);

	// The plain binding beside the block stays an assignment to the material.
	int32 BaseBindings = 0;
	for (const FStmtPtr& Statement : Entry->Body->Statements)
	{
		const FExprStmt* ExpressionStatement = Statement.IsValid() ? Statement->As<FExprStmt>() : nullptr;
		const FAssignExpr* Assign = (ExpressionStatement && ExpressionStatement->Expression.IsValid()) ? ExpressionStatement->Expression->As<FAssignExpr>() : nullptr;
		const FMemberExpr* Target = (Assign && Assign->Target.IsValid()) ? Assign->Target->As<FMemberExpr>() : nullptr;
		if (Target && Target->Member.Equals(TEXT("EmissiveColor"), ESearchCase::CaseSensitive))
		{
			++BaseBindings;
		}
	}
	TestEqual(TEXT("Base.EmissiveColor = Color stays one assignment"), BaseBindings, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderOutputsBlockDiagnosticsTest,
	"DreamShader.Lang.OutputsBlock.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The 1.x parser's DSH3133-3137 retired with it; the legacy front end reports the same mistakes under its own codes:
// DSH3266 a statement Outputs does not know, DSH3267 a malformed output target,
// DSH3268 a pin bound twice, DSH3269 an empty target block.
bool FDreamShaderOutputsBlockDiagnosticsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::OutputsBlockTests;

	struct FCase
	{
		const TCHAR* Label;
		const TCHAR* ExpectedCode;   // empty string = must parse
		const TCHAR* OutputsBody;
		const TCHAR* GraphBody;
	};

	static const FCase Cases[] =
	{
		{
			TEXT("brace after a plain variable"), TEXT("DSH3266"),
			TEXT("        vec3 Color;\n        Color\n        {\n            Pin[0] = Color;\n        }\n        Base.EmissiveColor = Color;\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		{
			// A `.Pin[i]` suffix belongs to the statement form; the block writes its pins inside.
			TEXT("brace after a pin-selecting target"), TEXT("DSH3267"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\").Pin[0]\n        {\n            Pin[1] = Color;\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		{
			TEXT("non-pin statement inside a block"), TEXT("DSH3267"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            float Extra;\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		{
			TEXT("empty block"), TEXT("DSH3269"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            // nothing\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		{
			TEXT("same pin twice inside one block"), TEXT("DSH3268"),
			TEXT("        vec3 Color;\n        vec3 A;\n        vec3 B;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            Pin[0] = A;\n            Pin[0] = B;\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n        A = vec3(1.0, 1.0, 1.0);\n        B = vec3(0.0, 0.0, 0.0);\n")
		},
		{
			// The cross-form rule: the block bound Pin[0], the loose statement names the same node.
			TEXT("block pin re-bound by a statement"), TEXT("DSH3268"),
			TEXT("        vec3 Color;\n        vec3 A;\n        vec3 B;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            Pin[0] = A;\n        }\n        Expression(Class=\"ThinTranslucentMaterialOutput\").Pin[0] = B;\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n        A = vec3(1.0, 1.0, 1.0);\n        B = vec3(0.0, 0.0, 0.0);\n")
		},
		{
			// ...and the same rule between two loose statements.
			TEXT("statement pin re-bound by a statement"), TEXT("DSH3268"),
			TEXT("        vec3 Color;\n        vec3 A;\n        vec3 B;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\").Pin[0] = A;\n        Expression(Class=\"ThinTranslucentMaterialOutput\").Pin[0] = B;\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n        A = vec3(1.0, 1.0, 1.0);\n        B = vec3(0.0, 0.0, 0.0);\n")
		},
		{
			// A different argument list is a different node, so Pin[0] on each is legal.
			TEXT("different argument list is a different node"), TEXT(""),
			TEXT("        vec3 Color;\n        vec3 A;\n        vec3 B;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            Pin[0] = A;\n        }\n        Expression(Class=\"ClearCoatNormalCustomOutput\").Pin[0] = B;\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n        A = vec3(1.0, 1.0, 1.0);\n        B = vec3(0.0, 0.0, 1.0);\n")
		},
		{
			// A trailing ';' after the closing brace is tolerated, like Group("Name") { ... }; is.
			TEXT("trailing semicolon after the block"), TEXT(""),
			TEXT("        vec3 Color;\n        vec3 A;\n        float C;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            Pin[0] = A;\n            Pin[1] = C;\n        };\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n        A = vec3(1.0, 1.0, 1.0);\n        C = 1.0;\n")
		},
	};

	for (const FCase& Case : Cases)
	{
		FString Message;
		const FString Code = ParseForCode(WrapOutputs(Case.OutputsBody, Case.GraphBody), Message);
		if (FCString::Strlen(Case.ExpectedCode) == 0)
		{
			TestTrue(
				*FString::Printf(TEXT("%s parses (got %s)"), Case.Label, *Message),
				Code.IsEmpty());
			continue;
		}

		if (!TestEqual(*FString::Printf(TEXT("%s raises the right code"), Case.Label), Code, FString(Case.ExpectedCode)))
		{
			AddInfo(FString::Printf(TEXT("%s actual first error: %s"), Case.Label, *Message));
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// Generate layer
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderOutputsBlockSingleNodeTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Gen.Graph.OutputsBlockSingleNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderOutputsBlockSingleNodeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::OutputsBlockTests;

	// LoadObject<UMaterial> only answers under the Graph backend; the default is ThinCustom.
	FScopedDreamShaderGraphBackendPin BackendPin;

	// A Graph material always saves, so the fixture compiles in a scratch package root that deletes it.
	FDreamShaderCompile2Fixture Fixture(TEXT("OutputsBlockSingleNode"), TEXT("Automation"), TEXT("dsm"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	if (!WriteOutputsBlockFixture(*this, Fixture, TEXT("M_OutputsBlock")))
	{
		return false;
	}

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("block-form material compiles: %s"), *Message),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestMaterial(Fixture.GetSourceFilePath(), Message, /*bForce*/ true, /*bEphemeralThinCustom*/ true)))
	{
		return false;
	}

	const FString ObjectPath = Fixture.MakeObjectPath(TEXT("M_OutputsBlock"));
	Fixture.TrackObjectPath(ObjectPath);
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("compiled material loads"), Material))
	{
		return false;
	}

	const TArray<UMaterialExpressionCustomOutput*> Nodes = FindCustomOutputs(Material, ThinTranslucentClassName);
	if (!TestEqual(TEXT("both pins share exactly one terminal node"), Nodes.Num(), 1))
	{
		return false;
	}

	FExpressionInput* Pin0 = Nodes[0]->GetInput(0);
	FExpressionInput* Pin1 = Nodes[0]->GetInput(1);
	if (TestNotNull(TEXT("terminal node has Pin[0]"), Pin0))
	{
		TestTrue(TEXT("Pin[0] is wired"), Pin0->IsConnected());
	}
	if (TestNotNull(TEXT("terminal node has Pin[1]"), Pin1))
	{
		TestTrue(TEXT("Pin[1] is wired"), Pin1->IsConnected());
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderOutputsBlockRoundtripTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Gen.Graph.OutputsBlockRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Compile -> decompile (the 1.x text decompiler, kept behind -Format=Legacy) -> the decompiled text uses the BLOCK
// form -> compile again through the legacy front end -> still one node. The middle step is the one that would silently
// regress: a decompiler that kept emitting N loose statements still round-trips, so only an assertion on the emitted
// syntax catches it.
bool FDreamShaderOutputsBlockRoundtripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::OutputsBlockTests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	FDreamShaderCompile2Fixture Fixture(TEXT("OutputsBlockRoundtrip"), TEXT("Automation"), TEXT("dsm"));
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

	if (!WriteOutputsBlockFixture(*this, Fixture, TEXT("M_OutputsBlock")))
	{
		return false;
	}

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("block-form material compiles: %s"), *Message),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestMaterial(Fixture.GetSourceFilePath(), Message, /*bForce*/ true, /*bEphemeralThinCustom*/ true)))
	{
		return false;
	}

	const FString ObjectPath = Fixture.MakeObjectPath(TEXT("M_OutputsBlock"));
	Fixture.TrackObjectPath(ObjectPath);
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *ObjectPath);
	if (!TestNotNull(TEXT("compiled material loads"), Material))
	{
		return false;
	}

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("decompile succeeds: %s"), *DecompileError),
		GetGraphDecompiler().DecompileMaterial(
			Material, Fixture.MakeLegacyAssetName(TEXT("M_Roundtrip")), DecompiledSource, DecompileError)))
	{
		return false;
	}

	// Two bound pins -> the block form, not two statements.
	const FString BlockHead = FString::Printf(TEXT("Expression(Class=\"%s\")\n\t\t{"), ThinTranslucentClassName);
	const FString StatementHead = FString::Printf(TEXT("Expression(Class=\"%s\").Pin["), ThinTranslucentClassName);
	TestTrue(TEXT("decompile emits the block form for a two-pin terminal node"), DecompiledSource.Contains(BlockHead));
	TestFalse(TEXT("decompile does not fall back to the statement form"), DecompiledSource.Contains(StatementHead));
	TestTrue(TEXT("block binds Pin[0]"), DecompiledSource.Contains(TEXT("\t\t\tPin[0] = ")));
	TestTrue(TEXT("block binds Pin[1]"), DecompiledSource.Contains(TEXT("\t\t\tPin[1] = ")));

	// Written beside the fixture's source: a legacy source compiles from under a DShader root, and the fixture deletes it.
	FString RoundtripPath;
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_OutputsBlockRoundtrip.dsm"), DecompiledSource, RoundtripPath))
	{
		return false;
	}

	FString RoundtripMessage;
	if (!TestTrue(
		FString::Printf(TEXT("decompiled block form compiles again: %s"), *RoundtripMessage),
		::UE::DreamShader::Editor::Private::Tests::CompileDreamShaderTestMaterial(RoundtripPath, RoundtripMessage, /*bForce*/ true, /*bEphemeralThinCustom*/ true)))
	{
		AddInfo(FString::Printf(TEXT("decompiled source:\n%s"), *DecompiledSource));
		return false;
	}

	const FString RoundtripObjectPath = Fixture.MakeObjectPath(TEXT("M_Roundtrip"));
	Fixture.TrackObjectPath(RoundtripObjectPath);
	UMaterial* Roundtripped = LoadObject<UMaterial>(nullptr, *RoundtripObjectPath);
	if (!TestNotNull(TEXT("re-compiled material loads"), Roundtripped))
	{
		return false;
	}

	const TArray<UMaterialExpressionCustomOutput*> Nodes = FindCustomOutputs(Roundtripped, ThinTranslucentClassName);
	if (!TestEqual(TEXT("round trip still produces exactly one terminal node"), Nodes.Num(), 1))
	{
		return false;
	}

	FExpressionInput* Pin0 = Nodes[0]->GetInput(0);
	FExpressionInput* Pin1 = Nodes[0]->GetInput(1);
	if (TestNotNull(TEXT("round-tripped node has Pin[0]"), Pin0))
	{
		TestTrue(TEXT("round-tripped Pin[0] is wired"), Pin0->IsConnected());
	}
	if (TestNotNull(TEXT("round-tripped node has Pin[1]"), Pin1))
	{
		TestTrue(TEXT("round-tripped Pin[1] is wired"), Pin1->IsConnected());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
