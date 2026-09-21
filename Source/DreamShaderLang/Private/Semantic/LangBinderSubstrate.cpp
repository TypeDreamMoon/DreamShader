// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The binder's Substrate sugar (Docs/language-v2/substrate.md).
//
// A sugar is a spelling and nothing else: each one binds to a fixed node pattern, the IR builder lowers the pattern
// through the path every reflected call takes, and both front ends get it because both arrive here.
//
//   S1  `A + B`            Substrate.Add(A, B)                              BindSubstrateBinary
//       `A * w`, `w * A`   Substrate.Weight(A, Weight = w)
//       `lerp(A, B, t)`    Substrate.HorizontalMix(Background, Foreground, Mix)   TryBindSubstrateLerp
//       The positional forms and the `Mix` / `Layer` names are catalog data (the reflection filler's tables), not code.
//   S3  `Substrate.Slab(BaseColor = ..., Metallic = ..., Haziness = ..., Transmittance = ..., IOR = ...)`
//       arguments that are no pin of the node: each family expands to a conversion node feeding the real pins.
//       TryBindSubstrateVirtualArgument, CheckSubstrateVirtualArguments
//   S5  `Substrate S = Substrate.Slab();  S.BaseColor = Albedo;  S.Metallic = 1.0;`
//       a local declared as a node call without arguments is a builder: until a value is taken from it, a member
//       write connects a pin (or a virtual argument of S3), in any order. Taking the value seals it; a member read is
//       whatever was connected and makes no node.                        NoteSubstrateBuilderDeclared, TryBindSubstrateBuilderMember
//       The IR builder makes the node when the value is first taken, not where it is declared: a node is made after
//       what it reads (IRPasses.cpp), and what a builder reads is written after its declaration. That is also why a
//       write under an `if` the declaration is outside of is refused -- the node would be one node in two versions --
//       and why a builder assigned whole (`S = T`, an `out` argument) has no members afterwards.
//   S7  a node this engine is too old for says which engine has it                  ReportMissingSubstrateNode
//
// An operator or `lerp` binds as EBoundExprKind::ReflectedCall on the operator's own AST node. Its FBoundArguments
// name operands by ordinal -- 0 and 1 are a binary expression's Left and Right, and a call's are its argument indices --
// which is the one thing FIRBuilder::ArgumentExpr had to learn.
//
// Parameter blending is the engine's default for every one of them (off). A source that wants it writes the named
// form, `Substrate.Mix(A, B, t, UseParameterBlending = true)`, and the decompiler writes that form back.
//
// Diagnostics owned by this file: DSH5293, DSH5294, DSH5295, DSH5296, DSH5297, DSH5298, DSH5299.

#include "LangBinderInternal.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder.Substrate"

namespace UE::DreamShader::Lang::Private
{
	namespace SubstrateSugar
	{
		/** A node that exists only from some engine version on: what to say when the catalog does not have it. */
		struct FVersionGate
		{
			const TCHAR* Name;
			const TCHAR* Engine;
		};

		static const FVersionGate VersionGates[] =
		{
			{ TEXT("Select"), TEXT("5.6") },
			{ TEXT("Toon"),   TEXT("5.8") },
		};

		/** One family of virtual arguments: the names that trigger it, the pins it feeds, the arguments it cannot share a call with. */
		struct FVirtualFamily
		{
			const TCHAR* Names[4];
			const TCHAR* FeedsPins[3];
			const TCHAR* ConflictsWith[5];
			/** A real pin the family reads as well: `Haziness` needs the call's own `Roughness`. */
			const TCHAR* RequiresPin;
		};

