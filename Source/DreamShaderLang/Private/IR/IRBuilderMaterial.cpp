// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The four product shapes, the `material` field map, and uniforms.
//
// A `material` is a map from attribute name to value for as long as it can be (contract section
// 6.2). It becomes a node only when it has to: at the end of an Entry the map's entries are the
// MaterialSink's inputs directly -- no MakeMaterialAttributes, which is what the 1.x generator
// produces and what an author expects to see -- and everywhere else (a Layer's output, a material
// crossing into a pin) it becomes SetMaterialAttributes over the material it came from, or
// MakeMaterialAttributes when it came from nowhere. A material that came out of a node -- a
// reflected MaterialAttributes output, a function call's `material` -- is a map with that output as
// its source, like a layer's input, and an Entry left holding one gives its sink the whole set.
//
// Diagnostics owned by this file: DSH4352, DSH4364, DSH4370.

#include "IRBuilderInternal.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "DreamShader.IRBuilder"

namespace UE::DreamShader::IR::Private
{
	static int32 IndexOfFunction(const FBoundModule& Module, const FBoundFunction& Function)
	{
		const int32 Index = static_cast<int32>(&Function - Module.Functions.GetData());
		return Module.Functions.IsValidIndex(Index) ? Index : INDEX_NONE;
	}

	/** The constant an initializer folded to, laid out the way a FunctionInput preview value wants it. */
	static void SpreadPreviewValue(const double* Components, int32 Num, double* OutValue)
	{
		OutValue[0] = 0.0;
		OutValue[1] = 0.0;
		OutValue[2] = 0.0;
		OutValue[3] = 1.0;
		if (Num <= 1)
		{
			// A scalar default fills every channel, as 1.x's ApplyFunctionInputPreviewDefault does.
			OutValue[0] = OutValue[1] = OutValue[2] = OutValue[3] = Components[0];
			return;
		}
		for (int32 Index = 0; Index < FMath::Min(Num, 4); ++Index)
		{
			OutValue[Index] = Components[Index];
		}
	}

	// ------------------------------------------------------------------------------- products

	void FIRBuilder::BuildEntryProduct(const FBoundFunction& Function)
	{
		if (!Function.Decl || !Function.Decl->Body)
		{
			return;
		}

		PushFrame(Function, IndexOfFunction(BoundModule, Function));

		const int32 MaterialParam = Function.MaterialResultParam != INDEX_NONE ? Function.MaterialResultParam : 0;
		if (Frame().Params.IsValidIndex(MaterialParam))
		{
			// The entry's material starts empty: every attribute is write-then-read, and what is
			// never written is never connected, which is what leaves the rest of the material's pins
			// at their defaults.
			Frame().Params[MaterialParam] = FLoweredValue::MakeMaterial();
		}

		LowerBlock(*Function.Decl->Body);
		ResolveExits();

		if (Frame().Params.IsValidIndex(MaterialParam) && Frame().Params[MaterialParam].IsMaterial())
		{
			EmitMaterialSink(Frame().Params[MaterialParam].Material, Function.Decl->Span);
		}
		else
		{
			EmitMaterialSink(FMaterialValue(), Function.Decl->Span);
		}

		PopFrame();
	}

