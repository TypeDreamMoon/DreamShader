#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor
{
	class IDreamShaderDecompiler;
}

namespace UE::DreamShader::Editor::Private
{
	const TCHAR* GetDreamShaderCommandletUsage();

	FString NormalizeCommandletValue(FString Value);
	FString NormalizeCommandletKey(FString Key);
	bool TrySplitCommandletAssignment(const FString& Text, FString& OutKey, FString& OutValue);
	bool TryGetCommandletParam(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		const FString& Name,
		FString& OutValue);

	/**
	 * Reads every `-Define=NAME=VALUE` (short form `-D=NAME=VALUE`) off a commandlet command line and
	 * installs them as the command-line tier of the preprocessor define table. Returns how many
	 * survived validation; invalid and reserved names are dropped with a warning, not a failure.
	 *
	 * Takes the WHOLE command-line string rather than the Tokens/Switches/Params triple every other
	 * function here takes, and that is not an oversight -- see the definition. Must be called before
	 * anything compiles: the table is resolved at the top of each compile, so a define applied
	 * afterwards is a define that changed nothing.
	 */
	int32 ApplyDreamShaderCommandletDefines(const FString& CommandLine);

	bool RunDreamShaderCompileCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params);
	bool RunDreamShaderDecompileCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		UE::DreamShader::Editor::IDreamShaderDecompiler& Decompiler);
}
