// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// RaiseDreamShaderIR: the emitter's hand-lowered shapes, read back as the core ops they came from.
//
// The emitter has no engine node for a handful of core ops and builds each out of others, always in the same
// shape (Compiler: Emitter/DreamShaderIREmitterNodes.cpp, EmitHandLoweredCoreOp, EmitSelect). A graph imported
// from an asset shows those shapes as what they are made of -- a Multiply by a Constant -1, an If over two
// Constants -- and printing them that way would be right but unreadable. Every rule below recognises one shape
// and rewrites its LAST node into the core op; the nodes it swallowed are left without readers and the prune
// pass takes them away.
//
// A rule fires only when the rebuild is graph-exact: every swallowed node is read by the shape alone (its
// fan-out is what the shape accounts for), and every constant is the exact value the emitter writes. A shape
// that shares a node with anything else stays as it is and prints as the nodes it is made of, which the
// compiler turns back into the same nodes.
//
// Imported shapes, for reference (research-decompiler.md section 3.3): a class with every operand pin connected
// is the core op; the same class with a `Const*` twin in use is a Reflected node with the connected pins as
// Inputs and the twin as a property.
//
// Not raised: Multiply and Max over 0/1 values (`&&`, `||`). They cannot be told from arithmetic, and `a * b`
// compiles to the same node.
//
// The Substrate sugar is read back here too, after everything else and once: Substrate.Add, Weight and
// HorizontalMixing with nothing set on them become the core ops the writer spells `+`, `*` and `lerp` (sugar S1), and
// a conversion node that feeds one BSDF and nothing else moves its inputs onto the BSDF under the names the sugar
// gives them (S3), where the writer's reflected call writes an input that is no pin of the class as the named
// argument it is. After, so that no arithmetic rule takes `A * -1` for a negation; by engine class, so that the rule
// is the same whatever the catalog calls the class.

#include "Decompile/IRToAst.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRPasses.h"
#include "IR/IRTypes.h"
#include "Lang/LangDiagnostic.h"

namespace UE::DreamShader::Lang
{
	namespace DecompileRaise
	{
		using IR::EIROp;
		using IR::FIRGraph;
		using IR::FIRInput;
		using IR::FIRNode;
		using IR::FIRProperty;
		using IR::FIRType;
		using IR::FIRValue;

		/** One product's graph with the reader counts the rules ask about. */
		class FRaiseGraph
		{
		public:
			explicit FRaiseGraph(FIRGraph& InGraph)
				: Graph(InGraph)
			{
				Recount();
			}

			/** Reading edges per node, every output together. Recomputed after each rewrite: the graphs are small. */
			void Recount()
			{
				FanOut.Init(0, Graph.Nodes.Num());
				TArray<FIRValue> Values;
				for (const FIRNode& Node : Graph.Nodes)
				{
					Values.Reset();
					FIRGraph::CollectInputValues(Node, Values);
					for (const FIRValue& Value : Values)
					{
						if (Graph.Nodes.IsValidIndex(Value.Node))
						{
							++FanOut[Value.Node];
						}
					}
				}
			}

			const FIRNode* NodeOf(const FIRValue Value) const
			{
				return Graph.Nodes.IsValidIndex(Value.Node) ? &Graph.Nodes[Value.Node] : nullptr;
			}

			int32 FanOutOf(const FIRValue Value) const
			{
				return FanOut.IsValidIndex(Value.Node) ? FanOut[Value.Node] : 0;
			}

			FIRType TypeOf(const FIRValue Value) const
			{
				return Graph.IsValidValue(Value) ? Graph.TypeOf(Value) : FIRType::Error();
			}

			/** A core op node of Op with exactly Arity operands, read through its output 0. */
			const FIRNode* CoreOp(const FIRValue Value, const EIROp Op, const int32 Arity) const
			{
				const FIRNode* Node = NodeOf(Value);
				return (Node && Value.Output == 0 && Node->Op == Op && Node->Operands.Num() == Arity && Node->Inputs.IsEmpty()) ? Node : nullptr;
			}

			/** A scalar Constant node holding exactly Expected. */
			bool IsScalarConstant(const FIRValue Value, const double Expected) const
			{
				const FIRNode* Node = NodeOf(Value);
				if (!Node || Value.Output != 0 || Node->Op != EIROp::Constant)
				{
					return false;
				}
				const FIRProperty* Property = Node->FindProperty(IR::Prop::Value);
				return Property
					&& Property->Value.Kind == IR::EIRPropertyKind::Float4
					&& Property->Value.N == 1
					&& Property->Value.V[0] == Expected;
			}

