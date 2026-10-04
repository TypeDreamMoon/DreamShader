# Custom Pass pipelines — the subset an author needs

Condensed from [`Docs/language-v2/passes.md`](../../Docs/language-v2/passes.md),
[`Docs/runtime/hlsl.md`](../../Docs/runtime/hlsl.md) and
[`Docs/builtins/dream-pass.md`](../../Docs/builtins/dream-pass.md). When this page and `Docs/` disagree,
`Docs/` wins.

A `.dsp` adds render passes to Unreal without touching the engine: a mask of selected objects, a blur, a
screen-space effect, a compute shader writing a data texture. It is **not** a material — the passes it runs
use materials, which are ordinary [`.dss`](dss.md) files.

| | |
| :-- | :-- |
| Builds | one `UDreamPassPipeline`, named after the file, where a `.dss` in that folder puts its product; plus a `UTextureRenderTarget2D` per buffer with `Export = true` |
| Engine | **UE 5.8 and later.** Below that a `.dsp` parses, formats and binds, and compiling it is `DSH8300` |
| Runs when | activated — Project Settings, an `ADreamPassVolume`, a `UDreamPassComponent`, Blueprint / C++. A compiled pipeline nobody activates draws nothing |
| Writes besides the asset | `<DShader>/.dreampass/` — the HLSL slot registry and snapshots. **Source files: commit them** |

---

## 1. The shape of a file

```hlsl
// DShader/Passes/CP_Highlight.dsp
#pragma pipeline(Order = 100)

/// @group Look
uniform float4 OutlineColor = float4(1.0, 0.6, 0.0, 1.0);
/// @group Look   @slider 1 8
uniform float  OutlineWidth = 3.0;

buffer Mask    : R8(Clear = 0);
buffer Blurred : R8(Scale = 0.5, Export = true);

pass DrawMask : mesh                       // the objects on the Highlight layer, into Mask
{
    Injection = AfterOpaque;
    Filter    = Layer(Highlight);
    Material  = "M_HighlightMask";         // a .dss with UE.DreamPassOutput
    Depth     = None;
    write Output0 = Mask;
}

pass Blur : compute                        // HLSL, written in the pass
{
    Injection = AfterOpaque;
    Dispatch  = Blurred;
    read  Source = Mask;
    write Result = Blurred;
    param Radius = OutlineWidth;

    hlsl
    {
        const float2 UV = (Id.xy + 0.5) * ResultSize.zw;
        const int R = (int)Radius;
        float Sum = 0;
        for (int y = -R; y <= R; ++y)
            for (int x = -R; x <= R; ++x)
                Sum += Source.SampleLevel(DP_LinearClamp, UV + float2(x, y) * SourceSize.zw, 0).r;
        Result[DP_ViewRect.xy + Id.xy] = Sum / ((2 * R + 1) * (2 * R + 1));
    }
}

pass Composite : fullscreen                // a Post Process material
{
    Injection = BeforePostProcess;
    Material  = "PP_OutlineComposite";
    read  Mask;
    read  Blurred;
    write SceneColor;
    param Color = OutlineColor;
}
```

| Declaration | |
| :-- | :-- |
| `#pragma pipeline(...)` | at most one. `Order` (int, default `0`), `Injection` (the default for passes naming none, `BeforePostProcess`), `Views` (`Game \| Editor` by default), `Requires` (`PostProcess`, `SceneResolve`, `CustomStencil`), `Enabled` |
| `uniform` | what an activation can override: `float`–`float4`, `int`, `bool`, `Texture2D` (with `/// @default <asset>`); doc tags `@group` `@desc` `@slider` `@sort` |
| `static const` | a constant |
| `buffer Name : Format(Key = Value, ...);` | formats `R8 RG8 RGBA8 R16F RG16F RGBA16F R32F RG32F RGBA32F Depth32`; keys `Scale`, `Size = int2(w, h)`, `Resolution = Render \| Output`, `Clear` (`0`, a `float4`, or `None`), `Mips`, `History`, `Export` |
| `hlsl { … }` | at most one per file: shared HLSL functions, and entries a pass names with `Entry =` |
| `pass Name : kind { … }` | passes run in declaration order within an injection point |

