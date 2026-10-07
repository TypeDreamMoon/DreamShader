// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "Math/UnrealMathUtility.h"

namespace UE::DreamShader::IR::Private::ConstantMath
{
	// Sine/Cosine graph nodes retain their default Period = 1. Both folders take cycles too.
	inline double Sin(double Value)
	{
		return FMath::Sin(Value * (2.0 * UE_PI));
	}

	inline double Cos(double Value)
	{
		return FMath::Cos(Value * (2.0 * UE_PI));
	}

	inline double Round(double Value)
	{
		// HLSL uses exact halfway ties to even. FMath::RoundHalfToEven first snaps values near
		// a half and casts the magnitude to uint64; neither is suitable for arbitrary constants.
		if (!FMath::IsFinite(Value))
		{
			return Value;
		}
		const double Lower = FMath::FloorToDouble(Value);
		const double Fraction = Value - Lower;
		if (Fraction < 0.5)
		{
			return Lower;
		}
		if (Fraction > 0.5)
		{
			return Lower + 1.0;
		}
		return FMath::Fmod(Lower, 2.0) == 0.0 ? Lower : Lower + 1.0;
	}
}
