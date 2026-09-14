// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamShaderLang 2.0 binder, split across five translation units that share this one class,
// the same way the M1 parser shares LangParserInternal.h:
//
//   LangBinder.cpp             entry point, includes, the declare pass, function kinds, products,
//                              the call graph, and the out-of-line members of LangBound.h
//   LangBinderDirectives.cpp   `///` blocks -> FBoundDirectives, `#pragma material` / `layout` /
//                              `region`, the backend
//   LangBinderExpressions.cpp  every expression kind, conversions, constant folding
//   LangBinderStatements.cpp   bodies, scopes, control flow, loop trip counts, in-body regions
//   LangSymbolIndex.cpp        BuildDreamShaderSymbolIndexJson
//
// Conventions every method follows:
//
//   - A bind method reports through the sink with a literal DSHnnnn code (CONTRACT §0.8) and keeps
//     going: a failing expression is typed Error, which ClassifyConversion turns into Identity
//     against everything, so one mistake does not cascade into a second message.
//   - NEVER hold a reference into FBoundModule::Expressions across a nested bind: the map rehashes.
//     Bind the children first, build the parent's FBoundExpr on the stack, then Emit() it. TypeOf()
//     and SetConversion() look the entry up again each time, which is what makes that safe.
//   - Identifier comparison is CASE-SENSITIVE (`Equals(..., ESearchCase::CaseSensitive)`) everywhere.
//     `FString::operator==` and `TMap<FString, ...>` are both case-INSENSITIVE in UE, so no symbol
//     table here is a TMap keyed by a name -- the tables are small and scanned linearly.
//   - Core-only (CONTRACT §0.9): every engine fact arrives through IR::FBuiltinCatalog.

#pragma once

#include "CoreMinimal.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

namespace UE::DreamShader::Lang::Private
{
	/** One lexical scope of a function body. Small, so it is scanned, never hashed (see the header note). */
	struct FBinderScope
	{
		/** Declared name -> slot in FBoundFunction::Locals, in declaration order. */
		TArray<TPair<FString, int32>> Names;
	};

	/** What a `///` block hangs off; decides which directives are meaningful there. */
	enum class EDirectiveTarget : uint8
	{
		Uniform,
		Constant,
		Function,
		StructField,
	};

	/** The two namespace roots a reflected call may be spelled with (plan §11 #11). */
	namespace Namespaces
	{
		inline const TCHAR* const UE = TEXT("UE");
		inline const TCHAR* const Substrate = TEXT("Substrate");
	}

	/** The one reflected name that takes its class from an argument. */
	inline const TCHAR* const ExpressionEscapeHatch = TEXT("Expression");

	/**
	 * Where a conversion was needed, which is the same thing as which diagnostic a failed one
	 * raises. It is an enum and not the code itself because .skill/gen-diagnostics.ps1 finds raise
	 * sites by the literal shape `.Error(TEXT("DSHnnnn")` (CONTRACT §0.8), and a code handed to a
	 * helper as a parameter is invisible to it. Convert() spells all five out.
	 */
	enum class EConversionSite : uint8
	{
		/** An operand of an operator, a builtin or a constructor: DSH4226. */
		Operand,
		/** The right of an `=`, an initializer, a default, a `return`: DSH4228. */
		Assignment,
		/** `(float3)x`: DSH4223. */
		Cast,
		/** The condition of an `if` / `for` / `while` / `?:`: DSH4260. */
		Condition,
		/** A pin of a reflected node or a texture sample: DSH5214. */
		Pin,
	};

	class FLangBinder
	{
	public:
		FLangBinder(const FModule& InModule, const FBindOptions& InOptions, FBoundModule& InBound, FLangDiagnosticSink& InDiagnostics);

		/** Declare pass, classification, products, bind pass, recursion. */
		void Run();

		// ------------------------------------------------------------- declare pass (LangBinder.cpp)

