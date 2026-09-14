// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// One case per EBoundExprKind. The binder already decided what every expression means; this file
// only has to say what shape of node it becomes, and the shape is chosen for parity with the 1.x
// generator wherever the two can produce the same graph (see the I2 report's lowering table).
//
// Two rules run through all of it. Nothing here resolves a name -- every `Index`, `LocalSlot`,
// `FieldIndex` and `Target` is read straight out of the bound record. And nothing here reports a
// second diagnostic for an expression the binder already rejected: an expression with no bound
// record, or one typed Error, lowers to an empty value in silence.
//
// Diagnostics owned by this file: DSH4350, DSH4352, DSH4361, DSH4365, DSH4371, DSH4373, DSH4376,
// DSH4377.

#include "IRBuilderInternal.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.IRBuilder"

namespace UE::DreamShader::IR::Private
{
	const FExpr* Unparen(const FExpr* Expr)
	{
		while (Expr)
		{
			const FParenExpr* Paren = Expr->As<FParenExpr>();
			if (!Paren)
			{
				return Expr;
			}
			Expr = Paren->Inner.Get();
		}
		return nullptr;
	}

	/**
	 * The word a property-bound argument was written as. Unit S records such an argument as a
	 * `Literal` whose `FIRType` may be `Error`, because there is no `FIRType` for a name, an
	 * enumerator or an asset path -- the spelling lives in the AST and this is where it is read
	 * (S-report, "What the IR builder can rely on" #4). `UE.Foo(Id = PP.Input0)` flattens to
	 * "PP.Input0"; the emitter's reflected enum writer resolves a prefixless entry name itself.
	 */
	static bool TryFlattenWord(const FExpr* Expr, FString& Out)
	{
		Expr = Unparen(Expr);
		if (!Expr)
		{
			return false;
		}
		if (const FLiteralExpr* Literal = Expr->As<FLiteralExpr>())
		{
			Out = Literal->Text;
			return true;
		}
		if (const FIdentifierExpr* Identifier = Expr->As<FIdentifierExpr>())
		{
			Out = Identifier->Name;
			return true;
		}
		if (const FMemberExpr* Member = Expr->As<FMemberExpr>())
		{
			FString Object;
			if (!TryFlattenWord(Member->Object.Get(), Object))
			{
				return false;
			}
			Out = Object + TEXT(".") + Member->Member;
			return true;
		}
		return false;
	}

	/** The object a member access or an index expression is applied to. */
	static const FExpr* ObjectOf(const FExpr& Expr)
	{
		if (const FMemberExpr* Member = Expr.As<FMemberExpr>())
		{
			return Member->Object.Get();
		}
		if (const FIndexExpr* Index = Expr.As<FIndexExpr>())
		{
			return Index->Object.Get();
		}
		return nullptr;
	}

	/** The operands of a core op, in the order the bound record's op expects them. */
	static void CollectOperandExprs(const FExpr& Expr, TArray<const FExpr*>& Out)
	{
		if (const FBinaryExpr* Binary = Expr.As<FBinaryExpr>())
		{
			Out.Add(Binary->Left.Get());
			Out.Add(Binary->Right.Get());
			return;
		}
		if (const FUnaryExpr* Unary = Expr.As<FUnaryExpr>())
		{
			Out.Add(Unary->Operand.Get());
			return;
		}
		if (const FCallExpr* Call = Expr.As<FCallExpr>())
		{
			for (const FArgument& Argument : Call->Arguments)
			{
				Out.Add(Argument.Value.Get());
			}
			return;
		}
		if (const FConditionalExpr* Conditional = Expr.As<FConditionalExpr>())
		{
			Out.Add(Conditional->Condition.Get());
			Out.Add(Conditional->TrueValue.Get());
			Out.Add(Conditional->FalseValue.Get());
		}
	}

	/** `x += e` -> Add. EIROp::Count for a plain `=`. */
	static EIROp CoreOpForAssign(EAssignOp Op)
	{
		switch (Op)
		{
		case EAssignOp::AddAssign: return EIROp::Add;
		case EAssignOp::SubtractAssign: return EIROp::Subtract;
		case EAssignOp::MultiplyAssign: return EIROp::Multiply;
		case EAssignOp::DivideAssign: return EIROp::Divide;
		case EAssignOp::ModuloAssign: return EIROp::Fmod;
		default: return EIROp::Count;
		}
	}

	// ------------------------------------------------------------------------------- conversions

	FIRValue FIRBuilder::CoerceToWidth(FIRValue Value, int32 Width, const FLangSpan& Span)
	{
		if (!Value.IsValid() || Width <= 0)
		{
			return Value;
		}

		const int32 Source = WidthOf(Value);
		if (Source == Width)
		{
			return Value;
		}
		if (Source == 1)
		{
			return MakeBroadcast(Value, Width, Span);
		}
		if (Source > Width)
		{
			// 1.x takes the leading components (CodeCoercion.cpp, LeadingSwizzles).
			return MakeSwizzle(Value, FString(GSwizzleComponents).Left(Width), Span);
		}
		// Narrower than wanted is left alone -- the engine pads what it has to -- with one exception: a
		// channel view that leads a wider value (`float4 VC = UE.VertexColor();` reads the RGB pin as its
		// default output) stands for that whole value, so it is widened from the node's own views.
		const FIRValue Whole = WidenChannelView(Value, Width, Span);
		return Whole.IsValid() ? Whole : Value;
	}

	FIRValue FIRBuilder::ValueForPin(const FLoweredValue& Value, ECatalogValueType PinType, EIRConversion Conversion, const FLangSpan& Span)
	{
		if (Value.IsMaterial())
		{
			if (PinType == ECatalogValueType::MaterialAttributes || PinType == ECatalogValueType::Unknown)
			{
				return MaterialiseMaterial(Value.Material, Span);
			}
			Diagnostics.Error(TEXT("DSH4371"), Span, FText::Format(
				LOCTEXT("IRBuilderMaterialIntoPin", "A 'material' value reached a pin that carries {0}; only a MaterialAttributes pin accepts one."),
				FText::FromString(LexToString(PinType))));
			return FIRValue::None();
		}

		if (!Value.IsValue())
		{
			if (Value.IsAggregate())
			{
				Diagnostics.Error(TEXT("DSH4365"), Span, LOCTEXT("IRBuilderAggregateIntoPin",
					"A struct has no graph form; pass one of its fields, or move the whole thing into a '/// @custom' function."));
			}
			return FIRValue::None();
		}

		FIRValue Result = ApplyConversion(Value.Value, Conversion, INDEX_NONE, Span);
		switch (PinType)
		{
		case ECatalogValueType::Float1: return CoerceToWidth(Result, 1, Span);
		case ECatalogValueType::Float2: return CoerceToWidth(Result, 2, Span);
		case ECatalogValueType::Float3: return CoerceToWidth(Result, 3, Span);
		case ECatalogValueType::Float4: return CoerceToWidth(Result, 4, Span);
		default:
			// Numeric, Bool, StaticBool, Texture, SamplerState, Substrate: the pin takes the value's
			// own width, which is what the engine's auto-broadcast expects.
			return Result;
		}
	}

	FIRValue FIRBuilder::ValueForParam(const FLoweredValue& Value, const FIRType& Target, EIRConversion Conversion, const FLangSpan& Span)
	{
		if (Value.IsMaterial())
		{
			if (Target.IsMaterial())
			{
				return MaterialiseMaterial(Value.Material, Span);
			}
			Diagnostics.Error(TEXT("DSH4371"), Span, FText::Format(
				LOCTEXT("IRBuilderMaterialIntoParam", "A 'material' value was passed where {0} was expected."),
				FText::FromString(Target.ToString())));
			return FIRValue::None();
		}

		if (!Value.IsValue())
		{
			return FIRValue::None();
		}

		FIRValue Result = ApplyConversion(Value.Value, Conversion, Target.GraphComponentCount(), Span);
		const int32 Width = Target.GraphComponentCount();
		return Width > 0 ? CoerceToWidth(Result, Width, Span) : Result;
	}

