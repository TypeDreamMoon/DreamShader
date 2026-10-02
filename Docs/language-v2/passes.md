# Custom Pass pipelines — `.dsp`

> [DreamShader](../index.md) » [DreamShaderLang 2.0](index.md) » **Custom Pass pipelines**

A `.dsp` file is one `UDreamPassPipeline`: named buffers, an ordered list of passes that each run at an
injection point of the renderer of their choosing, and the parameters an activation can override. It is
how you add a render pass to Unreal without touching the engine — a mask of selected objects, a blur, a
screen-space effect, a compute shader that writes a data texture.

| | |
| :-- | :-- |
| Produces | one `UDreamPassPipeline`, named after the file, beside where a `.dss` of the same folder puts its product; plus one `UTextureRenderTarget2D` per exported buffer |
| Runs in | the **DreamShaderPass** runtime module — Unreal Engine **5.8** and later. On an older engine a `.dsp` still parses, formats and binds; compiling it is refused |
| Passes | `fullscreen` (a Post Process material, or HLSL), `compute` (HLSL), `mesh` (selected primitives drawn again), `clear`, `copy` |
| Activated by | Project Settings, `ADreamPassVolume`, `UDreamPassComponent`, Blueprint / C++ — see [Custom Pass runtime](../runtime/index.md) |
| Since | `2.1.0` |

```hlsl
// DShader/Passes/CP_Highlight.dsp — selected objects get an orange outline, also behind walls
#pragma pipeline(Order = 100)

/// @group Look
uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);
/// @group Look   @slider 1 8
uniform float  OutlineWidth = 3.0;

buffer Mask    : R8(Clear = 0);
/// @desc The blurred mask; the ground material reads it too.
buffer Blurred : R8(Scale = 0.5, Export = true);

pass DrawMask : mesh
{
    Injection = AfterOpaque;
    Filter    = Layer(Highlight);
    Material  = "M_HighlightMask";
    Depth     = None;
    write Output0 = Mask;
}

pass Blur : compute
{
    Injection = AfterOpaque;
    Shader    = "Passes/BoxBlur.usf";
    Entry     = BlurCS;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;
}

pass Composite : fullscreen
{
    Injection = BeforePostProcess;
    Material  = "PP_OutlineComposite";
    read  Mask;
    read  Blurred;
    write SceneColor;
    param Color = OutlineColor;
}
```

The materials are ordinary `.dss` files: a `#pragma material(Domain = PostProcess, ...)` material reads
the pass's buffers through `UE.UserSceneTexture`, and a mesh pass's material writes its outputs through
`UE.DreamPassOutput`. See [Custom Pass material nodes](../builtins/dream-pass.md). Four complete
examples, with their `.dss` and `.usf`, are in [Custom Pass examples](../examples/custom-pass.md).

## The file

