// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// A cook and `compile -All` build every source in one batch, and whether that succeeds must not depend
// on the order the files were listed in (#72). Two layers:
//
//   * the scheduler alone, against a fake compiler whose sources depend on each other by name -- the
//     only way to pin the pass logic (chains, cycles, a real error beside a recoverable one) exactly;
//   * the real compiler on a clean workspace: a material that calls a function another source
//     generates, listed first, which used to fail with DSH8219 and abort the cook.

#include "Tests/DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderCompilerInterface.h"
#include "SourceFiles/DreamShaderBatchCompile.h"

#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::Tests
{
	// Fixture helpers shared with DreamShaderAutomationTests.cpp (defined there, external linkage).
	FString MakeUniqueTestAssetName(const TCHAR* Prefix);
	void AddExpectedNewAssetProbeWarnings(FAutomationTestBase& Test, const FString& ObjectPath);
	void AddExpectedAutomationCleanupWarnings(FAutomationTestBase& Test);
}

namespace UE::DreamShader::Editor::Private::BatchCompileTests
{
	/** Each source succeeds once every source it names has succeeded, unless it is marked broken. */
	class FFakeCompiler final : public IDreamShaderCompiler
	{
	public:
		TMap<FString, TArray<FString>> Dependencies;
		TSet<FString> Broken;
		TSet<FString> Built;
		TArray<FString> Calls;

		virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) override
		{
			Calls.Add(Request.SourceFilePath);

			FDreamShaderCompileResult Result;
			bool bReady = !Broken.Contains(Request.SourceFilePath);
			for (const FString& Dependency : Dependencies.FindRef(Request.SourceFilePath))
			{
				bReady &= Built.Contains(Dependency);
			}
			Result.bSucceeded = bReady;
			Result.Code = bReady ? FString() : TEXT("DSH8219");
			if (bReady)
			{
				Built.Add(Request.SourceFilePath);
			}
			return Result;
		}

		virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) override
		{
			return CompileAssets(Request);
		}
	};

	struct FRun
	{
		int32 Failures = 0;
		TArray<FString> FinalOrder;
		TMap<FString, FDreamShaderBatchCompileEntry> Entries;
	};

	FRun Run(FFakeCompiler& Compiler, const TArray<FString>& Files)
	{
		FRun Out;
		Out.Failures = CompileDreamShaderSourcesInBatch(Compiler, Files, FDreamShaderCompileRequest(),
			[&Out](const FDreamShaderBatchCompileEntry& Entry)
			{
				Out.FinalOrder.Add(Entry.SourceFile);
				Out.Entries.Add(Entry.SourceFile, Entry);
			});
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderBatchCompileSchedulingTest,
	"DreamShader.Compiler.Batch.Scheduling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderBatchCompileSchedulingTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::BatchCompileTests;

	// The callee first: one pass, nothing retried.
	{
		FFakeCompiler Compiler;
		Compiler.Dependencies.Add(TEXT("A.dsm"), TArray<FString>{ TEXT("Z.dsf") });
		const FRun Result = Run(Compiler, { TEXT("Z.dsf"), TEXT("A.dsm") });
		TestEqual(TEXT("callee first: no failure"), Result.Failures, 0);
		TestEqual(TEXT("callee first: two compiles"), Compiler.Calls.Num(), 2);
	}

	// The caller first -- the issue's case: the caller waits one pass and then builds.
	{
		FFakeCompiler Compiler;
		Compiler.Dependencies.Add(TEXT("A.dsm"), TArray<FString>{ TEXT("Z.dsf") });
		const FRun Result = Run(Compiler, { TEXT("A.dsm"), TEXT("Z.dsf") });
		TestEqual(TEXT("caller first: no failure"), Result.Failures, 0);
		TestEqual(TEXT("caller first: the caller took two attempts"), Result.Entries.FindRef(TEXT("A.dsm")).Attempts, 2);
		TestEqual(TEXT("caller first: the callee took one"), Result.Entries.FindRef(TEXT("Z.dsf")).Attempts, 1);
		TestEqual(TEXT("caller first: each file reported once"), Result.FinalOrder.Num(), 2);
	}

	// A chain listed back to front: a function calling a function, called by a material.
	{
		FFakeCompiler Compiler;
		Compiler.Dependencies.Add(TEXT("M.dss"), TArray<FString>{ TEXT("F2.dss") });
		Compiler.Dependencies.Add(TEXT("F2.dss"), TArray<FString>{ TEXT("F1.dss") });
		const FRun Result = Run(Compiler, { TEXT("M.dss"), TEXT("F2.dss"), TEXT("F1.dss") });
		TestEqual(TEXT("chain: no failure"), Result.Failures, 0);
		TestEqual(TEXT("chain: the material waited for both"), Result.Entries.FindRef(TEXT("M.dss")).Attempts, 3);
	}

	// A real error stays an error -- and does not stop a recoverable source beside it from building.
	{
		FFakeCompiler Compiler;
		Compiler.Dependencies.Add(TEXT("A.dsm"), TArray<FString>{ TEXT("Z.dsf") });
		Compiler.Broken.Add(TEXT("Bad.dsm"));
		const FRun Result = Run(Compiler, { TEXT("A.dsm"), TEXT("Bad.dsm"), TEXT("Z.dsf") });
		TestEqual(TEXT("broken: exactly one failure"), Result.Failures, 1);
		TestTrue(TEXT("broken: the recoverable source built"), Result.Entries.FindRef(TEXT("A.dsm")).Result.bSucceeded);
		TestFalse(TEXT("broken: the broken source failed"), Result.Entries.FindRef(TEXT("Bad.dsm")).Result.bSucceeded);
		TestEqual(TEXT("broken: its failure is reported last, once the batch settled"), Result.FinalOrder.Last(), FString(TEXT("Bad.dsm")));
		TestEqual(TEXT("broken: each file reported once"), Result.FinalOrder.Num(), 3);
	}

	// A broken source alone is compiled once: a pass that builds nothing ends the batch.
	{
		FFakeCompiler Compiler;
		Compiler.Broken.Add(TEXT("Bad.dsm"));
		const FRun Result = Run(Compiler, { TEXT("Bad.dsm") });
		TestEqual(TEXT("alone: one failure"), Result.Failures, 1);
		TestEqual(TEXT("alone: one compile"), Compiler.Calls.Num(), 1);
	}

	// A cycle never resolves, and the batch still ends.
	{
		FFakeCompiler Compiler;
		Compiler.Dependencies.Add(TEXT("A.dss"), TArray<FString>{ TEXT("B.dss") });
		Compiler.Dependencies.Add(TEXT("B.dss"), TArray<FString>{ TEXT("A.dss") });
		const FRun Result = Run(Compiler, { TEXT("A.dss"), TEXT("B.dss"), TEXT("C.dss") });
		TestEqual(TEXT("cycle: both members fail"), Result.Failures, 2);
		TestTrue(TEXT("cycle: the independent source built"), Result.Entries.FindRef(TEXT("C.dss")).Result.bSucceeded);
	}

	// The shared order: functions first, then by path, ignoring case.
	{
		TArray<FString> Files = { TEXT("b/M.dsm"), TEXT("Z/F.dsf"), TEXT("a/M.dss"), TEXT("A/F.dsf") };
		SortDreamShaderSourcesForBatch(Files);
		TestEqual(TEXT("sort"), FString::Join(Files, TEXT(" ")), FString(TEXT("A/F.dsf Z/F.dsf a/M.dss b/M.dsm")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderBatchCompileCleanWorkspaceTest,
	"DreamShader.Compiler.Batch.CallerBeforeFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The issue's reproduction with the real compiler: a material whose VirtualFunction names the asset another
// source generates, compiled first on a workspace where that asset does not exist yet.
bool FDreamShaderBatchCompileCleanWorkspaceTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader;
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;

	IDreamShaderCompiler* const Compiler = GetDreamShaderCompiler();
	if (!TestNotNull(TEXT("the compiler is available"), Compiler))
	{
		return false;
	}

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("CallerBeforeFunction"), TEXT("Batch"), TEXT("dsm"));
	// The first attempt at the material fails on purpose, naming the function's path.
	AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("DSH8219"), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedAutomationCleanupWarnings(*this);

	const FString FunctionName = MakeUniqueTestAssetName(TEXT("MF_BatchHelper"));
	const FString MaterialName = MakeUniqueTestAssetName(TEXT("M_BatchCaller"));
	const FString FunctionObjectPath = Fixture.MakeObjectPath(FunctionName);
	const FString MaterialObjectPath = Fixture.MakeObjectPath(MaterialName);
	Fixture.TrackObjectPath(FunctionObjectPath);
	Fixture.TrackObjectPath(MaterialObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, FunctionObjectPath);
	AddExpectedNewAssetProbeWarnings(*this, MaterialObjectPath);

	const FString MaterialSource = FString::Printf(TEXT(R"(
VirtualFunction(Name="MF_BatchHelper")
{
    Options = { Asset = Path(Game, "%s"); }
    Inputs = { float3 Colour; }
    Outputs = { float3 Result; }
}

Shader(Name="%s")
{
    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = MF_BatchHelper(vec3(1.0, 0.5, 0.25));
    }
}
)"), *Fixture.MakeLegacyAssetName(FunctionName), *Fixture.MakeLegacyAssetName(MaterialName));

	const FString FunctionSource = FString::Printf(TEXT(R"(
ShaderFunction(Name="%s")
{
    Inputs = { vec3 Colour; }
    Outputs = { vec3 Result; }
    Graph = { Result = Colour * 0.5; }
}
)"), *Fixture.MakeLegacyAssetName(FunctionName));

	FString FunctionFilePath;
	if (!Fixture.WriteSource(*this, MaterialSource)
		|| !Fixture.WriteSiblingSource(*this, TEXT("Z_Function.dsf"), FunctionSource, FunctionFilePath))
	{
		return false;
	}

	// The material first, as a path sort would list `A_...` before `Z_...`: not SortDreamShaderSourcesForBatch,
	// which would hide the case by putting the .dsf first.
	FDreamShaderCompileRequest RequestTemplate;
	RequestTemplate.bForce = true;
	RequestTemplate.ThinCustomPersistence = EThinCustomPersistence::Materialized;

	TMap<FString, FDreamShaderBatchCompileEntry> Entries;
	const int32 Failures = CompileDreamShaderSourcesInBatch(*Compiler, { Fixture.GetSourceFilePath(), FunctionFilePath }, RequestTemplate,
		[&Entries](const FDreamShaderBatchCompileEntry& Entry) { Entries.Add(Entry.SourceFile, Entry); });

	const FDreamShaderBatchCompileEntry MaterialEntry = Entries.FindRef(Fixture.GetSourceFilePath());
	const FDreamShaderBatchCompileEntry FunctionEntry = Entries.FindRef(FunctionFilePath);
	TestEqual(FString::Printf(TEXT("the batch succeeds (material: %s)"), *MaterialEntry.Result.Message.ToString()), Failures, 0);
	TestEqual(TEXT("the material waited one pass for the function"), MaterialEntry.Attempts, 2);
	TestEqual(TEXT("the function built at once"), FunctionEntry.Attempts, 1);
	TestNotNull(TEXT("the function exists"), LoadObject<UMaterialFunction>(nullptr, *FunctionObjectPath));
	TestNotNull(TEXT("the material exists"), LoadObject<UMaterial>(nullptr, *MaterialObjectPath));
	return true;
}

#endif
