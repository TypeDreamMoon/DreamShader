// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// BuildDreamShaderAstFromIR, the module: everything a `.dss` declares once, however many graphs use it.
//
//   #pragma material(...)        the material product's settings
//   extern prototypes            one per function asset the graphs call, from the interfaces the importer handed over
//   uniforms                     every Parameter / TextureParameter of every graph, merged by parameter name
//   `/// @custom` functions      read back out of the Custom nodes' code
//   the products                 `export void M(inout material m)`, exported functions, layers, blends
//   #pragma layout(...)          node positions and comment boxes
//
// A `uniform float3` is a float4 VectorParameter with a leading mask after it (IRBuilderMaterial.cpp MakeGlobalValue),
// so the declared width is not in the parameter: it is read off how the graphs use it. A parameter every reader of
// which takes a leading mask, and whose default carries the padding the emitter writes for that width, is declared
// that narrow; anything else is a float4.

// First, as the build tool asks of a .cpp that has a header of its own name.
#include "Decompile/IRToAst.h"

#include "IRToAstInternal.h"

#include "IR/IRCustomHlsl.h"
#include "IR/IRTypes.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangToken.h"
#include "Misc/Crc.h"
#include "Semantic/LangBound.h"

#define LOCTEXT_NAMESPACE "DreamShaderIRToAst"

namespace UE::DreamShader::Lang::DecompileAst
{
	// ------------------------------------------------------------------------------------ helpers

	static bool IsGraphProduct(const FIRProduct& Product)
	{
		return Product.Kind != IR::EIRProductKind::MaterialInstance;
	}

	static FString FindTextProperty(const FIRNode& Node, const TCHAR* Name)
	{
		const FIRProperty* Property = Node.FindProperty(Name);
		return Property ? Property->Value.S : FString();
	}

	/** What `#pragma` reads back unquoted: one identifier, or one number with its sign. */
	static bool IsSimplePragmaValue(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		if (IsIdentifierText(Value))
		{
			return true;
		}
		FString Number = Value;
		if (Number.StartsWith(TEXT("-")) || Number.StartsWith(TEXT("+")))
		{
			Number.RightChopInline(1);
		}
		return !Number.IsEmpty() && FCString::IsNumeric(*Number);
	}

	static void AddPragmaArgument(FPragmaDecl& Pragma, const FString& Key, const FString& Value, const bool bForceQuoted = false)
	{
		FPragmaArgument Argument;
		Argument.Key = Key;
		Argument.Value = Value;
		Argument.bQuoted = bForceQuoted || !IsSimplePragmaValue(Value);
		Pragma.Arguments.Add(MoveTemp(Argument));
	}

	static FString SingleLine(const FString& Text)
	{
		return Text.Replace(TEXT("\r\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
	}

	/** A name an HLSL body refers to can only be declared under that very name. */
	static bool CanDeclareVerbatim(const FString& Name)
	{
		ELangKeyword Keyword = ELangKeyword::None;
		return IsIdentifierText(Name)
			&& !TryGetLangKeyword(Name, Keyword)
			&& !Name.Equals(TEXT("UE"), ESearchCase::CaseSensitive)
			&& !Name.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive)
			&& MakeTypeRef(Name).Category == ETypeCategory::Named;
	}

	static bool SameValueShape(const FIRType& A, const FIRType& B)
	{
		if (A.IsMaterial() || B.IsMaterial() || A.IsTexture() || B.IsTexture())
		{
			return A.Kind == B.Kind && A.Texture == B.Texture;
		}
		return A.GraphComponentCount() == B.GraphComponentCount();
	}

	static void CopyPinToParam(const FPinModel& Pin, FCallableParam& Param)
	{
		Param.PinName = Pin.Name;
		Param.Type = Pin.Type;
		Param.TypeSpelling = Pin.TypeSpelling;
		Param.bOptional = Pin.bOptional;
		Param.bHasDefault = Pin.bHasDefault;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Param.Default[Index] = Pin.Default[Index];
		}
		Param.Description = Pin.Description;
		Param.bStatic = Pin.bStatic;
	}

	static FString ClaimParamIdentifier(FNameScope& ParamNames, const FString& PinName, const bool bKeepPinNames)
	{
		if (bKeepPinNames && CanDeclareVerbatim(PinName))
		{
			ParamNames.Reserve(PinName);
			return PinName;
		}
		return ParamNames.Claim(MakeSourceIdentifier(PinName, TEXT("Param")));
	}

	/** The output a function returns: its only one, or a first one called Result -- unless an input has the name. */
	static int32 ChooseReturnOutput(const TArray<FPinModel>& Inputs, const TArray<FPinModel>& Outputs)
	{
		if (Outputs.Num() == 0)
		{
			return INDEX_NONE;
		}
		for (const FPinModel& Input : Inputs)
		{
			if (Input.Name.Equals(Outputs[0].Name, ESearchCase::CaseSensitive))
			{
				return INDEX_NONE;
			}
		}
		return (Outputs.Num() == 1 || Outputs[0].Name.Equals(TEXT("Result"), ESearchCase::CaseSensitive)) ? 0 : INDEX_NONE;
	}

	static void BuildSignature(
		FCallable& Callable,
		const TArray<FPinModel>& Inputs,
		const TArray<FPinModel>& Outputs,
		const int32 ReturnIndex,
		const bool bKeepPinNames,
		const FNameScope& FileNames)
	{
		// The inverse of BuildFunctionProduct: inputs are the non-`out` parameters in order, outputs the return value
		// and then the non-`in` parameters in order. An output named like an input is that input's `inout`; where the two
		// orders cannot both be kept with it, it is an `out` of its own under another identifier.
		FNameScope ParamNames(&FileNames);
		Callable.Params.Reset();
		Callable.bHasReturn = Outputs.IsValidIndex(ReturnIndex);
		if (Callable.bHasReturn)
		{
			const FPinModel& Return = Outputs[ReturnIndex];
			Callable.ReturnPinName = Return.Name;
			Callable.ReturnType = Return.Type;
			Callable.ReturnTypeSpelling = Return.TypeSpelling;
			Callable.ReturnDescription = Return.Description;
		}

		TArray<int32> OutputOfInput;
		OutputOfInput.Init(INDEX_NONE, Inputs.Num());
		TArray<int32> InputOfOutput;
		InputOfOutput.Init(INDEX_NONE, Outputs.Num());
		for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
		{
			for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
			{
				if (OutputIndex != ReturnIndex
					&& InputOfOutput[OutputIndex] == INDEX_NONE
					&& Outputs[OutputIndex].Name.Equals(Inputs[InputIndex].Name, ESearchCase::CaseSensitive)
					&& SameValueShape(Outputs[OutputIndex].Type, Inputs[InputIndex].Type))
				{
					OutputOfInput[InputIndex] = OutputIndex;
					InputOfOutput[OutputIndex] = InputIndex;
					break;
				}
			}
		}

		int32 NextOutput = 0;
		const auto FlushOutputsBefore = [&](const int32 Limit)
		{
			for (; NextOutput < Limit; ++NextOutput)
			{
				if (NextOutput == ReturnIndex)
				{
					continue;
				}
				if (InputOfOutput[NextOutput] != INDEX_NONE)
				{
					// Paired with an input that comes later: the pair would put one of the two orders out.
					OutputOfInput[InputOfOutput[NextOutput]] = INDEX_NONE;
					InputOfOutput[NextOutput] = INDEX_NONE;
				}
				FCallableParam Param;
				CopyPinToParam(Outputs[NextOutput], Param);
				Param.Direction = EParamDirection::Out;
				Param.bOptional = false;
				Param.bHasDefault = false;
				Param.Identifier = ClaimParamIdentifier(ParamNames, Param.PinName, bKeepPinNames);
				Callable.Params.Add(MoveTemp(Param));
			}
		};

		for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
		{
			FCallableParam Param;
			CopyPinToParam(Inputs[InputIndex], Param);

			const int32 Paired = OutputOfInput[InputIndex];
			if (Paired != INDEX_NONE && Paired >= NextOutput)
			{
				FlushOutputsBefore(Paired);
			}
			// The flush may have given the pair up.
			if (OutputOfInput[InputIndex] != INDEX_NONE)
			{
				Param.Direction = EParamDirection::InOut;
				Param.bOptional = false;
				Param.bHasDefault = false;
				NextOutput = OutputOfInput[InputIndex] + 1;
			}
			Param.Identifier = ClaimParamIdentifier(ParamNames, Param.PinName, bKeepPinNames);
			Callable.Params.Add(MoveTemp(Param));
		}
		FlushOutputsBefore(Outputs.Num());
	}

	void BuildCallableSignature(FCallable& Callable, const TArray<FPinModel>& Inputs, const TArray<FPinModel>& Outputs, const FNameScope& FileNames)
	{
		BuildSignature(Callable, Inputs, Outputs, ChooseReturnOutput(Inputs, Outputs), /* bKeepPinNames */ false, FileNames);
	}

	/**
	 * A layer's and a blend's material inputs are optional in the language (IRBuilderMaterial.cpp MakeFunctionInput), as
	 * the engine's own are; one the asset requires is said, because the rebuilt asset lets it go unconnected.
	 */
	static void NoteRequiredLayerMaterials(const TArray<FPinModel>& Inputs, TArray<FText>& OutProblems)
	{
		for (const FPinModel& Input : Inputs)
		{
			if (Input.Type.IsMaterial() && !Input.bOptional)
			{
				OutProblems.Add(FText::Format(
					LOCTEXT("LayerMaterialRequired", "its material input '{0}' has to be connected, and the language makes the materials of a layer and of a blend optional, as the engine's own are: the rebuilt asset lets that input go unconnected"),
					FText::FromString(Input.Name)));
			}
		}
	}

