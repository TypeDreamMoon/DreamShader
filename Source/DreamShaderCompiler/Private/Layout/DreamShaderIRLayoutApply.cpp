// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The layout styles computed on the IR, on a live graph: measure the emitted nodes, let IR/IRLayout place them, write
// the coordinates -- and, for the block style, make the reroutes it counted on.
//
// The placement itself is engine-free (DreamShaderLang, IR/IRLayout.h) and knows IR nodes. What is left to do here is
// the part that needs the engine:
//
//   * sizes -- an emitted node is measured with the estimator the 1.x layout uses, so both agree on what a node takes;
//   * the expressions an IR node became besides its own -- a hand-lowered op makes a constant or two, a texture
//     parameter a sample -- which are stacked to the left of it, inside the room the node was given;
//   * region boxes and `#pragma layout(Comment, ...)` boxes, as the comment expressions the 1.x layout makes, so a
//     decompile and a rebuild see the same thing whichever style placed the graph;
//   * the material's own node;
//   * for Blocks, the reroutes between the boxes: a named reroute declaration beside every value another box reads and
//     a usage in each box that reads it, a constant repeated where it is read, and the reroute pair the emitter already
//     put in front of every output moved to where the layout wants its two halves.
//
// SourceBands and Layered insert nothing and change no wire, so a graph placed by them dumps the same as one that was
// never placed. Blocks adds reroutes and repeated constants, as the 1.x layout does; the graph parity comparator looks
// through both.

#include "Emitter/DreamShaderIREmitterInternal.h"

#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderSettings.h"
#include "IR/IRLayout.h"

#include "EdGraph/EdGraphNode.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialFunction.h"

namespace UE::DreamShader::Editor::Compiler
{
	namespace IRLayoutApply
	{
		/** Room between a node and the helpers stacked to its left, and between two of those. */
		constexpr int32 HelperGapX = 48;
		constexpr int32 HelperGapY = 24;

		static void SetExpressionPosition(UMaterialExpression* Expression, const int32 X, const int32 Y)
		{
			if (!Expression)
			{
				return;
			}
			Expression->MaterialExpressionEditorX = X;
			Expression->MaterialExpressionEditorY = Y;
			if (Expression->GraphNode)
			{
				Expression->GraphNode->NodePosX = X;
				Expression->GraphNode->NodePosY = Y;
			}
		}

		static void AddCommentBox(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const IR::FIRLayoutComment& Box,
			const bool bGenerated)
		{
			UObject* Outer = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(MaterialFunction);
			if (!Outer)
			{
				return;
			}
			UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Outer, NAME_None, RF_Transactional);
			if (!Comment)
			{
				return;
			}

			// A region box carries the marker the 1.x layout gives its own boxes: the decompiler leaves such a box out,
			// because the region it stands for comes back from the decompile hints. A `#pragma layout(Comment, ...)` box is
			// the author's and keeps its title as written.
			// A box of the layout's own -- a region, a block -- carries the same marker for the same reason: it is made again
			// by the next build, and what it stands for is in the source.
			Comment->Text = bGenerated
				? FString::Printf(TEXT("DreamShader: %s"), *Box.Title) /* I18N-EXEMPT: a marker the decompiler matches, not display text */
				: Box.Title;
			Comment->MaterialExpressionEditorX = Box.X;
			Comment->MaterialExpressionEditorY = Box.Y;
			Comment->SizeX = FMath::Max(bGenerated ? 420 : 64, Box.Width);
			Comment->SizeY = FMath::Max(bGenerated ? 240 : 64, Box.Height);
			Comment->FontSize = 24;
			Comment->CommentColor = FLinearColor(Box.Color[0], Box.Color[1], Box.Color[2], Box.Color[3]);
			Comment->bCommentBubbleVisible_InDetailsPanel = true;
			Comment->bColorCommentBubble = true;
			Comment->bGroupMode = true;

			if (Material)
			{
				Material->GetExpressionCollection().AddComment(Comment);
			}
			else
			{
				MaterialFunction->GetExpressionCollection().AddComment(Comment);
			}
		}

