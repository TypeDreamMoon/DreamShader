// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// `/// @custom` -> the Code string of a UMaterialExpressionCustom.
//
// The shape of the emitted code, and why:
//
//   struct generated_wrapper_<Hint>_<CRC>          <- only when other @custom functions are called.
//   {                                                 HLSL has no nested free functions and a Custom
//       float3 DreamShaderFn_Helper(float3 a)         node's Code IS a function body, so an embedded
//       {                                             helper has to be a struct member. This is the
//           // Begin DreamShader source: <file>       1.x SelfContained wrapper, ported.
//           // DreamShader custom: Helper line 12
//           <helper body, verbatim>
//           // End DreamShader source: <file>
//       }
//   };
//   generated_wrapper_<Hint>_<CRC> __ds_wrapper_<CRC>;
//
//   // Begin DreamShader source: <file>             <- the node's own body. No added indentation and
//   // DreamShader custom: Main line 30               no rewrite ever adds or removes a newline, so
//   <body, verbatim>                                  a shader-compile error maps back line for line.
//   // End DreamShader source: <file>
//   return 0.0;                                    <- only when the body has no top-level `return`.
//
// Three things the body text is not allowed to keep:
//
//   * leading `#include "..."` lines -- they ride on the node's IncludeFilePaths instead, because a
//     Custom node's code is pasted INSIDE a function. They are blanked in place (overwritten with
//     spaces, never deleted) so every offset, line and column after them still matches the source.
//   * `Tex.Sample(UV)` -- the sampler-less sugar (§6.5). Rewritten to `Texture2DSample(Tex,
//     TexSampler, UV)`. `TexSampler` is not invented here: the engine's translator declares
//     `Texture2D <Pin>, SamplerState <Pin>Sampler` for every texture input of a Custom node
//     (HLSLMaterialTranslator.cpp, FHLSLMaterialTranslator::CustomExpression), which is where 1.x's
//     BuildTextureSamplerArgumentName got the name from too.
//   * calls to other @custom functions -- rewritten to the wrapper member, with the companion
//     sampler argument spliced in after every texture argument.
//
// Engine facts this file encodes as text (all from HLSLMaterialTranslator::CustomExpression, 5.8):
// an additional output is declared by the engine as `inout MaterialFloatN <Name>` and is already
// zero-initialised at the call site, so the body assigns to it and declares nothing; the generated
// function's return type is MaterialFloatN / FMaterialAttributes, so a `void` @custom still has to
// return something; a Custom node input pin has no MaterialAttributes case at all; and the engine's
// own "no `return` anywhere -> wrap it in `return ...;`" fallback is a plain substring test that our
// markers and any embedded helper would defeat, which is exactly why EnsureTopLevelReturn exists.

#include "IR/IRCustomHlsl.h"

#include "IR/IRTypes.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangSource.h"
#include "Semantic/LangBound.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CString.h"
#include "Misc/Char.h"
#include "Misc/Crc.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "DreamShader.CustomHlsl"

// A named private namespace, not an anonymous one: this module builds as a unity blob, where two
// anonymous namespaces in two .cpp files become one and generic names like FPiece collide.
namespace UE::DreamShader::IR::CustomHlslPrivate
{
	using Lang::EBoundFunctionKind;
	using Lang::EParamDirection;
	using Lang::ETextureKind;
	using Lang::FBoundFunction;
	using Lang::FBoundModule;
	using Lang::FBoundParam;
	using Lang::FLangDiagnosticSink;
	using Lang::FLangSpan;

	// ----------------------------------------------------------------------- character classes

	bool IsIdentifierStart(const TCHAR Char)
	{
		return FChar::IsAlpha(Char) || Char == TCHAR('_');
	}

	bool IsIdentifierPart(const TCHAR Char)
	{
		return FChar::IsAlnum(Char) || Char == TCHAR('_');
	}

	/** Past any run of whitespace, newlines included, as the 1.x scanner did. */
	int32 SkipInlineWhitespace(const FString& Source, int32 Index)
	{
		while (Source.IsValidIndex(Index) && FChar::IsWhitespace(Source[Index]))
		{
			++Index;
		}
		return Index;
	}

	/**
	 * The newlines in [Start, End), as a string of their own.
	 *
	 * A rewrite that replaces `Tex . Sample (UV)` with `Texture2DSample(Tex, TexSampler, UV)` drops
	 * the whitespace between the tokens -- and SkipInlineWhitespace crosses newlines, so that
	 * whitespace can contain some. They are put back after the rewritten call, because the emitted
	 * code has to keep exactly one line per source line for the marker grammar in IRCustomHlsl.h to
	 * map a shader error back to the line it came from.
	 */
	FString CarriedNewlines(const FString& Source, const int32 Start, const int32 End)
	{
		FString Result;
		const int32 Last = FMath::Min(End, Source.Len());
		for (int32 Index = FMath::Max(Start, 0); Index < Last; ++Index)
		{
			if (Source[Index] == TCHAR('\n'))
			{
				Result.AppendChar(TCHAR('\n'));
			}
		}
		return Result;
	}

	/** Past whitespace AND `//` / block comments. Used where a directive or a `.` may be separated. */
	int32 SkipWhitespaceAndComments(const FString& Source, int32 Index)
	{
		for (;;)
		{
			Index = SkipInlineWhitespace(Source, Index);
			if (!Source.IsValidIndex(Index) || Source[Index] != TCHAR('/'))
			{
				return Index;
			}

			const TCHAR Next = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : TCHAR('\0');
			if (Next == TCHAR('/'))
			{
				Index += 2;
				while (Source.IsValidIndex(Index) && Source[Index] != TCHAR('\n'))
				{
					++Index;
				}
				continue;
			}
			if (Next == TCHAR('*'))
			{
				Index += 2;
				while (Source.IsValidIndex(Index)
					&& !(Source[Index] == TCHAR('*') && Source.IsValidIndex(Index + 1) && Source[Index + 1] == TCHAR('/')))
				{
					++Index;
				}
				Index = FMath::Min(Source.Len(), Index + 2);
				continue;
			}
			return Index;
		}
	}

	/** Reads `[A-Za-z_][A-Za-z0-9_]*` at InOutIndex. 2.0 has no `A::B` spellings, so 1.x's are gone. */
	bool TryReadIdentifier(const FString& Source, int32& InOutIndex, FString& OutIdentifier)
	{
		if (!Source.IsValidIndex(InOutIndex) || !IsIdentifierStart(Source[InOutIndex]))
		{
			return false;
		}

		const int32 Start = InOutIndex++;
		while (Source.IsValidIndex(InOutIndex) && IsIdentifierPart(Source[InOutIndex]))
		{
			++InOutIndex;
		}

		OutIdentifier = Source.Mid(Start, InOutIndex - Start);
		return true;
	}

	// -------------------------------------------------------------------------- the one scanner
	//
	// The 1.x codegen open-coded the same comment/string state machine four times. Here it is
	// written once: the text is split into code, comment and literal runs that tile it completely,
	// and every consumer (return detection, call collection, rewriting) works on the code runs.

	enum class EPieceKind : uint8
	{
		Code,
		Comment,
		Literal,
	};

	struct FPiece
	{
		EPieceKind Kind = EPieceKind::Code;
		int32 Start = 0;
		int32 End = 0;
	};

	void SplitHlslPieces(const FString& Source, TArray<FPiece>& OutPieces)
	{
		OutPieces.Reset();

		const int32 Length = Source.Len();
		int32 CodeStart = 0;
		int32 Index = 0;

		auto FlushCode = [&OutPieces, &CodeStart](const int32 End)
		{
			if (End > CodeStart)
			{
				OutPieces.Add(FPiece{ EPieceKind::Code, CodeStart, End });
			}
		};

		while (Index < Length)
		{
			const TCHAR Char = Source[Index];
			const TCHAR Next = (Index + 1 < Length) ? Source[Index + 1] : TCHAR('\0');

			if (Char == TCHAR('/') && Next == TCHAR('/'))
			{
				FlushCode(Index);
				const int32 Start = Index;
				Index += 2;
				while (Index < Length && Source[Index] != TCHAR('\n'))
				{
					++Index;
				}
				OutPieces.Add(FPiece{ EPieceKind::Comment, Start, Index });
				CodeStart = Index;
				continue;
			}

			if (Char == TCHAR('/') && Next == TCHAR('*'))
			{
				FlushCode(Index);
				const int32 Start = Index;
				Index += 2;
				while (Index < Length && !(Source[Index] == TCHAR('*') && Index + 1 < Length && Source[Index + 1] == TCHAR('/')))
				{
					++Index;
				}
				Index = FMath::Min(Length, Index + 2);
				OutPieces.Add(FPiece{ EPieceKind::Comment, Start, Index });
				CodeStart = Index;
				continue;
			}

			if (Char == TCHAR('"') || Char == TCHAR('\''))
			{
				const TCHAR Quote = Char;
				FlushCode(Index);
				const int32 Start = Index;
				++Index;
				while (Index < Length)
				{
					if (Source[Index] == TCHAR('\\') && Index + 1 < Length)
					{
						Index += 2;
						continue;
					}
					if (Source[Index] == Quote)
					{
						++Index;
						break;
					}
					++Index;
				}
				OutPieces.Add(FPiece{ EPieceKind::Literal, Start, Index });
				CodeStart = Index;
				continue;
			}

			++Index;
		}

		FlushCode(Length);
	}

