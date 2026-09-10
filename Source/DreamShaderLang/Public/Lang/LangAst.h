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

	/**
	 * The `///` lines immediately preceding a declaration (or a struct field). Text that is not a
	 * directive is collected into FreeText, one line per source line, and is what `@desc` falls
	 * back to when it is absent.
	 */
	struct FDocBlock
	{
		TArray<FDocDirective> Directives;
		TArray<FString> FreeText;
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
	};

	/** `Callee(args)`. Callee is an identifier, a member chain, or a FTypeExpr for a constructor. */
	struct FCallExpr final : FExpr
	{
		static constexpr ENodeKind StaticKind = ENodeKind::CallExpr;
		FCallExpr() : FExpr(StaticKind) {}

		FExprPtr Callee;
		TArray<FArgument> Arguments;

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

	// ------------------------------------------------------------------------------------------
	// Declarations
	// ------------------------------------------------------------------------------------------

	struct FDecl : FNode
	{
		using FNode::FNode;

		/** The `///` block above the declaration. Empty when there was none. */
		FDocBlock Doc;
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
		/** Any other `#pragma`; kept for the printer, ignored by everything else. */
		Unknown,
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

	/** One parsed source file. Declarations are in source order, pragmas and includes among them. */
	struct FModule
	{
		FString FilePath;
		ELangFileKind FileKind = ELangFileKind::Unknown;
		TArray<FDeclPtr> Declarations;

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
