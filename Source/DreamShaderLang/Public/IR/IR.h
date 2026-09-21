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
// them into the asset so the editor can jump from a node back to the line.

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
		inline const TCHAR* const BlendInputRelevance = TEXT("BlendInputRelevance"); // FunctionInput of a blend: Enum ("Bottom", "Top"); absent is General
		inline const TCHAR* const OutputName = TEXT("OutputName");           // FunctionOutput: Name
		inline const TCHAR* const FunctionPath = TEXT("FunctionPath");       // FunctionCall: Object (asset path); "" + IsLocalFunction for a same-module export
		inline const TCHAR* const LocalFunction = TEXT("LocalFunction");     // FunctionCall: Int (product index in this module) when calling a same-file export
		inline const TCHAR* const DeclaredInputs = TEXT("DeclaredInputs");   // FunctionCall to an asset: StringList, every input the prototype declares, in order -- where an input the asset names otherwise is found
		inline const TCHAR* const Code = TEXT("Code");                       // Custom: String
		inline const TCHAR* const OutputType = TEXT("OutputType");           // Custom: Enum ("Float1".."Float4", "MaterialAttributes")
		inline const TCHAR* const IncludeFilePaths = TEXT("IncludeFilePaths"); // Custom: StringList
		inline const TCHAR* const AdditionalOutputs = TEXT("AdditionalOutputs"); // Custom: StringList "Name:Type"
		inline const TCHAR* const AttributeSetTypes = TEXT("AttributeSetTypes"); // SetMaterialAttributes: StringList of attribute names, in Inputs order
		inline const TCHAR* const ClassSpecifier = TEXT("ClassSpecifier");   // Reflected: String, what the author wrote
		/** Not a property: the one named input a FunctionInput may have, the graph value wired to its Preview pin. */
		inline const TCHAR* const PreviewPin = TEXT("Preview");
		inline const TCHAR* const WrittenPins = TEXT("WrittenPins");         // Reflected: StringList "Pin=Written", inputs the source named by another spelling than the member's; the emitter looks that spelling up on the live node first
		inline const TCHAR* const LateBoundPins = TEXT("LateBoundPins");     // Reflected: StringList, inputs named by what the catalog does not list; the emitter finds them on the live node (legacy rule L24)
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
		/** Comment: `Color = "r g b a"` was given. */
		bool bHasColor = false;
		/** Comment: box colour, linear RGBA; the default is the layout pass's own. */
		float Color[4] = { 0.10f, 0.16f, 0.22f, 0.35f };
	};

	/** One statement that bound a named variable: what the graph-debug table maps a source line to. */
	struct FIRStatementBinding
	{
		/** The variable the statement assigned. */
		FString Name;
		/** The value it bound. */
		FIRValue Value;
		/** The statement. */
		FIRSourceRef Source;
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
		/** ParameterName of every uniform the prune pass removed (the data behind DSH4390); feeds FIRParameterSchemaEntry::bPruned. */
		TArray<FString> PrunedParameters;
		/** Filled by the IR builder in statement order: every statement that bound a named variable (probes and breakpoints). */
		TArray<FIRStatementBinding> StatementBindings;

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
		/** A `.dsi`: one UMaterialInstanceConstant. Graph is empty; Instance carries the assignments. */
		MaterialInstance,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EIRProductKind Kind);

	enum class EIRBackend : uint8
	{
		Graph,
		ThinCustom,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EIRBackend Backend);

	/**
	 * `#pragma material(Substrate = ...)` (Substrate sugar S4): what a material written against the legacy attributes
	 * becomes in a project that has Substrate on.
	 */
	enum class EIRSubstrateMode : uint8
	{
		/** The attributes as written, whatever the project: the engine converts them when it has to. The default. */
		Legacy,
		/**
		 * One source for both kinds of project. With Substrate off, the attributes as written; with it on, the shading
		 * attributes folded into one Substrate.ShadingModels node on FrontMaterial, the rest left on the material.
		 */
		Bridge,
		/** The source drives FrontMaterial itself; with Substrate off that is an error, not an asset the engine cannot compile. */
		Native,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EIRSubstrateMode Mode);

	// ------------------------------------------------------------------------ instances (.dsi)

	/** The engine parameter kinds an instance can override. Mirrors EMaterialParameterType's set; never cast from it. */
	enum class EIRParameterKind : uint8
	{
		Scalar,
		Vector,
		DoubleVector,
		Texture,
		TextureCollection,
		Font,
		RuntimeVirtualTexture,
		SparseVolumeTexture,
		StaticSwitch,
		ParameterCollection,
		StaticComponentMask,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EIRParameterKind Kind);
	DREAMSHADERLANG_API bool TryParseParameterKind(const FString& Text, EIRParameterKind& OutKind);

	/** EMaterialParameterAssociation without the engine. Only Global is expressible in a `.dsi` today. */
	enum class EIRParameterAssociation : uint8
	{
		Global,
		Layer,
		Blend,
	};

	/** One parameter of the parent, as an instance sees it. */
	struct FIRParameterSchemaEntry
	{
		FString Name;
		EIRParameterKind Kind = EIRParameterKind::Scalar;
		EIRParameterAssociation Association = EIRParameterAssociation::Global;
		int32 AssociationIndex = INDEX_NONE;
		/** The uniform's declared type when the parent is DreamShader source (float3, int, bool, Texture2D); Error when unknown. */
		FIRType DeclaredType = FIRType::Error();
		/** Texture-like kinds: the dimension when known. */
		Lang::ETextureKind TextureKind = Lang::ETextureKind::None;
		/** The parent's effective value (its default, or a parent instance's override); encoded like FIRInstanceOverride::Value. */
		FIRPropertyValue ParentValue;
		int32 ParentFontPage = 0;
		/** Declared by the parent source but pruned (DSH4390), so the parent asset has no such parameter. */
		bool bPruned = false;
		FString Group;
		int32 SortPriority = 0;
		/** Where the parent declares it, when known. */
		FString DeclFile;
		Lang::FLangSpan DeclSpan;
	};

	struct DREAMSHADERLANG_API FIRParameterSchema
	{
		/** False when no schema could be produced; name and type checks are then skipped (DSH7263). */
		bool bValid = false;
		/** "ir" (the parent's source, lowered this run) or "asset" (read off the loaded parent). */
		FString Origin;
		FString ParentObjectPath;
		/** The DreamShader source producing the parent; empty for a foreign parent. */
		FString ParentSourceFile;
		bool bParentIsInstance = false;
		TArray<FIRParameterSchemaEntry> Parameters;

		/** Case-sensitive. */
		int32 Find(const FString& Name, EIRParameterAssociation Association = EIRParameterAssociation::Global, int32 AssociationIndex = INDEX_NONE) const;
		/** For did-you-mean and the case-only mismatch (DSH7264). */
		int32 FindIgnoreCase(const FString& Name) const;
		/** Sorted "name|association|index|kind|type" lines. Tests and language service; deliberately not in the build key. */
		FString MakeFingerprint() const;
	};

	/** One `uniform` of a `.dsi`: an assignment to one parent parameter. */
	struct FIRInstanceOverride
	{
		/** The engine parameter name: `@name`, else the identifier. */
		FString ParameterName;
		/** The identifier as written; equals ParameterName unless `@name` was used. */
		FString VariableName;
		EIRParameterKind Kind = EIRParameterKind::Scalar;
		EIRParameterAssociation Association = EIRParameterAssociation::Global;
		int32 AssociationIndex = INDEX_NONE;
		/**
		 * Scalar: Float4 N=1. Vector, DoubleVector: Float4 N=4 (channels the source did not write carry the
		 * parent's value). StaticSwitch: Bool. StaticComponentMask: Float4 N=4 of 0/1. Texture-like kinds and
		 * Font: Object, "" = an explicit None.
		 */
		FIRPropertyValue Value;
		/** Font only. */
		int32 FontPage = 0;
		/** The spelled type; Error when the payload came from decompiling an instance of a foreign parent. */
		FIRType DeclaredType = FIRType::Error();
		FIRSourceRef Source;
	};

	/** The payload of a MaterialInstance product. */
	struct FIRInstance
	{
		/** `Parent = ...` as written. */
		FString ParentReference;
		/** Resolved by the host before binding; empty in an engine-free check that could not resolve it. */
		FString ParentObjectPath;
		TArray<FIRInstanceOverride> Overrides;
		/** The other `#pragma instance` keys, in source order, values as written (quotes removed). */
		TArray<TPair<FString, FString>> Settings;
		/** What the overrides were checked against; bValid false when nothing was available. */
		FIRParameterSchema ParentSchema;
	};

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
		/** Material products: `#pragma material(Substrate = ...)`. */
		EIRSubstrateMode SubstrateMode = EIRSubstrateMode::Legacy;
		/** `/// @library Cat|Sub`: exposes the function to the material function library. Functions only. */
		FString LibraryPath;
		/** `/// @desc`. */
		FString Description;
		/** The bound function this product came from (FBoundModule::Functions index). */
		int32 BoundFunctionIndex = INDEX_NONE;
		FIRSourceRef Source;
		FIRGraph Graph;
		/** See FBoundProduct::bLegacyAssetPath; Name then holds the 1.x Name= (folders allowed). */
		bool bLegacyAssetPath = false;
		/** See FBoundProduct::AssetRoot: the 1.x Root= spelling; empty is the 1.x default root. */
		FString AssetRoot;
		/** MaterialInstance only. */
		FIRInstance Instance;
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