	// -------------------------------------------------------------------------------- dispatch

	FLoweredValue FIRBuilder::LowerExpr(const FExpr& Expr)
	{
		const FBoundExpr* BoundExpr = Bound(Expr);
		if (!BoundExpr || BoundExpr->Kind == EBoundExprKind::Error || BoundExpr->Type.IsError())
		{
			// The binder already said what is wrong with it.
			return FLoweredValue();
		}

		if (BoundExpr->Type.IsMatrix())
		{
			// Everywhere, `/// @custom` pins included (CONTRACT section 6.13 #2): there is no matrix
			// ECustomMaterialOutputType and no matrix input pin, so a matrix that did not fold away
			// has nowhere to go. Inside a custom body it is free, because it never crosses a pin.
			Diagnostics.Error(TEXT("DSH4361"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderMatrixValue", "This expression is {0} and the material graph has no matrices; compute it inside the custom body instead."),
				FText::FromString(BoundExpr->Type.ToString())));
			return FLoweredValue();
		}

		switch (BoundExpr->Kind)
		{
		case EBoundExprKind::Literal:
			return LowerLiteral(Expr, *BoundExpr);

		case EBoundExprKind::Local:
		{
			if (!Frame().Locals.IsValidIndex(BoundExpr->LocalSlot))
			{
				return FLoweredValue();
			}
			if (Frame().Locals[BoundExpr->LocalSlot].IsNeverAssigned())
			{
				// A read of a local no path has assigned: DSH4376, said once. When the read does not count
				// -- an `inout` copy, an arm that may never run -- the never-assigned state travels on with
				// the value instead.
				FLValueRef Ref;
				Ref.FrameIndex = Frames.Num() - 1;
				Ref.BaseIndex = BoundExpr->LocalSlot;
				if (ReportUnsetRead(Expr, Ref))
				{
					return FLoweredValue();
				}
			}
			return Frame().Locals[BoundExpr->LocalSlot];
		}

		case EBoundExprKind::Param:
		{
			const FFrame& Current = Frame();
			return Current.Params.IsValidIndex(BoundExpr->Index) ? Current.Params[BoundExpr->Index] : FLoweredValue();
		}

		case EBoundExprKind::Global:
			return GlobalValue(BoundExpr->Index, Expr.Span);

		case EBoundExprKind::StructField:
		{
			const FExpr* Object = ObjectOf(Expr);
			if (!Object)
			{
				return FLoweredValue();
			}
			const FLoweredValue Base = LowerExpr(*Object);
			if (!Base.IsAggregate() || !Base.Fields.IsValidIndex(BoundExpr->FieldIndex))
			{
				return FLoweredValue();
			}
			if (Base.Fields[BoundExpr->FieldIndex].IsNeverAssigned())
			{
				// `P.B` where nothing wrote B: the same DSH4376 as a whole local, for one field of one.
				FLValueRef Ref;
				if (ResolveLValue(Expr, Ref) && ReportUnsetRead(Expr, Ref))
				{
					return FLoweredValue();
				}
			}
			return Base.Fields[BoundExpr->FieldIndex];
		}

		case EBoundExprKind::MaterialField:
		{
			const FExpr* Object = ObjectOf(Expr);
			if (!Object)
			{
				return FLoweredValue();
			}
			// The read goes through the slot, not through a copy: reading an attribute of a material
			// that arrived on a pin makes (and caches) the GetMaterialAttributes node in the slot. Only
			// when the object IS the slot, though: `m.MaterialAttributes.Roughness` resolves to m's slot
			// with an attribute on top, and reading Roughness off m itself would be a different value.
			FLValueRef Ref;
			if (ResolveLValue(*Object, Ref) && Ref.MaterialAttribute.IsEmpty())
			{
				if (FLoweredValue* Slot = ResolveSlot(Ref))
				{
					if (Slot->IsMaterial())
					{
						FIRValue Field = FIRValue::None();
						if (ReadMaterialField(Slot->Material, BoundExpr->FieldIndex, Expr.Span, Field))
						{
							return FLoweredValue::OfOutput(Field, BoundExpr->Type);
						}
						return FLoweredValue();
					}
				}
			}

			// A material that is not a slot: `UE.BlendMaterialAttributes(...).Roughness`, a function
			// call's `material` result, `m.MaterialAttributes`. Its producer hands it over as a map whose
			// source is the node's output (FLoweredValue::OfOutput), so the read breaks that output open
			// with GetMaterialAttributes like any material that arrived through a pin. A plain Value of
			// material type is taken the same way rather than trusted never to turn up.
			FLoweredValue Base = LowerExpr(*Object);
			if (Base.IsValue() && TypeOfValue(Base.Value).IsMaterial())
			{
				Base = FLoweredValue::MakeMaterial(Base.Value);
			}
			if (Base.IsMaterial())
			{
				FIRValue Field = FIRValue::None();
				if (ReadMaterialField(Base.Material, BoundExpr->FieldIndex, Expr.Span, Field))
				{
					return FLoweredValue::OfOutput(Field, BoundExpr->Type);
				}
				return FLoweredValue();
			}
			if (!Base.IsEmpty())
			{
				// The binder typed the object a material and it did not lower to one. Returning nothing
				// here in silence is how `m.Opacity = UE.BlendMaterialAttributes(...).Roughness` once
				// lost its write with no message at all, so this seam says so.
				const FCatalogMaterialAttribute* Entry = Attribute(BoundExpr->FieldIndex);
				Diagnostics.Error(TEXT("DSH4350"), Expr.Span, FText::Format(
					LOCTEXT("IRBuilderFieldOfNonMaterial", "The IR builder cannot read '{0}' here: the binder typed what it is read from as a material, but that did not lower to one."),
					FText::FromString(Entry ? Entry->Name : FString())));
			}
			return FLoweredValue();
		}

		case EBoundExprKind::Swizzle:
			return LowerSwizzle(Expr, *BoundExpr);

		case EBoundExprKind::IndexConst:
		{
			const FExpr* Object = ObjectOf(Expr);
			if (!Object)
			{
				return FLoweredValue();
			}
			const FLoweredValue Base = LowerExpr(*Object);
			if (Base.IsAggregate())
			{
				// An array element. The bound record puts the element index in FieldIndex.
				return Base.Fields.IsValidIndex(BoundExpr->FieldIndex) ? Base.Fields[BoundExpr->FieldIndex] : FLoweredValue();
			}
			if (Base.IsValue())
			{
				return FLoweredValue::Of(MakeSwizzle(Base.Value, BoundExpr->Swizzle, Expr.Span));
			}
			return FLoweredValue();
		}

		case EBoundExprKind::NodeOutput:
		{
			const FExpr* Object = ObjectOf(Expr);
			if (!Object)
			{
				return FLoweredValue();
			}
			const FLoweredValue Base = LowerExpr(*Object);
			if (!Base.IsValue())
			{
				return FLoweredValue();
			}
			FIRValue Result = Base.Value;
			Result.Output = BoundExpr->FieldIndex;
			// An output typed MaterialAttributes is a `material` that arrived through a pin.
			return FLoweredValue::OfOutput(Result, BoundExpr->Type);
		}

		case EBoundExprKind::CoreOp:
			return LowerCoreOp(Expr, *BoundExpr);

		case EBoundExprKind::Constructor:
			return LowerConstructor(Expr, *BoundExpr);

		case EBoundExprKind::StructConstructor:
			return LowerStructConstructor(Expr, *BoundExpr);

		case EBoundExprKind::InitializerList:
			return LowerInitializerList(Expr, *BoundExpr);

		case EBoundExprKind::Cast:
		{
			const FCastExpr* Cast = Expr.As<FCastExpr>();
			if (!Cast || !Cast->Operand)
			{
				return FLoweredValue();
			}
			const FLoweredValue Operand = LowerExpr(*Cast->Operand);
			if (!Operand.IsValue())
			{
				return Operand;
			}
			FIRValue Result = Operand.Value;
			if (TypeOfValue(Result).IsBool() && !BoundExpr->Type.IsBool())
			{
				Result = MakeCoreOp(EIROp::Convert, { Result }, FIRType::Float(WidthOf(Result)), Expr.Span);
			}
			const int32 Width = BoundExpr->Type.GraphComponentCount();
			return FLoweredValue::Of(Width > 0 ? CoerceToWidth(Result, Width, Expr.Span) : Result);
		}

		case EBoundExprKind::ReflectedCall:
			return LowerReflectedCall(Expr, *BoundExpr);

		case EBoundExprKind::FunctionCall:
			return LowerFunctionCall(Expr, *BoundExpr);

		case EBoundExprKind::TextureSample:
			return LowerTextureSample(Expr, *BoundExpr);

		case EBoundExprKind::Conditional:
			return LowerConditional(Expr, *BoundExpr);

		case EBoundExprKind::Assign:
			return LowerAssign(Expr, *BoundExpr);

		case EBoundExprKind::Paren:
		{
			// `(e)` and unary `+e`, which the binder records as transparent too (S-report #6).
			if (const FParenExpr* Paren = Expr.As<FParenExpr>())
			{
				return Paren->Inner ? LowerExpr(*Paren->Inner) : FLoweredValue();
			}
			if (const FUnaryExpr* Unary = Expr.As<FUnaryExpr>())
			{
				return Unary->Operand ? LowerExpr(*Unary->Operand) : FLoweredValue();
			}
			return FLoweredValue();
		}

		case EBoundExprKind::Error:
		default:
			Diagnostics.Error(TEXT("DSH4350"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderNoLowering", "The IR builder has no lowering for a bound expression of kind {0}."),
				FText::FromString(LexToString(BoundExpr->Kind))));
			return FLoweredValue();
		}
	}

