// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The legacy (1.x) front end, Graph bodies.
//
// A 1.x `Graph = { ... }` body is parsed by the ordinary statement and expression parser -- the 2.0
// grammar is a superset of the 1.x one -- with the legacy hooks switched on (types classified the 1.x way,
// `N::F` flattened, calls rewritten, `#Region` lines read as regions). What the superset accepts and 1.x
// did not is then refused by the restriction pass below, instead of being compiled into something the
// author never wrote: the 1.x Graph parser stopped at the first token it did not know and kept what it
// had, so `a % b` was `a` and `v[1]` was `v` with no word said. None of the real sources relies on that;
// every such construct is an error here, and the message points at a `.dss` file, where it means what it
// says.
//
// Two things are synthesized after the restriction pass, both recorded in FLegacyMigrationInfo: a typed
// zero initializer for every declaration without one (1.x read an unset variable as zero; 2.0 refuses to,
// DSH4376), and one local at the head of the body for each 1.x `UE.*` property the body reads.

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.LegacyStatements"

namespace UE::DreamShader::Lang::Private
{
	namespace LegacyStatements
	{
		// -----------------------------------------------------------------------------------------
		// The restriction pass
		// -----------------------------------------------------------------------------------------

		class FLegacyBodyValidator
		{
		public:
			FLegacyBodyValidator(FLangDiagnosticSink& InDiagnostics, const FLangSourceText& InSource, const TSet<const FExpr*>& InSynthesizedIndexExprs)
				: Diagnostics(InDiagnostics)
				, Source(InSource)
				, SynthesizedIndexExprs(InSynthesizedIndexExprs)
			{
			}

			/** One expression in value position, outside any body: an Outputs binding source, a parameter default. */
			void ValidateStandaloneValue(const FExpr& Expr)
			{
				ValidateValue(Expr);
			}

			void ValidateBody(const FBlockStmt& Body)
			{
				ValidateStatements(Body);
				if (OpenRegions.Num() > 0)
				{
					const FPragmaStmt* Open = OpenRegions.Last();
					Diagnostics.Error(
						TEXT("DSH2218"),
						Open->Span,
						FText::Format(
							LOCTEXT("RegionNotClosed", "Expected '#EndRegion' to close the region '{0}' before the end of the Graph body, found none."),
							FText::FromString(Open->Text)));
				}
			}

		private:
			void ValidateStatements(const FBlockStmt& Block)
			{
				for (const FStmtPtr& Statement : Block.Statements)
				{
					if (Statement.IsValid())
					{
						ValidateStatement(*Statement);
					}
				}
			}

			void ValidateStatement(const FStmt& Statement)
			{
				if (const FVarDeclStmt* Declaration = Statement.As<FVarDeclStmt>())
				{
					if (Declaration->Storage != EStorageClass::None)
					{
						const FString Keyword = Declaration->Storage == EStorageClass::StaticConst ? TEXT("static const")
							: Declaration->Storage == EStorageClass::Static ? TEXT("static")
							: Declaration->Storage == EStorageClass::Const ? TEXT("const")
							: TEXT("uniform");
						Diagnostics.Error(
							TEXT("DSH2212"),
							Declaration->Span,
							FText::Format(
								LOCTEXT("StorageOnLocal", "Expected no storage keyword on a 1.x Graph variable, found '{0}'; move this code to a .dss file."),
								FText::FromString(Keyword)));
					}

					for (const FDeclarator& Declarator : Declaration->Declarators)
					{
						if (Declarator.ArrayDimensions.Num() > 0)
						{
							Diagnostics.Error(
								TEXT("DSH2213"),
								Declarator.Span,
								FText::Format(
									LOCTEXT("ArrayLocal", "Expected a single value in a 1.x Graph declaration, found the array declarator '{0}'; move this code to a .dss file."),
									FText::FromString(Declarator.Name)));
						}
						if (Declarator.Initializer.IsValid())
						{
							ValidateValue(*Declarator.Initializer);
						}
					}
					return;
				}

				if (const FExprStmt* ExpressionStatement = Statement.As<FExprStmt>())
				{
					const FExpr* Expression = ExpressionStatement->Expression.Get();
					if (!Expression)
					{
						return;
					}
					if (const FAssignExpr* Assign = Expression->As<FAssignExpr>())
					{
						ValidateAssignment(*Assign);
						return;
					}
					if (const FCallExpr* Call = Expression->As<FCallExpr>())
					{
						ValidateValue(*Call);
						return;
					}
					if (const FUnaryExpr* Unary = Expression->As<FUnaryExpr>())
					{
						// `C++;` is not a value left unused: it is an operator 1.x did not have, and DSH2207 says so.
						if (Unary->Op == EUnaryOp::PreIncrement || Unary->Op == EUnaryOp::PreDecrement
							|| Unary->Op == EUnaryOp::PostIncrement || Unary->Op == EUnaryOp::PostDecrement)
						{
							ValidateValue(*Unary);
							return;
						}
					}
					Diagnostics.Error(
						TEXT("DSH2211"),
						Statement.Span,
						LOCTEXT("UnusedExpression", "Expected a call or an assignment as a 1.x Graph statement, found an expression whose value is never used."));
					return;
				}

				if (const FIfStmt* If = Statement.As<FIfStmt>())
				{
					ValidateIf(*If);
					return;
				}

				if (const FPragmaStmt* Pragma = Statement.As<FPragmaStmt>())
				{
					if (Pragma->PragmaKind == EPragmaKind::Region)
					{
						OpenRegions.Add(Pragma);
					}
					else if (OpenRegions.Num() == 0)
					{
						Diagnostics.Error(
							TEXT("DSH2217"),
							Pragma->Span,
							LOCTEXT("EndRegionWithoutRegion", "Expected a '#Region' before this '#EndRegion', found none open."));
					}
					else
					{
						OpenRegions.Pop();
					}
					return;
				}

				if (Statement.Is<FEmptyStmt>())
				{
					// 1.x dropped stray semicolons.
					return;
				}

				if (Statement.Is<FBlockStmt>())
				{
					Diagnostics.Error(
						TEXT("DSH2220"),
						Statement.Span,
						LOCTEXT("NestedBlock", "Expected no bare block in a 1.x Graph body, found one; 1.x has blocks only after 'if' and 'else'."));
					return;
				}

				FString Keyword = TEXT("statement");
				if (Statement.Is<FForStmt>())
				{
					Keyword = TEXT("for");
				}
				else if (Statement.Is<FWhileStmt>())
				{
					Keyword = TEXT("while");
				}
				else if (Statement.Is<FDoWhileStmt>())
				{
					Keyword = TEXT("do");
				}
				else if (Statement.Is<FReturnStmt>())
				{
					Keyword = TEXT("return");
				}
				else if (Statement.Is<FBreakStmt>())
				{
					Keyword = TEXT("break");
				}
				else if (Statement.Is<FContinueStmt>())
				{
					Keyword = TEXT("continue");
				}
				else if (Statement.Is<FDiscardStmt>())
				{
					Keyword = TEXT("discard");
				}
				Diagnostics.Error(
					TEXT("DSH2208"),
					Statement.Span,
					FText::Format(
						LOCTEXT("StatementNotInLegacy", "Expected a declaration, an assignment, a call or 'if' in a 1.x Graph body, found '{0}', which 1.x did not have; move this code to a .dss file."),
						FText::FromString(Keyword)));
			}

