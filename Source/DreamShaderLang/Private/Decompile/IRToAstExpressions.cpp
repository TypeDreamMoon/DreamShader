// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// BuildDreamShaderAstFromIR, one graph: which values get a name, and the expression of every value.
//
// A graph is a DAG and source is a tree, so the first question is what is written once and referred to by name
// and what is written in place. A value gets a local when
//
//   1. the asset's hints say the author named it (FIRNode::DebugName on the node that IS the variable: the one
//      none of whose readers carries the same name -- the builder stamps a statement's name on every node the
//      statement made, SetDebugName);
//   2. it is read more than once and is not a leaf -- counting a read through a value that is itself restated as
//      one read per restatement;
//   3. it sits in another `#pragma region` than the statement it would be written into;
//   4. it is a Constant on a pin that has a `Const*` twin, which the builder would fold into the twin otherwise;
//   5. its expression got too long or too deep to read (decided while building, IRToAstStatements.cpp).
//
// Everything else is inline. Uniforms and function inputs are names already; a GetMaterialAttributes is never a
// node in source (`material.Attribute`); a reflected node of several outputs is its call restated under a selector
// per output, which dedupe folds back into the one node.
//
// Every expression below is the spelling the IR builder lowers back to the node it came from; where two spellings
// lower alike the shorter one wins. The comments name the forward rule.

#include "IRToAstInternal.h"

#include "IR/IRCoreOps.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangToken.h"

#define LOCTEXT_NAMESPACE "DreamShaderIRToAstExpressions"

namespace UE::DreamShader::Lang::DecompileAst
{
	static const TCHAR* const GDecompileSwizzleComponents = TEXT("xyzw");
	static const TCHAR* const GDecompileSampleChannels[] = { TEXT(""), TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };

	/** A value a local can hold. A Node-typed or untyped value has no spelling to declare it with. */
	static bool IsDeclarableValueType(const FIRType& Type)
	{
		return Type.GraphComponentCount() > 0
			|| Type.IsBool()
			|| Type.IsMaterial()
			|| Type.IsTexture()
			|| Type.Kind == IR::EIRTypeKind::Substrate;
	}

	/** A name that can stand before `=` in an argument list. */
	static bool CanBeArgumentName(const FString& Name)
	{
		ELangKeyword Keyword = ELangKeyword::None;
		return IsIdentifierText(Name) && !TryGetLangKeyword(Name, Keyword);
	}

	static bool IsScalarComparisonOp(const EIROp Op)
	{
		switch (Op)
		{
		case EIROp::Less:
		case EIROp::LessEqual:
		case EIROp::Greater:
		case EIROp::GreaterEqual:
		case EIROp::Equal:
		case EIROp::NotEqual:
			return true;
		default:
			return false;
		}
	}

	/** The operator a core op is written with, when it is one (the inverse of FindCoreOpForBinary). */
	static bool TryFindBinaryOperator(const EIROp Op, EBinaryOp& OutOperator)
	{
		static const EBinaryOp Candidates[] =
		{
			EBinaryOp::Multiply, EBinaryOp::Divide, EBinaryOp::Modulo,
			EBinaryOp::Add, EBinaryOp::Subtract,
			EBinaryOp::Less, EBinaryOp::LessEqual, EBinaryOp::Greater, EBinaryOp::GreaterEqual,
			EBinaryOp::Equal, EBinaryOp::NotEqual,
			EBinaryOp::LogicalAnd, EBinaryOp::LogicalOr,
		};
		for (const EBinaryOp Candidate : Candidates)
		{
			const IR::FIRCoreOpInfo* Info = IR::FindCoreOpForBinary(Candidate);
			if (Info && Info->Op == Op)
			{
				OutOperator = Candidate;
				return true;
			}
		}
		return false;
	}

	static bool TryFindUnaryOperator(const EIROp Op, EUnaryOp& OutOperator)
	{
		static const EUnaryOp Candidates[] = { EUnaryOp::Negate, EUnaryOp::LogicalNot };
		for (const EUnaryOp Candidate : Candidates)
		{
			const IR::FIRCoreOpInfo* Info = IR::FindCoreOpForUnary(Candidate);
			if (Info && Info->Op == Op)
			{
				OutOperator = Candidate;
				return true;
			}
		}
		return false;
	}

	/** `(a < b)`: an operand whose own operators would bind looser than the cast or member put around it. */
	static FExprPtr ParenthesizeIfCompound(FExprPtr Expr)
	{
		if (Expr && (Expr->Is<FBinaryExpr>() || Expr->Is<FConditionalExpr>() || Expr->Is<FUnaryExpr>() || Expr->Is<FCastExpr>()))
		{
			TUniquePtr<FParenExpr> Paren = MakeUnique<FParenExpr>();
			Paren->Inner = MoveTemp(Expr);
			return Paren;
		}
		return Expr;
	}

	// ------------------------------------------------------------------------------ construction

	FGraphWriter::FGraphWriter(FModuleWriter& InOwner, const FIRProduct& InProduct, FNameScope& InFunctionNames)
		: Owner(InOwner)
		, Product(InProduct)
		, Graph(InProduct.Graph)
		, FunctionNames(InFunctionNames)
	{
	}

	void FGraphWriter::BindInput(const int32 NodeIndex, const FString& Identifier)
	{
		InputIdentifiers.Add(NodeIndex, Identifier);
	}

	void FGraphWriter::AddRoot(FRootSpec&& Root)
	{
		Roots.Add(MoveTemp(Root));
	}

	FText FGraphWriter::DescribeNode(const int32 NodeIndex) const
	{
		if (!Graph.Nodes.IsValidIndex(NodeIndex))
		{
			return FText::Format(LOCTEXT("DescribeMissingNode", "node {0} of '{1}'"), FText::AsNumber(NodeIndex), FText::FromString(Product.Name));
		}

		const FIRNode& Node = Graph.Nodes[NodeIndex];
		FString Label = IR::LexToString(Node.Op);
		if (!Node.ClassName.IsEmpty())
		{
			Label += TEXT(" ");
			Label += Node.ClassName;
		}
		if (!Node.DebugName.IsEmpty())
		{
			Label += TEXT(" '");
			Label += Node.DebugName;
			Label += TEXT("'");
		}
		return FText::Format(
			LOCTEXT("DescribeNode", "node {0} ({1}) of '{2}'"),
			FText::AsNumber(NodeIndex),
			FText::FromString(Label),
			FText::FromString(Product.Name));
	}

	int32 FGraphWriter::RegionOfNode(const int32 NodeIndex) const
	{
		if (!Graph.Nodes.IsValidIndex(NodeIndex))
		{
			return INDEX_NONE;
		}
		const int32 Region = Graph.Nodes[NodeIndex].Region;
		return Graph.Regions.IsValidIndex(Region) ? Region : INDEX_NONE;
	}

	// ---------------------------------------------------------------------------------- analysis

	void FGraphWriter::Analyze()
	{
		Plans.SetNum(Graph.Nodes.Num());
		Readers.SetNum(Graph.Nodes.Num());

		MarkReachable();

		// The emitter's order: every operand before its user. A node on a cycle is in neither list, is never built,
		// and whoever reads it reports DSH9078.
		for (const int32 NodeIndex : Graph.TopologicalOrder())
		{
			if (Plans[NodeIndex].bReachable)
			{
				Order.Add(NodeIndex);
			}
		}

		for (const int32 NodeIndex : Order)
		{
			FNodePlan& Plan = Plans[NodeIndex];
			Plan.Shape = ClassifyNode(NodeIndex);
			Plan.Slots.SetNum(FMath::Max(Graph.Nodes[NodeIndex].Outputs.Num(), 1));
		}

		CountReads();
		DecideNames();
	}

	void FGraphWriter::MarkReachable()
	{
		TArray<int32> Pending;
		const auto Visit = [this, &Pending](const int32 NodeIndex)
		{
			if (Graph.Nodes.IsValidIndex(NodeIndex) && !Plans[NodeIndex].bReachable)
			{
				Plans[NodeIndex].bReachable = true;
				Pending.Add(NodeIndex);
			}
		};

		for (const FRootSpec& Root : Roots)
		{
			Visit(Root.Kind == ERootKind::Statement ? Root.Node : Root.Value.Node);
		}

		TArray<FIRValue> Values;
		while (Pending.Num() > 0)
		{
			const int32 NodeIndex = Pending.Pop();
			FIRGraph::CollectInputValues(Graph.Nodes[NodeIndex], Values);
			for (const FIRValue& Value : Values)
			{
				Visit(Value.Node);
			}
		}
	}

