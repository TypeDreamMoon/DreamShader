// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.SubstrateSugar.* -- the Substrate sugar (LangBinderSubstrate.cpp, IRBuilderSubstrate.cpp).
//
// A sugar is a spelling and nothing else, and that is what is tested: a source written with the sugar and a source
// written with the nodes it stands for lower to equivalent modules (AreDreamShaderIRModulesEquivalent, which looks at
// the graph and not at how it was written down). What the corpus cannot say lives here too: an engine whose catalog
// lacks a node (the version gates, S7), and the sugar read back by the decompiler.
//
// Core only: the hand-built test catalog, the shared IR runner, no assets.

#include "DreamShaderTestCommon.h"
#include "DreamShaderTestCorpusLayers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Decompile/IRToAst.h"
#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCompare.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::SubstrateSugarTests
{
	using namespace UE::DreamShader::IR;

	using FIRRun = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRun;
	using FIRRunOptions = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRunOptions;

	/** The head every source here shares: three values to build from. */
	static const TCHAR* const GHead = TEXT(
		"uniform float3 Albedo = float3(0.8, 0.1, 0.1);\n"
		"uniform float Metal = 1.0;\n"
		"uniform float Rough = 0.4;\n"
		"uniform float Haze = 0.3;\n"
		"uniform float Blend = 0.25;\n"
		"export void M_Sugar(inout material m)\n"
		"{\n"
		"    Substrate A = Substrate.Slab(Roughness = 0.8);\n"
		"    Substrate B = Substrate.Slab(Roughness = 0.1);\n");

	inline FString Source(const TCHAR* Body)
	{
		return FString(GHead) + Body + TEXT("}\n");
	}

	inline void Lower(FIRRun& Run, const FString& Text, const FIRRunOptions& Options = FIRRunOptions())
	{
		UE::DreamShader::Editor::Private::Tests::RunDreamShaderIRPipeline(TEXT("Sugar.dss"), Text, Options, Run);
	}

	/** Both build, and to the same graph. */
	inline void ExpectSame(FAutomationTestBase& Test, const TCHAR* What, const TCHAR* Sugar, const TCHAR* Spelled)
	{
		FIRRun Left;
		FIRRun Right;
		Lower(Left, Source(Sugar));
		Lower(Right, Source(Spelled));
		if (!Left.Module.IsValid() || !Left.Succeeded())
		{
			Test.AddError(FString::Printf(TEXT("%s: the sugar does not build: %s"), What, *Left.ErrorText()));
			return;
		}
		if (!Right.Module.IsValid() || !Right.Succeeded())
		{
			Test.AddError(FString::Printf(TEXT("%s: the spelled-out form does not build: %s"), What, *Right.ErrorText()));
			return;
		}

		FIRCompareOptions CompareOptions;
		FString Difference;
		const bool bSame = AreDreamShaderIRModulesEquivalent(*Left.Module, *Right.Module, CompareOptions, Difference);
		Test.TestTrue(FString::Printf(TEXT("%s: the sugar and the nodes it stands for are one graph (%s)"), What, *Difference), bSame);
	}

	inline bool HasCode(const TArray<FString>& Lines, const TCHAR* Code)
	{
		return Lines.ContainsByPredicate([Code](const FString& Line) { return Line.Contains(Code, ESearchCase::CaseSensitive); });
	}

	inline void ExpectError(FAutomationTestBase& Test, const TCHAR* What, const FString& Text, const TCHAR* Code, const FIRRunOptions& Options = FIRRunOptions())
	{
		FIRRun Run;
		Lower(Run, Text, Options);
		Test.TestTrue(FString::Printf(TEXT("%s: refused with %s (got: %s)"), What, Code, *Run.ErrorText()), HasCode(Run.Errors, Code));
	}

	/** The test catalog without one engine class: an engine that is too old for it. */
	inline FBuiltinCatalog CatalogWithout(const TCHAR* EngineClass)
	{
		FBuiltinCatalog Catalog = UE::DreamShader::Editor::Private::Tests::GetDreamShaderTestBuiltinCatalog();
		Catalog.Expressions.RemoveAll([EngineClass](const FCatalogExpression& Expression)
		{
			return Expression.ClassName.Equals(EngineClass, ESearchCase::CaseSensitive);
		});
		return Catalog;
	}

	inline int32 CountNodes(const FIRGraph& Graph, const EIROp Op, const TCHAR* ClassName = nullptr)
	{
		int32 Count = 0;
		for (const FIRNode& Node : Graph.Nodes)
		{
			if (Node.Op == Op && (!ClassName || Node.ClassName.Equals(ClassName, ESearchCase::CaseSensitive)))
			{
				++Count;
			}
		}
		return Count;
	}
}

