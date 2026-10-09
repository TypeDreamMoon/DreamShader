# Custom Pass runtime

> [DreamShader](../index.md) » **Custom Pass runtime**

What happens to a compiled [`.dsp` pipeline](../language-v2/passes.md) at run time: what makes it apply to a
view, when its passes run, what its buffers hold, and how to see what it did.

| | |
| :-- | :-- |
| Module | `DreamShaderPass` (Runtime, PostConfigInit) |
| Engine | Unreal Engine **5.8** and later, deferred renderer, desktop RHIs (D3D12, Vulkan, Metal). Below 5.8 the module builds with its asset types only (`DREAMSHADER_WITH_CUSTOM_PASS` is `0`); the mobile renderer is not supported |
| Asset | `UDreamPassPipeline` |
| Settings | Project Settings ▸ DreamPlugin ▸ **DreamShader Custom Pass** (`UDreamPassSettings`) |
| Console | `r.DreamPass.Enable`, `r.DreamPass.DisablePipelines`, `r.DreamPass.Visualize`, `DreamPass.Dump`, `stat DreamPass` |

## What makes a pipeline apply

| Source | Where | Applies to |
| :-- | :-- | :-- |
| Project settings | `UDreamPassSettings::GlobalPipelines` | every view of every world |
| Volume | `ADreamPassVolume` | views inside it — or every view when `bUnbound` — fading in over `BlendRadius` outside it |
| Component | `UDreamPassComponent` on any actor | `Scope = World`: every view of its world; `Scope = ViewTarget`: the views whose view target is its owner |
| API | `UDreamPassSubsystem::AddPipeline` / `RemovePipeline`, `SetParameter`, `SetWeight`; the same in Blueprint (`DreamShader \| Custom Pass`) | every view of the world, or one local player's |

Every source gives a pipeline, a priority, a weight (0–1) and parameter overrides. For each view, a
pipeline runs **once** however many sources ask for it:

- its parameters start from their defaults, and each activation's overrides are blended in by its weight,
  in ascending **priority** — numbers lerp; a bool, an int or a texture switch at half weight — so the
  highest priority wins, as with post-process volumes;
- its weight is the largest of its activations' weights; it does not run at weight 0. Every pass receives
  it as `DreamPassWeight` (a material scalar parameter of that name; `DP_Weight` in HLSL), which is how a
  volume's fade reaches the effect;
- pipelines with passes at the same injection point run in ascending **`Order`**, then by asset path; the
  passes of one pipeline in declaration order. Priority and Order are unrelated.

A pipeline is left out of a view whose kind is not in its `Views`, that lacks something its `Requires`
names, or where its `Enabled` parameter is false.

## When passes run