			void ValidateIf(const FIfStmt& If)
			{
				if (If.Condition.IsValid())
				{
					// 1.x split the condition at the first top-level comparison and parsed each side as an
					// ordinary expression; with no comparison the condition was read as truthy.
					const FBinaryExpr* Comparison = If.Condition->As<FBinaryExpr>();
					if (Comparison && IsComparison(Comparison->Op))
					{
						ValidateConditionOperand(Comparison->Left.Get());
						ValidateConditionOperand(Comparison->Right.Get());
					}
					else
					{
						ValidateConditionOperand(If.Condition.Get());
					}
				}

				const FStmt* Then = If.Then.Get();
				if (Then && !Then->Is<FBlockStmt>())
				{
					Diagnostics.Error(
						TEXT("DSH2209"),
						Then->Span,
						LOCTEXT("IfWithoutBraces", "Expected braces around the body of a 1.x Graph 'if', found a single statement."));
				}
				else if (Then)
				{
					ValidateStatements(static_cast<const FBlockStmt&>(*Then));
				}

				const FStmt* Else = If.Else.Get();
				if (!Else)
				{
					return;
				}
				if (const FIfStmt* ElseIf = Else->As<FIfStmt>())
				{
					ValidateIf(*ElseIf);
				}
				else if (const FBlockStmt* ElseBlock = Else->As<FBlockStmt>())
				{
					ValidateStatements(*ElseBlock);
				}
				else
				{
					Diagnostics.Error(
						TEXT("DSH2209"),
						Else->Span,
						LOCTEXT("ElseWithoutBraces", "Expected braces around the body of a 1.x Graph 'else', found a single statement."));
				}
			}

			void ValidateConditionOperand(const FExpr* Operand)
			{
				if (!Operand)
				{
					return;
				}
				if (const FBinaryExpr* Binary = Operand->As<FBinaryExpr>())
				{
					if (IsComparison(Binary->Op) || Binary->Op == EBinaryOp::LogicalAnd || Binary->Op == EBinaryOp::LogicalOr)
					{
						Diagnostics.Error(
							TEXT("DSH2210"),
							Binary->Span,
							FText::Format(
								LOCTEXT("ConditionTruncated", "Expected at most one comparison in a 1.x Graph 'if' condition, found '{0}' as well, where 1.x silently dropped the rest of the condition; move this code to a .dss file."),
								FText::FromString(BinaryOpText(Binary->Op))));
						return;
					}
				}
				ValidateValue(*Operand);
			}

