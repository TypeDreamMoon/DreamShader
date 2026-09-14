// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Statements, which is to say: how control flow stops existing.
//
// An `if` snapshots every frame's slots, lowers both arms against the same starting state, and
// merges the slots the two arms disagree about into StaticSwitch (a static bool condition),
// Compare (a scalar comparison, the engine If) or Select. A loop is unrolled with the trip count
// the binder proved; one it could not prove has no graph form and says so. A `return` does not
// jump: it files a pending exit -- the slots as they stood, under the conjunction of every `if` arm
// that reached it -- and the end of the body folds the exits into the final state in reverse order,
// which is exactly what `if (c) return a; ... return b;` means once the jumps are gone.
//
// Diagnostics owned by this file: DSH4360, DSH4362, DSH4363, DSH4372, DSH4375.

#include "IRBuilderInternal.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.IRBuilder"

namespace UE::DreamShader::IR::Private
{
	// -------------------------------------------------------------------------------- dispatch

	void FIRBuilder::LowerStatement(const FStmt& Stmt)
	{
		if (Frames.IsEmpty() || Frame().bReturned || bBreak || bContinue)
		{
			return;
		}

		const int32 SavedRegion = CurrentRegion;
		if (const int32* Region = BoundModule.StatementRegions.Find(&Stmt))
		{
			// Every node this statement and its children make lands in this region box.
			CurrentRegion = *Region;
		}

		switch (Stmt.Kind)
		{
		case ENodeKind::VarDeclStmt:
			if (const FVarDeclStmt* Decl = Stmt.As<FVarDeclStmt>())
			{
				LowerVarDecl(*Decl);
			}
			break;

		case ENodeKind::ExprStmt:
			if (const FExprStmt* ExprStmt = Stmt.As<FExprStmt>())
			{
				if (ExprStmt->Expression)
				{
					LowerExpr(*ExprStmt->Expression);
				}
			}
			break;

		case ENodeKind::BlockStmt:
			if (const FBlockStmt* Block = Stmt.As<FBlockStmt>())
			{
				LowerBlock(*Block);
			}
			break;

		case ENodeKind::IfStmt:
			if (const FIfStmt* If = Stmt.As<FIfStmt>())
			{
				LowerIf(*If);
			}
			break;

		case ENodeKind::ForStmt:
			if (const FForStmt* For = Stmt.As<FForStmt>())
			{
				LowerLoop(Stmt, For->Init.Get(), For->Body.Get(), For->Step.Get());
			}
			break;

		case ENodeKind::WhileStmt:
			if (const FWhileStmt* While = Stmt.As<FWhileStmt>())
			{
				LowerLoop(Stmt, nullptr, While->Body.Get(), nullptr);
			}
			break;

		case ENodeKind::DoWhileStmt:
			if (const FDoWhileStmt* DoWhile = Stmt.As<FDoWhileStmt>())
			{
				LowerLoop(Stmt, nullptr, DoWhile->Body.Get(), nullptr);
			}
			break;

		case ENodeKind::ReturnStmt:
			if (const FReturnStmt* Return = Stmt.As<FReturnStmt>())
			{
				LowerReturn(*Return);
			}
			break;

		case ENodeKind::BreakStmt:
		case ENodeKind::ContinueStmt:
		{
			const bool bIsBreak = Stmt.Kind == ENodeKind::BreakStmt;
			if (LoopDepth == 0)
			{
				break;
			}
			if (BranchDepth > 0)
			{
				Diagnostics.Error(TEXT("DSH4363"), Stmt.Span, FText::Format(
					LOCTEXT("IRBuilderJumpInBranch", "'{0}' inside an 'if' cannot be unrolled, because both arms of the 'if' become nodes and only one of them may leave the loop; move it to the top of the loop body, or write the loop in a '/// @custom' function."),
					FText::FromString(bIsBreak ? TEXT("break") : TEXT("continue"))));
				break;
			}
			bBreak = bIsBreak;
			bContinue = !bIsBreak;
			break;
		}

		case ENodeKind::DiscardStmt:
			Diagnostics.Error(TEXT("DSH4362"), Stmt.Span, LOCTEXT("IRBuilderDiscard",
				"'discard' has no material-graph form; set the material's OpacityMask to zero instead, or move the branch into a '/// @custom' function."));
			break;

		case ENodeKind::PragmaStmt:
			// `#pragma region` / `#pragma endregion` make no node: the binder already put every
			// statement between them into FBoundModule::StatementRegions.
			break;

		case ENodeKind::EmptyStmt:
		default:
			break;
		}

		CurrentRegion = SavedRegion;
	}

