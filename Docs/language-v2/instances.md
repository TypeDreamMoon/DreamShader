# Material instances — `.dsi`

> [DreamShader](../index.md) » [DreamShaderLang 2.0](index.md) » **Material instances**

A `.dsi` file is one `UMaterialInstanceConstant`: a parent, a few overridden properties, and the
parameters it sets. It has no graph, so it has no functions, no `#include` and no `#pragma material`
— only a pragma and `uniform` lines.

```hlsl
// MI_HeroSkin.dsi
#pragma instance(Parent = "M_Skin", BlendMode = Masked, TwoSided = true)

uniform float Roughness = 0.35;
uniform float3 Tint = float3(1.0, 0.82, 0.76);

/// @static
uniform bool UseDetail = true;

/// @default /Game/Characters/Hero/T_Hero_Skin_D
uniform Texture2D Albedo;

/// @name Base Color Boost
uniform float BaseColorBoost = 1.2;
```

The asset is named after the file and goes where a `.dss` of the same place would put its product.

## The parent

`Parent` is either

- **a bare name** — a material, or another instance, built by a DreamShader source under the same
  source root (`DSH8261` when none is, `DSH8262` when several are), or
- **an object path** (`/Game/Materials/M_Skin`) to any material or instance, DreamShader's or not.

A parent that comes from a source is compiled first when its asset is missing or older than that
source (`DSH8264`), so building an instance never sees a stale parameter list. Instances of instances
work; a `Parent` chain that loops is `DSH8263`.

## Overrides

An override is a `uniform` **declared as the parent declares that parameter**, with the value it is
set to. The file is checked against the parent's real parameters: an unknown name is `DSH7258`
(`DSH7264` when only the case differs — the engine would silently set nothing), another type is
`DSH7259`.

| Parent parameter | Override |
| :-- | :-- |
| scalar | `uniform float Gain = 2.0;` (`int` and `bool` parameters are scalars too: `uniform bool Lit = false;`) |
| vector | `uniform float3 Tint = float3(0, 1, 0);` — a `float3` keeps the parent's alpha, a `float4` sets all four |
| static switch | `/// @static` + `uniform bool UseDetail = false;` |
| static component mask | `/// @static` + `uniform bool4 Mask = bool4(true, false, false, false);` |
| texture | `/// @default <asset>` + `uniform Texture2D Albedo;` — no initializer; `/// @default None` clears it |
| font | `/// @default <font asset>`, `/// @page <n>`, `uniform Font Glyphs;` |
| runtime virtual texture, sparse volume texture, texture collection, parameter collection | `/// @default <asset>` + `uniform RuntimeVirtualTexture Ground;` (and `SparseVolumeTexture`, `TextureCollection`, `ParameterCollection`) |

A value has to be something the compiler can fold — a literal or arithmetic over literals
(`DSH7265`). A parameter whose name is not an identifier is declared under any identifier with
`/// @name <real name>`. Metadata (`@group`, `@sort`, `@slider`, `@desc`) belongs to the parent and
is ignored with a warning (`DSH7262`).

Only **global** parameters can be set. One that exists only on a material layer or blend is
`DSH7268`.

An override deleted from the file is deleted from the asset on the next build: the instance is
rebuilt from the file, not patched.

## Keys of `#pragma instance`

Besides `Parent`, a key is a material property an instance can override. Setting one writes the value
**and** the override flag the engine pairs with it; a key that is no longer in the file goes back to
"not overridden".

| Keys | Where they live in the engine |
| :-- | :-- |
| `BlendMode`, `ShadingModel`, `TwoSided`, `bIsThinSurface`, `DitheredLODTransition`, `OpacityMaskClipValue`, `bCastDynamicShadowAsMasked`, `bOutputTranslucentVelocity`, `bHasPixelAnimation`, `bEnableTessellation`, `DisplacementScaling`, `MaxWorldPositionOffsetDisplacement`, ... | the fields of `FMaterialInstanceBasePropertyOverrides` — whatever this engine version has |
| `PhysMaterial`, `PhysMaterialMask`, `SubsurfaceProfile`, `SpecularProfile`, `BlendableLocation`, `BlendablePriority`, `NaniteOverrideMaterial` (and an engine fork's own, such as `ToonProfile`) | properties of the instance itself |

Enum values go without their prefix (`Translucent`, not `BLEND_Translucent`), assets as object
paths or `None`. An unknown key is `DSH8249`, a bad value `DSH8250`. `Backend`, `UsageFlags`, the
`bOverride...` flags and the raw parameter arrays are refused with the reason (`DSH8251`).

## Tools

- **New Instance** in the Material Content Browser writes the template with the chosen parent.
- `dsc compile MI_X.dsi`, `dsc check`, `dsc dump-ir` and `dsc index` take a `.dsi`.
- `dsc decompile /Game/.../MI_X` writes a `.dsi` for a plain material instance: what differs from
  the parent. Overrides the language cannot state are named and left out (`DSH9101`, `DSH9104`,
  `DSH9105`).
- **Adopt Into Source** on a hand-edited instance rewrites the `.dsi` in place, value by value, so
  comments and order in the file survive. A scalar declared as `bool`, `int` or `uint` becomes
  `float` if its current value cannot be represented by that type; representable values keep the
  existing declaration.
- `dump-graph` dumps an instance as its parent, its overrides and its keys.

## See also

- [The 2.0 language](index.md)
- [Diagnostics `DSH7250`–`DSH7270`](../diagnostics/DSH7xxx.md), [`DSH8240`–`DSH8265`](../diagnostics/DSH8xxx.md),
  [`DSH9100`–`DSH9109`](../diagnostics/DSH9xxx.md)
