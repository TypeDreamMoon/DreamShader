// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The block style of LayoutDreamShaderIRGraph: the graph as a page of boxes with no wire between two of them.
//
//   1. Bands, as SourceBands makes them: every node belongs to the first statement that needs it, and the statements
//      are in source order.
//   2. Leaves: consecutive bands of one innermost `#pragma region` (or of none) are one leaf, cut where a leaf would
//      pass FIRLayoutOptions::BlockMaxNodes. A function's outputs are a leaf of their own, to the right of the page.
//   3. Bridges: a value a node of another leaf reads. It leaves its leaf through a named reroute -- one declaration
//      beside the value, one usage per leaf that reads it -- and a constant is repeated instead. What drives the
//      material or a function output leaves the same way, through the reroute pair the host already makes for it.
//   4. Every leaf is one small layered drawing of its nodes and of the reroutes on its edges.
//   5. The page: a region that holds more than one leaf is a box around them, packed like words in a paragraph; the
//      top level is packed the same way. Nested boxes cannot overlap, because a box is made from what it holds.
//
// The boxes themselves are drawn last (FinishBlocks), from where the nodes ended up, so that a `#pragma layout(Node,
// ...)` hint moves a box with its node instead of leaving it behind.

#include "IRLayoutInternal.h"

#include "IR/IRCoreOps.h"

namespace UE::DreamShader::IR::LayoutDetail
{
	namespace BlockDetail
	{
		static uint64 ValueKey(const FIRValue& Value)
		{
			return (static_cast<uint64>(static_cast<uint32>(Value.Node)) << 32) | static_cast<uint32>(Value.Output);
		}

		static FString SanitizeRerouteName(const FString& Text)
		{
			FString Result;
			Result.Reserve(Text.Len());
			for (const TCHAR Char : Text)
			{
				Result.AppendChar(FChar::IsAlnum(Char) || Char == TEXT('_') ? Char : TEXT('_'));
			}
			return Result;
		}

		/**
		 * Words in a paragraph: boxes left to right until the line is full, then a new line under the tallest of the last.
		 * The line is as long as makes the whole come out Aspect times wider than tall, and never shorter than its
		 * widest box.
		 */
		static FIntPoint PackShelves(TArray<FBox>& Boxes, const TArray<int32>& Children, const int32 Gap, const float Aspect)
		{
			int64 Area = 0;
			int32 Widest = 0;
			for (const int32 Child : Children)
			{
				Area += static_cast<int64>(Boxes[Child].Size.X + Gap) * static_cast<int64>(Boxes[Child].Size.Y + Gap);
				Widest = FMath::Max(Widest, Boxes[Child].Size.X);
			}
			const int32 LineWidth = FMath::Max(Widest, static_cast<int32>(FMath::Sqrt(static_cast<double>(Area) * FMath::Max(0.25f, Aspect))));

			FIntPoint Extent = FIntPoint::ZeroValue;
			int32 CursorX = 0;
			int32 CursorY = 0;
			int32 LineHeight = 0;
			for (const int32 Child : Children)
			{
				FBox& Box = Boxes[Child];
				if (CursorX > 0 && CursorX + Box.Size.X > LineWidth)
				{
					CursorX = 0;
					CursorY += LineHeight + Gap;
					LineHeight = 0;
				}
				Box.Position = FIntPoint(CursorX, CursorY);
				CursorX += Box.Size.X + Gap;
				LineHeight = FMath::Max(LineHeight, Box.Size.Y);
				Extent.X = FMath::Max(Extent.X, Box.Position.X + Box.Size.X);
				Extent.Y = FMath::Max(Extent.Y, Box.Position.Y + Box.Size.Y);
			}
			return Extent;
		}
	}

