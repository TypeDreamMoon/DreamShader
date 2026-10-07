// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Calls. Four kinds, decided by the binder, and each one a different shape in the graph:
//
//   Helper          inlined -- a fresh frame, the body lowered into the caller's graph, `return`
//                   folded, `out` parameters written back through the caller's lvalues
//   Custom          one UMaterialExpressionCustom whose HLSL comes from the custom-HLSL builder
//   ExportFunction  a FunctionCall to the product this file also produces (Prop::LocalFunction)
//   Extern          a FunctionCall to the asset `/// @asset` names (Prop::FunctionPath)
//
// A frame boundary is also a control-flow boundary: the callee's `return` is relative to the
// callee's own body, so the caller's condition stack, loop depth and branch depth are put aside
// while the body is lowered and restored afterwards. Without that a helper called inside an `if`
// would think its own `return` was conditional.
//
// The legacy rules reach calls here -- a statement call's return-value receiver (L5), the calls lifted out of
// a GraphFunction body (L8), a legacy void custom function's primary output (L9) and the node an output selection reads
// (L3b, LastCallNode) -- with their lowering in IRBuilderLegacy.cpp.
//
// Diagnostics owned by this file: DSH6220, DSH6221, DSH6222, DSH6223, DSH6224.

#include "IRBuilderInternal.h"

#include "IR/IRCustomHlsl.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.IRBuilder"

namespace UE::DreamShader::IR::Private
{
	/** "Float1".."Float4" / "MaterialAttributes": what a Custom node's OutputType property says. */
	static FString CustomOutputTypeName(const FIRType& Type)
	{
		if (Type.IsMaterial())
		{
			return TEXT("MaterialAttributes");
		}
		const int32 Width = FMath::Clamp(Type.GraphComponentCount(), 1, 4);
		return FString::Printf(TEXT("Float%d"), Width);
	}

	FString FIRBuilder::CallPinName(const FBoundParam& Param, bool bEnginePinNames)
	{
		return (bEnginePinNames && !Param.PinName.IsEmpty()) ? Param.PinName : Param.Name;
	}

	void FIRBuilder::CollectCallOutputs(const FBoundFunction& Callee, TArray<FString>& OutNames, TArray<FIRType>& OutTypes, bool bEnginePinNames)
	{
		// The order a product's FunctionOutputs are made in, so a call's output names line up with
		// the asset's outputs without either side having to look at the other.
		if (!Callee.ReturnType.IsVoid())
		{
			const FString* ResultPin = bEnginePinNames ? Callee.Directives.FindPinName(TEXT("Result")) : nullptr;
			OutNames.Add(ResultPin ? *ResultPin : FString(TEXT("Result")));
			OutTypes.Add(Callee.ReturnType);
		}
		for (const FBoundParam& Param : Callee.Params)
		{
			if (Param.Direction != EParamDirection::In)
			{
				OutNames.Add(CallPinName(Param, bEnginePinNames));
				OutTypes.Add(Param.Type);
			}
		}
	}

	static void ConstrainAssignmentPaths(FLoweredValue& Value, FIRValue Condition, bool bHolds)
	{
		if (Value.AssignedWhen)
		{
			const FDefaultArgumentState State = Value.AssignedWhen->InArm(Condition, bHolds);
			Value.AssignedWhen = MakeShared<FDefaultArgumentState>(State);
			if (State.State == EDefaultArgumentState::Ready)
			{
				Value.bPartiallyAssigned = false;
			}
			// A Pending leaf still represents a missing value, even though the SSA cache holds
			// the other arm's value. Keep its diagnostic until a write supplies this arm too.
		}
		for (FLoweredValue& Field : Value.Fields)
		{
			ConstrainAssignmentPaths(Field, Condition, bHolds);
		}
		for (TPair<FString, TSharedPtr<const FDefaultArgumentState>>& Attribute : Value.Material.AssignedWhen)
		{
			const FDefaultArgumentState State = Attribute.Value->InArm(Condition, bHolds);
			Attribute.Value = MakeShared<FDefaultArgumentState>(State);
			if (State.State == EDefaultArgumentState::Ready)
			{
				const int32 Partial = Value.Material.FindPartialAttribute(Attribute.Key);
				if (Partial != INDEX_NONE) { Value.Material.PartialAttributes.RemoveAt(Partial); }
			}
		}
	}

