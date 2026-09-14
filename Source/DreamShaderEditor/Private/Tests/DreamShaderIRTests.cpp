// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.IR.* -- the IR builder, the passes and the validator (units I1 and I2), driven
// through their public entry points on small hand-written sources.
//
// One test per cross-unit agreement of Plan/m2m3/CONTRACT.md section 6, named after it. These are
// the agreements two units had to agree on in prose; a test that fails here is one of the two
// having read that prose differently, which is exactly the failure the section exists to catch.
//
// Assertions are structural rather than byte-exact wherever the CONTRACT pins a SHAPE (a
// StaticSwitch with both branches, one TextureSample whatever the spelling, one node where there
// were two identical calls) and full-string on the compact dump only where the whole graph is the
// claim -- idempotence, most of all. The dump is this file's own, not I1's DumpDreamShaderIRText:
// an oracle written against the thing it is checking is not an oracle, and the corpus under
// Tests/Corpus/IR is where I1's format is pinned.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRBuilder.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRDump.h"
#include "IR/IRPasses.h"
#include "IR/IRTypes.h"
#include "IR/IRValidator.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"

// This file's own namespace: the module builds as a unity blob and a helper repeated under the
// shared ...::Tests namespace would be a redefinition, not a local convenience.
namespace UE::DreamShader::Editor::Private::Lang2IRTests
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;

	// =============================================================================================
	// Driving the pipeline
	// =============================================================================================

	using FIRRun = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRun;
	using FIRRunOptions = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRunOptions;

	inline void Lower(FIRRun& Run, const TCHAR* Text, bool bRunPasses = true, bool bValidate = true)
	{
		FIRRunOptions Options;
		Options.bRunPasses = bRunPasses;
		Options.bValidate = bValidate;
		UE::DreamShader::Editor::Private::Tests::RunDreamShaderIRPipeline(TEXT("Test.dss"), Text, Options, Run);
	}

	/** True when some error of the run carries exactly this code. */
	inline bool HasCode(const FIRRun& Run, const TCHAR* Code)
	{
		const FString Needle = FString::Printf(TEXT("%s:"), Code);
		for (const FString& Line : Run.Errors)
		{
			if (Line.StartsWith(Needle, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** True when some error's code falls in [First, Last] -- for a range a unit has not published yet. */
	inline bool HasCodeInRange(const TArray<FString>& Errors, int32 First, int32 Last)
	{
		for (const FString& Line : Errors)
		{
			if (!Line.StartsWith(TEXT("DSH"), ESearchCase::CaseSensitive))
			{
				continue;
			}
			const FString Digits = Line.Mid(3, 4);
			if (Digits.Len() == 4 && FCString::IsNumeric(*Digits))
			{
				const int32 Value = FCString::Atoi(*Digits);
				if (Value >= First && Value <= Last)
				{
					return true;
				}
			}
		}
		return false;
	}

	inline TArray<FString> SinkErrors(const FLangDiagnosticSink& Sink)
	{
		TArray<FString> Lines;
		for (const FLangDiagnostic& Diagnostic : Sink.GetDiagnostics())
		{
			if (Diagnostic.Severity == ELangSeverity::Error)
			{
				Lines.Add(FLangDiagnosticSink::ToWireString(Diagnostic));
			}
		}
		return Lines;
	}

	/** The product of a run, or null with the reason recorded on the test. */
	inline const FIRProduct* Product(FAutomationTestBase& Test, const FIRRun& Run, int32 Index = 0)
	{
		if (!Run.Module.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("the source produced no IR module: %s"), *Run.ErrorText()));
			return nullptr;
		}
		if (!Run.Module->Products.IsValidIndex(Index))
		{
			Test.AddError(FString::Printf(TEXT("the module has no product #%d (it has %d)"), Index, Run.Module->Products.Num()));
			return nullptr;
		}
		return &Run.Module->Products[Index];
	}

	// =============================================================================================
	// Structural queries
	// =============================================================================================

	inline int32 CountOp(const FIRGraph& Graph, EIROp Op)
	{
		int32 Count = 0;
		for (const FIRNode& Node : Graph.Nodes)
		{
			if (Node.Op == Op)
			{
				++Count;
			}
		}
		return Count;
	}

	inline int32 FindOp(const FIRGraph& Graph, EIROp Op, int32 Nth = 0)
	{
		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (Graph.Nodes[Index].Op == Op && Nth-- == 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** A property's canonical text, or "<absent>" so a failure message says which it was. */
	inline FString PropText(const FIRNode& Node, const TCHAR* Name)
	{
		const FIRProperty* Property = Node.FindProperty(Name);
		return Property ? Property->Value.ToString() : FString(TEXT("<absent>"));
	}

	inline FString InputText(const FIRNode& Node, const TCHAR* Pin)
	{
		const FIRInput* Input = Node.FindInput(FString(Pin));
		return Input
			? FString::Printf(TEXT("n%d#%d"), Input->Value.Node, Input->Value.Output)
			: FString(TEXT("<absent>"));
	}

	// =============================================================================================
	// The dump
	// =============================================================================================

	struct FDumpFlags
	{
		bool bDebugNames = false;
		bool bRegions = false;
		bool bSourceSpans = false;
	};

	inline FString NodeLine(const FIRGraph& Graph, int32 Index, const FDumpFlags& Flags)
	{
		const FIRNode& Node = Graph.Nodes[Index];

		FString Line = FString::Printf(TEXT("n%d %s"), Index, LexToString(Node.Op));
		if (!Node.ClassName.IsEmpty())
		{
			Line += FString::Printf(TEXT("(%s)"), *Node.ClassName);
		}

		Line += TEXT(" :");
		for (int32 Output = 0; Output < Node.Outputs.Num(); ++Output)
		{
			if (Output > 0)
			{
				Line += TEXT(",");
			}
			if (Node.OutputNames.IsValidIndex(Output) && !Node.OutputNames[Output].IsEmpty())
			{
				Line += FString::Printf(TEXT("%s="), *Node.OutputNames[Output]);
			}
			Line += Node.Outputs[Output].ToString();
		}

		if (Node.Operands.Num() > 0)
		{
			Line += TEXT(" <");
			for (int32 Operand = 0; Operand < Node.Operands.Num(); ++Operand)
			{
				if (Operand > 0)
				{
					Line += TEXT(",");
				}
				// `_` for an absent slot, the spelling I1's text dump uses. A TextureSample always
				// has four operands (CONTRACT 6.13 #14) and two of them are usually empty, so a
				// dump that printed `n-1#0` for those would be unreadable exactly where it matters.
				Line += Node.Operands[Operand].IsValid()
					? FString::Printf(TEXT("n%d#%d"), Node.Operands[Operand].Node, Node.Operands[Operand].Output)
					: FString(TEXT("_"));
			}
			Line += TEXT(">");
		}

		if (Node.Inputs.Num() > 0)
		{
			Line += TEXT(" [");
			for (int32 Input = 0; Input < Node.Inputs.Num(); ++Input)
			{
				if (Input > 0)
				{
					Line += TEXT(",");
				}
				Line += FString::Printf(
					TEXT("%s=n%d#%d"),
					*Node.Inputs[Input].Pin, Node.Inputs[Input].Value.Node, Node.Inputs[Input].Value.Output);
			}
			Line += TEXT("]");
		}

		if (Node.Properties.Num() > 0)
		{
			// Sorted by name: the property array's order is not part of the node's identity (the
			// dedupe key sorts it too), so a dump that kept it would compare two equal graphs unequal.
			TArray<FString> Properties;
			for (const FIRProperty& Property : Node.Properties)
			{
				Properties.Add(FString::Printf(TEXT("%s=%s"), *Property.Name, *Property.Value.ToString()));
			}
			Properties.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });
			Line += TEXT(" {") + FString::Join(Properties, TEXT(",")) + TEXT("}");
		}

		if (Flags.bDebugNames && !Node.DebugName.IsEmpty())
		{
			Line += FString::Printf(TEXT(" \"%s\""), *Node.DebugName);
		}
		if (Flags.bRegions && Node.Region != INDEX_NONE)
		{
			Line += FString::Printf(TEXT(" @R%d"), Node.Region);
		}
		if (Flags.bSourceSpans)
		{
			Line += FString::Printf(TEXT(" ^%d:%d"), Node.Source.Span.Line, Node.Source.Span.Column);
			if (Node.Source.HasCallSite())
			{
				Line += FString::Printf(TEXT("<-%d:%d"), Node.Source.CallSite.Line, Node.Source.CallSite.Column);
			}
		}

		return Line;
	}

	inline FString GraphShape(const FIRGraph& Graph, const FDumpFlags& Flags = FDumpFlags())
	{
		TArray<FString> Lines;
		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			Lines.Add(NodeLine(Graph, Index, Flags));
		}

		if (Graph.Sink != INDEX_NONE)
		{
			Lines.Add(FString::Printf(TEXT("sink n%d"), Graph.Sink));
		}
		if (Graph.FunctionInputs.Num() > 0)
		{
			TArray<FString> Ids;
			for (const int32 Input : Graph.FunctionInputs) { Ids.Add(FString::Printf(TEXT("n%d"), Input)); }
			Lines.Add(TEXT("inputs ") + FString::Join(Ids, TEXT(",")));
		}
		if (Graph.FunctionOutputs.Num() > 0)
		{
			TArray<FString> Ids;
			for (const int32 Output : Graph.FunctionOutputs) { Ids.Add(FString::Printf(TEXT("n%d"), Output)); }
			Lines.Add(TEXT("outputs ") + FString::Join(Ids, TEXT(",")));
		}
		if (Flags.bRegions)
		{
			for (int32 Index = 0; Index < Graph.Regions.Num(); ++Index)
			{
				Lines.Add(FString::Printf(TEXT("R%d \"%s\" parent=%d"), Index, *Graph.Regions[Index].Name, Graph.Regions[Index].Parent));
			}
		}

		return FString::Join(Lines, TEXT("\n"));
	}

	// =============================================================================================
	// Hand-built modules, for the validator
	// =============================================================================================

	inline FIRNode MakeConstantNode(double Value)
	{
		FIRNode Node;
		Node.Op = EIROp::Constant;
		Node.Outputs.Add(FIRType::Float(1));
		const double Values[4] = { Value, 0.0, 0.0, 0.0 };
		Node.Properties.Add({ FString(Prop::Value), FIRPropertyValue::MakeFloat4(Values, 1) });
		return Node;
	}

	inline FIRNode MakeUnaryNode(EIROp Op, FIRValue Operand, const FIRType& Type)
	{
		FIRNode Node;
		Node.Op = Op;
		Node.Operands.Add(Operand);
		Node.Outputs.Add(Type);
		return Node;
	}
}