	void FState::LayoutBlocks()
	{
		using namespace BlockDetail;

		FBlockScratch& Scratch = BlockScratch;
		Scratch = FBlockScratch();
		TArray<FLeaf>& Leaves = Scratch.Leaves;
		TArray<FEndpoint>& Endpoints = Scratch.Endpoints;

		AssignBands();
		Out.Blocks.Init(INDEX_NONE, NodeCount);

		// ----- region depths, and a parent chain that loops cut short
		const int32 RegionCount = Graph.Regions.Num();
		Scratch.RegionDepth.Init(0, RegionCount);
		for (int32 Region = 0; Region < RegionCount; ++Region)
		{
			int32 Parent = Graph.Regions[Region].Parent;
			for (int32 Guard = 0; Graph.Regions.IsValidIndex(Parent) && Guard < RegionCount; ++Guard)
			{
				++Scratch.RegionDepth[Region];
				Parent = Graph.Regions[Parent].Parent;
			}
		}

		// ----- leaves
		TArray<int32> BandNodeCount;
		BandNodeCount.Init(0, Out.BandCount);
		for (int32 Node = 0; Node < NodeCount; ++Node)
		{
			if (IsPlaced(Node) && Out.Bands[Node] != INDEX_NONE)
			{
				++BandNodeCount[Out.Bands[Node]];
			}
		}

		TArray<int32> LeafOfBand;
		LeafOfBand.Init(INDEX_NONE, Out.BandCount);
		int32 OutputsLeaf = INDEX_NONE;
		int32 CurrentLeaf = INDEX_NONE;
		const int32 MaxNodes = FMath::Max(1, Options.BlockMaxNodes);
		for (int32 Band = 0; Band < Out.BandCount; ++Band)
		{
			const int32 Root = BandRootNode.IsValidIndex(Band) ? BandRootNode[Band] : INDEX_NONE;
			if (!Graph.Nodes.IsValidIndex(Root))
			{
				continue;
			}
			if (Graph.Nodes[Root].Op == EIROp::FunctionOutput)
			{
				if (OutputsLeaf == INDEX_NONE)
				{
					OutputsLeaf = Leaves.AddDefaulted();
					Leaves[OutputsLeaf].bOutputs = true;
				}
				LeafOfBand[Band] = OutputsLeaf;
				continue;
			}

			const int32 Region = Graph.Regions.IsValidIndex(Graph.Nodes[Root].Region) ? Graph.Nodes[Root].Region : INDEX_NONE;
			const bool bContinues = Leaves.IsValidIndex(CurrentLeaf)
				&& Leaves[CurrentLeaf].Region == Region
				&& Leaves[CurrentLeaf].Nodes.Num() + BandNodeCount[Band] <= MaxNodes;
			if (!bContinues)
			{
				CurrentLeaf = Leaves.AddDefaulted();
				Leaves[CurrentLeaf].Region = Region;
			}
			LeafOfBand[Band] = CurrentLeaf;
			// Counted now, filled below: the cut is by what the leaf WILL hold.
			for (int32 Count = 0; Count < BandNodeCount[Band]; ++Count)
			{
				Leaves[CurrentLeaf].Nodes.Add(INDEX_NONE);
			}

			FLeaf& Leaf = Leaves[CurrentLeaf];
			Leaf.FirstBand = FMath::Min(Leaf.FirstBand, Band);
			const FSourceKey Key = SourceKeyOf(Graph.Nodes[Root].Source);
			if (Key.IsKnown())
			{
				Leaf.FirstLine = Leaf.FirstLine == 0 ? Key.Line : FMath::Min(Leaf.FirstLine, Key.Line);
				Leaf.LastLine = FMath::Max(Leaf.LastLine, Key.Line);
			}
			if (!Graph.Nodes[Root].DebugName.IsEmpty())
			{
				if (Leaf.FirstName.IsEmpty())
				{
					Leaf.FirstName = Graph.Nodes[Root].DebugName;
				}
				Leaf.LastName = Graph.Nodes[Root].DebugName;
			}
		}
		for (FLeaf& Leaf : Leaves)
		{
			Leaf.Nodes.Reset();
		}
		for (int32 Node = 0; Node < NodeCount; ++Node)
		{
			if (!IsPlaced(Node) || Node == Graph.Sink || Out.Bands[Node] == INDEX_NONE)
			{
				continue;
			}
			const int32 Leaf = LeafOfBand[Out.Bands[Node]];
			if (Leaves.IsValidIndex(Leaf))
			{
				Out.Blocks[Node] = Leaf;
				Leaves[Leaf].Nodes.Add(Node);
				if (Leaves[Leaf].bOutputs)
				{
					Leaves[Leaf].FirstBand = FMath::Min(Leaves[Leaf].FirstBand, Out.Bands[Node]);
				}
			}
		}

		// ----- bridges and output routes
		Out.Bridges.Reset();
		Out.OutputRoutes.Reset();
		{
			TMap<uint64, int32> BridgeByValue;
			TArray<FIRValue> Values;
			for (int32 Consumer = 0; Consumer < NodeCount; ++Consumer)
			{
				if (!IsPlaced(Consumer))
				{
					continue;
				}
				const bool bIsSink = Consumer == Graph.Sink;
				const bool bIsOutput = Graph.Nodes[Consumer].Op == EIROp::FunctionOutput;
				if (!bIsSink && Out.Blocks[Consumer] == INDEX_NONE)
				{
					continue;
				}

				Values.Reset();
				FIRGraph::CollectInputValues(Graph.Nodes[Consumer], Values);
				for (int32 InputIndex = 0; InputIndex < Values.Num(); ++InputIndex)
				{
					const FIRValue& Value = Values[InputIndex];
					if (!IsPlaced(Value.Node) || Value.Node == Consumer || Out.Blocks[Value.Node] == INDEX_NONE)
					{
						continue;
					}
					if (bIsSink || bIsOutput)
					{
						FIRLayoutOutputRoute& Route = Out.OutputRoutes.AddDefaulted_GetRef();
						Route.Consumer = Consumer;
						Route.InputIndex = InputIndex;
						Route.Source = Value;
						continue;
					}
					if (Out.Blocks[Value.Node] == Out.Blocks[Consumer])
					{
						continue;
					}

					int32 BridgeIndex = INDEX_NONE;
					if (const int32* Existing = BridgeByValue.Find(ValueKey(Value)))
					{
						BridgeIndex = *Existing;
					}
					else
					{
						BridgeIndex = Out.Bridges.AddDefaulted();
						BridgeByValue.Add(ValueKey(Value), BridgeIndex);
						FIRLayoutBridge& Bridge = Out.Bridges[BridgeIndex];
						Bridge.Source = Value;
						Bridge.SourceBlock = Out.Blocks[Value.Node];
						Bridge.bClone = Graph.Nodes[Value.Node].Op == EIROp::Constant;
					}

					FIRLayoutBridge& Bridge = Out.Bridges[BridgeIndex];
					FIRLayoutBridgeUse* Use = Bridge.Uses.FindByPredicate([this, Consumer](const FIRLayoutBridgeUse& Candidate)
					{
						return Candidate.Block == Out.Blocks[Consumer];
					});
					if (!Use)
					{
						Use = &Bridge.Uses.AddDefaulted_GetRef();
						Use->Block = Out.Blocks[Consumer];
					}
					Use->Consumers.AddUnique(Consumer);
				}
			}
		}

		// Consumers were walked in ascending order, so the bridges are in the order their first reader comes; sorted by
		// the value instead, the order is a property of the graph and not of who happens to read first.
		Out.Bridges.StableSort([](const FIRLayoutBridge& A, const FIRLayoutBridge& B)
		{
			if (A.Source.Node != B.Source.Node)
			{
				return A.Source.Node < B.Source.Node;
			}
			return A.Source.Output < B.Source.Output;
		});
		{
			// A statement gives its name to every node of its expression, so several bridged values may carry one name.
			// The plain name goes to the value the variable holds; a part of the expression that made it gets a number
			// behind it. A repeated constant is no reroute and takes no name.
			TMap<FString, int32> OwnerOfName;
			TSet<FString> TakenNames;
			int32 SharedIndex = 0;
			const auto BaseNameOf = [this](const FIRLayoutBridge& Bridge) -> FString
			{
				const FIRNode& SourceNode = Graph.Nodes[Bridge.Source.Node];
				if (SourceNode.DebugName.IsEmpty())
				{
					return FString();
				}
				FString Base = SanitizeRerouteName(SourceNode.DebugName);
				if (Bridge.Source.Output > 0 && SourceNode.OutputNames.IsValidIndex(Bridge.Source.Output)
					&& !SourceNode.OutputNames[Bridge.Source.Output].IsEmpty())
				{
					Base += TEXT("_") + SanitizeRerouteName(SourceNode.OutputNames[Bridge.Source.Output]);
				}
				else if (Bridge.Source.Output > 0)
				{
					Base += FString::Printf(TEXT("_%d"), Bridge.Source.Output);
				}
				return Base;
			};
			const auto IsOwner = [this, &OwnerOfName](const FIRLayoutBridge& Bridge) -> bool
			{
				const FString& DebugName = Graph.Nodes[Bridge.Source.Node].DebugName;
				if (DebugName.IsEmpty())
				{
					return false;
				}
				const int32* Known = OwnerOfName.Find(DebugName);
				const int32 Owner = Known ? *Known : OwnerOfName.Add(DebugName, FindDreamShaderIRLayoutOwner(Graph, DebugName));
				return Owner == Bridge.Source.Node;
			};

			for (FIRLayoutBridge& Bridge : Out.Bridges)
			{
				Bridge.Uses.StableSort([](const FIRLayoutBridgeUse& A, const FIRLayoutBridgeUse& B) { return A.Block < B.Block; });
			}
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (FIRLayoutBridge& Bridge : Out.Bridges)
				{
					if (Bridge.bClone || !Bridge.Name.IsEmpty() || (Pass == 0 && !IsOwner(Bridge)))
					{
						continue;
					}
					FString Base = BaseNameOf(Bridge);
					if (Base.IsEmpty())
					{
						Base = FString::Printf(TEXT("Shared_%d"), SharedIndex++);
					}
					FString Name = TEXT("DS_") + Base;
					for (int32 Suffix = 2; TakenNames.Contains(Name); ++Suffix)
					{
						Name = FString::Printf(TEXT("DS_%s_%d"), *Base, Suffix);
					}
					TakenNames.Add(Name);
					Bridge.Name = MoveTemp(Name);
				}
			}
		}

