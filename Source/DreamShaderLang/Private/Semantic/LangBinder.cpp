// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The binder's spine: the entry point, the include graph, the declare pass, function
// classification, the products, the call graph -- plus the out-of-line members that
// Public/Semantic/LangBound.h declares.
//
// The declare pass runs before anything is typed, because a body may call a function declared
// further down the file and a struct may be used before the line that mentions it is reached.
// It walks the root module's declarations in source order and steps sideways into an `#include`
// at the point the include sits, so a header's names are visible to everything under it and to
// nothing above -- the C rule, which is the one every author already has in their head.

#include "LangBinderInternal.h"

#include "IR/IR.h"
#include "IR/IRCatalog.h"
#include "IR/IRCoreOps.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Binder"

namespace UE::DreamShader::Lang
{
	// ---------------------------------------------------------------------------------------------
	// LangBound.h: the members that need a translation unit
	// ---------------------------------------------------------------------------------------------

	const TCHAR* LexToString(EBoundFunctionKind Kind)
	{
		switch (Kind)
		{
		case EBoundFunctionKind::Helper:         return TEXT("Helper");
		case EBoundFunctionKind::Custom:         return TEXT("Custom");
		case EBoundFunctionKind::Entry:          return TEXT("Entry");
		case EBoundFunctionKind::Layer:          return TEXT("Layer");
		case EBoundFunctionKind::LayerBlend:     return TEXT("LayerBlend");
		case EBoundFunctionKind::ExportFunction: return TEXT("ExportFunction");
		case EBoundFunctionKind::Extern:         return TEXT("Extern");
		}
		return TEXT("Unknown");
	}

	const TCHAR* LexToString(EBoundExprKind Kind)
	{
		switch (Kind)
		{
		case EBoundExprKind::Error:             return TEXT("Error");
		case EBoundExprKind::Literal:           return TEXT("Literal");
		case EBoundExprKind::Local:             return TEXT("Local");
		case EBoundExprKind::Global:            return TEXT("Global");
		case EBoundExprKind::Param:             return TEXT("Param");
		case EBoundExprKind::StructField:       return TEXT("StructField");
		case EBoundExprKind::MaterialField:     return TEXT("MaterialField");
		case EBoundExprKind::Swizzle:           return TEXT("Swizzle");
		case EBoundExprKind::NodeOutput:        return TEXT("NodeOutput");
		case EBoundExprKind::CoreOp:            return TEXT("CoreOp");
		case EBoundExprKind::Constructor:       return TEXT("Constructor");
		case EBoundExprKind::Cast:              return TEXT("Cast");
		case EBoundExprKind::ReflectedCall:     return TEXT("ReflectedCall");
		case EBoundExprKind::FunctionCall:      return TEXT("FunctionCall");
		case EBoundExprKind::TextureSample:     return TEXT("TextureSample");
		case EBoundExprKind::IndexConst:        return TEXT("IndexConst");
		case EBoundExprKind::Conditional:       return TEXT("Conditional");
		case EBoundExprKind::Assign:            return TEXT("Assign");
		case EBoundExprKind::InitializerList:   return TEXT("InitializerList");
		case EBoundExprKind::Paren:             return TEXT("Paren");
		case EBoundExprKind::StructConstructor: return TEXT("StructConstructor");
		}
		return TEXT("Unknown");
	}

	const FString* FBoundDirectives::FindParamDoc(const FString& ParamName) const
	{
		for (const TPair<FString, FString>& Doc : ParamDocs)
		{
			if (Doc.Key.Equals(ParamName, ESearchCase::CaseSensitive))
			{
				return &Doc.Value;
			}
		}
		return nullptr;
	}

