#pragma once

#include "CoreMinimal.h"

// FDreamShaderError: the code-carrying overloads below. The FString ones stay so callers that
// only want the text -- notifications, tests -- are unchanged; the diagnostics store takes the
// other pair, so a DSHnnnn code survives the trip out of the generator.
#include "DreamShaderDiagnostic.h"

namespace UE::DreamShader::Editor
{
	/**
	 * Fired once per outermost generation of a source file, after it succeeded or failed, with the
	 * normalized source path. Every compile route -- the bridge's watcher, a commandlet, the Material
	 * Content Browser, a provenance action, a test -- ends up in FMaterialGenerator, so this is the one
	 * place a "this source was just (re)built" signal is complete. GenerateAssetsFromFile generates the
	 * material through GenerateMaterialFromFile; only the outer call fires.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDreamShaderSourceGenerated, const FString& /*SourceFilePath*/, bool /*bSucceeded*/);
	FOnDreamShaderSourceGenerated& OnDreamShaderSourceGenerated();

	class FMaterialGenerator
	{
	public:
		static bool GenerateAssetsFromFile(const FString& SourceFilePath, FText& OutMessage, bool bForce = false, bool bTransient = false)
		{
			FString Message;
			const bool bResult = GenerateAssetsFromFile(SourceFilePath, Message, bForce, bTransient);
			OutMessage = FText::FromString(Message);
			return bResult;
		}
		static bool GenerateMaterialFromFile(const FString& SourceFilePath, FText& OutMessage, bool bForce = false, bool bTransient = false)
		{
			FString Message;
			const bool bResult = GenerateMaterialFromFile(SourceFilePath, Message, bForce, bTransient);
			OutMessage = FText::FromString(Message);
			return bResult;
		}
		static bool GenerateAssetsFromFile(const FString& SourceFilePath, FString& OutMessage, bool bForce = false, bool bTransient = false);
		static bool GenerateMaterialFromFile(const FString& SourceFilePath, FString& OutMessage, bool bForce = false, bool bTransient = false);
		static bool GenerateAssetsFromFile(const FString& SourceFilePath, FDreamShaderError& OutError, bool bForce = false, bool bTransient = false);
		static bool GenerateMaterialFromFile(const FString& SourceFilePath, FDreamShaderError& OutError, bool bForce = false, bool bTransient = false);

	private:
		// The bodies. The public overloads above wrap them in the OnDreamShaderSourceGenerated notice.
		static bool GenerateAssetsFromFileInternal(const FString& SourceFilePath, FDreamShaderError& OutError, bool bForce, bool bTransient);
		static bool GenerateMaterialFromFileInternal(const FString& SourceFilePath, FDreamShaderError& OutError, bool bForce, bool bTransient);
	};
}
