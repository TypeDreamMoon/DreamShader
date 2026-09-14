// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The IR containers: property values, node lookups, graph traversal, module queries.
//
// Two of these are load-bearing beyond their size:
//
//   * FIRPropertyValue::ToString() is a WIRE format. It renders the dedupe key (CONTRACT §6.7),
//     the text dump (the corpus golden) and the JSON dump, so it must be culture-invariant, stable
//     between runs, and single-line -- a Custom node's Code is a whole HLSL body and a dump that
//     let it break the line would break "one node per line".
//   * FIRGraph::TopologicalOrder() is both the emitter's walk order and the validator's cycle
//     test. It returns FEWER entries than Nodes.Num() exactly when the graph has a cycle; there is
//     no second "bool bHadCycle" out-parameter, so that is the contract both callers read.

#include "IR/IR.h"

#include "IRJson.h"
#include "IR/IRTypes.h"

#include "Math/UnrealMathUtility.h"

namespace UE::DreamShader::IR
{
	namespace Private
	{
		/**
		 * Renders a text property so it can never leave its line: backslash, the three line-ending
		 * characters and tab become escapes, everything else is written through. Reversible enough
		 * to read, and stable enough to compare.
		 */
		static FString EscapeIRPropertyText(const FString& Value)
		{
			FString Result;
			Result.Reserve(Value.Len());

			for (int32 Index = 0; Index < Value.Len(); ++Index)
			{
				const TCHAR Character = Value[Index];
				switch (Character)
				{
				case TEXT('\\'): Result += TEXT("\\\\"); break;
				case TEXT('\n'): Result += TEXT("\\n");  break;
				case TEXT('\r'): Result += TEXT("\\r");  break;
				case TEXT('\t'): Result += TEXT("\\t");  break;
				default:         Result.AppendChar(Character); break;
				}
			}

			return Result;
		}
	}

	// ------------------------------------------------------------------------------ properties

	FIRPropertyValue FIRPropertyValue::MakeBool(const bool InB)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::Bool;
		Result.B = InB;
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeInt(const int64 InI)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::Int;
		Result.I = InI;
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeFloat(const double InF)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::Float;
		Result.F = InF;
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeFloat4(const double* InV, const int32 InN)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::Float4;
		Result.N = FMath::Clamp(InN, 1, 4);

		if (InV != nullptr)
		{
			for (int32 Index = 0; Index < Result.N; ++Index)
			{
				Result.V[Index] = InV[Index];
			}
		}

