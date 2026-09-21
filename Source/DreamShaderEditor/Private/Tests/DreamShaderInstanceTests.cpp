// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Compiler2.Instance.* -- a `.dsi` end to end: one source, one UMaterialInstanceConstant.
//
// The graph-shaped half of an instance (what it overrides, as a dump) is data: Tests/Corpus/Instance, run by
// DreamShader.Compiler2.CorpusInstance. These are the claims a dump cannot make: how a Parent resolves among the
// products beside the file, what a settings key does to the instance and what deleting it undoes, what a hand edit of
// the asset reads as, and which instances follow a parent's rebuild.

#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DreamShaderCompilePipeline.h"
#include "DreamShaderGeneratedAssetDigest.h"
#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderProductIndex.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::InstanceTests
{
	using namespace UE::DreamShader::Editor::Private::Tests;

	static const TCHAR* const GParentSource = TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"uniform float Gain = 0.5;\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"export void {PARENT}(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Tint * Gain;\n"
		"}\n");

	inline FString MakeParentSource(const TCHAR* ParentName)
	{
		return FString(GParentSource).Replace(TEXT("{PARENT}"), ParentName, ESearchCase::CaseSensitive);
	}

	/** A fixture whose main source is a `.dsi`, with its parent `.dss` written beside it. */
	inline bool WriteInstanceFixture(FAutomationTestBase& Test, FDreamShaderCompile2Fixture& Fixture, const TCHAR* ParentName, const FString& InstanceSource)
	{
		Test.AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		Test.AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);

		FString ParentPath;
		if (!Fixture.WriteSiblingSource(Test, FString(ParentName) + TEXT(".dss"), MakeParentSource(ParentName), ParentPath))
		{
			return false;
		}
		Fixture.TrackObjectPath(Fixture.MakeObjectPath(ParentName));
		return Fixture.WriteSource(Test, InstanceSource);
	}

	inline bool Compile(FDreamShaderCompile2Fixture& Fixture, UE::DreamShader::FDreamShaderError& OutError)
	{
		return CompileDreamShaderTestAssets(Fixture.GetSourceFilePath(), OutError, /*bForce*/ true, /*bEphemeralThinCustom*/ false);
	}
}

