#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Templates/UniquePtr.h"
#include "UObject/StrongObjectPtr.h"

class UTextureRenderTarget2D;
class UMaterialInterface;
class FMaterialThumbnailScene;
class FRHIGPUTextureReadback;

namespace UE::DreamShader::Editor::Private
{
	// Raw pixel data handed from the render thread (which owns the actual GPU readback) back to
	// the game thread via a TPromise/TFuture -- see FDreamShaderPreviewRenderContext::
	// TryConsumeReadyFrame(). Kept separate from PNG encoding, which happens back on the game
	// thread once the data arrives.
	struct FDreamShaderPreviewReadbackData
	{
		bool bSucceeded = false;
		FString Error;
		TArray<FColor> Colors;
		int32 Width = 0;
		int32 Height = 0;
	};

	struct FDreamShaderPreviewRequest
	{
		FString SourceFilePath;
		int32 Width = 512;
		int32 Height = 512;
		FString Mesh = TEXT("sphere");
		// Orbit camera angles in degrees, matching USceneThumbnailInfo::OrbitYaw/OrbitPitch's own
		// defaults (SceneThumbnailInfo.cpp) so a request that omits them renders identically to
		// today's default framing.
		float OrbitYaw = -157.5f;
		float OrbitPitch = -11.25f;
		// false = regenerate only when the source hash changed since the last generation (the same
		// skip the bridge's auto-compile applies), which is what a mesh/camera re-request or a save
		// the bridge already picked up wants; true = the explicit Refresh button / the one-shot
		// request-file path, which always rebuild.
		bool bForceRecompile = true;
	};

	struct FDreamShaderPreviewResult
	{
		bool bSucceeded = false;
		FString SourceFilePath;
		FString AssetPath;
		FString ImagePath;
		FString Mesh;
		FText Message;
	};

	// Owns a render target + thumbnail scene that persist across many streamed preview frames for
	// one client session, instead of allocating a fresh UTextureRenderTarget2D and
	// FMaterialThumbnailScene on every single frame. A continuously-streamed preview (e.g. a
	// material driven by Time/Panner nodes) calls RenderFrame()/KickoffFrame() every tick;
	// recreating both GPU resources from scratch each time was pure overhead unrelated to whether
	// the render itself needs to block. Resources are only recreated when the requested size
	// actually changes.
	class FDreamShaderPreviewRenderContext
	{
	public:
		FDreamShaderPreviewRenderContext();
		~FDreamShaderPreviewRenderContext();

		// Synchronous, one-shot render: blocks (FlushRenderingCommands + a blocking ReadPixels)
		// until the frame is fully read back. Fine for the one-off call sites (the initial preview
		// frame, the file-bridge fallback) that only render once per session; NOT used by the
		// continuously-streamed path below, which must not stall the game thread every tick. Mesh
		// selects the preview primitive shape ("sphere"/"plane"/"cube"/"cylinder"/"shaderball",
		// defaulting to sphere) by writing to the material's own ThumbnailInfo->PrimitiveType -- the
		// same field the native Material Editor's preview-shape toolbar button controls. OrbitYaw/
		// OrbitPitch (degrees) drive the preview camera the same way, via
		// ThumbnailInfo->OrbitYaw/OrbitPitch -- the same fields the Material Editor's own
		// drag-to-orbit preview viewport reads every frame (no caching to invalidate).
		// Takes a UMaterialInterface so a material INSTANCE (e.g. a ThinCustom
		// UDreamShaderMaterialInstance, which renders through its own static-permutation shader map)
		// can be previewed/pixel-compared too -- the thumbnail scene itself is interface-based.
		bool RenderFrame(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, TArray64<uint8>& OutPngData, FString& OutError);

		// Raw-pixel variant of RenderFrame(): the same synchronous render + blocking readback, but
		// hands back the BGRA8 pixels (alpha untouched) instead of PNG-encoding them. Exists for the
		// render-parity automation tests, which compare two materials pixel-by-pixel and have no use
		// for a lossless-compress/decompress round trip.
		bool RenderFramePixels(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, TArray<FColor>& OutColors, FString& OutError);

		// Renders the current material state and enqueues a non-blocking async GPU->CPU readback
		// (FRHIGPUTextureReadback), then returns immediately without waiting for either the render
		// or the copy to finish. Fails (returns false) if a previous readback from this context is
		// still in flight -- call TryConsumeReadyFrame() first. Bridges the same
		// EnsureResources()/RenderCurrentFrame() setup as RenderFrame() above.
		bool KickoffFrame(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, FString& OutError);

