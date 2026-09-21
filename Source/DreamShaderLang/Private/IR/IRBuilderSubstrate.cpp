// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The IR builder's Substrate sugar (Docs/language-v2/substrate.md).
//
// What the binder bound as a sugar arrives here as an ordinary reflected call, so most of a sugar costs the builder
// nothing. What is left is the part that makes nodes nobody wrote:
//
//   S3  the conversion node behind `Substrate.Slab(BaseColor = ..., Metallic = ...)` and its three siblings
//       (ExpandSubstrateVirtualArguments). The nodes are plain reflected nodes, so two slabs that convert the same
//       BaseColor and Metallic share one conversion node: the dedupe pass sees two equal keys, as it would for any
//       two equal calls.
//   S2  the `Substrate.Select` a run-time branch becomes lives with the other branches, in MakeConditional
//       (IRBuilder.cpp); it uses MakeReflectedNode, as everything here does.
//   S4  `#pragma material(Substrate = Bridge)`: in a Substrate project the shading attributes of the sink move into
//       one `Substrate.ShadingModels` node on FrontMaterial (FoldSinkIntoSubstrate). `Substrate = Native` with Substrate
//       off is refused here too, because only here is it known that the material drives FrontMaterial.
//   S5  `Substrate S = Substrate.Slab();  S.Roughness = r;`: the declaration makes a record (FSubstrateBuilderState) and
//       no node, a member write fills the record, and the first use of `S` makes the node through LowerReflectedCall --
//       the path `Substrate.Slab(Roughness = r)` takes, so the two spellings cannot come out as different graphs. The
//       node cannot be made at the declaration: a node is made after what it reads, and the passes stand on that.
//
// Every expansion is a fixed pattern, which is what lets the decompiler fold it back: a MetalnessToDiffuseAlbedoF0
// whose two outputs feed the DiffuseAlbedo and F0 of one slab and nothing else is `BaseColor / Metallic / Specular`.
// `IOR` is the one that is not folded back -- a constant F0 does not say it was an index of refraction.
//
// Diagnostics owned by this file: DSH4381, DSH4382, DSH4383.

#include "IRBuilderInternal.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.IRBuilder.Substrate"

