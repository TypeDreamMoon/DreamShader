// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The operations an IR node can be, and the table that describes the core family.
//
// Two families, on purpose (plan §3.3). The CORE family -- arithmetic, comparison, selection,
// swizzle, construction, sampling, static branching, function calls -- is modelled natively so its
// typing rules and its lowering live in one table here, and the emitter that turns a core op into
// an engine node is mechanical. The REFLECTED family -- `UE.*`, `Substrate.*`,
// `UE.Expression(Class = ...)` -- is one op, EIROp::Reflected, carrying a class name, named inputs
// and literal properties; the catalog (IRCatalog.h) is its only description and the long tail of
// engine nodes is never modelled.
//
// The table is the single place a rule such as "`%` is Fmod" or "`dot` returns a scalar" is
// written. The binder reads Typing to type a call, the IR builder reads it to build the node, the
// emitter reads ExpressionClass and InputPins to make the engine node. Nobody else may hold a copy.
//
// FROZEN for batch 1 (M2+M3). The enum may only GROW at the end (before Count); the table entries
// for existing ops may not change meaning.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"

namespace UE::DreamShader::IR
{
	enum class EIROp : uint8
	{
		// ---------------------------------------------------------------------------- leaves
		/** A literal float1..4. Property Value. */
		Constant,
		/** A scalar or vector parameter. Properties: ParameterName, Group, Description, SliderMin/Max, SortPriority, DefaultValue, bStatic. */
		Parameter,
		/** A texture parameter (TextureObjectParameter). Properties: ParameterName, Group, Description, SortPriority, DefaultAsset, SamplerType. */
		TextureParameter,
		/** A material function's input. Properties: InputName, Description, SortPriority, InputType, bOptional, PreviewValue. */
		FunctionInput,

		// ------------------------------------------------------------------------ structural
		/** Any engine expression class by name. ClassName + CatalogIndex, named Inputs, literal Properties. */
		Reflected,
		/** A MaterialFunctionCall. ClassName = the function asset's object path (or "" with Property FunctionPath); named Inputs; Outputs by name. */
		FunctionCall,
		/** A UMaterialExpressionCustom. Properties: Code, OutputType, Description, IncludeFilePaths; named Inputs; Outputs = result + AdditionalOutputs. */
		Custom,
		/**
		 * Operands: ALWAYS four, [Texture, UV, Sampler, Level], with FIRValue::None() in a slot the
		 * source did not give (the one op with optional operands). Properties: SamplerType,
		 * MipValueMode. Outputs: RGBA, R, G, B, A.
		 */
		TextureSample,
		/** ComponentMask. Operands: [Value]. Property Mask, canonical lower-case xyzw ("xyz"). */
		Swizzle,
		/** AppendVector. Operands: [A, B]; the builder chains it for three or more parts. */
		Append,
		/**
		 * `c ? a : b` when c is not a bare comparison (a bare comparison becomes Compare with the
		 * comparison's own operands, as 1.x did). Operands: [Condition (0/1 numeric), IfTrue, IfFalse].
		 * Lowers as 1.x did: If(A = c, B = Constant 0, AGreaterThanB = a, AEqualsB = b, ALessThanB = a).
		 */
		Select,
		/** The engine If node as a value. Operands: [A, B, IfGreater, IfEqual, IfLess]. */
		Compare,
		/** StaticSwitch. Operands: [Condition (a static bool value), IfTrue, IfFalse]. */
		StaticSwitch,
		/** MakeMaterialAttributes: named Inputs by attribute name. */
		MakeMaterialAttributes,
		/** SetMaterialAttributes: Input "MaterialAttributes" plus named attribute Inputs; Property AttributeSetTypes. */
		SetMaterialAttributes,
		/** BreakMaterialAttributes: Input "MaterialAttributes"; Outputs named by attribute. */
		GetMaterialAttributes,
		/** The material's own output pins. Named Inputs by attribute name. Exactly one per Material product; none elsewhere. */
		MaterialSink,
		/** A material function's output. Operands: [Value]. Properties: OutputName, Description, SortPriority. */
		FunctionOutput,

