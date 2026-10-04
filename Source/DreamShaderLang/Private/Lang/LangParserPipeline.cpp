// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The `.dsp` half of the 2.0 parser: the declarations a Custom Pass pipeline adds to the language,
//
//   buffer <Name> : <Format>[(<Key> = <Value>, ...)];
//   pass <Name> : <kind> { <Key> = <Value>; read [<Slot> =] <Buffer>[.Previous]; write [<Slot> =] <Buffer>; param <P> = <E>; hlsl { ... } }
//   hlsl { ... }
//
// and nothing else -- `#pragma pipeline` is read with the other pragmas (LangParserDeclarations.cpp), `uniform`
// and `static const` are the ordinary declarations, values are the ordinary expression grammar. An `hlsl` block, in a
// pass or at file scope, is HLSL for the shader compiler (DreamShader_Plan/10): captured verbatim as a `/// @custom`
// body is, braces matched over the tokens, and never parsed; what it holds is the binder's to judge.
//
// `buffer`, `pass` and `hlsl` are keywords only at the start of a declaration in a `.dsp`, and `read`, `write`, `param`
// and `hlsl` only at the start of a statement in a pass block -- always there, whatever follows, since no pass has a
// setting by those names: `read;` is a binding that names no buffer (DSH2304), not a setting without '=' (DSH2302). The
// lexer knows none of them, so a `.dss` that names a variable `buffer` or a parameter `pass` reads exactly as it did.
// Outside a `.dsp` the one thing this unit does is put a better message on text that was always a syntax error (DSH3310).
//
// Recovery follows ParseBlock: a statement that fails costs that statement, the block keeps parsing, and a pass whose
// `}` is missing ends where the next `buffer` / `pass` declaration starts on a line of its own.
//
// Diagnostics owned by this unit: DSH2300-DSH2314 (pass blocks, bindings, buffer argument lists, the `{` of an `hlsl`
// block -- DSH2313, raised for us by CaptureRawBody),
// DSH3300-DSH3305 (the declaration heads) and DSH3310 (a `.dsp` declaration in another kind of file). DSH2150 is
// raised for us by FailAtEnd; `#pragma pipeline` is DSH3202's, with the other pragmas (LangParserDeclarations.cpp).

#include "LangParserInternal.h"

#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Lang/LangToken.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "DreamShader.Lang.Pipeline"

namespace UE::DreamShader::Lang::Private
{
	// A named namespace, not an anonymous one: the module builds as one unity blob.
	namespace LangParserPipelinePrivate
	{
		static bool IsPipelineDeclarationWordText(const FString& Text)
		{
			return Text.Equals(TEXT("buffer"), ESearchCase::CaseSensitive) || Text.Equals(TEXT("pass"), ESearchCase::CaseSensitive);
		}

		/** `read` / `write` / `param`: binding words, at the start of a pass statement only. */
		static bool IsPassBindingWordText(const FString& Text)
		{
			return Text.Equals(TEXT("read"), ESearchCase::CaseSensitive)
				|| Text.Equals(TEXT("write"), ESearchCase::CaseSensitive)
				|| Text.Equals(TEXT("param"), ESearchCase::CaseSensitive);
		}

		/** `hlsl`: the word that opens a block of inline HLSL, at file scope or at the start of a pass statement. */
		static bool IsHlslBlockWordText(const FString& Text)
		{
			return Text.Equals(TEXT("hlsl"), ESearchCase::CaseSensitive);
		}
	}

	bool FLangParser::IsAtPipelineDeclarationWord() const
	{
		if (FileKind != ELangFileKind::Dsp || IsLegacyMode())
		{
			return false;
		}
		const FLangToken& Token = Current();
		return Token.Kind == ELangTokenKind::Identifier && LangParserPipelinePrivate::IsPipelineDeclarationWordText(Token.Text);
	}

