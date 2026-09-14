// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Parameter and texture-parameter nodes, and the metadata that goes on them.
//
// The class a `uniform` becomes is settled by CONTRACT §6.1 and carried in the IR, not decided
// here: a static bool is a StaticBoolParameter, a float2..4 is a VectorParameter (with the builder's
// own Swizzle node after it for the narrow widths -- a float2 uniform is NOT a
// DoubleVectorParameter, which is what 1.x does and what the parity test measures), and everything
// else is a ScalarParameter.
//
// Metadata follows 1.x's ApplyExpressionMetadata rules
// (MaterialAssetGeneration/DreamShaderExpressionFactory.cpp ~172) with one deliberate difference:
// 1.x fails the whole compile on a metadata key that is not a reflected property, and §6.1 says a
// `@key` the engine does not know is a WARNING (DSH8210). So the passthrough loop is written here
// rather than delegated, and only the value-writing half is reused.

#include "Compiler/DreamShaderIREmitterInternal.h"

#include "DreamShaderModule.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "Engine/Texture.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DreamShader.Emitter"

namespace UE::DreamShader::Editor::Compiler
{
	namespace
	{
		/** The property names §6.1 gives a dedicated meaning; everything else on a Parameter node is a passthrough. */
		bool IsReservedParameterProperty(const FString& Name)
		{
			return Name.Equals(IR::Prop::ParameterName, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::Group, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::Description, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::SliderMin, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::SliderMax, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::SortPriority, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::DefaultValue, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::IsStatic, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::DefaultAsset, ESearchCase::CaseSensitive)
				|| Name.Equals(IR::Prop::SamplerType, ESearchCase::CaseSensitive);
		}

		/**
		 * The engine output index that carries the WHOLE of a parameter's value, or INDEX_NONE.
		 *
		 * UMaterialExpressionVectorParameter publishes RGB, R, G, B, A, RGBA -- its output 0 is a
		 * THREE-channel mask, not the float4 the IR's slot 0 names. The builder gives a vector
		 * uniform a float4 output and takes the declared width with a Swizzle after it
		 * (IRBuilderMaterial.cpp, CONTRACT 6.13 #22), so an identity slot -> index map wires slot 0
		 * to RGB: a `uniform float4` would silently lose its alpha, and a float3's `.xyz` could
		 * never take the named-output shortcut #22 describes, because that shortcut first asks
		 * whether the operand is the whole of its expression's output -- which output 0 is not.
		 *
		 * A class whose default output is unmasked (ScalarParameter, StaticBoolParameter,
		 * TextureObjectParameter) answers INDEX_NONE and keeps the identity map: its output 0
		 * already IS the whole value.
		 *
		 * Named for this file. The equivalent test in DreamShaderIREmitterNodes.cpp is
		 * IsLeadingChannelOutput, in an anonymous namespace of the SAME enclosing namespace; two
		 * anonymous namespaces at one scope in one translation unit are one namespace, and these
		 * two files sit in the same unity blob.
		 */
		int32 FindFullWidthParameterOutputIndex(const UMaterialExpression* Expression, const int32 Width)
		{
			if (!Expression || Width < 1 || Width > 4)
			{
				return INDEX_NONE;
			}

			const bool bExpected[4] = { Width >= 1, Width >= 2, Width >= 3, Width >= 4 };
			for (int32 Index = 0; Index < Expression->Outputs.Num(); ++Index)
			{
				const FExpressionOutput& Output = Expression->Outputs[Index];
				if (Output.Mask == 0)
				{
					// Unmasked: the whole value already, whatever its width. Identity is right.
					continue;
				}
				if ((Output.MaskR != 0) == bExpected[0]
					&& (Output.MaskG != 0) == bExpected[1]
					&& (Output.MaskB != 0) == bExpected[2]
					&& (Output.MaskA != 0) == bExpected[3])
				{
					return Index;
				}
			}

			return INDEX_NONE;
		}

		/** The declared name, or `@name` when the source renamed the parameter. */
		FString GetParameterName(const IR::FIRNode& Node)
		{
			if (const IR::FIRProperty* NameProperty = Node.FindProperty(IR::Prop::ParameterName))
			{
				const FString Trimmed = NameProperty->Value.S.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					return Trimmed;
				}
			}