	void FIRBuilder::LowerBlock(const FBlockStmt& Block)
	{
		for (const FStmtPtr& Statement : Block.Statements)
		{
			if (Frame().bReturned || bBreak || bContinue)
			{
				break;
			}
			if (Statement)
			{
				LowerStatement(*Statement);
			}
		}
	}

	// ---------------------------------------------------------------------------- declarations

	void FIRBuilder::LowerVarDecl(const FVarDeclStmt& Stmt)
	{
		const FBoundFunction* Function = Frame().Function;
		if (!Function)
		{
			return;
		}

		for (const FDeclarator& Declarator : Stmt.Declarators)
		{
			int32 Slot = INDEX_NONE;
			for (int32 Index = 0; Index < Function->Locals.Num(); ++Index)
			{
				if (Function->Locals[Index].Decl == &Declarator)
				{
					Slot = Index;
					break;
				}
			}
			if (Slot == INDEX_NONE || !Frame().Locals.IsValidIndex(Slot))
			{
				continue;
			}

			if (!Declarator.Initializer)
			{
				// An uninitialised local keeps what PushFrame gave it -- never assigned, so a read of it
				// is DSH4376 and a merge with an arm that did set it is DSH4372 -- or, on a later trip of an
				// unrolled loop, whatever the last trip left in it.
				continue;
			}

			const int32 FirstNode = Graph ? Graph->Nodes.Num() : 0;

			// `float W[4] = { … }`: the binder types the initializer list as the ELEMENT type, since
			// there is no array FIRType, and gives one FBoundArgument per element with TargetIndex =
			// the element index (S-report #8). Lowering it as a constructor would Append the elements
			// into one vector instead of filling four slots.
			if (Function->Locals[Slot].ArrayCount > 0)
			{
				const FExpr* Initializer = Unparen(Declarator.Initializer.Get());
				if (const FInitializerListExpr* List = Initializer ? Initializer->As<FInitializerListExpr>() : nullptr)
				{
					const FBoundExpr* BoundList = Bound(*Initializer);
					FLoweredValue Elements = FLoweredValue::MakeAggregate(Function->Locals[Slot].ArrayCount);
					for (int32 Element = 0; Element < List->Elements.Num(); ++Element)
					{
						int32 Position = Element;
						if (BoundList && BoundList->Args.IsValidIndex(Element) && BoundList->Args[Element].TargetIndex != INDEX_NONE)
						{
							Position = BoundList->Args[Element].TargetIndex;
						}
						if (List->Elements[Element] && Elements.Fields.IsValidIndex(Position))
						{
							Elements.Fields[Position] = LowerExpr(*List->Elements[Element]);
						}
					}
					Frame().Locals[Slot] = MoveTemp(Elements);
					SetDebugName(FirstNode, Declarator.Name);
					continue;
				}
			}

			const FLoweredValue Value = LowerExpr(*Declarator.Initializer);

			// A declared type is authoritative about width: `float3 c = 1;` is a float3.
			const FIRType& Declared = Function->Locals[Slot].Type;
			if (Value.IsValue() && Declared.GraphComponentCount() > 0)
			{
				Frame().Locals[Slot] = FLoweredValue::Of(CoerceToWidth(Value.Value, Declared.GraphComponentCount(), Declarator.Span));
			}
			else
			{
				// Anything else is stored as it came, an Empty one included. An initializer that produced
				// nothing has already said why, and storing it is what makes the local count as assigned,
				// so a later read of it is not reported a second time as DSH4376.
				Frame().Locals[Slot] = Value;
			}

			SetDebugName(FirstNode, Declarator.Name);
		}
	}

	// ------------------------------------------------------------------------------------- if

