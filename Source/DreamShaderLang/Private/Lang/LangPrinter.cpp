// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamShaderLang printer: one FModule back to `.dss` source text.
//
// The contract (Public/Lang/LangPrinter.h) is STRUCTURAL fidelity, not textual -- the tree that
// comes out of parse(print(parse(X))) is the tree parse(X) produced. Everything here therefore
// treats layout as free and the token stream as fixed:
//
//   * Layout follows the 2.0 proposal (Plan/syntax-v2-proposal.md, sections 7 and 8): four spaces
//     per level, Allman braces, one blank line between top-level declarations, a `///` block glued
//     to the declaration below it, `in` omitted on parameters because it is the default.
//   * Parentheses come back where FParenExpr recorded them (it prints its own and counts as an
//     atom) AND wherever precedence would otherwise re-associate the tree -- see PrintOperand.
//     Inserting a parenthesis is safe for the round trip: the next parse turns it into a
//     FParenExpr, which prints exactly the same text, so print/parse reaches a fixed point after
//     one iteration. That fixed point is what the printer tests assert.
//   * Precedence is read from LangOperators.h -- the same table the expression parser climbs -- so
//     "what binds tighter" and "where a parenthesis is required" cannot drift apart.
//   * An opaque body (a `/// @custom` function) is written back verbatim between its braces: not
//     re-indented, not re-lexed, because it is not DreamShaderLang.
//   * `///` blocks are normalised to free text first, then one directive per line. The tree keeps
//     the two in separate arrays, so their original interleaving is not recoverable and printing
//     them in a fixed order is what makes the round trip converge.
//   * Comments other than `///` are not in the tree and do not come back.
//
// The printer raises no diagnostics and is total over any tree it is handed: a null child (what a
// language service holds after a broken parse) prints as nothing rather than crashing.

#include "Lang/LangPrinter.h"

#include "LangOperators.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Lang/LangAst.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CString.h"
#include "Templates/UniquePtr.h"

namespace UE::DreamShader::Lang
{
	// Everything below lives in its own nested namespace: this module is built as a unity blob and
	// the helper names here (PrintType, PrintExpression, ...) are exactly the names the other
	// front-end translation units would pick for their own file-local helpers.
	namespace Private::Printer
	{
		// --------------------------------------------------------------------------------------
		// Spellings
		// --------------------------------------------------------------------------------------

		static const TCHAR* GetScalarSpelling(EScalarKind Scalar)
		{
			switch (Scalar)
			{
			case EScalarKind::Float:  return TEXT("float");
			case EScalarKind::Half:   return TEXT("half");
			case EScalarKind::Double: return TEXT("double");
			case EScalarKind::Int:    return TEXT("int");
			case EScalarKind::UInt:   return TEXT("uint");
			case EScalarKind::Bool:   return TEXT("bool");
			default:                  return TEXT("");
			}
		}

		static const TCHAR* GetTextureSpelling(ETextureKind Texture)
		{
			switch (Texture)
			{
			case ETextureKind::Texture2D:      return TEXT("Texture2D");
			case ETextureKind::TextureCube:    return TEXT("TextureCube");
			case ETextureKind::Texture2DArray: return TEXT("Texture2DArray");
			case ETextureKind::Texture3D:      return TEXT("Texture3D");
			case ETextureKind::VolumeTexture:  return TEXT("VolumeTexture");
			default:                           return TEXT("");
			}
		}

		/** Storage keyword(s) with their trailing space, or nothing. */
		static const TCHAR* GetStorageSpelling(EStorageClass Storage)
		{
			switch (Storage)
			{
			case EStorageClass::Uniform:     return TEXT("uniform ");
			case EStorageClass::StaticConst: return TEXT("static const ");
			case EStorageClass::Static:      return TEXT("static ");
			case EStorageClass::Const:       return TEXT("const ");
			default:                         return TEXT("");
			}
		}

		/** Linkage keyword with its trailing space, or nothing. `Internal` has no keyword. */
		static const TCHAR* GetLinkageSpelling(EFunctionLinkage Linkage)
		{
			switch (Linkage)
			{
			case EFunctionLinkage::Export: return TEXT("export ");
			case EFunctionLinkage::Extern: return TEXT("extern ");
			default:                       return TEXT("");
			}
		}

		/** Direction keyword with its trailing space. `in` is the default and is left unwritten. */
		static const TCHAR* GetDirectionSpelling(EParamDirection Direction)
		{
			switch (Direction)
			{
			case EParamDirection::Out:   return TEXT("out ");
			case EParamDirection::InOut: return TEXT("inout ");
			default:                     return TEXT("");
			}
		}

