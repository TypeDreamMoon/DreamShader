// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The expression half of the 2.0 parser: precedence climbing over LangOperators.h, plus the two
// places DreamShaderLang differs from C -- named call arguments (`UE.TexCoord(Index = 0)`, which is
// how a 1.x Graph line survives verbatim) and initializer lists, which are reachable only where a
// declarator asks for one.
//
// Every method here follows the LangParserInternal.h convention: null (or false) is returned only
// after the failure has been reported exactly once, and the caller decides whether to recover.
//
// Diagnostics owned by this unit: DSH2151..DSH2153, DSH2155, DSH2158, DSH2159, DSH2161, DSH2162.
// DSH2152 is shared with the statement unit (both spell "expected ')'"); DSH2150 is raised for us
// by FailAtEnd.

#include "LangOperators.h"
#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.Expressions"

namespace UE::DreamShader::Lang::Private
{
	/**
	 * Could this token begin a unary expression? Used only by the cast lookahead -- `(Foo)` is a
	 * cast when something that can be an operand follows it, and a parenthesised expression when
	 * nothing can.
	 */
	static bool LangExprCanStartUnaryExpression(const FLangToken& Token)
	{
		switch (Token.Kind)
		{
		case ELangTokenKind::Keyword:
			// Only the two literals; `if`, `return`, ... start statements, not operands.
			return Token.Keyword == ELangKeyword::True || Token.Keyword == ELangKeyword::False;

		case ELangTokenKind::Identifier:
		case ELangTokenKind::IntLiteral:
		case ELangTokenKind::FloatLiteral:
		case ELangTokenKind::StringLiteral:
		case ELangTokenKind::LeftParen:
		case ELangTokenKind::Plus:
		case ELangTokenKind::Minus:
		case ELangTokenKind::Bang:
		case ELangTokenKind::Tilde:
		case ELangTokenKind::PlusPlus:
		case ELangTokenKind::MinusMinus:
			return true;

		default:
			return false;
		}
	}

	/**
	 * The same test, minus the four tokens that read as a binary or postfix operator just as well
	 * as they read as the start of an operand.
	 *
	 * `(float3) - x` can only be a cast, because `float3` is not a value. `(a) - x` almost always
	 * is a subtraction, because `a` is almost always a variable -- the parser cannot know it is a
	 * struct name without the symbol table it deliberately does not have. So a cast to a user type
	 * is recognised in front of an identifier, a literal, `(`, `!` and `~`, and a cast written in
	 * front of `+ - ++ --` has to be spelled with the operand parenthesised: `(MyStruct)(-x)`.
	 */
	static bool LangExprCanStartUnambiguousCastOperand(const FLangToken& Token)
	{
		switch (Token.Kind)
		{
		case ELangTokenKind::Plus:
		case ELangTokenKind::Minus:
		case ELangTokenKind::PlusPlus:
		case ELangTokenKind::MinusMinus:
			return false;

		default:
			return LangExprCanStartUnaryExpression(Token);
		}
	}

	FExprPtr FLangParser::ParseExpression()
	{
		return ParseAssignment();
	}

	FExprPtr FLangParser::ParseAssignment()
	{
		const int32 StartIndex = GetTokenIndex();

		FExprPtr Target = ParseConditional();
		if (!Target)
		{
			return nullptr;
		}

		EAssignOp Op = EAssignOp::Assign;
		if (!TryGetAssignOp(Current().Kind, Op))
		{
			return Target;
		}

		Advance();

		// Right-associative: `a = b = c` is `a = (b = c)`. Any expression is accepted as a target
		// here; whether it is assignable is a question for the semantic pass, not for the grammar.
		FExprPtr Value = ParseAssignment();
		if (!Value)
		{
			return nullptr;
		}

		TUniquePtr<FAssignExpr> Node = MakeUnique<FAssignExpr>();
		Node->Op = Op;
		Node->Target = MoveTemp(Target);
		Node->Value = MoveTemp(Value);
		Node->Span = SpanFrom(StartIndex);
		return MoveTemp(Node);
	}

