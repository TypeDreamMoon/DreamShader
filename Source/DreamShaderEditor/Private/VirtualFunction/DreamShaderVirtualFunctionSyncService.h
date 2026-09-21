#pragma once

#include "CoreMinimal.h"

#include "Diagnostics/DreamShaderDiagnosticsStore.h"
#include "DreamShaderTypes.h"

class UMaterialFunction;

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderVirtualFunctionDefinitionLocation
	{
		FString SourceFilePath;
		FString FunctionName;
		FString AssetObjectPath;
		FString CurrentText;
		TArray<FTextShaderFunctionParameter> Inputs;
		TArray<FTextShaderFunctionParameter> Outputs;
		int32 StartIndex = INDEX_NONE;
		int32 EndIndex = INDEX_NONE;
		int32 Line = 1;
		int32 Column = 1;
		/** A 2.0 `extern` prototype with `/// @asset`, not a 1.x VirtualFunction block. Found for navigation; sync does not rewrite it. */
		bool bExternPrototype = false;
		/** bExternPrototype only: the prototype returns a value, which Outputs lists first, named Result. */
		bool bExternPrototypeReturnsValue = false;
	};

	struct FDreamShaderVirtualFunctionSyncFileResult
	{
		FString SourceFilePath;
		int32 DefinitionCount = 0;
		int32 UpdatedDefinitionCount = 0;
		TArray<FDreamShaderDiagnosticRecord> Diagnostics;
	};

	struct FDreamShaderVirtualFunctionSyncResult
	{
		int32 ScannedDefinitionCount = 0;
		int32 UpdatedDefinitionCount = 0;
		int32 ErrorCount = 0;
		TArray<FDreamShaderVirtualFunctionSyncFileResult> Files;
	};

	struct FDreamShaderVirtualFunctionSyncService
	{
		using FDefinitionBuilder = TFunctionRef<bool(const UMaterialFunction*, FString&, FString&)>;

		static bool FindDefinitionForMaterialFunction(
			const UMaterialFunction* MaterialFunction,
			FDreamShaderVirtualFunctionDefinitionLocation& OutLocation);

		static FDreamShaderVirtualFunctionSyncResult SyncDefinitions(FDefinitionBuilder DefinitionBuilder);
	};
}
