// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// FunctionInput, FunctionOutput and MaterialFunctionCall.
//
// Two things here are load-bearing beyond "make a node":
//
// 1. Pin GUIDs. UMaterialExpressionMaterialFunctionCall matches a call site's wires by the stored
//    input/output Id first, and falls back to declaration order only when those do not resolve. A
//    rebuild that generates fresh Ids therefore silently rewires every caller. So the Ids of the
//    OLD nodes are cached before the rollback detaches them, and restored by name here.
//
// 2. SortPriority. UMaterialFunction::GetInputsAndOutputs sorts with an unstable introsort, so two
//    inputs sharing a priority come out in an order nothing in the source decides -- and that order
//    is the fallback the call sites use. The IR hands over dense, tie-free priorities
//    (CONTRACT §6.9, "Input/output SortPriority = declaration order, dense"), and they are written
//    through unchanged; the emitter does not re-derive them.

#include "Compiler/DreamShaderIREmitterInternal.h"

#include "DreamShaderModule.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"

#define LOCTEXT_NAMESPACE "DreamShader.Emitter"

namespace UE::DreamShader::Editor::Compiler
{
	bool FIREmitter::EmitFunctionInput(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		auto* Input = Cast<UMaterialExpressionFunctionInput>(
			CreateExpression(UMaterialExpressionFunctionInput::StaticClass(), Node));
		if (!Input)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("FunctionInputFailed", "Failed to create a FunctionInput node."));
		}

		FString InputName = Node.DebugName;
		if (const IR::FIRProperty* NameProperty = Node.FindProperty(IR::Prop::InputName))
		{
			InputName = NameProperty->Value.S;
		}
		Input->InputName = FName(*InputName);

		if (const IR::FIRProperty* TypeProperty = Node.FindProperty(IR::Prop::InputType))
		{
			int32 InputTypeValue = 0;
			if (!TryResolveFunctionInputTypeFromIR(TypeProperty->Value.S, InputTypeValue))
			{
				return Fail(TEXT("DSH8228"), Node, FText::Format(
					LOCTEXT("BadFunctionInputType", "'{0}' is not a material function input type; write one of Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, Texture2DArray, VolumeTexture, StaticBool, Bool, MaterialAttributes or Substrate."),
					FText::FromString(TypeProperty->Value.S)));
			}
			Input->InputType = static_cast<EFunctionInputType>(InputTypeValue);
		}

		if (const IR::FIRProperty* DescriptionProperty = Node.FindProperty(IR::Prop::Description))
		{
			Input->Description = DescriptionProperty->Value.S;
		}
		if (const IR::FIRProperty* SortProperty = Node.FindProperty(IR::Prop::SortPriority))
		{
			Input->SortPriority = static_cast<int32>(SortProperty->Value.I);
		}

		// An input that had a default in the source is optional, and its default is the preview
		// value the engine substitutes for an unconnected pin -- the two are one flag on this node,
		// which is why §6.9 says "a default -> IsOptional + PreviewValue".
		const IR::FIRProperty* OptionalProperty = Node.FindProperty(IR::Prop::IsOptional);
		const bool bOptional = OptionalProperty != nullptr && OptionalProperty->Value.B;
		Input->bUsePreviewValueAsDefault = bOptional ? 1U : 0U;
		if (const IR::FIRProperty* PreviewProperty = Node.FindProperty(IR::Prop::PreviewValue))
		{
			const IR::FIRPropertyValue& Preview = PreviewProperty->Value;
			Input->PreviewValue = FVector4f(
				static_cast<float>(Preview.V[0]),
				static_cast<float>(Preview.N > 1 ? Preview.V[1] : 0.0),
				static_cast<float>(Preview.N > 2 ? Preview.V[2] : 0.0),
				static_cast<float>(Preview.N > 3 ? Preview.V[3] : 0.0));
		}

		// Restore the Id the previous build gave this input, or make one. Copied in shape from the
		// file-static RestoreOrGenerateFunctionInputId in
		// MaterialAssetGeneration/DreamShaderMaterialGenerator.cpp (~1380): assign the cached Id if
		// the name is still there, then ConditionallyGenerateId(false), which fills in a fresh Id
		// only when there is none.
		if (const FGuid* ExistingId = ExistingInputIdsByName.Find(Input->InputName))
		{
			Input->Id = *ExistingId;
		}
		Input->ConditionallyGenerateId(false);

		FunctionInputExpressions.Add(Input);
		RegisterNode(NodeIndex, Node, Input);
		return true;
	}

	bool FIREmitter::EmitFunctionOutput(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		auto* Output = Cast<UMaterialExpressionFunctionOutput>(
			CreateExpression(UMaterialExpressionFunctionOutput::StaticClass(), Node));
		if (!Output)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("FunctionOutputFailed", "Failed to create a FunctionOutput node."));
		}

		FString OutputName = Node.DebugName;
		if (const IR::FIRProperty* NameProperty = Node.FindProperty(IR::Prop::OutputName))
		{
			OutputName = NameProperty->Value.S;
		}
		Output->OutputName = FName(*OutputName);

		if (const IR::FIRProperty* DescriptionProperty = Node.FindProperty(IR::Prop::Description))
		{
			Output->Description = DescriptionProperty->Value.S;
		}
		if (const IR::FIRProperty* SortProperty = Node.FindProperty(IR::Prop::SortPriority))
		{
			Output->SortPriority = static_cast<int32>(SortProperty->Value.I);
		}

		if (const FGuid* ExistingId = ExistingOutputIdsByName.Find(Output->OutputName))
		{
			Output->Id = *ExistingId;
		}
		Output->ConditionallyGenerateId(false);

		if (Node.Operands.Num() != 1)
		{
			return Fail(TEXT("DSH8234"), Node, FText::Format(
				LOCTEXT("FunctionOutputArity", "Function output '{0}' has {1} operands; it needs exactly the one value it returns."),
				FText::FromString(OutputName),
				FText::AsNumber(Node.Operands.Num())));
		}

		FEmittedValue Value;
		if (!ResolveOperand(Node, 0, Value))
		{
			return false;
		}

		// A named reroute in front of every function output, exactly as 1.x does
		// (DreamShaderMaterialGenerator.cpp ~2055: CreateOutputRerouteValue with the output's name and
		// its declaration index, so the pair is `DS_<Output>_<N>`). The MaterialSink already gets the
		// same treatment in DreamShaderIREmitterMaterial.cpp; without it here the parity dump of every
		// function is two nodes short, and the output node sits at the end of a long wire instead of
		// reading as its own name.
		Private::FCodeValue SourceValue;
		SourceValue.Expression = Value.Expression;
		SourceValue.OutputIndex = Value.OutputIndex;
		const Private::FCodeValue RoutedValue = Private::CreateOutputRerouteValue(
			nullptr,
			MaterialFunction,
			SourceValue,
			OutputName,
			Product.Graph.FunctionOutputs.Find(NodeIndex));

		FEmittedValue Routed;
		Routed.Expression = RoutedValue.Expression;
		Routed.OutputIndex = RoutedValue.OutputIndex;
		ConnectValueToInput(Output->A, Routed);

		FunctionOutputExpressions.Add(Output);
		RegisterNode(NodeIndex, Node, Output);
		return true;
	}

	bool FIREmitter::EmitFunctionCall(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		// Two ways a call names its callee. An `extern` call carries the asset path outright; a call
		// to another export of the SAME file carries a product index instead, because that asset's
		// path is not known until it has been emitted. The pipeline compiles products in dependency
		// order and hands the emitted paths back in the context, so by the time a caller is emitted
		// its callee is in the table (CONTRACT §2, "Helper vs export vs extern calls").
		FString FunctionPath;
		if (const IR::FIRProperty* LocalFunction = Node.FindProperty(IR::Prop::LocalFunction))
		{
			const int32 LocalProductIndex = static_cast<int32>(LocalFunction->Value.I);
			const FString* EmittedPath = Context.EmittedProductAssetPaths.Find(LocalProductIndex);
			if (!EmittedPath || EmittedPath->IsEmpty())
			{
				return Fail(TEXT("DSH8222"), Node, FText::Format(
					LOCTEXT("LocalFunctionNotEmitted", "This call targets product {0} of the same file, but that product has not been emitted yet; the pipeline must compile products in dependency order."),
					FText::AsNumber(LocalProductIndex)));
			}
			FunctionPath = *EmittedPath;
		}
		else if (const IR::FIRProperty* PathProperty = Node.FindProperty(IR::Prop::FunctionPath))
		{
			FunctionPath = PathProperty->Value.S;
		}
		else
		{
			FunctionPath = Node.ClassName;
		}

		FunctionPath.TrimStartAndEndInline();
		if (FunctionPath.IsEmpty())
		{
			return Fail(TEXT("DSH8219"), Node, LOCTEXT("FunctionCallNoPath", "This function call names no material function asset."));
		}

		UMaterialFunction* FunctionAsset = LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
		if (!FunctionAsset)
		{
			return Fail(TEXT("DSH8219"), Node, FText::Format(
				LOCTEXT("FunctionCallLoadFailed", "The material function asset '{0}' could not be loaded."),
				FText::FromString(FunctionPath)));
		}

		auto* Call = Cast<UMaterialExpressionMaterialFunctionCall>(
			CreateExpression(UMaterialExpressionMaterialFunctionCall::StaticClass(), Node));
		if (!Call)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("FunctionCallFailed", "Failed to create a MaterialFunctionCall node."));
		}

		// SetMaterialFunction is what populates FunctionInputs / FunctionOutputs from the asset, so
		// nothing below can resolve a pin until it has run.
		if (!Call->SetMaterialFunction(FunctionAsset))
		{
			return Fail(TEXT("DSH8219"), Node, FText::Format(
				LOCTEXT("FunctionCallAssignFailed", "'{0}' could not be assigned to the generated call node; a material function cannot call itself, directly or through another function."),
				FText::FromString(FunctionPath)));
		}

		for (const IR::FIRInput& Input : Node.Inputs)
		{
			int32 FunctionInputIndex = INDEX_NONE;
			for (int32 CandidateIndex = 0; CandidateIndex < Call->FunctionInputs.Num(); ++CandidateIndex)
			{
				const FFunctionExpressionInput& Candidate = Call->FunctionInputs[CandidateIndex];
				const FName CandidateName = Candidate.ExpressionInput
					? Candidate.ExpressionInput->InputName
					: Candidate.Input.InputName;
				if (CandidateName.ToString().Equals(Input.Pin, ESearchCase::CaseSensitive))
				{
					FunctionInputIndex = CandidateIndex;
					break;
				}
			}

			if (!Call->FunctionInputs.IsValidIndex(FunctionInputIndex))
			{
				return Fail(TEXT("DSH8220"), Node, FText::Format(
					LOCTEXT("FunctionCallUnknownInput", "'{0}' has no input named '{1}'."),
					FText::FromString(FunctionPath),
					FText::FromString(Input.Pin)));
			}

			FEmittedValue Value;
			if (!ResolveValue(Node, Input.Value, Value))
			{
				return false;
			}
			ConnectValueToInput(Call->FunctionInputs[FunctionInputIndex].Input, Value);
		}

		// Outputs by name, into the same slot -> engine index map every multi-output node uses. A
		// function's outputs are ordered by SortPriority on the asset, which is not necessarily the
		// order the IR listed them in, so the mapping is by name and never by position.
		TArray<int32> OutputIndices;
		OutputIndices.Reserve(Node.OutputNames.Num());
		for (int32 Slot = 0; Slot < Node.OutputNames.Num(); ++Slot)
		{
			int32 EngineIndex = INDEX_NONE;
			for (int32 CandidateIndex = 0; CandidateIndex < Call->FunctionOutputs.Num(); ++CandidateIndex)
			{
				const FFunctionExpressionOutput& Candidate = Call->FunctionOutputs[CandidateIndex];
				const FName CandidateName = Candidate.ExpressionOutput
					? Candidate.ExpressionOutput->OutputName
					: Candidate.Output.OutputName;
				if (CandidateName.ToString().Equals(Node.OutputNames[Slot], ESearchCase::CaseSensitive))
				{
					EngineIndex = CandidateIndex;
					break;
				}
			}

			if (EngineIndex == INDEX_NONE)
			{
				return Fail(TEXT("DSH8221"), Node, FText::Format(
					LOCTEXT("FunctionCallUnknownOutput", "'{0}' has no output named '{1}'."),
					FText::FromString(FunctionPath),
					FText::FromString(Node.OutputNames[Slot])));
			}

			OutputIndices.Add(EngineIndex);
		}

		RegisterNode(NodeIndex, Node, Call, MoveTemp(OutputIndices));
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