	bool FGraphWriter::IsUniformAlias(const int32 NodeIndex) const
	{
		// IRBuilderMaterial.cpp MakeGlobalValue: a `uniform float3` is a float4 Parameter and a Swizzle "xyz" after it.
		// That Swizzle is the uniform, as far as source is concerned.
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		if (Node.Op != EIROp::Swizzle || Node.Operands.Num() != 1 || Node.Operands[0].Output != 0 || !Graph.Nodes.IsValidIndex(Node.Operands[0].Node))
		{
			return false;
		}

		const FIRNode& Operand = Graph.Nodes[Node.Operands[0].Node];
		if (Operand.Op != EIROp::Parameter)
		{
			return false;
		}

		const FUniformModel* Uniform = Owner.FindUniform(Operand);
		const FIRProperty* Mask = Node.FindProperty(IR::Prop::Mask);
		return Uniform
			&& Mask
			&& Uniform->Width < Uniform->NodeWidth
			&& Mask->Value.S.Equals(FString(GDecompileSwizzleComponents).Left(Uniform->Width), ESearchCase::CaseSensitive);
	}

	FGraphWriter::ENodeShape FGraphWriter::ClassifyNode(const int32 NodeIndex) const
	{
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		switch (Node.Op)
		{
		case EIROp::Parameter:
		case EIROp::TextureParameter:
		case EIROp::FunctionInput:
			return ENodeShape::Identifier;

		case EIROp::Swizzle:
			return IsUniformAlias(NodeIndex) ? ENodeShape::Identifier : ENodeShape::Single;

		case EIROp::TextureSample:
			return ENodeShape::Sample;

		case EIROp::GetMaterialAttributes:
			return ENodeShape::Attributes;

		case EIROp::MakeMaterialAttributes:
		case EIROp::SetMaterialAttributes:
			return ENodeShape::MaterialGroup;

		case EIROp::MaterialSink:
		case EIROp::FunctionOutput:
			return ENodeShape::Root;

		case EIROp::Reflected:
			if (Node.Outputs.Num() == 0)
			{
				return ENodeShape::Root;
			}
			return Node.Outputs.Num() > 1 ? ENodeShape::MultiOutput : ENodeShape::Single;

		case EIROp::FunctionCall:
		case EIROp::Custom:
		{
			const FCallable* Callable = Owner.FindCallable(Node);
			return (Callable && Callable->HasOutParams()) ? ENodeShape::CallStatement : ENodeShape::Single;
		}

		default:
			break;
		}
		return ENodeShape::Single;
	}

	int32 FGraphWriter::SlotOfValue(const FIRValue& Value) const
	{
		if (!Plans.IsValidIndex(Value.Node))
		{
			return 0;
		}
		// A TextureSample's R, G, B and A are views of the one sample.
		return Plans[Value.Node].Shape == ENodeShape::Sample ? 0 : FMath::Max(Value.Output, 0);
	}

	void FGraphWriter::CountReads()
	{
		const auto AddEdge = [this](const FIRValue& Value, FReadEdge&& Edge)
		{
			if (!Graph.IsValidValue(Value) || !Plans[Value.Node].bReachable)
			{
				return;
			}
			FNodePlan& Plan = Plans[Value.Node];
			Edge.Slot = SlotOfValue(Value);
			if (Plan.Slots.IsValidIndex(Edge.Slot))
			{
				++Plan.Slots[Edge.Slot].DirectReads;
			}
			Readers[Value.Node].Add(MoveTemp(Edge));
		};

		for (const int32 NodeIndex : Order)
		{
			const FIRNode& Node = Graph.Nodes[NodeIndex];

			for (int32 OperandIndex = 0; OperandIndex < Node.Operands.Num(); ++OperandIndex)
			{
				// `a > b ? x : y` writes x once however many of the If's three branches it is wired to.
				if (Node.Op == EIROp::Compare && OperandIndex > 2)
				{
					bool bSeen = false;
					for (int32 Earlier = 2; Earlier < OperandIndex; ++Earlier)
					{
						bSeen = bSeen || Node.Operands[Earlier] == Node.Operands[OperandIndex];
					}
					if (bSeen)
					{
						continue;
					}
				}

				FReadEdge Edge;
				Edge.Reader = NodeIndex;
				AddEdge(Node.Operands[OperandIndex], MoveTemp(Edge));
			}

			for (const FIRInput& Input : Node.Inputs)
			{
				FReadEdge Edge;
				Edge.Reader = NodeIndex;
				Edge.Pin = Input.Pin;
				AddEdge(Input.Value, MoveTemp(Edge));
			}
		}

		for (const FRootSpec& Root : Roots)
		{
			if (Root.Kind == ERootKind::Statement)
			{
				continue;
			}
			FReadEdge Edge;
			Edge.bRoot = true;
			Edge.RootKind = Root.Kind;
			Edge.Pin = Root.PinName;
			AddEdge(Root.Value, MoveTemp(Edge));
		}
	}

	bool FGraphWriter::IsLeafLike(const int32 NodeIndex) const
	{
		// Cheap to say twice: a name, a literal, a channel of one, a node with nothing wired into it.
		int32 Current = NodeIndex;
		for (int32 Guard = 0; Guard < 8 && Graph.Nodes.IsValidIndex(Current); ++Guard)
		{
			const FIRNode& Node = Graph.Nodes[Current];
			switch (Plans[Current].Shape)
			{
			case ENodeShape::Identifier:
			case ENodeShape::Attributes:
				return true;
			case ENodeShape::Single:
				break;
			default:
				return false;
			}

			if (Node.Op == EIROp::Constant)
			{
				return true;
			}
			if (Node.Op == EIROp::Reflected)
			{
				for (const FIRInput& Input : Node.Inputs)
				{
					if (Input.Value.IsValid())
					{
						return false;
					}
				}
				return true;
			}
			if ((Node.Op == EIROp::Swizzle || Node.Op == EIROp::Convert || Node.Op == EIROp::Broadcast) && Node.Operands.Num() == 1)
			{
				Current = Node.Operands[0].Node;
				continue;
			}
			return false;
		}
		return false;
	}

	bool FGraphWriter::OwnsHintName(const int32 NodeIndex) const
	{
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		if (Node.DebugName.IsEmpty())
		{
			return false;
		}

		// The builder names EVERY unnamed node a statement makes after what the statement assigns, so
		// `m.Opacity = Mask(UE.TexCoord(...))` leaves the TexCoord called "Opacity" when the inlined helper's own locals
		// took the nodes in between. That is the name of a write, and a two-component UV declared as `Opacity` reads
		// as a mistake; only a name no root write goes by is a variable's.
		if (!bWriteNamesKnown)
		{
			bWriteNamesKnown = true;
			for (const TArray<FReadEdge>& Edges : Readers)
			{
				for (const FReadEdge& Edge : Edges)
				{
					const bool bKnown = WriteNames.ContainsByPredicate([&Edge](const FString& Name) { return Name.Equals(Edge.Pin, ESearchCase::CaseSensitive); });
					if (Edge.bRoot && !Edge.Pin.IsEmpty() && !bKnown)
					{
						WriteNames.Add(Edge.Pin);
					}
				}
			}
		}
		if (WriteNames.ContainsByPredicate([&Node](const FString& Name) { return Name.Equals(Node.DebugName, ESearchCase::CaseSensitive); }))
		{
			return false;
		}

		for (const FReadEdge& Edge : Readers[NodeIndex])
		{
			// `m.Roughness = x * 2;` names the Multiply "Roughness" (IRBuilderExpressions.cpp, the MaterialAttribute
			// branch of an assignment): the name of a write, not of a variable. The same goes for an `out` parameter.
			if (!Edge.Pin.IsEmpty() && Edge.Pin.Equals(Node.DebugName, ESearchCase::CaseSensitive))
			{
				if (Edge.bRoot)
				{
					return false;
				}
				const ENodeShape ReaderShape = Plans[Edge.Reader].Shape;
				if (ReaderShape == ENodeShape::MaterialGroup)
				{
					return false;
				}
			}
			if (Edge.bRoot)
			{
				continue;
			}

			// A reader with the same name was made by the same statement: this node is inside the variable's
			// expression, not the variable.
			if (Graph.Nodes[Edge.Reader].DebugName.Equals(Node.DebugName, ESearchCase::CaseSensitive))
			{
				return false;
			}
		}
		return true;
	}