namespace UE::DreamShader::IR::Private
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace IRBuilderSubstratePrivate
	{
		static const FIRValue* FindVirtual(const TArray<TPair<FString, FIRValue>>& Virtual, const TCHAR* Name)
		{
			for (const TPair<FString, FIRValue>& Pair : Virtual)
			{
				if (Pair.Key.Equals(Name, ESearchCase::CaseSensitive))
				{
					return &Pair.Value;
				}
			}
			return nullptr;
		}

		/**
		 * One row of the engine's own conversion of a legacy Surface material (UMaterial's legacy-to-Substrate
		 * conversion, Material.cpp): which attribute goes to which pin of the ShadingModels node, and whether the engine
		 * leaves it on the material as well -- it copies Normal and Opacity and moves the rest. Keyed by the
		 * EMaterialProperty name, which is what the catalog records for an attribute whatever it is called: ClearCoat is
		 * MP_CustomData0 in every engine. What has no row stays on the material: WorldPositionOffset, OpacityMask,
		 * PixelDepthOffset, Refraction, AmbientOcclusion, Displacement, the customized UVs, SurfaceThickness.
		 */
		struct FBridgePin
		{
			const TCHAR* Property;
			const TCHAR* Pin;
			bool bCopy;
		};

		static const FBridgePin* FindBridgePin(const FString& PropertyName)
		{
			static const FBridgePin Pins[] =
			{
				{ TEXT("MP_BaseColor"),       TEXT("BaseColor"),          false },
				{ TEXT("MP_Metallic"),        TEXT("Metallic"),           false },
				{ TEXT("MP_Specular"),        TEXT("Specular"),           false },
				{ TEXT("MP_Roughness"),       TEXT("Roughness"),          false },
				{ TEXT("MP_Anisotropy"),      TEXT("Anisotropy"),         false },
				{ TEXT("MP_EmissiveColor"),   TEXT("EmissiveColor"),      false },
				{ TEXT("MP_Normal"),          TEXT("Normal"),             true },
				{ TEXT("MP_Tangent"),         TEXT("Tangent"),            false },
				{ TEXT("MP_SubsurfaceColor"), TEXT("SubSurfaceColor"),    false },
				{ TEXT("MP_CustomData0"),     TEXT("ClearCoat"),          false },
				{ TEXT("MP_CustomData1"),     TEXT("ClearCoatRoughness"), false },
				{ TEXT("MP_Opacity"),         TEXT("Opacity"),            true },
				{ TEXT("MP_ShadingModel"),    TEXT("ShadingModel"),       true },
			};
			for (const FBridgePin& Pin : Pins)
			{
				if (PropertyName.Equals(Pin.Property, ESearchCase::CaseSensitive))
				{
					return &Pin;
				}
			}
			return nullptr;
		}

		static void SetInput(FIRNode& Node, const TCHAR* Pin, const FIRValue Value)
		{
			for (FIRInput& Input : Node.Inputs)
			{
				if (Input.Pin.Equals(Pin, ESearchCase::CaseSensitive))
				{
					Input.Value = Value;
					return;
				}
			}
			Node.Inputs.Add({ FString(Pin), Value });
		}
	}

	ECatalogValueType SubstrateVirtualArgumentType(const FString& Name)
	{
		const bool bColour = Name.Equals(TEXT("BaseColor"), ESearchCase::CaseSensitive)
			|| Name.Equals(TEXT("Transmittance"), ESearchCase::CaseSensitive);
		return bColour ? ECatalogValueType::Float3 : ECatalogValueType::Float1;
	}

	// ------------------------------------------------------------------------------- sugar S5

	int32 FIRBuilder::BeginSubstrateBuilder(const FExpr& Call, const FString& Name, const FLangSpan& DeclSpan)
	{
		FSubstrateBuilderState State;
		State.Call = Unparen(&Call);
		State.Name = Name;
		State.Region = CurrentRegion;
		State.DeclSpan = DeclSpan;
		return SubstrateBuilders.Add(MoveTemp(State));
	}

	FIRValue FIRBuilder::MaterialiseSubstrateBuilder(const int32 BuilderId)
	{
		if (!SubstrateBuilders.IsValidIndex(BuilderId))
		{
			return FIRValue::None();
		}
		if (SubstrateBuilders[BuilderId].bTaken)
		{
			return SubstrateBuilders[BuilderId].Node;
		}
		SubstrateBuilders[BuilderId].bTaken = true;

		// A copy: lowering may begin another builder, and the array would move under a reference.
		const FSubstrateBuilderState State = SubstrateBuilders[BuilderId];
		const FBoundExpr* BoundCall = State.Call ? Bound(*State.Call) : nullptr;
		if (!BoundCall || BoundCall->Kind != EBoundExprKind::ReflectedCall)
		{
			// The binder marked the local a builder because this call bound as a node.
			return FIRValue::None();
		}

		const int32 FirstNode = Graph ? Graph->Nodes.Num() : 0;
		FLoweredValue Made;
		{
			// The node stands in the box its declaration stood in, wherever the value is first taken.
			TGuardValue<int32> RegionGuard(CurrentRegion, State.Region);
			Made = LowerReflectedCall(*State.Call, *BoundCall, &State);
		}
		SetDebugName(FirstNode, State.Name);

		const FIRValue Node = Made.IsValue() ? Made.Value : FIRValue::None();
		SubstrateBuilders[BuilderId].Node = Node;
		// The declaration bound this name, and only now is there a value to say it holds: the
		// layout's band for that line, and a probe on it, find the node through this.
		RecordStatementBinding(State.Name, Node, State.DeclSpan);
		return Node;
	}

	FLoweredValue FIRBuilder::TakeSlotValue(const FLoweredValue& Slot)
	{
		if (!Slot.IsBuilder())
		{
			return Slot;
		}
		const FIRValue Node = MaterialiseSubstrateBuilder(Slot.BuilderId);
		// Nothing, in silence, when making the node failed: whatever failed has said so.
		FLoweredValue Result = Node.IsValid() ? FLoweredValue::Of(Node) : FLoweredValue();
		Result.bPartiallyAssigned = Slot.bPartiallyAssigned;
		Result.PartialSpan = Slot.PartialSpan;
		Result.PartialName = Slot.PartialName;
		return Result;
	}

	int32 FIRBuilder::FindSubstrateBuilder(const FBoundExpr& PinBound, const FExpr& Site)
	{
		const FFrame& Current = Frame();
		if (Current.Locals.IsValidIndex(PinBound.LocalSlot))
		{
			const FLoweredValue& Slot = Current.Locals[PinBound.LocalSlot];
			if (Slot.IsBuilder() && SubstrateBuilders.IsValidIndex(Slot.BuilderId))
			{
				return Slot.BuilderId;
			}
		}

		// The binder reads a body once, top to bottom, and refuses a member after `S = T`. What it cannot see is a loop
		// coming round: the second trip meets the member after the first trip's assignment.
		const FString Name = (Current.Function && Current.Function->Locals.IsValidIndex(PinBound.LocalSlot))
			? Current.Function->Locals[PinBound.LocalSlot].Name
			: FString();
		Diagnostics.Error(TEXT("DSH4383"), Site.Span, FText::Format(
			LOCTEXT("IRBuilderSubstrateBuilderGone", "'{0}' is not the Substrate value being built any more when a loop comes round to '{0}.{1}': an earlier trip assigned it. Declare the value inside the loop, or finish it before the loop."),
			FText::FromString(Name),
			FText::FromString(PinBound.BuilderPin)));
		return INDEX_NONE;
	}

	FIRValue FIRBuilder::ValueOfSubstrateBuilderMember(const int32 BuilderId, const FString& Member, const FLangSpan& Span)
	{
		using namespace IRBuilderSubstratePrivate;

		if (!SubstrateBuilders.IsValidIndex(BuilderId))
		{
			return FIRValue::None();
		}
		for (const FIRInput& Input : SubstrateBuilders[BuilderId].Inputs)
		{
			if (Input.Pin.Equals(Member, ESearchCase::CaseSensitive))
			{
				return Input.Value;
			}
		}
		if (const FIRValue* Virtual = FindVirtual(SubstrateBuilders[BuilderId].Virtual, *Member))
		{
			return *Virtual;
		}
		if (Member.Equals(TEXT("IOR"), ESearchCase::CaseSensitive) && SubstrateBuilders[BuilderId].IorConstant.IsSet())
		{
			// An index that was a number is kept as the number (it folds into F0); read back, it is a constant.
			const double Index = SubstrateBuilders[BuilderId].IorConstant.GetValue();
			return MakeScalarConstant(Index, Span);
		}
		return FIRValue::None();
	}

	FLoweredValue FIRBuilder::LowerSubstrateBuilderRead(const FExpr& Expr, const FBoundExpr& BoundExpr)
	{
		const int32 BuilderId = FindSubstrateBuilder(BoundExpr, Expr);
		if (BuilderId == INDEX_NONE)
		{
			return FLoweredValue();
		}
		const FIRValue Value = ValueOfSubstrateBuilderMember(BuilderId, BoundExpr.BuilderPin, Expr.Span);
		if (!Value.IsValid())
		{
			// The binder saw a write above this read. It did not run: a loop of no trips, an arm the compiler dropped.
			Diagnostics.Error(TEXT("DSH4383"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderSubstrateBuilderUnset", "'{0}.{1}' has no value where it is read: the line that writes it did not run on the way here. Give it a value on every path first."),
				FText::FromString(SubstrateBuilders[BuilderId].Name),
				FText::FromString(BoundExpr.BuilderPin)));
			return FLoweredValue();
		}
		return FLoweredValue::OfOutput(Value, BoundExpr.Type);
	}

	FLoweredValue FIRBuilder::LowerSubstrateBuilderWrite(
		const FExpr& Expr,
		const FBoundExpr& BoundExpr,
		const FBoundExpr& TargetBound,
		const FExpr* ValueExpr,
		const EIROp CompoundOp,
		const bool bValueIsPrevious)
	{
		using namespace IRBuilderSubstratePrivate;

		const int32 BuilderId = FindSubstrateBuilder(TargetBound, Expr);
		if (BuilderId == INDEX_NONE || !Catalog)
		{
			return FLoweredValue();
		}
		const FString Member = TargetBound.BuilderPin;

		if (SubstrateBuilders[BuilderId].bTaken)
		{
			// Sealed, as the binder says of a write below a use (DSH5297) -- but this use is above the write only
			// because a loop came round.
			Diagnostics.Error(TEXT("DSH4383"), Expr.Span, FText::Format(
				LOCTEXT("IRBuilderSubstrateBuilderTaken", "The Substrate value '{0}' has been used by the time a loop comes round to this write of '{0}.{1}', and its node is what it was then. Declare the value inside the loop, or finish it before the loop."),
				FText::FromString(SubstrateBuilders[BuilderId].Name),
				FText::FromString(Member)));
			return FLoweredValue();
		}

		// A pin of the node, or an argument of sugar S3 (FieldIndex is INDEX_NONE for one).
		const bool bVirtual = TargetBound.FieldIndex == INDEX_NONE;
		ECatalogValueType PinType = SubstrateVirtualArgumentType(Member);
		if (!bVirtual)
		{
			if (!Catalog->Expressions.IsValidIndex(TargetBound.Index) || !Catalog->Expressions[TargetBound.Index].Inputs.IsValidIndex(TargetBound.FieldIndex))
			{
				// Another catalog than the one the module was bound against; the node itself says DSH4352 when it is made.
				return FLoweredValue();
			}
			PinType = Catalog->Expressions[TargetBound.Index].Inputs[TargetBound.FieldIndex].Type;
		}

		// `S.Pin += x` and `++S.Pin` start from what the member holds.
		FIRValue Previous = FIRValue::None();
		if (CompoundOp != EIROp::Count)
		{
			Previous = ValueOfSubstrateBuilderMember(BuilderId, Member, Expr.Span);
			if (!Previous.IsValid())
			{
				Diagnostics.Error(TEXT("DSH4383"), Expr.Span, FText::Format(
					LOCTEXT("IRBuilderSubstrateBuilderUnsetWrite", "'{0}.{1}' has no value for this operator to start from: the line that writes it did not run on the way here. Give it a value on every path first."),
					FText::FromString(SubstrateBuilders[BuilderId].Name),
					FText::FromString(Member)));
				return FLoweredValue();
			}
		}

		FIRValue NewValue = FIRValue::None();
		TOptional<double> NewIorConstant;
		if (!ValueExpr)
		{
			const FIRValue One = MakeScalarConstant(1.0, Expr.Span);
			NewValue = MakeCoreOp(CompoundOp, { Previous, One }, GraphTypeOf(BoundExpr.Type), Expr.Span);
		}
		else
		{
			const FExpr* InnerValue = Unparen(ValueExpr);
			const FBoundExpr* BoundInner = InnerValue ? Bound(*InnerValue) : nullptr;
			if (bVirtual && CompoundOp == EIROp::Count && BoundInner && BoundInner->bIsConstant
				&& Member.Equals(TEXT("IOR"), ESearchCase::CaseSensitive))
			{
				// An index that is a number folds into F0 when the node is made, as it does in the call.
				NewIorConstant = BoundInner->ConstantValue[0];
			}
			else
			{
				const FBoundExpr* BoundValue = Bound(*ValueExpr);
				const FLoweredValue Lowered = LowerExpr(*ValueExpr);
				if (CompoundOp != EIROp::Count)
				{
					if (!Lowered.IsValue())
					{
						return FLoweredValue();
					}
					const FIRValue Combined = MakeCoreOp(CompoundOp, { Previous, Lowered.Value }, GraphTypeOf(BoundExpr.Type), Expr.Span);
					NewValue = ValueForPin(FLoweredValue::Of(Combined), PinType, EIRConversion::Identity, Expr.Span);
				}
				else
				{
					NewValue = ValueForPin(Lowered, PinType, BoundValue ? BoundValue->Conversion : EIRConversion::Identity, ValueExpr->Span);
				}
			}
		}
		if (!NewValue.IsValid() && !NewIorConstant.IsSet())
		{
			// What failed to lower has said why.
			return FLoweredValue();
		}

		// Lowering the value may have begun another builder; the array is indexed again, never held across it.
		FSubstrateBuilderState& State = SubstrateBuilders[BuilderId];
		if (!bVirtual)
		{
			bool bReplaced = false;
			for (FIRInput& Input : State.Inputs)
			{
				if (Input.Pin.Equals(Member, ESearchCase::CaseSensitive))
				{
					Input.Value = NewValue;
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				State.Inputs.Add({ Member, NewValue });
			}
		}
		else
		{
			State.Virtual.RemoveAll([&Member](const TPair<FString, FIRValue>& Pair)
			{
				return Pair.Key.Equals(Member, ESearchCase::CaseSensitive);
			});
			if (Member.Equals(TEXT("IOR"), ESearchCase::CaseSensitive))
			{
				State.IorConstant = NewIorConstant;
			}
			if (NewValue.IsValid())
			{
				State.Virtual.Add(TPair<FString, FIRValue>(Member, NewValue));
			}
		}

		if (NewIorConstant.IsSet())
		{
			// The assignment as a value (`x = S.IOR = 1.5`): the number, which nothing else had to make a node for.
			return FLoweredValue::Of(MakeScalarConstant(NewIorConstant.GetValue(), Expr.Span));
		}
		return (bValueIsPrevious && Previous.IsValid()) ? FLoweredValue::Of(Previous) : FLoweredValue::OfOutput(NewValue, BoundExpr.Type);
	}

	bool FIRBuilder::FoldSinkIntoSubstrate(FIRNode& Sink, const FLangSpan& Span)
	{
		using namespace IRBuilderSubstratePrivate;

		if (!CurrentBoundProduct || !Catalog)
		{
			return false;
		}

		const bool bDrivesFrontMaterial = Sink.Inputs.ContainsByPredicate([this](const FIRInput& Input)
		{
			const int32 Attribute = Catalog->FindMaterialAttribute(Input.Pin);
			return Attribute != INDEX_NONE && Catalog->MaterialAttributes[Attribute].Name.Equals(TEXT("FrontMaterial"), ESearchCase::CaseSensitive);
		});

		if (CurrentBoundProduct->SubstrateMode == EIRSubstrateMode::Native)
		{
			if (bDrivesFrontMaterial && !Options.bSubstrateEnabled)
			{
				Diagnostics.Error(TEXT("DSH4382"), Span, LOCTEXT("IRBuilderSubstrateNativeOff",
					"This material drives FrontMaterial and says 'Substrate = Native', and Substrate is off in this project; the engine could not compile the asset. Turn Substrate on, or write 'Substrate = Bridge' and the legacy attributes."));
			}
			return false;
		}

		// Bridge, in a Substrate project, on a material that does not drive FrontMaterial already.
		if (CurrentBoundProduct->SubstrateMode != EIRSubstrateMode::Bridge || !Options.bSubstrateEnabled || bDrivesFrontMaterial)
		{
			return false;
		}

		// Surface materials only. The engine's own conversion picks another node per domain (Volume, LightFunction,
		// PostProcess, UI, DeferredDecal); those stay as written and are converted by the engine, as under Legacy.
		if (const FString* Domain = CurrentBoundProduct->Settings.Find(FString(TEXT("Domain"))))
		{
			const FString Trimmed = Domain->TrimStartAndEnd().TrimQuotes();
			if (!Trimmed.IsEmpty() && !Trimmed.Equals(TEXT("Surface"), ESearchCase::IgnoreCase) && !Trimmed.Equals(TEXT("MD_Surface"), ESearchCase::IgnoreCase))
			{
				return false;
			}
		}

		const int32 ShadingModelsIndex = Catalog->FindExpression(TEXT("Substrate"), TEXT("ShadingModels"));
		if (!Catalog->Expressions.IsValidIndex(ShadingModelsIndex))
		{
			// A Substrate project on an engine without the node cannot exist; a catalog without it is a test catalog.
			return false;
		}
		const FCatalogExpression& ShadingModels = Catalog->Expressions[ShadingModelsIndex];

		TArray<FIRInput> Folded;
		TArray<FIRInput> Kept;
		for (const FIRInput& Input : Sink.Inputs)
		{
			const int32 Attribute = Catalog->FindMaterialAttribute(Input.Pin);
			const FBridgePin* Bridge = Attribute != INDEX_NONE ? FindBridgePin(Catalog->MaterialAttributes[Attribute].PropertyName) : nullptr;
			// A pin this engine's node does not have (an older ShadingModels) leaves the attribute where it was written.
			const int32 Pin = Bridge ? ShadingModels.FindInput(Bridge->Pin) : INDEX_NONE;
			if (Pin != INDEX_NONE)
			{
				Folded.Add({ ShadingModels.Inputs[Pin].Name, Input.Value });
			}
			if (Pin == INDEX_NONE || Bridge->bCopy)
			{
				Kept.Add(Input);
			}
		}
		// Only copies, or nothing at all: a material that shades nothing has nothing to bridge.
		const bool bAnyMoved = Folded.Num() > 0 && Kept.Num() < Sink.Inputs.Num();
		if (!bAnyMoved)
		{
			return false;
		}

		const FIRValue Front = MakeReflectedNode(TEXT("Substrate"), TEXT("ShadingModels"), MoveTemp(Folded), Span);
		if (!Front.IsValid() || !Graph)
		{
			return false;
		}

		// The material's shading model, which the legacy pins took from the material and the node has to be told. A
		// material that computes its shading model says so through the ShadingModel pin (a row of the table above) and
		// leaves the override at the node's default, as the engine's conversion does.
		if (const FString* ShadingModel = CurrentBoundProduct->Settings.Find(FString(TEXT("ShadingModel"))))
		{
			const FString Trimmed = ShadingModel->TrimStartAndEnd().TrimQuotes();
			const bool bFromExpression = Trimmed.Equals(TEXT("FromMaterialExpression"), ESearchCase::IgnoreCase)
				|| Trimmed.Equals(TEXT("MSM_FromMaterialExpression"), ESearchCase::IgnoreCase);
			if (!Trimmed.IsEmpty() && !bFromExpression && ShadingModels.FindProperty(TEXT("ShadingModelOverride")) != INDEX_NONE)
			{
				Graph->Nodes[Front.Node].Properties.Add({ FString(TEXT("ShadingModelOverride")), FIRPropertyValue::MakeEnum(Trimmed) });
			}
		}

		const int32 FrontAttribute = Catalog->FindMaterialAttribute(TEXT("FrontMaterial"));
		Kept.Add({ FrontAttribute != INDEX_NONE ? Catalog->MaterialAttributes[FrontAttribute].Name : FString(TEXT("FrontMaterial")), Front });
		Sink.Inputs = MoveTemp(Kept);
		return true;
	}

	void FIRBuilder::ExpandSubstrateVirtualArguments(
		FIRNode& Node,
		const TArray<TPair<FString, FIRValue>>& Virtual,
		const TOptional<double>& IorConstant,
		const FLangSpan& Span)
	{
		using namespace IRBuilderSubstratePrivate;

		const auto ReportMissing = [this, &Span](const TCHAR* NodeName, const TCHAR* Argument)
		{
			Diagnostics.Error(TEXT("DSH4381"), Span, FText::Format(
				LOCTEXT("IRBuilderSubstrateConversionMissing", "'{0}' is converted by the node 'Substrate.{1}', and this engine has no such node; give the pins it feeds directly."),
				FText::FromString(Argument),
				FText::FromString(NodeName)));
		};

		// ----- BaseColor / Metallic / Specular -> DiffuseAlbedo, F0. A pin that is not given keeps the engine's default
		// (BaseColor 0.18, Specular 0.5, Metallic 0).
		const FIRValue* BaseColor = FindVirtual(Virtual, TEXT("BaseColor"));
		const FIRValue* Metallic = FindVirtual(Virtual, TEXT("Metallic"));
		const FIRValue* Specular = FindVirtual(Virtual, TEXT("Specular"));
		if (BaseColor || Metallic || Specular)
		{
			TArray<FIRInput> Inputs;
			if (BaseColor) { Inputs.Add({ FString(TEXT("BaseColor")), *BaseColor }); }
			if (Metallic) { Inputs.Add({ FString(TEXT("Metallic")), *Metallic }); }
			if (Specular) { Inputs.Add({ FString(TEXT("Specular")), *Specular }); }
			const FIRValue Conversion = MakeReflectedNode(TEXT("Substrate"), TEXT("MetalnessToDiffuseAlbedoF0"), MoveTemp(Inputs), Span);
			if (Conversion.IsValid())
			{
				SetInput(Node, TEXT("DiffuseAlbedo"), FIRValue{ Conversion.Node, 0 });
				SetInput(Node, TEXT("F0"), FIRValue{ Conversion.Node, 1 });
			}
			else
			{
				ReportMissing(TEXT("MetalnessToDiffuseAlbedoF0"), BaseColor ? TEXT("BaseColor") : Metallic ? TEXT("Metallic") : TEXT("Specular"));
			}
		}

		// ----- Haziness, against the call's own Roughness -> SecondRoughness, SecondRoughnessWeight
		if (const FIRValue* Haziness = FindVirtual(Virtual, TEXT("Haziness")))
		{
			TArray<FIRInput> Inputs;
			if (const FIRInput* Roughness = Node.FindInput(FString(TEXT("Roughness"))))
			{
				Inputs.Add({ FString(TEXT("BaseRoughness")), Roughness->Value });
			}
			Inputs.Add({ FString(TEXT("Haziness")), *Haziness });
			const FIRValue Conversion = MakeReflectedNode(TEXT("Substrate"), TEXT("HazinessToSecondaryRoughness"), MoveTemp(Inputs), Span);
			if (Conversion.IsValid())
			{
				SetInput(Node, TEXT("SecondRoughness"), FIRValue{ Conversion.Node, 0 });
				SetInput(Node, TEXT("SecondRoughnessWeight"), FIRValue{ Conversion.Node, 1 });
			}
			else
			{
				ReportMissing(TEXT("HazinessToSecondaryRoughness"), TEXT("Haziness"));
			}
		}

		// ----- Transmittance [, Thickness] -> SSSMFP
		if (const FIRValue* Transmittance = FindVirtual(Virtual, TEXT("Transmittance")))
		{
			TArray<FIRInput> Inputs;
			Inputs.Add({ FString(TEXT("TransmittanceColor")), *Transmittance });
			if (const FIRValue* Thickness = FindVirtual(Virtual, TEXT("Thickness")))
			{
				Inputs.Add({ FString(TEXT("Thickness")), *Thickness });
			}
			const FIRValue Conversion = MakeReflectedNode(TEXT("Substrate"), TEXT("TransmittanceToMFP"), MoveTemp(Inputs), Span);
			if (Conversion.IsValid())
			{
				SetInput(Node, TEXT("SSSMFP"), FIRValue{ Conversion.Node, 0 });
			}
			else
			{
				ReportMissing(TEXT("TransmittanceToMFP"), TEXT("Transmittance"));
			}
		}

		// ----- IOR -> F0 = ((n - 1) / (n + 1))^2: a constant when the index is one, four nodes when it is computed
		if (IorConstant.IsSet())
		{
			const double Index = IorConstant.GetValue();
			const double Ratio = FMath::IsNearlyZero(Index + 1.0) ? 0.0 : (Index - 1.0) / (Index + 1.0);
			const double F0 = Ratio * Ratio;
			const double Components[3] = { F0, F0, F0 };
			SetInput(Node, TEXT("F0"), MakeConstant(Components, 3, Span));
		}
		else if (const FIRValue* Ior = FindVirtual(Virtual, TEXT("IOR")))
		{
			const FIRValue One = MakeScalarConstant(1.0, Span);
			const FIRValue Less = MakeCoreOp(EIROp::Subtract, { *Ior, One }, FIRType::Float(1), Span);
			const FIRValue More = MakeCoreOp(EIROp::Add, { *Ior, One }, FIRType::Float(1), Span);
			const FIRValue Ratio = MakeCoreOp(EIROp::Divide, { Less, More }, FIRType::Float(1), Span);
			const FIRValue Squared = MakeCoreOp(EIROp::Multiply, { Ratio, Ratio }, FIRType::Float(1), Span);
			SetInput(Node, TEXT("F0"), CoerceToWidth(Squared, 3, Span));
		}
	}
}

#undef LOCTEXT_NAMESPACE
