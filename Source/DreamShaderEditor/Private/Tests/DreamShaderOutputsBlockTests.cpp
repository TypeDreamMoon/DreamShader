// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The Outputs block form -- Expression(Class="...", args...) { Pin[0] = a; Pin[1] = b; } -- added in
// 1.9.0 for GitHub issues #30 / #33.
//
// The whole point of the form is a GUARANTEE that cannot be seen in the parse tree alone: however
// many pins the block binds, the material ends up with exactly ONE terminal node. That guarantee is
// not implemented in the generator -- it falls out of the parser lowering every pin to a binding
// with a byte-identical ExpressionClass + ExpressionArguments, which is the generator's existing
// output-target reuse key. So it has to be asserted where it is observable: on the generated graph.
//
// Layers here:
//   DreamShader.Lang.OutputsBlock.*  parse only, fast -- lowering shape and the new diagnostics.
//   DreamShader.Gen.Graph.*          generates a material and counts nodes; needs the editor.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderParser.h"
#include "DreamShaderTypes.h"
#include "DreamShaderTestCommon.h"

#include "Decompiler/DreamShaderDecompileService.h"
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGenerator.h"

#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private::OutputsBlockTests
{
	using namespace UE::DreamShader;

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

	/** Parse and return the DSHnnnn code of the failure, or an empty string when it parsed. */
	static FString ParseForCode(const FString& Source, FString& OutMessage)
	{
		FTextShaderDefinition Definition;
		FDreamShaderTextError Error;
		if (FTextShaderParser::Parse(Source, Definition, Error))
		{
			OutMessage.Reset();
			return FString();
		}

		OutMessage = Error.Message.ToString();
		return Error.Code;
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
		return Root.IsEmpty() ? FString() : FPaths::Combine(Root, TEXT("Generate"), TEXT("Material"), FileName);
	}

	/** A scratch .dsm under Intermediate/, deleted on scope exit. Never inside the DShader source
	 *  root: writing there would wake the editor's source watcher mid-test. */
	struct FScopedScratchSource
	{
		FString FilePath;

		bool Write(FAutomationTestBase& Test, const TCHAR* FileName, const FString& SourceText)
		{
			FilePath = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("DreamShaderTests"), FileName));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
			if (!FFileHelper::SaveStringToFile(SourceText, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				Test.AddError(TEXT("Failed to write the scratch DreamShader source file."));
				FilePath.Reset();
				return false;
			}
			return true;
		}

		~FScopedScratchSource()
		{
			if (!FilePath.IsEmpty())
			{
				IFileManager::Get().Delete(*FilePath, false, true);
			}
		}
	};
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
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private::OutputsBlockTests;

	FTextShaderDefinition Definition;
	FString ParseError;
	if (!TestTrue(FString::Printf(TEXT("block-form source parses: %s"), *ParseError),
		FTextShaderParser::Parse(MakeBlockFormSource(), Definition, ParseError)))
	{
		return false;
	}

	// One Base.* binding plus one binding per pin.
	if (!TestEqual(TEXT("block form lowers to one binding per pin"), Definition.Outputs.Num(), 4))
	{
		return false;
	}

	TArray<const FTextShaderOutputBinding*> PinBindings;
	for (const FTextShaderOutputBinding& Binding : Definition.Outputs)
	{
		if (Binding.TargetKind == FTextShaderOutputBinding::ETargetKind::ExpressionInput)
		{
			PinBindings.Add(&Binding);
		}
	}

	if (!TestEqual(TEXT("three expression-input bindings"), PinBindings.Num(), 3))
	{
		return false;
	}

	// THE contract: identical class + identical argument map on every pin, because that pair is the
	// generator's output-target reuse key. Anything that diverges here splits the node in two.
	for (int32 Index = 0; Index < PinBindings.Num(); ++Index)
	{
		const FTextShaderOutputBinding& Binding = *PinBindings[Index];
		TestEqual(
			*FString::Printf(TEXT("pin %d resolves the same class"), Index),
			Binding.ExpressionClass,
			PinBindings[0]->ExpressionClass);
		TestEqual(
			*FString::Printf(TEXT("pin %d carries the same argument count"), Index),
			Binding.ExpressionArguments.Num(),
			PinBindings[0]->ExpressionArguments.Num());
		for (const TPair<FString, FString>& Argument : PinBindings[0]->ExpressionArguments)
		{
			const FString* Value = Binding.ExpressionArguments.Find(Argument.Key);
			if (TestNotNull(*FString::Printf(TEXT("pin %d carries argument '%s'"), Index, *Argument.Key), Value))
			{
				TestEqual(*FString::Printf(TEXT("pin %d argument '%s' value"), Index, *Argument.Key), *Value, Argument.Value);
			}
		}
		TestEqual(*FString::Printf(TEXT("pin %d index"), Index), Binding.ExpressionPinIndex, Index);
	}

	TestEqual(TEXT("class resolved from the head"), PinBindings[0]->ExpressionClass, FString(TEXT("VolumetricAdvancedMaterialOutput")));
	TestEqual(TEXT("head arguments kept (Class + two properties)"), PinBindings[0]->ExpressionArguments.Num(), 3);
	TestEqual(TEXT("sources bound in written order"), PinBindings[1]->SourceText, FString(TEXT("PhaseG2")));

	// TargetText is what every downstream diagnostic quotes, so a head written across three lines
	// must still read as one line and still end in the pin it selects.
	const FString TargetText = PinBindings[2]->TargetText;
	AddInfo(FString::Printf(TEXT("lowered TargetText: %s"), *TargetText));
	TestFalse(TEXT("TargetText has no embedded newline"), TargetText.Contains(TEXT("\n")));
	TestTrue(TEXT("TargetText names the pin it selects"), TargetText.EndsWith(TEXT(".Pin[2]")));
	TestTrue(TEXT("TargetText keeps the head arguments"), TargetText.Contains(TEXT("bGroundContribution")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderOutputsBlockDiagnosticsTest,
	"DreamShader.Lang.OutputsBlock.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
			TEXT("brace after a plain variable"), TEXT("DSH3133"),
			TEXT("        vec3 Color;\n        Color\n        {\n            Pin[0] = Color;\n        }\n        Base.EmissiveColor = Color;\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		{
			// A `.Pin[i]` suffix belongs to the statement form; the block writes its pins inside.
			TEXT("brace after a pin-selecting target"), TEXT("DSH3133"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\").Pin[0]\n        {\n            Pin[1] = Color;\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		// DSH3134 (unterminated block) has no fixture on purpose: the Outputs body handed to the
		// section parser is already brace-balanced -- ExtractBalancedBlock returns the text between a
		// matching pair -- so a block whose '}' is missing is reported by the enclosing Shader block
		// as an unterminated block long before the Outputs scanner sees it. DSH3134 stays as the
		// guard that keeps the scanner from slicing with INDEX_NONE.
		{
			TEXT("non-pin statement inside a block"), TEXT("DSH3135"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            float Extra;\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		{
			TEXT("empty block"), TEXT("DSH3136"),
			TEXT("        vec3 Color;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            // nothing\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n")
		},
		{
			TEXT("same pin twice inside one block"), TEXT("DSH3137"),
			TEXT("        vec3 Color;\n        vec3 A;\n        vec3 B;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            Pin[0] = A;\n            Pin[0] = B;\n        }\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n        A = vec3(1.0, 1.0, 1.0);\n        B = vec3(0.0, 0.0, 0.0);\n")
		},
		{
			// The cross-form rule: the block bound Pin[0], the loose statement names the same node.
			TEXT("block pin re-bound by a statement"), TEXT("DSH3137"),
			TEXT("        vec3 Color;\n        vec3 A;\n        vec3 B;\n        Base.EmissiveColor = Color;\n        Expression(Class=\"ThinTranslucentMaterialOutput\")\n        {\n            Pin[0] = A;\n        }\n        Expression(Class=\"ThinTranslucentMaterialOutput\").Pin[0] = B;\n"),
			TEXT("        Color = vec3(1.0, 0.0, 0.0);\n        A = vec3(1.0, 1.0, 1.0);\n        B = vec3(0.0, 0.0, 0.0);\n")
		},
		{
			// ...and the same rule between two loose statements, which used to be caught only at
			// generation time (DSH8014).
			TEXT("statement pin re-bound by a statement"), TEXT("DSH3137"),
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
				*FString::Printf(TEXT("%s parses (got %s %s)"), Case.Label, *Code, *Message),
				Code.IsEmpty());
			continue;
		}

		if (!TestEqual(*FString::Printf(TEXT("%s raises the right code"), Case.Label), Code, FString(Case.ExpectedCode)))
		{
			AddInfo(FString::Printf(TEXT("%s actual message: %s"), Case.Label, *Message));
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// Generate layer
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderOutputsBlockSingleNodeTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderGenerateCorpusTestBase,
	"DreamShader.Gen.Graph.OutputsBlockSingleNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderOutputsBlockSingleNodeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::OutputsBlockTests;

	// LoadObject<UMaterial> only answers under the Graph backend; the default is ThinCustom.
	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString FixturePath = GetGenerateCorpusFixturePath(TEXT("M_OutputsBlock.dsm"));
	if (!TestFalse(TEXT("Generate corpus fixture located"), FixturePath.IsEmpty()))
	{
		return false;
	}

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("block-form material generates: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(FixturePath, Message, /*bForce*/ true, /*bTransient*/ true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/DreamShaderTests/Generate/M_OutputsBlock.M_OutputsBlock"));
	if (!TestNotNull(TEXT("generated material loads"), Material))
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
	UE::DreamShader::Editor::Private::Tests::FDreamShaderGenerateCorpusTestBase,
	"DreamShader.Gen.Graph.OutputsBlockRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Generate -> decompile -> the decompiled text uses the BLOCK form -> regenerate -> still one node.
// The middle step is the one that would silently regress: a decompiler that kept emitting N loose
// statements still round-trips, so only an assertion on the emitted syntax catches it.
bool FDreamShaderOutputsBlockRoundtripTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::OutputsBlockTests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	const FString FixturePath = GetGenerateCorpusFixturePath(TEXT("M_OutputsBlock.dsm"));
	if (!TestFalse(TEXT("Generate corpus fixture located"), FixturePath.IsEmpty()))
	{
		return false;
	}

	FString Message;
	if (!TestTrue(
		FString::Printf(TEXT("block-form material generates: %s"), *Message),
		FMaterialGenerator::GenerateMaterialFromFile(FixturePath, Message, /*bForce*/ true, /*bTransient*/ true)))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/DreamShaderTests/Generate/M_OutputsBlock.M_OutputsBlock"));
	if (!TestNotNull(TEXT("generated material loads"), Material))
	{
		return false;
	}

	FString DecompiledSource;
	FString DecompileError;
	if (!TestTrue(
		FString::Printf(TEXT("decompile succeeds: %s"), *DecompileError),
		GetGraphDecompiler().DecompileMaterial(
			Material, TEXT("DreamShaderTests/OutputsBlock/M_Roundtrip"), DecompiledSource, DecompileError)))
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

	FScopedScratchSource Scratch;
	if (!Scratch.Write(*this, TEXT("M_OutputsBlockRoundtrip.dsm"), DecompiledSource))
	{
		return false;
	}

	FString RoundtripMessage;
	if (!TestTrue(
		FString::Printf(TEXT("decompiled block form re-generates: %s"), *RoundtripMessage),
		FMaterialGenerator::GenerateMaterialFromFile(Scratch.FilePath, RoundtripMessage, /*bForce*/ true, /*bTransient*/ true)))
	{
		AddInfo(FString::Printf(TEXT("decompiled source:\n%s"), *DecompiledSource));
		return false;
	}

	UMaterial* Roundtripped = LoadObject<UMaterial>(nullptr, TEXT("/Game/DreamShaderTests/OutputsBlock/M_Roundtrip.M_Roundtrip"));
	if (!TestNotNull(TEXT("re-generated material loads"), Roundtripped))
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
