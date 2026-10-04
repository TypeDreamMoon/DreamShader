# DreamShaderLang 1.x — what you need to read and maintain it

Condensed from [`Docs/language/`](../../Docs/language/index.md) and checked against the compiler. When this page
and `Docs/` disagree, `Docs/` wins — except for messages: several 1.x pages still quote messages from the 1.x
generator, which is gone, and the codes below are what is reported today.

**Write new sources as `.dss`** ([`dss.md`](dss.md)). This page is for a project that already has `.dsm` /
`.dsf` files: they keep building, through the **legacy front end**, which reads the 1.x language into the same
tree the 2.0 parser builds — so they get the same binder, the same checks and the same diagnostics. What
1.x always allowed and 2.0 spells differently is a numbered legacy rule that applies to 1.x text only
(`DSH5275`–`DSH5292`), and `dsc check` on a `.dsm` lists them. `dsc migrate` rewrites a 1.x file as `.dss` —
[`dream-shader-migrate`](../dream-shader-migrate/SKILL.md).

---

## 1. File kinds

| Extension | Holds | Builds |
| :-- | :-- | :-- |
| `.dsm` | **one** `Shader` block (`DSH2250` for a second), plus helper blocks (`Function`, `GraphFunction`, `Namespace`, `VirtualFunction`) | the material — a `ShaderFunction` or layer beside the `Shader` is `DSH6201`: a file makes a material or function assets, not both |
| `.dsf` | `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend`, plus helpers; no `Shader` | the function assets |
| `.dsh` | `Function` / `GraphFunction` / `Namespace` / `VirtualFunction`, and 2.0 declarations side by side | nothing — included by `import` |

A `.dsh` naming an asset block is `DSH2249`. The check reads tokens, so a comment or a string that mentions
`Shader(` is fine.

**`import` brings in a `.dsh` only** (`DSH2252` for `import "x.dsf"`), and only one under the file's own source
root: a root-qualified import (`Plugin.X:…`) is `DSH2252` too. A header is parsed on its own and its names
are declared into the importing file — nothing is pasted in as text; a name defined twice is `DSH4210`, a
cycle `DSH4211`.

## 2. The `Shader` block

```c
Shader(Name="UI/M_Panel")                     // -> /Game/UI/M_Panel; Root="Plugin.MyPlugin" for a content plugin
{
    Properties = {
        float4 Tint = float4(1.0, 0.2, 0.2, 1.0) [Group="Look"; SortPriority=10; Description="Base colour";];
        float  Gain = 1.0 [Slider(0, 4);];
        Texture2D Noise = Path(Engine, "EngineResources/WhiteSquareTexture");
    }
    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
        Backend = "Graph";                     // optional; the project default is ThinCustom
    }
    Outputs = {
        float3 Color;
        Base.EmissiveColor = Color;
    }
    Graph = {
        float2 uv = UE.TexCoord(Index=0);
        Color = Tint.rgb * Gain * SampleTexture2D(Noise, uv).r;
    }
    Layout = { Node(Var="Tint", X=60, Y=-290); }   // optional editor positions
}
```

| Section | |
| :-- | :-- |
| `Properties` | parameters. Compact form `float` / `float4` / `Texture2D …`, or a node token (`ScalarParameter`, `VectorParameter`, `TextureObjectParameter`, `StaticSwitchParameter`, …). Of the 22 node tokens, 7 are refused today (`DSH3253`: DoubleVector, TextureCollection, CurveAtlasRow, Dynamic, FontSample, SpriteTextureSampler, SparseVolumeTextureObject). An unknown type is `DSH3252`. Metadata in `[ … ]`: `Group`, `SortPriority`, `Description`, `ParameterName`, `SamplerType`, `Slider(min, max)` — scalars only (`DSH7233`) |
| `Settings` | `UMaterial` properties by name, plus `Backend` (`Graph`, `ThinCustom`; `Instance` is an old alias) |
| `Outputs` | typed variables, bound with `Base.<Pin> = var;`. Every pin the engine's material has is accepted, with aliases (`Emissive`, `AO`, `WPO`, …); an unknown one is `DSH5200` |
| `Graph` | the node graph as statements |
| `Layout` | `Node(Var=…, X=…, Y=…)`, `Comment(Name=…, X, Y, W, H[, Color])` (`DSH3275` for a bad one); `#Region "text"` / `#EndRegion` in `Graph` make comment boxes |