	bool FLangParser::IsAtHlslBlockWord() const
	{
		if (FileKind != ELangFileKind::Dsp || IsLegacyMode())
		{
			return false;
		}
		const FLangToken& Token = Current();
		return Token.Kind == ELangTokenKind::Identifier && LangParserPipelinePrivate::IsHlslBlockWordText(Token.Text);
	}

	bool FLangParser::ReportPipelineDeclarationOutsideDsp(const FTypeRef& Type, const FString& Name, const FLangSpan& NameSpan)
	{
		if (FileKind == ELangFileKind::Dsp || IsLegacyMode())
		{
			return false;
		}
		if (Type.Category != ETypeCategory::Named || !LangParserPipelinePrivate::IsPipelineDeclarationWordText(Type.Name) || !Check(ELangTokenKind::Colon))
		{
			return false;
		}

		const bool bBuffer = Type.Name.Equals(TEXT("buffer"), ESearchCase::CaseSensitive);
		Diagnostics.Error(
			TEXT("DSH3310"),
			FLangSpan::Join(Type.Span, NameSpan),
			FText::Format(
				bBuffer
					? LOCTEXT("BufferOutsideDsp", "'buffer {0}' declares a buffer of a Custom Pass pipeline, which only a '.dsp' file holds; move it into the pipeline's '.dsp'.")
					: LOCTEXT("PassOutsideDsp", "'pass {0}' declares a pass of a Custom Pass pipeline, which only a '.dsp' file holds; move it into the pipeline's '.dsp'."),
				FText::FromString(Name)));
		return true;
	}

	// ---------------------------------------------------------------------------------------------
	// buffer
	// ---------------------------------------------------------------------------------------------

	FDeclPtr FLangParser::ParseBufferDecl(FDocBlock&& Doc)
	{
		const int32 StartIndex = GetTokenIndex();
		Advance(); // buffer

		TUniquePtr<FBufferDecl> Buffer = MakeUnique<FBufferDecl>();
		Buffer->Doc = MoveTemp(Doc);

		if (!ExpectIdentifier(Buffer->Name, Buffer->NameSpan, TEXT("DSH3300"), LOCTEXT("ExpectedBufferName", "the buffer's name after 'buffer'")))
		{
			return nullptr;
		}
		if (!Expect(ELangTokenKind::Colon, TEXT("DSH3301"), FText::Format(LOCTEXT("ExpectedBufferColon", "':' and a format after 'buffer {0}'"), FText::FromString(Buffer->Name))))
		{
			return nullptr;
		}
		if (!ExpectIdentifier(Buffer->Format, Buffer->FormatSpan, TEXT("DSH3302"), LOCTEXT("ExpectedBufferFormat", "a buffer format such as 'R8' or 'RGBA16F'")))
		{
			return nullptr;
		}

		if (Check(ELangTokenKind::LeftParen))
		{
			const int32 ListStart = GetTokenIndex();
			Advance(); // (
			Buffer->bHasArgumentList = true;

			if (!Match(ELangTokenKind::RightParen))
			{
				for (;;)
				{
					const int32 ArgumentStart = GetTokenIndex();
					FPipelineKeyValue Argument;
					if (!ExpectIdentifier(Argument.Key, Argument.KeySpan, TEXT("DSH2308"), FText::Format(LOCTEXT("ExpectedBufferKey", "a key such as 'Scale' in the arguments of buffer '{0}'"), FText::FromString(Buffer->Name))))
					{
						return nullptr;
					}
					if (!Expect(ELangTokenKind::Assign, TEXT("DSH2309"), FText::Format(LOCTEXT("ExpectedBufferKeyAssign", "'=' after '{0}'"), FText::FromString(Argument.Key))))
					{
						return nullptr;
					}
					Argument.Value = ParseExpression();
					if (!Argument.Value)
					{
						return nullptr;
					}
					Argument.Span = SpanFrom(ArgumentStart);
					Buffer->Arguments.Add(MoveTemp(Argument));

					if (Match(ELangTokenKind::Comma))
					{
						continue;
					}
					if (Match(ELangTokenKind::RightParen))
					{
						break;
					}
					Diagnostics.Error(
						TEXT("DSH2310"),
						Current().Span,
						FText::Format(
							LOCTEXT("ExpectedBufferArgumentSeparator", "Expected ',' or ')' in the arguments of buffer '{0}', found {1}."),
							FText::FromString(Buffer->Name),
							DescribeToken(Current())));
					return nullptr;
				}
			}
			Buffer->ArgumentListSpan = SpanFrom(ListStart);
		}

		if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2311"), FText::Format(LOCTEXT("ExpectedBufferSemicolon", "';' after the declaration of buffer '{0}'"), FText::FromString(Buffer->Name))))
		{
			return nullptr;
		}

		Buffer->Span = SpanFrom(StartIndex);
		return Buffer;
	}

