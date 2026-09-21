// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamShaderLang 2.0 binder, split across five translation units that share this one class,
// the same way the parser shares LangParserInternal.h:
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
//   - A bind method reports through the sink with a literal DSHnnnn code and keeps
//     going: a failing expression is typed Error, which ClassifyConversion turns into Identity
//     against everything, so one mistake does not cascade into a second message.
//   - NEVER hold a reference into FBoundModule::Expressions across a nested bind: the map rehashes.
//     Bind the children first, build the parent's FBoundExpr on the stack, then Emit() it. TypeOf()
//     and SetConversion() look the entry up again each time, which is what makes that safe.
//   - Identifier comparison is CASE-SENSITIVE (`Equals(..., ESearchCase::CaseSensitive)`) everywhere.
//     `FString::operator==` and `TMap<FString, ...>` are both case-INSENSITIVE in UE, so no symbol
//     table here is a TMap keyed by a name -- the tables are small and scanned linearly.
//   - Core-only: every engine fact arrives through IR::FBuiltinCatalog.

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

	/** The two namespace roots a reflected call may be spelled with. */
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
	 * sites by the literal shape `.Error(TEXT("DSHnnnn")`, and a code handed to a
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

	/** One operand of a Substrate sugar: the (already bound) expression, the ordinal the IR builder finds it by, the pin it feeds. */
	struct FSubstrateSugarOperand
	{
		const FExpr* Expr = nullptr;
		int32 Ordinal = 0;
		const TCHAR* Pin = nullptr;
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

		/** Decides Helper / Custom / Entry / Layer / LayerBlend / ExportFunction / Extern (§6.9). */
		void ClassifyFunctions();
		/** One FBoundProduct per Entry / Layer / LayerBlend / ExportFunction. */
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
		/** Whether a struct, a global or a function of this name is declared already; CheckNameAvailable without the message. */
		bool IsNameDeclared(const FString& Name) const;
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
		/** Takes `Substrate` out of MaterialSettings into ResolvedSubstrateMode (Substrate sugar S4); DSH7232. */
		void ResolveSubstrateMode();

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
		IR::FIRType BindMember(const FMemberExpr& Expr, const IR::FIRType* Expected = nullptr);
		IR::FIRType BindIndex(const FIndexExpr& Expr, const IR::FIRType* Expected = nullptr);
		/**
		 * The type of output OutputIndex of a node with several outputs. The catalog calls an output Numeric when the
		 * engine does not say how wide it is (every unmasked output); the declared type of what the output feeds is then
		 * the best answer there is, the rule a single-output node already follows.
		 */
		IR::FIRType TypeOfSelectedNodeOutput(const IR::FCatalogExpression& Class, int32 OutputIndex, const IR::FIRType* Expected) const;
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
		/**
		 * bSelection (legacy rule L3b): the call is the object of a selector (`F(args).Out`, `F(args)[k]`), so its `out`
		 * arguments may be absent and a void call is not refused as a value.
		 */
		IR::FIRType BindUserFunctionCall(const FCallExpr& Expr, int32 FunctionIndex, bool bStatement, bool bSelection = false);
		IR::FIRType BindReflectedCall(const FCallExpr& Expr, const FString& Namespace, const FString& Name, const FLangSpan& NameSpan, const IR::FIRType* Expected, bool bStatement);

		// ------------------------------------------------------ Substrate sugar (LangBinderSubstrate.cpp)

		/** Binds Expr as `Substrate.<NodeName>` with Operands on its pins; Spelling is how a message names the sugar. */
		IR::FIRType BindSubstrateSugarNode(const FExpr& Expr, const TCHAR* NodeName, const FText& Spelling, const TArray<FSubstrateSugarOperand>& Operands);
		/** S1: `A + B` and `A * w` where a side is a Substrate value; every other operator over one is DSH5293. Both sides are bound. */
		IR::FIRType BindSubstrateBinary(const FBinaryExpr& Expr, const IR::FIRType& LeftType, const IR::FIRType& RightType);
		/** S1: `lerp(A, B, t)` over two Substrate values. False, and nothing done, when neither of the first two is one. */
		bool TryBindSubstrateLerp(const FCallExpr& Expr, const TArray<const FExpr*>& Slots, const TArray<int32>& ArgumentOfSlot, IR::FIRType& OutType);
		/** S3: an argument that is no pin of the node but an input of a conversion node in front of it. False when Target is none. */
		bool TryBindSubstrateVirtualArgument(const FArgument& Argument, int32 ArgumentIndex, const IR::FCatalogExpression& Class, const FString& Target, FBoundExpr& InOutBinding, bool& bInOutAnyError);
		/** S3: the pairs that parameterize the same pins, and what a virtual argument needs beside it (DSH5295, DSH5296). */
		bool CheckSubstrateVirtualArguments(const FCallExpr& Expr, const IR::FCatalogExpression& Class, const FBoundExpr& Binding);
		/**
		 * S5: `S.Pin` on a builder local. False, and nothing bound, when Expr's object is no builder local. A write seals
		 * nothing; any other use of `S` does, and a write after that is DSH5297. The member is an lvalue only as the
		 * target it is being written as: `S.DiffuseAlbedo.r = 1` and `F(out S.Roughness)` have no pin to land on.
		 */
		bool TryBindSubstrateBuilderMember(const FMemberExpr& Expr, IR::FIRType& OutType);
		/** S5: marks the local a builder when its initializer is a Substrate node call without arguments. */
		void NoteSubstrateBuilderDeclared(int32 Slot, const IR::FIRType& Type, const FExpr& Initializer);
		/** S5: `S = ...`, or `S` as an `out` argument: the local holds another value now, and has no members any more. */
		void NoteSubstrateBuilderReassigned(const FExpr& Target);
		/** S5: what a call checks in one place a builder can only be asked when it is finished: `Haziness` wants a `Roughness`, `Thickness` a `Transmittance` (DSH5296). */
		void CheckSubstrateBuilderSealed(int32 Slot, const FIdentifierExpr& Use);
		/** S7: DSH5294, naming the engine version a node arrived with when that is why it is missing. */
		void ReportMissingSubstrateNode(const TCHAR* NodeName, const FLangSpan& Span, const FText& Spelling);
		bool TryDescribeSubstrateVersionGate(const FString& NodeName, FText& OutEngine) const;
		/** `Tex.Sample(UV)` / `Tex.Sample(S, UV)` / `Tex.SampleLevel(UV, L)`. */
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

		/** Lower-cases `rgba` onto `xyzw`, checks range and repeats. False after reporting. */
		bool CanonicaliseSwizzle(const FString& Mask, int32 SourceWidth, const FLangSpan& Span, FString& OutMask);

		/** A reflected literal property argument: a literal, an enumerator spelling or an asset path. */
		bool BindPropertyArgument(const FArgument& Argument, const IR::FCatalogProperty& Property, const FString& ClassName);
		/** `UE` spelled as an identifier, or `Substrate` spelled as a type. */
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

		// ------------------------------------------------- instance mode (LangBinderInstance.cpp)

		/**
		 * A `.dsi` (FModule::FileKind == Dsi): the pragma, the overrides, their initializers, the checks
		 * against FBindOptions::ParentSchema and the one MaterialInstance product. Replaces the declare
		 * pass and everything after it.
		 */
		void BindInstanceModule();
		/** `#pragma instance(...)` in a `.dsi`: Parent, the other keys in source order, the `@name` of its `///` block. */
		void BindInstancePragma(const FPragmaDecl& Pragma);
		/** DSH7255: `#pragma instance` in a file that is not a `.dsi`. */
		void ReportInstancePragmaOutsideDsi(const FPragmaDecl& Pragma);
		/** One `uniform` of a `.dsi`: its parameter kind from the spelled type and `@static`, its shape, its directives. */
		void DeclareInstanceOverride(const FVariableDecl& Decl);
		/** The directives an override may carry (`@name`, `@default`, `@static`, `@page`); DSH7262 for the rest. */
		FBoundDirectives BindInstanceOverrideDirectives(const FDocBlock& Doc, int32& OutFontPage, bool& bOutHasPage);
		/** Binds and folds every override initializer (numeric and bool kinds only). */
		void BindInstanceInitializers();
		/** Constant initializers (DSH7265), then every override against the parent schema (DSH7258-7261, 7264, 7268), or DSH7263. */
		void CheckInstanceOverrides();
		/** The one MaterialInstance product: the file stem, or the pragma's `@name` with the `.dss` split rule. */
		void BuildInstanceProduct();

		/** `.dsi` only: the `@name` written in the `///` block above `#pragma instance`. */
		FString InstanceAssetName;

		// ------------------------------------------------------- legacy rules (LangBinderLegacy.cpp)

		/** The declaration whose body, initializer or lifted calls are being bound; its FDecl::bLegacy selects the 1.x rules. */
		const FDecl* CurrentDecl = nullptr;
		/** Research-legacy section 3.7: the rules that apply only where FDecl::bLegacy is set. */
		bool IsLegacyScope() const { return CurrentDecl != nullptr && CurrentDecl->bLegacy; }
		/**
		 * Set while the value of a write into the ENTRY's material is converted. Legacy rule L22 stops there: 1.x narrowed
		 * everywhere but refused it into a material output (its DSH4046), because `Base.OpacityMask = SomeColour` would
		 * quietly become that colour's red channel and render.
		 */
		bool bConvertingIntoMaterialOutput = false;

		/** L19: the one local, parameter or global whose name matches Expr's ignoring case; fills the binding and warns DSH5275. */
		bool TryBindLegacyIdentifierIgnoringCase(const FIdentifierExpr& Expr, FBoundExpr& OutBinding);
		/** Whether any visible local, parameter or global matches ignoring case. No diagnostic. */
		bool HasDeclarationIgnoringCase(const FString& Name) const;
		/** L19: the one function whose name matches ignoring case, else INDEX_NONE. No diagnostic. */
		int32 FindFunctionIgnoringCaseUniquely(const FString& Name) const;
		/** L19: DSH5275, a name accepted through the case fallback. */
		void ReportLegacyCaseFallback(const FString& Written, const FString& Declared, const FLangSpan& Span);
		/** L19: DSH5276, an engine name (class, attribute, pin, property, output) accepted through the case fallback. */
		void ReportLegacyCatalogCaseFallback(const FString& Written, const FString& Catalogued, const FLangSpan& Span);
		/** L19: the one catalog class a `Class = "..."` specifier matches ignoring case, else INDEX_NONE. */
		int32 FindExpressionByClassIgnoringCaseUniquely(const FString& ClassSpecifier) const;
		/** L19: the one entry of Namespace whose short name or alias matches Name ignoring case, else INDEX_NONE. */
		int32 FindExpressionIgnoringCaseUniquely(const FString& Namespace, const FString& Name) const;
		/** L19: the one input / property / output (aliases included) matching ignoring case, else INDEX_NONE. */
		static int32 FindCatalogInputIgnoringCaseUniquely(const IR::FCatalogExpression& Class, const FString& Name);
		static int32 FindCatalogPropertyIgnoringCaseUniquely(const IR::FCatalogExpression& Class, const FString& Name);
		static int32 FindCatalogOutputIgnoringCaseUniquely(const IR::FCatalogExpression& Class, const FString& Name);
		/** L19: the one core op whose HLSL name (or GLSL alias) matches ignoring case, else null. */
		static const IR::FIRCoreOpInfo* FindCoreOpIgnoringCase(const FString& Name, bool& bOutIsGlslAlias);
		/** L12: the one enumerator a 1.x spelling names, compared ignoring case, spaces, `_`, `-`, `:`, `.`, `/` and an enum prefix. */
		static bool TryMatchLegacyEnumerator(const FString& Spelling, const IR::FCatalogProperty& Property, FString& OutEnumerator);

		/** L3b: the Extern / ExportFunction / Custom function a legacy selector may pick an output of, else INDEX_NONE. */
		int32 FindSelectableLegacyCallee(const FCallExpr& Call) const;
		/** The 1.x outputs of a function in ordinal order: the return value when not void ("Result"), then the out and inout parameters. */
		static void CollectLegacyOutputs(const FBoundFunction& Function, TArray<FString>& OutNames, TArray<FString>& OutPinNames, TArray<IR::FIRType>& OutTypes);
		/** L3b: whether Name spells one of Function's 1.x outputs (its identifier or its `@pin` name), ignoring case. */
		static bool NamesLegacyOutput(const FBoundFunction& Function, const FString& Name);
		/** L3b: the kinds a legacy call may select an output of, or read as its output 0: Extern, ExportFunction, Custom. */
		static bool IsSelectableLegacyKind(EBoundFunctionKind Kind);
		/** L3b: `F(args).Out` (OutputName) or `F(args)[k]` (OutputOrdinal): binds Call in selection mode and emits FunctionCallOutput on Selector. */
		IR::FIRType BindFunctionCallOutput(const FExpr& Selector, const FCallExpr& Call, int32 FunctionIndex, const FString& OutputName, int32 OutputOrdinal, const FLangSpan& SelectorSpan);

		/** L5: an undeclared identifier receiving an output of a legacy statement call becomes a local of that output's type (Info DSH5283). */
		void DeclareLegacyImplicitOutLocal(const FExpr& Argument, const IR::FIRType& Type, const FString& Receives, const FString& CalleeName);

		/** L8: every call lifted out of an opaque body (FFunctionDecl::HoistedCalls), bound once in its function's scope. */
		void BindHoistedCalls();
		/** L8: at a call site, each name the callee's lifted calls could not resolve: DSH6325 when the caller has it, DSH6326 when nothing does. */
		void CheckHoistedCallNamesAtCallSite(const FCallExpr& Call, int32 CalleeIndex);
		/** L8: true while BindHoistedCalls binds; an unresolved identifier is recorded rather than reported. */
		bool bBindingHoistedCall = false;
		int32 HoistingFunctionIndex = INDEX_NONE;
		/** L8: per function index, the names its lifted calls could not resolve, with their spans (in that function's file). */
		TMap<int32, TArray<TPair<FString, FLangSpan>>> HoistedUnresolvedNames;
		/** L8: DSH6325 / DSH6326 report keys already said, compared case-sensitively. */
		TArray<FString> HoistedNamesReported;

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
		/** The same for an operand: a Custom-class call answers with the output 0 it declared itself. */
		IR::FIRType ResolveNodeDefaultOf(const FExpr& Operand) const;
		/** The bound Custom-class call behind Object (parentheses looked through), when it declared its own outputs; else null. */
		const FBoundExpr* FindCallOutputs(const FExpr& Object) const;
		/** Fills Binding.CallOutputNames / CallOutputTypes from the call's `OutputType` and `AdditionalOutputs` arguments. */
		void CollectCustomClassOutputs(const FCallExpr& Expr, const IR::FCatalogExpression& Class, FBoundExpr& Binding);

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
		/** `#pragma material(Substrate = ...)`, taken out of MaterialSettings. */
		IR::EIRSubstrateMode ResolvedSubstrateMode = IR::EIRSubstrateMode::Legacy;

		// Substrate sugar S5, per function body.
		/** The builder locals a value has been taken from: their node is what it is, and a member write comes too late. */
		TSet<int32> SealedBuilderSlots;
		/** What has been written to each builder local, so that a read knows there is something to read. */
		TMap<int32, TArray<FString>> BuilderMembersWritten;
		/** The builder locals that were assigned whole (`S = ...`, an `out` argument): what they hold has no members. */
		TSet<int32> ReassignedBuilderSlots;
		/** How many `if` arms enclosed each builder's declaration; a write under more of them is DSH5298. */
		TMap<int32, int32> BuilderDeclBranchDepth;
		/**
		 * The target of the assignment, `++` or `--` being bound, parentheses stripped: a builder member that IS this
		 * expression is a write. By identity, so that `A[S.Pin > 0 ? 0 : 1] = x` reads the pin it mentions.
		 */
		const FExpr* BuilderWriteTarget = nullptr;
		/** ...and the write reads first: `S.Pin += x`, `++S.Pin`. */
		bool bBuilderWriteReadsFirst = false;
		/** Set while a builder member binds its own object, which takes no value from the local. */
		bool bBindingBuilderObject = false;
		/**
		 * Set while the initializer of a Substrate local is bound: a node call without arguments there begins a builder,
		 * and what it leaves open is asked when the value is first used (CheckSubstrateBuilderSealed), not here.
		 */
		bool bBindingBuilderInitializer = false;
		/** How many `if` arms enclose the statement being bound. */
		int32 BranchDepth = 0;
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
