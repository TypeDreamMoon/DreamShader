// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// BuildDreamShaderAstFromIR, shared between its translation units:
//
//   IRToAst.cpp             the module: uniforms, callables, product functions, pragmas, layout
//   IRToAstNames.cpp        identifiers, name scopes, the small AST factories, expression cloning
//   IRToAstExpressions.cpp  one graph: what gets a name, and the expression of every value
//   IRToAstStatements.cpp   one graph: statements, regions, their order, the body
//   IRCustomRecover.cpp     a Custom node's Code read back into `/// @custom` functions
//
// The direction of every rule here is "print what the IR builder would turn back into this graph": the comments
// name the forward rule they invert (IRBuilder*.cpp, Compiler: Emitter/DreamShaderIREmitter*.cpp).
//
// Diagnostics: DSH9075-9084.

#pragma once

#include "CoreMinimal.h"
#include "Decompile/IRToAst.h"
#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Templates/UniquePtr.h"

namespace UE::DreamShader::Lang::DecompileAst
{
	using IR::EIROp;
	using IR::FIRGraph;
	using IR::FIRInput;
	using IR::FIRNode;
	using IR::FIRProduct;
	using IR::FIRProperty;
	using IR::FIRPropertyValue;
	using IR::FIRType;
	using IR::FIRValue;

	// ------------------------------------------------------------------------------------ names

	/** `[A-Za-z_][A-Za-z0-9_]*`. */
	bool IsIdentifierText(const FString& Text);
	/** A word the language reads as something else: a keyword, a builtin type, `UE`, `Substrate`, a core intrinsic. */
	bool IsReservedIdentifier(const FString& Text);
	/** Text as a legal, non-reserved identifier; Fallback when nothing of Text survives. Not yet unique. */
	FString MakeSourceIdentifier(const FString& Text, const TCHAR* Fallback);

	/**
	 * The names taken at one level: the file's, or one function's on top of the file's. Compared ignoring case,
	 * which the language does not need and everything downstream that holds names as FName does.
	 */
	class FNameScope
	{
	public:
		explicit FNameScope(const FNameScope* InParent = nullptr)
			: Parent(InParent)
		{
		}

		bool IsTaken(const FString& Name) const;
		void Reserve(const FString& Name);
		/** Wanted, or Wanted_2, Wanted_3... -- the first one free here and above; it is reserved on the way out. */
		FString Claim(const FString& Wanted);

	private:
		const FNameScope* Parent = nullptr;
		TSet<FString> Taken;
	};

	// ------------------------------------------------------------------------------ AST factories

	FExprPtr MakeIdentifierExpr(const FString& Name);
	/** A float literal; a negative one is a Negate over the positive literal, which is how it reads back. */
	FExprPtr MakeNumberExpr(double Value);
	FExprPtr MakeIntegerExpr(int64 Value);
	FExprPtr MakeBoolExpr(bool bValue);
	FExprPtr MakeStringExpr(const FString& Value);
	FExprPtr MakeMemberExpr(FExprPtr Object, const FString& Member);
	FExprPtr MakeIndexExpr(FExprPtr Object, int32 Index);
	FExprPtr MakeUnaryExpr(EUnaryOp Op, FExprPtr Operand);
	FExprPtr MakeBinaryExpr(EBinaryOp Op, FExprPtr Left, FExprPtr Right);
	FExprPtr MakeConditionalExpr(FExprPtr Condition, FExprPtr TrueValue, FExprPtr FalseValue);
	FExprPtr MakeCastExpr(const FTypeRef& Type, FExprPtr Operand);
	FExprPtr MakeAssignExpr(FExprPtr Target, FExprPtr Value);
	TUniquePtr<FCallExpr> MakeCallExpr(FExprPtr Callee);
	/** `UE.Name` / `Substrate.Name`. */
	FExprPtr MakeNamespaceCallee(const FString& Namespace, const FString& Name);
	void AddPositionalArgument(FCallExpr& Call, FExprPtr Value);
	void AddNamedArgument(FCallExpr& Call, const FString& Name, FExprPtr Value);
	/** `Pin[Index] = Value`: an input pin whose name is not an identifier, by the catalog's index. */
	void AddPinArgument(FCallExpr& Call, int32 PinIndex, FExprPtr Value);
	/** `float3(a, b, c)`. */
	FExprPtr MakeConstructorExpr(const FTypeRef& Type, TArray<FExprPtr>&& Parts);
	/** One component as a literal, several as the constructor of that width. */
	FExprPtr MakeVectorLiteralExpr(const double* Values, int32 Count, bool bBool);
	/** A deep copy: a value restated at a second use. */
	FExprPtr CloneExpr(const FExpr& Expr);
	/** Binary operators nested below Expr, for the "too deep to read" rule. */
	int32 MeasureBinaryDepth(const FExpr& Expr);

