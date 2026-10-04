// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The bound module: the AST plus everything the binder worked out about it.
//
// The binder does not build a second tree. It keeps the AST -- whose nodes are stable in memory,
// they live in TUniquePtrs the module owns -- and records what it learned in side tables keyed by
// node address: the type of every expression, what every identifier refers to, how every call's
// arguments map onto pins, properties or parameters, which function is the material entry and what
// each exported function becomes. The IR builder walks the AST again and reads those tables; it
// never has to resolve a name.
//
// Every question a later stage could ask about a name, a type or a directive is answered here.
// If the IR builder or the emitter finds itself looking a name up, the binder has a gap.

#pragma once

#include "CoreMinimal.h"
#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangParser.h"

namespace UE::DreamShader::Lang
{
	/**
	 * The `///` directives that carry meaning, canonical lower-case (the parser lower-cases keys).
	 * Anything else is passed through by its key to the parameter or function node.
	 */
	namespace Directive
	{
		inline const TCHAR* const Group = TEXT("group");
		inline const TCHAR* const Desc = TEXT("desc");
		inline const TCHAR* const Slider = TEXT("slider");       // "min max"
		inline const TCHAR* const Sort = TEXT("sort");
		inline const TCHAR* const Name = TEXT("name");           // parameter name, or asset path for a function
		inline const TCHAR* const Sampler = TEXT("sampler");     // "Color" | "Normal" | "LinearColor" | ...
		inline const TCHAR* const Default = TEXT("default");     // texture default asset path
		inline const TCHAR* const Static = TEXT("static");       // uniform bool -> static switch; on a function, "<ParamName>": that bool parameter is a StaticBool pin
		inline const TCHAR* const Library = TEXT("library");     // "Cat|Sub"
		inline const TCHAR* const Param = TEXT("param");         // "<name> <text>"
		inline const TCHAR* const Asset = TEXT("asset");         // extern binding
		inline const TCHAR* const Custom = TEXT("custom");       // "" | "selfcontained"
		inline const TCHAR* const Layer = TEXT("layer");
		inline const TCHAR* const LayerBlend = TEXT("layerblend");
		inline const TCHAR* const Page = TEXT("page");           // `.dsi` Font override: the font page
		inline const TCHAR* const Pin = TEXT("pin");             // "<ParamName> <engine pin name...>"
		inline const TCHAR* const Root = TEXT("root");           // 1.x Root= spelling: Game | Engine | Plugin.Name | Plugins/Name | /Mount
	}

	/** Every directive on one declaration, parsed. Unknown keys sit in Passthrough. */
	struct DREAMSHADERLANG_API FBoundDirectives
	{
		FString Group;
		FString Desc;
		FString Name;
		FString Sampler;
		FString DefaultAsset;
		FString Library;
		FString Asset;
		bool bHasSlider = false;
		double SliderMin = 0.0;
		double SliderMax = 1.0;
		bool bHasSort = false;
		int32 Sort = 0;
		bool bStatic = false;
		bool bCustom = false;
		bool bSelfContained = false;
		bool bLayer = false;
		bool bLayerBlend = false;
		/** `@param <name> <text>`, in order. */
		TArray<TPair<FString, FString>> ParamDocs;
		/** Directives the language does not define, key -> value, for reflected pass-through. */
		TMap<FString, FString> Passthrough;
		/** The free text of the block, joined with newlines; what Desc falls back to. */
		FString FreeText;
		/** `@pin <Param> <Engine Name>`, in order: a function pin whose engine name is not an identifier. */
		TArray<TPair<FString, FString>> PinNames;
		/** `@static <Param>` on a function, in order: the bool parameters that are StaticBool pins (the 1.x `StaticBool` input). */
		TArray<FString> StaticParams;
		/** `@root`: the 1.x Root= spelling of a legacy product's destination (FBoundProduct::AssetRoot). */
		FString Root;
		bool bHasRoot = false;

