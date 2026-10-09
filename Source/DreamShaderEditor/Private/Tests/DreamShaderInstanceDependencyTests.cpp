// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderGenerationProgress.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"

namespace UE::DreamShader::Editor::Private::InstanceDependencyTests
{
	using namespace Tests;

	bool Compile(FAutomationTestBase& Test, const FDreamShaderCompile2Fixture& Fixture)
	{
		FDreamShaderError Error;
		const bool bCompiled = CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ false, /*bEphemeralThinCustom*/ false);
		return ExpectDreamShaderTestCompile(Test, TEXT("the instance compiles without forcing"), bCompiled, Error);
	}

	void Prepare(FAutomationTestBase& Test, FDreamShaderCompile2Fixture& Fixture, const TCHAR* BaseName, const TCHAR* ParentName, const TCHAR* ChildName)
	{
		Test.AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
		Fixture.TrackObjectPath(Fixture.MakeObjectPath(BaseName));
		Fixture.TrackObjectPath(Fixture.MakeObjectPath(ParentName));
		Fixture.TrackObjectPath(Fixture.MakeObjectPath(ChildName));
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceAncestorRefreshTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Instance.Dependencies.AncestorRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceAncestorRefreshTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::InstanceDependencyTests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("MI_DepAncestorChild"), TEXT("InstanceDependencies"), TEXT("dsi"));
	Prepare(*this, Fixture, TEXT("M_DepAncestorBase"), TEXT("MI_DepAncestorParent"), TEXT("MI_DepAncestorChild"));
	const auto BaseSource = [](const TCHAR* Gain)
	{
		return FString::Printf(TEXT(
			"#pragma material(Backend = Graph, ShadingModel = Unlit)\n"
			"uniform float Gain = %s;\n"
			"export void M_DepAncestorBase(inout material m) { m.EmissiveColor = Gain; }\n"), Gain);
	};
	FString BasePath;
	FString ParentPath;
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_DepAncestorBase.dss"), BaseSource(TEXT("0.25")), BasePath)
		|| !Fixture.WriteSiblingSource(*this, TEXT("MI_DepAncestorParent.dsi"), TEXT("#pragma instance(Parent = \"M_DepAncestorBase\")\n"), ParentPath)
		|| !Fixture.WriteSource(*this, TEXT("#pragma instance(Parent = \"MI_DepAncestorParent\")\n"))
		|| !Compile(*this, Fixture))
	{
		return false;
	}

	UMaterialInstanceConstant* Child = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_DepAncestorChild")));
	UMaterial* Base = LoadObject<UMaterial>(nullptr, *Fixture.MakeObjectPath(TEXT("M_DepAncestorBase")));
	if (!TestNotNull(TEXT("the leaf instance exists"), Child) || !TestNotNull(TEXT("the root material exists"), Base))
	{
		return false;
	}
	float Gain = 0.0f;
	TestTrue(TEXT("the leaf initially inherits Gain"), Child->GetScalarParameterValue(FName(TEXT("Gain")), Gain));
	TestEqual(TEXT("the initial inherited Gain"), Gain, 0.25f);

	// The intermediate .dsi stays byte-for-byte unchanged. Compiling only the leaf must still
	// refresh its stale ancestor, even when the intermediate asset's own source key is current.
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_DepAncestorBase.dss"), BaseSource(TEXT("0.75")), BasePath)
		|| !Compile(*this, Fixture))
	{
		return false;
	}
	TestTrue(TEXT("the root retains the Gain parameter"), Base->GetScalarParameterValue(FName(TEXT("Gain")), Gain));
	TestEqual(TEXT("compiling the leaf refreshes its grandparent's default"), Gain, 0.75f);
	TestTrue(TEXT("the leaf still inherits Gain"), Child->GetScalarParameterValue(FName(TEXT("Gain")), Gain));
	TestEqual(TEXT("the leaf sees the new ancestor value"), Gain, 0.75f);

	const FString GoodDigest = BuildOutputDigest(Base);
	const FString GoodSourceHash = GetGeneratedAssetSourceHash(Base);
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_DepAncestorBase.dss"), BaseSource(TEXT("0.875")), BasePath))
	{
		return false;
	}
	{
		// Cancel specifically inside ancestor emission, after rollback has captured its old graph.
		// A parent run without progress/cancellation checks would finish and save the new value.
		FScopedDreamShaderGenerationCancelOverride CancelAncestor(
			[Base]() { return Base->GetExpressions().Num() == 0; });
		UE::DreamShader::FDreamShaderError Error;
		const bool bCompiled = CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), Error, /*bForce*/ false, /*bEphemeralThinCustom*/ false);
		TestFalse(TEXT("cancelling the ancestor fails the leaf compile"), bCompiled);
		TestTrue(TEXT("the nested failure retains DSH8298"), Error.Code == TEXT("DSH8298") || Error.Message.Contains(TEXT("DSH8298")));
	}
	TestEqual(TEXT("cancelled ancestor emission restores its graph"), BuildOutputDigest(Base), GoodDigest);
	TestEqual(TEXT("cancelled ancestor emission keeps the saved source key"), GetGeneratedAssetSourceHash(Base), GoodSourceHash);
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceInheritedVectorChannelsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Instance.Dependencies.InheritedVectorChannels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceInheritedVectorChannelsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::Tests;
	using namespace UE::DreamShader::Editor::Private::InstanceDependencyTests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("MI_DepVectorChild"), TEXT("InstanceDependencies"), TEXT("dsi"));
	Prepare(*this, Fixture, TEXT("M_DepVectorBase"), TEXT("MI_DepVectorParent"), TEXT("MI_DepVectorChild"));
	const auto ParentSource = [](const TCHAR* Alpha)
	{
		return FString::Printf(TEXT(
			"#pragma instance(Parent = \"M_DepVectorBase\")\n"
			"uniform float4 Tint = float4(0.1, 0.2, 0.3, %s);\n"), Alpha);
	};
	FString BasePath;
	FString ParentPath;
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_DepVectorBase.dss"), TEXT(
		"#pragma material(Backend = Graph, ShadingModel = Unlit)\n"
		"uniform float3 Tint = float3(1, 1, 1);\n"
		"export void M_DepVectorBase(inout material m) { m.EmissiveColor = Tint; }\n"), BasePath)
		|| !Fixture.WriteSiblingSource(*this, TEXT("MI_DepVectorParent.dsi"), ParentSource(TEXT("0.25")), ParentPath)
		|| !Fixture.WriteSource(*this, TEXT(
			"#pragma instance(Parent = \"MI_DepVectorParent\")\n"
			"uniform float3 Tint = float3(0.5, 0.6, 0.7);\n"))
		|| !Compile(*this, Fixture))
	{
		return false;
	}

	UMaterialInstanceConstant* Child = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_DepVectorChild")));
	UMaterialInstanceConstant* Parent = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_DepVectorParent")));
	if (!TestNotNull(TEXT("the leaf instance exists"), Child) || !TestNotNull(TEXT("the parent instance exists"), Parent))
	{
		return false;
	}
	FLinearColor Tint;
	TestTrue(TEXT("the leaf overrides Tint"), Child->GetVectorParameterValue(FName(TEXT("Tint")), Tint));
	TestEqual(TEXT("the first build inherits the parent's alpha"), Tint.A, 0.25f);

	// A float3 override keeps the parent's alpha. That inherited channel is part of the emitted
	// value, so an unchanged child source must not skip after its parent's alpha changes.
	if (!Fixture.WriteSiblingSource(*this, TEXT("MI_DepVectorParent.dsi"), ParentSource(TEXT("0.75")), ParentPath)
		|| !Compile(*this, Fixture))
	{
		return false;
	}
	TestTrue(TEXT("the parent still overrides Tint"), Parent->GetVectorParameterValue(FName(TEXT("Tint")), Tint));
	TestEqual(TEXT("the parent was rebuilt before the child"), Tint.A, 0.75f);
	TestTrue(TEXT("the leaf still overrides Tint"), Child->GetVectorParameterValue(FName(TEXT("Tint")), Tint));
	TestEqual(TEXT("the leaf keeps its explicit red"), Tint.R, 0.5f);
	TestEqual(TEXT("the leaf keeps its explicit green"), Tint.G, 0.6f);
	TestEqual(TEXT("the leaf keeps its explicit blue"), Tint.B, 0.7f);
	TestEqual(TEXT("an unchanged float3 override inherits the new alpha"), Tint.A, 0.75f);

	// Unrelated parent defaults remain inherited at runtime; they must not invalidate a child
	// whose materialized vector override has not changed.
	const FString ChildHash = GetGeneratedAssetSourceHash(Child);
	TestFalse(TEXT("the child has a stamped build key"), ChildHash.IsEmpty());
	if (!Fixture.WriteSiblingSource(*this, TEXT("M_DepVectorBase.dss"), TEXT(
		"#pragma material(Backend = Graph, ShadingModel = Unlit)\n"
		"uniform float3 Tint = float3(1, 1, 1);\n"
		"uniform float UnrelatedGain = 2.0;\n"
		"export void M_DepVectorBase(inout material m) { m.EmissiveColor = Tint * UnrelatedGain; }\n"), BasePath)
		|| !Compile(*this, Fixture))
	{
		return false;
	}
	TestEqual(TEXT("an unrelated parent parameter leaves the child's key unchanged"), GetGeneratedAssetSourceHash(Child), ChildHash);
	float Gain = 0.0f;
	TestTrue(TEXT("the unchanged child inherits the new ancestor parameter"), Child->GetScalarParameterValue(FName(TEXT("UnrelatedGain")), Gain));
	TestEqual(TEXT("the inherited unrelated value is current"), Gain, 2.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
