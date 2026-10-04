# Parameters

> [DreamShader](../index.md) » **Parameters**

A parameter is a `Properties` declaration that becomes a named `UMaterialExpression` node in the
generated asset — a material parameter an instance can override, a constant, or a builtin input node.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — the `Properties` section of a `Shader`, `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` |
| Kind | parameter type catalogue |
| Generates | one node per property a body reads; a property nothing reads makes none ([`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390), info). A parameter node type that is expanded at its uses (see [Parameter node tokens](parameter-nodes.md#the-22-tokens)) makes one node per distinct use. Node positions come from the [graph layout](../generation/graph-layout.md). |
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

Since 2.0.0 a 1.x source is read by the legacy front end, which turns each property into what the
2.0 language writes for it: a `uniform`, a `static const`, or a parameter node built where it is read.
The `.dss` spelling is in [The 2.0 language](../language-v2/index.md); `dsc migrate` writes it for you
([Migrate](../tools/migrate.md)).

## The four kinds of type token

| Kind | Count | Example | Node generated | Reference |
| :-- | :-- | :-- | :-- | :-- |
| Compact scalar | 7 | `float Strength = 1.0;` | `UMaterialExpressionScalarParameter` | [Compact type tokens](compact-types.md#scalar-tokens) |
| Compact vector | 27 | `vec3 Tint = vec3(1, 1, 1);` | `UMaterialExpressionVectorParameter` | [Compact type tokens](compact-types.md#vector-tokens) |
| Compact texture | 5 | `Texture2D Base = Path(Game, "T_X");` | `UMaterialExpressionTextureObjectParameter` | [Compact type tokens](compact-types.md#texture-tokens) |
| Explicit `*Parameter` node | 22, of which 15 build | `TextureSampleParameter2D Tex;` | the named `UMaterialExpression` subclass | [Parameter node tokens](parameter-nodes.md) |
| `UE.<Name>` builtin | — | `UE.TexCoord(Index = 0) UV;` | the builtin's node, or a reflected class | [UE builtins](../builtins/ue.md) |

Every token is matched **case-insensitively**. A token that matches none of the above is
[`DSH3252`](../diagnostics/DSH3xxx.md#dsh3252); one of the seven parameter node types that have no
2.0 spelling is [`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253) *(since 2.0.0)*.

## Which form to use

| Goal | Write | Why |
| :-- | :-- | :-- |
| A float the artist can tweak on an instance | `float`, `half`, `int`, `uint`, `bool` (or `ScalarParameter`) | All seven compact scalar tokens and `ScalarParameter` produce the identical `ScalarParameter` node |
| A colour or 2/3/4-component parameter | `float2` … `bvec4` | The declared component count controls which output the `Graph` reads (`RG` / `RGB` / `RGBA`) |
| A four-component parameter that must always read as RGBA | `VectorParameter` | Fixed at 4 components regardless of how it is used |
| A texture the shader samples itself | `Texture2D` / `TextureCube` / `Texture2DArray` / `Texture3D` / `VolumeTexture` | Produces a texture *object* parameter of that dimension |
| A texture with a sampler node and configurable sampling | `TextureSampleParameter2D` and friends | Owns `Coordinates` and mip pins; accepts `SamplerType` / `MipValueMode` metadata |
| A texture object with no fixed dimension | `TextureObjectParameter` | Takes its dimension from the assigned default asset *(since 1.6.0)* |
| A compile-time branch | `StaticSwitchParameter` | Must be used in the call form `N(True = …, False = …)` |
| A compile-time boolean with no branch | `StaticBoolParameter` | Value only |
| A baked value the artist must not change | `const <compact-token>` | Emits `Constant` / `Constant2Vector` / `Constant3Vector` / `Constant4Vector` / `TextureObject` — no parameter, no instance override |
| A node the engine feeds (UVs, time, camera, vertex colour, MPC) | `UE.<Name>( … )` | See [UE builtins](../builtins/ue.md) |
| Any other `UMaterialExpression` class | `UE.<Class>(OutputType = "float4", … )` | Generic reflected construction — see [UE.Expression](../builtins/ue-expression.md) |

> [!NOTE]
> `const` is legal with the 39 compact tokens and, *(since 2.0.0)*, with `ScalarParameter`,
> `VectorParameter` and `TextureObjectParameter`, which then build the same constant as `const float`,
> `const float4` and `const Texture2D`. On any other `*Parameter` token it is `DSH3253`. On a `UE.*`
> declaration the `const` is ignored *(since 2.0.0)*.

## Pages

| Page | Covers |
| :-- | :-- |
| [Compact type tokens](compact-types.md) | All 39 compact tokens, the node each generates, default-value grammar, and the tokens that are *not* valid in `Properties` |
| [Parameter node tokens](parameter-nodes.md) | All 22 explicit `*Parameter` tokens, their generated classes, per-type default and metadata slots |
| [Metadata block](metadata.md) | `[ Key = Value ; … ]`, `Slider(min, max)`, `ParameterName`, and the reflected-UPROPERTY passthrough |
| [SamplerType](sampler-type.md) | Every `SamplerType` value and spelling, `SamplerSource`, and how the texture dimension is checked or inferred |
| [Using parameters in Graph](graph-usage.md) | Value reads, component selection, and the pin call form — with the pin table |
| [Path(…) asset references](path.md) | Every root spelling, the resolver, and its diagnostics |

## Notes

- **A default value is optional for every type.** With no `= <default>`, a scalar is `0`, a vector is
  `0` in every channel (alpha `1` for a 2- or 3-component token), and a texture is the engine's default
  texture of its dimension *(since 2.0.0; 1.x left a vector at the node's own default)*. A
  `Texture2DArray` has no engine default: its node keeps the class's 2D default texture, with no
  diagnostic *(since 2.0.0)*. Assign an array asset.
- **A property name is an identifier** *(since 2.0.0)*. `float 1Bad = 0;` is
  [`DSH2105`](../diagnostics/DSH2xxx.md#dsh2105) (a malformed number) and
  [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250).
- **A property that becomes a declaration is declared at file scope** *(since 2.0.0)*. Two properties
  of one name — in one block, or in two blocks of the same file — are
  [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210). Names are compared exactly. The 1.x rule that a
  function's property may not share a name with one of its inputs has no 2.0 check.
- **Declaration order does not constrain reads.** A `Graph` may read a property declared further down
  the block. The exception is a `UE.*` property: it is a local declared at the head of each body that
  reads it, in `Properties` order, so an argument naming another `UE.*` property needs that one earlier
  in the list and read by the same body; otherwise the name is not declared
  ([`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)).
- **`Properties` means something else inside a `VirtualFunction`**: there it is a synonym for `Inputs`
  and takes the typed-parameter grammar, not this one. See
  [VirtualFunction](../language/virtual-function.md).
- The parameter name written into the material is the declared name unless `[ParameterName="…"]`
  overrides it. See [Metadata](metadata.md#parametername).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250) | the statement does not start with a type and a name, the name is missing, or no `;` follows the property; a statement that is only a `[ … ]` block lands here too |
| [`DSH3251`](../diagnostics/DSH3xxx.md#dsh3251) | `const` with no type after it |
| `DSH3252` | the type token is no compact token, no `*Parameter` token and does not start with `UE.` |
| `DSH3253` | a `*Parameter` token with no 2.0 spelling, or `const` on a `*Parameter` token that cannot be a constant |
| [`DSH3254`](../diagnostics/DSH3xxx.md#dsh3254) | `=` with no default after it, or a default the token cannot take |
| [`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) | a `UE.*` declaration with an unclosed `(`, or a default after its name |
| [`DSH3260`](../diagnostics/DSH3xxx.md#dsh3260) | `Group("") { … }` |
| `DSH4210` | two properties of one name in one file |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the emitter could not create a parameter node (internal) |

Metadata, default-value and asset-reference diagnostics are on [Metadata](metadata.md#diagnostics),
[Compact type tokens](compact-types.md#diagnostics) and [Path(…)](path.md#diagnostics). The complete
list lives in the [diagnostics index](../diagnostics/index.md).

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
- [The 2.0 language](../language-v2/index.md) — `uniform`, `static const` and `///` directives
- [Diagnostics index](../diagnostics/index.md) — every code
