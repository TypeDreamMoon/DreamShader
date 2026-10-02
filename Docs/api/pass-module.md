# DreamShaderPass

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderPass**

The Custom Pass runtime: the asset a [`.dsp`](../language-v2/passes.md) compiles to, what decides which
pipelines apply to a view, and the scene view extension that runs their passes inside the renderer. This
page is the module's C++ and Blueprint surface; what the passes do at run time is
[Custom Pass runtime](../runtime/index.md).

| | |
| :-- | :-- |
| Module | `DreamShaderPass` (**Runtime**, `PostConfigInit` loading phase) |
| Engines | every engine the plugin supports; the render side only on 5.8 and later — see [`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass) |
| Export macro | `DREAMSHADERPASS_API` |
| Log category | `LogDreamPass` |
| Public dependencies | `Core`, `CoreUObject`, `DeveloperSettings`, `DreamShader`, `Engine`, `RenderCore`, `RHI` |
| Private dependencies | `Projects`, `Renderer` |
| Blueprint category | `DreamShader \| Custom Pass` |

## Public headers

| Header | Contents |
| :-- | :-- |
| `DreamShaderPassModule.h` | `LogDreamPass`; the slot counts; where the HLSL slot registry and snapshots live |
| `DreamPassTypes.h` | the enums and descriptors a pipeline is made of — injection points, pass kinds, buffer formats, parameters and their values, bindings, mesh filters — and the `.dsp` spellings of the enums |
| `DreamPassPipeline.h` | `UDreamPassPipeline`, the asset |
| `DreamPassSettings.h` | `UDreamPassSettings` — the [project settings](../settings/project.md#dreamshader-custom-pass) |
| `DreamPassSubsystem.h` | `UDreamPassSubsystem`, `IDreamPassActivationSource`, `FDreamPassHandle` |
| `DreamPassBlueprintLibrary.h` | `UDreamPassBlueprintLibrary` |
| `DreamPassVolume.h` | `ADreamPassVolume` |
| `DreamPassComponent.h` | `UDreamPassComponent`, `EDreamPassComponentScope` |
| `DreamPassLayerComponent.h` | `UDreamPassLayerComponent` ("Dream Pass Layers") — gives the primitives of its actor the pass layers a mesh pass selects with `Filter = Layer(...)` |
| `Materials/MaterialExpressionDreamPassOutput.h` | `UMaterialExpressionDreamPassOutput` — [`UE.DreamPassOutput`](../builtins/dream-pass.md#uedreampassoutput) |
| `Materials/MaterialExpressionDreamPassBuffer.h` | `UMaterialExpressionDreamPassBuffer` — [`UE.DreamPassBuffer`](../builtins/dream-pass.md#uedreampassbuffer); its static `ResolveExportTarget(Pipeline, Buffer, OutProblem)` is the check both the node and DreamShader use |

## `DREAMSHADER_WITH_CUSTOM_PASS`

A public definition of the module, set by `DreamShaderPass.Build.cs` from the target engine: `1` on 5.8 and
later, `0` below — or on any engine built with `-ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS`, for testing.
Every module that depends on this one reads the same answer.

- **Reflected types exist on every engine** — the asset, the settings, the subsystem, the volume and the
  components compile anywhere, so a project that opens on an older engine still loads and saves them.
- **Everything that touches the renderer is compiled out at `0`**: the scene view extension, the passes, the
  global and mesh-pass shaders. Nothing runs, and the module says so once in the log at startup.

## Activating a pipeline

From C++, through the world's subsystem:

```cpp
#include "DreamPassSubsystem.h"

UDreamPassSubsystem* Passes = UDreamPassSubsystem::Get(GetWorld());
FDreamPassHandle Handle = Passes->AddPipeline(HighlightPipeline, /*Priority*/ 0.0f, /*Overrides*/ {});
Passes->SetParameter(Handle, TEXT("Thickness"), FDreamPassParameterValue::MakeFloat(3.0f));
Passes->SetWeight(Handle, 0.5f);
// ...
Passes->RemovePipeline(Handle);
```

From Blueprints, the same calls are on `UDreamPassBlueprintLibrary` with a world context, plus
`SetFloatParameter`, `SetColorParameter`, `SetBoolParameter`, `SetIntParameter`, `SetTextureParameter` and the
`Make…Value` helpers.

Or place a source in the level — an `ADreamPassVolume`, a `UDreamPassComponent` — or list the pipeline in the
project settings. How the four kinds combine is in
[What makes a pipeline apply](../runtime/index.md#what-makes-a-pipeline-apply).

## `UDreamPassSubsystem`

A `UWorldSubsystem` of game, PIE and editor worlds; an editor preview world has none. Game thread only.

| Function | |
| :-- | :-- |
| `static Get(const UWorld*)` | the world's subsystem, or null |
| `AddPipeline(Pipeline, Priority, Overrides, PlayerIndex = -1)` | runs the pipeline in every view of the world — or one local player's — until `RemovePipeline`; returns its handle |
| `RemovePipeline(Handle)` · `IsActive(Handle)` | |
| `SetParameter(Handle, Name, Value)` | sets or adds one override; false for an unknown handle or parameter, or a value of the wrong type |
| `SetWeight(Handle, Weight)` | 0–1 |
| `AddToList(List, Primitive)` · `RemoveFromList` · `ClearList` | the named lists a mesh pass selects with `Filter = List(Name)` |
| `RegisterSource(Object, Source)` · `UnregisterSource(Object)` | an [activation source](#writing-an-activation-source) of your own |
| `ResolveView(Query, OutPipelines)` | every pipeline that applies to a view, blended, in execution order — what the renderer is given each frame |
| `DumpState(Ar)` | what `DreamPass.Dump` prints for this world |

`FDreamPassHandle` is a `BlueprintType` struct; a default one is not valid (`IsValid()`).

## `ADreamPassVolume`

An `AVolume` that runs its pipeline in the views inside it, with a post-process volume's arithmetic.

| Property | Default | |
| :-- | :-- | :-- |
| `Pipeline` | none | none: the volume does nothing |
| `bEnabled` | `true` | |
| `bUnbound` | `false` | applies to every view of the world, wherever it is; an unbound volume is not spatially loaded |
| `Priority` | `0` | where its overrides sit among every activation of the same pipeline: higher wins |
| `BlendRadius` | `100` | cm outside the volume over which its weight fades from `BlendWeight` to 0; below 1, a hard edge |
| `BlendWeight` | `1` | 0–1, inside the volume |
| `Overrides` | empty | parameter values; one left out keeps what lower priorities give it |

`SetEnabled`, `SetBlendWeight` and `SetParameter(Name, Value)` are Blueprint-callable; `GetWeightAt(Location)`
is the weight a view there would get. Outside a game world the volume follows its editor visibility: hiding it
in the outliner takes it out of the editor viewports. A volume spawned at runtime has no brush, and applies only
when unbound.

## `UDreamPassComponent`

An actor component that runs its pipeline from any actor.

| Property | Default | |
| :-- | :-- | :-- |
| `Pipeline` | none | |
| `bEnabled` | `true` | |
| `Priority` | `0` | |
| `Weight` | `1` | 0–1 |
| `Scope` | `World` | `World`: every view of its world · `ViewTarget`: the views whose view target is the owner — the player looking through it and anyone spectating it |
| `PlayerIndex` | `-1` | only that local player's views (the controller id); `-1`: any view, editor viewports included |
| `Overrides` | empty | |

`SetEnabled`, `SetWeight`, `SetParameter`, `SetFloatParameter` and `SetColorParameter` are Blueprint-callable;
`SetColorParameter` sets a `float3` or a `float4` parameter, whichever the pipeline declares.

## Writing an activation source

Anything can activate pipelines by implementing `IDreamPassActivationSource` and registering with the
subsystem while it should apply:

```cpp
class FMyGameModeSource : public IDreamPassActivationSource
{
public:
    virtual void GatherDreamPassActivations(const FDreamPassViewQuery& View, TArray<FDreamPassActivation>& Out) const override
    {
        if (View.PlayerIndex == 0)   // the first player only
        {
            FDreamPassActivation& Activation = Out.AddDefaulted_GetRef();
            Activation.Pipeline = Pipeline;
            Activation.Weight = Fade;
        }
    }

    UDreamPassPipeline* Pipeline = nullptr;   // keep it referenced elsewhere: the subsystem does not
    float Fade = 1.0f;
};

Subsystem->RegisterSource(Owner, &Source);   // Owner: the UObject whose lifetime bounds the registration
Subsystem->UnregisterSource(Owner);
```

`GatherDreamPassActivations` is called on the game thread for every view of every frame, so it should be
cheap. `FDreamPassViewQuery` gives the view's location, local player, view target, kind and capabilities; an
activation's `Overrides` is a view into storage the source owns, valid for the call.

## `UDreamPassPipeline`

The compiled pipeline. Its fields mirror the `.dsp` — `Order`, `DefaultInjection`, `Views`, `Requires`,
`EnabledParameter`, `Parameters`, `Buffers`, `Passes`, the exported render targets — and `SourceFilePath` and
`SourceHash` say which source built it. Edit the `.dsp`, not the asset: a compile overwrites it, and an edit made
in the details panel is a [divergence](../generation/divergence.md) to adopt or discard. A pass's HLSL written in the
`.dsp` is kept in the editor's data only -- `HlslSource` on the pass says where its code is, `InlineHlsl` and
`InlineHlslLine` hold its own `hlsl` block, and the pipeline's `SharedHlsl` the file's -- for the way back to text:
what runs is the slot's snapshot, and a cook carries no HLSL text.

| Member | |
| :-- | :-- |
| `GetExportTarget(Buffer)` | the render target an exported buffer is copied into; Blueprint-pure |
| `FindBuffer(Name)` · `FindParameter(Name)` · `FindPassIndex(Name)` | lookups |
| `Validate(OutProblems)` | the checks the runtime relies on, for the whole asset |
| `NotifyChanged()` · `OnPipelineChanged` | after an edit — the compiler calls it after writing the asset, `PostLoad` and the details panel too: re-checks every pass, skipping one that fails with a warning naming the problem, and broadcasts the static `OnPipelineChanged`. The renderer reads the asset afresh every frame |

## The HLSL slot registry

| Function (`UE::DreamPass`) | Returns |
| :-- | :-- |
| `GetUserShaderDirectory()` | `<project source root>/.dreampass` — commit it |
| `GetUserShaderVirtualDirectory()` | `/DreamPassUser` |
| `GetRegistryFilePath(bCompute)` | `RegistryCompute.ush` or `RegistryPixel.ush` in it |
| `GetSlotDirectory(bCompute, Slot)` · `GetSlotVirtualDirectory` | `Slots/C07`, `Slots/P07`: one slot's snapshot |
| `GetComputeSlotCount()` · `GetPixelSlotCount()` | `DREAMSHADER_PASS_COMPUTE_SLOTS` (32), `DREAMSHADER_PASS_PIXEL_SLOTS` (16) |

The counts are overridable from a `Target.cs` (`GlobalDefinitions.Add("DREAMSHADER_PASS_COMPUTE_SLOTS=48")`);
raising one recompiles the two global shader types, nothing else. How the registry is written is
[HLSL passes](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine).

## See also

- [Custom Pass runtime](../runtime/index.md)
- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md)
- [HLSL passes](../runtime/hlsl.md)
- [Custom Pass material nodes](../builtins/dream-pass.md)
- [Project settings](../settings/project.md#dreamshader-custom-pass)
