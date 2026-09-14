// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamShader IR: a typed value DAG per product, and the products a source file yields.
//
// A node is an op, its operands or named inputs, its literal properties, and the types of its
// outputs. A value is (node, output slot). A product is one asset the file will become -- a
// material, a material function, a layer, a layer blend -- with its own graph, its settings and
// the metadata the asset carries. A module is the products of one source file.
//
// What is deliberately NOT here: positions (layout is a later pass over the graph and the digest
// must not see it), engine objects (the emitter is the only thing that touches UObjects), and any
// decision the emitter would have to make (a node says exactly which class and which pins; if the
// emitter has to think, the builder or a pass did not finish its job).
//
// Source positions ARE here, on every node: FIRSourceRef carries the span the node came from and,
// for a node made while inlining a helper, the span of the call site as well. The emitter writes
// them into the asset so the editor can jump from a node back to the line (plan §13.2).
//
// FROZEN for batch 1 (M2+M3).

#pragma once

#include "CoreMinimal.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangSource.h"

namespace UE::DreamShader::IR
{
	// ----------------------------------------------------------------------------- values

	/** A handle to one output of one node in the enclosing FIRGraph. */
	struct FIRValue
	{
		int32 Node = INDEX_NONE;
		int32 Output = 0;

		bool IsValid() const { return Node != INDEX_NONE; }
		bool operator==(const FIRValue& Other) const { return Node == Other.Node && Output == Other.Output; }
		bool operator!=(const FIRValue& Other) const { return !(*this == Other); }
		static FIRValue None() { return FIRValue(); }
	};

	/** A named input binding: the pin an engine node exposes and the value feeding it. */
	struct FIRInput
	{
		FString Pin;
		FIRValue Value;
	};

	// ------------------------------------------------------------------------- properties

	enum class EIRPropertyKind : uint8
	{
		Bool,
		Int,
		Float,
		/** Up to four floats; Float4 with N = 2 is a float2 default and so on. */
		Float4,
		String,
		Name,
		/** The enumerator spelling without its prefix: "SAMPLERTYPE_Normal" is written "Normal". */
		Enum,
		/** An asset object path. */
		Object,
		StringList,
	};

	struct DREAMSHADERLANG_API FIRPropertyValue
	{
		EIRPropertyKind Kind = EIRPropertyKind::Float;
		bool B = false;
		int64 I = 0;
		double F = 0.0;
		double V[4] = { 0.0, 0.0, 0.0, 0.0 };
		/** Float4: how many of V are meaningful (1..4). */
		int32 N = 1;
		FString S;
		TArray<FString> List;

		static FIRPropertyValue MakeBool(bool InB);
		static FIRPropertyValue MakeInt(int64 InI);
		static FIRPropertyValue MakeFloat(double InF);
		static FIRPropertyValue MakeFloat4(const double* InV, int32 InN);
		static FIRPropertyValue MakeString(const FString& InS);
		static FIRPropertyValue MakeName(const FString& InS);
		static FIRPropertyValue MakeEnum(const FString& InS);
		static FIRPropertyValue MakeObject(const FString& InPath);
		static FIRPropertyValue MakeStringList(const TArray<FString>& InList);

		/** A stable, culture-invariant rendering for dumps and dedupe keys. */
		FString ToString() const;
		bool operator==(const FIRPropertyValue& Other) const;
	};

	struct FIRProperty
	{
		FString Name;
		FIRPropertyValue Value;
	};

