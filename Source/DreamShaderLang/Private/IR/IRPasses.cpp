// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Five passes, in this order: fold, canonicalise swizzles, dedupe, prune, re-index.
//
// One invariant holds all of it together: the node array is in topological order when the builder
// hands it over (a node is made after its operands), and no pass here ever ADDS a node. A folded
// expression is rewritten in place -- the Multiply becomes the Constant, at the same index -- a
// composed swizzle keeps its index, a merged duplicate is marked dead rather than moved. So the
// order survives every pass, re-indexing is a stable compaction, and running the whole thing twice
// produces the identical array, which is what the golden IR dumps depend on.
//
// Diagnostics owned by this file: DSH4390 (info, an unused uniform).

#include "IR/IRPasses.h"

#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/Crc.h"
#include "Templates/Function.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.IRPasses"

namespace UE::DreamShader::IR::Private
{
	/** A constant operand, as the folder sees it: up to four doubles and how many of them count. */
	struct FFoldConstant
	{
		double V[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 N = 1;

		double Get(int32 Index) const { return N == 1 ? V[0] : (Index < N ? V[Index] : 0.0); }
	};

	static bool TryReadConstant(const FIRGraph& Graph, FIRValue Value, FFoldConstant& Out)
	{
		if (!Graph.Nodes.IsValidIndex(Value.Node) || Value.Output != 0)
		{
			return false;
		}
		const FIRNode& Node = Graph.Nodes[Value.Node];
		if (Node.Op != EIROp::Constant)
		{
			return false;
		}
		const FIRProperty* Property = Node.FindProperty(Prop::Value);
		if (!Property)
		{
			return false;
		}
		Out.N = FMath::Clamp(Property->Value.N, 1, 4);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Out.V[Index] = Property->Value.V[Index];
		}
		return true;
	}

	static double Dot3(const FFoldConstant& A, const FFoldConstant& B, int32 Width)
	{
		double Sum = 0.0;
		for (int32 Index = 0; Index < Width; ++Index)
		{
			Sum += A.Get(Index) * B.Get(Index);
		}
		return Sum;
	}

	static double SafeDivide(double A, double B)
	{
		return B != 0.0 ? A / B : 0.0;
	}

