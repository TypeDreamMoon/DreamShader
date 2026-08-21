// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// DreamShader-generated material/function graph teardown. This file used to hold two "clear the
// graph" helpers that destroyed the old nodes on the spot; it now holds FDreamShaderGraphRollback,
// which detaches them instead and destroys them only once the rebuild has succeeded. See
// DreamShaderGraphRollback.h for why.
//
// The old teardown had two deletion strategies -- node-by-node through the material editing library
// below a threshold, a single mark-as-garbage sweep above it -- because
// UMaterialEditingLibrary::DeleteMaterialExpression breaks inbound links by scanning every other
// expression, which is O(n^2) across a whole graph. Detaching the graph as a unit makes that scan
// pointless (nothing that stays behind can still point into what left), so there is now one path,
// and it is the fast one.

#include "DreamShaderGraphRollback.h"

#include "DreamShaderMaterialGeneratorPrivate.h"
#include "DreamShaderModule.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialFunction.h"
#include "SceneTypes.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"

namespace UE::DreamShader::Editor::Private
{
	namespace
	{
		FMaterialExpressionCollection* GetExpressionCollection(UMaterial* Material, UMaterialFunction* MaterialFunction)
		{
			if (Material)
			{
				return &Material->GetExpressionCollection();
			}
			if (MaterialFunction)
			{
				return &MaterialFunction->GetExpressionCollection();
			}
			return nullptr;
		}

		// The object the graph actually lives on. See FDreamShaderGraphRollback::EditorOnlySnapshot.
		UObject* GetEditorOnlyData(UMaterial* Material, UMaterialFunction* MaterialFunction)
		{
			if (Material)
			{
				return Material->GetEditorOnlyData();
			}
			if (MaterialFunction)
			{
				return MaterialFunction->GetEditorOnlyData();
			}
			return nullptr;
		}
	}

	FDreamShaderGraphRollback::FDreamShaderGraphRollback(UMaterial* InMaterial)
		: Material(InMaterial)
	{
		DetachGraph();
	}

	FDreamShaderGraphRollback::FDreamShaderGraphRollback(UMaterialFunction* InMaterialFunction)
		: MaterialFunction(InMaterialFunction)
	{
		DetachGraph();
	}

	FDreamShaderGraphRollback::~FDreamShaderGraphRollback()
	{
		if (bCommitted)
		{
			return;
		}

		if (bArmed)
		{
			RestoreGraph();
			return;
		}

		// Never armed: there is nothing to put back, but the detached graph (if any) still has to be
		// released rather than leaked into the root set.
		DestroyDetachedExpressions();
	}

	void FDreamShaderGraphRollback::DetachGraph()
	{
		UObject* Asset = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(MaterialFunction);
		FMaterialExpressionCollection* Collection = GetExpressionCollection(Material, MaterialFunction);
		if (!Asset || !Collection)
		{
			return;
		}

		// Delta OFF. With it on, a property equal to the class default is skipped, and restoring would
		// then leave whatever the failed build had set it to -- so a material whose source turned
		// TwoSided on would come back two-sided.
		FObjectWriter(Asset, Snapshot, /*bIgnoreClassRef*/ false, /*bIgnoreArchetypeRef*/ false, /*bDoDelta*/ false);

		// Both halves, or neither: the render state is on the asset and the graph is on its editor-only
		// data, so a rollback holding one without the other would restore a coherent-looking asset that
		// is missing half of what it had.
		EditorOnlyData = GetEditorOnlyData(Material, MaterialFunction);
		if (EditorOnlyData)
		{
			FObjectWriter(EditorOnlyData, EditorOnlySnapshot, /*bIgnoreClassRef*/ false, /*bIgnoreArchetypeRef*/ false, /*bDoDelta*/ false);
		}

		bArmed = Snapshot.Num() > 0 && EditorOnlyData != nullptr && EditorOnlySnapshot.Num() > 0;
		if (!bArmed)
		{
			UE_LOG(
				LogDreamShader,
				Warning,
				TEXT("DreamShader could not snapshot '%s' before rebuilding it; a failed rebuild will not be rolled back."),
				*Asset->GetPathName());
		}

		DetachedExpressions.Reserve(Collection->Expressions.Num());
		for (const TObjectPtr<UMaterialExpression>& Expression : Collection->Expressions)
		{
			if (Expression)
			{
				DetachedExpressions.Emplace(Expression.Get());
			}
		}

		if (Material)
		{
			for (int32 MaterialPropertyIndex = 0; MaterialPropertyIndex < MP_MAX; ++MaterialPropertyIndex)
			{
				if (FExpressionInput* ExpressionInput = Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(MaterialPropertyIndex)))
				{
					ExpressionInput->Expression = nullptr;
				}
			}

			// The engine's own per-node deletion maintains this cache one entry at a time
			// (RemoveExpressionParameter); emptying the graph in one go means emptying it in one go.
			Material->EditorParameters.Reset();
		}