	/**
	 * Half-open ranges of the top-level arguments of `Text`, which is what stands between a call's
	 * parentheses. Depth tracking covers `()`, `[]` and `{}`; comments and literals are skipped. A
	 * blank argument block yields no ranges, as 1.x's SplitTopLevelCallArguments did.
	 */
	void SplitTopLevelArguments(const FString& Text, TArray<TPair<int32, int32>>& OutRanges)
	{
		OutRanges.Reset();
		if (Text.TrimStartAndEnd().IsEmpty())
		{
			return;
		}

		TArray<FPiece> Pieces;
		SplitHlslPieces(Text, Pieces);

		int32 SegmentStart = 0;
		int32 ParenDepth = 0;
		int32 BraceDepth = 0;
		int32 BracketDepth = 0;

		for (const FPiece& Piece : Pieces)
		{
			if (Piece.Kind != EPieceKind::Code)
			{
				continue;
			}

			for (int32 Index = Piece.Start; Index < Piece.End; ++Index)
			{
				switch (Text[Index])
				{
				case TCHAR('('): ++ParenDepth; break;
				case TCHAR(')'): ParenDepth = FMath::Max(0, ParenDepth - 1); break;
				case TCHAR('{'): ++BraceDepth; break;
				case TCHAR('}'): BraceDepth = FMath::Max(0, BraceDepth - 1); break;
				case TCHAR('['): ++BracketDepth; break;
				case TCHAR(']'): BracketDepth = FMath::Max(0, BracketDepth - 1); break;
				case TCHAR(','):
					if (ParenDepth == 0 && BraceDepth == 0 && BracketDepth == 0)
					{
						OutRanges.Add(TPair<int32, int32>(SegmentStart, Index));
						SegmentStart = Index + 1;
					}
					break;
				default:
					break;
				}
			}
		}

		OutRanges.Add(TPair<int32, int32>(SegmentStart, Text.Len()));
	}

	/** The index of the `)` matching the `(` at OpenIndex. */
	bool TryFindMatchingParenthesis(const FString& Source, const int32 OpenIndex, int32& OutCloseIndex)
	{
		if (!Source.IsValidIndex(OpenIndex) || Source[OpenIndex] != TCHAR('('))
		{
			return false;
		}

		TArray<FPiece> Pieces;
		SplitHlslPieces(Source, Pieces);

		int32 Depth = 0;
		for (const FPiece& Piece : Pieces)
		{
			if (Piece.Kind != EPieceKind::Code || Piece.End <= OpenIndex)
			{
				continue;
			}

			for (int32 Index = FMath::Max(Piece.Start, OpenIndex); Index < Piece.End; ++Index)
			{
				if (Source[Index] == TCHAR('('))
				{
					++Depth;
				}
				else if (Source[Index] == TCHAR(')'))
				{
					--Depth;
					if (Depth == 0)
					{
						OutCloseIndex = Index;
						return true;
					}
				}
			}
		}

		return false;
	}

	// ---------------------------------------------------------------------------- small helpers

	/**
	 * A local copy of UE::DreamShader::SanitizeIdentifier: that one lives in the DreamShader runtime
	 * module, and DreamShaderLang depends on Core and nothing else (CONTRACT §0.9). Behaviour is the
	 * 1.x one, character for character, so both codegens agree on the `DreamShaderFn_*` symbol a
	 * name produces -- which the decompiler relies on to recognise generated helpers.
	 */
	FString SanitizeIdentifier(const FString& InText)
	{
		FString Result;
		Result.Reserve(InText.Len() + 1);

		for (const TCHAR Char : InText)
		{
			const bool bKeep = (Char >= TCHAR('A') && Char <= TCHAR('Z'))
				|| (Char >= TCHAR('a') && Char <= TCHAR('z'))
				|| (Char >= TCHAR('0') && Char <= TCHAR('9'))
				|| Char == TCHAR('_');
			Result.AppendChar(bKeep ? Char : TCHAR('_'));
		}

		bool bOnlyUnderscores = true;
		for (const TCHAR Char : Result)
		{
			if (Char != TCHAR('_'))
			{
				bOnlyUnderscores = false;
				break;
			}
		}
		if (Result.IsEmpty() || bOnlyUnderscores)
		{
			return TEXT("DreamShaderSymbol");
		}

		if (!((Result[0] >= TCHAR('A') && Result[0] <= TCHAR('Z'))
			|| (Result[0] >= TCHAR('a') && Result[0] <= TCHAR('z'))
			|| Result[0] == TCHAR('_')))
		{
			Result.InsertAt(0, TCHAR('_'));
		}

		// Runs of underscores collapse to one, as the 1.x helper does after prefixing.
		FString Collapsed;
		Collapsed.Reserve(Result.Len());
		for (int32 Index = 0; Index < Result.Len(); ++Index)
		{
			if (Index > 0 && Result[Index] == TCHAR('_') && Result[Index - 1] == TCHAR('_'))
			{
				continue;
			}
			Collapsed.AppendChar(Result[Index]);
		}
		return Collapsed;
	}

	/**
	 * CRLF and lone CR become LF. Line COUNT is conserved (FLangSourceText counts `\r\n` and a lone
	 * `\r` as one line each), so this never disturbs the source mapping.
	 */
	FString NormalizeLineEndings(const FString& Text)
	{
		FString Result = Text;
		Result.ReplaceInline(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("\r"), TEXT("\n"), ESearchCase::CaseSensitive);
		return Result;
	}

	const TCHAR* ScalarHlslBaseName(const EIRTypeKind Kind)
	{
		switch (Kind)
		{
		case EIRTypeKind::Bool:   return TEXT("bool");
		case EIRTypeKind::Int:    return TEXT("int");
		case EIRTypeKind::UInt:   return TEXT("uint");
		case EIRTypeKind::Half:   return TEXT("half");
		case EIRTypeKind::Double: return TEXT("double");
		default:                  return TEXT("float");
		}
	}

	const TCHAR* TextureHlslName(const ETextureKind Kind)
	{
		switch (Kind)
		{
		case ETextureKind::TextureCube:    return TEXT("TextureCube");
		case ETextureKind::Texture2DArray: return TEXT("Texture2DArray");
		case ETextureKind::Texture3D:      return TEXT("Texture3D");
		// The 1.x spelling of a 3D texture. GetGeneratedHLSLTypeName rewrote it and so do we,
		// because `VolumeTexture` is not an HLSL type name.
		case ETextureKind::VolumeTexture:  return TEXT("Texture3D");
		default:                           return TEXT("Texture2D");
		}
	}

	/** The engine's own sampling helper family: Texture2DSample, TextureCubeSampleLevel, ... */
	const TCHAR* TextureSampleHelperPrefix(const ETextureKind Kind)
	{
		switch (Kind)
		{
		case ETextureKind::TextureCube:    return TEXT("TextureCube");
		case ETextureKind::Texture2DArray: return TEXT("Texture2DArray");
		case ETextureKind::Texture3D:
		case ETextureKind::VolumeTexture:  return TEXT("Texture3D");
		default:                           return TEXT("Texture2D");
		}
	}

	/** The HLSL spelling of a bound type, for the cases where the author's spelling is unavailable. */
	FString HlslTypeFromIRType(const FIRType& Type)
	{
		switch (Type.Kind)
		{
		case EIRTypeKind::Void:
			return TEXT("void");
		case EIRTypeKind::Texture:
			return TextureHlslName(Type.Texture);
		case EIRTypeKind::SamplerState:
			return TEXT("SamplerState");
		case EIRTypeKind::Material:
			return TEXT("FMaterialAttributes");
		case EIRTypeKind::Bool:
		case EIRTypeKind::Int:
		case EIRTypeKind::UInt:
		case EIRTypeKind::Float:
		case EIRTypeKind::Half:
		case EIRTypeKind::Double:
		{
			const TCHAR* const Base = ScalarHlslBaseName(Type.Kind);
			if (Type.Cols > 1)
			{
				return FString::Printf(TEXT("%s%dx%d"), Base, Type.Rows, Type.Cols);
			}
			if (Type.Rows > 1)
			{
				return FString::Printf(TEXT("%s%d"), Base, Type.Rows);
			}
			return Base;
		}
		default:
			return TEXT("float");
		}
	}

