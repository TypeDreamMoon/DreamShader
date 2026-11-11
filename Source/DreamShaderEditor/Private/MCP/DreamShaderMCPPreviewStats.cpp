// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "MCP/DreamShaderMCPPreviewStats.h"

namespace UE::DreamShader::Editor::MCP
{
	namespace
	{
		using FThresholds = FPreviewStatsThresholds;

		int32 MaxChannelDifference(const FColor& A, const FColor& B)
		{
			return FMath::Max3(FMath::Abs(int32(A.R) - int32(B.R)), FMath::Abs(int32(A.G) - int32(B.G)), FMath::Abs(int32(A.B) - int32(B.B)));
		}

		double Luma(double R, double G, double B)
		{
			return 0.2126 * R + 0.7152 * G + 0.0722 * B;
		}

		double StdDev(double Sum, double SumSquares, int64 Count)
		{
			if (Count <= 0)
			{
				return 0.0;
			}
			const double Mean = Sum / double(Count);
			return FMath::Sqrt(FMath::Max(0.0, SumSquares / double(Count) - Mean * Mean));
		}

		/** Three decimals: enough for a share or a 0-1 value, and the JSON stays short. */
		double Round3(double Value)
		{
			return FMath::RoundToDouble(Value * 1000.0) / 1000.0;
		}

		int32 Percent(double Share)
		{
			return FMath::RoundToInt(Share * 100.0);
		}
	}