	void FIRBuilder::BuildLayerProduct(const FBoundFunction& Function, bool bIsBlend)
	{
		if (!Function.Decl || !Function.Decl->Body)
		{
			return;
		}

		PushFrame(Function, IndexOfFunction(BoundModule, Function));

		const int32 ResultParam = Function.MaterialResultParam != INDEX_NONE ? Function.MaterialResultParam : 0;

		// Every `material` parameter of a layer or a layer blend is a MaterialAttributes
		// FunctionInput -- the `inout` result included, because `inout` means the caller's material
		// arrives and leaves. Reading an attribute of one makes GetMaterialAttributes against it;
		// writing one records it in the map and the output below sets it back.
		int32 SortPriority = 0;
		for (int32 Index = 0; Index < Function.Params.Num(); ++Index)
		{
			if (Function.Params[Index].Direction == EParamDirection::Out)
			{
				continue;
			}
			Frame().Params[Index] = MakeFunctionInput(Function, Index, SortPriority++);
		}

		LowerBlock(*Function.Decl->Body);
		ResolveExits();

		const FString OutputName = Function.Params.IsValidIndex(ResultParam) && !Function.Params[ResultParam].Name.IsEmpty()
			? Function.Params[ResultParam].Name
			: TEXT("Material");

		FLoweredValue Result = Frame().Params.IsValidIndex(ResultParam) ? Frame().Params[ResultParam] : FLoweredValue();
		MakeFunctionOutput(OutputName, Function.Directives.FindParamDoc(OutputName) ? *Function.Directives.FindParamDoc(OutputName) : FString(), 0, Result, Function.Decl->Span);

		(void)bIsBlend;
		PopFrame();
	}

	void FIRBuilder::BuildFunctionProduct(const FBoundFunction& Function)
	{
		if (!Function.Decl || !Function.Decl->Body)
		{
			return;
		}

		PushFrame(Function, IndexOfFunction(BoundModule, Function));

		// Inputs in declaration order with dense SortPriority (plan section 6.2). An `out` parameter
		// is not an input; an `inout` one is both.
		int32 InputSort = 0;
		for (int32 Index = 0; Index < Function.Params.Num(); ++Index)
		{
			if (Function.Params[Index].Direction == EParamDirection::Out)
			{
				continue;
			}
			Frame().Params[Index] = MakeFunctionInput(Function, Index, InputSort++);
		}

		LowerBlock(*Function.Decl->Body);
		ResolveExits();

		int32 OutputSort = 0;
		if (!Function.ReturnType.IsVoid())
		{
			const FString* Doc = Function.Directives.FindParamDoc(TEXT("Result"));
			MakeFunctionOutput(TEXT("Result"), Doc ? *Doc : FString(), OutputSort++, Frame().ReturnValue, Function.Decl->Span);
		}
		for (int32 Index = 0; Index < Function.Params.Num(); ++Index)
		{
			if (Function.Params[Index].Direction == EParamDirection::In)
			{
				continue;
			}
			MakeFunctionOutput(
				Function.Params[Index].Name,
				Function.Params[Index].Doc,
				OutputSort++,
				Frame().Params.IsValidIndex(Index) ? Frame().Params[Index] : FLoweredValue(),
				Function.Decl->Span);
		}

		PopFrame();
	}

	// ------------------------------------------------------------------------- function inputs

	FString FIRBuilder::FunctionInputTypeName(const FIRType& Type, const FLangSpan& Span)
	{
		switch (Type.Kind)
		{
		case EIRTypeKind::Material:
			return TEXT("MaterialAttributes");
		case EIRTypeKind::Substrate:
			return TEXT("Substrate");
		case EIRTypeKind::Texture:
			switch (Type.Texture)
			{
			case ETextureKind::TextureCube: return TEXT("TextureCube");
			case ETextureKind::Texture2DArray: return TEXT("Texture2DArray");
			case ETextureKind::Texture3D:
			case ETextureKind::VolumeTexture: return TEXT("VolumeTexture");
			default: return TEXT("Texture2D");
			}
		case EIRTypeKind::Bool:
		case EIRTypeKind::Int:
		case EIRTypeKind::UInt:
		case EIRTypeKind::Float:
		case EIRTypeKind::Half:
		case EIRTypeKind::Double:
			switch (Type.GraphComponentCount())
			{
			case 2: return TEXT("Vector2");
			case 3: return TEXT("Vector3");
			case 4: return TEXT("Vector4");
			case 1: return TEXT("Scalar");
			default: break;
			}
			break;
		default:
			break;
		}

		Diagnostics.Error(TEXT("DSH4364"), Span, FText::Format(
			LOCTEXT("IRBuilderNoInputType", "A parameter of type {0} cannot be a material function input; the graph has no pin that carries one."),
			FText::FromString(Type.ToString())));
		return FString();
	}