		/** The name a pragma of this kind is spelled with, for a node that lost its Name. */
		static const TCHAR* GetDefaultPragmaName(EPragmaKind Kind)
		{
			switch (Kind)
			{
			case EPragmaKind::Material:  return TEXT("material");
			case EPragmaKind::Layout:    return TEXT("layout");
			case EPragmaKind::Region:    return TEXT("region");
			case EPragmaKind::EndRegion: return TEXT("endregion");
			default:                     return TEXT("");
			}
		}

		/**
		 * A type as written. FTypeRef::Name is the spelling the parser saw, so `VolumeTexture` does
		 * not turn into `Texture3D` and a user struct keeps its case; the synthesis below only
		 * covers a tree built by hand (the decompiler, a test) that left Name empty.
		 */
		static FString PrintType(const FTypeRef& Type)
		{
			if (!Type.Name.IsEmpty())
			{
				return Type.Name;
			}

			switch (Type.Category)
			{
			case ETypeCategory::Void:      return TEXT("void");
			case ETypeCategory::Scalar:    return GetScalarSpelling(Type.Scalar);
			case ETypeCategory::Vector:    return FString::Printf(TEXT("%s%d"), GetScalarSpelling(Type.Scalar), Type.Rows);
			case ETypeCategory::Matrix:    return FString::Printf(TEXT("%s%dx%d"), GetScalarSpelling(Type.Scalar), Type.Rows, Type.Cols);
			case ETypeCategory::Texture:   return GetTextureSpelling(Type.Texture);
			case ETypeCategory::Sampler:   return TEXT("SamplerState");
			case ETypeCategory::Material:  return TEXT("material");
			case ETypeCategory::Substrate: return TEXT("Substrate");
			default:                       return FString();
			}
		}

		/** The six escapes the lexer resolves, put back. Anything else is written as it stands. */
		static FString EscapeStringLiteral(const FString& Value)
		{
			FString Result;
			Result.Reserve(Value.Len() + 8);
			for (int32 Index = 0; Index < Value.Len(); ++Index)
			{
				const TCHAR Char = Value[Index];
				switch (Char)
				{
				case TEXT('\\'): Result += TEXT("\\\\"); break;
				case TEXT('\"'): Result += TEXT("\\\""); break;
				case TEXT('\n'): Result += TEXT("\\n");  break;
				case TEXT('\r'): Result += TEXT("\\r");  break;
				case TEXT('\t'): Result += TEXT("\\t");  break;
				case TEXT('\0'): Result += TEXT("\\0");  break;
				default:         Result.AppendChar(Char); break;
				}
			}
			return Result;
		}

		static FString QuoteString(const FString& Value)
		{
			return FString::Printf(TEXT("\"%s\""), *EscapeStringLiteral(Value));
		}

		// --------------------------------------------------------------------------------------
		// Expressions
		// --------------------------------------------------------------------------------------

		/**
		 * The precedence a node occupies as somebody else's operand.
		 *
		 * Operator nodes defer to GetExprPrecedence (the shared table). The atoms are spelled out
		 * here because the printer must not depend on how a not-yet-written table classifies the
		 * two kinds LangOperators.h does not name -- FTypeExpr and FInitializerListExpr -- and
		 * because a wrong answer there would turn a constructor call into a cast.
		 */
		static int32 GetOperandPrecedence(const FExpr& Expr)
		{
			switch (Expr.Kind)
			{
			case ENodeKind::LiteralExpr:
			case ENodeKind::IdentifierExpr:
			case ENodeKind::TypeExpr:
			case ENodeKind::MemberExpr:
			case ENodeKind::IndexExpr:
			case ENodeKind::CallExpr:
			case ENodeKind::InitializerListExpr:
			case ENodeKind::ParenExpr:
				return PostfixPrecedence + 1;
			default:
				return GetExprPrecedence(Expr);
			}
		}

		static FString PrintExpression(const FExpr& Expr);

		static FString PrintExpressionOrEmpty(const FExprPtr& Expr)
		{
			return Expr ? PrintExpression(*Expr) : FString();
		}

		/**
		 * An operand, parenthesised when it does not bind at least as tightly as MinPrecedence.
		 *
		 * Callers pass P for the left operand of a left-associative operator of precedence P and
		 * P + 1 for its right operand, which is what keeps `a - (b - c)` from collapsing into
		 * `a - b - c`; a right-associative operator (assignment, `?:`) does the opposite.
		 */
		static FString PrintOperand(const FExpr* Expr, int32 MinPrecedence)
		{
			if (!Expr)
			{
				return FString();
			}

			const FString Text = PrintExpression(*Expr);
			if (GetOperandPrecedence(*Expr) < MinPrecedence)
			{
				return FString::Printf(TEXT("(%s)"), *Text);
			}
			return Text;
		}

