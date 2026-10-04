// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The editor's one seam onto the decompile service: which decompiler serves which format, what `Auto` means, and the
// service's diagnostics in the shapes the tools publish.
//
// Every editor caller goes through here -- the Content Browser and Material Content Browser exports, Adopt, the
// bridge's `decompile` request and the commandlet's `decompile` verb -- so "1.x text or 2.0 text" is decided in exactly
// one place. The service, the 1.x decompiler behind Format = Legacy (GetGraphDecompiler) and the 2.0 decompiler behind
// Format = Dss (GetIRDecompiler) belong to the decompiler unit (Decompiler/).

#pragma once

#include "CoreMinimal.h"

// FDreamShaderDecompileRequest / Result, EDreamShaderDecompileFormat.
#include "Decompiler/DreamShaderDecompileService.h"
// FLang2DiagnosticRecord: the shape the diagnostics JSON and the bridge responses are written from.
#include "DreamShaderCompilerDiagnostics.h"

class UObject;

namespace UE::DreamShader::Editor::Private
{
	/** `dss`, `legacy` or `auto`, in any case. False, with OutFormat untouched, for anything else. */
	bool TryParseDreamShaderDecompileFormat(const FString& Text, ::UE::DreamShader::Editor::EDreamShaderDecompileFormat& OutFormat);

	/** The lower-case spelling TryParseDreamShaderDecompileFormat reads back. */
	const TCHAR* LexDreamShaderDecompileFormat(::UE::DreamShader::Editor::EDreamShaderDecompileFormat Format);

	/**
	 * Auto made concrete. An output file ending in `.dsm` or `.dsf` asks for the 1.x text; anything else -- `.dss`, `.dsi`,
	 * `.dsp`, or no output file at all -- for the 2.0 text, which is the only text a pass pipeline has. Dss and Legacy come
	 * back unchanged.
	 */
	::UE::DreamShader::Editor::EDreamShaderDecompileFormat ResolveDreamShaderDecompileFormat(
		::UE::DreamShader::Editor::EDreamShaderDecompileFormat Format,
		const FString& OutputFilePath);

	/**
	 * One request through the service, with the decompiler its format names. The request's Format is resolved first
	 * (ResolveDreamShaderDecompileFormat) and handed to the service resolved, so the service never sees Auto from here.
	 * Nothing is written: FDecompiledSourceWriter::Save does that.
	 */
	::UE::DreamShader::Editor::FDreamShaderDecompileResult RunDreamShaderDecompileRequest(
		const ::UE::DreamShader::Editor::FDreamShaderDecompileRequest& Request);

	/**
	 * The result's diagnostics as diagnostics-store records, each with its code, stage, severity and span length.
	 * FallbackFilePath stands in for a diagnostic that names no file.
	 */
	void BuildDreamShaderDecompileDiagnosticRecords(
		const ::UE::DreamShader::Editor::FDreamShaderDecompileResult& Result,
		const FString& FallbackFilePath,
		TArray<::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord>& OutRecords);

	/**
	 * One line for a toast or a log about a failed decompile: the result's Error when it has one, else its first error
	 * diagnostic in the located wire form, else a generic sentence. Invariant text.
	 */
	FString DescribeDreamShaderDecompileFailure(const ::UE::DreamShader::Editor::FDreamShaderDecompileResult& Result);

	/** BuildDreamShaderDecompileDiagnosticRecords over a bare diagnostics array, for any tool result that carries one (migrate). */
	void BuildDreamShaderToolDiagnosticRecords(
		const TArray<::UE::DreamShader::Lang::FLangDiagnostic>& Diagnostics,
		const FString& FallbackFilePath,
		TArray<::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord>& OutRecords);

	/**
	 * The asset a tool names: an object path (`/Game/X/M_Steel.M_Steel`), or a package path for the asset named after its
	 * package (`/Game/X/M_Steel`). Found in memory first -- an Ephemeral product has no package -- and loaded otherwise.
	 * OutObjectPath is the path tried last. Null when nothing is found.
	 */
	UObject* LoadDreamShaderDecompileAsset(const FString& AssetPath, FString& OutObjectPath);

	/**
	 * For a decompile named by its source alone (`-SourceFile`, `sourceFile`): the first asset the source builds that exists, in
	 * declaration order, found in memory or loaded. The service then decompiles every product of the source. Null with
	 * OutError (invariant) when the source does not resolve or nothing it builds exists yet.
	 */
	UObject* LoadDreamShaderDecompileSourceProduct(const FString& SourceFilePath, FString& OutError);
}
