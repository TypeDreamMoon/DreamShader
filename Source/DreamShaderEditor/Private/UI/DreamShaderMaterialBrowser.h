#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;

	// Registers (and tears down) the "Material Content Browser" nomad tab and its Tools-menu entry. Called
	// once from the editor module's Startup/Shutdown. This is the first editor tab the plugin owns.
	class FDreamShaderMaterialBrowser
	{
	public:
		static void Register();
		static void Unregister();

		static const FName TabId;
	};

	// The tab's root widget: a two-page switcher ("Project" and "Dream Shader Gen") over a shared toolbar.
	// Owns the browser model both pages are views over.
	class SDreamShaderMaterialBrowser : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderMaterialBrowser) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		TSharedPtr<SWidgetSwitcher> PageSwitcher;
		int32 ActivePageIndex = 0;

		void SetActivePage(int32 PageIndex);
		ECheckBoxState GetPageCheckState(int32 PageIndex) const;
		void OnPageCheckStateChanged(ECheckBoxState NewState, int32 PageIndex);
	};
}
