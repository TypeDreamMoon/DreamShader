// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Expressions: the type of every node, the thing every name refers to, and the way every argument
// reaches the pin, property or parameter it feeds.
//
// Three rules run through all of it.
//
//   1. A failure types the expression Error and keeps going. ClassifyConversion lets Error stand in
//      for anything, so one bad sub-expression produces one message and not a column of them. That
//      is the whole reason this pass does not stop at the first mistake.
//   2. A call's CALLEE is never recorded in FBoundModule::Expressions. `float3`, `UE`, `Substrate`,
//      `dot` and the name of a helper all name a thing, not a value; the object of `Tex.Sample(UV)`
//      IS a value and is recorded. The IR builder can rely on that: anything it finds in the map
//      has a type and a lowering.
//   3. The core-op table (IRCoreOps.h) is the only place an arity or a typing rule is written. This
//      file reads Typing, MinArity, MaxArity and InputPins from it and holds no copy.

#include "LangBinderInternal.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "Misc/CString.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.Expressions"

namespace UE::DreamShader::Lang::Private
{
	namespace
	{
		/** One folded operand: up to four components, broadcast on demand. */
		struct FConstOperand
		{
			double V[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 N = 1;

			double At(int32 Index) const { return V[N == 1 ? 0 : FMath::Clamp(Index, 0, N - 1)]; }
		};

		double SafeDivide(double A, double B)
		{
			// A folded division by zero would bake an infinity into a Constant node. Leaving the
			// numerator alone keeps the graph finite and the mistake visible in the preview; the
			// shader compiler is where a real divide by zero belongs.
			return B == 0.0 ? A : A / B;
		}

		bool FoldComponentWise(IR::EIROp Op, const TArray<FConstOperand>& In, int32 Width, double Out[4])
		{
			for (int32 Index = 0; Index < Width; ++Index)
			{
				const double A = In.Num() > 0 ? In[0].At(Index) : 0.0;
				const double B = In.Num() > 1 ? In[1].At(Index) : 0.0;
				const double C = In.Num() > 2 ? In[2].At(Index) : 0.0;
				double R = 0.0;

				switch (Op)
				{
				case IR::EIROp::Add:          R = A + B; break;
				case IR::EIROp::Subtract:     R = A - B; break;
				case IR::EIROp::Multiply:     R = A * B; break;
				case IR::EIROp::Divide:       R = SafeDivide(A, B); break;
				case IR::EIROp::Fmod:         R = (B == 0.0) ? 0.0 : (A - B * FMath::TruncToDouble(A / B)); break;
				case IR::EIROp::Negate:       R = -A; break;
				case IR::EIROp::Abs:          R = FMath::Abs(A); break;
				case IR::EIROp::Floor:        R = FMath::FloorToDouble(A); break;
				case IR::EIROp::Ceil:         R = FMath::CeilToDouble(A); break;
				case IR::EIROp::Round:        R = FMath::RoundToDouble(A); break;
				case IR::EIROp::Frac:         R = A - FMath::FloorToDouble(A); break;
				case IR::EIROp::Truncate:     R = FMath::TruncToDouble(A); break;
				case IR::EIROp::Sign:         R = (A > 0.0) ? 1.0 : ((A < 0.0) ? -1.0 : 0.0); break;
				case IR::EIROp::Saturate:     R = FMath::Clamp(A, 0.0, 1.0); break;
				case IR::EIROp::Clamp:        R = FMath::Clamp(A, B, C); break;
				case IR::EIROp::Min:          R = FMath::Min(A, B); break;
				case IR::EIROp::Max:          R = FMath::Max(A, B); break;
				case IR::EIROp::Lerp:         R = A + (B - A) * C; break;
				case IR::EIROp::Step:         R = (B >= A) ? 1.0 : 0.0; break;
				case IR::EIROp::SmoothStep:
				{
					const double T = (B == A) ? 0.0 : FMath::Clamp((C - A) / (B - A), 0.0, 1.0);
					R = T * T * (3.0 - 2.0 * T);
					break;
				}
				case IR::EIROp::Sqrt:         R = (A < 0.0) ? 0.0 : FMath::Sqrt(A); break;
				case IR::EIROp::Rsqrt:        R = (A <= 0.0) ? 0.0 : (1.0 / FMath::Sqrt(A)); break;
				case IR::EIROp::Rcp:          R = SafeDivide(1.0, A); break;
				case IR::EIROp::Pow:          R = FMath::Pow(A, B); break;
				case IR::EIROp::Exp:          R = FMath::Exp(A); break;
				case IR::EIROp::Exp2:         R = FMath::Pow(2.0, A); break;
				case IR::EIROp::Log:          R = (A <= 0.0) ? 0.0 : FMath::Loge(A); break;
				case IR::EIROp::Log2:         R = (A <= 0.0) ? 0.0 : (FMath::Loge(A) / FMath::Loge(2.0)); break;
				case IR::EIROp::Log10:        R = (A <= 0.0) ? 0.0 : (FMath::Loge(A) / FMath::Loge(10.0)); break;
				case IR::EIROp::Sin:          R = FMath::Sin(A); break;
				case IR::EIROp::Cos:          R = FMath::Cos(A); break;
				case IR::EIROp::Tan:          R = FMath::Tan(A); break;
				case IR::EIROp::Asin:         R = FMath::Asin(FMath::Clamp(A, -1.0, 1.0)); break;
				case IR::EIROp::Acos:         R = FMath::Acos(FMath::Clamp(A, -1.0, 1.0)); break;
				case IR::EIROp::Atan:         R = FMath::Atan(A); break;
				case IR::EIROp::Atan2:        R = FMath::Atan2(A, B); break;
				// Sinh / Cosh / Tanh never reach here: BindCoreOpCall refuses them (DSH4246), because
				// the graph has no hyperbolic node to lower them onto.
				case IR::EIROp::Less:         R = (A < B) ? 1.0 : 0.0; break;
				case IR::EIROp::LessEqual:    R = (A <= B) ? 1.0 : 0.0; break;
				case IR::EIROp::Greater:      R = (A > B) ? 1.0 : 0.0; break;
				case IR::EIROp::GreaterEqual: R = (A >= B) ? 1.0 : 0.0; break;
				case IR::EIROp::Equal:        R = (A == B) ? 1.0 : 0.0; break;
				case IR::EIROp::NotEqual:     R = (A != B) ? 1.0 : 0.0; break;
				case IR::EIROp::LogicalAnd:   R = (A != 0.0 && B != 0.0) ? 1.0 : 0.0; break;
				case IR::EIROp::LogicalOr:    R = (A != 0.0 || B != 0.0) ? 1.0 : 0.0; break;
				case IR::EIROp::LogicalNot:   R = (A == 0.0) ? 1.0 : 0.0; break;
				case IR::EIROp::Convert:      R = A; break;
				case IR::EIROp::Broadcast:    R = In.Num() > 0 ? In[0].At(0) : 0.0; break;
				default:
					return false;
				}

				Out[Index] = R;
			}
			return true;
		}

		bool FoldOp(IR::EIROp Op, const TArray<FConstOperand>& In, int32 Width, double Out[4])
		{
			Out[0] = Out[1] = Out[2] = Out[3] = 0.0;

			// The reductions, which do not run component-wise.
			switch (Op)
			{
			case IR::EIROp::Dot:
			{
				if (In.Num() != 2)
				{
					return false;
				}
				const int32 N = FMath::Max(In[0].N, In[1].N);
				double Sum = 0.0;
				for (int32 Index = 0; Index < N; ++Index)
				{
					Sum += In[0].At(Index) * In[1].At(Index);
				}
				Out[0] = Sum;
				return true;
			}
			case IR::EIROp::Length:
			case IR::EIROp::Distance:
			{
				if (In.Num() < 1)
				{
					return false;
				}
				const int32 N = (In.Num() > 1) ? FMath::Max(In[0].N, In[1].N) : In[0].N;
				double Sum = 0.0;
				for (int32 Index = 0; Index < N; ++Index)
				{
					const double D = (In.Num() > 1) ? (In[0].At(Index) - In[1].At(Index)) : In[0].At(Index);
					Sum += D * D;
				}
				Out[0] = FMath::Sqrt(Sum);
				return true;
			}
			case IR::EIROp::Normalize:
			{
				if (In.Num() != 1)
				{
					return false;
				}
				double Sum = 0.0;
				for (int32 Index = 0; Index < In[0].N; ++Index)
				{
					Sum += In[0].At(Index) * In[0].At(Index);
				}
				const double Len = FMath::Sqrt(Sum);
				if (Len == 0.0)
				{
					return false;
				}
				for (int32 Index = 0; Index < Width; ++Index)
				{
					Out[Index] = In[0].At(Index) / Len;
				}
				return true;
			}
			case IR::EIROp::Cross:
			{
				if (In.Num() != 2 || Width != 3)
				{
					return false;
				}
				Out[0] = In[0].At(1) * In[1].At(2) - In[0].At(2) * In[1].At(1);
				Out[1] = In[0].At(2) * In[1].At(0) - In[0].At(0) * In[1].At(2);
				Out[2] = In[0].At(0) * In[1].At(1) - In[0].At(1) * In[1].At(0);
				return true;
			}
			// Screen-space derivatives of a constant are zero, but folding them away would drop a
			// node the author asked for and the shader compiler may treat differently. Left alone.
			case IR::EIROp::DDX:
			case IR::EIROp::DDY:
			case IR::EIROp::Fwidth:
			case IR::EIROp::Reflect:
			case IR::EIROp::Refract:
				return false;
			default:
				break;
			}

			return FoldComponentWise(Op, In, Width, Out);
		}

		/** The one component a swizzle letter names, or INDEX_NONE. bOutRgba says which family it came from. */
		int32 SwizzleComponent(TCHAR Char, bool& bOutRgba)
		{
			switch (FChar::ToLower(Char))
			{
			case TEXT('x'): bOutRgba = false; return 0;
			case TEXT('y'): bOutRgba = false; return 1;
			case TEXT('z'): bOutRgba = false; return 2;
			case TEXT('w'): bOutRgba = false; return 3;
			case TEXT('r'): bOutRgba = true;  return 0;
			case TEXT('g'): bOutRgba = true;  return 1;
			case TEXT('b'): bOutRgba = true;  return 2;
			case TEXT('a'): bOutRgba = true;  return 3;
			default: return INDEX_NONE;
			}
		}

		const TCHAR* GSwizzleLetters = TEXT("xyzw");

		/** True when a canonical mask names one component more than once (`xx`, `xyx`). */
		bool HasRepeatedComponent(const FString& Mask)
		{
			for (int32 Index = 0; Index < Mask.Len(); ++Index)
			{
				for (int32 Other = Index + 1; Other < Mask.Len(); ++Other)
				{
					if (Mask[Index] == Mask[Other])
					{
						return true;
					}
				}
			}
			return false;
		}

		/** `Engine.MaterialExpressionX` / `"MaterialExpressionX"` / `MaterialExpressionX` as one string. */
		bool FlattenClassSpecifier(const FExpr& Expr, FString& Out)
		{
			if (const FLiteralExpr* Literal = Expr.As<FLiteralExpr>())
			{
				if (Literal->LiteralKind == ELiteralKind::String)
				{
					Out = Literal->Text;
					return true;
				}
				return false;
			}
			if (const FIdentifierExpr* Identifier = Expr.As<FIdentifierExpr>())
			{
				Out = Identifier->Name;
				return true;
			}
			if (const FTypeExpr* TypeExpr = Expr.As<FTypeExpr>())
			{
				Out = TypeExpr->Type.Name;
				return true;
			}
			if (const FMemberExpr* Member = Expr.As<FMemberExpr>())
			{
				FString Prefix;
				if (Member->Object && FlattenClassSpecifier(*Member->Object, Prefix))
				{
					Out = Prefix + TEXT(".") + Member->Member;
					return true;
				}
			}
			return false;
		}

		/** `EAssignOp::AddAssign` -> `EBinaryOp::Add`; false for a plain `=`. */
		bool CompoundAssignOp(EAssignOp Op, EBinaryOp& OutBinary)
		{
			switch (Op)
			{
			case EAssignOp::AddAssign:        OutBinary = EBinaryOp::Add; return true;
			case EAssignOp::SubtractAssign:   OutBinary = EBinaryOp::Subtract; return true;
			case EAssignOp::MultiplyAssign:   OutBinary = EBinaryOp::Multiply; return true;
			case EAssignOp::DivideAssign:     OutBinary = EBinaryOp::Divide; return true;
			case EAssignOp::ModuloAssign:     OutBinary = EBinaryOp::Modulo; return true;
			case EAssignOp::AndAssign:        OutBinary = EBinaryOp::BitwiseAnd; return true;
			case EAssignOp::OrAssign:         OutBinary = EBinaryOp::BitwiseOr; return true;
			case EAssignOp::XorAssign:        OutBinary = EBinaryOp::BitwiseXor; return true;
			case EAssignOp::ShiftLeftAssign:  OutBinary = EBinaryOp::ShiftLeft; return true;
			case EAssignOp::ShiftRightAssign: OutBinary = EBinaryOp::ShiftRight; return true;
			default: return false;
			}
		}

		const TCHAR* BinaryOpSpelling(EBinaryOp Op)
		{
			switch (Op)
			{
			case EBinaryOp::ShiftLeft:  return TEXT("<<");
			case EBinaryOp::ShiftRight: return TEXT(">>");
			case EBinaryOp::BitwiseAnd: return TEXT("&");
			case EBinaryOp::BitwiseXor: return TEXT("^");
			case EBinaryOp::BitwiseOr:  return TEXT("|");
			default: return TEXT("?");
			}
		}

		/**
		 * The channels an output's name spells, as a mask with R = 1, G = 2, B = 4, A = 8; 0 when the name
		 * is not a channel name.
		 *
		 * A channel name is one to four of `RGBA`, upper case, in channel order, no letter twice -- `RGB`,
		 * `R`, `A`, `RGBA`. The engine names the component views it does name that way (TextureSample,
		 * ParticleColor, SceneColor), and the reflection catalog names the unnamed ones the same way
		 * (VertexColor, DynamicParameter), because the catalog carries no masks and the name is the one
		 * place it can say "this output is a view".
		 */
		int32 ChannelViewMaskOfName(const FString& Name)
		{
			if (Name.Len() == 0 || Name.Len() > 4)
			{
				return 0;
			}

			int32 Mask = 0;
			int32 Previous = INDEX_NONE;
			for (int32 Index = 0; Index < Name.Len(); ++Index)
			{
				int32 Channel = INDEX_NONE;
				switch (Name[Index])
				{
				case TEXT('R'): Channel = 0; break;
				case TEXT('G'): Channel = 1; break;
				case TEXT('B'): Channel = 2; break;
				case TEXT('A'): Channel = 3; break;
				default: return 0;
				}
				if (Channel <= Previous)
				{
					return 0;
				}
				Mask |= 1 << Channel;
				Previous = Channel;
			}
			return Mask;
		}

		/**
		 * Whether every output of a multi-output class is a channel VIEW of one value, and how wide that
		 * value is.
		 *
		 * This is the line 1.x's TryRetargetChannelMaskToOutput (DreamShaderMaterialGeneratorCodeShared.h)
		 * draws: when every output is masked, the outputs are component views of the same value --
		 * VertexColor's RGB, R, G, B, A are all cut from the one float4 its Compile returns -- and when the
		 * set is mixed they are different values (SceneTexture's Color, Size, InvSize). A view is an output
		 * whose name is a channel name as wide as the output itself (`RGB` is a float3). The value is as
		 * wide as the run of channels, from R, that the views cover between them; a set that skips one (R
		 * and A alone) has no whole value to stand for.
		 */
		bool TryGetChannelViewWidth(const IR::FCatalogExpression& Class, int32& OutWidth)
		{
			OutWidth = 0;
			if (Class.Outputs.Num() < 2)
			{
				return false;
			}

			int32 Covered = 0;
			for (const IR::FCatalogPin& Output : Class.Outputs)
			{
				const int32 Mask = ChannelViewMaskOfName(Output.Name);
				const IR::FIRType Type = IR::TypeFromCatalogValueType(Output.Type);
				if (Mask == 0
					|| Output.Type == IR::ECatalogValueType::Numeric
					|| Type.Kind != IR::EIRTypeKind::Float
					|| Type.Cols != 1
					|| Type.Rows != Output.Name.Len())
				{
					return false;
				}
				Covered |= Mask;
			}

			while (OutWidth < 4 && (Covered & (1 << OutWidth)) != 0)
			{
				++OutWidth;
			}
			return OutWidth >= 2;
		}

