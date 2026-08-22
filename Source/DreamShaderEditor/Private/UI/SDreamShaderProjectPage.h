#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;
	class SDreamShaderInspector;

	// Project tab: a browsable, filterable grid of every material / material instance under /Game, with
	// thumbnails, backed by the engine content-browser asset picker, over the shared inspector (which
	// joins a picked asset back to the DreamShader source it was generated from, when there is one).
	class SDreamShaderProjectPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderProjectPage) {}
			SLATE_ARGUMENT(TSharedPtr<FDreamShaderBrowserModel>, Model)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		FAssetData SelectedAsset;
		TSharedPtr<SDreamShaderInspector> Inspector;

		void OnAssetSelected(const FAssetData& AssetData);
		FReply OnCreateInstanceClicked();
	};
}