			/**
			 * A Reflected node of the class ShortName whose one literal is the twin TwinName == Expected, and whose
			 * connected pins are exactly PinNames; OutValues receives their values in that order.
			 */
			bool MatchTwinNode(
				const FIRValue Value,
				const TCHAR* ShortName,
				const TCHAR* TwinName,
				const double Expected,
				const TArray<const TCHAR*>& PinNames,
				TArray<FIRValue>& OutValues) const
			{
				OutValues.Reset();
				const FIRNode* Node = NodeOf(Value);
				if (!Node
					|| Value.Output != 0
					|| Node->Op != EIROp::Reflected
					|| !Node->ClassName.Equals(ShortName, ESearchCase::CaseSensitive)
					|| Node->Inputs.Num() != PinNames.Num()
					|| Node->Properties.Num() != 1)
				{
					return false;
				}

				const FIRProperty& Twin = Node->Properties[0];
				if (!Twin.Name.Equals(TwinName, ESearchCase::CaseSensitive) || !IsNumber(Twin.Value, Expected))
				{
					return false;
				}

				for (const TCHAR* PinName : PinNames)
				{
					const FIRInput* Input = Node->FindInput(FString(PinName));
					if (!Input || !Input->Value.IsValid())
					{
						return false;
					}
					OutValues.Add(Input->Value);
				}
				return true;
			}

			/** The catalog entry of a Reflected node; null without a catalog, or for any other node. */
			const IR::FCatalogExpression* ClassOf(const FIRNode& Node) const
			{
				return (Catalog && Node.Op == EIROp::Reflected && Catalog->Expressions.IsValidIndex(Node.CatalogIndex))
					? &Catalog->Expressions[Node.CatalogIndex]
					: nullptr;
			}

			/** The node is of this engine class (`MaterialExpressionSubstrateAdd`): the one name every catalog agrees on. */
			bool IsEngineClass(const FIRNode& Node, const TCHAR* EngineClass) const
			{
				const IR::FCatalogExpression* Class = ClassOf(Node);
				return Class && Class->ClassName.Equals(EngineClass, ESearchCase::CaseSensitive);
			}

			/** Turns the node into a core op in place; its name, source and region stay. */
			void RewriteAsCoreOp(const int32 NodeIndex, const EIROp Op, TArray<FIRValue>&& Operands, const FIRType& Type)
			{
				FIRNode& Node = Graph.Nodes[NodeIndex];
				Node.Op = Op;
				Node.Operands = MoveTemp(Operands);
				Node.Inputs.Reset();
				Node.Properties.Reset();
				Node.Outputs.Reset();
				Node.Outputs.Add(Type);
				Node.OutputNames.Reset();
				Node.ClassName.Reset();
				Node.CatalogIndex = INDEX_NONE;
				Node.DedupeKey.Reset();
				Recount();
			}

			FIRGraph& Graph;
			/** Set by whoever has one; the Substrate rules do nothing without it. */
			const IR::FBuiltinCatalog* Catalog = nullptr;

		private:
			static bool IsNumber(const IR::FIRPropertyValue& Value, const double Expected)
			{
				switch (Value.Kind)
				{
				case IR::EIRPropertyKind::Float:
					return Value.F == Expected;
				case IR::EIRPropertyKind::Int:
					return static_cast<double>(Value.I) == Expected;
				case IR::EIRPropertyKind::Float4:
					return Value.N == 1 && Value.V[0] == Expected;
				case IR::EIRPropertyKind::Bool:
				case IR::EIRPropertyKind::String:
				case IR::EIRPropertyKind::Name:
				case IR::EIRPropertyKind::Enum:
				case IR::EIRPropertyKind::Object:
				case IR::EIRPropertyKind::StringList:
					return false;
				}
				return false;
			}

			TArray<int32> FanOut;
		};

		// ------------------------------------------------------------------------------------ the rules

