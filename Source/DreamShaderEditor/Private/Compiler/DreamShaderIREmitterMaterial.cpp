// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The material's own surface: Make / Set / Get MaterialAttributes and the MaterialSink.
//
// The sink is the one node with no expression of its own. A material's outputs are pins on the
// UMaterial, not on a node in its graph, so a MaterialSink is emitted by connecting each of its
// named inputs to Material->GetExpressionInputForProperty(...). That is also why CONTRACT §6.2 has
// the entry's field map become the sink's inputs directly rather than a MakeMaterialAttributes node:
// the 1.x generator wires `Base.BaseColor = x` straight onto the property pin, and anything else
// would show up as an extra node in the parity diff.
//
// Make / Set / Get exist for the cases where a material value has to cross into a pin: passed to a
// reflected node, returned from a Layer, read out of a LayerBlend's input.

#include "Compiler/DreamShaderIREmitterInternal.h"

#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorCodeShared.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"

#define LOCTEXT_NAMESPACE "DreamShader.Emitter"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		/** The input name a SetMaterialAttributes node reserves for the material it is modifying. */
		const TCHAR* const MaterialAttributesPinName = TEXT("MaterialAttributes");

		/**
		 * The pin spellings a MakeMaterialAttributes attribute can have, in the order to try them.
		 *
		 * The node names its pins after the attributes, but not identically: the eight customized
		 * UVs are one `CustomizedUVs[8]` array property rather than eight separate pins, so
		 * `CustomizedUVs3` has to become `CustomizedUVs[3]` before reflection can find it.
		 *
		 * Why reflection and not UMaterialExpressionMakeMaterialAttributes::GetExpressionInput,
		 * which maps EMaterialProperty to the pin directly and would need none of this: the class is
		 * UCLASS(MinimalAPI) and that method carries no ENGINE_API, so calling it from a plugin
		 * links against a symbol Engine.dll does not export. It compiles clean and fails at link
		 * time, which is the failure mode DREAMSHADER_ENGINE_EXPRESSION_CLASS exists to warn about.
		 */
		void CollectMakeAttributePinCandidates(const FString& AttributeName, TArray<FString>& OutCandidates)
		{
			OutCandidates.Reset();
			OutCandidates.Add(AttributeName);

			// CustomizedUVs3 / CustomizedUV3 -> CustomizedUVs[3].
			FString Stem = AttributeName;
			if (Stem.StartsWith(TEXT("CustomizedUV"), ESearchCase::CaseSensitive))
			{
				const FString Digits = Stem.RightChop(Stem.StartsWith(TEXT("CustomizedUVs"), ESearchCase::CaseSensitive) ? 13 : 12);
				if (!Digits.IsEmpty() && Digits.IsNumeric())
				{
					OutCandidates.Add(FString::Printf(TEXT("CustomizedUVs[%s]"), *Digits)); /* I18N-EXEMPT: pin identifier */
				}
			}
		}
	}

	bool FIREmitter::EmitMakeMaterialAttributes(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		auto* Make = Cast<UMaterialExpressionMakeMaterialAttributes>(
			CreateExpression(UMaterialExpressionMakeMaterialAttributes::StaticClass(), Node));
		if (!Make)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("MakeAttributesFailed", "Failed to create a MakeMaterialAttributes node."));
		}

		TArray<FString> PinCandidates;
		for (const IR::FIRInput& Input : Node.Inputs)
		{
			// Resolved for its side effect: an attribute name the catalog does not know is reported
			// as an unknown attribute (DSH8216) rather than as a missing pin, which is the more
			// useful of the two messages when the source misspelled it.
			EMaterialProperty Property = MP_MAX;
			if (!ResolveMaterialAttribute(Node, Input.Pin, Property))
			{
				return false;
			}

			FEmittedValue Value;
			if (!ResolveValue(Node, Input.Value, Value))
			{
				return false;
			}

			CollectMakeAttributePinCandidates(Input.Pin, PinCandidates);
			bool bConnected = false;
			for (const FString& Candidate : PinCandidates)
			{
				if (TryConnectNamedInput(Make, Candidate, Value))
				{
					bConnected = true;
					break;
				}
			}

			if (!bConnected)
			{
				return Fail(TEXT("DSH8212"), Node, FText::Format(
					LOCTEXT("MakeAttributesNoPin", "MakeMaterialAttributes has no pin for the attribute '{0}'."),
					FText::FromString(Input.Pin)));
			}
		}

		RegisterNode(NodeIndex, Node, Make);
		return true;
	}

	bool FIREmitter::EmitSetMaterialAttributes(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		auto* Set = Cast<UMaterialExpressionSetMaterialAttributes>(
			CreateExpression(UMaterialExpressionSetMaterialAttributes::StaticClass(), Node));
		if (!Set)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("SetAttributesFailed", "Failed to create a SetMaterialAttributes node."));
		}

		// Input 0 is the material being modified and is not an attribute; the engine keeps it
		// implicit, which is why it has no entry in AttributeSetTypes.
		const IR::FIRInput* BaseInput = Node.FindInput(MaterialAttributesPinName);
		if (!BaseInput)
		{
			return Fail(TEXT("DSH8224"), Node, LOCTEXT("SetAttributesNoBase",
				"A SetMaterialAttributes node has no MaterialAttributes input; there is nothing for it to modify."));
		}

		FEmittedValue BaseValue;
		if (!ResolveValue(Node, BaseInput->Value, BaseValue))
		{
			return false;
		}
		if (!Set->Inputs.IsValidIndex(0))
		{
			Set->Inputs.Add(FExpressionInput());
		}
		ConnectValueToInput(Set->Inputs[0], BaseValue);

		// AttributeSetTypes is the node's own record of which attribute each extra pin drives, and
		// it has to grow in step with Inputs. ConnectDreamShaderSetMaterialAttributeInput is the 1.x
		// helper that does both halves together, including the display name the pin shows.
		for (const IR::FIRInput& Input : Node.Inputs)
		{
			if (Input.Pin.Equals(MaterialAttributesPinName, ESearchCase::CaseSensitive))
			{
				continue;
			}

			EMaterialProperty Property = MP_MAX;
			if (!ResolveMaterialAttribute(Node, Input.Pin, Property))
			{
				return false;
			}

			FEmittedValue Value;
			if (!ResolveValue(Node, Input.Value, Value))
			{
				return false;
			}

			if (!Private::ConnectDreamShaderSetMaterialAttributeInput(Set, Property, Value.Expression, Value.OutputIndex))
			{
				return Fail(TEXT("DSH8212"), Node, FText::Format(
					LOCTEXT("SetAttributesConnectFailed", "SetMaterialAttributes could not take a value for the attribute '{0}'."),
					FText::FromString(Input.Pin)));
			}
		}

		RegisterNode(NodeIndex, Node, Set);
		return true;
	}

	bool FIREmitter::EmitGetMaterialAttributes(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		// Break, not Get: BreakMaterialAttributes publishes every attribute as a fixed output, which
		// is what the IR's OutputNames describe, while GetMaterialAttributes carries its own
		// AttributeGetTypes list and would need the emitter to decide which attributes to expose.
		auto* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(
			CreateExpression(UMaterialExpressionBreakMaterialAttributes::StaticClass(), Node));
		if (!Break)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("BreakAttributesFailed", "Failed to create a BreakMaterialAttributes node."));
		}

		const IR::FIRInput* BaseInput = Node.FindInput(MaterialAttributesPinName);
		if (!BaseInput)
		{
			return Fail(TEXT("DSH8224"), Node, LOCTEXT("BreakAttributesNoBase",
				"A GetMaterialAttributes node has no MaterialAttributes input; there is nothing for it to read."));
		}

		FEmittedValue BaseValue;
		if (!ResolveValue(Node, BaseInput->Value, BaseValue))
		{
			return false;
		}
		ConnectValueToInput(Break->MaterialAttributes, BaseValue);

		// Output slot -> engine output index, by attribute. The IR builder gives this node one slot per
		// attribute in the catalog, and the catalog carries attributes BreakMaterialAttributes has no
		// pin for: the `MaterialAttributes` pseudo-attribute the reflection filler always adds, plus
		// FrontMaterial and SurfaceThickness. So a slot that maps to no pin is INDEX_NONE rather than a
		// failed node -- almost every slot goes unread -- and ResolveValue raises DSH8216 at the one
		// read of an unmapped slot, with that read's span. (Failing here failed every read of a
		// material that came through a pin, whichever attribute it read.)
		//
		// Three ways to the pin, in order. The attribute's own name, because the engine names the Break
		// outputs after the attributes (`EmissiveColor`, `ShadingModel`). The display name 1.x matched
		// first, which inside a material is the spaced pin label (`Emissive Color`, from
		// GetAttributeOverrideForMaterialInLock) and so only ever matched a one-word attribute there.
		// And the 1.x literal-index table, for CustomData0/1, whose pins are `ClearCoat` /
		// `ClearCoatRoughness`.
		TArray<int32> OutputIndices;
		OutputIndices.Reserve(Node.OutputNames.Num());
		for (int32 Slot = 0; Slot < Node.OutputNames.Num(); ++Slot)
		{
			const FString& AttributeName = Node.OutputNames[Slot];
			int32 EngineIndex = INDEX_NONE;
			for (int32 CandidateIndex = 0; CandidateIndex < Break->Outputs.Num(); ++CandidateIndex)
			{
				if (Break->Outputs[CandidateIndex].OutputName.ToString().Equals(AttributeName, ESearchCase::CaseSensitive))
				{
					EngineIndex = CandidateIndex;
					break;
				}
			}

			EMaterialProperty Property = MP_MAX;
			if (!Break->Outputs.IsValidIndex(EngineIndex)
				&& Context.Catalog
				&& TryResolveMaterialPropertyFromCatalog(*Context.Catalog, AttributeName, Property))
			{
				const FGuid AttributeId = FMaterialAttributeDefinitionMap::GetID(Property);
				const FString DisplayName =
					FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(AttributeId, Break->Material).ToString();
				for (int32 CandidateIndex = 0; CandidateIndex < Break->Outputs.Num(); ++CandidateIndex)
				{
					if (Break->Outputs[CandidateIndex].OutputName.ToString().Equals(DisplayName, ESearchCase::IgnoreCase))
					{
						EngineIndex = CandidateIndex;
						break;
					}
				}

				if (!Break->Outputs.IsValidIndex(EngineIndex)
					&& !Private::TryResolveMaterialAttributesBreakOutputIndex(Property, EngineIndex))
				{
					EngineIndex = INDEX_NONE;
				}
			}

			OutputIndices.Add(Break->Outputs.IsValidIndex(EngineIndex) ? EngineIndex : INDEX_NONE);
		}

		RegisterNode(NodeIndex, Node, Break, MoveTemp(OutputIndices));
		return true;
	}

	bool FIREmitter::EmitMaterialSink(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		if (!Material)
		{
			return Fail(TEXT("DSH8233"), Node, LOCTEXT("SinkWithoutMaterial",
				"This graph carries a MaterialSink, which only a material product has; a material function drives FunctionOutput nodes instead."));
		}

		for (const IR::FIRInput& Input : Node.Inputs)
		{
			EMaterialProperty Property = MP_MAX;
			if (!ResolveMaterialAttribute(Node, Input.Pin, Property))
			{
				return false;
			}

			FExpressionInput* MaterialInput = Material->GetExpressionInputForProperty(Property);
			if (!MaterialInput)
			{
				return Fail(TEXT("DSH8217"), Node, FText::Format(
					LOCTEXT("SinkNoPropertyInput", "This material has no input for the attribute '{0}'; check the material domain and shading model the file asks for."),
					FText::FromString(Input.Pin)));
			}

			// Writing the whole attribute set at once is the one case where the material stops
			// reading its individual property pins, so the flag has to be set or the graph compiles
			// to the defaults with no error.
			if (Property == MP_MaterialAttributes)
			{
				Material->bUseMaterialAttributes = true;
			}

			FEmittedValue Value;
			if (!ResolveValue(Node, Input.Value, Value))
			{
				return false;
			}

			// A named reroute in front of every material output, exactly as 1.x does: the pin then
			// reads as the attribute's own name in the graph, and the long wire from wherever the
			// value was computed does not cross the whole canvas.
			const Private::FCodeValue SourceValue = [&Value]()
			{
				Private::FCodeValue CodeValue;
				CodeValue.Expression = Value.Expression;
				CodeValue.OutputIndex = Value.OutputIndex;
				return CodeValue;
			}();
			const Private::FCodeValue RoutedValue = Private::CreateOutputRerouteValue(
				Material,
				nullptr,
				SourceValue,
				Input.Pin,
				static_cast<int32>(Property));

			FEmittedValue Routed;
			Routed.Expression = RoutedValue.Expression;
			Routed.OutputIndex = RoutedValue.OutputIndex;
			ConnectValueToInput(*MaterialInput, Routed);
		}

		// The sink itself is not a node, so nothing is registered for it: it has no outputs, and a
		// layout entry or a region box for it would name an expression that does not exist.
		EmittedNodes[NodeIndex].Expression = nullptr;
		EmittedNodes[NodeIndex].OutputIndices.Reset();
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