		// Non-blocking. Returns true and fills OutPngData once the frame kicked off by
		// KickoffFrame() has actually finished on the GPU and been PNG-encoded (usually a frame or
		// two later, once IsReadbackInFlight() was already true) -- returns false without
		// blocking if it's still pending, or if nothing was kicked off. Must be polled every tick
		// while IsReadbackInFlight() is true.
		bool TryConsumeReadyFrame(TArray64<uint8>& OutPngData, FString& OutError);

		// The raw twin of TryConsumeReadyFrame(): same polling contract, but hands back the pixels as
		// tightly packed RGBA8 (alpha forced to 255) instead of a PNG. This is the streaming wire
		// format -- a browser canvas takes it as-is (putImageData), and skipping the PNG encode is
		// most of what makes 30-60 FPS streaming possible; PNGCompressImageArray on a 512x512 frame
		// costs tens of milliseconds of game-thread time per frame. The render target is BGRA, so
		// this is one swizzle pass over the frame, no more than the alpha pass the PNG path already
		// paid. `OutWidth`/`OutHeight` are the frame's actual dimensions.
		bool TryConsumeReadyFramePixels(TArray<uint8>& OutRGBA, int32& OutWidth, int32& OutHeight, FString& OutError);

		bool IsReadbackInFlight() const { return bReadbackInFlight; }

	private:
		// Shared readback pump behind both TryConsume* entry points: polls the GPU copy, then moves
		// the staged BGRA pixels to the game thread. Returns true exactly once per kicked-off frame.
		bool TryConsumeReadyFrameColors(FDreamShaderPreviewReadbackData& OutData, FString& OutError);
		bool EnsureResources(int32 Width, int32 Height, FString& OutError);
		// Shared setup used by both RenderFrame() and KickoffFrame(): assigns the material,
		// submits the scene render to the render thread, and returns once the *game thread* side
		// of that submission (Canvas.Flush_GameThread()) is done -- it does NOT wait for the
		// render thread/GPU to actually finish, so callers that need the pixels immediately
		// (RenderFrame) must still flush + block afterwards themselves.
		bool RenderCurrentFrame(UMaterialInterface* Material, const FString& Mesh, float OrbitYaw, float OrbitPitch, FString& OutError);

		TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
		TUniquePtr<FMaterialThumbnailScene> ThumbnailScene;
		int32 CachedWidth = 0;
		int32 CachedHeight = 0;

		// Async GPU readback state. The readback object itself is reused across many frames (like
		// the render target); the promise/future pair is recreated for each individual frame to
		// hand its pixel data from the render thread back to the game thread once ready.
		TSharedPtr<FRHIGPUTextureReadback> PendingReadback;
		TSharedPtr<TPromise<FDreamShaderPreviewReadbackData>> PendingPromise;
		TOptional<TFuture<FDreamShaderPreviewReadbackData>> PendingFuture;
		bool bReadbackInFlight = false;
		// True once the Lock/Unlock copy-to-CPU render command has been dispatched for the
		// CURRENT in-flight readback -- guards against re-dispatching it on every tick spent
		// waiting for the promise it fulfills to actually resolve.
		bool bCopyEnqueued = false;
	};

	class FDreamShaderPreviewRenderer
	{
	public:
		static FString GetPreviewDirectory();
		static FString GetPreviewManifestPath();
		// Interface-typed: ThinCustom (the default backend) generates a thin material instance, and
		// the render pipeline below is interface-based, so instances and materials preview alike.
		static bool ResolvePreviewMaterial(const FDreamShaderPreviewRequest& Request, FDreamShaderPreviewResult& OutResult, UMaterialInterface*& OutMaterial);
		static bool RenderMaterialPreviewFrame(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, TArray64<uint8>& OutPngData, FString& OutError);
		static bool SaveMaterialPreviewFrame(UMaterialInterface* Material, const FString& SourceFilePath, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, FString& OutImagePath, FString& OutError);
		static bool RenderMaterialPreview(const FDreamShaderPreviewRequest& Request, FDreamShaderPreviewResult& OutResult);
		static void WritePreviewResult(const FDreamShaderPreviewResult& Result, const FString& Status, const FString& RequestId = FString());
	};
}
