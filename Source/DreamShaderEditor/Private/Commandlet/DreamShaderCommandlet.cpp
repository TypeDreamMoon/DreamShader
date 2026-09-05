#include "DreamShaderCommandlet.h"

#include "Commandlet/DreamShaderCommandletRunner.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "DreamShaderModule.h"

UDreamShaderCommandlet::UDreamShaderCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UDreamShaderCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamValues;
	ParseCommandLine(*Params, Tokens, Switches, ParamValues);

	// Ahead of the dispatch below, because every branch of it can compile and the define table is
	// resolved once at the top of each compile. A define installed afterwards is a define that
	// changed nothing -- and it would change nothing silently, since the run still succeeds.
	//
	// Handed the whole command-line string rather than the triple parsed just above: that four-
	// argument parse folds `-Define=A=1 -Define=B=2` into one TMap entry and deletes both switches
	// from the array. ApplyDreamShaderCommandletDefines re-tokenizes to get all of them back; its
	// definition carries the detail.
	UE::DreamShader::Editor::Private::ApplyDreamShaderCommandletDefines(Params);

	FString Command;
	if (!Tokens.IsEmpty())
	{
		Command = Tokens[0];
	}
	else if (!UE::DreamShader::Editor::Private::TryGetCommandletParam(Tokens, Switches, ParamValues, TEXT("Command"), Command))
	{
		UE_LOG(LogDreamShader, Error, TEXT("%s"), UE::DreamShader::Editor::Private::GetDreamShaderCommandletUsage());
		return 1;
	}

	Command.TrimStartAndEndInline();
	if (Command.Equals(TEXT("compile"), ESearchCase::IgnoreCase)
		|| Command.Equals(TEXT("generate"), ESearchCase::IgnoreCase))
	{
		return UE::DreamShader::Editor::Private::RunDreamShaderCompileCommandlet(Tokens, Switches, ParamValues) ? 0 : 1;
	}

	if (Command.Equals(TEXT("decompile"), ESearchCase::IgnoreCase)
		|| Command.Equals(TEXT("export"), ESearchCase::IgnoreCase))
	{
		return UE::DreamShader::Editor::Private::RunDreamShaderDecompileCommandlet(
			Tokens,
			Switches,
			ParamValues,
			UE::DreamShader::Editor::Private::GetGraphDecompiler()) ? 0 : 1;
	}

	UE_LOG(LogDreamShader, Error, TEXT("Unknown DreamShader command '%s'.\n%s"), *Command, UE::DreamShader::Editor::Private::GetDreamShaderCommandletUsage());
	return 1;
}
