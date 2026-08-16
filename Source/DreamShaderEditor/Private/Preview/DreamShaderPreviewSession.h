#pragma once

#include "CoreMinimal.h"
#include "Preview/DreamShaderPreviewRenderer.h"
#include "Preview/DreamShaderProbePreview.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtr.h"

class UMaterialInterface;

namespace UE::DreamShader::Editor::Private
{
	// How a streamed frame is encoded on the wire.
	enum class EDreamShaderPreviewFrameEncoding : uint8
	{
		// Legacy: a JSON `previewFrame` message followed by a PNG. Kept for clients that predate the
		// raw path; costs a PNG encode per frame on the game thread.
		Png,
		// Tightly packed RGBA8, sRGB-encoded, alpha 255 -- what a browser canvas wants, no encode/decode
		// on either side. The default for new clients (see FDreamShaderPreviewWebSocketServer).
		RawRGBA8,
	};

	// Frame-header flag bits (mirrored in the VS Code webview's decoder).
	namespace EDreamShaderPreviewFrameFlags
	{
		enum Type : uint32
		{
			None = 0,
			// The rendered material's shader map is still compiling; the pixels show the engine's
			// fallback material and a later frame will replace them.
			Compiling = 1u << 0,
			// A probe (breakpoint) is attached and this frame shows the probed value, not the material.
			ProbeActive = 1u << 1,
			// A probe was requested but could not attach (no table yet, line resolves to nothing, ...);
			// the frame shows the plain material.
			ProbePending = 1u << 2,
			// The frame was sent because something changed (compile, camera, probe, mesh, size), not
			// because the streaming clock ticked -- lets a client know the picture is authoritative.
			Keyframe = 1u << 3,
		};
	}

	struct FDreamShaderPreviewFrame
	{
		EDreamShaderPreviewFrameEncoding Encoding = EDreamShaderPreviewFrameEncoding::RawRGBA8;
		int32 Width = 0;
		int32 Height = 0;
		int32 FrameIndex = 0;
		uint32 Flags = 0;
		float OrbitYaw = 0.0f;
		float OrbitPitch = 0.0f;
		// The line the active probe resolved to (0 = none). Lets the client mark the line the pixels
		// belong to even if its own breakpoint sat on a blank line that snapped forward.
		int32 ProbeLine = 0;
		// PNG bytes or RGBA8 pixels, per Encoding.
		TArray<uint8> Payload;
	};

	// Everything the streaming server holds per connected client, and the whole "what to render this
	// tick" state machine, kept transport-agnostic so it can be driven from a test as easily as from
	// the WebSocket server.
	//
	// The Material Editor analogue: the session's main material is the editor's `Material` (what the
	// viewport shows), the probe preview is `ExpressionPreviewMaterial` + `PreviewExpression`, and
	// GetRenderMaterial() is SetPreviewMaterial()'s choice between them.
	class FDreamShaderPreviewSession
	{
	public:
		FDreamShaderPreviewSession();
		~FDreamShaderPreviewSession();

		FDreamShaderPreviewSession(const FDreamShaderPreviewSession&) = delete;
		FDreamShaderPreviewSession& operator=(const FDreamShaderPreviewSession&) = delete;

		// Compiles (or hash-skips) the request's source and points the session at the result. Does
		// NOT wait for the shader map -- streaming starts on the next Tick and the frames carry the
		// Compiling flag until the map lands. Returns the compile/resolve outcome in OutResult and
		// false when nothing can be streamed. A probe requested earlier for the same source is
		// re-attached automatically through the debug registry's publish.
		bool BeginPreview(const FDreamShaderPreviewRequest& Request, const FString& InRequestId, EDreamShaderPreviewFrameEncoding InEncoding, double FrameIntervalSeconds, bool bStream, FDreamShaderPreviewResult& OutResult);

		// Streaming controls. Each one that changes what a frame looks like marks the next frame as a
		// keyframe and resets the idle back-off (see Tick).
		void SetStreaming(bool bInStreaming, double FrameIntervalSeconds);
		void SetOrbit(float InOrbitYaw, float InOrbitPitch);
		void SetViewportSize(int32 InWidth, int32 InHeight);
		void SetMesh(const FString& InMesh);
		void AckFrame(int32 FrameIndex);

		// Breakpoints. `Line` is 1-based in the session's source file; `PreferredName` disambiguates
		// several bindings on one line. Attaches now when the source already has a debug table, or
		// as soon as the next generation publishes one.
		bool SetProbe(int32 Line, const FString& PreferredName, FString& OutError);
		void ClearProbe();
		const FDreamShaderProbePreview& GetProbePreview() const { return ProbePreview; }

		// Advances the session. Returns true with a frame to send when one is ready this tick; false
		// (with OutError empty) when there is nothing to send yet, false with OutError set when the
		// session hit a rendering error and stopped streaming.
		bool Tick(double NowSeconds, FDreamShaderPreviewFrame& OutFrame, FString& OutError);

		bool IsStreaming() const { return bStreaming; }
		bool HasMaterial();
		const FString& GetRequestId() const { return RequestId; }
		const FString& GetSourceFilePath() const { return SourceFilePath; }
		const FString& GetAssetPath() const { return AssetPath; }
		const FString& GetMesh() const { return Mesh; }
		EDreamShaderPreviewFrameEncoding GetEncoding() const { return Encoding; }
		double GetFrameIntervalSeconds() const { return FrameIntervalSeconds; }
		float GetOrbitYaw() const { return OrbitYaw; }
		float GetOrbitPitch() const { return OrbitPitch; }
		int32 GetWidth() const { return Width; }
		int32 GetHeight() const { return Height; }

		// The material a frame renders right now: the probe preview while a probe is attached, else
		// the generated material (re-resolved by object path if it was collected).
		UMaterialInterface* GetRenderMaterial();

	private:
		void MarkDirty();
		bool BuildFrame(TArray<uint8>&& Payload, int32 FrameWidth, int32 FrameHeight, uint32 Flags, FDreamShaderPreviewFrame& OutFrame);
		static bool IsMaterialCompiling(UMaterialInterface* Material);

		FString RequestId;
		FString SourceFilePath;
		FString AssetPath;
		FString Mesh = TEXT("sphere");
		TWeakObjectPtr<UMaterialInterface> MainMaterial;

		EDreamShaderPreviewFrameEncoding Encoding = EDreamShaderPreviewFrameEncoding::RawRGBA8;
		int32 Width = 512;
		int32 Height = 512;
		float OrbitYaw = -157.5f;
		float OrbitPitch = -11.25f;
		bool bStreaming = false;
		double FrameIntervalSeconds = 0.5;

		TUniquePtr<FDreamShaderPreviewRenderContext> RenderContext;
		FDreamShaderProbePreview ProbePreview;

		// Flow control: at most one frame between send and acknowledgement, with a timeout so a
		// dropped ack cannot wedge the stream.
		int32 NextFrameIndex = 0;
		bool bFrameInFlight = false;
		double FrameInFlightSinceSeconds = 0.0;
		double LastKickoffSeconds = 0.0;

		// Change detection: identical frames are not sent again, and after a few in a row the render
		// clock backs off to an idle rate until something marks the session dirty. Frames rendered
		// while a shader map compiles never back off -- the compile landing is exactly the change
		// being waited for.
		uint64 LastSentFrameHash = 0;
		bool bHasSentFrame = false;
		bool bForceNextFrame = true;
		int32 IdenticalFrameRun = 0;
	};
}