// ---------------------------------------------------------------------------------------------
// S1: operators, lerp, positional arguments, short names
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSubstrateSugarOperatorsTest,
	"DreamShader.Lang2.SubstrateSugar.Operators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSubstrateSugarOperatorsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::SubstrateSugarTests;

	ExpectSame(*this, TEXT("A + B"),
		TEXT("    m.FrontMaterial = A + B;\n"),
		TEXT("    m.FrontMaterial = Substrate.Add(A = A, B = B);\n"));
	ExpectSame(*this, TEXT("A * w"),
		TEXT("    m.FrontMaterial = A * Blend;\n"),
		TEXT("    m.FrontMaterial = Substrate.Weight(A = A, Weight = Blend);\n"));
	ExpectSame(*this, TEXT("w * A"),
		TEXT("    m.FrontMaterial = Blend * A;\n"),
		TEXT("    m.FrontMaterial = Substrate.Weight(A = A, Weight = Blend);\n"));
	ExpectSame(*this, TEXT("lerp(A, B, t)"),
		TEXT("    m.FrontMaterial = lerp(A, B, Blend);\n"),
		TEXT("    m.FrontMaterial = Substrate.HorizontalMix(Background = A, Foreground = B, Mix = Blend);\n"));
	ExpectSame(*this, TEXT("precedence: A + B * w"),
		TEXT("    m.FrontMaterial = A + B * Blend;\n"),
		TEXT("    m.FrontMaterial = Substrate.Add(A = A, B = Substrate.Weight(A = B, Weight = Blend));\n"));

	ExpectSame(*this, TEXT("positional Add"),
		TEXT("    m.FrontMaterial = Substrate.Add(A, B);\n"),
		TEXT("    m.FrontMaterial = Substrate.Add(A = A, B = B);\n"));
	ExpectSame(*this, TEXT("Substrate.Mix"),
		TEXT("    m.FrontMaterial = Substrate.Mix(A, B, Blend);\n"),
		TEXT("    m.FrontMaterial = Substrate.HorizontalMixing(Background = A, Foreground = B, Mix = Blend);\n"));
	ExpectSame(*this, TEXT("Substrate.Layer"),
		TEXT("    m.FrontMaterial = Substrate.Layer(A, B, 0.01);\n"),
		TEXT("    m.FrontMaterial = Substrate.VerticalLayering(Top = A, Base = B, Thickness = 0.01);\n"));

	ExpectError(*this, TEXT("A - B"), Source(TEXT("    m.FrontMaterial = A - B;\n")), TEXT("DSH5293"));
	ExpectError(*this, TEXT("A * B"), Source(TEXT("    m.FrontMaterial = A * B;\n")), TEXT("DSH5293"));
	ExpectError(*this, TEXT("-A"), Source(TEXT("    m.FrontMaterial = -A;\n")), TEXT("DSH"));
	ExpectError(*this, TEXT("lerp(A, 1.0, t)"), Source(TEXT("    m.FrontMaterial = lerp(A, 1.0, Blend);\n")), TEXT("DSH5293"));
	ExpectError(*this, TEXT("A += B"), Source(TEXT("    A += B;\n    m.FrontMaterial = A;\n")), TEXT("DSH5293"));
	return true;
}

