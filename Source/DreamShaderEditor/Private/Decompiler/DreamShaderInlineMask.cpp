// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "Decompiler/DreamShaderInlineMask.h"

#include "Materials/MaterialExpression.h"

namespace UE::DreamShader::Editor::Private
{
	FDreamShaderInlineMask ResolveDreamShaderInlineMask(
		const FExpressionInput& Input,
		const FExpressionOutput* SourceOutput,
		const int32 UnmaskedOperandWidth)
	{
		FDreamShaderInlineMask Result;
		if (Input.Mask == 0)
		{
			return Result;
		}
		Result.bMasked = true;

		// The operand's channels: a masked output's own, else every channel of its width.
		TArray<int32, TInlineAllocator<4>> Channels;
		if (SourceOutput && SourceOutput->Mask != 0)
		{
			if (SourceOutput->MaskR) { Channels.Add(0); }
			if (SourceOutput->MaskG) { Channels.Add(1); }
			if (SourceOutput->MaskB) { Channels.Add(2); }
			if (SourceOutput->MaskA) { Channels.Add(3); }
		}
		if (Channels.Num() == 0)
		{
			const int32 Width = FMath::Clamp(UnmaskedOperandWidth, 1, 4);
			for (int32 Channel = 0; Channel < Width; ++Channel)
			{
				Channels.Add(Channel);
			}
		}
		Result.OperandWidth = Channels.Num();

		const bool bPicked[4] = { Input.MaskR != 0, Input.MaskG != 0, Input.MaskB != 0, Input.MaskA != 0 };
		static const TCHAR* const Components = TEXT("xyzw");
		for (int32 Position = 0; Position < Channels.Num(); ++Position)
		{
			if (bPicked[Channels[Position]])
			{
				Result.Relative.AppendChar(Components[Position]);
			}
		}

		// A mask that keeps nothing the operand has (A of a float3) does not compile in the engine either. It is read as
		// the whole operand: a Swizzle with no components is not something the IR can hold.
		Result.bIdentity = Result.Relative.Len() == Channels.Num() || Result.Relative.IsEmpty();
		return Result;
	}
}