		static const FVirtualFamily VirtualFamilies[] =
		{
			// Substrate.MetalnessToDiffuseAlbedoF0(BaseColor, Metallic, Specular) -> DiffuseAlbedo, F0
			{ { TEXT("BaseColor"), TEXT("Metallic"), TEXT("Specular"), nullptr },
			  { TEXT("DiffuseAlbedo"), TEXT("F0"), nullptr },
			  { TEXT("DiffuseAlbedo"), TEXT("F0"), TEXT("IOR"), nullptr, nullptr },
			  nullptr },
			// Substrate.HazinessToSecondaryRoughness(BaseRoughness = <Roughness>, Haziness) -> SecondRoughness, SecondRoughnessWeight
			{ { TEXT("Haziness"), nullptr, nullptr, nullptr },
			  { TEXT("SecondRoughness"), TEXT("SecondRoughnessWeight"), nullptr },
			  { TEXT("SecondRoughness"), TEXT("SecondRoughnessWeight"), nullptr, nullptr, nullptr },
			  TEXT("Roughness") },
			// Substrate.TransmittanceToMFP(TransmittanceColor = Transmittance, Thickness) -> SSSMFP
			{ { TEXT("Transmittance"), TEXT("Thickness"), nullptr, nullptr },
			  { TEXT("SSSMFP"), nullptr, nullptr },
			  { TEXT("SSSMFP"), nullptr, nullptr, nullptr, nullptr },
			  nullptr },
			// F0 = ((IOR - 1) / (IOR + 1))^2
			{ { TEXT("IOR"), nullptr, nullptr, nullptr },
			  { TEXT("F0"), nullptr, nullptr },
			  { TEXT("F0"), TEXT("Specular"), TEXT("Metallic"), TEXT("BaseColor"), nullptr },
			  nullptr },
		};

		static const FVirtualFamily* FindFamily(const FString& ArgumentName)
		{
			for (const FVirtualFamily& Family : VirtualFamilies)
			{
				for (const TCHAR* Name : Family.Names)
				{
					if (Name && ArgumentName.Equals(Name, ESearchCase::CaseSensitive))
					{
						return &Family;
					}
				}
			}
			return nullptr;
		}

		/** A class takes a family when it has every pin the family feeds -- Slab all four, SimpleClearCoat the two that end in F0. */
		static bool ClassTakesFamily(const IR::FCatalogExpression& Class, const FVirtualFamily& Family)
		{
			if (!Class.Namespace.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive))
			{
				return false;
			}
			for (const TCHAR* Pin : Family.FeedsPins)
			{
				if (Pin && Class.FindInput(Pin) == INDEX_NONE)
				{
					return false;
				}
			}
			// A class that has the argument as a real pin (ShadingModels has BaseColor) never gets here: the pin wins.
			return true;
		}

