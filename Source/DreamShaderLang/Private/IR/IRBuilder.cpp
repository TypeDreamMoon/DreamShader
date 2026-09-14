// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The builder's spine: the entry point, the per-product setup, frames, and every routine that
// makes a node. Expressions, statements, the material map and inlining live in the four files
// beside this one; they all come back here to AddNode, which is the only place FIRNode::Source,
// Region and DebugName are written.
//
// Diagnostics owned by this file: DSH4352 (no builtin catalog, said once for the whole module)
// and DSH4374 (Append over four components).

#include "IRBuilderInternal.h"

#include "Containers/Map.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"

#define LOCTEXT_NAMESPACE "DreamShader.IRBuilder"

namespace UE::DreamShader::IR::Private
{
	const TCHAR* const GSwizzleComponents = TEXT("xyzw");

	int32 ComponentIndexOf(TCHAR Component)
	{
		switch (Component)
		{
		case TCHAR('x'): return 0;
		case TCHAR('y'): return 1;
		case TCHAR('z'): return 2;
		case TCHAR('w'): return 3;
		default: return INDEX_NONE;
		}
	}

	// ------------------------------------------------------------------------------- comparisons

	bool FMaterialValue::operator==(const FMaterialValue& Other) const
	{
		if (Source != Other.Source || Order.Num() != Other.Order.Num() || Fields.Num() != Other.Fields.Num())
		{
			return false;
		}
		for (const TPair<FString, FIRValue>& Pair : Fields)
		{
			const FIRValue* Mine = Other.Fields.Find(Pair.Key);
			if (!Mine || *Mine != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool FLoweredValue::operator==(const FLoweredValue& Other) const
	{
		if (Kind != Other.Kind)
		{
			return false;
		}
		switch (Kind)
		{
		case EKind::Empty:
			return true;
		case EKind::Value:
			return Value == Other.Value;
		case EKind::Aggregate:
			if (Fields.Num() != Other.Fields.Num())
			{
				return false;
			}
			for (int32 Index = 0; Index < Fields.Num(); ++Index)
			{
				if (Fields[Index] != Other.Fields[Index])
				{
					return false;
				}
			}
			return true;
		case EKind::Material:
			return Material == Other.Material;
		default:
			return false;
		}
	}

	// ------------------------------------------------------------------------------------ setup

	FIRBuilder::FIRBuilder(const FBoundModule& InBound, const FIRBuildOptions& InOptions, FLangDiagnosticSink& InDiagnostics)
		: BoundModule(InBound)
		// The bound module carries the catalog its indices point into; the option overrides it for a
		// tool that loaded one of its own.
		, Catalog(InOptions.Catalog ? InOptions.Catalog : InBound.Catalog)
		, Options(InOptions)
		, Diagnostics(InDiagnostics)
	{
		SourceFile = InBound.Module ? InBound.Module->FilePath : FString();
	}

	void FIRBuilder::BuildModule(FIRModule& OutModule)
	{
		OutModule.SourceFilePath = SourceFile;
		OutModule.Includes = BoundModule.IncludePaths;

		if (!Catalog)
		{
			// Said once, at the top of the file, rather than at every `UE.*` call and every material
			// attribute below: one missing catalog is one problem, not fifty.
			Diagnostics.Error(TEXT("DSH4352"), FLangSpan(), LOCTEXT("IRBuilderNoCatalog",
				"This module was bound without a builtin catalog, so no 'UE.*' call and no material attribute can be named; run the bind with a catalog, or pass one in FIRBuildOptions."));
		}

		ProductByFunction.Reset();
		for (int32 Index = 0; Index < BoundModule.Products.Num(); ++Index)
		{
			if (BoundModule.Products[Index].FunctionIndex != INDEX_NONE)
			{
				ProductByFunction.Add(BoundModule.Products[Index].FunctionIndex, Index);
			}
		}

		OutModule.Products.SetNum(BoundModule.Products.Num());
		for (int32 Index = 0; Index < BoundModule.Products.Num(); ++Index)
		{
			BuildProduct(BoundModule.Products[Index], OutModule.Products[Index]);
		}
	}

	void FIRBuilder::BuildProduct(const FBoundProduct& BoundProduct, FIRProduct& OutProduct)
	{
		OutProduct.Kind = BoundProduct.Kind;
		OutProduct.Name = BoundProduct.AssetName;
		OutProduct.AssetPathOverride = BoundProduct.AssetPathOverride;
		OutProduct.Settings = BoundProduct.Settings;
		OutProduct.Backend = BoundProduct.Backend;
		OutProduct.BoundFunctionIndex = BoundProduct.FunctionIndex;

		// Every product carries the file's whole region tree and its layout hints: FIRNode::Region is
		// an index into it and FBoundModule::StatementRegions is indexed the same way, so copying the
		// tree whole is the only thing that keeps those indices meaningful. Hints for another
		// product's variables simply match nothing when the layout pass looks them up by name.
		OutProduct.Graph.Regions = BoundModule.Regions;
		OutProduct.Graph.LayoutHints = BoundModule.LayoutHints;

		if (!BoundModule.Functions.IsValidIndex(BoundProduct.FunctionIndex))
		{
			return;
		}

		const FBoundFunction& Function = BoundModule.Functions[BoundProduct.FunctionIndex];
		OutProduct.Description = Function.Directives.Desc.IsEmpty() ? Function.Directives.FreeText : Function.Directives.Desc;
		OutProduct.LibraryPath = Function.Directives.Library;
		OutProduct.Source.File = Function.File.IsEmpty() ? SourceFile : Function.File;
		OutProduct.Source.Span = Function.Decl ? Function.Decl->Span : FLangSpan();

		Graph = &OutProduct.Graph;
		Frames.Reset();
		GlobalValues.Reset();
		// A body's own regions nest under the box its function was declared in (S-report #9).
		CurrentRegion = INDEX_NONE;
		if (Function.Decl)
		{
			if (const int32* DeclRegion = BoundModule.StatementRegions.Find(Function.Decl))
			{
				CurrentRegion = *DeclRegion;
			}
		}
		BranchDepth = 0;
		LoopDepth = 0;
		bBreak = false;
		bContinue = false;

		switch (Function.Kind)
		{
		case EBoundFunctionKind::Entry:
			BuildEntryProduct(Function);
			break;
		case EBoundFunctionKind::Layer:
			BuildLayerProduct(Function, /* bIsBlend */ false);
			break;
		case EBoundFunctionKind::LayerBlend:
			BuildLayerProduct(Function, /* bIsBlend */ true);
			break;
		case EBoundFunctionKind::ExportFunction:
			BuildFunctionProduct(Function);
			break;
		case EBoundFunctionKind::Helper:
		case EBoundFunctionKind::Custom:
		case EBoundFunctionKind::Extern:
		default:
			// A Helper, a Custom or an Extern is never a product; the binder does not make one.
			break;
		}

		Graph = nullptr;
		Frames.Reset();
	}

	// ----------------------------------------------------------------------------------- frames

	void FIRBuilder::PushFrame(const FBoundFunction& Function, int32 FunctionIndex)
	{
		TUniquePtr<FFrame> NewFrame = MakeUnique<FFrame>();
		NewFrame->Function = &Function;
		NewFrame->FunctionIndex = FunctionIndex;
		NewFrame->Locals.SetNum(Function.Locals.Num());
		NewFrame->Params.SetNum(Function.Params.Num());
		NewFrame->OutTargets.SetNum(Function.Params.Num());

		// Every local starts as nothing-assigned-yet, in the shape its type needs: an aggregate has its
		// fields in place from the start, so an assignment to one field of an otherwise untouched struct
		// does not have to invent the rest, and a read of what nobody wrote is DSH4376 rather than a
		// value that is quietly not there.
		for (int32 Index = 0; Index < Function.Locals.Num(); ++Index)
		{
			const FBoundLocal& Local = Function.Locals[Index];
			NewFrame->Locals[Index] = MakeUnassigned(Local.Type, Local.ArrayCount);
		}

		Frames.Add(MoveTemp(NewFrame));
	}

	void FIRBuilder::PopFrame()
	{
		Frames.Pop();
	}

	FLoweredValue* FIRBuilder::ResolveSlot(const FLValueRef& Ref)
	{
		if (!Ref.IsValid() || !Frames.IsValidIndex(Ref.FrameIndex))
		{
			return nullptr;
		}

		FFrame& Target = *Frames[Ref.FrameIndex];
		TArray<FLoweredValue>& Slots = Ref.bParam ? Target.Params : Target.Locals;
		if (!Slots.IsValidIndex(Ref.BaseIndex))
		{
			return nullptr;
		}

		FLoweredValue* Slot = &Slots[Ref.BaseIndex];
		for (const int32 Field : Ref.FieldPath)
		{
			if (!Slot->IsAggregate() || !Slot->Fields.IsValidIndex(Field))
			{
				return nullptr;
			}
			Slot = &Slot->Fields[Field];
		}
		return Slot;
	}

	FIRType FIRBuilder::DeclaredTypeOf(const FLValueRef& Ref) const
	{
		if (!Ref.IsValid() || !Frames.IsValidIndex(Ref.FrameIndex) || !Frames[Ref.FrameIndex]->Function)
		{
			return FIRType::Error();
		}

		const FBoundFunction& Function = *Frames[Ref.FrameIndex]->Function;
		FIRType Type = FIRType::Error();
		int32 ArrayCount = 0;
		if (Ref.bParam)
		{
			if (!Function.Params.IsValidIndex(Ref.BaseIndex))
			{
				return FIRType::Error();
			}
			Type = Function.Params[Ref.BaseIndex].Type;
			ArrayCount = Function.Params[Ref.BaseIndex].ArrayCount;
		}
		else
		{
			if (!Function.Locals.IsValidIndex(Ref.BaseIndex))
			{
				return FIRType::Error();
			}
			Type = Function.Locals[Ref.BaseIndex].Type;
			ArrayCount = Function.Locals[Ref.BaseIndex].ArrayCount;
		}

		for (const int32 Field : Ref.FieldPath)
		{
			if (ArrayCount > 0)
			{
				// An element of an array: the declared type already is the element type.
				ArrayCount = 0;
				continue;
			}
			if (!Type.IsStruct()
				|| !BoundModule.Structs.IsValidIndex(Type.StructIndex)
				|| !BoundModule.Structs[Type.StructIndex].Fields.IsValidIndex(Field))
			{
				return FIRType::Error();
			}
			const FBoundStructField& StructField = BoundModule.Structs[Type.StructIndex].Fields[Field];
			Type = StructField.Type;
			ArrayCount = StructField.ArrayCount;
		}
		return Type;
	}

	FLoweredValue FIRBuilder::MakeUnassigned(const FIRType& Type, int32 ArrayCount, int32 Depth) const
	{
		if (ArrayCount > 0)
		{
			FLoweredValue Result = FLoweredValue::MakeAggregate(ArrayCount);
			for (FLoweredValue& Element : Result.Fields)
			{
				Element = MakeUnassigned(Type, 0, Depth + 1);
			}
			return Result;
		}
		// The depth bound is a guard, not a feature: a struct that contained itself would have no size,
		// so no real type comes near it.
		if (Type.IsStruct() && BoundModule.Structs.IsValidIndex(Type.StructIndex) && Depth < 16)
		{
			const FBoundStruct& Struct = BoundModule.Structs[Type.StructIndex];
			FLoweredValue Result = FLoweredValue::MakeAggregate(Struct.Fields.Num());
			for (int32 Index = 0; Index < Struct.Fields.Num(); ++Index)
			{
				Result.Fields[Index] = MakeUnassigned(Struct.Fields[Index].Type, Struct.Fields[Index].ArrayCount, Depth + 1);
			}
			return Result;
		}
		if (Type.IsMaterial())
		{
			// A `material` local is a map from the start, as the entry's own material is: writing an
			// attribute of it is how it gets its value, not a read of something missing.
			return FLoweredValue::MakeMaterial();
		}
		return FLoweredValue::NeverAssigned();
	}

	bool FIRBuilder::ActiveCallSite(FLangSpan& OutSpan, FString& OutFile) const
	{
		// The OUTERMOST call site, not the innermost: it is the only span guaranteed to be in the
		// product's own function body, which is what the editor's "Open Source Line" needs when a
		// node came out of a helper that a helper called.
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			if (!Frames[Index]->bHasCallSite)
			{
				continue;
			}
			OutSpan = Frames[Index]->CallSite;
			// The file the CALL was written in, which is the frame below this one -- a different
			// file from the helper's exactly when the helper came out of an included header.
			const FBoundFunction* Caller = (Index > 0) ? Frames[Index - 1]->Function : nullptr;
			OutFile = (Caller && !Caller->File.IsEmpty()) ? Caller->File : SourceFile;
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------ nodes

	FIRValue FIRBuilder::AddNode(FIRNode&& Node, const FLangSpan& Span)
	{
		check(Graph != nullptr);

		// File names the span, so it is the file the code that produced the node was written in --
		// an inlined helper from a `.dsh` keeps the header's path -- and CallSite/CallSiteFile point
		// back into the product's own body, which is a different file exactly then.
		Node.Source.File = SourceFile;
		if (!Frames.IsEmpty() && Frames.Last()->Function && !Frames.Last()->Function->File.IsEmpty())
		{
			Node.Source.File = Frames.Last()->Function->File;
		}
		Node.Source.Span = Span;
		ActiveCallSite(Node.Source.CallSite, Node.Source.CallSiteFile);
		Node.Region = CurrentRegion;
		if (!Options.bKeepDebugNames)
		{
			Node.DebugName.Reset();
		}

		FIRValue Result;
		Result.Node = Graph->AddNode(MoveTemp(Node));
		Result.Output = 0;
		return Result;
	}

	FIRValue FIRBuilder::MakeConstant(const double* Components, int32 Num, const FLangSpan& Span)
	{
		const int32 Width = FMath::Clamp(Num, 1, 4);
		FIRNode Node;
		Node.Op = EIROp::Constant;
		Node.Outputs.Add(FIRType::Float(Width));
		Node.Properties.Add({ FString(Prop::Value), FIRPropertyValue::MakeFloat4(Components, Width) });
		return AddNode(MoveTemp(Node), Span);
	}

	FIRValue FIRBuilder::MakeScalarConstant(double Value, const FLangSpan& Span)
	{
		const double Components[4] = { Value, 0.0, 0.0, 0.0 };
		return MakeConstant(Components, 1, Span);
	}

	FIRValue FIRBuilder::MakeCoreOp(EIROp Op, TArray<FIRValue> Operands, const FIRType& Result, const FLangSpan& Span)
	{
		FIRNode Node;
		Node.Op = Op;
		Node.Operands = MoveTemp(Operands);
		Node.Outputs.Add(Result);
		return AddNode(MoveTemp(Node), Span);
	}

	int32 FIRBuilder::WidthOf(FIRValue Value) const
	{
		const FIRType Type = TypeOfValue(Value);
		const int32 Width = Type.GraphComponentCount();
		return Width > 0 ? Width : 1;
	}

	FIRType FIRBuilder::TypeOfValue(FIRValue Value) const
	{
		if (!Graph || !Graph->Nodes.IsValidIndex(Value.Node))
		{
			return FIRType::Error();
		}
		const FIRNode& Node = Graph->Nodes[Value.Node];
		return Node.Outputs.IsValidIndex(Value.Output) ? Node.Outputs[Value.Output] : FIRType::Error();
	}

	FIRType FIRBuilder::GraphTypeOf(const FIRType& Type) const
	{
		switch (Type.Kind)
		{
		case EIRTypeKind::Bool:
			return FIRType::Bool(FMath::Clamp(Type.Rows, 1, 4));
		case EIRTypeKind::Int:
		case EIRTypeKind::UInt:
		case EIRTypeKind::Float:
		case EIRTypeKind::Half:
		case EIRTypeKind::Double:
			// A matrix has no graph form at all; callers test IsMatrix() and report DSH4361 before
			// they get here, so the clamp below is a floor, not a policy.
			return FIRType::Float(FMath::Clamp(Type.GraphComponentCount(), 1, 4));
		default:
			return Type;
		}
	}

	FIRValue FIRBuilder::ExtractComponent(FIRValue Value, int32 Index, const FLangSpan& Span)
	{
		if (WidthOf(Value) <= 1)
		{
			return Value;
		}
		const int32 Component = FMath::Clamp(Index, 0, 3);
		return MakeSwizzle(Value, FString::Chr(GSwizzleComponents[Component]), Span);
	}

	// ----------------------------------------------------------------------------- channel views

	int32 FIRBuilder::ChannelMaskOfOutputName(const FString& Name)
	{
		// One to four of `RGBA`, upper case, in channel order, no letter twice: the names the engine gives
		// the component views it names, and the names the reflection filler gives the ones it does not
		// (FIX3-Binder). The binder's ChannelViewMaskOfName reads them the same way; the two must agree.
		if (Name.Len() == 0 || Name.Len() > 4)
		{
			return 0;
		}
		int32 Mask = 0;
		int32 Previous = INDEX_NONE;
		for (int32 Index = 0; Index < Name.Len(); ++Index)
		{
			int32 Channel = INDEX_NONE;
			switch (Name[Index])
			{
			case TCHAR('R'): Channel = 0; break;
			case TCHAR('G'): Channel = 1; break;
			case TCHAR('B'): Channel = 2; break;
			case TCHAR('A'): Channel = 3; break;
			default: return 0;
			}
			if (Channel <= Previous)
			{
				return 0;
			}
			Mask |= 1 << Channel;
			Previous = Channel;
		}
		return Mask;
	}

	bool FIRBuilder::IsChannelViewNode(const FIRNode& Node) const
	{
		// The binder's TryGetChannelViewWidth, read off the node its catalog entry became: every output a
		// view exactly as wide as its name, never a `Numeric` pin, and the views together covering at least
		// R and G. That is the line 1.x's TryRetargetChannelMaskToOutput draws between views of one value
		// (VertexColor) and different values (SceneTexture's Color, Size, InvSize).
		if (Node.Op != EIROp::Reflected || Node.Outputs.Num() < 2 || Node.OutputNames.Num() != Node.Outputs.Num())
		{
			return false;
		}
		const FCatalogExpression* Entry = (Catalog && Catalog->Expressions.IsValidIndex(Node.CatalogIndex))
			? &Catalog->Expressions[Node.CatalogIndex]
			: nullptr;

		int32 Covered = 0;
		for (int32 Slot = 0; Slot < Node.Outputs.Num(); ++Slot)
		{
			const FString& Name = Node.OutputNames[Slot];
			const int32 Channels = ChannelMaskOfOutputName(Name);
			const FIRType& Type = Node.Outputs[Slot];
			if (Channels == 0
				|| Type.Kind != EIRTypeKind::Float
				|| Type.Cols != 1
				|| Type.Rows != Name.Len()
				|| (Entry && Entry->Outputs.IsValidIndex(Slot) && Entry->Outputs[Slot].Type == ECatalogValueType::Numeric))
			{
				return false;
			}
			Covered |= Channels;
		}

		int32 Width = 0;
		while (Width < 4 && (Covered & (1 << Width)) != 0)
		{
			++Width;
		}
		return Width >= 2;
	}

	int32 FIRBuilder::FindChannelViewSlot(const FIRNode& Node, int32 ChannelMask)
	{
		for (int32 Slot = 0; ChannelMask != 0 && Slot < Node.OutputNames.Num(); ++Slot)
		{
			if (ChannelMaskOfOutputName(Node.OutputNames[Slot]) == ChannelMask)
			{
				return Slot;
			}
		}
		return INDEX_NONE;
	}

	int32 FIRBuilder::ChannelViewAppendSource(FIRValue Value, int32 Depth) const
	{
		if (!Graph || !Graph->Nodes.IsValidIndex(Value.Node) || Depth > 4)
		{
			return INDEX_NONE;
		}
		const FIRNode& Node = Graph->Nodes[Value.Node];
		if (Node.Op == EIROp::Reflected)
		{
			return IsChannelViewNode(Node) ? Value.Node : INDEX_NONE;
		}
		if (Node.Op != EIROp::Append || Node.Operands.Num() != 2)
		{
			return INDEX_NONE;
		}
		const int32 Left = ChannelViewAppendSource(Node.Operands[0], Depth + 1);
		const int32 Right = ChannelViewAppendSource(Node.Operands[1], Depth + 1);
		return (Left != INDEX_NONE && Left == Right) ? Left : INDEX_NONE;
	}

	FIRValue FIRBuilder::WidenChannelView(FIRValue Value, int32 Width, const FLangSpan& Span)
	{
		if (!Graph || !Graph->Nodes.IsValidIndex(Value.Node) || Width < 2 || Width > 4)
		{
			return FIRValue::None();
		}

		const int32 NodeIndex = Value.Node;
		int32 Covered = 0;
		{
			const FIRNode& Node = Graph->Nodes[NodeIndex];
			if (!IsChannelViewNode(Node) || !Node.OutputNames.IsValidIndex(Value.Output))
			{
				return FIRValue::None();
			}
			// Only a view that LEADS the value -- R first, no gap -- stands for its first components.
			const int32 Length = Node.OutputNames[Value.Output].Len();
			if (ChannelMaskOfOutputName(Node.OutputNames[Value.Output]) != (1 << Length) - 1 || Length >= Width)
			{
				return FIRValue::None();
			}
			// A view that is exactly the wanted run is the answer on its own (a TextureSample's RGBA).
			const int32 Exact = FindChannelViewSlot(Node, (1 << Width) - 1);
			if (Exact != INDEX_NONE)
			{
				return FIRValue{ NodeIndex, Exact };
			}
			Covered = Length;
		}

		// Otherwise the views that carry the rest, in channel order, each the widest that starts where the
		// last one stopped: VertexColor's RGB, then its A.
		TArray<FIRValue> Parts;
		Parts.Add(Value);
		while (Covered < Width)
		{
			const FIRNode& Node = Graph->Nodes[NodeIndex];
			int32 Next = INDEX_NONE;
			int32 NextLength = 0;
			for (int32 Slot = 0; Slot < Node.OutputNames.Num(); ++Slot)
			{
				const int32 Length = Node.OutputNames[Slot].Len();
				const int32 Channels = ChannelMaskOfOutputName(Node.OutputNames[Slot]);
				if (Channels != 0
					&& Channels == (((1 << Length) - 1) << Covered)
					&& Covered + Length <= Width
					&& Length > NextLength)
				{
					Next = Slot;
					NextLength = Length;
				}
			}
			if (Next == INDEX_NONE)
			{
				return FIRValue::None();
			}
			Parts.Add(FIRValue{ NodeIndex, Next });
			Covered += NextLength;
		}
		return MakeAppend(Parts, Span);
	}

	FIRValue FIRBuilder::MakeSwizzle(FIRValue Value, const FString& Mask, const FLangSpan& Span)
	{
		if (!Value.IsValid() || Mask.IsEmpty())
		{
			return Value;
		}

		// A read off channel views picks a pin rather than masking one, as 1.x's retarget does
		// (FIX3-Binder). Two shapes arrive here: the whole value WidenChannelView made -- an Append of
		// views -- where a mask inside one operand is a read of that operand; and a view itself, where a
		// mask another view publishes exactly IS that view (`VC.r` is the R pin, not a ComponentMask over
		// RGB). Only for the views of one reflected node: a constructor's Append keeps the ComponentMask
		// the 1.x generator builds for it.
		if (Graph && Graph->Nodes.IsValidIndex(Value.Node))
		{
			const EIROp SourceOp = Graph->Nodes[Value.Node].Op;
			if (SourceOp == EIROp::Append && ChannelViewAppendSource(Value, 0) != INDEX_NONE)
			{
				const FIRValue Lead = Graph->Nodes[Value.Node].Operands[0];
				const FIRValue Tail = Graph->Nodes[Value.Node].Operands[1];
				const int32 LeadWidth = WidthOf(Lead);
				int32 Lowest = 4;
				int32 Highest = INDEX_NONE;
				for (int32 Index = 0; Index < Mask.Len(); ++Index)
				{
					const int32 Component = ComponentIndexOf(Mask[Index]);
					Lowest = FMath::Min(Lowest, Component);
					Highest = FMath::Max(Highest, Component);
				}
				if (Lowest >= 0 && Highest < LeadWidth)
				{
					return MakeSwizzle(Lead, Mask, Span);
				}
				if (Lowest >= LeadWidth)
				{
					FString Rebased;
					for (int32 Index = 0; Index < Mask.Len(); ++Index)
					{
						Rebased.AppendChar(GSwizzleComponents[FMath::Clamp(ComponentIndexOf(Mask[Index]) - LeadWidth, 0, 3)]);
					}
					return MakeSwizzle(Tail, Rebased, Span);
				}
			}
			else if (SourceOp == EIROp::Reflected
				&& IsChannelViewNode(Graph->Nodes[Value.Node])
				&& Graph->Nodes[Value.Node].OutputNames.IsValidIndex(Value.Output))
			{
				const FIRNode& View = Graph->Nodes[Value.Node];
				const int32 OutputChannels = ChannelMaskOfOutputName(View.OutputNames[Value.Output]);
				int32 Wanted = 0;
				int32 Previous = INDEX_NONE;
				for (int32 Index = 0; Index < Mask.Len(); ++Index)
				{
					const int32 Component = ComponentIndexOf(Mask[Index]);
					if (Component == INDEX_NONE || Component <= Previous)
					{
						// A reordering or a repeat is never one pin.
						Wanted = 0;
						break;
					}
					Previous = Component;
					// Component N of the mask is the output's Nth channel.
					int32 Channel = INDEX_NONE;
					int32 Seen = 0;
					for (int32 Bit = 0; Bit < 4; ++Bit)
					{
						if ((OutputChannels & (1 << Bit)) == 0)
						{
							continue;
						}
						if (Seen == Component)
						{
							Channel = Bit;
							break;
						}
						++Seen;
					}
					if (Channel == INDEX_NONE)
					{
						Wanted = 0;
						break;
					}
					Wanted |= 1 << Channel;
				}
				const int32 Slot = FindChannelViewSlot(View, Wanted);
				if (Slot != INDEX_NONE && Slot != Value.Output)
				{
					return FIRValue{ Value.Node, Slot };
				}
			}
		}

		const int32 SourceWidth = WidthOf(Value);

		// An identity mask -- the full ascending run of the value's own components -- is not a node.
		// `.r` on a float1 and `.rgb` on a float3 both land here.
		if (Mask.Len() == SourceWidth)
		{
			bool bIdentity = true;
			for (int32 Index = 0; Index < Mask.Len(); ++Index)
			{
				if (Mask[Index] != GSwizzleComponents[Index])
				{
					bIdentity = false;
					break;
				}
			}
			if (bIdentity)
			{
				return Value;
			}
		}

		// A strictly ascending mask is one ComponentMask node: R/G/B/A flags are all a
		// UMaterialExpressionComponentMask can say, and they cannot express an order. Anything else
		// (`.yx`, `.xx`) is per-component masks plus AppendVector, which is exactly what the 1.x
		// generator emits for the same source (CodeSwizzle.cpp, TryBuildOrderedSwizzleMask).
		bool bAscending = true;
		int32 Previous = INDEX_NONE;
		for (int32 Index = 0; Index < Mask.Len(); ++Index)
		{
			const int32 Component = ComponentIndexOf(Mask[Index]);
			if (Component == INDEX_NONE || Component <= Previous)
			{
				bAscending = false;
				break;
			}
			Previous = Component;
		}

		if (bAscending)
		{
			FIRNode Node;
			Node.Op = EIROp::Swizzle;
			Node.Operands.Add(Value);
			Node.Outputs.Add(FIRType::Float(Mask.Len()));
			Node.Properties.Add({ FString(Prop::Mask), FIRPropertyValue::MakeString(Mask) });
			return AddNode(MoveTemp(Node), Span);
		}

		TArray<FIRValue> Parts;
		Parts.Reserve(Mask.Len());
		for (int32 Index = 0; Index < Mask.Len(); ++Index)
		{
			const int32 Component = ComponentIndexOf(Mask[Index]);
			Parts.Add(ExtractComponent(Value, FMath::Max(Component, 0), Span));
		}
		return MakeAppend(Parts, Span);
	}

	FIRValue FIRBuilder::MakeAppend(const TArray<FIRValue>& Parts, const FLangSpan& Span)
	{
		if (Parts.IsEmpty())
		{
			return FIRValue::None();
		}
		if (Parts.Num() == 1)
		{
			return Parts[0];
		}

		int32 Total = 0;
		for (const FIRValue& Part : Parts)
		{
			Total += WidthOf(Part);
		}
		if (Total > 4)
		{
			Diagnostics.Error(TEXT("DSH4374"), Span, FText::Format(
				LOCTEXT("IRBuilderAppendTooWide", "This builds a {0}-component value, but a material graph carries at most four components."),
				FText::AsNumber(Total)));
			return Parts[0];
		}

		// Every part a literal: one ConstantNVector, not N constants and N-1 appends. The 1.x
		// generator folds the same case while it builds (CodeConstructors.cpp) and the corpus
		// goldens were taken from it, so folding here rather than in the pass keeps them equal even
		// when the caller turns constant folding off.
		bool bAllConstant = true;
		double Folded[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 FoldedNum = 0;
		for (const FIRValue& Part : Parts)
		{
			const FIRNode* Node = (Graph && Graph->Nodes.IsValidIndex(Part.Node)) ? &Graph->Nodes[Part.Node] : nullptr;
			const FIRProperty* Constant = (Node && Node->Op == EIROp::Constant) ? Node->FindProperty(Prop::Value) : nullptr;
			if (!Constant || Part.Output != 0)
			{
				bAllConstant = false;
				break;
			}
			for (int32 Index = 0; Index < Constant->Value.N && FoldedNum < 4; ++Index)
			{
				Folded[FoldedNum++] = Constant->Value.V[Index];
			}
		}
		if (bAllConstant && FoldedNum == Total)
		{
			return MakeConstant(Folded, FoldedNum, Span);
		}

		FIRValue Current = Parts[0];
		for (int32 Index = 1; Index < Parts.Num(); ++Index)
		{
			const int32 Width = WidthOf(Current) + WidthOf(Parts[Index]);
			Current = MakeCoreOp(EIROp::Append, { Current, Parts[Index] }, FIRType::Float(FMath::Clamp(Width, 1, 4)), Span);
		}
		return Current;
	}

	FIRValue FIRBuilder::MakeBroadcast(FIRValue Value, int32 Width, const FLangSpan& Span)
	{
		if (!Value.IsValid() || Width <= 1 || WidthOf(Value) != 1)
		{
			return Value;
		}
		return MakeCoreOp(EIROp::Broadcast, { Value }, FIRType::Float(FMath::Clamp(Width, 1, 4)), Span);
	}

	FIRValue FIRBuilder::ApplyConversion(FIRValue Value, EIRConversion Conversion, int32 TargetWidth, const FLangSpan& Span)
	{
		if (!Value.IsValid())
		{
			return Value;
		}

		switch (Conversion)
		{
		case EIRConversion::Broadcast:
			// Only where the destination's width is fixed and known. A core math op broadcasts its
			// own scalar operands -- `float3 * float` is one Multiply in 1.x too -- so the callers
			// that have no fixed width pass -1 and get no node.
			return TargetWidth > 1 ? MakeBroadcast(Value, TargetWidth, Span) : Value;

		case EIRConversion::Numeric:
			// int/uint/half/double are already float1..4 in the graph; only a bool needs a node to
			// become a number, and only then does Convert survive to the emitter.
			if (TypeOfValue(Value).IsBool())
			{
				return MakeCoreOp(EIROp::Convert, { Value }, FIRType::Float(WidthOf(Value)), Span);
			}
			return Value;

		case EIRConversion::DefaultOutput:
		{
			// A Node value is already (node, 0); selecting its default output is a retype, not a node --
			// unless the place wants the whole value the node's channel views are cut from (FIX3-Binder
			// rule 2: `float4 VC = UE.VertexColor();`), which output 0 alone is too narrow to be.
			const FIRValue OutputZero{ Value.Node, 0 };
			if (TargetWidth > WidthOf(OutputZero))
			{
				const FIRValue Whole = WidenChannelView(OutputZero, TargetWidth, Span);
				if (Whole.IsValid())
				{
					return Whole;
				}
			}
			return OutputZero;
		}

		case EIRConversion::Identity:
		case EIRConversion::None:
		default:
			return Value;
		}
	}

	bool FIRBuilder::IsStaticCondition(FIRValue Value) const
	{
		if (!Graph || !Graph->Nodes.IsValidIndex(Value.Node))
		{
			return false;
		}

		const FIRNode& Node = Graph->Nodes[Value.Node];
		switch (Node.Op)
		{
		case EIROp::Parameter:
		{
			const FIRProperty* Static = Node.FindProperty(Prop::IsStatic);
			return Static != nullptr && Static->Value.B;
		}
		case EIROp::LogicalNot:
		case EIROp::LogicalAnd:
		case EIROp::LogicalOr:
			if (Node.Operands.IsEmpty())
			{
				return false;
			}
			for (const FIRValue& Operand : Node.Operands)
			{
				if (!IsStaticCondition(Operand))
				{
					return false;
				}
			}
			return true;
		default:
			return false;
		}
	}

	bool FIRBuilder::IsCompileTimeConstant(FIRValue Value) const
	{
		if (!Graph || !Graph->Nodes.IsValidIndex(Value.Node))
		{
			return false;
		}

		// Walked with a visited set rather than recursively: an unrolled loop makes a chain as long as its
		// trip count, and a value read twice would otherwise be walked twice at every level.
		TArray<int32> Pending;
		TSet<int32> Visited;
		Pending.Add(Value.Node);
		while (!Pending.IsEmpty())
		{
			const int32 Index = Pending.Pop();
			bool bAlreadyVisited = false;
			Visited.Add(Index, &bAlreadyVisited);
			if (bAlreadyVisited)
			{
				continue;
			}
			if (!Graph->Nodes.IsValidIndex(Index))
			{
				return false;
			}
			const FIRNode& Node = Graph->Nodes[Index];
			if (Node.Op == EIROp::Constant)
			{
				continue;
			}
			// What the fold pass folds: a core math op, a Swizzle or an Append, over constants only.
			const bool bFoldable = Node.Op == EIROp::Swizzle || Node.Op == EIROp::Append || IsCoreMathOp(Node.Op);
			if (!bFoldable || Node.Operands.IsEmpty() || !Node.Inputs.IsEmpty())
			{
				return false;
			}
			for (const FIRValue& Operand : Node.Operands)
			{
				if (!Operand.IsValid())
				{
					return false;
				}
				Pending.Add(Operand.Node);
			}
		}
		return true;
	}

	bool FIRBuilder::TryEvaluateConstant(FIRValue Value, double& OutValue, int32& Budget) const
	{
		if (--Budget < 0 || !Graph || !Graph->Nodes.IsValidIndex(Value.Node))
		{
			return false;
		}

		const FIRNode& Node = Graph->Nodes[Value.Node];
		if (Node.Op == EIROp::Constant)
		{
			const FIRProperty* Constant = Node.FindProperty(Prop::Value);
			if (!Constant || Value.Output != 0 || Constant->Value.N < 1)
			{
				return false;
			}
			OutValue = Constant->Value.V[0];
			return true;
		}

		if (Node.Op == EIROp::Swizzle)
		{
			// `v.y > 0`: the first mask letter picks a component of a constant operand.
			const FIRProperty* Mask = Node.FindProperty(Prop::Mask);
			const FIRNode* Operand = (Node.Operands.Num() == 1 && Graph->Nodes.IsValidIndex(Node.Operands[0].Node))
				? &Graph->Nodes[Node.Operands[0].Node]
				: nullptr;
			const FIRProperty* Constant = (Operand && Operand->Op == EIROp::Constant) ? Operand->FindProperty(Prop::Value) : nullptr;
			const int32 Component = (Mask && !Mask->Value.S.IsEmpty()) ? ComponentIndexOf(Mask->Value.S[0]) : INDEX_NONE;
			if (!Constant || Component == INDEX_NONE || Node.Operands[0].Output != 0)
			{
				return false;
			}
			OutValue = Constant->Value.N <= 1 ? Constant->Value.V[0] : Constant->Value.V[FMath::Min(Component, 3)];
			return true;
		}

		const int32 Arity = Node.Operands.Num();
		double A = 0.0;
		double B = 0.0;
		if (Arity < 1 || Arity > 2 || !Node.Inputs.IsEmpty()
			|| !TryEvaluateConstant(Node.Operands[0], A, Budget)
			|| (Arity == 2 && !TryEvaluateConstant(Node.Operands[1], B, Budget)))
		{
			return false;
		}

		switch (Node.Op)
		{
		case EIROp::Convert:
		case EIROp::Broadcast:    OutValue = A; return Arity == 1;
		case EIROp::Negate:       OutValue = -A; return Arity == 1;
		case EIROp::LogicalNot:   OutValue = (A == 0.0) ? 1.0 : 0.0; return Arity == 1;
		case EIROp::Add:          OutValue = A + B; return Arity == 2;
		case EIROp::Subtract:     OutValue = A - B; return Arity == 2;
		case EIROp::Multiply:     OutValue = A * B; return Arity == 2;
		case EIROp::Divide:       OutValue = (B != 0.0) ? A / B : 0.0; return Arity == 2 && B != 0.0;
		case EIROp::Less:         OutValue = (A < B) ? 1.0 : 0.0; return Arity == 2;
		case EIROp::LessEqual:    OutValue = (A <= B) ? 1.0 : 0.0; return Arity == 2;
		case EIROp::Greater:      OutValue = (A > B) ? 1.0 : 0.0; return Arity == 2;
		case EIROp::GreaterEqual: OutValue = (A >= B) ? 1.0 : 0.0; return Arity == 2;
		case EIROp::Equal:        OutValue = (A == B) ? 1.0 : 0.0; return Arity == 2;
		case EIROp::NotEqual:     OutValue = (A != B) ? 1.0 : 0.0; return Arity == 2;
		case EIROp::LogicalAnd:   OutValue = (A != 0.0 && B != 0.0) ? 1.0 : 0.0; return Arity == 2;
		case EIROp::LogicalOr:    OutValue = (A != 0.0 || B != 0.0) ? 1.0 : 0.0; return Arity == 2;
		default:
			return false;
		}
	}

	bool FIRBuilder::TryDecideCondition(FIRValue Condition, bool& bOutHolds) const
	{
		double Result = 0.0;
		int32 Budget = 1024;
		if (!Condition.IsValid() || !IsCompileTimeConstant(Condition) || !TryEvaluateConstant(Condition, Result, Budget))
		{
			return false;
		}
		bOutHolds = Result != 0.0;
		return true;
	}

	bool FIRBuilder::IsInArmThatMayNotRun() const
	{
		for (const FConditionEntry& Entry : ConditionStack)
		{
			if (!Entry.Value.IsValid() || !IsCompileTimeConstant(Entry.Value))
			{
				// A condition the shader decides at run time: either arm may run.
				continue;
			}
			double Result = 0.0;
			int32 Budget = 1024;
			if (!TryEvaluateConstant(Entry.Value, Result, Budget))
			{
				// Constant, but not something this can evaluate: this may be the arm that never runs.
				return true;
			}
			// An `else` arm runs when the condition does not hold.
			if ((Result != 0.0) == Entry.bNegated)
			{
				return true;
			}
		}
		return false;
	}

	FIRValue FIRBuilder::MakeConditional(
		FIRValue Condition,
		bool bStaticCondition,
		FIRValue TrueValue,
		FIRValue FalseValue,
		const FLangSpan& Span)
	{
		if (TrueValue == FalseValue)
		{
			return TrueValue;
		}
		// A condition that failed to lower (its own diagnostic is already out) must not become a
		// Select with an unset operand 0: nothing but TextureSample may leave an operand empty, so
		// the validator would answer a reported error with an internal-sounding DSH4303 on top.
		if (!Condition.IsValid())
		{
			return TrueValue.IsValid() ? TrueValue : FalseValue;
		}
		if (!TrueValue.IsValid())
		{
			return FalseValue;
		}
		if (!FalseValue.IsValid())
		{
			return TrueValue;
		}
		// Select, Compare and StaticSwitch are typed by width just below, so over two whole materials they
		// would come out float1: a graph that validates and means nothing. MergeValues names the variable
		// before it ever gets here; this is the backstop for any other way in.
		if (TypeOfValue(TrueValue).IsMaterial() || TypeOfValue(FalseValue).IsMaterial())
		{
			ReportWholeMaterialMerge(FString(), Span);
			return TrueValue;
		}

		const int32 Width = FMath::Max(WidthOf(TrueValue), WidthOf(FalseValue));
		const FIRType Result = FIRType::Float(Width);

		if (bStaticCondition && Condition.IsValid())
		{
			return MakeCoreOp(EIROp::StaticSwitch, { Condition, TrueValue, FalseValue }, Result, Span);
		}

		// A scalar comparison as the condition becomes the engine If directly: Compare's five
		// operands are exactly UMaterialExpressionIf's A, B and its three branches, which is the
		// node the 1.x generator emits for `a > b ? x : y` (CodeExpressions.cpp,
		// CreateConditionalValue). The comparison node itself then feeds nothing and the prune pass
		// takes it away.
		if (Graph && Graph->Nodes.IsValidIndex(Condition.Node))
		{
			const FIRNode& ConditionNode = Graph->Nodes[Condition.Node];
			FIRValue Greater = FIRValue::None();
			FIRValue Equal = FIRValue::None();
			FIRValue Less = FIRValue::None();
			bool bIsComparison = true;
			switch (ConditionNode.Op)
			{
			case EIROp::Greater:      Greater = TrueValue;  Equal = FalseValue; Less = FalseValue; break;
			case EIROp::GreaterEqual: Greater = TrueValue;  Equal = TrueValue;  Less = FalseValue; break;
			case EIROp::Less:         Greater = FalseValue; Equal = FalseValue; Less = TrueValue;  break;
			case EIROp::LessEqual:    Greater = FalseValue; Equal = TrueValue;  Less = TrueValue;  break;
			case EIROp::Equal:        Greater = FalseValue; Equal = TrueValue;  Less = FalseValue; break;
			case EIROp::NotEqual:     Greater = TrueValue;  Equal = FalseValue; Less = TrueValue;  break;
			default: bIsComparison = false; break;
			}

			if (bIsComparison && ConditionNode.Operands.Num() == 2)
			{
				const FIRValue Left = ConditionNode.Operands[0];
				const FIRValue Right = ConditionNode.Operands[1];
				// The engine If compares scalars only; a vector comparison stays a Select.
				if (WidthOf(Left) == 1 && WidthOf(Right) == 1)
				{
					return MakeCoreOp(EIROp::Compare, { Left, Right, Greater, Equal, Less }, Result, Span);
				}
			}
		}

		return MakeCoreOp(EIROp::Select, { Condition, TrueValue, FalseValue }, Result, Span);
	}

	void FIRBuilder::SetDebugName(int32 FirstNode, const FString& Name)
	{
		if (!Options.bKeepDebugNames || Name.IsEmpty() || !Graph)
		{
			return;
		}
		// "The variable a value was first assigned to": only the nodes this statement made, and only
		// the ones with no name yet, so a second assignment does not rename a shared sub-expression.
		for (int32 Index = FirstNode; Index < Graph->Nodes.Num(); ++Index)
		{
			if (Graph->Nodes[Index].DebugName.IsEmpty())
			{
				Graph->Nodes[Index].DebugName = Name;
			}
		}
	}

	const FCatalogMaterialAttribute* FIRBuilder::Attribute(int32 Index) const
	{
		return (Catalog && Catalog->MaterialAttributes.IsValidIndex(Index)) ? &Catalog->MaterialAttributes[Index] : nullptr;
	}
}

namespace UE::DreamShader::IR
{
	TUniquePtr<FIRModule> BuildDreamShaderIR(
		const Lang::FBoundModule& Bound,
		const FIRBuildOptions& Options,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		if (!Bound.Module)
		{
			return nullptr;
		}

		TUniquePtr<FIRModule> Module = MakeUnique<FIRModule>();
		Private::FIRBuilder Builder(Bound, Options, Diagnostics);
		Builder.BuildModule(*Module);
		return Module;
	}
}

#undef LOCTEXT_NAMESPACE
