# HLSL passes

> [DreamShader](../index.md) » [Custom Pass runtime](index.md) » **HLSL passes**

A `compute` pass, or a `fullscreen` pass without a `Material`, runs HLSL. The code is written in the `.dsp`
itself — in an `hlsl { }` block of the pass, or as an entry of the file's `hlsl { }` block — or in a `.usf`
the pass names with `Shader =`, anywhere in the project. This page is what that HLSL can use and how it gets
into the engine.

| | |
| :-- | :-- |
| Header | `/Plugin/DreamShader/Pass/DreamPass.ush` — included for you; including it again is harmless |
| Shader types | `FDreamPassCS` (compute) and `FDreamPassPS` (pixel), one permutation per **slot** |
| Slots | 32 compute, 16 pixel (`DREAMSHADER_PASS_COMPUTE_SLOTS`, `DREAMSHADER_PASS_PIXEL_SLOTS`, overridable from a Target.cs with `GlobalDefinitions`) |
| Snapshots | `<DShader>/.dreampass/` — **commit it** with your sources |

## Where the code is

Each HLSL pass has its code in exactly one place:

| Where | How the pass says it | `Entry` |
| :-- | :-- | :-- |
| its own block | `hlsl { ... }` in the pass | whole functions: the function that runs, `Main` when not written; the statements of the entry: none |
| the file's block | `Entry = <function>;` alone | required: a function of the file's `hlsl { }` block |
| a shader file | `Shader = "<file>.usf";` | required: a function in the file |

A block holds either **whole functions** — what a `.usf` holds: functions, `groupshared`, `struct`, `#define`,
`#include` — or only **the statements of the entry**, whose function the compiler writes. Which one it is is
read off the block's top level: a function definition, a `groupshared`, `struct` or `cbuffer` declaration, or
a `[numthreads(...)]` makes it whole functions; anything else — statements, `if`, `for`, `[unroll]` — makes it
the statements of the entry. Moving a `.usf` into the `.dsp` is pasting its text into a block.

### Whole functions

```hlsl
pass TileLuma : compute
{
    Injection = BeforePostProcess;
    Dispatch  = TileAverage * 16;
    read  InColor = SceneColor;
    write Result  = TileAverage;

    hlsl
    {
        groupshared float Partial[256];

        [numthreads(16, 16, 1)]
        void Main(uint3 Id : SV_DispatchThreadID, uint3 Group : SV_GroupID, uint Index : SV_GroupIndex)
        {
            const float2 UV = (Id.xy + 0.5) / (ResultSize.xy * 16.0);
            Partial[Index] = dot(DreamPassSample(InColor, UV).rgb, float3(0.2126, 0.7152, 0.0722));
            GroupMemoryBarrierWithGroupSync();
            for (uint Stride = 128; Stride > 0; Stride >>= 1)
            {
                if (Index < Stride)
                {
                    Partial[Index] += Partial[Index + Stride];
                }
                GroupMemoryBarrierWithGroupSync();
            }
            if (Index == 0)
            {
                Result[Group.xy] = Partial[0] / 256.0;
            }
        }
    }
}
```

The entry of a compute pass has `[numthreads(x, y, z)]` in front of it, and that is the group size the
dispatch is counted in; `Threads = uint3(...)` is needed only when it is spelled with macros.

### The statements of the entry

```hlsl
pass Pulse : fullscreen
{
    Injection = BeforePostProcess;
    read  InColor = SceneColor;
    write Out     = SceneColor;
    param PulseTint = PulseColor;

    hlsl
    {
        const float3 Scene = DreamPassSample(InColor, UV).rgb;
        const float  Depth = CalcSceneDepth(DreamPassSceneUV(UV));
        Out = float4(Scene + PulseTint.rgb * saturate(1.0 - Depth / 4000.0) * DP_Weight, 1.0);
    }
}
```

The compiler writes the function around the statements, with these names in it:

