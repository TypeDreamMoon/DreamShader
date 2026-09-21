// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Graph layout on the IR: where every node of a product goes, decided before a single UObject exists.
//
// The 1.x layout (DreamShaderCompiler/Private/Layout) works on live UMaterialExpressions: it can only run in an
// editor, it can only be tested by looking, and it knows nothing about the source -- it recovers "blocks" from the
// material's output pins. This pass works on the FIRGraph, which still knows which statement made which node, so it can
// do the one thing a reader of a generated graph wants: put the graph in the order of the text.
//
// Three styles, all deterministic (the same graph gives the same coordinates, byte for byte) and all left to right,
// the way the material editor reads: a node is always to the right of everything it reads through a wire.
//
//   * Blocks      -- the graph cut into blocks, one box each, packed in source order like words on a page. A top-level
//     `#pragma region` is a block; what lies between regions is cut into runs of statements. Inside a block the nodes
//     are one small layered drawing. No wire leaves a block: a value that another block reads travels through a named
//     reroute -- a declaration beside the value, a usage in every block that reads it -- and a constant is simply
//     repeated where it is read. What drives the material, or a function's outputs, leaves its block the same way.
//   * SourceBands -- one horizontal band per statement, in source order, top to bottom. A node belongs to the first
//     statement that needs it; inside a band the expression is a tidy tree, a node centred on the operands it owns.
//     `#pragma region` is a range of statements, so a region comes out as a box around consecutive bands.
//   * Layered     -- the whole graph as one layered drawing (longest-path layers, barycentre ordering, median
//     alignment). Denser, and blind to the source.
//
// In the last two a node's column is its longest distance from a root (the sink, a function output, a custom-output
// statement), so no wire ever runs right to left; in Blocks the same holds inside each block.
//
// The pass never changes the graph. The last two insert nothing and report the long wires instead
// (FIRLayoutResult::LongEdges). Blocks says which reroutes it counted on (FIRLayoutResult::Bridges, ::OutputRoutes)
// and where they go; making them is the host's, because only the host has nodes to make.
//
// Positions are node top-left corners in material-editor units, snapped to GridSize. Sizes come from the host when it
// has live nodes to measure (FIRLayoutOptions::NodeSizes); EstimateDreamShaderIRNodeSize is the engine-free stand-in
// the tests and `dsc dump-layout` use.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"

namespace UE::DreamShader::IR
{
	enum class EIRLayoutStyle : uint8
	{
		/** One box per region or run of statements, named reroutes between the boxes. */
		Blocks,
		/** One band per statement, in source order. */
		SourceBands,
		/** One layered drawing of the whole graph. */
		Layered,
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(EIRLayoutStyle Style);
	/** "Blocks" / "SourceBands" / "Layered", ignoring case. False, with OutStyle untouched, for anything else. */
	DREAMSHADERLANG_API bool TryParseIRLayoutStyle(const FString& Text, EIRLayoutStyle& OutStyle);

	struct FIRLayoutNodeSize
	{
		int32 Width = 0;
		int32 Height = 0;
	};

	struct FIRLayoutOptions
	{
		EIRLayoutStyle Style = EIRLayoutStyle::Blocks;

		/** Horizontal space between two columns, and vertical space between two nodes of one column. */
		int32 ColumnGap = 120;
		int32 RowGap = 48;
		/** SourceBands: vertical space between two bands. */
		int32 BandGap = 96;
		/** Blocks: space between two boxes, and the most nodes a run of statements outside every region puts in one box. */
		int32 BlockGap = 160;
		int32 BlockMaxNodes = 40;
		/** Blocks: how much wider than tall the page of boxes should come out. */
		float BlockPageAspect = 1.8f;
		/** Space between a region box and what it holds, per level of nesting; and the title bar on top of it. */
		int32 RegionPaddingX = 110;
		int32 RegionPaddingY = 90;
		int32 RegionTitleHeight = 40;
		/** Space between the rightmost column and the material's own node. */
		int32 SinkGap = 320;
		/** Every coordinate is a multiple of this; 1 turns snapping off. */
		int32 GridSize = 16;

