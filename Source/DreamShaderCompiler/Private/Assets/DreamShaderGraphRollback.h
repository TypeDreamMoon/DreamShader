// Copyright (c) 2026 TypeDreamMoon. All rights reserved.
//
// Atomic regeneration: either the rebuilt graph replaces the old one, or the asset comes out of the
// compile exactly as it went in.
//
// Regeneration used to clear the graph and then build into the emptied asset, so any failure after
// the clear left the asset EMPTY -- and the parse stages that can still fail at that point are real
// ones (a syntax error inside a `Graph` block is parsed statement by statement, long after the
// teardown). An emptied material is bad; an emptied material FUNCTION is worse, because its call
// sites read their pins from the live asset, so one bad `.dsf` took every material that called it
// down with it. None of it was undoable either: generated assets are deliberately not
// RF_Transactional, because undo/redo desynchronizes the shader map.

#pragma once

#include "CoreMinimal.h"

#include "UObject/StrongObjectPtr.h"

class UMaterial;
class UMaterialExpression;
class UMaterialFunction;

namespace UE::DreamShader::Editor::Private
{
	/**
	 * Detaches the existing graph, keeps it alive, and puts it back if the rebuild does not finish.
	 *
	 * Construct it where the old teardown used to run: it empties the expression collection and nulls
	 * the material property inputs, exactly as clearing did, but it does NOT destroy anything. The
	 * generator then builds into the emptied asset as before, unchanged. Call Commit() once the build
	 * has fully succeeded; the destructor rolls back if you did not.
	 *
	 * How the restore can be complete without a hand-maintained list of "everything a rebuild
	 * touches": the snapshot is the asset's own serialized bytes (delta OFF, so every property is
	 * written, not just the ones differing from the class default). Object references serialize as raw
	 * pointers through FObjectWriter/FObjectReader, so restoring them yields the very same expression
	 * objects -- which is exactly why the detached graph has to be kept ALIVE rather than marked as
	 * garbage. That is the whole trick, and the reason destruction is deferred to Commit().
	 *
	 * Not covered, deliberately: the `DreamShader: ` comment boxes, which ClearDreamShaderGeneratedComments
	 * destroys before the snapshot is taken (it must run first, or the snapshot would capture pointers
	 * to comments that are already garbage). A failed compile therefore comes back with its graph
	 * intact but without those boxes. They are regenerated decoration -- the next successful compile
	 * recreates them -- and comments a user wrote are never prefixed, so they are never removed at all.
	 */
	class FDreamShaderGraphRollback
	{
	public:
		explicit FDreamShaderGraphRollback(UMaterial* InMaterial);
		explicit FDreamShaderGraphRollback(UMaterialFunction* InMaterialFunction);
		~FDreamShaderGraphRollback();

		FDreamShaderGraphRollback(const FDreamShaderGraphRollback&) = delete;
		FDreamShaderGraphRollback& operator=(const FDreamShaderGraphRollback&) = delete;

		/** The rebuild finished: destroy the old graph for good and stop guarding the asset. */
		void Commit();

		/** False when the snapshot could not be taken; the caller must then not rely on a rollback. */
		bool IsArmed() const { return bArmed; }

	private:
		void DetachGraph();
		void RestoreGraph();
		void DestroyDetachedExpressions();

		UMaterial* Material = nullptr;
		UMaterialFunction* MaterialFunction = nullptr;

		/**
		 * The asset's own serialized state from before the teardown -- render state, usage, and the
		 * rest of what lives directly on the UObject.
		 */
		TArray<uint8> Snapshot;

		/**
		 * And its editor-only data object's, which is where the graph actually lives. Since UE 5.1 a
		 * material keeps its expression collection and its material-property inputs on a SEPARATE
		 * UObject (UMaterialEditorOnlyData, UMaterialFunctionEditorOnlyData), reached through
		 * GetEditorOnlyData(). Serializing the material alone writes only the pointer to it, so a
		 * snapshot that stopped there restored nothing of the graph and rolled back silently into an
		 * empty asset -- which is exactly what the first run of the atomic tests measured.
		 */
		UObject* EditorOnlyData = nullptr;
		TArray<uint8> EditorOnlySnapshot;

		/**
		 * The detached graph, held strongly. Nothing else references these once they leave the
		 * expression collection, and generation can trip a GC on its way through
		 * (BuildTextureStreamingData collects), so without this the pointers in the snapshot would be
		 * restored onto objects that no longer exist.
		 */
		TArray<TStrongObjectPtr<UMaterialExpression>> DetachedExpressions;

		bool bArmed = false;
		bool bCommitted = false;
	};
}