	void FIRBuilder::ConstrainDefaultArguments(FIRValue Condition, bool bHolds)
	{
		if (!Frames.ContainsByPredicate([](const TUniquePtr<FFrame>& Each) { return !Each->DefaultArguments.IsEmpty(); }))
		{
			return;
		}
		for (const TUniquePtr<FFrame>& Each : Frames)
		{
			for (FDefaultArgumentState& State : Each->DefaultArguments)
			{
				State = State.InArm(Condition, bHolds);
			}
			for (FLoweredValue& Value : Each->Locals) { ConstrainAssignmentPaths(Value, Condition, bHolds); }
			for (FLoweredValue& Value : Each->Params) { ConstrainAssignmentPaths(Value, Condition, bHolds); }
		}
	}

	bool FIRBuilder::ResolveDefaultArgument(int32 ParamIndex)
	{
		FFrame& Current = Frame();
		if (!Current.DefaultArguments.IsValidIndex(ParamIndex)
			|| Current.DefaultArguments[ParamIndex].State == EDefaultArgumentState::Ready)
		{
			return true;
		}
		const FBoundParam& Param = Current.Function->Params[ParamIndex];
		const FDefaultArgumentState State = Current.DefaultArguments[ParamIndex];
		if (State.State == EDefaultArgumentState::Conditional)
		{
			// Do not initialize a missing arm at the earlier join: the enclosing default may
			// still be evaluating, and this default is allowed to depend on its completed value.
			const FEnvSnapshot Before = Snapshot();
			ConstrainDefaultArguments(State.Condition, true);
			ConditionStack.Push({ State.Condition, State.bStaticCondition, false });
			const bool bTrueResolved = ResolveDefaultArgument(ParamIndex);
			ConditionStack.Pop();
			const FEnvSnapshot TrueState = Snapshot();

			Restore(Before);
			ConstrainDefaultArguments(State.Condition, false);
			ConditionStack.Push({ State.Condition, State.bStaticCondition, true });
			const bool bFalseResolved = ResolveDefaultArgument(ParamIndex);
			ConditionStack.Pop();
			const FEnvSnapshot FalseState = Snapshot();
			MergeStates(TrueState, FalseState, State.Condition, State.bStaticCondition, Param.Default->Span);
			return bTrueResolved && bFalseResolved;
		}
		if (State.State == EDefaultArgumentState::Evaluating)
		{
			Diagnostics.Error(TEXT("DSH6224"), Param.Default->Span, FText::Format(
				LOCTEXT("IRBuilderDefaultCycle", "The default value of '{0}' in '{1}' depends on itself; pass an explicit value for a parameter in the cycle."),
				FText::FromString(Param.Name), FText::FromString(Current.Function->Name)));
			return false;
		}

		Current.DefaultArguments[ParamIndex] = EDefaultArgumentState::Evaluating;
		const int32 ErrorsBefore = Diagnostics.NumErrors();
		FLoweredValue Value = LowerExpr(*Param.Default);
		const int32 Width = Param.Type.GraphComponentCount();
		if (Value.IsValue() && Width > 0)
		{
			Value = FLoweredValue::Of(CoerceToWidth(Value.Value, Width, Param.Default->Span));
		}
		Current.Params[ParamIndex] = Value;
		Current.DefaultArguments[ParamIndex] = EDefaultArgumentState::Ready;
		if (Value.IsEmpty() && Diagnostics.NumErrors() == ErrorsBefore)
		{
			Diagnostics.Error(TEXT("DSH6224"), Param.Default->Span, FText::Format(
				LOCTEXT("IRBuilderDefaultHasNoValue", "The default value of '{0}' in '{1}' reads a parameter that has no value; pass that input explicitly or initialize its default."),
				FText::FromString(Param.Name), FText::FromString(Current.Function->Name)));
		}
		return !Value.IsEmpty();
	}