// =================================================================================================
// 6.1 -- uniforms become parameter nodes
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRParameterKindsTest,
	"DreamShader.Lang2.IR.ParameterKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRParameterKindsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	FIRRun Run;
	Lower(Run, TEXT(
		"/// @group Look @sort 20 @slider 0 4 @desc The gain\n"
		"uniform float Gain = 0.7;\n"
		"uniform float3 Colour = float3(1, 0, 0);\n"
		"/// @static\n"
		"uniform bool UseColour = true;\n"
		"/// @sampler Normal\n"
		"uniform Texture2D Tex;\n"
		"static const float K = 2;\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
		"    m.EmissiveColor = Colour * Gain * K + Tex.Sample(UV).rgb * 0;\n"
		"    m.Opacity = UseColour ? 1 : 0;\n"
		"}\n"));

	const FIRProduct* Found = Product(*this, Run);
	if (!Found)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		return false;
	}
	const FIRGraph& Graph = Found->Graph;

	// A scalar and a vector uniform are both EIROp::Parameter; the width lives in Outputs[0].
	const int32 GainIndex = [&Graph]() -> int32
	{
		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (Graph.Nodes[Index].Op == EIROp::Parameter
				&& PropText(Graph.Nodes[Index], Prop::ParameterName).Equals(TEXT("Gain"), ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}();

	if (TestTrue(TEXT("the scalar uniform became a Parameter node"), GainIndex != INDEX_NONE))
	{
		const FIRNode& Gain = Graph.Nodes[GainIndex];
		TestEqual(TEXT("one output"), Gain.Outputs.Num(), 1);
		if (Gain.Outputs.Num() == 1)
		{
			TestEqualSensitive(TEXT("a scalar uniform is float1"), Gain.Outputs[0].ToString(), FString(TEXT("float")));
		}
		// Every piece of the parameter's metadata that CONTRACT 6.1 lists comes from the directives.
		TestEqualSensitive(TEXT("Group"), PropText(Gain, Prop::Group), FString(TEXT("Look")));
		TestEqualSensitive(TEXT("Description"), PropText(Gain, Prop::Description), FString(TEXT("The gain")));
		TestEqualSensitive(TEXT("SortPriority"), PropText(Gain, Prop::SortPriority), FString(TEXT("20")));
		TestNotEqual(TEXT("SliderMin is written"), PropText(Gain, Prop::SliderMin), FString(TEXT("<absent>")));
		TestNotEqual(TEXT("SliderMax is written"), PropText(Gain, Prop::SliderMax), FString(TEXT("<absent>")));
		TestNotEqual(TEXT("DefaultValue is written"), PropText(Gain, Prop::DefaultValue), FString(TEXT("<absent>")));
	}

	// A static bool carries IsStatic, which is what picks StaticBoolParameter over a scalar.
	const int32 StaticIndex = [&Graph]() -> int32
	{
		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (Graph.Nodes[Index].Op == EIROp::Parameter
				&& PropText(Graph.Nodes[Index], Prop::ParameterName).Equals(TEXT("UseColour"), ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}();
	if (TestTrue(TEXT("the bool uniform became a Parameter node"), StaticIndex != INDEX_NONE))
	{
		TestEqualSensitive(TEXT("IsStatic"), PropText(Graph.Nodes[StaticIndex], Prop::IsStatic), FString(TEXT("true")));
	}

	// A texture uniform is its own op, with the sampler type from `@sampler`.
	const int32 TextureIndex = FindOp(Graph, EIROp::TextureParameter);
	if (TestTrue(TEXT("the texture uniform became a TextureParameter node"), TextureIndex != INDEX_NONE))
	{
		TestEqualSensitive(TEXT("ParameterName"), PropText(Graph.Nodes[TextureIndex], Prop::ParameterName), FString(TEXT("Tex")));
		TestEqualSensitive(TEXT("SamplerType"), PropText(Graph.Nodes[TextureIndex], Prop::SamplerType), FString(TEXT("Normal")));
	}

	// CONTRACT 6.13 #22: a vector uniform is a float4 VectorParameter narrowed by a Swizzle --
	// `xy` for a float2, `xyz` for a float3, nothing at all for a float4. One rule, so the emitter
	// never has to ask how wide the author declared the parameter.
	const int32 ColourIndex = [&Graph]() -> int32
	{
		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (Graph.Nodes[Index].Op == EIROp::Parameter
				&& PropText(Graph.Nodes[Index], Prop::ParameterName).Equals(TEXT("Colour"), ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}();

	if (TestTrue(TEXT("the float3 uniform became a Parameter node"), ColourIndex != INDEX_NONE))
	{
		const FIRNode& Colour = Graph.Nodes[ColourIndex];
		if (TestEqual(TEXT("one output"), Colour.Outputs.Num(), 1))
		{
			TestEqualSensitive(
				TEXT("a vector parameter node is a float4 whatever the declared width"),
				Colour.Outputs[0].ToString(), FString(TEXT("float4")));
		}

		bool bNarrowed = false;
		for (const FIRNode& Node : Graph.Nodes)
		{
			if (Node.Op == EIROp::Swizzle
				&& Node.Operands.Num() == 1
				&& Node.Operands[0].Node == ColourIndex
				&& PropText(Node, Prop::Mask).Equals(TEXT("xyz"), ESearchCase::CaseSensitive))
			{
				bNarrowed = true;
				break;
			}
		}
		TestTrue(TEXT("and a float3 uniform is narrowed by an xyz Swizzle"), bNarrowed);
	}

	// `static const` is a Constant, never a parameter: nothing about it is tunable.
	TestEqual(TEXT("a static const makes no parameter node"),
		CountOp(Graph, EIROp::Parameter), 3);
	TestTrue(TEXT("and the graph has constants"), CountOp(Graph, EIROp::Constant) > 0);

	AddInfo(FString::Printf(TEXT("graph:\n%s"), *GraphShape(Graph)));
	return true;
}

// =================================================================================================
// 6.2 -- the material value is a field map, and a plain entry makes NO MakeMaterialAttributes
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRMaterialFieldsTest,
	"DreamShader.Lang2.IR.MaterialFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRMaterialFieldsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	FIRRun Run;
	Lower(Run, TEXT(
		"uniform float3 Colour = float3(1, 0, 0);\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    m.BaseColor = Colour;\n"
		"    m.Roughness = 0.4;\n"
		"    m.EmissiveColor = m.BaseColor * 2;\n"
		"}\n"));

	const FIRProduct* Found = Product(*this, Run);
	if (!Found)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		return false;
	}
	const FIRGraph& Graph = Found->Graph;

	// The whole of CONTRACT 6.2 in one assertion: the map's entries become the sink's inputs
	// directly, so there is nothing to make attributes out of.
	TestEqual(TEXT("a plain entry makes no MakeMaterialAttributes"), CountOp(Graph, EIROp::MakeMaterialAttributes), 0);
	TestEqual(TEXT("and no SetMaterialAttributes"), CountOp(Graph, EIROp::SetMaterialAttributes), 0);
	TestEqual(TEXT("and no GetMaterialAttributes"), CountOp(Graph, EIROp::GetMaterialAttributes), 0);
	TestEqual(TEXT("exactly one MaterialSink"), CountOp(Graph, EIROp::MaterialSink), 1);

	if (TestTrue(TEXT("the graph records its sink"), Graph.Sink != INDEX_NONE))
	{
		const FIRNode& Sink = Graph.Nodes[Graph.Sink];
		TestEqual(TEXT("three attributes were written"), Sink.Inputs.Num(), 3);
		TestNotEqual(TEXT("BaseColor is wired"), InputText(Sink, TEXT("BaseColor")), FString(TEXT("<absent>")));
		TestNotEqual(TEXT("Roughness is wired"), InputText(Sink, TEXT("Roughness")), FString(TEXT("<absent>")));
		TestNotEqual(TEXT("EmissiveColor is wired"), InputText(Sink, TEXT("EmissiveColor")), FString(TEXT("<absent>")));

		// Reading back an attribute reads the VALUE that was recorded, not a Break node: the map is
		// the material while lowering.
		const FIRInput* BaseColor = Sink.FindInput(FString(TEXT("BaseColor")));
		const FIRInput* Emissive = Sink.FindInput(FString(TEXT("EmissiveColor")));
		if (BaseColor && Emissive && Graph.Nodes.IsValidIndex(Emissive->Value.Node))
		{
			const FIRNode& EmissiveNode = Graph.Nodes[Emissive->Value.Node];
			TestEqual(TEXT("the emissive is the multiply"), static_cast<int32>(EmissiveNode.Op), static_cast<int32>(EIROp::Multiply));
			TestTrue(
				TEXT("whose first operand is the value BaseColor was set to"),
				EmissiveNode.Operands.Num() > 0 && EmissiveNode.Operands[0] == BaseColor->Value);
		}
	}

	// Reading an attribute that was never written is the one refusal of 6.2.
	{
		FIRRun Unwritten;
		Lower(Unwritten, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = m.BaseColor;\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("reading an unset attribute is refused: DSH4370 (actual: %s)"), *Unwritten.ErrorText()),
			HasCode(Unwritten, TEXT("DSH4370")));
	}

	// A material that crosses a pin DOES become a node: a layer blend reads its inputs.
	{
		FIRRun Blend;
		Lower(Blend, TEXT(
			"/// @layerblend\n"
			"export void B_Case(material A, material B, inout material R)\n"
			"{\n"
			"    R = A;\n"
			"    R.Roughness = B.Roughness;\n"
			"}\n"));
		if (Blend.Module.IsValid() && Blend.Module->Products.Num() == 1)
		{
			const FIRGraph& BlendGraph = Blend.Module->Products[0].Graph;
			TestTrue(
				TEXT("a material that arrived through a pin is read with GetMaterialAttributes"),
				CountOp(BlendGraph, EIROp::GetMaterialAttributes) > 0);
			TestTrue(
				TEXT("and a layer blend returns a SetMaterialAttributes chain"),
				CountOp(BlendGraph, EIROp::SetMaterialAttributes) > 0);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("the layer blend did not lower: %s"), *Blend.ErrorText()));
		}
	}

	return true;
}