		// ----- the reroutes as things that take room
		for (int32 BridgeIndex = 0; BridgeIndex < Out.Bridges.Num(); ++BridgeIndex)
		{
			FIRLayoutBridge& Bridge = Out.Bridges[BridgeIndex];
			const FIRLayoutNodeSize RerouteSize = EstimateDreamShaderIRRerouteSize(Bridge.Name);
			if (!Bridge.bClone)
			{
				Bridge.DeclarationSize = RerouteSize;
				FEndpoint& Declaration = Endpoints.AddDefaulted_GetRef();
				Declaration.Kind = EEndpointKind::BridgeDeclaration;
				Declaration.Index = BridgeIndex;
				Declaration.Leaf = Bridge.SourceBlock;
				Declaration.Anchor = Bridge.Source.Node;
				Declaration.Size = RerouteSize;
				Leaves[Declaration.Leaf].Endpoints.Add(Endpoints.Num() - 1);
			}
			for (int32 UseIndex = 0; UseIndex < Bridge.Uses.Num(); ++UseIndex)
			{
				FIRLayoutBridgeUse& Use = Bridge.Uses[UseIndex];
				Use.Size = Bridge.bClone ? Out.Sizes[Bridge.Source.Node] : RerouteSize;
				FEndpoint& Usage = Endpoints.AddDefaulted_GetRef();
				Usage.Kind = EEndpointKind::BridgeUse;
				Usage.Index = BridgeIndex;
				Usage.Use = UseIndex;
				Usage.Leaf = Use.Block;
				Usage.Anchor = Use.Consumers.Num() > 0 ? Use.Consumers[0] : INDEX_NONE;
				Usage.Size = Use.Size;
				Leaves[Usage.Leaf].Endpoints.Add(Endpoints.Num() - 1);
			}
		}
		for (int32 RouteIndex = 0; RouteIndex < Out.OutputRoutes.Num(); ++RouteIndex)
		{
			FIRLayoutOutputRoute& Route = Out.OutputRoutes[RouteIndex];
			const FIRNode& ConsumerNode = Graph.Nodes[Route.Consumer];
			// The host names the pair after the output; the name only decides how wide the two come out.
			FString RouteName = TEXT("DS_Output");
			if (Route.Consumer == Graph.Sink)
			{
				int32 NamedInput = Route.InputIndex - ConsumerNode.Operands.Num();
				if (ConsumerNode.Inputs.IsValidIndex(NamedInput))
				{
					RouteName = TEXT("DS_") + ConsumerNode.Inputs[NamedInput].Pin + TEXT("_00");
				}
			}
			else if (const FIRProperty* OutputName = ConsumerNode.FindProperty(TEXT("OutputName")))
			{
				RouteName = TEXT("DS_") + OutputName->Value.ToString() + TEXT("_0");
			}
			Route.DeclarationSize = EstimateDreamShaderIRRerouteSize(RouteName);
			Route.UsageSize = Route.DeclarationSize;

			FEndpoint& Declaration = Endpoints.AddDefaulted_GetRef();
			Declaration.Kind = EEndpointKind::RouteDeclaration;
			Declaration.Index = RouteIndex;
			Declaration.Leaf = Out.Blocks[Route.Source.Node];
			Declaration.Anchor = Route.Source.Node;
			Declaration.Size = Route.DeclarationSize;
			Leaves[Declaration.Leaf].Endpoints.Add(Endpoints.Num() - 1);

			FEndpoint& Usage = Endpoints.AddDefaulted_GetRef();
			Usage.Kind = EEndpointKind::RouteUsage;
			Usage.Index = RouteIndex;
			Usage.Leaf = Route.Consumer == Graph.Sink ? INDEX_NONE : Out.Blocks[Route.Consumer];
			Usage.Anchor = Route.Consumer;
			Usage.Size = Route.UsageSize;
			if (Leaves.IsValidIndex(Usage.Leaf))
			{
				Leaves[Usage.Leaf].Endpoints.Add(Endpoints.Num() - 1);
			}
		}

