#include "Preview/DreamShaderPreviewRenderer.h"

#include "DreamShaderCompileService.h"
#include "DreamShaderModule.h"
#include "DreamShaderParser.h"
#include "DreamShaderSettings.h"
#include "Compile/DreamShaderEditorCompileAdapter.h"
#include "DependencyGraph/DreamShaderDependencyGraphService.h"
#include "MaterialAssetGeneration/DreamShaderMaterialGeneratorPrivate.h"

#include "AssetCompilingManager.h"
#include "CanvasTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "LegacyScreenPercentageDriver.h"
#include "Materials/Material.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Diagnostics/DreamShaderTextWireUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RHITransition.h"
#include "SceneView.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "ThumbnailHelpers.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"

#define LOCTEXT_NAMESPACE "DreamShader.Preview"

// A NOTE ON THE `FString& OutError` PARAMETERS BELOW.
//
// FDreamShaderPreviewResult::Message is an FText and the wire serializes it through
// ToInvariantWireString, so a message that stays an FText all the way to the wire comes out English
// under any editor culture. An `FString& OutError` does NOT: the FText has to be collapsed with
// ToString() to fill it, and ToString() is the LOCALIZED display. Wrapping the result back up with
// FText::FromString at the call site cannot undo that -- the invariant rebuild has nothing left to
// rebuild from, and a zh-Hans editor would put Chinese into preview.json and the preview WebSocket,
// which the VSCode and Rider extensions parse.
//
// The render context's whole error channel (EnsureResources / RenderCurrentFrame / RenderFrame /
// KickoffFrame / TryConsumeReadyFrame / FDreamShaderPreviewReadbackData::Error) is FString-typed, so
// every site filling one of those is marked I18N-EXEMPT and stays a literal. Sites that assign
// straight to FDreamShaderPreviewResult::Message are FText and are localized normally.
//
// Localizing this channel properly means converting that whole error path to FText -- worth doing,
// but it is not a one-line change.

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		FString BuildPreviewImagePath(const FString& SourceFilePath)
		{
			FString RelativePath = SourceFilePath;
			FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectDir());
			RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			const FString StableKey = RelativePath.IsEmpty() ? SourceFilePath : RelativePath;
			const FString FileStem = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(SourceFilePath));
			const FString Hash = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*StableKey));
			return FPaths::Combine(
				FDreamShaderPreviewRenderer::GetPreviewDirectory(),
				FString::Printf(TEXT("%s-%s.png"), FileStem.IsEmpty() ? TEXT("DreamShaderPreview") : *FileStem, *Hash));
		}

		bool ResolveGeneratedMaterialPath(const FString& SourceFilePath, FString& OutObjectPath, FString& OutError)
		{
			FString SourceText;
			if (!FFileHelper::LoadFileToString(SourceText, *SourceFilePath))
			{
				OutError = FString::Printf(TEXT("Failed to read DreamShader source '%s'."), *SourceFilePath); // I18N-EXEMPT: reaches the wire, see the note above
				return false;
			}

			TArray<FString> Lines;
			SourceText.ParseIntoArrayLines(Lines, false);
			SourceText.Reset();
			for (const FString& Line : Lines)
			{
				FString ImportPath;
				if (FDreamShaderDependencyGraphService::TryExtractImportPathFromLine(Line, ImportPath))
				{
					continue;
				}
				SourceText += Line;
				SourceText += TEXT("\n");
			}

			FTextShaderDefinition Definition;
			FString ParseError;
			if (!FTextShaderParser::Parse(SourceText, Definition, ParseError))
			{
				OutError = FString::Printf(TEXT("%s: %s"), *SourceFilePath, *ParseError); // I18N-EXEMPT: reaches the wire, see the note above
				return false;
			}

			if (Definition.Name.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s: This file does not define a top-level Shader block."), *SourceFilePath); // I18N-EXEMPT: reaches the wire, see the note above
				return false;
			}

			// Same default the generator applies, or the preview would look up an object path that
			// generation never wrote.
			ApplyDefaultRootFromSourceFile(SourceFilePath, Definition);

			FString PackageName;
			FString AssetName;
			return ResolveDreamShaderAssetDestination(Definition.Name, Definition.Root, PackageName, OutObjectPath, AssetName, OutError);
		}

		// Maps the VSCode extension's mesh selector value to the same primitive-shape enum the
		// native Material Editor's own preview-shape toolbar button writes to
		// ThumbnailInfo->PrimitiveType. Unrecognized/empty values fall back to sphere, matching
		// FDreamShaderPreviewRequest::Mesh's default.
		EThumbnailPrimType ResolvePreviewPrimitiveType(const FString& Mesh)
		{
			if (Mesh.Equals(TEXT("plane"), ESearchCase::IgnoreCase))
			{
				return TPT_Plane;
			}
			if (Mesh.Equals(TEXT("cube"), ESearchCase::IgnoreCase))
			{
				return TPT_Cube;
			}
			if (Mesh.Equals(TEXT("cylinder"), ESearchCase::IgnoreCase))
			{
				return TPT_Cylinder;
			}
			if (Mesh.Equals(TEXT("shaderball"), ESearchCase::IgnoreCase))
			{
				// TPT_ShaderBall only exists from UE 5.8 on; older engines get the sphere.
				return DREAMSHADER_THUMBNAIL_PRIM_SHADERBALL;
			}
			return TPT_Sphere;
		}

		// Shared by ApplyPreviewMeshSelection/ApplyPreviewCameraOrbit -- both need to read/write the
		// same USceneThumbnailInfoWithPrimitive that FMaterialThumbnailScene itself reads from
		// Material->ThumbnailInfo, lazily creating one (matching what the native Material Editor's
		// own preview toolbar would do the first time you touch shape or orbit for an asset that
		// never had one).
		USceneThumbnailInfoWithPrimitive* GetOrCreatePreviewThumbnailInfo(UMaterialInterface* Material)
		{
			USceneThumbnailInfoWithPrimitive* ThumbnailInfo = Cast<USceneThumbnailInfoWithPrimitive>(Material->ThumbnailInfo);
			if (!ThumbnailInfo)
			{
				ThumbnailInfo = NewObject<USceneThumbnailInfoWithPrimitive>(Material);
				Material->ThumbnailInfo = ThumbnailInfo;
			}
			return ThumbnailInfo;
		}

		// FMaterialThumbnailScene::SetMaterialInterface() picks its preview primitive by reading
		// Material->ThumbnailInfo->PrimitiveType (falling back to a plane if the material's own
		// Domain forces one, e.g. UI materials) -- there's no way to select a shape through the
		// scene itself. So the requested mesh has to be written onto the material asset's own
		// ThumbnailInfo before every SetMaterialInterface() call, same as the native Material
		// Editor's preview-shape button does.
		void ApplyPreviewMeshSelection(UMaterialInterface* Material, const FString& Mesh)
		{
			if (!Material)
			{
				return;
			}

			GetOrCreatePreviewThumbnailInfo(Material)->PrimitiveType = ResolvePreviewPrimitiveType(Mesh);
		}

		// FMaterialThumbnailScene::GetViewMatrixParameters()/CreateView() likewise read
		// Material->ThumbnailInfo->OrbitYaw/OrbitPitch fresh on every call (no cached view matrix to
		// invalidate), the same fields the Material Editor's own drag-to-orbit preview viewport
		// writes to -- so a client-driven orbit angle just needs to land here before each render,
		// same shape as the mesh selection above. The engine applies no clamp of its own; the VSCode
		// side clamps pitch to roughly +/-89 degrees before sending it to avoid gimbal-flip framing
		// (see EditorViewportClient.cpp's own +/-90 pitch clamp for the native equivalent).
		void ApplyPreviewCameraOrbit(UMaterialInterface* Material, float OrbitYaw, float OrbitPitch)
		{
			if (!Material)
			{
				return;
			}

			USceneThumbnailInfoWithPrimitive* ThumbnailInfo = GetOrCreatePreviewThumbnailInfo(Material);
			ThumbnailInfo->OrbitYaw = OrbitYaw;
			ThumbnailInfo->OrbitPitch = OrbitPitch;
		}

		void WaitForPreviewMaterialCompilation(UMaterialInterface* Material)
		{
			TArray<UObject*> MaterialObjects;
			MaterialObjects.Add(Material);
			FAssetCompilingManager::Get().FinishCompilationForObjects(MaterialObjects);
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			FlushRenderingCommands();
		}

		bool BuildThumbnailPng(UMaterialInterface* Material, const int32 Width, const int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, const bool bWaitForCompilation, TArray64<uint8>& OutPngData, FString& OutError)
		{
			if (bWaitForCompilation && Material)
			{
				WaitForPreviewMaterialCompilation(Material);
			}

			// One-shot call sites (the initial preview frame, and the file-bridge fallback) don't
			// stream many frames for the same material, so a short-lived context here is fine --
			// FDreamShaderPreviewRenderContext's persistence benefit is for SendPreviewFrame's
			// repeated per-tick calls (see FClientPreviewState::RenderContext), which reuse one
			// context across the whole streaming session instead of constructing one per call.
			FDreamShaderPreviewRenderContext TemporaryContext;
			return TemporaryContext.RenderFrame(Material, Width, Height, Mesh, OrbitYaw, OrbitPitch, OutPngData, OutError);
		}

		bool SaveThumbnailPng(UMaterialInterface* Material, const int32 Width, const int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, const FString& ImagePath, FString& OutError)
		{
			TArray64<uint8> PngData;
			if (!BuildThumbnailPng(Material, Width, Height, Mesh, OrbitYaw, OrbitPitch, true, PngData, OutError))
			{
				return false;
			}

			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ImagePath), true);
			if (!FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PngData.GetData(), PngData.Num()), *ImagePath))
			{
				OutError = FString::Printf(TEXT("Failed to write preview image '%s'."), *ImagePath); // I18N-EXEMPT: reaches the wire, see the note above
				return false;
			}

			return true;
		}
	}

	FDreamShaderPreviewRenderContext::FDreamShaderPreviewRenderContext() = default;
	FDreamShaderPreviewRenderContext::~FDreamShaderPreviewRenderContext() = default;

	bool FDreamShaderPreviewRenderContext::EnsureResources(int32 Width, int32 Height, FString& OutError)
	{
		const int32 PreviewWidth = FMath::Clamp(Width, 64, 2048);
		const int32 PreviewHeight = FMath::Clamp(Height, 64, 2048);

		if (!ThumbnailScene.IsValid())
		{
			ThumbnailScene = MakeUnique<FMaterialThumbnailScene>();
		}

		if (RenderTarget.IsValid() && CachedWidth == PreviewWidth && CachedHeight == PreviewHeight)
		{
			return true;
		}

		UTextureRenderTarget2D* NewRenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		if (!NewRenderTarget)
		{
			OutError = TEXT("Failed to create preview render target.");
			return false;
		}
		NewRenderTarget->ClearColor = FLinearColor(0.025f, 0.025f, 0.03f, 1.0f);
		NewRenderTarget->InitCustomFormat(PreviewWidth, PreviewHeight, PF_B8G8R8A8, false);
		NewRenderTarget->UpdateResourceImmediate(true);

		RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(NewRenderTarget);
		CachedWidth = PreviewWidth;
		CachedHeight = PreviewHeight;
		return true;
	}

	// Shared by RenderFrame() and KickoffFrame(): assigns the material and submits the scene
	// render. Does NOT clear the thumbnail scene's material interface afterward -- this context
	// is reused across many frames of (usually) the same material, so there's nothing to clean up
	// between frames, and clearing it asynchronously (before the render thread has actually
	// finished processing the submission) would race with in-flight rendering commands that still
	// reference the scene's current state. The temporary context RenderFrame's one-shot callers
	// construct locally is torn down as a whole right after use anyway.
	bool FDreamShaderPreviewRenderContext::RenderCurrentFrame(UMaterialInterface* Material, const FString& Mesh, float OrbitYaw, float OrbitPitch, FString& OutError)
	{
		FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
		if (!RenderTargetResource)
		{
			OutError = TEXT("Failed to access preview render target resource.");
			return false;
		}

		ApplyPreviewMeshSelection(Material, Mesh);
		ApplyPreviewCameraOrbit(Material, OrbitYaw, OrbitPitch);
		ThumbnailScene->SetMaterialInterface(Material);
		FCanvas Canvas(RenderTargetResource, nullptr, FGameTime::GetTimeSinceAppStart(), GMaxRHIFeatureLevel);
		Canvas.Clear(RenderTarget->ClearColor);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			RenderTargetResource,
			ThumbnailScene->GetScene(),
			FEngineShowFlags(ESFIM_Game))
			.SetTime(FGameTime::GetTimeSinceAppStart()));
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.AntiAliasing = 0;
		ViewFamily.EngineShowFlags.SetSeparateTranslucency(ThumbnailScene->ShouldSetSeparateTranslucency(Material));

		FSceneView* View = ThumbnailScene->CreateView(&ViewFamily, 0, 0, CachedWidth, CachedHeight);
		if (!View)
		{
			OutError = FString::Printf(TEXT("Failed to create preview view for '%s'."), *Material->GetPathName()); // I18N-EXEMPT: reaches the wire, see the note above
			return false;
		}

		ViewFamily.EngineShowFlags.ScreenPercentage = false;
		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
			ViewFamily,
			1.0f));
		GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);
		Canvas.Flush_GameThread();
		return true;
	}

	bool FDreamShaderPreviewRenderContext::RenderFramePixels(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, TArray<FColor>& OutColors, FString& OutError)
	{
		OutColors.Reset();
		if (!Material)
		{
			OutError = TEXT("Preview material is not valid.");
			return false;
		}

		if (!EnsureResources(Width, Height, OutError) || !RenderCurrentFrame(Material, Mesh, OrbitYaw, OrbitPitch, OutError))
		{
			return false;
		}

		// Unlike KickoffFrame(), this call site needs the pixels back before it returns, so it
		// still pays for a full game-thread stall here.
		FlushRenderingCommands();

		FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
		if (!RenderTargetResource || !RenderTargetResource->ReadPixels(OutColors) || OutColors.Num() != CachedWidth * CachedHeight)
		{
			OutError = FString::Printf(TEXT("Failed to read preview pixels for '%s'."), *Material->GetPathName()); // I18N-EXEMPT: reaches the wire, see the note above
			return false;
		}

		return true;
	}

	bool FDreamShaderPreviewRenderContext::RenderFrame(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, TArray64<uint8>& OutPngData, FString& OutError)
	{
		OutPngData.Reset();
		TArray<FColor> Colors;
		if (!RenderFramePixels(Material, Width, Height, Mesh, OrbitYaw, OrbitPitch, Colors, OutError))
		{
			return false;
		}

		for (FColor& Color : Colors)
		{
			Color.A = 255;
		}

		FImageUtils::PNGCompressImageArray(
			CachedWidth,
			CachedHeight,
			TArrayView64<const FColor>(Colors.GetData(), Colors.Num()),
			OutPngData);
		if (OutPngData.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Failed to encode preview thumbnail for '%s'."), *Material->GetPathName()); // I18N-EXEMPT: reaches the wire, see the note above
			return false;
		}

		return true;
	}

	bool FDreamShaderPreviewRenderContext::KickoffFrame(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, FString& OutError)
	{
		if (bReadbackInFlight)
		{
			OutError = TEXT("A preview readback is already in flight.");
			return false;
		}
		if (!Material)
		{
			OutError = TEXT("Preview material is not valid.");
			return false;
		}
		if (!EnsureResources(Width, Height, OutError) || !RenderCurrentFrame(Material, Mesh, OrbitYaw, OrbitPitch, OutError))
		{
			return false;
		}

		FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
		if (!RenderTargetResource)
		{
			OutError = TEXT("Failed to access preview render target resource.");
			return false;
		}

		if (!PendingReadback.IsValid())
		{
			PendingReadback = MakeShared<FRHIGPUTextureReadback>(TEXT("DreamShaderPreviewReadback"));
		}

		const int32 ReadbackWidth = CachedWidth;
		const int32 ReadbackHeight = CachedHeight;
		TSharedPtr<FRHIGPUTextureReadback> Readback = PendingReadback;
		// Mirrors ObjectTools.cpp's ReadbackThumbnailAsync()/FAsyncObjectThumbnail pattern (the
		// engine's own async thumbnail-readback helper) -- enqueue the GPU->CPU copy on the render
		// thread and return immediately without waiting for it.
		ENQUEUE_RENDER_COMMAND(DreamShaderPreviewEnqueueCopy)(
			[RenderTargetResource, Readback, ReadbackWidth, ReadbackHeight](FRHICommandListImmediate& RHICmdList)
			{
				FRHITexture* Texture = RenderTargetResource->GetRenderTargetTexture();
				if (!Texture)
				{
					return;
				}
				RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
				const FResolveRect SrcRect(0, 0, ReadbackWidth, ReadbackHeight);
				Readback->EnqueueCopy(RHICmdList, Texture, SrcRect);
				RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
			});

		bReadbackInFlight = true;
		bCopyEnqueued = false;
		PendingPromise.Reset();
		PendingFuture.Reset();
		return true;
	}

	bool FDreamShaderPreviewRenderContext::TryConsumeReadyFrame(TArray64<uint8>& OutPngData, FString& OutError)
	{
		FDreamShaderPreviewReadbackData Data;
		if (!TryConsumeReadyFrameColors(Data, OutError))
		{
			return false;
		}

		for (FColor& Color : Data.Colors)
		{
			Color.A = 255;
		}

		OutPngData.Reset();
		FImageUtils::PNGCompressImageArray(
			Data.Width,
			Data.Height,
			TArrayView64<const FColor>(Data.Colors.GetData(), Data.Colors.Num()),
			OutPngData);
		if (OutPngData.IsEmpty())
		{
			OutError = TEXT("Failed to encode preview thumbnail.");
			return false;
		}

		return true;
	}

	bool FDreamShaderPreviewRenderContext::TryConsumeReadyFramePixels(TArray<uint8>& OutRGBA, int32& OutWidth, int32& OutHeight, FString& OutError)
	{
		FDreamShaderPreviewReadbackData Data;
		if (!TryConsumeReadyFrameColors(Data, OutError))
		{
			return false;
		}

		OutWidth = Data.Width;
		OutHeight = Data.Height;
		const int32 PixelCount = Data.Colors.Num();
		OutRGBA.SetNumUninitialized(PixelCount * 4);
		uint8* Dst = OutRGBA.GetData();
		const FColor* Src = Data.Colors.GetData();
		for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
		{
			// FColor is B,G,R,A in memory (it mirrors the BGRA render target); the wire is RGBA.
			const FColor& Color = Src[PixelIndex];
			Dst[0] = Color.R;
			Dst[1] = Color.G;
			Dst[2] = Color.B;
			Dst[3] = 255;
			Dst += 4;
		}
		return true;
	}

	bool FDreamShaderPreviewRenderContext::TryConsumeReadyFrameColors(FDreamShaderPreviewReadbackData& OutData, FString& OutError)
	{
		if (!bReadbackInFlight || !PendingReadback.IsValid())
		{
			return false;
		}

		if (!bCopyEnqueued)
		{
			// Non-blocking poll (safe to call directly from the game thread -- this mirrors
			// FAsyncObjectThumbnail::IsTextureReadbackReady() in ObjectTools.cpp, which does the
			// same). Once true, the GPU has actually finished the copy enqueued by KickoffFrame().
			if (!PendingReadback->IsReady())
			{
				return false;
			}

			TSharedPtr<TPromise<FDreamShaderPreviewReadbackData>> Promise = MakeShared<TPromise<FDreamShaderPreviewReadbackData>>();
			PendingFuture = Promise->GetFuture();
			PendingPromise = Promise;

			const int32 Width = CachedWidth;
			const int32 Height = CachedHeight;
			TSharedPtr<FRHIGPUTextureReadback> Readback = PendingReadback;
			// Also mirrors ObjectTools.cpp's EnqueueReadbackDataCopy(): Lock()/Unlock() must
			// happen on the render thread, so the copy-to-CPU-memory step is itself another
			// render command; its result is hand off to the game thread via the promise/future.
			ENQUEUE_RENDER_COMMAND(DreamShaderPreviewCopyReadback)(
				[Promise, Readback, Width, Height](FRHICommandListImmediate& RHICmdList)
				{
					FDreamShaderPreviewReadbackData Data;
					Data.Width = Width;
					Data.Height = Height;

					int32 ReadbackRowWidth = 0;
					int32 ReadbackRowHeight = 0;
					void* ReadbackBuffer = Readback->Lock(ReadbackRowWidth, &ReadbackRowHeight);
					if (ReadbackBuffer && ReadbackRowWidth >= Width && ReadbackRowHeight >= Height)
					{
						Data.Colors.SetNumUninitialized(Width * Height);
						if (ReadbackRowWidth == Width)
						{
							FMemory::Memcpy(Data.Colors.GetData(), ReadbackBuffer, static_cast<SIZE_T>(Width) * Height * sizeof(FColor));
						}
						else
						{
							// The staging texture's row pitch differs from the requested width --
							// copy one row at a time instead of assuming a tightly-packed buffer.
							const uint8* Src = reinterpret_cast<const uint8*>(ReadbackBuffer);
							for (int32 Row = 0; Row < Height; ++Row)
							{
								FMemory::Memcpy(
									reinterpret_cast<uint8*>(Data.Colors.GetData()) + static_cast<SIZE_T>(Row) * Width * sizeof(FColor),
									Src + static_cast<SIZE_T>(Row) * ReadbackRowWidth * sizeof(FColor),
									static_cast<SIZE_T>(Width) * sizeof(FColor));
							}
						}
						Data.bSucceeded = true;
					}
					else
					{
						Data.Error = TEXT("Failed to lock preview readback buffer.");
					}
					Readback->Unlock();
					Promise->SetValue(MoveTemp(Data));
				});

			bCopyEnqueued = true;
			return false;
		}

		if (!PendingFuture.IsSet() || !PendingFuture->IsReady())
		{
			return false;
		}

		OutData = PendingFuture->Get();
		bReadbackInFlight = false;
		bCopyEnqueued = false;
		PendingPromise.Reset();
		PendingFuture.Reset();

		if (!OutData.bSucceeded)
		{
			OutError = OutData.Error.IsEmpty() ? TEXT("Preview readback failed.") : OutData.Error;
			return false;
		}

		return true;
	}

	FString FDreamShaderPreviewRenderer::GetPreviewDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Bridge"), TEXT("Preview"));
	}

	FString FDreamShaderPreviewRenderer::GetPreviewManifestPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DreamShader"), TEXT("Bridge"), TEXT("preview.json"));
	}

	bool FDreamShaderPreviewRenderer::ResolvePreviewMaterial(const FDreamShaderPreviewRequest& Request, FDreamShaderPreviewResult& OutResult, UMaterialInterface*& OutMaterial)
	{
		OutMaterial = nullptr;
		const FString SourceFilePath = UE::DreamShader::NormalizeSourceFilePath(Request.SourceFilePath);
		OutResult = FDreamShaderPreviewResult();
		OutResult.SourceFilePath = SourceFilePath;
		OutResult.Mesh = Request.Mesh.IsEmpty() ? TEXT("sphere") : Request.Mesh;

		if (SourceFilePath.IsEmpty() || !IFileManager::Get().FileExists(*SourceFilePath))
		{
			OutResult.Message = FText::Format(LOCTEXT("PreviewSourceMissing", "DreamShader source '{0}' does not exist."), FText::FromString(SourceFilePath));
			return false;
		}

		if (!UE::DreamShader::IsDreamShaderMaterialFile(SourceFilePath))
		{
			OutResult.Message = FText::Format(LOCTEXT("PreviewSupportsOnlyDsm", "DreamShader preview only supports .dsm material files: '{0}'."), FText::FromString(SourceFilePath));
			return false;
		}

		FString ObjectPath;
		FString ResolveError;
		if (!ResolveGeneratedMaterialPath(SourceFilePath, ObjectPath, ResolveError))
		{
			OutResult.Message = FText::FromString(ResolveError);
			return false;
		}
		OutResult.AssetPath = ObjectPath;

		UE::DreamShader::Compiler::FDreamShaderCompileService CompileService(UE::DreamShader::Editor::GetEditorCompileAdapter());
		// Editor materials are always memory-only, so a preview compile is transient (never persists).
		const UE::DreamShader::Compiler::FDreamShaderCompileResult CompileResult = CompileService.CompileMaterial(SourceFilePath, Request.bForceRecompile, /*bTransient*/ true);
		if (!CompileResult.bSucceeded)
		{
			OutResult.Message = CompileResult.Message;
			return false;
		}

		// Interface-typed on purpose: ThinCustom (the default backend) generates a thin material
		// INSTANCE whose hidden base carries the graph, and the whole render pipeline downstream is
		// interface-based, so both shapes preview identically.
		OutMaterial = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
		if (!OutMaterial)
		{
			OutResult.Message = FText::Format(LOCTEXT("GeneratedMaterialCouldNotBeLoaded", "Generated material '{0}' could not be loaded."), FText::FromString(ObjectPath));
			return false;
		}

		OutResult.bSucceeded = true;
		OutResult.Message = FText::Format(LOCTEXT("CompiledPreviewMaterial", "Compiled preview material for {0}."), FText::FromString(ObjectPath));
		if (!CompileResult.Message.IsEmpty())
		{
			OutResult.Message = FText::Format(LOCTEXT("CompiledPreviewMaterialWithDetails", "Compiled preview material for {0}. {1}"), FText::FromString(ObjectPath), CompileResult.Message);
		}
		return true;
	}

	bool FDreamShaderPreviewRenderer::RenderMaterialPreviewFrame(UMaterialInterface* Material, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, TArray64<uint8>& OutPngData, FString& OutError)
	{
		return BuildThumbnailPng(Material, Width, Height, Mesh, OrbitYaw, OrbitPitch, false, OutPngData, OutError);
	}

	bool FDreamShaderPreviewRenderer::SaveMaterialPreviewFrame(UMaterialInterface* Material, const FString& SourceFilePath, int32 Width, int32 Height, const FString& Mesh, float OrbitYaw, float OrbitPitch, FString& OutImagePath, FString& OutError)
	{
		OutImagePath = BuildPreviewImagePath(SourceFilePath);
		return SaveThumbnailPng(Material, Width, Height, Mesh, OrbitYaw, OrbitPitch, OutImagePath, OutError);
	}

	bool FDreamShaderPreviewRenderer::RenderMaterialPreview(const FDreamShaderPreviewRequest& Request, FDreamShaderPreviewResult& OutResult)
	{
		UMaterialInterface* Material = nullptr;
		if (!ResolvePreviewMaterial(Request, OutResult, Material))
		{
			return false;
		}

		FString RenderError;
		FString ImagePath;
		if (!SaveMaterialPreviewFrame(Material, OutResult.SourceFilePath, Request.Width, Request.Height, Request.Mesh, Request.OrbitYaw, Request.OrbitPitch, ImagePath, RenderError))
		{
			OutResult.Message = FText::FromString(RenderError);
			return false;
		}

		OutResult.bSucceeded = true;
		OutResult.ImagePath = ImagePath;
		OutResult.Message = FText::Format(LOCTEXT("RenderedPreview", "Rendered preview for {0}."), FText::FromString(OutResult.AssetPath));
		return true;
	}

	void FDreamShaderPreviewRenderer::WritePreviewResult(const FDreamShaderPreviewResult& Result, const FString& Status, const FString& RequestId)
	{
		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetNumberField(TEXT("version"), 1);
		if (!RequestId.IsEmpty())
		{
			RootObject->SetStringField(TEXT("requestId"), RequestId);
		}
		RootObject->SetStringField(TEXT("status"), Status);
		RootObject->SetStringField(TEXT("sourceFile"), Result.SourceFilePath);
		RootObject->SetStringField(TEXT("assetPath"), Result.AssetPath);
		RootObject->SetStringField(TEXT("imagePath"), Result.ImagePath);
		RootObject->SetStringField(TEXT("mesh"), Result.Mesh);
		RootObject->SetStringField(TEXT("message"), ToInvariantWireString(Result.Message));
		RootObject->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());

		FString OutputText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputText);
		if (FJsonSerializer::Serialize(RootObject, Writer))
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(GetPreviewManifestPath()), true);
			FFileHelper::SaveStringToFile(OutputText, *GetPreviewManifestPath());
		}
	}
}

#undef LOCTEXT_NAMESPACE