	int32 FBoundStruct::FindField(const FString& InName) const
	{
		for (int32 Index = 0; Index < Fields.Num(); ++Index)
		{
			if (Fields[Index].Name.Equals(InName, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	const FBoundExpr& FBoundModule::Get(const FExpr& Expr) const
	{
		// An unbound expression is a binder gap, not an author mistake. Rather than crash a tool
		// that walks a partially bound module, hand back a shared Error binding: every consumer
		// already has to tolerate Error, because that is what a reported mistake looks like.
		static const FBoundExpr Missing;
		if (const FBoundExpr* Found = Expressions.Find(&Expr))
		{
			return *Found;
		}
		return Missing;
	}

	int32 FBoundModule::FindStruct(const FString& Name) const
	{
		for (int32 Index = 0; Index < Structs.Num(); ++Index)
		{
			if (Structs[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FBoundModule::FindGlobal(const FString& Name) const
	{
		for (int32 Index = 0; Index < Globals.Num(); ++Index)
		{
			if (Globals[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FBoundModule::FindFunction(const FString& Name) const
	{
		for (int32 Index = 0; Index < Functions.Num(); ++Index)
		{
			if (Functions[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FBoundModule::FindEntryFunction() const
	{
		for (int32 Index = 0; Index < Functions.Num(); ++Index)
		{
			if (Functions[Index].Kind == EBoundFunctionKind::Entry)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

namespace UE::DreamShader::Lang::Private
{
	// ---------------------------------------------------------------------------------------------
	// Shared helpers
	// ---------------------------------------------------------------------------------------------

	IR::EIRTypeKind PromoteNumericKind(IR::EIRTypeKind A, IR::EIRTypeKind B)
	{
		// HLSL's usual arithmetic conversions, narrowed to the kinds this language has. The graph
		// makes every one of these a float in the end; the honest kind is kept until then so a
		// `/// @custom` body gets the signature its author wrote and `int / int` reads as it should.
		auto Rank = [](IR::EIRTypeKind Kind)
		{
			switch (Kind)
			{
			case IR::EIRTypeKind::Bool:   return 0;
			case IR::EIRTypeKind::Int:    return 1;
			case IR::EIRTypeKind::UInt:   return 2;
			case IR::EIRTypeKind::Half:   return 3;
			case IR::EIRTypeKind::Float:  return 4;
			case IR::EIRTypeKind::Double: return 5;
			default:                      return -1;
			}
		};

		const int32 RankA = Rank(A);
		const int32 RankB = Rank(B);
		if (RankA < 0)
		{
			return B;
		}
		if (RankB < 0)
		{
			return A;
		}
		return RankA >= RankB ? A : B;
	}

	IR::FIRType MakeNumeric(IR::EIRTypeKind Kind, int32 Components)
	{
		return Components <= 1 ? IR::FIRType::Scalar(Kind) : IR::FIRType::Vector(Kind, Components);
	}

	bool IsConditionType(const IR::FIRType& Type)
	{
		if (Type.IsError())
		{
			return true;
		}
		// A vector condition would need a per-component branch, which the graph has no shape for.
		return (Type.IsBool() || Type.IsNumeric()) && Type.Cols == 1 && Type.Rows == 1;
	}

	// ---------------------------------------------------------------------------------------------
	// Construction and the run order
	// ---------------------------------------------------------------------------------------------

	namespace
	{
		const IR::FBuiltinCatalog& EmptyCatalog()
		{
			static const IR::FBuiltinCatalog Empty;
			return Empty;
		}
	}

	FLangBinder::FLangBinder(const FModule& InModule, const FBindOptions& InOptions, FBoundModule& InBound, FLangDiagnosticSink& InDiagnostics)
		: RootModule(InModule)
		, Options(InOptions)
		, Catalog(InOptions.Catalog ? *InOptions.Catalog : EmptyCatalog())
		, Bound(InBound)
		, Diagnostics(InDiagnostics)
	{
		CurrentFile = InModule.FilePath;
	}

	void FLangBinder::Run()
	{
		Bound.Module = &RootModule;
		// CONTRACT §6.13 #18: every catalog index the binder records -- FIRType::CatalogIndex,
		// FBoundExpr::Index on a ReflectedCall, FieldIndex on a MaterialField -- is an index into
		// THIS catalog, so the bound module carries it rather than leaving the reader to guess.
		Bound.Catalog = Options.Catalog;

		if (Catalog.IsEmpty())
		{
			// Not an error: `dsc index` on a file the editor has never opened still wants a symbol
			// index, and a bound module with errors is exactly what a language service asks for.
			Diagnostics.Warning(
				TEXT("DSH5202"),
				FLangSpan(),
				LOCTEXT("EmptyCatalog", "The builtin catalog is empty, so no 'UE.' expression and no material attribute can be resolved; export it with 'dsc export-catalog'."));
		}

		IncludeStack.Add(RootModule.FilePath);
		DeclareModule(RootModule);
		IncludeStack.Pop();

		ResolveBackend();
		ClassifyFunctions();
		BuildProducts();

		// Globals first: a `static const` has to be folded before a body can read it as a constant,
		// and before a `for` bounded by one can be proved.
		BindGlobals();

		CallGraph.SetNum(Bound.Functions.Num());
		for (int32 Index = 0; Index < Bound.Functions.Num(); ++Index)
		{
			BindFunctionBody(Index);
		}

		DetectRecursion();
	}

	// ---------------------------------------------------------------------------------------------
	// Declare pass
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::DeclareModule(const FModule& InModule)
	{
		// Region nesting is per file: a `#pragma region` a header forgets to close must not swallow
		// the declarations that follow the `#include` in the file that pulled it in.
		TArray<int32> FileRegions;
		const FString File = InModule.FilePath;

		// Saved and restored, so the declarations AFTER an `#include` are attributed to the file
		// that wrote them and not to the header the recursion just came out of.
		const FString OuterFile = CurrentFile;
		CurrentFile = File;

		for (const FDeclPtr& DeclPtr : InModule.Declarations)
		{
			const FDecl* Decl = DeclPtr.Get();
			if (!Decl)
			{
				continue;
			}

			// A file-scope declaration remembers its box too, not just a statement: a `uniform`
			// written inside `#pragma region Parameters` puts its parameter node in that box, which
			// is how the 1.x `#Region` behaved and what a decompiled file round-trips through.
			Bound.StatementRegions.Add(Decl, FileRegions.Num() > 0 ? FileRegions.Last() : INDEX_NONE);

			switch (Decl->Kind)
			{
			case ENodeKind::IncludeDecl:
				ResolveInclude(*static_cast<const FIncludeDecl*>(Decl), InModule);
				break;

			case ENodeKind::PragmaDecl:
			{
				const FPragmaDecl& Pragma = *static_cast<const FPragmaDecl*>(Decl);
				switch (Pragma.PragmaKind)
				{
				case EPragmaKind::Material:  BindMaterialPragma(Pragma); break;
				case EPragmaKind::Layout:    BindLayoutPragma(Pragma); break;
				case EPragmaKind::Region:    OpenRegion(Pragma.Text, Pragma.Span, FileRegions); break;
				case EPragmaKind::EndRegion: CloseRegion(Pragma.Span, FileRegions); break;
				case EPragmaKind::Unknown:   break;
				}
				break;
			}

			case ENodeKind::StructDecl:
				DeclareStruct(*static_cast<const FStructDecl*>(Decl), File);
				break;

			case ENodeKind::VariableDecl:
				DeclareGlobal(*static_cast<const FVariableDecl*>(Decl), File);
				break;

			case ENodeKind::FunctionDecl:
				DeclareFunction(*static_cast<const FFunctionDecl*>(Decl), File);
				break;

			default:
				break;
			}
		}

		CloseDanglingRegions(FileRegions);
		CurrentFile = OuterFile;
	}

	void FLangBinder::ResolveInclude(const FIncludeDecl& Decl, const FModule& From)
	{
		if (!Options.IncludeResolver)
		{
			Diagnostics.Error(
				TEXT("DSH4212"),
				CurrentFile,
				Decl.PathSpan,
				FText::Format(
					LOCTEXT("IncludesUnavailable", "'{0}' cannot be read: this front end was started without an include resolver, so nothing a header declares is visible."),
					FText::FromString(Decl.Path)));
			return;
		}

		// One resolver call per (path as written, including file): the same spelling means the same
		// file from the same place, and two files may spell one relative path differently.
		for (const TPair<FString, FString>& Requested : RequestedIncludes)
		{
			if (Requested.Key.Equals(Decl.Path, ESearchCase::CaseSensitive)
				&& Requested.Value.Equals(From.FilePath, ESearchCase::IgnoreCase))
			{
				return;
			}
		}
		RequestedIncludes.Emplace(Decl.Path, From.FilePath);

		const FModule* Included = Options.IncludeResolver(Decl.Path, From.FilePath, Diagnostics);
		if (!Included)
		{
			// The resolver reported; it owns that message because it knows the roots it searched.
			return;
		}

		// File paths are compared case-insensitively -- they are paths, not identifiers, and the one
		// platform this ships on does not distinguish them.
		const FString Resolved = Included->FilePath;
		for (const FString& Active : IncludeStack)
		{
			if (Active.Equals(Resolved, ESearchCase::IgnoreCase))
			{
				Diagnostics.Error(
					TEXT("DSH4211"),
					CurrentFile,
					Decl.PathSpan,
					FText::Format(
						LOCTEXT("IncludeCycle", "Including '{0}' from '{1}' closes a cycle; a header may not include itself, directly or through another header."),
						FText::FromString(Resolved),
						FText::FromString(From.FilePath)));
				return;
			}
		}

		for (const FString& Visited : VisitedIncludes)
		{
			if (Visited.Equals(Resolved, ESearchCase::IgnoreCase))
			{
				// A diamond: already declared once, and declaring it twice would be DSH4210 on every
				// name in it.
				return;
			}
		}

		VisitedIncludes.Add(Resolved);
		Bound.Included.Add(Included);
		Bound.IncludePaths.Add(Resolved);

		IncludeStack.Add(Resolved);
		DeclareModule(*Included);
		IncludeStack.Pop();
	}

	bool FLangBinder::CheckNameAvailable(const FString& Name, const FLangSpan& Span, const FString& File)
	{
		auto Clash = [this, &Name, &Span, &File](const FString& OtherFile) -> bool
		{
			if (OtherFile.Equals(File, ESearchCase::IgnoreCase))
			{
				Diagnostics.Error(
					TEXT("DSH4210"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("RedefinitionSameFile", "'{0}' is already declared in this file; one name declares one thing."),
						FText::FromString(Name)));
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH4210"),
					CurrentFile,
					Span,
					FText::Format(
						LOCTEXT("RedefinitionAcrossFiles", "'{0}' is declared in '{1}' and again in '{2}'; a name included from a header may not be declared a second time."),
						FText::FromString(Name),
						FText::FromString(OtherFile),
						FText::FromString(File)));
			}
			return false;
		};

		for (int32 Index = 0; Index < Bound.Structs.Num(); ++Index)
		{
			if (Bound.Structs[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				// A struct carries its own constructor spelling, so it shares the one namespace with
				// globals and functions rather than living beside them as it would in C.
				return Clash(Bound.Structs[Index].File);
			}
		}
		for (const FBoundGlobal& Global : Bound.Globals)
		{
			if (Global.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Clash(Global.File);
			}
		}
		for (const FBoundFunction& Function : Bound.Functions)
		{
			if (Function.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Clash(Function.File);
			}
		}
		return true;
	}

	bool FLangBinder::ResolveTypeRef(const FTypeRef& Ref, IR::FIRType& OutType)
	{
		if (Ref.Category == ETypeCategory::Named)
		{
			const int32 StructIndex = FindStruct(Ref.Name);
			if (StructIndex != INDEX_NONE)
			{
				OutType = IR::FIRType::Struct(StructIndex);
				return true;
			}

			OutType = IR::FIRType::Error();

			TArray<FString> Names;
			Names.Reserve(Bound.Structs.Num());
			for (const FBoundStruct& Struct : Bound.Structs)
			{
				Names.Add(Struct.Name);
			}
			const FString Suggestion = SuggestCaseInsensitive(Ref.Name, Names);
			if (!Suggestion.IsEmpty())
			{
				Diagnostics.Error(
					TEXT("DSH4201"),
					CurrentFile,
					Ref.Span,
					FText::Format(
						LOCTEXT("UnknownTypeDidYouMean", "'{0}' is not a type; did you mean '{1}'? Type names are case-sensitive."),
						FText::FromString(Ref.Name),
						FText::FromString(Suggestion)));
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH4201"),
					CurrentFile,
					Ref.Span,
					FText::Format(
						LOCTEXT("UnknownType", "'{0}' is not a builtin type and no 'struct' of that name is declared before this line."),
						FText::FromString(Ref.Name)));
			}
			return false;
		}

		OutType = IR::TypeFromBuiltinRef(Ref);
		if (OutType.IsError())
		{
			Diagnostics.Error(
				TEXT("DSH4201"),
				CurrentFile,
				Ref.Span,
				FText::Format(
					LOCTEXT("UnsupportedType", "'{0}' is not a type this front end can resolve."),
					FText::FromString(Ref.Name)));
			return false;
		}
		return true;
	}

	bool FLangBinder::ResolveArrayCount(const TArray<FExprPtr>& Dimensions, const FExpr* Initializer, const FLangSpan& Span, int32& OutCount)
	{
		OutCount = 0;
		if (Dimensions.Num() == 0)
		{
			return true;
		}

		if (Dimensions.Num() > 1)
		{
			Diagnostics.Error(
				TEXT("DSH4241"),
				CurrentFile,
				Span,
				LOCTEXT("MultiDimensionalArray", "A multi-dimensional array has no graph form; declare one dimension, or move the code into a '/// @custom' function."));
			return false;
		}

		const FExpr* Size = Dimensions[0].Get();
		if (!Size)
		{
			// An unsized `[]` takes its length from the initializer, counted syntactically: the
			// declare pass runs before anything is bound.
			if (const FInitializerListExpr* List = Initializer ? Initializer->As<FInitializerListExpr>() : nullptr)
			{
				OutCount = List->Elements.Num();
				return true;
			}

			Diagnostics.Error(
				TEXT("DSH4245"),
				CurrentFile,
				Span,
				LOCTEXT("UnsizedArrayNeedsInitializer", "An unsized array needs an initializer list to take its length from."));
			return false;
		}

		// The size is read syntactically, for the same reason: an integer literal, optionally in
		// parentheses. Anything else would need the whole file bound first.
		const FExpr* Peeled = Size;
		while (const FParenExpr* Paren = Peeled->As<FParenExpr>())
		{
			Peeled = Paren->Inner.Get();
			if (!Peeled)
			{
				break;
			}
		}

		const FLiteralExpr* Literal = Peeled ? Peeled->As<FLiteralExpr>() : nullptr;
		if (!Literal || (Literal->LiteralKind != ELiteralKind::Int && Literal->LiteralKind != ELiteralKind::UInt))
		{
			Diagnostics.Error(
				TEXT("DSH4245"),
				CurrentFile,
				Size->Span,
				LOCTEXT("ArraySizeNotLiteral", "An array size must be an integer literal."));
			return false;
		}

		if (Literal->Integer == 0 || Literal->Integer > 4096)
		{
			Diagnostics.Error(
				TEXT("DSH4245"),
				CurrentFile,
				Size->Span,
				LOCTEXT("ArraySizeOutOfRange", "An array size must be between 1 and 4096."));
			return false;
		}

		OutCount = static_cast<int32>(Literal->Integer);
		return true;
	}

	void FLangBinder::DeclareStruct(const FStructDecl& Decl, const FString& File)
	{
		if (!CheckNameAvailable(Decl.Name, Decl.NameSpan, File))
		{
			return;
		}

		FBoundStruct Struct;
		Struct.Name = Decl.Name;
		Struct.Decl = &Decl;
		Struct.File = File;
		Struct.Fields.Reserve(Decl.Fields.Num());

		for (const FStructField& Field : Decl.Fields)
		{
			if (Struct.FindField(Field.Name) != INDEX_NONE)
			{
				Diagnostics.Error(
					TEXT("DSH4213"),
					CurrentFile,
					Field.NameSpan,
					FText::Format(
						LOCTEXT("DuplicateStructField", "'{0}' is declared twice in struct '{1}'."),
						FText::FromString(Field.Name),
						FText::FromString(Decl.Name)));
				continue;
			}

			FBoundStructField Bound_;
			Bound_.Name = Field.Name;
			if (!ResolveTypeRef(Field.Type, Bound_.Type))
			{
				Bound_.Type = IR::FIRType::Error();
			}
			else if (Bound_.Type.IsVoid())
			{
				Diagnostics.Error(
					TEXT("DSH4201"),
					CurrentFile,
					Field.Type.Span,
					LOCTEXT("VoidStructField", "A struct field cannot be 'void'."));
				Bound_.Type = IR::FIRType::Error();
			}

			ResolveArrayCount(Field.ArrayDimensions, nullptr, Field.Span, Bound_.ArrayCount);
			Struct.Fields.Add(MoveTemp(Bound_));
		}

		Bound.Structs.Add(MoveTemp(Struct));
	}

	void FLangBinder::DeclareGlobal(const FVariableDecl& Decl, const FString& File)
	{
		const FDeclarator& Declarator = Decl.Declarator;
		if (!CheckNameAvailable(Declarator.Name, Declarator.NameSpan, File))
		{
			return;
		}

		FBoundGlobal Global;
		Global.Name = Declarator.Name;
		Global.Decl = &Decl;
		Global.Storage = Decl.Storage;
		Global.File = File;
		if (!ResolveTypeRef(Decl.Type, Global.Type))
		{
			Global.Type = IR::FIRType::Error();
		}

		switch (Decl.Storage)
		{
		case EStorageClass::Uniform:
			Global.bIsParameter = true;
			break;

		case EStorageClass::StaticConst:
		case EStorageClass::Const:
			Global.bIsConstant = true;
			break;

		default:
			// CONTRACT §6.1: the two storage classes a global may have are the two that have a node.
			Diagnostics.Error(
				TEXT("DSH7211"),
				CurrentFile,
				Declarator.NameSpan,
				FText::Format(
					LOCTEXT("GlobalStorage", "'{0}' is a file-scope variable with no storage class; write 'uniform' for a material parameter or 'static const' for a compile-time constant."),
					FText::FromString(Declarator.Name)));
			// Bound as a constant anyway, so the rest of the file still types.
			Global.bIsConstant = true;
			break;
		}

		Global.Directives = BindDirectives(
			Decl.Doc,
			Global.bIsParameter ? EDirectiveTarget::Uniform : EDirectiveTarget::Constant,
			&Decl.Type);

		if (!Global.Type.IsError())
		{
			const bool bHasGraphForm =
				Global.Type.IsNumeric()
				|| Global.Type.IsBool()
				|| Global.Type.IsTexture()
				|| Global.Type.Kind == IR::EIRTypeKind::SamplerState;

			if (!bHasGraphForm)
			{
				Diagnostics.Error(
					TEXT("DSH7212"),
					CurrentFile,
					Decl.Type.Span,
					FText::Format(
						LOCTEXT("GlobalTypeUnsupported", "A file-scope variable of type {0} has no node; a 'uniform' or 'static const' must be numeric, bool, a texture or a sampler."),
						DescribeType(Global.Type)));
			}
		}

		if (Global.bIsParameter && Global.Type.IsTexture() && Declarator.Initializer)
		{
			Diagnostics.Error(
				TEXT("DSH7213"),
				CurrentFile,
				Declarator.Initializer->Span,
				LOCTEXT("TextureUniformInitializer", "A texture uniform has no HLSL initializer; write its default asset as '/// @default /Game/...'."));
		}

		if (Global.bIsConstant && !Declarator.Initializer)
		{
			Diagnostics.Error(
				TEXT("DSH7214"),
				CurrentFile,
				Declarator.NameSpan,
				FText::Format(
					LOCTEXT("ConstantNeedsInitializer", "'{0}' is a compile-time constant and must be initialised where it is declared."),
					FText::FromString(Declarator.Name)));
		}

		if (Global.Directives.bStatic && !(Global.bIsParameter && Global.Type.IsBool() && Global.Type.Rows == 1))
		{
			Diagnostics.Error(
				TEXT("DSH7223"),
				CurrentFile,
				Decl.Doc.Span,
				LOCTEXT("StaticOnNonBoolUniform", "'@static' asks for a static switch and is only meaningful on a 'uniform bool'."));
		}

		ResolveArrayCount(Declarator.ArrayDimensions, Declarator.Initializer.Get(), Declarator.Span, Global.ArrayCount);

		if (Global.ArrayCount > 0 && Global.bIsParameter)
		{
			Diagnostics.Error(
				TEXT("DSH7216"),
				CurrentFile,
				Declarator.Span,
				LOCTEXT("UniformArray", "A 'uniform' array has no parameter node; declare one uniform per element, or make it 'static const'."));
		}

		Bound.Globals.Add(MoveTemp(Global));
		GlobalArrayValues.AddDefaulted();
	}

	void FLangBinder::DeclareFunction(const FFunctionDecl& Decl, const FString& File)
	{
		if (IR::FindCoreOpByHlslName(Decl.Name) != nullptr || IR::FindCoreOpByGlslAlias(Decl.Name) != nullptr)
		{
			Diagnostics.Error(
				TEXT("DSH6206"),
				CurrentFile,
				Decl.NameSpan,
				FText::Format(
					LOCTEXT("FunctionShadowsBuiltin", "'{0}' is a builtin operation and cannot be redeclared; rename the function."),
					FText::FromString(Decl.Name)));
			return;
		}

		if (!CheckNameAvailable(Decl.Name, Decl.NameSpan, File))
		{
			return;
		}

		FBoundFunction Function;
		Function.Name = Decl.Name;
		Function.Decl = &Decl;
		Function.Linkage = Decl.Linkage;
		Function.File = File;
		if (!ResolveTypeRef(Decl.ReturnType, Function.ReturnType))
		{
			Function.ReturnType = IR::FIRType::Error();
		}
		Function.Directives = BindDirectives(Decl.Doc, EDirectiveTarget::Function, nullptr);

		// BindDirectives only knows it is looking at a function. Three keys are read for one linkage
		// alone and ignored on every other: `@asset` binds an `extern` prototype to its asset, and
		// `@library` and `@name` describe the asset an `export` produces. Said here, where the linkage
		// is known, with the code and wording of the other misplaced directives.
		for (const FDocDirective& Entry : Decl.Doc.Directives)
		{
			const bool bAsset = Entry.Key.Equals(Directive::Asset, ESearchCase::CaseSensitive);
			const bool bExportOnly = Entry.Key.Equals(Directive::Library, ESearchCase::CaseSensitive)
				|| Entry.Key.Equals(Directive::Name, ESearchCase::CaseSensitive);
			const bool bMisplaced = (bAsset && Decl.Linkage != EFunctionLinkage::Extern)
				|| (bExportOnly && Decl.Linkage != EFunctionLinkage::Export);
			if (!bMisplaced)
			{
				continue;
			}

			Diagnostics.Warning(
				TEXT("DSH7224"),
				File,
				Entry.Span,
				FText::Format(
					LOCTEXT("DirectiveWrongLinkage", "'@{0}' means nothing here; it belongs on {1}."),
					FText::FromString(Entry.Key),
					bAsset
						? LOCTEXT("LinkageTargetExtern", "an 'extern' prototype")
						: LOCTEXT("LinkageTargetExport", "an exported function")));
		}

		Function.Params.Reserve(Decl.Params.Num());
		for (const FParam& Param : Decl.Params)
		{
			bool bDuplicate = false;
			for (const FBoundParam& Existing : Function.Params)
			{
				if (Existing.Name.Equals(Param.Name, ESearchCase::CaseSensitive))
				{
					bDuplicate = true;
					break;
				}
			}
			if (bDuplicate)
			{
				Diagnostics.Error(
					TEXT("DSH4214"),
					CurrentFile,
					Param.NameSpan,
					FText::Format(
						LOCTEXT("DuplicateParameter", "'{0}' is declared twice in the parameter list of '{1}'."),
						FText::FromString(Param.Name),
						FText::FromString(Decl.Name)));
				continue;
			}

			FBoundParam Bound_;
			Bound_.Name = Param.Name;
			Bound_.Direction = Param.Direction;
			Bound_.Default = Param.Default.Get();
			Bound_.bOptional = Param.Default != nullptr;
			if (!ResolveTypeRef(Param.Type, Bound_.Type))
			{
				Bound_.Type = IR::FIRType::Error();
			}
			else if (Bound_.Type.IsVoid())
			{
				Diagnostics.Error(
					TEXT("DSH4201"),
					CurrentFile,
					Param.Type.Span,
					LOCTEXT("VoidParameter", "A parameter cannot be 'void'."));
				Bound_.Type = IR::FIRType::Error();
			}

			ResolveArrayCount(Param.ArrayDimensions, nullptr, Param.Span, Bound_.ArrayCount);

			if (const FString* Doc = Function.Directives.FindParamDoc(Param.Name))
			{
				Bound_.Doc = *Doc;
			}

			Function.Params.Add(MoveTemp(Bound_));
		}

		// `@param` naming something that is not a parameter is a stale doc comment, which is the one
		// thing a doc comment must not silently become when it carries semantics elsewhere.
		for (const TPair<FString, FString>& Doc : Function.Directives.ParamDocs)
		{
			bool bFound = false;
			for (const FBoundParam& Param : Function.Params)
			{
				if (Param.Name.Equals(Doc.Key, ESearchCase::CaseSensitive))
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				Diagnostics.Warning(
					TEXT("DSH7225"),
					CurrentFile,
					Decl.Doc.Span,
					FText::Format(
						LOCTEXT("ParamDocUnknown", "'@param {0}' does not name a parameter of '{1}'."),
						FText::FromString(Doc.Key),
						FText::FromString(Decl.Name)));
			}
		}

		Bound.Functions.Add(MoveTemp(Function));
	}

	// ---------------------------------------------------------------------------------------------
	// Function kinds and products
	// ---------------------------------------------------------------------------------------------

	namespace
	{
		/** `export void L(inout material m)` -- what a Layer and the Entry both have to be. */
		bool HasMaterialEntrySignature(const FBoundFunction& Function)
		{
			return Function.ReturnType.IsVoid()
				&& Function.Params.Num() == 1
				&& Function.Params[0].Direction == EParamDirection::InOut
				&& Function.Params[0].Type.IsMaterial();
		}

		/** `export void B(material A, material B, ..., inout material R)`. */
		bool HasLayerBlendSignature(const FBoundFunction& Function)
		{
			if (!Function.ReturnType.IsVoid() || Function.Params.Num() < 2)
			{
				return false;
			}

			const FBoundParam& Result = Function.Params.Last();
			if (Result.Direction != EParamDirection::InOut || !Result.Type.IsMaterial())
			{
				return false;
			}

			int32 MaterialInputs = 0;
			for (int32 Index = 0; Index < Function.Params.Num() - 1; ++Index)
			{
				const FBoundParam& Param = Function.Params[Index];
				if (Param.Direction != EParamDirection::In)
				{
					return false;
				}
				if (Param.Type.IsMaterial())
				{
					++MaterialInputs;
				}
			}
			return MaterialInputs >= 1;
		}
	}

	void FLangBinder::ClassifyFunctions()
	{
		int32 EntryIndex = INDEX_NONE;

		for (int32 Index = 0; Index < Bound.Functions.Num(); ++Index)
		{
			FBoundFunction& Function = Bound.Functions[Index];
			const FFunctionDecl& Decl = *Function.Decl;
			const bool bExported = Function.Linkage == EFunctionLinkage::Export;
			CurrentFile = Function.File;

			// The order below is CONTRACT §2 with the directives lifted above the signature test:
			// `/// @layer export void L(inout material)` matches the entry signature too, and the
			// directive is the more specific statement of intent.
			if (Function.Linkage == EFunctionLinkage::Extern)
			{
				Function.Kind = EBoundFunctionKind::Extern;

				if (Function.Directives.Asset.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH6202"),
						CurrentFile,
						Decl.NameSpan,
						FText::Format(
							LOCTEXT("ExternWithoutAsset", "'extern {0}' has nothing to bind to; add '/// @asset /Game/.../MF_Name' above it."),
							FText::FromString(Function.Name)));
				}
			}
			else if (Function.Directives.bLayer)
			{
				Function.Kind = EBoundFunctionKind::Layer;
				if (!bExported)
				{
					Diagnostics.Error(
						TEXT("DSH6203"),
						CurrentFile,
						Decl.NameSpan,
						FText::Format(
							LOCTEXT("LayerNotExported", "'@layer' makes '{0}' a material layer asset, so it has to be 'export'."),
							FText::FromString(Function.Name)));
				}
				if (!HasMaterialEntrySignature(Function))
				{
					Diagnostics.Error(
						TEXT("DSH6204"),
						CurrentFile,
						Decl.NameSpan,
						FText::Format(
							LOCTEXT("LayerSignature", "A '@layer' function is written 'export void {0}(inout material m)'."),
							FText::FromString(Function.Name)));
				}
				else
				{
					Function.MaterialResultParam = 0;
				}
			}
			else if (Function.Directives.bLayerBlend)
			{
				Function.Kind = EBoundFunctionKind::LayerBlend;
				if (!bExported)
				{
					Diagnostics.Error(
						TEXT("DSH6203"),
						CurrentFile,
						Decl.NameSpan,
						FText::Format(
							LOCTEXT("LayerBlendNotExported", "'@layerblend' makes '{0}' a material layer blend asset, so it has to be 'export'."),
							FText::FromString(Function.Name)));
				}
				if (!HasLayerBlendSignature(Function))
				{
					Diagnostics.Error(
						TEXT("DSH6205"),
						CurrentFile,
						Decl.NameSpan,
						FText::Format(
							LOCTEXT("LayerBlendSignature", "A '@layerblend' function is written 'export void {0}(material Base, material Top, ..., inout material Result)': at least one 'material' input and a final 'inout material'."),
							FText::FromString(Function.Name)));
				}
				else
				{
					Function.MaterialResultParam = Function.Params.Num() - 1;
				}
			}
			else if (bExported && HasMaterialEntrySignature(Function))
			{
				Function.Kind = EBoundFunctionKind::Entry;
				Function.MaterialResultParam = 0;

				if (EntryIndex != INDEX_NONE)
				{
					Diagnostics.Error(
						TEXT("DSH6200"),
						CurrentFile,
						Decl.NameSpan,
						FText::Format(
							LOCTEXT("TwoEntries", "'{0}' is a second material entry; '{1}' above it is already the entry, and one file makes one material."),
							FText::FromString(Function.Name),
							FText::FromString(Bound.Functions[EntryIndex].Name)));
				}
				else
				{
					EntryIndex = Index;
				}
			}
			else if (bExported)
			{
				Function.Kind = EBoundFunctionKind::ExportFunction;
			}
			else if (Function.Directives.bCustom)
			{
				Function.Kind = EBoundFunctionKind::Custom;
			}
			else
			{
				Function.Kind = EBoundFunctionKind::Helper;
			}

			if (Function.Kind != EBoundFunctionKind::Extern && Decl.IsPrototype())
			{
				Diagnostics.Error(
					TEXT("DSH6209"),
					CurrentFile,
					Decl.NameSpan,
					FText::Format(
						LOCTEXT("PrototypeWithoutExtern", "'{0}' has no body; a prototype has to be 'extern' and carry '/// @asset'."),
						FText::FromString(Function.Name)));
			}

			if (Function.Directives.bCustom)
			{
				// CONTRACT §6.13: a Custom node's inputs are translator-typed pins, and the 5.8
				// material translator has no MaterialAttributes input type for one. An `out material`
				// or a `material` return is still fine -- a Custom node may produce attributes.
				// Unit H checks this again at IR-build time (DSH6252); this is the same refusal said
				// at the declaration, where the author can see the parameter list.
				for (const FBoundParam& Param : Function.Params)
				{
					if (Param.Type.IsMaterial() && Param.Direction != EParamDirection::Out)
					{
						Diagnostics.Error(
							TEXT("DSH6210"),
							CurrentFile,
							Decl.NameSpan,
							FText::Format(
								LOCTEXT("CustomMaterialParameter", "'{0}' is '@custom', so '{1}' becomes an input pin of a Custom node, and a Custom node cannot take a material; pass the fields it needs instead."),
								FText::FromString(Function.Name),
								FText::FromString(Param.Name)));
					}
				}
			}

			if (Function.Directives.bCustom && !Decl.bOpaqueBody && Decl.Body)
			{
				// The parser only captures a body verbatim when it saw `@custom` on the block above
				// the declaration; a `@custom` that arrived some other way would be a parsed body
				// handed to a shader compiler, which is not the same text.
				Diagnostics.Warning(
					TEXT("DSH7228"),
					CurrentFile,
					Decl.NameSpan,
					FText::Format(
						LOCTEXT("CustomBodyParsed", "'@custom' on '{0}' did not make its body opaque; the directive has to sit in the '///' block directly above the declaration."),
						FText::FromString(Function.Name)));
			}
		}

		// CONTRACT §11 #15: one file, one product kind.
		if (EntryIndex != INDEX_NONE)
		{
			for (int32 Index = 0; Index < Bound.Functions.Num(); ++Index)
			{
				const FBoundFunction& Function = Bound.Functions[Index];
				if (Index == EntryIndex)
				{
					continue;
				}
				const bool bIsProduct =
					Function.Kind == EBoundFunctionKind::ExportFunction
					|| Function.Kind == EBoundFunctionKind::Layer
					|| Function.Kind == EBoundFunctionKind::LayerBlend;
				if (bIsProduct)
				{
					CurrentFile = Function.File;
					Diagnostics.Error(
						TEXT("DSH6201"),
						CurrentFile,
						Function.Decl->NameSpan,
						FText::Format(
							LOCTEXT("EntryAndExports", "'{0}' is exported from a file whose entry is '{1}'; a file makes a material or it makes functions, not both. Move it to its own file, or drop 'export' to make it a helper."),
							FText::FromString(Function.Name),
							FText::FromString(Bound.Functions[EntryIndex].Name)));
				}
			}
		}

		CurrentFile = RootModule.FilePath;
	}

	void FLangBinder::BuildProducts()
	{
		if (bHasMaterialPragma && Bound.FindEntryFunction() == INDEX_NONE)
		{
			CurrentFile = FirstMaterialPragmaFile;
			Diagnostics.Warning(
				TEXT("DSH7203"),
				CurrentFile,
				FirstMaterialPragmaSpan,
				LOCTEXT("MaterialPragmaWithoutEntry", "'#pragma material' configures a material, and this file has no 'export void Name(inout material m)' entry to configure."));
			CurrentFile = RootModule.FilePath;
		}

		for (int32 Index = 0; Index < Bound.Functions.Num(); ++Index)
		{
			const FBoundFunction& Function = Bound.Functions[Index];

			IR::EIRProductKind Kind = IR::EIRProductKind::Material;
			switch (Function.Kind)
			{
			case EBoundFunctionKind::Entry:          Kind = IR::EIRProductKind::Material; break;
			case EBoundFunctionKind::Layer:          Kind = IR::EIRProductKind::MaterialLayer; break;
			case EBoundFunctionKind::LayerBlend:     Kind = IR::EIRProductKind::MaterialLayerBlend; break;
			case EBoundFunctionKind::ExportFunction: Kind = IR::EIRProductKind::MaterialFunction; break;
			default:
				continue;
			}

			// A header cannot export (DSH3210, M1), so a product always comes from the root file.
			// The guard is here so a file that already reported that mistake does not also produce
			// an asset out of the header it included.
			if (!Function.File.Equals(RootModule.FilePath, ESearchCase::IgnoreCase))
			{
				continue;
			}

			FBoundProduct Product;
			Product.Kind = Kind;
			Product.FunctionIndex = Index;
			Product.Backend = ResolvedBackend;
			Product.AssetName = Function.Name;

			// CONTRACT §2: a full path in `@name` overrides the destination; a bare name replaces
			// the leaf and lets the 1.x root rules place it.
			const FString& NameDirective = Function.Directives.Name;
			if (!NameDirective.IsEmpty())
			{
				if (NameDirective.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
				{
					Product.AssetPathOverride = NameDirective;

					FString Leaf = NameDirective;
					int32 Slash = INDEX_NONE;
					if (Leaf.FindLastChar(TEXT('/'), Slash) && Slash + 1 < Leaf.Len())
					{
						Leaf = Leaf.RightChop(Slash + 1);
					}
					if (!Leaf.IsEmpty())
					{
						Product.AssetName = Leaf;
					}
				}
				else
				{
					Product.AssetName = NameDirective;
				}
			}

			if (Kind == IR::EIRProductKind::Material)
			{
				Product.Settings = Bound.MaterialSettings;
			}

			Bound.Products.Add(MoveTemp(Product));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Recursion
	// ---------------------------------------------------------------------------------------------

	void FLangBinder::DetectRecursion()
	{
		// Reachability by repeated closure: the call graph is a handful of nodes, so the simple
		// fixed point is cheaper than anything cleverer and cannot get the answer wrong.
		const int32 Count = Bound.Functions.Num();
		TArray<TSet<int32>> Reaches;
		Reaches = CallGraph;
		Reaches.SetNum(Count);

		bool bChanged = true;
		while (bChanged)
		{
			bChanged = false;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				TArray<int32> Callees = Reaches[Index].Array();
				for (int32 Callee : Callees)
				{
					if (!Reaches.IsValidIndex(Callee))
					{
						continue;
					}
					// Copied: when Callee is Index, the loop below adds to the very set it reads.
					const TArray<int32> Transitives = Reaches[Callee].Array();
					for (int32 Transitive : Transitives)
					{
						bool bAlready = false;
						Reaches[Index].Add(Transitive, &bAlready);
						if (!bAlready)
						{
							bChanged = true;
						}
					}
				}
			}
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			// Reported by I2 when it tries to inline (DSH6220); the binder only records the fact,
			// because a recursive Custom or Extern function is not a problem until something inlines.
			Bound.Functions[Index].bRecursive = Reaches[Index].Contains(Index);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Utilities
	// ---------------------------------------------------------------------------------------------

	IR::FIRType FLangBinder::Emit(const FExpr& Expr, FBoundExpr&& Binding)
	{
		const IR::FIRType Type = Binding.Type;
		Bound.Expressions.Add(&Expr, MoveTemp(Binding));
		return Type;
	}

	IR::FIRType FLangBinder::TypeOf(const FExpr& Expr) const
	{
		if (const FBoundExpr* Found = Bound.Expressions.Find(&Expr))
		{
			return Found->Type;
		}
		return IR::FIRType::Error();
	}

	bool FLangBinder::IsLValue(const FExpr& Expr) const
	{
		const FBoundExpr* Found = Bound.Expressions.Find(&Expr);
		return Found != nullptr && Found->bLValue;
	}

	bool FLangBinder::IsConstantExpr(const FExpr& Expr) const
	{
		const FBoundExpr* Found = Bound.Expressions.Find(&Expr);
		return Found != nullptr && Found->bIsConstant;
	}

	bool FLangBinder::GetConstant(const FExpr& Expr, double Out[4], int32& OutComponents) const
	{
		const FBoundExpr* Found = Bound.Expressions.Find(&Expr);
		if (!Found || !Found->bIsConstant)
		{
			return false;
		}

		OutComponents = FMath::Clamp(Found->Type.NumComponents(), 1, 4);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Out[Index] = Found->ConstantValue[Index];
		}
		return true;
	}

	void FLangBinder::SetConversion(const FExpr& Expr, IR::EIRConversion Conversion)
	{
		if (FBoundExpr* Found = Bound.Expressions.Find(&Expr))
		{
			Found->Conversion = Conversion;
		}
	}

	IR::FIRType FLangBinder::ResolveNodeDefault(const IR::FIRType& Type) const
	{
		if (!Type.IsNode() || !Catalog.Expressions.IsValidIndex(Type.CatalogIndex))
		{
			return Type;
		}
		const IR::FCatalogExpression& Class = Catalog.Expressions[Type.CatalogIndex];
		if (Class.Outputs.Num() == 0)
		{
			return Type;
		}
		return IR::TypeFromCatalogValueType(Class.Outputs[0].Type);
	}

	IR::FIRType FLangBinder::Fail(const FExpr& Expr)
	{
		FBoundExpr Binding;
		Binding.Kind = EBoundExprKind::Error;
		Binding.Type = IR::FIRType::Error();
		return Emit(Expr, MoveTemp(Binding));
	}

	FText FLangBinder::DescribeType(const IR::FIRType& Type) const
	{
		// FIRType::ToString() writes `struct#2` and `node#17`, which are right for a dump and wrong
		// for a message an author reads.
		if (Type.IsStruct() && Bound.Structs.IsValidIndex(Type.StructIndex))
		{
			return FText::FromString(Bound.Structs[Type.StructIndex].Name);
		}
		if (Type.IsNode() && Catalog.Expressions.IsValidIndex(Type.CatalogIndex))
		{
			const IR::FCatalogExpression& Entry = Catalog.Expressions[Type.CatalogIndex];
			return FText::FromString(Entry.Namespace + TEXT(".") + Entry.ShortName);
		}
		return FText::FromString(Type.ToString());
	}

	FString FLangBinder::SuggestCaseInsensitive(const FString& Name, TArrayView<const FString> Candidates)
	{
		for (const FString& Candidate : Candidates)
		{
			if (Candidate.Equals(Name, ESearchCase::IgnoreCase) && !Candidate.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Candidate;
			}
		}
		return FString();
	}

	int32 FLangBinder::FindStruct(const FString& Name) const
	{
		return Bound.FindStruct(Name);
	}

	int32 FLangBinder::FindGlobal(const FString& Name) const
	{
		return Bound.FindGlobal(Name);
	}

	int32 FLangBinder::FindFunction(const FString& Name) const
	{
		return Bound.FindFunction(Name);
	}

	int32 FLangBinder::FindLocal(const FString& Name) const
	{
		// Innermost scope first: a name may be reused in sibling blocks, and each declarator has its
		// own slot, so the answer is whichever scope is nearest.
		for (int32 ScopeIndex = Scopes.Num() - 1; ScopeIndex >= 0; --ScopeIndex)
		{
			const FBinderScope& Scope = Scopes[ScopeIndex];
			for (int32 Index = Scope.Names.Num() - 1; Index >= 0; --Index)
			{
				if (Scope.Names[Index].Key.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Scope.Names[Index].Value;
				}
			}
		}
		return INDEX_NONE;
	}

	int32 FLangBinder::FindParam(const FString& Name) const
	{
		if (!CurrentFunction)
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < CurrentFunction->Params.Num(); ++Index)
		{
			if (CurrentFunction->Params[Index].Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FLangBinder::GetGlobalArrayCount(int32 GlobalIndex) const
	{
		return Bound.Globals.IsValidIndex(GlobalIndex) ? Bound.Globals[GlobalIndex].ArrayCount : 0;
	}
}

namespace UE::DreamShader::Lang
{
	FLangBindResult BindDreamShaderLang(const FModule& Module, const FBindOptions& Options)
	{
		FLangBindResult Result;
		Result.Diagnostics = FLangDiagnosticSink(Module.FilePath);
		Result.Bound = MakeUnique<FBoundModule>();

		Private::FLangBinder Binder(Module, Options, *Result.Bound, Result.Diagnostics);
		Binder.Run();

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
