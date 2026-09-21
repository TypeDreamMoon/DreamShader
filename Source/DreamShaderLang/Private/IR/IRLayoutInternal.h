// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// What the layout's translation units share: the state of one layout run, and the few helpers both of them use.
// IRLayout.cpp holds the columns, the bands, the layered drawing, hints, region boxes and the dumps; IRLayoutBlocks.cpp
// holds the block style.

#pragma once

#include "IR/IRLayout.h"

namespace UE::DreamShader::IR::LayoutDetail
{
	/** Floor to a multiple of Grid; correct for negative coordinates, which is most of a graph. */
	inline int32 SnapDown(const int32 Value, const int32 Grid)
	{
		if (Grid <= 1)
		{
			return Value;
		}
		const int32 Remainder = Value % Grid;
		if (Remainder == 0)
		{
			return Value;
		}
		return Remainder > 0 ? Value - Remainder : Value - Remainder - Grid;
	}

	/** What a node is called in a dump: the variable it was assigned to, else its class, else its op. */
	FString DisplayNameOf(const FIRNode& Node);

	/** Where a node sorts in the text: the call site for a node made while inlining, else its own span. */
	struct FSourceKey
	{
		int32 Line = MAX_int32;
		int32 Column = MAX_int32;
		int32 Offset = MAX_int32;

		bool IsKnown() const { return Line != MAX_int32; }
	};

	inline FSourceKey SourceKeyOf(const FIRSourceRef& Source)
	{
		const Lang::FLangSpan& Span = Source.HasCallSite() ? Source.CallSite : Source.Span;
		FSourceKey Key;
		if (Span.Line > 0)
		{
			Key.Line = Span.Line;
			Key.Column = Span.Column;
			Key.Offset = Span.Offset;
		}
		return Key;
	}

	struct FBandRoot
	{
		int32 Node = INDEX_NONE;
		FSourceKey Key;
		/** Bindings before statement nodes before everything else when two sit on one source position. */
		int32 Rank = 0;
		int32 Sequence = 0;
	};

	/**
	 * A layered drawing of any set of nodes: given a column per node (0 is the rightmost) and who reads whom, orders every
	 * column to keep wires from crossing and then pulls every node towards the median of its neighbours without changing
	 * that order. The whole-graph Layered style is one call over every node; the block style is one call per block, over
	 * the block's nodes and the reroutes on its edges.
	 */
	struct FLayeredGraph
	{
		/** INDEX_NONE for an index that takes no part. */
		TArray<int32> Column;
		/** Distinct indices an index reads, in the order it reads them; and who reads it, ascending. */
		TArray<TArray<int32>> Reads;
		TArray<TArray<int32>> Readers;
		TArray<int32> Height;

		/** Out: the top edge of every index that takes part. */
		TArray<int32> Top;

		int32 Num() const { return Column.Num(); }
		void SetNum(int32 Count);
		void Solve(int32 RowGap);
	};

	namespace BlockDetail
	{
		enum class EEndpointKind : uint8
		{
			/** The declaration of a bridge, in the leaf that makes the value. */
			BridgeDeclaration,
			/** A usage of a bridge -- or the repeated constant -- in a leaf that reads the value. */
			BridgeUse,
			/** The declaration of the reroute pair in front of an output, in the leaf that makes the value. */
			RouteDeclaration,
			/** Its usage: beside the function output it feeds, or beside the material's own node (Leaf is INDEX_NONE). */
			RouteUsage,
		};

		/** A reroute the layout counts on. It takes room in a leaf like a node, and the host makes it. */
		struct FEndpoint
		{
			EEndpointKind Kind = EEndpointKind::BridgeUse;
			/** Index into Bridges or OutputRoutes, and for a BridgeUse the use. */
			int32 Index = INDEX_NONE;
			int32 Use = INDEX_NONE;
			int32 Leaf = INDEX_NONE;
			/** The node whose hint offset it follows: the value's node for a declaration, the first reader for a usage. */
			int32 Anchor = INDEX_NONE;
			FIRLayoutNodeSize Size;
			FIntPoint Position = FIntPoint::ZeroValue;
		};

