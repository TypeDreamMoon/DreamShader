#include "Preview/DreamShaderProbePreview.h"

#include "DreamShaderModule.h"
#include "DreamShaderVersionCompat.h"
#include "Preview/DreamShaderGraphDebugInfo.h"

#include "MaterialDomain.h"
#include "MaterialEditor/PreviewMaterial.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "UObject/Package.h"

namespace UE::DreamShader::Editor::Private
{
	FDreamShaderProbePreview::FDreamShaderProbePreview()
	{
		FDreamShaderGraphDebugRegistry& Registry = FDreamShaderGraphDebugRegistry::Get();
		AboutToResetHandle = Registry.OnGraphMaterialAboutToReset.AddRaw(this, &FDreamShaderProbePreview::HandleGraphMaterialAboutToReset);
		PublishedHandle = Registry.OnTablePublished.AddRaw(this, &FDreamShaderProbePreview::HandleTablePublished);
	}

	FDreamShaderProbePreview::~FDreamShaderProbePreview()
	{
		FDreamShaderGraphDebugRegistry& Registry = FDreamShaderGraphDebugRegistry::Get();
		Registry.OnGraphMaterialAboutToReset.Remove(AboutToResetHandle);
		Registry.OnTablePublished.Remove(PublishedHandle);

		// Same teardown as ClearProbe(): the shared collection must not outlive this object inside
		// the preview material, and the material itself must not go away under a frame that is
		// still being rendered with it.
		ReleaseSharedExpressions();
		if (PreviewMaterial.IsValid())
		{
			FlushRenderingCommands();
		}
		PreviewMaterial.Reset();
	}

	void FDreamShaderProbePreview::SetSource(const FString& InSourceFilePath)
	{
		const FString Normalized = UE::DreamShader::NormalizeSourceFilePath(InSourceFilePath);
		if (Normalized == SourceFilePath)
		{
			return;
		}
		ClearProbe();
		SourceFilePath = Normalized;
	}

	bool FDreamShaderProbePreview::SetProbe(const int32 Line, const FString& PreferredName, FString& OutError)
	{
		RequestedLine = FMath::Max(0, Line);
		RequestedName = PreferredName;
		if (RequestedLine <= 0)
		{
			ClearProbe();
			return true;
		}
		const bool bWired = Rewire(OutError);
		OnProbeChanged.Broadcast();
		return bWired;
	}

	void FDreamShaderProbePreview::ClearProbe()
	{
		const bool bWasActive = IsActive() || IsRequested();
		RequestedLine = 0;
		RequestedName.Reset();
		LastError.Reset();
		Resolved.Reset();
		// Mirrors the "stop previewing" branch of FMaterialEditor::SetPreviewExpression: the
		// collection is emptied, the material is left to be GC'd. It is kept alive here only until
		// the next probe (creating one is cheap; the point is not to keep touching shared nodes).
		ReleaseSharedExpressions();
		if (bWasActive)
		{
			OnProbeChanged.Broadcast();
		}
	}

	UMaterialInterface* FDreamShaderProbePreview::GetPreviewMaterial() const
	{
		return IsActive() ? PreviewMaterial.Get() : nullptr;
	}

	void FDreamShaderProbePreview::HandleGraphMaterialAboutToReset(UMaterial* GraphMaterial)
	{
		if (!GraphMaterial || SharedGraphMaterial.Get() != GraphMaterial)
		{
			return;
		}
		// The nodes are about to be marked garbage. Drop the share now; HandleTablePublished
		// re-establishes it (and re-resolves the line, which may have moved) once the new graph exists.
		ReleaseSharedExpressions();
		Resolved.Reset();
	}

	void FDreamShaderProbePreview::HandleTablePublished(const FDreamShaderGraphDebugTable& Table)
	{
		if (!IsRequested() || Table.SourceFilePath != SourceFilePath)
		{
			return;
		}
		FString Error;
		Rewire(Error);
		OnProbeChanged.Broadcast();
	}