	FExprPtr FLangParser::ParseConditional()
	{
		const int32 StartIndex = GetTokenIndex();

		FExprPtr Condition = ParseBinary(1);
		if (!Condition)
		{
			return nullptr;
		}

		if (!Check(ELangTokenKind::Question))
		{
			return Condition;
		}

		Advance();

		// The middle operand sits between `?` and `:`, so it can be any expression at all.
		FExprPtr TrueValue = ParseExpression();
		if (!TrueValue)
		{
			return nullptr;
		}

		if (!Check(ELangTokenKind::Colon))
		{
			Diagnostics.Error(TEXT("DSH2159"), Current().Span, FText::Format(
				LOCTEXT("ExpectedColonInConditional", "Expected ':' to complete the conditional operator, found {0}."),
				DescribeToken(Current())));
			return nullptr;
		}

		Advance();

		// Right-associative: `a ? b : c ? d : e` is `a ? b : (c ? d : e)`.
		FExprPtr FalseValue = ParseConditional();
		if (!FalseValue)
		{
			return nullptr;
		}

		TUniquePtr<FConditionalExpr> Node = MakeUnique<FConditionalExpr>();
		Node->Condition = MoveTemp(Condition);
		Node->TrueValue = MoveTemp(TrueValue);
		Node->FalseValue = MoveTemp(FalseValue);
		Node->Span = SpanFrom(StartIndex);
		return MoveTemp(Node);
	}

	FExprPtr FLangParser::ParseBinary(int32 MinPrecedence)
	{
		const int32 StartIndex = GetTokenIndex();

		FExprPtr Left = ParseUnary();
		if (!Left)
		{
			return nullptr;
		}

		for (;;)
		{
			EBinaryOp Op = EBinaryOp::Add;
			if (!TryGetBinaryOp(Current().Kind, Op))
			{
				break;
			}

			const int32 Precedence = GetBinaryPrecedence(Op);
			if (Precedence < MinPrecedence)
			{
				break;
			}

			Advance();

			// Precedence + 1 on the right: every binary operator is left-associative, so an
			// operator of equal precedence ends the right operand instead of joining it.
			FExprPtr Right = ParseBinary(Precedence + 1);
			if (!Right)
			{
				return nullptr;
			}

			TUniquePtr<FBinaryExpr> Node = MakeUnique<FBinaryExpr>();
			Node->Op = Op;
			Node->Left = MoveTemp(Left);
			Node->Right = MoveTemp(Right);
			Node->Span = SpanFrom(StartIndex);
			Left = MoveTemp(Node);
		}

		return Left;
	}

	FExprPtr FLangParser::ParseUnary()
	{
		const int32 StartIndex = GetTokenIndex();

		EUnaryOp UnaryOp = EUnaryOp::Plus;
		if (TryGetPrefixUnaryOp(Current().Kind, UnaryOp))
		{
			Advance();

			FExprPtr Operand = ParseUnary();
			if (!Operand)
			{
				return nullptr;
			}

			TUniquePtr<FUnaryExpr> Node = MakeUnique<FUnaryExpr>();
			Node->Op = UnaryOp;
			Node->Operand = MoveTemp(Operand);
			Node->Span = SpanFrom(StartIndex);
			return MoveTemp(Node);
		}

		if (IsCastStart())
		{
			Advance(); // '('

			TUniquePtr<FCastExpr> Node = MakeUnique<FCastExpr>();
			if (!ParseType(Node->Type))
			{
				return nullptr;
			}

			if (!Expect(ELangTokenKind::RightParen, TEXT("DSH2152"), LOCTEXT("CastRightParen", "')' to close a cast")))
			{
				return nullptr;
			}

			FExprPtr Operand = ParseUnary();
			if (!Operand)
			{
				return nullptr;
			}

			Node->Operand = MoveTemp(Operand);
			Node->Span = SpanFrom(StartIndex);
			return MoveTemp(Node);
		}

		FExprPtr Primary = ParsePrimary();
		if (!Primary)
		{
			return nullptr;
		}

		return ParsePostfix(MoveTemp(Primary));
	}