| Pass | Function | Names the statements see |
| :-- | :-- | :-- |
| `compute` | `[numthreads(...)]` from `Threads` (`uint3(8, 8, 1)` when not written); a thread past `DP_DispatchSize` returns before your statements | `Id` (`SV_DispatchThreadID`), `GroupId` (`SV_GroupID`), `LocalId` (`SV_GroupThreadID`), `LocalIndex` (`SV_GroupIndex`) |
| `fullscreen` | one `out float4` per `write`, named after it and cleared to 0, `SV_Target0`..`3` in `write` order | `SvPosition` (`SV_POSITION`), `Pixel` (the pixel's centre counted from the view's top left), `UV` (its place in the view, 0..1), and the writes |

`return;` ends the statements early (a pixel pass's outputs are already cleared). A binding cannot take one of
these names, and the statements cannot hold a function, a `groupshared` or an `#include` — those go in the
file's block, or the block holds whole functions instead.

### The file's `hlsl` block

```hlsl
hlsl
{
    // Shared code: compiled into every pass whose code is in the .dsp.
    float GaussianWeight(int Offset, float Spread)
    {
        return exp(-float(Offset * Offset) / (2.0 * Spread * Spread));
    }

    // An entry: compiled only for the passes that name it.
    [numthreads(8, 8, 1)]
    void BlurCS(uint3 Id : SV_DispatchThreadID)
    {
        if (any(Id.xy >= DP_DispatchSize.xy))
        {
            return;
        }
        float4 Sum = 0;
        float Total = 0;
        for (int i = -12; i <= 12; ++i)
        {
            const float W = GaussianWeight(i, SigmaTexels);
            Sum   += W * Source.Load(int3(clamp(int2(Id.xy) + int2(Axis) * i, int2(0, 0), int2(SourceSize.xy) - 1), 0));
            Total += W;
        }
        Result[Id.xy] = Sum / Total;
    }
}

pass BlurH : compute
{
    Entry = BlurCS;
    read  Source = Bright;
    write Result = BlurX;
    param SigmaTexels = Sigma;
    param Axis        = float2(1, 0);
}
```

A `.dsp` has one. What a pass names with `Entry` is an **entry**; everything else in the block is **shared
code**, compiled before the code of every pass whose code is in the `.dsp`. Two rules follow from how the
names of a pass are made (below):

- **Shared code uses no pass's names** — no `read`, `write` or `param` name, none of their `Size` and
  `UVRect` names, not even as a parameter or a local variable: the names are `#define`s of the slot it is
  compiled into. An entry may use them: it is compiled only for the passes that name it, and each of those
  binds what it uses.
- **Nothing calls an entry.** In the slot of any other pass it is not there. Move what two entries share into
  a function of its own.

Both are checked when the pipeline compiles. A pass's own block sees the shared code too, and calls its
functions freely.

## Names a pass makes

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

The names `Source`, `Result` and `Radius` exist because the pass binds them: before its code, each binding
becomes a `#define` onto the slot's fixed parameters, in the order the bindings are written.

| Binding | Defines | Meaning |
| :-- | :-- | :-- |
| `read X = …` (the i-th) | `X`, `XSize`, `XUVRect` | `DP_Input<i>` (a `Texture2D<float4>`), its size (xy, and one over it in zw), the UV rect of its picture inside the texture |
| `write Y = …` (the j-th) | `Y`, `YSize` | compute: `DP_Output<j>` (a `RWTexture2D<float4>`); pixel: only the size — write `SV_Target<j>` |
| `param P = …` | `P` | a component of `DP_Params[]`: a `float` is one, a `float2` an aligned half, a `float3` the xyz of a fresh vector, a `float4` a whole one; `int` and `bool` read back with `asint` / `asuint` |

Because they are macros, pick binding names that cannot collide with engine identifiers (`InMask`, not
`Texture`). The `Entry` is renamed to the slot's fixed entry point the same way, so a `[numthreads]` in front
of it is what the dispatch uses.

## The fixed parameters

| Name | |
| :-- | :-- |
| `DP_Input0`…`DP_Input7` | the `read` textures; an unbound one is black |
| `DP_InputSize[8]`, `DP_InputUVRect[8]` | size (xy) and inverse size (zw); the picture's UV rect (min xy, max zw) — (0, 0, 1, 1) for a buffer of the pipeline, a sub-rect for the scene colour in split screen |
| `DreamPassInputUV(i, uv)` | `uv` over input i's picture, as a UV in its texture |
| `DreamPassSample(Name, UV)` | the `read` Name sampled (linear) at a UV over the view, inside its picture |
| `DreamPassLoad(Name, Pixel)` | a texel of the `read` Name, at a pixel counted from the view's top left |
| `DreamPassSceneUV(UV)` | a UV over the view as a UV of the scene textures, for `CalcSceneDepth` and the rest; not at `BeginView` |
| `DP_PointClamp`, `DP_LinearClamp` | samplers |
| `DP_Output0`…`DP_Output3` | compute only: the `write` UAVs |
| `DP_OutputSize[4]` | the outputs' sizes |
| `DP_Params[16]` | the packed `param` values |
| `DP_Weight` | the pipeline's weight in the view |
| `DP_ViewRect` | output 0's rect for this view (min xy, max zw). A buffer of the pipeline starts at 0; the scene colour of one view in split screen does not — write `Out[DP_ViewRect.xy + Id.xy]` |
| `DP_DispatchSize` | compute: the threads asked for; a dispatch rounds up to whole groups, so return early past it |
| `DP_Time` | world time, world delta time, real time (seconds), frame number |
| `View`, `SceneTexturesStruct` | the engine's view uniform buffer and scene textures — `CalcSceneDepth`, `CalcSceneCustomStencil` and the rest of `SceneTexturesCommon.ush` work |

`DreamPassSample`, `DreamPassLoad` and `DreamPassSceneUV` are macros — `DreamPassSample(InColor, UV)` reads
`InColorUVRect`, the name the binding made — and work in a `.usf` as well as in a block.

At `BeginView` the view uniform buffer does not exist yet; using `View` in a pass there is a compile error,
and `DP_Time` stands in for the timing values. The scene textures there are placeholders. The shared code of
the file's block may use `View` — other passes may run later in the frame — but a `BeginView` pass that calls
such a function is refused when its slot is pre-checked (`DSH8318`).

## How the HLSL gets into the engine

A pass cannot have a global shader type of its own: shader types register when the plugin loads. Every HLSL
pass is given a **slot** — a permutation of `FDreamPassCS` or `FDreamPassPS` — when its pipeline compiles:

1. **A pre-check** compiles the slot with your code for the project's shader formats. An error is a
   DreamShader diagnostic with the file and line — for code in the `.dsp`, the line of the `.dsp` — and the
   pipeline does not compile. An error in the shared code of the file's block is said once, with the passes it
   failed. Code no entry reaches is not compiled at all — the engine drops it before the shader compiler reads
   the slot — so a helper no pass calls yet is not checked, as in a `.usf`.
2. **A snapshot** of the code — the `.usf` and every file it includes by a relative path, or, for code in the
   `.dsp`, the file's block with the other passes' entries blanked out, the pass's own code after it, and every
   file they include by a path relative to the `.dsp` — is written into `<DShader>/.dreampass/Slots/<C|P><NN>/`,
   and the registry (`RegistryCompute.ush`, `RegistryPixel.ush`) maps the slot onto it. `Registry.json` records
   which pass has which slot.
3. **The two shader types are recompiled** in the editor, and the pass runs on the next frame.

The global shaders are built from the snapshots, never from the file you are editing. That matters because a
compile error in a global shader is fatal: an editor or a cook that compiled your half-typed code directly
would stop. With the snapshots, a teammate who pulls your change gets what passed your pre-check, and so does
the cook. A comment added to the `.dsp` above a block moves no line of a snapshot: the snapshot holds no
line numbers of the `.dsp`, so it recompiles no global shader.

- Slots are stable: a pass keeps its slot across compiles; a new pass takes the lowest free one; a removed
  pass frees its slot when its pipeline compiles again; `dsc pass-registry -Gc` frees the slots of
  pipelines that no longer exist. Running out of slots is a compile error naming the counts to raise.
- An unused slot compiles to an empty entry, so the permutation list is the same in every build — which a
  cooked build requires.
- The global shaders are cooked into the base game's global shader cache: **an HLSL pass cannot ship in DLC.**
- A block ends at the `}` that matches its `{`; braces in comments, strings and `#` lines do not count. A
  multi-line macro (`\` at the end of a line) that holds a brace is not supported in a block.

## See also

- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md)
- [Custom Pass runtime](index.md)
- [Custom Pass examples](../examples/custom-pass.md)
