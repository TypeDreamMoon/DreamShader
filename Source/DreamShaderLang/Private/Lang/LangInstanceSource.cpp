// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `.dsi` source text without the compiler (Public/Lang/LangInstanceSource.h).
//
// Two directions. BuildDreamShaderInstanceModule turns an instance payload -- what the decompiler reads off
// a MaterialInstanceConstant -- into the ordinary tree, and PrintDreamShaderInstance prints it; the output is
// deterministic (overrides in payload order, canonical number text), which is what makes decompile -> print
// idempotent (research-instance.md 5).
//
// The rewrites go the other way: Adopt of a `.dsi`, and "Adopt tweaks as source defaults" on a `.dss`, change
// an existing file. They never reprint it. Each change is an edit over the parsed spans, and everything between
// the edits is the author's text byte for byte: `//` comments, blank lines, declaration order, the spelling of
// every value that did not change (`4` stays `4` where the payload would print `4.0`). research-instance.md 4.4.
//
// The smallest edit that states the new value wins: the initializer alone, or one `@default` / `@page`
// directive. A declaration whose shape cannot hold the value any more (a `float` that must become a texture,
// a `@static` that must go) is replaced whole by the printer's text for it, keeping the author's name. A
// `.dss` declaration is never replaced whole: its other directives belong to the author, so a shape mismatch
// there is refused (DSH9109).

#include "Lang/LangInstanceSource.h"

#include "LangParserInternal.h"

#include "IR/IR.h"
#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangPrinter.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"
#include "Semantic/LangBound.h"

#include "Containers/Set.h"
#include "HAL/UnrealMemory.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.InstanceSource"

namespace UE::DreamShader::Lang
{
	namespace Private::InstanceSource
	{
		// -----------------------------------------------------------------------------------------
		// Names and types
		// -----------------------------------------------------------------------------------------

		static bool IsInstanceIdentifierText(const FString& Text)
		{
			if (Text.IsEmpty() || !((Text[0] >= TEXT('A') && Text[0] <= TEXT('Z')) || (Text[0] >= TEXT('a') && Text[0] <= TEXT('z')) || Text[0] == TEXT('_')))
			{
				return false;
			}
			for (const TCHAR Character : Text)
			{
				const bool bPart = (Character >= TEXT('A') && Character <= TEXT('Z'))
					|| (Character >= TEXT('a') && Character <= TEXT('z'))
					|| (Character >= TEXT('0') && Character <= TEXT('9'))
					|| Character == TEXT('_');
				if (!bPart)
				{
					return false;
				}
			}
			return true;
		}

		/** A word 2.0 reads as something else: a keyword, a builtin type, or one of the two node namespaces. */
		static bool IsInstanceReservedWord(const FString& Text)
		{
			ELangKeyword Keyword = ELangKeyword::None;
			return TryGetLangKeyword(Text, Keyword)
				|| FLangParser::IsBuiltinTypeName(Text)
				|| Text.Equals(TEXT("UE"), ESearchCase::CaseSensitive)
				|| Text.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive);
		}

		static bool IsTextureLikeParameterKind(const IR::EIRParameterKind Kind)
		{
			switch (Kind)
			{
			case IR::EIRParameterKind::Texture:
			case IR::EIRParameterKind::TextureCollection:
			case IR::EIRParameterKind::Font:
			case IR::EIRParameterKind::RuntimeVirtualTexture:
			case IR::EIRParameterKind::SparseVolumeTexture:
			case IR::EIRParameterKind::ParameterCollection:
				return true;
			case IR::EIRParameterKind::Scalar:
			case IR::EIRParameterKind::Vector:
			case IR::EIRParameterKind::DoubleVector:
			case IR::EIRParameterKind::StaticSwitch:
			case IR::EIRParameterKind::StaticComponentMask:
				return false;
			}
			return false;
		}

		/** The spelling an override is declared with: the parent's declared type when known, else the kind's own. */
		static FString InstanceTypeSpelling(const IR::FIRInstanceOverride& Override)
		{
			if (!Override.DeclaredType.IsError())
			{
				// Struct and Node kinds render as `struct#N` / `node#N`: not a uniform spelling, so fall through.
				FString Spelled = Override.DeclaredType.ToString();
				if (IsInstanceIdentifierText(Spelled))
				{
					return Spelled;
				}
			}
			switch (Override.Kind)
			{
			case IR::EIRParameterKind::Scalar:                return TEXT("float");
			case IR::EIRParameterKind::Vector:                return TEXT("float4");
			case IR::EIRParameterKind::DoubleVector:          return TEXT("double4");
			case IR::EIRParameterKind::Texture:               return TEXT("Texture2D");
			case IR::EIRParameterKind::TextureCollection:     return TEXT("TextureCollection");
			case IR::EIRParameterKind::Font:                  return TEXT("Font");
			case IR::EIRParameterKind::RuntimeVirtualTexture: return TEXT("RuntimeVirtualTexture");
			case IR::EIRParameterKind::SparseVolumeTexture:   return TEXT("SparseVolumeTexture");
			case IR::EIRParameterKind::StaticSwitch:          return TEXT("bool");
			case IR::EIRParameterKind::ParameterCollection:   return TEXT("ParameterCollection");
			case IR::EIRParameterKind::StaticComponentMask:   return TEXT("bool4");
			}
			return TEXT("float");
		}

		/** One component of an override value, whatever encoding the payload used. */
		static double InstanceValueComponent(const IR::FIRPropertyValue& Value, const int32 Index)
		{
			switch (Value.Kind)
			{
			case IR::EIRPropertyKind::Float4:
				return Index < FMath::Clamp(Value.N, 1, 4) ? Value.V[Index] : (Index == 3 ? 1.0 : 0.0);
			case IR::EIRPropertyKind::Float:
				return Value.F;
			case IR::EIRPropertyKind::Int:
				return static_cast<double>(Value.I);
			case IR::EIRPropertyKind::Bool:
				return Value.B ? 1.0 : 0.0;
			case IR::EIRPropertyKind::String:
			case IR::EIRPropertyKind::Name:
			case IR::EIRPropertyKind::Enum:
			case IR::EIRPropertyKind::Object:
			case IR::EIRPropertyKind::StringList:
				return 0.0;
			}
			return 0.0;
		}