		// ----- one layered drawing per leaf
		TArray<int32> OrderPosition;
		OrderPosition.Init(INDEX_NONE, NodeCount);
		for (int32 Index = 0; Index < Order.Num(); ++Index)
		{
			OrderPosition[Order[Index]] = Index;
		}

		TArray<int32> LocalOfNode;
		LocalOfNode.Init(INDEX_NONE, NodeCount);
		TMap<uint64, int32> UseEndpointByValue;
		TMap<uint64, int32> RouteUsageByConsumerInput;
		int32 MaxColumns = 0;

		const int32 ColumnGap = FMath::Max(0, Options.ColumnGap);
		const int32 RowGap = FMath::Max(0, Options.RowGap);
		TArray<FIRValue> Values;
		for (int32 LeafIndex = 0; LeafIndex < Leaves.Num(); ++LeafIndex)
		{
			FLeaf& Leaf = Leaves[LeafIndex];
			const int32 LocalNodeCount = Leaf.Nodes.Num();
			const int32 LocalCount = LocalNodeCount + Leaf.Endpoints.Num();
			if (LocalCount == 0)
			{
				continue;
			}

			for (int32 Local = 0; Local < LocalNodeCount; ++Local)
			{
				LocalOfNode[Leaf.Nodes[Local]] = Local;
			}
			UseEndpointByValue.Reset();
			RouteUsageByConsumerInput.Reset();
			for (int32 Slot = 0; Slot < Leaf.Endpoints.Num(); ++Slot)
			{
				const FEndpoint& Endpoint = Endpoints[Leaf.Endpoints[Slot]];
				if (Endpoint.Kind == EEndpointKind::BridgeUse)
				{
					UseEndpointByValue.Add(ValueKey(Out.Bridges[Endpoint.Index].Source), LocalNodeCount + Slot);
				}
				else if (Endpoint.Kind == EEndpointKind::RouteUsage)
				{
					const FIRLayoutOutputRoute& Route = Out.OutputRoutes[Endpoint.Index];
					RouteUsageByConsumerInput.Add(ValueKey(FIRValue{ Route.Consumer, Route.InputIndex }), LocalNodeCount + Slot);
				}
			}

			FLayeredGraph Layered;
			Layered.SetNum(LocalCount);
			TArray<int32> LocalWidth;
			LocalWidth.Init(0, LocalCount);

			for (int32 Local = 0; Local < LocalNodeCount; ++Local)
			{
				const int32 Node = Leaf.Nodes[Local];
				Layered.Height[Local] = HeightOf(Node);
				LocalWidth[Local] = WidthOf(Node);

				Values.Reset();
				FIRGraph::CollectInputValues(Graph.Nodes[Node], Values);
				for (int32 InputIndex = 0; InputIndex < Values.Num(); ++InputIndex)
				{
					const FIRValue& Value = Values[InputIndex];
					if (!IsPlaced(Value.Node) || Value.Node == Node)
					{
						continue;
					}
					int32 ReadLocal = INDEX_NONE;
					if (const int32* RouteUsage = RouteUsageByConsumerInput.Find(ValueKey(FIRValue{ Node, InputIndex })))
					{
						ReadLocal = *RouteUsage;
					}
					else if (Out.Blocks[Value.Node] == LeafIndex)
					{
						ReadLocal = LocalOfNode[Value.Node];
					}
					else if (const int32* UseEndpoint = UseEndpointByValue.Find(ValueKey(Value)))
					{
						ReadLocal = *UseEndpoint;
					}
					if (ReadLocal != INDEX_NONE)
					{
						Layered.Reads[Local].AddUnique(ReadLocal);
					}
				}
			}
			for (int32 Slot = 0; Slot < Leaf.Endpoints.Num(); ++Slot)
			{
				const int32 Local = LocalNodeCount + Slot;
				const FEndpoint& Endpoint = Endpoints[Leaf.Endpoints[Slot]];
				Layered.Height[Local] = Endpoint.Size.Height;
				LocalWidth[Local] = Endpoint.Size.Width;
				if (Endpoint.Kind == EEndpointKind::BridgeDeclaration || Endpoint.Kind == EEndpointKind::RouteDeclaration)
				{
					const int32 SourceLocal = LocalOfNode[Endpoint.Anchor];
					if (SourceLocal != INDEX_NONE)
					{
						Layered.Reads[Local].Add(SourceLocal);
					}
				}
			}
			for (int32 Local = 0; Local < LocalCount; ++Local)
			{
				for (const int32 Read : Layered.Reads[Local])
				{
					Layered.Readers[Read].AddUnique(Local);
				}
			}

			// Columns: the longest way to something nothing in the leaf reads. Readers before what they read -- the
			// declarations read nodes, the nodes read each other in the graph's own order, and nothing reads... a usage
			// reads nothing, so it comes last.
			TArray<int32> NodesLatestFirst = Leaf.Nodes;
			NodesLatestFirst.Sort([&OrderPosition](const int32 A, const int32 B) { return OrderPosition[A] > OrderPosition[B]; });
			const auto ColumnFromReaders = [&Layered](const int32 Local)
			{
				int32 Column = 0;
				for (const int32 Reader : Layered.Readers[Local])
				{
					Column = FMath::Max(Column, Layered.Column[Reader] + 1);
				}
				Layered.Column[Local] = Column;
			};
			for (int32 Slot = 0; Slot < Leaf.Endpoints.Num(); ++Slot)
			{
				if (Layered.Reads[LocalNodeCount + Slot].Num() > 0)
				{
					Layered.Column[LocalNodeCount + Slot] = 0;
				}
			}
			for (const int32 Node : NodesLatestFirst)
			{
				ColumnFromReaders(LocalOfNode[Node]);
			}
			for (int32 Slot = 0; Slot < Leaf.Endpoints.Num(); ++Slot)
			{
				const int32 Local = LocalNodeCount + Slot;
				if (Layered.Reads[Local].Num() == 0)
				{
					ColumnFromReaders(Local);
				}
				else
				{
					// A declaration sits right beside the value it takes out, not at the far end of the box.
					Layered.Column[Local] = FMath::Max(0, Layered.Column[Layered.Reads[Local][0]] - 1);
				}
			}

			Layered.Solve(RowGap);

			int32 LocalColumns = 0;
			for (int32 Local = 0; Local < LocalCount; ++Local)
			{
				LocalColumns = FMath::Max(LocalColumns, Layered.Column[Local] + 1);
			}
			MaxColumns = FMath::Max(MaxColumns, LocalColumns);
			TArray<int32> LocalColumnWidth;
			LocalColumnWidth.Init(0, LocalColumns);
			for (int32 Local = 0; Local < LocalCount; ++Local)
			{
				LocalColumnWidth[Layered.Column[Local]] = FMath::Max(LocalColumnWidth[Layered.Column[Local]], LocalWidth[Local]);
			}
			// Column 0 is the rightmost; X grows to the right from the leftmost column.
			TArray<int32> LocalColumnLeft;
			LocalColumnLeft.Init(0, LocalColumns);
			int32 Cursor = 0;
			for (int32 Column = LocalColumns - 1; Column >= 0; --Column)
			{
				LocalColumnLeft[Column] = Cursor;
				Cursor += LocalColumnWidth[Column] + (Column > 0 ? ColumnGap : 0);
			}
			const int32 InnerWidth = Cursor;

			int32 MinTop = MAX_int32;
			int32 MaxBottom = MIN_int32;
			for (int32 Local = 0; Local < LocalCount; ++Local)
			{
				MinTop = FMath::Min(MinTop, Layered.Top[Local]);
				MaxBottom = FMath::Max(MaxBottom, Layered.Top[Local] + Layered.Height[Local]);
			}
			Leaf.InnerSize = FIntPoint(InnerWidth, MaxBottom - MinTop);

			for (int32 Local = 0; Local < LocalCount; ++Local)
			{
				// Right-aligned in its column, as the other styles do: the output pins of a column line up.
				const int32 Column = Layered.Column[Local];
				const FIntPoint LocalPosition(
					LocalColumnLeft[Column] + (LocalColumnWidth[Column] - LocalWidth[Local]),
					Layered.Top[Local] - MinTop);
				if (Local < LocalNodeCount)
				{
					const int32 Node = Leaf.Nodes[Local];
					Out.Positions[Node] = LocalPosition;
					Out.Columns[Node] = Column;
				}
				else
				{
					Endpoints[Leaf.Endpoints[Local - LocalNodeCount]].Position = LocalPosition;
				}
			}

			// What a hint's offset travels along: the nearest reader inside the leaf.
			for (int32 Local = 0; Local < LocalNodeCount; ++Local)
			{
				int32 Best = INDEX_NONE;
				for (const int32 Reader : Layered.Readers[Local])
				{
					if (Reader < LocalNodeCount && (Best == INDEX_NONE || Layered.Column[Reader] > Layered.Column[Best]))
					{
						Best = Reader;
					}
				}
				TreeParent[Leaf.Nodes[Local]] = Best != INDEX_NONE ? Leaf.Nodes[Best] : INDEX_NONE;
			}

			for (int32 Local = 0; Local < LocalNodeCount; ++Local)
			{
				LocalOfNode[Leaf.Nodes[Local]] = INDEX_NONE;
			}
		}
		Out.ColumnCount = MaxColumns;