	bool FIRBuilder::BindCallArguments(
		const FExpr& Expr,
		const FBoundExpr& BoundExpr,
		const FBoundFunction& Callee,
		TArray<FLoweredValue>& OutValues,
		TArray<FLValueRef>& OutTargets)
	{
		OutValues.SetNum(Callee.Params.Num());
		OutTargets.SetNum(Callee.Params.Num());

		TArray<bool> bBound;
		bBound.Init(false, Callee.Params.Num());

		for (const FBoundArgument& Argument : BoundExpr.Args)
		{
			int32 ParamIndex = Argument.TargetIndex;
			if (ParamIndex == Callee.Params.Num())
			{
				// A legacy statement call's return-value receiver (L5): stored once the node exists.
				continue;
			}
			if (!Callee.Params.IsValidIndex(ParamIndex))
			{
				ParamIndex = INDEX_NONE;
				for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
				{
					if (Callee.Params[Index].Name.Equals(Argument.Target, ESearchCase::CaseSensitive))
					{
						ParamIndex = Index;
						break;
					}
				}
			}
			if (!Callee.Params.IsValidIndex(ParamIndex))
			{
				continue;
			}

			const FExpr* Value = ArgumentExpr(Expr, Argument);
			if (!Value)
			{
				continue;
			}

			const FBoundParam& Param = Callee.Params[ParamIndex];
			bBound[ParamIndex] = true;

			if (Param.Direction != EParamDirection::In)
			{
				if (!ResolveLValue(*Value, OutTargets[ParamIndex]))
				{
					if (OutTargets[ParamIndex].bNestedAttribute)
					{
						// Assignable to the binder, but a write into the material held in an attribute
						// has no lowering; saying "not assignable" would send the author the wrong way.
						ReportNestedAttributeWrite(*Value);
						return false;
					}
					Diagnostics.Error(TEXT("DSH6222"), Value->Span, FText::Format(
						LOCTEXT("IRBuilderOutArgNotLValue", "'{0}' is an '{1}' parameter of {2}, so the argument has to be something that can be assigned to; this expression cannot."),
						FText::FromString(Param.Name),
						FText::FromString(Param.Direction == EParamDirection::Out ? TEXT("out") : TEXT("inout")),
						FText::FromString(Callee.Name)));
					return false;
				}
			}

			if (Param.Direction != EParamDirection::Out)
			{
				// An `inout` argument is copied in, but the callee may only ever write it, so a local
				// nothing has assigned yet is not a mistake the author made HERE: DSH4376 stands down for
				// the copy, and the never-assigned state travels in and back out instead.
				const bool bInOut = Param.Direction == EParamDirection::InOut;
				InOutArgumentDepth += bInOut ? 1 : 0;
				OutValues[ParamIndex] = LowerExpr(*Value);
				InOutArgumentDepth -= bInOut ? 1 : 0;
			}
		}

		// A material-function asset owns its defaults: an omitted pin must stay unconnected.
		// In particular, do not execute a prototype's default expression in the caller.
		if (Callee.Kind != EBoundFunctionKind::Helper && Callee.Kind != EBoundFunctionKind::Custom)
		{
			return true;
		}
		bool bHasDefaults = false;
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			bHasDefaults |= !bBound[Index] && Callee.Params[Index].Default != nullptr;
		}
		if (!bHasDefaults)
		{
			return true;
		}

		// Custom bodies are emitted once, but their omitted inputs still expand in the graph.
		// A recursive default therefore needs the same finite call boundary as a helper body.
		for (const TUniquePtr<FFrame>& Active : Frames)
		{
			if (Active->FunctionIndex == BoundExpr.Index && !Active->DefaultArguments.IsEmpty())
			{
				Diagnostics.Error(TEXT("DSH6224"), Expr.Span, FText::Format(
					LOCTEXT("IRBuilderDefaultCallCycle", "Resolving the defaults of '{0}' calls that function with omitted arguments again; pass explicit values to break the default call cycle."),
					FText::FromString(Callee.Name)));
				return false;
			}
		}
		if (Frames.Num() >= Options.MaxInlineDepth)
		{
			Diagnostics.Error(TEXT("DSH6221"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderDefaultDepth", "Resolving the defaults of '{0}' would go {1} calls deep, past the limit of {2}; pass explicit values or flatten the default call chain."),
				FText::FromString(Callee.Name), FText::AsNumber(Frames.Num() + 1), FText::AsNumber(Options.MaxInlineDepth)));
			return false;
		}

