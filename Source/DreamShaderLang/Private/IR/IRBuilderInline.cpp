// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Calls. Four kinds, decided by the binder, and each one a different shape in the graph:
//
//   Helper          inlined -- a fresh frame, the body lowered into the caller's graph, `return`
//                   folded, `out` parameters written back through the caller's lvalues
//   Custom          one UMaterialExpressionCustom whose HLSL comes from unit H
//   ExportFunction  a FunctionCall to the product this file also produces (Prop::LocalFunction)
//   Extern          a FunctionCall to the asset `/// @asset` names (Prop::FunctionPath)
//
// A frame boundary is also a control-flow boundary: the callee's `return` is relative to the
// callee's own body, so the caller's condition stack, loop depth and branch depth are put aside
// while the body is lowered and restored afterwards. Without that a helper called inside an `if`
// would think its own `return` was conditional.
//
// Diagnostics owned by this file: DSH6220, DSH6221, DSH6222, DSH6223.

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

	void FIRBuilder::CollectCallOutputs(const FBoundFunction& Callee, TArray<FString>& OutNames, TArray<FIRType>& OutTypes)
	{
		// The order a product's FunctionOutputs are made in, so a call's output names line up with
		// the asset's outputs without either side having to look at the other.
		if (!Callee.ReturnType.IsVoid())
		{
			OutNames.Add(TEXT("Result"));
			OutTypes.Add(Callee.ReturnType);
		}
		for (const FBoundParam& Param : Callee.Params)
		{
			if (Param.Direction != EParamDirection::In)
			{
				OutNames.Add(Param.Name);
				OutTypes.Add(Param.Type);
			}
		}
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

		// A parameter nobody passed takes its default, evaluated where the call is: it can only name
		// globals and constants, so the caller's frame is as good a place as the callee's.
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (bBound[Index] || !Callee.Params[Index].Default)
			{
				continue;
			}
			OutValues[Index] = LowerExpr(*Callee.Params[Index].Default);
		}

		return true;
	}

	void FIRBuilder::WriteBackOutputs(
		const FBoundFunction& Callee,
		const TArray<FLValueRef>& Targets,
		FIRValue Node,
		const TArray<FString>& OutputNames,
		const FLangSpan& Span)
	{
		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::In || !Targets.IsValidIndex(Index) || !Targets[Index].IsValid())
			{
				continue;
			}
			const int32 OutputIndex = OutputNames.IndexOfByKey(Callee.Params[Index].Name);
			if (OutputIndex == INDEX_NONE)
			{
				continue;
			}
			// An `out material` comes back as a MaterialAttributes output: the caller's material is
			// replaced by that set, which it can go on reading and writing like any other material.
			StoreLValue(Targets[Index], FLoweredValue::OfOutput(FIRValue{ Node.Node, OutputIndex }, Callee.Params[Index].Type), Span);
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
		switch (Callee.Kind)
		{
		case EBoundFunctionKind::Helper:
			return InlineHelper(Expr, BoundExpr, Callee, BoundExpr.Index);

		case EBoundFunctionKind::Custom:
			return MakeCustomNode(Expr, BoundExpr, Callee, BoundExpr.Index);

		case EBoundFunctionKind::ExportFunction:
		case EBoundFunctionKind::Layer:
		case EBoundFunctionKind::LayerBlend:
		case EBoundFunctionKind::Extern:
			return MakeFunctionCallNode(Expr, BoundExpr, Callee, BoundExpr.Index);

		case EBoundFunctionKind::Entry:
		default:
			Diagnostics.Error(TEXT("DSH6223"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderCallEntry", "'{0}' is this file's material entry and is called by the engine, not by the shader."),
				FText::FromString(Callee.Name)));
			return FLoweredValue();
		}
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
			if (Callee.Params[Index].Direction == EParamDirection::Out || !Arguments.IsValidIndex(Index))
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
		// Unit H is asked first, because it is the one that refuses a body that cannot become a
		// Custom node at all -- a `material` parameter among them (DSH6252, CONTRACT section 6.13
		// #1: the 5.8 translator has no MaterialAttributes case for a custom INPUT pin). When it
		// says no, there is nothing left to lower and it has already reported why.
		FCustomNodeCode Code;
		if (!BuildDreamShaderCustomNodeCode(BoundModule, CalleeIndex, Code, Diagnostics))
		{
			return FLoweredValue();
		}

		TArray<FLoweredValue> Arguments;
		TArray<FLValueRef> Targets;
		if (!BindCallArguments(Expr, BoundExpr, Callee, Arguments, Targets))
		{
			return FLoweredValue();
		}

		FIRNode Node;
		Node.Op = EIROp::Custom;
		Node.ClassName = Callee.Name;
		Node.Properties.Add({ FString(Prop::Code), FIRPropertyValue::MakeString(Code.Code) });
		Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Callee.Name) });
		if (!Code.IncludeFilePaths.IsEmpty())
		{
			Node.Properties.Add({ FString(Prop::IncludeFilePaths), FIRPropertyValue::MakeStringList(Code.IncludeFilePaths) });
		}

		// A void `/// @custom` function still has to hand the graph one output, because the engine's
		// Custom node always has one; unit H's EnsureTopLevelReturn gives the body something to
		// return, and Float1 is what that something is.
		Node.Properties.Add({ FString(Prop::OutputType), FIRPropertyValue::MakeEnum(
			Callee.ReturnType.IsVoid() ? FString(TEXT("Float1")) : CustomOutputTypeName(Callee.ReturnType)) });

		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::Out || !Arguments.IsValidIndex(Index))
			{
				continue;
			}
			// Belt and braces for section 6.13 #1: a Custom input pin never carries attributes, so
			// nothing here may build MakeMaterialAttributes. Unit H refuses such a function above,
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

		TArray<FString> OutputNames;
		TArray<FIRType> OutputTypes;
		CollectCallOutputs(Callee, OutputNames, OutputTypes);

		// Output 0 is the return value. Everything after it is an AdditionalOutputs entry, spelled
		// "Name:Type" so the emitter can rebuild the node's pins without consulting anything else.
		Node.Outputs.Add(Callee.ReturnType.IsVoid() ? FIRType::Float(1) : GraphTypeOf(Callee.ReturnType));
		Node.OutputNames.Add(Callee.ReturnType.IsVoid() ? FString(TEXT("Result")) : OutputNames[0]);

		TArray<FString> Additional;
		const int32 FirstOutParam = Callee.ReturnType.IsVoid() ? 0 : 1;
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

		// Copied out before the write-back, which makes nodes of its own and may move the array.
		TArray<FString> NodeOutputNames = Graph->Nodes[AddedNode.Node].OutputNames;
		WriteBackOutputs(Callee, Targets, AddedNode, NodeOutputNames, Expr.Span);

		// A `material` return (OutputType MaterialAttributes) is a material that arrived through a pin.
		return Callee.ReturnType.IsVoid() ? FLoweredValue() : FLoweredValue::OfOutput(AddedNode, Callee.ReturnType);
	}

	// -------------------------------------------------------------------------- function calls

	FLoweredValue FIRBuilder::MakeFunctionCallNode(const FExpr& Expr, const FBoundExpr& BoundExpr, const FBoundFunction& Callee, int32 CalleeIndex)
	{
		TArray<FLoweredValue> Arguments;
		TArray<FLValueRef> Targets;
		if (!BindCallArguments(Expr, BoundExpr, Callee, Arguments, Targets))
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
		}

		for (int32 Index = 0; Index < Callee.Params.Num(); ++Index)
		{
			if (Callee.Params[Index].Direction == EParamDirection::Out || !Arguments.IsValidIndex(Index))
			{
				continue;
			}
			const FIRValue Value = ValueForParam(Arguments[Index], Callee.Params[Index].Type, EIRConversion::Identity, Expr.Span);
			if (Value.IsValid())
			{
				Node.Inputs.Add({ Callee.Params[Index].Name, Value });
			}
		}

		TArray<FString> OutputNames;
		TArray<FIRType> OutputTypes;
		CollectCallOutputs(Callee, OutputNames, OutputTypes);
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
		WriteBackOutputs(Callee, Targets, Result, OutputNames, Expr.Span);

		// A `material` result is a material that arrived through a pin: `MF_Layer(m).Roughness` and
		// `m = MF_Layer(m);` both need the map with that output as its source, not a bare value.
		return Callee.ReturnType.IsVoid() ? FLoweredValue() : FLoweredValue::OfOutput(Result, Callee.ReturnType);
	}
}

#undef LOCTEXT_NAMESPACE
