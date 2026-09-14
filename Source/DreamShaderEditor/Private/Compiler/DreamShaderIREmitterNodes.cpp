// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// One UMaterialExpression per IR node.
//
// The dispatch below is the whole of the emitter's "intelligence", and it is deliberately shallow:
// a structural op has one shape, a core math op reads its class and its pin names out of
// FIRCoreOpInfo, and the handful of core ops the engine has no node for are lowered here by hand in
// exactly the shape the 1.x generator produced -- so a `.dss` and its 1.x twin dump the same graph
// (CONTRACT §1, the parity test unit T writes).
//
// Nothing here infers a type. The IR carries Outputs[] on every node; if a width or a kind is
// needed it is read, never worked out.

#include "Compiler/DreamShaderIREmitterInternal.h"

#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeShared.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DreamShader.Emitter"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		/**
		 * Splits a pin name of the form `Inputs[2]` into `Inputs` and 2.
		 *
		 * A handful of engine classes declare their inputs as C arrays --
		 * UMaterialExpressionQualitySwitch, FeatureLevelSwitch, ShadingPathSwitch and
		 * MakeMaterialAttributes' CustomizedUVs[8]. UMaterialExpression::GetInput walks those by
		 * ArrayDim, so they are real, separately addressable pins, but a single FProperty covers all
		 * of them and FindMaterialExpressionArgumentProperty can only answer with that one property.
		 * So the catalog spells element i as `Name[i]` and this is where that spelling is undone.
		 */
		bool TrySplitIndexedPinName(const FString& PinName, FString& OutBaseName, int32& OutArrayIndex)
		{
			OutArrayIndex = 0;
			if (!PinName.EndsWith(TEXT("]")))
			{
				OutBaseName = PinName;
				return false;
			}

			FString Left;
			FString Right;
			if (!PinName.Split(TEXT("["), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				OutBaseName = PinName;
				return false;
			}

			Right.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			if (Right.IsEmpty() || !Right.IsNumeric())
			{
				OutBaseName = PinName;
				return false;
			}

			OutBaseName = Left;
			OutArrayIndex = FCString::Atoi(*Right);
			return true;
		}

		/** The engine's name for the leading N channels of a value: 1 -> "R", 2 -> "RG", ... */
		const TCHAR* GetLeadingChannelOutputName(const int32 ChannelCount)
		{
			switch (ChannelCount)
			{
			case 1:  return TEXT("R");
			case 2:  return TEXT("RG");
			case 3:  return TEXT("RGB");
			case 4:  return TEXT("RGBA");
			default: return nullptr;
			}
		}

		/** How many channels an output publishes, and whether they are the leading ones in order. */
		bool IsLeadingChannelOutput(const FExpressionOutput& Output, const int32 ChannelCount)
		{
			if (Output.Mask == 0)
			{
				// An unmasked output is the whole value, whatever its width.
				return false;
			}

			const bool bExpected[4] = { ChannelCount >= 1, ChannelCount >= 2, ChannelCount >= 3, ChannelCount >= 4 };
			return (Output.MaskR != 0) == bExpected[0]
				&& (Output.MaskG != 0) == bExpected[1]
				&& (Output.MaskB != 0) == bExpected[2]
				&& (Output.MaskA != 0) == bExpected[3];
		}

		/**
		 * Whether a value is the WHOLE of its expression's output rather than a slice of it.
		 *
		 * Only then can a swizzle be re-pointed at another of that expression's outputs: `.rgb` of a
		 * TextureSample's RGBA is the sample's RGB output, but `.r` of its G output is not its R
		 * output. An unmasked pin is whole by definition; a masked one is whole when its channels
		 * are exactly the value's own width.
		 */
		bool IsWholeValueOutput(const UMaterialExpression* Expression, const int32 OutputIndex, const int32 ValueWidth)
		{
			if (!Expression || !Expression->Outputs.IsValidIndex(OutputIndex))
			{
				return false;
			}

			const FExpressionOutput& Output = Expression->Outputs[OutputIndex];
			return Output.Mask == 0 || IsLeadingChannelOutput(Output, ValueWidth);
		}

		/** R/G/B/A flags out of a canonical lower-case `xyzw` subset. */
		bool TryResolveSwizzleMask(const FString& Mask, bool& bOutR, bool& bOutG, bool& bOutB, bool& bOutA)
		{
			bOutR = bOutG = bOutB = bOutA = false;
			if (Mask.IsEmpty() || Mask.Len() > 4)
			{
				return false;
			}

			for (int32 Index = 0; Index < Mask.Len(); ++Index)
			{
				switch (Mask[Index])
				{
				case TCHAR('x'): bOutR = true; break;
				case TCHAR('y'): bOutG = true; break;
				case TCHAR('z'): bOutB = true; break;
				case TCHAR('w'): bOutA = true; break;
				default:
					return false;
				}
			}

			return true;
		}
	}

	/**
	 * The output-selection half of the swizzle rule; the whole rule is written out where this is
	 * declared (DreamShaderIREmitterInternal.h). It lives outside the anonymous namespace so the parity
	 * oracle can judge a 1.x graph by this function instead of by a restatement of it.
	 *
	 * 1.x agrees with this for a float3 uniform only: it selected a VectorParameter's `RGB` output for
	 * the uniform's value (TryCreatePropertyValue, CodeProperties.cpp ~110). `.rgb` of a float4
	 * VectorParameter or of a TextureSample it wrote as an inline `rgb` mask on the RGBA output instead
	 * (41 VectorParameter wires in the 1.9.0 trial baseline) -- the form plan §3.3 retires, so that is
	 * a registered parity delta, not a shape to copy.
	 *
	 * The output must be genuinely NAMED, so VertexColor's unnamed mask-shaped outputs are left to a
	 * ComponentMask node. That is narrowness by choice, not necessity: a Mask = 0 wire is graph-stable
	 * on any output (GetValidOutputIndex trusts the index whenever the mask is 0), and the rule keeps
	 * to pins a user can read by name.
	 */
	bool TryResolveSwizzleAsNamedOutput(
		const UMaterialExpression* Expression,
		const int32 OperandOutputIndex,
		const int32 OperandWidth,
		const FString& Mask,
		int32& OutOutputIndex)
	{
		static const TCHAR* const Prefixes[] = { TEXT("x"), TEXT("xy"), TEXT("xyz"), TEXT("xyzw") };
		if (!Expression || Mask.IsEmpty() || Mask.Len() >= OperandWidth || Mask.Len() > static_cast<int32>(UE_ARRAY_COUNT(Prefixes)))
		{
			// A mask as wide as the value is the identity the builder already elides; a wider one
			// does not exist.
			return false;
		}

		// Strictly an ascending prefix: `xy` yes, `xz` and `yx` no. The builder guarantees the
		// mask is ascending, so this only has to test that it starts at x and has no gaps.
		if (!Mask.Equals(Prefixes[Mask.Len() - 1], ESearchCase::CaseSensitive))
		{
			return false;
		}

		if (!IsWholeValueOutput(Expression, OperandOutputIndex, OperandWidth))
		{
			return false;
		}

		const TCHAR* const DesiredName = GetLeadingChannelOutputName(Mask.Len());
		if (DesiredName == nullptr)
		{
			return false;
		}

		for (int32 Index = 0; Index < Expression->Outputs.Num(); ++Index)
		{
			const FExpressionOutput& Output = Expression->Outputs[Index];
			if (Output.OutputName.IsNone())
			{
				continue;
			}
			if (Output.OutputName.ToString().Equals(DesiredName, ESearchCase::IgnoreCase)
				&& IsLeadingChannelOutput(Output, Mask.Len()))
			{
				OutOutputIndex = Index;
				return true;
			}
		}

		return false;
	}

	// --------------------------------------------------------------------------------- the walk

	bool FIREmitter::EmitGraph()
	{
		const IR::FIRGraph& Graph = Product.Graph;
		EmittedNodes.SetNum(Graph.Nodes.Num());

		for (const int32 NodeIndex : Graph.TopologicalOrder())
		{
			if (!Graph.Nodes.IsValidIndex(NodeIndex))
			{
				Diagnostics.Error(TEXT("DSH8205"), Product.Source.Span, FText::Format(
					LOCTEXT("BadNodeIndex", "The topological order of '{0}' names node {1}, which the graph does not have."),
					FText::FromString(Product.Name),
					FText::AsNumber(NodeIndex)));
				return false;
			}

			if (!EmitNode(NodeIndex))
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * PROPERTIES BEFORE PINS. Every Emit* below writes all of a node's literal properties -- reflected
	 * properties, parameter metadata, Custom code and output type, sampler type, mask flags, a
	 * MaterialFunctionCall's function -- BEFORE it connects a single input.
	 *
	 * Not a style preference. Several expression classes rebuild or re-validate their input list when
	 * a property changes: TextureSample on SamplerType, MaterialFunctionCall on the assigned function,
	 * Custom on OutputType and its Inputs array. Connect first and the property write drops the wire
	 * or leaves a stale pin list behind, which the graph then reports as an unconnected input on a
	 * node that visibly has one. The 1.x generator learned the same thing the same way
	 * (`52070d8 fix(generator): literal properties bind before pins`).
	 *
	 * The one place the two genuinely interleave is SetMaterialAttributes, where each attribute pin
	 * and its AttributeSetTypes entry are created together by the engine's own bookkeeping; that is
	 * the node's design, not an exception to the rule, and the 1.x helper does both halves atomically.
	 */
	bool FIREmitter::EmitNode(const int32 NodeIndex)
	{
		const IR::FIRNode& Node = Product.Graph.Nodes[NodeIndex];

		switch (Node.Op)
		{
		case IR::EIROp::Constant:               return EmitConstant(NodeIndex, Node);
		case IR::EIROp::Parameter:              return EmitParameter(NodeIndex, Node);
		case IR::EIROp::TextureParameter:       return EmitTextureParameter(NodeIndex, Node);
		case IR::EIROp::FunctionInput:          return EmitFunctionInput(NodeIndex, Node);
		case IR::EIROp::Reflected:              return EmitReflected(NodeIndex, Node);
		case IR::EIROp::FunctionCall:           return EmitFunctionCall(NodeIndex, Node);
		case IR::EIROp::Custom:                 return EmitCustom(NodeIndex, Node);
		case IR::EIROp::TextureSample:          return EmitTextureSample(NodeIndex, Node);
		case IR::EIROp::Swizzle:                return EmitSwizzle(NodeIndex, Node);
		case IR::EIROp::Append:                 return EmitAppend(NodeIndex, Node);
		case IR::EIROp::Select:                 return EmitSelect(NodeIndex, Node);
		case IR::EIROp::Compare:                return EmitCompare(NodeIndex, Node);
		case IR::EIROp::StaticSwitch:           return EmitStaticSwitch(NodeIndex, Node);
		case IR::EIROp::MakeMaterialAttributes: return EmitMakeMaterialAttributes(NodeIndex, Node);
		case IR::EIROp::SetMaterialAttributes:  return EmitSetMaterialAttributes(NodeIndex, Node);
		case IR::EIROp::GetMaterialAttributes:  return EmitGetMaterialAttributes(NodeIndex, Node);
		case IR::EIROp::MaterialSink:           return EmitMaterialSink(NodeIndex, Node);
		case IR::EIROp::FunctionOutput:         return EmitFunctionOutput(NodeIndex, Node);

		// Neither of these is a node. A Convert is a kind change the graph does not have (every
		// numeric value in a material graph is a float already) and a Broadcast is the graph's own
		// scalar-to-vector rule; both exist in the IR so the typing is honest, and both disappear
		// here by wiring the user straight to the operand.
		case IR::EIROp::Convert:
		case IR::EIROp::Broadcast:
			return EmitPassthrough(NodeIndex, Node);

		default:
			break;
		}

		// Everything below the structural block: arithmetic, comparison, logic. There are ~70 of
		// them and they are all table-driven, so they are handled here rather than as 70 cases --
		// which is also why this switch carries a `default` and cannot rely on -Wswitch to catch an
		// op added to the frozen enum. A new structural op therefore lands on IsCoreMathOp's `false`
		// and is reported, not silently dropped.
		if (IR::IsCoreMathOp(Node.Op))
		{
			return EmitCoreMathOp(NodeIndex, Node);
		}

		return Fail(TEXT("DSH8223"), Node, FText::Format(
			LOCTEXT("UnhandledOp", "The emitter has no rule for the IR operation '{0}'."),
			FText::FromString(IR::LexToString(Node.Op))));
	}

	// --------------------------------------------------------------------------------- leaves

	bool FIREmitter::EmitConstant(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		const IR::FIRProperty* ValueProperty = Node.FindProperty(IR::Prop::Value);
		if (!ValueProperty)
		{
			return Fail(TEXT("DSH8224"), Node, LOCTEXT("ConstantNoValue", "A Constant node carries no Value property."));
		}

		const IR::FIRPropertyValue& Value = ValueProperty->Value;
		const int32 Components = FMath::Clamp(Value.N, 1, 4);

		switch (Components)
		{
		case 1:
		{
			auto* Expression = Cast<UMaterialExpressionConstant>(CreateExpression(UMaterialExpressionConstant::StaticClass(), Node));
			if (!Expression)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("ConstantFailed", "Failed to create a Constant node."));
			}
			Expression->R = static_cast<float>(Value.V[0]);
			RegisterNode(NodeIndex, Node, Expression);
			return true;
		}
		case 2:
		{
			auto* Expression = Cast<UMaterialExpressionConstant2Vector>(CreateExpression(UMaterialExpressionConstant2Vector::StaticClass(), Node));
			if (!Expression)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("Constant2Failed", "Failed to create a Constant2Vector node."));
			}
			Expression->R = static_cast<float>(Value.V[0]);
			Expression->G = static_cast<float>(Value.V[1]);
			RegisterNode(NodeIndex, Node, Expression);
			return true;
		}
		case 3:
		{
			auto* Expression = Cast<UMaterialExpressionConstant3Vector>(CreateExpression(UMaterialExpressionConstant3Vector::StaticClass(), Node));
			if (!Expression)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("Constant3Failed", "Failed to create a Constant3Vector node."));
			}
			Expression->Constant = FLinearColor(
				static_cast<float>(Value.V[0]),
				static_cast<float>(Value.V[1]),
				static_cast<float>(Value.V[2]),
				1.0f); // alpha 1, as the 1.x generator's Constant3Vector
			RegisterNode(NodeIndex, Node, Expression);
			return true;
		}
		default:
		{
			auto* Expression = Cast<UMaterialExpressionConstant4Vector>(CreateExpression(UMaterialExpressionConstant4Vector::StaticClass(), Node));
			if (!Expression)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("Constant4Failed", "Failed to create a Constant4Vector node."));
			}
			Expression->Constant = FLinearColor(
				static_cast<float>(Value.V[0]),
				static_cast<float>(Value.V[1]),
				static_cast<float>(Value.V[2]),
				static_cast<float>(Value.V[3]));
			RegisterNode(NodeIndex, Node, Expression);
			return true;
		}
		}
	}

	// ----------------------------------------------------------------------------- reflected

	bool FIREmitter::EmitReflected(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		UClass* ExpressionClass = nullptr;

		// The catalog index is the precise answer -- it names the class path the binder resolved
		// against -- and ResolveMaterialExpressionClass is the tolerant one, which is what a
		// hand-written `UE.Expression(Class = "...")` needs.
		if (Context.Catalog && Context.Catalog->Expressions.IsValidIndex(Node.CatalogIndex))
		{
			const IR::FCatalogExpression& Entry = Context.Catalog->Expressions[Node.CatalogIndex];
			if (!Entry.ClassPathName.IsEmpty())
			{
				ExpressionClass = LoadObject<UClass>(nullptr, *Entry.ClassPathName);
			}
			if (!ExpressionClass && !Entry.ClassName.IsEmpty())
			{
				ExpressionClass = Private::ResolveMaterialExpressionClass(Entry.ClassName);
			}
		}
		if (!ExpressionClass)
		{
			ExpressionClass = Private::ResolveMaterialExpressionClass(Node.ClassName);
		}
		if (!ExpressionClass)
		{
			if (const IR::FIRProperty* Specifier = Node.FindProperty(IR::Prop::ClassSpecifier))
			{
				ExpressionClass = Private::ResolveMaterialExpressionClass(Specifier->Value.S);
			}
		}
		if (!ExpressionClass)
		{
			return Fail(TEXT("DSH8211"), Node, FText::Format(
				LOCTEXT("UnknownExpressionClass", "'{0}' is not a material expression class this engine has."),
				FText::FromString(Node.ClassName)));
		}

		UMaterialExpression* Expression = CreateExpression(ExpressionClass, Node);
		if (!Expression)
		{
			return Fail(TEXT("DSH8214"), Node, FText::Format(
				LOCTEXT("ReflectedFailed", "Failed to create a '{0}' node."),
				FText::FromString(ExpressionClass->GetName())));
		}

		// Properties before pins: a `Const*` twin decides what an unconnected pin means, and a few
		// classes (Time's bOverride_Period, TextureCoordinate's tiling) read their literal fields
		// when the pin is empty.
		for (const IR::FIRProperty& Property : Node.Properties)
		{
			if (Property.Name.Equals(IR::Prop::ClassSpecifier, ESearchCase::CaseSensitive))
			{
				continue;
			}
			if (!ApplyReflectedProperty(Node, Expression, Property, /*bWarnWhenMissing*/ false))
			{
				return false;
			}
		}

		for (const IR::FIRInput& Input : Node.Inputs)
		{
			FEmittedValue Value;
			if (!ResolveValue(Node, Input.Value, Value))
			{
				return false;
			}
			if (!ConnectNamedInput(Node, Expression, Input.Pin, Value))
			{
				return false;
			}
		}

		RegisterNodeWithNamedOutputs(NodeIndex, Node, Expression);
		return true;
	}

	// ----------------------------------------------------------------------------- core math

	bool FIREmitter::EmitCoreMathOp(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		const IR::FIRCoreOpInfo& Info = IR::GetCoreOpInfo(Node.Op);
		if (Info.ExpressionClass == nullptr)
		{
			return EmitHandLoweredCoreOp(NodeIndex, Node, Info);
		}

		UClass* ExpressionClass = Private::ResolveMaterialExpressionClass(Info.ExpressionClass);
		if (!ExpressionClass)
		{
			return Fail(TEXT("DSH8211"), Node, FText::Format(
				LOCTEXT("CoreOpClassMissing", "The core operation '{0}' maps to expression class '{1}', which this engine does not have."),
				FText::FromString(IR::LexToString(Node.Op)),
				FText::FromString(Info.ExpressionClass)));
		}

		UMaterialExpression* Expression = CreateExpression(ExpressionClass, Node);
		if (!Expression)
		{
			return Fail(TEXT("DSH8214"), Node, FText::Format(
				LOCTEXT("CoreOpFailed", "Failed to create a '{0}' node."),
				FText::FromString(ExpressionClass->GetName())));
		}

		// A core op may still carry reflected properties (Clamp's ClampMode, an enum a pass folded
		// in); they are written the same way a Reflected node's are, and for the same reason BEFORE
		// the pins. See the note at the top of EmitNode.
		for (const IR::FIRProperty& Property : Node.Properties)
		{
			if (!ApplyReflectedProperty(Node, Expression, Property, /*bWarnWhenMissing*/ false))
			{
				return false;
			}
		}

		for (int32 OperandIndex = 0; OperandIndex < Node.Operands.Num(); ++OperandIndex)
		{
			if (OperandIndex >= UE_ARRAY_COUNT(Info.InputPins) || Info.InputPins[OperandIndex] == nullptr)
			{
				return Fail(TEXT("DSH8224"), Node, FText::Format(
					LOCTEXT("CoreOpTooManyOperands", "The core operation '{0}' was given {1} operands, but its table names only {2} pins."),
					FText::FromString(IR::LexToString(Node.Op)),
					FText::AsNumber(Node.Operands.Num()),
					FText::AsNumber(OperandIndex)));
			}

			FEmittedValue Value;
			if (!ResolveOperand(Node, OperandIndex, Value))
			{
				return false;
			}
			if (!ConnectNamedInput(Node, Expression, Info.InputPins[OperandIndex], Value))
			{
				return false;
			}
		}

		RegisterNode(NodeIndex, Node, Expression);
		return true;
	}

	bool FIREmitter::EmitHandLoweredCoreOp(const int32 NodeIndex, const IR::FIRNode& Node, const IR::FIRCoreOpInfo& Info)
	{
		(void)Info;

		// Every shape below is the 1.x generator's, so a 2.0 source and its 1.x twin produce the
		// same graph. Where 1.x had no spelling for the operation at all (the comparisons, && / ||
		// / !), the shape is the one the decompiler already reads back.
		const auto MakeNode = [this, &Node](UClass* Class) -> UMaterialExpression*
		{
			return CreateExpression(Class, Node);
		};

		switch (Node.Op)
		{
		case IR::EIROp::Negate:
		{
			// 1.x: `-x` is `x * -1` with the -1 as a real Constant node (EvaluateUnary in
			// DreamShaderMaterialGeneratorCodeExpressions.cpp ~1305).
			FEmittedValue Operand;
			if (!ResolveOperand(Node, 0, Operand))
			{
				return false;
			}
			UMaterialExpression* MinusOne = CreateScalarConstant(Node, -1.0);
			auto* Multiply = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			if (!MinusOne || !Multiply)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("NegateFailed", "Failed to create the Multiply node a negation lowers to."));
			}
			ConnectValueToInput(Multiply->A, Operand);
			Multiply->B.Connect(0, MinusOne);
			RegisterNode(NodeIndex, Node, Multiply);
			return true;
		}

		case IR::EIROp::LogicalNot:
		{
			// `!b` on a 0/1 bool is `1 - b`. ConstA rather than a Constant node, matching how 1.x
			// writes a literal operand it does not need a pin for (refract's `1 - ...`).
			FEmittedValue Operand;
			if (!ResolveOperand(Node, 0, Operand))
			{
				return false;
			}
			auto* Subtract = Cast<UMaterialExpressionSubtract>(MakeNode(UMaterialExpressionSubtract::StaticClass()));
			if (!Subtract)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("NotFailed", "Failed to create the Subtract node a logical not lowers to."));
			}
			Subtract->ConstA = 1.0f;
			ConnectValueToInput(Subtract->B, Operand);
			RegisterNode(NodeIndex, Node, Subtract);
			return true;
		}

		case IR::EIROp::LogicalAnd:
		{
			// Both operands are 0 or 1, so `a && b` is `a * b` -- and it stays a Multiply rather
			// than an If so that a static-bool pair keeps folding.
			FEmittedValue A;
			FEmittedValue B;
			if (!ResolveOperand(Node, 0, A) || !ResolveOperand(Node, 1, B))
			{
				return false;
			}
			auto* Multiply = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			if (!Multiply)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("AndFailed", "Failed to create the Multiply node a logical and lowers to."));
			}
			ConnectValueToInput(Multiply->A, A);
			ConnectValueToInput(Multiply->B, B);
			RegisterNode(NodeIndex, Node, Multiply);
			return true;
		}

		case IR::EIROp::LogicalOr:
		{
			// `a || b` on 0/1 is `max(a, b)`. Not `a + b - a*b`: the extra nodes buy nothing once
			// both sides are known to be 0 or 1, and max reads correctly in the graph.
			FEmittedValue A;
			FEmittedValue B;
			if (!ResolveOperand(Node, 0, A) || !ResolveOperand(Node, 1, B))
			{
				return false;
			}
			auto* Max = Cast<UMaterialExpressionMax>(MakeNode(UMaterialExpressionMax::StaticClass()));
			if (!Max)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("OrFailed", "Failed to create the Max node a logical or lowers to."));
			}
			ConnectValueToInput(Max->A, A);
			ConnectValueToInput(Max->B, B);
			RegisterNode(NodeIndex, Node, Max);
			return true;
		}

		case IR::EIROp::Less:
		case IR::EIROp::LessEqual:
		case IR::EIROp::Greater:
		case IR::EIROp::GreaterEqual:
		case IR::EIROp::Equal:
		case IR::EIROp::NotEqual:
		{
			// One If node producing 1 or 0, with the three branches wired from the same table the
			// 1.x `if` statement uses (FCodeGraphBuilder::CreateConditionalValue ~630). Keeping the
			// table identical is what makes a 2.0 `a > b ? x : y` and a 1.x `if (a > b)` collapse to
			// the same shape after the dedupe pass.
			FEmittedValue A;
			FEmittedValue B;
			if (!ResolveOperand(Node, 0, A) || !ResolveOperand(Node, 1, B))
			{
				return false;
			}

			const bool bGreater = Node.Op == IR::EIROp::Greater || Node.Op == IR::EIROp::GreaterEqual || Node.Op == IR::EIROp::NotEqual;
			const bool bEqual = Node.Op == IR::EIROp::LessEqual || Node.Op == IR::EIROp::GreaterEqual || Node.Op == IR::EIROp::Equal;
			const bool bLess = Node.Op == IR::EIROp::Less || Node.Op == IR::EIROp::LessEqual || Node.Op == IR::EIROp::NotEqual;

			UMaterialExpression* One = CreateScalarConstant(Node, 1.0);
			UMaterialExpression* Zero = CreateScalarConstant(Node, 0.0);
			auto* If = Cast<UMaterialExpressionIf>(MakeNode(UMaterialExpressionIf::StaticClass()));
			if (!One || !Zero || !If)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("CompareLowerFailed", "Failed to create the If node a comparison lowers to."));
			}

			ConnectValueToInput(If->A, A);
			ConnectValueToInput(If->B, B);
			If->AGreaterThanB.Connect(0, bGreater ? One : Zero);
			If->AEqualsB.Connect(0, bEqual ? One : Zero);
			If->ALessThanB.Connect(0, bLess ? One : Zero);
			RegisterNode(NodeIndex, Node, If);
			return true;
		}

		case IR::EIROp::Rcp:
		{
			FEmittedValue Operand;
			if (!ResolveOperand(Node, 0, Operand))
			{
				return false;
			}
			auto* Divide = Cast<UMaterialExpressionDivide>(MakeNode(UMaterialExpressionDivide::StaticClass()));
			if (!Divide)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("RcpFailed", "Failed to create the Divide node a reciprocal lowers to."));
			}
			Divide->ConstA = 1.0f;
			ConnectValueToInput(Divide->B, Operand);
			RegisterNode(NodeIndex, Node, Divide);
			return true;
		}

		case IR::EIROp::Rsqrt:
		{
			FEmittedValue Operand;
			if (!ResolveOperand(Node, 0, Operand))
			{
				return false;
			}
			auto* Sqrt = Cast<UMaterialExpressionSquareRoot>(MakeNode(UMaterialExpressionSquareRoot::StaticClass()));
			auto* Divide = Cast<UMaterialExpressionDivide>(MakeNode(UMaterialExpressionDivide::StaticClass()));
			if (!Sqrt || !Divide)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("RsqrtFailed", "Failed to create the nodes an inverse square root lowers to."));
			}
			ConnectValueToInput(Sqrt->Input, Operand);
			Divide->ConstA = 1.0f;
			Divide->B.Connect(0, Sqrt);
			RegisterNode(NodeIndex, Node, Divide);
			return true;
		}

		case IR::EIROp::Fwidth:
		{
			// fwidth(x) == abs(ddx(x)) + abs(ddy(x)). The engine has DDX and DDY nodes but no
			// fwidth, and the abs pair is what the HLSL intrinsic is defined as.
			FEmittedValue Operand;
			if (!ResolveOperand(Node, 0, Operand))
			{
				return false;
			}
			auto* DDX = Cast<UMaterialExpressionDDX>(MakeNode(UMaterialExpressionDDX::StaticClass()));
			auto* DDY = Cast<UMaterialExpressionDDY>(MakeNode(UMaterialExpressionDDY::StaticClass()));
			auto* AbsX = Cast<UMaterialExpressionAbs>(MakeNode(UMaterialExpressionAbs::StaticClass()));
			auto* AbsY = Cast<UMaterialExpressionAbs>(MakeNode(UMaterialExpressionAbs::StaticClass()));
			auto* Add = Cast<UMaterialExpressionAdd>(MakeNode(UMaterialExpressionAdd::StaticClass()));
			if (!DDX || !DDY || !AbsX || !AbsY || !Add)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("FwidthFailed", "Failed to create the nodes fwidth lowers to."));
			}

			ConnectValueToInput(DDX->Value, Operand);
			ConnectValueToInput(DDY->Value, Operand);
			AbsX->Input.Connect(0, DDX);
			AbsY->Input.Connect(0, DDY);
			Add->A.Connect(0, AbsX);
			Add->B.Connect(0, AbsY);
			RegisterNode(NodeIndex, Node, Add);
			return true;
		}

		case IR::EIROp::Reflect:
		{
			// Ported verbatim from the 1.x lowering in
			// MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeMathBuiltins.cpp (~600):
			//   reflect(I, N) = I - 2 * dot(I, N) * N
			FEmittedValue Incident;
			FEmittedValue Normal;
			if (!ResolveOperand(Node, 0, Incident) || !ResolveOperand(Node, 1, Normal))
			{
				return false;
			}

			auto* IncidentDotNormal = Cast<UMaterialExpressionDotProduct>(MakeNode(UMaterialExpressionDotProduct::StaticClass()));
			auto* ScaledDot = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* ScaledNormal = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* Reflected = Cast<UMaterialExpressionSubtract>(MakeNode(UMaterialExpressionSubtract::StaticClass()));
			if (!IncidentDotNormal || !ScaledDot || !ScaledNormal || !Reflected)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("ReflectFailed", "Failed to create the nodes reflect lowers to."));
			}

			ConnectValueToInput(IncidentDotNormal->A, Incident);
			ConnectValueToInput(IncidentDotNormal->B, Normal);
			ScaledDot->ConstB = 2.0f;
			ScaledDot->A.Connect(0, IncidentDotNormal);
			ScaledNormal->A.Connect(0, ScaledDot);
			ConnectValueToInput(ScaledNormal->B, Normal);
			ConnectValueToInput(Reflected->A, Incident);
			Reflected->B.Connect(0, ScaledNormal);
			RegisterNode(NodeIndex, Node, Reflected);
			return true;
		}

		case IR::EIROp::Refract:
		{
			// Ported verbatim from the 1.x lowering (same file, ~665):
			//   k = 1 - eta*eta * (1 - dot(N, I)^2)
			//   k < 0 ? 0 : eta*I - (eta*dot(N, I) + sqrt(k)) * N
			// The zero branch is I * 0 rather than a Constant so its width always equals I's, which
			// the If node requires.
			FEmittedValue Incident;
			FEmittedValue Normal;
			FEmittedValue Eta;
			if (!ResolveOperand(Node, 0, Incident) || !ResolveOperand(Node, 1, Normal) || !ResolveOperand(Node, 2, Eta))
			{
				return false;
			}

			auto* NormalDotIncident = Cast<UMaterialExpressionDotProduct>(MakeNode(UMaterialExpressionDotProduct::StaticClass()));
			auto* NdotISquared = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* SinSquared = Cast<UMaterialExpressionSubtract>(MakeNode(UMaterialExpressionSubtract::StaticClass()));
			auto* EtaSquared = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* EtaSinSquared = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* K = Cast<UMaterialExpressionSubtract>(MakeNode(UMaterialExpressionSubtract::StaticClass()));
			auto* SqrtK = Cast<UMaterialExpressionSquareRoot>(MakeNode(UMaterialExpressionSquareRoot::StaticClass()));
			auto* EtaNdotI = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* NormalScale = Cast<UMaterialExpressionAdd>(MakeNode(UMaterialExpressionAdd::StaticClass()));
			auto* ScaledNormal = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* ScaledIncident = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* Refracted = Cast<UMaterialExpressionSubtract>(MakeNode(UMaterialExpressionSubtract::StaticClass()));
			auto* ZeroVector = Cast<UMaterialExpressionMultiply>(MakeNode(UMaterialExpressionMultiply::StaticClass()));
			auto* Selected = Cast<UMaterialExpressionIf>(MakeNode(UMaterialExpressionIf::StaticClass()));
			if (!NormalDotIncident || !NdotISquared || !SinSquared || !EtaSquared || !EtaSinSquared
				|| !K || !SqrtK || !EtaNdotI || !NormalScale || !ScaledNormal || !ScaledIncident
				|| !Refracted || !ZeroVector || !Selected)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("RefractFailed", "Failed to create the nodes refract lowers to."));
			}

			ConnectValueToInput(NormalDotIncident->A, Normal);
			ConnectValueToInput(NormalDotIncident->B, Incident);
			NdotISquared->A.Connect(0, NormalDotIncident);
			NdotISquared->B.Connect(0, NormalDotIncident);
			SinSquared->ConstA = 1.0f;
			SinSquared->B.Connect(0, NdotISquared);
			ConnectValueToInput(EtaSquared->A, Eta);
			ConnectValueToInput(EtaSquared->B, Eta);
			EtaSinSquared->A.Connect(0, EtaSquared);
			EtaSinSquared->B.Connect(0, SinSquared);
			K->ConstA = 1.0f;
			K->B.Connect(0, EtaSinSquared);
			SqrtK->Input.Connect(0, K);
			ConnectValueToInput(EtaNdotI->A, Eta);
			EtaNdotI->B.Connect(0, NormalDotIncident);
			NormalScale->A.Connect(0, EtaNdotI);
			NormalScale->B.Connect(0, SqrtK);
			ScaledNormal->A.Connect(0, NormalScale);
			ConnectValueToInput(ScaledNormal->B, Normal);
			ConnectValueToInput(ScaledIncident->A, Incident);
			ConnectValueToInput(ScaledIncident->B, Eta);
			Refracted->A.Connect(0, ScaledIncident);
			Refracted->B.Connect(0, ScaledNormal);
			ZeroVector->ConstB = 0.0f;
			ConnectValueToInput(ZeroVector->A, Incident);
			// k == 0 still satisfies the refraction formula (sqrt(0) == 0), so it takes the
			// refracted side; only k < 0 falls through to zero.
			Selected->ConstB = 0.0f;
			Selected->A.Connect(0, K);
			Selected->AGreaterThanB.Connect(0, Refracted);
			Selected->AEqualsB.Connect(0, Refracted);
			Selected->ALessThanB.Connect(0, ZeroVector);
			RegisterNode(NodeIndex, Node, Selected);
			return true;
		}

		default:
			break;
		}

		return Fail(TEXT("DSH8223"), Node, FText::Format(
			LOCTEXT("NoLowering", "The core operation '{0}' has no engine expression class and no lowering in the emitter."),
			FText::FromString(IR::LexToString(Node.Op))));
	}

	// -------------------------------------------------------------------------- structural

	bool FIREmitter::EmitPassthrough(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		FEmittedValue Operand;
		if (!ResolveOperand(Node, 0, Operand))
		{
			return false;
		}

		// No node, and no entry in ExpressionsByVariable either: a pass-through has nothing to lay
		// out and nothing to put in a region box. Its users read the operand's expression directly,
		// which is also why a DebugName on a Convert does not become a graph node's name.
		EmittedNodes[NodeIndex].Expression = Operand.Expression;
		EmittedNodes[NodeIndex].OutputIndices.Reset();
		EmittedNodes[NodeIndex].OutputIndices.Add(Operand.OutputIndex);
		return true;
	}

	bool FIREmitter::EmitSwizzle(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		const IR::FIRProperty* MaskProperty = Node.FindProperty(IR::Prop::Mask);
		if (!MaskProperty)
		{
			return Fail(TEXT("DSH8226"), Node, LOCTEXT("SwizzleNoMask", "A Swizzle node carries no Mask property."));
		}

		bool bR = false;
		bool bG = false;
		bool bB = false;
		bool bA = false;
		if (!TryResolveSwizzleMask(MaskProperty->Value.S, bR, bG, bB, bA))
		{
			return Fail(TEXT("DSH8226"), Node, FText::Format(
				LOCTEXT("SwizzleBadMask", "'{0}' is not a canonical swizzle mask; the emitter expects a subset of xyzw in order."),
				FText::FromString(MaskProperty->Value.S)));
		}

		FEmittedValue Operand;
		if (!ResolveOperand(Node, 0, Operand))
		{
			return false;
		}

		// First chance: the operand's expression may already publish exactly these channels as a
		// named output, in which case the swizzle IS that output and needs no node of its own.
		const IR::FIRType& OperandType = Product.Graph.TypeOf(Node.Operands[0]);
		int32 NamedOutputIndex = INDEX_NONE;
		if (TryResolveSwizzleAsNamedOutput(
			Operand.Expression,
			Operand.OutputIndex,
			OperandType.GraphComponentCount(),
			MaskProperty->Value.S,
			NamedOutputIndex))
		{
			// No node registered, so nothing to lay out and nothing in a region box: this swizzle is
			// a different pin on a node that already exists.
			EmittedNodes[NodeIndex].Expression = Operand.Expression;
			EmittedNodes[NodeIndex].OutputIndices.Reset();
			EmittedNodes[NodeIndex].OutputIndices.Add(NamedOutputIndex);
			return true;
		}

		// Otherwise a real ComponentMask node, never an inline FExpressionInput mask (plan §3.3,
		// CONTRACT §6.6). The inline form is what DSK2 was about: the material graph editor re-points
		// a masked wire at whichever output matches the mask, so an inline mask on a multi-output
		// node silently became a different value on the first Apply.
		auto* Mask = Cast<UMaterialExpressionComponentMask>(CreateExpression(UMaterialExpressionComponentMask::StaticClass(), Node));
		if (!Mask)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("SwizzleFailed", "Failed to create a ComponentMask node."));
		}

		// Channel flags before the pin: they are literal properties like any other, and the graph
		// editor re-reads a ComponentMask's width from them.
		Mask->R = bR ? 1U : 0U;
		Mask->G = bG ? 1U : 0U;
		Mask->B = bB ? 1U : 0U;
		Mask->A = bA ? 1U : 0U;
		ConnectValueToInput(Mask->Input, Operand);
		RegisterNode(NodeIndex, Node, Mask);
		return true;
	}

	bool FIREmitter::EmitAppend(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		if (Node.Operands.Num() != 2)
		{
			return Fail(TEXT("DSH8224"), Node, FText::Format(
				LOCTEXT("AppendArity", "An Append node takes two operands; this one has {0}. Three or more parts are chained by the IR builder, not here."),
				FText::AsNumber(Node.Operands.Num())));
		}

		FEmittedValue A;
		FEmittedValue B;
		if (!ResolveOperand(Node, 0, A) || !ResolveOperand(Node, 1, B))
		{
			return false;
		}

		auto* Append = Cast<UMaterialExpressionAppendVector>(CreateExpression(UMaterialExpressionAppendVector::StaticClass(), Node));
		if (!Append)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("AppendFailed", "Failed to create an AppendVector node."));
		}

		ConnectValueToInput(Append->A, A);
		ConnectValueToInput(Append->B, B);
		RegisterNode(NodeIndex, Node, Append);
		return true;
	}

	bool FIREmitter::EmitSelect(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		if (Node.Operands.Num() != 3)
		{
			return Fail(TEXT("DSH8224"), Node, FText::Format(
				LOCTEXT("SelectArity", "A Select node takes three operands (condition, then, else); this one has {0}."),
				FText::AsNumber(Node.Operands.Num())));
		}

		FEmittedValue Condition;
		FEmittedValue IfTrue;
		FEmittedValue IfFalse;
		if (!ResolveOperand(Node, 0, Condition) || !ResolveOperand(Node, 1, IfTrue) || !ResolveOperand(Node, 2, IfFalse))
		{
			return false;
		}

		// `c ? a : b` is `c != 0`, which is an If against ZERO with the true branch on BOTH sides of
		// it: anything above zero and anything below zero takes `a`, and only exactly zero takes
		// `b`. That is 1.x's truthy shape verbatim (FCodeGraphBuilder::CreateConditionalValue,
		// CodeExpressions.cpp ~630: a zero literal node for B, then ConnectBranches(True, False,
		// True)), and it is what keeps a 2.0 ternary and its 1.x `if` twin dumping identically.
		//
		// NOT an If against 0.5 -- the comment in IRCoreOps.h says 0.5 and is wrong (CONTRACT
		// §6.13 #21). A midpoint test would agree with this one for a strict 0/1 bool and disagree
		// for every other truthy value, which is exactly the case a `float` condition produces.
		UMaterialExpression* Zero = CreateScalarConstant(Node, 0.0);
		auto* If = Cast<UMaterialExpressionIf>(CreateExpression(UMaterialExpressionIf::StaticClass(), Node));
		if (!Zero || !If)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("SelectFailed", "Failed to create the If node a select lowers to."));
		}

		ConnectValueToInput(If->A, Condition);
		If->B.Connect(0, Zero);
		ConnectValueToInput(If->AGreaterThanB, IfTrue);
		ConnectValueToInput(If->AEqualsB, IfFalse);
		ConnectValueToInput(If->ALessThanB, IfTrue);
		RegisterNode(NodeIndex, Node, If);
		return true;
	}

	bool FIREmitter::EmitCompare(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		if (Node.Operands.Num() != 5)
		{
			return Fail(TEXT("DSH8224"), Node, FText::Format(
				LOCTEXT("CompareArity", "A Compare node takes five operands (A, B, greater, equal, less); this one has {0}."),
				FText::AsNumber(Node.Operands.Num())));
		}

		FEmittedValue A;
		FEmittedValue B;
		FEmittedValue Greater;
		FEmittedValue Equal;
		FEmittedValue Less;
		if (!ResolveOperand(Node, 0, A) || !ResolveOperand(Node, 1, B)
			|| !ResolveOperand(Node, 2, Greater) || !ResolveOperand(Node, 3, Equal) || !ResolveOperand(Node, 4, Less))
		{
			return false;
		}

		auto* If = Cast<UMaterialExpressionIf>(CreateExpression(UMaterialExpressionIf::StaticClass(), Node));
		if (!If)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("CompareFailed", "Failed to create an If node."));
		}

		ConnectValueToInput(If->A, A);
		ConnectValueToInput(If->B, B);
		ConnectValueToInput(If->AGreaterThanB, Greater);
		ConnectValueToInput(If->AEqualsB, Equal);
		ConnectValueToInput(If->ALessThanB, Less);
		RegisterNode(NodeIndex, Node, If);
		return true;
	}

	bool FIREmitter::EmitStaticSwitch(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		if (Node.Operands.Num() != 3)
		{
			return Fail(TEXT("DSH8224"), Node, FText::Format(
				LOCTEXT("StaticSwitchArity", "A StaticSwitch node takes three operands (condition, then, else); this one has {0}."),
				FText::AsNumber(Node.Operands.Num())));
		}

		FEmittedValue Condition;
		FEmittedValue IfTrue;
		FEmittedValue IfFalse;
		if (!ResolveOperand(Node, 0, Condition) || !ResolveOperand(Node, 1, IfTrue) || !ResolveOperand(Node, 2, IfFalse))
		{
			return false;
		}

		auto* Switch = Cast<UMaterialExpressionStaticSwitch>(CreateExpression(UMaterialExpressionStaticSwitch::StaticClass(), Node));
		if (!Switch)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("StaticSwitchFailed", "Failed to create a StaticSwitch node."));
		}

		ConnectValueToInput(Switch->A, IfTrue);
		ConnectValueToInput(Switch->B, IfFalse);
		ConnectValueToInput(Switch->Value, Condition);
		RegisterNode(NodeIndex, Node, Switch);
		return true;
	}

	bool FIREmitter::EmitTextureSample(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		// FOUR fixed slots, always present: [Texture, UV, Sampler, Level], with FIRValue::None() in
		// any the source did not write. Indexed and tested, never counted: a `Tex.SampleLevel(UV, L)`
		// has a valid slot 3 behind an invalid slot 2, so Operands.Num() says nothing about which of
		// them are real. Counting is how slot 3 gets read as slot 2.
		const auto IsSlotSet = [&Node](const int32 Slot)
		{
			return Node.Operands.IsValidIndex(Slot) && Node.Operands[Slot].IsValid();
		};

		if (!IsSlotSet(0) || !IsSlotSet(1))
		{
			return Fail(TEXT("DSH8224"), Node, LOCTEXT("TextureSampleArity",
				"A TextureSample node needs a texture in operand 0 and a UV in operand 1; one of them is not set."));
		}

		FEmittedValue Texture;
		FEmittedValue UV;
		if (!ResolveOperand(Node, 0, Texture) || !ResolveOperand(Node, 1, UV))
		{
			return false;
		}

		auto* Sample = Cast<UMaterialExpressionTextureSample>(CreateExpression(UMaterialExpressionTextureSample::StaticClass(), Node));
		if (!Sample)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("TextureSampleFailed", "Failed to create a TextureSample node."));
		}

		// PROPERTIES BEFORE PINS, and on this class more than most: UMaterialExpressionTextureSample
		// re-validates its inputs when SamplerType changes, so a wire made first is a wire the
		// property write can drop. See the note at the top of EmitNode.
		const bool bHasMipLevel = IsSlotSet(3);
		if (bHasMipLevel)
		{
			// The default the MipValue pin needs to mean anything. Written before the loop, not
			// after, so an explicit Prop::MipValueMode from the source still overrides it.
			Sample->MipValueMode = TMVM_MipLevel;
		}

		for (const IR::FIRProperty& Property : Node.Properties)
		{
			if (!ApplyReflectedProperty(Node, Sample, Property, /*bWarnWhenMissing*/ true))
			{
				return false;
			}
		}

		ConnectValueToInput(Sample->TextureObject, Texture);
		ConnectValueToInput(Sample->Coordinates, UV);

		// Slot 2, the sampler state, has no pin and is dropped without a word. A material graph does
		// not choose samplers that way -- SamplerSource on the node decides, and the binder accepts
		// the `Texture2DSample(Tex, S, UV)` spelling knowing the S goes nowhere -- so warning here
		// would fire on every correctly written sample.

		// Slot 3 is an explicit mip level: the MipValue pin, whose meaning MipValueMode above set.
		if (bHasMipLevel)
		{
			FEmittedValue Level;
			if (!ResolveOperand(Node, 3, Level))
			{
				return false;
			}
			ConnectValueToInput(Sample->MipValue, Level);
		}

		// Engine output order is RGB, R, G, B, A, RGBA; the IR's is RGBA, R, G, B, A. Mapped by
		// name, so neither list has to be hard-coded here.
		RegisterNodeWithNamedOutputs(NodeIndex, Node, Sample);
		return true;
	}

	bool FIREmitter::EmitCustom(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		auto* Custom = Cast<UMaterialExpressionCustom>(CreateExpression(UMaterialExpressionCustom::StaticClass(), Node));
		if (!Custom)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("CustomFailed", "Failed to create a Custom node."));
		}

		Custom->Inputs.Reset();
		Custom->AdditionalOutputs.Reset();
		Custom->IncludeFilePaths.Reset();
		Custom->Description = Node.ClassName;