// =================================================================================================
// 6.3 -- static and dynamic `if`
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRBranchesTest,
	"DreamShader.Lang2.IR.Branches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRBranchesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// A STATIC condition: a StaticBool parameter, so the merge is a StaticSwitch.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"/// @static\n"
			"uniform bool UseWarm = true;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float3 Colour = float3(0, 0, 0);\n"
			"    if (UseWarm) { Colour = float3(1, 0.5, 0); }\n"
			"    else { Colour = float3(0, 0.5, 1); }\n"
			"    m.EmissiveColor = Colour;\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			const FIRGraph& Graph = Found->Graph;
			TestEqual(TEXT("a static condition merges with StaticSwitch"), CountOp(Graph, EIROp::StaticSwitch), 1);
			TestEqual(TEXT("and never with Select"), CountOp(Graph, EIROp::Select), 0);

			const int32 SwitchIndex = FindOp(Graph, EIROp::StaticSwitch);
			if (SwitchIndex != INDEX_NONE)
			{
				const FIRNode& Switch = Graph.Nodes[SwitchIndex];
				// Both branches are lowered, both are present, and the condition is operand 0.
				TestEqual(TEXT("StaticSwitch takes three operands"), Switch.Operands.Num(), 3);
				if (Switch.Operands.Num() == 3)
				{
					TestTrue(TEXT("the true branch is a real value"), Switch.Operands[1].IsValid());
					TestTrue(TEXT("the false branch is a real value"), Switch.Operands[2].IsValid());
					TestTrue(TEXT("and they are different values"), Switch.Operands[1] != Switch.Operands[2]);
				}
			}
			AddInfo(FString::Printf(TEXT("static branch graph:\n%s"), *GraphShape(Graph)));
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		}
	}

	// A DYNAMIC condition: the same shape, with Select.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float Threshold = 0.5;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
			"    float3 Colour = float3(0, 0, 0);\n"
			"    if (UV.x > Threshold) { Colour = float3(1, 0, 0); }\n"
			"    else { Colour = float3(0, 0, 1); }\n"
			"    m.EmissiveColor = Colour;\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			const FIRGraph& Graph = Found->Graph;
			// CONTRACT 6.13 #20: the condition is a BARE COMPARISON, so the merge is a Compare
			// node carrying the comparison's own operands -- and the bool node that comparison
			// would otherwise have produced is pruned, because nothing reads it any more.
			TestEqual(TEXT("a bare comparison merges with Compare"), CountOp(Graph, EIROp::Compare), 1);
			TestEqual(TEXT("and not with Select"), CountOp(Graph, EIROp::Select), 0);
			TestEqual(TEXT("and never with StaticSwitch"), CountOp(Graph, EIROp::StaticSwitch), 0);
			TestEqual(TEXT("the comparison's own bool node is pruned"), CountOp(Graph, EIROp::Greater), 0);

			const int32 CompareIndex = FindOp(Graph, EIROp::Compare);
			if (CompareIndex != INDEX_NONE)
			{
				// [A, B, IfGreater, IfEqual, IfLess]: the comparison's operands, then the three
				// branches the engine's If node picks between.
				const FIRNode& Compare = Graph.Nodes[CompareIndex];
				TestEqual(TEXT("Compare takes five operands"), Compare.Operands.Num(), 5);
				if (Compare.Operands.Num() == 5)
				{
					TestTrue(TEXT("A is a real value"), Compare.Operands[0].IsValid());
					TestTrue(TEXT("B is a real value"), Compare.Operands[1].IsValid());
					TestTrue(TEXT("and both branches are present"),
						Compare.Operands[2].IsValid() && Compare.Operands[4].IsValid());
				}
			}
			AddInfo(FString::Printf(TEXT("dynamic branch graph:\n%s"), *GraphShape(Graph)));
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		}
	}

	// The other half of 6.13 #20: a dynamic condition that is NOT a comparison has nothing to put
	// on a Compare's A and B pins, so it merges with Select.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform bool Enabled = true;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float3 Colour = float3(0, 0, 0);\n"
			"    if (Enabled) { Colour = float3(1, 0, 0); }\n"
			"    else { Colour = float3(0, 0, 1); }\n"
			"    m.EmissiveColor = Colour;\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			const FIRGraph& Graph = Found->Graph;
			TestEqual(TEXT("a non-comparison condition merges with Select"), CountOp(Graph, EIROp::Select), 1);
			TestEqual(TEXT("and not with Compare"), CountOp(Graph, EIROp::Compare), 0);

			const int32 SelectIndex = FindOp(Graph, EIROp::Select);
			if (SelectIndex != INDEX_NONE && Graph.Nodes[SelectIndex].Operands.Num() == 3)
			{
				TestTrue(TEXT("both branches are present"),
					Graph.Nodes[SelectIndex].Operands[1].IsValid() && Graph.Nodes[SelectIndex].Operands[2].IsValid());
			}
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		}
	}

	// A branch with no `else`: the merged value is the value the variable already had.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float Threshold = 0.5;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
			"    float Value = 1;\n"
			"    if (UV.x > Threshold) { Value = 2; }\n"
			"    m.Opacity = Value;\n"
			"}\n"));
		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			TestEqual(TEXT("a one-sided if still merges once"), CountOp(Found->Graph, EIROp::Compare), 1);
		}
	}

	// `discard` has no graph form, in a branch or anywhere else.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float Threshold = 0.5;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
			"    if (UV.x > Threshold) { discard; }\n"
			"    m.Opacity = 1;\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("discard inside a branch is refused: DSH4362 (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4362")));
	}

	return true;
}

// =================================================================================================
// 6.4 -- helper inlining and the CallSite span
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRInliningTest,
	"DreamShader.Lang2.IR.Inlining",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRInliningTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	FIRRun Run;
	Lower(Run, TEXT(
		"uniform float Gain = 2;\n"
		"float Scale(float x, float k)\n"
		"{\n"
		"    return x * k;\n"
		"}\n"
		"void Split(float x, out float Half, out float Double)\n"
		"{\n"
		"    Half = x * 0.5;\n"
		"    Double = x * 2;\n"
		"}\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float A = Scale(Gain, 3);\n"
		"    float H = 0;\n"
		"    float D = 0;\n"
		"    Split(A, H, D);\n"
		"    m.Opacity = H + D;\n"
		"}\n"));

	const FIRProduct* Found = Product(*this, Run);
	if (!Found)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		return false;
	}
	const FIRGraph& Graph = Found->Graph;

	// A helper leaves no trace of itself: it is not a FunctionCall, it is the nodes of its body.
	TestEqual(TEXT("a helper call makes no FunctionCall node"), CountOp(Graph, EIROp::FunctionCall), 0);

	// Every node made while inlining carries the CALL SITE as well as its own span, which is what
	// gives the editor two places to jump to (plan 13.2).
	int32 WithCallSite = 0;
	for (const FIRNode& Node : Graph.Nodes)
	{
		if (Node.Source.HasCallSite())
		{
			++WithCallSite;
			TestTrue(
				TEXT("a node with a call site also has its own span inside the helper"),
				Node.Source.Span.Length > 0);
			TestTrue(
				TEXT("and the two spans are different places"),
				Node.Source.Span.Offset != Node.Source.CallSite.Offset);
			// CONTRACT 6.13 #6: CallSiteFile names the CALLER's file, and differs from File only
			// when the helper came out of an included header. This helper lives in the same file, so
			// the two agree -- Tests/Corpus/IR/Includes is where the cross-file case is written down.
			TestTrue(
				TEXT("and a same-file inline does not claim a different call-site file"),
				Node.Source.CallSiteFile.IsEmpty()
					|| Node.Source.CallSiteFile.Equals(Node.Source.File, ESearchCase::CaseSensitive));
		}
	}
	TestTrue(TEXT("some node was made while inlining"), WithCallSite > 0);

	// An `out` parameter writes back into the caller's lvalue rather than producing a node.
	TestTrue(TEXT("both out parameters reached the sink"),
		Graph.Sink != INDEX_NONE && Graph.Nodes[Graph.Sink].Inputs.Num() == 1);

	AddInfo(FString::Printf(TEXT("inlined graph:\n%s"),
		*GraphShape(Graph, FDumpFlags{ /*bDebugNames*/ true, /*bRegions*/ false, /*bSourceSpans*/ true })));

	// Recursion cannot be inlined, and saying so is better than running out of stack.
	{
		FIRRun Recursive;
		Lower(Recursive, TEXT(
			"float Ping(float x) { return Pong(x); }\n"
			"float Pong(float x) { return Ping(x); }\n"
			"export void M_Case(inout material m) { m.Opacity = Ping(1); }\n"));
		TestTrue(
			FString::Printf(TEXT("recursion is refused: DSH6220 (actual: %s)"), *Recursive.ErrorText()),
			HasCode(Recursive, TEXT("DSH6220")));
	}

	// A helper taking `inout material` is inlined on the field map like any other (CONTRACT 6.4).
	{
		FIRRun Material;
		Lower(Material, TEXT(
			"void ApplyBase(inout material m, float3 c) { m.BaseColor = c; }\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    ApplyBase(m, float3(1, 0, 0));\n"
			"    m.Opacity = 1;\n"
			"}\n"));
		const FIRProduct* MaterialProduct = Material.Module.IsValid() && Material.Module->Products.Num() > 0
			? &Material.Module->Products[0]
			: nullptr;
		if (TestNotNull(TEXT("the material helper lowered"), MaterialProduct))
		{
			TestEqual(
				TEXT("a helper on the field map still makes no MakeMaterialAttributes"),
				CountOp(MaterialProduct->Graph, EIROp::MakeMaterialAttributes), 0);
			if (MaterialProduct->Graph.Sink != INDEX_NONE)
			{
				TestEqual(
					TEXT("the helper's write reached the sink"),
					MaterialProduct->Graph.Nodes[MaterialProduct->Graph.Sink].Inputs.Num(), 2);
			}
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Material.ErrorText()));
		}
	}

	return true;
}