		/** refract(I, N, eta): the fourteen nodes of EmitHandLoweredCoreOp, ending in an If with ConstB = 0. */
		static bool TryRaiseRefract(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			const FIRValue Self{ NodeIndex, 0 };

			TArray<FIRValue> Selected;
			if (!Raise.MatchTwinNode(Self, TEXT("If"), TEXT("ConstB"), 0.0,
				{ TEXT("A"), TEXT("AGreaterThanB"), TEXT("AEqualsB"), TEXT("ALessThanB") }, Selected))
			{
				return false;
			}
			const FIRValue K = Selected[0];
			const FIRValue Refracted = Selected[1];
			const FIRValue ZeroVector = Selected[3];
			if (Refracted != Selected[2] || Raise.FanOutOf(Refracted) != 2 || Raise.FanOutOf(K) != 2)
			{
				return false;
			}

			const FIRNode* RefractedNode = Raise.CoreOp(Refracted, EIROp::Subtract, 2);
			if (!RefractedNode)
			{
				return false;
			}
			const FIRValue ScaledIncident = RefractedNode->Operands[0];
			const FIRValue ScaledNormal = RefractedNode->Operands[1];

			const FIRNode* ScaledIncidentNode = Raise.CoreOp(ScaledIncident, EIROp::Multiply, 2);
			const FIRNode* ScaledNormalNode = Raise.CoreOp(ScaledNormal, EIROp::Multiply, 2);
			if (!ScaledIncidentNode || !ScaledNormalNode
				|| Raise.FanOutOf(ScaledIncident) != 1 || Raise.FanOutOf(ScaledNormal) != 1)
			{
				return false;
			}
			const FIRValue Incident = ScaledIncidentNode->Operands[0];
			const FIRValue Eta = ScaledIncidentNode->Operands[1];
			const FIRValue NormalScale = ScaledNormalNode->Operands[0];
			const FIRValue Normal = ScaledNormalNode->Operands[1];

			const FIRNode* NormalScaleNode = Raise.CoreOp(NormalScale, EIROp::Add, 2);
			if (!NormalScaleNode || Raise.FanOutOf(NormalScale) != 1)
			{
				return false;
			}
			const FIRValue EtaNdotI = NormalScaleNode->Operands[0];
			const FIRValue SqrtK = NormalScaleNode->Operands[1];

			const FIRNode* EtaNdotINode = Raise.CoreOp(EtaNdotI, EIROp::Multiply, 2);
			const FIRNode* SqrtKNode = Raise.CoreOp(SqrtK, EIROp::Sqrt, 1);
			if (!EtaNdotINode || !SqrtKNode
				|| Raise.FanOutOf(EtaNdotI) != 1 || Raise.FanOutOf(SqrtK) != 1
				|| EtaNdotINode->Operands[0] != Eta
				|| SqrtKNode->Operands[0] != K)
			{
				return false;
			}
			const FIRValue NormalDotIncident = EtaNdotINode->Operands[1];

			const FIRNode* NormalDotIncidentNode = Raise.CoreOp(NormalDotIncident, EIROp::Dot, 2);
			if (!NormalDotIncidentNode
				|| Raise.FanOutOf(NormalDotIncident) != 3
				|| NormalDotIncidentNode->Operands[0] != Normal
				|| NormalDotIncidentNode->Operands[1] != Incident)
			{
				return false;
			}

			// k = 1 - eta * eta * (1 - dot(N, I) ^ 2)
			TArray<FIRValue> KInputs;
			if (!Raise.MatchTwinNode(K, TEXT("Subtract"), TEXT("ConstA"), 1.0, { TEXT("B") }, KInputs))
			{
				return false;
			}
			const FIRValue EtaSinSquared = KInputs[0];
			const FIRNode* EtaSinSquaredNode = Raise.CoreOp(EtaSinSquared, EIROp::Multiply, 2);
			if (!EtaSinSquaredNode || Raise.FanOutOf(EtaSinSquared) != 1)
			{
				return false;
			}
			const FIRValue EtaSquared = EtaSinSquaredNode->Operands[0];
			const FIRValue SinSquared = EtaSinSquaredNode->Operands[1];

			const FIRNode* EtaSquaredNode = Raise.CoreOp(EtaSquared, EIROp::Multiply, 2);
			if (!EtaSquaredNode
				|| Raise.FanOutOf(EtaSquared) != 1
				|| EtaSquaredNode->Operands[0] != Eta
				|| EtaSquaredNode->Operands[1] != Eta)
			{
				return false;
			}

			TArray<FIRValue> SinSquaredInputs;
			if (!Raise.MatchTwinNode(SinSquared, TEXT("Subtract"), TEXT("ConstA"), 1.0, { TEXT("B") }, SinSquaredInputs)
				|| Raise.FanOutOf(SinSquared) != 1)
			{
				return false;
			}
			const FIRValue NdotISquared = SinSquaredInputs[0];
			const FIRNode* NdotISquaredNode = Raise.CoreOp(NdotISquared, EIROp::Multiply, 2);
			if (!NdotISquaredNode
				|| Raise.FanOutOf(NdotISquared) != 1
				|| NdotISquaredNode->Operands[0] != NormalDotIncident
				|| NdotISquaredNode->Operands[1] != NormalDotIncident)
			{
				return false;
			}

			// The zero branch is I * 0, so its width always equals I's.
			TArray<FIRValue> ZeroInputs;
			if (!Raise.MatchTwinNode(ZeroVector, TEXT("Multiply"), TEXT("ConstB"), 0.0, { TEXT("A") }, ZeroInputs)
				|| Raise.FanOutOf(ZeroVector) != 1
				|| ZeroInputs[0] != Incident)
			{
				return false;
			}

			// refract is typed float3 over float3 operands; any other width would be coerced on the way back.
			if (Raise.TypeOf(Incident).GraphComponentCount() != 3 || Raise.TypeOf(Normal).GraphComponentCount() != 3)
			{
				return false;
			}

			Raise.RewriteAsCoreOp(NodeIndex, EIROp::Refract, { Incident, Normal, Eta }, FIRType::Float(3));
			return true;
		}

