// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// ValidateDreamShaderIR: every rule the emitter is allowed to assume.
//
// House rules for this file:
//
//   * Every raise site spells its own `Sink.Error(TEXT("DSHnnnn"), ...)` literally, because
//     .skill/gen-diagnostics.ps1 finds codes by that exact shape (CONTRACT §0.8). Error() returns
//     false, so `bValid &= Sink.Error(...)` both reports and records in one line.
//   * Nothing here indexes without checking first. The whole point of a validator is to survive
//     data that is wrong, including data that is wrong in ways the builder did not imagine.
//   * A check that would need engine knowledge is SKIPPED when the catalog is empty rather than
//     reported, so a hand-built graph in a unit test does not drown in "unknown class".
//   * A value already typed Error was reported upstream; checks that would fire on it are skipped
//     so one bad expression costs one diagnostic, not twenty.

#include "IR/IRValidator.h"

#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/CString.h"

#define LOCTEXT_NAMESPACE "DreamShader.IR.Validator"

namespace UE::DreamShader::IR
{
	namespace Private
	{
		/** Whether a node's inputs arrive as positional operands, as named pins, or not at all. */
		enum class EIROperandStyle : uint8
		{
			None,
			Positional,
			Named,
		};

		/**
		 * IR.h's FIRNode comment is the authority here: Operands are for "core math ops, Swizzle,
		 * Append, Select, Compare, StaticSwitch, TextureSample, FunctionOutput, Convert, Broadcast";
		 * Inputs are for "Reflected, FunctionCall, Custom, Make/Set/GetMaterialAttributes,
		 * MaterialSink"; the four leaves read nothing.
		 */
		static EIROperandStyle GetOperandStyle(const EIROp Op)
		{
			switch (Op)
			{
			case EIROp::Constant:
			case EIROp::Parameter:
			case EIROp::TextureParameter:
			case EIROp::FunctionInput:
				return EIROperandStyle::None;

			case EIROp::Reflected:
			case EIROp::FunctionCall:
			case EIROp::Custom:
			case EIROp::MakeMaterialAttributes:
			case EIROp::SetMaterialAttributes:
			case EIROp::GetMaterialAttributes:
			case EIROp::MaterialSink:
				return EIROperandStyle::Named;

			default:
				// TextureSample, Swizzle, Append, Select, Compare, StaticSwitch, FunctionOutput and
				// every math / comparison / logic / conversion op.
				return EIROperandStyle::Positional;
			}
		}

		/**
		 * Operand slots that may legitimately be left unset.
		 *
		 * TextureSample is the ONLY op with optional operands (IR.h FIRNode::Operands,
		 * CONTRACT §6.13 #14): its operands are always the four [Texture, UV, Sampler, Level], with
		 * FIRValue::None() in a slot the source did not give, so the emitter indexes rather than
		 * counts. Texture and UV are still required -- a sample with no texture is not a sample.
		 *
		 * Everywhere else an unset value is an error, Compare included: the engine's If node treats
		 * AEqualsB as optional, but the IR does not, so a lowered `a > b` fills all five slots.
		 */
		static bool IsOptionalOperand(const EIROp Op, const int32 OperandIndex)
		{
			return Op == EIROp::TextureSample && (OperandIndex == 2 || OperandIndex == 3);
		}

		/** The kind an IR-owned property must hold. Reflected engine properties are not in this table. */
		struct FPropertyKindRule
		{
			const TCHAR* Name;
			EIRPropertyKind Kind;
		};

		static bool TryGetExpectedPropertyKind(const FString& Name, EIRPropertyKind& OutKind)
		{
			static const FPropertyKindRule Rules[] =
			{
				{ Prop::Value,             EIRPropertyKind::Float4 },
				{ Prop::ParameterName,     EIRPropertyKind::Name },
				{ Prop::Group,             EIRPropertyKind::Name },
				{ Prop::Description,       EIRPropertyKind::String },
				{ Prop::SliderMin,         EIRPropertyKind::Float },
				{ Prop::SliderMax,         EIRPropertyKind::Float },
				{ Prop::SortPriority,      EIRPropertyKind::Int },
				{ Prop::DefaultValue,      EIRPropertyKind::Float4 },
				{ Prop::IsStatic,          EIRPropertyKind::Bool },
				{ Prop::DefaultAsset,      EIRPropertyKind::Object },
				{ Prop::SamplerType,       EIRPropertyKind::Enum },
				{ Prop::MipValueMode,      EIRPropertyKind::Enum },
				{ Prop::Mask,              EIRPropertyKind::String },
				{ Prop::InputName,         EIRPropertyKind::Name },
				{ Prop::InputType,         EIRPropertyKind::Enum },
				{ Prop::IsOptional,        EIRPropertyKind::Bool },
				{ Prop::PreviewValue,      EIRPropertyKind::Float4 },
				{ Prop::OutputName,        EIRPropertyKind::Name },
				{ Prop::FunctionPath,      EIRPropertyKind::Object },
				{ Prop::LocalFunction,     EIRPropertyKind::Int },
				{ Prop::Code,              EIRPropertyKind::String },
				{ Prop::OutputType,        EIRPropertyKind::Enum },
				{ Prop::IncludeFilePaths,  EIRPropertyKind::StringList },
				{ Prop::AdditionalOutputs, EIRPropertyKind::StringList },
				{ Prop::AttributeSetTypes, EIRPropertyKind::StringList },
				{ Prop::ClassSpecifier,    EIRPropertyKind::String },
			};

			for (const FPropertyKindRule& Rule : Rules)
			{
				if (Name.Equals(Rule.Name, ESearchCase::CaseSensitive))
				{
					OutKind = Rule.Kind;
					return true;
				}
			}

			return false;
		}