	/**
	 * The property names the builder writes and the emitter reads. Spelled once here so the two
	 * cannot drift; a property not in this list is a reflected engine property passed through by
	 * its engine name.
	 */
	namespace Prop
	{
		inline const TCHAR* const Value = TEXT("Value");                     // Constant: Float4
		inline const TCHAR* const ParameterName = TEXT("ParameterName");     // Parameter/TextureParameter: Name
		inline const TCHAR* const Group = TEXT("Group");                     // Name
		inline const TCHAR* const Description = TEXT("Description");         // String
		inline const TCHAR* const SliderMin = TEXT("SliderMin");             // Float
		inline const TCHAR* const SliderMax = TEXT("SliderMax");             // Float
		inline const TCHAR* const SortPriority = TEXT("SortPriority");       // Int
		inline const TCHAR* const DefaultValue = TEXT("DefaultValue");       // Parameter: Float4 (N = component count)
		inline const TCHAR* const IsStatic = TEXT("IsStatic");               // Parameter (bool): Bool -- StaticBoolParameter/StaticSwitchParameter
		inline const TCHAR* const DefaultAsset = TEXT("DefaultAsset");       // TextureParameter: Object
		inline const TCHAR* const SamplerType = TEXT("SamplerType");         // TextureParameter/TextureSample: Enum ("Color", "Normal", "LinearColor"...)
		inline const TCHAR* const MipValueMode = TEXT("MipValueMode");       // TextureSample: Enum
		inline const TCHAR* const Mask = TEXT("Mask");                       // Swizzle: String, canonical "xyzw" subset in order
		inline const TCHAR* const InputName = TEXT("InputName");             // FunctionInput: Name
		inline const TCHAR* const InputType = TEXT("InputType");             // FunctionInput: Enum ("Scalar", "Vector2", "Vector3", "Vector4", "Texture2D", "TextureCube", "MaterialAttributes", "StaticBool", "Bool", "Substrate"...)
		inline const TCHAR* const IsOptional = TEXT("IsOptional");           // FunctionInput: Bool (had a default)
		inline const TCHAR* const PreviewValue = TEXT("PreviewValue");       // FunctionInput: Float4
		inline const TCHAR* const OutputName = TEXT("OutputName");           // FunctionOutput: Name
		inline const TCHAR* const FunctionPath = TEXT("FunctionPath");       // FunctionCall: Object (asset path); "" + IsLocalFunction for a same-module export
		inline const TCHAR* const LocalFunction = TEXT("LocalFunction");     // FunctionCall: Int (product index in this module) when calling a same-file export
		inline const TCHAR* const Code = TEXT("Code");                       // Custom: String
		inline const TCHAR* const OutputType = TEXT("OutputType");           // Custom: Enum ("Float1".."Float4", "MaterialAttributes")
		inline const TCHAR* const IncludeFilePaths = TEXT("IncludeFilePaths"); // Custom: StringList
		inline const TCHAR* const AdditionalOutputs = TEXT("AdditionalOutputs"); // Custom: StringList "Name:Type"
		inline const TCHAR* const AttributeSetTypes = TEXT("AttributeSetTypes"); // SetMaterialAttributes: StringList of attribute names, in Inputs order
		inline const TCHAR* const ClassSpecifier = TEXT("ClassSpecifier");   // Reflected: String, what the author wrote
	}

	// ------------------------------------------------------------------------------ nodes

	/**
	 * Where a node came from. File is the file Span lies in -- for a node made while inlining a
	 * helper that lives in an included `.dsh`, that is the header. CallSite and CallSiteFile are set
	 * only for nodes made while inlining, and name the call expression in the CALLER's file, which
	 * is a different file exactly when the helper was included.
	 */
	struct FIRSourceRef
	{
		FString File;
		Lang::FLangSpan Span;
		Lang::FLangSpan CallSite;
		FString CallSiteFile;
		bool HasCallSite() const { return CallSite.Length > 0; }
	};

	struct DREAMSHADERLANG_API FIRNode
	{
		EIROp Op = EIROp::Constant;
		/**
		 * Positional operands: core math ops, Swizzle, Append, Select, Compare, StaticSwitch,
		 * TextureSample, FunctionOutput, Convert, Broadcast. TextureSample is the one op with
		 * optional operands: its Operands are ALWAYS four, [Texture, UV, Sampler, Level], with
		 * FIRValue::None() in a slot that was not given -- so a reader indexes, never counts.
		 */
		TArray<FIRValue> Operands;
		/** Named inputs: Reflected, FunctionCall, Custom, Make/Set/GetMaterialAttributes, MaterialSink. */
		TArray<FIRInput> Inputs;
		TArray<FIRProperty> Properties;
		/**
		 * One per output slot; Outputs[0] is the default. Empty only for the statement nodes:
		 * MaterialSink, FunctionOutput, and a Reflected custom-output class (`UE.VolumetricAdvancedMaterialOutput(...)`
		 * and friends), which is a root the prune pass keeps and a value nobody may read.
		 */
		TArray<FIRType> Outputs;
		/** Parallel to Outputs when the node has named outputs (Reflected, FunctionCall, Custom, TextureSample, GetMaterialAttributes); else empty. */
		TArray<FString> OutputNames;
		/** Reflected: the catalog ShortName; FunctionCall: the asset object path; Custom: the function name. */
		FString ClassName;
		/** Reflected: index into the catalog; else INDEX_NONE. */
		int32 CatalogIndex = INDEX_NONE;
		/** The variable name the value was first assigned to, for node descriptions and dumps; may be empty. */
		FString DebugName;
		FIRSourceRef Source;
		/** Index into FIRGraph::Regions, or INDEX_NONE. */
		int32 Region = INDEX_NONE;
		/** Filled by the dedupe pass; empty before it runs. Two nodes with equal keys are the same node. */
		FString DedupeKey;

