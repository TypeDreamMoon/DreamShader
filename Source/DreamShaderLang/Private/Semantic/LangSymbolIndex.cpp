// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The symbol index a language service reads (plan §13.4): what this file declares, where every
// name is mentioned, which headers it pulled in, and the parameter schema.
//
// It works on a bound module that has errors, deliberately. A file that does not compile is
// exactly the file whose author most needs go-to-definition, so nothing here asks Succeeded() --
// it reads whatever the declare pass managed to record and walks the AST for the rest.
//
// The JSON is written by hand. `DreamShaderLang` depends on Core alone (CONTRACT §0.9) and `Json`
// is a separate module, so FJsonObject and TJsonWriter are not reachable from here; the escaper
// below is the whole of what this file needs from one.

#include "LangBinderInternal.h"

#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"

namespace UE::DreamShader::Lang::Private
{
	void AppendJsonString(FString& Out, const FString& Value)
	{
		Out.AppendChar(TEXT('"'));
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Char = Value[Index];
			switch (Char)
			{
			case TEXT('"'):  Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\b'): Out += TEXT("\\b"); break;
			case TEXT('\f'): Out += TEXT("\\f"); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\r'): Out += TEXT("\\r"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default:
				if (Char < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Char));
				}
				else
				{
					Out.AppendChar(Char);
				}
				break;
			}
		}
		Out.AppendChar(TEXT('"'));
	}

	namespace
	{
		// ----------------------------------------------------------------------------- references

		struct FSymbolReference
		{
			FString File;
			FLangSpan Span;
		};

		struct FSymbolReferenceGroup
		{
			FString Name;
			TArray<FSymbolReference> Spans;
		};

		void AddReference(TArray<FSymbolReferenceGroup>& Groups, const FString& Name, const FString& File, const FLangSpan& Span)
		{
			if (Name.IsEmpty())
			{
				return;
			}
			for (FSymbolReferenceGroup& Group : Groups)
			{
				if (Group.Name.Equals(Name, ESearchCase::CaseSensitive))
				{
					Group.Spans.Add({ File, Span });
					return;
				}
			}
			FSymbolReferenceGroup Group;
			Group.Name = Name;
			Group.Spans.Add({ File, Span });
			Groups.Add(MoveTemp(Group));
		}

		void CollectTypeReference(const FTypeRef& Type, const FString& File, TArray<FSymbolReferenceGroup>& Groups)
		{
			// Only a Named type is a reference to something this file declares; `float3` names nothing.
			if (Type.Category == ETypeCategory::Named)
			{
				AddReference(Groups, Type.Name, File, Type.Span);
			}
		}

		void CollectExprReferences(const FExpr* Expr, const FString& File, TArray<FSymbolReferenceGroup>& Groups);

		void CollectArgumentReferences(const TArray<FArgument>& Arguments, const FString& File, TArray<FSymbolReferenceGroup>& Groups)
		{
			for (const FArgument& Argument : Arguments)
			{
				if (!Argument.Name.IsEmpty())
				{
					// A named argument mentions a pin, a property or a parameter by name; the editor
					// wants to find those too.
					AddReference(Groups, Argument.Name, File, Argument.NameSpan);
				}
				CollectExprReferences(Argument.Value.Get(), File, Groups);
			}
		}

		void CollectExprReferences(const FExpr* Expr, const FString& File, TArray<FSymbolReferenceGroup>& Groups)
		{
			if (!Expr)
			{
				return;
			}

			switch (Expr->Kind)
			{
			case ENodeKind::IdentifierExpr:
				AddReference(Groups, static_cast<const FIdentifierExpr*>(Expr)->Name, File, Expr->Span);
				break;

			case ENodeKind::TypeExpr:
				CollectTypeReference(static_cast<const FTypeExpr*>(Expr)->Type, File, Groups);
				break;

			case ENodeKind::MemberExpr:
			{
				const FMemberExpr& Member = *static_cast<const FMemberExpr*>(Expr);
				CollectExprReferences(Member.Object.Get(), File, Groups);
				AddReference(Groups, Member.Member, File, Member.MemberSpan);
				break;
			}

			case ENodeKind::IndexExpr:
			{
				const FIndexExpr& IndexExpr = *static_cast<const FIndexExpr*>(Expr);
				CollectExprReferences(IndexExpr.Object.Get(), File, Groups);
				CollectExprReferences(IndexExpr.Index.Get(), File, Groups);
				break;
			}

			case ENodeKind::CallExpr:
			{
				const FCallExpr& Call = *static_cast<const FCallExpr*>(Expr);
				CollectExprReferences(Call.Callee.Get(), File, Groups);
				CollectArgumentReferences(Call.Arguments, File, Groups);
				break;
			}

			case ENodeKind::UnaryExpr:
				CollectExprReferences(static_cast<const FUnaryExpr*>(Expr)->Operand.Get(), File, Groups);
				break;

			case ENodeKind::BinaryExpr:
			{
				const FBinaryExpr& Binary = *static_cast<const FBinaryExpr*>(Expr);
				CollectExprReferences(Binary.Left.Get(), File, Groups);
				CollectExprReferences(Binary.Right.Get(), File, Groups);
				break;
			}

			case ENodeKind::AssignExpr:
			{
				const FAssignExpr& Assign = *static_cast<const FAssignExpr*>(Expr);
				CollectExprReferences(Assign.Target.Get(), File, Groups);
				CollectExprReferences(Assign.Value.Get(), File, Groups);
				break;
			}

			case ENodeKind::ConditionalExpr:
			{
				const FConditionalExpr& Conditional = *static_cast<const FConditionalExpr*>(Expr);
				CollectExprReferences(Conditional.Condition.Get(), File, Groups);
				CollectExprReferences(Conditional.TrueValue.Get(), File, Groups);
				CollectExprReferences(Conditional.FalseValue.Get(), File, Groups);
				break;
			}

			case ENodeKind::CastExpr:
			{
				const FCastExpr& Cast = *static_cast<const FCastExpr*>(Expr);
				CollectTypeReference(Cast.Type, File, Groups);
				CollectExprReferences(Cast.Operand.Get(), File, Groups);
				break;
			}

			case ENodeKind::InitializerListExpr:
				for (const FExprPtr& Element : static_cast<const FInitializerListExpr*>(Expr)->Elements)
				{
					CollectExprReferences(Element.Get(), File, Groups);
				}
				break;

			case ENodeKind::ParenExpr:
				CollectExprReferences(static_cast<const FParenExpr*>(Expr)->Inner.Get(), File, Groups);
				break;

			default:
				break;
			}
		}

		void CollectStmtReferences(const FStmt* Stmt, const FString& File, TArray<FSymbolReferenceGroup>& Groups)
		{
			if (!Stmt)
			{
				return;
			}

			switch (Stmt->Kind)
			{
			case ENodeKind::BlockStmt:
				for (const FStmtPtr& Child : static_cast<const FBlockStmt*>(Stmt)->Statements)
				{
					CollectStmtReferences(Child.Get(), File, Groups);
				}
				break;

			case ENodeKind::VarDeclStmt:
			{
				const FVarDeclStmt& Decl = *static_cast<const FVarDeclStmt*>(Stmt);
				CollectTypeReference(Decl.Type, File, Groups);
				for (const FDeclarator& Declarator : Decl.Declarators)
				{
					for (const FExprPtr& Dimension : Declarator.ArrayDimensions)
					{
						CollectExprReferences(Dimension.Get(), File, Groups);
					}
					CollectExprReferences(Declarator.Initializer.Get(), File, Groups);
				}
				break;
			}

			case ENodeKind::ExprStmt:
				CollectExprReferences(static_cast<const FExprStmt*>(Stmt)->Expression.Get(), File, Groups);
				break;

			case ENodeKind::IfStmt:
			{
				const FIfStmt& If = *static_cast<const FIfStmt*>(Stmt);
				CollectExprReferences(If.Condition.Get(), File, Groups);
				CollectStmtReferences(If.Then.Get(), File, Groups);
				CollectStmtReferences(If.Else.Get(), File, Groups);
				break;
			}

			case ENodeKind::ForStmt:
			{
				const FForStmt& For = *static_cast<const FForStmt*>(Stmt);
				CollectStmtReferences(For.Init.Get(), File, Groups);
				CollectExprReferences(For.Condition.Get(), File, Groups);
				CollectExprReferences(For.Step.Get(), File, Groups);
				CollectStmtReferences(For.Body.Get(), File, Groups);
				break;
			}

			case ENodeKind::WhileStmt:
			{
				const FWhileStmt& While = *static_cast<const FWhileStmt*>(Stmt);
				CollectExprReferences(While.Condition.Get(), File, Groups);
				CollectStmtReferences(While.Body.Get(), File, Groups);
				break;
			}

			case ENodeKind::DoWhileStmt:
			{
				const FDoWhileStmt& Do = *static_cast<const FDoWhileStmt*>(Stmt);
				CollectStmtReferences(Do.Body.Get(), File, Groups);
				CollectExprReferences(Do.Condition.Get(), File, Groups);
				break;
			}

			case ENodeKind::ReturnStmt:
				CollectExprReferences(static_cast<const FReturnStmt*>(Stmt)->Value.Get(), File, Groups);
				break;

			default:
				break;
			}
		}

		// ---------------------------------------------------------------------------- signatures

		FString DescribeStorage(EStorageClass Storage)
		{
			switch (Storage)
			{
			case EStorageClass::Uniform:     return TEXT("uniform");
			case EStorageClass::StaticConst: return TEXT("static const");
			case EStorageClass::Static:      return TEXT("static");
			case EStorageClass::Const:       return TEXT("const");
			default:                         return FString();
			}
		}

		FString BuildVariableSignature(const FVariableDecl& Decl)
		{
			FString Signature = DescribeStorage(Decl.Storage);
			if (!Signature.IsEmpty())
			{
				Signature += TEXT(" ");
			}
			Signature += PrintDreamShaderLangType(Decl.Type);
			Signature += TEXT(" ");
			Signature += Decl.Declarator.Name;
			if (Decl.Declarator.ArrayDimensions.Num() > 0)
			{
				Signature += TEXT("[]");
			}
			if (Decl.Declarator.Initializer)
			{
				Signature += TEXT(" = ");
				Signature += PrintDreamShaderLangExpr(*Decl.Declarator.Initializer);
			}
			return Signature;
		}

		FString BuildFunctionSignature(const FFunctionDecl& Decl)
		{
			FString Signature;
			switch (Decl.Linkage)
			{
			case EFunctionLinkage::Export: Signature += TEXT("export "); break;
			case EFunctionLinkage::Extern: Signature += TEXT("extern "); break;
			default: break;
			}

			Signature += PrintDreamShaderLangType(Decl.ReturnType);
			Signature += TEXT(" ");
			Signature += Decl.Name;
			Signature += TEXT("(");

			for (int32 Index = 0; Index < Decl.Params.Num(); ++Index)
			{
				const FParam& Param = Decl.Params[Index];
				if (Index > 0)
				{
					Signature += TEXT(", ");
				}
				switch (Param.Direction)
				{
				case EParamDirection::Out:   Signature += TEXT("out "); break;
				case EParamDirection::InOut: Signature += TEXT("inout "); break;
				default: break;
				}
				Signature += PrintDreamShaderLangType(Param.Type);
				Signature += TEXT(" ");
				Signature += Param.Name;
				if (Param.ArrayDimensions.Num() > 0)
				{
					Signature += TEXT("[]");
				}
				if (Param.Default)
				{
					Signature += TEXT(" = ");
					Signature += PrintDreamShaderLangExpr(*Param.Default);
				}
			}

			Signature += TEXT(")");
			return Signature;
		}

		// --------------------------------------------------------------------------- json pieces

		void AppendSpan(FString& Out, const FString& Indent, const FString& File, const FLangSpan& Span)
		{
			Out += Indent;
			Out += TEXT("\"file\": ");
			AppendJsonString(Out, File);
			Out += TEXT(",\n");
			Out += Indent + FString::Printf(TEXT("\"line\": %d,\n"), Span.Line);
			Out += Indent + FString::Printf(TEXT("\"column\": %d,\n"), Span.Column);
			Out += Indent + FString::Printf(TEXT("\"offset\": %d,\n"), Span.Offset);
			Out += Indent + FString::Printf(TEXT("\"length\": %d"), Span.Length);
		}

		void AppendField(FString& Out, const FString& Indent, const TCHAR* Key, const FString& Value, bool bTrailingComma)
		{
			Out += Indent;
			Out += TEXT("\"");
			Out += Key;
			Out += TEXT("\": ");
			AppendJsonString(Out, Value);
			Out += bTrailingComma ? TEXT(",\n") : TEXT("\n");
		}

	}
}

