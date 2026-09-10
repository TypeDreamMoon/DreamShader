// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShaderLang 2.0 expression and statement tests (M1, agent C).
//
// Everything here goes through the PUBLIC front-end API -- ParseDreamShaderLangExpression for the
// expression grammar, ParseDreamShaderLang on a `void f() { ... }` source for statements -- so the
// tests survive any refactor of the parser's internals.
//
// Shape is asserted by dumping the tree into a compact S-expression and comparing the whole string:
// one line per case, and a failure prints the tree that was actually built instead of "expected 3,
// got 4". The operator spellings the dumper uses are written out below rather than read from
// LangOperators.h -- that header is private to DreamShaderLang, and an independent table is what
// makes a swapped EBinaryOp mapping visible.
//
// Diagnostics are asserted by CODE only (DSHnnnn); the messages are FText and the editor is
// localised, so their English is not a test fixture. String comparisons use TestEqualSensitive /
// Equals(..., ESearchCase::CaseSensitive) because FString::operator== is case-insensitive and a
// swizzle that came back `XY` instead of `xy` would otherwise pass.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"

namespace UE::DreamShader::Editor::Private::Lang2Tests
{
	using namespace UE::DreamShader::Lang;

	// ---------------------------------------------------------------------------------------------
	// Spellings
	// ---------------------------------------------------------------------------------------------