		/** reflect(I, N) = I - 2 * dot(I, N) * N, the 2 on the first Multiply's ConstB. */
		static bool TryRaiseReflect(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			const FIRNode* Self = Raise.CoreOp(FIRValue{ NodeIndex, 0 }, EIROp::Subtract, 2);
			if (!Self)
			{
				return false;
			}
			const FIRValue Incident = Self->Operands[0];
			const FIRValue ScaledNormal = Self->Operands[1];

			const FIRNode* ScaledNormalNode = Raise.CoreOp(ScaledNormal, EIROp::Multiply, 2);
			if (!ScaledNormalNode || Raise.FanOutOf(ScaledNormal) != 1)
			{
				return false;
			}
			const FIRValue ScaledDot = ScaledNormalNode->Operands[0];
			const FIRValue Normal = ScaledNormalNode->Operands[1];

			TArray<FIRValue> ScaledDotInputs;
			if (!Raise.MatchTwinNode(ScaledDot, TEXT("Multiply"), TEXT("ConstB"), 2.0, { TEXT("A") }, ScaledDotInputs)
				|| Raise.FanOutOf(ScaledDot) != 1)
			{
				return false;
			}

			const FIRValue IncidentDotNormal = ScaledDotInputs[0];
			const FIRNode* DotNode = Raise.CoreOp(IncidentDotNormal, EIROp::Dot, 2);
			if (!DotNode
				|| Raise.FanOutOf(IncidentDotNormal) != 1
				|| DotNode->Operands[0] != Incident
				|| DotNode->Operands[1] != Normal)
			{
				return false;
			}

			if (Raise.TypeOf(Incident).GraphComponentCount() != 3 || Raise.TypeOf(Normal).GraphComponentCount() != 3)
			{
				return false;
			}

			Raise.RewriteAsCoreOp(NodeIndex, EIROp::Reflect, { Incident, Normal }, FIRType::Float(3));
			return true;
		}

		/** fwidth(x) = abs(ddx(x)) + abs(ddy(x)). */
		static bool TryRaiseFwidth(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			const FIRNode* Self = Raise.CoreOp(FIRValue{ NodeIndex, 0 }, EIROp::Add, 2);
			if (!Self)
			{
				return false;
			}
			const FIRValue AbsX = Self->Operands[0];
			const FIRValue AbsY = Self->Operands[1];
			const FIRNode* AbsXNode = Raise.CoreOp(AbsX, EIROp::Abs, 1);
			const FIRNode* AbsYNode = Raise.CoreOp(AbsY, EIROp::Abs, 1);
			if (!AbsXNode || !AbsYNode || AbsX == AbsY || Raise.FanOutOf(AbsX) != 1 || Raise.FanOutOf(AbsY) != 1)
			{
				return false;
			}

			const FIRValue DdxValue = AbsXNode->Operands[0];
			const FIRValue DdyValue = AbsYNode->Operands[0];
			const FIRNode* DdxNode = Raise.CoreOp(DdxValue, EIROp::DDX, 1);
			const FIRNode* DdyNode = Raise.CoreOp(DdyValue, EIROp::DDY, 1);
			if (!DdxNode || !DdyNode
				|| Raise.FanOutOf(DdxValue) != 1 || Raise.FanOutOf(DdyValue) != 1
				|| DdxNode->Operands[0] != DdyNode->Operands[0])
			{
				return false;
			}

			const FIRValue Operand = DdxNode->Operands[0];
			const FIRType Type = Raise.TypeOf(Operand);
			if (Type.GraphComponentCount() <= 0)
			{
				return false;
			}
			Raise.RewriteAsCoreOp(NodeIndex, EIROp::Fwidth, { Operand }, FIRType::Float(Type.GraphComponentCount()));
			return true;
		}

		/** rsqrt(x) = 1 / sqrt(x) and rcp(x) = 1 / x, the 1 on Divide's ConstA. */
		static bool TryRaiseReciprocal(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			TArray<FIRValue> DivideInputs;
			if (!Raise.MatchTwinNode(FIRValue{ NodeIndex, 0 }, TEXT("Divide"), TEXT("ConstA"), 1.0, { TEXT("B") }, DivideInputs))
			{
				return false;
			}

			const FIRValue Divisor = DivideInputs[0];
			const FIRType DivisorType = Raise.TypeOf(Divisor);
			if (DivisorType.GraphComponentCount() <= 0)
			{
				return false;
			}
			const FIRType ResultType = FIRType::Float(DivisorType.GraphComponentCount());

			if (const FIRNode* SqrtNode = Raise.CoreOp(Divisor, EIROp::Sqrt, 1))
			{
				if (Raise.FanOutOf(Divisor) == 1)
				{
					const FIRValue Operand = SqrtNode->Operands[0];
					Raise.RewriteAsCoreOp(NodeIndex, EIROp::Rsqrt, { Operand }, ResultType);
					return true;
				}
			}

			Raise.RewriteAsCoreOp(NodeIndex, EIROp::Rcp, { Divisor }, ResultType);
			return true;
		}

