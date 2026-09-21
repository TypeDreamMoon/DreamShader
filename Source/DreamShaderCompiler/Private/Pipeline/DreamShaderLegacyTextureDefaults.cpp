// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// See DreamShaderLegacyTextureDefaults.h.

#include "Pipeline/DreamShaderLegacyTextureDefaults.h"

#include "DreamShaderCompilePipeline.h"

// TryStripDreamShaderReferenceShell and the two class tables, shared with the asset-reference resolver.
#include "DreamShaderAssetReferenceText.h"

#include "DreamShaderGeneratedAssets.h"
#include "DreamShaderModule.h"

#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Lang/LangAst.h"
#include "Lang/LangDiagnostic.h"
#include "Lang/LangLegacy.h"

#define LOCTEXT_NAMESPACE "DreamShader.LegacyTextureDefaults"

namespace UE::DreamShader::Editor::Compiler
{
	namespace LegacyTextureDefaults
	{
		/** The shell of a default as written: bare, `"..."`, `Path("...")` or `Path(Root, "...")`. */
		static bool TryFindShell(const FString& Written, FDreamShaderReferenceShell& OutShell)
		{
			FString Text = Written.TrimStartAndEnd();
			if (TryStripDreamShaderReferenceShell(Text, OutShell))
			{
				return true;
			}

			if (Text.StartsWith(TEXT("Path("), ESearchCase::IgnoreCase) && Text.EndsWith(TEXT(")"), ESearchCase::CaseSensitive))
			{
				// The reference is the last argument; an object path holds no comma.
				Text = Text.Mid(5, Text.Len() - 6);
				int32 Comma = INDEX_NONE;
				if (Text.FindLastChar(TCHAR(','), Comma))
				{
					Text = Text.Mid(Comma + 1);
				}
				Text.TrimStartAndEndInline();
			}

			if (Text.Len() >= 2 && Text.StartsWith(TEXT("\""), ESearchCase::CaseSensitive) && Text.EndsWith(TEXT("\""), ESearchCase::CaseSensitive))
			{
				Text = Text.Mid(1, Text.Len() - 2);
			}
			return TryStripDreamShaderReferenceShell(Text, OutShell);
		}

		/** The 1.x ValidateTextureReferenceShellClass, word for word. */
		static bool JudgeShell(
			const FDreamShaderReferenceShell& Shell,
			const ETextShaderTextureType Expected,
			const bool bExpectedIsExplicit,
			const Lang::FLangSpan& Span,
			Lang::FLangDiagnosticSink& Diagnostics)
		{
			if (Shell.ClassName.IsEmpty())
			{
				return true;
			}

			ETextShaderTextureType Written = ETextShaderTextureType::Texture2D;
			if (TryGetDreamShaderReferenceTextureType(Shell.ClassName, Written))
			{
				if (bExpectedIsExplicit && Written != Expected)
				{
					Diagnostics.Error(TEXT("DSH1044"), Span, FText::Format(
						LOCTEXT("TextureReferenceClassWrongDimension", "Asset reference is written as '{0}', but this property is declared as {1}."),
						FText::FromString(Shell.ClassName),
						FText::FromString(FString(LexDreamShaderTextureType(Expected)))));
					return false;
				}
				return true;
			}

			if (IsKnownDreamShaderNonTextureReferenceClass(Shell.ClassName))
			{
				Diagnostics.Error(TEXT("DSH1043"), Span, FText::Format(
					LOCTEXT("TextureReferenceClassNotATexture", "Asset reference is written as '{0}', which is not a texture class; a texture default requires {1}."),
					FText::FromString(Shell.ClassName),
					FText::FromString(FString(bExpectedIsExplicit
						? LexDreamShaderTextureType(Expected)
						: TEXT("a texture such as Texture2D, TextureCube, Texture2DArray or VolumeTexture")))));
				return false;
			}
			return true;
		}

		/** `TextureSampleParameterVolume` and its siblings: the dimension the node type fixes, when it fixes one. */
		static bool TryGetSampleParameterTextureType(const FString& NodeType, ETextShaderTextureType& OutType, bool& bOutExplicit)
		{
			if (!NodeType.StartsWith(TEXT("TextureSampleParameter"), ESearchCase::IgnoreCase))
			{
				return false;
			}
			const FString Suffix = NodeType.Mid(22);
			bOutExplicit = true;
			if (Suffix.Equals(TEXT("2D"), ESearchCase::IgnoreCase) || Suffix.Equals(TEXT("SubUV"), ESearchCase::IgnoreCase)) { OutType = ETextShaderTextureType::Texture2D; }
			else if (Suffix.Equals(TEXT("Cube"), ESearchCase::IgnoreCase)) { OutType = ETextShaderTextureType::TextureCube; }
			else if (Suffix.Equals(TEXT("2DArray"), ESearchCase::IgnoreCase)) { OutType = ETextShaderTextureType::Texture2DArray; }
			else if (Suffix.Equals(TEXT("Volume"), ESearchCase::IgnoreCase)) { OutType = ETextShaderTextureType::VolumeTexture; }
			else
			{
				// A cube array and whatever the engine adds next: a texture, of no dimension this check knows.
				OutType = ETextShaderTextureType::Texture2D;
				bOutExplicit = false;
			}
			return true;
		}
	}

