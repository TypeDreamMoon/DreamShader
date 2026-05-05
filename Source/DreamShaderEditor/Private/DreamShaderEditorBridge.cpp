#include "DreamShaderEditorBridge.h"

#include "DreamShaderMaterialGenerator.h"
#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "DreamShaderSettings.h"

#include "Async/Async.h"
#include "CoreGlobals.h"
#include "ContentBrowserMenuContexts.h"
#include "DirectoryWatcherModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "IDirectoryWatcher.h"
#include "IMaterialEditor.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "MaterialEditorContext.h"
#include "MaterialShared.h"
#include "MaterialValueType.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCore.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DreamShaderEditorBridge"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		static const FName DreamShaderToolMenuOwnerName(TEXT("DreamShaderEditor"));

		struct FVirtualFunctionDefinitionLocation
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
		};

		bool IsPathUnderDirectory(const FString& InPath, const FString& InDirectory)
		{
			const FString Path = UE::DreamShader::NormalizeSourceFilePath(InPath);
			FString Directory = UE::DreamShader::NormalizeSourceFilePath(InDirectory);
			Directory.RemoveFromEnd(TEXT("/"));

			return Path.Equals(Directory, ESearchCase::IgnoreCase)
				|| Path.StartsWith(Directory + TEXT("/"), ESearchCase::IgnoreCase);
		}

		bool IsPackageMaterialFile(const FString& InPath)
		{
			return UE::DreamShader::IsDreamShaderMaterialFile(InPath)
				&& IsPathUnderDirectory(InPath, UE::DreamShader::GetPackageShaderDirectory());
		}

		FString EscapeDreamShaderString(const FString& InText)
		{
			FString Result = InText;
			Result.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
			Result.ReplaceInline(TEXT("\""), TEXT("\\\""));
			return Result;
		}

		FString GetDreamShaderTypeForFunctionInput(EFunctionInputType InputType)
		{
			switch (InputType)
			{
			case FunctionInput_Scalar:
				return TEXT("float");
			case FunctionInput_Vector2:
				return TEXT("float2");
			case FunctionInput_Vector3:
				return TEXT("float3");
			case FunctionInput_Vector4:
				return TEXT("float4");
			case FunctionInput_Texture2D:
				return TEXT("Texture2D");
			case FunctionInput_TextureCube:
				return TEXT("TextureCube");
			case FunctionInput_Texture2DArray:
				return TEXT("Texture2DArray");
			case FunctionInput_StaticBool:
			case FunctionInput_Bool:
				return TEXT("bool");
			default:
				return TEXT("float4");
			}
		}

		FString GetDreamShaderTypeForMaterialValueType(EMaterialValueType ValueType)
		{
			switch (ValueType)
			{
			case MCT_Float:
			case MCT_Float1:
			case MCT_LWCScalar:
				return TEXT("float");
			case MCT_Float2:
			case MCT_LWCVector2:
				return TEXT("float2");
			case MCT_Float3:
			case MCT_LWCVector3:
				return TEXT("float3");
			case MCT_Float4:
			case MCT_LWCVector4:
				return TEXT("float4");
			case MCT_Texture2D:
				return TEXT("Texture2D");
			case MCT_TextureCube:
				return TEXT("TextureCube");
			case MCT_Texture2DArray:
				return TEXT("Texture2DArray");
			case MCT_StaticBool:
			case MCT_Bool:
				return TEXT("bool");
			default:
				return TEXT("float4");
			}
		}

		FString MakeDreamShaderDeclarationName(const FString& InName, const TCHAR* FallbackPrefix, int32 Index)
		{
			FString Result = UE::DreamShader::SanitizeIdentifier(InName.TrimStartAndEnd());
			if (Result.IsEmpty() || Result == TEXT("DreamShaderSymbol"))
			{
				Result = FString::Printf(TEXT("%s%d"), FallbackPrefix, Index + 1);
			}
			return Result;
		}

		FString MakeFunctionParameterMetadataSuffix(
			const FString& Description,
			const int32 SortPriority,
			const int32 DefaultSortPriority)
		{
			TArray<FString> MetadataEntries;
			if (!Description.TrimStartAndEnd().IsEmpty())
			{
				MetadataEntries.Add(FString::Printf(TEXT("Description=\"%s\";"), *EscapeDreamShaderString(Description.TrimStartAndEnd())));
			}
			if (SortPriority != DefaultSortPriority)
			{
				MetadataEntries.Add(FString::Printf(TEXT("SortPriority=%d;"), SortPriority));
			}

			return MetadataEntries.IsEmpty()
				? FString()
				: FString::Printf(TEXT(" [\n\t\t\t%s\n\t\t]"), *FString::Join(MetadataEntries, TEXT("\n\t\t\t")));
		}

		FString MakePreviewValueText(EFunctionInputType InputType, const FVector4f& PreviewValue)
		{
			switch (InputType)
			{
			case FunctionInput_Scalar:
				return FString::SanitizeFloat(PreviewValue.X);
			case FunctionInput_StaticBool:
			case FunctionInput_Bool:
				return PreviewValue.X != 0.0f ? TEXT("true") : TEXT("false");
			case FunctionInput_Vector2:
				return FString::Printf(TEXT("float2(%g, %g)"), PreviewValue.X, PreviewValue.Y);
			case FunctionInput_Vector3:
				return FString::Printf(TEXT("float3(%g, %g, %g)"), PreviewValue.X, PreviewValue.Y, PreviewValue.Z);
			case FunctionInput_Vector4:
				return FString::Printf(TEXT("float4(%g, %g, %g, %g)"), PreviewValue.X, PreviewValue.Y, PreviewValue.Z, PreviewValue.W);
			default:
				return FString();
			}
		}

		bool TryMakeVirtualFunctionAssetLiteral(const UMaterialFunction* MaterialFunction, FString& OutLiteral, FString& OutError)
		{
			if (!MaterialFunction)
			{
				OutError = TEXT("No MaterialFunction asset was provided.");
				return false;
			}

			FString PackageName = MaterialFunction->GetOutermost() ? MaterialFunction->GetOutermost()->GetName() : FString();
			PackageName.TrimStartAndEndInline();
			PackageName.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (PackageName.IsEmpty() || !PackageName.StartsWith(TEXT("/")))
			{
				OutError = FString::Printf(TEXT("MaterialFunction '%s' does not have a valid package path."), *MaterialFunction->GetName());
				return false;
			}

			const auto BuildLiteral = [&OutLiteral](const TCHAR* RootName, const FString& RelativePath)
			{
				OutLiteral = FString::Printf(TEXT("Path(%s, \"%s\")"), RootName, *EscapeDreamShaderString(RelativePath));
			};

			if (PackageName.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
			{
				BuildLiteral(TEXT("Game"), PackageName.Mid(6));
				return true;
			}
			if (PackageName.StartsWith(TEXT("/Engine/"), ESearchCase::IgnoreCase))
			{
				BuildLiteral(TEXT("Engine"), PackageName.Mid(8));
				return true;
			}

			FString BestPluginName;
			FString BestMountedPath;
			for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
			{
				FString MountedPath = Plugin->GetMountedAssetPath();
				MountedPath.TrimStartAndEndInline();
				MountedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
				while (MountedPath.EndsWith(TEXT("/")))
				{
					MountedPath.LeftChopInline(1, EAllowShrinking::No);
				}
				if (!MountedPath.StartsWith(TEXT("/")))
				{
					MountedPath = TEXT("/") + MountedPath;
				}
				if (MountedPath.IsEmpty() || MountedPath == TEXT("/"))
				{
					MountedPath = TEXT("/") + Plugin->GetName();
				}

				if ((PackageName.Equals(MountedPath, ESearchCase::IgnoreCase)
					|| PackageName.StartsWith(MountedPath + TEXT("/"), ESearchCase::IgnoreCase))
					&& MountedPath.Len() > BestMountedPath.Len())
				{
					BestMountedPath = MountedPath;
					BestPluginName = Plugin->GetName();
				}
			}

			if (!BestPluginName.IsEmpty())
			{
				FString RelativePath = PackageName.Mid(BestMountedPath.Len());
				while (RelativePath.StartsWith(TEXT("/")))
				{
					RelativePath.RightChopInline(1, EAllowShrinking::No);
				}
				OutLiteral = FString::Printf(
					TEXT("Path(Plugins.%s, \"%s\")"),
					*BestPluginName,
					*EscapeDreamShaderString(RelativePath));
				return true;
			}

			OutLiteral = FString::Printf(TEXT("\"%s\""), *EscapeDreamShaderString(MaterialFunction->GetPathName()));
			return true;
		}

		bool BuildVirtualFunctionDefinition(const UMaterialFunction* MaterialFunction, FString& OutDefinition, FString& OutError)
		{
			if (!MaterialFunction)
			{
				OutError = TEXT("No MaterialFunction asset was provided.");
				return false;
			}

			FString AssetLiteral;
			if (!TryMakeVirtualFunctionAssetLiteral(MaterialFunction, AssetLiteral, OutError))
			{
				return false;
			}

			TArray<FFunctionExpressionInput> Inputs;
			TArray<FFunctionExpressionOutput> Outputs;
			MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);

			if (Outputs.IsEmpty())
			{
				OutError = FString::Printf(TEXT("MaterialFunction '%s' does not expose any outputs."), *MaterialFunction->GetName());
				return false;
			}

			TArray<FString> Lines;
			Lines.Add(FString::Printf(
				TEXT("VirtualFunction(Name=\"%s\")"),
				*EscapeDreamShaderString(MakeDreamShaderDeclarationName(MaterialFunction->GetName(), TEXT("VirtualFunction"), 0))));
			Lines.Add(TEXT("{"));
			Lines.Add(TEXT("\tOptions = {"));
			Lines.Add(FString::Printf(TEXT("\t\tAsset = %s;"), *AssetLiteral));
			Lines.Add(FString::Printf(
				TEXT("\t\tDescription = \"Generated from %s\";"),
				*EscapeDreamShaderString(MaterialFunction->GetPathName())));
			Lines.Add(TEXT("\t}"));
			Lines.Add(TEXT(""));
			Lines.Add(TEXT("\tInputs = {"));
			for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
			{
				const FFunctionExpressionInput& Input = Inputs[InputIndex];
				const UMaterialExpressionFunctionInput* InputExpression = Input.ExpressionInput;
				const FString InputName = InputExpression
					? Input.ExpressionInput->InputName.ToString()
					: Input.Input.InputName.ToString();
				const EFunctionInputType InputType = InputExpression
					? InputExpression->InputType.GetValue()
					: FunctionInput_Vector4;
				const bool bOptional = InputExpression && InputExpression->bUsePreviewValueAsDefault != 0;
				const FString DefaultText = bOptional && InputExpression
					? MakePreviewValueText(InputType, InputExpression->PreviewValue)
					: FString();
				const FString DefaultSuffix = DefaultText.IsEmpty()
					? FString()
					: FString::Printf(TEXT(" = %s"), *DefaultText);
				const FString MetadataSuffix = InputExpression
					? MakeFunctionParameterMetadataSuffix(InputExpression->Description, InputExpression->SortPriority, InputIndex)
					: FString();
				Lines.Add(FString::Printf(
					TEXT("\t\t%s%s %s%s%s;"),
					bOptional ? TEXT("opt ") : TEXT(""),
					*GetDreamShaderTypeForFunctionInput(InputType),
					*MakeDreamShaderDeclarationName(InputName, TEXT("Input"), InputIndex),
					*DefaultSuffix,
					*MetadataSuffix));
			}
			Lines.Add(TEXT("\t}"));
			Lines.Add(TEXT(""));
			Lines.Add(TEXT("\tOutputs = {"));
			for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
			{
				const FFunctionExpressionOutput& Output = Outputs[OutputIndex];
				UMaterialExpressionFunctionOutput* OutputExpression = Output.ExpressionOutput;
				const FString OutputName = OutputExpression
					? OutputExpression->OutputName.ToString()
					: Output.Output.OutputName.ToString();
				const EMaterialValueType OutputType = OutputExpression
					? OutputExpression->GetInputValueType(0)
					: MCT_Float4;
				const FString MetadataSuffix = OutputExpression
					? MakeFunctionParameterMetadataSuffix(OutputExpression->Description, OutputExpression->SortPriority, OutputIndex)
					: FString();
				Lines.Add(FString::Printf(
					TEXT("\t\t%s %s%s;"),
					*GetDreamShaderTypeForMaterialValueType(OutputType),
					*MakeDreamShaderDeclarationName(OutputName, TEXT("Output"), OutputIndex),
					*MetadataSuffix));
			}
			Lines.Add(TEXT("\t}"));
			Lines.Add(TEXT("}"));

			OutDefinition = FString::Join(Lines, TEXT("\n"));
			return true;
		}

		bool BuildVirtualFunctionCallTextFromSignature(
			const FString& FunctionName,
			const TArray<FTextShaderFunctionParameter>& Inputs,
			const TArray<FTextShaderFunctionParameter>& Outputs,
			FString& OutCallText,
			FString& OutError)
		{
			if (FunctionName.TrimStartAndEnd().IsEmpty())
			{
				OutError = TEXT("VirtualFunction name cannot be empty.");
				return false;
			}

			if (Outputs.IsEmpty())
			{
				OutError = FString::Printf(TEXT("VirtualFunction '%s' does not expose any outputs."), *FunctionName);
				return false;
			}

			TArray<FString> Arguments;
			for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
			{
				Arguments.Add(Inputs[InputIndex].bOptional
					? TEXT("default")
					: MakeDreamShaderDeclarationName(Inputs[InputIndex].Name, TEXT("Input"), InputIndex));
			}

			Arguments.Add(FString::Printf(
				TEXT("Output=\"%s\""),
				*EscapeDreamShaderString(MakeDreamShaderDeclarationName(Outputs[0].Name, TEXT("Output"), 0))));

			OutCallText = FString::Printf(
				TEXT("%s(%s)"),
				*MakeDreamShaderDeclarationName(FunctionName, TEXT("VirtualFunction"), 0),
				*FString::Join(Arguments, TEXT(", ")));
			return true;
		}

		bool BuildVirtualFunctionCallText(const UMaterialFunction* MaterialFunction, FString& OutCallText, FString& OutError)
		{
			if (!MaterialFunction)
			{
				OutError = TEXT("No MaterialFunction asset was provided.");
				return false;
			}

			TArray<FFunctionExpressionInput> FunctionInputs;
			TArray<FFunctionExpressionOutput> FunctionOutputs;
			MaterialFunction->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);

			TArray<FTextShaderFunctionParameter> Inputs;
			for (int32 InputIndex = 0; InputIndex < FunctionInputs.Num(); ++InputIndex)
			{
				const FFunctionExpressionInput& Input = FunctionInputs[InputIndex];
				const FString InputName = Input.ExpressionInput
					? Input.ExpressionInput->InputName.ToString()
					: Input.Input.InputName.ToString();
				FTextShaderFunctionParameter& Parameter = Inputs.AddDefaulted_GetRef();
				Parameter.Name = InputName;
				Parameter.bOptional = Input.ExpressionInput && Input.ExpressionInput->bUsePreviewValueAsDefault != 0;
			}

			TArray<FTextShaderFunctionParameter> Outputs;
			for (int32 OutputIndex = 0; OutputIndex < FunctionOutputs.Num(); ++OutputIndex)
			{
				const FFunctionExpressionOutput& Output = FunctionOutputs[OutputIndex];
				const FString OutputName = Output.ExpressionOutput
					? Output.ExpressionOutput->OutputName.ToString()
					: Output.Output.OutputName.ToString();
				FTextShaderFunctionParameter& Parameter = Outputs.AddDefaulted_GetRef();
				Parameter.Name = OutputName;
			}

			return BuildVirtualFunctionCallTextFromSignature(
				MaterialFunction->GetName(),
				Inputs,
				Outputs,
				OutCallText,
				OutError);
		}

		FString MakeVirtualFunctionDefinitionFilePath(const UMaterialFunction* MaterialFunction)
		{
			const FString DefinitionDirectory = FPaths::Combine(
				UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("VirtualFunctions"));
			const FString BaseName = MakeDreamShaderDeclarationName(
				MaterialFunction ? MaterialFunction->GetName() : FString(),
				TEXT("VirtualFunction"),
				0);

			const FString PreferredCandidate = FPaths::Combine(DefinitionDirectory, BaseName + TEXT(".dsh"));
			if (!IFileManager::Get().FileExists(*PreferredCandidate) || !MaterialFunction)
			{
				return UE::DreamShader::NormalizeSourceFilePath(PreferredCandidate);
			}

			const uint32 AssetPathHash = FCrc::StrCrc32(*MaterialFunction->GetPathName());
			return UE::DreamShader::NormalizeSourceFilePath(FPaths::Combine(
				DefinitionDirectory,
				FString::Printf(TEXT("%s_%08x.dsh"), *BaseName, AssetPathHash)));
		}

		bool IsIdentifierCharacter(TCHAR Character)
		{
			return FChar::IsAlnum(Character) || Character == TCHAR('_');
		}

		bool IsValidStringIndex(const FString& Text, int32 Index)
		{
			return Index >= 0 && Index < Text.Len();
		}

		bool IsKeywordAt(const FString& Text, int32 Index, const TCHAR* Keyword)
		{
			const int32 KeywordLength = FCString::Strlen(Keyword);
			if (Index < 0 || Index + KeywordLength > Text.Len())
			{
				return false;
			}

			if (Index > 0 && IsIdentifierCharacter(Text[Index - 1]))
			{
				return false;
			}

			if (Index + KeywordLength < Text.Len() && IsIdentifierCharacter(Text[Index + KeywordLength]))
			{
				return false;
			}

			return Text.Mid(Index, KeywordLength).Equals(Keyword, ESearchCase::CaseSensitive);
		}

		void SkipQuotedString(const FString& Text, int32& Index)
		{
			if (!IsValidStringIndex(Text, Index) || (Text[Index] != TCHAR('"') && Text[Index] != TCHAR('\'')))
			{
				return;
			}

			const TCHAR Quote = Text[Index];
			++Index;
			bool bEscaped = false;
			while (Index < Text.Len())
			{
				const TCHAR Character = Text[Index++];
				if (bEscaped)
				{
					bEscaped = false;
					continue;
				}
				if (Character == TCHAR('\\'))
				{
					bEscaped = true;
					continue;
				}
				if (Character == Quote)
				{
					return;
				}
			}
		}

		bool TrySkipComment(const FString& Text, int32& Index)
		{
			if (Index + 1 >= Text.Len() || Text[Index] != TCHAR('/'))
			{
				return false;
			}

			if (Text[Index + 1] == TCHAR('/'))
			{
				Index += 2;
				while (Index < Text.Len() && Text[Index] != TCHAR('\n'))
				{
					++Index;
				}
				return true;
			}

			if (Text[Index + 1] == TCHAR('*'))
			{
				Index += 2;
				while (Index + 1 < Text.Len())
				{
					if (Text[Index] == TCHAR('*') && Text[Index + 1] == TCHAR('/'))
					{
						Index += 2;
						return true;
					}
					++Index;
				}
				Index = Text.Len();
				return true;
			}

			return false;
		}

		void SkipIgnoredText(const FString& Text, int32& Index)
		{
			while (Index < Text.Len())
			{
				if (FChar::IsWhitespace(Text[Index]))
				{
					++Index;
					continue;
				}
				if (TrySkipComment(Text, Index))
				{
					continue;
				}
				return;
			}
		}

		bool TryExtractBalancedRange(const FString& Text, int32 OpenIndex, TCHAR OpenCharacter, TCHAR CloseCharacter, int32& OutEndIndex)
		{
			OutEndIndex = INDEX_NONE;
			if (!IsValidStringIndex(Text, OpenIndex) || Text[OpenIndex] != OpenCharacter)
			{
				return false;
			}

			int32 Depth = 0;
			int32 Index = OpenIndex;
			while (Index < Text.Len())
			{
				if (Text[Index] == TCHAR('"') || Text[Index] == TCHAR('\''))
				{
					SkipQuotedString(Text, Index);
					continue;
				}
				if (TrySkipComment(Text, Index))
				{
					continue;
				}

				if (Text[Index] == OpenCharacter)
				{
					++Depth;
				}
				else if (Text[Index] == CloseCharacter)
				{
					--Depth;
					if (Depth == 0)
					{
						OutEndIndex = Index + 1;
						return true;
					}
				}
				++Index;
			}

			return false;
		}

		void CalculateLineColumnForIndex(const FString& Text, int32 Position, int32& OutLine, int32& OutColumn)
		{
			OutLine = 1;
			OutColumn = 1;
			const int32 ClampedPosition = FMath::Clamp(Position, 0, Text.Len());
			for (int32 Index = 0; Index < ClampedPosition; ++Index)
			{
				if (Text[Index] == TCHAR('\n'))
				{
					++OutLine;
					OutColumn = 1;
				}
				else if (Text[Index] != TCHAR('\r'))
				{
					++OutColumn;
				}
			}
		}

		FString NormalizeVirtualFunctionDefinitionText(FString Text)
		{
			Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
			Text.ReplaceInline(TEXT("\r"), TEXT("\n"));
			Text.TrimStartAndEndInline();
			return Text;
		}

		FDreamShaderEditorBridge::FDiagnosticRecord MakeVirtualFunctionDiagnostic(
			const FString& SourceFilePath,
			const FString& Message,
			const FString& Detail,
			const FString& AssetPath,
			int32 Line,
			int32 Column)
		{
			FDreamShaderEditorBridge::FDiagnosticRecord Diagnostic;
			Diagnostic.FilePath = SourceFilePath;
			Diagnostic.Message = Message;
			Diagnostic.Detail = Detail;
			Diagnostic.Stage = TEXT("virtualFunctionSync");
			Diagnostic.AssetPath = AssetPath;
			Diagnostic.Code = TEXT("virtual-function-sync");
			Diagnostic.Line = FMath::Max(1, Line);
			Diagnostic.Column = FMath::Max(1, Column);
			Diagnostic.Source = TEXT("DreamShader VirtualFunction");
			return Diagnostic;
		}

		void FindProjectDreamShaderSourceFiles(TArray<FString>& OutSourceFiles)
		{
			TArray<FString> MaterialFiles;
			TArray<FString> HeaderFiles;
			IFileManager::Get().FindFilesRecursive(
				MaterialFiles,
				*UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("*.dsm"),
				true,
				false,
				false);
			IFileManager::Get().FindFilesRecursive(
				HeaderFiles,
				*UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("*.dsh"),
				true,
				false,
				false);

			OutSourceFiles.Reset();
			OutSourceFiles.Append(MaterialFiles);
			OutSourceFiles.Append(HeaderFiles);

			for (FString& SourceFile : OutSourceFiles)
			{
				SourceFile = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			}

			OutSourceFiles.RemoveAll([](const FString& SourceFile)
			{
				return IsPathUnderDirectory(SourceFile, UE::DreamShader::GetPackageShaderDirectory());
			});
			OutSourceFiles.Sort();
		}

		bool TryParseVirtualFunctionBlock(
			const FString& BlockText,
			FTextShaderVirtualFunctionDefinition& OutFunction,
			FString& OutError)
		{
			FTextShaderDefinition ParsedDefinition;
			if (!FTextShaderParser::Parse(BlockText, ParsedDefinition, OutError))
			{
				return false;
			}

			if (ParsedDefinition.VirtualFunctions.Num() != 1)
			{
				OutError = TEXT("Expected exactly one VirtualFunction block.");
				return false;
			}

			OutFunction = ParsedDefinition.VirtualFunctions[0];
			return true;
		}

		void CollectVirtualFunctionDefinitionLocationsFromFile(
			const FString& SourceFilePath,
			TArray<FVirtualFunctionDefinitionLocation>& OutLocations,
			FString* OutSourceText = nullptr,
			TArray<FDreamShaderEditorBridge::FDiagnosticRecord>* OutDiagnostics = nullptr)
		{
			OutLocations.Reset();

			FString SourceText;
			if (!FFileHelper::LoadFileToString(SourceText, *SourceFilePath))
			{
				if (OutDiagnostics)
				{
					OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
						SourceFilePath,
						FString::Printf(TEXT("DreamShader could not read VirtualFunction source file '%s'."), *SourceFilePath),
						FString(),
						FString(),
						1,
						1));
				}
				return;
			}

			if (OutSourceText)
			{
				*OutSourceText = SourceText;
			}

			int32 Index = 0;
			while (Index < SourceText.Len())
			{
				if (SourceText[Index] == TCHAR('"') || SourceText[Index] == TCHAR('\''))
				{
					SkipQuotedString(SourceText, Index);
					continue;
				}
				if (TrySkipComment(SourceText, Index))
				{
					continue;
				}
				if (!IsKeywordAt(SourceText, Index, TEXT("VirtualFunction")))
				{
					++Index;
					continue;
				}

				const int32 StartIndex = Index;
				Index += FCString::Strlen(TEXT("VirtualFunction"));
				SkipIgnoredText(SourceText, Index);
				if (!IsValidStringIndex(SourceText, Index) || SourceText[Index] != TCHAR('('))
				{
					Index = StartIndex + 1;
					continue;
				}

				int32 AttributesEndIndex = INDEX_NONE;
				if (!TryExtractBalancedRange(SourceText, Index, TCHAR('('), TCHAR(')'), AttributesEndIndex))
				{
					int32 Line = 1;
					int32 Column = 1;
					CalculateLineColumnForIndex(SourceText, StartIndex, Line, Column);
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							TEXT("VirtualFunction attributes are missing a closing ')'."),
							FString(),
							FString(),
							Line,
							Column));
					}
					Index = StartIndex + 1;
					continue;
				}

				Index = AttributesEndIndex;
				SkipIgnoredText(SourceText, Index);
				if (!IsValidStringIndex(SourceText, Index) || SourceText[Index] != TCHAR('{'))
				{
					Index = StartIndex + 1;
					continue;
				}

				int32 BodyEndIndex = INDEX_NONE;
				if (!TryExtractBalancedRange(SourceText, Index, TCHAR('{'), TCHAR('}'), BodyEndIndex))
				{
					int32 Line = 1;
					int32 Column = 1;
					CalculateLineColumnForIndex(SourceText, StartIndex, Line, Column);
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							TEXT("VirtualFunction body is missing a closing '}'."),
							FString(),
							FString(),
							Line,
							Column));
					}
					Index = StartIndex + 1;
					continue;
				}

				int32 EndIndex = BodyEndIndex;
				int32 AfterBodyIndex = EndIndex;
				SkipIgnoredText(SourceText, AfterBodyIndex);
				if (IsValidStringIndex(SourceText, AfterBodyIndex) && SourceText[AfterBodyIndex] == TCHAR(';'))
				{
					EndIndex = AfterBodyIndex + 1;
				}

				const FString BlockText = SourceText.Mid(StartIndex, EndIndex - StartIndex);
				int32 Line = 1;
				int32 Column = 1;
				CalculateLineColumnForIndex(SourceText, StartIndex, Line, Column);

				FTextShaderVirtualFunctionDefinition ParsedFunction;
				FString ParseError;
				if (!TryParseVirtualFunctionBlock(BlockText, ParsedFunction, ParseError))
				{
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							FString::Printf(TEXT("VirtualFunction declaration is invalid: %s"), *ParseError),
							ParseError,
							FString(),
							Line,
							Column));
					}
					Index = EndIndex;
					continue;
				}

				FString ObjectPath;
				FString ResolveError;
				if (!TryResolveDreamShaderAssetReference(ParsedFunction.Asset, ObjectPath, ResolveError))
				{
					if (OutDiagnostics)
					{
						OutDiagnostics->Add(MakeVirtualFunctionDiagnostic(
							SourceFilePath,
							FString::Printf(TEXT("VirtualFunction '%s' asset reference is invalid: %s"), *ParsedFunction.Name, *ResolveError),
							ResolveError,
							ParsedFunction.Asset,
							Line,
							Column));
					}
					Index = EndIndex;
					continue;
				}

				FVirtualFunctionDefinitionLocation& Location = OutLocations.AddDefaulted_GetRef();
				Location.SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
				Location.FunctionName = ParsedFunction.Name;
				Location.AssetObjectPath = ObjectPath;
				Location.CurrentText = BlockText;
				Location.Inputs = ParsedFunction.Inputs;
				Location.Outputs = ParsedFunction.Outputs;
				Location.StartIndex = StartIndex;
				Location.EndIndex = EndIndex;
				Location.Line = Line;
				Location.Column = Column;

				Index = EndIndex;
			}
		}

		bool FindVirtualFunctionDefinitionForMaterialFunction(
			const UMaterialFunction* MaterialFunction,
			FVirtualFunctionDefinitionLocation& OutLocation)
		{
			if (!MaterialFunction)
			{
				return false;
			}

			const FString TargetObjectPath = MaterialFunction->GetPathName();
			TArray<FString> SourceFiles;
			FindProjectDreamShaderSourceFiles(SourceFiles);
			for (const FString& SourceFile : SourceFiles)
			{
				TArray<FVirtualFunctionDefinitionLocation> Locations;
				CollectVirtualFunctionDefinitionLocationsFromFile(SourceFile, Locations);
				for (const FVirtualFunctionDefinitionLocation& Location : Locations)
				{
					if (Location.AssetObjectPath.Equals(TargetObjectPath, ESearchCase::IgnoreCase))
					{
						OutLocation = Location;
						return true;
					}
				}
			}

			return false;
		}

		FString QuoteProcessArgument(const FString& Argument)
		{
			FString Escaped = Argument;
			Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
			return FString::Printf(TEXT("\"%s\""), *Escaped);
		}

		bool WriteDreamShaderWorkspaceFile(FString& OutWorkspaceFilePath, FString& OutError)
		{
			const FString SourceDirectory = UE::DreamShader::NormalizeSourceFilePath(UE::DreamShader::GetSourceShaderDirectory());
			if (SourceDirectory.IsEmpty())
			{
				OutError = TEXT("DreamShader source directory is empty.");
				return false;
			}

			if (!IFileManager::Get().MakeDirectory(*SourceDirectory, true))
			{
				OutError = FString::Printf(TEXT("Failed to create DreamShader source directory '%s'."), *SourceDirectory);
				return false;
			}

			const FString WorkspaceFilePath = UE::DreamShader::NormalizeSourceFilePath(
				FPaths::Combine(SourceDirectory, TEXT("DreamShader.code-workspace")));

			FString WorkspaceText;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&WorkspaceText);
			Writer->WriteObjectStart();
			Writer->WriteArrayStart(TEXT("folders"));

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), TEXT("DreamShader Source"));
			Writer->WriteValue(TEXT("path"), TEXT("."));
			Writer->WriteObjectEnd();

			Writer->WriteArrayEnd();
			Writer->WriteObjectStart(TEXT("settings"));
			Writer->WriteObjectStart(TEXT("files.associations"));
			Writer->WriteValue(TEXT("*.dsm"), TEXT("dreamshaderlang"));
			Writer->WriteValue(TEXT("*.dsh"), TEXT("dreamshaderlang"));
			Writer->WriteObjectEnd();
			Writer->WriteObjectEnd();
			Writer->WriteObjectEnd();
			Writer->Close();

			if (!FFileHelper::SaveStringToFile(WorkspaceText, *WorkspaceFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				OutError = FString::Printf(TEXT("Failed to write DreamShader workspace file '%s'."), *WorkspaceFilePath);
				return false;
			}

			OutWorkspaceFilePath = WorkspaceFilePath;
			return true;
		}

		void AddExistingFileCandidate(TArray<FString>& OutCandidates, const FString& Candidate)
		{
			if (!Candidate.IsEmpty() && FPaths::FileExists(Candidate))
			{
				OutCandidates.AddUnique(UE::DreamShader::NormalizeSourceFilePath(Candidate));
			}
		}

		TArray<FString> FindVSCodeExecutableCandidates()
		{
			TArray<FString> Candidates;

			auto AddFromEnvironmentDirectory = [&Candidates](const TCHAR* VariableName, const TCHAR* RelativePath)
			{
				const FString Directory = FPlatformMisc::GetEnvironmentVariable(VariableName);
				if (!Directory.IsEmpty())
				{
					AddExistingFileCandidate(Candidates, FPaths::Combine(Directory, RelativePath));
				}
			};

			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code/Code.exe"));
			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code/bin/code.cmd"));
			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code Insiders/Code - Insiders.exe"));
			AddFromEnvironmentDirectory(TEXT("LOCALAPPDATA"), TEXT("Programs/Microsoft VS Code Insiders/bin/code-insiders.cmd"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles"), TEXT("Microsoft VS Code/Code.exe"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles"), TEXT("Microsoft VS Code/bin/code.cmd"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles(x86)"), TEXT("Microsoft VS Code/Code.exe"));
			AddFromEnvironmentDirectory(TEXT("ProgramFiles(x86)"), TEXT("Microsoft VS Code/bin/code.cmd"));

			const FString PathEnvironment = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
			TArray<FString> PathEntries;
			PathEnvironment.ParseIntoArray(PathEntries, TEXT(";"), true);
			for (FString PathEntry : PathEntries)
			{
				PathEntry.TrimStartAndEndInline();
				if (PathEntry.IsEmpty())
				{
					continue;
				}

				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("code.cmd")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("code.exe")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("Code.exe")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("code-insiders.cmd")));
				AddExistingFileCandidate(Candidates, FPaths::Combine(PathEntry, TEXT("Code - Insiders.exe")));
			}

			return Candidates;
		}

		bool LaunchVSCodeWorkspace(const FString& WorkspaceFilePath)
		{
			for (const FString& Candidate : FindVSCodeExecutableCandidates())
			{
				const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
				
				FProcHandle ProcessHandle;
				if (Candidate.EndsWith(TEXT(".cmd"), ESearchCase::IgnoreCase)
					|| Candidate.EndsWith(TEXT(".bat"), ESearchCase::IgnoreCase))
				{
					FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
					if (CmdExe.IsEmpty())
					{
						CmdExe = TEXT("C:/Windows/System32/cmd.exe");
					}
					
					FString Parameters = FString::Printf(
						TEXT("/C \"\"%s\" %s %s\""),
						*Candidate,
						((Settings && !Settings->bOpenInNewWindow) ? TEXT(" --reuse-window") : TEXT("")),
						*QuoteProcessArgument(WorkspaceFilePath));
					ProcessHandle = FPlatformProcess::CreateProc(*CmdExe, *Parameters, true, true, true, nullptr, 0, nullptr, nullptr);
				}
				else
				{
					const FString Parameters = FString::Printf(TEXT("%s %s"), 
					((Settings && !Settings->bOpenInNewWindow) ? TEXT(" --reuse-window") : TEXT("")),
					*QuoteProcessArgument(WorkspaceFilePath));
					ProcessHandle = FPlatformProcess::CreateProc(*Candidate, *Parameters, true, false, false, nullptr, 0, nullptr, nullptr);
				}

				if (ProcessHandle.IsValid())
				{
					FPlatformProcess::CloseProc(ProcessHandle);
					return true;
				}
			}

			return false;
		}

		bool LaunchVSCodeFile(const FString& FilePath, int32 Line = 1, int32 Column = 1)
		{
			const FString GotoArgument = FString::Printf(
				TEXT("%s:%d:%d"),
				*FilePath,
				FMath::Max(1, Line),
				FMath::Max(1, Column));

			for (const FString& Candidate : FindVSCodeExecutableCandidates())
			{
				FProcHandle ProcessHandle;
				if (Candidate.EndsWith(TEXT(".cmd"), ESearchCase::IgnoreCase)
					|| Candidate.EndsWith(TEXT(".bat"), ESearchCase::IgnoreCase))
				{
					FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
					if (CmdExe.IsEmpty())
					{
						CmdExe = TEXT("C:/Windows/System32/cmd.exe");
					}

					const FString Parameters = FString::Printf(
						TEXT("/C \"\"%s\" --reuse-window -g %s\""),
						*Candidate,
						*QuoteProcessArgument(GotoArgument));
					ProcessHandle = FPlatformProcess::CreateProc(*CmdExe, *Parameters, true, true, true, nullptr, 0, nullptr, nullptr);
				}
				else
				{
					const FString Parameters = FString::Printf(TEXT("--reuse-window -g %s"), *QuoteProcessArgument(GotoArgument));
					ProcessHandle = FPlatformProcess::CreateProc(*Candidate, *Parameters, true, false, false, nullptr, 0, nullptr, nullptr);
				}

				if (ProcessHandle.IsValid())
				{
					FPlatformProcess::CloseProc(ProcessHandle);
					return true;
				}
			}

			return false;
		}

		bool LaunchTextFileWithNotepad(const FString& FilePath)
		{
			TArray<FString> Candidates;
			const FString SystemRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot"));
			AddExistingFileCandidate(Candidates, FPaths::Combine(SystemRoot, TEXT("System32/notepad.exe")));
			Candidates.Add(TEXT("notepad.exe"));

			for (const FString& Candidate : Candidates)
			{
				const FString Parameters = QuoteProcessArgument(FilePath);
				FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*Candidate, *Parameters, true, false, false, nullptr, 0, nullptr, nullptr);
				if (ProcessHandle.IsValid())
				{
					FPlatformProcess::CloseProc(ProcessHandle);
					return true;
				}
			}

			return false;
		}

		bool LaunchTextFileInPreferredEditor(const FString& FilePath, int32 Line = 1, int32 Column = 1)
		{
			if (LaunchVSCodeFile(FilePath, Line, Column))
			{
				return true;
			}
			if (FPlatformProcess::LaunchFileInDefaultExternalApplication(*FilePath, nullptr, ELaunchVerb::Edit, false))
			{
				return true;
			}
			return LaunchTextFileWithNotepad(FilePath);
		}

		void ShowDreamShaderNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
		{
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 4.0f;
			Info.bUseLargeFont = false;
			if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
			{
				Notification->SetCompletionState(CompletionState);
			}
		}

		bool TryExtractImportPathFromLine(const FString& Line, FString& OutPath)
		{
			FString TrimmedLine = Line.TrimStartAndEnd();
			if (TrimmedLine.StartsWith(TEXT("//"))
				|| !TrimmedLine.StartsWith(TEXT("import"), ESearchCase::CaseSensitive))
			{
				return false;
			}

			TrimmedLine.RightChopInline(6, EAllowShrinking::No);
			TrimmedLine.TrimStartInline();
			if (TrimmedLine.Len() < 2 || (TrimmedLine[0] != TCHAR('"') && TrimmedLine[0] != TCHAR('\'')))
			{
				return false;
			}

			const TCHAR Quote = TrimmedLine[0];
			int32 ClosingQuoteIndex = INDEX_NONE;
			for (int32 Index = 1; Index < TrimmedLine.Len(); ++Index)
			{
				if (TrimmedLine[Index] == Quote)
				{
					ClosingQuoteIndex = Index;
					break;
				}
			}

			if (ClosingQuoteIndex == INDEX_NONE)
			{
				return false;
			}

			OutPath = TrimmedLine.Mid(1, ClosingQuoteIndex - 1).TrimStartAndEnd();
			return !OutPath.IsEmpty();
		}

		FString NormalizeDreamShaderImportSpecifier(const FString& ImportSpecifier)
		{
			FString Normalized = ImportSpecifier.TrimStartAndEnd();
			Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (Normalized.StartsWith(TEXT("./")))
			{
				Normalized.RightChopInline(2, EAllowShrinking::No);
			}

			if (FPaths::GetExtension(Normalized, true).IsEmpty())
			{
				Normalized += TEXT(".dsh");
			}

			return Normalized;
		}

		bool ResolveDreamShaderImportPath(const FString& CurrentFilePath, const FString& ImportSpecifier, FString& OutResolvedPath)
		{
			const FString NormalizedImport = NormalizeDreamShaderImportSpecifier(ImportSpecifier);
			if (NormalizedImport.IsEmpty())
			{
				return false;
			}

			const TArray<FString> Candidates =
			{
				FPaths::Combine(FPaths::GetPath(CurrentFilePath), NormalizedImport),
				FPaths::Combine(UE::DreamShader::GetSourceShaderDirectory(), NormalizedImport),
				FPaths::Combine(UE::DreamShader::GetPackageShaderDirectory(), NormalizedImport),
				FPaths::Combine(UE::DreamShader::GetBuiltinShaderLibraryDirectory(), NormalizedImport)
			};

			for (const FString& Candidate : Candidates)
			{
				const FString NormalizedCandidate = UE::DreamShader::NormalizeSourceFilePath(Candidate);
				if (IFileManager::Get().FileExists(*NormalizedCandidate))
				{
					OutResolvedPath = NormalizedCandidate;
					return true;
				}
			}

			return false;
		}

		void FindProjectMaterialSourceFiles(TArray<FString>& OutSourceFiles)
		{
			IFileManager::Get().FindFilesRecursive(
				OutSourceFiles,
				*UE::DreamShader::GetSourceShaderDirectory(),
				TEXT("*.dsm"),
				true,
				false,
				false);

			for (FString& SourceFile : OutSourceFiles)
			{
				SourceFile = UE::DreamShader::NormalizeSourceFilePath(SourceFile);
			}

			OutSourceFiles.RemoveAll([](const FString& SourceFile)
			{
				return IsPackageMaterialFile(SourceFile);
			});
		}

		void CollectHeaderDependenciesRecursive(
			const FString& SourceFilePath,
			TSet<FString>& OutHeaders,
			TSet<FString>& InOutVisitedFiles)
		{
			const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
			if (InOutVisitedFiles.Contains(NormalizedPath))
			{
				return;
			}
			InOutVisitedFiles.Add(NormalizedPath);

			FString SourceText;
			if (!FFileHelper::LoadFileToString(SourceText, *NormalizedPath))
			{
				return;
			}

			TArray<FString> Lines;
			SourceText.ParseIntoArrayLines(Lines, false);
			for (const FString& Line : Lines)
			{
				FString ImportPath;
				if (!TryExtractImportPathFromLine(Line, ImportPath))
				{
					continue;
				}

				FString ResolvedImportPath;
				if (!ResolveDreamShaderImportPath(NormalizedPath, ImportPath, ResolvedImportPath))
				{
					continue;
				}

				if (UE::DreamShader::IsDreamShaderHeaderFile(ResolvedImportPath))
				{
					OutHeaders.Add(ResolvedImportPath);
				}

				CollectHeaderDependenciesRecursive(ResolvedImportPath, OutHeaders, InOutVisitedFiles);
			}
		}

		bool TryParseErrorLocation(
			const FString& Line,
			FString& OutFilePath,
			int32& OutLine,
			int32& OutColumn,
			FString& OutMessage)
		{
			const int32 CloseMarkerIndex = Line.Find(TEXT("): "));
			if (CloseMarkerIndex == INDEX_NONE)
			{
				return false;
			}

			const int32 OpenMarkerIndex = Line.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromEnd, CloseMarkerIndex);
			if (OpenMarkerIndex == INDEX_NONE || OpenMarkerIndex >= CloseMarkerIndex)
			{
				return false;
			}

			const FString LocationText = Line.Mid(OpenMarkerIndex + 1, CloseMarkerIndex - OpenMarkerIndex - 1);
			FString LineText;
			FString ColumnText;
			if (!LocationText.Split(TEXT(","), &LineText, &ColumnText))
			{
				return false;
			}

			LineText.TrimStartAndEndInline();
			ColumnText.TrimStartAndEndInline();
			if (!LineText.IsNumeric() || !ColumnText.IsNumeric())
			{
				return false;
			}

			OutLine = FMath::Max(1, FCString::Atoi(*LineText));
			OutColumn = FMath::Max(1, FCString::Atoi(*ColumnText));

			OutFilePath = UE::DreamShader::NormalizeSourceFilePath(Line.Left(OpenMarkerIndex));
			OutMessage = Line.Mid(CloseMarkerIndex + 3).TrimStartAndEnd();
			return !OutFilePath.IsEmpty() && !OutMessage.IsEmpty();
		}

		void AddDiagnosticJson(TArray<TSharedPtr<FJsonValue>>& OutDiagnostics, const FDreamShaderEditorBridge::FDiagnosticRecord& Diagnostic)
		{
			TSharedRef<FJsonObject> DiagnosticObject = MakeShared<FJsonObject>();
			DiagnosticObject->SetStringField(TEXT("message"), Diagnostic.Message);
			if (!Diagnostic.Detail.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("detail"), Diagnostic.Detail);
			}
			if (!Diagnostic.Stage.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("stage"), Diagnostic.Stage);
			}
			if (!Diagnostic.AssetPath.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("assetPath"), Diagnostic.AssetPath);
			}
			if (!Diagnostic.ShaderPlatform.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("shaderPlatform"), Diagnostic.ShaderPlatform);
			}
			if (!Diagnostic.QualityLevel.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("qualityLevel"), Diagnostic.QualityLevel);
			}
			if (!Diagnostic.Code.IsEmpty())
			{
				DiagnosticObject->SetStringField(TEXT("code"), Diagnostic.Code);
			}
			DiagnosticObject->SetNumberField(TEXT("line"), Diagnostic.Line);
			DiagnosticObject->SetNumberField(TEXT("column"), Diagnostic.Column);
			DiagnosticObject->SetStringField(TEXT("severity"), Diagnostic.Severity);
			DiagnosticObject->SetStringField(TEXT("source"), Diagnostic.Source);
			OutDiagnostics.Add(MakeShared<FJsonValueObject>(DiagnosticObject));
		}

		FString GetShaderPlatformLabel(const EShaderPlatform ShaderPlatform)
		{
			const FName ShaderFormat = LegacyShaderPlatformToShaderFormat(ShaderPlatform);
			return ShaderFormat.IsNone()
				? FString::Printf(TEXT("Platform %d"), static_cast<int32>(ShaderPlatform))
				: ShaderFormat.ToString();
		}

		FString GetMaterialQualityLevelLabel(const EMaterialQualityLevel::Type QualityLevel)
		{
			return LexToString(QualityLevel);
		}

		FString GetFirstMeaningfulErrorLine(const FString& InError)
		{
			TArray<FString> Lines;
			InError.ParseIntoArrayLines(Lines, false);
			for (const FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					return Trimmed;
				}
			}
			return InError.TrimStartAndEnd();
		}
	}

	FString FDreamShaderEditorBridge::GetBridgeDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Bridge"));
	}

	FString FDreamShaderEditorBridge::GetRequestDirectory()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("Requests"));
	}

	FString FDreamShaderEditorBridge::GetDiagnosticsFilePath()
	{
		return FPaths::Combine(GetBridgeDirectory(), TEXT("diagnostics.json"));
	}

	FString FDreamShaderEditorBridge::GetSourceFileMetadata(UObject* Asset)
	{
		if (!Asset)
		{
			return FString();
		}

		if (UPackage* Package = Asset->GetOutermost())
		{
			return Package->GetMetaData().GetValue(Asset, TEXT("DreamShader.SourceFile"));
		}

		return FString();
	}

	void FDreamShaderEditorBridge::Startup()
	{
		bIsShuttingDown = false;

		IFileManager::Get().MakeDirectory(*GetBridgeDirectory(), true);
		IFileManager::Get().MakeDirectory(*GetRequestDirectory(), true);

		SyncVirtualFunctionDefinitions();
		QueueFullScan();
		UpdateDiagnosticsFile();

		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
		{
			WatchedSourceDirectory = UE::DreamShader::GetSourceShaderDirectory();
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				WatchedSourceDirectory,
				IDirectoryWatcher::FDirectoryChanged::CreateSP(AsShared(), &FDreamShaderEditorBridge::OnDirectoryChanged),
				DirectoryWatcherHandle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
		}

		MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddSP(
			AsShared(),
			&FDreamShaderEditorBridge::OnMaterialCompilationFinished);

		ToolMenusStartupCallbackHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::RegisterMenus));

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::Tick),
			0.1f);
	}

	void FDreamShaderEditorBridge::Shutdown()
	{
		bIsShuttingDown = true;

		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}

		if (MaterialCompilationFinishedHandle.IsValid())
		{
			UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
			MaterialCompilationFinishedHandle.Reset();
		}

		if (ToolMenusStartupCallbackHandle.IsValid())
		{
			UToolMenus::UnRegisterStartupCallback(ToolMenusStartupCallbackHandle);
			ToolMenusStartupCallbackHandle.Reset();
		}

		if (!IsEngineExitRequested() && !GExitPurge)
		{
			UToolMenus::UnregisterOwner(DreamShaderToolMenuOwnerName);
		}

		if (DirectoryWatcherHandle.IsValid())
		{
			if (FDirectoryWatcherModule* DirectoryWatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
			{
				if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule->Get())
				{
					DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(WatchedSourceDirectory, DirectoryWatcherHandle);
				}
			}

			DirectoryWatcherHandle.Reset();
			WatchedSourceDirectory.Reset();
		}

		PendingFiles.Reset();
		DiagnosticsByFile.Reset();
	}

	void FDreamShaderEditorBridge::QueueFullScan()
	{
		TArray<FString> SourceFiles;
		FindProjectMaterialSourceFiles(SourceFiles);
		RebuildDependencyGraph();

		const double Now = FPlatformTime::Seconds();
		for (FString& SourceFile : SourceFiles)
		{
			PendingFiles.Add(UE::DreamShader::NormalizeSourceFilePath(SourceFile), Now);
		}
	}

	void FDreamShaderEditorBridge::QueueSourceFile(const FString& SourceFilePath)
	{
		PendingFiles.Add(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath), FPlatformTime::Seconds());
	}

	void FDreamShaderEditorBridge::QueueDependentMaterialsForHeader(const FString& HeaderFilePath)
	{
		const FString NormalizedHeaderPath = UE::DreamShader::NormalizeSourceFilePath(HeaderFilePath);
		TSet<FString> MaterialsToQueue;

		if (const TSet<FString>* ExistingDependents = HeaderDependentsByFile.Find(NormalizedHeaderPath))
		{
			for (const FString& Dependent : *ExistingDependents)
			{
				MaterialsToQueue.Add(Dependent);
			}
		}

		RebuildDependencyGraph();

		if (const TSet<FString>* RebuiltDependents = HeaderDependentsByFile.Find(NormalizedHeaderPath))
		{
			for (const FString& Dependent : *RebuiltDependents)
			{
				MaterialsToQueue.Add(Dependent);
			}
		}

		const double Now = FPlatformTime::Seconds();
		for (const FString& MaterialFile : MaterialsToQueue)
		{
			PendingFiles.Add(MaterialFile, Now);
		}

		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		if (Settings && Settings->bVerboseLogs)
		{
			UE_LOG(
				LogDreamShader,
				Display,
				TEXT("DreamShader queued %d dependent .dsm file(s) for header '%s'."),
				MaterialsToQueue.Num(),
				*NormalizedHeaderPath);
		}
	}

	void FDreamShaderEditorBridge::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
	{
		TArray<FFileChangeData> ChangesCopy = FileChanges;
		TWeakPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> WeakBridge = AsWeak();
		AsyncTask(ENamedThreads::GameThread, [WeakBridge, Changes = MoveTemp(ChangesCopy)]()
		{
			TSharedPtr<FDreamShaderEditorBridge, ESPMode::ThreadSafe> Bridge = WeakBridge.Pin();
			if (!Bridge.IsValid() || Bridge->bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
			{
				return;
			}

			const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
			if (Settings && !Settings->bAutoCompileOnSave)
			{
				return;
			}

			for (const FFileChangeData& FileChange : Changes)
			{
				if (FileChange.Action == FFileChangeData::FCA_RescanRequired)
				{
					Bridge->QueueFullScan();
					continue;
				}

				if (!UE::DreamShader::IsDreamShaderSourceFile(FileChange.Filename))
				{
					continue;
				}

				if (FileChange.Action == FFileChangeData::FCA_Added || FileChange.Action == FFileChangeData::FCA_Modified)
				{
					if (UE::DreamShader::IsDreamShaderHeaderFile(FileChange.Filename))
					{
						Bridge->QueueDependentMaterialsForHeader(FileChange.Filename);
					}
					else if (IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						Bridge->RebuildDependencyGraph();
						Bridge->QueueSourceFile(FileChange.Filename);
					}
				}
				else if (FileChange.Action == FFileChangeData::FCA_Removed)
				{
					const FString SourceFile = UE::DreamShader::NormalizeSourceFilePath(FileChange.Filename);
					if (UE::DreamShader::IsDreamShaderHeaderFile(FileChange.Filename))
					{
						Bridge->QueueDependentMaterialsForHeader(FileChange.Filename);
					}
					else if (IsPackageMaterialFile(FileChange.Filename))
					{
						continue;
					}
					else
					{
						Bridge->PendingFiles.Remove(SourceFile);
						Bridge->ClearDiagnosticsForSourceAndDependencies(SourceFile);
						Bridge->RebuildDependencyGraph();
						Bridge->UpdateDiagnosticsFile();
					}
					UE_LOG(LogDreamShader, Display, TEXT("DreamShader source removed, existing generated assets were left untouched: %s"), *FileChange.Filename);
				}
			}
		});
	}

	bool FDreamShaderEditorBridge::Tick(float DeltaSeconds)
	{
		(void)DeltaSeconds;

		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return false;
		}

		ProcessRequestFiles();
		ProcessReadyFiles();
		return true;
	}

	void FDreamShaderEditorBridge::ProcessRequestFiles()
	{
		TArray<FString> RequestFiles;
		IFileManager::Get().FindFiles(RequestFiles, *FPaths::Combine(GetRequestDirectory(), TEXT("*.json")), true, false);

		for (const FString& RequestFileName : RequestFiles)
		{
			const FString RequestPath = FPaths::Combine(GetRequestDirectory(), RequestFileName);

			FString RequestText;
			if (!FFileHelper::LoadFileToString(RequestText, *RequestPath))
			{
				IFileManager::Get().Delete(*RequestPath);
				continue;
			}

			TSharedPtr<FJsonObject> RequestObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestText);
			if (FJsonSerializer::Deserialize(Reader, RequestObject) && RequestObject.IsValid())
			{
				FString Action;
				FString Scope;
				RequestObject->TryGetStringField(TEXT("action"), Action);
				RequestObject->TryGetStringField(TEXT("scope"), Scope);
				if (Action.Equals(TEXT("recompile"), ESearchCase::IgnoreCase))
				{
					if (Scope.Equals(TEXT("all"), ESearchCase::IgnoreCase))
					{
						RequestRecompileAll();
					}
					else if (Scope.Equals(TEXT("file"), ESearchCase::IgnoreCase))
					{
						FString SourceFilePath;
						if (RequestObject->TryGetStringField(TEXT("sourceFile"), SourceFilePath) && !SourceFilePath.IsEmpty())
						{
							QueueSourceFile(SourceFilePath);
						}
					}
				}
				else if (Action.Equals(TEXT("cleanGeneratedShaders"), ESearchCase::IgnoreCase))
				{
					RequestCleanGeneratedShaders();
				}
			}

			IFileManager::Get().Delete(*RequestPath);
		}
	}

	void FDreamShaderEditorBridge::ProcessReadyFiles()
	{
		const double Now = FPlatformTime::Seconds();
		const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();
		const double SaveDebounceSeconds = Settings ? FMath::Clamp(static_cast<double>(Settings->SaveDebounceSeconds), 0.05, 10.0) : 0.25;
		TArray<FString> ReadyFiles;
		for (const TPair<FString, double>& PendingFile : PendingFiles)
		{
			if (Now - PendingFile.Value >= SaveDebounceSeconds)
			{
				ReadyFiles.Add(PendingFile.Key);
			}
		}

		for (const FString& ReadyFile : ReadyFiles)
		{
			PendingFiles.Remove(ReadyFile);
			if (IFileManager::Get().FileExists(*ReadyFile))
			{
				ProcessSourceFile(ReadyFile);
			}
		}
	}

	void FDreamShaderEditorBridge::ProcessSourceFile(const FString& SourceFilePath)
	{
		FString Message;
		if (FMaterialGenerator::GenerateAssetsFromFile(SourceFilePath, Message))
		{
			ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
			UpdateDiagnosticsFile();
			UE_LOG(LogDreamShader, Display, TEXT("%s"), *Message);
			return;
		}

		TArray<FDiagnosticRecord> Diagnostics = BuildErrorDiagnostics(SourceFilePath, Message);
		ClearDiagnosticsForSourceAndDependencies(SourceFilePath);
		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
		UE_LOG(LogDreamShader, Error, TEXT("%s"), *Message);
	}

	void FDreamShaderEditorBridge::OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface)
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		UMaterial* Material = Cast<UMaterial>(MaterialInterface);
		if (!Material)
		{
			return;
		}

		const FString SourceFilePath = GetSourceFileMetadata(Material);
		if (SourceFilePath.IsEmpty())
		{
			return;
		}

		TArray<FDiagnosticRecord> Diagnostics;
		const FString MaterialAssetPath = Material->GetPathName();
		TSet<FString> SeenDiagnosticKeys;
		for (int32 ShaderPlatformIndex = 0; ShaderPlatformIndex < EShaderPlatform::SP_NumPlatforms; ++ShaderPlatformIndex)
		{
			const EShaderPlatform ShaderPlatform = static_cast<EShaderPlatform>(ShaderPlatformIndex);
			for (int32 QualityLevelIndex = 0; QualityLevelIndex <= static_cast<int32>(EMaterialQualityLevel::Num); ++QualityLevelIndex)
			{
				const EMaterialQualityLevel::Type QualityLevel = static_cast<EMaterialQualityLevel::Type>(QualityLevelIndex);
				const FMaterialResource* MaterialResource = Material->GetMaterialResource(ShaderPlatform, QualityLevel);
				if (!MaterialResource)
				{
					continue;
				}

				const FString ShaderPlatformLabel = GetShaderPlatformLabel(ShaderPlatform);
				const FString QualityLabel = GetMaterialQualityLevelLabel(QualityLevel);
				for (const FString& Error : MaterialResource->GetCompileErrors())
				{
					const FString RawError = Error.TrimStartAndEnd();
					if (RawError.IsEmpty())
					{
						continue;
					}

					FString ParsedFilePath;
					int32 ParsedLine = 1;
					int32 ParsedColumn = 1;
					FString ParsedMessage;
					const bool bHasParsedLocation = TryParseErrorLocation(RawError, ParsedFilePath, ParsedLine, ParsedColumn, ParsedMessage);
					const bool bMapsToDreamShaderSource = bHasParsedLocation && UE::DreamShader::IsDreamShaderSourceFile(ParsedFilePath);

					const FString DisplayMessage = FString::Printf(
						TEXT("[%s / %s] %s"),
						*ShaderPlatformLabel,
						*QualityLabel,
						*(bHasParsedLocation ? ParsedMessage : GetFirstMeaningfulErrorLine(RawError)));

					const FString DeduplicationKey = FString::Printf(
						TEXT("%s|%s|%s|%s|%d|%d"),
						*SourceFilePath,
						*ShaderPlatformLabel,
						*QualityLabel,
						*DisplayMessage,
						bMapsToDreamShaderSource ? ParsedLine : 1,
						bMapsToDreamShaderSource ? ParsedColumn : 1);
					if (SeenDiagnosticKeys.Contains(DeduplicationKey))
					{
						continue;
					}
					SeenDiagnosticKeys.Add(DeduplicationKey);

					FDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
					Diagnostic.FilePath = bMapsToDreamShaderSource ? ParsedFilePath : SourceFilePath;
					Diagnostic.Message = DisplayMessage;
					Diagnostic.Detail = RawError;
					Diagnostic.Stage = TEXT("materialCompile");
					Diagnostic.AssetPath = MaterialAssetPath;
					Diagnostic.ShaderPlatform = ShaderPlatformLabel;
					Diagnostic.QualityLevel = QualityLabel;
					Diagnostic.Code = TEXT("material-compile");
					Diagnostic.Source = TEXT("DreamShader Material Compile");
					Diagnostic.Line = bMapsToDreamShaderSource ? ParsedLine : 1;
					Diagnostic.Column = bMapsToDreamShaderSource ? ParsedColumn : 1;
				}
			}
		}

		SetDiagnostics(SourceFilePath, MoveTemp(Diagnostics));
		UpdateDiagnosticsFile();
	}

	void FDreamShaderEditorBridge::RegisterMenus()
	{
		if (bIsShuttingDown || bMenusRegistered || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		bMenusRegistered = true;

		FToolMenuOwnerScoped MenuOwner(DreamShaderToolMenuOwnerName);

		if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddMenuEntry(
				TEXT("DreamShader.RecompileAll"),
				LOCTEXT("DreamShaderRecompileLabel", "Recompile DSM"),
				LOCTEXT("DreamShaderRecompileTooltip", "Recompile all DreamShader .dsm files and refresh diagnostics."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestRecompileAll)));
			Section.AddMenuEntry(
				TEXT("DreamShader.CleanGeneratedShaders"),
				LOCTEXT("DreamShaderCleanGeneratedShadersLabel", "Clean Generated Shaders"),
				LOCTEXT("DreamShaderCleanGeneratedShadersTooltip", "Delete Intermediate/DreamShader/GeneratedShaders and queue a full DreamShader recompile."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Delete")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestCleanGeneratedShaders)));
			Section.AddMenuEntry(
				TEXT("DreamShader.OpenWorkspace"),
				LOCTEXT("DreamShaderOpenWorkspaceLabel", "Open Dream Shader Workspace (VSCode)"),
				LOCTEXT("DreamShaderOpenWorkspaceTooltip", "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor")),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::OpenDreamShaderWorkspace)));
		}

		if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar")))
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("DreamShader"));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("DreamShader.RecompileAllToolbar"),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::RequestRecompileAll)),
				LOCTEXT("DreamShaderRecompileToolbarLabel", "DSM"),
				LOCTEXT("DreamShaderRecompileToolbarTooltip", "Recompile all DreamShader .dsm files."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh"))));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("DreamShader.OpenWorkspaceToolbar"),
				FUIAction(FExecuteAction::CreateSP(AsShared(), &FDreamShaderEditorBridge::OpenDreamShaderWorkspace)),
				LOCTEXT("DreamShaderOpenWorkspaceToolbarLabel", "Open Dream Shader Workspace (VSCode)"),
				LOCTEXT("DreamShaderOpenWorkspaceToolbarTooltip", "Open the configured DreamShader source workspace in VSCode, or Notepad if VSCode is unavailable."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor"))));
		}

		if (UToolMenu* MaterialFunctionAssetMenu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMaterialFunction::StaticClass()))
		{
			FToolMenuSection& Section = MaterialFunctionAssetMenu->FindOrAddSection(TEXT("GetAssetActions"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.VirtualFunctionAssetActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu));
		}

		if (UToolMenu* MaterialEditorToolbar = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.MaterialEditor.ToolBar")))
		{
			FToolMenuSection& Section = MaterialEditorToolbar->FindOrAddSection(TEXT("DreamShader"));
			Section.AddDynamicEntry(
				TEXT("DreamShader.VirtualFunctionToolbarActions"),
				FNewToolMenuSectionDelegate::CreateSP(AsShared(), &FDreamShaderEditorBridge::PopulateMaterialFunctionEditorToolbar));
		}
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionAssetMenu(FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
		if (!Context || Context->SelectedAssets.Num() != 1 || !Context->SelectedAssets[0].IsInstanceOf(UMaterialFunction::StaticClass()))
		{
			return;
		}

		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Context->SelectedAssets[0].GetAsset());
		if (!MaterialFunction)
		{
			return;
		}

		InSection.AddSubMenu(
			TEXT("DreamShader.MaterialFunctionActions"),
			LOCTEXT("DreamShaderMaterialFunctionActionsLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialFunctionActionsTooltip", "DreamShader actions for this Material Function."),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu,
				TWeakObjectPtr<UMaterialFunction>(MaterialFunction)),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")));
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionEditorToolbar(FToolMenuSection& InSection)
	{
		const UMaterialEditorMenuContext* Context = InSection.FindContext<UMaterialEditorMenuContext>();
		TSharedPtr<IMaterialEditor> MaterialEditor = Context ? Context->MaterialEditor.Pin() : nullptr;
		if (!MaterialEditor.IsValid())
		{
			return;
		}

		UMaterialFunction* MaterialFunction = nullptr;
		const TArray<UObject*>* EditingObjects = MaterialEditor->GetObjectsCurrentlyBeingEdited();
		if (EditingObjects)
		{
			for (UObject* EditingObject : *EditingObjects)
			{
				MaterialFunction = Cast<UMaterialFunction>(EditingObject);
				if (MaterialFunction)
				{
					break;
				}
			}
		}

		if (!MaterialFunction)
		{
			return;
		}

		InSection.AddEntry(FToolMenuEntry::InitComboButton(
			TEXT("DreamShader.MaterialFunctionToolbarMenu"),
			FUIAction(),
			FNewToolMenuDelegate::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu,
				TWeakObjectPtr<UMaterialFunction>(MaterialFunction)),
			LOCTEXT("DreamShaderMaterialFunctionToolbarMenuLabel", "DreamShader"),
			LOCTEXT("DreamShaderMaterialFunctionToolbarMenuTooltip", "DreamShader actions for this Material Function."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings"))));
	}

	void FDreamShaderEditorBridge::PopulateMaterialFunctionDreamShaderMenu(UToolMenu* InMenu, TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		if (!InMenu || !MaterialFunction.IsValid())
		{
			return;
		}

		FToolMenuSection& Section = InMenu->AddSection(
			TEXT("DreamShader.VirtualFunctionActions"),
			LOCTEXT("DreamShaderVirtualFunctionActionsSection", "VirtualFunction"));
		FVirtualFunctionDefinitionLocation ExistingDefinition;
		if (FindVirtualFunctionDefinitionForMaterialFunction(MaterialFunction.Get(), ExistingDefinition))
		{
			Section.AddMenuEntry(
				TEXT("DreamShader.OpenVirtualFunction"),
				LOCTEXT("DreamShaderOpenVirtualFunctionLabel", "OpenVirtualFunction"),
				LOCTEXT("DreamShaderOpenVirtualFunctionTooltip", "Open the existing DreamShader VirtualFunction definition in VSCode."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.OpenInExternalEditor")),
				FUIAction(FExecuteAction::CreateSP(
					AsShared(),
					&FDreamShaderEditorBridge::OpenVirtualFunctionDefinitionFile,
					MaterialFunction)));
			Section.AddMenuEntry(
				TEXT("DreamShader.CopyVirtualFunctionReference"),
				LOCTEXT("DreamShaderCopyVirtualFunctionReferenceLabel", "Copy Virtual Function Reference"),
				LOCTEXT("DreamShaderCopyVirtualFunctionReferenceTooltip", "Copy a DreamShader Graph call that references this existing VirtualFunction."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
				FUIAction(FExecuteAction::CreateSP(
					AsShared(),
					&FDreamShaderEditorBridge::CopyVirtualFunctionReference,
					MaterialFunction)));
			return;
		}

		Section.AddMenuEntry(
			TEXT("DreamShader.CopyVirtualFunction"),
			LOCTEXT("DreamShaderCopyVirtualFunctionLabel", "CopyVirtualFunction"),
			LOCTEXT("DreamShaderCopyVirtualFunctionTooltip", "Copy a complete DreamShader VirtualFunction declaration for this Material Function."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CopyVirtualFunctionDefinition,
				MaterialFunction)));
		Section.AddMenuEntry(
			TEXT("DreamShader.CreateVirtualFunction"),
			LOCTEXT("DreamShaderCreateVirtualFunctionLabel", "CreateVirtualFunction"),
			LOCTEXT("DreamShaderCreateVirtualFunctionTooltip", "Create a .dsh file containing the VirtualFunction declaration."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CreateVirtualFunctionDefinitionFile,
				MaterialFunction)));
		Section.AddMenuEntry(
			TEXT("DreamShader.CopyVirtualFunctionCall"),
			LOCTEXT("DreamShaderCopyVirtualFunctionCallLabel", "CopyVirtualFunctionCall"),
			LOCTEXT("DreamShaderCopyVirtualFunctionCallTooltip", "Copy a DreamShader Graph call example for this VirtualFunction."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GenericCommands.Copy")),
			FUIAction(FExecuteAction::CreateSP(
				AsShared(),
				&FDreamShaderEditorBridge::CopyVirtualFunctionCall,
				MaterialFunction)));
	}

	void FDreamShaderEditorBridge::RequestRecompileAll()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		QueueFullScan();
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader queued a full .dsm recompile scan."));
	}

	void FDreamShaderEditorBridge::RequestCleanGeneratedShaders()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		CleanGeneratedShaderDirectory();
		QueueFullScan();
		UE_LOG(LogDreamShader, Display, TEXT("DreamShader cleaned generated shader includes and queued a full .dsm recompile scan."));
	}

	void FDreamShaderEditorBridge::OpenDreamShaderWorkspace()
	{
		if (bIsShuttingDown || IsEngineExitRequested() || GExitPurge)
		{
			return;
		}

		FString WorkspaceFilePath;
		FString Error;
		if (!WriteDreamShaderWorkspaceFile(WorkspaceFilePath, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to create workspace: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to create DreamShader workspace: %s"), *Error);
			return;
		}

		if (LaunchVSCodeWorkspace(WorkspaceFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Opened DreamShader workspace in VSCode: %s"), *WorkspaceFilePath)),
				SNotificationItem::CS_Success);
			UE_LOG(LogDreamShader, Display, TEXT("Opened DreamShader workspace in VSCode: %s"), *WorkspaceFilePath);
			return;
		}

		if (FPlatformProcess::LaunchFileInDefaultExternalApplication(*WorkspaceFilePath, nullptr, ELaunchVerb::Edit, false))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Opened DreamShader workspace: %s"), *WorkspaceFilePath)),
				SNotificationItem::CS_Success);
			UE_LOG(LogDreamShader, Display, TEXT("Opened DreamShader workspace with the default editor: %s"), *WorkspaceFilePath);
			return;
		}

		if (LaunchTextFileWithNotepad(WorkspaceFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Opened DreamShader workspace in Notepad: %s"), *WorkspaceFilePath)),
				SNotificationItem::CS_Success);
			UE_LOG(LogDreamShader, Display, TEXT("Opened DreamShader workspace in Notepad: %s"), *WorkspaceFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("DreamShader could not open workspace: %s"), *WorkspaceFilePath)),
			SNotificationItem::CS_Fail);
		UE_LOG(LogDreamShader, Warning, TEXT("Failed to open DreamShader workspace: %s"), *WorkspaceFilePath);
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionDefinition(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FString DefinitionText;
		FString Error;
		if (!BuildVirtualFunctionDefinition(Function, DefinitionText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction definition for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*DefinitionText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction definition for %s."), *Function->GetName())),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Copied VirtualFunction definition for '%s'.\n%s"), *Function->GetPathName(), *DefinitionText);
	}

	void FDreamShaderEditorBridge::CreateVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCreateVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FVirtualFunctionDefinitionLocation ExistingDefinition;
		if (FindVirtualFunctionDefinitionForMaterialFunction(Function, ExistingDefinition))
		{
			OpenVirtualFunctionDefinitionFile(MaterialFunction);
			return;
		}

		FString DefinitionText;
		FString Error;
		if (!BuildVirtualFunctionDefinition(Function, DefinitionText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction definition file for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		const FString DefinitionFilePath = MakeVirtualFunctionDefinitionFilePath(Function);
		const FString DefinitionDirectory = FPaths::GetPath(DefinitionFilePath);
		if (!IFileManager::Get().MakeDirectory(*DefinitionDirectory, true))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to create directory: %s"), *DefinitionDirectory)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to create VirtualFunction definition directory '%s'."), *DefinitionDirectory);
			return;
		}

		if (!FFileHelper::SaveStringToFile(DefinitionText, *DefinitionFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to write VirtualFunction file: %s"), *DefinitionFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to write VirtualFunction definition file '%s'."), *DefinitionFilePath);
			return;
		}

		if (!LaunchTextFileInPreferredEditor(DefinitionFilePath))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("Created VirtualFunction file but could not open it: %s"), *DefinitionFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Created VirtualFunction definition file '%s' but failed to open it."), *DefinitionFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Created VirtualFunction file: %s"), *DefinitionFilePath)),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Created VirtualFunction definition file '%s' for '%s'.\n%s"), *DefinitionFilePath, *Function->GetPathName(), *DefinitionText);
	}

	void FDreamShaderEditorBridge::OpenVirtualFunctionDefinitionFile(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderOpenVirtualFunctionNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FVirtualFunctionDefinitionLocation ExistingDefinition;
		if (!FindVirtualFunctionDefinitionForMaterialFunction(Function, ExistingDefinition))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not find a VirtualFunction definition for %s."), *Function->GetName())),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("No VirtualFunction definition found for '%s'."), *Function->GetPathName());
			return;
		}

		if (!LaunchTextFileInPreferredEditor(ExistingDefinition.SourceFilePath, ExistingDefinition.Line, ExistingDefinition.Column))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not open VirtualFunction file: %s"), *ExistingDefinition.SourceFilePath)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to open VirtualFunction definition file '%s'."), *ExistingDefinition.SourceFilePath);
			return;
		}

		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Opened VirtualFunction definition: %s"), *ExistingDefinition.SourceFilePath)),
			SNotificationItem::CS_Success);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("Opened VirtualFunction definition '%s' for '%s'."),
			*ExistingDefinition.SourceFilePath,
			*Function->GetPathName());
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionReference(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionReferenceNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FVirtualFunctionDefinitionLocation ExistingDefinition;
		if (!FindVirtualFunctionDefinitionForMaterialFunction(Function, ExistingDefinition))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader could not find a VirtualFunction definition for %s."), *Function->GetName())),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("No VirtualFunction definition found for '%s'."), *Function->GetPathName());
			return;
		}

		FString CallText;
		FString Error;
		if (!BuildVirtualFunctionCallTextFromSignature(
			ExistingDefinition.FunctionName,
			ExistingDefinition.Inputs,
			ExistingDefinition.Outputs,
			CallText,
			Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction reference: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("Failed to build VirtualFunction reference for '%s' from '%s': %s"),
				*Function->GetPathName(),
				*ExistingDefinition.SourceFilePath,
				*Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*CallText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction reference for %s."), *ExistingDefinition.FunctionName)),
			SNotificationItem::CS_Success);
		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("Copied VirtualFunction reference for '%s' from '%s': %s"),
			*Function->GetPathName(),
			*ExistingDefinition.SourceFilePath,
			*CallText);
	}

	void FDreamShaderEditorBridge::CopyVirtualFunctionCall(TWeakObjectPtr<UMaterialFunction> MaterialFunction)
	{
		UMaterialFunction* Function = MaterialFunction.Get();
		if (!Function)
		{
			ShowDreamShaderNotification(
				LOCTEXT("DreamShaderCopyVirtualFunctionCallNoAsset", "DreamShader could not find the selected Material Function."),
				SNotificationItem::CS_Fail);
			return;
		}

		FString CallText;
		FString Error;
		if (!BuildVirtualFunctionCallText(Function, CallText, Error))
		{
			ShowDreamShaderNotification(
				FText::FromString(FString::Printf(TEXT("DreamShader failed to build VirtualFunction call: %s"), *Error)),
				SNotificationItem::CS_Fail);
			UE_LOG(LogDreamShader, Warning, TEXT("Failed to build VirtualFunction call for '%s': %s"), *Function->GetPathName(), *Error);
			return;
		}

		FPlatformApplicationMisc::ClipboardCopy(*CallText);
		ShowDreamShaderNotification(
			FText::FromString(FString::Printf(TEXT("Copied VirtualFunction call for %s."), *Function->GetName())),
			SNotificationItem::CS_Success);
		UE_LOG(LogDreamShader, Display, TEXT("Copied VirtualFunction call for '%s': %s"), *Function->GetPathName(), *CallText);
	}

	void FDreamShaderEditorBridge::CleanGeneratedShaderDirectory()
	{
		const FString GeneratedShaderDirectory = UE::DreamShader::GetGeneratedShaderDirectory();
		IFileManager& FileManager = IFileManager::Get();

		TArray<FString> GeneratedShaderFiles;
		FileManager.FindFilesRecursive(
			GeneratedShaderFiles,
			*GeneratedShaderDirectory,
			TEXT("*"),
			true,
			false,
			false);

		const int32 DeletedFileCount = GeneratedShaderFiles.Num();
		FileManager.DeleteDirectory(*GeneratedShaderDirectory, false, true);
		FileManager.MakeDirectory(*GeneratedShaderDirectory, true);

		UE_LOG(
			LogDreamShader,
			Display,
			TEXT("DreamShader deleted %d generated shader file(s) from '%s'."),
			DeletedFileCount,
			*GeneratedShaderDirectory);
	}

	void FDreamShaderEditorBridge::RebuildDependencyGraph()
	{
		HeaderDependentsByFile.Reset();

		TArray<FString> MaterialFiles;
		FindProjectMaterialSourceFiles(MaterialFiles);
		for (const FString& MaterialFile : MaterialFiles)
		{
			TSet<FString> Dependencies;
			TSet<FString> VisitedFiles;
			CollectHeaderDependenciesRecursive(MaterialFile, Dependencies, VisitedFiles);
			for (const FString& HeaderFile : Dependencies)
			{
				HeaderDependentsByFile.FindOrAdd(HeaderFile).Add(MaterialFile);
			}
		}
	}

	void FDreamShaderEditorBridge::SyncVirtualFunctionDefinitions()
	{
		struct FVirtualFunctionReplacement
		{
			int32 StartIndex = INDEX_NONE;
			int32 EndIndex = INDEX_NONE;
			FString DefinitionText;
		};

		TArray<FString> SourceFiles;
		FindProjectDreamShaderSourceFiles(SourceFiles);

		int32 ScannedDefinitionCount = 0;
		int32 UpdatedDefinitionCount = 0;
		int32 ErrorCount = 0;

		for (const FString& SourceFile : SourceFiles)
		{
			FString SourceText;
			TArray<FVirtualFunctionDefinitionLocation> Locations;
			TArray<FDiagnosticRecord> Diagnostics;
			CollectVirtualFunctionDefinitionLocationsFromFile(SourceFile, Locations, &SourceText, &Diagnostics);

			TArray<FVirtualFunctionReplacement> Replacements;
			for (const FVirtualFunctionDefinitionLocation& Location : Locations)
			{
				++ScannedDefinitionCount;

				UMaterialFunction* Function = LoadObject<UMaterialFunction>(nullptr, *Location.AssetObjectPath);
				if (!Function)
				{
					Diagnostics.Add(MakeVirtualFunctionDiagnostic(
						SourceFile,
						FString::Printf(
							TEXT("VirtualFunction '%s' references missing MaterialFunction '%s'."),
							*Location.FunctionName,
							*Location.AssetObjectPath),
						FString(),
						Location.AssetObjectPath,
						Location.Line,
						Location.Column));
					continue;
				}

				FString GeneratedDefinition;
				FString Error;
				if (!BuildVirtualFunctionDefinition(Function, GeneratedDefinition, Error))
				{
					Diagnostics.Add(MakeVirtualFunctionDiagnostic(
						SourceFile,
						FString::Printf(
							TEXT("VirtualFunction '%s' could not be refreshed from MaterialFunction '%s': %s"),
							*Location.FunctionName,
							*Location.AssetObjectPath,
							*Error),
						Error,
						Location.AssetObjectPath,
						Location.Line,
						Location.Column));
					continue;
				}

				if (NormalizeVirtualFunctionDefinitionText(Location.CurrentText)
					!= NormalizeVirtualFunctionDefinitionText(GeneratedDefinition))
				{
					FVirtualFunctionReplacement& Replacement = Replacements.AddDefaulted_GetRef();
					Replacement.StartIndex = Location.StartIndex;
					Replacement.EndIndex = Location.EndIndex;
					Replacement.DefinitionText = GeneratedDefinition;
				}
			}

			if (!Replacements.IsEmpty())
			{
				FString UpdatedSourceText = SourceText;
				Replacements.Sort([](const FVirtualFunctionReplacement& A, const FVirtualFunctionReplacement& B)
				{
					return A.StartIndex > B.StartIndex;
				});

				for (const FVirtualFunctionReplacement& Replacement : Replacements)
				{
					UpdatedSourceText =
						UpdatedSourceText.Left(Replacement.StartIndex)
						+ Replacement.DefinitionText
						+ UpdatedSourceText.Mid(Replacement.EndIndex);
				}

				if (!FFileHelper::SaveStringToFile(UpdatedSourceText, *SourceFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					Diagnostics.Add(MakeVirtualFunctionDiagnostic(
						SourceFile,
						FString::Printf(TEXT("DreamShader failed to update VirtualFunction source file '%s'."), *SourceFile),
						FString(),
						FString(),
						1,
						1));
				}
				else
				{
					UpdatedDefinitionCount += Replacements.Num();
					UE_LOG(
						LogDreamShader,
						Display,
						TEXT("DreamShader refreshed %d VirtualFunction definition(s) in '%s'."),
						Replacements.Num(),
						*SourceFile);
				}
			}

			if (Diagnostics.IsEmpty())
			{
				if (!Locations.IsEmpty())
				{
					ClearDiagnostics(SourceFile);
				}
			}
			else
			{
				ErrorCount += Diagnostics.Num();
				SetDiagnostics(SourceFile, MoveTemp(Diagnostics));
			}
		}

		if (ScannedDefinitionCount > 0 || UpdatedDefinitionCount > 0 || ErrorCount > 0)
		{
			UE_LOG(
				LogDreamShader,
				Display,
				TEXT("DreamShader scanned %d VirtualFunction definition(s), refreshed %d, reported %d issue(s)."),
				ScannedDefinitionCount,
				UpdatedDefinitionCount,
				ErrorCount);
		}
	}

	void FDreamShaderEditorBridge::SetDiagnostics(const FString& SourceFilePath, TArray<FDiagnosticRecord>&& Diagnostics)
	{
		const FString NormalizedPath = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath);
		DiagnosticsByFile.Remove(NormalizedPath);
		if (Diagnostics.IsEmpty())
		{
			return;
		}

		for (FDiagnosticRecord& Diagnostic : Diagnostics)
		{
			const FString DiagnosticFilePath = Diagnostic.FilePath.IsEmpty()
				? NormalizedPath
				: UE::DreamShader::NormalizeSourceFilePath(Diagnostic.FilePath);
			Diagnostic.FilePath.Reset();
			DiagnosticsByFile.FindOrAdd(DiagnosticFilePath).Add(MoveTemp(Diagnostic));
		}
	}

	void FDreamShaderEditorBridge::ClearDiagnostics(const FString& SourceFilePath)
	{
		DiagnosticsByFile.Remove(UE::DreamShader::NormalizeSourceFilePath(SourceFilePath));
	}

	void FDreamShaderEditorBridge::ClearDiagnosticsForSourceAndDependencies(const FString& SourceFilePath)
	{
		ClearDiagnostics(SourceFilePath);

		TSet<FString> Dependencies;
		TSet<FString> VisitedFiles;
		CollectHeaderDependenciesRecursive(SourceFilePath, Dependencies, VisitedFiles);
		for (const FString& HeaderFile : Dependencies)
		{
			ClearDiagnostics(HeaderFile);
		}
	}

	void FDreamShaderEditorBridge::UpdateDiagnosticsFile() const
	{
		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetNumberField(TEXT("version"), 1);
		RootObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());

		TArray<TSharedPtr<FJsonValue>> FileEntries;
		for (const TPair<FString, TArray<FDiagnosticRecord>>& Pair : DiagnosticsByFile)
		{
			TSharedRef<FJsonObject> FileObject = MakeShared<FJsonObject>();
			FileObject->SetStringField(TEXT("path"), Pair.Key);

			TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
			for (const FDiagnosticRecord& Diagnostic : Pair.Value)
			{
				AddDiagnosticJson(DiagnosticValues, Diagnostic);
			}

			FileObject->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
			FileEntries.Add(MakeShared<FJsonValueObject>(FileObject));
		}

		RootObject->SetArrayField(TEXT("files"), FileEntries);

		FString OutputText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputText);
		FJsonSerializer::Serialize(RootObject, Writer);
		FFileHelper::SaveStringToFile(OutputText, *GetDiagnosticsFilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	TArray<FDreamShaderEditorBridge::FDiagnosticRecord> FDreamShaderEditorBridge::BuildErrorDiagnostics(
		const FString& SourceFilePath,
		const FString& ErrorMessage) const
	{
		TArray<FDiagnosticRecord> Diagnostics;

		TArray<FString> Lines;
		ErrorMessage.ParseIntoArrayLines(Lines, false);
		if (Lines.IsEmpty())
		{
			Lines.Add(ErrorMessage);
		}

		const FString PathPrefix = UE::DreamShader::NormalizeSourceFilePath(SourceFilePath) + TEXT(": ");
		for (FString Line : Lines)
		{
			FString DiagnosticFilePath;
			int32 DiagnosticLine = 1;
			int32 DiagnosticColumn = 1;
			FString DiagnosticMessage;
			if (TryParseErrorLocation(Line, DiagnosticFilePath, DiagnosticLine, DiagnosticColumn, DiagnosticMessage))
			{
				FDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
				Diagnostic.FilePath = DiagnosticFilePath;
				Diagnostic.Message = DiagnosticMessage;
				Diagnostic.Detail = Line;
				Diagnostic.Stage = TEXT("generate");
				Diagnostic.Code = TEXT("generate-error");
				Diagnostic.Source = TEXT("DreamShader Generate");
				Diagnostic.Line = DiagnosticLine;
				Diagnostic.Column = DiagnosticColumn;
				continue;
			}

			if (Line.StartsWith(PathPrefix))
			{
				Line.RightChopInline(PathPrefix.Len(), EAllowShrinking::No);
			}

			FDiagnosticRecord& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Message = Line;
			Diagnostic.Detail = Line;
			Diagnostic.Stage = TEXT("generate");
			Diagnostic.Code = TEXT("generate-error");
			Diagnostic.Source = TEXT("DreamShader Generate");
		}

		return Diagnostics;
	}
}

#undef LOCTEXT_NAMESPACE