		const FString* FindParamDoc(const FString& ParamName) const;
		/** The `@pin` engine name given for a parameter, or null. */
		const FString* FindPinName(const FString& ParamName) const;
	};

	// ------------------------------------------------------------------------ declarations

	struct FBoundStructField
	{
		FString Name;
		IR::FIRType Type;
		/** 0 for a scalar field; the element count for `float Weights[4]`. Unsized is an error. */
		int32 ArrayCount = 0;
	};

	struct DREAMSHADERLANG_API FBoundStruct
	{
		FString Name;
		TArray<FBoundStructField> Fields;
		const FStructDecl* Decl = nullptr;
		/** The file it was declared in: the module's own path, or an included header's. */
		FString File;
		int32 FindField(const FString& Name) const;
	};

	struct FBoundGlobal
	{
		FString Name;
		IR::FIRType Type;
		EStorageClass Storage = EStorageClass::None;
		const FVariableDecl* Decl = nullptr;
		FBoundDirectives Directives;
		/** 0 for a scalar global; the element count for `static const float Weights[4]`. */
		int32 ArrayCount = 0;
		/** `uniform`: becomes a parameter node. */
		bool bIsParameter = false;
		/** `static const` (or `const`): becomes a Constant node, initializer folded. */
		bool bIsConstant = false;
		/** From which file it came, for messages; the module's own path for its own declarations. */
		FString File;
	};

	struct FBoundParam
	{
		FString Name;
		IR::FIRType Type;
		EParamDirection Direction = EParamDirection::In;
		/** The default expression, or null. Bound like any expression. */
		const FExpr* Default = nullptr;
		/** For exported functions: a default makes the input optional. */
		bool bOptional = false;
		/** `@param` text, if any. */
		FString Doc;
		int32 ArrayCount = 0;
		/** `@pin`: the engine's pin name when it is not an identifier; empty = Name. */
		FString PinName;
		/**
		 * `@static <Name>` on the function: a `bool` input that is a FunctionInput_StaticBool pin, whose value is known
		 * when the material compiles. An `if` on it is a StaticSwitch, and its default is a StaticBool node on the
		 * input's Preview pin, because the engine reads no PreviewValue for such a pin.
		 */
		bool bStatic = false;
	};

	enum class EBoundFunctionKind : uint8
	{
		/** No linkage, no @custom: inlined at every call. */
		Helper,
		/** `/// @custom`: becomes a Custom node at every call (or one shared node when arguments are equal). */
		Custom,
		/** `export void M(inout material m)`: the material entry; one per file at most. */
		Entry,
		/** `/// @layer export void L(inout material m)`. */
		Layer,
		/** `/// @layerblend export void B(material A, material B, ..., inout material R)`. */
		LayerBlend,
		/** Any other `export`: a material function asset; callers use a MaterialFunctionCall. */
		ExportFunction,
		/** `extern` prototype with `/// @asset`: a MaterialFunctionCall to an existing asset. */
		Extern,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EBoundFunctionKind Kind);

	/** A local variable (or a `for` init variable) of one function. */
	struct FBoundLocal
	{
		FString Name;
		IR::FIRType Type;
		/** The declarator that introduced it; null for a parameter shadow. */
		const FDeclarator* Decl = nullptr;
		int32 ArrayCount = 0;
		FLangSpan Span;
		/**
		 * Substrate sugar S5: the catalog entry of the node this local builds, when it was declared as a call without
		 * arguments (`Substrate S = Substrate.Slab();`); INDEX_NONE for every other local. Until a value is taken from it,
		 * `S.Pin = x` connects the node's pins.
		 */
		int32 SubstrateBuilderClass = INDEX_NONE;
	};

