# DreamShaderLang 2.0 — the subset an author needs

Condensed from [`Docs/language-v2/`](../../Docs/language-v2/index.md) and checked against the compiler. When
this page and `Docs/` disagree, `Docs/` wins. Custom Pass pipelines (`.dsp`) have their own page,
[`dsp.md`](dsp.md); reading or maintaining 1.x sources (`.dsm` / `.dsf`) is [`legacy.md`](legacy.md).

A `.dss` is **HLSL with declarations**: parameters are `uniform`s, metadata rides on `///` doc lines and
`#pragma` lines, and one `export` function is the asset.

---

## 1. File kinds

| Extension | Holds | Builds |
| :-- | :-- | :-- |
| `.dss` | `uniform`s, helpers, `export` functions, `extern` prototypes, `#pragma material` | a material, or material functions / layers / blends |
| `.dsi` | `#pragma instance(Parent = …)` and `uniform` overrides — nothing else | one material instance |
| `.dsp` | `#pragma pipeline`, `buffer`s, `pass`es | one Custom Pass pipeline, UE 5.8+ — [`dsp.md`](dsp.md) |
| `.dsh` | shared helpers, included by `#include`; may hold 2.0 and 1.x declarations | nothing on its own |

**Where the asset lands:** under `/Game`, at the file's folder relative to its source root (`DShader/`), named
after the `export` function. `DShader/UI/M_Panel.dss` with `export void M_Panel(…)` builds `/Game/UI/M_Panel`. A
file under a plugin's `DShader/` builds under the plugin's mount point. `/// @name Other` renames the asset;
`/// @name /Game/Some/Path/M_X` puts it at that exact path. Details:
[`Docs/generation/asset-paths.md`](../../Docs/generation/asset-paths.md).

## 2. A material

```hlsl
// DShader/UI/M_ScanlinePanel.dss -- scanlines and a glow band that scrolls down the panel
#pragma material(Domain = UI, BlendMode = Translucent, ShadingModel = Unlit)

/// @group Scanline|Style   @sort 10   @desc Base panel colour; alpha scales the opacity
uniform float4 Tint = float4(0.25, 0.85, 1.0, 1.0);

/// @group Scanline|Style   @sort 20   @slider 8 128   @desc Scanlines across the panel height
uniform float LineCount = 48.0;

/// @group Scanline|Motion   @sort 30   @slider 0 2   @desc Glow band travel speed, panels per second
uniform float ScrollSpeed = 0.35;

float GlowBand(float V, float Phase)              // a helper: inlined into the graph at each call
{
    float Band = frac(V - Phase);
    return pow(saturate(1.0 - abs(Band - 0.5) * 4.0), 3.0);
}

export void M_ScanlinePanel(inout material m)     // the entry: this signature is what makes it a material
{
    float2 UV = UE.TexCoord(Index = 0);
    float Scan = 0.5 + 0.5 * sin(UV.y * LineCount * 6.2831853);
    float Glow = GlowBand(UV.y, UE.Time() * ScrollSpeed);
    m.EmissiveColor = Tint.rgb * (0.4 + Glow * 1.6);
    m.Opacity = saturate(Scan * 0.6 + Glow) * Tint.a;
}
```

- **Exactly one entry**, `export void <Name>(inout material m)`. A second one is `DSH6200`; an entry beside
  other `export`s in one file is `DSH6201` — a material and its functions go in separate files.
- **Outputs** are members of `m`: `m.BaseColor`, `m.Metallic`, `m.Roughness`, `m.EmissiveColor`,
  `m.Opacity`, `m.OpacityMask`, `m.Normal`, `m.WorldPositionOffset`, … — every pin the engine's material has,
  checked against the engine (`DSH5200` for a name it does not have). Case-sensitive.

## 3. Parameters

```hlsl
uniform <Type> <Name> = <value>;      // a material parameter
static const <Type> <Name> = <value>; // a constant, folded where it is used
```

