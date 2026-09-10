// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Operator tables shared by the expression parser and the printer: precedence, the token <-> op
// mapping, and spellings. One table, so the parser's idea of "what binds tighter" and the
// printer's idea of "where parentheses are required" cannot drift apart.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangAst.h"
#include "Lang/LangToken.h"

namespace UE::DreamShader::Lang::Private
{
	/**
	 * Binding power of a binary operator; higher binds tighter. HLSL / C order:
	 *
	 *   10  * / %
	 *    9  + -
	 *    8  << >>
	 *    7  < <= > >=
	 *    6  == !=
	 *    5  &
	 *    4  ^
	 *    3  |
	 *    2  &&
	 *    1  ||
	 *
	 * The conditional operator sits below all of these (ConditionalPrecedence) and assignment
	 * below that (AssignmentPrecedence); both associate to the right. Unary and postfix operators
	 * bind tighter than any binary operator.
	 */
	int32 GetBinaryPrecedence(EBinaryOp Op);

	constexpr int32 ConditionalPrecedence = 0;
	constexpr int32 AssignmentPrecedence = -1;
	constexpr int32 UnaryPrecedence = 11;
	constexpr int32 PostfixPrecedence = 12;

	/** True and the op when a token is a binary operator. `&&`, `||` and the bitwise family included. */
	bool TryGetBinaryOp(ELangTokenKind Kind, EBinaryOp& OutOp);
	/** True and the op when a token is `=` or a compound assignment. */
	bool TryGetAssignOp(ELangTokenKind Kind, EAssignOp& OutOp);
	/** True and the op when a token can start a prefix unary expression (`+ - ! ~ ++ --`). */
	bool TryGetPrefixUnaryOp(ELangTokenKind Kind, EUnaryOp& OutOp);

	const TCHAR* GetBinaryOpSpelling(EBinaryOp Op);
	const TCHAR* GetAssignOpSpelling(EAssignOp Op);
	/** Spelling of a unary operator; bOutPostfix tells the printer which side it goes on. */
	const TCHAR* GetUnaryOpSpelling(EUnaryOp Op, bool& bOutPostfix);

	/**
	 * The precedence an expression node itself has when it appears as an operand, for the printer's
	 * "do I need parentheses here" test. Literals, identifiers, calls, members and indexes are
	 * atoms (PostfixPrecedence + 1); a FParenExpr is an atom too, since it prints its own parentheses.
	 */
	int32 GetExprPrecedence(const FExpr& Expr);
}
