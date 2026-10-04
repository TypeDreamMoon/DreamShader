// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The parameter schema a `.dsi` binds against, produced from the parent's IR (producer A), and the small
// helpers IR.h declares for it: FIRParameterSchema::Find / FindIgnoreCase / MakeFingerprint and the
// EIRParameterKind names.
//
// What a parent's parameters ARE is what its source lowers to: the Parameter and TextureParameter nodes
// of the material product after the passes, read the way the emitter reads them
// (DreamShaderIREmitterParameters.cpp) -- a static bool is a StaticSwitch, a scalar a Scalar, anything
// wider a four-channel Vector, a texture a Texture. The bound module, when the host has it, adds each
// uniform's declared type and declaration span, and the uniforms the product has no parameter for
// (pruned, or never read at all) as `bPruned` entries, so the binder can say why an override of one is
// unknown (DSH7258).

#include "IR/IRInstanceSchema.h"

#include "IR/IR.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CString.h"

namespace UE::DreamShader::IR
{
	namespace IRInstanceSchemaPrivate
	{
		const TCHAR* LexIRInstanceSchemaAssociation(const EIRParameterAssociation Association)
		{
			switch (Association)
			{
			case EIRParameterAssociation::Global: return TEXT("Global");
			case EIRParameterAssociation::Layer:  return TEXT("Layer");
			case EIRParameterAssociation::Blend:  return TEXT("Blend");
			}
			return TEXT("Global");
		}

		/** The engine parameter name of a bound uniform: `@name`, else the identifier (ApplyParameterMetadata's rule). */
		FString IRInstanceSchemaParameterName(const Lang::FBoundGlobal& Global)
		{
			return Global.Directives.Name.IsEmpty() ? Global.Name : Global.Directives.Name;
		}

		const Lang::FBoundGlobal* FindIRInstanceSchemaUniform(const Lang::FBoundModule* ParentBound, const FString& ParameterName)
		{
			if (!ParentBound)
			{
				return nullptr;
			}
			for (const Lang::FBoundGlobal& Global : ParentBound->Globals)
			{
				if (Global.bIsParameter && IRInstanceSchemaParameterName(Global).Equals(ParameterName, ESearchCase::CaseSensitive))
				{
					return &Global;
				}
			}
			return nullptr;
		}

		/** The kind the emitter gives a `.dss` uniform; false for a uniform that becomes no parameter (a sampler). */
		bool TryGetIRInstanceSchemaUniformKind(const Lang::FBoundGlobal& Global, EIRParameterKind& OutKind)
		{
			const FIRType& Type = Global.Type;
			if (Type.IsTexture())
			{
				OutKind = EIRParameterKind::Texture;
				return true;
			}
			if (Type.IsBool() && Type.Rows == 1 && Type.Cols == 1 && Global.Directives.bStatic)
			{
				OutKind = EIRParameterKind::StaticSwitch;
				return true;
			}
			if ((Type.IsNumeric() || Type.IsBool()) && Type.Cols == 1)
			{
				OutKind = Type.Rows <= 1 ? EIRParameterKind::Scalar : EIRParameterKind::Vector;
				return true;
			}
			return false;
		}

		/**
		 * A default as an instance sees it, in FIRInstanceOverride::Value's encoding: a Scalar is Float4 N=1, a
		 * Vector is Float4 N=4 with the channels its declared width did not write padded the emitter's way
		 * (zero, alpha one), a StaticSwitch is a Bool.
		 */
		FIRPropertyValue MakeIRInstanceSchemaValue(const EIRParameterKind Kind, const double* Components, const int32 Num)
		{
			switch (Kind)
			{
			case EIRParameterKind::StaticSwitch:
				return FIRPropertyValue::MakeBool(Num > 0 && Components[0] != 0.0);
			case EIRParameterKind::Scalar:
			{
				const double Scalar[4] = { Num > 0 ? Components[0] : 0.0, 0.0, 0.0, 0.0 };
				return FIRPropertyValue::MakeFloat4(Scalar, 1);
			}
			case EIRParameterKind::Vector:
			case EIRParameterKind::DoubleVector:
			{
				double Channels[4] = { 0.0, 0.0, 0.0, 1.0 };
				for (int32 Index = 0; Index < FMath::Min(Num, 4); ++Index)
				{
					Channels[Index] = Components[Index];
				}
				return FIRPropertyValue::MakeFloat4(Channels, 4);
			}
			case EIRParameterKind::StaticComponentMask:
			{
				double Mask[4] = { 0.0, 0.0, 0.0, 0.0 };
				for (int32 Index = 0; Index < FMath::Min(Num, 4); ++Index)
				{
					Mask[Index] = Components[Index] != 0.0 ? 1.0 : 0.0;
				}
				return FIRPropertyValue::MakeFloat4(Mask, 4);
			}
			case EIRParameterKind::Texture:
			case EIRParameterKind::TextureCollection:
			case EIRParameterKind::Font:
			case EIRParameterKind::RuntimeVirtualTexture:
			case EIRParameterKind::SparseVolumeTexture:
			case EIRParameterKind::ParameterCollection:
				return FIRPropertyValue::MakeObject(FString());
			}
			return FIRPropertyValue();
		}

