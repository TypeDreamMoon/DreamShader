// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// LayoutDreamShaderIRGraph: columns, bands, tidy trees, the layered drawing, hints, region boxes, and the dumps. The
// block style is in IRLayoutBlocks.cpp; what the two share is in IRLayoutInternal.h.
//
// Everything is integer arithmetic over arrays indexed by node, every sort is stable with the node index as its last
// key, and nothing iterates a hash container: the same graph gives the same coordinates on every run and on every
// machine, which is what lets a layout be a golden.
//
// Nothing here recurses. A 3000-node chain is a legal graph (FIRGraph::TopologicalOrder says the same), and the tidy
// tree below walks it with an explicit stack.

#include "IR/IRLayout.h"

#include "IRLayoutInternal.h"

#include "IR/IRCoreOps.h"
#include "IRJson.h"

namespace UE::DreamShader::IR
{
	namespace LayoutDetail
	{
		FString DisplayNameOf(const FIRNode& Node)
		{
			if (!Node.DebugName.IsEmpty())
			{
				return Node.DebugName;
			}
			if (Node.Op == EIROp::Reflected && !Node.ClassName.IsEmpty())
			{
				return Node.ClassName;
			}
			if (Node.Op == EIROp::FunctionCall && !Node.ClassName.IsEmpty())
			{
				// `/Game/Functions/MF_Tint.MF_Tint` reads as `MF_Tint`.
				int32 Dot = INDEX_NONE;
				if (Node.ClassName.FindLastChar(TEXT('.'), Dot))
				{
					return Node.ClassName.Mid(Dot + 1);
				}
				return Node.ClassName;
			}
			if (Node.Op == EIROp::Custom && !Node.ClassName.IsEmpty())
			{
				return Node.ClassName;
			}
			return FString(LexToString(Node.Op));
		}

		void FState::BuildAdjacency()
		{
			Reads.SetNum(NodeCount);
			Readers.SetNum(NodeCount);

			TArray<FIRValue> Values;
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				Values.Reset();
				FIRGraph::CollectInputValues(Graph.Nodes[Node], Values);
				for (const FIRValue& Value : Values)
				{
					if (Graph.Nodes.IsValidIndex(Value.Node) && Value.Node != Node)
					{
						Reads[Node].AddUnique(Value.Node);
					}
				}
			}
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				for (const int32 Read : Reads[Node])
				{
					Readers[Read].AddUnique(Node);
				}
			}