	FTypeRef MakeTypeRef(const FString& Spelling);
	/** The spelling a value of Type is declared with; bKeepBool false writes a bool as the float the graph carries. */
	FString SpellValueType(const FIRType& Type, bool bKeepBool);
	FTypeRef MakeValueTypeRef(const FIRType& Type, bool bKeepBool);
	/**
	 * How a callable declares a parameter or its return value: the spelling it was recovered with, else the type's own.
	 * A variable passed for an `out` parameter is declared through this too, because the binder wants the two to be
	 * exactly the same type (DSH4218).
	 */
	FTypeRef MakeSignatureTypeRef(const FIRType& Type, const FString& TypeSpelling);

	void AddDocDirective(FDocBlock& Doc, const TCHAR* Key, const FString& Value);
	/** `@desc` for one line; free text lines for several, which is what `@desc` falls back to when it is absent. */
	void AddDocDescription(FDocBlock& Doc, const FString& Description);

	FStmtPtr MakeVarDeclStmt(const FTypeRef& Type, const FString& Name, FExprPtr Initializer);
	FStmtPtr MakeExprStmt(FExprPtr Expression);
	FStmtPtr MakeAssignStmt(FExprPtr Target, FExprPtr Value);
	FStmtPtr MakeReturnStmt(FExprPtr Value);
	FStmtPtr MakeRegionStmt(bool bBegin, const FString& Title);

	// ---------------------------------------------------------------------------- custom recovery

	struct FRecoveredCustomParam
	{
		FString Name;
		FString TypeSpelling;
		EParamDirection Direction = EParamDirection::In;
	};

	/** One `/// @custom` function as a Custom node's Code spelled it. */
	struct FRecoveredCustomFunction
	{
		FString Name;
		/** Embedded helpers carry their whole signature; the node's own function only its body. */
		bool bHasSignature = false;
		FString ReturnTypeSpelling;
		TArray<FRecoveredCustomParam> Params;
		/** The text between the braces, the builder's rewrites undone. */
		FString RawBody;
		/** The node's own body ended in the `return 0.0;` the builder adds to a void function. */
		bool bZeroFallback = false;
		/** Rule L9: the first `out` parameter of a void function, which its node returns; declared ahead of the body. */
		FString PrimaryOutType;
		FString PrimaryOutName;
	};

	struct FRecoveredCustomCode
	{
		FRecoveredCustomFunction Root;
		/** Dependency-first, as the wrapper struct listed them. */
		TArray<FRecoveredCustomFunction> Helpers;
	};

	/**
	 * Reads the Code the IR builder writes (IRCustomHlsl.cpp: the wrapper struct, the Begin/End markers, the
	 * DreamShaderFn_ call rewrites, the blanked `#include`s). False when the text is not that shape -- a node made by
	 * hand -- and the caller keeps the code verbatim as the body.
	 */
	bool RecoverCustomCode(const FString& Code, const TArray<FString>& IncludeFilePaths, FRecoveredCustomCode& Out);

	// --------------------------------------------------------------------------------- callables

	/** One pin of something callable, before it became a parameter. */
	struct FPinModel
	{
		FString Name;
		FIRType Type = FIRType::Float(1);
		/** Custom functions: the HLSL spelling the signature had (`int`, `half3`); empty writes Type. */
		FString TypeSpelling;
		bool bOptional = false;
		bool bHasDefault = false;
		double Default[4] = { 0.0, 0.0, 0.0, 0.0 };
		FString Description;
		/** A FunctionInput_StaticBool pin: a `bool` parameter the function's `/// @static` names. */
		bool bStatic = false;
	};

	struct FCallableParam
	{
		/** The engine pin: a FunctionCall's input or output name, a Custom node's pin. */
		FString PinName;
		/** The parameter as the source spells it. */
		FString Identifier;
		FIRType Type = FIRType::Float(1);
		FString TypeSpelling;
		EParamDirection Direction = EParamDirection::In;
		bool bOptional = false;
		bool bHasDefault = false;
		double Default[4] = { 0.0, 0.0, 0.0, 0.0 };
		FString Description;
		bool bStatic = false;
	};

