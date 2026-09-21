// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// BuildDreamShaderAstFromIR, one graph: statements, regions, their order, the body.
//
// Every named value is one statement; a material built up attribute by attribute and a call with `out` parameters
// are one statement of several lines. The body ends in the roots: the writes to the material, the assignments to
// `out` parameters, the reflected custom-output calls, `return`.
//
// Order. A statement depends on the statements that declared the names it uses, and the order statements were made
// in -- operands first -- always satisfies that. It does not keep a `#pragma region` in one piece, and a region
// split in two comes back as two boxes, so the order is rebuilt per region: inside a region its statements and its
// child regions (each as one block) are sorted by dependency, earliest-made first. A graph whose regions cannot each
// be one block -- region A needs a value of B that needs a value of A -- falls back to the order of making, with the
// regions opened and closed as often as that takes.

#include "IRToAstInternal.h"

#include "Lang/LangPrinter.h"

#define LOCTEXT_NAMESPACE "DreamShaderIRToAstStatements"

namespace UE::DreamShader::Lang::DecompileAst
{
	/** Roots sort after everything they could depend on, in the order they were given; `return` after all of them. */
	static constexpr int64 GDecompileRootSequenceBase = int64(1) << 40;
	static constexpr int64 GDecompileReturnSequence = int64(1) << 50;

	static constexpr int32 GDecompileMaxInlineLength = 120;
	static constexpr int32 GDecompileMaxInlineDepth = 4;

	int32 FGraphWriter::AddStatement(FPlannedStatement&& Statement)
	{
		if (Statement.Sequence < 0)
		{
			Statement.Sequence = NextSequence++;
		}
		return Statements.Add(MoveTemp(Statement));
	}

	bool FGraphWriter::ShouldNameForLength(const FExpr& Expr) const
	{
		return MeasureBinaryDepth(Expr) > GDecompileMaxInlineDepth
			|| PrintDreamShaderLangExpr(Expr).Len() > GDecompileMaxInlineLength;
	}

	// -------------------------------------------------------------------------------- declarations

	void FGraphWriter::DeclareSlot(const int32 NodeIndex, const int32 SlotIndex, FExprPtr Initializer, const TArray<int32>& Dependencies)
	{
		FNodePlan& Plan = Plans[NodeIndex];
		FValuePlan& Slot = Plan.Slots[SlotIndex];
		const FIRNode& Node = Graph.Nodes[NodeIndex];

		int32 LowestUsedSlot = INDEX_NONE;
		for (int32 Index = 0; Index < Plan.Slots.Num(); ++Index)
		{
			if (Plan.Slots[Index].DirectReads > 0)
			{
				LowestUsedSlot = Index;
				break;
			}
		}

		const bool bHinted = Plan.bOwnsHintName && SlotIndex == LowestUsedSlot;
		const FString Wanted = bHinted ? MakeSourceIdentifier(Node.DebugName, TEXT("Value")) : MakeAutoName(NodeIndex, SlotIndex);
		Slot.Name = FunctionNames.Claim(Wanted);
		if (bHinted && !Slot.Name.Equals(Node.DebugName, ESearchCase::CaseSensitive))
		{
			Owner.Info(TEXT("DSH9075"), FText::Format(
				LOCTEXT("LocalRenamed", "The variable '{0}' of '{1}' is written as '{2}': the name is taken or is not one the language allows."),
				FText::FromString(Node.DebugName),
				FText::FromString(Product.Name),
				FText::FromString(Slot.Name)));
		}

		const FIRType Type = Node.Outputs.IsValidIndex(SlotIndex) ? Node.Outputs[SlotIndex] : FIRType::Float(1);

		FPlannedStatement Statement;
		Statement.Lines.Add(MakeVarDeclStmt(MakeValueTypeRef(Type, /* bKeepBool */ true), Slot.Name, MoveTemp(Initializer)));
		Statement.Region = RegionOfNode(NodeIndex);
		Statement.Dependencies = Dependencies;

		Slot.Statement = AddStatement(MoveTemp(Statement));
		Slot.bNamed = true;
	}

