// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Statements: scopes and slots, control flow, the regions a body draws around its own nodes, and
// the one analysis this pass owes the IR builder -- the trip count of a loop.
//
// On loops. The graph has no iteration: a `for` either unrolls at compile time or the code belongs
// in a `/// @custom` function, and the contract is explicit that a loop the binder cannot PROVE
// bounded must not appear in LoopTripCounts at all. So the proof below is deliberately narrow --
// one induction variable, a constant initial value, a comparison against a constant, a step that is
// itself constant, no `break`, no `continue`, and no other write to the variable anywhere in the
// body. Everything outside that shape is left out of the map and the IR builder refuses it with a
// message that names `@custom`. A wrong "unrollable" answer would silently produce the wrong graph;
// a missing one produces a diagnostic. Only one of those is recoverable.

#include "LangBinderInternal.h"

#include "IR/IR.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.Statements"

namespace UE::DreamShader::Lang::Private
{
	namespace
	{
		/**
		 * The name a write lands on: `v`, `v.x`, `v[0]` and `s.field.y` all write `v`. Null when the
		 * target is not rooted in a name (a call result, a literal) or the tree is incomplete.
		 */
		const FExpr* RootOfWriteTarget(const FExpr& Target)
		{
			const FExpr* Current = &Target;
			while (Current != nullptr)
			{
				if (Current->Kind == ENodeKind::MemberExpr)
				{
					Current = static_cast<const FMemberExpr*>(Current)->Object.Get();
				}
				else if (Current->Kind == ENodeKind::IndexExpr)
				{
					Current = static_cast<const FIndexExpr*>(Current)->Object.Get();
				}
				else
				{
					break;
				}
			}
			return Current;
		}

