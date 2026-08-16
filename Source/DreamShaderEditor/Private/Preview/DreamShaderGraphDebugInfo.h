#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class UMaterial;
class UMaterialExpression;
class UMaterialInterface;

namespace UE::DreamShader::Editor::Private
{
	// One "probe" = one place in a .dsm Graph block where a name was (re)bound to a value, plus the
	// exact node/pin/mask that value resolves to. This is the text-language equivalent of the Material
	// Editor's "Start Previewing Node": a node there is a (line, name) here.
	//
	// The expression pointer is weak on purpose. Regenerating a source destroys and recreates every
	// node, so a table outlives its nodes only until the next generation; consumers must check
	// IsValid() and re-resolve after FDreamShaderGraphDebugRegistry::OnTablePublished fires.
	struct FDreamShaderGraphProbe
	{
		// 1-based line/column in the SOURCE FILE (the Graph block's own offset already applied), i.e.
		// the same coordinates the generator's diagnostics report.
		int32 Line = 0;
		int32 Column = 0;

		// The bound name ("Color", "Attrs", ...). For an if statement this is the name whose merged
		// value the statement produced; the line is the `if` line itself.
		FString Name;

		// The value descriptor, mirroring FCodeValue's connection-relevant fields.
		TWeakObjectPtr<UMaterialExpression> Expression;
		int32 OutputIndex = 0;
		int32 ComponentCount = 1;
		bool bHasInputMask = false;
		bool bInputMaskR = false;
		bool bInputMaskG = false;
		bool bInputMaskB = false;
		bool bInputMaskA = false;
		bool bIsTextureObject = false;
		bool bIsMaterialAttributes = false;
		bool bIsSubstrateMaterial = false;

		// True when the statement that produced this probe named exactly this variable as its target
		// (`vec3 X = ...;` / `X = ...;`). False for names that changed as a side effect of the
		// statement (out-parameters of a call, names merged by an if). Used to pick the default probe
		// when several share a line.
		bool bIsStatementTarget = false;
	};

	// Everything the debugger-style preview needs to know about one generated .dsm.
	struct FDreamShaderGraphDebugTable
	{
		FString SourceFilePath;                    // normalized
		TWeakObjectPtr<UMaterial> GraphMaterial;   // the UMaterial that owns the nodes (ThinCustom base, or the Graph-backend material)
		TArray<FDreamShaderGraphProbe> Probes;     // sorted by (Line, Column, insertion order)
		uint64 Generation = 0;                     // bumped on every publish for this source

		// Selection used by breakpoints. Picks the probe for `Line`; when several share the line the
		// statement target wins, then the last one by column. `PreferredName` (a breakpoint's
		// condition text, say) overrides both when it matches a probe on that line. When no probe
		// sits on `Line`, snaps forward to the nearest later line that has one (like a debugger moving
		// a breakpoint off a blank/comment line); returns null past the last probe.
		const FDreamShaderGraphProbe* ResolveProbe(int32 Line, const FString& PreferredName = FString()) const;

		// All probes on exactly `Line`, in column order.
		void GetProbesOnLine(int32 Line, TArray<const FDreamShaderGraphProbe*>& OutProbes) const;
	};

	// Process-wide table store, keyed by normalized source path. Published by the generator every
	// time it (re)builds a .dsm graph; read by preview sessions. Everything here is game-thread only.
	class FDreamShaderGraphDebugRegistry
	{
	public:
		static FDreamShaderGraphDebugRegistry& Get();

		// Fired right before a graph material's expressions are torn down for regeneration. Anything
		// sharing that material's expression collection (a probe preview material) must let go of it
		// here, or it ends up holding pointers to garbage-marked nodes.
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphMaterialAboutToReset, UMaterial* /*GraphMaterial*/);
		FOnGraphMaterialAboutToReset OnGraphMaterialAboutToReset;

		// Fired after a table replaces the previous one for its source.
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnTablePublished, const FDreamShaderGraphDebugTable& /*Table*/);
		FOnTablePublished OnTablePublished;

		void Publish(const FString& SourceFilePath, UMaterial* GraphMaterial, TArray<FDreamShaderGraphProbe>&& Probes);
		void Remove(const FString& SourceFilePath);

		// Null when the source was never generated in this process (or was removed).
		TSharedPtr<const FDreamShaderGraphDebugTable> Find(const FString& SourceFilePath) const;

		// Convenience for callers that only hold the material.
		TSharedPtr<const FDreamShaderGraphDebugTable> FindByGraphMaterial(const UMaterial* GraphMaterial) const;

		void NotifyGraphMaterialAboutToReset(UMaterial* GraphMaterial);

	private:
		TMap<FString, TSharedPtr<FDreamShaderGraphDebugTable>> Tables;
		uint64 NextGeneration = 1;
	};
}