	FIRValue FIRBuilder::LowerValue(const FExpr& Expr)
	{
		const FLoweredValue Lowered = LowerExpr(Expr);
		if (Lowered.IsValue())
		{
			return Lowered.Value;
		}
		if (Lowered.IsMaterial())
		{
			return MaterialiseMaterial(Lowered.Material, Expr.Span);
		}
		if (Lowered.IsAggregate())
		{
			Diagnostics.Error(TEXT("DSH4365"), Expr.Span, LOCTEXT("IRBuilderAggregateAsValue",
				"A struct has no graph form here; use one of its fields."));
		}
		return FIRValue::None();
	}

	FIRValue FIRBuilder::LowerOperand(const FExpr& Expr, int32 TargetWidth)
	{
		const FBoundExpr* BoundExpr = Bound(Expr);
		FIRValue Value = LowerValue(Expr);
		if (BoundExpr)
		{
			Value = ApplyConversion(Value, BoundExpr->Conversion, TargetWidth, Expr.Span);
		}
		return TargetWidth > 0 ? CoerceToWidth(Value, TargetWidth, Expr.Span) : Value;
	}

	// --------------------------------------------------------------------------------- leaves

	FLoweredValue FIRBuilder::LowerLiteral(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		if (BoundExpr.Type.IsBool())
		{
			// A bool literal is a StaticBool in the graph; the emitter picks the class off the
			// output type, as it does for every other Constant.
			FIRNode Node;
			Node.Op = EIROp::Constant;
			Node.Outputs.Add(FIRType::Bool(1));
			const double Components[4] = { BoundExpr.ConstantValue[0] != 0.0 ? 1.0 : 0.0, 0.0, 0.0, 0.0 };
			Node.Properties.Add({ FString(Prop::Value), FIRPropertyValue::MakeFloat4(Components, 1) });
			return FLoweredValue::Of(AddNode(MoveTemp(Node), Expr.Span));
		}

		// A string literal has no value form: it only ever binds to a property, and the property
		// path reads the AST node directly rather than asking for a value.
		if (const FLiteralExpr* Literal = Expr.As<FLiteralExpr>())
		{
			if (Literal->LiteralKind == ELiteralKind::String)
			{
				return FLoweredValue();
			}
		}

		const int32 Width = FMath::Max(BoundExpr.Type.GraphComponentCount(), 1);
		return FLoweredValue::Of(MakeConstant(BoundExpr.ConstantValue, Width, Expr.Span));
	}

	FLoweredValue FIRBuilder::LowerSwizzle(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		const FExpr* Object = ObjectOf(Expr);
		if (!Object)
		{
			return FLoweredValue();
		}
		const FIRValue Base = LowerValue(*Object);
		if (!Base.IsValid())
		{
			return FLoweredValue();
		}
		return FLoweredValue::Of(MakeSwizzle(Base, BoundExpr.Swizzle, Expr.Span));
	}

	// --------------------------------------------------------------------------------- core ops

	FLoweredValue FIRBuilder::LowerCoreOp(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		// Call syntax carries Args, one per operand slot, with TargetIndex = the operand POSITION;
		// operator syntax carries none and its operands are the AST children in order (S-report #3).
		// Reading the arguments in AST order instead would put `lerp(0.0, 1.0, Alpha = 0.5)` together
		// wrong, which is exactly the case the binder added Args for.
		TArray<const FExpr*> OperandExprs;
		TArray<EIRConversion> OperandConversions;
		if (!BoundExpr.Args.IsEmpty())
		{
			for (const FBoundArgument& Argument : BoundExpr.Args)
			{
				const int32 Slot = Argument.TargetIndex;
				if (Slot < 0)
				{
					continue;
				}
				if (OperandExprs.Num() <= Slot)
				{
					OperandExprs.SetNum(Slot + 1);
					OperandConversions.SetNum(Slot + 1);
				}
				OperandExprs[Slot] = ArgumentExpr(Expr, Argument);
				OperandConversions[Slot] = Argument.Conversion;
			}
		}
		else
		{
			CollectOperandExprs(Expr, OperandExprs);
			OperandConversions.Init(EIRConversion::Identity, OperandExprs.Num());
		}

		const FIRCoreOpInfo& Info = GetCoreOpInfo(BoundExpr.CoreOp);
		// Only the ops whose operands must all be float3 (cross, reflect, refract) fix a width; for
		// everything else the graph broadcasts a scalar operand itself, and inserting a node here
		// would put one in the asset that the 1.x generator never emitted.
		const int32 OperandWidth = (Info.Typing == EIRTypingRule::Float3) ? 3 : INDEX_NONE;

		// `!`, `&&` and `||` want a truth value. A number reaching one carries Conversion Numeric
		// into a bool target, which the graph has no type for, so it becomes the comparison against
		// zero that HLSL means by it (S-report #10). A bool operand is already one and is left alone.
		const bool bLogicalOp = BoundExpr.CoreOp == EIROp::LogicalAnd
			|| BoundExpr.CoreOp == EIROp::LogicalOr
			|| BoundExpr.CoreOp == EIROp::LogicalNot;

		TArray<FIRValue> Operands;
		Operands.Reserve(OperandExprs.Num());
		for (int32 Index = 0; Index < OperandExprs.Num(); ++Index)
		{
			const FExpr* OperandExpr = OperandExprs[Index];
			if (!OperandExpr)
			{
				return FLoweredValue();
			}
			FIRValue Operand = LowerOperand(*OperandExpr, OperandWidth);
			if (!Operand.IsValid())
			{
				return FLoweredValue();
			}
			if (OperandConversions.IsValidIndex(Index) && OperandConversions[Index] != EIRConversion::Identity)
			{
				Operand = ApplyConversion(Operand, OperandConversions[Index], OperandWidth, OperandExpr->Span);
			}
			if (bLogicalOp && !TypeOfValue(Operand).IsBool())
			{
				const FIRValue Zero = MakeScalarConstant(0.0, OperandExpr->Span);
				Operand = MakeCoreOp(EIROp::NotEqual, { Operand, Zero }, FIRType::Bool(1), OperandExpr->Span);
			}
			Operands.Add(Operand);
		}

		if (Operands.IsEmpty())
		{
			return FLoweredValue();
		}

		return FLoweredValue::Of(MakeCoreOp(BoundExpr.CoreOp, MoveTemp(Operands), GraphTypeOf(BoundExpr.Type), Expr.Span));
	}

