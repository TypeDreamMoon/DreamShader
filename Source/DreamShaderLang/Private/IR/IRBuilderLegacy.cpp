// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The IR builder's side of the legacy rules (design: Plan/m4m5/research-legacy.md section 3.7).
//
//   L3b  `F(args).Out` / `F(args)[k]` on an Extern / ExportFunction / Custom function. The call becomes its node the
//        way every call does -- out arguments that are there are written back, absent ones are not -- and the value
//        is the node output the 1.x ordinal names. Two selections of one call make two equal nodes, which the dedupe
//        pass folds into one: 1.x's "same arguments, one node". A 1.x value call of a void function is ordinal 0.
//   L5   a legacy statement call's return value, stored into the argument that receives it.
//   L8   the `UE.` calls lifted out of a GraphFunction body, lowered at every call site with the function's own
//        parameters bound to that call's arguments; each is an input of the custom node, named FHoistedCall::InputName.
//   L9   a void `@custom` function's first `out` parameter is its node's primary output (the 1.x shape, every source).
//   L12  a loosely spelled 1.x enumerator, written as the catalog spells it.
//
// Diagnostics owned by this file: none. Every mistake these rules can meet was reported by the binder, and a lowering
// that finds nothing to lower stays silent (IRBuilderExpressions.cpp, the second rule).

#include "IRBuilderInternal.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/Char.h"