	/** Everything that folds component by component; false when the op is not one of them. */
	static bool FoldElementwise(EIROp Op, const TArray<FFoldConstant>& In, int32 Index, double& Out)
	{
		const double A = In.Num() > 0 ? In[0].Get(Index) : 0.0;
		const double B = In.Num() > 1 ? In[1].Get(Index) : 0.0;
		const double C = In.Num() > 2 ? In[2].Get(Index) : 0.0;

		switch (Op)
		{
		case EIROp::Add: Out = A + B; return true;
		case EIROp::Subtract: Out = A - B; return true;
		case EIROp::Multiply: Out = A * B; return true;
		case EIROp::Divide: Out = SafeDivide(A, B); return true;
		case EIROp::Fmod: Out = B != 0.0 ? FMath::Fmod(A, B) : 0.0; return true;
		case EIROp::Negate: Out = -A; return true;
		case EIROp::Abs: Out = FMath::Abs(A); return true;
		case EIROp::Floor: Out = FMath::FloorToDouble(A); return true;
		case EIROp::Ceil: Out = FMath::CeilToDouble(A); return true;
		case EIROp::Round: Out = FMath::RoundToDouble(A); return true;
		case EIROp::Frac: Out = A - FMath::FloorToDouble(A); return true;
		case EIROp::Truncate: Out = FMath::TruncToDouble(A); return true;
		case EIROp::Sign: Out = (A > 0.0) ? 1.0 : ((A < 0.0) ? -1.0 : 0.0); return true;
		case EIROp::Saturate: Out = FMath::Clamp(A, 0.0, 1.0); return true;
		case EIROp::Clamp: Out = FMath::Clamp(A, B, C); return true;
		case EIROp::Min: Out = FMath::Min(A, B); return true;
		case EIROp::Max: Out = FMath::Max(A, B); return true;
		case EIROp::Lerp: Out = A + (B - A) * C; return true;
		case EIROp::Step: Out = (B >= A) ? 1.0 : 0.0; return true;
		case EIROp::SmoothStep:
		{
			const double T = FMath::Clamp(SafeDivide(C - A, B - A), 0.0, 1.0);
			Out = T * T * (3.0 - 2.0 * T);
			return true;
		}
		case EIROp::Sqrt: Out = A > 0.0 ? FMath::Sqrt(A) : 0.0; return true;
		case EIROp::Rsqrt: Out = A > 0.0 ? 1.0 / FMath::Sqrt(A) : 0.0; return true;
		case EIROp::Rcp: Out = SafeDivide(1.0, A); return true;
		case EIROp::Pow: Out = FMath::Pow(A, B); return true;
		case EIROp::Exp: Out = FMath::Exp(A); return true;
		case EIROp::Exp2: Out = FMath::Pow(2.0, A); return true;
		case EIROp::Log: Out = A > 0.0 ? FMath::Loge(A) : 0.0; return true;
		case EIROp::Log2: Out = A > 0.0 ? FMath::Log2(A) : 0.0; return true;
		case EIROp::Log10: Out = A > 0.0 ? FMath::LogX(10.0, A) : 0.0; return true;
		case EIROp::Sin: Out = FMath::Sin(A); return true;
		case EIROp::Cos: Out = FMath::Cos(A); return true;
		case EIROp::Tan: Out = FMath::Tan(A); return true;
		case EIROp::Asin: Out = FMath::Asin(FMath::Clamp(A, -1.0, 1.0)); return true;
		case EIROp::Acos: Out = FMath::Acos(FMath::Clamp(A, -1.0, 1.0)); return true;
		case EIROp::Atan: Out = FMath::Atan(A); return true;
		case EIROp::Atan2: Out = FMath::Atan2(A, B); return true;
		// Built from Exp rather than named directly: FMath spells the hyperbolic three differently
		// across engine versions and this is the same number in all of them.
		case EIROp::Sinh: Out = 0.5 * (FMath::Exp(A) - FMath::Exp(-A)); return true;
		case EIROp::Cosh: Out = 0.5 * (FMath::Exp(A) + FMath::Exp(-A)); return true;
		case EIROp::Tanh:
		{
			const double Positive = FMath::Exp(A);
			const double Negative = FMath::Exp(-A);
			Out = SafeDivide(Positive - Negative, Positive + Negative);
			return true;
		}
		case EIROp::Less: Out = (A < B) ? 1.0 : 0.0; return true;
		case EIROp::LessEqual: Out = (A <= B) ? 1.0 : 0.0; return true;
		case EIROp::Greater: Out = (A > B) ? 1.0 : 0.0; return true;
		case EIROp::GreaterEqual: Out = (A >= B) ? 1.0 : 0.0; return true;
		case EIROp::Equal: Out = (A == B) ? 1.0 : 0.0; return true;
		case EIROp::NotEqual: Out = (A != B) ? 1.0 : 0.0; return true;
		case EIROp::LogicalAnd: Out = (A != 0.0 && B != 0.0) ? 1.0 : 0.0; return true;
		case EIROp::LogicalOr: Out = (A != 0.0 || B != 0.0) ? 1.0 : 0.0; return true;
		case EIROp::LogicalNot: Out = (A == 0.0) ? 1.0 : 0.0; return true;
		case EIROp::Convert: Out = A; return true;
		case EIROp::Broadcast: Out = In.Num() > 0 ? In[0].Get(0) : 0.0; return true;
		default: return false;
		}
	}

