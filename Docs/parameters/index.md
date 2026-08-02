# Parameters

> [DreamShader](../index.md) » **Parameters**

A parameter is a `Properties` declaration that becomes one named `UMaterialExpression` node in the
generated asset — a material parameter an instance can override, a constant, or a builtin input node.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — the `Properties` section of a `Shader`, `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` |
| Kind | parameter type catalogue |
| Generates | exactly one node per declaration, positioned at X = -800 (a `const` scalar or vector literal node at X = -1120) with a Y stride of 220 |
| Since | `1.2.3` (explicit `*Parameter` tokens), `1.2.6` (`const`), `1.5.0` (`Group(…) { … }`, `Slider(…)`) |

## Synopsis

```c
[const] <type-token> <name> [ = <default-value> ] [ [ <metadata-entry> { ; | , } … ] ] ;
```

The outer `[ … ]` of the metadata block is **literal DreamShaderLang punctuation**; the `[ … ]` around
`= <default-value>` and around the whole metadata block is the "optional" meta-bracket. In other
words, a declaration with metadata is written `float A = 1.0 [Group="X"];`.

The enclosing section grammar — where `Properties` may appear, the optional `=` before `{`, repetition,
`Group("Name") { … }` scopes and the ordering rules — is specified in
[Properties (section)](../language/properties.md). This section documents only what may stand in the
`<type-token>`, `<default-value>` and `<metadata-entry>` slots.

## The four kinds of type token