		/** True when the statement holds a `break` or `continue` that belongs to the loop around it. */
		bool HasLoopJump(const FStmt* Stmt)
		{
			if (!Stmt)
			{
				return false;
			}

			switch (Stmt->Kind)
			{
			case ENodeKind::BreakStmt:
			case ENodeKind::ContinueStmt:
				return true;

			case ENodeKind::BlockStmt:
			{
				const FBlockStmt& Block = *static_cast<const FBlockStmt*>(Stmt);
				for (const FStmtPtr& Child : Block.Statements)
				{
					if (HasLoopJump(Child.Get()))
					{
						return true;
					}
				}
				return false;
			}

			case ENodeKind::IfStmt:
			{
				const FIfStmt& If = *static_cast<const FIfStmt*>(Stmt);
				return HasLoopJump(If.Then.Get()) || HasLoopJump(If.Else.Get());
			}

			// An inner loop owns its own `break` and `continue`, so the walk stops there.
			case ENodeKind::ForStmt:
			case ENodeKind::WhileStmt:
			case ENodeKind::DoWhileStmt:
				return false;

			default:
				return false;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Scopes
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::PushScope()
	{
		Scopes.AddDefaulted();
	}

	void FLangBinder::PopScope()
	{
		if (Scopes.Num() > 0)
		{
			Scopes.Pop();
		}
	}

	int32 FLangBinder::DeclareLocal(const FString& Name, const IR::FIRType& Type, const FDeclarator* Decl, int32 ArrayCount, const FLangSpan& Span)
	{
		if (!CurrentFunction)
		{
			return INDEX_NONE;
		}
		if (Scopes.Num() == 0)
		{
			PushScope();
		}

		// Same block: a redefinition. An outer block: shadowing, which HLSL allows and this language
		// does not -- in a graph the two variables become two unrelated nodes with one name, and the
		// node descriptions a reader then sees are a lie.
		bool bRedeclared = false;
		for (const TPair<FString, int32>& Existing : Scopes.Last().Names)
		{
			if (Existing.Key.Equals(Name, ESearchCase::CaseSensitive))
			{
				bRedeclared = true;
				break;
			}
		}

		if (bRedeclared)
		{
			Diagnostics.Error(
				TEXT("DSH4220"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("LocalRedeclared", "'{0}' is already declared in this block."),
					FText::FromString(Name)));
		}
		else if (FindLocal(Name) != INDEX_NONE)
		{
			Diagnostics.Error(
				TEXT("DSH4220"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("LocalShadowsLocal", "'{0}' hides a variable of the same name from an enclosing block; give one of them another name."),
					FText::FromString(Name)));
		}
		else if (FindParam(Name) != INDEX_NONE)
		{
			Diagnostics.Error(
				TEXT("DSH4220"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("LocalShadowsParam", "'{0}' hides the parameter of the same name; give one of them another name."),
					FText::FromString(Name)));
		}
		else if (FindGlobal(Name) != INDEX_NONE)
		{
			// Allowed, because a header the file did not write may declare anything; said out loud,
			// because from here on the uniform of that name is unreachable in this function.
			Diagnostics.Warning(
				TEXT("DSH4251"),
				CurrentFile,
				Span,
				FText::Format(
					LOCTEXT("LocalShadowsGlobal", "'{0}' hides the file-scope declaration of the same name for the rest of this function."),
					FText::FromString(Name)));
		}

		FBoundLocal Local;
		Local.Name = Name;
		Local.Type = Type;
		Local.Decl = Decl;
		Local.ArrayCount = ArrayCount;
		Local.Span = Span;

		const int32 Slot = CurrentFunction->Locals.Add(MoveTemp(Local));
		LocalArrayValues.AddDefaulted();
		Scopes.Last().Names.Emplace(Name, Slot);
		return Slot;
	}

	void FLangBinder::RecordLocalWrite(const FNode& Node, const FExpr& Target)
	{
		// `v.x = 1` and `v[0] = 1` write `v`: the write is filed under the root name. That is what
		// lets an `out` parameter be satisfied one component at a time, and what keeps ProveTripCount
		// honest about `i.x = 5` on an induction variable.
		const FExpr* Root = RootOfWriteTarget(Target);
		const FBoundExpr* Binding = Root ? Lookup(*Root) : nullptr;
		if (Binding == nullptr)
		{
			return;
		}

		if (Binding->Kind == EBoundExprKind::Local)
		{
			LocalWrites.Emplace(&Node, Binding->LocalSlot);
		}
		else if (Binding->Kind == EBoundExprKind::Param && ParamWrites.IsValidIndex(Binding->Index))
		{
			ParamWrites[Binding->Index] = true;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Globals
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindGlobals()
	{
		CurrentFunctionIndex = INDEX_NONE;
		CurrentFunction = nullptr;
		Scopes.Reset();
		LocalArrayValues.Reset();
		LocalWrites.Reset();
		ParamWrites.Reset();
		CurrentRegion = INDEX_NONE;
		OuterRegion = INDEX_NONE;

		for (int32 Index = 0; Index < Bound.Globals.Num(); ++Index)
		{
			const FBoundGlobal& Global = Bound.Globals[Index];
			if (!Global.Decl)
			{
				continue;
			}

			const FDeclarator& Declarator = Global.Decl->Declarator;
			if (!Declarator.Initializer)
			{
				continue;
			}

			// A `static const` declared in a header is bound here, so its initializer's spans lie
			// in the header.
			CurrentFile = Global.File;

			const IR::FIRType DeclaredType = Global.Type;
			const int32 ArrayCount = GetGlobalArrayCount(Index);

			if (ArrayCount > 0)
			{
				TArray<double> Values;
				BindArrayInitializer(*Declarator.Initializer, DeclaredType, ArrayCount, Values);
				if (GlobalArrayValues.IsValidIndex(Index))
				{
					GlobalArrayValues[Index] = MoveTemp(Values);
				}
				continue;
			}

			BindExpr(*Declarator.Initializer, &DeclaredType);
			Convert(
				*Declarator.Initializer,
				DeclaredType,
				EConversionSite::Assignment,
				FText::Format(
					LOCTEXT("GlobalInitializer", "The initializer of '{0}'"),
					FText::FromString(Global.Name)));

			if (Global.bIsConstant && !IsConstantExpr(*Declarator.Initializer))
			{
				// CONTRACT §6.1: a `static const` becomes a Constant node, so its value has to exist
				// before anything runs.
				Diagnostics.Error(
					TEXT("DSH7210"),
					CurrentFile,
					Declarator.Initializer->Span,
					FText::Format(
						LOCTEXT("ConstantNotConstant", "'{0}' is a compile-time constant, and this initializer is not one; a constant is built from literals and other constants."),
						FText::FromString(Global.Name)));
			}
		}

		CurrentFile = RootModule.FilePath;
	}

	// ---------------------------------------------------------------------------------------------
	// Bodies
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::BindFunctionBody(int32 FunctionIndex)
	{
		if (!Bound.Functions.IsValidIndex(FunctionIndex))
		{
			return;
		}

		CurrentFunctionIndex = FunctionIndex;
		CurrentFunction = &Bound.Functions[FunctionIndex];
		// A helper that came from a `.dsh` has its whole body bound here, and every span in it lies
		// in that header.
		CurrentFile = CurrentFunction->File;
		Scopes.Reset();
		LocalArrayValues.Reset();
		LocalWrites.Reset();
		ParamWrites.Init(false, CurrentFunction->Params.Num());
		BodyRegionStack.Reset();
		CurrentCallees.Reset();
		LoopDepth = 0;

		// A body's regions nest inside the file-scope box the function itself was declared in.
		OuterRegion = INDEX_NONE;
		if (CurrentFunction->Decl)
		{
			if (const int32* Found = Bound.StatementRegions.Find(CurrentFunction->Decl))
			{
				OuterRegion = *Found;
			}
		}
		CurrentRegion = OuterRegion;

		PushScope();

		// Parameter defaults are bound in the function's own scope, which is what lets one default
		// refer to a parameter beside it -- the same thing 1.x's second pass over the inputs allowed.
		for (int32 Index = 0; Index < CurrentFunction->Params.Num(); ++Index)
		{
			const FExpr* Default = CurrentFunction->Params[Index].Default;
			if (!Default)
			{
				continue;
			}

			const IR::FIRType ParamType = CurrentFunction->Params[Index].Type;
			const FString ParamName = CurrentFunction->Params[Index].Name;

			BindExpr(*Default, &ParamType);
			Convert(
				*Default,
				ParamType,
				EConversionSite::Assignment,
				FText::Format(
					LOCTEXT("ParameterDefault", "The default value of '{0}'"),
					FText::FromString(ParamName)));
		}

		const FFunctionDecl* Decl = CurrentFunction->Decl;
		if (Decl && Decl->Body && !Decl->bOpaqueBody)
		{
			// The body's own block pushes a scope of its own, so a local may not hide a parameter.
			BindBlock(*Decl->Body);

			// An `out` parameter the body never writes (CONTRACT §6.13 #48). Whole, swizzled, indexed
			// and field writes all count, and so does handing it to another call's `out` / `inout`
			// parameter; a write on one branch only counts too -- this is "never", not "not always".
			// Opaque bodies (`@custom`, 1.x `Function`) never get here: they are not statements.
			for (int32 Index = 0; Index < CurrentFunction->Params.Num(); ++Index)
			{
				const FBoundParam& Param = CurrentFunction->Params[Index];
				if (Param.Direction != EParamDirection::Out || (ParamWrites.IsValidIndex(Index) && ParamWrites[Index]))
				{
					continue;
				}

				const FLangSpan& Span = Decl->Params.IsValidIndex(Index) ? Decl->Params[Index].NameSpan : Decl->NameSpan;
				Diagnostics.Error(
					TEXT("DSH6211"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("OutParamNeverAssigned", "'{0}' is an 'out' parameter of '{1}' but the body never assigns it, so a caller would read a value nothing produced. Assign it before the function returns, or remove the parameter."),
						FText::FromString(Param.Name),
						FText::FromString(CurrentFunction->Name)));
			}
		}

		PopScope();
		CloseDanglingRegions(BodyRegionStack);

		if (CallGraph.IsValidIndex(FunctionIndex))
		{
			CallGraph[FunctionIndex] = CurrentCallees;
		}

		CurrentFunction = nullptr;
		CurrentFunctionIndex = INDEX_NONE;
		CurrentRegion = INDEX_NONE;
		OuterRegion = INDEX_NONE;
		CurrentFile = RootModule.FilePath;
	}

	void FLangBinder::BindStmt(const FStmt& Stmt)
	{
		// Every statement remembers the innermost box it sits in; the IR builder stamps the nodes it
		// makes from that statement with it (CONTRACT §6.8).
		Bound.StatementRegions.Add(&Stmt, CurrentRegion);

		switch (Stmt.Kind)
		{
		case ENodeKind::BlockStmt:
			BindBlock(*static_cast<const FBlockStmt*>(&Stmt));
			break;

		case ENodeKind::VarDeclStmt:
			BindVarDecl(*static_cast<const FVarDeclStmt*>(&Stmt));
			break;

		case ENodeKind::ExprStmt:
		{
			const FExprStmt& ExprStmt = *static_cast<const FExprStmt*>(&Stmt);
			if (ExprStmt.Expression)
			{
				// Statement position: a custom-output node and a `void` call are legal here and
				// nowhere else.
				BindExpr(*ExprStmt.Expression, nullptr, true);
			}
			break;
		}

		case ENodeKind::IfStmt:
			BindIf(*static_cast<const FIfStmt*>(&Stmt));
			break;

		case ENodeKind::ForStmt:
			BindFor(*static_cast<const FForStmt*>(&Stmt));
			break;

		case ENodeKind::WhileStmt:
			BindWhile(*static_cast<const FWhileStmt*>(&Stmt));
			break;

		case ENodeKind::DoWhileStmt:
			BindDoWhile(*static_cast<const FDoWhileStmt*>(&Stmt));
			break;

		case ENodeKind::ReturnStmt:
			BindReturn(*static_cast<const FReturnStmt*>(&Stmt));
			break;

		case ENodeKind::BreakStmt:
		case ENodeKind::ContinueStmt:
			if (LoopDepth == 0)
			{
				Diagnostics.Error(
					TEXT("DSH4264"),
					CurrentFile,
					Stmt.Span,
					Stmt.Kind == ENodeKind::BreakStmt
						? LOCTEXT("BreakOutsideLoop", "'break' leaves a loop, and this one is not inside a 'for', 'while' or 'do'.")
						: LOCTEXT("ContinueOutsideLoop", "'continue' starts the next turn of a loop, and this one is not inside a 'for', 'while' or 'do'."));
			}
			break;

		case ENodeKind::DiscardStmt:
		{
			const EBoundFunctionKind Kind = CurrentFunction ? CurrentFunction->Kind : EBoundFunctionKind::Helper;
			const bool bAllowed =
				Kind == EBoundFunctionKind::Entry
				|| Kind == EBoundFunctionKind::Layer
				|| Kind == EBoundFunctionKind::LayerBlend;
			if (!bAllowed)
			{
				// A material function is a subgraph of whatever uses it; there is no pixel of its own
				// for it to throw away.
				Diagnostics.Error(
					TEXT("DSH4263"),
					CurrentFile,
					Stmt.Span,
					LOCTEXT("DiscardOutsideEntry", "'discard' belongs in a material entry or a layer; a material function has no pixel of its own to drop. Write the mask into 'm.OpacityMask' instead."));
			}
			break;
		}

		case ENodeKind::PragmaStmt:
		{
			const FPragmaStmt& Pragma = *static_cast<const FPragmaStmt*>(&Stmt);
			if (Pragma.PragmaKind == EPragmaKind::Region)
			{
				OpenRegion(Pragma.Text, Pragma.Span, BodyRegionStack);
			}
			else if (Pragma.PragmaKind == EPragmaKind::EndRegion)
			{
				CloseRegion(Pragma.Span, BodyRegionStack);
			}
			break;
		}

		case ENodeKind::EmptyStmt:
		default:
			break;
		}
	}

	void FLangBinder::BindBlock(const FBlockStmt& Stmt)
	{
		PushScope();
		for (const FStmtPtr& Child : Stmt.Statements)
		{
			if (Child)
			{
				BindStmt(*Child);
			}
		}
		PopScope();
	}

	void FLangBinder::BindVarDecl(const FVarDeclStmt& Stmt)
	{
		IR::FIRType Type;
		if (!ResolveTypeRef(Stmt.Type, Type))
		{
			Type = IR::FIRType::Error();
		}
		else if (Type.IsVoid())
		{
			Diagnostics.Error(
				TEXT("DSH4201"),
				CurrentFile,
				Stmt.Type.Span,
				LOCTEXT("VoidLocal", "A variable cannot be 'void'."));
			Type = IR::FIRType::Error();
		}

		const bool bConstant =
			Stmt.Storage == EStorageClass::StaticConst
			|| Stmt.Storage == EStorageClass::Const;

		for (const FDeclarator& Declarator : Stmt.Declarators)
		{
			int32 ArrayCount = 0;
			ResolveArrayCount(Declarator.ArrayDimensions, Declarator.Initializer.Get(), Declarator.Span, ArrayCount);

			// The slot is taken BEFORE the initializer is bound, so `float x = x;` reports the
			// shadowing or the self-reference rather than silently reading an outer `x`.
			const int32 Slot = DeclareLocal(Declarator.Name, Type, &Declarator, ArrayCount, Declarator.NameSpan);

			if (!Declarator.Initializer)
			{
				if (bConstant)
				{
					Diagnostics.Error(
						TEXT("DSH7214"),
						CurrentFile,
						Declarator.NameSpan,
						FText::Format(
							LOCTEXT("LocalConstantNeedsInitializer", "'{0}' is a compile-time constant and must be initialised where it is declared."),
							FText::FromString(Declarator.Name)));
				}
				continue;
			}

			if (ArrayCount > 0)
			{
				TArray<double> Values;
				BindArrayInitializer(*Declarator.Initializer, Type, ArrayCount, Values);
				if (LocalArrayValues.IsValidIndex(Slot))
				{
					LocalArrayValues[Slot] = MoveTemp(Values);
				}
				continue;
			}

			BindExpr(*Declarator.Initializer, &Type);
			Convert(
				*Declarator.Initializer,
				Type,
				EConversionSite::Assignment,
				FText::Format(
					LOCTEXT("LocalInitializer", "The initializer of '{0}'"),
					FText::FromString(Declarator.Name)));

			if (bConstant && !IsConstantExpr(*Declarator.Initializer))
			{
				Diagnostics.Error(
					TEXT("DSH7210"),
					CurrentFile,
					Declarator.Initializer->Span,
					FText::Format(
						LOCTEXT("LocalConstantNotConstant", "'{0}' is a compile-time constant, and this initializer is not one."),
						FText::FromString(Declarator.Name)));
			}
		}
	}

	void FLangBinder::BindCondition(const FExpr& Condition, const FText& What)
	{
		const IR::FIRType Type = BindExpr(Condition);
		if (Type.IsError())
		{
			return;
		}

		if (!IsConditionType(Type))
		{
			Diagnostics.Error(
				TEXT("DSH4260"),
				CurrentFile,
				Condition.Span,
				FText::Format(
					LOCTEXT("ConditionNotScalar", "{0} has to be a single true-or-false value, and this is {1}."),
					What,
					DescribeType(Type)));
			return;
		}

		Convert(Condition, IR::FIRType::Bool(1), EConversionSite::Condition, What);
	}

	void FLangBinder::BindIf(const FIfStmt& Stmt)
	{
		if (Stmt.Condition)
		{
			BindCondition(*Stmt.Condition, LOCTEXT("IfCondition", "The condition of an 'if'"));
		}
		if (Stmt.Then)
		{
			BindStmt(*Stmt.Then);
		}
		if (Stmt.Else)
		{
			BindStmt(*Stmt.Else);
		}
	}

	void FLangBinder::BindFor(const FForStmt& Stmt)
	{
		// The init variable belongs to the loop, not to the block around it.
		PushScope();

		if (Stmt.Init)
		{
			BindStmt(*Stmt.Init);
		}
		if (Stmt.Condition)
		{
			BindCondition(*Stmt.Condition, LOCTEXT("ForCondition", "The condition of a 'for'"));
		}
		if (Stmt.Step)
		{
			BindExpr(*Stmt.Step, nullptr, true);
		}

		++LoopDepth;
		if (Stmt.Body)
		{
			BindStmt(*Stmt.Body);
		}
		--LoopDepth;

		// Proved while the loop's own scope is still alive: the induction variable's slot is only
		// reachable from here.
		ProveTripCount(Stmt, Stmt.Init.Get(), Stmt.Condition.Get(), Stmt.Step.Get(), Stmt.Body.Get());

		PopScope();
	}

	void FLangBinder::BindWhile(const FWhileStmt& Stmt)
	{
		if (Stmt.Condition)
		{
			BindCondition(*Stmt.Condition, LOCTEXT("WhileCondition", "The condition of a 'while'"));
		}

		++LoopDepth;
		if (Stmt.Body)
		{
			BindStmt(*Stmt.Body);
		}
		--LoopDepth;

		ProveTripCount(Stmt, nullptr, Stmt.Condition.Get(), nullptr, Stmt.Body.Get());
	}

	void FLangBinder::BindDoWhile(const FDoWhileStmt& Stmt)
	{
		++LoopDepth;
		if (Stmt.Body)
		{
			BindStmt(*Stmt.Body);
		}
		--LoopDepth;

		if (Stmt.Condition)
		{
			BindCondition(*Stmt.Condition, LOCTEXT("DoWhileCondition", "The condition of a 'do'"));
		}

		ProveTripCount(Stmt, nullptr, Stmt.Condition.Get(), nullptr, Stmt.Body.Get());
	}

	void FLangBinder::BindReturn(const FReturnStmt& Stmt)
	{
		const IR::FIRType ReturnType = CurrentFunction ? CurrentFunction->ReturnType : IR::FIRType::Void();

		if (!Stmt.Value)
		{
			if (!ReturnType.IsVoid() && !ReturnType.IsError())
			{
				Diagnostics.Error(
					TEXT("DSH4261"),
					CurrentFile,
					Stmt.Span,
					FText::Format(
						LOCTEXT("ReturnWithoutValue", "'{0}' returns {1}, so this 'return' needs a value."),
						FText::FromString(CurrentFunction ? CurrentFunction->Name : FString()),
						DescribeType(ReturnType)));
			}
			return;
		}

		BindExpr(*Stmt.Value, &ReturnType);

		if (ReturnType.IsVoid())
		{
			Diagnostics.Error(
				TEXT("DSH4262"),
				CurrentFile,
				Stmt.Value->Span,
				FText::Format(
					LOCTEXT("ReturnWithValue", "'{0}' returns nothing, so this 'return' cannot carry a value; extra results are written to 'out' parameters."),
					FText::FromString(CurrentFunction ? CurrentFunction->Name : FString())));
			return;
		}

		Convert(
			*Stmt.Value,
			ReturnType,
			EConversionSite::Assignment,
			FText::Format(
				LOCTEXT("ReturnValue", "The value returned from '{0}'"),
				FText::FromString(CurrentFunction ? CurrentFunction->Name : FString())));
	}

	// ---------------------------------------------------------------------------------------------
	// Loop trip counts
	// ---------------------------------------------------------------------------------------------

	namespace
	{
		/** The slot an expression names, or INDEX_NONE when it names anything else. */
		int32 NamedLocalSlot(const FBoundModule& Bound, const FExpr* Expr)
		{
			if (!Expr)
			{
				return INDEX_NONE;
			}
			const FBoundExpr* Binding = Bound.Expressions.Find(Expr);
			if (!Binding || Binding->Kind != EBoundExprKind::Local)
			{
				return INDEX_NONE;
			}
			return Binding->LocalSlot;
		}
	}

	void FLangBinder::ProveTripCount(const FStmt& Loop, const FStmt* Init, const FExpr* Condition, const FExpr* Step, const FStmt* Body)
	{
		const int32 Limit = FMath::Max(Options.MaxUnrolledIterations, 0);

		// `break` and `continue` turn an unrolled loop into a chain of conditionals with a shape the
		// contract does not describe, so a loop that holds one is simply not reported.
		if (HasLoopJump(Body))
		{
			return;
		}

		// No induction variable: the only loops that can be proved are the ones whose condition is
		// already a constant. `while (true)` is not one of them.
		if (!Init || !Step)
		{
			if (!Condition)
			{
				return;
			}
			double Value[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 Components = 0;
			if (!GetConstant(*Condition, Value, Components) || Value[0] != 0.0)
			{
				return;
			}
			// A condition that is constantly false: a `while` never runs, a `do` runs once.
			Bound.LoopTripCounts.Add(&Loop, Loop.Kind == ENodeKind::DoWhileStmt ? 1 : 0);
			return;
		}

		if (!Condition)
		{
			return;
		}

		// -- the induction variable and where it starts
		const FVarDeclStmt* InitDecl = Init->As<FVarDeclStmt>();
		if (!InitDecl || InitDecl->Declarators.Num() != 1)
		{
			return;
		}

		const FDeclarator& Declarator = InitDecl->Declarators[0];
		if (!Declarator.Initializer)
		{
			return;
		}

		const int32 Slot = FindLocal(Declarator.Name);
		if (Slot == INDEX_NONE)
		{
			return;
		}

		double StartValue[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 StartComponents = 0;
		if (!GetConstant(*Declarator.Initializer, StartValue, StartComponents) || StartComponents != 1)
		{
			return;
		}

		// -- the comparison, which has to be against a constant
		const FBinaryExpr* Comparison = Condition->As<FBinaryExpr>();
		if (!Comparison)
		{
			return;
		}

		bool bVariableOnLeft = NamedLocalSlot(Bound, Comparison->Left.Get()) == Slot;
		const FExpr* Bound_ = bVariableOnLeft ? Comparison->Right.Get() : Comparison->Left.Get();
		if (!bVariableOnLeft && NamedLocalSlot(Bound, Comparison->Right.Get()) != Slot)
		{
			return;
		}

		double LimitValue[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 LimitComponents = 0;
		if (!Bound_ || !GetConstant(*Bound_, LimitValue, LimitComponents) || LimitComponents != 1)
		{
			return;
		}

		EBinaryOp Op = Comparison->Op;
		if (!bVariableOnLeft)
		{
			// `4 > i` is `i < 4`.
			switch (Op)
			{
			case EBinaryOp::Less:         Op = EBinaryOp::Greater; break;
			case EBinaryOp::LessEqual:    Op = EBinaryOp::GreaterEqual; break;
			case EBinaryOp::Greater:      Op = EBinaryOp::Less; break;
			case EBinaryOp::GreaterEqual: Op = EBinaryOp::LessEqual; break;
			default: break;
			}
		}

		switch (Op)
		{
		case EBinaryOp::Less:
		case EBinaryOp::LessEqual:
		case EBinaryOp::Greater:
		case EBinaryOp::GreaterEqual:
		case EBinaryOp::NotEqual:
			break;
		default:
			return;
		}

		// -- the step, which has to move the variable by a constant
		double Delta = 0.0;
		if (const FUnaryExpr* Unary = Step->As<FUnaryExpr>())
		{
			if (NamedLocalSlot(Bound, Unary->Operand.Get()) != Slot)
			{
				return;
			}
			switch (Unary->Op)
			{
			case EUnaryOp::PreIncrement:
			case EUnaryOp::PostIncrement: Delta = 1.0; break;
			case EUnaryOp::PreDecrement:
			case EUnaryOp::PostDecrement: Delta = -1.0; break;
			default: return;
			}
		}
		else if (const FAssignExpr* Assign = Step->As<FAssignExpr>())
		{
			if (NamedLocalSlot(Bound, Assign->Target.Get()) != Slot || !Assign->Value)
			{
				return;
			}

			double StepValue[4] = { 0.0, 0.0, 0.0, 0.0 };
			int32 StepComponents = 0;
			if (!GetConstant(*Assign->Value, StepValue, StepComponents) || StepComponents != 1)
			{
				return;
			}

			switch (Assign->Op)
			{
			case EAssignOp::AddAssign:      Delta = StepValue[0]; break;
			case EAssignOp::SubtractAssign: Delta = -StepValue[0]; break;
			default: return;
			}
		}
		else
		{
			return;
		}

		if (Delta == 0.0)
		{
			return;
		}

		// -- nothing else may touch the variable, or the step above is not the whole story
		if (Body)
		{
			// LocalWrites holds every write THIS function's body makes: an assignment, an increment,
			// and an `out` / `inout` argument, which no expression kind records. It has to be that
			// list and not FBoundModule::Expressions -- the only thing that says where a node sits is
			// its span, a span is an offset into ONE file, and an included header's offsets overlap
			// the root file's, so scanning every bound expression would count a header's assignment
			// as a write inside this body.
			for (const TPair<const FNode*, int32>& Write : LocalWrites)
			{
				if (Write.Value != Slot || Write.Key == nullptr)
				{
					continue;
				}
				const FLangSpan& WriteSpan = Write.Key->Span;
				if (WriteSpan.Offset < Body->Span.Offset || WriteSpan.End() > Body->Span.End())
				{
					// The loop's own step, or a write somewhere else in the function.
					continue;
				}
				return;
			}
		}

		// -- run it
		double Value = StartValue[0];
		const double LimitConstant = LimitValue[0];
		int32 Count = 0;
		for (;;)
		{
			bool bContinue = false;
			switch (Op)
			{
			case EBinaryOp::Less:         bContinue = Value < LimitConstant; break;
			case EBinaryOp::LessEqual:    bContinue = Value <= LimitConstant; break;
			case EBinaryOp::Greater:      bContinue = Value > LimitConstant; break;
			case EBinaryOp::GreaterEqual: bContinue = Value >= LimitConstant; break;
			case EBinaryOp::NotEqual:     bContinue = Value != LimitConstant; break;
			default: return;
			}

			if (!bContinue)
			{
				break;
			}

			++Count;
			if (Count > Limit)
			{
				// Over the budget: not reported, so the IR builder refuses it and names `@custom`
				// rather than quietly emitting thousands of nodes.
				return;
			}
			Value += Delta;
		}

		Bound.LoopTripCounts.Add(&Loop, Count);
	}
}

#undef LOCTEXT_NAMESPACE
