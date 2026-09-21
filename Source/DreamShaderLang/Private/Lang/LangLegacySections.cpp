// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The legacy (1.x) front end, sections: `Properties`, `Settings`, `Outputs`, `Inputs` / `Outputs` of a
// function block, and `Layout`.
//
// 1.x cut a section body into strings on `;` and parsed each string with its own little splitter. Here the
// section bodies are read from the shared token stream instead: statement boundaries are `;` at depth zero,
// the declaration shapes are read token by token, and a value (a default, a binding source, a pin source)
// is handed to the ordinary expression parser over exactly its own tokens (ParseLegacyExpressionRange), so
// the `[...]` metadata that follows a default is never read as an index.
//
// Every node made here carries the span of the 1.x statement it came from: the trivia pass puts a comment
// written next to a Properties line onto the uniform that line became, wherever the uniform ends up.

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangInstanceSource.h"
#include "Lang/LangLegacy.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.LegacySections"

namespace UE::DreamShader::Lang::Private
{
	namespace LegacySections
	{
		// -----------------------------------------------------------------------------------------
		// Token helpers
		// -----------------------------------------------------------------------------------------

		static bool TokenIsWord(const FLangToken& Token, const TCHAR* Word)
		{
			return (Token.Kind == ELangTokenKind::Identifier || Token.Kind == ELangTokenKind::Keyword)
				&& Token.Text.Equals(Word, ESearchCase::IgnoreCase);
		}

		static bool TokenIsName(const FLangToken& Token)
		{
			return Token.Kind == ELangTokenKind::Identifier;
		}

		/**
		 * Advances over one value: tokens up to (not including) the first token at depth zero whose kind is in
		 * Stops, or the `}` / end of file that closes the enclosing section. Returns [First, End).
		 */
		static void SkipLegacyValue(FLangParser& Parser, const TArray<ELangTokenKind>& Stops, int32& OutFirst, int32& OutEnd)
		{
			OutFirst = Parser.GetTokenIndex();
			int32 Depth = 0;
			while (!Parser.AtEnd())
			{
				const ELangTokenKind Kind = Parser.Current().Kind;
				if (Depth == 0 && (Stops.Contains(Kind) || Kind == ELangTokenKind::RightBrace))
				{
					break;
				}
				if (Kind == ELangTokenKind::LeftParen || Kind == ELangTokenKind::LeftBracket || Kind == ELangTokenKind::LeftBrace)
				{
					++Depth;
				}
				else if (Kind == ELangTokenKind::RightParen || Kind == ELangTokenKind::RightBracket || Kind == ELangTokenKind::RightBrace)
				{
					Depth = FMath::Max(0, Depth - 1);
				}
				Parser.Advance();
			}
			OutEnd = Parser.GetTokenIndex();
		}

		/** The source text of tokens [First, End), as written. */
		static FString SliceLegacyTokens(FLangParser& Parser, const int32 First, const int32 End)
		{
			if (End <= First)
			{
				return FString();
			}
			const int32 Saved = Parser.GetTokenIndex();
			Parser.SetTokenIndex(First);
			const FLangSpan FirstSpan = Parser.Current().Span;
			Parser.SetTokenIndex(End - 1);
			const FLangSpan LastSpan = Parser.Current().Span;
			Parser.SetTokenIndex(Saved);
			return Parser.Slice(FLangSpan::Join(FirstSpan, LastSpan)).TrimStartAndEnd();
		}

		static FLangSpan SpanOfLegacyTokens(FLangParser& Parser, const int32 First, const int32 End)
		{
			const int32 Saved = Parser.GetTokenIndex();
			Parser.SetTokenIndex(First);
			FLangSpan Span = Parser.Current().Span;
			if (End - 1 > First)
			{
				Parser.SetTokenIndex(End - 1);
				Span = FLangSpan::Join(Span, Parser.Current().Span);
			}
			Parser.SetTokenIndex(Saved);
			return Span;
		}

		/** After a broken statement: past the next `;` at depth zero, or stopping before the section's `}`. */
		static void SkipLegacySectionStatement(FLangParser& Parser)
		{
			int32 Depth = 0;
			while (!Parser.AtEnd())
			{
				const ELangTokenKind Kind = Parser.Current().Kind;
				if (Depth == 0 && Kind == ELangTokenKind::RightBrace)
				{
					return;
				}
				if (Kind == ELangTokenKind::LeftParen || Kind == ELangTokenKind::LeftBracket || Kind == ELangTokenKind::LeftBrace)
				{
					++Depth;
				}
				else if (Kind == ELangTokenKind::RightParen || Kind == ELangTokenKind::RightBracket || Kind == ELangTokenKind::RightBrace)
				{
					Depth = FMath::Max(0, Depth - 1);
				}
				Parser.Advance();
				if (Depth == 0 && Parser.Previous().Kind == ELangTokenKind::Semicolon)
				{
					return;
				}
			}
		}