	/**
	 * The type a `@custom` signature is emitted with. The author's spelling wins -- IRTypes.h is
	 * explicit that a @custom body gets the signature its author wrote -- with the two rewrites the
	 * shader compiler needs: `VolumeTexture` is not an HLSL type, and `material` is spelled
	 * `FMaterialAttributes` in a shader.
	 */
	FString HlslTypeName(const FString& Spelling, const FIRType& Type)
	{
		if (Type.IsMaterial())
		{
			return TEXT("FMaterialAttributes");
		}
		if (Type.IsTexture())
		{
			return TextureHlslName(Type.Texture);
		}
		if (Spelling.IsEmpty())
		{
			return HlslTypeFromIRType(Type);
		}
		return Spelling;
	}

	/** The author's spelling of a parameter type, or empty when the declaration cannot be trusted. */
	FString ParamTypeSpelling(const FBoundFunction& Function, const int32 ParamIndex)
	{
		if (Function.Decl
			&& Function.Decl->Params.Num() == Function.Params.Num()
			&& Function.Decl->Params.IsValidIndex(ParamIndex))
		{
			return Function.Decl->Params[ParamIndex].Type.Name;
		}
		return FString();
	}

	FString ReturnTypeSpelling(const FBoundFunction& Function)
	{
		return Function.Decl ? Function.Decl->ReturnType.Name : FString();
	}

	/** A scalar literal broadcasts to floatN on return, as 1.x relied on; attributes need a cast. */
	const TCHAR* ZeroLiteralFor(const FIRType& Type)
	{
		return Type.IsMaterial() ? TEXT("(FMaterialAttributes)0") : TEXT("0.0");
	}

	FString BuildWrapperTypeName(const FString& Hint)
	{
		// The hint is hashed UNSANITIZED, exactly as 1.x does, so two names that sanitize alike
		// still produce distinct wrappers.
		const FString SanitizedHint = SanitizeIdentifier(Hint.IsEmpty() ? TEXT("Generated") : Hint);
		return FString::Printf(TEXT("generated_wrapper_%s_%08X"), *SanitizedHint, FCrc::StrCrc32(*Hint));
	}

	FString BuildWrapperVariableName(const FString& Hint)
	{
		return FString::Printf(TEXT("__ds_wrapper_%08X"), FCrc::StrCrc32(*Hint));
	}

	FString BuildFunctionSymbolName(const FString& Name)
	{
		// An unprefixed `Luminance` would shadow /Engine/Private/Common.ush's intrinsic when a
		// sibling member calls it unqualified. 1.x prefixes for the same reason.
		return TEXT("DreamShaderFn_") + SanitizeIdentifier(Name);
	}

	// ---------------------------------------------------------------------------- body analysis

	struct FBodyShape
	{
		bool bHasTopLevelReturn = false;
		bool bHasBareReturn = false;
		bool bHasAnyReturn = false;
		bool bHasTopLevelSemicolon = false;
		bool bHasTopLevelBrace = false;
		bool bHasCode = false;
		/** Offset of the first bare `return`, for the diagnostic. */
		int32 BareReturnOffset = INDEX_NONE;
	};

	/**
	 * EnsureTopLevelReturn's question, plus the two the 2.0 shape needs: is there a bare `return;`
	 * (illegal once the body becomes a value-returning function), and is the body one expression
	 * with no statement of its own.
	 */
	void AnalyseBody(const FString& Body, FBodyShape& Out)
	{
		TArray<FPiece> Pieces;
		SplitHlslPieces(Body, Pieces);

		int32 BraceDepth = 0;
		for (const FPiece& Piece : Pieces)
		{
			if (Piece.Kind != EPieceKind::Code)
			{
				continue;
			}

			int32 Index = Piece.Start;
			while (Index < Piece.End)
			{
				const TCHAR Char = Body[Index];

				if (!FChar::IsWhitespace(Char))
				{
					Out.bHasCode = true;
				}

				if (Char == TCHAR('{'))
				{
					if (BraceDepth == 0)
					{
						Out.bHasTopLevelBrace = true;
					}
					++BraceDepth;
					++Index;
					continue;
				}
				if (Char == TCHAR('}'))
				{
					BraceDepth = FMath::Max(0, BraceDepth - 1);
					++Index;
					continue;
				}
				if (Char == TCHAR(';') && BraceDepth == 0)
				{
					Out.bHasTopLevelSemicolon = true;
					++Index;
					continue;
				}

				if (IsIdentifierStart(Char))
				{
					int32 IdentifierEnd = Index;
					FString Identifier;
					if (TryReadIdentifier(Body, IdentifierEnd, Identifier))
					{
						if (Identifier.Equals(TEXT("return"), ESearchCase::CaseSensitive))
						{
							Out.bHasAnyReturn = true;
							if (BraceDepth == 0)
							{
								Out.bHasTopLevelReturn = true;
							}

							const int32 AfterReturn = SkipWhitespaceAndComments(Body, IdentifierEnd);
							if (Body.IsValidIndex(AfterReturn) && Body[AfterReturn] == TCHAR(';'))
							{
								if (!Out.bHasBareReturn)
								{
									Out.BareReturnOffset = Index;
								}
								Out.bHasBareReturn = true;
							}
						}
						Index = IdentifierEnd;
						continue;
					}
				}

				++Index;
			}
		}
	}

	// ------------------------------------------------------------------------ prepared function

	/** One `@custom` function, ready to emit. */
	struct FPreparedFunction
	{
		int32 Index = INDEX_NONE;
		const FBoundFunction* Function = nullptr;
		FString Symbol;
		FString ReturnType;
		/** Parallel to Function->Params. */
		TArray<FString> ParamTypes;
		/** RawBody with the hoisted `#include`s blanked out; exactly as long as RawBody. */
		FString Body;
		TArray<FString> Includes;
		FBodyShape Shape;
		bool bBareExpression = false;
		bool bSelfContained = false;
	};

	// ------------------------------------------------------------------------------ the builder

	class FCustomCodeBuilder
	{
	public:
		FCustomCodeBuilder(const FBoundModule& InBound, FLangDiagnosticSink& InDiagnostics)
			: Bound(InBound)
			, Diagnostics(InDiagnostics)
		{
			for (int32 Index = 0; Index < Bound.Functions.Num(); ++Index)
			{
				// TMap<FString> hashes case-insensitively, so this map answers "is there a function
				// spelled anything like this"; the exact-case check happens after.
				FunctionsByName.FindOrAdd(Bound.Functions[Index].Name).Add(Index);
			}
		}

		bool Build(int32 FunctionIndex, FCustomNodeCode& Out);

	private:
		const FPreparedFunction* Prepare(int32 FunctionIndex);
		void ValidateSignature(const FPreparedFunction& Prepared, bool bIsRoot);
		void HoistLeadingIncludes(const FBoundFunction& Function, FString& InOutBody, TArray<FString>& OutIncludes);

		void CollectClosure(int32 RootIndex, TArray<int32>& OutOrder);
		bool VisitForClosure(int32 FunctionIndex, TArray<int32>& OutOrder);
		void CollectDirectCustomCalls(const FPreparedFunction& Prepared, TArray<int32>& OutCallees);

		FString RewriteText(const FPreparedFunction& Owner, const FString& Text, int32 BaseOffset, const TMap<int32, FString>& Replacements);
		bool TryRewriteSamplerSugar(
			const FPreparedFunction& Owner,
			const FString& Text,
			int32 BaseOffset,
			int32 IdentifierEnd,
			const FString& Identifier,
			const TMap<int32, FString>& Replacements,
			FString& OutRewritten,
			int32& OutNextIndex);
		bool TryRewriteCustomCall(
			const FPreparedFunction& Owner,
			const FString& Text,
			int32 BaseOffset,
			int32 IdentifierEnd,
			int32 OpenParenIndex,
			const FBoundFunction& Callee,
			const FString& Replacement,
			const TMap<int32, FString>& Replacements,
			FString& OutRewritten,
			int32& OutNextIndex);

		void AppendHelperDefinition(FString& OutCode, const FPreparedFunction& Prepared, const TMap<int32, FString>& Replacements);
		void AppendBodyBlock(FString& OutCode, const FPreparedFunction& Prepared, const TMap<int32, FString>& Replacements, const FString& TailIndent, bool bIsRoot);
		FString BuildParameterList(const FPreparedFunction& Prepared) const;