| Kind | Count | Example | Node generated | Reference |
| :-- | :-- | :-- | :-- | :-- |
| Compact scalar | 7 | `float Strength = 1.0;` | `UMaterialExpressionScalarParameter` | [Compact type tokens](compact-types.md#scalar-tokens) |
| Compact vector | 27 | `vec3 Tint = vec3(1, 1, 1);` | `UMaterialExpressionVectorParameter` | [Compact type tokens](compact-types.md#vector-tokens) |
| Compact texture | 5 | `Texture2D Base = Path(Game, "T_X");` | `UMaterialExpressionTextureObjectParameter` | [Compact type tokens](compact-types.md#texture-tokens) |
| Explicit `*Parameter` node | 22 | `TextureSampleParameter2D Tex;` | the named `UMaterialExpression` subclass | [Parameter node tokens](parameter-nodes.md) |
| `UE.<Name>` builtin | — | `UE.TexCoord(Index = 0) UV;` | the builtin's node, or a reflected class | [UE builtins](../builtins/ue.md) |

Every token is matched **case-insensitively**. A token that matches none of the above fails with
`Unsupported property type '{Token}'.`

## Which form to use

| Goal | Write | Why |
| :-- | :-- | :-- |
| A float the artist can tweak on an instance | `float`, `half`, `int`, `uint`, `bool` (or `ScalarParameter`) | All seven compact scalar tokens and `ScalarParameter` produce the identical `ScalarParameter` node |
| A colour or 2/3/4-component parameter | `float2` … `bvec4` | The declared component count controls which output the `Graph` reads (`R` / `RG` / `RGB` / `RGBA`) |
| A four-component parameter that must always read as RGBA | `VectorParameter` | Fixed at 4 components regardless of how it is used |
| A texture the shader samples itself | `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | Produces a texture *object* parameter, dimension-checked against the asset |
| A texture with a sampler node and configurable sampling | `TextureSampleParameter2D` and friends | Owns `Coordinates` and mip pins; accepts `SamplerType` / `MipValueMode` metadata |
| A texture object with no fixed dimension | `TextureObjectParameter` | Takes its dimension from the assigned default asset *(unreleased)* |
| A compile-time branch | `StaticSwitchParameter` | Must be used in the call form `N(True = …, False = …)` |
| A compile-time boolean with no branch | `StaticBoolParameter` | Value only |
| A baked value the artist must not change | `const <compact-token>` | Emits `Constant` / `Constant2Vector` / `Constant3Vector` / `Constant4Vector` / `TextureObject` — no parameter, no instance override |
| A node the engine feeds (UVs, time, camera, vertex colour, MPC) | `UE.<Name>( … )` | See [UE builtins](../builtins/ue.md) |
| Any other `UMaterialExpression` class | `UE.<Class>(OutputType = "float4", … )` | Generic reflected construction — see [UE.Expression](../builtins/ue-expression.md) |

> [!NOTE]
> `const` is legal **only** with the 39 compact tokens. `const` combined with any `*Parameter` token or
> any `UE.*` declaration is rejected at generation with
> `Const property '{Name}' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration.`

## Pages

| Page | Covers |
| :-- | :-- |
| [Compact type tokens](compact-types.md) | All 39 compact tokens, the node each generates, default-value grammar, and the tokens that are *not* valid in `Properties` |
| [Parameter node tokens](parameter-nodes.md) | All 22 explicit `*Parameter` tokens, their generated classes, per-type default and metadata slots |
| [Metadata block](metadata.md) | `[ Key = Value ; … ]`, `Slider(min, max)`, `ParameterName`, and the reflected-UPROPERTY passthrough |
| [SamplerType](sampler-type.md) | Every `SamplerType` value and spelling, `SamplerSource`, and how the texture dimension is validated or inferred |
| [Using parameters in Graph](graph-usage.md) | Value reads, component selection, and the pin call form — with the exhaustive pin table |
| [Path(…) asset references](path.md) | Every root spelling, the two resolvers, and both error sets |

## Notes

- **A default value is optional for every type.** With no `= <default>` the node keeps its engine
  default: `0.0` for a scalar parameter, `(1, 1, 1, 1)` for a vector parameter, an engine placeholder
  texture for a texture parameter. The one exception is a `Texture2DArray`-typed compact declaration,
  for which no engine fallback asset exists.
- **A property name is only required to be non-empty.** Unlike an
  [`Inputs` parameter](../language/inputs-outputs.md), a property name is never validated as an
  identifier, so `float 1Bad = 0;` parses. It is then unreachable from `Graph`, where names are looked
  up as identifiers.
- **Property names must be unique ignoring case** within a `Shader`; inside a material function a
  property may not collide with an input name either.
- **Declaration order does not constrain reads.** Nodes are created lazily on first reference, so a
  `Graph` may read a property declared later in the block. Only a `UE.*` argument that references
  another property requires that property to exist in the same `Properties` list.
- **`Properties` means something else inside a `VirtualFunction`**: there it is a synonym for `Inputs`
  and takes the typed-parameter grammar, not this one. See
  [VirtualFunction](../language/virtual-function.md).
- The parameter name written into the material is the declared name unless `[ParameterName="…"]`
  overrides it. See [Metadata](metadata.md#parametername).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this page.

| Message | Cause |
| :-- | :-- |
| `Invalid property declaration '{Statement}'.` | no top-level whitespace separating the type token from the name |
| `Missing property name in declaration '{Statement}'.` | the name token is empty |
| `Missing property type after const in declaration '{Statement}'.` | `const` with nothing after it |
| `Unsupported property type '{Token}'.` | the type token matches no compact token, no `*Parameter` token and does not start with `UE.` |
| `Metadata must follow a declaration.` | the statement is only a `[ … ]` block |
| `{File}: Property '{Name}' is declared more than once. Property names must be unique.` | two declarations whose names are equal ignoring case |
| `{Kind} '{Function}' property '{Name}' conflicts with another property or input name.` | in a material function, a property name collides with another property or an input |
| `Failed to create a parameter node for property '{Name}'.` | the parameter path returned no node and no more specific message |
| `Const property '{Name}' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration.` | `const` applied to a `*Parameter` token or a `UE.*` declaration |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_Parameters")
{
    Properties = {
        const float DebugScale = 1.0;                     // Constant, not overridable

        Group("Surface") {
            vec3  Tint      = vec3(1.0, 0.5, 0.2);        // VectorParameter, reads RGB
            float Roughness = 0.4 [Slider(0, 1)];         // ScalarParameter with a UI range
        }

        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_White") [
            SamplerType  = "LinearColor";
            MipValueMode = "None";
        ];

        StaticSwitchParameter UseDetail = true;

        UE.TexCoord(Index = 0) UV;                        // MaterialExpressionTextureCoordinate
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3  Color;
        float Rough;

        Base.BaseColor  = Color;
        Base.Roughness  = Rough;
    }

    Graph = {
        vec4 Sample = BaseTex(Coordinates = UV);
        vec3 Lit    = Sample.rgb * Tint * DebugScale;
        Color = UseDetail(True = Lit, False = Tint);
        Rough = Roughness;
    }
}
```

Generated nodes:

```text
Constant              DebugScale        (const -> not a parameter)
VectorParameter       Tint              Group="Surface" SortPriority=0
ScalarParameter       Roughness         Group="Surface" SortPriority=10 SliderMin=0 SliderMax=1
TextureSampleParameter2D BaseTex        SamplerType=LinearColor
StaticSwitchParameter UseDetail
TextureCoordinate     UV                CoordinateIndex=0
```

## See also

- [Properties (section)](../language/properties.md) — the section grammar, `const`, and `Group(…)` scopes
- [Type tokens](../language/types.md) — the per-context validity matrix for every type token
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — the *other* parameter grammar, used by functions
- [Shader](../language/shader.md) — the block a `Properties` section lives in
- [ShaderFunction](../language/shader-function.md) — function-local properties
- [UE builtins](../builtins/ue.md) — the complete `UE.*` catalogue
- [UE.Expression](../builtins/ue-expression.md) — generic reflected node construction
- [Graph](../graph/index.md) — the language that consumes these parameters
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