		Collection->Expressions.Empty();
		Collection->ExpressionExecBegin = nullptr;
		Collection->ExpressionExecEnd = nullptr;
	}

	void FDreamShaderGraphRollback::RestoreGraph()
	{
		UObject* Asset = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(MaterialFunction);
		FMaterialExpressionCollection* Collection = GetExpressionCollection(Material, MaterialFunction);
		if (!Asset || !Collection)
		{
			return;
		}

		// What the failed attempt managed to build, captured before the restore overwrites the
		// collection with the old contents and drops the only reference to it.
		TArray<UMaterialExpression*> Abandoned;
		Abandoned.Reserve(Collection->Expressions.Num());
		for (const TObjectPtr<UMaterialExpression>& Expression : Collection->Expressions)
		{
			if (Expression)
			{
				Abandoned.Add(Expression.Get());
			}
		}

		FObjectReader(Asset, Snapshot);
		if (EditorOnlyData)
		{
			FObjectReader(EditorOnlyData, EditorOnlySnapshot);
		}

		// Whatever came back is referenced by the asset again, so the strong handles can go. Dropping
		// them BEFORE the sweep below also keeps the set membership test honest: the two lists are
		// disjoint (a build only ever creates new nodes), and the test is belt and braces.
		TSet<UMaterialExpression*> Restored;
		Restored.Reserve(DetachedExpressions.Num());
		for (const TStrongObjectPtr<UMaterialExpression>& Expression : DetachedExpressions)
		{
			Restored.Add(Expression.Get());
		}
		DetachedExpressions.Empty();

		for (UMaterialExpression* Expression : Abandoned)
		{
			if (Expression && !Restored.Contains(Expression))
			{
				EnsureExpressionCanBeDeleted(Expression);
				Expression->MarkAsGarbage();
			}
		}

		// No recompile here on purpose. The asset now holds exactly the graph and the render state its
		// existing shader map was built from, and nothing between the detach and a failure recompiles
		// anything -- so asking for one would only pay for a rebuild of the map it already has.
		UE_LOG(
			LogDreamShader,
			Verbose,
			TEXT("DreamShader restored '%s' after a failed generation: %d node(s) back, %d abandoned."),
			*Asset->GetPathName(),
			Restored.Num(),
			Abandoned.Num());
	}

	void FDreamShaderGraphRollback::DestroyDetachedExpressions()
	{
		for (const TStrongObjectPtr<UMaterialExpression>& Expression : DetachedExpressions)
		{
			if (UMaterialExpression* DetachedExpression = Expression.Get())
			{
				EnsureExpressionCanBeDeleted(DetachedExpression);
				DetachedExpression->MarkAsGarbage();
			}
		}
		DetachedExpressions.Empty();
	}

	void FDreamShaderGraphRollback::Commit()
	{
		if (bCommitted)
		{
			return;
		}

		bCommitted = true;
		DestroyDetachedExpressions();
		Snapshot.Empty();
		EditorOnlySnapshot.Empty();

		if (MaterialFunction)
		{
			// A UMaterialFunction keeps a SECOND, serialized list of its function-call nodes that the
			// expression collection knows nothing about, and `UMaterialFunction::IterateDependentFunctions`
			// walks it with no null check (MaterialExpressions.cpp:14406). The nodes it points at were
			// just marked as garbage, so GC would null the entries and the next
			// `ForceRecompileForRendering` -- which iterates every loaded UMaterialInterface, so any
			// later compile of any file reaches it -- would dereference null.
			//
			// This is why it lives at COMMIT and not at teardown: the engine refreshes the list from
			// exactly two places, PostLoad and ForceRecompileForRendering, so resetting it early used to
			// be the only thing standing between a failed compile and a crash for the rest of the
			// session. A failed compile no longer empties the function at all, so that window is closed
			// from the other side; this reset is now simply bookkeeping for a rebuild that succeeded.
			MaterialFunction->DependentFunctionExpressionCandidates.Reset();
		}
	}
}
