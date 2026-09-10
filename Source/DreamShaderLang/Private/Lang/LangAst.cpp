// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The AST is a header of plain structs; the only thing it needs a translation unit for is the
// node-kind name table. It is spelled once, here, so the printer, the diagnostics and the future
// `dsc dump-ast` all say the same word for the same node.

#include "Lang/LangAst.h"

namespace UE::DreamShader::Lang
{
	const TCHAR* LexToString(ENodeKind Kind)
	{
		switch (Kind)
		{
		// expressions
		case ENodeKind::LiteralExpr:         return TEXT("LiteralExpr");
		case ENodeKind::IdentifierExpr:      return TEXT("IdentifierExpr");
		case ENodeKind::TypeExpr:            return TEXT("TypeExpr");
		case ENodeKind::MemberExpr:          return TEXT("MemberExpr");
		case ENodeKind::IndexExpr:           return TEXT("IndexExpr");
		case ENodeKind::CallExpr:            return TEXT("CallExpr");
		case ENodeKind::UnaryExpr:           return TEXT("UnaryExpr");
		case ENodeKind::BinaryExpr:          return TEXT("BinaryExpr");
		case ENodeKind::AssignExpr:          return TEXT("AssignExpr");
		case ENodeKind::ConditionalExpr:     return TEXT("ConditionalExpr");
		case ENodeKind::CastExpr:            return TEXT("CastExpr");
		case ENodeKind::InitializerListExpr: return TEXT("InitializerListExpr");
		case ENodeKind::ParenExpr:           return TEXT("ParenExpr");

		// statements
		case ENodeKind::VarDeclStmt:         return TEXT("VarDeclStmt");
		case ENodeKind::ExprStmt:            return TEXT("ExprStmt");
		case ENodeKind::BlockStmt:           return TEXT("BlockStmt");
		case ENodeKind::IfStmt:              return TEXT("IfStmt");
		case ENodeKind::ForStmt:             return TEXT("ForStmt");
		case ENodeKind::WhileStmt:           return TEXT("WhileStmt");
		case ENodeKind::DoWhileStmt:         return TEXT("DoWhileStmt");
		case ENodeKind::ReturnStmt:          return TEXT("ReturnStmt");
		case ENodeKind::BreakStmt:           return TEXT("BreakStmt");
		case ENodeKind::ContinueStmt:        return TEXT("ContinueStmt");
		case ENodeKind::DiscardStmt:         return TEXT("DiscardStmt");
		case ENodeKind::EmptyStmt:           return TEXT("EmptyStmt");

		// declarations
		case ENodeKind::VariableDecl:        return TEXT("VariableDecl");
		case ENodeKind::FunctionDecl:        return TEXT("FunctionDecl");
		case ENodeKind::StructDecl:          return TEXT("StructDecl");
		case ENodeKind::IncludeDecl:         return TEXT("IncludeDecl");
		case ENodeKind::PragmaDecl:          return TEXT("PragmaDecl");
		}

		return TEXT("Unknown");
	}
}