	bool FGraphWriter::FeedsTwinPin(const int32 NodeIndex) const
	{
		// IRBuilderExpressions.cpp, the reflected call: a constant argument on a pin with a `Const*` twin becomes the
		// twin property and the Constant node goes away. A Constant that is wired to such a pin in the asset stays a
		// node only when it reaches the call through a variable.
		if (Owner.GetOptions().bReadable)
		{
			return false;
		}

		const FIRNode& Node = Graph.Nodes[NodeIndex];
		const FIRProperty* Value = Node.FindProperty(IR::Prop::Value);
		if (Node.Op != EIROp::Constant || !Value || Value->Value.Kind != IR::EIRPropertyKind::Float4)
		{
			return false;
		}
		for (int32 Component = 1; Component < FMath::Clamp(Value->Value.N, 1, 4); ++Component)
		{
			if (Value->Value.V[Component] != Value->Value.V[0])
			{
				return false;
			}
		}

		const IR::FBuiltinCatalog& Catalog = Owner.GetCatalog();
		for (const FReadEdge& Edge : Readers[NodeIndex])
		{
			if (Edge.bRoot || Edge.Pin.IsEmpty())
			{
				continue;
			}
			const FIRNode& Reader = Graph.Nodes[Edge.Reader];
			if (Reader.Op != EIROp::Reflected || !Catalog.Expressions.IsValidIndex(Reader.CatalogIndex))
			{
				continue;
			}
			const IR::FCatalogExpression& Class = Catalog.Expressions[Reader.CatalogIndex];
			const int32 PinIndex = Class.FindInput(Edge.Pin);
			if (PinIndex != INDEX_NONE && !Class.Inputs[PinIndex].ConstPropertyName.IsEmpty())
			{
				return true;
			}
		}
		return false;
	}

	int32 FGraphWriter::ReaderWeight(const int32 ReaderIndex) const
	{
		const FNodePlan& Plan = Plans[ReaderIndex];
		switch (Plan.Shape)
		{
		case ENodeShape::Single:
		case ENodeShape::Sample:
			return Plan.Slots[0].bNamed ? 1 : FMath::Max(Plan.Slots[0].EffectiveReads, 1);

		case ENodeShape::MultiOutput:
		case ENodeShape::Attributes:
		{
			// Restated once per declared output and once per inline read of the others.
			int32 Weight = 0;
			for (const FValuePlan& Slot : Plan.Slots)
			{
				if (Slot.DirectReads > 0)
				{
					Weight += Slot.bNamed ? 1 : FMath::Max(Slot.EffectiveReads, 1);
				}
			}
			return FMath::Max(Weight, 1);
		}

		default:
			break;
		}
		// A statement of its own reads each of its inputs once.
		return 1;
	}

	void FGraphWriter::DecideNames()
	{
		// A material that reaches the end of the body as writes on top of what came in is written into the parameter
		// itself -- `m.Roughness = x;` -- rather than into a local handed over afterwards. A fresh set of attributes
		// can only be written that way into a material that starts empty: the entry's, and a blend's result, which no
		// FunctionInput feeds (Root.Node is INDEX_NONE). A layer's arrives full.
		for (const FRootSpec& Root : Roots)
		{
			if ((Root.Kind != ERootKind::SinkWhole && Root.Kind != ERootKind::OutputMaterial)
				|| !Graph.IsValidValue(Root.Value)
				|| Root.Value.Output != 0
				|| Readers[Root.Value.Node].Num() != 1)
			{
				continue;
			}
			const EIROp Op = Graph.Nodes[Root.Value.Node].Op;
			const bool bStartsEmpty = Root.Kind == ERootKind::SinkWhole || Root.Node == INDEX_NONE;
			if (Op == EIROp::SetMaterialAttributes || (Op == EIROp::MakeMaterialAttributes && bStartsEmpty))
			{
				Plans[Root.Value.Node].bWrittenThroughRoot = true;
			}
		}

		// Users before operands: what a value's readers turned out to be decides how often it is written.
		for (int32 OrderIndex = Order.Num() - 1; OrderIndex >= 0; --OrderIndex)
		{
			const int32 NodeIndex = Order[OrderIndex];
			FNodePlan& Plan = Plans[NodeIndex];
			const FIRNode& Node = Graph.Nodes[NodeIndex];
			const TArray<FReadEdge>& Edges = Readers[NodeIndex];

			for (const FReadEdge& Edge : Edges)
			{
				if (Plan.Slots.IsValidIndex(Edge.Slot))
				{
					Plan.Slots[Edge.Slot].EffectiveReads += Edge.bRoot ? 1 : ReaderWeight(Edge.Reader);
				}
			}

			if (Plan.Shape == ENodeShape::Identifier)
			{
				continue;
			}
			if (Plan.Shape == ENodeShape::Root || Plan.Shape == ENodeShape::MaterialGroup || Plan.Shape == ENodeShape::CallStatement)
			{
				// Statements of their own. A group written through a root is as many statements as it has attributes,
				// each in the region of the value it writes, so it pins nothing.
				Plan.HomeRegion = RegionOfNode(NodeIndex);
				Plan.bHomeRegionKnown = !Plan.bWrittenThroughRoot;
				continue;
			}

			Plan.bOwnsHintName = OwnsHintName(NodeIndex);
			const bool bLeaf = IsLeafLike(NodeIndex);
			const int32 OwnRegion = RegionOfNode(NodeIndex);

			int32 LowestUsedSlot = INDEX_NONE;
			for (int32 SlotIndex = 0; SlotIndex < Plan.Slots.Num(); ++SlotIndex)
			{
				if (Plan.Slots[SlotIndex].DirectReads > 0)
				{
					LowestUsedSlot = SlotIndex;
					break;
				}
			}

			bool bAnyNamed = false;
			for (int32 SlotIndex = 0; SlotIndex < Plan.Slots.Num(); ++SlotIndex)
			{
				FValuePlan& Slot = Plan.Slots[SlotIndex];
				if (Slot.DirectReads <= 0)
				{
					continue;
				}

				const FIRType Type = Node.Outputs.IsValidIndex(SlotIndex) ? Node.Outputs[SlotIndex] : FIRType::Error();
				if (!IsDeclarableValueType(Type))
				{
					continue;
				}

				if (Plan.bOwnsHintName && SlotIndex == LowestUsedSlot)
				{
					Slot.bNamed = true;
				}
				else if (Plan.Shape == ENodeShape::Attributes)
				{
					// `m.Roughness` costs nothing to say again; the material it reads is what gets a name.
				}
				else if (Slot.EffectiveReads >= 2 && !bLeaf)
				{
					Slot.bNamed = true;
				}
				else if (Node.Op == EIROp::Constant && FeedsTwinPin(NodeIndex))
				{
					Slot.bNamed = true;
				}
				else if (!bLeaf && Slot.DirectReads == 1)
				{
					// One reader: written into that reader's statement, unless the statement is in another region.
					for (const FReadEdge& Edge : Edges)
					{
						if (Edge.Slot != SlotIndex)
						{
							continue;
						}
						if (Edge.bRoot)
						{
							// A write takes the region of the value it writes. `return` cannot: it has to come last.
							Slot.bNamed = Edge.RootKind == ERootKind::OutputReturn && OwnRegion != INDEX_NONE;
						}
						else if (Plans[Edge.Reader].bHomeRegionKnown)
						{
							Slot.bNamed = Plans[Edge.Reader].HomeRegion != OwnRegion;
						}
						break;
					}
				}

				bAnyNamed = bAnyNamed || Slot.bNamed;
			}

			if (bAnyNamed)
			{
				Plan.HomeRegion = OwnRegion;
				Plan.bHomeRegionKnown = true;
			}
			else if (Edges.Num() == 1)
			{
				const FReadEdge& Edge = Edges[0];
				if (Edge.bRoot)
				{
					Plan.HomeRegion = Edge.RootKind == ERootKind::OutputReturn ? INDEX_NONE : OwnRegion;
					Plan.bHomeRegionKnown = true;
				}
				else if (Plans[Edge.Reader].bWrittenThroughRoot)
				{
					Plan.HomeRegion = OwnRegion;
					Plan.bHomeRegionKnown = true;
				}
				else
				{
					Plan.HomeRegion = Plans[Edge.Reader].HomeRegion;
					Plan.bHomeRegionKnown = Plans[Edge.Reader].bHomeRegionKnown;
				}
			}
		}
	}