		static FString PrintArrayDimensions(const TArray<FExprPtr>& Dimensions)
		{
			FString Result;
			for (const FExprPtr& Dimension : Dimensions)
			{
				// A null entry is an unsized `[]`, which is how the parser records it.
				Result += TEXT("[");
				Result += PrintExpressionOrEmpty(Dimension);
				Result += TEXT("]");
			}
			return Result;
		}

		static FString PrintLiteral(const FLiteralExpr& Literal)
		{
			switch (Literal.LiteralKind)
			{
			case ELiteralKind::String:
				// Text is the RESOLVED value for a string, so it has to be escaped again.
				return QuoteString(Literal.Text);

			case ELiteralKind::Bool:
				return Literal.Text.IsEmpty() ? FString(Literal.bBool ? TEXT("true") : TEXT("false")) : Literal.Text;

			case ELiteralKind::UInt:
				// Text is the lexeme as written (`1.0f`, `0x10`, `2u`) precisely so it survives.
				return Literal.Text.IsEmpty() ? FString::Printf(TEXT("%lluu"), Literal.Integer) : Literal.Text;

			case ELiteralKind::Int:
				return Literal.Text.IsEmpty() ? FString::Printf(TEXT("%llu"), Literal.Integer) : Literal.Text;

			case ELiteralKind::Float:
			default:
				return Literal.Text.IsEmpty() ? FString::SanitizeFloat(Literal.Real) : Literal.Text;
			}
		}

		static bool IsSignCharacter(TCHAR Char)
		{
			return Char == TEXT('+') || Char == TEXT('-');
		}

		static FString PrintUnary(const FUnaryExpr& Unary)
		{
			bool bPostfix = false;
			const TCHAR* Spelling = GetUnaryOpSpelling(Unary.Op, bPostfix);
			if (Spelling == nullptr)
			{
				Spelling = TEXT("");
			}

			if (bPostfix)
			{
				return PrintOperand(Unary.Operand.Get(), PostfixPrecedence) + Spelling;
			}

			const FString Operand = PrintOperand(Unary.Operand.Get(), UnaryPrecedence);

			// `-` immediately before `-x` would lex back as one `--` token. A space separates the
			// two without inventing a parenthesis node the tree never had.
			const int32 SpellingLen = FCString::Strlen(Spelling);
			const bool bWouldGlue =
				SpellingLen > 0 && Operand.Len() > 0 &&
				IsSignCharacter(Spelling[SpellingLen - 1]) && IsSignCharacter(Operand[0]);

			FString Result = Spelling;
			if (bWouldGlue)
			{
				Result += TEXT(" ");
			}
			Result += Operand;
			return Result;
		}

		static FString PrintCall(const FCallExpr& Call)
		{
			FString Result;
			if (Call.Callee)
			{
				// A constructor's callee is a type. It is an atom by construction and must never be
				// wrapped, or `float3(1, 2, 3)` would print as the cast `(float3)(1, 2, 3)`.
				Result = (Call.Callee->Kind == ENodeKind::TypeExpr)
					? PrintType(static_cast<const FTypeExpr&>(*Call.Callee).Type)
					: PrintOperand(Call.Callee.Get(), PostfixPrecedence);
			}

			Result += TEXT("(");
			for (int32 ArgumentIndex = 0; ArgumentIndex < Call.Arguments.Num(); ++ArgumentIndex)
			{
				const FArgument& Argument = Call.Arguments[ArgumentIndex];
				if (ArgumentIndex > 0)
				{
					Result += TEXT(", ");
				}

				if (!Argument.Name.IsEmpty())
				{
					Result += Argument.Name;
					Result += TEXT(" = ");
					Result += PrintOperand(Argument.Value.Get(), AssignmentPrecedence);
				}
				else
				{
					// A positional argument that IS an assignment keeps its parentheses: without
					// them the next parse would read `f(a = b)` as a named argument.
					Result += PrintOperand(Argument.Value.Get(), AssignmentPrecedence + 1);
				}
			}
			Result += TEXT(")");
			return Result;
		}