Built-in buffers need no declaration: `SceneColor`, `SceneDepth`, `CustomDepth`, `CustomStencil`,
`GBufferA`…`GBufferF`, `Velocity`, `Translucency`. `R32U` / `RG32U` are reserved (`DSH7321`) — an id fits
`R32F` exactly up to 16777216.

## 2. Passes

| Kind | Code | `read X = B` | `write Y = B` | `param P = E` |
| :-- | :-- | :-- | :-- | :-- |
| `fullscreen` + `Material` | a Post Process material | the material's `UE.UserSceneTexture` named `X` | exactly one | the material parameter `P` |
| `fullscreen`, no material | `hlsl { }`, `Entry =`, or `Shader = "x.usf"` + `Entry` | the HLSL name `X` | up to four, `SV_Target0..3` | the HLSL name `P` |
| `compute` | the same three, plus `Threads`, `Dispatch` | the HLSL name `X` | up to four UAVs | the HLSL name `P` |
| `mesh` | `Filter`, `Material`, `Mode`, `Depth`, `Cull`, `Blend`, `Usage`, `Nanite` | — | `Output0`…`Output3` of `UE.DreamPassOutput` | the override material's `P` |
| `clear` | `Value` | — | one | — |
| `copy` | — | one | one | — |

Every pass takes `Injection` and `Enabled`. `read Mask;` is `read Mask = Mask;`. A `param` value is computed
on the CPU once per view: a uniform, a `static const`, a literal, `DreamPassWeight`, or those times and plus
constants — nothing else.