		// ----- the page
		const int32 PadX = FMath::Max(0, Options.RegionPaddingX) / 2;
		const int32 PadY = FMath::Max(0, Options.RegionPaddingY) / 2;
		const int32 TitleHeight = FMath::Max(0, Options.RegionTitleHeight);
		const int32 BlockGap = FMath::Max(0, Options.BlockGap);

		TArray<FBox>& Boxes = Scratch.Boxes;
		TArray<int32> BoxOfRegion;
		BoxOfRegion.Init(INDEX_NONE, RegionCount);
		TArray<int32> TopLevel;
		int32 OutputsBox = INDEX_NONE;

		// A region gets a box of its own as soon as something is in it; one that turns out to hold a single leaf and
		// nothing else is folded into that leaf below, so the leaf carries the region's name.
		const auto BoxForRegion = [this, &Boxes, &BoxOfRegion, &TopLevel, RegionCount](const int32 Region, const int32 FirstBand) -> int32
		{
			int32 Child = INDEX_NONE;
			int32 Walk = Region;
			for (int32 Guard = 0; Graph.Regions.IsValidIndex(Walk) && Guard <= RegionCount; ++Guard)
			{
				const bool bNew = BoxOfRegion[Walk] == INDEX_NONE;
				if (bNew)
				{
					BoxOfRegion[Walk] = Boxes.AddDefaulted();
					Boxes[BoxOfRegion[Walk]].Region = Walk;
				}
				FBox& Box = Boxes[BoxOfRegion[Walk]];
				Box.FirstBand = FMath::Min(Box.FirstBand, FirstBand);
				if (Child != INDEX_NONE)
				{
					Box.Children.AddUnique(Child);
				}
				Child = BoxOfRegion[Walk];
				if (!bNew)
				{
					// Already hung under its own parents.
					Child = INDEX_NONE;
					// Still walk up: FirstBand may have moved.
				}
				const int32 Parent = Graph.Regions[Walk].Parent;
				if (!Graph.Regions.IsValidIndex(Parent))
				{
					if (bNew)
					{
						TopLevel.Add(BoxOfRegion[Walk]);
					}
					break;
				}
				Walk = Parent;
			}
			return BoxOfRegion[Region];
		};