		static FString PrintExpression(const FExpr& Expr)
		{
			switch (Expr.Kind)
			{
			case ENodeKind::LiteralExpr:
				return PrintLiteral(static_cast<const FLiteralExpr&>(Expr));

			case ENodeKind::IdentifierExpr:
				return static_cast<const FIdentifierExpr&>(Expr).Name;

			case ENodeKind::TypeExpr:
				return PrintType(static_cast<const FTypeExpr&>(Expr).Type);

			case ENodeKind::MemberExpr:
			{
				const FMemberExpr& Member = static_cast<const FMemberExpr&>(Expr);
				FString Result = PrintOperand(Member.Object.Get(), PostfixPrecedence);
				Result += TEXT(".");
				Result += Member.Member;
				return Result;
			}

			case ENodeKind::IndexExpr:
			{
				const FIndexExpr& IndexExpr = static_cast<const FIndexExpr&>(Expr);
				FString Result = PrintOperand(IndexExpr.Object.Get(), PostfixPrecedence);
				Result += TEXT("[");
				Result += PrintExpressionOrEmpty(IndexExpr.Index);
				Result += TEXT("]");
				return Result;
			}

			case ENodeKind::CallExpr:
				return PrintCall(static_cast<const FCallExpr&>(Expr));

			case ENodeKind::UnaryExpr:
				return PrintUnary(static_cast<const FUnaryExpr&>(Expr));

			case ENodeKind::BinaryExpr:
			{
				const FBinaryExpr& Binary = static_cast<const FBinaryExpr&>(Expr);
				const int32 Precedence = GetBinaryPrecedence(Binary.Op);

				FString Result = PrintOperand(Binary.Left.Get(), Precedence);
				Result += TEXT(" ");
				Result += GetBinaryOpSpelling(Binary.Op);
				Result += TEXT(" ");
				// Left-associative: an equal-precedence right operand needs its parentheses.
				Result += PrintOperand(Binary.Right.Get(), Precedence + 1);
				return Result;
			}

			case ENodeKind::AssignExpr:
			{
				const FAssignExpr& Assign = static_cast<const FAssignExpr&>(Expr);

				// Right-associative: the LEFT side is the one that needs parentheses when equal.
				FString Result = PrintOperand(Assign.Target.Get(), AssignmentPrecedence + 1);
				Result += TEXT(" ");
				Result += GetAssignOpSpelling(Assign.Op);
				Result += TEXT(" ");
				Result += PrintOperand(Assign.Value.Get(), AssignmentPrecedence);
				return Result;
			}

			case ENodeKind::ConditionalExpr:
			{
				const FConditionalExpr& Conditional = static_cast<const FConditionalExpr&>(Expr);

				FString Result = PrintOperand(Conditional.Condition.Get(), ConditionalPrecedence + 1);
				Result += TEXT(" ? ");
				// The middle operand is fenced in by `?` and `:`, so it is a full expression and
				// nothing in it ever needs parenthesising -- not even an assignment, which the
				// parser accepts there (`a ? b = c : d`) and would have to re-read as a FParenExpr
				// if this wrapped it.
				Result += PrintOperand(Conditional.TrueValue.Get(), AssignmentPrecedence);
				Result += TEXT(" : ");
				// Right-associative: a nested conditional here needs no parentheses; an assignment does.
				Result += PrintOperand(Conditional.FalseValue.Get(), ConditionalPrecedence);
				return Result;
			}

			case ENodeKind::CastExpr:
			{
				const FCastExpr& Cast = static_cast<const FCastExpr&>(Expr);
				return FString::Printf(
					TEXT("(%s)%s"),
					*PrintType(Cast.Type),
					*PrintOperand(Cast.Operand.Get(), UnaryPrecedence));
			}

			case ENodeKind::InitializerListExpr:
			{
				const FInitializerListExpr& List = static_cast<const FInitializerListExpr&>(Expr);
				if (List.Elements.Num() == 0)
				{
					return TEXT("{ }");
				}

				FString Result = TEXT("{ ");
				for (int32 ElementIndex = 0; ElementIndex < List.Elements.Num(); ++ElementIndex)
				{
					if (ElementIndex > 0)
					{
						Result += TEXT(", ");
					}
					Result += PrintExpressionOrEmpty(List.Elements[ElementIndex]);
				}
				Result += TEXT(" }");
				return Result;
			}

			case ENodeKind::ParenExpr:
			{
				const FParenExpr& Paren = static_cast<const FParenExpr&>(Expr);
				return FString::Printf(TEXT("(%s)"), *PrintExpressionOrEmpty(Paren.Inner));
			}

			default:
				// A statement or declaration node handed in as an expression: nothing sensible to
				// print, and the printer never fails.
				return FString();
			}
		}

		// --------------------------------------------------------------------------------------
		// Statements and declarations
		// --------------------------------------------------------------------------------------

		/** Accumulates lines; every Append* method leaves the buffer at the start of a fresh line. */
		class FLangPrinter
		{
		public:
			explicit FLangPrinter(const FLangPrintOptions& InOptions)
				: Options(InOptions)
			{
			}

			void PrintModule(const FModule& Module);
			void PrintDeclaration(const FDecl& Decl, int32 IndentLevel);
			void PrintStatement(const FStmt& Stmt, int32 IndentLevel);

			FString TakeText() { return MoveTemp(Out); }

		private:
			void PrintDocBlock(const FDocBlock& Doc, int32 IndentLevel);
			void PrintVariableDecl(const FVariableDecl& Decl, int32 IndentLevel);
			void PrintFunctionDecl(const FFunctionDecl& Decl, int32 IndentLevel);
			void PrintStructDecl(const FStructDecl& Decl, int32 IndentLevel);
			void PrintIncludeDecl(const FIncludeDecl& Decl, int32 IndentLevel);
			void PrintPragmaDecl(const FPragmaDecl& Decl, int32 IndentLevel);