// ---------------------------------------------------------------------------------------------
// S2: a run-time branch is a Select
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSubstrateSugarSelectTest,
	"DreamShader.Lang2.SubstrateSugar.Select",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSubstrateSugarSelectTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::SubstrateSugarTests;

	// `c ? T : F` is Select(A = F, B = T, SelectValue = c): the engine's node answers B above its threshold.
	ExpectSame(*this, TEXT("c ? B : A"),
		TEXT("    m.FrontMaterial = Blend > 0.5 ? B : A;\n"),
		TEXT("    m.FrontMaterial = Substrate.Select(A = A, B = B, SelectValue = Blend > 0.5);\n"));
	ExpectSame(*this, TEXT("if"),
		TEXT("    Substrate Front = A;\n    if (Blend > 0.5) { Front = B; }\n    m.FrontMaterial = Front;\n"),
		TEXT("    m.FrontMaterial = Substrate.Select(A = A, B = B, SelectValue = Blend > 0.5);\n"));

	// S7: an engine without the node. The branch says what it would have become; the call says which engine has it.
	const FBuiltinCatalog NoSelect = CatalogWithout(TEXT("MaterialExpressionSubstrateSelect"));
	FIRRunOptions Options;
	Options.Catalog = &NoSelect;
	ExpectError(*this, TEXT("branch without Select"), Source(TEXT("    m.FrontMaterial = Blend > 0.5 ? B : A;\n")), TEXT("DSH4378"), Options);

	FIRRun Gate;
	Lower(Gate, Source(TEXT("    m.FrontMaterial = Substrate.Select(A, B, Blend);\n")), Options);
	TestTrue(FString::Printf(TEXT("Substrate.Select without the node is DSH5294 (got: %s)"), *Gate.ErrorText()), HasCode(Gate.Errors, TEXT("DSH5294")));
	TestTrue(TEXT("...and names the engine that has it"), HasCode(Gate.Errors, TEXT("5.6")));

	const FBuiltinCatalog NoMix = CatalogWithout(TEXT("MaterialExpressionSubstrateHorizontalMixing"));
	Options.Catalog = &NoMix;
	ExpectError(*this, TEXT("lerp without HorizontalMixing"), Source(TEXT("    m.FrontMaterial = lerp(A, B, Blend);\n")), TEXT("DSH5294"), Options);
	return true;
}