	/** The ops whose result width is not "the widest operand". */
	static bool TryFoldSpecialWidth(EIROp Op, const TArray<FFoldConstant>& In, int32 OperandWidth, FFoldConstant& Out)
	{
		switch (Op)
		{
		case EIROp::Dot:
			Out.N = 1;
			Out.V[0] = Dot3(In[0], In[1], OperandWidth);
			return true;

		case EIROp::Length:
			Out.N = 1;
			Out.V[0] = FMath::Sqrt(Dot3(In[0], In[0], OperandWidth));
			return true;

		case EIROp::Distance:
		{
			FFoldConstant Delta;
			Delta.N = OperandWidth;
			for (int32 Index = 0; Index < OperandWidth; ++Index)
			{
				Delta.V[Index] = In[0].Get(Index) - In[1].Get(Index);
			}
			Out.N = 1;
			Out.V[0] = FMath::Sqrt(Dot3(Delta, Delta, OperandWidth));
			return true;
		}

		case EIROp::Normalize:
		{
			const double Length = FMath::Sqrt(Dot3(In[0], In[0], OperandWidth));
			Out.N = OperandWidth;
			for (int32 Index = 0; Index < OperandWidth; ++Index)
			{
				Out.V[Index] = Length > 0.0 ? In[0].Get(Index) / Length : 0.0;
			}
			return true;
		}

		case EIROp::Cross:
			Out.N = 3;
			Out.V[0] = In[0].Get(1) * In[1].Get(2) - In[0].Get(2) * In[1].Get(1);
			Out.V[1] = In[0].Get(2) * In[1].Get(0) - In[0].Get(0) * In[1].Get(2);
			Out.V[2] = In[0].Get(0) * In[1].Get(1) - In[0].Get(1) * In[1].Get(0);
			return true;

		default:
			return false;
		}
	}

	/** A node index followed through however many merges took it somewhere else. */
	static int32 ResolveCanonical(const TArray<int32>& Canonical, int32 Node)
	{
		int32 Target = Node;
		int32 Guard = 0;
		while (Canonical.IsValidIndex(Target) && Canonical[Target] != Target && Guard++ < 64)
		{
			Target = Canonical[Target];
		}
		return Target;
	}

	/** What the passes remember about a parameter they threw away, for DSH4390. */
	struct FPrunedParameter
	{
		Lang::FLangSpan Span;
		FString Name;
	};

	/** Every value a node reads, as references that can be rewritten. */
	static void ForEachValueRef(FIRNode& Node, TFunctionRef<void(FIRValue&)> Visit)
	{
		for (FIRValue& Operand : Node.Operands)
		{
			Visit(Operand);
		}
		for (FIRInput& Input : Node.Inputs)
		{
			Visit(Input.Value);
		}
	}

	// -------------------------------------------------------------------------- constant folding

