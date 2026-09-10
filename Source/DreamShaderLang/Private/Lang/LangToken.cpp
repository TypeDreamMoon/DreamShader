// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Token names, keyword spellings and operator spellings.
//
// Two conventions this file fixes for everyone downstream:
//
//   LexToString(ELangTokenKind)  -> the ENUMERATOR name ("LeftParen"), for logs and test failures.
//   GetLangTokenSpelling(kind)   -> the SOURCE spelling ("("), for messages and the printer.
//   LexToString(ELangKeyword)    -> the SOURCE spelling ("uniform"), because a keyword's identity
//                                   IS its spelling and the printer must be able to write it back.
//
// The keyword table below is the single source of truth for both LexToString(ELangKeyword) and
// TryGetLangKeyword, so the two can never disagree about a spelling.
//
// Comparisons are made with FCString::Strcmp, never FString::operator== and never a
// TMap<FString, ...>: both of those are CASE-INSENSITIVE in Unreal, which would happily accept
// `IMPORT` or `Struct` as keywords and silently break the "type names are identifiers" rule that
// the whole grammar rests on.

#include "Lang/LangToken.h"

// FCString. CoreMinimal reaches it through Containers/UnrealString.h, but this TU calls Strcmp
// directly and must compile on its own outside the unity blob.
#include "Misc/CString.h"

namespace UE::DreamShader::Lang
{
	namespace Private
	{
		struct FLangKeywordEntry
		{
			const TCHAR* Spelling;
			ELangKeyword Keyword;
		};

		/** Exactly the members of ELangKeyword, in declaration order. Case-sensitive, HLSL spelling. */
		static const FLangKeywordEntry GLangKeywordTable[] =
		{
			{ TEXT("uniform"),  ELangKeyword::Uniform },
			{ TEXT("static"),   ELangKeyword::Static },
			{ TEXT("const"),    ELangKeyword::Const },
			{ TEXT("extern"),   ELangKeyword::Extern },
			{ TEXT("export"),   ELangKeyword::Export },
			{ TEXT("in"),       ELangKeyword::In },
			{ TEXT("out"),      ELangKeyword::Out },
			{ TEXT("inout"),    ELangKeyword::InOut },
			{ TEXT("struct"),   ELangKeyword::Struct },
			{ TEXT("if"),       ELangKeyword::If },
			{ TEXT("else"),     ELangKeyword::Else },
			{ TEXT("for"),      ELangKeyword::For },
			{ TEXT("while"),    ELangKeyword::While },
			{ TEXT("do"),       ELangKeyword::Do },
			{ TEXT("return"),   ELangKeyword::Return },
			{ TEXT("break"),    ELangKeyword::Break },
			{ TEXT("continue"), ELangKeyword::Continue },
			{ TEXT("discard"),  ELangKeyword::Discard },
			{ TEXT("true"),     ELangKeyword::True },
			{ TEXT("false"),    ELangKeyword::False },
			{ TEXT("import"),   ELangKeyword::Import },
		};
	}

