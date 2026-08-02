# Transform builtins

> [DreamShader](../index.md) » [Builtins](index.md) » **Transform builtins**

The two `UE.*` builtins that change the coordinate basis of a value: `UE.TransformVector` for
directions and `UE.TransformPosition` for points.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration initializer |
| Kind | registered `UE.*` builtins |
| Generates | `UMaterialExpressionTransform` / `UMaterialExpressionTransformPosition` |
| Output | 3 components, authoritative, for both |

Basis names are matched **case-insensitively** after trimming: `"world"`, `"World"` and `" WORLD "`
are the same token. Spaces, underscores and hyphens are **not** stripped — `"absolute world"` and
`"absolute_world"` do not resolve; write `"AbsoluteWorld"`.

---

## UE.TransformVector

Transforms a direction vector between bases. Rotation only — translation is not applied, which is
what makes it the wrong choice for a point.

### Synopsis

```c
UE.TransformVector ( { <expression> | Input = <expression> }
                     [ , Source      = "<vector-basis>" ]
                     [ , Destination = "<vector-basis>" ] )
```

### Arguments

| Name | Required | Kind | Default | Effect |
| :-- | :-- | :-- | :-- | :-- |
| **`Input`** | yes | expression | — | wired to the node's `Input` pin. May be given positionally as argument 0 instead of by name. |
| `Source` | no | text literal | `"Tangent"` | sets `TransformSourceType` |
| `Destination` | no | text literal | `"World"` | sets `TransformType` |

`Source` and `Destination` accept a quoted string or a bare identifier — `Source = World` and
`Source = "World"` are equivalent.

### Source basis names

Nine spellings resolve to six engine values. Every spelling is available on all supported engine
versions (UE 5.3 – 5.8).

| Spelling | `EMaterialVectorCoordTransformSource` | Since UE |
| :-- | :-- | :-- |
| `Tangent` | `TRANSFORMSOURCE_Tangent` | all |
| `Local` | `TRANSFORMSOURCE_Local` | all |
| `World` | `TRANSFORMSOURCE_World` | all |
| `AbsoluteWorld` | `TRANSFORMSOURCE_World` | all |
| `View` | `TRANSFORMSOURCE_View` | all |
| `Camera` | `TRANSFORMSOURCE_Camera` | all |
| `Instance` | `TRANSFORMSOURCE_Instance` | all |
| `Particle` | `TRANSFORMSOURCE_Instance` | all |
| `InstanceParticle` | `TRANSFORMSOURCE_Instance` | all |

### Destination basis names

The same nine spellings, resolved by a separate function to the destination enum. There is no
spelling accepted by one side and rejected by the other.

| Spelling | `EMaterialVectorCoordTransform` | Since UE |
| :-- | :-- | :-- |
| `Tangent` | `TRANSFORM_Tangent` | all |
| `Local` | `TRANSFORM_Local` | all |
| `World` | `TRANSFORM_World` | all |
| `AbsoluteWorld` | `TRANSFORM_World` | all |
| `View` | `TRANSFORM_View` | all |
| `Camera` | `TRANSFORM_Camera` | all |
| `Instance` | `TRANSFORM_Instance` | all |
| `Particle` | `TRANSFORM_Instance` | all |
| `InstanceParticle` | `TRANSFORM_Instance` | all |