namespace UE::DreamShader::Lang
{
	FString BuildDreamShaderSymbolIndexJson(const FBoundModule& Bound)
	{
		using namespace UE::DreamShader::Lang::Private;

		const FString RootFile = Bound.Module ? Bound.Module->FilePath : FString();

		// Every module the binder saw, root first, then the headers in first-seen order.
		TArray<const FModule*> Modules;
		if (Bound.Module)
		{
			Modules.Add(Bound.Module);
		}
		for (const FModule* Included : Bound.Included)
		{
			if (Included)
			{
				Modules.Add(Included);
			}
		}

		TArray<FSymbolReferenceGroup> References;
		for (const FModule* Module : Modules)
		{
			const FString& File = Module->FilePath;
			for (const FDeclPtr& DeclPtr : Module->Declarations)
			{
				const FDecl* Decl = DeclPtr.Get();
				if (!Decl)
				{
					continue;
				}

				switch (Decl->Kind)
				{
				case ENodeKind::VariableDecl:
				{
					const FVariableDecl& Variable = *static_cast<const FVariableDecl*>(Decl);
					CollectTypeReference(Variable.Type, File, References);
					for (const FExprPtr& Dimension : Variable.Declarator.ArrayDimensions)
					{
						CollectExprReferences(Dimension.Get(), File, References);
					}
					CollectExprReferences(Variable.Declarator.Initializer.Get(), File, References);
					break;
				}

				case ENodeKind::FunctionDecl:
				{
					const FFunctionDecl& Function = *static_cast<const FFunctionDecl*>(Decl);
					CollectTypeReference(Function.ReturnType, File, References);
					for (const FParam& Param : Function.Params)
					{
						CollectTypeReference(Param.Type, File, References);
						CollectExprReferences(Param.Default.Get(), File, References);
					}
					if (Function.Body)
					{
						CollectStmtReferences(Function.Body.Get(), File, References);
					}
					break;
				}

				case ENodeKind::StructDecl:
				{
					const FStructDecl& Struct = *static_cast<const FStructDecl*>(Decl);
					for (const FStructField& Field : Struct.Fields)
					{
						CollectTypeReference(Field.Type, File, References);
						for (const FExprPtr& Dimension : Field.ArrayDimensions)
						{
							CollectExprReferences(Dimension.Get(), File, References);
						}
					}
					break;
				}

				default:
					break;
				}
			}
		}

		FString Out;
		Out.Reserve(8192);
		Out += TEXT("{\n");
		Out += TEXT("  \"schema\": \"dreamshader-symbol-index\",\n");
		Out += TEXT("  \"version\": 1,\n");
		Out += TEXT("  \"file\": ");
		AppendJsonString(Out, RootFile);
		Out += TEXT(",\n");

		// ------------------------------------------------------------------------- declarations

		Out += TEXT("  \"declarations\": [\n");
		bool bFirstDeclaration = true;

		auto BeginDeclaration = [&Out, &bFirstDeclaration]()
		{
			if (!bFirstDeclaration)
			{
				Out += TEXT(",\n");
			}
			bFirstDeclaration = false;
			Out += TEXT("    {\n");
		};

		for (int32 Index = 0; Index < Bound.Structs.Num(); ++Index)
		{
			const FBoundStruct& Struct = Bound.Structs[Index];
			if (!Struct.Decl)
			{
				continue;
			}
			const FString& File = Struct.File;

			BeginDeclaration();
			AppendField(Out, TEXT("      "), TEXT("kind"), TEXT("struct"), true);
			AppendField(Out, TEXT("      "), TEXT("name"), Struct.Name, true);
			AppendField(Out, TEXT("      "), TEXT("signature"), FString(TEXT("struct ")) + Struct.Name, true);
			AppendField(Out, TEXT("      "), TEXT("doc"), FString::Join(Struct.Decl->Doc.FreeText, TEXT("\n")), true);
			AppendSpan(Out, TEXT("      "), File, Struct.Decl->NameSpan);
			Out += TEXT("\n    }");

			for (int32 FieldIndex = 0; FieldIndex < Struct.Fields.Num(); ++FieldIndex)
			{
				if (!Struct.Decl->Fields.IsValidIndex(FieldIndex))
				{
					continue;
				}
				const FStructField& Field = Struct.Decl->Fields[FieldIndex];

				BeginDeclaration();
				AppendField(Out, TEXT("      "), TEXT("kind"), TEXT("field"), true);
				AppendField(Out, TEXT("      "), TEXT("name"), Field.Name, true);
				AppendField(Out, TEXT("      "), TEXT("container"), Struct.Name, true);
				AppendField(Out, TEXT("      "), TEXT("signature"), PrintDreamShaderLangType(Field.Type) + TEXT(" ") + Field.Name, true);
				AppendField(Out, TEXT("      "), TEXT("type"), Struct.Fields[FieldIndex].Type.ToString(), true);
				AppendField(Out, TEXT("      "), TEXT("doc"), FString::Join(Field.Doc.FreeText, TEXT("\n")), true);
				AppendSpan(Out, TEXT("      "), File, Field.NameSpan);
				Out += TEXT("\n    }");
			}
		}

		for (const FBoundGlobal& Global : Bound.Globals)
		{
			if (!Global.Decl)
			{
				continue;
			}

			BeginDeclaration();
			AppendField(Out, TEXT("      "), TEXT("kind"), Global.bIsParameter ? TEXT("uniform") : TEXT("constant"), true);
			AppendField(Out, TEXT("      "), TEXT("name"), Global.Name, true);
			AppendField(Out, TEXT("      "), TEXT("signature"), BuildVariableSignature(*Global.Decl), true);
			AppendField(Out, TEXT("      "), TEXT("type"), Global.Type.ToString(), true);
			AppendField(Out, TEXT("      "), TEXT("doc"), Global.Directives.Desc, true);
			AppendSpan(Out, TEXT("      "), Global.File, Global.Decl->Declarator.NameSpan);
			Out += TEXT("\n    }");
		}

		for (const FBoundFunction& Function : Bound.Functions)
		{
			if (!Function.Decl)
			{
				continue;
			}

			BeginDeclaration();
			AppendField(Out, TEXT("      "), TEXT("kind"), TEXT("function"), true);
			AppendField(Out, TEXT("      "), TEXT("name"), Function.Name, true);
			AppendField(Out, TEXT("      "), TEXT("role"), LexToString(Function.Kind), true);
			AppendField(Out, TEXT("      "), TEXT("signature"), BuildFunctionSignature(*Function.Decl), true);
			AppendField(Out, TEXT("      "), TEXT("type"), Function.ReturnType.ToString(), true);
			AppendField(Out, TEXT("      "), TEXT("doc"), Function.Directives.Desc, true);
			if (!Function.Directives.Asset.IsEmpty())
			{
				AppendField(Out, TEXT("      "), TEXT("asset"), Function.Directives.Asset, true);
			}
			if (!Function.Directives.Library.IsEmpty())
			{
				AppendField(Out, TEXT("      "), TEXT("library"), Function.Directives.Library, true);
			}
			AppendSpan(Out, TEXT("      "), Function.File, Function.Decl->NameSpan);
			Out += TEXT("\n    }");

			for (int32 Index = 0; Index < Function.Params.Num(); ++Index)
			{
				if (!Function.Decl->Params.IsValidIndex(Index))
				{
					continue;
				}
				const FParam& Param = Function.Decl->Params[Index];
				const FBoundParam& BoundParam = Function.Params[Index];

				BeginDeclaration();
				AppendField(Out, TEXT("      "), TEXT("kind"), TEXT("parameter"), true);
				AppendField(Out, TEXT("      "), TEXT("name"), Param.Name, true);
				AppendField(Out, TEXT("      "), TEXT("container"), Function.Name, true);
				AppendField(Out, TEXT("      "), TEXT("type"), BoundParam.Type.ToString(), true);
				AppendField(Out, TEXT("      "), TEXT("doc"), BoundParam.Doc, true);
				AppendSpan(Out, TEXT("      "), Function.File, Param.NameSpan);
				Out += TEXT("\n    }");
			}
		}

		Out += bFirstDeclaration ? TEXT("") : TEXT("\n");
		Out += TEXT("  ],\n");

		// ---------------------------------------------------------------------------- references

		Out += TEXT("  \"references\": [\n");
		for (int32 Index = 0; Index < References.Num(); ++Index)
		{
			const FSymbolReferenceGroup& Group = References[Index];
			Out += TEXT("    {\n");
			AppendField(Out, TEXT("      "), TEXT("name"), Group.Name, true);
			Out += TEXT("      \"spans\": [\n");
			for (int32 SpanIndex = 0; SpanIndex < Group.Spans.Num(); ++SpanIndex)
			{
				Out += TEXT("        {\n");
				AppendSpan(Out, TEXT("          "), Group.Spans[SpanIndex].File, Group.Spans[SpanIndex].Span);
				Out += TEXT("\n        }");
				Out += (SpanIndex + 1 < Group.Spans.Num()) ? TEXT(",\n") : TEXT("\n");
			}
			Out += TEXT("      ]\n");
			Out += TEXT("    }");
			Out += (Index + 1 < References.Num()) ? TEXT(",\n") : TEXT("\n");
		}
		Out += TEXT("  ],\n");

		// ------------------------------------------------------------------------------ includes

		Out += TEXT("  \"includes\": [\n");
		for (int32 Index = 0; Index < Bound.IncludePaths.Num(); ++Index)
		{
			Out += TEXT("    ");
			AppendJsonString(Out, Bound.IncludePaths[Index]);
			Out += (Index + 1 < Bound.IncludePaths.Num()) ? TEXT(",\n") : TEXT("\n");
		}
		Out += TEXT("  ],\n");

		// ---------------------------------------------------------------------------- parameters

		Out += TEXT("  \"parameters\": [\n");
		bool bFirstParameter = true;
		for (const FBoundGlobal& Global : Bound.Globals)
		{
			if (!Global.bIsParameter || !Global.Decl)
			{
				continue;
			}

			if (!bFirstParameter)
			{
				Out += TEXT(",\n");
			}
			bFirstParameter = false;

			const FBoundDirectives& Directives = Global.Directives;

			Out += TEXT("    {\n");
			AppendField(Out, TEXT("      "), TEXT("name"), Directives.Name.IsEmpty() ? Global.Name : Directives.Name, true);
			AppendField(Out, TEXT("      "), TEXT("variable"), Global.Name, true);
			AppendField(Out, TEXT("      "), TEXT("type"), Global.Type.ToString(), true);
			AppendField(Out, TEXT("      "), TEXT("group"), Directives.Group, true);
			AppendField(Out, TEXT("      "), TEXT("desc"), Directives.Desc, true);
			AppendField(Out, TEXT("      "), TEXT("sampler"), Directives.Sampler, true);
			AppendField(Out, TEXT("      "), TEXT("defaultAsset"), Directives.DefaultAsset, true);
			Out += FString::Printf(TEXT("      \"static\": %s,\n"), Directives.bStatic ? TEXT("true") : TEXT("false"));

			if (Directives.bHasSort)
			{
				Out += FString::Printf(TEXT("      \"sort\": %d,\n"), Directives.Sort);
			}
			if (Directives.bHasSlider)
			{
				Out += TEXT("      \"slider\": { \"min\": ");
				Out += FString::SanitizeFloat(Directives.SliderMin);
				Out += TEXT(", \"max\": ");
				Out += FString::SanitizeFloat(Directives.SliderMax);
				Out += TEXT(" },\n");
			}

			// The folded default, when the binder could evaluate the initializer.
			if (Global.Decl->Declarator.Initializer)
			{
				if (const FBoundExpr* Init = Bound.Find(*Global.Decl->Declarator.Initializer))
				{
					if (Init->bIsConstant)
					{
						const int32 Components = FMath::Clamp(Global.Type.NumComponents(), 1, 4);
						Out += TEXT("      \"default\": [");
						for (int32 Component = 0; Component < Components; ++Component)
						{
							if (Component > 0)
							{
								Out += TEXT(", ");
							}
							Out += FString::SanitizeFloat(Init->ConstantValue[Component]);
						}
						Out += TEXT("],\n");
					}
				}
			}

			Out += TEXT("      \"directives\": {");
			bool bFirstDirective = true;
			for (const TPair<FString, FString>& Passthrough : Directives.Passthrough)
			{
				if (!bFirstDirective)
				{
					Out += TEXT(",");
				}
				bFirstDirective = false;
				Out += TEXT(" ");
				AppendJsonString(Out, Passthrough.Key);
				Out += TEXT(": ");
				AppendJsonString(Out, Passthrough.Value);
			}
			Out += bFirstDirective ? TEXT("},\n") : TEXT(" },\n");

			AppendSpan(Out, TEXT("      "), Global.File, Global.Decl->Declarator.NameSpan);
			Out += TEXT("\n    }");
		}
		Out += bFirstParameter ? TEXT("") : TEXT("\n");
		Out += TEXT("  ]\n");

		Out += TEXT("}\n");
		return Out;
	}
}