	inline const TCHAR* BinaryOpName(EBinaryOp Op)
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
		default:                      return TEXT("<binary?>");
		}
	}

	inline const TCHAR* AssignOpName(EAssignOp Op)
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
		default:                          return TEXT("<assign?>");
		}
	}

	/** `u` prefixes the prefix operators so a unary minus never reads like a subtraction in a dump. */
	inline const TCHAR* UnaryOpName(EUnaryOp Op)
	{
		switch (Op)
		{
		case EUnaryOp::Plus:          return TEXT("u+");
		case EUnaryOp::Negate:        return TEXT("u-");
		case EUnaryOp::LogicalNot:    return TEXT("u!");
		case EUnaryOp::BitwiseNot:    return TEXT("u~");
		case EUnaryOp::PreIncrement:  return TEXT("pre++");
		case EUnaryOp::PreDecrement:  return TEXT("pre--");
		case EUnaryOp::PostIncrement: return TEXT("post++");
		case EUnaryOp::PostDecrement: return TEXT("post--");
		default:                      return TEXT("<unary?>");
		}
	}

	inline const TCHAR* LiteralKindName(ELiteralKind Kind)
	{
		switch (Kind)
		{
		case ELiteralKind::Int:    return TEXT("int");
		case ELiteralKind::UInt:   return TEXT("uint");
		case ELiteralKind::Float:  return TEXT("float");
		case ELiteralKind::Bool:   return TEXT("bool");
		case ELiteralKind::String: return TEXT("string");
		default:                   return TEXT("<literal?>");
		}
	}

	inline const TCHAR* StorageName(EStorageClass Storage)
	{
		switch (Storage)
		{
		case EStorageClass::None:        return TEXT("-");
		case EStorageClass::Uniform:     return TEXT("uniform");
		case EStorageClass::StaticConst: return TEXT("static-const");
		case EStorageClass::Static:      return TEXT("static");
		case EStorageClass::Const:       return TEXT("const");
		default:                         return TEXT("<storage?>");
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Tree dumps
	// ---------------------------------------------------------------------------------------------

	inline FString DescribeExpr(const FExpr* Expr);
	inline FString DescribeStmt(const FStmt* Stmt);

	inline FString DescribeExpr(const FExpr* Expr)
	{
		if (!Expr)
		{
			return TEXT("-");
		}

		switch (Expr->Kind)
		{
		case ENodeKind::LiteralExpr:
			if (const FLiteralExpr* Literal = Expr->As<FLiteralExpr>())
			{
				return FString::Printf(TEXT("(lit %s %s)"), LiteralKindName(Literal->LiteralKind), *Literal->Text);
			}
			break;

		case ENodeKind::IdentifierExpr:
			if (const FIdentifierExpr* Identifier = Expr->As<FIdentifierExpr>())
			{
				return FString::Printf(TEXT("(id %s)"), *Identifier->Name);
			}
			break;

		case ENodeKind::TypeExpr:
			if (const FTypeExpr* Type = Expr->As<FTypeExpr>())
			{
				return FString::Printf(TEXT("(type %s)"), *Type->Type.Name);
			}
			break;

		case ENodeKind::MemberExpr:
			if (const FMemberExpr* Member = Expr->As<FMemberExpr>())
			{
				return FString::Printf(TEXT("(member %s %s)"), *DescribeExpr(Member->Object.Get()), *Member->Member);
			}
			break;

		case ENodeKind::IndexExpr:
			if (const FIndexExpr* Index = Expr->As<FIndexExpr>())
			{
				return FString::Printf(TEXT("(index %s %s)"), *DescribeExpr(Index->Object.Get()), *DescribeExpr(Index->Index.Get()));
			}
			break;

		case ENodeKind::CallExpr:
			if (const FCallExpr* Call = Expr->As<FCallExpr>())
			{
				FString Result = FString::Printf(TEXT("(call %s"), *DescribeExpr(Call->Callee.Get()));
				for (const FArgument& Argument : Call->Arguments)
				{
					if (Argument.Name.IsEmpty())
					{
						Result += FString::Printf(TEXT(" %s"), *DescribeExpr(Argument.Value.Get()));
					}
					else
					{
						Result += FString::Printf(TEXT(" (arg %s %s)"), *Argument.Name, *DescribeExpr(Argument.Value.Get()));
					}
				}
				return Result + TEXT(")");
			}
			break;

		case ENodeKind::UnaryExpr:
			if (const FUnaryExpr* Unary = Expr->As<FUnaryExpr>())
			{
				return FString::Printf(TEXT("(%s %s)"), UnaryOpName(Unary->Op), *DescribeExpr(Unary->Operand.Get()));
			}
			break;

		case ENodeKind::BinaryExpr:
			if (const FBinaryExpr* Binary = Expr->As<FBinaryExpr>())
			{
				return FString::Printf(TEXT("(%s %s %s)"),
					BinaryOpName(Binary->Op),
					*DescribeExpr(Binary->Left.Get()),
					*DescribeExpr(Binary->Right.Get()));
			}
			break;

		case ENodeKind::AssignExpr:
			if (const FAssignExpr* Assign = Expr->As<FAssignExpr>())
			{
				return FString::Printf(TEXT("(%s %s %s)"),
					AssignOpName(Assign->Op),
					*DescribeExpr(Assign->Target.Get()),
					*DescribeExpr(Assign->Value.Get()));
			}
			break;

		case ENodeKind::ConditionalExpr:
			if (const FConditionalExpr* Conditional = Expr->As<FConditionalExpr>())
			{
				return FString::Printf(TEXT("(?: %s %s %s)"),
					*DescribeExpr(Conditional->Condition.Get()),
					*DescribeExpr(Conditional->TrueValue.Get()),
					*DescribeExpr(Conditional->FalseValue.Get()));
			}
			break;

		case ENodeKind::CastExpr:
			if (const FCastExpr* Cast = Expr->As<FCastExpr>())
			{
				return FString::Printf(TEXT("(cast %s %s)"), *Cast->Type.Name, *DescribeExpr(Cast->Operand.Get()));
			}
			break;

		case ENodeKind::InitializerListExpr:
			if (const FInitializerListExpr* List = Expr->As<FInitializerListExpr>())
			{
				FString Result = TEXT("(init");
				for (const FExprPtr& Element : List->Elements)
				{
					Result += FString::Printf(TEXT(" %s"), *DescribeExpr(Element.Get()));
				}
				return Result + TEXT(")");
			}
			break;

		case ENodeKind::ParenExpr:
			if (const FParenExpr* Paren = Expr->As<FParenExpr>())
			{
				return FString::Printf(TEXT("(paren %s)"), *DescribeExpr(Paren->Inner.Get()));
			}
			break;

		default:
			break;
		}

		return FString::Printf(TEXT("<expr? %s>"), LexToString(Expr->Kind));
	}

	inline FString DescribeDeclarator(const FDeclarator& Declarator)
	{
		FString Result = FString::Printf(TEXT("(d %s"), *Declarator.Name);
		for (const FExprPtr& Dimension : Declarator.ArrayDimensions)
		{
			if (Dimension.IsValid())
			{
				Result += FString::Printf(TEXT(" [%s]"), *DescribeExpr(Dimension.Get()));
			}
			else
			{
				Result += TEXT(" []");
			}
		}
		if (Declarator.Initializer.IsValid())
		{
			Result += FString::Printf(TEXT(" = %s"), *DescribeExpr(Declarator.Initializer.Get()));
		}
		return Result + TEXT(")");
	}

	inline FString DescribeStmt(const FStmt* Stmt)
	{
		if (!Stmt)
		{
			return TEXT("-");
		}

		switch (Stmt->Kind)
		{
		case ENodeKind::VarDeclStmt:
			if (const FVarDeclStmt* VarDecl = Stmt->As<FVarDeclStmt>())
			{
				FString Result = FString::Printf(TEXT("(var %s %s"), StorageName(VarDecl->Storage), *VarDecl->Type.Name);
				for (const FDeclarator& Declarator : VarDecl->Declarators)
				{
					Result += FString::Printf(TEXT(" %s"), *DescribeDeclarator(Declarator));
				}
				return Result + TEXT(")");
			}
			break;

		case ENodeKind::ExprStmt:
			if (const FExprStmt* ExprStmt = Stmt->As<FExprStmt>())
			{
				return FString::Printf(TEXT("(expr %s)"), *DescribeExpr(ExprStmt->Expression.Get()));
			}
			break;

		case ENodeKind::BlockStmt:
			if (const FBlockStmt* Block = Stmt->As<FBlockStmt>())
			{
				FString Result = TEXT("(block");
				for (const FStmtPtr& Inner : Block->Statements)
				{
					Result += FString::Printf(TEXT(" %s"), *DescribeStmt(Inner.Get()));
				}
				return Result + TEXT(")");
			}
			break;

		case ENodeKind::IfStmt:
			if (const FIfStmt* If = Stmt->As<FIfStmt>())
			{
				return FString::Printf(TEXT("(if %s %s %s)"),
					*DescribeExpr(If->Condition.Get()),
					*DescribeStmt(If->Then.Get()),
					*DescribeStmt(If->Else.Get()));
			}
			break;

		case ENodeKind::ForStmt:
			if (const FForStmt* For = Stmt->As<FForStmt>())
			{
				return FString::Printf(TEXT("(for %s %s %s %s)"),
					*DescribeStmt(For->Init.Get()),
					*DescribeExpr(For->Condition.Get()),
					*DescribeExpr(For->Step.Get()),
					*DescribeStmt(For->Body.Get()));
			}
			break;

		case ENodeKind::WhileStmt:
			if (const FWhileStmt* While = Stmt->As<FWhileStmt>())
			{
				return FString::Printf(TEXT("(while %s %s)"),
					*DescribeExpr(While->Condition.Get()),
					*DescribeStmt(While->Body.Get()));
			}
			break;

		case ENodeKind::DoWhileStmt:
			if (const FDoWhileStmt* DoWhile = Stmt->As<FDoWhileStmt>())
			{
				return FString::Printf(TEXT("(do %s %s)"),
					*DescribeStmt(DoWhile->Body.Get()),
					*DescribeExpr(DoWhile->Condition.Get()));
			}
			break;

		case ENodeKind::ReturnStmt:
			if (const FReturnStmt* Return = Stmt->As<FReturnStmt>())
			{
				return FString::Printf(TEXT("(return %s)"), *DescribeExpr(Return->Value.Get()));
			}
			break;

		case ENodeKind::BreakStmt:    return TEXT("(break)");
		case ENodeKind::ContinueStmt: return TEXT("(continue)");
		case ENodeKind::DiscardStmt:  return TEXT("(discard)");
		case ENodeKind::EmptyStmt:    return TEXT("(empty)");

		default:
			break;
		}

		return FString::Printf(TEXT("<stmt? %s>"), LexToString(Stmt->Kind));
	}

	// ---------------------------------------------------------------------------------------------
	// Parse helpers
	// ---------------------------------------------------------------------------------------------

	inline FString FirstErrorCode(const FLangDiagnosticSink& Diagnostics)
	{
		const FLangDiagnostic* First = Diagnostics.FirstError();
		return First ? First->Code : FString(TEXT("<none>"));
	}

	inline FExprPtr ParseExpr(const TCHAR* Text, FLangDiagnosticSink& Diagnostics)
	{
		// Spans are offsets, not pointers, so the tree outlives the source text it was cut from.
		const FLangSourceText Source(TEXT("Expr.dss"), Text);
		return ParseDreamShaderLangExpression(Source, Diagnostics);
	}

	/** The dumped shape of one expression, or the first diagnostic code when it does not parse. */
	inline FString ExprShape(const TCHAR* Text)
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(Text, Diagnostics);
		if (!Expr.IsValid())
		{
			return FirstErrorCode(Diagnostics);
		}
		if (Diagnostics.HasErrors())
		{
			// A tree AND an error is a contract violation of its own; say so instead of comparing shapes.
			return FString::Printf(TEXT("%s+%s"), *FirstErrorCode(Diagnostics), *DescribeExpr(Expr.Get()));
		}
		return DescribeExpr(Expr.Get());
	}

	/** The first diagnostic code of an expression expected NOT to parse. */
	inline FString ExprErrorCode(const TCHAR* Text)
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(Text, Diagnostics);
		return Expr.IsValid() ? FString(TEXT("<parsed>")) : FirstErrorCode(Diagnostics);
	}

	/** One `void f() { ... }` parse: the module keeps owning the tree, the raw pointers index into it. */
	struct FParsedBody
	{
		FLangParseResult Result;
		const FFunctionDecl* Function = nullptr;
		const FBlockStmt* Body = nullptr;
		FString FirstError;
		int32 NumErrors = 0;
		int32 NumDeclarations = 0;
	};

	inline FParsedBody ParseFunctionBody(const TCHAR* BodyText)
	{
		FParsedBody Parsed;

		const FString SourceText = FString::Printf(TEXT("void f()\n{\n%s\n}\n"), BodyText);
		const FLangSourceText Source(TEXT("Stmt.dss"), SourceText);
		Parsed.Result = ParseDreamShaderLang(Source);

		if (Parsed.Result.Module.IsValid())
		{
			Parsed.NumDeclarations = Parsed.Result.Module->Declarations.Num();
			for (const FDeclPtr& Declaration : Parsed.Result.Module->Declarations)
			{
				const FFunctionDecl* Function = Declaration.IsValid() ? Declaration->As<FFunctionDecl>() : nullptr;
				if (Function)
				{
					Parsed.Function = Function;
					Parsed.Body = Function->Body.Get();
					break;
				}
			}
		}

		Parsed.FirstError = FirstErrorCode(Parsed.Result.Diagnostics);
		Parsed.NumErrors = Parsed.Result.Diagnostics.NumErrors();
		return Parsed;
	}

	/** The dumped statement list of `void f() { <BodyText> }`, or the first diagnostic code. */
	inline FString BodyShape(const TCHAR* BodyText)
	{
		const FParsedBody Parsed = ParseFunctionBody(BodyText);
		if (Parsed.Result.Diagnostics.HasErrors())
		{
			return Parsed.FirstError;
		}
		if (!Parsed.Body)
		{
			return TEXT("<no body>");
		}
		return DescribeStmt(Parsed.Body);
	}

	/** The dump of the single statement a body was expected to hold. */
	inline FString SingleStmtShape(const TCHAR* BodyText)
	{
		const FParsedBody Parsed = ParseFunctionBody(BodyText);
		if (Parsed.Result.Diagnostics.HasErrors())
		{
			return Parsed.FirstError;
		}
		if (!Parsed.Body)
		{
			return TEXT("<no body>");
		}
		if (Parsed.Body->Statements.Num() != 1)
		{
			return FString::Printf(TEXT("<%d statements>"), Parsed.Body->Statements.Num());
		}
		return DescribeStmt(Parsed.Body->Statements[0].Get());
	}
}

