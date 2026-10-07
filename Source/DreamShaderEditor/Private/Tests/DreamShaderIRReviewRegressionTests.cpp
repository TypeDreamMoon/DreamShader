// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRCoreOps.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::IRReviewRegressionTests
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Tests;

	// Inspect the value that reaches the function output, not an unconsumed constant that happens
	// to remain in the graph. These sources exercise parse, bind, lower, passes and validation.
	static bool ExpectConstantResult(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const FString& Source,
		const TArray<double>& Expected,
		double Tolerance = 1.0e-8)
	{
		FDreamShaderIRRun Run;
		RunDreamShaderIRPipeline(TEXT("ReviewRegression.dss"), Source, FDreamShaderIRRunOptions(), Run);
		if (!Test.TestTrue(FString::Printf(TEXT("%s builds: %s"), What, *Run.ErrorText()), Run.Succeeded()))
		{
			return false;
		}
		if (!Test.TestTrue(FString::Printf(TEXT("%s has one product"), What), Run.Module && Run.Module->Products.Num() == 1))
		{
			return false;
		}
		const FIRGraph& Graph = Run.Module->Products[0].Graph;
		if (!Test.TestTrue(FString::Printf(TEXT("%s has one function output"), What), Graph.FunctionOutputs.Num() == 1))
		{
			return false;
		}
		const FIRNode& Output = Graph.Nodes[Graph.FunctionOutputs[0]];
		if (!Test.TestTrue(FString::Printf(TEXT("%s has a connected result"), What), Output.Operands.Num() == 1 && Graph.IsValidValue(Output.Operands[0])))
		{
			return false;
		}
		const FIRNode& Value = Graph.Nodes[Output.Operands[0].Node];
		const FIRProperty* Constant = Value.FindProperty(Prop::Value);
		if (!Test.TestTrue(FString::Printf(TEXT("%s folds its result"), What), Value.Op == EIROp::Constant && Constant != nullptr))
		{
			return false;
		}
		bool bMatches = Test.TestEqual(FString::Printf(TEXT("%s result width"), What), Constant->Value.N, Expected.Num());
		for (int32 Index = 0; Index < Expected.Num() && Index < Constant->Value.N; ++Index)
		{
			bMatches &= Test.TestTrue(
				FString::Printf(TEXT("%s component %d: expected %.17g, got %.17g"), What, Index, Expected[Index], Constant->Value.V[Index]),
				FMath::Abs(Constant->Value.V[Index] - Expected[Index]) <= Tolerance);
		}
		return bMatches;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRChainedLValueRegressionTest,
	"DreamShader.Lang2.IR.ChainedLValueWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRChainedLValueRegressionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRReviewRegressionTests;
	struct FCase
	{
		const TCHAR* Write;
		double Expected[4];
	};
	const FCase Cases[] =
	{
		{ TEXT("v.yz.x = 9;"), { 1, 9, 3, 4 } },
		{ TEXT("v.wzy.xz = float2(9, 8);"), { 1, 8, 3, 9 } },
		{ TEXT("v.yz[0] = 9;"), { 1, 9, 3, 4 } },
		{ TEXT("v[2].x = 9;"), { 1, 2, 9, 4 } },
		{ TEXT("v.zw.yx[1] += 5;"), { 1, 2, 8, 4 } },
		{ TEXT("v.wzy.xz.y++;"), { 1, 3, 3, 4 } },
		{ TEXT("Set(v.yz.x);"), { 1, 9, 3, 4 } },
		{ TEXT("Bump(v.zw.yx[1]);"), { 1, 2, 13, 4 } },
	};
	for (const FCase& Case : Cases)
	{
		const FString Source = FString::Printf(TEXT(
			"void Set(out float x) { x = 9; }\n"
			"void Bump(inout float x) { x += 10; }\n"
			"export float4 MF_Write() { float4 v = float4(1, 2, 3, 4); %s return v; }\n"), Case.Write);
		ExpectConstantResult(*this, Case.Write, Source, { Case.Expected[0], Case.Expected[1], Case.Expected[2], Case.Expected[3] });
	}

	ExpectConstantResult(*this, TEXT("array element copied to a writable vector"), TEXT(
		"export float4 MF_Array() {\n"
		"    float4 v[2] = { float4(0, 0, 0, 0), float4(1, 2, 3, 4) };\n"
		"    float4 result = v[1];\n"
		"    result.zw.yx[1] = 9;\n"
		"    return result;\n"
		"}\n"), { 1, 2, 9, 4 });
	{
		// Arrays are compile-time lookup tables: BindIndex returns a literal, not an lvalue.
		// Composing vector masks must not turn a read from one into a writable array element.
		using namespace UE::DreamShader::Editor::Private::Tests;
		FDreamShaderIRRun ReadOnlyArray;
		RunDreamShaderIRPipeline(TEXT("ReadOnlyArray.dss"), TEXT(
			"export float4 MF_Array() {\n"
			"    float4 v[2] = { float4(0, 0, 0, 0), float4(1, 2, 3, 4) };\n"
			"    v[1].zw.yx[1] = 9;\n"
			"    return v[1];\n"
			"}\n"), FDreamShaderIRRunOptions(), ReadOnlyArray);
		TestTrue(TEXT("array-write source parses"), ReadOnlyArray.bParsed);
		TestFalse(TEXT("constant array elements remain read-only"), ReadOnlyArray.bBound);
		TestTrue(TEXT("array-write refusal is DSH4229"), ReadOnlyArray.Errors.ContainsByPredicate([](const FString& Error)
		{
			return Error.StartsWith(TEXT("DSH4229:"), ESearchCase::CaseSensitive);
		}));
	}
	ExpectConstantResult(*this, TEXT("struct field and chained mask"), TEXT(
		"struct Data { float4 color; };\n"
		"export float4 MF_Struct() {\n"
		"    Data v; v.color = float4(1, 2, 3, 4);\n"
		"    v.color.yz.x = 9;\n"
		"    return v.color;\n"
		"}\n"), { 1, 9, 3, 4 });
	ExpectConstantResult(*this, TEXT("material attribute and chained mask"), TEXT(
		"export float3 MF_Attribute() {\n"
		"    material m; m.EmissiveColor = float3(1, 2, 3);\n"
		"    m.EmissiveColor.yz.x = 9;\n"
		"    return m.EmissiveColor;\n"
		"}\n"), { 1, 9, 3 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRCastConstantRegressionTest,
	"DreamShader.Lang2.IR.CastConstantParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRCastConstantRegressionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRReviewRegressionTests;
	ExpectConstantResult(*this, TEXT("direct and local integer-cast conditions"), TEXT(
		"export float2 MF_Condition() {\n"
		"    float direct = ((int)0.5) ? 1.0 : 0.0;\n"
		"    int value = (int)0.5;\n"
		"    float viaLocal = value ? 1.0 : 0.0;\n"
		"    return float2(direct, viaLocal);\n"
		"}\n"), { 1, 1 });
	ExpectConstantResult(*this, TEXT("integer and bool casts retain graph numbers"), TEXT(
		"export float4 MF_Kinds() {\n"
		"    int i = (int)1.75;\n"
		"    bool b = (bool)0.5;\n"
		"    return float4(((int)1.75 == 1.75) ? 1 : 0, i, ((bool)0.5 == 0.5) ? 1 : 0, b);\n"
		"}\n"), { 1, 1.75, 1, 0.5 });
	ExpectConstantResult(*this, TEXT("vector cast components and scalar broadcast"), TEXT(
		"export float4 MF_Vector() {\n"
		"    int3 v = (int3)float3(0.5, 1.75, -2.5);\n"
		"    float same = (((int3)float3(0.5, 1.75, -2.5)).y == 1.75) ? 1 : 0;\n"
		"    return float4(v.x, v.y, v.z, same);\n"
		"}\n"), { 0.5, 1.75, -2.5, 1 });
	ExpectConstantResult(*this, TEXT("broadcast cast constant condition"), TEXT(
		"export float4 MF_Broadcast() {\n"
		"    int3 v = (int3)0.5;\n"
		"    return float4(v, (((int3)0.5).z == 0.5) ? 1 : 0);\n"
		"}\n"), { 0.5, 0.5, 0.5, 1 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRPeriodicMathRegressionTest,
	"DreamShader.Lang2.IR.PeriodicMathConstantParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRPeriodicMathRegressionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRReviewRegressionTests;
	// Calls over a local fold in the IR pass; literal conditions are decided by the binder.
	// The oracle is the graph node's documented unit period, not the folding helper itself.
	ExpectConstantResult(*this, TEXT("quarter cycle in both constant evaluators"), TEXT(
		"export float4 MF_Quarter() {\n"
		"    float x = 0.25;\n"
		"    return float4(sin(x), cos(x), (sin(0.25) > 0.9) ? 1 : 0, (cos(0.25) < 0.1) ? 1 : 0);\n"
		"}\n"), { 1, 0, 1, 1 }, 1.0e-6);
	ExpectConstantResult(*this, TEXT("negative and full cycles"), TEXT(
		"export float4 MF_Period() {\n"
		"    return float4(sin(-0.25), cos(-0.5), sin(1.0), cos(1.0));\n"
		"}\n"), { -1, -1, 0, 1 }, 1.0e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRRoundConstantRegressionTest,
	"DreamShader.Lang2.IR.RoundConstantParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRRoundConstantRegressionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRReviewRegressionTests;
	struct FCase
	{
		const TCHAR* Input;
		const TCHAR* Expected;
		double Value;
	};
	const FCase Cases[] =
	{
		{ TEXT("0.5"), TEXT("0"), 0 },
		{ TEXT("1.5"), TEXT("2"), 2 },
		{ TEXT("2.5"), TEXT("2"), 2 },
		{ TEXT("-0.5"), TEXT("0"), 0 },
		{ TEXT("-1.5"), TEXT("-2"), -2 },
		{ TEXT("-2.5"), TEXT("-2"), -2 },
		{ TEXT("0.5000000596046448"), TEXT("1"), 1 },
		{ TEXT("0.4999999701976776"), TEXT("0"), 0 },
		{ TEXT("1.0e30"), TEXT("1.0e30"), 1.0e30 },
		{ TEXT("-1.0e30"), TEXT("-1.0e30"), -1.0e30 },
	};
	for (const FCase& Case : Cases)
	{
		const FString Source = FString::Printf(TEXT(
			"export float2 MF_Round() { float x = %s; return float2(round(x), (round(%s) == %s) ? 1 : 0); }\n"),
			Case.Input, Case.Input, Case.Expected);
		ExpectConstantResult(*this, Case.Input, Source, { Case.Value, 1 });
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
