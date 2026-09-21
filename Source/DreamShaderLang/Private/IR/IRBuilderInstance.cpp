// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The MaterialInstance product of a `.dsi` (batch 2; research-instance section 3.5, CONTRACT-UNITS A2).
//
// An instance is a set of assignments against the parent's parameters, not a graph: the product's Graph
// stays empty and no `.dsi` uniform ever becomes a Parameter node. FBoundModule::Instance and
// FBoundModule::ParentSchema are the only inputs. Each override's value is its folded initializer (or its
// `@default` asset) laid out the way FIRInstanceOverride::Value documents; a vector narrower than four
// channels takes the rest from the parent's value, which is what the engine keeps when an instance sets a
// VectorParameter from a float3.

#include "IRBuilderInternal.h"

#include "Math/UnrealMathUtility.h"

namespace UE::DreamShader::IR::Private
{
	void FIRBuilder::BuildInstanceProduct(const FBoundProduct& BoundProduct, FIRProduct& OutProduct)
	{
		const FBoundInstance& Instance = BoundModule.Instance;
		const FIRParameterSchema* Schema = BoundModule.ParentSchema;

		OutProduct.Graph = FIRGraph();
		OutProduct.Backend = BoundProduct.Backend;
		OutProduct.Source.File = StampFile(SourceFile);
		OutProduct.Source.Span = Instance.Pragma ? Instance.Pragma->Span : FLangSpan();

		FIRInstance& Out = OutProduct.Instance;
		Out.ParentReference = Instance.ParentReference;
		// FBoundInstance has no resolved path of its own; the schema names the parent it describes (see the SE
		// report, "Contract changes needed"). The host may overwrite it after the build.
		Out.ParentObjectPath = Schema ? Schema->ParentObjectPath : FString();
		Out.Settings = Instance.Settings;
		if (Schema)
		{
			Out.ParentSchema = *Schema;
		}

		for (const FBoundInstanceOverride& BoundOverride : Instance.Overrides)
		{
			if (!BoundModule.Globals.IsValidIndex(BoundOverride.GlobalIndex))
			{
				continue;
			}
			const FBoundGlobal& Global = BoundModule.Globals[BoundOverride.GlobalIndex];

			FIRInstanceOverride Override;
			Override.ParameterName = BoundOverride.ParameterName;
			Override.VariableName = Global.Name;
			Override.Kind = BoundOverride.Kind;
			Override.Association = EIRParameterAssociation::Global;
			Override.AssociationIndex = INDEX_NONE;
			Override.FontPage = BoundOverride.FontPage;
			Override.DeclaredType = Global.Type;
			Override.Source.File = StampFile(Global.File.IsEmpty() ? SourceFile : Global.File);
			Override.Source.Span = Global.Decl ? Global.Decl->Span : FLangSpan();

			const FExpr* Initializer = Global.Decl ? Global.Decl->Declarator.Initializer.Get() : nullptr;
			const FBoundExpr* Folded = Initializer ? Bound(*Initializer) : nullptr;
			const bool bConstant = Folded != nullptr && Folded->bIsConstant;
			const FIRParameterSchemaEntry* Entry = (Schema && Schema->Parameters.IsValidIndex(BoundOverride.SchemaIndex))
				? &Schema->Parameters[BoundOverride.SchemaIndex]
				: nullptr;

			switch (Override.Kind)
			{
			case EIRParameterKind::Texture:
			case EIRParameterKind::TextureCollection:
			case EIRParameterKind::Font:
			case EIRParameterKind::RuntimeVirtualTexture:
			case EIRParameterKind::SparseVolumeTexture:
			case EIRParameterKind::ParameterCollection:
			{
				// `/// @default None` is an explicit None: the override clears the parent's asset.
				const FString& Asset = Global.Directives.DefaultAsset;
				Override.Value = FIRPropertyValue::MakeObject(Asset.Equals(TEXT("None"), ESearchCase::IgnoreCase) ? FString() : Asset);
				break;
			}

			case EIRParameterKind::StaticSwitch:
				Override.Value = FIRPropertyValue::MakeBool(bConstant && Folded->ConstantValue[0] != 0.0);
				break;

			case EIRParameterKind::StaticComponentMask:
			{
				double Mask[4] = { 0.0, 0.0, 0.0, 0.0 };
				for (int32 Channel = 0; Channel < 4; ++Channel)
				{
					Mask[Channel] = (bConstant && Folded->ConstantValue[Channel] != 0.0) ? 1.0 : 0.0;
				}
				Override.Value = FIRPropertyValue::MakeFloat4(Mask, 4);
				break;
			}

			case EIRParameterKind::Scalar:
			{
				const double Scalar[4] = { bConstant ? Folded->ConstantValue[0] : 0.0, 0.0, 0.0, 0.0 };
				Override.Value = FIRPropertyValue::MakeFloat4(Scalar, 1);
				break;
			}

			case EIRParameterKind::Vector:
			case EIRParameterKind::DoubleVector:
			{
				// The declared width's channels come from the initializer; the rest are the parent's, or zero with
				// an alpha of one (the emitter's padding) when the parent is not known.
				double Channels[4] = { 0.0, 0.0, 0.0, 1.0 };
				if (Entry && Entry->ParentValue.Kind == EIRPropertyKind::Float4)
				{
					for (int32 Channel = 0; Channel < FMath::Clamp(Entry->ParentValue.N, 1, 4); ++Channel)
					{
						Channels[Channel] = Entry->ParentValue.V[Channel];
					}
				}
				const int32 Written = FMath::Clamp(Global.Type.GraphComponentCount(), 1, 4);
				if (bConstant)
				{
					for (int32 Channel = 0; Channel < Written; ++Channel)
					{
						Channels[Channel] = Folded->ConstantValue[Channel];
					}
				}
				Override.Value = FIRPropertyValue::MakeFloat4(Channels, 4);
				break;
			}
			}

			Out.Overrides.Add(MoveTemp(Override));
		}
	}
}
