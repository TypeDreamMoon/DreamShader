# UE builtins

> [DreamShader](../index.md) » [Builtins](index.md) » **UE builtins**

The catalogue of names in the `UE.` namespace that DreamShader implements itself, each mapping to one
`UMaterialExpression` class with a fixed argument set.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body (expression form) or a `Properties { … }` section (declaration form) |
| Kind | builtin namespace |
| Generates | one `UMaterialExpression` per call |

## Synopsis

```c
// expression form, inside Graph
UE.<Name> ( [ <argument> [ , <argument> ] … ] )

<argument> := <arg-name> = <expression>

// declaration form, inside Properties
UE.<Name> [ ( <key> = <value> [ , <key> = <value> ] … ) ] <property-name> ;
```

The namespace prefix and the builtin name are both matched case-insensitively: `ue.texcoord()` is
`UE.TexCoord()`. Argument names are matched case-insensitively after trimming, but are otherwise
exact — `Un_Mirror_U` is not `UnMirrorU`.

A name that is not in this catalogue is not an error: it falls through to the
[generic reflected path](ue-expression.md), where `UE.<Name>` means "create
`UMaterialExpression<Name>`".

> [!WARNING]
> **Registered builtins do not validate their argument list.** Each one reads only the argument names
> listed in its entry below and drops everything else — unknown names, misspellings and positional
> arguments alike — with no diagnostic. `UE.TexCoord(Indx = 3)` silently produces UV channel 0;
> `UE.Panner(SpedX = 1)` silently pans at the node default speed. If an argument appears to have no
> effect, check its spelling against the entry. The only registered builtins that read a positional
> argument at all are `UE.TransformVector`, `UE.TransformPosition` (index 0 → `Input`) and
> `UE.StaticSwitchParameter` (indices 0 and 1 → `True` and `False`).

## Output widths