			void ValidateAssignment(const FAssignExpr& Assign)
			{
				if (Assign.Op != EAssignOp::Assign)
				{
					const FString Target = Assign.Target.IsValid() ? Source.Slice(Assign.Target->Span) : FString();
					const FString Operator = AssignOpText(Assign.Op);
					Diagnostics.Error(
						TEXT("DSH2205"),
						Assign.Span,
						FText::Format(
							LOCTEXT("CompoundAssignment", "Expected '=' in a 1.x Graph assignment, found '{0}', which 1.x read as the declaration of a variable named '{1}'; write 'x = x + y' or move this code to a .dss file."),
							FText::FromString(Operator),
							FText::FromString(Target + Operator.Left(Operator.Len() - 1))));
					return;
				}

				const FExpr* Target = Assign.Target.Get();
				bool bAssignable = false;
				if (Target)
				{
					if (Target->Is<FIdentifierExpr>())
					{
						bAssignable = true;
					}
					else if (const FMemberExpr* Member = Target->As<FMemberExpr>())
					{
						bAssignable = Member->Object.IsValid() && Member->Object->Is<FIdentifierExpr>();
					}
				}
				if (!bAssignable)
				{
					Diagnostics.Error(
						TEXT("DSH2206"),
						Target ? Target->Span : Assign.Span,
						FText::Format(
							LOCTEXT("BadAssignmentTarget", "Expected a variable or 'variable.member' on the left of a 1.x Graph assignment, found '{0}'."),
							FText::FromString(Target ? Source.Slice(Target->Span) : FString())));
					return;
				}

				if (Assign.Value.IsValid())
				{
					ValidateValue(*Assign.Value);
				}
			}

			/** An expression in value position. Reports the first construct 1.x did not have and does not descend into it. */
			void ValidateValue(const FExpr& Expr)
			{
				if (const FLiteralExpr* Literal = Expr.As<FLiteralExpr>())
				{
					if ((Literal->LiteralKind == ELiteralKind::Int || Literal->LiteralKind == ELiteralKind::UInt)
						&& (Literal->Text.StartsWith(TEXT("0x"), ESearchCase::IgnoreCase)))
					{
						Diagnostics.Error(
							TEXT("DSH2222"),
							Literal->Span,
							FText::Format(
								LOCTEXT("HexLiteral", "Expected a decimal number in a 1.x Graph expression, found the hexadecimal literal '{0}', which 1.x read as 0 followed by a name; move this code to a .dss file."),
								FText::FromString(Literal->Text)));
					}
					return;
				}

				if (Expr.Is<FIdentifierExpr>() || Expr.Is<FTypeExpr>())
				{
					return;
				}

				if (const FMemberExpr* Member = Expr.As<FMemberExpr>())
				{
					if (Member->Object.IsValid())
					{
						ValidateValue(*Member->Object);
					}
					return;
				}

				if (const FIndexExpr* Index = Expr.As<FIndexExpr>())
				{
					if (!SynthesizedIndexExprs.Contains(Index))
					{
						Diagnostics.Error(
							TEXT("DSH2202"),
							Index->Span,
							FText::Format(
								LOCTEXT("IndexTruncated", "Expected no '[ ]' in a 1.x Graph expression, found '{0}', where 1.x silently dropped the index and everything after it; use a swizzle such as '.r' or move this code to a .dss file."),
								FText::FromString(Source.Slice(Index->Span))));
						return;
					}
					if (Index->Object.IsValid())
					{
						ValidateValue(*Index->Object);
					}
					return;
				}

				if (const FCallExpr* Call = Expr.As<FCallExpr>())
				{
					if (Call->Callee.IsValid() && !Call->Callee->Is<FIdentifierExpr>() && !Call->Callee->Is<FTypeExpr>())
					{
						ValidateValue(*Call->Callee);
					}
					for (const FArgument& Argument : Call->Arguments)
					{
						if (Argument.Value.IsValid())
						{
							ValidateValue(*Argument.Value);
						}
					}
					return;
				}

				if (const FUnaryExpr* Unary = Expr.As<FUnaryExpr>())
				{
					switch (Unary->Op)
					{
					case EUnaryOp::Plus:
					case EUnaryOp::Negate:
						if (Unary->Operand.IsValid())
						{
							ValidateValue(*Unary->Operand);
						}
						return;
					case EUnaryOp::LogicalNot:
						ReportTruncation(TEXT("DSH2200"), Unary->Span, TEXT("!"));
						return;
					case EUnaryOp::BitwiseNot:
						ReportTruncation(TEXT("DSH2201"), Unary->Span, TEXT("~"));
						return;
					case EUnaryOp::PreIncrement:
					case EUnaryOp::PreDecrement:
					case EUnaryOp::PostIncrement:
					case EUnaryOp::PostDecrement:
						Diagnostics.Error(
							TEXT("DSH2207"),
							Unary->Span,
							FText::Format(
								LOCTEXT("IncrementDecrement", "Expected no '++' or '--' in a 1.x Graph expression, found '{0}'; move this code to a .dss file."),
								FText::FromString(Source.Slice(Unary->Span))));
						return;
					}
					return;
				}

				if (const FBinaryExpr* Binary = Expr.As<FBinaryExpr>())
				{
					switch (Binary->Op)
					{
					case EBinaryOp::Multiply:
					case EBinaryOp::Divide:
					case EBinaryOp::Add:
					case EBinaryOp::Subtract:
						if (Binary->Left.IsValid())
						{
							ValidateValue(*Binary->Left);
						}
						if (Binary->Right.IsValid())
						{
							ValidateValue(*Binary->Right);
						}
						return;
					case EBinaryOp::Modulo:
					case EBinaryOp::ShiftLeft:
					case EBinaryOp::ShiftRight:
					case EBinaryOp::BitwiseAnd:
					case EBinaryOp::BitwiseXor:
					case EBinaryOp::BitwiseOr:
						ReportTruncation(TEXT("DSH2201"), Binary->Span, BinaryOpText(Binary->Op));
						return;
					case EBinaryOp::Less:
					case EBinaryOp::LessEqual:
					case EBinaryOp::Greater:
					case EBinaryOp::GreaterEqual:
					case EBinaryOp::Equal:
					case EBinaryOp::NotEqual:
					case EBinaryOp::LogicalAnd:
					case EBinaryOp::LogicalOr:
						ReportTruncation(TEXT("DSH2200"), Binary->Span, BinaryOpText(Binary->Op));
						return;
					}
					return;
				}

				if (Expr.Is<FConditionalExpr>())
				{
					ReportTruncation(TEXT("DSH2200"), Expr.Span, TEXT("?"));
					return;
				}

				if (const FCastExpr* Cast = Expr.As<FCastExpr>())
				{
					Diagnostics.Error(
						TEXT("DSH2203"),
						Cast->Span,
						FText::Format(
							LOCTEXT("CastInLegacy", "Expected a constructor such as 'float3(x)' in a 1.x Graph expression, found the cast '({0})', which 1.x never had; move this code to a .dss file."),
							FText::FromString(PrintDreamShaderLangType(Cast->Type))));
					return;
				}

				if (Expr.Is<FAssignExpr>())
				{
					Diagnostics.Error(
						TEXT("DSH2204"),
						Expr.Span,
						LOCTEXT("NestedAssignment", "Expected an assignment only as a whole 1.x Graph statement, found one inside an expression; move this code to a .dss file."));
					return;
				}

				if (Expr.Is<FInitializerListExpr>())
				{
					Diagnostics.Error(
						TEXT("DSH2214"),
						Expr.Span,
						LOCTEXT("InitializerList", "Expected an expression as a 1.x Graph initializer, found an initializer list; use a constructor such as 'float3(a, b, c)'."));
					return;
				}

				if (const FParenExpr* Paren = Expr.As<FParenExpr>())
				{
					if (Paren->Inner.IsValid())
					{
						ValidateValue(*Paren->Inner);
					}
				}
			}

