// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Reading HLSL text, for the inline HLSL of a `.dsp` (DreamShader_Plan/10): what a block holds at its top level, where
// each of its functions is, which identifiers it uses.
//
// A `.dsp` may write a pass's HLSL in the file itself:
//
//     hlsl { float Weight(int i, float s) { ... }  [numthreads(8, 8, 1)] void BlurCS(uint3 Id : SV_DispatchThreadID) { ... } }
//
//     pass BlurH : compute { Entry = BlurCS; ... }              // an entry of the file's block (FIRPass::HlslSource Shared)
//     pass Tone  : fullscreen { ... hlsl { Out = ...; } }        // the statements of the entry      (Body)
//     pass Tile  : compute { ... hlsl { groupshared ...; [numthreads(16, 16, 1)] void Main(...) { ... } } }   (Block)
//
// None of it is DreamShaderLang: the parser captures the blocks verbatim, and the binder and the compiler read them with
// the scanner below -- a character scanner, not a parser, which knows comments, strings, preprocessor lines, brace depth
// and the shape of a function definition, and nothing else. What it cannot tell, the shader compiler's pre-check does.
//
// Engine-free, so the binder can use it and the Lang tests can exercise it.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Lang
{
	/** One function defined at the top level of a piece of HLSL -- outside every brace. Offsets index the scanned text. */
	struct FHlslTopLevelFunction
	{
		FString Name;
		/** Where its declaration starts: its first attribute (`[numthreads(...)]`), else the first word of its return type. */
		int32 DeclarationStart = 0;
		int32 NameOffset = 0;
		/** The `{` that opens its body. */
		int32 BodyStart = 0;
		/** Just past the `}` that closes its body. */
		int32 End = 0;
		/** `[numthreads(x, y, z)]` with three positive integer literals in front of it. */
		bool bComputeEntry = false;
		FIntVector GroupSize = FIntVector(0, 0, 0);
	};

	/** What a scan of HLSL text found at its top level. */
	struct DREAMSHADERLANG_API FHlslTextScan
	{
		/** In source order. A name defined twice (overloads) is here twice. */
		TArray<FHlslTopLevelFunction> Functions;
		/**
		 * A function definition, a `groupshared`, `struct`, `cbuffer` or `tbuffer` declaration, or a `[numthreads(...)]`
		 * attribute at the top level: the text holds declarations, and is not the statements of one function. The rule an
		 * inline pass's block is told apart by (Block or Body form).
		 */
		bool bHasDeclarations = false;
		/** The 1-based lines of the `#include` directives, at any depth. */
		TArray<int32> IncludeLines;
		/** Every brace has its partner. */
		bool bBalanced = true;

		/** The first function of that name, case-sensitively (HLSL names are); null when there is none. */
		const FHlslTopLevelFunction* FindFunction(const FString& Name) const;
	};

	/** The text with every `//` and block comment replaced by spaces, newlines kept: offsets and lines still match. */
	DREAMSHADERLANG_API FString BlankHlslComments(const FString& Text);

	/** The text with the characters of [Start, End) replaced by spaces, newlines kept. */
	DREAMSHADERLANG_API FString BlankHlslRange(const FString& Text, int32 Start, int32 End);

	/** Scans HLSL text: its top-level functions, whether it holds declarations, its includes. */
	DREAMSHADERLANG_API FHlslTextScan ScanHlslText(const FString& Text);

	/** One identifier of HLSL text, outside comments and string literals (preprocessor lines included). */
	struct FHlslIdentifier
	{
		FString Name;
		int32 Offset = 0;
		/** The next character that is not white space is `(`: a call, a function's name, a function-like macro. */
		bool bFollowedByParenthesis = false;
	};

	/** Every identifier of Text, in order. Numbers are skipped whole, so `0x1F` and `2.0f` name nothing. */
	DREAMSHADERLANG_API void FindHlslIdentifiers(const FString& Text, TArray<FHlslIdentifier>& OutIdentifiers);

	/** The 1-based line Offset is on. */
	DREAMSHADERLANG_API int32 GetHlslLineOfOffset(const FString& Text, int32 Offset);

	// ------------------------------------------------------------------------------------- inline pass slots

	/** The entry point of a slot (Shaders/Pass/DreamPassCompute.usf, DreamPassPixel.usf): DreamPassMainCS or DreamPassMainPS. */
	DREAMSHADERLANG_API const TCHAR* GetDreamPassMainEntryName(bool bCompute);

	/**
	 * The names the body form gives its statements (DreamShader_Plan/10, 3.2): Id, GroupId, LocalId and LocalIndex for a
	 * compute pass; SvPosition, Pixel and UV for a fullscreen one -- whose outputs are named after its writes besides.
	 */
	DREAMSHADERLANG_API TConstArrayView<const TCHAR*> GetDreamPassBodyFormNames(bool bCompute);
}
