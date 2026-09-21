// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The DreamShaderLang AST -- the ONE tree both front ends produce and everything downstream
// (semantic analysis, the IR builder, the printer, the migrate tool) consumes.
//
// Design rules, so the two parsers and the printer agree without talking to each other:
//
//   1. The tree is syntax, not meaning. A member access is FMemberExpr whether it turns out to be
//      a swizzle, a struct field or `UE.TexCoord`; an identifier is FIdentifierExpr whether it names
//      a uniform, a local or a builtin. Resolution is the semantic pass's job (M2) and is recorded
//      there, not here, so a parser never has to know the symbol table.
//   2. Every node carries the span it was parsed from. Diagnostics, the printer's raw-body slices,
//      node<->source navigation and the future language service all read it.
//   3. Ownership is TUniquePtr down the tree; nodes are non-copyable. Sub-kinds are told apart by
//      ENodeKind and a static_cast -- no RTTI, no virtual visitors. `As<T>()` below checks the kind.
//   4. The legacy front end lowers 1.x constructs onto the SAME node kinds (a `Shader` block is a
//      FFunctionDecl with an `inout material` parameter plus FVariableDecls; `Outputs` bindings are
//      assignments in that body). Where 1.x carries information 2.0 spells differently -- Layout
//      sections, #Region -- it lands in the same FPragmaDecl kinds the 2.0 `#pragma` lines use.
//      Nothing in this header is 1.x-only.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangSource.h"
#include "Misc/Optional.h"

namespace UE::DreamShader::Lang
{
	// ------------------------------------------------------------------------------------------
	// Types
	// ------------------------------------------------------------------------------------------

	enum class ETypeCategory : uint8
	{
		/** `void` -- return type only. */
		Void,
		/** `float`, `int`, `uint`, `bool`, `half`, `double`. */
		Scalar,
		/** `float2` .. `float4`, `int2`.., `bool2`.. -- Rows holds the component count (2..4). */
		Vector,
		/** `float2x2` .. `float4x4` -- Rows x Cols. */
		Matrix,
		/** `Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture`. */
		Texture,
		/** `SamplerState`. */
		Sampler,
		/** `material` -- the built-in struct whose fields are the engine's material pins. */
		Material,
		/** `Substrate` -- the opaque Substrate value type. */
		Substrate,
		/** A name the parser did not recognise: a user `struct`, or a spelling the semantic pass will judge. */
		Named,
	};

	enum class EScalarKind : uint8
	{
		None,
		Float,
		Half,
		Double,
		Int,
		UInt,
		Bool,
	};

	enum class ETextureKind : uint8
	{
		None,
		Texture2D,
		TextureCube,
		Texture2DArray,
		Texture3D,
		/** The 1.x spelling of a 3D texture; kept distinct so the printer reproduces what was written. */
		VolumeTexture,
	};

	/**
	 * A type as written. The parser classifies the built-in spellings (see ParseType in the parser
	 * contract); everything else is Named and resolved later.
	 */
	struct FTypeRef
	{
		/** The spelling as written: `float3`, `Texture2D`, `material`, `MyStruct`. */
		FString Name;
		ETypeCategory Category = ETypeCategory::Named;
		EScalarKind Scalar = EScalarKind::None;
		ETextureKind Texture = ETextureKind::None;
		/** Vector: component count. Matrix: rows. Otherwise 1. */
		int32 Rows = 1;
		/** Matrix: columns. Otherwise 1. */
		int32 Cols = 1;
		FLangSpan Span;

		bool IsVoid() const { return Category == ETypeCategory::Void; }
		bool IsScalar() const { return Category == ETypeCategory::Scalar; }
		bool IsVector() const { return Category == ETypeCategory::Vector; }
		bool IsMatrix() const { return Category == ETypeCategory::Matrix; }
		bool IsNumeric() const { return IsScalar() || IsVector() || IsMatrix(); }
		bool IsTexture() const { return Category == ETypeCategory::Texture; }
		bool IsMaterial() const { return Category == ETypeCategory::Material; }
		/** Scalar/vector component count: 1 for a scalar, 2..4 for a vector, Rows*Cols for a matrix, 0 otherwise. */
		int32 ComponentCount() const
		{
			switch (Category)
			{
			case ETypeCategory::Scalar: return 1;
			case ETypeCategory::Vector: return Rows;
			case ETypeCategory::Matrix: return Rows * Cols;
			default: return 0;
			}
		}
	};

