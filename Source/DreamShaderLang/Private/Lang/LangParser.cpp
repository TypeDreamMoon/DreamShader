// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Front-end selection, the token cursor, error recovery and the module loop.
//
// The two public entry points both do the same three things -- build the token stream, hand it to
// one FLangParser, hand back whatever survived -- and differ only in what they ask the parser for.
// Everything below the entry points is the machinery the three parser translation units share:
// a clamped cursor that can never run off the end (the last token is always EndOfFile, and
// Advance() refuses to move past it, so a parse method may Peek without bounds-checking), the
// Expect/Describe pair every "expected X, found Y" message goes through, and the two skip
// routines that let one broken declaration cost exactly one declaration.

#include "Lang/LangParser.h"

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLexer.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.Parser"

namespace UE::DreamShader::Lang::Private
{
	/**
	 * Defined in LangParserDeclarations.cpp.
	 *
	 * `uniform float a, b;` is one FVariableDecl per name, but ParseDeclaration() can only hand
	 * back one node, so ParseFunctionOrVariableDecl() stops at the `,` and leaves the rest of the
	 * list to the module loop, which is the only place that can append several declarations at
	 * once. Consumes through the terminating `;`.
	 */
	bool ParseTrailingVariableDeclarators(FLangParser& Parser, const FVariableDecl& First, TArray<FDeclPtr>& OutDecls);

	namespace
	{
		/** The leading word of a directive payload, so a message says `#pragma` and not the whole line. */
		FString FirstWordOf(const FString& Text)
		{
			int32 End = 0;
			while (End < Text.Len() && !FChar::IsWhitespace(Text[End]))
			{
				++End;
			}
			return Text.Left(End);
		}

