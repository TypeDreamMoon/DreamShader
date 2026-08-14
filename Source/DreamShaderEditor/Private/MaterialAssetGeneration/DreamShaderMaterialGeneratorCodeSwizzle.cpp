// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Vector swizzle lowering for FCodeGraphBuilder: resolve channel chars, build an ordered
// component-mask fast path, and otherwise compose per-channel masks + AppendVector. Two file-local
// statics (channel index / ordered mask) feed the CreateSingleChannelMask / CreateSwizzleExpression
// members. Extracted byte-for-byte from DreamShaderMaterialGeneratorCodeExpressions.cpp; the member
// declarations stay in the FCodeGraphBuilder class header, so all call sites are unchanged.

#include "DreamShaderMaterialGeneratorCodeShared.h"

#include "Misc/ScopedSlowTask.h"

namespace UE::DreamShader::Editor::Private
{
	static bool TryResolveSwizzleChannelIndex(const TCHAR ChannelChar, int32& OutChannelIndex)
	{
		switch (FChar::ToLower(ChannelChar))
		{
		case TCHAR('x'):
		case TCHAR('r'):
			OutChannelIndex = 0;
			return true;
		case TCHAR('y'):
		case TCHAR('g'):
			OutChannelIndex = 1;
			return true;
		case TCHAR('z'):
		case TCHAR('b'):
			OutChannelIndex = 2;
			return true;
		case TCHAR('w'):
		case TCHAR('a'):
			OutChannelIndex = 3;
			return true;
		default:
			OutChannelIndex = INDEX_NONE;
			return false;
		}
	}

	static bool TryBuildOrderedSwizzleMask(
		const FCodeValue& BaseValue,
		const FString& Swizzle,
		int32& OutChannelMask,
		int32& OutComponentCount)
	{
		OutChannelMask = 0;
		OutComponentCount = 0;

		int32 PreviousChannelIndex = INDEX_NONE;
		TArray<int32> SourceChannels;
		if (BaseValue.bHasInputMask)
		{
			if (BaseValue.InputMaskR)
			{
				SourceChannels.Add(0);
			}
			if (BaseValue.InputMaskG)
			{
				SourceChannels.Add(1);
			}
			if (BaseValue.InputMaskB)
			{
				SourceChannels.Add(2);
			}
			if (BaseValue.InputMaskA)
			{
				SourceChannels.Add(3);
			}
		}

		for (int32 Index = 0; Index < Swizzle.Len(); ++Index)
		{
			int32 ChannelIndex = INDEX_NONE;
			if (!TryResolveSwizzleChannelIndex(Swizzle[Index], ChannelIndex)
				|| ChannelIndex >= BaseValue.ComponentCount)
			{
				return false;
			}

			const int32 SourceChannelIndex = BaseValue.bHasInputMask
				? (SourceChannels.IsValidIndex(ChannelIndex) ? SourceChannels[ChannelIndex] : INDEX_NONE)
				: ChannelIndex;
			if (SourceChannelIndex == INDEX_NONE || SourceChannelIndex <= PreviousChannelIndex)
			{
				return false;
			}

			const int32 ChannelBit = 1 << SourceChannelIndex;
			if ((OutChannelMask & ChannelBit) != 0)
			{
				return false;
			}

			OutChannelMask |= ChannelBit;
			PreviousChannelIndex = SourceChannelIndex;
			++OutComponentCount;
		}

		return OutComponentCount > 0;
	}