	// ----------------------------------------------------------------------------- constructors

	FLoweredValue FIRBuilder::LowerConstructor(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		const int32 Width = FMath::Max(BoundExpr.Type.GraphComponentCount(), 1);

		TArray<const FExpr*> Parts;
		CollectOperandExprs(Expr, Parts);
		if (const FInitializerListExpr* List = Expr.As<FInitializerListExpr>())
		{
			Parts.Reset();
			for (const FExprPtr& Element : List->Elements)
			{
				Parts.Add(Element.Get());
			}
		}

		if (Parts.IsEmpty())
		{
			const double Zeroes[4] = { 0.0, 0.0, 0.0, 0.0 };
			return FLoweredValue::Of(MakeConstant(Zeroes, Width, Expr.Span));
		}

		TArray<FIRValue> Values;
		Values.Reserve(Parts.Num());
		int32 Total = 0;
		for (const FExpr* Part : Parts)
		{
			if (!Part)
			{
				return FLoweredValue();
			}
			const FIRValue Value = LowerOperand(*Part, INDEX_NONE);
			if (!Value.IsValid())
			{
				return FLoweredValue();
			}
			Total += WidthOf(Value);
			Values.Add(Value);
		}

		// `float3(x)` -- one scalar for a wider target -- replicates. 1.x appends the same value N
		// times; the IR says Broadcast and leaves the emitter to choose, which keeps the two
		// readings of "replicate" in one place instead of N-1 Append nodes in every graph.
		if (Values.Num() == 1 && Total == 1 && Width > 1)
		{
			return FLoweredValue::Of(MakeBroadcast(Values[0], Width, Expr.Span));
		}
		if (Values.Num() == 1 && Total == Width)
		{
			return FLoweredValue::Of(Values[0]);
		}

		return FLoweredValue::Of(MakeAppend(Values, Expr.Span));
	}

	FLoweredValue FIRBuilder::LowerStructConstructor(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		if (!BoundModule.Structs.IsValidIndex(BoundExpr.Type.StructIndex))
		{
			return FLoweredValue();
		}

		const FBoundStruct& Struct = BoundModule.Structs[BoundExpr.Type.StructIndex];
		FLoweredValue Result = FLoweredValue::MakeAggregate(Struct.Fields.Num());

		TArray<const FExpr*> Parts;
		CollectOperandExprs(Expr, Parts);
		if (const FInitializerListExpr* List = Expr.As<FInitializerListExpr>())
		{
			Parts.Reset();
			for (const FExprPtr& Element : List->Elements)
			{
				Parts.Add(Element.Get());
			}
		}

		// A struct is a compile-time aggregate: the arguments fill its fields in order, and a named
		// argument the binder matched says which field through FBoundArgument::TargetIndex.
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			int32 FieldIndex = Index;
			if (BoundExpr.Args.IsValidIndex(Index) && BoundExpr.Args[Index].TargetIndex != INDEX_NONE)
			{
				FieldIndex = BoundExpr.Args[Index].TargetIndex;
			}
			if (!Parts[Index] || !Result.Fields.IsValidIndex(FieldIndex))
			{
				continue;
			}
			Result.Fields[FieldIndex] = LowerExpr(*Parts[Index]);
		}