		static const TCHAR* LexPropertyKind(const EIRPropertyKind Kind)
		{
			switch (Kind)
			{
			case EIRPropertyKind::Bool:       return TEXT("Bool");
			case EIRPropertyKind::Int:        return TEXT("Int");
			case EIRPropertyKind::Float:      return TEXT("Float");
			case EIRPropertyKind::Float4:     return TEXT("Float4");
			case EIRPropertyKind::String:     return TEXT("String");
			case EIRPropertyKind::Name:       return TEXT("Name");
			case EIRPropertyKind::Enum:       return TEXT("Enum");
			case EIRPropertyKind::Object:     return TEXT("Object");
			case EIRPropertyKind::StringList: return TEXT("StringList");
			}
			return TEXT("Float");
		}

		static FText TextFromInt(const int32 Value)
		{
			// FText::AsNumber would group the digits for the current culture; an index is not a
			// quantity and `1,024` in a diagnostic is nonsense.
			return FText::FromString(FString::FromInt(Value));
		}

		static FText DescribeNodeRef(const int32 NodeIndex, const FIRNode& Node)
		{
			return FText::FromString(FString::Printf(TEXT("%%%d (%s)"), NodeIndex, LexToString(Node.Op)));
		}

		static FText DescribeProductRef(const int32 ProductIndex, const FIRProduct& Product)
		{
			return FText::FromString(FString::Printf(TEXT("%d '%s'"), ProductIndex, *Product.Name));
		}

		/** Everything one validation run needs to carry. */
		struct FIRValidationContext
		{
			const FIRModule& Module;
			const FBuiltinCatalog& Catalog;
			Lang::FLangDiagnosticSink& Sink;
			bool bValid = true;
			/** No catalog means no engine knowledge; the checks that need it stand down. */
			bool bHasCatalog = false;
		};

		// ------------------------------------------------------------------------------- edges

		/**
		 * One operand or named input. Reports at most one diagnostic and answers whether the value
		 * is usable, so the type-dependent checks downstream can skip a broken edge in silence.
		 */
		static bool ValidateValue(
			FIRValidationContext& Context,
			const FIRGraph& Graph,
			const int32 NodeIndex,
			const FIRNode& Node,
			const FIRValue Value,
			const FText& SlotDescription,
			const bool bOptional)
		{
			if (!Value.IsValid())
			{
				if (bOptional)
				{
					return false;
				}
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4303"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("UnsetOperand", "Node {0} has nothing connected to {1}; every operand of this op must carry a value."),
						DescribeNodeRef(NodeIndex, Node),
						SlotDescription));
				return false;
			}