			void ReportTruncation(const TCHAR* Code, const FLangSpan& Span, const FString& Operator)
			{
				// Two literal raise sites, so the diagnostics scanner sees both codes.
				const FText Message = FText::Format(
					LOCTEXT("OperatorTruncated", "Expected only '+', '-', '*', '/', calls and members in a 1.x Graph expression, found '{0}', where 1.x silently dropped the rest of the expression; move this code to a .dss file."),
					FText::FromString(Operator));
				if (FCString::Strcmp(Code, TEXT("DSH2201")) == 0)
				{
					Diagnostics.Error(TEXT("DSH2201"), Span, Message);
				}
				else
				{
					Diagnostics.Error(TEXT("DSH2200"), Span, Message);
				}
			}

			static bool IsComparison(const EBinaryOp Op)
			{
				switch (Op)
				{
				case EBinaryOp::Less:
				case EBinaryOp::LessEqual:
				case EBinaryOp::Greater:
				case EBinaryOp::GreaterEqual:
				case EBinaryOp::Equal:
				case EBinaryOp::NotEqual:
					return true;
				case EBinaryOp::Multiply:
				case EBinaryOp::Divide:
				case EBinaryOp::Modulo:
				case EBinaryOp::Add:
				case EBinaryOp::Subtract:
				case EBinaryOp::ShiftLeft:
				case EBinaryOp::ShiftRight:
				case EBinaryOp::BitwiseAnd:
				case EBinaryOp::BitwiseXor:
				case EBinaryOp::BitwiseOr:
				case EBinaryOp::LogicalAnd:
				case EBinaryOp::LogicalOr:
					return false;
				}
				return false;
			}

			static FString BinaryOpText(const EBinaryOp Op)
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
				}
				return FString();
			}

