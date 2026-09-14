// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FIREmitter: the per-product emitter state, shared by the five DreamShaderIREmitter*.cpp units.
//
// One instance per product. It owns the asset being built (exactly one of Material /
// MaterialFunction is set, the same way FCodeGraphBuilder does), the node -> value table the walk
// fills in, and the three side tables the tail of the emit needs: the layout maps (variable name ->
// expression, variable name -> region name, which is the shape 1.x LayoutGeneratedExpressions
// takes) and the span map WriteDreamShaderSourceSpans writes.
//
// FEmittedValue is the 2.0 FCodeValue, minus everything the IR already settled. 1.x had to carry a
// component count, a texture flag, a MaterialAttributes flag, a Substrate flag and an inline mask
// on every value because it was inferring types as it built the graph. The IR knows all of that
// before the emitter starts, so a value here is only (expression, output index) -- and the inline
// mask is gone for good: a Swizzle is a ComponentMask node, never an FExpressionInput mask
// (plan §3.3, CONTRACT §6.6).

#pragma once

#include "CoreMinimal.h"

#include "Compiler/DreamShaderIREmitter.h"
#include "DreamShaderDiagnostic.h"
#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "Lang/LangDiagnostic.h"

// ECustomMaterialOutputType is declared here, and TryResolveCustomOutputTypeFromIR below takes it
// by reference. An unscoped enum cannot be used through a forward declaration in every context the
// engine puts it in (TEnumAsByte), and this is the same include the 1.x
// MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h makes for the same reason.
#include "Materials/MaterialExpressionCustom.h"
#include "SceneTypes.h"

class FProperty;
class UClass;
class UMaterial;
class UMaterialExpression;
class UMaterialExpressionCustom;
class UMaterialExpressionFunctionInput;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionMaterialFunctionCall;
class UMaterialFunction;
struct FExpressionInput;

namespace UE::DreamShader::Editor::Compiler
{
	/** One output of one emitted expression: the whole of what an IR value becomes. */
	struct FEmittedValue
	{
		UMaterialExpression* Expression = nullptr;
		int32 OutputIndex = 0;

		bool IsValid() const { return Expression != nullptr; }
	};

	/**
	 * What one IR node became.
	 *
	 * OutputIndices maps an IR output SLOT to the engine output INDEX, and it exists because the two
	 * disagree on real classes. TextureSample is the plain example: the IR names its outputs
	 * RGBA,R,G,B,A (CONTRACT §6.5) while UMaterialExpressionTextureSample publishes them as
	 * RGB,R,G,B,A,RGBA -- slot 0 and index 0 are different values, and a wire built on the
	 * assumption that they are the same is the "SceneTexture Color.r became InvSize" failure with a
	 * different name. Empty means identity, which is every node whose outputs the catalog read off
	 * the same GetOutputs() the emitted node has.
	 */
	struct FEmittedNode
	{
		UMaterialExpression* Expression = nullptr;
		TArray<int32> OutputIndices;
	};

	/**
	 * The walk over one product's graph.
	 *
	 * Every Emit* method answers false having already raised exactly one diagnostic, so a caller
	 * propagates with a bare `return false` and the top of the emit rolls the asset back. Nothing
	 * here asserts: a malformed graph is the validator's failure to report, not a reason to take
	 * the editor down with it.
	 */
	class FIREmitter
	{
	public:
		FIREmitter(
			UMaterial* InMaterial,
			UMaterialFunction* InMaterialFunction,
			const IR::FIRProduct& InProduct,
			const FIREmitContext& InContext,
			Lang::FLangDiagnosticSink& InDiagnostics)
			: Material(InMaterial)
			, MaterialFunction(InMaterialFunction)
			, Product(InProduct)
			, Context(InContext)
			, Diagnostics(InDiagnostics)
		{
		}

		/** Emits every node in topological order. Statements (MaterialSink, FunctionOutput) are emitted last by construction: nothing reads them. */
		bool EmitGraph();

		/** name -> expression, for the 1.x layout's explicit `#pragma layout` entries and its region boxes. */
		const TMap<FString, UMaterialExpression*>& GetExpressionsByVariable() const { return ExpressionsByVariable; }
		/** name -> innermost region name, the shape LayoutGeneratedExpressions takes. */
		const TMap<FString, FString>& GetRegionByVariable() const { return RegionByVariable; }
		/** MaterialExpressionGuid -> where the node came from, for DreamShader.SourceSpans. */
		const TMap<FGuid, IR::FIRSourceRef>& GetSourceSpans() const { return SourceSpans; }

		/** The FunctionInput expressions in FIRGraph::FunctionInputs order; the ThinCustom/function paths restore their pin ids from these. */
		const TArray<UMaterialExpressionFunctionInput*>& GetFunctionInputExpressions() const { return FunctionInputExpressions; }
		const TArray<UMaterialExpressionFunctionOutput*>& GetFunctionOutputExpressions() const { return FunctionOutputExpressions; }