		/**
		 * How many channels the DEFAULT output carries when it is the leading view of the value: 3 for
		 * VertexColor's `RGB`, 1 for DynamicParameter's `R`, 0 when output 0 is not a view that starts at
		 * R. EIRConversion::DefaultOutput is output 0, so this is how far a value read through it is real.
		 */
		int32 GetLeadingChannelViewWidth(const IR::FCatalogExpression& Class)
		{
			if (Class.Outputs.IsEmpty())
			{
				return 0;
			}
			const FString& Name = Class.Outputs[0].Name;
			const int32 Mask = ChannelViewMaskOfName(Name);
			return (Mask != 0 && Mask == (1 << Name.Len()) - 1) ? Name.Len() : 0;
		}

		/** The output whose channels are exactly those of a canonical `xyzw` mask, or INDEX_NONE. */
		int32 FindChannelViewOutput(const IR::FCatalogExpression& Class, const FString& CanonicalMask)
		{
			int32 Wanted = 0;
			int32 Previous = INDEX_NONE;
			for (int32 Index = 0; Index < CanonicalMask.Len(); ++Index)
			{
				bool bRgba = false;
				const int32 Component = SwizzleComponent(CanonicalMask[Index], bRgba);
				if (Component == INDEX_NONE || Component <= Previous)
				{
					// A reordering or a repeat is never one output: `.bgr` is three reads and an Append.
					return INDEX_NONE;
				}
				Wanted |= 1 << Component;
				Previous = Component;
			}

			for (int32 OutputIndex = 0; Wanted != 0 && OutputIndex < Class.Outputs.Num(); ++OutputIndex)
			{
				if (ChannelViewMaskOfName(Class.Outputs[OutputIndex].Name) == Wanted)
				{
					return OutputIndex;
				}
			}
			return INDEX_NONE;
		}

		/** One to four swizzle letters, and nothing else: whether a member could be a swizzle at all. */
		bool IsChannelViewSwizzleSpelling(const FString& Member)
		{
			if (Member.Len() == 0 || Member.Len() > 4)
			{
				return false;
			}
			for (int32 Index = 0; Index < Member.Len(); ++Index)
			{
				bool bRgba = false;
				if (SwizzleComponent(Member[Index], bRgba) == INDEX_NONE)
				{
					return false;
				}
			}
			return true;
		}

		/** `'Color', 'Size', 'InvSize'` -- a class's output names, for a message. Empty when none has a name. */
		FString QuoteNodeOutputNames(const IR::FCatalogExpression& Class)
		{
			FString Out;
			for (const IR::FCatalogPin& Output : Class.Outputs)
			{
				if (Output.Name.IsEmpty())
				{
					continue;
				}
				if (!Out.IsEmpty())
				{
					Out += TEXT(", ");
				}
				Out += TEXT("'") + Output.Name + TEXT("'");
			}
			return Out;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Dispatch
	// ---------------------------------------------------------------------------------------------

	IR::FIRType FLangBinder::BindExpr(const FExpr& Expr, const IR::FIRType* Expected, bool bStatement)
	{
		switch (Expr.Kind)
		{
		case ENodeKind::LiteralExpr:
			return BindLiteral(*static_cast<const FLiteralExpr*>(&Expr));

		case ENodeKind::IdentifierExpr:
			return BindIdentifier(*static_cast<const FIdentifierExpr*>(&Expr));

		case ENodeKind::TypeExpr:
		{
			const FTypeExpr& TypeExpr = *static_cast<const FTypeExpr*>(&Expr);
			Diagnostics.Error(
				TEXT("DSH4204"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("TypeAsValue", "'{0}' is a type, not a value; write '{0}(...)' to construct one."),
					FText::FromString(TypeExpr.Type.Name)));
			return Fail(Expr);
		}

		case ENodeKind::MemberExpr:
			return BindMember(*static_cast<const FMemberExpr*>(&Expr));

		case ENodeKind::IndexExpr:
			return BindIndex(*static_cast<const FIndexExpr*>(&Expr));

		case ENodeKind::CallExpr:
			return BindCall(*static_cast<const FCallExpr*>(&Expr), Expected, bStatement);

		case ENodeKind::UnaryExpr:
			return BindUnary(*static_cast<const FUnaryExpr*>(&Expr));

		case ENodeKind::BinaryExpr:
			return BindBinary(*static_cast<const FBinaryExpr*>(&Expr));

		case ENodeKind::AssignExpr:
			return BindAssign(*static_cast<const FAssignExpr*>(&Expr));

		case ENodeKind::ConditionalExpr:
			return BindConditional(*static_cast<const FConditionalExpr*>(&Expr));

		case ENodeKind::CastExpr:
			return BindCast(*static_cast<const FCastExpr*>(&Expr));

		case ENodeKind::InitializerListExpr:
			return BindInitializerList(*static_cast<const FInitializerListExpr*>(&Expr), Expected);

		case ENodeKind::ParenExpr:
		{
			const FParenExpr& Paren = *static_cast<const FParenExpr*>(&Expr);
			if (!Paren.Inner)
			{
				return Fail(Expr);
			}

			const IR::FIRType Inner = BindExpr(*Paren.Inner, Expected, bStatement);

			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::Paren;
			Binding.Type = Inner;
			Binding.bLValue = IsLValue(*Paren.Inner);
			if (const FBoundExpr* Found = Lookup(*Paren.Inner))
			{
				Binding.bIsConstant = Found->bIsConstant;
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Binding.ConstantValue[Index] = Found->ConstantValue[Index];
				}
			}
			return Emit(Expr, MoveTemp(Binding));
		}

		default:
			break;
		}

		return Fail(Expr);
	}

	// ---------------------------------------------------------------------------------------------
	// Leaves
	// ---------------------------------------------------------------------------------------------