	FLoweredValue FIRBuilder::MakeFunctionInput(const FBoundFunction& Function, int32 ParamIndex, int32 SortPriority)
	{
		if (!Function.Params.IsValidIndex(ParamIndex))
		{
			return FLoweredValue();
		}

		const FBoundParam& Param = Function.Params[ParamIndex];
		const FLangSpan Span = Function.Decl ? Function.Decl->Span : FLangSpan();

		const FString InputType = FunctionInputTypeName(Param.Type, Span);
		if (InputType.IsEmpty())
		{
			return FLoweredValue();
		}

		FIRNode Node;
		Node.Op = EIROp::FunctionInput;
		Node.Outputs.Add(GraphTypeOf(Param.Type));
		Node.DebugName = Param.Name;
		Node.Properties.Add({ FString(Prop::InputName), FIRPropertyValue::MakeName(Param.Name) });
		Node.Properties.Add({ FString(Prop::InputType), FIRPropertyValue::MakeEnum(InputType) });
		Node.Properties.Add({ FString(Prop::SortPriority), FIRPropertyValue::MakeInt(SortPriority) });
		if (!Param.Doc.IsEmpty())
		{
			Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Param.Doc) });
		}

		const bool bOptional = Param.bOptional || Param.Default != nullptr;
		Node.Properties.Add({ FString(Prop::IsOptional), FIRPropertyValue::MakeBool(bOptional) });
		if (Param.Default)
		{
			if (const FBoundExpr* BoundDefault = Bound(*Param.Default))
			{
				if (BoundDefault->bIsConstant)
				{
					double Preview[4] = { 0.0, 0.0, 0.0, 1.0 };
					SpreadPreviewValue(BoundDefault->ConstantValue, FMath::Max(BoundDefault->Type.GraphComponentCount(), 1), Preview);
					Node.Properties.Add({ FString(Prop::PreviewValue), FIRPropertyValue::MakeFloat4(Preview, 4) });
				}
			}
		}

		const FIRValue Value = AddNode(MoveTemp(Node), Span);
		if (Graph)
		{
			Graph->FunctionInputs.Add(Value.Node);
		}

		if (Param.Type.IsMaterial())
		{
			// A material that arrived on a pin: the map starts empty and remembers where it came
			// from, so a read of an attribute nobody wrote breaks the incoming attributes open.
			return FLoweredValue::MakeMaterial(Value);
		}
		return FLoweredValue::Of(Value);
	}

	void FIRBuilder::MakeFunctionOutput(const FString& Name, const FString& Description, int32 SortPriority, const FLoweredValue& Value, const FLangSpan& Span)
	{
		FIRValue Operand = FIRValue::None();
		if (Value.IsMaterial())
		{
			Operand = MaterialiseMaterial(Value.Material, Span);
		}
		else if (Value.IsValue())
		{
			Operand = Value.Value;
		}

		FIRNode Node;
		Node.Op = EIROp::FunctionOutput;
		Node.Operands.Add(Operand);
		Node.DebugName = Name;
		Node.Properties.Add({ FString(Prop::OutputName), FIRPropertyValue::MakeName(Name) });
		Node.Properties.Add({ FString(Prop::SortPriority), FIRPropertyValue::MakeInt(SortPriority) });
		if (!Description.IsEmpty())
		{
			Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Description) });
		}

		const FIRValue Result = AddNode(MoveTemp(Node), Span);
		if (Graph)
		{
			Graph->FunctionOutputs.Add(Result.Node);
		}
	}

	// ------------------------------------------------------------------------- the material map

	bool FIRBuilder::ReadMaterialField(FMaterialValue& Material, int32 AttributeIndex, const FLangSpan& Span, FIRValue& Out)
	{
		const FCatalogMaterialAttribute* Entry = Attribute(AttributeIndex);
		if (!Entry)
		{
			// Silent when there is no catalog at all (reported once for the module); an index the
			// catalog does not have means the bind and the build read different tables.
			if (Catalog)
			{
				Diagnostics.Error(TEXT("DSH4352"), Span, FText::Format(
					LOCTEXT("IRBuilderReadNoAttributeEntry", "The material attribute table has no entry {0}; the module was bound against a different catalog than this build is reading."),
					FText::AsNumber(AttributeIndex)));
			}
			return false;
		}

		if (const FIRValue* Existing = Material.Fields.Find(Entry->Name))
		{
			Out = *Existing;
			return true;
		}

		if (Material.HasSource())
		{
			if (Material.BreakNode == INDEX_NONE)
			{
				FIRNode Node;
				Node.Op = EIROp::GetMaterialAttributes;
				Node.Inputs.Add({ TEXT("MaterialAttributes"), Material.Source });
				for (const FCatalogMaterialAttribute& Candidate : Catalog->MaterialAttributes)
				{
					Node.Outputs.Add(Candidate.ValueType);
					Node.OutputNames.Add(Candidate.Name);
				}
				Material.BreakNode = AddNode(MoveTemp(Node), Span).Node;
			}
			Out = FIRValue{ Material.BreakNode, AttributeIndex };
			return true;
		}

		Diagnostics.Error(TEXT("DSH4370"), Span, FText::Format(
			LOCTEXT("IRBuilderAttributeNotSet", "'{0}' is read before anything wrote it; a material attribute has no value until this function assigns one."),
			FText::FromString(Entry->Name)));
		return false;
	}

	FIRValue FIRBuilder::MakeSetMaterialAttributes(const FMaterialValue& Material, const FLangSpan& Span)
	{
		FIRNode Node;
		Node.Op = EIROp::SetMaterialAttributes;
		Node.Inputs.Add({ TEXT("MaterialAttributes"), Material.Source });
		Node.Outputs.Add(FIRType::Material());

		TArray<FString> AttributeNames;
		AttributeNames.Reserve(Material.Order.Num());
		for (const FString& Name : Material.Order)
		{
			if (const FIRValue* Value = Material.Fields.Find(Name))
			{
				Node.Inputs.Add({ Name, *Value });
				AttributeNames.Add(Name);
			}
		}
		// Parallel to Inputs[1..]: the engine node pairs AttributeSetTypes[i] with Inputs[i + 1].
		Node.Properties.Add({ FString(Prop::AttributeSetTypes), FIRPropertyValue::MakeStringList(AttributeNames) });

		return AddNode(MoveTemp(Node), Span);
	}

	FIRValue FIRBuilder::MaterialiseMaterial(const FMaterialValue& Material, const FLangSpan& Span)
	{
		if (Material.HasSource())
		{
			// Nothing was written: the value IS the material that came in.
			return Material.Order.IsEmpty() ? Material.Source : MakeSetMaterialAttributes(Material, Span);
		}

		FIRNode Node;
		Node.Op = EIROp::MakeMaterialAttributes;
		Node.Outputs.Add(FIRType::Material());
		for (const FString& Name : Material.Order)
		{
			if (const FIRValue* Value = Material.Fields.Find(Name))
			{
				Node.Inputs.Add({ Name, *Value });
			}
		}
		return AddNode(MoveTemp(Node), Span);
	}

	void FIRBuilder::EmitMaterialSink(const FMaterialValue& Material, const FLangSpan& Span)
	{
		FIRNode Node;
		Node.Op = EIROp::MaterialSink;

		// A material replaced as a whole on the way here -- `m = UE.BlendMaterialAttributes(...)`, or
		// an `inout material` a function call wrote back -- reaches the end of the entry as an
		// attribute SET with a source, not as a bare map, and wiring the map's entries alone would
		// drop everything that source carries without a word. A set has one way into a material: its
		// MaterialAttributes input, with whatever was written on top folded into a
		// SetMaterialAttributes. That is the 1.x generator's shape for a MaterialAttributes output too
		// (the emitter turns bUseMaterialAttributes on for it), and it is why nothing is wired beside
		// it: a material that reads its attributes as a set no longer looks at the individual pins.
		if (Material.HasSource() && Catalog)
		{
			const int32 WholeSet = Catalog->FindMaterialAttribute(TEXT("MaterialAttributes"));
			if (WholeSet != INDEX_NONE)
			{
				const FIRValue Set = MaterialiseMaterial(Material, Span);
				Node.Inputs.Add({ Catalog->MaterialAttributes[WholeSet].Name, Set });
			}
			else
			{
				Diagnostics.Error(TEXT("DSH4352"), Span, LOCTEXT("IRBuilderNoWholeSetAttribute",
					"The material attribute table has no 'MaterialAttributes' entry, so a material that was replaced as a whole has no input to reach this material's output through; export the catalog again from this engine."));
			}
		}
		else
		{
			for (const FString& Name : Material.Order)
			{
				if (const FIRValue* Value = Material.Fields.Find(Name))
				{
					Node.Inputs.Add({ Name, *Value });
				}
			}
		}

		const FIRValue Sink = AddNode(MoveTemp(Node), Span);
		if (Graph)
		{
			Graph->Sink = Sink.Node;
		}
	}

	// --------------------------------------------------------------------------------- uniforms

	void FIRBuilder::ApplyParameterMetadata(FIRNode& Node, const FBoundDirectives& Directives, const FString& FallbackName, int32 DefaultSortPriority)
	{
		const FString ParameterName = Directives.Name.IsEmpty() ? FallbackName : Directives.Name;
		Node.Properties.Add({ FString(Prop::ParameterName), FIRPropertyValue::MakeName(ParameterName) });

		if (!Directives.Group.IsEmpty())
		{
			Node.Properties.Add({ FString(Prop::Group), FIRPropertyValue::MakeName(Directives.Group) });
		}
		const FString Description = Directives.Desc.IsEmpty() ? Directives.FreeText : Directives.Desc;
		if (!Description.IsEmpty())
		{
			Node.Properties.Add({ FString(Prop::Description), FIRPropertyValue::MakeString(Description) });
		}
		if (Directives.bHasSlider)
		{
			Node.Properties.Add({ FString(Prop::SliderMin), FIRPropertyValue::MakeFloat(Directives.SliderMin) });
			Node.Properties.Add({ FString(Prop::SliderMax), FIRPropertyValue::MakeFloat(Directives.SliderMax) });
		}
		if (Directives.bHasSort)
		{
			Node.Properties.Add({ FString(Prop::SortPriority), FIRPropertyValue::MakeInt(Directives.Sort) });
		}
		else if (DefaultSortPriority != INDEX_NONE)
		{
			// Without `@sort`, declaration order (the language's rule, and DSH7221's): the engine sorts equal
			// priorities by name, so leaving its default of 32 on every parameter would list them alphabetically.
			Node.Properties.Add({ FString(Prop::SortPriority), FIRPropertyValue::MakeInt(DefaultSortPriority) });
		}

		// A directive the language does not define rides along by its key; the emitter writes it onto
		// the parameter expression by reflection, as the 1.x generator does with unknown metadata.
		for (const TPair<FString, FString>& Passthrough : Directives.Passthrough)
		{
			Node.Properties.Add({ Passthrough.Key, FIRPropertyValue::MakeString(Passthrough.Value) });
		}
	}

	FLoweredValue FIRBuilder::MakeGlobalValue(int32 GlobalIndex, const FLangSpan& Span)
	{
		const FBoundGlobal& Global = BoundModule.Globals[GlobalIndex];
		const FLangSpan DeclSpan = Global.Decl ? Global.Decl->Span : Span;

		// The uniform's place among the uniforms, dense, for the SortPriority of one without `@sort`.
		int32 DeclarationOrder = INDEX_NONE;
		if (Global.bIsParameter)
		{
			DeclarationOrder = 0;
			for (int32 Earlier = 0; Earlier < GlobalIndex; ++Earlier)
			{
				DeclarationOrder += BoundModule.Globals[Earlier].bIsParameter ? 1 : 0;
			}
		}

		const FExpr* Initializer = Global.Decl ? Global.Decl->Declarator.Initializer.Get() : nullptr;
		const FBoundExpr* BoundInitializer = Initializer ? Bound(*Initializer) : nullptr;

		// ----- `static const`: one Constant node with the value the binder folded.
		if (Global.bIsConstant && !Global.Type.IsTexture())
		{
			const int32 Width = FMath::Max(Global.Type.GraphComponentCount(), 1);
			double Components[4] = { 0.0, 0.0, 0.0, 0.0 };
			if (BoundInitializer && BoundInitializer->bIsConstant)
			{
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Components[Index] = BoundInitializer->ConstantValue[Index];
				}
			}
			return FLoweredValue::Of(MakeConstant(Components, Width, DeclSpan));
		}

		// ----- a texture, uniform or const: one TextureParameter either way; a const one simply has
		// no name the material instance can override, which the emitter turns into a TextureObject.
		if (Global.Type.IsTexture())
		{
			FIRNode Node;
			Node.Op = EIROp::TextureParameter;
			Node.Outputs.Add(Global.Type);
			Node.DebugName = Global.Name;
			ApplyParameterMetadata(Node, Global.Directives, Global.Name, DeclarationOrder);
			if (!Global.Directives.DefaultAsset.IsEmpty())
			{
				Node.Properties.Add({ FString(Prop::DefaultAsset), FIRPropertyValue::MakeObject(Global.Directives.DefaultAsset) });
			}
			// The sampler type travels with the texture: every TextureSample that reads this
			// parameter copies it off the node rather than guessing.
			if (!Global.Directives.Sampler.IsEmpty())
			{
				Node.Properties.Add({ FString(Prop::SamplerType), FIRPropertyValue::MakeEnum(Global.Directives.Sampler) });
			}
			return FLoweredValue::Of(AddNode(MoveTemp(Node), DeclSpan));
		}

		// ----- a static bool: a StaticBoolParameter, which is the only thing a StaticSwitch accepts.
		const int32 Width = FMath::Max(Global.Type.GraphComponentCount(), 1);
		double Default[4] = { 0.0, 0.0, 0.0, 0.0 };
		if (BoundInitializer && BoundInitializer->bIsConstant)
		{
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Default[Index] = BoundInitializer->ConstantValue[Index];
			}
		}

		FIRNode Node;
		Node.Op = EIROp::Parameter;
		Node.DebugName = Global.Name;

		if (Global.Type.IsBool() && Global.Directives.bStatic)
		{
			Node.Outputs.Add(FIRType::Bool(1));
			Node.Properties.Add({ FString(Prop::IsStatic), FIRPropertyValue::MakeBool(true) });
		}
		else
		{
			// A scalar is a ScalarParameter and its output is the value itself; anything wider is a
			// VectorParameter, whose own output is the whole float4, so the declared width is taken
			// with a mask below. That is 1.x's shape: it selects the RG / RGB output of the same
			// node, which is what the emitter turns an ascending mask back into.
			Node.Outputs.Add(FIRType::Float(Width == 1 ? 1 : 4));
		}

		ApplyParameterMetadata(Node, Global.Directives, Global.Name, DeclarationOrder);
		Node.Properties.Add({ FString(Prop::DefaultValue), FIRPropertyValue::MakeFloat4(Default, Width) });

		const FIRValue Parameter = AddNode(MoveTemp(Node), DeclSpan);
		if (Width > 1 && Width < 4)
		{
			return FLoweredValue::Of(MakeSwizzle(Parameter, FString(GSwizzleComponents).Left(Width), DeclSpan));
		}
		return FLoweredValue::Of(Parameter);
	}
}

#undef LOCTEXT_NAMESPACE