		/**
		 * Per node, measured by the host; an entry with Width <= 0 (and every node past the end) falls back to
		 * EstimateDreamShaderIRNodeSize. The emitter fills this from the live expressions, whose size it already knows.
		 */
		TArray<FIRLayoutNodeSize> NodeSizes;

		/** `#pragma layout(Node, Var = ..., X = ..., Y = ...)` wins over the computed position of that node. */
		bool bApplyNodeHints = true;
		/** `#pragma layout(Comment, ...)` boxes are passed through into FIRLayoutResult::Comments. */
		bool bApplyCommentHints = true;
		/** One box per `#pragma region`, around its nodes and around the regions nested in it. */
		bool bRegionBoxes = true;

		/** A wire that spans at least this many columns, or leaves its band, is reported in LongEdges. 0 reports none. */
		int32 LongEdgeColumns = 6;
	};

	/** A comment box: a region, or a `#pragma layout(Comment, ...)` hint. */
	struct FIRLayoutComment
	{
		FString Title;
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;
		/** Index into FIRGraph::Regions, or INDEX_NONE for a hint. */
		int32 Region = INDEX_NONE;
		/** Nesting depth of a region box, 0 for the outermost; a host draws deeper boxes later so that they stay on top. */
		int32 Depth = 0;
		/** Made by the layout -- a region, a block -- rather than written by the author as a `#pragma layout(Comment, ...)`. */
		bool bGenerated = false;
		bool bHasColor = false;
		float Color[4] = { 0.10f, 0.16f, 0.22f, 0.35f };
	};

	/** A wire the host may want to replace by a named reroute pair. */
	struct FIRLayoutLongEdge
	{
		/** The value that travels, and the node that reads it. */
		FIRValue Source;
		int32 Consumer = INDEX_NONE;
		/** Columns between the two. */
		int32 ColumnSpan = 0;
		/** SourceBands: the two nodes sit in different bands. */
		bool bCrossesBands = false;
	};

	/** Blocks: one box of the page. */
	struct FIRLayoutBlock
	{
		FString Title;
		/** The top-level region the block stands for, or INDEX_NONE for a run of statements or the outputs. */
		int32 Region = INDEX_NONE;
		/** The nodes of the block, ascending. */
		TArray<int32> Nodes;
		/** Source lines the block covers, 0 where unknown. */
		int32 FirstLine = 0;
		int32 LastLine = 0;
		/** True for the box that holds a function's outputs. */
		bool bOutputs = false;
		/** The box, title and padding included. */
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;
	};

	/** Blocks: where a bridged value is read. */
	struct FIRLayoutBridgeUse
	{
		int32 Block = INDEX_NONE;
		FIntPoint Position = FIntPoint::ZeroValue;
		FIRLayoutNodeSize Size;
		/** The nodes of that block that read the value, ascending. */
		TArray<int32> Consumers;
	};

	/**
	 * Blocks: a value one block makes and others read. The host puts a named reroute declaration at DeclarationPosition,
	 * fed by Source, and a usage of it at every Uses[i].Position; when bClone is set -- the value is a constant -- it
	 * repeats the node at every use instead and makes no declaration.
	 */
	struct FIRLayoutBridge
	{
		FIRValue Source;
		/** Unique in the graph: `DS_<variable>`, or `DS_Shared_<n>` for a value no variable names. */
		FString Name;
		int32 SourceBlock = INDEX_NONE;
		bool bClone = false;
		FIntPoint DeclarationPosition = FIntPoint::ZeroValue;
		FIRLayoutNodeSize DeclarationSize;
		TArray<FIRLayoutBridgeUse> Uses;
	};

	/**
	 * Blocks: a value that drives the material or a function's output. The host already routes every one of those
	 * through a named reroute pair of its own; this says where the two halves go.
	 */
	struct FIRLayoutOutputRoute
	{
		/** The sink or a FunctionOutput node, and which of its input values this is (FIRGraph::CollectInputValues order). */
		int32 Consumer = INDEX_NONE;
		int32 InputIndex = 0;
		FIRValue Source;
		FIntPoint DeclarationPosition = FIntPoint::ZeroValue;
		FIRLayoutNodeSize DeclarationSize;
		FIntPoint UsagePosition = FIntPoint::ZeroValue;
		FIRLayoutNodeSize UsageSize;
	};