		void FillIRInstanceSchemaDeclaration(FIRParameterSchemaEntry& Entry, const Lang::FBoundGlobal& Global)
		{
			Entry.DeclaredType = Global.Type;
			Entry.DeclFile = Global.File;
			Entry.DeclSpan = Global.Decl ? Global.Decl->Declarator.NameSpan : Lang::FLangSpan();
		}
	}

	// ------------------------------------------------------------------------------ kind names

	const TCHAR* LexToString(const EIRParameterKind Kind)
	{
		switch (Kind)
		{
		case EIRParameterKind::Scalar:                return TEXT("Scalar");
		case EIRParameterKind::Vector:                return TEXT("Vector");
		case EIRParameterKind::DoubleVector:          return TEXT("DoubleVector");
		case EIRParameterKind::Texture:               return TEXT("Texture");
		case EIRParameterKind::TextureCollection:     return TEXT("TextureCollection");
		case EIRParameterKind::Font:                  return TEXT("Font");
		case EIRParameterKind::RuntimeVirtualTexture: return TEXT("RuntimeVirtualTexture");
		case EIRParameterKind::SparseVolumeTexture:   return TEXT("SparseVolumeTexture");
		case EIRParameterKind::StaticSwitch:          return TEXT("StaticSwitch");
		case EIRParameterKind::ParameterCollection:   return TEXT("ParameterCollection");
		case EIRParameterKind::StaticComponentMask:   return TEXT("StaticComponentMask");
		}
		return TEXT("Scalar");
	}

	bool TryParseParameterKind(const FString& Text, EIRParameterKind& OutKind)
	{
		static const EIRParameterKind Kinds[] =
		{
			EIRParameterKind::Scalar,
			EIRParameterKind::Vector,
			EIRParameterKind::DoubleVector,
			EIRParameterKind::Texture,
			EIRParameterKind::TextureCollection,
			EIRParameterKind::Font,
			EIRParameterKind::RuntimeVirtualTexture,
			EIRParameterKind::SparseVolumeTexture,
			EIRParameterKind::StaticSwitch,
			EIRParameterKind::ParameterCollection,
			EIRParameterKind::StaticComponentMask,
		};
		for (const EIRParameterKind Kind : Kinds)
		{
			if (Text.Equals(LexToString(Kind), ESearchCase::CaseSensitive))
			{
				OutKind = Kind;
				return true;
			}
		}
		return false;
	}

	// ---------------------------------------------------------------------------------- schema