	enum class ECallableKind : uint8
	{
		/** `extern` + `/// @asset`: a function asset outside this module. */
		Extern,
		/** An exported function of this module, called through Prop::LocalFunction. */
		Product,
		/** A `/// @custom` function recovered from Custom nodes. */
		Custom,
	};

	struct FCallable
	{
		ECallableKind Kind = ECallableKind::Extern;
		FString Identifier;
		/** Extern. */
		FString AssetPath;
		/** Product. */
		int32 ProductIndex = INDEX_NONE;
		FString Description;

		bool bHasReturn = false;
		/** The output pin the return value is. */
		FString ReturnPinName;
		FIRType ReturnType = FIRType::Void();
		FString ReturnTypeSpelling;
		FString ReturnDescription;
		/** Signature order. */
		TArray<FCallableParam> Params;

		/** Custom. */
		FString RawBody;
		/** Custom: what the node is titled, when that is not the function's name (`/// @name`). */
		FString NodeTitle;
		/** Extern: no interface was handed over; the prototype is what the calls show (DSH9081). */
		bool bInferred = false;

		bool HasOutParams() const
		{
			for (const FCallableParam& Param : Params)
			{
				if (Param.Direction != EParamDirection::In)
				{
					return true;
				}
			}
			return false;
		}

		const FCallableParam* FindParamByPin(const FString& PinName, bool bOutput) const
		{
			for (const FCallableParam& Param : Params)
			{
				const bool bIsOutput = Param.Direction != EParamDirection::In;
				const bool bIsInput = Param.Direction != EParamDirection::Out;
				if ((bOutput ? bIsOutput : bIsInput) && Param.PinName.Equals(PinName, ESearchCase::CaseSensitive))
				{
					return &Param;
				}
			}
			return nullptr;
		}
	};

	/**
	 * Inputs and outputs as one parameter list (the inverse of IRBuilderMaterial.cpp BuildFunctionProduct): inputs keep
	 * their order, outputs keep theirs, an output named like an input is that input's `inout`, and the return value is
	 * the single output, or a first output called Result.
	 */
	void BuildCallableSignature(FCallable& Callable, const TArray<FPinModel>& Inputs, const TArray<FPinModel>& Outputs, const FNameScope& FileNames);

	// ---------------------------------------------------------------------------------- uniforms

	struct FUniformModel
	{
		FString ParameterName;
		FString Identifier;
		/** The variable name the asset's hints recorded; preferred over a name made from ParameterName. */
		FString HintName;
		bool bTexture = false;
		bool bStatic = false;
		FIRType TextureType;
		/** 1..4 as declared. A float4 parameter every reader of which takes a leading mask is declared that narrow. */
		int32 Width = 1;
		/** The parameter node's own output width: 1 or 4. */
		int32 NodeWidth = 1;
		double Default[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 DefaultCount = 0;
		FString Group;
		FString Description;
		FString DefaultAsset;
		FString SamplerType;
		bool bHasSlider = false;
		double SliderMin = 0.0;
		double SliderMax = 1.0;
		bool bHasSort = false;
		int64 Sort = 0;
		TArray<TPair<FString, FString>> Passthrough;
		/** Reader analysis, over every graph of the module. */
		bool bReadWhole = false;
		int32 WidestLeadingMask = 0;
		/** First appearance, for a stable order among equal sort priorities. */
		int32 FirstSeen = 0;
		/**
		 * The file-scope `#pragma region` the declaration sits in: the title of the box its parameter node is in, when
		 * that box holds parameter nodes only. A parameter node in a box that holds a body's nodes got there by being
		 * read first inside that region, not by its declaration, and a file-scope region would say something else.
		 */
		FString Region;
	};

	// ------------------------------------------------------------------------------- the writers

	class FGraphWriter;

	/** The file: everything that is declared once however many graphs use it. */
	class FModuleWriter
	{
	public:
		FModuleWriter(const IR::FIRModule& InSource, const IR::FBuiltinCatalog& InCatalog, const FIRToAstOptions& InOptions, FLangDiagnosticSink& InDiagnostics);

		TUniquePtr<FModule> Build();