	struct FIRLayoutResult
	{
		/** Per node of the graph: its top-left corner. Meaningful where Placed is set. */
		TArray<FIntPoint> Positions;
		/** Per node: the size the pass worked with. */
		TArray<FIRLayoutNodeSize> Sizes;
		/** Per node: false for a node no root reaches (the prune pass normally removed those already). */
		TArray<bool> Placed;
		/** Per node: its column, 0 at the roots; INDEX_NONE where not placed. */
		TArray<int32> Columns;
		/** Per node, SourceBands only: the band it belongs to, in source order; INDEX_NONE otherwise. */
		TArray<int32> Bands;
		/** Per node: true where a `#pragma layout(Node, ...)` hint set the position. */
		TArray<bool> Hinted;

		/** Per node, Blocks only: the block it belongs to; INDEX_NONE otherwise. */
		TArray<int32> Blocks;

		TArray<FIRLayoutComment> Comments;
		TArray<FIRLayoutLongEdge> LongEdges;
		/** Blocks only: the boxes in source order, the reroutes between them, and the reroutes in front of the outputs. */
		TArray<FIRLayoutBlock> BlockList;
		TArray<FIRLayoutBridge> Bridges;
		TArray<FIRLayoutOutputRoute> OutputRoutes;

		/** The bounding box of every placed node, comments not included. */
		FIntPoint BoundsMin = FIntPoint::ZeroValue;
		FIntPoint BoundsMax = FIntPoint::ZeroValue;
		/**
		 * Where the material's own node goes: right of everything, level with the middle of what feeds it. For a
		 * function product it is the same point and the host has no use for it.
		 */
		FIntPoint RootPosition = FIntPoint::ZeroValue;

		int32 BandCount = 0;
		int32 ColumnCount = 0;
	};

	/**
	 * An engine-free guess at a node's footprint: a title as wide as its name, a row per pin. Good enough to keep nodes
	 * from overlapping when no live node can be measured; a host that has live nodes passes their sizes instead.
	 */
	DREAMSHADERLANG_API FIRLayoutNodeSize EstimateDreamShaderIRNodeSize(const FIRGraph& Graph, int32 NodeIndex);

	/**
	 * The node a `#pragma layout(Node, Var = Name)` hint places: among the nodes whose DebugName is Name, the one none
	 * of whose readers carries that name too -- the value the variable holds, not a part of the expression that made it.
	 * INDEX_NONE when no node carries the name. The emitter and the decompiler agree on this rule, which is what makes a
	 * position written out come back to the same node.
	 */
	DREAMSHADERLANG_API int32 FindDreamShaderIRLayoutOwner(const FIRGraph& Graph, const FString& VariableName);

	/**
	 * Lays one graph out. Never fails: a graph with a cycle places what it can (the validator reports the cycle), and an
	 * empty graph gives an empty result. Does not touch the graph.
	 */
	DREAMSHADERLANG_API void LayoutDreamShaderIRGraph(
		const FIRGraph& Graph,
		const FIRLayoutOptions& Options,
		FIRLayoutResult& OutResult);

	/** The room a named reroute declaration or usage of that name takes; what the pass gives the reroutes it counts on. */
	DREAMSHADERLANG_API FIRLayoutNodeSize EstimateDreamShaderIRRerouteSize(const FString& Name);

	/**
	 * The layout as a standalone SVG: boxes with their names, wires, region and block boxes, reroutes. For looking at a
	 * layout without an editor -- `dsc dump-layout` writes one per product and style.
	 */
	DREAMSHADERLANG_API FString DumpDreamShaderIRLayoutSvg(
		const FIRGraph& Graph,
		const FIRLayoutResult& Layout,
		const FString& Title);

	/** The layout as JSON (schema `dreamshader-ir-layout`, version 1): what a golden or a script compares. */
	DREAMSHADERLANG_API FString DumpDreamShaderIRLayoutJson(
		const FIRGraph& Graph,
		const FIRLayoutResult& Layout,
		EIRLayoutStyle Style);
}