	bool FDreamShaderProbePreview::Rewire(FString& OutError)
	{
		LastError.Reset();
		Resolved.Reset();
		ReleaseSharedExpressions();

		if (SourceFilePath.IsEmpty() || RequestedLine <= 0)
		{
			return false;
		}

		const TSharedPtr<const FDreamShaderGraphDebugTable> Table = FDreamShaderGraphDebugRegistry::Get().Find(SourceFilePath);
		if (!Table.IsValid())
		{
			OutError = TEXT("The source has not been generated yet; the breakpoint will attach after the next compile."); // I18N-EXEMPT: reaches the preview wire
			LastError = OutError;
			return false;
		}

		UMaterial* GraphMaterial = Table->GraphMaterial.Get();
		if (!GraphMaterial)
		{
			OutError = TEXT("The generated material is gone; recompile the source to re-attach the breakpoint."); // I18N-EXEMPT: reaches the preview wire
			LastError = OutError;
			return false;
		}

		const FDreamShaderGraphProbe* Probe = Table->ResolveProbe(RequestedLine, RequestedName);
		if (!Probe)
		{
			OutError = FString::Printf(TEXT("No Graph statement at or after line %d binds a value."), RequestedLine); // I18N-EXEMPT: reaches the preview wire
			LastError = OutError;
			return false;
		}
		if (!Probe->Expression.IsValid())
		{
			OutError = FString::Printf(TEXT("The node behind '%s' (line %d) no longer exists; recompile the source."), *Probe->Name, Probe->Line); // I18N-EXEMPT: reaches the preview wire
			LastError = OutError;
			return false;
		}

		if (!WireProbe(*Probe, GraphMaterial, OutError))
		{
			LastError = OutError;
			ReleaseSharedExpressions();
			return false;
		}

		FResolvedProbe& Out = Resolved.Emplace();
		Out.Line = Probe->Line;
		Out.Column = Probe->Column;
		Out.Name = Probe->Name;
		Out.ComponentCount = Probe->ComponentCount;
		Out.bIsMaterialAttributes = Probe->bIsMaterialAttributes;
		Out.bIsSubstrateMaterial = Probe->bIsSubstrateMaterial;
		Out.bIsTextureObject = Probe->bIsTextureObject;
		return true;
	}

	void FDreamShaderProbePreview::EnsurePreviewMaterial(UMaterial* GraphMaterial)
	{
		if (!PreviewMaterial.IsValid())
		{
			// FMaterialEditor::SetPreviewExpression, minus RF_Transactional: nothing here is undoable.
			UPreviewMaterial* NewPreview = NewObject<UPreviewMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
			NewPreview->bIsPreviewMaterial = true;
			PreviewMaterial.Reset(NewPreview);
		}

		UPreviewMaterial* Preview = PreviewMaterial.Get();
		// UI and post-process graphs only make sense previewed in their own domain (the engine's
		// IsPreviewUnlitMaterial rule keys off exactly this); everything else previews as a surface.
		if (GraphMaterial->IsUIMaterial())
		{
			Preview->MaterialDomain = MD_UI;
		}
		else if (GraphMaterial->IsPostProcessMaterial())
		{
			Preview->MaterialDomain = MD_PostProcess;
		}
		else
		{
			Preview->MaterialDomain = MD_Surface;
		}
		Preview->bEnableNewHLSLGenerator = GraphMaterial->IsUsingNewHLSLGenerator();
	}

	void FDreamShaderProbePreview::ResetPreviewMaterialInputs()
	{
		UPreviewMaterial* Preview = PreviewMaterial.Get();
		if (!Preview)
		{
			return;
		}
		// A previous probe may have used a different input (MaterialAttributes vs EmissiveColor);
		// only one may be live at a time.
		for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
		{
			if (FExpressionInput* Input = Preview->GetExpressionInputForProperty(static_cast<EMaterialProperty>(PropertyIndex)))
			{
				Input->Expression = nullptr;
				Input->OutputIndex = 0;
				Input->Mask = 0;
				Input->MaskR = 0;
				Input->MaskG = 0;
				Input->MaskB = 0;
				Input->MaskA = 0;
			}
		}
		Preview->bUseMaterialAttributes = false;
	}