			void PrintBlock(const FBlockStmt& Block, int32 IndentLevel);
			/** A loop / branch body, braced even when the tree holds a single statement. */
			void PrintBody(const FStmt* Body, int32 IndentLevel);
			void PrintIf(const FIfStmt& Stmt, int32 IndentLevel);

			/** A statement as one fragment with no indent, no terminator and no newline (a `for` init). */
			static FString PrintStatementFragment(const FStmt& Stmt);
			static FString PrintVarDeclFragment(const FVarDeclStmt& Stmt);
			static FString PrintDeclarator(const FDeclarator& Declarator);
			static FString PrintParam(const FParam& Param);
			static FString PrintPragmaArgument(const FPragmaArgument& Argument);

			FString MakeIndent(int32 IndentLevel) const;
			void AppendLine(int32 IndentLevel, const FString& Text);
			void AppendBlankLine() { Out += Options.NewLine; }

			const FLangPrintOptions& Options;
			FString Out;
		};

		FString FLangPrinter::MakeIndent(int32 IndentLevel) const
		{
			FString Result;
			for (int32 Level = 0; Level < IndentLevel; ++Level)
			{
				Result += Options.Indent;
			}
			return Result;
		}

		void FLangPrinter::AppendLine(int32 IndentLevel, const FString& Text)
		{
			// An empty line carries no indentation: trailing whitespace is not something a printer
			// should invent.
			if (!Text.IsEmpty())
			{
				Out += MakeIndent(FMath::Max(IndentLevel, 0));
				Out += Text;
			}
			Out += Options.NewLine;
		}

		void FLangPrinter::PrintModule(const FModule& Module)
		{
			bool bFirst = true;
			for (const FDeclPtr& Decl : Module.Declarations)
			{
				if (!Decl)
				{
					continue;
				}

				if (!bFirst && Options.bBlankLineBetweenDeclarations)
				{
					// The blank line goes BEFORE the next declaration's `///` block, which is what
					// keeps that block glued to the declaration it documents.
					AppendBlankLine();
				}
				bFirst = false;

				PrintDeclaration(*Decl, 0);
			}
		}

		void FLangPrinter::PrintDeclaration(const FDecl& Decl, int32 IndentLevel)
		{
			PrintDocBlock(Decl.Doc, IndentLevel);

			switch (Decl.Kind)
			{
			case ENodeKind::VariableDecl:
				PrintVariableDecl(static_cast<const FVariableDecl&>(Decl), IndentLevel);
				break;
			case ENodeKind::FunctionDecl:
				PrintFunctionDecl(static_cast<const FFunctionDecl&>(Decl), IndentLevel);
				break;
			case ENodeKind::StructDecl:
				PrintStructDecl(static_cast<const FStructDecl&>(Decl), IndentLevel);
				break;
			case ENodeKind::IncludeDecl:
				PrintIncludeDecl(static_cast<const FIncludeDecl&>(Decl), IndentLevel);
				break;
			case ENodeKind::PragmaDecl:
				PrintPragmaDecl(static_cast<const FPragmaDecl&>(Decl), IndentLevel);
				break;
			default:
				break;
			}
		}

		void FLangPrinter::PrintDocBlock(const FDocBlock& Doc, int32 IndentLevel)
		{
			for (const FString& Text : Doc.FreeText)
			{
				// Trimmed on the way out because the parser trims on the way in. An EMPTY entry is
				// NOT dropped: the parser records a blank `///` line deliberately, as the paragraph
				// break of a description, so dropping it here would glue two paragraphs together on
				// the way back out. A bare `///` -- no trailing space to leave behind -- reads back
				// as exactly that empty free-text line.
				const FString Trimmed = Text.TrimStartAndEnd();
				AppendLine(IndentLevel, Trimmed.IsEmpty() ? FString(TEXT("///")) : FString(TEXT("/// ")) + Trimmed);
			}

			for (const FDocDirective& Directive : Doc.Directives)
			{
				// One directive per line: a value runs to the next " @", so a line that carried
				// several directives cannot be reassembled without risking a different split.
				FString Line = TEXT("/// @");
				Line += Directive.Key;

				const FString Value = Directive.Value.TrimStartAndEnd();
				if (!Value.IsEmpty())
				{
					Line += TEXT(" ");
					Line += Value;
				}
				AppendLine(IndentLevel, Line);
			}
		}

		FString FLangPrinter::PrintDeclarator(const FDeclarator& Declarator)
		{
			FString Result = Declarator.Name;
			Result += PrintArrayDimensions(Declarator.ArrayDimensions);
			if (Declarator.Initializer)
			{
				Result += TEXT(" = ");
				Result += PrintExpression(*Declarator.Initializer);
			}
			return Result;
		}