Asset references: `Path(Game, "…")`, `Path(Engine, "…")`, `Path(Plugins.X, "…")`, or a quoted `"/Game/…"`.

**Backends.** `ThinCustom`, the project default, builds the full node graph on a hidden base material behind a
`UDreamShaderMaterialInstance`, which stays in memory (Ephemeral) in the editor until a cook or *Materialize*.
`Graph` builds a plain `UMaterial`, saved on every build.

## 3. `Graph`

Statements: declarations, assignments, calls, `if` / `else`. Types are matched case-insensitively (`vec3` ≡
`float3`); texture, `SamplerState` and `Substrate` variables need an initializer (`DSH2215`). Graph has no matrix
values (`DSH4361`; a `mat3` property is `DSH3252`).

- **Math builtins** are HLSL's intrinsics, typed: operands are widened to the widest one, a scalar broadcasts,
  and a narrower vector is `DSH4226` — the compiler checks component counts. Named arguments select a pin
  where the node has named ones (`lerp(A, B, Alpha = t)`); an unknown name is `DSH4216`.
- **GLSL spellings** `mix`, `fract`, `mod` resolve in 1.x with a warning (`DSH5277`); a name matched only by
  case resolves with `DSH5275`. `dsc migrate` respells both.
- `reflect` and `refract` have no node: they expand to a 4-node and a 14-node subgraph.
- Lookup order: `Path()`, property calls, `SampleTexture2D` and the `UE.*` sugar at parse time; then
  constructors, `UE.*` / `Substrate.*`, texture methods, builtins, user functions. A property call beats a
  builtin, a builtin beats a user function.
- A misspelled name is `DSH4208` / `DSH4200`, with "did you mean" for a case-only match.

## 4. Reusable code

| Block | Builds | |
| :-- | :-- | :-- |
| `Function` | a Custom HLSL node holding its body, and the bodies of the `Function`s it calls | dense arithmetic; a body that is HLSL |
| `GraphFunction` | a Custom node whose `UE.*` calls are pulled out into real nodes wired to its inputs | HLSL that needs engine nodes |
| `Namespace(Name="N")` | groups `Function`s as `N::F` | |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` | a material function / layer / blend asset | |
| `VirtualFunction(Name="…") { Options = { Asset = Path(…); } … }` | nothing — declares an existing function asset so `Graph` can call it | multiple outputs: `F(a, OutputIndex=1)` |

- **A function named like a builtin** (`lerp`, `dot`, `saturate`, …) is `DSH6206` at its declaration.
- **Inside a `Function` / `GraphFunction` body**, the whole identifiers `mix` `fract` `mod` `vec2..4` `ivec*`
  `uvec*` `bvec*` `mat2..4` are rewritten case-insensitively (`Mix` → `lerp`) — **silently**, comments and
  strings excepted. A local named `Mix` or `Mod` becomes something else. `Graph` blocks are not rewritten.
- `#include "/Plugin/X/Y.ush"` lines at the start of a `Function` body are hoisted onto the Custom node's include
  list, so a header that defines functions works there.
- **`Function SelfContained` / `Inline` means the opposite in 2.0.** It becomes `@custom selfcontained`, which
  embeds nothing: a call from that body to another `Function` is left to the shader compiler (`DSH6264`), where
  1.x embedded it. A plain `Function` embeds what it calls; drop the modifier.

## 5. What 1.x gives you that 2.0 spells differently

| 1.x | `.dss` |
| :-- | :-- |
| `Shader(Name=…) { Properties / Settings / Outputs / Graph }` | `#pragma material(…)`, `uniform`s with `///` tags, `export void M(inout material m)` |
| `ShaderFunction` | `export` function; extra outputs are `out` parameters |
| `Function` / `GraphFunction` | `/// @custom` function, body verbatim |
| `VirtualFunction` | `/// @asset` + `extern` prototype |
| `import "x.dsh";` | `#include "x.dsh"` |

The full rewrite table, rule by rule: [`Docs/tools/migrate.md`](../../Docs/tools/migrate.md).

## See also

- [`Docs/language/index.md`](../../Docs/language/index.md) — the 1.x grammar
- [`Docs/graph/index.md`](../../Docs/graph/index.md) — what `Graph` accepts
- [`Docs/diagnostics/README.md`](../../Docs/diagnostics/README.md) — every code
- [`dss.md`](dss.md) — the 2.0 language, for anything new
