// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#include "UI/SDreamShaderLivePreview.h"

#include "Preview/DreamShaderPreviewRenderer.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Styling/AppStyle.h"
#include "TextureResource.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamShaderMaterialBrowser"

namespace UE::DreamShader::Editor::Private
{
	void SDreamShaderLivePreview::Construct(const FArguments& InArgs)
	{
		Size = FMath::Clamp(InArgs._Size, 64, 1024);
		Context = MakeUnique<FDreamShaderPreviewRenderContext>();

		Texture.Reset(UTexture2D::CreateTransient(Size, Size, PF_R8G8B8A8));
		Texture->SRGB = true;
		Texture->NeverStream = true;
		Texture->UpdateResource();

		Brush.SetResourceObject(Texture.Get());
		Brush.ImageSize = FVector2D(Size, Size);
		Brush.DrawAs = ESlateBrushDrawType::Image;

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(&Brush)
				.Visibility_Lambda([this]() { return bHasFrame ? EVisibility::Visible : EVisibility::Collapsed; })
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this]() { return bHasFrame ? EVisibility::Collapsed : EVisibility::Visible; })
				.Text_Lambda([this]()
				{
					return LastError.IsEmpty() ? LOCTEXT("PreviewRendering", "rendering…") : FText::FromString(LastError);
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
				.Justification(ETextJustify::Center)
			]
		];
	}

	SDreamShaderLivePreview::~SDreamShaderLivePreview()
	{
		// The brush must not outlive the texture it points at, and the context owns GPU resources.
		Brush.SetResourceObject(nullptr);
		Context.Reset();
	}

	void SDreamShaderLivePreview::SetMaterial(UMaterialInterface* InMaterial)
	{
		Material = InMaterial;
		bHasFrame = false;
		LastError.Reset();
		RequestRender();
	}

	void SDreamShaderLivePreview::SetMesh(const FString& InMesh)
	{
		if (Mesh != InMesh)
		{
			Mesh = InMesh;
			RequestRender();
		}
	}

	void SDreamShaderLivePreview::Refresh()
	{
		RequestRender();
	}

	void SDreamShaderLivePreview::RequestRender()
	{
		if (!Material.IsValid())
		{
			return;
		}
		// One frame in flight at a time; a request during one is honoured when it lands, so a drag
		// that asks thirty times a second renders as fast as the GPU returns frames and no faster.
		if (Context->IsReadbackInFlight())
		{
			bRenderPending = true;
			return;
		}
		KickoffNow();
	}

	bool SDreamShaderLivePreview::KickoffNow()
	{
		bRenderPending = false;
		UMaterialInterface* MaterialPtr = Material.Get();
		if (!MaterialPtr)
		{
			return false;
		}
		FString Error;
		if (!Context->KickoffFrame(MaterialPtr, Size, Size, Mesh, OrbitYaw, OrbitPitch, Error))
		{
			LastError = Error;
			return false;
		}
		if (!PollTimer.IsValid())
		{
			PollTimer = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SDreamShaderLivePreview::Poll));
		}
		return true;
	}

	EActiveTimerReturnType SDreamShaderLivePreview::Poll(double, float)
	{
		TArray<uint8> RGBA;
		int32 Width = 0;
		int32 Height = 0;
		FString Error;
		if (Context->TryConsumeReadyFramePixels(RGBA, Width, Height, Error))
		{
			UploadFrame(RGBA, Width, Height);
		}
		else if (!Error.IsEmpty())
		{
			LastError = Error;
		}

		if (Context->IsReadbackInFlight())
		{
			return EActiveTimerReturnType::Continue;
		}
		if (bRenderPending && KickoffNow())
		{
			return EActiveTimerReturnType::Continue;
		}
		PollTimer.Reset();
		return EActiveTimerReturnType::Stop;
	}

	void SDreamShaderLivePreview::UploadFrame(const TArray<uint8>& RGBA, int32 Width, int32 Height)
	{
		if (!Texture.IsValid() || Width != Size || Height != Size || RGBA.Num() != Size * Size * 4)
		{
			return;
		}
		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return;
		}
		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, RGBA.GetData(), RGBA.Num());
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		bHasFrame = true;
		LastError.Reset();
	}

	FReply SDreamShaderLivePreview::OnMouseButtonDown(const FGeometry&, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Material.IsValid())
		{
			return FReply::Handled().CaptureMouse(SharedThis(this)).UseHighPrecisionMouseMovement(SharedThis(this));
		}
		return FReply::Unhandled();
	}

	FReply SDreamShaderLivePreview::OnMouseButtonUp(const FGeometry&, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	FReply SDreamShaderLivePreview::OnMouseMove(const FGeometry&, const FPointerEvent& MouseEvent)
	{
		if (!HasMouseCapture())
		{
			return FReply::Unhandled();
		}
		const FVector2D Delta = MouseEvent.GetCursorDelta();
		if (Delta.IsNearlyZero())
		{
			return FReply::Handled();
		}
		// Same feel as the Material Editor's preview viewport: drag right turns the camera right.
		OrbitYaw += static_cast<float>(Delta.X) * 0.5f;
		OrbitPitch = FMath::Clamp(OrbitPitch - static_cast<float>(Delta.Y) * 0.5f, -89.0f, 89.0f);
		RequestRender();
		return FReply::Handled();
	}

	FCursorReply SDreamShaderLivePreview::OnCursorQuery(const FGeometry&, const FPointerEvent&) const
	{
		return FCursorReply::Cursor(HasMouseCapture() ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
	}
}

#undef LOCTEXT_NAMESPACE
