// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// The details panel of a UDreamPassPipeline. The properties stay, and stay editable -- the panel is where a pipeline is
// tuned, and Adopt Into Source writes the tuning back into its `.dsp` -- and three categories above them read the asset
// the way the frame runs it:
//
//   Pipeline Overview   the `.dsp` it was built from, with Open Source, Revert to Source and Adopt Into Source; what it
//                       is (order, views, requirements, how many of each); every problem the runtime would skip a pass
//                       for (UDreamPassPipeline::Validate);
//   Passes in Frame Order  one group per injection point, in the order the frame reaches them, each pass with its settings
//                       and bindings spelled as the source spells them, and -- for an HLSL pass -- its global shader slot
//                       and whether that slot's snapshot exists;
//   Buffers             format and size, history and clear, and the render target an exported buffer is copied into.
//
// The rows read the asset live, so a value edited below shows above at once; a pass or a buffer added, removed, renamed
// or moved to another injection point rebuilds the panel. Registered by the editor module for the class; the asset types
// exist on every engine, and so does this.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class IPropertyUtilities;
class UDreamPassPipeline;

namespace UE::DreamShader::Editor::Private
{
	class FDreamPassPipelineCustomization final : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance();

		/** Registers the layout with the PropertyEditor module. The editor module calls it once, at startup. */
		static void Register();
		/** Undoes Register; safe when it never ran, and when the PropertyEditor module is gone already. */
		static void Unregister();

		virtual ~FDreamPassPipelineCustomization() override;
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	private:
		void BuildOverview(IDetailLayoutBuilder& DetailBuilder, UDreamPassPipeline& Pipeline);
		void BuildPasses(IDetailLayoutBuilder& DetailBuilder, UDreamPassPipeline& Pipeline);
		void BuildBuffers(IDetailLayoutBuilder& DetailBuilder, UDreamPassPipeline& Pipeline);

		void OnPipelineChanged(UDreamPassPipeline* Changed);

		/** What the layout was built from: the names, kinds and injection points of the passes, the buffers, the export targets. */
		static FString MakeStructureKey(const UDreamPassPipeline& Pipeline);

		TWeakObjectPtr<UDreamPassPipeline> WeakPipeline;
		TWeakPtr<IPropertyUtilities> PropertyUtilities;
		FString StructureKey;
		/** The provenance label, worked out when the panel is built and after every change: classifying hashes the asset. */
		TSharedRef<FText> ProvenanceLabel = MakeShared<FText>();
		FDelegateHandle ChangedHandle;

		static bool bRegistered;
	};
}
