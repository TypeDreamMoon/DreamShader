#include "Preview/DreamShaderPreviewSession.h"

#include "DreamShaderModule.h"

#include "Hash/xxhash.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		// A frame whose acknowledgement never arrives (client tab hidden mid-frame, message lost)
		// must not stall the stream forever; after this long the next frame goes out regardless.
		constexpr double AckTimeoutSeconds = 2.0;
		// After this many consecutive identical frames the session assumes the picture is static and
		// re-renders at IdleIntervalSeconds instead of the requested rate. Any control change, probe
		// change or in-progress compile resets it.
		constexpr int32 IdleFrameThreshold = 3;
		constexpr double IdleIntervalSeconds = 0.5;
	}

	FDreamShaderPreviewSession::FDreamShaderPreviewSession()
	{
		ProbePreview.OnProbeChanged.AddLambda([this]()
		{
			MarkDirty();
		});
	}

	FDreamShaderPreviewSession::~FDreamShaderPreviewSession() = default;

	bool FDreamShaderPreviewSession::BeginPreview(
		const FDreamShaderPreviewRequest& Request,
		const FString& InRequestId,
		const EDreamShaderPreviewFrameEncoding InEncoding,
		const double InFrameIntervalSeconds,
		const bool bStream,
		FDreamShaderPreviewResult& OutResult)
	{
		RequestId = InRequestId;
		Encoding = InEncoding;
		FrameIntervalSeconds = InFrameIntervalSeconds;
		Width = FMath::Clamp(Request.Width, 64, 2048);
		Height = FMath::Clamp(Request.Height, 64, 2048);
		Mesh = Request.Mesh.IsEmpty() ? TEXT("sphere") : Request.Mesh;
		OrbitYaw = Request.OrbitYaw;
		OrbitPitch = Request.OrbitPitch;

		UMaterialInterface* Material = nullptr;
		const bool bResolved = FDreamShaderPreviewRenderer::ResolvePreviewMaterial(Request, OutResult, Material);
		if (bResolved)
		{
			SourceFilePath = OutResult.SourceFilePath;
			AssetPath = OutResult.AssetPath;
			MainMaterial = Material;
			// Same source keeps its requested probe (it re-attached during the compile's publish, if
			// the compile regenerated anything); a different source starts clean.
			ProbePreview.SetSource(SourceFilePath);

			if (Encoding == EDreamShaderPreviewFrameEncoding::Png)
			{
				// Legacy clients read `imagePath` from previewResult / preview.json and expect it to
				// exist by then, which means the old synchronous first frame -- shader compile wait
				// and all. The raw path never blocks here: its first frames simply carry Compiling.
				FString RenderError;
				FString ImagePath;
				if (!FDreamShaderPreviewRenderer::SaveMaterialPreviewFrame(Material, SourceFilePath, Width, Height, Mesh, OrbitYaw, OrbitPitch, ImagePath, RenderError))
				{
					OutResult.bSucceeded = false;
					OutResult.Message = FText::FromString(RenderError);
				}
				else
				{
					OutResult.ImagePath = ImagePath;
				}
			}
		}

		const bool bReady = bResolved && OutResult.bSucceeded;
		FDreamShaderPreviewRenderer::WritePreviewResult(OutResult, bReady ? TEXT("ready") : TEXT("error"), RequestId);

		if (!bReady)
		{
			bStreaming = false;
			return false;
		}

		if (!RenderContext.IsValid())
		{
			RenderContext = MakeUnique<FDreamShaderPreviewRenderContext>();
		}
		bStreaming = bStream && FrameIntervalSeconds > 0.0;
		bFrameInFlight = false;
		LastKickoffSeconds = 0.0;
		MarkDirty();
		return true;
	}

	void FDreamShaderPreviewSession::SetStreaming(const bool bInStreaming, const double InFrameIntervalSeconds)
	{
		FrameIntervalSeconds = InFrameIntervalSeconds;
		const bool bWasStreaming = bStreaming;
		bStreaming = bInStreaming && FrameIntervalSeconds > 0.0 && HasMaterial();
		if (bStreaming && !bWasStreaming)
		{
			// Coming back from hidden: whatever the client last saw may be stale.
			MarkDirty();
		}
	}

	void FDreamShaderPreviewSession::SetOrbit(const float InOrbitYaw, const float InOrbitPitch)
	{
		if (InOrbitYaw != OrbitYaw || InOrbitPitch != OrbitPitch)
		{
			OrbitYaw = InOrbitYaw;
			OrbitPitch = InOrbitPitch;
			MarkDirty();
		}
	}

	void FDreamShaderPreviewSession::SetViewportSize(const int32 InWidth, const int32 InHeight)
	{
		const int32 NewWidth = FMath::Clamp(InWidth, 64, 2048);
		const int32 NewHeight = FMath::Clamp(InHeight, 64, 2048);
		if (NewWidth != Width || NewHeight != Height)
		{
			Width = NewWidth;
			Height = NewHeight;
			MarkDirty();
		}
	}

	void FDreamShaderPreviewSession::SetMesh(const FString& InMesh)
	{
		const FString NewMesh = InMesh.IsEmpty() ? TEXT("sphere") : InMesh;
		if (!NewMesh.Equals(Mesh, ESearchCase::IgnoreCase))
		{
			Mesh = NewMesh;
			MarkDirty();
		}
	}

	void FDreamShaderPreviewSession::AckFrame(const int32 FrameIndex)
	{
		(void)FrameIndex;
		bFrameInFlight = false;
	}

	bool FDreamShaderPreviewSession::SetProbe(const int32 Line, const FString& PreferredName, FString& OutError)
	{
		if (SourceFilePath.IsEmpty())
		{
			OutError = TEXT("No source is being previewed."); // I18N-EXEMPT: reaches the preview wire
			return false;
		}
		ProbePreview.SetSource(SourceFilePath);
		return ProbePreview.SetProbe(Line, PreferredName, OutError);
	}

	void FDreamShaderPreviewSession::ClearProbe()
	{
		ProbePreview.ClearProbe();
	}

	bool FDreamShaderPreviewSession::HasMaterial()
	{
		return GetRenderMaterial() != nullptr;
	}

	UMaterialInterface* FDreamShaderPreviewSession::GetRenderMaterial()
	{
		if (UMaterialInterface* ProbeMaterial = ProbePreview.GetPreviewMaterial())
		{
			return ProbeMaterial;
		}
		if (!MainMaterial.IsValid() && !AssetPath.IsEmpty())
		{
			// The generated material lives in memory only; a regeneration may have replaced the
			// object behind this path. FindObject, not LoadObject: there is nothing on disk to load
			// and a failed load would log every tick.
			MainMaterial = FindObject<UMaterialInterface>(nullptr, *AssetPath);
		}
		return MainMaterial.Get();
	}

	void FDreamShaderPreviewSession::MarkDirty()
	{
		bForceNextFrame = true;
		IdenticalFrameRun = 0;
	}

	bool FDreamShaderPreviewSession::IsMaterialCompiling(UMaterialInterface* Material)
	{
		return Material && Material->IsCompiling();
	}

	bool FDreamShaderPreviewSession::BuildFrame(TArray<uint8>&& Payload, const int32 FrameWidth, const int32 FrameHeight, const uint32 Flags, FDreamShaderPreviewFrame& OutFrame)
	{
		OutFrame.Encoding = Encoding;
		OutFrame.Width = FrameWidth;
		OutFrame.Height = FrameHeight;
		OutFrame.FrameIndex = NextFrameIndex++;
		OutFrame.Flags = Flags;
		OutFrame.OrbitYaw = OrbitYaw;
		OutFrame.OrbitPitch = OrbitPitch;
		OutFrame.ProbeLine = ProbePreview.GetResolvedProbe().IsSet() ? ProbePreview.GetResolvedProbe()->Line : 0;
		OutFrame.Payload = MoveTemp(Payload);
		return true;
	}

	bool FDreamShaderPreviewSession::Tick(const double NowSeconds, FDreamShaderPreviewFrame& OutFrame, FString& OutError)
	{
		OutError.Reset();
		if (!bStreaming || !RenderContext.IsValid())
		{
			return false;
		}

		UMaterialInterface* Material = GetRenderMaterial();
		if (!Material)
		{
			OutError = TEXT("Preview material is not valid."); // I18N-EXEMPT: reaches the preview wire
			bStreaming = false;
			return false;
		}

		// A frame kicked off earlier is being rendered / read back on the GPU. Poll without blocking;
		// this can take several ticks and every one of them returns immediately.
		if (RenderContext->IsReadbackInFlight())
		{
			TArray<uint8> Payload;
			int32 FrameWidth = Width;
			int32 FrameHeight = Height;
			FString Error;
			bool bReady = false;
			if (Encoding == EDreamShaderPreviewFrameEncoding::Png)
			{
				TArray64<uint8> PngData;
				bReady = RenderContext->TryConsumeReadyFrame(PngData, Error);
				if (bReady)
				{
					Payload.Append(PngData.GetData(), static_cast<int32>(PngData.Num()));
				}
			}
			else
			{
				bReady = RenderContext->TryConsumeReadyFramePixels(Payload, FrameWidth, FrameHeight, Error);
			}

			if (!bReady)
			{
				if (!Error.IsEmpty())
				{
					OutError = Error;
					bStreaming = false;
				}
				return false;
			}

			// Identical pixels are not worth a send. The compare is on the encoded payload, so a
			// PNG session dedupes too (PNG is deterministic for identical input).
			const uint64 Hash = FXxHash64::HashBuffer(Payload.GetData(), Payload.Num()).Hash;
			const bool bChanged = !bHasSentFrame || Hash != LastSentFrameHash;
			if (!bChanged && !bForceNextFrame)
			{
				++IdenticalFrameRun;
				return false;
			}

			uint32 Flags = EDreamShaderPreviewFrameFlags::None;
			if (IsMaterialCompiling(Material))
			{
				Flags |= EDreamShaderPreviewFrameFlags::Compiling;
			}
			if (ProbePreview.IsActive())
			{
				Flags |= EDreamShaderPreviewFrameFlags::ProbeActive;
			}
			else if (ProbePreview.IsRequested())
			{
				Flags |= EDreamShaderPreviewFrameFlags::ProbePending;
			}
			if (bForceNextFrame)
			{
				Flags |= EDreamShaderPreviewFrameFlags::Keyframe;
			}

			BuildFrame(MoveTemp(Payload), FrameWidth, FrameHeight, Flags, OutFrame);
			IdenticalFrameRun = bChanged ? 0 : IdenticalFrameRun + 1;
			LastSentFrameHash = Hash;
			bHasSentFrame = true;
			bForceNextFrame = false;
			bFrameInFlight = true;
			FrameInFlightSinceSeconds = NowSeconds;
			return true;
		}

		// Nothing in flight on the GPU. Start the next render once the client has acknowledged the
		// last frame (or that ack is overdue) and the pacing clock says so.
		const bool bAckOverdue = bFrameInFlight && (NowSeconds - FrameInFlightSinceSeconds) > AckTimeoutSeconds;
		if (bFrameInFlight && !bAckOverdue)
		{
			return false;
		}

		double Interval = FrameIntervalSeconds;
		const bool bIdle = IdenticalFrameRun >= IdleFrameThreshold && !bForceNextFrame && !IsMaterialCompiling(Material);
		if (bIdle)
		{
			Interval = FMath::Max(Interval, IdleIntervalSeconds);
		}
		if (NowSeconds - LastKickoffSeconds < Interval)
		{
			return false;
		}

		LastKickoffSeconds = NowSeconds;
		bFrameInFlight = false;
		FString Error;
		if (!RenderContext->KickoffFrame(Material, Width, Height, Mesh, OrbitYaw, OrbitPitch, Error))
		{
			OutError = Error;
			bStreaming = false;
		}
		return false;
	}
}
