# OutputType

> [DreamShader](../index.md) » [Builtins](index.md) » **OutputType**

The argument that tells a reflected builtin call what kind of value it produces, and the only place a
type token appears inside an argument list.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — as an argument of a generic `UE.*` call in `Graph`, or of a `UE.*` property declaration |
| Kind | builtin argument |
| Aliases | `ResultType` |

## Synopsis

```c
{ OutputType | ResultType } = <type-token>
```

`OutputType` is looked up first; `ResultType` is consulted only when `OutputType` is absent. When both
are written, `OutputType` wins and `ResultType` is ignored — on the `Graph` surface both are reserved
names and are never dispatched as properties, and on the declaration surface both are on the
accepted-argument list of `UE.CollectionParam` / `UE.CollectionParameter` and of every generic
(reflected) declaration. The other builtins in the
[declaration-form table](ue.md#properties-declaration-form) do **not** accept either name: writing one
there fails with `UE.{Name} for property '{Property}' does not support argument '{Argument}'.`

The token may be quoted or bare: `OutputType = "float3"` and `OutputType = float3` are identical.

## Where it is read

| Surface | Required | Token set | Effect |
| :-- | :-- | :-- | :-- |
| Generic `UE.Expression(…)` / `UE.<ClassName>(…)` in `Graph` | **yes** | full | decides which kinds of value are legal; supplies the component count unless the node's own kind or class overrides it (see [Notes](#notes)) |
| `UE.Expression(Class = "Custom", …)` in `Graph` | **yes** | reduced — numeric families and `MaterialAttributes` only | **authoritative** — written to the Custom node, decides the HLSL return type |
| `UE.<Name>(…)` property declaration | only when the name is outside the [declaration-form table](ue.md#declared-output-width) | reduced — no `MaterialAttributes`, `Substrate`, `SamplerState`, `StaticBool` or `StaticBoolParameter` | declares the property's type and component count |
| Registered `UE.*` sugar in `Graph` | never read | — | silently discarded like any other unknown argument |
| `Substrate.*` in `Graph` | never required | — | **ignored** — the output type is synthesized from the node descriptor |

For a `Substrate.*` call the synthesized type is `Substrate` with 0 components for a BSDF or operator
node, and `auto` with 1 component for a utility node; supplying `OutputType` changes nothing. See
[Substrate](substrate.md).

## Accepted tokens

Every token, on every surface. `✔` = accepted, `✘` = rejected.

| Token | Kind | Components | Generic `UE.*` | Custom node | Property declaration |
| :-- | :-- | :-- | :-: | :-: | :-: |
| `float` | numeric | 1 | ✔ | ✔ | ✔ |
| `float1` | numeric | 1 | ✔ | ✔ | ✔ |
| `half` | numeric | 1 | ✔ | ✔ | ✔ |
| `half1` | numeric | 1 | ✔ | ✔ | ✔ |
| `int` | numeric | 1 | ✔ | ✔ | ✔ |
| `uint` | numeric | 1 | ✔ | ✔ | ✔ |
| `bool` | numeric | 1 | ✔ | ✔ | ✔ |
| `float2` | numeric | 2 | ✔ | ✔ | ✔ |
| `half2` | numeric | 2 | ✔ | ✔ | ✔ |
| `vec2` | numeric | 2 | ✔ | ✔ | ✔ |
| `int2` | numeric | 2 | ✔ | ✔ | ✔ |
| `uint2` | numeric | 2 | ✔ | ✔ | ✔ |
| `bool2` | numeric | 2 | ✔ | ✔ | ✔ |
| `ivec2` | numeric | 2 | ✔ | ✔ | ✔ |
| `uvec2` | numeric | 2 | ✔ | ✔ | ✔ |
| `bvec2` | numeric | 2 | ✔ | ✔ | ✔ |
| `float3` | numeric | 3 | ✔ | ✔ | ✔ |
| `half3` | numeric | 3 | ✔ | ✔ | ✔ |
| `vec3` | numeric | 3 | ✔ | ✔ | ✔ |
| `int3` | numeric | 3 | ✔ | ✔ | ✔ |
| `uint3` | numeric | 3 | ✔ | ✔ | ✔ |
| `bool3` | numeric | 3 | ✔ | ✔ | ✔ |
| `ivec3` | numeric | 3 | ✔ | ✔ | ✔ |
| `uvec3` | numeric | 3 | ✔ | ✔ | ✔ |
| `bvec3` | numeric | 3 | ✔ | ✔ | ✔ |
| `float4` | numeric | 4 | ✔ | ✔ | ✔ |
| `half4` | numeric | 4 | ✔ | ✔ | ✔ |
| `vec4` | numeric | 4 | ✔ | ✔ | ✔ |
| `int4` | numeric | 4 | ✔ | ✔ | ✔ |
| `uint4` | numeric | 4 | ✔ | ✔ | ✔ |
| `bool4` | numeric | 4 | ✔ | ✔ | ✔ |
| `ivec4` | numeric | 4 | ✔ | ✔ | ✔ |
| `uvec4` | numeric | 4 | ✔ | ✔ | ✔ |
| `bvec4` | numeric | 4 | ✔ | ✔ | ✔ |
| `MaterialAttributes` | material attributes | 0 | ✔ | ✔ | ✘ |
| `Substrate` | Substrate | 0 | ✔ *(UE 5.4)* | ✘ | ✘ |
| `StaticBool` | numeric | 1 | ✔ | ✘ | ✘ |
| `StaticBoolParameter` | numeric | 1 | ✔ | ✘ | ✘ |
| `Texture2D` | texture object — 2D | 0 | ✔ | ✘ | ✔ † |
| `SamplerState` | texture object — 2D | 0 | ✔ | ✘ | ✘ |
| `TextureCube` | texture object — cube | 0 | ✔ | ✘ | ✔ † |
| `Texture2DArray` | texture object — 2D array | 0 | ✔ | ✘ | ✔ † |
| `Texture3D` | texture object — volume | 0 | ✔ | ✘ | ✔ † |
| `VolumeTexture` | texture object — volume | 0 | ✔ | ✘ | ✔ † |

**44 tokens.** All matching is case-insensitive.

† On the property-declaration surface all five texture tokens resolve to the **same** declared kind —
a texture-object property with 0 components. The dimension is not recorded there, unlike the `Graph`
surface where `TextureCube` and `Texture2DArray` produce distinctly typed values. Declare the
dimension with a plain type token (`TextureCube Tex;`) when it matters; see
[Types](../language/types.md).

`SamplerState` is an accepted spelling of `Texture2D` on the `Graph` surface only.

`StaticBool` and `StaticBoolParameter` both resolve to a one-component value. They exist so that a
value can be type-checked against a `StaticBool` function input; nothing else distinguishes them from
`float`.

## Normalization

The token is not normalized uniformly — three different comparisons are used, and the difference is
observable when the token carries stray whitespace.

| Token group | Comparison |
| :-- | :-- |
| The 34 numeric tokens and `MaterialAttributes` | trimmed, lower-cased, and **all** inner spaces removed |
| `Substrate` | trimmed and all inner spaces removed, compared ignoring case |
| `StaticBool`, `StaticBoolParameter`, `Texture2D`, `SamplerState`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture` | compared ignoring case against the value **as written** — no trimming, no space removal |

> [!NOTE]
> On the `Graph` surface, `OutputType = " float4 "` resolves and `OutputType = " Texture2D "` does
> not, because only the numeric families are trimmed. This is only reachable through a quoted literal
> — a bare token cannot contain whitespace. On the property-declaration surface the value is trimmed
> before resolution, so the asymmetry does not arise there.

Underscores and dashes are never removed from a type token. `float_4` and `Material-Attributes` are
not accepted spellings.

## Output mask pseudo-names

Distinct from `OutputType`: the `Output` / `OutputName` argument selects **which** output pin of a
multi-output node is read. Named outputs are matched by name, case-insensitively. Outputs with no
name are additionally matched by their channel mask against these seven pseudo-names, tested in this
order:

| Pseudo-name | Matches an unnamed output whose mask is |
| :-- | :-- |
| `RG` | R and G |
| `RGB` | R, G and B |
| `RGBA` | R, G, B and A |
| `R` | R only |
| `G` | G only |
| `B` | B only |
| `A` | A only |

An empty or omitted selector reads output 0. The selector text is trimmed. `Output` and `OutputIndex`
are mutually exclusive. Full rules: [`UE.Expression`](ue-expression.md#selecting-an-output).

> [!NOTE]
> These are not `OutputType` values, and an `OutputType` token is never a valid `Output` selector.
> `UE.Expression(Class = "BreakMaterialAttributes", OutputType = "float3", Output = "BaseColor")`
> selects by output *name*; `Output = "RGB"` selects by mask.

## Notes

- **The hint in the diagnostic is not the accepted set.** The message emitted when `OutputType` is
  missing lists nine spellings; the table above has 44. The message text is not a specification.
- On the generic `Graph` path the declared token controls which *kinds* of value are legal — a
  Substrate declaration on a non-Substrate node is an error. The resulting **component count** comes
  from the node only for a Substrate, `MaterialAttributes` or texture output and for the classes in
  the generator's [known-width table](ue-expression.md#result-type-and-component-count):
  `UE.Expression(Class = "WorldPosition", OutputType = "float1")` yields a 3-component value. For any
  other class the token *is* the width. On `UMaterialExpressionCustom` the token is additionally
  written to the node, so it also decides the HLSL return type.
- The token also participates in the node reuse key, in its normalized form. Two calls differing only
  in `OutputType` spelling — `float3` versus `vec3` — build different keys and therefore produce two
  nodes even though the resulting value is identical. See [Node reuse](../graph/node-reuse.md).
- This token set is close to, but not identical with, the language's declaration type tokens. The
  `Properties` section additionally accepts the 22 parameter-node tokens and the plain `Texture*`
  family with dimensions preserved; function `Inputs` additionally accept `Substrate` and
  `MaterialAttributes`. See [Types](../language/types.md),
  [Compact types](../parameters/compact-types.md) and
  [Inputs / Outputs / Results](../language/inputs-outputs.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}`; the compiler emits the substituted text.

| Message | Cause |
| :-- | :-- |
| `Unsupported UE builtin call '{Function}' in Graph. For generic MaterialExpression calls, add OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture/Substrate".` | neither `OutputType` nor `ResultType` on a generic `Graph` call |
| `UE.{Function} OutputType must be a literal value.` | the value is an expression rather than a literal |
| `UE.{Function} OutputType '{Token}' is not supported.` | the token is not in the table above |
| `UE.{Function} OutputType="Substrate" requires Unreal Engine 5.4 or newer.` | `Substrate` on UE 5.3 |
| `UE.{Function} OutputType="Substrate" is not supported by UMaterialExpressionCustom.` | `Substrate` on a Custom node |
| `UE.{Function} OutputType '{Token}' is not a valid Custom node output type.` | a texture or static-bool token on a Custom node |
| `Unsupported UE builtin function '{Function}'. Use OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture" for generic MaterialExpression calls.` | a property declaration whose name is outside the parser's table and which supplied no accepted `OutputType` |
| `This builtin is not implemented by the material generator yet. For generic MaterialExpression support, add OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture".` | the same, reported at generation time |
| `{Namespace}.{Function} output is not a Substrate value.` | `OutputType="Substrate"` on a node whose real output is not Substrate |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_OutputTypes")
{
    Properties {
        // Declaration surface: the reduced token set.
        UE.Expression(Class = "ObjectRadius", OutputType = "float1") Radius;
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // Advisory: the node reports float2 regardless of what is declared here.
        float2 uv = UE.Expression(Class = "TextureCoordinate", OutputType = "float2");

        // ResultType is the alias.
        float t = UE.Expression(Class = "Time", ResultType = "float1");

        // Authoritative on a Custom node.
        float3 tinted = UE.Expression(Class = "Custom", OutputType = "float3",
                                      Code = "return In * 0.5f;", In = vec3(uv.x, uv.y, t));

        // Mask pseudo-name on an unnamed output.
        float3 vcol = UE.Expression(Class = "VertexColor", OutputType = "float3", Output = "RGB");

        Color = tinted * vcol * Radius;
    }
}
```

## See also

- [`UE.Expression`](ue-expression.md) — the call form this argument belongs to
- [`UE.*` catalogue](ue.md) — the registered builtins, which never read this argument
- [Builtins](index.md) — the five call surfaces
- [Substrate](substrate.md) — where the output type is synthesized instead
- [Types](../language/types.md) — the language's declaration type tokens and their validity matrix
- [Compact types](../parameters/compact-types.md) — the parameter type tokens
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — the function parameter type set
- [Conversions](../graph/conversions.md) — component counts and authoritative widths
- [Node reuse](../graph/node-reuse.md) — why the token spelling affects node identity
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