		struct FLeaf
		{
			int32 Region = INDEX_NONE;
			bool bOutputs = false;
			int32 FirstBand = MAX_int32;
			TArray<int32> Nodes;
			TArray<int32> Endpoints;
			FString FirstName;
			FString LastName;
			int32 FirstLine = 0;
			int32 LastLine = 0;

			/** The drawing inside the box, its top-left at the origin. */
			FIntPoint InnerSize = FIntPoint::ZeroValue;
			/** Where that origin went on the page. */
			FIntPoint Origin = FIntPoint::ZeroValue;
		};

		/** A box of the page: a leaf, or a region around several boxes. */
		struct FBox
		{
			int32 Leaf = INDEX_NONE;
			int32 Region = INDEX_NONE;
			int32 FirstBand = MAX_int32;
			TArray<int32> Children;
			FIntPoint Size = FIntPoint::ZeroValue;
			FIntPoint Position = FIntPoint::ZeroValue;
		};
	}

	/**
	 * What the block style keeps between LayoutBlocks and the two steps that come after the hints. The result's own arrays
	 * hold what a host is told; the leaves and the reroutes-as-things-that-take-room have no place there.
	 */
	struct FBlockScratch
	{
		TArray<BlockDetail::FLeaf> Leaves;
		TArray<BlockDetail::FEndpoint> Endpoints;
		TArray<BlockDetail::FBox> Boxes;
		TArray<int32> RegionDepth;
	};

	struct FState
	{
		const FIRGraph& Graph;
		const FIRLayoutOptions& Options;
		FIRLayoutResult& Out;
		const int32 NodeCount;

		/** Distinct nodes a node reads, in the order it reads them; and the distinct nodes that read it, ascending. */
		TArray<TArray<int32>> Reads;
		TArray<TArray<int32>> Readers;
		/** Operands first; a node on a cycle is not in it. */
		TArray<int32> Order;

		TArray<int32> ColumnWidth;
		TArray<int32> ColumnLeft;

		/** Y of a node's top edge before hints and snapping. */
		TArray<int32> Top;
		/** Per node: the reader it hangs under in its band's tree (SourceBands), or the reader used to carry a hint's offset. */
		TArray<int32> TreeParent;
		/** Per band: the node that opened it. */
		TArray<int32> BandRootNode;
		/** Per node: how far a `#pragma layout(Node, ...)` hint moved it, for what travels with a node. */
		TArray<FIntPoint> HintOffset;
		FBlockScratch BlockScratch;

		FState(const FIRGraph& InGraph, const FIRLayoutOptions& InOptions, FIRLayoutResult& InOut)
			: Graph(InGraph)
			, Options(InOptions)
			, Out(InOut)
			, NodeCount(InGraph.Nodes.Num())
		{
		}

		bool IsPlaced(const int32 Node) const { return Out.Placed.IsValidIndex(Node) && Out.Placed[Node]; }
		int32 HeightOf(const int32 Node) const { return Out.Sizes[Node].Height; }
		int32 WidthOf(const int32 Node) const { return Out.Sizes[Node].Width; }

		void BuildAdjacency();
		void AssignColumns();
		void AssignSizesAndColumnGeometry();
		void AssignBands();
		void LayoutBands();
		void LayoutLayered();
		void ApplyHorizontal();
		void ApplyNodeHints();
		void SnapAndMeasure();
		void BuildRegionBoxes();
		void PassCommentHints();
		void CollectLongEdges();
		void PlaceRoot();

		// The block style (IRLayoutBlocks.cpp).
		void LayoutBlocks();
		void FinishBlocks();
		void PlaceBlockOutputs();
	};
}