**Injection points**, in frame order: `BeginView` (nothing drawn, **no `View` uniform buffer**) ·
`BeforeBasePass` (SceneDepth) · `AfterBasePass` (GBuffer, unlit) · `AfterOpaque` (opaque lit, CustomDepth /
Stencil) · `BeforePostProcess` (the default) · `PostProcess.BeforeDOF` / `AfterDOF` /
`TranslucencyAfterDOF` / `ReplaceTonemapper` / `AfterMotionBlur` / `AfterTonemap` / `AfterFXAA` ·
`EndOfView` (LDR, before the UI). The full table of what each point can read is in
[`Docs/language-v2/passes.md`](../../Docs/language-v2/passes.md#injection-points).

`mesh` `Filter`: `Stencil(v)`, `Stencil(v, mask)`, `Layer(A | B)` (the project's pass layers, given to actors by
`UDreamPassLayerComponent`), `List(Name)`, combined with `|` and `&`.

## 3. HLSL passes

Each HLSL pass has its code in **one** place: an `hlsl { }` block in the pass, an `Entry =` naming a function of
the file's `hlsl` block, or a `.usf` (`Shader =` + `Entry =`). A pass's own block is either **whole functions**
(what a `.usf` holds — `[numthreads]`, `groupshared`, `struct`, `#include`; the entry is `Main` unless
`Entry` says otherwise) or **only the statements of the entry**, whose function the compiler writes.

| Statements-only block | The compiler writes | Names the statements see |
| :-- | :-- | :-- |
| `compute` | `[numthreads]` from `Threads` (default `uint3(8, 8, 1)`); threads past `DP_DispatchSize` return first | `Id`, `GroupId`, `LocalId`, `LocalIndex` |
| `fullscreen` | one `out float4` per `write`, named after it, cleared to 0 | `SvPosition`, `Pixel`, `UV` (0..1 over the view) |

Each binding becomes a `#define` onto the slot's fixed parameters:

| Binding | Names it makes |
| :-- | :-- |
| `read X = …` | `X` (a `Texture2D<float4>`), `XSize` (xy, and 1/size in zw), `XUVRect` |
| `write Y = …` | compute: `Y` (a `RWTexture2D<float4>`) and `YSize`; fullscreen: the output `Y` |
| `param P = …` | `P` |

Helpers, from `DreamPass.ush` (included for you): `DreamPassSample(Name, UV)`, `DreamPassLoad(Name, Pixel)`,
`DreamPassSceneUV(UV)` (for `CalcSceneDepth` and friends), `DP_PointClamp`, `DP_LinearClamp`, `DP_Weight`,
`DP_ViewRect`, `DP_DispatchSize`, `DP_Time` (world time, delta, real time, frame), `View`,
`SceneTexturesStruct`. A slot has **8 inputs, 4 outputs and 64 scalars** of parameters.

## 4. The materials a pipeline uses

| Used by | Write it as | Key points |
| :-- | :-- | :-- |
| a `fullscreen` pass | `#pragma material(Domain = PostProcess, BlendableLocation = SceneColorAfterDOF)` | reads its buffers with `UE.UserSceneTexture(UserSceneTexture = "Mask", Coordinates = UV)`; the scene with `UE.SceneTexture(SceneTextureId = PostProcessInput0, …)`. Writing a **data** buffer rather than the scene colour needs `bDisablePreExposureScale = true` |
| a `mesh` pass | a Surface material calling `UE.DreamPassOutput(Output0 = …)` | a statement, one per material, **top level only** (inside a material function it does nothing); add the usage flags the pass needs (`bUsedWithSkeletalMesh`, `bUsedWithInstancedStaticMeshes`) |
| any material, reading an exported buffer | `UE.DreamPassBuffer(Pipeline = "CP_X", Buffer = "Name").r` | the buffer must have `Export = true`; an opaque or translucent material sees the **previous frame** when the last writer runs after the base pass (`DSH7354`, info) |

A `Material` key, like a `.dsi`'s `Parent`, is a bare name — the product of a `.dss` / `.dsi` under the same
source root, compiled first when it is missing or stale — or an object path.

## 5. Checking it

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check -Shaders DShader/Passes/CP_Highlight.dsp
```

`check` binds the pipeline against its materials and HLSL files and writes no asset; **`-Shaders` also
pre-checks every HLSL pass in its slot**, for every shader format the project builds, and reports HLSL errors
at the line of the `.dsp`. Then `compile` builds the pipeline. The pass materials are separate `.dss` files —
check those too.

## 6. Traps

- **Code no entry reaches is not compiled at all.** The engine drops it before the shader compiler reads the
  slot, so a broken helper that nothing calls yet passes `check -Shaders` and fails the day a pass calls it.
- **One HLSL error can come back twice, worded differently.** The pre-check compiles each shader format the
  project targets (SM5 with FXC, SM6 with DXC by default); each compiler reports it its own way. It is one
  mistake.
- **Binding names are macros.** Pick names no engine identifier uses (`InMask`, not `Texture`). The file's
  shared `hlsl` code may not use any pass's binding names at all — not even as a local — and nothing may call
  an entry (`DSH7369`): put what two entries share in a function of its own.
- **`BeginView` has no `View` uniform buffer.** Using `View` there — also through a shared function a pass
  calls — is refused (`DSH8318`); use `DP_Time`, or move the pass later.
- **A pass may not read and write one buffer** (`DSH7333`). Write another buffer, or read `B.Previous` of a
  `History = true` buffer — that is also how a simulation keeps state across frames.
- **A read before the first write in frame order reads `Clear`** (`DSH7332`, warning); with `Clear = None` it
  is an error.
- **A fullscreen material has 4–5 buffer slots** (`DSH7336`): five when it does not read `PostProcessInput0`
  itself, four when it does.
- **Slots are finite**: 32 compute, 16 pixel per project. A pass keeps its slot; a deleted pipeline keeps its
  slots until `dsc.ps1 pass-registry -Gc`. A decompiled `.dsp` compiled for a look takes slots too.
- **An HLSL pass cannot ship in DLC** — its shaders are cooked into the base game's global shader cache.
- **A Nanite primitive cannot be drawn by a `mesh` pass.** `Nanite = Skip` (the default for layers and lists)
  leaves it out with a warning; `StencilMask` / `AssignStencil(v)` cover it as a stencil mask instead.

## See also

- [`Docs/language-v2/passes.md`](../../Docs/language-v2/passes.md) — every key and value
- [`Docs/runtime/index.md`](../../Docs/runtime/index.md) — activation, views, debugging (`DreamPass.Dump`), limits
- [`Docs/runtime/hlsl.md`](../../Docs/runtime/hlsl.md) — slots, snapshots and the pre-check
- [`Docs/examples/custom-pass.md`](../../Docs/examples/custom-pass.md) — seven complete pipelines with their `.dss`
