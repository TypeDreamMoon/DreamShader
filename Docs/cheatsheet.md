# DreamShaderLang cheat sheet

One page of the 2.0 language: the file kinds, the shape of each, and the mistakes that cost the most
time. Every complete source on this page is compiled by the test suite. For the rules behind a line, read
[the 2.0 language](language-v2/index.md); for a code, [the diagnostics](diagnostics/README.md) or the same
data as JSON, [`diagnostics/codes.json`](diagnostics/codes.json).

A `.dss` is **HLSL with declarations**: parameters are `uniform`s, metadata rides on `///` lines and
`#pragma` lines, and one `export` function is the asset. It is not HLSL: do not guess names from HLSL,
GLSL or the material editor.

## Files

| File | Holds | Builds |
| :-- | :-- | :-- |
| `.dss` | `uniform`s, helpers, one `export` entry or several `export` functions, `#pragma material` | a material, or material functions / layers / blends |
| `.dsi` | `#pragma instance(Parent = …)` and `uniform` overrides | a material instance |
| `.dsp` | `#pragma pipeline`, `buffer`s, `pass`es | a Custom Pass pipeline (UE 5.8+) |
| `.dsh` | shared helpers, pulled in with `#include "Path/File.dsh"` | nothing on its own |
| `.dsm` / `.dsf` | the 1.x language; still builds, write new sources as `.dss` | as 1.x did |

`DreamSource/Shader/Props/M_Crate.dss` with `export void M_Crate(…)` builds `/Game/Props/M_Crate`: the folder under
`DreamSource/Shader/`, the name of the `export`. `/// @name Other` renames the asset.

## A material

```hlsl
// DreamSource/Shader/CheatSheet/M_CheatSheetSurface.dss -- a lit, textured surface with an optional pulsing edge glow
#pragma material(Domain = Surface, BlendMode = Opaque, ShadingModel = DefaultLit)

/// @group Surface   @sort 10   @default /Engine/EngineResources/WhiteSquareTexture
uniform Texture2D Albedo;

/// @group Surface   @sort 20   @desc Multiplies the texture
uniform float4 Tint = float4(0.8, 0.75, 0.7, 1.0);

/// @group Surface   @sort 30   @slider 0 1
uniform float SurfaceRoughness = 0.6;

/// @static
/// @group Glow   @sort 40
uniform bool UseGlow = true;

/// @group Glow   @sort 50   @slider 0 10
uniform float GlowStrength = 2.0;

float EdgeMask(float2 UV, float Width)              // a helper: inlined at each call
{
    float2 D = min(UV, 1.0 - UV);
    return 1.0 - saturate(min(D.x, D.y) / Width);
}

export void M_CheatSheetSurface(inout material m)   // the entry: this signature makes a material
{
    float2 UV = UE.TexCoord(Index = 0);
    float4 Texel = Albedo.Sample(UV);
    m.BaseColor = Texel.rgb * Tint.rgb;
    m.Roughness = SurfaceRoughness;
    m.Metallic = 0.0;

    float3 Glow = float3(0, 0, 0);
    if (UseGlow)                                     // a static bool: a StaticSwitch, not a branch
    {
        float Pulse = 0.5 + 0.5 * sin(UE.Time() * 0.5);   // sin takes cycles: one pulse every 2 s
        Glow = Tint.rgb * EdgeMask(UV, 0.08) * Pulse * GlowStrength;
    }
    m.EmissiveColor = Glow;
}
```

- One entry per file, `export void <Name>(inout material m)`. Outputs are members of `m`: `BaseColor`,
  `Metallic`, `Roughness`, `Specular`, `EmissiveColor`, `Opacity`, `OpacityMask`, `Normal`,
  `WorldPositionOffset`, `AmbientOcclusion`, … — the engine's own pin names, case-sensitive.
- Name a uniform apart from the outputs (`SurfaceRoughness`, not `Roughness`); it reads better and
  searches better.

## A UI material

```hlsl
// DreamSource/Shader/CheatSheet/M_CheatSheetPanel.dss -- a translucent UI panel: a soft vertical gradient and a scrolling band
#pragma material(Domain = UI, BlendMode = Translucent, ShadingModel = Unlit)

/// @group Panel   @sort 10   @desc Base colour; alpha scales the opacity
uniform float4 Tint = float4(0.12, 0.16, 0.2, 0.85);

/// @group Panel   @sort 20   @slider 0 2   @desc Band travel, panels per second
uniform float ScrollSpeed = 0.25;

export void M_CheatSheetPanel(inout material m)
{
    float2 UV = UE.TexCoord(Index = 0);
    float Band = frac(UV.y - UE.Time() * ScrollSpeed);
    float Glow = pow(saturate(1.0 - abs(Band - 0.5) * 4.0), 3.0);
    m.EmissiveColor = Tint.rgb * lerp(1.15, 0.85, UV.y) + Glow * 0.08;
    m.Opacity = Tint.a;
}
```

