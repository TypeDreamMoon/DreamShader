// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The core-op table: the single place an op's arity, typing rule, engine class and pin names live.
//
// Where the facts come from:
//
//   * class + pin names for the math builtins: ported from the 1.x graph generator,
//     `DreamShaderEditor/Private/MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeMathBuiltins.cpp`,
//     which is the shipped truth for `abs .. step` and is what the 2.0 output has to match node for
//     node. Every pin name was then re-checked against the engine header's `FExpressionInput`
//     member (UE_Moon: `Engine/Source/Runtime/Engine/Public/Materials/MaterialExpression*.h`),
//     because the engine is not consistent: Power is Base/Exponent, Clamp is Input/Min/Max,
//     Normalize is VectorInput, Step is Y/X (reversed against HLSL's `step(edge, x)`), Arctangent2
//     is Y/X, Logarithm is Input but Logarithm2 and Logarithm10 are X.
//   * ExpressionClass == nullptr means "no single engine node does this" -- the emitter lowers it
//     by hand. That is the case for Negate, Rsqrt, Rcp, Fwidth, the hyperbolics, reflect/refract
//     (1.x builds a 4- and a 14-node subgraph), every comparison (they become an If) and the
//     logical operators (whose lowering depends on whether the operands are static bools).
//
// Adding an op: append to EIROp before Count, append a row here in the same position, and the
// static_assert below stops the build if the two ever drift.

#include "IR/IRCoreOps.h"

#include "Lang/LangAst.h"

