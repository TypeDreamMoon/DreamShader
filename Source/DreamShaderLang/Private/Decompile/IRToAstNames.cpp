// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// BuildDreamShaderAstFromIR: identifiers, name scopes and the small AST factories the other units build with.
//
// Nothing here knows about graphs. The factories make nodes with empty spans -- a decompiled tree has no source
// yet -- and never a second spelling of something the module already spells: literals go through the legacy front
// end's makers (LangLegacyExpressions.cpp), floats through FormatDreamShaderFloatLiteral, identifiers through
// MakeDreamShaderIdentifier.

#include "IRToAstInternal.h"

#include "IR/IRCoreOps.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangParserInternal.h"
#include "Lang/LangToken.h"
#include "Semantic/LangBound.h"

namespace UE::DreamShader::Lang::DecompileAst
{
	// ------------------------------------------------------------------------------------ names

	bool IsIdentifierText(const FString& Text)
	{
		if (Text.IsEmpty())
		{
			return false;
		}
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Character = Text[Index];
			const bool bLetter = (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('a') && Character <= TEXT('z'))
				|| Character == TEXT('_');
			const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
			if (!bLetter && !(bDigit && Index > 0))
			{
				return false;
			}
		}
		return true;
	}

	bool IsReservedIdentifier(const FString& Text)
	{
		ELangKeyword Keyword = ELangKeyword::None;
		if (TryGetLangKeyword(Text, Keyword) || Private::FLangParser::IsBuiltinTypeName(Text))
		{
			return true;
		}
		if (Text.Equals(TEXT("UE"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("true"), ESearchCase::CaseSensitive)
			|| Text.Equals(TEXT("false"), ESearchCase::CaseSensitive))
		{
			return true;
		}
		// `lerp` as a variable would read, but the call `lerp(...)` next to it would not read well.
		return IR::FindCoreOpByHlslName(Text) != nullptr || IR::FindCoreOpByGlslAlias(Text) != nullptr;
	}

	FString MakeSourceIdentifier(const FString& Text, const TCHAR* Fallback)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return FString(Fallback);
		}

		FString Result = MakeDreamShaderIdentifier(Trimmed);

		bool bOnlyUnderscores = true;
		for (const TCHAR Character : Result)
		{
			if (Character != TEXT('_'))
			{
				bOnlyUnderscores = false;
				break;
			}
		}
		if (bOnlyUnderscores)
		{
			return FString(Fallback);
		}

		if (IsReservedIdentifier(Result))
		{
			Result += TEXT("_");
		}
		return Result;
	}

	bool FNameScope::IsTaken(const FString& Name) const
	{
		for (const FNameScope* Scope = this; Scope != nullptr; Scope = Scope->Parent)
		{
			if (Scope->Taken.Contains(Name))
			{
				return true;
			}
		}
		return false;
	}

	void FNameScope::Reserve(const FString& Name)
	{
		Taken.Add(Name);
	}

	FString FNameScope::Claim(const FString& Wanted)
	{
		FString Candidate = Wanted;
		for (int32 Suffix = 2; IsTaken(Candidate); ++Suffix)
		{
			Candidate = FString::Printf(TEXT("%s_%d"), *Wanted, Suffix);
		}
		Taken.Add(Candidate);
		return Candidate;
	}

	// ------------------------------------------------------------------------------ expressions

	FExprPtr MakeIdentifierExpr(const FString& Name)
	{
		return Private::LegacyAst::MakeIdentifier(Name, FLangSpan());
	}

	FExprPtr MakeNumberExpr(const double Value)
	{
		return Private::LegacyAst::MakeFloatLiteral(FormatDreamShaderFloatLiteral(Value), Value, FLangSpan());
	}

	FExprPtr MakeIntegerExpr(const int64 Value)
	{
		return Private::LegacyAst::MakeIntLiteral(Value, FLangSpan());
	}

	FExprPtr MakeBoolExpr(const bool bValue)
	{
		return Private::LegacyAst::MakeBoolLiteral(bValue, FLangSpan());
	}

	FExprPtr MakeStringExpr(const FString& Value)
	{
		return Private::LegacyAst::MakeStringLiteral(Value, FLangSpan());
	}

	FExprPtr MakeMemberExpr(FExprPtr Object, const FString& Member)
	{
		return Private::LegacyAst::MakeMember(MoveTemp(Object), Member, FLangSpan());
	}

	FExprPtr MakeIndexExpr(FExprPtr Object, const int32 Index)
	{
		TUniquePtr<FIndexExpr> Node = MakeUnique<FIndexExpr>();
		Node->Object = MoveTemp(Object);
		Node->Index = MakeIntegerExpr(Index);
		return Node;
	}

	FExprPtr MakeUnaryExpr(const EUnaryOp Op, FExprPtr Operand)
	{
		TUniquePtr<FUnaryExpr> Node = MakeUnique<FUnaryExpr>();
		Node->Op = Op;
		Node->Operand = MoveTemp(Operand);
		return Node;
	}

	FExprPtr MakeBinaryExpr(const EBinaryOp Op, FExprPtr Left, FExprPtr Right)
	{
		TUniquePtr<FBinaryExpr> Node = MakeUnique<FBinaryExpr>();
		Node->Op = Op;
		Node->Left = MoveTemp(Left);
		Node->Right = MoveTemp(Right);
		return Node;
	}

	FExprPtr MakeConditionalExpr(FExprPtr Condition, FExprPtr TrueValue, FExprPtr FalseValue)
	{
		TUniquePtr<FConditionalExpr> Node = MakeUnique<FConditionalExpr>();
		Node->Condition = MoveTemp(Condition);
		Node->TrueValue = MoveTemp(TrueValue);
		Node->FalseValue = MoveTemp(FalseValue);
		return Node;
	}

	FExprPtr MakeCastExpr(const FTypeRef& Type, FExprPtr Operand)
	{
		TUniquePtr<FCastExpr> Node = MakeUnique<FCastExpr>();
		Node->Type = Type;
		Node->Operand = MoveTemp(Operand);
		return Node;
	}

	FExprPtr MakeAssignExpr(FExprPtr Target, FExprPtr Value)
	{
		TUniquePtr<FAssignExpr> Node = MakeUnique<FAssignExpr>();
		Node->Op = EAssignOp::Assign;
		Node->Target = MoveTemp(Target);
		Node->Value = MoveTemp(Value);
		return Node;
	}

	TUniquePtr<FCallExpr> MakeCallExpr(FExprPtr Callee)
	{
		TUniquePtr<FCallExpr> Node = MakeUnique<FCallExpr>();
		Node->Callee = MoveTemp(Callee);
		return Node;
	}

	FExprPtr MakeNamespaceCallee(const FString& Namespace, const FString& Name)
	{
		return Private::LegacyAst::MakeReflectedCallee(Namespace, Name, FLangSpan());
	}

	void AddPositionalArgument(FCallExpr& Call, FExprPtr Value)
	{
		FArgument Argument;
		Argument.Value = MoveTemp(Value);
		Call.Arguments.Add(MoveTemp(Argument));
	}

	void AddNamedArgument(FCallExpr& Call, const FString& Name, FExprPtr Value)
	{
		Call.Arguments.Add(Private::LegacyAst::MakeNamedArgument(Name, MoveTemp(Value), FLangSpan()));
	}

	void AddPinArgument(FCallExpr& Call, const int32 PinIndex, FExprPtr Value)
	{
		FArgument Argument;
		Argument.PinIndex = PinIndex;
		Argument.Value = MoveTemp(Value);
		Call.Arguments.Add(MoveTemp(Argument));
	}

	FExprPtr MakeConstructorExpr(const FTypeRef& Type, TArray<FExprPtr>&& Parts)
	{
		TUniquePtr<FTypeExpr> Callee = MakeUnique<FTypeExpr>();
		Callee->Type = Type;

		TUniquePtr<FCallExpr> Call = MakeCallExpr(MoveTemp(Callee));
		for (FExprPtr& Part : Parts)
		{
			AddPositionalArgument(*Call, MoveTemp(Part));
		}
		return Call;
	}

	FExprPtr MakeVectorLiteralExpr(const double* Values, const int32 Count, const bool bBool)
	{
		const int32 Width = FMath::Clamp(Count, 1, 4);
		if (Width == 1)
		{
			return bBool ? MakeBoolExpr(Values[0] != 0.0) : MakeNumberExpr(Values[0]);
		}

		TArray<FExprPtr> Parts;
		Parts.Reserve(Width);
		for (int32 Index = 0; Index < Width; ++Index)
		{
			Parts.Add(bBool ? MakeBoolExpr(Values[Index] != 0.0) : MakeNumberExpr(Values[Index]));
		}
		return MakeConstructorExpr(MakeValueTypeRef(bBool ? FIRType::Bool(Width) : FIRType::Float(Width), /* bKeepBool */ true), MoveTemp(Parts));
	}

	FExprPtr CloneExpr(const FExpr& Expr)
	{
		switch (Expr.Kind)
		{
		case ENodeKind::LiteralExpr:
		{
			const FLiteralExpr& Source = static_cast<const FLiteralExpr&>(Expr);
			TUniquePtr<FLiteralExpr> Copy = MakeUnique<FLiteralExpr>();
			Copy->LiteralKind = Source.LiteralKind;
			Copy->Text = Source.Text;
			Copy->Integer = Source.Integer;
			Copy->Real = Source.Real;
			Copy->bBool = Source.bBool;
			return Copy;
		}
		case ENodeKind::IdentifierExpr:
			return MakeIdentifierExpr(static_cast<const FIdentifierExpr&>(Expr).Name);
		case ENodeKind::TypeExpr:
		{
			TUniquePtr<FTypeExpr> Copy = MakeUnique<FTypeExpr>();
			Copy->Type = static_cast<const FTypeExpr&>(Expr).Type;
			return Copy;
		}
		case ENodeKind::MemberExpr:
		{
			const FMemberExpr& Source = static_cast<const FMemberExpr&>(Expr);
			return MakeMemberExpr(Source.Object ? CloneExpr(*Source.Object) : FExprPtr(), Source.Member);
		}
		case ENodeKind::IndexExpr:
		{
			const FIndexExpr& Source = static_cast<const FIndexExpr&>(Expr);
			TUniquePtr<FIndexExpr> Copy = MakeUnique<FIndexExpr>();
			Copy->Object = Source.Object ? CloneExpr(*Source.Object) : FExprPtr();
			Copy->Index = Source.Index ? CloneExpr(*Source.Index) : FExprPtr();
			return Copy;
		}
		case ENodeKind::CallExpr:
		{
			const FCallExpr& Source = static_cast<const FCallExpr&>(Expr);
			TUniquePtr<FCallExpr> Copy = MakeCallExpr(Source.Callee ? CloneExpr(*Source.Callee) : FExprPtr());
			for (const FArgument& Argument : Source.Arguments)
			{
				FArgument ArgumentCopy;
				ArgumentCopy.Name = Argument.Name;
				ArgumentCopy.PinIndex = Argument.PinIndex;
				ArgumentCopy.Value = Argument.Value ? CloneExpr(*Argument.Value) : FExprPtr();
				Copy->Arguments.Add(MoveTemp(ArgumentCopy));
			}
			return Copy;
		}
		case ENodeKind::UnaryExpr:
		{
			const FUnaryExpr& Source = static_cast<const FUnaryExpr&>(Expr);
			return MakeUnaryExpr(Source.Op, Source.Operand ? CloneExpr(*Source.Operand) : FExprPtr());
		}
		case ENodeKind::BinaryExpr:
		{
			const FBinaryExpr& Source = static_cast<const FBinaryExpr&>(Expr);
			return MakeBinaryExpr(
				Source.Op,
				Source.Left ? CloneExpr(*Source.Left) : FExprPtr(),
				Source.Right ? CloneExpr(*Source.Right) : FExprPtr());
		}
		case ENodeKind::AssignExpr:
		{
			const FAssignExpr& Source = static_cast<const FAssignExpr&>(Expr);
			TUniquePtr<FAssignExpr> Copy = MakeUnique<FAssignExpr>();
			Copy->Op = Source.Op;
			Copy->Target = Source.Target ? CloneExpr(*Source.Target) : FExprPtr();
			Copy->Value = Source.Value ? CloneExpr(*Source.Value) : FExprPtr();
			return Copy;
		}
		case ENodeKind::ConditionalExpr:
		{
			const FConditionalExpr& Source = static_cast<const FConditionalExpr&>(Expr);
			return MakeConditionalExpr(
				Source.Condition ? CloneExpr(*Source.Condition) : FExprPtr(),
				Source.TrueValue ? CloneExpr(*Source.TrueValue) : FExprPtr(),
				Source.FalseValue ? CloneExpr(*Source.FalseValue) : FExprPtr());
		}
		case ENodeKind::CastExpr:
		{
			const FCastExpr& Source = static_cast<const FCastExpr&>(Expr);
			return MakeCastExpr(Source.Type, Source.Operand ? CloneExpr(*Source.Operand) : FExprPtr());
		}
		case ENodeKind::InitializerListExpr:
		{
			const FInitializerListExpr& Source = static_cast<const FInitializerListExpr&>(Expr);
			TUniquePtr<FInitializerListExpr> Copy = MakeUnique<FInitializerListExpr>();
			for (const FExprPtr& Element : Source.Elements)
			{
				Copy->Elements.Add(Element ? CloneExpr(*Element) : FExprPtr());
			}
			return Copy;
		}
		case ENodeKind::ParenExpr:
		{
			const FParenExpr& Source = static_cast<const FParenExpr&>(Expr);
			TUniquePtr<FParenExpr> Copy = MakeUnique<FParenExpr>();
			Copy->Inner = Source.Inner ? CloneExpr(*Source.Inner) : FExprPtr();
			return Copy;
		}
		default:
			break;
		}
		return FExprPtr();
	}

	int32 MeasureBinaryDepth(const FExpr& Expr)
	{
		switch (Expr.Kind)
		{
		case ENodeKind::BinaryExpr:
		{
			const FBinaryExpr& Binary = static_cast<const FBinaryExpr&>(Expr);
			const int32 Left = Binary.Left ? MeasureBinaryDepth(*Binary.Left) : 0;
			const int32 Right = Binary.Right ? MeasureBinaryDepth(*Binary.Right) : 0;
			return 1 + FMath::Max(Left, Right);
		}
		case ENodeKind::ConditionalExpr:
		{
			const FConditionalExpr& Conditional = static_cast<const FConditionalExpr&>(Expr);
			int32 Deepest = Conditional.Condition ? MeasureBinaryDepth(*Conditional.Condition) : 0;
			Deepest = FMath::Max(Deepest, Conditional.TrueValue ? MeasureBinaryDepth(*Conditional.TrueValue) : 0);
			Deepest = FMath::Max(Deepest, Conditional.FalseValue ? MeasureBinaryDepth(*Conditional.FalseValue) : 0);
			return 1 + Deepest;
		}
		case ENodeKind::UnaryExpr:
		{
			const FUnaryExpr& Unary = static_cast<const FUnaryExpr&>(Expr);
			return Unary.Operand ? MeasureBinaryDepth(*Unary.Operand) : 0;
		}
		case ENodeKind::CastExpr:
		{
			const FCastExpr& Cast = static_cast<const FCastExpr&>(Expr);
			return Cast.Operand ? MeasureBinaryDepth(*Cast.Operand) : 0;
		}
		case ENodeKind::ParenExpr:
		{
			const FParenExpr& Paren = static_cast<const FParenExpr&>(Expr);
			return Paren.Inner ? MeasureBinaryDepth(*Paren.Inner) : 0;
		}
		case ENodeKind::MemberExpr:
		{
			const FMemberExpr& Member = static_cast<const FMemberExpr&>(Expr);
			return Member.Object ? MeasureBinaryDepth(*Member.Object) : 0;
		}
		default:
			// A call's arguments sit between commas of their own; the length rule is what catches a long one.
			break;
		}
		return 0;
	}

	// ------------------------------------------------------------------------------------ types

	FTypeRef MakeTypeRef(const FString& Spelling)
	{
		FTypeRef Type;
		Type.Name = Spelling;
		Private::FLangParser::ClassifyTypeName(Spelling, Type);
		return Type;
	}

	FString SpellValueType(const FIRType& Type, const bool bKeepBool)
	{
		switch (Type.Kind)
		{
		case IR::EIRTypeKind::Void:
			return TEXT("void");
		case IR::EIRTypeKind::Material:
			return TEXT("material");
		case IR::EIRTypeKind::Substrate:
			return TEXT("Substrate");
		case IR::EIRTypeKind::SamplerState:
			return TEXT("SamplerState");
		case IR::EIRTypeKind::Texture:
			switch (Type.Texture)
			{
			case ETextureKind::TextureCube: return TEXT("TextureCube");
			case ETextureKind::Texture2DArray: return TEXT("Texture2DArray");
			case ETextureKind::Texture3D: return TEXT("Texture3D");
			case ETextureKind::VolumeTexture: return TEXT("VolumeTexture");
			default: return TEXT("Texture2D");
			}
		case IR::EIRTypeKind::Bool:
			if (bKeepBool)
			{
				const int32 Width = FMath::Clamp(Type.NumComponents(), 1, 4);
				return Width == 1 ? FString(TEXT("bool")) : FString::Printf(TEXT("bool%d"), Width);
			}
			break;
		default:
			break;
		}

		// Everything numeric is the float the graph carries; a width the graph has none for is written as a scalar
		// and the binder says what is wrong with it.
		const int32 Width = FMath::Clamp(Type.GraphComponentCount(), 1, 4);
		return Width == 1 ? FString(TEXT("float")) : FString::Printf(TEXT("float%d"), Width);
	}

	FTypeRef MakeValueTypeRef(const FIRType& Type, const bool bKeepBool)
	{
		return MakeTypeRef(SpellValueType(Type, bKeepBool));
	}

	FTypeRef MakeSignatureTypeRef(const FIRType& Type, const FString& TypeSpelling)
	{
		return TypeSpelling.IsEmpty() ? MakeValueTypeRef(Type, /* bKeepBool */ true) : MakeTypeRef(TypeSpelling);
	}

	// ------------------------------------------------------------------------------- doc blocks

	void AddDocDirective(FDocBlock& Doc, const TCHAR* Key, const FString& Value)
	{
		FDocDirective Directive;
		Directive.Key = Key;
		Directive.Value = Value.TrimStartAndEnd();
		Doc.Directives.Add(MoveTemp(Directive));
	}

	void AddDocDescription(FDocBlock& Doc, const FString& Description)
	{
		FString Text = Description.Replace(TEXT("\r\n"), TEXT("\n")).Replace(TEXT("\r"), TEXT("\n")).TrimStartAndEnd();
		if (Text.IsEmpty())
		{
			return;
		}

		// Free text is what `@desc` falls back to, line for line. An `@` anywhere would read back as a directive, so
		// such a description goes on one `@desc` line instead.
		if (!Text.Contains(TEXT("@")))
		{
			TArray<FString> Lines;
			Text.ParseIntoArray(Lines, TEXT("\n"), /* bCullEmpty */ false);
			for (const FString& Line : Lines)
			{
				// Its end only: a line of a list is indented, and a doc block keeps that.
				Doc.FreeText.Add(Line.TrimEnd());
			}
			return;
		}

		Text.ReplaceInline(TEXT("\n"), TEXT(" "));
		AddDocDirective(Doc, Directive::Desc, Text);
	}

	// ------------------------------------------------------------------------------- statements

	FStmtPtr MakeVarDeclStmt(const FTypeRef& Type, const FString& Name, FExprPtr Initializer)
	{
		TUniquePtr<FVarDeclStmt> Statement = MakeUnique<FVarDeclStmt>();
		Statement->Type = Type;

		FDeclarator Declarator;
		Declarator.Name = Name;
		Declarator.Initializer = MoveTemp(Initializer);
		Statement->Declarators.Add(MoveTemp(Declarator));
		return Statement;
	}

	FStmtPtr MakeExprStmt(FExprPtr Expression)
	{
		TUniquePtr<FExprStmt> Statement = MakeUnique<FExprStmt>();
		Statement->Expression = MoveTemp(Expression);
		return Statement;
	}

	FStmtPtr MakeAssignStmt(FExprPtr Target, FExprPtr Value)
	{
		return MakeExprStmt(MakeAssignExpr(MoveTemp(Target), MoveTemp(Value)));
	}

	FStmtPtr MakeReturnStmt(FExprPtr Value)
	{
		TUniquePtr<FReturnStmt> Statement = MakeUnique<FReturnStmt>();
		Statement->Value = MoveTemp(Value);
		return Statement;
	}

	FStmtPtr MakeRegionStmt(const bool bBegin, const FString& Title)
	{
		TUniquePtr<FPragmaStmt> Statement = MakeUnique<FPragmaStmt>();
		Statement->PragmaKind = bBegin ? EPragmaKind::Region : EPragmaKind::EndRegion;
		Statement->Text = bBegin ? Title : FString();
		return Statement;
	}
}