Every registered builtin reports an **authoritative** component count, which makes it a valid partner
for the widening rescue in binary operators (see
[Conversions](../graph/conversions.md#authoritative-component-counts)). None of them produces a
texture object, a `MaterialAttributes` value or a Substrate value.

The width in the tables below is the declared width. It is overridden by the node's own known output
width where one is recorded, which is why `UE.ScreenPosition` is 2 components even though the same
name declares 4 in the [declaration form](#properties-declaration-form).

## Catalogue

All 27 registered builtins, in registration order.

| Builtin | `UMaterialExpression` class | Output | Arguments |
| :-- | :-- | :-- | :-- |
| [`UE.TexCoord`](#uetexcoord) | `TextureCoordinate` | `float2` | `Index`, `UTiling`, `VTiling`, `UnMirrorU`, `UnMirrorV` |
| [`UE.Time`](#uetime) | `Time` | `float1` | `Period`, `IgnorePause` |
| [`UE.Panner`](#uepanner) | `Panner` | `float2` | `Coordinate`, `Time`, `Speed`, `SpeedX`, `SpeedY`, `FractionalPart` |
| [`UE.WorldPosition`](#ueworldposition) | `WorldPosition` | `float3` | none |
| [`UE.ObjectPositionWS`](#ueobjectpositionws) | `ObjectPositionWS` | `float3` | none |
| [`UE.CameraVectorWS`](#uecameravectorws) | `CameraVectorWS` | `float3` | none |
| [`UE.VertexNormalWS`](#uevertexnormalws) | `VertexNormalWS` | `float3` | none |
| [`UE.VertexTangentWS`](#uevertextangentws) | `VertexTangentWS` | `float3` | none |
| [`UE.ScreenPosition`](#uescreenposition) | `ScreenPosition` | `float2` | none |
| [`UE.VertexColor`](#uevertexcolor) | `VertexColor` | `float4` | none |
| [`UE.PixelDepth`](#uepixeldepth) | `PixelDepth` | `float1` | none |
| [`UE.SceneDepth`](#uescenedepth) | `SceneDepth` | `float1` | none |
| [`UE.SceneColor`](#uescenecolor) | `SceneColor` | `float4` | none |
| [`UE.TranslatedWorldPosition`](#uetranslatedworldposition) | `WorldPosition` (camera-relative) | `float3` | none |
| [`UE.ObjectPosition`](#ueobjectposition) | `ObjectPositionWS` | `float3` | none |
| [`UE.ObjectRadius`](#ueobjectradius) | `ObjectRadius` | `float1` | none |
| [`UE.ObjectBounds`](#ueobjectbounds) | `ObjectBounds` | `float3` | none |
| [`UE.CameraVector`](#uecameravector) | `CameraVectorWS` | `float3` | none |
| [`UE.CameraPosition`](#uecameraposition) | `CameraPositionWS` | `float3` | none |
| [`UE.ReflectionVector`](#uereflectionvector) | `ReflectionVectorWS` | `float3` | none |
| [`UE.PixelNormalWS`](#uepixelnormalws) | `PixelNormalWS` | `float3` | none |
| [`UE.TwoSidedSign`](#uetwosidedsign) | `TwoSidedSign` | `float1` | none |
| [`UE.PerInstanceRandom`](#ueperinstancerandom) | `PerInstanceRandom` | `float1` | none |
| [`UE.PerInstanceFadeAmount`](#ueperinstancefadeamount) | `PerInstanceFadeAmount` | `float1` | none |
| [`UE.ViewportUV`](#ueviewportuv) | `ScreenPosition` | `float2` | none |
| [`UE.TransformVector`](#uetransformvector) | `Transform` | `float3` | **`Input`**, `Source`, `Destination` |
| [`UE.TransformPosition`](#uetransformposition) | `TransformPosition` | `float3` | **`Input`**, `Source`, `Destination`, `PeriodicWorldTileSize`, `FirstPersonInterpolationAlpha` |

Three further names are handled before this table and are documented under
[Special-cased builtins](#special-cased-builtins): `UE.StaticSwitchParameter`, `UE.CollectionParam` /
`UE.CollectionParameter`, and `UE.SceneTexture`.

Class names are written without the `UMaterialExpression` prefix in the table above.

> [!NOTE]
> **Four builtins are registered conditionally.** `UE.ObjectPositionWS`, `UE.ObjectPosition`,
> `UE.ScreenPosition` and `UE.ViewportUV` are only added to the table when their engine class resolves
> on the running editor — directly on UE 5.5+ (object position) and UE 5.6+ (screen position), and by
> a name lookup of `/Script/Engine.MaterialExpressionObjectPositionWS` /
> `/Script/Engine.MaterialExpressionScreenPosition` below those versions. If the lookup fails the
> builtin is not registered at all and the call falls through to the generic path, where it fails with
> `Unsupported UE builtin call '{Name}' in Graph. …` unless an `OutputType` is supplied.

### UE.TexCoord

```c
UE.TexCoord([Index = <int>] [, UTiling = <float>] [, VTiling = <float>]
            [, UnMirrorU = <bool>] [, UnMirrorV = <bool>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| `Index` | integer literal | node default (UV channel 0) | no |
| `UTiling` | numeric literal | node default | no |
| `VTiling` | numeric literal | node default | no |
| `UnMirrorU` | boolean literal | node default | no |
| `UnMirrorV` | boolean literal | node default | no |

Node `UMaterialExpressionTextureCoordinate`; output `float2`.

`CoordinateIndex` is **not** an accepted spelling here — it is accepted only by the
[declaration form](#properties-declaration-form). A negative `Index` is accepted without a diagnostic
in the expression form.

### UE.Time

```c
UE.Time([Period = <float>] [, IgnorePause = <bool>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| `Period` | numeric literal | absent — the node's period override stays off | no |
| `IgnorePause` | boolean literal | node default | no |

Node `UMaterialExpressionTime`; output `float1`. Supplying `Period` also turns on the node's period
override. No range check is applied: a negative `Period` is written through unchanged. The
[declaration form](#properties-declaration-form) rejects a negative `Period`.

### UE.Panner

```c
UE.Panner([Coordinate = <expr>] [, Time = <expr>] [, Speed = <expr>]
          [, SpeedX = <float>] [, SpeedY = <float>] [, FractionalPart = <bool>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| `Coordinate` | input pin | unconnected | no |
| `Time` | input pin | unconnected | no |
| `Speed` | input pin | unconnected | no |
| `SpeedX` | numeric literal | node default | no |
| `SpeedY` | numeric literal | node default | no |
| `FractionalPart` | boolean literal | node default | no |

Node `UMaterialExpressionPanner`; output `float2`. `ConstCoordinate` is not accepted in the expression
form; the [declaration form](#properties-declaration-form) accepts it, and the generic path can reach
it as `UE.Expression(Class = "Panner", OutputType = "float2", ConstCoordinate = 1)`.

### UE.WorldPosition

```c
UE.WorldPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionWorldPosition` | `float3` | none |

Absolute world position — the node's shader-offset mode is left at its default.
`ShaderOffsets` is not accepted here; use [`UE.TranslatedWorldPosition`](#uetranslatedworldposition)
for the camera-relative variant, or
`UE.Expression(Class = "WorldPosition", OutputType = "float3", WorldPositionShaderOffset = …)` for the
other modes. Also available as a [property declaration](#properties-declaration-form), which *does*
accept `ShaderOffsets`.

### UE.ObjectPositionWS

```c
UE.ObjectPositionWS()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectPositionWS` | `float3` | none |

Conditionally registered (see the note above). `UE.ObjectPosition` is an alias. The
[declaration form](#properties-declaration-form) accepts an `Origin` argument; the expression form
does not.

### UE.CameraVectorWS

```c
UE.CameraVectorWS()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionCameraVectorWS` | `float3` | none |

`UE.CameraVector` is an alias for the same node.

### UE.VertexNormalWS

```c
UE.VertexNormalWS()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionVertexNormalWS` | `float3` | none |

### UE.VertexTangentWS

```c
UE.VertexTangentWS()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionVertexTangentWS` | `float3` | none |

### UE.ScreenPosition

```c
UE.ScreenPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionScreenPosition` | `float2` | none |

Conditionally registered (see the note above). Output 0 of the node is *ViewportUV*;
[`UE.ViewportUV`](#ueviewportuv) is an alias that reads the same output.

> [!NOTE]
> The two surfaces disagree on this one. In a `Graph` the result is **2** components. As a
> [property declaration](#properties-declaration-form) the parser declares **4**. Nothing reconciles
> them; write the `Graph` form when the width matters.

### UE.VertexColor

```c
UE.VertexColor()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionVertexColor` | `float4` | none |

### UE.PixelDepth

```c
UE.PixelDepth()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionPixelDepth` | `float1` | none |

A scene read for the current pixel: the node is created with no UV or offset input wired.

### UE.SceneDepth

```c
UE.SceneDepth()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionSceneDepth` | `float1` | none |

No-argument form only; the node's UV input is left unconnected. To wire one, use
`UE.Expression(Class = "SceneDepth", OutputType = "float1", Input = <uv>)`.

### UE.SceneColor

```c
UE.SceneColor()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionSceneColor` | `float4` | none |

### UE.TranslatedWorldPosition

```c
UE.TranslatedWorldPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionWorldPosition` with the camera-relative shader-offset mode forced | `float3` | none |

Equivalent to `GetTranslatedWorldPosition(Parameters)` in HLSL. The node's default mode is *absolute*
world position, so the builtin overrides it; this is the only difference from
[`UE.WorldPosition`](#ueworldposition).

### UE.ObjectPosition

```c
UE.ObjectPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectPositionWS` | `float3` | none |

Alias of [`UE.ObjectPositionWS`](#ueobjectpositionws), conditionally registered on the same terms.

### UE.ObjectRadius

```c
UE.ObjectRadius()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectRadius` | `float1` | none |

### UE.ObjectBounds

```c
UE.ObjectBounds()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectBounds` | `float3` | none |

### UE.CameraVector

```c
UE.CameraVector()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionCameraVectorWS` | `float3` | none |

Alias of [`UE.CameraVectorWS`](#uecameravectorws).

### UE.CameraPosition

```c
UE.CameraPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionCameraPositionWS` | `float3` | none |

### UE.ReflectionVector

```c
UE.ReflectionVector()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionReflectionVectorWS` | `float3` | none |

### UE.PixelNormalWS

```c
UE.PixelNormalWS()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionPixelNormalWS` | `float3` | none |

### UE.TwoSidedSign

```c
UE.TwoSidedSign()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionTwoSidedSign` | `float1` | none |

### UE.PerInstanceRandom

```c
UE.PerInstanceRandom()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionPerInstanceRandom` | `float1` | none |

### UE.PerInstanceFadeAmount

```c
UE.PerInstanceFadeAmount()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionPerInstanceFadeAmount` | `float1` | none |

### UE.ViewportUV

```c
UE.ViewportUV()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionScreenPosition` | `float2` | none |

Conditionally registered (see the note above). The engine has no dedicated *ViewportUV* expression
class; this is [`UE.ScreenPosition`](#uescreenposition) under a second name, reading the node's
output 0.

### UE.TransformVector

```c
UE.TransformVector({ Input = <expr> | <expr> } [, Source = <basis>] [, Destination = <basis>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| **`Input`** | input pin; may be given positionally at index 0 | — | **yes** |
| `Source` | text literal | `"Tangent"` | no |
| `Destination` | text literal | `"World"` | no |

Node `UMaterialExpressionTransform`; output `float3`.

Both basis arguments accept the same six-token vocabulary, case-insensitively:

| Token(s) | Basis |
| :-- | :-- |
| `tangent` | tangent space |
| `local` | local space |
| `world`, `absoluteworld` | absolute world space |
| `view` | view space |
| `camera` | camera space |
| `instance`, `particle`, `instanceparticle` | instance / particle space |

Either token failing to resolve produces the single message
`UE.TransformVector Source/Destination is invalid.` — it does not say which one. Full basis reference:
[Transform bases](transform.md).

### UE.TransformPosition

```c
UE.TransformPosition({ Input = <expr> | <expr> } [, Source = <basis>] [, Destination = <basis>]
                     [, PeriodicWorldTileSize = <expr>] [, FirstPersonInterpolationAlpha = <expr>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| **`Input`** | input pin; may be given positionally at index 0 | — | **yes** |
| `Source` | text literal | `"Local"` | no |
| `Destination` | text literal | `"World"` | no |
| `PeriodicWorldTileSize` *(since UE 5.5)* | input pin | unconnected | no |
| `FirstPersonInterpolationAlpha` *(since UE 5.6)* | input pin | unconnected | no |

Node `UMaterialExpressionTransformPosition`; output `float3`.

Both basis arguments accept this vocabulary, case-insensitively:

| Token(s) | Basis | Requires |
| :-- | :-- | :-- |
| `local` | local space | — |
| `world`, `absoluteworld` | absolute world space | — |
| `periodicworld` | periodic world space | UE 5.5 |
| `translatedworld`, `camerarelativeworld` | camera-relative world space | — |
| `firstperson`, `firstpersontranslatedworld` | first-person translated world space | UE 5.6 |
| `view` | view space | — |
| `camera` | camera space | — |
| `instance`, `particle`, `instanceparticle` | instance / particle space | — |

A version-gated token below its engine version does not resolve, and the failure surfaces as the
generic `UE.TransformPosition Source/Destination is invalid.`

> [!WARNING]
> Below UE 5.5, `PeriodicWorldTileSize` is **silently dropped**: the argument is neither applied nor
> diagnosed. `FirstPersonInterpolationAlpha` behaves differently — below UE 5.6 it is a hard error.

## Special-cased builtins

These three names are resolved before the registered table and do not follow its rules.

### UE.StaticSwitchParameter

*(since 1.2.3)*

```c
UE.StaticSwitchParameter(Name = "<parameter-name>",
                         { True = <expr> | <expr> }, { False = <expr> | <expr> }
                         [, Default = <bool>] [, Group = "<text>"]
                         [, Description = "<text>"] [, SortPriority = <int>])
```

| Argument | Aliases | Kind | Default | Required |
| :-- | :-- | :-- | :-- | :-- |
| **`Name`** | `ParameterName` | text literal, non-blank after trimming | — | **yes** |
| **`True`** | `A`, positional index 0 | value | — | **yes** |
| **`False`** | `B`, positional index 1 | value | — | **yes** |
| `Default` | `DefaultValue` | boolean literal | `false` | no |
| `Group` | — | text literal | none | no |
| `Description` | — | text literal | none | no |
| `SortPriority` | — | integer literal | node default | no |

Node `UMaterialExpressionStaticSwitchParameter` at canvas X = 520. The output takes its component
count and its `MaterialAttributes` flag from the `True` branch. Both branches must agree: neither may
be a texture object or a Substrate value, they may not mix `MaterialAttributes` with numeric values,
and their component counts must be equal.

The parameter is registered on the generated **material** with an editor-only static-switch value, so
it appears in the material instance editor; inside a material function nothing is registered. A node
already registered under the same property name is reused instead of a second node being created.

> [!NOTE]
> `Group` and `Description` are read with the tolerant text handler: if the value is not a text
> literal, the argument is **discarded without a diagnostic**. `SortPriority` and `Default` do
> diagnose a malformed value.

### UE.CollectionParam

*(since 1.2.3)*

```c
UE.CollectionParam(Collection = Path(<root>, "<asset>"), Parameter = "<name>"
                   [, Group = "<text>"] [, SortPriority = <int>] [, Description = "<text>"])
```

`UE.CollectionParameter` is an accepted second spelling of the same builtin.

| Argument | Aliases | Kind | Default | Required |
| :-- | :-- | :-- | :-- | :-- |
| **`Collection`** | `Asset` | `Path(…)` call or an Unreal object path | — | **yes** |
| **`Parameter`** | `ParameterName` | text literal, non-blank | — | **yes** |
| `Group` *(since UE 5.7)* | — | text literal | none | no |
| `SortPriority` *(since UE 5.7)* | — | integer literal | node default | no |
| `Description` | — | text literal | none | no |

Node `UMaterialExpressionCollectionParameter` at canvas X = **-520** — the only builtin placed at a
negative X. The referenced `UMaterialParameterCollection` is loaded at generation time and the
parameter is looked up by name in it.

| Parameter kind found in the collection | Output |
| :-- | :-- |
| vector | `float4` |
| scalar | `float1` |
| neither | error |

> [!NOTE]
> Unlike every builtin in the [catalogue](#catalogue), this one's output width is **not** marked
> authoritative — and neither is [`UE.StaticSwitchParameter`](#uestaticswitchparameter)'s. Neither can
> act as the widening partner in a mixed-width binary operator; see
> [Conversions](../graph/conversions.md#authoritative-component-counts).

> [!WARNING]
> Below UE 5.7, `Group` and `SortPriority` are **silently dropped**. `SortPriority` is still parsed
> and still diagnoses a non-integer value on every engine version — it simply has no effect. The
> node's `ExpressionGUID` is likewise only seeded on UE 5.7+.

`Description` is written to the node's description field on every version.
`Path(…)` grammar and its accepted roots: [`Path(…)`](../parameters/path.md).

### UE.SceneTexture

```c
UE.SceneTexture(Id = "<scene-texture-id>")
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| **`Id`** | text literal | — | **yes** |

Pure sugar, resolved before every other `UE.` name. It rewrites the call to

```c
UE.Expression(Class = "SceneTexture", OutputType = "float4", SceneTextureId = <Id>)
```

and evaluates that, so the resulting node is `UMaterialExpressionSceneTexture` with a `float4` output
and all the [generic-path rules](ue-expression.md) apply from there.

The call must have **exactly one** argument and it must be named `Id`; anything else fails with
`UE.SceneTexture expects exactly Id="..." (e.g. Id="PostProcessInput0").`

The `Id` text is resolved by the reflected enum writer, which accepts the entry name with or without
its enum prefix, the fully qualified name, and the display name, ignoring case and the characters
` `, `_`, `-`, `:`, `.`, `/`. All of `"PostProcessInput0"`, `"PPI_PostProcessInput0"` and
`"ppi postprocessinput0"` select the same value. See
[value parsing](ue-expression.md#enum-values).

## Properties declaration form

`UE.<Name>(…)` may also stand where a type token would in a [`Properties`](../language/properties.md)
section. This is a **separate implementation** with a smaller catalogue and different rules.

```c
Properties {
    UE.TexCoord(Index = 0) UV;
    UE.CollectionParam(Collection = Path(Game, "MPC_Weather"), Parameter = "Wind") Wind;
}
```

| Rule | Declaration form | Expression form |
| :-- | :-- | :-- |
| Argument syntax | `Key=Value` text pairs; values are unquoted and trimmed, keys lower-cased | full expressions, including nested calls |
| Unknown argument name | **error** | silently ignored |
| Duplicate argument name | **error** | last one wins |
| Positional arguments | not expressible | accepted by three builtins |
| Argument order | lost — arguments are stored in a map | preserved |
| Inline default (`= …`) | **error** | not applicable |
| Node canvas X | -800 | 520 (or -520 for `UE.CollectionParam`) |
| Output width | from the parser's own table, or `OutputType` | from the node |

Recognized builtins, with the argument names each accepts. An argument outside its row is rejected
with `UE.{Name} for property '{Property}' does not support argument '{Argument}'.`

| `UE.Name` | Node class | Accepted arguments | Differences from the expression form |
| :-- | :-- | :-- | :-- |
| `TexCoord` | `TextureCoordinate` | `Index`, `CoordinateIndex`, `UTiling`, `VTiling`, `UnMirrorU`, `UnMirrorV` | `CoordinateIndex` is accepted; supplying both spellings is an error; a negative index is an error |
| `Time` | `Time` | `Period`, `IgnorePause` | `Period` must be ≥ 0 |
| `Panner` | `Panner` | `Coordinate`, `Time`, `Speed`, `SpeedX`, `SpeedY`, `ConstCoordinate`, `FractionalPart` | `ConstCoordinate` is accepted; `Coordinate` accepts *either* an integer (written to `ConstCoordinate`) or a property reference |
| `WorldPosition` | `WorldPosition` | `ShaderOffsets` | `ShaderOffsets` is accepted |
| `ObjectPositionWS` | `ObjectPositionWS` | `Origin` | `Origin` is accepted |
| `CameraVectorWS` | `CameraVectorWS` | none | any argument is an error, rather than ignored |
| `ScreenPosition` | `ScreenPosition` | none | declared as **4** components, not 2 |
| `VertexColor` | `VertexColor` | none | any argument is an error |
| `CollectionParam`, `CollectionParameter` | `CollectionParameter` | `Collection`, `Asset`, `Parameter`, `ParameterName`, `OutputType`, `ResultType` | no `Group` / `SortPriority` / `Description` arguments |
| *(any other name)* | resolved by reflection | requires `OutputType` or `ResultType` | see [generic declarations](ue-expression.md#declaration-form) |

`ShaderOffsets` vocabulary (lower-cased, spaces removed):

| Token(s) | Meaning |
| :-- | :-- |
| `default`, `includingshaderoffsets`, `absolute` | absolute world position including shader offsets |
| `excludeallshaderoffsets`, `excludingallshaderoffsets`, `nooffsets` | absolute world position, offsets excluded |
| `camerarelative` | camera-relative world position |
| `camerarelativenooffsets`, `camerarelativeexcludeoffsets` | camera-relative world position, offsets excluded |

`Origin` vocabulary (lower-cased, spaces removed):

| Token(s) | Meaning |
| :-- | :-- |
| `absolute`, `world` | absolute origin |
| `camerarelative` | camera-relative origin |

### Declared output width

The declared width of a `UE.*` property comes from the first of these that resolves:

1. an explicit `OutputType` or `ResultType` argument — see [`OutputType`](output-type.md), which lists
   the reduced token set this surface accepts;
2. the parser's own name table, below;
3. `CollectionParam` / `CollectionParameter` → scalar, 1 component;
4. otherwise the declaration is rejected.

| Name(s) | Declared as |
| :-- | :-- |
| `TexCoord`, `Panner` | vector, 2 |
| `Time` | scalar, 1 |
| `WorldPosition`, `CameraVectorWS`, `ObjectPositionWS`, `VertexNormalWS`, `VertexTangentWS` | vector, 3 |
| `ScreenPosition`, `VertexColor` | vector, 4 |

Any other name without an `OutputType` fails with
`Unsupported UE builtin function '{Name}'. Use OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture" for generic MaterialExpression calls.`

> [!NOTE]
> This table names ten builtins. It is not the expression-form catalogue and does not track it, and it
> is not the same list as the recognized-builtins table above — `VertexNormalWS` and `VertexTangentWS`
> get a declared width here but have no node-creation branch, so like `SceneColor`, `PixelDepth`,
> `TranslatedWorldPosition` and the rest of the 27 they are only reachable as declarations through an
> explicit `OutputType`. Without one the declaration parses and then fails at generation with
> `This builtin is not implemented by the material generator yet. …`

A `const` qualifier is rejected on a `UE.*` property with
`Const property '{Name}' must use a plain scalar, vector, or texture type instead of a parameter node or UE builtin declaration.`

## Notes

- A `UE.*` call may be swizzled directly: `UE.TexCoord(Index = 0).x` is a postfix chain. See
  [Swizzle](../graph/swizzle.md#swizzling-a-call-result).
- Two identical **generic** `UE.*` calls in one `Graph` produce **one** node; that reuse key is built
  from the class, the normalized `OutputType` text and every non-reserved argument. The registered
  builtins in the [catalogue](#catalogue), `UE.StaticSwitchParameter` and `UE.CollectionParam` consult
  no such cache — each call creates its own node. `UE.SceneTexture` desugars to a generic call and so
  does take part. See [Node reuse](../graph/node-reuse.md).
- `UE.*` names are resolved before user declarations, so a property or function named `TexCoord` does
  not shadow `UE.TexCoord` — but nothing prevents a property named `TexCoord` from existing and being
  read as a bare identifier. See [Name resolution](../graph/name-resolution.md).
- A registered builtin never accepts `OutputType`, `Class`, `Output`, `OutputName` or `OutputIndex`.
  Those arguments are read only on the [generic path](ue-expression.md); on a registered builtin they
  are silently discarded like any other unknown name.
- Inside a [`GraphFunction`](../language/graph-function.md), `UE.*` calls are lifted out of the HLSL
  body and become generated input pins on the emitted Custom node.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table; the compiler emits the
substituted text. `{Function}` is the builtin name **as the author spelled it**, with the author's
casing preserved — the one exception is `Failed to create UE.{Function}.`, which prints the builtin's
registered spelling instead.

### Expression form

| Message | Cause |
| :-- | :-- |
| `Failed to create UE.{Function}.` | the node could not be created |
| `UE.{Function} {Argument} must be an integer literal.` | a non-integer value for an integer argument |
| `UE.{Function} {Argument} must be a numeric literal.` | a non-numeric value for a scalar argument |
| `UE.{Function} {Argument} must be a boolean literal.` | a value other than `true` / `false` |
| `UE.{Function} {Argument} must be a text value.` | a non-text value for a text argument |
| `UE.{Function} requires parameter: {Argument}` | a required input was given neither by name nor positionally |
| `UE.{Function} Period must be a numeric literal.` | `UE.Time` with a non-numeric `Period` |
| `UE.TransformVector Source/Destination is invalid.` | either basis token is not in the vector vocabulary |
| `UE.TransformPosition Source/Destination is invalid.` | either basis token is not in the position vocabulary, including a token gated above the running engine version |
| `UE.TransformPosition FirstPersonInterpolationAlpha requires Unreal Engine 5.6 or newer.` | the argument was supplied on UE 5.3 – 5.5 |
| `UE.SceneTexture expects exactly Id="..." (e.g. Id="PostProcessInput0").` | not exactly one argument, or the argument is not named `Id` |

### `UE.StaticSwitchParameter`

| Message | Cause |
| :-- | :-- |
| `UE.StaticSwitchParameter requires Name="ParameterName".` | neither `Name` nor `ParameterName` given |
| `UE.StaticSwitchParameter Name must be a text value.` | the name is not a text literal, or is blank after trimming |
| `UE.StaticSwitchParameter Default/DefaultValue must be true or false.` | non-boolean default |
| `UE.StaticSwitchParameter SortPriority must be an integer literal.` | non-integer sort priority |
| `StaticSwitchParameter '{Name}' requires True=... and False=... inputs.` | a branch is missing |
| `StaticSwitchParameter '{Name}' True input: {Message}` | the `True` expression failed to evaluate |
| `StaticSwitchParameter '{Name}' False input: {Message}` | the `False` expression failed to evaluate |
| `StaticSwitchParameter '{Name}' cannot switch Texture object values.` | a branch is a texture object |
| `StaticSwitchParameter '{Name}' cannot switch Substrate values.` | a branch is a Substrate value |
| `StaticSwitchParameter '{Name}' cannot mix MaterialAttributes and numeric branches.` | one branch is `MaterialAttributes`, the other is not |
| `StaticSwitchParameter '{Name}' branches must have the same component count, got {Left} and {Right}.` | branch widths differ |
| `Failed to create StaticSwitchParameter node '{Name}'.` | node creation failed |
| `StaticSwitchParameter '{Name}': {Message}` | the metadata could not be applied to the node |

### `UE.CollectionParam`

The message text says `UE.CollectionParam` regardless of which of the two spellings was written.

| Message | Cause |
| :-- | :-- |
| `UE.CollectionParam requires Collection=Path(...).` | neither `Collection` nor `Asset` given |
| `UE.CollectionParam Collection must be Path(...) or an Unreal object path.` | the value is not an asset reference |
| `UE.CollectionParam Collection is invalid: {Message}` | the asset reference did not resolve |
| `UE.CollectionParam could not load MaterialParameterCollection '{Path}'.` | the asset loaded as something else, or not at all |
| `UE.CollectionParam requires Parameter="Name".` | neither `Parameter` nor `ParameterName` given |
| `UE.CollectionParam Parameter must be a text value.` | non-text or blank parameter name |
| `UE.CollectionParam collection '{Collection}' does not contain parameter '{Name}'.` | the name is neither a scalar nor a vector parameter of that collection |
| `Failed to create UE.CollectionParam node.` | node creation failed |
| `UE.CollectionParam SortPriority must be an integer literal.` | non-integer sort priority |

### Declaration form

Every generation-time message from this surface is wrapped as
`UE.{Function} for property '{Property}': {Message}`.

| Message | Cause |
| :-- | :-- |
| `UE builtin property declarations must specify a function name, for example UE.TexCoord UV.` | `UE.` with nothing after it |
| `Invalid UE builtin declaration '{Text}'.` | the declaration is not `UE.<Name>` or `UE.<Name>( … )` |
| `Unexpected characters after UE builtin argument list in '{Text}'.` | text follows the closing `)` |
| `UE builtin argument '{Argument}' must use named syntax like Key=Value in '{Text}'.` | an argument with no `=` |
| `Invalid UE builtin argument '{Argument}' in '{Text}'.` | an empty key or an empty value |
| `UE builtin argument '{Key}' is declared more than once in '{Text}'.` | duplicate key |
| `UE builtin property '{Property}' does not support inline defaults. Put arguments inside UE.{Function}(...).` | `UE.TexCoord(…) UV = 1;` |
| `Unsupported UE builtin function '{Function}'. Use OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture" for generic MaterialExpression calls.` | the name is not in the parser's table and no `OutputType` was given |
| `UE.{Function} for property '{Property}' does not support argument '{Argument}'.` | an argument outside the builtin's accepted set |
| `Use either Index or CoordinateIndex, not both.` | `UE.TexCoord` with both spellings |
| `'{Value}' is not a valid non-negative UV channel index.` | `UE.TexCoord` index is negative or unparseable |
| `UTiling value '{Value}' is invalid.` / `VTiling value '{Value}' is invalid.` | non-numeric tiling |
| `UnMirrorU value '{Value}' is invalid.` / `UnMirrorV value '{Value}' is invalid.` | non-boolean mirror flag |
| `Period value '{Value}' is invalid.` | `UE.Time` period is non-numeric or negative |
| `IgnorePause value '{Value}' is invalid.` | non-boolean |
| `Coordinate input is invalid. {Message}` | `UE.Panner` coordinate is neither an integer nor a resolvable property reference |
| `ConstCoordinate value '{Value}' is invalid.` | non-integer |
| `Time input is invalid. {Message}` / `Speed input is invalid. {Message}` | the referenced input did not resolve |
| `SpeedX value '{Value}' is invalid.` / `SpeedY value '{Value}' is invalid.` | non-numeric |
| `FractionalPart value '{Value}' is invalid.` | non-boolean |
| `ShaderOffsets value '{Value}' is invalid.` | not in the `ShaderOffsets` vocabulary |
| `Origin value '{Value}' is invalid.` | not in the `Origin` vocabulary |
| `CameraVectorWS does not take any arguments.` | any argument on `UE.CameraVectorWS` |
| `ScreenPosition does not take any arguments.` | any argument on `UE.ScreenPosition` |
| `VertexColor does not take any arguments.` | any argument on `UE.VertexColor` |
| `CollectionParam requires Collection=Path(...).` | no collection argument |
| `Collection is invalid: {Message}` | the asset reference did not resolve |
| `Could not load MaterialParameterCollection '{Path}'.` | load failure |
| `CollectionParam requires Parameter="Name".` | no parameter argument |
| `Collection '{Collection}' does not contain parameter '{Name}'.` | unknown parameter |
| `Failed to create the native {Node} node.` | node creation failed; `{Node}` is `TexCoord`, `Time`, `Panner`, `WorldPosition`, `ObjectPositionWS`, `CameraVectorWS`, `ScreenPosition`, `VertexColor` or `CollectionParameter` |
| `This builtin is not implemented by the material generator yet. For generic MaterialExpression support, add OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture".` | a name outside the table reached generation with no `OutputType` |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_UEBuiltins")
{
    Properties {
        vec3 Tint = vec3(1.0, 0.6, 0.2);
        UE.TexCoord(Index = 0) UV0;
    }

    Settings {
        Domain       = "UI";
        ShadingModel = "Unlit";
    }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        float2 uv    = UE.Panner(Coordinate = UV0, Time = UE.Time(), SpeedX = 0.1, SpeedY = 0.0);
        float3 wp    = UE.TranslatedWorldPosition();
        float3 nrm   = UE.TransformVector(UE.VertexNormalWS(), Source = "World", Destination = "Tangent");
        float  fade  = UE.PerInstanceFadeAmount();
        float4 vcol  = UE.VertexColor();

        Color = (vec3(uv.x, uv.y, wp.z) + nrm) * Tint * vcol.rgb * fade;
    }
}
```

Generated nodes:

```text
TextureCoordinate  (Index 0)                  <- UV0        (Properties, X = -800)
Panner             (Coordinate, Time, SpeedX) <- uv         (X = 520)
Time                                          <- Panner.Time
WorldPosition      (camera-relative)          <- wp
VertexNormalWS                                -> Transform
Transform          (World -> Tangent)         <- nrm
PerInstanceFadeAmount                         <- fade
VertexColor                                   <- vcol
```

## See also

- [Builtins](index.md) — the five call surfaces and how a callee is dispatched
- [`UE.Expression`](ue-expression.md) — every name that is *not* in this catalogue
- [`OutputType`](output-type.md) — the token table, including the reduced declaration-form set
- [Transform bases](transform.md) — the full basis vocabulary reference
- [Substrate](substrate.md) — the sibling namespace and its UE 5.4 gate
- [Math builtins](math.md) — `pow`, `fmod`, `min`, `max` and the rest
- [Properties](../language/properties.md) — the section grammar the declaration form lives in
- [Parameter nodes](../parameters/parameter-nodes.md) — the explicit `*Parameter` tokens
- [`Path(…)`](../parameters/path.md) — asset references for `UE.CollectionParam`
- [Node reuse](../graph/node-reuse.md) — which call surfaces collapse two identical calls into one node
- [Conversions](../graph/conversions.md) — authoritative component counts and widening
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
