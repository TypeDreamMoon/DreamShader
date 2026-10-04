# Transform builtins

> [DreamShader](../index.md) » [Builtins](index.md) » **Transform builtins**

The two `UE.*` builtins that change the coordinate basis of a value: `UE.TransformVector` for
directions and `UE.TransformPosition` for points.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding expression, or an `Outputs` declaration initializer |
| Kind | 1.x names of reflected nodes — `TransformVector` is the catalog's alias of `Transform`, `TransformPosition` its short name |
| Generates | `UMaterialExpressionTransform` / `UMaterialExpressionTransformPosition` |
| Output | `float3`, for both |

A basis is a value of the engine's enum, written without its prefix: `"World"`, or the bare word
`World`. In a `.dss` it is matched exactly. In a 1.x source case, spaces, `_` and `-` are ignored and
the enum's prefix or scope may be written, with the warning
[`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278): `"world"`, `" WORLD "` and `TRANSFORMSOURCE_World` are
`World`. The names 1.x had of its own — `AbsoluteWorld`, `Particle`, and for a position
`CameraRelativeWorld` and `FirstPerson` — are written as the engine's, with the same warning (below).
Any other value the enum does not have is [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215).

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
| `Source` | no | enum value | `Tangent` — the node's | sets `TransformSourceType` |
| `Destination` | no | enum value | `World` — the node's | sets `TransformType` |

`Source` and `Destination` accept a quoted string or a bare word — `Source = World` and
`Source = "World"` are equivalent. They are the catalog's aliases of the two properties, which can be
named directly as well.

### Source basis names

| Spelling | `EMaterialVectorCoordTransformSource` |
| :-- | :-- |
| `Tangent` | `TRANSFORMSOURCE_Tangent` |
| `Local` | `TRANSFORMSOURCE_Local` |
| `World` | `TRANSFORMSOURCE_World` |
| `View` | `TRANSFORMSOURCE_View` |
| `Camera` | `TRANSFORMSOURCE_Camera` |
| `Instance` | `TRANSFORMSOURCE_Instance` |

### Destination basis names

The same six spellings, values of the destination enum.

| Spelling | `EMaterialVectorCoordTransform` |
| :-- | :-- |
| `Tangent` | `TRANSFORM_Tangent` |
| `Local` | `TRANSFORM_Local` |
| `World` | `TRANSFORM_World` |
| `View` | `TRANSFORM_View` |
| `Camera` | `TRANSFORM_Camera` |
| `Instance` | `TRANSFORM_Instance` |

> [!NOTE]
> A 1.x source may also write `AbsoluteWorld` for `World`, and `Particle` or `InstanceParticle` for
> `Instance`, in either list, in any case: the front end writes the engine's name, with the warning
> `DSH5278` *(2.0.0 – 2.1.0 refused each with `DSH5215`)*. The engine's own `ParticleWorld` value is
> hidden, and the catalog leaves it out.

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

| Name | Required | Kind | Default | Effect |
| :-- | :-- | :-- | :-- | :-- |
| **`Input`** | yes | expression | — | wired to the node's `Input` pin. May be given positionally as argument 0. |
| `Source` | no | enum value | `Local` — the node's | sets `TransformSourceType` |
| `Destination` | no | enum value | `World`, as in 1.x — not the node's `Local` | sets `TransformType` |
| `PeriodicWorldTileSize` | no | expression | pin left unconnected | wired to the node's pin of that name, where the engine's class has it; meaningful with the `PeriodicWorld` basis |
| `FirstPersonInterpolationAlpha` | no | expression | pin left unconnected | wired to the node's pin of that name, where the engine's class has it; meaningful with the `FirstPersonTranslatedWorld` basis |

> [!IMPORTANT]
> The node's own `Destination` is `Local`. 1.x wrote `World`, and so does the front end: a call
> without `Destination` goes to world space, and [`dsc migrate`](../tools/migrate.md) writes
> `Destination = World` out, because a `.dss` call gets the node's `Local`. *(2.0.0 – 2.1.0 wrote no
> default, and such a call was a transform that changed nothing.)*

### Basis names

One enum serves both `Source` and `Destination`.

| Spelling | `EMaterialPositionTransformSource` |
| :-- | :-- |
| `Local` | `TRANSFORMPOSSOURCE_Local` |
| `World` | `TRANSFORMPOSSOURCE_World` |
| `PeriodicWorld` | `TRANSFORMPOSSOURCE_PeriodicWorld` — where the engine's enum has it |
| `TranslatedWorld` | `TRANSFORMPOSSOURCE_TranslatedWorld` |
| `FirstPersonTranslatedWorld` | `TRANSFORMPOSSOURCE_FirstPersonTranslatedWorld` — where the engine's enum has it |
| `View` | `TRANSFORMPOSSOURCE_View` |
| `Camera` | `TRANSFORMPOSSOURCE_Camera` |
| `Instance` | `TRANSFORMPOSSOURCE_Instance` |

A 1.x source may also write `AbsoluteWorld` (`World`), `CameraRelativeWorld` (`TranslatedWorld`),
`FirstPerson` (`FirstPersonTranslatedWorld`), `Particle` and `InstanceParticle` (`Instance`), in any
case: the front end writes the engine's name, with the warning `DSH5278` *(2.0.0 – 2.1.0 refused each
with `DSH5215`)*. The engine's own `Particle` value is hidden and left out of the catalog. The engine
marks `FirstPersonTranslatedWorld` as no valid *Source*; DreamShader does not check that.

> [!WARNING]
> **A basis the running engine lacks is reported as an unknown value, not as a version error.**
> `UE.TransformPosition(P, Source = "Local", Destination = "PeriodicWorld")` on an engine whose enum
> has no `PeriodicWorld` is `DSH5215`, the code a typo gets. The message names the property that
> failed — `TransformSourceType` for `Source`, `TransformType` for `Destination` *(since 2.0.0)*.

> [!WARNING]
> **The two optional pins follow the engine, both the same way.** On an engine whose class lacks
> `PeriodicWorldTileSize` or `FirstPersonInterpolationAlpha`, the argument is no pin of the node: a
> 1.x source keeps it with the info [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291), and building the
> node fails with [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) *(since 2.0.0; 1.x dropped
> `PeriodicWorldTileSize` silently and refused `FirstPersonInterpolationAlpha` with an error of its
> own)*. To keep one source building across engine versions, guard the argument with
> `#if DS_ENGINE_MINOR >= …` — see [Preprocessor](../language/preprocessor.md).

---

## Shared behaviour

| Property | Value |
| :-- | :-- |
| Result | `float3` — the catalog's known width for both classes |
| Result flags | not a texture object, not `MaterialAttributes`, not `Substrate` |
| Argument-name matching | exact; in a 1.x source a name that matches only ignoring case is accepted with [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276). Otherwise exact — no separator stripping |
| Node reuse | two identical calls are one node *(since 2.0.0)* |

> [!WARNING]
> **An argument these names did not read in 1.x is dropped, with a warning.** Only the names
> documented above are kept; anything else is dropped with
> [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) *(since 2.0.0; 1.x dropped it without a word)*.
> `UE.TransformVector(V, Src = "World")` uses the default `Source`, `Tangent`, because the argument is
> spelled `Src` rather than `Source`; a second positional argument — `UE.TransformVector(V, "World")` —
> is dropped too, since only position 0 is read. Always name `Source` and `Destination` in full.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279) (warning) | no `Input`, by name or at position 0: the required pin is left unconnected *(since 2.0.0: 1.x refused the call)* |
| `DSH5254` (warning) | an argument other than the ones documented, or a positional argument past position 0, is dropped |
| `DSH5215` | a basis is no value of the running engine's enum |
| `DSH5278` (warning) | a basis is spelled the 1.x way — another case, the prefix, spaces, or a name only 1.x had, such as `AbsoluteWorld` |
| [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224) | `Source` or `Destination` is neither a quoted string nor a word |
| `DSH5276` (warning) | an argument name matches only ignoring case |
| `DSH5291` (info), `DSH8212` | an optional pin the running engine's class does not have |
| [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) | a pin is given a value it cannot take |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the material node could not be created |