// ---------------------------------------------------------------------------------------------
// S3: virtual arguments
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSubstrateSugarVirtualArgumentsTest,
	"DreamShader.Lang2.SubstrateSugar.VirtualArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSubstrateSugarVirtualArgumentsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::SubstrateSugarTests;

	// The spelled-out side calls the conversion once per output; the dedupe pass makes them one node, as it would
	// for any two equal calls, and that is the one node the sugar made.
	ExpectSame(*this, TEXT("BaseColor / Metallic"),
		TEXT("    m.FrontMaterial = Substrate.Slab(BaseColor = Albedo, Metallic = Metal, Roughness = Rough);\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab(\n"
		     "        DiffuseAlbedo = Substrate.MetalnessToDiffuseAlbedoF0(BaseColor = Albedo, Metallic = Metal).DiffuseAlbedo,\n"
		     "        F0 = Substrate.MetalnessToDiffuseAlbedoF0(BaseColor = Albedo, Metallic = Metal).F0,\n"
		     "        Roughness = Rough);\n"));
	ExpectSame(*this, TEXT("Haziness"),
		TEXT("    m.FrontMaterial = Substrate.Slab(Roughness = Rough, Haziness = Haze);\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab(\n"
		     "        Roughness = Rough,\n"
		     "        SecondRoughness = Substrate.HazinessToSecondaryRoughness(BaseRoughness = Rough, Haziness = Haze).SecondRoughness,\n"
		     "        SecondRoughnessWeight = Substrate.HazinessToSecondaryRoughness(BaseRoughness = Rough, Haziness = Haze).SecondRoughnessWeight);\n"));
	ExpectSame(*this, TEXT("Transmittance, Thickness"),
		TEXT("    m.FrontMaterial = Substrate.Slab(Roughness = Rough, Transmittance = Albedo, Thickness = 0.2);\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab(\n"
		     "        Roughness = Rough,\n"
		     "        SSSMFP = Substrate.TransmittanceToMFP(TransmittanceColor = Albedo, Thickness = 0.2).MFP);\n"));
	ExpectSame(*this, TEXT("SimpleClearCoat takes the metalness family"),
		TEXT("    m.FrontMaterial = Substrate.SimpleClearCoat(BaseColor = Albedo, Metallic = Metal, Roughness = Rough);\n"),
		TEXT("    m.FrontMaterial = Substrate.SimpleClearCoat(\n"
		     "        DiffuseAlbedo = Substrate.MetalnessToDiffuseAlbedoF0(BaseColor = Albedo, Metallic = Metal).DiffuseAlbedo,\n"
		     "        F0 = Substrate.MetalnessToDiffuseAlbedoF0(BaseColor = Albedo, Metallic = Metal).F0,\n"
		     "        Roughness = Rough);\n"));

	// IOR: a number folds into a constant F0; a computed index is four nodes.
	{
		FIRRun Constant;
		Lower(Constant, Source(TEXT("    m.FrontMaterial = Substrate.Slab(IOR = 1.5, Roughness = Rough);\n")));
		FIRRun Computed;
		Lower(Computed, Source(TEXT("    m.FrontMaterial = Substrate.Slab(IOR = 1.0 + Blend, Roughness = Rough);\n")));
		if (TestTrue(TEXT("IOR builds both ways"), Constant.Succeeded() && Computed.Succeeded()))
		{
			TestEqual(TEXT("a constant IOR divides nothing"), CountNodes(Constant.Module->Products[0].Graph, EIROp::Divide), 0);
			TestEqual(TEXT("a computed IOR divides once"), CountNodes(Computed.Module->Products[0].Graph, EIROp::Divide), 1);
		}
	}

	ExpectError(*this, TEXT("BaseColor against DiffuseAlbedo"),
		Source(TEXT("    m.FrontMaterial = Substrate.Slab(BaseColor = Albedo, DiffuseAlbedo = Albedo);\n")), TEXT("DSH5295"));
	ExpectError(*this, TEXT("IOR against Metallic"),
		Source(TEXT("    m.FrontMaterial = Substrate.Slab(IOR = 1.5, Metallic = Metal);\n")), TEXT("DSH5295"));
	ExpectError(*this, TEXT("Haziness without Roughness"),
		Source(TEXT("    m.FrontMaterial = Substrate.Slab(Haziness = Haze);\n")), TEXT("DSH5296"));
	ExpectError(*this, TEXT("Thickness alone"),
		Source(TEXT("    m.FrontMaterial = Substrate.Slab(Roughness = Rough, Thickness = 0.2);\n")), TEXT("DSH5296"));
	ExpectError(*this, TEXT("BaseColor twice"),
		Source(TEXT("    m.FrontMaterial = Substrate.Slab(BaseColor = Albedo, BaseColor = Albedo);\n")), TEXT("DSH4215"));

	// A class that has the name as a real pin takes it as that pin: ShadingModels' BaseColor converts nothing.
	{
		FIRRun Run;
		Lower(Run, Source(TEXT("    m.FrontMaterial = Substrate.ShadingModels(BaseColor = Albedo, Roughness = Rough);\n")));
		if (TestTrue(FString::Printf(TEXT("ShadingModels(BaseColor = ...) builds (%s)"), *Run.ErrorText()), Run.Succeeded()))
		{
			TestEqual(TEXT("...and makes no conversion node"), CountNodes(Run.Module->Products[0].Graph, EIROp::Reflected, TEXT("MetalnessToDiffuseAlbedoF0")), 0);
		}
	}

	// S7: an engine without the conversion node.
	const FBuiltinCatalog NoConversion = CatalogWithout(TEXT("MaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0"));
	FIRRunOptions Options;
	Options.Catalog = &NoConversion;
	ExpectError(*this, TEXT("BaseColor without the conversion node"),
		Source(TEXT("    m.FrontMaterial = Substrate.Slab(BaseColor = Albedo, Roughness = Rough);\n")), TEXT("DSH4381"), Options);
	return true;
}

