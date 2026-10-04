#include "DreamPassSettings.h"

#include "DreamShaderPassModule.h"

int32 UDreamPassSettings::FindLayerIndex(FName Layer) const
{
	if (Layer.IsNone())
	{
		return INDEX_NONE;
	}
	const int32 Index = LayerNames.IndexOfByKey(Layer);
	return Index < MaxLayers ? Index : INDEX_NONE;
}

uint32 UDreamPassSettings::MakeLayerMask(TConstArrayView<FName> Layers, TArray<FName>* OutUnknown) const
{
	uint32 Mask = 0;
	for (const FName Layer : Layers)
	{
		const int32 Index = FindLayerIndex(Layer);
		if (Index == INDEX_NONE)
		{
			if (OutUnknown)
			{
				OutUnknown->AddUnique(Layer);
			}
			continue;
		}
		Mask |= 1u << Index;
	}
	return Mask;
}

#if WITH_EDITOR
void UDreamPassSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (LayerNames.Num() > MaxLayers)
	{
		UE_LOG(LogDreamPass, Warning, TEXT("DreamShader Custom Pass: %d pass layers are named; only the first %d are used."), LayerNames.Num(), MaxLayers);
	}
}
#endif