	static bool FoldConstants(FIRGraph& Graph)
	{
		bool bChanged = false;

		for (FIRNode& Node : Graph.Nodes)
		{
			const bool bStructural = Node.Op == EIROp::Swizzle || Node.Op == EIROp::Append;
			if (!bStructural && !IsCoreMathOp(Node.Op))
			{
				continue;
			}
			if (Node.Op == EIROp::Constant || Node.Operands.IsEmpty())
			{
				continue;
			}
			// A derivative of a constant is zero, but DDX/DDY/Fwidth are screen-space reads and
			// folding them away would quietly change what a graph means to someone reading it.
			if (Node.Op == EIROp::DDX || Node.Op == EIROp::DDY || Node.Op == EIROp::Fwidth)
			{
				continue;
			}

			TArray<FFoldConstant> Operands;
			Operands.Reserve(Node.Operands.Num());
			bool bAllConstant = true;
			for (const FIRValue& Operand : Node.Operands)
			{
				FFoldConstant Constant;
				if (!TryReadConstant(Graph, Operand, Constant))
				{
					bAllConstant = false;
					break;
				}
				Operands.Add(Constant);
			}
			if (!bAllConstant || Operands.IsEmpty())
			{
				continue;
			}

			// The passes run BEFORE the validator, so a node whose arity the builder got wrong
			// reaches this loop unchecked, and TryFoldSpecialWidth indexes In[0] and In[1] without
			// asking. Leave a malformed node alone and let the validator name it (DSH4302).
			const FIRCoreOpInfo& OpInfo = GetCoreOpInfo(Node.Op);
			if (Operands.Num() < OpInfo.MinArity || Operands.Num() > OpInfo.MaxArity)
			{
				continue;
			}

			FFoldConstant Result;
			int32 OperandWidth = 1;
			for (const FFoldConstant& Operand : Operands)
			{
				OperandWidth = FMath::Max(OperandWidth, Operand.N);
			}

			if (Node.Op == EIROp::Swizzle)
			{
				const FIRProperty* Mask = Node.FindProperty(Prop::Mask);
				if (!Mask || Mask->Value.S.IsEmpty())
				{
					continue;
				}
				const FString& Text = Mask->Value.S;
				Result.N = FMath::Clamp(Text.Len(), 1, 4);
				for (int32 Index = 0; Index < Result.N; ++Index)
				{
					const int32 Component =
						Text[Index] == TCHAR('x') ? 0 :
						Text[Index] == TCHAR('y') ? 1 :
						Text[Index] == TCHAR('z') ? 2 :
						Text[Index] == TCHAR('w') ? 3 : INDEX_NONE;
					if (Component == INDEX_NONE)
					{
						Result.N = 0;
						break;
					}
					Result.V[Index] = Operands[0].Get(Component);
				}
				if (Result.N == 0)
				{
					continue;
				}
			}
			else if (Node.Op == EIROp::Append)
			{
				int32 Written = 0;
				for (const FFoldConstant& Operand : Operands)
				{
					for (int32 Index = 0; Index < Operand.N && Written < 4; ++Index)
					{
						Result.V[Written++] = Operand.V[Index];
					}
				}
				Result.N = FMath::Max(Written, 1);
			}
			else if (!TryFoldSpecialWidth(Node.Op, Operands, OperandWidth, Result))
			{
				// Reflect and Refract have a physical meaning that is not worth a second
				// implementation here; they stay as nodes.
				if (Node.Op == EIROp::Reflect || Node.Op == EIROp::Refract)
				{
					continue;
				}
				const int32 Width = (Node.Op == EIROp::Broadcast)
					? FMath::Max(Node.Outputs.IsValidIndex(0) ? Node.Outputs[0].GraphComponentCount() : 1, 1)
					: OperandWidth;
				Result.N = FMath::Clamp(Width, 1, 4);
				bool bFolded = true;
				for (int32 Index = 0; Index < Result.N; ++Index)
				{
					if (!FoldElementwise(Node.Op, Operands, Index, Result.V[Index]))
					{
						bFolded = false;
						break;
					}
				}
				if (!bFolded)
				{
					continue;
				}
			}

			// Rewritten in place, so the array stays topologically ordered and every FIRValue that
			// pointed at this node still points at the same index.
			const FIRType ResultType = Node.Outputs.IsValidIndex(0) ? Node.Outputs[0] : FIRType::Float(Result.N);
			Node.Op = EIROp::Constant;
			Node.Operands.Reset();
			Node.Inputs.Reset();
			Node.Properties.Reset();
			Node.ClassName.Reset();
			Node.CatalogIndex = INDEX_NONE;
			Node.OutputNames.Reset();
			Node.Outputs.Reset();
			Node.Outputs.Add(ResultType.IsBool() ? FIRType::Bool(Result.N) : FIRType::Float(Result.N));
			Node.Properties.Add({ FString(Prop::Value), FIRPropertyValue::MakeFloat4(Result.V, Result.N) });
			bChanged = true;
		}

		return bChanged;
	}

	// ------------------------------------------------------------------- swizzle canonicalisation