	bool FCodeGraphBuilder::CreateSingleChannelMask(
		const FCodeValue& BaseValue,
		const int32 ChannelIndex,
		FCodeValue& OutValue,
		FString& OutError)
	{
		int32 SourceChannelIndex = ChannelIndex;
		if (BaseValue.bHasInputMask)
		{
			TArray<int32> SourceChannels;
			if (BaseValue.InputMaskR)
			{
				SourceChannels.Add(0);
			}
			if (BaseValue.InputMaskG)
			{
				SourceChannels.Add(1);
			}
			if (BaseValue.InputMaskB)
			{
				SourceChannels.Add(2);
			}
			if (BaseValue.InputMaskA)
			{
				SourceChannels.Add(3);
			}

			if (!SourceChannels.IsValidIndex(ChannelIndex))
			{
				OutError = FString::Printf(TEXT("Channel %d is invalid for a value with %d components."), ChannelIndex, BaseValue.ComponentCount); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			SourceChannelIndex = SourceChannels[ChannelIndex];
		}

		OutValue = BaseValue;
		ClearCodeValueInputMask(OutValue);
		if (!ApplyCodeValueInputMask(OutValue, 1 << SourceChannelIndex, 1))
		{
			OutError = TEXT("Failed to compose swizzle channel mask.");
			return false;
		}
		OutValue.bHasAuthoritativeComponentCount = BaseValue.bHasAuthoritativeComponentCount;
		return true;
	}

	bool FCodeGraphBuilder::CreateSwizzleExpression(
		const FCodeValue& BaseValue,
		const FString& Swizzle,
		FCodeValue& OutValue,
		FString& OutError)
	{
		if (Swizzle.IsEmpty() || Swizzle.Len() > 4)
		{
			OutError = FString::Printf(TEXT("Unsupported swizzle '%s'."), *Swizzle); /* I18N-EXEMPT: deferred codegen or compatibility path */
			return false;
		}

		int32 DirectChannelMask = 0;
		int32 DirectComponentCount = 0;
		if (BaseValue.Expression
			&& TryBuildOrderedSwizzleMask(BaseValue, Swizzle, DirectChannelMask, DirectComponentCount))
		{
			OutValue = BaseValue;
			if (ApplyCodeValueInputMask(OutValue, DirectChannelMask, DirectComponentCount))
			{
				OutValue.bHasAuthoritativeComponentCount = BaseValue.bHasAuthoritativeComponentCount;
				return true;
			}
		}

		TArray<FCodeValue> Channels;
		if (BaseValue.ComponentCount == 1)
		{
			for (int32 Index = 0; Index < Swizzle.Len(); ++Index)
			{
				// A scalar only exposes channel 0 (x/r); .y/.z/.w/.g/.b/.a are out of range and
				// must error (mirrors the multi-component branch) instead of silently splatting.
				int32 ChannelIndex = INDEX_NONE;
				if (!TryResolveSwizzleChannelIndex(Swizzle[Index], ChannelIndex)
					|| ChannelIndex >= BaseValue.ComponentCount)
				{
					OutError = FString::Printf(TEXT("Swizzle '%s' is invalid for a value with %d components."), *Swizzle, BaseValue.ComponentCount); /* I18N-EXEMPT: deferred codegen or compatibility path */
					return false;
				}
				Channels.Add(BaseValue);
			}

			if (Channels.Num() == 1)
			{
				OutValue = Channels[0];
				return true;
			}

			if (!AppendValues(Channels, OutValue, OutError))
			{
				return false;
			}

			OutValue.ComponentCount = Channels.Num();
			return true;
		}

		for (int32 Index = 0; Index < Swizzle.Len(); ++Index)
		{
			int32 ChannelIndex = INDEX_NONE;
			if (!TryResolveSwizzleChannelIndex(Swizzle[Index], ChannelIndex) || ChannelIndex >= BaseValue.ComponentCount)
			{
				OutError = FString::Printf(TEXT("Swizzle '%s' is invalid for a value with %d components."), *Swizzle, BaseValue.ComponentCount); /* I18N-EXEMPT: deferred codegen or compatibility path */
				return false;
			}

			FCodeValue ChannelValue;
			if (!CreateSingleChannelMask(BaseValue, ChannelIndex, ChannelValue, OutError))
			{
				return false;
			}
			Channels.Add(ChannelValue);
		}

		if (Channels.Num() == 1)
		{
			OutValue = Channels[0];
			return true;
		}

		if (!AppendValues(Channels, OutValue, OutError))
		{
			return false;
		}

		OutValue.ComponentCount = Channels.Num();
		return true;
	}
}
