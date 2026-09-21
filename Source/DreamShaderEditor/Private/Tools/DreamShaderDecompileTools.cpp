// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Tools/DreamShaderDecompileTools.h"

// GetGraphDecompiler (Format = Legacy) and GetIRDecompiler (Format = Dss): the decompiler unit's two factories.
#include "Decompiler/DreamShaderGraphDecompiler.h"
#include "Decompiler/DreamShaderIRDecompiler.h"
// ResolveDreamShaderSourceProducts: which assets a source named alone stands for.
#include "DreamShaderCompilePipeline.h"
#include "DreamShaderTextWireUtils.h"
#include "Lang/LangDiagnostic.h"

#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DreamShader.EditorDecompileTools"

namespace UE::DreamShader::Editor::Private
{
	bool TryParseDreamShaderDecompileFormat(const FString& Text, ::UE::DreamShader::Editor::EDreamShaderDecompileFormat& OutFormat)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.Equals(TEXT("dss"), ESearchCase::IgnoreCase))
		{
			OutFormat = ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Dss;
			return true;
		}
		if (Trimmed.Equals(TEXT("legacy"), ESearchCase::IgnoreCase))
		{
			OutFormat = ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Legacy;
			return true;
		}
		if (Trimmed.Equals(TEXT("auto"), ESearchCase::IgnoreCase))
		{
			OutFormat = ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Auto;
			return true;
		}
		return false;
	}

	const TCHAR* LexDreamShaderDecompileFormat(const ::UE::DreamShader::Editor::EDreamShaderDecompileFormat Format)
	{
		switch (Format)
		{
		case ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Auto:
			return TEXT("auto");
		case ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Dss:
			return TEXT("dss");
		case ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Legacy:
			return TEXT("legacy");
		}
		return TEXT("auto");
	}

	::UE::DreamShader::Editor::EDreamShaderDecompileFormat ResolveDreamShaderDecompileFormat(
		const ::UE::DreamShader::Editor::EDreamShaderDecompileFormat Format,
		const FString& OutputFilePath)
	{
		if (Format != ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Auto)
		{
			return Format;
		}

		// The 1.x extensions are the only thing that still asks for the 1.x text; 2.0 is the default for everything else,
		// an instance's `.dsi` included.
		const FString Extension = FPaths::GetExtension(OutputFilePath, /*bIncludeDot*/ false);
		const bool bLegacyExtension = Extension.Equals(TEXT("dsm"), ESearchCase::IgnoreCase)
			|| Extension.Equals(TEXT("dsf"), ESearchCase::IgnoreCase);
		return bLegacyExtension
			? ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Legacy
			: ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Dss;
	}

	::UE::DreamShader::Editor::FDreamShaderDecompileResult RunDreamShaderDecompileRequest(
		const ::UE::DreamShader::Editor::FDreamShaderDecompileRequest& InRequest)
	{
		::UE::DreamShader::Editor::FDreamShaderDecompileRequest Request = InRequest;
		Request.Format = ResolveDreamShaderDecompileFormat(InRequest.Format, InRequest.OutputFilePath);

		::UE::DreamShader::Editor::IDreamShaderDecompiler& Decompiler =
			Request.Format == ::UE::DreamShader::Editor::EDreamShaderDecompileFormat::Legacy
				? GetGraphDecompiler()
				: GetIRDecompiler();

		FDreamShaderDecompileService Service(Decompiler);
		return Service.DecompileAsset(Request);
	}

	void BuildDreamShaderDecompileDiagnosticRecords(
		const ::UE::DreamShader::Editor::FDreamShaderDecompileResult& Result,
		const FString& FallbackFilePath,
		TArray<::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord>& OutRecords)
	{
		BuildDreamShaderToolDiagnosticRecords(Result.Diagnostics, FallbackFilePath, OutRecords);
	}

	void BuildDreamShaderToolDiagnosticRecords(
		const TArray<::UE::DreamShader::Lang::FLangDiagnostic>& Diagnostics,
		const FString& FallbackFilePath,
		TArray<::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord>& OutRecords)
	{
		OutRecords.Reserve(OutRecords.Num() + Diagnostics.Num());
		for (const ::UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Diagnostics)
		{
			// The same fields BuildLang2DiagnosticRecords fills from a sink; the service hands its diagnostics over as an array.
			::UE::DreamShader::Editor::Compiler::FLang2DiagnosticRecord& Lang2Record = OutRecords.AddDefaulted_GetRef();
			FDreamShaderDiagnosticRecord& Record = Lang2Record.Record;
			Record.FilePath = Diagnostic.FilePath.IsEmpty() ? FallbackFilePath : Diagnostic.FilePath;
			Record.Message = Diagnostic.Message;
			Record.Stage = ::UE::DreamShader::Editor::Compiler::Lang2StageForCode(Diagnostic.Code);
			Record.Code = Diagnostic.Code;
			Record.Line = FMath::Max(1, Diagnostic.Span.Line);
			Record.Column = FMath::Max(1, Diagnostic.Span.Column);
			Record.Severity = ::UE::DreamShader::Editor::Compiler::Lang2SeverityWireName(Diagnostic.Severity);
			Lang2Record.Length = FMath::Max(0, Diagnostic.Span.Length);
		}
	}

	FString DescribeDreamShaderDecompileFailure(const ::UE::DreamShader::Editor::FDreamShaderDecompileResult& Result)
	{
		if (!Result.Error.IsEmpty())
		{
			return Result.Error;
		}
		for (const ::UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Result.Diagnostics)
		{
			if (Diagnostic.Severity == ::UE::DreamShader::Lang::ELangSeverity::Error)
			{
				return ::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(Diagnostic, Result.OutputFilePath);
			}
		}
		return ToInvariantWireString(LOCTEXT("DecompileToolsNoReason", "The decompiler reported a failure without saying why."));
	}

	UObject* LoadDreamShaderDecompileAsset(const FString& InAssetPath, FString& OutObjectPath)
	{
		FString WrittenPath = InAssetPath.TrimStartAndEnd().TrimQuotes();
		WrittenPath.ReplaceInline(TEXT("\\"), TEXT("/"));

		// `/Game/X/M_Steel` names the asset called after its package.
		FString ObjectPath = WrittenPath;
		if (ObjectPath.StartsWith(TEXT("/")) && !ObjectPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(ObjectPath);
			if (!AssetName.IsEmpty())
			{
				ObjectPath = FString::Printf(TEXT("%s.%s"), *ObjectPath, *AssetName);
			}
		}

		OutObjectPath = ObjectPath;
		if (UObject* const Found = FindObject<UObject>(nullptr, *ObjectPath))
		{
			return Found;
		}
		if (UObject* const Loaded = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			return Loaded;
		}
		if (!WrittenPath.Equals(ObjectPath, ESearchCase::CaseSensitive))
		{
			OutObjectPath = WrittenPath;
			return LoadObject<UObject>(nullptr, *WrittenPath);
		}
		return nullptr;
	}

	UObject* LoadDreamShaderDecompileSourceProduct(const FString& SourceFilePath, FString& OutError)
	{
		::UE::DreamShader::Editor::Compiler::FDreamShaderProductResolution Resolution;
		if (!::UE::DreamShader::Editor::Compiler::ResolveDreamShaderSourceProducts(SourceFilePath, Resolution))
		{
			const ::UE::DreamShader::Lang::FLangDiagnostic* const FirstError = Resolution.Diagnostics.FirstError();
			OutError = FirstError
				? ::UE::DreamShader::Editor::Compiler::FormatLang2DiagnosticWireLine(*FirstError, SourceFilePath)
				: ToInvariantWireString(FText::Format(
					LOCTEXT("DecompileToolsSourceUnresolved", "'{0}' could not be resolved to the assets it builds."),
					FText::FromString(SourceFilePath)));
			return nullptr;
		}

		for (const ::UE::DreamShader::Editor::Compiler::FDreamShaderResolvedProduct& Product : Resolution.Products)
		{
			if (UObject* const Found = FindObject<UObject>(nullptr, *Product.ObjectPath))
			{
				return Found;
			}
			if (FPackageName::DoesPackageExist(Product.PackageName))
			{
				if (UObject* const Loaded = LoadObject<UObject>(nullptr, *Product.ObjectPath))
				{
					return Loaded;
				}
			}
		}

		OutError = ToInvariantWireString(FText::Format(
			LOCTEXT("DecompileToolsSourceNotBuilt", "Nothing '{0}' builds exists yet, so there is nothing to decompile; compile it first."),
			FText::FromString(SourceFilePath)));
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