		/** As FIREmitter::ConnectValueToInput: the value arriving is already the output it names, so no inline mask. */
		static void ConnectWithoutMask(FExpressionInput& Input, UMaterialExpression* Expression, const int32 OutputIndex)
		{
			Input.Connect(OutputIndex, Expression);
			Input.Mask = 0;
			Input.MaskR = 0;
			Input.MaskG = 0;
			Input.MaskB = 0;
			Input.MaskA = 0;
		}

		/** The reroute pair CreateOutputRerouteValue put in front of an input, or null where the input is wired directly. */
		static UMaterialExpressionNamedRerouteUsage* FindOutputUsage(const FExpressionInput* Input)
		{
			UMaterialExpressionNamedRerouteUsage* Usage = Input ? Cast<UMaterialExpressionNamedRerouteUsage>(Input->Expression) : nullptr;
			return Usage && Usage->Declaration ? Usage : nullptr;
		}

		/**
		 * The block style's reroutes, made and wired.
		 *
		 * The layout knows IR nodes; the wires are between expressions, and the two differ where an IR node is no node of
		 * its own -- a Convert or a Broadcast answers with the expression of what it reads. So the rewiring is done on the
		 * expressions: every input in a reading box that is fed by the bridged value's expression is moved onto the usage
		 * (or the repeated constant) the layout placed in that box. A declaration made for such a pass-through in a box
		 * other than the value's own is fed by that box's usage of the value, not by a wire across the page.
		 */
		static void MakeBlockBridges(
			UMaterial* Material,
			UMaterialFunction* MaterialFunction,
			const IR::FIRGraph& Graph,
			const IR::FIRLayoutResult& Layout,
			const FIREmitter& Emitter)
		{
			const TArray<TArray<UMaterialExpression*>>& CreatedByNode = Emitter.GetCreatedExpressionsByNode();
			const int32 BlockCount = Layout.BlockList.Num();

			TArray<TArray<UMaterialExpression*>> ExpressionsOfBlock;
			ExpressionsOfBlock.SetNum(BlockCount);
			TMap<UMaterialExpression*, int32> BlockOfExpression;
			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				const int32 Block = Layout.Blocks.IsValidIndex(NodeIndex) ? Layout.Blocks[NodeIndex] : INDEX_NONE;
				if (!ExpressionsOfBlock.IsValidIndex(Block) || !CreatedByNode.IsValidIndex(NodeIndex))
				{
					continue;
				}
				for (UMaterialExpression* Expression : CreatedByNode[NodeIndex])
				{
					if (Expression && !BlockOfExpression.Contains(Expression))
					{
						BlockOfExpression.Add(Expression, Block);
						ExpressionsOfBlock[Block].Add(Expression);
					}
				}
			}