		int32 FindFunctionExact(const FString& Name) const;
		int32 FindFunctionIgnoringCase(const FString& Name) const;
		FLangSpan MakeBodySpan(const FBoundFunction& Function, int32 OffsetInBody, int32 Length) const;
		FLangSpan NameSpanOf(const FBoundFunction& Function) const;

		const FBoundModule& Bound;
		FLangDiagnosticSink& Diagnostics;
		TMap<FString, TArray<int32>> FunctionsByName;
		/** TUniquePtr so a later Prepare() cannot move the object an earlier pointer refers to. */
		TMap<int32, TUniquePtr<FPreparedFunction>> PreparedByIndex;

		/** Closure walk state: 0 unvisited, 1 visiting, 2 visited. */
		TMap<int32, uint8> VisitStates;
		TArray<int32> VisitStack;

		/** One diagnostic per distinct name, not per call site. */
		TSet<FString> ReportedNonCustomCalls;
		TSet<FString> ReportedCaseOnlyMatches;
		TSet<FString> ReportedSelfContainedCalls;

		bool bOk = true;
	};

	int32 FCustomCodeBuilder::FindFunctionExact(const FString& Name) const
	{
		if (const TArray<int32>* Candidates = FunctionsByName.Find(Name))
		{
			for (const int32 Candidate : *Candidates)
			{
				if (Bound.Functions[Candidate].Name.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Candidate;
				}
			}
		}
		return INDEX_NONE;
	}

	int32 FCustomCodeBuilder::FindFunctionIgnoringCase(const FString& Name) const
	{
		if (const TArray<int32>* Candidates = FunctionsByName.Find(Name))
		{
			if (!Candidates->IsEmpty())
			{
				return (*Candidates)[0];
			}
		}
		return INDEX_NONE;
	}

	FLangSpan FCustomCodeBuilder::NameSpanOf(const FBoundFunction& Function) const
	{
		return Function.Decl ? Function.Decl->NameSpan : FLangSpan();
	}

	FLangSpan FCustomCodeBuilder::MakeBodySpan(const FBoundFunction& Function, const int32 OffsetInBody, const int32 Length) const
	{
		FLangSpan Span;
		if (!Function.Decl)
		{
			return Span;
		}

		const Lang::FFunctionDecl& Decl = *Function.Decl;

		// BodySpan covers the braces inclusive, so RawBody[0] sits one TCHAR past its start, on the
		// same line as the opening brace and one column to its right.
		Span.Offset = Decl.BodySpan.Offset + 1 + OffsetInBody;
		Span.Length = Length;

		int32 Line = Decl.BodySpan.Line;
		int32 LastLineStart = INDEX_NONE;
		const int32 Limit = FMath::Min(OffsetInBody, Decl.RawBody.Len());
		for (int32 Index = 0; Index < Limit; ++Index)
		{
			if (Decl.RawBody[Index] == TCHAR('\n'))
			{
				++Line;
				LastLineStart = Index + 1;
			}
		}

		Span.Line = Line;
		Span.Column = (LastLineStart == INDEX_NONE)
			? Decl.BodySpan.Column + 1 + OffsetInBody
			: OffsetInBody - LastLineStart + 1;
		return Span;
	}

	// --------------------------------------------------------------------------- include hoisting

	void FCustomCodeBuilder::HoistLeadingIncludes(const FBoundFunction& Function, FString& InOutBody, TArray<FString>& OutIncludes)
	{
		int32 Index = 0;
		for (;;)
		{
			const int32 DirectiveStart = SkipWhitespaceAndComments(InOutBody, Index);
			if (!InOutBody.IsValidIndex(DirectiveStart) || InOutBody[DirectiveStart] != TCHAR('#'))
			{
				return;
			}

			int32 Cursor = SkipInlineWhitespace(InOutBody, DirectiveStart + 1);
			FString Keyword;
			if (!TryReadIdentifier(InOutBody, Cursor, Keyword)
				|| !Keyword.Equals(TEXT("include"), ESearchCase::CaseSensitive))
			{
				// Any other `#` line ends the leading run and stays in the body verbatim, which is
				// the 1.x rule: a directive that follows a statement is not hoisted.
				return;
			}

			Cursor = SkipInlineWhitespace(InOutBody, Cursor);
			if (!InOutBody.IsValidIndex(Cursor))
			{
				return;
			}

			const TCHAR Open = InOutBody[Cursor];
			TCHAR Close = TCHAR('\0');
			if (Open == TCHAR('"'))
			{
				Close = TCHAR('"');
			}
			else if (Open == TCHAR('<'))
			{
				// 1.x accepted both spellings inside a body and stored the path bare; the engine
				// re-emits every entry as `#include "..."`.
				Close = TCHAR('>');
			}
			else
			{
				return;
			}

			const int32 PathStart = Cursor + 1;
			int32 PathEnd = PathStart;
			while (InOutBody.IsValidIndex(PathEnd)
				&& InOutBody[PathEnd] != Close
				&& InOutBody[PathEnd] != TCHAR('\n'))
			{
				++PathEnd;
			}

			if (!InOutBody.IsValidIndex(PathEnd) || InOutBody[PathEnd] != Close)
			{
				// Unterminated: leave it alone and let the shader compiler say so.
				return;
			}

			const FString Path = InOutBody.Mid(PathStart, PathEnd - PathStart).TrimStartAndEnd();
			if (Path.IsEmpty())
			{
				bOk = Diagnostics.Error(
					TEXT("DSH6258"),
					MakeBodySpan(Function, DirectiveStart, PathEnd + 1 - DirectiveStart),
					FText::Format(
						LOCTEXT("CustomEmptyInclude", "'{0}' has an '#include' with an empty path; write the virtual shader path the header lives at, for example \"/Engine/Private/Common.ush\"."),
						FText::FromString(Function.Name))) && bOk;
			}
			else
			{
				OutIncludes.AddUnique(Path);
			}

			// Blanked in place rather than deleted: every offset, line and column after it has to
			// keep matching the source file (the line-conservation rule the preprocessor follows).
			// Line breaks survive the blanking for the same reason -- SkipInlineWhitespace crosses
			// newlines, so a malformed `#include` split over two lines would otherwise lose one.
			for (int32 Blank = DirectiveStart; Blank <= PathEnd; ++Blank)
			{
				if (InOutBody[Blank] != TCHAR('\n') && InOutBody[Blank] != TCHAR('\r'))
				{
					InOutBody[Blank] = TCHAR(' ');
				}
			}

			Index = PathEnd + 1;
		}
	}

	// ----------------------------------------------------------------------------------- prepare

	const FPreparedFunction* FCustomCodeBuilder::Prepare(const int32 FunctionIndex)
	{
		if (const TUniquePtr<FPreparedFunction>* Existing = PreparedByIndex.Find(FunctionIndex))
		{
			return Existing->Get();
		}
		if (!Bound.Functions.IsValidIndex(FunctionIndex))
		{
			return nullptr;
		}

		const FBoundFunction& Function = Bound.Functions[FunctionIndex];

		TUniquePtr<FPreparedFunction> Prepared = MakeUnique<FPreparedFunction>();
		Prepared->Index = FunctionIndex;
		Prepared->Function = &Function;
		Prepared->Symbol = BuildFunctionSymbolName(Function.Name);
		Prepared->ReturnType = HlslTypeName(ReturnTypeSpelling(Function), Function.ReturnType);
		Prepared->bSelfContained = Function.Directives.bSelfContained;

		Prepared->ParamTypes.Reserve(Function.Params.Num());
		for (int32 ParamIndex = 0; ParamIndex < Function.Params.Num(); ++ParamIndex)
		{
			Prepared->ParamTypes.Add(HlslTypeName(ParamTypeSpelling(Function, ParamIndex), Function.Params[ParamIndex].Type));
		}

		Prepared->Body = Function.Decl ? Function.Decl->RawBody : FString();
		HoistLeadingIncludes(Function, Prepared->Body, Prepared->Includes);
		AnalyseBody(Prepared->Body, Prepared->Shape);

		// A body that is one expression and nothing else -- no top-level `return`, no statement of
		// its own, no block. That is the 1.x Custom-node idiom, and the engine's own "wrap it in a
		// return" fallback cannot fire for us: our markers, and any embedded helper, already put
		// the word `return` into the node's code.
		Prepared->bBareExpression = !Function.ReturnType.IsVoid()
			&& Prepared->Shape.bHasCode
			&& !Prepared->Shape.bHasTopLevelReturn
			&& !Prepared->Shape.bHasTopLevelSemicolon
			&& !Prepared->Shape.bHasTopLevelBrace;

		return PreparedByIndex.Add(FunctionIndex, MoveTemp(Prepared)).Get();
	}

	// -------------------------------------------------------------------- signature validation

