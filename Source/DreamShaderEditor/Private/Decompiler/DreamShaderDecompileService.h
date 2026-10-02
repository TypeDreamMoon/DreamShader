// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The decompile service: one request in, one text out, whichever decompiler is behind it.
//
// Two decompilers implement IDreamShaderDecompiler. The 1.x one (GetGraphDecompiler, Format = Legacy) writes `.dsm` /
// `.dsf` text one asset at a time through DecompileMaterial / DecompileFunction, and the service drives it as it
// always did. The 2.0 one (GetIRDecompiler, Format = Dss: graph -> IR -> AST -> printer) answers a whole request
// itself through DecompileRequest, because a request can be more than one asset -- every product of a source, in one
// module -- and because it has diagnostics to hand back. A plain material instance is neither's: Format = Dss writes
// its `.dsi` through the instance decompiler, and a Custom Pass pipeline its `.dsp` through the pipeline decompiler.
//
// Every editor caller goes through Tools/DreamShaderDecompileTools.h, which picks the decompiler for the format.
// Diagnostics: DSH9085-9089.

#pragma once

#include "CoreMinimal.h"
#include "Lang/LangDiagnostic.h"

class UObject;
class UMaterial;
class UMaterialFunction;
class UMaterialInterface;

namespace UE::DreamShader::Editor
{
	enum class EDreamShaderDecompiledFunctionKind : uint8
	{
		Function,
		MaterialLayer,
		MaterialLayerBlend
	};

	/** Which text a decompile writes. */
	enum class EDreamShaderDecompileFormat : uint8
	{
		/** By the output file's extension: `.dsm` / `.dsf` -> Legacy; anything else, or no output file -> Dss. */
		Auto,
		/** 2.0 text: `.dss` for a material, function, layer or blend; `.dsi` for a material instance; `.dsp` for a pass pipeline. */
		Dss,
		/** The 1.x text decompiler, unchanged (`.dsm` / `.dsf`); lives through 2.0.x. */
		Legacy,
	};

	struct FDreamShaderDecompileRequest
	{
		UObject* Asset = nullptr;
		FString OutputFilePath;
		EDreamShaderDecompileFormat Format = EDreamShaderDecompileFormat::Auto;
		/** Keep the asset's own object path: write `/// @name` when the output file would derive another. */
		bool bKeepAssetPath = false;
		/** Decompile every product of this source into ONE module (Adopt of a multi-product source). */
		FString SourceFilePath;
		/** Prefer HLSL sugar over class-exact reflected calls (RD-1). */
		bool bReadable = false;
	};

	struct FDreamShaderDecompileResult
	{
		bool bSucceeded = false;
		FString SourceText;
		FString OutputFilePath;
		FString Error;
		/** Every diagnostic of the decompile, errors included; warnings are also written into SourceText as `// Warning:` trivia. */
		TArray<UE::DreamShader::Lang::FLangDiagnostic> Diagnostics;
	};

	class IDreamShaderDecompiler
	{
	public:
		virtual ~IDreamShaderDecompiler() = default;

		virtual bool DecompileMaterial(UMaterial* Material, const FString& DecompiledName, FString& OutSourceText, FString& OutError) = 0;
		virtual bool DecompileFunction(
			UMaterialFunction* MaterialFunction,
			const FString& DecompiledName,
			EDreamShaderDecompiledFunctionKind FunctionKind,
			FString& OutSourceText,
			FString& OutError) = 0;

		/**
		 * A decompiler that answers a whole request itself fills OutResult -- success or not -- and answers true; the
		 * service then adds nothing but the checks that do not depend on the decompiler. The default answers false, and
		 * the service drives DecompileMaterial / DecompileFunction one asset at a time.
		 */
		virtual bool DecompileRequest(const FDreamShaderDecompileRequest& Request, FDreamShaderDecompileResult& OutResult)
		{
			return false;
		}
	};
}

namespace UE::DreamShader::Editor::Private
{
	struct FDecompiledAssetNaming
	{
		static FString MakeMaterialFilePath(const UMaterial* Material);
		static FString MakeFunctionFilePath(const UMaterialFunction* MaterialFunction);
		/** The 2.0 twin of the two above: `Decompiled/<Materials|Functions|Layers|LayerBlends>/<package path>.dss`. Empty for any other asset. */
		static FString MakeDssFilePath(const UObject* Asset);
		/** `Decompiled/Instances/<package path>.dsi`. */
		static FString MakeInstanceFilePath(const UMaterialInterface* Instance);
		/** `Decompiled/Pipelines/<package path>.dsp`: where a UDreamPassPipeline decompiles to when no file is named. */
		static FString MakePipelineFilePath(const UObject* Pipeline);
		static const TCHAR* GetFunctionCategory(EDreamShaderDecompiledFunctionKind FunctionKind);
		static EDreamShaderDecompiledFunctionKind GetFunctionKind(const UMaterialFunction* MaterialFunction);
		static FString MakeAssetName(const UObject* Asset, const TCHAR* Category);
	};

	struct FDecompiledSourceWriter
	{
		static bool Save(const FDreamShaderDecompileResult& Result, FString& OutError);
	};

	class FDreamShaderDecompileService
	{
	public:
		explicit FDreamShaderDecompileService(IDreamShaderDecompiler& InDecompiler)
			: Decompiler(InDecompiler)
		{
		}

		FDreamShaderDecompileResult DecompileAsset(const FDreamShaderDecompileRequest& Request);

	private:
		IDreamShaderDecompiler& Decompiler;
	};

}
