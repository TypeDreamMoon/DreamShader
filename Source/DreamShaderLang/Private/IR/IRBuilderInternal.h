// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FIRBuilder: the state one BuildDreamShaderIR call carries, shared by the five builder .cpp files.
//
// The shape of the thing, in one paragraph. One FIRBuilder exists per module and is re-pointed at
// each product in turn (Graph is the product being built). Lowering a function pushes an FFrame:
// its locals and parameters are slots holding FLoweredValue, which is either a graph value, an
// aggregate (a user struct, or an array) or a material attribute map. An assignment rebinds a slot;
// nothing mutates a node. An `if` snapshots every frame's slots, lowers both arms, and merges the
// slots that disagree into StaticSwitch / Select / Compare. A `return` does not jump: it records a
// pending exit -- the slot state and the condition that reached it -- and the end of the function
// folds the pending exits into the final state in reverse order, which is what `if (c) return a;
// ... return b;` means in a DAG.
//
// Nothing here allocates a node without going through AddNode, because AddNode is the one place
// FIRNode::Source, Region and DebugName are stamped.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "IR/IR.h"
#include "IR/IRBuilder.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

namespace UE::DreamShader::IR::Private
{
	using namespace UE::DreamShader::Lang;

	/** `xyzw` -- the canonical swizzle alphabet; the binder rewrote `rgba` onto it at bind time. */
	extern const TCHAR* const GSwizzleComponents;
	/** 'y' -> 1. INDEX_NONE for anything that is not a canonical component letter. */
	int32 ComponentIndexOf(TCHAR Component);
	/** Through FParenExpr, to the expression that actually says something. */
	const FExpr* Unparen(const FExpr* Expr);

	/** The attribute map a `material` value is while lowering (contract section 6.2). */
	struct FMaterialValue
	{
		/** Attribute names in first-write order; MaterialSink inputs follow this order. */
		TArray<FString> Order;
		/** Attribute name (the catalog's canonical spelling) -> the value written to it. */
		TMap<FString, FIRValue> Fields;
		/**
		 * Set when the material arrived through a pin (a Layer's or a LayerBlend's MaterialAttributes
		 * FunctionInput). Reading an attribute this map has not written then makes GetMaterialAttributes
		 * against it instead of DSH4370.
		 */
		FIRValue Source;
		/** The GetMaterialAttributes node made for Source, so one material breaks once. */
		int32 BreakNode = INDEX_NONE;

		bool HasSource() const { return Source.IsValid(); }
		bool operator==(const FMaterialValue& Other) const;
	};

	/**
	 * What a slot holds. `Value` is the only kind that can reach a pin; `Aggregate` (a user struct
	 * or an array) is flattened per field at every use and never becomes a node; `Material` is the
	 * attribute map above and becomes a node only when it crosses into a pin.
	 */
	struct FLoweredValue
	{
		enum class EKind : uint8
		{
			/** No value: an uninitialised local, a void call, an expression that already reported. */
			Empty,
			Value,
			Aggregate,
			Material,
		};

		EKind Kind = EKind::Empty;
		FIRValue Value;
		TArray<FLoweredValue> Fields;
		FMaterialValue Material;
		/**
		 * Empty because nothing has assigned the slot on any path that reaches here -- a local declared
		 * without an initializer -- rather than because what was assigned already reported. Reading a
		 * slot in this state is DSH4376; the other kind of Empty stays silent, one mistake one message.
		 * Not part of operator==: to a merge, two Empty slots are the same value.
		 */
		bool bNeverAssigned = false;

		bool IsEmpty() const { return Kind == EKind::Empty; }
		bool IsNeverAssigned() const { return Kind == EKind::Empty && bNeverAssigned; }
		bool IsValue() const { return Kind == EKind::Value && Value.IsValid(); }
		bool IsAggregate() const { return Kind == EKind::Aggregate; }
		bool IsMaterial() const { return Kind == EKind::Material; }