	void FCustomCodeBuilder::ValidateSignature(const FPreparedFunction& Prepared, const bool bIsRoot)
	{
		const FBoundFunction& Function = *Prepared.Function;

		if (!Function.Decl || !Function.Decl->bOpaqueBody)
		{
			bOk = Diagnostics.Error(
				TEXT("DSH6250"),
				NameSpanOf(Function),
				FText::Format(
					LOCTEXT("CustomNoVerbatimBody", "'{0}' has no verbatim HLSL body, so it cannot become a custom node; only a '/// @custom' function can."),
					FText::FromString(Function.Name))) && bOk;
			return;
		}

		TArray<FString> UsedNames;
		auto ClaimName = [this, &Function, &UsedNames](const FString& Name, const FLangSpan& Span)
		{
			// Case-sensitive: HLSL is, and the engine declares these names verbatim.
			for (const FString& Used : UsedNames)
			{
				if (Used.Equals(Name, ESearchCase::CaseSensitive))
				{
					bOk = Diagnostics.Error(
						TEXT("DSH6255"),
						Span,
						FText::Format(
							LOCTEXT("CustomDuplicateParameterName", "'{0}' declares '{1}' twice in the HLSL it generates; a texture parameter also claims '{1}Sampler', which the engine declares alongside it."),
							FText::FromString(Function.Name),
							FText::FromString(Name))) && bOk;
					return;
				}
			}
			UsedNames.Add(Name);
		};

		for (int32 ParamIndex = 0; ParamIndex < Function.Params.Num(); ++ParamIndex)
		{
			const FBoundParam& Param = Function.Params[ParamIndex];
			const FLangSpan Span = (Function.Decl->Params.Num() == Function.Params.Num() && Function.Decl->Params.IsValidIndex(ParamIndex))
				? Function.Decl->Params[ParamIndex].Span
				: NameSpanOf(Function);

			if (Param.Direction == EParamDirection::InOut)
			{
				// An additional output is already an `inout` parameter of the generated function;
				// an input pin of the same name would declare that name twice.
				bOk = Diagnostics.Error(
					TEXT("DSH6251"),
					Span,
					FText::Format(
						LOCTEXT("CustomInoutParameter", "'{0}' declares '{1}' as 'inout', which a custom node cannot carry; split it into an 'in' parameter and an 'out' parameter."),
						FText::FromString(Function.Name),
						FText::FromString(Param.Name))) && bOk;
			}

			if (Param.Type.Kind == EIRTypeKind::Substrate)
			{
				bOk = Diagnostics.Error(
					TEXT("DSH6253"),
					Span,
					FText::Format(
						LOCTEXT("CustomSubstrateParameter", "'{0}' uses Substrate on '{1}'; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes."),
						FText::FromString(Function.Name),
						FText::FromString(Param.Name))) && bOk;
			}

			if (Param.Direction == EParamDirection::Out
				&& (Param.Type.IsTexture() || Param.Type.Kind == EIRTypeKind::SamplerState))
			{
				bOk = Diagnostics.Error(
					TEXT("DSH6254"),
					Span,
					FText::Format(
						LOCTEXT("CustomTextureOutput", "'{0}' returns a texture through '{1}'; a custom node output carries float1..4 or material attributes, never a texture object."),
						FText::FromString(Function.Name),
						FText::FromString(Param.Name))) && bOk;
			}

			if (bIsRoot && Param.Direction != EParamDirection::Out && Param.Type.IsMaterial())
			{
				// FHLSLMaterialTranslator::CustomExpression types every input pin and has no case
				// for MaterialAttributes: it answers "Bad type MaterialAttributes for <node> input
				// <pin>". Outputs may be material attributes; inputs may not.
				bOk = Diagnostics.Error(
					TEXT("DSH6252"),
					Span,
					FText::Format(
						LOCTEXT("CustomMaterialInput", "'{0}' takes the material '{1}' as an input; a custom node cannot accept material attributes on a pin, so read the fields the body needs and pass them as floats."),
						FText::FromString(Function.Name),
						FText::FromString(Param.Name))) && bOk;
			}

			ClaimName(Param.Name, Span);
			if (Param.Type.IsTexture())
			{
				ClaimName(Param.Name + TEXT("Sampler"), Span);
			}
		}

		if (Function.ReturnType.Kind == EIRTypeKind::Substrate)
		{
			bOk = Diagnostics.Error(
				TEXT("DSH6253"),
				NameSpanOf(Function),
				FText::Format(
					LOCTEXT("CustomSubstrateReturn", "'{0}' returns Substrate; a custom node has no Substrate pins, so build that part of the material out of reflected Substrate nodes."),
					FText::FromString(Function.Name))) && bOk;
		}

		if (Function.ReturnType.IsTexture() || Function.ReturnType.Kind == EIRTypeKind::SamplerState)
		{
			bOk = Diagnostics.Error(
				TEXT("DSH6254"),
				NameSpanOf(Function),
				FText::Format(
					LOCTEXT("CustomTextureReturn", "'{0}' returns a texture object; a custom node output carries float1..4 or material attributes, never a texture object."),
					FText::FromString(Function.Name))) && bOk;
		}

		if (Function.ReturnType.IsVoid() && Prepared.Shape.bHasBareReturn)
		{
			// The generated function returns the node's first output, so a `return;` anywhere in
			// the body -- at any brace depth -- stops compiling.
			const int32 Offset = Prepared.Shape.BareReturnOffset != INDEX_NONE ? Prepared.Shape.BareReturnOffset : 0;
			bOk = Diagnostics.Error(
				TEXT("DSH6256"),
				MakeBodySpan(Function, Offset, 6),
				FText::Format(
					LOCTEXT("CustomVoidBareReturn", "'{0}' returns void but its body uses 'return;'; a custom node always returns its first output, so give the function a return type or restructure the body."),
					FText::FromString(Function.Name))) && bOk;
		}

		if (!Function.ReturnType.IsVoid()
			&& !Prepared.Shape.bHasAnyReturn
			&& Prepared.Shape.bHasCode
			&& !Prepared.bBareExpression)
		{
			Diagnostics.Warning(
				TEXT("DSH6257"),
				NameSpanOf(Function),
				FText::Format(
					LOCTEXT("CustomNeverReturns", "'{0}' declares a return type but its body never returns a value; the node's first output will be 0."),
					FText::FromString(Function.Name)));
		}
	}

	// ------------------------------------------------------------------------------ the closure

	void FCustomCodeBuilder::CollectDirectCustomCalls(const FPreparedFunction& Prepared, TArray<int32>& OutCallees)
	{
		const FBoundFunction& Function = *Prepared.Function;
		const FString& Body = Prepared.Body;

		TArray<FPiece> Pieces;
		SplitHlslPieces(Body, Pieces);

		for (const FPiece& Piece : Pieces)
		{
			if (Piece.Kind != EPieceKind::Code)
			{
				continue;
			}

			int32 Index = Piece.Start;
			while (Index < Piece.End)
			{
				if (!IsIdentifierStart(Body[Index]))
				{
					++Index;
					continue;
				}

				int32 IdentifierEnd = Index;
				FString Identifier;
				if (!TryReadIdentifier(Body, IdentifierEnd, Identifier))
				{
					++Index;
					continue;
				}

				const int32 PostIdentifier = SkipInlineWhitespace(Body, IdentifierEnd);
				const bool bIsCall = Body.IsValidIndex(PostIdentifier) && Body[PostIdentifier] == TCHAR('(');
				// `A.B(...)` is a member call, never a module function.
				const bool bIsMember = Index > 0 && Body[Index - 1] == TCHAR('.');

				if (bIsCall && !bIsMember)
				{
					const int32 Callee = FindFunctionExact(Identifier);
					if (Callee != INDEX_NONE)
					{
						if (Bound.Functions[Callee].Kind == EBoundFunctionKind::Custom)
						{
							OutCallees.AddUnique(Callee);
						}
						else if (!ReportedNonCustomCalls.Contains(Identifier))
						{
							ReportedNonCustomCalls.Add(Identifier);
							bOk = Diagnostics.Error(
								TEXT("DSH6261"),
								MakeBodySpan(Function, Index, IdentifierEnd - Index),
								FText::Format(
									LOCTEXT("CustomCallsGraphFunction", "'{0}' is not a '@custom' function and cannot be called from the HLSL body of '{1}'; a custom node sees no graph values, so mark '{0}' '@custom' as well or move the call out of the body."),
									FText::FromString(Bound.Functions[Callee].Name),
									FText::FromString(Function.Name))) && bOk;
						}
					}
					else if (!ReportedCaseOnlyMatches.Contains(Identifier))
					{
						const int32 Loose = FindFunctionIgnoringCase(Identifier);
						if (Loose != INDEX_NONE)
						{
							ReportedCaseOnlyMatches.Add(Identifier);
							Diagnostics.Warning(
								TEXT("DSH6259"),
								MakeBodySpan(Function, Index, IdentifierEnd - Index),
								FText::Format(
									LOCTEXT("CustomCaseOnlyMatch", "'{0}' differs from the function '{1}' only in case; HLSL is case-sensitive, so this call is left for the shader compiler. Did you mean '{1}'?"),
									FText::FromString(Identifier),
									FText::FromString(Bound.Functions[Loose].Name)));
						}
					}
				}

				Index = IdentifierEnd;
			}
		}
	}