		/** Walks one module's declarations; recurses through `#include` / `import` at the point they sit. */
		void DeclareModule(const FModule& InModule);
		void ResolveInclude(const FIncludeDecl& Decl, const FModule& From);
		void DeclareStruct(const FStructDecl& Decl, const FString& File);
		void DeclareGlobal(const FVariableDecl& Decl, const FString& File);
		void DeclareFunction(const FFunctionDecl& Decl, const FString& File);

		/** Decides Helper / Custom / Entry / Layer / LayerBlend / ExportFunction / Extern (CONTRACT §2, §6.9). */
		void ClassifyFunctions();
		/** One FBoundProduct per Entry / Layer / LayerBlend / ExportFunction (CONTRACT §6.9). */
		void BuildProducts();
		/** Marks every Helper that can reach itself through the call graph; I2 reports it. */
		void DetectRecursion();

		/**
		 * A spelled type resolved against this module: a Named type is a user struct (or DSH4201);
		 * everything else goes through IR::TypeFromBuiltinRef. False after reporting.
		 */
		bool ResolveTypeRef(const FTypeRef& Ref, IR::FIRType& OutType);
		/** DSH4210 when the name is already a struct, a global or a function; names both files. */
		bool CheckNameAvailable(const FString& Name, const FLangSpan& Span, const FString& File);
		/**
		 * The single `[n]` a declarator may carry: 0 when there is none, the element count otherwise.
		 * An unsized `[]` takes its count from Initializer. Reports DSH4241 / DSH4242 and returns false.
		 */
		bool ResolveArrayCount(const TArray<FExprPtr>& Dimensions, const FExpr* Initializer, const FLangSpan& Span, int32& OutCount);

		// -------------------------------------------------------- directives (LangBinderDirectives.cpp)

		FBoundDirectives BindDirectives(const FDocBlock& Doc, EDirectiveTarget Target, const FTypeRef* DeclaredType);
		/** Merges one `#pragma material(...)` line into FBoundModule::MaterialSettings; DSH7200 on a repeat. */
		void BindMaterialPragma(const FPragmaDecl& Decl);
		/** One `#pragma layout(...)` line -> IR::FIRLayoutHint, carried through untouched. */
		void BindLayoutPragma(const FPragmaDecl& Decl);
		/** Takes `Backend` out of MaterialSettings into ResolvedBackend; DSH7201 / DSH7202. */
		void ResolveBackend();

		/** `#pragma region` at file scope or in a body: pushes a FIRRegion and returns its index. */
		int32 OpenRegion(const FString& Title, const FLangSpan& Span, TArray<int32>& Stack);
		/** `#pragma endregion`: pops; DSH4240 when nothing is open. */
		void CloseRegion(const FLangSpan& Span, TArray<int32>& Stack);
		/** Reports DSH4240 for every region still open at the end of a file or a body, and empties the stack. */
		void CloseDanglingRegions(TArray<int32>& Stack);

		// ------------------------------------------------------ expressions (LangBinderExpressions.cpp)

		/**
		 * Types one expression, resolves every name in it and records a FBoundExpr for it and for
		 * every node under it. Expected drives initializer lists and struct constructors and is null
		 * everywhere else. bStatement is true only at statement level, where a custom-output
		 * reflected call and a `void` call are legal.
		 */
		IR::FIRType BindExpr(const FExpr& Expr, const IR::FIRType* Expected = nullptr, bool bStatement = false);

		IR::FIRType BindLiteral(const FLiteralExpr& Expr);
		IR::FIRType BindIdentifier(const FIdentifierExpr& Expr);
		IR::FIRType BindMember(const FMemberExpr& Expr);
		IR::FIRType BindIndex(const FIndexExpr& Expr);
		IR::FIRType BindCall(const FCallExpr& Expr, const IR::FIRType* Expected, bool bStatement);
		IR::FIRType BindUnary(const FUnaryExpr& Expr);
		IR::FIRType BindBinary(const FBinaryExpr& Expr);
		IR::FIRType BindAssign(const FAssignExpr& Expr);
		IR::FIRType BindConditional(const FConditionalExpr& Expr);
		IR::FIRType BindCast(const FCastExpr& Expr);
		IR::FIRType BindInitializerList(const FInitializerListExpr& Expr, const IR::FIRType* Expected);
		/** `float Kernel[3] = { ... }`: one element per slot, each against ElementType. */
		IR::FIRType BindArrayInitializer(const FExpr& Init, const IR::FIRType& ElementType, int32 Count, TArray<double>& OutValues);