		/** `a < b` and its five siblings: one If whose three branches are the Constants 1 and 0. */
		static bool TryRaiseComparison(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			const FIRNode* Self = Raise.CoreOp(FIRValue{ NodeIndex, 0 }, EIROp::Compare, 5);
			if (!Self)
			{
				return false;
			}

			FIRValue One = FIRValue::None();
			FIRValue Zero = FIRValue::None();
			int32 OneCount = 0;
			int32 ZeroCount = 0;
			bool bBranchIsOne[3] = { false, false, false };
			for (int32 Branch = 0; Branch < 3; ++Branch)
			{
				const FIRValue Value = Self->Operands[2 + Branch];
				if (Raise.IsScalarConstant(Value, 1.0))
				{
					if (One.IsValid() && One != Value)
					{
						return false;
					}
					One = Value;
					++OneCount;
					bBranchIsOne[Branch] = true;
				}
				else if (Raise.IsScalarConstant(Value, 0.0))
				{
					if (Zero.IsValid() && Zero != Value)
					{
						return false;
					}
					Zero = Value;
					++ZeroCount;
				}
				else
				{
					return false;
				}
			}

			// The emitter makes one Constant of each and wires nothing else to them.
			if (OneCount == 0 || ZeroCount == 0 || Raise.FanOutOf(One) != OneCount || Raise.FanOutOf(Zero) != ZeroCount)
			{
				return false;
			}
			// B may be the very Constant 0 a branch uses only in a graph the emitter did not make.
			if (Self->Operands[0] == One || Self->Operands[0] == Zero || Self->Operands[1] == One || Self->Operands[1] == Zero)
			{
				return false;
			}

			const bool bGreater = bBranchIsOne[0];
			const bool bEqual = bBranchIsOne[1];
			const bool bLess = bBranchIsOne[2];
			EIROp Op = EIROp::Count;
			if (bGreater && !bEqual && !bLess)       { Op = EIROp::Greater; }
			else if (bGreater && bEqual && !bLess)   { Op = EIROp::GreaterEqual; }
			else if (!bGreater && !bEqual && bLess)  { Op = EIROp::Less; }
			else if (!bGreater && bEqual && bLess)   { Op = EIROp::LessEqual; }
			else if (!bGreater && bEqual && !bLess)  { Op = EIROp::Equal; }
			else if (bGreater && !bEqual && bLess)   { Op = EIROp::NotEqual; }
			if (Op == EIROp::Count)
			{
				return false;
			}

			const FIRValue Left = Self->Operands[0];
			const FIRValue Right = Self->Operands[1];
			Raise.RewriteAsCoreOp(NodeIndex, Op, { Left, Right }, FIRType::Bool(1));
			return true;
		}

		/** `c ? t : f`: an If against a Constant 0 with the true value on both sides of it. */
		static bool TryRaiseSelect(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			const FIRNode* Self = Raise.CoreOp(FIRValue{ NodeIndex, 0 }, EIROp::Compare, 5);
			if (!Self)
			{
				return false;
			}

			const FIRValue Condition = Self->Operands[0];
			const FIRValue Zero = Self->Operands[1];
			const FIRValue IfTrue = Self->Operands[2];
			const FIRValue IfFalse = Self->Operands[3];
			if (!Raise.IsScalarConstant(Zero, 0.0)
				|| Raise.FanOutOf(Zero) != 1
				|| IfTrue != Self->Operands[4]
				|| IfTrue == IfFalse
				|| Raise.TypeOf(Condition).GraphComponentCount() != 1)
			{
				return false;
			}

			const FIRType Type = Self->Outputs.IsValidIndex(0) ? Self->Outputs[0] : FIRType::Float(1);
			Raise.RewriteAsCoreOp(NodeIndex, EIROp::Select, { Condition, IfTrue, IfFalse }, Type);
			return true;
		}

		/** `-x`: x * -1 with the -1 as a Constant node of its own. */
		static bool TryRaiseNegate(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			const FIRNode* Self = Raise.CoreOp(FIRValue{ NodeIndex, 0 }, EIROp::Multiply, 2);
			if (!Self)
			{
				return false;
			}
			const FIRValue Operand = Self->Operands[0];
			const FIRValue MinusOne = Self->Operands[1];
			if (!Raise.IsScalarConstant(MinusOne, -1.0) || Raise.FanOutOf(MinusOne) != 1 || Operand == MinusOne)
			{
				return false;
			}

			const FIRType Type = Self->Outputs.IsValidIndex(0) ? Self->Outputs[0] : Raise.TypeOf(Operand);
			Raise.RewriteAsCoreOp(NodeIndex, EIROp::Negate, { Operand }, Type);
			return true;
		}