	// ------------------------------------------------------------------------------------------
	// Doc-comment directives
	// ------------------------------------------------------------------------------------------

	/**
	 * One `@key value` inside a `///` block. Key is the canonical lower-case spelling
	 * (`@Group` and `@group` are the same directive); Value is trimmed and may be empty.
	 * Unknown keys are kept verbatim: the semantic pass reflects them onto the parameter node, as
	 * 1.x does with unknown metadata keys.
	 */
	struct FDocDirective
	{
		FString Key;
		FString Value;
		FLangSpan Span;
	};

	/** Where one piece of a `///` block sat, so the printer can put it back beside its neighbours. */
	struct FDocItem
	{
		enum class EKind : uint8
		{
			FreeText,
			Directive,
		};

		EKind Kind = EKind::FreeText;
		/** Index into FDocBlock::FreeText or FDocBlock::Directives, by Kind. */
		int32 Index = INDEX_NONE;
		/** 0-based `///` line inside the block; items with the same Line were written on one physical line. */
		int32 Line = 0;
	};

	/**
	 * The `///` lines immediately preceding a declaration (or a struct field). Text that is not a
	 * directive is collected into FreeText, one line per source line, and is what `@desc` falls
	 * back to when it is absent.
	 */
	struct FDocBlock
	{
		TArray<FDocDirective> Directives;
		TArray<FString> FreeText;
		/** Every FreeText and Directive entry in source order. Empty for a hand-built block: the printer then uses its canonical order. */
		TArray<FDocItem> Order;
		FLangSpan Span;

		bool IsEmpty() const { return Directives.Num() == 0 && FreeText.Num() == 0; }
		/** First directive with the given canonical key, or null. */
		const FDocDirective* Find(const TCHAR* Key) const
		{
			for (const FDocDirective& Directive : Directives)
			{
				if (Directive.Key.Equals(Key, ESearchCase::CaseSensitive))
				{
					return &Directive;
				}
			}
			return nullptr;
		}
		bool Has(const TCHAR* Key) const { return Find(Key) != nullptr; }
	};

	// ------------------------------------------------------------------------------------------
	// Node base
	// ------------------------------------------------------------------------------------------

	enum class ENodeKind : uint8
	{
		// expressions
		LiteralExpr,
		IdentifierExpr,
		TypeExpr,
		MemberExpr,
		IndexExpr,
		CallExpr,
		UnaryExpr,
		BinaryExpr,
		AssignExpr,
		ConditionalExpr,
		CastExpr,
		InitializerListExpr,
		ParenExpr,

		// statements
		VarDeclStmt,
		ExprStmt,
		BlockStmt,
		IfStmt,
		ForStmt,
		WhileStmt,
		DoWhileStmt,
		ReturnStmt,
		BreakStmt,
		ContinueStmt,
		DiscardStmt,
		EmptyStmt,
		PragmaStmt,

		// declarations
		VariableDecl,
		FunctionDecl,
		StructDecl,
		IncludeDecl,
		PragmaDecl,
	};

	DREAMSHADERLANG_API const TCHAR* LexToString(ENodeKind Kind);

	struct FNode
	{
		ENodeKind Kind;
		FLangSpan Span;

		explicit FNode(ENodeKind InKind) : Kind(InKind) {}
		virtual ~FNode() = default;

		FNode(const FNode&) = delete;
		FNode& operator=(const FNode&) = delete;

