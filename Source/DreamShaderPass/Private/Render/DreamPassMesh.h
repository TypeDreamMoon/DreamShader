#pragma once

#include "CoreMinimal.h"

#if DREAMSHADER_WITH_CUSTOM_PASS

#include "Render/DreamPassSnapshot.h"

class UDreamPassSubsystem;

/**
 * Mesh passes (Render/DreamPassMesh.cpp). The render thread half is ExecuteMeshPass, declared with the other executors
 * in Render/DreamPassFrame.h: selection by the pass's filter, culling, the mesh pass processor and its material shaders
 * (Shaders/Pass/DreamPassMesh.usf), and the Nanite fill (Shaders/Pass/DreamPassStencilMask.usf). The game thread half
 * is below.
 */
namespace UE::DreamPass
{
	/**
	 * Game thread, once per family snapshot: gives the Nanite members of every layer or list a mesh pass with
	 * `Nanite = AssignStencil(...)` selects custom depth and the pass's stencil value, and gives back what they had
	 * when no active pass asks for it any more (UDreamPassSubsystem releases them at the end of a frame no pass asked
	 * in). Nanite primitives cannot be drawn again by a mesh pass (E/Private/Rendering/NaniteResources.cpp:691-693), so
	 * this is how they reach the pass's stencil mask. Game and PIE worlds only: an editor world would save the change.
	 */
	void UpdateNaniteStencilAssignments(UDreamPassSubsystem& Subsystem, const FFamilySnapshot& Snapshot);
}

#endif // DREAMSHADER_WITH_CUSTOM_PASS