// =================================================================================================
// 6.5 -- the four texture spellings normalise to one shape
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRTextureShapeTest,
	"DreamShader.Lang2.IR.TextureShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRTextureShapeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// CONTRACT 6.13 #14: a TextureSample ALWAYS has four operands, [Texture, UV, Sampler, Level],
	// with FIRValue::None() in an absent slot. So the four spellings do not all lower to the same
	// node -- they lower to the same SHAPE, with the slots the spelling supplied filled in and the
	// rest empty. A fixed arity is what lets the emitter index operands instead of counting them.
	struct FSpelling
	{
		const TCHAR* Text;
		bool bSampler;
		bool bLevel;
	};

	const FSpelling Spellings[] =
	{
		{ TEXT("Tex.Sample(UV)"),                 false, false },
		{ TEXT("Tex.Sample(Tex, UV)"),            true,  false },
		{ TEXT("Texture2DSample(Tex, Tex, UV)"),  true,  false },
		{ TEXT("Tex.SampleLevel(UV, 0)"),         false, true  },
	};

	TArray<FString> Shapes;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Spellings); ++Index)
	{
		const FSpelling& Spelling = Spellings[Index];
		const FString Source = FString::Printf(TEXT(
			"uniform Texture2D Tex;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
			"    m.EmissiveColor = %s.rgb;\n"
			"}\n"), Spelling.Text);

		FIRRun Run;
		Lower(Run, *Source);

		const FIRProduct* Found = Product(*this, Run);
		if (!Found)
		{
			AddInfo(FString::Printf(TEXT("'%s' did not lower: %s"), Spelling.Text, *Run.ErrorText()));
			Shapes.Add(FString());
			continue;
		}
		const FIRGraph& Graph = Found->Graph;

		TestEqual(
			FString::Printf(TEXT("'%s' makes exactly one TextureSample"), Spelling.Text),
			CountOp(Graph, EIROp::TextureSample), 1);

		const int32 SampleIndex = FindOp(Graph, EIROp::TextureSample);
		if (SampleIndex == INDEX_NONE)
		{
			Shapes.Add(FString());
			continue;
		}
		const FIRNode& Sample = Graph.Nodes[SampleIndex];

		// Five outputs, named: `.rgb` on the result is then an ordinary Swizzle of RGBA.
		TestEqual(FString::Printf(TEXT("'%s' has five outputs"), Spelling.Text), Sample.Outputs.Num(), 5);
		if (Sample.OutputNames.Num() == 5)
		{
			const TCHAR* Names[] = { TEXT("RGBA"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
			for (int32 Output = 0; Output < 5; ++Output)
			{
				TestEqualSensitive(
					*FString::Printf(TEXT("'%s' output %d"), Spelling.Text, Output),
					Sample.OutputNames[Output],
					FString(Names[Output]));
			}
		}
		else
		{
			AddError(FString::Printf(TEXT("'%s' named %d outputs, expected 5"), Spelling.Text, Sample.OutputNames.Num()));
		}

		// The fixed four slots.
		if (!TestEqual(
				FString::Printf(TEXT("'%s' has four operand slots"), Spelling.Text),
				Sample.Operands.Num(), 4))
		{
			Shapes.Add(GraphShape(Graph));
			continue;
		}

		TestTrue(FString::Printf(TEXT("'%s' slot 0 (Texture) is filled"), Spelling.Text), Sample.Operands[0].IsValid());
		TestTrue(FString::Printf(TEXT("'%s' slot 1 (UV) is filled"), Spelling.Text), Sample.Operands[1].IsValid());
		TestEqual(
			FString::Printf(TEXT("'%s' slot 2 (Sampler)"), Spelling.Text),
			Sample.Operands[2].IsValid(), Spelling.bSampler);
		TestEqual(
			FString::Printf(TEXT("'%s' slot 3 (Level)"), Spelling.Text),
			Sample.Operands[3].IsValid(), Spelling.bLevel);

		if (Sample.Operands[0].IsValid() && Graph.Nodes.IsValidIndex(Sample.Operands[0].Node))
		{
			TestEqual(
				FString::Printf(TEXT("'%s' slot 0 is the texture parameter"), Spelling.Text),
				static_cast<int32>(Graph.Nodes[Sample.Operands[0].Node].Op),
				static_cast<int32>(EIROp::TextureParameter));
		}

		Shapes.Add(GraphShape(Graph));
	}

	// The two spellings that supply the SAME slots lower to the same graph, character for
	// character; the two that supply different slots do not. Both halves matter: the first says the
	// normalisation happened, the second says it did not go too far and erase the sampler or the mip.
	if (Shapes.Num() == 4 && !Shapes[1].IsEmpty() && !Shapes[2].IsEmpty())
	{
		TestEqualSensitive(
			TEXT("'Tex.Sample(Tex, UV)' and 'Texture2DSample(Tex, Tex, UV)' are the same graph"),
			Shapes[2], Shapes[1]);
	}
	if (Shapes.Num() == 4 && !Shapes[0].IsEmpty() && !Shapes[1].IsEmpty())
	{
		TestFalse(
			TEXT("an explicit sampler is not erased by the normalisation"),
			Shapes[0].Equals(Shapes[1], ESearchCase::CaseSensitive));
	}
	if (Shapes.Num() == 4 && !Shapes[0].IsEmpty() && !Shapes[3].IsEmpty())
	{
		TestFalse(
			TEXT("an explicit mip level is not erased by the normalisation"),
			Shapes[0].Equals(Shapes[3], ESearchCase::CaseSensitive));
	}

	for (int32 Index = 0; Index < Shapes.Num(); ++Index)
	{
		AddInfo(FString::Printf(TEXT("'%s':\n%s"), Spellings[Index].Text, *Shapes[Index]));
	}

	return true;
}

// =================================================================================================
// 6.6 -- swizzle canonical form
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRSwizzleShapeTest,
	"DreamShader.Lang2.IR.SwizzleShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRSwizzleShapeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// CONTRACT 6.6 with the 6.13 #19 ruling: a Swizzle node's mask is always strictly ascending. A
	// ComponentMask is therefore only ever a NARROWING -- a reorder is Swizzles plus an Append,
	// which is what a person building the graph by hand would have had to do as well.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float4 V = float4(1, 2, 3, 4);\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = V.rgb;\n"
			"    m.Opacity = V[3];\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (!Found)
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
			return false;
		}
		const FIRGraph& Graph = Found->Graph;

		TArray<FString> Masks;
		for (const FIRNode& Node : Graph.Nodes)
		{
			if (Node.Op == EIROp::Swizzle)
			{
				Masks.Add(PropText(Node, Prop::Mask));
				TestEqual(TEXT("a swizzle takes exactly one operand"), Node.Operands.Num(), 1);
			}
		}
		Masks.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });

		// Two narrowings, and no Append: neither of these reorders anything. The float4 uniform's
		// own VectorParameter needs no swizzle either (CONTRACT 6.13 #22).
		if (TestEqual(TEXT("two masks"), Masks.Num(), 2))
		{
			TestEqualSensitive(TEXT("V[3] is the w mask"), Masks[0], FString(TEXT("w")));
			TestEqualSensitive(TEXT("V.rgb is the xyz mask"), Masks[1], FString(TEXT("xyz")));
		}
		TestEqual(TEXT("a pure narrowing needs no Append"), CountOp(Graph, EIROp::Append), 0);
	}

	// The reorder: one Swizzle per component, then an Append. The thing that must NOT exist is a
	// single Swizzle whose mask is out of order.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float4 V = float4(1, 2, 3, 4);\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = float3(V.yx, 0);\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (!Found)
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
			return false;
		}
		const FIRGraph& Graph = Found->Graph;

		TestTrue(TEXT("a reorder makes an Append"), CountOp(Graph, EIROp::Append) >= 1);
		TestTrue(TEXT("and one Swizzle per component"), CountOp(Graph, EIROp::Swizzle) >= 2);

		for (const FIRNode& Node : Graph.Nodes)
		{
			if (Node.Op != EIROp::Swizzle)
			{
				continue;
			}

			const FString Mask = PropText(Node, Prop::Mask);
			bool bAscending = true;
			for (int32 Index = 1; Index < Mask.Len(); ++Index)
			{
				bAscending &= (Mask[Index - 1] < Mask[Index]);
			}
			TestTrue(
				FString::Printf(TEXT("the mask '%s' is strictly ascending"), *Mask),
				bAscending);
		}

		AddInfo(FString::Printf(TEXT("reorder graph:\n%s"), *GraphShape(Graph)));
	}

	// CONTRACT 6.13 #29: replication is the same shape as a reorder. `V.xxx` is legal HLSL and
	// lowers to single-channel masks plus an Append -- there is no ComponentMask that repeats a
	// channel, so "ascending" and "replicating" are satisfied by the same rule: one mask per
	// component, assembled afterwards.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float4 V = float4(1, 2, 3, 4);\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = V.xxx;\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (!Found)
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
			return false;
		}
		const FIRGraph& Graph = Found->Graph;

		TestTrue(TEXT("a replicating swizzle makes an Append"), CountOp(Graph, EIROp::Append) >= 1);

		// One single-channel Swizzle, deduped: all three components are the same `x` mask off the
		// same parameter, so the structural key merges them and the Append reads it three times.
		int32 SingleChannel = 0;
		for (const FIRNode& Node : Graph.Nodes)
		{
			if (Node.Op != EIROp::Swizzle)
			{
				continue;
			}
			const FString Mask = PropText(Node, Prop::Mask);
			TestEqual(
				FString::Printf(TEXT("the replicated mask '%s' is one component"), *Mask),
				Mask.Len(), 1);
			++SingleChannel;
		}
		TestTrue(TEXT("a replication is built out of single-channel masks"), SingleChannel >= 1);
		TestEqual(TEXT("and the three identical ones merge into one"), SingleChannel, 1);

		AddInfo(FString::Printf(TEXT("replication graph:\n%s"), *GraphShape(Graph)));
	}

	return true;
}

// =================================================================================================
// 6.7 -- the dedupe pass
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRDedupeTest,
	"DreamShader.Lang2.IR.Dedupe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRDedupeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// Two identical `UE.TexCoord` calls written in two places. Decision 11 #5 rests on this: the
	// language has no `let` because two identical calls cost one node.
	const TCHAR* Source = TEXT(
		"uniform Texture2D Tex;\n"
		"uniform float Gain = 1;\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float4 A = Tex.Sample(UE.TexCoord(CoordinateIndex = 0));\n"
		"    float4 B = Tex.Sample(UE.TexCoord(CoordinateIndex = 0));\n"
		"    m.EmissiveColor = (A * Gain).rgb + (B * Gain).rgb;\n"
		"}\n");

	// Before the passes: two of everything, because the builder is a faithful translation.
	FIRRun Raw;
	Lower(Raw, Source, /*bRunPasses*/ false, /*bValidate*/ true);
	const FIRProduct* RawProduct = Product(*this, Raw);
	if (!RawProduct)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Raw.ErrorText()));
		return false;
	}
	TestEqual(TEXT("the raw graph has two TexCoord calls"), CountOp(RawProduct->Graph, EIROp::Reflected), 2);
	TestEqual(TEXT("and two TextureSamples"), CountOp(RawProduct->Graph, EIROp::TextureSample), 2);

	// After the passes: one of each, because the keys are structural.
	FIRRun Deduped;
	Lower(Deduped, Source, /*bRunPasses*/ true, /*bValidate*/ true);
	const FIRProduct* DedupedProduct = Product(*this, Deduped);
	if (!DedupedProduct)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Deduped.ErrorText()));
		return false;
	}
	const FIRGraph& Graph = DedupedProduct->Graph;

	TestEqual(TEXT("two identical UE.TexCoord calls merge into one node"), CountOp(Graph, EIROp::Reflected), 1);
	TestEqual(TEXT("and the structurally equal samples above them merge too"), CountOp(Graph, EIROp::TextureSample), 1);
	TestEqual(TEXT("as does the multiply, which is now the same subtree twice"), CountOp(Graph, EIROp::Multiply), 1);

	// The key is on the node, filled by the pass, and it is what E must NOT re-derive.
	for (const FIRNode& Node : Graph.Nodes)
	{
		TestTrue(
			FString::Printf(TEXT("every node carries a dedupe key after the pass (%s)"), LexToString(Node.Op)),
			!Node.DedupeKey.IsEmpty());
	}

	// A different argument is a different key: dedupe must not merge what only LOOKS alike.
	{
		FIRRun Different;
		Lower(Different, TEXT(
			"uniform Texture2D Tex;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float4 A = Tex.Sample(UE.TexCoord(CoordinateIndex = 0));\n"
			"    float4 B = Tex.Sample(UE.TexCoord(CoordinateIndex = 1));\n"
			"    m.EmissiveColor = A.rgb + B.rgb;\n"
			"}\n"));
		const FIRProduct* DifferentProduct = Product(*this, Different);
		if (DifferentProduct)
		{
			TestEqual(
				TEXT("two TexCoord calls with different indices stay two nodes"),
				CountOp(DifferentProduct->Graph, EIROp::Reflected), 2);
		}
	}

	// A DebugName difference is not an identity difference (CONTRACT 6.7): the first name wins.
	{
		FIRRun Named;
		Lower(Named, TEXT(
			"uniform float Gain = 1;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float Alpha = Gain * 2;\n"
			"    float Beta  = Gain * 2;\n"
			"    m.Opacity = Alpha + Beta;\n"
			"}\n"));
		const FIRProduct* NamedProduct = Product(*this, Named);
		if (NamedProduct)
		{
			TestEqual(
				TEXT("two equal subtrees with different variable names still merge"),
				CountOp(NamedProduct->Graph, EIROp::Multiply), 1);

			const int32 MultiplyIndex = FindOp(NamedProduct->Graph, EIROp::Multiply);
			if (MultiplyIndex != INDEX_NONE && !NamedProduct->Graph.Nodes[MultiplyIndex].DebugName.IsEmpty())
			{
				TestEqualSensitive(
					TEXT("the first name wins"),
					NamedProduct->Graph.Nodes[MultiplyIndex].DebugName,
					FString(TEXT("Alpha")));
			}
		}
	}

	return true;
}