		IR::FIRType BindSwizzle(const FMemberExpr& Expr, const IR::FIRType& ObjectType);
		/**
		 * A swizzle on a node whose outputs are all channel views of one value (`UE.VertexColor().a`):
		 * the view that publishes exactly those channels, else a swizzle inside the default output's.
		 * ViewWidth is the value's width. DSH5201 when the channels sit on different outputs.
		 */
		IR::FIRType BindChannelViewSwizzle(const FMemberExpr& Expr, int32 CatalogIndex, int32 ViewWidth);
		IR::FIRType BindConstructor(const FCallExpr& Expr, const FTypeRef& TypeRef);
		IR::FIRType BindStructConstructor(const FCallExpr& Expr, int32 StructIndex);
		IR::FIRType BindCoreOpCall(const FCallExpr& Expr, const IR::FIRCoreOpInfo& Info);
		IR::FIRType BindUserFunctionCall(const FCallExpr& Expr, int32 FunctionIndex, bool bStatement);
		IR::FIRType BindReflectedCall(const FCallExpr& Expr, const FString& Namespace, const FString& Name, const FLangSpan& NameSpan, const IR::FIRType* Expected, bool bStatement);
		/** `Tex.Sample(UV)` / `Tex.Sample(S, UV)` / `Tex.SampleLevel(UV, L)` (CONTRACT §6.5). */
		IR::FIRType BindTextureSampleMethod(const FCallExpr& Expr, const FMemberExpr& Callee, const IR::FIRType& TextureType);
		/** `Texture2DSample(Tex, S, UV)` / `Texture2DSampleLevel(Tex, S, UV, L)`. */
		IR::FIRType BindTextureSampleFunction(const FCallExpr& Expr, bool bHasLevel);

		/**
		 * Types an already-bound operand list by the op's EIRTypingRule, records the conversion on
		 * every operand and folds the result when all of them are constant. The caller fills Args.
		 */
		FBoundExpr BuildCoreOp(const IR::FIRCoreOpInfo& Info, const TArray<const FExpr*>& Operands, const FLangSpan& Span);

		/**
		 * Records the conversion an already-bound operand needs to stand in for To, and reports Code
		 * when there is none. What names the place, for the message.
		 */
		IR::EIRConversion Convert(const FExpr& Operand, const IR::FIRType& To, EConversionSite Site, const FText& What);

		/** Lower-cases `rgba` onto `xyzw`, checks range and repeats (CONTRACT §6.6). False after reporting. */
		bool CanonicaliseSwizzle(const FString& Mask, int32 SourceWidth, const FLangSpan& Span, FString& OutMask);

		/** A reflected literal property argument: a literal, an enumerator spelling or an asset path. */
		bool BindPropertyArgument(const FArgument& Argument, const IR::FCatalogProperty& Property, const FString& ClassName);
		/** `UE` spelled as an identifier, or `Substrate` spelled as a type (plan §11 #11). */
		bool IsNamespaceRoot(const FExpr& Object, FString& OutNamespace) const;

		/** How many elements the value this expression names has, or 0 when it is not an array. */
		int32 GetArrayCount(const FExpr& Expr) const;
		/** The folded elements of a `static const` array the expression names; false when there are none. */
		bool GetArrayValues(const FExpr& Expr, const TArray<double>*& OutValues) const;

		// ------------------------------------------------------- statements (LangBinderStatements.cpp)

		/** Every global initializer, in declaration order, so a constant may read the one above it. */
		void BindGlobals();
		void BindFunctionBody(int32 FunctionIndex);
		void BindStmt(const FStmt& Stmt);
		void BindBlock(const FBlockStmt& Stmt);
		void BindVarDecl(const FVarDeclStmt& Stmt);
		void BindIf(const FIfStmt& Stmt);
		void BindFor(const FForStmt& Stmt);
		void BindWhile(const FWhileStmt& Stmt);
		void BindDoWhile(const FDoWhileStmt& Stmt);
		void BindReturn(const FReturnStmt& Stmt);