		/**
		 * A `.dsh` is a shared header: it is included, never compiled into an asset, so an `export`
		 * in one has nowhere to put what it would produce. The declaration is kept -- a language
		 * service still wants the symbol -- but the file cannot succeed.
		 */
		void ReportExportInHeader(FLangDiagnosticSink& Diagnostics, ELangFileKind FileKind, const FDecl& Decl)
		{
			if (FileKind != ELangFileKind::Dsh)
			{
				return;
			}

			const FFunctionDecl* Function = Decl.As<FFunctionDecl>();
			if (!Function || Function->Linkage != EFunctionLinkage::Export)
			{
				return;
			}

			Diagnostics.Error(
				TEXT("DSH3210"),
				Function->NameSpan,
				FText::Format(
					LOCTEXT("ExportInHeader", "A '.dsh' header cannot export '{0}'; only a '.dss' file produces assets."),
					FText::FromString(Function->Name)));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Construction
	// ---------------------------------------------------------------------------------------------

	FLangParser::FLangParser(
		const FLangSourceText& InSource,
		TArray<FLangToken>&& InTokens,
		ELangFrontend InFrontend,
		ELangFileKind InFileKind,
		FLangDiagnosticSink& InDiagnostics)
		: Source(InSource)
		, Tokens(MoveTemp(InTokens))
		, Frontend(InFrontend)
		, FileKind(InFileKind)
		, Diagnostics(InDiagnostics)
	{
		// The lexer always terminates with EndOfFile; a caller that built the array by hand might
		// not. Every cursor operation below leans on "the last token is EndOfFile", so make it true
		// here once instead of testing for it in Peek().
		if (Tokens.Num() == 0 || Tokens.Last().Kind != ELangTokenKind::EndOfFile)
		{
			FLangToken EndToken;
			EndToken.Kind = ELangTokenKind::EndOfFile;
			EndToken.Span = Source.MakeSpan(Source.Len(), 0);
			EndToken.bAtLineStart = true;
			Tokens.Add(MoveTemp(EndToken));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Cursor
	// ---------------------------------------------------------------------------------------------

	const FLangToken& FLangParser::Peek(int32 Ahead) const
	{
		const int32 Last = Tokens.Num() - 1;
		const int32 Target = (Ahead >= Last - Index) ? Last : FMath::Max(0, Index + Ahead);
		return Tokens[Target];
	}

	const FLangToken& FLangParser::Previous() const
	{
		return Tokens[FMath::Clamp(Index - 1, 0, Tokens.Num() - 1)];
	}

	bool FLangParser::AtEnd() const
	{
		return Current().Kind == ELangTokenKind::EndOfFile;
	}

	const FLangToken& FLangParser::Advance()
	{
		const int32 Consumed = Index;
		if (Index < Tokens.Num() - 1)
		{
			++Index;
		}
		return Tokens[Consumed];
	}

	bool FLangParser::Match(ELangTokenKind Kind)
	{
		if (!Check(Kind))
		{
			return false;
		}
		Advance();
		return true;
	}

	bool FLangParser::MatchKeyword(ELangKeyword Keyword)
	{
		if (!CheckKeyword(Keyword))
		{
			return false;
		}
		Advance();
		return true;
	}

	bool FLangParser::Expect(ELangTokenKind Kind, const TCHAR* Code, const FText& What)
	{
		if (Check(Kind))
		{
			Advance();
			return true;
		}

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("What"), What);
		Arguments.Add(TEXT("Token"), DescribeToken(Current()));
		return Diagnostics.Error(
			Code,
			Current().Span,
			FText::Format(LOCTEXT("ExpectedFound", "Expected {What}, found {Token}."), Arguments));
	}

	bool FLangParser::ExpectIdentifier(FString& OutName, FLangSpan& OutSpan, const TCHAR* Code, const FText& What)
	{
		if (Check(ELangTokenKind::Identifier))
		{
			const FLangToken& Token = Advance();
			OutName = Token.Text;
			OutSpan = Token.Span;
			return true;
		}

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("What"), What);
		Arguments.Add(TEXT("Token"), DescribeToken(Current()));
		return Diagnostics.Error(
			Code,
			Current().Span,
			FText::Format(LOCTEXT("ExpectedFoundIdentifier", "Expected {What}, found {Token}."), Arguments));
	}

	FLangSpan FLangParser::SpanFrom(int32 StartIndex) const
	{
		const FLangSpan& StartSpan = Tokens[FMath::Clamp(StartIndex, 0, Tokens.Num() - 1)].Span;
		const FLangSpan& EndSpan = Previous().Span;
		if (EndSpan.End() <= StartSpan.Offset)
		{
			// Nothing was consumed since StartIndex; the start token alone is the best span there is.
			return StartSpan;
		}
		return FLangSpan::Join(StartSpan, EndSpan);
	}

	FText FLangParser::DescribeToken(const FLangToken& Token) const
	{
		switch (Token.Kind)
		{
		case ELangTokenKind::EndOfFile:
			return LOCTEXT("DescribeEndOfFile", "end of file");
		case ELangTokenKind::Identifier:
			return FText::Format(LOCTEXT("DescribeIdentifier", "identifier '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::Keyword:
			return FText::Format(LOCTEXT("DescribeKeyword", "keyword '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::IntLiteral:
		case ELangTokenKind::FloatLiteral:
			return FText::Format(LOCTEXT("DescribeNumber", "number '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::StringLiteral:
			return FText::Format(LOCTEXT("DescribeString", "string '{0}'"), FText::FromString(Token.Text));
		case ELangTokenKind::DocComment:
			return LOCTEXT("DescribeDocComment", "a '///' comment");
		case ELangTokenKind::Directive:
			return FText::Format(LOCTEXT("DescribeDirective", "directive '#{0}'"), FText::FromString(FirstWordOf(Token.Text)));
		case ELangTokenKind::Unknown:
			return FText::Format(LOCTEXT("DescribeUnknown", "'{0}'"), FText::FromString(Token.Text));
		default:
			break;
		}

		const TCHAR* Spelling = GetLangTokenSpelling(Token.Kind);
		if (Spelling && Spelling[0] != TEXT('\0'))
		{
			return FText::Format(LOCTEXT("DescribeSpelling", "'{0}'"), FText::FromString(Spelling));
		}
		return FText::Format(LOCTEXT("DescribeKind", "'{0}'"), FText::FromString(LexToString(Token.Kind)));
	}

	bool FLangParser::FailAtEnd(const FText& WhileParsing)
	{
		return Diagnostics.Error(
			TEXT("DSH2150"),
			Current().Span,
			FText::Format(LOCTEXT("UnexpectedEndOfFile", "Unexpected end of file while parsing {0}."), WhileParsing));
	}

	// ---------------------------------------------------------------------------------------------
	// Trivia bookkeeping
	// ---------------------------------------------------------------------------------------------

	void FLangParser::RecordSkippedDocComment(const FLangToken& Token)
	{
		if (!bKeepTrivia || Token.Kind != ELangTokenKind::DocComment || Token.Span.Length <= 0)
		{
			return;
		}

		// The token's Text is the payload after `///`; the comment keeps the line verbatim, so the
		// printed file carries exactly the characters the author wrote.
		FLangComment Comment;
		Comment.Span = Token.Span;
		Comment.Text = Source.Slice(Token.Span);
		Comment.bBlock = false;
		SkippedComments.Add(MoveTemp(Comment));
	}

	void FLangParser::RecordSkippedDocBlock(const FDocBlock& Doc)
	{
		if (!bKeepTrivia || Doc.Span.Length <= 0)
		{
			return;
		}

		// The block's span joins every `///` token it consumed; cut it back into lines. Lines in
		// between that are ordinary `//` comments were Comment tokens already and are not taken
		// twice: only a line that starts with exactly three slashes is a doc line.
		const FString& Text = Source.GetText();
		const int32 End = FMath::Min(Doc.Span.End(), Text.Len());
		int32 Offset = FMath::Clamp(Doc.Span.Offset, 0, Text.Len());
		while (Offset < End)
		{
			while (Offset < End && FChar::IsWhitespace(Text[Offset]))
			{
				++Offset;
			}
			if (Offset >= End)
			{
				break;
			}

			int32 LineEnd = Offset;
			while (LineEnd < End && Text[LineEnd] != TEXT('\n') && Text[LineEnd] != TEXT('\r'))
			{
				++LineEnd;
			}

			const bool bDocLine = LineEnd - Offset >= 3
				&& Text[Offset] == TEXT('/') && Text[Offset + 1] == TEXT('/') && Text[Offset + 2] == TEXT('/')
				&& !(LineEnd - Offset >= 4 && Text[Offset + 3] == TEXT('/'));
			if (bDocLine)
			{
				FLangComment Comment;
				Comment.Span = Source.MakeSpan(Offset, LineEnd - Offset);
				Comment.Text = Text.Mid(Offset, LineEnd - Offset);
				Comment.bBlock = false;
				SkippedComments.Add(MoveTemp(Comment));
			}

			Offset = LineEnd;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Recovery
	// ---------------------------------------------------------------------------------------------

	void FLangParser::SkipToDeclarationBoundary()
	{
		int32 Depth = 0;
		int32 ParenDepth = 0;
		while (!AtEnd())
		{
			const FLangToken& Token = Current();

			// Anything at the start of a line that can only be the start of the NEXT declaration
			// (a `///` block, a `#` line, `struct`, `import`, or `[prefix keywords] Type Name`)
			// is not part of the broken one; stop before it so it is not swallowed. Most broken
			// declarations fail at a token that has already consumed their own `;` or `}` (a
			// missing `;`, a prototype without `extern`, a body on an `extern`), and without this
			// rule the skip would run straight through the declaration that follows.
			if (Depth == 0 && ParenDepth == 0 && Token.bAtLineStart)
			{
				const bool bStructOrImport = Token.Kind == ELangTokenKind::Keyword
					&& (Token.Keyword == ELangKeyword::Struct || Token.Keyword == ELangKeyword::Import);
				if (Token.Kind == ELangTokenKind::DocComment
					|| Token.Kind == ELangTokenKind::Directive
					|| bStructOrImport
					|| (Token.Kind == ELangTokenKind::Identifier && IsLegacyTopLevelWord(Token.Text))
					|| LooksLikeDeclarationStart())
				{
					return;
				}
			}

			switch (Token.Kind)
			{
			case ELangTokenKind::LeftBrace:
				++Depth;
				Advance();
				break;

			case ELangTokenKind::RightBrace:
				Advance();
				if (Depth == 0)
				{
					// A stray `}` at file scope: it closes nothing, but it is still a boundary.
					return;
				}
				if (--Depth == 0)
				{
					return;
				}
				break;

			case ELangTokenKind::LeftParen:
				++ParenDepth;
				Advance();
				break;

			case ELangTokenKind::RightParen:
				// The skip may start inside a parameter list; a `)` it did not see opened is not a
				// reason to disable the line-start rule for the rest of the file.
				ParenDepth = FMath::Max(0, ParenDepth - 1);
				Advance();
				break;

			case ELangTokenKind::Semicolon:
				Advance();
				if (Depth == 0)
				{
					return;
				}
				break;

			default:
				Advance();
				break;
			}
		}
	}

	void FLangParser::SkipToStatementBoundary()
	{
		int32 Depth = 0;
		while (!AtEnd())
		{
			switch (Current().Kind)
			{
			case ELangTokenKind::Semicolon:
				Advance();
				if (Depth == 0)
				{
					return;
				}
				break;

			case ELangTokenKind::LeftBrace:
				++Depth;
				Advance();
				break;

			case ELangTokenKind::RightBrace:
				if (Depth == 0)
				{
					// The `}` that closes the enclosing block: stop BEFORE it, so the block parser
					// sees its own terminator.
					return;
				}
				--Depth;
				Advance();
				break;

			default:
				Advance();
				break;
			}
		}
	}

	bool FLangParser::SkipBalancedBraces()
	{
		if (!Check(ELangTokenKind::LeftBrace))
		{
			// Callers only reach here standing on a `{`; nothing to skip and nothing to report.
			return false;
		}

		int32 Depth = 0;
		do
		{
			if (AtEnd())
			{
				return FailAtEnd(LOCTEXT("WhileParsingABlock", "a block"));
			}

			const ELangTokenKind Kind = Advance().Kind;
			if (Kind == ELangTokenKind::LeftBrace)
			{
				++Depth;
			}
			else if (Kind == ELangTokenKind::RightBrace)
			{
				--Depth;
			}
		}
		while (Depth > 0);

		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// Entry
	// ---------------------------------------------------------------------------------------------

	TUniquePtr<FModule> FLangParser::ParseModule()
	{
		TUniquePtr<FModule> Module = MakeUnique<FModule>();
		Module->FilePath = Source.GetPath();
		Module->FileKind = FileKind;

		while (!AtEnd())
		{
			const int32 StartIndex = Index;

			// A `.dsh` holds both syntaxes, one declaration at a time: past the
			// `///` block, an exact-case 1.x word hands the declaration to the legacy front end.
			if (FileKind == ELangFileKind::Dsh)
			{
				int32 Ahead = 0;
				while (Peek(Ahead).Kind == ELangTokenKind::DocComment || Peek(Ahead).Kind == ELangTokenKind::Semicolon)
				{
					++Ahead;
				}
				const FLangToken& Word = Peek(Ahead);
				if (Word.Kind == ELangTokenKind::Identifier && IsLegacyTopLevelWord(Word.Text))
				{
					FDocBlock Doc;
					ParseDocBlock(Doc);
					while (Match(ELangTokenKind::Semicolon))
					{
						ParseDocBlock(Doc);
					}

					TArray<FDeclPtr> LegacyDeclarations;
					if (!ParseLegacyDeclarationInHeader(MoveTemp(Doc), LegacyDeclarations))
					{
						SkipToLegacyTopLevelBoundary();
					}
					for (FDeclPtr& LegacyDeclaration : LegacyDeclarations)
					{
						if (LegacyDeclaration.IsValid())
						{
							Module->Declarations.Add(MoveTemp(LegacyDeclaration));
						}
					}

					if (Index == StartIndex)
					{
						Advance();
					}
					continue;
				}
			}

			FDeclPtr Declaration = ParseDeclaration();
			if (Declaration.IsValid())
			{
				ReportExportInHeader(Diagnostics, FileKind, *Declaration);

				// The node itself never moves (only the TUniquePtr does), so this stays valid
				// across the Add below and across the reallocation the extra names may cause.
				const FVariableDecl* First = Declaration->As<FVariableDecl>();
				Module->Declarations.Add(MoveTemp(Declaration));

				if (First && Check(ELangTokenKind::Comma))
				{
					TArray<FDeclPtr> Extra;
					ParseTrailingVariableDeclarators(*this, *First, Extra);
					for (FDeclPtr& Additional : Extra)
					{
						if (Additional.IsValid())
						{
							ReportExportInHeader(Diagnostics, FileKind, *Additional);
							Module->Declarations.Add(MoveTemp(Additional));
						}
					}
				}
			}
			else
			{
				SkipToDeclarationBoundary();
			}

			// The loop must always make progress: a declaration that failed without consuming
			// anything, and a recovery that had nothing to skip, would otherwise spin here.
			if (Index == StartIndex)
			{
				Advance();
			}
		}

		return Module;
	}

	FExprPtr FLangParser::ParseStandaloneExpression()
	{
		FExprPtr Expression = ParseExpression();
		if (!Expression)
		{
			return nullptr;
		}

		if (!AtEnd())
		{
			// Returning the partial expression here would be the 1.x silent-truncation bug wearing
			// a new hat (`a%b` parsed as `a`), so a trailing token loses the whole result.
			Diagnostics.Error(
				TEXT("DSH3211"),
				Current().Span,
				FText::Format(
					LOCTEXT("TrailingTokenAfterExpression", "Expected the end of the expression, found {0}."),
					DescribeToken(Current())));
			return nullptr;
		}

		return Expression;
	}
}

// -------------------------------------------------------------------------------------------------
// Trivia: comments and blank lines kept beside the tree (FLangParseOptions::bKeepTrivia)
// -------------------------------------------------------------------------------------------------
//
// The lexer emits Comment tokens only when asked; ParseDreamShaderLang pulls every one of them out of
// the stream before the parser runs, so no grammar rule ever sees a comment. After the parse, the
// attach pass hangs each comment on an anchor -- a file-scope declaration, or a statement of a block at
// any depth:
//
//   - inside an anchor's span, and not inside a block that anchor holds: that anchor's Trailing when
//     the comment starts on the anchor's last line, else one of its Leading comments;
//   - else, on the last line of the anchor that ended just before it: that anchor's Trailing;
//   - else a Leading comment of the next anchor, unless that anchor lies past the end of the innermost
//     block around the comment, in which case the block's Inner;
//   - else FModule::TrailingComments;
//   - a comment inside an opaque body is part of RawBody already and is not attached a second time.
//
// Anchors are compared by source position only, never by where they sit in the tree. The legacy front
// end gives every node it synthesizes the span of the 1.x construct it came from and then places it
// where 2.0 wants it (Outputs bindings at the end of the entry body, Properties as file-scope
// uniforms), so the statements of one body interleave in the source with declarations of the module;
// comparing positions is what lets a comment travel with the construct it was written next to.
//
// Every comment lands somewhere, so the printed file carries the same multiset of comments as the
// source; `dsc migrate` refuses to write a file for which that is false (DSH9092).

namespace UE::DreamShader::Lang::Private::ParserTrivia
{
	/** Moves every Comment token out of the stream, in order, into OutComments. */
	static void ExtractCommentTokens(TArray<FLangToken>& Tokens, TArray<FLangComment>& OutComments)
	{
		int32 Write = 0;
		for (int32 Read = 0; Read < Tokens.Num(); ++Read)
		{
			if (Tokens[Read].Kind == ELangTokenKind::Comment)
			{
				FLangComment Comment;
				Comment.Text = MoveTemp(Tokens[Read].Text);
				Comment.Span = Tokens[Read].Span;
				Comment.bBlock = Comment.Text.StartsWith(TEXT("/*"), ESearchCase::CaseSensitive);
				OutComments.Add(MoveTemp(Comment));
				continue;
			}
			if (Write != Read)
			{
				Tokens[Write] = MoveTemp(Tokens[Read]);
			}
			++Write;
		}
		Tokens.SetNum(Write);
	}

	class FParserTriviaAttacher
	{
	public:
		FParserTriviaAttacher(const FLangSourceText& InSource, FModule& InModule, const TArray<FLangComment>& InComments)
			: Source(InSource)
			, Module(InModule)
			, Comments(InComments)
		{
		}

		void Run()
		{
			for (const FDeclPtr& Decl : Module.Declarations)
			{
				if (!Decl.IsValid())
				{
					continue;
				}
				// A declaration starts at its `///` block: a comment between two doc lines, or between
				// the block and the declaration, belongs to that declaration.
				int32 Start = Decl->Span.Offset;
				if (Decl->Doc.Span.Length > 0)
				{
					Start = FMath::Min(Start, Decl->Doc.Span.Offset);
				}
				AddAnchor(*Decl, Start);
				GatherInside(*Decl);
			}

			BuildEndIndex();

			for (const FLangComment& Comment : Comments)
			{
				Place(Comment);
			}

			ComputeBlankLines();
		}

	private:
		/** A node trivia can hang on: a file-scope declaration, or a statement of a block at any depth. */
		struct FAnchor
		{
			const FNode* Node = nullptr;
			int32 Start = 0;
			int32 End = 0;
			int32 StartLine = 1;
			int32 EndLine = 1;
		};

		/** A block whose braces are in the source: a comment after its last statement is its Inner. */
		struct FBlockRange
		{
			const FNode* Node = nullptr;
			int32 Start = 0;
			int32 End = 0;
			int32 StartLine = 1;
		};

		int32 LineOf(const int32 Offset) const
		{
			int32 Line = 1;
			int32 Column = 1;
			Source.GetLineAndColumn(Offset, Line, Column);
			return Line;
		}

		bool IsBlankLine(const int32 Line) const
		{
			const FString Text = Source.GetLineText(Line);
			for (const TCHAR Character : Text)
			{
				if (!FChar::IsWhitespace(Character))
				{
					return false;
				}
			}
			return true;
		}

		int32 CountBlankLines(const int32 AfterLine, const int32 BeforeLine) const
		{
			int32 Count = 0;
			for (int32 Line = FMath::Max(AfterLine + 1, 1); Line < BeforeLine; ++Line)
			{
				if (IsBlankLine(Line))
				{
					++Count;
				}
			}
			return Count;
		}

		void AddAnchor(const FNode& Node, const int32 Start)
		{
			const int32 End = Node.Span.End();
			if (End <= Start)
			{
				// No span (a hand-built node): nothing can be placed next to it.
				return;
			}
			FAnchor& Anchor = Anchors.AddDefaulted_GetRef();
			Anchor.Node = &Node;
			Anchor.Start = Start;
			Anchor.End = End;
			Anchor.StartLine = LineOf(Start);
			Anchor.EndLine = LineOf(End - 1);
		}

		void GatherBlock(const FBlockStmt& Block)
		{
			if (Block.Span.Length > 0)
			{
				FBlockRange& Range = Blocks.AddDefaulted_GetRef();
				Range.Node = &Block;
				Range.Start = Block.Span.Offset;
				Range.End = Block.Span.End();
				Range.StartLine = Block.Span.Line;
			}

			for (const FStmtPtr& Statement : Block.Statements)
			{
				if (Statement.IsValid())
				{
					AddAnchor(*Statement, Statement->Span.Offset);
					GatherInside(*Statement);
				}
			}
		}

		/**
		 * A branch or loop body. A block is walked; a chained `else if` is walked but not anchored, since
		 * the printer writes the chain flat and a comment inside it goes to the chain's first `if`; a
		 * single statement is walked for the blocks it may hold, and is not anchored either, because it
		 * is printed inside braces the source did not have.
		 */
		void GatherBranch(const FStmt* Branch)
		{
			if (!Branch)
			{
				return;
			}
			if (const FBlockStmt* Block = Branch->As<FBlockStmt>())
			{
				GatherBlock(*Block);
				return;
			}
			GatherInside(*Branch);
		}

		void GatherInside(const FNode& Node)
		{
			if (const FPassDecl* Pass = Node.As<FPassDecl>())
			{
				// A `.dsp` pass block: its statements are anchors like a body's, and the declaration itself is the
				// block a comment after the last statement belongs to (its Inner).
				if (Pass->BodySpan.Length > 0)
				{
					FBlockRange& Range = Blocks.AddDefaulted_GetRef();
					Range.Node = Pass;
					Range.Start = Pass->BodySpan.Offset;
					Range.End = Pass->BodySpan.End();
					Range.StartLine = Pass->BodySpan.Line;
				}
				for (const TUniquePtr<FPassStmt>& Statement : Pass->Statements)
				{
					if (Statement.IsValid())
					{
						AddAnchor(*Statement, Statement->Span.Offset);
						// An `hlsl` block's comments are HLSL's, part of RawBody.
						if (Statement->StmtKind == EPassStmtKind::Hlsl && Statement->BodySpan.Length > 0)
						{
							OpaqueBodies.Add(Statement->BodySpan);
						}
					}
				}
				return;
			}
			if (const FHlslBlockDecl* HlslBlock = Node.As<FHlslBlockDecl>())
			{
				if (HlslBlock->BodySpan.Length > 0)
				{
					OpaqueBodies.Add(HlslBlock->BodySpan);
				}
				return;
			}
			if (const FFunctionDecl* Function = Node.As<FFunctionDecl>())
			{
				if (Function->bOpaqueBody && Function->BodySpan.Length > 0)
				{
					OpaqueBodies.Add(Function->BodySpan);
				}
				if (Function->Body.IsValid())
				{
					GatherBlock(*Function->Body);
				}
				return;
			}
			if (const FBlockStmt* Block = Node.As<FBlockStmt>())
			{
				GatherBlock(*Block);
				return;
			}
			if (const FIfStmt* If = Node.As<FIfStmt>())
			{
				GatherBranch(If->Then.Get());
				GatherBranch(If->Else.Get());
				return;
			}
			if (const FForStmt* For = Node.As<FForStmt>())
			{
				GatherBranch(For->Body.Get());
				return;
			}
			if (const FWhileStmt* While = Node.As<FWhileStmt>())
			{
				GatherBranch(While->Body.Get());
				return;
			}
			if (const FDoWhileStmt* DoWhile = Node.As<FDoWhileStmt>())
			{
				GatherBranch(DoWhile->Body.Get());
			}
		}

		/** Anchors ordered by End, with the running maximum of their last lines, for the blank-line count. */
		void BuildEndIndex()
		{
			ByEnd.Reserve(Anchors.Num());
			for (int32 Index = 0; Index < Anchors.Num(); ++Index)
			{
				ByEnd.Add(Index);
			}
			ByEnd.StableSort([this](const int32 A, const int32 B) { return Anchors[A].End < Anchors[B].End; });

			MaxEndLineByEnd.SetNum(ByEnd.Num());
			int32 Running = 0;
			for (int32 Index = 0; Index < ByEnd.Num(); ++Index)
			{
				Running = FMath::Max(Running, Anchors[ByEnd[Index]].EndLine);
				MaxEndLineByEnd[Index] = Running;
			}
		}

		/** The last line of everything that ended at or before Offset; 0 when nothing did. */
		int32 LastEndLineBefore(const int32 Offset) const
		{
			int32 Low = 0;
			int32 High = ByEnd.Num();
			while (Low < High)
			{
				const int32 Mid = Low + (High - Low) / 2;
				if (Anchors[ByEnd[Mid]].End <= Offset)
				{
					Low = Mid + 1;
				}
				else
				{
					High = Mid;
				}
			}
			return Low > 0 ? MaxEndLineByEnd[Low - 1] : 0;
		}

		int32 FindSmallestBlockContaining(const int32 Offset) const
		{
			int32 Best = INDEX_NONE;
			for (int32 Index = 0; Index < Blocks.Num(); ++Index)
			{
				const FBlockRange& Range = Blocks[Index];
				if (Offset >= Range.Start && Offset < Range.End
					&& (Best == INDEX_NONE || (Range.End - Range.Start) < (Blocks[Best].End - Blocks[Best].Start)))
				{
					Best = Index;
				}
			}
			return Best;
		}

		bool HasTrailing(const FNode* Node) const
		{
			const FLangTrivia* Trivia = Module.Trivia.Find(Node);
			return Trivia && Trivia->Trailing.IsSet();
		}

		void Place(const FLangComment& Comment)
		{
			const int32 Offset = Comment.Span.Offset;

			for (const FLangSpan& Body : OpaqueBodies)
			{
				if (Offset >= Body.Offset && Offset < Body.End())
				{
					// Part of RawBody, printed with it.
					return;
				}
			}

			int32 Containing = INDEX_NONE;
			int32 Previous = INDEX_NONE;
			int32 Next = INDEX_NONE;
			for (int32 Index = 0; Index < Anchors.Num(); ++Index)
			{
				const FAnchor& Anchor = Anchors[Index];
				if (Offset >= Anchor.Start && Offset < Anchor.End)
				{
					if (Containing == INDEX_NONE || (Anchor.End - Anchor.Start) < (Anchors[Containing].End - Anchors[Containing].Start))
					{
						Containing = Index;
					}
				}
				else if (Anchor.End <= Offset)
				{
					if (Previous == INDEX_NONE || Anchor.End > Anchors[Previous].End)
					{
						Previous = Index;
					}
				}
				else if (Next == INDEX_NONE || Anchor.Start < Anchors[Next].Start)
				{
					Next = Index;
				}
			}

			const int32 Block = FindSmallestBlockContaining(Offset);

			// Inside a node, and not inside a block that node holds: the node takes it.
			if (Containing != INDEX_NONE)
			{
				const FAnchor& Anchor = Anchors[Containing];
				const bool bInsideOwnedBlock = Block != INDEX_NONE
					&& Blocks[Block].Start >= Anchor.Start
					&& Blocks[Block].End <= Anchor.End;
				if (!bInsideOwnedBlock)
				{
					if (Comment.Span.Line >= Anchor.EndLine && !HasTrailing(Anchor.Node))
					{
						Module.Trivia.FindOrAdd(Anchor.Node).Trailing.Emplace(Comment);
					}
					else
					{
						Module.Trivia.FindOrAdd(Anchor.Node).Leading.Add(Comment);
					}
					return;
				}
			}

			if (Previous != INDEX_NONE
				&& Comment.Span.Line == Anchors[Previous].EndLine
				&& !HasTrailing(Anchors[Previous].Node))
			{
				Module.Trivia.FindOrAdd(Anchors[Previous].Node).Trailing.Emplace(Comment);
				return;
			}

			if (Block != INDEX_NONE && (Next == INDEX_NONE || Anchors[Next].Start >= Blocks[Block].End))
			{
				Module.Trivia.FindOrAdd(Blocks[Block].Node).Inner.Add(Comment);
				return;
			}

			if (Next != INDEX_NONE)
			{
				Module.Trivia.FindOrAdd(Anchors[Next].Node).Leading.Add(Comment);
				return;
			}

			Module.TrailingComments.Add(Comment);
		}

		void ComputeBlankLines()
		{
			for (const FAnchor& Anchor : Anchors)
			{
				int32 FromLine = LastEndLineBefore(Anchor.Start);

				const int32 Block = FindSmallestBlockContaining(Anchor.Start);
				if (Block != INDEX_NONE)
				{
					FromLine = FMath::Max(FromLine, Blocks[Block].StartLine);
				}

				// Counted up to the node's first printed line: its first Leading comment when it has one. The
				// printer finds the gaps between those comments and the node from their spans, which it can; the
				// gap above the comments needs the source text, which only this pass has.
				int32 TargetLine = Anchor.StartLine;
				if (const FLangTrivia* Existing = Module.Trivia.Find(Anchor.Node))
				{
					if (Existing->Leading.Num() > 0)
					{
						TargetLine = FMath::Min(TargetLine, Existing->Leading[0].Span.Line);
					}
				}

				const int32 Blank = CountBlankLines(FromLine, TargetLine);
				if (Blank > 0)
				{
					Module.Trivia.FindOrAdd(Anchor.Node).BlankLinesBefore = Blank;
				}
			}
		}

		const FLangSourceText& Source;
		FModule& Module;
		const TArray<FLangComment>& Comments;
		TArray<FAnchor> Anchors;
		TArray<FBlockRange> Blocks;
		TArray<FLangSpan> OpaqueBodies;
		TArray<int32> ByEnd;
		TArray<int32> MaxEndLineByEnd;
	};

	/** The one attach pass both front ends run (see the section comment above). */
	static void AttachDreamShaderTrivia(const FLangSourceText& Source, TArray<FLangComment>&& InComments, FModule& Module)
	{
		TArray<FLangComment> Comments = MoveTemp(InComments);
		Comments.StableSort([](const FLangComment& A, const FLangComment& B) { return A.Span.Offset < B.Span.Offset; });

		// The same line recorded twice (a skipped `///` token and the block it belonged to) is one comment.
		for (int32 Index = Comments.Num() - 1; Index > 0; --Index)
		{
			if (Comments[Index].Span.Offset == Comments[Index - 1].Span.Offset)
			{
				Comments.RemoveAt(Index);
			}
		}

		Module.Trivia.Reset();
		Module.TrailingComments.Reset();

		FParserTriviaAttacher Attacher(Source, Module, Comments);
		Attacher.Run();
	}
}

namespace UE::DreamShader::Lang
{
	namespace
	{
		/**
		 * The spans of a module's `/// @custom` bodies and `.dsp` `hlsl` blocks, braces included.
		 *
		 * A `@custom` body is opaque HLSL, and so is an `hlsl` block. The parser lexes it only far enough
		 * to find the brace that closes it and then slices the text straight out of the source, so what
		 * the lexer made of the characters in between was never used for anything. Its complaints about
		 * them -- a `@` in a macro, a `$`, a `'`, a `#` that is not the first thing on its line -- are
		 * therefore not facts about the program: they are this front end objecting to a language
		 * that is not its own. `dsc check --shaders` is what gets to complain about that text.
		 */
		TArray<FLangSpan> GatherOpaqueBodySpans(const FModule& Module)
		{
			TArray<FLangSpan> Spans;
			Module.ForEachDecl(ENodeKind::FunctionDecl, [&Spans](const FDecl& Decl)
			{
				const FFunctionDecl& Function = static_cast<const FFunctionDecl&>(Decl);
				if (Function.bOpaqueBody && Function.BodySpan.Length > 0)
				{
					Spans.Add(Function.BodySpan);
				}
			});
			Module.ForEachDecl(ENodeKind::HlslBlockDecl, [&Spans](const FDecl& Decl)
			{
				const FHlslBlockDecl& Block = static_cast<const FHlslBlockDecl&>(Decl);
				if (Block.BodySpan.Length > 0)
				{
					Spans.Add(Block.BodySpan);
				}
			});
			Module.ForEachDecl(ENodeKind::PassDecl, [&Spans](const FDecl& Decl)
			{
				for (const TUniquePtr<FPassStmt>& Statement : static_cast<const FPassDecl&>(Decl).Statements)
				{
					if (Statement.IsValid() && Statement->StmtKind == EPassStmtKind::Hlsl && Statement->BodySpan.Length > 0)
					{
						Spans.Add(Statement->BodySpan);
					}
				}
			});
			return Spans;
		}

		bool StartsInsideAnyOf(const FLangSpan& Span, const TArray<FLangSpan>& Ranges)
		{
			for (const FLangSpan& Range : Ranges)
			{
				if (Span.Offset >= Range.Offset && Span.Offset < Range.End())
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Whether a lexical diagnostic may be dropped for falling inside an opaque body.
		 *
		 * Most may: they say "this character begins no token", which is not a fact about text that
		 * was never DreamShaderLang. The two unterminated-construct codes may not. Those mean the
		 * lexer ran past where it believed it was, and the brace matching that decided where the
		 * body ENDS ran over those same tokens -- so the span being suppressed against is itself
		 * suspect. Dropping the one clue would leave a body captured to the wrong brace with
		 * nothing at all said about why.
		 */
		bool MaySuppressInsideOpaqueBody(const FLangDiagnostic& Diagnostic)
		{
			return !Diagnostic.Code.Equals(TEXT("DSH2102"), ESearchCase::CaseSensitive)
				&& !Diagnostic.Code.Equals(TEXT("DSH2103"), ESearchCase::CaseSensitive);
		}

		void Emit(FLangDiagnosticSink& Sink, const FLangDiagnostic& Diagnostic)
		{
			switch (Diagnostic.Severity)
			{
			case ELangSeverity::Error:
				Sink.Error(*Diagnostic.Code, Diagnostic.Span, Diagnostic.Message);
				break;
			case ELangSeverity::Warning:
				Sink.Warning(*Diagnostic.Code, Diagnostic.Span, Diagnostic.Message);
				break;
			case ELangSeverity::Info:
				Sink.Info(*Diagnostic.Code, Diagnostic.Span, Diagnostic.Message);
				break;
			}
		}
	}

	FLangParseResult ParseDreamShaderLang(const FLangSourceText& Source, const FLangParseOptions& Options)
	{
		FLangParseResult Result;
		Result.Diagnostics = FLangDiagnosticSink(Source.GetPath());

		const ELangFileKind FileKind = GetLangFileKindFromPath(Source.GetPath());

		ELangFrontend Frontend = Options.Frontend;
		if (Frontend == ELangFrontend::Auto)
		{
			// The two frozen 1.x extensions take the legacy front end. `.dss`, `.dsi`, `.dsp` and an unknown or
			// absent extension take the 2.0 one -- a `.dsp` with its `buffer` / `pass` declarations, which the 2.0
			// module loop reads only in a `.dsp` (ParseDeclaration); so does a `.dsh`, whose module loop hands each
			// 1.x declaration to the legacy front end by itself.
			switch (FileKind)
			{
			case ELangFileKind::Dsm:
			case ELangFileKind::Dsf:
				Frontend = ELangFrontend::Legacy;
				break;
			case ELangFileKind::Dss:
			case ELangFileKind::Dsh:
			case ELangFileKind::Dsi:
			case ELangFileKind::Dsp:
			case ELangFileKind::Unknown:
			default:
				Frontend = ELangFrontend::Dss;
				break;
			}
		}

		const bool bLegacyModule = Frontend == ELangFrontend::Legacy;

		FLangLexOptions LexOptions;
		// 1.x has no doc comments: in a legacy module a `///` line is an ordinary comment.
		LexOptions.bEmitDocComments = !bLegacyModule;
		LexOptions.bEmitDirectives = true;
		// A 1.x Properties default may be the engine's asset shell with no double quotes around it; a `.dsh` may
		// hold 1.x blocks too.
		LexOptions.bLexAssetShellQuotes = bLegacyModule || FileKind == ELangFileKind::Dsh;
		// Comments become tokens only when they are kept; they are taken out of the stream again
		// before the parser runs, so the grammar is the same either way.
		LexOptions.bEmitComments = Options.bKeepTrivia;

		// The two stages report into their own sinks so they can be merged once the tree says which
		// stretches of the file were opaque HLSL (GatherOpaqueBodySpans). A lexing error is recorded
		// and the token stream is used anyway: the Unknown token it leaves behind fails at its own
		// spot, which is a better story than "the file is broken".
		FLangDiagnosticSink LexicalDiagnostics(Source.GetPath());
		FLangDiagnosticSink ParseDiagnostics(Source.GetPath());

		TArray<FLangToken> Tokens;
		LexDreamShaderLang(Source, LexOptions, Tokens, LexicalDiagnostics);

		TArray<FLangComment> Comments;
		if (Options.bKeepTrivia)
		{
			Private::ParserTrivia::ExtractCommentTokens(Tokens, Comments);
		}

		Private::FLangParser Parser(Source, MoveTemp(Tokens), Frontend, FileKind, ParseDiagnostics);
		Parser.SetKeepTrivia(Options.bKeepTrivia);

		// A legacy module always reports what it knows about the 1.x source; a header only when it held a
		// legacy declaration (checked after the parse).
		if (bLegacyModule || FileKind == ELangFileKind::Dsh)
		{
			Result.Legacy = MakeUnique<FLegacyMigrationInfo>();
			Parser.SetLegacyInfo(Result.Legacy.Get());
		}

		Result.Module = bLegacyModule ? Parser.ParseLegacyModule(Result.Legacy.Get()) : Parser.ParseModule();

		if (!bLegacyModule && Result.Legacy.IsValid())
		{
			bool bAnyLegacyDeclaration = false;
			if (Result.Module.IsValid())
			{
				for (const FDeclPtr& Declaration : Result.Module->Declarations)
				{
					bAnyLegacyDeclaration |= Declaration.IsValid() && Declaration->bLegacy;
				}
			}
			if (!bAnyLegacyDeclaration)
			{
				Parser.SetLegacyInfo(nullptr);
				Result.Legacy.Reset();
			}
		}

		if (Options.bKeepTrivia && Result.Module.IsValid())
		{
			Comments.Append(Parser.TakeSkippedComments());
			Private::ParserTrivia::AttachDreamShaderTrivia(Source, MoveTemp(Comments), *Result.Module);
		}

		TArray<FLangSpan> OpaqueBodies;
		if (Result.Module.IsValid())
		{
			OpaqueBodies = GatherOpaqueBodySpans(*Result.Module);
		}

		// Both lists are already in source order, so one merge keeps them that way: whichever stage
		// noticed it, the first complaint about a file should be the first one reported.
		const TArray<FLangDiagnostic>& Lexical = LexicalDiagnostics.GetDiagnostics();
		const TArray<FLangDiagnostic>& Parsed = ParseDiagnostics.GetDiagnostics();
		int32 LexicalIndex = 0;
		int32 ParsedIndex = 0;
		while (LexicalIndex < Lexical.Num() || ParsedIndex < Parsed.Num())
		{
			const bool bTakeLexical = ParsedIndex >= Parsed.Num()
				|| (LexicalIndex < Lexical.Num() && Lexical[LexicalIndex].Span.Offset <= Parsed[ParsedIndex].Span.Offset);

			if (bTakeLexical)
			{
				const FLangDiagnostic& Diagnostic = Lexical[LexicalIndex++];
				if (!StartsInsideAnyOf(Diagnostic.Span, OpaqueBodies))
				{
					Emit(Result.Diagnostics, Diagnostic);
				}
			}
			else
			{
				Emit(Result.Diagnostics, Parsed[ParsedIndex++]);
			}
		}

		return Result;
	}

	FExprPtr ParseDreamShaderLangExpression(const FLangSourceText& Source, FLangDiagnosticSink& Diagnostics)
	{
		// An expression has no declarations to attach a `///` block to and no line for a `#`
		// directive to own, so both are lexed as plain comments here.
		FLangLexOptions LexOptions;
		LexOptions.bEmitDocComments = false;
		LexOptions.bEmitDirectives = false;

		TArray<FLangToken> Tokens;
		LexDreamShaderLang(Source, LexOptions, Tokens, Diagnostics);

		Private::FLangParser Parser(
			Source,
			MoveTemp(Tokens),
			ELangFrontend::Dss,
			GetLangFileKindFromPath(Source.GetPath()),
			Diagnostics);
		return Parser.ParseStandaloneExpression();
	}
}

#undef LOCTEXT_NAMESPACE