| Declaration | Form | |
| :-- | :-- | :-- |
| `#pragma pipeline(...)` | at most one | [Pipeline keys](#pipeline-keys) |
| `uniform` | `uniform <type> <Name> [= <value>];` | a parameter activations can override: `float`–`float4`, `int`, `bool`, `Texture2D` (with `/// @default <asset>`). `///` takes `@group`, `@desc`, `@slider`, `@sort` |
| `static const` | `static const <type> <Name> = <value>;` | a constant, folded where it is used |
| `buffer` | `buffer <Name> : <Format>[(<Key> = <Value>, ...)];` | [Buffers](#buffers) |
| `pass` | `pass <Name> : <kind> { ... }` | [Passes](#passes); they run in declaration order within an injection point |

`buffer` and `pass` are keywords in a `.dsp` only, and `read`, `write`, `param` inside a pass only, so a
`.dss` reads exactly as before. A `.dsp` has no functions and, for now, no `#include`. Comments survive
`dsc fmt` and Adopt.

## Pipeline keys

| Key | Value | Default |
| :-- | :-- | :-- |
| `Order` | integer — pipelines with passes at one injection point run in ascending `Order`, then by asset path | `0` |
| `Injection` | the [injection point](#injection-points) of a pass that names none | `BeforePostProcess` |
| `Views` | `Game`, `Editor`, `SceneCapture`, `PlanarReflection`, `ReflectionCapture`, `Thumbnail`, joined with `\|` | `Game \| Editor` |
| `Requires` | `PostProcess`, `SceneResolve`, `CustomStencil` — in a view that lacks one, the whole pipeline is off rather than half of it | none |
| `Enabled` | a `bool` uniform, or a constant | `true` |

## Injection points

Every pass names its own; the pipeline only gives the default.

| Point | Where in the frame | The scene textures there |
| :-- | :-- | :-- |
| `BeginView` | before visibility, before anything is drawn | none — no SceneColor, no SceneDepth, no `View` uniform buffer (see [HLSL passes](../runtime/hlsl.md)) |
| `BeforeBasePass` | after the depth prepass | SceneDepth |
| `AfterBasePass` | the GBuffer is written, nothing is lit | GBuffer A–F, Velocity, SceneDepth; SceneColor holds emissive only; CustomDepth if `r.CustomDepth.Order` puts it first (the default with DBuffer decals) |
| `AfterOpaque` | lighting, fog and sky done, translucency not | everything opaque, CustomDepth / CustomStencil |
| `BeforePostProcess` | everything but the post-process chain | everything; translucency rendered after DOF is still separate |
| `PostProcess.BeforeDOF`, `PostProcess.AfterDOF` | inside the post-process chain, render resolution | the chain's colour, all scene textures |
| `PostProcess.TranslucencyAfterDOF` | the after-DOF translucency, before it is composited | `Translucency` is the chain here; SceneColor is read-only. The engine skips this point when there is no separate translucency |
| `PostProcess.ReplaceTonemapper` | instead of the tonemapper — one pipeline at most, and only one extension in the process gets it | SceneColor, bloom; must produce the final colour |
| `PostProcess.AfterMotionBlur`, `PostProcess.AfterTonemap`, `PostProcess.AfterFXAA` | after the upscaler, output resolution | the chain's colour |
| `EndOfView` | the finished view, before the UI | the view family texture (LDR) |

A point a view does not have — post processing turned off, a scene capture that does not resolve, path
tracing for `AfterBasePass` — is simply not reached there; its passes are counted as skipped by
`DreamPass.Dump`, and a buffer they would have written reads as its `Clear` value.

## Buffers

```hlsl
buffer <Name> : <Format>[(<Key> = <Value>, ...)];
```

Formats: `R8 RG8 RGBA8 R16F RG16F RGBA16F R32F RG32F RGBA32F Depth32` — `Depth32` only as a mesh pass's own
depth. `R32U` and `RG32U` are reserved: no pass reads or writes an integer buffer yet (`DSH7321`); an id fits
`R32F` exactly up to 16777216.

| Key | Value | Default |
| :-- | :-- | :-- |
| `Scale` | relative to `Resolution` | `1.0` |
| `Size` | `int2(w, h)`, a fixed size instead of `Scale` | — |
| `Resolution` | `Render` (before the upscaler) or `Output` (after it) | the resolution of the first pass that writes it |
| `Clear` | a scalar, `float4(...)`, or `None` — with `None`, a read before every write is an error | `0` |
| `Mips` | integer | `1` |
| `History` | `true`: kept from one frame to the next, per view; `<Name>.Previous` reads last frame's | `false` |
| `Export` | `true`: copied, after the last pass that writes it, into a render target asset ordinary materials, Blueprints, UMG and Niagara read — see [Exported buffers](../runtime/index.md#exported-buffers) | `false` |

Built-in names need no declaration: `SceneColor`, `SceneDepth`, `CustomDepth`, `CustomStencil`,
`GBufferA`…`GBufferF`, `Velocity`, `Translucency`. Where each can be read and written is in
[Custom Pass runtime](../runtime/index.md#built-in-buffers).

## Passes

```hlsl
/// @desc optional description
pass <Name> : <kind>
{
    <Key> = <Value>;
    read  [<name in the pass> =] <buffer>[.Previous];
    write [<name in the pass> =] <buffer>;
    param <name in the pass> = <expression>;
}
```

Keys start with a capital letter, bindings with `read`, `write` or `param`. `read Mask;` is
`read Mask = Mask;`.

| Kind | Keys | `read X = B` | `write Y = B` | `param P = E` |
| :-- | :-- | :-- | :-- | :-- |
| `fullscreen` with `Material` | `Material` | the material's `UE.UserSceneTexture` named X | exactly one: `write B;` | the material parameter P |
| `fullscreen` with `Shader` | `Shader`, `Entry` | the HLSL name X | up to four, SV_Target0..3 in order | the HLSL name P |
| `compute` | `Shader`, `Entry`, `Threads`, `Dispatch` | the HLSL name X | up to four UAVs | the HLSL name P |
| `mesh` | `Filter`, `Material`, `Mode`, `Depth`, `Cull`, `Blend`, `Usage`, `Nanite`, `NaniteValue` | — | `Output0`…`Output3`: the outputs of `UE.DreamPassOutput` | the override material's parameter P |
| `clear` | `Value` | — | one | — |
| `copy` | — | one, the source | one, the target | — |

Every kind also takes `Injection` and `Enabled` (a `bool` uniform; the pass is off where it is false).

`E` in a `param` is a uniform, a `static const`, a literal, `DreamPassWeight` (the pipeline's weight in
the view), or one of those times and plus constants — `param Radius = OutlineWidth * 2 + 1;`. Anything
else is refused: the value is computed on the CPU once per view.

### `fullscreen`

`Material = "<name or object path>"` runs a Post Process material through the engine's own post-process
pass, so it sees exactly what a post-process volume's material would. Its buffers arrive as
`UE.UserSceneTexture` inputs, placed as the engine places them: one slot per name, in the material's
order, skipping the slots its `SceneTexture` nodes read (`PostProcessInput0`…`4`). A material that does
not read `PostProcessInput0` itself can bind five buffers, otherwise four. A material that writes a data
buffer rather than the scene colour must set `bDisablePreExposureScale = true` in its
`#pragma material`, or exposure scales what it reads and writes.

`Shader = "<file>.usf"` with `Entry = <function>` runs HLSL in a pixel shader slot instead. See
[HLSL passes](../runtime/hlsl.md).

### `compute`

`Shader` and `Entry` name a function with `[numthreads(x, y, z)]` in front of it; `Threads = uint3(...)`
is needed only when the group size is a macro. `Dispatch` is `<buffer>` (one thread per texel),
`<buffer> * <scale>` / `<buffer> / <n>`, or `uint3(x, y, z)`. See [HLSL passes](../runtime/hlsl.md).

### `mesh`

Draws the selected primitives again into up to four buffers.

| Key | Values | |
| :-- | :-- | :-- |
| `Filter` | `Stencil(v)`, `Stencil(v, mask)`, `Layer(A \| B)`, `List(Name)`, combined with `\|` and `&` (`&` binds tighter) | which primitives. Layers are the project's (Project Settings ▸ DreamShader Custom Pass), given to primitives by `UDreamPassLayerComponent`; lists are filled by `UDreamPassSubsystem::AddToList` |
| `Material` | a Surface material with `UE.DreamPassOutput` | the override material |
| `Mode` | `Override` (default with a material), `Own` (default without), `OwnOrOverride` | whose material draws: the override, the primitive's own (which needs `UE.DreamPassOutput`), or its own where it can |
| `Depth` | `TestScene` (default), `None`, `Own(<Depth32 buffer>)` | against the scene's depth, not at all, or among these primitives only |
| `Cull` | `Auto` (default), `Back`, `Front`, `None` | `Auto` follows the primitive's own material and the primitive, also when an override draws |
| `Blend` | `Replace` (default), `Add`, `Max`, `Min`, `AlphaBlend` | |
| `Usage` | `StaticMesh`, `InstancedStaticMeshes`, `SkeletalMesh`, `Landscape`, `SplineMesh`, `GeometryCache` | what the override material needs usage flags for; checked when the pipeline compiles. Default: the first three |
| `Nanite` | `Skip` (default for layers and lists), `StencilMask` (default for stencil filters), `AssignStencil(<value>)` | see below |
| `NaniteValue` | `float4(...)` | what `StencilMask` writes |

> [!IMPORTANT]
> **A Nanite primitive cannot be drawn again by a mesh pass**: the engine gives Nanite geometry no mesh
> batches a pass could draw, and its own Nanite rasterizer is not public. `Skip` leaves such primitives
> out (one warning per pass). `StencilMask` writes `NaniteValue` wherever CustomStencil matches the pass's
> `Stencil(...)` filter — the engine's custom depth pass does rasterize Nanite, with `r.CustomDepth=3` —
> so a mask still covers them, without their material's output. `AssignStencil(v)` makes that work for
> layers and lists too, by giving their Nanite members custom depth and stencil `v`; it collides with any
> stencil your project uses itself, which is why it is off by default.

When `Mode = Override` meets a primitive whose own material is masked or moves its vertices, the
primitive's own material draws it if that material has `UE.DreamPassOutput` — otherwise the override
draws it as if opaque, and a warning says so once. That is the rule the engine's custom depth pass uses.

### `clear` and `copy`

`clear` clears its buffer to `Value` (a depth for `Depth32`). Buffers are cleared when first used anyway;
`clear` is for clearing one again later in the frame. `copy` copies when format and size agree and draws
scaled and converted otherwise — `copy` of `SceneColor` into a half-size buffer is a grab.

## References

A `Material` is, as a `.dsi`'s `Parent` is, either a **bare name** — the material of a `.dss`, or the
instance of a `.dsi`, under the same source root, compiled first when it is missing or older than its
source — or an **object path** to any material. A `Shader` is a path relative to the `.dsp`'s folder —
next to the `.dsp` is the usual place — or a virtual path such as `/Project/Passes/Blur.usf`. The file needs no mapped shader directory: what the engine
compiles is a snapshot of it ([HLSL passes](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)).

## What the compiler checks

Everything that can be known before the frame is checked when the pipeline compiles: that every name is
declared or built in; that a kind may run at its injection point and a built-in buffer may be read or
written there; that no buffer is read before every write in frame order (`Clear = None`); that no pass
reads and writes one buffer (`B.Previous` and `B` are two); that a fullscreen material reads the names
its `read`s bind, and no more than it has slots for; that a mesh pass's material has `UE.DreamPassOutput`
with the outputs it writes connected, and the usage flags it needs; that an HLSL file exists, its `Entry`
does, and its bindings fit the slot (8 inputs, 4 outputs, 64 scalars of parameters); that a pass writing
the scene colour runs at the scene colour's resolution; that an exported buffer is not a depth or integer
buffer. The codes are `DSH2300`–`DSH2349`, `DSH3300`–`DSH3349`, `DSH4400`–`DSH4449`, `DSH7300`–`DSH7379`,
`DSH8300`–`DSH8339` — see the [diagnostics index](../diagnostics/index.md).

## Tools

- `dsc compile CP_X.dsp`, `dsc check`, `dsc check -Shaders` (which also pre-checks every HLSL pass for
  the project's shader formats), `dsc dump-ir`, `dsc index` and `dsc fmt` take a `.dsp`.
- `dsc decompile /Game/.../CP_X` writes `Decompiled/Pipelines/Game/.../CP_X.dsp` (the folder follows the
  asset's package path), then parses and binds what it wrote and compares it with the asset.
- **Adopt Into Source** on a pipeline edited in the details panel rewrites the `.dsp` in place, value by
  value; comments and order survive.
- `dsc pass-registry` lists the HLSL slots and which pass has each; `-Gc` frees the slots of passes that no
  longer exist.
- The material browser lists `.dsp` files, and **New Source** offers three pipeline templates.

## See also

- [Custom Pass runtime](../runtime/index.md) — activation, injection points, buffers, views, debugging, limits
- [HLSL passes](../runtime/hlsl.md) — `DreamPass.ush`, slots, snapshots and the pre-check
- [Custom Pass material nodes](../builtins/dream-pass.md) — `UE.DreamPassOutput`, `UE.DreamPassBuffer`
- [Custom Pass examples](../examples/custom-pass.md)
- [Material instances](instances.md) — `.dsi`, the other non-graph source kind