	void FGraphWriter::DeclareMaterialGroup(const int32 NodeIndex)
	{
		// IRBuilderMaterial.cpp MaterialiseMaterial: a `material` local nobody initialised is a MakeMaterialAttributes of
		// what was written to it; one initialised from a material is a SetMaterialAttributes on top of that material.
		FNodePlan& Plan = Plans[NodeIndex];
		const FIRNode& Node = Graph.Nodes[NodeIndex];

		CurrentDependencies.Reset();

		const FString Wanted = OwnsHintName(NodeIndex) ? MakeSourceIdentifier(Node.DebugName, TEXT("Attributes")) : FString(TEXT("Attributes"));
		const FString Name = FunctionNames.Claim(Wanted);

		FPlannedStatement Statement;
		Statement.Region = RegionOfNode(NodeIndex);

		FExprPtr Source;
		if (Node.Op == EIROp::SetMaterialAttributes)
		{
			const FIRInput* SourceInput = Node.FindInput(TEXT("MaterialAttributes"));
			if (SourceInput && SourceInput->Value.IsValid())
			{
				Source = TakeValue(SourceInput->Value);
			}
		}
		Statement.Lines.Add(MakeVarDeclStmt(MakeTypeRef(TEXT("material")), Name, MoveTemp(Source)));

		for (const FIRInput& Input : Node.Inputs)
		{
			if (!Input.Value.IsValid() || (Node.Op == EIROp::SetMaterialAttributes && Input.Pin.Equals(TEXT("MaterialAttributes"), ESearchCase::CaseSensitive)))
			{
				continue;
			}
			if (!IsIdentifierText(Input.Pin))
			{
				Owner.Error(TEXT("DSH9078"), FText::Format(
					LOCTEXT("GroupAttributeUnnamed", "{0} writes an attribute '{1}' that is not a name the language has; the write is dropped."),
					DescribeNode(NodeIndex),
					FText::FromString(Input.Pin)));
				continue;
			}
			FExprPtr Value = TakeValue(Input.Value);
			Statement.Lines.Add(MakeAssignStmt(MakeMemberExpr(MakeIdentifierExpr(Name), Input.Pin), MoveTemp(Value)));
		}

		Statement.Dependencies = CurrentDependencies;
		const int32 StatementIndex = AddStatement(MoveTemp(Statement));

		Plan.PlacedName = Name;
		for (FValuePlan& Slot : Plan.Slots)
		{
			Slot.bNamed = true;
			Slot.Name = Name;
			Slot.Statement = StatementIndex;
		}
	}