		void FLangPrinter::PrintVariableDecl(const FVariableDecl& Decl, int32 IndentLevel)
		{
			FString Line = GetStorageSpelling(Decl.Storage);
			Line += PrintType(Decl.Type);
			Line += TEXT(" ");
			Line += PrintDeclarator(Decl.Declarator);
			Line += TEXT(";");
			AppendLine(IndentLevel, Line);
		}

		FString FLangPrinter::PrintParam(const FParam& Param)
		{
			FString Result = GetDirectionSpelling(Param.Direction);
			Result += PrintType(Param.Type);
			if (!Param.Name.IsEmpty())
			{
				Result += TEXT(" ");
				Result += Param.Name;
			}
			Result += PrintArrayDimensions(Param.ArrayDimensions);
			if (Param.Default)
			{
				Result += TEXT(" = ");
				Result += PrintExpression(*Param.Default);
			}
			return Result;
		}

		void FLangPrinter::PrintFunctionDecl(const FFunctionDecl& Decl, int32 IndentLevel)
		{
			FString Signature = GetLinkageSpelling(Decl.Linkage);
			Signature += PrintType(Decl.ReturnType);
			Signature += TEXT(" ");
			Signature += Decl.Name;
			Signature += TEXT("(");
			for (int32 ParamIndex = 0; ParamIndex < Decl.Params.Num(); ++ParamIndex)
			{
				if (ParamIndex > 0)
				{
					Signature += TEXT(", ");
				}
				Signature += PrintParam(Decl.Params[ParamIndex]);
			}
			Signature += TEXT(")");

			if (Decl.IsPrototype())
			{
				// `extern float2 MF_UVChannelSwitch(float UVChannelIndex);`
				Signature += TEXT(";");
				AppendLine(IndentLevel, Signature);
				return;
			}

			AppendLine(IndentLevel, Signature);

			if (Decl.bOpaqueBody)
			{
				// A `/// @custom` body is HLSL handed to the shader compiler, not DreamShaderLang:
				// braces back around the captured text, byte for byte, no re-indentation. RawBody
				// carries its own line terminators, which is why this bypasses AppendLine.
				Out += MakeIndent(FMath::Max(IndentLevel, 0));
				Out += TEXT("{");
				Out += Decl.RawBody;
				Out += TEXT("}");
				Out += Options.NewLine;
				return;
			}

			PrintBlock(*Decl.Body, IndentLevel);
		}

		void FLangPrinter::PrintStructDecl(const FStructDecl& Decl, int32 IndentLevel)
		{
			AppendLine(IndentLevel, FString(TEXT("struct ")) + Decl.Name);
			AppendLine(IndentLevel, TEXT("{"));

			for (const FStructField& Field : Decl.Fields)
			{
				PrintDocBlock(Field.Doc, IndentLevel + 1);

				FString Line = PrintType(Field.Type);
				if (!Field.Name.IsEmpty())
				{
					Line += TEXT(" ");
					Line += Field.Name;
				}
				Line += PrintArrayDimensions(Field.ArrayDimensions);
				Line += TEXT(";");
				AppendLine(IndentLevel + 1, Line);
			}

			AppendLine(IndentLevel, TEXT("};"));
		}

		void FLangPrinter::PrintIncludeDecl(const FIncludeDecl& Decl, int32 IndentLevel)
		{
			if (Decl.bImportSpelling)
			{
				// `import "path";` -- the path arrived as a string literal, so it is re-escaped.
				AppendLine(IndentLevel, FString::Printf(TEXT("import %s;"), *QuoteString(Decl.Path)));
				return;
			}

			// `#include "path"` -- the path arrived inside a Directive token, which the lexer does
			// not unescape; writing it back verbatim is the exact inverse.
			AppendLine(IndentLevel, FString::Printf(TEXT("#include \"%s\""), *Decl.Path));
		}

		FString FLangPrinter::PrintPragmaArgument(const FPragmaArgument& Argument)
		{
			const FString Value = Argument.bQuoted ? QuoteString(Argument.Value) : Argument.Value;

			if (Argument.Key.IsEmpty())
			{
				// The positional selector of `#pragma layout(Node, ...)`.
				return Value;
			}
			if (Value.IsEmpty())
			{
				// A bare word that was recorded as a key with no value.
				return Argument.Key;
			}
			return Argument.Key + TEXT(" = ") + Value;
		}

