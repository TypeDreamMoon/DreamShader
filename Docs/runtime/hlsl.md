# HLSL passes

> [DreamShader](../index.md) » [Custom Pass runtime](index.md) » **HLSL passes**

A `compute` pass, or a `fullscreen` pass with `Shader =`, runs HLSL from a `.usf` in your project — anywhere
in it; next to the `.dsp` is the usual place. This page is what that HLSL can use and how it gets into the
engine.

| | |
| :-- | :-- |
| Header | `/Plugin/DreamShader/Pass/DreamPass.ush` — included for you; including it again is harmless |
| Shader types | `FDreamPassCS` (compute) and `FDreamPassPS` (pixel), one permutation per **slot** |
| Slots | 32 compute, 16 pixel (`DREAMSHADER_PASS_COMPUTE_SLOTS`, `DREAMSHADER_PASS_PIXEL_SLOTS`, overridable from a Target.cs with `GlobalDefinitions`) |
| Snapshots | `<DShader>/.dreampass/` — **commit it** with your sources |

## Writing one

```hlsl
// DShader/Passes/BoxBlur.usf, next to the pipeline that runs it
[numthreads(8, 8, 1)]
void BlurCS(uint3 Id : SV_DispatchThreadID)
{
    if (any(Id.xy >= DP_DispatchSize.xy))
    {
        return;
    }
    const float2 UV = (Id.xy + 0.5) * ResultSize.zw;
    const int R = (int)Radius;
    float Sum = 0;
    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            Sum += Source.SampleLevel(DP_LinearClamp, UV + float2(x, y) * SourceSize.zw, 0).r;
        }
    }
    Result[DP_ViewRect.xy + Id.xy] = Sum / ((2 * R + 1) * (2 * R + 1));
}
```

```hlsl
pass Blur : compute
{
    Shader = "BoxBlur.usf";
    Entry  = BlurCS;
    read   Source = Mask;
    write  Result = Blurred;
    param  Radius = BlurRadius;
}
```

The names `Source`, `Result` and `Radius` exist because the pass binds them: before your file is included,
each binding becomes a `#define` onto the slot's fixed parameters, in the order the bindings are written.

| Binding | Defines | Meaning |
| :-- | :-- | :-- |
| `read X = …` (the i-th) | `X`, `XSize`, `XUVRect` | `DP_Input<i>` (a `Texture2D<float4>`), its size (xy, and one over it in zw), the UV rect of its picture inside the texture |
| `write Y = …` (the j-th) | `Y`, `YSize` | compute: `DP_Output<j>` (a `RWTexture2D<float4>`); pixel: only the size — write `SV_Target<j>` |
| `param P = …` | `P` | a component of `DP_Params[]`: a `float` is one, a `float2` an aligned half, a `float3` the xyz of a fresh vector, a `float4` a whole one; `int` and `bool` read back with `asint` / `asuint` |

Because they are macros, pick binding names that cannot collide with engine identifiers (`InMask`, not
`Texture`). Your `Entry` is renamed to the slot's fixed entry point the same way, so a `[numthreads]` in front
of it is what the dispatch uses.

## The fixed parameters

| Name | |
| :-- | :-- |
| `DP_Input0`…`DP_Input7` | the `read` textures; an unbound one is black |
| `DP_InputSize[8]`, `DP_InputUVRect[8]` | size (xy) and inverse size (zw); the picture's UV rect (min xy, max zw) — (0, 0, 1, 1) for a buffer of the pipeline, a sub-rect for the scene colour in split screen |
| `DreamPassInputUV(i, uv)` | `uv` over input i's picture, as a UV in its texture |
| `DP_PointClamp`, `DP_LinearClamp` | samplers |
| `DP_Output0`…`DP_Output3` | compute only: the `write` UAVs |
| `DP_OutputSize[4]` | the outputs' sizes |
| `DP_Params[16]` | the packed `param` values |
| `DP_Weight` | the pipeline's weight in the view |
| `DP_ViewRect` | output 0's rect for this view (min xy, max zw). A buffer of the pipeline starts at 0; the scene colour of one view in split screen does not — write `Out[DP_ViewRect.xy + Id.xy]` |
| `DP_DispatchSize` | compute: the threads asked for; a dispatch rounds up to whole groups, so return early past it |
| `DP_Time` | world time, world delta time, real time (seconds), frame number |
| `View`, `SceneTexturesStruct` | the engine's view uniform buffer and scene textures — `CalcSceneDepth`, `CalcSceneCustomStencil` and the rest of `SceneTexturesCommon.ush` work |

At `BeginView` the view uniform buffer does not exist yet; using `View` in a pass there is a compile error,
and `DP_Time` stands in for the timing values. The scene textures there are placeholders.

## How the HLSL gets into the engine

A pass cannot have a global shader type of its own: shader types register when the plugin loads. Every HLSL
pass is given a **slot** — a permutation of `FDreamPassCS` or `FDreamPassPS` — when its pipeline compiles:

1. **A pre-check** compiles the slot with your file for the project's shader formats. An error is a
   DreamShader diagnostic with the file and line, and the pipeline does not compile.
2. **A snapshot** of the file — and every file it includes by a relative path — is copied into
   `<DShader>/.dreampass/Slots/<C|P><NN>/`, and the registry (`RegistryCompute.ush`, `RegistryPixel.ush`)
   maps the slot onto it. `Registry.json` records which pass has which slot.
3. **The two shader types are recompiled** in the editor, and the pass runs on the next frame.

The global shaders are built from the snapshots, never from the file you are editing. That matters because a
compile error in a global shader is fatal: an editor or a cook that compiled your half-typed `.usf` directly
would stop. With the snapshots, a teammate who pulls your change gets what passed your pre-check, and so does
the cook.

- Slots are stable: a pass keeps its slot across compiles; a new pass takes the lowest free one; a removed
  pass frees its slot when its pipeline compiles again; `dsc pass-registry -Gc` frees the slots of
  pipelines that no longer exist. Running out of slots is a compile error naming the counts to raise.
- An unused slot compiles to an empty entry, so the permutation list is the same in every build — which a
  cooked build requires.
- The global shaders are cooked into the base game's global shader cache: **an HLSL pass cannot ship in DLC.**

## See also

- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md)
- [Custom Pass runtime](index.md)