Each pass runs at its own [injection point](../language-v2/passes.md#injection-points). Under the hood these
are the engine's scene view extension callbacks plus its post-opaque render delegate. Every point of one view
family shares one render graph, so a buffer made at `BeginView` is still there at `EndOfView` — and is never
seen by another view family, such as a scene capture rendered in the same frame.

A point the view does not have is not reached, and its passes are counted as skipped:

| Point | Not reached when |
| :-- | :-- |
| `AfterBasePass` | path tracing |
| `BeforePostProcess` | the view does not resolve the scene (most scene capture sources), a debug view mode |
| `PostProcess.*` | post processing is off for the view |
| `PostProcess.TranslucencyAfterDOF` | no separate translucency, or DOF composited it already |

## Built-in buffers

| Name | Readable from | Writable at |
| :-- | :-- | :-- |
| `SceneColor` | `AfterBasePass` on | `AfterBasePass`, `AfterOpaque`, `BeforePostProcess`, `PostProcess.*` (except `TranslucencyAfterDOF`), `EndOfView` (the view family texture) |
| `SceneDepth` | `BeforeBasePass` on | — |
| `CustomDepth` | `AfterOpaque` on; `AfterBasePass` when `r.CustomDepth.Order` puts it first | — |
| `CustomStencil` | through the scene textures only (a `SceneTexture` node, `CalcSceneCustomStencil`) | — |
| `GBufferA`…`F`, `Velocity` | `AfterBasePass` on | `GBufferA`…`F` at `AfterBasePass` |
| `Translucency` | `PostProcess.*` | `PostProcess.TranslucencyAfterDOF` |

A write to the scene colour goes to a scratch texture and is copied back into the view's rect — or, on the
post-process chain, handed on as the chain's new colour — so a pass may read the scene colour while it
writes it.

Depth, custom depth, GBuffer and velocity reads keep their render-resolution view rect after the
colour chain is upscaled. Their input sizes and UV rects describe those original textures, while
`SceneColor` and `Translucency` use their own chain input rects.

## Buffers across frames

Buffers have one mip. Sources with `Mips` other than `1` are rejected, and older assets with such buffers are
skipped with a warning. Passes expose mip 0 only; no uninitialized higher mips are allocated or sampled.

`History = true` keeps a buffer per view from one frame to the next; `<Name>.Previous` reads last frame's
copy. A resized view starts its history over (black), and a view without a view state — a scene capture
with neither `bCaptureEveryFrame` nor `bAlwaysPersistRenderingState`, a thumbnail — has none at all: `.Previous` reads black there, with one warning.

## Exported buffers

`Export = true` copies a buffer, right after the last pass of the frame that writes it, into a render target
asset the compiler makes next to the pipeline (`<Pipeline>_<Buffer>`). Ordinary materials read it with
[`UE.DreamPassBuffer`](../builtins/dream-pass.md#uedreampassbuffer); Blueprints, UMG and Niagara use the
render target like any other (`UDreamPassBlueprintLibrary::GetExportTarget`).

If the planned last writer is skipped, an earlier successful write is exported at `EndOfView`.
If no writer succeeds, the frame does not replace the exported copy. A successful planned last writer
still exports immediately after its pass.

- **One view writes it**: the first view each frame that is a first local player's game view or an editor
  viewport. Split screen, other editor viewports and scene captures still use the buffer within their own
  frame, but never overwrite the exported copy.
- **Its size** follows that view's output size times the buffer's `Scale` (or its fixed size); a resize
  recreates the render target, so the first frame after one reads it cleared.
- **What a material reads** is whatever the render target held when the material rendered:

| The buffer's last writer runs at | an opaque material reads | a translucent material reads | UMG reads |
| :-- | :-- | :-- | :-- |
| `BeginView`, `BeforeBasePass` | this frame | this frame | this frame |
| `AfterBasePass`, `AfterOpaque` | **last frame** | this frame | this frame |
| `BeforePostProcess` and later | **last frame** | **last frame** | this frame |

- **When nothing fills it any more**, it is cleared to the buffer's `Clear` value (0 for `Clear = None`): at the
  end of the first frame that view renders without the pipeline -- removed, disabled, faded out of the view -- and
  at once when no pipeline can run in the world at all or the world ends. A frame without such a view, a minimised
  window or an editor viewport that is not real-time, leaves it as it is. So a material that reads it does not keep
  showing the last picture of an effect that is off.
- **Alpha**: the picture at `PostProcess.AfterTonemap` and later has an alpha of 0, and a `copy` of it keeps that.
  UMG multiplies an image by its texture's alpha, so the pass that fills an export meant for UMG writes 1 there --
  the [UI frosted glass](../examples/custom-pass.md#ui-frosted-glass) example does.

## Which views run pipelines

| View | Kind for `Views` |
| :-- | :-- |
| a game viewport (PIE included), one view per local player | `Game` |
| an editor viewport, perspective or orthographic (never its hit-proxy pass) | `Editor` |
| a scene capture, 2D or cube | `SceneCapture` |
| a planar reflection | `PlanarReflection` |
| a reflection capture | `ReflectionCapture` |
| a thumbnail or another render without a view state | `Thumbnail` |

## Debugging

| Tool | |
| :-- | :-- |
| `DreamPass.Dump` | every world's sources, lists and layers, and what the last frame ran and skipped, with the reason |
| `r.DreamPass.Visualize <Pipeline>.<Buffer>` | draws that buffer into the lower left of every view that runs the pipeline |
| `r.DreamPass.Enable 0` | turns every pipeline off for the session |
| `r.DreamPass.DisablePipelines CP_A,CP_B` | turns those off |
| `stat DreamPass` | passes run and failed, graph setup time |
| GPU captures, Insights | every pass is an event named `<Pipeline>.<Pass>` inside `DreamPass <injection point>` |

## Limits

- **Nanite primitives cannot be drawn again** by a mesh pass; see `Nanite =` in
  [Passes](../language-v2/passes.md#mesh).
- **Primitives drawn only through dynamic mesh elements** — CPU-skinned skeletal meshes, procedural
  meshes — are skipped by mesh passes, with one warning. GPU-skinned skeletal meshes, static meshes,
  instanced static meshes and landscape are drawn.
- **No per-instance culling** in mesh passes: an instanced static mesh's instances are drawn whenever its
  bounds are in view.
- **HLSL passes cannot ship in DLC**: their shaders are in the global shader cache of the base game.
- **First use may hitch**: the passes' pipeline states are not precached.
- **A material with `UE.DreamPassOutput` or `UE.DreamPassBuffer` cannot use the new material translator.**

## See also

- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md)
- [HLSL passes](hlsl.md)
- [Custom Pass material nodes](../builtins/dream-pass.md)
- [Custom Pass examples](../examples/custom-pass.md)