			Order = Graph.TopologicalOrder();
		}

		void FState::AssignColumns()
		{
			Out.Placed.Init(false, NodeCount);
			Out.Columns.Init(INDEX_NONE, NodeCount);
			Out.Bands.Init(INDEX_NONE, NodeCount);
			Out.Blocks.Init(INDEX_NONE, NodeCount);
			Out.Hinted.Init(false, NodeCount);
			Out.Positions.Init(FIntPoint::ZeroValue, NodeCount);
			TreeParent.Init(INDEX_NONE, NodeCount);
			Top.Init(0, NodeCount);

			for (const int32 Node : Order)
			{
				Out.Placed[Node] = true;
			}

			// Readers before what they read: the reverse of the topological order. A node nothing reads is a root.
			int32 MaxColumn = 0;
			for (int32 Index = Order.Num() - 1; Index >= 0; --Index)
			{
				const int32 Node = Order[Index];
				int32 Column = 0;
				for (const int32 Reader : Readers[Node])
				{
					if (IsPlaced(Reader) && Out.Columns[Reader] != INDEX_NONE)
					{
						Column = FMath::Max(Column, Out.Columns[Reader] + 1);
					}
				}
				Out.Columns[Node] = Column;
				MaxColumn = FMath::Max(MaxColumn, Column);
			}
			Out.ColumnCount = Order.Num() > 0 ? MaxColumn + 1 : 0;
		}

		void FState::AssignSizesAndColumnGeometry()
		{
			Out.Sizes.SetNum(NodeCount);
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				FIRLayoutNodeSize Size;
				if (Options.NodeSizes.IsValidIndex(Node) && Options.NodeSizes[Node].Width > 0 && Options.NodeSizes[Node].Height > 0)
				{
					Size = Options.NodeSizes[Node];
				}
				else
				{
					Size = EstimateDreamShaderIRNodeSize(Graph, Node);
				}
				// The sink is the material's own node: it takes no room in a column, and PlaceRoot puts it.
				if (Node == Graph.Sink)
				{
					Size.Width = 0;
					Size.Height = 0;
				}
				Out.Sizes[Node] = Size;
			}

			ColumnWidth.Init(0, Out.ColumnCount);
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (IsPlaced(Node))
				{
					ColumnWidth[Out.Columns[Node]] = FMath::Max(ColumnWidth[Out.Columns[Node]], WidthOf(Node));
				}
			}

			// Column 0 ends at x = 0 and the columns grow to the left, a gap apart. A column that holds nothing wide (the
			// sink alone) takes no gap either.
			ColumnLeft.Init(0, Out.ColumnCount);
			int32 Right = 0;
			for (int32 Column = 0; Column < Out.ColumnCount; ++Column)
			{
				ColumnLeft[Column] = Right - ColumnWidth[Column];
				Right = ColumnLeft[Column] - (ColumnWidth[Column] > 0 ? FMath::Max(0, Options.ColumnGap) : 0);
			}
		}

		void FState::AssignBands()
		{
			TArray<FBandRoot> Roots;
			int32 Sequence = 0;
			const auto AddRootAt = [this, &Roots, &Sequence](const int32 Node, const FSourceKey& Key, const int32 Rank)
			{
				if (!IsPlaced(Node) || Node == Graph.Sink)
				{
					return;
				}
				FBandRoot Root;
				Root.Node = Node;
				Root.Key = Key;
				Root.Rank = Rank;
				Root.Sequence = Sequence++;
				Roots.Add(Root);
			};
			const auto AddRoot = [&AddRootAt](const int32 Node, const FIRSourceRef& Source, const int32 Rank)
			{
				AddRootAt(Node, SourceKeyOf(Source), Rank);
			};
			// A statement happens after what it reads. A function's outputs are made when the body ends and carry the
			// DECLARATION's position, which stacked every one of them above the first statement with a wire running back
			// down to its value -- forty of them, on a real function. Keyed by the latest thing it reads, an output sits
			// right under the statement that computed it.
			const auto StatementKeyOf = [this](const int32 Node)
			{
				FSourceKey Key = SourceKeyOf(Graph.Nodes[Node].Source);
				for (const int32 Read : Reads[Node])
				{
					const FSourceKey ReadKey = SourceKeyOf(Graph.Nodes[Read].Source);
					if (ReadKey.Line == MAX_int32)
					{
						continue;
					}
					if (Key.Line == MAX_int32 || ReadKey.Line > Key.Line || (ReadKey.Line == Key.Line && ReadKey.Offset > Key.Offset))
					{
						Key = ReadKey;
					}
				}
				return Key;
			};

			// A statement that named a variable; what is wired into the material; what a function returns; and, last, any
			// node nothing reads -- so that every placed node ends up under some root.
			for (const FIRStatementBinding& Binding : Graph.StatementBindings)
			{
				AddRoot(Binding.Value.Node, Binding.Source, 0);
			}
			if (Graph.Nodes.IsValidIndex(Graph.Sink))
			{
				for (const int32 Read : Reads[Graph.Sink])
				{
					AddRoot(Read, Graph.Nodes[Read].Source, 1);
				}
			}
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (IsPlaced(Node) && Node != Graph.Sink && (Graph.Nodes[Node].IsStatement() || Readers[Node].Num() == 0))
				{
					if (Graph.Nodes[Node].IsStatement())
					{
						AddRootAt(Node, StatementKeyOf(Node), 2);
					}
					else
					{
						AddRoot(Node, Graph.Nodes[Node].Source, 2);
					}
				}
			}

			Roots.StableSort([](const FBandRoot& A, const FBandRoot& B)
			{
				if (A.Key.Line != B.Key.Line)
				{
					return A.Key.Line < B.Key.Line;
				}
				if (A.Key.Offset != B.Key.Offset)
				{
					return A.Key.Offset < B.Key.Offset;
				}
				if (A.Rank != B.Rank)
				{
					return A.Rank < B.Rank;
				}
				return A.Sequence < B.Sequence;
			});

			// A root that another band reaches first is still its own band: `float3 albedo = ...;` stays the band of
			// `albedo` however early something reads it.
			TArray<bool> IsRoot;
			IsRoot.Init(false, NodeCount);
			for (const FBandRoot& Root : Roots)
			{
				IsRoot[Root.Node] = true;
			}

			int32 BandCount = 0;
			TArray<int32> Stack;
			BandRootNode.Reset();
			for (const FBandRoot& Root : Roots)
			{
				if (Out.Bands[Root.Node] != INDEX_NONE)
				{
					continue;
				}
				const int32 Band = BandCount++;
				BandRootNode.Add(Root.Node);
				Out.Bands[Root.Node] = Band;
				Stack.Reset();
				Stack.Add(Root.Node);
				while (Stack.Num() > 0)
				{
					const int32 Node = Stack.Pop();
					for (const int32 Read : Reads[Node])
					{
						if (!IsPlaced(Read) || Out.Bands[Read] != INDEX_NONE || IsRoot[Read] || Read == Graph.Sink)
						{
							continue;
						}
						Out.Bands[Read] = Band;
						Stack.Add(Read);
					}
				}
			}
			Out.BandCount = BandCount;
		}

		void FState::LayoutBands()
		{
			AssignBands();

			// The reader a node hangs under: in its own band, and the nearest one, so that a tree edge is a short wire.
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (!IsPlaced(Node) || Out.Bands[Node] == INDEX_NONE)
				{
					continue;
				}
				int32 Best = INDEX_NONE;
				for (const int32 Reader : Readers[Node])
				{
					if (!IsPlaced(Reader) || Out.Bands[Reader] != Out.Bands[Node])
					{
						continue;
					}
					if (Best == INDEX_NONE || Out.Columns[Reader] > Out.Columns[Best])
					{
						Best = Reader;
					}
				}
				TreeParent[Node] = Best;
			}

			// Children in the order their parent reads them.
			TArray<TArray<int32>> Children;
			Children.SetNum(NodeCount);
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (!IsPlaced(Node))
				{
					continue;
				}
				for (const int32 Read : Reads[Node])
				{
					if (IsPlaced(Read) && TreeParent[Read] == Node)
					{
						Children[Node].Add(Read);
					}
				}
			}

			// Extent of every subtree, children before parents: operands come first in Order and a child is an operand.
			TArray<int32> Extent;
			Extent.Init(0, NodeCount);
			const int32 RowGap = FMath::Max(0, Options.RowGap);
			for (const int32 Node : Order)
			{
				int32 ChildrenExtent = 0;
				for (int32 Index = 0; Index < Children[Node].Num(); ++Index)
				{
					ChildrenExtent += Extent[Children[Node][Index]] + (Index > 0 ? RowGap : 0);
				}
				Extent[Node] = FMath::Max(HeightOf(Node), ChildrenExtent);
			}

			// The roots of each band's trees, in band order. A band has one root by construction, but a node whose only
			// readers live in later bands is a tree root of its own band too.
			TArray<TArray<int32>> BandTreeRoots;
			BandTreeRoots.SetNum(Out.BandCount);
			for (const int32 Node : Order)
			{
				if (Out.Bands[Node] != INDEX_NONE && TreeParent[Node] == INDEX_NONE)
				{
					BandTreeRoots[Out.Bands[Node]].Add(Node);
				}
			}

			struct FFrame
			{
				int32 Node = INDEX_NONE;
				int32 BlockTop = 0;
			};
			TArray<FFrame> Stack;

			int32 Cursor = 0;
			int32 PreviousRegion = INDEX_NONE;
			for (int32 Band = 0; Band < Out.BandCount; ++Band)
			{
				TArray<int32>& TreeRoots = BandTreeRoots[Band];
				// The band's own root last: it is the rightmost node, and what feeds it from outside its tree sits above it.
				TreeRoots.StableSort([this](const int32 A, const int32 B)
				{
					if (Out.Columns[A] != Out.Columns[B])
					{
						return Out.Columns[A] > Out.Columns[B];
					}
					return A < B;
				});

				// Room for the box when the region changes: two paddings and a title would otherwise overlap the neighbour.
				const int32 BandRegion = TreeRoots.Num() > 0 ? Graph.Nodes[TreeRoots.Last()].Region : INDEX_NONE;
				if (Band > 0)
				{
					Cursor += FMath::Max(0, Options.BandGap);
					if (Options.bRegionBoxes && BandRegion != PreviousRegion)
					{
						Cursor += 2 * FMath::Max(0, Options.RegionPaddingY) + FMath::Max(0, Options.RegionTitleHeight);
					}
				}
				PreviousRegion = BandRegion;

				for (const int32 TreeRoot : TreeRoots)
				{
					Stack.Reset();
					Stack.Add({ TreeRoot, Cursor });
					while (Stack.Num() > 0)
					{
						const FFrame Frame = Stack.Pop();
						const int32 Node = Frame.Node;
						Top[Node] = Frame.BlockTop + (Extent[Node] - HeightOf(Node)) / 2;

						int32 ChildrenExtent = 0;
						for (int32 Index = 0; Index < Children[Node].Num(); ++Index)
						{
							ChildrenExtent += Extent[Children[Node][Index]] + (Index > 0 ? RowGap : 0);
						}
						int32 ChildTop = Frame.BlockTop + (Extent[Node] - ChildrenExtent) / 2;
						for (const int32 Child : Children[Node])
						{
							Stack.Add({ Child, ChildTop });
							ChildTop += Extent[Child] + RowGap;
						}
					}
					Cursor += Extent[TreeRoot] + RowGap;
				}
				if (TreeRoots.Num() > 0)
				{
					Cursor -= RowGap;
				}
			}
		}

		void FLayeredGraph::SetNum(const int32 Count)
		{
			Column.Init(INDEX_NONE, Count);
			Reads.Reset();
			Reads.SetNum(Count);
			Readers.Reset();
			Readers.SetNum(Count);
			Height.Init(0, Count);
			Top.Init(0, Count);
		}

		void FLayeredGraph::Solve(const int32 RowGap)
		{
			const int32 Count = Num();
			Top.Init(0, Count);
			int32 ColumnCount = 0;
			for (const int32 Value : Column)
			{
				ColumnCount = FMath::Max(ColumnCount, Value + 1);
			}
			if (ColumnCount == 0)
			{
				return;
			}
			const auto TakesPart = [this](const int32 Index) { return Column.IsValidIndex(Index) && Column[Index] != INDEX_NONE; };

			TArray<TArray<int32>> Layers;
			Layers.SetNum(ColumnCount);

			// First order: a depth-first walk from everything nothing reads, operands in the order they are read. It already
			// untangles most of a tree-like graph, and it is what makes the result independent of node numbering.
			{
				TArray<bool> Seen;
				Seen.Init(false, Count);
				struct FWalk
				{
					int32 Node = INDEX_NONE;
					int32 Cursor = 0;
				};
				TArray<FWalk> Stack;
				const auto WalkFrom = [this, &Seen, &Layers, &Stack, &TakesPart](const int32 Root)
				{
					Seen[Root] = true;
					Layers[Column[Root]].Add(Root);
					Stack.Add({ Root, 0 });
					while (Stack.Num() > 0)
					{
						FWalk& Frame = Stack.Last();
						if (Frame.Cursor >= Reads[Frame.Node].Num())
						{
							Stack.Pop();
							continue;
						}
						const int32 Read = Reads[Frame.Node][Frame.Cursor++];
						if (TakesPart(Read) && !Seen[Read])
						{
							Seen[Read] = true;
							Layers[Column[Read]].Add(Read);
							Stack.Add({ Read, 0 });
						}
					}
				};
				for (int32 Root = 0; Root < Count; ++Root)
				{
					if (!TakesPart(Root) || Seen[Root])
					{
						continue;
					}
					bool bIsRead = false;
					for (const int32 Reader : Readers[Root])
					{
						bIsRead = bIsRead || TakesPart(Reader);
					}
					if (!bIsRead)
					{
						WalkFrom(Root);
					}
				}
				// What only a cycle reaches still has to stand somewhere.
				for (int32 Root = 0; Root < Count; ++Root)
				{
					if (TakesPart(Root) && !Seen[Root])
					{
						WalkFrom(Root);
					}
				}
			}

			// Position of a node inside its layer, scaled so that layers of different sizes compare.
			TArray<int32> Rank;
			Rank.Init(0, Count);
			const auto Rerank = [&Layers, &Rank](const int32 LayerIndex)
			{
				const TArray<int32>& Layer = Layers[LayerIndex];
				for (int32 Index = 0; Index < Layer.Num(); ++Index)
				{
					Rank[Layer[Index]] = Layer.Num() > 1 ? (Index * 10000) / (Layer.Num() - 1) : 5000;
				}
			};
			for (int32 LayerIndex = 0; LayerIndex < ColumnCount; ++LayerIndex)
			{
				Rerank(LayerIndex);
			}

			// One key array for every sort: a graph as deep as it is long has a layer per node, and an array per layer
			// would make the pass quadratic.
			TArray<int32> Key;
			Key.Init(0, Count);
			const auto SortByNeighbours = [this, &Layers, &Rank, &Rerank, &Key, &TakesPart](const int32 LayerIndex, const bool bByReaders)
			{
				TArray<int32>& Layer = Layers[LayerIndex];
				for (const int32 Node : Layer)
				{
					const TArray<int32>& Neighbours = bByReaders ? Readers[Node] : Reads[Node];
					int64 Sum = 0;
					int32 NeighbourCount = 0;
					for (const int32 Neighbour : Neighbours)
					{
						if (TakesPart(Neighbour))
						{
							Sum += Rank[Neighbour];
							++NeighbourCount;
						}
					}
					Key[Node] = NeighbourCount > 0 ? static_cast<int32>(Sum / NeighbourCount) : Rank[Node];
				}
				Layer.StableSort([&Key, &Rank](const int32 A, const int32 B)
				{
					if (Key[A] != Key[B])
					{
						return Key[A] < Key[B];
					}
					if (Rank[A] != Rank[B])
					{
						return Rank[A] < Rank[B];
					}
					return A < B;
				});
				Rerank(LayerIndex);
			};

			constexpr int32 OrderingSweeps = 4;
			for (int32 Sweep = 0; Sweep < OrderingSweeps; ++Sweep)
			{
				for (int32 LayerIndex = 1; LayerIndex < ColumnCount; ++LayerIndex)
				{
					SortByNeighbours(LayerIndex, /* bByReaders */ true);
				}
				for (int32 LayerIndex = ColumnCount - 2; LayerIndex >= 0; --LayerIndex)
				{
					SortByNeighbours(LayerIndex, /* bByReaders */ false);
				}
			}

			// Coordinates: packed first, then pulled towards the median of the neighbours without ever changing the order.
			for (int32 LayerIndex = 0; LayerIndex < ColumnCount; ++LayerIndex)
			{
				int32 Cursor = 0;
				for (const int32 Node : Layers[LayerIndex])
				{
					Top[Node] = Cursor;
					Cursor += Height[Node] + RowGap;
				}
			}

			const auto Align = [this, &Layers, RowGap, &TakesPart](const int32 LayerIndex, const bool bByReaders)
			{
				const TArray<int32>& Layer = Layers[LayerIndex];
				if (Layer.Num() == 0)
				{
					return;
				}

				TArray<int32> Wanted;
				Wanted.SetNum(Layer.Num());
				TArray<int32> Centres;
				for (int32 Index = 0; Index < Layer.Num(); ++Index)
				{
					const int32 Node = Layer[Index];
					const TArray<int32>& Neighbours = bByReaders ? Readers[Node] : Reads[Node];
					Centres.Reset();
					for (const int32 Neighbour : Neighbours)
					{
						if (TakesPart(Neighbour))
						{
							Centres.Add(Top[Neighbour] + Height[Neighbour] / 2);
						}
					}
					if (Centres.Num() == 0)
					{
						Wanted[Index] = Top[Node];
						continue;
					}
					Centres.Sort();
					const int32 Median = Centres.Num() % 2 == 1
						? Centres[Centres.Num() / 2]
						: (Centres[Centres.Num() / 2 - 1] + Centres[Centres.Num() / 2]) / 2;
					Wanted[Index] = Median - Height[Node] / 2;
				}

				// Two feasible placements -- everybody pushed down only, everybody pushed up only -- and the middle of the
				// two. Both keep the order and the gaps, so their mean does; and a cluster that wants one spot ends up
				// centred on it instead of hanging below it.
				TArray<int32> Down;
				TArray<int32> Up;
				Down.SetNum(Layer.Num());
				Up.SetNum(Layer.Num());
				for (int32 Index = 0; Index < Layer.Num(); ++Index)
				{
					Down[Index] = Wanted[Index];
					if (Index > 0)
					{
						Down[Index] = FMath::Max(Down[Index], Down[Index - 1] + Height[Layer[Index - 1]] + RowGap);
					}
				}
				for (int32 Index = Layer.Num() - 1; Index >= 0; --Index)
				{
					Up[Index] = Wanted[Index];
					if (Index + 1 < Layer.Num())
					{
						Up[Index] = FMath::Min(Up[Index], Up[Index + 1] - RowGap - Height[Layer[Index]]);
					}
				}
				for (int32 Index = 0; Index < Layer.Num(); ++Index)
				{
					// Floor, not truncation: rounding two neighbours in different directions would eat a unit of their gap.
					const int64 Sum = static_cast<int64>(Down[Index]) + static_cast<int64>(Up[Index]);
					Top[Layer[Index]] = static_cast<int32>(Sum >= 0 ? Sum / 2 : -((-Sum + 1) / 2));
				}
			};

			constexpr int32 AlignmentSweeps = 6;
			for (int32 Sweep = 0; Sweep < AlignmentSweeps; ++Sweep)
			{
				for (int32 LayerIndex = 1; LayerIndex < ColumnCount; ++LayerIndex)
				{
					Align(LayerIndex, /* bByReaders */ true);
				}
				for (int32 LayerIndex = ColumnCount - 2; LayerIndex >= 0; --LayerIndex)
				{
					Align(LayerIndex, /* bByReaders */ false);
				}
			}
		}

		void FState::LayoutLayered()
		{
			FLayeredGraph Layered;
			Layered.SetNum(NodeCount);
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (IsPlaced(Node))
				{
					Layered.Column[Node] = Out.Columns[Node];
					Layered.Height[Node] = HeightOf(Node);
				}
			}
			Layered.Reads = Reads;
			Layered.Readers = Readers;
			Layered.Solve(FMath::Max(0, Options.RowGap));
			Top = Layered.Top;

			// What a hint's offset travels along: the nearest reader.
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (!IsPlaced(Node))
				{
					continue;
				}
				int32 Best = INDEX_NONE;
				for (const int32 Reader : Readers[Node])
				{
					if (IsPlaced(Reader) && (Best == INDEX_NONE || Out.Columns[Reader] > Out.Columns[Best]))
					{
						Best = Reader;
					}
				}
				TreeParent[Node] = Best;
			}
		}

		void FState::ApplyHorizontal()
		{
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (IsPlaced(Node))
				{
					Out.Positions[Node] = FIntPoint(ColumnLeft[Out.Columns[Node]], Top[Node]);
				}
			}
		}

		void FState::ApplyNodeHints()
		{
			HintOffset.Init(FIntPoint::ZeroValue, NodeCount);
			if (!Options.bApplyNodeHints)
			{
				return;
			}

			TArray<FIntPoint> Offset;
			Offset.Init(FIntPoint::ZeroValue, NodeCount);
			TArray<int32> HintedNodes;
			for (const FIRLayoutHint& Hint : Graph.LayoutHints)
			{
				if (!Hint.Kind.Equals(TEXT("Node"), ESearchCase::CaseSensitive))
				{
					continue;
				}
				const int32 Owner = FindDreamShaderIRLayoutOwner(Graph, Hint.Var);
				if (!IsPlaced(Owner) || Owner == Graph.Sink || Out.Hinted[Owner])
				{
					continue;
				}
				Out.Hinted[Owner] = true;
				Offset[Owner] = FIntPoint(Hint.X, Hint.Y) - Out.Positions[Owner];
				HintedNodes.Add(Owner);
			}
			if (HintedNodes.Num() == 0)
			{
				return;
			}

			// A hinted variable takes the expression that made it along: a node follows the nearest hinted node among the
			// readers it hangs under, and a node with none follows the first hint, so that nothing stays behind in the
			// computed frame while the rest of the graph moved to the author's.
			const FIntPoint Fallback = Offset[HintedNodes[0]];
			TArray<bool> Resolved;
			Resolved.Init(false, NodeCount);
			for (int32 Index = Order.Num() - 1; Index >= 0; --Index)
			{
				const int32 Node = Order[Index];
				if (Out.Hinted[Node])
				{
					Resolved[Node] = true;
					continue;
				}
				const int32 Parent = TreeParent[Node];
				if (Parent != INDEX_NONE && Resolved[Parent])
				{
					Offset[Node] = Offset[Parent];
					Resolved[Node] = true;
				}
			}
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (IsPlaced(Node))
				{
					HintOffset[Node] = Resolved[Node] ? Offset[Node] : Fallback;
					Out.Positions[Node] += HintOffset[Node];
				}
			}
		}

		void FState::SnapAndMeasure()
		{
			bool bAny = false;
			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (!IsPlaced(Node))
				{
					continue;
				}
				FIntPoint& Position = Out.Positions[Node];
				// A hinted position is the author's and stays exactly where it was written.
				if (!Out.Hinted[Node])
				{
					Position.X = SnapDown(Position.X, Options.GridSize);
					Position.Y = SnapDown(Position.Y, Options.GridSize);
				}
				if (Node == Graph.Sink)
				{
					continue;
				}
				const FIntPoint Max(Position.X + WidthOf(Node), Position.Y + HeightOf(Node));
				if (!bAny)
				{
					Out.BoundsMin = Position;
					Out.BoundsMax = Max;
					bAny = true;
				}
				else
				{
					Out.BoundsMin.X = FMath::Min(Out.BoundsMin.X, Position.X);
					Out.BoundsMin.Y = FMath::Min(Out.BoundsMin.Y, Position.Y);
					Out.BoundsMax.X = FMath::Max(Out.BoundsMax.X, Max.X);
					Out.BoundsMax.Y = FMath::Max(Out.BoundsMax.Y, Max.Y);
				}
			}
		}

		void FState::BuildRegionBoxes()
		{
			if (!Options.bRegionBoxes || Graph.Regions.Num() == 0)
			{
				return;
			}

			const int32 RegionCount = Graph.Regions.Num();
			TArray<int32> Depth;
			Depth.Init(0, RegionCount);
			for (int32 Region = 0; Region < RegionCount; ++Region)
			{
				int32 Parent = Graph.Regions[Region].Parent;
				for (int32 Guard = 0; Graph.Regions.IsValidIndex(Parent) && Guard < RegionCount; ++Guard)
				{
					++Depth[Region];
					Parent = Graph.Regions[Parent].Parent;
				}
			}

			struct FBox
			{
				bool bValid = false;
				int32 MinX = 0;
				int32 MinY = 0;
				int32 MaxX = 0;
				int32 MaxY = 0;
				int32 DeepestInside = 0;
			};
			TArray<FBox> Boxes;
			Boxes.SetNum(RegionCount);
			for (int32 Region = 0; Region < RegionCount; ++Region)
			{
				Boxes[Region].DeepestInside = Depth[Region];
			}

			for (int32 Node = 0; Node < NodeCount; ++Node)
			{
				if (!IsPlaced(Node) || Node == Graph.Sink || !Graph.Regions.IsValidIndex(Graph.Nodes[Node].Region))
				{
					continue;
				}
				const FIntPoint Min = Out.Positions[Node];
				const FIntPoint Max(Min.X + WidthOf(Node), Min.Y + HeightOf(Node));
				const int32 Innermost = Graph.Nodes[Node].Region;
				// The node's own region and every region around it.
				int32 Region = Innermost;
				for (int32 Guard = 0; Graph.Regions.IsValidIndex(Region) && Guard <= RegionCount; ++Guard)
				{
					FBox& Box = Boxes[Region];
					if (!Box.bValid)
					{
						Box.bValid = true;
						Box.MinX = Min.X;
						Box.MinY = Min.Y;
						Box.MaxX = Max.X;
						Box.MaxY = Max.Y;
					}
					else
					{
						Box.MinX = FMath::Min(Box.MinX, Min.X);
						Box.MinY = FMath::Min(Box.MinY, Min.Y);
						Box.MaxX = FMath::Max(Box.MaxX, Max.X);
						Box.MaxY = FMath::Max(Box.MaxY, Max.Y);
					}
					Box.DeepestInside = FMath::Max(Box.DeepestInside, Depth[Innermost]);
					Region = Graph.Regions[Region].Parent;
				}
			}

			// Outermost first, so that a host drawing them in order leaves the inner boxes on top.
			TArray<int32> RegionOrder;
			for (int32 Region = 0; Region < RegionCount; ++Region)
			{
				if (Boxes[Region].bValid)
				{
					RegionOrder.Add(Region);
				}
			}
			RegionOrder.StableSort([&Depth](const int32 A, const int32 B)
			{
				if (Depth[A] != Depth[B])
				{
					return Depth[A] < Depth[B];
				}
				return A < B;
			});

			for (const int32 Region : RegionOrder)
			{
				const FBox& Box = Boxes[Region];
				// One more ring of padding per level nested inside this region.
				const int32 Rings = 1 + (Box.DeepestInside - Depth[Region]);
				const int32 PadX = Rings * FMath::Max(0, Options.RegionPaddingX);
				const int32 PadY = Rings * FMath::Max(0, Options.RegionPaddingY);
				const int32 Title = Rings * FMath::Max(0, Options.RegionTitleHeight);

				FIRLayoutComment Comment;
				Comment.Title = Graph.Regions[Region].Name;
				Comment.Region = Region;
				Comment.Depth = Depth[Region];
				Comment.bGenerated = true;
				Comment.X = Box.MinX - PadX;
				Comment.Y = Box.MinY - PadY - Title;
				Comment.Width = (Box.MaxX - Box.MinX) + 2 * PadX;
				Comment.Height = (Box.MaxY - Box.MinY) + 2 * PadY + Title;
				Out.Comments.Add(MoveTemp(Comment));
			}
		}

		void FState::PassCommentHints()
		{
			if (!Options.bApplyCommentHints)
			{
				return;
			}
			for (const FIRLayoutHint& Hint : Graph.LayoutHints)
			{
				if (!Hint.Kind.Equals(TEXT("Comment"), ESearchCase::CaseSensitive))
				{
					continue;
				}
				FIRLayoutComment Comment;
				Comment.Title = Hint.Name;
				Comment.X = Hint.X;
				Comment.Y = Hint.Y;
				Comment.Width = Hint.bHasSize ? Hint.W : 0;
				Comment.Height = Hint.bHasSize ? Hint.H : 0;
				Comment.bHasColor = Hint.bHasColor;
				for (int32 Channel = 0; Channel < 4; ++Channel)
				{
					Comment.Color[Channel] = Hint.Color[Channel];
				}
				Out.Comments.Add(MoveTemp(Comment));
			}
		}

		void FState::CollectLongEdges()
		{
			if (Options.LongEdgeColumns <= 0)
			{
				return;
			}

			TArray<FIRValue> Values;
			for (int32 Consumer = 0; Consumer < NodeCount; ++Consumer)
			{
				if (!IsPlaced(Consumer))
				{
					continue;
				}
				Values.Reset();
				FIRGraph::CollectInputValues(Graph.Nodes[Consumer], Values);
				for (const FIRValue& Value : Values)
				{
					if (!IsPlaced(Value.Node) || Value.Node == Consumer)
					{
						continue;
					}
					// A value that crosses two blocks travels through a reroute, and a column of one block says nothing
					// about a column of another.
					if (Out.Blocks.IsValidIndex(Consumer) && Out.Blocks.IsValidIndex(Value.Node) && Out.Blocks[Consumer] != Out.Blocks[Value.Node])
					{
						continue;
					}
					const int32 Span = Out.Columns[Value.Node] - Out.Columns[Consumer];
					const int32 BandA = Out.Bands[Value.Node];
					const int32 BandB = Out.Bands[Consumer];
					const bool bCrossesBands = BandA != INDEX_NONE && BandB != INDEX_NONE && BandA != BandB;
					const bool bFarBands = bCrossesBands && FMath::Abs(BandA - BandB) >= 2;
					if (Span < Options.LongEdgeColumns && !bFarBands)
					{
						continue;
					}
					FIRLayoutLongEdge Edge;
					Edge.Source = Value;
					Edge.Consumer = Consumer;
					Edge.ColumnSpan = Span;
					Edge.bCrossesBands = bCrossesBands;
					Out.LongEdges.Add(Edge);
				}
			}
		}

		void FState::PlaceRoot()
		{
			// Level with what feeds the material; with everything when nothing does.
			int32 MinY = MAX_int32;
			int32 MaxY = MIN_int32;
			if (Graph.Nodes.IsValidIndex(Graph.Sink))
			{
				for (const int32 Read : Reads[Graph.Sink])
				{
					if (IsPlaced(Read))
					{
						MinY = FMath::Min(MinY, Out.Positions[Read].Y);
						MaxY = FMath::Max(MaxY, Out.Positions[Read].Y + HeightOf(Read));
					}
				}
			}
			// In the block style nothing is wired to the material's node: what feeds it arrives through usages that stand
			// beside it, so it is level with the page.
			if (MinY > MaxY || Options.Style == EIRLayoutStyle::Blocks)
			{
				MinY = Out.BoundsMin.Y;
				MaxY = Out.BoundsMax.Y;
			}

			constexpr int32 RootNodeHalfHeight = 240;
			Out.RootPosition.X = SnapDown(Out.BoundsMax.X + FMath::Max(0, Options.SinkGap), Options.GridSize);
			Out.RootPosition.Y = SnapDown((MinY + MaxY) / 2 - RootNodeHalfHeight, Options.GridSize);
			if (IsPlaced(Graph.Sink))
			{
				Out.Positions[Graph.Sink] = Out.RootPosition;
			}
		}

		// ------------------------------------------------------------------------------------------ dumps

		static void AppendXmlEscaped(FString& Out, const FString& Text)
		{
			for (const TCHAR Char : Text)
			{
				switch (Char)
				{
				case TEXT('&'): Out += TEXT("&amp;"); break;
				case TEXT('<'): Out += TEXT("&lt;"); break;
				case TEXT('>'): Out += TEXT("&gt;"); break;
				case TEXT('"'): Out += TEXT("&quot;"); break;
				default: Out.AppendChar(Char); break;
				}
			}
		}

		static const TCHAR* FillOf(const FIRNode& Node)
		{
			switch (Node.Op)
			{
			case EIROp::Parameter:
			case EIROp::TextureParameter:
				return TEXT("#2f5d3a");
			case EIROp::FunctionInput:
			case EIROp::FunctionOutput:
				return TEXT("#5d2f2f");
			case EIROp::Constant:
				return TEXT("#4a4a2a");
			case EIROp::Custom:
			case EIROp::FunctionCall:
				return TEXT("#2f3f6d");
			case EIROp::TextureSample:
				return TEXT("#5a3a6d");
			default:
				return TEXT("#3a3f47");
			}
		}
	}

	const TCHAR* LexToString(const EIRLayoutStyle Style)
	{
		switch (Style)
		{
		case EIRLayoutStyle::Blocks:      return TEXT("Blocks");
		case EIRLayoutStyle::SourceBands: return TEXT("SourceBands");
		case EIRLayoutStyle::Layered:     return TEXT("Layered");
		}
		return TEXT("Blocks");
	}

	bool TryParseIRLayoutStyle(const FString& Text, EIRLayoutStyle& OutStyle)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.Equals(TEXT("Blocks"), ESearchCase::IgnoreCase))
		{
			OutStyle = EIRLayoutStyle::Blocks;
			return true;
		}
		if (Trimmed.Equals(TEXT("SourceBands"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("Bands"), ESearchCase::IgnoreCase))
		{
			OutStyle = EIRLayoutStyle::SourceBands;
			return true;
		}
		if (Trimmed.Equals(TEXT("Layered"), ESearchCase::IgnoreCase))
		{
			OutStyle = EIRLayoutStyle::Layered;
			return true;
		}
		return false;
	}

	FIRLayoutNodeSize EstimateDreamShaderIRNodeSize(const FIRGraph& Graph, const int32 NodeIndex)
	{
		FIRLayoutNodeSize Size;
		if (!Graph.Nodes.IsValidIndex(NodeIndex))
		{
			return Size;
		}
		const FIRNode& Node = Graph.Nodes[NodeIndex];

		// A title bar, then a row per pin; pins on the left and on the right share rows.
		int32 InputRows = 0;
		for (const FIRValue& Operand : Node.Operands)
		{
			InputRows += Operand.Node != INDEX_NONE ? 1 : 0;
		}
		InputRows += Node.Inputs.Num();
		int32 LongestPin = 0;
		for (const FIRInput& Input : Node.Inputs)
		{
			LongestPin = FMath::Max(LongestPin, Input.Pin.Len());
		}
		for (const FString& OutputName : Node.OutputNames)
		{
			LongestPin = FMath::Max(LongestPin, OutputName.Len());
		}
		const int32 Rows = FMath::Max3(1, InputRows, Node.Outputs.Num());
		const int32 TitleLength = LayoutDetail::DisplayNameOf(Node).Len();

		constexpr int32 TitleHeight = 40;
		constexpr int32 RowHeight = 26;
		constexpr int32 CharWidth = 8;
		Size.Width = FMath::Clamp(FMath::Max(64 + TitleLength * CharWidth, 96 + 2 * LongestPin * CharWidth), 144, 480);
		Size.Height = TitleHeight + Rows * RowHeight;

		// A constant shows its value under the title, a custom node its code preview.
		if (Node.Op == EIROp::Custom)
		{
			Size.Width = FMath::Max(Size.Width, 320);
		}
		return Size;
	}

	FIRLayoutNodeSize EstimateDreamShaderIRRerouteSize(const FString& Name)
	{
		// A named reroute is its name and one pin.
		FIRLayoutNodeSize Size;
		Size.Width = FMath::Clamp(72 + Name.Len() * 8, 128, 400);
		Size.Height = 48;
		return Size;
	}

	int32 FindDreamShaderIRLayoutOwner(const FIRGraph& Graph, const FString& VariableName)
	{
		if (VariableName.IsEmpty())
		{
			return INDEX_NONE;
		}

		TArray<int32> Named;
		for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
		{
			if (Graph.Nodes[Node].DebugName.Equals(VariableName, ESearchCase::CaseSensitive))
			{
				Named.Add(Node);
			}
		}
		if (Named.Num() == 0)
		{
			return INDEX_NONE;
		}

		// The one none of whose readers carries the name: every other one is a part of the expression that made it.
		TArray<bool> ReadByNamesake;
		ReadByNamesake.Init(false, Graph.Nodes.Num());
		TArray<FIRValue> Values;
		for (const int32 Reader : Named)
		{
			Values.Reset();
			FIRGraph::CollectInputValues(Graph.Nodes[Reader], Values);
			for (const FIRValue& Value : Values)
			{
				if (Graph.Nodes.IsValidIndex(Value.Node) && Value.Node != Reader)
				{
					ReadByNamesake[Value.Node] = true;
				}
			}
		}
		int32 Owner = INDEX_NONE;
		for (const int32 Node : Named)
		{
			if (!ReadByNamesake[Node])
			{
				// Several would be two statements that named one value; the later one is what the variable holds.
				Owner = Node;
			}
		}
		return Owner != INDEX_NONE ? Owner : Named.Last();
	}

	void LayoutDreamShaderIRGraph(const FIRGraph& Graph, const FIRLayoutOptions& Options, FIRLayoutResult& OutResult)
	{
		OutResult = FIRLayoutResult();

		LayoutDetail::FState State(Graph, Options, OutResult);
		State.BuildAdjacency();
		State.AssignColumns();
		if (State.Order.Num() == 0)
		{
			return;
		}
		State.AssignSizesAndColumnGeometry();

		const bool bBlocks = Options.Style == EIRLayoutStyle::Blocks;
		switch (Options.Style)
		{
		case EIRLayoutStyle::Layered:
			State.LayoutLayered();
			State.ApplyHorizontal();
			break;
		case EIRLayoutStyle::SourceBands:
			State.LayoutBands();
			State.ApplyHorizontal();
			break;
		case EIRLayoutStyle::Blocks:
		default:
			// Places every node itself: a block has columns of its own.
			State.LayoutBlocks();
			break;
		}

		State.ApplyNodeHints();
		State.SnapAndMeasure();
		if (bBlocks)
		{
			State.FinishBlocks();
		}
		else
		{
			State.BuildRegionBoxes();
		}
		State.PassCommentHints();
		State.CollectLongEdges();
		State.PlaceRoot();
		if (bBlocks)
		{
			State.PlaceBlockOutputs();
		}
	}

	FString DumpDreamShaderIRLayoutSvg(const FIRGraph& Graph, const FIRLayoutResult& Layout, const FString& Title)
	{
		constexpr int32 Margin = 80;
		constexpr int32 RootWidth = 240;
		constexpr int32 RootHeight = 480;

		int32 MinX = Layout.BoundsMin.X;
		int32 MinY = Layout.BoundsMin.Y;
		int32 MaxX = Layout.BoundsMax.X;
		int32 MaxY = Layout.BoundsMax.Y;
		for (const FIRLayoutComment& Comment : Layout.Comments)
		{
			MinX = FMath::Min(MinX, Comment.X);
			MinY = FMath::Min(MinY, Comment.Y);
			MaxX = FMath::Max(MaxX, Comment.X + Comment.Width);
			MaxY = FMath::Max(MaxY, Comment.Y + Comment.Height);
		}
		const bool bHasSink = Graph.Nodes.IsValidIndex(Graph.Sink) && Layout.Placed.IsValidIndex(Graph.Sink) && Layout.Placed[Graph.Sink];
		if (bHasSink)
		{
			MinY = FMath::Min(MinY, Layout.RootPosition.Y);
			MaxX = FMath::Max(MaxX, Layout.RootPosition.X + RootWidth);
			MaxY = FMath::Max(MaxY, Layout.RootPosition.Y + RootHeight);
		}
		for (const FIRLayoutOutputRoute& Route : Layout.OutputRoutes)
		{
			MinY = FMath::Min(MinY, Route.UsagePosition.Y);
			MaxY = FMath::Max(MaxY, Route.UsagePosition.Y + Route.UsageSize.Height);
		}

		// The block style: who reads a value through which reroute.
		struct FStub
		{
			FIntPoint Position = FIntPoint::ZeroValue;
			FIRLayoutNodeSize Size;
		};
		const auto StubKey = [](const int32 A, const int32 B, const int32 C)
		{
			return FString::Printf(TEXT("%d/%d/%d"), A, B, C);
		};
		TMap<FString, FStub> UseStubs;
		TMap<FString, FStub> RouteStubs;
		for (const FIRLayoutBridge& Bridge : Layout.Bridges)
		{
			for (const FIRLayoutBridgeUse& Use : Bridge.Uses)
			{
				UseStubs.Add(StubKey(Bridge.Source.Node, Bridge.Source.Output, Use.Block), FStub{ Use.Position, Use.Size });
			}
		}
		for (const FIRLayoutOutputRoute& Route : Layout.OutputRoutes)
		{
			RouteStubs.Add(StubKey(Route.Consumer, Route.InputIndex, 0), FStub{ Route.UsagePosition, Route.UsageSize });
		}
		MinX -= Margin;
		MinY -= Margin + 40;
		MaxX += Margin;
		MaxY += Margin;

		FString Svg;
		Svg += FString::Printf(
			TEXT("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%d %d %d %d\" font-family=\"Consolas, monospace\" font-size=\"14\">\n"),
			MinX, MinY, MaxX - MinX, MaxY - MinY);
		Svg += FString::Printf(TEXT("<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#1b1d21\"/>\n"), MinX, MinY, MaxX - MinX, MaxY - MinY);
		Svg += FString::Printf(TEXT("<text x=\"%d\" y=\"%d\" fill=\"#c8ccd4\" font-size=\"20\">"), MinX + 24, MinY + 36);
		LayoutDetail::AppendXmlEscaped(Svg, Title);
		Svg += TEXT("</text>\n");

		for (const FIRLayoutComment& Comment : Layout.Comments)
		{
			Svg += FString::Printf(
				TEXT("<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"6\" fill=\"#24405a\" fill-opacity=\"0.25\" stroke=\"#4f7aa3\"/>\n"),
				Comment.X, Comment.Y, FMath::Max(Comment.Width, 1), FMath::Max(Comment.Height, 1));
			Svg += FString::Printf(TEXT("<text x=\"%d\" y=\"%d\" fill=\"#9fc4e8\" font-size=\"18\">"), Comment.X + 12, Comment.Y + 26);
			LayoutDetail::AppendXmlEscaped(Svg, Comment.Title);
			Svg += TEXT("</text>\n");
		}

		// Wires under the nodes.
		TArray<FIRValue> Values;
		for (int32 Consumer = 0; Consumer < Graph.Nodes.Num(); ++Consumer)
		{
			if (!Layout.Placed.IsValidIndex(Consumer) || !Layout.Placed[Consumer])
			{
				continue;
			}
			Values.Reset();
			FIRGraph::CollectInputValues(Graph.Nodes[Consumer], Values);
			const bool bIsSink = Consumer == Graph.Sink;
			const int32 ToX = bIsSink ? Layout.RootPosition.X : Layout.Positions[Consumer].X;
			const int32 ToHeight = bIsSink ? RootHeight : Layout.Sizes[Consumer].Height;
			const int32 ToTop = bIsSink ? Layout.RootPosition.Y : Layout.Positions[Consumer].Y;
			int32 Slot = 0;
			const int32 SlotCount = FMath::Max(1, Values.Num());
			for (const FIRValue& Value : Values)
			{
				const int32 ThisSlot = Slot++;
				if (!Layout.Placed.IsValidIndex(Value.Node) || !Layout.Placed[Value.Node] || Value.Node == Consumer)
				{
					continue;
				}
				int32 FromX = Layout.Positions[Value.Node].X + Layout.Sizes[Value.Node].Width;
				int32 FromY = Layout.Positions[Value.Node].Y + Layout.Sizes[Value.Node].Height / 2;
				// Through a reroute where the layout counted on one: the wire starts at the usage, not at the value.
				const FStub* Stub = RouteStubs.Find(StubKey(Consumer, ThisSlot, 0));
				if (!Stub && Layout.Blocks.IsValidIndex(Consumer) && Layout.Blocks.IsValidIndex(Value.Node)
					&& Layout.Blocks[Consumer] != Layout.Blocks[Value.Node])
				{
					Stub = UseStubs.Find(StubKey(Value.Node, Value.Output, Layout.Blocks[Consumer]));
				}
				if (Stub)
				{
					FromX = Stub->Position.X + Stub->Size.Width;
					FromY = Stub->Position.Y + Stub->Size.Height / 2;
				}
				const int32 ToY = ToTop + ((2 * ThisSlot + 1) * ToHeight) / (2 * SlotCount);
				const int32 Bend = FMath::Max(40, (ToX - FromX) / 2);
				Svg += FString::Printf(
					TEXT("<path d=\"M %d %d C %d %d, %d %d, %d %d\" fill=\"none\" stroke=\"#8a929e\" stroke-opacity=\"0.7\" stroke-width=\"1.5\"/>\n"),
					FromX, FromY, FromX + Bend, FromY, ToX - Bend, ToY, ToX, ToY);
			}
		}

		for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
		{
			if (!Layout.Placed.IsValidIndex(Node) || !Layout.Placed[Node] || Node == Graph.Sink)
			{
				continue;
			}
			const FIntPoint Position = Layout.Positions[Node];
			const FIRLayoutNodeSize Size = Layout.Sizes[Node];
			Svg += FString::Printf(
				TEXT("<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"5\" fill=\"%s\" stroke=\"%s\"/>\n"),
				Position.X, Position.Y, Size.Width, Size.Height,
				LayoutDetail::FillOf(Graph.Nodes[Node]),
				Layout.Hinted[Node] ? TEXT("#e8c15a") : TEXT("#0d0e10"));
			Svg += FString::Printf(TEXT("<text x=\"%d\" y=\"%d\" fill=\"#e6e8eb\">"), Position.X + 8, Position.Y + 24);
			LayoutDetail::AppendXmlEscaped(Svg, LayoutDetail::DisplayNameOf(Graph.Nodes[Node]));
			Svg += TEXT("</text>\n");
		}

		// The reroutes of the block style: a declaration takes its wire from the value beside it; a usage starts one.
		const auto DrawReroute = [&Svg](const FIntPoint& Position, const FIRLayoutNodeSize& Size, const FString& Name, const TCHAR* Fill)
		{
			Svg += FString::Printf(
				TEXT("<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"%d\" fill=\"%s\" stroke=\"#0d0e10\"/>\n"),
				Position.X, Position.Y, Size.Width, Size.Height, FMath::Min(Size.Height / 2, 24), Fill);
			Svg += FString::Printf(TEXT("<text x=\"%d\" y=\"%d\" fill=\"#e6e8eb\" font-size=\"12\">"), Position.X + 12, Position.Y + Size.Height / 2 + 4);
			LayoutDetail::AppendXmlEscaped(Svg, Name);
			Svg += TEXT("</text>\n");
		};
		const auto DrawDeclarationWire = [&Svg, &Layout](const FIRValue& Source, const FIntPoint& Position, const FIRLayoutNodeSize& Size)
		{
			if (!Layout.Placed.IsValidIndex(Source.Node) || !Layout.Placed[Source.Node])
			{
				return;
			}
			const int32 FromX = Layout.Positions[Source.Node].X + Layout.Sizes[Source.Node].Width;
			const int32 FromY = Layout.Positions[Source.Node].Y + Layout.Sizes[Source.Node].Height / 2;
			const int32 ToX = Position.X;
			const int32 ToY = Position.Y + Size.Height / 2;
			const int32 Bend = FMath::Max(24, (ToX - FromX) / 2);
			Svg += FString::Printf(
				TEXT("<path d=\"M %d %d C %d %d, %d %d, %d %d\" fill=\"none\" stroke=\"#5fb3a8\" stroke-opacity=\"0.8\" stroke-width=\"1.5\"/>\n"),
				FromX, FromY, FromX + Bend, FromY, ToX - Bend, ToY, ToX, ToY);
		};
		for (const FIRLayoutBridge& Bridge : Layout.Bridges)
		{
			if (!Bridge.bClone)
			{
				DrawDeclarationWire(Bridge.Source, Bridge.DeclarationPosition, Bridge.DeclarationSize);
				DrawReroute(Bridge.DeclarationPosition, Bridge.DeclarationSize, Bridge.Name, TEXT("#1f5d57"));
			}
			for (const FIRLayoutBridgeUse& Use : Bridge.Uses)
			{
				DrawReroute(
					Use.Position, Use.Size,
					Bridge.bClone ? LayoutDetail::DisplayNameOf(Graph.Nodes[Bridge.Source.Node]) : Bridge.Name,
					Bridge.bClone ? TEXT("#4a4a2a") : TEXT("#2b7a72"));
			}
		}
		for (const FIRLayoutOutputRoute& Route : Layout.OutputRoutes)
		{
			FString Name = TEXT("output");
			if (Graph.Nodes.IsValidIndex(Route.Consumer))
			{
				const FIRNode& ConsumerNode = Graph.Nodes[Route.Consumer];
				const int32 NamedInput = Route.InputIndex - ConsumerNode.Operands.Num();
				if (Route.Consumer == Graph.Sink && ConsumerNode.Inputs.IsValidIndex(NamedInput))
				{
					Name = ConsumerNode.Inputs[NamedInput].Pin;
				}
				else if (const FIRProperty* OutputName = ConsumerNode.FindProperty(TEXT("OutputName")))
				{
					Name = OutputName->Value.ToString();
				}
			}
			DrawDeclarationWire(Route.Source, Route.DeclarationPosition, Route.DeclarationSize);
			DrawReroute(Route.DeclarationPosition, Route.DeclarationSize, Name, TEXT("#5d3a1f"));
			DrawReroute(Route.UsagePosition, Route.UsageSize, Name, TEXT("#7a4f2b"));
		}

		if (bHasSink)
		{
			Svg += FString::Printf(
				TEXT("<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"5\" fill=\"#6d5a2f\" stroke=\"#0d0e10\"/>\n"),
				Layout.RootPosition.X, Layout.RootPosition.Y, RootWidth, RootHeight);
			Svg += FString::Printf(TEXT("<text x=\"%d\" y=\"%d\" fill=\"#e6e8eb\">material</text>\n"), Layout.RootPosition.X + 8, Layout.RootPosition.Y + 24);
		}

		Svg += TEXT("</svg>\n");
		return Svg;
	}

	FString DumpDreamShaderIRLayoutJson(const FIRGraph& Graph, const FIRLayoutResult& Layout, const EIRLayoutStyle Style)
	{
		FString Json;
		Json += TEXT("{\n");
		Json += TEXT("  \"schema\": \"dreamshader-ir-layout\",\n");
		Json += TEXT("  \"version\": 1,\n");
		Json += FString::Printf(TEXT("  \"style\": \"%s\",\n"), LexToString(Style));
		Json += FString::Printf(TEXT("  \"columns\": %d,\n"), Layout.ColumnCount);
		Json += FString::Printf(TEXT("  \"bands\": %d,\n"), Layout.BandCount);
		Json += FString::Printf(
			TEXT("  \"bounds\": [%d, %d, %d, %d],\n"),
			Layout.BoundsMin.X, Layout.BoundsMin.Y, Layout.BoundsMax.X, Layout.BoundsMax.Y);
		Json += FString::Printf(TEXT("  \"root\": [%d, %d],\n"), Layout.RootPosition.X, Layout.RootPosition.Y);

		Json += TEXT("  \"nodes\": [");
		bool bFirst = true;
		for (int32 Node = 0; Node < Graph.Nodes.Num(); ++Node)
		{
			if (!Layout.Placed.IsValidIndex(Node) || !Layout.Placed[Node])
			{
				continue;
			}
			Json += bFirst ? TEXT("\n") : TEXT(",\n");
			bFirst = false;
			Json += FString::Printf(TEXT("    { \"node\": %d, \"op\": \"%s\", \"name\": \""), Node, LexToString(Graph.Nodes[Node].Op));
			Private::AppendIRJsonEscaped(Json, LayoutDetail::DisplayNameOf(Graph.Nodes[Node]));
			Json += FString::Printf(
				TEXT("\", \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d, \"column\": %d, \"band\": %d, \"block\": %d, \"hinted\": %s }"),
				Layout.Positions[Node].X,
				Layout.Positions[Node].Y,
				Layout.Sizes[Node].Width,
				Layout.Sizes[Node].Height,
				Layout.Columns[Node],
				Layout.Bands[Node],
				Layout.Blocks.IsValidIndex(Node) ? Layout.Blocks[Node] : INDEX_NONE,
				Layout.Hinted[Node] ? TEXT("true") : TEXT("false"));
		}
		Json += bFirst ? TEXT("],\n") : TEXT("\n  ],\n");

		Json += TEXT("  \"comments\": [");
		bFirst = true;
		for (const FIRLayoutComment& Comment : Layout.Comments)
		{
			Json += bFirst ? TEXT("\n") : TEXT(",\n");
			bFirst = false;
			Json += TEXT("    { \"title\": \"");
			Private::AppendIRJsonEscaped(Json, Comment.Title);
			Json += FString::Printf(
				TEXT("\", \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d, \"region\": %d, \"depth\": %d, \"generated\": %s }"),
				Comment.X, Comment.Y, Comment.Width, Comment.Height, Comment.Region, Comment.Depth,
				Comment.bGenerated ? TEXT("true") : TEXT("false"));
		}
		Json += bFirst ? TEXT("],\n") : TEXT("\n  ],\n");

		Json += TEXT("  \"blocks\": [");
		bFirst = true;
		for (const FIRLayoutBlock& Block : Layout.BlockList)
		{
			Json += bFirst ? TEXT("\n") : TEXT(",\n");
			bFirst = false;
			Json += TEXT("    { \"title\": \"");
			Private::AppendIRJsonEscaped(Json, Block.Title);
			Json += FString::Printf(
				TEXT("\", \"region\": %d, \"nodes\": %d, \"lines\": [%d, %d], \"outputs\": %s, \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d }"),
				Block.Region, Block.Nodes.Num(), Block.FirstLine, Block.LastLine,
				Block.bOutputs ? TEXT("true") : TEXT("false"),
				Block.X, Block.Y, Block.Width, Block.Height);
		}
		Json += bFirst ? TEXT("],\n") : TEXT("\n  ],\n");

		Json += TEXT("  \"bridges\": [");
		bFirst = true;
		for (const FIRLayoutBridge& Bridge : Layout.Bridges)
		{
			Json += bFirst ? TEXT("\n") : TEXT(",\n");
			bFirst = false;
			Json += TEXT("    { \"name\": \"");
			Private::AppendIRJsonEscaped(Json, Bridge.Name);
			Json += FString::Printf(
				TEXT("\", \"node\": %d, \"output\": %d, \"block\": %d, \"clone\": %s, \"declaration\": [%d, %d], \"uses\": ["),
				Bridge.Source.Node, Bridge.Source.Output, Bridge.SourceBlock,
				Bridge.bClone ? TEXT("true") : TEXT("false"),
				Bridge.DeclarationPosition.X, Bridge.DeclarationPosition.Y);
			for (int32 UseIndex = 0; UseIndex < Bridge.Uses.Num(); ++UseIndex)
			{
				const FIRLayoutBridgeUse& Use = Bridge.Uses[UseIndex];
				Json += FString::Printf(
					TEXT("%s{ \"block\": %d, \"x\": %d, \"y\": %d, \"readers\": %d }"),
					UseIndex > 0 ? TEXT(", ") : TEXT(""), Use.Block, Use.Position.X, Use.Position.Y, Use.Consumers.Num());
			}
			Json += TEXT("] }");
		}
		Json += bFirst ? TEXT("],\n") : TEXT("\n  ],\n");

		Json += TEXT("  \"outputRoutes\": [");
		bFirst = true;
		for (const FIRLayoutOutputRoute& Route : Layout.OutputRoutes)
		{
			Json += bFirst ? TEXT("\n") : TEXT(",\n");
			bFirst = false;
			Json += FString::Printf(
				TEXT("    { \"consumer\": %d, \"input\": %d, \"node\": %d, \"output\": %d, \"declaration\": [%d, %d], \"usage\": [%d, %d] }"),
				Route.Consumer, Route.InputIndex, Route.Source.Node, Route.Source.Output,
				Route.DeclarationPosition.X, Route.DeclarationPosition.Y, Route.UsagePosition.X, Route.UsagePosition.Y);
		}
		Json += bFirst ? TEXT("],\n") : TEXT("\n  ],\n");

		Json += FString::Printf(TEXT("  \"longEdges\": %d\n"), Layout.LongEdges.Num());
		Json += TEXT("}\n");
		return Json;
	}
}
