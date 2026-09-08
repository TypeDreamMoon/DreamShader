// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Text helpers for asset references written in Unreal's *export* form --
// `Class'/Game/Folder/Asset.Asset'` -- which is exactly what the Content Browser's "Copy Reference"
// puts on the clipboard:
//
//     /Script/Engine.Texture2D'/Game/Cloud/T_Volume_03.T_Volume_03'
//     /Script/Engine.VolumeTexture'/Game/Cloud/T_Volume_03.T_Volume_03'
//     Texture2D'/Game/Cloud/T_Volume_03.T_Volume_03'            (older engine spelling)
//
// DreamShaderLang has two independent resolvers for asset references -- the texture-default resolver
// in the parser and the asset-reference resolver in the generator (see Docs/parameters/path.md) --
// and both must accept the pasted spelling. Rather than growing the shell grammar twice, both strip
// it with the one function below and hand the bare object path to their existing absolute-path
// branch.
//
// Header-only and deliberately dependency-light (CoreMinimal + DreamShaderTypes for the texture
// dimension enum): a third consumer -- the rename-sync service, which has to *recognise* the shelled
// spelling in source text -- can include this and nothing else.

#pragma once

#include "CoreMinimal.h"

#include "DreamShaderTypes.h"

namespace UE::DreamShader
{
	/** An asset reference split out of its `Class'...'` shell. */
	struct FDreamShaderReferenceShell
	{
		/** The class as written, verbatim: `/Script/Engine.Texture2D`, or `Texture2D`. May be empty. */
		FString ClassPath;

		/** Just the class name: `Texture2D` for both spellings above. May be empty. */
		FString ClassName;

		/** The object path written between the single quotes, trimmed. Never empty on success. */
		FString ObjectPath;
	};

	/**
	 * Strips a `<prefix>'<object path>'` shell.
	 *
	 * Returns false -- leaving OutShell untouched -- when the text is not shelled, which is the
	 * ordinary case and never an error: the caller then carries on with the text it already had.
	 * A shell whose quotes contain nothing, or contain another quote, is also rejected rather than
	 * guessed at, so a malformed value still reaches the resolver's own diagnostics intact.
	 *
	 * The prefix is NOT validated. Any leading text is accepted and reported as written; deciding
	 * whether a class name means anything -- and whether it fits the slot -- belongs to the caller,
	 * because only the caller knows what the slot requires.
	 */
	inline bool TryStripDreamShaderReferenceShell(const FString& InText, FDreamShaderReferenceShell& OutShell)
	{
		const FString Trimmed = InText.TrimStartAndEnd();
		if (Trimmed.Len() < 2 || !Trimmed.EndsWith(TEXT("'"), ESearchCase::CaseSensitive))
		{
			return false;
		}

		int32 FirstQuoteIndex = INDEX_NONE;
		if (!Trimmed.FindChar(TCHAR('\''), FirstQuoteIndex) || FirstQuoteIndex == Trimmed.Len() - 1)
		{
			return false;
		}

		FString Inner = Trimmed.Mid(FirstQuoteIndex + 1, Trimmed.Len() - FirstQuoteIndex - 2);
		int32 UnusedIndex = INDEX_NONE;
		if (Inner.FindChar(TCHAR('\''), UnusedIndex))
		{
			return false;
		}

		Inner.TrimStartAndEndInline();
		if (Inner.IsEmpty())
		{
			return false;
		}

		FString ClassPath = Trimmed.Left(FirstQuoteIndex).TrimStartAndEnd();
		FString ClassName = ClassPath;
		int32 LastDotIndex = INDEX_NONE;
		if (ClassPath.FindLastChar(TCHAR('.'), LastDotIndex))
		{
			ClassName = ClassPath.Mid(LastDotIndex + 1);
		}

		OutShell.ClassPath = MoveTemp(ClassPath);
		OutShell.ClassName = MoveTemp(ClassName);
		OutShell.ObjectPath = MoveTemp(Inner);
		return true;
	}

