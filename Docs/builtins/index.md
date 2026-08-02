# Builtins

> [DreamShader](../index.md) » **Builtins**

The names DreamShaderLang resolves itself instead of looking them up among the author's declarations:
the `UE.*` namespace, the `Substrate.*` namespace, the reserved `SampleTexture2D` form, and the math
builtin set.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` |
| Kind | section hub |
| Generates | one or more `UMaterialExpression` nodes per call |

## The call surfaces

These are five different implementations, not five spellings of one. They differ in where they may be
written, how arguments are matched, whether `OutputType` is required, and which diagnostics they emit.
Do not carry a rule from one row to another.

| # | Surface | Written as | Where | Argument model | `OutputType` | Reference |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | Registered `UE.*` sugar | `UE.TexCoord(Index = 0)` | `Graph` | named only; unknown and positional arguments are **silently ignored** | not read | [`UE.*` catalogue](ue.md) |
| 2 | Generic reflected call | `UE.Expression(Class = "Sine", …)` or `UE.Sine(…)` | `Graph` | named only; each argument is dispatched to an input pin, then to a reflected `UPROPERTY` | **required** | [`UE.Expression`](ue-expression.md) |
| 3 | Property declaration | `UE.TexCoord(Index = 0) UV;` | `Properties` | `Key=Value` text pairs; **whitelisted per builtin**, an unlisted name is an error | accepted by `UE.CollectionParam` and generic declarations, from a **smaller** token set | [`UE.*` catalogue](ue.md#properties-declaration-form) |
| 4 | Math builtins | `pow(a, b)` | `Graph` | positional | not applicable | [Math builtins](math.md) |
| 5 | `Substrate.*` | `Substrate.Slab(BaseColor = c)` | `Graph` | named only | **ignored** — synthesized from the node | [Substrate](substrate.md) |

Surface 3 is a *declaration*, not an expression: it creates one node at parse+generate time and binds
it to a property name that the `Graph` then reads. Surfaces 1, 2, 4 and 5 are expressions evaluated
inside a `Graph` body.

> [!IMPORTANT]
> Surfaces 1 and 3 share builtin names but not behaviour. `UE.TexCoord` in a `Graph` accepts `Index`
> but not `CoordinateIndex`; as a property declaration it accepts both. `UE.Time` validates
> `Period >= 0` as a declaration and does not validate it in a `Graph`. `UE.ScreenPosition` reports 2
> components in a `Graph` and 4 as a declaration. Every divergence is tabulated in
> [`UE.*` catalogue § Properties declaration form](ue.md#properties-declaration-form).

### The reserved `SampleTexture2D` form

`SampleTexture2D(<texture-object>, <uv>)` is not in a namespace but belongs to the same cluster. It is
a reserved name, resolved before user properties and functions, and it desugars to

```c
UE.Expression(Class = "TextureSample", OutputType = "float4", TextureObject = <arg0>, Coordinates = <arg1>)
```

It is the only builtin call form matched **case-sensitively**: `sampletexture2d(t, uv)` is not it.
It takes exactly two positional arguments. See [`UE.Expression`](ue-expression.md#sampletexture2d).

## Dispatch order inside a `Graph`

A call expression is resolved against these in order. The first match wins.

| Order | Test on the callee | Result |
| :-- | :-- | :-- |
| 1 | is a vector/scalar constructor name | [constructor](../graph/constructors.md) |
| 2 | equals `UE.SceneTexture` | desugared, then re-entered as a generic `UE.Expression` call |
| 3 | starts with `UE.` (case-insensitive) | UE builtin evaluation, namespace `UE` |
| 4 | starts with `Substrate.` (case-insensitive) | UE builtin evaluation, namespace `Substrate` |
| 5 | is a math builtin name | [math builtin](math.md) |
| 6 | equals `SampleTexture2D` (case-**sensitive**) | desugared, then re-entered as a generic `UE.Expression` call |

Inside UE builtin evaluation the namespace prefix is removed and the remaining function name is
resolved in this order:

| Order | Match | Namespace | Result |
| :-- | :-- | :-- | :-- |
| 1 | `StaticSwitchParameter` | `UE` only | [special-cased builtin](ue.md#uestaticswitchparameter) |
| 2 | `CollectionParam`, `CollectionParameter` | `UE` only | [special-cased builtin](ue.md#uecollectionparam) |
| 3 | any of the 27 registered names | `UE` only | [registered sugar](ue.md#catalogue) |
| 4 | a `Substrate.*` descriptor name | `Substrate` only | [Substrate node](substrate.md) |
| 5 | anything else | both | [generic reflected path](ue-expression.md) |

The prefix strip removes a fixed three characters for `UE.` and ten for `Substrate.`. The namespace
token — literally `UE` or `Substrate` — is reproduced in several diagnostics.

## Name and argument matching

| Element | Rule |
| :-- | :-- |
| Namespace prefix (`UE.`, `Substrate.`) | case-insensitive |
| Builtin function name | case-insensitive |
| `SampleTexture2D` | case-**sensitive** |
| Argument names | case-insensitive, whitespace-trimmed, otherwise exact — `_`, `-` and spaces are **not** removed |
| Enum *values* passed to reflected properties | case-insensitive, and ` `, `_`, `-`, `:`, `.`, `/` are removed |
| Unquoted text arguments | a bare identifier or dotted name is accepted wherever a string is — `OutputType = float3` equals `OutputType = "float3"` |

Outside the math builtins, positional arguments are accepted in exactly three places:
`UE.TransformVector` and `UE.TransformPosition` take `Input` at index 0, `UE.StaticSwitchParameter`
takes `True` at index 0 and `False` at index 1, and `SampleTexture2D` takes both of its arguments
positionally. Everything on the generic and `Substrate.*` paths rejects them with
`Generic {Namespace}.{Function} calls require named arguments.` The
[math builtins](math.md) are the reverse: they accept **only** positional arguments.

> [!WARNING]
> **Registered `UE.*` builtins never validate their argument list.** A registered builtin reads only
> the names it knows and discards the rest without a diagnostic. `UE.Time(Bogus = 1)` compiles.
> `UE.TexCoord(0)` compiles and produces UV channel 0 because the positional argument is dropped and
> `CoordinateIndex` keeps its node default — the same result by accident, not by design. Misspell
> `UTiling` and the tiling is silently not applied. The generic path
> ([`UE.Expression`](ue-expression.md)) does validate every argument name, and so does the
> [property declaration form](ue.md#properties-declaration-form).

## Pages

| Page | Contents |
| :-- | :-- |
| [`UE.*` catalogue](ue.md) | Every registered builtin, its node class, arguments, output width and version gates; the special-cased builtins; the property declaration form |
| [`UE.Expression`](ue-expression.md) | `Class=` resolution, the argument dispatch algorithm, per-property-type value parsing, output selection, Custom-node behaviour |
| [`OutputType`](output-type.md) | Every accepted `OutputType` / `ResultType` token, per surface, plus the output mask pseudo-names |
| [Math builtins](math.md) | `pow`, `min`, `max`, `fmod`, `fract` and the rest |
| [Transform bases](transform.md) | The basis vocabularies for `UE.TransformVector` and `UE.TransformPosition` |
| [Substrate](substrate.md) | The `Substrate.*` node catalogue and its UE 5.4 gate |
| [HLSL library](hlsl-library.md) | `Shaders/DreamShaderBuiltins.ush` |

## Notes

- **Node reuse covers the generic and `Substrate.*` surfaces, not the registered sugar.** Two textually
  identical generic or `Substrate.*` calls over identical operands collapse into one
  `UMaterialExpression`; that reuse key covers the class, the normalized `OutputType` text and every
  non-reserved argument, and `UMaterialExpressionCustom` is the one class excluded. Math builtins
  cache under their own per-builtin keys. A registered `UE.*` builtin, `UE.StaticSwitchParameter` and
  `UE.CollectionParam` consult no cache — each call creates a fresh node. See
  [Node reuse](../graph/node-reuse.md).
- Builtin nodes are placed on the canvas at fixed X coordinates before auto-layout runs: 520 for
  registered sugar and `UE.StaticSwitchParameter`, **-520** for `UE.CollectionParam`, and -800 for
  every node created by the property declaration form. See [Graph layout](../generation/graph-layout.md).
- A `UE.*` call written inside a [`GraphFunction`](../language/graph-function.md) body is hoisted out
  of the HLSL and becomes a generated input pin on the emitted Custom node. The argument rules on this
  page still apply to the call itself.
- The `Substrate.*` namespace is compiled out entirely below UE 5.4; a call then fails with
  `Substrate builtin call '{Name}' requires Unreal Engine 5.4 or newer.`
- There is no user-extensible builtin registry. A name that is not a builtin, a declared property, a
  `Graph` variable or a declared function is an [unknown identifier](../graph/name-resolution.md).

## Example

```c
Shader(Name="Docs/M_Surfaces")
{
    Properties {
        Texture2D BaseTex = Path(Game, "Textures/T_Noise");
        float     Speed   = 0.2;
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // 1 — registered UE.* sugar
        float2 uv = UE.Panner(Coordinate = UE.TexCoord(Index = 0), Speed = Speed);

        // 2 — generic reflected call, Class defaults to the function name
        float pulse = UE.Sine(OutputType = "float1", Input = UE.Time());

        // the reserved two-argument sample form
        float4 tex = SampleTexture2D(BaseTex, uv);

        // 4 — math builtin
        float k = pow(pulse, 2.0);

        Color = tex.rgb * k;
    }
}
```

## See also

- [`UE.*` catalogue](ue.md) — every registered builtin, one entry each
- [`UE.Expression`](ue-expression.md) — the generic reflected escape hatch
- [`OutputType`](output-type.md) — the token table shared by all the surfaces
- [Substrate](substrate.md) — the `Substrate.*` sibling namespace
- [Math builtins](math.md) — the scalar/vector math set
- [Calls](../graph/calls.md) — call syntax, named arguments and out arguments
- [Name resolution](../graph/name-resolution.md) — the lookup order a callee goes through
- [Node reuse](../graph/node-reuse.md) — why two identical builtin calls yield one node
- [Properties](../language/properties.md) — the section the declaration form lives in
- [`Path(…)`](../parameters/path.md) — asset-reference syntax used by `UE.CollectionParam`
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