	const TCHAR* LexToString(const ELangTokenKind Kind)
	{
		switch (Kind)
		{
		case ELangTokenKind::EndOfFile:            return TEXT("EndOfFile");
		case ELangTokenKind::Identifier:           return TEXT("Identifier");
		case ELangTokenKind::Keyword:              return TEXT("Keyword");
		case ELangTokenKind::IntLiteral:           return TEXT("IntLiteral");
		case ELangTokenKind::FloatLiteral:         return TEXT("FloatLiteral");
		case ELangTokenKind::StringLiteral:        return TEXT("StringLiteral");
		case ELangTokenKind::DocComment:           return TEXT("DocComment");
		case ELangTokenKind::Directive:            return TEXT("Directive");

		case ELangTokenKind::LeftParen:            return TEXT("LeftParen");
		case ELangTokenKind::RightParen:           return TEXT("RightParen");
		case ELangTokenKind::LeftBrace:            return TEXT("LeftBrace");
		case ELangTokenKind::RightBrace:           return TEXT("RightBrace");
		case ELangTokenKind::LeftBracket:          return TEXT("LeftBracket");
		case ELangTokenKind::RightBracket:         return TEXT("RightBracket");
		case ELangTokenKind::Comma:                return TEXT("Comma");
		case ELangTokenKind::Semicolon:            return TEXT("Semicolon");
		case ELangTokenKind::Colon:                return TEXT("Colon");
		case ELangTokenKind::Dot:                  return TEXT("Dot");
		case ELangTokenKind::Question:             return TEXT("Question");

		case ELangTokenKind::Plus:                 return TEXT("Plus");
		case ELangTokenKind::Minus:                return TEXT("Minus");
		case ELangTokenKind::Star:                 return TEXT("Star");
		case ELangTokenKind::Slash:                return TEXT("Slash");
		case ELangTokenKind::Percent:              return TEXT("Percent");
		case ELangTokenKind::PlusPlus:             return TEXT("PlusPlus");
		case ELangTokenKind::MinusMinus:           return TEXT("MinusMinus");
		case ELangTokenKind::Ampersand:            return TEXT("Ampersand");
		case ELangTokenKind::Pipe:                 return TEXT("Pipe");
		case ELangTokenKind::Caret:                return TEXT("Caret");
		case ELangTokenKind::Tilde:                return TEXT("Tilde");
		case ELangTokenKind::Bang:                 return TEXT("Bang");
		case ELangTokenKind::AmpersandAmpersand:   return TEXT("AmpersandAmpersand");
		case ELangTokenKind::PipePipe:             return TEXT("PipePipe");
		case ELangTokenKind::Less:                 return TEXT("Less");
		case ELangTokenKind::Greater:              return TEXT("Greater");
		case ELangTokenKind::LessEqual:            return TEXT("LessEqual");
		case ELangTokenKind::GreaterEqual:         return TEXT("GreaterEqual");
		case ELangTokenKind::EqualEqual:           return TEXT("EqualEqual");
		case ELangTokenKind::BangEqual:            return TEXT("BangEqual");
		case ELangTokenKind::LessLess:             return TEXT("LessLess");
		case ELangTokenKind::GreaterGreater:       return TEXT("GreaterGreater");
		case ELangTokenKind::Assign:               return TEXT("Assign");
		case ELangTokenKind::PlusAssign:           return TEXT("PlusAssign");
		case ELangTokenKind::MinusAssign:          return TEXT("MinusAssign");
		case ELangTokenKind::StarAssign:           return TEXT("StarAssign");
		case ELangTokenKind::SlashAssign:          return TEXT("SlashAssign");
		case ELangTokenKind::PercentAssign:        return TEXT("PercentAssign");
		case ELangTokenKind::AmpersandAssign:      return TEXT("AmpersandAssign");
		case ELangTokenKind::PipeAssign:           return TEXT("PipeAssign");
		case ELangTokenKind::CaretAssign:          return TEXT("CaretAssign");
		case ELangTokenKind::LessLessAssign:       return TEXT("LessLessAssign");
		case ELangTokenKind::GreaterGreaterAssign: return TEXT("GreaterGreaterAssign");

		case ELangTokenKind::Unknown:
		default:                                   return TEXT("Unknown");
		}
	}

	const TCHAR* LexToString(const ELangKeyword Keyword)
	{
		for (const Private::FLangKeywordEntry& Entry : Private::GLangKeywordTable)
		{
			if (Entry.Keyword == Keyword)
			{
				return Entry.Spelling;
			}
		}

		// ELangKeyword::None, and anything added to the enum without a table row (which
		// TryGetLangKeyword would then never produce either).
		return TEXT("None");
	}