		// ----- what a graph writer asks
		const IR::FBuiltinCatalog& GetCatalog() const { return Catalog; }
		const FIRToAstOptions& GetOptions() const { return Options; }
		FLangDiagnosticSink& GetDiagnostics() { return Diagnostics; }
		const FNameScope& GetFileNames() const { return FileNames; }
		FModule& GetModule() { return *Module; }

		/** The uniform a Parameter / TextureParameter node is; null when the node carries no ParameterName. */
		const FUniformModel* FindUniform(const FIRNode& Node) const;
		/** The callable a FunctionCall or Custom node calls; null after DSH9078. */
		const FCallable* FindCallable(const FIRNode& Node) const;

		void Info(const TCHAR* Code, const FText& Message);
		void Warning(const TCHAR* Code, const FText& Message);
		void Error(const TCHAR* Code, const FText& Message);

	private:
		// IRToAst.cpp
		void CollectUniforms();
		void CollectProductCallables();
		void CollectExternCallables();
		void CollectCustomCallables();
		void NameUniforms();

		void EmitMaterialPragma();
		void EmitExternPrototypes();
		void EmitUniforms();
		void EmitCustomFunctions();
		void EmitProductFunctions();
		void EmitLayoutPragmas();
		void ApplyHeaderComments();

		void EmitMaterialProduct(int32 ProductIndex);
		void EmitFunctionProduct(int32 ProductIndex);
		/** Description, `@name`, `@library`, `@layer` / `@layerblend`. */
		void AddProductDoc(FDocBlock& Doc, int32 ProductIndex) const;
		/** The `///` lines a callable's signature needs: `@param`, `@pin`. */
		void AddSignatureDirectives(FDocBlock& Doc, const FCallable& Callable) const;
		void FillParams(FFunctionDecl& Decl, const FCallable& Callable) const;
		/** One product's `#pragma layout` hints, under the names its body gave the nodes. */
		void ResolveLayoutHints(int32 ProductIndex, const FGraphWriter& Writer);
		void AddDeclaration(FDeclPtr Decl, bool bBlankLineBefore);

		static FString MakeUniformKey(bool bTexture, const FString& ParameterName);
		static FString NormalizeAssetPath(const FString& Path);
		static FString AssetLeafName(const FString& Path);
		/** The function a Custom node is: its name and its code. */
		static FString MakeCustomNodeKey(const FIRNode& Node);

		const IR::FIRModule& Source;
		const IR::FBuiltinCatalog& Catalog;
		const FIRToAstOptions& Options;
		FLangDiagnosticSink& Diagnostics;

		TUniquePtr<FModule> Module;
		FNameScope FileNames;

		TArray<FUniformModel> Uniforms;
		TMap<FString, int32> UniformByKey;
		/** Declaration order: by sort priority, then first appearance. */
		TArray<int32> UniformOrder;
		TSet<FString> DisagreeingUniforms;

		/** Parallel to the module's products: the function each one is written as. */
		TArray<FString> ProductIdentifiers;
		TMap<FString, int32> ProductByAssetPath;

		TArray<FCallable> Callables;
		TMap<int32, int32> CallableByProduct;
		TMap<FString, int32> CallableByAssetPath;
		/** MakeCustomNodeKey -> callable: what a Custom node calls. */
		TMap<FString, int32> CallableByCustomNode;
		/** The name the code's markers give a function -> callable: one declaration however many nodes embed it. */
		TMap<FString, int32> CallableByCustomFunction;
		/** Products whose signature could not be written as the kind they are (DSH9083): written as plain functions. */
		TSet<int32> DemotedLayers;

		/** A node position under the variable its value got. */
		struct FPlacedName
		{
			FString Name;
			int32 ProductIndex = INDEX_NONE;
			int32 X = 0;
			int32 Y = 0;
		};
		TArray<FPlacedName> PlacedNames;
		TArray<IR::FIRLayoutHint> CommentHints;
	};

	/** What a graph's body ends in. */
	enum class ERootKind : uint8
	{
		/** `m.Attribute = value;` */
		SinkAttribute,
		/** The sink's whole-set input: `m = value;` plus what was written on top. */
		SinkWhole,
		/** `Out = value;` */
		OutputAssign,
		/** `return value;` */
		OutputReturn,
		/** A layer's or a blend's `inout material`: the writes on top of what came in. */
		OutputMaterial,
		/** A reflected custom-output node: `UE.X(...);` */
		Statement,
	};

