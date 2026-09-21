// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.IRLayout.* -- LayoutDreamShaderIRGraph (IR/IRLayout.h), the engine-free graph layout.
//
// A layout has no single right answer to assert, so these tests assert what every answer has to have: the same result
// twice, no two nodes on top of each other, no wire running right to left, bands in the order of the text, a hint that
// is obeyed to the unit, a region box that holds its nodes. Each property is checked for every style it is a property
// of; what Blocks alone promises -- no wire from one box to another, a reroute for every value that crosses -- has a
// test of its own.
//
// Core only: sources are lowered with the hand-built test catalog through the shared IR runner, and one graph is built
// by hand to be big.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRLayout.h"

#include "HAL/PlatformTime.h"

// This file's own namespace: the module builds as a unity blob.
namespace UE::DreamShader::Editor::Private::IRLayoutTests
{
	using namespace UE::DreamShader::IR;

	using FIRRun = UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRun;

	inline bool Lower(FAutomationTestBase& Test, FIRRun& Run, const TCHAR* Text)
	{
		UE::DreamShader::Editor::Private::Tests::FDreamShaderIRRunOptions Options;
		UE::DreamShader::Editor::Private::Tests::RunDreamShaderIRPipeline(TEXT("Layout.dss"), Text, Options, Run);
		if (!Run.Module.IsValid() || !Run.Succeeded() || Run.Module->Products.Num() == 0)
		{
			Test.AddError(FString::Printf(TEXT("a layout fixture does not build: %s"), *Run.ErrorText()));
			return false;
		}
		return true;
	}

	inline FIRLayoutResult LayOut(const FIRGraph& Graph, const EIRLayoutStyle Style)
	{
		FIRLayoutOptions Options;
		Options.Style = Style;
		FIRLayoutResult Result;
		LayoutDreamShaderIRGraph(Graph, Options, Result);
		return Result;
	}

	inline int32 FindNamed(const FIRGraph& Graph, const TCHAR* Name)
	{
		return FindDreamShaderIRLayoutOwner(Graph, Name);
	}

	/** A few statements, a shared value, a helper that inlines, two sink attributes. */
	static const TCHAR* const GSource = TEXT(
		"uniform float A = 1;\n"
		"uniform float B = 2;\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"float Half(float x) { return x * 0.5; }\n"
		"export void M_Layout(inout material m)\n"
		"{\n"
		"    float Sum = A + B;\n"
		"    float Scaled = Half(Sum) * 3.0;\n"
		"    float3 Color = Tint * Scaled + Sum;\n"
		"    m.BaseColor = Color;\n"
		"    m.Roughness = saturate(Scaled - A);\n"
		"}\n");

	/** The two styles that draw the whole graph as one picture: every wire is a wire, and none runs right to left. */
	static const EIRLayoutStyle GStyles[] = { EIRLayoutStyle::SourceBands, EIRLayoutStyle::Layered };
	static const EIRLayoutStyle GAllStyles[] = { EIRLayoutStyle::Blocks, EIRLayoutStyle::SourceBands, EIRLayoutStyle::Layered };

	/** Regions, statements outside them, a value one region makes and another reads, and a constant both use. */
	static const TCHAR* const GBlockSource = TEXT(
		"uniform float Gain = 2;\n"
		"uniform float3 Tint = float3(1, 0.5, 0.25);\n"
		"export void M_Blocks(inout material m)\n"
		"{\n"
		"    #pragma region Base\n"
		"    float3 Albedo = Tint * 0.5;\n"
		"    #pragma region Inner\n"
		"    float3 Dimmed = Albedo * Gain;\n"
		"    #pragma endregion\n"
		"    float3 Warm = Dimmed + Albedo;\n"
		"    #pragma endregion\n"
		"    #pragma region Glow\n"
		"    float3 Lit = Warm + Gain;\n"
		"    float Half = Gain * 0.5;\n"
		"    #pragma endregion\n"
		"    float3 Final = Lit * Half;\n"
		"    m.EmissiveColor = Final;\n"
		"    m.Opacity = Half;\n"
		"}\n");

	struct FRect
	{
		int32 X0 = 0;
		int32 Y0 = 0;
		int32 X1 = 0;
		int32 Y1 = 0;

		bool Overlaps(const FRect& Other) const
		{
			return X0 < Other.X1 && Other.X0 < X1 && Y0 < Other.Y1 && Other.Y0 < Y1;
		}
		bool Holds(const FRect& Other) const
		{
			return X0 <= Other.X0 && Y0 <= Other.Y0 && Other.X1 <= X1 && Other.Y1 <= Y1;
		}
	};

	inline FRect RectOf(const FIntPoint& Position, const FIRLayoutNodeSize& Size)
	{
		return FRect{ Position.X, Position.Y, Position.X + Size.Width, Position.Y + Size.Height };
	}
}