		void FLangPrinter::PrintPragmaDecl(const FPragmaDecl& Decl, int32 IndentLevel)
		{
			FString Line = TEXT("#pragma");

			const FString Name = Decl.Name.IsEmpty() ? FString(GetDefaultPragmaName(Decl.PragmaKind)) : Decl.Name;
			if (!Name.IsEmpty())
			{
				Line += TEXT(" ");
				Line += Name;
			}

			switch (Decl.PragmaKind)
			{
			case EPragmaKind::Material:
			case EPragmaKind::Layout:
			{
				Line += TEXT("(");
				for (int32 ArgumentIndex = 0; ArgumentIndex < Decl.Arguments.Num(); ++ArgumentIndex)
				{
					if (ArgumentIndex > 0)
					{
						Line += TEXT(", ");
					}
					Line += PrintPragmaArgument(Decl.Arguments[ArgumentIndex]);
				}
				Line += TEXT(")");
				break;
			}

			case EPragmaKind::Region:
			case EPragmaKind::EndRegion:
			case EPragmaKind::Unknown:
			default:
			{
				// Region: the title. Unknown: the rest of the line, kept as it was written.
				const FString Text = Decl.Text.TrimStartAndEnd();
				if (!Text.IsEmpty())
				{
					Line += TEXT(" ");
					Line += Text;
				}
				break;
			}
			}

			AppendLine(IndentLevel, Line);
		}

		FString FLangPrinter::PrintVarDeclFragment(const FVarDeclStmt& Stmt)
		{
			FString Result = GetStorageSpelling(Stmt.Storage);
			Result += PrintType(Stmt.Type);
			for (int32 DeclaratorIndex = 0; DeclaratorIndex < Stmt.Declarators.Num(); ++DeclaratorIndex)
			{
				if (DeclaratorIndex == 0)
				{
					Result += TEXT(" ");
				}
				else
				{
					Result += TEXT(", ");
				}
				Result += PrintDeclarator(Stmt.Declarators[DeclaratorIndex]);
			}
			return Result;
		}

		FString FLangPrinter::PrintStatementFragment(const FStmt& Stmt)
		{
			switch (Stmt.Kind)
			{
			case ENodeKind::VarDeclStmt:
				return PrintVarDeclFragment(static_cast<const FVarDeclStmt&>(Stmt));
			case ENodeKind::ExprStmt:
				return PrintExpressionOrEmpty(static_cast<const FExprStmt&>(Stmt).Expression);
			default:
				return FString();
			}
		}

		void FLangPrinter::PrintBlock(const FBlockStmt& Block, int32 IndentLevel)
		{
			AppendLine(IndentLevel, TEXT("{"));
			for (const FStmtPtr& Statement : Block.Statements)
			{
				if (Statement)
				{
					PrintStatement(*Statement, IndentLevel + 1);
				}
			}
			AppendLine(IndentLevel, TEXT("}"));
		}

		void FLangPrinter::PrintBody(const FStmt* Body, int32 IndentLevel)
		{
			if (Body && Body->Kind == ENodeKind::BlockStmt)
			{
				PrintBlock(static_cast<const FBlockStmt&>(*Body), IndentLevel);
				return;
			}

			// A single statement as a branch/loop body gets braces anyway. It changes the tree by
			// one FBlockStmt and it costs nothing structurally; what it buys is that a later edit
			// to the body cannot silently fall out of the branch.
			AppendLine(IndentLevel, TEXT("{"));
			if (Body)
			{
				PrintStatement(*Body, IndentLevel + 1);
			}
			AppendLine(IndentLevel, TEXT("}"));
		}

		void FLangPrinter::PrintIf(const FIfStmt& Stmt, int32 IndentLevel)
		{
			const FIfStmt* Current = &Stmt;
			const TCHAR* Keyword = TEXT("if");

			for (;;)
			{
				AppendLine(IndentLevel, FString::Printf(TEXT("%s (%s)"), Keyword, *PrintExpressionOrEmpty(Current->Condition)));
				PrintBody(Current->Then.Get(), IndentLevel);

				const FStmt* Else = Current->Else.Get();
				if (!Else)
				{
					return;
				}

				// A chain stays a chain: `else` + a lone if prints as `else if (...)` rather than
				// as a nested block, so the indentation does not walk off the right of the file.
				if (Else->Kind == ENodeKind::IfStmt)
				{
					Current = static_cast<const FIfStmt*>(Else);
					Keyword = TEXT("else if");
					continue;
				}

				AppendLine(IndentLevel, TEXT("else"));
				PrintBody(Else, IndentLevel);
				return;
			}
		}