// =================================================================================================
// 6.8 -- regions reach the graph
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRRegionsTest,
	"DreamShader.Lang2.IR.Regions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRRegionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	FIRRun Run;
	Lower(Run, TEXT(
		"#pragma region Parameters\n"
		"uniform float A = 1;\n"
		"uniform float B = 2;\n"
		"#pragma endregion\n"
		"\n"
		"#pragma layout(Comment, Name = \"Maths\", X = 0, Y = 0, W = 400, H = 200)\n"
		"\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    #pragma region Outer\n"
		"    float Sum = A + B;\n"
		"    #pragma region Inner\n"
		"    float Scaled = Sum * 2;\n"
		"    #pragma endregion\n"
		"    #pragma endregion\n"
		"    m.Opacity = Scaled;\n"
		"}\n"));

	const FIRProduct* Found = Product(*this, Run);
	if (!Found)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		return false;
	}
	const FIRGraph& Graph = Found->Graph;

	// The tree is copied whole, nesting included.
	TestEqual(TEXT("three regions reached the graph"), Graph.Regions.Num(), 3);
	if (Graph.Regions.Num() == 3)
	{
		TestEqualSensitive(TEXT("region 0"), Graph.Regions[0].Name, FString(TEXT("Parameters")));
		TestEqualSensitive(TEXT("region 1"), Graph.Regions[1].Name, FString(TEXT("Outer")));
		TestEqualSensitive(TEXT("region 2"), Graph.Regions[2].Name, FString(TEXT("Inner")));
		TestEqual(TEXT("Inner nests inside Outer"), Graph.Regions[2].Parent, 1);
	}

	// And a node made from a statement inside a region is stamped with it.
	const int32 AddIndex = FindOp(Graph, EIROp::Add);
	const int32 MultiplyIndex = FindOp(Graph, EIROp::Multiply);
	if (TestTrue(TEXT("the sum is in the graph"), AddIndex != INDEX_NONE))
	{
		TestEqual(TEXT("the sum is stamped with Outer"), Graph.Nodes[AddIndex].Region, 1);
	}
	if (TestTrue(TEXT("the scale is in the graph"), MultiplyIndex != INDEX_NONE))
	{
		TestEqual(TEXT("the scale is stamped with Inner"), Graph.Nodes[MultiplyIndex].Region, 2);
	}

	// A layout hint is carried through untouched, for the layout pass to apply after emission.
	if (TestEqual(TEXT("one layout hint"), Graph.LayoutHints.Num(), 1))
	{
		TestEqualSensitive(TEXT("the hint's kind"), Graph.LayoutHints[0].Kind, FString(TEXT("Comment")));
		TestEqualSensitive(TEXT("the hint's name"), Graph.LayoutHints[0].Name, FString(TEXT("Maths")));
		TestTrue(TEXT("the hint carries a size"), Graph.LayoutHints[0].bHasSize);
	}

	AddInfo(FString::Printf(TEXT("graph:\n%s"),
		*GraphShape(Graph, FDumpFlags{ /*bDebugNames*/ false, /*bRegions*/ true, /*bSourceSpans*/ false })));
	return true;
}

// =================================================================================================
// 6.9 -- products and function IO
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRProductsTest,
	"DreamShader.Lang2.IR.Products",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRProductsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	FIRRun Run;
	Lower(Run, TEXT(
		"/// @library MoonToon|Shared\n"
		"/// @desc UV channel select, then scale and offset.\n"
		"export float2 MF_ToonUV(float UVChannel = 0.0, float4 ScaleOffset = float4(1, 1, 0, 0))\n"
		"{\n"
		"    return float2(UVChannel, UVChannel) * ScaleOffset.xy + ScaleOffset.zw;\n"
		"}\n"
		"\n"
		"export void SampleTinted(Texture2D Tex, float2 UV, float3 InTint, out float3 RGB, out float Alpha)\n"
		"{\n"
		"    float4 Texel = Tex.Sample(UV);\n"
		"    RGB = Texel.rgb * InTint;\n"
		"    Alpha = Texel.a;\n"
		"}\n"));

	if (!Run.Module.IsValid())
	{
		AddError(FString::Printf(TEXT("the library did not lower: %s"), *Run.ErrorText()));
		return false;
	}

	TestEqual(TEXT("two products"), Run.Module->Products.Num(), 2);
	TestEqual(TEXT("both are material functions"), Run.Module->CountProducts(EIRProductKind::MaterialFunction), 2);
	TestNotNull(TEXT("a product can be found by name"), Run.Module->FindProduct(TEXT("MF_ToonUV")));

	// MF_ToonUV: two inputs, both optional (they have defaults), one output named Result.
	if (const FIRProduct* ToonUV = Run.Module->FindProduct(TEXT("MF_ToonUV")))
	{
		const FIRGraph& Graph = ToonUV->Graph;
		TestEqualSensitive(TEXT("the description came from @desc"), ToonUV->Description,
			FString(TEXT("UV channel select, then scale and offset.")));
		TestEqualSensitive(TEXT("the library path came from @library"), ToonUV->LibraryPath, FString(TEXT("MoonToon|Shared")));
		TestEqual(TEXT("a function product has no sink"), Graph.Sink, INDEX_NONE);

		if (TestEqual(TEXT("two function inputs"), Graph.FunctionInputs.Num(), 2))
		{
			// SortPriority is declaration order, DENSE (plan 6.2): 1.x's tie-break bug was that two
			// inputs could share a priority and the engine then reordered them silently.
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const FIRNode& Input = Graph.Nodes[Graph.FunctionInputs[Index]];
				TestEqual(
					FString::Printf(TEXT("input %d is a FunctionInput"), Index),
					static_cast<int32>(Input.Op), static_cast<int32>(EIROp::FunctionInput));
				TestEqualSensitive(
					*FString::Printf(TEXT("input %d SortPriority"), Index),
					PropText(Input, Prop::SortPriority),
					FString::FromInt(Index));
				TestEqualSensitive(
					*FString::Printf(TEXT("input %d IsOptional"), Index),
					PropText(Input, Prop::IsOptional),
					FString(TEXT("true")));
			}
			TestEqualSensitive(TEXT("input 0 name"), PropText(Graph.Nodes[Graph.FunctionInputs[0]], Prop::InputName), FString(TEXT("UVChannel")));
			TestEqualSensitive(TEXT("input 1 name"), PropText(Graph.Nodes[Graph.FunctionInputs[1]], Prop::InputName), FString(TEXT("ScaleOffset")));
		}

		if (TestEqual(TEXT("one function output"), Graph.FunctionOutputs.Num(), 1))
		{
			const FIRNode& Output = Graph.Nodes[Graph.FunctionOutputs[0]];
			TestEqualSensitive(TEXT("a return value is named Result"), PropText(Output, Prop::OutputName), FString(TEXT("Result")));
			TestEqualSensitive(TEXT("output SortPriority"), PropText(Output, Prop::SortPriority), FString(TEXT("0")));
		}
	}

	// SampleTinted: three inputs, two outputs from `out` parameters, in declaration order.
	if (const FIRProduct* SampleTinted = Run.Module->FindProduct(TEXT("SampleTinted")))
	{
		const FIRGraph& Graph = SampleTinted->Graph;
		if (TestEqual(TEXT("three function inputs"), Graph.FunctionInputs.Num(), 3))
		{
			TestEqualSensitive(TEXT("input 0 name"), PropText(Graph.Nodes[Graph.FunctionInputs[0]], Prop::InputName), FString(TEXT("Tex")));
			TestEqualSensitive(TEXT("input 0 type"), PropText(Graph.Nodes[Graph.FunctionInputs[0]], Prop::InputType), FString(TEXT("Texture2D")));
			TestEqualSensitive(TEXT("input 1 type"), PropText(Graph.Nodes[Graph.FunctionInputs[1]], Prop::InputType), FString(TEXT("Vector2")));
			TestEqualSensitive(TEXT("input 2 type"), PropText(Graph.Nodes[Graph.FunctionInputs[2]], Prop::InputType), FString(TEXT("Vector3")));
			TestEqualSensitive(TEXT("no default, so not optional"), PropText(Graph.Nodes[Graph.FunctionInputs[0]], Prop::IsOptional), FString(TEXT("false")));
		}
		if (TestEqual(TEXT("two function outputs"), Graph.FunctionOutputs.Num(), 2))
		{
			TestEqualSensitive(TEXT("output 0 name"), PropText(Graph.Nodes[Graph.FunctionOutputs[0]], Prop::OutputName), FString(TEXT("RGB")));
			TestEqualSensitive(TEXT("output 1 name"), PropText(Graph.Nodes[Graph.FunctionOutputs[1]], Prop::OutputName), FString(TEXT("Alpha")));
			TestEqualSensitive(TEXT("output 0 SortPriority"), PropText(Graph.Nodes[Graph.FunctionOutputs[0]], Prop::SortPriority), FString(TEXT("0")));
			TestEqualSensitive(TEXT("output 1 SortPriority"), PropText(Graph.Nodes[Graph.FunctionOutputs[1]], Prop::SortPriority), FString(TEXT("1")));
		}
	}

	// A call from one export to another is a FunctionCall to the LOCAL product, not an inline.
	{
		FIRRun Local;
		Lower(Local, TEXT(
			"export float Inner(float x) { return x * 2; }\n"
			"export float Outer(float x) { return Inner(x) + 1; }\n"));
		if (Local.Module.IsValid() && Local.Module->Products.Num() == 2)
		{
			const FIRProduct* OuterProduct = Local.Module->FindProduct(TEXT("Outer"));
			if (TestNotNull(TEXT("the outer product exists"), OuterProduct))
			{
				const int32 CallIndex = FindOp(OuterProduct->Graph, EIROp::FunctionCall);
				if (TestTrue(TEXT("calling a sibling export makes a FunctionCall"), CallIndex != INDEX_NONE))
				{
					const FIRNode& Call = OuterProduct->Graph.Nodes[CallIndex];
					TestNotEqual(
						TEXT("and it names the local product rather than an asset path"),
						PropText(Call, Prop::LocalFunction), FString(TEXT("<absent>")));
				}
			}
		}
		else
		{
			AddInfo(FString::Printf(TEXT("the local-call library did not lower: %s"), *Local.ErrorText()));
		}
	}

	return true;
}

// =================================================================================================
// 6.10 -- reflected arguments and the const-property preference
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRReflectedArgumentsTest,
	"DreamShader.Lang2.IR.ReflectedArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRReflectedArgumentsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	FIRRun Run;
	Lower(Run, TEXT(
		"uniform float3 Colour = float3(1, 0, 0);\n"
		"uniform float Alpha = 0.25;\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    m.BaseColor = UE.LinearInterpolate(A = Colour, B = float3(0, 0, 0), Alpha = 0.5);\n"
		"    m.EmissiveColor = UE.LinearInterpolate(A = Colour, B = float3(0, 0, 0), Alpha = Alpha);\n"
		"}\n"));

	const FIRProduct* Found = Product(*this, Run);
	if (!Found)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		return false;
	}
	const FIRGraph& Graph = Found->Graph;

	TestEqual(TEXT("two reflected nodes"), CountOp(Graph, EIROp::Reflected), 2);

	// A constant argument to a pin with a Const* twin is written as the PROPERTY -- matching what
	// the 1.x generator produced, which is what makes the parity dump comparable at all.
	const int32 ConstantAlpha = FindOp(Graph, EIROp::Reflected, 0);
	const int32 DynamicAlpha = FindOp(Graph, EIROp::Reflected, 1);
	if (ConstantAlpha != INDEX_NONE && DynamicAlpha != INDEX_NONE)
	{
		const FIRNode& WithConstant = Graph.Nodes[ConstantAlpha];
		const FIRNode& WithDynamic = Graph.Nodes[DynamicAlpha];

		TestEqualSensitive(
			TEXT("a constant Alpha becomes ConstAlpha"),
			PropText(WithConstant, TEXT("ConstAlpha")), FString(TEXT("0.5")));
		TestEqualSensitive(
			TEXT("and leaves the Alpha pin unwired"),
			InputText(WithConstant, TEXT("Alpha")), FString(TEXT("<absent>")));

		TestEqualSensitive(
			TEXT("a parameter Alpha writes no ConstAlpha"),
			PropText(WithDynamic, TEXT("ConstAlpha")), FString(TEXT("<absent>")));
		TestNotEqual(
			TEXT("and wires the Alpha pin instead"),
			InputText(WithDynamic, TEXT("Alpha")), FString(TEXT("<absent>")));

		// Both name the class the same way, so the emitter never has to guess.
		TestEqualSensitive(TEXT("ClassName"), WithConstant.ClassName, FString(TEXT("LinearInterpolate")));
		TestTrue(TEXT("and carry the catalog index"), WithConstant.CatalogIndex != INDEX_NONE);
	}

	// CONTRACT 6.13 #23: a reflected class with NO outputs is a legal statement ROOT. Nothing can
	// read it, so the prune pass has to know it is a root rather than dead weight -- the opposite of
	// the rule it applies to every other node with no users.
	{
		FIRRun Statement;
		Lower(Statement, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    UE.ClearCoatNormalCustomOutput(Input = float3(0, 0, 1));\n"
			"}\n"));

		const FIRProduct* StatementProduct = Product(*this, Statement);
		if (StatementProduct)
		{
			const FIRGraph& StatementGraph = StatementProduct->Graph;
			int32 CustomOutputs = 0;
			for (const FIRNode& Node : StatementGraph.Nodes)
			{
				if (Node.Op == EIROp::Reflected && Node.Outputs.Num() == 0)
				{
					++CustomOutputs;
					TestTrue(TEXT("the custom output has its input wired"), Node.Inputs.Num() >= 1);
				}
			}
			TestEqual(TEXT("a zero-output reflected node survives the prune pass"), CustomOutputs, 1);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Statement.ErrorText()));
		}
	}

	AddInfo(FString::Printf(TEXT("graph:\n%s"), *GraphShape(Graph)));
	return true;
}