		// Explicit arguments and lvalue targets above belong to the caller. The binder resolves
		// names in defaults against the callee's parameters, so install those values in a temporary
		// callee frame before lowering defaults. Reads resolve dependent defaults on demand.
		PushFrame(Callee, BoundExpr.Index);
		Frame().CallSite = Expr.Span;
		Frame().bHasCallSite = true;
		Frame().Params = OutValues;
		Frame().DefaultArguments.Init(EDefaultArgumentState::Ready, Callee.Params.Num());
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			FLoweredValue& Value = Frame().Params[Index];
			const int32 Width = Callee.Params[Index].Type.GraphComponentCount();
			if (Value.IsValue() && Width > 0)
			{
				Value = FLoweredValue::Of(CoerceToWidth(Value.Value, Width, Expr.Span));
			}
			if (!bBound[Index] && Callee.Params[Index].Default)
			{
				Frame().DefaultArguments[Index] = EDefaultArgumentState::Pending;
			}
		}
		bool bResolved = true;
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			bResolved = ResolveDefaultArgument(Index) && bResolved;
		}
		OutValues = Frame().Params;
		PopFrame();
		if (Callee.Kind == EBoundFunctionKind::Custom)
		{
			for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
			{
				if (Callee.Params[Index].Direction == EParamDirection::Out && !OutValues[Index].IsEmpty())
				{
					Diagnostics.Error(TEXT("DSH6224"), Expr.Span, FText::Format(
						LOCTEXT("IRBuilderCustomDefaultOut", "A default of '{0}' initializes its 'out' parameter '{1}', but a Custom output has no input pin to carry that value; pass the initial value through a separate input and assign the output in the body."),
						FText::FromString(Callee.Name), FText::FromString(Callee.Params[Index].Name)));
					bResolved = false;
				}
			}
		}
		return bResolved;
	}

	void FIRBuilder::WriteBackOutputs(
		const FBoundFunction& Callee,
		const TArray<FLValueRef>& Targets,
		FIRValue Node,
		const TArray<FString>& OutputNames,
		const FLangSpan& Span,
		bool bEnginePinNames)
	{
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::In || !Targets.IsValidIndex(Index) || !Targets[Index].IsValid())
			{
				continue;
			}
			// Case-sensitive: two outputs may differ only in case, and IndexOfByKey on FString would not see it.
			const FString PinName = CallPinName(Callee.Params[Index], bEnginePinNames);
			const int32 OutputIndex = OutputNames.IndexOfByPredicate([&PinName](const FString& Name)
			{
				return Name.Equals(PinName, ESearchCase::CaseSensitive);
			});
			if (OutputIndex == INDEX_NONE)
			{
				continue;
			}
			// An `out material` comes back as a MaterialAttributes output: the caller's material is
			// replaced by that set, which it can go on reading and writing like any other material.
			FLoweredValue Returned = FLoweredValue::OfOutput(FIRValue{ Node.Node, OutputIndex }, Callee.Params[Index].Type);
			// Legacy rule L22: what comes back is fitted to the variable that receives it. 2.0 asks for an exact match, so
			// the widths differ in a 1.x body only.
			const int32 TargetWidth = DeclaredTypeOf(Targets[Index]).GraphComponentCount();
			if (Returned.IsValue() && TargetWidth > 0 && Targets[Index].SwizzleMask.IsEmpty() && WidthOf(Returned.Value) != TargetWidth)
			{
				Returned = FLoweredValue::Of(CoerceToWidth(Returned.Value, TargetWidth, Span));
			}
			StoreLValue(Targets[Index], Returned, Span);
			// The call statement bound this variable too, as a side effect.
			RecordStatementBinding(DescribeLValueRef(Targets[Index]), BoundValueOfLValueRef(Targets[Index]), Span);
		}
	}

	// -------------------------------------------------------------------------------- dispatch

	FLoweredValue FIRBuilder::LowerFunctionCall(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		if (!BoundModule.Functions.IsValidIndex(BoundExpr.Index))
		{
			return FLoweredValue();
		}

		const FBoundFunction& Callee = BoundModule.Functions[BoundExpr.Index];
		LastCallNode = FIRValue::None();

		FLoweredValue Result;
		switch (Callee.Kind)
		{
		case EBoundFunctionKind::Helper:
			return InlineHelper(Expr, BoundExpr, Callee, BoundExpr.Index);

		case EBoundFunctionKind::Custom:
			Result = MakeCustomNode(Expr, BoundExpr, Callee, BoundExpr.Index);
			break;

		case EBoundFunctionKind::ExportFunction:
		case EBoundFunctionKind::Layer:
		case EBoundFunctionKind::LayerBlend:
		case EBoundFunctionKind::Extern:
			Result = MakeFunctionCallNode(Expr, BoundExpr, Callee, BoundExpr.Index);
			break;

		case EBoundFunctionKind::Entry:
		default:
			Diagnostics.Error(TEXT("DSH6223"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderCallEntry", "'{0}' is this file's material entry and is called by the engine, not by the shader."),
				FText::FromString(Callee.Name)));
			return FLoweredValue();
		}

		// Legacy rule L5: a 1.x statement call hands its return value to the argument that receives it.
		WriteBackLegacyResultReceiver(Expr, BoundExpr, Callee, Result);

		// Legacy rule L3b: a 1.x value call of a function that returns nothing is its first output, which is what the
		// binder typed the call as.
		if (Callee.ReturnType.IsVoid() && !BoundExpr.Type.IsVoid() && !BoundExpr.Type.IsError())
		{
			return SelectCallOutput(Callee, 0, BoundExpr.Type);
		}
		return Result;
	}

	// --------------------------------------------------------------------------------- inlining

	FLoweredValue FIRBuilder::InlineHelper(const FExpr& Expr, const FBoundExpr& BoundExpr, const FBoundFunction& Callee, int32 CalleeIndex)
	{
		if (Callee.bRecursive)
		{
			Diagnostics.Error(TEXT("DSH6220"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderRecursion", "'{0}' calls itself, and an inlined function has no stack to recurse on; rewrite it as a loop with a constant trip count, or as a '/// @custom' function."),
				FText::FromString(Callee.Name)));
			return FLoweredValue();
		}
		for (const TUniquePtr<FFrame>& Active : Frames)
		{
			if (Active->FunctionIndex == CalleeIndex)
			{
				Diagnostics.Error(TEXT("DSH6220"), Expr.Span, FText::Format(
					LOCTEXT("IRBuilderRecursionCycle", "'{0}' is already being inlined further up this call chain; an inlined function cannot call back into itself."),
					FText::FromString(Callee.Name)));
				return FLoweredValue();
			}
		}
		if (Frames.Num() >= Options.MaxInlineDepth)
		{
			Diagnostics.Error(TEXT("DSH6221"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderInlineDepth", "Inlining '{0}' would go {1} calls deep, past the limit of {2}; flatten the call chain or move part of it into a '/// @custom' function."),
				FText::FromString(Callee.Name),
				FText::AsNumber(Frames.Num() + 1),
				FText::AsNumber(Options.MaxInlineDepth)));
			return FLoweredValue();
		}
		if (!Callee.Decl || !Callee.Decl->Body)
		{
			Diagnostics.Error(TEXT("DSH6223"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderNoBody", "'{0}' has no body to inline; give it one, mark it 'extern' with '/// @asset', or '/// @custom'."),
				FText::FromString(Callee.Name)));
			return FLoweredValue();
		}

		TArray<FLoweredValue> Arguments;
		TArray<FLValueRef> Targets;
		if (!BindCallArguments(Expr, BoundExpr, Callee, Arguments, Targets))
		{
			return FLoweredValue();
		}

		PushFrame(Callee, CalleeIndex);
		Frame().CallSite = Expr.Span;
		Frame().bHasCallSite = true;
		Frame().OutTargets = Targets;

		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			// Explicit out arguments start empty, but another parameter's default may have assigned
			// this slot already. Carry that value into the body just like any other callee state.
			if (!Arguments.IsValidIndex(Index))
			{
				continue;
			}
			const FLoweredValue& Argument = Arguments[Index];
			if (Argument.IsValue())
			{
				const int32 Width = Callee.Params[Index].Type.GraphComponentCount();
				Frame().Params[Index] = FLoweredValue::Of(Width > 0 ? CoerceToWidth(Argument.Value, Width, Expr.Span) : Argument.Value);
			}
			else if (!Argument.IsEmpty() || Argument.IsNeverAssigned())
			{
				// A struct or a material passes by value: the callee gets its own copy of the map,
				// and an `inout` one is written back below. So does a local nothing had assigned yet,
				// handed to an `inout`: if the callee never writes it, it comes back never assigned and a
				// later read in the caller is still DSH4376.
				Frame().Params[Index] = Argument;
			}
		}

		// The callee's own control flow starts clean: its `return` is unconditional at the top of its
		// body however deep inside an `if` the call sits.
		TArray<FConditionEntry> SavedConditions = MoveTemp(ConditionStack);
		ConditionStack.Reset();
		const int32 SavedBranchDepth = BranchDepth;
		const int32 SavedLoopDepth = LoopDepth;
		const bool bSavedBreak = bBreak;
		const bool bSavedContinue = bContinue;
		BranchDepth = 0;
		LoopDepth = 0;
		bBreak = false;
		bContinue = false;

		LowerBlock(*Callee.Decl->Body);
		ResolveExits();

		ConditionStack = MoveTemp(SavedConditions);
		BranchDepth = SavedBranchDepth;
		LoopDepth = SavedLoopDepth;
		bBreak = bSavedBreak;
		bContinue = bSavedContinue;

		FLoweredValue Result = Frame().ReturnValue;
		const TArray<FLoweredValue> FinalParams = Frame().Params;
		PopFrame();

		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::In
				|| !Targets.IsValidIndex(Index)
				|| !Targets[Index].IsValid()
				|| !FinalParams.IsValidIndex(Index))
			{
				continue;
			}
			StoreLValue(Targets[Index], FinalParams[Index], Expr.Span);
			RecordStatementBinding(DescribeLValueRef(Targets[Index]), BoundValueOfLValueRef(Targets[Index]), Expr.Span);
		}

		if (Result.IsValue())
		{
			const int32 Width = Callee.ReturnType.GraphComponentCount();
			if (Width > 0)
			{
				Result = FLoweredValue::Of(CoerceToWidth(Result.Value, Width, Expr.Span));
			}
		}
		return Result;
	}

	// ---------------------------------------------------------------------------- custom nodes

	FLoweredValue FIRBuilder::MakeCustomNode(const FExpr& Expr, const FBoundExpr& BoundExpr, const FBoundFunction& Callee, int32 CalleeIndex)
	{
		// The custom-HLSL builder is asked first, because it is the one that refuses a body that cannot become a
		// Custom node at all -- a `material` parameter among them (DSH6252:
		// the 5.8 translator has no MaterialAttributes case for a custom INPUT pin). When it
		// says no, there is nothing left to lower and it has already reported why.
		FCustomNodeCode Code;
		// The stamper names the file in the code's Begin/End markers, so the node code -- and
		// with it the shader keys -- is the same on every machine.
		if (!BuildDreamShaderCustomNodeCode(BoundModule, CalleeIndex, Code, Diagnostics, Options.StampSourcePath))
		{
			return FLoweredValue();
		}

		TArray<FLoweredValue> Arguments;
		TArray<FLValueRef> Targets;
		const bool bArgumentsBound = BindCallArguments(Expr, BoundExpr, Callee, Arguments, Targets);

		// Legacy rule L8: the `UE.` calls lifted out of the body, lowered with this call's arguments; each becomes an input
		// of the node, under the name the custom-HLSL builder put in the code where the call was.
		TArray<FIRInput> HoistedInputs;
		const bool bHoistedLowered = bArgumentsBound && LowerHoistedCallInputs(Callee, CalleeIndex, Arguments, Expr.Span, HoistedInputs);

		// Calls among the arguments made nodes of their own; the node an output selection reads is the one made below.
		LastCallNode = FIRValue::None();
		if (!bHoistedLowered)
		{
			return FLoweredValue();
		}

		FIRNode Node;
		Node.Op = EIROp::Custom;
		Node.ClassName = Callee.Name;
		Node.Properties.Add({ FString(Prop::Code), FIRPropertyValue::MakeString(Code.Code) });
		// The node says which function it is: its `/// @name`, else its own name. A 1.x function of a Namespace block
		// says it the way 1.x did, `BL::TexNoise1D`.
		const bool bLegacyQualified = Callee.Decl != nullptr && !Callee.Decl->LegacyQualifiedName.IsEmpty();
		Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(
			bLegacyQualified ? Callee.Decl->LegacyQualifiedName
			: !Callee.Directives.Name.IsEmpty() ? Callee.Directives.Name
			: Callee.Name) });
		if (!Code.IncludeFilePaths.IsEmpty())
		{
			Node.Properties.Add({ FString(Prop::IncludeFilePaths), FIRPropertyValue::MakeStringList(Code.IncludeFilePaths) });
		}

		// A void `/// @custom` function still has to hand the graph one output, because the engine's
		// Custom node always has one. It is the function's first `out` parameter, which is the node 1.x
		// made of such a function (rule L9) and the one a migrated file has to keep making; a function
		// with no `out` at all returns a Float1 that the custom-HLSL builder's EnsureTopLevelReturn gives the body.
		const int32 PrimaryOut = CustomPrimaryOutParam(Callee);
		Node.Properties.Add({ FString(Prop::OutputType), FIRPropertyValue::MakeEnum(
			PrimaryOut != INDEX_NONE ? CustomOutputTypeName(Callee.Params[PrimaryOut].Type)
			: Callee.ReturnType.IsVoid() ? FString(TEXT("Float1"))
			: CustomOutputTypeName(Callee.ReturnType)) });

		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::Out || !Arguments.IsValidIndex(Index))
			{
				continue;
			}
			// Belt and braces for section 6.13 #1: a Custom input pin never carries attributes, so
			// nothing here may build MakeMaterialAttributes. The custom-HLSL builder refuses such a function above,
			// which is where the diagnostic comes from; this is only the guarantee that no other
			// path can quietly synthesise the node the translator would choke on.
			if (Callee.Params[Index].Type.IsMaterial() || Arguments[Index].IsMaterial())
			{
				continue;
			}
			const FIRValue Value = ValueForParam(Arguments[Index], Callee.Params[Index].Type, EIRConversion::Identity, Expr.Span);
			if (Value.IsValid())
			{
				Node.Inputs.Add({ Callee.Params[Index].Name, Value });
			}
		}
		Node.Inputs.Append(HoistedInputs);

		TArray<FString> OutputNames;
		TArray<FIRType> OutputTypes;
		// A Custom node's pins are the HLSL identifiers its code declares: never the `@pin` names.
		CollectCallOutputs(Callee, OutputNames, OutputTypes, /* bEnginePinNames */ false);

		// Output 0 is the return value -- or, under L9, the first `out` parameter, which CollectCallOutputs lists first
		// too. Everything after it is an AdditionalOutputs entry, spelled "Name:Type" so the emitter can rebuild the
		// node's pins without consulting anything else. LegacySelectionSlot reads this layout.
		const bool bPlaceholderOutput = OutputNames.IsEmpty() || (Callee.ReturnType.IsVoid() && PrimaryOut == INDEX_NONE);
		Node.Outputs.Add(bPlaceholderOutput ? FIRType::Float(1) : GraphTypeOf(OutputTypes[0]));
		Node.OutputNames.Add(bPlaceholderOutput ? FString(TEXT("Result")) : OutputNames[0]);

		TArray<FString> Additional;
		const int32 FirstOutParam = bPlaceholderOutput ? 0 : 1;
		for (int32 Index = FirstOutParam; Index < OutputNames.Num(); ++Index)
		{
			Node.Outputs.Add(GraphTypeOf(OutputTypes[Index]));
			Node.OutputNames.Add(OutputNames[Index]);
			Additional.Add(FString::Printf(TEXT("%s:%s"), *OutputNames[Index], *CustomOutputTypeName(OutputTypes[Index])));
		}
		if (!Additional.IsEmpty())
		{
			Node.Properties.Add({ FString(Prop::AdditionalOutputs), FIRPropertyValue::MakeStringList(Additional) });
		}

		const FIRValue AddedNode = AddNode(MoveTemp(Node), Expr.Span);
		LastCallNode = AddedNode;

		// Copied out before the write-back, which makes nodes of its own and may move the array.
		TArray<FString> NodeOutputNames = Graph->Nodes[AddedNode.Node].OutputNames;
		WriteBackOutputs(Callee, Targets, AddedNode, NodeOutputNames, Expr.Span, /* bEnginePinNames */ false);

		// A `material` return (OutputType MaterialAttributes) is a material that arrived through a pin.
		return Callee.ReturnType.IsVoid() ? FLoweredValue() : FLoweredValue::OfOutput(AddedNode, Callee.ReturnType);
	}

	// -------------------------------------------------------------------------- function calls

	FLoweredValue FIRBuilder::MakeFunctionCallNode(const FExpr& Expr, const FBoundExpr& BoundExpr, const FBoundFunction& Callee, int32 CalleeIndex)
	{
		TArray<FLoweredValue> Arguments;
		TArray<FLValueRef> Targets;
		const bool bArgumentsBound = BindCallArguments(Expr, BoundExpr, Callee, Arguments, Targets);
		// Calls among the arguments made nodes of their own; the node an output selection reads is the one made below.
		LastCallNode = FIRValue::None();
		if (!bArgumentsBound)
		{
			return FLoweredValue();
		}

		FIRNode Node;
		Node.Op = EIROp::FunctionCall;

		if (const int32* ProductIndex = ProductByFunction.Find(CalleeIndex))
		{
			// A call to an export of this same file. There is no asset path yet -- the pipeline
			// compiles the products in dependency order and the emitter fills the reference in --
			// so the product index is the whole of the reference and ClassName is only a name.
			Node.ClassName = Callee.Name;
			Node.Properties.Add({ FString(Prop::LocalFunction), FIRPropertyValue::MakeInt(*ProductIndex) });
			Node.Properties.Add({ FString(Prop::FunctionPath), FIRPropertyValue::MakeObject(FString()) });
		}
		else
		{
			const FString AssetPath = Callee.Directives.Asset;
			Node.ClassName = AssetPath;
			Node.Properties.Add({ FString(Prop::FunctionPath), FIRPropertyValue::MakeObject(AssetPath) });

			// A 1.x VirtualFunction's inputs in its own order. An asset is somebody else's: its pin may be `Alpha Threshold`
			// where the declaration can only say `Alpha_Threshold`, and 1.x then went by where the input stands (name first,
			// then the declared position). A 2.0 `extern` says such a pin with `/// @pin` and needs no second guess.
			if (Callee.Decl != nullptr && Callee.Decl->bLegacy)
			{
				TArray<FString> DeclaredInputs;
				for (const FBoundParam& Param : Callee.Params)
				{
					if (Param.Direction != EParamDirection::Out)
					{
						DeclaredInputs.Add(CallPinName(Param, /* bEnginePinNames */ true));
					}
				}
				Node.Properties.Add({ FString(Prop::DeclaredInputs), FIRPropertyValue::MakeStringList(DeclaredInputs) });
			}
		}

		// An input the call does not pass stays UNCONNECTED, and the asset's own default stands in for it -- which is what
		// the engine means by an optional input, what 1.x built, and what the decompiler reads back as an argument left
		// out. BindCallArguments skips omitted defaults for this kind of call: a prototype cannot override its asset.
		TArray<bool> bPassed;
		bPassed.Init(false, Callee.Params.Num());
		for (const FBoundArgument& Passed : BoundExpr.Args)
		{
			if (bPassed.IsValidIndex(Passed.TargetIndex))
			{
				bPassed[Passed.TargetIndex] = true;
			}
		}

		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::Out || !Arguments.IsValidIndex(Index) || !bPassed[Index])
			{
				continue;
			}
			const FIRValue Value = ValueForParam(Arguments[Index], Callee.Params[Index].Type, EIRConversion::Identity, Expr.Span);
			if (Value.IsValid())
			{
				// The called asset's FunctionInput is named by `@pin` when it has one.
				Node.Inputs.Add({ CallPinName(Callee.Params[Index], /* bEnginePinNames */ true), Value });
			}
		}

		TArray<FString> OutputNames;
		TArray<FIRType> OutputTypes;
		CollectCallOutputs(Callee, OutputNames, OutputTypes, /* bEnginePinNames */ true);
		for (int32 Index = 0; Index < OutputNames.Num(); ++Index)
		{
			Node.Outputs.Add(GraphTypeOf(OutputTypes[Index]));
			Node.OutputNames.Add(OutputNames[Index]);
		}
		if (Node.Outputs.IsEmpty())
		{
			// A function that returns nothing and has no `out` parameter still needs one output for
			// the value form; it is never read.
			Node.Outputs.Add(FIRType::Float(1));
			Node.OutputNames.Add(TEXT("Result"));
		}

		const FIRValue Result = AddNode(MoveTemp(Node), Expr.Span);
		LastCallNode = Result;
		WriteBackOutputs(Callee, Targets, Result, OutputNames, Expr.Span, /* bEnginePinNames */ true);

		// A `material` result is a material that arrived through a pin: `MF_Layer(m).Roughness` and
		// `m = MF_Layer(m);` both need the map with that output as its source, not a bare value.
		return Callee.ReturnType.IsVoid() ? FLoweredValue() : FLoweredValue::OfOutput(Result, Callee.ReturnType);
	}
}

#undef LOCTEXT_NAMESPACE