	/**
	 * Maps a written class name onto the texture dimension DreamShaderLang models.
	 *
	 * Only classes whose dimension is unambiguous are listed. `SparseVolumeTexture`,
	 * `TextureCollection` and `TextureCubeArray` are textures but have no ETextShaderTextureType, so
	 * they answer false here and true from IsDreamShaderTextureReferenceClass -- "a texture, dimension
	 * not judged" rather than "not a texture".
	 */
	inline bool TryGetDreamShaderReferenceTextureType(const FString& InClassName, ETextShaderTextureType& OutTextureType)
	{
		struct FTextureClassRow
		{
			const TCHAR* ClassName;
			ETextShaderTextureType TextureType;
		};

		static const FTextureClassRow Rows[] =
		{
			{ TEXT("Texture2D"),                  ETextShaderTextureType::Texture2D },
			{ TEXT("Texture2DDynamic"),           ETextShaderTextureType::Texture2D },
			{ TEXT("TextureLightProfile"),        ETextShaderTextureType::Texture2D },
			{ TEXT("TextureRenderTarget2D"),      ETextShaderTextureType::Texture2D },
			{ TEXT("CanvasRenderTarget2D"),       ETextShaderTextureType::Texture2D },
			{ TEXT("MediaTexture"),               ETextShaderTextureType::Texture2D },
			{ TEXT("TextureCube"),                ETextShaderTextureType::TextureCube },
			{ TEXT("TextureRenderTargetCube"),    ETextShaderTextureType::TextureCube },
			{ TEXT("Texture2DArray"),             ETextShaderTextureType::Texture2DArray },
			{ TEXT("TextureRenderTarget2DArray"), ETextShaderTextureType::Texture2DArray },
			{ TEXT("VolumeTexture"),              ETextShaderTextureType::VolumeTexture },
			{ TEXT("Texture3D"),                  ETextShaderTextureType::VolumeTexture },
			{ TEXT("TextureRenderTargetVolume"),  ETextShaderTextureType::VolumeTexture },
		};

		for (const FTextureClassRow& Row : Rows)
		{
			if (InClassName.Equals(Row.ClassName, ESearchCase::IgnoreCase))
			{
				OutTextureType = Row.TextureType;
				return true;
			}
		}

		return false;
	}

	/** True for any class name DreamShader knows to be a texture, dimension judged or not. */
	inline bool IsDreamShaderTextureReferenceClass(const FString& InClassName)
	{
		ETextShaderTextureType UnusedTextureType = ETextShaderTextureType::Texture2D;
		if (TryGetDreamShaderReferenceTextureType(InClassName, UnusedTextureType))
		{
			return true;
		}

		static const TCHAR* const DimensionlessTextureClasses[] =
		{
			TEXT("Texture"),
			TEXT("TextureCubeArray"),
			TEXT("TextureCollection"),
			TEXT("SparseVolumeTexture"),
			TEXT("StreamableSparseVolumeTexture"),
			// A runtime virtual texture is not a UTexture, but RuntimeVirtualTextureSampleParameter
			// takes one where a texture would go, so judging it here would reject a correct source.
			TEXT("RuntimeVirtualTexture"),
		};

		for (const TCHAR* const ClassName : DimensionlessTextureClasses)
		{
			if (InClassName.Equals(ClassName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	/**
	 * True only for class names DreamShader positively knows are NOT textures.
	 *
	 * The list is short on purpose. An unknown prefix must never be an error -- a project can name
	 * any class it likes, and refusing one DreamShader has not heard of would break sources that
	 * work -- so "not on this list" means "no opinion", not "fine".
	 */
	inline bool IsKnownDreamShaderNonTextureReferenceClass(const FString& InClassName)
	{
		static const TCHAR* const NonTextureClasses[] =
		{
			TEXT("Material"),
			TEXT("MaterialInstanceConstant"),
			TEXT("MaterialInstanceDynamic"),
			TEXT("MaterialFunction"),
			TEXT("MaterialFunctionInstance"),
			TEXT("MaterialFunctionMaterialLayer"),
			TEXT("MaterialFunctionMaterialLayerInstance"),
			TEXT("MaterialFunctionMaterialLayerBlend"),
			TEXT("MaterialFunctionMaterialLayerBlendInstance"),
			TEXT("MaterialParameterCollection"),
			TEXT("PhysicalMaterial"),
			TEXT("SubsurfaceProfile"),
			TEXT("SpecularProfile"),
			TEXT("CurveFloat"),
			TEXT("CurveVector"),
			TEXT("CurveLinearColor"),
			TEXT("CurveLinearColorAtlas"),
			TEXT("Font"),
			TEXT("StaticMesh"),
			TEXT("SkeletalMesh"),
			TEXT("Blueprint"),
			TEXT("DataTable"),
			TEXT("SoundWave"),
			TEXT("SoundCue"),
			TEXT("NiagaraSystem"),
			TEXT("ParticleSystem"),
			TEXT("LandscapeGrassType"),
			TEXT("LevelSequence"),
			TEXT("World"),
		};

		for (const TCHAR* const ClassName : NonTextureClasses)
		{
			if (InClassName.Equals(ClassName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	/** The DreamShaderLang spelling of a texture dimension, for diagnostics. */
	inline const TCHAR* LexDreamShaderTextureType(const ETextShaderTextureType InTextureType)
	{
		switch (InTextureType)
		{
		case ETextShaderTextureType::TextureCube:
			return TEXT("TextureCube");
		case ETextShaderTextureType::Texture2DArray:
			return TEXT("Texture2DArray");
		case ETextShaderTextureType::VolumeTexture:
			return TEXT("VolumeTexture");
		case ETextShaderTextureType::Texture2D:
		default:
			return TEXT("Texture2D");
		}
	}
}