		static FLoweredValue NeverAssigned()
		{
			FLoweredValue Result;
			Result.bNeverAssigned = true;
			return Result;
		}
		static FLoweredValue Of(FIRValue InValue)
		{
			FLoweredValue Result;
			Result.Kind = EKind::Value;
			Result.Value = InValue;
			return Result;
		}
		static FLoweredValue MakeAggregate(int32 Num)
		{
			FLoweredValue Result;
			Result.Kind = EKind::Aggregate;
			Result.Fields.SetNum(Num);
			return Result;
		}
		static FLoweredValue MakeMaterial(FIRValue InSource = FIRValue::None())
		{
			FLoweredValue Result;
			Result.Kind = EKind::Material;
			Result.Material.Source = InSource;
			return Result;
		}
		/**
		 * A node output as a slot holds it. A MaterialAttributes output is a `material` that arrived
		 * through a pin -- an empty map whose Source is that output, so a field read breaks it open with
		 * GetMaterialAttributes and a write sets on top of it, exactly as for a layer's input (contract
		 * section 6.2) -- and anything else is a plain value. Every producer of a value whose type may
		 * be `material` goes through here: held as a plain Value, a material has no fields the rest of
		 * the builder can read, and a read of one used to lower to nothing without a word.
		 */
		static FLoweredValue OfOutput(FIRValue InValue, const FIRType& InType)
		{
			return (InType.IsMaterial() && InValue.IsValid()) ? MakeMaterial(InValue) : Of(InValue);
		}

		bool operator==(const FLoweredValue& Other) const;
		bool operator!=(const FLoweredValue& Other) const { return !(*this == Other); }
	};

	/**
	 * Where an assignment lands, as a path rather than a pointer: the slots live in TArrays that a
	 * branch snapshot reassigns wholesale, so a pointer taken before a branch would dangle. Resolve
	 * it again at every use.
	 */
	struct FLValueRef
	{
		int32 FrameIndex = INDEX_NONE;
		/** The base slot is a parameter rather than a local. */
		bool bParam = false;
		int32 BaseIndex = INDEX_NONE;
		/** Aggregate field / array element indices from the base down to the slot. */
		TArray<int32> FieldPath;
		/** Non-empty: the base resolves to a material and this attribute is what is written. */
		FString MaterialAttribute;
		/** Non-empty: a read-modify-write through this component mask. */
		FString SwizzleMask;
		/**
		 * Set, with ResolveLValue answering false, when the target is an attribute of a material that is
		 * itself held in an attribute (`m.MaterialAttributes.Roughness`): the caller refuses it, DSH4377.
		 */
		bool bNestedAttribute = false;

		bool IsValid() const { return FrameIndex != INDEX_NONE && BaseIndex != INDEX_NONE; }
	};

	/** One exit of a function body: the slot state that reached a `return`, and under what condition. */
	struct FPendingExit
	{
		/** Invalid for an unconditional exit. */
		FIRValue Condition;
		bool bStaticCondition = false;
		Lang::FLangSpan Span;
		TArray<FLoweredValue> Locals;
		TArray<FLoweredValue> Params;
		FLoweredValue ReturnValue;
	};

	/** One function being lowered: the product's own function, or a helper being inlined into it. */
	struct FFrame
	{
		const FBoundFunction* Function = nullptr;
		int32 FunctionIndex = INDEX_NONE;
		TArray<FLoweredValue> Locals;
		TArray<FLoweredValue> Params;
		/** Parallel to Params: where an `out`/`inout` parameter writes back, in the caller. */
		TArray<FLValueRef> OutTargets;
		TArray<FPendingExit> PendingExits;
		FLoweredValue ReturnValue;
		bool bReturned = false;
		/**
		 * Every arm that could still reach the end of the body has returned, so the live slots hold
		 * values nothing can observe. ResolveExits then starts from the last pending exit instead of
		 * from them, which keeps a dead `else` out of the graph.
		 */
		bool bStateDead = false;
		/** The call this frame was inlined at; unset for a product's own function. */
		Lang::FLangSpan CallSite;
		bool bHasCallSite = false;
	};