#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
		// Same as 1.x's whole-surface node: the code is generated, so the graph shows the node's
		// name rather than a wall of HLSL.
		Custom->ShowCode = false;
#endif

		if (const IR::FIRProperty* CodeProperty = Node.FindProperty(IR::Prop::Code))
		{
			// Verbatim. Prop::Code arrives finished from unit H
			// (IR/IRCustomHlsl.h, BuildDreamShaderCustomNodeCode), which owns the 2.0 versions of
			// every rule the 1.x DreamShaderHlslFunctionCodegen.cpp had -- the sampler pairing, the
			// generated_wrapper_* struct, include hoisting, and EnsureTopLevelReturn among them.
			//
			// Re-applying EnsureTopLevelReturn here would be worse than redundant. H's code carries
			// the CustomCodeMarker comment lines that make `check --shaders` able to map a
			// shader-compile error back to a source line (CONTRACT §6.13.3), and those depend on one
			// emitted line per source line. EnsureTopLevelReturn appends `\nreturn 0.0;` whenever
			// its brace-depth-0 scan finds no `return` -- which would land AFTER the closing
			// `// End DreamShader source:` marker, breaking the mapping and silently changing what
			// the node computes. The emitter does not edit generated HLSL.
			Custom->Code = CodeProperty->Value.S;
		}
		if (const IR::FIRProperty* DescriptionProperty = Node.FindProperty(IR::Prop::Description))
		{
			Custom->Description = DescriptionProperty->Value.S;
		}
		if (const IR::FIRProperty* OutputTypeProperty = Node.FindProperty(IR::Prop::OutputType))
		{
			ECustomMaterialOutputType OutputType = CMOT_Float1;
			if (!TryResolveCustomOutputTypeFromIR(OutputTypeProperty->Value.S, OutputType))
			{
				return Fail(TEXT("DSH8227"), Node, FText::Format(
					LOCTEXT("CustomBadOutputType", "'{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes."),
					FText::FromString(OutputTypeProperty->Value.S)));
			}
			Custom->OutputType = OutputType;
		}
		if (const IR::FIRProperty* IncludesProperty = Node.FindProperty(IR::Prop::IncludeFilePaths))
		{
			for (const FString& IncludePath : IncludesProperty->Value.List)
			{
				Custom->IncludeFilePaths.AddUnique(IncludePath);
			}
		}

		// The pin LIST is part of the node's declaration, not part of its wiring: the names go on
		// first, every one of them, and only then is anything connected. Adding an FCustomInput
		// between two connects would reallocate Inputs and leave the earlier pins' addresses stale.
		for (const IR::FIRInput& Input : Node.Inputs)
		{
			FCustomInput CustomInput;
			CustomInput.InputName = FName(*Input.Pin);
			Custom->Inputs.Add(CustomInput);
		}

		if (const IR::FIRProperty* AdditionalOutputs = Node.FindProperty(IR::Prop::AdditionalOutputs))
		{
			for (const FString& Entry : AdditionalOutputs->Value.List)
			{
				// "Name:Type", the spelling IR.h fixes for this list.
				FString OutputName;
				FString OutputTypeText;
				if (!Entry.Split(TEXT(":"), &OutputName, &OutputTypeText, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					return Fail(TEXT("DSH8227"), Node, FText::Format(
						LOCTEXT("CustomBadAdditionalOutput", "'{0}' is not a Custom node additional output; the emitter expects Name:Type."),
						FText::FromString(Entry)));
				}

				ECustomMaterialOutputType OutputType = CMOT_Float1;
				if (!TryResolveCustomOutputTypeFromIR(OutputTypeText, OutputType))
				{
					return Fail(TEXT("DSH8227"), Node, FText::Format(
						LOCTEXT("CustomBadAdditionalOutputType", "'{0}' is not a Custom node output type; expected Float1 through Float4 or MaterialAttributes."),
						FText::FromString(OutputTypeText)));
				}

				FCustomOutput CustomOutput;
				CustomOutput.OutputName = FName(*OutputName.TrimStartAndEnd());
				CustomOutput.OutputType = OutputType;
				Custom->AdditionalOutputs.Add(CustomOutput);
			}
		}

		// Rebuilds Outputs from AdditionalOutputs, which is what makes the node publish `return`
		// plus one pin per additional output rather than one unnamed pin. Still ahead of the wiring
		// below: everything that reshapes the node happens before anything is connected to it.
		Private::RebuildDreamShaderCustomOutputs(Custom);

		// And now the wiring, into a pin list that is finished and will not move.
		for (int32 InputIndex = 0; InputIndex < Node.Inputs.Num(); ++InputIndex)
		{
			FEmittedValue Value;
			if (!ResolveValue(Node, Node.Inputs[InputIndex].Value, Value))
			{
				return false;
			}
			ConnectValueToInput(Custom->Inputs[InputIndex].Input, Value);
		}

		RegisterNodeWithNamedOutputs(NodeIndex, Node, Custom);
		return true;
	}

	// -------------------------------------------------------------------------- the plumbing

	UMaterialExpression* FIREmitter::CreateExpression(UClass* ExpressionClass, const IR::FIRNode& Node)
	{
		(void)Node;
		if (!ExpressionClass)
		{
			return nullptr;
		}

		// One call for both asset kinds, which is the whole reason this 1.x helper exists: it wraps
		// UMaterialEditingLibrary::CreateMaterialExpressionEx and picks the material or the function
		// overload from whichever pointer is set. X is fixed because the layout pass rewrites every
		// position afterwards; Y only has to keep the pre-layout graph from stacking on one point.
		return Private::CreateOwnedMaterialExpression(Material, MaterialFunction, ExpressionClass, 0, ConsumeNodeY());
	}

	void FIREmitter::RegisterNode(const int32 NodeIndex, const IR::FIRNode& Node, UMaterialExpression* Expression)
	{
		RegisterNode(NodeIndex, Node, Expression, TArray<int32>());
	}

	void FIREmitter::RegisterNode(const int32 NodeIndex, const IR::FIRNode& Node, UMaterialExpression* Expression, TArray<int32>&& OutputIndices)
	{
		EmittedNodes[NodeIndex].Expression = Expression;
		EmittedNodes[NodeIndex].OutputIndices = MoveTemp(OutputIndices);

		if (!Expression)
		{
			return;
		}

		// The layout key: the variable the value was first assigned to when there is one, so a
		// `#pragma layout(Node, Var = albedo, ...)` finds it, and a synthetic key otherwise so the
		// region map can still name a node the author never named. Both maps have to agree on the
		// key -- AddRegionLayoutBlocks looks a region entry's key up in the expression map.
		const FString Key = MakeLayoutKey(NodeIndex, Node);
		ExpressionsByVariable.Add(Key, Expression);
		if (Product.Graph.Regions.IsValidIndex(Node.Region))
		{
			RegionByVariable.Add(Key, Product.Graph.Regions[Node.Region].Name);
		}

		// The node -> line table. MaterialExpressionGuid is stable across a rebuild only for nodes
		// whose identity the engine preserves, which is exactly why the table is rewritten in full
		// on every emit rather than merged.
		if (!Node.Source.File.IsEmpty() || Node.Source.Span.Length > 0)
		{
			SourceSpans.Add(Expression->MaterialExpressionGuid, Node.Source);
		}
	}

	void FIREmitter::RegisterNodeWithNamedOutputs(const int32 NodeIndex, const IR::FIRNode& Node, UMaterialExpression* Expression)
	{
		TArray<int32> OutputIndices;
		if (Expression && Node.OutputNames.Num() > 0)
		{
			OutputIndices.Reserve(Node.OutputNames.Num());
			for (int32 Slot = 0; Slot < Node.OutputNames.Num(); ++Slot)
			{
				// TryResolveExpressionOutputIndex matches a named output first and falls back to the
				// mask-shaped ones (R/G/B/A/RGB/RGBA), which is what makes TextureSample's unnamed
				// masked outputs addressable by the names the IR gave them.
				int32 EngineIndex = Slot;
				if (!Private::TryResolveExpressionOutputIndex(Expression, Node.OutputNames[Slot], EngineIndex))
				{
					EngineIndex = Slot;
				}
				OutputIndices.Add(EngineIndex);
			}
		}

		RegisterNode(NodeIndex, Node, Expression, MoveTemp(OutputIndices));
	}

	bool FIREmitter::ResolveValue(const IR::FIRNode& Node, const IR::FIRValue Value, FEmittedValue& OutValue)
	{
		OutValue = FEmittedValue{};

		if (!Value.IsValid() || !EmittedNodes.IsValidIndex(Value.Node) || EmittedNodes[Value.Node].Expression == nullptr)
		{
			return Fail(TEXT("DSH8225"), Node, FText::Format(
				LOCTEXT("ValueNotEmitted", "This node reads node {0}, which has not been emitted; the graph's topological order is inconsistent."),
				FText::AsNumber(Value.Node)));
		}

		const FEmittedNode& Emitted = EmittedNodes[Value.Node];
		if (Emitted.OutputIndices.IsValidIndex(Value.Output) && Emitted.OutputIndices[Value.Output] == INDEX_NONE)
		{
			// A slot the emitted expression does not publish. Only EmitGetMaterialAttributes marks one
			// (an attribute BreakMaterialAttributes has no pin for), and only a READ of it is an error:
			// that node carries a slot for every attribute in the catalog, so failing the node for a
			// slot nobody wires would fail every material read through a pin. Wiring INDEX_NONE would
			// not fail either -- UMaterialExpression::ConnectExpression ignores an out-of-range index
			// and leaves the pin empty, which is the silent version of this message.
			const IR::FIRNode& Producer = Product.Graph.Nodes[Value.Node];
			return Fail(TEXT("DSH8216"), Node, FText::Format(
				LOCTEXT("BreakAttributesSlotNotPublished", "BreakMaterialAttributes does not publish the attribute '{0}', so it cannot be read from a material that came through a pin."),
				FText::FromString(Producer.OutputNames.IsValidIndex(Value.Output)
					? Producer.OutputNames[Value.Output]
					: FString::FromInt(Value.Output))));
		}

		OutValue.Expression = Emitted.Expression;
		OutValue.OutputIndex = Emitted.OutputIndices.IsValidIndex(Value.Output)
			? Emitted.OutputIndices[Value.Output]
			: Value.Output;
		return true;
	}

	bool FIREmitter::ResolveOperand(const IR::FIRNode& Node, const int32 OperandIndex, FEmittedValue& OutValue)
	{
		if (!Node.Operands.IsValidIndex(OperandIndex))
		{
			return Fail(TEXT("DSH8224"), Node, FText::Format(
				LOCTEXT("MissingOperand", "This node needs operand {0}, but it has only {1}."),
				FText::AsNumber(OperandIndex),
				FText::AsNumber(Node.Operands.Num())));
		}

		return ResolveValue(Node, Node.Operands[OperandIndex], OutValue);
	}

	void FIREmitter::ConnectValueToInput(FExpressionInput& Input, const FEmittedValue& Value)
	{
		// The 2.0 ConnectCodeValueToInput. The 1.x one also carried an inline component mask; 2.0
		// never emits one (a Swizzle is a ComponentMask node), so this clears the mask
		// unconditionally -- which matters when a pin is being reconnected on a reused node.
		if (!Value.Expression)
		{
			return;
		}

		Input.Connect(Value.OutputIndex, Value.Expression);
		Input.Mask = 0;
		Input.MaskR = 0;
		Input.MaskG = 0;
		Input.MaskB = 0;
		Input.MaskA = 0;
	}

	bool FIREmitter::TryConnectNamedInput(UMaterialExpression* Expression, const FString& PinName, const FEmittedValue& Value)
	{
		if (!Expression)
		{
			return false;
		}

		FString BaseName;
		int32 ArrayIndex = 0;
		TrySplitIndexedPinName(PinName, BaseName, ArrayIndex);

		FProperty* Property = Private::FindMaterialExpressionArgumentProperty(Expression->GetClass(), BaseName);
		if (!Property || !Private::IsMaterialExpressionInputProperty(Property))
		{
			return false;
		}
		if (ArrayIndex < 0 || ArrayIndex >= Property->ArrayDim)
		{
			return false;
		}

		FExpressionInput* Input = Property->ContainerPtrToValuePtr<FExpressionInput>(Expression, ArrayIndex);
		if (!Input)
		{
			return false;
		}

		ConnectValueToInput(*Input, Value);
		return true;
	}

	bool FIREmitter::ConnectNamedInput(const IR::FIRNode& Node, UMaterialExpression* Expression, const FString& PinName, const FEmittedValue& Value)
	{
		if (!Expression)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("ConnectNoExpression", "Cannot connect an input on an expression that was not created."));
		}

		if (!TryConnectNamedInput(Expression, PinName, Value))
		{
			return Fail(TEXT("DSH8212"), Node, FText::Format(
				LOCTEXT("UnknownPin", "'{0}' has no input pin named '{1}'."),
				FText::FromString(Expression->GetClass()->GetName()),
				FText::FromString(PinName)));
		}

		return true;
	}

	bool FIREmitter::ApplyReflectedProperty(const IR::FIRNode& Node, UMaterialExpression* Expression, const IR::FIRProperty& Property, const bool bWarnWhenMissing)
	{
		if (!Expression)
		{
			return true;
		}

		FProperty* Target = Private::FindMaterialExpressionArgumentProperty(Expression->GetClass(), Property.Name);
		if (!Target || Private::IsMaterialExpressionInputProperty(Target))
		{
			if (bWarnWhenMissing)
			{
				Warn(TEXT("DSH8210"), Node, FText::Format(
					LOCTEXT("PropertyNotReflected", "'{0}' has no property named '{1}', so that value was not written."),
					FText::FromString(Expression->GetClass()->GetName()),
					FText::FromString(Property.Name)));
				return true;
			}

			return Fail(TEXT("DSH8213"), Node, FText::Format(
				LOCTEXT("PropertyMissing", "'{0}' has no property named '{1}'."),
				FText::FromString(Expression->GetClass()->GetName()),
				FText::FromString(Property.Name)));
		}

		// The 1.x literal writer does the parsing: bool / int / uint / float / double / string /
		// name / object-by-path / enum-by-enumerator / byte, and ImportText for anything structured.
		// Reusing it is what keeps a 2.0 `UE.X(Mode = Wrap)` and its 1.x twin writing the same value.
		FDreamShaderError LiteralError;
		const FString ValueText = FormatIRPropertyForReflection(Property.Value);
		if (!Private::SetMaterialExpressionLiteralProperty(Expression, Target, ValueText, LiteralError))
		{
			return Fail(TEXT("DSH8213"), Node, FText::Format(
				LOCTEXT("PropertyWriteFailed", "'{0}' could not take '{1}' for its '{2}' property. {3}"),
				FText::FromString(Expression->GetClass()->GetName()),
				FText::FromString(ValueText),
				FText::FromString(Property.Name),
				FText::FromString(LiteralError.HasCode()
					? FString::Printf(TEXT("%s: %s"), *LiteralError.Code, *LiteralError.Message) /* I18N-EXEMPT: quotes a 1.x generator message verbatim */
					: LiteralError.Message)));
		}

		return true;
	}

	UMaterialExpression* FIREmitter::CreateScalarConstant(const IR::FIRNode& Node, const double Value)
	{
		auto* Constant = Cast<UMaterialExpressionConstant>(CreateExpression(UMaterialExpressionConstant::StaticClass(), Node));
		if (Constant)
		{
			Constant->R = static_cast<float>(Value);
		}
		return Constant;
	}

	bool FIREmitter::ResolveMaterialAttribute(const IR::FIRNode& Node, const FString& AttributeName, EMaterialProperty& OutProperty)
	{
		if (Context.Catalog && TryResolveMaterialPropertyFromCatalog(*Context.Catalog, AttributeName, OutProperty))
		{
			return true;
		}

		return Fail(TEXT("DSH8216"), Node, FText::Format(
			LOCTEXT("UnknownAttribute", "'{0}' is not a material attribute this engine has."),
			FText::FromString(AttributeName)));
	}

	FString FIREmitter::MakeLayoutKey(const int32 NodeIndex, const IR::FIRNode& Node) const
	{
		if (!Node.DebugName.IsEmpty() && !ExpressionsByVariable.Contains(Node.DebugName))
		{
			return Node.DebugName;
		}

		// `$` cannot start a DreamShaderLang identifier, so a synthetic key can never collide with a
		// real variable name -- including the second node to claim an already-taken DebugName, which
		// is what the Contains check above is for.
		return FString::Printf(TEXT("$%d"), NodeIndex); /* I18N-EXEMPT: internal map key, never displayed */
	}

	bool FIREmitter::Fail(const TCHAR* Code, const IR::FIRNode& Node, const FText& Message)
	{
		return Fail(Code, Node.Source, Message);
	}

	bool FIREmitter::Fail(const TCHAR* Code, const IR::FIRSourceRef& Source, const FText& Message)
	{
		return Diagnostics.Error(Code, Source.Span, Message);
	}

	void FIREmitter::Warn(const TCHAR* Code, const IR::FIRNode& Node, const FText& Message)
	{
		Diagnostics.Warning(Code, Node.Source.Span, Message);
	}

	// ------------------------------------------------------------------ shared free helpers

	FString FormatIRPropertyForReflection(const IR::FIRPropertyValue& Value)
	{
		switch (Value.Kind)
		{
		case IR::EIRPropertyKind::Bool:
			return Value.B ? TEXT("true") : TEXT("false");

		case IR::EIRPropertyKind::Int:
			return FString::Printf(TEXT("%lld"), Value.I);

		case IR::EIRPropertyKind::Float:
			// %.9g round-trips a float exactly and does not grow a tail of zeros, which matters
			// because this text is also what a reflected float property is compared against when a
			// test diffs two generated assets.
			return FString::Printf(TEXT("%.9g"), Value.F);

		case IR::EIRPropertyKind::Float4:
		{
			// The ImportText spelling of a vector-ish struct. A property that is really a scalar
			// never gets here -- the builder writes those as Float -- so the four-component form is
			// the only one the 1.x writer has to parse back.
			const int32 Components = FMath::Clamp(Value.N, 1, 4);
			if (Components == 1)
			{
				return FString::Printf(TEXT("%.9g"), Value.V[0]);
			}
			return FString::Printf(
				TEXT("(R=%.9g,G=%.9g,B=%.9g,A=%.9g)"), /* I18N-EXEMPT: UE ImportText literal */
				Value.V[0],
				Components > 1 ? Value.V[1] : 0.0,
				Components > 2 ? Value.V[2] : 0.0,
				Components > 3 ? Value.V[3] : 0.0);
		}

		case IR::EIRPropertyKind::String:
		case IR::EIRPropertyKind::Name:
		case IR::EIRPropertyKind::Enum:
			return Value.S;

		case IR::EIRPropertyKind::Object:
			// A bare `/Game/...` path: TryResolveDreamShaderAssetReference accepts that form, and
			// the writer then loads and type-checks the asset itself.
			return Value.S;

		case IR::EIRPropertyKind::StringList:
			return FString::Join(Value.List, TEXT(","));

		default:
			break;
		}

		return Value.S;
	}

	bool TryResolveCustomOutputTypeFromIR(const FString& Spelling, ECustomMaterialOutputType& OutType)
	{
		// Delegates to the 1.x resolver, which already accepts float/float2/half3/int4/... as well
		// as the IR's own Float1..Float4 spelling, so a decompiled source and an IR-built one agree.
		return Private::TryResolveCustomOutputType(Spelling, OutType);
	}

	bool TryResolveFunctionInputTypeFromIR(const FString& Spelling, int32& OutInputTypeValue)
	{
		UEnum* InputTypeEnum = StaticEnum<EFunctionInputType>();
		if (!InputTypeEnum)
		{
			return false;
		}

		int64 EnumValue = INDEX_NONE;
		if (!Private::TryResolveEnumLiteral(InputTypeEnum, Spelling, EnumValue))
		{
			return false;
		}

		OutInputTypeValue = static_cast<int32>(EnumValue);
		return true;
	}

	bool TryResolveMaterialPropertyFromCatalog(const IR::FBuiltinCatalog& Catalog, const FString& AttributeName, EMaterialProperty& OutProperty)
	{
		const int32 AttributeIndex = Catalog.FindMaterialAttribute(AttributeName);
		if (AttributeIndex == INDEX_NONE)
		{
			return false;
		}

		const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
		if (!PropertyEnum)
		{
			return false;
		}

		const int64 EnumValue = PropertyEnum->GetValueByNameString(Catalog.MaterialAttributes[AttributeIndex].PropertyName);
		if (EnumValue == INDEX_NONE)
		{
			return false;
		}

		OutProperty = static_cast<EMaterialProperty>(EnumValue);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