		static IR::FIRType TypeOfVirtualArgument(const FString& ArgumentName)
		{
			return ArgumentName.Equals(TEXT("BaseColor"), ESearchCase::CaseSensitive) || ArgumentName.Equals(TEXT("Transmittance"), ESearchCase::CaseSensitive)
				? IR::FIRType::Float(3)
				: IR::FIRType::Float(1);
		}
	}

	void FLangBinder::ReportMissingSubstrateNode(const TCHAR* NodeName, const FLangSpan& Span, const FText& Spelling)
	{
		for (const SubstrateSugar::FVersionGate& Gate : SubstrateSugar::VersionGates)
		{
			if (FCString::Strcmp(Gate.Name, NodeName) == 0)
			{
				Diagnostics.Error(
					TEXT("DSH5294"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("SubstrateNodeNeedsEngine", "{0} needs the node 'Substrate.{1}', which Unreal Engine has from {2} on; this engine does not have it."),
						Spelling,
						FText::FromString(NodeName),
						FText::FromString(Gate.Engine)));
				return;
			}
		}

		Diagnostics.Error(
			TEXT("DSH5294"),
			CurrentFile,
			Span,
			FText::Format(
				LOCTEXT("SubstrateNodeMissing", "{0} needs the node 'Substrate.{1}', and this engine has no such node; Substrate nodes exist from Unreal Engine 5.4 on."),
				Spelling,
				FText::FromString(NodeName)));
	}

	bool FLangBinder::TryDescribeSubstrateVersionGate(const FString& NodeName, FText& OutEngine) const
	{
		for (const SubstrateSugar::FVersionGate& Gate : SubstrateSugar::VersionGates)
		{
			if (NodeName.Equals(Gate.Name, ESearchCase::CaseSensitive))
			{
				OutEngine = FText::FromString(Gate.Engine);
				return true;
			}
		}
		return false;
	}

	IR::FIRType FLangBinder::BindSubstrateSugarNode(
		const FExpr& Expr,
		const TCHAR* NodeName,
		const FText& Spelling,
		const TArray<FSubstrateSugarOperand>& Operands)
	{
		const int32 CatalogIndex = Catalog.FindExpression(Namespaces::Substrate, NodeName);
		if (CatalogIndex == INDEX_NONE)
		{
			ReportMissingSubstrateNode(NodeName, Expr.Span, Spelling);
			return Fail(Expr);
		}
		const IR::FCatalogExpression& Class = Catalog.Expressions[CatalogIndex];

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::ReflectedCall;
		Binding.Index = CatalogIndex;
		Binding.Type = IR::FIRType::Substrate();

		bool bAnyError = false;
		for (const FSubstrateSugarOperand& Operand : Operands)
		{
			const int32 PinIndex = Class.FindInput(Operand.Pin);
			if (!Operand.Expr || PinIndex == INDEX_NONE)
			{
				// A catalog whose node has other pins than the engine's is not a catalog this sugar can be bound against.
				ReportMissingSubstrateNode(NodeName, Expr.Span, Spelling);
				return Fail(Expr);
			}

			const IR::FCatalogPin& Pin = Class.Inputs[PinIndex];
			const IR::FIRType PinType = IR::TypeFromCatalogValueType(Pin.Type);

			FBoundArgument Argument;
			Argument.ArgumentIndex = Operand.Ordinal;
			Argument.Target = Pin.Name;
			Argument.TargetIndex = PinIndex;
			Argument.Conversion = Convert(
				*Operand.Expr,
				PinType,
				EConversionSite::Pin,
				FText::Format(
					LOCTEXT("SugarPinOf", "The '{0}' side of {1}"),
					FText::FromString(Pin.Name),
					Spelling));
			if (Argument.Conversion == IR::EIRConversion::None)
			{
				bAnyError = true;
			}
			Binding.Args.Add(MoveTemp(Argument));
		}

		if (bAnyError)
		{
			return Fail(Expr);
		}
		return Emit(Expr, MoveTemp(Binding));
	}

	IR::FIRType FLangBinder::BindSubstrateBinary(const FBinaryExpr& Expr, const IR::FIRType& LeftType, const IR::FIRType& RightType)
	{
		const bool bLeftSubstrate = LeftType.Kind == IR::EIRTypeKind::Substrate;
		const bool bRightSubstrate = RightType.Kind == IR::EIRTypeKind::Substrate;

		TArray<FSubstrateSugarOperand> Operands;

		if (Expr.Op == EBinaryOp::Add && bLeftSubstrate && bRightSubstrate)
		{
			Operands.Add({ Expr.Left.Get(), 0, TEXT("A") });
			Operands.Add({ Expr.Right.Get(), 1, TEXT("B") });
			return BindSubstrateSugarNode(Expr, TEXT("Add"), LOCTEXT("SugarAdd", "'+' over two Substrate values"), Operands);
		}

		if (Expr.Op == EBinaryOp::Multiply && bLeftSubstrate != bRightSubstrate)
		{
			const IR::FIRType& WeightType = bLeftSubstrate ? RightType : LeftType;
			const bool bScalar = (WeightType.IsNumeric() || WeightType.IsBool()) && !WeightType.IsMatrix() && WeightType.Rows == 1;
			if (bScalar)
			{
				Operands.Add({ bLeftSubstrate ? Expr.Left.Get() : Expr.Right.Get(), bLeftSubstrate ? 0 : 1, TEXT("A") });
				Operands.Add({ bLeftSubstrate ? Expr.Right.Get() : Expr.Left.Get(), bLeftSubstrate ? 1 : 0, TEXT("Weight") });
				return BindSubstrateSugarNode(Expr, TEXT("Weight"), LOCTEXT("SugarWeight", "'*' over a Substrate value and a number"), Operands);
			}
		}

		if (LeftType.IsError() || RightType.IsError())
		{
			return Fail(Expr);
		}

		Diagnostics.Error(
			TEXT("DSH5293"),
			CurrentFile,
			Expr.Span,
			LOCTEXT("SubstrateOperator", "Substrate values support only '+' (Substrate.Add) and '* scalar' (Substrate.Weight); use lerp() for mixing and Substrate.Layer() for layering."));
		return Fail(Expr);
	}

	bool FLangBinder::TryBindSubstrateLerp(
		const FCallExpr& Expr,
		const TArray<const FExpr*>& Slots,
		const TArray<int32>& ArgumentOfSlot,
		IR::FIRType& OutType)
	{
		if (Slots.Num() != 3 || !Slots[0] || !Slots[1] || !Slots[2] || ArgumentOfSlot.Num() != 3)
		{
			return false;
		}
		const IR::FIRType A = ResolveNodeDefaultOf(*Slots[0]);
		const IR::FIRType B = ResolveNodeDefaultOf(*Slots[1]);
		if (A.Kind != IR::EIRTypeKind::Substrate && B.Kind != IR::EIRTypeKind::Substrate)
		{
			return false;
		}

		if (A.Kind != B.Kind)
		{
			// `lerp(Slab, 0.5, t)`: one Substrate value cannot be mixed with a number.
			Diagnostics.Error(
				TEXT("DSH5293"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("SubstrateLerpMixed", "'lerp' mixes two Substrate values or two numbers, and these are {0} and {1}."),
					DescribeType(A),
					DescribeType(B)));
			OutType = Fail(Expr);
			return true;
		}

		TArray<FSubstrateSugarOperand> Operands;
		Operands.Add({ Slots[0], ArgumentOfSlot[0], TEXT("Background") });
		Operands.Add({ Slots[1], ArgumentOfSlot[1], TEXT("Foreground") });
		Operands.Add({ Slots[2], ArgumentOfSlot[2], TEXT("Mix") });
		OutType = BindSubstrateSugarNode(Expr, TEXT("HorizontalMix"), LOCTEXT("SugarLerp", "'lerp' over two Substrate values"), Operands);
		return true;
	}

	void FLangBinder::NoteSubstrateBuilderDeclared(const int32 Slot, const IR::FIRType& Type, const FExpr& Initializer)
	{
		if (!CurrentFunction || !CurrentFunction->Locals.IsValidIndex(Slot) || Type.Kind != IR::EIRTypeKind::Substrate)
		{
			return;
		}

		const FExpr* Inner = &Initializer;
		while (const FParenExpr* Paren = Inner->As<FParenExpr>())
		{
			if (!Paren->Inner)
			{
				return;
			}
			Inner = Paren->Inner.Get();
		}

		// `Substrate.Slab()` and nothing else: a call with arguments is a finished node, the way it always was.
		const FCallExpr* Call = Inner->As<FCallExpr>();
		const FBoundExpr* BoundCall = Call ? Lookup(*Call) : nullptr;
		if (!Call || Call->Arguments.Num() != 0 || !BoundCall
			|| BoundCall->Kind != EBoundExprKind::ReflectedCall
			|| !Catalog.Expressions.IsValidIndex(BoundCall->Index)
			|| Catalog.Expressions[BoundCall->Index].Inputs.Num() == 0
			|| !Catalog.Expressions[BoundCall->Index].Namespace.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive))
		{
			// A node of the Substrate namespace, which is where the required-pin question was put off for it
			// (BindReflectedCall, bBeginsBuilder).
			return;
		}
		CurrentFunction->Locals[Slot].SubstrateBuilderClass = BoundCall->Index;
		BuilderDeclBranchDepth.Add(Slot, BranchDepth);
	}

	void FLangBinder::NoteSubstrateBuilderReassigned(const FExpr& Target)
	{
		const FExpr* Inner = &Target;
		while (const FParenExpr* Paren = Inner->As<FParenExpr>())
		{
			if (!Paren->Inner)
			{
				return;
			}
			Inner = Paren->Inner.Get();
		}

		// The whole local, not a member of it: `S.Pin = x` is what a builder is for.
		const FIdentifierExpr* Name = Inner->As<FIdentifierExpr>();
		const FBoundExpr* Binding = Name ? Lookup(*Name) : nullptr;
		if (Binding && Binding->Kind == EBoundExprKind::Local && CurrentFunction
			&& CurrentFunction->Locals.IsValidIndex(Binding->LocalSlot)
			&& CurrentFunction->Locals[Binding->LocalSlot].SubstrateBuilderClass != INDEX_NONE)
		{
			ReassignedBuilderSlots.Add(Binding->LocalSlot);
		}
	}

	void FLangBinder::CheckSubstrateBuilderSealed(const int32 Slot, const FIdentifierExpr& Use)
	{
		if (!CurrentFunction || !CurrentFunction->Locals.IsValidIndex(Slot)
			|| !Catalog.Expressions.IsValidIndex(CurrentFunction->Locals[Slot].SubstrateBuilderClass))
		{
			return;
		}
		const IR::FCatalogExpression& Class = Catalog.Expressions[CurrentFunction->Locals[Slot].SubstrateBuilderClass];

		// Nothing written at all is a builder too: the node without arguments.
		static const TArray<FString> NothingWritten;
		const TArray<FString>* Found = BuilderMembersWritten.Find(Slot);
		const TArray<FString>* Written = Found ? Found : &NothingWritten;

		// The required pins a call is asked about where it is written (DSH5219, DSH5279 in a 1.x body); a builder was
		// let off there, because its pins are written on the lines that follow.
		for (const IR::FCatalogPin& Pin : Class.Inputs)
		{
			if (!Pin.bRequired || Written->Contains(Pin.Name))
			{
				continue;
			}
			if (IsLegacyScope())
			{
				Diagnostics.Warning(
					TEXT("DSH5279"),
					CurrentFile,
					Use.Span,
					FText::Format(
						LOCTEXT("LegacyBuilderRequiredPin", "'{0}' is used with the required '{1}' pin of '{2}.{3}' unconnected, which 1.x allowed and the engine reports when the material compiles."),
						FText::FromString(Use.Name),
						FText::FromString(Pin.Name),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName)));
			}
			else
			{
				Diagnostics.Warning(
					TEXT("DSH5219"),
					CurrentFile,
					Use.Span,
					FText::Format(
						LOCTEXT("BuilderRequiredPin", "'{0}' is used with the required '{1}' pin of '{2}.{3}' unconnected; unless the node reads a default for it, the engine reports it when the material compiles."),
						FText::FromString(Use.Name),
						FText::FromString(Pin.Name),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName)));
			}
		}

		// The companions CheckSubstrateVirtualArguments asks of a call. The members are written one statement at a time
		// and in any order, so the question waits for the first use of the value.
		for (const SubstrateSugar::FVirtualFamily& Family : SubstrateSugar::VirtualFamilies)
		{
			if (!SubstrateSugar::ClassTakesFamily(Class, Family))
			{
				continue;
			}
			const TCHAR* Trigger = nullptr;
			for (const TCHAR* Name : Family.Names)
			{
				// A name that is a real pin of this class is that pin, and asks for nothing.
				if (Name && Written->Contains(FString(Name)) && Class.FindInput(FString(Name)) == INDEX_NONE)
				{
					Trigger = Name;
					break;
				}
			}
			if (!Trigger)
			{
				continue;
			}

			if (FCString::Strcmp(Trigger, TEXT("Thickness")) == 0 && !Written->Contains(FString(TEXT("Transmittance"))))
			{
				Diagnostics.Error(
					TEXT("DSH5296"),
					CurrentFile,
					Use.Span,
					FText::Format(
						LOCTEXT("BuilderThicknessAlone", "'{0}.Thickness' is how deep 'Transmittance' is measured, and '{0}' was given no 'Transmittance' before this use."),
						FText::FromString(Use.Name)));
			}
			if (Family.RequiresPin && !Written->Contains(FString(Family.RequiresPin)))
			{
				Diagnostics.Error(
					TEXT("DSH5296"),
					CurrentFile,
					Use.Span,
					FText::Format(
						LOCTEXT("BuilderRequires", "'{0}.{1}' is measured against '{2}', and '{0}' was given no '{2}' before this use."),
						FText::FromString(Use.Name),
						FText::FromString(Trigger),
						FText::FromString(Family.RequiresPin)));
			}
		}
	}

	bool FLangBinder::TryBindSubstrateBuilderMember(const FMemberExpr& Expr, IR::FIRType& OutType)
	{
		const FIdentifierExpr* Object = Expr.Object ? Expr.Object->As<FIdentifierExpr>() : nullptr;
		if (!Object || !CurrentFunction)
		{
			return false;
		}
		const int32 Slot = FindLocal(Object->Name);
		if (Slot == INDEX_NONE || !CurrentFunction->Locals.IsValidIndex(Slot))
		{
			return false;
		}
		const int32 ClassIndex = CurrentFunction->Locals[Slot].SubstrateBuilderClass;
		if (!Catalog.Expressions.IsValidIndex(ClassIndex))
		{
			return false;
		}
		const IR::FCatalogExpression& Class = Catalog.Expressions[ClassIndex];
		const bool bWrite = BuilderWriteTarget == &Expr;
		const bool bReadsFirst = bWrite && bBuilderWriteReadsFirst;

		{
			// The object is named, not used: binding it here must not seal the builder it names.
			TGuardValue<bool> ObjectGuard(bBindingBuilderObject, true);
			BindExpr(*Expr.Object);
		}

		if (ReassignedBuilderSlots.Contains(Slot))
		{
			Diagnostics.Error(
				TEXT("DSH5297"),
				CurrentFile,
				Expr.Span,
				FText::Format(
					LOCTEXT("BuilderReassigned", "'{0}' has been assigned a whole Substrate value since it was declared, and that value has no members; build a new value, or write the members before the assignment."),
					FText::FromString(Object->Name)));
			OutType = Fail(Expr);
			return true;
		}

		// A pin of the node, or an argument of sugar S3 the node takes.
		FString MemberName = Expr.Member;
		IR::FIRType MemberType = IR::FIRType::Error();
		const int32 PinIndex = Class.FindInput(Expr.Member);
		if (PinIndex != INDEX_NONE)
		{
			MemberName = Class.Inputs[PinIndex].Name;
			MemberType = IR::TypeFromCatalogValueType(Class.Inputs[PinIndex].Type);
		}
		else
		{
			const SubstrateSugar::FVirtualFamily* Family = SubstrateSugar::FindFamily(Expr.Member);
			if (Family && SubstrateSugar::ClassTakesFamily(Class, *Family))
			{
				MemberType = SubstrateSugar::TypeOfVirtualArgument(Expr.Member);
			}
		}
		if (MemberType.IsError())
		{
			Diagnostics.Error(
				TEXT("DSH5299"),
				CurrentFile,
				Expr.MemberSpan,
				FText::Format(
					LOCTEXT("BuilderNoSuchPin", "'{0}' builds a '{1}.{2}', which has no pin called '{3}'."),
					FText::FromString(Object->Name),
					FText::FromString(Class.Namespace),
					FText::FromString(Class.ShortName),
					FText::FromString(Expr.Member)));
			OutType = Fail(Expr);
			return true;
		}

		TArray<FString>& Written = BuilderMembersWritten.FindOrAdd(Slot);
		if (bWrite)
		{
			if (SealedBuilderSlots.Contains(Slot))
			{
				Diagnostics.Error(
					TEXT("DSH5297"),
					CurrentFile,
					Expr.Span,
					FText::Format(
						LOCTEXT("BuilderSealed", "The Substrate value '{0}' is sealed: it has been used already, and its node is what it was then. Write its members before using it."),
						FText::FromString(Object->Name)));
				OutType = Fail(Expr);
				return true;
			}
			// Under an `if` the declaration is outside of. A builder declared inside the arm is that arm's own.
			const int32* DeclDepth = BuilderDeclBranchDepth.Find(Slot);
			if (BranchDepth > (DeclDepth ? *DeclDepth : 0))
			{
				Diagnostics.Error(
					TEXT("DSH5298"),
					CurrentFile,
					Expr.Span,
					FText::Format(
						LOCTEXT("BuilderWriteInBranch", "A member of the Substrate value '{0}' cannot be written inside an 'if': that would be one node in two versions. Build two values and choose between them."),
						FText::FromString(Object->Name)));
				OutType = Fail(Expr);
				return true;
			}
			if (bReadsFirst && !Written.Contains(MemberName))
			{
				Diagnostics.Error(
					TEXT("DSH5299"),
					CurrentFile,
					Expr.MemberSpan,
					FText::Format(
						LOCTEXT("BuilderReadWriteUnset", "'{0}.{1}' has not been given a value, so there is nothing for this operator to start from; assign it first."),
						FText::FromString(Object->Name),
						FText::FromString(MemberName)));
				OutType = Fail(Expr);
				return true;
			}

			// The pairs that parameterize the same pins (sugar S3), across statements this time.
			if (const SubstrateSugar::FVirtualFamily* Family = SubstrateSugar::FindFamily(MemberName))
			{
				for (const TCHAR* Conflict : Family->ConflictsWith)
				{
					if (Conflict && Written.Contains(FString(Conflict)) && PinIndex == INDEX_NONE)
					{
						Diagnostics.Error(
							TEXT("DSH5295"),
							CurrentFile,
							Expr.MemberSpan,
							FText::Format(
								LOCTEXT("BuilderVirtualConflict", "'{0}.{1}': '{2}' and '{3}' parameterize the same pins; give one of them."),
								FText::FromString(Class.Namespace),
								FText::FromString(Class.ShortName),
								FText::FromString(MemberName),
								FText::FromString(Conflict)));
					}
				}
			}
			else
			{
				for (const SubstrateSugar::FVirtualFamily& Other : SubstrateSugar::VirtualFamilies)
				{
					bool bFeedsThisPin = false;
					for (const TCHAR* Fed : Other.FeedsPins)
					{
						bFeedsThisPin = bFeedsThisPin || (Fed && MemberName.Equals(Fed, ESearchCase::CaseSensitive));
					}
					if (!bFeedsThisPin)
					{
						continue;
					}
					for (const TCHAR* Name : Other.Names)
					{
						// `Thickness` feeds nothing on its own; the family's other names do.
						if (Name && Written.Contains(FString(Name)) && FCString::Strcmp(Name, TEXT("Thickness")) != 0)
						{
							Diagnostics.Error(
								TEXT("DSH5295"),
								CurrentFile,
								Expr.MemberSpan,
								FText::Format(
									LOCTEXT("BuilderPinConflict", "'{0}.{1}': '{2}' and '{3}' parameterize the same pins; give one of them."),
									FText::FromString(Class.Namespace),
									FText::FromString(Class.ShortName),
									FText::FromString(Name),
									FText::FromString(MemberName)));
						}
					}
				}
			}
			Written.AddUnique(MemberName);
		}
		else if (!Written.Contains(MemberName))
		{
			Diagnostics.Error(
				TEXT("DSH5299"),
				CurrentFile,
				Expr.MemberSpan,
				FText::Format(
					LOCTEXT("BuilderReadUnset", "'{0}.{1}' has not been given a value, so there is nothing to read; a member of a Substrate value reads back what was written to it."),
					FText::FromString(Object->Name),
					FText::FromString(MemberName)));
			OutType = Fail(Expr);
			return true;
		}

		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::SubstrateBuilderPin;
		Binding.LocalSlot = Slot;
		Binding.Index = ClassIndex;
		Binding.FieldIndex = PinIndex;
		Binding.BuilderPin = MemberName;
		Binding.Type = MemberType;
		// An lvalue as the target it is being written as and nowhere else: a swizzle of a pin, or a pin as an `out`
		// argument, has nothing to write back into.
		Binding.bLValue = bWrite;
		OutType = Emit(Expr, MoveTemp(Binding));
		return true;
	}

	bool FLangBinder::TryBindSubstrateVirtualArgument(
		const FArgument& Argument,
		const int32 ArgumentIndex,
		const IR::FCatalogExpression& Class,
		const FString& Target,
		FBoundExpr& InOutBinding,
		bool& bInOutAnyError)
	{
		const SubstrateSugar::FVirtualFamily* Family = SubstrateSugar::FindFamily(Target);
		if (!Family || !SubstrateSugar::ClassTakesFamily(Class, *Family))
		{
			return false;
		}

		for (const FBoundArgument& Existing : InOutBinding.Args)
		{
			if (Existing.bIsVirtual && Existing.Target.Equals(Target, ESearchCase::CaseSensitive))
			{
				if (Argument.Value)
				{
					BindExpr(*Argument.Value);
				}
				Diagnostics.Error(
					TEXT("DSH4215"),
					CurrentFile,
					Argument.Span,
					FText::Format(
						LOCTEXT("VirtualTwice", "'{0}' is given twice in this call."),
						FText::FromString(Target)));
				bInOutAnyError = true;
				return true;
			}
		}

		FBoundArgument Virtual;
		Virtual.ArgumentIndex = ArgumentIndex;
		Virtual.Target = Target;
		Virtual.bIsVirtual = true;

		if (Argument.Value)
		{
			const IR::FIRType Wanted = SubstrateSugar::TypeOfVirtualArgument(Target);
			BindExpr(*Argument.Value, &Wanted);
			Virtual.Conversion = Convert(
				*Argument.Value,
				Wanted,
				EConversionSite::Pin,
				FText::Format(
					LOCTEXT("VirtualArgumentOf", "The '{0}' argument of '{1}.{2}'"),
					FText::FromString(Target),
					FText::FromString(Class.Namespace),
					FText::FromString(Class.ShortName)));
			if (Virtual.Conversion == IR::EIRConversion::None)
			{
				bInOutAnyError = true;
			}
		}
		else
		{
			bInOutAnyError = true;
		}

		InOutBinding.Args.Add(MoveTemp(Virtual));
		return true;
	}

	bool FLangBinder::CheckSubstrateVirtualArguments(const FCallExpr& Expr, const IR::FCatalogExpression& Class, const FBoundExpr& Binding)
	{
		const auto IsGiven = [&Binding](const TCHAR* Name) -> const FBoundArgument*
		{
			for (const FBoundArgument& Argument : Binding.Args)
			{
				if (Argument.Target.Equals(Name, ESearchCase::CaseSensitive))
				{
					return &Argument;
				}
			}
			return nullptr;
		};
		const auto SpanOf = [&Expr](const FBoundArgument& Argument) -> FLangSpan
		{
			return Expr.Arguments.IsValidIndex(Argument.ArgumentIndex) ? Expr.Arguments[Argument.ArgumentIndex].Span : Expr.Span;
		};

		bool bOk = true;
		for (const SubstrateSugar::FVirtualFamily& Family : SubstrateSugar::VirtualFamilies)
		{
			const FBoundArgument* Trigger = nullptr;
			for (const TCHAR* Name : Family.Names)
			{
				if (!Name)
				{
					continue;
				}
				const FBoundArgument* Given = IsGiven(Name);
				if (Given && Given->bIsVirtual)
				{
					Trigger = Given;
					break;
				}
			}
			if (!Trigger)
			{
				continue;
			}

			// `Thickness` alone says nothing: it is the second half of `Transmittance`.
			if (Trigger->Target.Equals(TEXT("Thickness"), ESearchCase::CaseSensitive) && !IsGiven(TEXT("Transmittance")))
			{
				Diagnostics.Error(
					TEXT("DSH5296"),
					CurrentFile,
					SpanOf(*Trigger),
					FText::Format(
						LOCTEXT("VirtualThicknessAlone", "'{0}.{1}': 'Thickness' is how deep 'Transmittance' is measured, and this call gives no 'Transmittance'."),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName)));
				bOk = false;
			}

			for (const TCHAR* Conflict : Family.ConflictsWith)
			{
				if (!Conflict)
				{
					continue;
				}
				// A family does not conflict with its own members (IOR lists BaseColor; BaseColor lists IOR: said once).
				const FBoundArgument* Other = IsGiven(Conflict);
				if (!Other || Other == Trigger)
				{
					continue;
				}
				bool bOwnMember = false;
				for (const TCHAR* Name : Family.Names)
				{
					bOwnMember = bOwnMember || (Name && FCString::Strcmp(Name, Conflict) == 0);
				}
				if (bOwnMember)
				{
					continue;
				}
				// Reported from the family that comes first in the table, so that a pair is one message.
				if (Other->bIsVirtual)
				{
					const SubstrateSugar::FVirtualFamily* OtherFamily = SubstrateSugar::FindFamily(Other->Target);
					if (OtherFamily && OtherFamily < &Family)
					{
						continue;
					}
				}
				Diagnostics.Error(
					TEXT("DSH5295"),
					CurrentFile,
					SpanOf(*Other),
					FText::Format(
						LOCTEXT("VirtualConflict", "'{0}.{1}': '{2}' and '{3}' parameterize the same pins; give one of them."),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName),
						FText::FromString(Trigger->Target),
						FText::FromString(Other->Target)));
				bOk = false;
			}

			if (Family.RequiresPin && !IsGiven(Family.RequiresPin))
			{
				Diagnostics.Error(
					TEXT("DSH5296"),
					CurrentFile,
					SpanOf(*Trigger),
					FText::Format(
						LOCTEXT("VirtualRequires", "'{0}.{1}': '{2}' is measured against '{3}', and this call gives no '{3}'."),
						FText::FromString(Class.Namespace),
						FText::FromString(Class.ShortName),
						FText::FromString(Trigger->Target),
						FText::FromString(Family.RequiresPin)));
				bOk = false;
			}
		}
		return bOk;
	}
}

#undef LOCTEXT_NAMESPACE