	/** One arm of one enclosing `if`, while its body is being lowered. */
	struct FConditionEntry
	{
		FIRValue Value;
		bool bStatic = false;
		/** This is the `else` arm: the predicate is the negation. */
		bool bNegated = false;
	};

	/** A snapshot of every frame's slots, taken around a branch. */
	struct FEnvSnapshot
	{
		TArray<TArray<FLoweredValue>> Locals;
		TArray<TArray<FLoweredValue>> Params;
	};

	class FIRBuilder
	{
	public:
		FIRBuilder(const FBoundModule& InBound, const FIRBuildOptions& InOptions, FLangDiagnosticSink& InDiagnostics);

		void BuildModule(FIRModule& OutModule);

	private:
		// ------------------------------------------------------------------------------ products
		void BuildProduct(const FBoundProduct& BoundProduct, FIRProduct& OutProduct);
		void BuildEntryProduct(const FBoundFunction& Function);
		void BuildLayerProduct(const FBoundFunction& Function, bool bIsBlend);
		void BuildFunctionProduct(const FBoundFunction& Function);
		/** The FunctionInput node for one parameter, plus the slot value it seeds. */
		FLoweredValue MakeFunctionInput(const FBoundFunction& Function, int32 ParamIndex, int32 SortPriority);
		void MakeFunctionOutput(const FString& Name, const FString& Description, int32 SortPriority, const FLoweredValue& Value, const Lang::FLangSpan& Span);
		/** Prop::InputType for a parameter type; empty (and DSH4364) when the type has no function-input form. */
		FString FunctionInputTypeName(const FIRType& Type, const Lang::FLangSpan& Span);

		// -------------------------------------------------------------------------------- frames
		FFrame& Frame() { return *Frames.Last(); }
		const FFrame& Frame() const { return *Frames.Last(); }
		void PushFrame(const FBoundFunction& Function, int32 FunctionIndex);
		void PopFrame();
		FLoweredValue* ResolveSlot(const FLValueRef& Ref);
		/** The declared type of the slot a reference names, before its attribute or mask; Error when it cannot be told. */
		FIRType DeclaredTypeOf(const FLValueRef& Ref) const;
		/**
		 * What a local holds before anything assigns it: never-assigned leaves (a read of one is DSH4376)
		 * inside the shape its type needs -- an aggregate per struct and per array, fields in place -- and
		 * an empty map for a `material`.
		 */
		FLoweredValue MakeUnassigned(const FIRType& Type, int32 ArrayCount, int32 Depth = 0) const;
		/**
		 * The call site stamped on nodes while inlining: the OUTERMOST call, plus the file the caller
		 * was written in, so navigation lands in the product's own body rather than in a helper that
		 * a helper called. False, and both outputs untouched, outside inlining.
		 */
		bool ActiveCallSite(Lang::FLangSpan& OutSpan, FString& OutFile) const;

		// -------------------------------------------------------------------------------- nodes
		FIRValue AddNode(FIRNode&& Node, const Lang::FLangSpan& Span);
		FIRValue MakeConstant(const double* Components, int32 Num, const Lang::FLangSpan& Span);
		FIRValue MakeScalarConstant(double Value, const Lang::FLangSpan& Span);
		FIRValue MakeCoreOp(EIROp Op, TArray<FIRValue> Operands, const FIRType& Result, const Lang::FLangSpan& Span);
		FIRValue MakeSwizzle(FIRValue Value, const FString& Mask, const Lang::FLangSpan& Span);
		FIRValue MakeAppend(const TArray<FIRValue>& Parts, const Lang::FLangSpan& Span);
		FIRValue MakeBroadcast(FIRValue Value, int32 Width, const Lang::FLangSpan& Span);
		/**
		 * Widen or narrow a value to an exact component count, the way 1.x's CoerceValueToType does:
		 * a scalar broadcasts, a wider value takes its leading components, equal widths pass through.
		 */
		FIRValue CoerceToWidth(FIRValue Value, int32 Width, const Lang::FLangSpan& Span);
		/** Component `Index` of a value, as a Swizzle (or the value itself when it is already a scalar). */
		FIRValue ExtractComponent(FIRValue Value, int32 Index, const Lang::FLangSpan& Span);
		/**
		 * `Base` with the components `Mask` names replaced by `Value`'s, in mask order, rebuilt as `Width`
		 * components: the read-modify-write behind `v.xz = e` and `m.BaseColor.x = e`.
		 */
		FIRValue WriteComponents(FIRValue Base, int32 Width, const FString& Mask, FIRValue Value, const Lang::FLangSpan& Span);