		void PushScope();
		void PopScope();
		/** A new slot in FBoundFunction::Locals; DSH4220 when it shadows an outer local or a parameter. */
		int32 DeclareLocal(const FString& Name, const IR::FIRType& Type, const FDeclarator* Decl, int32 ArrayCount, const FLangSpan& Span);
		/**
		 * Notes that Node writes the local Target names, for LocalWrites. Every way a local is
		 * written goes through here: an assignment, an increment, and an `out` / `inout` argument.
		 */
		void RecordLocalWrite(const FNode& Node, const FExpr& Target);
		/** Binds a condition and reports DSH4260 when it cannot drive a branch. */
		void BindCondition(const FExpr& Condition, const FText& What);
		/** Records FBoundModule::LoopTripCounts when, and only when, the count can be proved. */
		void ProveTripCount(const FStmt& Loop, const FStmt* Init, const FExpr* Condition, const FExpr* Step, const FStmt* Body);

		// ------------------------------------------------------------------------------- shared state

		const FModule& GetRootModule() const { return RootModule; }
		const IR::FBuiltinCatalog& GetCatalog() const { return Catalog; }
		const FBindOptions& GetOptions() const { return Options; }
		FBoundModule& GetBound() { return Bound; }
		FLangDiagnosticSink& GetDiagnostics() { return Diagnostics; }

		// --------------------------------------------------------------------------------- utilities

		/** Adds the binding for one expression and returns its type. Never keep the reference. */
		IR::FIRType Emit(const FExpr& Expr, FBoundExpr&& Binding);
		const FBoundExpr* Lookup(const FExpr& Expr) const { return Bound.Expressions.Find(&Expr); }
		IR::FIRType TypeOf(const FExpr& Expr) const;
		bool IsLValue(const FExpr& Expr) const;
		bool IsConstantExpr(const FExpr& Expr) const;
		/** The folded value of an already-bound expression; false when it is not constant. */
		bool GetConstant(const FExpr& Expr, double Out[4], int32& OutComponents) const;
		void SetConversion(const FExpr& Expr, IR::EIRConversion Conversion);

		/**
		 * A `Node` type resolved to the type of its default (index 0) output, which is what
		 * EIRConversion::DefaultOutput names. Anything else comes back unchanged.
		 */
		IR::FIRType ResolveNodeDefault(const IR::FIRType& Type) const;

		/** An Error-typed binding for an expression whose diagnostic was already reported. */
		IR::FIRType Fail(const FExpr& Expr);

		/** `float3`, `material`, `ToonInputs`, `<error>` -- for messages. */
		FText DescribeType(const IR::FIRType& Type) const;
		/** The one candidate that differs only in case, or an empty string. */
		static FString SuggestCaseInsensitive(const FString& Name, TArrayView<const FString> Candidates);

		/** Case-sensitive lookups; the tables are small (see the header note on TMap). */
		int32 FindStruct(const FString& Name) const;
		int32 FindGlobal(const FString& Name) const;
		int32 FindFunction(const FString& Name) const;
		int32 FindLocal(const FString& Name) const;
		int32 FindParam(const FString& Name) const;

		/** FBoundGlobal::ArrayCount, guarded against a stale index. */
		int32 GetGlobalArrayCount(int32 GlobalIndex) const;

		// ---------------------------------------------------------------------- per-function bind state

		int32 CurrentFunctionIndex = INDEX_NONE;
		FBoundFunction* CurrentFunction = nullptr;
		TArray<FBinderScope> Scopes;
		TArray<int32> BodyRegionStack;
		/** The innermost region a statement is in; INDEX_NONE outside any. */
		int32 CurrentRegion = INDEX_NONE;
		/** The region a body's own regions nest inside: the file-scope box the function was declared in. */
		int32 OuterRegion = INDEX_NONE;
		int32 LoopDepth = 0;
		/** Function indices this function calls; folded into CallGraph when the body is done. */
		TSet<int32> CurrentCallees;
		TArray<TSet<int32>> CallGraph;