	void FGraphWriter::DeclareCallStatement(const int32 NodeIndex)
	{
		// An `out` argument has to be a variable of exactly the parameter's type (DSH4239, DSH4218), so every output
		// gets a local, read or not; an `inout` one starts out holding what the call's input pin is wired to.
		FNodePlan& Plan = Plans[NodeIndex];
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		const FCallable* Callable = Owner.FindCallable(Node);
		if (!Callable)
		{
			return;
		}

		CurrentDependencies.Reset();

		FPlannedStatement Statement;
		Statement.Region = RegionOfNode(NodeIndex);

		const bool bHinted = OwnsHintName(NodeIndex);
		bool bHintUsed = false;

		const int32 ReturnSlot = Callable->bHasReturn ? Node.FindOutput(Callable->ReturnPinName) : INDEX_NONE;
		const bool bReturnRead = Plan.Slots.IsValidIndex(ReturnSlot) && Plan.Slots[ReturnSlot].DirectReads > 0;

		// The statement's own name goes to the value the call returns, as it does where the builder names the node.
		FString ReturnLocal;
		if (bReturnRead)
		{
			ReturnLocal = FunctionNames.Claim(bHinted
				? MakeSourceIdentifier(Node.DebugName, TEXT("Result"))
				: MakeSourceIdentifier(Callable->Identifier + TEXT("_Result"), TEXT("Result")));
			bHintUsed = bHinted;
		}

		TMap<FString, FString> Locals;
		for (const FCallableParam& Param : Callable->Params)
		{
			if (Param.Direction == EParamDirection::In)
			{
				continue;
			}

			FString Wanted = MakeSourceIdentifier(Param.Identifier, TEXT("Out"));
			if (bHinted && !bHintUsed)
			{
				Wanted = MakeSourceIdentifier(Node.DebugName, TEXT("Out"));
				bHintUsed = true;
			}
			const FString Local = FunctionNames.Claim(Wanted);
			Locals.Add(Param.Identifier, Local);

			FExprPtr Initial;
			if (Param.Direction == EParamDirection::InOut)
			{
				const FIRInput* Input = Node.FindInput(Param.PinName);
				if (Input && Input->Value.IsValid())
				{
					Initial = TakeValue(Input->Value);
				}
				else
				{
					Initial = TakeZero(Param.Type);
					Owner.Warning(TEXT("DSH9084"), FText::Format(
						LOCTEXT("InOutInputUnconnected", "{0} leaves the 'inout' input '{1}' of '{2}' unconnected; the variable passed for it starts at zero."),
						DescribeNode(NodeIndex),
						FText::FromString(Param.PinName),
						FText::FromString(Callable->Identifier)));
				}
			}

			Statement.Lines.Add(MakeVarDeclStmt(MakeSignatureTypeRef(Param.Type, Param.TypeSpelling), Local, MoveTemp(Initial)));
		}

		FExprPtr Call = BuildUserCall(NodeIndex, Node, *Callable, &Locals);
		if (bReturnRead)
		{
			Statement.Lines.Add(MakeVarDeclStmt(MakeSignatureTypeRef(Callable->ReturnType, Callable->ReturnTypeSpelling), ReturnLocal, MoveTemp(Call)));
			Plan.PlacedName = ReturnLocal;
		}
		else
		{
			Statement.Lines.Add(MakeExprStmt(MoveTemp(Call)));
		}

		Statement.Dependencies = CurrentDependencies;
		const int32 StatementIndex = AddStatement(MoveTemp(Statement));

		for (int32 SlotIndex = 0; SlotIndex < Plan.Slots.Num(); ++SlotIndex)
		{
			FValuePlan& Slot = Plan.Slots[SlotIndex];
			Slot.bNamed = true;
			Slot.Statement = StatementIndex;

			if (SlotIndex == ReturnSlot)
			{
				Slot.Name = ReturnLocal;
				continue;
			}
			const FString OutputName = Node.OutputNames.IsValidIndex(SlotIndex) ? Node.OutputNames[SlotIndex] : FString();
			if (const FCallableParam* Param = Callable->FindParamByPin(OutputName, /* bOutput */ true))
			{
				if (const FString* Local = Locals.Find(Param->Identifier))
				{
					Slot.Name = *Local;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------- roots

	int32 FGraphWriter::RegionOfWrite(const FIRValue& Value) const
	{
		// `m.Roughness = x * 2;` inside a region made the Multiply there. A write whose value is written in place goes
		// where that value was; one that only names a variable has no nodes of its own and belongs nowhere.
		if (!Graph.IsValidValue(Value))
		{
			return INDEX_NONE;
		}
		const FNodePlan& Plan = Plans[Value.Node];
		const int32 SlotIndex = SlotOfValue(Value);
		const bool bInline = (Plan.Shape == ENodeShape::Single || Plan.Shape == ENodeShape::Sample || Plan.Shape == ENodeShape::MultiOutput || Plan.Shape == ENodeShape::Attributes)
			&& Plan.Slots.IsValidIndex(SlotIndex)
			&& !Plan.Slots[SlotIndex].bNamed;
		return (bInline && Plan.bHomeRegionKnown) ? Plan.HomeRegion : INDEX_NONE;
	}

	void FGraphWriter::AddRootStatement(FStmtPtr Line, const int32 Region, const int64 Sequence, const FString& OrderedTarget)
	{
		FPlannedStatement Statement;
		Statement.Lines.Add(MoveTemp(Line));
		Statement.Region = Region;
		Statement.Sequence = Sequence;
		Statement.Dependencies = CurrentDependencies;

		// Writes to one material keep their order: SetMaterialAttributes pairs its inputs with AttributeSetTypes in the
		// order the attributes were first written.
		if (!OrderedTarget.IsEmpty())
		{
			if (const int32* Previous = PreviousWriteByTarget.Find(OrderedTarget))
			{
				Statement.Dependencies.AddUnique(*Previous);
			}
		}

		const int32 StatementIndex = AddStatement(MoveTemp(Statement));
		if (!OrderedTarget.IsEmpty())
		{
			PreviousWriteByTarget.Add(OrderedTarget, StatementIndex);
		}
	}

	void FGraphWriter::EmitMaterialWrites(const FRootSpec& Root, int64& InOutSequence)
	{
		const FIRNode& Group = Graph.Nodes[Root.Value.Node];

		if (Group.Op == EIROp::SetMaterialAttributes)
		{
			const FIRInput* Source = Group.FindInput(TEXT("MaterialAttributes"));
			// A layer's own material arriving and leaving: nothing to say. Anything else replaces the parameter first.
			const bool bOwnInput = Source && Root.Kind == ERootKind::OutputMaterial && Source->Value == FIRValue{ Root.Node, 0 };
			if (Source && Source->Value.IsValid() && !bOwnInput)
			{
				CurrentDependencies.Reset();
				FExprPtr Value = TakeValue(Source->Value);
				AddRootStatement(MakeAssignStmt(MakeIdentifierExpr(Root.Target), MoveTemp(Value)), RegionOfWrite(Source->Value), InOutSequence++, Root.Target);
			}
		}

		for (const FIRInput& Input : Group.Inputs)
		{
			if (!Input.Value.IsValid() || (Group.Op == EIROp::SetMaterialAttributes && Input.Pin.Equals(TEXT("MaterialAttributes"), ESearchCase::CaseSensitive)))
			{
				continue;
			}
			if (!IsIdentifierText(Input.Pin))
			{
				Owner.Error(TEXT("DSH9078"), FText::Format(
					LOCTEXT("WriteAttributeUnnamed", "{0} writes an attribute '{1}' that is not a name the language has; the write is dropped."),
					DescribeNode(Root.Value.Node),
					FText::FromString(Input.Pin)));
				continue;
			}

			CurrentDependencies.Reset();
			FExprPtr Value = TakeValue(Input.Value);
			AddRootStatement(
				MakeAssignStmt(MakeMemberExpr(MakeIdentifierExpr(Root.Target), Input.Pin), MoveTemp(Value)),
				RegionOfWrite(Input.Value),
				InOutSequence++,
				Root.Target);
		}
	}

	void FGraphWriter::EmitRoots()
	{
		int64 Sequence = GDecompileRootSequenceBase;

		for (const FRootSpec& Root : Roots)
		{
			switch (Root.Kind)
			{
			case ERootKind::Statement:
			{
				if (!Graph.Nodes.IsValidIndex(Root.Node))
				{
					break;
				}
				CurrentDependencies.Reset();
				FExprPtr Call = BuildReflectedCall(Root.Node, Graph.Nodes[Root.Node]);
				AddRootStatement(MakeExprStmt(MoveTemp(Call)), RegionOfNode(Root.Node), Sequence++, FString());
				break;
			}

			case ERootKind::SinkAttribute:
			{
				if (!IsIdentifierText(Root.PinName))
				{
					Owner.Error(TEXT("DSH9078"), FText::Format(
						LOCTEXT("SinkAttributeUnnamed", "'{0}' wires a material attribute '{1}' that is not a name the language has; the connection is dropped."),
						FText::FromString(Product.Name),
						FText::FromString(Root.PinName)));
					break;
				}
				CurrentDependencies.Reset();
				FExprPtr Value = TakeValue(Root.Value);
				AddRootStatement(
					MakeAssignStmt(MakeMemberExpr(MakeIdentifierExpr(Root.Target), Root.PinName), MoveTemp(Value)),
					RegionOfWrite(Root.Value),
					Sequence++,
					Root.Target);
				break;
			}

			case ERootKind::SinkWhole:
			case ERootKind::OutputMaterial:
			{
				if (Graph.IsValidValue(Root.Value) && Plans[Root.Value.Node].bWrittenThroughRoot)
				{
					EmitMaterialWrites(Root, Sequence);
					break;
				}
				// The material that came in, handed back untouched.
				if (Root.Kind == ERootKind::OutputMaterial && Root.Value == FIRValue{ Root.Node, 0 })
				{
					break;
				}
				CurrentDependencies.Reset();
				FExprPtr Value = TakeValue(Root.Value);
				AddRootStatement(MakeAssignStmt(MakeIdentifierExpr(Root.Target), MoveTemp(Value)), RegionOfWrite(Root.Value), Sequence++, Root.Target);
				break;
			}

			case ERootKind::OutputAssign:
			{
				CurrentDependencies.Reset();
				FExprPtr Value = TakeValue(Root.Value);
				AddRootStatement(MakeAssignStmt(MakeIdentifierExpr(Root.Target), MoveTemp(Value)), RegionOfWrite(Root.Value), Sequence++, Root.Target);
				break;
			}

			case ERootKind::OutputReturn:
			{
				CurrentDependencies.Reset();
				FExprPtr Value = TakeValue(Root.Value);
				AddRootStatement(MakeReturnStmt(MoveTemp(Value)), INDEX_NONE, GDecompileReturnSequence, FString());
				break;
			}
			}
		}
	}

	// --------------------------------------------------------------------------------------- order

	int32 FGraphWriter::ParentOfRegion(const int32 Region) const
	{
		if (!Graph.Regions.IsValidIndex(Region))
		{
			return INDEX_NONE;
		}
		const int32 Parent = Graph.Regions[Region].Parent;
		return (Graph.Regions.IsValidIndex(Parent) && Parent != Region) ? Parent : INDEX_NONE;
	}

	int32 FGraphWriter::ChildRegionOnPath(const int32 Ancestor, const int32 Region) const
	{
		// The region directly under Ancestor that Region lies in (Region itself when it is that child); INDEX_NONE when
		// Region is not inside Ancestor at all.
		int32 Current = Region;
		for (int32 Guard = 0; Guard <= Graph.Regions.Num() && Current != INDEX_NONE; ++Guard)
		{
			const int32 Parent = ParentOfRegion(Current);
			if (Parent == Ancestor)
			{
				return Current;
			}
			Current = Parent;
		}
		return INDEX_NONE;
	}

	bool FGraphWriter::OrderRegion(const int32 Region, TArray<int32>& OutOrder) const
	{
		struct FItem
		{
			int32 Statement = INDEX_NONE;
			int32 ChildRegion = INDEX_NONE;
			int64 Key = MAX_int64;
			TArray<int32> Members;
			TArray<int32> DependsOn;
			bool bDone = false;
		};

		TArray<FItem> Items;
		TMap<int32, int32> ItemOfStatement;
		TMap<int32, int32> ItemOfChildRegion;

		for (int32 StatementIndex = 0; StatementIndex < Statements.Num(); ++StatementIndex)
		{
			const FPlannedStatement& Statement = Statements[StatementIndex];
			int32 ItemIndex = INDEX_NONE;
			if (Statement.Region == Region)
			{
				ItemIndex = Items.AddDefaulted();
				Items[ItemIndex].Statement = StatementIndex;
			}
			else
			{
				const int32 Child = ChildRegionOnPath(Region, Statement.Region);
				if (Child == INDEX_NONE)
				{
					continue;
				}
				if (const int32* Existing = ItemOfChildRegion.Find(Child))
				{
					ItemIndex = *Existing;
				}
				else
				{
					ItemIndex = Items.AddDefaulted();
					Items[ItemIndex].ChildRegion = Child;
					ItemOfChildRegion.Add(Child, ItemIndex);
				}
			}

			Items[ItemIndex].Members.Add(StatementIndex);
			Items[ItemIndex].Key = FMath::Min(Items[ItemIndex].Key, Statement.Sequence);
			ItemOfStatement.Add(StatementIndex, ItemIndex);
		}

		for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
		{
			for (const int32 Member : Items[ItemIndex].Members)
			{
				for (const int32 Dependency : Statements[Member].Dependencies)
				{
					// A dependency outside this block is the enclosing block's to order.
					const int32* Other = ItemOfStatement.Find(Dependency);
					if (Other && *Other != ItemIndex)
					{
						Items[ItemIndex].DependsOn.AddUnique(*Other);
					}
				}
			}
		}

		for (int32 Emitted = 0; Emitted < Items.Num(); ++Emitted)
		{
			int32 Best = INDEX_NONE;
			for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
			{
				const FItem& Item = Items[ItemIndex];
				if (Item.bDone)
				{
					continue;
				}
				bool bReady = true;
				for (const int32 Other : Item.DependsOn)
				{
					bReady = bReady && Items[Other].bDone;
				}
				if (bReady && (Best == INDEX_NONE || Item.Key < Items[Best].Key))
				{
					Best = ItemIndex;
				}
			}
			if (Best == INDEX_NONE)
			{
				// Two blocks need each other: this region cannot be written in one piece.
				return false;
			}

			Items[Best].bDone = true;
			if (Items[Best].ChildRegion != INDEX_NONE)
			{
				if (!OrderRegion(Items[Best].ChildRegion, OutOrder))
				{
					return false;
				}
			}
			else
			{
				OutOrder.Add(Items[Best].Statement);
			}
		}
		return true;
	}

	void FGraphWriter::AppendInSequence(TArray<int32>& OutOrder) const
	{
		OutOrder.Reset();
		for (int32 StatementIndex = 0; StatementIndex < Statements.Num(); ++StatementIndex)
		{
			OutOrder.Add(StatementIndex);
		}
		OutOrder.StableSort([this](const int32 A, const int32 B)
		{
			return Statements[A].Sequence < Statements[B].Sequence;
		});
	}

	FString FGraphWriter::MakeRegionTitle(const int32 Region)
	{
		// `#pragma region <title>` runs to the end of its line.
		const FString Name = Graph.Regions.IsValidIndex(Region) ? Graph.Regions[Region].Name : FString();
		FString Title = Name.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
		if (Title.IsEmpty())
		{
			Title = TEXT("Region");
		}
		if (!Title.Equals(Name, ESearchCase::CaseSensitive) && !ReportedRegionTitles.Contains(Region))
		{
			ReportedRegionTitles.Add(Region);
			Owner.Warning(TEXT("DSH9076"), FText::Format(
				LOCTEXT("RegionTitleChanged", "A region of '{0}' is titled '{1}', which does not fit on a '#pragma region' line; it is written as '{2}'."),
				FText::FromString(Product.Name),
				FText::FromString(Name),
				FText::FromString(Title)));
		}
		return Title;
	}

	void FGraphWriter::EmitOrdered(const TArray<int32>& InOrder, FBlockStmt& Body)
	{
		FModule& Module = Owner.GetModule();
		const auto MarkBlankLineBefore = [&Module](const FStmt& Statement)
		{
			Module.Trivia.FindOrAdd(&Statement).BlankLinesBefore = 1;
		};

		TArray<int32> Open;
		bool bFirst = true;
		bool bAfterRegion = false;
		bool bSeenRoot = false;

		for (const int32 StatementIndex : InOrder)
		{
			FPlannedStatement& Statement = Statements[StatementIndex];

			// Outermost first.
			TArray<int32> Path;
			for (int32 Region = Statement.Region, Guard = 0; Region != INDEX_NONE && Guard <= Graph.Regions.Num(); Region = ParentOfRegion(Region), ++Guard)
			{
				Path.Insert(Region, 0);
			}

			int32 Common = 0;
			while (Common < Open.Num() && Common < Path.Num() && Open[Common] == Path[Common])
			{
				++Common;
			}
			while (Open.Num() > Common)
			{
				Body.Statements.Add(MakeRegionStmt(/* bBegin */ false, FString()));
				Open.Pop();
				bAfterRegion = true;
			}
			for (int32 Depth = Common; Depth < Path.Num(); ++Depth)
			{
				FStmtPtr Begin = MakeRegionStmt(/* bBegin */ true, MakeRegionTitle(Path[Depth]));
				if (!bFirst)
				{
					MarkBlankLineBefore(*Begin);
				}
				Body.Statements.Add(MoveTemp(Begin));
				Open.Add(Path[Depth]);
				bFirst = false;
				bAfterRegion = false;
			}

			// The writes the body ends in stand apart from what computes them.
			const bool bIsRoot = Statement.Sequence >= GDecompileRootSequenceBase;
			const bool bBlank = !bFirst && (bAfterRegion || (bIsRoot && !bSeenRoot && Open.Num() == 0));
			bSeenRoot = bSeenRoot || bIsRoot;

			for (int32 LineIndex = 0; LineIndex < Statement.Lines.Num(); ++LineIndex)
			{
				if (!Statement.Lines[LineIndex])
				{
					continue;
				}
				if (LineIndex == 0 && bBlank)
				{
					MarkBlankLineBefore(*Statement.Lines[LineIndex]);
				}
				Body.Statements.Add(MoveTemp(Statement.Lines[LineIndex]));
				bFirst = false;
			}
			bAfterRegion = false;
		}

		while (Open.Num() > 0)
		{
			Body.Statements.Add(MakeRegionStmt(/* bBegin */ false, FString()));
			Open.Pop();
		}
	}

	TUniquePtr<FBlockStmt> FGraphWriter::AssembleBody()
	{
		TArray<int32> StatementOrder;
		if (!OrderRegion(INDEX_NONE, StatementOrder) || StatementOrder.Num() != Statements.Num())
		{
			AppendInSequence(StatementOrder);
		}

		TUniquePtr<FBlockStmt> Body = MakeUnique<FBlockStmt>();
		EmitOrdered(StatementOrder, *Body);
		return Body;
	}

	TUniquePtr<FBlockStmt> FGraphWriter::Build()
	{
		Analyze();
		for (const int32 NodeIndex : Order)
		{
			BuildNode(NodeIndex);
		}
		EmitRoots();
		return AssembleBody();
	}

	FString FGraphWriter::FindPlacedName(const int32 NodeIndex) const
	{
		if (!Plans.IsValidIndex(NodeIndex) || !Plans[NodeIndex].bReachable)
		{
			return FString();
		}

		const FNodePlan& Plan = Plans[NodeIndex];
		const FIRNode& Node = Graph.Nodes[NodeIndex];
		switch (Plan.Shape)
		{
		case ENodeShape::Identifier:
			if (Node.Op == EIROp::FunctionInput)
			{
				const FString* Identifier = InputIdentifiers.Find(NodeIndex);
				return Identifier ? *Identifier : FString();
			}
			if (Node.Op == EIROp::Parameter || Node.Op == EIROp::TextureParameter)
			{
				const FUniformModel* Uniform = Owner.FindUniform(Node);
				return Uniform ? Uniform->Identifier : FString();
			}
			return FString();

		case ENodeShape::MaterialGroup:
		case ENodeShape::CallStatement:
			return Plan.PlacedName;

		case ENodeShape::Single:
		case ENodeShape::Sample:
		case ENodeShape::MultiOutput:
		case ENodeShape::Attributes:
			// The variable whose statement makes the node on the way back: the first one declared.
			for (const FValuePlan& Slot : Plan.Slots)
			{
				if (Slot.bNamed && !Slot.Name.IsEmpty())
				{
					return Slot.Name;
				}
			}
			return FString();

		default:
			break;
		}
		return FString();
	}
}

#undef LOCTEXT_NAMESPACE