		for (int32 LeafIndex = 0; LeafIndex < Leaves.Num(); ++LeafIndex)
		{
			FLeaf& Leaf = Leaves[LeafIndex];
			if (Leaf.Nodes.Num() == 0 && Leaf.Endpoints.Num() == 0)
			{
				continue;
			}
			const int32 LeafBox = Boxes.AddDefaulted();
			Boxes[LeafBox].Leaf = LeafIndex;
			Boxes[LeafBox].FirstBand = Leaf.FirstBand;
			Boxes[LeafBox].Size = FIntPoint(Leaf.InnerSize.X + 2 * PadX, Leaf.InnerSize.Y + 2 * PadY + TitleHeight);
			if (Leaf.bOutputs)
			{
				OutputsBox = LeafBox;
			}
			else if (Graph.Regions.IsValidIndex(Leaf.Region))
			{
				const int32 RegionBox = BoxForRegion(Leaf.Region, Leaf.FirstBand);
				Boxes[RegionBox].Children.Add(LeafBox);
			}
			else
			{
				TopLevel.Add(LeafBox);
			}
		}

		// Children in source order; a region box with one leaf and nothing else IS that leaf.
		const auto SortBySource = [&Boxes](TArray<int32>& Children)
		{
			Children.StableSort([&Boxes](const int32 A, const int32 B)
			{
				if (Boxes[A].FirstBand != Boxes[B].FirstBand)
				{
					return Boxes[A].FirstBand < Boxes[B].FirstBand;
				}
				return A < B;
			});
		};

		// Sizes from the inside out: a region nests at most a handful deep, and its depth orders the work.
		TArray<int32> RegionBoxes;
		for (int32 Region = 0; Region < RegionCount; ++Region)
		{
			if (BoxOfRegion[Region] != INDEX_NONE)
			{
				RegionBoxes.Add(BoxOfRegion[Region]);
			}
		}
		RegionBoxes.StableSort([&Boxes, &Scratch](const int32 A, const int32 B)
		{
			return Scratch.RegionDepth[Boxes[A].Region] > Scratch.RegionDepth[Boxes[B].Region];
		});
		for (const int32 RegionBox : RegionBoxes)
		{
			FBox& Box = Boxes[RegionBox];
			SortBySource(Box.Children);
			if (Box.Children.Num() == 1 && Boxes[Box.Children[0]].Leaf != INDEX_NONE)
			{
				// The leaf's own box is the region's: same size, the child at its corner.
				Boxes[Box.Children[0]].Position = FIntPoint::ZeroValue;
				Box.Size = Boxes[Box.Children[0]].Size;
				continue;
			}
			const FIntPoint Extent = PackShelves(Boxes, Box.Children, BlockGap / 2, 1.6f);
			for (const int32 Child : Box.Children)
			{
				Boxes[Child].Position += FIntPoint(PadX, PadY + TitleHeight);
			}
			Box.Size = FIntPoint(Extent.X + 2 * PadX, Extent.Y + 2 * PadY + TitleHeight);
		}

		SortBySource(TopLevel);
		const FIntPoint PageExtent = PackShelves(Boxes, TopLevel, BlockGap, Options.BlockPageAspect);
		if (OutputsBox != INDEX_NONE)
		{
			Boxes[OutputsBox].Position = FIntPoint(PageExtent.X + (TopLevel.Num() > 0 ? BlockGap : 0), 0);
			TopLevel.Add(OutputsBox);
		}

		// Positions from the outside in, without recursion: a stack of (box, where its corner is).
		int32 PageRight = 0;
		{
			struct FFrame
			{
				int32 Box = INDEX_NONE;
				FIntPoint Corner = FIntPoint::ZeroValue;
			};
			TArray<FFrame> Stack;
			for (const int32 Box : TopLevel)
			{
				Stack.Add({ Box, Boxes[Box].Position });
			}
			while (Stack.Num() > 0)
			{
				const FFrame Frame = Stack.Pop();
				FBox& Box = Boxes[Frame.Box];
				Box.Position = Frame.Corner;
				PageRight = FMath::Max(PageRight, Frame.Corner.X + Box.Size.X);
				if (Box.Leaf != INDEX_NONE)
				{
					Leaves[Box.Leaf].Origin = Frame.Corner + FIntPoint(PadX, PadY + TitleHeight);
					continue;
				}
				for (const int32 Child : Box.Children)
				{
					Stack.Add({ Child, Frame.Corner + Boxes[Child].Position });
				}
			}
		}

		// The page ends at x = 0, as the other styles do: the material's own node goes to the right of that.
		for (int32 LeafIndex = 0; LeafIndex < Leaves.Num(); ++LeafIndex)
		{
			FLeaf& Leaf = Leaves[LeafIndex];
			Leaf.Origin.X -= PageRight;
			for (const int32 Node : Leaf.Nodes)
			{
				Out.Positions[Node] += Leaf.Origin;
			}
			for (const int32 EndpointIndex : Leaf.Endpoints)
			{
				Endpoints[EndpointIndex].Position += Leaf.Origin;
			}
		}