	FExprPtr FLangParser::ParsePostfix(FExprPtr Primary)
	{
		FExprPtr Result = MoveTemp(Primary);
		if (!Result)
		{
			return nullptr;
		}

		for (;;)
		{
			if (Check(ELangTokenKind::Dot))
			{
				const FLangSpan ObjectSpan = Result->Span;
				Advance(); // '.'

				if (!Check(ELangTokenKind::Identifier))
				{
					Diagnostics.Error(TEXT("DSH2161"), Current().Span, FText::Format(
						LOCTEXT("ExpectedMemberName", "Expected a member or swizzle name after '.', found {0}."),
						DescribeToken(Current())));
					return nullptr;
				}

				const FLangToken& MemberToken = Advance();
				const FString MemberName = MemberToken.Text;
				const FLangSpan MemberSpan = MemberToken.Span;

				TUniquePtr<FMemberExpr> Node = MakeUnique<FMemberExpr>();
				Node->Member = MemberName;
				Node->MemberSpan = MemberSpan;
				Node->Object = MoveTemp(Result);
				Node->Span = FLangSpan::Join(ObjectSpan, MemberSpan);
				Result = MoveTemp(Node);
				continue;
			}

			if (Check(ELangTokenKind::LeftBracket))
			{
				const FLangSpan ObjectSpan = Result->Span;
				Advance(); // '['

				FExprPtr IndexExpr = ParseExpression();
				if (!IndexExpr)
				{
					return nullptr;
				}

				if (!Expect(ELangTokenKind::RightBracket, TEXT("DSH2153"), LOCTEXT("IndexRightBracket", "']' to close an index")))
				{
					return nullptr;
				}

				TUniquePtr<FIndexExpr> Node = MakeUnique<FIndexExpr>();
				Node->Object = MoveTemp(Result);
				Node->Index = MoveTemp(IndexExpr);
				Node->Span = FLangSpan::Join(ObjectSpan, Previous().Span);
				Result = MoveTemp(Node);
				continue;
			}

			if (Check(ELangTokenKind::LeftParen))
			{
				const FLangSpan CalleeSpan = Result->Span;
				Advance(); // '('

				TUniquePtr<FCallExpr> Node = MakeUnique<FCallExpr>();
				if (!ParseArguments(Node->Arguments))
				{
					return nullptr;
				}

				Node->Callee = MoveTemp(Result);
				Node->Span = FLangSpan::Join(CalleeSpan, Previous().Span);
				Result = MoveTemp(Node);
				continue;
			}

			if (Check(ELangTokenKind::PlusPlus) || Check(ELangTokenKind::MinusMinus))
			{
				const FLangSpan OperandSpan = Result->Span;
				const bool bIncrement = Check(ELangTokenKind::PlusPlus);
				const FLangSpan OpSpan = Advance().Span;

				TUniquePtr<FUnaryExpr> Node = MakeUnique<FUnaryExpr>();
				Node->Op = bIncrement ? EUnaryOp::PostIncrement : EUnaryOp::PostDecrement;
				Node->Operand = MoveTemp(Result);
				Node->Span = FLangSpan::Join(OperandSpan, OpSpan);
				Result = MoveTemp(Node);
				continue;
			}

			break;
		}

		return Result;
	}