namespace UE::DreamShader::IR
{
	namespace Private
	{
		// The table, in EIROp order. Columns:
		//   Op, HlslName, Spelling, MinArity, MaxArity, Typing, ExpressionClass, InputPins, GlslAlias
		static const FIRCoreOpInfo GCoreOpTable[] =
		{
			// ------------------------------------------------------------------------- leaves
			{ EIROp::Constant,               nullptr,        TEXT("constant"),    0, 0, EIRTypingRule::Special,         nullptr,                        {},                                                                           nullptr },
			{ EIROp::Parameter,              nullptr,        TEXT("uniform"),     0, 0, EIRTypingRule::Special,         nullptr,                        {},                                                                           nullptr },
			{ EIROp::TextureParameter,       nullptr,        TEXT("uniform tex"), 0, 0, EIRTypingRule::Special,         TEXT("TextureObjectParameter"), {},                                                                           nullptr },
			{ EIROp::FunctionInput,          nullptr,        TEXT("input"),       0, 0, EIRTypingRule::Special,         TEXT("FunctionInput"),          {},                                                                           nullptr },

			// --------------------------------------------------------------------- structural
			{ EIROp::Reflected,              nullptr,        TEXT("UE."),         0, 0, EIRTypingRule::Special,         nullptr,                        {},                                                                           nullptr },
			{ EIROp::FunctionCall,           nullptr,        TEXT("call"),        0, 0, EIRTypingRule::Special,         TEXT("MaterialFunctionCall"),   {},                                                                           nullptr },
			{ EIROp::Custom,                 nullptr,        TEXT("@custom"),     0, 0, EIRTypingRule::Special,         TEXT("Custom"),                 {},                                                                           nullptr },
			// Arity is 4-4, not 2-4: the operands are ALWAYS [Texture, UV, Sampler, Level] with
			// FIRValue::None() in an absent slot (IR.h FIRNode::Operands, CONTRACT §6.13 #14), so a
			// reader indexes and never counts. InputPins stops after two because the sampler slot has
			// no FExpressionInput at all on UMaterialExpressionTextureSample -- it is SamplerSource
			// plus the paired sampler object -- and the array is nullptr-terminated, so it cannot
			// carry a hole. Slot 3 goes on the `MipValue` pin; see the I1 report.
			{ EIROp::TextureSample,          nullptr,        TEXT("Sample("),     4, 4, EIRTypingRule::Special,         TEXT("TextureSample"),          { TEXT("TextureObject"), TEXT("Coordinates") },                                nullptr },
			{ EIROp::Swizzle,                nullptr,        TEXT(".xyzw"),       1, 1, EIRTypingRule::Special,         TEXT("ComponentMask"),          { TEXT("Input") },                                                            nullptr },
			{ EIROp::Append,                 nullptr,        TEXT("append("),     2, 2, EIRTypingRule::Special,         TEXT("AppendVector"),           { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Select,                 nullptr,        TEXT("?:"),          3, 3, EIRTypingRule::Special,         nullptr,                        {},                                                                           nullptr },
			{ EIROp::Compare,                nullptr,        TEXT("if("),         5, 5, EIRTypingRule::Special,         TEXT("If"),                     { TEXT("A"), TEXT("B"), TEXT("AGreaterThanB"), TEXT("AEqualsB"), TEXT("ALessThanB") }, nullptr },
			{ EIROp::StaticSwitch,           nullptr,        TEXT("static ?:"),   3, 3, EIRTypingRule::Special,         TEXT("StaticSwitch"),           { TEXT("Value"), TEXT("A"), TEXT("B") },                                      nullptr },
			{ EIROp::MakeMaterialAttributes, nullptr,        TEXT("make material"), 0, 0, EIRTypingRule::Special,       TEXT("MakeMaterialAttributes"), {},                                                                           nullptr },
			{ EIROp::SetMaterialAttributes,  nullptr,        TEXT("set material"),  0, 0, EIRTypingRule::Special,       TEXT("SetMaterialAttributes"),  {},                                                                           nullptr },
			{ EIROp::GetMaterialAttributes,  nullptr,        TEXT("break material"),0, 0, EIRTypingRule::Special,       TEXT("BreakMaterialAttributes"),{},                                                                           nullptr },
			{ EIROp::MaterialSink,           nullptr,        TEXT("material out"),  0, 0, EIRTypingRule::Special,       nullptr,                        {},                                                                           nullptr },
			{ EIROp::FunctionOutput,         nullptr,        TEXT("output"),      1, 1, EIRTypingRule::Special,         TEXT("FunctionOutput"),         { TEXT("A") },                                                                nullptr },

			// --------------------------------------------------------------------- arithmetic
			{ EIROp::Add,                    nullptr,        TEXT("+"),           2, 2, EIRTypingRule::SameAsOperands,  TEXT("Add"),                    { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Subtract,               nullptr,        TEXT("-"),           2, 2, EIRTypingRule::SameAsOperands,  TEXT("Subtract"),               { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Multiply,               nullptr,        TEXT("*"),           2, 2, EIRTypingRule::SameAsOperands,  TEXT("Multiply"),               { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Divide,                 nullptr,        TEXT("/"),           2, 2, EIRTypingRule::SameAsOperands,  TEXT("Divide"),                 { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Fmod,                   TEXT("fmod"),   TEXT("fmod("),       2, 2, EIRTypingRule::SameAsOperands,  TEXT("Fmod"),                   { TEXT("A"), TEXT("B") },                                                     TEXT("mod") },
			{ EIROp::Negate,                 nullptr,        TEXT("-"),           1, 1, EIRTypingRule::SameAsOperands,  nullptr,                        {},                                                                           nullptr },

			{ EIROp::Abs,                    TEXT("abs"),    TEXT("abs("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("Abs"),                    { TEXT("Input") },                                                            nullptr },
			{ EIROp::Floor,                  TEXT("floor"),  TEXT("floor("),      1, 1, EIRTypingRule::SameAsOperands,  TEXT("Floor"),                  { TEXT("Input") },                                                            nullptr },
			{ EIROp::Ceil,                   TEXT("ceil"),   TEXT("ceil("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Ceil"),                   { TEXT("Input") },                                                            nullptr },
			{ EIROp::Round,                  TEXT("round"),  TEXT("round("),      1, 1, EIRTypingRule::SameAsOperands,  TEXT("Round"),                  { TEXT("Input") },                                                            nullptr },
			{ EIROp::Frac,                   TEXT("frac"),   TEXT("frac("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Frac"),                   { TEXT("Input") },                                                            TEXT("fract") },
			{ EIROp::Truncate,               TEXT("trunc"),  TEXT("trunc("),      1, 1, EIRTypingRule::SameAsOperands,  TEXT("Truncate"),               { TEXT("Input") },                                                            nullptr },
			{ EIROp::Sign,                   TEXT("sign"),   TEXT("sign("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Sign"),                   { TEXT("Input") },                                                            nullptr },

			{ EIROp::Saturate,               TEXT("saturate"),   TEXT("saturate("),   1, 1, EIRTypingRule::SameAsOperands, TEXT("Saturate"),          { TEXT("Input") },                                                            nullptr },
			{ EIROp::Clamp,                  TEXT("clamp"),      TEXT("clamp("),      3, 3, EIRTypingRule::SameAsOperands, TEXT("Clamp"),             { TEXT("Input"), TEXT("Min"), TEXT("Max") },                                  nullptr },
			{ EIROp::Min,                    TEXT("min"),        TEXT("min("),        2, 2, EIRTypingRule::SameAsOperands, TEXT("Min"),               { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Max,                    TEXT("max"),        TEXT("max("),        2, 2, EIRTypingRule::SameAsOperands, TEXT("Max"),               { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Lerp,                   TEXT("lerp"),       TEXT("lerp("),       3, 3, EIRTypingRule::SameAsOperands, TEXT("LinearInterpolate"), { TEXT("A"), TEXT("B"), TEXT("Alpha") },                                      TEXT("mix") },
			{ EIROp::Step,                   TEXT("step"),       TEXT("step("),       2, 2, EIRTypingRule::SameAsOperands, TEXT("Step"),              { TEXT("Y"), TEXT("X") },                                                     nullptr },
			{ EIROp::SmoothStep,             TEXT("smoothstep"), TEXT("smoothstep("), 3, 3, EIRTypingRule::SameAsOperands, TEXT("SmoothStep"),        { TEXT("Min"), TEXT("Max"), TEXT("Value") },                                  nullptr },

			{ EIROp::Sqrt,                   TEXT("sqrt"),   TEXT("sqrt("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("SquareRoot"),             { TEXT("Input") },                                                            nullptr },
			{ EIROp::Rsqrt,                  TEXT("rsqrt"),  TEXT("rsqrt("),      1, 1, EIRTypingRule::SameAsOperands,  nullptr,                        {},                                                                           TEXT("inversesqrt") },
			{ EIROp::Rcp,                    TEXT("rcp"),    TEXT("rcp("),        1, 1, EIRTypingRule::SameAsOperands,  nullptr,                        {},                                                                           nullptr },
			{ EIROp::Pow,                    TEXT("pow"),    TEXT("pow("),        2, 2, EIRTypingRule::SameAsOperands,  TEXT("Power"),                  { TEXT("Base"), TEXT("Exponent") },                                           nullptr },
			{ EIROp::Exp,                    TEXT("exp"),    TEXT("exp("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("Exponential"),            { TEXT("Input") },                                                            nullptr },
			{ EIROp::Exp2,                   TEXT("exp2"),   TEXT("exp2("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Exponential2"),           { TEXT("Input") },                                                            nullptr },
			{ EIROp::Log,                    TEXT("log"),    TEXT("log("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("Logarithm"),              { TEXT("Input") },                                                            nullptr },
			{ EIROp::Log2,                   TEXT("log2"),   TEXT("log2("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Logarithm2"),             { TEXT("X") },                                                                nullptr },
			{ EIROp::Log10,                  TEXT("log10"),  TEXT("log10("),      1, 1, EIRTypingRule::SameAsOperands,  TEXT("Logarithm10"),            { TEXT("X") },                                                                nullptr },

			{ EIROp::Sin,                    TEXT("sin"),    TEXT("sin("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("Sine"),                   { TEXT("Input") },                                                            nullptr },
			{ EIROp::Cos,                    TEXT("cos"),    TEXT("cos("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("Cosine"),                 { TEXT("Input") },                                                            nullptr },
			{ EIROp::Tan,                    TEXT("tan"),    TEXT("tan("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("Tangent"),                { TEXT("Input") },                                                            nullptr },
			{ EIROp::Asin,                   TEXT("asin"),   TEXT("asin("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Arcsine"),                { TEXT("Input") },                                                            nullptr },
			{ EIROp::Acos,                   TEXT("acos"),   TEXT("acos("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Arccosine"),              { TEXT("Input") },                                                            nullptr },
			{ EIROp::Atan,                   TEXT("atan"),   TEXT("atan("),       1, 1, EIRTypingRule::SameAsOperands,  TEXT("Arctangent"),             { TEXT("Input") },                                                            nullptr },
			{ EIROp::Atan2,                  TEXT("atan2"),  TEXT("atan2("),      2, 2, EIRTypingRule::SameAsOperands,  TEXT("Arctangent2"),            { TEXT("Y"), TEXT("X") },                                                     nullptr },
			{ EIROp::Sinh,                   TEXT("sinh"),   TEXT("sinh("),       1, 1, EIRTypingRule::SameAsOperands,  nullptr,                        {},                                                                           nullptr },
			{ EIROp::Cosh,                   TEXT("cosh"),   TEXT("cosh("),       1, 1, EIRTypingRule::SameAsOperands,  nullptr,                        {},                                                                           nullptr },
			{ EIROp::Tanh,                   TEXT("tanh"),   TEXT("tanh("),       1, 1, EIRTypingRule::SameAsOperands,  nullptr,                        {},                                                                           nullptr },

			{ EIROp::Dot,                    TEXT("dot"),        TEXT("dot("),        2, 2, EIRTypingRule::Scalar,        TEXT("DotProduct"),        { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Cross,                  TEXT("cross"),      TEXT("cross("),      2, 2, EIRTypingRule::Float3,        TEXT("CrossProduct"),      { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Normalize,              TEXT("normalize"),  TEXT("normalize("),  1, 1, EIRTypingRule::SameAsOperands, TEXT("Normalize"),        { TEXT("VectorInput") },                                                      nullptr },
			{ EIROp::Length,                 TEXT("length"),     TEXT("length("),     1, 1, EIRTypingRule::Scalar,        TEXT("Length"),            { TEXT("Input") },                                                            nullptr },
			{ EIROp::Distance,               TEXT("distance"),   TEXT("distance("),   2, 2, EIRTypingRule::Scalar,        TEXT("Distance"),          { TEXT("A"), TEXT("B") },                                                     nullptr },
			{ EIROp::Reflect,                TEXT("reflect"),    TEXT("reflect("),    2, 2, EIRTypingRule::Float3,        nullptr,                   {},                                                                           nullptr },
			{ EIROp::Refract,                TEXT("refract"),    TEXT("refract("),    3, 3, EIRTypingRule::Float3,        nullptr,                   {},                                                                           nullptr },

			{ EIROp::DDX,                    TEXT("ddx"),    TEXT("ddx("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("DDX"),                    { TEXT("Value") },                                                            nullptr },
			{ EIROp::DDY,                    TEXT("ddy"),    TEXT("ddy("),        1, 1, EIRTypingRule::SameAsOperands,  TEXT("DDY"),                    { TEXT("Value") },                                                            nullptr },
			{ EIROp::Fwidth,                 TEXT("fwidth"), TEXT("fwidth("),     1, 1, EIRTypingRule::SameAsOperands,  nullptr,                        {},                                                                           nullptr },

			// ------------------------------------------------------------ comparison and logic
			{ EIROp::Less,                   nullptr,        TEXT("<"),           2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::LessEqual,              nullptr,        TEXT("<="),          2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::Greater,                nullptr,        TEXT(">"),           2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::GreaterEqual,           nullptr,        TEXT(">="),          2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::Equal,                  nullptr,        TEXT("=="),          2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::NotEqual,               nullptr,        TEXT("!="),          2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::LogicalAnd,             nullptr,        TEXT("&&"),          2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::LogicalOr,              nullptr,        TEXT("||"),          2, 2, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },
			{ EIROp::LogicalNot,             nullptr,        TEXT("!"),           1, 1, EIRTypingRule::Bool,            nullptr,                        {},                                                                           nullptr },

			// --------------------------------------------------------------------- conversions
			{ EIROp::Convert,                nullptr,        TEXT("convert"),     1, 1, EIRTypingRule::Special,         nullptr,                        {},                                                                           nullptr },
			{ EIROp::Broadcast,              nullptr,        TEXT("broadcast"),   1, 1, EIRTypingRule::Special,         nullptr,                        {},                                                                           nullptr },
		};

		static_assert(
			static_cast<int32>(UE_ARRAY_COUNT(GCoreOpTable)) == static_cast<int32>(EIROp::Count),
			"GCoreOpTable must hold exactly one row per EIROp, in enum order.");

		/** The answer for an op outside the table: everything null, arity 0, Typing Special. */
		static const FIRCoreOpInfo& GetUnknownCoreOpInfo()
		{
			static const FIRCoreOpInfo Unknown;
			return Unknown;
		}
	}

	const TCHAR* LexToString(const EIROp Op)
	{
		switch (Op)
		{
		case EIROp::Constant:               return TEXT("Constant");
		case EIROp::Parameter:              return TEXT("Parameter");
		case EIROp::TextureParameter:       return TEXT("TextureParameter");
		case EIROp::FunctionInput:          return TEXT("FunctionInput");

		case EIROp::Reflected:              return TEXT("Reflected");
		case EIROp::FunctionCall:           return TEXT("FunctionCall");
		case EIROp::Custom:                 return TEXT("Custom");
		case EIROp::TextureSample:          return TEXT("TextureSample");
		case EIROp::Swizzle:                return TEXT("Swizzle");
		case EIROp::Append:                 return TEXT("Append");
		case EIROp::Select:                 return TEXT("Select");
		case EIROp::Compare:                return TEXT("Compare");
		case EIROp::StaticSwitch:           return TEXT("StaticSwitch");
		case EIROp::MakeMaterialAttributes: return TEXT("MakeMaterialAttributes");
		case EIROp::SetMaterialAttributes:  return TEXT("SetMaterialAttributes");
		case EIROp::GetMaterialAttributes:  return TEXT("GetMaterialAttributes");
		case EIROp::MaterialSink:           return TEXT("MaterialSink");
		case EIROp::FunctionOutput:         return TEXT("FunctionOutput");

		case EIROp::Add:                    return TEXT("Add");
		case EIROp::Subtract:               return TEXT("Subtract");
		case EIROp::Multiply:               return TEXT("Multiply");
		case EIROp::Divide:                 return TEXT("Divide");
		case EIROp::Fmod:                   return TEXT("Fmod");
		case EIROp::Negate:                 return TEXT("Negate");
		case EIROp::Abs:                    return TEXT("Abs");
		case EIROp::Floor:                  return TEXT("Floor");
		case EIROp::Ceil:                   return TEXT("Ceil");
		case EIROp::Round:                  return TEXT("Round");
		case EIROp::Frac:                   return TEXT("Frac");
		case EIROp::Truncate:               return TEXT("Truncate");
		case EIROp::Sign:                   return TEXT("Sign");
		case EIROp::Saturate:               return TEXT("Saturate");
		case EIROp::Clamp:                  return TEXT("Clamp");
		case EIROp::Min:                    return TEXT("Min");
		case EIROp::Max:                    return TEXT("Max");
		case EIROp::Lerp:                   return TEXT("Lerp");
		case EIROp::Step:                   return TEXT("Step");
		case EIROp::SmoothStep:             return TEXT("SmoothStep");
		case EIROp::Sqrt:                   return TEXT("Sqrt");
		case EIROp::Rsqrt:                  return TEXT("Rsqrt");
		case EIROp::Rcp:                    return TEXT("Rcp");
		case EIROp::Pow:                    return TEXT("Pow");
		case EIROp::Exp:                    return TEXT("Exp");
		case EIROp::Exp2:                   return TEXT("Exp2");
		case EIROp::Log:                    return TEXT("Log");
		case EIROp::Log2:                   return TEXT("Log2");
		case EIROp::Log10:                  return TEXT("Log10");
		case EIROp::Sin:                    return TEXT("Sin");
		case EIROp::Cos:                    return TEXT("Cos");
		case EIROp::Tan:                    return TEXT("Tan");
		case EIROp::Asin:                   return TEXT("Asin");
		case EIROp::Acos:                   return TEXT("Acos");
		case EIROp::Atan:                   return TEXT("Atan");
		case EIROp::Atan2:                  return TEXT("Atan2");
		case EIROp::Sinh:                   return TEXT("Sinh");
		case EIROp::Cosh:                   return TEXT("Cosh");
		case EIROp::Tanh:                   return TEXT("Tanh");
		case EIROp::Dot:                    return TEXT("Dot");
		case EIROp::Cross:                  return TEXT("Cross");
		case EIROp::Normalize:              return TEXT("Normalize");
		case EIROp::Length:                 return TEXT("Length");
		case EIROp::Distance:               return TEXT("Distance");
		case EIROp::Reflect:                return TEXT("Reflect");
		case EIROp::Refract:                return TEXT("Refract");
		case EIROp::DDX:                    return TEXT("DDX");
		case EIROp::DDY:                    return TEXT("DDY");
		case EIROp::Fwidth:                 return TEXT("Fwidth");

		case EIROp::Less:                   return TEXT("Less");
		case EIROp::LessEqual:              return TEXT("LessEqual");
		case EIROp::Greater:                return TEXT("Greater");
		case EIROp::GreaterEqual:           return TEXT("GreaterEqual");
		case EIROp::Equal:                  return TEXT("Equal");
		case EIROp::NotEqual:               return TEXT("NotEqual");
		case EIROp::LogicalAnd:             return TEXT("LogicalAnd");
		case EIROp::LogicalOr:              return TEXT("LogicalOr");
		case EIROp::LogicalNot:             return TEXT("LogicalNot");

		case EIROp::Convert:                return TEXT("Convert");
		case EIROp::Broadcast:              return TEXT("Broadcast");

		case EIROp::Count:                  break;
		}

		return TEXT("Unknown");
	}

	const FIRCoreOpInfo& GetCoreOpInfo(const EIROp Op)
	{
		const int32 Index = static_cast<int32>(Op);
		if (Index < 0 || Index >= static_cast<int32>(UE_ARRAY_COUNT(Private::GCoreOpTable)))
		{
			return Private::GetUnknownCoreOpInfo();
		}

		const FIRCoreOpInfo& Info = Private::GCoreOpTable[Index];
		if (Info.Op == Op)
		{
			return Info;
		}

		// The static_assert only guards the COUNT. If a row were ever transposed the indexed
		// lookup would quietly answer the wrong op, so fall back to a scan rather than lie.
		for (const FIRCoreOpInfo& Candidate : Private::GCoreOpTable)
		{
			if (Candidate.Op == Op)
			{
				return Candidate;
			}
		}

		return Private::GetUnknownCoreOpInfo();
	}

	const FIRCoreOpInfo* FindCoreOpByHlslName(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}

		// A linear scan of 78 rows, once per call expression. A TMap is not an option: TMap<FString>
		// compares keys with FString::operator==, which is case-INSENSITIVE, and `Dot` must not
		// resolve to `dot` in a case-sensitive language.
		for (const FIRCoreOpInfo& Info : Private::GCoreOpTable)
		{
			if (Info.HlslName != nullptr && Name.Equals(Info.HlslName, ESearchCase::CaseSensitive))
			{
				return &Info;
			}
		}

		return nullptr;
	}

	const FIRCoreOpInfo* FindCoreOpByGlslAlias(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return nullptr;
		}

		for (const FIRCoreOpInfo& Info : Private::GCoreOpTable)
		{
			if (Info.GlslAlias != nullptr && Name.Equals(Info.GlslAlias, ESearchCase::CaseSensitive))
			{
				return &Info;
			}
		}

		return nullptr;
	}

	const FIRCoreOpInfo* FindCoreOpForBinary(const Lang::EBinaryOp Op)
	{
		switch (Op)
		{
		case Lang::EBinaryOp::Multiply:     return &GetCoreOpInfo(EIROp::Multiply);
		case Lang::EBinaryOp::Divide:       return &GetCoreOpInfo(EIROp::Divide);
		// `%` on floats is fmod, which is what the graph's Fmod node computes. There is no integer
		// remainder in a material graph, so one op covers both spellings.
		case Lang::EBinaryOp::Modulo:       return &GetCoreOpInfo(EIROp::Fmod);
		case Lang::EBinaryOp::Add:          return &GetCoreOpInfo(EIROp::Add);
		case Lang::EBinaryOp::Subtract:     return &GetCoreOpInfo(EIROp::Subtract);
		case Lang::EBinaryOp::Less:         return &GetCoreOpInfo(EIROp::Less);
		case Lang::EBinaryOp::LessEqual:    return &GetCoreOpInfo(EIROp::LessEqual);
		case Lang::EBinaryOp::Greater:      return &GetCoreOpInfo(EIROp::Greater);
		case Lang::EBinaryOp::GreaterEqual: return &GetCoreOpInfo(EIROp::GreaterEqual);
		case Lang::EBinaryOp::Equal:        return &GetCoreOpInfo(EIROp::Equal);
		case Lang::EBinaryOp::NotEqual:     return &GetCoreOpInfo(EIROp::NotEqual);
		case Lang::EBinaryOp::LogicalAnd:   return &GetCoreOpInfo(EIROp::LogicalAnd);
		case Lang::EBinaryOp::LogicalOr:    return &GetCoreOpInfo(EIROp::LogicalOr);

		// The bitwise and shift operators have no graph form at all. The binder turns a null here
		// into its own refusal, which is where the "use @custom" advice belongs.
		case Lang::EBinaryOp::ShiftLeft:
		case Lang::EBinaryOp::ShiftRight:
		case Lang::EBinaryOp::BitwiseAnd:
		case Lang::EBinaryOp::BitwiseXor:
		case Lang::EBinaryOp::BitwiseOr:
			return nullptr;
		}

		return nullptr;
	}

	const FIRCoreOpInfo* FindCoreOpForUnary(const Lang::EUnaryOp Op)
	{
		switch (Op)
		{
		// The AST spells unary minus `Negate` (LangAst.h EUnaryOp), not `Minus`.
		case Lang::EUnaryOp::Negate:     return &GetCoreOpInfo(EIROp::Negate);
		case Lang::EUnaryOp::LogicalNot: return &GetCoreOpInfo(EIROp::LogicalNot);

		// Unary plus is the identity, so it has no node; `~` has no graph form; the increments are
		// assignments and the binder rewrites them before it ever asks for an op.
		case Lang::EUnaryOp::Plus:
		case Lang::EUnaryOp::BitwiseNot:
		case Lang::EUnaryOp::PreIncrement:
		case Lang::EUnaryOp::PreDecrement:
		case Lang::EUnaryOp::PostIncrement:
		case Lang::EUnaryOp::PostDecrement:
			return nullptr;
		}

		return nullptr;
	}

	bool IsCoreMathOp(const EIROp Op)
	{
		// "Everything below the structural block": Add .. Broadcast, which is exactly the
		// arithmetic, comparison, logic and conversion range.
		const int32 Index = static_cast<int32>(Op);
		return Index >= static_cast<int32>(EIROp::Add) && Index <= static_cast<int32>(EIROp::Broadcast);
	}
}