// ---------------------------------------------------------------------------------------------
// S4: Bridge
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSubstrateSugarBridgeTest,
	"DreamShader.Lang2.SubstrateSugar.Bridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSubstrateSugarBridgeTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::SubstrateSugarTests;

	static const TCHAR* const Text = TEXT(
		"#pragma material(Substrate = Bridge)\n"
		"export void M_Bridge(inout material m)\n"
		"{\n"
		"    m.BaseColor = float3(0.8, 0.2, 0.2);\n"
		"    m.Roughness = 0.4;\n"
		"    m.Opacity = 1.0;\n"
		"}\n");

	const auto SinkInputs = [](const FIRRun& Run)
	{
		TArray<FString> Pins;
		const FIRGraph& Graph = Run.Module->Products[0].Graph;
		if (Graph.Nodes.IsValidIndex(Graph.Sink))
		{
			for (const FIRInput& Input : Graph.Nodes[Graph.Sink].Inputs)
			{
				Pins.Add(Input.Pin);
			}
		}
		Pins.Sort();
		return FString::Join(Pins, TEXT(","));
	};

	FIRRun Off;
	Lower(Off, Text);
	FIRRunOptions OnOptions;
	OnOptions.bSubstrateEnabled = true;
	FIRRun On;
	Lower(On, Text, OnOptions);
	if (!TestTrue(FString::Printf(TEXT("Bridge builds with Substrate off and on (%s | %s)"), *Off.ErrorText(), *On.ErrorText()), Off.Succeeded() && On.Succeeded()))
	{
		return false;
	}

	TestEqual(TEXT("both products say Bridge"), static_cast<int32>(Off.Module->Products[0].SubstrateMode), static_cast<int32>(EIRSubstrateMode::Bridge));
	TestEqual(TEXT("Substrate off: the attributes stay where they were written"), SinkInputs(Off), FString(TEXT("BaseColor,Opacity,Roughness")));
	TestEqual(TEXT("Substrate on: the shading attributes moved to FrontMaterial, Opacity stayed"), SinkInputs(On), FString(TEXT("FrontMaterial,Opacity")));
	TestEqual(TEXT("...through one ShadingModels node"), CountNodes(On.Module->Products[0].Graph, EIROp::Reflected, TEXT("ShadingModels")), 1);

	// The engine's conversion MOVES BaseColor and Roughness and COPIES Opacity: the node has all three.
	for (const FIRNode& Node : On.Module->Products[0].Graph.Nodes)
	{
		if (Node.Op == EIROp::Reflected && Node.ClassName.Equals(TEXT("ShadingModels"), ESearchCase::CaseSensitive))
		{
			TArray<FString> Pins;
			for (const FIRInput& Input : Node.Inputs)
			{
				Pins.Add(Input.Pin);
			}
			Pins.Sort();
			TestEqual(TEXT("the node's pins"), FString::Join(Pins, TEXT(",")), FString(TEXT("BaseColor,Opacity,Roughness")));
		}
	}

	// Only what the engine copies, and nothing it moves: there is nothing to bridge, and the material is left as written.
	{
		FIRRun CopiesOnly;
		Lower(CopiesOnly, TEXT(
			"#pragma material(Substrate = Bridge)\n"
			"export void M_Bridge(inout material m)\n"
			"{\n"
			"    m.Normal = float3(0, 0, 1);\n"
			"    m.Opacity = 1.0;\n"
			"}\n"), OnOptions);
		if (TestTrue(FString::Printf(TEXT("a material of copies builds (%s)"), *CopiesOnly.ErrorText()), CopiesOnly.Succeeded()))
		{
			TestEqual(TEXT("...and is not bridged"), SinkInputs(CopiesOnly), FString(TEXT("Normal,Opacity")));
		}
	}

	// Legacy is the default, and with it nothing moves whatever the project says.
	FIRRun Legacy;
	Lower(Legacy, FString(Text).Replace(TEXT("#pragma material(Substrate = Bridge)\n"), TEXT("")), OnOptions);
	if (TestTrue(TEXT("Legacy builds"), Legacy.Succeeded()))
	{
		TestEqual(TEXT("Legacy: nothing moves"), SinkInputs(Legacy), FString(TEXT("BaseColor,Opacity,Roughness")));
	}

	ExpectError(*this, TEXT("Native with Substrate off"),
		TEXT("#pragma material(Substrate = Native)\nexport void M_Native(inout material m)\n{\n    m.FrontMaterial = Substrate.Slab(Roughness = 0.4);\n}\n"),
		TEXT("DSH4382"));
	ExpectError(*this, TEXT("an unknown mode"),
		TEXT("#pragma material(Substrate = Sometimes)\nexport void M_Mode(inout material m)\n{\n    m.Opacity = 1.0;\n}\n"),
		TEXT("DSH7232"));
	return true;
}