		/** `!b`: 1 - b over a value that is a bool by now, the 1 on Subtract's ConstA. */
		static bool TryRaiseLogicalNot(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			TArray<FIRValue> SubtractInputs;
			if (!Raise.MatchTwinNode(FIRValue{ NodeIndex, 0 }, TEXT("Subtract"), TEXT("ConstA"), 1.0, { TEXT("B") }, SubtractInputs))
			{
				return false;
			}
			const FIRType OperandType = Raise.TypeOf(SubtractInputs[0]);
			if (!OperandType.IsBool() || OperandType.GraphComponentCount() != 1)
			{
				return false;
			}

			const FIRValue Operand = SubtractInputs[0];
			Raise.RewriteAsCoreOp(NodeIndex, EIROp::LogicalNot, { Operand }, FIRType::Bool(1));
			return true;
		}

		// ------------------------------------------------------------------------ the Substrate sugar

		static FIRValue ValueOfPin(const FIRNode& Node, const TCHAR* Pin)
		{
			const FIRInput* Input = Node.FindInput(FString(Pin));
			return Input ? Input->Value : FIRValue::None();
		}

		/** Every connected input of the node is one of Allowed. */
		static bool HasOnlyInputs(const FIRNode& Node, const TArray<const TCHAR*>& Allowed)
		{
			for (const FIRInput& Input : Node.Inputs)
			{
				if (!Input.Value.IsValid())
				{
					continue;
				}
				bool bAllowed = false;
				for (const TCHAR* Name : Allowed)
				{
					bAllowed = bAllowed || Input.Pin.Equals(Name, ESearchCase::CaseSensitive);
				}
				if (!bAllowed)
				{
					return false;
				}
			}
			return true;
		}

		static void RemoveInputs(FIRNode& Node, const TArray<const TCHAR*>& Pins)
		{
			Node.Inputs.RemoveAll([&Pins](const FIRInput& Input)
			{
				for (const TCHAR* Pin : Pins)
				{
					if (Input.Pin.Equals(Pin, ESearchCase::CaseSensitive))
					{
						return true;
					}
				}
				return false;
			});
		}

		/**
		 * Sugar S3: `Substrate.Slab(BaseColor = c, Metallic = m)` is a MetalnessToDiffuseAlbedoF0 wired into DiffuseAlbedo
		 * and F0, and so on for Haziness and Transmittance (IRBuilderSubstrate.cpp). Read back when the conversion node
		 * is exactly that: both of its outputs on the two pins, nothing else reading it, nothing set on it, and -- what
		 * the binder asks before it takes an argument for a virtual one -- no pin of that name on the BSDF itself.
		 * `IOR` is not read back: a constant F0 does not say it was an index of refraction.
		 */
		static bool TryRaiseSubstrateArguments(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			FIRNode& Self = Raise.Graph.Nodes[NodeIndex];
			const IR::FCatalogExpression* Class = Raise.ClassOf(Self);
			if (!Class || !Class->Namespace.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive))
			{
				return false;
			}
			const auto HasPin = [Class](const TCHAR* Pin)
			{
				return Class->FindInput(FString(Pin)) != INDEX_NONE;
			};

			bool bChanged = false;

			// ----- DiffuseAlbedo, F0 <- BaseColor / Metallic / Specular
			{
				const FIRValue Diffuse = ValueOfPin(Self, TEXT("DiffuseAlbedo"));
				const FIRValue F0 = ValueOfPin(Self, TEXT("F0"));
				const FIRNode* Conversion = Raise.NodeOf(Diffuse);
				if (Conversion && F0.IsValid() && Diffuse.Node == F0.Node && Diffuse.Output == 0 && F0.Output == 1
					&& Raise.IsEngineClass(*Conversion, TEXT("MaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0"))
					&& Conversion->Properties.IsEmpty()
					&& Raise.FanOutOf(Diffuse) == 2
					&& HasOnlyInputs(*Conversion, { TEXT("BaseColor"), TEXT("Metallic"), TEXT("Specular") })
					&& !HasPin(TEXT("BaseColor")) && !HasPin(TEXT("Metallic")) && !HasPin(TEXT("Specular")))
				{
					const FIRValue BaseColor = ValueOfPin(*Conversion, TEXT("BaseColor"));
					const FIRValue Metallic = ValueOfPin(*Conversion, TEXT("Metallic"));
					const FIRValue Specular = ValueOfPin(*Conversion, TEXT("Specular"));
					if (BaseColor.IsValid() || Metallic.IsValid() || Specular.IsValid())
					{
						RemoveInputs(Self, { TEXT("DiffuseAlbedo"), TEXT("F0") });
						if (BaseColor.IsValid()) { Self.Inputs.Add({ FString(TEXT("BaseColor")), BaseColor }); }
						if (Metallic.IsValid()) { Self.Inputs.Add({ FString(TEXT("Metallic")), Metallic }); }
						if (Specular.IsValid()) { Self.Inputs.Add({ FString(TEXT("Specular")), Specular }); }
						bChanged = true;
					}
				}
			}