			return Node.DebugName;
		}
	}

	bool FIREmitter::EmitParameter(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		const IR::FIRProperty* StaticProperty = Node.FindProperty(IR::Prop::IsStatic);
		const bool bIsStatic = StaticProperty != nullptr && StaticProperty->Value.B;

		const IR::FIRProperty* DefaultProperty = Node.FindProperty(IR::Prop::DefaultValue);

		// The width comes off Outputs[0], not off the default: a `uniform float3 c;` with no
		// initializer still has to be a VectorParameter, and a default that happens to carry four
		// components does not make a scalar uniform into a vector one.
		const int32 Components = Node.Outputs.IsValidIndex(0)
			? FMath::Clamp(Node.Outputs[0].GraphComponentCount(), 1, 4)
			: 1;

		UMaterialExpression* Expression = nullptr;

		if (bIsStatic)
		{
			auto* StaticBool = Cast<UMaterialExpressionStaticBoolParameter>(
				CreateExpression(UMaterialExpressionStaticBoolParameter::StaticClass(), Node));
			if (!StaticBool)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("StaticBoolParamFailed", "Failed to create a StaticBoolParameter node."));
			}
			StaticBool->DefaultValue = (DefaultProperty && DefaultProperty->Value.V[0] != 0.0) ? 1U : 0U;
			Expression = StaticBool;
		}
		else if (Components >= 2)
		{
			// VectorParameter even for a float2: 1.x emits VectorParameter + a ComponentMask, and
			// the IR builder has already put that Swizzle in, so emitting a narrower parameter class
			// here would leave the swizzle masking a value that no longer has those channels.
			auto* Vector = Cast<UMaterialExpressionVectorParameter>(
				CreateExpression(UMaterialExpressionVectorParameter::StaticClass(), Node));
			if (!Vector)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("VectorParamFailed", "Failed to create a VectorParameter node."));
			}
			if (DefaultProperty)
			{
				const IR::FIRPropertyValue& Default = DefaultProperty->Value;
				Vector->DefaultValue = FLinearColor(
					static_cast<float>(Default.V[0]),
					static_cast<float>(Default.N > 1 ? Default.V[1] : 0.0),
					static_cast<float>(Default.N > 2 ? Default.V[2] : 0.0),
					// A float2 or float3 default pads alpha with 1, as the 1.x generator's literals did.
					static_cast<float>(Default.N > 3 ? Default.V[3] : 1.0));
			}
			Expression = Vector;
		}
		else
		{
			auto* Scalar = Cast<UMaterialExpressionScalarParameter>(
				CreateExpression(UMaterialExpressionScalarParameter::StaticClass(), Node));
			if (!Scalar)
			{
				return Fail(TEXT("DSH8214"), Node, LOCTEXT("ScalarParamFailed", "Failed to create a ScalarParameter node."));
			}
			if (DefaultProperty)
			{
				Scalar->DefaultValue = static_cast<float>(DefaultProperty->Value.V[0]);
			}
			Expression = Scalar;
		}

		if (!ApplyParameterMetadata(Expression, Node))
		{
			return false;
		}

		// Slot 0 of the IR node is the parameter's whole value; on a VectorParameter that is the
		// RGBA output (index 5), never output 0. See FindFullWidthParameterOutputIndex.
		const int32 FullWidthIndex = FindFullWidthParameterOutputIndex(Expression, Components);
		if (FullWidthIndex != INDEX_NONE && FullWidthIndex != 0)
		{
			TArray<int32> OutputIndices;
			OutputIndices.Add(FullWidthIndex);
			RegisterNode(NodeIndex, Node, Expression, MoveTemp(OutputIndices));
			return true;
		}

		RegisterNode(NodeIndex, Node, Expression);
		return true;
	}

	bool FIREmitter::EmitTextureParameter(const int32 NodeIndex, const IR::FIRNode& Node)
	{
		// TextureObjectParameter rather than TextureSampleParameter2D: a 2.0 texture uniform is a
		// texture OBJECT that a separate TextureSample node reads (CONTRACT §6.5), so one uniform
		// sampled twice is one parameter and two samples rather than two parameters.
		auto* Texture = Cast<UMaterialExpressionTextureObjectParameter>(
			CreateExpression(UMaterialExpressionTextureObjectParameter::StaticClass(), Node));
		if (!Texture)
		{
			return Fail(TEXT("DSH8214"), Node, LOCTEXT("TextureParamFailed", "Failed to create a TextureObjectParameter node."));
		}

		if (const IR::FIRProperty* DefaultAsset = Node.FindProperty(IR::Prop::DefaultAsset))
		{
			const FString AssetPath = DefaultAsset->Value.S.TrimStartAndEnd();
			if (!AssetPath.IsEmpty())
			{
				UTexture* LoadedTexture = LoadObject<UTexture>(nullptr, *AssetPath);
				if (!LoadedTexture)
				{
					return Fail(TEXT("DSH8218"), Node, FText::Format(
						LOCTEXT("TextureDefaultMissing", "The default texture for parameter '{0}' could not be loaded from '{1}'."),
						FText::FromString(GetParameterName(Node)),
						FText::FromString(AssetPath)));
				}
				Texture->Texture = LoadedTexture;
			}
		}

		if (const IR::FIRProperty* SamplerType = Node.FindProperty(IR::Prop::SamplerType))
		{
			UEnum* SamplerEnum = StaticEnum<EMaterialSamplerType>();
			int64 EnumValue = INDEX_NONE;
			if (!SamplerEnum || !Private::TryResolveEnumLiteral(SamplerEnum, SamplerType->Value.S, EnumValue))
			{
				return Fail(TEXT("DSH8235"), Node, FText::Format(
					LOCTEXT("BadSamplerType", "'{0}' is not a sampler type; write one of the EMaterialSamplerType names, such as Color, Normal or LinearColor."),
					FText::FromString(SamplerType->Value.S)));
			}
			Texture->SamplerType = static_cast<EMaterialSamplerType>(EnumValue);
		}

		if (!ApplyParameterMetadata(Texture, Node))
		{
			return false;
		}

		RegisterNode(NodeIndex, Node, Texture);
		return true;
	}

	bool FIREmitter::ApplyParameterMetadata(UMaterialExpression* Expression, const IR::FIRNode& Node)
	{
		if (!Expression)
		{
			return true;
		}

		UClass* ExpressionClass = Expression->GetClass();

		// ParameterName goes through reflection rather than through a cast to
		// UMaterialExpressionParameter, for the same reason 1.x's SetExpressionParameterName does:
		// the texture parameter classes carry their own ParameterName and are not Parameter
		// subclasses, so a cast would silently skip them.
		const FString ParameterName = GetParameterName(Node);
		if (!ParameterName.IsEmpty())
		{
			if (FProperty* NameProperty = Private::FindMaterialExpressionArgumentProperty(ExpressionClass, TEXT("ParameterName")))
			{
				FDreamShaderError NameError;
				if (!Private::SetMaterialExpressionLiteralProperty(Expression, NameProperty, ParameterName, NameError))
				{
					return Fail(TEXT("DSH8213"), Node, FText::Format(
						LOCTEXT("ParameterNameFailed", "'{0}' could not take '{1}' as its parameter name. {2}"),
						FText::FromString(ExpressionClass->GetName()),
						FText::FromString(ParameterName),
						FText::FromString(NameError.Message)));
				}
			}
			else
			{
				Warn(TEXT("DSH8210"), Node, FText::Format(
					LOCTEXT("NoParameterNameProperty", "'{0}' exposes no ParameterName, so the name '{1}' was not written."),
					FText::FromString(ExpressionClass->GetName()),
					FText::FromString(ParameterName)));
			}
		}

		// Group / Desc / SortPriority / SliderMin / SliderMax. `Desc` is the engine's spelling of a
		// description on a UMaterialExpression, and `Group`/`SortPriority` live on
		// UMaterialExpressionParameter -- a parameter class that is not one of those (the engine has
		// a couple) simply does not get them, with a warning rather than a failed compile, which is
		// the same call 1.x makes for its "soft organization fields".
		struct FMappedProperty
		{
			const TCHAR* IRName;
			const TCHAR* EngineName;
		};
		static const FMappedProperty MappedProperties[] =
		{
			{ IR::Prop::Group,        TEXT("Group") },
			{ IR::Prop::Description,  TEXT("Desc") },
			{ IR::Prop::SortPriority, TEXT("SortPriority") },
			{ IR::Prop::SliderMin,    TEXT("SliderMin") },
			{ IR::Prop::SliderMax,    TEXT("SliderMax") },
		};

		for (const FMappedProperty& Mapped : MappedProperties)
		{
			const IR::FIRProperty* Property = Node.FindProperty(Mapped.IRName);
			if (!Property)
			{
				continue;
			}

			FProperty* Target = Private::FindMaterialExpressionArgumentProperty(ExpressionClass, Mapped.EngineName);
			if (!Target)
			{
				Warn(TEXT("DSH8210"), Node, FText::Format(
					LOCTEXT("NoOrganizationField", "'{0}' exposes no '{1}' field, so that value was not written."),
					FText::FromString(ExpressionClass->GetName()),
					FText::FromString(Mapped.EngineName)));
				continue;
			}

			FDreamShaderError LiteralError;
			const FString ValueText = FormatIRPropertyForReflection(Property->Value);
			if (!Private::SetMaterialExpressionLiteralProperty(Expression, Target, ValueText, LiteralError))
			{
				return Fail(TEXT("DSH8213"), Node, FText::Format(
					LOCTEXT("OrganizationFieldFailed", "'{0}' could not take '{1}' for its '{2}' field. {3}"),
					FText::FromString(ExpressionClass->GetName()),
					FText::FromString(ValueText),
					FText::FromString(Mapped.EngineName),
					FText::FromString(LiteralError.Message)));
			}
		}

		// Everything else the doc block carried: an unknown `@key` is a passthrough that goes to the
		// expression by its engine name, and a key reflection does not know is DSH8210 -- a warning,
		// because a `@key` the author wrote for a different engine version should not stop a build.
		for (const IR::FIRProperty& Property : Node.Properties)
		{
			if (IsReservedParameterProperty(Property.Name))
			{
				continue;
			}

			if (!ApplyReflectedProperty(Node, Expression, Property, /*bWarnWhenMissing*/ true))
			{
				return false;
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
