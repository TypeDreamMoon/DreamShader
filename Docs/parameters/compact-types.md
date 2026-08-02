# Compact type tokens

> [DreamShader](../index.md) » [Parameters](index.md) » **Compact type tokens**

The 39 built-in type tokens a `Properties` declaration may use without naming an Unreal expression
class: 7 scalar spellings, 27 vector spellings and 5 texture spellings.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — the `Properties` section of a `Shader`, `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` |
| Kind | parameter type tokens |
| Generates | `UMaterialExpressionScalarParameter`, `UMaterialExpressionVectorParameter`, `UMaterialExpressionTextureObjectParameter` — or, with `const`, `UMaterialExpressionConstant`, `Constant2Vector`, `Constant3Vector`, `Constant4Vector`, `UMaterialExpressionTextureObject` |
| Since | `1.2.0`; `VolumeTexture` since `1.3.8`; `const` since `1.2.6` |

## Synopsis

```c
[const] { <scalar-token> | <vector-token> | <texture-token> } <name> [ = <default> ] [ [ <metadata> ] ] ;
```

All tokens are compared **case-insensitively**: `FLOAT3`, `Float3` and `float3` are the same token.

## Scalar tokens

Seven spellings, all identical in every observable respect.

| Token | Type | Components | Node (parameter) | Node (`const`) |
| :-- | :-- | :-- | :-- | :-- |
| `float` | Scalar | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `float1` | Scalar | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `half` | Scalar | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `half1` | Scalar | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `int` | Scalar | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `uint` | Scalar | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |
| `bool` | Scalar | 1 | `UMaterialExpressionScalarParameter` | `UMaterialExpressionConstant` |