	struct FRootSpec
	{
		ERootKind Kind = ERootKind::SinkAttribute;
		FIRValue Value;
		/** The material parameter, the `out` parameter; empty for a return and a statement. */
		FString Target;
		/** SinkAttribute: the attribute. OutputAssign: the output pin. What a value named after it is inlined into. */
		FString PinName;
		/** Statement: the node. OutputMaterial: the FunctionInput the parameter arrives through, or INDEX_NONE. */
		int32 Node = INDEX_NONE;
	};

	/** One reading edge of a value: a node's operand or named input, or a root of the body. */
	struct FReadEdge
	{
		/** The reading node; INDEX_NONE for a root. */
		int32 Reader = INDEX_NONE;
		/** The output slot read, after a TextureSample's channel views were folded onto its slot 0. */
		int32 Slot = 0;
		/** The pin the value is read through when the reader names its inputs; a root's attribute or output pin. */
		FString Pin;
		bool bRoot = false;
		ERootKind RootKind = ERootKind::SinkAttribute;
	};

	/** One product's graph turned into one function body. */
	class FGraphWriter
	{
	public:
		FGraphWriter(FModuleWriter& InOwner, const FIRProduct& InProduct, FNameScope& InFunctionNames);

		void BindInput(int32 NodeIndex, const FString& Identifier);
		void AddRoot(FRootSpec&& Root);

		TUniquePtr<FBlockStmt> Build();

		/** The source name the value of a node ended up under; empty when it was written inline. */
		FString FindPlacedName(int32 NodeIndex) const;

	private:
		// ------------------------------------------------------------------ IRToAstExpressions.cpp

		enum class ENodeShape : uint8
		{
			/** A name of its own: a uniform, a function input, the leading mask that IS a narrow uniform. */
			Identifier,
			/** One value, written inline or declared. */
			Single,
			/** TextureSample: one value, and four channel views of it. */
			Sample,
			/** A reflected node of several outputs: the call, restated under a selector per output. */
			MultiOutput,
			/** GetMaterialAttributes: `material.Attribute`, never a node of its own in source. */
			Attributes,
			/** Make / SetMaterialAttributes: a `material` local and its writes. */
			MaterialGroup,
			/** A call with `out` parameters: a statement, every output a local. */
			CallStatement,
			/** The sink, a function output, a reflected custom-output node. */
			Root,
		};

		struct FValuePlan
		{
			int32 DirectReads = 0;
			int32 EffectiveReads = 0;
			bool bNamed = false;
			FString Name;
			/** The statement that declared Name. */
			int32 Statement = INDEX_NONE;
			/** Inline: the expression, until its one reader takes it. */
			FExprPtr Pending;
			TArray<int32> PendingDependencies;
		};

		struct FNodePlan
		{
			ENodeShape Shape = ENodeShape::Single;
			bool bReachable = false;
			/** A Make/Set written straight into the material parameter a root names. */
			bool bWrittenThroughRoot = false;
			/** The region of the statement this node's expression lands in. */
			int32 HomeRegion = INDEX_NONE;
			bool bHomeRegionKnown = false;
			/** The hint name is this node's own: no reader shares it, and it did not come from an attribute write. */
			bool bOwnsHintName = false;
			TArray<FValuePlan> Slots;
			/** MultiOutput / Attributes: the call, or the material, cloned per use. */
			FExprPtr Base;
			TArray<int32> BaseDependencies;
			/** MaterialGroup, CallStatement: the variable whose statement makes the node on the way back. */
			FString PlacedName;
		};

		struct FPlannedStatement
		{
			TArray<FStmtPtr> Lines;
			int32 Region = INDEX_NONE;
			/** Negative until AddStatement numbers it; a root brings its own. */
			int64 Sequence = -1;
			TArray<int32> Dependencies;
		};

		void Analyze();
		void MarkReachable();
		void CountReads();
		ENodeShape ClassifyNode(int32 NodeIndex) const;
		bool IsLeafLike(int32 NodeIndex) const;
		bool IsUniformAlias(int32 NodeIndex) const;
		void DecideNames();
		int32 ReaderWeight(int32 ReaderIndex) const;
		int32 SlotOfValue(const FIRValue& Value) const;
		bool OwnsHintName(int32 NodeIndex) const;
		bool FeedsTwinPin(int32 NodeIndex) const;
		FString MakeAutoName(int32 NodeIndex, int32 Slot) const;
		int32 RegionOfNode(int32 NodeIndex) const;
		FText DescribeNode(int32 NodeIndex) const;

