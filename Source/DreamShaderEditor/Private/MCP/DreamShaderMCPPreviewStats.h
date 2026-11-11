// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace UE::DreamShader::Editor::MCP
{
	/**
	 * What a preview shows, measured on the 8-bit pixels of its PNG. Three renders of the same mesh and camera go in:
	 * the material, the background alone, and the mesh's silhouette. The silhouette says which pixels the mesh covers;
	 * a covered pixel that still matches the background is one the material lets the background through.
	 *
	 * The thresholds are part of the endpoint's contract (Docs/tools/dreammcp.md lists them):
	 * a pixel is on the mesh when the silhouette render differs from the background by more than
	 * MeshThreshold in some channel; a mesh pixel is transparent when every channel is within TransparentTolerance
	 * of the background; near white is every channel at NearWhite or above; near black is every channel at NearBlack
	 * or below. Luminance is Rec. 709 weights on the 0-1 values of the PNG (its gamma-encoded values, as the image
	 * looks), not on scene light.
	 */
	struct FPreviewStatsThresholds
	{
		static constexpr int32 MeshThreshold = 32;
		static constexpr int32 TransparentTolerance = 4;
		static constexpr int32 NearWhite = 240;
		static constexpr int32 NearBlack = 12;
		/** A luminance standard deviation at or under this, with every channel as steady, is one flat colour. */
		static constexpr double FlatStdDev = 0.012;
		/** Frames whose mean per-pixel difference stays at or under this (0-255) are the same image. */
		static constexpr double SameFrameDifference = 0.5;
	};

	/** Sums over the pixels of one or more frames; Finish() turns them into the figures the result reports. */
	struct FPreviewStats
	{
		int64 Pixels = 0;
		int64 MeshPixels = 0;
		int64 TransparentPixels = 0;
		int64 ForegroundPixels = 0;
		int64 NearWhitePixels = 0;
		int64 NearBlackPixels = 0;
		double Sum[3] = { 0.0, 0.0, 0.0 };
		double SumSquares[3] = { 0.0, 0.0, 0.0 };
		double LumaSum = 0.0;
		double LumaSumSquares = 0.0;
		double LumaMin = 1.0;
		double LumaMax = 0.0;

		/** Adds one frame. The three arrays are the same size: the material, the background, the silhouette. */
		void Add(TConstArrayView<FColor> Frame, TConstArrayView<FColor> Background, TConstArrayView<FColor> Silhouette);

		/** 0-1 sRGB-encoded mean of the foreground (the mesh pixels that are not transparent). */
		FLinearColor GetMeanColor() const;
		double GetLumaMean() const;
		double GetLumaStdDev() const;
		/** The largest of the three channels' standard deviations, 0-1. */
		double GetChannelStdDev() const;
		/** Share of the image the mesh covers, 0-1. */
		double GetCoverage() const;
		/** Share of the mesh that shows the background, 0-1. */
		double GetTransparentShare() const;
		/** Share of the foreground that is near white / near black, 0-1. */
		double GetNearWhiteShare() const;
		double GetNearBlackShare() const;
		bool IsFlat() const;

		TSharedRef<FJsonObject> ToJson() const;
	};

	/** "#RRGGBB" of a 0-1 sRGB-encoded colour. */
	FString FormatHexColor(const FLinearColor& Color);

	/** A plain name for a 0-1 sRGB-encoded colour: "orange", "dark blue", "light grey", "white". */
	FString NameColor(const FLinearColor& Color);

	/** The mean absolute difference between two frames of the same size, per channel, 0-255. */
	double MeanFrameDifference(TConstArrayView<FColor> A, TConstArrayView<FColor> B);

	/**
	 * The one fact about a preview a model should know first, as a sentence without its full stop, and what to do
	 * about it in OutNext (nothing when it looks like an ordinary render). bLinear: the image was drawn with
	 * tone="linear", so white means a value at or above 1 rather than a bright, tone-mapped one.
	 */
	FString DescribePreview(const FPreviewStats& Stats, bool bLinear, TArray<FString>& OutNext);
}
