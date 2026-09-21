// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// One located diagnostic about one source file, in the shape the editor's diagnostics store holds and
// the wire files (diagnostics.json, diagnostics/*.json, bridge.db) publish.
//
// Split out of Diagnostics/DreamShaderDiagnosticsStore.h in the compiler relocation, so that the compiler
// module can build records (DreamShaderCompilerDiagnostics.h) without depending on the store, which
// keeps SQLite and stays in the editor module. Plain data: nothing here needs exporting.

#pragma once

#include "CoreMinimal.h"

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderDiagnosticRecord
	{
		FString FilePath;
		FText Message;
		FText Detail;
		FString Stage;
		FString AssetPath;
		FString ShaderPlatform;
		FString QualityLevel;
		FString Code;
		int32 Line = 1;
		int32 Column = 1;
		FString Severity = TEXT("error");
		FString Source = TEXT("DreamShader");
		FString OwnerSourceFilePath;
	};

	struct FDreamShaderDiagnosticLocation
	{
		FString FilePath;
		FText Message;
		int32 Line = 1;
		int32 Column = 1;
	};
}
