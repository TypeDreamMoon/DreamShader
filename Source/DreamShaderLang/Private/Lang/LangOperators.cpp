// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The one operator table. The parser reads it to decide what binds tighter; the printer reads the
// same numbers to decide where a parenthesis is required. Nothing here reports a diagnostic, so
// this translation unit owns no DSH code and needs no LOCTEXT namespace.

#include "LangOperators.h"

#include "Lang/LangAst.h"
#include "Lang/LangToken.h"

#include "Misc/AssertionMacros.h"

namespace UE::DreamShader::Lang::Private
{
	int32 GetBinaryPrecedence(EBinaryOp Op)
	{
		switch (Op)
		{
		case EBinaryOp::Multiply:
		case EBinaryOp::Divide:
		case EBinaryOp::Modulo:
			return 10;

		case EBinaryOp::Add:
		case EBinaryOp::Subtract:
			return 9;

		case EBinaryOp::ShiftLeft:
		case EBinaryOp::ShiftRight:
			return 8;

		case EBinaryOp::Less:
		case EBinaryOp::LessEqual:
		case EBinaryOp::Greater:
		case EBinaryOp::GreaterEqual:
			return 7;

		case EBinaryOp::Equal:
		case EBinaryOp::NotEqual:
			return 6;

		case EBinaryOp::BitwiseAnd:
			return 5;

		case EBinaryOp::BitwiseXor:
			return 4;

		case EBinaryOp::BitwiseOr:
			return 3;

		case EBinaryOp::LogicalAnd:
			return 2;

		case EBinaryOp::LogicalOr:
			return 1;

		default:
			break;
		}

		// Every enumerator is handled above; a new one must be given a rung before it is used.
		checkNoEntry();
		return 1;
	}

	bool TryGetBinaryOp(ELangTokenKind Kind, EBinaryOp& OutOp)
	{
		switch (Kind)
		{
		case ELangTokenKind::Star:               OutOp = EBinaryOp::Multiply;     return true;
		case ELangTokenKind::Slash:              OutOp = EBinaryOp::Divide;       return true;
		case ELangTokenKind::Percent:            OutOp = EBinaryOp::Modulo;       return true;
		case ELangTokenKind::Plus:               OutOp = EBinaryOp::Add;          return true;
		case ELangTokenKind::Minus:              OutOp = EBinaryOp::Subtract;     return true;
		case ELangTokenKind::LessLess:           OutOp = EBinaryOp::ShiftLeft;    return true;
		case ELangTokenKind::GreaterGreater:     OutOp = EBinaryOp::ShiftRight;   return true;
		case ELangTokenKind::Less:               OutOp = EBinaryOp::Less;         return true;
		case ELangTokenKind::LessEqual:          OutOp = EBinaryOp::LessEqual;    return true;
		case ELangTokenKind::Greater:            OutOp = EBinaryOp::Greater;      return true;
		case ELangTokenKind::GreaterEqual:       OutOp = EBinaryOp::GreaterEqual; return true;
		case ELangTokenKind::EqualEqual:         OutOp = EBinaryOp::Equal;        return true;
		case ELangTokenKind::BangEqual:          OutOp = EBinaryOp::NotEqual;     return true;
		case ELangTokenKind::Ampersand:          OutOp = EBinaryOp::BitwiseAnd;   return true;
		case ELangTokenKind::Caret:              OutOp = EBinaryOp::BitwiseXor;   return true;
		case ELangTokenKind::Pipe:               OutOp = EBinaryOp::BitwiseOr;    return true;
		case ELangTokenKind::AmpersandAmpersand: OutOp = EBinaryOp::LogicalAnd;   return true;
		case ELangTokenKind::PipePipe:           OutOp = EBinaryOp::LogicalOr;    return true;
		default:
			return false;
		}
	}

	bool TryGetAssignOp(ELangTokenKind Kind, EAssignOp& OutOp)
	{
		switch (Kind)
		{
		case ELangTokenKind::Assign:              OutOp = EAssignOp::Assign;           return true;
		case ELangTokenKind::PlusAssign:          OutOp = EAssignOp::AddAssign;        return true;
		case ELangTokenKind::MinusAssign:         OutOp = EAssignOp::SubtractAssign;   return true;
		case ELangTokenKind::StarAssign:          OutOp = EAssignOp::MultiplyAssign;   return true;
		case ELangTokenKind::SlashAssign:         OutOp = EAssignOp::DivideAssign;     return true;
		case ELangTokenKind::PercentAssign:       OutOp = EAssignOp::ModuloAssign;     return true;
		case ELangTokenKind::AmpersandAssign:     OutOp = EAssignOp::AndAssign;        return true;
		case ELangTokenKind::PipeAssign:          OutOp = EAssignOp::OrAssign;         return true;
		case ELangTokenKind::CaretAssign:         OutOp = EAssignOp::XorAssign;        return true;
		case ELangTokenKind::LessLessAssign:      OutOp = EAssignOp::ShiftLeftAssign;  return true;
		case ELangTokenKind::GreaterGreaterAssign:OutOp = EAssignOp::ShiftRightAssign; return true;
		default:
			return false;
		}
	}

