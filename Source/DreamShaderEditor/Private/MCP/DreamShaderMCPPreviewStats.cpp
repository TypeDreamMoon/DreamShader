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
		if (Saturation < 0.12f)
		{
			if (Value > 0.92f)
			{
				return TEXT("white");
			}
			if (Value > 0.7f)
			{
				return TEXT("light grey");
			}
			return Value < 0.4f ? TEXT("dark grey") : TEXT("grey");
		}

		enum class EHue : uint8 { Red, Orange, Yellow, Green, Cyan, Blue, Purple, Magenta };
		EHue Family = EHue::Red;
		if (Hue >= 15.0f && Hue < 40.0f) { Family = EHue::Orange; }
		else if (Hue >= 40.0f && Hue < 65.0f) { Family = EHue::Yellow; }
		else if (Hue >= 65.0f && Hue < 160.0f) { Family = EHue::Green; }
		else if (Hue >= 160.0f && Hue < 195.0f) { Family = EHue::Cyan; }
		else if (Hue >= 195.0f && Hue < 255.0f) { Family = EHue::Blue; }
		else if (Hue >= 255.0f && Hue < 285.0f) { Family = EHue::Purple; }
		else if (Hue >= 285.0f && Hue < 345.0f) { Family = EHue::Magenta; }

		// A muted colour is a grey with a tint: "red" for a dusty pink overstates it.
		if (Saturation < 0.4f)
		{
			const bool bLight = Value > 0.55f;
			const TCHAR* Tint = TEXT("");
			switch (Family)
			{
			case EHue::Red:     Tint = bLight ? TEXT("pinkish") : TEXT("reddish"); break;
			case EHue::Orange:  Tint = bLight ? TEXT("warm") : TEXT("brownish"); break;
			case EHue::Yellow:  Tint = TEXT("yellowish"); break;
			case EHue::Green:   Tint = TEXT("greenish"); break;
			case EHue::Cyan:    Tint = TEXT("bluish-green"); break;
			case EHue::Blue:    Tint = TEXT("bluish"); break;
			case EHue::Purple:  Tint = TEXT("purplish"); break;
			case EHue::Magenta: Tint = bLight ? TEXT("pinkish") : TEXT("purplish"); break;
			}
			const TCHAR* Lightness = Value > 0.8f ? TEXT("light ") : (Value < 0.4f ? TEXT("dark ") : TEXT(""));
			return FString::Printf(TEXT("%s%s grey"), Lightness, Tint);
		}

		// Dark orange and dark yellow are what anyone calls brown.
		if ((Family == EHue::Orange || Family == EHue::Yellow) && Value < 0.5f)
		{
			return Value < 0.3f ? TEXT("dark brown") : TEXT("brown");
		}
		const TCHAR* Name = TEXT("red");
		switch (Family)
		{
		case EHue::Red:     Name = TEXT("red"); break;
		case EHue::Orange:  Name = TEXT("orange"); break;
		case EHue::Yellow:  Name = TEXT("yellow"); break;
		case EHue::Green:   Name = TEXT("green"); break;
		case EHue::Cyan:    Name = TEXT("cyan"); break;
		case EHue::Blue:    Name = TEXT("blue"); break;
		case EHue::Purple:  Name = TEXT("purple"); break;
		case EHue::Magenta: Name = TEXT("magenta"); break;
		}
		if (Value < 0.35f)
		{
			return FString::Printf(TEXT("dark %s"), Name);
		}
		if (Saturation < 0.5f && Value > 0.8f)
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
	namespace
	{
		/** The findings that say something is wrong, in the order they win; false when none holds. */
		bool DescribeProblem(const FPreviewStats& Stats, bool bLinear, FString& OutFinding, TArray<FString>& OutNext)
		{
			if (Stats.MeshPixels * 200 < Stats.Pixels)
			{
				OutNext.Add(TEXT("Check yaw, pitch and mesh. A World Position Offset can also move the mesh out of view."));
				OutFinding = FString::Printf(TEXT("The mesh is not in the picture: it covers %.1f%% of the image"), Stats.GetCoverage() * 100.0); // I18N-EXEMPT: wire text
				return true;
			}
			if (Stats.GetTransparentShare() >= 0.95)
			{
				OutNext.Add(TEXT("Check Opacity, OpacityMask and the blend mode. At opacity 0 a Translucent or Masked material draws nothing."));
				OutFinding = FString::Printf(TEXT("The material is invisible: %d%% of the mesh shows the background through it"), Percent(Stats.GetTransparentShare())); // I18N-EXEMPT: wire text
				return true;
			}
			if (Stats.GetNearBlackShare() >= 0.9)
			{
				OutNext.Add(TEXT("An Unlit material shows only EmissiveColor; a lit one needs BaseColor. Check that the export writes the one its shading model uses."));
				OutFinding = FString::Printf(TEXT("The material renders black: %d%% of what shows is near black"), Percent(Stats.GetNearBlackShare())); // I18N-EXEMPT: wire text
				return true;
			}
			if (Stats.GetNearWhiteShare() >= 0.3)
			{
				if (bLinear)
				{
					OutNext.Add(TEXT("tone=\"linear\" clips at 1. Lower the value if it should not reach 1."));
					OutFinding = FString::Printf(TEXT("%d%% of the material is clipped to white: its output is 1 or more there"), Percent(Stats.GetNearWhiteShare())); // I18N-EXEMPT: wire text
					return true;
				}
				OutNext.Add(TEXT("The preview is tone-mapped: bright values are pressed toward white (emissive 1 reads about 208 of 255, 4 about 240). Preview with tone=\"linear\" to read the values, or lower the emissive."));
				OutFinding = FString::Printf(TEXT("The material is mostly white: %d%% of it is near white"), Percent(Stats.GetNearWhiteShare())); // I18N-EXEMPT: wire text
				return true;
			}
			return false;
		}
	}

	FString DescribePreview(const FPreviewStats& Stats, bool bLinear, TArray<FString>& OutNext)
	{
		FString Problem;
		if (DescribeProblem(Stats, bLinear, Problem, OutNext))
		{
			return Problem;
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

	// ----------------------------------------------------------------------------------------- animation

	FPreviewAnimation AnalysePreviewAnimation(TConstArrayView<TArray<FColor>> Frames, TConstArrayView<FPreviewStats> PerFrame)
	{
		FPreviewAnimation Animation;
		Animation.Frames = Frames.Num();
		for (int32 Index = 1; Index < Frames.Num(); ++Index)
		{
			Animation.MaxDifference = FMath::Max(Animation.MaxDifference, MeanFrameDifference(Frames[Index], Frames[0]));
		}
		Animation.bChanges = Animation.MaxDifference > FThresholds::SameFrameDifference;

		TArray<double> Adjacent;
		for (int32 Index = 0; Index + 1 < Frames.Num(); ++Index)
		{
			Adjacent.Add(MeanFrameDifference(Frames[Index], Frames[Index + 1]));
		}
		double Direct = 0.0;
		double Path = 0.0;
		for (int32 Index = 0; Index + 2 < Frames.Num(); ++Index)
		{
			Direct += MeanFrameDifference(Frames[Index], Frames[Index + 2]);
			Path += Adjacent[Index] + Adjacent[Index + 1];
		}
		Animation.Straightness = Path > 0.0 ? Direct / Path : 1.0;
		double AdjacentMean = 0.0;
		for (const double Step : Adjacent)
		{
			AdjacentMean += Step / double(Adjacent.Num());
		}
		// Noise in a strip that barely moves zigzags too; only a change worth seeing is judged.
		Animation.bUndersampled = Animation.bChanges && Frames.Num() >= 4 && AdjacentMean >= 1.0
			&& Animation.Straightness < FPreviewAnimation::UndersampledStraightness;

		double Darkest = TNumericLimits<double>::Max();
		double Brightest = -1.0;
		for (int32 Index = 0; Index < PerFrame.Num(); ++Index)
		{
			if (PerFrame[Index].ForegroundPixels <= 0)
			{
				continue;
			}
			const double Luma = PerFrame[Index].GetLumaMean();
			if (Luma < Darkest)
			{
				Darkest = Luma;
				Animation.Darkest = Index;
			}
			if (Luma > Brightest)
			{
				Brightest = Luma;
				Animation.Brightest = Index;
			}
		}
		return Animation;
	}

	namespace
	{
		FString NameFrame(const FPreviewStats& Stats)
		{
			return Stats.ForegroundPixels > 0 ? NameColor(Stats.GetMeanColor()) : FString(TEXT("nothing visible"));
		}

		FString DescribeFrame(const FPreviewStats& Stats, double Time)
		{
			if (Stats.ForegroundPixels <= 0)
			{
				return FString::Printf(TEXT("at %g s, nothing visible"), Time); // I18N-EXEMPT: wire text
			}
			const FLinearColor Mean = Stats.GetMeanColor();
			return FString::Printf(TEXT("at %g s, %s (%s)"), Time, *FormatHexColor(Mean), *NameColor(Mean)); // I18N-EXEMPT: wire text
		}
	}

	FString DescribePreviewAnimation(const FPreviewAnimation& Animation, const FPreviewStats& Overall, TConstArrayView<FPreviewStats> PerFrame,
		TConstArrayView<double> Times, double Duration, bool bLinear, TArray<FString>& OutNext)
	{
		if (PerFrame.IsEmpty() || PerFrame.Num() != Times.Num())
		{
			return FString();
		}
		if (!Animation.bChanges)
		{
			const FString First = DescribePreview(PerFrame[0], bLinear, OutNext);
			OutNext.Add(TEXT("If it should move, check that Time (or a Panner) reaches the output, or set duration to one period of it."));
			return FString::Printf(TEXT("%s. The %d frames over %g s are the same: nothing animates"), *First, Animation.Frames, Duration); // I18N-EXEMPT: wire text
		}

		// What holds for every frame together still comes first: an invisible strip is invisible whatever it does.
		FString Sentence;
		FString Problem;
		if (DescribeProblem(Overall, bLinear, Problem, OutNext))
		{
			Sentence = Problem + TEXT(". ");
		}

		TArray<FString> Names;
		for (const FPreviewStats& Frame : PerFrame)
		{
			const FString Name = NameFrame(Frame);
			if (Names.IsEmpty() || Names.Last() != Name)
			{
				Names.Add(Name);
			}
		}
		Sentence += FString::Printf(TEXT("It animates over %d frames in %g s: darkest %s, brightest %s"), Animation.Frames, Duration, // I18N-EXEMPT: wire text
			*DescribeFrame(PerFrame[Animation.Darkest], Times[Animation.Darkest]), *DescribeFrame(PerFrame[Animation.Brightest], Times[Animation.Brightest]));
		Sentence += Names.Num() > 1
			? FString::Printf(TEXT("; the frames in order are %s"), *FString::Join(Names, TEXT(", "))) // I18N-EXEMPT: wire text
			: FString::Printf(TEXT("; every frame is %s, only its brightness changes"), *Names[0]); // I18N-EXEMPT: wire text
		if (Animation.bUndersampled)
		{
			Sentence += TEXT("; it jumps back and forth between frames, so it may change faster than they sample it");
			OutNext.Add(FString::Printf(TEXT("The material may change faster than %d frames in %g s can show. Shorten duration, or raise frames, until neighbouring frames look alike."), // I18N-EXEMPT: wire text
				Animation.Frames, Duration));
		}
		return Sentence;
	}

	TSharedRef<FJsonObject> PreviewAnimationToJson(const FPreviewAnimation& Animation, TConstArrayView<FPreviewStats> PerFrame,
		TConstArrayView<double> Times, double Duration)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("frames"), Animation.Frames);
		Json->SetNumberField(TEXT("duration"), Duration);
		Json->SetNumberField(TEXT("maxDifference"), FMath::RoundToDouble(Animation.MaxDifference * 100.0) / 100.0);
		Json->SetBoolField(TEXT("changes"), Animation.bChanges);
		Json->SetNumberField(TEXT("straightness"), Round3(Animation.Straightness));
		Json->SetBoolField(TEXT("undersampled"), Animation.bUndersampled);
		const auto Frame = [&PerFrame, &Times](int32 Index)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetNumberField(TEXT("index"), Index);
			if (Times.IsValidIndex(Index))
			{
				Object->SetNumberField(TEXT("time"), FMath::RoundToDouble(Times[Index] * 10000.0) / 10000.0);
			}
			if (PerFrame.IsValidIndex(Index) && PerFrame[Index].ForegroundPixels > 0)
			{
				const FLinearColor Mean = PerFrame[Index].GetMeanColor();
				Object->SetStringField(TEXT("meanColor"), FormatHexColor(Mean));
				Object->SetStringField(TEXT("meanColorName"), NameColor(Mean));
				Object->SetNumberField(TEXT("luminance"), Round3(PerFrame[Index].GetLumaMean()));
			}
			return Object;
		};
		Json->SetObjectField(TEXT("darkest"), Frame(Animation.Darkest));
		Json->SetObjectField(TEXT("brightest"), Frame(Animation.Brightest));
		return Json;
	}
}