		// The components past N stay zero rather than uninitialised: they take part in nothing, but
		// a dedupe key computed from a half-filled struct would depend on whatever was on the stack.
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeString(const FString& InS)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::String;
		Result.S = InS;
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeName(const FString& InS)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::Name;
		Result.S = InS;
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeEnum(const FString& InS)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::Enum;
		Result.S = InS;
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeObject(const FString& InPath)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::Object;
		Result.S = InPath;
		return Result;
	}

	FIRPropertyValue FIRPropertyValue::MakeStringList(const TArray<FString>& InList)
	{
		FIRPropertyValue Result;
		Result.Kind = EIRPropertyKind::StringList;
		Result.List = InList;
		return Result;
	}

	FString FIRPropertyValue::ToString() const
	{
		switch (Kind)
		{
		case EIRPropertyKind::Bool:
			return B ? TEXT("true") : TEXT("false");
		case EIRPropertyKind::Int:
			return FString::Printf(TEXT("%lld"), I);
		case EIRPropertyKind::Float:
			return Private::FormatIRNumber(F);
		case EIRPropertyKind::Float4:
		{
			// Always parenthesised, even for N == 1: a Float4 and a Float are different kinds and
			// a dedupe key must not be able to confuse `DefaultValue=1` with `DefaultValue=(1)`.
			const int32 Count = FMath::Clamp(N, 1, 4);
			FString Result = TEXT("(");
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (Index > 0)
				{
					Result += TEXT(", ");
				}
				Result += Private::FormatIRNumber(V[Index]);
			}
			Result += TEXT(")");
			return Result;
		}
		case EIRPropertyKind::String:
		case EIRPropertyKind::Name:
		case EIRPropertyKind::Enum:
		case EIRPropertyKind::Object:
			return Private::EscapeIRPropertyText(S);
		case EIRPropertyKind::StringList:
		{
			FString Result = TEXT("[");
			for (int32 Index = 0; Index < List.Num(); ++Index)
			{
				if (Index > 0)
				{
					Result += TEXT(", ");
				}
				Result += Private::EscapeIRPropertyText(List[Index]);
			}
			Result += TEXT("]");
			return Result;
		}
		}

		return FString();
	}

	bool FIRPropertyValue::operator==(const FIRPropertyValue& Other) const
	{
		if (Kind != Other.Kind)
		{
			return false;
		}

		switch (Kind)
		{
		case EIRPropertyKind::Bool:
			return B == Other.B;
		case EIRPropertyKind::Int:
			return I == Other.I;
		case EIRPropertyKind::Float:
			return F == Other.F;
		case EIRPropertyKind::Float4:
		{
			if (N != Other.N)
			{
				return false;
			}
			const int32 Count = FMath::Clamp(N, 1, 4);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (V[Index] != Other.V[Index])
				{
					return false;
				}
			}
			return true;
		}
		case EIRPropertyKind::String:
		case EIRPropertyKind::Name:
		case EIRPropertyKind::Enum:
		case EIRPropertyKind::Object:
			// Case-SENSITIVE. FString::operator== is not, and two parameters called `Tint` and
			// `tint` are two parameters.
			return S.Equals(Other.S, ESearchCase::CaseSensitive);
		case EIRPropertyKind::StringList:
		{
			if (List.Num() != Other.List.Num())
			{
				return false;
			}
			for (int32 Index = 0; Index < List.Num(); ++Index)
			{
				if (!List[Index].Equals(Other.List[Index], ESearchCase::CaseSensitive))
				{
					return false;
				}
			}
			return true;
		}
		}

		return false;
	}

	// ----------------------------------------------------------------------------------- nodes

	const FIRProperty* FIRNode::FindProperty(const TCHAR* Name) const
	{
		if (Name == nullptr)
		{
			return nullptr;
		}

		for (const FIRProperty& Property : Properties)
		{
			if (Property.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return &Property;
			}
		}

		return nullptr;
	}

	FIRProperty* FIRNode::FindProperty(const TCHAR* Name)
	{
		if (Name == nullptr)
		{
			return nullptr;
		}

		for (FIRProperty& Property : Properties)
		{
			if (Property.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return &Property;
			}
		}

		return nullptr;
	}

	const FIRInput* FIRNode::FindInput(const FString& Pin) const
	{
		for (const FIRInput& Input : Inputs)
		{
			if (Input.Pin.Equals(Pin, ESearchCase::CaseSensitive))
			{
				return &Input;
			}
		}

		return nullptr;
	}

	int32 FIRNode::FindOutput(const FString& Name) const
	{
		for (int32 Index = 0; Index < OutputNames.Num(); ++Index)
		{
			if (OutputNames[Index].Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}

		return INDEX_NONE;
	}

	// ----------------------------------------------------------------------------------- graph

	int32 FIRGraph::AddNode(FIRNode&& Node)
	{
		return Nodes.Add(MoveTemp(Node));
	}

	const FIRType& FIRGraph::TypeOf(const FIRValue Value) const
	{
		// One shared Error for every unanswerable question, so the reference is always valid and a
		// caller that forgot to check IsValidValue() gets `<error>` rather than a crash.
		static const FIRType ErrorType = FIRType::Error();

		if (!Nodes.IsValidIndex(Value.Node))
		{
			return ErrorType;
		}

		const FIRNode& Node = Nodes[Value.Node];
		if (!Node.Outputs.IsValidIndex(Value.Output))
		{
			return ErrorType;
		}

		return Node.Outputs[Value.Output];
	}

	bool FIRGraph::IsValidValue(const FIRValue Value) const
	{
		return Nodes.IsValidIndex(Value.Node) && Nodes[Value.Node].Outputs.IsValidIndex(Value.Output);
	}

	void FIRGraph::CollectInputValues(const FIRNode& Node, TArray<FIRValue>& OutValues)
	{
		OutValues.Reset();
		OutValues.Reserve(Node.Operands.Num() + Node.Inputs.Num());

		// Operands first, then named inputs, both in stored order. Values that are not valid are
		// included rather than filtered: a hole is part of the node's shape and the validator has
		// to see it. Callers that walk edges must test IsValid().
		for (const FIRValue& Operand : Node.Operands)
		{
			OutValues.Add(Operand);
		}
		for (const FIRInput& Input : Node.Inputs)
		{
			OutValues.Add(Input.Value);
		}
	}

	TArray<int32> FIRGraph::TopologicalOrder() const
	{
		TArray<int32> Order;

		const int32 NodeCount = Nodes.Num();
		if (NodeCount == 0)
		{
			return Order;
		}

		// Dependencies flattened once, CSR style: EdgeStart[N]..EdgeStart[N+1] indexes the nodes N
		// reads. Building it up front keeps the walk below iterative -- a 3000-node chain would
		// overflow the stack if this recursed.
		TArray<int32> EdgeStart;
		TArray<int32> EdgeTargets;
		EdgeStart.SetNumUninitialized(NodeCount + 1);

		TArray<FIRValue> Values;
		for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
		{
			EdgeStart[NodeIndex] = EdgeTargets.Num();
			CollectInputValues(Nodes[NodeIndex], Values);
			for (const FIRValue& Value : Values)
			{
				// A self-reference IS an edge: it makes the node its own dependency and so falls
				// out below as a one-node cycle rather than being quietly emitted.
				if (Nodes.IsValidIndex(Value.Node))
				{
					EdgeTargets.Add(Value.Node);
				}
			}
		}
		EdgeStart[NodeCount] = EdgeTargets.Num();

		enum class EVisitState : uint8
		{
			Unvisited,
			Visiting,
			/** Ordered: every dependency is already in Order. */
			Done,
			/** On a cycle, or reachable from one. Never enters Order. */
			Poisoned,
		};

		TArray<EVisitState> States;
		States.Init(EVisitState::Unvisited, NodeCount);

		struct FFrame
		{
			int32 Node = INDEX_NONE;
			int32 Cursor = 0;
			bool bPoisoned = false;
		};

		TArray<FFrame> Stack;
		Order.Reserve(NodeCount);

		// Roots in ascending index and dependencies in stored order: the same graph always
		// produces the same order, which is what the corpus goldens and the layout pass rely on.
		for (int32 Root = 0; Root < NodeCount; ++Root)
		{
			if (States[Root] != EVisitState::Unvisited)
			{
				continue;
			}

			States[Root] = EVisitState::Visiting;
			Stack.Add(FFrame{ Root, EdgeStart[Root], false });

			while (Stack.Num() > 0)
			{
				const int32 TopIndex = Stack.Num() - 1;
				const int32 Current = Stack[TopIndex].Node;

				if (Stack[TopIndex].Cursor < EdgeStart[Current + 1])
				{
					const int32 Dependency = EdgeTargets[Stack[TopIndex].Cursor++];
					switch (States[Dependency])
					{
					case EVisitState::Unvisited:
						States[Dependency] = EVisitState::Visiting;
						// Nothing may hold a reference into Stack across this Add: it reallocates.
						Stack.Add(FFrame{ Dependency, EdgeStart[Dependency], false });
						break;
					case EVisitState::Visiting:
						// A back edge. The dependency is an ancestor of this frame, so poisoning
						// this frame and letting it propagate up on unwind reaches every node on
						// the cycle -- including the dependency itself.
						Stack[TopIndex].bPoisoned = true;
						break;
					case EVisitState::Poisoned:
						Stack[TopIndex].bPoisoned = true;
						break;
					case EVisitState::Done:
						break;
					}
					continue;
				}

				const int32 Finished = Current;
				const bool bPoisoned = Stack[TopIndex].bPoisoned;
				Stack.SetNum(TopIndex);

				if (bPoisoned)
				{
					States[Finished] = EVisitState::Poisoned;
					if (Stack.Num() > 0)
					{
						Stack.Last().bPoisoned = true;
					}
				}
				else
				{
					States[Finished] = EVisitState::Done;
					Order.Add(Finished);
				}
			}
		}

		// Order.Num() < Nodes.Num() is the cycle signal. Do not "fix" this by appending the
		// poisoned nodes: the emitter would then write a node before its operand.
		return Order;
	}

	// -------------------------------------------------------------------------------- products

	const TCHAR* LexToString(const EIRProductKind Kind)
	{
		switch (Kind)
		{
		case EIRProductKind::Material:           return TEXT("Material");
		case EIRProductKind::MaterialFunction:   return TEXT("MaterialFunction");
		case EIRProductKind::MaterialLayer:      return TEXT("MaterialLayer");
		case EIRProductKind::MaterialLayerBlend: return TEXT("MaterialLayerBlend");
		}

		return TEXT("Material");
	}

	const TCHAR* LexToString(const EIRBackend Backend)
	{
		switch (Backend)
		{
		case EIRBackend::Graph:      return TEXT("Graph");
		case EIRBackend::ThinCustom: return TEXT("ThinCustom");
		}

		return TEXT("Graph");
	}

	int32 FIRModule::CountProducts(const EIRProductKind Kind) const
	{
		int32 Count = 0;
		for (const FIRProduct& Product : Products)
		{
			if (Product.Kind == Kind)
			{
				++Count;
			}
		}
		return Count;
	}

	const FIRProduct* FIRModule::FindProduct(const FString& Name) const
	{
		for (const FIRProduct& Product : Products)
		{
			if (Product.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return &Product;
			}
		}

		return nullptr;
	}
}