		void FLangPrinter::PrintStatement(const FStmt& Stmt, int32 IndentLevel)
		{
			switch (Stmt.Kind)
			{
			case ENodeKind::VarDeclStmt:
				AppendLine(IndentLevel, PrintVarDeclFragment(static_cast<const FVarDeclStmt&>(Stmt)) + TEXT(";"));
				break;

			case ENodeKind::ExprStmt:
				AppendLine(IndentLevel, PrintExpressionOrEmpty(static_cast<const FExprStmt&>(Stmt).Expression) + TEXT(";"));
				break;

			case ENodeKind::BlockStmt:
				PrintBlock(static_cast<const FBlockStmt&>(Stmt), IndentLevel);
				break;

			case ENodeKind::IfStmt:
				PrintIf(static_cast<const FIfStmt&>(Stmt), IndentLevel);
				break;

			case ENodeKind::ForStmt:
			{
				const FForStmt& For = static_cast<const FForStmt&>(Stmt);

				// The init declaration is printed as a fragment: the `;` separators below are the
				// loop header's, not the declaration's.
				FString Header = TEXT("for (");
				if (For.Init)
				{
					Header += PrintStatementFragment(*For.Init);
				}
				Header += TEXT(";");
				if (For.Condition)
				{
					Header += TEXT(" ");
					Header += PrintExpression(*For.Condition);
				}
				Header += TEXT(";");
				if (For.Step)
				{
					Header += TEXT(" ");
					Header += PrintExpression(*For.Step);
				}
				Header += TEXT(")");

				AppendLine(IndentLevel, Header);
				PrintBody(For.Body.Get(), IndentLevel);
				break;
			}

			case ENodeKind::WhileStmt:
			{
				const FWhileStmt& While = static_cast<const FWhileStmt&>(Stmt);
				AppendLine(IndentLevel, FString::Printf(TEXT("while (%s)"), *PrintExpressionOrEmpty(While.Condition)));
				PrintBody(While.Body.Get(), IndentLevel);
				break;
			}

			case ENodeKind::DoWhileStmt:
			{
				const FDoWhileStmt& DoWhile = static_cast<const FDoWhileStmt&>(Stmt);
				AppendLine(IndentLevel, TEXT("do"));
				PrintBody(DoWhile.Body.Get(), IndentLevel);
				AppendLine(IndentLevel, FString::Printf(TEXT("while (%s);"), *PrintExpressionOrEmpty(DoWhile.Condition)));
				break;
			}

			case ENodeKind::ReturnStmt:
			{
				const FReturnStmt& Return = static_cast<const FReturnStmt&>(Stmt);
				AppendLine(
					IndentLevel,
					Return.Value
						? FString::Printf(TEXT("return %s;"), *PrintExpression(*Return.Value))
						: FString(TEXT("return;")));
				break;
			}

			case ENodeKind::BreakStmt:
				AppendLine(IndentLevel, TEXT("break;"));
				break;

			case ENodeKind::ContinueStmt:
				AppendLine(IndentLevel, TEXT("continue;"));
				break;

			case ENodeKind::DiscardStmt:
				AppendLine(IndentLevel, TEXT("discard;"));
				break;

			case ENodeKind::EmptyStmt:
				AppendLine(IndentLevel, TEXT(";"));
				break;

			default:
				break;
			}
		}

		/** Drops the one trailing terminator the line-oriented printer always leaves behind. */
		static void RemoveTrailingNewLine(FString& Text, const FString& NewLine)
		{
			if (!NewLine.IsEmpty() && Text.EndsWith(NewLine, ESearchCase::CaseSensitive))
			{
				Text.LeftChopInline(NewLine.Len());
			}
		}
	}

	// ------------------------------------------------------------------------------------------
	// Public entry points
	// ------------------------------------------------------------------------------------------

	FString PrintDreamShaderLang(const FModule& Module, const FLangPrintOptions& Options)
	{
		Private::Printer::FLangPrinter Printer(Options);
		Printer.PrintModule(Module);
		// A whole file ends with exactly one terminator after its last declaration.
		return Printer.TakeText();
	}

	FString PrintDreamShaderLangDecl(const FDecl& Decl, const FLangPrintOptions& Options)
	{
		Private::Printer::FLangPrinter Printer(Options);
		Printer.PrintDeclaration(Decl, 0);

		// A single node is a value, not a file: no trailing terminator, so callers can embed it.
		FString Text = Printer.TakeText();
		Private::Printer::RemoveTrailingNewLine(Text, Options.NewLine);
		return Text;
	}

	FString PrintDreamShaderLangStmt(const FStmt& Stmt, const FLangPrintOptions& Options, int32 IndentLevel)
	{
		Private::Printer::FLangPrinter Printer(Options);
		Printer.PrintStatement(Stmt, IndentLevel);

		FString Text = Printer.TakeText();
		Private::Printer::RemoveTrailingNewLine(Text, Options.NewLine);
		return Text;
	}

	FString PrintDreamShaderLangExpr(const FExpr& Expr)
	{
		return Private::Printer::PrintExpression(Expr);
	}

	FString PrintDreamShaderLangType(const FTypeRef& Type)
	{
		return Private::Printer::PrintType(Type);
	}
}