	/** `export void L(inout material m)`: one material in, the same one out. */
	static bool BuildLayerSignature(FCallable& Callable, const TArray<FPinModel>& Inputs, const TArray<FPinModel>& Outputs, const FNameScope& FileNames, TArray<FText>& OutProblems)
	{
		if (Inputs.Num() != 1 || Outputs.Num() != 1 || !Inputs[0].Type.IsMaterial() || !Outputs[0].Type.IsMaterial())
		{
			return false;
		}

		FNameScope ParamNames(&FileNames);
		Callable.Params.Reset();
		Callable.bHasReturn = false;

		FCallableParam Param;
		CopyPinToParam(Inputs[0], Param);
		Param.Direction = EParamDirection::InOut;
		Param.bOptional = false;
		Param.bHasDefault = false;
		Param.Identifier = ParamNames.Claim(MakeSourceIdentifier(Param.PinName, TEXT("Material")));
		Callable.Params.Add(MoveTemp(Param));

		if (!Outputs[0].Name.Equals(Inputs[0].Name, ESearchCase::CaseSensitive))
		{
			OutProblems.Add(FText::Format(
				LOCTEXT("LayerOutputRenamed", "its output pin '{0}' is written under the input's name '{1}', because a layer's material goes in and out through one parameter"),
				FText::FromString(Outputs[0].Name),
				FText::FromString(Inputs[0].Name)));
		}
		NoteRequiredLayerMaterials(Inputs, OutProblems);
		return true;
	}

	/**
	 * `export void B(material Base, material Top, ..., inout material Result)`: the inputs in their order, and the one
	 * output as the `inout material` at the end. Nothing arrives through a blend's result (IRBuilderMaterial.cpp
	 * BuildLayerProduct), so an input that happens to share the output's name is one more input, under an identifier
	 * of its own.
	 */
	static bool BuildBlendSignature(FCallable& Callable, const TArray<FPinModel>& Inputs, const TArray<FPinModel>& Outputs, const FNameScope& FileNames, TArray<FText>& OutProblems)
	{
		if (Outputs.Num() != 1 || !Outputs[0].Type.IsMaterial()
			|| !Inputs.ContainsByPredicate([](const FPinModel& Input) { return Input.Type.IsMaterial(); }))
		{
			return false;
		}

		FNameScope ParamNames(&FileNames);
		Callable.Params.Reset();
		Callable.bHasReturn = false;

		for (const FPinModel& Input : Inputs)
		{
			FCallableParam Param;
			CopyPinToParam(Input, Param);
			Param.Direction = EParamDirection::In;
			Param.Identifier = ParamNames.Claim(MakeSourceIdentifier(Param.PinName, TEXT("Param")));
			Callable.Params.Add(MoveTemp(Param));
		}

		FCallableParam Result;
		CopyPinToParam(Outputs[0], Result);
		Result.Direction = EParamDirection::InOut;
		Result.bOptional = false;
		Result.bHasDefault = false;
		Result.Identifier = ParamNames.Claim(MakeSourceIdentifier(Result.PinName, TEXT("Result")));
		Callable.Params.Add(MoveTemp(Result));
		NoteRequiredLayerMaterials(Inputs, OutProblems);
		return true;
	}

	// ------------------------------------------------------------------------------------ plumbing

	FModuleWriter::FModuleWriter(const IR::FIRModule& InSource, const IR::FBuiltinCatalog& InCatalog, const FIRToAstOptions& InOptions, FLangDiagnosticSink& InDiagnostics)
		: Source(InSource)
		, Catalog(InCatalog)
		, Options(InOptions)
		, Diagnostics(InDiagnostics)
	{
	}

	void FModuleWriter::Info(const TCHAR* Code, const FText& Message)
	{
		Diagnostics.Info(Code, FLangSpan(), Message);
	}

	void FModuleWriter::Warning(const TCHAR* Code, const FText& Message)
	{
		Diagnostics.Warning(Code, FLangSpan(), Message);
	}

	void FModuleWriter::Error(const TCHAR* Code, const FText& Message)
	{
		Diagnostics.Error(Code, FLangSpan(), Message);
	}

	FString FModuleWriter::MakeUniformKey(const bool bTexture, const FString& ParameterName)
	{
		return (bTexture ? FString(TEXT("T:")) : FString(TEXT("P:"))) + ParameterName;
	}

	FString FModuleWriter::NormalizeAssetPath(const FString& Path)
	{
		FString Result = Path.TrimStartAndEnd().TrimQuotes();

		// `MaterialFunction'/Game/F/MF_X.MF_X'`
		int32 FirstQuote = INDEX_NONE;
		int32 LastQuote = INDEX_NONE;
		if (Result.FindChar(TEXT('\''), FirstQuote) && Result.FindLastChar(TEXT('\''), LastQuote) && LastQuote > FirstQuote)
		{
			Result = Result.Mid(FirstQuote + 1, LastQuote - FirstQuote - 1);
		}

		// `/Game/F/MF_X.MF_X` names the same asset as `/Game/F/MF_X`.
		int32 Dot = INDEX_NONE;
		int32 Slash = INDEX_NONE;
		if (Result.FindLastChar(TEXT('.'), Dot) && Result.FindLastChar(TEXT('/'), Slash) && Dot > Slash)
		{
			const FString Leaf = Result.Mid(Slash + 1, Dot - Slash - 1);
			if (Leaf.Equals(Result.Mid(Dot + 1), ESearchCase::IgnoreCase))
			{
				Result.LeftInline(Dot);
			}
		}
		return Result;
	}