	FString FGraphWriter::MakeAutoName(const int32 NodeIndex, const int32 Slot) const
	{
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		const FString OutputName = Node.OutputNames.IsValidIndex(Slot) ? Node.OutputNames[Slot] : FString();

		FString Base;
		if (!Node.DebugName.IsEmpty())
		{
			// Inside a named statement, or a second output of a named node: the name it is known by, made unique.
			Base = Node.DebugName;
			if (Plans[NodeIndex].Shape == ENodeShape::MultiOutput && !OutputName.IsEmpty())
			{
				Base += TEXT("_");
				Base += OutputName;
			}
		}
		else
		{
			switch (Node.Op)
			{
			case EIROp::Reflected:
				Base = Node.ClassName;
				if (Plans[NodeIndex].Shape == ENodeShape::MultiOutput && !OutputName.IsEmpty())
				{
					Base += TEXT("_");
					Base += OutputName;
				}
				break;
			case EIROp::FunctionCall:
			case EIROp::Custom:
			{
				const FCallable* Callable = Owner.FindCallable(Node);
				Base = Callable ? Callable->Identifier : FString(TEXT("Call"));
				Base += TEXT("_Result");
				break;
			}
			case EIROp::GetMaterialAttributes:
				Base = OutputName;
				break;
			case EIROp::TextureSample:
				Base = TEXT("Sample");
				break;
			case EIROp::Swizzle:
				Base = TEXT("Mask");
				break;
			case EIROp::Append:
				Base = TEXT("Combined");
				break;
			case EIROp::Select:
			case EIROp::Compare:
			case EIROp::StaticSwitch:
				Base = TEXT("Selected");
				break;
			default:
				Base = IR::LexToString(Node.Op);
				// The readable form's `A * w` over a Substrate value is a Coverage Weight (IRRaise.cpp, sugar S1), not a
				// Multiply: named as the node is in the plain form, so the local says what the graph has.
				if (Node.Outputs.Num() == 1 && Node.Outputs[0].Kind == IR::EIRTypeKind::Substrate)
				{
					if (Node.Op == EIROp::Add) { Base = TEXT("SubstrateAdd"); }
					else if (Node.Op == EIROp::Multiply) { Base = TEXT("SubstrateWeight"); }
					else if (Node.Op == EIROp::Lerp) { Base = TEXT("SubstrateHorizontalMixing"); }
				}
				break;
			}
		}
		return MakeSourceIdentifier(Base, TEXT("Value"));
	}

	// ------------------------------------------------------------------------------ expressions

	FExprPtr FGraphWriter::TakeZero(const FIRType& Type)
	{
		if (Type.IsBool())
		{
			return MakeBoolExpr(false);
		}
		if (Type.GraphComponentCount() > 0 || Type.IsError())
		{
			// A scalar broadcasts to whatever width the reader wants.
			return MakeNumberExpr(0.0);
		}
		return FExprPtr();
	}

	FExprPtr FGraphWriter::FailValue(const int32 NodeIndex, const FText& Why)
	{
		Owner.Error(TEXT("DSH9078"), FText::Format(
			LOCTEXT("ValueHasNoSourceForm", "{0} cannot be written as source: {1}. '0.0' stands in its place."),
			DescribeNode(NodeIndex),
			Why));
		return MakeNumberExpr(0.0);
	}

	void FGraphWriter::AddDependency(const int32 StatementIndex)
	{
		if (StatementIndex != INDEX_NONE)
		{
			CurrentDependencies.AddUnique(StatementIndex);
		}
	}

	FExprPtr FGraphWriter::SelectOutput(FExprPtr Call, const FIRNode& Node, const int32 Slot) const
	{
		// `UE.SceneTexture(...).Color`, or `[k]` for an output whose name is not an identifier (legacy rule L3a, every
		// source). Output 0 is spelled out too: a node-typed call converts to it silently, but only where the reader's
		// type asks for exactly that.
		const FString OutputName = Node.OutputNames.IsValidIndex(Slot) ? Node.OutputNames[Slot] : FString();
		if (CanBeArgumentName(OutputName))
		{
			return MakeMemberExpr(MoveTemp(Call), OutputName);
		}
		return MakeIndexExpr(MoveTemp(Call), Slot);
	}

	FExprPtr FGraphWriter::TakeValue(const FIRValue& Value)
	{
		if (!Graph.IsValidValue(Value))
		{
			return FailValue(Value.Node, LOCTEXT("TakeInvalidValue", "a reader names an output the graph does not have"));
		}

		const int32 NodeIndex = Value.Node;
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		FNodePlan& Plan = Plans[NodeIndex];

		switch (Plan.Shape)
		{
		case ENodeShape::Identifier:
		{
			if (Node.Op == EIROp::FunctionInput)
			{
				if (const FString* Identifier = InputIdentifiers.Find(NodeIndex))
				{
					return MakeIdentifierExpr(*Identifier);
				}
				return FailValue(NodeIndex, LOCTEXT("TakeUnboundInput", "the function input is not one of the function's parameters"));
			}
			if (Node.Op == EIROp::Swizzle)
			{
				// The leading mask that narrows a float4 parameter to the declared width: the uniform itself.
				return TakeValue(Node.Operands[0]);
			}
			if (const FUniformModel* Uniform = Owner.FindUniform(Node))
			{
				return MakeIdentifierExpr(Uniform->Identifier);
			}
			return FailValue(NodeIndex, LOCTEXT("TakeUnnamedParameter", "the parameter node carries no ParameterName"));
		}

		case ENodeShape::Single:
		case ENodeShape::Sample:
		{
			if (Plan.Shape == ENodeShape::Single && Value.Output != 0)
			{
				return FailValue(NodeIndex, LOCTEXT("TakeExtraOutput", "a reader names an output past the one this node has in source"));
			}

			FValuePlan& Slot = Plan.Slots[0];
			FExprPtr Result;
			if (Slot.bNamed)
			{
				if (Slot.Name.IsEmpty())
				{
					return FailValue(NodeIndex, LOCTEXT("TakeBeforeDeclared", "it is read before the statement that declares it (the graph has a cycle)"));
				}
				AddDependency(Slot.Statement);
				Result = MakeIdentifierExpr(Slot.Name);
			}
			else
			{
				if (!Slot.Pending)
				{
					return FailValue(NodeIndex, LOCTEXT("TakeBeforeBuilt", "it is read before it was written (the graph has a cycle)"));
				}
				for (const int32 Dependency : Slot.PendingDependencies)
				{
					AddDependency(Dependency);
				}
				Result = Slot.DirectReads <= 1 ? MoveTemp(Slot.Pending) : CloneExpr(*Slot.Pending);
			}

			if (Plan.Shape == ENodeShape::Sample && Value.Output > 0 && Value.Output < static_cast<int32>(UE_ARRAY_COUNT(GDecompileSampleChannels)))
			{
				// IR slots RGBA, R, G, B, A: the single channels are `.r` .. `.a` of the sample, which the builder turns
				// back into the node's own channel outputs.
				return MakeMemberExpr(ParenthesizeIfCompound(MoveTemp(Result)), GDecompileSampleChannels[Value.Output]);
			}
			return Result;
		}

		case ENodeShape::MultiOutput:
		case ENodeShape::Attributes:
		{
			const int32 SlotIndex = FMath::Max(Value.Output, 0);
			if (!Plan.Slots.IsValidIndex(SlotIndex))
			{
				return FailValue(NodeIndex, LOCTEXT("TakeMissingOutput", "a reader names an output this node does not have"));
			}

			FValuePlan& Slot = Plan.Slots[SlotIndex];
			if (Slot.bNamed && !Slot.Name.IsEmpty())
			{
				AddDependency(Slot.Statement);
				return MakeIdentifierExpr(Slot.Name);
			}
			if (!Plan.Base)
			{
				return FailValue(NodeIndex, LOCTEXT("TakeBaseBeforeBuilt", "it is read before it was written (the graph has a cycle)"));
			}
			for (const int32 Dependency : Plan.BaseDependencies)
			{
				AddDependency(Dependency);
			}

			if (Plan.Shape == ENodeShape::Attributes)
			{
				const FString Attribute = Node.OutputNames.IsValidIndex(SlotIndex) ? Node.OutputNames[SlotIndex] : FString();
				if (!IsIdentifierText(Attribute))
				{
					return FailValue(NodeIndex, LOCTEXT("TakeUnnamedAttribute", "the attribute it reads has no name"));
				}
				return MakeMemberExpr(ParenthesizeIfCompound(CloneExpr(*Plan.Base)), Attribute);
			}
			return SelectOutput(CloneExpr(*Plan.Base), Node, SlotIndex);
		}

		case ENodeShape::MaterialGroup:
		case ENodeShape::CallStatement:
		{
			const int32 SlotIndex = FMath::Max(Value.Output, 0);
			if (Plan.Slots.IsValidIndex(SlotIndex) && !Plan.Slots[SlotIndex].Name.IsEmpty())
			{
				AddDependency(Plan.Slots[SlotIndex].Statement);
				return MakeIdentifierExpr(Plan.Slots[SlotIndex].Name);
			}
			return FailValue(NodeIndex, LOCTEXT("TakeUndeclaredOutput", "the output a reader names is not one the called function declares"));
		}

		case ENodeShape::Root:
		default:
			break;
		}
		return FailValue(NodeIndex, LOCTEXT("TakeStatement", "it is a statement and has no value"));
	}