> [!NOTE]
> `AbsoluteWorld` is an accepted alias of `World`, not a distinct basis — vectors have no
> translation component, so the engine's vector transform has one world basis. The distinction
> between absolute and translated world exists only on
> [`UE.TransformPosition`](#uetransformposition).

---

## UE.TransformPosition

Transforms a point between bases, applying translation.

### Synopsis

```c
UE.TransformPosition ( { <expression> | Input = <expression> }
                       [ , Source                        = "<position-basis>" ]
                       [ , Destination                   = "<position-basis>" ]
                       [ , PeriodicWorldTileSize         = <expression> ]
                       [ , FirstPersonInterpolationAlpha = <expression> ] )
```

### Arguments

| Name | Required | Kind | Default | Since UE | Effect |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **`Input`** | yes | expression | — | all | wired to the node's `Input` pin. May be given positionally as argument 0. |
| `Source` | no | text literal | `"Local"` | all | sets `TransformSourceType` |
| `Destination` | no | text literal | `"World"` | all | sets `TransformType` |
| `PeriodicWorldTileSize` | no | expression | pin left unconnected | **5.5** | wired to the node's `PeriodicWorldTileSize` pin; meaningful with the `PeriodicWorld` basis |
| `FirstPersonInterpolationAlpha` | no | expression | pin left unconnected | **5.6** | wired to the node's `FirstPersonInterpolationAlpha` pin; meaningful with the `FirstPerson` basis |

Note the default `Source` differs from `UE.TransformVector`: it is `"Local"` here, `"Tangent"`
there.

### Basis names

One resolver serves both `Source` and `Destination`, so the accepted set is identical for the two
arguments. Thirteen spellings resolve to eight engine values.

| Spelling | `EMaterialPositionTransformSource` | Since UE | Below that version |
| :-- | :-- | :-- | :-- |
| `Local` | `TRANSFORMPOSSOURCE_Local` | all | — |
| `World` | `TRANSFORMPOSSOURCE_World` | all | — |
| `AbsoluteWorld` | `TRANSFORMPOSSOURCE_World` | all | — |
| `PeriodicWorld` | `TRANSFORMPOSSOURCE_PeriodicWorld` | **5.5** | rejected: `UE.TransformPosition Source/Destination is invalid.` |
| `TranslatedWorld` | `TRANSFORMPOSSOURCE_TranslatedWorld` | all | — |
| `CameraRelativeWorld` | `TRANSFORMPOSSOURCE_TranslatedWorld` | all | — |
| `FirstPerson` | `TRANSFORMPOSSOURCE_FirstPersonTranslatedWorld` | **5.6** | rejected: `UE.TransformPosition Source/Destination is invalid.` |
| `FirstPersonTranslatedWorld` | `TRANSFORMPOSSOURCE_FirstPersonTranslatedWorld` | **5.6** | rejected: same message |
| `View` | `TRANSFORMPOSSOURCE_View` | all | — |
| `Camera` | `TRANSFORMPOSSOURCE_Camera` | all | — |
| `Instance` | `TRANSFORMPOSSOURCE_Instance` | all | — |
| `Particle` | `TRANSFORMPOSSOURCE_Instance` | all | — |
| `InstanceParticle` | `TRANSFORMPOSSOURCE_Instance` | all | — |

Alias groups: `World` / `AbsoluteWorld`; `TranslatedWorld` / `CameraRelativeWorld`;
`FirstPerson` / `FirstPersonTranslatedWorld`; `Instance` / `Particle` / `InstanceParticle`.

> [!WARNING]
> **A version-gated spelling below its gate is reported as an invalid basis, not as a version
> error.** On UE 5.3 or 5.4, `UE.TransformPosition(P, Source="Local", Destination="PeriodicWorld")`
> fails with `UE.TransformPosition Source/Destination is invalid.` — the same message a typo
> produces. The message never says which of the two arguments failed, because one resolver failure
> on either side raises it.

> [!WARNING]
> **`PeriodicWorldTileSize` is silently ignored on UE 5.3 and 5.4.** The argument is not read, not
> validated and not reported on those engines; the call otherwise compiles and the node is created
> without the pin connection. This is the only asymmetry between the two version-gated inputs —
> `FirstPersonInterpolationAlpha` below UE 5.6 is a hard error
> (`UE.TransformPosition FirstPersonInterpolationAlpha requires Unreal Engine 5.6 or newer.`).
> To keep a source file portable across 5.3 – 5.8, guard the value on the authoring side rather
> than relying on the argument being rejected.

---

## Shared behaviour

| Property | Value |
| :-- | :-- |
| Result component count | 3, marked authoritative |
| Result flags | not a texture object, not `MaterialAttributes`, not `Substrate` |
| Editor X coordinate of the generated node | `520` |
| Argument-name matching | case-insensitive, whitespace-trimmed, otherwise exact |
| Node reuse | **not** applied — two textually identical calls still produce two nodes |

> [!WARNING]
> **Registered `UE.*` builtins do not validate their argument list.** Only the names documented above
> are read; anything else is discarded without a diagnostic. `UE.TransformVector(V, Src="World")`
> compiles and silently uses the default `Source` of `"Tangent"`, because the argument is spelled
> `Src` rather than `Source`. Likewise a second positional argument —
> `UE.TransformVector(V, "World")` — is ignored, since only positional index 0 is consumed. Always
> name `Source` and `Destination` in full.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table; the compiler emits the
substituted text. `{Name}` is the builtin name as the author spelled it, so its casing is preserved —
except in `Failed to create UE.{Name}.`, which prints the registered spelling
(`TransformVector` / `TransformPosition`).

| Message | Cause |
| :-- | :-- |
| `UE.TransformVector requires parameter: Input` | no `Input=` argument and no positional argument 0 |
| `UE.TransformPosition requires parameter: Input` | same, for `UE.TransformPosition` |
| `UE.{Name} Source must be a text value.` | `Source=` is not a string literal or bare identifier |
| `UE.{Name} Destination must be a text value.` | `Destination=` is not a string literal or bare identifier |
| `UE.TransformVector Source/Destination is invalid.` | either basis name is not in the vector table above |
| `UE.TransformPosition Source/Destination is invalid.` | either basis name is not in the position table above, **or** is gated above the running engine version |
| `UE.TransformPosition FirstPersonInterpolationAlpha requires Unreal Engine 5.6 or newer.` | the argument was supplied on UE 5.3 – 5.5 |
| `Failed to create UE.{Name}.` | the material node could not be created |

Evaluating the `Input`, `PeriodicWorldTileSize` or `FirstPersonInterpolationAlpha` expression
propagates the inner expression's own diagnostic unchanged — those failures carry no
`UE.TransformVector` / `UE.TransformPosition` prefix.

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_Transforms")
{
    Properties = {
        vec3 TangentNormal = vec3(0.0, 0.0, 1.0);
    }
    Outputs  = {
        vec3 N;
        vec3 Emissive;
        Base.Normal        = N;
        Base.EmissiveColor = Emissive;
    }
    Graph = {
        // Direction: tangent space -> world space (both defaults, written out for clarity).
        N = UE.TransformVector(TangentNormal, Source = "Tangent", Destination = "World");

        // Point: absolute world space -> camera-relative world space.
        vec3 WorldP = UE.WorldPosition();
        vec3 ViewP  = UE.TransformPosition(WorldP, Source = "World", Destination = "TranslatedWorld");

        Emissive = normalize(ViewP) * 0.5 + vec3(0.5, 0.5, 0.5);
    }
}
```

Generated nodes:

```text
Transform          Input=TangentNormal  TransformSourceType=TRANSFORMSOURCE_Tangent
                                        TransformType=TRANSFORM_World          -> N        (3 components)
WorldPosition                                                                  -> WorldP   (3 components)
TransformPosition  Input=WorldP         TransformSourceType=TRANSFORMPOSSOURCE_World
                                        TransformType=TRANSFORMPOSSOURCE_TranslatedWorld
                                                                               -> ViewP    (3 components)
Normalize / Multiply / Add chain                                               -> Emissive
```

## See also

- [Builtins](index.md) — the call surfaces available inside `Graph`
- [`UE.*` catalogue](ue.md) — the other 25 registered builtins and their arguments
- [`UE.Expression`](ue-expression.md) — reaching any `UMaterialExpression` not covered by a builtin
- [`OutputType` values](output-type.md) — the token set `UE.Expression` accepts
- [Math builtins](math.md) — `normalize`, `dot` and the rest of the unprefixed call surface
- [`Substrate.*`](substrate.md) — Substrate node wrappers (UE 5.4+)
- [Calls](../graph/calls.md) — call syntax, named arguments, positional arguments
- [Expressions and operators](../graph/expressions.md) — what an argument expression may contain
- [Conversions](../graph/conversions.md) — authoritative component counts and widening
- [Node reuse](../graph/node-reuse.md) — the surfaces that do collapse identical calls
- [Output bindings](../language/output-bindings.md) — `Base.Normal` and the other binding targets
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