	FString FModuleWriter::AssetLeafName(const FString& Path)
	{
		FString Leaf = Path;
		int32 Slash = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('/'), Slash))
		{
			Leaf.RightChopInline(Slash + 1);
		}
		int32 Dot = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), Dot))
		{
			Leaf.LeftInline(Dot);
		}
		return Leaf;
	}

	FString FModuleWriter::MakeCustomNodeKey(const FIRNode& Node)
	{
		const FString Name = Node.ClassName.IsEmpty() ? FindTextProperty(Node, IR::Prop::Description) : Node.ClassName;
		return FString::Printf(TEXT("%s#%08X"), *Name, FCrc::StrCrc32(*FindTextProperty(Node, IR::Prop::Code)));
	}

	const FUniformModel* FModuleWriter::FindUniform(const FIRNode& Node) const
	{
		const FString ParameterName = FindTextProperty(Node, IR::Prop::ParameterName);
		if (ParameterName.IsEmpty())
		{
			return nullptr;
		}
		const int32* Index = UniformByKey.Find(MakeUniformKey(Node.Op == EIROp::TextureParameter, ParameterName));
		return Index ? &Uniforms[*Index] : nullptr;
	}

	const FCallable* FModuleWriter::FindCallable(const FIRNode& Node) const
	{
		const int32* Index = nullptr;
		if (Node.Op == EIROp::FunctionCall)
		{
			const FString Path = FindTextProperty(Node, IR::Prop::FunctionPath);
			const FIRProperty* Local = Node.FindProperty(IR::Prop::LocalFunction);
			if (Local && Path.IsEmpty())
			{
				Index = CallableByProduct.Find(static_cast<int32>(Local->Value.I));
			}
			else
			{
				Index = CallableByAssetPath.Find(NormalizeAssetPath(Path.IsEmpty() ? Node.ClassName : Path));
			}
		}
		else if (Node.Op == EIROp::Custom)
		{
			Index = CallableByCustomNode.Find(MakeCustomNodeKey(Node));
		}
		return (Index && Callables.IsValidIndex(*Index)) ? &Callables[*Index] : nullptr;
	}

	void FModuleWriter::AddDeclaration(FDeclPtr Decl, const bool bBlankLineBefore)
	{
		if (!Decl)
		{
			return;
		}
		// Every declaration gets an entry: a module that carries trivia is laid out by it, which is what puts the blank
		// lines where they are asked for and nowhere else.
		FLangTrivia& Trivia = Module->Trivia.FindOrAdd(Decl.Get());
		Trivia.BlankLinesBefore = (bBlankLineBefore && Module->Declarations.Num() > 0) ? 1 : 0;
		Module->Declarations.Add(MoveTemp(Decl));
	}

	// ------------------------------------------------------------------------------------ uniforms

	void FModuleWriter::CollectUniforms()
	{
		for (const FIRProduct& Product : Source.Products)
		{
			if (!IsGraphProduct(Product))
			{
				continue;
			}
			const FIRGraph& Graph = Product.Graph;

			for (const FIRNode& Node : Graph.Nodes)
			{
				if (Node.Op != EIROp::Parameter && Node.Op != EIROp::TextureParameter)
				{
					continue;
				}
				const FString ParameterName = FindTextProperty(Node, IR::Prop::ParameterName);
				if (ParameterName.IsEmpty())
				{
					continue;
				}

				FUniformModel Seen;
				Seen.ParameterName = ParameterName;
				Seen.HintName = Node.DebugName;
				Seen.bTexture = Node.Op == EIROp::TextureParameter;
				Seen.TextureType = Node.Outputs.IsValidIndex(0) ? Node.Outputs[0] : FIRType::TextureOf(ETextureKind::Texture2D);
				Seen.NodeWidth = (!Seen.bTexture && Node.Outputs.IsValidIndex(0)) ? FMath::Clamp(Node.Outputs[0].GraphComponentCount(), 1, 4) : 1;
				if (Graph.Regions.IsValidIndex(Node.Region))
				{
					const bool bParametersOnly = !Graph.Nodes.ContainsByPredicate([&Node](const FIRNode& Other)
					{
						return Other.Region == Node.Region && Other.Op != EIROp::Parameter && Other.Op != EIROp::TextureParameter;
					});
					const bool bNested = Graph.Regions[Node.Region].Parent != INDEX_NONE
						|| Graph.Regions.ContainsByPredicate([&Node](const IR::FIRRegion& Other) { return Other.Parent == Node.Region; });
					if (bParametersOnly && !bNested)
					{
						// One line, as `#pragma region <title>` runs to the end of its own.
						Seen.Region = Graph.Regions[Node.Region].Name.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
					}
				}

				for (const FIRProperty& Property : Node.Properties)
				{
					const FIRPropertyValue& Value = Property.Value;
					if (Property.Name.Equals(IR::Prop::ParameterName, ESearchCase::CaseSensitive))
					{
						continue;
					}
					if (Property.Name.Equals(IR::Prop::Group, ESearchCase::CaseSensitive)) { Seen.Group = Value.S; }
					else if (Property.Name.Equals(IR::Prop::Description, ESearchCase::CaseSensitive)) { Seen.Description = Value.S; }
					else if (Property.Name.Equals(IR::Prop::DefaultAsset, ESearchCase::CaseSensitive)) { Seen.DefaultAsset = Value.S; }
					else if (Property.Name.Equals(IR::Prop::SamplerType, ESearchCase::CaseSensitive)) { Seen.SamplerType = Value.S; }
					else if (Property.Name.Equals(IR::Prop::IsStatic, ESearchCase::CaseSensitive)) { Seen.bStatic = Value.B; }
					else if (Property.Name.Equals(IR::Prop::SliderMin, ESearchCase::CaseSensitive)) { Seen.bHasSlider = true; Seen.SliderMin = Value.F; }
					else if (Property.Name.Equals(IR::Prop::SliderMax, ESearchCase::CaseSensitive)) { Seen.bHasSlider = true; Seen.SliderMax = Value.F; }
					else if (Property.Name.Equals(IR::Prop::SortPriority, ESearchCase::CaseSensitive)) { Seen.bHasSort = true; Seen.Sort = Value.I; }
					else if (Property.Name.Equals(IR::Prop::DefaultValue, ESearchCase::CaseSensitive))
					{
						if (Value.Kind == IR::EIRPropertyKind::Float4)
						{
							Seen.DefaultCount = FMath::Clamp(Value.N, 1, 4);
							for (int32 Index = 0; Index < 4; ++Index)
							{
								Seen.Default[Index] = Value.V[Index];
							}
						}
						else
						{
							Seen.DefaultCount = 1;
							Seen.Default[0] = Value.Kind == IR::EIRPropertyKind::Bool ? (Value.B ? 1.0 : 0.0)
								: Value.Kind == IR::EIRPropertyKind::Int ? static_cast<double>(Value.I)
								: Value.F;
						}
					}
					else if (Value.Kind == IR::EIRPropertyKind::String || Value.Kind == IR::EIRPropertyKind::Name || Value.Kind == IR::EIRPropertyKind::Enum)
					{
						// A directive the language does not define rides along by its key (ApplyParameterMetadata).
						Seen.Passthrough.Emplace(Property.Name, Value.S);
					}
				}

				const FString Key = MakeUniformKey(Seen.bTexture, ParameterName);
				if (const int32* Existing = UniformByKey.Find(Key))
				{
					FUniformModel& First = Uniforms[*Existing];
					bool bAgrees = First.bStatic == Seen.bStatic
						&& First.NodeWidth == Seen.NodeWidth
						&& First.Group.Equals(Seen.Group, ESearchCase::CaseSensitive)
						&& First.DefaultAsset.Equals(Seen.DefaultAsset, ESearchCase::CaseSensitive);
					for (int32 Index = 0; Index < 4; ++Index)
					{
						bAgrees = bAgrees && First.Default[Index] == Seen.Default[Index];
					}
					if (!bAgrees && !DisagreeingUniforms.Contains(Key))
					{
						DisagreeingUniforms.Add(Key);
						Warning(TEXT("DSH9080"), FText::Format(
							LOCTEXT("UniformDisagrees", "The parameter '{0}' appears more than once and its nodes do not agree on its default, group or kind; the uniform is written from the first one, in '{1}'."),
							FText::FromString(ParameterName),
							FText::FromString(Product.Name)));
					}
					if (First.HintName.IsEmpty())
					{
						First.HintName = Seen.HintName;
					}
				}
				else
				{
					Seen.FirstSeen = Uniforms.Num();
					UniformByKey.Add(Key, Uniforms.Num());
					Uniforms.Add(MoveTemp(Seen));
				}
			}

			// How the graph reads each vector parameter: whole, or through leading masks only.
			for (const FIRNode& Reader : Graph.Nodes)
			{
				TArray<FIRValue> Values;
				FIRGraph::CollectInputValues(Reader, Values);
				for (const FIRValue& Value : Values)
				{
					if (!Graph.Nodes.IsValidIndex(Value.Node) || Graph.Nodes[Value.Node].Op != EIROp::Parameter)
					{
						continue;
					}
					const FString ParameterName = FindTextProperty(Graph.Nodes[Value.Node], IR::Prop::ParameterName);
					const int32* Index = ParameterName.IsEmpty() ? nullptr : UniformByKey.Find(MakeUniformKey(false, ParameterName));
					if (!Index)
					{
						continue;
					}
					FUniformModel& Uniform = Uniforms[*Index];

					const FString Mask = Reader.Op == EIROp::Swizzle ? FindTextProperty(Reader, IR::Prop::Mask) : FString();
					const bool bLeadingMask = Value.Output == 0
						&& !Mask.IsEmpty()
						&& Mask.Len() < 4
						&& FString(TEXT("xyzw")).StartsWith(Mask, ESearchCase::CaseSensitive);
					if (bLeadingMask)
					{
						Uniform.WidestLeadingMask = FMath::Max(Uniform.WidestLeadingMask, Mask.Len());
					}
					else
					{
						Uniform.bReadWhole = true;
					}
				}
			}
		}
	}

	void FModuleWriter::NameUniforms()
	{
		for (FUniformModel& Uniform : Uniforms)
		{
			// The width. A float2 default is padded (a, b, 0, 1) and a float3 one (r, g, b, 1) by the emitter; a parameter
			// that carries anything else in the padding was not declared that narrow.
			Uniform.Width = Uniform.NodeWidth;
			if (!Uniform.bTexture && !Uniform.bStatic && Uniform.NodeWidth == 4 && !Uniform.bReadWhole && Uniform.WidestLeadingMask >= 2)
			{
				const int32 Narrow = Uniform.WidestLeadingMask;
				bool bFits = true;
				if (Uniform.DefaultCount >= 4)
				{
					// Read off an asset: all four channels are there, and the padding says whether they were declared.
					bFits = Uniform.Default[3] == 1.0 && (Narrow == 3 || Uniform.Default[2] == 0.0);
				}
				else if (Uniform.DefaultCount >= 2)
				{
					// Built from source: the default has the declared width.
					bFits = Uniform.DefaultCount == Narrow;
				}
				if (bFits)
				{
					Uniform.Width = Narrow;
				}
			}
		}

		UniformOrder.Reset();
		for (int32 Index = 0; Index < Uniforms.Num(); ++Index)
		{
			UniformOrder.Add(Index);
		}
		UniformOrder.StableSort([this](const int32 A, const int32 B)
		{
			const int64 SortA = Uniforms[A].bHasSort ? Uniforms[A].Sort : MAX_int64;
			const int64 SortB = Uniforms[B].bHasSort ? Uniforms[B].Sort : MAX_int64;
			return SortA != SortB ? SortA < SortB : Uniforms[A].FirstSeen < Uniforms[B].FirstSeen;
		});

		// Without `@sort` a uniform's priority is its place among the uniforms (MakeGlobalValue), whatever the others
		// say. A priority that is exactly that place says nothing the order does not, so it is left out -- one uniform at
		// a time: `@sort 70` on one of them does not make the six in front of it spell out 0 to 5.
		for (int32 Position = 0; Position < UniformOrder.Num(); ++Position)
		{
			FUniformModel& Uniform = Uniforms[UniformOrder[Position]];
			if (Uniform.bHasSort && Uniform.Sort == Position)
			{
				Uniform.bHasSort = false;
			}
		}

		for (const int32 Index : UniformOrder)
		{
			FUniformModel& Uniform = Uniforms[Index];
			const bool bHintUsable = IsIdentifierText(Uniform.HintName) && !IsReservedIdentifier(Uniform.HintName);
			const FString Wanted = bHintUsable ? Uniform.HintName : MakeSourceIdentifier(Uniform.ParameterName, TEXT("Parameter"));
			Uniform.Identifier = FileNames.Claim(Wanted);
			if (bHintUsable && !Uniform.Identifier.Equals(Uniform.HintName, ESearchCase::CaseSensitive))
			{
				Info(TEXT("DSH9075"), FText::Format(
					LOCTEXT("UniformRenamed", "The uniform '{0}' is written as '{1}': the name is already taken in this file."),
					FText::FromString(Uniform.HintName),
					FText::FromString(Uniform.Identifier)));
			}
		}
	}

	void FModuleWriter::EmitUniforms()
	{
		// `#pragma region <Title>` ... `#pragma endregion` around every run of uniforms declared in one file-scope region.
		// The uniforms come in parameter order, so a region whose uniforms are not neighbours there is written as two
		// runs of one title, which is the same box in the graph.
		FString OpenRegion;
		const auto SwitchRegion = [this, &OpenRegion](const FString& Region)
		{
			if (Region.Equals(OpenRegion, ESearchCase::CaseSensitive))
			{
				return false;
			}
			if (!OpenRegion.IsEmpty())
			{
				TUniquePtr<FPragmaDecl> End = MakeUnique<FPragmaDecl>();
				End->PragmaKind = EPragmaKind::EndRegion;
				End->Name = TEXT("endregion");
				AddDeclaration(MoveTemp(End), /* bBlankLineBefore */ false);
			}
			if (!Region.IsEmpty())
			{
				TUniquePtr<FPragmaDecl> Begin = MakeUnique<FPragmaDecl>();
				Begin->PragmaKind = EPragmaKind::Region;
				Begin->Name = TEXT("region");
				Begin->Text = Region;
				AddDeclaration(MoveTemp(Begin), /* bBlankLineBefore */ true);
			}
			OpenRegion = Region;
			return true;
		};

		bool bFirst = true;
		bool bPreviousDocumented = false;
		for (const int32 Index : UniformOrder)
		{
			const FUniformModel& Uniform = Uniforms[Index];
			const bool bRegionChanged = SwitchRegion(Uniform.Region);
			// The first uniform of a region sits right under its `#pragma region` line; the one after an `endregion`
			// stands apart from it.
			const bool bOpensRegion = bRegionChanged && !Uniform.Region.IsEmpty();
			const bool bFollowsRegion = bRegionChanged && Uniform.Region.IsEmpty();

			TUniquePtr<FVariableDecl> Decl = MakeUnique<FVariableDecl>();
			Decl->Storage = EStorageClass::Uniform;
			Decl->Declarator.Name = Uniform.Identifier;

			if (Uniform.bTexture)
			{
				Decl->Type = MakeValueTypeRef(Uniform.TextureType, /* bKeepBool */ false);
			}
			else if (Uniform.bStatic)
			{
				Decl->Type = MakeTypeRef(TEXT("bool"));
				Decl->Declarator.Initializer = MakeBoolExpr(Uniform.Default[0] != 0.0);
			}
			else
			{
				Decl->Type = MakeValueTypeRef(FIRType::Float(Uniform.Width), /* bKeepBool */ false);
				Decl->Declarator.Initializer = MakeVectorLiteralExpr(Uniform.Default, Uniform.Width, /* bBool */ false);
			}

			AddDocDescription(Decl->Doc, Uniform.Description);
			if (!Uniform.Identifier.Equals(Uniform.ParameterName, ESearchCase::CaseSensitive))
			{
				AddDocDirective(Decl->Doc, Directive::Name, Uniform.ParameterName);
			}
			if (!Uniform.Group.IsEmpty())
			{
				AddDocDirective(Decl->Doc, Directive::Group, Uniform.Group);
			}
			if (Uniform.bHasSlider)
			{
				AddDocDirective(Decl->Doc, Directive::Slider, FormatDreamShaderFloatLiteral(Uniform.SliderMin) + TEXT(" ") + FormatDreamShaderFloatLiteral(Uniform.SliderMax));
			}
			if (Uniform.bHasSort)
			{
				AddDocDirective(Decl->Doc, Directive::Sort, FString::Printf(TEXT("%lld"), Uniform.Sort));
			}
			if (Uniform.bStatic)
			{
				AddDocDirective(Decl->Doc, Directive::Static, FString());
			}
			if (!Uniform.SamplerType.IsEmpty())
			{
				AddDocDirective(Decl->Doc, Directive::Sampler, Uniform.SamplerType);
			}
			if (!Uniform.DefaultAsset.IsEmpty())
			{
				AddDocDirective(Decl->Doc, Directive::Default, Uniform.DefaultAsset);
			}
			for (const TPair<FString, FString>& Passthrough : Uniform.Passthrough)
			{
				AddDocDirective(Decl->Doc, *Passthrough.Key, SingleLine(Passthrough.Value));
			}

			// A documented uniform stands apart; a run of bare ones reads as a block.
			const bool bBlank = !bOpensRegion && (bFirst || bFollowsRegion || !Decl->Doc.IsEmpty() || bPreviousDocumented);
			bPreviousDocumented = !Decl->Doc.IsEmpty();
			bFirst = false;
			AddDeclaration(MoveTemp(Decl), bBlank);
		}
		SwitchRegion(FString());
	}

	// ----------------------------------------------------------------------------------- callables

	void FModuleWriter::AddSignatureDirectives(FDocBlock& Doc, const FCallable& Callable) const
	{
		for (const FCallableParam& Param : Callable.Params)
		{
			if (!Param.Description.IsEmpty())
			{
				AddDocDirective(Doc, Directive::Param, Param.Identifier + TEXT(" ") + SingleLine(Param.Description));
			}
		}
		if (Callable.bHasReturn && !Callable.ReturnDescription.IsEmpty())
		{
			AddDocDirective(Doc, Directive::Param, FString(TEXT("Result ")) + SingleLine(Callable.ReturnDescription));
		}

		// A Custom node's pins are the HLSL identifiers themselves: never `@pin`.
		if (Callable.Kind == ECallableKind::Custom)
		{
			return;
		}
		for (const FCallableParam& Param : Callable.Params)
		{
			if (Param.bStatic)
			{
				AddDocDirective(Doc, Directive::Static, Param.Identifier);
			}
		}
		for (const FCallableParam& Param : Callable.Params)
		{
			if (!Param.Identifier.Equals(Param.PinName, ESearchCase::CaseSensitive))
			{
				AddDocDirective(Doc, Directive::Pin, Param.Identifier + TEXT(" ") + Param.PinName);
			}
		}
		if (Callable.bHasReturn && !Callable.ReturnPinName.Equals(TEXT("Result"), ESearchCase::CaseSensitive) && !Callable.ReturnPinName.IsEmpty())
		{
			AddDocDirective(Doc, Directive::Pin, FString(TEXT("Result ")) + Callable.ReturnPinName);
		}
	}

	void FModuleWriter::FillParams(FFunctionDecl& Decl, const FCallable& Callable) const
	{
		Decl.ReturnType = Callable.bHasReturn
			? MakeSignatureTypeRef(Callable.ReturnType, Callable.ReturnTypeSpelling)
			: MakeTypeRef(TEXT("void"));

		for (const FCallableParam& Model : Callable.Params)
		{
			FParam Param;
			Param.Direction = Model.Direction;
			Param.Type = MakeSignatureTypeRef(Model.Type, Model.TypeSpelling);
			Param.Name = Model.Identifier;

			// An input the asset lets go unconnected is one with a default. 2.0 has no `opt` without one, so an optional
			// input that carries none is written with the zero of its type.
			const int32 Width = Model.Type.GraphComponentCount();
			if (Model.Direction == EParamDirection::In && (Model.bHasDefault || Model.bOptional) && Width > 0)
			{
				const double Zero[4] = { 0.0, 0.0, 0.0, 0.0 };
				const double* Values = Model.bHasDefault ? Model.Default : Zero;
				// A static bool pin's default is `true` / `false`; every other pin carries numbers.
				Param.Default = Model.bStatic ? MakeBoolExpr(Values[0] != 0.0) : MakeVectorLiteralExpr(Values, Width, /* bBool */ false);
			}
			Decl.Params.Add(MoveTemp(Param));
		}
	}

	static FPinModel MakeInputPinModel(const FIRNode& Node)
	{
		FPinModel Pin;
		Pin.Name = FindTextProperty(Node, IR::Prop::InputName);
		Pin.Type = Node.Outputs.IsValidIndex(0) ? Node.Outputs[0] : FIRType::Float(1);
		Pin.Description = FindTextProperty(Node, IR::Prop::Description);
		if (FindTextProperty(Node, IR::Prop::InputType).Equals(TEXT("StaticBool"), ESearchCase::CaseSensitive))
		{
			Pin.bStatic = true;
			Pin.Type = FIRType::Bool(1);
		}
		if (const FIRProperty* Optional = Node.FindProperty(IR::Prop::IsOptional))
		{
			Pin.bOptional = Optional->Value.B;
		}
		if (const FIRProperty* Preview = Node.FindProperty(IR::Prop::PreviewValue))
		{
			// (c, c, c, c) for a scalar, the components and then (0, 1) padding for a vector (SpreadPreviewValue).
			Pin.bHasDefault = Pin.bOptional && Preview->Value.Kind == IR::EIRPropertyKind::Float4;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Pin.Default[Index] = Preview->Value.V[Index];
			}
		}
		return Pin;
	}

	void FModuleWriter::CollectProductCallables()
	{
		ProductIdentifiers.SetNum(Source.Products.Num());

		for (int32 ProductIndex = 0; ProductIndex < Source.Products.Num(); ++ProductIndex)
		{
			const FIRProduct& Product = Source.Products[ProductIndex];
			if (!IsGraphProduct(Product))
			{
				continue;
			}

			// 1.x names may carry folders; the function is called by the leaf.
			FString Leaf = Product.Name;
			int32 Slash = INDEX_NONE;
			if (Leaf.FindLastChar(TEXT('/'), Slash))
			{
				Leaf.RightChopInline(Slash + 1);
			}
			ProductIdentifiers[ProductIndex] = FileNames.Claim(MakeSourceIdentifier(Leaf, Product.Kind == IR::EIRProductKind::Material ? TEXT("Material") : TEXT("Function")));

			if (Options.ProductAssetPaths.IsValidIndex(ProductIndex) && !Options.ProductAssetPaths[ProductIndex].IsEmpty())
			{
				ProductByAssetPath.Add(NormalizeAssetPath(Options.ProductAssetPaths[ProductIndex]), ProductIndex);
			}
			if (Product.Kind == IR::EIRProductKind::Material)
			{
				continue;
			}

			const FIRGraph& Graph = Product.Graph;
			TArray<FPinModel> Inputs;
			for (const int32 NodeIndex : Graph.FunctionInputs)
			{
				if (Graph.Nodes.IsValidIndex(NodeIndex))
				{
					Inputs.Add(MakeInputPinModel(Graph.Nodes[NodeIndex]));
				}
			}
			TArray<FPinModel> Outputs;
			for (const int32 NodeIndex : Graph.FunctionOutputs)
			{
				if (!Graph.Nodes.IsValidIndex(NodeIndex))
				{
					continue;
				}
				const FIRNode& Node = Graph.Nodes[NodeIndex];
				FPinModel Pin;
				Pin.Name = FindTextProperty(Node, IR::Prop::OutputName);
				Pin.Description = FindTextProperty(Node, IR::Prop::Description);
				Pin.Type = (Node.Operands.Num() > 0 && Graph.IsValidValue(Node.Operands[0])) ? Graph.TypeOf(Node.Operands[0]) : FIRType::Float(1);
				Outputs.Add(MoveTemp(Pin));
			}

			FCallable Callable;
			Callable.Kind = ECallableKind::Product;
			Callable.ProductIndex = ProductIndex;
			Callable.Identifier = ProductIdentifiers[ProductIndex];
			Callable.Description = Product.Description;

			bool bWrittenAsKind = true;
			TArray<FText> Problems;
			if (Product.Kind == IR::EIRProductKind::MaterialLayer)
			{
				bWrittenAsKind = BuildLayerSignature(Callable, Inputs, Outputs, FileNames, Problems);
			}
			else if (Product.Kind == IR::EIRProductKind::MaterialLayerBlend)
			{
				bWrittenAsKind = BuildBlendSignature(Callable, Inputs, Outputs, FileNames, Problems);
			}
			else
			{
				BuildCallableSignature(Callable, Inputs, Outputs, FileNames);
			}

			if (!bWrittenAsKind)
			{
				DemotedLayers.Add(ProductIndex);
				BuildCallableSignature(Callable, Inputs, Outputs, FileNames);
				Warning(TEXT("DSH9083"), FText::Format(
					LOCTEXT("LayerDemoted", "'{0}' is a {1}, and its pins are not the ones the language writes that kind with; it is written as a plain exported function, which builds a material function."),
					FText::FromString(Product.Name),
					FText::FromString(IR::LexToString(Product.Kind))));
			}
			else
			{
				for (const FText& Problem : Problems)
				{
					Warning(TEXT("DSH9083"), FText::Format(
						LOCTEXT("LayerApproximated", "'{0}' is a {1}, and {2}."),
						FText::FromString(Product.Name),
						FText::FromString(IR::LexToString(Product.Kind)),
						Problem));
				}
			}

			CallableByProduct.Add(ProductIndex, Callables.Num());
			Callables.Add(MoveTemp(Callable));
		}
	}

	void FModuleWriter::CollectExternCallables()
	{
		struct FExternDraft
		{
			FString Path;
			const FIRToAstExternInterface* Interface = nullptr;
			TArray<FPinModel> Inputs;
			TArray<FPinModel> Outputs;
			int32 Calls = 0;
			/** Inputs some call leaves out. */
			TSet<FString> OmittedSomewhere;
		};

		TMap<FString, const FIRToAstExternInterface*> InterfaceByPath;
		for (const FIRToAstExternInterface& Interface : Options.ExternInterfaces)
		{
			InterfaceByPath.Add(NormalizeAssetPath(Interface.AssetPath), &Interface);
		}

		TArray<FExternDraft> Drafts;
		TMap<FString, int32> DraftByPath;

		for (const FIRProduct& Product : Source.Products)
		{
			if (!IsGraphProduct(Product))
			{
				continue;
			}
			const FIRGraph& Graph = Product.Graph;
			for (const FIRNode& Node : Graph.Nodes)
			{
				if (Node.Op != EIROp::FunctionCall)
				{
					continue;
				}
				const FString RawPath = FindTextProperty(Node, IR::Prop::FunctionPath);
				if (RawPath.IsEmpty() && Node.FindProperty(IR::Prop::LocalFunction))
				{
					continue;
				}
				const FString Path = NormalizeAssetPath(RawPath.IsEmpty() ? Node.ClassName : RawPath);
				if (Path.IsEmpty() || CallableByAssetPath.Contains(Path))
				{
					continue;
				}

				// An asset this very file makes: the call is to the function, not to a prototype of its own product.
				if (const int32* OwnProduct = ProductByAssetPath.Find(Path))
				{
					if (const int32* Callable = CallableByProduct.Find(*OwnProduct))
					{
						CallableByAssetPath.Add(Path, *Callable);
						continue;
					}
				}

				int32 DraftIndex = INDEX_NONE;
				if (const int32* Existing = DraftByPath.Find(Path))
				{
					DraftIndex = *Existing;
				}
				else
				{
					DraftIndex = Drafts.AddDefaulted();
					DraftByPath.Add(Path, DraftIndex);
					FExternDraft& Draft = Drafts[DraftIndex];
					Draft.Path = Path;
					if (const FIRToAstExternInterface* const* Interface = InterfaceByPath.Find(Path))
					{
						Draft.Interface = *Interface;
					}
				}

				FExternDraft& Draft = Drafts[DraftIndex];
				++Draft.Calls;
				if (Draft.Interface)
				{
					continue;
				}

				// No interface: the prototype is whatever the calls show.
				for (const FPinModel& Known : Draft.Inputs)
				{
					const FIRInput* Wired = Node.FindInput(Known.Name);
					if (!Wired || !Wired->Value.IsValid())
					{
						Draft.OmittedSomewhere.Add(Known.Name);
					}
				}
				for (const FIRInput& Input : Node.Inputs)
				{
					if (!Input.Value.IsValid())
					{
						continue;
					}
					const bool bKnown = Draft.Inputs.ContainsByPredicate([&Input](const FPinModel& Pin) { return Pin.Name.Equals(Input.Pin, ESearchCase::CaseSensitive); });
					if (!bKnown)
					{
						FPinModel Pin;
						Pin.Name = Input.Pin;
						Pin.Type = Graph.IsValidValue(Input.Value) ? Graph.TypeOf(Input.Value) : FIRType::Float(1);
						if (Draft.Calls > 1)
						{
							Draft.OmittedSomewhere.Add(Pin.Name);
						}
						Draft.Inputs.Add(MoveTemp(Pin));
					}
				}
				for (int32 OutputIndex = 0; OutputIndex < Node.OutputNames.Num(); ++OutputIndex)
				{
					const FString& OutputName = Node.OutputNames[OutputIndex];
					const bool bKnown = Draft.Outputs.ContainsByPredicate([&OutputName](const FPinModel& Pin) { return Pin.Name.Equals(OutputName, ESearchCase::CaseSensitive); });
					if (!bKnown)
					{
						FPinModel Pin;
						Pin.Name = OutputName;
						Pin.Type = Node.Outputs.IsValidIndex(OutputIndex) ? Node.Outputs[OutputIndex] : FIRType::Float(1);
						Draft.Outputs.Add(MoveTemp(Pin));
					}
				}
			}
		}

		for (FExternDraft& Draft : Drafts)
		{
			FCallable Callable;
			Callable.Kind = ECallableKind::Extern;
			Callable.AssetPath = Draft.Path;
			Callable.Identifier = FileNames.Claim(MakeSourceIdentifier(AssetLeafName(Draft.Path), TEXT("Function")));

			TArray<FPinModel> Inputs;
			TArray<FPinModel> Outputs;
			if (Draft.Interface)
			{
				Callable.Description = Draft.Interface->Description;
				const auto Convert = [](const TArray<FIRToAstExternPin>& From, TArray<FPinModel>& To)
				{
					for (const FIRToAstExternPin& Pin : From)
					{
						FPinModel Model;
						Model.Name = Pin.Name;
						Model.Type = Pin.Type;
						Model.bOptional = Pin.bOptional;
						Model.bHasDefault = Pin.bHasDefault;
						for (int32 Index = 0; Index < 4; ++Index)
						{
							Model.Default[Index] = Pin.Default[Index];
						}
						Model.Description = Pin.Description;
						To.Add(MoveTemp(Model));
					}
				};
				Convert(Draft.Interface->Inputs, Inputs);
				Convert(Draft.Interface->Outputs, Outputs);
			}
			else
			{
				Callable.bInferred = true;
				Inputs = MoveTemp(Draft.Inputs);
				Outputs = MoveTemp(Draft.Outputs);
				for (FPinModel& Input : Inputs)
				{
					Input.bOptional = Draft.OmittedSomewhere.Contains(Input.Name);
				}
				Warning(TEXT("DSH9081"), FText::Format(
					LOCTEXT("ExternInferred", "'{0}' is called and its interface was not available, so its 'extern' prototype is written from the calls alone: pins no call connects are missing from it, and their order is the order the calls wire them in."),
					FText::FromString(Draft.Path)));
			}

			BuildCallableSignature(Callable, Inputs, Outputs, FileNames);
			CallableByAssetPath.Add(Draft.Path, Callables.Num());
			Callables.Add(MoveTemp(Callable));
		}
	}

	void FModuleWriter::CollectCustomCallables()
	{
		struct FCustomDraft
		{
			FString NodeKey;
			FString NodeName;
			/** The node's Description, which a built node takes from the function's `/// @name` or its name. */
			FString NodeTitle;
			FString Code;
			TArray<FString> Includes;
			bool bRecovered = false;
			FRecoveredCustomCode Recovered;
			TArray<FPinModel> Inputs;
			TArray<FPinModel> Outputs;
			bool bPrimaryOutputRead = false;
		};

		TArray<FCustomDraft> Drafts;
		TMap<FString, int32> DraftByKey;

		for (const FIRProduct& Product : Source.Products)
		{
			if (!IsGraphProduct(Product))
			{
				continue;
			}
			const FIRGraph& Graph = Product.Graph;

			TSet<int32> PrimaryOutputRead;
			TArray<FIRValue> Values;
			for (const FIRNode& Reader : Graph.Nodes)
			{
				FIRGraph::CollectInputValues(Reader, Values);
				for (const FIRValue& Value : Values)
				{
					if (Value.Output == 0 && Graph.Nodes.IsValidIndex(Value.Node) && Graph.Nodes[Value.Node].Op == EIROp::Custom)
					{
						PrimaryOutputRead.Add(Value.Node);
					}
				}
			}

			for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
			{
				const FIRNode& Node = Graph.Nodes[NodeIndex];
				if (Node.Op != EIROp::Custom)
				{
					continue;
				}

				const FString Key = MakeCustomNodeKey(Node);
				int32 DraftIndex = INDEX_NONE;
				if (const int32* Existing = DraftByKey.Find(Key))
				{
					DraftIndex = *Existing;
				}
				else
				{
					DraftIndex = Drafts.AddDefaulted();
					DraftByKey.Add(Key, DraftIndex);

					FCustomDraft& Draft = Drafts[DraftIndex];
					Draft.NodeKey = Key;
					Draft.NodeName = Node.ClassName.IsEmpty() ? FindTextProperty(Node, IR::Prop::Description) : Node.ClassName;
					Draft.NodeTitle = FindTextProperty(Node, IR::Prop::Description);
					Draft.Code = FindTextProperty(Node, IR::Prop::Code);
					if (const FIRProperty* Includes = Node.FindProperty(IR::Prop::IncludeFilePaths))
					{
						Draft.Includes = Includes->Value.List;
					}
					Draft.bRecovered = RecoverCustomCode(Draft.Code, Draft.Includes, Draft.Recovered);
					if (!Draft.bRecovered && Draft.Code.Contains(IR::CustomCodeMarker::BodyPrefix, ESearchCase::CaseSensitive))
					{
						Warning(TEXT("DSH9082"), FText::Format(
							LOCTEXT("CustomCodeUnreadable", "The code of the custom node '{0}' carries DreamShader's markers and does not read as what the compiler writes; it is kept verbatim as the body of one function, the functions it embeds included."),
							FText::FromString(Draft.NodeName)));
					}
				}

				FCustomDraft& Draft = Drafts[DraftIndex];
				Draft.bPrimaryOutputRead = Draft.bPrimaryOutputRead || PrimaryOutputRead.Contains(NodeIndex);

				for (const FIRInput& Input : Node.Inputs)
				{
					const bool bKnown = Draft.Inputs.ContainsByPredicate([&Input](const FPinModel& Pin) { return Pin.Name.Equals(Input.Pin, ESearchCase::CaseSensitive); });
					if (!bKnown)
					{
						FPinModel Pin;
						Pin.Name = Input.Pin;
						Pin.Type = Graph.IsValidValue(Input.Value) ? Graph.TypeOf(Input.Value) : FIRType::Float(1);
						Draft.Inputs.Add(MoveTemp(Pin));
					}
				}
				if (Draft.Outputs.IsEmpty())
				{
					for (int32 OutputIndex = 0; OutputIndex < Node.Outputs.Num(); ++OutputIndex)
					{
						FPinModel Pin;
						Pin.Name = Node.OutputNames.IsValidIndex(OutputIndex) ? Node.OutputNames[OutputIndex] : FString(TEXT("Result"));
						Pin.Type = Node.Outputs[OutputIndex];
						Draft.Outputs.Add(MoveTemp(Pin));
					}
				}
			}
		}

		// The signatures the wrapper structs spelled out: exact, where a node's own function has only its pins to go by.
		TMap<FString, const FRecoveredCustomFunction*> HeaderByFunction;
		for (const FCustomDraft& Draft : Drafts)
		{
			if (!Draft.bRecovered)
			{
				continue;
			}
			for (const FRecoveredCustomFunction& Helper : Draft.Recovered.Helpers)
			{
				if (Helper.bHasSignature && !HeaderByFunction.Contains(Helper.Name))
				{
					HeaderByFunction.Add(Helper.Name, &Helper);
				}
			}
		}

		const auto ClaimCustomIdentifier = [this](const FString& Name) -> FString
		{
			// The bodies call each other by these names, so a name that has to change is said out loud.
			const FString Wanted = CanDeclareVerbatim(Name) ? Name : MakeSourceIdentifier(Name, TEXT("CustomFunction"));
			const FString Claimed = FileNames.Claim(Wanted);
			if (!Claimed.Equals(Name, ESearchCase::CaseSensitive))
			{
				Info(TEXT("DSH9075"), FText::Format(
					LOCTEXT("CustomRenamed", "The custom function '{0}' is written as '{1}': the name is taken or is not one the language allows. A custom body that calls it by the old name has to be changed by hand."),
					FText::FromString(Name),
					FText::FromString(Claimed)));
			}
			return Claimed;
		};

		const auto ApplyHeader = [](FCallable& Callable, const FRecoveredCustomFunction& Header)
		{
			Callable.Params.Reset();
			Callable.bHasReturn = !Header.ReturnTypeSpelling.Equals(TEXT("void"), ESearchCase::CaseSensitive);
			Callable.ReturnPinName = TEXT("Result");
			Callable.ReturnTypeSpelling = Callable.bHasReturn ? Header.ReturnTypeSpelling : FString();
			Callable.ReturnType = Callable.bHasReturn ? IR::TypeFromBuiltinRef(MakeTypeRef(Header.ReturnTypeSpelling)) : FIRType::Void();
			for (const FRecoveredCustomParam& Recovered : Header.Params)
			{
				FCallableParam Param;
				Param.PinName = Recovered.Name;
				Param.Identifier = Recovered.Name;
				Param.TypeSpelling = Recovered.TypeSpelling;
				Param.Type = IR::TypeFromBuiltinRef(MakeTypeRef(Recovered.TypeSpelling));
				Param.Direction = Recovered.Direction;
				Callable.Params.Add(MoveTemp(Param));
			}
		};

		const auto AddCustomCallable = [this](FCallable&& Callable, const FString& FunctionName) -> int32
		{
			const int32 Index = Callables.Num();
			if (!FunctionName.IsEmpty())
			{
				CallableByCustomFunction.Add(FunctionName, Index);
			}
			Callables.Add(MoveTemp(Callable));
			return Index;
		};

		for (const FCustomDraft& Draft : Drafts)
		{
			if (!Draft.bRecovered)
			{
				continue;
			}
			for (const FRecoveredCustomFunction& Helper : Draft.Recovered.Helpers)
			{
				if (CallableByCustomFunction.Contains(Helper.Name))
				{
					continue;
				}
				FCallable Callable;
				Callable.Kind = ECallableKind::Custom;
				Callable.Identifier = ClaimCustomIdentifier(Helper.Name);
				Callable.RawBody = Helper.RawBody;
				ApplyHeader(Callable, Helper);
				AddCustomCallable(MoveTemp(Callable), Helper.Name);
			}
		}

		for (FCustomDraft& Draft : Drafts)
		{
			const FString FunctionName = Draft.bRecovered ? Draft.Recovered.Root.Name : FString();

			if (Draft.bRecovered)
			{
				if (const int32* Existing = CallableByCustomFunction.Find(FunctionName))
				{
					// Already declared as a helper of another node, or as the function of an earlier one. What this node
					// wires that the signature does not list are the calls lifted out of the body (legacy rule L8).
					FCallable& Callable = Callables[*Existing];
					for (const FPinModel& Input : Draft.Inputs)
					{
						if (!Callable.FindParamByPin(Input.Name, /* bOutput */ false))
						{
							FCallableParam Param;
							CopyPinToParam(Input, Param);
							Param.Identifier = Input.Name;
							Callable.Params.Add(MoveTemp(Param));
						}
					}
					if (Callable.bHasReturn && Draft.Outputs.Num() > 0)
					{
						Callable.ReturnPinName = Draft.Outputs[0].Name;
					}
					CallableByCustomNode.Add(Draft.NodeKey, *Existing);
					continue;
				}
			}

			FCallable Callable;
			Callable.Kind = ECallableKind::Custom;
			Callable.Identifier = ClaimCustomIdentifier(Draft.bRecovered ? FunctionName : Draft.NodeName);
			if (Draft.bRecovered && !Draft.NodeTitle.IsEmpty() && !Draft.NodeTitle.Equals(FunctionName, ESearchCase::CaseSensitive))
			{
				Callable.NodeTitle = Draft.NodeTitle;
			}

			const FRecoveredCustomFunction* const* Header = Draft.bRecovered ? HeaderByFunction.Find(FunctionName) : nullptr;
			if (Header)
			{
				ApplyHeader(Callable, **Header);
				if (Callable.bHasReturn && Draft.Outputs.Num() > 0)
				{
					Callable.ReturnPinName = Draft.Outputs[0].Name;
				}
				for (const FPinModel& Input : Draft.Inputs)
				{
					if (!Callable.FindParamByPin(Input.Name, /* bOutput */ false))
					{
						FCallableParam Param;
						CopyPinToParam(Input, Param);
						Param.Identifier = Input.Name;
						Callable.Params.Add(MoveTemp(Param));
					}
				}
				Callable.RawBody = Draft.Recovered.Root.RawBody;
			}
			else
			{
				// Output 0 is what the body returns. A void function's node still has one -- a float the builder makes it
				// return (`return 0.0;`) and nobody reads -- and that is how one is told from a function returning a float.
				const bool bVoid = Draft.bRecovered
					&& Draft.Recovered.Root.bZeroFallback
					&& Draft.Recovered.Root.PrimaryOutName.IsEmpty()
					&& !Draft.bPrimaryOutputRead
					&& Draft.Outputs.Num() > 0
					&& Draft.Outputs[0].Type.GraphComponentCount() == 1;

				// Rule L9: the node of a void function with `out` parameters returns the first of them, which its code
				// declares ahead of the body. Every output is an `out` parameter then, the first under that declared name.
				const bool bPrimaryOut = Draft.bRecovered && !Draft.Recovered.Root.PrimaryOutName.IsEmpty() && Draft.Outputs.Num() > 0;

				TArray<FPinModel> Outputs = Draft.Outputs;
				if (bVoid)
				{
					Outputs.RemoveAt(0);
				}
				BuildSignature(Callable, Draft.Inputs, Outputs, (bVoid || bPrimaryOut || Outputs.Num() == 0) ? INDEX_NONE : 0, /* bKeepPinNames */ true, FileNames);
				if (bPrimaryOut)
				{
					for (FCallableParam& Param : Callable.Params)
					{
						if (Param.Direction == EParamDirection::Out && Param.PinName.Equals(Outputs[0].Name, ESearchCase::CaseSensitive))
						{
							Param.Identifier = Draft.Recovered.Root.PrimaryOutName;
							break;
						}
					}
				}

				for (const FCallableParam& Param : Callable.Params)
				{
					if (!Param.Identifier.Equals(Param.PinName, ESearchCase::CaseSensitive))
					{
						Warning(TEXT("DSH9082"), FText::Format(
							LOCTEXT("CustomPinRenamed", "The custom function '{0}' has a pin '{1}' the language cannot declare under that name; it is written as '{2}', and the body still says '{1}'."),
							FText::FromString(Callable.Identifier),
							FText::FromString(Param.PinName),
							FText::FromString(Param.Identifier)));
					}
				}

				if (Draft.bRecovered)
				{
					Callable.RawBody = Draft.Recovered.Root.RawBody;
				}
				else
				{
					// Written by hand: the code is the body, and the includes go back on top of it, where the builder
					// takes them from.
					FString Body = TEXT("\n");
					for (const FString& Include : Draft.Includes)
					{
						Body += FString::Printf(TEXT("#include \"%s\"\n"), *Include);
					}
					Body += Draft.Code.Replace(TEXT("\r\n"), TEXT("\n"));
					if (!Body.EndsWith(TEXT("\n"), ESearchCase::CaseSensitive))
					{
						Body += TEXT("\n");
					}
					Callable.RawBody = MoveTemp(Body);
				}
			}

			const int32 Index = AddCustomCallable(MoveTemp(Callable), FunctionName);
			CallableByCustomNode.Add(Draft.NodeKey, Index);
		}
	}

	// ------------------------------------------------------------------------------------ emission

	void FModuleWriter::EmitMaterialPragma()
	{
		const FIRProduct* Material = nullptr;
		for (const FIRProduct& Product : Source.Products)
		{
			if (Product.Kind == IR::EIRProductKind::Material)
			{
				Material = &Product;
				break;
			}
		}
		if (!Material)
		{
			return;
		}

		const bool bBackend = Options.bEmitBackend || Material->Backend != Options.DefaultBackend;
		if (Material->Settings.Num() == 0 && !bBackend)
		{
			return;
		}

		TUniquePtr<FPragmaDecl> Pragma = MakeUnique<FPragmaDecl>();
		Pragma->PragmaKind = EPragmaKind::Material;
		Pragma->Name = TEXT("material");

		TArray<FString> Keys;
		Material->Settings.GenerateKeyArray(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			AddPragmaArgument(*Pragma, Key, Material->Settings[Key]);
		}
		if (bBackend)
		{
			AddPragmaArgument(*Pragma, TEXT("Backend"), IR::LexToString(Material->Backend));
		}
		AddDeclaration(MoveTemp(Pragma), /* bBlankLineBefore */ true);
	}

	void FModuleWriter::EmitExternPrototypes()
	{
		TArray<int32> Externs;
		for (int32 Index = 0; Index < Callables.Num(); ++Index)
		{
			if (Callables[Index].Kind == ECallableKind::Extern)
			{
				Externs.Add(Index);
			}
		}
		Externs.Sort([this](const int32 A, const int32 B)
		{
			return Callables[A].AssetPath.Compare(Callables[B].AssetPath, ESearchCase::IgnoreCase) < 0;
		});

		for (const int32 Index : Externs)
		{
			const FCallable& Callable = Callables[Index];

			TUniquePtr<FFunctionDecl> Decl = MakeUnique<FFunctionDecl>();
			Decl->Linkage = EFunctionLinkage::Extern;
			Decl->Name = Callable.Identifier;
			FillParams(*Decl, Callable);

			AddDocDescription(Decl->Doc, Callable.Description);
			AddDocDirective(Decl->Doc, Directive::Asset, Callable.AssetPath);
			AddSignatureDirectives(Decl->Doc, Callable);

			AddDeclaration(MoveTemp(Decl), /* bBlankLineBefore */ true);
		}
	}

	void FModuleWriter::EmitCustomFunctions()
	{
		for (const FCallable& Callable : Callables)
		{
			if (Callable.Kind != ECallableKind::Custom)
			{
				continue;
			}

			TUniquePtr<FFunctionDecl> Decl = MakeUnique<FFunctionDecl>();
			Decl->Linkage = EFunctionLinkage::Internal;
			Decl->Name = Callable.Identifier;
			FillParams(*Decl, Callable);
			Decl->bOpaqueBody = true;
			Decl->RawBody = Callable.RawBody;

			AddDocDescription(Decl->Doc, Callable.Description);
			AddDocDirective(Decl->Doc, Directive::Custom, FString());
			if (!Callable.NodeTitle.IsEmpty())
			{
				AddDocDirective(Decl->Doc, Directive::Name, Callable.NodeTitle);
			}
			AddSignatureDirectives(Decl->Doc, Callable);

			AddDeclaration(MoveTemp(Decl), /* bBlankLineBefore */ true);
		}
	}

	void FModuleWriter::AddProductDoc(FDocBlock& Doc, const int32 ProductIndex) const
	{
		const FIRProduct& Product = Source.Products[ProductIndex];
		AddDocDescription(Doc, Product.Description);

		int32 GraphProducts = 0;
		for (const FIRProduct& Other : Source.Products)
		{
			GraphProducts += IsGraphProduct(Other) ? 1 : 0;
		}

		// A full path where the asset has to stay where it is; else the asset's name, when the identifier is not it.
		FString NamePath = Product.AssetPathOverride;
		if (NamePath.IsEmpty() && GraphProducts == 1)
		{
			NamePath = Options.AssetPathOverride;
		}
		if (!NamePath.IsEmpty())
		{
			AddDocDirective(Doc, Directive::Name, NamePath);
		}
		else if (!ProductIdentifiers[ProductIndex].Equals(Product.Name, ESearchCase::CaseSensitive))
		{
			AddDocDirective(Doc, Directive::Name, Product.Name);
		}

		if (!Product.LibraryPath.IsEmpty())
		{
			AddDocDirective(Doc, Directive::Library, Product.LibraryPath);
		}
		if (!DemotedLayers.Contains(ProductIndex))
		{
			if (Product.Kind == IR::EIRProductKind::MaterialLayer)
			{
				AddDocDirective(Doc, Directive::Layer, FString());
			}
			else if (Product.Kind == IR::EIRProductKind::MaterialLayerBlend)
			{
				AddDocDirective(Doc, Directive::LayerBlend, FString());
			}
		}
	}

	static void AddStatementRoots(FGraphWriter& Writer, const FIRGraph& Graph)
	{
		// `UE.VolumetricAdvancedMaterialOutput(...);` and its kind: statements the prune pass keeps as roots.
		for (int32 NodeIndex = 0; NodeIndex < Graph.Nodes.Num(); ++NodeIndex)
		{
			const FIRNode& Node = Graph.Nodes[NodeIndex];
			if (Node.Op == EIROp::Reflected && Node.Outputs.Num() == 0)
			{
				FRootSpec Root;
				Root.Kind = ERootKind::Statement;
				Root.Node = NodeIndex;
				Writer.AddRoot(MoveTemp(Root));
			}
		}
	}

	void FModuleWriter::ResolveLayoutHints(const int32 ProductIndex, const FGraphWriter& Writer)
	{
		if (!Options.bEmitLayout)
		{
			return;
		}

		const FIRProduct& Product = Source.Products[ProductIndex];
		const FIRGraph& Graph = Product.Graph;

		int32 Dropped = 0;
		for (const IR::FIRLayoutHint& Hint : Graph.LayoutHints)
		{
			if (Hint.Kind.Equals(TEXT("Comment"), ESearchCase::CaseSensitive))
			{
				const bool bKnown = CommentHints.ContainsByPredicate([&Hint](const IR::FIRLayoutHint& Other)
				{
					return Other.Name.Equals(Hint.Name, ESearchCase::CaseSensitive)
						&& Other.X == Hint.X && Other.Y == Hint.Y
						&& Other.bHasSize == Hint.bHasSize && Other.W == Hint.W && Other.H == Hint.H;
				});
				if (!bKnown && !Hint.Name.IsEmpty())
				{
					CommentHints.Add(Hint);
				}
				continue;
			}
			if (!Hint.Kind.Equals(TEXT("Node"), ESearchCase::CaseSensitive) || Hint.Var.IsEmpty())
			{
				continue;
			}

			FString Var = Hint.Var;
			if (Var.StartsWith(TEXT("$"), ESearchCase::CaseSensitive))
			{
				// An importer's hint: the binding says which node, the body says what that node is called now.
				Var.Reset();
				for (const IR::FIRStatementBinding& Binding : Graph.StatementBindings)
				{
					if (!Binding.Name.Equals(Hint.Var, ESearchCase::CaseSensitive) || !Graph.Nodes.IsValidIndex(Binding.Value.Node))
					{
						continue;
					}
					const FIRNode& Node = Graph.Nodes[Binding.Value.Node];
					Var = Node.Op == EIROp::FunctionOutput ? FindTextProperty(Node, IR::Prop::OutputName) : Writer.FindPlacedName(Binding.Value.Node);
					break;
				}
				if (Var.IsEmpty())
				{
					++Dropped;
					continue;
				}
			}

			FPlacedName Placed;
			Placed.Name = Var;
			Placed.ProductIndex = ProductIndex;
			Placed.X = Hint.X;
			Placed.Y = Hint.Y;
			PlacedNames.Add(MoveTemp(Placed));
		}

		if (Dropped > 0)
		{
			Info(TEXT("DSH9077"), FText::Format(
				LOCTEXT("LayoutHintsDropped", "{0} node position(s) of '{1}' belong to values the source writes inline, and a position is kept by variable name; those nodes are placed by the layout pass when the file is built."),
				FText::AsNumber(Dropped),
				FText::FromString(Product.Name)));
		}
	}

	void FModuleWriter::EmitMaterialProduct(const int32 ProductIndex)
	{
		const FIRProduct& Product = Source.Products[ProductIndex];
		const FIRGraph& Graph = Product.Graph;

		TUniquePtr<FFunctionDecl> Decl = MakeUnique<FFunctionDecl>();
		Decl->Linkage = EFunctionLinkage::Export;
		Decl->ReturnType = MakeTypeRef(TEXT("void"));
		Decl->Name = ProductIdentifiers[ProductIndex];
		AddProductDoc(Decl->Doc, ProductIndex);

		FNameScope FunctionNames(&FileNames);
		const FString MaterialParam = FunctionNames.Claim(TEXT("m"));
		{
			FParam Param;
			Param.Direction = EParamDirection::InOut;
			Param.Type = MakeTypeRef(TEXT("material"));
			Param.Name = MaterialParam;
			Decl->Params.Add(MoveTemp(Param));
		}

		FGraphWriter Writer(*this, Product, FunctionNames);
		if (Graph.Nodes.IsValidIndex(Graph.Sink))
		{
			for (const FIRInput& Input : Graph.Nodes[Graph.Sink].Inputs)
			{
				if (!Input.Value.IsValid())
				{
					continue;
				}
				FRootSpec Root;
				Root.Value = Input.Value;
				Root.Target = MaterialParam;
				if (Input.Pin.Equals(TEXT("MaterialAttributes"), ESearchCase::CaseSensitive))
				{
					Root.Kind = ERootKind::SinkWhole;
				}
				else
				{
					Root.Kind = ERootKind::SinkAttribute;
					Root.PinName = Input.Pin;
				}
				Writer.AddRoot(MoveTemp(Root));
			}
		}
		AddStatementRoots(Writer, Graph);

		Decl->Body = Writer.Build();
		ResolveLayoutHints(ProductIndex, Writer);
		AddDeclaration(MoveTemp(Decl), /* bBlankLineBefore */ true);
	}

	void FModuleWriter::EmitFunctionProduct(const int32 ProductIndex)
	{
		const FIRProduct& Product = Source.Products[ProductIndex];
		const FIRGraph& Graph = Product.Graph;
		const int32* CallableIndex = CallableByProduct.Find(ProductIndex);
		if (!CallableIndex)
		{
			return;
		}
		const FCallable& Callable = Callables[*CallableIndex];

		TUniquePtr<FFunctionDecl> Decl = MakeUnique<FFunctionDecl>();
		Decl->Linkage = EFunctionLinkage::Export;
		Decl->Name = Callable.Identifier;
		FillParams(*Decl, Callable);
		AddProductDoc(Decl->Doc, ProductIndex);
		AddSignatureDirectives(Decl->Doc, Callable);

		FNameScope FunctionNames(&FileNames);
		for (const FCallableParam& Param : Callable.Params)
		{
			FunctionNames.Reserve(Param.Identifier);
		}

		FGraphWriter Writer(*this, Product, FunctionNames);

		TMap<FString, int32> InputNodeByPin;
		for (const int32 NodeIndex : Graph.FunctionInputs)
		{
			if (!Graph.Nodes.IsValidIndex(NodeIndex))
			{
				continue;
			}
			const FString PinName = FindTextProperty(Graph.Nodes[NodeIndex], IR::Prop::InputName);
			if (const FCallableParam* Param = Callable.FindParamByPin(PinName, /* bOutput */ false))
			{
				Writer.BindInput(NodeIndex, Param->Identifier);
				InputNodeByPin.Add(Param->PinName, NodeIndex);
			}
		}

		const bool bLayerShaped = !DemotedLayers.Contains(ProductIndex)
			&& (Product.Kind == IR::EIRProductKind::MaterialLayer || Product.Kind == IR::EIRProductKind::MaterialLayerBlend);

		FRootSpec Return;
		bool bHasReturnRoot = false;
		for (const int32 NodeIndex : Graph.FunctionOutputs)
		{
			if (!Graph.Nodes.IsValidIndex(NodeIndex))
			{
				continue;
			}
			const FIRNode& Node = Graph.Nodes[NodeIndex];
			const FString PinName = FindTextProperty(Node, IR::Prop::OutputName);
			const FIRValue Value = Node.Operands.Num() > 0 ? Node.Operands[0] : FIRValue::None();
			if (!Value.IsValid())
			{
				Warning(TEXT("DSH9084"), FText::Format(
					LOCTEXT("OutputUnconnected", "The output '{0}' of '{1}' is not connected to anything; nothing is written to it."),
					FText::FromString(PinName),
					FText::FromString(Product.Name)));
				continue;
			}

			if (Callable.bHasReturn && !bHasReturnRoot && PinName.Equals(Callable.ReturnPinName, ESearchCase::CaseSensitive))
			{
				Return.Kind = ERootKind::OutputReturn;
				Return.Value = Value;
				bHasReturnRoot = true;
				continue;
			}

			// A layer's and a blend's one output is the `inout material`, whatever the pin is called.
			const FCallableParam* Param = Callable.FindParamByPin(PinName, /* bOutput */ true);
			if (!Param && bLayerShaped && Callable.Params.Num() > 0)
			{
				Param = &Callable.Params.Last();
			}
			if (!Param)
			{
				Error(TEXT("DSH9078"), FText::Format(
					LOCTEXT("OutputWithoutParam", "The output '{0}' of '{1}' did not become a parameter; nothing is written to it."),
					FText::FromString(PinName),
					FText::FromString(Product.Name)));
				continue;
			}

			FRootSpec Root;
			Root.Value = Value;
			Root.Target = Param->Identifier;
			Root.PinName = PinName;
			if (Param->Direction == EParamDirection::InOut && Param->Type.IsMaterial())
			{
				Root.Kind = ERootKind::OutputMaterial;
				// A blend's result arrives through no input, even when one of its inputs has the output's name.
				const bool bBlendResult = bLayerShaped && Product.Kind == IR::EIRProductKind::MaterialLayerBlend;
				const int32* InputNode = bBlendResult ? nullptr : InputNodeByPin.Find(Param->PinName);
				Root.Node = InputNode ? *InputNode : INDEX_NONE;
			}
			else
			{
				Root.Kind = ERootKind::OutputAssign;
			}
			Writer.AddRoot(MoveTemp(Root));
		}
		AddStatementRoots(Writer, Graph);
		if (bHasReturnRoot)
		{
			Writer.AddRoot(MoveTemp(Return));
		}

		Decl->Body = Writer.Build();
		ResolveLayoutHints(ProductIndex, Writer);
		AddDeclaration(MoveTemp(Decl), /* bBlankLineBefore */ true);
	}

	void FModuleWriter::EmitProductFunctions()
	{
		for (int32 ProductIndex = 0; ProductIndex < Source.Products.Num(); ++ProductIndex)
		{
			const FIRProduct& Product = Source.Products[ProductIndex];
			switch (Product.Kind)
			{
			case IR::EIRProductKind::Material:
				EmitMaterialProduct(ProductIndex);
				break;
			case IR::EIRProductKind::MaterialFunction:
			case IR::EIRProductKind::MaterialLayer:
			case IR::EIRProductKind::MaterialLayerBlend:
				EmitFunctionProduct(ProductIndex);
				break;
			case IR::EIRProductKind::MaterialInstance:
				Error(TEXT("DSH9078"), FText::Format(
					LOCTEXT("InstanceProduct", "'{0}' is a material instance, which is written as a '.dsi' (PrintDreamShaderInstance), not as part of a '.dss'."),
					FText::FromString(Product.Name)));
				break;
			}
		}
	}

	void FModuleWriter::EmitLayoutPragmas()
	{
		if (!Options.bEmitLayout)
		{
			return;
		}

		// A hint is matched by variable name in every product of the file, so a name two products place differently
		// cannot be written: it would move both.
		bool bFirst = true;
		TSet<FString> Ambiguous;
		for (int32 Index = 0; Index < PlacedNames.Num(); ++Index)
		{
			for (int32 Other = Index + 1; Other < PlacedNames.Num(); ++Other)
			{
				if (PlacedNames[Index].ProductIndex != PlacedNames[Other].ProductIndex
					&& PlacedNames[Index].Name.Equals(PlacedNames[Other].Name, ESearchCase::CaseSensitive)
					&& (PlacedNames[Index].X != PlacedNames[Other].X || PlacedNames[Index].Y != PlacedNames[Other].Y))
				{
					Ambiguous.Add(PlacedNames[Index].Name);
				}
			}
		}
		if (Ambiguous.Num() > 0)
		{
			Info(TEXT("DSH9077"), FText::Format(
				LOCTEXT("LayoutHintsAmbiguous", "{0} variable name(s) are placed differently by more than one function of this file, and '#pragma layout' goes by name for the whole file; their positions are not kept."),
				FText::AsNumber(Ambiguous.Num())));
		}

		TSet<FString> Written;
		for (const FPlacedName& Placed : PlacedNames)
		{
			if (Ambiguous.Contains(Placed.Name) || Written.Contains(Placed.Name))
			{
				continue;
			}
			Written.Add(Placed.Name);

			TUniquePtr<FPragmaDecl> Pragma = MakeUnique<FPragmaDecl>();
			Pragma->PragmaKind = EPragmaKind::Layout;
			Pragma->Name = TEXT("layout");
			AddPragmaArgument(*Pragma, FString(), TEXT("Node"));
			AddPragmaArgument(*Pragma, TEXT("Var"), Placed.Name);
			AddPragmaArgument(*Pragma, TEXT("X"), FString::FromInt(Placed.X));
			AddPragmaArgument(*Pragma, TEXT("Y"), FString::FromInt(Placed.Y));
			AddDeclaration(MoveTemp(Pragma), bFirst);
			bFirst = false;
		}

		for (const IR::FIRLayoutHint& Hint : CommentHints)
		{
			TUniquePtr<FPragmaDecl> Pragma = MakeUnique<FPragmaDecl>();
			Pragma->PragmaKind = EPragmaKind::Layout;
			Pragma->Name = TEXT("layout");
			AddPragmaArgument(*Pragma, FString(), TEXT("Comment"));
			AddPragmaArgument(*Pragma, TEXT("Name"), SingleLine(Hint.Name), /* bForceQuoted */ true);
			AddPragmaArgument(*Pragma, TEXT("X"), FString::FromInt(Hint.X));
			AddPragmaArgument(*Pragma, TEXT("Y"), FString::FromInt(Hint.Y));
			if (Hint.bHasSize)
			{
				AddPragmaArgument(*Pragma, TEXT("W"), FString::FromInt(Hint.W));
				AddPragmaArgument(*Pragma, TEXT("H"), FString::FromInt(Hint.H));
			}
			if (Hint.bHasColor)
			{
				AddPragmaArgument(*Pragma, TEXT("Color"), FString::Printf(
					TEXT("%s %s %s %s"),
					*FormatDreamShaderFloatLiteral(Hint.Color[0]),
					*FormatDreamShaderFloatLiteral(Hint.Color[1]),
					*FormatDreamShaderFloatLiteral(Hint.Color[2]),
					*FormatDreamShaderFloatLiteral(Hint.Color[3])), /* bForceQuoted */ true);
			}
			AddDeclaration(MoveTemp(Pragma), bFirst);
			bFirst = false;
		}
	}

	void FModuleWriter::ApplyHeaderComments()
	{
		if (Options.HeaderComments.Num() == 0)
		{
			return;
		}

		TArray<FLangComment> Comments;
		for (int32 Index = 0; Index < Options.HeaderComments.Num(); ++Index)
		{
			const FString Line = SingleLine(Options.HeaderComments[Index]);
			FLangComment Comment;
			Comment.Text = Line.IsEmpty() ? FString(TEXT("//")) : FString(TEXT("// ")) + Line;
			Comment.Span.Line = Index + 1;
			Comments.Add(MoveTemp(Comment));
		}

		if (Module->Declarations.Num() == 0 || !Module->Declarations[0])
		{
			Module->TrailingComments = MoveTemp(Comments);
			return;
		}

		// The printer leaves a blank line under leading comments where the node's line says there was one.
		FDecl& First = *Module->Declarations[0];
		First.Span.Line = Comments.Num() + 2;
		First.Span.Length = 1;
		Module->Trivia.FindOrAdd(&First).Leading = MoveTemp(Comments);
	}

	TUniquePtr<FModule> FModuleWriter::Build()
	{
		Module = MakeUnique<FModule>();
		Module->FilePath = Source.SourceFilePath;
		Module->FileKind = ELangFileKind::Dss;

		// Names in the order that matters when two collide: the assets' own, then what they call, then the uniforms.
		CollectUniforms();
		CollectProductCallables();
		CollectCustomCallables();
		CollectExternCallables();
		NameUniforms();

		EmitMaterialPragma();
		EmitExternPrototypes();
		EmitUniforms();
		EmitCustomFunctions();
		EmitProductFunctions();
		EmitLayoutPragmas();
		ApplyHeaderComments();

		return MoveTemp(Module);
	}
}

namespace UE::DreamShader::Lang
{
	TUniquePtr<FModule> BuildDreamShaderAstFromIR(
		const IR::FIRModule& Module,
		const IR::FBuiltinCatalog& Catalog,
		const FIRToAstOptions& Options,
		FLangDiagnosticSink& Diagnostics)
	{
		DecompileAst::FModuleWriter Writer(Module, Catalog, Options, Diagnostics);
		return Writer.Build();
	}
}

#undef LOCTEXT_NAMESPACE