		/** `RGB` -> 0b0111: the channels an output name spells, or 0 when the name is not a channel set. */
		static int32 ChannelMaskOfOutputName(const FString& Name);
		/** A reflected node whose outputs are all channel views of one value (FIX3-Binder's test). */
		bool IsChannelViewNode(const FIRNode& Node) const;
		/** The output of a channel-view node carrying exactly these channels (bit 0 = R), or INDEX_NONE. */
		static int32 FindChannelViewSlot(const FIRNode& Node, int32 ChannelMask);
		/** The channel-view node an Append of its own views reads, or INDEX_NONE when it is anything else. */
		int32 ChannelViewAppendSource(FIRValue Value, int32 Depth) const;
		/**
		 * A channel view widened to the whole value it leads (FIX3-Binder rule 2): the view that is exactly
		 * the first `Width` channels when there is one, else an Append of the views covering them in order.
		 * FIRValue::None() when `Value` is not a leading view of such a node, or cannot be widened that far.
		 */
		FIRValue WidenChannelView(FIRValue Value, int32 Width, const Lang::FLangSpan& Span);

		/**
		 * `Condition ? True : False`. Static conditions become StaticSwitch; a condition that is a
		 * scalar comparison becomes Compare (the engine If, which is what 1.x emits); anything else
		 * becomes Select.
		 */
		FIRValue MakeConditional(
			FIRValue Condition,
			bool bStaticCondition,
			FIRValue TrueValue,
			FIRValue FalseValue,
			const Lang::FLangSpan& Span);

		int32 WidthOf(FIRValue Value) const;
		FIRType TypeOfValue(FIRValue Value) const;
		/** The graph's narrowing of a bound type: numerics become float1..4, bools stay bool, the rest passes. */
		FIRType GraphTypeOf(const FIRType& Type) const;
		/** True when every leaf of the value is a static bool Parameter (contract section 6.3). */
		bool IsStaticCondition(FIRValue Value) const;
		/** True when the value is built from Constant nodes alone, through ops the fold pass can fold. */
		bool IsCompileTimeConstant(FIRValue Value) const;
		/**
		 * The first component of a compile-time constant, evaluated the way the fold pass would. False when
		 * it is not one this can evaluate: not constant, an op it does not know, or past `Budget` steps.
		 */
		bool TryEvaluateConstant(FIRValue Value, double& OutValue, int32& Budget) const;
		/** Whether a branch condition holds, when the compiler can already tell; false when it cannot. */
		bool TryDecideCondition(FIRValue Condition, bool& bOutHolds) const;
		/**
		 * True while lowering an `if` arm that may never run: one under a constant condition that decides
		 * against it, or under a constant condition this cannot evaluate. `if (i > 0)` on the first trip of
		 * an unrolled loop is the case that matters.
		 */
		bool IsInArmThatMayNotRun() const;

		FIRValue ApplyConversion(FIRValue Value, EIRConversion Conversion, int32 TargetWidth, const Lang::FLangSpan& Span);
		/** A lowered value as the single graph value a reflected pin of this catalog type wants. */
		FIRValue ValueForPin(const FLoweredValue& Value, ECatalogValueType PinType, EIRConversion Conversion, const Lang::FLangSpan& Span);
		/** A lowered value as the single graph value a declared parameter type wants. */
		FIRValue ValueForParam(const FLoweredValue& Value, const FIRType& Target, EIRConversion Conversion, const Lang::FLangSpan& Span);