		template <typename T>
		T* As() { return Kind == T::StaticKind ? static_cast<T*>(this) : nullptr; }
		template <typename T>
		const T* As() const { return Kind == T::StaticKind ? static_cast<const T*>(this) : nullptr; }
		template <typename T>
		bool Is() const { return Kind == T::StaticKind; }
	};

	// ------------------------------------------------------------------------------------------
	// Expressions
	// ------------------------------------------------------------------------------------------

	struct FExpr : FNode
	{
		using FNode::FNode;
	};

	using FExprPtr = TUniquePtr<FExpr>;

	enum class ELiteralKind : uint8
	{
		Int,
		UInt,
		Float,
		Bool,
		String,
	};

	struct FLiteralExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::LiteralExpr;
		FLiteralExpr() : FExpr(StaticKind) {}

		ELiteralKind LiteralKind = ELiteralKind::Int;
		/** The lexeme as written (numbers), or the resolved value (strings). Printed back verbatim. */
		FString Text;
		uint64 Integer = 0;
		double Real = 0.0;
		bool bBool = false;
	};

	struct FIdentifierExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::IdentifierExpr;
		FIdentifierExpr() : FExpr(StaticKind) {}

		FString Name;
	};

	/** A type in expression position: the callee of a constructor call `float3(...)`, or a cast target. */
	struct FTypeExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::TypeExpr;
		FTypeExpr() : FExpr(StaticKind) {}

		FTypeRef Type;
	};

	/** `Object.Member` -- swizzles, struct fields and the `UE.` / `Substrate.` namespaces all look like this. */
	struct FMemberExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::MemberExpr;
		FMemberExpr() : FExpr(StaticKind) {}

		FExprPtr Object;
		FString Member;
		FLangSpan MemberSpan;
	};

	struct FIndexExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::IndexExpr;
		FIndexExpr() : FExpr(StaticKind) {}

		FExprPtr Object;
		FExprPtr Index;
	};

	/**
	 * One call argument. Name is empty for a positional argument. A named argument is spelled
	 * `Name = value` at the top level of the argument (an assignment expression as an argument must
	 * be parenthesised), which is how `UE.TexCoord(Index = 0)` keeps its 1.x spelling.
	 */
	struct FArgument
	{
		FString Name;
		FLangSpan NameSpan;
		FExprPtr Value;
		FLangSpan Span;
		/**
		 * 1.x `Expression(Class = "...").Pin[i] = x`: the input pin by engine index (UMaterialExpression::GetInput order); Name
		 * is empty. INDEX_NONE otherwise. Also parsed in 2.0 files, because a migrated `.dss` prints it and must read it back;
		 * it binds on a `UE.Expression(Class = ...)` call only.
		 */
		int32 PinIndex = INDEX_NONE;
	};

	/** `Callee(args)`. Callee is an identifier, a member chain, or a FTypeExpr for a constructor. */
	struct FCallExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::CallExpr;
		FCallExpr() : FExpr(StaticKind) {}

		FExprPtr Callee;
		TArray<FArgument> Arguments;
		/**
		 * A 1.x node call only: what its `OutputType = "float3"` said, when that is a scalar or a vector. The 1.x generator
		 * had no catalog and took the author's word for how wide a node's value is -- and put no conversion on the wire when
		 * the word was wrong. The legacy front end drops the argument (2.0 reads widths from the catalog) and keeps the word
		 * here, for the binder to type the call by as 1.x did. Category Named when the call said nothing.
		 */
		FTypeRef LegacyResultType;

		bool HasNamedArguments() const
		{
			for (const FArgument& Argument : Arguments)
			{
				if (!Argument.Name.IsEmpty())
				{
					return true;
				}
			}
			return false;
		}
	};

	enum class EUnaryOp : uint8
	{
		Plus,
		Negate,
		LogicalNot,
		BitwiseNot,
		PreIncrement,
		PreDecrement,
		PostIncrement,
		PostDecrement,
	};

	struct FUnaryExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::UnaryExpr;
		FUnaryExpr() : FExpr(StaticKind) {}

		EUnaryOp Op = EUnaryOp::Plus;
		FExprPtr Operand;
	};

	enum class EBinaryOp : uint8
	{
		Multiply, Divide, Modulo,
		Add, Subtract,
		ShiftLeft, ShiftRight,
		Less, LessEqual, Greater, GreaterEqual,
		Equal, NotEqual,
		BitwiseAnd, BitwiseXor, BitwiseOr,
		LogicalAnd, LogicalOr,
	};

	struct FBinaryExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::BinaryExpr;
		FBinaryExpr() : FExpr(StaticKind) {}

		EBinaryOp Op = EBinaryOp::Add;
		FExprPtr Left;
		FExprPtr Right;
	};

	enum class EAssignOp : uint8
	{
		Assign,
		AddAssign, SubtractAssign, MultiplyAssign, DivideAssign, ModuloAssign,
		AndAssign, OrAssign, XorAssign, ShiftLeftAssign, ShiftRightAssign,
	};

	struct FAssignExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::AssignExpr;
		FAssignExpr() : FExpr(StaticKind) {}

		EAssignOp Op = EAssignOp::Assign;
		FExprPtr Target;
		FExprPtr Value;
	};

	struct FConditionalExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::ConditionalExpr;
		FConditionalExpr() : FExpr(StaticKind) {}

		FExprPtr Condition;
		FExprPtr TrueValue;
		FExprPtr FalseValue;
	};

	/** `(Type)operand`. */
	struct FCastExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::CastExpr;
		FCastExpr() : FExpr(StaticKind) {}

		FTypeRef Type;
		FExprPtr Operand;
	};

	/** `{ a, b, c }` -- only ever an initializer. */
	struct FInitializerListExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::InitializerListExpr;
		FInitializerListExpr() : FExpr(StaticKind) {}

		TArray<FExprPtr> Elements;
	};

	/** Parentheses the author wrote. Kept so the printer reproduces them; the semantic pass sees through. */
	struct FParenExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::ParenExpr;
		FParenExpr() : FExpr(StaticKind) {}

		FExprPtr Inner;
	};

	// ------------------------------------------------------------------------------------------
	// Statements
	// ------------------------------------------------------------------------------------------

	struct FStmt : FNode
	{
		using FNode::FNode;
	};

	using FStmtPtr = TUniquePtr<FStmt>;

	/** One name in a declaration: `Name[dims] = init`. */
	struct FDeclarator
	{
		FString Name;
		FLangSpan NameSpan;
		/** Each `[expr]`; an unsized `[]` is a null entry. */
		TArray<FExprPtr> ArrayDimensions;
		FExprPtr Initializer;
		FLangSpan Span;
	};

	enum class EStorageClass : uint8
	{
		None,
		/** `uniform` -- a material parameter (2.0) / a `Properties` entry (1.x). */
		Uniform,
		/** `static const` -- a compile-time constant (2.0) / a `const` property (1.x). */
		StaticConst,
		/** `static` alone. */
		Static,
		/** `const` alone. */
		Const,
	};

	/** `[static] [const] Type a = 1, b;` as a statement. */
	struct FVarDeclStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::VarDeclStmt;
		FVarDeclStmt() : FStmt(StaticKind) {}

		EStorageClass Storage = EStorageClass::None;
		FTypeRef Type;
		TArray<FDeclarator> Declarators;
	};

	struct FExprStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::ExprStmt;
		FExprStmt() : FStmt(StaticKind) {}

		FExprPtr Expression;
	};

	struct FBlockStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::BlockStmt;
		FBlockStmt() : FStmt(StaticKind) {}

		TArray<FStmtPtr> Statements;
	};

	struct FIfStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::IfStmt;
		FIfStmt() : FStmt(StaticKind) {}

		FExprPtr Condition;
		FStmtPtr Then;
		/** Null when there is no else. An `else if` is an FIfStmt here. */
		FStmtPtr Else;
	};

	struct FForStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::ForStmt;
		FForStmt() : FStmt(StaticKind) {}

		/** A FVarDeclStmt or FExprStmt, or null. */
		FStmtPtr Init;
		FExprPtr Condition;
		FExprPtr Step;
		FStmtPtr Body;
	};

	struct FWhileStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::WhileStmt;
		FWhileStmt() : FStmt(StaticKind) {}

		FExprPtr Condition;
		FStmtPtr Body;
	};

	struct FDoWhileStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::DoWhileStmt;
		FDoWhileStmt() : FStmt(StaticKind) {}

		FStmtPtr Body;
		FExprPtr Condition;
	};

	struct FReturnStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::ReturnStmt;
		FReturnStmt() : FStmt(StaticKind) {}

		/** Null for a bare `return;`. */
		FExprPtr Value;
	};

	struct FBreakStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::BreakStmt;
		FBreakStmt() : FStmt(StaticKind) {}
	};

	struct FContinueStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::ContinueStmt;
		FContinueStmt() : FStmt(StaticKind) {}
	};

	struct FDiscardStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::DiscardStmt;
		FDiscardStmt() : FStmt(StaticKind) {}
	};

	/** A lone `;`. */
	struct FEmptyStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::EmptyStmt;
		FEmptyStmt() : FStmt(StaticKind) {}
	};

	enum class EPragmaKind : uint8
	{
		/** `#pragma material(Key = Value, ...)` -- file-level material settings. */
		Material,
		/** `#pragma layout(Node|Comment, Key = Value, ...)` -- decompiler-written node coordinates. */
		Layout,
		/** `#pragma region Name`. */
		Region,
		/** `#pragma endregion`. */
		EndRegion,
		/** `#pragma instance(Parent = "...", Key = Value, ...)` -- `.dsi` only; arguments parsed like `material`. */
		Instance,
		/** Any other `#pragma`; kept for the printer, ignored by everything else. */
		Unknown,
	};

	/**
	 * `#pragma region Title` / `#pragma endregion` inside a function body: the one `#` line a body
	 * may hold. It draws a comment box around the nodes the statements between the pair produce,
	 * as the 1.x `#Region` did. Every other `#` line in a body is DSH2160. PragmaKind is Region or
	 * EndRegion, never anything else.
	 */
	struct FPragmaStmt final : FStmt
	{
		static constexpr ENodeKind StaticKind = ENodeKind::PragmaStmt;
		FPragmaStmt() : FStmt(StaticKind) {}

		EPragmaKind PragmaKind = EPragmaKind::Region;
		/** The title after `region`; empty for `endregion`. */
		FString Text;
	};

	// ------------------------------------------------------------------------------------------
	// Declarations
	// ------------------------------------------------------------------------------------------

	struct FDecl : FNode
	{
		using FNode::FNode;

		/** The `///` block above the declaration. Empty when there was none. */
		FDocBlock Doc;
		/**
		 * Parsed by the legacy (1.x) front end: a `.dsm`/`.dsf` declaration, or a `Function` /
		 * `GraphFunction` / `Namespace` / `VirtualFunction` block in a `.dsh`. The binder and the IR builder
		 * apply the documented 1.x rules to it and to everything in its body (Plan/m4m5/research-legacy.md section 3.7).
		 */
		bool bLegacy = false;
	};

	using FDeclPtr = TUniquePtr<FDecl>;

	/** `uniform float Intensity = 2.0;`, `static const float K = 1.0;` at file scope. */
	struct FVariableDecl final : FDecl
	{
		static constexpr ENodeKind StaticKind = ENodeKind::VariableDecl;
		FVariableDecl() : FDecl(StaticKind) {}

		EStorageClass Storage = EStorageClass::None;
		FTypeRef Type;
		/** File-scope declarations are one name each; a `uniform float a, b;` still yields one node per name. */
		FDeclarator Declarator;
		/** This name followed a comma in the previous FVariableDecl's declaration: `uniform float a, b;`. The printer joins such a run. */
		bool bSharesDeclarationWithPrevious = false;
	};

	enum class EParamDirection : uint8
	{
		In,
		Out,
		InOut,
	};

	struct FParam
	{
		EParamDirection Direction = EParamDirection::In;
		FTypeRef Type;
		FString Name;
		FLangSpan NameSpan;
		TArray<FExprPtr> ArrayDimensions;
		/** `= expr` -- an optional input in 2.0 (1.x `opt`). */
		FExprPtr Default;
		FLangSpan Span;
		/** 1.x `opt` written without a default: an optional input with no preview value. A default implies it. */
		bool bOptional = false;
	};

	enum class EFunctionLinkage : uint8
	{
		/** No keyword: a file-local helper, inlined into the graph by default. */
		Internal,
		/** `export`: produces an asset -- a UMaterial when the signature is `void (inout material)`, otherwise a UMaterialFunction. */
		Export,
		/** `extern`: a prototype bound to an existing asset through `/// @asset`. Body is null. */
		Extern,
	};

	/**
	 * A `UE.*` / `Substrate.*` call written inside an opaque `/// @custom` body, lifted into an input pin of the
	 * Custom node (plan section 11 #19; the 1.x GraphFunction rule). RawBody keeps the call's text; the code builder
	 * replaces [RawBodyOffset, RawBodyOffset + RawBodyLength) with InputName. Call is bound once, in its function's own
	 * scope; the IR builder lowers it at every call site with the parameters bound to that call's arguments.
	 */
	struct FHoistedCall
	{
		FString InputName;
		FExprPtr Call;
		int32 RawBodyOffset = 0;
		int32 RawBodyLength = 0;
		/** The call in the source file; the line is exact, the column approximate after 1.x body normalisation. */
		FLangSpan Span;
	};

	struct FFunctionDecl final : FDecl
	{
		static constexpr ENodeKind StaticKind = ENodeKind::FunctionDecl;
		FFunctionDecl() : FDecl(StaticKind) {}

		EFunctionLinkage Linkage = EFunctionLinkage::Internal;
		FTypeRef ReturnType;
		FString Name;
		FLangSpan NameSpan;
		TArray<FParam> Params;

		/** Parsed body. Null for an extern prototype, and null when the body is opaque. */
		TUniquePtr<FBlockStmt> Body;

		/**
		 * True when the body was NOT parsed as DreamShaderLang statements but captured verbatim --
		 * a `/// @custom` function, whose body is HLSL handed straight to the shader compiler, or a
		 * 1.x `Function` block. RawBody is the text between the braces (exclusive), BodySpan covers
		 * the braces inclusive.
		 */
		bool bOpaqueBody = false;
		FString RawBody;
		FLangSpan BodySpan;
		/** The `UE.*` / `Substrate.*` calls lifted out of RawBody into Custom node inputs (FHoistedCall), in body order. Empty otherwise. */
		TArray<FHoistedCall> HoistedCalls;
		/**
		 * A 1.x function declared inside `Namespace(Name="N")`: `N::F`, the name 1.x knew it by and wrote on its Custom node.
		 * Name is the flattened `N_F`. Empty for every other function.
		 */
		FString LegacyQualifiedName;

		bool IsPrototype() const { return Linkage == EFunctionLinkage::Extern || (!Body && !bOpaqueBody); }
		/** `void Name(inout material m)` -- the material entry signature. */
		bool IsMaterialEntry() const
		{
			return ReturnType.IsVoid()
				&& Params.Num() == 1
				&& Params[0].Direction == EParamDirection::InOut
				&& Params[0].Type.IsMaterial();
		}
	};

	struct FStructField
	{
		FDocBlock Doc;
		FTypeRef Type;
		FString Name;
		FLangSpan NameSpan;
		TArray<FExprPtr> ArrayDimensions;
		FLangSpan Span;
	};

	struct FStructDecl final : FDecl
	{
		static constexpr ENodeKind StaticKind = ENodeKind::StructDecl;
		FStructDecl() : FDecl(StaticKind) {}

		FString Name;
		FLangSpan NameSpan;
		TArray<FStructField> Fields;
	};

	/** `#include "path"` or `import "path";` -- same meaning, the spelling is kept for the printer. */
	struct FIncludeDecl final : FDecl
	{
		static constexpr ENodeKind StaticKind = ENodeKind::IncludeDecl;
		FIncludeDecl() : FDecl(StaticKind) {}

		/** As written: `/Game/Shared/Common.dsh`, `@scope/pkg/Library/Noise.dsh`, `Hash.dsh`. Unresolved. */
		FString Path;
		FLangSpan PathSpan;
		bool bImportSpelling = false;
	};

	/** One `Key = Value` of a pragma. Value is the raw spelling; a quoted string has its quotes removed and bQuoted set. */
	struct FPragmaArgument
	{
		/** Empty for a positional argument (the `Node` / `Comment` selector of `#pragma layout`). */
		FString Key;
		FString Value;
		bool bQuoted = false;
		FLangSpan Span;
	};

	struct FPragmaDecl final : FDecl
	{
		static constexpr ENodeKind StaticKind = ENodeKind::PragmaDecl;
		FPragmaDecl() : FDecl(StaticKind) {}

		EPragmaKind PragmaKind = EPragmaKind::Unknown;
		/** The pragma name as written (`material`, `layout`, `region`, ...). */
		FString Name;
		/** Region: the title. Unknown: the rest of the line. */
		FString Text;
		TArray<FPragmaArgument> Arguments;

		const FPragmaArgument* Find(const TCHAR* Key) const
		{
			for (const FPragmaArgument& Argument : Arguments)
			{
				if (Argument.Key.Equals(Key, ESearchCase::IgnoreCase))
				{
					return &Argument;
				}
			}
			return nullptr;
		}
	};

	// ------------------------------------------------------------------------------------------
	// Module
	// ------------------------------------------------------------------------------------------

	/** One `//` or block comment kept as trivia. */
	struct FLangComment
	{
		/** Verbatim, delimiters included. */
		FString Text;
		FLangSpan Span;
		bool bBlock = false;
	};

	/**
	 * The comments and blank lines around one node, kept only when the parse asked for them
	 * (FLangParseOptions::bKeepTrivia). A side table on FModule rather than fields on FNode: nodes are
	 * address-stable, and no node kind changes layout.
	 */
	struct FLangTrivia
	{
		/** Own-line comments above the node, in source order. */
		TArray<FLangComment> Leading;
		/**
		 * Blank lines above the node's first Leading comment (the node itself when it has none), counted from the previous
		 * sibling's last line or the enclosing block's `{`; the printer keeps at most one. A blank line between the Leading
		 * comments and the node follows from their spans.
		 */
		int32 BlankLinesBefore = 0;
		/** A comment that starts on the node's last line, after it. */
		TOptional<FLangComment> Trailing;
		/** Blocks only: comments after the last statement, before the closing brace. */
		TArray<FLangComment> Inner;
	};

	/** One parsed source file. Declarations are in source order, pragmas and includes among them. */
	struct FModule
	{
		FString FilePath;
		ELangFileKind FileKind = ELangFileKind::Unknown;
		TArray<FDeclPtr> Declarations;
		/** Comments and blank lines by node; empty unless the parse kept trivia. Keys point into this module's own tree. */
		TMap<const FNode*, FLangTrivia> Trivia;
		/** Comments after the last declaration. */
		TArray<FLangComment> TrailingComments;

		FModule() = default;
		FModule(const FModule&) = delete;
		FModule& operator=(const FModule&) = delete;

		template <typename TFunc>
		void ForEachDecl(ENodeKind Kind, TFunc Func) const
		{
			for (const FDeclPtr& Decl : Declarations)
			{
				if (Decl && Decl->Kind == Kind)
				{
					Func(*Decl);
				}
			}
		}
		int32 CountDecls(ENodeKind Kind) const
		{
			int32 Count = 0;
			ForEachDecl(Kind, [&Count](const FDecl&) { ++Count; });
			return Count;
		}
	};
}