		// ----- what the host is told about the boxes
		Out.BlockList.Reset();
		Out.BlockList.SetNum(Leaves.Num());
		for (int32 LeafIndex = 0; LeafIndex < Leaves.Num(); ++LeafIndex)
		{
			const FLeaf& Leaf = Leaves[LeafIndex];
			FIRLayoutBlock& Block = Out.BlockList[LeafIndex];
			Block.Region = Leaf.Region;
			Block.Nodes = Leaf.Nodes;
			Block.FirstLine = Leaf.FirstLine;
			Block.LastLine = Leaf.LastLine;
			Block.bOutputs = Leaf.bOutputs;
			if (Leaf.bOutputs)
			{
				Block.Title = TEXT("Outputs");
			}
			else if (Leaf.FirstName.IsEmpty())
			{
				Block.Title = Leaf.FirstLine > 0
					? (Leaf.LastLine > Leaf.FirstLine
						? FString::Printf(TEXT("Lines %d-%d"), Leaf.FirstLine, Leaf.LastLine)
						: FString::Printf(TEXT("Line %d"), Leaf.FirstLine))
					: FString::Printf(TEXT("Block %d"), LeafIndex + 1);
			}
			else if (Leaf.FirstName.Equals(Leaf.LastName, ESearchCase::CaseSensitive))
			{
				Block.Title = Leaf.FirstName;
			}
			else
			{
				Block.Title = Leaf.FirstName + TEXT(" ... ") + Leaf.LastName;
			}
		}
	}

	void FState::FinishBlocks()
	{
		using namespace BlockDetail;

		FBlockScratch& Scratch = BlockScratch;
		TArray<FLeaf>& Leaves = Scratch.Leaves;
		TArray<FEndpoint>& Endpoints = Scratch.Endpoints;
		const int32 PadX = FMath::Max(0, Options.RegionPaddingX) / 2;
		const int32 PadY = FMath::Max(0, Options.RegionPaddingY) / 2;
		const int32 TitleHeight = FMath::Max(0, Options.RegionTitleHeight);

		// A reroute goes where its node went.
		for (FEndpoint& Endpoint : Endpoints)
		{
			if (HintOffset.IsValidIndex(Endpoint.Anchor) && Endpoint.Leaf != INDEX_NONE)
			{
				Endpoint.Position += HintOffset[Endpoint.Anchor];
			}
			Endpoint.Position.X = SnapDown(Endpoint.Position.X, Options.GridSize);
			Endpoint.Position.Y = SnapDown(Endpoint.Position.Y, Options.GridSize);
			if (Endpoint.Leaf != INDEX_NONE)
			{
				Out.BoundsMin.X = FMath::Min(Out.BoundsMin.X, Endpoint.Position.X);
				Out.BoundsMin.Y = FMath::Min(Out.BoundsMin.Y, Endpoint.Position.Y);
				Out.BoundsMax.X = FMath::Max(Out.BoundsMax.X, Endpoint.Position.X + Endpoint.Size.Width);
				Out.BoundsMax.Y = FMath::Max(Out.BoundsMax.Y, Endpoint.Position.Y + Endpoint.Size.Height);
			}
		}

		// Every box from what it holds, leaves first, then the regions from the inside out.
		struct FRect
		{
			bool bValid = false;
			int32 MinX = 0;
			int32 MinY = 0;
			int32 MaxX = 0;
			int32 MaxY = 0;

			void Include(const int32 X0, const int32 Y0, const int32 X1, const int32 Y1)
			{
				if (!bValid)
				{
					bValid = true;
					MinX = X0;
					MinY = Y0;
					MaxX = X1;
					MaxY = Y1;
					return;
				}
				MinX = FMath::Min(MinX, X0);
				MinY = FMath::Min(MinY, Y0);
				MaxX = FMath::Max(MaxX, X1);
				MaxY = FMath::Max(MaxY, Y1);
			}
		};

		TArray<FBox>& Boxes = Scratch.Boxes;
		TArray<FRect> Rects;
		Rects.SetNum(Boxes.Num());
		for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
		{
			const FBox& Box = Boxes[BoxIndex];
			if (Box.Leaf == INDEX_NONE)
			{
				continue;
			}
			const FLeaf& Leaf = Leaves[Box.Leaf];
			FRect Inner;
			for (const int32 Node : Leaf.Nodes)
			{
				Inner.Include(Out.Positions[Node].X, Out.Positions[Node].Y, Out.Positions[Node].X + WidthOf(Node), Out.Positions[Node].Y + HeightOf(Node));
			}
			for (const int32 EndpointIndex : Leaf.Endpoints)
			{
				const FEndpoint& Endpoint = Endpoints[EndpointIndex];
				Inner.Include(Endpoint.Position.X, Endpoint.Position.Y, Endpoint.Position.X + Endpoint.Size.Width, Endpoint.Position.Y + Endpoint.Size.Height);
			}
			if (Inner.bValid)
			{
				Rects[BoxIndex].Include(Inner.MinX - PadX, Inner.MinY - PadY - TitleHeight, Inner.MaxX + PadX, Inner.MaxY + PadY);
			}
		}

		TArray<int32> RegionBoxes;
		for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
		{
			if (Boxes[BoxIndex].Leaf == INDEX_NONE)
			{
				RegionBoxes.Add(BoxIndex);
			}
		}
		RegionBoxes.StableSort([&Boxes, &Scratch](const int32 A, const int32 B)
		{
			return Scratch.RegionDepth[Boxes[A].Region] > Scratch.RegionDepth[Boxes[B].Region];
		});
		TArray<bool> FoldedIntoLeaf;
		FoldedIntoLeaf.Init(false, Boxes.Num());
		for (const int32 RegionBox : RegionBoxes)
		{
			const FBox& Box = Boxes[RegionBox];
			const bool bIsItsLeaf = Box.Children.Num() == 1 && Boxes[Box.Children[0]].Leaf != INDEX_NONE;
			for (const int32 Child : Box.Children)
			{
				if (!Rects[Child].bValid)
				{
					continue;
				}
				if (bIsItsLeaf)
				{
					Rects[RegionBox] = Rects[Child];
					FoldedIntoLeaf[Child] = true;
				}
				else
				{
					Rects[RegionBox].Include(
						Rects[Child].MinX - PadX, Rects[Child].MinY - PadY - TitleHeight,
						Rects[Child].MaxX + PadX, Rects[Child].MaxY + PadY);
				}
			}
		}

		// Outermost first, so that a host drawing them in order leaves the inner boxes on top.
		struct FDrawn
		{
			int32 Box = INDEX_NONE;
			int32 Depth = 0;
		};
		TArray<FDrawn> Drawn;
		for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
		{
			if (!Rects[BoxIndex].bValid || FoldedIntoLeaf[BoxIndex])
			{
				continue;
			}
			const FBox& Box = Boxes[BoxIndex];
			int32 Depth = 0;
			if (Box.Leaf == INDEX_NONE)
			{
				Depth = Scratch.RegionDepth[Box.Region];
			}
			else if (Graph.Regions.IsValidIndex(Leaves[Box.Leaf].Region))
			{
				Depth = Scratch.RegionDepth[Leaves[Box.Leaf].Region] + 1;
			}
			Drawn.Add({ BoxIndex, Depth });
		}
		Drawn.StableSort([](const FDrawn& A, const FDrawn& B)
		{
			if (A.Depth != B.Depth)
			{
				return A.Depth < B.Depth;
			}
			return A.Box < B.Box;
		});

		for (const FDrawn& Entry : Drawn)
		{
			const FBox& Box = Boxes[Entry.Box];
			const FRect& Rect = Rects[Entry.Box];
			FIRLayoutComment Comment;
			Comment.bGenerated = true;
			Comment.Depth = Entry.Depth;
			Comment.X = Rect.MinX;
			Comment.Y = Rect.MinY;
			Comment.Width = Rect.MaxX - Rect.MinX;
			Comment.Height = Rect.MaxY - Rect.MinY;
			if (Box.Leaf == INDEX_NONE)
			{
				Comment.Title = Graph.Regions[Box.Region].Name;
				Comment.Region = Box.Region;
				// The region's one leaf stands for it.
				if (Box.Children.Num() == 1 && Boxes[Box.Children[0]].Leaf != INDEX_NONE)
				{
					FIRLayoutBlock& Block = Out.BlockList[Boxes[Box.Children[0]].Leaf];
					Block.Title = Comment.Title;
					Block.X = Comment.X;
					Block.Y = Comment.Y;
					Block.Width = Comment.Width;
					Block.Height = Comment.Height;
				}
			}
			else
			{
				FIRLayoutBlock& Block = Out.BlockList[Box.Leaf];
				Comment.Title = Block.Title;
				Block.X = Comment.X;
				Block.Y = Comment.Y;
				Block.Width = Comment.Width;
				Block.Height = Comment.Height;
			}
			Out.Comments.Add(MoveTemp(Comment));
		}

		// The reroutes between the leaves have their place now; the ones beside the material's node wait for it.
		for (const FEndpoint& Endpoint : Endpoints)
		{
			switch (Endpoint.Kind)
			{
			case EEndpointKind::BridgeDeclaration:
				Out.Bridges[Endpoint.Index].DeclarationPosition = Endpoint.Position;
				break;
			case EEndpointKind::BridgeUse:
				Out.Bridges[Endpoint.Index].Uses[Endpoint.Use].Position = Endpoint.Position;
				break;
			case EEndpointKind::RouteDeclaration:
				Out.OutputRoutes[Endpoint.Index].DeclarationPosition = Endpoint.Position;
				break;
			case EEndpointKind::RouteUsage:
				if (Endpoint.Leaf != INDEX_NONE)
				{
					Out.OutputRoutes[Endpoint.Index].UsagePosition = Endpoint.Position;
				}
				break;
			}
		}
	}

	void FState::PlaceBlockOutputs()
	{
		// The usages that drive the material stand in a column between the page and the material's own node, in the
		// order of its inputs, centred on it.
		TArray<int32> SinkRoutes;
		int32 Widest = 0;
		int32 Total = 0;
		constexpr int32 UsageGap = 24;
		for (int32 RouteIndex = 0; RouteIndex < Out.OutputRoutes.Num(); ++RouteIndex)
		{
			const FIRLayoutOutputRoute& Route = Out.OutputRoutes[RouteIndex];
			if (Route.Consumer == Graph.Sink)
			{
				SinkRoutes.Add(RouteIndex);
				Widest = FMath::Max(Widest, Route.UsageSize.Width);
				Total += Route.UsageSize.Height + (SinkRoutes.Num() > 1 ? UsageGap : 0);
			}
		}
		if (SinkRoutes.Num() > 0)
		{
			constexpr int32 RootNodeHalfHeight = 240;
			const int32 UsageColumnLeft = SnapDown(Out.BoundsMax.X + FMath::Max(0, Options.BlockGap), Options.GridSize);
			Out.RootPosition.X = SnapDown(UsageColumnLeft + Widest + FMath::Max(0, Options.ColumnGap), Options.GridSize);
			int32 Cursor = Out.RootPosition.Y + RootNodeHalfHeight - Total / 2;
			for (const int32 RouteIndex : SinkRoutes)
			{
				FIRLayoutOutputRoute& Route = Out.OutputRoutes[RouteIndex];
				Route.UsagePosition = FIntPoint(
					SnapDown(UsageColumnLeft + (Widest - Route.UsageSize.Width), Options.GridSize),
					SnapDown(Cursor, Options.GridSize));
				Cursor += Route.UsageSize.Height + UsageGap;
			}
			if (IsPlaced(Graph.Sink))
			{
				Out.Positions[Graph.Sink] = Out.RootPosition;
			}
		}

		BlockScratch = FBlockScratch();
	}
}