	bool TryGetPrefixUnaryOp(ELangTokenKind Kind, EUnaryOp& OutOp)
	{
		switch (Kind)
		{
		case ELangTokenKind::Plus:       OutOp = EUnaryOp::Plus;         return true;
		case ELangTokenKind::Minus:      OutOp = EUnaryOp::Negate;       return true;
		case ELangTokenKind::Bang:       OutOp = EUnaryOp::LogicalNot;   return true;
		case ELangTokenKind::Tilde:      OutOp = EUnaryOp::BitwiseNot;   return true;
		case ELangTokenKind::PlusPlus:   OutOp = EUnaryOp::PreIncrement; return true;
		case ELangTokenKind::MinusMinus: OutOp = EUnaryOp::PreDecrement; return true;
		default:
			return false;
		}
	}

	const TCHAR* GetBinaryOpSpelling(EBinaryOp Op)
	{
		switch (Op)
		{
		case EBinaryOp::Multiply:     return TEXT("*");
		case EBinaryOp::Divide:       return TEXT("/");
		case EBinaryOp::Modulo:       return TEXT("%");
		case EBinaryOp::Add:          return TEXT("+");
		case EBinaryOp::Subtract:     return TEXT("-");
		case EBinaryOp::ShiftLeft:    return TEXT("<<");
		case EBinaryOp::ShiftRight:   return TEXT(">>");
		case EBinaryOp::Less:         return TEXT("<");
		case EBinaryOp::LessEqual:    return TEXT("<=");
		case EBinaryOp::Greater:      return TEXT(">");
		case EBinaryOp::GreaterEqual: return TEXT(">=");
		case EBinaryOp::Equal:        return TEXT("==");
		case EBinaryOp::NotEqual:     return TEXT("!=");
		case EBinaryOp::BitwiseAnd:   return TEXT("&");
		case EBinaryOp::BitwiseXor:   return TEXT("^");
		case EBinaryOp::BitwiseOr:    return TEXT("|");
		case EBinaryOp::LogicalAnd:   return TEXT("&&");
		case EBinaryOp::LogicalOr:    return TEXT("||");
		default:
			break;
		}

		checkNoEntry();
		return TEXT("?");
	}

	const TCHAR* GetAssignOpSpelling(EAssignOp Op)
	{
		switch (Op)
		{
		case EAssignOp::Assign:           return TEXT("=");
		case EAssignOp::AddAssign:        return TEXT("+=");
		case EAssignOp::SubtractAssign:   return TEXT("-=");
		case EAssignOp::MultiplyAssign:   return TEXT("*=");
		case EAssignOp::DivideAssign:     return TEXT("/=");
		case EAssignOp::ModuloAssign:     return TEXT("%=");
		case EAssignOp::AndAssign:        return TEXT("&=");
		case EAssignOp::OrAssign:         return TEXT("|=");
		case EAssignOp::XorAssign:        return TEXT("^=");
		case EAssignOp::ShiftLeftAssign:  return TEXT("<<=");
		case EAssignOp::ShiftRightAssign: return TEXT(">>=");
		default:
			break;
		}

		checkNoEntry();
		return TEXT("=");
	}

	const TCHAR* GetUnaryOpSpelling(EUnaryOp Op, bool& bOutPostfix)
	{
		bOutPostfix = false;
		switch (Op)
		{
		case EUnaryOp::Plus:          return TEXT("+");
		case EUnaryOp::Negate:        return TEXT("-");
		case EUnaryOp::LogicalNot:    return TEXT("!");
		case EUnaryOp::BitwiseNot:    return TEXT("~");
		case EUnaryOp::PreIncrement:  return TEXT("++");
		case EUnaryOp::PreDecrement:  return TEXT("--");
		case EUnaryOp::PostIncrement: bOutPostfix = true; return TEXT("++");
		case EUnaryOp::PostDecrement: bOutPostfix = true; return TEXT("--");
		default:
			break;
		}

		checkNoEntry();
		return TEXT("+");
	}

	int32 GetExprPrecedence(const FExpr& Expr)
	{
		switch (Expr.Kind)
		{
		// Atoms: they either are a single token or carry their own brackets, so no operand of
		// theirs can ever be captured by a neighbouring operator.
		case ENodeKind::LiteralExpr:
		case ENodeKind::IdentifierExpr:
		case ENodeKind::TypeExpr:
		case ENodeKind::MemberExpr:
		case ENodeKind::IndexExpr:
		case ENodeKind::CallExpr:
		case ENodeKind::ParenExpr:
		case ENodeKind::InitializerListExpr:
			return PostfixPrecedence + 1;

		// Prefix and postfix `++`/`--` share the unary rung: a postfix node printed as the object
		// of a member access or an index still needs its parentheses back.
		case ENodeKind::UnaryExpr:
		case ENodeKind::CastExpr:
			return UnaryPrecedence;

		case ENodeKind::BinaryExpr:
			if (const FBinaryExpr* Binary = Expr.As<FBinaryExpr>())
			{
				return GetBinaryPrecedence(Binary->Op);
			}
			return 1;

		case ENodeKind::ConditionalExpr:
			return ConditionalPrecedence;

		case ENodeKind::AssignExpr:
			return AssignmentPrecedence;

		default:
			break;
		}

		// A statement or declaration node cannot reach here through the FExpr parameter; treat
		// anything unknown as an atom so the printer never drops parentheses it needed.
		return PostfixPrecedence + 1;
	}
}