		/** Parallel to the current function's Locals; the folded elements of a constant array local. */
		TArray<TArray<double>> LocalArrayValues;
		/**
		 * Every write to a local made by the CURRENT function's body: the node that wrote it and the
		 * slot it wrote. ProveTripCount reads this to answer "does anything but the step touch the
		 * induction variable", and it has to be per function: the only thing that says where a node
		 * sits is its span, spans are offsets into ONE file, and an included header's offsets
		 * overlap the root file's. Reset by BindFunctionBody.
		 */
		TArray<TPair<const FNode*, int32>> LocalWrites;
		/**
		 * Parallel to the CURRENT function's Params: true once the body has written the parameter --
		 * as a whole, a swizzle, an element or a field of it, or by handing it to another call's
		 * `out` / `inout` parameter. Read at the end of BindFunctionBody for DSH6211 (an `out`
		 * parameter never assigned). Reset by BindFunctionBody.
		 */
		TArray<bool> ParamWrites;
		/**
		 * Parallel to FBoundModule::Globals; the folded elements of a `static const` array. The
		 * COUNT lives on FBoundGlobal::ArrayCount, which the IR builder reads; the values are a fold
		 * cache this pass keeps for itself, because every array element read folds to a constant and
		 * nothing downstream has an array to put them in.
		 */
		TArray<TArray<double>> GlobalArrayValues;

		/** `#pragma material(Backend = ...)`, taken out of MaterialSettings. */
		IR::EIRBackend ResolvedBackend = IR::EIRBackend::Graph;
		/** Where each MaterialSettings key was written, so a repeat can name the first one. */
		TMap<FString, FLangSpan> MaterialSettingSpans;
		/** The file each MaterialSettings key was written in; a header may carry the pragma. */
		TMap<FString, FString> MaterialSettingFiles;
		/** True once any `#pragma material` line has been seen; drives DSH7203. */
		bool bHasMaterialPragma = false;
		FLangSpan FirstMaterialPragmaSpan;
		FString FirstMaterialPragmaFile;

		/**
		 * The file every span raised right now lies in. The binder walks included modules through
		 * ONE sink, so a diagnostic about a header has to name the header rather than the `.dss`
		 * that included it -- otherwise the bridge opens the wrong file at the header's line number.
		 * Every raise site passes this to the file-qualified sink overload; the sink compares it
		 * against its own path, so a span in the main module costs nothing.
		 *
		 * Set by DeclareModule (saved and restored around the include recursion), by the
		 * ClassifyFunctions loop, by BindGlobals per global and by BindFunctionBody per body.
		 */
		FString CurrentFile;

	private:
		const FModule& RootModule;
		const FBindOptions& Options;
		const IR::FBuiltinCatalog& Catalog;
		FBoundModule& Bound;
		FLangDiagnosticSink& Diagnostics;

		/** Resolved include paths currently being walked, for DSH4211. */
		TArray<FString> IncludeStack;
		/** Resolved include paths already declared, so a diamond include is declared once. */
		TArray<FString> VisitedIncludes;
		/** Include paths as written, per including file, so the resolver is called once per pair. */
		TArray<TPair<FString, FString>> RequestedIncludes;
	};

	// ------------------------------------------------------------------------------- shared helpers

	/** The HLSL promotion order used when a core op has to pick one kind for mixed operands. */
	IR::EIRTypeKind PromoteNumericKind(IR::EIRTypeKind A, IR::EIRTypeKind B);
	/** float1..4 / bool1..4 of the given kind and width; matrices keep their shape. */
	IR::FIRType MakeNumeric(IR::EIRTypeKind Kind, int32 Components);
	/** True when a value of this type may drive an `if` / `for` / `while` condition. */
	bool IsConditionType(const IR::FIRType& Type);

	/** Writes one JSON string literal, quotes included, escaping per RFC 8259. */
	void AppendJsonString(FString& Out, const FString& Value);
}