			if (!Graph.Nodes.IsValidIndex(Value.Node))
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4300"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("OperandBadNode", "Node {0} reads {1} from node %{2}, which does not exist; the graph has {3} nodes."),
						DescribeNodeRef(NodeIndex, Node),
						SlotDescription,
						TextFromInt(Value.Node),
						TextFromInt(Graph.Nodes.Num())));
				return false;
			}

			const FIRNode& Source = Graph.Nodes[Value.Node];
			if (Source.IsStatement())
			{
				// A sink, a function output, or a reflected custom-output class. All three are roots
				// that produce nothing, so reading one is not "the wrong output index" -- it is a
				// value that does not exist. The binder already says so for the custom-output case
				// (DSH4231); this catches a builder that made the node anyway.
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4301"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("OperandReadsStatement", "Node {0} reads {1} from node {2}, which is a statement and produces no value."),
						DescribeNodeRef(NodeIndex, Node),
						SlotDescription,
						DescribeNodeRef(Value.Node, Source)));
				return false;
			}

			if (!Source.Outputs.IsValidIndex(Value.Output))
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4301"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("OperandBadOutput", "Node {0} reads output {1} of node {2}, which has {3} output(s)."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Value.Output),
						DescribeNodeRef(Value.Node, Source),
						TextFromInt(Source.Outputs.Num())));
				return false;
			}

			return true;
		}

		// ------------------------------------------------------------------------------- shape

		static void ValidateNodeShape(
			FIRValidationContext& Context,
			const FIRGraph& Graph,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			const FIRCoreOpInfo& Info = GetCoreOpInfo(Node.Op);

			// Outputs. A statement produces nothing and everything else produces something; a value
			// node with no outputs cannot be read at all, which no later pass checks for.
			//
			// The test is FIRNode::IsStatement() rather than an op list ON PURPOSE (CONTRACT §6.13
			// #19): a reflected custom-output class -- UE.VolumetricAdvancedMaterialOutput and its
			// kind -- is a legal root with zero Outputs, and the frozen header is the one place that
			// says so. Do not "fix" this into `Op == MaterialSink || Op == FunctionOutput`.
			if (Node.IsStatement())
			{
				if (Node.Outputs.Num() != 0)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4307"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("StatementHasOutputs", "Node {0} is a statement and must have no outputs, but it declares {1}."),
							DescribeNodeRef(NodeIndex, Node),
							TextFromInt(Node.Outputs.Num())));
				}
			}
			else if (Node.Outputs.Num() == 0)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4307"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("ValueHasNoOutputs", "Node {0} declares no outputs, so nothing can read it."),
						DescribeNodeRef(NodeIndex, Node)));
			}

			// Output names, when present, are one per output.
			if (Node.OutputNames.Num() != 0 && Node.OutputNames.Num() != Node.Outputs.Num())
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4308"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("OutputNamesNotParallel", "Node {0} has {1} output name(s) for {2} output(s); the two lists are either parallel or the names are omitted."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Node.OutputNames.Num()),
						TextFromInt(Node.Outputs.Num())));
			}

			for (int32 First = 0; First < Node.OutputNames.Num(); ++First)
			{
				if (Node.OutputNames[First].IsEmpty())
				{
					continue;
				}
				for (int32 Second = First + 1; Second < Node.OutputNames.Num(); ++Second)
				{
					if (Node.OutputNames[First].Equals(Node.OutputNames[Second], ESearchCase::CaseSensitive))
					{
						Context.bValid &= Context.Sink.Error(
							TEXT("DSH4308"),
							Node.Source.Span,
							FText::Format(
								LOCTEXT("DuplicateOutputName", "Node {0} names two outputs '{1}'; an output name selects one slot and must be unique."),
								DescribeNodeRef(NodeIndex, Node),
								FText::FromString(Node.OutputNames[First])));
						break;
					}
				}
			}

			// Positional versus named.
			const EIROperandStyle Style = GetOperandStyle(Node.Op);
			if (Style != EIROperandStyle::Positional && Node.Operands.Num() != 0)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4304"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("UnexpectedOperands", "Node {0} carries {1} positional operand(s); this op takes {2}."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Node.Operands.Num()),
						Style == EIROperandStyle::Named
							? LOCTEXT("TakesNamedInputs", "named inputs")
							: LOCTEXT("TakesNothing", "no inputs at all")));
			}
			if (Style != EIROperandStyle::Named && Node.Inputs.Num() != 0)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4304"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("UnexpectedNamedInputs", "Node {0} carries {1} named input(s); this op takes {2}."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Node.Inputs.Num()),
						Style == EIROperandStyle::Positional
							? LOCTEXT("TakesOperands", "positional operands")
							: LOCTEXT("TakesNothing2", "no inputs at all")));
			}

			// Arity, from the one table that knows it.
			if (Style == EIROperandStyle::Positional
				&& (Node.Operands.Num() < Info.MinArity || Node.Operands.Num() > Info.MaxArity))
			{
				if (Info.MinArity == Info.MaxArity)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4302"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("WrongArityExact", "Node {0} has {1} operand(s); this op takes exactly {2}."),
							DescribeNodeRef(NodeIndex, Node),
							TextFromInt(Node.Operands.Num()),
							TextFromInt(Info.MinArity)));
				}
				else
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4302"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("WrongArityRange", "Node {0} has {1} operand(s); this op takes {2} to {3}."),
							DescribeNodeRef(NodeIndex, Node),
							TextFromInt(Node.Operands.Num()),
							TextFromInt(Info.MinArity),
							TextFromInt(Info.MaxArity)));
				}
			}

			// Region membership.
			if (Node.Region != INDEX_NONE && !Graph.Regions.IsValidIndex(Node.Region))
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4320"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("NodeBadRegion", "Node {0} sits in region {1}, which does not exist; the graph has {2} region(s)."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Node.Region),
						TextFromInt(Graph.Regions.Num())));
			}
		}

		// --------------------------------------------------------------------------- properties

		static void ValidateNodeProperties(
			FIRValidationContext& Context,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			for (int32 First = 0; First < Node.Properties.Num(); ++First)
			{
				const FIRProperty& Property = Node.Properties[First];

				if (Property.Name.IsEmpty())
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4324"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("EmptyPropertyName", "Node {0} carries a property with no name."),
							DescribeNodeRef(NodeIndex, Node)));
					continue;
				}

				for (int32 Second = First + 1; Second < Node.Properties.Num(); ++Second)
				{
					if (Property.Name.Equals(Node.Properties[Second].Name, ESearchCase::CaseSensitive))
					{
						Context.bValid &= Context.Sink.Error(
							TEXT("DSH4324"),
							Node.Source.Span,
							FText::Format(
								LOCTEXT("DuplicateProperty", "Node {0} sets property '{1}' twice."),
								DescribeNodeRef(NodeIndex, Node),
								FText::FromString(Property.Name)));
						break;
					}
				}

				// The kind table describes the IR's OWN property names. A Reflected node passes
				// engine properties through by their engine name, and an engine class is free to
				// have a `Description` or a `Group` that means something else entirely.
				if (Node.Op == EIROp::Reflected)
				{
					continue;
				}

				EIRPropertyKind Expected = EIRPropertyKind::Float;
				if (TryGetExpectedPropertyKind(Property.Name, Expected) && Property.Value.Kind != Expected)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4324"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("WrongPropertyKind", "Node {0} stores property '{1}' as {2}; it must be {3}."),
							DescribeNodeRef(NodeIndex, Node),
							FText::FromString(Property.Name),
							FText::FromString(LexPropertyKind(Property.Value.Kind)),
							FText::FromString(LexPropertyKind(Expected))));
				}
			}
		}

		/** Reports when a property the op cannot do without is missing. */
		static void ValidateRequiredProperty(
			FIRValidationContext& Context,
			const int32 NodeIndex,
			const FIRNode& Node,
			const TCHAR* PropertyName)
		{
			if (Node.FindProperty(PropertyName) != nullptr)
			{
				return;
			}

			Context.bValid &= Context.Sink.Error(
				TEXT("DSH4323"),
				Node.Source.Span,
				FText::Format(
					LOCTEXT("MissingProperty", "Node {0} needs property '{1}' and does not have it."),
					DescribeNodeRef(NodeIndex, Node),
					FText::FromString(PropertyName)));
		}

		// ------------------------------------------------------------------------- op specifics

		static void ValidateSwizzle(
			FIRValidationContext& Context,
			const FIRGraph& Graph,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			const FIRProperty* MaskProperty = Node.FindProperty(Prop::Mask);
			if (MaskProperty == nullptr || MaskProperty->Value.Kind != EIRPropertyKind::String)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4317"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("SwizzleNoMask", "Node {0} needs a string Mask property holding the component letters."),
						DescribeNodeRef(NodeIndex, Node)));
				return;
			}

			const FString& Mask = MaskProperty->Value.S;
			if (Mask.IsEmpty() || Mask.Len() > 4)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4317"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("SwizzleMaskLength", "Node {0} has mask '{1}'; a mask is one to four of x, y, z and w."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(Mask)));
				return;
			}

			int32 HighestComponent = 0;
			int32 PreviousComponent = INDEX_NONE;
			bool bMaskIsWellFormed = true;
			for (int32 Index = 0; Index < Mask.Len(); ++Index)
			{
				const TCHAR Character = Mask[Index];
				int32 Component = INDEX_NONE;
				switch (Character)
				{
				case TEXT('x'): Component = 0; break;
				case TEXT('y'): Component = 1; break;
				case TEXT('z'): Component = 2; break;
				case TEXT('w'): Component = 3; break;
				default:        Component = INDEX_NONE; break;
				}

				if (Component == INDEX_NONE)
				{
					// Canonical masks are lower-case xyzw; `rgba` is rewritten at bind time
					// (CONTRACT §6.6), so anything else here is a builder mistake, not an author's.
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4317"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("SwizzleMaskCharacter", "Node {0} has mask '{1}'; masks are canonical lower-case x, y, z and w."),
							DescribeNodeRef(NodeIndex, Node),
							FText::FromString(Mask)));
					bMaskIsWellFormed = false;
					break;
				}

				if (Component == PreviousComponent)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4317"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("SwizzleMaskRepeat", "Node {0} has mask '{1}', which names the same component twice; a ComponentMask cannot repeat a channel."),
							DescribeNodeRef(NodeIndex, Node),
							FText::FromString(Mask)));
					bMaskIsWellFormed = false;
					break;
				}

				if (Component < PreviousComponent)
				{
					// CONTRACT §6.13 #20: a ComponentMask carries four independent R/G/B/A flags and
					// has no way to express ORDER, so a mask can only ever be strictly ascending.
					// The builder splits `.yx` into per-channel masks plus an AppendVector, which is
					// what 1.x emitted; a descending mask reaching here would silently come out as
					// `.xy` in the asset.
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4317"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("SwizzleMaskOrder", "Node {0} has mask '{1}', which is not in ascending order; a ComponentMask cannot reorder channels, so a reordering swizzle is masks plus an Append."),
							DescribeNodeRef(NodeIndex, Node),
							FText::FromString(Mask)));
					bMaskIsWellFormed = false;
					break;
				}

				PreviousComponent = Component;
				HighestComponent = FMath::Max(HighestComponent, Component + 1);
			}

			if (!bMaskIsWellFormed || Node.Operands.Num() < 1)
			{
				return;
			}

			const FIRType& OperandType = Graph.TypeOf(Node.Operands[0]);
			const int32 Available = OperandType.GraphComponentCount();
			if (OperandType.IsError() || Available == 0)
			{
				// Already reported as DSH4306 (or upstream); one mistake, one diagnostic.
				return;
			}

			if (HighestComponent > Available)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4318"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("SwizzleTooWide", "Node {0} masks '{1}' out of a {2}, which has only {3} component(s)."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(Mask),
						FText::FromString(OperandType.ToString()),
						TextFromInt(Available)));
			}
		}

		static void ValidateControlOperand(
			FIRValidationContext& Context,
			const FIRGraph& Graph,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			if (Node.Operands.Num() < 1 || !Graph.IsValidValue(Node.Operands[0]))
			{
				return;
			}

			const FIRType& ConditionType = Graph.TypeOf(Node.Operands[0]);
			if (ConditionType.IsError())
			{
				return;
			}

			if (Node.Op == EIROp::StaticSwitch)
			{
				if (!ConditionType.IsBool() || ConditionType.GraphComponentCount() != 1)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4319"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("StaticSwitchNotBool", "Node {0} switches on a {1}; a StaticSwitch condition is a single static bool."),
							DescribeNodeRef(NodeIndex, Node),
							FText::FromString(ConditionType.ToString())));
				}
				return;
			}

			if (Node.Op == EIROp::Select && ConditionType.GraphComponentCount() != 1)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4319"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("SelectNotScalar", "Node {0} selects on a {1}; a condition is one 0/1 component."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(ConditionType.ToString())));
			}
		}

		static void ValidateMathWidths(
			FIRValidationContext& Context,
			const FIRGraph& Graph,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			for (int32 Index = 0; Index < Node.Operands.Num(); ++Index)
			{
				if (!Graph.IsValidValue(Node.Operands[Index]))
				{
					continue;
				}

				const FIRType& OperandType = Graph.TypeOf(Node.Operands[Index]);
				if (OperandType.IsError() || OperandType.GraphComponentCount() > 0)
				{
					continue;
				}

				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4306"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("MathOperandNotCarryable", "Node {0} does arithmetic on operand {1}, which is a {2}; the graph carries only float1 to float4, so a matrix, a texture or a material cannot reach a math node."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Index),
						FText::FromString(OperandType.ToString())));
			}

			if (Node.Outputs.Num() > 0
				&& !Node.Outputs[0].IsError()
				&& Node.Outputs[0].GraphComponentCount() == 0)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4306"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("MathResultNotCarryable", "Node {0} produces a {1}; a math node's result must be float1 to float4."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(Node.Outputs[0].ToString())));
			}
		}

		static void ValidateReflected(
			FIRValidationContext& Context,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			if (!Context.Catalog.Expressions.IsValidIndex(Node.CatalogIndex))
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4309"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("ReflectedNoCatalogIndex", "Node {0} is a reflected node with catalog index {1}, which is not an entry of the builtin catalog."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Node.CatalogIndex)));
				return;
			}

			const FCatalogExpression& Expression = Context.Catalog.Expressions[Node.CatalogIndex];

			if (!Node.ClassName.Equals(Expression.ShortName, ESearchCase::CaseSensitive))
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4312"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("ReflectedClassMismatch", "Node {0} names class '{1}' but its catalog entry is '{2}'; the two must agree."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(Node.ClassName),
						FText::FromString(Expression.ShortName)));
			}

			for (const FIRInput& Input : Node.Inputs)
			{
				if (Expression.FindInput(Input.Pin) != INDEX_NONE)
				{
					continue;
				}

				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4310"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("ReflectedUnknownPin", "Node {0} connects pin '{1}', which '{2}' does not have."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(Input.Pin),
						FText::FromString(Expression.ShortName)));
			}

			for (const FIRProperty& Property : Node.Properties)
			{
				// ClassSpecifier is the IR's own record of what the author wrote; it never reaches
				// reflection, so the class is not expected to have it.
				if (Property.Name.Equals(Prop::ClassSpecifier, ESearchCase::CaseSensitive))
				{
					continue;
				}
				if (Expression.FindProperty(Property.Name) != INDEX_NONE)
				{
					continue;
				}

				// A literal written where a pin's `Const*` twin exists is stored as that property
				// (CONTRACT §6.10); accept it whether or not the exporter listed it separately.
				bool bIsConstTwin = false;
				for (const FCatalogPin& Pin : Expression.Inputs)
				{
					if (!Pin.ConstPropertyName.IsEmpty()
						&& Pin.ConstPropertyName.Equals(Property.Name, ESearchCase::CaseSensitive))
					{
						bIsConstTwin = true;
						break;
					}
				}
				if (bIsConstTwin)
				{
					continue;
				}

				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4311"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("ReflectedUnknownProperty", "Node {0} sets property '{1}', which '{2}' does not have."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(Property.Name),
						FText::FromString(Expression.ShortName)));
			}
		}

		static void ValidateFunctionCall(
			FIRValidationContext& Context,
			const int32 ProductIndex,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			const FIRProperty* LocalFunction = Node.FindProperty(Prop::LocalFunction);
			const FIRProperty* FunctionPath = Node.FindProperty(Prop::FunctionPath);

			if (LocalFunction != nullptr && LocalFunction->Value.Kind == EIRPropertyKind::Int)
			{
				const int32 Target = static_cast<int32>(LocalFunction->Value.I);
				if (!Context.Module.Products.IsValidIndex(Target))
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4327"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("CallBadLocalProduct", "Node {0} calls local product {1}, which this module does not have; it has {2}."),
							DescribeNodeRef(NodeIndex, Node),
							TextFromInt(Target),
							TextFromInt(Context.Module.Products.Num())));
					return;
				}
				if (Target == ProductIndex)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4327"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("CallSelfProduct", "Node {0} calls the product it belongs to; a material function cannot call itself."),
							DescribeNodeRef(NodeIndex, Node)));
				}
				return;
			}

			const bool bHasPathProperty = FunctionPath != nullptr
				&& FunctionPath->Value.Kind == EIRPropertyKind::Object
				&& !FunctionPath->Value.S.IsEmpty();

			if (!bHasPathProperty && Node.ClassName.IsEmpty())
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4327"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("CallNoTarget", "Node {0} names neither a function asset path nor a local product; a MaterialFunctionCall needs one of the two."),
						DescribeNodeRef(NodeIndex, Node)));
			}
		}

		static void ValidateMaterialAttributeInputs(
			FIRValidationContext& Context,
			const int32 NodeIndex,
			const FIRNode& Node)
		{
			if (!Context.bHasCatalog)
			{
				return;
			}

			const bool bAllowsAttributesPin =
				Node.Op == EIROp::SetMaterialAttributes || Node.Op == EIROp::GetMaterialAttributes;

			for (const FIRInput& Input : Node.Inputs)
			{
				if (bAllowsAttributesPin && Input.Pin.Equals(TEXT("MaterialAttributes"), ESearchCase::CaseSensitive))
				{
					continue;
				}
				if (Context.Catalog.FindMaterialAttribute(Input.Pin) != INDEX_NONE)
				{
					continue;
				}

				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4325"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("UnknownMaterialAttribute", "Node {0} writes attribute '{1}', which is not in the engine's material attribute table."),
						DescribeNodeRef(NodeIndex, Node),
						FText::FromString(Input.Pin)));
			}

			if (const FIRProperty* SetTypes = Node.FindProperty(Prop::AttributeSetTypes))
			{
				for (const FString& AttributeName : SetTypes->Value.List)
				{
					if (Context.Catalog.FindMaterialAttribute(AttributeName) != INDEX_NONE)
					{
						continue;
					}

					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4325"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("UnknownAttributeSetType", "Node {0} lists '{1}' in AttributeSetTypes, which is not a material attribute."),
							DescribeNodeRef(NodeIndex, Node),
							FText::FromString(AttributeName)));
				}
			}
		}

		// -------------------------------------------------------------------------------- node

		static void ValidateNode(
			FIRValidationContext& Context,
			const FIRProduct& Product,
			const int32 ProductIndex,
			const int32 NodeIndex)
		{
			const FIRGraph& Graph = Product.Graph;
			const FIRNode& Node = Graph.Nodes[NodeIndex];

			ValidateNodeShape(Context, Graph, NodeIndex, Node);
			ValidateNodeProperties(Context, NodeIndex, Node);

			// Edges.
			for (int32 Index = 0; Index < Node.Operands.Num(); ++Index)
			{
				ValidateValue(
					Context,
					Graph,
					NodeIndex,
					Node,
					Node.Operands[Index],
					FText::Format(LOCTEXT("SlotOperand", "operand {0}"), TextFromInt(Index)),
					IsOptionalOperand(Node.Op, Index));
			}

			for (int32 First = 0; First < Node.Inputs.Num(); ++First)
			{
				const FIRInput& Input = Node.Inputs[First];

				if (Input.Pin.IsEmpty())
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4305"),
						Node.Source.Span,
						FText::Format(
							LOCTEXT("EmptyPinName", "Node {0} has a named input with no pin name."),
							DescribeNodeRef(NodeIndex, Node)));
				}
				else
				{
					for (int32 Second = First + 1; Second < Node.Inputs.Num(); ++Second)
					{
						if (Input.Pin.Equals(Node.Inputs[Second].Pin, ESearchCase::CaseSensitive))
						{
							Context.bValid &= Context.Sink.Error(
								TEXT("DSH4305"),
								Node.Source.Span,
								FText::Format(
									LOCTEXT("DuplicatePinName", "Node {0} connects pin '{1}' twice; a pin takes one value."),
									DescribeNodeRef(NodeIndex, Node),
									FText::FromString(Input.Pin)));
							break;
						}
					}
				}

				ValidateValue(
					Context,
					Graph,
					NodeIndex,
					Node,
					Input.Value,
					FText::Format(LOCTEXT("SlotPin", "pin '{0}'"), FText::FromString(Input.Pin)),
					/*bOptional*/ false);
			}

			// Op specifics.
			if (IsCoreMathOp(Node.Op) && Node.Op != EIROp::Convert && Node.Op != EIROp::Broadcast)
			{
				ValidateMathWidths(Context, Graph, NodeIndex, Node);
			}

			switch (Node.Op)
			{
			case EIROp::Constant:
				ValidateRequiredProperty(Context, NodeIndex, Node, Prop::Value);
				break;
			case EIROp::Parameter:
			case EIROp::TextureParameter:
				ValidateRequiredProperty(Context, NodeIndex, Node, Prop::ParameterName);
				break;
			case EIROp::FunctionInput:
				ValidateRequiredProperty(Context, NodeIndex, Node, Prop::InputName);
				break;
			case EIROp::FunctionOutput:
				ValidateRequiredProperty(Context, NodeIndex, Node, Prop::OutputName);
				break;
			case EIROp::Custom:
				ValidateRequiredProperty(Context, NodeIndex, Node, Prop::Code);
				break;
			case EIROp::Swizzle:
				ValidateSwizzle(Context, Graph, NodeIndex, Node);
				break;
			case EIROp::Select:
			case EIROp::StaticSwitch:
				ValidateControlOperand(Context, Graph, NodeIndex, Node);
				break;
			case EIROp::Reflected:
				if (Context.bHasCatalog)
				{
					ValidateReflected(Context, NodeIndex, Node);
				}
				break;
			case EIROp::FunctionCall:
				ValidateFunctionCall(Context, ProductIndex, NodeIndex, Node);
				break;
			case EIROp::MakeMaterialAttributes:
			case EIROp::SetMaterialAttributes:
			case EIROp::GetMaterialAttributes:
			case EIROp::MaterialSink:
				ValidateMaterialAttributeInputs(Context, NodeIndex, Node);
				break;
			default:
				break;
			}

			// A catalog index belongs to a reflected node and nowhere else: it is how the emitter
			// decides which path to take, and a stray one on a Multiply would send it down the
			// wrong one.
			if (Node.Op != EIROp::Reflected && Node.CatalogIndex != INDEX_NONE)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4309"),
					Node.Source.Span,
					FText::Format(
						LOCTEXT("StrayCatalogIndex", "Node {0} carries catalog index {1}; only a reflected node has one."),
						DescribeNodeRef(NodeIndex, Node),
						TextFromInt(Node.CatalogIndex)));
			}
		}

		// ----------------------------------------------------------------------------- product

		static void ValidateProductStructure(
			FIRValidationContext& Context,
			const FIRProduct& Product,
			const int32 ProductIndex)
		{
			const FIRGraph& Graph = Product.Graph;
			const bool bIsMaterial = Product.Kind == EIRProductKind::Material;

			// Sinks.
			TArray<int32> SinkNodes;
			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				if (Graph.Nodes[NodeIndex].Op == EIROp::MaterialSink)
				{
					SinkNodes.Add(NodeIndex);
				}
			}

			if (bIsMaterial)
			{
				if (SinkNodes.Num() != 1)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4313"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("MaterialSinkCount", "Material product {0} has {1} MaterialSink node(s); a material has exactly one."),
							DescribeProductRef(ProductIndex, Product),
							TextFromInt(SinkNodes.Num())));
				}
				else if (Graph.Sink != SinkNodes[0])
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4313"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("MaterialSinkIndex", "Material product {0} records its sink as {1} but the MaterialSink node is %{2}."),
							DescribeProductRef(ProductIndex, Product),
							TextFromInt(Graph.Sink),
							TextFromInt(SinkNodes[0])));
				}
			}
			else
			{
				if (SinkNodes.Num() != 0)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4313"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("FunctionHasSink", "Product {0} is a {1} and must have no MaterialSink node, but it has {2}."),
							DescribeProductRef(ProductIndex, Product),
							FText::FromString(LexToString(Product.Kind)),
							TextFromInt(SinkNodes.Num())));
				}
				if (Graph.Sink != INDEX_NONE)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4313"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("FunctionRecordsSink", "Product {0} is a {1} and records sink node {2}; only a material has a sink."),
							DescribeProductRef(ProductIndex, Product),
							FText::FromString(LexToString(Product.Kind)),
							TextFromInt(Graph.Sink)));
				}
			}

			// Function inputs and outputs.
			const auto ValidateFunctionList = [&](const TArray<int32>& List, const EIROp ExpectedOp, const FText& ListName)
			{
				for (const int32 NodeIndex : List)
				{
					if (!Graph.Nodes.IsValidIndex(NodeIndex))
					{
						Context.bValid &= Context.Sink.Error(
							TEXT("DSH4314"),
							Product.Source.Span,
							FText::Format(
								LOCTEXT("FunctionListBadIndex", "Product {0} lists node %{1} in its {2}, and that node does not exist."),
								DescribeProductRef(ProductIndex, Product),
								TextFromInt(NodeIndex),
								ListName));
						continue;
					}
					if (Graph.Nodes[NodeIndex].Op != ExpectedOp)
					{
						Context.bValid &= Context.Sink.Error(
							TEXT("DSH4314"),
							Product.Source.Span,
							FText::Format(
								LOCTEXT("FunctionListWrongOp", "Product {0} lists node {1} in its {2}, where only {3} nodes belong."),
								DescribeProductRef(ProductIndex, Product),
								DescribeNodeRef(NodeIndex, Graph.Nodes[NodeIndex]),
								ListName,
								FText::FromString(LexToString(ExpectedOp))));
					}
				}
			};

			ValidateFunctionList(Graph.FunctionInputs, EIROp::FunctionInput, LOCTEXT("ListInputs", "function inputs"));
			ValidateFunctionList(Graph.FunctionOutputs, EIROp::FunctionOutput, LOCTEXT("ListOutputs", "function outputs"));

			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				const EIROp Op = Graph.Nodes[NodeIndex].Op;
				if (Op == EIROp::FunctionInput && !Graph.FunctionInputs.Contains(NodeIndex))
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4314"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("UnlistedFunctionInput", "Product {0} has node {1} that is not in its function input list; the list is what gives an input its order."),
							DescribeProductRef(ProductIndex, Product),
							DescribeNodeRef(NodeIndex, Graph.Nodes[NodeIndex])));
				}
				else if (Op == EIROp::FunctionOutput && !Graph.FunctionOutputs.Contains(NodeIndex))
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4314"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("UnlistedFunctionOutput", "Product {0} has node {1} that is not in its function output list; the list is what gives an output its order."),
							DescribeProductRef(ProductIndex, Product),
							DescribeNodeRef(NodeIndex, Graph.Nodes[NodeIndex])));
				}
			}

			if (bIsMaterial)
			{
				if (Graph.FunctionInputs.Num() != 0 || Graph.FunctionOutputs.Num() != 0)
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4314"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("MaterialHasFunctionIO", "Material product {0} declares function inputs or outputs; a material has parameters and a sink, not a signature."),
							DescribeProductRef(ProductIndex, Product)));
				}
			}
			else if (Graph.FunctionOutputs.Num() == 0)
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4315"),
					Product.Source.Span,
					FText::Format(
						LOCTEXT("FunctionNoOutputs", "Product {0} is a {1} and produces nothing; a material function needs at least one FunctionOutput."),
						DescribeProductRef(ProductIndex, Product),
						FText::FromString(LexToString(Product.Kind))));
			}

			// Regions: a valid parent, and a chain that ends.
			for (int32 RegionIndex = 0; RegionIndex < Graph.Regions.Num(); ++RegionIndex)
			{
				const int32 Parent = Graph.Regions[RegionIndex].Parent;
				if (Parent == INDEX_NONE)
				{
					continue;
				}
				if (!Graph.Regions.IsValidIndex(Parent))
				{
					Context.bValid &= Context.Sink.Error(
						TEXT("DSH4320"),
						Graph.Regions[RegionIndex].Span,
						FText::Format(
							LOCTEXT("RegionBadParent", "Region {0} of product {1} names parent {2}, which does not exist."),
							TextFromInt(RegionIndex),
							DescribeProductRef(ProductIndex, Product),
							TextFromInt(Parent)));
					continue;
				}

				int32 Walk = Parent;
				int32 Steps = 0;
				while (Walk != INDEX_NONE && Graph.Regions.IsValidIndex(Walk) && Steps <= Graph.Regions.Num())
				{
					if (Walk == RegionIndex)
					{
						Context.bValid &= Context.Sink.Error(
							TEXT("DSH4320"),
							Graph.Regions[RegionIndex].Span,
							FText::Format(
								LOCTEXT("RegionCycle", "Region {0} of product {1} encloses itself; regions nest, they do not loop."),
								TextFromInt(RegionIndex),
								DescribeProductRef(ProductIndex, Product)));
						break;
					}
					Walk = Graph.Regions[Walk].Parent;
					++Steps;
				}
			}

			// Layout hints are carried through untouched, so this is the only place a malformed one
			// is noticed before the layout pass silently ignores it.
			//
			// WARNINGS, not errors, on purpose. The binder already decided that layout is a machine
			// domain where "a stale coordinate must never stop a material from building"
			// (LangBinderDirectives.cpp, DSH7230, every complaint there is a warning), and a hint
			// the layout pass cannot use costs a node its saved position and nothing else. A hint
			// the binder lets through -- it accepts the selector case-INSENSITIVELY and stores the
			// author's spelling -- must not fail the build here after being waved past there.
			// The emitter matches `Comment` case-sensitively, so a non-canonical spelling is still
			// worth saying out loud.
			for (int32 HintIndex = 0; HintIndex < Graph.LayoutHints.Num(); ++HintIndex)
			{
				const FIRLayoutHint& Hint = Graph.LayoutHints[HintIndex];
				const bool bIsNode = Hint.Kind.Equals(TEXT("Node"), ESearchCase::CaseSensitive);
				const bool bIsComment = Hint.Kind.Equals(TEXT("Comment"), ESearchCase::CaseSensitive);

				if (!bIsNode && !bIsComment)
				{
					Context.Sink.Warning(
						TEXT("DSH4328"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("LayoutHintKind", "Layout hint {0} of product {1} has kind '{2}'; a hint is spelled exactly 'Node' or 'Comment', so this one places nothing."),
							TextFromInt(HintIndex),
							DescribeProductRef(ProductIndex, Product),
							FText::FromString(Hint.Kind)));
					continue;
				}
				if (bIsNode && Hint.Var.IsEmpty())
				{
					Context.Sink.Warning(
						TEXT("DSH4328"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("LayoutHintNoVar", "Layout hint {0} of product {1} places a node but names no variable, so it places nothing."),
							TextFromInt(HintIndex),
							DescribeProductRef(ProductIndex, Product)));
				}
				if (bIsComment && Hint.Name.IsEmpty())
				{
					Context.Sink.Warning(
						TEXT("DSH4328"),
						Product.Source.Span,
						FText::Format(
							LOCTEXT("LayoutHintNoName", "Layout hint {0} of product {1} draws a comment box with no title, so it draws nothing."),
							TextFromInt(HintIndex),
							DescribeProductRef(ProductIndex, Product)));
				}
			}
		}

		static void ValidateProductOrderAndKeys(
			FIRValidationContext& Context,
			const FIRProduct& Product,
			const int32 ProductIndex)
		{
			const FIRGraph& Graph = Product.Graph;

			// Cycles. TopologicalOrder() returns fewer entries than there are nodes exactly when it
			// could not order them all, which is the definition of a cycle in this IR.
			const TArray<int32> Order = Graph.TopologicalOrder();
			if (Order.Num() != Graph.Nodes.Num())
			{
				TArray<bool> Ordered;
				Ordered.Init(false, Graph.Nodes.Num());
				for (const int32 NodeIndex : Order)
				{
					if (Ordered.IsValidIndex(NodeIndex))
					{
						Ordered[NodeIndex] = true;
					}
				}

				FString Unordered;
				int32 Reported = 0;
				for (int32 NodeIndex = 0; NodeIndex < Ordered.Num(); ++NodeIndex)
				{
					if (Ordered[NodeIndex])
					{
						continue;
					}
					if (Reported >= 8)
					{
						Unordered += TEXT(", ...");
						break;
					}
					if (Reported > 0)
					{
						Unordered += TEXT(", ");
					}
					Unordered += FString::Printf(TEXT("%%%d"), NodeIndex);
					++Reported;
				}

				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4316"),
					Product.Source.Span,
					FText::Format(
						LOCTEXT("GraphHasCycle", "Product {0} cannot be ordered: these nodes feed themselves, directly or through others -- {1}."),
						DescribeProductRef(ProductIndex, Product),
						FText::FromString(Unordered)));
			}

			// Dedupe keys: all or none, and no two the same.
			int32 KeyedNodes = 0;
			for (const FIRNode& Node : Graph.Nodes)
			{
				if (!Node.DedupeKey.IsEmpty())
				{
					++KeyedNodes;
				}
			}

			if (KeyedNodes != 0 && KeyedNodes != Graph.Nodes.Num())
			{
				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4321"),
					Product.Source.Span,
					FText::Format(
						LOCTEXT("PartialDedupeKeys", "Product {0} has dedupe keys on {1} of its {2} nodes; the key is either computed for the whole graph or for none of it."),
						DescribeProductRef(ProductIndex, Product),
						TextFromInt(KeyedNodes),
						TextFromInt(Graph.Nodes.Num())));
			}

			if (KeyedNodes == 0)
			{
				return;
			}

			// Only the nodes the dedupe pass is allowed to merge. A statement -- the sink, a
			// function output, a reflected custom-output node -- and a FunctionInput each own a
			// place in the graph that merging would corrupt, so the pass deliberately leaves two
			// identical ones alone (IRPasses.cpp, IsMergeable). Demanding they be merged here would
			// turn `UE.ClearCoatNormal(Input = n)` written twice into an internal-sounding error.
			TArray<int32> ByKey;
			ByKey.Reserve(Graph.Nodes.Num());
			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				const FIRNode& Node = Graph.Nodes[NodeIndex];
				if (Node.IsStatement() || Node.Op == EIROp::FunctionInput)
				{
					continue;
				}
				ByKey.Add(NodeIndex);
			}

			// Strcmp, not FString::operator<: the comparison has to be case-sensitive for the same
			// reason the keys themselves are.
			ByKey.Sort([&Graph](const int32 Left, const int32 Right)
			{
				const int32 Comparison = FCString::Strcmp(*Graph.Nodes[Left].DedupeKey, *Graph.Nodes[Right].DedupeKey);
				return Comparison != 0 ? (Comparison < 0) : (Left < Right);
			});

			for (int32 Index = 1; Index < ByKey.Num(); ++Index)
			{
				const int32 Previous = ByKey[Index - 1];
				const int32 Current = ByKey[Index];
				if (!Graph.Nodes[Previous].DedupeKey.Equals(Graph.Nodes[Current].DedupeKey, ESearchCase::CaseSensitive))
				{
					continue;
				}

				Context.bValid &= Context.Sink.Error(
					TEXT("DSH4322"),
					Graph.Nodes[Current].Source.Span,
					FText::Format(
						LOCTEXT("DuplicateDedupeKey", "Nodes %{0} and %{1} of product {2} have the same dedupe key, so the dedupe pass should have merged them into one."),
						TextFromInt(FMath::Min(Previous, Current)),
						TextFromInt(FMath::Max(Previous, Current)),
						DescribeProductRef(ProductIndex, Product)));
			}
		}
	}

	bool ValidateDreamShaderIR(
		const FIRModule& Module,
		const FBuiltinCatalog& Catalog,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		Private::FIRValidationContext Context{ Module, Catalog, Diagnostics };
		Context.bHasCatalog = !Catalog.IsEmpty();

		for (int32 ProductIndex = 0; ProductIndex < Module.Products.Num(); ++ProductIndex)
		{
			const FIRProduct& Product = Module.Products[ProductIndex];

			if (Product.Name.IsEmpty())
			{
				Context.bValid &= Diagnostics.Error(
					TEXT("DSH4326"),
					Product.Source.Span,
					FText::Format(
						LOCTEXT("ProductNoName", "Product {0} has no name; the asset name comes from the exported function or from '/// @name'."),
						Private::TextFromInt(ProductIndex)));
			}
			else
			{
				for (int32 Other = ProductIndex + 1; Other < Module.Products.Num(); ++Other)
				{
					if (Product.Name.Equals(Module.Products[Other].Name, ESearchCase::CaseSensitive))
					{
						Context.bValid &= Diagnostics.Error(
							TEXT("DSH4326"),
							Module.Products[Other].Source.Span,
							FText::Format(
								LOCTEXT("DuplicateProductName", "Products {0} and {1} would both be called '{2}'; one file cannot produce two assets of the same name."),
								Private::TextFromInt(ProductIndex),
								Private::TextFromInt(Other),
								FText::FromString(Product.Name)));
						break;
					}
				}
			}

			Private::ValidateProductStructure(Context, Product, ProductIndex);

			for (int32 NodeIndex = 0; NodeIndex < Product.Graph.Nodes.Num(); ++NodeIndex)
			{
				Private::ValidateNode(Context, Product, ProductIndex, NodeIndex);
			}

			Private::ValidateProductOrderAndKeys(Context, Product, ProductIndex);
		}

		return Context.bValid;
	}
}

#undef LOCTEXT_NAMESPACE