	struct FBoundFunction
	{
		FString Name;
		EBoundFunctionKind Kind = EBoundFunctionKind::Helper;
		EFunctionLinkage Linkage = EFunctionLinkage::Internal;
		IR::FIRType ReturnType;
		TArray<FBoundParam> Params;
		const FFunctionDecl* Decl = nullptr;
		FBoundDirectives Directives;
		/** Entry/Layer/LayerBlend: the index of the `inout material` parameter that is the result. */
		int32 MaterialResultParam = INDEX_NONE;
		/** Locals declared anywhere in the body, in declaration order; the slot is the index. */
		TArray<FBoundLocal> Locals;
		FString File;
		/** Helper: it calls itself, directly or through others; inlining is refused (DSH6xxx). */
		bool bRecursive = false;
	};

	struct FBoundProduct
	{
		IR::EIRProductKind Kind = IR::EIRProductKind::Material;
		int32 FunctionIndex = INDEX_NONE;
		FString AssetName;
		FString AssetPathOverride;
		TMap<FString, FString> Settings;
		IR::EIRBackend Backend = IR::EIRBackend::Graph;
		/** `#pragma material(Substrate = Legacy | Bridge | Native)` (Substrate sugar S4). */
		IR::EIRSubstrateMode SubstrateMode = IR::EIRSubstrateMode::Legacy;
		/** 1.x destination: AssetName may carry folders, AssetRoot is the Root= spelling (empty: the 1.x default root), and the source folder is not mirrored. */
		bool bLegacyAssetPath = false;
		FString AssetRoot;
	};

	// ------------------------------------------------------------------------- expressions

	enum class EBoundExprKind : uint8
	{
		/** Typed Error; already reported. */
		Error,
		Literal,
		/** An identifier naming a local: LocalSlot. */
		Local,
		/** An identifier naming a global: Index into Globals. */
		Global,
		/** An identifier naming a parameter of the enclosing function: Index into its Params. */
		Param,
		/** `s.field` on a struct value: FieldIndex. */
		StructField,
		/** `m.BaseColor` on a material value: Index into the catalog's MaterialAttributes. */
		MaterialField,
		/** `v.xyz`: Swizzle holds the canonical mask. */
		Swizzle,
		/** `node.OutputName` on a Node-typed value: FieldIndex is the output index. */
		NodeOutput,
		/** A core op: CoreOp; operands are the AST operands / call arguments in order. */
		CoreOp,
		/** `float3(a, b, c)` / `float3(x)`: Type is the result; arguments in order. */
		Constructor,
		/** `(float3)x`: Type is the target. */
		Cast,
		/** `UE.X(...)` / `Substrate.X(...)` / `UE.Expression(Class = ...)`: Index into the catalog; Args map arguments. */
		ReflectedCall,
		/**
		 * A call to a Helper / Custom / ExportFunction / Extern function: Index into Functions; Args map arguments to parameters.
		 * In a legacy scope a value call of a void function is typed as its first output, which the IR builder reads (L3b).
		 */
		FunctionCall,
		/** `Tex.Sample(UV)` / `Tex.Sample(S, UV)` / `Texture2DSample(Tex, S, UV)`: arguments normalised in Args as Texture, UV, [Sampler]. */
		TextureSample,
		/** `v[const]`: Swizzle holds the single component. */
		IndexConst,
		/** `c ? a : b`. */
		Conditional,
		/** `x = e`, `x += e`...: Target is an lvalue of kind Local/Param/StructField/MaterialField/Swizzle-of-lvalue. */
		Assign,
		/** `{ a, b }` as an initializer: Type is the target. */
		InitializerList,
		/** `(e)`: transparent; Type is the inner type. */
		Paren,
		/** A constructor of a user struct: `ToonInputs(a, b)` or `{a, b}` against a struct target. */
		StructConstructor,
		/**
		 * Legacy only: `F(args).Out` / `F(args)[k]` where F is an Extern / ExportFunction / Custom function.
		 * The Object call is bound in selection mode (its out arguments may be absent and are not written
		 * back); FieldIndex is the 1.x output ordinal: 0 is the return value when F is not void, then the out
		 * parameters in declaration order.
		 */
		FunctionCallOutput,
		/**
		 * Substrate sugar S5: `S.Roughness` where S is a builder local (`Substrate S = Substrate.Slab();`). LocalSlot is the
		 * local, Index the node's catalog entry, BuilderPin the pin -- or the virtual argument of sugar S3 -- and FieldIndex
		 * the pin's index (INDEX_NONE for a virtual one). As the target of `=` it connects the pin; as a value it is whatever
		 * was connected, and makes no node.
		 */
		SubstrateBuilderPin,
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EBoundExprKind Kind);

	/** How one call argument was matched. */
	struct FBoundArgument
	{
		/**
		 * Index into FCallExpr::Arguments. On a ReflectedCall that an OPERATOR bound (Substrate sugar S1: `A + B`, `A * w`)
		 * it counts the operands of the FBinaryExpr instead: 0 is Left, 1 is Right.
		 */
		int32 ArgumentIndex = INDEX_NONE;
		/** ReflectedCall: the pin or property name; FunctionCall: the parameter name; TextureSample: "Texture"/"UV"/"Sampler". */
		FString Target;
		/**
		 * ReflectedCall, a pin: what the source called it, when that is not the member's own name. The catalog is read off each
		 * class's default object, and a node may show other names on its pins once its properties are set (the engine
		 * fork's MoonToon nodes do), so an alias taken from a default object's display name can belong to another pin of the
		 * live node. The emitter looks this spelling up on the live node first, which is the order 1.x matched in.
		 */
		FString WrittenTarget;
		/** ReflectedCall: the argument is a literal property (not a pin). */
		bool bIsProperty = false;
		/**
		 * ReflectedCall, Substrate sugar S3: the argument is no pin and no property of the node. `Substrate.Slab(BaseColor =
		 * ..., Metallic = ...)` names inputs of a conversion node, which the IR builder makes and whose outputs it wires to
		 * the node's real pins (DiffuseAlbedo, F0). Target is the virtual name; TargetIndex stays INDEX_NONE.
		 */
		bool bIsVirtual = false;
		/**
		 * ReflectedCall: index into the catalog entry's Inputs or Properties; INDEX_NONE with bIsProperty == false is an input
		 * a Custom class's call named (L4). FunctionCall: the parameter index; Params.Num() marks a legacy statement call's
		 * return-value receiver (L5).
		 */
		int32 TargetIndex = INDEX_NONE;
		/** The conversion applied to the argument's value. */
		IR::EIRConversion Conversion = IR::EIRConversion::Identity;
	};

	struct FBoundExpr
	{
		EBoundExprKind Kind = EBoundExprKind::Error;
		IR::FIRType Type;
		/** Global / Param / ReflectedCall / FunctionCall: the index the Kind says. */
		int32 Index = INDEX_NONE;
		/** Local: slot in the enclosing FBoundFunction::Locals. */
		int32 LocalSlot = INDEX_NONE;
		/** StructField / MaterialField / NodeOutput: which field / attribute / output. */
		int32 FieldIndex = INDEX_NONE;
		/** CoreOp: which op. */
		IR::EIROp CoreOp = IR::EIROp::Count;
		/** Swizzle / IndexConst: canonical lower-case xyzw mask. */
		FString Swizzle;
		/** SubstrateBuilderPin: the pin's own name, or the virtual argument's. */
		FString BuilderPin;
		/** ReflectedCall / FunctionCall / TextureSample / Constructor / StructConstructor: argument bindings, in argument order. */
		TArray<FBoundArgument> Args;
		/** Whether this expression may be assigned to. */
		bool bLValue = false;
		/** A constant the binder could evaluate (literals, `static const` reads, arithmetic on them). */
		bool bIsConstant = false;
		double ConstantValue[4] = { 0.0, 0.0, 0.0, 0.0 };
		/** Conversion applied when this expression is used where its parent expects Type's counterpart (set on operands by the parent). */
		IR::EIRConversion Conversion = IR::EIRConversion::Identity;
		/**
		 * Legacy rule L3c: a node with several outputs read as its first one because a 1.x source used it as a value, where
		 * 2.0 asks for the output's name (DSH5201). Conversion is DefaultOutput; the migrator writes the name in.
		 */
		bool bLegacyDefaultOutput = false;
		/**
		 * Legacy rule L22: the width this expression is cut down to where it is used. 1.x took the leading components of a
		 * value wider than its place without a word (Conversion is Truncate) -- except at a node's pin, where it connected
		 * the value as it was and left the rest to the engine (Conversion is Identity). The migrator writes the swizzle
		 * either way.
		 */
		int32 LegacyTruncateWidth = 0;
		/**
		 * Legacy rule L27: the left side of a `/` between integers that 1.x divided as floats, because it typed every
		 * number literal float (`7 / 2` is 3.5). The CoreOp is a float division; the migrator writes `float(7) / 2`.
		 */
		bool bLegacyFloatDivide = false;
		/**
		 * ReflectedCall in a 1.x body: Type is what the call's `OutputType` said and not what the catalog says, which is how
		 * 1.x typed it. The node carries that width too, so that nothing downstream corrects what 1.x never corrected.
		 */
		bool bLegacyDeclaredType = false;
		/** With bLegacyDeclaredType: what the catalog says the call makes, which is what a `.dss` has to declare. */
		IR::FIRType LegacyCatalogType = IR::FIRType::Error();
		/**
		 * ReflectedCall on the Custom class (`UE.Expression(Class = "Custom", ...)`): the outputs THIS call declares, which
		 * the class cannot say -- output 0 typed by its `OutputType` argument (named `return` once there are more), then
		 * every entry of `AdditionalOutputs` in order. Empty for every other call. A NodeOutput selected from such a call
		 * keeps the slot in FieldIndex like any other.
		 */
		TArray<FString> CallOutputNames;
		TArray<IR::FIRType> CallOutputTypes;
	};

	// ------------------------------------------------------------------------ instances (.dsi)

	/** One `uniform` of a `.dsi`, bound: the parent parameter it assigns. */
	struct FBoundInstanceOverride
	{
		/** Index into FBoundModule::Globals. */
		int32 GlobalIndex = INDEX_NONE;
		FString ParameterName;
		IR::EIRParameterKind Kind = IR::EIRParameterKind::Scalar;
		/** Index into the schema bound against; INDEX_NONE without one. */
		int32 SchemaIndex = INDEX_NONE;
		int32 FontPage = 0;
	};

	/** The `#pragma instance(...)` header of a `.dsi` and its overrides, bound. */
	struct FBoundInstance
	{
		bool bIsInstance = false;
		const FPragmaDecl* Pragma = nullptr;
		FString ParentReference;
		FLangSpan ParentSpan;
		/** Every key but Parent, in source order, values as written. */
		TArray<TPair<FString, FString>> Settings;
		TArray<FLangSpan> SettingSpans;
		TArray<FBoundInstanceOverride> Overrides;
	};

	// ------------------------------------------------------------------------ pipelines (.dsp)

	struct FPipelineReferences;

	/**
	 * A `.dsp`, bound: the payload of its one PassPipeline product, canonical and with every default applied, plus where
	 * each part of it was written. The IR builder copies Payload into FIRProduct::PassPipeline (stamping its files);
	 * the decompiler's re-parse check and Adopt read the declarations back through the parallel arrays.
	 */
	struct DREAMSHADERLANG_API FBoundPipeline
	{
		bool bIsPipeline = false;
		/** The `#pragma pipeline` line; null when the file has none (every pipeline key at its default). */
		const FPragmaDecl* Pragma = nullptr;
		/** What the PassPipeline product carries (IR.h). Built even when the file has errors, as far as it got. */
		IR::FIRPassPipeline Payload;
		/** Parallel to Payload.Parameters: the FBoundModule::Globals entry each came from. */
		TArray<int32> ParameterGlobals;
		/** Parallel to Payload.Buffers and Payload.Passes: the declaration each came from. */
		TArray<const FBufferDecl*> BufferDecls;
		TArray<const FPassDecl*> PassDecls;
		/** What the engine-dependent checks were made against; null in an engine-free check. Owned by the caller. */
		const FPipelineReferences* References = nullptr;

		int32 FindBuffer(const FString& Name) const;
		int32 FindPass(const FString& Name) const;
		int32 FindParameter(const FString& Name) const;
	};

	// ------------------------------------------------------------------------------ module

	struct DREAMSHADERLANG_API FBoundModule
	{
		/** The module that was bound; owned by the caller and must outlive this. */
		const FModule* Module = nullptr;
		/**
		 * The catalog the module was bound against: every catalog index recorded below (reflected
		 * calls, material attributes, Node types) is an index into THIS catalog, so the IR builder
		 * and the validator read it from here. Owned by the caller; must outlive this.
		 */
		const IR::FBuiltinCatalog* Catalog = nullptr;
		/** Modules pulled in through `#include` / `import`, in first-seen order; owned by the include resolver. */
		TArray<const FModule*> Included;
		TArray<FString> IncludePaths;

		TArray<FBoundStruct> Structs;
		TArray<FBoundGlobal> Globals;
		TArray<FBoundFunction> Functions;
		TArray<FBoundProduct> Products;

		/** `#pragma material(...)`, merged across lines; Backend removed into the product. */
		TMap<FString, FString> MaterialSettings;
		/** `#pragma region` / `#pragma endregion` at file scope and inside bodies, as a tree. */
		TArray<IR::FIRRegion> Regions;
		TArray<IR::FIRLayoutHint> LayoutHints;

		/** Every expression node in every function body, initializer and default. */
		TMap<const FNode*, FBoundExpr> Expressions;
		/** Every statement -> the region it sits in (INDEX_NONE when none). */
		TMap<const FNode*, int32> StatementRegions;
		/** Every `for`/`while`/`do` statement the binder proved bounded: the trip count. Absent means "not unrollable". */
		TMap<const FNode*, int32> LoopTripCounts;
		/** `.dsi` only. */
		FBoundInstance Instance;
		/** `.dsi` only: the schema bound against; owned by the caller, must outlive this. Null when unchecked. */
		const IR::FIRParameterSchema* ParentSchema = nullptr;
		/** `.dsp` only. */
		FBoundPipeline Pipeline;

		const FBoundExpr* Find(const FExpr& Expr) const { return Expressions.Find(&Expr); }
		const FBoundExpr& Get(const FExpr& Expr) const;
		int32 FindStruct(const FString& Name) const;
		int32 FindGlobal(const FString& Name) const;
		int32 FindFunction(const FString& Name) const;
		/** The single Entry function, or INDEX_NONE for a function library. */
		int32 FindEntryFunction() const;
	};

	/**
	 * Supplies the parsed, preprocessed module for an include path. The binder calls it once per
	 * distinct path; the resolver owns the returned module for at least as long as the bound
	 * module lives. Returns null after reporting into the sink when the file cannot be read or
	 * parsed. FromFile is the including file, for relative resolution and for messages.
	 */
	using FLangIncludeResolver = TFunction<const FModule*(const FString& IncludePath, const FString& FromFile, FLangDiagnosticSink& Diagnostics)>;

	/**
	 * What the host found for one material a `.dsp` names (`Material = "..."`): the engine facts the binder checks a
	 * pass against (V6, V7 in DreamShader_Plan/05). Plain data, so the binder stays engine-free.
	 */
	struct FPipelineMaterialInfo
	{
		/** The reference as written. */
		FString Reference;
		/** Empty when nothing was found. */
		FString ObjectPath;
		bool bFound = false;
		/** MaterialDomain without its prefix: Surface, PostProcess, ... */
		FString Domain;
		/** Post-process materials: BlendableLocation without its prefix (SceneColorAfterDOF, SceneColorAfterTonemapping, ...). */
		FString BlendableLocation;
		bool bDisablePreExposureScale = false;
		/**
		 * The UserSceneTexture names the material reads, in the order its shader map lists them, as a pass binds them: an
		 * instance's UserSceneTextureOverrides applied, which is the name the runtime matches a `read` against.
		 */
		TArray<FString> UserSceneTextureInputs;
		/** Bit i: a SceneTexture node reads PostProcessInput i (that input slot is taken). */
		uint32 PostProcessInputsUsed = 0;
		/** The material has a UE.DreamPassOutput node at the top level of its graph. */
		bool bHasPassOutput = false;
		/** Bit i: Output<i> of that node is connected. */
		uint32 PassOutputsConnected = 0;
		/** The mesh usage flags the material has, as EDreamPassMeshUsageFlags spellings (StaticMesh, SkeletalMesh, ...). */
		TArray<FString> Usages;
		/** The material asks for the new material translator, which the pass nodes do not support. */
		bool bUsesNewTranslator = false;
	};

	/**
	 * Map key functions for an FString key compared as HLSL compares names: case-sensitively. TMap's own for FString ignore
	 * case, which would take `BlurCS` and `blurCS`, two functions of one shader file, for one.
	 */
	template <typename ValueType>
	struct TCaseSensitiveStringMapKeyFuncs : BaseKeyFuncs<TPair<FString, ValueType>, FString, /*bInAllowDuplicateKeys*/ false>
	{
		static FORCEINLINE const FString& GetSetKey(const TPair<FString, ValueType>& Element) { return Element.Key; }
		static FORCEINLINE bool Matches(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
		static FORCEINLINE uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
	};

	/** A shader file's compute entries -- `[numthreads(x, y, z)]` functions -- by name, case-sensitively, with that group size. */
	using FPipelineComputeEntryMap = TMap<FString, FIntVector, FDefaultSetAllocator, TCaseSensitiveStringMapKeyFuncs<FIntVector>>;

	/** What the host found for one `.usf` a `.dsp` names (`Shader = "..."`). */
	struct FPipelineShaderInfo
	{
		/** The reference as written. */
		FString Reference;
		/**
		 * The virtual shader path (`/Project/Passes/Blur.usf`) when the host has one -- the reference itself when it starts
		 * with `/`; empty is no error. The file may live anywhere: the engine compiles a snapshot of it under the mapped
		 * /DreamPassUser directory, never the original. A reference without a leading `/` is relative to the `.dsp`'s folder.
		 */
		FString VirtualPath;
		/** The file on disk. */
		FString FilePath;
		bool bExists = false;
		/**
		 * Every function the text defines with `[numthreads(x, y, z)]` in front of it, with that group size -- the text being
		 * the file and every file it includes that the host could follow, by a relative path or a mapped virtual one. Keyed
		 * case-sensitively, as HLSL names are.
		 */
		FPipelineComputeEntryMap ComputeEntries;
		/** Every other function the text defines at file scope, each spelling once (case-sensitively). */
		TArray<FString> Functions;
		/**
		 * False when the file includes something the host could not read -- a virtual path no mapped directory covers, a
		 * relative path that names no file, `#include MACRO`: an Entry missing from ComputeEntries and Functions may be
		 * defined there, so the binder does not call it missing.
		 */
		bool bEntryScanComplete = true;
	};

	/** Everything the host resolved for a `.dsp` before binding it. */
	struct FPipelineReferences
	{
		TArray<FPipelineMaterialInfo> Materials;
		TArray<FPipelineShaderInfo> Shaders;
		/**
		 * The project's pass layers (UDreamPassSettings::LayerNames), in bit order: the first UDreamPassSettings::MaxLayers
		 * names only, the ones that have a bit -- a name past them is no layer, to the binder as to the runtime.
		 */
		TArray<FString> LayerNames;
		/**
		 * False on an engine older than 5.8: a `.dsp` still parses and binds, and the emitter refuses it. The host then reads
		 * a material's domain and blendable location only, and the binder skips the checks that need what only the Custom
		 * Pass runtime gives (UserSceneTexture inputs, UE.DreamPassOutput pins, usage flags, pre-exposure, translator).
		 */
		bool bCustomPassAvailable = true;

		/** By the reference exactly as written: the collector keeps two spellings apart (CollectDreamShaderPipelineReferences). */
		const FPipelineMaterialInfo* FindMaterial(const FString& Reference) const
		{
			return Materials.FindByPredicate([&Reference](const FPipelineMaterialInfo& Info) { return Info.Reference.Equals(Reference, ESearchCase::CaseSensitive); });
		}

		const FPipelineShaderInfo* FindShader(const FString& Reference) const
		{
			return Shaders.FindByPredicate([&Reference](const FPipelineShaderInfo& Info) { return Info.Reference.Equals(Reference, ESearchCase::CaseSensitive); });
		}
	};

	struct FBindOptions
	{
		/** Required. An empty catalog binds nothing that names a builtin. */
		const IR::FBuiltinCatalog* Catalog = nullptr;
		/** Optional; without it every `#include` is DSH4xxx "includes unavailable". */
		FLangIncludeResolver IncludeResolver;
		/** The project's default backend when `#pragma material` does not say. */
		IR::EIRBackend DefaultBackend = IR::EIRBackend::Graph;
		/** Loops with a constant trip count up to this are unrolled; longer ones are refused with a pointer at @custom. */
		int32 MaxUnrolledIterations = 64;
		/** Helper inlining depth before the binder calls it recursion. */
		int32 MaxInlineDepth = 32;
		/** `.dsi` only: the parent's parameters, resolved by the host. Null = names and types unchecked (DSH7263). */
		const IR::FIRParameterSchema* ParentSchema = nullptr;
		/** `.dsi` only: the resolved parent object path, carried into FIRInstance::ParentObjectPath. */
		FString ParentObjectPath;
		/**
		 * `.dsp` only: the materials, shader files and layers the host resolved. Null = an engine-free check: the
		 * references are taken as written and every check that needs an engine fact is skipped.
		 */
		const FPipelineReferences* PipelineReferences = nullptr;
	};

	struct FLangBindResult
	{
		TUniquePtr<FBoundModule> Bound;
		FLangDiagnosticSink Diagnostics;
		bool Succeeded() const { return Bound.IsValid() && !Diagnostics.HasErrors(); }
	};

	/**
	 * Binds a parsed module: resolves every name, types every expression, matches every call,
	 * decides the products. The bound module keeps pointers into Module and into whatever the
	 * include resolver returned; both must outlive it.
	 *
	 * Like the parser, it keeps going after an error where it can, so one bad expression does not
	 * hide the next; Succeeded() is the question to ask, not whether the sink is empty.
	 */
	DREAMSHADERLANG_API FLangBindResult BindDreamShaderLang(const FModule& Module, const FBindOptions& Options);

	/**
	 * The symbol index for the language service: declarations with kind, name, span,
	 * signature and doc; every reference as name -> spans; resolved include paths; the parameter
	 * schema. JSON, schema "dreamshader-symbol-index" version 1. Works on a bound module with
	 * errors -- an editor wants navigation on a broken file most of all.
	 */
	DREAMSHADERLANG_API FString BuildDreamShaderSymbolIndexJson(const FBoundModule& Bound);
}
