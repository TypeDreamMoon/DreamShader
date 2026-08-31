// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Preview/DreamShaderPreviewRenderer.h"

class UMaterialInterface;
class UTexture2D;

namespace UE::DreamShader::Editor::Private
{

	// A real render of a material -- the plugin's own preview renderer, the same one the VSCode
	// extension streams from -- instead of the asset thumbnail cache. Renders asynchronously (kick
	// off, poll the GPU readback, upload into a transient texture), so a re-render on every compile
	// or camera drag never stalls the editor. Left-drag orbits; the mesh is the caller's to pick.
	class SDreamShaderLivePreview : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderLivePreview)
			: _Size(256)
		{}
			SLATE_ARGUMENT(int32, Size)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		virtual ~SDreamShaderLivePreview() override;

		void SetMaterial(UMaterialInterface* InMaterial);
		void SetMesh(const FString& InMesh);
		// Re-render the current material (after a compile changed it).
		void Refresh();

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	private:
		TUniquePtr<FDreamShaderPreviewRenderContext> Context;
		TWeakObjectPtr<UMaterialInterface> Material;
		TStrongObjectPtr<UTexture2D> Texture;
		FSlateBrush Brush;
		int32 Size = 256;
		FString Mesh = TEXT("sphere");
		// USceneThumbnailInfo's own defaults, so the first frame matches the asset thumbnail.
		float OrbitYaw = -157.5f;
		float OrbitPitch = -11.25f;
		bool bRenderPending = false;
		bool bHasFrame = false;
		FString LastError;
		TSharedPtr<FActiveTimerHandle> PollTimer;

		void RequestRender();
		bool KickoffNow();
		EActiveTimerReturnType Poll(double CurrentTime, float DeltaTime);
		void UploadFrame(const TArray<uint8>& RGBA, int32 Width, int32 Height);
	};
}