	void FPreviewStats::Add(TConstArrayView<FColor> Frame, TConstArrayView<FColor> Background, TConstArrayView<FColor> Silhouette)
	{
		if (Frame.Num() != Background.Num() || Frame.Num() != Silhouette.Num())
		{
			return;
		}

		for (int32 Index = 0; Index < Frame.Num(); ++Index)
		{
			++Pixels;
			if (MaxChannelDifference(Silhouette[Index], Background[Index]) <= FThresholds::MeshThreshold)
			{
				continue;
			}
			++MeshPixels;

			const FColor& Pixel = Frame[Index];
			if (MaxChannelDifference(Pixel, Background[Index]) <= FThresholds::TransparentTolerance)
			{
				++TransparentPixels;
				continue;
			}
			++ForegroundPixels;

			const double Channels[3] = { Pixel.R / 255.0, Pixel.G / 255.0, Pixel.B / 255.0 };
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				Sum[Channel] += Channels[Channel];
				SumSquares[Channel] += Channels[Channel] * Channels[Channel];
			}
			const double Y = Luma(Channels[0], Channels[1], Channels[2]);
			LumaSum += Y;
			LumaSumSquares += Y * Y;
			LumaMin = FMath::Min(LumaMin, Y);
			LumaMax = FMath::Max(LumaMax, Y);

			if (Pixel.R >= FThresholds::NearWhite && Pixel.G >= FThresholds::NearWhite && Pixel.B >= FThresholds::NearWhite)
			{
				++NearWhitePixels;
			}
			if (Pixel.R <= FThresholds::NearBlack && Pixel.G <= FThresholds::NearBlack && Pixel.B <= FThresholds::NearBlack)
			{
				++NearBlackPixels;
			}
		}
	}

	FLinearColor FPreviewStats::GetMeanColor() const
	{
		if (ForegroundPixels <= 0)
		{
			return FLinearColor::Black;
		}
		const double Count = double(ForegroundPixels);
		return FLinearColor(float(Sum[0] / Count), float(Sum[1] / Count), float(Sum[2] / Count), 1.0f);
	}

	double FPreviewStats::GetLumaMean() const
	{
		return ForegroundPixels > 0 ? LumaSum / double(ForegroundPixels) : 0.0;
	}

	double FPreviewStats::GetLumaStdDev() const
	{
		return StdDev(LumaSum, LumaSumSquares, ForegroundPixels);
	}

	double FPreviewStats::GetChannelStdDev() const
	{
		return FMath::Max3(StdDev(Sum[0], SumSquares[0], ForegroundPixels), StdDev(Sum[1], SumSquares[1], ForegroundPixels),
			StdDev(Sum[2], SumSquares[2], ForegroundPixels));
	}

	double FPreviewStats::GetCoverage() const
	{
		return Pixels > 0 ? double(MeshPixels) / double(Pixels) : 0.0;
	}

	double FPreviewStats::GetTransparentShare() const
	{
		return MeshPixels > 0 ? double(TransparentPixels) / double(MeshPixels) : 0.0;
	}

	double FPreviewStats::GetNearWhiteShare() const
	{
		return ForegroundPixels > 0 ? double(NearWhitePixels) / double(ForegroundPixels) : 0.0;
	}

	double FPreviewStats::GetNearBlackShare() const
	{
		return ForegroundPixels > 0 ? double(NearBlackPixels) / double(ForegroundPixels) : 0.0;
	}

	bool FPreviewStats::IsFlat() const
	{
		return ForegroundPixels > 0 && GetLumaStdDev() <= FThresholds::FlatStdDev && GetChannelStdDev() <= FThresholds::FlatStdDev;
	}

	TSharedRef<FJsonObject> FPreviewStats::ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("coverage"), Round3(GetCoverage()));
		Json->SetNumberField(TEXT("transparent"), Round3(GetTransparentShare()));
		Json->SetNumberField(TEXT("foreground"), Round3(Pixels > 0 ? double(ForegroundPixels) / double(Pixels) : 0.0));
		if (ForegroundPixels > 0)
		{
			const FLinearColor Mean = GetMeanColor();
			Json->SetStringField(TEXT("meanColor"), FormatHexColor(Mean));
			Json->SetStringField(TEXT("meanColorName"), NameColor(Mean));

			TSharedRef<FJsonObject> Luminance = MakeShared<FJsonObject>();
			Luminance->SetNumberField(TEXT("min"), Round3(LumaMin));
			Luminance->SetNumberField(TEXT("max"), Round3(LumaMax));
			Luminance->SetNumberField(TEXT("mean"), Round3(GetLumaMean()));
			Luminance->SetNumberField(TEXT("stdDev"), Round3(GetLumaStdDev()));
			Json->SetObjectField(TEXT("luminance"), Luminance);

			Json->SetNumberField(TEXT("nearWhite"), Round3(GetNearWhiteShare()));
			Json->SetNumberField(TEXT("nearBlack"), Round3(GetNearBlackShare()));
			Json->SetBoolField(TEXT("flat"), IsFlat());
		}
		return Json;
	}

	FString FormatHexColor(const FLinearColor& Color)
	{
		const auto Byte = [](float Value) { return FMath::Clamp(FMath::RoundToInt(Value * 255.0f), 0, 255); };
		return FString::Printf(TEXT("#%02X%02X%02X"), Byte(Color.R), Byte(Color.G), Byte(Color.B));
	}

	FString NameColor(const FLinearColor& Color)
	{
		// The HSV split is plain arithmetic on whatever values go in; here they are the image's encoded values.
		const FLinearColor Hsv = Color.LinearRGBToHSV();
		const float Hue = Hsv.R;
		const float Saturation = Hsv.G;
		const float Value = Hsv.B;

		if (Value < 0.08f)
		{
			return TEXT("black");
		}
		if (Saturation < 0.15f)
		{
			if (Value > 0.9f)
			{
				return TEXT("white");
			}
			if (Value > 0.65f)
			{
				return TEXT("light grey");
			}
			return Value < 0.3f ? TEXT("dark grey") : TEXT("grey");
		}

		const TCHAR* Name = TEXT("red");
		if (Hue >= 15.0f && Hue < 40.0f) { Name = TEXT("orange"); }
		else if (Hue >= 40.0f && Hue < 65.0f) { Name = TEXT("yellow"); }
		else if (Hue >= 65.0f && Hue < 160.0f) { Name = TEXT("green"); }
		else if (Hue >= 160.0f && Hue < 195.0f) { Name = TEXT("cyan"); }
		else if (Hue >= 195.0f && Hue < 255.0f) { Name = TEXT("blue"); }
		else if (Hue >= 255.0f && Hue < 285.0f) { Name = TEXT("purple"); }
		else if (Hue >= 285.0f && Hue < 335.0f) { Name = TEXT("magenta"); }

		if (Value < 0.35f)
		{
			return FString::Printf(TEXT("dark %s"), Name);
		}
		if (Saturation < 0.4f && Value > 0.75f)
		{
			return FString::Printf(TEXT("pale %s"), Name);
		}
		return Name;
	}

	double MeanFrameDifference(TConstArrayView<FColor> A, TConstArrayView<FColor> B)
	{
		if (A.Num() != B.Num() || A.Num() == 0)
		{
			return 0.0;
		}
		double Total = 0.0;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			Total += FMath::Abs(int32(A[Index].R) - int32(B[Index].R)) + FMath::Abs(int32(A[Index].G) - int32(B[Index].G))
				+ FMath::Abs(int32(A[Index].B) - int32(B[Index].B));
		}
		return Total / (3.0 * double(A.Num()));
	}

	// Wire text for a model: invariant English literals, like every summary and next step of the endpoint.
	FString DescribePreview(const FPreviewStats& Stats, bool bLinear, TArray<FString>& OutNext)
	{
		if (Stats.MeshPixels * 200 < Stats.Pixels)
		{
			OutNext.Add(TEXT("Check yaw, pitch and mesh. A World Position Offset can also move the mesh out of view."));
			return FString::Printf(TEXT("The mesh is not in the picture: it covers %.1f%% of the image"), Stats.GetCoverage() * 100.0); // I18N-EXEMPT: wire text
		}
		if (Stats.GetTransparentShare() >= 0.95)
		{
			OutNext.Add(TEXT("Check Opacity, OpacityMask and the blend mode. At opacity 0 a Translucent or Masked material draws nothing."));
			return FString::Printf(TEXT("The material is invisible: %d%% of the mesh shows the background through it"), Percent(Stats.GetTransparentShare())); // I18N-EXEMPT: wire text
		}
		if (Stats.GetNearBlackShare() >= 0.9)
		{
			OutNext.Add(TEXT("An Unlit material shows only EmissiveColor; a lit one needs BaseColor. Check that the export writes the one its shading model uses."));
			return FString::Printf(TEXT("The material renders black: %d%% of what shows is near black"), Percent(Stats.GetNearBlackShare())); // I18N-EXEMPT: wire text
		}
		if (Stats.GetNearWhiteShare() >= 0.3)
		{
			if (bLinear)
			{
				OutNext.Add(TEXT("tone=\"linear\" clips at 1. Lower the value if it should not reach 1."));
				return FString::Printf(TEXT("%d%% of the material is clipped to white: its output is 1 or more there"), Percent(Stats.GetNearWhiteShare())); // I18N-EXEMPT: wire text
			}
			OutNext.Add(TEXT("The preview is tone-mapped: bright values are pressed toward white (emissive 1 reads about 208 of 255, 4 about 240). Preview with tone=\"linear\" to read the values, or lower the emissive."));
			return FString::Printf(TEXT("The material is mostly white: %d%% of it is near white"), Percent(Stats.GetNearWhiteShare())); // I18N-EXEMPT: wire text
		}

		const FLinearColor Mean = Stats.GetMeanColor();
		FString Finding;
		if (Stats.IsFlat())
		{
			OutNext.Add(TEXT("If it should vary over the surface, check the UVs, textures or parameters that drive it."));
			Finding = FString::Printf(TEXT("The material is one flat colour, %s (%s)"), *FormatHexColor(Mean), *NameColor(Mean)); // I18N-EXEMPT: wire text
		}
		else
		{
			Finding = FString::Printf(TEXT("Mean colour %s (%s), luminance %.2f to %.2f"), *FormatHexColor(Mean), *NameColor(Mean), // I18N-EXEMPT: wire text
				Stats.LumaMin, Stats.LumaMax);
		}
		if (Stats.GetTransparentShare() >= 0.05)
		{
			Finding += FString::Printf(TEXT("; %d%% of the mesh is see-through"), Percent(Stats.GetTransparentShare())); // I18N-EXEMPT: wire text
		}
		return Finding;
	}
}