		/** Existing pin ids cached off the live asset BEFORE the rollback detached it; see CacheInterfaceIds. */
		void SetExistingInterfaceIds(TMap<FName, FGuid>&& InInputIds, TMap<FName, FGuid>&& InOutputIds)
		{
			ExistingInputIdsByName = MoveTemp(InInputIds);
			ExistingOutputIdsByName = MoveTemp(InOutputIds);
		}

	private:
		// ----------------------------------------------------------------- node emission (Nodes.cpp)
		bool EmitNode(int32 NodeIndex);
		bool EmitConstant(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitReflected(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitCoreMathOp(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitSwizzle(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitAppend(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitSelect(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitCompare(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitStaticSwitch(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitTextureSample(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitCustom(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitPassthrough(int32 NodeIndex, const IR::FIRNode& Node);
		/** The core ops whose FIRCoreOpInfo::ExpressionClass is nullptr and that the emitter lowers by hand. */
		bool EmitHandLoweredCoreOp(int32 NodeIndex, const IR::FIRNode& Node, const IR::FIRCoreOpInfo& Info);

		// ------------------------------------------------------- parameters (Parameters.cpp)
		bool EmitParameter(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitTextureParameter(int32 NodeIndex, const IR::FIRNode& Node);
		/** ParameterName / Group / Desc / SortPriority / SliderMin / SliderMax / every Passthrough property, all by reflection. */
		bool ApplyParameterMetadata(UMaterialExpression* Expression, const IR::FIRNode& Node);

		// -------------------------------------------------------- functions (Functions.cpp)
		bool EmitFunctionInput(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitFunctionOutput(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitFunctionCall(int32 NodeIndex, const IR::FIRNode& Node);

		// --------------------------------------------------------- material (Material.cpp)
		bool EmitMakeMaterialAttributes(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitSetMaterialAttributes(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitGetMaterialAttributes(int32 NodeIndex, const IR::FIRNode& Node);
		bool EmitMaterialSink(int32 NodeIndex, const IR::FIRNode& Node);

		// ------------------------------------------------------------------------- helpers
		UMaterialExpression* CreateExpression(UClass* ExpressionClass, const IR::FIRNode& Node);
		/** Records the value a node produced and everything the tail of the emit needs about it. */
		void RegisterNode(int32 NodeIndex, const IR::FIRNode& Node, UMaterialExpression* Expression);
		/** As RegisterNode, with an explicit IR output slot -> engine output index map. */
		void RegisterNode(int32 NodeIndex, const IR::FIRNode& Node, UMaterialExpression* Expression, TArray<int32>&& OutputIndices);
		/** Maps the node's OutputNames onto the expression's own outputs by name; the map is identity when a name has no match. */
		void RegisterNodeWithNamedOutputs(int32 NodeIndex, const IR::FIRNode& Node, UMaterialExpression* Expression);
		/** Looks up an already-emitted value. Raises DSH8225 and answers false when the walk order was wrong. */
		bool ResolveValue(const IR::FIRNode& Node, IR::FIRValue Value, FEmittedValue& OutValue);
		bool ResolveOperand(const IR::FIRNode& Node, int32 OperandIndex, FEmittedValue& OutValue);
		/** The 2.0 ConnectCodeValueToInput: connect, and clear any mask the pin carried. */
		static void ConnectValueToInput(FExpressionInput& Input, const FEmittedValue& Value);
		/** Connects an operand straight into a named pin of an expression, by reflection. */
		bool ConnectNamedInput(const IR::FIRNode& Node, UMaterialExpression* Expression, const FString& PinName, const FEmittedValue& Value);
		/** As ConnectNamedInput, but answers false without raising when the class has no such pin. */
		static bool TryConnectNamedInput(UMaterialExpression* Expression, const FString& PinName, const FEmittedValue& Value);
		/** A `Const`-twin literal or any other reflected property, written by the 1.x literal writer. */
		bool ApplyReflectedProperty(const IR::FIRNode& Node, UMaterialExpression* Expression, const IR::FIRProperty& Property, bool bWarnWhenMissing);
		/** A scalar Constant node, for the hand-lowered ops that need a literal operand. */
		UMaterialExpression* CreateScalarConstant(const IR::FIRNode& Node, double Value);

		/** `MP_BaseColor` for `BaseColor`, through the catalog's attribute table. */
		bool ResolveMaterialAttribute(const IR::FIRNode& Node, const FString& AttributeName, EMaterialProperty& OutProperty);

		/** The unique key both layout maps use: the DebugName when there is one, else a synthetic `$<index>`. */
		FString MakeLayoutKey(int32 NodeIndex, const IR::FIRNode& Node) const;

		// ----------------------------------------------------------------- diagnostics
		bool Fail(const TCHAR* Code, const IR::FIRNode& Node, const FText& Message);
		bool Fail(const TCHAR* Code, const IR::FIRSourceRef& Source, const FText& Message);
		void Warn(const TCHAR* Code, const IR::FIRNode& Node, const FText& Message);

		UMaterial* Material = nullptr;
		UMaterialFunction* MaterialFunction = nullptr;
		const IR::FIRProduct& Product;
		const FIREmitContext& Context;
		Lang::FLangDiagnosticSink& Diagnostics;

		/** Per IR node index: what it became. Named EmittedNodes, not Nodes, so it can never be misread as the graph's own Nodes array. */
		TArray<FEmittedNode> EmittedNodes;
		TMap<FString, UMaterialExpression*> ExpressionsByVariable;
		TMap<FString, FString> RegionByVariable;
		TMap<FGuid, IR::FIRSourceRef> SourceSpans;
		TArray<UMaterialExpressionFunctionInput*> FunctionInputExpressions;
		TArray<UMaterialExpressionFunctionOutput*> FunctionOutputExpressions;
		TMap<FName, FGuid> ExistingInputIdsByName;
		TMap<FName, FGuid> ExistingOutputIdsByName;

		/**
		 * Node placement while building. The layout pass rewrites every position afterwards, so this
		 * only has to keep the pre-layout graph from stacking every node on one point -- which is
		 * what FCodeGraphBuilder::ConsumeNodeY did, and for the same reason.
		 */
		int32 NextNodeY = -120;
		int32 ConsumeNodeY() { const int32 Y = NextNodeY; NextNodeY += 180; return Y; }
	};

	// ------------------------------------------------------------------ shared free helpers

	/**
	 * Renders an IR property value as the literal text the 1.x reflection writer
	 * (SetMaterialExpressionLiteralProperty) parses: `true`/`false`, a decimal, an enumerator
	 * spelling, an object path, a struct's ImportText form.
	 *
	 * NOT FIRPropertyValue::ToString(): that one exists for dumps and dedupe keys and is free to
	 * decorate. This one has to round-trip through ParseBooleanLiteral / ParseScalarLiteral /
	 * TryResolveEnumLiteral / TryResolveDreamShaderAssetReference.
	 */
	FString FormatIRPropertyForReflection(const IR::FIRPropertyValue& Value);

	/** `Float1`..`Float4` / `MaterialAttributes` -> ECustomMaterialOutputType. False for anything else. */
	bool TryResolveCustomOutputTypeFromIR(const FString& Spelling, ECustomMaterialOutputType& OutType);

	/** The IR InputType spelling -> EFunctionInputType, by the enum's own DisplayName (`Vector3`, `StaticBool`, ...). */
	bool TryResolveFunctionInputTypeFromIR(const FString& Spelling, int32& OutInputTypeValue);

	/** Looks up an attribute in the catalog and answers its `MP_*` enumerator. */
	bool TryResolveMaterialPropertyFromCatalog(const IR::FBuiltinCatalog& Catalog, const FString& AttributeName, EMaterialProperty& OutProperty);

	/**
	 * THE swizzle rule, in the one place it is decided (plan §3.3 as amended by CONTRACT §6.13 #22).
	 *
	 * A Swizzle is never an inline FExpressionInput mask. Every pin the emitter connects is written
	 * with Mask = 0 (FIREmitter::ConnectValueToInput), so UMaterialGraph::GetValidOutputIndex -- which
	 * re-points a masked wire on output 0 at whichever output matches its mask when the material
	 * editor rebuilds the graph (DSK2) -- never has anything to re-point. A Swizzle is one of two
	 * things instead:
	 *
	 * - OUTPUT SELECTION, when the mask is a strictly ascending prefix narrower than its operand
	 *   (`x`, `xy`, `xyz`), the operand is the whole of its expression's output, and the expression
	 *   publishes a NAMED output with exactly those leading channels (`R` / `RGB` on VectorParameter,
	 *   TextureSample, Constant4Vector, ParticleColor, DecalColor, ...). The wire leaves that pin,
	 *   which is exactly what the editor writes when a user drags from it. No engine class that
	 *   publishes such a named output mixes it with an unmasked output of a different value, so this
	 *   never re-points a read at a different value (checked against 5.8's MaterialExpressions.cpp).
	 * - a ComponentMask node, in every other case.
	 *
	 * Answers true, with the engine output index, for the first case. Exposed so that anything else
	 * that has to judge a graph by the same rule -- the parity oracle normalising a 1.x graph's inline
	 * masks -- asks the emitter rather than restating it.
	 */
	bool TryResolveSwizzleAsNamedOutput(
		const UMaterialExpression* Expression,
		int32 OperandOutputIndex,
		int32 OperandWidth,
		const FString& Mask,
		int32& OutOutputIndex);
}