| Doc tag (`///` above the uniform) | Effect |
| :-- | :-- |
| `@group A\|B` | the parameter group |
| `@sort N` | sort priority (an integer, `DSH7221` otherwise) |
| `@desc text` | the tooltip |
| `@slider min max` | slider range — **scalars only**; on a vector or bool it is ignored with `DSH7233` |
| `@name Real Name` | the parameter's name in the engine — the identifier when absent. A name that is not an identifier goes here, and so does the old name when the identifier is renamed, which keeps every material instance's override |
| `@static` | on a `uniform bool`: a static switch instead of a dynamic scalar (`DSH7223` on anything else) |
| `@default /Game/T_X` | a texture's default asset; `@sampler` sets its sampler type |

Several tags share a line: `/// @group Look   @sort 10   @desc Base colour`. A tag that does not apply is a
warning (`DSH7224`), a repeated one `DSH7229`; an unknown tag is passed to the node by its engine property name.

- **Textures take no initializer.** `/// @default /Engine/EngineResources/WhiteSquareTexture` above
  `uniform Texture2D Albedo;` (`DSH7213` for `= …`). `/// @default` + `static const Texture2D T;` is a
  TextureObject nobody can override.
- No uniform arrays (`DSH7216`); a global without `uniform` or `static const` is `DSH7211`.

## 4. `#pragma material`

```hlsl
#pragma material(Domain = PostProcess, BlendMode = Translucent, ShadingModel = Unlit, TwoSided = true)
```

Keys are `UMaterial` property names, resolved by reflection: `Domain`, `BlendMode`, `ShadingModel`,
`TwoSided`, `bUsedWithSkeletalMesh`, `OpacityMaskClipValue`, … Enum values go without their prefix
(`Translucent`, not `BLEND_Translucent`). Two keys are DreamShader's own:

| Key | Values | |
| :-- | :-- | :-- |
| `Backend` | `ThinCustom` (the project default), `Graph` | `ThinCustom` builds the node graph on a hidden base material behind a `UDreamShaderMaterialInstance`, Ephemeral in the editor; `Graph` builds a plain `UMaterial` an artist can open. `Instance` is an old alias (`DSH7204`) |
| `Substrate` | `Legacy`, `Bridge`, `Native` | how Substrate values are built — [`Docs/language-v2/substrate.md`](../../Docs/language-v2/substrate.md) |

Enum strings: [`Docs/settings/material-enums.md`](../../Docs/settings/material-enums.md).

## 5. Graph code

Statements and expressions are HLSL: declarations, `if` / `else`, `for` / `while` / `do` with a **constant
trip count** (unrolled, 64 at most — `DSH4360`), `return`, `?:`, swizzles, casts, constructors.

| | |
| :-- | :-- |
| Types | `float half double int uint bool`, `float2`..`float4` (and the other vectors), `Texture2D TextureCube Texture2DArray Texture3D VolumeTexture`, `SamplerState`, `material`, `Substrate` |
| Math | HLSL's intrinsics — `abs saturate lerp clamp min max pow sqrt rsqrt exp log sin cos tan atan2 frac floor ceil round sign step smoothstep dot cross length distance normalize reflect refract ddx ddy fwidth` and more. Named arguments select a pin where the node has named pins: `lerp(A, B, Alpha = t)` |
| Engine nodes | `UE.<Node>(Pin = value, …)` — **named arguments**; positional is `DSH5220` except for a few (`TexCoord`, `Constant*`, `Transform*`, `Panner`, `Mask`, `Time`). A node with several outputs used as a value names one: `UE.ScreenPosition().ViewportUV` (`DSH5201`). The catalogue: [`Docs/builtins/ue.md`](../../Docs/builtins/ue.md) |
| Textures | `Albedo.Sample(UV)`, `Albedo.Sample(S, UV)`, `Albedo.SampleLevel(UV, Mip)` — positional arguments |
| Conversions | a scalar widens to any vector; a vector never narrows silently — write the swizzle (`v.rgb`) (`DSH4226`) |