	bool ValidateDreamShaderLegacyTextureDefaults(
		const Lang::FModule& Module,
		const Lang::FLegacyMigrationInfo& Legacy,
		Lang::FLangDiagnosticSink& Diagnostics)
	{
		(void)Module;
		bool bOk = true;

		// Parameter nodes (`TextureSampleParameter2D P = ...`): declared here, expanded only where they are read.
		for (const Lang::FLegacyParameterDeclaration& Declaration : Legacy.ParameterDeclarations)
		{
			ETextShaderTextureType Expected = ETextShaderTextureType::Texture2D;
			bool bExplicit = false;
			FDreamShaderReferenceShell Shell;
			if (Declaration.DefaultText.IsEmpty()
				|| !LegacyTextureDefaults::TryGetSampleParameterTextureType(Declaration.NodeType, Expected, bExplicit)
				|| !LegacyTextureDefaults::TryFindShell(Declaration.DefaultText, Shell))
			{
				continue;
			}
			bOk &= LegacyTextureDefaults::JudgeShell(Shell, Expected, bExplicit, Declaration.DeclarationSpan, Diagnostics);
		}

		// Texture uniforms (`Texture2D T = ...`, `TextureObjectParameter T = ...`).
		for (const Lang::FLegacyAssetReference& Reference : Legacy.AssetReferences)
		{
			if (Reference.Use != Lang::FLegacyAssetReference::EUse::TextureDefault || !Reference.Node)
			{
				continue;
			}
			const Lang::FVariableDecl* Variable = Reference.Node->As<Lang::FVariableDecl>();
			FDreamShaderReferenceShell Shell;
			if (!Variable || !Variable->Type.IsTexture() || !LegacyTextureDefaults::TryFindShell(Reference.Text, Shell))
			{
				continue;
			}

			ETextShaderTextureType Expected = ETextShaderTextureType::Texture2D;
			// `TextureObjectParameter` takes its dimension from the asset (bTypeFromAsset); only a dimension the author
			// spelled is held against the shell.
			bool bExplicit = !Reference.bTypeFromAsset;
			switch (Variable->Type.Texture)
			{
			case Lang::ETextureKind::TextureCube: Expected = ETextShaderTextureType::TextureCube; break;
			case Lang::ETextureKind::Texture2DArray: Expected = ETextShaderTextureType::Texture2DArray; break;
			case Lang::ETextureKind::Texture3D:
			case Lang::ETextureKind::VolumeTexture: Expected = ETextShaderTextureType::VolumeTexture; break;
			default: break;
			}
			bOk &= LegacyTextureDefaults::JudgeShell(Shell, Expected, bExplicit, Reference.Span, Diagnostics);
		}

		return bOk;
	}

	void ResolveDreamShaderLegacyTextureTypes(Lang::FModule& Module, const Lang::FLegacyMigrationInfo& Legacy)
	{
		for (const Lang::FLegacyAssetReference& Reference : Legacy.AssetReferences)
		{
			if (Reference.Use != Lang::FLegacyAssetReference::EUse::TextureDefault || !Reference.bTypeFromAsset || !Reference.Node)
			{
				continue;
			}

			// The legacy record points into the module this function was handed to change.
			const Lang::FVariableDecl* Found = Reference.Node->As<Lang::FVariableDecl>();
			Lang::FVariableDecl* Variable = nullptr;
			for (const Lang::FDeclPtr& Decl : Module.Declarations)
			{
				if (Decl.Get() == Found)
				{
					Variable = static_cast<Lang::FVariableDecl*>(Decl.Get());
					break;
				}
			}
			if (!Variable || !Variable->Type.IsTexture())
			{
				continue;
			}

			FString ObjectPath;
			FDreamShaderError Unread;
			if (!Private::TryResolveDreamShaderAssetReference(Reference.Text, ObjectPath, Unread, UTexture::StaticClass()))
			{
				continue;
			}
			const UTexture* Texture = LoadObject<UTexture>(nullptr, *ObjectPath);
			if (!Texture)
			{
				continue;
			}

			if (Texture->IsA<UTextureCube>())
			{
				Variable->Type.Name = TEXT("TextureCube");
				Variable->Type.Texture = Lang::ETextureKind::TextureCube;
			}
			else if (Texture->IsA<UTexture2DArray>())
			{
				Variable->Type.Name = TEXT("Texture2DArray");
				Variable->Type.Texture = Lang::ETextureKind::Texture2DArray;
			}
			else if (Texture->IsA<UVolumeTexture>())
			{
				Variable->Type.Name = TEXT("VolumeTexture");
				Variable->Type.Texture = Lang::ETextureKind::VolumeTexture;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
