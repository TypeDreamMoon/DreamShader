#include "DreamShaderDecompileService.h"

#include "DreamShaderModule.h"

// FormatLang2DiagnosticWireLine: the located form FDreamShaderDecompileResult::Error carries.
#include "DreamShaderCompilerDiagnostics.h"
#include "DreamShaderTextWireUtils.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "DreamShader.Decompiler.Service"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		FString MakeDecompiledAssetPathSegment(const FString& InSegment, const TCHAR* FallbackPrefix, const int32 Index)
		{
			FString Result = InSegment.TrimStartAndEnd();
			if (Result.IsEmpty())
			{
				return FString::Printf(TEXT("%s%d"), FallbackPrefix, Index + 1); // I18N-EXEMPT
			}

			Result.ReplaceInline(TEXT("\\"), TEXT("_"));
			Result.ReplaceInline(TEXT("/"), TEXT("_"));
			Result.ReplaceInline(TEXT("."), TEXT("_"));
			Result.ReplaceInline(TEXT(":"), TEXT("_"));
			return Result;
		}

		FString MakeDecompiledSourceFileSegment(const FString& InSegment, const TCHAR* FallbackPrefix, const int32 Index)
		{
			FString Result = InSegment.TrimStartAndEnd();
			if (Result.IsEmpty())
			{
				return FString::Printf(TEXT("%s%d"), FallbackPrefix, Index + 1); // I18N-EXEMPT
			}

			for (int32 CharIndex = 0; CharIndex < Result.Len(); ++CharIndex)
			{
				const TCHAR Character = Result[CharIndex];
				if (Character < TCHAR(' ')
					|| Character == TCHAR('<')
					|| Character == TCHAR('>')
					|| Character == TCHAR(':')
					|| Character == TCHAR('"')
					|| Character == TCHAR('/')
					|| Character == TCHAR('\\')
					|| Character == TCHAR('|')
					|| Character == TCHAR('?')
					|| Character == TCHAR('*'))
				{
					Result[CharIndex] = TCHAR('_');
				}
			}

			Result.TrimStartAndEndInline();
			return Result.IsEmpty()
				? FString::Printf(TEXT("%s%d"), FallbackPrefix, Index + 1) // I18N-EXEMPT
				: Result;
		}

		FString MakeStableDecompiledSourcePath(const UObject* Asset, const FString& CategoryDirectory, const TCHAR* Extension)
		{
			FString PackageName = Asset && Asset->GetOutermost() ? Asset->GetOutermost()->GetName() : FString();
			PackageName.TrimStartAndEndInline();
			PackageName.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (PackageName.StartsWith(TEXT("/")))
			{
				PackageName.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}
			while (PackageName.EndsWith(TEXT("/")))
			{
				PackageName.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
			}

			TArray<FString> Segments;
			PackageName.ParseIntoArray(Segments, TEXT("/"), true);
			if (Segments.IsEmpty())
			{
				Segments.Add(Asset ? Asset->GetName() : TEXT("Export"));
			}

			FString RelativePath;
			for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
			{
				const FString SanitizedSegment = MakeDecompiledSourceFileSegment(
					Segments[SegmentIndex],
					SegmentIndex + 1 == Segments.Num() ? TEXT("Asset") : TEXT("Folder"),
					SegmentIndex);
				RelativePath = RelativePath.IsEmpty()
					? SanitizedSegment
					: FPaths::Combine(RelativePath, SanitizedSegment);
			}

			return UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(
				UE::DreamShader::GetSourceShaderDirectory(),
				CategoryDirectory,
				RelativePath + Extension));
		}
	}

	namespace
	{
		/** One service-level error: in the result's list, and as its Error line when it is the first thing that went wrong. */
		void AddDecompileServiceError(FDreamShaderDecompileResult& Result, const TCHAR* Code, const FText& Message)
		{
			UE::DreamShader::Lang::FLangDiagnostic Diagnostic;
			Diagnostic.Code = Code;
			Diagnostic.Severity = UE::DreamShader::Lang::ELangSeverity::Error;
			Diagnostic.Message = Message;
			Diagnostic.FilePath = Result.OutputFilePath;
			if (Result.Error.IsEmpty())
			{
				Result.Error = ::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(Diagnostic, Result.OutputFilePath);
			}
			Result.Diagnostics.Add(MoveTemp(Diagnostic));
		}

		bool HasDecompileErrorDiagnostic(const FDreamShaderDecompileResult& Result)
		{
			return Result.Diagnostics.ContainsByPredicate([](const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == UE::DreamShader::Lang::ELangSeverity::Error;
			});
		}
	}

	FString FDecompiledAssetNaming::MakeDssFilePath(const UObject* Asset)
	{
		if (const UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Asset))
		{
			return MakeStableDecompiledSourcePath(
				MaterialFunction,
				FString::Printf(TEXT("Decompiled/%s"), GetFunctionCategory(GetFunctionKind(MaterialFunction))), // I18N-EXEMPT
				TEXT(".dss"));
		}
		// A material, or the ThinCustom pair that stands for one.
		if (const UMaterialInterface* Material = Cast<UMaterialInterface>(Asset))
		{
			return MakeStableDecompiledSourcePath(Material, TEXT("Decompiled/Materials"), TEXT(".dss"));
		}
		return FString();
	}

	FString FDecompiledAssetNaming::MakeInstanceFilePath(const UMaterialInterface* Instance)
	{
		return MakeStableDecompiledSourcePath(Instance, TEXT("Decompiled/Instances"), TEXT(".dsi"));
	}

	FString FDecompiledAssetNaming::MakeMaterialFilePath(const UMaterial* Material)
	{
		return MakeStableDecompiledSourcePath(Material, TEXT("Decompiled/Materials"), TEXT(".dsm"));
	}

	FString FDecompiledAssetNaming::MakeFunctionFilePath(const UMaterialFunction* MaterialFunction)
	{
		return MakeStableDecompiledSourcePath(
			MaterialFunction,
			FString::Printf(TEXT("Decompiled/%s"), GetFunctionCategory(GetFunctionKind(MaterialFunction))), // I18N-EXEMPT
			TEXT(".dsf"));
	}

	const TCHAR* FDecompiledAssetNaming::GetFunctionCategory(const EDreamShaderDecompiledFunctionKind FunctionKind)
	{
		switch (FunctionKind)
		{
		case EDreamShaderDecompiledFunctionKind::MaterialLayer:
			return TEXT("Layers");
		case EDreamShaderDecompiledFunctionKind::MaterialLayerBlend:
			return TEXT("LayerBlends");
		case EDreamShaderDecompiledFunctionKind::Function:
		default:
			return TEXT("Functions");
		}
	}

	EDreamShaderDecompiledFunctionKind FDecompiledAssetNaming::GetFunctionKind(const UMaterialFunction* MaterialFunction)
	{
		if (MaterialFunction && MaterialFunction->IsA<UMaterialFunctionMaterialLayerBlend>())
		{
			return EDreamShaderDecompiledFunctionKind::MaterialLayerBlend;
		}
		if (MaterialFunction && MaterialFunction->IsA<UMaterialFunctionMaterialLayer>())
		{
			return EDreamShaderDecompiledFunctionKind::MaterialLayer;
		}
		return EDreamShaderDecompiledFunctionKind::Function;
	}

	FString FDecompiledAssetNaming::MakeAssetName(const UObject* Asset, const TCHAR* Category)
	{
		FString PackageName = Asset && Asset->GetOutermost() ? Asset->GetOutermost()->GetName() : FString();
		PackageName.TrimStartAndEndInline();
		PackageName.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (PackageName.StartsWith(TEXT("/")))
		{
			PackageName.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
		}
		while (PackageName.EndsWith(TEXT("/")))
		{
			PackageName.LeftChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
		}

		TArray<FString> Segments;
		PackageName.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.IsEmpty())
		{
			Segments.Add(Asset ? Asset->GetName() : TEXT("Asset"));
		}

		FString RelativeName;
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FString SanitizedSegment = MakeDecompiledAssetPathSegment(
				Segments[SegmentIndex],
				SegmentIndex + 1 == Segments.Num() ? TEXT("Asset") : TEXT("Folder"),
				SegmentIndex);
			if (!RelativeName.IsEmpty())
			{
				RelativeName += TEXT("/");
			}
			RelativeName += SanitizedSegment;
		}

		return FString::Printf(TEXT("Decompiled/%s/%s"), Category, *RelativeName); // I18N-EXEMPT
	}

	bool FDecompiledSourceWriter::Save(const FDreamShaderDecompileResult& Result, FString& OutError)
	{
		if (!Result.bSucceeded)
		{
			OutError = Result.Error.IsEmpty() ? ToInvariantWireString(LOCTEXT("DecompileDidNotProduceSourceText", "Decompile did not produce source text.")) : Result.Error;
			return false;
		}
		if (Result.OutputFilePath.IsEmpty())
		{
			OutError = ToInvariantWireString(LOCTEXT("FailedToResolveOutputFilePath", "DreamShader failed to resolve an output file path."));
			return false;
		}

		const FString OutputDirectory = FPaths::GetPath(Result.OutputFilePath);
		if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
		{
			OutError = ToInvariantWireString(FText::Format(LOCTEXT("FailedToCreateOutputDirectory", "DreamShader failed to create output directory '{0}'."), FText::FromString(OutputDirectory)));
			return false;
		}

		if (!FFileHelper::SaveStringToFile(Result.SourceText, *Result.OutputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = ToInvariantWireString(FText::Format(LOCTEXT("FailedToWriteDecompiledSource", "DreamShader failed to write decompiled source '{0}'."), FText::FromString(Result.OutputFilePath)));
			return false;
		}

		return true;
	}

	FDreamShaderDecompileResult FDreamShaderDecompileService::DecompileAsset(const FDreamShaderDecompileRequest& Request)
	{
		FDreamShaderDecompileResult Result;
		if (!Request.Asset && Request.SourceFilePath.IsEmpty())
		{
			Result.Error = ToInvariantWireString(LOCTEXT("NoAssetProvided", "No asset was provided."));
			return Result;
		}

		// The extension says which language the file is read as, so text of the other one under it is a file nothing
		// opens. Auto never gets here from the editor (Tools/DreamShaderDecompileTools.h resolves it), and means "by the
		// extension" when it does.
		const FString Extension = FPaths::GetExtension(Request.OutputFilePath, /*bIncludeDot*/ false);
		const bool bLegacyExtension = Extension.Equals(TEXT("dsm"), ESearchCase::IgnoreCase) || Extension.Equals(TEXT("dsf"), ESearchCase::IgnoreCase);
		const bool bDssExtension = Extension.Equals(TEXT("dss"), ESearchCase::IgnoreCase) || Extension.Equals(TEXT("dsi"), ESearchCase::IgnoreCase);
		const EDreamShaderDecompileFormat Format = Request.Format != EDreamShaderDecompileFormat::Auto
			? Request.Format
			: (bLegacyExtension ? EDreamShaderDecompileFormat::Legacy : EDreamShaderDecompileFormat::Dss);
		if ((Format == EDreamShaderDecompileFormat::Legacy && bDssExtension) || (Format == EDreamShaderDecompileFormat::Dss && bLegacyExtension))
		{
			Result.OutputFilePath = UE::DreamShader::NormalizeSourceFilePath(Request.OutputFilePath);
			AddDecompileServiceError(Result, TEXT("DSH9085"), FText::Format(
				LOCTEXT("DecompileFormatContradictsExtension", "'{0}' ends in '.{1}', which is read as {2} source, and the decompile was asked for {3} text; name the file after the text, or leave the format to the extension."),
				FText::FromString(Request.OutputFilePath),
				FText::FromString(Extension),
				bLegacyExtension ? LOCTEXT("DecompileLegacyLanguage", "1.x") : LOCTEXT("DecompileDssLanguage", "2.0"),
				Format == EDreamShaderDecompileFormat::Legacy ? LOCTEXT("DecompileLegacyText", "1.x") : LOCTEXT("DecompileDssText", "2.0")));
			return Result;
		}

		if (Format == EDreamShaderDecompileFormat::Dss)
		{
			FDreamShaderDecompileRequest Resolved = Request;
			Resolved.Format = Format;
			if (!Decompiler.DecompileRequest(Resolved, Result))
			{
				// The 1.x decompiler behind a 2.0 request: it has no such text to give.
				Result = FDreamShaderDecompileResult();
				AddDecompileServiceError(Result, TEXT("DSH9086"), LOCTEXT("DecompilerCannotWriteDss",
					"The decompile was asked for 2.0 text and was handed the 1.x decompiler, which writes '.dsm' and '.dsf' only; build the service with GetIRDecompiler() for Format = Dss."));
				return Result;
			}

			// A failure always says why: a wire line, or an error among the diagnostics.
			if (Result.bSucceeded && HasDecompileErrorDiagnostic(Result))
			{
				Result.bSucceeded = false;
			}
			if (!Result.bSucceeded && Result.Error.IsEmpty())
			{
				for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Result.Diagnostics)
				{
					if (Diagnostic.Severity == UE::DreamShader::Lang::ELangSeverity::Error)
					{
						Result.Error = ::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(Diagnostic, Result.OutputFilePath);
						break;
					}
				}
				if (Result.Error.IsEmpty())
				{
					Result.Error = ToInvariantWireString(LOCTEXT("DecompileDidNotSayWhy", "The decompile failed without reporting why."));
				}
			}
			return Result;
		}

		if (!Request.Asset)
		{
			Result.Error = ToInvariantWireString(LOCTEXT("LegacyDecompileNeedsAsset", "The 1.x decompiler takes one asset at a time; name the asset rather than its source."));
			return Result;
		}

		if (UMaterial* Material = Cast<UMaterial>(Request.Asset))
		{
			Result.bSucceeded = Decompiler.DecompileMaterial(
				Material,
				FDecompiledAssetNaming::MakeAssetName(Material, TEXT("Materials")),
				Result.SourceText,
				Result.Error);
			Result.OutputFilePath = Request.OutputFilePath.IsEmpty()
				? FDecompiledAssetNaming::MakeMaterialFilePath(Material)
				: UE::DreamShader::NormalizeSourceFilePath(Request.OutputFilePath);
			return Result;
		}

		if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Request.Asset))
		{
			const EDreamShaderDecompiledFunctionKind FunctionKind = FDecompiledAssetNaming::GetFunctionKind(MaterialFunction);
			Result.bSucceeded = Decompiler.DecompileFunction(
				MaterialFunction,
				FDecompiledAssetNaming::MakeAssetName(
					MaterialFunction,
					FDecompiledAssetNaming::GetFunctionCategory(FunctionKind)),
				FunctionKind,
				Result.SourceText,
				Result.Error);
			Result.OutputFilePath = Request.OutputFilePath.IsEmpty()
				? FDecompiledAssetNaming::MakeFunctionFilePath(MaterialFunction)
				: UE::DreamShader::NormalizeSourceFilePath(Request.OutputFilePath);
			return Result;
		}

		Result.Error = ToInvariantWireString(FText::Format(LOCTEXT("UnsupportedAssetType", "DreamShader decompile supports Material and MaterialFunction assets only: {0}"), FText::FromString(Request.Asset->GetPathName())));
		return Result;
	}
#undef LOCTEXT_NAMESPACE

}
