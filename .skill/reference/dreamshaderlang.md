# DreamShaderLang — the subset an author needs

Condensed from `Plugins/DreamShader/Docs/`. Every claim here is either quoted from those docs or
was observed in a real commandlet run. When this page and `Docs/` disagree, `Docs/` wins — it is
generated against the source, this is a working summary.

Full reference: [`Docs/index.md`](../../Docs/index.md) · online at <https://lang.64hz.cn/docs>

---

## 1. File kinds

| Extension | Holds | Generates |
| :-- | :-- | :-- |
| `.dsm` | at most one `Shader` block, plus any function/namespace block | the `UMaterial`, plus every function asset it declares |
| `.dsf` | `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend`, plus helpers. **No `Shader`.** | the function assets |
| `.dsh` | `Function` / `GraphFunction` / `Namespace` / `VirtualFunction` only | nothing — consumed through `import` |

The kind check is a **case-insensitive substring scan of the file text**, run before parsing. A
`.dsh` containing the literal text `Shader(` anywhere — including inside a comment or a string — is
rejected. Write `Shader (` with a space if you must mention it.

Default layout under `<Project>/DShader/` (the *Source Directory* project setting):

```
DShader/
├─ Materials/   *.dsm
├─ Functions/   *.dsf
├─ Shared/      *.dsh
└─ Packages/    installed libraries — excluded from compile -All (except .dsf, see Docs/tools/packages.md)
```

---

## 2. The `Shader` block

```c
Shader(Name="<path under Root>"[, Root="Game"])
{
    Properties = { … }   // material parameters — become UMaterialExpression*Parameter nodes
    Settings   = { … }   // UMaterial properties
    Outputs    = { … }   // declare variables, then bind them to Base.*
    Graph      = { … }   // the node graph, written as statements
    Layout     = { … }   // optional editor node positions
}
```

`Name` is the asset path relative to `Root`, and `Root` defaults to `Game` — so
`Name="UI/M_Panel"` produces `/Game/UI/M_Panel`. `Root="Plugin.MyPlugin"` targets a content plugin.

A minimal, verified-compiling material:

```c
Shader(Name="DreamMaterials/M_Minimal")
{
    Properties = {
        vec3 Tint = vec3(1.0, 0.2, 0.2);
    }

    Settings = {
        Domain = "UI";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        Color = Tint;
    }
}
```

### `Settings`

Verified keys in use in this repo: `Backend`, `Domain`, `ShadingModel`, `BlendMode`, `TwoSided`.
Everything else resolves by reflection against `UMaterial` — see
[`Docs/settings/material.md`](../../Docs/settings/material.md) for the blessed set and
[`Docs/settings/material-enums.md`](../../Docs/settings/material-enums.md) for the string values.

`Backend = "Graph"` builds a real node graph. The default backend produces a *thin-custom* material
(one `Custom` HLSL node behind a generated base). Pick `Graph` when the material must be readable or
editable in the Material Editor; leave it default when you only care about the shader.

### `Outputs`

Declare a typed variable, then bind it. Binding targets, in the decompiler's fixed order:

```
EmissiveColor  BaseColor  Metallic  Specular  Roughness  Anisotropy  Opacity  OpacityMask
Normal  Tangent  WorldPositionOffset  SubsurfaceColor  CustomData0  CustomData1
AmbientOcclusion  Refraction  PixelDepthOffset  MaterialAttributes  FrontMaterial (UE 5.4+)
```

```c
Outputs = {
    float3 EmissiveColor;
    float  Opacity;

    Base.EmissiveColor = EmissiveColor;
    Base.Opacity       = Opacity;
}
```

---

## 3. Types

Scalars `float float1 half half1 int uint bool` · vectors `float2..4 half2..4 vec2..4 int2..4
uint2..4 bool2..4 ivec2..4 uvec2..4 bvec2..4` · textures `Texture2D TextureCube Texture2DArray
Texture3D VolumeTexture` · opaque `SamplerState MaterialAttributes Substrate StaticBool
StaticBoolParameter`.