	FIRValue FIRBuilder::CurrentPredicate(bool& bOutStatic, const FLangSpan& Span)
	{
		bOutStatic = true;
		FIRValue Predicate = FIRValue::None();

		for (const FConditionEntry& Entry : ConditionStack)
		{
			if (!Entry.Value.IsValid())
			{
				continue;
			}
			bOutStatic = bOutStatic && Entry.bStatic;

			FIRValue Term = Entry.Value;
			if (Entry.bNegated)
			{
				Term = MakeCoreOp(EIROp::LogicalNot, { Term }, TypeOfValue(Term).IsBool() ? FIRType::Bool(1) : FIRType::Float(1), Span);
			}
			Predicate = Predicate.IsValid()
				? MakeCoreOp(EIROp::LogicalAnd, { Predicate, Term }, FIRType::Bool(1), Span)
				: Term;
		}

		bOutStatic = bOutStatic && Predicate.IsValid() && IsStaticCondition(Predicate);
		return Predicate;
	}

	void FIRBuilder::LowerIf(const FIfStmt& Stmt)
	{
		if (!Stmt.Condition)
		{
			return;
		}

		const FIRValue Condition = LowerValue(*Stmt.Condition);
		const bool bStatic = IsStaticCondition(Condition);

		const FEnvSnapshot Before = Snapshot();
		FFrame& Current = Frame();

		++BranchDepth;

		// ----- the `then` arm
		ConditionStack.Push({ Condition, bStatic, /* bNegated */ false });
		if (Stmt.Then)
		{
			LowerStatement(*Stmt.Then);
		}
		ConditionStack.Pop();

		const bool bThenReturned = Current.bReturned;
		const FEnvSnapshot ThenState = Snapshot();
		const FLoweredValue ThenReturn = Current.ReturnValue;
		Current.bReturned = false;
		Current.ReturnValue = FLoweredValue();

		// ----- the `else` arm, from the same starting state
		Restore(Before);
		ConditionStack.Push({ Condition, bStatic, /* bNegated */ true });
		if (Stmt.Else)
		{
			LowerStatement(*Stmt.Else);
		}
		ConditionStack.Pop();

		const bool bElseReturned = Current.bReturned;
		const FEnvSnapshot ElseState = Snapshot();
		const FLoweredValue ElseReturn = Current.ReturnValue;
		Current.bReturned = false;
		Current.ReturnValue = FLoweredValue();

		--BranchDepth;

		const auto FileExit = [this, &Stmt, &Current](const FEnvSnapshot& State, const FLoweredValue& Value, FIRValue Cond, bool bCondStatic, bool bNegated)
		{
			ConditionStack.Push({ Cond, bCondStatic, bNegated });
			bool bPredicateStatic = false;
			const FIRValue Predicate = CurrentPredicate(bPredicateStatic, Stmt.Span);
			ConditionStack.Pop();

			FPendingExit Exit;
			Exit.Condition = Predicate;
			Exit.bStaticCondition = bPredicateStatic;
			Exit.Span = Stmt.Span;
			Exit.ReturnValue = Value;
			const int32 FrameIndex = Frames.Num() - 1;
			if (State.Locals.IsValidIndex(FrameIndex))
			{
				Exit.Locals = State.Locals[FrameIndex];
				Exit.Params = State.Params[FrameIndex];
			}
			Current.PendingExits.Add(MoveTemp(Exit));
		};

		if (bThenReturned)
		{
			FileExit(ThenState, ThenReturn, Condition, bStatic, /* bNegated */ false);
		}
		if (bElseReturned)
		{
			FileExit(ElseState, ElseReturn, Condition, bStatic, /* bNegated */ true);
		}

		if (bThenReturned && bElseReturned)
		{
			// Nothing after the `if` can be reached. The live slots are whatever the `else` arm left
			// behind; ResolveExits is told to ignore them.
			Restore(ElseState);
			Current.bReturned = true;
			Current.bStateDead = true;
			return;
		}
		if (bThenReturned)
		{
			Restore(ElseState);
			return;
		}
		if (bElseReturned)
		{
			Restore(ThenState);
			return;
		}

		// A local declared inside either arm is out of scope once the `if` ends (CONTRACT 6.3): no later
		// line can name it, so there is nothing to merge -- and merging it anyway reports DSH4372 against
		// a variable that only ever existed in one arm by definition. Cleared in both states, so the merge
		// sees two Empty slots and leaves the slot alone. A declaration's span and the `if`'s are in the
		// same file: both belong to the body of the function this frame is lowering.
		const auto DropArmScopedLocals = [this, &Stmt](FEnvSnapshot& State)
		{
			const int32 FrameIndex = Frames.Num() - 1;
			const FBoundFunction* Function = Frame().Function;
			if (!Function || !State.Locals.IsValidIndex(FrameIndex))
			{
				return;
			}
			TArray<FLoweredValue>& Locals = State.Locals[FrameIndex];
			for (int32 Slot = 0; Slot < Locals.Num() && Slot < Function->Locals.Num(); ++Slot)
			{
				const FLangSpan& Declared = Function->Locals[Slot].Span;
				if (Declared.Offset >= Stmt.Span.Offset && Declared.End() <= Stmt.Span.End())
				{
					// Back to never assigned, in its own shape: the next trip of an unrolled loop meets the
					// declaration again, and a struct local must still have its fields to write into.
					Locals[Slot] = MakeUnassigned(Function->Locals[Slot].Type, Function->Locals[Slot].ArrayCount);
				}
			}
		};

		FEnvSnapshot ThenMerge = ThenState;
		FEnvSnapshot ElseMerge = ElseState;
		DropArmScopedLocals(ThenMerge);
		DropArmScopedLocals(ElseMerge);
		MergeStates(ThenMerge, ElseMerge, Condition, bStatic, Stmt.Span);
	}