> [!NOTE]
> `int`, `uint`, `bool` and `half` carry **no** integer, boolean or precision semantics at the node
> level. The generated node is a float `ScalarParameter` in every case, and the material graph performs
> no truncation. The only place an integer marker exists at all is a direct integer *constructor* call
> in `Graph` — see [Conversions](../graph/conversions.md#float-int-and-bool).

### Scalar default grammar

```c
float A = 0.5;      float B = -2;      float C = 1e3;      float D = true;
```

The value is parsed as a `double`, with `true` → `1.0` and `false` → `0.0` accepted as aliases
(case-insensitive). Anything else fails with
`Invalid scalar default value '{Text}' for property '{Name}'.`

With no `= <default>` the node keeps the engine default `0.0` — the default value is never written.

## Vector tokens

27 spellings. Every one produces a `UMaterialExpressionVectorParameter` whose `DefaultValue` is a full
`FLinearColor`; only the declared component count differs, and it is what decides which node output a
`Graph` read targets.

The count comes from the **last character** of the token: a token ending in `2` is 2 components, one
ending in `4` is 4 components, anything else is 3.

| Token | Components | Node (parameter) | Node (`const`) |
| :-- | :-- | :-- | :-- |
| `float2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `float3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `float4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `half2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `half3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `half4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `vec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `vec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `vec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `int2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `int3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `int4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `uint2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `uint3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `uint4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `bool2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `bool3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `bool4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `ivec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `ivec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `ivec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `uvec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `uvec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `uvec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |
| `bvec2` | 2 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant2Vector` |
| `bvec3` | 3 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant3Vector` |
| `bvec4` | 4 | `UMaterialExpressionVectorParameter` | `UMaterialExpressionConstant4Vector` |

A `const` declaration with component count 3 forces `A = 1.0` on the generated `Constant3Vector`.

### Vector default grammar

```c
<anything> ( <part> [ , <part> ] … )
```

| Rule | Behaviour |
| :-- | :-- |
| Text before `(` | **ignored** — `float4(…)`, `vec3(…)`, `(…)` and `banana(…)` all parse |
| Delimiters | from the first `(` to the **last** `)` |
| Parts | split on every `,`; empty parts are dropped; each part is a `double`, or `true` / `false` (case-insensitive) |
| 1 part `a` | `(a, a, a, 1)` |
| 2 parts `a, b` | `(a, b, 0, 0)` |
| 3 parts `a, b, c` | `(a, b, c, 1)` |
| 4 parts `a, b, c, d` | `(a, b, c, d)` |
| More than 4 parts | parts 5 and beyond are **never read** — not even parsed, so an unparsable part there is not an error |
| Any part unparsable | `Invalid vector default value '{Text}' for property '{Name}'.` |

With no `= <default>` the node keeps the engine default `(1, 1, 1, 1)`.

> [!WARNING]
> **The declared component count is never checked against the literal's arity.** `float2 P = float4(1, 2, 3, 4);`
> and `float4 P = vec2(1, 2);` both parse. The literal fills the `FLinearColor` by the table above and
> the declared count then decides which output the `Graph` reads — so `float4 P = vec2(1, 2)` yields
> `(1, 2, 0, 0)` read as RGBA. Write the literal with the same arity as the declared token.

> [!WARNING]
> The token before `(` is not validated either, so a typo such as `flaot3(1, 0, 0)` is accepted
> silently as a vector literal. Only the *declaration's* type token is checked.

## Texture tokens

Five spellings, four distinct dimensions.

| Token | Texture type | Explicit dimension | Node (parameter) | Node (`const`) |
| :-- | :-- | :-- | :-- | :-- |
| `Texture2D` | `Texture2D` | yes | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `TextureCube` | `TextureCube` | yes | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `Texture2DArray` | `Texture2DArray` | yes | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `Texture3D` | `VolumeTexture` | yes | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |
| `VolumeTexture` | `VolumeTexture` | yes | `UMaterialExpressionTextureObjectParameter` | `UMaterialExpressionTextureObject` |

`Texture3D` and `VolumeTexture` are exact synonyms.

A compact texture declaration produces a texture **object** parameter — not a sample node. To sample
it, call it from `Graph` through a `TextureSample` builtin, or declare a
[`TextureSampleParameter2D`](parameter-nodes.md) instead.

### Texture default grammar

```c
Texture2D A = Path(Game, "Textures/T_X");
Texture2D B = Path("/Game/Textures/T_X");
Texture2D C = "/Game/Textures/T_X";            // bare quoted absolute path (since 1.5.0)
```

The full grammar, every root spelling and both error sets are in [Path(…)](path.md). A failure is
wrapped as `Invalid texture default value '{Text}' for property '{Name}'. {Inner}`.

### Defaults when no asset is assigned

| Declared texture type | Fallback asset loaded |
| :-- | :-- |
| `Texture2D` | `/Engine/EngineResources/DefaultTexture.DefaultTexture` |
| `TextureCube` | `/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube` |
| `VolumeTexture` | `/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture` |
| `Texture2DArray` | **none exists** |

> [!WARNING]
> `Texture2DArray` has no engine fallback. A `Texture2DArray` (or `const Texture2DArray`) declared
> without `= Path(…)` fails generation with
> `Texture property '{Name}' with type Texture2DArray requires an explicit default asset.`
> Assign an explicit array asset.

### Dimension validation

The assigned asset is checked against the declared dimension, because all five compact tokens set an
explicit dimension. `Texture2D` here means "not a cube, not a 2D array, not a volume". A mismatch is
reported as

```text
{Context} texture property '{Name}' expects {ExpectedType} but '{Path}' is a '{ActualClass}'.
```

with `{Context}` being `Texture` for a parameter and `Const` for a `const` declaration, and `{Path}`
being the declared path or the literal `<default>`. After a successful load, `AutoSetSampleType()`
runs, so `SamplerType` follows the asset unless metadata overrides it — see
[SamplerType](sampler-type.md).

> [!NOTE]
> This validation applies to the compact tokens and to `TextureObjectParameter` only. The
> `TextureSampleParameter*`, `TextureCollectionParameter` and `SparseVolumeTextureObjectParameter`
> tokens are **never** dimension-checked. See
> [Parameter node tokens](parameter-nodes.md#dimension-validation-asymmetry).

## Tokens that are not valid in `Properties`

These are real DreamShaderLang type tokens in other contexts. In a `Properties` declaration each of
them falls through to `Unsupported property type '{Token}'.`

| Token | Valid where instead |
| :-- | :-- |
| `MaterialAttributes` | `Inputs` / `Outputs` / `Results` of a material function; `Shader` `Outputs` declarations; `Function` signatures |
| `Substrate` | same as above, and only on UE 5.4 or newer |
| `SamplerState` | `Inputs` / `Outputs` / function signatures, where it is an alias for `Texture2D` |
| `StaticBool` | `Inputs` / `Outputs` of a material function (use `StaticBoolParameter` in `Properties`) |
| `mat2` | a `Function` signature or `Code` body only, where the parser normalizes it to `float2x2`; generation then rejects the matrix type |
| `mat3` | same, normalized to `float3x3` |
| `mat4` | same, normalized to `float4x4` |

The `Inputs` / `Outputs` token set is documented in
[Inputs / Outputs / Results](../language/inputs-outputs.md) and the full cross-context matrix in
[Type tokens](../language/types.md).

## Notes

- `vec*`, `ivec*`, `uvec*` and `bvec*` are GLSL-flavoured spellings only; they behave exactly like
  `float*`, `int*`, `uint*` and `bool*` respectively.
- There is no compact token for a 1-component *vector*: `float1` and `half1` are scalar tokens.
- A compact texture property reports **0** components to the `Graph`, and reads output 0 of its node.
- A `const` vector with a component count other than 2 or 3 uses the `Constant4Vector` branch; since
  every accepted vector token is 2, 3 or 4 components, that arm is only ever reached by 4.
- `const` properties still accept a metadata block; `[Desc="…"]` on a `Constant` node works.
- A `const` **vector** read from `Graph` always targets output 0 of the constant node, not the
  named `R` / `RG` / `RGB` component output a parameter read would use. See
  [Using parameters in Graph](graph-usage.md#value-reads).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Unsupported property type '{Token}'.` | the token is not one of the 39 compact tokens, not a `*Parameter` token, and does not start with `UE.` |
| `Invalid scalar default value '{Text}' for property '{Name}'.` | a scalar default that is neither a number nor `true` / `false` |
| `Invalid vector default value '{Text}' for property '{Name}'.` | no parenthesised part list, or a part that is neither a number nor `true` / `false` |
| `Invalid texture default value '{Text}' for property '{Name}'. {Inner}` | the texture default failed `Path(…)` resolution |
| `Failed to create a scalar parameter node for property '{Name}'.` | node construction failed |
| `Failed to create a vector parameter node for property '{Name}'.` | node construction failed |
| `Failed to create a texture parameter node for property '{Name}'.` | node construction failed |
| `Failed to create a texture object node for const property '{Name}'.` | `const` texture node construction failed |
| `Failed to create a const node for property '{Name}'.` | `const` scalar/vector node construction failed |
| `Texture property '{Name}' could not load asset '{Path}'.` | the assigned asset does not load |
| `Const texture property '{Name}' could not load asset '{Path}'.` | as above, on a `const` declaration |
| `Texture property '{Name}' could not load default {Type} asset '{Path}'.` | the engine fallback asset for the dimension does not load |
| `Const texture property '{Name}' could not load default {Type} asset '{Path}'.` | as above, on a `const` declaration |
| `Texture property '{Name}' with type Texture2DArray requires an explicit default asset.` | a `Texture2DArray` with no `= Path(…)` |
| `Const texture property '{Name}' with type Texture2DArray requires an explicit default asset.` | as above, on a `const` declaration |
| `Texture texture property '{Name}' expects {Expected} but '{Path}' is a '{Class}'.` | the assigned asset's class does not match the declared dimension |
| `Const texture property '{Name}' expects {Expected} but '{Path}' is a '{Class}'.` | as above, on a `const` declaration |
| `Const property '{Name}' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration.` | `const` applied to a `*Parameter` token or a `UE.*` declaration |

## Example

```c
Shader(Name="Docs/M_CompactTypes")
{
    Properties = {
        const float Gamma = 2.2;                       // Constant

        float  Strength = 1.0;                         // ScalarParameter, reads R
        float2 Tiling   = float2(4.0, 4.0);            // VectorParameter, reads RG
        vec3   Tint     = vec3(1.0, 0.5, 0.2);         // VectorParameter, reads RGB
        vec4   Overlay  = vec4(0.0, 0.0, 0.0, 1.0);    // VectorParameter, reads RGBA

        // A texture *object* parameter: it carries the asset, it does not sample it.
        Texture2D BaseTex = Path(Game, "Textures/T_White");
    }

    Settings = { Domain = "Surface"; ShadingModel = "Unlit"; BlendMode = "Opaque"; }
    Outputs  = { vec3 Color; Base.EmissiveColor = Color; }

    Graph = {
        vec2 UV = UE.TexCoord(Index = 0) * Tiling;
        Color = Tint * Strength * Gamma + Overlay.rgb + vec3(UV.x, UV.y, 0.0);
    }
}
```

Generated nodes:

```text
Constant(2.2)                        Gamma
ScalarParameter                      Strength    DefaultValue = 1.0
VectorParameter                      Tiling      DefaultValue = (4, 4, 0, 0)     read through RG
VectorParameter                      Tint        DefaultValue = (1, 0.5, 0.2, 1) read through RGB
VectorParameter                      Overlay     DefaultValue = (0, 0, 0, 1)     read through RGBA
TextureCoordinate                    UV          CoordinateIndex = 0
```

`BaseTex` generates nothing here: an unreferenced property node is skipped. A texture object
parameter has to be consumed by something — a sample node, or a `Texture2D` input of a
[`ShaderFunction`](../language/shader-function.md). To declare a texture that samples itself, use
[`TextureSampleParameter2D`](parameter-nodes.md).

## See also

- [Parameters](index.md) — the hub and the decision table
- [Parameter node tokens](parameter-nodes.md) — the 22 explicit `*Parameter` tokens
- [Metadata block](metadata.md) — `[ … ]` entries, `Slider(min, max)`, reflected properties
- [Path(…)](path.md) — texture default asset references
- [SamplerType](sampler-type.md) — sampler type inference and override
- [Using parameters in Graph](graph-usage.md) — which node output a declared component count reads
- [Properties (section)](../language/properties.md) — the enclosing section grammar
- [Type tokens](../language/types.md) — the full cross-context token matrix
- [Constructors](../graph/constructors.md) — the `Graph`-side constructor forms these literals resemble
- [Conversions](../graph/conversions.md) — component-count rules in `Graph`
