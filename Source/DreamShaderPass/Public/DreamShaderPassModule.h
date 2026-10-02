#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DREAMSHADERPASS_API DECLARE_LOG_CATEGORY_EXTERN(LogDreamPass, Log, All);

/**
 * How many global shader slots each HLSL pass kind has. A slot is one permutation of FDreamPassCS or FDreamPassPS;
 * every slot is compiled whether a pass uses it or not (an unused one is an empty stub), because cooked builds
 * evaluate the permutation list again and a missing permutation is fatal there. Raising a count recompiles both
 * shader types for every platform, nothing else. Overridable from a Target.cs with GlobalDefinitions.
 */
#ifndef DREAMSHADER_PASS_COMPUTE_SLOTS
#define DREAMSHADER_PASS_COMPUTE_SLOTS 32
#endif

#ifndef DREAMSHADER_PASS_PIXEL_SLOTS
#define DREAMSHADER_PASS_PIXEL_SLOTS 16
#endif

namespace UE::DreamPass
{
	/** `/DreamPassUser`: the virtual shader directory the HLSL slot registry and snapshots are read from. */
	DREAMSHADERPASS_API const FString& GetUserShaderVirtualDirectory();

	/**
	 * `<project source root>/.dreampass`, absolute: where the compiler writes the registry and the snapshots of
	 * every `.usf` that passed its pre-check. Committed with the sources -- a teammate and a cook build the
	 * global shaders from it, never from the `.usf` files themselves.
	 */
	DREAMSHADERPASS_API FString GetUserShaderDirectory();

	/** `<user shader directory>/Registry<Kind>.ush` for Kind = Compute or Pixel. */
	DREAMSHADERPASS_API FString GetRegistryFilePath(bool bCompute);

	/** `<user shader directory>/Slots/<C|P><NN>`: where one slot's snapshot lives. */
	DREAMSHADERPASS_API FString GetSlotDirectory(bool bCompute, int32 Slot);

	/** The virtual path of the same directory. */
	DREAMSHADERPASS_API FString GetSlotVirtualDirectory(bool bCompute, int32 Slot);

	inline constexpr int32 GetComputeSlotCount() { return DREAMSHADER_PASS_COMPUTE_SLOTS; }
	inline constexpr int32 GetPixelSlotCount() { return DREAMSHADER_PASS_PIXEL_SLOTS; }

	/** The text of a registry that defines no slot: every slot compiles to its stub. */
	DREAMSHADERPASS_API FString MakeEmptyRegistryText(bool bCompute);
}

class FDreamShaderPassModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