		/** A 1.x whole number (`LexTryParseString<int32>`): optional sign, digits. */
		static bool TryParseLegacyInteger(const FString& Text, int32& OutValue)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.IsEmpty() || !FCString::IsNumeric(*Trimmed) || Trimmed.Contains(TEXT(".")) || Trimmed.Contains(TEXT("e"), ESearchCase::IgnoreCase))
			{
				return false;
			}
			OutValue = FCString::Atoi(*Trimmed);
			return true;
		}

		static bool TryParseLegacyScalar(const FString& Text)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("false"), ESearchCase::IgnoreCase))
			{
				return true;
			}
			FString Number = Trimmed;
			if (Number.EndsWith(TEXT("f"), ESearchCase::IgnoreCase))
			{
				Number.LeftChopInline(1);
			}
			return !Number.IsEmpty() && FCString::IsNumeric(*Number);
		}

		// -----------------------------------------------------------------------------------------
		// `[ Key = Value; Slider(a, b), ... ]`
		// -----------------------------------------------------------------------------------------

		struct FLegacyMetadataEntry
		{
			/** As written. */
			FString Key;
			/** As written, quotes included; for `Slider(a, b)`, "a, b". */
			FString Value;
			FLangSpan Span;
			bool bSlider = false;
		};

		/** At `[`. Reports and returns false on a malformed entry; the cursor is then past the `]` or at the section end. */
		static bool ParseLegacyMetadata(FLangParser& Parser, TArray<FLegacyMetadataEntry>& OutEntries)
		{
			FLangDiagnosticSink& Diagnostics = Parser.GetDiagnostics();
			Parser.Advance(); // [

			bool bOk = true;
			while (!Parser.Check(ELangTokenKind::RightBracket))
			{
				if (Parser.AtEnd() || Parser.Check(ELangTokenKind::RightBrace))
				{
					return Diagnostics.Error(
						TEXT("DSH3255"),
						Parser.Current().Span,
						FText::Format(LOCTEXT("MetadataUnclosed", "Expected ']' to close the metadata block, found {0}."), Parser.DescribeToken(Parser.Current())));
				}
				if (Parser.Match(ELangTokenKind::Semicolon) || Parser.Match(ELangTokenKind::Comma))
				{
					continue;
				}

				const int32 StartIndex = Parser.GetTokenIndex();
				if (!TokenIsName(Parser.Current()))
				{
					Diagnostics.Error(
						TEXT("DSH3255"),
						Parser.Current().Span,
						FText::Format(LOCTEXT("MetadataExpectedKey", "Expected a metadata key such as 'Group', found {0}."), Parser.DescribeToken(Parser.Current())));
					bOk = false;
					int32 First = 0;
					int32 End = 0;
					SkipLegacyValue(Parser, { ELangTokenKind::Semicolon, ELangTokenKind::Comma, ELangTokenKind::RightBracket }, First, End);
					if (End == First)
					{
						Parser.Advance();
					}
					continue;
				}

				FLegacyMetadataEntry Entry;
				Entry.Key = Parser.Advance().Text;

				if (Entry.Key.Equals(TEXT("Slider"), ESearchCase::IgnoreCase) && Parser.Check(ELangTokenKind::LeftParen))
				{
					Parser.Advance(); // (
					int32 MinFirst = 0;
					int32 MinEnd = 0;
					SkipLegacyValue(Parser, { ELangTokenKind::Comma, ELangTokenKind::RightParen }, MinFirst, MinEnd);
					const FString MinText = SliceLegacyTokens(Parser, MinFirst, MinEnd);
					FString MaxText;
					if (Parser.Match(ELangTokenKind::Comma))
					{
						int32 MaxFirst = 0;
						int32 MaxEnd = 0;
						SkipLegacyValue(Parser, { ELangTokenKind::RightParen }, MaxFirst, MaxEnd);
						MaxText = SliceLegacyTokens(Parser, MaxFirst, MaxEnd);
					}
					const bool bClosed = Parser.Match(ELangTokenKind::RightParen);
					Entry.Span = Parser.SpanFrom(StartIndex);
					if (!bClosed || !TryParseLegacyScalar(MinText) || !TryParseLegacyScalar(MaxText))
					{
						Diagnostics.Error(
							TEXT("DSH3257"),
							Entry.Span,
							FText::Format(
								LOCTEXT("SliderMalformed", "Expected 'Slider(min, max)' with two numbers, found '{0}'."),
								FText::FromString(Parser.Slice(Entry.Span))));
						bOk = false;
						continue;
					}
					Entry.bSlider = true;
					Entry.Value = MinText + TEXT(", ") + MaxText;
					OutEntries.Add(MoveTemp(Entry));
					continue;
				}

				if (!Parser.Match(ELangTokenKind::Assign))
				{
					Diagnostics.Error(
						TEXT("DSH3255"),
						Parser.Current().Span,
						FText::Format(
							LOCTEXT("MetadataExpectedEquals", "Expected '=' after the metadata key '{0}', found {1}."),
							FText::FromString(Entry.Key),
							Parser.DescribeToken(Parser.Current())));
					bOk = false;
					int32 First = 0;
					int32 End = 0;
					SkipLegacyValue(Parser, { ELangTokenKind::Semicolon, ELangTokenKind::Comma, ELangTokenKind::RightBracket }, First, End);
					continue;
				}

				int32 ValueFirst = 0;
				int32 ValueEnd = 0;
				SkipLegacyValue(Parser, { ELangTokenKind::Semicolon, ELangTokenKind::Comma, ELangTokenKind::RightBracket }, ValueFirst, ValueEnd);
				Entry.Span = Parser.SpanFrom(StartIndex);
				if (ValueEnd == ValueFirst)
				{
					Diagnostics.Error(
						TEXT("DSH3255"),
						Entry.Span,
						FText::Format(LOCTEXT("MetadataExpectedValue", "Expected a value after '{0} =', found none."), FText::FromString(Entry.Key)));
					bOk = false;
					continue;
				}
				Entry.Value = SliceLegacyTokens(Parser, ValueFirst, ValueEnd);
				OutEntries.Add(MoveTemp(Entry));
			}
			Parser.Advance(); // ]

			// 1.x keys are one key whatever their case; `Slider(...)` is SliderMin and SliderMax at once.
			for (int32 Index = 0; Index < OutEntries.Num(); ++Index)
			{
				for (int32 Earlier = 0; Earlier < Index; ++Earlier)
				{
					const FLegacyMetadataEntry& A = OutEntries[Earlier];
					const FLegacyMetadataEntry& B = OutEntries[Index];
					const auto IsSliderKey = [](const FLegacyMetadataEntry& Entry) -> bool
					{
						return Entry.bSlider || Entry.Key.Equals(TEXT("SliderMin"), ESearchCase::IgnoreCase) || Entry.Key.Equals(TEXT("SliderMax"), ESearchCase::IgnoreCase);
					};
					const bool bSliderClash = (A.bSlider || B.bSlider) && IsSliderKey(A) && IsSliderKey(B);
					if (bSliderClash)
					{
						Diagnostics.Error(
							TEXT("DSH3257"),
							B.Span,
							LOCTEXT("SliderRepeated", "Expected the slider range once, found 'Slider(...)' together with another slider bound."));
						bOk = false;
					}
					else if (!A.bSlider && !B.bSlider && A.Key.Equals(B.Key, ESearchCase::IgnoreCase))
					{
						Diagnostics.Error(
							TEXT("DSH3256"),
							B.Span,
							FText::Format(LOCTEXT("MetadataRepeated", "Expected the metadata key '{0}' once, found it again."), FText::FromString(B.Key)));
						bOk = false;
					}
				}
			}
			return bOk;
		}

		// -----------------------------------------------------------------------------------------
		// Property types
		// -----------------------------------------------------------------------------------------

		enum class ELegacyPropertyForm : uint8
		{
			/** `uniform float` (ScalarParameter, compact scalar). */
			ScalarUniform,
			/** `/// @static uniform bool` (StaticBoolParameter). */
			StaticBoolUniform,
			/** `uniform floatN` (VectorParameter, compact vector). */
			VectorUniform,
			/** `/// @default ... uniform Texture2D` (TextureObjectParameter, compact texture). */
			TextureUniform,
			/** No uniform: one reflected call per use (StaticSwitchParameter and the input-bearing node types). */
			ExpandAtUse,
			/** A parameter node type 1.x knew and 2.0 has no spelling for. */
			Unsupported,
			/** Not a type 1.x knew. */
			Unknown,
		};

		static ELegacyPropertyForm ClassifyLegacyPropertyType(const FString& Token, FTypeRef& OutType)
		{
			auto Is = [&Token](const TCHAR* Candidate) -> bool
			{
				return Token.Equals(Candidate, ESearchCase::IgnoreCase);
			};

			if (Is(TEXT("ScalarParameter")))
			{
				FLangParser::ClassifyTypeName(TEXT("float"), OutType);
				return ELegacyPropertyForm::ScalarUniform;
			}
			if (Is(TEXT("StaticBoolParameter")))
			{
				FLangParser::ClassifyTypeName(TEXT("bool"), OutType);
				return ELegacyPropertyForm::StaticBoolUniform;
			}
			if (Is(TEXT("VectorParameter")))
			{
				FLangParser::ClassifyTypeName(TEXT("float4"), OutType);
				return ELegacyPropertyForm::VectorUniform;
			}
			if (Is(TEXT("TextureObjectParameter")))
			{
				FLangParser::ClassifyTypeName(TEXT("Texture2D"), OutType);
				return ELegacyPropertyForm::TextureUniform;
			}
			if (Is(TEXT("StaticSwitchParameter"))
				|| Is(TEXT("ChannelMaskParameter"))
				|| Is(TEXT("StaticComponentMaskParameter"))
				|| Is(TEXT("TextureSampleParameter2D"))
				|| Is(TEXT("TextureSampleParameter2DArray"))
				|| Is(TEXT("TextureSampleParameterCube"))
				|| Is(TEXT("TextureSampleParameterCubeArray"))
				|| Is(TEXT("TextureSampleParameterVolume"))
				|| Is(TEXT("TextureSampleParameterSubUV"))
				|| Is(TEXT("RuntimeVirtualTextureSampleParameter"))
				|| Is(TEXT("SparseVolumeTextureSampleParameter")))
			{
				return ELegacyPropertyForm::ExpandAtUse;
			}
			if (Is(TEXT("DoubleVectorParameter"))
				|| Is(TEXT("TextureCollectionParameter"))
				|| Is(TEXT("CurveAtlasRowParameter"))
				|| Is(TEXT("DynamicParameter"))
				|| Is(TEXT("FontSampleParameter"))
				|| Is(TEXT("SpriteTextureSampler"))
				|| Is(TEXT("SparseVolumeTextureObjectParameter")))
			{
				return ELegacyPropertyForm::Unsupported;
			}

			FLangParser::ClassifyLegacyTypeName(Token, OutType);
			switch (OutType.Category)
			{
			case ETypeCategory::Scalar:
				return ELegacyPropertyForm::ScalarUniform;
			case ETypeCategory::Vector:
				return ELegacyPropertyForm::VectorUniform;
			case ETypeCategory::Texture:
				return ELegacyPropertyForm::TextureUniform;
			case ETypeCategory::Void:
			case ETypeCategory::Matrix:
			case ETypeCategory::Sampler:
			case ETypeCategory::Material:
			case ETypeCategory::Substrate:
			case ETypeCategory::Named:
				return ELegacyPropertyForm::Unknown;
			}
			return ELegacyPropertyForm::Unknown;
		}

		/** One `///` directive for a synthesized declaration, canonical key. */
		static void AddLegacyDirective(FDocBlock& Doc, const FString& Key, const FString& Value, const FLangSpan& Span)
		{
			FDocDirective Directive;
			Directive.Key = Key;
			Directive.Value = Value;
			Directive.Span = Span;
			Doc.Directives.Add(MoveTemp(Directive));
		}

		/**
		 * The `///` directives a uniform gets from its 1.x metadata: Group/Category ->
		 * @group, Description/Desc/Tooltip -> @desc, SortPriority/Sort -> @sort, ParameterName -> @name,
		 * SamplerType -> @sampler, Slider(a,b) or both slider bounds -> @slider; every other key passes through
		 * under its lower-case spelling. False when SortPriority is not a whole number (reported).
		 */
		static bool MakeLegacyUniformDirectives(
			FLangDiagnosticSink& Diagnostics,
			const TArray<FLegacyMetadataEntry>& Entries,
			const FString& InheritedGroup,
			const bool bHasAutoSort,
			const int32 AutoSort,
			FDocBlock& OutDoc,
			bool& bOutHasSort)
		{
			bool bOk = true;
			bool bHasGroup = false;
			bOutHasSort = false;
			FString SliderMin;
			FString SliderMax;
			FLangSpan SliderSpan;

			for (const FLegacyMetadataEntry& Entry : Entries)
			{
				const FString Lower = Entry.Key.ToLower();
				const FString Value = LegacyAst::Unquote(Entry.Value);

				if (Entry.bSlider)
				{
					TArray<FString> Bounds;
					Entry.Value.ParseIntoArray(Bounds, TEXT(","), true);
					if (Bounds.Num() == 2)
					{
						AddLegacyDirective(OutDoc, TEXT("slider"), Bounds[0].TrimStartAndEnd() + TEXT(" ") + Bounds[1].TrimStartAndEnd(), Entry.Span);
					}
					continue;
				}
				if (Lower == TEXT("group") || Lower == TEXT("category"))
				{
					AddLegacyDirective(OutDoc, TEXT("group"), Value, Entry.Span);
					bHasGroup = true;
				}
				else if (Lower == TEXT("description") || Lower == TEXT("desc") || Lower == TEXT("tooltip"))
				{
					AddLegacyDirective(OutDoc, TEXT("desc"), Value, Entry.Span);
				}
				else if (Lower == TEXT("sortpriority") || Lower == TEXT("sort"))
				{
					int32 Sort = 0;
					if (!TryParseLegacyInteger(Value, Sort))
					{
						Diagnostics.Error(
							TEXT("DSH3258"),
							Entry.Span,
							FText::Format(LOCTEXT("SortNotInteger", "Expected 'SortPriority' to be a whole number, found '{0}'."), FText::FromString(Value)));
						bOk = false;
						continue;
					}
					AddLegacyDirective(OutDoc, TEXT("sort"), FString::FromInt(Sort), Entry.Span);
					bOutHasSort = true;
				}
				else if (Lower == TEXT("parametername"))
				{
					AddLegacyDirective(OutDoc, TEXT("name"), Value, Entry.Span);
				}
				else if (Lower == TEXT("samplertype"))
				{
					// The directive takes the enumerator without its prefix, as the reflected writer did.
					FString Sampler = Value;
					if (Sampler.StartsWith(TEXT("SAMPLERTYPE_"), ESearchCase::IgnoreCase))
					{
						Sampler.RightChopInline(12);
					}
					AddLegacyDirective(OutDoc, TEXT("sampler"), Sampler, Entry.Span);
				}
				else if (Lower == TEXT("slidermin"))
				{
					SliderMin = Value;
					SliderSpan = Entry.Span;
				}
				else if (Lower == TEXT("slidermax"))
				{
					SliderMax = Value;
					SliderSpan = Entry.Span;
				}
				else
				{
					AddLegacyDirective(OutDoc, Lower, Value, Entry.Span);
				}
			}

			if (!SliderMin.IsEmpty() && !SliderMax.IsEmpty())
			{
				AddLegacyDirective(OutDoc, TEXT("slider"), SliderMin + TEXT(" ") + SliderMax, SliderSpan);
			}
			else if (!SliderMin.IsEmpty())
			{
				AddLegacyDirective(OutDoc, TEXT("slidermin"), SliderMin, SliderSpan);
			}
			else if (!SliderMax.IsEmpty())
			{
				AddLegacyDirective(OutDoc, TEXT("slidermax"), SliderMax, SliderSpan);
			}

			if (!bHasGroup && !InheritedGroup.IsEmpty())
			{
				AddLegacyDirective(OutDoc, TEXT("group"), InheritedGroup, FLangSpan());
			}
			if (!bOutHasSort && bHasAutoSort)
			{
				AddLegacyDirective(OutDoc, TEXT("sort"), FString::FromInt(AutoSort), FLangSpan());
				bOutHasSort = true;
			}
			return bOk;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The sub-parser for values embedded in section syntax
	// ---------------------------------------------------------------------------------------------

	FExprPtr FLangParser::ParseLegacyExpressionRange(const int32 First, const int32 End)
	{
		if (End <= First || First < 0 || End > Tokens.Num())
		{
			return nullptr;
		}

		TArray<FLangToken> Range;
		Range.Reserve(End - First + 1);
		for (int32 ItemIndex = First; ItemIndex < End; ++ItemIndex)
		{
			Range.Add(Tokens[ItemIndex]);
		}
		FLangToken EndToken;
		EndToken.Kind = ELangTokenKind::EndOfFile;
		EndToken.Span = Source.MakeSpan(Tokens[End - 1].Span.End(), 0);
		Range.Add(MoveTemp(EndToken));

		FLangParser Sub(Source, MoveTemp(Range), ELangFrontend::Legacy, FileKind, Diagnostics);
		Sub.bKeepTrivia = bKeepTrivia;
		Sub.LegacyInfo = LegacyInfo;
		Sub.LegacyBlock = LegacyBlock;
		Sub.LegacyRenameDecl = LegacyRenameDecl;
		Sub.LegacySelectionGroupKeys = MoveTemp(LegacySelectionGroupKeys);

		FExprPtr Result = Sub.ParseStandaloneExpression();

		LegacySelectionGroupKeys = MoveTemp(Sub.LegacySelectionGroupKeys);
		LegacySynthesizedIndexExprs.Append(Sub.LegacySynthesizedIndexExprs);
		SkippedComments.Append(Sub.TakeSkippedComments());
		return Result;
	}

	// ---------------------------------------------------------------------------------------------
	// Properties
	// ---------------------------------------------------------------------------------------------

	namespace LegacySections
	{
		/** State of one Properties section across nested `Group("G") { }` blocks. */
		struct FLegacyPropertiesState
		{
			FLegacyBlockContext& Block;
			TArray<FDeclPtr>& OutDecls;
			int32 NextAutoSort = 0;
			bool bOk = true;

			FLegacyPropertiesState(FLegacyBlockContext& InBlock, TArray<FDeclPtr>& InOutDecls)
				: Block(InBlock)
				, OutDecls(InOutDecls)
			{
			}
		};

		static void ParseLegacyPropertyStatement(FLangParser& Parser, FLegacyPropertiesState& State, const FString& InheritedGroup);

		/** Items up to (not including) the `}` that closes the current Properties or Group body. */
		static void ParseLegacyPropertyItems(FLangParser& Parser, FLegacyPropertiesState& State, const FString& InheritedGroup)
		{
			FLangDiagnosticSink& Diagnostics = Parser.GetDiagnostics();
			while (!Parser.Check(ELangTokenKind::RightBrace))
			{
				if (Parser.AtEnd())
				{
					State.bOk = Parser.FailAtEnd(LOCTEXT("WhileProperties", "a Properties section"));
					return;
				}
				if (Parser.Match(ELangTokenKind::Semicolon))
				{
					continue;
				}
				if (Parser.Check(ELangTokenKind::DocComment))
				{
					Parser.RecordSkippedDocComment(Parser.Advance());
					continue;
				}

				// `Group("Name") { ... }`: members inherit the group and are numbered 0, 10, 20, ...
				if (TokenIsWord(Parser.Current(), TEXT("Group"))
					&& Parser.Peek(1).Kind == ELangTokenKind::LeftParen
					&& Parser.Peek(2).Kind == ELangTokenKind::StringLiteral
					&& Parser.Peek(3).Kind == ELangTokenKind::RightParen
					&& Parser.Peek(4).Kind == ELangTokenKind::LeftBrace)
				{
					const FLangSpan GroupSpan = Parser.Current().Span;
					const FString GroupName = Parser.Peek(2).Text.TrimStartAndEnd();
					for (int32 Step = 0; Step < 5; ++Step)
					{
						Parser.Advance();
					}
					if (GroupName.IsEmpty())
					{
						Diagnostics.Error(TEXT("DSH3260"), GroupSpan, LOCTEXT("GroupEmptyName", "Expected a name inside 'Group(\"...\")', found an empty string."));
						State.bOk = false;
					}

					const FString Composed = InheritedGroup.IsEmpty() ? GroupName : InheritedGroup + TEXT("|") + GroupName;
					ParseLegacyPropertyItems(Parser, State, Composed);
					if (!Parser.Match(ELangTokenKind::RightBrace))
					{
						State.bOk = Parser.FailAtEnd(LOCTEXT("WhileGroup", "a Group block"));
						return;
					}
					Parser.Match(ELangTokenKind::Semicolon);
					continue;
				}

				ParseLegacyPropertyStatement(Parser, State, InheritedGroup);
			}
		}

		static void ParseLegacyPropertyStatement(FLangParser& Parser, FLegacyPropertiesState& State, const FString& InheritedGroup)
		{
			FLangDiagnosticSink& Diagnostics = Parser.GetDiagnostics();
			FLegacyMigrationInfo* const Info = Parser.GetLegacyInfo();
			const int32 StartIndex = Parser.GetTokenIndex();

			auto Fail = [&Parser, &State]() -> void
			{
				State.bOk = false;
				SkipLegacySectionStatement(Parser);
			};

			// -- `const`
			bool bConst = false;
			if (TokenIsWord(Parser.Current(), TEXT("const")))
			{
				bConst = true;
				Parser.Advance();
				if (!TokenIsName(Parser.Current()))
				{
					Diagnostics.Error(
						TEXT("DSH3251"),
						Parser.Current().Span,
						FText::Format(LOCTEXT("ConstWithoutType", "Expected a property type after 'const', found {0}."), Parser.DescribeToken(Parser.Current())));
					Fail();
					return;
				}
			}

			// -- the type: a word, or `UE.Name(arguments)`
			const int32 TypeStart = Parser.GetTokenIndex();
			FString TypeToken;
			FString BuiltinName;
			FString BuiltinArguments;
			bool bBuiltin = false;
			if (TokenIsWord(Parser.Current(), TEXT("UE")) && Parser.Peek(1).Kind == ELangTokenKind::Dot && Parser.Peek(2).Kind == ELangTokenKind::Identifier)
			{
				Parser.Advance();
				Parser.Advance();
				BuiltinName = Parser.Advance().Text;
				bBuiltin = true;
				if (Parser.Match(ELangTokenKind::LeftParen))
				{
					int32 ArgumentsFirst = 0;
					int32 ArgumentsEnd = 0;
					SkipLegacyValue(Parser, { ELangTokenKind::RightParen }, ArgumentsFirst, ArgumentsEnd);
					BuiltinArguments = SliceLegacyTokens(Parser, ArgumentsFirst, ArgumentsEnd);
					if (!Parser.Match(ELangTokenKind::RightParen))
					{
						Diagnostics.Error(
							TEXT("DSH3259"),
							Parser.Current().Span,
							FText::Format(LOCTEXT("BuiltinUnclosed", "Expected ')' to close the arguments of 'UE.{0}', found {1}."), FText::FromString(BuiltinName), Parser.DescribeToken(Parser.Current())));
						Fail();
						return;
					}
				}
				TypeToken = TEXT("UE.") + BuiltinName;
			}
			else if (TokenIsName(Parser.Current()))
			{
				TypeToken = Parser.Advance().Text;
			}
			else
			{
				Diagnostics.Error(
					TEXT("DSH3250"),
					Parser.Current().Span,
					FText::Format(LOCTEXT("PropertyExpectedType", "Expected a property type and a name, found {0}."), Parser.DescribeToken(Parser.Current())));
				Fail();
				return;
			}
			const FLangSpan TypeSpan = Parser.SpanFrom(TypeStart);

			// -- the name
			if (!TokenIsName(Parser.Current()))
			{
				Diagnostics.Error(
					TEXT("DSH3250"),
					Parser.Current().Span,
					FText::Format(
						LOCTEXT("PropertyExpectedName", "Expected a property name after the type '{0}', found {1}."),
						FText::FromString(TypeToken),
						Parser.DescribeToken(Parser.Current())));
				Fail();
				return;
			}
			const FLangToken& NameToken = Parser.Advance();
			const FString Name = NameToken.Text;
			const FLangSpan NameSpan = NameToken.Span;

			// -- `= default`, `[metadata]`, `;`
			int32 DefaultFirst = INDEX_NONE;
			int32 DefaultEnd = INDEX_NONE;
			if (Parser.Match(ELangTokenKind::Assign))
			{
				SkipLegacyValue(Parser, { ELangTokenKind::Semicolon, ELangTokenKind::LeftBracket }, DefaultFirst, DefaultEnd);
				if (DefaultEnd == DefaultFirst)
				{
					Diagnostics.Error(
						TEXT("DSH3254"),
						Parser.Current().Span,
						FText::Format(LOCTEXT("DefaultMissing", "Expected a default value after '{0} =', found {1}."), FText::FromString(Name), Parser.DescribeToken(Parser.Current())));
					Fail();
					return;
				}
			}

			TArray<FLegacyMetadataEntry> Metadata;
			if (Parser.Check(ELangTokenKind::LeftBracket))
			{
				if (!ParseLegacyMetadata(Parser, Metadata))
				{
					State.bOk = false;
				}
			}

			if (!Parser.Match(ELangTokenKind::Semicolon) && !Parser.Check(ELangTokenKind::RightBrace))
			{
				Diagnostics.Error(
					TEXT("DSH3250"),
					Parser.Current().Span,
					FText::Format(LOCTEXT("PropertyExpectedEnd", "Expected ';' after the property '{0}', found {1}."), FText::FromString(Name), Parser.DescribeToken(Parser.Current())));
				Fail();
				return;
			}

			const FLangSpan StatementSpan = Parser.SpanFrom(StartIndex);
			const bool bHasDefault = DefaultFirst != INDEX_NONE;
			const FString DefaultText = bHasDefault ? SliceLegacyTokens(Parser, DefaultFirst, DefaultEnd) : FString();
			const FLangSpan DefaultSpan = bHasDefault ? SpanOfLegacyTokens(Parser, DefaultFirst, DefaultEnd) : StatementSpan;

			FLegacyProperty Property;
			Property.Name = Name;
			Property.NodeType = TypeToken;
			Property.DefaultText = DefaultText;
			Property.bConst = bConst;
			Property.Span = StatementSpan;
			for (const FLegacyMetadataEntry& Entry : Metadata)
			{
				Property.RawMetadata.Emplace(Entry.bSlider ? FString(TEXT("Slider")) : Entry.Key, Entry.Value);
			}

			auto RecordDeclaration = [Info, &Property]() -> void
			{
				if (!Info)
				{
					return;
				}
				FLegacyParameterDeclaration Declaration;
				Declaration.Name = Property.Name;
				Declaration.NodeType = Property.NodeType;
				Declaration.DefaultText = Property.DefaultText;
				Declaration.Metadata = Property.RawMetadata;
				Declaration.DeclarationSpan = Property.Span;
				Info->ParameterDeclarations.Add(MoveTemp(Declaration));
			};

			// -- UE builtin properties: a local at the head of every body that reads them
			if (bBuiltin)
			{
				if (bHasDefault)
				{
					Diagnostics.Error(
						TEXT("DSH3259"),
						DefaultSpan,
						FText::Format(
							LOCTEXT("BuiltinWithDefault", "Expected the arguments of 'UE.{0}' inside its parentheses, found a default value after '{1}'."),
							FText::FromString(BuiltinName),
							FText::FromString(Name)));
					State.bOk = false;
					return;
				}
				Property.DefaultText = BuiltinArguments;
				RecordDeclaration();
				State.Block.Properties.Add(MoveTemp(Property));
				return;
			}

			FTypeRef Type;
			const ELegacyPropertyForm Form = ClassifyLegacyPropertyType(TypeToken, Type);
			Type.Span = TypeSpan;

			if (Form == ELegacyPropertyForm::Unknown)
			{
				Diagnostics.Error(
					TEXT("DSH3252"),
					TypeSpan,
					FText::Format(LOCTEXT("PropertyUnknownType", "Expected a property type such as 'float', 'float4', 'Texture2D' or 'ScalarParameter', found '{0}'."), FText::FromString(TypeToken)));
				State.bOk = false;
				return;
			}
			// `const Texture2D Noise = Path(...)` is a texture nobody can override: a TextureObject node in 1.x, and what a
			// `static const` texture with a `/// @default` is in 2.0.
			if (Form == ELegacyPropertyForm::Unsupported
				|| (bConst && Form != ELegacyPropertyForm::ScalarUniform && Form != ELegacyPropertyForm::VectorUniform && Form != ELegacyPropertyForm::TextureUniform))
			{
				Diagnostics.Error(
					TEXT("DSH3253"),
					TypeSpan,
					FText::Format(
						LOCTEXT("PropertyNoForm", "Expected a property type with a 2.0 spelling, found '{0}{1}', which has none; move this material to a .dss file and write the node with UE.Expression."),
						FText::FromString(bConst ? FString(TEXT("const ")) : FString()),
						FText::FromString(TypeToken)));
				State.bOk = false;
				return;
			}

			// -- expanded at every use
			if (Form == ELegacyPropertyForm::ExpandAtUse)
			{
				const bool bStaticSwitch = TypeToken.Equals(TEXT("StaticSwitchParameter"), ESearchCase::IgnoreCase);
				if (bHasDefault && bStaticSwitch
					&& !(DefaultText.Equals(TEXT("true"), ESearchCase::IgnoreCase) || DefaultText.Equals(TEXT("false"), ESearchCase::IgnoreCase)))
				{
					Diagnostics.Error(
						TEXT("DSH3254"),
						DefaultSpan,
						FText::Format(LOCTEXT("SwitchDefault", "Expected 'true' or 'false' as the default of the static switch '{0}', found '{1}'."), FText::FromString(Name), FText::FromString(DefaultText)));
					State.bOk = false;
					return;
				}
				Property.bExpandAtUse = true;
				RecordDeclaration();
				State.Block.Properties.Add(MoveTemp(Property));
				return;
			}

			// -- a declaration
			TUniquePtr<FVariableDecl> Variable = MakeUnique<FVariableDecl>();
			Variable->bLegacy = true;
			Variable->Storage = bConst ? EStorageClass::StaticConst : EStorageClass::Uniform;
			Variable->Type = Type;
			Variable->Declarator.Name = Name;
			Variable->Declarator.NameSpan = NameSpan;
			Variable->Declarator.Span = StatementSpan;
			Variable->Span = StatementSpan;

			if (Type.Name.Equals(TypeToken, ESearchCase::CaseSensitive) == false
				&& Form != ELegacyPropertyForm::StaticBoolUniform
				&& !TypeToken.Equals(TEXT("ScalarParameter"), ESearchCase::IgnoreCase)
				&& !TypeToken.Equals(TEXT("VectorParameter"), ESearchCase::IgnoreCase)
				&& !TypeToken.Equals(TEXT("TextureObjectParameter"), ESearchCase::IgnoreCase))
			{
				Parser.RecordLegacyRename(FLegacyRename::EKind::TypeSpelling, TypeToken, Type.Name, TypeSpan);
			}

			switch (Form)
			{
			case ELegacyPropertyForm::ScalarUniform:
			case ELegacyPropertyForm::StaticBoolUniform:
				if (bHasDefault)
				{
					const bool bBoolDefault = DefaultText.Equals(TEXT("true"), ESearchCase::IgnoreCase) || DefaultText.Equals(TEXT("false"), ESearchCase::IgnoreCase);
					const bool bValid = Form == ELegacyPropertyForm::StaticBoolUniform ? bBoolDefault : TryParseLegacyScalar(DefaultText);
					if (!bValid)
					{
						Diagnostics.Error(
							TEXT("DSH3254"),
							DefaultSpan,
							FText::Format(
								LOCTEXT("ScalarDefault", "Expected a number as the default of '{0}', found '{1}'."),
								FText::FromString(Name),
								FText::FromString(DefaultText)));
						State.bOk = false;
						return;
					}
					Variable->Declarator.Initializer = LegacyAst::MakeValueExpressionFromText(DefaultText, DefaultSpan);
				}
				else if (bConst)
				{
					Variable->Declarator.Initializer = FLangParser::MakeLegacyZeroInitializer(Type, StatementSpan);
				}
				break;

			case ELegacyPropertyForm::VectorUniform:
				if (bHasDefault)
				{
					double Values[4] = { 0.0, 0.0, 0.0, 1.0 };
					if (!LegacyAst::TryNormalizeVectorLiteral(DefaultText, Values))
					{
						Diagnostics.Error(
							TEXT("DSH3254"),
							DefaultSpan,
							FText::Format(
								LOCTEXT("VectorDefault", "Expected a vector literal such as 'float4(1, 0, 0, 1)' as the default of '{0}', found '{1}'."),
								FText::FromString(Name),
								FText::FromString(DefaultText)));
						State.bOk = false;
						return;
					}

					// Written out component by component: 1.x filled a short literal its own way (one value
					// v,v,v,1; two values a,b,0,0), which is not what a 2.0 constructor of fewer arguments means.
					TUniquePtr<FTypeExpr> Callee = MakeUnique<FTypeExpr>();
					Callee->Type = Type;
					Callee->Span = DefaultSpan;
					TUniquePtr<FCallExpr> Constructor = MakeUnique<FCallExpr>();
					Constructor->Callee = MoveTemp(Callee);
					for (int32 Component = 0; Component < Type.Rows; ++Component)
					{
						FArgument Argument;
						Argument.Value = LegacyAst::MakeFloatLiteral(FormatDreamShaderFloatLiteral(Values[Component]), Values[Component], DefaultSpan);
						Argument.Span = DefaultSpan;
						Constructor->Arguments.Add(MoveTemp(Argument));
					}
					Constructor->Span = DefaultSpan;
					Variable->Declarator.Initializer = MoveTemp(Constructor);
				}
				else if (bConst)
				{
					Variable->Declarator.Initializer = FLangParser::MakeLegacyZeroInitializer(Type, StatementSpan);
				}
				break;

			case ELegacyPropertyForm::TextureUniform:
				if (bHasDefault)
				{
					// Unresolved on purpose: `Path(Root, "rel")` needs plugin mounts, which the emitter has.
					AddLegacyDirective(Variable->Doc, TEXT("default"), DefaultText, DefaultSpan);
					if (Info)
					{
						FLegacyAssetReference& Reference = Info->AssetReferences.AddDefaulted_GetRef();
						Reference.Use = FLegacyAssetReference::EUse::TextureDefault;
						Reference.Text = DefaultText;
						Reference.Span = DefaultSpan;
						Reference.Node = Variable.Get();
						Reference.bTypeFromAsset = TypeToken.Equals(TEXT("TextureObjectParameter"), ESearchCase::IgnoreCase);
					}
				}
				break;

			case ELegacyPropertyForm::ExpandAtUse:
			case ELegacyPropertyForm::Unsupported:
			case ELegacyPropertyForm::Unknown:
				break;
			}

			if (Form == ELegacyPropertyForm::StaticBoolUniform)
			{
				AddLegacyDirective(Variable->Doc, TEXT("static"), FString(), TypeSpan);
			}

			const bool bInGroup = !InheritedGroup.IsEmpty();
			bool bHasSort = false;
			if (!MakeLegacyUniformDirectives(Diagnostics, Metadata, InheritedGroup, bInGroup, State.NextAutoSort, Variable->Doc, bHasSort))
			{
				State.bOk = false;
			}
			if (bInGroup)
			{
				// An explicit SortPriority does not use up an automatic slot (1.x StampGroupedProperty).
				bool bExplicitSort = false;
				for (const FLegacyMetadataEntry& Entry : Metadata)
				{
					bExplicitSort |= Entry.Key.Equals(TEXT("SortPriority"), ESearchCase::IgnoreCase) || Entry.Key.Equals(TEXT("Sort"), ESearchCase::IgnoreCase);
				}
				if (!bExplicitSort)
				{
					State.NextAutoSort += 10;
				}
			}
			if (!bHasSort && !bConst)
			{
				// 1.x left SortPriority at the engine default when none was written; 2.0 would number the
				// declaration order instead.
				AddLegacyDirective(Variable->Doc, TEXT("sort"), TEXT("32"), FLangSpan());
				if (Info)
				{
					FLegacySynthesizedDirective& Synthesized = Info->SynthesizedDirectives.AddDefaulted_GetRef();
					Synthesized.Decl = Variable.Get();
					Synthesized.Key = TEXT("sort");
					Synthesized.Value = TEXT("32");
				}
			}

			for (const FDocDirective& Directive : Variable->Doc.Directives)
			{
				Property.Metadata.Add(Directive);
			}
			Variable->Doc.Span = FLangSpan();
			State.Block.Properties.Add(MoveTemp(Property));
			State.OutDecls.Add(MoveTemp(Variable));
		}
	}

	bool FLangParser::ParseLegacyProperties(FLegacyBlockContext& Block, TArray<FDeclPtr>& OutDecls)
	{
		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH3250"), LOCTEXT("PropertiesOpen", "'{' to open the Properties section")))
		{
			return false;
		}

		LegacySections::FLegacyPropertiesState State(Block, OutDecls);
		LegacySections::ParseLegacyPropertyItems(*this, State, FString());
		if (!Match(ELangTokenKind::RightBrace))
		{
			return State.bOk && FailAtEnd(LOCTEXT("WhilePropertiesEnd", "a Properties section"));
		}
		return State.bOk;
	}

	namespace LegacySections
	{
		/** What `#pragma` can read back unquoted: one identifier, or one number with its sign. */
		static bool IsLegacySimplePragmaValue(const FString& Value)
		{
			if (Value.IsEmpty())
			{
				return false;
			}
			if (FChar::IsAlpha(Value[0]) || Value[0] == TEXT('_'))
			{
				for (const TCHAR Character : Value)
				{
					if (!(FChar::IsAlnum(Character) || Character == TEXT('_')))
					{
						return false;
					}
				}
				return true;
			}
			FString Number = Value;
			if (Number.StartsWith(TEXT("-")) || Number.StartsWith(TEXT("+")))
			{
				Number.RightChopInline(1);
			}
			return !Number.IsEmpty() && FCString::IsNumeric(*Number);
		}

		/** A `Key = Value` of a Settings or Layout entry: a single string token keeps its value quoted, anything else its text. */
		static void FillLegacyPragmaValue(FLangParser& Parser, const int32 First, const int32 End, FPragmaArgument& OutArgument)
		{
			const int32 Saved = Parser.GetTokenIndex();
			Parser.SetTokenIndex(First);
			const bool bSingleString = End - First == 1 && Parser.Current().Kind == ELangTokenKind::StringLiteral;
			const FString StringValue = bSingleString ? Parser.Current().Text : FString();
			Parser.SetTokenIndex(Saved);

			if (bSingleString)
			{
				OutArgument.Value = StringValue;
				OutArgument.bQuoted = true;
				return;
			}
			OutArgument.Value = SliceLegacyTokens(Parser, First, End);
			OutArgument.bQuoted = !IsLegacySimplePragmaValue(OutArgument.Value);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Settings
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::ParseLegacySettings(FLegacyBlockContext& Block, TArray<FPragmaArgument>& OutSettings)
	{
		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH3261"), LOCTEXT("SettingsOpen", "'{' to open the Settings section")))
		{
			return false;
		}

		bool bOk = true;
		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileSettings", "a Settings section"));
			}
			if (Match(ELangTokenKind::Semicolon))
			{
				continue;
			}
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}

			const int32 StartIndex = GetTokenIndex();
			if (!Check(ELangTokenKind::Identifier) && !Check(ELangTokenKind::Keyword))
			{
				Diagnostics.Error(
					TEXT("DSH3261"),
					Current().Span,
					FText::Format(LOCTEXT("SettingExpectedKey", "Expected a setting name such as 'BlendMode', found {0}."), DescribeToken(Current())));
				bOk = false;
				LegacySections::SkipLegacySectionStatement(*this);
				continue;
			}
			const FString Key = Advance().Text;

			if (!Match(ELangTokenKind::Assign))
			{
				Diagnostics.Error(
					TEXT("DSH3261"),
					Current().Span,
					FText::Format(LOCTEXT("SettingExpectedEquals", "Expected '=' after the setting '{0}', found {1}."), FText::FromString(Key), DescribeToken(Current())));
				bOk = false;
				LegacySections::SkipLegacySectionStatement(*this);
				continue;
			}

			int32 First = 0;
			int32 End = 0;
			LegacySections::SkipLegacyValue(*this, { ELangTokenKind::Semicolon }, First, End);
			if (End == First)
			{
				Diagnostics.Error(
					TEXT("DSH3261"),
					Current().Span,
					FText::Format(LOCTEXT("SettingExpectedValue", "Expected a value after '{0} =', found {1}."), FText::FromString(Key), DescribeToken(Current())));
				bOk = false;
				LegacySections::SkipLegacySectionStatement(*this);
				continue;
			}

			FPragmaArgument Argument;
			Argument.Key = Key;
			LegacySections::FillLegacyPragmaValue(*this, First, End, Argument);
			Argument.Span = SpanFrom(StartIndex);
			Match(ELangTokenKind::Semicolon);

			for (int32 ItemIndex = 0; ItemIndex < OutSettings.Num(); ++ItemIndex)
			{
				// 1.x kept settings in a map keyed by the lower-case name: the later line won.
				if (OutSettings[ItemIndex].Key.Equals(Key, ESearchCase::IgnoreCase))
				{
					Diagnostics.Warning(
						TEXT("DSH3262"),
						Argument.Span,
						FText::Format(LOCTEXT("SettingRepeated", "The setting '{0}' is written twice; the later value wins, as it did in 1.x."), FText::FromString(Key)));
					OutSettings.RemoveAt(ItemIndex);
					break;
				}
			}
			OutSettings.Add(MoveTemp(Argument));
		}

		Advance(); // }
		return bOk;
	}

	// ---------------------------------------------------------------------------------------------
	// Outputs (Shader)
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::ParseLegacyOutputs(FLegacyBlockContext& Block, TArray<FStmtPtr>& OutHead, TArray<FStmtPtr>& OutTail)
	{
		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH3266"), LOCTEXT("OutputsOpen", "'{' to open the Outputs section")))
		{
			return false;
		}

		struct FLegacyExpressionTarget
		{
			/** 1.x's reuse key: class and arguments, lower case, sorted. Case-sensitive compare. */
			FString Key;
			FCallExpr* Call = nullptr;
			TArray<int32> Pins;
			FString ClassName;
		};
		TArray<FLegacyExpressionTarget> Targets;
		bool bOk = true;

		// `Pin[<index>] = <source>;` -- the cursor at `Pin`.
		auto ParsePinBinding = [this](int32& OutPin, FExprPtr& OutSource, FLangSpan& OutSpan) -> bool
		{
			const int32 PinStart = GetTokenIndex();
			const bool bShape = Check(ELangTokenKind::Identifier)
				&& Current().Text.Equals(TEXT("Pin"), ESearchCase::IgnoreCase)
				&& Peek(1).Kind == ELangTokenKind::LeftBracket
				&& Peek(2).Kind == ELangTokenKind::IntLiteral
				&& Peek(3).Kind == ELangTokenKind::RightBracket
				&& Peek(4).Kind == ELangTokenKind::Assign;
			if (!bShape)
			{
				return Diagnostics.Error(
					TEXT("DSH3267"),
					Current().Span,
					FText::Format(LOCTEXT("PinBindingShape", "Expected 'Pin[<index>] = <source>' for an Expression(...) output target, found {0}."), DescribeToken(Current())));
			}
			Advance();
			Advance();
			OutPin = static_cast<int32>(Advance().Integer);
			Advance();
			Advance();

			int32 First = 0;
			int32 End = 0;
			LegacySections::SkipLegacyValue(*this, { ELangTokenKind::Semicolon }, First, End);
			if (End == First)
			{
				return Diagnostics.Error(
					TEXT("DSH3270"),
					Current().Span,
					FText::Format(LOCTEXT("PinSourceMissing", "Expected a source after 'Pin[{0}] =', found none."), FText::AsNumber(OutPin)));
			}
			OutSource = ParseLegacyExpressionRange(First, End);
			if (!OutSource.IsValid())
			{
				return false;
			}
			ValidateLegacyValue(*OutSource);
			Match(ELangTokenKind::Semicolon);
			OutSpan = SpanFrom(PinStart);
			return true;
		};

		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileOutputs", "an Outputs section"));
			}
			if (Match(ELangTokenKind::Semicolon))
			{
				continue;
			}
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}

			const int32 StartIndex = GetTokenIndex();

			// -- `Type Name [= init];`
			if (Check(ELangTokenKind::Identifier) && Peek(1).Kind == ELangTokenKind::Identifier)
			{
				const FLangToken& TypeToken = Advance();
				const FLangToken& NameToken = Advance();

				TUniquePtr<FVarDeclStmt> Declaration = MakeUnique<FVarDeclStmt>();
				ClassifyLegacyTypeName(TypeToken.Text, Declaration->Type);
				Declaration->Type.Span = TypeToken.Span;
				RecordLegacyRename(FLegacyRename::EKind::TypeSpelling, TypeToken.Text, Declaration->Type.Name, TypeToken.Span);

				FDeclarator Declarator;
				Declarator.Name = NameToken.Text;
				Declarator.NameSpan = NameToken.Span;

				if (Match(ELangTokenKind::Assign))
				{
					int32 First = 0;
					int32 End = 0;
					LegacySections::SkipLegacyValue(*this, { ELangTokenKind::Semicolon }, First, End);
					if (End == First)
					{
						Diagnostics.Error(
							TEXT("DSH3270"),
							Current().Span,
							FText::Format(LOCTEXT("OutputInitializerMissing", "Expected an initializer after '{0} =', found none."), FText::FromString(NameToken.Text)));
						bOk = false;
						LegacySections::SkipLegacySectionStatement(*this);
						continue;
					}
					Declarator.Initializer = ParseLegacyExpressionRange(First, End);
					if (!Declarator.Initializer.IsValid())
					{
						bOk = false;
						LegacySections::SkipLegacySectionStatement(*this);
						continue;
					}
					ValidateLegacyValue(*Declarator.Initializer);
				}

				if (!Match(ELangTokenKind::Semicolon) && !Check(ELangTokenKind::RightBrace))
				{
					Diagnostics.Error(
						TEXT("DSH3266"),
						Current().Span,
						FText::Format(LOCTEXT("OutputDeclarationEnd", "Expected ';' after the output declaration '{0}', found {1}."), FText::FromString(NameToken.Text), DescribeToken(Current())));
					bOk = false;
					LegacySections::SkipLegacySectionStatement(*this);
					continue;
				}

				Declarator.Span = SpanFrom(StartIndex);
				Declaration->Span = Declarator.Span;
				if (!Declarator.Initializer.IsValid())
				{
					// 1.x zero-initialised a declared output. A Substrate or texture output has no zero; the
					// Graph assigns it, and the binder reports a read before that.
					Declarator.Initializer = MakeLegacyZeroInitializer(Declaration->Type, Declarator.Span);
					if (Declarator.Initializer.IsValid() && LegacyInfo)
					{
						FLegacySynthesizedInitializer& Record = LegacyInfo->SynthesizedInitializers.AddDefaulted_GetRef();
						Record.Declaration = Declaration.Get();
						Record.Name = Declarator.Name;
						Record.Span = Declarator.Span;
					}
				}
				Declaration->Declarators.Add(MoveTemp(Declarator));
				OutHead.Add(MoveTemp(Declaration));
				continue;
			}

			// -- `Base.Attribute = source;`
			if (LegacySections::TokenIsWord(Current(), TEXT("Base"))
				&& Peek(1).Kind == ELangTokenKind::Dot
				&& Peek(2).Kind == ELangTokenKind::Identifier
				&& Peek(3).Kind == ELangTokenKind::Assign)
			{
				const FLangToken& BaseToken = Advance();
				Advance(); // .
				const FLangToken& AttributeToken = Advance();
				Advance(); // =

				int32 First = 0;
				int32 End = 0;
				LegacySections::SkipLegacyValue(*this, { ELangTokenKind::Semicolon }, First, End);
				if (End == First)
				{
					Diagnostics.Error(
						TEXT("DSH3270"),
						Current().Span,
						FText::Format(LOCTEXT("BindingSourceMissing", "Expected a source after 'Base.{0} =', found none."), FText::FromString(AttributeToken.Text)));
					bOk = false;
					LegacySections::SkipLegacySectionStatement(*this);
					continue;
				}

				FExprPtr SourceValue = ParseLegacyExpressionRange(First, End);
				Match(ELangTokenKind::Semicolon);
				if (!SourceValue.IsValid())
				{
					bOk = false;
					continue;
				}
				ValidateLegacyValue(*SourceValue);

				// The member keeps the author's spelling (`CustomizedUV1`): it names the sink reroute (Tools/Parity/README.md, D12).
				TUniquePtr<FAssignExpr> Assign = MakeUnique<FAssignExpr>();
				Assign->Op = EAssignOp::Assign;
				Assign->Target = LegacyAst::MakeMember(LegacyAst::MakeIdentifier(TEXT("Base"), BaseToken.Span), AttributeToken.Text, AttributeToken.Span);
				Assign->Target->Span = FLangSpan::Join(BaseToken.Span, AttributeToken.Span);
				Assign->Value = MoveTemp(SourceValue);
				Assign->Span = SpanFrom(StartIndex);

				TUniquePtr<FExprStmt> Statement = MakeUnique<FExprStmt>();
				Statement->Span = Assign->Span;
				Statement->Expression = MoveTemp(Assign);
				OutTail.Add(MoveTemp(Statement));
				continue;
			}

			// -- `Expression(Class = "C", ...).Pin[i] = source;` and `Expression(...) { Pin[i] = source; ... }`
			if (LegacySections::TokenIsWord(Current(), TEXT("Expression")) && Peek(1).Kind == ELangTokenKind::LeftParen)
			{
				const int32 HeadStart = GetTokenIndex();
				Advance(); // Expression
				Advance(); // (

				TArray<TPair<FString, FString>> Arguments;
				bool bHeadOk = true;
				if (!Check(ELangTokenKind::RightParen))
				{
					for (;;)
					{
						if (!Check(ELangTokenKind::Identifier) && !Check(ELangTokenKind::Keyword))
						{
							Diagnostics.Error(
								TEXT("DSH3267"),
								Current().Span,
								FText::Format(LOCTEXT("TargetArgumentShape", "Expected 'Key = Value' inside the output target Expression(...), found {0}."), DescribeToken(Current())));
							bHeadOk = false;
							break;
						}
						const FString Key = Advance().Text;
						if (!Match(ELangTokenKind::Assign))
						{
							Diagnostics.Error(
								TEXT("DSH3267"),
								Current().Span,
								FText::Format(LOCTEXT("TargetArgumentEquals", "Expected '=' after '{0}' inside the output target Expression(...), found {1}."), FText::FromString(Key), DescribeToken(Current())));
							bHeadOk = false;
							break;
						}
						int32 First = 0;
						int32 End = 0;
						LegacySections::SkipLegacyValue(*this, { ELangTokenKind::Comma, ELangTokenKind::RightParen }, First, End);
						if (End == First)
						{
							Diagnostics.Error(
								TEXT("DSH3267"),
								Current().Span,
								FText::Format(LOCTEXT("TargetArgumentValue", "Expected a value after '{0} =' inside the output target Expression(...), found {1}."), FText::FromString(Key), DescribeToken(Current())));
							bHeadOk = false;
							break;
						}
						for (const TPair<FString, FString>& Existing : Arguments)
						{
							if (Existing.Key.Equals(Key, ESearchCase::IgnoreCase))
							{
								Diagnostics.Error(
									TEXT("DSH3267"),
									Previous().Span,
									FText::Format(LOCTEXT("TargetArgumentRepeated", "Expected the argument '{0}' once in the output target Expression(...), found it again."), FText::FromString(Key)));
								bHeadOk = false;
							}
						}
						Arguments.Emplace(Key, LegacySections::SliceLegacyTokens(*this, First, End));
						if (!Match(ELangTokenKind::Comma))
						{
							break;
						}
					}
				}
				if (bHeadOk && !Match(ELangTokenKind::RightParen))
				{
					Diagnostics.Error(
						TEXT("DSH3267"),
						Current().Span,
						FText::Format(LOCTEXT("TargetClose", "Expected ')' to close the output target Expression(...), found {0}."), DescribeToken(Current())));
					bHeadOk = false;
				}
				if (!bHeadOk)
				{
					bOk = false;
					LegacySections::SkipLegacySectionStatement(*this);
					continue;
				}
				const FLangSpan HeadSpan = SpanFrom(HeadStart);

				FString ClassName;
				TArray<FString> KeyParts;
				for (const TPair<FString, FString>& Argument : Arguments)
				{
					if (Argument.Key.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
					{
						ClassName = LegacyAst::Unquote(Argument.Value);
					}
					else
					{
						KeyParts.Add(Argument.Key.ToLower() + TEXT("=") + LegacyAst::Unquote(Argument.Value));
					}
				}
				if (ClassName.IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH3267"),
						HeadSpan,
						LOCTEXT("TargetWithoutClass", "Expected 'Class = \"...\"' in the output target Expression(...), found no class."));
					bOk = false;
					LegacySections::SkipLegacySectionStatement(*this);
					continue;
				}
				KeyParts.Sort();
				const FString TargetKey = ClassName.ToLower() + TEXT("|") + FString::Join(KeyParts, TEXT("|"));

				struct FPendingPin
				{
					int32 Pin = 0;
					FExprPtr Source;
					FLangSpan Span;
				};
				TArray<FPendingPin> Pins;

				if (Match(ELangTokenKind::Dot))
				{
					FPendingPin Pending;
					if (!ParsePinBinding(Pending.Pin, Pending.Source, Pending.Span))
					{
						bOk = false;
						LegacySections::SkipLegacySectionStatement(*this);
						continue;
					}
					Pending.Span = SpanFrom(StartIndex);
					Pins.Add(MoveTemp(Pending));
				}
				else if (Match(ELangTokenKind::LeftBrace))
				{
					while (!Check(ELangTokenKind::RightBrace))
					{
						if (AtEnd())
						{
							return FailAtEnd(LOCTEXT("WhileTargetBlock", "an Expression(...) block in Outputs"));
						}
						if (Match(ELangTokenKind::Semicolon))
						{
							continue;
						}
						FPendingPin Pending;
						if (!ParsePinBinding(Pending.Pin, Pending.Source, Pending.Span))
						{
							bOk = false;
							LegacySections::SkipLegacySectionStatement(*this);
							continue;
						}
						Pins.Add(MoveTemp(Pending));
					}
					Advance(); // }
					Match(ELangTokenKind::Semicolon);
					if (Pins.Num() == 0)
					{
						Diagnostics.Error(
							TEXT("DSH3269"),
							HeadSpan,
							LOCTEXT("TargetBlockEmpty", "Expected at least one 'Pin[<index>] = <source>;' in the Expression(...) block, found none."));
						bOk = false;
						continue;
					}
				}
				else
				{
					Diagnostics.Error(
						TEXT("DSH3267"),
						Current().Span,
						FText::Format(LOCTEXT("TargetNeedsPin", "Expected '.Pin[<index>] = <source>' or a block of pin bindings after the output target Expression(...), found {0}."), DescribeToken(Current())));
					bOk = false;
					LegacySections::SkipLegacySectionStatement(*this);
					continue;
				}

				for (FPendingPin& Pending : Pins)
				{
					FLegacyExpressionTarget* Target = nullptr;
					for (FLegacyExpressionTarget& Candidate : Targets)
					{
						if (Candidate.Key.Equals(TargetKey, ESearchCase::CaseSensitive))
						{
							Target = &Candidate;
							break;
						}
					}

					if (!Target)
					{
						// One statement call per node: 1.x merged equal heads into one expression.
						TUniquePtr<FCallExpr> Call = LegacyAst::MakeExpressionCall(ClassName, HeadSpan);
						for (const TPair<FString, FString>& Argument : Arguments)
						{
							if (!Argument.Key.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
							{
								Call->Arguments.Add(LegacyAst::MakeNamedArgument(Argument.Key, LegacyAst::MakeValueExpressionFromText(Argument.Value, HeadSpan), HeadSpan));
							}
						}

						FLegacyExpressionTarget& Added = Targets.AddDefaulted_GetRef();
						Added.Key = TargetKey;
						Added.Call = Call.Get();
						Added.ClassName = ClassName;
						Target = &Added;

						TUniquePtr<FExprStmt> Statement = MakeUnique<FExprStmt>();
						Statement->Span = Pending.Span;
						Statement->Expression = MoveTemp(Call);
						OutTail.Add(MoveTemp(Statement));
					}

					if (Target->Pins.Contains(Pending.Pin))
					{
						Diagnostics.Error(
							TEXT("DSH3268"),
							Pending.Span,
							FText::Format(
								LOCTEXT("PinBoundTwice", "Expected each pin of Expression(Class = \"{0}\") to be bound once, found Pin[{1}] bound again."),
								FText::FromString(Target->ClassName),
								FText::AsNumber(Pending.Pin)));
						bOk = false;
						continue;
					}
					Target->Pins.Add(Pending.Pin);

					FArgument PinArgument;
					PinArgument.Value = MoveTemp(Pending.Source);
					PinArgument.PinIndex = Pending.Pin;
					PinArgument.Span = Pending.Span;
					Target->Call->Arguments.Add(MoveTemp(PinArgument));
				}
				continue;
			}

			Diagnostics.Error(
				TEXT("DSH3266"),
				Current().Span,
				FText::Format(
					LOCTEXT("OutputsStatementShape", "Expected an output declaration, 'Base.<Attribute> = <source>;' or 'Expression(...).Pin[<index>] = <source>;', found {0}."),
					DescribeToken(Current())));
			bOk = false;
			LegacySections::SkipLegacySectionStatement(*this);
		}

		Advance(); // }
		return bOk;
	}

	// ---------------------------------------------------------------------------------------------
	// Inputs / Outputs of a function block
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::ParseLegacyParams(FLegacyBlockContext& Block, const EParamDirection Direction, TArray<FParam>& OutParams)
	{
		TArray<FString> DeclaredNames;
		TArray<TPair<FString, FString>> Docs;
		return ParseLegacyParamsWithDocs(Block, Direction, OutParams, DeclaredNames, Docs);
	}

	bool FLangParser::ParseLegacyParamsWithDocs(
		FLegacyBlockContext& Block,
		const EParamDirection Direction,
		TArray<FParam>& OutParams,
		TArray<FString>& OutDeclaredNames,
		TArray<TPair<FString, FString>>& OutDocs)
	{
		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH3271"), LOCTEXT("ParamsOpen", "'{' to open the parameter section")))
		{
			return false;
		}

		TArray<FParam> Declared;
		TArray<int32> Ranks;
		bool bOk = true;

		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileParams", "a parameter section"));
			}
			if (Match(ELangTokenKind::Semicolon))
			{
				continue;
			}
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}

			const int32 StartIndex = GetTokenIndex();
			bool bOpt = false;
			if (LegacySections::TokenIsWord(Current(), TEXT("opt")) && Peek(1).Kind == ELangTokenKind::Identifier && Peek(2).Kind == ELangTokenKind::Identifier)
			{
				bOpt = true;
				Advance();
			}

			if (!Check(ELangTokenKind::Identifier) || Peek(1).Kind != ELangTokenKind::Identifier)
			{
				Diagnostics.Error(
					TEXT("DSH3271"),
					Current().Span,
					FText::Format(LOCTEXT("ParamShape", "Expected a parameter type and name such as 'float Amount', found {0}."), DescribeToken(Current())));
				bOk = false;
				LegacySections::SkipLegacySectionStatement(*this);
				continue;
			}

			const FLangToken& TypeToken = Advance();
			const FLangToken& NameToken = Advance();

			// A 1.x `StaticBool` input is a FunctionInput_StaticBool pin. The alias table reads the token as `bool`, which
			// on its own would be a Scalar pin; the function's `/// @static <Name>` is what 2.0 says instead.
			if (Direction == EParamDirection::In
				&& (TypeToken.Text.Equals(TEXT("StaticBool"), ESearchCase::IgnoreCase)
					|| TypeToken.Text.Equals(TEXT("StaticBoolParameter"), ESearchCase::IgnoreCase)))
			{
				LegacyStaticBoolInputs.AddUnique(NameToken.Text);
			}

			FParam Param;
			Param.Direction = Direction;
			ClassifyLegacyTypeName(TypeToken.Text, Param.Type);
			Param.Type.Span = TypeToken.Span;
			RecordLegacyRename(FLegacyRename::EKind::TypeSpelling, TypeToken.Text, Param.Type.Name, TypeToken.Span);
			Param.Name = NameToken.Text;
			Param.NameSpan = NameToken.Span;

			if (Match(ELangTokenKind::Assign))
			{
				int32 First = 0;
				int32 End = 0;
				LegacySections::SkipLegacyValue(*this, { ELangTokenKind::Semicolon, ELangTokenKind::LeftBracket }, First, End);
				if (End == First)
				{
					Diagnostics.Error(
						TEXT("DSH3271"),
						Current().Span,
						FText::Format(LOCTEXT("ParamDefaultMissing", "Expected a default value after '{0} =', found {1}."), FText::FromString(Param.Name), DescribeToken(Current())));
					bOk = false;
					LegacySections::SkipLegacySectionStatement(*this);
					continue;
				}
				Param.Default = ParseLegacyExpressionRange(First, End);
				if (!Param.Default.IsValid())
				{
					bOk = false;
					LegacySections::SkipLegacySectionStatement(*this);
					continue;
				}
				ValidateLegacyValue(*Param.Default);
			}

			TArray<LegacySections::FLegacyMetadataEntry> Metadata;
			if (Check(ELangTokenKind::LeftBracket) && !LegacySections::ParseLegacyMetadata(*this, Metadata))
			{
				bOk = false;
			}

			if (!Match(ELangTokenKind::Semicolon) && !Check(ELangTokenKind::RightBrace))
			{
				Diagnostics.Error(
					TEXT("DSH3271"),
					Current().Span,
					FText::Format(LOCTEXT("ParamEnd", "Expected ';' after the parameter '{0}', found {1}."), FText::FromString(Param.Name), DescribeToken(Current())));
				bOk = false;
				LegacySections::SkipLegacySectionStatement(*this);
				continue;
			}
			Param.Span = SpanFrom(StartIndex);

			if (Direction == EParamDirection::Out)
			{
				if (bOpt)
				{
					Diagnostics.Warning(
						TEXT("DSH3272"),
						Param.Span,
						FText::Format(LOCTEXT("OptOnOutput", "'opt' on the output '{0}' means nothing; 1.x ignored it and so does this front end."), FText::FromString(Param.Name)));
					bOpt = false;
				}
				if (Param.Default.IsValid())
				{
					Diagnostics.Warning(
						TEXT("DSH3273"),
						Param.Span,
						FText::Format(LOCTEXT("DefaultOnOutput", "A default on the output '{0}' means nothing; 1.x ignored it, so it is dropped."), FText::FromString(Param.Name)));
					Param.Default.Reset();
				}
			}
			Param.bOptional = bOpt && !Param.Default.IsValid();

			// 1.x ordered function pins by the author's SortPriority, else the declaration index, stably.
			int32 Rank = Declared.Num();
			for (const LegacySections::FLegacyMetadataEntry& Entry : Metadata)
			{
				const FString Lower = Entry.Key.ToLower();
				if (Lower == TEXT("sortpriority") || Lower == TEXT("sort"))
				{
					int32 Sort = 0;
					if (!LegacySections::TryParseLegacyInteger(LegacyAst::Unquote(Entry.Value), Sort))
					{
						Diagnostics.Error(
							TEXT("DSH3258"),
							Entry.Span,
							FText::Format(LOCTEXT("ParamSortNotInteger", "Expected 'SortPriority' to be a whole number, found '{0}'."), FText::FromString(Entry.Value)));
						bOk = false;
						continue;
					}
					Rank = Sort;
				}
				else if (Lower == TEXT("description") || Lower == TEXT("desc") || Lower == TEXT("tooltip"))
				{
					OutDocs.Emplace(Param.Name, LegacyAst::Unquote(Entry.Value));
				}
			}

			OutDeclaredNames.Add(Param.Name);
			Ranks.Add(Rank);
			Declared.Add(MoveTemp(Param));
		}
		Advance(); // }

		TArray<int32> Order;
		Order.Reserve(Declared.Num());
		for (int32 ItemIndex = 0; ItemIndex < Declared.Num(); ++ItemIndex)
		{
			Order.Add(ItemIndex);
		}
		Order.StableSort([&Ranks](const int32 A, const int32 B) { return Ranks[A] < Ranks[B]; });
		for (const int32 ItemIndex : Order)
		{
			OutParams.Add(MoveTemp(Declared[ItemIndex]));
		}
		return bOk;
	}

	// ---------------------------------------------------------------------------------------------
	// Layout
	// ---------------------------------------------------------------------------------------------

	bool FLangParser::ParseLegacyLayout(TArray<FDeclPtr>& OutDecls)
	{
		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH3274"), LOCTEXT("LayoutOpen", "'{' to open the Layout section")))
		{
			return false;
		}

		bool bOk = true;
		while (!Check(ELangTokenKind::RightBrace))
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileLayout", "a Layout section"));
			}
			if (Match(ELangTokenKind::Semicolon))
			{
				continue;
			}
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}

			const int32 StartIndex = GetTokenIndex();
			if (!Check(ELangTokenKind::Identifier) || Peek(1).Kind != ELangTokenKind::LeftParen)
			{
				Diagnostics.Error(
					TEXT("DSH3274"),
					Current().Span,
					FText::Format(LOCTEXT("LayoutShape", "Expected 'Node(...)' or 'Comment(...)' in the Layout section, found {0}."), DescribeToken(Current())));
				bOk = false;
				LegacySections::SkipLegacySectionStatement(*this);
				continue;
			}

			const FLangToken& CallToken = Advance();
			Advance(); // (

			TArray<FPragmaArgument> Written;
			bool bArgumentsOk = true;
			if (!Check(ELangTokenKind::RightParen))
			{
				for (;;)
				{
					const int32 ArgumentStart = GetTokenIndex();
					if (!Check(ELangTokenKind::Identifier))
					{
						Diagnostics.Error(
							TEXT("DSH3274"),
							Current().Span,
							FText::Format(LOCTEXT("LayoutArgumentShape", "Expected 'Key = Value' inside '{0}(...)', found {1}."), FText::FromString(CallToken.Text), DescribeToken(Current())));
						bArgumentsOk = false;
						break;
					}
					const FString Key = Advance().Text;
					if (!Match(ELangTokenKind::Assign))
					{
						Diagnostics.Error(
							TEXT("DSH3274"),
							Current().Span,
							FText::Format(LOCTEXT("LayoutArgumentEquals", "Expected '=' after '{0}' inside '{1}(...)', found {2}."), FText::FromString(Key), FText::FromString(CallToken.Text), DescribeToken(Current())));
						bArgumentsOk = false;
						break;
					}
					int32 First = 0;
					int32 End = 0;
					LegacySections::SkipLegacyValue(*this, { ELangTokenKind::Comma, ELangTokenKind::RightParen }, First, End);
					if (End == First)
					{
						Diagnostics.Error(
							TEXT("DSH3274"),
							Current().Span,
							FText::Format(LOCTEXT("LayoutArgumentValue", "Expected a value after '{0} =' inside '{1}(...)', found {2}."), FText::FromString(Key), FText::FromString(CallToken.Text), DescribeToken(Current())));
						bArgumentsOk = false;
						break;
					}

					FPragmaArgument Argument;
					Argument.Key = Key;
					LegacySections::FillLegacyPragmaValue(*this, First, End, Argument);
					Argument.Span = SpanFrom(ArgumentStart);
					for (const FPragmaArgument& Existing : Written)
					{
						if (Existing.Key.Equals(Key, ESearchCase::IgnoreCase))
						{
							Diagnostics.Error(
								TEXT("DSH3274"),
								Argument.Span,
								FText::Format(LOCTEXT("LayoutArgumentRepeated", "Expected the Layout argument '{0}' once, found it again."), FText::FromString(Key)));
							bArgumentsOk = false;
						}
					}
					Written.Add(MoveTemp(Argument));
					if (!Match(ELangTokenKind::Comma))
					{
						break;
					}
				}
			}
			if (bArgumentsOk && !Match(ELangTokenKind::RightParen))
			{
				Diagnostics.Error(
					TEXT("DSH3274"),
					Current().Span,
					FText::Format(LOCTEXT("LayoutClose", "Expected ')' to close '{0}(...)', found {1}."), FText::FromString(CallToken.Text), DescribeToken(Current())));
				bArgumentsOk = false;
			}
			if (!bArgumentsOk)
			{
				bOk = false;
				LegacySections::SkipLegacySectionStatement(*this);
				continue;
			}
			Match(ELangTokenKind::Semicolon);
			const FLangSpan StatementSpan = SpanFrom(StartIndex);

			const bool bNode = CallToken.Text.Equals(TEXT("Node"), ESearchCase::IgnoreCase);
			const bool bComment = CallToken.Text.Equals(TEXT("Comment"), ESearchCase::IgnoreCase);
			if (!bNode && !bComment)
			{
				Diagnostics.Error(
					TEXT("DSH3274"),
					CallToken.Span,
					FText::Format(LOCTEXT("LayoutUnknown", "Expected 'Node' or 'Comment' in the Layout section, found '{0}'."), FText::FromString(CallToken.Text)));
				bOk = false;
				continue;
			}

			TUniquePtr<FPragmaDecl> Pragma = MakeUnique<FPragmaDecl>();
			Pragma->bLegacy = true;
			Pragma->PragmaKind = EPragmaKind::Layout;
			Pragma->Name = TEXT("layout");
			Pragma->Span = StatementSpan;

			FPragmaArgument Selector;
			Selector.Value = bNode ? TEXT("Node") : TEXT("Comment");
			Selector.Span = CallToken.Span;
			Pragma->Arguments.Add(MoveTemp(Selector));

			static const TCHAR* const NodeKeys[] = { TEXT("Var"), TEXT("X"), TEXT("Y") };
			static const TCHAR* const CommentKeys[] = { TEXT("Name"), TEXT("X"), TEXT("Y"), TEXT("W"), TEXT("H") };
			const TCHAR* const* RequiredKeys = bNode ? NodeKeys : CommentKeys;
			const int32 RequiredCount = bNode ? 3 : 5;

			bool bShapeOk = true;
			TArray<bool> Used;
			Used.Init(false, Written.Num());
			for (int32 Required = 0; Required < RequiredCount; ++Required)
			{
				const FString Key = RequiredKeys[Required];
				int32 Found = INDEX_NONE;
				for (int32 ItemIndex = 0; ItemIndex < Written.Num(); ++ItemIndex)
				{
					if (Written[ItemIndex].Key.Equals(Key, ESearchCase::IgnoreCase))
					{
						Found = ItemIndex;
						break;
					}
				}
				if (Found == INDEX_NONE || Written[Found].Value.TrimStartAndEnd().IsEmpty())
				{
					Diagnostics.Error(
						TEXT("DSH3275"),
						StatementSpan,
						FText::Format(LOCTEXT("LayoutMissingArgument", "Expected the argument '{0}' in '{1}(...)', found none."), FText::FromString(Key), FText::FromString(CallToken.Text)));
					bShapeOk = false;
					continue;
				}

				const bool bText = Key.Equals(TEXT("Var")) || Key.Equals(TEXT("Name"));
				int32 Number = 0;
				if (!bText && !LegacySections::TryParseLegacyInteger(Written[Found].Value, Number))
				{
					Diagnostics.Error(
						TEXT("DSH3275"),
						Written[Found].Span,
						FText::Format(LOCTEXT("LayoutNotInteger", "Expected the Layout argument '{0}' to be a whole number, found '{1}'."), FText::FromString(Key), FText::FromString(Written[Found].Value)));
					bShapeOk = false;
					continue;
				}

				FPragmaArgument Canonical = Written[Found];
				Canonical.Key = Key;
				Used[Found] = true;
				Pragma->Arguments.Add(MoveTemp(Canonical));
			}

			for (int32 ItemIndex = 0; ItemIndex < Written.Num(); ++ItemIndex)
			{
				if (Used[ItemIndex])
				{
					continue;
				}
				FPragmaArgument Extra = Written[ItemIndex];
				if (bComment && Extra.Key.Equals(TEXT("Color"), ESearchCase::IgnoreCase))
				{
					double Values[4] = { 0.0, 0.0, 0.0, 1.0 };
					if (!LegacyAst::TryNormalizeVectorLiteral(Extra.Value, Values))
					{
						Diagnostics.Error(
							TEXT("DSH3276"),
							Extra.Span,
							FText::Format(LOCTEXT("LayoutColor", "Expected 'Color' to be a vector literal such as '(0.1, 0.16, 0.22, 0.35)', found '{0}'."), FText::FromString(Extra.Value)));
						bShapeOk = false;
						continue;
					}
					// `Color = "r g b a"`: the quoted form the 2.0 layout pragma reads.
					Extra.Key = TEXT("Color");
					Extra.Value = FString::Printf(
						TEXT("%s %s %s %s"),
						*FormatDreamShaderFloatLiteral(Values[0]),
						*FormatDreamShaderFloatLiteral(Values[1]),
						*FormatDreamShaderFloatLiteral(Values[2]),
						*FormatDreamShaderFloatLiteral(Values[3]));
					Extra.bQuoted = true;
				}
				Pragma->Arguments.Add(MoveTemp(Extra));
			}

			if (!bShapeOk)
			{
				bOk = false;
				continue;
			}
			OutDecls.Add(MoveTemp(Pragma));
		}

		Advance(); // }
		return bOk;
	}
}

#undef LOCTEXT_NAMESPACE
