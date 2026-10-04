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
A compact property is a `uniform` of the 2.0 type of that name (`vec3` is `float3`), or a
`static const` with `const`.

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

`int1`, `uint1` and `bool1` are scalar tokens too *(since 2.0.0)*: the token is read as the 2.0 type
of that name, and HLSL spells a scalar that way.

> [!NOTE]
> `int`, `uint`, `bool` and `half` carry **no** integer, boolean or precision semantics at the node
> level. The generated node is a float `ScalarParameter` in every case, and the material graph performs
> no truncation. The only place an integer marker exists at all is a direct integer *constructor* call
> in `Graph` — see [Conversions](../graph/conversions.md#float-int-and-bool).

### Scalar default grammar

```c
float A = 0.5;      float B = -2;      float C = 2.5f;      float D = true;
```

The value is a number — an optional sign, digits with at most one `.`, an optional trailing `f` — or
`true` / `false` (any case), which are `1.0` and `0.0`. Anything else is
[`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254), and that includes an exponent: `float C = 1e3;` is
refused *(since 2.0.0)*. A number glued to letters, such as `1abc`, is not even one token: it is
[`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) *(since 2.0.0; 1.x read it as `1.0`)*.

With no `= <default>` the default is `0`.

## Vector tokens

27 spellings. Every one produces a `UMaterialExpressionVectorParameter` whose `DefaultValue` is a full
`FLinearColor`; only the declared component count differs, and it is what decides which node output a
`Graph` read targets.

The count is the digit the token ends in.

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
| Parts | split on every `,`; empty parts are dropped; each part is a number (sign, digits, one `.`, an exponent, a trailing `f`), or `true` / `false` (case-insensitive) |
| 1 part `a` | `(a, a, a, 1)` |
| 2 parts `a, b` | `(a, b, 0, 0)` |
| 3 parts `a, b, c` | `(a, b, c, 1)` |
| 4 parts `a, b, c, d` | `(a, b, c, d)` |
| More than 4 parts | parts 5 and beyond are **never read** — not even parsed, so an unparsable part there is not an error |
| No `( … )`, or a part unparsable | [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) |

The declaration keeps the first *N* of those four values, *N* being its component count, and the
parameter node's `DefaultValue` is those *N* with the missing channels `0` and alpha `1`:
`float2 T = float2(4, 4)` is `(4, 4, 0, 1)` *(since 2.0.0; 1.x wrote `(4, 4, 0, 0)`)*.

With no `= <default>` the default is `(0, 0, 0, 1)` for a 2- or 3-component token and `(0, 0, 0, 0)`
for a 4-component one *(since 2.0.0)*.

> [!WARNING]
> **The declared component count is never checked against the literal's arity.** `float2 P = float4(1, 2, 3, 4);`
> and `float4 P = vec2(1, 2);` both parse, with no diagnostic. The literal fills four values by the
> table above and the declared count keeps the leading ones — so `float4 P = vec2(1, 2)` yields
> `(1, 2, 0, 0)` read as RGBA, and `float2 P = float4(1, 2, 3, 4)` yields `(1, 2)`. Write the literal
> with the same arity as the declared token.

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

The full grammar and every root spelling are in [Path(…)](path.md). The default is resolved when the
material is built: a reference that does not resolve is
[`DSH8271`](../diagnostics/DSH8xxx.md#dsh8271), whose message quotes the resolver's own code, and one
that resolves to nothing loadable is [`DSH8218`](../diagnostics/DSH8xxx.md#dsh8218). A `const`
texture's default is written to its `TextureObject` node's `Texture` property instead, where an asset
that does not load is written as nothing, with no diagnostic — see
[Metadata](metadata.md#object-properties).

### Defaults when no asset is assigned

| Declared texture type | Fallback asset |
| :-- | :-- |
| `Texture2D` | the node's own default, `/Engine/EngineResources/DefaultTexture` |
| `TextureCube` | `/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube` |
| `VolumeTexture` | `/Engine/EngineResources/DefaultVolumeTexture.DefaultVolumeTexture` |
| `Texture2DArray` | **none exists** |

> [!WARNING]
> `Texture2DArray` has no engine fallback. A `Texture2DArray` declared without `= Path(…)` keeps the
> node's 2D default texture, which is not an array, and the compiler says nothing *(since 2.0.0; 1.x
> refused the declaration)*. Assign an explicit array asset.

### Dimension validation

*(since 2.0.0)* The asset a default loads is **not** checked against the declared dimension. What is
checked, before anything loads, is the class a Content Browser reference names in its `Class'…'`
shell: a texture class of another dimension is [`DSH1044`](../diagnostics/DSH1xxx.md#dsh1044), and a
class DreamShader knows is not a texture (`MaterialFunction`, `CurveLinearColor`, `Font`, …) is
[`DSH1043`](../diagnostics/DSH1xxx.md#dsh1043). See [Path(…)](path.md#class-checking).

After the asset is assigned, `AutoSetSampleType()` runs unless the declaration says `SamplerType`, so
the sampler type follows the asset — see [SamplerType](sampler-type.md).

> [!NOTE]
> The texture-sample tokens get the same shell check, and `TextureObjectParameter` only its
> "not a texture" half. See
> [Parameter node tokens](parameter-nodes.md#dimension-validation-asymmetry).

## Tokens that are not valid in `Properties`

These are real DreamShaderLang type tokens in other contexts. In a `Properties` declaration each of
them is [`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252), except `StaticBool`.

| Token | Valid where instead |
| :-- | :-- |
| `MaterialAttributes` | `Inputs` / `Outputs` / `Results` of a material function; `Shader` `Outputs` declarations; `Function` signatures |
| `Substrate` | same as above, and only on UE 5.4 or newer |
| `SamplerState` | `Inputs` / `Outputs` / function signatures, where it is an alias for `Texture2D` |
| `StaticBool` | `Inputs` / `Outputs` of a material function. In `Properties` it is accepted *(since 2.0.0)* and read as `bool`, which makes a `ScalarParameter`, not a static bool: write `StaticBoolParameter` |
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
- A compact texture property is a texture value in `Graph`, read from output 0 of its node.
- A `const` vector with a component count other than 2 or 3 uses the `Constant4Vector` branch; since
  every accepted vector token is 2, 3 or 4 components, that arm is only ever reached by 4.
- A `const` property keeps only what a constant node has *(since 2.0.0)*. `Description` and
  `SamplerType` reach a `const` texture's `TextureObject` node. On a `const` scalar or vector the
  description is dropped, and `Group`, `SortPriority` and `Slider` — written, or inherited from a
  `Group(…)` scope — are [`DSH7224`](../diagnostics/DSH7xxx.md#dsh7224) warnings.
- A `const` **vector** read from `Graph` is the whole value of its constant node, not a named
  `R` / `RG` / `RGB` component output. See [Using parameters in Graph](graph-usage.md#value-reads).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH3252` | the token is not one of the compact tokens, not a `*Parameter` token, and does not start with `UE.` |
| `DSH3254` | a scalar default that is neither a number nor `true` / `false`; a vector default with no parenthesised part list, or with a part that is neither |
| `DSH2105` | a number glued to letters (`1abc`) |
| `DSH1043` / `DSH1044` | a `Class'…'` default names a class that is not a texture / a texture of another dimension |
| `DSH8271` | a texture default does not resolve to an object path |
| `DSH8218` | a texture default resolves and does not load |
| [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) | `const` on a `*Parameter` token that cannot be a constant |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the emitter could not create the node (internal) |

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
VectorParameter                      Tiling      DefaultValue = (4, 4, 0, 1)     read through RG
VectorParameter                      Tint        DefaultValue = (1, 0.5, 0.2, 1) read through RGB
VectorParameter                      Overlay     DefaultValue = (0, 0, 0, 1)     read through RGBA
TextureCoordinate                    UV          CoordinateIndex = 0
```

`BaseTex` generates nothing here: a property nothing reads makes no node, and the build says so
([`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390), info). A texture object parameter has to be consumed
by something — a sample node, or a `Texture2D` input of a
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