	bool TryGetLangKeyword(const FString& Text, ELangKeyword& OutKeyword)
	{
		if (Text.IsEmpty())
		{
			OutKeyword = ELangKeyword::None;
			return false;
		}

		const TCHAR* const Characters = *Text;
		for (const Private::FLangKeywordEntry& Entry : Private::GLangKeywordTable)
		{
			// Cheap reject on the first character before the full compare; every keyword is
			// lower case ASCII, so an identifier starting with an upper-case letter -- `Import`,
			// `Material`, `Struct` -- never reaches the Strcmp at all.
			if (Entry.Spelling[0] == Characters[0] && FCString::Strcmp(Characters, Entry.Spelling) == 0)
			{
				OutKeyword = Entry.Keyword;
				return true;
			}
		}

		OutKeyword = ELangKeyword::None;
		return false;
	}

	const TCHAR* GetLangTokenSpelling(const ELangTokenKind Kind)
	{
		switch (Kind)
		{
		case ELangTokenKind::LeftParen:            return TEXT("(");
		case ELangTokenKind::RightParen:           return TEXT(")");
		case ELangTokenKind::LeftBrace:            return TEXT("{");
		case ELangTokenKind::RightBrace:           return TEXT("}");
		case ELangTokenKind::LeftBracket:          return TEXT("[");
		case ELangTokenKind::RightBracket:         return TEXT("]");
		case ELangTokenKind::Comma:                return TEXT(",");
		case ELangTokenKind::Semicolon:            return TEXT(";");
		case ELangTokenKind::Colon:                return TEXT(":");
		case ELangTokenKind::Dot:                  return TEXT(".");
		case ELangTokenKind::Question:             return TEXT("?");

		case ELangTokenKind::Plus:                 return TEXT("+");
		case ELangTokenKind::Minus:                return TEXT("-");
		case ELangTokenKind::Star:                 return TEXT("*");
		case ELangTokenKind::Slash:                return TEXT("/");
		case ELangTokenKind::Percent:              return TEXT("%");
		case ELangTokenKind::PlusPlus:             return TEXT("++");
		case ELangTokenKind::MinusMinus:           return TEXT("--");
		case ELangTokenKind::Ampersand:            return TEXT("&");
		case ELangTokenKind::Pipe:                 return TEXT("|");
		case ELangTokenKind::Caret:                return TEXT("^");
		case ELangTokenKind::Tilde:                return TEXT("~");
		case ELangTokenKind::Bang:                 return TEXT("!");
		case ELangTokenKind::AmpersandAmpersand:   return TEXT("&&");
		case ELangTokenKind::PipePipe:             return TEXT("||");
		case ELangTokenKind::Less:                 return TEXT("<");
		case ELangTokenKind::Greater:              return TEXT(">");
		case ELangTokenKind::LessEqual:            return TEXT("<=");
		case ELangTokenKind::GreaterEqual:         return TEXT(">=");
		case ELangTokenKind::EqualEqual:           return TEXT("==");
		case ELangTokenKind::BangEqual:            return TEXT("!=");
		case ELangTokenKind::LessLess:             return TEXT("<<");
		case ELangTokenKind::GreaterGreater:       return TEXT(">>");
		case ELangTokenKind::Assign:               return TEXT("=");
		case ELangTokenKind::PlusAssign:           return TEXT("+=");
		case ELangTokenKind::MinusAssign:          return TEXT("-=");
		case ELangTokenKind::StarAssign:           return TEXT("*=");
		case ELangTokenKind::SlashAssign:          return TEXT("/=");
		case ELangTokenKind::PercentAssign:        return TEXT("%=");
		case ELangTokenKind::AmpersandAssign:      return TEXT("&=");
		case ELangTokenKind::PipeAssign:           return TEXT("|=");
		case ELangTokenKind::CaretAssign:          return TEXT("^=");
		case ELangTokenKind::LessLessAssign:       return TEXT("<<=");
		case ELangTokenKind::GreaterGreaterAssign: return TEXT(">>=");

		default:
			// EndOfFile, Identifier, Keyword, the literals, DocComment, Directive and Unknown have
			// no fixed spelling: what they look like is in FLangToken::Text.
			return TEXT("");
		}
	}
}