	FExprPtr FGraphWriter::BuildConstant(const FIRNode& Node) const
	{
		const FIRProperty* Value = Node.FindProperty(IR::Prop::Value);
		const bool bBool = Node.Outputs.IsValidIndex(0) && Node.Outputs[0].IsBool();
		if (!Value)
		{
			return bBool ? MakeBoolExpr(false) : MakeNumberExpr(0.0);
		}

		switch (Value->Value.Kind)
		{
		case IR::EIRPropertyKind::Float4:
			// `float3(1.0, 0.5, 0.25)`: all-constant parts fold back into the one Constant3Vector (MakeAppend).
			return MakeVectorLiteralExpr(Value->Value.V, Value->Value.N, bBool);
		case IR::EIRPropertyKind::Float:
			return bBool ? MakeBoolExpr(Value->Value.F != 0.0) : MakeNumberExpr(Value->Value.F);
		case IR::EIRPropertyKind::Int:
			return bBool ? MakeBoolExpr(Value->Value.I != 0) : MakeNumberExpr(static_cast<double>(Value->Value.I));
		case IR::EIRPropertyKind::Bool:
			return MakeBoolExpr(Value->Value.B);
		default:
			break;
		}
		return MakeNumberExpr(0.0);
	}

	FExprPtr FGraphWriter::BuildPropertyValue(const FIRPropertyValue& Value, const IR::ECatalogValueType Type) const
	{
		switch (Value.Kind)
		{
		case IR::EIRPropertyKind::Bool:
			return MakeBoolExpr(Value.B);
		case IR::EIRPropertyKind::Int:
			return MakeIntegerExpr(Value.I);
		case IR::EIRPropertyKind::Float:
			return MakeNumberExpr(Value.F);
		case IR::EIRPropertyKind::Float4:
			return MakeVectorLiteralExpr(Value.V, Value.N, /* bBool */ false);
		case IR::EIRPropertyKind::Enum:
			// `SamplerType = Normal`. Only where the catalog says enum: the binder reads a bare word on any other
			// property as a variable when one of that name is in scope.
			if (Type == IR::ECatalogValueType::Enum && IsIdentifierText(Value.S) && !IsReservedIdentifier(Value.S))
			{
				return MakeIdentifierExpr(Value.S);
			}
			return MakeStringExpr(Value.S);
		case IR::EIRPropertyKind::String:
		case IR::EIRPropertyKind::Name:
		case IR::EIRPropertyKind::Object:
			return MakeStringExpr(Value.S);
		case IR::EIRPropertyKind::StringList:
		default:
			break;
		}
		return FExprPtr();
	}

	FExprPtr FGraphWriter::BuildReflectedCall(const int32 NodeIndex, const FIRNode& Node)
	{
		const IR::FBuiltinCatalog& Catalog = Owner.GetCatalog();
		if (!Catalog.Expressions.IsValidIndex(Node.CatalogIndex))
		{
			return FailValue(NodeIndex, LOCTEXT("ReflectedNoCatalogEntry", "its class is not in the builtin catalog"));
		}
		const IR::FCatalogExpression& Class = Catalog.Expressions[Node.CatalogIndex];

		// A Substrate class goes by the name its sources use -- `Substrate.Slab`, the first alias the catalog lists for it --
		// and not by the reflected one (`SubstrateSlabBSDF`), which is the engine's. Both resolve to the same entry.
		const bool bSubstrateClass = Class.Namespace.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive);
		const FString& CalleeName = (bSubstrateClass && Class.Aliases.Num() > 0) ? Class.Aliases[0] : Class.ShortName;
		TUniquePtr<FCallExpr> Call = MakeCallExpr(MakeNamespaceCallee(Class.Namespace.IsEmpty() ? FString(TEXT("UE")) : Class.Namespace, CalleeName));

		// Pins in the class's own order, every one by name: a positional argument means whatever the catalog's
		// canonical order says this year.
		TArray<bool> InputWritten;
		InputWritten.Init(false, Node.Inputs.Num());
		TArray<bool> PropertyWritten;
		PropertyWritten.Init(false, Node.Properties.Num());
		for (int32 PinIndex = 0; PinIndex < Class.Inputs.Num(); ++PinIndex)
		{
			const FString& PinName = Class.Inputs[PinIndex].Name;
			bool bConnected = false;
			for (int32 InputIndex = 0; InputIndex < Node.Inputs.Num(); ++InputIndex)
			{
				const FIRInput& Input = Node.Inputs[InputIndex];
				if (InputWritten[InputIndex] || !Input.Value.IsValid() || !Input.Pin.Equals(PinName, ESearchCase::CaseSensitive))
				{
					continue;
				}
				InputWritten[InputIndex] = true;
				bConnected = true;

				FExprPtr Value = TakeValue(Input.Value);
				if (CanBeArgumentName(PinName))
				{
					AddNamedArgument(*Call, PinName, MoveTemp(Value));
				}
				else
				{
					// `Inputs[2]`, `Customized UV 3`: by the engine's pin index (FArgument::PinIndex).
					AddPinArgument(*Call, PinIndex, MoveTemp(Value));
				}
				break;
			}

			// The `Const*` twin of a pin nothing is wired to is what the pin reads, and is written on the pin, in the pin's
			// place: `UE.LinearInterpolate(A = 0.0, B = x, Alpha = t)`, not with A trailing after the rest.
			const FString& TwinName = Class.Inputs[PinIndex].ConstPropertyName;
			if (!bConnected && !TwinName.IsEmpty() && CanBeArgumentName(PinName))
			{
				for (int32 PropertyIndex = 0; PropertyIndex < Node.Properties.Num(); ++PropertyIndex)
				{
					const FIRProperty& Twin = Node.Properties[PropertyIndex];
					if (PropertyWritten[PropertyIndex] || !Twin.Name.Equals(TwinName, ESearchCase::CaseSensitive))
					{
						continue;
					}
					const int32 TwinIndex = Class.FindProperty(Twin.Name);
					if (FExprPtr Value = BuildPropertyValue(Twin.Value, TwinIndex != INDEX_NONE ? Class.Properties[TwinIndex].Type : IR::ECatalogValueType::Unknown))
					{
						AddNamedArgument(*Call, PinName, MoveTemp(Value));
						PropertyWritten[PropertyIndex] = true;
					}
					break;
				}
			}
		}

		// Legacy rule L4: a Custom class takes a named argument it has no pin for as an input of that name.
		for (int32 InputIndex = 0; InputIndex < Node.Inputs.Num(); ++InputIndex)
		{
			const FIRInput& Input = Node.Inputs[InputIndex];
			if (InputWritten[InputIndex] || !Input.Value.IsValid())
			{
				continue;
			}
			if (!CanBeArgumentName(Input.Pin))
			{
				Owner.Error(TEXT("DSH9078"), FText::Format(
					LOCTEXT("ReflectedUnnamedPin", "{0} has an input '{1}' that is neither a pin of '{2}.{3}' nor a name an argument can carry; it is left unconnected."),
					DescribeNode(NodeIndex),
					FText::FromString(Input.Pin),
					FText::FromString(Class.Namespace),
					FText::FromString(Class.ShortName)));
				continue;
			}
			AddNamedArgument(*Call, Input.Pin, TakeValue(Input.Value));
		}