		void BuildNode(int32 NodeIndex);
		FExprPtr BuildNodeExpression(int32 NodeIndex);
		FExprPtr BuildCoreOpExpression(int32 NodeIndex, const FIRNode& Node);
		FExprPtr BuildEngineClassCall(const FIRNode& Node, const IR::FIRCoreOpInfo& Info);
		FExprPtr BuildReflectedCall(int32 NodeIndex, const FIRNode& Node);
		FExprPtr BuildReadableReflected(int32 NodeIndex, const FIRNode& Node);
		/** OutLocals: parameter identifier -> the local passed for it; null for a call that has no `out` parameters. */
		FExprPtr BuildUserCall(int32 NodeIndex, const FIRNode& Node, const FCallable& Callable, const TMap<FString, FString>* OutLocals);
		FExprPtr BuildTextureSample(int32 NodeIndex, const FIRNode& Node);
		FExprPtr BuildConstant(const FIRNode& Node) const;
		FExprPtr BuildPropertyValue(const FIRPropertyValue& Value, IR::ECatalogValueType Type) const;
		FExprPtr BuildCondition(const FIRValue& Condition);
		FExprPtr BuildAppend(const FIRNode& Node);
		void CollectAppendParts(const FIRValue& Value, TArray<FExprPtr>& OutParts);

		/** The expression a reader writes for Value; records what it depends on in CurrentDependencies. */
		FExprPtr TakeValue(const FIRValue& Value);
		FExprPtr TakeZero(const FIRType& Type);
		/** DSH9078, and the `0.0` that stands in. */
		FExprPtr FailValue(int32 NodeIndex, const FText& Why);
		void AddDependency(int32 StatementIndex);
		FExprPtr SelectOutput(FExprPtr Call, const FIRNode& Node, int32 Slot) const;

		// ------------------------------------------------------------------- IRToAstStatements.cpp

		int32 AddStatement(FPlannedStatement&& Statement);
		void DeclareSlot(int32 NodeIndex, int32 SlotIndex, FExprPtr Initializer, const TArray<int32>& Dependencies);
		void DeclareMaterialGroup(int32 NodeIndex);
		void DeclareCallStatement(int32 NodeIndex);
		bool ShouldNameForLength(const FExpr& Expr) const;

		void EmitRoots();
		void EmitMaterialWrites(const FRootSpec& Root, int64& InOutSequence);
		/** OrderedTarget: the material or parameter written, when writes to it have to keep their order. */
		void AddRootStatement(FStmtPtr Line, int32 Region, int64 Sequence, const FString& OrderedTarget);
		int32 RegionOfWrite(const FIRValue& Value) const;

		TUniquePtr<FBlockStmt> AssembleBody();
		bool OrderRegion(int32 Region, TArray<int32>& OutOrder) const;
		void AppendInSequence(TArray<int32>& OutOrder) const;
		int32 ParentOfRegion(int32 Region) const;
		int32 ChildRegionOnPath(int32 Ancestor, int32 Region) const;
		FString MakeRegionTitle(int32 Region);
		void EmitOrdered(const TArray<int32>& InOrder, FBlockStmt& Body);

		FModuleWriter& Owner;
		const FIRProduct& Product;
		const FIRGraph& Graph;
		FNameScope& FunctionNames;

		TArray<FRootSpec> Roots;
		TMap<int32, FString> InputIdentifiers;

		TArray<FNodePlan> Plans;
		/** Per node, one entry per reading edge. */
		TArray<TArray<FReadEdge>> Readers;
		/** Every name a root write goes by -- an attribute of the sink, a function output -- gathered on first use (OwnsHintName). */
		/** A TArray, not a TSet: FString keys hash and compare ignoring case, and these names are exact. */
		mutable TArray<FString> WriteNames;
		mutable bool bWriteNamesKnown = false;
		TArray<int32> Order;

		TArray<FPlannedStatement> Statements;
		TArray<int32> CurrentDependencies;
		int64 NextSequence = 0;
		TMap<FString, int32> PreviousWriteByTarget;
		TSet<int32> ReportedRegionTitles;
	};
}