- Matched **case-insensitively** everywhere.
- `vec3` ≡ `float3`; `int3`/`bool3`/`float3` differ only as documentation.
- **There are no matrix types.** `mat3` / `float3x3` are rejected. Matrix work goes through
  `UE.TransformVector` / `UE.TransformPosition`.
- No `vec1`. Use `float`.
- Texture, `SamplerState` and `Substrate` declarations inside `Graph` **require an initializer**.
- `Substrate` requires UE 5.4+.

Full validity matrix per declaration position: [`Docs/language/types.md`](../../Docs/language/types.md).

---

## 4. `Properties` — parameters

Compact form uses a type token; explicit form uses a parameter-node token.

```c
Properties = {
    ScalarParameter Strength = 0.65 [
        Group="Foil | Style";
        SortPriority=10;
        Description="Overall holographic brightness";
    ];
    VectorParameter Tint = float4(1.0, 1.0, 1.0, 1.0);
    TextureSampleParameter2D Artwork = Path(Engine, "EngineResources/WhiteSquareTexture") [
        SamplerType="SAMPLERTYPE_Color";
    ];
    TextureObjectParameter Noise = Path(Game, "Textures/T_Noise");
}
```

The 21 parameter-node tokens: `ChannelMaskParameter` `CurveAtlasRowParameter`
`DoubleVectorParameter` `DynamicParameter` `FontSampleParameter`
`RuntimeVirtualTextureSampleParameter` `ScalarParameter` `SparseVolumeTextureObjectParameter`
`SparseVolumeTextureSampleParameter` `StaticBoolParameter` `StaticComponentMaskParameter`
`StaticSwitchParameter` `TextureCollectionParameter` `TextureObjectParameter`
`TextureSampleParameter2D` `TextureSampleParameter2DArray` `TextureSampleParameterCube`
`TextureSampleParameterCubeArray` `TextureSampleParameterSubUV` `TextureSampleParameterVolume`
`VectorParameter`.

Metadata keys seen in this repo: `Group`, `SortPriority`, `Description`, `ParameterName`,
`SamplerType`. Full list: [`Docs/parameters/metadata.md`](../../Docs/parameters/metadata.md).

Asset references use `Path(<root>, "<path>")` — `Path(Game, …)`, `Path(Engine, …)`,
`Path(Plugins.LGUI, …)`.

---

## 5. `Graph`

Statements, not expressions-in-a-tree. Declare, assign, call.

```c
Graph = {
    float2 uv = UE.TexCoord(Index=0);
    float  t  = UE.Time();
    float3 c  = Tint.rgb * saturate(sin(uv.y * 40.0 + t));
    Color = c;
}
```

### Math builtins — exactly 19 spellings, called bare

```
abs ceil cos floor frac fract normalize saturate sin sqrt      (1 argument)
dot fmod max min mod pow                                       (2 arguments)
clamp lerp mix                                                 (3 arguments)
```

Aliases: `lerp`≡`mix`, `frac`≡`fract`, `fmod`≡`mod`. Arity is exact; every argument is positional
— a named argument is reported as an *arity* error. Component counts are **not** checked, so
`dot(float3Value, floatValue)` passes DreamShader and fails later in Unreal's shader compile.

> **These 19 names are reserved and shadow user code silently.** A `Function` or property named
> `lerp`, `dot`, `pow`… is unreachable from a `Graph` block, with no diagnostic. Rename it.

A misspelling is not a math error — `saturte(x)` reports `Unknown Graph function 'saturte'.`

### `UE.*` builtins

`UE.TexCoord(Index=0)` `UE.Time()` `UE.VertexColor()` `UE.ScreenPosition()` `UE.WorldPosition()`
`UE.CameraVectorWS()` `UE.ObjectPositionWS()` `UE.Panner(…)` `UE.SceneTexture(…)`
`UE.TransformVector(…)` `UE.CollectionParameter(…)` `UE.StaticSwitchParameter(…)` … and the escape
hatch `UE.Expression(Class="<UMaterialExpression suffix>", OutputType="float3", …)`, which reaches
any node without a curated wrapper. `SampleTexture2D(tex, uv)` is called bare, not `UE.`-prefixed.