// ---------------------------------------------------------------------------------------------
// The same twice
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRLayoutDeterministicTest,
	"DreamShader.Lang2.IRLayout.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRLayoutDeterministicTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRLayoutTests;

	// Two separate lowerings, not one graph laid out twice: node numbering is the builder's, and the layout must not
	// depend on anything but the graph.
	FIRRun First;
	FIRRun Second;
	if (!Lower(*this, First, GSource) || !Lower(*this, Second, GSource))
	{
		return false;
	}

	for (const EIRLayoutStyle Style : GAllStyles)
	{
		const FIRGraph& GraphA = First.Module->Products[0].Graph;
		const FIRGraph& GraphB = Second.Module->Products[0].Graph;
		const FString A = DumpDreamShaderIRLayoutJson(GraphA, LayOut(GraphA, Style), Style);
		const FString B = DumpDreamShaderIRLayoutJson(GraphB, LayOut(GraphB, Style), Style);
		TestEqual(FString::Printf(TEXT("%s: two runs give the same coordinates"), LexToString(Style)), A, B);
		TestTrue(FString::Printf(TEXT("%s: the dump names its schema"), LexToString(Style)), A.Contains(TEXT("\"dreamshader-ir-layout\"")));
	}

	EIRLayoutStyle Parsed = EIRLayoutStyle::SourceBands;
	TestTrue(TEXT("'layered' parses, ignoring case"), TryParseIRLayoutStyle(TEXT("layered"), Parsed) && Parsed == EIRLayoutStyle::Layered);
	TestTrue(TEXT("'SourceBands' parses"), TryParseIRLayoutStyle(TEXT("SourceBands"), Parsed) && Parsed == EIRLayoutStyle::SourceBands);
	TestTrue(TEXT("'blocks' parses"), TryParseIRLayoutStyle(TEXT("blocks"), Parsed) && Parsed == EIRLayoutStyle::Blocks);
	TestFalse(TEXT("'Classic' is the 1.x layout, which does not run on the IR"), TryParseIRLayoutStyle(TEXT("Classic"), Parsed));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Geometry every style owes
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRLayoutGeometryTest,
	"DreamShader.Lang2.IRLayout.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRLayoutGeometryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRLayoutTests;

	FIRRun Run;
	if (!Lower(*this, Run, GSource))
	{
		return false;
	}
	const FIRGraph& Graph = Run.Module->Products[0].Graph;

	for (const EIRLayoutStyle Style : GStyles)
	{
		const FIRLayoutResult Layout = LayOut(Graph, Style);
		const TCHAR* StyleName = LexToString(Style);

		TestEqual(FString::Printf(TEXT("%s: one position per node"), StyleName), Layout.Positions.Num(), Graph.Nodes.Num());

		int32 PlacedCount = 0;
		for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
		{
			PlacedCount += Layout.Placed[Node] ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s: a pruned graph places every node"), StyleName), PlacedCount, Graph.Nodes.Num());

		// No wire runs right to left: what a node reads ends before the node begins.
		TArray<FIRValue> Values;
		for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
		{
			if (Node == Graph.Sink)
			{
				continue;
			}
			Values.Reset();
			FIRGraph::CollectInputValues(Graph.Nodes[Node], Values);
			for (const FIRValue& Value : Values)
			{
				if (!Graph.Nodes.IsValidIndex(Value.Node))
				{
					continue;
				}
				const int32 ReadRight = Layout.Positions[Value.Node].X + Layout.Sizes[Value.Node].Width;
				TestTrue(
					FString::Printf(TEXT("%s: node %d reads node %d from its left (%d <= %d)"), StyleName, Node, Value.Node, ReadRight, Layout.Positions[Node].X),
					ReadRight <= Layout.Positions[Node].X);
			}
		}

		// No two nodes on top of each other.
		for (int32 A = 0; A < Graph.Nodes.Num(); ++A)
		{
			for (int32 B = A + 1; B < Graph.Nodes.Num(); ++B)
			{
				if (A == Graph.Sink || B == Graph.Sink)
				{
					continue;
				}
				const bool bApartX = Layout.Positions[A].X + Layout.Sizes[A].Width <= Layout.Positions[B].X
					|| Layout.Positions[B].X + Layout.Sizes[B].Width <= Layout.Positions[A].X;
				const bool bApartY = Layout.Positions[A].Y + Layout.Sizes[A].Height <= Layout.Positions[B].Y
					|| Layout.Positions[B].Y + Layout.Sizes[B].Height <= Layout.Positions[A].Y;
				TestTrue(FString::Printf(TEXT("%s: nodes %d and %d do not overlap"), StyleName, A, B), bApartX || bApartY);
			}
		}

		// The material's own node: right of everything.
		TestTrue(FString::Printf(TEXT("%s: the material node is right of the graph"), StyleName), Layout.RootPosition.X >= Layout.BoundsMax.X);

		// Everything on the grid.
		FIRLayoutOptions Defaults;
		for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
		{
			TestEqual(FString::Printf(TEXT("%s: node %d is on the grid in x"), StyleName, Node), Layout.Positions[Node].X % Defaults.GridSize, 0);
			TestEqual(FString::Printf(TEXT("%s: node %d is on the grid in y"), StyleName, Node), Layout.Positions[Node].Y % Defaults.GridSize, 0);
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Blocks: a page of boxes, and no wire between two of them
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRLayoutBlocksTest,
	"DreamShader.Lang2.IRLayout.Blocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRLayoutBlocksTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRLayoutTests;

	FIRRun Run;
	if (!Lower(*this, Run, GBlockSource))
	{
		return false;
	}
	const FIRGraph& Graph = Run.Module->Products[0].Graph;
	const FIRLayoutResult Layout = LayOut(Graph, EIRLayoutStyle::Blocks);

	// ----- every node is in a block, and a statement's block is its region's
	for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
	{
		if (Node != Graph.Sink)
		{
			TestTrue(FString::Printf(TEXT("node %d is in a block"), Node), Layout.BlockList.IsValidIndex(Layout.Blocks[Node]));
		}
	}
	const int32 Albedo = FindNamed(Graph, TEXT("Albedo"));
	const int32 Dimmed = FindNamed(Graph, TEXT("Dimmed"));
	const int32 Warm = FindNamed(Graph, TEXT("Warm"));
	const int32 Lit = FindNamed(Graph, TEXT("Lit"));
	const int32 Half = FindNamed(Graph, TEXT("Half"));
	const int32 Final = FindNamed(Graph, TEXT("Final"));
	if (!TestTrue(TEXT("the named statements are found"),
		Graph.Nodes.IsValidIndex(Albedo) && Graph.Nodes.IsValidIndex(Dimmed) && Graph.Nodes.IsValidIndex(Warm)
		&& Graph.Nodes.IsValidIndex(Lit) && Graph.Nodes.IsValidIndex(Half) && Graph.Nodes.IsValidIndex(Final)))
	{
		return false;
	}
	TestEqual(TEXT("two statements of one region share a block"), Layout.Blocks[Lit], Layout.Blocks[Half]);
	TestNotEqual(TEXT("a nested region is a block of its own"), Layout.Blocks[Albedo], Layout.Blocks[Dimmed]);
	TestNotEqual(TEXT("...and what follows it in the outer region is another"), Layout.Blocks[Albedo], Layout.Blocks[Warm]);
	TestNotEqual(TEXT("two regions are two blocks"), Layout.Blocks[Warm], Layout.Blocks[Lit]);
	TestNotEqual(TEXT("what is outside every region is a block too"), Layout.Blocks[Final], Layout.Blocks[Lit]);
	TestEqual(TEXT("a region's only block carries its name"), Layout.BlockList[Layout.Blocks[Lit]].Title, FString(TEXT("Glow")));
	TestEqual(TEXT("a run of statements is named after what it defines"), Layout.BlockList[Layout.Blocks[Final]].Title, FString(TEXT("Final")));

	// ----- no wire leaves a block: every such read has a reroute in the reader's block, a constant its copy
	TArray<FIRValue> Values;
	int32 CrossingReads = 0;
	for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
	{
		if (Node == Graph.Sink)
		{
			continue;
		}
		Values.Reset();
		FIRGraph::CollectInputValues(Graph.Nodes[Node], Values);
		for (const FIRValue& Value : Values)
		{
			if (!Graph.Nodes.IsValidIndex(Value.Node))
			{
				continue;
			}
			if (Layout.Blocks[Value.Node] == Layout.Blocks[Node])
			{
				const int32 ReadRight = Layout.Positions[Value.Node].X + Layout.Sizes[Value.Node].Width;
				TestTrue(FString::Printf(TEXT("inside a block node %d reads node %d from its left"), Node, Value.Node), ReadRight <= Layout.Positions[Node].X);
				continue;
			}

			++CrossingReads;
			const FIRLayoutBridge* Bridge = Layout.Bridges.FindByPredicate([&Value](const FIRLayoutBridge& Candidate) { return Candidate.Source == Value; });
			if (!TestNotNull(FString::Printf(TEXT("the value of node %d, read from another block, is bridged"), Value.Node), Bridge))
			{
				continue;
			}
			const FIRLayoutBridgeUse* Use = Bridge->Uses.FindByPredicate([&Layout, Node](const FIRLayoutBridgeUse& Candidate) { return Candidate.Block == Layout.Blocks[Node]; });
			if (TestNotNull(FString::Printf(TEXT("...with a usage in the block of its reader %d"), Node), Use))
			{
				TestTrue(TEXT("...that names the reader"), Use->Consumers.Contains(Node));
				TestTrue(TEXT("...and stands to its left"), Use->Position.X + Use->Size.Width <= Layout.Positions[Node].X);
			}
			TestEqual(FString::Printf(TEXT("a constant is repeated, anything else is rerouted (node %d)"), Value.Node),
				Bridge->bClone, Graph.Nodes[Value.Node].Op == EIROp::Constant);
			if (!Bridge->bClone)
			{
				TestEqual(TEXT("the declaration is in the block of its value"), Bridge->SourceBlock, Layout.Blocks[Value.Node]);
				TestTrue(TEXT("...to its right"), Layout.Positions[Value.Node].X + Layout.Sizes[Value.Node].Width <= Bridge->DeclarationPosition.X);
			}
		}
	}
	TestTrue(TEXT("the fixture has reads that cross blocks"), CrossingReads >= 4);

	const FIRLayoutBridge* AlbedoBridge = Layout.Bridges.FindByPredicate([Albedo](const FIRLayoutBridge& Candidate) { return Candidate.Source.Node == Albedo; });
	if (TestNotNull(TEXT("Albedo leaves its block"), AlbedoBridge))
	{
		TestEqual(TEXT("...under the variable's name"), AlbedoBridge->Name, FString(TEXT("DS_Albedo")));
		TestEqual(TEXT("...into the two blocks that read it"), AlbedoBridge->Uses.Num(), 2);
	}
	TSet<FString> Names;
	for (const FIRLayoutBridge& Bridge : Layout.Bridges)
	{
		TestFalse(FString::Printf(TEXT("the reroute name '%s' is taken once"), *Bridge.Name), Names.Contains(Bridge.Name));
		Names.Add(Bridge.Name);
	}

	// ----- what drives the material leaves its block through the pair in front of the output
	TestEqual(TEXT("one output route per sink input"), Layout.OutputRoutes.Num(), 2);
	for (const FIRLayoutOutputRoute& Route : Layout.OutputRoutes)
	{
		TestEqual(TEXT("the route ends at the sink"), Route.Consumer, Graph.Sink);
		TestTrue(TEXT("its declaration is right of the value"), Layout.Positions[Route.Source.Node].X + Layout.Sizes[Route.Source.Node].Width <= Route.DeclarationPosition.X);
		TestTrue(TEXT("its usage is right of the page"), Route.UsagePosition.X >= Layout.BoundsMax.X);
		TestTrue(TEXT("...and left of the material's node"), Route.UsagePosition.X + Route.UsageSize.Width <= Layout.RootPosition.X);
	}

	// ----- nothing on top of anything: nodes, reroutes, and the boxes of two blocks
	TArray<FRect> Things;
	for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
	{
		if (Node != Graph.Sink)
		{
			Things.Add(RectOf(Layout.Positions[Node], Layout.Sizes[Node]));
		}
	}
	for (const FIRLayoutBridge& Bridge : Layout.Bridges)
	{
		if (!Bridge.bClone)
		{
			Things.Add(RectOf(Bridge.DeclarationPosition, Bridge.DeclarationSize));
		}
		for (const FIRLayoutBridgeUse& Use : Bridge.Uses)
		{
			Things.Add(RectOf(Use.Position, Use.Size));
		}
	}
	for (const FIRLayoutOutputRoute& Route : Layout.OutputRoutes)
	{
		Things.Add(RectOf(Route.DeclarationPosition, Route.DeclarationSize));
		Things.Add(RectOf(Route.UsagePosition, Route.UsageSize));
	}
	for (int32 A = 0; A < Things.Num(); ++A)
	{
		for (int32 B = A + 1; B < Things.Num(); ++B)
		{
			TestFalse(FString::Printf(TEXT("things %d and %d do not overlap"), A, B), Things[A].Overlaps(Things[B]));
		}
	}

	const FIRLayoutComment* BaseBox = Layout.Comments.FindByPredicate([](const FIRLayoutComment& Comment) { return Comment.Title == TEXT("Base"); });
	const FIRLayoutComment* InnerBox = Layout.Comments.FindByPredicate([](const FIRLayoutComment& Comment) { return Comment.Title == TEXT("Inner"); });
	const FIRLayoutComment* GlowBox = Layout.Comments.FindByPredicate([](const FIRLayoutComment& Comment) { return Comment.Title == TEXT("Glow"); });
	if (TestNotNull(TEXT("the Base box"), BaseBox) && TestNotNull(TEXT("the Inner box"), InnerBox) && TestNotNull(TEXT("the Glow box"), GlowBox))
	{
		const FRect Base{ BaseBox->X, BaseBox->Y, BaseBox->X + BaseBox->Width, BaseBox->Y + BaseBox->Height };
		const FRect Inner{ InnerBox->X, InnerBox->Y, InnerBox->X + InnerBox->Width, InnerBox->Y + InnerBox->Height };
		const FRect Glow{ GlowBox->X, GlowBox->Y, GlowBox->X + GlowBox->Width, GlowBox->Y + GlowBox->Height };
		TestTrue(TEXT("a nested region lies inside its parent"), Base.Holds(Inner));
		TestFalse(TEXT("two regions do not overlap"), Base.Overlaps(Glow));
		TestTrue(TEXT("a region's box is the layout's own"), BaseBox->bGenerated && GlowBox->bGenerated);
		TestTrue(TEXT("the inner box is drawn after the outer"), InnerBox->Depth > BaseBox->Depth);
	}
	for (int32 A = 0; A < Layout.BlockList.Num(); ++A)
	{
		for (int32 B = A + 1; B < Layout.BlockList.Num(); ++B)
		{
			const FIRLayoutBlock& Left = Layout.BlockList[A];
			const FIRLayoutBlock& Right = Layout.BlockList[B];
			const FRect BoxA{ Left.X, Left.Y, Left.X + Left.Width, Left.Y + Left.Height };
			const FRect BoxB{ Right.X, Right.Y, Right.X + Right.Width, Right.Y + Right.Height };
			TestFalse(FString::Printf(TEXT("the boxes of blocks '%s' and '%s' do not overlap"), *Left.Title, *Right.Title), BoxA.Overlaps(BoxB));
		}
	}

	// ----- a long run of statements is cut
	{
		FIRLayoutOptions Small;
		Small.Style = EIRLayoutStyle::Blocks;
		Small.BlockMaxNodes = 1;
		FIRLayoutResult Cut;
		LayoutDreamShaderIRGraph(Graph, Small, Cut);
		TestTrue(TEXT("a smaller BlockMaxNodes makes more blocks"), Cut.BlockList.Num() > Layout.BlockList.Num());
	}

	// ----- a function's outputs are a block of their own, fed by the pairs in front of them
	{
		FIRRun FunctionRun;
		if (!Lower(*this, FunctionRun, TEXT(
			"export void MF_Blocks(float A, float B, out float First, out float Second)\n"
			"{\n"
			"    #pragma region Maths\n"
			"    float Early = A * 2.0;\n"
			"    float Late = Early + B;\n"
			"    #pragma endregion\n"
			"    First = Early;\n"
			"    Second = Late;\n"
			"}\n")))
		{
			return false;
		}
		const FIRGraph& FunctionGraph = FunctionRun.Module->Products[0].Graph;
		const FIRLayoutResult FunctionLayout = LayOut(FunctionGraph, EIRLayoutStyle::Blocks);
		int32 OutputBlocks = 0;
		for (const FIRLayoutBlock& Block : FunctionLayout.BlockList)
		{
			OutputBlocks += Block.bOutputs ? 1 : 0;
		}
		TestEqual(TEXT("one block holds the outputs"), OutputBlocks, 1);
		TestEqual(TEXT("one route per output"), FunctionLayout.OutputRoutes.Num(), 2);
		for (const FIRLayoutOutputRoute& Route : FunctionLayout.OutputRoutes)
		{
			TestEqual(TEXT("the route ends at a function output"), static_cast<int32>(FunctionGraph.Nodes[Route.Consumer].Op), static_cast<int32>(EIROp::FunctionOutput));
			TestTrue(TEXT("its usage is left of the output"), Route.UsagePosition.X + Route.UsageSize.Width <= FunctionLayout.Positions[Route.Consumer].X);
			TestNotEqual(TEXT("the value and the output are in two blocks"), FunctionLayout.Blocks[Route.Source.Node], FunctionLayout.Blocks[Route.Consumer]);
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Source bands
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRLayoutBandsTest,
	"DreamShader.Lang2.IRLayout.BandsFollowSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRLayoutBandsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRLayoutTests;

	FIRRun Run;
	if (!Lower(*this, Run, GSource))
	{
		return false;
	}
	const FIRGraph& Graph = Run.Module->Products[0].Graph;
	const FIRLayoutResult Layout = LayOut(Graph, EIRLayoutStyle::SourceBands);

	const int32 Sum = FindNamed(Graph, TEXT("Sum"));
	const int32 Scaled = FindNamed(Graph, TEXT("Scaled"));
	const int32 Color = FindNamed(Graph, TEXT("Color"));
	if (!TestTrue(TEXT("the three named values are nodes"), Graph.Nodes.IsValidIndex(Sum) && Graph.Nodes.IsValidIndex(Scaled) && Graph.Nodes.IsValidIndex(Color)))
	{
		return false;
	}

	TestTrue(TEXT("at least one band per named statement"), Layout.BandCount >= 3);
	TestTrue(TEXT("Sum's band is above Scaled's"), Layout.Bands[Sum] < Layout.Bands[Scaled]);
	TestTrue(TEXT("Scaled's band is above Color's"), Layout.Bands[Scaled] < Layout.Bands[Color]);

	// Bands do not interleave: everything of band k ends above everything of band k + 1.
	TArray<int32> BandTop;
	TArray<int32> BandBottom;
	BandTop.Init(MAX_int32, Layout.BandCount);
	BandBottom.Init(MIN_int32, Layout.BandCount);
	for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
	{
		const int32 Band = Layout.Bands[Node];
		if (Band == INDEX_NONE)
		{
			continue;
		}
		BandTop[Band] = FMath::Min(BandTop[Band], Layout.Positions[Node].Y);
		BandBottom[Band] = FMath::Max(BandBottom[Band], Layout.Positions[Node].Y + Layout.Sizes[Node].Height);
	}
	for (int32 Band = 0; Band + 1 < Layout.BandCount; ++Band)
	{
		TestTrue(
			FString::Printf(TEXT("band %d ends (%d) above where band %d begins (%d)"), Band, BandBottom[Band], Band + 1, BandTop[Band + 1]),
			BandBottom[Band] <= BandTop[Band + 1]);
	}

	// A uniform belongs to the first statement that needs it: A is read by Sum first.
	int32 ParameterA = INDEX_NONE;
	for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
	{
		if (Graph.Nodes[Node].Op == EIROp::Parameter && Graph.Nodes[Node].DebugName == TEXT("A"))
		{
			ParameterA = Node;
		}
	}
	if (Graph.Nodes.IsValidIndex(ParameterA))
	{
		TestEqual(TEXT("the uniform A sits in the band of its first reader"), Layout.Bands[ParameterA], Layout.Bands[Sum]);
	}

	// The layered style has no bands.
	const FIRLayoutResult Layered = LayOut(Graph, EIRLayoutStyle::Layered);
	TestEqual(TEXT("Layered reports no bands"), Layered.BandCount, 0);

	// A function's outputs are made when the body ends and carry the declaration's position. They are statements, and a
	// statement happens after what it reads: an output sits under the statement that computed its value, not above the
	// first one with a wire running back down.
	{
		FIRRun FunctionRun;
		if (!Lower(*this, FunctionRun, TEXT(
			"export void MF_Layout(float A, float B, out float First, out float Second)\n"
			"{\n"
			"    float Early = A * 2.0;\n"
			"    float Middle = Early + B;\n"
			"    float Late = Middle * Middle;\n"
			"    First = Early;\n"
			"    Second = Late;\n"
			"}\n")))
		{
			return false;
		}
		const FIRGraph& FunctionGraph = FunctionRun.Module->Products[0].Graph;
		const FIRLayoutResult FunctionLayout = LayOut(FunctionGraph, EIRLayoutStyle::SourceBands);

		int32 FirstOutput = INDEX_NONE;
		int32 SecondOutput = INDEX_NONE;
		for (const int32 Output : FunctionGraph.FunctionOutputs)
		{
			if (!FunctionGraph.Nodes.IsValidIndex(Output))
			{
				continue;
			}
			for (const FIRProperty& Property : FunctionGraph.Nodes[Output].Properties)
			{
				if (Property.Name == TEXT("OutputName"))
				{
					const FString OutputName = Property.Value.ToString();
					FirstOutput = OutputName.Contains(TEXT("First")) ? Output : FirstOutput;
					SecondOutput = OutputName.Contains(TEXT("Second")) ? Output : SecondOutput;
				}
			}
		}
		const int32 Early = FindNamed(FunctionGraph, TEXT("Early"));
		const int32 Late = FindNamed(FunctionGraph, TEXT("Late"));
		if (TestTrue(TEXT("the function's outputs and statements are found"),
			FunctionGraph.Nodes.IsValidIndex(FirstOutput) && FunctionGraph.Nodes.IsValidIndex(SecondOutput)
			&& FunctionGraph.Nodes.IsValidIndex(Early) && FunctionGraph.Nodes.IsValidIndex(Late)))
		{
			TestTrue(TEXT("an output is below the statement it reads"), FunctionLayout.Bands[FirstOutput] > FunctionLayout.Bands[Early]);
			TestTrue(TEXT("...and above a later statement it does not read"), FunctionLayout.Bands[FirstOutput] < FunctionLayout.Bands[Late]);
			TestTrue(TEXT("the last output is below the last statement"), FunctionLayout.Bands[SecondOutput] > FunctionLayout.Bands[Late]);
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Hints and regions
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRLayoutHintsAndRegionsTest,
	"DreamShader.Lang2.IRLayout.HintsAndRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRLayoutHintsAndRegionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRLayoutTests;

	FIRRun Run;
	if (!Lower(*this, Run, TEXT(
		"uniform float A = 1;\n"
		"uniform float B = 2;\n"
		"#pragma layout(Node, Var = \"Sum\", X = 1003, Y = -2005)\n"
		"#pragma layout(Comment, Name = \"Maths\", X = 10, Y = 20, W = 400, H = 200)\n"
		"export void M_Hints(inout material m)\n"
		"{\n"
		"    #pragma region Outer\n"
		"    float Sum = A + B;\n"
		"    #pragma region Inner\n"
		"    float Scaled = Sum * 2;\n"
		"    #pragma endregion\n"
		"    float Final = Scaled + 1;\n"
		"    #pragma endregion\n"
		"    m.Opacity = Final;\n"
		"}\n")))
	{
		return false;
	}
	const FIRGraph& Graph = Run.Module->Products[0].Graph;

	for (const EIRLayoutStyle Style : GStyles)
	{
		const FIRLayoutResult Layout = LayOut(Graph, Style);
		const TCHAR* StyleName = LexToString(Style);

		// A hinted position is the author's: exactly as written, off the grid and all.
		const int32 Sum = FindNamed(Graph, TEXT("Sum"));
		if (TestTrue(FString::Printf(TEXT("%s: Sum is a node"), StyleName), Graph.Nodes.IsValidIndex(Sum)))
		{
			TestTrue(FString::Printf(TEXT("%s: Sum is marked hinted"), StyleName), Layout.Hinted[Sum]);
			TestEqual(FString::Printf(TEXT("%s: Sum.x is the hint"), StyleName), Layout.Positions[Sum].X, 1003);
			TestEqual(FString::Printf(TEXT("%s: Sum.y is the hint"), StyleName), Layout.Positions[Sum].Y, -2005);
		}

		const FIRLayoutComment* Maths = Layout.Comments.FindByPredicate([](const FIRLayoutComment& Comment) { return Comment.Title == TEXT("Maths"); });
		const FIRLayoutComment* Outer = Layout.Comments.FindByPredicate([](const FIRLayoutComment& Comment) { return Comment.Title == TEXT("Outer"); });
		const FIRLayoutComment* Inner = Layout.Comments.FindByPredicate([](const FIRLayoutComment& Comment) { return Comment.Title == TEXT("Inner"); });

		if (TestNotNull(*FString::Printf(TEXT("%s: the author's comment box is passed through"), StyleName), Maths))
		{
			TestTrue(FString::Printf(TEXT("%s: with its own rectangle"), StyleName), Maths->X == 10 && Maths->Y == 20);
			TestTrue(FString::Printf(TEXT("%s: and its own size"), StyleName), Maths->Width == 400 && Maths->Height == 200);
			TestEqual(FString::Printf(TEXT("%s: and it is no region"), StyleName), Maths->Region, static_cast<int32>(INDEX_NONE));
		}

		if (TestNotNull(*FString::Printf(TEXT("%s: the outer region has a box"), StyleName), Outer)
			&& TestNotNull(*FString::Printf(TEXT("%s: the inner region has a box"), StyleName), Inner))
		{
			TestTrue(FString::Printf(TEXT("%s: the inner box is nested deeper"), StyleName), Inner->Depth > Outer->Depth);
			TestTrue(
				FString::Printf(TEXT("%s: the inner box lies inside the outer one"), StyleName),
				Inner->X >= Outer->X && Inner->Y >= Outer->Y
					&& Inner->X + Inner->Width <= Outer->X + Outer->Width
					&& Inner->Y + Inner->Height <= Outer->Y + Outer->Height);

			// Every node of a region lies inside its box.
			for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
			{
				const int32 Region = Graph.Nodes[Node].Region;
				if (!Graph.Regions.IsValidIndex(Region) || Node == Graph.Sink)
				{
					continue;
				}
				const FIRLayoutComment* Box = Graph.Regions[Region].Name == TEXT("Inner") ? Inner : Outer;
				TestTrue(
					FString::Printf(TEXT("%s: node %d lies inside the box of region '%s'"), StyleName, Node, *Graph.Regions[Region].Name),
					Layout.Positions[Node].X >= Box->X && Layout.Positions[Node].Y >= Box->Y
						&& Layout.Positions[Node].X + Layout.Sizes[Node].Width <= Box->X + Box->Width
						&& Layout.Positions[Node].Y + Layout.Sizes[Node].Height <= Box->Y + Box->Height);
			}
		}

		// Hints can be switched off, and then nothing is marked.
		FIRLayoutOptions NoHints;
		NoHints.Style = Style;
		NoHints.bApplyNodeHints = false;
		NoHints.bApplyCommentHints = false;
		NoHints.bRegionBoxes = false;
		FIRLayoutResult Plain;
		LayoutDreamShaderIRGraph(Graph, NoHints, Plain);
		TestFalse(FString::Printf(TEXT("%s: no node is hinted with hints off"), StyleName), Plain.Hinted.Contains(true));
		TestEqual(FString::Printf(TEXT("%s: and no box is made"), StyleName), Plain.Comments.Num(), 0);
	}

	const FString Svg = DumpDreamShaderIRLayoutSvg(Graph, LayOut(Graph, EIRLayoutStyle::SourceBands), TEXT("a <title> & more"));
	TestTrue(TEXT("the SVG is one document"), Svg.StartsWith(TEXT("<svg ")) && Svg.TrimEnd().EndsWith(TEXT("</svg>")));
	TestTrue(TEXT("the SVG escapes its text"), Svg.Contains(TEXT("a &lt;title&gt; &amp; more")));
	return true;
}

// ---------------------------------------------------------------------------------------------
// The owner rule, and a graph that is big
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderIRLayoutLargeGraphTest,
	"DreamShader.Lang2.IRLayout.LargeGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderIRLayoutLargeGraphTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Editor::Private::IRLayoutTests;

	// By hand, because no source is this shape: a chain 1200 long -- the depth that would overflow a recursive walk --
	// with a constant hanging off every link, 2401 nodes in all, ending in a function output.
	FIRGraph Graph;
	const auto AddNode = [&Graph](const EIROp Op, const TCHAR* Name) -> int32
	{
		FIRNode Node;
		Node.Op = Op;
		Node.DebugName = Name;
		if (Op != EIROp::FunctionOutput)
		{
			Node.Outputs.Add(FIRType::Float(1));
		}
		return Graph.AddNode(MoveTemp(Node));
	};

	constexpr int32 ChainLength = 1200;
	int32 Previous = AddNode(EIROp::Constant, TEXT("Seed"));
	for (int32 Link = 0; Link < ChainLength; ++Link)
	{
		const int32 Leaf = AddNode(EIROp::Constant, TEXT(""));
		const int32 Sum = AddNode(EIROp::Add, Link == ChainLength / 2 ? TEXT("Middle") : TEXT(""));
		Graph.Nodes[Sum].Operands.Add(FIRValue{ Previous, 0 });
		Graph.Nodes[Sum].Operands.Add(FIRValue{ Leaf, 0 });
		Previous = Sum;
	}
	const int32 Output = AddNode(EIROp::FunctionOutput, TEXT("Result"));
	Graph.Nodes[Output].Operands.Add(FIRValue{ Previous, 0 });
	Graph.FunctionOutputs.Add(Output);

	TestEqual(TEXT("the owner of a name is the node that carries it"), FindDreamShaderIRLayoutOwner(Graph, TEXT("Middle")), 2 * (ChainLength / 2) + 2);
	TestEqual(TEXT("a name nothing carries has no owner"), FindDreamShaderIRLayoutOwner(Graph, TEXT("Nobody")), static_cast<int32>(INDEX_NONE));

	for (const EIRLayoutStyle Style : GAllStyles)
	{
		const double Start = FPlatformTime::Seconds();
		const FIRLayoutResult Layout = LayOut(Graph, Style);
		const double Milliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
		AddInfo(FString::Printf(TEXT("%s: %d nodes in %.1f ms, %d columns"), LexToString(Style), Graph.Nodes.Num(), Milliseconds, Layout.ColumnCount));

		if (Style == EIRLayoutStyle::Blocks)
		{
			// One statement, however long, is one block: a block is cut between statements, never through one.
			TestTrue(TEXT("Blocks: the chain is as deep as it is long"), Layout.ColumnCount >= ChainLength);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s: the chain is as deep as it is long"), LexToString(Style)), Layout.ColumnCount, ChainLength + 2);
		}
		TestFalse(FString::Printf(TEXT("%s: every node is placed"), LexToString(Style)), Layout.Placed.Contains(false));
		// The plan asks for 1200 nodes in 50 ms; this is twice the nodes on whatever machine runs the suite, so the bound
		// only catches a pass that went quadratic.
		TestTrue(FString::Printf(TEXT("%s: a 2400-node graph lays out in under two seconds (%.1f ms)"), LexToString(Style), Milliseconds), Milliseconds < 2000.0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