// ---------------------------------------------------------------------------------------------
// The compile, and what the keys of `#pragma instance` do
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceKeyTableTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Instance.KeyTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceKeyTableTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::InstanceTests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("MI_InKeys"), TEXT("Instance2"), TEXT("dsi"));
	if (!WriteInstanceFixture(*this, Fixture, TEXT("M_InKeysParent"), TEXT(
		"#pragma instance(Parent = \"M_InKeysParent\", BlendMode = Translucent)\n"
		"uniform float Gain = 2.0;\n")))
	{
		return false;
	}

	// Before anything is built: the source resolves to one MaterialInstance product.
	{
		::UE::DreamShader::Editor::Compiler::FDreamShaderProductResolution Resolution;
		const bool bResolved = ::UE::DreamShader::Editor::Compiler::ResolveDreamShaderSourceProducts(Fixture.GetSourceFilePath(), Resolution);
		TestTrue(TEXT("a `.dsi` resolves to its product without building it"), bResolved && Resolution.Products.Num() == 1);
		if (Resolution.Products.Num() == 1)
		{
			TestTrue(TEXT("of kind MaterialInstance"), Resolution.Products[0].Kind == UE::DreamShader::IR::EIRProductKind::MaterialInstance);
			TestTrue(TEXT("at the fixture's package path"), Resolution.Products[0].ObjectPath.Equals(Fixture.MakeObjectPath(TEXT("MI_InKeys")), ESearchCase::IgnoreCase));
		}
	}

	UE::DreamShader::FDreamShaderError Error;
	const bool bInstanceCompiled = Compile(Fixture, Error);
	if (!ExpectDreamShaderTestCompile(*this, TEXT("the instance compiles, building its stale parent first"), bInstanceCompiled, Error))
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MI_InKeys")));

	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_InKeys")));
	UMaterial* Parent = LoadObject<UMaterial>(nullptr, *Fixture.MakeObjectPath(TEXT("M_InKeysParent")));
	if (!TestNotNull(TEXT("the instance asset"), Instance) || !TestNotNull(TEXT("the parent asset"), Parent))
	{
		return false;
	}
	TestTrue(TEXT("the bare-name Parent resolved to the sibling product"), Instance->Parent == Parent);

	float Gain = 0.0f;
	TestTrue(TEXT("the scalar override is set"), Instance->GetScalarParameterValue(FName(TEXT("Gain")), Gain) && FMath::IsNearlyEqual(Gain, 2.0f));

	// A key sets the value AND the flag that makes the instance use it.
	TestTrue(TEXT("BlendMode: the override flag"), Instance->BasePropertyOverrides.bOverride_BlendMode);
	TestTrue(TEXT("BlendMode: the value"), Instance->BasePropertyOverrides.BlendMode == BLEND_Translucent);

	// A freshly built instance is what its source says.
	TestTrue(TEXT("the instance reads as Generated"), ClassifyGeneratedAsset(Instance) == EDreamShaderDigestState::Generated);

	// A hand edit of a base property is a divergence like any other.
	Instance->BasePropertyOverrides.BlendMode = BLEND_Additive;
	TestTrue(TEXT("a hand-edited BlendMode reads as Diverged"), ClassifyGeneratedAsset(Instance) == EDreamShaderDigestState::Diverged);
	Instance->BasePropertyOverrides.BlendMode = BLEND_Translucent;

	// Deleting the key reverts it on the next build.
	if (Fixture.WriteSource(*this, TEXT("#pragma instance(Parent = \"M_InKeysParent\")\nuniform float Gain = 2.0;\n")))
	{
		UE::DreamShader::FDreamShaderError Again;
		const bool bRebuilt = Compile(Fixture, Again);
		ExpectDreamShaderTestCompile(*this, TEXT("the rebuild succeeds"), bRebuilt, Again);
		Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Fixture.MakeObjectPath(TEXT("MI_InKeys")));
		if (TestNotNull(TEXT("the instance after the rebuild"), Instance))
		{
			TestFalse(TEXT("a deleted key is no longer overridden"), Instance->BasePropertyOverrides.bOverride_BlendMode);
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Parent resolution
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceParentResolutionTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Instance.ParentResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceParentResolutionTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::InstanceTests;

	FScopedDreamShaderGraphBackendPin BackendPin;

	// No product of that name.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("MI_InNoParent"), TEXT("Instance2"), TEXT("dsi"));
		AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		if (Fixture.WriteSource(*this, TEXT("#pragma instance(Parent = \"M_InNobodyBuildsThis\")\nuniform float Gain = 2.0;\n")))
		{
			UE::DreamShader::FDreamShaderError Error;
			TestFalse(TEXT("a parent nothing builds: refused"), Compile(Fixture, Error));
			TestTrue(FString::Printf(TEXT("with DSH8261 (%s: %s)"), *Error.Code, *Error.Message), Error.Code == TEXT("DSH8261") || Error.Message.Contains(TEXT("DSH8261")));
		}
	}

	// Two products of one name under the root: a bare name cannot choose.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("MI_InAmbiguous"), TEXT("Instance2"), TEXT("dsi"));
		AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		FString First;
		FString Second;
		if (Fixture.WriteSiblingSource(*this, TEXT("A/M_InTwin.dss"), MakeParentSource(TEXT("M_InTwin")), First)
			&& Fixture.WriteSiblingSource(*this, TEXT("B/M_InTwin.dss"), MakeParentSource(TEXT("M_InTwin")), Second)
			&& Fixture.WriteSource(*this, TEXT("#pragma instance(Parent = \"M_InTwin\")\nuniform float Gain = 2.0;\n")))
		{
			UE::DreamShader::FDreamShaderError Error;
			TestFalse(TEXT("two products of one name: refused"), Compile(Fixture, Error));
			TestTrue(FString::Printf(TEXT("with DSH8262 (%s: %s)"), *Error.Code, *Error.Message), Error.Code == TEXT("DSH8262") || Error.Message.Contains(TEXT("DSH8262")));
		}
	}

	// A path names one of them.
	{
		FDreamShaderCompile2Fixture Fixture(TEXT("MI_InByPath"), TEXT("Instance2"), TEXT("dsi"));
		AddExpectedError(Fixture.GetPackagePath(), EAutomationExpectedErrorFlags::Contains, -1);
		AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, -1);
		FString ParentPath;
		const FString InstanceSource = FString::Printf(TEXT("#pragma instance(Parent = \"%s/Sub/M_InPathParent\")\nuniform float Gain = 2.0;\n"), *Fixture.GetPackagePath());
		if (Fixture.WriteSiblingSource(*this, TEXT("Sub/M_InPathParent.dss"), MakeParentSource(TEXT("M_InPathParent")), ParentPath)
			&& Fixture.WriteSource(*this, InstanceSource))
		{
			UE::DreamShader::FDreamShaderError Error;
			const bool bPackageParentCompiled = Compile(Fixture, Error);
			ExpectDreamShaderTestCompile(*this, TEXT("a package path Parent compiles"), bPackageParentCompiled, Error);
			Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MI_InByPath")));
			Fixture.TrackObjectPath(FString::Printf(TEXT("%s/Sub/M_InPathParent.M_InPathParent"), *Fixture.GetPackagePath()));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Who follows a parent's rebuild
// ---------------------------------------------------------------------------------------------

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FDreamShaderInstanceDependentsTest,
	UE::DreamShader::Editor::Private::Tests::FDreamShaderCompile2CorpusTestBase,
	"DreamShader.Compiler2.Instance.Dependents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderInstanceDependentsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private;
	using namespace UE::DreamShader::Editor::Private::InstanceTests;

	FScopedDreamShaderGraphBackendPin BackendPin;
	FDreamShaderCompile2Fixture Fixture(TEXT("MI_InChild"), TEXT("Instance2"), TEXT("dsi"));
	if (!WriteInstanceFixture(*this, Fixture, TEXT("M_InChainParent"), TEXT("#pragma instance(Parent = \"M_InChainParent\")\nuniform float Gain = 2.0;\n")))
	{
		return false;
	}
	FString GrandchildPath;
	if (!Fixture.WriteSiblingSource(*this, TEXT("MI_InGrandchild.dsi"), TEXT("#pragma instance(Parent = \"MI_InChild\")\nuniform float Gain = 3.0;\n"), GrandchildPath))
	{
		return false;
	}
	const FString ParentSourcePath = UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(FPaths::GetPath(Fixture.GetSourceFilePath()), TEXT("M_InChainParent.dss")));

	::UE::DreamShader::Editor::Compiler::FDreamShaderProductIndex::Get().Refresh();

	// The chain, transitively, each source once.
	TArray<FString> Dependents;
	::UE::DreamShader::Editor::Compiler::CollectInstanceDependents(ParentSourcePath, Dependents);
	TestTrue(FString::Printf(TEXT("the child follows the parent (%s)"), *FString::Join(Dependents, TEXT(", "))), Dependents.ContainsByPredicate([&Fixture](const FString& Path) { return Path.Equals(Fixture.GetSourceFilePath(), ESearchCase::IgnoreCase); }));
	TestTrue(TEXT("and the grandchild follows the child"), Dependents.ContainsByPredicate([&GrandchildPath](const FString& Path) { return Path.Equals(GrandchildPath, ESearchCase::IgnoreCase); }));
	TestEqual(TEXT("each once"), Dependents.Num(), 2);

	// The dependency sort's edge: parent before instance.
	TestTrue(TEXT("the child's parent source is the `.dss`"), ::UE::DreamShader::Editor::Compiler::FindInstanceParentSourceFile(Fixture.GetSourceFilePath()).Equals(ParentSourcePath, ESearchCase::IgnoreCase));
	TestTrue(TEXT("the grandchild's parent source is the child"), ::UE::DreamShader::Editor::Compiler::FindInstanceParentSourceFile(GrandchildPath).Equals(Fixture.GetSourceFilePath(), ESearchCase::IgnoreCase));

	// Retyping the parent's uniform surfaces as the CHILD's error, with no edit of the child.
	UE::DreamShader::FDreamShaderError Error;
	const bool bChainCompiled = CompileDreamShaderTestAssets(GrandchildPath, Error, /*bForce*/ true, false);
	if (!ExpectDreamShaderTestCompile(*this, TEXT("the chain compiles first"), bChainCompiled, Error))
	{
		return false;
	}
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MI_InChild")));
	Fixture.TrackObjectPath(Fixture.MakeObjectPath(TEXT("MI_InGrandchild")));

	FString Unused;
	if (Fixture.WriteSiblingSource(*this, TEXT("M_InChainParent.dss"), TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Opaque)\n"
		"uniform float3 Gain = float3(0.5, 0.5, 0.5);\n"
		"export void M_InChainParent(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Gain;\n"
		"}\n"), Unused))
	{
		UE::DreamShader::FDreamShaderError ChildError;
		TestFalse(TEXT("the child no longer fits its parent"), Compile(Fixture, ChildError));
		TestTrue(FString::Printf(TEXT("and says DSH7259 (%s: %s)"), *ChildError.Code, *ChildError.Message), ChildError.Code == TEXT("DSH7259") || ChildError.Message.Contains(TEXT("DSH7259")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