	FExprPtr FLangParser::ParsePrimary()
	{
		const int32 StartIndex = GetTokenIndex();
		const FLangToken& Token = Current();

		switch (Token.Kind)
		{
		case ELangTokenKind::EndOfFile:
			FailAtEnd(LOCTEXT("WhileParsingExpression", "an expression"));
			return nullptr;

		case ELangTokenKind::IntLiteral:
		{
			TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
			Node->LiteralKind = Token.bUnsigned ? ELiteralKind::UInt : ELiteralKind::Int;
			// The lexeme, not a re-rendering of the value: `0x10` and `2u` must print back as written.
			Node->Text = Token.Text;
			Node->Integer = Token.Integer;
			Node->Real = Token.Real;
			Node->Span = Token.Span;
			Advance();
			return MoveTemp(Node);
		}

		case ELangTokenKind::FloatLiteral:
		{
			TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
			Node->LiteralKind = ELiteralKind::Float;
			Node->Text = Token.Text;
			Node->Integer = Token.Integer;
			Node->Real = Token.Real;
			Node->Span = Token.Span;
			Advance();
			return MoveTemp(Node);
		}

		case ELangTokenKind::StringLiteral:
		{
			TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
			Node->LiteralKind = ELiteralKind::String;
			// Escapes are already resolved by the lexer; the printer re-escapes on the way out.
			Node->Text = Token.Text;
			Node->Span = Token.Span;
			Advance();
			return MoveTemp(Node);
		}

		case ELangTokenKind::Keyword:
			if (Token.Keyword == ELangKeyword::True || Token.Keyword == ELangKeyword::False)
			{
				TUniquePtr<FLiteralExpr> Node = MakeUnique<FLiteralExpr>();
				Node->LiteralKind = ELiteralKind::Bool;
				Node->bBool = (Token.Keyword == ELangKeyword::True);
				Node->Text = Token.Text;
				Node->Span = Token.Span;
				Advance();
				return MoveTemp(Node);
			}
			break;

		case ELangTokenKind::Identifier:
			if (IsBuiltinTypeName(Token.Text))
			{
				// `float3(1, 2, 3)` is a call on a type, and a bare `float3` stays a type node --
				// classification is the same one declarations get, so nothing downstream has to
				// guess whether a name was a type.
				TUniquePtr<FTypeExpr> Node = MakeUnique<FTypeExpr>();
				if (!ParseType(Node->Type))
				{
					return nullptr;
				}
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}
			else
			{
				TUniquePtr<FIdentifierExpr> Node = MakeUnique<FIdentifierExpr>();
				Node->Name = Token.Text;
				Node->Span = Token.Span;
				Advance();
				return MoveTemp(Node);
			}

		case ELangTokenKind::LeftParen:
		{
			Advance(); // '('

			FExprPtr Inner = ParseExpression();
			if (!Inner)
			{
				return nullptr;
			}

			if (!Expect(ELangTokenKind::RightParen, TEXT("DSH2152"), LOCTEXT("ParenRightParen", "')' to close a parenthesized expression")))
			{
				return nullptr;
			}

			TUniquePtr<FParenExpr> Node = MakeUnique<FParenExpr>();
			Node->Inner = MoveTemp(Inner);
			Node->Span = SpanFrom(StartIndex);
			return MoveTemp(Node);
		}

		case ELangTokenKind::LeftBrace:
			// Initializer lists are reached through ParseInitializerList, which only a declarator
			// calls; anywhere else a brace is a mistake, and saying so beats "expected expression".
			Diagnostics.Error(TEXT("DSH2162"), Token.Span, LOCTEXT("InitializerListNotAnExpression",
				"An initializer list is only allowed as a variable initializer, not as a general expression."));
			return nullptr;

		default:
			break;
		}

		Diagnostics.Error(TEXT("DSH2151"), Current().Span, FText::Format(
			LOCTEXT("ExpectedExpression", "Expected an expression, found {0}."),
			DescribeToken(Current())));
		return nullptr;
	}

	bool FLangParser::ParseArguments(TArray<FArgument>& OutArguments)
	{
		if (Match(ELangTokenKind::RightParen))
		{
			return true;
		}

		bool bSeenNamedArgument = false;
		for (;;)
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileParsingArgumentList", "an argument list"));
			}

			const int32 ArgumentStart = GetTokenIndex();