		const FIRProperty* FindProperty(const TCHAR* Name) const;
		FIRProperty* FindProperty(const TCHAR* Name);
		const FIRInput* FindInput(const FString& Pin) const;
		int32 FindOutput(const FString& Name) const;
		/** A root with no value: the sink, a function output, or a reflected custom-output node. */
		bool IsStatement() const
		{
			return Op == EIROp::MaterialSink
				|| Op == EIROp::FunctionOutput
				|| (Op == EIROp::Reflected && Outputs.Num() == 0);
		}
	};

	/** A `#pragma region` box: the nodes with Region == index are drawn inside it. */
	struct FIRRegion
	{
		FString Name;
		/** Nesting: the enclosing region or INDEX_NONE. */
		int32 Parent = INDEX_NONE;
		Lang::FLangSpan Span;
	};

	/** A `#pragma layout(...)` line, carried through untouched for the layout pass. */
	struct FIRLayoutHint
	{
		/** "Node" or "Comment". */
		FString Kind;
		/** Node: the variable name the node was assigned to. */
		FString Var;
		/** Comment: the box title. */
		FString Name;
		int32 X = 0;
		int32 Y = 0;
		int32 W = 0;
		int32 H = 0;
		bool bHasSize = false;
	};

	struct DREAMSHADERLANG_API FIRGraph
	{
		TArray<FIRNode> Nodes;
		TArray<FIRRegion> Regions;
		TArray<FIRLayoutHint> LayoutHints;
		/** Material products: the MaterialSink node. INDEX_NONE for functions. */
		int32 Sink = INDEX_NONE;
		/** Function products: FunctionInput nodes in declaration order, and FunctionOutput nodes in declaration order. */
		TArray<int32> FunctionInputs;
		TArray<int32> FunctionOutputs;

		int32 AddNode(FIRNode&& Node);
		const FIRNode& operator[](int32 Index) const { return Nodes[Index]; }
		FIRNode& operator[](int32 Index) { return Nodes[Index]; }
		const FIRType& TypeOf(FIRValue Value) const;
		bool IsValidValue(FIRValue Value) const;

		/** Nodes in an order where every operand precedes its user; the emitter walks this. */
		TArray<int32> TopologicalOrder() const;
		/** Every value a node reads, operands and named inputs together. */
		static void CollectInputValues(const FIRNode& Node, TArray<FIRValue>& OutValues);
	};

	// --------------------------------------------------------------------------- products

	enum class EIRProductKind : uint8
	{
		Material,
		MaterialFunction,
		MaterialLayer,
		MaterialLayerBlend,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EIRProductKind Kind);

	enum class EIRBackend : uint8
	{
		Graph,
		ThinCustom,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EIRBackend Backend);

	struct FIRProduct
	{
		EIRProductKind Kind = EIRProductKind::Material;
		/** The asset name: the exported function's name, or `/// @name`. */
		FString Name;
		/** `/// @name /Game/Some/Path/M_X` when the doc block gave a full path; else empty and the root rules apply. */
		FString AssetPathOverride;
		/** `#pragma material(...)` keys as written, Backend removed. Materials only. */
		TMap<FString, FString> Settings;
		EIRBackend Backend = EIRBackend::Graph;
		/** `/// @library Cat|Sub`: exposes the function to the material function library. Functions only. */
		FString LibraryPath;
		/** `/// @desc`. */
		FString Description;
		/** The bound function this product came from (FBoundModule::Functions index). */
		int32 BoundFunctionIndex = INDEX_NONE;
		FIRSourceRef Source;
		FIRGraph Graph;
	};

	struct DREAMSHADERLANG_API FIRModule
	{
		FString SourceFilePath;
		/** Resolved include paths, in first-seen order, for the dependency graph and the build key. */
		TArray<FString> Includes;
		TArray<FIRProduct> Products;

		int32 CountProducts(EIRProductKind Kind) const;
		const FIRProduct* FindProduct(const FString& Name) const;
	};
}
