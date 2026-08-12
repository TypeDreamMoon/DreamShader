#include "DreamShaderDecompileService.h"

#include "DreamShaderModule.h"

#include "Diagnostics/DreamShaderTextWireUtils.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"

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
		if (!Request.Asset)
		{
			Result.Error = ToInvariantWireString(LOCTEXT("NoAssetProvided", "No asset was provided."));
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