		for (int32 WrittenIndex = 0; WrittenIndex < Node.Properties.Num(); ++WrittenIndex)
		{
			const FIRProperty& Property = Node.Properties[WrittenIndex];
			if (PropertyWritten[WrittenIndex]
				|| Property.Name.Equals(IR::Prop::ClassSpecifier, ESearchCase::CaseSensitive)
				|| Property.Name.Equals(IR::Prop::LateBoundPins, ESearchCase::CaseSensitive)
				|| Property.Name.Equals(IR::Prop::WrittenPins, ESearchCase::CaseSensitive))
			{
				continue;
			}

			const int32 PropertyIndex = Class.FindProperty(Property.Name);
			FExprPtr Value = BuildPropertyValue(
				Property.Value,
				PropertyIndex != INDEX_NONE ? Class.Properties[PropertyIndex].Type : IR::ECatalogValueType::Unknown);
			if (!Value)
			{
				Owner.Warning(TEXT("DSH9084"), FText::Format(
					LOCTEXT("ReflectedPropertySkipped", "{0} sets '{1}' to a list, which a call has no argument for; the property is left at its default."),
					DescribeNode(NodeIndex),
					FText::FromString(Property.Name)));
				continue;
			}

			// A `Const*` twin is written on its pin -- `UE.Multiply(A = x, B = 2.0)` -- and the builder folds it back.
			FString ArgumentName = Property.Name;
			for (const IR::FCatalogPin& Pin : Class.Inputs)
			{
				if (Pin.ConstPropertyName.Equals(Property.Name, ESearchCase::CaseSensitive) && CanBeArgumentName(Pin.Name))
				{
					const FIRInput* Connected = Node.FindInput(Pin.Name);
					if (!Connected || !Connected->Value.IsValid())
					{
						ArgumentName = Pin.Name;
					}
					break;
				}
			}
			if (!CanBeArgumentName(ArgumentName))
			{
				Owner.Warning(TEXT("DSH9084"), FText::Format(
					LOCTEXT("ReflectedPropertyUnnamed", "{0} sets a property '{1}' whose name an argument cannot carry; it is left at its default."),
					DescribeNode(NodeIndex),
					FText::FromString(Property.Name)));
				continue;
			}
			AddNamedArgument(*Call, ArgumentName, MoveTemp(Value));
		}