	// ---------------------------------------------------------------------------------------------
	// hlsl
	// ---------------------------------------------------------------------------------------------

	FDeclPtr FLangParser::ParseHlslBlockDecl(FDocBlock&& Doc)
	{
		const int32 StartIndex = GetTokenIndex();
		const FLangToken& Word = Advance(); // hlsl

		TUniquePtr<FHlslBlockDecl> Block = MakeUnique<FHlslBlockDecl>();
		Block->Doc = MoveTemp(Doc);
		Block->KeywordSpan = Word.Span;
		if (!CaptureRawBody(Block->RawBody, Block->BodySpan, ERawBodyKind::Hlsl))
		{
			return nullptr;
		}

		Block->Span = SpanFrom(StartIndex);
		return Block;
	}

	// ---------------------------------------------------------------------------------------------
	// pass
	// ---------------------------------------------------------------------------------------------

	FDeclPtr FLangParser::ParsePassDecl(FDocBlock&& Doc)
	{
		const int32 StartIndex = GetTokenIndex();
		Advance(); // pass

		TUniquePtr<FPassDecl> Pass = MakeUnique<FPassDecl>();
		Pass->Doc = MoveTemp(Doc);

		if (!ExpectIdentifier(Pass->Name, Pass->NameSpan, TEXT("DSH3303"), LOCTEXT("ExpectedPassName", "the pass's name after 'pass'")))
		{
			return nullptr;
		}
		if (!Expect(ELangTokenKind::Colon, TEXT("DSH3304"), FText::Format(LOCTEXT("ExpectedPassColon", "':' and a pass kind after 'pass {0}'"), FText::FromString(Pass->Name))))
		{
			return nullptr;
		}
		if (!ExpectIdentifier(Pass->PassKind, Pass->PassKindSpan, TEXT("DSH3305"), LOCTEXT("ExpectedPassKind", "a pass kind: fullscreen, compute, mesh, clear or copy")))
		{
			return nullptr;
		}

		const int32 BodyStart = GetTokenIndex();
		if (!Expect(ELangTokenKind::LeftBrace, TEXT("DSH2300"), FText::Format(LOCTEXT("ExpectedPassOpen", "'`{' to open the block of pass '{0}'"), FText::FromString(Pass->Name))))
		{
			return nullptr;
		}

		bool bClosed = false;
		while (!AtEnd())
		{
			if (Check(ELangTokenKind::RightBrace))
			{
				Advance();
				bClosed = true;
				break;
			}

			// A `///` line documents nothing inside a block; it is a comment, as it is in a function body.
			if (Check(ELangTokenKind::DocComment))
			{
				RecordSkippedDocComment(Advance());
				continue;
			}

			// A `buffer` or `pass` declaration on a line of its own is the next declaration, not a statement of this
			// block: the `}` is missing. Said once, and the block ends here so the declaration below is not lost.
			if (Current().bAtLineStart
				&& IsAtPipelineDeclarationWord()
				&& Peek(1).Kind == ELangTokenKind::Identifier
				&& Peek(2).Kind == ELangTokenKind::Colon)
			{
				Diagnostics.Error(
					TEXT("DSH2314"),
					Current().Span,
					FText::Format(
						LOCTEXT("PassMissingClose", "Expected '`}' to close pass '{0}' before the next declaration, found {1}."),
						FText::FromString(Pass->Name),
						DescribeToken(Current())));
				break;
			}

			const int32 BeforeIndex = GetTokenIndex();
			const bool bDirective = Check(ELangTokenKind::Directive);
			TUniquePtr<FPassStmt> Statement = ParsePassStatement();
			if (Statement)
			{
				Pass->Statements.Add(MoveTemp(Statement));
				continue;
			}

			// A `#` line is one token, which ParsePassStatement reported (DSH2312) and consumed: the next statement starts
			// right after it. Skipping to the next `;` would swallow that statement too.
			if (bDirective && GetTokenIndex() > BeforeIndex)
			{
				continue;
			}

			// Reported already. Skip to the next statement of this block; the guard keeps the loop moving when the
			// boundary is where the cursor already stands.
			SkipToStatementBoundary();
			if (GetTokenIndex() <= BeforeIndex && !Check(ELangTokenKind::RightBrace) && !AtEnd())
			{
				Advance();
			}
		}

		if (!bClosed && AtEnd())
		{
			FailAtEnd(FText::Format(LOCTEXT("WhileParsingPass", "pass '{0}'"), FText::FromString(Pass->Name)));
			return nullptr;
		}

		Pass->BodySpan = SpanFrom(BodyStart);
		Pass->Span = SpanFrom(StartIndex);
		return Pass;
	}

