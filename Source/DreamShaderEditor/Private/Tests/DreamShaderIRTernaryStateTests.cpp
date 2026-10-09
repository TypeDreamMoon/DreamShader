// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRCoreOps.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::TernaryStateTests
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Evaluate the emitted scalar graph at a supplied function input. The input remains dynamic
	// during compilation, so the builder cannot hide branch-state errors by picking a constant arm.
	static bool ReadScalar(const FIRGraph& Graph, FIRValue Value, double Input, double& Out, int32 Budget)
	{
		if (Budget <= 0 || !Graph.IsValidValue(Value) || Value.Output != 0)
		{
			return false;
		}
		const FIRNode& Node = Graph.Nodes[Value.Node];
		if (Node.Op == EIROp::FunctionInput)
		{
			const FIRProperty* Name = Node.FindProperty(Prop::InputName);
			if (!Name || Name->Value.S != TEXT("t"))
			{
				return false;
			}
			Out = Input;
			return true;
		}
		if (Node.Op == EIROp::Constant)
		{
			const FIRProperty* Constant = Node.FindProperty(Prop::Value);
			if (!Constant || Constant->Value.N != 1)
			{
				return false;
			}
			Out = Constant->Value.V[0];
			return true;
		}
		if ((Node.Op == EIROp::Select || Node.Op == EIROp::StaticSwitch) && Node.Operands.Num() == 3)
		{
			double Condition = 0;
			return ReadScalar(Graph, Node.Operands[0], Input, Condition, Budget - 1)
				&& ReadScalar(Graph, Node.Operands[Condition != 0 ? 1 : 2], Input, Out, Budget - 1);
		}
		if (Node.Op == EIROp::Compare && Node.Operands.Num() == 5)
		{
			double A = 0, B = 0;
			return ReadScalar(Graph, Node.Operands[0], Input, A, Budget - 1)
				&& ReadScalar(Graph, Node.Operands[1], Input, B, Budget - 1)
				&& ReadScalar(Graph, Node.Operands[A > B ? 2 : A == B ? 3 : 4], Input, Out, Budget - 1);
		}
		if (Node.Operands.Num() == 2)
		{
			double A = 0, B = 0;
			if (!ReadScalar(Graph, Node.Operands[0], Input, A, Budget - 1)
				|| !ReadScalar(Graph, Node.Operands[1], Input, B, Budget - 1))
			{
				return false;
			}
			switch (Node.Op)
			{
			case EIROp::Greater: Out = A > B ? 1 : 0; return true;
			case EIROp::Add: Out = A + B; return true;
			case EIROp::Subtract: Out = A - B; return true;
			default: break;
			}
		}
		return false;
	}

	static void ExpectResult(FAutomationTestBase& Test, const TCHAR* What, const FString& Source,
		const TArray<double>& Inputs, const TArray<double>& Expected)
	{
		FDreamShaderIRRun Run;
		RunDreamShaderIRPipeline(TEXT("TernaryState.dss"), Source, FDreamShaderIRRunOptions(), Run);
		if (!Test.TestTrue(FString::Printf(TEXT("%s builds: %s"), What, *Run.ErrorText()), Run.Succeeded())
			|| !Test.TestTrue(TEXT("one function product"), Run.Module && Run.Module->Products.Num() == 1))
		{
			return;
		}
		const FIRGraph& Graph = Run.Module->Products[0].Graph;
		if (!Test.TestEqual(TEXT("one function result"), Graph.FunctionOutputs.Num(), 1))
		{
			return;
		}
		const FIRNode& Output = Graph.Nodes[Graph.FunctionOutputs[0]];
		if (!Test.TestEqual(TEXT("one connected result"), Output.Operands.Num(), 1)
			|| !Test.TestEqual(TEXT("one expected result per input"), Inputs.Num(), Expected.Num()))
		{
			return;
		}
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			double Actual = 0;
			const FString Label = FString::Printf(TEXT("%s at t=%g"), What, Inputs[Index]);
			if (Test.TestTrue(Label + TEXT(" evaluates"), ReadScalar(Graph, Output.Operands[0], Inputs[Index], Actual, Graph.Nodes.Num())))
			{
				Test.TestEqual(Label, Actual, Expected[Index]);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderIRTernaryStateTest,
	"DreamShader.Lang2.IR.TernaryState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRTernaryStateTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::TernaryStateTests;
	ExpectResult(*this, TEXT("assignments merge according to the dynamic condition"), TEXT(
		"export float MF_Test(float t) { float x=0; float r=t>0 ? (x=1) : (x=2); return x; }\n"),
		{ -1, 0, 1 }, { 2, 2, 1 });
	ExpectResult(*this, TEXT("both arms start from the same local value"), TEXT(
		"export float MF_Test(float t) { float x=10; float r=t>0 ? ++x : ++x; return x; }\n"),
		{ -1, 1 }, { 11, 11 });
	ExpectResult(*this, TEXT("the ternary result sees only its own arm's write"), TEXT(
		"export float MF_Test(float t) { float x=10; return t>0 ? ++x : ++x; }\n"),
		{ -1, 1 }, { 11, 11 });
	ExpectResult(*this, TEXT("out helper writes are conditional"), TEXT(
		"float Set(out float x, float value) { x=value; return value; }\n"
		"export float MF_Test(float t) { float x=10; float r=t>0 ? Set(x,3) : Set(x,7); return x; }\n"),
		{ -1, 1 }, { 7, 3 });
	ExpectResult(*this, TEXT("inout helper calls share the incoming state"), TEXT(
		"float Add(inout float x, float value) { x+=value; return x; }\n"
		"export float MF_Test(float t) { float x=10; float r=t>0 ? Add(x,1) : Add(x,2); return x; }\n"),
		{ -1, 1 }, { 12, 11 });
	ExpectResult(*this, TEXT("nested ternaries compose their state merges"), TEXT(
		"export float MF_Test(float t) { float x=0; float r=t>0 ? (t>1 ? (x=3) : (x=2)) : (x=1); return x; }\n"),
		{ -1, 0.5, 2 }, { 1, 2, 3 });
	ExpectResult(*this, TEXT("constant true keeps only the chosen side effect"), TEXT(
		"export float MF_Test(float t) { float x=10; float r=1>0 ? ++x : ++x; return x; }\n"),
		{ -1, 1 }, { 11, 11 });
	ExpectResult(*this, TEXT("constant false keeps only the chosen side effect"), TEXT(
		"export float MF_Test(float t) { float x=10; float r=0>1 ? ++x : ++x; return x; }\n"),
		{ -1, 1 }, { 11, 11 });
	ExpectResult(*this, TEXT("the condition's own side effect executes once before both arms"), TEXT(
		"export float MF_Test(float t) { float count=0; float x=0; float r=(++count+t)>0 ? (x=1) : (x=2); return x+count; }\n"),
		{ -2, 0 }, { 3, 2 });
	ExpectResult(*this, TEXT("defaults first read in opposite arms remain initialized after the join"), TEXT(
		"float F(float t, float x=t>0?a:b, float a=3, float b=7) { return x+a+b; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 17, 13 });
	ExpectResult(*this, TEXT("dynamic forward defaults first read in opposite arms remain available"), TEXT(
		"float F(float t, float x=t>0?a:b, float a=t+3, float b=t+7) { return x+a+b; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 14, 16 });
	ExpectResult(*this, TEXT("a default first read by both arms has a value in both arms"), TEXT(
		"float F(float t, float x=t>0?a:(a+1), float a=t+3) { return x+a; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 5, 8 });
	ExpectResult(*this, TEXT("later defaults see only the selected arm's parameter assignment"), TEXT(
		"float F(float t, float x=t>0?(b=3):a, float a=b, inout float b=t+1) { return x+a+b; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 0, 9 });
	ExpectResult(*this, TEXT("a default's increment executes once on each path"), TEXT(
		"float F(float t, float x=t>0?a:b, float a=++b, inout float b=t+10) { return x+a+b; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 29, 36 });
	ExpectResult(*this, TEXT("a default needed after its dependency completes stays lazy"), TEXT(
		"float F(float t, float x=t>0?1:2, float a=x+1) { return x+a; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 5, 3 });
	ExpectResult(*this, TEXT("a deferred default retains constants from its original arm"), TEXT(
		"float F(float t, float x=(t>0?(a+(c=0)):(c=1))+a, float a=c?1:x, inout float c=1) { return x; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 2, 2 });
	ExpectResult(*this, TEXT("deferred defaults see writes performed after the branch join"), TEXT(
		"float F(float t, float x=(t>0?a:0)+(c=9), float a=++c, inout float c=0) { return x+a+c; }\n"
		"export float MF_Test(float t) { return F(t); }\n"),
		{ -1, 1 }, { 29, 20 });
	ExpectResult(*this, TEXT("a default's deferred out write initializes both paths"), TEXT(
		"float Init(out float c) { c=7; return 1; }\n"
		"float F(float t, out float c, float x=t>0?a:0, float a=Init(c)) { return x+a+c; }\n"
		"export float MF_Test(float t) { float c; return F(t,c); }\n"),
		{ -1, 1 }, { 8, 9 });
	ExpectResult(*this, TEXT("a later default can read a deferred out write on both paths"), TEXT(
		"float Init(out float c) { c=7; return 1; }\n"
		"float F(float t, out float c, float x=t>0?a:0, float a=Init(c), float y=c) { return x+a+y; }\n"
		"export float MF_Test(float t) { float c; return F(t,c); }\n"),
		{ -1, 1 }, { 8, 9 });
	ExpectResult(*this, TEXT("assignment-path facts preserve a later explicit out overwrite"), TEXT(
		"float Init(out float c) { c=7; return 1; }\n"
		"float F(float t, out float c, float x=(t>0?a:0)+(c=9), float a=Init(c), float y=c) { return x+a+y; }\n"
		"export float MF_Test(float t) { float c; return F(t,c); }\n"),
		{ -1, 1 }, { 17, 20 });
	{
		using namespace UE::DreamShader::Editor::Private::Tests;
		struct FErrorCase
		{
			const TCHAR* What;
			const TCHAR* Source;
			const TCHAR* Code;
		};
		const FErrorCase Cases[] =
		{
			{ TEXT("one arm cannot read what only the other arm assigned"),
				TEXT("export float MF_Test(float t) { float x; float r=t>0 ? (x=1) : x; return r; }\n"), TEXT("DSH4376:") },
			{ TEXT("a later read cannot rely on a write in only one arm"),
				TEXT("export float MF_Test(float t) { float x; float r=t>0 ? (x=1) : 0; return x; }\n"), TEXT("DSH4372:") },
			{ TEXT("a later default still rejects an out value missing on one path"),
				TEXT("float Init(out float c) { c=7; return 1; }\n"
					"float F(float t, out float c, float x=t>0?Init(c):0, float y=c) { return x+y; }\n"
					"export float MF_Test(float t) { float c; return F(t,c); }\n"), TEXT("DSH4372:") },
		};
		for (const FErrorCase& Case : Cases)
		{
			FDreamShaderIRRun Run;
			RunDreamShaderIRPipeline(TEXT("TernaryUnset.dss"), Case.Source, FDreamShaderIRRunOptions(), Run);
			TestFalse(Case.What, Run.Succeeded());
			TestTrue(FString::Printf(TEXT("%s reports %s (actual: %s)"), Case.What, Case.Code, *Run.ErrorText()),
				Run.Errors.ContainsByPredicate([&Case](const FString& Error)
				{
					return Error.StartsWith(Case.Code, ESearchCase::CaseSensitive);
				}));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