		// ------------------------------------------------------------------------ arithmetic
		Add, Subtract, Multiply, Divide, Fmod, Negate,
		Abs, Floor, Ceil, Round, Frac, Truncate, Sign,
		Saturate, Clamp, Min, Max, Lerp, Step, SmoothStep,
		Sqrt, Rsqrt, Rcp, Pow, Exp, Exp2, Log, Log2, Log10,
		Sin, Cos, Tan, Asin, Acos, Atan, Atan2, Sinh, Cosh, Tanh,
		Dot, Cross, Normalize, Length, Distance, Reflect, Refract,
		DDX, DDY, Fwidth,

		// ---------------------------------------------------------- comparison and logic (bool 0/1 results)
		Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
		LogicalAnd, LogicalOr, LogicalNot,

		// ------------------------------------------------------------------------- conversions
		/** Numeric kind change with no graph node: int -> float, bool -> float. Operands: [Value]. */
		Convert,
		/** A scalar used where a vector is required. Operands: [Value]; Outputs[0] says the width. */
		Broadcast,

		Count
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(EIROp Op);

	/** How the result type of a core op follows from its operands. */
	enum class EIRTypingRule : uint8
	{
		/** All operands numeric; scalars broadcast; the result has the widest operand's width and float kind. */
		SameAsOperands,
		/** The result has operand 0's type. */
		SameAsFirst,
		/** The result is a float scalar whatever the operands (dot, length, distance). */
		Scalar,
		/** Like SameAsOperands, but the result is bool. */
		Bool,
		/** Operands and result are float3 (cross, reflect, refract). */
		Float3,
		/** The binder types it by hand (leaves, structural ops, conversions). */
		Special,
	};

	struct FIRCoreOpInfo
	{
		EIROp Op = EIROp::Count;
		/** The HLSL call spelling, "dot"; nullptr when the op has none (operators, structural ops). */
		const TCHAR* HlslName = nullptr;
		/** How it reads in a message or a dump: "+", "dot(", "?:". */
		const TCHAR* Spelling = nullptr;
		int32 MinArity = 0;
		int32 MaxArity = 0;
		EIRTypingRule Typing = EIRTypingRule::Special;
		/**
		 * The engine expression class (short name, "DotProduct") when the op is one node with the
		 * operands on InputPins in order. nullptr when the emitter lowers it by hand (Select,
		 * Fmod with a constant, LogicalNot as 1-x, Broadcast...).
		 */
		const TCHAR* ExpressionClass = nullptr;
		/** Pin names in operand order, nullptr-terminated. */
		const TCHAR* InputPins[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		/**
		 * The GLSL spelling 1.x silently accepted (mix, fract, mod). The 2.0 front end REJECTS it
		 * and this is only so the diagnostic can say "write lerp". nullptr when there is none.
		 */
		const TCHAR* GlslAlias = nullptr;
	};

	/** Every op has an entry, structural and leaf ops included (with Typing == Special). */
	DREAMSHADERLANG_API const FIRCoreOpInfo& GetCoreOpInfo(EIROp Op);
	/** `dot` -> Dot. Case-sensitive. nullptr for a name that is not a core op. */
	DREAMSHADERLANG_API const FIRCoreOpInfo* FindCoreOpByHlslName(const FString& Name);
	/** `mix` -> the Lerp entry, so the diagnostic can name the HLSL spelling. nullptr when not an alias. */
	DREAMSHADERLANG_API const FIRCoreOpInfo* FindCoreOpByGlslAlias(const FString& Name);
	/** `EBinaryOp::Modulo` -> Fmod; comparison and logical operators map to the bool ops. nullptr for assignment-style ops. */
	DREAMSHADERLANG_API const FIRCoreOpInfo* FindCoreOpForBinary(Lang::EBinaryOp Op);
	/** `EUnaryOp::Minus` -> Negate, `Not` -> LogicalNot; nullptr for Plus and the increments. */
	DREAMSHADERLANG_API const FIRCoreOpInfo* FindCoreOpForUnary(Lang::EUnaryOp Op);
	/** True for the arithmetic, comparison, logic and conversion ops: everything below the structural block. */
	DREAMSHADERLANG_API bool IsCoreMathOp(EIROp Op);
}
