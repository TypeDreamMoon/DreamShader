# OutputType

> [DreamShader](../index.md) » [Builtins](index.md) » **OutputType**

The argument with which a 1.x call said what kind of value a reflected builtin produces. Since 2.0.0
the builtin catalog says it for every class, and `OutputType` keeps two jobs: the output type of a
Custom node, and a width hint in a 1.x source.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — as an argument of a `UE.*` call in `Graph`, or of a `UE.*` property declaration |
| Kind | builtin argument |
| Aliases | `ResultType` |

## Synopsis

```c
{ OutputType | ResultType } = <type-token>
```

The legacy front end finds `OutputType` and `ResultType` ignoring case and takes them off the call
before the binder sees it — on a Custom call it keeps the first of them as the node's output type and
drops a second. Neither is ever bound as a pin or property. The token may be quoted or bare:
`OutputType = "float3"` and `OutputType = float3` are identical.

It is required nowhere *(since 2.0.0; 1.x required it on every generic call and on a property
declaration outside its table)*. A `.dss` has no such argument: a reflected node is called as
`UE.<ClassShortName>(Pin = …, Property = …)` and its value is typed by the catalog.

## Where it is read

| Surface | Tokens | Effect |
| :-- | :-- | :-- |
| `UE.Expression(Class = "Custom", …)` in `Graph` | the Custom token set below | **the node's output type** — written to the node, decides the HLSL return type |
| any other `UE.*` call in `Graph` | a scalar or vector type spelling | on a node with **one numeric output**: the type of the value, as 1.x typed it, the node wired whole. Otherwise dropped |
| `UE.<Name>(…)` property declaration | any type spelling | the type of the local the declaration becomes — see [Declared output width](ue.md#declared-output-width) |
| one of the 27 1.x names of the [`UE.*` catalogue](ue.md#catalogue) in `Graph` | — | dropped with [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254), like any argument the name does not read |
| `Substrate.*` | — | dropped without a diagnostic |

A token that fits none of these — a texture, `MaterialAttributes`, `Substrate` or `SamplerState`
token on a non-Custom call, a matrix, a misspelling — is dropped without a diagnostic
*(since 2.0.0; 1.x reported an unsupported token)*, and the catalog types the value.

> [!NOTE]
> The width hint follows 1.x where the catalog would build another graph:
> `float Cam = UE.Expression(Class = "CameraPositionWS", OutputType = "float")` types `Cam` as a
> `float` and connects the node's whole output wherever `Cam` is read, which is what the 1.x graph
> did. On a node the engine types the same way the hint does, it changes nothing.

## Accepted tokens

### On a Custom node

Compared ignoring case and with every space removed. Anything else is
[`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261).

| Tokens | Custom output type |
| :-- | :-- |
| `float`, `float1`, `half`, `half1` | `Float1` |
| `float2`, `vec2`, `half2` | `Float2` |
| `float3`, `vec3`, `half3` | `Float3` |
| `float4`, `vec4`, `half4` | `Float4` |
| `MaterialAttributes` — quoted | `MaterialAttributes` |

*(since 2.0.0)* `int`, `uint`, `bool` and their vectors, `ivec*`, `uvec*`, `bvec*`, the texture tokens,
`StaticBool` and `Substrate` are `DSH5261` here. A bare `MaterialAttributes` is read as the type
`material` and is `DSH5261` as well; quote it.

### On any other call, and on a property declaration

A token is a 1.x type spelling, compared ignoring case:

| Tokens | Kind | Graph call | Property declaration |
| :-- | :-- | :-: | :-: |
| `float`, `half`, `double`, `int`, `uint`, `bool`, and each with `1` | scalar | ✔ | ✔ |
| the same with `2`, `3`, `4`; `vec2`–`vec4`, `ivec2`–`ivec4`, `uvec2`–`uvec4`, `bvec2`–`bvec4` | vector | ✔ | ✔ |
| `StaticBool`, `StaticBoolParameter` | read as `bool` | ✔ | ✔ |
| `Texture2D`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture` | texture | dropped | ✔ — the dimension is kept |
| `MaterialAttributes` | `material` | dropped | ✔ |
| `Substrate`, `SamplerState` | | dropped | ✔ |
| `mat2`–`mat4`, `float3x3` and the other matrices | matrix | dropped | read, but no node's value fits a matrix local |

On a `Graph` call the token is trimmed but keeps its inner spaces (`" float 4 "` is no token); on a
property declaration every space is removed first. Underscores and dashes are never removed:
`float_4` and `Material-Attributes` are not spellings.

## Output mask pseudo-names

Distinct from `OutputType`: the `Output` / `OutputName` argument selects **which** output of a
multi-output node is read, and becomes `.Name` on the call. The name is one of the outputs the catalog
lists for the class. An output the engine leaves unnamed is listed under the channels of its mask —
`R`, `G`, `B`, `A`, `RG`, `RGB`, `RGBA` — when the class has more than one output, unless the class
already uses that name. A single output keeps the empty name and is the value of the call.

| Node | Outputs as the catalog lists them (UE 5.8) |
| :-- | :-- |
| `VertexColor` | `RGB`, `R`, `G`, `B`, `A` |
| `TextureSample` | `RGB`, `R`, `G`, `B`, `A`, `RGBA` |
| `SceneColor` | `RGB`, `A` |
| `SceneTexture` | `Color`, `Size`, `InvSize` |

The name is compared exactly; in a 1.x source a name that matches only ignoring case is accepted with
[`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276). A name no output has is
[`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) *(since 2.0.0; 1.x tested the seven mask names against
every unnamed output, in a fixed order)*. On a node whose outputs are all channel views of one value,
a swizzle inside one output is that output (`UE.VertexColor().a`). Full rules:
[`UE.Expression`](ue-expression.md#selecting-an-output).

> [!NOTE]
> These are not `OutputType` values, and an `OutputType` token is never a valid `Output` selector.
> `UE.Expression(Class = "BreakMaterialAttributes", Output = "BaseColor")` selects by output *name*;
> `UE.VertexColor(Output = "RGB")` selects the output its mask names.

## Notes

- **The width comes from the catalog.** `UE.Expression(Class = "WorldPosition")` is a node whose
  first output, `XYZ`, is a `float3`; no token is needed or consulted for that. See
  [Result type and component count](ue-expression.md#result-type-and-component-count).
- Two calls that differ only in how `OutputType` is spelled — `float3` against `vec3` — are one node:
  the IR merges nodes, not spellings *(since 2.0.0)*. See [Node reuse](../graph/node-reuse.md).
- This token set is the 1.x type vocabulary. The `Properties` section additionally accepts the
  parameter-node tokens; see [Types](../language/types.md),
  [Compact types](../parameters/compact-types.md) and
  [Inputs / Outputs / Results](../language/inputs-outputs.md).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| [`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261) | the `OutputType` of a Custom node is not one of the tokens above |
| [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) (warning) | `OutputType` / `ResultType` on one of the 27 1.x names, dropped |
| [`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228) | a property declaration's type does not take the node's value — a narrower vector than the node makes |
| [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) | an `Output` name that is no output of the node |

Every code is described on its page in [Diagnostics](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_OutputTypes")
{
    Properties {
        // Declaration surface: the type of the local `Radius`.
        UE.Expression(Class = "ObjectRadius", OutputType = "float1") Radius;
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // Not needed: the catalog gives TextureCoordinate a float2.
        float2 uv = UE.Expression(Class = "TextureCoordinate", OutputType = "float2");

        // ResultType is the alias.
        float t = UE.Expression(Class = "Time", ResultType = "float1");

        // The node's own output type on a Custom node.
        float3 tinted = UE.Expression(Class = "Custom", OutputType = "float3",
                                      Code = "return In * 0.5f;", In = vec3(uv.x, uv.y, t));

        // An output the catalog names after its mask.
        float3 vcol = UE.Expression(Class = "VertexColor", Output = "RGB");

        Color = tinted * vcol * Radius;
    }
}
```

## See also

- [`UE.Expression`](ue-expression.md) — the call form this argument belongs to
- [`UE.*` catalogue](ue.md) — the 1.x names, which drop this argument
- [Builtins](index.md) — the call surfaces
- [Substrate](substrate.md) — where the argument is dropped
- [Types](../language/types.md) — the language's declaration type tokens
- [Compact types](../parameters/compact-types.md) — the parameter type tokens
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — the function parameter type set
- [Conversions](../graph/conversions.md) — widths and how values fit
- [Node reuse](../graph/node-reuse.md) — identical nodes
- [Diagnostics index](../diagnostics/index.md) — every code
