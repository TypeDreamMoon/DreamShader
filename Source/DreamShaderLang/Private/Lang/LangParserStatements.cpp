// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Statements and blocks. The set is deliberately small -- 2.0 lowers to a material graph, so every
// statement here has a graph meaning (see Plan/syntax-v2-proposal.md section 10): declarations,
// expressions, blocks, `if`/`else`, the three loops, `return`, `break`, `continue`, `discard` and
// the empty statement. `switch` is not in the language, and says so rather than reading as a call.
//
// Recovery lives in ParseBlock: a statement that failed has already reported, so the block skips to
// the next boundary and keeps parsing. One broken line therefore costs one statement, not the rest
// of the function -- which is the whole reason a language service can work on a file mid-edit.
//
// Diagnostics owned by this unit: DSH2152 (shared with the expression unit), DSH2154, DSH2157,
// DSH2160, DSH2163, DSH2164, DSH2165. DSH2150 is raised for us by FailAtEnd.

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.Statements"

namespace UE::DreamShader::Lang::Private
{
	/**
	 * `LooksLikeDeclarationStart` allows one leading storage keyword; a local may carry two
	 * (`static const float K = 1;`), and `const float3 P = ...;` is the ordinary spelling of a
	 * local constant. Either keyword at statement start settles it without further lookahead --
	 * neither can begin an expression.
	 */
	static bool LangStmtLooksLikeLocalDeclaration(const FLangParser& Parser)
	{
		return Parser.CheckKeyword(ELangKeyword::Static)
			|| Parser.CheckKeyword(ELangKeyword::Const)
			|| Parser.LooksLikeDeclarationStart();
	}