	static bool CanonicaliseSwizzles(FIRGraph& Graph, TArray<int32>& Canonical, TArray<bool>& bAlive)
	{
		bool bChanged = false;
		const TCHAR* const Components = TEXT("xyzw");

		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (!bAlive[Index] || Graph.Nodes[Index].Op != EIROp::Swizzle || Graph.Nodes[Index].Operands.IsEmpty())
			{
				continue;
			}

			FIRNode& Node = Graph.Nodes[Index];
			const FIRProperty* MaskProperty = Node.FindProperty(Prop::Mask);
			if (!MaskProperty)
			{
				continue;
			}
			const FString Mask = MaskProperty->Value.S;
			const FIRValue Source = Node.Operands[0];
			if (!Graph.Nodes.IsValidIndex(Source.Node))
			{
				continue;
			}

			// An identity mask -- the source's own components in order -- is not a node.
			const FIRNode& SourceNode = Graph.Nodes[Source.Node];
			const int32 SourceWidth = SourceNode.Outputs.IsValidIndex(Source.Output)
				? FMath::Max(SourceNode.Outputs[Source.Output].GraphComponentCount(), 1)
				: 1;
			// ...and only when the source is its node's DEFAULT output. Canonical[] repoints a
			// user's FIRValue by node index alone and leaves the output slot where it was, so
			// collapsing `(%7#3).x` onto %7 would quietly move the read from output 3 to output 0.
			if (Mask.Len() == SourceWidth && Source.Output == 0)
			{
				bool bIdentity = true;
				for (int32 Component = 0; Component < Mask.Len(); ++Component)
				{
					bIdentity = bIdentity && Mask[Component] == Components[Component];
				}
				if (bIdentity)
				{
					Canonical[Index] = Source.Node;
					// Only the source's own output survives, which is what a caller of an identity
					// mask meant anyway.
					bAlive[Index] = false;
					bChanged = true;
					continue;
				}
			}

			// A swizzle of a swizzle is one swizzle, when the composition is still ascending (the
			// only shape a ComponentMask can express).
			if (SourceNode.Op == EIROp::Swizzle && Source.Output == 0 && !SourceNode.Operands.IsEmpty())
			{
				const FIRProperty* InnerMaskProperty = SourceNode.FindProperty(Prop::Mask);
				if (!InnerMaskProperty)
				{
					continue;
				}
				const FString Inner = InnerMaskProperty->Value.S;
				FString Composed;
				bool bComposable = true;
				int32 Previous = INDEX_NONE;
				for (int32 Component = 0; Component < Mask.Len(); ++Component)
				{
					const int32 Which =
						Mask[Component] == TCHAR('x') ? 0 :
						Mask[Component] == TCHAR('y') ? 1 :
						Mask[Component] == TCHAR('z') ? 2 :
						Mask[Component] == TCHAR('w') ? 3 : INDEX_NONE;
					if (Which == INDEX_NONE || Which >= Inner.Len())
					{
						bComposable = false;
						break;
					}
					const TCHAR Letter = Inner[Which];
					const int32 Absolute =
						Letter == TCHAR('x') ? 0 :
						Letter == TCHAR('y') ? 1 :
						Letter == TCHAR('z') ? 2 :
						Letter == TCHAR('w') ? 3 : INDEX_NONE;
					if (Absolute == INDEX_NONE || Absolute <= Previous)
					{
						bComposable = false;
						break;
					}
					Previous = Absolute;
					Composed.AppendChar(Letter);
				}

				if (bComposable && !Composed.IsEmpty())
				{
					Node.Operands[0] = SourceNode.Operands[0];
					if (FIRProperty* Writable = Node.FindProperty(Prop::Mask))
					{
						Writable->Value = FIRPropertyValue::MakeString(Composed);
					}
					bChanged = true;
				}
			}
		}