		/** The asset path of a texture-like override: "" (an explicit None) prints as `None`. */
		static FString InstanceAssetText(const IR::FIRPropertyValue& Value)
		{
			const FString Path = Value.S.TrimStartAndEnd();
			return Path.IsEmpty() ? FString(TEXT("None")) : Path;
		}

		/** An asset path compared the way the engine resolves it: quotes and case do not matter, `None` is empty. */
		static FString NormalizeInstanceAssetText(const FString& Text)
		{
			FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.Len() >= 2 && Trimmed[0] == TEXT('"') && Trimmed[Trimmed.Len() - 1] == TEXT('"'))
			{
				Trimmed = Trimmed.Mid(1, Trimmed.Len() - 2).TrimStartAndEnd();
			}
			return Trimmed.Equals(TEXT("None"), ESearchCase::IgnoreCase) ? FString() : Trimmed;
		}

		/** The initializer an override prints with, for a declaration of Type; null for texture-like kinds. */
		static FExprPtr MakeInstanceInitializer(const IR::FIRInstanceOverride& Override, const FTypeRef& Type, const FLangSpan& Span)
		{
			if (IsTextureLikeParameterKind(Override.Kind))
			{
				return nullptr;
			}

			const bool bBool = Type.Scalar == EScalarKind::Bool;
			const bool bInteger = Type.Scalar == EScalarKind::Int || Type.Scalar == EScalarKind::UInt;
			auto MakeComponent = [&Override, bBool, bInteger, &Span](const int32 Index) -> FExprPtr
			{
				const double Component = InstanceValueComponent(Override.Value, Index);
				if (bBool)
				{
					return LegacyAst::MakeBoolLiteral(Component != 0.0, Span);
				}
				if (bInteger)
				{
					return LegacyAst::MakeIntLiteral(static_cast<int64>(FMath::RoundToDouble(Component)), Span);
				}
				return LegacyAst::MakeFloatLiteral(FormatDreamShaderFloatLiteral(Component), Component, Span);
			};

			if (!Type.IsVector())
			{
				return MakeComponent(0);
			}

			TUniquePtr<FTypeExpr> Callee = MakeUnique<FTypeExpr>();
			Callee->Type = Type;
			Callee->Span = Span;
			TUniquePtr<FCallExpr> Constructor = MakeUnique<FCallExpr>();
			Constructor->Callee = MoveTemp(Callee);
			for (int32 Index = 0; Index < FMath::Clamp(Type.Rows, 2, 4); ++Index)
			{
				FArgument Argument;
				Argument.Value = MakeComponent(Index);
				Argument.Span = Span;
				Constructor->Arguments.Add(MoveTemp(Argument));
			}
			Constructor->Span = Span;
			return Constructor;
		}

		static void AddInstanceDirective(FDocBlock& Doc, const TCHAR* Key, const FString& Value)
		{
			FDocDirective Directive;
			Directive.Key = Key;
			Directive.Value = Value;
			Doc.Directives.Add(MoveTemp(Directive));
		}

		/**
		 * `/// @name ...  /// @static  /// @default ...  uniform <Type> <Name> [= value];` for one override.
		 * UsedNames, when given, keeps the identifier unique (case-insensitively) and records it.
		 */
		static TUniquePtr<FVariableDecl> BuildInstanceOverrideDecl(const IR::FIRInstanceOverride& Override, TSet<FString>* UsedNames)
		{
			TUniquePtr<FVariableDecl> Variable = MakeUnique<FVariableDecl>();
			Variable->Storage = EStorageClass::Uniform;
			FLangParser::ClassifyTypeName(InstanceTypeSpelling(Override), Variable->Type);

			FString Name = Override.VariableName;
			if (!IsInstanceIdentifierText(Name) || IsInstanceReservedWord(Name))
			{
				Name = MakeDreamShaderIdentifier(Override.ParameterName);
			}
			if (UsedNames)
			{
				const FString BaseName = Name;
				for (int32 Suffix = 2; UsedNames->Contains(Name); ++Suffix)
				{
					Name = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix);
				}
				UsedNames->Add(Name);
			}
			Variable->Declarator.Name = Name;

			if (!Override.ParameterName.Equals(Name, ESearchCase::CaseSensitive))
			{
				AddInstanceDirective(Variable->Doc, Directive::Name, Override.ParameterName);
			}
			if (Override.Kind == IR::EIRParameterKind::StaticSwitch || Override.Kind == IR::EIRParameterKind::StaticComponentMask)
			{
				AddInstanceDirective(Variable->Doc, Directive::Static, FString());
			}
			if (IsTextureLikeParameterKind(Override.Kind))
			{
				AddInstanceDirective(Variable->Doc, Directive::Default, InstanceAssetText(Override.Value));
			}
			if (Override.Kind == IR::EIRParameterKind::Font)
			{
				AddInstanceDirective(Variable->Doc, Directive::Page, FString::FromInt(Override.FontPage));
			}

			Variable->Declarator.Initializer = MakeInstanceInitializer(Override, Variable->Type, FLangSpan());
			return Variable;
		}

		/** What `#pragma` reads back unquoted: one identifier, or one number with its sign. */
		static bool IsInstanceSimplePragmaValue(const FString& Value)
		{
			if (Value.IsEmpty())
			{
				return false;
			}
			if (IsInstanceIdentifierText(Value))
			{
				return true;
			}
			FString Number = Value;
			if (Number.StartsWith(TEXT("-")) || Number.StartsWith(TEXT("+")))
			{
				Number.RightChopInline(1);
			}
			return !Number.IsEmpty() && FCString::IsNumeric(*Number);
		}

		static FString InstanceParentText(const IR::FIRInstance& Instance)
		{
			return Instance.ParentReference.IsEmpty() ? Instance.ParentObjectPath : Instance.ParentReference;
		}

		/** `#pragma instance(Parent = "...", Key = Value, ...)`, keys in payload order. */
		static TUniquePtr<FPragmaDecl> BuildInstancePragma(const IR::FIRInstance& Instance)
		{
			TUniquePtr<FPragmaDecl> Pragma = MakeUnique<FPragmaDecl>();
			Pragma->PragmaKind = EPragmaKind::Instance;
			Pragma->Name = TEXT("instance");

			FPragmaArgument Parent;
			Parent.Key = TEXT("Parent");
			Parent.Value = InstanceParentText(Instance);
			Parent.bQuoted = true;
			Pragma->Arguments.Add(MoveTemp(Parent));

			for (const TPair<FString, FString>& Setting : Instance.Settings)
			{
				FPragmaArgument Argument;
				Argument.Key = Setting.Key;
				Argument.Value = Setting.Value;
				Argument.bQuoted = !IsInstanceSimplePragmaValue(Setting.Value);
				Pragma->Arguments.Add(MoveTemp(Argument));
			}
			return Pragma;
		}

		// -----------------------------------------------------------------------------------------
		// Text positions
		// -----------------------------------------------------------------------------------------

		static FString DetectInstanceNewLine(const FString& Text)
		{
			const int32 LineFeed = Text.Find(TEXT("\n"));
			return (LineFeed > 0 && Text[LineFeed - 1] == TEXT('\r')) ? FString(TEXT("\r\n")) : FString(TEXT("\n"));
		}

		static int32 InstanceLineStart(const FString& Text, int32 Offset)
		{
			Offset = FMath::Clamp(Offset, 0, Text.Len());
			while (Offset > 0 && Text[Offset - 1] != TEXT('\n') && Text[Offset - 1] != TEXT('\r'))
			{
				--Offset;
			}
			return Offset;
		}

		/** The offset just past the line terminator of the line holding Offset (the text end on the last line). */
		static int32 InstanceLineEndWithTerminator(const FString& Text, int32 Offset)
		{
			Offset = FMath::Clamp(Offset, 0, Text.Len());
			while (Offset < Text.Len() && Text[Offset] != TEXT('\n') && Text[Offset] != TEXT('\r'))
			{
				++Offset;
			}
			if (Offset < Text.Len() && Text[Offset] == TEXT('\r'))
			{
				++Offset;
			}
			if (Offset < Text.Len() && Text[Offset] == TEXT('\n'))
			{
				++Offset;
			}
			return Offset;
		}

		static FString InstanceLineIndent(const FString& Text, const int32 Offset)
		{
			const int32 Start = InstanceLineStart(Text, Offset);
			int32 End = Start;
			while (End < Text.Len() && (Text[End] == TEXT(' ') || Text[End] == TEXT('\t')))
			{
				++End;
			}
			return Text.Mid(Start, End - Start);
		}

		/** Where a declaration's text starts: its `///` block when it has one. */
		static int32 InstanceDeclStart(const FVariableDecl& Decl)
		{
			return Decl.Doc.Span.Length > 0 ? FMath::Min(Decl.Doc.Span.Offset, Decl.Span.Offset) : Decl.Span.Offset;
		}

		static FLangPrintOptions InstancePrintOptions(const FString& NewLine)
		{
			FLangPrintOptions Options;
			Options.NewLine = NewLine;
			Options.bPrintTrivia = false;
			return Options;
		}

		// -----------------------------------------------------------------------------------------
		// What a parsed declaration states
		// -----------------------------------------------------------------------------------------

		/** The parameter a parsed uniform overrides: its `@name`, else its identifier. */
		static FString InstanceParameterNameOf(const FVariableDecl& Decl)
		{
			if (const FDocDirective* Name = Decl.Doc.Find(Directive::Name))
			{
				const FString Value = Name->Value.TrimStartAndEnd();
				if (!Value.IsEmpty())
				{
					return Value;
				}
			}
			return Decl.Declarator.Name;
		}

		/** A literal, a signed literal, or a constructor of those -- the spellings a value override uses. Appends components. */
		static bool TryFoldInstanceConstant(const FExpr& Expr, TArray<double>& OutComponents)
		{
			if (const FLiteralExpr* Literal = Expr.As<FLiteralExpr>())
			{
				switch (Literal->LiteralKind)
				{
				case ELiteralKind::Int:
				case ELiteralKind::UInt:
					OutComponents.Add(static_cast<double>(Literal->Integer));
					return true;
				case ELiteralKind::Float:
					OutComponents.Add(Literal->Real);
					return true;
				case ELiteralKind::Bool:
					OutComponents.Add(Literal->bBool ? 1.0 : 0.0);
					return true;
				case ELiteralKind::String:
					return false;
				}
				return false;
			}

			if (const FUnaryExpr* Unary = Expr.As<FUnaryExpr>())
			{
				if (!Unary->Operand.IsValid() || (Unary->Op != EUnaryOp::Negate && Unary->Op != EUnaryOp::Plus))
				{
					return false;
				}
				const int32 First = OutComponents.Num();
				if (!TryFoldInstanceConstant(*Unary->Operand, OutComponents))
				{
					return false;
				}
				if (Unary->Op == EUnaryOp::Negate)
				{
					for (int32 Index = First; Index < OutComponents.Num(); ++Index)
					{
						OutComponents[Index] = -OutComponents[Index];
					}
				}
				return true;
			}

			if (const FCallExpr* Call = Expr.As<FCallExpr>())
			{
				const FTypeExpr* Constructor = Call->Callee.IsValid() ? Call->Callee->As<FTypeExpr>() : nullptr;
				if (!Constructor || !(Constructor->Type.IsScalar() || Constructor->Type.IsVector()))
				{
					return false;
				}
				const int32 First = OutComponents.Num();
				for (const FArgument& Argument : Call->Arguments)
				{
					if (!Argument.Name.IsEmpty() || Argument.PinIndex != INDEX_NONE || !Argument.Value.IsValid()
						|| !TryFoldInstanceConstant(*Argument.Value, OutComponents))
					{
						return false;
					}
				}
				const int32 Wanted = Constructor->Type.IsVector() ? Constructor->Type.Rows : 1;
				const int32 Written = OutComponents.Num() - First;
				if (Written == 1 && Wanted > 1)
				{
					// `float3(0.5)` fills every component. Copied first: Add must not read from its own storage.
					const double Fill = OutComponents[First];
					for (int32 Index = 1; Index < Wanted; ++Index)
					{
						OutComponents.Add(Fill);
					}
					return true;
				}
				return Written == Wanted;
			}
			return false;
		}

		/** The components Decl's initializer states: the binder's fold when it made one, else the literal fold. */
		static bool ReadInstanceInitializer(const FBoundModule& Bound, const FVariableDecl& Decl, TArray<double>& OutComponents)
		{
			OutComponents.Reset();
			if (!Decl.Declarator.Initializer.IsValid())
			{
				return false;
			}
			const FExpr& Initializer = *Decl.Declarator.Initializer;
			if (const FBoundExpr* BoundInitializer = Bound.Find(Initializer))
			{
				if (BoundInitializer->bIsConstant)
				{
					const int32 Count = Decl.Type.IsVector() ? FMath::Clamp(Decl.Type.Rows, 2, 4) : 1;
					for (int32 Index = 0; Index < Count; ++Index)
					{
						OutComponents.Add(BoundInitializer->ConstantValue[Index]);
					}
					return true;
				}
			}
			return TryFoldInstanceConstant(Initializer, OutComponents);
		}

		static bool InstanceAssetMatches(const FVariableDecl& Decl, const IR::FIRInstanceOverride& Want)
		{
			const FDocDirective* Default = Decl.Doc.Find(Directive::Default);
			return Default && NormalizeInstanceAssetText(Default->Value).Equals(NormalizeInstanceAssetText(Want.Value.S), ESearchCase::IgnoreCase);
		}

		static int32 InstanceFontPageOf(const FVariableDecl& Decl)
		{
			const FDocDirective* Page = Decl.Doc.Find(Directive::Page);
			return Page ? FCString::Atoi(*Page->Value.TrimStartAndEnd()) : 0;
		}

		/** Whether Decl's declaration form can state Want at all: value vs asset, scalar vs vector, `@static` or not. */
		static bool InstanceDeclShapeFits(const FVariableDecl& Decl, const IR::FIRInstanceOverride& Want)
		{
			const bool bStaticWritten = Decl.Doc.Has(Directive::Static);
			switch (Want.Kind)
			{
			case IR::EIRParameterKind::Scalar:
				return Decl.Type.IsScalar() && !bStaticWritten;
			case IR::EIRParameterKind::Vector:
			case IR::EIRParameterKind::DoubleVector:
				return Decl.Type.IsVector() && !bStaticWritten;
			case IR::EIRParameterKind::StaticSwitch:
				return Decl.Type.IsScalar() && bStaticWritten;
			case IR::EIRParameterKind::StaticComponentMask:
				return Decl.Type.IsVector() && bStaticWritten;
			case IR::EIRParameterKind::Texture:
			case IR::EIRParameterKind::TextureCollection:
			case IR::EIRParameterKind::Font:
			case IR::EIRParameterKind::RuntimeVirtualTexture:
			case IR::EIRParameterKind::SparseVolumeTexture:
			case IR::EIRParameterKind::ParameterCollection:
				return !Decl.Declarator.Initializer.IsValid() && !Decl.Type.IsNumeric();
			}
			return false;
		}

		/** Whether Decl already states Want's value, compared the way the engine stores it (float32 channels). */
		static bool InstanceValueUnchanged(const FBoundModule& Bound, const FVariableDecl& Decl, const IR::FIRInstanceOverride& Want)
		{
			if (IsTextureLikeParameterKind(Want.Kind))
			{
				return InstanceAssetMatches(Decl, Want)
					&& (Want.Kind != IR::EIRParameterKind::Font || InstanceFontPageOf(Decl) == Want.FontPage);
			}

			TArray<double> Components;
			if (!ReadInstanceInitializer(Bound, Decl, Components))
			{
				return false;
			}
			const int32 Expected = Decl.Type.IsVector() ? FMath::Clamp(Decl.Type.Rows, 2, 4) : 1;
			if (Components.Num() != Expected)
			{
				return false;
			}

			const bool bBool = Decl.Type.Scalar == EScalarKind::Bool;
			for (int32 Index = 0; Index < Expected; ++Index)
			{
				const double Wanted = InstanceValueComponent(Want.Value, Index);
				const bool bSame = bBool
					? ((Components[Index] != 0.0) == (Wanted != 0.0))
					: static_cast<float>(Components[Index]) == static_cast<float>(Wanted);
				if (!bSame)
				{
					return false;
				}
			}
			return true;
		}

		/** True when Decl shares one `uniform float a, b;` statement with another name. */
		static bool InstanceDeclSharesStatement(const FModule& Parsed, const FVariableDecl& Decl)
		{
			if (Decl.bSharesDeclarationWithPrevious)
			{
				return true;
			}
			for (int32 Index = 0; Index + 1 < Parsed.Declarations.Num(); ++Index)
			{
				if (Parsed.Declarations[Index].Get() == &Decl)
				{
					const FVariableDecl* Next = Parsed.Declarations[Index + 1].IsValid() ? Parsed.Declarations[Index + 1]->As<FVariableDecl>() : nullptr;
					return Next && Next->bSharesDeclarationWithPrevious;
				}
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------
		// Edits
		// -----------------------------------------------------------------------------------------

		static void ReportInstanceSharedStatement(FLangDiagnosticSink& Diagnostics, const FLangSourceText& Original, const FVariableDecl& Decl)
		{
			Diagnostics.Error(
				TEXT("DSH9107"),
				Original.GetPath(),
				Decl.Declarator.NameSpan,
				FText::Format(
					LOCTEXT("SharedStatement", "Expected '{0}' to be declared alone to rewrite its value, found it in a declaration shared with other names; split the declaration first."),
					FText::FromString(Decl.Declarator.Name)));
		}

		static void AddInstanceEdit(const FLangSourceText& Original, TArray<FLangSourceEdit>& OutEdits, const int32 Offset, const int32 Length, const FString& NewText)
		{
			FLangSourceEdit& Edit = OutEdits.AddDefaulted_GetRef();
			Edit.Span = Original.MakeSpan(Offset, Length);
			Edit.NewText = NewText;
		}

		/** Replaces or inserts one `///` directive's value on Decl, keeping the rest of its line. */
		static void EditInstanceDirective(
			const FLangSourceText& Original,
			const FVariableDecl& Decl,
			const TCHAR* Key,
			const FString& Value,
			const FString& NewLine,
			TArray<FLangSourceEdit>& OutEdits)
		{
			const FString& Text = Original.GetText();
			if (const FDocDirective* Existing = Decl.Doc.Find(Key))
			{
				if (Existing->Span.Length > 0)
				{
					// The span runs to the next directive on the line; the whitespace before that one stays.
					const FString Slice = Original.Slice(Existing->Span);
					int32 KeepFrom = Slice.Len();
					while (KeepFrom > 0 && FChar::IsWhitespace(Slice[KeepFrom - 1]))
					{
						--KeepFrom;
					}
					const FString NewDirective = FString::Printf(TEXT("@%s %s"), Key, *Value) + Slice.RightChop(KeepFrom);
					AddInstanceEdit(Original, OutEdits, Existing->Span.Offset, Existing->Span.Length, NewDirective);
					return;
				}
			}

			// A new `///` line right above the declaration's own line, after any existing doc lines.
			const int32 LineStart = InstanceLineStart(Text, Decl.Span.Offset);
			const FString Indent = InstanceLineIndent(Text, InstanceDeclStart(Decl));
			AddInstanceEdit(Original, OutEdits, LineStart, 0, FString::Printf(TEXT("%s/// @%s %s"), *Indent, Key, *Value) + NewLine);
		}

		/** The edits that make Decl (whose shape fits) state Want: its initializer, or its `@default` / `@page`. */
		static void EditInstanceValue(
			const FLangSourceText& Original,
			const FVariableDecl& Decl,
			const IR::FIRInstanceOverride& Want,
			const FString& NewLine,
			TArray<FLangSourceEdit>& OutEdits)
		{
			if (IsTextureLikeParameterKind(Want.Kind))
			{
				if (!InstanceAssetMatches(Decl, Want))
				{
					EditInstanceDirective(Original, Decl, Directive::Default, InstanceAssetText(Want.Value), NewLine, OutEdits);
				}
				if (Want.Kind == IR::EIRParameterKind::Font && InstanceFontPageOf(Decl) != Want.FontPage)
				{
					EditInstanceDirective(Original, Decl, Directive::Page, FString::FromInt(Want.FontPage), NewLine, OutEdits);
				}
				return;
			}

			const FExprPtr Initializer = MakeInstanceInitializer(Want, Decl.Type, FLangSpan());
			if (!Initializer.IsValid())
			{
				return;
			}
			const FString InitializerText = PrintDreamShaderLangExpr(*Initializer);
			if (Decl.Declarator.Initializer.IsValid() && Decl.Declarator.Initializer->Span.Length > 0)
			{
				AddInstanceEdit(Original, OutEdits, Decl.Declarator.Initializer->Span.Offset, Decl.Declarator.Initializer->Span.Length, InitializerText);
			}
			else
			{
				AddInstanceEdit(Original, OutEdits, Decl.Declarator.NameSpan.End(), 0, TEXT(" = ") + InitializerText);
			}
		}

		/** `.dsi` only: Decl's `///` block and declaration replaced by the printed override, under the author's name. */
		static void ReplaceInstanceDecl(
			const FLangSourceText& Original,
			const FVariableDecl& Decl,
			const IR::FIRInstanceOverride& Want,
			const FString& NewLine,
			TArray<FLangSourceEdit>& OutEdits)
		{
			IR::FIRInstanceOverride Kept = Want;
			Kept.VariableName = Decl.Declarator.Name;
			const TUniquePtr<FVariableDecl> NewDecl = BuildInstanceOverrideDecl(Kept, nullptr);
			const int32 First = InstanceDeclStart(Decl);
			AddInstanceEdit(Original, OutEdits, First, Decl.Span.End() - First, PrintDreamShaderLangDecl(*NewDecl, InstancePrintOptions(NewLine)));
		}

		/** Deletes Decl with its `///` block: whole lines when nothing else shares them, else exactly its text. */
		static void DeleteInstanceDecl(const FLangSourceText& Original, const FModule& Parsed, const FVariableDecl& Decl, TArray<FLangSourceEdit>& OutEdits)
		{
			const FString& Text = Original.GetText();
			const int32 First = InstanceDeclStart(Decl);
			const int32 LineStart = InstanceLineStart(Text, First);
			const int32 LineEnd = InstanceLineEndWithTerminator(Text, FMath::Max(Decl.Span.End() - 1, Decl.Span.Offset));

			bool bShared = false;
			for (const FDeclPtr& Other : Parsed.Declarations)
			{
				if (Other.IsValid() && Other.Get() != &Decl && Other->Span.Length > 0
					&& Other->Span.Offset < LineEnd && Other->Span.End() > LineStart)
				{
					bShared = true;
					break;
				}
			}

			if (!bShared)
			{
				AddInstanceEdit(Original, OutEdits, LineStart, LineEnd - LineStart, FString());
				return;
			}
			AddInstanceEdit(Original, OutEdits, First, Decl.Span.End() - First, FString());
		}

		/** Sorts, checks for overlap (DSH9108) and applies. OutText stays Original's text when this fails. */
		static bool ApplyInstanceEdits(const FLangSourceText& Original, TArray<FLangSourceEdit>& Edits, FString& OutText, FLangDiagnosticSink& Diagnostics)
		{
			Edits.StableSort([](const FLangSourceEdit& A, const FLangSourceEdit& B) { return A.Span.Offset < B.Span.Offset; });

			const FString& Text = Original.GetText();
			for (int32 Index = 0; Index < Edits.Num(); ++Index)
			{
				const bool bOutside = Edits[Index].Span.Offset < 0 || Edits[Index].Span.Length < 0 || Edits[Index].Span.End() > Text.Len();
				const bool bOverlaps = Index > 0 && Edits[Index].Span.Offset < Edits[Index - 1].Span.End();
				if (bOutside || bOverlaps)
				{
					Diagnostics.Error(
						TEXT("DSH9108"),
						Original.GetPath(),
						Edits[Index].Span,
						FText::Format(
							LOCTEXT("OverlappingEdits", "Expected every change to this file to touch its own stretch of text, found an edit at line {0} that overlaps another or runs past the end; the file was left unchanged."),
							FText::AsNumber(Edits[Index].Span.Line)));
					Edits.Reset();
					OutText = Text;
					return false;
				}
			}

			FString Result;
			Result.Reserve(Text.Len() + 256);
			int32 Cursor = 0;
			for (const FLangSourceEdit& Edit : Edits)
			{
				Result += Text.Mid(Cursor, Edit.Span.Offset - Cursor);
				Result += Edit.NewText;
				Cursor = Edit.Span.End();
			}
			Result += Text.Mid(Cursor);
			OutText = MoveTemp(Result);
			return true;
		}

		/** Whether the parsed `#pragma instance` already states Desired's parent and keys, in order. */
		static bool InstancePragmaUnchanged(const FPragmaDecl& Pragma, const IR::FIRInstance& Desired)
		{
			const FPragmaArgument* Parent = Pragma.Find(TEXT("Parent"));
			if (!Parent)
			{
				return false;
			}
			// The author's spelling of an unchanged parent stays: either field of the payload may carry it.
			const bool bSameParent = Parent->Value.Equals(Desired.ParentReference, ESearchCase::CaseSensitive)
				|| (!Desired.ParentObjectPath.IsEmpty() && Parent->Value.Equals(Desired.ParentObjectPath, ESearchCase::IgnoreCase));
			if (!bSameParent)
			{
				return false;
			}

			int32 SettingIndex = 0;
			for (const FPragmaArgument& Argument : Pragma.Arguments)
			{
				if (&Argument == Parent)
				{
					continue;
				}
				if (!Desired.Settings.IsValidIndex(SettingIndex)
					|| !Desired.Settings[SettingIndex].Key.Equals(Argument.Key, ESearchCase::CaseSensitive)
					|| !Desired.Settings[SettingIndex].Value.Equals(Argument.Value, ESearchCase::CaseSensitive))
				{
					return false;
				}
				++SettingIndex;
			}
			return SettingIndex == Desired.Settings.Num();
		}

		/** The `#pragma instance` line reprinted, a trailing `//` comment on it kept. */
		static void ReplaceInstancePragma(const FLangSourceText& Original, const FPragmaDecl& Pragma, const IR::FIRInstance& Desired, const FString& NewLine, TArray<FLangSourceEdit>& OutEdits)
		{
			const FString& Text = Original.GetText();
			const int32 SpanEnd = FMath::Min(Pragma.Span.End(), Text.Len());
			int32 ReplaceEnd = SpanEnd;
			bool bInString = false;
			for (int32 Index = Pragma.Span.Offset; Index + 1 < SpanEnd; ++Index)
			{
				if (Text[Index] == TEXT('"'))
				{
					bInString = !bInString;
				}
				else if (!bInString && Text[Index] == TEXT('/') && Text[Index + 1] == TEXT('/'))
				{
					ReplaceEnd = Index;
					break;
				}
			}
			while (ReplaceEnd > Pragma.Span.Offset && FChar::IsWhitespace(Text[ReplaceEnd - 1]))
			{
				--ReplaceEnd;
			}

			const TUniquePtr<FPragmaDecl> NewPragma = BuildInstancePragma(Desired);
			AddInstanceEdit(Original, OutEdits, Pragma.Span.Offset, ReplaceEnd - Pragma.Span.Offset, PrintDreamShaderLangDecl(*NewPragma, InstancePrintOptions(NewLine)));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Public entry points
	// ---------------------------------------------------------------------------------------------

	TUniquePtr<FModule> BuildDreamShaderInstanceModule(const IR::FIRInstance& Instance, const FString& FilePath, const FString& AssetPathOverride)
	{
		TUniquePtr<FModule> Module = MakeUnique<FModule>();
		Module->FilePath = FilePath;
		Module->FileKind = ELangFileKind::Dsi;

		TUniquePtr<FPragmaDecl> Pragma = Private::InstanceSource::BuildInstancePragma(Instance);
		if (!AssetPathOverride.IsEmpty())
		{
			Private::InstanceSource::AddInstanceDirective(Pragma->Doc, Directive::Name, AssetPathOverride);
		}
		Module->Declarations.Add(MoveTemp(Pragma));

		TSet<FString> UsedNames;
		for (int32 Index = 0; Index < Instance.Overrides.Num(); ++Index)
		{
			TUniquePtr<FVariableDecl> Override = Private::InstanceSource::BuildInstanceOverrideDecl(Instance.Overrides[Index], &UsedNames);
			// A blank line after the pragma and none between overrides: the layout the printer takes from trivia.
			Module->Trivia.FindOrAdd(static_cast<const FNode*>(Override.Get())).BlankLinesBefore = Index == 0 ? 1 : 0;
			Module->Declarations.Add(MoveTemp(Override));
		}
		return Module;
	}

	FString PrintDreamShaderInstance(const IR::FIRInstance& Instance, const FString& FilePath, const FString& AssetPathOverride, const FLangPrintOptions& Options)
	{
		const TUniquePtr<FModule> Module = BuildDreamShaderInstanceModule(Instance, FilePath, AssetPathOverride);
		return PrintDreamShaderLang(*Module, Options);
	}

	bool RewriteDreamShaderInstanceSource(
		const FLangSourceText& Original,
		const FModule& Parsed,
		const FBoundModule& Bound,
		const IR::FIRInstance& Desired,
		TArray<FLangSourceEdit>& OutEdits,
		FString& OutText,
		FLangDiagnosticSink& Diagnostics)
	{
		using namespace Private::InstanceSource;

		OutEdits.Reset();
		OutText = Original.GetText();
		const FString& Text = Original.GetText();
		const FString NewLine = DetectInstanceNewLine(Text);

		const FPragmaDecl* Pragma = Bound.Instance.Pragma;
		for (int32 Index = 0; !Pragma && Index < Parsed.Declarations.Num(); ++Index)
		{
			const FPragmaDecl* Candidate = Parsed.Declarations[Index].IsValid() ? Parsed.Declarations[Index]->As<FPragmaDecl>() : nullptr;
			if (Candidate && Candidate->PragmaKind == EPragmaKind::Instance)
			{
				Pragma = Candidate;
			}
		}
		if (!Pragma)
		{
			return Diagnostics.Error(
				TEXT("DSH9109"),
				Original.GetPath(),
				Original.MakeSpan(0, 0),
				LOCTEXT("NoInstancePragma", "Expected a '#pragma instance(...)' line in this .dsi file, found none; the file was left unchanged."));
		}

		struct FExistingOverride
		{
			const FVariableDecl* Decl = nullptr;
			FString ParameterName;
			bool bMatched = false;
		};
		TArray<FExistingOverride> Existing;
		TSet<FString> UsedNames;
		const FDecl* LastAnchor = Pragma;
		for (const FDeclPtr& Declaration : Parsed.Declarations)
		{
			const FVariableDecl* Variable = Declaration.IsValid() ? Declaration->As<FVariableDecl>() : nullptr;
			if (Variable && Variable->Storage == EStorageClass::Uniform)
			{
				FExistingOverride& Entry = Existing.AddDefaulted_GetRef();
				Entry.Decl = Variable;
				Entry.ParameterName = InstanceParameterNameOf(*Variable);
				UsedNames.Add(Variable->Declarator.Name);
				if (Variable->Span.End() > LastAnchor->Span.End())
				{
					LastAnchor = Variable;
				}
			}
		}

		FString Inserted;
		for (const IR::FIRInstanceOverride& Want : Desired.Overrides)
		{
			FExistingOverride* Match = nullptr;
			for (FExistingOverride& Candidate : Existing)
			{
				if (!Candidate.bMatched && Candidate.ParameterName.Equals(Want.ParameterName, ESearchCase::CaseSensitive))
				{
					Match = &Candidate;
					break;
				}
			}

			if (!Match)
			{
				const TUniquePtr<FVariableDecl> NewDecl = BuildInstanceOverrideDecl(Want, &UsedNames);
				Inserted += PrintDreamShaderLangDecl(*NewDecl, InstancePrintOptions(NewLine)) + NewLine;
				continue;
			}

			Match->bMatched = true;
			const FVariableDecl& Decl = *Match->Decl;
			const bool bShapeFits = InstanceDeclShapeFits(Decl, Want);
			if (bShapeFits && InstanceValueUnchanged(Bound, Decl, Want))
			{
				continue;
			}
			if (InstanceDeclSharesStatement(Parsed, Decl))
			{
				ReportInstanceSharedStatement(Diagnostics, Original, Decl);
				OutEdits.Reset();
				return false;
			}
			if (bShapeFits)
			{
				EditInstanceValue(Original, Decl, Want, NewLine, OutEdits);
			}
			else
			{
				ReplaceInstanceDecl(Original, Decl, Want, NewLine, OutEdits);
			}
		}

		for (const FExistingOverride& Entry : Existing)
		{
			if (Entry.bMatched)
			{
				continue;
			}
			if (InstanceDeclSharesStatement(Parsed, *Entry.Decl))
			{
				ReportInstanceSharedStatement(Diagnostics, Original, *Entry.Decl);
				OutEdits.Reset();
				return false;
			}
			DeleteInstanceDecl(Original, Parsed, *Entry.Decl, OutEdits);
		}

		if (!Inserted.IsEmpty())
		{
			// After the last override (or the pragma), on lines of their own.
			const int32 AnchorLast = FMath::Max(LastAnchor->Span.End() - 1, LastAnchor->Span.Offset);
			const int32 InsertAt = InstanceLineEndWithTerminator(Text, AnchorLast);
			const bool bNeedsBreak = InsertAt == Text.Len()
				&& (Text.IsEmpty() || (Text[Text.Len() - 1] != TEXT('\n') && Text[Text.Len() - 1] != TEXT('\r')));
			AddInstanceEdit(Original, OutEdits, InsertAt, 0, (bNeedsBreak ? NewLine : FString()) + Inserted);
		}

		if (Pragma->Span.Length > 0 && !InstancePragmaUnchanged(*Pragma, Desired))
		{
			ReplaceInstancePragma(Original, *Pragma, Desired, NewLine, OutEdits);
		}

		return ApplyInstanceEdits(Original, OutEdits, OutText, Diagnostics);
	}

	bool RewriteDreamShaderUniformDefaults(
		const FLangSourceText& Original,
		const FModule& Parsed,
		const FBoundModule& Bound,
		const TArray<IR::FIRInstanceOverride>& NewDefaults,
		TArray<FLangSourceEdit>& OutEdits,
		FString& OutText,
		FLangDiagnosticSink& Diagnostics)
	{
		using namespace Private::InstanceSource;

		OutEdits.Reset();
		OutText = Original.GetText();
		const FString NewLine = DetectInstanceNewLine(Original.GetText());

		bool bOk = true;
		for (const IR::FIRInstanceOverride& Want : NewDefaults)
		{
			const FVariableDecl* Found = nullptr;
			for (const FBoundGlobal& Global : Bound.Globals)
			{
				if (!Global.bIsParameter || !Global.Decl)
				{
					continue;
				}
				const FString ParameterName = Global.Directives.Name.IsEmpty() ? Global.Name : Global.Directives.Name;
				if (!ParameterName.Equals(Want.ParameterName, ESearchCase::CaseSensitive))
				{
					continue;
				}
				// Only a declaration of this file can be spliced; an included header's text is not in Original.
				for (const FDeclPtr& Declaration : Parsed.Declarations)
				{
					if (Declaration.Get() == Global.Decl)
					{
						Found = Global.Decl;
						break;
					}
				}
				if (Found)
				{
					break;
				}
			}

			if (!Found)
			{
				Diagnostics.Error(
					TEXT("DSH9109"),
					Original.GetPath(),
					Original.MakeSpan(0, 0),
					FText::Format(
						LOCTEXT("NoUniformForDefault", "Expected a uniform whose parameter name is '{0}' declared in this file, found none; no default was written."),
						FText::FromString(Want.ParameterName)));
				bOk = false;
				continue;
			}
			if (!InstanceDeclShapeFits(*Found, Want))
			{
				Diagnostics.Error(
					TEXT("DSH9109"),
					Original.GetPath(),
					Found->Declarator.NameSpan,
					FText::Format(
						LOCTEXT("UniformKindMismatch", "Expected '{0}' to be declared as a {1} parameter to take this default, found a declaration of another kind; no default was written."),
						FText::FromString(Found->Declarator.Name),
						FText::FromString(FString(IR::LexToString(Want.Kind)))));
				bOk = false;
				continue;
			}
			if (InstanceValueUnchanged(Bound, *Found, Want))
			{
				continue;
			}
			if (InstanceDeclSharesStatement(Parsed, *Found))
			{
				ReportInstanceSharedStatement(Diagnostics, Original, *Found);
				bOk = false;
				continue;
			}
			EditInstanceValue(Original, *Found, Want, NewLine, OutEdits);
		}

		if (!bOk)
		{
			OutEdits.Reset();
			return false;
		}
		return ApplyInstanceEdits(Original, OutEdits, OutText, Diagnostics);
	}

	FString FormatDreamShaderFloatLiteral(const double Value)
	{
		const float Single = static_cast<float>(Value);
		if (!FMath::IsFinite(Single))
		{
			return TEXT("0.0");
		}

		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Single, sizeof(Bits));
		const bool bNegative = (Bits >> 31) != 0;
		if (Single == 0.0f)
		{
			return bNegative ? TEXT("-0.0") : TEXT("0.0");
		}

		// An integral value prints in full (`100.0`, never `1e+02`); every float32 integer is exact in a double.
		if (FMath::Abs(Single) < 1.0e15f && FMath::RoundToFloat(Single) == Single)
		{
			return FString::Printf(TEXT("%.0f.0"), static_cast<double>(Single));
		}

		// Otherwise the fewest significant digits that read back as the same float32 (nine always do).
		const double Wide = static_cast<double>(Single);
		FString Text;
		for (int32 Precision = 1; Precision <= 9; ++Precision)
		{
			switch (Precision)
			{
			case 1:  Text = FString::Printf(TEXT("%.1g"), Wide); break;
			case 2:  Text = FString::Printf(TEXT("%.2g"), Wide); break;
			case 3:  Text = FString::Printf(TEXT("%.3g"), Wide); break;
			case 4:  Text = FString::Printf(TEXT("%.4g"), Wide); break;
			case 5:  Text = FString::Printf(TEXT("%.5g"), Wide); break;
			case 6:  Text = FString::Printf(TEXT("%.6g"), Wide); break;
			case 7:  Text = FString::Printf(TEXT("%.7g"), Wide); break;
			case 8:  Text = FString::Printf(TEXT("%.8g"), Wide); break;
			default: Text = FString::Printf(TEXT("%.9g"), Wide); break;
			}
			if (static_cast<float>(FCString::Atod(*Text)) == Single)
			{
				break;
			}
		}

		// Always a float literal: `2` would read back as an int.
		if (!Text.Contains(TEXT(".")) && !Text.Contains(TEXT("e"), ESearchCase::IgnoreCase))
		{
			Text += TEXT(".0");
		}
		return Text;
	}

	FString MakeDreamShaderIdentifier(const FString& ParameterName)
	{
		const FString Trimmed = ParameterName.TrimStartAndEnd();
		FString Result;
		Result.Reserve(Trimmed.Len() + 1);
		for (const TCHAR Character : Trimmed)
		{
			const bool bPart = (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9'))
				|| Character == TEXT('_');
			Result.AppendChar(bPart ? Character : TEXT('_'));
		}

		if (Result.IsEmpty())
		{
			return TEXT("Parameter");
		}
		if (Result[0] >= TEXT('0') && Result[0] <= TEXT('9'))
		{
			Result.InsertAt(0, TEXT('_'));
		}
		if (Private::InstanceSource::IsInstanceReservedWord(Result))
		{
			Result += TEXT("_");
		}
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