	int32 FIRParameterSchema::Find(const FString& Name, const EIRParameterAssociation Association, const int32 AssociationIndex) const
	{
		for (int32 Index = 0; Index < Parameters.Num(); ++Index)
		{
			const FIRParameterSchemaEntry& Entry = Parameters[Index];
			if (Entry.Association == Association
				&& Entry.AssociationIndex == AssociationIndex
				&& Entry.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FIRParameterSchema::FindIgnoreCase(const FString& Name) const
	{
		// A global parameter first: that is what a `.dsi` can name, so it is the likelier meaning.
		int32 Found = INDEX_NONE;
		for (int32 Index = 0; Index < Parameters.Num(); ++Index)
		{
			if (!Parameters[Index].Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (Parameters[Index].Association == EIRParameterAssociation::Global)
			{
				return Index;
			}
			if (Found == INDEX_NONE)
			{
				Found = Index;
			}
		}
		return Found;
	}

	FString FIRParameterSchema::MakeFingerprint() const
	{
		TArray<FString> Lines;
		Lines.Reserve(Parameters.Num());
		for (const FIRParameterSchemaEntry& Entry : Parameters)
		{
			Lines.Add(FString::Printf(
				TEXT("%s|%s|%d|%s|%s"),
				*Entry.Name,
				IRInstanceSchemaPrivate::LexIRInstanceSchemaAssociation(Entry.Association),
				Entry.AssociationIndex,
				LexToString(Entry.Kind),
				*Entry.DeclaredType.ToString()));
		}
		// Strcmp, not FString's operator<: the order has to be case-sensitive to be stable.
		Lines.Sort([](const FString& Left, const FString& Right)
		{
			return FCString::Strcmp(*Left, *Right) < 0;
		});
		return FString::Join(Lines, TEXT("\n"));
	}

	// -------------------------------------------------------------------------------- producer A

	bool BuildParameterSchemaFromIR(
		const FIRModule& ParentModule,
		const int32 ProductIndex,
		const Lang::FBoundModule* ParentBound,
		FIRParameterSchema& OutSchema)
	{
		using namespace IRInstanceSchemaPrivate;

		OutSchema = FIRParameterSchema();
		if (!ParentModule.Products.IsValidIndex(ProductIndex))
		{
			return false;
		}

		const FIRProduct& Product = ParentModule.Products[ProductIndex];
		switch (Product.Kind)
		{
		case EIRProductKind::MaterialInstance:
		{
			// Instance of an instance: the root's parameter set, with the parent instance's values overlaid.
			// The parent instance's own object path is the host's to fill; this module cannot know it.
			OutSchema = Product.Instance.ParentSchema;
			OutSchema.bParentIsInstance = true;
			OutSchema.ParentObjectPath.Reset();
			OutSchema.ParentSourceFile = ParentModule.SourceFilePath;
			for (const FIRInstanceOverride& Override : Product.Instance.Overrides)
			{
				const int32 Index = OutSchema.Find(Override.ParameterName, Override.Association, Override.AssociationIndex);
				if (Index != INDEX_NONE)
				{
					OutSchema.Parameters[Index].ParentValue = Override.Value;
					OutSchema.Parameters[Index].ParentFontPage = Override.FontPage;
				}
			}
			return OutSchema.bValid;
		}
		case EIRProductKind::Material:
			break;
		case EIRProductKind::MaterialFunction:
		case EIRProductKind::MaterialLayer:
		case EIRProductKind::MaterialLayerBlend:
		case EIRProductKind::PassPipeline:
			// Nothing instances a function, a layer, a blend or a pipeline.
			return false;
		}

		OutSchema.bValid = true;
		OutSchema.Origin = TEXT("ir");
		OutSchema.ParentSourceFile = ParentModule.SourceFilePath;

		for (const FIRNode& Node : Product.Graph.Nodes)
		{
			if (Node.Op != EIROp::Parameter && Node.Op != EIROp::TextureParameter)
			{
				continue;
			}
			const FIRProperty* NameProperty = Node.FindProperty(Prop::ParameterName);
			if (!NameProperty || NameProperty->Value.S.IsEmpty())
			{
				continue;
			}
			const FString& Name = NameProperty->Value.S;
			if (OutSchema.Find(Name, EIRParameterAssociation::Global, INDEX_NONE) != INDEX_NONE)
			{
				// One entry per parameter: a graph passed without dedupe may still hold two nodes of one name.
				continue;
			}

			FIRParameterSchemaEntry Entry;
			Entry.Name = Name;
			if (Node.Op == EIROp::TextureParameter)
			{
				Entry.Kind = EIRParameterKind::Texture;
				Entry.TextureKind = Node.Outputs.Num() > 0 ? Node.Outputs[0].Texture : Lang::ETextureKind::None;
				const FIRProperty* Asset = Node.FindProperty(Prop::DefaultAsset);
				Entry.ParentValue = FIRPropertyValue::MakeObject(Asset ? Asset->Value.S : FString());
			}
			else
			{
				const FIRProperty* Static = Node.FindProperty(Prop::IsStatic);
				const FIRProperty* Default = Node.FindProperty(Prop::DefaultValue);
				const int32 Width = Node.Outputs.Num() > 0 ? FMath::Max(Node.Outputs[0].GraphComponentCount(), 1) : 1;
				Entry.Kind = (Static && Static->Value.B)
					? EIRParameterKind::StaticSwitch
					: (Width <= 1 ? EIRParameterKind::Scalar : EIRParameterKind::Vector);
				const double NoDefault[4] = { 0.0, 0.0, 0.0, 0.0 };
				Entry.ParentValue = (Default && Default->Value.Kind == EIRPropertyKind::Float4)
					? MakeIRInstanceSchemaValue(Entry.Kind, Default->Value.V, FMath::Clamp(Default->Value.N, 1, 4))
					: MakeIRInstanceSchemaValue(Entry.Kind, NoDefault, 1);
			}
			if (const FIRProperty* Group = Node.FindProperty(Prop::Group))
			{
				Entry.Group = Group->Value.S;
			}
			if (const FIRProperty* Sort = Node.FindProperty(Prop::SortPriority))
			{
				Entry.SortPriority = static_cast<int32>(Sort->Value.I);
			}
			if (const Lang::FBoundGlobal* Global = FindIRInstanceSchemaUniform(ParentBound, Name))
			{
				FillIRInstanceSchemaDeclaration(Entry, *Global);
			}
			OutSchema.Parameters.Add(MoveTemp(Entry));
		}

		// The uniforms the parent declares with no parameter in this product: pruned by the passes (DSH4390), or
		// never read at all and so never lowered. Listed so an override of one is told why it is unknown.
		const auto AddPruned = [&OutSchema, ParentBound](const FString& Name, const Lang::FBoundGlobal* Global)
		{
			if (Name.IsEmpty() || OutSchema.Find(Name, EIRParameterAssociation::Global, INDEX_NONE) != INDEX_NONE)
			{
				return;
			}
			FIRParameterSchemaEntry Entry;
			Entry.Name = Name;
			Entry.bPruned = true;
			if (Global)
			{
				EIRParameterKind Kind = EIRParameterKind::Scalar;
				if (!TryGetIRInstanceSchemaUniformKind(*Global, Kind))
				{
					return;
				}
				Entry.Kind = Kind;
				Entry.TextureKind = Global->Type.IsTexture() ? Global->Type.Texture : Lang::ETextureKind::None;
				Entry.Group = Global->Directives.Group;
				Entry.SortPriority = Global->Directives.bHasSort ? Global->Directives.Sort : 0;
				FillIRInstanceSchemaDeclaration(Entry, *Global);

				const Lang::FExpr* Initializer = Global->Decl ? Global->Decl->Declarator.Initializer.Get() : nullptr;
				const Lang::FBoundExpr* Folded = (ParentBound && Initializer) ? ParentBound->Find(*Initializer) : nullptr;
				if (Kind == EIRParameterKind::Texture)
				{
					Entry.ParentValue = FIRPropertyValue::MakeObject(Global->Directives.DefaultAsset);
				}
				else if (Folded && Folded->bIsConstant)
				{
					Entry.ParentValue = MakeIRInstanceSchemaValue(Kind, Folded->ConstantValue, FMath::Clamp(Global->Type.GraphComponentCount(), 1, 4));
				}
				else
				{
					const double NoDefault[4] = { 0.0, 0.0, 0.0, 0.0 };
					Entry.ParentValue = MakeIRInstanceSchemaValue(Kind, NoDefault, 1);
				}
			}
			OutSchema.Parameters.Add(MoveTemp(Entry));
		};

		for (const FString& Name : Product.Graph.PrunedParameters)
		{
			AddPruned(Name, FindIRInstanceSchemaUniform(ParentBound, Name));
		}
		if (ParentBound)
		{
			for (const Lang::FBoundGlobal& Global : ParentBound->Globals)
			{
				if (Global.bIsParameter)
				{
					AddPruned(IRInstanceSchemaParameterName(Global), &Global);
				}
			}
		}

		return true;
	}
}