	bool FCustomCodeBuilder::VisitForClosure(const int32 FunctionIndex, TArray<int32>& OutOrder)
	{
		const uint8 State = VisitStates.FindRef(FunctionIndex);
		if (State == 2)
		{
			return true;
		}

		if (State == 1)
		{
			int32 CycleStart = VisitStack.IndexOfByKey(FunctionIndex);
			if (CycleStart == INDEX_NONE)
			{
				CycleStart = 0;
			}

			TArray<FString> Names;
			for (int32 Index = CycleStart; Index < VisitStack.Num(); ++Index)
			{
				Names.Add(Bound.Functions[VisitStack[Index]].Name);
			}
			Names.Add(Bound.Functions[FunctionIndex].Name);

			bOk = Diagnostics.Error(
				TEXT("DSH6260"),
				NameSpanOf(Bound.Functions[FunctionIndex]),
				FText::Format(
					LOCTEXT("CustomCallCycle", "The '@custom' functions {0} call each other in a cycle; HLSL has no recursion, so their bodies cannot be embedded in a custom node."),
					FText::FromString(FString::Join(Names, TEXT(" -> "))))) && bOk;
			return false;
		}

		const FPreparedFunction* Prepared = Prepare(FunctionIndex);
		if (!Prepared)
		{
			return false;
		}

		VisitStates.Add(FunctionIndex, 1);
		VisitStack.Add(FunctionIndex);

		// A `selfcontained` body promises to stand on its own: nothing of the module is pulled into
		// it, so its own calls are not followed. See H-report.md for how 1.x's modifier maps over.
		if (!Prepared->bSelfContained)
		{
			TArray<int32> Callees;
			CollectDirectCustomCalls(*Prepared, Callees);
			for (const int32 Callee : Callees)
			{
				if (!VisitForClosure(Callee, OutOrder))
				{
					VisitStack.Pop();
					return false;
				}
			}
		}

		VisitStack.Pop();
		VisitStates.Add(FunctionIndex, 2);
		OutOrder.Add(FunctionIndex);
		return true;
	}

	void FCustomCodeBuilder::CollectClosure(const int32 RootIndex, TArray<int32>& OutOrder)
	{
		const FPreparedFunction* Root = Prepare(RootIndex);
		if (!Root)
		{
			return;
		}

		TArray<int32> Callees;
		CollectDirectCustomCalls(*Root, Callees);

		if (Root->bSelfContained)
		{
			// Nothing is embedded, but the body was still walked so a call that WOULD have been
			// embedded is reported rather than left silently dangling.
			for (const int32 Callee : Callees)
			{
				const FString& Name = Bound.Functions[Callee].Name;
				if (!ReportedSelfContainedCalls.Contains(Name))
				{
					ReportedSelfContainedCalls.Add(Name);
					Diagnostics.Warning(
						TEXT("DSH6264"),
						NameSpanOf(*Root->Function),
						FText::Format(
							LOCTEXT("CustomSelfContainedCall", "'{0}' is 'selfcontained', so the '@custom' function '{1}' it calls is not embedded in its node; the call is left for the shader compiler to resolve out of this body's own includes."),
							FText::FromString(Root->Function->Name),
							FText::FromString(Name)));
				}
			}
			return;
		}

		// Mark the root visiting so a call back into it is reported as the cycle it is. The root is
		// never added to OutOrder: it is the body, not an embedded helper.
		VisitStates.Add(RootIndex, 1);
		VisitStack.Add(RootIndex);

		for (const int32 Callee : Callees)
		{
			if (!VisitForClosure(Callee, OutOrder))
			{
				break;
			}
		}

		VisitStack.Pop();
		VisitStates.Add(RootIndex, 2);
	}

	// ------------------------------------------------------------------------------ the rewrite

	bool FCustomCodeBuilder::TryRewriteSamplerSugar(
		const FPreparedFunction& Owner,
		const FString& Text,
		const int32 BaseOffset,
		const int32 IdentifierEnd,
		const FString& Identifier,
		const TMap<int32, FString>& Replacements,
		FString& OutRewritten,
		int32& OutNextIndex)
	{
		// Only a texture parameter of the enclosing function is a sugar receiver; every other
		// `x.Sample(...)` is somebody else's HLSL and stays as written.
		ETextureKind TextureKind = ETextureKind::None;
		bool bIsTextureParam = false;
		for (const FBoundParam& Param : Owner.Function->Params)
		{
			if (Param.Type.IsTexture() && Param.Name.Equals(Identifier, ESearchCase::CaseSensitive))
			{
				bIsTextureParam = true;
				TextureKind = Param.Type.Texture;
				break;
			}
		}
		if (!bIsTextureParam)
		{
			return false;
		}

		const int32 DotIndex = SkipInlineWhitespace(Text, IdentifierEnd);
		if (!Text.IsValidIndex(DotIndex) || Text[DotIndex] != TCHAR('.'))
		{
			return false;
		}

		int32 MemberEnd = SkipInlineWhitespace(Text, DotIndex + 1);
		FString Member;
		if (!TryReadIdentifier(Text, MemberEnd, Member))
		{
			return false;
		}

		// The argument counts the sugar uses. The native HLSL spellings take one more (the sampler
		// comes first) and are left exactly as written -- both forms are accepted (§6.5).
		int32 SugarArgumentCount = 0;
		if (Member.Equals(TEXT("Sample"), ESearchCase::CaseSensitive))
		{
			SugarArgumentCount = 1;
		}
		else if (Member.Equals(TEXT("SampleLevel"), ESearchCase::CaseSensitive)
			|| Member.Equals(TEXT("SampleBias"), ESearchCase::CaseSensitive))
		{
			SugarArgumentCount = 2;
		}
		else if (Member.Equals(TEXT("SampleGrad"), ESearchCase::CaseSensitive))
		{
			SugarArgumentCount = 3;
		}
		else
		{
			return false;
		}

		const int32 OpenParen = SkipInlineWhitespace(Text, MemberEnd);
		if (!Text.IsValidIndex(OpenParen) || Text[OpenParen] != TCHAR('('))
		{
			return false;
		}

		int32 CloseParen = INDEX_NONE;
		if (!TryFindMatchingParenthesis(Text, OpenParen, CloseParen))
		{
			return false;
		}

		const FString ArgumentBlock = Text.Mid(OpenParen + 1, CloseParen - OpenParen - 1);
		TArray<TPair<int32, int32>> Ranges;
		SplitTopLevelArguments(ArgumentBlock, Ranges);
		if (Ranges.Num() != SugarArgumentCount)
		{
			return false;
		}

		TArray<FString> Arguments;
		Arguments.Reserve(Ranges.Num());
		for (const TPair<int32, int32>& Range : Ranges)
		{
			Arguments.Add(RewriteText(
				Owner,
				ArgumentBlock.Mid(Range.Key, Range.Value - Range.Key),
				BaseOffset + OpenParen + 1 + Range.Key,
				Replacements));
		}

		// Everything between the texture name and the `(` is the `.`, the member name and
		// whitespace; the rewrite drops all of it, so any newlines it held ride out after the call.
		OutRewritten = FString::Printf(
			TEXT("%s%s(%s, %sSampler, %s)"),
			TextureSampleHelperPrefix(TextureKind),
			*Member,
			*Identifier,
			*Identifier,
			*FString::Join(Arguments, TEXT(",")));
		OutRewritten += CarriedNewlines(Text, IdentifierEnd, OpenParen);
		OutNextIndex = CloseParen + 1;
		return true;
	}