Two identical calls are one node, so there is no reason to store a node in a variable to share it.

## 6. Functions

| Write | Builds | Notes |
| :-- | :-- | :-- |
| `float3 F(float2 UV) { … }` | nothing — **inlined** into the graph at each call | a file-local helper; no matrices, no unbounded loops, as in the entry |
| `export float3 F(float2 UV, float S = 1.0) { … }` | a material function | extra outputs are `out` parameters; a default argument makes an input optional; `/// @library Cat\|Sub`, `/// @param UV text`; `/// @static P` makes a `bool` parameter a StaticBool pin |
| `/// @layer` `export void L(inout material m)` | a material layer | |
| `/// @layerblend` `export void B(material Bottom, material Top, inout material Result)` | a layer blend | `Result` starts empty |
| `/// @asset /Game/MF_X` `extern float3 MF_X(float2 UV);` | nothing — calls the existing asset | `DSH6202` without `@asset` |
| `/// @custom` `float F(float2 UV) { … }` | one Custom HLSL node per call | the body is **verbatim HLSL**: matrices, real loops, `#include`, anything the shader compiler takes. Nothing checks it but `check -Shaders`. A `UE.` call inside is lifted into a real node wired in — it may read the parameters and globals, not the body's locals (`DSH6326`) |

`#include "Shared/Common.dsh"` (or `import "Shared/Common.dsh";`) — double-quoted, a `.dsh` or `.dss` only.

## 7. Instances — `.dsi`

```hlsl
// DShader/UI/MI_ScanlinePanel_Red.dsi
#pragma instance(Parent = "M_ScanlinePanel", BlendMode = Translucent)

uniform float4 Tint = float4(1.0, 0.2, 0.15, 1.0);
/// @default /Game/UI/T_Noise
uniform Texture2D Noise;
```

`Parent` is a bare name — the product of a `.dss` / `.dsi` under the same source root, built first when
missing or stale — or an object path. Each override is declared as the parent declares it (`DSH7258` unknown
name, `DSH7264` a case-only match, `DSH7259` another type); metadata belongs to the parent. Only global
parameters — a layer's are `DSH7268`. Full rules: [`Docs/language-v2/instances.md`](../../Docs/language-v2/instances.md).

## 8. Traps

| Mistake | Code |
| :-- | :-- |
| a name in the wrong case — `tint` for `Tint` | `DSH4200` / `DSH4208`, with "did you mean" |
| a GLSL spelling — `mix`, `fract`, `mod`, `vec3` | `DSH4250` — write `lerp`, `frac`, `fmod`, `float3` |
| a function named like a builtin — `float lerp(…)` | `DSH6206` |
| a vector into a narrower place | `DSH4226` — swizzle it |
| integer `/` | `DSH4243` |
| a matrix anywhere in the graph | `DSH4361` — do matrix work in a `/// @custom` body |
| `sinh` / `cosh` / `tanh` | `DSH4246` — no node; use a `/// @custom` body |
| `discard` in a branch, `switch` | `DSH4362`, `DSH2160` |
| a `struct` outside a `/// @custom` body | `DSH4365` |
| reading a local nothing assigned | `DSH4376` |
| a `material` parameter on a `/// @custom` function | `DSH6252` — the Custom node has no attributes input |
| a required pin of a `UE.` node left open | `DSH5219`, a warning: the material compile decides |

Every code has its page: [`Docs/diagnostics/README.md`](../../Docs/diagnostics/README.md).

## See also

- [`Docs/language-v2/index.md`](../../Docs/language-v2/index.md) — the whole 2.0 language
- [`Docs/language-v2/substrate.md`](../../Docs/language-v2/substrate.md) — Substrate values and their sugar
- [`Docs/builtins/ue.md`](../../Docs/builtins/ue.md) — every `UE.*` node
- [`dsp.md`](dsp.md) — Custom Pass pipelines · [`legacy.md`](legacy.md) — the 1.x language