A UI material writes `EmissiveColor` and `Opacity` (or `OpacityMask` with `BlendMode = Masked`); nothing
else reaches the screen.

## Parameters

```hlsl
uniform <Type> <Name> = <value>;          // a material parameter
static const <Type> <Name> = <value>;     // a constant, folded where it is used
```

| `///` tag above a uniform | Effect |
| :-- | :-- |
| `@group A\|B` · `@sort N` · `@desc text` | group, sort priority, tooltip |
| `@slider min max` | slider range, scalars only |
| `@static` | a `uniform bool` becomes a static switch |
| `@default /Game/T_X` · `@sampler Color` | a texture's default asset and sampler type |
| `@name Real Name` | the parameter's engine name (keeps overrides when you rename the identifier) |

Several tags share a line. Textures take no initializer: `@default` instead. No uniform arrays.

## `#pragma material`

```hlsl
#pragma material(Domain = PostProcess, BlendMode = Translucent, ShadingModel = Unlit, TwoSided = true)
```

Keys are `UMaterial` property names; enum values drop their prefix (`Translucent`, not `BLEND_Translucent`).

| Key | Common values |
| :-- | :-- |
| `Domain` | `Surface`, `DeferredDecal`, `LightFunction`, `Volume`, `PostProcess`, `UI` |
| `BlendMode` | `Opaque`, `Masked`, `Translucent`, `Additive`, `Modulate`, `AlphaComposite` |
| `ShadingModel` | `Unlit`, `DefaultLit`, `Subsurface`, `ClearCoat`, `TwoSidedFoliage`, `Cloth`, `Substrate` |
| `Backend` | `ThinCustom` (project default), `Graph` (a plain `UMaterial` an artist can open) |

## Expressions

- Types: `float` `half` `int` `uint` `bool`, `float2`..`float4`, `Texture2D` `TextureCube`
  `Texture2DArray` `Texture3D`, `SamplerState`, `material`, `Substrate`.
- Statements are HLSL: declarations, `if`/`else`, `for`/`while`/`do` with a constant trip count (64 at
  most), `return`, `?:`, swizzles, casts, constructors. No matrices, no `struct`, no `switch`.
- Math, called bare (`saturate(x)`):

`abs` `acos` `asin` `atan` `atan2` `ceil` `clamp` `cos` `cross` `ddx` `ddy` `distance` `dot` `exp` `exp2` `floor` `fmod` `frac` `fwidth` `length` `lerp` `log` `log2` `log10` `max` `min` `normalize` `pow` `rcp` `reflect` `refract` `round` `rsqrt` `saturate` `sign` `sin` `smoothstep` `sqrt` `step` `tan` `trunc`

- **`sin`, `cos`, `tan` take cycles, not radians** — `sin(0.25)` is 1. For radians write
  `sin(x / 6.2831853)`.
- Engine nodes: `UE.<Node>(Pin = value, …)`, named arguments: `UE.TexCoord(Index = 0)`, `UE.Time()`,
  `UE.Panner(Coordinate = UV, Speed = float2(0.1, 0))`. A node with several outputs names one:
  `UE.ScreenPosition().ViewportUV`. DreamMCP's `lookup` lists any node's pins.
- Textures: `Albedo.Sample(UV)`, `Albedo.Sample(S, UV)`, `Albedo.SampleLevel(UV, Mip)`.
- A scalar widens to any vector; a vector never narrows silently — write the swizzle (`c.rgb`).
- Two identical calls are one node: no need to keep a node in a variable to share it.

## Material functions

```hlsl
// DreamSource/Shader/CheatSheet/MF_CheatSheetTiling.dss -- a material function: tiled UV plus the cell index
/// @library CheatSheet|UV
/// @param UV       The coordinates to tile.
/// @param Scale    Tiles per UV unit.
export float2 MF_CheatSheetTiling(float2 UV, float Scale = 4.0, out float2 Cell)
{
    float2 T = UV * Scale;
    Cell = floor(T);
    return frac(T);
}
```

| Write | Builds |
| :-- | :-- |
| `float3 F(float2 UV) { … }` | nothing — inlined into the graph at each call |
| `export float3 F(float2 UV, float S = 1.0, out float A) { … }` | a material function; `out` = extra outputs, a default = optional input |
| `/// @layer` + `export void L(inout material m)` | a material layer |
| `/// @layerblend` + `export void B(material Bottom, material Top, inout material Result)` | a layer blend |
| `/// @asset /Game/MF_X` + `extern float3 MF_X(float2 UV);` | nothing — calls an existing asset |
| `/// @custom` + `float F(float2 UV) { … }` | one Custom HLSL node; the body is verbatim HLSL (matrices, real loops) |