		return Call;
	}

	FExprPtr FGraphWriter::BuildReadableReflected(const int32 NodeIndex, const FIRNode& Node)
	{
		// RD-1: `x * 2.0` for `UE.Multiply(A = x, B = 2.0)`. It compiles to a Multiply and a Constant rather than a
		// Multiply with ConstB, so only when the caller asked for the readable form.
		if (!Owner.GetOptions().bReadable || Node.Inputs.Num() != 1 || Node.Properties.Num() != 1 || !Node.Inputs[0].Value.IsValid())
		{
			return FExprPtr();
		}

		EBinaryOp Operator = EBinaryOp::Add;
		if (Node.ClassName.Equals(TEXT("Add"), ESearchCase::CaseSensitive)) { Operator = EBinaryOp::Add; }
		else if (Node.ClassName.Equals(TEXT("Subtract"), ESearchCase::CaseSensitive)) { Operator = EBinaryOp::Subtract; }
		else if (Node.ClassName.Equals(TEXT("Multiply"), ESearchCase::CaseSensitive)) { Operator = EBinaryOp::Multiply; }
		else if (Node.ClassName.Equals(TEXT("Divide"), ESearchCase::CaseSensitive)) { Operator = EBinaryOp::Divide; }
		else
		{
			return FExprPtr();
		}

		const FIRProperty& Twin = Node.Properties[0];
		const bool bTwinIsA = Twin.Name.Equals(TEXT("ConstA"), ESearchCase::CaseSensitive) && Node.Inputs[0].Pin.Equals(TEXT("B"), ESearchCase::CaseSensitive);
		const bool bTwinIsB = Twin.Name.Equals(TEXT("ConstB"), ESearchCase::CaseSensitive) && Node.Inputs[0].Pin.Equals(TEXT("A"), ESearchCase::CaseSensitive);
		if ((!bTwinIsA && !bTwinIsB) || (Twin.Value.Kind != IR::EIRPropertyKind::Float && Twin.Value.Kind != IR::EIRPropertyKind::Int))
		{
			return FExprPtr();
		}

		const double Literal = Twin.Value.Kind == IR::EIRPropertyKind::Float ? Twin.Value.F : static_cast<double>(Twin.Value.I);
		FExprPtr Connected = TakeValue(Node.Inputs[0].Value);
		(void)NodeIndex;
		return bTwinIsA
			? MakeBinaryExpr(Operator, MakeNumberExpr(Literal), MoveTemp(Connected))
			: MakeBinaryExpr(Operator, MoveTemp(Connected), MakeNumberExpr(Literal));
	}

	FExprPtr FGraphWriter::BuildCondition(const FIRValue& Condition)
	{
		FExprPtr Expr = TakeValue(Condition);

		// IRBuilder.cpp MakeConditional looks at the NODE the condition is, through any variable: a scalar comparison
		// there becomes the engine If over the comparison's own operands, and this Select would come back as that one
		// node. A cast keeps the two apart -- it lowers to a Convert, which is no node in the graph.
		if (Graph.Nodes.IsValidIndex(Condition.Node))
		{
			const FIRNode& Node = Graph.Nodes[Condition.Node];
			if (IsScalarComparisonOp(Node.Op)
				&& Node.Operands.Num() == 2
				&& Graph.IsValidValue(Node.Operands[0])
				&& Graph.IsValidValue(Node.Operands[1])
				&& Graph.TypeOf(Node.Operands[0]).GraphComponentCount() == 1
				&& Graph.TypeOf(Node.Operands[1]).GraphComponentCount() == 1)
			{
				return MakeCastExpr(MakeTypeRef(TEXT("float")), ParenthesizeIfCompound(MoveTemp(Expr)));
			}
		}
		return Expr;
	}

	void FGraphWriter::CollectAppendParts(const FIRValue& Value, TArray<FExprPtr>& OutParts)
	{
		// `float3(a, b, c)` is Append(Append(a, b), c) (MakeAppend chains to the left), so a left operand that is an
		// Append written nowhere else hands over its parts. A right-hand one stays `float2(b, c)`: that is how it nests.
		if (Graph.IsValidValue(Value) && Value.Output == 0)
		{
			const FIRNode& Node = Graph.Nodes[Value.Node];
			FNodePlan& Plan = Plans[Value.Node];
			if (Node.Op == EIROp::Append
				&& Plan.Shape == ENodeShape::Single
				&& !Plan.Slots[0].bNamed
				&& Plan.Slots[0].DirectReads == 1
				&& Plan.Slots[0].Pending
				&& Plan.Slots[0].Pending->Is<FCallExpr>())
			{
				for (const int32 Dependency : Plan.Slots[0].PendingDependencies)
				{
					AddDependency(Dependency);
				}
				FExprPtr Inner = MoveTemp(Plan.Slots[0].Pending);
				FCallExpr& InnerCall = static_cast<FCallExpr&>(*Inner);
				for (FArgument& Argument : InnerCall.Arguments)
				{
					OutParts.Add(MoveTemp(Argument.Value));
				}
				return;
			}
		}
		OutParts.Add(TakeValue(Value));
	}

	FExprPtr FGraphWriter::BuildAppend(const FIRNode& Node)
	{
		TArray<FExprPtr> Parts;
		if (Node.Operands.Num() > 0)
		{
			CollectAppendParts(Node.Operands[0], Parts);
		}
		for (int32 OperandIndex = 1; OperandIndex < Node.Operands.Num(); ++OperandIndex)
		{
			Parts.Add(TakeValue(Node.Operands[OperandIndex]));
		}

		const FIRType Type = Node.Outputs.IsValidIndex(0) ? Node.Outputs[0] : FIRType::Float(1);
		return MakeConstructorExpr(MakeValueTypeRef(Type, /* bKeepBool */ false), MoveTemp(Parts));
	}

	FExprPtr FGraphWriter::BuildTextureSample(const int32 NodeIndex, const FIRNode& Node)
	{
		// Operands are always [Texture, UV, Sampler, Level]; `Tex.Sample([Sampler,] UV)` and
		// `Tex.SampleLevel([Sampler,] UV, Level)` are the two spellings that make one (BindTextureSampleMethod).
		if (Node.Operands.Num() < 2 || !Node.Operands[0].IsValid() || !Node.Operands[1].IsValid())
		{
			return FailValue(NodeIndex, LOCTEXT("SampleWithoutOperands", "a texture sample needs its texture and its coordinates"));
		}

		const bool bHasSampler = Node.Operands.IsValidIndex(2) && Node.Operands[2].IsValid();
		const bool bHasLevel = Node.Operands.IsValidIndex(3) && Node.Operands[3].IsValid();

		FExprPtr Texture = ParenthesizeIfCompound(TakeValue(Node.Operands[0]));
		TUniquePtr<FCallExpr> Call = MakeCallExpr(MakeMemberExpr(MoveTemp(Texture), bHasLevel ? TEXT("SampleLevel") : TEXT("Sample")));
		if (bHasSampler)
		{
			AddPositionalArgument(*Call, TakeValue(Node.Operands[2]));
		}
		AddPositionalArgument(*Call, TakeValue(Node.Operands[1]));
		if (bHasLevel)
		{
			AddPositionalArgument(*Call, TakeValue(Node.Operands[3]));
		}
		return Call;
	}

	FExprPtr FGraphWriter::BuildEngineClassCall(const FIRNode& Node, const IR::FIRCoreOpInfo& Info)
	{
		// The reflected spelling of a core op, for the shapes its own syntax cannot say: `UE.If(A = ..., ...)`.
		TUniquePtr<FCallExpr> Call = MakeCallExpr(MakeNamespaceCallee(TEXT("UE"), Info.ExpressionClass));

		// One value on several pins was counted as one read (CountReads), so it is taken once and restated after that.
		TArray<TPair<FIRValue, const FExpr*>> Taken;
		const int32 PinCount = FMath::Min(Node.Operands.Num(), static_cast<int32>(UE_ARRAY_COUNT(Info.InputPins)));
		for (int32 OperandIndex = 0; OperandIndex < PinCount; ++OperandIndex)
		{
			const FIRValue& Operand = Node.Operands[OperandIndex];
			if (!Info.InputPins[OperandIndex] || !Operand.IsValid())
			{
				continue;
			}

			const FExpr* Earlier = nullptr;
			for (const TPair<FIRValue, const FExpr*>& Pair : Taken)
			{
				if (Pair.Key == Operand)
				{
					Earlier = Pair.Value;
					break;
				}
			}

			FExprPtr Value = Earlier ? CloneExpr(*Earlier) : TakeValue(Operand);
			if (!Earlier && Value)
			{
				Taken.Emplace(Operand, Value.Get());
			}
			AddNamedArgument(*Call, Info.InputPins[OperandIndex], MoveTemp(Value));
		}
		return Call;
	}

	FExprPtr FGraphWriter::BuildCoreOpExpression(const int32 NodeIndex, const FIRNode& Node)
	{
		const IR::FIRCoreOpInfo& Info = IR::GetCoreOpInfo(Node.Op);

		for (const FIRValue& Operand : Node.Operands)
		{
			if (!Operand.IsValid())
			{
				return FailValue(NodeIndex, LOCTEXT("CoreOpMissingOperand", "one of its operands is not connected"));
			}
		}
		if (Node.Operands.Num() < Info.MinArity || Node.Operands.Num() > Info.MaxArity)
		{
			return FailValue(NodeIndex, LOCTEXT("CoreOpArity", "it has the wrong number of operands"));
		}

		switch (Node.Op)
		{
		case EIROp::Swizzle:
		{
			const FIRProperty* Mask = Node.FindProperty(IR::Prop::Mask);
			if (!Mask || Mask->Value.S.IsEmpty())
			{
				return FailValue(NodeIndex, LOCTEXT("SwizzleWithoutMask", "the component mask is missing"));
			}
			return MakeMemberExpr(ParenthesizeIfCompound(TakeValue(Node.Operands[0])), Mask->Value.S);
		}

		case EIROp::Append:
			return BuildAppend(Node);

		case EIROp::Select:
		{
			FExprPtr Condition = BuildCondition(Node.Operands[0]);
			FExprPtr IfTrue = TakeValue(Node.Operands[1]);
			FExprPtr IfFalse = TakeValue(Node.Operands[2]);
			return MakeConditionalExpr(MoveTemp(Condition), MoveTemp(IfTrue), MoveTemp(IfFalse));
		}

		case EIROp::Compare:
		{
			// MakeConditional: `a > b ? x : y` is If(a, b, x, y, y), `a < b ? x : y` is If(a, b, y, y, x) and
			// `a == b ? x : y` is If(a, b, y, x, y). Each pair of equal branches is one of those; three different
			// branches, or a vector on A or B, is a node only the reflected call can say.
			const FIRValue& A = Node.Operands[0];
			const FIRValue& B = Node.Operands[1];
			const FIRValue& Greater = Node.Operands[2];
			const FIRValue& Equal = Node.Operands[3];
			const FIRValue& Less = Node.Operands[4];

			const bool bScalarSides = Graph.IsValidValue(A) && Graph.IsValidValue(B)
				&& Graph.TypeOf(A).GraphComponentCount() == 1
				&& Graph.TypeOf(B).GraphComponentCount() == 1;

			EBinaryOp Operator = EBinaryOp::Greater;
			const FIRValue* IfTrue = nullptr;
			const FIRValue* IfFalse = nullptr;
			if (bScalarSides && Equal == Less && Greater != Equal)
			{
				Operator = EBinaryOp::Greater;
				IfTrue = &Greater;
				IfFalse = &Equal;
			}
			else if (bScalarSides && Greater == Equal && Less != Greater)
			{
				Operator = EBinaryOp::Less;
				IfTrue = &Less;
				IfFalse = &Greater;
			}
			else if (bScalarSides && Greater == Less && Equal != Greater)
			{
				Operator = EBinaryOp::Equal;
				IfTrue = &Equal;
				IfFalse = &Greater;
			}

			if (!IfTrue || !IfFalse)
			{
				Owner.Info(TEXT("DSH9079"), FText::Format(
					LOCTEXT("CompareAsReflectedCall", "{0} is an If whose branches no comparison selects between; it is written as 'UE.If(...)'."),
					DescribeNode(NodeIndex)));
				return BuildEngineClassCall(Node, Info);
			}

			FExprPtr Left = TakeValue(A);
			FExprPtr Right = TakeValue(B);
			FExprPtr TrueExpr = TakeValue(*IfTrue);
			FExprPtr FalseExpr = TakeValue(*IfFalse);
			return MakeConditionalExpr(MakeBinaryExpr(Operator, MoveTemp(Left), MoveTemp(Right)), MoveTemp(TrueExpr), MoveTemp(FalseExpr));
		}

		case EIROp::StaticSwitch:
		{
			// `Flag ? a : b` is a StaticSwitch only where the builder can see the condition is static: a `/// @static`
			// uniform, or a `/// @static` parameter (a StaticBool pin). Anything else wired to Value is said with the
			// reflected call.
			const FIRNode* ConditionNode = Graph.Nodes.IsValidIndex(Node.Operands[0].Node) ? &Graph.Nodes[Node.Operands[0].Node] : nullptr;
			const FIRProperty* Static = (ConditionNode && ConditionNode->Op == EIROp::Parameter) ? ConditionNode->FindProperty(IR::Prop::IsStatic) : nullptr;
			const FIRProperty* PinType = (ConditionNode && ConditionNode->Op == EIROp::FunctionInput) ? ConditionNode->FindProperty(IR::Prop::InputType) : nullptr;
			const bool bStaticPin = PinType != nullptr && PinType->Value.S.Equals(TEXT("StaticBool"), ESearchCase::CaseSensitive);
			if (!bStaticPin && (!Static || !Static->Value.B))
			{
				return BuildEngineClassCall(Node, Info);
			}

			FExprPtr Condition = TakeValue(Node.Operands[0]);
			FExprPtr IfTrue = TakeValue(Node.Operands[1]);
			FExprPtr IfFalse = TakeValue(Node.Operands[2]);
			return MakeConditionalExpr(MoveTemp(Condition), MoveTemp(IfTrue), MoveTemp(IfFalse));
		}

		case EIROp::Convert:
			// No node in the graph, and the conversion happens again wherever the type asks for it.
			return TakeValue(Node.Operands[0]);

		case EIROp::Broadcast:
		{
			TArray<FExprPtr> Parts;
			Parts.Add(TakeValue(Node.Operands[0]));
			const FIRType Type = Node.Outputs.IsValidIndex(0) ? Node.Outputs[0] : FIRType::Float(1);
			return MakeConstructorExpr(MakeValueTypeRef(Type, /* bKeepBool */ false), MoveTemp(Parts));
		}

		default:
			break;
		}

		EBinaryOp BinaryOperator = EBinaryOp::Add;
		if (Node.Operands.Num() == 2 && TryFindBinaryOperator(Node.Op, BinaryOperator))
		{
			FExprPtr Left = TakeValue(Node.Operands[0]);
			FExprPtr Right = TakeValue(Node.Operands[1]);
			return MakeBinaryExpr(BinaryOperator, MoveTemp(Left), MoveTemp(Right));
		}

		EUnaryOp UnaryOperator = EUnaryOp::Negate;
		if (Node.Operands.Num() == 1 && TryFindUnaryOperator(Node.Op, UnaryOperator))
		{
			return MakeUnaryExpr(UnaryOperator, TakeValue(Node.Operands[0]));
		}

		if (Info.HlslName)
		{
			TUniquePtr<FCallExpr> Call = MakeCallExpr(MakeIdentifierExpr(Info.HlslName));
			for (const FIRValue& Operand : Node.Operands)
			{
				AddPositionalArgument(*Call, TakeValue(Operand));
			}
			return Call;
		}

		return FailValue(NodeIndex, LOCTEXT("CoreOpNoSpelling", "the language has no spelling for this operation"));
	}

	FExprPtr FGraphWriter::BuildUserCall(const int32 NodeIndex, const FIRNode& Node, const FCallable& Callable, const TMap<FString, FString>* OutLocals)
	{
		TUniquePtr<FCallExpr> Call = MakeCallExpr(MakeIdentifierExpr(Callable.Identifier));

		// Positional until an optional input is left out; from there on by name (no positional after a named one).
		bool bByName = false;
		for (const FCallableParam& Param : Callable.Params)
		{
			FExprPtr Argument;
			if (Param.Direction == EParamDirection::In)
			{
				const FIRInput* Input = Node.FindInput(Param.PinName);
				if (Input && Input->Value.IsValid())
				{
					Argument = TakeValue(Input->Value);
				}
				else if (Param.bOptional || Param.bHasDefault)
				{
					bByName = true;
					continue;
				}
				else
				{
					Argument = TakeZero(Param.Type);
					Owner.Warning(TEXT("DSH9084"), FText::Format(
						Argument
							? LOCTEXT("CallInputUnconnectedZero", "{0} leaves the required input '{1}' of '{2}' unconnected; '0.0' is passed for it.")
							: LOCTEXT("CallInputUnconnectedSkipped", "{0} leaves the required input '{1}' of '{2}' unconnected and nothing can stand in for it; the call is written without it and will not compile as it is."),
						DescribeNode(NodeIndex),
						FText::FromString(Param.PinName),
						FText::FromString(Callable.Identifier)));
					if (!Argument)
					{
						bByName = true;
						continue;
					}
				}
			}
			else
			{
				const FString* Local = OutLocals ? OutLocals->Find(Param.Identifier) : nullptr;
				if (!Local)
				{
					return FailValue(NodeIndex, LOCTEXT("CallOutWithoutLocal", "the function it calls has 'out' parameters and the call was taken for a plain value"));
				}
				Argument = MakeIdentifierExpr(*Local);
			}

			if (bByName)
			{
				AddNamedArgument(*Call, Param.Identifier, MoveTemp(Argument));
			}
			else
			{
				AddPositionalArgument(*Call, MoveTemp(Argument));
			}
		}

		// An input the call wires that the callee does not declare: the interface and the call disagree.
		for (const FIRInput& Input : Node.Inputs)
		{
			if (Input.Value.IsValid() && !Callable.FindParamByPin(Input.Pin, /* bOutput */ false))
			{
				Owner.Warning(TEXT("DSH9084"), FText::Format(
					LOCTEXT("CallInputUndeclared", "{0} connects an input '{1}' that '{2}' does not declare; the connection is dropped."),
					DescribeNode(NodeIndex),
					FText::FromString(Input.Pin),
					FText::FromString(Callable.Identifier)));
			}
		}

		return Call;
	}

	FExprPtr FGraphWriter::BuildNodeExpression(const int32 NodeIndex)
	{
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		switch (Node.Op)
		{
		case EIROp::Constant:
			return BuildConstant(Node);

		case EIROp::Reflected:
		{
			if (FExprPtr Readable = BuildReadableReflected(NodeIndex, Node))
			{
				return Readable;
			}
			return BuildReflectedCall(NodeIndex, Node);
		}

		case EIROp::FunctionCall:
		case EIROp::Custom:
		{
			const FCallable* Callable = Owner.FindCallable(Node);
			if (!Callable)
			{
				return FailValue(NodeIndex, LOCTEXT("CallWithoutCallee", "the function it calls could not be declared"));
			}
			if (!Callable->bHasReturn)
			{
				return FailValue(NodeIndex, LOCTEXT("CallOfVoidAsValue", "the function it calls returns nothing"));
			}
			return BuildUserCall(NodeIndex, Node, *Callable, nullptr);
		}

		case EIROp::TextureSample:
			return BuildTextureSample(NodeIndex, Node);

		default:
			break;
		}
		return BuildCoreOpExpression(NodeIndex, Node);
	}

	void FGraphWriter::BuildNode(const int32 NodeIndex)
	{
		FNodePlan& Plan = Plans[NodeIndex];
		const FIRNode& Node = Graph.Nodes[NodeIndex];

		switch (Plan.Shape)
		{
		case ENodeShape::Identifier:
		case ENodeShape::Root:
			return;

		case ENodeShape::MaterialGroup:
			if (!Plan.bWrittenThroughRoot)
			{
				DeclareMaterialGroup(NodeIndex);
			}
			return;

		case ENodeShape::CallStatement:
			DeclareCallStatement(NodeIndex);
			return;

		case ENodeShape::Single:
		case ENodeShape::Sample:
		{
			CurrentDependencies.Reset();
			FExprPtr Expr = BuildNodeExpression(NodeIndex);
			if (!Expr)
			{
				Expr = MakeNumberExpr(0.0);
			}

			FValuePlan& Slot = Plan.Slots[0];
			const FIRType Type = Node.Outputs.IsValidIndex(0) ? Node.Outputs[0] : FIRType::Error();
			if (!Slot.bNamed && Slot.DirectReads > 0 && IsDeclarableValueType(Type) && !IsLeafLike(NodeIndex) && ShouldNameForLength(*Expr))
			{
				Slot.bNamed = true;
			}

			if (Slot.bNamed)
			{
				DeclareSlot(NodeIndex, 0, MoveTemp(Expr), CurrentDependencies);
			}
			else
			{
				Slot.Pending = MoveTemp(Expr);
				Slot.PendingDependencies = CurrentDependencies;
			}
			return;
		}

		case ENodeShape::MultiOutput:
		case ENodeShape::Attributes:
		{
			CurrentDependencies.Reset();
			if (Plan.Shape == ENodeShape::MultiOutput)
			{
				Plan.Base = BuildReflectedCall(NodeIndex, Node);
			}
			else
			{
				const FIRInput* Material = Node.FindInput(TEXT("MaterialAttributes"));
				Plan.Base = (Material && Material->Value.IsValid())
					? TakeValue(Material->Value)
					: FailValue(NodeIndex, LOCTEXT("AttributesWithoutMaterial", "it reads the attributes of no material"));
			}
			Plan.BaseDependencies = CurrentDependencies;

			for (int32 SlotIndex = 0; SlotIndex < Plan.Slots.Num(); ++SlotIndex)
			{
				if (!Plan.Slots[SlotIndex].bNamed || !Plan.Base)
				{
					continue;
				}

				FExprPtr Initializer;
				if (Plan.Shape == ENodeShape::Attributes)
				{
					const FString Attribute = Node.OutputNames.IsValidIndex(SlotIndex) ? Node.OutputNames[SlotIndex] : FString();
					if (!IsIdentifierText(Attribute))
					{
						Plan.Slots[SlotIndex].bNamed = false;
						continue;
					}
					Initializer = MakeMemberExpr(ParenthesizeIfCompound(CloneExpr(*Plan.Base)), Attribute);
				}
				else
				{
					Initializer = SelectOutput(CloneExpr(*Plan.Base), Node, SlotIndex);
				}
				DeclareSlot(NodeIndex, SlotIndex, MoveTemp(Initializer), Plan.BaseDependencies);
			}
			return;
		}

		default:
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE
