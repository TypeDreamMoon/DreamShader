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
//
// FROZEN for batch 1 (M2+M3).

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
	 * Anything else is passed through by its key to the parameter or function node (plan §5).
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
		inline const TCHAR* const Static = TEXT("static");       // uniform bool -> static switch
		inline const TCHAR* const Library = TEXT("library");     // "Cat|Sub"
		inline const TCHAR* const Param = TEXT("param");         // "<name> <text>"
		inline const TCHAR* const Asset = TEXT("asset");         // extern binding
		inline const TCHAR* const Custom = TEXT("custom");       // "" | "selfcontained"
		inline const TCHAR* const Layer = TEXT("layer");
		inline const TCHAR* const LayerBlend = TEXT("layerblend");
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

		const FString* FindParamDoc(const FString& ParamName) const;
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
		/** A call to a Helper / Custom / ExportFunction / Extern function: Index into Functions; Args map arguments to parameters. */
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
	};
	DREAMSHADERLANG_API const TCHAR* LexToString(EBoundExprKind Kind);

	/** How one call argument was matched. */
	struct FBoundArgument
	{
		/** Index into FCallExpr::Arguments. */
		int32 ArgumentIndex = INDEX_NONE;
		/** ReflectedCall: the pin or property name; FunctionCall: the parameter name; TextureSample: "Texture"/"UV"/"Sampler". */
		FString Target;
		/** ReflectedCall: the argument is a literal property (not a pin). */
		bool bIsProperty = false;
		/** ReflectedCall: index into the catalog entry's Inputs or Properties. FunctionCall: the parameter index. */
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
		/** ReflectedCall / FunctionCall / TextureSample / Constructor / StructConstructor: argument bindings, in argument order. */
		TArray<FBoundArgument> Args;
		/** Whether this expression may be assigned to. */
		bool bLValue = false;
		/** A constant the binder could evaluate (literals, `static const` reads, arithmetic on them). */
		bool bIsConstant = false;
		double ConstantValue[4] = { 0.0, 0.0, 0.0, 0.0 };
		/** Conversion applied when this expression is used where its parent expects Type's counterpart (set on operands by the parent). */
		IR::EIRConversion Conversion = IR::EIRConversion::Identity;
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
	 * The symbol index for the language service (plan §13.4): declarations with kind, name, span,
	 * signature and doc; every reference as name -> spans; resolved include paths; the parameter
	 * schema. JSON, schema "dreamshader-symbol-index" version 1. Works on a bound module with
	 * errors -- an editor wants navigation on a broken file most of all.
	 */
	DREAMSHADERLANG_API FString BuildDreamShaderSymbolIndexJson(const FBoundModule& Bound);
}