	bool FCustomCodeBuilder::TryRewriteCustomCall(
		const FPreparedFunction& Owner,
		const FString& Text,
		const int32 BaseOffset,
		const int32 IdentifierEnd,
		const int32 OpenParenIndex,
		const FBoundFunction& Callee,
		const FString& Replacement,
		const TMap<int32, FString>& Replacements,
		FString& OutRewritten,
		int32& OutNextIndex)
	{
		bool bCalleeHasTexture = false;
		for (const FBoundParam& Param : Callee.Params)
		{
			if (Param.Type.IsTexture())
			{
				bCalleeHasTexture = true;
				break;
			}
		}

		if (!bCalleeHasTexture)
		{
			// Nothing to splice: replace the name and let the scan walk the argument list, so
			// nested calls and sugar inside it are rewritten in place. This is what 1.x did.
			OutRewritten = Replacement;
			OutNextIndex = IdentifierEnd;
			return true;
		}

		int32 CloseParen = INDEX_NONE;
		if (!TryFindMatchingParenthesis(Text, OpenParenIndex, CloseParen))
		{
			OutRewritten = Replacement;
			OutNextIndex = IdentifierEnd;
			return true;
		}

		const FString ArgumentBlock = Text.Mid(OpenParenIndex + 1, CloseParen - OpenParenIndex - 1);
		TArray<TPair<int32, int32>> Ranges;
		SplitTopLevelArguments(ArgumentBlock, Ranges);

		if (Ranges.Num() != Callee.Params.Num())
		{
			// Each sampler has to be spliced in beside its texture, so the positions have to line
			// up. Without that there is nothing sensible to emit.
			bOk = Diagnostics.Error(
				TEXT("DSH6262"),
				MakeBodySpan(*Owner.Function, BaseOffset + OpenParenIndex, CloseParen - OpenParenIndex + 1),
				FText::Format(
					LOCTEXT("CustomCallArity", "'{0}' takes {1} argument(s) but this call passes {2}; a call that carries a texture cannot be matched up by position otherwise."),
					FText::FromString(Callee.Name),
					Callee.Params.Num(),
					Ranges.Num())) && bOk;
			OutRewritten = Replacement;
			OutNextIndex = IdentifierEnd;
			return true;
		}

		TArray<FString> Arguments;
		Arguments.Reserve(Ranges.Num() * 2);
		for (int32 ArgumentIndex = 0; ArgumentIndex < Ranges.Num(); ++ArgumentIndex)
		{
			const TPair<int32, int32>& Range = Ranges[ArgumentIndex];
			const FString RawArgument = ArgumentBlock.Mid(Range.Key, Range.Value - Range.Key);
			Arguments.Add(RewriteText(Owner, RawArgument, BaseOffset + OpenParenIndex + 1 + Range.Key, Replacements));

			if (!Callee.Params[ArgumentIndex].Type.IsTexture())
			{
				continue;
			}

			const FString Trimmed = RawArgument.TrimStartAndEnd();
			int32 Cursor = 0;
			FString TextureName;
			const bool bPlainName = TryReadIdentifier(Trimmed, Cursor, TextureName) && Cursor == Trimmed.Len();
			if (!bPlainName)
			{
				// 1.x built the sampler name by pasting "Sampler" onto whatever text the argument
				// was, which turns `a ? b : c` into `a ? b : cSampler`. Refuse instead.
				bOk = Diagnostics.Error(
					TEXT("DSH6263"),
					MakeBodySpan(*Owner.Function, BaseOffset + OpenParenIndex + 1 + Range.Key, Range.Value - Range.Key),
					FText::Format(
						LOCTEXT("CustomTextureArgumentNotAName", "The texture argument for '{0}' of '{1}' has to be a plain texture name, because the sampler that goes with it is named after it."),
						FText::FromString(Callee.Params[ArgumentIndex].Name),
						FText::FromString(Callee.Name))) && bOk;
				continue;
			}

			Arguments.Add(FString(TEXT(" ")) + TextureName + TEXT("Sampler"));
		}

		// The whitespace between the callee's name and its `(` is dropped by the rewrite; its
		// newlines follow the call so the body keeps one emitted line per source line.
		OutRewritten = Replacement + TEXT("(") + FString::Join(Arguments, TEXT(",")) + TEXT(")")
			+ CarriedNewlines(Text, IdentifierEnd, OpenParenIndex);
		OutNextIndex = CloseParen + 1;
		return true;
	}

	/**
	 * Rewrites one run of body text. BaseOffset is where Text starts inside the owner's body, so a
	 * diagnostic still points at the source. No branch here ever adds or removes a newline: the
	 * emitted code has exactly as many lines as the body it came from.
	 *
	 * The cursor walks the whole string rather than each piece in turn, because a rewritten call can
	 * swallow text that reaches past the piece it started in -- a comment between a sampler call's
	 * parentheses is the easy example.
	 */
	FString FCustomCodeBuilder::RewriteText(
		const FPreparedFunction& Owner,
		const FString& Text,
		const int32 BaseOffset,
		const TMap<int32, FString>& Replacements)
	{
		FString Result;
		Result.Reserve(Text.Len() + 64);

		TArray<FPiece> Pieces;
		SplitHlslPieces(Text, Pieces);

		int32 PieceIndex = 0;
		int32 Index = 0;
		while (Index < Text.Len())
		{
			while (PieceIndex < Pieces.Num() && Pieces[PieceIndex].End <= Index)
			{
				++PieceIndex;
			}
			if (PieceIndex >= Pieces.Num())
			{
				Result += Text.Mid(Index);
				break;
			}

			const FPiece& Piece = Pieces[PieceIndex];
			if (Index < Piece.Start)
			{
				Result += Text.Mid(Index, Piece.Start - Index);
				Index = Piece.Start;
				continue;
			}
			if (Piece.Kind != EPieceKind::Code)
			{
				Result += Text.Mid(Index, Piece.End - Index);
				Index = Piece.End;
				continue;
			}

			const TCHAR Char = Text[Index];
			if (!IsIdentifierStart(Char))
			{
				Result.AppendChar(Char);
				++Index;
				continue;
			}

			int32 IdentifierEnd = Index;
			FString Identifier;
			if (!TryReadIdentifier(Text, IdentifierEnd, Identifier))
			{
				Result.AppendChar(Char);
				++Index;
				continue;
			}

			// `A.B(...)` is a member access, never one of ours.
			const bool bIsMember = Index > 0 && Text[Index - 1] == TCHAR('.');

			FString Rewritten;
			int32 NextIndex = INDEX_NONE;
			if (!bIsMember
				&& TryRewriteSamplerSugar(Owner, Text, BaseOffset, IdentifierEnd, Identifier, Replacements, Rewritten, NextIndex))
			{
				Result += Rewritten;
				Index = NextIndex;
				continue;
			}

			const int32 PostIdentifier = SkipInlineWhitespace(Text, IdentifierEnd);
			const bool bIsCall = Text.IsValidIndex(PostIdentifier) && Text[PostIdentifier] == TCHAR('(');
			if (bIsCall && !bIsMember)
			{
				const int32 Callee = FindFunctionExact(Identifier);
				if (Callee != INDEX_NONE)
				{
					if (const FString* Replacement = Replacements.Find(Callee))
					{
						if (TryRewriteCustomCall(Owner, Text, BaseOffset, IdentifierEnd, PostIdentifier,
							Bound.Functions[Callee], *Replacement, Replacements, Rewritten, NextIndex))
						{
							Result += Rewritten;
							Index = NextIndex;
							continue;
						}
					}
				}
			}

			Result += Text.Mid(Index, IdentifierEnd - Index);
			Index = IdentifierEnd;
		}

		return Result;
	}

	// ----------------------------------------------------------------------------- the emission

	FString FCustomCodeBuilder::BuildParameterList(const FPreparedFunction& Prepared) const
	{
		TArray<FString> Parameters;
		const FBoundFunction& Function = *Prepared.Function;

		// Declaration order, not "inputs then outputs": a 2.0 signature may interleave them, and the
		// call sites we rewrite are in the author's order. (1.x reordered because its section
		// grammar kept Inputs and Results in separate lists to begin with.)
		for (int32 ParamIndex = 0; ParamIndex < Function.Params.Num(); ++ParamIndex)
		{
			const FBoundParam& Param = Function.Params[ParamIndex];
			const FString& TypeName = Prepared.ParamTypes[ParamIndex];

			if (Param.Direction == EParamDirection::Out)
			{
				Parameters.Add(FString::Printf(TEXT("out %s %s"), *TypeName, *Param.Name));
				continue;
			}

			Parameters.Add(FString::Printf(TEXT("%s %s"), *TypeName, *Param.Name));
			if (Param.Type.IsTexture())
			{
				Parameters.Add(FString::Printf(TEXT("SamplerState %sSampler"), *Param.Name));
			}
		}

		return FString::Join(Parameters, TEXT(", "));
	}

