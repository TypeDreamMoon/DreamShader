// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// File-scope declarations of DreamShaderLang 2.0: `uniform` / `static const` variables, helper,
// `export` and `extern` functions, `struct`, `#include` / `import`, `#pragma`, and the `///` doc
// blocks that hang off all of them. Types and array dimensions live here too because a
// declaration is where they are spelled; the statement parser borrows them for locals.
//
// Everything below reports through the sink with a literal DSH32nn code and returns null (or
// false) exactly once per failure; recovery is the module loop's job (LangParser.cpp).

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.Declarations"

namespace UE::DreamShader::Lang::Private
{
	// Declared (and documented) in LangParser.cpp, which is the only caller.
	bool ParseTrailingVariableDeclarators(FLangParser& Parser, const FVariableDecl& First, TArray<FDeclPtr>& OutDecls);

	namespace
	{
		// ------------------------------------------------------------------------------ types

		struct FScalarSpelling
		{
			const TCHAR* Name;
			EScalarKind Kind;
		};

		const FScalarSpelling GScalarSpellings[] =
		{
			{ TEXT("float"),  EScalarKind::Float },
			{ TEXT("half"),   EScalarKind::Half },
			{ TEXT("double"), EScalarKind::Double },
			{ TEXT("int"),    EScalarKind::Int },
			{ TEXT("uint"),   EScalarKind::UInt },
			{ TEXT("bool"),   EScalarKind::Bool },
		};

		struct FTextureSpelling
		{
			const TCHAR* Name;
			ETextureKind Kind;
		};

		const FTextureSpelling GTextureSpellings[] =
		{
			{ TEXT("Texture2D"),      ETextureKind::Texture2D },
			{ TEXT("TextureCube"),    ETextureKind::TextureCube },
			{ TEXT("Texture2DArray"), ETextureKind::Texture2DArray },
			{ TEXT("Texture3D"),      ETextureKind::Texture3D },
			{ TEXT("VolumeTexture"),  ETextureKind::VolumeTexture },
		};

		bool IsDimensionDigit(const TCHAR Char)
		{
			return Char >= TEXT('2') && Char <= TEXT('4');
		}