// =================================================================================================
// The passes are idempotent
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRPassIdempotenceTest,
	"DreamShader.Lang2.IR.PassIdempotence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRPassIdempotenceTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// Running the passes twice must change nothing the second time. A pass that is not idempotent
	// is a pass whose output depends on how many times the pipeline happened to call it -- and the
	// pipeline calls it once today and could call it twice tomorrow (a rebuild after an include
	// changed, say). The failure is invisible in a single run and fatal in a digest.
	FIRRun Run;
	Lower(Run, TEXT(
		"uniform Texture2D Tex;\n"
		"uniform float Gain = 1;\n"
		"/// @static\n"
		"uniform bool UseWarm = true;\n"
		"float Scale(float x) { return x * Gain; }\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
		"    float4 Texel = Tex.Sample(UV);\n"
		"    float3 Warm = Texel.rgb * float3(1, 0.5, 0.2);\n"
		"    float3 Cool = Texel.rgb * float3(0.2, 0.5, 1);\n"
		"    float3 Colour = float3(0, 0, 0);\n"
		"    if (UseWarm) { Colour = Warm; } else { Colour = Cool; }\n"
		"    m.EmissiveColor = Colour * Scale(2);\n"
		"    m.Opacity = Texel.a;\n"
		"}\n"));

	const FIRProduct* Found = Product(*this, Run);
	if (!Found)
	{
		AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		return false;
	}

	const FDumpFlags Flags{ /*bDebugNames*/ true, /*bRegions*/ true, /*bSourceSpans*/ false };
	const FString First = GraphShape(Found->Graph, Flags);

	FLangDiagnosticSink Sink;
	FIRPassOptions PassOptions;
	RunDreamShaderIRPasses(*Run.Module, PassOptions, Sink);

	TestFalse(TEXT("a second pass run reports no errors"), Sink.HasErrors());

	const FString Second = GraphShape(Run.Module->Products[0].Graph, Flags);
	const bool bEqual = First.Equals(Second, ESearchCase::CaseSensitive);
	TestTrue(TEXT("running the passes twice produces the same graph"), bEqual);
	if (!bEqual)
	{
		AddInfo(FString::Printf(TEXT("first:\n%s"), *First));
		AddInfo(FString::Printf(TEXT("second:\n%s"), *Second));
	}

	// The same source lowered twice from scratch also has to agree: nothing in the builder may
	// depend on address order, iteration order or a counter that survives between runs.
	FIRRun Again;
	Lower(Again, TEXT(
		"uniform Texture2D Tex;\n"
		"uniform float Gain = 1;\n"
		"/// @static\n"
		"uniform bool UseWarm = true;\n"
		"float Scale(float x) { return x * Gain; }\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
		"    float4 Texel = Tex.Sample(UV);\n"
		"    float3 Warm = Texel.rgb * float3(1, 0.5, 0.2);\n"
		"    float3 Cool = Texel.rgb * float3(0.2, 0.5, 1);\n"
		"    float3 Colour = float3(0, 0, 0);\n"
		"    if (UseWarm) { Colour = Warm; } else { Colour = Cool; }\n"
		"    m.EmissiveColor = Colour * Scale(2);\n"
		"    m.Opacity = Texel.a;\n"
		"}\n"));
	if (Again.Module.IsValid() && Again.Module->Products.Num() > 0)
	{
		TestEqualSensitive(
			TEXT("two independent lowerings of one source agree"),
			GraphShape(Again.Module->Products[0].Graph, Flags),
			First);

		// And so does I1's own dump, which is what the corpus goldens are made of.
		TestEqualSensitive(TEXT("and so does DumpDreamShaderIRText"), Again.IRText, Run.IRText);
	}

	return true;
}

// =================================================================================================
// The validator, on graphs built by hand
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRValidatorTest,
	"DreamShader.Lang2.IR.Validator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRValidatorTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	const FBuiltinCatalog& Catalog = UE::DreamShader::Editor::Private::Tests::GetDreamShaderTestBuiltinCatalog();

	// The validator's whole reason for existing is the graph the BUILDER should never make; these
	// are built by hand because no source produces them, and an untested validator is a validator
	// nobody finds out is broken until a malformed graph reaches the emitter and crashes it.

	// A well-formed module first, so a later failure means what it says.
	{
		FIRModule Module;
		Module.SourceFilePath = TEXT("Test.dss");
		FIRProduct& ProductRef = Module.Products.AddDefaulted_GetRef();
		ProductRef.Kind = EIRProductKind::Material;
		ProductRef.Name = TEXT("M_Valid");

		const int32 Constant = ProductRef.Graph.AddNode(MakeConstantNode(1.0));
		FIRNode Sink;
		Sink.Op = EIROp::MaterialSink;
		Sink.Inputs.Add({ FString(TEXT("Opacity")), FIRValue{ Constant, 0 } });
		ProductRef.Graph.Sink = ProductRef.Graph.AddNode(MoveTemp(Sink));

		FLangDiagnosticSink Diagnostics;
		TestTrue(TEXT("a well-formed module validates"), ValidateDreamShaderIR(Module, Catalog, Diagnostics));
		TestFalse(TEXT("and reports nothing"), Diagnostics.HasErrors());
	}

	// A dangling operand: an FIRValue naming a node index that is not in the graph.
	{
		FIRModule Module;
		Module.SourceFilePath = TEXT("Test.dss");
		FIRProduct& ProductRef = Module.Products.AddDefaulted_GetRef();
		ProductRef.Kind = EIRProductKind::Material;
		ProductRef.Name = TEXT("M_Dangling");

		const int32 Constant = ProductRef.Graph.AddNode(MakeConstantNode(1.0));
		const int32 Saturate = ProductRef.Graph.AddNode(MakeUnaryNode(EIROp::Saturate, FIRValue{ 99, 0 }, FIRType::Float(1)));

		FIRNode Sink;
		Sink.Op = EIROp::MaterialSink;
		Sink.Inputs.Add({ FString(TEXT("Opacity")), FIRValue{ Saturate, 0 } });
		ProductRef.Graph.Sink = ProductRef.Graph.AddNode(MoveTemp(Sink));
		(void)Constant;

		FLangDiagnosticSink Diagnostics;
		const bool bValid = ValidateDreamShaderIR(Module, Catalog, Diagnostics);
		TestFalse(TEXT("a dangling operand does not validate"), bValid);
		TestTrue(
			TEXT("and is reported in the validator's own range"),
			HasCodeInRange(SinkErrors(Diagnostics), 4300, 4349));
	}

	// Two sinks in one material product: the sink is the material's outputs, and there is one set.
	{
		FIRModule Module;
		Module.SourceFilePath = TEXT("Test.dss");
		FIRProduct& ProductRef = Module.Products.AddDefaulted_GetRef();
		ProductRef.Kind = EIRProductKind::Material;
		ProductRef.Name = TEXT("M_TwoSinks");

		const int32 Constant = ProductRef.Graph.AddNode(MakeConstantNode(1.0));

		FIRNode FirstSink;
		FirstSink.Op = EIROp::MaterialSink;
		FirstSink.Inputs.Add({ FString(TEXT("Opacity")), FIRValue{ Constant, 0 } });
		ProductRef.Graph.Sink = ProductRef.Graph.AddNode(MoveTemp(FirstSink));

		FIRNode SecondSink;
		SecondSink.Op = EIROp::MaterialSink;
		SecondSink.Inputs.Add({ FString(TEXT("Roughness")), FIRValue{ Constant, 0 } });
		ProductRef.Graph.AddNode(MoveTemp(SecondSink));

		FLangDiagnosticSink Diagnostics;
		TestFalse(TEXT("two material sinks do not validate"), ValidateDreamShaderIR(Module, Catalog, Diagnostics));
		TestTrue(
			TEXT("and are reported in the validator's own range"),
			HasCodeInRange(SinkErrors(Diagnostics), 4300, 4349));
	}

	// A cycle: two nodes feeding each other. TopologicalOrder() has no answer for this, so the
	// validator has to catch it before the emitter walks the graph forever.
	{
		FIRModule Module;
		Module.SourceFilePath = TEXT("Test.dss");
		FIRProduct& ProductRef = Module.Products.AddDefaulted_GetRef();
		ProductRef.Kind = EIRProductKind::Material;
		ProductRef.Name = TEXT("M_Cycle");

		const int32 First = ProductRef.Graph.AddNode(MakeUnaryNode(EIROp::Saturate, FIRValue{ 1, 0 }, FIRType::Float(1)));
		const int32 Second = ProductRef.Graph.AddNode(MakeUnaryNode(EIROp::Abs, FIRValue{ 0, 0 }, FIRType::Float(1)));

		FIRNode Sink;
		Sink.Op = EIROp::MaterialSink;
		Sink.Inputs.Add({ FString(TEXT("Opacity")), FIRValue{ First, 0 } });
		ProductRef.Graph.Sink = ProductRef.Graph.AddNode(MoveTemp(Sink));
		(void)Second;

		FLangDiagnosticSink Diagnostics;
		TestFalse(TEXT("a cycle does not validate"), ValidateDreamShaderIR(Module, Catalog, Diagnostics));
		TestTrue(
			TEXT("and is reported in the validator's own range"),
			HasCodeInRange(SinkErrors(Diagnostics), 4300, 4349));
	}

	// A material function product with a sink, and a material product without one: the two shapes
	// the Sink field distinguishes, each wrong in the other's place.
	{
		FIRModule Module;
		Module.SourceFilePath = TEXT("Test.dss");
		FIRProduct& ProductRef = Module.Products.AddDefaulted_GetRef();
		ProductRef.Kind = EIRProductKind::Material;
		ProductRef.Name = TEXT("M_NoSink");
		ProductRef.Graph.AddNode(MakeConstantNode(1.0));

		FLangDiagnosticSink Diagnostics;
		TestFalse(TEXT("a material product without a sink does not validate"), ValidateDreamShaderIR(Module, Catalog, Diagnostics));
		TestTrue(
			TEXT("and is reported in the validator's own range"),
			HasCodeInRange(SinkErrors(Diagnostics), 4300, 4349));
	}

	return true;
}


