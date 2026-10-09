// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRCoreOps.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::DefaultArgumentTests
{
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Tests;

	static const FIRGraph* Build(FAutomationTestBase& Test, const FString& Source, FDreamShaderIRRun& Run)
	{
		RunDreamShaderIRPipeline(TEXT("DefaultArguments.dss"), Source, FDreamShaderIRRunOptions(), Run);
		if (!Test.TestTrue(FString::Printf(TEXT("default arguments build: %s"), *Run.ErrorText()), Run.Succeeded()))
		{
			return nullptr;
		}
		for (const FIRProduct& Product : Run.Module->Products)
		{
			if (Product.Name == TEXT("MF_Test"))
			{
				return &Product.Graph;
			}
		}
		Test.AddError(TEXT("MF_Test was not produced"));
		return nullptr;
	}

	static void ExpectConstant(FAutomationTestBase& Test, const TCHAR* Name, const FIRGraph& Graph, FIRValue Value, const TArray<double>& Expected)
	{
		if (!Test.TestTrue(Name, Graph.IsValidValue(Value)))
		{
			return;
		}
		const FIRNode& Node = Graph.Nodes[Value.Node];
		const FIRProperty* Property = Node.FindProperty(Prop::Value);
		if (!Test.TestTrue(FString::Printf(TEXT("%s is a constant"), Name), Node.Op == EIROp::Constant && Value.Output == 0 && Property)
			|| !Test.TestEqual(FString::Printf(TEXT("%s width"), Name), Property->Value.N, Expected.Num()))
		{
			return;
		}
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			Test.TestEqual(FString::Printf(TEXT("%s component %d"), Name, Index), Property->Value.V[Index], Expected[Index]);
		}
	}

	static void ExpectResult(FAutomationTestBase& Test, const TCHAR* Name, const FString& Source, const TArray<double>& Expected)
	{
		FDreamShaderIRRun Run;
		const FIRGraph* Graph = Build(Test, Source, Run);
		if (!Graph || !Test.TestEqual(TEXT("one result"), Graph->FunctionOutputs.Num(), 1))
		{
			return;
		}
		const FIRNode& Output = Graph->Nodes[Graph->FunctionOutputs[0]];
		if (Test.TestEqual(TEXT("one result operand"), Output.Operands.Num(), 1))
		{
			ExpectConstant(Test, Name, *Graph, Output.Operands[0], Expected);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderIRDefaultArgumentScopeTest,
	"DreamShader.Lang2.IR.DefaultArguments.Scope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRDefaultArgumentScopeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::DefaultArgumentTests;
	ExpectResult(*this, TEXT("omitted and explicit arguments agree"), TEXT(
		"float F(float x, float y = x) { return y; }\n"
		"float G(float callerValue) { return F(7); }\n"
		"export float2 MF_Test() { return float2(G(99), F(7, 7)); }\n"), { 7, 7 });
	ExpectResult(*this, TEXT("explicit expressions and inout targets stay in caller scope"), TEXT(
		"float F(float x, inout float changed, float y = x) { changed += y; return y; }\n"
		"float2 G(inout float v) { float r = F(++v, v); return float2(r, v); }\n"
		"export float2 MF_Test() { float v = 99; return G(v); }\n"), { 100, 200 });
	ExpectResult(*this, TEXT("defaults can depend on earlier defaults"), TEXT(
		"float F(float a, float b = a + 1, float c = b + 1) { return c; }\n"
		"float G(float a, float b, float c) { return F(7); }\n"
		"export float MF_Test() { return G(99, 88, 77); }\n"), { 9 });
	ExpectResult(*this, TEXT("a named later parameter is available to a default"), TEXT(
		"float F(float a = b + 1, float b = 2) { return a; }\n"
		"float G(float a, float b) { return F(b = 7); }\n"
		"export float MF_Test() { return G(99, 88); }\n"), { 8 });
	ExpectResult(*this, TEXT("a later omitted dependency resolves on demand"), TEXT(
		"float F(float a = b + 1, float b = 2) { return a; }\n"
		"float G(float a, float b) { return F(); }\n"
		"export float MF_Test() { return G(99, 88); }\n"), { 3 });
	ExpectResult(*this, TEXT("explicit scalar arguments have their declared width"), TEXT(
		"float F(float3 value, float last = value.z) { return last; }\n"
		"float G(float callerValue) { return F(7); }\n"
		"export float MF_Test() { return G(99); }\n"), { 7 });
	ExpectResult(*this, TEXT("default scalar broadcasts before a dependent read"), TEXT(
		"float F(float a = 7, float3 b = a, float c = b.z) { return c; }\n"
		"float G(float a, float3 b) { return F(); }\n"
		"export float MF_Test() { return G(99, float3(88, 77, 66)); }\n"), { 7 });
	ExpectResult(*this, TEXT("literal and global defaults retain their meaning"), TEXT(
		"static const float Base = 3;\n"
		"float F(float a = Base + 4) { return a; }\n"
		"export float MF_Test() { return F(); }\n"), { 7 });
	ExpectResult(*this, TEXT("a default can call a helper with a callee parameter"), TEXT(
		"float Twice(float v) { return v * 2; }\n"
		"float F(float a, float b = Twice(a)) { return b; }\n"
		"float G(float callerValue) { return F(7); }\n"
		"export float MF_Test() { return G(99); }\n"), { 14 });
	ExpectResult(*this, TEXT("a default's out assignment reaches the body and caller"), TEXT(
		"float Init(out float x) { x = 7; return 1; }\n"
		"float F(out float x, float y = Init(x)) { return y; }\n"
		"export float2 MF_Test() { float v; float r = F(v); return float2(r, v); }\n"), { 1, 7 });
	ExpectResult(*this, TEXT("a compound default read initializes its pending parameter"), TEXT(
		"float F(float y = ++x, inout float x = 7) { return y; }\n"
		"export float MF_Test() { return F(); }\n"), { 8 });
	ExpectResult(*this, TEXT("a default write replaces a pending parameter initializer"), TEXT(
		"float F(float y = (x = 9), inout float x = 7) { return x; }\n"
		"export float MF_Test() { return F(); }\n"), { 9 });
	ExpectResult(*this, TEXT("an explicit argument breaks a default dependency cycle"), TEXT(
		"float F(float a = b, float b = a) { return a; }\n"
		"export float MF_Test() { return F(b = 7); }\n"), { 7 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderIRCustomDefaultArgumentTest,
	"DreamShader.Lang2.IR.DefaultArguments.CustomInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRCustomDefaultArgumentTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::DefaultArgumentTests;
	for (const bool bVector : { false, true })
	{
		FDreamShaderIRRun Run;
		const FString Source = FString(TEXT("/// @custom\n")) + (bVector
			? TEXT("float F(float3 x, float y = x.z) { return y; }\n")
			: TEXT("float F(float x, float y = x) { return y; }\n"))
			+ TEXT("float G(float callerValue) { return F(7); }\nexport float MF_Test() { return G(99); }\n");
		const FIRGraph* Graph = Build(*this, Source, Run);
		if (!Graph)
		{
			continue;
		}
		const FIRNode* Custom = Graph->Nodes.FindByPredicate([](const FIRNode& Node) { return Node.Op == EIROp::Custom; });
		if (!TestNotNull(TEXT("the call makes a Custom node"), Custom))
		{
			continue;
		}
		const FIRInput* X = Custom->FindInput(TEXT("x"));
		const FIRInput* Y = Custom->FindInput(TEXT("y"));
		if (TestNotNull(TEXT("explicit input is connected"), X) && TestNotNull(TEXT("default input is connected"), Y))
		{
			ExpectConstant(*this, TEXT("Custom x"), *Graph, X->Value, bVector ? TArray<double>{ 7, 7, 7 } : TArray<double>{ 7 });
			ExpectConstant(*this, TEXT("Custom y"), *Graph, Y->Value, { 7 });
		}
	}
	FDreamShaderIRRun FiniteRun;
	const FIRGraph* FiniteGraph = Build(*this, TEXT(
		"/// @custom\nfloat F(float x = F(1)) { return x; }\n"
		"export float MF_Test() { return F(); }\n"), FiniteRun);
	if (FiniteGraph)
	{
		int32 CustomCount = 0;
		for (const FIRNode& Node : FiniteGraph->Nodes)
		{
			CustomCount += Node.Op == EIROp::Custom ? 1 : 0;
		}
		TestEqual(TEXT("an explicit nested Custom call ends default expansion"), CustomCount, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderIRAssetDefaultArgumentTest,
	"DreamShader.Lang2.IR.DefaultArguments.AssetPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRAssetDefaultArgumentTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::DefaultArgumentTests;
	for (const bool bExtern : { false, true })
	{
		const FString Declaration = bExtern
			? TEXT("/// @asset /Game/MF_External\nextern float MF_Asset(inout float value, float omitted = ++value);\n")
			: TEXT("export float MF_Asset(inout float value, float omitted = ++value) { return omitted; }\n");
		ExpectResult(*this, TEXT("an asset default cannot change the caller"), Declaration + TEXT(
			"float G(float callerValue) { float v = 7; MF_Asset(v); return callerValue; }\n"
			"export float MF_Test() { return G(99); }\n"), { 99 });
		FDreamShaderIRRun Run;
		const FIRGraph* Graph = Build(*this, Declaration + TEXT("export float MF_Test() { float v = 7; return MF_Asset(v); }\n"), Run);
		if (!Graph)
		{
			continue;
		}
		const FIRNode* Call = Graph->Nodes.FindByPredicate([](const FIRNode& Node) { return Node.Op == EIROp::FunctionCall; });
		if (TestNotNull(TEXT("the asset call remains a FunctionCall"), Call))
		{
			TestEqual(TEXT("only the explicit input is connected"), Call->Inputs.Num(), 1);
			TestNotNull(TEXT("the explicit value pin is connected"), Call->FindInput(TEXT("value")));
			TestNull(TEXT("the omitted pin keeps the asset's own default"), Call->FindInput(TEXT("omitted")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamShaderIRDefaultArgumentCycleTest,
	"DreamShader.Lang2.IR.DefaultArguments.Cycles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRDefaultArgumentCycleTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::Tests;
	for (const TCHAR* Signature : { TEXT("float a = a"), TEXT("float a = b, float b = a") })
	{
		FDreamShaderIRRun Run;
		const FString Source = FString::Printf(TEXT(
			"float F(%s) { return a; }\nfloat G(float a, float b) { return F(); }\nexport float MF_Test() { return G(99, 88); }\n"), Signature);
		RunDreamShaderIRPipeline(TEXT("DefaultCycle.dss"), Source, FDreamShaderIRRunOptions(), Run);
		TestFalse(TEXT("a cyclic default fails instead of reading caller slots"), Run.Succeeded());
		TestTrue(TEXT("a cyclic default is DSH6224"), Run.Errors.ContainsByPredicate([](const FString& Error)
		{
			return Error.StartsWith(TEXT("DSH6224:"), ESearchCase::CaseSensitive);
		}));
	}
	FDreamShaderIRRun RecursiveHelperRun;
	RunDreamShaderIRPipeline(TEXT("RecursiveHelperDefault.dss"), TEXT(
		"float F(float x = F(1)) { return x; }\nexport float MF_Test() { return F(); }\n"),
		FDreamShaderIRRunOptions(), RecursiveHelperRun);
	TestFalse(TEXT("helper recursion retains its existing rejection"), RecursiveHelperRun.Succeeded());
	TestTrue(TEXT("the binder's recursive helper still reports DSH6220"), RecursiveHelperRun.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.StartsWith(TEXT("DSH6220:"), ESearchCase::CaseSensitive);
	}));
	for (const TCHAR* Source : {
		TEXT("/// @custom\nfloat F(float a = F()) { return a; }\nexport float MF_Test() { return F(); }\n"),
		TEXT("/// @custom\nfloat F(float a = G()) { return a; }\n/// @custom\nfloat G(float a = F()) { return a; }\nexport float MF_Test() { return F(); }\n"),
		TEXT("float Init(out float x) { x = 7; return 1; }\n/// @custom\nfloat F(out float x, float y = Init(x)) { x = y; return y; }\nexport float MF_Test() { float v; return F(v); }\n") })
	{
		FDreamShaderIRRun Run;
		RunDreamShaderIRPipeline(TEXT("DefaultBoundary.dss"), Source, FDreamShaderIRRunOptions(), Run);
		TestFalse(TEXT("unsupported default boundaries fail safely"), Run.Succeeded());
		TestTrue(TEXT("unsupported default boundaries are DSH6224"), Run.Errors.ContainsByPredicate([](const FString& Error)
		{
			return Error.StartsWith(TEXT("DSH6224:"), ESearchCase::CaseSensitive);
		}));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