	FStmtPtr FLangParser::ParseStatement()
	{
		// The lexer emits every `///` line, wherever it is. Above a declaration one forms a doc
		// block; inside a body it documents nothing in M1, so it is a comment like any other and
		// must not turn into "expected an expression, found a '///' comment". Skipped before the
		// span starts, so the statement's span covers the statement and not the comment above it.
		while (Match(ELangTokenKind::DocComment))
		{
		}

		const int32 StartIndex = GetTokenIndex();

		if (AtEnd())
		{
			FailAtEnd(LOCTEXT("WhileParsingStatement", "a statement"));
			return nullptr;
		}

		if (Check(ELangTokenKind::LeftBrace))
		{
			TUniquePtr<FBlockStmt> Block = ParseBlock();
			if (!Block)
			{
				return nullptr;
			}
			return MoveTemp(Block);
		}

		if (Check(ELangTokenKind::Semicolon))
		{
			Advance();

			TUniquePtr<FEmptyStmt> Node = MakeUnique<FEmptyStmt>();
			Node->Span = SpanFrom(StartIndex);
			return MoveTemp(Node);
		}

		if (Check(ELangTokenKind::Directive))
		{
			// A `#` line inside a body is either a preprocessor line the preprocessor should have
			// eaten, or an attempt to write HLSL that only `/// @custom` bodies may contain.
			const FLangSpan DirectiveSpan = Current().Span;
			const FString DirectiveText = Current().Text;
			Advance();

			Diagnostics.Error(TEXT("DSH2160"), DirectiveSpan, FText::Format(
				LOCTEXT("DirectiveInsideBody", "Unsupported statement: the preprocessor line '#{0}' cannot appear inside a function body; mark the function /// @custom to hand its body to the shader compiler."),
				FText::FromString(DirectiveText)));
			return nullptr;
		}

		if (Current().Kind == ELangTokenKind::Keyword)
		{
			switch (Current().Keyword)
			{
			case ELangKeyword::If:
			{
				Advance();

				if (!Expect(ELangTokenKind::LeftParen, TEXT("DSH2157"), LOCTEXT("IfLeftParen", "'(' after 'if'")))
				{
					return nullptr;
				}

				FExprPtr Condition = ParseExpression();
				if (!Condition)
				{
					return nullptr;
				}

				if (!Expect(ELangTokenKind::RightParen, TEXT("DSH2152"), LOCTEXT("IfRightParen", "')' to close the 'if' condition")))
				{
					return nullptr;
				}

				FStmtPtr Then = ParseStatement();
				if (!Then)
				{
					return nullptr;
				}

				// `else if` is just an `if` statement in the else slot; the chain nests.
				FStmtPtr Else;
				if (MatchKeyword(ELangKeyword::Else))
				{
					Else = ParseStatement();
					if (!Else)
					{
						return nullptr;
					}
				}

				TUniquePtr<FIfStmt> Node = MakeUnique<FIfStmt>();
				Node->Condition = MoveTemp(Condition);
				Node->Then = MoveTemp(Then);
				Node->Else = MoveTemp(Else);
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			case ELangKeyword::For:
			{
				Advance();

				if (!Expect(ELangTokenKind::LeftParen, TEXT("DSH2157"), LOCTEXT("ForLeftParen", "'(' after 'for'")))
				{
					return nullptr;
				}

				FStmtPtr Init;
				if (Check(ELangTokenKind::Semicolon))
				{
					Advance();
				}
				else if (LangStmtLooksLikeLocalDeclaration(*this))
				{
					// ParseLocalVarDecl consumes the `;` that ends the initializer clause.
					TUniquePtr<FVarDeclStmt> InitDecl = ParseLocalVarDecl();
					if (!InitDecl)
					{
						return nullptr;
					}
					Init = MoveTemp(InitDecl);
				}
				else
				{
					const int32 InitStart = GetTokenIndex();

					FExprPtr InitExpr = ParseExpression();
					if (!InitExpr)
					{
						return nullptr;
					}

					if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("ForInitSemicolon", "';' after the 'for' initializer")))
					{
						return nullptr;
					}

					TUniquePtr<FExprStmt> InitStmt = MakeUnique<FExprStmt>();
					InitStmt->Expression = MoveTemp(InitExpr);
					InitStmt->Span = SpanFrom(InitStart);
					Init = MoveTemp(InitStmt);
				}

				FExprPtr Condition;
				if (!Check(ELangTokenKind::Semicolon))
				{
					Condition = ParseExpression();
					if (!Condition)
					{
						return nullptr;
					}
				}

				if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("ForConditionSemicolon", "';' after the 'for' condition")))
				{
					return nullptr;
				}

				FExprPtr Step;
				if (!Check(ELangTokenKind::RightParen))
				{
					Step = ParseExpression();
					if (!Step)
					{
						return nullptr;
					}
				}

				if (!Expect(ELangTokenKind::RightParen, TEXT("DSH2152"), LOCTEXT("ForRightParen", "')' to close the 'for' header")))
				{
					return nullptr;
				}

				FStmtPtr Body = ParseStatement();
				if (!Body)
				{
					return nullptr;
				}

				TUniquePtr<FForStmt> Node = MakeUnique<FForStmt>();
				Node->Init = MoveTemp(Init);
				Node->Condition = MoveTemp(Condition);
				Node->Step = MoveTemp(Step);
				Node->Body = MoveTemp(Body);
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			case ELangKeyword::While:
			{
				Advance();

				if (!Expect(ELangTokenKind::LeftParen, TEXT("DSH2157"), LOCTEXT("WhileLeftParen", "'(' after 'while'")))
				{
					return nullptr;
				}

				FExprPtr Condition = ParseExpression();
				if (!Condition)
				{
					return nullptr;
				}

				if (!Expect(ELangTokenKind::RightParen, TEXT("DSH2152"), LOCTEXT("WhileRightParen", "')' to close the 'while' condition")))
				{
					return nullptr;
				}

				FStmtPtr Body = ParseStatement();
				if (!Body)
				{
					return nullptr;
				}

				TUniquePtr<FWhileStmt> Node = MakeUnique<FWhileStmt>();
				Node->Condition = MoveTemp(Condition);
				Node->Body = MoveTemp(Body);
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			case ELangKeyword::Do:
			{
				Advance();

				FStmtPtr Body = ParseStatement();
				if (!Body)
				{
					return nullptr;
				}

				if (!MatchKeyword(ELangKeyword::While))
				{
					Diagnostics.Error(TEXT("DSH2164"), Current().Span, FText::Format(
						LOCTEXT("ExpectedWhileAfterDo", "Expected 'while' after the body of a 'do' statement, found {0}."),
						DescribeToken(Current())));
					return nullptr;
				}

				if (!Expect(ELangTokenKind::LeftParen, TEXT("DSH2157"), LOCTEXT("DoWhileLeftParen", "'(' after 'while'")))
				{
					return nullptr;
				}

				FExprPtr Condition = ParseExpression();
				if (!Condition)
				{
					return nullptr;
				}

				if (!Expect(ELangTokenKind::RightParen, TEXT("DSH2152"), LOCTEXT("DoWhileRightParen", "')' to close the 'while' condition")))
				{
					return nullptr;
				}

				if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("DoWhileSemicolon", "';' after a 'do ... while' statement")))
				{
					return nullptr;
				}

				TUniquePtr<FDoWhileStmt> Node = MakeUnique<FDoWhileStmt>();
				Node->Body = MoveTemp(Body);
				Node->Condition = MoveTemp(Condition);
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			case ELangKeyword::Return:
			{
				Advance();

				FExprPtr Value;
				if (!Check(ELangTokenKind::Semicolon))
				{
					Value = ParseExpression();
					if (!Value)
					{
						return nullptr;
					}
				}

				if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("ReturnSemicolon", "';' after a 'return' statement")))
				{
					return nullptr;
				}