	// ----------------------------------------------------------------------------------- loops

	void FIRBuilder::LowerLoop(const FStmt& Stmt, const FStmt* Init, const FStmt* Body, const FExpr* Step)
	{
		const int32* TripCount = BoundModule.LoopTripCounts.Find(&Stmt);
		if (!TripCount || *TripCount < 0 || *TripCount > Options.MaxUnrolledIterations)
		{
			Diagnostics.Error(TEXT("DSH4360"), Stmt.Span, FText::Format(
				LOCTEXT("IRBuilderLoopNotUnrollable", "This loop runs a number of times the compiler cannot fix at {0} or fewer, and a material graph has no loops; give it a constant trip count, or move it into a '/// @custom' function."),
				FText::AsNumber(Options.MaxUnrolledIterations)));
			return;
		}

		if (Init)
		{
			LowerStatement(*Init);
		}

		++LoopDepth;
		const bool bSavedBreak = bBreak;
		const bool bSavedContinue = bContinue;
		bBreak = false;
		bContinue = false;

		for (int32 Iteration = 0; Iteration < *TripCount; ++Iteration)
		{
			if (Body)
			{
				// A fresh iteration: the slots carry over from the last one (that is what a loop
				// does), but `continue` only skips the rest of THIS body.
				bContinue = false;
				LowerStatement(*Body);
			}
			if (bBreak || Frame().bReturned)
			{
				break;
			}
			bContinue = false;
			// The condition is not lowered at all: the binder proved the trip count, and evaluating
			// the condition would put a comparison in the graph that nothing reads.
			if (Step)
			{
				LowerExpr(*Step);
			}
		}

		bBreak = bSavedBreak;
		bContinue = bSavedContinue;
		--LoopDepth;
	}

	// --------------------------------------------------------------------------------- returns

	void FIRBuilder::LowerReturn(const FReturnStmt& Stmt)
	{
		FFrame& Current = Frame();
		Current.ReturnValue = Stmt.Value ? LowerExpr(*Stmt.Value) : FLoweredValue();
		Current.bReturned = true;
		// The enclosing LowerIf turns this into a pending exit; at the top level of a body it simply
		// stops the walk, and the live slots are the state the function ends in.
	}

	// ------------------------------------------------------------------------------- snapshots

	FEnvSnapshot FIRBuilder::Snapshot() const
	{
		FEnvSnapshot State;
		State.Locals.Reserve(Frames.Num());
		State.Params.Reserve(Frames.Num());
		for (const TUniquePtr<FFrame>& Each : Frames)
		{
			State.Locals.Add(Each->Locals);
			State.Params.Add(Each->Params);
		}
		return State;
	}