			// ----- SecondRoughness, SecondRoughnessWeight <- Haziness, against the BSDF's own Roughness
			{
				const FIRValue Second = ValueOfPin(Self, TEXT("SecondRoughness"));
				const FIRValue Weight = ValueOfPin(Self, TEXT("SecondRoughnessWeight"));
				const FIRValue Roughness = ValueOfPin(Self, TEXT("Roughness"));
				const FIRNode* Conversion = Raise.NodeOf(Second);
				if (Conversion && Weight.IsValid() && Second.Node == Weight.Node && Second.Output == 0 && Weight.Output == 1
					&& Raise.IsEngineClass(*Conversion, TEXT("MaterialExpressionSubstrateHazinessToSecondaryRoughness"))
					&& Conversion->Properties.IsEmpty()
					&& Raise.FanOutOf(Second) == 2
					&& HasOnlyInputs(*Conversion, { TEXT("BaseRoughness"), TEXT("Haziness") })
					&& !HasPin(TEXT("Haziness")))
				{
					const FIRValue Haziness = ValueOfPin(*Conversion, TEXT("Haziness"));
					const FIRValue BaseRoughness = ValueOfPin(*Conversion, TEXT("BaseRoughness"));
					// The sugar reads the call's own Roughness, and asks for one.
					if (Haziness.IsValid() && Roughness.IsValid() && BaseRoughness == Roughness)
					{
						RemoveInputs(Self, { TEXT("SecondRoughness"), TEXT("SecondRoughnessWeight") });
						Self.Inputs.Add({ FString(TEXT("Haziness")), Haziness });
						bChanged = true;
					}
				}
			}

			// ----- SSSMFP <- Transmittance [, Thickness]
			{
				const FIRValue Mfp = ValueOfPin(Self, TEXT("SSSMFP"));
				const FIRNode* Conversion = Raise.NodeOf(Mfp);
				if (Conversion && Mfp.Output == 0
					&& Raise.IsEngineClass(*Conversion, TEXT("MaterialExpressionSubstrateTransmittanceToMFP"))
					&& Conversion->Properties.IsEmpty()
					&& Raise.FanOutOf(Mfp) == 1
					&& HasOnlyInputs(*Conversion, { TEXT("TransmittanceColor"), TEXT("Thickness") })
					&& !HasPin(TEXT("Transmittance")) && !HasPin(TEXT("Thickness")))
				{
					const FIRValue Transmittance = ValueOfPin(*Conversion, TEXT("TransmittanceColor"));
					const FIRValue Thickness = ValueOfPin(*Conversion, TEXT("Thickness"));
					if (Transmittance.IsValid())
					{
						RemoveInputs(Self, { TEXT("SSSMFP") });
						Self.Inputs.Add({ FString(TEXT("Transmittance")), Transmittance });
						if (Thickness.IsValid()) { Self.Inputs.Add({ FString(TEXT("Thickness")), Thickness }); }
						bChanged = true;
					}
				}
			}

			if (bChanged)
			{
				Self.DedupeKey.Reset();
				Raise.Recount();
			}
			return bChanged;
		}