		return bChanged;
	}

	// ---------------------------------------------------------------------------------- dedupe

	static FString BuildDedupeKey(const FIRNode& Node)
	{
		TArray<FString> Parts;
		Parts.Add(LexToString(Node.Op));
		Parts.Add(Node.ClassName);
		// ClassName is the catalog ShortName, which is unique only WITHIN a namespace: `UE.Foo` and
		// `Substrate.Foo` are two entries with one name, and the index is what tells them apart.
		if (Node.CatalogIndex != INDEX_NONE)
		{
			Parts.Add(FString::Printf(TEXT("@%d"), Node.CatalogIndex));
		}

		// The output signature is part of the identity, not a consequence of the rest of it. Two
		// `Broadcast` nodes over the same operand carry the same (empty) property table and differ
		// only in the width they broadcast TO, and a bool `Constant` differs from a float one only
		// in its output kind -- both would merge, and one of the two users would silently get the
		// other's width or class. Everything a node's outputs are derived from is either already in
		// the key or, for these two, nowhere else at all.
		for (int32 Index = 0; Index < Node.Outputs.Num(); ++Index)
		{
			Parts.Add(FString::Printf(
				TEXT("->%s:%s"),
				Node.OutputNames.IsValidIndex(Index) ? *Node.OutputNames[Index] : TEXT(""),
				*Node.Outputs[Index].ToString()));
		}

		TArray<FString> Properties;
		Properties.Reserve(Node.Properties.Num());
		for (const FIRProperty& Property : Node.Properties)
		{
			Properties.Add(FString::Printf(TEXT("%s=%s"), *Property.Name, *Property.Value.ToString()));
		}
		Properties.Sort();
		Parts.Append(Properties);

		for (const FIRValue& Operand : Node.Operands)
		{
			Parts.Add(FString::Printf(TEXT("%d#%d"), Operand.Node, Operand.Output));
		}

		TArray<FString> Inputs;
		Inputs.Reserve(Node.Inputs.Num());
		for (const FIRInput& Input : Node.Inputs)
		{
			Inputs.Add(FString::Printf(TEXT("%s=%d#%d"), *Input.Pin, Input.Value.Node, Input.Value.Output));
		}
		Inputs.Sort();
		Parts.Append(Inputs);

		return FString::Join(Parts, TEXT("|"));
	}

	/**
	 * A node that is an identity rather than a value: a statement (which includes a reflected
	 * custom-output node) and a FunctionInput both have a place in the graph's index arrays that
	 * merging them away would corrupt.
	 */
	static bool IsMergeable(const FIRNode& Node)
	{
		return !Node.IsStatement() && Node.Op != EIROp::FunctionInput;
	}

	/**
	 * FString's own hash and operator== ignore case, so a plain TMap<FString, int32> merges two keys
	 * that differ only in case -- `uniform float Gain` and `uniform float gain`, or two @custom bodies
	 * -- and a structural key must never do that (CONTRACT 6.7).
	 */
	struct FIRPassesCaseSensitiveKeyFuncs : BaseKeyFuncs<TPair<FString, int32>, FString, /*bInAllowDuplicateKeys*/ false>
	{
		static const FString& GetSetKey(const TPair<FString, int32>& Element) { return Element.Key; }
		static bool Matches(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
		static uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
	};

	static bool Dedupe(FIRGraph& Graph, TArray<int32>& Canonical, TArray<bool>& bAlive)
	{
		bool bChanged = false;
		TMap<FString, int32, FDefaultSetAllocator, FIRPassesCaseSensitiveKeyFuncs> FirstByKey;
		FirstByKey.Reserve(Graph.Nodes.Num());

		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (!bAlive[Index])
			{
				continue;
			}

			FIRNode& Node = Graph.Nodes[Index];
			// Every operand has already been given its canonical index, so the key below is
			// structural by induction: two nodes with the same key read the same values.
			ForEachValueRef(Node, [&Canonical](FIRValue& Value)
			{
				Value.Node = ResolveCanonical(Canonical, Value.Node);
			});

			Node.DedupeKey = BuildDedupeKey(Node);
			if (!IsMergeable(Node))
			{
				continue;
			}

			if (const int32* First = FirstByKey.Find(Node.DedupeKey))
			{
				Canonical[Index] = *First;
				bAlive[Index] = false;
				bChanged = true;
			}
			else
			{
				FirstByKey.Add(Node.DedupeKey, Index);
			}
		}

		return bChanged;
	}

	// ----------------------------------------------------------------------------------- prune

	static void Prune(FIRGraph& Graph, const TArray<int32>& Canonical, TArray<bool>& bAlive, TArray<FPrunedParameter>& OutUnusedParameters)
	{
		TArray<int32> Stack;
		TSet<int32> Reached;

		const auto Push = [&Graph, &Canonical, &bAlive, &Stack, &Reached](int32 Index)
		{
			const int32 Node = ResolveCanonical(Canonical, Index);
			if (Graph.Nodes.IsValidIndex(Node) && bAlive[Node] && !Reached.Contains(Node))
			{
				Reached.Add(Node);
				Stack.Add(Node);
			}
		};

		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (!bAlive[Index])
			{
				continue;
			}
			const FIRNode& Node = Graph.Nodes[Index];
			// The roots: every statement -- the material's own pins, a function's outputs and a
			// reflected custom-output node, which IsStatement() covers since CONTRACT 6.13 #19 --
			// plus a function's inputs, which are part of the asset's signature whether or not the
			// body reads them.
			const bool bIsRoot = Node.IsStatement() || Node.Op == EIROp::FunctionInput;
			if (bIsRoot)
			{
				Push(Index);
			}
		}

		while (!Stack.IsEmpty())
		{
			const int32 Index = Stack.Pop();
			TArray<FIRValue> Values;
			FIRGraph::CollectInputValues(Graph.Nodes[Index], Values);
			for (const FIRValue& Value : Values)
			{
				Push(Value.Node);
			}
		}

		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (!bAlive[Index] || Reached.Contains(Index))
			{
				continue;
			}
			const FIRNode& Node = Graph.Nodes[Index];
			if (Node.Op == EIROp::Parameter || Node.Op == EIROp::TextureParameter)
			{
				// Captured now, because the node itself is about to stop existing.
				const FIRProperty* Name = Node.FindProperty(Prop::ParameterName);
				OutUnusedParameters.Add({ Node.Source.Span, Name ? Name->Value.S : Node.DebugName });
			}
			bAlive[Index] = false;
		}
	}

	// --------------------------------------------------------------------------------- re-index

	static void Reindex(FIRGraph& Graph, const TArray<int32>& Canonical, const TArray<bool>& bAlive)
	{
		TArray<int32> NewIndex;
		NewIndex.Init(INDEX_NONE, Graph.Nodes.Num());

		TArray<FIRNode> Survivors;
		Survivors.Reserve(Graph.Nodes.Num());
		for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
		{
			if (bAlive[Index])
			{
				NewIndex[Index] = Survivors.Num();
				Survivors.Add(MoveTemp(Graph.Nodes[Index]));
			}
		}

		const auto Remap = [&Canonical, &NewIndex](int32 Node) -> int32
		{
			if (!Canonical.IsValidIndex(Node))
			{
				return INDEX_NONE;
			}
			// Follow the merge chain first: a node may have been merged into one that was itself
			// merged (dedupe writes Canonical in topological order, so one step is enough, but the
			// loop costs nothing and makes that not matter).
			int32 Target = Node;
			while (Canonical.IsValidIndex(Target) && Canonical[Target] != Target)
			{
				Target = Canonical[Target];
			}
			return NewIndex.IsValidIndex(Target) ? NewIndex[Target] : INDEX_NONE;
		};

		for (FIRNode& Node : Survivors)
		{
			ForEachValueRef(Node, [&Remap](FIRValue& Value)
			{
				Value.Node = Remap(Value.Node);
			});
		}

		Graph.Nodes = MoveTemp(Survivors);
		Graph.Sink = Graph.Sink != INDEX_NONE ? Remap(Graph.Sink) : INDEX_NONE;
		for (int32& Index : Graph.FunctionInputs)
		{
			Index = Remap(Index);
		}
		for (int32& Index : Graph.FunctionOutputs)
		{
			Index = Remap(Index);
		}
		Graph.FunctionInputs.RemoveAll([](int32 Index) { return Index == INDEX_NONE; });
		Graph.FunctionOutputs.RemoveAll([](int32 Index) { return Index == INDEX_NONE; });
	}

	static void RunGraphPasses(FIRGraph& Graph, const FIRPassOptions& Options, TArray<FPrunedParameter>& OutUnusedParameters)
	{
		TArray<int32> Canonical;
		Canonical.SetNum(Graph.Nodes.Num());
		for (int32 Index = 0; Index < Canonical.Num(); ++Index)
		{
			Canonical[Index] = Index;
		}

		TArray<bool> bAlive;
		bAlive.Init(true, Graph.Nodes.Num());

		// Folding exposes swizzles to canonicalise, canonicalising exposes duplicates, and a merge
		// can put two equal constants in front of the folder. Three rounds is more than any real
		// graph needs; the bound is here so a bug cannot spin.
		// Unreachable nodes out before anything merges: the builder folds an all-literal constructor into
		// one Constant and leaves the literals it folded behind, unread, and a merge must never keep one of
		// those as the node that survives -- a live read would inherit the span and name of a value nothing
		// reads. This pass reports nothing; the prune at the end reports the parameters nobody reads.
		if (Options.bPrune)
		{
			TArray<FPrunedParameter> NotReported;
			Prune(Graph, Canonical, bAlive, NotReported);
		}

		for (int32 Round = 0; Round < 4; ++Round)
		{
			bool bChanged = false;
			if (Options.bFoldConstants)
			{
				bChanged |= FoldConstants(Graph);
				bChanged |= CanonicaliseSwizzles(Graph, Canonical, bAlive);
			}
			if (Options.bDedupe)
			{
				bChanged |= Dedupe(Graph, Canonical, bAlive);
			}
			if (!bChanged)
			{
				break;
			}
		}

		if (Options.bPrune)
		{
			Prune(Graph, Canonical, bAlive, OutUnusedParameters);
		}

		Reindex(Graph, Canonical, bAlive);

		// The keys are stamped again on the final indices, so a second run of the whole pipeline
		// produces byte-identical keys on a graph nothing else changed.
		//
		// With dedupe turned OFF they are cleared instead, rather than left describing a graph
		// nobody merged: the validator reads a filled key as a promise that no two nodes share one
		// (DSH4322) and "empty everywhere" as "the pass did not run", so stamping here would turn
		// every legitimate duplicate into an error. Clearing rather than skipping also keeps a
		// module that was passed once with dedupe and again without from carrying stale keys that
		// name pre-re-index node numbers.
		for (FIRNode& Node : Graph.Nodes)
		{
			Node.DedupeKey = Options.bDedupe ? BuildDedupeKey(Node) : FString();
		}
	}
}

namespace UE::DreamShader::IR
{
	void RunDreamShaderIRPasses(FIRModule& Module, const FIRPassOptions& Options, Lang::FLangDiagnosticSink& Diagnostics)
	{
		for (FIRProduct& Product : Module.Products)
		{
			TArray<Private::FPrunedParameter> UnusedParameters;
			Private::RunGraphPasses(Product.Graph, Options, UnusedParameters);

			for (const Private::FPrunedParameter& Parameter : UnusedParameters)
			{
				Diagnostics.Info(TEXT("DSH4390"), Parameter.Span, FText::Format(
					LOCTEXT("IRPassesUnusedUniform", "'{0}' is declared but nothing reads it, so it is not in the generated material."),
					FText::FromString(Parameter.Name)));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
