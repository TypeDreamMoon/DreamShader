#include "DreamShaderCommandletRunner.h"

#include "DreamShaderCompileService.h"
#include "Decompiler/DreamShaderDecompileService.h"
#include "Compile/DreamShaderEditorCompileAdapter.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "DreamShaderDefineTable.h"
#include "DreamShaderModule.h"
#include "SourceFiles/DreamShaderSourceFileUtils.h"

#include "Commandlets/Commandlet.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace UE::DreamShader::Editor::Private
{
	const TCHAR* GetDreamShaderCommandletUsage()
	{
		return TEXT(
			"Usage:\n"
			"  -run=DreamShader compile -Source=\"C:/Project/DShader/File.dsm\" [-Force] [-Define=NAME=VALUE ...]\n"
			"  -run=DreamShader compile -All [-Force] [-Define=NAME=VALUE ...]\n"
			"  -run=DreamShader decompile -Asset=\"/Game/Path/Asset.Asset\" [-Out=\"C:/Project/DShader/Decompiled/File.dsm\"]\n"
			"Supported asset types: Material -> .dsm, MaterialFunction -> .dsf.\n"
			"-Define (short form -D) may be repeated; -Define=NAME with no value is a bare marker that\n"
			"defined(NAME) sees. Names starting with DS_ are reserved for the built-in constants.");
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

	int32 ApplyDreamShaderCommandletDefines(const FString& CommandLine)
	{
		// The command line is re-tokenized here, with the THREE-argument ParseCommandLine, instead of
		// reading the Switches array Main already has. That is the whole reason this function takes a
		// string.
		//
		// UCommandlet's four-argument overload does not COPY an assignment-shaped switch into its
		// Params map, it MOVES it: `Params.Add(Switch.Left(i), ...)` is immediately followed by
		// `Switches.RemoveAt(SwitchIdx)`. Two consequences, both fatal to repeating a flag:
		//   * every `-Define=...` is gone from Switches by the time Main sees it, so scanning that
		//     array finds nothing at all; and
		//   * Params is a TMap keyed on the SWITCH name, so `-Define=A=1 -Define=B=2` collapses to a
		//     single entry under "Define" -- one define survives out of any number passed, silently.
		// The three-argument overload does none of that folding, so the array it fills keeps every
		// occurrence, in the order they were written.
		//
		// The cost is one extra pass over a string that is already in memory, which is nothing next to
		// dropping every define but one and never saying so.
		TArray<FString> RawTokens;
		TArray<FString> RawSwitches;
		UCommandlet::ParseCommandLine(*CommandLine, RawTokens, RawSwitches);

		UE::DreamShader::FDreamShaderDefineValueMap Defines;
		for (const FString& Switch : RawSwitches)
		{
			FString SwitchName;
			FString Payload;
			if (!TrySplitCommandletAssignment(Switch, SwitchName, Payload))
			{
				// A bare flag such as -Force or -All. It cannot carry a define, and a bare `-Define`
				// with nothing after it names nothing, so there is nothing to warn about either.
				continue;
			}

			if (!SwitchName.Equals(TEXT("Define"), ESearchCase::IgnoreCase)
				&& !SwitchName.Equals(TEXT("D"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			// The payload is `NAME=VALUE`, or a bare `NAME` whose value is empty. Empty is not the same
			// as absent: defined(NAME) is true for a bare marker and arithmetic reads it as 1, which is
			// the entire point of allowing the short form.
			//
			// Split by hand rather than through TrySplitCommandletAssignment. That helper normalizes a
			// SWITCH name -- it strips leading dashes -- so it would quietly turn a mistyped
			// `-D=-FOO=1` into a perfectly valid `FOO`. Define names are also CASE-SENSITIVE
			// (FDreamShaderDefineTable keys on FString precisely so `Foo` and `FOO` stay distinct), so
			// nothing on this path may fold case either.
			FString Name = Payload;
			FString Value;
			int32 AssignmentIndex = INDEX_NONE;
			if (Payload.FindChar(TEXT('='), AssignmentIndex))
			{
				Name = Payload.Left(AssignmentIndex);
				Value = NormalizeCommandletValue(Payload.Mid(AssignmentIndex + 1));
			}
			Name.TrimStartAndEndInline();

			if (!UE::DreamShader::IsValidDreamShaderDefineName(Name))
			{
				UE_LOG(
					LogDreamShader,
					Warning,
					TEXT("DreamShader ignored -%s: '%s' is not a valid define name (it must match [A-Za-z_][A-Za-z0-9_]*)."),
					*Switch,
					*Name);
				continue;
			}

			// Checked here as well as in ResolveDreamShaderDefines, which drops reserved names on its
			// own. This is not redundancy for its own sake: the resolve path has no command line to
			// quote back and runs once per compile, so the same typo would either be invisible or
			// repeated for every file of a -All run. Refusing at the point of entry says it once, with
			// the text the user actually typed.
			if (UE::DreamShader::IsReservedDreamShaderDefineName(Name))
			{
				UE_LOG(
					LogDreamShader,
					Warning,
					TEXT("DreamShader ignored -%s: the DS_ prefix is reserved for the built-in constants, which describe the compiling process itself and cannot be overridden."),
					*Switch);
				continue;
			}

			// Last occurrence wins, matching how a shell reads a repeated flag and how a C compiler
			// treats a repeated -D.
			Defines.Add(MoveTemp(Name), MoveTemp(Value));
		}

		if (Defines.IsEmpty())
		{
			// Deliberately not calling with an empty map. In a fresh commandlet process the two are
			// equivalent in effect, but the setter bumps the define revision, and a bump that nothing
			// changed is a lie told to every cache that watches it.
			return 0;
		}

		// Logged from a sorted key array rather than by iterating the map, because TMap iteration order
		// is not stable and an unsorted line would shuffle between otherwise identical runs, which
		// makes two build logs impossible to diff.
		//
		// Sorted case-sensitively for the same reason FDreamShaderDefineTable::GetSortedNames is:
		// TArray::Sort's default predicate is FString::operator<, which compares with Stricmp, so two
		// names differing only in case have no defined relative order under it -- and case is exactly
		// what this feature promises to preserve.
		TArray<FString> Names;
		Defines.GenerateKeyArray(Names);
		Names.Sort([](const FString& Left, const FString& Right)
		{
			return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
		});

		FString Summary;
		for (const FString& Name : Names)
		{
			const FString& Value = Defines.FindChecked(Name);
			if (!Summary.IsEmpty())
			{
				Summary += TEXT(", ");
			}
			Summary += Value.IsEmpty() ? Name : FString::Printf(TEXT("%s=%s"), *Name, *Value);
		}

		UE::DreamShader::SetDreamShaderCommandLineDefines(Defines);
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader command-line defines: %s"), *Summary);
		return Defines.Num();
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