			static FString AssignOpText(const EAssignOp Op)
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
				}
				return FString();
			}

			FLangDiagnosticSink& Diagnostics;
			const FLangSourceText& Source;
			const TSet<const FExpr*>& SynthesizedIndexExprs;
			TArray<const FPragmaStmt*> OpenRegions;
		};

		// -----------------------------------------------------------------------------------------
		// Reads of 1.x `UE.*` properties
		// -----------------------------------------------------------------------------------------

		static void CollectLegacyIdentifierReads(const FExpr* Expr, TArray<const FIdentifierExpr*>& Out)
		{
			if (!Expr)
			{
				return;
			}
			if (const FIdentifierExpr* Identifier = Expr->As<FIdentifierExpr>())
			{
				Out.Add(Identifier);
			}
			else if (const FMemberExpr* Member = Expr->As<FMemberExpr>())
			{
				CollectLegacyIdentifierReads(Member->Object.Get(), Out);
			}
			else if (const FIndexExpr* Index = Expr->As<FIndexExpr>())
			{
				CollectLegacyIdentifierReads(Index->Object.Get(), Out);
				CollectLegacyIdentifierReads(Index->Index.Get(), Out);
			}
			else if (const FCallExpr* Call = Expr->As<FCallExpr>())
			{
				if (Call->Callee.IsValid() && !Call->Callee->Is<FIdentifierExpr>())
				{
					CollectLegacyIdentifierReads(Call->Callee.Get(), Out);
				}
				for (const FArgument& Argument : Call->Arguments)
				{
					CollectLegacyIdentifierReads(Argument.Value.Get(), Out);
				}
			}
			else if (const FUnaryExpr* Unary = Expr->As<FUnaryExpr>())
			{
				CollectLegacyIdentifierReads(Unary->Operand.Get(), Out);
			}
			else if (const FBinaryExpr* Binary = Expr->As<FBinaryExpr>())
			{
				CollectLegacyIdentifierReads(Binary->Left.Get(), Out);
				CollectLegacyIdentifierReads(Binary->Right.Get(), Out);
			}
			else if (const FAssignExpr* Assign = Expr->As<FAssignExpr>())
			{
				CollectLegacyIdentifierReads(Assign->Target.Get(), Out);
				CollectLegacyIdentifierReads(Assign->Value.Get(), Out);
			}
			else if (const FConditionalExpr* Conditional = Expr->As<FConditionalExpr>())
			{
				CollectLegacyIdentifierReads(Conditional->Condition.Get(), Out);
				CollectLegacyIdentifierReads(Conditional->TrueValue.Get(), Out);
				CollectLegacyIdentifierReads(Conditional->FalseValue.Get(), Out);
			}
			else if (const FCastExpr* Cast = Expr->As<FCastExpr>())
			{
				CollectLegacyIdentifierReads(Cast->Operand.Get(), Out);
			}
			else if (const FParenExpr* Paren = Expr->As<FParenExpr>())
			{
				CollectLegacyIdentifierReads(Paren->Inner.Get(), Out);
			}
			else if (const FInitializerListExpr* List = Expr->As<FInitializerListExpr>())
			{
				for (const FExprPtr& Element : List->Elements)
				{
					CollectLegacyIdentifierReads(Element.Get(), Out);
				}
			}
		}

		static void CollectLegacyStatementReads(const FStmt* Statement, TArray<const FIdentifierExpr*>& Out)
		{
			if (!Statement)
			{
				return;
			}
			if (const FBlockStmt* Block = Statement->As<FBlockStmt>())
			{
				for (const FStmtPtr& Child : Block->Statements)
				{
					CollectLegacyStatementReads(Child.Get(), Out);
				}
			}
			else if (const FVarDeclStmt* Declaration = Statement->As<FVarDeclStmt>())
			{
				for (const FDeclarator& Declarator : Declaration->Declarators)
				{
					CollectLegacyIdentifierReads(Declarator.Initializer.Get(), Out);
				}
			}
			else if (const FExprStmt* ExpressionStatement = Statement->As<FExprStmt>())
			{
				CollectLegacyIdentifierReads(ExpressionStatement->Expression.Get(), Out);
			}
			else if (const FIfStmt* If = Statement->As<FIfStmt>())
			{
				CollectLegacyIdentifierReads(If->Condition.Get(), Out);
				CollectLegacyStatementReads(If->Then.Get(), Out);
				CollectLegacyStatementReads(If->Else.Get(), Out);
			}
		}

		/** The width 1.x gave a `UE.*` property: its `OutputType`, else the builtin's own (ParseUEBuiltinPropertyType). */
		static FString LegacyBuiltinPropertyTypeName(const FString& FunctionName, const TArray<TPair<FString, FString>>& Arguments)
		{
			for (const TPair<FString, FString>& Argument : Arguments)
			{
				if (Argument.Key.Equals(TEXT("OutputType"), ESearchCase::IgnoreCase) || Argument.Key.Equals(TEXT("ResultType"), ESearchCase::IgnoreCase))
				{
					FTypeRef Type;
					FLangParser::ClassifyLegacyTypeName(LegacyAst::Unquote(Argument.Value).Replace(TEXT(" "), TEXT("")), Type);
					if (Type.Category != ETypeCategory::Named)
					{
						return Type.Name;
					}
				}
			}

			if (FunctionName.Equals(TEXT("TexCoord"), ESearchCase::IgnoreCase) || FunctionName.Equals(TEXT("Panner"), ESearchCase::IgnoreCase))
			{
				return TEXT("float2");
			}
			if (FunctionName.Equals(TEXT("Time"), ESearchCase::IgnoreCase))
			{
				return TEXT("float");
			}
			if (FunctionName.Equals(TEXT("WorldPosition"), ESearchCase::IgnoreCase)
				|| FunctionName.Equals(TEXT("CameraVectorWS"), ESearchCase::IgnoreCase)
				|| FunctionName.Equals(TEXT("ObjectPositionWS"), ESearchCase::IgnoreCase)
				|| FunctionName.Equals(TEXT("VertexNormalWS"), ESearchCase::IgnoreCase)
				|| FunctionName.Equals(TEXT("VertexTangentWS"), ESearchCase::IgnoreCase))
			{
				return TEXT("float3");
			}
			// ScreenPosition, VertexColor, and a collection parameter: 1.x read a collection's width off the
			// asset; without assets the front end takes the vector width (research-legacy.md 9 Q21).
			return TEXT("float4");
		}

		/** `Collection = Path(...), Parameter = "P"` as the Properties section stored it: key and value text as written. */
		static void SplitLegacyBuiltinArguments(const FString& ArgumentText, TArray<TPair<FString, FString>>& OutArguments)
		{
			int32 Depth = 0;
			bool bInString = false;
			FString Current;
			TArray<FString> Parts;
			for (int32 Index = 0; Index < ArgumentText.Len(); ++Index)
			{
				const TCHAR Character = ArgumentText[Index];
				if (bInString)
				{
					Current.AppendChar(Character);
					if (Character == TEXT('\\') && Index + 1 < ArgumentText.Len())
					{
						Current.AppendChar(ArgumentText[++Index]);
					}
					else if (Character == TEXT('"'))
					{
						bInString = false;
					}
					continue;
				}
				if (Character == TEXT('"'))
				{
					bInString = true;
				}
				else if (Character == TEXT('(') || Character == TEXT('['))
				{
					++Depth;
				}
				else if (Character == TEXT(')') || Character == TEXT(']'))
				{
					Depth = FMath::Max(0, Depth - 1);
				}
				else if (Character == TEXT(',') && Depth == 0)
				{
					Parts.Add(Current);
					Current.Reset();
					continue;
				}
				Current.AppendChar(Character);
			}
			Parts.Add(Current);

			for (const FString& Part : Parts)
			{
				const int32 Equals = Part.Find(TEXT("="));
				if (Equals == INDEX_NONE)
				{
					continue;
				}
				const FString Key = Part.Left(Equals).TrimStartAndEnd();
				const FString Value = Part.Mid(Equals + 1).TrimStartAndEnd();
				if (!Key.IsEmpty() && !Value.IsEmpty())
				{
					OutArguments.Emplace(Key, Value);
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Entry points
	// ---------------------------------------------------------------------------------------------

	TUniquePtr<FBlockStmt> FLangParser::ParseLegacyGraphBody(FLegacyBlockContext& Block)
	{
		FLegacyBlockContext* const OuterBlock = LegacyBlock;
		LegacyBlock = &Block;
		++LegacyScopeDepth;

		TUniquePtr<FBlockStmt> Body = ParseBlock();

		--LegacyScopeDepth;
		LegacyBlock = OuterBlock;

		if (!Body.IsValid())
		{
			return nullptr;
		}

		RewriteLegacyBraceInitializers(*Body);
		ValidateLegacyBody(*Body);
		SynthesizeLegacyInitializers(*Body);

		// One local at the head of the body for every 1.x `UE.*` property the body reads, in property order.
		TArray<const FIdentifierExpr*> Reads;
		LegacyStatements::CollectLegacyStatementReads(Body.Get(), Reads);

		TArray<FStmtPtr> Head;
		for (const FLegacyProperty& Property : Block.Properties)
		{
			if (!Property.NodeType.StartsWith(TEXT("UE."), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FIdentifierExpr* FirstRead = nullptr;
			for (const FIdentifierExpr* Read : Reads)
			{
				if (Read->Name.Equals(Property.Name, ESearchCase::IgnoreCase))
				{
					FirstRead = Read;
					break;
				}
			}
			if (!FirstRead)
			{
				continue;
			}

			const FString FunctionName = Property.NodeType.RightChop(3);
			TArray<TPair<FString, FString>> Arguments;
			LegacyStatements::SplitLegacyBuiltinArguments(Property.DefaultText, Arguments);

			const bool bCollection = FunctionName.Equals(TEXT("CollectionParam"), ESearchCase::IgnoreCase)
				|| FunctionName.Equals(TEXT("CollectionParameter"), ESearchCase::IgnoreCase);

			TUniquePtr<FCallExpr> Call = MakeUnique<FCallExpr>();
			Call->Callee = LegacyAst::MakeReflectedCallee(TEXT("UE"), bCollection ? FString(TEXT("CollectionParameter")) : FunctionName, Property.Span);
			Call->Span = Property.Span;
			const FExpr* CollectionNode = nullptr;
			FString CollectionText;
			for (const TPair<FString, FString>& Argument : Arguments)
			{
				const FString Key = Argument.Key;
				if (Key.Equals(TEXT("OutputType"), ESearchCase::IgnoreCase) || Key.Equals(TEXT("ResultType"), ESearchCase::IgnoreCase))
				{
					continue;
				}

				FString Name = Key;
				FExprPtr Value;
				if (bCollection && (Key.Equals(TEXT("Collection"), ESearchCase::IgnoreCase) || Key.Equals(TEXT("Asset"), ESearchCase::IgnoreCase)))
				{
					// Unresolved on purpose; the emitter resolves `Path(...)` (research-legacy.md 3.8).
					Name = TEXT("Collection");
					CollectionText = LegacyAst::Unquote(Argument.Value);
					Value = LegacyAst::MakeStringLiteral(CollectionText, Property.Span);
					CollectionNode = Value.Get();
				}
				else if (bCollection && (Key.Equals(TEXT("Parameter"), ESearchCase::IgnoreCase) || Key.Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase)))
				{
					Name = TEXT("ParameterName");
					Value = LegacyAst::MakeStringLiteral(LegacyAst::Unquote(Argument.Value), Property.Span);
				}
				else if (Key.Equals(TEXT("Description"), ESearchCase::IgnoreCase))
				{
					Name = TEXT("Desc");
					Value = LegacyAst::MakeValueExpressionFromText(Argument.Value, Property.Span);
				}
				else
				{
					Value = LegacyAst::MakeValueExpressionFromText(Argument.Value, Property.Span);
				}
				Call->Arguments.Add(LegacyAst::MakeNamedArgument(Name, MoveTemp(Value), Property.Span));
			}

			TUniquePtr<FVarDeclStmt> Local = MakeUnique<FVarDeclStmt>();
			ClassifyTypeName(LegacyStatements::LegacyBuiltinPropertyTypeName(FunctionName, Arguments), Local->Type);
			Local->Type.Span = Property.Span;
			FDeclarator Declarator;
			Declarator.Name = Property.Name;
			Declarator.NameSpan = Property.Span;
			Declarator.Span = Property.Span;
			const FExpr* Initializer = Call.Get();
			Declarator.Initializer = MoveTemp(Call);
			Local->Declarators.Add(MoveTemp(Declarator));
			Local->Span = Property.Span;

			if (LegacyInfo)
			{
				for (FLegacyParameterDeclaration& Declaration : LegacyInfo->ParameterDeclarations)
				{
					if (Declaration.DeclarationSpan.Offset == Property.Span.Offset && Declaration.Name.Equals(Property.Name, ESearchCase::CaseSensitive))
					{
						Declaration.UseSpans.Add(FirstRead->Span);
						Declaration.Expansions.Add(Initializer);
						break;
					}
				}
				if (CollectionNode)
				{
					FLegacyAssetReference& Reference = LegacyInfo->AssetReferences.AddDefaulted_GetRef();
					Reference.Use = FLegacyAssetReference::EUse::CollectionParameter;
					Reference.Text = CollectionText;
					Reference.Span = Property.Span;
					Reference.Node = CollectionNode;
				}
			}

			Head.Add(MoveTemp(Local));
		}

		if (Head.Num() > 0)
		{
			Body->Statements.Insert(MoveTemp(Head), 0);
		}

		return Body;
	}

	void FLangParser::ValidateLegacyBody(const FBlockStmt& Body)
	{
		LegacyStatements::FLegacyBodyValidator Validator(Diagnostics, Source, LegacySynthesizedIndexExprs);
		Validator.ValidateBody(Body);
	}

	void FLangParser::ValidateLegacyValue(const FExpr& Expr)
	{
		LegacyStatements::FLegacyBodyValidator Validator(Diagnostics, Source, LegacySynthesizedIndexExprs);
		Validator.ValidateStandaloneValue(Expr);
	}

	FStmtPtr FLangParser::ParseLegacyRegionDirective()
	{
		const FLangToken& Token = Advance();
		const FLangSpan Span = Token.Span;

		// 1.x compared the trimmed line against `#Region` / `#EndRegion` with nothing between the `#` and the
		// word, ignoring case, and wanted whitespace, a quote or the end of the line after it.
		const FString& Text = Source.GetText();
		int32 WordEnd = Span.Offset + 1;
		while (WordEnd < Span.End() && (FChar::IsAlnum(Text[WordEnd]) || Text[WordEnd] == TEXT('_')))
		{
			++WordEnd;
		}
		const FString Word = Text.Mid(Span.Offset + 1, WordEnd - (Span.Offset + 1));
		const bool bRegion = Word.Equals(TEXT("Region"), ESearchCase::IgnoreCase);
		const bool bEndRegion = Word.Equals(TEXT("EndRegion"), ESearchCase::IgnoreCase);

		if (!bRegion && !bEndRegion)
		{
			Diagnostics.Error(
				TEXT("DSH2219"),
				Span,
				FText::Format(
					LOCTEXT("OtherDirectiveInGraph", "Expected '#Region' or '#EndRegion' as the only '#' line in a 1.x Graph body, found '#{0}'."),
					FText::FromString(Token.Text)));
			return nullptr;
		}

		// Token.Text is the line after `#`, trimmed, trailing `//` comment removed.
		const int32 WordInPayload = Token.Text.Find(Word, ESearchCase::IgnoreCase);
		const FString Rest = WordInPayload == INDEX_NONE ? FString() : Token.Text.Mid(WordInPayload + Word.Len()).TrimStartAndEnd();
		const FString Title = LegacyAst::Unquote(Rest).TrimStartAndEnd();

		if (bRegion && Title.IsEmpty())
		{
			Diagnostics.Error(
				TEXT("DSH2216"),
				Span,
				LOCTEXT("RegionWithoutName", "Expected a name after '#Region', found none."));
			return nullptr;
		}

		TUniquePtr<FPragmaStmt> Node = MakeUnique<FPragmaStmt>();
		Node->PragmaKind = bRegion ? EPragmaKind::Region : EPragmaKind::EndRegion;
		Node->Text = bRegion ? Title : FString();
		Node->Span = Span;
		return Node;
	}

	void FLangParser::RewriteLegacyBraceInitializers(FBlockStmt& Body)
	{
		for (FStmtPtr& Statement : Body.Statements)
		{
			if (!Statement.IsValid())
			{
				continue;
			}

			if (FIfStmt* If = Statement->As<FIfStmt>())
			{
				// The same walk SynthesizeLegacyInitializers makes: `if` arms are the only nested blocks 1.x had.
				FIfStmt* Current = If;
				while (Current)
				{
					if (FBlockStmt* Then = Current->Then.IsValid() ? Current->Then->As<FBlockStmt>() : nullptr)
					{
						RewriteLegacyBraceInitializers(*Then);
					}
					FStmt* Else = Current->Else.Get();
					if (FBlockStmt* ElseBlock = Else ? Else->As<FBlockStmt>() : nullptr)
					{
						RewriteLegacyBraceInitializers(*ElseBlock);
						Current = nullptr;
					}
					else
					{
						Current = Else ? Else->As<FIfStmt>() : nullptr;
					}
				}
				continue;
			}

			FVarDeclStmt* Declaration = Statement->As<FVarDeclStmt>();
			if (!Declaration)
			{
				continue;
			}

			for (FDeclarator& Declarator : Declaration->Declarators)
			{
				FInitializerListExpr* List = Declarator.Initializer.IsValid() ? Declarator.Initializer->As<FInitializerListExpr>() : nullptr;
				if (!List || Declarator.ArrayDimensions.Num() > 0)
				{
					continue;
				}
				if (List->Elements.Num() == 0)
				{
					// `T x = {};`: no initializer, which SynthesizeLegacyInitializers turns into T's zero.
					Declarator.Initializer.Reset();
					continue;
				}

				TUniquePtr<FTypeExpr> Callee = MakeUnique<FTypeExpr>();
				Callee->Type = Declaration->Type;
				Callee->Span = List->Span;

				TUniquePtr<FCallExpr> Constructor = MakeUnique<FCallExpr>();
				Constructor->Callee = MoveTemp(Callee);
				Constructor->Span = List->Span;
				for (FExprPtr& Element : List->Elements)
				{
					FArgument Argument;
					Argument.Span = Element.IsValid() ? Element->Span : List->Span;
					Argument.Value = MoveTemp(Element);
					Constructor->Arguments.Add(MoveTemp(Argument));
				}
				Declarator.Initializer = MoveTemp(Constructor);
			}
		}
	}

	void FLangParser::SynthesizeLegacyInitializers(FBlockStmt& Body)
	{
		for (FStmtPtr& Statement : Body.Statements)
		{
			if (!Statement.IsValid())
			{
				continue;
			}

			if (FIfStmt* If = Statement->As<FIfStmt>())
			{
				FIfStmt* Current = If;
				while (Current)
				{
					if (FBlockStmt* Then = Current->Then.IsValid() ? Current->Then->As<FBlockStmt>() : nullptr)
					{
						SynthesizeLegacyInitializers(*Then);
					}
					FStmt* Else = Current->Else.Get();
					Current = nullptr;
					if (Else)
					{
						if (FBlockStmt* ElseBlock = Else->As<FBlockStmt>())
						{
							SynthesizeLegacyInitializers(*ElseBlock);
						}
						else
						{
							Current = Else->As<FIfStmt>();
						}
					}
				}
				continue;
			}

			FVarDeclStmt* Declaration = Statement->As<FVarDeclStmt>();
			if (!Declaration)
			{
				continue;
			}

			for (FDeclarator& Declarator : Declaration->Declarators)
			{
				if (Declarator.Initializer.IsValid())
				{
					continue;
				}

				FExprPtr Zero = MakeLegacyZeroInitializer(Declaration->Type, Declarator.Span);
				if (!Zero.IsValid())
				{
					const bool bRefused = Declaration->Type.Category == ETypeCategory::Texture
						|| Declaration->Type.Category == ETypeCategory::Substrate
						|| Declaration->Type.Category == ETypeCategory::Sampler;
					if (bRefused)
					{
						Diagnostics.Error(
							TEXT("DSH2215"),
							Declarator.Span,
							FText::Format(
								LOCTEXT("NoZeroForType", "Expected an initializer on the 1.x Graph variable '{0}' of type '{1}', found none, and 1.x had no zero value for that type."),
								FText::FromString(Declarator.Name),
								FText::FromString(PrintDreamShaderLangType(Declaration->Type))));
					}
					continue;
				}

				Declarator.Initializer = MoveTemp(Zero);
				if (LegacyInfo)
				{
					FLegacySynthesizedInitializer& Record = LegacyInfo->SynthesizedInitializers.AddDefaulted_GetRef();
					Record.Declaration = Declaration;
					Record.Name = Declarator.Name;
					Record.Span = Declarator.Span;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