		/**
		 * Sugar S1: Substrate.Add, Weight and HorizontalMixing with every pin wired and nothing set -- parameter blending
		 * is a property, and a node that has it keeps its call -- are `A + B`, `A * w` and `lerp(A, B, t)`. Rewritten as
		 * the core op of that spelling, typed Substrate: the binder sends an operator over a Substrate value back to
		 * the node (LangBinderSubstrate.cpp), so the text compiles to the node it was read from.
		 */
		static bool TryRaiseSubstrateOperator(FRaiseGraph& Raise, const int32 NodeIndex)
		{
			const FIRNode& Self = Raise.Graph.Nodes[NodeIndex];
			if (!Raise.ClassOf(Self) || !Self.Properties.IsEmpty()
				|| Self.Outputs.Num() != 1 || Self.Outputs[0].Kind != IR::EIRTypeKind::Substrate)
			{
				return false;
			}

			if (Raise.IsEngineClass(Self, TEXT("MaterialExpressionSubstrateAdd")) && HasOnlyInputs(Self, { TEXT("A"), TEXT("B") }))
			{
				const FIRValue A = ValueOfPin(Self, TEXT("A"));
				const FIRValue B = ValueOfPin(Self, TEXT("B"));
				if (A.IsValid() && B.IsValid())
				{
					Raise.RewriteAsCoreOp(NodeIndex, EIROp::Add, { A, B }, FIRType::Substrate());
					return true;
				}
			}
			else if (Raise.IsEngineClass(Self, TEXT("MaterialExpressionSubstrateWeight")) && HasOnlyInputs(Self, { TEXT("A"), TEXT("Weight") }))
			{
				const FIRValue A = ValueOfPin(Self, TEXT("A"));
				const FIRValue Weight = ValueOfPin(Self, TEXT("Weight"));
				if (A.IsValid() && Weight.IsValid())
				{
					Raise.RewriteAsCoreOp(NodeIndex, EIROp::Multiply, { A, Weight }, FIRType::Substrate());
					return true;
				}
			}
			else if (Raise.IsEngineClass(Self, TEXT("MaterialExpressionSubstrateHorizontalMixing"))
				&& HasOnlyInputs(Self, { TEXT("Background"), TEXT("Foreground"), TEXT("Mix") }))
			{
				const FIRValue Background = ValueOfPin(Self, TEXT("Background"));
				const FIRValue Foreground = ValueOfPin(Self, TEXT("Foreground"));
				const FIRValue Mix = ValueOfPin(Self, TEXT("Mix"));
				if (Background.IsValid() && Foreground.IsValid() && Mix.IsValid())
				{
					Raise.RewriteAsCoreOp(NodeIndex, EIROp::Lerp, { Background, Foreground, Mix }, FIRType::Substrate());
					return true;
				}
			}
			return false;
		}

		static bool RaiseSubstrateSugar(FRaiseGraph& Raise)
		{
			if (!Raise.Catalog)
			{
				return false;
			}
			bool bChanged = false;
			for (int32 NodeIndex = 0; NodeIndex < Raise.Graph.Nodes.Num(); ++NodeIndex)
			{
				// The arguments first: they are read off a BSDF, which no operator rule touches.
				bChanged |= TryRaiseSubstrateArguments(Raise, NodeIndex);
				bChanged |= TryRaiseSubstrateOperator(Raise, NodeIndex);
			}
			return bChanged;
		}

		static bool RaiseGraphOnce(FRaiseGraph& Raise)
		{
			bool bChanged = false;
			for (int32 NodeIndex = 0; NodeIndex < Raise.Graph.Nodes.Num(); ++NodeIndex)
			{
				// Largest shape first: refract contains two `1 - x` nodes and an If, reflect a Subtract, and the
				// smaller rules must not take a piece of either.
				bChanged |= TryRaiseRefract(Raise, NodeIndex)
					|| TryRaiseReflect(Raise, NodeIndex)
					|| TryRaiseFwidth(Raise, NodeIndex)
					|| TryRaiseReciprocal(Raise, NodeIndex)
					|| TryRaiseComparison(Raise, NodeIndex)
					|| TryRaiseSelect(Raise, NodeIndex)
					|| TryRaiseNegate(Raise, NodeIndex)
					|| TryRaiseLogicalNot(Raise, NodeIndex);
			}
			return bChanged;
		}
	}

	void RaiseDreamShaderIR(IR::FIRModule& Module, FLangDiagnosticSink& Diagnostics, const IR::FBuiltinCatalog* Catalog)
	{
		bool bAnyChange = false;
		for (IR::FIRProduct& Product : Module.Products)
		{
			if (Product.Kind == IR::EIRProductKind::MaterialInstance)
			{
				continue;
			}

			DecompileRaise::FRaiseGraph Raise(Product.Graph);
			// To a fixed point: `!b` needs b raised to a comparison first, and the node order says nothing about
			// which of the two comes first. Every pass that changes something removes at least one candidate.
			for (int32 Guard = 0; Guard < 64 && DecompileRaise::RaiseGraphOnce(Raise); ++Guard)
			{
				bAnyChange = true;
			}

			// Last, and once: the core ops it makes are typed Substrate, and no rule above is about those.
			Raise.Catalog = Catalog;
			bAnyChange |= DecompileRaise::RaiseSubstrateSugar(Raise);
		}

		if (!bAnyChange)
		{
			return;
		}

		// The nodes a rule swallowed have no reader left. Prune renumbers the graph and keeps every name, region
		// and source reference; folding and dedupe stay off, as they are for the whole decompile.
		IR::FIRPassOptions PassOptions;
		PassOptions.bFoldConstants = false;
		PassOptions.bDedupe = false;
		PassOptions.bPrune = true;
		IR::RunDreamShaderIRPasses(Module, PassOptions, Diagnostics);
	}
}