	void FCustomCodeBuilder::AppendBodyBlock(
		FString& OutCode,
		const FPreparedFunction& Prepared,
		const TMap<int32, FString>& Replacements,
		const FString& TailIndent,
		const bool bIsRoot)
	{
		const FBoundFunction& Function = *Prepared.Function;
		const FString MarkerLine = FString::Printf(
			TEXT("%s%s%s%d"),
			CustomCodeMarker::BodyPrefix,
			*Function.Name,
			CustomCodeMarker::BodyLineInfix,
			Function.Decl ? Function.Decl->BodySpan.Line : 1);

		if (Prepared.bBareExpression)
		{
			// `return` and `;` go on their own lines around the block so the expression keeps its
			// own line AND column numbers. Comments are whitespace to the compiler, so the markers
			// sitting inside the return expression are harmless.
			OutCode += TailIndent + TEXT("return\n");
		}

		OutCode += FString(CustomCodeMarker::BeginPrefix) + Function.File + TEXT("\n");
		OutCode += MarkerLine + TEXT("\n");

		const FString Rewritten = NormalizeLineEndings(RewriteText(Prepared, Prepared.Body, 0, Replacements));
		OutCode += Rewritten;
		if (!Rewritten.EndsWith(TEXT("\n"), ESearchCase::CaseSensitive))
		{
			OutCode += TEXT("\n");
		}

		OutCode += FString(CustomCodeMarker::EndPrefix) + Function.File + TEXT("\n");

		if (Prepared.bBareExpression)
		{
			OutCode += TailIndent + TEXT(";\n");
			return;
		}

		// EnsureTopLevelReturn: a body with no `return` of its own gets one, so the engine's own
		// naive "does the code contain the word return" fallback is never relied on. The node's own
		// body always needs one -- its first output IS the function's return value, and a `void`
		// @custom still has to hand the graph a number.
		const bool bNeedsFallback = bIsRoot
			? (!Prepared.Shape.bHasTopLevelReturn || Function.ReturnType.IsVoid())
			: (!Function.ReturnType.IsVoid() && !Prepared.Shape.bHasTopLevelReturn);

		if (bNeedsFallback)
		{
			const TCHAR* const Zero = Function.ReturnType.IsVoid() ? TEXT("0.0") : ZeroLiteralFor(Function.ReturnType);
			OutCode += TailIndent + TEXT("return ") + Zero + TEXT(";\n");
		}
	}

	void FCustomCodeBuilder::AppendHelperDefinition(
		FString& OutCode,
		const FPreparedFunction& Prepared,
		const TMap<int32, FString>& Replacements)
	{
		const FBoundFunction& Function = *Prepared.Function;
		const FString Indent = TEXT("\t");

		OutCode += FString::Printf(
			TEXT("%s%s %s(%s)\n%s{\n"),
			*Indent,
			*Prepared.ReturnType,
			*Prepared.Symbol,
			*BuildParameterList(Prepared),
			*Indent);

		// A real HLSL `out` parameter arrives uninitialised -- unlike a Custom node's additional
		// outputs, which the engine zero-initialises at the call site -- so 1.x's zeroing stays.
		for (int32 ParamIndex = 0; ParamIndex < Function.Params.Num(); ++ParamIndex)
		{
			if (Function.Params[ParamIndex].Direction != EParamDirection::Out)
			{
				continue;
			}
			OutCode += FString::Printf(
				TEXT("%s\t%s = (%s)0;\n"),
				*Indent,
				*Function.Params[ParamIndex].Name,
				*Prepared.ParamTypes[ParamIndex]);
		}

		AppendBodyBlock(OutCode, Prepared, Replacements, Indent + TEXT("\t"), /*bIsRoot*/ false);

		OutCode += Indent + TEXT("}\n");
	}

	// -------------------------------------------------------------------------------- the build

	bool FCustomCodeBuilder::Build(const int32 FunctionIndex, FCustomNodeCode& Out)
	{
		Out.Code.Reset();
		Out.IncludeFilePaths.Reset();
		Out.InlinedFunctions.Reset();

		if (!Bound.Functions.IsValidIndex(FunctionIndex))
		{
			return Diagnostics.Error(
				TEXT("DSH6250"),
				FLangSpan(),
				FText::Format(
					LOCTEXT("CustomBadFunctionIndex", "Custom node code was asked for function {0}, but the bound module has {1}."),
					FunctionIndex,
					Bound.Functions.Num()));
		}

		const FBoundFunction& Function = Bound.Functions[FunctionIndex];
		if (Function.Kind != EBoundFunctionKind::Custom)
		{
			return Diagnostics.Error(
				TEXT("DSH6250"),
				NameSpanOf(Function),
				FText::Format(
					LOCTEXT("CustomNotACustomFunction", "'{0}' is not marked '/// @custom', so it has no custom node to build."),
					FText::FromString(Function.Name)));
		}

		const FPreparedFunction* Root = Prepare(FunctionIndex);
		if (!Root)
		{
			return Diagnostics.Error(
				TEXT("DSH6250"),
				NameSpanOf(Function),
				FText::Format(
					LOCTEXT("CustomPrepareFailed", "'{0}' could not be prepared for a custom node."),
					FText::FromString(Function.Name)));
		}

		TArray<int32> Order;
		CollectClosure(FunctionIndex, Order);

		ValidateSignature(*Root, /*bIsRoot*/ true);
		for (const int32 HelperIndex : Order)
		{
			if (const TUniquePtr<FPreparedFunction>* Helper = PreparedByIndex.Find(HelperIndex))
			{
				ValidateSignature(**Helper, /*bIsRoot*/ false);
			}
		}

		// Sibling calls inside the wrapper are plain `DreamShaderFn_*` with no `this.`; the node's
		// own body reaches them through the wrapper instance. (1.x, verbatim.)
		TMap<int32, FString> MemberReplacements;
		TMap<int32, FString> RootReplacements;
		FString WrapperType;
		FString WrapperVariable;
		if (!Order.IsEmpty())
		{
			WrapperType = BuildWrapperTypeName(Function.Name);
			WrapperVariable = BuildWrapperVariableName(Function.Name);
			for (const int32 HelperIndex : Order)
			{
				const TUniquePtr<FPreparedFunction>* Helper = PreparedByIndex.Find(HelperIndex);
				if (!Helper)
				{
					continue;
				}
				MemberReplacements.Add(HelperIndex, (*Helper)->Symbol);
				RootReplacements.Add(HelperIndex, WrapperVariable + TEXT(".") + (*Helper)->Symbol);
			}
		}

		FString Code;
		if (!Order.IsEmpty())
		{
			Code += FString::Printf(TEXT("struct %s\n{\n"), *WrapperType);
			for (const int32 HelperIndex : Order)
			{
				const TUniquePtr<FPreparedFunction>* Helper = PreparedByIndex.Find(HelperIndex);
				if (!Helper)
				{
					continue;
				}
				AppendHelperDefinition(Code, **Helper, MemberReplacements);
				Code += TEXT("\n");
			}
			Code += FString::Printf(TEXT("};\n%s %s;\n\n"), *WrapperType, *WrapperVariable);
		}

		AppendBodyBlock(Code, *Root, RootReplacements, FString(), /*bIsRoot*/ true);

		Out.Code = MoveTemp(Code);
		Out.InlinedFunctions = Order;

		for (const int32 HelperIndex : Order)
		{
			if (const TUniquePtr<FPreparedFunction>* Helper = PreparedByIndex.Find(HelperIndex))
			{
				for (const FString& Path : (*Helper)->Includes)
				{
					Out.IncludeFilePaths.AddUnique(Path);
				}
			}
		}
		for (const FString& Path : Root->Includes)
		{
			Out.IncludeFilePaths.AddUnique(Path);
		}

		return bOk;
	}
}

namespace UE::DreamShader::IR
{
	bool TryParseCustomCodeBodyMarker(const FString& Line, FString& OutFunctionName, int32& OutSourceLine)
	{
		const FString Prefix = CustomCodeMarker::BodyPrefix;
		if (!Line.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return false;
		}

		const FString Infix = CustomCodeMarker::BodyLineInfix;
		const int32 InfixIndex = Line.Find(Infix, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (InfixIndex == INDEX_NONE || InfixIndex < Prefix.Len())
		{
			return false;
		}

		const FString Number = Line.Mid(InfixIndex + Infix.Len()).TrimStartAndEnd();
		if (Number.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Char : Number)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}

		OutFunctionName = Line.Mid(Prefix.Len(), InfixIndex - Prefix.Len()).TrimStartAndEnd();
		OutSourceLine = FCString::Atoi(*Number);
		return true;
	}

	bool BuildDreamShaderCustomNodeCode(
		const Lang::FBoundModule& Bound,
		const int32 FunctionIndex,
		FCustomNodeCode& Out,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		CustomHlslPrivate::FCustomCodeBuilder Builder(Bound, Diagnostics);
		return Builder.Build(FunctionIndex, Out);
	}
}

#undef LOCTEXT_NAMESPACE