	TUniquePtr<FPassStmt> FLangParser::ParsePassStatement()
	{
		const int32 StartIndex = GetTokenIndex();
		const FLangToken& First = Current();

		if (First.Kind == ELangTokenKind::Directive)
		{
			Diagnostics.Error(
				TEXT("DSH2312"),
				First.Span,
				FText::Format(
					LOCTEXT("DirectiveInsidePass", "The line '#{0}' cannot appear inside a pass block; a pass holds settings, 'read', 'write' and 'param' lines and an 'hlsl' block, where HLSL's own '#' lines go."),
					FText::FromString(First.Text)));
			Advance();
			return nullptr;
		}

		if (First.Kind != ELangTokenKind::Identifier)
		{
			Diagnostics.Error(
				TEXT("DSH2301"),
				First.Span,
				FText::Format(
					LOCTEXT("ExpectedPassStatement", "Expected a setting ('Key = Value;'), a 'read', 'write' or 'param' line or an 'hlsl' block in a pass block, found {0}."),
					DescribeToken(First)));
			return nullptr;
		}

		TUniquePtr<FPassStmt> Statement = MakeUnique<FPassStmt>();
		const FString Word = First.Text;

		if (LangParserPipelinePrivate::IsHlslBlockWordText(Word))
		{
			// `hlsl { ... }`: the pass's own HLSL, verbatim. A block, so no `;` follows it; and its `#` lines are HLSL's,
			// consumed with the rest of the block, so DSH2312 is not said about them.
			Statement->StmtKind = EPassStmtKind::Hlsl;
			Statement->Name = Word;
			Statement->NameSpan = First.Span;
			Advance(); // hlsl
			if (!CaptureRawBody(Statement->RawBody, Statement->BodySpan, ERawBodyKind::Hlsl))
			{
				return nullptr;
			}
			Statement->Span = SpanFrom(StartIndex);
			return Statement;
		}

		// A binding whatever follows the word: no pass key is spelled read, write or param (LangPipelineInternal.h, the key
		// tables), so `read;` or `param = 3;` is a binding missing its name -- DSH2304 / DSH2306 below -- and never a setting.
		const bool bBindingWord = LangParserPipelinePrivate::IsPassBindingWordText(Word);

		if (bBindingWord && (Word.Equals(TEXT("read"), ESearchCase::CaseSensitive) || Word.Equals(TEXT("write"), ESearchCase::CaseSensitive)))
		{
			const bool bRead = Word.Equals(TEXT("read"), ESearchCase::CaseSensitive);
			Statement->StmtKind = bRead ? EPassStmtKind::Read : EPassStmtKind::Write;
			Advance(); // read / write

			FString FirstName;
			FLangSpan FirstSpan;
			if (!ExpectIdentifier(FirstName, FirstSpan, TEXT("DSH2304"), FText::Format(LOCTEXT("ExpectedBindingBuffer", "a buffer after '{0}'"), FText::FromString(Word))))
			{
				return nullptr;
			}

			if (Match(ELangTokenKind::Assign))
			{
				// `read Slot = Buffer`: the name inside the pass, then the buffer.
				Statement->Name = FirstName;
				Statement->NameSpan = FirstSpan;
				if (!ExpectIdentifier(Statement->Buffer, Statement->BufferSpan, TEXT("DSH2304"), FText::Format(LOCTEXT("ExpectedBindingBufferAfterSlot", "a buffer after '{0} {1} ='"), FText::FromString(Word), FText::FromString(FirstName))))
				{
					return nullptr;
				}
			}
			else
			{
				// `read Buffer`: the buffer under its own name.
				Statement->Buffer = FirstName;
				Statement->BufferSpan = FirstSpan;
			}

			if (Check(ELangTokenKind::Dot))
			{
				const int32 DotIndex = GetTokenIndex();
				Advance(); // .
				FString Member;
				FLangSpan MemberSpan;
				if (!ExpectIdentifier(Member, MemberSpan, TEXT("DSH2305"), LOCTEXT("ExpectedPrevious", "'Previous' after '.'")))
				{
					return nullptr;
				}
				if (!Member.Equals(TEXT("Previous"), ESearchCase::CaseSensitive))
				{
					Diagnostics.Error(
						TEXT("DSH2305"),
						MemberSpan,
						FText::Format(
							LOCTEXT("OnlyPrevious", "'{0}.{1}': the one thing a buffer has after '.' is 'Previous', last frame's contents of a 'History = true' buffer."),
							FText::FromString(Statement->Buffer),
							FText::FromString(Member)));
					return nullptr;
				}
				Statement->bPrevious = true;
				Statement->PreviousSpan = SpanFrom(DotIndex);
			}
		}
		else if (bBindingWord)
		{
			// `param Target = Expression`.
			Statement->StmtKind = EPassStmtKind::Param;
			Advance(); // param
			if (!ExpectIdentifier(Statement->Name, Statement->NameSpan, TEXT("DSH2306"), LOCTEXT("ExpectedParamName", "a parameter name after 'param'")))
			{
				return nullptr;
			}
			if (!Expect(ELangTokenKind::Assign, TEXT("DSH2307"), FText::Format(LOCTEXT("ExpectedParamAssign", "'=' after 'param {0}'"), FText::FromString(Statement->Name))))
			{
				return nullptr;
			}
			Statement->Value = ParseExpression();
			if (!Statement->Value)
			{
				return nullptr;
			}
		}
		else
		{
			// `Key = Value`.
			Statement->StmtKind = EPassStmtKind::Setting;
			Statement->Name = First.Text;
			Statement->NameSpan = First.Span;
			Advance(); // the key
			if (!Expect(ELangTokenKind::Assign, TEXT("DSH2302"), FText::Format(LOCTEXT("ExpectedSettingAssign", "'=' after the key '{0}'"), FText::FromString(Statement->Name))))
			{
				return nullptr;
			}
			Statement->Value = ParseExpression();
			if (!Statement->Value)
			{
				return nullptr;
			}
		}

		if (!Expect(ELangTokenKind::Semicolon, TEXT("DSH2303"), LOCTEXT("ExpectedPassStatementSemicolon", "';' at the end of the pass statement")))
		{
			return nullptr;
		}

		Statement->Span = SpanFrom(StartIndex);
		return Statement;
	}
}

#undef LOCTEXT_NAMESPACE