			// The reroute pair in front of every output: the declaration belongs to the box of the value, and is one more
			// reader of it there.
			const auto FindRoutePair = [Material, &Graph, &Emitter](const IR::FIRLayoutOutputRoute& Route) -> UMaterialExpressionNamedRerouteUsage*
			{
				if (Route.Consumer != Graph.Sink)
				{
					const TArray<FEmittedNode>& EmittedNodes = Emitter.GetEmittedNodes();
					const UMaterialExpressionFunctionOutput* Output = EmittedNodes.IsValidIndex(Route.Consumer)
						? Cast<UMaterialExpressionFunctionOutput>(EmittedNodes[Route.Consumer].Expression)
						: nullptr;
					return Output ? FindOutputUsage(&Output->A) : nullptr;
				}
				if (!Material || !Graph.Nodes.IsValidIndex(Route.Consumer))
				{
					return nullptr;
				}

				// By what feeds it and by the pin's name, which the pair was named after (DS_<Pin>_<property index>).
				FEmittedValue Source;
				if (!Emitter.TryGetEmittedValue(Route.Source, Source))
				{
					return nullptr;
				}
				const IR::FIRNode& SinkNode = Graph.Nodes[Route.Consumer];
				const int32 NamedInput = Route.InputIndex - SinkNode.Operands.Num();
				const FString Prefix = SinkNode.Inputs.IsValidIndex(NamedInput)
					? Private::MakeDreamShaderOutputRerouteName(SinkNode.Inputs[NamedInput].Pin, INDEX_NONE) + TEXT("_")
					: FString();
				UMaterialExpressionNamedRerouteUsage* Fallback = nullptr;
				for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
				{
					UMaterialExpressionNamedRerouteUsage* Usage = FindOutputUsage(Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(PropertyIndex)));
					if (!Usage || Usage->Declaration->Input.Expression != Source.Expression || Usage->Declaration->Input.OutputIndex != Source.OutputIndex)
					{
						continue;
					}
					if (!Prefix.IsEmpty() && Usage->Declaration->Name.ToString().StartsWith(Prefix, ESearchCase::CaseSensitive))
					{
						return Usage;
					}
					Fallback = Fallback ? Fallback : Usage;
				}
				return Fallback;
			};
			for (const IR::FIRLayoutOutputRoute& Route : Layout.OutputRoutes)
			{
				UMaterialExpressionNamedRerouteUsage* Usage = FindRoutePair(Route);
				if (!Usage)
				{
					continue;
				}
				SetExpressionPosition(Usage->Declaration, Route.DeclarationPosition.X, Route.DeclarationPosition.Y);
				SetExpressionPosition(Usage, Route.UsagePosition.X, Route.UsagePosition.Y);
				const int32 Block = Layout.Blocks.IsValidIndex(Route.Source.Node) ? Layout.Blocks[Route.Source.Node] : INDEX_NONE;
				if (ExpressionsOfBlock.IsValidIndex(Block) && !BlockOfExpression.Contains(Usage->Declaration))
				{
					BlockOfExpression.Add(Usage->Declaration, Block);
					ExpressionsOfBlock[Block].Add(Usage->Declaration);
				}
			}

			// What stands for a value inside a box: the value itself where it lives there, else the usage made for it.
			using FStandKey = TTuple<UMaterialExpression*, int32, int32>;
			TMap<FStandKey, FEmittedValue> StandIns;
			const auto ValueInBlock = [&BlockOfExpression, &StandIns](const FEmittedValue& Value, const int32 Block) -> FEmittedValue
			{
				const int32* Home = BlockOfExpression.Find(Value.Expression);
				if (Home && *Home == Block)
				{
					return Value;
				}
				if (const FEmittedValue* StandIn = StandIns.Find(FStandKey(Value.Expression, Value.OutputIndex, Block)))
				{
					return *StandIn;
				}
				return Value;
			};

			// Ascending by the value's node (the layout's order): what a declaration reads has its usage by then.
			for (const IR::FIRLayoutBridge& Bridge : Layout.Bridges)
			{
				FEmittedValue Source;
				if (!Emitter.TryGetEmittedValue(Bridge.Source, Source) || !Source.Expression)
				{
					continue;
				}

				UMaterialExpressionNamedRerouteDeclaration* Declaration = nullptr;
				if (!Bridge.bClone)
				{
					Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Private::CreateOwnedMaterialExpression(
						Material, MaterialFunction, UMaterialExpressionNamedRerouteDeclaration::StaticClass(),
						Bridge.DeclarationPosition.X, Bridge.DeclarationPosition.Y));
					if (!Declaration)
					{
						continue;
					}
					Declaration->Name = FName(*Bridge.Name);
					if (!Declaration->VariableGuid.IsValid())
					{
						Declaration->VariableGuid = FGuid::NewGuid();
					}
					const FEmittedValue Feed = ValueInBlock(Source, Bridge.SourceBlock);
					ConnectWithoutMask(Declaration->Input, Feed.Expression, Feed.OutputIndex);
					if (ExpressionsOfBlock.IsValidIndex(Bridge.SourceBlock))
					{
						BlockOfExpression.Add(Declaration, Bridge.SourceBlock);
						ExpressionsOfBlock[Bridge.SourceBlock].Add(Declaration);
					}
				}

				for (const IR::FIRLayoutBridgeUse& Use : Bridge.Uses)
				{
					if (!ExpressionsOfBlock.IsValidIndex(Use.Block))
					{
						continue;
					}

					FEmittedValue StandIn;
					if (Bridge.bClone)
					{
						// A constant is written where it is read: the node again, not a reroute to it.
						StandIn.Expression = UMaterialEditingLibrary::DuplicateMaterialExpression(Material, MaterialFunction, Source.Expression);
						StandIn.OutputIndex = Source.OutputIndex;
					}
					else
					{
						auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Private::CreateOwnedMaterialExpression(
							Material, MaterialFunction, UMaterialExpressionNamedRerouteUsage::StaticClass(),
							Use.Position.X, Use.Position.Y));
						if (Usage)
						{
							Usage->Declaration = Declaration;
							Usage->DeclarationGuid = Declaration->VariableGuid;
						}
						StandIn.Expression = Usage;
						StandIn.OutputIndex = 0;
					}
					if (!StandIn.Expression)
					{
						continue;
					}
					SetExpressionPosition(StandIn.Expression, Use.Position.X, Use.Position.Y);

					for (UMaterialExpression* Reader : ExpressionsOfBlock[Use.Block])
					{
						const int32 InputCount = Private::GetDreamShaderExpressionInputCount(Reader);
						for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
						{
							FExpressionInput* Input = Reader->GetInput(InputIndex);
							if (Input && Input->Expression == Source.Expression && Input->OutputIndex == Source.OutputIndex)
							{
								ConnectWithoutMask(*Input, StandIn.Expression, StandIn.OutputIndex);
							}
						}
					}

					StandIns.Add(FStandKey(Source.Expression, Source.OutputIndex, Use.Block), StandIn);
					BlockOfExpression.Add(StandIn.Expression, Use.Block);
					ExpressionsOfBlock[Use.Block].Add(StandIn.Expression);
				}
			}
		}
	}

	bool ApplyDreamShaderIRLayout(
		UMaterial* Material,
		UMaterialFunction* MaterialFunction,
		const IR::FIRProduct& Product,
		const FIREmitter& Emitter)
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		IR::EIRLayoutStyle Style = IR::EIRLayoutStyle::Blocks;
		switch (Settings ? Settings->GraphLayoutStyle : EDreamShaderGraphLayoutStyle::Blocks)
		{
		case EDreamShaderGraphLayoutStyle::Blocks:
			Style = IR::EIRLayoutStyle::Blocks;
			break;
		case EDreamShaderGraphLayoutStyle::SourceBands:
			Style = IR::EIRLayoutStyle::SourceBands;
			break;
		case EDreamShaderGraphLayoutStyle::Layered:
			Style = IR::EIRLayoutStyle::Layered;
			break;
		case EDreamShaderGraphLayoutStyle::Classic:
		default:
			// The 1.x layout, which works on the live graph and is the caller's to run.
			return false;
		}

		if (!Material && !MaterialFunction)
		{
			return false;
		}

		const IR::FIRGraph& Graph = Product.Graph;
		const TArray<FEmittedNode>& EmittedNodes = Emitter.GetEmittedNodes();
		const TArray<TArray<UMaterialExpression*>>& CreatedByNode = Emitter.GetCreatedExpressionsByNode();
		if (EmittedNodes.Num() != Graph.Nodes.Num())
		{
			return false;
		}

		// ----- what every IR node takes: its own expression, and whatever else was made for it, stacked to its left
		IR::FIRLayoutOptions Options;
		Options.Style = Style;
		Options.NodeSizes.SetNum(Graph.Nodes.Num());

		TArray<Private::FLayoutNodeSize> OwnSize;
		TArray<int32> HelperColumnWidth;
		TArray<bool> OwnsExpression;
		OwnSize.SetNum(Graph.Nodes.Num());
		HelperColumnWidth.Init(0, Graph.Nodes.Num());
		OwnsExpression.Init(false, Graph.Nodes.Num());

		for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
		{
			UMaterialExpression* Own = EmittedNodes[NodeIndex].Expression;
			// A Convert or a Broadcast is no node of its own: it answers with the expression of what it reads. Only the
			// node that MADE an expression places it, or the pass-through further right would drag it along.
			OwnsExpression[NodeIndex] = Own && CreatedByNode.IsValidIndex(NodeIndex) && CreatedByNode[NodeIndex].Contains(Own);
			if (!OwnsExpression[NodeIndex])
			{
				// Next to no room: the sink gets none from the layout itself, and a pass-through has nothing to show.
				Options.NodeSizes[NodeIndex].Width = 1;
				Options.NodeSizes[NodeIndex].Height = 1;
				continue;
			}

			OwnSize[NodeIndex] = Private::EstimateMaterialNodeSize(Own);
			int32 HelperHeight = 0;
			if (CreatedByNode.IsValidIndex(NodeIndex))
			{
				for (UMaterialExpression* Helper : CreatedByNode[NodeIndex])
				{
					if (!Helper || Helper == Own)
					{
						continue;
					}
					const Private::FLayoutNodeSize HelperSize = Private::EstimateMaterialNodeSize(Helper);
					HelperColumnWidth[NodeIndex] = FMath::Max(HelperColumnWidth[NodeIndex], HelperSize.Width);
					HelperHeight += HelperSize.Height + (HelperHeight > 0 ? IRLayoutApply::HelperGapY : 0);
				}
			}

			IR::FIRLayoutNodeSize& Size = Options.NodeSizes[NodeIndex];
			Size.Width = OwnSize[NodeIndex].Width + (HelperColumnWidth[NodeIndex] > 0 ? HelperColumnWidth[NodeIndex] + IRLayoutApply::HelperGapX : 0);
			Size.Height = FMath::Max(OwnSize[NodeIndex].Height, HelperHeight);
		}

		IR::FIRLayoutResult Layout;
		IR::LayoutDreamShaderIRGraph(Graph, Options, Layout);
		if (Layout.Positions.Num() != Graph.Nodes.Num())
		{
			return false;
		}

		// ----- coordinates
		for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
		{
			UMaterialExpression* Own = EmittedNodes[NodeIndex].Expression;
			if (!OwnsExpression[NodeIndex] || !Layout.Placed[NodeIndex])
			{
				continue;
			}

			const FIntPoint Origin = Layout.Positions[NodeIndex];
			const int32 OwnX = Origin.X + (HelperColumnWidth[NodeIndex] > 0 ? HelperColumnWidth[NodeIndex] + IRLayoutApply::HelperGapX : 0);
			IRLayoutApply::SetExpressionPosition(Own, OwnX, Origin.Y);

			int32 HelperY = Origin.Y;
			if (CreatedByNode.IsValidIndex(NodeIndex))
			{
				for (UMaterialExpression* Helper : CreatedByNode[NodeIndex])
				{
					if (!Helper || Helper == Own)
					{
						continue;
					}
					IRLayoutApply::SetExpressionPosition(Helper, Origin.X, HelperY);
					HelperY += Private::EstimateMaterialNodeSize(Helper).Height + IRLayoutApply::HelperGapY;
				}
			}
		}

		// ----- the reroutes between the boxes of the block style
		if (Style == IR::EIRLayoutStyle::Blocks)
		{
			IRLayoutApply::MakeBlockBridges(Material, MaterialFunction, Graph, Layout, Emitter);
		}

		// ----- boxes: the layout's own first and outermost first, the author's own boxes after them
		for (const IR::FIRLayoutComment& Box : Layout.Comments)
		{
			IRLayoutApply::AddCommentBox(Material, MaterialFunction, Box, Box.bGenerated);
		}

		// ----- the material's own node
		if (Material)
		{
			Material->EditorX = Layout.RootPosition.X;
			Material->EditorY = Layout.RootPosition.Y;
			if (Material->MaterialGraph && Material->MaterialGraph->RootNode)
			{
				Material->MaterialGraph->RootNode->NodePosX = Layout.RootPosition.X;
				Material->MaterialGraph->RootNode->NodePosY = Layout.RootPosition.Y;
			}
		}

		return true;
	}
}
