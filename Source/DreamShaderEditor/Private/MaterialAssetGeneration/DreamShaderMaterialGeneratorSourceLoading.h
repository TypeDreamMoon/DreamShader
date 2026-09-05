// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Source loading for the DreamShader material generator: read a .dsm/.dsf, run the conditional
// compilation preprocessor over it, and recursively inline its imports into a single prepared source
// string (with "// Begin/End DreamShader source:" markers used by the diagnostics line mapping),
// detecting import cycles and enforcing header/function-file rules.

#pragma once

#include "CoreMinimal.h"
#include "DreamShaderDefineTable.h"

// FDreamShaderError, the out-parameter type of both overloads below. This header used to get it for
// free from whichever neighbour landed earlier in the unity blob; naming it here is what makes the
// declaration mean the same thing in a non-unity compile.
#include "DreamShaderDiagnostic.h"

namespace UE::DreamShader::Editor
{
	/**
	 * Loads one source and everything it imports, preprocessed and inlined, ready for the parser.
	 *
	 * @param OutTouchedDefines         Union, over every file inlined, of the defines the preprocessor
	 *                                  actually read -- with GDreamShaderUndefinedDefineSentinel for
	 *                                  the ones that were read while undefined. This is build key
	 *                                  material: BuildSourceHash folds it in, so an asset rebuilds
	 *                                  when a define it depends on changes and stays put when one it
	 *                                  never looked at changes.
	 * @param bOutAnySourceHadDirectives True when ANY file in the inlined set contained a preprocessor
	 *                                  directive, taken or not. The Adopt action refuses on this: the
	 *                                  asset only holds the post-cut result, so writing it back over
	 *                                  the source would delete the conditionals (DSH8149).
	 */
	bool LoadPreparedDreamShaderSource(
		const FString& SourceFilePath,
		FString& OutSourceText,
		UE::DreamShader::FDreamShaderDefineValueMap& OutTouchedDefines,
		bool& bOutAnySourceHadDirectives,
		FDreamShaderError& OutError);

	/**
	 * The same load, for callers that want only the text.
	 *
	 * Kept as its own overload rather than as defaulted parameters so that a caller which does not
	 * need the define set does not accidentally acquire an empty one and hand it to something that
	 * treats "no defines" as an answer -- BuildSourceHash above all, where an empty map is not a
	 * missing argument but a positive claim that this source reads no defines.
	 */
	bool LoadPreparedDreamShaderSource(const FString& SourceFilePath, FString& OutSourceText, FDreamShaderError& OutError);
}