// =================================================================================================
// Expressions
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionPrecedenceTest,
	"DreamShader.Lang2.Expressions.Precedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionPrecedenceTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// The C ladder, one rung at a time: the tighter operator has to end up deeper in the tree.
	TestEqualSensitive(TEXT("a + b * c"), ExprShape(TEXT("a + b * c")), FString(TEXT("(+ (id a) (* (id b) (id c)))")));
	TestEqualSensitive(TEXT("a * b + c"), ExprShape(TEXT("a * b + c")), FString(TEXT("(+ (* (id a) (id b)) (id c))")));
	TestEqualSensitive(TEXT("a + b % c"), ExprShape(TEXT("a + b % c")), FString(TEXT("(+ (id a) (% (id b) (id c)))")));

	// Every binary operator is left-associative: `a - b - c` is `(a - b) - c`, never `a - (b - c)`.
	TestEqualSensitive(TEXT("a - b - c"), ExprShape(TEXT("a - b - c")), FString(TEXT("(- (- (id a) (id b)) (id c))")));
	TestEqualSensitive(TEXT("a / b / c"), ExprShape(TEXT("a / b / c")), FString(TEXT("(/ (/ (id a) (id b)) (id c))")));

	// Shifts bind tighter than the relational operators, which bind tighter than equality.
	TestEqualSensitive(TEXT("a << b < c"), ExprShape(TEXT("a << b < c")), FString(TEXT("(< (<< (id a) (id b)) (id c))")));
	TestEqualSensitive(TEXT("a < b == c"), ExprShape(TEXT("a < b == c")), FString(TEXT("(== (< (id a) (id b)) (id c))")));
	TestEqualSensitive(TEXT("a + b >> c"), ExprShape(TEXT("a + b >> c")), FString(TEXT("(>> (+ (id a) (id b)) (id c))")));

	// Bitwise & ^ | then && then ||, the classic C order the 1.x parser never had.
	TestEqualSensitive(TEXT("a & b == c"), ExprShape(TEXT("a & b == c")), FString(TEXT("(& (id a) (== (id b) (id c)))")));
	TestEqualSensitive(TEXT("a | b ^ c & d"), ExprShape(TEXT("a | b ^ c & d")), FString(TEXT("(| (id a) (^ (id b) (& (id c) (id d))))")));
	TestEqualSensitive(TEXT("a || b && c"), ExprShape(TEXT("a || b && c")), FString(TEXT("(|| (id a) (&& (id b) (id c)))")));
	TestEqualSensitive(TEXT("a && b | c"), ExprShape(TEXT("a && b | c")), FString(TEXT("(&& (id a) (| (id b) (id c)))")));

	// Parentheses the author wrote survive as FParenExpr, and change the shape.
	TestEqualSensitive(TEXT("(a + b) * c"), ExprShape(TEXT("(a + b) * c")), FString(TEXT("(* (paren (+ (id a) (id b))) (id c))")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionAssociativityTest,
	"DreamShader.Lang2.Expressions.Associativity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionAssociativityTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// Assignment is right-associative: `a = b = c` is `a = (b = c)`.
	TestEqualSensitive(TEXT("a = b = c"), ExprShape(TEXT("a = b = c")), FString(TEXT("(= (id a) (= (id b) (id c)))")));
	TestEqualSensitive(TEXT("a += b -= c"), ExprShape(TEXT("a += b -= c")), FString(TEXT("(+= (id a) (-= (id b) (id c)))")));
	TestEqualSensitive(TEXT("a *= b + c"), ExprShape(TEXT("a *= b + c")), FString(TEXT("(*= (id a) (+ (id b) (id c)))")));

	// The conditional is right-associative too, and sits between the binary rungs and assignment.
	TestEqualSensitive(TEXT("a ? b : c ? d : e"),
		ExprShape(TEXT("a ? b : c ? d : e")),
		FString(TEXT("(?: (id a) (id b) (?: (id c) (id d) (id e)))")));
	TestEqualSensitive(TEXT("a ? b ? c : d : e"),
		ExprShape(TEXT("a ? b ? c : d : e")),
		FString(TEXT("(?: (id a) (?: (id b) (id c) (id d)) (id e))")));
	TestEqualSensitive(TEXT("a = b ? c : d"),
		ExprShape(TEXT("a = b ? c : d")),
		FString(TEXT("(= (id a) (?: (id b) (id c) (id d)))")));
	TestEqualSensitive(TEXT("a || b ? c : d"),
		ExprShape(TEXT("a || b ? c : d")),
		FString(TEXT("(?: (|| (id a) (id b)) (id c) (id d))")));

	// The middle operand is fenced by `?` and `:`, so it takes a whole expression.
	TestEqualSensitive(TEXT("a ? b = c : d"),
		ExprShape(TEXT("a ? b = c : d")),
		FString(TEXT("(?: (id a) (= (id b) (id c)) (id d))")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionUnaryTest,
	"DreamShader.Lang2.Expressions.Unary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionUnaryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// A unary operator binds tighter than any binary one: `-a * b` is `(-a) * b`.
	TestEqualSensitive(TEXT("-a * b"), ExprShape(TEXT("-a * b")), FString(TEXT("(* (u- (id a)) (id b))")));
	TestEqualSensitive(TEXT("-a + b"), ExprShape(TEXT("-a + b")), FString(TEXT("(+ (u- (id a)) (id b))")));
	TestEqualSensitive(TEXT("!a && b || c"),
		ExprShape(TEXT("!a && b || c")),
		FString(TEXT("(|| (&& (u! (id a)) (id b)) (id c))")));
	TestEqualSensitive(TEXT("~a | b"), ExprShape(TEXT("~a | b")), FString(TEXT("(| (u~ (id a)) (id b))")));
	TestEqualSensitive(TEXT("+a - -b"), ExprShape(TEXT("+a - -b")), FString(TEXT("(- (u+ (id a)) (u- (id b)))")));

	// ... but looser than a member access or an index, which belong to the operand.
	TestEqualSensitive(TEXT("-a.b"), ExprShape(TEXT("-a.b")), FString(TEXT("(u- (member (id a) b))")));
	TestEqualSensitive(TEXT("-a[0]"), ExprShape(TEXT("-a[0]")), FString(TEXT("(u- (index (id a) (lit int 0)))")));
	TestEqualSensitive(TEXT("!f(x)"), ExprShape(TEXT("!f(x)")), FString(TEXT("(u! (call (id f) (id x)))")));

	// Unary operators stack.
	TestEqualSensitive(TEXT("- -a"), ExprShape(TEXT("- -a")), FString(TEXT("(u- (u- (id a)))")));
	TestEqualSensitive(TEXT("!!a"), ExprShape(TEXT("!!a")), FString(TEXT("(u! (u! (id a)))")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionIncrementTest,
	"DreamShader.Lang2.Expressions.Increment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionIncrementTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// Same token, two nodes: which side it was written on decides the EUnaryOp.
	TestEqualSensitive(TEXT("++a"), ExprShape(TEXT("++a")), FString(TEXT("(pre++ (id a))")));
	TestEqualSensitive(TEXT("a++"), ExprShape(TEXT("a++")), FString(TEXT("(post++ (id a))")));
	TestEqualSensitive(TEXT("--a"), ExprShape(TEXT("--a")), FString(TEXT("(pre-- (id a))")));
	TestEqualSensitive(TEXT("a--"), ExprShape(TEXT("a--")), FString(TEXT("(post-- (id a))")));
	TestEqualSensitive(TEXT("a++ + ++b"),
		ExprShape(TEXT("a++ + ++b")),
		FString(TEXT("(+ (post++ (id a)) (pre++ (id b)))")));

	// Postfix `++` applies to the whole postfix chain that precedes it.
	TestEqualSensitive(TEXT("a[0]++"), ExprShape(TEXT("a[0]++")), FString(TEXT("(post++ (index (id a) (lit int 0)))")));
	TestEqualSensitive(TEXT("a.x++"), ExprShape(TEXT("a.x++")), FString(TEXT("(post++ (member (id a) x))")));
	TestEqualSensitive(TEXT("--a.x"), ExprShape(TEXT("--a.x")), FString(TEXT("(pre-- (member (id a) x))")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionCastTest,
	"DreamShader.Lang2.Expressions.Casts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionCastTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// `(Type)x` is a cast ...
	TestEqualSensitive(TEXT("(float3)x"), ExprShape(TEXT("(float3)x")), FString(TEXT("(cast float3 (id x))")));
	TestEqualSensitive(TEXT("(float)i / 2.0f"),
		ExprShape(TEXT("(float)i / 2.0f")),
		FString(TEXT("(/ (cast float (id i)) (lit float 2.0f))")));
	TestEqualSensitive(TEXT("(float3)(x + y)"),
		ExprShape(TEXT("(float3)(x + y)")),
		FString(TEXT("(cast float3 (paren (+ (id x) (id y))))")));
	TestEqualSensitive(TEXT("(float3)-x"), ExprShape(TEXT("(float3)-x")), FString(TEXT("(cast float3 (u- (id x)))")));
	TestEqualSensitive(TEXT("(float3)x.rgb"), ExprShape(TEXT("(float3)x.rgb")), FString(TEXT("(cast float3 (member (id x) rgb))")));

	// ... and `(x)` is a parenthesised expression, not a cast to a type called `x`.
	TestEqualSensitive(TEXT("(x)"), ExprShape(TEXT("(x)")), FString(TEXT("(paren (id x))")));
	TestEqualSensitive(TEXT("(a) - b"), ExprShape(TEXT("(a) - b")), FString(TEXT("(- (paren (id a)) (id b))")));
	TestEqualSensitive(TEXT("(a) + b"), ExprShape(TEXT("(a) + b")), FString(TEXT("(+ (paren (id a)) (id b))")));
	TestEqualSensitive(TEXT("(a) * b"), ExprShape(TEXT("(a) * b")), FString(TEXT("(* (paren (id a)) (id b))")));
	TestEqualSensitive(TEXT("(a) ? b : c"), ExprShape(TEXT("(a) ? b : c")), FString(TEXT("(?: (paren (id a)) (id b) (id c))")));
	TestEqualSensitive(TEXT("(a = b)"), ExprShape(TEXT("(a = b)")), FString(TEXT("(paren (= (id a) (id b)))")));

	// A user type is still a cast when what follows can only be an operand.
	TestEqualSensitive(TEXT("(MyStruct)x"), ExprShape(TEXT("(MyStruct)x")), FString(TEXT("(cast MyStruct (id x))")));

	// The cast target is classified exactly the way a declaration's type is.
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("(float3)x"), Diagnostics);
		const FCastExpr* Cast = Expr.IsValid() ? Expr->As<FCastExpr>() : nullptr;
		if (TestNotNull(TEXT("(float3)x is a cast"), Cast))
		{
			TestEqualSensitive(TEXT("cast type spelling"), Cast->Type.Name, FString(TEXT("float3")));
			TestEqual(TEXT("cast type category"), static_cast<int32>(Cast->Type.Category), static_cast<int32>(ETypeCategory::Vector));
			TestEqual(TEXT("cast type rows"), Cast->Type.Rows, 3);
		}
	}
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("(MyStruct)x"), Diagnostics);
		const FCastExpr* Cast = Expr.IsValid() ? Expr->As<FCastExpr>() : nullptr;
		if (TestNotNull(TEXT("(MyStruct)x is a cast"), Cast))
		{
			TestEqual(TEXT("user cast category"), static_cast<int32>(Cast->Type.Category), static_cast<int32>(ETypeCategory::Named));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionConstructorTest,
	"DreamShader.Lang2.Expressions.Constructors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionConstructorTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// A constructor is a call whose callee is a FTypeExpr -- nothing downstream has to re-decide
	// whether `float3` was a type or a function.
	TestEqualSensitive(TEXT("float3(1, 2, 3)"),
		ExprShape(TEXT("float3(1, 2, 3)")),
		FString(TEXT("(call (type float3) (lit int 1) (lit int 2) (lit int 3))")));
	TestEqualSensitive(TEXT("float4(v.xyz, 1.0)"),
		ExprShape(TEXT("float4(v.xyz, 1.0)")),
		FString(TEXT("(call (type float4) (member (id v) xyz) (lit float 1.0))")));
	TestEqualSensitive(TEXT("float3(0, 0, 0).x"),
		ExprShape(TEXT("float3(0, 0, 0).x")),
		FString(TEXT("(member (call (type float3) (lit int 0) (lit int 0) (lit int 0)) x)")));
	TestEqualSensitive(TEXT("float2()"), ExprShape(TEXT("float2()")), FString(TEXT("(call (type float2))")));

	// A bare built-in type name stays a type node.
	TestEqualSensitive(TEXT("float3"), ExprShape(TEXT("float3")), FString(TEXT("(type float3)")));

	// A user name is an identifier; whether it names a struct is the semantic pass's problem.
	TestEqualSensitive(TEXT("MyStruct(1)"), ExprShape(TEXT("MyStruct(1)")), FString(TEXT("(call (id MyStruct) (lit int 1))")));

	// The callee really is a FTypeExpr carrying a classified FTypeRef.
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("float3(1, 2, 3)"), Diagnostics);
		const FCallExpr* Call = Expr.IsValid() ? Expr->As<FCallExpr>() : nullptr;
		if (TestNotNull(TEXT("float3(...) is a call"), Call))
		{
			const FTypeExpr* Callee = Call->Callee.IsValid() ? Call->Callee->As<FTypeExpr>() : nullptr;
			if (TestNotNull(TEXT("callee is a type expression"), Callee))
			{
				TestEqual(TEXT("constructor type category"), static_cast<int32>(Callee->Type.Category), static_cast<int32>(ETypeCategory::Vector));
				TestEqual(TEXT("constructor component count"), Callee->Type.ComponentCount(), 3);
			}
			TestEqual(TEXT("constructor argument count"), Call->Arguments.Num(), 3);
			TestFalse(TEXT("constructor has no named arguments"), Call->HasNamedArguments());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionMemberChainTest,
	"DreamShader.Lang2.Expressions.MemberChains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionMemberChainTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// The 1.x Graph line that has to keep its spelling in 2.0.
	TestEqualSensitive(TEXT("UE.TexCoord(Index = 0).xy"),
		ExprShape(TEXT("UE.TexCoord(Index = 0).xy")),
		FString(TEXT("(member (call (member (id UE) TexCoord) (arg Index (lit int 0))) xy)")));

	TestEqualSensitive(TEXT("Tex.Sample(UV)"),
		ExprShape(TEXT("Tex.Sample(UV)")),
		FString(TEXT("(call (member (id Tex) Sample) (id UV))")));

	// Members, indexes and calls chain in written order.
	TestEqualSensitive(TEXT("a.b.c"), ExprShape(TEXT("a.b.c")), FString(TEXT("(member (member (id a) b) c)")));
	TestEqualSensitive(TEXT("a.b[1].c"),
		ExprShape(TEXT("a.b[1].c")),
		FString(TEXT("(member (index (member (id a) b) (lit int 1)) c)")));
	TestEqualSensitive(TEXT("f(x).y[0]"),
		ExprShape(TEXT("f(x).y[0]")),
		FString(TEXT("(index (member (call (id f) (id x)) y) (lit int 0))")));
	TestEqualSensitive(TEXT("a[i + 1]"),
		ExprShape(TEXT("a[i + 1]")),
		FString(TEXT("(index (id a) (+ (id i) (lit int 1)))")));

	// A swizzle is an ordinary member access, and its case is preserved.
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("v.XY"), Diagnostics);
		const FMemberExpr* Member = Expr.IsValid() ? Expr->As<FMemberExpr>() : nullptr;
		if (TestNotNull(TEXT("v.XY is a member access"), Member))
		{
			TestTrue(TEXT("swizzle case is kept"), Member->Member.Equals(TEXT("XY"), ESearchCase::CaseSensitive));
			TestFalse(TEXT("swizzle is not lower-cased"), Member->Member.Equals(TEXT("xy"), ESearchCase::CaseSensitive));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionNamedArgumentTest,
	"DreamShader.Lang2.Expressions.NamedArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionNamedArgumentTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// `Ident =` at the top level of an argument is a named argument, i.e. a pin binding.
	TestEqualSensitive(TEXT("Fn(A = 1, B = 2)"),
		ExprShape(TEXT("Fn(A = 1, B = 2)")),
		FString(TEXT("(call (id Fn) (arg A (lit int 1)) (arg B (lit int 2)))")));

	// Positional before named is fine; only the other order is not.
	TestEqualSensitive(TEXT("Fn(x, A = 1)"),
		ExprShape(TEXT("Fn(x, A = 1)")),
		FString(TEXT("(call (id Fn) (id x) (arg A (lit int 1)))")));

	// A positional argument after a named one is DSH2158.
	TestEqualSensitive(TEXT("Fn(A = 1, x) is DSH2158"), ExprErrorCode(TEXT("Fn(A = 1, x)")), FString(TEXT("DSH2158")));
	TestEqualSensitive(TEXT("Fn(A = 1, 2) is DSH2158"), ExprErrorCode(TEXT("Fn(A = 1, 2)")), FString(TEXT("DSH2158")));

	// An assignment as a POSITIONAL argument has to be parenthesised -- and then it stays one.
	TestEqualSensitive(TEXT("Fn((a = b), c)"),
		ExprShape(TEXT("Fn((a = b), c)")),
		FString(TEXT("(call (id Fn) (paren (= (id a) (id b))) (id c))")));

	// `==` is one token, so a comparison is never mistaken for a pin binding.
	TestEqualSensitive(TEXT("Fn(a == b)"), ExprShape(TEXT("Fn(a == b)")), FString(TEXT("(call (id Fn) (== (id a) (id b)))")));
	TestEqualSensitive(TEXT("Fn(A += 1)"), ExprShape(TEXT("Fn(A += 1)")), FString(TEXT("(call (id Fn) (+= (id A) (lit int 1)))")));

	// A named argument only names the top level of its own call; a nested call names its own pins.
	TestEqualSensitive(TEXT("Fn(A = g(B = 1))"),
		ExprShape(TEXT("Fn(A = g(B = 1))")),
		FString(TEXT("(call (id Fn) (arg A (call (id g) (arg B (lit int 1)))))")));

	// The name is recorded verbatim, case included, because it has to match a pin.
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("UE.TexCoord(Index = 0)"), Diagnostics);
		const FCallExpr* Call = Expr.IsValid() ? Expr->As<FCallExpr>() : nullptr;
		if (TestNotNull(TEXT("UE.TexCoord(Index = 0) is a call"), Call))
		{
			TestTrue(TEXT("the call has named arguments"), Call->HasNamedArguments());
			if (TestEqual(TEXT("argument count"), Call->Arguments.Num(), 1))
			{
				TestTrue(TEXT("argument name case is kept"), Call->Arguments[0].Name.Equals(TEXT("Index"), ESearchCase::CaseSensitive));
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionLiteralTest,
	"DreamShader.Lang2.Expressions.Literals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionLiteralTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// FLiteralExpr::Text is the lexeme as written, so the printer reproduces `1.0f`, `0x10`, `2u`.
	struct FLiteralCase
	{
		const TCHAR* Source;
		ELiteralKind Kind;
		const TCHAR* Text;
	};

	const FLiteralCase Cases[] =
	{
		{ TEXT("1"),      ELiteralKind::Int,    TEXT("1") },
		{ TEXT("0x10"),   ELiteralKind::Int,    TEXT("0x10") },
		{ TEXT("2u"),     ELiteralKind::UInt,   TEXT("2u") },
		{ TEXT("1.0f"),   ELiteralKind::Float,  TEXT("1.0f") },
		{ TEXT("1.5e-3"), ELiteralKind::Float,  TEXT("1.5e-3") },
		{ TEXT("true"),   ELiteralKind::Bool,   TEXT("true") },
		{ TEXT("false"),  ELiteralKind::Bool,   TEXT("false") },
	};

	for (const FLiteralCase& Case : Cases)
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(Case.Source, Diagnostics);
		const FLiteralExpr* Literal = Expr.IsValid() ? Expr->As<FLiteralExpr>() : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("%s is a literal"), Case.Source), Literal))
		{
			TestEqual(*FString::Printf(TEXT("%s literal kind"), Case.Source),
				static_cast<int32>(Literal->LiteralKind), static_cast<int32>(Case.Kind));
			TestEqualSensitive(*FString::Printf(TEXT("%s lexeme"), Case.Source), Literal->Text, FString(Case.Text));
		}
	}

	// The decoded values ride along with the lexeme.
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("0x10"), Diagnostics);
		const FLiteralExpr* Literal = Expr.IsValid() ? Expr->As<FLiteralExpr>() : nullptr;
		if (TestNotNull(TEXT("0x10 is a literal"), Literal))
		{
			TestEqual(TEXT("0x10 value"), static_cast<int32>(Literal->Integer), 16);
		}
	}
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("true"), Diagnostics);
		const FLiteralExpr* Literal = Expr.IsValid() ? Expr->As<FLiteralExpr>() : nullptr;
		if (TestNotNull(TEXT("true is a literal"), Literal))
		{
			TestTrue(TEXT("true decodes to true"), Literal->bBool);
		}
	}

	// A string literal reaches the tree with its escapes already resolved.
	{
		FLangDiagnosticSink Diagnostics;
		const FExprPtr Expr = ParseExpr(TEXT("\"/Game/Shared/Noise\""), Diagnostics);
		const FLiteralExpr* Literal = Expr.IsValid() ? Expr->As<FLiteralExpr>() : nullptr;
		if (TestNotNull(TEXT("a string is a literal"), Literal))
		{
			TestEqual(TEXT("string literal kind"), static_cast<int32>(Literal->LiteralKind), static_cast<int32>(ELiteralKind::String));
			TestEqualSensitive(TEXT("string value"), Literal->Text, FString(TEXT("/Game/Shared/Noise")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2ExpressionErrorTest,
	"DreamShader.Lang2.Expressions.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2ExpressionErrorTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// Running out of tokens mid-expression is DSH2150, wherever it happens.
	TestEqualSensitive(TEXT("`a +` is DSH2150"), ExprErrorCode(TEXT("a +")), FString(TEXT("DSH2150")));

	// A token that cannot start an operand is DSH2151.
	TestEqualSensitive(TEXT("`,` is DSH2151"), ExprErrorCode(TEXT(",")), FString(TEXT("DSH2151")));
	TestEqualSensitive(TEXT("`a + *b` is DSH2151"), ExprErrorCode(TEXT("a + *b")), FString(TEXT("DSH2151")));

	// Unclosed brackets each name their own closer.
	TestEqualSensitive(TEXT("`(1 + 2` is DSH2152"), ExprErrorCode(TEXT("(1 + 2")), FString(TEXT("DSH2152")));
	TestEqualSensitive(TEXT("`f(1` is DSH2152"), ExprErrorCode(TEXT("f(1")), FString(TEXT("DSH2152")));
	TestEqualSensitive(TEXT("`a[1` is DSH2153"), ExprErrorCode(TEXT("a[1")), FString(TEXT("DSH2153")));

	// A conditional without its `:` is DSH2159.
	TestEqualSensitive(TEXT("`a ? b` is DSH2159"), ExprErrorCode(TEXT("a ? b")), FString(TEXT("DSH2159")));

	// `.` has to be followed by a name -- but only a `.` the lexer actually handed over as a Dot.
	TestEqualSensitive(TEXT("`a.+b` is DSH2161"), ExprErrorCode(TEXT("a.+b")), FString(TEXT("DSH2161")));
	TestEqualSensitive(TEXT("`a.` is DSH2161"), ExprErrorCode(TEXT("a.")), FString(TEXT("DSH2161")));

	// `a.1` never reaches that check: a `.` followed by a digit STARTS A NUMBER, so the tokens are
	// `a` and the float `.1`, ParsePostfix is never offered a Dot, the expression `a` is complete,
	// and the float is left over as a trailing token. Pinned because the code you get is genuinely
	// surprising, and because it is the one place a swizzle typo reads as two expressions.
	TestEqualSensitive(TEXT("`a.1` lexes as `a` then `.1`, so it is DSH3211"), ExprErrorCode(TEXT("a.1")), FString(TEXT("DSH3211")));

	// An initializer list is only ever an initializer, and says so instead of "expected expression".
	TestEqualSensitive(TEXT("`{ 1, 2 }` is DSH2162"), ExprErrorCode(TEXT("{ 1, 2 }")), FString(TEXT("DSH2162")));
	TestEqualSensitive(TEXT("`f({1})` is DSH2162"), ExprErrorCode(TEXT("f({1})")), FString(TEXT("DSH2162")));

	// A COMPLETE expression with something after it loses the whole result instead of quietly
	// returning the part that parsed -- that silent truncation (`a%b` read as `a`) is the 1.x bug
	// this front end exists to kill. Only this entry point can raise DSH3211: a corpus fixture goes
	// through ParseDreamShaderLang, which never parses a bare expression, so nothing else covers it.
	TestEqualSensitive(TEXT("`a + b c` is DSH3211"), ExprErrorCode(TEXT("a + b c")), FString(TEXT("DSH3211")));
	TestEqualSensitive(TEXT("`x;` is DSH3211"), ExprErrorCode(TEXT("x;")), FString(TEXT("DSH3211")));
	TestEqualSensitive(TEXT("`1 2` is DSH3211"), ExprErrorCode(TEXT("1 2")), FString(TEXT("DSH3211")));

	return true;
}

// =================================================================================================
// Statements
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2StatementKindsTest,
	"DreamShader.Lang2.Statements.Kinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2StatementKindsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	const FParsedBody Parsed = ParseFunctionBody(
		TEXT("    ;\n")
		TEXT("    { }\n")
		TEXT("    x = 1;\n")
		TEXT("    float a = 2.0f;\n")
		TEXT("    if (x > 0) { discard; }\n")
		TEXT("    while (x > 0) { break; }\n")
		TEXT("    do { continue; } while (x < 4);\n")
		TEXT("    for (int i = 0; i < 4; ++i) { x += i; }\n")
		TEXT("    return;\n"));

	TestEqualSensitive(TEXT("every statement kind parses"), Parsed.FirstError, FString(TEXT("<none>")));
	if (!TestNotNull(TEXT("the function has a parsed body"), Parsed.Body))
	{
		return false;
	}

	const ENodeKind Expected[] =
	{
		ENodeKind::EmptyStmt,
		ENodeKind::BlockStmt,
		ENodeKind::ExprStmt,
		ENodeKind::VarDeclStmt,
		ENodeKind::IfStmt,
		ENodeKind::WhileStmt,
		ENodeKind::DoWhileStmt,
		ENodeKind::ForStmt,
		ENodeKind::ReturnStmt,
	};

	if (TestEqual(TEXT("statement count"), Parsed.Body->Statements.Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected))))
	{
		for (int32 Index = 0; Index < Parsed.Body->Statements.Num(); ++Index)
		{
			const FStmt* Statement = Parsed.Body->Statements[Index].Get();
			TestEqual(*FString::Printf(TEXT("statement %d kind"), Index),
				Statement ? static_cast<int32>(Statement->Kind) : -1,
				static_cast<int32>(Expected[Index]));
		}
	}

	// The loop bodies really do hold their jump statements.
	TestEqualSensitive(TEXT("while body"),
		SingleStmtShape(TEXT("while (x > 0) { break; }")),
		FString(TEXT("(while (> (id x) (lit int 0)) (block (break)))")));
	TestEqualSensitive(TEXT("do ... while"),
		SingleStmtShape(TEXT("do { continue; } while (x < 4);")),
		FString(TEXT("(do (block (continue)) (< (id x) (lit int 4)))")));
	TestEqualSensitive(TEXT("discard"), SingleStmtShape(TEXT("discard;")), FString(TEXT("(discard)")));
	TestEqualSensitive(TEXT("empty statement"), SingleStmtShape(TEXT(";")), FString(TEXT("(empty)")));
	TestEqualSensitive(TEXT("nested blocks"),
		SingleStmtShape(TEXT("{ { x = 1; } }")),
		FString(TEXT("(block (block (expr (= (id x) (lit int 1)))))")));

	// A `///` line inside a body is a comment: statements carry no doc block, and the lexer emits
	// the token anyway, so the statement parser has to drop it rather than fail on it.
	TestEqualSensitive(TEXT("/// before a statement"),
		SingleStmtShape(TEXT("    /// a note\n    x = 1;")),
		FString(TEXT("(expr (= (id x) (lit int 1)))")));
	TestEqualSensitive(TEXT("/// at the end of a body"),
		BodyShape(TEXT("    x = 1;\n    /// a trailing note")),
		FString(TEXT("(block (expr (= (id x) (lit int 1))))")));

	// `return` with and without a value.
	TestEqualSensitive(TEXT("bare return"), SingleStmtShape(TEXT("return;")), FString(TEXT("(return -)")));
	TestEqualSensitive(TEXT("return with value"),
		SingleStmtShape(TEXT("return a * 2;")),
		FString(TEXT("(return (* (id a) (lit int 2)))")));

	// A `do` body does not have to be a block.
	TestEqualSensitive(TEXT("do with a single statement"),
		SingleStmtShape(TEXT("do x++; while (x < 4);")),
		FString(TEXT("(do (expr (post++ (id x))) (< (id x) (lit int 4)))")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2StatementIfElseTest,
	"DreamShader.Lang2.Statements.IfElse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2StatementIfElseTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// `else if` is an `if` in the else slot: the chain nests, it is not a list.
	TestEqualSensitive(TEXT("if / else if / else"),
		SingleStmtShape(
			TEXT("    if (a) { x = 1; }\n")
			TEXT("    else if (b) { x = 2; }\n")
			TEXT("    else { x = 3; }")),
		FString(TEXT("(if (id a) (block (expr (= (id x) (lit int 1)))) ")
			TEXT("(if (id b) (block (expr (= (id x) (lit int 2)))) ")
			TEXT("(block (expr (= (id x) (lit int 3))))))")));

	// No else at all leaves the slot null.
	TestEqualSensitive(TEXT("if without else"),
		SingleStmtShape(TEXT("if (a) x = 1;")),
		FString(TEXT("(if (id a) (expr (= (id x) (lit int 1))) -)")));

	// A dangling `else` binds to the nearest `if`.
	TestEqualSensitive(TEXT("dangling else"),
		SingleStmtShape(TEXT("if (a) if (b) x = 1; else x = 2;")),
		FString(TEXT("(if (id a) (if (id b) (expr (= (id x) (lit int 1))) (expr (= (id x) (lit int 2)))) -)")));

	// Three levels deep, so an `else if` chain cannot silently flatten.
	{
		const FParsedBody Parsed = ParseFunctionBody(
			TEXT("    if (a) x = 1;\n")
			TEXT("    else if (b) x = 2;\n")
			TEXT("    else if (c) x = 3;\n"));

		TestEqualSensitive(TEXT("chain parses"), Parsed.FirstError, FString(TEXT("<none>")));
		const FIfStmt* Outer = (Parsed.Body && Parsed.Body->Statements.Num() == 1)
			? Parsed.Body->Statements[0]->As<FIfStmt>()
			: nullptr;
		if (TestNotNull(TEXT("outer if"), Outer))
		{
			const FIfStmt* Middle = Outer->Else.IsValid() ? Outer->Else->As<FIfStmt>() : nullptr;
			if (TestNotNull(TEXT("middle if lives in the else slot"), Middle))
			{
				const FIfStmt* Inner = Middle->Else.IsValid() ? Middle->Else->As<FIfStmt>() : nullptr;
				if (TestNotNull(TEXT("inner if lives in the middle else slot"), Inner))
				{
					TestFalse(TEXT("the last if has no else"), Inner->Else.IsValid());
				}
			}
		}
	}

	// The condition takes a whole expression.
	TestEqualSensitive(TEXT("compound condition"),
		SingleStmtShape(TEXT("if (a > 0 && b < 1) x = 1;")),
		FString(TEXT("(if (&& (> (id a) (lit int 0)) (< (id b) (lit int 1))) (expr (= (id x) (lit int 1))) -)")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2StatementForTest,
	"DreamShader.Lang2.Statements.For",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2StatementForTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// A declaration as the initializer -- the form every loop in the corpus is written in.
	TestEqualSensitive(TEXT("for with a declaration init"),
		SingleStmtShape(TEXT("for (int i = 0; i < 4; ++i) { x += i; }")),
		FString(TEXT("(for (var - int (d i = (lit int 0))) (< (id i) (lit int 4)) (pre++ (id i)) ")
			TEXT("(block (expr (+= (id x) (id i)))))")));

	// An expression as the initializer.
	TestEqualSensitive(TEXT("for with an expression init"),
		SingleStmtShape(TEXT("for (i = 0; i < 4; i++) { }")),
		FString(TEXT("(for (expr (= (id i) (lit int 0))) (< (id i) (lit int 4)) (post++ (id i)) (block))")));

	// All three clauses are optional.
	TestEqualSensitive(TEXT("for (;;)"),
		SingleStmtShape(TEXT("for (;;) { break; }")),
		FString(TEXT("(for - - - (block (break)))")));
	TestEqualSensitive(TEXT("for with no condition"),
		SingleStmtShape(TEXT("for (int i = 0; ; ++i) { break; }")),
		FString(TEXT("(for (var - int (d i = (lit int 0))) - (pre++ (id i)) (block (break)))")));

	// Several names in one initializer.
	TestEqualSensitive(TEXT("for with two declarators"),
		SingleStmtShape(TEXT("for (int i = 0, j = 1; i < j; ++i) { }")),
		FString(TEXT("(for (var - int (d i = (lit int 0)) (d j = (lit int 1))) (< (id i) (id j)) (pre++ (id i)) (block))")));

	// The body does not have to be a block.
	TestEqualSensitive(TEXT("for with a single statement body"),
		SingleStmtShape(TEXT("for (int i = 0; i < 4; ++i) x += i;")),
		FString(TEXT("(for (var - int (d i = (lit int 0))) (< (id i) (lit int 4)) (pre++ (id i)) (expr (+= (id x) (id i))))")));

	// The Init slot really holds a FVarDeclStmt, not an expression statement wrapping a call.
	{
		const FParsedBody Parsed = ParseFunctionBody(TEXT("for (int i = 0; i < 4; ++i) { }"));
		const FForStmt* For = (Parsed.Body && Parsed.Body->Statements.Num() == 1)
			? Parsed.Body->Statements[0]->As<FForStmt>()
			: nullptr;
		if (TestNotNull(TEXT("for statement"), For))
		{
			const FVarDeclStmt* Init = For->Init.IsValid() ? For->Init->As<FVarDeclStmt>() : nullptr;
			if (TestNotNull(TEXT("for init is a declaration"), Init))
			{
				TestEqualSensitive(TEXT("init type"), Init->Type.Name, FString(TEXT("int")));
				if (TestEqual(TEXT("init declarator count"), Init->Declarators.Num(), 1))
				{
					TestTrue(TEXT("init name"), Init->Declarators[0].Name.Equals(TEXT("i"), ESearchCase::CaseSensitive));
				}
			}
		}
	}

	// A missing `;` in the header is DSH2154.
	{
		const FParsedBody Parsed = ParseFunctionBody(TEXT("for (int i = 0 i < 4; ++i) { }"));
		TestEqualSensitive(TEXT("for header without ';' is DSH2154"), Parsed.FirstError, FString(TEXT("DSH2154")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2StatementLocalDeclarationTest,
	"DreamShader.Lang2.Statements.LocalDeclarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2StatementLocalDeclarationTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// One declaration, several declarators, mixed initializers and array dimensions.
	TestEqualSensitive(TEXT("several declarators"),
		SingleStmtShape(TEXT("float3 a = float3(0, 0, 0), b, c[2];")),
		FString(TEXT("(var - float3 (d a = (call (type float3) (lit int 0) (lit int 0) (lit int 0))) (d b) (d c [(lit int 2)]))")));

	// Storage classes.
	TestEqualSensitive(TEXT("plain local"),
		SingleStmtShape(TEXT("float K = 1.0f;")),
		FString(TEXT("(var - float (d K = (lit float 1.0f)))")));
	TestEqualSensitive(TEXT("const local"),
		SingleStmtShape(TEXT("const float K = 1.0f;")),
		FString(TEXT("(var const float (d K = (lit float 1.0f)))")));
	TestEqualSensitive(TEXT("static local"),
		SingleStmtShape(TEXT("static float K = 1.0f;")),
		FString(TEXT("(var static float (d K = (lit float 1.0f)))")));
	TestEqualSensitive(TEXT("static const local"),
		SingleStmtShape(TEXT("static const float K = 1.0f;")),
		FString(TEXT("(var static-const float (d K = (lit float 1.0f)))")));

	// Initializer lists, flat and nested -- the only place a `{` is an expression.
	TestEqualSensitive(TEXT("initializer list"),
		SingleStmtShape(TEXT("float2 P = { 1.0f, 2.0f };")),
		FString(TEXT("(var - float2 (d P = (init (lit float 1.0f) (lit float 2.0f))))")));
	TestEqualSensitive(TEXT("nested initializer lists"),
		SingleStmtShape(TEXT("float2 M[2] = { { 1, 2 }, { 3, 4 } };")),
		FString(TEXT("(var - float2 (d M [(lit int 2)] = (init (init (lit int 1) (lit int 2)) (init (lit int 3) (lit int 4)))))")));
	TestEqualSensitive(TEXT("trailing comma in an initializer list"),
		SingleStmtShape(TEXT("float2 P = { 1.0f, 2.0f, };")),
		FString(TEXT("(var - float2 (d P = (init (lit float 1.0f) (lit float 2.0f))))")));

	// A user type is a declaration too -- `MyStruct s;` is not a call.
	TestEqualSensitive(TEXT("user type local"),
		SingleStmtShape(TEXT("MyStruct s;")),
		FString(TEXT("(var - MyStruct (d s))")));

	// ... while `Foo(x);` stays a call.
	TestEqualSensitive(TEXT("call statement"),
		SingleStmtShape(TEXT("Foo(x);")),
		FString(TEXT("(expr (call (id Foo) (id x)))")));

	// A declaration without its `;` is DSH2154; a declaration without a name is DSH2163.
	{
		const FParsedBody Parsed = ParseFunctionBody(TEXT("float a = 1.0f"));
		TestEqualSensitive(TEXT("declaration without ';' is DSH2154"), Parsed.FirstError, FString(TEXT("DSH2154")));
	}
	{
		const FParsedBody Parsed = ParseFunctionBody(TEXT("float3 a, = 1.0f;"));
		TestEqualSensitive(TEXT("declarator without a name is DSH2163"), Parsed.FirstError, FString(TEXT("DSH2163")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2StatementUnsupportedTest,
	"DreamShader.Lang2.Statements.Unsupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2StatementUnsupportedTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// `switch` is not a keyword, so without this it would parse as a call to a function called
	// `switch` and fail somewhere else entirely. DSH2160 names it.
	{
		const FParsedBody Parsed = ParseFunctionBody(
			TEXT("    switch (x)\n")
			TEXT("    {\n")
			TEXT("        case 1: break;\n")
			TEXT("    }\n"));

		TestEqualSensitive(TEXT("switch is DSH2160"), Parsed.FirstError, FString(TEXT("DSH2160")));
		TestFalse(TEXT("a file with a switch does not succeed"), Parsed.Result.Succeeded());

		// Recovery still hands back the function, so a language service keeps working on the file.
		TestEqual(TEXT("the function is still in the module"), Parsed.NumDeclarations, 1);
		TestNotNull(TEXT("the function still has a body"), Parsed.Body);
	}

	// A bare `case` / `default` says the same thing rather than "expected an expression".
	{
		const FParsedBody Parsed = ParseFunctionBody(TEXT("case 1: break;"));
		TestEqualSensitive(TEXT("case is DSH2160"), Parsed.FirstError, FString(TEXT("DSH2160")));
	}

	// A `#` line inside a body: the preprocessor should have eaten it, and a body that wants HLSL
	// has to say `/// @custom`.
	{
		const FParsedBody Parsed = ParseFunctionBody(
			TEXT("#define X 1\n")
			TEXT("    return;\n"));

		TestEqualSensitive(TEXT("directive in a body is DSH2160"), Parsed.FirstError, FString(TEXT("DSH2160")));
		TestEqual(TEXT("one error, not a cascade"), Parsed.NumErrors, 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2StatementRecoveryTest,
	"DreamShader.Lang2.Statements.Recovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2StatementRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2Tests;

	// A missing `;` costs the statement it broke (and the run of tokens up to the next `;`), and
	// nothing after that: the block keeps parsing.
	{
		const FParsedBody Parsed = ParseFunctionBody(
			TEXT("    a = 1\n")
			TEXT("    b = 2;\n")
			TEXT("    c = 3;\n"));

		TestEqualSensitive(TEXT("missing ';' is DSH2154"), Parsed.FirstError, FString(TEXT("DSH2154")));
		TestEqual(TEXT("one error, not one per following line"), Parsed.NumErrors, 1);
		if (TestNotNull(TEXT("the body survived"), Parsed.Body))
		{
			if (TestEqual(TEXT("only the broken statement was lost"), Parsed.Body->Statements.Num(), 1))
			{
				TestEqualSensitive(TEXT("the statement after the break parsed"),
					DescribeStmt(Parsed.Body->Statements[0].Get()),
					FString(TEXT("(expr (= (id c) (lit int 3)))")));
			}
		}
	}

	// A broken statement does not eat the rest of the file either.
	{
		const FLangSourceText Source(TEXT("Recover.dss"), FString(
			TEXT("void f()\n{\n    a = 1\n}\n")
			TEXT("void g()\n{\n    b = 2;\n}\n")));
		const FLangParseResult Result = ParseDreamShaderLang(Source);

		TestTrue(TEXT("the module survived"), Result.Module.IsValid());
		if (Result.Module.IsValid())
		{
			TestEqual(TEXT("both functions are in the module"), Result.Module->CountDecls(ENodeKind::FunctionDecl), 2);
		}
		TestEqual(TEXT("one error"), Result.Diagnostics.NumErrors(), 1);
		TestEqualSensitive(TEXT("the error is the missing ';'"),
			FirstErrorCode(Result.Diagnostics), FString(TEXT("DSH2154")));
	}

	// An error inside a nested block stays inside that block.
	{
		const FParsedBody Parsed = ParseFunctionBody(
			TEXT("    if (a)\n")
			TEXT("    {\n")
			TEXT("        b = ;\n")
			TEXT("    }\n")
			TEXT("    c = 3;\n"));

		TestEqualSensitive(TEXT("a broken inner statement is DSH2151"), Parsed.FirstError, FString(TEXT("DSH2151")));
		TestEqual(TEXT("one error"), Parsed.NumErrors, 1);
		if (TestNotNull(TEXT("the body survived"), Parsed.Body))
		{
			TestEqual(TEXT("the if and the statement after it both survived"), Parsed.Body->Statements.Num(), 2);
		}
	}

	// An unterminated block reports the end of file exactly once.
	{
		const FLangSourceText Source(TEXT("Unterminated.dss"), FString(TEXT("void f()\n{\n    a = 1;\n")));
		const FLangParseResult Result = ParseDreamShaderLang(Source);

		TestTrue(TEXT("the module is still returned"), Result.Module.IsValid());
		TestEqualSensitive(TEXT("an unterminated block is DSH2150"),
			FirstErrorCode(Result.Diagnostics), FString(TEXT("DSH2150")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