namespace UE::DreamShader::IR::Private
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace IRBuilderLegacyPrivate
	{
		/** The binder's L12 key (LangBinderLegacy.cpp): lower case, without spaces, tabs, `_`, `-`, `:`, `.` and `/`. */
		FString MakeIRBuilderLegacyEnumKey(const FString& Text)
		{
			FString Key;
			Key.Reserve(Text.Len());
			for (const TCHAR Char : Text)
			{
				if (Char == TCHAR(' ') || Char == TCHAR('\t') || Char == TCHAR('_') || Char == TCHAR('-')
					|| Char == TCHAR(':') || Char == TCHAR('.') || Char == TCHAR('/'))
				{
					continue;
				}
				Key.AppendChar(FChar::ToLower(Char));
			}
			return Key;
		}
	}

	bool FIRBuilder::IsLegacyFrame() const
	{
		if (Frames.IsEmpty())
		{
			return false;
		}
		const FFrame& Current = *Frames.Last();
		return Current.Function != nullptr && Current.Function->Decl != nullptr && Current.Function->Decl->bLegacy;
	}

	// ---------------------------------------------------------------------------------------------
	// L3b and L9: which output a selection reads
	// ---------------------------------------------------------------------------------------------

	int32 FIRBuilder::CustomPrimaryOutParam(const FBoundFunction& Callee)
	{
		if (Callee.Kind != EBoundFunctionKind::Custom || !Callee.ReturnType.IsVoid())
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::Out)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FIRBuilder::LegacySelectionSlot(const FBoundFunction& Callee, const int32 Ordinal)
	{
		// MakeCustomNode's layout: a void custom node without an `out` keeps a placeholder output 0. A FunctionCall node
		// lists its outputs in CollectCallOutputs order, which is the 1.x order.
		const bool bPlaceholderOutput = Callee.Kind == EBoundFunctionKind::Custom
			&& Callee.ReturnType.IsVoid()
			&& CustomPrimaryOutParam(Callee) == INDEX_NONE;
		return bPlaceholderOutput ? Ordinal + 1 : Ordinal;
	}

	FLoweredValue FIRBuilder::SelectCallOutput(const FBoundFunction& Callee, const int32 Ordinal, const FIRType& Type)
	{
		// No node: the call already reported, or made none; nothing is said twice.
		if (!Graph || !LastCallNode.IsValid() || !Graph->Nodes.IsValidIndex(LastCallNode.Node))
		{
			return FLoweredValue();
		}
		const int32 Slot = LegacySelectionSlot(Callee, Ordinal);
		if (!Graph->Nodes[LastCallNode.Node].Outputs.IsValidIndex(Slot))
		{
			// The binder counted the outputs from the same parameters CollectCallOutputs lists, so a slot past them is not
			// a case this can meet.
			return FLoweredValue();
		}
		return FLoweredValue::OfOutput(FIRValue{ LastCallNode.Node, Slot }, Type);
	}

	FLoweredValue FIRBuilder::LowerFunctionCallOutput(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		const FExpr* Object = nullptr;
		if (const FMemberExpr* Member = Expr.As<FMemberExpr>())
		{
			Object = Member->Object.Get();
		}
		else if (const FIndexExpr* Index = Expr.As<FIndexExpr>())
		{
			Object = Index->Object.Get();
		}

		const FExpr* CallExpr = Unparen(Object);
		const FBoundExpr* CallBinding = CallExpr ? Bound(*CallExpr) : nullptr;
		if (!CallBinding
			|| CallBinding->Kind != EBoundExprKind::FunctionCall
			|| !BoundModule.Functions.IsValidIndex(CallBinding->Index))
		{
			return FLoweredValue();
		}

		// The value LowerFunctionCall answers is the return value, or nothing for a void call; the selection reads the
		// node it made instead.
		LowerFunctionCall(*CallExpr, *CallBinding);
		return SelectCallOutput(BoundModule.Functions[CallBinding->Index], BoundExpr.FieldIndex, BoundExpr.Type);
	}

	// ---------------------------------------------------------------------------------------------
	// L5: the return value's receiver
	// ---------------------------------------------------------------------------------------------

	void FIRBuilder::WriteBackLegacyResultReceiver(const FExpr& Expr, const FBoundExpr& BoundExpr, const FBoundFunction& Callee, const FLoweredValue& Result)
	{
		if (Result.IsEmpty())
		{
			return;
		}
		for (const FBoundArgument& Argument : BoundExpr.Args)
		{
			if (Argument.TargetIndex != Callee.Params.Num())
			{
				continue;
			}
			const FExpr* Receiver = ArgumentExpr(Expr, Argument);
			FLValueRef Target;
			if (!Receiver || !ResolveLValue(*Receiver, Target))
			{
				continue;
			}
			// Legacy rule L22: fitted to the variable that receives it, as an `out` argument's value is (WriteBackOutputs).
			FLoweredValue Received = Result;
			const int32 TargetWidth = DeclaredTypeOf(Target).GraphComponentCount();
			if (Received.IsValue() && TargetWidth > 0 && Target.SwizzleMask.IsEmpty() && WidthOf(Received.Value) != TargetWidth)
			{
				Received = FLoweredValue::Of(CoerceToWidth(Received.Value, TargetWidth, Expr.Span));
			}
			StoreLValue(Target, Received, Expr.Span);
			RecordStatementBinding(DescribeLValueRef(Target), BoundValueOfLValueRef(Target), Expr.Span);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// L8: the calls lifted out of a GraphFunction body
	// ---------------------------------------------------------------------------------------------

	bool FIRBuilder::LowerHoistedCallInputs(
		const FBoundFunction& Callee,
		const int32 CalleeIndex,
		const TArray<FLoweredValue>& Arguments,
		const FLangSpan& Span,
		TArray<FIRInput>& OutInputs)
	{
		if (!Callee.Decl || Callee.Decl->HoistedCalls.Num() == 0)
		{
			return true;
		}
		// A function whose lifted calls reach itself was refused by the binder (DSH6330); the stack is the guarantee that
		// nothing reaching here anyway lowers forever.
		if (Callee.bRecursive || HoistingStack.Contains(CalleeIndex))
		{
			return false;
		}

		HoistingStack.Push(CalleeIndex);

		// The binder bound the lifted calls in the function's own scope -- its parameters, the file's globals, literals --
		// so they are lowered in a frame of the function's own, its parameters holding this call's arguments.
		PushFrame(Callee, CalleeIndex);
		Frame().CallSite = Span;
		Frame().bHasCallSite = true;
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::Out || !Arguments.IsValidIndex(Index))
			{
				continue;
			}
			const FLoweredValue& Argument = Arguments[Index];
			if (Argument.IsValue())
			{
				const int32 Width = Callee.Params[Index].Type.GraphComponentCount();
				Frame().Params[Index] = FLoweredValue::Of(Width > 0 ? CoerceToWidth(Argument.Value, Width, Span) : Argument.Value);
			}
			else
			{
				Frame().Params[Index] = Argument;
			}
		}

		for (const FHoistedCall& Hoisted : Callee.Decl->HoistedCalls)
		{
			if (!Hoisted.Call)
			{
				continue;
			}
			const FBoundExpr* BoundCall = Bound(*Hoisted.Call);
			if (!BoundCall || BoundCall->Type.IsError())
			{
				continue;
			}
			// LowerOperand applies the conversion the binder recorded: a node with several outputs feeds its default one.
			const FIRValue Value = LowerOperand(*Hoisted.Call, 0);
			if (Value.IsValid())
			{
				OutInputs.Add({ Hoisted.InputName, Value });
			}
		}

		PopFrame();
		HoistingStack.Pop();
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// L12: enumerators
	// ---------------------------------------------------------------------------------------------

	void FIRBuilder::CanonicaliseLegacyEnumerator(const FCatalogProperty& Property, FIRPropertyValue& InOutValue)
	{
		using namespace IRBuilderLegacyPrivate;

		if (InOutValue.Kind != EIRPropertyKind::Enum || Property.EnumValues.IsEmpty())
		{
			return;
		}
		for (const FString& Enumerator : Property.EnumValues)
		{
			if (Enumerator.Equals(InOutValue.S, ESearchCase::CaseSensitive))
			{
				return;
			}
		}

		// The binder's candidates (TryMatchLegacyEnumerator): the value whole, after an `Enum::` scope, after its own prefix.
		TArray<FString> Keys;
		FString Rest = InOutValue.S.TrimStartAndEnd();
		Keys.Add(MakeIRBuilderLegacyEnumKey(Rest));
		const int32 Scope = Rest.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Scope != INDEX_NONE)
		{
			Rest = Rest.Mid(Scope + 2);
			Keys.Add(MakeIRBuilderLegacyEnumKey(Rest));
		}
		const int32 Prefix = Rest.Find(TEXT("_"), ESearchCase::CaseSensitive);
		if (Prefix != INDEX_NONE)
		{
			Keys.Add(MakeIRBuilderLegacyEnumKey(Rest.Mid(Prefix + 1)));
		}

		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Property.EnumValues.Num(); ++Index)
		{
			const FString Key = MakeIRBuilderLegacyEnumKey(Property.EnumValues[Index]);
			const bool bMatches = !Key.IsEmpty() && Keys.ContainsByPredicate([&Key](const FString& Candidate)
			{
				return Candidate.Equals(Key, ESearchCase::CaseSensitive);
			});
			if (bMatches)
			{
				if (Found != INDEX_NONE)
				{
					return;
				}
				Found = Index;
			}
		}
		if (Found != INDEX_NONE)
		{
			InOutValue.S = Property.EnumValues[Found];
		}
	}
}