	IR::FIRType FLangBinder::BindLiteral(const FLiteralExpr& Expr)
	{
		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Literal;
		Binding.bIsConstant = true;

		switch (Expr.LiteralKind)
		{
		case ELiteralKind::Int:
			Binding.Type = IR::FIRType::Scalar(IR::EIRTypeKind::Int);
			Binding.ConstantValue[0] = static_cast<double>(static_cast<int64>(Expr.Integer));
			break;

		case ELiteralKind::UInt:
			Binding.Type = IR::FIRType::Scalar(IR::EIRTypeKind::UInt);
			Binding.ConstantValue[0] = static_cast<double>(Expr.Integer);
			break;

		case ELiteralKind::Float:
		{
			// The lexeme keeps its suffix, and `0.5h` is a half in HLSL. The graph makes every one
			// of these a float; the honest kind matters to a `/// @custom` signature.
			const bool bHalf = Expr.Text.EndsWith(TEXT("h"), ESearchCase::IgnoreCase);
			Binding.Type = IR::FIRType::Scalar(bHalf ? IR::EIRTypeKind::Half : IR::EIRTypeKind::Float);
			Binding.ConstantValue[0] = Expr.Real;
			break;
		}

		case ELiteralKind::Bool:
			Binding.Type = IR::FIRType::Bool(1);
			Binding.ConstantValue[0] = Expr.bBool ? 1.0 : 0.0;
			break;

		case ELiteralKind::String:
		default:
			// A string is only ever a reflected property value, and that path never reaches here:
			// BindPropertyArgument records the binding itself.
			Binding.Kind = EBoundExprKind::Literal;
			Binding.Type = IR::FIRType::Error();
			Binding.bIsConstant = false;
			Diagnostics.Error(
				TEXT("DSH4202"),
				CurrentFile,
				Expr.Span,
				LOCTEXT("StringInExpression", "A string has no value in an expression; it is only ever the value of a reflected property, as in 'UE.Expression(Class = \"...\")'."));
			break;
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindIdentifier(const FIdentifierExpr& Expr)
	{
		FBoundExpr Binding;

		const int32 LocalSlot = FindLocal(Expr.Name);
		if (LocalSlot != INDEX_NONE && CurrentFunction && CurrentFunction->Locals.IsValidIndex(LocalSlot))
		{
			const FBoundLocal& Local = CurrentFunction->Locals[LocalSlot];
			Binding.Kind = EBoundExprKind::Local;
			Binding.LocalSlot = LocalSlot;
			Binding.Type = Local.Type;
			Binding.bLValue = true;
			return Emit(Expr, MoveTemp(Binding));
		}

		const int32 ParamIndex = FindParam(Expr.Name);
		if (ParamIndex != INDEX_NONE)
		{
			const FBoundParam& Param = CurrentFunction->Params[ParamIndex];
			Binding.Kind = EBoundExprKind::Param;
			Binding.Index = ParamIndex;
			Binding.Type = Param.Type;
			// CONTRACT §5: only an `out` / `inout` parameter is an lvalue. An `in` parameter is a
			// function input pin, and a graph cannot write to one.
			Binding.bLValue = Param.Direction != EParamDirection::In;
			return Emit(Expr, MoveTemp(Binding));
		}

		const int32 GlobalIndex = FindGlobal(Expr.Name);
		if (GlobalIndex != INDEX_NONE)
		{
			const FBoundGlobal& Global = Bound.Globals[GlobalIndex];
			Binding.Kind = EBoundExprKind::Global;
			Binding.Index = GlobalIndex;
			Binding.Type = Global.Type;
			Binding.bLValue = false;

			if (Global.bIsConstant && Global.Decl && Global.Decl->Declarator.Initializer)
			{
				// A `static const` read is a constant, which is what makes `Steps * 2` fold and a
				// `for` bounded by one provable.
				if (const FBoundExpr* Init = Lookup(*Global.Decl->Declarator.Initializer))
				{
					if (Init->bIsConstant && GetGlobalArrayCount(GlobalIndex) == 0)
					{
						Binding.bIsConstant = true;
						for (int32 Index = 0; Index < 4; ++Index)
						{
							Binding.ConstantValue[Index] = Init->ConstantValue[Index];
						}
					}
				}
			}
			return Emit(Expr, MoveTemp(Binding));
		}

		if (Expr.Name.Equals(Namespaces::UE, ESearchCase::CaseSensitive)
			|| Expr.Name.Equals(Namespaces::Substrate, ESearchCase::CaseSensitive))
		{
			Diagnostics.Error(
				TEXT("DSH4203"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("NamespaceAsValue", "'{0}' is a namespace, not a value; write '{0}.SomeNode(...)'."),
					FText::FromString(Expr.Name)));
			return Fail(Expr);
		}

		// "Did you mean" over everything that is in scope, because a case slip is the mistake this
		// language invites: 1.x was case-insensitive and 2.0 is not.
		TArray<FString> Candidates;
		if (CurrentFunction)
		{
			for (const FBoundLocal& Local : CurrentFunction->Locals)
			{
				Candidates.Add(Local.Name);
			}
			for (const FBoundParam& Param : CurrentFunction->Params)
			{
				Candidates.Add(Param.Name);
			}
		}
		for (const FBoundGlobal& Global : Bound.Globals)
		{
			Candidates.Add(Global.Name);
		}

		const FString Suggestion = SuggestCaseInsensitive(Expr.Name, Candidates);
		if (!Suggestion.IsEmpty())
		{
			Diagnostics.Error(
				TEXT("DSH4200"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("UnknownNameDidYouMean", "'{0}' is not declared; did you mean '{1}'? Names are case-sensitive."),
					FText::FromString(Expr.Name),
					FText::FromString(Suggestion)));
		}
		else
		{
			Diagnostics.Error(
				TEXT("DSH4200"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("UnknownName", "'{0}' is not declared in this scope."),
					FText::FromString(Expr.Name)));
		}
		return Fail(Expr);
	}

	// ---------------------------------------------------------------------------------------------
	// Members and swizzles
	// ---------------------------------------------------------------------------------------------

	bool FLangBinder::IsNamespaceRoot(const FExpr& Object, FString& OutNamespace) const
	{
		if (const FIdentifierExpr* Identifier = Object.As<FIdentifierExpr>())
		{
			// A declared name wins over the namespace: an author who wrote `float UE = 1;` gets the
			// variable, and the reflected call they wanted is then the one that fails.
			const bool bShadowed =
				FindLocal(Identifier->Name) != INDEX_NONE
				|| FindParam(Identifier->Name) != INDEX_NONE
				|| FindGlobal(Identifier->Name) != INDEX_NONE;
			if (bShadowed)
			{
				return false;
			}

			if (Identifier->Name.Equals(Namespaces::UE, ESearchCase::CaseSensitive))
			{
				OutNamespace = Namespaces::UE;
				return true;
			}
			if (Identifier->Name.Equals(Namespaces::Substrate, ESearchCase::CaseSensitive))
			{
				OutNamespace = Namespaces::Substrate;
				return true;
			}
			return false;
		}

		if (const FTypeExpr* TypeExpr = Object.As<FTypeExpr>())
		{
			// `Substrate` is a builtin type spelling, so the parser hands it over as a type and not
			// as an identifier; plan §11 #11 accepts both shapes rather than changing the parser.
			if (TypeExpr->Type.Category == ETypeCategory::Substrate
				|| TypeExpr->Type.Name.Equals(Namespaces::Substrate, ESearchCase::CaseSensitive))
			{
				OutNamespace = Namespaces::Substrate;
				return true;
			}
		}

		return false;
	}

	IR::FIRType FLangBinder::BindMember(const FMemberExpr& Expr)
	{
		if (!Expr.Object)
		{
			return Fail(Expr);
		}

		FString Namespace;
		if (IsNamespaceRoot(*Expr.Object, Namespace))
		{
			Diagnostics.Error(
				TEXT("DSH5211"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("ReflectedNotCalled", "'{0}.{1}' is a node and has to be called: write '{0}.{1}(...)'."),
					FText::FromString(Namespace),
					FText::FromString(Expr.Member)));
			return Fail(Expr);
		}

		const IR::FIRType ObjectType = BindExpr(*Expr.Object);
		if (ObjectType.IsError())
		{
			return Fail(Expr);
		}

		if (ObjectType.IsMaterial())
		{
			const int32 AttributeIndex = Catalog.FindMaterialAttribute(Expr.Member);
			if (AttributeIndex == INDEX_NONE)
			{
				const int32 Close = Catalog.FindMaterialAttributeIgnoreCase(Expr.Member);
				if (Close != INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH5200"),
						CurrentFile,
						Expr.MemberSpan,
						FText::Format(
							LOCTEXT("UnknownAttributeDidYouMean", "A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive."),
							FText::FromString(Expr.Member),
							FText::FromString(Catalog.MaterialAttributes[Close].Name)));
				}
				else
				{
					Diagnostics.Error(
						TEXT("DSH5200"),
						CurrentFile,
						Expr.MemberSpan,
						FText::Format(
							LOCTEXT("UnknownAttribute", "A material has no '{0}' pin."),
							FText::FromString(Expr.Member)));
				}
				return Fail(Expr);
			}

			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::MaterialField;
			Binding.FieldIndex = AttributeIndex;
			Binding.Type = Catalog.MaterialAttributes[AttributeIndex].ValueType;
			// CONTRACT §6.2: a material is a field map, so every attribute of an assignable material
			// is assignable. Reading one that was never written is the IR builder's DSH4370.
			Binding.bLValue = IsLValue(*Expr.Object);
			return Emit(Expr, MoveTemp(Binding));
		}

		if (ObjectType.IsStruct())
		{
			if (!Bound.Structs.IsValidIndex(ObjectType.StructIndex))
			{
				return Fail(Expr);
			}
			const FBoundStruct& Struct = Bound.Structs[ObjectType.StructIndex];
			const int32 FieldIndex = Struct.FindField(Expr.Member);
			if (FieldIndex == INDEX_NONE)
			{
				TArray<FString> Names;
				for (const FBoundStructField& Field : Struct.Fields)
				{
					Names.Add(Field.Name);
				}
				const FString Suggestion = SuggestCaseInsensitive(Expr.Member, Names);

				Diagnostics.Error(
					TEXT("DSH4205"),
					CurrentFile,
					Expr.MemberSpan,
					Suggestion.IsEmpty()
						? FText::Format(
							LOCTEXT("UnknownField", "'{0}' has no field called '{1}'."),
							FText::FromString(Struct.Name),
							FText::FromString(Expr.Member))
						: FText::Format(
							LOCTEXT("UnknownFieldDidYouMean", "'{0}' has no field called '{1}'; did you mean '{2}'?"),
							FText::FromString(Struct.Name),
							FText::FromString(Expr.Member),
							FText::FromString(Suggestion)));
				return Fail(Expr);
			}

			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::StructField;
			Binding.FieldIndex = FieldIndex;
			Binding.Type = Struct.Fields[FieldIndex].Type;
			Binding.bLValue = IsLValue(*Expr.Object);
			return Emit(Expr, MoveTemp(Binding));
		}

		if (ObjectType.IsNode())
		{
			if (!Catalog.Expressions.IsValidIndex(ObjectType.CatalogIndex))
			{
				return Fail(Expr);
			}
			const IR::FCatalogExpression& Class = Catalog.Expressions[ObjectType.CatalogIndex];
			const int32 OutputIndex = Class.FindOutput(Expr.Member);
			if (OutputIndex == INDEX_NONE)
			{
				// A node whose outputs are all channel views of one value (see Convert) answers a swizzle
				// as well as an output name: `UE.VertexColor().a` is its A view, `.rg` reads inside RGB.
				int32 ViewWidth = 0;
				if (IsChannelViewSwizzleSpelling(Expr.Member) && TryGetChannelViewWidth(Class, ViewWidth))
				{
					return BindChannelViewSwizzle(Expr, ObjectType.CatalogIndex, ViewWidth);
				}

				TArray<FString> Names;
				for (const IR::FCatalogPin& Output : Class.Outputs)
				{
					Names.Add(Output.Name);
					Names.Append(Output.Aliases);
				}
				const FString Suggestion = SuggestCaseInsensitive(Expr.Member, Names);
				const FString OutputNames = QuoteNodeOutputNames(Class);

				Diagnostics.Error(
					TEXT("DSH5201"),
					CurrentFile,
					Expr.MemberSpan,
					Suggestion.IsEmpty()
						? (OutputNames.IsEmpty()
							? FText::Format(
								LOCTEXT("UnknownOutput", "'{0}.{1}' has no output called '{2}'."),
								FText::FromString(Class.Namespace),
								FText::FromString(Class.ShortName),
								FText::FromString(Expr.Member))
							: FText::Format(
								LOCTEXT("UnknownOutputListed", "'{0}.{1}' has no output called '{2}'; its outputs are {3}."),
								FText::FromString(Class.Namespace),
								FText::FromString(Class.ShortName),
								FText::FromString(Expr.Member),
								FText::FromString(OutputNames)))
						: FText::Format(
							LOCTEXT("UnknownOutputDidYouMean", "'{0}.{1}' has no output called '{2}'; did you mean '{3}'?"),
							FText::FromString(Class.Namespace),
							FText::FromString(Class.ShortName),
							FText::FromString(Expr.Member),
							FText::FromString(Suggestion)));
				return Fail(Expr);
			}

			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::NodeOutput;
			Binding.FieldIndex = OutputIndex;
			Binding.Index = ObjectType.CatalogIndex;
			Binding.Type = IR::TypeFromCatalogValueType(Class.Outputs[OutputIndex].Type);
			return Emit(Expr, MoveTemp(Binding));
		}

		if (ObjectType.IsNumeric() && ObjectType.Cols == 1)
		{
			return BindSwizzle(Expr, ObjectType);
		}

		if (ObjectType.IsTexture())
		{
			Diagnostics.Error(
				TEXT("DSH4206"),
				CurrentFile,
				Expr.MemberSpan,
				FText::Format(
					LOCTEXT("TextureMethodNotCalled", "'{0}' on a texture is a call: write 'Tex.{0}(UV)'."),
					FText::FromString(Expr.Member)));
			return Fail(Expr);
		}

		Diagnostics.Error(
			TEXT("DSH4207"),
			CurrentFile,
			Expr.MemberSpan,
			FText::Format(
				LOCTEXT("NoSuchMember", "A value of type {0} has no member '{1}'."),
				DescribeType(ObjectType),
				FText::FromString(Expr.Member)));
		return Fail(Expr);
	}

	bool FLangBinder::CanonicaliseSwizzle(const FString& Mask, int32 SourceWidth, const FLangSpan& Span, FString& OutMask)
	{
		OutMask.Reset();

		if (Mask.Len() == 0 || Mask.Len() > 4)
		{
			Diagnostics.Error(
				TEXT("DSH4230"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("SwizzleLength", "'.{0}' is not a swizzle; a swizzle is one to four of 'xyzw' or 'rgba'."),
					FText::FromString(Mask)));
			return false;
		}

		bool bSeenXyzw = false;
		bool bSeenRgba = false;

		for (int32 Index = 0; Index < Mask.Len(); ++Index)
		{
			bool bRgba = false;
			const int32 Component = SwizzleComponent(Mask[Index], bRgba);
			if (Component == INDEX_NONE)
			{
				Diagnostics.Error(
					TEXT("DSH4230"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("SwizzleLetter", "'.{0}' is not a swizzle; '{1}' is not one of 'xyzw' or 'rgba'."),
						FText::FromString(Mask),
						FText::FromString(FString::Chr(Mask[Index]))));
				return false;
			}

			bSeenXyzw = bSeenXyzw || !bRgba;
			bSeenRgba = bSeenRgba || bRgba;

			if (Component >= SourceWidth)
			{
				Diagnostics.Error(
					TEXT("DSH4230"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("SwizzleOutOfRange", "'.{0}' reads component {1} of a value that has {2}."),
						FText::FromString(Mask),
						FText::AsNumber(Component + 1),
						FText::AsNumber(SourceWidth)));
				return false;
			}

			// A repeated component (`v.xxx`) is HLSL replication and binds like any other mask; the
			// builder lowers a mask that is not a plain ascending subset as Swizzle + Append. Only an
			// assignment TARGET refuses a repeat, and BindAssign is where that is said.
			OutMask.AppendChar(GSwizzleLetters[Component]);
		}

		if (bSeenXyzw && bSeenRgba)
		{
			Diagnostics.Error(
				TEXT("DSH4230"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("SwizzleMixedSets", "'.{0}' mixes 'xyzw' with 'rgba'; a swizzle picks one family."),
					FText::FromString(Mask)));
			OutMask.Reset();
			return false;
		}

		return true;
	}

	IR::FIRType FLangBinder::BindSwizzle(const FMemberExpr& Expr, const IR::FIRType& ObjectType)
	{
		FString Mask;
		if (!CanonicaliseSwizzle(Expr.Member, ObjectType.Rows, Expr.MemberSpan, Mask))
		{
			return Fail(Expr);
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Swizzle;
		Binding.Swizzle = Mask;
		Binding.Type = MakeNumeric(ObjectType.Kind, Mask.Len());
		// Readable whatever the mask is; assignable only when each component is named once, because
		// `v.xx = f` would write one component twice with no order between the two writes. BindAssign
		// turns that into DSH4230 with a message about the repeat.
		Binding.bLValue = IsLValue(*Expr.Object) && !HasRepeatedComponent(Mask);

		if (const FBoundExpr* Source = Lookup(*Expr.Object))
		{
			if (Source->bIsConstant)
			{
				Binding.bIsConstant = true;
				for (int32 Index = 0; Index < Mask.Len(); ++Index)
				{
					bool bRgba = false;
					const int32 Component = SwizzleComponent(Mask[Index], bRgba);
					Binding.ConstantValue[Index] = Source->ConstantValue[FMath::Clamp(Component, 0, 3)];
				}
			}
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindChannelViewSwizzle(const FMemberExpr& Expr, int32 CatalogIndex, int32 ViewWidth)
	{
		const IR::FCatalogExpression& Class = Catalog.Expressions[CatalogIndex];

		FString Mask;
		if (!CanonicaliseSwizzle(Expr.Member, ViewWidth, Expr.MemberSpan, Mask))
		{
			return Fail(Expr);
		}

		// Channels one view publishes exactly ARE that view: the pin 1.x's TryRetargetChannelMaskToOutput
		// re-points such a swizzle at, and the one an author would have wired in the editor.
		const int32 ViewOutput = FindChannelViewOutput(Class, Mask);
		if (ViewOutput != INDEX_NONE)
		{
			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::NodeOutput;
			Binding.FieldIndex = ViewOutput;
			Binding.Index = CatalogIndex;
			Binding.Type = IR::TypeFromCatalogValueType(Class.Outputs[ViewOutput].Type);
			return Emit(Expr, MoveTemp(Binding));
		}

		// Anything else is a swizzle of the value, which the IR builder lowers through output 0 -- so it
		// has to stay inside the channels that output carries. Past them the channels sit on different
		// pins, and one swizzle cannot read two outputs.
		const int32 LeadingWidth = GetLeadingChannelViewWidth(Class);
		bool bInsideDefault = LeadingWidth > 0;
		for (int32 Index = 0; bInsideDefault && Index < Mask.Len(); ++Index)
		{
			bool bRgba = false;
			bInsideDefault = SwizzleComponent(Mask[Index], bRgba) < LeadingWidth;
		}
		if (!bInsideDefault)
		{
			Diagnostics.Error(
				TEXT("DSH5201"),
				CurrentFile,
				Expr.MemberSpan,
				FText::Format(
					LOCTEXT("ChannelViewAcrossOutputs", "'{0}.{1}' publishes the channels of '.{2}' on different outputs; read them one output at a time from {3}."),
					FText::FromString(Class.Namespace),
					FText::FromString(Class.ShortName),
					FText::FromString(Expr.Member),
					FText::FromString(QuoteNodeOutputNames(Class))));
			return Fail(Expr);
		}

		// The node stands in for its value here, which is what Convert records for every other use.
		SetConversion(*Expr.Object, IR::EIRConversion::DefaultOutput);

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Swizzle;
		Binding.Swizzle = Mask;
		Binding.Type = MakeNumeric(IR::EIRTypeKind::Float, Mask.Len());
		// A node's output is never assignable, and neither is a swizzle of one.
		Binding.bLValue = false;
		return Emit(Expr, MoveTemp(Binding));
	}

	// ---------------------------------------------------------------------------------------------
	// Indexing
	// ---------------------------------------------------------------------------------------------

	int32 FLangBinder::GetArrayCount(const FExpr& Expr) const
	{
		const FBoundExpr* Binding = Lookup(Expr);
		if (!Binding)
		{
			return 0;
		}

		switch (Binding->Kind)
		{
		case EBoundExprKind::Local:
			return (CurrentFunction && CurrentFunction->Locals.IsValidIndex(Binding->LocalSlot))
				? CurrentFunction->Locals[Binding->LocalSlot].ArrayCount
				: 0;

		case EBoundExprKind::Param:
			return (CurrentFunction && CurrentFunction->Params.IsValidIndex(Binding->Index))
				? CurrentFunction->Params[Binding->Index].ArrayCount
				: 0;

		case EBoundExprKind::Global:
			return GetGlobalArrayCount(Binding->Index);

		case EBoundExprKind::StructField:
		{
			// Which struct the field belongs to is the type of the object it was read from; matching
			// on the field's type alone would pick the wrong struct as soon as two of them agree.
			const FMemberExpr* Member = Expr.As<FMemberExpr>();
			if (!Member || !Member->Object)
			{
				return 0;
			}
			const IR::FIRType ObjectType = TypeOf(*Member->Object);
			if (!ObjectType.IsStruct() || !Bound.Structs.IsValidIndex(ObjectType.StructIndex))
			{
				return 0;
			}
			const FBoundStruct& Struct = Bound.Structs[ObjectType.StructIndex];
			return Struct.Fields.IsValidIndex(Binding->FieldIndex) ? Struct.Fields[Binding->FieldIndex].ArrayCount : 0;
		}

		default:
			return 0;
		}
	}

	bool FLangBinder::GetArrayValues(const FExpr& Expr, const TArray<double>*& OutValues) const
	{
		const FBoundExpr* Binding = Lookup(Expr);
		if (!Binding)
		{
			return false;
		}

		if (Binding->Kind == EBoundExprKind::Global && GlobalArrayValues.IsValidIndex(Binding->Index))
		{
			OutValues = &GlobalArrayValues[Binding->Index];
			return OutValues->Num() > 0;
		}
		if (Binding->Kind == EBoundExprKind::Local && LocalArrayValues.IsValidIndex(Binding->LocalSlot))
		{
			OutValues = &LocalArrayValues[Binding->LocalSlot];
			return OutValues->Num() > 0;
		}
		return false;
	}

	IR::FIRType FLangBinder::BindIndex(const FIndexExpr& Expr)
	{
		if (!Expr.Object || !Expr.Index)
		{
			return Fail(Expr);
		}

		const IR::FIRType ObjectType = BindExpr(*Expr.Object);
		BindExpr(*Expr.Index);
		if (ObjectType.IsError())
		{
			return Fail(Expr);
		}

		double IndexValue[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 IndexComponents = 0;
		const bool bConstantIndex = GetConstant(*Expr.Index, IndexValue, IndexComponents) && IndexComponents == 1;
		const int32 Index = bConstantIndex ? static_cast<int32>(IndexValue[0]) : INDEX_NONE;

		const int32 ArrayCount = GetArrayCount(*Expr.Object);
		if (ArrayCount > 0)
		{
			if (!bConstantIndex)
			{
				Diagnostics.Error(
					TEXT("DSH4242"),
					CurrentFile,
					Expr.Index->Span,
					LOCTEXT("ArrayIndexNotConstant", "An array index must be a compile-time constant: the graph has no arrays, so every element is read at compile time."));
				return Fail(Expr);
			}
			if (Index < 0 || Index >= ArrayCount)
			{
				Diagnostics.Error(
					TEXT("DSH4242"),
					CurrentFile,
					Expr.Index->Span,
					FText::Format(
						LOCTEXT("ArrayIndexOutOfRange", "Element {0} is out of range for an array of {1}."),
						FText::AsNumber(Index),
						FText::AsNumber(ArrayCount)));
				return Fail(Expr);
			}

			const int32 Components = FMath::Max(1, ObjectType.NumComponents());

			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::Literal;
			Binding.Type = ObjectType;

			const TArray<double>* Values = nullptr;
			if (GetArrayValues(*Expr.Object, Values) && Values->Num() >= (Index + 1) * Components)
			{
				Binding.bIsConstant = true;
				for (int32 Component = 0; Component < FMath::Min(Components, 4); ++Component)
				{
					Binding.ConstantValue[Component] = (*Values)[Index * Components + Component];
				}
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH4242"),
					CurrentFile,
					Expr.Span,
					LOCTEXT("ArrayNotFolded", "This array is not a compile-time constant, and the graph has no arrays; declare it 'static const' with a constant initializer."));
				return Fail(Expr);
			}

			return Emit(Expr, MoveTemp(Binding));
		}

		if (ObjectType.IsMatrix())
		{
			// A matrix has no graph value at all (CONTRACT §2), so reading a row is refused here
			// rather than lowered into something that silently is not one.
			Diagnostics.Error(
				TEXT("DSH4244"),
				CurrentFile,
				Expr.Span,
				LOCTEXT("MatrixIndex", "A matrix row cannot be read: the graph has no matrices. Move the code into a '/// @custom' function, where the matrix is an input."));
			return Fail(Expr);
		}

		if (ObjectType.IsNumeric())
		{
			// CONTRACT §6.6: `v[3]` with a constant index is the swizzle `w`.
			if (!bConstantIndex)
			{
				Diagnostics.Error(
					TEXT("DSH4233"),
					CurrentFile,
					Expr.Index->Span,
					LOCTEXT("VectorIndexNotConstant", "A component index must be a compile-time constant; write a swizzle such as '.z', or select with 'lerp'."));
				return Fail(Expr);
			}
			if (Index < 0 || Index >= ObjectType.Rows)
			{
				Diagnostics.Error(
					TEXT("DSH4230"),
					CurrentFile,
					Expr.Index->Span,
					FText::Format(
						LOCTEXT("VectorIndexOutOfRange", "Component {0} is out of range for a value that has {1}."),
						FText::AsNumber(Index),
						FText::AsNumber(ObjectType.Rows)));
				return Fail(Expr);
			}

			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::IndexConst;
			Binding.Swizzle = FString::Chr(GSwizzleLetters[Index]);
			Binding.Type = MakeNumeric(ObjectType.Kind, 1);
			Binding.bLValue = IsLValue(*Expr.Object);
			if (const FBoundExpr* Source = Lookup(*Expr.Object))
			{
				if (Source->bIsConstant)
				{
					Binding.bIsConstant = true;
					Binding.ConstantValue[0] = Source->ConstantValue[Index];
				}
			}
			return Emit(Expr, MoveTemp(Binding));
		}

		Diagnostics.Error(
			TEXT("DSH4234"),
			CurrentFile,
			Expr.Span,
			FText::Format(
				LOCTEXT("CannotIndex", "A value of type {0} cannot be indexed."),
				DescribeType(ObjectType)));
		return Fail(Expr);
	}

	// ---------------------------------------------------------------------------------------------
	// Conversions and core ops
	// ---------------------------------------------------------------------------------------------

	IR::EIRConversion FLangBinder::Convert(const FExpr& Operand, const IR::FIRType& To, EConversionSite Site, const FText& What)
	{
		const IR::FIRType From = TypeOf(Operand);
		if (From.IsError() || To.IsError())
		{
			SetConversion(Operand, IR::EIRConversion::Identity);
			return IR::EIRConversion::Identity;
		}

		// A multi-output node used as a value takes its default output (EIRConversion::DefaultOutput).
		// Only an exact fit is accepted: a node whose default output is the wrong width should say
		// which output it means -- that is what the member access is for.
		//
		// The one wider reading is the 1.x generator's own (TryRetargetChannelMaskToOutput,
		// DreamShaderMaterialGeneratorCodeShared.h): when every output is a channel VIEW of one value,
		// the node IS that value, and a place that wants exactly the whole of it takes it --
		// `float4 VC = UE.VertexColor();`, which is how 1.x, the decompiler and the syntax proposal all
		// write a vertex colour. Only when the default output LEADS that value (VertexColor's RGB):
		// DefaultOutput is output 0 and the IR builder lowers the value through it, so a read inside
		// those channels (`VC.rgb`) is exactly that pin, and a lone leading channel is never broadcast
		// into a vector. A node of different values (SceneTexture's Color / Size / InvSize) has no whole
		// to stand for.
		if (From.IsNode())
		{
			const IR::FCatalogExpression* Class = Catalog.Expressions.IsValidIndex(From.CatalogIndex)
				? &Catalog.Expressions[From.CatalogIndex]
				: nullptr;

			const IR::FIRType Default = ResolveNodeDefault(From);
			bool bFits = !Default.IsNode() && Default == To;

			int32 ViewWidth = 0;
			if (!bFits
				&& Class != nullptr
				&& TryGetChannelViewWidth(*Class, ViewWidth)
				&& GetLeadingChannelViewWidth(*Class) >= 2
				&& To == IR::FIRType::Float(ViewWidth))
			{
				bFits = true;
			}

			if (bFits)
			{
				SetConversion(Operand, IR::EIRConversion::DefaultOutput);
				return IR::EIRConversion::DefaultOutput;
			}

			const FString OutputNames = Class != nullptr ? QuoteNodeOutputNames(*Class) : FString();
			Diagnostics.Error(
				TEXT("DSH5201"),
				CurrentFile,
				Operand.Span,
				OutputNames.IsEmpty()
					? FText::Format(
						LOCTEXT("NodeNeedsOutput", "{0} expects {1}, and this node has more than one output; name the one you mean."),
						What,
						DescribeType(To))
					: FText::Format(
						LOCTEXT("NodeNeedsNamedOutput", "{0} expects {1}, and '{2}' has more than one output; name the one you mean: {3}."),
						What,
						DescribeType(To),
						DescribeType(From),
						FText::FromString(OutputNames)));
			SetConversion(Operand, IR::EIRConversion::Identity);
			return IR::EIRConversion::None;
		}

		IR::EIRConversion Conversion = IR::ClassifyConversion(From, To);

		// A numeric value standing in for a bool is HLSL's truth test. ClassifyConversion does not
		// model it because the graph has no bool: the IR builder reads `Numeric` into a Bool target
		// as "compare against zero", which is exactly what the `!= 0` it emits does.
		if (Conversion == IR::EIRConversion::None && To.IsBool() && From.IsNumeric() && From.Cols == 1 && From.Rows == To.Rows)
		{
			Conversion = IR::EIRConversion::Numeric;
		}

		if (Conversion == IR::EIRConversion::None)
		{
			// One sentence, five codes. The message is written out at each of the five raise sites
			// rather than hoisted into a local, because .skill/gen-diagnostics.ps1 reads a code's
			// message out of the LOCTEXT that follows it (CONTRACT §0.8) and a local would leave all
			// five documented as "(built at runtime)". The key is the same at every site, so the
			// localization gather still sees exactly one entry.
			switch (Site)
			{
			case EConversionSite::Assignment:
				Diagnostics.Error(
					TEXT("DSH4228"),
					CurrentFile,
					Operand.Span,
					FText::Format(
						LOCTEXT("NoConversion", "{0} expects {1}, and this is {2}."),
						What,
						DescribeType(To),
						DescribeType(From)));
				break;
			case EConversionSite::Cast:
				Diagnostics.Error(
					TEXT("DSH4223"),
					CurrentFile,
					Operand.Span,
					FText::Format(
						LOCTEXT("NoConversion", "{0} expects {1}, and this is {2}."),
						What,
						DescribeType(To),
						DescribeType(From)));
				break;
			case EConversionSite::Condition:
				Diagnostics.Error(
					TEXT("DSH4260"),
					CurrentFile,
					Operand.Span,
					FText::Format(
						LOCTEXT("NoConversion", "{0} expects {1}, and this is {2}."),
						What,
						DescribeType(To),
						DescribeType(From)));
				break;
			case EConversionSite::Pin:
				Diagnostics.Error(
					TEXT("DSH5214"),
					CurrentFile,
					Operand.Span,
					FText::Format(
						LOCTEXT("NoConversion", "{0} expects {1}, and this is {2}."),
						What,
						DescribeType(To),
						DescribeType(From)));
				break;
			case EConversionSite::Operand:
			default:
				Diagnostics.Error(
					TEXT("DSH4226"),
					CurrentFile,
					Operand.Span,
					FText::Format(
						LOCTEXT("NoConversion", "{0} expects {1}, and this is {2}."),
						What,
						DescribeType(To),
						DescribeType(From)));
				break;
			}

			SetConversion(Operand, IR::EIRConversion::Identity);
			return IR::EIRConversion::None;
		}

		SetConversion(Operand, Conversion);
		return Conversion;
	}

	FBoundExpr FLangBinder::BuildCoreOp(const IR::FIRCoreOpInfo& Info, const TArray<const FExpr*>& Operands, const FLangSpan& Span)
	{
		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::CoreOp;
		Binding.CoreOp = Info.Op;
		Binding.Type = IR::FIRType::Error();

		const FText Spelling = FText::FromString(Info.Spelling ? Info.Spelling : LexToString(Info.Op));

		// The widest operand decides the width, and the kinds promote the way HLSL promotes them.
		int32 Width = 1;
		bool bAnyError = false;
		bool bAnyMatrix = false;
		IR::FIRType MatrixType;
		IR::EIRTypeKind Kind = IR::EIRTypeKind::Bool;

		for (const FExpr* Operand : Operands)
		{
			// A node standing in for a value is widened to its default output here too, so the width
			// and kind fold sees the real type; Convert() records the DefaultOutput step.
			const IR::FIRType Type = ResolveNodeDefault(Operand ? TypeOf(*Operand) : IR::FIRType::Error());
			if (Type.IsError())
			{
				bAnyError = true;
				continue;
			}
			if (!Type.IsNumeric() && !Type.IsBool())
			{
				Diagnostics.Error(
					TEXT("DSH4226"),
					CurrentFile,
					Operand->Span,
					FText::Format(
						LOCTEXT("OperandNotNumeric", "'{0}' works on numbers, and this is {1}."),
						Spelling,
						DescribeType(Type)));
				bAnyError = true;
				continue;
			}
			if (Type.IsMatrix())
			{
				bAnyMatrix = true;
				MatrixType = Type;
			}
			Width = FMath::Max(Width, Type.Rows);
			Kind = PromoteNumericKind(Kind, Type.Kind);
		}

		if (bAnyError)
		{
			return Binding;
		}

		if (Kind == IR::EIRTypeKind::Bool && Info.Typing != IR::EIRTypingRule::Bool)
		{
			// Every operand was a bool and the op is arithmetic: the graph would make them 0/1
			// floats, and so does the type.
			Kind = IR::EIRTypeKind::Float;
		}

		IR::FIRType Result;
		IR::FIRType Expected;

		switch (Info.Typing)
		{
		case IR::EIRTypingRule::SameAsOperands:
			Result = bAnyMatrix ? MatrixType : MakeNumeric(Kind, Width);
			Expected = Result;
			break;

		case IR::EIRTypingRule::SameAsFirst:
			Result = Operands.Num() > 0 && Operands[0] ? TypeOf(*Operands[0]) : IR::FIRType::Error();
			Expected = Result;
			break;

		case IR::EIRTypingRule::Scalar:
			Result = IR::FIRType::Float(1);
			Expected = MakeNumeric(Kind, Width);
			break;

		case IR::EIRTypingRule::Bool:
			Result = IR::FIRType::Bool(Width);
			// Comparison operands are numbers; the logical operators want truth values, and a number
			// standing in for one is handled in Convert().
			Expected = (Info.Op == IR::EIROp::LogicalAnd || Info.Op == IR::EIROp::LogicalOr || Info.Op == IR::EIROp::LogicalNot)
				? IR::FIRType::Bool(Width)
				: MakeNumeric(Kind, Width);
			break;

		case IR::EIRTypingRule::Float3:
			Result = IR::FIRType::Float(3);
			Expected = Result;
			break;

		case IR::EIRTypingRule::Special:
		default:
			Diagnostics.Error(
				TEXT("DSH4227"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("OpNotCallable", "'{0}' is not an operation this language spells as a call."),
					Spelling));
			return Binding;
		}

		if (Result.IsError())
		{
			return Binding;
		}

		bool bConversionFailed = false;
		for (const FExpr* Operand : Operands)
		{
			if (!Operand)
			{
				continue;
			}
			const FText What = FText::Format(LOCTEXT("OperandOf", "'{0}'"), Spelling);
			if (Convert(*Operand, Expected, EConversionSite::Operand, What) == IR::EIRConversion::None)
			{
				bConversionFailed = true;
			}
		}
		if (bConversionFailed)
		{
			return Binding;
		}

		Binding.Type = Result;

		// Folding: literals, `static const` reads and arithmetic over them.
		bool bAllConstant = Operands.Num() > 0;
		TArray<FConstOperand> Constants;
		Constants.Reserve(Operands.Num());
		for (const FExpr* Operand : Operands)
		{
			FConstOperand Value;
			int32 Components = 1;
			if (!Operand || !GetConstant(*Operand, Value.V, Components))
			{
				bAllConstant = false;
				break;
			}
			Value.N = FMath::Clamp(Components, 1, 4);
			Constants.Add(Value);
		}

		if (bAllConstant && !bAnyMatrix)
		{
			const int32 ResultWidth = FMath::Clamp(Result.NumComponents(), 1, 4);
			double Folded[4] = { 0.0, 0.0, 0.0, 0.0 };
			if (FoldOp(Info.Op, Constants, ResultWidth, Folded))
			{
				Binding.bIsConstant = true;
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Binding.ConstantValue[Index] = Folded[Index];
				}
			}
		}

		return Binding;
	}

	// ---------------------------------------------------------------------------------------------
	// Operators
	// ---------------------------------------------------------------------------------------------

	IR::FIRType FLangBinder::BindUnary(const FUnaryExpr& Expr)
	{
		if (!Expr.Operand)
		{
			return Fail(Expr);
		}

		const IR::FIRType OperandType = BindExpr(*Expr.Operand);

		switch (Expr.Op)
		{
		case EUnaryOp::Plus:
		{
			// `+x` is `x`. Recorded as a transparent node so the IR builder walks straight through.
			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::Paren;
			Binding.Type = OperandType;
			Binding.bLValue = false;
			if (const FBoundExpr* Source = Lookup(*Expr.Operand))
			{
				Binding.bIsConstant = Source->bIsConstant;
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Binding.ConstantValue[Index] = Source->ConstantValue[Index];
				}
			}
			return Emit(Expr, MoveTemp(Binding));
		}

		case EUnaryOp::BitwiseNot:
			Diagnostics.Error(
				TEXT("DSH4227"),
				CurrentFile,
				Expr.Span,
				LOCTEXT("BitwiseNotUnsupported", "'~' has no graph form; the graph has no integers to complement. Move the code into a '/// @custom' function."));
			return Fail(Expr);

		case EUnaryOp::PreIncrement:
		case EUnaryOp::PreDecrement:
		case EUnaryOp::PostIncrement:
		case EUnaryOp::PostDecrement:
		{
			if (!IsLValue(*Expr.Operand))
			{
				Diagnostics.Error(
					TEXT("DSH4235"),
					CurrentFile,
					Expr.Span,
					LOCTEXT("IncrementNeedsLValue", "'++' and '--' write back into what they read, so they need a variable."));
				return Fail(Expr);
			}

			const bool bIncrement = Expr.Op == EUnaryOp::PreIncrement || Expr.Op == EUnaryOp::PostIncrement;

			RecordLocalWrite(Expr, *Expr.Operand);

			// An increment is an assignment: the operand is both the target and the left operand,
			// and the IR builder reads EUnaryOp to know whether the value is the old one or the new.
			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::Assign;
			Binding.CoreOp = bIncrement ? IR::EIROp::Add : IR::EIROp::Subtract;
			Binding.Type = OperandType;
			Binding.bLValue = false;
			return Emit(Expr, MoveTemp(Binding));
		}

		default:
			break;
		}

		const IR::FIRCoreOpInfo* Info = IR::FindCoreOpForUnary(Expr.Op);
		if (!Info)
		{
			Diagnostics.Error(
				TEXT("DSH4227"),
				CurrentFile,
				Expr.Span,
				LOCTEXT("UnaryUnsupported", "This operator has no graph form."));
			return Fail(Expr);
		}

		TArray<const FExpr*> Operands;
		Operands.Add(Expr.Operand.Get());
		FBoundExpr Binding = BuildCoreOp(*Info, Operands, Expr.Span);
		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindBinary(const FBinaryExpr& Expr)
	{
		if (!Expr.Left || !Expr.Right)
		{
			return Fail(Expr);
		}

		BindExpr(*Expr.Left);
		BindExpr(*Expr.Right);

		const IR::FIRCoreOpInfo* Info = IR::FindCoreOpForBinary(Expr.Op);
		if (!Info)
		{
			Diagnostics.Error(
				TEXT("DSH4227"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("BitwiseUnsupported", "'{0}' has no graph form; the graph carries floats, not bit patterns. Move the code into a '/// @custom' function."),
					FText::FromString(BinaryOpSpelling(Expr.Op))));
			return Fail(Expr);
		}

		TArray<const FExpr*> Operands;
		Operands.Add(Expr.Left.Get());
		Operands.Add(Expr.Right.Get());
		FBoundExpr Binding = BuildCoreOp(*Info, Operands, Expr.Span);
		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindAssign(const FAssignExpr& Expr)
	{
		if (!Expr.Target || !Expr.Value)
		{
			return Fail(Expr);
		}

		const IR::FIRType TargetType = BindExpr(*Expr.Target);
		BindExpr(*Expr.Value);

		if (!TargetType.IsError() && !IsLValue(*Expr.Target))
		{
			const FBoundExpr* TargetBinding = Lookup(*Expr.Target);
			const EBoundExprKind TargetKind = TargetBinding ? TargetBinding->Kind : EBoundExprKind::Error;

			if (TargetKind == EBoundExprKind::Global)
			{
				Diagnostics.Error(
					TEXT("DSH4229"),
					CurrentFile,
					Expr.Target->Span,
					LOCTEXT("AssignToGlobal", "A 'uniform' is an input and a 'static const' is a constant; neither can be assigned to. Copy it into a local first."));
			}
			else if (TargetKind == EBoundExprKind::Param)
			{
				Diagnostics.Error(
					TEXT("DSH4236"),
					CurrentFile,
					Expr.Target->Span,
					LOCTEXT("AssignToInParam", "An 'in' parameter is a function input pin and cannot be written to; declare it 'out' or 'inout', or copy it into a local."));
			}
			else if ((TargetKind == EBoundExprKind::Swizzle || TargetKind == EBoundExprKind::IndexConst)
				&& TargetBinding && HasRepeatedComponent(TargetBinding->Swizzle))
			{
				Diagnostics.Error(
					TEXT("DSH4230"),
					CurrentFile,
					Expr.Target->Span,
					FText::Format(
						LOCTEXT("SwizzleRepeatTarget", "'.{0}' names one component twice, so this assignment would write it twice with no order between the two; assign each component on its own line."),
						FText::FromString(TargetBinding->Swizzle)));
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH4229"),
					CurrentFile,
					Expr.Target->Span,
					LOCTEXT("AssignToNonLValue", "The left of '=' has to be a variable, a struct field, a material pin or a swizzle of one."));
			}
			return Fail(Expr);
		}

		EBinaryOp BinaryOp = EBinaryOp::Add;
		IR::EIROp CoreOp = IR::EIROp::Count;
		if (CompoundAssignOp(Expr.Op, BinaryOp))
		{
			const IR::FIRCoreOpInfo* Info = IR::FindCoreOpForBinary(BinaryOp);
			if (!Info)
			{
				Diagnostics.Error(
					TEXT("DSH4227"),
					CurrentFile,
					Expr.Span,
					FText::Format(
						LOCTEXT("BitwiseAssignUnsupported", "'{0}=' has no graph form; the graph carries floats, not bit patterns."),
						FText::FromString(BinaryOpSpelling(BinaryOp))));
				return Fail(Expr);
			}
			CoreOp = Info->Op;
		}

		Convert(*Expr.Value, TargetType, EConversionSite::Assignment, LOCTEXT("AssignmentTarget", "This assignment"));

		RecordLocalWrite(Expr, *Expr.Target);

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Assign;
		Binding.CoreOp = CoreOp;
		Binding.Type = TargetType;
		Binding.bLValue = false;
		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindConditional(const FConditionalExpr& Expr)
	{
		if (!Expr.Condition || !Expr.TrueValue || !Expr.FalseValue)
		{
			return Fail(Expr);
		}

		const IR::FIRType ConditionType = BindExpr(*Expr.Condition);
		const IR::FIRType TrueType = ResolveNodeDefault(BindExpr(*Expr.TrueValue));
		const IR::FIRType FalseType = ResolveNodeDefault(BindExpr(*Expr.FalseValue));

		if (!ConditionType.IsError() && !IsConditionType(ConditionType))
		{
			Diagnostics.Error(
				TEXT("DSH4260"),
				CurrentFile,
				Expr.Condition->Span,
				FText::Format(
					LOCTEXT("ConditionalCondition", "The condition of '?:' has to be a single true-or-false value, and this is {0}."),
					DescribeType(ConditionType)));
		}
		else
		{
			Convert(*Expr.Condition, IR::FIRType::Bool(1), EConversionSite::Condition, LOCTEXT("ConditionalConditionWhat", "The condition of '?:'"));
		}

		if (TrueType.IsError() || FalseType.IsError())
		{
			return Fail(Expr);
		}

		IR::FIRType Result;
		if (TrueType == FalseType)
		{
			Result = TrueType;
		}
		else if ((TrueType.IsNumeric() || TrueType.IsBool()) && (FalseType.IsNumeric() || FalseType.IsBool()))
		{
			Result = MakeNumeric(
				PromoteNumericKind(TrueType.Kind, FalseType.Kind),
				FMath::Max(TrueType.Rows, FalseType.Rows));
		}
		else
		{
			Diagnostics.Error(
				TEXT("DSH4226"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("ConditionalBranches", "The two halves of '?:' are {0} and {1}; they have to make one value."),
					DescribeType(TrueType),
					DescribeType(FalseType)));
			return Fail(Expr);
		}

		Convert(*Expr.TrueValue, Result, EConversionSite::Operand, LOCTEXT("ConditionalTrue", "The 'then' half of '?:'"));
		Convert(*Expr.FalseValue, Result, EConversionSite::Operand, LOCTEXT("ConditionalFalse", "The 'else' half of '?:'"));

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Conditional;
		Binding.Type = Result;

		double ConditionValue[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 ConditionComponents = 0;
		if (GetConstant(*Expr.Condition, ConditionValue, ConditionComponents))
		{
			const FExpr& Chosen = (ConditionValue[0] != 0.0) ? *Expr.TrueValue : *Expr.FalseValue;
			if (const FBoundExpr* Source = Lookup(Chosen))
			{
				if (Source->bIsConstant)
				{
					Binding.bIsConstant = true;
					for (int32 Index = 0; Index < 4; ++Index)
					{
						Binding.ConstantValue[Index] = Source->ConstantValue[Index];
					}
				}
			}
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindCast(const FCastExpr& Expr)
	{
		if (!Expr.Operand)
		{
			return Fail(Expr);
		}

		const IR::FIRType SourceType = BindExpr(*Expr.Operand);

		IR::FIRType Target;
		if (!ResolveTypeRef(Expr.Type, Target))
		{
			return Fail(Expr);
		}

		if (SourceType.IsError())
		{
			return Fail(Expr);
		}

		if (!Target.IsNumeric() && !Target.IsBool())
		{
			Diagnostics.Error(
				TEXT("DSH4223"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("CastToNonNumeric", "A cast to {0} has no meaning here; only numbers and bools can be cast."),
					DescribeType(Target)));
			return Fail(Expr);
		}

		if (!SourceType.IsNumeric() && !SourceType.IsBool())
		{
			Diagnostics.Error(
				TEXT("DSH4223"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("CastFromNonNumeric", "A value of type {0} cannot be cast to {1}."),
					DescribeType(SourceType),
					DescribeType(Target)));
			return Fail(Expr);
		}

		if (SourceType.Rows > 1 && Target.Rows < SourceType.Rows)
		{
			// A narrowing cast would need a mask node the cast does not spell; the swizzle does, and
			// says which components were meant.
			Diagnostics.Error(
				TEXT("DSH4223"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("CastNarrows", "A cast from {0} to {1} drops components; write the swizzle that says which, such as '.xyz'."),
					DescribeType(SourceType),
					DescribeType(Target)));
			return Fail(Expr);
		}

		Convert(*Expr.Operand, Target, EConversionSite::Cast, LOCTEXT("CastOperand", "This cast"));

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Cast;
		Binding.Type = Target;

		double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 Components = 0;
		if (GetConstant(*Expr.Operand, Value, Components))
		{
			Binding.bIsConstant = true;
			const int32 Width = FMath::Clamp(Target.NumComponents(), 1, 4);
			for (int32 Index = 0; Index < Width; ++Index)
			{
				const double Source = Value[(Components == 1) ? 0 : FMath::Min(Index, Components - 1)];
				Binding.ConstantValue[Index] = Target.IsIntegral()
					? FMath::TruncToDouble(Source)
					: (Target.IsBool() ? ((Source != 0.0) ? 1.0 : 0.0) : Source);
			}
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	// ---------------------------------------------------------------------------------------------
	// Initializer lists
	// ---------------------------------------------------------------------------------------------

	IR::FIRType FLangBinder::BindInitializerList(const FInitializerListExpr& Expr, const IR::FIRType* Expected)
	{
		if (!Expected || Expected->IsError())
		{
			for (const FExprPtr& Element : Expr.Elements)
			{
				if (Element)
				{
					BindExpr(*Element);
				}
			}
			Diagnostics.Error(
				TEXT("DSH4237"),
				CurrentFile,
				Expr.Span,
				LOCTEXT("InitializerListNoTarget", "An initializer list only has a meaning against a declared type; it cannot stand on its own."));
			return Fail(Expr);
		}

		if (Expected->IsStruct())
		{
			if (!Bound.Structs.IsValidIndex(Expected->StructIndex))
			{
				return Fail(Expr);
			}
			const FBoundStruct& Struct = Bound.Structs[Expected->StructIndex];

			FBoundExpr Binding;
			Binding.Kind = EBoundExprKind::StructConstructor;
			Binding.Index = Expected->StructIndex;
			Binding.Type = *Expected;

			if (Expr.Elements.Num() != Struct.Fields.Num())
			{
				Diagnostics.Error(
					TEXT("DSH4222"),
					CurrentFile,
					Expr.Span,
					FText::Format(
						LOCTEXT("StructInitializerCount", "'{0}' has {1} fields and this list has {2}."),
						FText::FromString(Struct.Name),
						FText::AsNumber(Struct.Fields.Num()),
						FText::AsNumber(Expr.Elements.Num())));
			}

			for (int32 Index = 0; Index < Expr.Elements.Num(); ++Index)
			{
				const FExpr* Element = Expr.Elements[Index].Get();
				if (!Element)
				{
					continue;
				}
				const IR::FIRType* FieldType = Struct.Fields.IsValidIndex(Index) ? &Struct.Fields[Index].Type : nullptr;
				BindExpr(*Element, FieldType);

				FBoundArgument Argument;
				Argument.ArgumentIndex = Index;
				Argument.TargetIndex = Index;
				if (FieldType)
				{
					Argument.Target = Struct.Fields[Index].Name;
					Argument.Conversion = Convert(*Element, *FieldType, EConversionSite::Operand, LOCTEXT("StructField2", "This struct field"));
				}
				Binding.Args.Add(MoveTemp(Argument));
			}

			return Emit(Expr, MoveTemp(Binding));
		}

		if (!Expected->IsNumeric() && !Expected->IsBool())
		{
			for (const FExprPtr& Element : Expr.Elements)
			{
				if (Element)
				{
					BindExpr(*Element);
				}
			}
			Diagnostics.Error(
				TEXT("DSH4237"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("InitializerListBadTarget", "A value of type {0} cannot be written as an initializer list."),
					DescribeType(*Expected)));
			return Fail(Expr);
		}

		const IR::FIRType Element = MakeNumeric(Expected->Kind, 1);

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::InitializerList;
		Binding.Type = *Expected;
		Binding.bIsConstant = Expr.Elements.Num() > 0;

		int32 Total = 0;
		int32 Written = 0;
		for (int32 Index = 0; Index < Expr.Elements.Num(); ++Index)
		{
			const FExpr* Item = Expr.Elements[Index].Get();
			if (!Item)
			{
				continue;
			}

			const IR::FIRType ItemType = BindExpr(*Item, &Element);
			const int32 Components = FMath::Max(1, ItemType.NumComponents());
			Total += Components;

			FBoundArgument Argument;
			Argument.ArgumentIndex = Index;
			Argument.TargetIndex = Written;
			Argument.Conversion = Convert(*Item, MakeNumeric(Expected->Kind, Components), EConversionSite::Operand, LOCTEXT("InitializerElement", "This initializer element"));
			Binding.Args.Add(MoveTemp(Argument));

			double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 ValueComponents = 0;
			if (GetConstant(*Item, Value, ValueComponents))
			{
				for (int32 Component = 0; Component < Components && Written < 4; ++Component, ++Written)
				{
					Binding.ConstantValue[Written] = Value[FMath::Min(Component, ValueComponents - 1)];
				}
			}
			else
			{
				Binding.bIsConstant = false;
				Written += Components;
			}
		}

		const int32 Want = Expected->NumComponents();
		if (Want > 4)
		{
			// Same reason as the constructor above: four components is all a folded value holds.
			Binding.bIsConstant = false;
		}
		if (Total != Want)
		{
			Diagnostics.Error(
				TEXT("DSH4222"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("InitializerListCount", "{0} needs {1} components and this list supplies {2}."),
					DescribeType(*Expected),
					FText::AsNumber(Want),
					FText::AsNumber(Total)));
			Binding.bIsConstant = false;
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindArrayInitializer(const FExpr& Init, const IR::FIRType& ElementType, int32 Count, TArray<double>& OutValues)
	{
		OutValues.Reset();

		const FInitializerListExpr* List = Init.As<FInitializerListExpr>();
		if (!List)
		{
			BindExpr(Init, &ElementType);
			Diagnostics.Error(
				TEXT("DSH4237"),
				CurrentFile,
				Init.Span,
				LOCTEXT("ArrayNeedsList", "An array is initialised with a list, as in '= { 1.0, 2.0 }'."));
			return IR::FIRType::Error();
		}

		if (List->Elements.Num() != Count)
		{
			Diagnostics.Error(
				TEXT("DSH4222"),
				CurrentFile,
				Init.Span,
				FText::Format(
					LOCTEXT("ArrayInitializerCount", "This array has {0} elements and its initializer has {1}."),
					FText::AsNumber(Count),
					FText::AsNumber(List->Elements.Num())));
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::InitializerList;
		Binding.Type = ElementType;

		const int32 Components = FMath::Clamp(ElementType.NumComponents(), 1, 4);
		bool bAllConstant = List->Elements.Num() == Count;

		for (int32 Index = 0; Index < List->Elements.Num(); ++Index)
		{
			const FExpr* Element = List->Elements[Index].Get();
			if (!Element)
			{
				bAllConstant = false;
				continue;
			}

			BindExpr(*Element, &ElementType);

			FBoundArgument Argument;
			Argument.ArgumentIndex = Index;
			Argument.TargetIndex = Index;
			Argument.Conversion = Convert(*Element, ElementType, EConversionSite::Operand, LOCTEXT("ArrayElement", "This array element"));
			Binding.Args.Add(MoveTemp(Argument));

			double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 ValueComponents = 0;
			if (bAllConstant && GetConstant(*Element, Value, ValueComponents))
			{
				for (int32 Component = 0; Component < Components; ++Component)
				{
					OutValues.Add(Value[FMath::Min(Component, FMath::Max(ValueComponents - 1, 0))]);
				}
			}
			else
			{
				bAllConstant = false;
			}
		}

		if (!bAllConstant)
		{
			OutValues.Reset();
		}

		Binding.bIsConstant = bAllConstant;
		Emit(Init, MoveTemp(Binding));
		return ElementType;
	}

	// ---------------------------------------------------------------------------------------------
	// Calls
	// ---------------------------------------------------------------------------------------------

	IR::FIRType FLangBinder::BindCall(const FCallExpr& Expr, const IR::FIRType* Expected, bool bStatement)
	{
		const FExpr* Callee = Expr.Callee.Get();
		if (!Callee)
		{
			return Fail(Expr);
		}

		// A constructor: `float3(...)`, and `ToonInputs(...)` once the type resolves to a struct.
		if (const FTypeExpr* TypeCallee = Callee->As<FTypeExpr>())
		{
			return BindConstructor(Expr, TypeCallee->Type);
		}

		if (const FMemberExpr* Member = Callee->As<FMemberExpr>())
		{
			FString Namespace;
			if (Member->Object && IsNamespaceRoot(*Member->Object, Namespace))
			{
				return BindReflectedCall(Expr, Namespace, Member->Member, Member->MemberSpan, Expected, bStatement);
			}

			if (!Member->Object)
			{
				return Fail(Expr);
			}

			const IR::FIRType ObjectType = BindExpr(*Member->Object);
			if (ObjectType.IsError())
			{
				for (const FArgument& Argument : Expr.Arguments)
				{
					if (Argument.Value)
					{
						BindExpr(*Argument.Value);
					}
				}
				return Fail(Expr);
			}

			if (ObjectType.IsTexture())
			{
				return BindTextureSampleMethod(Expr, *Member, ObjectType);
			}

			Diagnostics.Error(
				TEXT("DSH4208"),
				CurrentFile,
				Member->MemberSpan,
				FText::Format(
					LOCTEXT("NoSuchMethod", "A value of type {0} has no method '{1}'."),
					DescribeType(ObjectType),
					FText::FromString(Member->Member)));
			return Fail(Expr);
		}

		if (const FIdentifierExpr* Identifier = Callee->As<FIdentifierExpr>())
		{
			const FString& Name = Identifier->Name;

			// CONTRACT §6.5: the free-function spellings of a texture sample.
			if (Name.Equals(TEXT("Texture2DSample"), ESearchCase::CaseSensitive))
			{
				return BindTextureSampleFunction(Expr, false);
			}
			if (Name.Equals(TEXT("Texture2DSampleLevel"), ESearchCase::CaseSensitive))
			{
				return BindTextureSampleFunction(Expr, true);
			}

			if (const IR::FIRCoreOpInfo* Info = IR::FindCoreOpByHlslName(Name))
			{
				return BindCoreOpCall(Expr, *Info);
			}

			if (const IR::FIRCoreOpInfo* Alias = IR::FindCoreOpByGlslAlias(Name))
			{
				// 1.x rewrote `mix` to `lerp` behind the author's back; 2.0 names the HLSL spelling
				// and refuses, because a silent rewrite is exactly the class of mistake this front
				// end exists to stop.
				Diagnostics.Error(
					TEXT("DSH4250"),
					CurrentFile,
					Identifier->Span,
					FText::Format(
						LOCTEXT("GlslAlias", "'{0}' is the GLSL spelling; this language is HLSL, so write '{1}'."),
						FText::FromString(Name),
						FText::FromString(Alias->HlslName ? Alias->HlslName : TEXT(""))));
				for (const FArgument& Argument : Expr.Arguments)
				{
					if (Argument.Value)
					{
						BindExpr(*Argument.Value);
					}
				}
				return Fail(Expr);
			}

			const int32 FunctionIndex = FindFunction(Name);
			if (FunctionIndex != INDEX_NONE)
			{
				return BindUserFunctionCall(Expr, FunctionIndex, bStatement);
			}

			const int32 StructIndex = FindStruct(Name);
			if (StructIndex != INDEX_NONE)
			{
				return BindStructConstructor(Expr, StructIndex);
			}

			for (const FArgument& Argument : Expr.Arguments)
			{
				if (Argument.Value)
				{
					BindExpr(*Argument.Value);
				}
			}

			TArray<FString> Candidates;
			for (const FBoundFunction& Function : Bound.Functions)
			{
				Candidates.Add(Function.Name);
			}
			for (const FBoundStruct& Struct : Bound.Structs)
			{
				Candidates.Add(Struct.Name);
			}
			const FString Suggestion = SuggestCaseInsensitive(Name, Candidates);

			Diagnostics.Error(
				TEXT("DSH4208"),
				CurrentFile,
				Identifier->Span,
				Suggestion.IsEmpty()
					? FText::Format(
						LOCTEXT("UnknownCallee", "'{0}' is not a function, a builtin or a struct."),
						FText::FromString(Name))
					: FText::Format(
						LOCTEXT("UnknownCalleeDidYouMean", "'{0}' is not declared; did you mean '{1}'? Names are case-sensitive."),
						FText::FromString(Name),
						FText::FromString(Suggestion)));
			return Fail(Expr);
		}

		Diagnostics.Error(
			TEXT("DSH4208"),
			CurrentFile,
			Expr.Span,
			LOCTEXT("CalleeNotCallable", "This is not something that can be called; a call names a function, a builtin, a type or 'UE.'/'Substrate.' followed by a node."));
		return Fail(Expr);
	}

	IR::FIRType FLangBinder::BindConstructor(const FCallExpr& Expr, const FTypeRef& TypeRef)
	{
		IR::FIRType Target;
		if (!ResolveTypeRef(TypeRef, Target))
		{
			for (const FArgument& Argument : Expr.Arguments)
			{
				if (Argument.Value)
				{
					BindExpr(*Argument.Value);
				}
			}
			return Fail(Expr);
		}

		if (Target.IsStruct())
		{
			return BindStructConstructor(Expr, Target.StructIndex);
		}

		if (!Target.IsNumeric() && !Target.IsBool())
		{
			for (const FArgument& Argument : Expr.Arguments)
			{
				if (Argument.Value)
				{
					BindExpr(*Argument.Value);
				}
			}
			Diagnostics.Error(
				TEXT("DSH4223"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("NotConstructible", "A value of type {0} cannot be constructed; it comes from a declaration or a node."),
					DescribeType(Target)));
			return Fail(Expr);
		}

		if (Expr.HasNamedArguments())
		{
			Diagnostics.Error(
				TEXT("DSH4225"),
				CurrentFile,
				Expr.Span,
				LOCTEXT("ConstructorNamedArguments", "A constructor takes its components in order; named arguments belong on 'UE.' nodes and on function calls."));
		}

		if (Expr.Arguments.Num() == 0)
		{
			Diagnostics.Error(
				TEXT("DSH4221"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("ConstructorNoArguments", "{0}() has no components; write the value, as in 'float3(0.0)'."),
					DescribeType(Target)));
			return Fail(Expr);
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Constructor;
		Binding.Type = Target;

		const int32 Want = Target.NumComponents();
		int32 Total = 0;
		bool bAnyError = false;

		for (int32 Index = 0; Index < Expr.Arguments.Num(); ++Index)
		{
			const FExpr* Value = Expr.Arguments[Index].Value.Get();
			if (!Value)
			{
				bAnyError = true;
				continue;
			}
			const IR::FIRType ValueType = BindExpr(*Value);
			if (ValueType.IsError())
			{
				bAnyError = true;
				continue;
			}
			if (!ValueType.IsNumeric() && !ValueType.IsBool())
			{
				Diagnostics.Error(
					TEXT("DSH4226"),
					CurrentFile,
					Value->Span,
					FText::Format(
						LOCTEXT("ConstructorArgumentType", "A constructor takes numbers, and this is {0}."),
						DescribeType(ValueType)));
				bAnyError = true;
				continue;
			}
			if (ValueType.IsMatrix())
			{
				Diagnostics.Error(
					TEXT("DSH4226"),
					CurrentFile,
					Value->Span,
					LOCTEXT("ConstructorMatrixArgument", "A matrix cannot be a constructor component."));
				bAnyError = true;
				continue;
			}
			Total += FMath::Max(1, ValueType.Rows);
		}

		if (bAnyError)
		{
			return Fail(Expr);
		}

		// Either the one-scalar broadcast form, or the components add up exactly.
		const bool bBroadcast = Expr.Arguments.Num() == 1 && Total == 1 && Want > 1;
		if (!bBroadcast && Total != Want)
		{
			Diagnostics.Error(
				TEXT("DSH4222"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("ConstructorCount", "{0} needs {1} components and these arguments supply {2}."),
					DescribeType(Target),
					FText::AsNumber(Want),
					FText::AsNumber(Total)));
			return Fail(Expr);
		}

		int32 Written = 0;
		bool bAllConstant = true;
		for (int32 Index = 0; Index < Expr.Arguments.Num(); ++Index)
		{
			const FExpr* Value = Expr.Arguments[Index].Value.Get();
			if (!Value)
			{
				continue;
			}
			const IR::FIRType ValueType = TypeOf(*Value);
			const int32 Components = bBroadcast ? Want : FMath::Max(1, ValueType.Rows);

			FBoundArgument Argument;
			Argument.ArgumentIndex = Index;
			Argument.TargetIndex = Written;
			// The kind changes; the width does not, because the pieces are laid side by side.
			Argument.Conversion = Convert(
				*Value,
				MakeNumeric(Target.Kind, bBroadcast ? 1 : Components),
				EConversionSite::Operand,
				LOCTEXT("ConstructorComponent", "This constructor component"));
			Binding.Args.Add(MoveTemp(Argument));

			double Value4[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 ValueComponents = 0;
			if (bAllConstant && GetConstant(*Value, Value4, ValueComponents) && ValueComponents > 0)
			{
				for (int32 Component = 0; Component < Components && Written < 4; ++Component, ++Written)
				{
					Binding.ConstantValue[Written] = Value4[bBroadcast ? 0 : FMath::Min(Component, ValueComponents - 1)];
				}
			}
			else
			{
				bAllConstant = false;
				Written += Components;
			}
		}

		// FBoundExpr::ConstantValue holds four components, so a matrix constructor is never folded;
		// a half-folded matrix would be worse than an unfolded one.
		Binding.bIsConstant = bAllConstant && Want <= 4;
		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindStructConstructor(const FCallExpr& Expr, int32 StructIndex)
	{
		if (!Bound.Structs.IsValidIndex(StructIndex))
		{
			return Fail(Expr);
		}
		const FBoundStruct& Struct = Bound.Structs[StructIndex];

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::StructConstructor;
		Binding.Index = StructIndex;
		Binding.Type = IR::FIRType::Struct(StructIndex);

		if (Expr.Arguments.Num() != Struct.Fields.Num())
		{
			Diagnostics.Error(
				TEXT("DSH4222"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("StructConstructorCount", "'{0}' has {1} fields and this call supplies {2}."),
					FText::FromString(Struct.Name),
					FText::AsNumber(Struct.Fields.Num()),
					FText::AsNumber(Expr.Arguments.Num())));
		}

		for (int32 Index = 0; Index < Expr.Arguments.Num(); ++Index)
		{
			const FExpr* Value = Expr.Arguments[Index].Value.Get();
			if (!Value)
			{
				continue;
			}

			const IR::FIRType* FieldType = Struct.Fields.IsValidIndex(Index) ? &Struct.Fields[Index].Type : nullptr;
			BindExpr(*Value, FieldType);

			FBoundArgument Argument;
			Argument.ArgumentIndex = Index;
			Argument.TargetIndex = Index;
			if (FieldType)
			{
				Argument.Target = Struct.Fields[Index].Name;
				Argument.Conversion = Convert(*Value, *FieldType, EConversionSite::Operand, LOCTEXT("StructFieldArgument", "This struct field"));
			}
			Binding.Args.Add(MoveTemp(Argument));
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindCoreOpCall(const FCallExpr& Expr, const IR::FIRCoreOpInfo& Info)
	{
		const FString Name = FString(Info.HlslName ? Info.HlslName : TEXT(""));

		// CONTRACT §6.13 #23: the ops exist in the core table, but the material graph has no
		// hyperbolic node and the emitter will not synthesise one out of Exp. Refused here, at the
		// call, rather than three stages later where the message would be about a missing class.
		if (Info.Op == IR::EIROp::Sinh || Info.Op == IR::EIROp::Cosh || Info.Op == IR::EIROp::Tanh)
		{
			for (const FArgument& Argument : Expr.Arguments)
			{
				if (Argument.Value)
				{
					BindExpr(*Argument.Value);
				}
			}
			Diagnostics.Error(
				TEXT("DSH4246"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("HyperbolicUnsupported", "The material graph has no hyperbolic node, so '{0}' cannot be lowered; write it in a '/// @custom' body, where the shader compiler has it."),
					FText::FromString(Name)));
			return Fail(Expr);
		}

		// Operands sit in the op's own pin order. A positional argument takes the next free slot; a
		// named one names a pin of the op (`lerp(0.0, 1.0, Alpha = 0.5)`), which is how the 1.x
		// spelling of a builtin call survives into 2.0.
		int32 PinCount = 0;
		while (PinCount < 6 && Info.InputPins[PinCount] != nullptr)
		{
			++PinCount;
		}

		TArray<const FExpr*> Operands;
		TArray<int32> ArgumentOfSlot;
		Operands.Init(nullptr, FMath::Max3(PinCount, Info.MaxArity, Expr.Arguments.Num()));
		ArgumentOfSlot.Init(INDEX_NONE, Operands.Num());

		int32 NextSlot = 0;
		bool bAnyError = false;

		for (int32 Index = 0; Index < Expr.Arguments.Num(); ++Index)
		{
			const FArgument& Argument = Expr.Arguments[Index];
			if (!Argument.Value)
			{
				bAnyError = true;
				continue;
			}
			BindExpr(*Argument.Value);

			int32 Slot = INDEX_NONE;
			if (Argument.Name.IsEmpty())
			{
				Slot = NextSlot++;
			}
			else
			{
				for (int32 Pin = 0; Pin < PinCount; ++Pin)
				{
					if (Argument.Name.Equals(Info.InputPins[Pin], ESearchCase::CaseSensitive))
					{
						Slot = Pin;
						break;
					}
				}
				if (Slot == INDEX_NONE)
				{
					FString Pins;
					for (int32 Pin = 0; Pin < PinCount; ++Pin)
					{
						Pins += Pins.IsEmpty() ? TEXT("") : TEXT(", ");
						Pins += Info.InputPins[Pin];
					}
					Diagnostics.Error(
						TEXT("DSH4216"),
						CurrentFile,
						Argument.NameSpan,
						Pins.IsEmpty()
							? FText::Format(
								LOCTEXT("CoreOpNoPins", "'{0}' takes its arguments in order and has no argument called '{1}'."),
								FText::FromString(Name),
								FText::FromString(Argument.Name))
							: FText::Format(
								LOCTEXT("CoreOpNoSuchPin", "'{0}' has no argument called '{1}'; its arguments are {2}."),
								FText::FromString(Name),
								FText::FromString(Argument.Name),
								FText::FromString(Pins)));
					bAnyError = true;
					continue;
				}
			}

			if (!Operands.IsValidIndex(Slot))
			{
				Diagnostics.Error(
					TEXT("DSH4224"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("CoreOpTooManyArguments", "'{0}' takes at most {1} arguments."),
						FText::FromString(Name),
						FText::AsNumber(Info.MaxArity)));
				bAnyError = true;
				continue;
			}

			if (Operands[Slot] != nullptr)
			{
				Diagnostics.Error(
					TEXT("DSH4215"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("CoreOpArgumentTwice", "The '{0}' argument of '{1}' is given twice."),
						FText::FromString(Info.InputPins[FMath::Min(Slot, PinCount > 0 ? PinCount - 1 : 0)] ? Info.InputPins[FMath::Min(Slot, PinCount > 0 ? PinCount - 1 : 0)] : TEXT("")),
						FText::FromString(Name)));
				bAnyError = true;
				continue;
			}

			Operands[Slot] = Argument.Value.Get();
			ArgumentOfSlot[Slot] = Index;
		}

		// Trim the trailing empty slots so the operand count is the arity the author actually wrote.
		while (Operands.Num() > 0 && Operands.Last() == nullptr)
		{
			Operands.Pop();
			ArgumentOfSlot.Pop();
		}

		int32 Supplied = 0;
		for (int32 Index = 0; Index < Operands.Num(); ++Index)
		{
			if (Operands[Index] == nullptr)
			{
				Diagnostics.Error(
					TEXT("DSH4217"),
					CurrentFile,
					Expr.Span,
					FText::Format(
						LOCTEXT("CoreOpMissingArgument", "'{0}' is missing its '{1}' argument."),
						FText::FromString(Name),
						FText::FromString((Index < PinCount && Info.InputPins[Index]) ? Info.InputPins[Index] : TEXT("?"))));
				bAnyError = true;
			}
			else
			{
				++Supplied;
			}
		}

		if (Supplied < Info.MinArity || Supplied > Info.MaxArity)
		{
			Diagnostics.Error(
				TEXT("DSH4224"),
				CurrentFile,
				Expr.Span,
				Info.MinArity == Info.MaxArity
					? FText::Format(
						LOCTEXT("CoreOpArityExact", "'{0}' takes {1} arguments and {2} were given."),
						FText::FromString(Name),
						FText::AsNumber(Info.MinArity),
						FText::AsNumber(Supplied))
					: FText::Format(
						LOCTEXT("CoreOpArityRange", "'{0}' takes between {1} and {2} arguments, and {3} were given."),
						FText::FromString(Name),
						FText::AsNumber(Info.MinArity),
						FText::AsNumber(Info.MaxArity),
						FText::AsNumber(Supplied)));
			bAnyError = true;
		}

		if (bAnyError)
		{
			return Fail(Expr);
		}

		FBoundExpr Binding = BuildCoreOp(Info, Operands, Expr.Span);

		// A core op built from call syntax always carries Args, so the IR builder never has to guess
		// whether the AST argument order is the operand order -- once a name is used, it is not.
		for (int32 Slot = 0; Slot < Operands.Num(); ++Slot)
		{
			FBoundArgument Argument;
			Argument.ArgumentIndex = ArgumentOfSlot[Slot];
			Argument.TargetIndex = Slot;
			if (Slot < PinCount && Info.InputPins[Slot])
			{
				Argument.Target = Info.InputPins[Slot];
			}
			if (Operands[Slot])
			{
				if (const FBoundExpr* Operand = Lookup(*Operands[Slot]))
				{
					Argument.Conversion = Operand->Conversion;
				}
			}
			Binding.Args.Add(MoveTemp(Argument));
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindUserFunctionCall(const FCallExpr& Expr, int32 FunctionIndex, bool bStatement)
	{
		const FBoundFunction& Function = Bound.Functions[FunctionIndex];

		if (Function.Kind == EBoundFunctionKind::Entry
			|| Function.Kind == EBoundFunctionKind::Layer
			|| Function.Kind == EBoundFunctionKind::LayerBlend)
		{
			for (const FArgument& Argument : Expr.Arguments)
			{
				if (Argument.Value)
				{
					BindExpr(*Argument.Value);
				}
			}
			Diagnostics.Error(
				TEXT("DSH6208"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("CallToProduct", "'{0}' is a {1} asset, not a function this file may call."),
					FText::FromString(Function.Name),
					FText::FromString(LexToString(Function.Kind))));
			return Fail(Expr);
		}

		// Recorded whatever happens next: the call graph is what DetectRecursion() closes over, and
		// a call that fails to type is still a call.
		CurrentCallees.Add(FunctionIndex);

		TArray<const FExpr*> Matched;
		TArray<int32> ArgumentOfParam;
		Matched.Init(nullptr, Function.Params.Num());
		ArgumentOfParam.Init(INDEX_NONE, Function.Params.Num());

		int32 NextParam = 0;
		bool bAnyError = false;

		for (int32 Index = 0; Index < Expr.Arguments.Num(); ++Index)
		{
			const FArgument& Argument = Expr.Arguments[Index];
			if (!Argument.Value)
			{
				bAnyError = true;
				continue;
			}

			int32 ParamIndex = INDEX_NONE;
			if (Argument.Name.IsEmpty())
			{
				ParamIndex = NextParam++;
				if (!Function.Params.IsValidIndex(ParamIndex))
				{
					BindExpr(*Argument.Value);
					Diagnostics.Error(
						TEXT("DSH4224"),
						CurrentFile,
						Argument.Span,
						FText::Format(
							LOCTEXT("TooManyArguments", "'{0}' takes {1} arguments and more were given."),
							FText::FromString(Function.Name),
							FText::AsNumber(Function.Params.Num())));
					bAnyError = true;
					continue;
				}
			}
			else
			{
				for (int32 Param = 0; Param < Function.Params.Num(); ++Param)
				{
					if (Function.Params[Param].Name.Equals(Argument.Name, ESearchCase::CaseSensitive))
					{
						ParamIndex = Param;
						break;
					}
				}
				if (ParamIndex == INDEX_NONE)
				{
					BindExpr(*Argument.Value);

					TArray<FString> Names;
					for (const FBoundParam& Param : Function.Params)
					{
						Names.Add(Param.Name);
					}
					const FString Suggestion = SuggestCaseInsensitive(Argument.Name, Names);

					Diagnostics.Error(
						TEXT("DSH4216"),
						CurrentFile,
						Argument.NameSpan,
						Suggestion.IsEmpty()
							? FText::Format(
								LOCTEXT("NoSuchParameter", "'{0}' has no parameter called '{1}'."),
								FText::FromString(Function.Name),
								FText::FromString(Argument.Name))
							: FText::Format(
								LOCTEXT("NoSuchParameterDidYouMean", "'{0}' has no parameter called '{1}'; did you mean '{2}'?"),
								FText::FromString(Function.Name),
								FText::FromString(Argument.Name),
								FText::FromString(Suggestion)));
					bAnyError = true;
					continue;
				}
			}

			if (Matched[ParamIndex] != nullptr)
			{
				BindExpr(*Argument.Value);
				Diagnostics.Error(
					TEXT("DSH4215"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("ArgumentTwice", "'{0}' is given twice in this call to '{1}'."),
						FText::FromString(Function.Params[ParamIndex].Name),
						FText::FromString(Function.Name)));
				bAnyError = true;
				continue;
			}

			BindExpr(*Argument.Value, &Function.Params[ParamIndex].Type);
			Matched[ParamIndex] = Argument.Value.Get();
			ArgumentOfParam[ParamIndex] = Index;
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::FunctionCall;
		Binding.Index = FunctionIndex;
		Binding.Type = Function.ReturnType;

		for (int32 ParamIndex = 0; ParamIndex < Function.Params.Num(); ++ParamIndex)
		{
			const FBoundParam& Param = Function.Params[ParamIndex];
			const FExpr* Value = Matched[ParamIndex];

			if (!Value)
			{
				if (!Param.bOptional)
				{
					Diagnostics.Error(
						TEXT("DSH4217"),
						CurrentFile,
						Expr.Span,
						FText::Format(
							LOCTEXT("MissingArgument", "'{0}' needs an argument for '{1}'."),
							FText::FromString(Function.Name),
							FText::FromString(Param.Name)));
					bAnyError = true;
				}
				continue;
			}

			FBoundArgument Argument;
			Argument.ArgumentIndex = ArgumentOfParam[ParamIndex];
			Argument.TargetIndex = ParamIndex;
			Argument.Target = Param.Name;

			if (Param.Direction == EParamDirection::In)
			{
				Argument.Conversion = Convert(
					*Value,
					Param.Type,
					EConversionSite::Operand,
					FText::Format(
						LOCTEXT("ArgumentOf", "The '{0}' argument of '{1}'"),
						FText::FromString(Param.Name),
						FText::FromString(Function.Name)));
			}
			else if (!IsLValue(*Value))
			{
				// An `out` / `inout` argument is written back into, so it has to be a variable.
				Diagnostics.Error(
					TEXT("DSH4239"),
					CurrentFile,
					Value->Span,
					FText::Format(
						LOCTEXT("OutArgumentNotLValue", "'{0}' is an out parameter of '{1}', so its argument has to be a variable."),
						FText::FromString(Param.Name),
						FText::FromString(Function.Name)));
				bAnyError = true;
			}
			else
			{
				// The call writes back into this variable, which is a write ProveTripCount has to
				// see: `for (int i = ...) Advance(i);` with an `inout` parameter moves the induction
				// variable, and an unrolled count taken from the step alone would be wrong.
				RecordLocalWrite(Expr, *Value);

				// ...and exactly the parameter's type: a conversion has nowhere to live on the way back.
				const IR::FIRType ValueType = TypeOf(*Value);
				if (!ValueType.IsError() && !Param.Type.IsError() && ValueType != Param.Type)
				{
					Diagnostics.Error(
						TEXT("DSH4218"),
						CurrentFile,
						Value->Span,
						FText::Format(
							LOCTEXT("OutArgumentType", "'{0}' writes back {1}, and this variable is {2}; an out argument has to match exactly."),
							FText::FromString(Param.Name),
							DescribeType(Param.Type),
							DescribeType(ValueType)));
					bAnyError = true;
				}
			}

			Binding.Args.Add(MoveTemp(Argument));
		}

		if (bAnyError)
		{
			return Fail(Expr);
		}

		if (Function.ReturnType.IsVoid() && !bStatement)
		{
			Diagnostics.Error(
				TEXT("DSH4238"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("VoidCallAsValue", "'{0}' returns nothing, so its call has no value; its results come back through its out parameters."),
					FText::FromString(Function.Name)));
			return Fail(Expr);
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	// ---------------------------------------------------------------------------------------------
	// Reflected calls (CONTRACT §6.10)
	// ---------------------------------------------------------------------------------------------

	bool FLangBinder::BindPropertyArgument(const FArgument& Argument, const IR::FCatalogProperty& Property, const FString& ClassName)
	{
		const FExpr* Value = Argument.Value.Get();
		if (!Value)
		{
			return false;
		}

		// A property value that is a SPELLING -- an enumerator, a name, an asset path -- is recorded
		// as a Literal typed Error. There is no FIRType for a word; the IR builder reads the text
		// straight out of the AST, and the Error type stops anything else from trying to use it as
		// a value. This is the one place an Error type is not a reported mistake.
		auto RecordSpelling = [this, Value]()
		{
			if (!Lookup(*Value))
			{
				FBoundExpr Binding;
				Binding.Kind = EBoundExprKind::Literal;
				Binding.Type = IR::FIRType::Error();
				Emit(*Value, MoveTemp(Binding));
			}
		};

		switch (Property.Type)
		{
		case IR::ECatalogValueType::Enum:
		{
			FString Spelling;
			if (!FlattenClassSpecifier(*Value, Spelling))
			{
				Diagnostics.Error(
					TEXT("DSH5224"),
					CurrentFile,
					Value->Span,
					FText::Format(
						LOCTEXT("EnumNotSpelling", "'{0}' is an enumerated property; write one of its values, as in 'SamplerType = Normal'."),
						FText::FromString(Property.Name)));
				RecordSpelling();
				return false;
			}

			if (Property.EnumValues.Num() > 0)
			{
				bool bFound = false;
				for (const FString& Enumerator : Property.EnumValues)
				{
					if (Enumerator.Equals(Spelling, ESearchCase::CaseSensitive))
					{
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					const FString Suggestion = SuggestCaseInsensitive(Spelling, Property.EnumValues);
					Diagnostics.Error(
						TEXT("DSH5215"),
						CurrentFile,
						Value->Span,
						Suggestion.IsEmpty()
							? FText::Format(
								LOCTEXT("UnknownEnumerator", "'{0}' is not a value of '{1}' on '{2}'."),
								FText::FromString(Spelling),
								FText::FromString(Property.Name),
								FText::FromString(ClassName))
							: FText::Format(
								LOCTEXT("UnknownEnumeratorDidYouMean", "'{0}' is not a value of '{1}'; did you mean '{2}'?"),
								FText::FromString(Spelling),
								FText::FromString(Property.Name),
								FText::FromString(Suggestion)));
					RecordSpelling();
					return false;
				}
			}

			RecordSpelling();
			return true;
		}

		case IR::ECatalogValueType::String:
		case IR::ECatalogValueType::Name:
		case IR::ECatalogValueType::Object:
		{
			FString Spelling;
			if (!FlattenClassSpecifier(*Value, Spelling))
			{
				Diagnostics.Error(
					TEXT("DSH5224"),
					CurrentFile,
					Value->Span,
					FText::Format(
						LOCTEXT("PropertyNotSpelling", "'{0}' takes a name or an asset path; write it as a quoted string."),
						FText::FromString(Property.Name)));
				RecordSpelling();
				return false;
			}
			RecordSpelling();
			return true;
		}

		case IR::ECatalogValueType::Unknown:
		{
			// A property the catalog could not classify. A bare word that names nothing in scope is
			// taken at face value rather than reported as an undeclared variable, which is what the
			// author meant by writing it.
			const FIdentifierExpr* Identifier = Value->As<FIdentifierExpr>();
			const bool bIsWord =
				(Identifier != nullptr
					&& FindLocal(Identifier->Name) == INDEX_NONE
					&& FindParam(Identifier->Name) == INDEX_NONE
					&& FindGlobal(Identifier->Name) == INDEX_NONE)
				|| Value->Is<FMemberExpr>()
				|| (Value->Is<FLiteralExpr>() && static_cast<const FLiteralExpr*>(Value)->LiteralKind == ELiteralKind::String);
			if (bIsWord)
			{
				RecordSpelling();
				return true;
			}
			break;
		}

		default:
			break;
		}

		BindExpr(*Value);
		if (!IsConstantExpr(*Value))
		{
			Diagnostics.Error(
				TEXT("DSH5224"),
				CurrentFile,
				Value->Span,
				FText::Format(
					LOCTEXT("PropertyNotConstant", "'{0}' is written into the node itself, not connected to it, so its value has to be known at compile time."),
					FText::FromString(Property.Name)));
			return false;
		}
		return true;
	}

	IR::FIRType FLangBinder::BindReflectedCall(
		const FCallExpr& Expr,
		const FString& Namespace,
		const FString& Name,
		const FLangSpan& NameSpan,
		const IR::FIRType* Expected,
		bool bStatement)
	{
		const FArgument* ClassArgument = nullptr;
		for (const FArgument& Argument : Expr.Arguments)
		{
			if (Argument.Name.Equals(TEXT("Class"), ESearchCase::CaseSensitive))
			{
				ClassArgument = &Argument;
				break;
			}
		}

		int32 CatalogIndex = INDEX_NONE;
		FString ClassSpecifier;

		if (ClassArgument)
		{
			if (Namespace.Equals(Namespaces::Substrate, ESearchCase::CaseSensitive))
			{
				Diagnostics.Error(
					TEXT("DSH5216"),
					CurrentFile,
					ClassArgument->Span,
					LOCTEXT("SubstrateClass", "'Substrate.' already names the node, so it takes no 'Class' argument."));
			}
			else if (!ClassArgument->Value || !FlattenClassSpecifier(*ClassArgument->Value, ClassSpecifier))
			{
				Diagnostics.Error(
					TEXT("DSH5217"),
					CurrentFile,
					ClassArgument->Span,
					LOCTEXT("ClassNotLiteral", "'Class' takes the expression class as a quoted string."));
			}
			else
			{
				CatalogIndex = Catalog.FindExpressionByClass(ClassSpecifier);
				if (CatalogIndex == INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH5212"),
						CurrentFile,
						ClassArgument->Span,
						FText::Format(
							LOCTEXT("UnknownClass", "'{0}' is not a material expression class this engine has."),
							FText::FromString(ClassSpecifier)));
				}
			}
		}
		else if (Name.Equals(ExpressionEscapeHatch, ESearchCase::CaseSensitive)
			&& Namespace.Equals(Namespaces::UE, ESearchCase::CaseSensitive))
		{
			Diagnostics.Error(
				TEXT("DSH5218"),
				CurrentFile,
				NameSpan,
				LOCTEXT("ExpressionNeedsClass", "'UE.Expression' reaches a node this language has no name for, so it needs 'Class = \"MaterialExpressionName\"'."));
		}
		else
		{
			CatalogIndex = Catalog.FindExpression(Namespace, Name);
			if (CatalogIndex == INDEX_NONE)
			{
				const int32 Close = Catalog.FindExpressionIgnoreCase(Namespace, Name);
				if (Close != INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH5210"),
						CurrentFile,
						NameSpan,
						FText::Format(
							LOCTEXT("UnknownReflectedDidYouMean", "'{0}.{1}' is not a node; did you mean '{0}.{2}'? Node names are case-sensitive."),
							FText::FromString(Namespace),
							FText::FromString(Name),
							FText::FromString(Catalog.Expressions[Close].ShortName)));
				}
				else
				{
					Diagnostics.Error(
						TEXT("DSH5210"),
						CurrentFile,
						NameSpan,
						FText::Format(
							LOCTEXT("UnknownReflected", "'{0}.{1}' is not a node this engine has; 'UE.Expression(Class = \"...\")' reaches one the language has no name for."),
							FText::FromString(Namespace),
							FText::FromString(Name)));
				}
			}
		}

		if (CatalogIndex == INDEX_NONE)
		{
			for (const FArgument& Argument : Expr.Arguments)
			{
				if (Argument.Value && &Argument != ClassArgument && !Lookup(*Argument.Value))
				{
					BindExpr(*Argument.Value);
				}
			}
			return Fail(Expr);
		}

		const IR::FCatalogExpression& Class = Catalog.Expressions[CatalogIndex];

		if (Class.bIsAbstract)
		{
			Diagnostics.Error(
				TEXT("DSH5223"),
				CurrentFile,
				NameSpan,
				FText::Format(
					LOCTEXT("AbstractClass", "'{0}' is abstract and cannot be made into a node."),
					FText::FromString(Class.ClassName)));
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::ReflectedCall;
		Binding.Index = CatalogIndex;

		TArray<bool> InputBound;
		TArray<bool> PropertyBound;
		InputBound.Init(false, Class.Inputs.Num());
		// The widest value arriving on a pin that does not fix its width: a Numeric output follows it.
		decltype(IR::FIRType::Rows) WidestAnyWidthArgument = 1;
		PropertyBound.Init(false, Class.Properties.Num());

		int32 PositionalIndex = 0;
		bool bAnyError = false;

		for (int32 Index = 0; Index < Expr.Arguments.Num(); ++Index)
		{
			const FArgument& Argument = Expr.Arguments[Index];

			if (&Argument == ClassArgument)
			{
				if (Argument.Value && !Lookup(*Argument.Value))
				{
					FBoundExpr Spelling;
					Spelling.Kind = EBoundExprKind::Literal;
					Spelling.Type = IR::FIRType::Error();
					Emit(*Argument.Value, MoveTemp(Spelling));
				}

				// Recorded so the IR builder can write Prop::ClassSpecifier without going back to the
				// AST; TargetIndex stays INDEX_NONE because `Class` is not a property of the node.
				FBoundArgument Selector;
				Selector.ArgumentIndex = Index;
				Selector.Target = TEXT("Class");
				Selector.bIsProperty = true;
				Binding.Args.Add(MoveTemp(Selector));
				continue;
			}

			FString Target = Argument.Name;
			if (Target.IsEmpty())
			{
				// Only the classes the catalog gives a canonical order take positional arguments;
				// everything else is named-only, as all of them were in 1.x.
				if (Class.PositionalParameters.Num() == 0)
				{
					if (Argument.Value)
					{
						BindExpr(*Argument.Value);
					}
					Diagnostics.Error(
						TEXT("DSH5220"),
						CurrentFile,
						Argument.Span,
						FText::Format(
							LOCTEXT("NamedOnly", "'{0}.{1}' takes named arguments: write 'Pin = value'."),
							FText::FromString(Class.Namespace),
							FText::FromString(Class.ShortName)));
					bAnyError = true;
					continue;
				}
				if (PositionalIndex >= Class.PositionalParameters.Num())
				{
					if (Argument.Value)
					{
						BindExpr(*Argument.Value);
					}
					Diagnostics.Error(
						TEXT("DSH5221"),
						CurrentFile,
						Argument.Span,
						FText::Format(
							LOCTEXT("TooManyPositional", "'{0}.{1}' takes {2} arguments in order; name the rest."),
							FText::FromString(Class.Namespace),
							FText::FromString(Class.ShortName),
							FText::AsNumber(Class.PositionalParameters.Num())));
					bAnyError = true;
					continue;
				}
				Target = Class.PositionalParameters[PositionalIndex++];
			}

			// An input pin first, then a reflected literal property.
			const int32 InputIndex = Class.FindInput(Target);
			if (InputIndex != INDEX_NONE)
			{
				if (InputBound[InputIndex])
				{
					Diagnostics.Error(
						TEXT("DSH4215"),
						CurrentFile,
						Argument.Span,
						FText::Format(
							LOCTEXT("PinTwice", "'{0}' is connected twice in this call."),
							FText::FromString(Target)));
					bAnyError = true;
					continue;
				}
				InputBound[InputIndex] = true;

				const IR::FCatalogPin& Pin = Class.Inputs[InputIndex];
				bool bAnyWidth = false;
				const IR::FIRType PinType = IR::TypeFromCatalogValueType(Pin.Type, &bAnyWidth);

				if (Argument.Value)
				{
					const IR::FIRType ValueType = BindExpr(*Argument.Value, bAnyWidth ? nullptr : &PinType);
					if (bAnyWidth && ValueType.IsNumeric() && ValueType.Cols == 1 && ValueType.Rows > WidestAnyWidthArgument)
					{
						WidestAnyWidthArgument = ValueType.Rows;
					}

					FBoundArgument Bound_;
					Bound_.ArgumentIndex = Index;
					Bound_.Target = Pin.Name;
					Bound_.TargetIndex = InputIndex;

					if (bAnyWidth && ValueType.IsNumeric() && ValueType.Cols == 1)
					{
						// The pin does not constrain the width, so whatever arrives is what it gets.
						Bound_.Conversion = IR::EIRConversion::Identity;
					}
					else
					{
						Bound_.Conversion = Convert(
							*Argument.Value,
							PinType,
							EConversionSite::Pin,
							FText::Format(
								LOCTEXT("PinOf", "The '{0}' pin of '{1}.{2}'"),
								FText::FromString(Pin.Name),
								FText::FromString(Class.Namespace),
								FText::FromString(Class.ShortName)));
						if (Bound_.Conversion == IR::EIRConversion::None)
						{
							bAnyError = true;
						}
					}

					Binding.Args.Add(MoveTemp(Bound_));
				}
				continue;
			}

			const int32 PropertyIndex = Class.FindProperty(Target);
			if (PropertyIndex != INDEX_NONE)
			{
				if (PropertyBound[PropertyIndex])
				{
					Diagnostics.Error(
						TEXT("DSH4215"),
						CurrentFile,
						Argument.Span,
						FText::Format(
							LOCTEXT("PropertyTwice", "'{0}' is set twice in this call."),
							FText::FromString(Target)));
					bAnyError = true;
					continue;
				}
				PropertyBound[PropertyIndex] = true;

				const FString ClassLabel = Class.Namespace + TEXT(".") + Class.ShortName;
				if (!BindPropertyArgument(Argument, Class.Properties[PropertyIndex], ClassLabel))
				{
					bAnyError = true;
				}

				FBoundArgument Bound_;
				Bound_.ArgumentIndex = Index;
				Bound_.Target = Class.Properties[PropertyIndex].Name;
				Bound_.bIsProperty = true;
				Bound_.TargetIndex = PropertyIndex;
				Binding.Args.Add(MoveTemp(Bound_));
				continue;
			}

			if (Argument.Value)
			{
				BindExpr(*Argument.Value);
			}

			// The 1.x spellings are candidates too, so a near miss on an alias still gets named.
			TArray<FString> Names;
			for (const IR::FCatalogPin& Pin : Class.Inputs)
			{
				Names.Add(Pin.Name);
				Names.Append(Pin.Aliases);
			}
			for (const IR::FCatalogProperty& Property : Class.Properties)
			{
				Names.Add(Property.Name);
				Names.Append(Property.Aliases);
			}
			const FString Suggestion = SuggestCaseInsensitive(Target, Names);

			Diagnostics.Error(
				TEXT("DSH5213"),
				CurrentFile,
				Argument.Name.IsEmpty() ? Argument.Span : Argument.NameSpan,
				Suggestion.IsEmpty()
					? FText::Format(
						LOCTEXT("NoSuchPinOrProperty", "'{0}.{1}' has no pin or property called '{2}'."),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName),
						FText::FromString(Target))
					: FText::Format(
						LOCTEXT("NoSuchPinOrPropertyDidYouMean", "'{0}.{1}' has no pin or property called '{2}'; did you mean '{3}'?"),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName),
						FText::FromString(Target),
						FText::FromString(Suggestion)));
			bAnyError = true;
		}

		for (int32 InputIndex = 0; InputIndex < Class.Inputs.Num(); ++InputIndex)
		{
			const IR::FCatalogPin& Pin = Class.Inputs[InputIndex];
			if (!Pin.bRequired || InputBound[InputIndex])
			{
				continue;
			}

			// A required pin is satisfied by its literal twin too: `ConstA = 1` fills the slot the
			// `A` pin would.
			bool bSatisfiedByConst = false;
			if (!Pin.ConstPropertyName.IsEmpty())
			{
				const int32 ConstIndex = Class.FindProperty(Pin.ConstPropertyName);
				bSatisfiedByConst = ConstIndex != INDEX_NONE && PropertyBound.IsValidIndex(ConstIndex) && PropertyBound[ConstIndex];
			}
			if (bSatisfiedByConst)
			{
				continue;
			}

			Diagnostics.Error(
				TEXT("DSH5219"),
				CurrentFile,
				Expr.Span,
				Pin.ConstPropertyName.IsEmpty()
					? FText::Format(
						LOCTEXT("RequiredPin", "'{0}.{1}' needs its '{2}' pin connected."),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName),
						FText::FromString(Pin.Name))
					: FText::Format(
						LOCTEXT("RequiredPinOrConst", "'{0}.{1}' needs its '{2}' pin connected, or '{3}' set to a literal."),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName),
						FText::FromString(Pin.Name),
						FText::FromString(Pin.ConstPropertyName)));
			bAnyError = true;
		}

		// A custom-output class is a statement, and a class with no output is one too: there is
		// nothing to read back either way.
		const bool bValueless = Class.bIsCustomOutput || Class.Outputs.Num() == 0;
		if (bValueless)
		{
			if (!bStatement)
			{
				Diagnostics.Error(
					TEXT("DSH4231"),
					CurrentFile,
					Expr.Span,
					FText::Format(
						LOCTEXT("CustomOutputAsValue", "'{0}.{1}' is an output node, not a value: write it as a statement on a line of its own."),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName)));
				bAnyError = true;
			}
			Binding.Type = IR::FIRType::Void();
		}
		else if (Class.Outputs.Num() > 1)
		{
			Binding.Type = IR::FIRType::Node(CatalogIndex);
		}
		else
		{
			bool bAnyWidth = false;
			IR::FIRType OutputType = IR::TypeFromCatalogValueType(Class.Outputs[0].Type, &bAnyWidth);
			if (bAnyWidth && WidestAnyWidthArgument > 1)
			{
				// A Numeric output follows its inputs (CONTRACT #43): lerp(float3, 0, a) is a float3 whatever
				// it is written into, and a use that wants another width converts from there.
				OutputType = MakeNumeric(IR::EIRTypeKind::Float, WidestAnyWidthArgument);
			}
			else if (bAnyWidth && Expected && Expected->IsNumeric() && Expected->Cols == 1)
			{
				// No input is wider than a scalar, so the declared type of whatever it feeds is the best
				// answer there is. A catalog that spells the width wins over this.
				OutputType = MakeNumeric(IR::EIRTypeKind::Float, Expected->Rows);
			}
			Binding.Type = OutputType;
		}

		if (bAnyError)
		{
			return Fail(Expr);
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	// ---------------------------------------------------------------------------------------------
	// Texture sampling (CONTRACT §6.5)
	// ---------------------------------------------------------------------------------------------

	namespace
	{
		/** The width of the coordinate a texture of this kind is addressed with. */
		int32 TextureCoordinateWidth(ETextureKind Kind)
		{
			switch (Kind)
			{
			case ETextureKind::TextureCube:
			case ETextureKind::Texture2DArray:
			case ETextureKind::Texture3D:
			case ETextureKind::VolumeTexture:
				return 3;
			case ETextureKind::Texture2D:
			default:
				return 2;
			}
		}
	}

	IR::FIRType FLangBinder::BindTextureSampleMethod(const FCallExpr& Expr, const FMemberExpr& Callee, const IR::FIRType& TextureType)
	{
		const bool bLevel = Callee.Member.Equals(TEXT("SampleLevel"), ESearchCase::CaseSensitive);
		if (!bLevel && !Callee.Member.Equals(TEXT("Sample"), ESearchCase::CaseSensitive))
		{
			for (const FArgument& Argument : Expr.Arguments)
			{
				if (Argument.Value)
				{
					BindExpr(*Argument.Value);
				}
			}
			Diagnostics.Error(
				TEXT("DSH4208"),
				CurrentFile,
				Callee.MemberSpan,
				FText::Format(
					LOCTEXT("NoSuchTextureMethod", "A texture has no '{0}' method; it has 'Sample' and 'SampleLevel'."),
					FText::FromString(Callee.Member)));
			return Fail(Expr);
		}

		if (Expr.HasNamedArguments())
		{
			Diagnostics.Error(
				TEXT("DSH4225"),
				CurrentFile,
				Expr.Span,
				LOCTEXT("SampleNamedArguments", "A texture sample takes its arguments in order: an optional sampler, the coordinates, and for 'SampleLevel' the mip level."));
		}

		TArray<const FExpr*> Values;
		for (const FArgument& Argument : Expr.Arguments)
		{
			if (!Argument.Value)
			{
				return Fail(Expr);
			}
			BindExpr(*Argument.Value);
			Values.Add(Argument.Value.Get());
		}

		// Whether the sampler was written is decided by the TYPE of the first argument rather than by
		// counting, so `Tex.Sample(UV)` and `Tex.Sample(S, UV)` both land where they should and a
		// wrong argument gets a message about its type instead of about the arity. A texture there is
		// the 1.x spelling of the same slot -- `Tex.Sample(Tex, UV)`, a texture standing in for its own
		// sampler (CONTRACT 6.13 #51) -- and counts only when the arguments leave room for a sampler,
		// so a texture passed where the coordinates belong still gets the type error.
		const int32 WithSampler = 2 + (bLevel ? 1 : 0);
		const bool bHasSampler = Values.Num() > 0
			&& (TypeOf(*Values[0]).Kind == IR::EIRTypeKind::SamplerState
				|| (TypeOf(*Values[0]).Kind == IR::EIRTypeKind::Texture && Values.Num() == WithSampler));
		const int32 Wanted = (bHasSampler ? 1 : 0) + 1 + (bLevel ? 1 : 0);

		if (Values.Num() != Wanted)
		{
			Diagnostics.Error(
				TEXT("DSH4224"),
				CurrentFile,
				Expr.Span,
				bLevel
					? LOCTEXT("SampleLevelArity", "'SampleLevel' is written 'Tex.SampleLevel(UV, Level)' or 'Tex.SampleLevel(Sampler, UV, Level)'.")
					: LOCTEXT("SampleArity", "'Sample' is written 'Tex.Sample(UV)' or 'Tex.Sample(Sampler, UV)'."));
			return Fail(Expr);
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::TextureSample;
		Binding.Type = IR::FIRType::Float(4);

		// The texture is the OBJECT of the member access and not an argument, so its ArgumentIndex
		// stays INDEX_NONE and the IR builder reads FMemberExpr::Object for it.
		FBoundArgument Texture;
		Texture.Target = TEXT("Texture");
		Binding.Args.Add(MoveTemp(Texture));

		int32 Cursor = 0;
		if (bHasSampler)
		{
			FBoundArgument Sampler;
			Sampler.ArgumentIndex = Cursor;
			Sampler.Target = TEXT("Sampler");
			Binding.Args.Add(MoveTemp(Sampler));
			++Cursor;
		}

		const IR::FIRType UVType = IR::FIRType::Float(TextureCoordinateWidth(TextureType.Texture));
		FBoundArgument UV;
		UV.ArgumentIndex = Cursor;
		UV.Target = TEXT("UV");
		UV.Conversion = Convert(*Values[Cursor], UVType, EConversionSite::Pin, LOCTEXT("SampleUV", "The coordinates of a texture sample"));
		Binding.Args.Add(MoveTemp(UV));
		++Cursor;

		if (bLevel)
		{
			FBoundArgument Level;
			Level.ArgumentIndex = Cursor;
			Level.Target = TEXT("Level");
			Level.Conversion = Convert(*Values[Cursor], IR::FIRType::Float(1), EConversionSite::Pin, LOCTEXT("SampleLevelArgument", "The mip level of a texture sample"));
			Binding.Args.Add(MoveTemp(Level));
		}

		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindTextureSampleFunction(const FCallExpr& Expr, bool bHasLevel)
	{
		const int32 Wanted = bHasLevel ? 4 : 3;

		TArray<const FExpr*> Values;
		for (const FArgument& Argument : Expr.Arguments)
		{
			if (!Argument.Value)
			{
				return Fail(Expr);
			}
			BindExpr(*Argument.Value);
			Values.Add(Argument.Value.Get());
		}

		if (Expr.HasNamedArguments() || Values.Num() != Wanted)
		{
			Diagnostics.Error(
				TEXT("DSH4224"),
				CurrentFile,
				Expr.Span,
				bHasLevel
					? LOCTEXT("Texture2DSampleLevelArity", "'Texture2DSampleLevel' is written 'Texture2DSampleLevel(Tex, TexSampler, UV, Level)'.")
					: LOCTEXT("Texture2DSampleArity", "'Texture2DSample' is written 'Texture2DSample(Tex, TexSampler, UV)'."));
			return Fail(Expr);
		}

		const IR::FIRType TextureType = TypeOf(*Values[0]);
		if (!TextureType.IsError() && !TextureType.IsTexture())
		{
			Diagnostics.Error(
				TEXT("DSH5230"),
				CurrentFile,
				Values[0]->Span,
				FText::Format(
					LOCTEXT("SampleNotTexture", "The first argument of a texture sample is the texture, and this is {0}."),
					DescribeType(TextureType)));
			return Fail(Expr);
		}

		// The sampler argument is the `<Name>Sampler` half of the pair the shader translator makes
		// for a texture input. A texture in that slot is accepted: it is how a `.dsh` header shared
		// with fxc spells the pairing, and the graph takes the sampler from the texture anyway.
		const IR::FIRType SamplerType = TypeOf(*Values[1]);
		if (!SamplerType.IsError() && SamplerType.Kind != IR::EIRTypeKind::SamplerState && !SamplerType.IsTexture())
		{
			Diagnostics.Error(
				TEXT("DSH5231"),
				CurrentFile,
				Values[1]->Span,
				FText::Format(
					LOCTEXT("SampleNotSampler", "The second argument of 'Texture2DSample' is the sampler, and this is {0}."),
					DescribeType(SamplerType)));
			return Fail(Expr);
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::TextureSample;
		Binding.Type = IR::FIRType::Float(4);

		FBoundArgument Texture;
		Texture.ArgumentIndex = 0;
		Texture.Target = TEXT("Texture");
		Binding.Args.Add(MoveTemp(Texture));

		FBoundArgument Sampler;
		Sampler.ArgumentIndex = 1;
		Sampler.Target = TEXT("Sampler");
		Binding.Args.Add(MoveTemp(Sampler));

		const IR::FIRType UVType = IR::FIRType::Float(TextureCoordinateWidth(TextureType.Texture));
		FBoundArgument UV;
		UV.ArgumentIndex = 2;
		UV.Target = TEXT("UV");
		UV.Conversion = Convert(*Values[2], UVType, EConversionSite::Pin, LOCTEXT("SampleUV2", "The coordinates of a texture sample"));
		Binding.Args.Add(MoveTemp(UV));

		if (bHasLevel)
		{
			FBoundArgument Level;
			Level.ArgumentIndex = 3;
			Level.Target = TEXT("Level");
			Level.Conversion = Convert(*Values[3], IR::FIRType::Float(1), EConversionSite::Pin, LOCTEXT("SampleLevelArgument2", "The mip level of a texture sample"));
			Binding.Args.Add(MoveTemp(Level));
		}

		return Emit(Expr, MoveTemp(Binding));
	}
}

#undef LOCTEXT_NAMESPACE