		// -------------------------------------------------------------------------- expressions
		FLoweredValue LowerExpr(const FExpr& Expr);
		/** LowerExpr plus "and it had better be a single graph value"; reports DSH4365 when it is not. */
		FIRValue LowerValue(const FExpr& Expr);
		/** LowerValue plus the conversion the binder recorded on this expression for its parent. */
		FIRValue LowerOperand(const FExpr& Expr, int32 TargetWidth);

		FLoweredValue LowerLiteral(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerCoreOp(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerConstructor(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerStructConstructor(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerInitializerList(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerReflectedCall(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerTextureSample(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerConditional(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerAssign(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue LowerSwizzle(const FExpr& Expr, const FBoundExpr& Bound);

		/** The AST argument an FBoundArgument points at. */
		const FExpr* ArgumentExpr(const FExpr& CallExpr, const FBoundArgument& Argument) const;
		/** The literal value of a property-bound argument, typed by the catalog. */
		bool MakePropertyValue(const FExpr& Value, ECatalogValueType Type, FIRPropertyValue& Out);

		bool ResolveLValue(const FExpr& Expr, FLValueRef& Out);
		void StoreLValue(const FLValueRef& Ref, const FLoweredValue& Value, const Lang::FLangSpan& Span);
		FLoweredValue LoadLValue(const FLValueRef& Ref, const Lang::FLangSpan& Span);
		/**
		 * DSH4376 for a read that found a slot no path has assigned -- unless the slot is a parameter's, an
		 * `inout` argument is being copied in, or the read sits in an arm that may never run. Said once per
		 * read site, and the slot then counts as assigned so the same mistake is not said twice. True when
		 * the read counts as reported, now or earlier.
		 */
		bool ReportUnsetRead(const FExpr& ReadExpr, const FLValueRef& Ref);
		/** DSH4377: a write into an attribute of the material held in an attribute. */
		void ReportNestedAttributeWrite(const FExpr& Target);

		// --------------------------------------------------------------------------- statements
		void LowerStatement(const FStmt& Stmt);
		void LowerBlock(const FBlockStmt& Block);
		void LowerVarDecl(const FVarDeclStmt& Stmt);
		void LowerIf(const FIfStmt& Stmt);
		void LowerLoop(const FStmt& Stmt, const FStmt* Init, const FStmt* Body, const FExpr* Step);
		void LowerReturn(const FReturnStmt& Stmt);
		/** The conjunction of every enclosing `if` arm; invalid when there is none (an unconditional exit). */
		FIRValue CurrentPredicate(bool& bOutStatic, const Lang::FLangSpan& Span);

		FEnvSnapshot Snapshot() const;
		void Restore(const FEnvSnapshot& State);
		/** Merge two post-branch states into the live frames; every slot that disagrees gets a conditional. */
		void MergeStates(
			const FEnvSnapshot& TrueState,
			const FEnvSnapshot& FalseState,
			FIRValue Condition,
			bool bStaticCondition,
			const Lang::FLangSpan& Span);
		FLoweredValue MergeValues(
			const FLoweredValue& TrueValue,
			const FLoweredValue& FalseValue,
			FIRValue Condition,
			bool bStaticCondition,
			const Lang::FLangSpan& Span,
			const FString& What);
		/** Folds the frame's pending exits into its live state; the last thing a function body does. */
		void ResolveExits();
		/** DSH4375: two different whole materials meet in a merge. `Name` empty for the unnamed message. */
		void ReportWholeMaterialMerge(const FString& Name, const Lang::FLangSpan& Span);

		// ----------------------------------------------------------------------------- material
		/** Read one attribute of a material slot. */
		bool ReadMaterialField(FMaterialValue& Material, int32 AttributeIndex, const Lang::FLangSpan& Span, FIRValue& Out);
		/** A material as one graph value: SetMaterialAttributes over its source, or MakeMaterialAttributes. */
		FIRValue MaterialiseMaterial(const FMaterialValue& Material, const Lang::FLangSpan& Span);
		FIRValue MakeSetMaterialAttributes(const FMaterialValue& Material, const Lang::FLangSpan& Span);
		void EmitMaterialSink(const FMaterialValue& Material, const Lang::FLangSpan& Span);
		const FCatalogMaterialAttribute* Attribute(int32 Index) const;

		// -------------------------------------------------------------------------- calls
		FLoweredValue LowerFunctionCall(const FExpr& Expr, const FBoundExpr& Bound);
		FLoweredValue InlineHelper(const FExpr& Expr, const FBoundExpr& Bound, const FBoundFunction& Callee, int32 CalleeIndex);
		FLoweredValue MakeCustomNode(const FExpr& Expr, const FBoundExpr& Bound, const FBoundFunction& Callee, int32 CalleeIndex);
		FLoweredValue MakeFunctionCallNode(const FExpr& Expr, const FBoundExpr& Bound, const FBoundFunction& Callee, int32 CalleeIndex);
		/** The arguments of a call, matched to the callee's parameters; missing ones take the default. */
		bool BindCallArguments(
			const FExpr& Expr,
			const FBoundExpr& Bound,
			const FBoundFunction& Callee,
			TArray<FLoweredValue>& OutValues,
			TArray<FLValueRef>& OutTargets);
		/** "Result" plus every out/inout parameter, in the order a product's FunctionOutputs use. */
		static void CollectCallOutputs(const FBoundFunction& Callee, TArray<FString>& OutNames, TArray<FIRType>& OutTypes);
		void WriteBackOutputs(const FBoundFunction& Callee, const TArray<FLValueRef>& Targets, FIRValue Node, const TArray<FString>& OutputNames, const Lang::FLangSpan& Span);

		// ------------------------------------------------------------------------------ globals
		FLoweredValue GlobalValue(int32 GlobalIndex, const Lang::FLangSpan& Span);
		/** The uncached half of GlobalValue: the Parameter / TextureParameter / Constant node itself. */
		FLoweredValue MakeGlobalValue(int32 GlobalIndex, const Lang::FLangSpan& Span);
		/** DefaultSortPriority: the uniform's place in declaration order, written when there is no `@sort`. */
		void ApplyParameterMetadata(FIRNode& Node, const FBoundDirectives& Directives, const FString& FallbackName, int32 DefaultSortPriority = INDEX_NONE);

		// -------------------------------------------------------------------------------- misc
		void SetDebugName(int32 FirstNode, const FString& Name);
		const FBoundExpr* Bound(const FExpr& Expr) const { return BoundModule.Expressions.Find(&Expr); }
		/** An expression with no bound record already reported; lowering it would double-report. */
		bool HasBinding(const FExpr& Expr) const { return BoundModule.Expressions.Contains(&Expr); }

		const FBoundModule& BoundModule;
		const FBuiltinCatalog* Catalog = nullptr;
		FIRBuildOptions Options;
		FLangDiagnosticSink& Diagnostics;

		/** The product being built. */
		FIRGraph* Graph = nullptr;
		FString SourceFile;
		/** Product index by bound function index, so a call to a same-file export finds Prop::LocalFunction. */
		TMap<int32, int32> ProductByFunction;

		TArray<TUniquePtr<FFrame>> Frames;
		/** Uniform/constant globals already lowered in this graph, by FBoundModule::Globals index. */
		TMap<int32, FLoweredValue> GlobalValues;

		TArray<FConditionEntry> ConditionStack;
		int32 CurrentRegion = INDEX_NONE;
		/** Inside an `if` arm: `break` / `continue` there cannot be unrolled (DSH4363). */
		int32 BranchDepth = 0;
		int32 LoopDepth = 0;
		bool bBreak = false;
		bool bContinue = false;
		/** Inside BindCallArguments' copy of an `inout` argument, where a read is not the author's (DSH4376). */
		int32 InOutArgumentDepth = 0;
		/** Read sites DSH4376 was said for, so an unrolled loop or a helper inlined twice says it once. */
		TSet<const FExpr*> UnsetReadsReported;
	};
}
