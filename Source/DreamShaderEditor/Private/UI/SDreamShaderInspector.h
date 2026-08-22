// Copyright (c) 2026 TypeDreamMoon. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnail;
class SBorder;
class UMaterialInterface;

namespace UE::DreamShader::Editor::Private
{
	class FDreamShaderBrowserModel;
	struct FBrowserEntry;

	// The browser's detail panel, shared by every page: renders whichever halves of an entry are
	// present -- the source's compile status and diagnostics, the asset's surface settings, storage,
	// provenance, inheritance chain and loaded children -- plus the actions that apply. Rebuilds
	// itself when the model changes.
	class SDreamShaderInspector : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDreamShaderInspector) {}
			SLATE_ARGUMENT(TSharedPtr<FDreamShaderBrowserModel>, Model)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		virtual ~SDreamShaderInspector() override;

		void SetEntry(TSharedPtr<FBrowserEntry> InEntry);
		TSharedPtr<FBrowserEntry> GetEntry() const { return Entry; }

		// Re-targets the panel at a material picked out of the inheritance chain or a child list.
		void SetMaterial(UMaterialInterface* Material);

	private:
		TSharedPtr<FDreamShaderBrowserModel> Model;
		TSharedPtr<FBrowserEntry> Entry;
		TSharedPtr<SBorder> ContentContainer;
		TSharedPtr<FAssetThumbnail> Thumbnail;
		FDelegateHandle ModelChangedHandle;

		void Rebuild();
		TSharedRef<SWidget> BuildContent();
		TSharedRef<SWidget> BuildHeader(UMaterialInterface* Material);
		TSharedRef<SWidget> BuildActions(UMaterialInterface* Material);
		TSharedRef<SWidget> BuildInfoRows(UMaterialInterface* Material);
		TSharedRef<SWidget> BuildDiagnostics();
		TSharedRef<SWidget> BuildDependents();
		TSharedRef<SWidget> BuildInheritance(UMaterialInterface* Material);
		TSharedRef<SWidget> MakeMaterialLink(UMaterialInterface* Target, const FText& Prefix, bool bIsSelf);
	};
}