		return Result;
	}

	FLoweredValue FIRBuilder::LowerInitializerList(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		return BoundExpr.Type.IsStruct() ? LowerStructConstructor(Expr, BoundExpr) : LowerConstructor(Expr, BoundExpr);
	}

	// ------------------------------------------------------------------------- reflected calls

	const FExpr* FIRBuilder::ArgumentExpr(const FExpr& CallExpr, const FBoundArgument& Argument) const
	{
		const FCallExpr* Call = CallExpr.As<FCallExpr>();
		if (!Call || !Call->Arguments.IsValidIndex(Argument.ArgumentIndex))
		{
			return nullptr;
		}
		return Call->Arguments[Argument.ArgumentIndex].Value.Get();
	}

	bool FIRBuilder::MakePropertyValue(const FExpr& Value, ECatalogValueType Type, FIRPropertyValue& Out)
	{
		const FExpr* Inner = Unparen(&Value);
		if (!Inner)
		{
			return false;
		}

		const FLiteralExpr* Literal = Inner->As<FLiteralExpr>();
		const FBoundExpr* BoundValue = Bound(*Inner);
		const bool bHasConstant = BoundValue != nullptr && BoundValue->bIsConstant;

		switch (Type)
		{
		case ECatalogValueType::String:
		case ECatalogValueType::Enum:
		case ECatalogValueType::Name:
		case ECatalogValueType::Object:
		{
			// A word, not a value: a quoted string, a bare enumerator, a dotted enum path or an
			// asset path. The binder types all of them Error on purpose, so the spelling comes out
			// of the AST rather than out of the bound record (S-report #4).
			FString Text;
			if (!TryFlattenWord(Inner, Text))
			{
				return false;
			}
			Out = (Type == ECatalogValueType::Enum) ? FIRPropertyValue::MakeEnum(Text)
				: (Type == ECatalogValueType::Name) ? FIRPropertyValue::MakeName(Text)
				: (Type == ECatalogValueType::Object) ? FIRPropertyValue::MakeObject(Text)
				: FIRPropertyValue::MakeString(Text);
			return true;
		}

		case ECatalogValueType::Bool:
		case ECatalogValueType::StaticBool:
			if (Literal && Literal->LiteralKind == ELiteralKind::Bool)
			{
				Out = FIRPropertyValue::MakeBool(Literal->bBool);
				return true;
			}
			if (bHasConstant)
			{
				Out = FIRPropertyValue::MakeBool(BoundValue->ConstantValue[0] != 0.0);
				return true;
			}
			return false;

		case ECatalogValueType::Int:
			if (bHasConstant)
			{
				Out = FIRPropertyValue::MakeInt(static_cast<int64>(BoundValue->ConstantValue[0]));
				return true;
			}
			if (Literal && (Literal->LiteralKind == ELiteralKind::Int || Literal->LiteralKind == ELiteralKind::UInt))
			{
				Out = FIRPropertyValue::MakeInt(static_cast<int64>(Literal->Integer));
				return true;
			}
			return false;

		case ECatalogValueType::Float1:
			if (bHasConstant)
			{
				Out = FIRPropertyValue::MakeFloat(BoundValue->ConstantValue[0]);
				return true;
			}
			return false;

		case ECatalogValueType::Float2:
		case ECatalogValueType::Float3:
		case ECatalogValueType::Float4:
		case ECatalogValueType::Numeric:
			if (bHasConstant)
			{
				const int32 Width = (Type == ECatalogValueType::Float2) ? 2
					: (Type == ECatalogValueType::Float3) ? 3
					: (Type == ECatalogValueType::Float4) ? 4
					: FMath::Max(BoundValue->Type.GraphComponentCount(), 1);
				Out = FIRPropertyValue::MakeFloat4(BoundValue->ConstantValue, Width);
				return true;
			}
			return false;

		default:
		{
			if (bHasConstant && !BoundValue->Type.IsError())
			{
				Out = FIRPropertyValue::MakeFloat4(BoundValue->ConstantValue, FMath::Max(BoundValue->Type.GraphComponentCount(), 1));
				return true;
			}
			// An unknown property type with a word in it: hand the spelling through as a string and
			// let the emitter's reflection decide what the engine property wanted.
			FString Text;
			if (TryFlattenWord(Inner, Text))
			{
				Out = FIRPropertyValue::MakeString(Text);
				return true;
			}
			return false;
		}
		}
	}

	FLoweredValue FIRBuilder::LowerReflectedCall(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		// A missing catalog was reported once for the whole module; saying it again per call would
		// bury the one message that matters under one per `UE.*` line.
		if (!Catalog)
		{
			return FLoweredValue();
		}
		if (!Catalog->Expressions.IsValidIndex(BoundExpr.Index))
		{
			// A catalog without the entry the binder recorded is not the catalog the module was bound
			// against (CONTRACT section 6.13 #28). Said here, at the call, because returning nothing in
			// silence would drop whatever this call feeds.
			Diagnostics.Error(TEXT("DSH4352"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderNoExpressionEntry", "The builtin catalog has no expression entry {0}; the module was bound against a different catalog than this build is reading."),
				FText::AsNumber(BoundExpr.Index)));
			return FLoweredValue();
		}

		const FCatalogExpression& Entry = Catalog->Expressions[BoundExpr.Index];

		FIRNode Node;
		Node.Op = EIROp::Reflected;
		Node.ClassName = Entry.ShortName;
		Node.CatalogIndex = BoundExpr.Index;

		// A custom-output class has no outputs at all: it is a statement, and the prune pass keeps
		// it alive because of that (see the I2 report, "Contract changes needed").
		for (const FCatalogPin& Output : Entry.Outputs)
		{
			// A Numeric output follows its inputs (Add, Multiply, LinearInterpolate). The binder has already
			// widened the call to what it is used as, and that width is what every reader of the node --
			// WidthOf, CoerceToWidth, the validator -- has to see (CONTRACT 6.13 #32).
			FIRType OutputType = TypeFromCatalogValueType(Output.Type);
			if (Output.Type == ECatalogValueType::Numeric && Entry.Outputs.Num() == 1
				&& BoundExpr.Type.IsNumeric() && !BoundExpr.Type.IsMatrix())
			{
				OutputType = BoundExpr.Type;
			}
			Node.Outputs.Add(OutputType);
			Node.OutputNames.Add(Output.Name);
		}

		for (const FBoundArgument& Argument : BoundExpr.Args)
		{
			const FExpr* Value = ArgumentExpr(Expr, Argument);
			if (!Value)
			{
				continue;
			}

			if (Argument.bIsProperty)
			{
				// `UE.Expression(Class = "X")`: the Class argument selected the catalog entry rather
				// than naming one of its properties, and the binder records it with no TargetIndex
				// (S-report #4). Keep what the author wrote under its own property name.
				if (Argument.TargetIndex == INDEX_NONE && Argument.Target.Equals(TEXT("Class"), ESearchCase::CaseSensitive))
				{
					FString ClassText;
					if (TryFlattenWord(Value, ClassText))
					{
						Node.Properties.Add({ FString(Prop::ClassSpecifier), FIRPropertyValue::MakeString(ClassText) });
					}
					continue;
				}

				const FCatalogProperty* Property = Entry.Properties.IsValidIndex(Argument.TargetIndex) ? &Entry.Properties[Argument.TargetIndex] : nullptr;
				FIRPropertyValue PropertyValue;
				if (!MakePropertyValue(*Value, Property ? Property->Type : ECatalogValueType::Unknown, PropertyValue))
				{
					Diagnostics.Error(TEXT("DSH4373"), Value->Span, FText::Format(
						LOCTEXT("IRBuilderPropertyNotLiteral", "'{0}' is a property of {1} and needs a literal; this argument is computed at run time."),
						FText::FromString(Argument.Target), FText::FromString(Entry.ShortName)));
					continue;
				}
				Node.Properties.Add({ Argument.Target, PropertyValue });
				continue;
			}

			const FCatalogPin* Pin = Entry.Inputs.IsValidIndex(Argument.TargetIndex) ? &Entry.Inputs[Argument.TargetIndex] : nullptr;

			// A constant reaching a pin that has a "Const*" twin is written as the twin, which is
			// what the 1.x generator's output looks like and what an author sees in the editor.
			const FExpr* Inner = Unparen(Value);
			const FBoundExpr* BoundValue = Inner ? Bound(*Inner) : nullptr;
			if (Pin && !Pin->ConstPropertyName.IsEmpty() && BoundValue && BoundValue->bIsConstant)
			{
				// In the TWIN's type, not the pin's: the engine types a pin loosely (Lerp's Alpha is any
				// float, `Numeric`) while its twin is a plain `float` property, and a width-1 vector would
				// be written "(0.5)", which a float property does not import.
				const int32 TwinIndex = Entry.FindProperty(Pin->ConstPropertyName);
				const ECatalogValueType TwinType = Entry.Properties.IsValidIndex(TwinIndex)
					&& Entry.Properties[TwinIndex].Type != ECatalogValueType::Unknown
					? Entry.Properties[TwinIndex].Type
					: Pin->Type;
				// A scalar twin holds one number. A vector constant fits it only when every component is that
				// number, which the pin then broadcasts; `float3(1, 0, 0)` would be written as 1 and lerp toward
				// white, so it goes to the pin as a node instead.
				const int32 ConstantWidth = FMath::Clamp(BoundValue->Type.GraphComponentCount(), 1, 4);
				bool bFitsTwin = true;
				if (TwinType == ECatalogValueType::Float1 || TwinType == ECatalogValueType::Int || TwinType == ECatalogValueType::Bool)
				{
					for (int32 Component = 1; Component < ConstantWidth; ++Component)
					{
						bFitsTwin = bFitsTwin && BoundValue->ConstantValue[Component] == BoundValue->ConstantValue[0];
					}
				}
				FIRPropertyValue PropertyValue;
				if (bFitsTwin && MakePropertyValue(*Value, TwinType, PropertyValue))
				{
					Node.Properties.Add({ Pin->ConstPropertyName, PropertyValue });
					continue;
				}
			}

			const FLoweredValue Lowered = LowerExpr(*Value);
			const FIRValue PinValue = ValueForPin(Lowered, Pin ? Pin->Type : ECatalogValueType::Numeric, Argument.Conversion, Value->Span);
			if (PinValue.IsValid())
			{
				Node.Inputs.Add({ Argument.Target, PinValue });
			}
		}

		// A class whose one output is MaterialAttributes (`UE.BlendMaterialAttributes`) makes a
		// `material` that arrived through a pin: a field read off the call breaks it open with
		// GetMaterialAttributes, and `m = UE.BlendMaterialAttributes(...)` gives m that set to build on.
		return FLoweredValue::OfOutput(AddNode(MoveTemp(Node), Expr.Span), BoundExpr.Type);
	}

	// ------------------------------------------------------------------------------- textures

	FLoweredValue FIRBuilder::LowerTextureSample(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		FIRValue Texture = FIRValue::None();
		FIRValue Coordinates = FIRValue::None();
		FIRValue Sampler = FIRValue::None();
		FIRValue Level = FIRValue::None();

		for (const FBoundArgument& Argument : BoundExpr.Args)
		{
			// `Tex.Sample(UV)` and `Tex.SampleLevel(UV, L)` have no argument for the texture: it is
			// the object the method was called on, and the binder marks that with ArgumentIndex
			// INDEX_NONE (S-report #2). The callee itself is never in the bound map, but its object
			// is, so this reaches the value and not the name.
			const FExpr* Value = ArgumentExpr(Expr, Argument);
			if (!Value && Argument.ArgumentIndex == INDEX_NONE)
			{
				const FCallExpr* Call = Expr.As<FCallExpr>();
				const FExpr* Method = Call ? Unparen(Call->Callee.Get()) : nullptr;
				if (const FMemberExpr* Member = Method ? Method->As<FMemberExpr>() : nullptr)
				{
					Value = Member->Object.Get();
				}
			}
			if (!Value)
			{
				continue;
			}
			const FIRValue Lowered = LowerValue(*Value);
			if (Argument.Target.Equals(TEXT("Texture"), ESearchCase::CaseSensitive))
			{
				Texture = Lowered;
			}
			else if (Argument.Target.Equals(TEXT("UV"), ESearchCase::CaseSensitive))
			{
				Coordinates = ApplyConversion(Lowered, Argument.Conversion, 2, Value->Span);
			}
			else if (Argument.Target.Equals(TEXT("Sampler"), ESearchCase::CaseSensitive))
			{
				Sampler = Lowered;
			}
			else if (Argument.Target.Equals(TEXT("Level"), ESearchCase::CaseSensitive))
			{
				Level = CoerceToWidth(Lowered, 1, Value->Span);
			}
		}

		if (!Texture.IsValid())
		{
			return FLoweredValue();
		}

		FIRNode Node;
		Node.Op = EIROp::TextureSample;
		// EXACTLY four operands, always (CONTRACT section 6.13 #14): [Texture, UV, Sampler, Level],
		// with FIRValue::None() in the slots the source did not fill. The validator accepts an empty
		// value in slots 2 and 3 of this op alone, and the emitter indexes rather than counts, so
		// `Tex.SampleLevel(UV, L)` keeps its level at operand 3 with no sampler in front of it.
		Node.Operands.Add(Texture);
		Node.Operands.Add(Coordinates);
		Node.Operands.Add(Sampler);
		Node.Operands.Add(Level);

		static const TCHAR* const OutputNames[5] = { TEXT("RGBA"), TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
		Node.Outputs.Add(FIRType::Float(4));
		Node.OutputNames.Add(OutputNames[0]);
		for (int32 Index = 1; Index < 5; ++Index)
		{
			Node.Outputs.Add(FIRType::Float(1));
			Node.OutputNames.Add(OutputNames[Index]);
		}

		// SamplerType travels with the texture parameter, so copy it off the texture operand when it
		// has one. When it has none the property is left off entirely and the emitter calls
		// AutoSetSampleType, which is what the 1.x generator does for a declared texture property.
		if (Graph && Graph->Nodes.IsValidIndex(Texture.Node))
		{
			if (const FIRProperty* SamplerType = Graph->Nodes[Texture.Node].FindProperty(Prop::SamplerType))
			{
				Node.Properties.Add({ FString(Prop::SamplerType), SamplerType->Value });
			}
		}

		if (Level.IsValid())
		{
			Node.Properties.Add({ FString(Prop::MipValueMode), FIRPropertyValue::MakeEnum(TEXT("MipLevel")) });
		}

		return FLoweredValue::Of(AddNode(MoveTemp(Node), Expr.Span));
	}

	// ---------------------------------------------------------------------------- conditionals

	FLoweredValue FIRBuilder::LowerConditional(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		const FConditionalExpr* Conditional = Expr.As<FConditionalExpr>();
		if (!Conditional || !Conditional->Condition || !Conditional->TrueValue || !Conditional->FalseValue)
		{
			return FLoweredValue();
		}

		const FIRValue Condition = LowerValue(*Conditional->Condition);
		const FLoweredValue TrueValue = LowerExpr(*Conditional->TrueValue);
		const FLoweredValue FalseValue = LowerExpr(*Conditional->FalseValue);

		if (TrueValue.IsMaterial() || FalseValue.IsMaterial() || TrueValue.IsAggregate() || FalseValue.IsAggregate())
		{
			return MergeValues(TrueValue, FalseValue, Condition, IsStaticCondition(Condition), Expr.Span, TEXT("?:"));
		}
		if (!TrueValue.IsValue() || !FalseValue.IsValue())
		{
			return FLoweredValue();
		}

		const int32 Width = FMath::Max(BoundExpr.Type.GraphComponentCount(), 1);
		const FIRValue TrueSide = CoerceToWidth(TrueValue.Value, Width, Expr.Span);
		const FIRValue FalseSide = CoerceToWidth(FalseValue.Value, Width, Expr.Span);
		return FLoweredValue::Of(MakeConditional(Condition, IsStaticCondition(Condition), TrueSide, FalseSide, Expr.Span));
	}

	// ------------------------------------------------------------------------------ assignment

	FLoweredValue FIRBuilder::LowerAssign(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		// Two spellings land here. `x = e` and `x += e` are an FAssignExpr; `++x`, `x++`, `--x` and
		// `x--` are an FUnaryExpr the binder also records as an Assign, with CoreOp saying which way
		// and the operand serving as both the target and the left operand (S-report #5).
		const FExpr* TargetExpr = nullptr;
		const FExpr* ValueExpr = nullptr;
		EIROp CompoundOp = EIROp::Count;
		bool bIsStep = false;
		bool bValueIsPrevious = false;

		if (const FAssignExpr* Assign = Expr.As<FAssignExpr>())
		{
			TargetExpr = Assign->Target.Get();
			ValueExpr = Assign->Value.Get();
			CompoundOp = CoreOpForAssign(Assign->Op);
			if (CompoundOp == EIROp::Count && Assign->Op != EAssignOp::Assign && BoundExpr.CoreOp != EIROp::Count)
			{
				// A compound operator the AST map does not name; take the binder's word for it.
				CompoundOp = BoundExpr.CoreOp;
			}
		}
		else if (const FUnaryExpr* Unary = Expr.As<FUnaryExpr>())
		{
			TargetExpr = Unary->Operand.Get();
			CompoundOp = (BoundExpr.CoreOp != EIROp::Count) ? BoundExpr.CoreOp : EIROp::Add;
			bIsStep = true;
			// `x++` is worth what x was; `++x` is worth what it became.
			bValueIsPrevious = Unary->Op == EUnaryOp::PostIncrement || Unary->Op == EUnaryOp::PostDecrement;
		}

		if (!TargetExpr || (!bIsStep && !ValueExpr))
		{
			return FLoweredValue();
		}

		FLValueRef Ref;
		if (!ResolveLValue(*TargetExpr, Ref))
		{
			if (Ref.bNestedAttribute)
			{
				ReportNestedAttributeWrite(*TargetExpr);
			}
			return FLoweredValue();
		}

		const int32 FirstNode = Graph ? Graph->Nodes.Num() : 0;
		FLoweredValue Previous;
		FLoweredValue Value;

		if (bIsStep)
		{
			Previous = LoadLValue(Ref, Expr.Span);
			if (!Previous.IsValue())
			{
				// `++x` reads x first: a read of what no path assigned is DSH4376, as anywhere else.
				if (Previous.IsNeverAssigned())
				{
					ReportUnsetRead(*TargetExpr, Ref);
				}
				return FLoweredValue();
			}
			const FIRValue One = MakeScalarConstant(1.0, Expr.Span);
			Value = FLoweredValue::Of(MakeCoreOp(CompoundOp, { Previous.Value, One }, GraphTypeOf(BoundExpr.Type), Expr.Span));
		}
		else
		{
			Value = LowerExpr(*ValueExpr);
			if (CompoundOp != EIROp::Count)
			{
				Previous = LoadLValue(Ref, Expr.Span);
				if (!Previous.IsValue() || !Value.IsValue())
				{
					// `x += e` reads x first -- the same DSH4376 as a plain read when nothing gave x a value.
					if (Previous.IsNeverAssigned())
					{
						ReportUnsetRead(*TargetExpr, Ref);
					}
					return FLoweredValue();
				}
				Value = FLoweredValue::Of(MakeCoreOp(
					CompoundOp,
					{ Previous.Value, Value.Value },
					GraphTypeOf(BoundExpr.Type),
					Expr.Span));
			}
			else if (Value.IsValue() && Ref.MaterialAttribute.IsEmpty() && Ref.SwizzleMask.IsEmpty())
			{
				// A plain store does not coerce, so a node of channel views the binder took as the whole
				// value this target is (FIX3-Binder rule 2) is widened here, as a declaration would be.
				const FBoundExpr* BoundValue = Bound(*ValueExpr);
				const int32 TargetWidth = DeclaredTypeOf(Ref).GraphComponentCount();
				if (BoundValue && BoundValue->Conversion == EIRConversion::DefaultOutput && TargetWidth > WidthOf(Value.Value))
				{
					Value = FLoweredValue::Of(ApplyConversion(Value.Value, EIRConversion::DefaultOutput, TargetWidth, Expr.Span));
				}
			}
		}

		StoreLValue(Ref, Value, Expr.Span);

		// The name a value is known by: the variable it was first assigned to, which is what the
		// layout hints match on and what a node's description shows.
		if (Ref.FieldPath.IsEmpty() && Ref.MaterialAttribute.IsEmpty() && Frames.IsValidIndex(Ref.FrameIndex) && Frames[Ref.FrameIndex]->Function)
		{
			const FFrame& Owner = *Frames[Ref.FrameIndex];
			const FString& Name = Ref.bParam
				? (Owner.Function->Params.IsValidIndex(Ref.BaseIndex) ? Owner.Function->Params[Ref.BaseIndex].Name : FString())
				: (Owner.Function->Locals.IsValidIndex(Ref.BaseIndex) ? Owner.Function->Locals[Ref.BaseIndex].Name : FString());
			SetDebugName(FirstNode, Name);
		}
		else if (!Ref.MaterialAttribute.IsEmpty())
		{
			SetDebugName(FirstNode, Ref.MaterialAttribute);
		}

		return bValueIsPrevious ? Previous : Value;
	}

	// ----------------------------------------------------------------------------- unset reads

	bool FIRBuilder::ReportUnsetRead(const FExpr& ReadExpr, const FLValueRef& Ref)
	{
		// A parameter is the caller's to fill (an `out` one nobody assigns is the binder's DSH6211); an
		// `inout` argument's copy is not a read the author wrote; an attribute is DSH4370's; and an arm the
		// compiler can tell may never run proves nothing about the paths the shader takes.
		if (!Ref.IsValid() || Ref.bParam || !Ref.MaterialAttribute.IsEmpty() || InOutArgumentDepth > 0 || IsInArmThatMayNotRun())
		{
			return false;
		}
		FLoweredValue* Slot = ResolveSlot(Ref);
		if (!Slot || !Slot->IsNeverAssigned())
		{
			return false;
		}

		// One mistake, one message: once said, the slot counts as assigned, so a second read of it on the
		// same path stays silent; and the per-site set keeps an unrolled loop, or a helper inlined twice,
		// from saying it once per copy.
		Slot->bNeverAssigned = false;
		bool bAlreadyReported = false;
		UnsetReadsReported.Add(&ReadExpr, &bAlreadyReported);
		if (bAlreadyReported)
		{
			return true;
		}

		FString Name;
		if (!TryFlattenWord(&ReadExpr, Name))
		{
			const FFrame& Owner = *Frames[Ref.FrameIndex];
			Name = (Owner.Function && Owner.Function->Locals.IsValidIndex(Ref.BaseIndex))
				? Owner.Function->Locals[Ref.BaseIndex].Name
				: FString();
		}
		Diagnostics.Error(TEXT("DSH4376"), ReadExpr.Span, FText::Format(
			LOCTEXT("IRBuilderUnsetLocalRead", "'{0}' is read here, but nothing gives it a value on any path that reaches this line; assign it first, or give it an initializer where it is declared."),
			FText::FromString(Name)));
		return true;
	}

	void FIRBuilder::ReportNestedAttributeWrite(const FExpr& Target)
	{
		FString Spelling;
		if (!TryFlattenWord(&Target, Spelling))
		{
			Spelling = TEXT("MaterialAttributes");
		}
		// CONTRACT section 6.2's field map records one value per attribute and has no record inside a
		// record, so a write into the material an attribute holds has no lowering -- and it must never be
		// taken for a write to the outer material's attribute of the same name, which it once was.
		Diagnostics.Error(TEXT("DSH4377"), Target.Span, FText::Format(
			LOCTEXT("IRBuilderNestedAttributeWrite", "'{0}' writes into the material held in an attribute, and an attribute takes one whole value, not a write to part of it; assign that attribute a whole material, or set the attribute on the material itself."),
			FText::FromString(Spelling)));
	}

	// --------------------------------------------------------------------------------- lvalues

	bool FIRBuilder::ResolveLValue(const FExpr& Expr, FLValueRef& Out)
	{
		const FExpr* Inner = Unparen(&Expr);
		if (!Inner)
		{
			return false;
		}

		const FBoundExpr* BoundExpr = Bound(*Inner);
		if (!BoundExpr)
		{
			return false;
		}

		switch (BoundExpr->Kind)
		{
		case EBoundExprKind::Local:
			Out.FrameIndex = Frames.Num() - 1;
			Out.bParam = false;
			Out.BaseIndex = BoundExpr->LocalSlot;
			return Out.IsValid();

		case EBoundExprKind::Param:
			Out.FrameIndex = Frames.Num() - 1;
			Out.bParam = true;
			Out.BaseIndex = BoundExpr->Index;
			return Out.IsValid();

		case EBoundExprKind::StructField:
		{
			const FExpr* Object = ObjectOf(*Inner);
			if (!Object || !ResolveLValue(*Object, Out))
			{
				return false;
			}
			Out.FieldPath.Add(BoundExpr->FieldIndex);
			return true;
		}

		case EBoundExprKind::IndexConst:
		{
			const FExpr* Object = ObjectOf(*Inner);
			if (!Object || !ResolveLValue(*Object, Out))
			{
				return false;
			}
			// An array element is a field; a vector component is a mask on the value itself.
			const FLoweredValue* Slot = ResolveSlot(Out);
			if (Slot && Slot->IsAggregate())
			{
				Out.FieldPath.Add(BoundExpr->FieldIndex);
			}
			else
			{
				Out.SwizzleMask = BoundExpr->Swizzle;
			}
			return true;
		}

		case EBoundExprKind::MaterialField:
		{
			const FExpr* Object = ObjectOf(*Inner);
			if (!Object || !ResolveLValue(*Object, Out))
			{
				return false;
			}
			if (!Out.MaterialAttribute.IsEmpty())
			{
				// An attribute of the material held in an attribute: `m.MaterialAttributes.Roughness`. The
				// field map records one value per attribute and has no record inside a record (CONTRACT
				// section 6.2), so there is no write to lower -- and resolving on used to overwrite the
				// attribute and land the write on m.Roughness. The caller refuses it (DSH4377); a READ of
				// the same spelling goes through the value and never asks.
				Out.bNestedAttribute = true;
				return false;
			}
			const FCatalogMaterialAttribute* Entry = Attribute(BoundExpr->FieldIndex);
			if (!Entry)
			{
				// Silent when there is no catalog at all (reported once for the module); an index the
				// catalog does not have means the bind and the build read different tables.
				if (Catalog)
				{
					Diagnostics.Error(TEXT("DSH4352"), Inner->Span, FText::Format(
						LOCTEXT("IRBuilderNoAttributeEntry", "The material attribute table has no entry {0}; the module was bound against a different catalog than this build is reading."),
						FText::AsNumber(BoundExpr->FieldIndex)));
				}
				return false;
			}
			Out.MaterialAttribute = Entry->Name;
			return true;
		}

		case EBoundExprKind::Swizzle:
		{
			const FExpr* Object = ObjectOf(*Inner);
			if (!Object || !ResolveLValue(*Object, Out))
			{
				return false;
			}
			Out.SwizzleMask = BoundExpr->Swizzle;
			return true;
		}

		default:
			return false;
		}
	}

	FLoweredValue FIRBuilder::LoadLValue(const FLValueRef& Ref, const FLangSpan& Span)
	{
		FLoweredValue* Slot = ResolveSlot(Ref);
		if (!Slot)
		{
			return FLoweredValue();
		}

		if (!Ref.MaterialAttribute.IsEmpty())
		{
			if (!Slot->IsMaterial())
			{
				return FLoweredValue();
			}
			// A compound assignment reads before it writes, and a read of an attribute is the read of
			// contract section 6.2 wherever it happens: the recorded value, the incoming material's
			// through GetMaterialAttributes, or DSH4370. Looking at the recorded values alone made
			// `m.Roughness *= 0.5` in a layer find nothing and leave the attribute untouched, silently.
			const int32 AttributeIndex = Catalog ? Catalog->FindMaterialAttribute(Ref.MaterialAttribute) : INDEX_NONE;
			FIRValue Field = FIRValue::None();
			if (AttributeIndex == INDEX_NONE || !ReadMaterialField(Slot->Material, AttributeIndex, Span, Field))
			{
				return FLoweredValue();
			}
			return FLoweredValue::Of(Ref.SwizzleMask.IsEmpty() ? Field : MakeSwizzle(Field, Ref.SwizzleMask, Span));
		}

		if (!Ref.SwizzleMask.IsEmpty() && Slot->IsValue())
		{
			return FLoweredValue::Of(MakeSwizzle(Slot->Value, Ref.SwizzleMask, Span));
		}

		return *Slot;
	}

	void FIRBuilder::StoreLValue(const FLValueRef& Ref, const FLoweredValue& Value, const FLangSpan& Span)
	{
		FLoweredValue* Slot = ResolveSlot(Ref);
		if (!Slot)
		{
			return;
		}

		if (!Ref.MaterialAttribute.IsEmpty())
		{
			if (!Slot->IsMaterial())
			{
				*Slot = FLoweredValue::MakeMaterial();
			}
			// An attribute holds one graph value. The attribute that carries a whole set --
			// `m.MaterialAttributes = UE.BlendMaterialAttributes(...)` -- is handed a `material`, which
			// is one value only once it is made into one; skipping it for not being a Value would lose
			// the write without a word.
			FIRValue Stored = Value.IsValue() ? Value.Value : FIRValue::None();
			if (Value.IsMaterial())
			{
				Stored = MaterialiseMaterial(Value.Material, Span);
			}
			if (!Stored.IsValid())
			{
				return;
			}
			if (!Ref.SwizzleMask.IsEmpty())
			{
				// `m.BaseColor.x = e` (or `m.BaseColor[0] = e`) writes one component, so it is a
				// read-modify-write of the attribute, like `v.x = e` of a local: the other components are
				// what the attribute already holds -- recorded, read off the incoming material, or DSH4370
				// when it holds nothing yet. Recording e as the whole attribute set every component to it.
				const int32 AttributeIndex = Catalog ? Catalog->FindMaterialAttribute(Ref.MaterialAttribute) : INDEX_NONE;
				FIRValue Current = FIRValue::None();
				if (AttributeIndex == INDEX_NONE || !ReadMaterialField(Slot->Material, AttributeIndex, Span, Current))
				{
					return;
				}
				const FCatalogMaterialAttribute* Entry = Attribute(AttributeIndex);
				const int32 AttributeWidth = Entry ? Entry->ValueType.GraphComponentCount() : 0;
				Stored = WriteComponents(Current, AttributeWidth > 0 ? AttributeWidth : WidthOf(Current), Ref.SwizzleMask, Stored, Span);
			}
			if (!Slot->Material.Fields.Contains(Ref.MaterialAttribute))
			{
				Slot->Material.Order.Add(Ref.MaterialAttribute);
			}
			Slot->Material.Fields.Add(Ref.MaterialAttribute, Stored);
			return;
		}

		if (!Ref.SwizzleMask.IsEmpty())
		{
			if (!Value.IsValue())
			{
				return;
			}
			// `v.xz = e` rebuilds the whole value: every component of the old value survives except the
			// ones the mask names, which come from e in mask order. The width is the DECLARED one, and a
			// slot nothing has assigned yet starts from zero -- the value the 1.x generator gives a
			// declaration with no initializer (CreateDefaultValue) -- so `float3 v; v.x = a; v.y = b;`
			// keeps every write instead of shrinking v to the first one's width and dropping the rest.
			int32 Width = DeclaredTypeOf(Ref).GraphComponentCount();
			if (Width <= 0)
			{
				Width = Slot->IsValue() ? WidthOf(Slot->Value) : 0;
				for (int32 MaskIndex = 0; MaskIndex < Ref.SwizzleMask.Len(); ++MaskIndex)
				{
					Width = FMath::Max(Width, ComponentIndexOf(Ref.SwizzleMask[MaskIndex]) + 1);
				}
			}
			FIRValue Base = Slot->IsValue() ? Slot->Value : FIRValue::None();
			if (!Base.IsValid())
			{
				const double Zeroes[4] = { 0.0, 0.0, 0.0, 0.0 };
				Base = MakeConstant(Zeroes, Width, Span);
			}
			*Slot = FLoweredValue::Of(WriteComponents(Base, Width, Ref.SwizzleMask, Value.Value, Span));
			return;
		}

		*Slot = Value;
	}

	FIRValue FIRBuilder::WriteComponents(FIRValue Base, int32 Width, const FString& Mask, FIRValue Value, const FLangSpan& Span)
	{
		TArray<FIRValue> Components;
		Components.Reserve(Width);
		for (int32 Index = 0; Index < Width; ++Index)
		{
			int32 FromMask = INDEX_NONE;
			for (int32 MaskIndex = 0; MaskIndex < Mask.Len(); ++MaskIndex)
			{
				if (ComponentIndexOf(Mask[MaskIndex]) == Index)
				{
					FromMask = MaskIndex;
					break;
				}
			}
			Components.Add(FromMask != INDEX_NONE
				? ExtractComponent(Value, FromMask, Span)
				: ExtractComponent(Base, Index, Span));
		}
		return MakeAppend(Components, Span);
	}

	// --------------------------------------------------------------------------------- globals

	FLoweredValue FIRBuilder::GlobalValue(int32 GlobalIndex, const FLangSpan& Span)
	{
		if (const FLoweredValue* Existing = GlobalValues.Find(GlobalIndex))
		{
			// One node per global per graph: two reads of `uniform float Roughness` are one
			// ScalarParameter, which is what the dedupe key would collapse them to anyway.
			return *Existing;
		}
		if (!BoundModule.Globals.IsValidIndex(GlobalIndex))
		{
			return FLoweredValue();
		}

		// A uniform declared inside `#pragma region Parameters` puts its parameter node in that box.
		// The binder keys file-scope declarations into StatementRegions by their FDecl (S-report #9),
		// so the node takes the DECLARATION's region rather than the region of whichever statement
		// happened to read the uniform first -- which, since the node is made once and memoised,
		// would otherwise be an arbitrary choice.
		const int32 SavedRegion = CurrentRegion;
		if (BoundModule.Globals[GlobalIndex].Decl)
		{
			if (const int32* DeclRegion = BoundModule.StatementRegions.Find(BoundModule.Globals[GlobalIndex].Decl))
			{
				CurrentRegion = *DeclRegion;
			}
		}

		const int32 FirstNewNode = Graph ? Graph->Nodes.Num() : 0;
		const FLoweredValue Value = MakeGlobalValue(GlobalIndex, Span);
		CurrentRegion = SavedRegion;

		// Same reasoning for the file and the call site: the node belongs to the declaration, not to the
		// first reader. Made inside an inlined helper it would otherwise say `via` that call, or name the
		// helper's header as the file of a uniform the product's own file declares.
		if (Graph)
		{
			const FString& DeclarationFile = BoundModule.Globals[GlobalIndex].File;
			for (int32 NodeIndex = FirstNewNode; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
			{
				FIRSourceRef& Source = Graph->Nodes[NodeIndex].Source;
				if (!DeclarationFile.IsEmpty())
				{
					Source.File = DeclarationFile;
				}
				Source.CallSite = FLangSpan();
				Source.CallSiteFile.Reset();
			}
		}

		GlobalValues.Add(GlobalIndex, Value);
		return Value;
	}
}

#undef LOCTEXT_NAMESPACE
