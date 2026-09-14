// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader.Lang2.Binder.* -- the 2.0 semantic binder (unit S), through its public entry points
// BindDreamShaderLang and BuildDreamShaderSymbolIndexJson.
//
// The catalog is the hand-built MakeDreamShaderTestBuiltinCatalog() from DreamShaderTestCommon.h,
// never the engine's reflection: these tests are about the BINDER, they have to run without an
// editor, and a catalog that moved when the engine gained a pin would make them say something
// about Unreal rather than about DreamShader.
//
// Shape is asserted by dumping the bound side tables into a compact S-expression and comparing the
// whole string -- one line per case, and a failure prints the tree that was actually bound instead
// of "expected 3, got 4". The op spellings and conversion names the dumper uses are written out
// here rather than taken from the IR headers wherever a rename would otherwise silently rewrite
// every expected string.
//
// Diagnostics are asserted by CODE only (DSHnnnn). The messages are FText and the editor is
// localised, so their English is not a test fixture. String comparisons use TestEqualSensitive /
// Equals(..., ESearchCase::CaseSensitive): FString::operator== is case-insensitive, and a swizzle
// that came back `XY` instead of `xy` would otherwise pass.

#include "DreamShaderTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"

// A namespace of this file's own: the module builds as a unity blob, so a helper defined here under
// the shared ...::Tests namespace would be a redefinition of the identically named one in
// DreamShaderAutomationTests.cpp rather than a local convenience.
namespace UE::DreamShader::Editor::Private::Lang2BinderTests
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;

	// =============================================================================================
	// Spellings -- an independent table, so a renamed enumerator shows up as a failure here rather
	// than as a silently rewritten expectation.
	// =============================================================================================

	inline FString OpName(EIROp Op)
	{
		switch (Op)
		{
		case EIROp::Constant:              return TEXT("Constant");
		case EIROp::Parameter:             return TEXT("Parameter");
		case EIROp::TextureParameter:      return TEXT("TextureParameter");
		case EIROp::FunctionInput:         return TEXT("FunctionInput");
		case EIROp::Reflected:             return TEXT("Reflected");
		case EIROp::FunctionCall:          return TEXT("FunctionCall");
		case EIROp::Custom:                return TEXT("Custom");
		case EIROp::TextureSample:         return TEXT("TextureSample");
		case EIROp::Swizzle:               return TEXT("Swizzle");
		case EIROp::Append:                return TEXT("Append");
		case EIROp::Select:                return TEXT("Select");
		case EIROp::Compare:               return TEXT("Compare");
		case EIROp::StaticSwitch:          return TEXT("StaticSwitch");
		case EIROp::MakeMaterialAttributes: return TEXT("MakeMaterialAttributes");
		case EIROp::SetMaterialAttributes: return TEXT("SetMaterialAttributes");
		case EIROp::GetMaterialAttributes: return TEXT("GetMaterialAttributes");
		case EIROp::MaterialSink:          return TEXT("MaterialSink");
		case EIROp::FunctionOutput:        return TEXT("FunctionOutput");
		case EIROp::Add:                   return TEXT("Add");
		case EIROp::Subtract:              return TEXT("Subtract");
		case EIROp::Multiply:              return TEXT("Multiply");
		case EIROp::Divide:                return TEXT("Divide");
		case EIROp::Fmod:                  return TEXT("Fmod");
		case EIROp::Negate:                return TEXT("Negate");
		case EIROp::Abs:                   return TEXT("Abs");
		case EIROp::Floor:                 return TEXT("Floor");
		case EIROp::Frac:                  return TEXT("Frac");
		case EIROp::Saturate:              return TEXT("Saturate");
		case EIROp::Clamp:                 return TEXT("Clamp");
		case EIROp::Min:                   return TEXT("Min");
		case EIROp::Max:                   return TEXT("Max");
		case EIROp::Lerp:                  return TEXT("Lerp");
		case EIROp::Step:                  return TEXT("Step");
		case EIROp::Sqrt:                  return TEXT("Sqrt");
		case EIROp::Pow:                   return TEXT("Pow");
		case EIROp::Sin:                   return TEXT("Sin");
		case EIROp::Cos:                   return TEXT("Cos");
		case EIROp::Dot:                   return TEXT("Dot");
		case EIROp::Cross:                 return TEXT("Cross");
		case EIROp::Normalize:             return TEXT("Normalize");
		case EIROp::Length:                return TEXT("Length");
		case EIROp::Less:                  return TEXT("Less");
		case EIROp::LessEqual:             return TEXT("LessEqual");
		case EIROp::Greater:               return TEXT("Greater");
		case EIROp::GreaterEqual:          return TEXT("GreaterEqual");
		case EIROp::Equal:                 return TEXT("Equal");
		case EIROp::NotEqual:              return TEXT("NotEqual");
		case EIROp::LogicalAnd:            return TEXT("LogicalAnd");
		case EIROp::LogicalOr:             return TEXT("LogicalOr");
		case EIROp::LogicalNot:            return TEXT("LogicalNot");
		case EIROp::Convert:               return TEXT("Convert");
		case EIROp::Broadcast:             return TEXT("Broadcast");
		default:                           return FString(LexToString(Op));
		}
	}

	inline const TCHAR* ConversionName(EIRConversion Conversion)
	{
		switch (Conversion)
		{
		case EIRConversion::Identity:      return TEXT("id");
		case EIRConversion::Broadcast:     return TEXT("bcast");
		case EIRConversion::Numeric:       return TEXT("num");
		case EIRConversion::DefaultOutput: return TEXT("defout");
		case EIRConversion::None:          return TEXT("none");
		default:                           return TEXT("<conv?>");
		}
	}

	inline const TCHAR* FunctionKindName(EBoundFunctionKind Kind)
	{
		switch (Kind)
		{
		case EBoundFunctionKind::Helper:         return TEXT("Helper");
		case EBoundFunctionKind::Custom:         return TEXT("Custom");
		case EBoundFunctionKind::Entry:          return TEXT("Entry");
		case EBoundFunctionKind::Layer:          return TEXT("Layer");
		case EBoundFunctionKind::LayerBlend:     return TEXT("LayerBlend");
		case EBoundFunctionKind::ExportFunction: return TEXT("ExportFunction");
		case EBoundFunctionKind::Extern:         return TEXT("Extern");
		default:                                 return TEXT("<kind?>");
		}
	}

	inline const TCHAR* ProductKindName(EIRProductKind Kind)
	{
		switch (Kind)
		{
		case EIRProductKind::Material:           return TEXT("Material");
		case EIRProductKind::MaterialFunction:   return TEXT("MaterialFunction");
		case EIRProductKind::MaterialLayer:      return TEXT("MaterialLayer");
		case EIRProductKind::MaterialLayerBlend: return TEXT("MaterialLayerBlend");
		default:                                 return TEXT("<product?>");
		}
	}

	// =============================================================================================
	// Driving the binder
	// =============================================================================================

	/** One parse + bind, with the ownership order the bound module needs (see the member comments). */
	struct FBindCase
	{
		FLangParseResult Parse;
		TArray<TUniquePtr<FLangParseResult>> Included;
		FLangBindResult Bind;

		bool Parsed() const { return Parse.Succeeded() && Parse.Module.IsValid(); }
		bool Bound() const { return Bind.Succeeded() && Bind.Bound.IsValid(); }
		const FBoundModule& Module() const { return *Bind.Bound; }

		/** Every error's `DSHnnnn: message` wire string, parse and bind together. */
		TArray<FString> Errors() const
		{
			TArray<FString> Lines;
			for (const FLangDiagnostic& Diagnostic : Parse.Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Severity == ELangSeverity::Error) { Lines.Add(FLangDiagnosticSink::ToWireString(Diagnostic)); }
			}
			for (const FLangDiagnostic& Diagnostic : Bind.Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Severity == ELangSeverity::Error) { Lines.Add(FLangDiagnosticSink::ToWireString(Diagnostic)); }
			}
			return Lines;
		}

		TArray<FString> Warnings() const
		{
			TArray<FString> Lines;
			for (const FLangDiagnostic& Diagnostic : Bind.Diagnostics.GetDiagnostics())
			{
				if (Diagnostic.Severity == ELangSeverity::Warning) { Lines.Add(FLangDiagnosticSink::ToWireString(Diagnostic)); }
			}
			return Lines;
		}

		FString ErrorText() const { return FString::Join(Errors(), TEXT(" | ")); }
	};

	/** Sources for `#include` resolution inside one test, by leaf name. */
	using FHeaderMap = TMap<FString, FString>;

	inline void BindSource(FBindCase& Case, const TCHAR* Text, const FHeaderMap* Headers = nullptr, const TCHAR* Path = TEXT("Test.dss"))
	{
		Case.Parse = ParseDreamShaderLang(FLangSourceText(Path, Text), FLangParseOptions());
		if (!Case.Parse.Module.IsValid())
		{
			return;
		}

		FBindOptions Options;
		Options.Catalog = &UE::DreamShader::Editor::Private::Tests::GetDreamShaderTestBuiltinCatalog();
		if (Headers)
		{
			// A resolver over an in-memory table: the corpus is where a real file tree belongs, and
			// a unit test that wrote files would be a unit test that needed cleaning up.
			const FHeaderMap* Table = Headers;
			FBindCase* Self = &Case;
			Options.IncludeResolver =
				[Table, Self](const FString& IncludePath, const FString& FromFile, FLangDiagnosticSink& Diagnostics) -> const FModule*
			{
				const FString Leaf = FPaths::GetCleanFilename(IncludePath);
				for (const TUniquePtr<FLangParseResult>& Existing : Self->Included)
				{
					if (Existing.IsValid() && Existing->Module.IsValid()
						&& Existing->Module->FilePath.Equals(Leaf, ESearchCase::CaseSensitive))
					{
						return Existing->Module.Get();
					}
				}

				const FString* Text = Table->Find(Leaf);
				if (!Text)
				{
					return nullptr;
				}

				TUniquePtr<FLangParseResult> Parsed = MakeUnique<FLangParseResult>(
					ParseDreamShaderLang(FLangSourceText(Leaf, *Text), FLangParseOptions()));
				if (!Parsed->Module.IsValid())
				{
					return nullptr;
				}
				Diagnostics.Append(MoveTemp(Parsed->Diagnostics));

				const FModule* Module = Parsed->Module.Get();
				Self->Included.Add(MoveTemp(Parsed));
				return Module;
			};
		}

		Case.Bind = BindDreamShaderLang(*Case.Parse.Module, Options);
	}

	/** True when some error of the run carries exactly this code. */
	inline bool HasCode(const FBindCase& Case, const TCHAR* Code)
	{
		const FString Needle = FString::Printf(TEXT("%s:"), Code);
		for (const FString& Line : Case.Errors())
		{
			if (Line.StartsWith(Needle, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** True when some error's code falls in [First, Last] -- for a code S has not published yet. */
	inline bool HasCodeInRange(const FBindCase& Case, int32 First, int32 Last)
	{
		for (const FString& Line : Case.Errors())
		{
			if (!Line.StartsWith(TEXT("DSH"), ESearchCase::CaseSensitive))
			{
				continue;
			}
			const FString Digits = Line.Mid(3, 4);
			if (Digits.Len() == 4 && FCString::IsNumeric(*Digits))
			{
				const int32 Value = FCString::Atoi(*Digits);
				if (Value >= First && Value <= Last)
				{
					return true;
				}
			}
		}
		return false;
	}

	inline bool HasWarningCode(const FBindCase& Case, const TCHAR* Code)
	{
		const FString Needle = FString::Printf(TEXT("%s:"), Code);
		for (const FString& Line : Case.Warnings())
		{
			if (Line.StartsWith(Needle, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** `Test.TestTrue` on "this source is refused, with this code". */
	inline bool ExpectCode(FAutomationTestBase& Test, const TCHAR* What, const TCHAR* Text, const TCHAR* Code)
	{
		FBindCase Case;
		BindSource(Case, Text);
		const bool bHas = HasCode(Case, Code);
		Test.TestTrue(
			FString::Printf(TEXT("%s: %s (actual: %s)"), What, Code, *Case.ErrorText()),
			bHas);
		return bHas;
	}

	// =============================================================================================
	// AST navigation
	// =============================================================================================

	inline const FFunctionDecl* FindFunctionDecl(const FModule& Module, const TCHAR* Name)
	{
		for (const FDeclPtr& Decl : Module.Declarations)
		{
			if (const FFunctionDecl* Function = Decl ? Decl->As<FFunctionDecl>() : nullptr)
			{
				if (Function->Name.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Function;
				}
			}
		}
		return nullptr;
	}

	inline const FVariableDecl* FindVariableDecl(const FModule& Module, const TCHAR* Name)
	{
		for (const FDeclPtr& Decl : Module.Declarations)
		{
			if (const FVariableDecl* Variable = Decl ? Decl->As<FVariableDecl>() : nullptr)
			{
				if (Variable->Declarator.Name.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Variable;
				}
			}
		}
		return nullptr;
	}

	inline const FStmt* BodyStatement(const FFunctionDecl& Function, int32 Index)
	{
		if (!Function.Body || !Function.Body->Statements.IsValidIndex(Index))
		{
			return nullptr;
		}
		return Function.Body->Statements[Index].Get();
	}

	// =============================================================================================
	// The dumper
	// =============================================================================================

	FString ExprShape(const FBoundModule& Bound, const FExpr* Expr);

	inline FString ArgsShape(const FBoundModule& Bound, const FBoundExpr& Info, const FCallExpr& Call)
	{
		FString Result;
		for (const FBoundArgument& Argument : Info.Args)
		{
			Result += FString::Printf(
				TEXT(" [%s%s#%d %s]"),
				*Argument.Target,
				Argument.bIsProperty ? TEXT("=prop") : TEXT("=pin"),
				Argument.TargetIndex,
				ConversionName(Argument.Conversion));
		}
		for (const FArgument& Argument : Call.Arguments)
		{
			Result += TEXT(" ") + ExprShape(Bound, Argument.Value.Get());
		}
		return Result;
	}

	/**
	 * `(<kind> <detail> : <type> <children...>)`.
	 *
	 * The children come from the AST, the facts from the side tables: that is the whole point of
	 * the bound module, and a dumper that walked anything else would not be testing it.
	 */
	inline FString ExprShape(const FBoundModule& Bound, const FExpr* Expr)
	{
		if (!Expr)
		{
			return TEXT("(null)");
		}

		const FBoundExpr* Info = Bound.Find(*Expr);
		if (!Info)
		{
			return FString::Printf(TEXT("(unbound %s)"), LexToString(Expr->Kind));
		}

		const FString Type = Info->Type.ToString();
		const FString Suffix = (Info->Conversion == EIRConversion::Identity)
			? FString()
			: FString::Printf(TEXT("~%s"), ConversionName(Info->Conversion));
		const FString Constant = Info->bIsConstant ? TEXT("!") : TEXT("");

		auto Head = [&Type, &Suffix, &Constant](const FString& Tag)
		{
			return FString::Printf(TEXT("(%s%s : %s%s"), *Tag, *Constant, *Type, *Suffix);
		};

		switch (Info->Kind)
		{
		case EBoundExprKind::Error:
			return Head(TEXT("err")) + TEXT(")");

		case EBoundExprKind::Literal:
		{
			const FLiteralExpr* Literal = Expr->As<FLiteralExpr>();
			return Head(FString::Printf(TEXT("lit %s"), Literal ? *Literal->Text : TEXT("?"))) + TEXT(")");
		}

		case EBoundExprKind::Local:
			return Head(FString::Printf(TEXT("local #%d"), Info->LocalSlot)) + TEXT(")");

		case EBoundExprKind::Global:
			return Head(FString::Printf(TEXT("global #%d"), Info->Index)) + TEXT(")");

		case EBoundExprKind::Param:
			return Head(FString::Printf(TEXT("param #%d"), Info->Index)) + TEXT(")");

		case EBoundExprKind::StructField:
		{
			const FMemberExpr* Member = Expr->As<FMemberExpr>();
			return Head(FString::Printf(TEXT("sfield #%d"), Info->FieldIndex))
				+ TEXT(" ") + ExprShape(Bound, Member ? Member->Object.Get() : nullptr) + TEXT(")");
		}

		case EBoundExprKind::MaterialField:
		{
			const FMemberExpr* Member = Expr->As<FMemberExpr>();
			return Head(FString::Printf(TEXT("mfield #%d"), Info->FieldIndex))
				+ TEXT(" ") + ExprShape(Bound, Member ? Member->Object.Get() : nullptr) + TEXT(")");
		}

		case EBoundExprKind::NodeOutput:
		{
			const FMemberExpr* Member = Expr->As<FMemberExpr>();
			return Head(FString::Printf(TEXT("out #%d"), Info->FieldIndex))
				+ TEXT(" ") + ExprShape(Bound, Member ? Member->Object.Get() : nullptr) + TEXT(")");
		}

		case EBoundExprKind::Swizzle:
		{
			const FMemberExpr* Member = Expr->As<FMemberExpr>();
			return Head(FString::Printf(TEXT("swz %s"), *Info->Swizzle))
				+ TEXT(" ") + ExprShape(Bound, Member ? Member->Object.Get() : nullptr) + TEXT(")");
		}

		case EBoundExprKind::IndexConst:
		{
			const FIndexExpr* Index = Expr->As<FIndexExpr>();
			return Head(FString::Printf(TEXT("idx %s"), *Info->Swizzle))
				+ TEXT(" ") + ExprShape(Bound, Index ? Index->Object.Get() : nullptr) + TEXT(")");
		}

		case EBoundExprKind::CoreOp:
		{
			FString Result = Head(FString::Printf(TEXT("op %s"), *OpName(Info->CoreOp)));
			if (const FBinaryExpr* Binary = Expr->As<FBinaryExpr>())
			{
				Result += TEXT(" ") + ExprShape(Bound, Binary->Left.Get());
				Result += TEXT(" ") + ExprShape(Bound, Binary->Right.Get());
			}
			else if (const FUnaryExpr* Unary = Expr->As<FUnaryExpr>())
			{
				Result += TEXT(" ") + ExprShape(Bound, Unary->Operand.Get());
			}
			else if (const FCallExpr* Call = Expr->As<FCallExpr>())
			{
				for (const FArgument& Argument : Call->Arguments)
				{
					Result += TEXT(" ") + ExprShape(Bound, Argument.Value.Get());
				}
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::Constructor:
		case EBoundExprKind::StructConstructor:
		{
			const TCHAR* Tag = (Info->Kind == EBoundExprKind::Constructor) ? TEXT("ctor") : TEXT("sctor");
			FString Result = Head(Tag);
			if (const FCallExpr* Call = Expr->As<FCallExpr>())
			{
				for (const FArgument& Argument : Call->Arguments)
				{
					Result += TEXT(" ") + ExprShape(Bound, Argument.Value.Get());
				}
			}
			else if (const FInitializerListExpr* Init = Expr->As<FInitializerListExpr>())
			{
				for (const FExprPtr& Element : Init->Elements)
				{
					Result += TEXT(" ") + ExprShape(Bound, Element.Get());
				}
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::Cast:
		{
			const FCastExpr* Cast = Expr->As<FCastExpr>();
			return Head(TEXT("cast")) + TEXT(" ") + ExprShape(Bound, Cast ? Cast->Operand.Get() : nullptr) + TEXT(")");
		}

		case EBoundExprKind::ReflectedCall:
		{
			const FCallExpr* Call = Expr->As<FCallExpr>();
			FString Result = Head(FString::Printf(TEXT("refl #%d"), Info->Index));
			if (Call)
			{
				Result += ArgsShape(Bound, *Info, *Call);
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::FunctionCall:
		{
			const FCallExpr* Call = Expr->As<FCallExpr>();
			FString Result = Head(FString::Printf(TEXT("call #%d"), Info->Index));
			if (Call)
			{
				Result += ArgsShape(Bound, *Info, *Call);
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::TextureSample:
		{
			const FCallExpr* Call = Expr->As<FCallExpr>();
			FString Result = Head(TEXT("tex"));
			if (Call)
			{
				Result += ArgsShape(Bound, *Info, *Call);
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::Conditional:
		{
			const FConditionalExpr* Conditional = Expr->As<FConditionalExpr>();
			FString Result = Head(TEXT("cond"));
			if (Conditional)
			{
				Result += TEXT(" ") + ExprShape(Bound, Conditional->Condition.Get());
				Result += TEXT(" ") + ExprShape(Bound, Conditional->TrueValue.Get());
				Result += TEXT(" ") + ExprShape(Bound, Conditional->FalseValue.Get());
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::Assign:
		{
			const FAssignExpr* Assign = Expr->As<FAssignExpr>();
			FString Result = Head(TEXT("assign"));
			if (Assign)
			{
				Result += TEXT(" ") + ExprShape(Bound, Assign->Target.Get());
				Result += TEXT(" ") + ExprShape(Bound, Assign->Value.Get());
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::InitializerList:
		{
			const FInitializerListExpr* Init = Expr->As<FInitializerListExpr>();
			FString Result = Head(TEXT("init"));
			if (Init)
			{
				for (const FExprPtr& Element : Init->Elements)
				{
					Result += TEXT(" ") + ExprShape(Bound, Element.Get());
				}
			}
			return Result + TEXT(")");
		}

		case EBoundExprKind::Paren:
		{
			const FParenExpr* Paren = Expr->As<FParenExpr>();
			return Head(TEXT("paren")) + TEXT(" ") + ExprShape(Bound, Paren ? Paren->Inner.Get() : nullptr) + TEXT(")");
		}

		default:
			return Head(TEXT("<bound?>")) + TEXT(")");
		}
	}

	/** The initializer of declarator `Which` of statement `Index` of a function body, bound-dumped. */
	inline FString InitShape(const FBindCase& Case, const TCHAR* FunctionName, int32 Index, int32 Which = 0)
	{
		if (!Case.Bound())
		{
			return FString::Printf(TEXT("<not bound: %s>"), *Case.ErrorText());
		}

		const FFunctionDecl* Function = FindFunctionDecl(*Case.Parse.Module, FunctionName);
		if (!Function)
		{
			return TEXT("<no function>");
		}
		const FStmt* Stmt = BodyStatement(*Function, Index);
		const FVarDeclStmt* VarDecl = Stmt ? Stmt->As<FVarDeclStmt>() : nullptr;
		if (!VarDecl || !VarDecl->Declarators.IsValidIndex(Which))
		{
			return TEXT("<no declarator>");
		}
		return ExprShape(Case.Module(), VarDecl->Declarators[Which].Initializer.Get());
	}

	/** The expression of statement `Index` of a function body, bound-dumped. */
	inline FString StmtExprShape(const FBindCase& Case, const TCHAR* FunctionName, int32 Index)
	{
		if (!Case.Bound())
		{
			return FString::Printf(TEXT("<not bound: %s>"), *Case.ErrorText());
		}

		const FFunctionDecl* Function = FindFunctionDecl(*Case.Parse.Module, FunctionName);
		if (!Function)
		{
			return TEXT("<no function>");
		}
		const FStmt* Stmt = BodyStatement(*Function, Index);
		if (const FExprStmt* ExprStmt = Stmt ? Stmt->As<FExprStmt>() : nullptr)
		{
			return ExprShape(Case.Module(), ExprStmt->Expression.Get());
		}
		if (const FReturnStmt* ReturnStmt = Stmt ? Stmt->As<FReturnStmt>() : nullptr)
		{
			return ExprShape(Case.Module(), ReturnStmt->Value.Get());
		}
		return TEXT("<no expression statement>");
	}

}

// =================================================================================================
// Uniforms and constants -- CONTRACT 6.1
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderUniformsTest,
	"DreamShader.Lang2.Binder.Uniforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderUniformsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"uniform float Scalar = 1;\n"
		"uniform float3 Vector = float3(1, 0, 0);\n"
		"uniform int Count = 2;\n"
		"uniform bool Toggle = true;\n"
		"/// @static\n"
		"uniform bool StaticToggle = false;\n"
		"/// @sampler Normal\n"
		"uniform Texture2D Tex;\n"
		"static const float K = 2 * 3;\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = float3(Scalar, Scalar, Scalar);\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();

	// The bound module carries the catalog it was bound against, so a later stage never has to be
	// handed it a second time -- and never has a chance to be handed a DIFFERENT one.
	TestNotNull(TEXT("the bound module exposes its catalog"), Bound.Catalog);
	if (Bound.Catalog)
	{
		TestTrue(
			TEXT("and it is the catalog that was passed in"),
			Bound.Catalog->FindMaterialAttribute(TEXT("BaseColor"))
				== UE::DreamShader::Editor::Private::Tests::GetDreamShaderTestBuiltinCatalog().FindMaterialAttribute(TEXT("BaseColor")));
	}

	TestEqual(TEXT("seven globals"), Bound.Globals.Num(), 7);

	// Storage decides the node kind; the type decides its width. Both are read straight back.
	struct FExpected { const TCHAR* Name; const TCHAR* Type; bool bParameter; bool bConstant; };
	const FExpected Expected[] =
	{
		{ TEXT("Scalar"),       TEXT("float"),     true,  false },
		{ TEXT("Vector"),       TEXT("float3"),    true,  false },
		{ TEXT("Count"),        TEXT("int"),       true,  false },
		{ TEXT("Toggle"),       TEXT("bool"),      true,  false },
		{ TEXT("StaticToggle"), TEXT("bool"),      true,  false },
		{ TEXT("Tex"),          TEXT("Texture2D"), true,  false },
		{ TEXT("K"),            TEXT("float"),     false, true  },
	};

	for (const FExpected& One : Expected)
	{
		const int32 Index = Bound.FindGlobal(One.Name);
		if (!TestTrue(FString::Printf(TEXT("global '%s' is bound"), One.Name), Index != INDEX_NONE))
		{
			continue;
		}

		const FBoundGlobal& Global = Bound.Globals[Index];
		TestEqualSensitive(
			*FString::Printf(TEXT("global '%s' type"), One.Name),
			Global.Type.ToString(),
			FString(One.Type));
		TestEqual(FString::Printf(TEXT("global '%s' is a parameter"), One.Name), Global.bIsParameter, One.bParameter);
		TestEqual(FString::Printf(TEXT("global '%s' is a constant"), One.Name), Global.bIsConstant, One.bConstant);
	}

	// `/// @static` on a uniform bool is what makes it a StaticBoolParameter downstream.
	const int32 StaticIndex = Bound.FindGlobal(TEXT("StaticToggle"));
	if (StaticIndex != INDEX_NONE)
	{
		TestTrue(TEXT("@static reaches the directives"), Bound.Globals[StaticIndex].Directives.bStatic);
	}
	const int32 ToggleIndex = Bound.FindGlobal(TEXT("Toggle"));
	if (ToggleIndex != INDEX_NONE)
	{
		TestFalse(TEXT("a plain uniform bool is not static"), Bound.Globals[ToggleIndex].Directives.bStatic);
	}

	// The sampler type of a texture parameter comes from `@sampler`, not from the pin.
	const int32 TexIndex = Bound.FindGlobal(TEXT("Tex"));
	if (TexIndex != INDEX_NONE)
	{
		TestEqualSensitive(TEXT("@sampler"), Bound.Globals[TexIndex].Directives.Sampler, FString(TEXT("Normal")));
	}

	// `static const` folds: the whole point of the kind is that the value is known here.
	const int32 ConstantIndex = Bound.FindGlobal(TEXT("K"));
	if (ConstantIndex != INDEX_NONE && Case.Parse.Module.IsValid())
	{
		const FVariableDecl* Decl = FindVariableDecl(*Case.Parse.Module, TEXT("K"));
		if (TestNotNull(TEXT("the constant's declarator is in the module"), Decl))
		{
			const FBoundExpr* Init = Decl->Declarator.Initializer
				? Bound.Find(*Decl->Declarator.Initializer)
				: nullptr;
			if (TestNotNull(TEXT("the constant's initializer is bound"), Init))
			{
				TestTrue(TEXT("the constant's initializer folded"), Init->bIsConstant);
				TestEqual(TEXT("2 * 3 folds to 6"), Init->ConstantValue[0], 6.0);
			}
		}
	}

	// The two refusals of the table: a global that is neither, and a `const` that is not constant.
	ExpectCode(*this, TEXT("a bare global is refused"),
		TEXT("float Loose = 1;\nexport void M_Case(inout material m) { m.Opacity = 1; }\n"),
		TEXT("DSH7211"));
	ExpectCode(*this, TEXT("a non-constant static const is refused"),
		TEXT("uniform float U = 1;\nstatic const float K = U;\nexport void M_Case(inout material m) { m.Opacity = K; }\n"),
		TEXT("DSH7210"));
	return true;
}

// =================================================================================================
// `///` directives -- CONTRACT 6.1, second half
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderDirectivesTest,
	"DreamShader.Lang2.Binder.Directives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderDirectivesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"/// The free text above a parameter.\n"
		"/// @group Look|Colour  @slider 0 4  @sort 20  @name TintColour  @somethingElse Yes\n"
		"uniform float4 Tint = float4(1, 1, 1, 1);\n"
		"\n"
		"/// @desc An explicit description wins over the free text.\n"
		"/// Some free text.\n"
		"uniform float Gain = 1;\n"
		"\n"
		"/// @library MoonToon|Shared\n"
		"/// @param UVChannel Which UV channel to read.\n"
		"export float2 MF_Case(float UVChannel = 0.0)\n"
		"{\n"
		"    return float2(UVChannel, UVChannel);\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();

	const int32 TintIndex = Bound.FindGlobal(TEXT("Tint"));
	if (TestTrue(TEXT("Tint is bound"), TintIndex != INDEX_NONE))
	{
		const FBoundDirectives& Directives = Bound.Globals[TintIndex].Directives;
		TestEqualSensitive(TEXT("@group"), Directives.Group, FString(TEXT("Look|Colour")));
		TestEqualSensitive(TEXT("@name"), Directives.Name, FString(TEXT("TintColour")));
		TestTrue(TEXT("@slider was seen"), Directives.bHasSlider);
		TestEqual(TEXT("@slider min"), Directives.SliderMin, 0.0);
		TestEqual(TEXT("@slider max"), Directives.SliderMax, 4.0);
		TestTrue(TEXT("@sort was seen"), Directives.bHasSort);
		TestEqual(TEXT("@sort"), Directives.Sort, 20);

		// An unknown key is not an error: it is a reflected property the emitter will try to apply.
		const FString* Passthrough = Directives.Passthrough.Find(TEXT("somethingelse"));
		if (TestNotNull(TEXT("an unknown directive becomes a passthrough"), Passthrough))
		{
			TestEqualSensitive(TEXT("the passthrough value"), *Passthrough, FString(TEXT("Yes")));
		}

		// The free text is what Desc falls back to -- here there was no @desc.
		TestTrue(TEXT("the free text reached the block"), !Directives.FreeText.IsEmpty());
	}

	const int32 GainIndex = Bound.FindGlobal(TEXT("Gain"));
	if (TestTrue(TEXT("Gain is bound"), GainIndex != INDEX_NONE))
	{
		TestEqualSensitive(
			TEXT("@desc wins over the free text"),
			Bound.Globals[GainIndex].Directives.Desc,
			FString(TEXT("An explicit description wins over the free text.")));
	}

	const int32 FunctionIndex = Bound.FindFunction(TEXT("MF_Case"));
	if (TestTrue(TEXT("MF_Case is bound"), FunctionIndex != INDEX_NONE))
	{
		const FBoundFunction& Function = Bound.Functions[FunctionIndex];
		TestEqualSensitive(TEXT("@library"), Function.Directives.Library, FString(TEXT("MoonToon|Shared")));

		const FString* Doc = Function.Directives.FindParamDoc(TEXT("UVChannel"));
		if (TestNotNull(TEXT("@param reaches FindParamDoc"), Doc))
		{
			TestEqualSensitive(TEXT("the @param text"), *Doc, FString(TEXT("Which UV channel to read.")));
		}

		if (TestEqual(TEXT("one parameter"), Function.Params.Num(), 1))
		{
			TestTrue(TEXT("a default makes the input optional"), Function.Params[0].bOptional);
			TestEqualSensitive(TEXT("the parameter doc"), Function.Params[0].Doc, FString(TEXT("Which UV channel to read.")));
		}
	}

	return true;
}

// =================================================================================================
// Function kinds, the entry rule and the products -- CONTRACT 6.9, decision 11 #15
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderFunctionKindsTest,
	"DreamShader.Lang2.Binder.FunctionKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderFunctionKindsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	// A function library: three exports and one internal helper, no entry.
	{
		FBindCase Case;
		BindSource(Case, TEXT(
			"/// @asset /MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch\n"
			"extern float2 MF_UVChannelSwitch(float UVChannelIndex);\n"
			"\n"
			"float Helper(float x) { return x * 2; }\n"
			"\n"
			"/// @custom\n"
			"float3 CustomOne(float3 c) { return c * 0.5; }\n"
			"\n"
			"export float2 MF_Exported(float UVChannel = 0.0)\n"
			"{\n"
			"    return MF_UVChannelSwitch(Helper(UVChannel));\n"
			"}\n"));

		if (TestTrue(FString::Printf(TEXT("the library binds: %s"), *Case.ErrorText()), Case.Bound()))
		{
			const FBoundModule& Bound = Case.Module();

			struct FExpected { const TCHAR* Name; const TCHAR* Kind; };
			const FExpected Expected[] =
			{
				{ TEXT("MF_UVChannelSwitch"), TEXT("Extern") },
				{ TEXT("Helper"),             TEXT("Helper") },
				{ TEXT("CustomOne"),          TEXT("Custom") },
				{ TEXT("MF_Exported"),        TEXT("ExportFunction") },
			};
			for (const FExpected& One : Expected)
			{
				const int32 Index = Bound.FindFunction(One.Name);
				if (TestTrue(FString::Printf(TEXT("'%s' is bound"), One.Name), Index != INDEX_NONE))
				{
					TestEqualSensitive(
						*FString::Printf(TEXT("'%s' kind"), One.Name),
						FString(FunctionKindName(Bound.Functions[Index].Kind)),
						FString(One.Kind));
				}
			}

			TestEqual(TEXT("a library has no entry"), Bound.FindEntryFunction(), INDEX_NONE);
			TestEqual(TEXT("one product"), Bound.Products.Num(), 1);
			if (Bound.Products.Num() == 1)
			{
				TestEqualSensitive(
					TEXT("the product kind"),
					FString(ProductKindName(Bound.Products[0].Kind)),
					FString(TEXT("MaterialFunction")));
				TestEqualSensitive(TEXT("the asset name"), Bound.Products[0].AssetName, FString(TEXT("MF_Exported")));
			}

			const int32 ExternIndex = Bound.FindFunction(TEXT("MF_UVChannelSwitch"));
			if (ExternIndex != INDEX_NONE)
			{
				TestEqualSensitive(
					TEXT("@asset reaches the extern"),
					Bound.Functions[ExternIndex].Directives.Asset,
					FString(TEXT("/MoonToon/MaterialFunctions/Utils/MF_UVChannelSwitch")));
			}
		}
	}

	// A material: the entry, its sink parameter, and a helper with the same SIGNATURE that is not
	// an entry because it is not exported (decision 11 #15).
	{
		FBindCase Case;
		BindSource(Case, TEXT(
			"void ApplyBase(inout material m) { m.BaseColor = float3(1, 0, 0); }\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    ApplyBase(m);\n"
			"    m.Opacity = 1;\n"
			"}\n"));

		if (TestTrue(FString::Printf(TEXT("the material binds: %s"), *Case.ErrorText()), Case.Bound()))
		{
			const FBoundModule& Bound = Case.Module();

			const int32 EntryIndex = Bound.FindEntryFunction();
			if (TestTrue(TEXT("the entry is found"), EntryIndex != INDEX_NONE))
			{
				const FBoundFunction& Entry = Bound.Functions[EntryIndex];
				TestEqualSensitive(TEXT("the entry's name"), Entry.Name, FString(TEXT("M_Case")));
				TestEqualSensitive(TEXT("the entry's kind"), FString(FunctionKindName(Entry.Kind)), FString(TEXT("Entry")));
				TestEqual(TEXT("the entry's material parameter is the sink"), Entry.MaterialResultParam, 0);
			}

			const int32 HelperIndex = Bound.FindFunction(TEXT("ApplyBase"));
			if (TestTrue(TEXT("the helper is bound"), HelperIndex != INDEX_NONE))
			{
				TestEqualSensitive(
					TEXT("an unexported void(inout material) is a helper, not a second entry"),
					FString(FunctionKindName(Bound.Functions[HelperIndex].Kind)),
					FString(TEXT("Helper")));
			}

			TestEqual(TEXT("one product"), Bound.Products.Num(), 1);
			if (Bound.Products.Num() == 1)
			{
				TestEqualSensitive(
					TEXT("the product kind"),
					FString(ProductKindName(Bound.Products[0].Kind)),
					FString(TEXT("Material")));
			}
		}
	}

	// @layer and @layerblend.
	{
		FBindCase Case;
		BindSource(Case, TEXT(
			"/// @layer\n"
			"export void L_Case(inout material m) { m.BaseColor = float3(0, 1, 0); }\n"));

		if (TestTrue(FString::Printf(TEXT("a layer binds: %s"), *Case.ErrorText()), Case.Bound()))
		{
			const FBoundModule& Bound = Case.Module();
			const int32 Index = Bound.FindFunction(TEXT("L_Case"));
			if (TestTrue(TEXT("the layer is bound"), Index != INDEX_NONE))
			{
				TestEqualSensitive(TEXT("the layer's kind"), FString(FunctionKindName(Bound.Functions[Index].Kind)), FString(TEXT("Layer")));
			}
			if (TestEqual(TEXT("one product"), Bound.Products.Num(), 1))
			{
				TestEqualSensitive(
					TEXT("a @layer becomes a MaterialLayer"),
					FString(ProductKindName(Bound.Products[0].Kind)),
					FString(TEXT("MaterialLayer")));
			}
		}
	}

	{
		FBindCase Case;
		BindSource(Case, TEXT(
			"/// @layerblend\n"
			"export void B_Case(material A, material B, inout material R) { R = A; }\n"));

		if (TestTrue(FString::Printf(TEXT("a layer blend binds: %s"), *Case.ErrorText()), Case.Bound()))
		{
			const FBoundModule& Bound = Case.Module();
			if (TestEqual(TEXT("one product"), Bound.Products.Num(), 1))
			{
				TestEqualSensitive(
					TEXT("a @layerblend becomes a MaterialLayerBlend"),
					FString(ProductKindName(Bound.Products[0].Kind)),
					FString(TEXT("MaterialLayerBlend")));
			}
		}
	}

	// The two refusals of decision 11 #15.
	ExpectCode(*this, TEXT("two exported entries are refused"), TEXT(
		"export void M_One(inout material m) { m.Opacity = 1; }\n"
		"export void M_Two(inout material m) { m.Opacity = 1; }\n"), TEXT("DSH6200"));

	ExpectCode(*this, TEXT("an entry plus an exported function is refused"), TEXT(
		"export float Extra(float x) { return x; }\n"
		"export void M_One(inout material m) { m.Opacity = 1; }\n"), TEXT("DSH6201"));
	return true;
}

// =================================================================================================
// Expressions: types, core ops, conversions
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderExpressionsTest,
	"DreamShader.Lang2.Binder.Expressions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderExpressionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"uniform float Gain = 1;\n"
		"uniform float3 Colour = float3(1, 1, 1);\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float A = 1 + 2;\n"                                 // 0: folded constant
		"    float3 B = Colour * Gain;\n"                        // 1: scalar broadcasts
		"    float C = dot(Colour, Colour);\n"                   // 2: Scalar typing rule
		"    float3 D = lerp(Colour, float3(0, 0, 0), 0.5);\n"   // 3: three operands
		"    bool E = Gain > 0.5;\n"                             // 4: Bool typing rule
		"    float F = -Gain;\n"                                 // 5: unary
		"    float G = Gain % 2;\n"                              // 6: `%` is Fmod
		"    float3 H = (float3)Gain;\n"                         // 7: explicit cast
		"    float I = E ? Gain : 0;\n"                          // 8: conditional
		"    m.EmissiveColor = B + D + H;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	// `1 + 2` is a constant the binder evaluated: the `!` marks bIsConstant. Both literals are
	// `int` -- while BINDING the type is the HLSL truth (IRTypes.h), and the narrowing to the
	// graph's float-only world happens at lowering -- so the Add is an int too and the `~num` is
	// the conversion the `float A` declaration asks for.
	TestEqualSensitive(TEXT("1 + 2"), InitShape(Case, TEXT("M_Case"), 0),
		FString(TEXT("(op Add! : int~num (lit 1! : int) (lit 2! : int))")));

	// A scalar used against a vector broadcasts; the result takes the wider width.
	TestEqualSensitive(TEXT("Colour * Gain"), InitShape(Case, TEXT("M_Case"), 1),
		FString(TEXT("(op Multiply : float3 (global #1 : float3) (global #0 : float~bcast))")));

	// `dot` is Scalar whatever its operands are.
	TestEqualSensitive(TEXT("dot(Colour, Colour)"), InitShape(Case, TEXT("M_Case"), 2),
		FString(TEXT("(op Dot : float (global #1 : float3) (global #1 : float3))")));

	TestEqualSensitive(TEXT("lerp(...)"), InitShape(Case, TEXT("M_Case"), 3),
		FString(TEXT("(op Lerp : float3 (global #1 : float3) (ctor! : float3 (lit 0! : int~num) (lit 0! : int~num) (lit 0! : int~num)) (lit 0.5! : float~bcast))")));

	// A comparison is bool, not float: the narrowing to 0/1 happens at lowering, not here.
	TestEqualSensitive(TEXT("Gain > 0.5"), InitShape(Case, TEXT("M_Case"), 4),
		FString(TEXT("(op Greater : bool (global #0 : float) (lit 0.5! : float))")));

	TestEqualSensitive(TEXT("-Gain"), InitShape(Case, TEXT("M_Case"), 5),
		FString(TEXT("(op Negate : float (global #0 : float))")));

	// `%` on floats is Fmod -- the one place the table's op mapping is not the operator's name.
	// The int literal converts to the op's float width; the op itself is already float.
	TestEqualSensitive(TEXT("Gain % 2"), InitShape(Case, TEXT("M_Case"), 6),
		FString(TEXT("(op Fmod : float (global #0 : float) (lit 2! : int~num))")));

	// A scalar cast to a vector is a broadcast, and the cast records it on its operand.
	TestEqualSensitive(TEXT("(float3)Gain"), InitShape(Case, TEXT("M_Case"), 7),
		FString(TEXT("(cast : float3 (global #0 : float~bcast))")));

	TestEqualSensitive(TEXT("E ? Gain : 0"), InitShape(Case, TEXT("M_Case"), 8),
		FString(TEXT("(cond : float (local #4 : bool) (global #0 : float) (lit 0! : int~num))")));

	// CONTRACT 6.13 #23: the hyperbolic spellings are binder errors. EIROp keeps entries for them --
	// the enum may only grow -- but the graph has no node that computes one, and accepting the
	// spelling only to fail further down would be the worst of both worlds.
	for (const TCHAR* Spelling : { TEXT("sinh"), TEXT("cosh"), TEXT("tanh") })
	{
		const FString Source = FString::Printf(TEXT(
			"export void M_Case(inout material m) { m.Opacity = %s(0.5); }\n"), Spelling);

		FBindCase Hyperbolic;
		BindSource(Hyperbolic, *Source);
		TestTrue(
			FString::Printf(TEXT("'%s' is refused (actual: %s)"), Spelling, *Hyperbolic.ErrorText()),
			Hyperbolic.Errors().Num() > 0);
		// DSH4246 is the code S allocated for it (LangBinderExpressions.cpp, BindCoreOpCall).
		TestTrue(
			FString::Printf(TEXT("'%s' is refused with DSH4246 (actual: %s)"), Spelling, *Hyperbolic.ErrorText()),
			HasCode(Hyperbolic, TEXT("DSH4246")));
	}

	// Locals are slots in declaration order, and the slot is what the IR builder keys on.
	const FBoundModule& Bound = Case.Module();
	const int32 EntryIndex = Bound.FindEntryFunction();
	if (TestTrue(TEXT("the entry is found"), EntryIndex != INDEX_NONE))
	{
		const FBoundFunction& Entry = Bound.Functions[EntryIndex];
		TestEqual(TEXT("nine locals"), Entry.Locals.Num(), 9);
		if (Entry.Locals.Num() == 9)
		{
			TestEqualSensitive(TEXT("slot 0"), Entry.Locals[0].Name, FString(TEXT("A")));
			TestEqualSensitive(TEXT("slot 8"), Entry.Locals[8].Name, FString(TEXT("I")));
		}
	}

	return true;
}

// =================================================================================================
// Swizzle -- CONTRACT 6.6
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderSwizzleTest,
	"DreamShader.Lang2.Binder.Swizzle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderSwizzleTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"uniform float4 V = float4(1, 2, 3, 4);\n"
		"uniform float S = 1;\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float3 A = V.rgb;\n"      // 0: rgba is rewritten to xyzw
		"    float2 B = V.xy;\n"       // 1: already canonical
		"    float C = V.a;\n"         // 2: single component
		"    float D = S.r;\n"         // 3: `.r` on a float1 is identity
		"    float E = V[3];\n"        // 4: a constant index is a swizzle
		"    float2 F = V.zx;\n"       // 5: order is the author's, not sorted
		"    m.EmissiveColor = A;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	// The canonical mask is lower-case xyzw; `rgb` never survives into the bound module.
	TestEqualSensitive(TEXT("V.rgb"), InitShape(Case, TEXT("M_Case"), 0),
		FString(TEXT("(swz xyz : float3 (global #0 : float4))")));
	TestEqualSensitive(TEXT("V.xy"), InitShape(Case, TEXT("M_Case"), 1),
		FString(TEXT("(swz xy : float2 (global #0 : float4))")));
	TestEqualSensitive(TEXT("V.a"), InitShape(Case, TEXT("M_Case"), 2),
		FString(TEXT("(swz w : float (global #0 : float4))")));

	// `.r` on a scalar is the identity, but it is still a swizzle node in the bound module: the
	// decision to drop it belongs to the lowering pass, not to the binder.
	TestEqualSensitive(TEXT("S.r"), InitShape(Case, TEXT("M_Case"), 3),
		FString(TEXT("(swz x : float (global #1 : float))")));

	// `v[3]` with a constant index is the same node as `.w` (CONTRACT 6.6).
	TestEqualSensitive(TEXT("V[3]"), InitShape(Case, TEXT("M_Case"), 4),
		FString(TEXT("(idx w : float (global #0 : float4))")));
	TestEqualSensitive(TEXT("V.zx"), InitShape(Case, TEXT("M_Case"), 5),
		FString(TEXT("(swz zx : float2 (global #0 : float4))")));

	// CONTRACT 6.13 #29: a repeated component in an RVALUE is HLSL replication and is perfectly
	// legal -- `V.xxx` is "broadcast x three ways", which the graph builds out of single-channel
	// masks and an Append. Only an out-of-range component has nothing to lower to.
	{
		FBindCase Replication;
		BindSource(Replication, TEXT(
			"uniform float4 V = float4(1, 2, 3, 4);\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float3 A = V.xxx;\n"
			"    float3 B = V.rrr;\n"
			"    float3 C = V.xxy;\n"
			"    m.EmissiveColor = A + B + C;\n"
			"}\n"));

		if (TestTrue(
				FString::Printf(TEXT("a replicating swizzle binds: %s"), *Replication.ErrorText()),
				Replication.Bound()))
		{
			// The mask is still canonical lower-case xyzw -- `rrr` is rewritten like any other
			// rgba spelling -- and the replication survives into it unchanged.
			TestEqualSensitive(TEXT("V.xxx"), InitShape(Replication, TEXT("M_Case"), 0),
				FString(TEXT("(swz xxx : float3 (global #0 : float4))")));
			TestEqualSensitive(TEXT("V.rrr is the same mask as V.xxx"), InitShape(Replication, TEXT("M_Case"), 1),
				FString(TEXT("(swz xxx : float3 (global #0 : float4))")));
			TestEqualSensitive(TEXT("V.xxy"), InitShape(Replication, TEXT("M_Case"), 2),
				FString(TEXT("(swz xxy : float3 (global #0 : float4))")));
		}
	}

	// The one place a repeat is still an error: an assignment TARGET. `Local.xxy = ...` would have
	// to write two different values into x, and there is no order in which that means anything.
	{
		FBindCase LValue;
		BindSource(LValue, TEXT(
			"uniform float4 V = float4(1, 2, 3, 4);\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float4 Local = V;\n"
			"    Local.xxy = float3(1, 2, 3);\n"
			"    m.EmissiveColor = Local.rgb;\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a repeated component in an assignment target is refused (actual: %s)"), *LValue.ErrorText()),
			LValue.Errors().Num() > 0);
		// The same code an out-of-range mask gets: DSH4230 is S's one swizzle-shape code
		// (LangBinderExpressions.cpp, `SwizzleRepeatTarget` in BindAssign).
		TestTrue(
			FString::Printf(TEXT("and by DSH4230 (actual: %s)"), *LValue.ErrorText()),
			HasCode(LValue, TEXT("DSH4230")));
	}

	// Out of range is still out of range: `.z` on a float2 names a component that is not there.
	ExpectCode(*this, TEXT("an out-of-range swizzle component is refused"), TEXT(
		"uniform float2 V = float2(1, 2);\n"
		"export void M_Case(inout material m) { m.Opacity = V.z; }\n"), TEXT("DSH4230"));

	// Mixing the two component sets in one mask. HLSL refuses it and so does the binder, with the
	// same DSH4230 every other malformed mask gets (`SwizzleMixedSets` in CanonicaliseSwizzle).
	ExpectCode(*this, TEXT("a mixed rgba/xyzw mask is refused"), TEXT(
		"uniform float4 V = float4(1, 2, 3, 4);\n"
		"export void M_Case(inout material m) { m.EmissiveColor = V.rgz; }\n"), TEXT("DSH4230"));

	return true;
}

// =================================================================================================
// The material value -- CONTRACT 6.2
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderMaterialAttributesTest,
	"DreamShader.Lang2.Binder.MaterialAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderMaterialAttributesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	const FBuiltinCatalog& Catalog = UE::DreamShader::Editor::Private::Tests::GetDreamShaderTestBuiltinCatalog();
	const int32 BaseColour = Catalog.FindMaterialAttribute(TEXT("BaseColor"));
	const int32 Emissive = Catalog.FindMaterialAttribute(TEXT("EmissiveColor"));
	const int32 Opacity = Catalog.FindMaterialAttribute(TEXT("Opacity"));
	TestTrue(TEXT("the test catalog knows BaseColor"), BaseColour != INDEX_NONE);
	TestTrue(TEXT("the test catalog knows EmissiveColor"), Emissive != INDEX_NONE);
	TestTrue(TEXT("an alias resolves to the same attribute"),
		Catalog.FindMaterialAttribute(TEXT("Emissive")) == Emissive);
	TestEqual(TEXT("the lookup is case-sensitive"),
		Catalog.FindMaterialAttribute(TEXT("basecolor")), INDEX_NONE);

	FBindCase Case;
	BindSource(Case, TEXT(
		"uniform float3 Colour = float3(1, 1, 1);\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    m.BaseColor = Colour;\n"
		"    m.Emissive = Colour;\n"
		"    m.Opacity = 1;\n"
		"    float3 Read = m.BaseColor;\n"
		"    m.Normal = Read;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	// The field index is the catalog's, so the IR builder never has to look a name up again.
	TestEqualSensitive(TEXT("m.BaseColor = Colour"), StmtExprShape(Case, TEXT("M_Case"), 0),
		*FString::Printf(TEXT("(assign : float3 (mfield #%d : float3 (param #0 : material)) (global #0 : float3))"), BaseColour));

	// An alias binds to the attribute it aliases, not to a second entry.
	TestEqualSensitive(TEXT("m.Emissive = Colour"), StmtExprShape(Case, TEXT("M_Case"), 1),
		*FString::Printf(TEXT("(assign : float3 (mfield #%d : float3 (param #0 : material)) (global #0 : float3))"), Emissive));

	// An int literal into a float1 attribute is a numeric kind change, recorded on the literal.
	TestEqualSensitive(TEXT("m.Opacity = 1"), StmtExprShape(Case, TEXT("M_Case"), 2),
		*FString::Printf(TEXT("(assign : float (mfield #%d : float (param #0 : material)) (lit 1! : int~num))"), Opacity));

	// A read of an attribute that was written earlier is legal: write-then-read (CONTRACT 6.2).
	TestEqualSensitive(TEXT("m.BaseColor read back"), InitShape(Case, TEXT("M_Case"), 3),
		*FString::Printf(TEXT("(mfield #%d : float3 (param #0 : material))"), BaseColour));

	// An attribute the catalog does not carry is a CATALOG error: the object really is a `material`,
	// the member really is looked up in the attribute table, and the message can suggest a spelling
	// that differs only in case -- which is the whole point, since 1.x matched these
	// case-insensitively and 2.0 does not.
	ExpectCode(*this, TEXT("an unknown material attribute is refused"), TEXT(
		"export void M_Case(inout material m) { m.Metallic = 1; }\n"), TEXT("DSH5200"));
	ExpectCode(*this, TEXT("a wrongly cased material attribute is refused"), TEXT(
		"export void M_Case(inout material m) { m.basecolor = float3(1, 1, 1); }\n"), TEXT("DSH5200"));

	// And the rule next door, so the two stay distinguishable: a member access on a value that is
	// not a `material` at all is the binder's general "this type has no such member" (DSH4207).
	// `Substrate.Unlit(...)` has one output, so the call is a Substrate VALUE rather than a `node`,
	// and a Substrate value has no members -- not an attribute, not a swizzle.
	ExpectCode(*this, TEXT("a member access on a non-material value is refused"), TEXT(
		"export void M_Case(inout material m)\n"
		"{\n"
		"    m.EmissiveColor = Substrate.Unlit(EmissiveColor = float3(1, 1, 1)).BaseColor;\n"
		"}\n"), TEXT("DSH4207"));
	return true;
}

// =================================================================================================
// Reflected calls -- CONTRACT 6.10
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderReflectedCallsTest,
	"DreamShader.Lang2.Binder.ReflectedCalls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderReflectedCallsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	const FBuiltinCatalog& Catalog = UE::DreamShader::Editor::Private::Tests::GetDreamShaderTestBuiltinCatalog();
	const int32 TexCoord = Catalog.FindExpression(TEXT("UE"), TEXT("TextureCoordinate"));
	const int32 VertexColor = Catalog.FindExpression(TEXT("UE"), TEXT("VertexColor"));
	const int32 SceneTexture = Catalog.FindExpression(TEXT("UE"), TEXT("SceneTexture"));
	const int32 Lerp = Catalog.FindExpression(TEXT("UE"), TEXT("LinearInterpolate"));
	const int32 Unlit = Catalog.FindExpression(TEXT("Substrate"), TEXT("Unlit"));

	// Alias and case-sensitivity, at the catalog level, before any binding.
	TestEqual(TEXT("an alias resolves to the same entry"), Catalog.FindExpression(TEXT("UE"), TEXT("TexCoord")), TexCoord);
	TestEqual(TEXT("the lookup is case-sensitive"), Catalog.FindExpression(TEXT("UE"), TEXT("texcoord")), INDEX_NONE);
	TestTrue(TEXT("but the ignore-case lookup finds it, for the did-you-mean"),
		Catalog.FindExpressionIgnoreCase(TEXT("UE"), TEXT("texcoord")) == TexCoord);
	TestEqual(TEXT("a namespace is part of the identity"), Catalog.FindExpression(TEXT("UE"), TEXT("Unlit")), INDEX_NONE);
	TestEqual(TEXT("FindExpressionByClass matches the short name"),
		Catalog.FindExpressionByClass(TEXT("MaterialExpressionVertexColor")), VertexColor);

	FBindCase Case;
	BindSource(Case, TEXT(
		"uniform float3 Colour = float3(1, 1, 1);\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float2 A = UE.TexCoord(CoordinateIndex = 0);\n"        // 0: named argument -> property
		"    float2 B = UE.TexCoord(0);\n"                          // 1: positional
		"    float4 C = UE.VertexColor();\n"                        // 2: no arguments at all
		"    float4 D = UE.SceneTexture(SceneTextureId = PostProcessInput0).Color;\n" // 3: Node + output
		"    float3 E = UE.LinearInterpolate(A = Colour, B = Colour, Alpha = 0.5);\n" // 4: const-property preference
		"    float3 F = UE.Expression(Class = \"MaterialExpressionVertexColor\").rgb;\n" // 5: by class
		"    m.EmissiveColor = E;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	// A named argument matches a pin first and a property second; `CoordinateIndex` is a property,
	// so the binding says `prop`.
	TestEqualSensitive(TEXT("UE.TexCoord(CoordinateIndex = 0)"), InitShape(Case, TEXT("M_Case"), 0),
		*FString::Printf(TEXT("(refl #%d : float2 [CoordinateIndex=prop#0 id] (lit 0! : int))"), TexCoord));

	// The 1.x spelling binds to the same property through the catalog's alias -- and resolves to
	// the CANONICAL name, so nothing downstream ever sees `Index` (CONTRACT 6.13 #35).
	{
		FBindCase Sugar;
		BindSource(Sugar, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float2 UV = UE.TexCoord(Index = 0);\n"
			"    m.Opacity = UV.x;\n"
			"}\n"));
		if (TestTrue(
				FString::Printf(TEXT("the 1.x argument spelling binds: %s"), *Sugar.ErrorText()),
				Sugar.Bound()))
		{
			TestEqualSensitive(TEXT("UE.TexCoord(Index = 0)"), InitShape(Sugar, TEXT("M_Case"), 0),
				*FString::Printf(TEXT("(refl #%d : float2 [CoordinateIndex=prop#0 id] (lit 0! : int))"), TexCoord));
		}
	}

	// A positional argument follows PositionalParameters, which names the same property.
	TestEqualSensitive(TEXT("UE.TexCoord(0)"), InitShape(Case, TEXT("M_Case"), 1),
		*FString::Printf(TEXT("(refl #%d : float2 [CoordinateIndex=prop#0 id] (lit 0! : int))"), TexCoord));

	// One output -> the result is that output's type, not `node`.
	TestEqualSensitive(TEXT("UE.VertexColor()"), InitShape(Case, TEXT("M_Case"), 2),
		*FString::Printf(TEXT("(refl #%d : float4)"), VertexColor));

	// Three outputs -> the call is a `node` and an output has to be picked by member access. The
	// enumerator argument is a SPELLING, not a value: the binder records it as a Literal typed
	// Error (BindPropertyArgument's RecordSpelling) so nothing downstream can read it as one, and
	// the dumper prints `lit ?` because the AST node under it is an identifier, not a literal.
	TestEqualSensitive(TEXT("UE.SceneTexture(...).Color"), InitShape(Case, TEXT("M_Case"), 3),
		*FString::Printf(
			TEXT("(out #0 : float4 (refl #%d : node#%d [SceneTextureId=prop#0 id] (lit ? : <error>)))"),
			SceneTexture, SceneTexture));

	// An argument matches an INPUT PIN first and a property second (CONTRACT 6.10), so `Alpha` is
	// the pin even though it has a `ConstAlpha` twin: preferring the property for a constant is
	// I2's choice at lowering time, not the binder's, and DreamShader.Lang2.IR.ReflectedArguments
	// is where that preference is pinned. A Numeric pin does not constrain the width, so the
	// binder records Identity on every one of the three and the result width comes from the
	// declared type the call feeds.
	TestEqualSensitive(TEXT("UE.LinearInterpolate(...)"), InitShape(Case, TEXT("M_Case"), 4),
		*FString::Printf(
			TEXT("(refl #%d : float3 [A=pin#0 id] [B=pin#1 id] [Alpha=pin#2 id] (global #0 : float3) (global #0 : float3) (lit 0.5! : float))"),
			Lerp));

	// `UE.Expression(Class = ...)` selects the entry; the rest binds as usual. The Class argument
	// is recorded as a property-shaped selector with no property index (it is not a property of the
	// node) and its value as an Error-typed Literal, the same "this is a spelling" rule enums take.
	TestEqualSensitive(TEXT("UE.Expression(Class = ...)"), InitShape(Case, TEXT("M_Case"), 5),
		*FString::Printf(
			TEXT("(swz xyz : float3 (refl #%d : float4 [Class=prop#-1 id] (lit MaterialExpressionVertexColor : <error>)))"),
			VertexColor));

	// Substrate is a namespace root of its own (decision 11 #11), and its result is opaque.
	{
		FBindCase Substrate;
		BindSource(Substrate, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.FrontMaterial = Substrate.Unlit(EmissiveColor = float3(0.1, 0.6, 1.0));\n"
			"}\n"));
		TestTrue(FString::Printf(TEXT("a Substrate call binds: %s"), *Substrate.ErrorText()), Substrate.Bound());
		if (Substrate.Bound())
		{
			const int32 FrontMaterial = Catalog.FindMaterialAttribute(TEXT("FrontMaterial"));
			TestEqualSensitive(TEXT("Substrate.Unlit(...)"), StmtExprShape(Substrate, TEXT("M_Case"), 0),
				*FString::Printf(
					TEXT("(assign : Substrate (mfield #%d : Substrate (param #0 : material)) (refl #%d : Substrate [EmissiveColor=pin#0 id] (ctor! : float3 (lit 0.1! : float) (lit 0.6! : float) (lit 1.0! : float))))"),
					FrontMaterial, Unlit));
		}
	}

	// The refusals.
	ExpectCode(*this, TEXT("an unknown reflected name is refused"), TEXT(
		"export void M_Case(inout material m) { m.Opacity = UE.NoSuchNode(); }\n"), TEXT("DSH5210"));
	ExpectCode(*this, TEXT("a wrongly cased reflected name is refused"), TEXT(
		"export void M_Case(inout material m) { float2 uv = UE.texcoord(0); m.Opacity = uv.x; }\n"), TEXT("DSH5210"));
	ExpectCode(*this, TEXT("a positional argument to a named-only class is refused"), TEXT(
		"export void M_Case(inout material m) { m.Opacity = UE.VertexColor(1).x; }\n"), TEXT("DSH5220"));
	// A reflected name written without an argument list at all. `UE.Time` is not a value: every
	// reflected expression is a CALL, because the argument list is where its pins and properties go.
	ExpectCode(*this, TEXT("a reflected name without an argument list is refused"), TEXT(
		"export void M_Case(inout material m) { m.Opacity = UE.Time; }\n"), TEXT("DSH5211"));
	ExpectCode(*this, TEXT("a custom output used as a value is refused"), TEXT(
		"export void M_Case(inout material m) { m.Opacity = UE.ClearCoatNormalCustomOutput(Input = float3(0, 0, 1)).x; }\n"), TEXT("DSH4231"));

	// The positive half of the same rule (CONTRACT 6.13 #23): as a STATEMENT it is perfectly legal.
	// A class with no outputs is a statement root, not an error -- DSH4231 is about reading a value
	// that does not exist, never about the call.
	{
		FBindCase Statement;
		BindSource(Statement, TEXT(
			"export void M_Case(inout material m)\n"
			"{\n"
			"    m.BaseColor = float3(1, 0, 0);\n"
			"    UE.ClearCoatNormalCustomOutput(Input = float3(0, 0, 1));\n"
			"}\n"));
		TestTrue(
			FString::Printf(TEXT("a custom output as a statement binds: %s"), *Statement.ErrorText()),
			Statement.Bound());
	}

	return true;
}

// =================================================================================================
// Textures -- CONTRACT 6.5
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderTexturesTest,
	"DreamShader.Lang2.Binder.Textures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderTexturesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"uniform Texture2D Tex;\n"
		"uniform float2 UV = float2(0, 0);\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float4 A = Tex.Sample(UV);\n"                      // 0
		"    float4 B = Tex.Sample(Tex, UV);\n"                 // 1: an explicit sampler
		"    float4 C = Texture2DSample(Tex, Tex, UV);\n"       // 2: the HLSL spelling
		"    float4 D = Tex.SampleLevel(UV, 0);\n"              // 3: with an explicit mip
		"    m.EmissiveColor = (A + B + C + D).rgb;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();
	const FFunctionDecl* Entry = FindFunctionDecl(*Case.Parse.Module, TEXT("M_Case"));
	if (!TestNotNull(TEXT("the entry is in the module"), Entry))
	{
		return false;
	}

	// CONTRACT 6.5: every spelling normalises to ONE bound kind whose arguments are named from one
	// vocabulary -- Texture, UV, Sampler, Level. Which of the four a spelling supplies is the whole
	// difference between them, and the names are what let the IR builder fill fixed operand slots
	// (CONTRACT 6.13 #14) without re-reading the syntax.
	//
	// Asserted as the SET of targets rather than as a whole-tree dump on purpose: whether the
	// receiver of `Tex.Sample(UV)` is recorded as argument -1 or as a synthetic argument 0 is an
	// internal choice the contract does not make, and a test that pinned it would be pinning the
	// binder's bookkeeping rather than the normalisation everyone downstream depends on.
	struct FExpected
	{
		int32 Statement;
		const TCHAR* Spelling;
		bool bSampler;
		bool bLevel;
	};

	const FExpected Expected[] =
	{
		{ 0, TEXT("Tex.Sample(UV)"),                false, false },
		{ 1, TEXT("Tex.Sample(Tex, UV)"),           true,  false },
		{ 2, TEXT("Texture2DSample(Tex, Tex, UV)"), true,  false },
		{ 3, TEXT("Tex.SampleLevel(UV, 0)"),        false, true  },
	};

	for (const FExpected& One : Expected)
	{
		const FStmt* Stmt = BodyStatement(*Entry, One.Statement);
		const FVarDeclStmt* VarDecl = Stmt ? Stmt->As<FVarDeclStmt>() : nullptr;
		const FExpr* Init = (VarDecl && VarDecl->Declarators.Num() > 0)
			? VarDecl->Declarators[0].Initializer.Get()
			: nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("'%s' is a declaration with an initializer"), One.Spelling), Init))
		{
			continue;
		}

		const FBoundExpr* Info = Bound.Find(*Init);
		if (!TestNotNull(*FString::Printf(TEXT("'%s' is bound"), One.Spelling), Info))
		{
			continue;
		}

		TestEqual(
			FString::Printf(TEXT("'%s' binds as a TextureSample"), One.Spelling),
			static_cast<int32>(Info->Kind),
			static_cast<int32>(EBoundExprKind::TextureSample));
		TestEqualSensitive(
			*FString::Printf(TEXT("'%s' is a float4"), One.Spelling),
			Info->Type.ToString(), FString(TEXT("float4")));

		bool bHasUV = false;
		bool bHasSampler = false;
		bool bHasLevel = false;
		for (const FBoundArgument& Argument : Info->Args)
		{
			// The vocabulary is closed: an argument normalised to anything else means the binder
			// grew a fifth name and nothing downstream knows what to do with it.
			const bool bKnown =
				Argument.Target.Equals(TEXT("Texture"), ESearchCase::CaseSensitive)
				|| Argument.Target.Equals(TEXT("UV"), ESearchCase::CaseSensitive)
				|| Argument.Target.Equals(TEXT("Sampler"), ESearchCase::CaseSensitive)
				|| Argument.Target.Equals(TEXT("Level"), ESearchCase::CaseSensitive);
			TestTrue(
				FString::Printf(TEXT("'%s' argument target '%s' is one of Texture/UV/Sampler/Level"),
					One.Spelling, *Argument.Target),
				bKnown);

			bHasUV |= Argument.Target.Equals(TEXT("UV"), ESearchCase::CaseSensitive);
			bHasSampler |= Argument.Target.Equals(TEXT("Sampler"), ESearchCase::CaseSensitive);
			bHasLevel |= Argument.Target.Equals(TEXT("Level"), ESearchCase::CaseSensitive);
		}

		TestTrue(FString::Printf(TEXT("'%s' binds a UV"), One.Spelling), bHasUV);
		TestEqual(FString::Printf(TEXT("'%s' binds a Sampler"), One.Spelling), bHasSampler, One.bSampler);
		TestEqual(FString::Printf(TEXT("'%s' binds a Level"), One.Spelling), bHasLevel, One.bLevel);
	}

	// A texture that arrived as a function input samples the same way -- there is no second path.
	{
		FBindCase Input;
		BindSource(Input, TEXT(
			"export void SampleTinted(Texture2D Tex, float2 UV, out float3 RGB)\n"
			"{\n"
			"    float4 Texel = Tex.Sample(UV);\n"
			"    RGB = Texel.rgb;\n"
			"}\n"));
		if (TestTrue(FString::Printf(TEXT("a texture parameter binds: %s"), *Input.ErrorText()), Input.Bound()))
		{
			const FFunctionDecl* Function = FindFunctionDecl(*Input.Parse.Module, TEXT("SampleTinted"));
			const FStmt* Stmt = Function ? BodyStatement(*Function, 0) : nullptr;
			const FVarDeclStmt* VarDecl = Stmt ? Stmt->As<FVarDeclStmt>() : nullptr;
			const FExpr* Init = (VarDecl && VarDecl->Declarators.Num() > 0)
				? VarDecl->Declarators[0].Initializer.Get()
				: nullptr;
			const FBoundExpr* Info = Init ? Input.Module().Find(*Init) : nullptr;
			if (TestNotNull(TEXT("the sample on a parameter is bound"), Info))
			{
				TestEqual(
					TEXT("sampling a texture PARAMETER is the same bound kind"),
					static_cast<int32>(Info->Kind),
					static_cast<int32>(EBoundExprKind::TextureSample));
			}
		}
	}

	return true;
}

// =================================================================================================
// Calls to functions of this file -- CONTRACT decision "Helper vs export vs extern"
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderCallsTest,
	"DreamShader.Lang2.Binder.Calls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderCallsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"/// @asset /Game/Functions/MF_External\n"
		"extern float MF_External(float x);\n"
		"float Helper(float x, float y) { return x + y; }\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float A = Helper(1, 2);\n"          // 0
		"    float B = Helper(y = 2, x = 1);\n"  // 1: named arguments, out of order
		"    float C = MF_External(A);\n"        // 2
		"    m.Opacity = A + B + C;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();
	const int32 HelperIndex = Bound.FindFunction(TEXT("Helper"));
	const int32 ExternIndex = Bound.FindFunction(TEXT("MF_External"));

	// The argument table of a user call is in PARAMETER order (the binder walks the parameters and
	// pulls the argument that matched each), and an int literal into a float parameter converts.
	TestEqualSensitive(TEXT("Helper(1, 2)"), InitShape(Case, TEXT("M_Case"), 0),
		*FString::Printf(TEXT("(call #%d : float [x=pin#0 num] [y=pin#1 num] (lit 1! : int~num) (lit 2! : int~num))"), HelperIndex));

	// A named argument binds to the PARAMETER it names, whatever order it was written in: the
	// TargetIndex, not the argument's position, is what the IR builder uses. So the table below is
	// the same as the one above -- what moved is the AST order the children are printed in.
	TestEqualSensitive(TEXT("Helper(y = 2, x = 1)"), InitShape(Case, TEXT("M_Case"), 1),
		*FString::Printf(TEXT("(call #%d : float [x=pin#0 num] [y=pin#1 num] (lit 2! : int~num) (lit 1! : int~num))"), HelperIndex));

	TestEqualSensitive(TEXT("MF_External(A)"), InitShape(Case, TEXT("M_Case"), 2),
		*FString::Printf(TEXT("(call #%d : float [x=pin#0 id] (local #0 : float))"), ExternIndex));

	// Recursion is a property of the bound function, decided before anyone tries to inline it.
	{
		FBindCase Recursive;
		BindSource(Recursive, TEXT(
			"float Ping(float x) { return Pong(x); }\n"
			"float Pong(float x) { return Ping(x); }\n"
			"export void M_Case(inout material m) { m.Opacity = Ping(1); }\n"));
		if (Recursive.Bind.Bound.IsValid())
		{
			const FBoundModule& RecursiveModule = *Recursive.Bind.Bound;
			const int32 PingIndex = RecursiveModule.FindFunction(TEXT("Ping"));
			if (TestTrue(TEXT("Ping is bound"), PingIndex != INDEX_NONE))
			{
				TestTrue(TEXT("mutual recursion is recorded on the function"), RecursiveModule.Functions[PingIndex].bRecursive);
			}
		}
	}

	return true;
}

// =================================================================================================
// Regions -- CONTRACT 6.8
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderRegionsTest,
	"DreamShader.Lang2.Binder.Regions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderRegionsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"#pragma region Parameters\n"
		"uniform float A = 1;\n"
		"uniform float B = 2;\n"
		"#pragma endregion\n"
		"\n"
		"#pragma layout(Node, Var = \"Sum\", X = 120, Y = -40)\n"
		"\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    #pragma region Outer\n"
		"    float Sum = A + B;\n"
		"    #pragma region Inner\n"
		"    float Scaled = Sum * 2;\n"
		"    #pragma endregion\n"
		"    float Final = Scaled + 1;\n"
		"    #pragma endregion\n"
		"    m.Opacity = Final;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();

	// File-scope and in-body regions land in ONE tree, which is what the IR graph copies.
	TestEqual(TEXT("three regions"), Bound.Regions.Num(), 3);
	if (Bound.Regions.Num() == 3)
	{
		TestEqualSensitive(TEXT("region 0"), Bound.Regions[0].Name, FString(TEXT("Parameters")));
		TestEqual(TEXT("region 0 is a root"), Bound.Regions[0].Parent, INDEX_NONE);
		TestEqualSensitive(TEXT("region 1"), Bound.Regions[1].Name, FString(TEXT("Outer")));
		TestEqual(TEXT("region 1 is a root"), Bound.Regions[1].Parent, INDEX_NONE);
		TestEqualSensitive(TEXT("region 2"), Bound.Regions[2].Name, FString(TEXT("Inner")));
		TestEqual(TEXT("region 2 nests inside Outer"), Bound.Regions[2].Parent, 1);
	}

	// Every statement carries its INNERMOST region, which is what stamps FIRNode::Region.
	const FFunctionDecl* Entry = FindFunctionDecl(*Case.Parse.Module, TEXT("M_Case"));
	if (TestNotNull(TEXT("the entry is in the module"), Entry) && Entry->Body)
	{
		// The statement list holds the FPragmaStmt lines too; find the declarations by name.
		auto RegionOfLocal = [&Bound, Entry](const TCHAR* Name) -> int32
		{
			for (const FStmtPtr& Stmt : Entry->Body->Statements)
			{
				const FVarDeclStmt* VarDecl = Stmt ? Stmt->As<FVarDeclStmt>() : nullptr;
				if (VarDecl && VarDecl->Declarators.Num() > 0
					&& VarDecl->Declarators[0].Name.Equals(Name, ESearchCase::CaseSensitive))
				{
					const int32* Region = Bound.StatementRegions.Find(VarDecl);
					return Region ? *Region : INDEX_NONE;
				}
			}
			return INDEX_NONE;
		};

		TestEqual(TEXT("Sum sits in Outer"), RegionOfLocal(TEXT("Sum")), 1);
		TestEqual(TEXT("Scaled sits in Inner"), RegionOfLocal(TEXT("Scaled")), 2);
		TestEqual(TEXT("Final is back in Outer"), RegionOfLocal(TEXT("Final")), 1);
	}

	// A layout pragma is carried through untouched -- the binder does not interpret coordinates.
	if (TestEqual(TEXT("one layout hint"), Bound.LayoutHints.Num(), 1))
	{
		TestEqualSensitive(TEXT("the hint's kind"), Bound.LayoutHints[0].Kind, FString(TEXT("Node")));
		TestEqualSensitive(TEXT("the hint's variable"), Bound.LayoutHints[0].Var, FString(TEXT("Sum")));
		TestEqual(TEXT("the hint's X"), Bound.LayoutHints[0].X, 120);
		TestEqual(TEXT("the hint's Y"), Bound.LayoutHints[0].Y, -40);
	}

	ExpectCode(*this, TEXT("an unbalanced file-scope region is refused"), TEXT(
		"#pragma region Open\n"
		"uniform float A = 1;\n"
		"export void M_Case(inout material m) { m.Opacity = A; }\n"), TEXT("DSH4240"));
	ExpectCode(*this, TEXT("an unbalanced in-body region is refused"), TEXT(
		"export void M_Case(inout material m)\n"
		"{\n"
		"    #pragma endregion\n"
		"    m.Opacity = 1;\n"
		"}\n"), TEXT("DSH4240"));
	return true;
}

// =================================================================================================
// Loops -- decision "Loops"
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderLoopsTest,
	"DreamShader.Lang2.Binder.Loops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderLoopsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"uniform float Gain = 1;\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    float Sum = 0;\n"
		"    for (int i = 0; i < 4; ++i)\n"
		"    {\n"
		"        Sum = Sum + Gain;\n"
		"    }\n"
		"    m.Opacity = Sum;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();
	const FFunctionDecl* Entry = FindFunctionDecl(*Case.Parse.Module, TEXT("M_Case"));
	if (TestNotNull(TEXT("the entry is in the module"), Entry) && Entry->Body)
	{
		const FForStmt* Loop = nullptr;
		for (const FStmtPtr& Stmt : Entry->Body->Statements)
		{
			if (const FForStmt* Candidate = Stmt ? Stmt->As<FForStmt>() : nullptr)
			{
				Loop = Candidate;
				break;
			}
		}

		if (TestNotNull(TEXT("the for statement is in the body"), Loop))
		{
			const int32* TripCount = Bound.LoopTripCounts.Find(Loop);
			if (TestNotNull(TEXT("the binder proved the loop bounded"), TripCount))
			{
				TestEqual(TEXT("i goes 0..3"), *TripCount, 4);
			}
		}
	}

	// A loop whose bound is a uniform is NOT in the table: "absent" is how the map says
	// "not unrollable", and DSH4360 is raised when the lowering pass finds it missing.
	{
		FBindCase Unbounded;
		BindSource(Unbounded, TEXT(
			"uniform float Count = 4;\n"
			"export void M_Case(inout material m)\n"
			"{\n"
			"    float Sum = 0;\n"
			"    for (float i = 0; i < Count; ++i) { Sum = Sum + 1; }\n"
			"    m.Opacity = Sum;\n"
			"}\n"));
		if (Unbounded.Bind.Bound.IsValid())
		{
			TestEqual(
				TEXT("a loop with a dynamic bound records no trip count"),
				Unbounded.Bind.Bound->LoopTripCounts.Num(),
				0);
		}
	}

	return true;
}

// =================================================================================================
// Includes -- CONTRACT 6.11
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderIncludesTest,
	"DreamShader.Lang2.Binder.Includes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderIncludesTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FHeaderMap Headers;
	Headers.Add(TEXT("Shared.dsh"), TEXT(
		"uniform float SharedGain = 2;\n"
		"float SharedHelper(float x) { return x * SharedGain; }\n"));

	FBindCase Case;
	BindSource(Case, TEXT(
		"#include \"Shared.dsh\"\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    m.Opacity = SharedHelper(0.5);\n"
		"}\n"), &Headers);

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();
	TestEqual(TEXT("one include was resolved"), Bound.IncludePaths.Num(), 1);
	TestEqual(TEXT("one included module is kept"), Bound.Included.Num(), 1);

	// The header's declarations joined the module's tables, with File saying where they came from.
	const int32 GlobalIndex = Bound.FindGlobal(TEXT("SharedGain"));
	if (TestTrue(TEXT("the header's uniform is visible"), GlobalIndex != INDEX_NONE))
	{
		TestTrue(TEXT("it remembers which file it came from"),
			Bound.Globals[GlobalIndex].File.Contains(TEXT("Shared.dsh"), ESearchCase::CaseSensitive));
	}
	TestTrue(TEXT("the header's helper is callable"), Bound.FindFunction(TEXT("SharedHelper")) != INDEX_NONE);

	// A name defined in two files is a conflict, not a silent shadow.
	{
		FHeaderMap Duplicate;
		Duplicate.Add(TEXT("Shared.dsh"), TEXT("uniform float SharedGain = 2;\n"));

		FBindCase Conflict;
		BindSource(Conflict, TEXT(
			"#include \"Shared.dsh\"\n"
			"uniform float SharedGain = 3;\n"
			"export void M_Case(inout material m) { m.Opacity = SharedGain; }\n"), &Duplicate);
		TestTrue(
			FString::Printf(TEXT("a duplicate definition is refused: DSH4210 (actual: %s)"), *Conflict.ErrorText()),
			HasCode(Conflict, TEXT("DSH4210")));
	}

	// A cycle terminates and is named, rather than recursing until the stack runs out.
	{
		FHeaderMap Cycle;
		Cycle.Add(TEXT("A.dsh"), TEXT("#include \"B.dsh\"\nfloat FromA(float x) { return x; }\n"));
		Cycle.Add(TEXT("B.dsh"), TEXT("#include \"A.dsh\"\nfloat FromB(float x) { return x; }\n"));

		FBindCase Cyclic;
		BindSource(Cyclic, TEXT(
			"#include \"A.dsh\"\n"
			"export void M_Case(inout material m) { m.Opacity = FromA(1); }\n"), &Cycle);
		TestTrue(
			FString::Printf(TEXT("an include cycle is refused: DSH4211 (actual: %s)"), *Cyclic.ErrorText()),
			HasCode(Cyclic, TEXT("DSH4211")));
	}

	// Without a resolver an include cannot be honoured, and silently ignoring it would compile a
	// file against half its declarations.
	{
		FBindCase NoResolver;
		BindSource(NoResolver, TEXT(
			"#include \"Shared.dsh\"\n"
			"export void M_Case(inout material m) { m.Opacity = 1; }\n"));
		TestTrue(
			FString::Printf(TEXT("an include without a resolver is refused (actual: %s)"), *NoResolver.ErrorText()),
			NoResolver.Errors().Num() > 0);
	}

	return true;
}

// =================================================================================================
// `#pragma material` and the products -- CONTRACT 6.9, decision "Product naming"
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderPragmaMaterialTest,
	"DreamShader.Lang2.Binder.PragmaMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderPragmaMaterialTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::IR;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"#pragma material(ShadingModel = Unlit, BlendMode = Additive)\n"
		"#pragma material(Backend = ThinCustom, bUsedWithNiagaraSprites = true)\n"
		"export void M_Case(inout material m) { m.EmissiveColor = float3(1, 1, 1); }\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();

	// Two `#pragma material` lines merge into one table.
	const FString* ShadingModel = Bound.MaterialSettings.Find(TEXT("ShadingModel"));
	const FString* BlendMode = Bound.MaterialSettings.Find(TEXT("BlendMode"));
	const FString* Niagara = Bound.MaterialSettings.Find(TEXT("bUsedWithNiagaraSprites"));
	if (TestNotNull(TEXT("ShadingModel survived"), ShadingModel)) { TestEqualSensitive(TEXT("ShadingModel"), *ShadingModel, FString(TEXT("Unlit"))); }
	if (TestNotNull(TEXT("BlendMode survived"), BlendMode)) { TestEqualSensitive(TEXT("BlendMode"), *BlendMode, FString(TEXT("Additive"))); }
	if (TestNotNull(TEXT("the second line merged in"), Niagara)) { TestEqualSensitive(TEXT("bUsedWithNiagaraSprites"), *Niagara, FString(TEXT("true"))); }

	// Backend is NOT a setting: it is removed into the product, so nothing downstream has to know
	// that one key of the table means something different from all the others.
	TestNull(TEXT("Backend was removed from the settings"), Bound.MaterialSettings.Find(TEXT("Backend")));
	if (TestEqual(TEXT("one product"), Bound.Products.Num(), 1))
	{
		TestEqual(
			TEXT("the product carries the ThinCustom backend"),
			static_cast<int32>(Bound.Products[0].Backend),
			static_cast<int32>(EIRBackend::ThinCustom));
	}

	// `@name` with a full path is an override; a bare name replaces the leaf only.
	{
		FBindCase Override;
		BindSource(Override, TEXT(
			"/// @name /Game/Generated/M_Renamed\n"
			"export void M_Case(inout material m) { m.Opacity = 1; }\n"));
		if (TestTrue(FString::Printf(TEXT("the override binds: %s"), *Override.ErrorText()), Override.Bound())
			&& TestEqual(TEXT("one product"), Override.Module().Products.Num(), 1))
		{
			TestEqualSensitive(
				TEXT("a full path becomes an AssetPathOverride"),
				Override.Module().Products[0].AssetPathOverride,
				FString(TEXT("/Game/Generated/M_Renamed")));
		}
	}

	{
		FBindCase Leaf;
		BindSource(Leaf, TEXT(
			"/// @name M_Renamed\n"
			"export void M_Case(inout material m) { m.Opacity = 1; }\n"));
		if (TestTrue(FString::Printf(TEXT("the leaf rename binds: %s"), *Leaf.ErrorText()), Leaf.Bound())
			&& TestEqual(TEXT("one product"), Leaf.Module().Products.Num(), 1))
		{
			TestEqualSensitive(TEXT("a bare name replaces the leaf"), Leaf.Module().Products[0].AssetName, FString(TEXT("M_Renamed")));
			TestTrue(TEXT("and sets no path override"), Leaf.Module().Products[0].AssetPathOverride.IsEmpty());
		}
	}

	return true;
}

// =================================================================================================
// Structs -- decision 11 #1(c)
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderStructsTest,
	"DreamShader.Lang2.Binder.Structs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderStructsTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"struct ToonInputs\n"
		"{\n"
		"    float3 Albedo;\n"
		"    float Shadow;\n"
		"};\n"
		"export void M_Case(inout material m)\n"
		"{\n"
		"    ToonInputs In = ToonInputs(float3(1, 1, 1), 0.5);\n"
		"    m.BaseColor = In.Albedo * In.Shadow;\n"
		"}\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FBoundModule& Bound = Case.Module();
	const int32 StructIndex = Bound.FindStruct(TEXT("ToonInputs"));
	if (TestTrue(TEXT("the struct is bound"), StructIndex != INDEX_NONE))
	{
		const FBoundStruct& Struct = Bound.Structs[StructIndex];
		TestEqual(TEXT("two fields"), Struct.Fields.Num(), 2);
		TestEqual(TEXT("FindField is by name"), Struct.FindField(TEXT("Shadow")), 1);
		TestEqual(TEXT("FindField is case-sensitive"), Struct.FindField(TEXT("shadow")), INDEX_NONE);
		if (Struct.Fields.Num() == 2)
		{
			TestEqualSensitive(TEXT("field 0 type"), Struct.Fields[0].Type.ToString(), FString(TEXT("float3")));
			TestEqualSensitive(TEXT("field 1 type"), Struct.Fields[1].Type.ToString(), FString(TEXT("float")));
		}
	}

	// A struct value is a compile-time aggregate; a field read is a FieldIndex, never a node.
	TestEqualSensitive(TEXT("ToonInputs(...)"), InitShape(Case, TEXT("M_Case"), 0),
		*FString::Printf(
			TEXT("(sctor : struct#%d (ctor! : float3 (lit 1! : int~num) (lit 1! : int~num) (lit 1! : int~num)) (lit 0.5! : float))"),
			StructIndex));

	return true;
}

// =================================================================================================
// The symbol index -- plan 13.4
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDreamShaderLang2BinderSymbolIndexTest,
	"DreamShader.Lang2.Binder.SymbolIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamShaderLang2BinderSymbolIndexTest::RunTest(const FString& Parameters)
{
	using namespace UE::DreamShader::Lang;
	using namespace UE::DreamShader::Editor::Private::Lang2BinderTests;

	FBindCase Case;
	BindSource(Case, TEXT(
		"/// @group Look\n"
		"uniform float Gain = 1;\n"
		"float Helper(float x) { return x * Gain; }\n"
		"export void M_Case(inout material m) { m.Opacity = Helper(0.5); }\n"));

	if (!TestTrue(FString::Printf(TEXT("the module binds: %s"), *Case.ErrorText()), Case.Bound()))
	{
		return false;
	}

	const FString Json = BuildDreamShaderSymbolIndexJson(Case.Module());
	TestTrue(TEXT("the index names its schema"),
		Json.Contains(TEXT("dreamshader-symbol-index"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("the index carries the uniform"), Json.Contains(TEXT("Gain"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("the index carries the helper"), Json.Contains(TEXT("Helper"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("the index carries the entry"), Json.Contains(TEXT("M_Case"), ESearchCase::CaseSensitive));

	// An editor wants navigation on a BROKEN file most of all, so the index has to survive errors.
	{
		FBindCase Broken;
		BindSource(Broken, TEXT(
			"uniform float Gain = 1;\n"
			"export void M_Case(inout material m) { m.NoSuchAttribute = Gain; }\n"));
		if (TestTrue(TEXT("the broken file still produced a bound module"), Broken.Bind.Bound.IsValid()))
		{
			const FString BrokenJson = BuildDreamShaderSymbolIndexJson(*Broken.Bind.Bound);
			TestTrue(TEXT("the index is still built for a file with errors"), !BrokenJson.IsEmpty());
			TestTrue(TEXT("and still names what did bind"), BrokenJson.Contains(TEXT("Gain"), ESearchCase::CaseSensitive));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
