#include "DreamShaderModule.h"

#include "DreamShaderSettings.h"

#include "HAL/FileManager.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogDreamShader);

namespace UE::DreamShader
{
	namespace Private
	{
		static const FString GeneratedShaderVirtualDirectory = TEXT("/DreamShaderGenerated");

		static FString ResolveProjectDirectory(const FString& ConfiguredPath, const FString& DefaultPath)
		{
			FString PathText = ConfiguredPath;
			PathText.TrimStartAndEndInline();
			if (PathText.IsEmpty())
			{
				PathText = DefaultPath;
			}

			FString ResolvedPath = FPaths::IsRelative(PathText)
				? FPaths::Combine(FPaths::ProjectDir(), PathText)
				: PathText;
			FPaths::NormalizeFilename(ResolvedPath);
			FPaths::MakeStandardFilename(ResolvedPath);
			return ResolvedPath;
		}
	}

	FString GetSourceShaderDirectory()
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Private::ResolveProjectDirectory(
			Settings ? Settings->SourceDirectory.Path : FString(),
			TEXT("DShader"));
	}

	FString GetPackageShaderDirectory()
	{
		return FPaths::Combine(GetSourceShaderDirectory(), TEXT("Packages"));
	}

	FString GetBuiltinShaderLibraryDirectory()
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Private::ResolveProjectDirectory(
			Settings ? Settings->BuiltinLibraryDirectory.Path : FString(),
			TEXT("Plugins/DreamShader/Library"));
	}

	FString GetGeneratedShaderDirectory()
	{
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		return Private::ResolveProjectDirectory(
			Settings ? Settings->GeneratedShaderDirectory.Path : FString(),
			TEXT("Intermediate/DreamShader/GeneratedShaders"));
	}

	FString GetGeneratedShaderVirtualDirectory()
	{
		return Private::GeneratedShaderVirtualDirectory;
	}

	FString SanitizeIdentifier(const FString& InText)
	{
		FString Result;
		Result.Reserve(InText.Len() + 1);

		for (TCHAR Char : InText)
		{
			if (FChar::IsAlnum(Char) || Char == TCHAR('_'))
			{
				Result.AppendChar(Char);
			}
			else
			{
				Result.AppendChar(TEXT('_'));
			}
		}

		if (Result.IsEmpty())
		{
			Result = TEXT("DreamShaderSymbol");
		}

		if (!(FChar::IsAlpha(Result[0]) || Result[0] == TCHAR('_')))
		{
			Result.InsertAt(0, TCHAR('_'));
		}

		for (int32 Index = Result.Len() - 1; Index > 0; --Index)
		{
			if (Result[Index] == TCHAR('_') && Result[Index - 1] == TCHAR('_'))
			{
				Result.RemoveAt(Index, 1, EAllowShrinking::No);
			}
		}

		return Result;
	}

	FString NormalizeSourceFilePath(const FString& InPath)
	{
		FString Result = FPaths::ConvertRelativePathToFull(InPath);
		FPaths::NormalizeFilename(Result);
		FPaths::MakeStandardFilename(Result);
		return Result;
	}

	bool IsDreamShaderMaterialFile(const FString& InPath)
	{
		return FPaths::GetExtension(InPath, true).Equals(TEXT(".dsm"), ESearchCase::IgnoreCase);
	}

	bool IsDreamShaderHeaderFile(const FString& InPath)
	{
		return FPaths::GetExtension(InPath, true).Equals(TEXT(".dsh"), ESearchCase::IgnoreCase);
	}

	bool IsDreamShaderSourceFile(const FString& InPath)
	{
		return IsDreamShaderMaterialFile(InPath) || IsDreamShaderHeaderFile(InPath);
	}
}

void FDreamShaderModule::StartupModule()
{
	IFileManager::Get().MakeDirectory(*UE::DreamShader::GetSourceShaderDirectory(), true);
	IFileManager::Get().MakeDirectory(*UE::DreamShader::GetPackageShaderDirectory(), true);
	IFileManager::Get().MakeDirectory(*UE::DreamShader::GetBuiltinShaderLibraryDirectory(), true);
	IFileManager::Get().MakeDirectory(*UE::DreamShader::GetGeneratedShaderDirectory(), true);

	const FString VirtualDirectory = UE::DreamShader::GetGeneratedShaderVirtualDirectory();
	const FString GeneratedShaderDirectory = UE::DreamShader::GetGeneratedShaderDirectory();
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualDirectory))
	{
		AddShaderSourceDirectoryMapping(VirtualDirectory, GeneratedShaderDirectory);
	}
}

void FDreamShaderModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDreamShaderModule, DreamShader);