An entry and other `export`s cannot share a file: put a material and its functions in separate files.

## Instances — `.dsi`

```hlsl
// DreamSource/Shader/CheatSheet/MI_CheatSheetSurface_Red.dsi
#pragma instance(Parent = "M_CheatSheetSurface")

uniform float4 Tint = float4(0.9, 0.2, 0.15, 1.0);
uniform bool UseGlow = false;
```

`Parent` is a bare product name under the same source root, or an object path. Override only what the
parent declares, with the parent's type; metadata belongs to the parent.

## Custom Pass pipelines — `.dsp` (UE 5.8+)

```hlsl
// DreamSource/Shader/Passes/CP_Highlight.dsp
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

The materials a pass uses are ordinary `.dss` files. Details: [Passes](language-v2/passes.md).

## Mistakes that cost the most time

| Code | Severity | Message |
| :-- | :-- | :-- |
| [DSH4200](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4200) | error | '{0}' is not declared; did you mean '{1}'? Names are case-sensitive. |
| [DSH4250](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4250) | error | '{0}' is the GLSL spelling; this language is HLSL, so write '{1}'. |
| [DSH4226](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4226) | error | {0} expects {1}, and this is {2}. |
| [DSH4243](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4243) | error | Both sides of this '/' are integers, and the material graph has no integer division; write 'float(a) / b' for the fraction, or 'floor(float(a) / b)' for the whole part. |
| [DSH4361](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4361) | error | This expression is {0} and the material graph has no matrices; compute it inside the custom body instead. |
| [DSH4246](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4246) | error | The material graph has no hyperbolic node, so '{0}' cannot be lowered; write it in a '/// @custom' body, where the shader compiler has it. |
| [DSH4362](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4362) | error | 'discard' has no material-graph form; set the material's OpacityMask to zero instead, or move the branch into a '/// @custom' function. |
| [DSH2160](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh2xxx/#dsh2160) | error | Unsupported statement: the preprocessor line '#{0}' cannot appear inside a function body; mark the function /// @custom to hand its body to the shader compiler. |
| [DSH4365](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4365) | error | A struct has no graph form; pass one of its fields, or move the whole thing into a '/// @custom' function. |
| [DSH6200](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh6xxx/#dsh6200) | error | '{0}' is a second material entry; '{1}' above it is already the entry, and one file makes one material. |
| [DSH6201](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh6xxx/#dsh6201) | error | '{0}' is exported from a file whose entry is '{1}'; a file makes a material or it makes functions, not both. Move it to its own file, or drop 'export' to make it a helper. |
| [DSH6206](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh6xxx/#dsh6206) | error | '{0}' is a builtin operation and cannot be redeclared; rename the function. |
| [DSH5200](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh5xxx/#dsh5200) | error | A material has no '{0}' pin; did you mean '{1}'? Attribute names are case-sensitive. |
| [DSH5220](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh5xxx/#dsh5220) | error | '{0}.{1}' takes named arguments: write 'Pin = value'. |
| [DSH5201](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh5xxx/#dsh5201) | error | This Custom node declares no output called '{0}'; its outputs are '{1}'. An output is declared by 'AdditionalOutputs'. |
| [DSH7213](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh7xxx/#dsh7213) | error | A texture uniform has no HLSL initializer; write its default asset as '/// @default /Game/...'. |
| [DSH7211](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh7xxx/#dsh7211) | error | '{0}' is a file-scope variable with no storage class; write 'uniform' for a material parameter or 'static const' for a compile-time constant. |
| [DSH7233](https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh7xxx/#dsh7233) | warning | A slider range ('@slider', or 'Slider(min, max)' in a 1.x property) is for a scalar parameter; this one is '{0}', which has no slider, so the range is ignored. |

- A name in the wrong case is not found: `tint` for `Tint`. The message suggests the right one.
- GLSL spellings — `mix`, `fract`, `mod`, `vec3` — are `lerp`, `frac`, `fmod`, `float3`.
- Matrix work, `sinh`/`cosh`/`tanh` and real loops go in a `/// @custom` function.
- Run `check` after every edit; it writes nothing. `compile` builds the asset; `preview` renders it.

<!-- Generated by .skill/gen-knowledge.ps1 from .skill/cheatsheet/cheatsheet.src.md. Edit that file, not this one. -->
