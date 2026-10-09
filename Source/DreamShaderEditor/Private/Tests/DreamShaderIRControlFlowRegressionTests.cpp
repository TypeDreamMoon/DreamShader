// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRCoreOps.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::ControlFlowRegressionTests
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Evaluate the small, already optimized output graph. Branches deliberately remain as
	// Select/Compare nodes after constant folding; checking only for a Constant would miss
	// the selected value. This oracle knows graph values, never source control-flow state.
	static bool ReadResult(const FIRGraph& Graph, FIRValue Value, TArray<double>& Out, int32 Budget)
	{
		if (Budget <= 0 || !Graph.IsValidValue(Value) || Value.Output != 0)
		{
			return false;
		}
		const FIRNode& Node = Graph.Nodes[Value.Node];
		Out.Reset();
		if (Node.Op == EIROp::Constant)
		{
			const FIRProperty* Property = Node.FindProperty(Prop::Value);
			if (!Property || Property->Value.N < 1 || Property->Value.N > 4)
			{
				return false;
			}
			Out.Append(Property->Value.V, Property->Value.N);
			return true;
		}
		if ((Node.Op == EIROp::Select || Node.Op == EIROp::StaticSwitch) && Node.Operands.Num() == 3)
		{
			TArray<double> Condition;
			return ReadResult(Graph, Node.Operands[0], Condition, Budget - 1) && Condition.Num() == 1
				&& ReadResult(Graph, Node.Operands[Condition[0] != 0 ? 1 : 2], Out, Budget - 1);
		}
		if (Node.Op == EIROp::Compare && Node.Operands.Num() == 5)
		{
			TArray<double> A, B;
			if (!ReadResult(Graph, Node.Operands[0], A, Budget - 1) || A.Num() != 1
				|| !ReadResult(Graph, Node.Operands[1], B, Budget - 1) || B.Num() != 1)
			{
				return false;
			}
			return ReadResult(Graph, Node.Operands[A[0] > B[0] ? 2 : A[0] == B[0] ? 3 : 4], Out, Budget - 1);
		}
		if (Node.Op == EIROp::Append)
		{
			for (const FIRValue& Operand : Node.Operands)
			{
				TArray<double> Part;
				if (!ReadResult(Graph, Operand, Part, Budget - 1))
				{
					return false;
				}
				Out.Append(Part);
			}
			return !Out.IsEmpty();
		}
		return false;
	}

	static void ExpectResult(FAutomationTestBase& Test, const TCHAR* Name, const FString& Source, const TArray<double>& Expected)
	{
		FDreamShaderIRRun Run;
		RunDreamShaderIRPipeline(TEXT("ControlFlowRegression.dss"), Source, FDreamShaderIRRunOptions(), Run);
		if (!Test.TestTrue(FString::Printf(TEXT("%s builds: %s"), Name, *Run.ErrorText()), Run.Succeeded())
			|| !Test.TestTrue(TEXT("one exported function"), Run.Module && Run.Module->Products.Num() == 1))
		{
			return;
		}
		const FIRGraph& Graph = Run.Module->Products[0].Graph;
		if (!Test.TestEqual(TEXT("one function result"), Graph.FunctionOutputs.Num(), 1))
		{
			return;
		}
		const FIRNode& Output = Graph.Nodes[Graph.FunctionOutputs[0]];
		TArray<double> Actual;
		if (!Test.TestTrue(FString::Printf(TEXT("%s result can be evaluated"), Name), Output.Operands.Num() == 1
			&& ReadResult(Graph, Output.Operands[0], Actual, Graph.Nodes.Num()))
			|| !Test.TestEqual(TEXT("result width"), Actual.Num(), Expected.Num()))
		{
			return;
		}
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			Test.TestEqual(FString::Printf(TEXT("%s component %d"), Name, Index), Actual[Index], Expected[Index]);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderIRNestedReturnRegressionTest,
	"DreamShader.Lang2.IR.NestedReturnState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRNestedReturnRegressionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::ControlFlowRegressionTests;
	ExpectResult(*this, TEXT("nested then leaves the outer fallthrough live"), TEXT(
		"float F(float x) { if (x > 0) { if (x > 1) return 2; else return 1; } return 0; }\n"
		"export float3 MF_Test() { return float3(F(-1), F(0.5), F(2)); }\n"), { 0, 1, 2 });
	ExpectResult(*this, TEXT("nested else leaves the outer fallthrough live"), TEXT(
		"float F(float x) { if (x >= 0) {} else { if (x < -1) return -2; else return -1; } return 0; }\n"
		"export float3 MF_Test() { return float3(F(-2), F(-0.5), F(1)); }\n"), { -2, -1, 0 });
	ExpectResult(*this, TEXT("a complete nested return tree has no phantom exits"), TEXT(
		"float F(float x) { if (x >= 0) { if (x > 1) return 2; else return 1; }\n"
		" else { if (x < -1) return -2; else return -1; } }\n"
		"export float4 MF_Test() { return float4(F(-2), F(-0.5), F(0.5), F(2)); }\n"), { -2, -1, 1, 2 });
	ExpectResult(*this, TEXT("three nested levels preserve both live tails"), TEXT(
		"float F(float x) { if (x > 0) { if (x > 1) { if (x > 2) return 3; else return 2; } return 1; } return 0; }\n"
		"export float4 MF_Test() { return float4(F(-1), F(0.5), F(1.5), F(2.5)); }\n"), { 0, 1, 2, 3 });
	ExpectResult(*this, TEXT("inout copyback follows the selected return state"), TEXT(
		"void Set(float x, inout float y) { if (x > 0) { if (x > 1) { y = 2; return; } else { y = 1; return; } } y = 0; }\n"
		"float F(float x) { float y = 99; Set(x, y); return y; }\n"
		"export float3 MF_Test() { return float3(F(-1), F(0.5), F(2)); }\n"), { 0, 1, 2 });
	ExpectResult(*this, TEXT("material attribute copyback follows the selected return state"), TEXT(
		"void Set(float x, inout material m) { if (x > 0) { if (x > 1) { m.Roughness = 2; return; } else { m.Roughness = 1; return; } } m.Roughness = 0; }\n"
		"float F(float x) { material m; m.Roughness = 99; Set(x, m); return m.Roughness; }\n"
		"export float3 MF_Test() { return float3(F(-1), F(0.5), F(2)); }\n"), { 0, 1, 2 });
	ExpectResult(*this, TEXT("loop iterations preserve the reachable final return"), TEXT(
		"float F(float x) { for (int i = 0; i < 2; ++i) { if (x > 0) { if (x > 1) return 2; else return 1; } } return 0; }\n"
		"export float3 MF_Test() { return float3(F(-1), F(0.5), F(2)); }\n"), { 0, 1, 2 });
	ExpectResult(*this, TEXT("a callee cannot terminate the caller branch"), TEXT(
		"float F(float x) { if (x > 0) { if (x > 1) return 2; else return 1; } return 0; }\n"
		"float G(float x) { float y = 10; if (x < 1) y = F(x); else y = 5; return y; }\n"
		"export float3 MF_Test() { return float3(G(-1), G(0.5), G(2)); }\n"), { 0, 1, 5 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderIRBranchJumpBoundaryTest,
	"DreamShader.Lang2.IR.BranchJumpBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRBranchJumpBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;
	// A graph loop with break/continue is intentionally outside the binder's trip-count proof.
	// Fixing the nested return join must not accidentally let either jump escape its branch.
	for (const TCHAR* Jump : { TEXT("break"), TEXT("continue") })
	{
		FDreamShaderIRRun Run;
		const FString Source = FString::Printf(TEXT(
			"export float MF_Test() { float y = 0; for (int i = 0; i < 2; ++i) { if (i > 0) %s; y = 1; } return y; }"), Jump);
		RunDreamShaderIRPipeline(TEXT("BranchJump.dss"), Source, FDreamShaderIRRunOptions(), Run);
		TestFalse(FString::Printf(TEXT("%s loop remains unsupported"), Jump), Run.Succeeded());
		TestTrue(TEXT("the unsupported trip count is diagnosed"), Run.Errors.ContainsByPredicate([](const FString& Error)
		{
			return Error.StartsWith(TEXT("DSH4360:"), ESearchCase::CaseSensitive);
		}));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