Evaluating the `Input`, `PeriodicWorldTileSize` or `FirstPersonInterpolationAlpha` expression reports
the inner expression's own diagnostics. Every code is described on its page in
[Diagnostics](../diagnostics/index.md).

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
        // Direction: tangent space -> world space (both node defaults, written out for clarity).
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
WorldPosition                                                                  -> WorldP   (output XYZ)
TransformPosition  Input=WorldP         TransformSourceType=TRANSFORMPOSSOURCE_World
                                        TransformType=TRANSFORMPOSSOURCE_TranslatedWorld
                                                                               -> ViewP    (3 components)
Normalize / Multiply / Add chain                                               -> Emissive
```

## See also

- [Builtins](index.md) — the call surfaces available inside `Graph`
- [`UE.*` catalogue](ue.md) — the other 25 1.x names and their arguments
- [`UE.Expression`](ue-expression.md) — any `UMaterialExpression` by its class, and enum values
- [Math builtins](math.md) — `normalize`, `dot` and the rest of the unprefixed call surface
- [`Substrate.*`](substrate.md) — Substrate nodes
- [Calls](../graph/calls.md) — call syntax, named arguments, positional arguments
- [Expressions and operators](../graph/expressions.md) — what an argument expression may contain
- [Conversions](../graph/conversions.md) — widths and how values fit
- [Node reuse](../graph/node-reuse.md) — identical nodes
- [Output bindings](../language/output-bindings.md) — `Base.Normal` and the other binding targets
- [Diagnostics index](../diagnostics/index.md) — every code
