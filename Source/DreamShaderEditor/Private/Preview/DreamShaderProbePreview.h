#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Misc/Optional.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

class UMaterial;
class UMaterialInterface;
class UPreviewMaterial;

namespace UE::DreamShader::Editor::Private
{
	struct FDreamShaderGraphDebugTable;
	struct FDreamShaderGraphProbe;

	// The Material Editor's "Start Previewing Node", for a text source: instead of a node it takes a
	// (line, name) in the .dsm's Graph block, and instead of the editor viewport it hands the material
	// to whoever renders the session (FDreamShaderPreviewSession).
	//
	// The mechanism is the engine's own (FMaterialEditor::SetPreviewExpression /
	// UpdatePreviewMaterial): a transient UPreviewMaterial that SHARES the graph material's expression
	// collection, gets the probed node wired into its EmissiveColor (Unlit) -- or MaterialAttributes /
	// FrontMaterial when the value is one of those -- and is recompiled through FMaterialUpdateContext.
	// UPreviewMaterial's restricted ShouldCache keeps that recompile to a handful of shaders.
	//
	// Because the expressions are shared, not copied, two engine facts drive the lifecycle here:
	// - regenerating the source destroys every node, so the share must be released
	//   (FDreamShaderGraphDebugRegistry::OnGraphMaterialAboutToReset) and re-established afterwards
	//   (OnTablePublished), and
	// - stopping must Empty() the collection, or the preview material's later PostEditChange keeps
	//   touching the graph material's nodes.
	class FDreamShaderProbePreview
	{
	public:
		struct FResolvedProbe
		{
			int32 Line = 0;
			int32 Column = 0;
			FString Name;
			int32 ComponentCount = 0;
			bool bIsMaterialAttributes = false;
			bool bIsSubstrateMaterial = false;
			bool bIsTextureObject = false;
		};

		FDreamShaderProbePreview();
		~FDreamShaderProbePreview();

		FDreamShaderProbePreview(const FDreamShaderProbePreview&) = delete;
		FDreamShaderProbePreview& operator=(const FDreamShaderProbePreview&) = delete;

		// Which source the line numbers refer to. Changing it drops any active probe.
		void SetSource(const FString& InSourceFilePath);
		const FString& GetSource() const { return SourceFilePath; }

		// Activates the probe nearest at or after `Line` (see FDreamShaderGraphDebugTable::ResolveProbe).
		// `PreferredName` disambiguates when several bindings share the line. Returns false with a
		// reason when the source has no debug table yet, or nothing resolves; the request is still
		// remembered and retried on the next publish, so a breakpoint set before the first compile
		// lands as soon as it can.
		bool SetProbe(int32 Line, const FString& PreferredName, FString& OutError);
		void ClearProbe();

		bool IsRequested() const { return RequestedLine > 0; }
		bool IsActive() const { return Resolved.IsSet() && PreviewMaterial.IsValid(); }

		// The material to render while active; null otherwise.
		UMaterialInterface* GetPreviewMaterial() const;
		const TOptional<FResolvedProbe>& GetResolvedProbe() const { return Resolved; }
		// Empty when the last SetProbe/re-wire succeeded.
		const FString& GetLastError() const { return LastError; }

		// Fires after anything that changes what GetPreviewMaterial()/GetResolvedProbe() report --
		// activation, clearing, and every re-wire after a regeneration.
		DECLARE_MULTICAST_DELEGATE(FOnProbeChanged);
		FOnProbeChanged OnProbeChanged;

	private:
		void HandleGraphMaterialAboutToReset(UMaterial* GraphMaterial);
		void HandleTablePublished(const FDreamShaderGraphDebugTable& Table);

		bool Rewire(FString& OutError);
		bool WireProbe(const FDreamShaderGraphProbe& Probe, UMaterial* GraphMaterial, FString& OutError);
		void EnsurePreviewMaterial(UMaterial* GraphMaterial);
		void ResetPreviewMaterialInputs();
		void ReleaseSharedExpressions();
		void RecompilePreviewMaterial();

		FString SourceFilePath;
		int32 RequestedLine = 0;
		FString RequestedName;
		FString LastError;

		TStrongObjectPtr<UPreviewMaterial> PreviewMaterial;
		TWeakObjectPtr<UMaterial> SharedGraphMaterial;
		TOptional<FResolvedProbe> Resolved;

		FDelegateHandle AboutToResetHandle;
		FDelegateHandle PublishedHandle;
	};
}