	void FIRBuilder::Restore(const FEnvSnapshot& State)
	{
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			if (State.Locals.IsValidIndex(Index))
			{
				Frames[Index]->Locals = State.Locals[Index];
				Frames[Index]->Params = State.Params[Index];
			}
		}
	}

	void FIRBuilder::MergeStates(
		const FEnvSnapshot& TrueState,
		const FEnvSnapshot& FalseState,
		FIRValue Condition,
		bool bStaticCondition,
		const FLangSpan& Span)
	{
		for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
		{
			if (!TrueState.Locals.IsValidIndex(FrameIndex) || !FalseState.Locals.IsValidIndex(FrameIndex))
			{
				continue;
			}

			FFrame& Target = *Frames[FrameIndex];
			const TArray<FLoweredValue>& TrueLocals = TrueState.Locals[FrameIndex];
			const TArray<FLoweredValue>& FalseLocals = FalseState.Locals[FrameIndex];
			for (int32 Slot = 0; Slot < Target.Locals.Num(); ++Slot)
			{
				if (!TrueLocals.IsValidIndex(Slot) || !FalseLocals.IsValidIndex(Slot))
				{
					continue;
				}
				const FString& Name = Target.Function && Target.Function->Locals.IsValidIndex(Slot)
					? Target.Function->Locals[Slot].Name
					: FString();
				Target.Locals[Slot] = MergeValues(TrueLocals[Slot], FalseLocals[Slot], Condition, bStaticCondition, Span, Name);
			}

			const TArray<FLoweredValue>& TrueParams = TrueState.Params[FrameIndex];
			const TArray<FLoweredValue>& FalseParams = FalseState.Params[FrameIndex];
			for (int32 Slot = 0; Slot < Target.Params.Num(); ++Slot)
			{
				if (!TrueParams.IsValidIndex(Slot) || !FalseParams.IsValidIndex(Slot))
				{
					continue;
				}
				const FString& Name = Target.Function && Target.Function->Params.IsValidIndex(Slot)
					? Target.Function->Params[Slot].Name
					: FString();
				Target.Params[Slot] = MergeValues(TrueParams[Slot], FalseParams[Slot], Condition, bStaticCondition, Span, Name);
			}
		}
	}

	FLoweredValue FIRBuilder::MergeValues(
		const FLoweredValue& TrueValue,
		const FLoweredValue& FalseValue,
		FIRValue Condition,
		bool bStaticCondition,
		const FLangSpan& Span,
		const FString& What)
	{
		if (TrueValue.IsEmpty() && FalseValue.IsEmpty())
		{
			// Neither arm holds a value. The slot stays never assigned only when neither arm assigned it:
			// an arm whose assignment already reported counts as having assigned it, so a later read is
			// not a second message (DSH4376).
			return TrueValue.IsNeverAssigned() ? FalseValue : TrueValue;
		}
		if (TrueValue == FalseValue)
		{
			return TrueValue;
		}

		if (TrueValue.IsAggregate() && FalseValue.IsAggregate())
		{
			FLoweredValue Result = FLoweredValue::MakeAggregate(FMath::Max(TrueValue.Fields.Num(), FalseValue.Fields.Num()));
			for (int32 Index = 0; Index < Result.Fields.Num(); ++Index)
			{
				const FLoweredValue& Left = TrueValue.Fields.IsValidIndex(Index) ? TrueValue.Fields[Index] : FLoweredValue();
				const FLoweredValue& Right = FalseValue.Fields.IsValidIndex(Index) ? FalseValue.Fields[Index] : FLoweredValue();
				Result.Fields[Index] = MergeValues(Left, Right, Condition, bStaticCondition, Span, What);
			}
			return Result;
		}

		if (TrueValue.IsMaterial() && FalseValue.IsMaterial())
		{
			// `?:` names its merge "?:", which is not a variable anybody can look for, so it and a function
			// exit (no name at all) get the unnamed message.
			const FString MergeName = (What.IsEmpty() || What.Equals(TEXT("?:"), ESearchCase::CaseSensitive)) ? FString() : What;

			if (TrueValue.Material.Source != FalseValue.Material.Source)
			{
				// Two different whole materials: one arm replaced the material (`m = UE.Blend...(...)`,
				// a function call writing an `inout material` back) and the other did not, or each arm
				// replaced it with a different one. The attribute-by-attribute merge below would keep
				// the TRUE arm's set under both arms in silence: an attribute neither arm wrote has no
				// value the IR can name on a side without a source, and the conditionals this builder
				// makes are typed as one value, not as a set.
				ReportWholeMaterialMerge(MergeName, Span);
				return TrueValue;
			}

			FLoweredValue Result = FLoweredValue::MakeMaterial(TrueValue.Material.Source);
			Result.Material.BreakNode = TrueValue.Material.BreakNode != INDEX_NONE
				? TrueValue.Material.BreakNode
				: FalseValue.Material.BreakNode;

			TArray<FString> Attributes = TrueValue.Material.Order;
			for (const FString& Name : FalseValue.Material.Order)
			{
				Attributes.AddUnique(Name);
			}

			// An attribute that holds a material -- the `MaterialAttributes` entry, a whole set -- is a whole
			// material one attribute down, and two different ones meet the same refusal: a conditional over
			// them would be typed by width and come out float1.
			const auto IsWholeMaterialChoice = [this](FIRValue Left, FIRValue Right)
			{
				return Left != Right && (TypeOfValue(Left).IsMaterial() || TypeOfValue(Right).IsMaterial());
			};

			for (const FString& Name : Attributes)
			{
				const FIRValue* Left = TrueValue.Material.Fields.Find(Name);
				const FIRValue* Right = FalseValue.Material.Fields.Find(Name);
				const FString FieldName = MergeName.IsEmpty() ? FString() : MergeName + TEXT(".") + Name;
				FIRValue Merged = FIRValue::None();
				if (Left && Right)
				{
					if (IsWholeMaterialChoice(*Left, *Right))
					{
						ReportWholeMaterialMerge(FieldName, Span);
						Merged = *Left;
					}
					else
					{
						Merged = MakeConditional(Condition, bStaticCondition, *Left, *Right, Span);
					}
				}
				else
				{
					// One arm set the attribute and the other did not. When the material came in
					// through a pin the other arm's value is simply what was already on it; when it
					// did not, there is nothing to select against.
					FIRValue Fallback = FIRValue::None();
					FMaterialValue Reader = TrueValue.Material;
					const int32 AttributeIndex = Catalog ? Catalog->FindMaterialAttribute(Name) : INDEX_NONE;
					if (Reader.HasSource() && AttributeIndex != INDEX_NONE && ReadMaterialField(Reader, AttributeIndex, Span, Fallback))
					{
						Result.Material.BreakNode = Reader.BreakNode;
						const FIRValue TrueSide = Left ? *Left : Fallback;
						const FIRValue FalseSide = Right ? *Right : Fallback;
						if (IsWholeMaterialChoice(TrueSide, FalseSide))
						{
							ReportWholeMaterialMerge(FieldName, Span);
							Merged = TrueSide;
						}
						else
						{
							Merged = MakeConditional(Condition, bStaticCondition, TrueSide, FalseSide, Span);
						}
					}
					else
					{
						// Unless the compiler can tell the arm without it never runs (`if (i > 0)` on the
						// first trip of an unrolled loop): then every path the shader takes has it.
						bool bConditionHolds = false;
						const bool bDecided = TryDecideCondition(Condition, bConditionHolds);
						const bool bMissingArmRuns = Left ? !bConditionHolds : bConditionHolds;
						if (!bDecided || bMissingArmRuns)
						{
							Diagnostics.Error(TEXT("DSH4372"), Span, FText::Format(
								LOCTEXT("IRBuilderOneArmedMaterialWrite", "'{0}' is set in only one arm of this 'if' and has no value before it; set it in both arms, or before the 'if'."),
								FText::FromString(Name)));
						}
						Merged = Left ? *Left : (Right ? *Right : FIRValue::None());
					}
				}

				if (Merged.IsValid())
				{
					Result.Material.Order.Add(Name);
					Result.Material.Fields.Add(Name, Merged);
				}
			}
			return Result;
		}

		if (TrueValue.IsValue() && FalseValue.IsValue())
		{
			const int32 Width = FMath::Max(WidthOf(TrueValue.Value), WidthOf(FalseValue.Value));
			return FLoweredValue::Of(MakeConditional(
				Condition,
				bStaticCondition,
				CoerceToWidth(TrueValue.Value, Width, Span),
				CoerceToWidth(FalseValue.Value, Width, Span),
				Span));
		}

		// One arm produced a value and the other produced nothing at all.
		if (TrueValue.IsEmpty() != FalseValue.IsEmpty())
		{
			// When the compiler can tell which arm runs and the one without a value is not it -- `if (i > 0)`
			// on the first trip of an unrolled `for (int i = 0; ...)` -- nothing is missing on any path the
			// shader takes, and saying otherwise would be a false DSH4372.
			bool bConditionHolds = false;
			const bool bDecided = TryDecideCondition(Condition, bConditionHolds);
			const bool bEmptyArmRuns = TrueValue.IsEmpty() ? bConditionHolds : !bConditionHolds;
			if (!What.IsEmpty() && (!bDecided || bEmptyArmRuns))
			{
				Diagnostics.Error(TEXT("DSH4372"), Span, FText::Format(
					LOCTEXT("IRBuilderOneArmedWrite", "'{0}' is assigned in only one arm of this 'if' and has no value before it; assign it in both arms, or give it a value before the 'if'."),
					FText::FromString(What)));
			}
			return TrueValue.IsEmpty() ? FalseValue : TrueValue;
		}

		return TrueValue;
	}

	void FIRBuilder::ReportWholeMaterialMerge(const FString& Name, const FLangSpan& Span)
	{
		if (Name.IsEmpty())
		{
			Diagnostics.Error(TEXT("DSH4375"), Span, LOCTEXT("IRBuilderWholeMaterialMerge",
				"The two sides of this branch end with a different whole material, and DreamShader chooses between attribute values, not between whole materials; assign the attributes one at a time on both sides, or mix the two materials with UE.BlendMaterialAttributes."));
			return;
		}
		Diagnostics.Error(TEXT("DSH4375"), Span, FText::Format(
			LOCTEXT("IRBuilderWholeMaterialMergeNamed", "'{0}' holds a different whole material in each arm of this 'if', and DreamShader chooses between attribute values, not between whole materials; assign its attributes one at a time in both arms, or mix the two materials with UE.BlendMaterialAttributes."),
			FText::FromString(Name)));
	}

	void FIRBuilder::ResolveExits()
	{
		FFrame& Current = Frame();
		if (Current.PendingExits.IsEmpty())
		{
			return;
		}

		int32 First = 0;
		if (Current.bStateDead)
		{
			// Nothing reaches the end of the body: the last exit IS the final state.
			const FPendingExit& Last = Current.PendingExits.Last();
			Current.Locals = Last.Locals;
			Current.Params = Last.Params;
			Current.ReturnValue = Last.ReturnValue;
			Current.PendingExits.Pop();
			Current.bStateDead = false;
		}

		for (int32 Index = Current.PendingExits.Num() - 1; Index >= First; --Index)
		{
			const FPendingExit& Exit = Current.PendingExits[Index];

			Current.ReturnValue = MergeValues(Exit.ReturnValue, Current.ReturnValue, Exit.Condition, Exit.bStaticCondition, Exit.Span, FString());

			for (int32 Slot = 0; Slot < Current.Locals.Num(); ++Slot)
			{
				if (Exit.Locals.IsValidIndex(Slot))
				{
					Current.Locals[Slot] = MergeValues(Exit.Locals[Slot], Current.Locals[Slot], Exit.Condition, Exit.bStaticCondition, Exit.Span, FString());
				}
			}
			for (int32 Slot = 0; Slot < Current.Params.Num(); ++Slot)
			{
				if (Exit.Params.IsValidIndex(Slot))
				{
					Current.Params[Slot] = MergeValues(Exit.Params[Slot], Current.Params[Slot], Exit.Condition, Exit.bStaticCondition, Exit.Span, FString());
				}
			}
		}

		Current.PendingExits.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