// =================================================================================================
// 6.13 -- where a `material` may and may not go, and where a matrix may not go at all
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRCustomBoundariesTest,
	"DreamShader.Lang2.IR.CustomBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRCustomBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// ---- the POSITIVE half: a `material` crossing into a pin is still MakeMaterialAttributes ----

	// A reflected class whose pins carry MaterialAttributes. The field map becomes a real node here
	// because it has to: a pin takes a value, and there is no pin shaped like a map.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    m.Roughness = 0.5;\n"
			"    m.Opacity = UE.BlendMaterialAttributes(A = m, B = m, Alpha = 0.25).Roughness;\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			const FIRGraph& Graph = Found->Graph;
			TestTrue(
				TEXT("a material passed to a reflected pin becomes MakeMaterialAttributes"),
				CountOp(Graph, EIROp::MakeMaterialAttributes) >= 1);
			TestTrue(
				TEXT("and reading a field back off that pin's result is GetMaterialAttributes"),
				CountOp(Graph, EIROp::GetMaterialAttributes) >= 1);
			AddInfo(FString::Printf(TEXT("reflected-pin graph:\n%s"), *GraphShape(Graph)));
			AddInfo(FString::Printf(TEXT("reflected-pin diagnostics: %s"), *Run.ErrorText()));
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		}
	}

	// A FunctionCall input, through an extern: the same rule, the other kind of pin.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"/// @asset /Game/Functions/MF_Shade\n"
			"extern float3 MF_Shade(material In);\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    m.EmissiveColor = MF_Shade(m);\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			const FIRGraph& Graph = Found->Graph;
			TestTrue(
				TEXT("a material passed to a FunctionCall input becomes MakeMaterialAttributes"),
				CountOp(Graph, EIROp::MakeMaterialAttributes) >= 1);
			TestEqual(TEXT("and the call itself is one FunctionCall"), CountOp(Graph, EIROp::FunctionCall), 1);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		}
	}

	// ---- the NEGATIVE half: a `material` may NOT cross into a @custom (CONTRACT 6.13) ----

	// The engine has no pin typed like a MaterialAttributes input on a Custom node, so there is
	// nothing to make the node out of. Refusing at the boundary is the only honest answer; the
	// alternative -- a MakeMaterialAttributes feeding a Custom input -- compiles here and fails in
	// the shader compiler, which is the worst place to find out.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"/// @custom\n"
			"float3 Shade(material In, float3 Colour)\n"
			"{\n"
			"    return Colour;\n"
			"}\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    m.EmissiveColor = Shade(m, float3(1, 1, 1));\n"
			"}\n"));

		TestTrue(
			FString::Printf(TEXT("a material passed into a @custom is refused (actual: %s)"), *Run.ErrorText()),
			Run.Errors.Num() > 0);
		// DSH6252 is the code the amendment names; the range check keeps the test honest if the
		// refusal turns out to belong to the binder's function-kind range instead.
		TestTrue(
			FString::Printf(TEXT("and named by a custom-HLSL or function-kind code (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH6252"))
				|| HasCodeInRange(Run.Errors, 6250, 6299)
				|| HasCodeInRange(Run.Errors, 6200, 6249));

		// And no such node was quietly made on the way to the refusal.
		if (Run.Module.IsValid() && Run.Module->Products.Num() > 0)
		{
			TestEqual(
				TEXT("and no MakeMaterialAttributes was made feeding a Custom input"),
				CountOp(Run.Module->Products[0].Graph, EIROp::Custom), 0);
		}
	}

	// ---- matrices: nowhere in a graph body, @custom pins included ----

	{
		// The matrix is declared and never read: DSH4361 is about a matrix VALUE reaching the
		// lowering pass, which the declaration's initializer already is. Reading a row (`R[0][0]`)
		// would not get here at all -- the binder refuses that earlier with its own DSH4244 -- so a
		// source written that way would be testing the binder's index rule, not this one.
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float3x3 Rotation = float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);\n"
			"    m.Normal = float3(0, 0, 1);\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a matrix in a graph body is refused: DSH4361 (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4361")));
	}

	{
		// There is no @custom matrix pin either: `@custom` widens what the BODY may say, not what
		// its signature may carry across the boundary.
		FIRRun Run;
		Lower(Run, TEXT(
			"/// @custom\n"
			"float3 Rotate(float3x3 Rotation, float3 V)\n"
			"{\n"
			"    return mul(Rotation, V);\n"
			"}\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.Normal = Rotate(float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1), float3(0, 0, 1));\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a matrix on a @custom pin is refused (actual: %s)"), *Run.ErrorText()),
			Run.Errors.Num() > 0);
		TestTrue(
			FString::Printf(TEXT("and named by the matrix code or a custom-HLSL one (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4361")) || HasCodeInRange(Run.Errors, 6250, 6299));
	}

	// ---- the Custom node a legal @custom DOES make ----

	{
		FIRRun Run;
		Lower(Run, TEXT(
			"/// @custom\n"
			"float3 Posterise(float3 Colour, float Steps)\n"
			"{\n"
			"    return floor(Colour * Steps) / Steps;\n"
			"}\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.EmissiveColor = Posterise(float3(1, 0.5, 0.25), 4);\n"
			"}\n"));

		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			const FIRGraph& Graph = Found->Graph;
			TestEqual(TEXT("a @custom call makes one Custom node"), CountOp(Graph, EIROp::Custom), 1);

			const int32 CustomIndex = FindOp(Graph, EIROp::Custom);
			if (CustomIndex != INDEX_NONE)
			{
				const FIRNode& Custom = Graph.Nodes[CustomIndex];
				TestEqualSensitive(TEXT("the node names the function"), Custom.ClassName, FString(TEXT("Posterise")));

				// The generated code opens with two marker lines, so `Contains` -- never an exact
				// match on the whole string. The markers are unit H's, they carry a line number,
				// and pinning them here would make this test fail every time the body moved.
				const FString Code = PropText(Custom, Prop::Code);
				TestTrue(
					TEXT("the code carries H's second marker line"),
					Code.Contains(TEXT("// DreamShader custom: Posterise line "), ESearchCase::CaseSensitive));
				TestTrue(
					TEXT("and the author's body"),
					Code.Contains(TEXT("floor(Colour * Steps) / Steps"), ESearchCase::CaseSensitive));

				// Two inputs, named after the parameters: the pins are the signature.
				TestEqual(TEXT("two named inputs"), Custom.Inputs.Num(), 2);
				TestNotEqual(TEXT("Colour is wired"), InputText(Custom, TEXT("Colour")), FString(TEXT("<absent>")));
				TestNotEqual(TEXT("Steps is wired"), InputText(Custom, TEXT("Steps")), FString(TEXT("<absent>")));
			}
		}
		else
		{
			AddInfo(FString::Printf(TEXT("errors: %s"), *Run.ErrorText()));
		}
	}

	return true;
}