	bool FDreamShaderProbePreview::WireProbe(const FDreamShaderGraphProbe& Probe, UMaterial* GraphMaterial, FString& OutError)
	{
		UMaterialExpression* Expression = Probe.Expression.Get();
		check(Expression);

		EnsurePreviewMaterial(GraphMaterial);
		UPreviewMaterial* Preview = PreviewMaterial.Get();

		// Share, don't copy: the very same UMaterialExpression objects, so the preview compiles the
		// live graph. Referenced textures/parameters are gathered from this collection by
		// UpdateCachedExpressionData inside PostEditChange, which is why an empty collection with a
		// bare EmissiveColor link would not do (the translator asserts on textures it cannot find).
		Preview->AssignExpressionCollection(GraphMaterial->GetExpressionCollection());
		SharedGraphMaterial = GraphMaterial;
		ResetPreviewMaterialInputs();

		UMaterialExpression* SourceExpression = Expression;
		int32 OutputIndex = Probe.OutputIndex;

		// A texture OBJECT has no color to show; sample it with default (TexCoord0) coordinates, the
		// way UMaterialExpressionTextureObject::CompilePreview does for the node thumbnail. The
		// sampler is owned by the preview material, so it dies with it and never enters the graph.
		if (Probe.bIsTextureObject)
		{
			UMaterialExpressionTextureSample* Sample = NewObject<UMaterialExpressionTextureSample>(Preview, NAME_None, RF_Transient);
			Sample->Material = Preview;
			Sample->TextureObject.Connect(OutputIndex, Expression);
			SourceExpression = Sample;
			OutputIndex = 0;
		}

#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
		if (Substrate::IsSubstrateEnabled())
		{
			// Substrate needs the BSDF/convert scaffolding the engine already knows how to build.
			SourceExpression->ConnectToPreviewMaterial(Preview, OutputIndex);
		}
		else
#endif
		if (Probe.bIsMaterialAttributes && !Probe.bIsTextureObject)
		{
			Preview->SetShadingModel(MSM_DefaultLit);
			Preview->bUseMaterialAttributes = true;
			FExpressionInput* Input = Preview->GetExpressionInputForProperty(MP_MaterialAttributes);
			check(Input);
			Input->Connect(OutputIndex, SourceExpression);
		}
		else
		{
			// Emissive is unaffected by lighting, so the mesh shows the value itself -- the same
			// choice UMaterialExpression::ConnectToPreviewMaterial makes.
			Preview->SetShadingModel(MSM_Unlit);
			Preview->bUseMaterialAttributes = false;
			FExpressionInput* Input = Preview->GetExpressionInputForProperty(MP_EmissiveColor);
			check(Input);
			Input->Connect(OutputIndex, SourceExpression);
		}

		// FExpressionInput::Connect copies the node OUTPUT's mask. DreamShader's value semantics put
		// the swizzle on the connection instead (see ConnectCodeValueToInput), so re-apply the
		// probe's mask -- or none -- on top, exactly as the generator would have when binding an
		// output. Only the emissive path can carry one.
		if (!Probe.bIsMaterialAttributes && !Probe.bIsSubstrateMaterial)
		{
			if (FExpressionInput* Input = Preview->GetExpressionInputForProperty(MP_EmissiveColor))
			{
				Input->Mask = Probe.bHasInputMask ? 1 : 0;
				Input->MaskR = Probe.bHasInputMask && Probe.bInputMaskR ? 1 : 0;
				Input->MaskG = Probe.bHasInputMask && Probe.bInputMaskG ? 1 : 0;
				Input->MaskB = Probe.bHasInputMask && Probe.bInputMaskB ? 1 : 0;
				Input->MaskA = Probe.bHasInputMask && Probe.bInputMaskA ? 1 : 0;
			}
		}

		RecompilePreviewMaterial();
		return true;
	}

	void FDreamShaderProbePreview::RecompilePreviewMaterial()
	{
		UPreviewMaterial* Preview = PreviewMaterial.Get();
		if (!Preview)
		{
			return;
		}
		// FMaterialEditor::UpdatePreviewMaterial's core: the update context (destructor) is what syncs
		// the render thread, rebuilds the shader map and re-registers users; PostEditChange alone
		// would not be safe on a material the render thread already holds.
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
		UpdateContext.AddMaterial(Preview);
		Preview->PreEditChange(nullptr);
		Preview->PostEditChange();
	}

	void FDreamShaderProbePreview::ReleaseSharedExpressions()
	{
		SharedGraphMaterial.Reset();
		if (UPreviewMaterial* Preview = PreviewMaterial.Get())
		{
			ResetPreviewMaterialInputs();
			Preview->GetExpressionCollection().Empty();
		}
	}
}