			FArgument Argument;
			// `Ident =` is a named argument. `==` is a single token, so nothing else can be
			// mistaken for one, and a real assignment as an argument is written `(a = b)`.
			if (Check(ELangTokenKind::Identifier) && Peek(1).Kind == ELangTokenKind::Assign)
			{
				const FLangToken& NameToken = Advance();
				Argument.Name = NameToken.Text;
				Argument.NameSpan = NameToken.Span;
				Advance(); // '='
				bSeenNamedArgument = true;
			}

			FExprPtr Value = ParseExpression();
			if (!Value)
			{
				return false;
			}

			if (Argument.Name.IsEmpty() && bSeenNamedArgument)
			{
				Diagnostics.Error(TEXT("DSH2158"), Value->Span, LOCTEXT("PositionalAfterNamedArgument",
					"A positional argument cannot follow a named argument; give this argument a name too."));
				return false;
			}

			Argument.Value = MoveTemp(Value);
			Argument.Span = SpanFrom(ArgumentStart);
			OutArguments.Add(MoveTemp(Argument));

			if (Match(ELangTokenKind::Comma))
			{
				continue;
			}

			break;
		}

		return Expect(ELangTokenKind::RightParen, TEXT("DSH2152"), LOCTEXT("ArgumentsRightParen", "')' to close an argument list"));
	}

	FExprPtr FLangParser::ParseInitializerList()
	{
		const int32 StartIndex = GetTokenIndex();

		if (!Check(ELangTokenKind::LeftBrace))
		{
			// Only a declarator calls this, and only after looking at the brace; the guard is here
			// so a future caller gets a diagnostic instead of a silently empty list.
			Diagnostics.Error(TEXT("DSH2151"), Current().Span, FText::Format(
				LOCTEXT("ExpectedInitializerList", "Expected an initializer list, found {0}."),
				DescribeToken(Current())));
			return nullptr;
		}

		Advance(); // '{'

		TUniquePtr<FInitializerListExpr> Node = MakeUnique<FInitializerListExpr>();
		if (!Check(ELangTokenKind::RightBrace))
		{
			for (;;)
			{
				if (AtEnd())
				{
					FailAtEnd(LOCTEXT("WhileParsingInitializerList", "an initializer list"));
					return nullptr;
				}

				// Nested lists: `float2 a[2] = { { 1, 2 }, { 3, 4 } };`
				FExprPtr Element = Check(ELangTokenKind::LeftBrace) ? ParseInitializerList() : ParseExpression();
				if (!Element)
				{
					return nullptr;
				}

				Node->Elements.Add(MoveTemp(Element));

				if (!Match(ELangTokenKind::Comma))
				{
					break;
				}

				// A trailing comma before the closing brace is allowed.
				if (Check(ELangTokenKind::RightBrace))
				{
					break;
				}
			}
		}

		if (!Expect(ELangTokenKind::RightBrace, TEXT("DSH2155"), LOCTEXT("InitializerListRightBrace", "'}' to close an initializer list")))
		{
			return nullptr;
		}

		Node->Span = SpanFrom(StartIndex);
		return MoveTemp(Node);
	}

	bool FLangParser::IsCastStart() const
	{
		if (!Check(ELangTokenKind::LeftParen))
		{
			return false;
		}

		// A type spelling is exactly one identifier (ParseType never consumes array dimensions),
		// so three tokens of lookahead decide this and no backtracking is needed.
		const FLangToken& TypeToken = Peek(1);
		if (TypeToken.Kind != ELangTokenKind::Identifier)
		{
			return false;
		}

		if (Peek(2).Kind != ELangTokenKind::RightParen)
		{
			return false;
		}

		const FLangToken& AfterToken = Peek(3);
		return IsBuiltinTypeName(TypeToken.Text)
			? LangExprCanStartUnaryExpression(AfterToken)
			: LangExprCanStartUnambiguousCastOperand(AfterToken);
	}
}

#undef LOCTEXT_NAMESPACE