		/** The four 1.x top-level words the legacy front end (M4) will own. */
		bool IsLegacyDeclarationWord(const FString& Name)
		{
			return Name.Equals(TEXT("Function"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("GraphFunction"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("Namespace"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("VirtualFunction"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("Shader"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("ShaderFunction"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("ShaderLayer"), ESearchCase::CaseSensitive)
				|| Name.Equals(TEXT("ShaderLayerBlend"), ESearchCase::CaseSensitive);
		}

		// ------------------------------------------------------------------------ doc lines

		bool IsDocIdentifierStart(const TCHAR Char)
		{
			return FChar::IsAlpha(Char) || Char == TEXT('_');
		}

		bool IsDocIdentifierChar(const TCHAR Char)
		{
			return FChar::IsAlnum(Char) || Char == TEXT('_');
		}

		/** A `@` that starts a directive: at the start of the text or after whitespace, followed by an identifier. */
		bool IsDirectiveStartAt(const FString& Line, const int32 Index)
		{
			if (Index < 0 || Index >= Line.Len() || Line[Index] != TEXT('@'))
			{
				return false;
			}
			if (Index > 0 && !FChar::IsWhitespace(Line[Index - 1]))
			{
				return false;
			}
			return Index + 1 < Line.Len() && IsDocIdentifierStart(Line[Index + 1]);
		}

		/** A `@` at a directive position that is NOT followed by an identifier -- the DSH3220 shape. */
		bool HasMalformedDirectiveAt(const FString& Line)
		{
			for (int32 Index = 0; Index < Line.Len(); ++Index)
			{
				if (Line[Index] != TEXT('@'))
				{
					continue;
				}
				const bool bAtDirectivePosition = Index == 0 || FChar::IsWhitespace(Line[Index - 1]);
				if (bAtDirectivePosition && !(Index + 1 < Line.Len() && IsDocIdentifierStart(Line[Index + 1])))
				{
					return true;
				}
			}
			return false;
		}

		// ------------------------------------------------------------------- pragma payload

		/**
		 * A hand scanner over a directive's payload. The payload is one line that the lexer never
		 * tokenised (it is not DreamShaderLang, it is a `#` line), so the little grammar of
		 * `name(Key = Value, ...)` is read here directly.
		 */
		struct FPragmaScanner
		{
			const FString& Text;
			int32 Index = 0;

			explicit FPragmaScanner(const FString& InText) : Text(InText) {}

			bool AtEnd() const { return Index >= Text.Len(); }
			TCHAR Peek() const { return AtEnd() ? TEXT('\0') : Text[Index]; }

			void SkipWhitespace()
			{
				while (!AtEnd() && FChar::IsWhitespace(Text[Index]))
				{
					++Index;
				}
			}

			bool Accept(const TCHAR Char)
			{
				SkipWhitespace();
				if (Peek() == Char)
				{
					++Index;
					return true;
				}
				return false;
			}

			/** An identifier, a signed number, or a quoted string. */
			bool ReadValue(FString& OutValue, bool& bOutQuoted, int32& OutStart)
			{
				SkipWhitespace();
				OutStart = Index;
				bOutQuoted = false;
				OutValue.Reset();

				if (AtEnd())
				{
					return false;
				}

				const TCHAR First = Peek();
				if (First == TEXT('"'))
				{
					++Index;
					while (!AtEnd() && Text[Index] != TEXT('"'))
					{
						if (Text[Index] == TEXT('\\') && Index + 1 < Text.Len())
						{
							++Index;
						}
						OutValue.AppendChar(Text[Index]);
						++Index;
					}
					if (AtEnd())
					{
						return false;
					}
					++Index; // closing quote
					bOutQuoted = true;
					return true;
				}

				if (IsDocIdentifierStart(First))
				{
					while (!AtEnd() && IsDocIdentifierChar(Text[Index]))
					{
						OutValue.AppendChar(Text[Index]);
						++Index;
					}
					return true;
				}

				if (First == TEXT('-') || First == TEXT('+') || First == TEXT('.') || FChar::IsDigit(First))
				{
					OutValue.AppendChar(First);
					++Index;
					while (!AtEnd())
					{
						const TCHAR Char = Text[Index];
						const bool bNumberChar = FChar::IsAlnum(Char) || Char == TEXT('.') || Char == TEXT('_')
							|| ((Char == TEXT('-') || Char == TEXT('+')) && (Text[Index - 1] == TEXT('e') || Text[Index - 1] == TEXT('E')));
						if (!bNumberChar)
						{
							break;
						}
						OutValue.AppendChar(Char);
						++Index;
					}
					return OutValue.Len() > 0 && !(OutValue.Len() == 1 && (First == TEXT('-') || First == TEXT('+') || First == TEXT('.')));
				}

				return false;
			}
		};

		/** Splits `name` and `rest` of a `#pragma` payload: `material(...)` -> ("material", "(...)"). */
		void SplitPragmaName(const FString& Payload, FString& OutName, FString& OutRest)
		{
			int32 Index = 0;
			while (Index < Payload.Len() && FChar::IsWhitespace(Payload[Index]))
			{
				++Index;
			}
			const int32 NameStart = Index;
			while (Index < Payload.Len() && IsDocIdentifierChar(Payload[Index]))
			{
				++Index;
			}
			OutName = Payload.Mid(NameStart, Index - NameStart);
			OutRest = Payload.Mid(Index).TrimStartAndEnd();
		}

		/** The first whitespace-delimited word of a directive payload (`pragma`, `include`, `if`, ...). */
		void SplitFirstWord(const FString& Payload, FString& OutWord, FString& OutRest)
		{
			int32 Index = 0;
			while (Index < Payload.Len() && !FChar::IsWhitespace(Payload[Index]))
			{
				++Index;
			}
			OutWord = Payload.Left(Index);
			OutRest = Payload.Mid(Index).TrimStartAndEnd();
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Types
	// ---------------------------------------------------------------------------------------------

	void FLangParser::ClassifyTypeName(const FString& Name, FTypeRef& InOutType)
	{
		InOutType.Name = Name;
		InOutType.Category = ETypeCategory::Named;
		InOutType.Scalar = EScalarKind::None;
		InOutType.Texture = ETextureKind::None;
		InOutType.Rows = 1;
		InOutType.Cols = 1;

		if (Name.Equals(TEXT("void"), ESearchCase::CaseSensitive))
		{
			InOutType.Category = ETypeCategory::Void;
			return;
		}
		if (Name.Equals(TEXT("material"), ESearchCase::CaseSensitive))
		{
			InOutType.Category = ETypeCategory::Material;
			return;
		}
		if (Name.Equals(TEXT("Substrate"), ESearchCase::CaseSensitive))
		{
			InOutType.Category = ETypeCategory::Substrate;
			return;
		}
		if (Name.Equals(TEXT("SamplerState"), ESearchCase::CaseSensitive))
		{
			InOutType.Category = ETypeCategory::Sampler;
			return;
		}

		for (const FTextureSpelling& Texture : GTextureSpellings)
		{
			if (Name.Equals(Texture.Name, ESearchCase::CaseSensitive))
			{
				InOutType.Category = ETypeCategory::Texture;
				InOutType.Texture = Texture.Kind;
				return;
			}
		}

		for (const FScalarSpelling& Scalar : GScalarSpellings)
		{
			const int32 PrefixLength = FCString::Strlen(Scalar.Name);
			if (!Name.StartsWith(Scalar.Name, ESearchCase::CaseSensitive))
			{
				continue;
			}

			const FString Suffix = Name.Mid(PrefixLength);
			if (Suffix.IsEmpty())
			{
				InOutType.Category = ETypeCategory::Scalar;
				InOutType.Scalar = Scalar.Kind;
				return;
			}
			if (Suffix.Len() == 1 && IsDimensionDigit(Suffix[0]))
			{
				InOutType.Category = ETypeCategory::Vector;
				InOutType.Scalar = Scalar.Kind;
				InOutType.Rows = Suffix[0] - TEXT('0');
				return;
			}
			if (Suffix.Len() == 1 && Suffix[0] == TEXT('1'))
			{
				// `float1` is HLSL's spelling of a scalar.
				InOutType.Category = ETypeCategory::Scalar;
				InOutType.Scalar = Scalar.Kind;
				return;
			}
			if (Suffix.Len() == 3 && IsDimensionDigit(Suffix[0]) && Suffix[1] == TEXT('x') && IsDimensionDigit(Suffix[2]))
			{
				InOutType.Category = ETypeCategory::Matrix;
				InOutType.Scalar = Scalar.Kind;
				InOutType.Rows = Suffix[0] - TEXT('0');
				InOutType.Cols = Suffix[2] - TEXT('0');
				return;
			}
			// `floatX` with any other suffix is some user's name (`floatingPoint`), not a builtin.
		}
	}

	bool FLangParser::IsBuiltinTypeName(const FString& Name)
	{
		FTypeRef Type;
		ClassifyTypeName(Name, Type);
		return Type.Category != ETypeCategory::Named;
	}

	bool FLangParser::ParseType(FTypeRef& OutType)
	{
		if (!Check(ELangTokenKind::Identifier))
		{
			return Diagnostics.Error(
				TEXT("DSH3204"),
				Current().Span,
				FText::Format(LOCTEXT("ExpectedTypeName", "Expected a type name, found {0}."), DescribeToken(Current())));
		}

		const FLangToken& Token = Advance();
		ClassifyTypeName(Token.Text, OutType);
		OutType.Span = Token.Span;
		return true;
	}

	bool FLangParser::ParseArrayDimensions(TArray<FExprPtr>& OutDimensions)
	{
		while (Match(ELangTokenKind::LeftBracket))
		{
			if (Match(ELangTokenKind::RightBracket))
			{
				// `[]` -- unsized; the initializer decides.
				OutDimensions.AddDefaulted();
				continue;
			}

			FExprPtr Dimension = ParseExpression();
			if (!Dimension)
			{
				return false;
			}
			if (!Expect(ELangTokenKind::RightBracket, TEXT("DSH3215"), LOCTEXT("ExpectedArrayClose", "']' to close the array dimension")))
			{
				return false;
			}
			OutDimensions.Add(MoveTemp(Dimension));
		}
		return true;
	}

	bool FLangParser::LooksLikeDeclarationStart() const
	{
		int32 Ahead = 0;
		while (Peek(Ahead).Kind == ELangTokenKind::Keyword)
		{
			const ELangKeyword Keyword = Peek(Ahead).Keyword;
			const bool bPrefix = Keyword == ELangKeyword::Uniform
				|| Keyword == ELangKeyword::Static
				|| Keyword == ELangKeyword::Const
				|| Keyword == ELangKeyword::Extern
				|| Keyword == ELangKeyword::Export;
			if (!bPrefix)
			{
				return false;
			}
			++Ahead;
		}

		// `Type Name` -- two identifiers in a row. `Foo(x)` is a call, `Foo.x` a member, `Foo x` a
		// declaration; nothing else in the grammar puts two bare identifiers side by side.
		return Peek(Ahead).Kind == ELangTokenKind::Identifier
			&& Peek(Ahead + 1).Kind == ELangTokenKind::Identifier;
	}

	// ---------------------------------------------------------------------------------------------
	// Doc blocks
	// ---------------------------------------------------------------------------------------------

	void FLangParser::ParseDocLine(const FString& Line, const FLangSpan& LineSpan, FDocBlock& InOutDoc)
	{
		// The token span covers `///...` to the end of the line; the payload is its tail, so the
		// payload's first character sits at (span end - payload length).
		const int32 PayloadOffset = LineSpan.End() - Line.Len();

		int32 FirstDirective = -1;
		for (int32 Index = 0; Index < Line.Len(); ++Index)
		{
			if (IsDirectiveStartAt(Line, Index))
			{
				FirstDirective = Index;
				break;
			}
		}

		const FString FreeText = (FirstDirective < 0 ? Line : Line.Left(FirstDirective)).TrimStartAndEnd();
		if (!FreeText.IsEmpty() || FirstDirective < 0)
		{
			// A blank `///` line is kept as an empty free-text line: it is a paragraph break in the
			// description, and dropping it would glue two paragraphs together on the way back out.
			InOutDoc.FreeText.Add(FreeText);
		}

		int32 Index = FirstDirective;
		while (Index >= 0 && Index < Line.Len())
		{
			// Key: the identifier after `@`, canonical lower case.
			int32 KeyEnd = Index + 1;
			while (KeyEnd < Line.Len() && IsDocIdentifierChar(Line[KeyEnd]))
			{
				++KeyEnd;
			}

			// Value: up to the next directive start, or the end of the line.
			int32 Next = KeyEnd;
			while (Next < Line.Len() && !IsDirectiveStartAt(Line, Next))
			{
				++Next;
			}

			FDocDirective Directive;
			Directive.Key = Line.Mid(Index + 1, KeyEnd - (Index + 1)).ToLower();
			Directive.Value = Line.Mid(KeyEnd, Next - KeyEnd).TrimStartAndEnd();
			Directive.Span.Offset = PayloadOffset + Index;
			Directive.Span.Length = Next - Index;
			Directive.Span.Line = LineSpan.Line;
			Directive.Span.Column = LineSpan.Column + (Directive.Span.Offset - LineSpan.Offset);
			InOutDoc.Directives.Add(MoveTemp(Directive));

			Index = Next;
		}
	}

	bool FLangParser::ParseDocBlock(FDocBlock& OutDoc)
	{
		// One block can be filled by more than one call: ParseDeclaration() calls this again after
		// every stray `;`, and those runs belong to the same block. So the span joins onto whatever
		// is already there instead of replacing it -- a default FLangSpan has Length 0 and is the
		// "nothing recorded yet" marker.
		bool bHasSpan = OutDoc.Span.Length > 0;
		bool bConsumedAny = false;
		while (Check(ELangTokenKind::DocComment))
		{
			const FLangToken& Token = Advance();

			if (HasMalformedDirectiveAt(Token.Text))
			{
				Diagnostics.Warning(
					TEXT("DSH3220"),
					Token.Span,
					LOCTEXT("MalformedDocDirective", "A '@' in a '///' line must be followed by a directive name; the text is kept as description."));
			}

			OutDoc.Span = bHasSpan ? FLangSpan::Join(OutDoc.Span, Token.Span) : Token.Span;
			bHasSpan = true;
			ParseDocLine(Token.Text, Token.Span, OutDoc);
			bConsumedAny = true;
		}
		return bConsumedAny;
	}

	// ---------------------------------------------------------------------------------------------
	// Directives
	// ---------------------------------------------------------------------------------------------

	FDeclPtr FLangParser::ParseDirective(FDocBlock&& Doc)
	{
		const FLangToken& Token = Advance();
		const FLangSpan Span = Token.Span;

		FString Word;
		FString Rest;
		SplitFirstWord(Token.Text, Word, Rest);

		// Offsets inside the payload map back to the source. Anchor at the FRONT -- the first
		// non-space character after the `#` -- and never at `Span.End() - Text.Len()`: the span
		// covers the whole physical line, while the text has had both its leading whitespace and
		// any trailing `//` comment removed, so measuring back from the end shifts every derived
		// offset right by the length of a comment that is not there any more.
		int32 PayloadOffset = Span.Offset + 1;
		{
			const FString& SourceText = Source.GetText();
			while (PayloadOffset < Span.End() && FChar::IsWhitespace(SourceText[PayloadOffset]))
			{
				++PayloadOffset;
			}
		}

		if (Word.Equals(TEXT("include"), ESearchCase::CaseSensitive))
		{
			FString Path;
			if (Rest.Len() >= 2 && Rest[0] == TEXT('"') && Rest[Rest.Len() - 1] == TEXT('"'))
			{
				Path = Rest.Mid(1, Rest.Len() - 2);
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH3203"),
					Span,
					LOCTEXT("MalformedInclude", "'#include' needs a quoted path: #include \"/Game/Shared/Common.dsh\"."));
				return nullptr;
			}

			TUniquePtr<FIncludeDecl> Include = MakeUnique<FIncludeDecl>();
			Include->Doc = MoveTemp(Doc);
			Include->Span = Span;
			Include->Path = Path;
			Include->PathSpan.Offset = PayloadOffset + Token.Text.Len() - Rest.Len();
			Include->PathSpan.Length = Rest.Len();
			Include->PathSpan.Line = Span.Line;
			Include->PathSpan.Column = Span.Column + (Include->PathSpan.Offset - Span.Offset);
			Include->bImportSpelling = false;
			return Include;
		}

		if (!Word.Equals(TEXT("pragma"), ESearchCase::CaseSensitive))
		{
			Diagnostics.Error(
				TEXT("DSH3201"),
				Span,
				FText::Format(
					LOCTEXT("StrayDirective", "Preprocessor directive '#{0}' reached the parser; only '#pragma' and '#include' belong here, and '#if' / '#define' lines must be resolved by the preprocessor first."),
					FText::FromString(Word)));
			return nullptr;
		}

		FString Name;
		FString Arguments;
		SplitPragmaName(Rest, Name, Arguments);

		TUniquePtr<FPragmaDecl> Pragma = MakeUnique<FPragmaDecl>();
		Pragma->Doc = MoveTemp(Doc);
		Pragma->Span = Span;
		Pragma->Name = Name;

		if (Name.IsEmpty())
		{
			Diagnostics.Error(TEXT("DSH3202"), Span, LOCTEXT("PragmaWithoutName", "'#pragma' needs a name: material, layout, region or endregion."));
			return nullptr;
		}

		if (Name.Equals(TEXT("region"), ESearchCase::CaseSensitive))
		{
			Pragma->PragmaKind = EPragmaKind::Region;
			Pragma->Text = Arguments;
			return Pragma;
		}
		if (Name.Equals(TEXT("endregion"), ESearchCase::CaseSensitive))
		{
			Pragma->PragmaKind = EPragmaKind::EndRegion;
			Pragma->Text = Arguments;
			return Pragma;
		}

		const bool bMaterial = Name.Equals(TEXT("material"), ESearchCase::CaseSensitive);
		const bool bLayout = Name.Equals(TEXT("layout"), ESearchCase::CaseSensitive);
		if (!bMaterial && !bLayout)
		{
			Pragma->PragmaKind = EPragmaKind::Unknown;
			Pragma->Text = Arguments;
			return Pragma;
		}
		Pragma->PragmaKind = bMaterial ? EPragmaKind::Material : EPragmaKind::Layout;

		// `(` Key = Value {, Key = Value} `)` -- a bare word is positional (`#pragma layout(Node, ...)`).
		FPragmaScanner Scanner(Arguments);
		const int32 ArgumentsOffset = PayloadOffset + (Token.Text.Len() - Arguments.Len());
		auto MakeArgumentSpan = [&](const int32 Start, const int32 End)
		{
			FLangSpan ArgumentSpan;
			ArgumentSpan.Offset = ArgumentsOffset + Start;
			ArgumentSpan.Length = FMath::Max(0, End - Start);
			ArgumentSpan.Line = Span.Line;
			ArgumentSpan.Column = Span.Column + (ArgumentSpan.Offset - Span.Offset);
			return ArgumentSpan;
		};
		auto FailPragma = [&](const FText& Why)
		{
			Diagnostics.Error(
				TEXT("DSH3202"),
				Span,
				FText::Format(LOCTEXT("MalformedPragma", "Malformed '#pragma {0}': {1}"), FText::FromString(Name), Why));
			return FDeclPtr();
		};

		if (!Scanner.Accept(TEXT('(')))
		{
			return FailPragma(LOCTEXT("PragmaExpectedOpen", "expected '(' after the pragma name."));
		}

		if (!Scanner.Accept(TEXT(')')))
		{
			while (true)
			{
				FString First;
				bool bFirstQuoted = false;
				int32 FirstStart = 0;
				if (!Scanner.ReadValue(First, bFirstQuoted, FirstStart))
				{
					return FailPragma(LOCTEXT("PragmaExpectedKey", "expected a key or a value."));
				}

				FPragmaArgument Argument;
				if (Scanner.Accept(TEXT('=')))
				{
					if (bFirstQuoted)
					{
						return FailPragma(LOCTEXT("PragmaQuotedKey", "a key cannot be a quoted string."));
					}
					FString Value;
					bool bValueQuoted = false;
					int32 ValueStart = 0;
					if (!Scanner.ReadValue(Value, bValueQuoted, ValueStart))
					{
						return FailPragma(FText::Format(LOCTEXT("PragmaExpectedValue", "expected a value after '{0} ='."), FText::FromString(First)));
					}
					Argument.Key = First;
					Argument.Value = Value;
					Argument.bQuoted = bValueQuoted;
					Argument.Span = MakeArgumentSpan(FirstStart, Scanner.Index);
				}
				else
				{
					if (bMaterial)
					{
						return FailPragma(FText::Format(LOCTEXT("PragmaPositionalInMaterial", "'{0}' needs a value: write '{0} = ...'."), FText::FromString(First)));
					}
					Argument.Value = First;
					Argument.bQuoted = bFirstQuoted;
					Argument.Span = MakeArgumentSpan(FirstStart, Scanner.Index);
				}
				Pragma->Arguments.Add(MoveTemp(Argument));

				if (Scanner.Accept(TEXT(',')))
				{
					continue;
				}
				if (Scanner.Accept(TEXT(')')))
				{
					break;
				}
				return FailPragma(LOCTEXT("PragmaExpectedSeparator", "expected ',' or ')'."));
			}
		}

		Scanner.SkipWhitespace();
		if (!Scanner.AtEnd())
		{
			return FailPragma(LOCTEXT("PragmaTrailingText", "unexpected text after ')'."));
		}

		return Pragma;
	}

	FDeclPtr FLangParser::ParseImport(FDocBlock&& Doc)
	{
		const int32 StartIndex = GetTokenIndex();
		Advance(); // import

		if (!Check(ELangTokenKind::StringLiteral))
		{
			Diagnostics.Error(
				TEXT("DSH3203"),
				Current().Span,
				FText::Format(LOCTEXT("ImportNeedsPath", "Expected a quoted path after 'import', found {0}."), DescribeToken(Current())));
			return nullptr;
		}

		const FLangToken& PathToken = Advance();
		TUniquePtr<FIncludeDecl> Include = MakeUnique<FIncludeDecl>();
		Include->Doc = MoveTemp(Doc);
		Include->Path = PathToken.Text;
		Include->PathSpan = PathToken.Span;
		Include->bImportSpelling = true;

		if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH3216"), LOCTEXT("ExpectedSemicolonAfterImport", "';' after the import path")))
		{
			return nullptr;
		}

		Include->Span = SpanFrom(StartIndex);
		return Include;
	}

	// ---------------------------------------------------------------------------------------------
	// struct
	// ---------------------------------------------------------------------------------------------

	FDeclPtr FLangParser::ParseStructDecl(FDocBlock&& Doc)
	{
		const int32 StartIndex = GetTokenIndex();
		Advance(); // struct

		TUniquePtr<FStructDecl> Struct = MakeUnique<FStructDecl>();
		Struct->Doc = MoveTemp(Doc);

		if (!ExpectIdentifier(Struct->Name, Struct->NameSpan, TEXT("DSH3205"), LOCTEXT("ExpectedStructName", "a name after 'struct'")))
		{
			return nullptr;
		}
		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH3217"), LOCTEXT("ExpectedStructOpen", "'{' after the struct name")))
		{
			return nullptr;
		}

		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				FailAtEnd(FText::Format(LOCTEXT("WhileParsingStruct", "struct '{0}'"), FText::FromString(Struct->Name)));
				return nullptr;
			}

			FStructField Field;
			const int32 FieldStart = GetTokenIndex();
			ParseDocBlock(Field.Doc);
			if (Check(ELangTokenKind::RightBrace))
			{
				// A `///` block with no field after it.
				if (!Field.Doc.IsEmpty())
				{
					Diagnostics.Warning(TEXT("DSH3221"), Field.Doc.Span, LOCTEXT("OrphanDocBlockInStruct", "This '///' block is not followed by a field and is ignored."));
				}
				break;
			}

			if (!ParseType(Field.Type))
			{
				return nullptr;
			}
			if (!ExpectIdentifier(Field.Name, Field.NameSpan, TEXT("DSH3205"), LOCTEXT("ExpectedFieldName", "a field name")))
			{
				return nullptr;
			}
			if (!ParseArrayDimensions(Field.ArrayDimensions))
			{
				return nullptr;
			}
			if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH3216"), LOCTEXT("ExpectedSemicolonAfterField", "';' after the field")))
			{
				return nullptr;
			}
			Field.Span = SpanFrom(FieldStart);
			Struct->Fields.Add(MoveTemp(Field));
		}

		Advance(); // }
		if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH3216"), LOCTEXT("ExpectedSemicolonAfterStruct", "';' after the closing '}' of the struct")))
		{
			return nullptr;
		}

		Struct->Span = SpanFrom(StartIndex);
		return Struct;
	}

	// ---------------------------------------------------------------------------------------------
	// Functions and variables
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::ParseParameterList(TArray<FParam>& OutParams)
	{
		if (Match(ELangTokenKind::RightParen))
		{
			return true;
		}

		while (true)
		{
			FParam Param;
			const int32 ParamStart = GetTokenIndex();

			if (MatchKeyword(ELangKeyword::In))
			{
				Param.Direction = EParamDirection::In;
			}
			else if (MatchKeyword(ELangKeyword::Out))
			{
				Param.Direction = EParamDirection::Out;
			}
			else if (MatchKeyword(ELangKeyword::InOut))
			{
				Param.Direction = EParamDirection::InOut;
			}

			if (!ParseType(Param.Type))
			{
				return false;
			}
			if (!ExpectIdentifier(Param.Name, Param.NameSpan, TEXT("DSH3205"), LOCTEXT("ExpectedParameterName", "a parameter name")))
			{
				return false;
			}
			if (!ParseArrayDimensions(Param.ArrayDimensions))
			{
				return false;
			}

			if (Match(ELangTokenKind::Assign))
			{
				const FLangSpan DefaultStart = Current().Span;
				Param.Default = Check(ELangTokenKind::LeftBrace) ? ParseInitializerList() : ParseExpression();
				if (!Param.Default)
				{
					return false;
				}
				if (Param.Direction == EParamDirection::Out)
				{
					return Diagnostics.Error(
						TEXT("DSH3214"),
						FLangSpan::Join(DefaultStart, Previous().Span),
						FText::Format(LOCTEXT("DefaultOnOutParameter", "Parameter '{0}' is 'out' and cannot have a default value; only inputs are optional."), FText::FromString(Param.Name)));
				}
			}

			Param.Span = SpanFrom(ParamStart);
			OutParams.Add(MoveTemp(Param));

			if (Match(ELangTokenKind::Comma))
			{
				continue;
			}
			return Expect(ELangTokenKind::RightParen, TEXT("DSH3218"), LOCTEXT("ExpectedParameterListClose", "')' to close the parameter list"));
		}
	}

	bool FLangParser::CaptureRawBody(FString& OutRaw, FLangSpan& OutSpan)
	{
		if (!Check(ELangTokenKind::LeftBrace))
		{
			return Expect(ELangTokenKind::LeftBrace, TEXT("DSH3206"), LOCTEXT("ExpectedBodyOpen", "'{' to open the function body"));
		}

		const FLangToken& Open = Advance();
		int32 Depth = 1;
		while (Depth > 0)
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileParsingRawBody", "a function body"));
			}
			const FLangToken& Token = Advance();
			if (Token.Kind == ELangTokenKind::LeftBrace)
			{
				++Depth;
			}
			else if (Token.Kind == ELangTokenKind::RightBrace)
			{
				--Depth;
			}
		}

		const FLangToken& Close = Previous();
		OutSpan = FLangSpan::Join(Open.Span, Close.Span);
		// Strings and comments cannot fool this: the lexer already made each string one token and
		// dropped the comments, so a `}` inside either never reaches the depth count above. The
		// text between the braces is sliced from the source, comments and all, because the shader
		// compiler is the one reading it.
		OutRaw = Source.GetText().Mid(Open.Span.End(), Close.Span.Offset - Open.Span.End());
		return true;
	}

	FDeclPtr FLangParser::ParseFunctionOrVariableDecl(FDocBlock&& Doc)
	{
		const int32 StartIndex = GetTokenIndex();

		// -- prefix keywords
		EStorageClass Storage = EStorageClass::None;
		EFunctionLinkage Linkage = EFunctionLinkage::Internal;
		bool bSawStatic = false;
		bool bSawConst = false;
		bool bSawUniform = false;
		bool bSawLinkage = false;

		bool bPrefixDone = false;
		while (!bPrefixDone && Check(ELangTokenKind::Keyword))
		{
			const FLangToken& Token = Current();
			bool bConflict = false;

			switch (Token.Keyword)
			{
			case ELangKeyword::Uniform:
				bConflict = bSawUniform || bSawStatic || bSawConst || bSawLinkage;
				bSawUniform = true;
				break;
			case ELangKeyword::Static:
				bConflict = bSawStatic || bSawUniform || bSawLinkage;
				bSawStatic = true;
				break;
			case ELangKeyword::Const:
				bConflict = bSawConst || bSawUniform || bSawLinkage;
				bSawConst = true;
				break;
			case ELangKeyword::Extern:
				bConflict = bSawLinkage || bSawUniform || bSawStatic || bSawConst;
				bSawLinkage = true;
				Linkage = EFunctionLinkage::Extern;
				break;
			case ELangKeyword::Export:
				bConflict = bSawLinkage || bSawUniform || bSawStatic || bSawConst;
				bSawLinkage = true;
				Linkage = EFunctionLinkage::Export;
				break;
			default:
				// Not a declaration prefix (`if`, `return`, ...): let the type parser complain.
				bPrefixDone = true;
				break;
			}

			if (bPrefixDone)
			{
				break;
			}
			if (bConflict)
			{
				Diagnostics.Error(
					TEXT("DSH3213"),
					Token.Span,
					FText::Format(
						LOCTEXT("BadStorageCombination", "'{0}' cannot be combined with the keywords before it; a declaration is 'uniform', 'static const', 'static', 'const', 'extern' or 'export', not a mix."),
						FText::FromString(Token.Text)));
				return nullptr;
			}
			Advance();
		}

		if (bSawUniform)
		{
			Storage = EStorageClass::Uniform;
		}
		else if (bSawStatic && bSawConst)
		{
			Storage = EStorageClass::StaticConst;
		}
		else if (bSawStatic)
		{
			Storage = EStorageClass::Static;
		}
		else if (bSawConst)
		{
			Storage = EStorageClass::Const;
		}

		// -- the 1.x words: an honest "not yet" beats "expected a declaration name, found 'float'".
		if (Check(ELangTokenKind::Identifier) && IsLegacyDeclarationWord(Current().Text))
		{
			Diagnostics.Error(
				TEXT("DSH3222"),
				Current().Span,
				FText::Format(
					LOCTEXT("LegacyDeclarationNotYet", "'{0}' is a 1.x declaration; the 2.0 front end does not parse it yet. Keep it in a .dsm/.dsf/.dsh compiled by the 1.x front end."),
					FText::FromString(Current().Text)));
			return nullptr;
		}

		// -- type and name
		FTypeRef Type;
		if (!ParseType(Type))
		{
			return nullptr;
		}

		FString Name;
		FLangSpan NameSpan;
		if (!ExpectIdentifier(Name, NameSpan, TEXT("DSH3205"), LOCTEXT("ExpectedDeclarationName", "a declaration name")))
		{
			return nullptr;
		}

		// -- function
		if (Check(ELangTokenKind::LeftParen))
		{
			if (Storage != EStorageClass::None)
			{
				Diagnostics.Error(
					TEXT("DSH3213"),
					SpanFrom(StartIndex),
					FText::Format(LOCTEXT("StorageOnFunction", "'{0}' is a function; 'uniform', 'static' and 'const' apply to variables only."), FText::FromString(Name)));
				return nullptr;
			}

			const bool bCustom = Doc.Has(TEXT("custom"));

			TUniquePtr<FFunctionDecl> Function = MakeUnique<FFunctionDecl>();
			Function->Doc = MoveTemp(Doc);
			Function->Linkage = Linkage;
			Function->ReturnType = Type;
			Function->Name = Name;
			Function->NameSpan = NameSpan;

			Advance(); // (
			if (!ParseParameterList(Function->Params))
			{
				return nullptr;
			}

			if (Check(ELangTokenKind::LeftBrace))
			{
				if (Linkage == EFunctionLinkage::Extern)
				{
					Diagnostics.Error(
						TEXT("DSH3207"),
						Current().Span,
						FText::Format(LOCTEXT("ExternWithBody", "'{0}' is 'extern' and binds to an existing asset, so it cannot have a body; write a prototype ending in ';'."), FText::FromString(Name)));
					SkipBalancedBraces();
					return nullptr;
				}

				if (bCustom)
				{
					if (!CaptureRawBody(Function->RawBody, Function->BodySpan))
					{
						return nullptr;
					}
					Function->bOpaqueBody = true;
				}
				else
				{
					Function->Body = ParseBlock();
					if (!Function->Body)
					{
						return nullptr;
					}
				}
			}
			else if (Match(ELangTokenKind::Semicolon))
			{
				if (Linkage != EFunctionLinkage::Extern)
				{
					Diagnostics.Error(
						TEXT("DSH3208"),
						NameSpan,
						FText::Format(LOCTEXT("FunctionWithoutBody", "'{0}' has no body. Only an 'extern' prototype may end in ';'; a function you define needs '`{...`}'."), FText::FromString(Name)));
					return nullptr;
				}
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH3206"),
					Current().Span,
					FText::Format(LOCTEXT("ExpectedBodyOrSemicolon", "Expected '`{' or ';' after the parameter list of '{0}', found {1}."), FText::FromString(Name), DescribeToken(Current())));
				return nullptr;
			}

			Function->Span = SpanFrom(StartIndex);
			return Function;
		}

		// -- variable
		if (Linkage != EFunctionLinkage::Internal)
		{
			Diagnostics.Error(
				TEXT("DSH3213"),
				SpanFrom(StartIndex),
				FText::Format(LOCTEXT("LinkageOnVariable", "'{0}' is a variable; 'extern' and 'export' apply to functions only."), FText::FromString(Name)));
			return nullptr;
		}

		TUniquePtr<FVariableDecl> Variable = MakeUnique<FVariableDecl>();
		Variable->Doc = MoveTemp(Doc);
		Variable->Storage = Storage;
		Variable->Type = Type;
		Variable->Declarator.Name = Name;
		Variable->Declarator.NameSpan = NameSpan;

		const int32 DeclaratorStart = GetTokenIndex() - 1;
		if (!ParseArrayDimensions(Variable->Declarator.ArrayDimensions))
		{
			return nullptr;
		}
		if (Match(ELangTokenKind::Assign))
		{
			Variable->Declarator.Initializer = Check(ELangTokenKind::LeftBrace) ? ParseInitializerList() : ParseExpression();
			if (!Variable->Declarator.Initializer)
			{
				return nullptr;
			}
		}
		Variable->Declarator.Span = SpanFrom(DeclaratorStart);

		if (Check(ELangTokenKind::Comma))
		{
			// `uniform float a, b;` -- the module loop collects the rest (ParseTrailingVariableDeclarators).
			Variable->Span = SpanFrom(StartIndex);
			return Variable;
		}

		if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH3216"), LOCTEXT("ExpectedSemicolonAfterVariable", "';' after the declaration")))
		{
			return nullptr;
		}

		Variable->Span = SpanFrom(StartIndex);
		return Variable;
	}

	bool ParseTrailingVariableDeclarators(FLangParser& Parser, const FVariableDecl& First, TArray<FDeclPtr>& OutDecls)
	{
		while (Parser.Match(ELangTokenKind::Comma))
		{
			const int32 DeclaratorStart = Parser.GetTokenIndex();

			TUniquePtr<FVariableDecl> Variable = MakeUnique<FVariableDecl>();
			Variable->Doc = First.Doc;
			Variable->Storage = First.Storage;
			Variable->Type = First.Type;

			if (!Parser.ExpectIdentifier(Variable->Declarator.Name, Variable->Declarator.NameSpan, TEXT("DSH3205"), LOCTEXT("ExpectedNextDeclarator", "another name after ','")))
			{
				Parser.SkipToDeclarationBoundary();
				return false;
			}
			if (!Parser.ParseArrayDimensions(Variable->Declarator.ArrayDimensions))
			{
				Parser.SkipToDeclarationBoundary();
				return false;
			}
			if (Parser.Match(ELangTokenKind::Assign))
			{
				Variable->Declarator.Initializer = Parser.Check(ELangTokenKind::LeftBrace) ? Parser.ParseInitializerList() : Parser.ParseExpression();
				if (!Variable->Declarator.Initializer)
				{
					Parser.SkipToDeclarationBoundary();
					return false;
				}
			}
			Variable->Declarator.Span = Parser.SpanFrom(DeclaratorStart);
			Variable->Span = Variable->Declarator.Span;
			OutDecls.Add(MoveTemp(Variable));
		}

		if (!Parser.Expect(ELangTokenKind::Semicolon, TEXT("DSH3216"), LOCTEXT("ExpectedSemicolonAfterDeclarators", "';' after the declaration")))
		{
			Parser.SkipToDeclarationBoundary();
			return false;
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Dispatch
	// ---------------------------------------------------------------------------------------------

	FDeclPtr FLangParser::ParseDeclaration()
	{
		FDocBlock Doc;
		ParseDocBlock(Doc);

		// A stray `;` at file scope is nothing; skip it and go on to whatever follows. The doc block
		// (if any) still belongs to the next real declaration.
		while (Match(ELangTokenKind::Semicolon))
		{
			ParseDocBlock(Doc);
		}

		if (AtEnd())
		{
			if (!Doc.IsEmpty())
			{
				Diagnostics.Warning(TEXT("DSH3221"), Doc.Span, LOCTEXT("OrphanDocBlock", "This '///' block is not followed by a declaration and is ignored."));
			}
			return nullptr;
		}

		const FLangToken& Token = Current();
		switch (Token.Kind)
		{
		case ELangTokenKind::Directive:
			return ParseDirective(MoveTemp(Doc));

		case ELangTokenKind::Keyword:
			switch (Token.Keyword)
			{
			case ELangKeyword::Import:
				return ParseImport(MoveTemp(Doc));
			case ELangKeyword::Struct:
				return ParseStructDecl(MoveTemp(Doc));
			case ELangKeyword::Uniform:
			case ELangKeyword::Static:
			case ELangKeyword::Const:
			case ELangKeyword::Extern:
			case ELangKeyword::Export:
				return ParseFunctionOrVariableDecl(MoveTemp(Doc));
			default:
				break;
			}
			break;

		case ELangTokenKind::Identifier:
			return ParseFunctionOrVariableDecl(MoveTemp(Doc));

		default:
			break;
		}

		Diagnostics.Error(
			TEXT("DSH3200"),
			Token.Span,
			FText::Format(LOCTEXT("UnexpectedAtFileScope", "Unexpected {0} at file scope; expected a declaration, '#pragma', '#include' or 'import'."), DescribeToken(Token)));
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