// ---------------------------------------------------------------------------------------------
// S5: the builder
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSubstrateSugarBuilderTest,
	"DreamShader.Lang2.SubstrateSugar.Builder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSubstrateSugarBuilderTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::SubstrateSugarTests;

	ExpectSame(*this, TEXT("members are the call's arguments"),
		TEXT("    Substrate S = Substrate.Slab();\n"
		     "    S.Roughness = Rough;\n"
		     "    S.BaseColor = Albedo;\n"
		     "    S.Metallic = Metal;\n"
		     "    S.Haziness = Haze;\n"
		     "    m.FrontMaterial = S;\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab(Roughness = Rough, BaseColor = Albedo, Metallic = Metal, Haziness = Haze);\n"));
	ExpectSame(*this, TEXT("any order"),
		TEXT("    Substrate S = Substrate.Slab();\n"
		     "    S.Haziness = Haze;\n"
		     "    S.Metallic = Metal;\n"
		     "    S.BaseColor = Albedo;\n"
		     "    S.Roughness = Rough;\n"
		     "    m.FrontMaterial = S;\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab(Roughness = Rough, BaseColor = Albedo, Metallic = Metal, Haziness = Haze);\n"));
	ExpectSame(*this, TEXT("the last write wins, and a read is what was written"),
		TEXT("    Substrate S = Substrate.Slab();\n"
		     "    S.Roughness = 0.9;\n"
		     "    S.Roughness = Rough;\n"
		     "    S.FuzzAmount = S.Roughness;\n"
		     "    m.FrontMaterial = S;\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab(Roughness = Rough, FuzzAmount = Rough);\n"));
	ExpectSame(*this, TEXT("a compound write starts from what the member holds"),
		TEXT("    Substrate S = Substrate.Slab();\n"
		     "    S.Roughness = Rough;\n"
		     "    S.Roughness *= Blend;\n"
		     "    m.FrontMaterial = S;\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab(Roughness = Rough * Blend);\n"));
	ExpectSame(*this, TEXT("a builder nothing was written to is the node without arguments"),
		TEXT("    Substrate S = Substrate.Slab();\n    m.FrontMaterial = S + A;\n"),
		TEXT("    m.FrontMaterial = Substrate.Slab() + A;\n"));

	// The node is made once, whichever arm takes the value first.
	{
		// Both arms read A or B as well, so nothing here is dead and the passes leave all three slabs standing.
		const FString BothArms = Source(TEXT(
			"    Substrate S = Substrate.Slab();\n"
			"    S.Roughness = 0.33;\n"
			"    Substrate Front = A;\n"
			"    if (Blend > 0.5) { Front = S + B; } else { Front = S + A; }\n"
			"    m.FrontMaterial = Front;\n"));

		FIRRun Run;
		Lower(Run, BothArms);
		if (TestTrue(FString::Printf(TEXT("a builder taken in both arms builds (%s)"), *Run.ErrorText()), Run.Succeeded()))
		{
			// A, B and S. A builder made once per arm would be a fourth, and dedupe is what would have hidden it --
			// so the count is taken before the passes as well.
			TestEqual(TEXT("three slabs after the passes"), CountNodes(Run.Module->Products[0].Graph, EIROp::Reflected, TEXT("Slab")), 3);
		}

		FIRRunOptions NoPasses;
		NoPasses.bRunPasses = false;
		FIRRun Raw;
		Lower(Raw, BothArms, NoPasses);
		if (TestTrue(TEXT("...and before them"), Raw.Module.IsValid() && Raw.Errors.Num() == 0))
		{
			TestEqual(TEXT("three slabs before the passes"), CountNodes(Raw.Module->Products[0].Graph, EIROp::Reflected, TEXT("Slab")), 3);
		}
	}

	ExpectError(*this, TEXT("a write after the value was taken"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    m.FrontMaterial = S;\n    S.Roughness = Rough;\n")), TEXT("DSH5297"));
	ExpectError(*this, TEXT("a member after the local was assigned whole"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    S.Roughness = Rough;\n    S = A;\n    m.Opacity = S.Roughness;\n    m.FrontMaterial = S;\n")), TEXT("DSH5297"));
	ExpectError(*this, TEXT("a write under an if the declaration is outside of"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    if (Blend > 0.5) { S.Roughness = Rough; }\n    m.FrontMaterial = S;\n")), TEXT("DSH5298"));
	ExpectError(*this, TEXT("no such pin"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    S.Shininess = Rough;\n    m.FrontMaterial = S;\n")), TEXT("DSH5299"));
	ExpectError(*this, TEXT("a read of a member nothing wrote"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    m.Opacity = S.Roughness;\n    m.FrontMaterial = S;\n")), TEXT("DSH5299"));
	ExpectError(*this, TEXT("a compound write to a member nothing wrote"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    S.Roughness *= Blend;\n    m.FrontMaterial = S;\n")), TEXT("DSH5299"));
	ExpectError(*this, TEXT("two names for the same pins, across statements"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    S.BaseColor = Albedo;\n    S.F0 = Albedo;\n    m.FrontMaterial = S;\n")), TEXT("DSH5295"));
	ExpectError(*this, TEXT("Haziness and no Roughness by the first use"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    S.Haziness = Haze;\n    m.FrontMaterial = S;\n")), TEXT("DSH5296"));
	ExpectError(*this, TEXT("one component of a pin"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    S.DiffuseAlbedo = Albedo;\n    S.DiffuseAlbedo.r = 0.5;\n    m.FrontMaterial = S;\n")), TEXT("DSH4229"));
	ExpectError(*this, TEXT("a loop that comes round to a write after the use"),
		Source(TEXT("    Substrate S = Substrate.Slab();\n    Substrate Stack = A;\n    for (int i = 0; i < 2; ++i) { S.Roughness = i * 0.5; Stack = Stack + S; }\n    m.FrontMaterial = Stack;\n")), TEXT("DSH4383"));

	// A call with arguments is a finished node, as it always was: it has no members to write.
	ExpectError(*this, TEXT("a call with arguments is no builder"),
		Source(TEXT("    Substrate S = Substrate.Slab(Roughness = Rough);\n    S.Roughness = 0.1;\n    m.FrontMaterial = S;\n")), TEXT("DSH"));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Read back: the decompiler writes the sugar, and the text builds the graph it was read from
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderSubstrateSugarReadBackTest,
	"DreamShader.Lang2.SubstrateSugar.ReadBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderSubstrateSugarReadBackTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::SubstrateSugarTests;
	using namespace UE::DreamShader::Lang;

	const FString Text = Source(TEXT(
		"    Substrate Paint = Substrate.Slab(BaseColor = Albedo, Metallic = Metal, Roughness = Rough, Haziness = Haze);\n"
		"    Substrate Glass = Substrate.Slab(Roughness = 0.05, Transmittance = Albedo, Thickness = 0.2);\n"
		"    m.FrontMaterial = lerp(Paint + A * Blend, Glass, Blend);\n"));

	FIRRun Built;
	Lower(Built, Text);
	if (!TestTrue(FString::Printf(TEXT("the source builds (%s)"), *Built.ErrorText()), Built.Succeeded()))
	{
		return false;
	}

	FIRToAstOptions AstOptions;
	FLangDiagnosticSink Sink(TEXT("Sugar.dss"));
	FString Decompiled;
	const bool bDecompiled = UE::DreamShader::Editor::Private::Tests::DecompileDreamShaderIRToText(*Built.Module, AstOptions, Decompiled, Sink);
	if (!TestTrue(TEXT("the module decompiles"), bDecompiled))
	{
		return false;
	}

	TestTrue(TEXT("BaseColor is written as the argument it was"), Decompiled.Contains(TEXT("BaseColor = Albedo")));
	TestTrue(TEXT("Metallic too"), Decompiled.Contains(TEXT("Metallic = Metal")));
	TestTrue(TEXT("Haziness too"), Decompiled.Contains(TEXT("Haziness = Haze")));
	TestTrue(TEXT("Transmittance and its Thickness too"), Decompiled.Contains(TEXT("Transmittance = Albedo")) && Decompiled.Contains(TEXT("Thickness = 0.2")));
	TestFalse(TEXT("no conversion node is left to see"), Decompiled.Contains(TEXT("MetalnessToDiffuseAlbedoF0")) || Decompiled.Contains(TEXT("HazinessToSecondaryRoughness")) || Decompiled.Contains(TEXT("TransmittanceToMFP")));
	TestTrue(TEXT("HorizontalMixing is lerp"), Decompiled.Contains(TEXT("lerp(")));
	TestFalse(TEXT("...and Add and Weight are operators"), Decompiled.Contains(TEXT("Substrate.Add")) || Decompiled.Contains(TEXT("Substrate.Weight")));
	AddInfo(Decompiled);

	FIRRun Again;
	Lower(Again, Decompiled);
	if (TestTrue(FString::Printf(TEXT("the decompiled text builds (%s)"), *Again.ErrorText()), Again.Succeeded()))
	{
		FIRCompareOptions CompareOptions;
		FString Difference;
		TestTrue(FString::Printf(TEXT("...to the graph it was read from (%s)"), *Difference),
			AreDreamShaderIRModulesEquivalent(*Built.Module, *Again.Module, CompareOptions, Difference));
	}

	// A conversion node something else reads as well is no sugar: it stays a node, and so do the pins it feeds.
	const FString Shared = Source(TEXT(
		"    Substrate Paint = Substrate.Slab(BaseColor = Albedo, Metallic = Metal, Roughness = Rough);\n"
		"    m.FrontMaterial = Paint;\n"
		"    m.EmissiveColor = Substrate.MetalnessToDiffuseAlbedoF0(BaseColor = Albedo, Metallic = Metal).F0;\n"));
	FIRRun SharedBuilt;
	Lower(SharedBuilt, Shared);
	if (TestTrue(FString::Printf(TEXT("the shared source builds (%s)"), *SharedBuilt.ErrorText()), SharedBuilt.Succeeded()))
	{
		FLangDiagnosticSink SharedSink(TEXT("Sugar.dss"));
		FString SharedText;
		if (TestTrue(TEXT("...and decompiles"), UE::DreamShader::Editor::Private::Tests::DecompileDreamShaderIRToText(*SharedBuilt.Module, AstOptions, SharedText, SharedSink)))
		{
			TestTrue(TEXT("a shared conversion node stays a node"), SharedText.Contains(TEXT("MetalnessToDiffuseAlbedoF0")));
			FIRRun SharedAgain;
			Lower(SharedAgain, SharedText);
			FIRCompareOptions CompareOptions;
			FString Difference;
			TestTrue(FString::Printf(TEXT("...and the text still builds the same graph (%s | %s)"), *SharedAgain.ErrorText(), *Difference),
				SharedAgain.Succeeded() && AreDreamShaderIRModulesEquivalent(*SharedBuilt.Module, *SharedAgain.Module, CompareOptions, Difference));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