				TUniquePtr<FReturnStmt> Node = MakeUnique<FReturnStmt>();
				Node->Value = MoveTemp(Value);
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			case ELangKeyword::Break:
			{
				Advance();

				if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("BreakSemicolon", "';' after 'break'")))
				{
					return nullptr;
				}

				TUniquePtr<FBreakStmt> Node = MakeUnique<FBreakStmt>();
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			case ELangKeyword::Continue:
			{
				Advance();

				if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("ContinueSemicolon", "';' after 'continue'")))
				{
					return nullptr;
				}

				TUniquePtr<FContinueStmt> Node = MakeUnique<FContinueStmt>();
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			case ELangKeyword::Discard:
			{
				Advance();

				if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("DiscardSemicolon", "';' after 'discard'")))
				{
					return nullptr;
				}

				TUniquePtr<FDiscardStmt> Node = MakeUnique<FDiscardStmt>();
				Node->Span = SpanFrom(StartIndex);
				return MoveTemp(Node);
			}

			default:
				// `static` / `const` fall through to the declaration test below; anything else
				// (`struct`, `uniform`, a stray `else`) is not a statement and ends up as an
				// expression statement, where ParsePrimary names the token it could not use.
				break;
			}
		}

		// `switch` is not a keyword of this language, so it arrives as an identifier and would
		// otherwise parse as a call to a function named `switch` -- silently, which is exactly the
		// class of 1.x bug 2.0 exists to remove.
		if (CheckIdentifier(TEXT("switch")) || CheckIdentifier(TEXT("case")) || CheckIdentifier(TEXT("default")))
		{
			const FLangSpan WordSpan = Current().Span;
			const FString Word = Current().Text;
			Advance();

			Diagnostics.Error(TEXT("DSH2160"), WordSpan, FText::Format(
				LOCTEXT("SwitchNotSupported", "Unsupported statement '{0}': DreamShaderLang 2.0 has no switch statement, write if / else if instead."),
				FText::FromString(Word)));
			return nullptr;
		}

		if (LangStmtLooksLikeLocalDeclaration(*this))
		{
			TUniquePtr<FVarDeclStmt> Decl = ParseLocalVarDecl();
			if (!Decl)
			{
				return nullptr;
			}
			return MoveTemp(Decl);
		}

		FExprPtr Expression = ParseExpression();
		if (!Expression)
		{
			return nullptr;
		}

		if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("ExpressionSemicolon", "';' after an expression statement")))
		{
			return nullptr;
		}

		TUniquePtr<FExprStmt> Node = MakeUnique<FExprStmt>();
		Node->Expression = MoveTemp(Expression);
		Node->Span = SpanFrom(StartIndex);
		return MoveTemp(Node);
	}

	TUniquePtr<FBlockStmt> FLangParser::ParseBlock()
	{
		const int32 StartIndex = GetTokenIndex();

		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH2165"), LOCTEXT("BlockLeftBrace", "'{' to open a block")))
		{
			return nullptr;
		}

		TUniquePtr<FBlockStmt> Block = MakeUnique<FBlockStmt>();
		while (!Check(ELangTokenKind::RightBrace) && !AtEnd())
		{
			// A `///` line with nothing after it has no statement to introduce; drop it here so the
			// loop never hands the closing `}` to ParseStatement. Always makes progress.
			if (Match(ELangTokenKind::DocComment))
			{
				continue;
			}

			const int32 BeforeIndex = GetTokenIndex();

			FStmtPtr Statement = ParseStatement();
			if (Statement)
			{
				Block->Statements.Add(MoveTemp(Statement));
				continue;
			}

			// The failure is already reported. Skip to the next statement boundary and keep going;
			// the guard below makes sure a boundary that is already where we stand still advances,
			// because a loop that reports the same error forever is worse than a lost statement.
			SkipToStatementBoundary();
			if (GetTokenIndex() <= BeforeIndex && !Check(ELangTokenKind::RightBrace) && !AtEnd())
			{
				Advance();
			}
		}

		if (AtEnd())
		{
			FailAtEnd(LOCTEXT("WhileParsingBlock", "a block"));
			return nullptr;
		}

		Advance(); // '}'
		Block->Span = SpanFrom(StartIndex);
		return Block;
	}

	TUniquePtr<FVarDeclStmt> FLangParser::ParseLocalVarDecl()
	{
		const int32 StartIndex = GetTokenIndex();

		// `static const` and `const static` both read as one storage class; neither keyword may
		// repeat. `uniform` is a file-scope spelling only, and reaches ParseType as a bad type name.
		bool bStatic = false;
		bool bConst = false;
		for (;;)
		{
			if (!bStatic && MatchKeyword(ELangKeyword::Static))
			{
				bStatic = true;
				continue;
			}
			if (!bConst && MatchKeyword(ELangKeyword::Const))
			{
				bConst = true;
				continue;
			}
			break;
		}

		TUniquePtr<FVarDeclStmt> Node = MakeUnique<FVarDeclStmt>();
		if (bStatic && bConst)
		{
			Node->Storage = EStorageClass::StaticConst;
		}
		else if (bStatic)
		{
			Node->Storage = EStorageClass::Static;
		}
		else if (bConst)
		{
			Node->Storage = EStorageClass::Const;
		}

		if (!ParseType(Node->Type))
		{
			return nullptr;
		}

		for (;;)
		{
			const int32 DeclaratorStart = GetTokenIndex();

			FDeclarator Declarator;
			if (!ExpectIdentifier(Declarator.Name, Declarator.NameSpan, TEXT("DSH2163"), LOCTEXT("ExpectedVariableName", "a variable name")))
			{
				return nullptr;
			}

			if (!ParseArrayDimensions(Declarator.ArrayDimensions))
			{
				return nullptr;
			}

			if (Match(ELangTokenKind::Assign))
			{
				Declarator.Initializer = Check(ELangTokenKind::LeftBrace) ? ParseInitializerList() : ParseExpression();
				if (!Declarator.Initializer)
				{
					return nullptr;
				}
			}

			Declarator.Span = SpanFrom(DeclaratorStart);
			Node->Declarators.Add(MoveTemp(Declarator));

			if (!Match(ELangTokenKind::Comma))
			{
				break;
			}
		}

		if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2154"), LOCTEXT("VarDeclSemicolon", "';' after a variable declaration")))
		{
			return nullptr;
		}

		Node->Span = SpanFrom(StartIndex);
		return Node;
	}
}

#undef LOCTEXT_NAMESPACE