// =================================================================================================
// 6.2 / 6.13, followed up -- a whole material, and what a branch may do with one
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRWholeMaterialTest,
	"DreamShader.Lang2.IR.WholeMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRWholeMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// A material replaced as a whole reaches the sink as ONE attribute set, through its
	// MaterialAttributes input, with what was written on top folded into a SetMaterialAttributes. The
	// replacement used to lower and the sink never heard of it: nothing wired, and no message.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    m = UE.BlendMaterialAttributes(A = m, B = m, Alpha = 0.25);\n"
			"    m.Roughness = 0.5;\n"
			"}\n"));

		TestEqual(FString::Printf(TEXT("a whole assignment lowers cleanly (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found && TestTrue(TEXT("the graph records its sink"), Found->Graph.Sink != INDEX_NONE))
		{
			const FIRGraph& Graph = Found->Graph;
			const FIRNode& Sink = Graph.Nodes[Graph.Sink];
			TestEqual(TEXT("the sink takes one input"), Sink.Inputs.Num(), 1);
			const FIRInput* Whole = Sink.FindInput(FString(TEXT("MaterialAttributes")));
			if (TestNotNull(TEXT("and it is the MaterialAttributes input"), Whole) && Graph.Nodes.IsValidIndex(Whole->Value.Node))
			{
				const FIRNode& Set = Graph.Nodes[Whole->Value.Node];
				TestEqual(TEXT("which the write on top made a SetMaterialAttributes"), static_cast<int32>(Set.Op), static_cast<int32>(EIROp::SetMaterialAttributes));
				TestNotNull(TEXT("that sets Roughness"), Set.FindInput(FString(TEXT("Roughness"))));
				const FIRInput* Base = Set.FindInput(FString(TEXT("MaterialAttributes")));
				TestTrue(
					TEXT("over the BlendMaterialAttributes node"),
					Base != nullptr
						&& Graph.Nodes.IsValidIndex(Base->Value.Node)
						&& Graph.Nodes[Base->Value.Node].Op == EIROp::Reflected
						&& Graph.Nodes[Base->Value.Node].ClassName.Equals(TEXT("BlendMaterialAttributes"), ESearchCase::CaseSensitive));
			}
			TestEqual(TEXT("the material passed to A and B is one MakeMaterialAttributes"), CountOp(Graph, EIROp::MakeMaterialAttributes), 1);
			AddInfo(FString::Printf(TEXT("whole-material graph:\n%s"), *GraphShape(Graph)));
		}
	}

	// Nothing written on top: the set IS the node's output, and no SetMaterialAttributes is made for it.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    m = UE.BlendMaterialAttributes(A = m, B = m, Alpha = 0.25);\n"
			"}\n"));

		TestEqual(FString::Printf(TEXT("a plain whole assignment lowers cleanly (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found && Found->Graph.Sink != INDEX_NONE)
		{
			const FIRGraph& Graph = Found->Graph;
			const FIRInput* Whole = Graph.Nodes[Graph.Sink].FindInput(FString(TEXT("MaterialAttributes")));
			TestTrue(
				TEXT("the sink's MaterialAttributes input reads the reflected node directly"),
				Whole != nullptr && Graph.Nodes.IsValidIndex(Whole->Value.Node) && Graph.Nodes[Whole->Value.Node].Op == EIROp::Reflected);
			TestEqual(TEXT("and no SetMaterialAttributes is made"), CountOp(Graph, EIROp::SetMaterialAttributes), 0);
		}
	}

	// A branch cannot choose between two whole materials: DSH4375, and not DSH4372 on top of it.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform bool Enabled = true;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    if (Enabled) { m = UE.BlendMaterialAttributes(A = m, B = m, Alpha = 0.5); }\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a whole material replaced in one arm is refused: DSH4375 (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4375")));
		TestFalse(
			FString::Printf(TEXT("and it is not also reported as a one-armed write (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4372")));
	}

	// The attribute that holds a whole set, written with a different one in each arm: the same refusal
	// one attribute down -- and never a float1 conditional over two materials, which is what it was.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform bool Enabled = true;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    if (Enabled) { m.MaterialAttributes = UE.BlendMaterialAttributes(A = m, B = m, Alpha = 0.25); }\n"
			"    else { m.MaterialAttributes = UE.BlendMaterialAttributes(A = m, B = m, Alpha = 0.75); }\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("two different sets in m.MaterialAttributes are refused: DSH4375 (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4375")));
		if (Run.Module.IsValid() && Run.Module->Products.Num() > 0)
		{
			const FIRGraph& Graph = Run.Module->Products[0].Graph;
			int32 MaterialOperands = 0;
			for (const FIRNode& Node : Graph.Nodes)
			{
				if (Node.Op != EIROp::Select && Node.Op != EIROp::Compare && Node.Op != EIROp::StaticSwitch)
				{
					continue;
				}
				for (const FIRValue& Operand : Node.Operands)
				{
					if (Graph.IsValidValue(Operand) && Graph.TypeOf(Operand).IsMaterial())
					{
						++MaterialOperands;
					}
				}
			}
			TestEqual(TEXT("and no conditional node was made over a material value"), MaterialOperands, 0);
		}
	}

	// A field of the material held in an attribute has no write of its own (DSH4377) -- and, above all,
	// the write must not land on the material's own attribute of the same name, which it used to.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.MaterialAttributes = UE.BlendMaterialAttributes(A = m, B = m, Alpha = 0.25);\n"
			"    m.MaterialAttributes.Roughness = 0.5;\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a write into m.MaterialAttributes.Roughness is refused: DSH4377 (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4377")));
		if (Run.Module.IsValid() && Run.Module->Products.Num() > 0 && Run.Module->Products[0].Graph.Sink != INDEX_NONE)
		{
			const FIRGraph& Graph = Run.Module->Products[0].Graph;
			TestNull(TEXT("and m.Roughness was not written in its place"), Graph.Nodes[Graph.Sink].FindInput(FString(TEXT("Roughness"))));
		}
	}

	// One component of an attribute is a read-modify-write, like one component of a local: the other two
	// channels survive. The scalar used to become the whole BaseColor.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float Red = 0.25;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(0, 0.5, 1);\n"
			"    m.BaseColor.x = Red;\n"
			"}\n"));
		TestEqual(FString::Printf(TEXT("a component write to an attribute lowers cleanly (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found && Found->Graph.Sink != INDEX_NONE)
		{
			const FIRGraph& Graph = Found->Graph;
			const FIRInput* BaseColor = Graph.Nodes[Graph.Sink].FindInput(FString(TEXT("BaseColor")));
			TestTrue(
				TEXT("BaseColor is still a float3"),
				BaseColor != nullptr && Graph.IsValidValue(BaseColor->Value) && Graph.TypeOf(BaseColor->Value).GraphComponentCount() == 3);
			TestEqual(TEXT("and the written component reads the parameter"), CountOp(Graph, EIROp::Parameter), 1);
		}
	}

	// A component of an attribute that holds nothing yet has no other components to keep.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.Normal.z = 1;\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a component write to an attribute nothing set is a read before a write: DSH4370 (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4370")));
	}

	// A compound assignment reads first, and in a layer that read is off the incoming material. It used
	// to find nothing recorded and leave the attribute untouched.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"/// @layer\n"
			"export void L_Case(inout material m)\n"
			"{\n"
			"    m.Roughness *= 0.5;\n"
			"}\n"));
		TestEqual(FString::Printf(TEXT("a compound write in a layer lowers cleanly (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found)
		{
			TestEqual(TEXT("the incoming Roughness is read with GetMaterialAttributes"), CountOp(Found->Graph, EIROp::GetMaterialAttributes), 1);
			TestEqual(TEXT("the result is set back with SetMaterialAttributes"), CountOp(Found->Graph, EIROp::SetMaterialAttributes), 1);
			TestEqual(TEXT("through one Multiply"), CountOp(Found->Graph, EIROp::Multiply), 1);
		}
	}

	return true;
}

// =================================================================================================
// A local nothing has assigned: component writes land, and a read of it says so
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRUnsetLocalsTest,
	"DreamShader.Lang2.IR.UnsetLocals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRUnsetLocalsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	const auto CountCode = [](const FIRRun& Run, const TCHAR* Code) -> int32
	{
		const FString Needle = FString::Printf(TEXT("%s:"), Code);
		int32 Count = 0;
		for (const FString& Line : Run.Errors)
		{
			if (Line.StartsWith(Needle, ESearchCase::CaseSensitive))
			{
				++Count;
			}
		}
		return Count;
	};

	// Component by component into a local declared without a value: every write lands. The first write
	// used to shrink the local to one component, and the other two were dropped without a word.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float Red = 0.25;\n"
			"uniform float Green = 0.5;\n"
			"uniform float Blue = 0.75;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float3 Colour;\n"
			"    Colour.x = Red;\n"
			"    Colour.y = Green;\n"
			"    Colour.z = Blue;\n"
			"    m.BaseColor = Colour;\n"
			"}\n"));
		TestEqual(FString::Printf(TEXT("component writes lower cleanly (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found && Found->Graph.Sink != INDEX_NONE)
		{
			const FIRGraph& Graph = Found->Graph;
			TestEqual(TEXT("all three writes reach the sink, so no parameter is pruned"), CountOp(Graph, EIROp::Parameter), 3);
			const FIRInput* BaseColor = Graph.Nodes[Graph.Sink].FindInput(FString(TEXT("BaseColor")));
			TestTrue(
				TEXT("and BaseColor is a float3"),
				BaseColor != nullptr && Graph.IsValidValue(BaseColor->Value) && Graph.TypeOf(BaseColor->Value).GraphComponentCount() == 3);
			AddInfo(FString::Printf(TEXT("component-write graph:\n%s"), *GraphShape(Graph)));
		}
	}

	// The components nobody writes are zero -- the default the 1.x generator gives a declaration with no
	// initializer (CreateDefaultValue) -- so after folding the whole local is one constant.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float3 Colour;\n"
			"    Colour.y = 0.5;\n"
			"    m.BaseColor = Colour;\n"
			"}\n"));
		TestEqual(FString::Printf(TEXT("a single component write lowers cleanly (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found && Found->Graph.Sink != INDEX_NONE)
		{
			const FIRGraph& Graph = Found->Graph;
			const FIRInput* BaseColor = Graph.Nodes[Graph.Sink].FindInput(FString(TEXT("BaseColor")));
			if (TestNotNull(TEXT("BaseColor is written"), BaseColor) && Graph.Nodes.IsValidIndex(BaseColor->Value.Node))
			{
				const FIRNode& Written = Graph.Nodes[BaseColor->Value.Node];
				TestEqual(TEXT("it folds to a Constant"), static_cast<int32>(Written.Op), static_cast<int32>(EIROp::Constant));
				TestEqualSensitive(TEXT("of the written component over zeros"), PropText(Written, Prop::Value), FString(TEXT("(0, 0.5, 0)")));
			}
		}
	}

	// A read of a local nothing has assigned: DSH4376, once, however many times it is read.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float Unset;\n"
			"    m.Opacity = Unset;\n"
			"    m.Roughness = Unset;\n"
			"}\n"));
		TestEqual(
			FString::Printf(TEXT("reading a never-assigned local is DSH4376, said once (actual: %s)"), *Run.ErrorText()),
			CountCode(Run, TEXT("DSH4376")), 1);
	}

	// A compound assignment reads first, so it is the same error, and the read after it is not a second.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float Total;\n"
			"    Total += 1;\n"
			"    m.Opacity = Total;\n"
			"}\n"));
		TestEqual(
			FString::Printf(TEXT("a compound assignment to a never-assigned local is DSH4376, said once (actual: %s)"), *Run.ErrorText()),
			CountCode(Run, TEXT("DSH4376")), 1);
	}

	// A field of a struct local counts the same way, and the message names the field.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"struct Pair\n"
			"{\n"
			"    float A;\n"
			"    float B;\n"
			"};\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    Pair P;\n"
			"    P.A = 1;\n"
			"    m.Opacity = P.A;\n"
			"    m.Roughness = P.B;\n"
			"}\n"));
		TestEqual(
			FString::Printf(TEXT("reading the one field nothing wrote is DSH4376, once (actual: %s)"), *Run.ErrorText()),
			CountCode(Run, TEXT("DSH4376")), 1);
		TestTrue(
			FString::Printf(TEXT("and the message names the field (actual: %s)"), *Run.ErrorText()),
			Run.ErrorText().Contains(TEXT("'P.B'"), ESearchCase::CaseSensitive));
	}

	// Assigned in one arm only is DSH4372, as it was -- not DSH4376 as well.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"uniform float Threshold = 0.5;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float2 UV = UE.TexCoord(CoordinateIndex = 0);\n"
			"    float Value;\n"
			"    if (UV.x > Threshold) { Value = 1; }\n"
			"    m.Opacity = Value;\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a one-armed assignment is still DSH4372 (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4372")));
		TestFalse(
			FString::Printf(TEXT("and not DSH4376 on top of it (actual: %s)"), *Run.ErrorText()),
			HasCode(Run, TEXT("DSH4376")));
	}

	// No false positives: an `out` argument, an `inout` argument, and an unrolled loop whose first trip
	// reads the local only in an arm the compiler can tell does not run.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"void Fill(out float Result) { Result = 0.5; }\n"
			"void Keep(inout float Result) { Result = 0.25; }\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float Filled;\n"
			"    Fill(Filled);\n"
			"    float Kept;\n"
			"    Keep(Kept);\n"
			"    float Running;\n"
			"    for (int i = 0; i < 3; ++i)\n"
			"    {\n"
			"        if (i > 0) { Running = Running + 1; }\n"
			"        else { Running = 0; }\n"
			"    }\n"
			"    m.Opacity = Filled + Kept + Running;\n"
			"}\n"));
		TestEqual(
			FString::Printf(TEXT("out, inout and a loop's dead first-trip arm raise nothing (errors: %s)"), *Run.ErrorText()),
			Run.Errors.Num(), 0);
	}

	return true;
}

// =================================================================================================
// A node of channel views stands for its whole value (FIX3-Binder, rule 2)
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2IRChannelViewsTest,
	"DreamShader.Lang2.IR.ChannelViews",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2IRChannelViewsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2IRTests;

	// `float4 VC = UE.VertexColorViews();` is the whole float4, and every read of it through the local is
	// the pin 1.x wires: `.rgb` the RGB view, `.a` the A view, `.r` the R view. The local used to hold the
	// RGB view alone, and `.a` became a mask wider than its operand.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float4 VC = UE.VertexColorViews();\n"
			"    m.BaseColor = VC.rgb;\n"
			"    m.Opacity = VC.a;\n"
			"    m.Roughness = VC.r;\n"
			"}\n"));
		TestEqual(FString::Printf(TEXT("reads through a channel-view local lower and validate (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found && Found->Graph.Sink != INDEX_NONE)
		{
			const FIRGraph& Graph = Found->Graph;
			const FIRNode& Sink = Graph.Nodes[Graph.Sink];
			const auto ReadsView = [&Graph, &Sink](const TCHAR* Pin, int32 Output) -> bool
			{
				const FIRInput* Input = Sink.FindInput(FString(Pin));
				return Input != nullptr
					&& Graph.Nodes.IsValidIndex(Input->Value.Node)
					&& Graph.Nodes[Input->Value.Node].Op == EIROp::Reflected
					&& Input->Value.Output == Output;
			};
			TestTrue(TEXT("VC.rgb is the RGB view (output 0)"), ReadsView(TEXT("BaseColor"), 0));
			TestTrue(TEXT("VC.a is the A view (output 4)"), ReadsView(TEXT("Opacity"), 4));
			TestTrue(TEXT("VC.r is the R view (output 1), not a mask over RGB"), ReadsView(TEXT("Roughness"), 1));
			TestEqual(TEXT("one node carries all three"), CountOp(Graph, EIROp::Reflected), 1);
			TestEqual(TEXT("no ComponentMask is left"), CountOp(Graph, EIROp::Swizzle), 0);
			TestEqual(TEXT("and the widened value, read only through its views, is pruned"), CountOp(Graph, EIROp::Append), 0);
			AddInfo(FString::Printf(TEXT("channel-view graph:\n%s"), *GraphShape(Graph)));
		}
	}

	// The same read written on the call is the binder's member form: the A view directly.
	{
		FIRRun Run;
		Lower(Run, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.Opacity = UE.VertexColorViews().a;\n"
			"}\n"));
		TestEqual(FString::Printf(TEXT("a view read off the call lowers cleanly (errors: %s)"), *Run.ErrorText()), Run.Errors.Num(), 0);
		const FIRProduct* Found = Product(*this, Run);
		if (Found && Found->Graph.Sink != INDEX_NONE)
		{
			const FIRGraph& Graph = Found->Graph;
			const FIRInput* Opacity = Graph.Nodes[Graph.Sink].FindInput(FString(TEXT("Opacity")));
			TestTrue(TEXT("and reads output 4"), Opacity != nullptr && Opacity->Value.Output == 4);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
