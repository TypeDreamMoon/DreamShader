#include "DreamShaderCommandletRunner.h"

#include "DreamShaderCompileService.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Compile/DreamShaderEditorCompileAdapter.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "DreamShaderModule.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"

#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private
{
	const TCHAR* GetDreamShaderCommandletUsage()
	{
		return TEXT(
			"Usage:\n"
			"  -run=DreamShader compile -Source=\"C:/Project/DShader/File.dsm\" [-Force]\n"
			"  -run=DreamShader compile -All [-Force]\n"
			"  -run=DreamShader decompile -Asset=\"/Game/Path/Asset.Asset\" [-Out=\"C:/Project/DShader/Decompiled/File.dsm\"]\n"
			"Supported asset types: Material -> .dsm, MaterialFunction -> .dsf.");
	}

	FString NormalizeCommandletValue(FString Value)
	{
		Value.TrimStartAndEndInline();
		Value = Value.TrimQuotes();
		Value.TrimStartAndEndInline();
		return Value;
	}

	FString NormalizeCommandletKey(FString Key)
	{
		Key.TrimStartAndEndInline();
		while (Key.StartsWith(TEXT("-")))
		{
			Key.RightChopInline(1, DREAMSHADER_ALLOW_SHRINKING_NO);
		}
		Key.TrimStartAndEndInline();
		return Key;
	}

	bool TrySplitCommandletAssignment(const FString& Text, FString& OutKey, FString& OutValue)
	{
		FString Key;
		FString Value;
		if (!Text.Split(TEXT("="), &Key, &Value))
		{
			return false;
		}

		OutKey = NormalizeCommandletKey(Key);
		OutValue = NormalizeCommandletValue(Value);
		return !OutKey.IsEmpty();
	}

	bool TryGetCommandletParam(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		const FString& Name,
		FString& OutValue)
	{
		for (const TPair<FString, FString>& Param : Params)
		{
			if (NormalizeCommandletKey(Param.Key).Equals(Name, ESearchCase::IgnoreCase))
			{
				OutValue = NormalizeCommandletValue(Param.Value);
				return !OutValue.IsEmpty();
			}
		}

		for (const FString& Switch : Switches)
		{
			FString Key;
			FString Value;
			if (TrySplitCommandletAssignment(Switch, Key, Value) && Key.Equals(Name, ESearchCase::IgnoreCase))
			{
				OutValue = MoveTemp(Value);
				return !OutValue.IsEmpty();
			}
		}

		for (const FString& Token : Tokens)
		{
			FString Key;
			FString Value;
			if (TrySplitCommandletAssignment(Token, Key, Value) && Key.Equals(Name, ESearchCase::IgnoreCase))
			{
				OutValue = MoveTemp(Value);
				return !OutValue.IsEmpty();
			}
		}

		return false;
	}

	namespace
	{
		bool TryParseCommandletBool(const FString& Text, bool& OutValue)
		{
			const FString Normalized = NormalizeCommandletValue(Text).ToLower();
			if (Normalized.IsEmpty()
				|| Normalized == TEXT("1")
				|| Normalized == TEXT("true")
				|| Normalized == TEXT("yes")
				|| Normalized == TEXT("on"))
			{
				OutValue = true;
				return true;
			}

			if (Normalized == TEXT("0")
				|| Normalized == TEXT("false")
				|| Normalized == TEXT("no")
				|| Normalized == TEXT("off"))
			{
				OutValue = false;
				return true;
			}

			return false;
		}

		bool HasCommandletFlag(const TArray<FString>& Tokens, const TArray<FString>& Switches, const FString& Name)
		{
			for (const FString& Switch : Switches)
			{
				FString Key;
				FString Value;
				const bool bHasValue = TrySplitCommandletAssignment(Switch, Key, Value);
				if (!bHasValue)
				{
					Key = NormalizeCommandletKey(Switch);
				}

				if (Key.Equals(Name, ESearchCase::IgnoreCase))
				{
					bool bParsedValue = true;
					return !bHasValue || !TryParseCommandletBool(Value, bParsedValue) || bParsedValue;
				}
			}

			for (const FString& Token : Tokens)
			{
				FString Key;
				FString Value;
				const bool bHasValue = TrySplitCommandletAssignment(Token, Key, Value);
				if (!bHasValue)
				{
					Key = NormalizeCommandletKey(Token);
				}

				if (Key.Equals(Name, ESearchCase::IgnoreCase))
				{
					bool bParsedValue = true;
					return !bHasValue || !TryParseCommandletBool(Value, bParsedValue) || bParsedValue;
				}
			}

			return false;
		}

		FString ResolveCommandletSourceFilePath(const FString& InSourceFilePath)
		{
			FString SourceFilePath = NormalizeCommandletValue(InSourceFilePath);
			if (SourceFilePath.IsEmpty() || !FPaths::IsRelative(SourceFilePath))
			{
				return UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
			}

			const FString SourceDirectoryCandidate = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), SourceFilePath));
			if (IFileManager::Get().FileExists(*SourceDirectoryCandidate))
			{
				return SourceDirectoryCandidate;
			}

			const FString ProjectCandidate = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(FPaths::ProjectDir(), SourceFilePath));
			if (IFileManager::Get().FileExists(*ProjectCandidate))
			{
				return ProjectCandidate;
			}

			return UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		}

		FString NormalizeCommandletAssetPath(const FString& InAssetPath)
		{
			FString AssetPath = NormalizeCommandletValue(InAssetPath);
			AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (AssetPath.StartsWith(TEXT("/")) && !AssetPath.Contains(TEXT(".")))
			{
				const FString AssetName = FPackageName::GetShortName(AssetPath);
				if (!AssetName.IsEmpty())
				{
					return FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
				}
			}
			return AssetPath;
		}

		UObject* LoadCommandletAsset(const FString& InAssetPath, FString& OutLoadPath)
		{
			OutLoadPath = NormalizeCommandletAssetPath(InAssetPath);
			UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *OutLoadPath);
			if (!Asset && !OutLoadPath.Equals(InAssetPath, ESearchCase::CaseSensitive))
			{
				const FString OriginalPath = NormalizeCommandletValue(InAssetPath);
				Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *OriginalPath);
				if (Asset)
				{
					OutLoadPath = OriginalPath;
				}
			}
			return Asset;
		}
	}

	bool RunDreamShaderCompileCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params)
	{
		const bool bForce = HasCommandletFlag(Tokens, Switches, TEXT("Force"));
		const bool bAll = HasCommandletFlag(Tokens, Switches, TEXT("All"));

		TArray<FString> SourceFiles;
		FString SourceFilePath;
		if (TryGetCommandletParam(Tokens, Switches, Params, TEXT("Source"), SourceFilePath)
			|| TryGetCommandletParam(Tokens, Switches, Params, TEXT("File"), SourceFilePath))
		{
			SourceFiles.Add(ResolveCommandletSourceFilePath(SourceFilePath));
		}
		else if (bAll)
		{
			FDreamShaderSourceFileUtils::FindProjectDreamShaderSourceFiles(SourceFiles);
			SourceFiles.RemoveAll([](const FString& SourceFile)
			{
				return UE::DreamShader::IsDreamShaderHeaderFile(SourceFile);
			});
			SourceFiles.Sort([](const FString& Left, const FString& Right)
			{
				const int32 LeftRank = UE::DreamShader::IsDreamShaderFunctionFile(Left) ? 0 : 1;
				const int32 RightRank = UE::DreamShader::IsDreamShaderFunctionFile(Right) ? 0 : 1;
				if (LeftRank != RightRank)
				{
					return LeftRank < RightRank;
				}

				return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
			});
		}
		else
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderCommandletUsage());
			return false;
		}

		if (SourceFiles.IsEmpty())
		{
			UE_LOG(LogDreamShader, Warning, TEXT("DreamShader commandlet found no source files to compile."));
			return true;
		}

		UE::DreamShader::Compiler::FDreamShaderCompileService CompileService(UE::DreamShader::Editor::GetEditorCompileAdapter());
		bool bSucceeded = true;
		for (const FString& SourceFile : SourceFiles)
		{
			if (!UE::DreamShader::IsDreamShaderSourceFile(SourceFile) || UE::DreamShader::IsDreamShaderHeaderFile(SourceFile))
			{
				UE_LOG(LogDreamShader, Error, TEXT("DreamShader compile requires a .dsm or .dsf file: %s"), *SourceFile);
				bSucceeded = false;
				continue;
			}

			const UE::DreamShader::Compiler::FDreamShaderCompileResult Result = CompileService.CompileAssets(SourceFile, bForce);
			if (Result.bSucceeded)
			{
				UE_LOG(LogDreamShader, Display, TEXT("%s"), *ToInvariantWireString(Result.Message));
			}
			else
			{
				UE_LOG(LogDreamShader, Error, TEXT("%s"), *ToInvariantWireString(Result.Message));
				bSucceeded = false;
			}
		}

		return bSucceeded;
	}

	bool RunDreamShaderDecompileCommandlet(
		const TArray<FString>& Tokens,
		const TArray<FString>& Switches,
		const TMap<FString, FString>& Params,
		UE::DreamShader::Editor::IDreamShaderDecompiler& Decompiler)
	{
		FString AssetPath;
		if (!TryGetCommandletParam(Tokens, Switches, Params, TEXT("Asset"), AssetPath))
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), GetDreamShaderCommandletUsage());
			return false;
		}

		FString LoadPath;
		UObject* Asset = LoadCommandletAsset(AssetPath, LoadPath);
		if (!Asset)
		{
			UE_LOG(LogDreamShader, Error, TEXT("DreamShader could not load asset '%s'."), *AssetPath);
			return false;
		}

		FString OutputPath;
		if (!TryGetCommandletParam(Tokens, Switches, Params, TEXT("Out"), OutputPath)
			&& !TryGetCommandletParam(Tokens, Switches, Params, TEXT("Output"), OutputPath))
		{
			OutputPath.Reset();
		}

		FDreamShaderDecompileService DecompileService(Decompiler);
		UE::DreamShader::Editor::FDreamShaderDecompileRequest Request;
		Request.Asset = Asset;
		Request.OutputFilePath = OutputPath;
		const UE::DreamShader::Editor::FDreamShaderDecompileResult Result = DecompileService.DecompileAsset(Request);
		if (!Result.bSucceeded)
		{
			UE_LOG(LogDreamShader, Error, TEXT("DreamShader failed to decompile '%s': %s"), *LoadPath, *Result.Error);
			return false;
		}

		FString SaveError;
		if (!FDecompiledSourceWriter::Save(Result, SaveError))
		{
			UE_LOG(LogDreamShader, Error, TEXT("%s"), *SaveError);
			return false;
		}

		UE_LOG(LogDreamShader, Display, TEXT("DreamShader decompiled '%s' to '%s'."), *LoadPath, *Result.OutputFilePath);
		return true;
	}
}