Full catalogue: [`Docs/builtins/ue.md`](../../Docs/builtins/ue.md).

Name resolution order inside `Graph`: constructors → `UE.SceneTexture` → `UE.*` → `Substrate.*` →
math builtins → `SampleTexture2D` → properties → user functions.

---

## 6. Reusable code

| Block | Lowers to | Use when |
| :-- | :-- | :-- |
| `Function` | one HLSL `Custom` node, via a generated `.ush` helper | dense arithmetic; you want HLSL, not 40 nodes |
| `GraphFunction` | inlined into the caller's graph, hoisting `UE.*` calls | reusable logic that must stay real nodes |
| `ShaderFunction` | a `UMaterialFunction` asset | shared across materials, visible in the library |
| `VirtualFunction` | nothing — declares an **existing** asset so `Graph` can call it | calling engine or plugin material functions |

```c
GraphFunction BuildFoil(in float2 uv, in float strength, out float4 result)
{
    float3 spectrum = 0.5 + 0.5 * cos(6.2831853 * (uv.x + float3(0.0, 0.333333, 0.666667)));
    result = float4(spectrum * strength, 1.0);
}
```

> **GLSL identifier rewrite trap.** Inside a `Function` / `GraphFunction` **body**, the whole
> identifiers `mix` `fract` `mod` `vec2..4` `ivec*` `uvec*` `bvec*` `mat2..4` are rewritten
> case-insensitively. A local named `Mix`, `Mod` or `Fract` is silently renamed to `lerp`, `fmod`,
> `frac`. No diagnostic — it surfaces as an HLSL error or as wrong math. `Graph` blocks and
> `ShaderFunction` bodies are **not** rewritten.

`VirtualFunction` declares an asset that already exists:

```c
VirtualFunction(Name="MF_LexUI_Clip")
{
    Options = {
        Asset = Path(Plugins.LGUI, "Materials/MF_LexUI_Clip");
    }
    Inputs  = { opt float CoordinateInDataTex = 0.0; opt float Opacity = 1.0; }
    Outputs = { float Result; }
}
```

Multiple outputs are selected at the call site with `OutputIndex=`:

```c
float  clipAmount = MF_LexUI_Clip(coord, opacity);
float3 colour     = MF_LexUI_RectBlock(MainTexture, OutputIndex=0);
float  alpha      = MF_LexUI_RectBlock(MainTexture, OutputIndex=1);
```

---

## 7. `import`

```c
import "Shared/Common.dsh";
import "Functions/F_Tint.dsf";
```

The whole closure is inlined into **one** text before parsing. Consequences: at most one `Shader`
block across the entire closure, and a `.dsh` that imports a `.dsf` full of `ShaderFunction(` blocks
is legal (each file is kind-checked against its own extension only).

---

## 8. `Layout`

Optional, and worth keeping for `Backend = "Graph"` materials — without it a large regenerated graph
comes back auto-laid-out.

```c
Layout = {
    Comment(Name="Premultiply", X=1200, Y=-16, W=400, H=220, Color=float4(1,1,1,1));
    Node(Var="Tint", X=60, Y=-290);
}
```

`#Region "text"` / `#EndRegion` around `Graph` statements produce comment boxes independently of the
`Layout` block.

---

## See also

- [`Docs/language/index.md`](../../Docs/language/index.md) — declaration grammar
- [`Docs/graph/index.md`](../../Docs/graph/index.md) — what `Graph` accepts, and
  [`unsupported.md`](../../Docs/graph/unsupported.md) — what it does not
- [`Docs/diagnostics/index.md`](../../Docs/diagnostics/index.md) — every message, by stage
- [`Docs/examples/index.md`](../../Docs/examples/index.md) — complete sources
