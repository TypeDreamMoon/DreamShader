# UE builtins

> [DreamShader](../index.md) » [Builtins](index.md) » **UE builtins**

The 27 names in the `UE.` namespace that 1.x implemented itself, each with a fixed argument list, and
the three it special-cased. Since 2.0.0 every `UE.` name — these included — is a node of the
reflected builtin catalog; what the 1.x names keep is their 1.x argument lists, which the legacy
front end applies to a `.dsm` / `.dsf` / `.dsh` before the binder sees the call.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body (expression form) or a `Properties { … }` section (declaration form) |
| Kind | builtin namespace |
| Generates | one `UMaterialExpression` per call; identical calls are one node |

## Synopsis

```c
// expression form, inside Graph
UE.<Name> ( [ <argument> [ , <argument> ] … ] )

<argument> := <arg-name> = <expression>

// declaration form, inside Properties
UE.<Name> [ ( <key> = <value> [ , <key> = <value> ] … ) ] <property-name> ;
```

The namespace prefix is matched exactly *(since 2.0.0)*: in a `Graph`, `ue.TexCoord()` reads `ue` as a
variable and fails with [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200). The builtin name is matched
exactly, and in a 1.x source a name that matches only when case is ignored is accepted with the
warning [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) — `UE.texcoord()` is `UE.TexCoord()`. Argument
names are trimmed and matched the same way, and are otherwise exact — `Un_Mirror_U` is not
`UnMirrorU`.

Each name is a class of the catalog: its short name (the class name without `MaterialExpression`) or
one of the 1.x aliases the catalog carries — `TexCoord`, `ObjectPosition`, `CameraVector`,
`CameraPosition`, `ReflectionVector`, `ViewportUV`, `TransformVector`. Any other engine class is
reached the same way, by its short name ([`UE.Expression`](ue-expression.md)); a name the running
engine has no class for is [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210).

> [!WARNING]
> **An argument a 1.x name did not read is dropped, with a warning.** For the 27 names in the
> [catalogue](#catalogue), the legacy front end keeps the argument list of each entry below and drops
> every other argument — an unknown or misspelled name, or a positional argument — with
> [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) *(since 2.0.0; 1.x dropped them without a word)*.
> `UE.TexCoord(Indx = 3)` still produces UV channel 0, and says why; `UE.Panner(SpedX = 1)` pans at
> the node default speed and says so. The only positional argument kept is index 0 of
> `UE.TransformVector` and `UE.TransformPosition` (→ `Input`). An argument that passes the filter is
> bound against the catalog like the argument of any node call.

## Output widths

The type of a call is the catalog's — the engine's own description of the class, on the running
engine (`dsc export-catalog` writes it out):

- a masked output is as wide as its mask;
- an unmasked output has the engine's value type, or, where the engine says only "a float", a width
  DreamShader keeps for the class: 1 for `Time`, `PixelDepth`, `SceneDepth`, `ObjectRadius`,
  `TwoSidedSign`, `PerInstanceRandom`, `PerInstanceFadeAmount`; 2 for `TextureCoordinate`, `Panner`;
  3 for `ObjectPositionWS`, `ObjectBounds`, `CameraVectorWS`, `CameraPositionWS`,
  `ReflectionVectorWS`, `VertexNormalWS`, `VertexTangentWS`, `PixelNormalWS`, `Transform`,
  `TransformPosition`;
- an output the engine does not type at all is as wide as the place it is read into;
- a node with several outputs is read by naming one (`UE.ScreenPosition().ViewportUV`), or used
  whole where its first output — or the value its channel outputs make up (`float4 VC =
  UE.VertexColor();`) — fits exactly. In a 1.x source, a node whose first output has no fixed width
  is read as that output ([`DSH5287`](../diagnostics/DSH5xxx.md#dsh5287)).

None of these builtins produces a texture object, a `MaterialAttributes` value or a Substrate value.
See [Conversions](../graph/conversions.md).

## Catalogue

All 27 1.x names, in 1.x registration order. The *Output* column is what the UE 5.8 catalog gives.

| Builtin | `UMaterialExpression` class | Output | 1.x arguments |
| :-- | :-- | :-- | :-- |
| [`UE.TexCoord`](#uetexcoord) | `TextureCoordinate` | `float2` | `Index`, `UTiling`, `VTiling`, `UnMirrorU`, `UnMirrorV` |
| [`UE.Time`](#uetime) | `Time` | `float1` | `Period`, `IgnorePause` |
| [`UE.Panner`](#uepanner) | `Panner` | `float2` | `Coordinate`, `Time`, `Speed`, `SpeedX`, `SpeedY`, `FractionalPart` |
| [`UE.WorldPosition`](#ueworldposition) | `WorldPosition` | `XYZ` `float3`, also `XY`, `Z` | none |
| [`UE.ObjectPositionWS`](#ueobjectpositionws) | `ObjectPositionWS` | `float3` | none |
| [`UE.CameraVectorWS`](#uecameravectorws) | `CameraVectorWS` | `float3` | none |
| [`UE.VertexNormalWS`](#uevertexnormalws) | `VertexNormalWS` | `float3` | none |
| [`UE.VertexTangentWS`](#uevertextangentws) | `VertexTangentWS` | `float3` | none |
| [`UE.ScreenPosition`](#uescreenposition) | `ScreenPosition` | `ViewportUV`, `PixelPosition` | none |
| [`UE.VertexColor`](#uevertexcolor) | `VertexColor` | `RGB`, `R`, `G`, `B`, `A`; whole `float4` | none |
| [`UE.PixelDepth`](#uepixeldepth) | `PixelDepth` | `float1` | none |
| [`UE.SceneDepth`](#uescenedepth) | `SceneDepth` | `float1` | none |
| [`UE.SceneColor`](#uescenecolor) | `SceneColor` | `RGB`, `A`; whole `float4` | none |
| [`UE.TranslatedWorldPosition`](#uetranslatedworldposition) | `WorldPosition` (camera-relative) | as `UE.WorldPosition` | none |
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
| [`UE.ViewportUV`](#ueviewportuv) | `ScreenPosition` | as `UE.ScreenPosition` | none |
| [`UE.TransformVector`](#uetransformvector) | `Transform` | `float3` | **`Input`**, `Source`, `Destination` |
| [`UE.TransformPosition`](#uetransformposition) | `TransformPosition` | `float3` | **`Input`**, `Source`, `Destination`, `PeriodicWorldTileSize`, `FirstPersonInterpolationAlpha` |

Three further names are handled apart and documented under
[Special-cased builtins](#special-cased-builtins): `UE.StaticSwitchParameter`, `UE.CollectionParam` /
`UE.CollectionParameter`, and `UE.SceneTexture`.

Class names are written without the `MaterialExpression` prefix in the table above.

> [!NOTE]
> 1.x registered `UE.ObjectPositionWS`, `UE.ObjectPosition`, `UE.ScreenPosition` and `UE.ViewportUV`
> only where their class resolved on the running editor. The catalog lists every class the running
> engine has, so nothing is registered conditionally *(since 2.0.0)*.

### UE.TexCoord

```c
UE.TexCoord([Index = <int>] [, UTiling = <float>] [, VTiling = <float>]
            [, UnMirrorU = <bool>] [, UnMirrorV = <bool>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| `Index` | constant — the property `CoordinateIndex` | node default (UV channel 0) | no |
| `UTiling` | constant | node default | no |
| `VTiling` | constant | node default | no |
| `UnMirrorU` | constant `true` / `false` | node default | no |
| `UnMirrorV` | constant `true` / `false` | node default | no |

Node `UMaterialExpressionTextureCoordinate`; output `float2`.

`Index` is the catalog's alias of `CoordinateIndex`. `CoordinateIndex` itself is not on the 1.x list,
so in this form it is dropped with `DSH5254`; the [declaration form](#properties-declaration-form)
takes both. A negative `Index` is written through without a diagnostic.

### UE.Time

```c
UE.Time([Period = <float>] [, IgnorePause = <bool>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| `Period` | constant | absent — the node's period override stays off | no |
| `IgnorePause` | constant `true` / `false` — the property `bIgnorePause` | node default | no |

Node `UMaterialExpressionTime`; output `float1`. Supplying `Period` also turns on the node's period
override: the front end adds `bOverride_Period = true` to the call. No range check is applied: a
negative `Period` is written through unchanged — in the
[declaration form](#properties-declaration-form) too *(since 2.0.0)*.

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
| `SpeedX` | constant | node default | no |
| `SpeedY` | constant | node default | no |
| `FractionalPart` | constant `true` / `false` — the property `bFractionalPart` | node default | no |

Node `UMaterialExpressionPanner`; output `float2`. `ConstCoordinate` is dropped with `DSH5254` in this
form; the [declaration form](#properties-declaration-form) passes it through, and so does the class
name: `UE.Expression(Class = "Panner", ConstCoordinate = 1)`.

### UE.WorldPosition

```c
UE.WorldPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionWorldPosition` | `XYZ` (`float3`), also `XY` and `Z` | none |

Absolute world position — the node's shader-offset mode is left at its default. `ShaderOffsets` is
dropped with `DSH5254` here; use [`UE.TranslatedWorldPosition`](#uetranslatedworldposition) for the
camera-relative variant, or `UE.Expression(Class = "WorldPosition", WorldPositionShaderOffset = …)`
for the other modes. The [property declaration](#properties-declaration-form) does take
`ShaderOffsets`.

### UE.ObjectPositionWS

```c
UE.ObjectPositionWS()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectPositionWS` | `float3` | none |

`UE.ObjectPosition` is an alias. The [declaration form](#properties-declaration-form) takes an
`Origin` argument; the expression form drops it with `DSH5254`.

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
| `UMaterialExpressionScreenPosition` | `ViewportUV`, `PixelPosition` | none |

The node has two outputs, which the engine does not type: `UE.ScreenPosition().ViewportUV` is as wide
as the place it is read into. Used as a plain value in a 1.x source the call is its first output,
*ViewportUV* (`DSH5287`). [`UE.ViewportUV`](#ueviewportuv) is an alias of the class.

### UE.VertexColor

```c
UE.VertexColor()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionVertexColor` | `RGB`, `R`, `G`, `B`, `A`; whole `float4` | none |

The outputs are channel views of one colour: `float4 c = UE.VertexColor();` is the whole colour, and
`UE.VertexColor().a` is the `A` output.

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

No-argument form only; an argument is dropped with `DSH5254` and the node's UV input is left
unconnected. To wire one, use `UE.Expression(Class = "SceneDepth", Input = <uv>)`.

### UE.SceneColor

```c
UE.SceneColor()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionSceneColor` | `RGB`, `A`; whole `float4` | none |

### UE.TranslatedWorldPosition

```c
UE.TranslatedWorldPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionWorldPosition` with the camera-relative shader-offset mode forced | as [`UE.WorldPosition`](#ueworldposition) | none |

Equivalent to `GetTranslatedWorldPosition(Parameters)` in HLSL. The front end rewrites the call to
`UE.WorldPosition` with `WorldPositionShaderOffset` set to the camera-relative mode, after dropping
any argument with `DSH5254`; the node's default mode is *absolute* world position, which is the only
difference from [`UE.WorldPosition`](#ueworldposition). The catalog has no class of this name.

### UE.ObjectPosition

```c
UE.ObjectPosition()
```

| Node | Output | Arguments |
| :-- | :-- | :-- |
| `UMaterialExpressionObjectPositionWS` | `float3` | none |

Alias of [`UE.ObjectPositionWS`](#ueobjectpositionws).

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
| `UMaterialExpressionScreenPosition` | as [`UE.ScreenPosition`](#uescreenposition) | none |

The engine has no dedicated *ViewportUV* expression class; this is
[`UE.ScreenPosition`](#uescreenposition) under a second name — the catalog's alias of the class —
and *ViewportUV* is the node's first output.

### UE.TransformVector

```c
UE.TransformVector({ Input = <expr> | <expr> } [, Source = <basis>] [, Destination = <basis>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| **`Input`** | input pin; may be given positionally at index 0 | — | **yes** |
| `Source` | enum value — the property `TransformSourceType` | `Tangent` (the node's) | no |
| `Destination` | enum value — the property `TransformType` | `World` (the node's) | no |

Node `UMaterialExpressionTransform`; output `float3`.

A basis is a value of the engine's enum, written without its prefix — `Tangent`, `Local`, `World`,
`View`, `Camera`, `Instance` — and, in a 1.x source, also with its prefix (`TRANSFORMSOURCE_World`)
or in another case, with the warning [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278). The 1.x
spellings that are no value of the enum — `AbsoluteWorld`, `Particle`, `InstanceParticle` — are
[`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215) *(since 2.0.0)*, which names the property that failed —
`TransformSourceType` for `Source`, `TransformType` for `Destination`. Full basis reference:
[Transform bases](transform.md).

### UE.TransformPosition

```c
UE.TransformPosition({ Input = <expr> | <expr> } [, Source = <basis>] [, Destination = <basis>]
                     [, PeriodicWorldTileSize = <expr>] [, FirstPersonInterpolationAlpha = <expr>])
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| **`Input`** | input pin; may be given positionally at index 0 | — | **yes** |
| `Source` | enum value — the property `TransformSourceType` | `Local` (the node's) | no |
| `Destination` | enum value — the property `TransformType` | the node's: `Local` on UE 5.8 | no |
| `PeriodicWorldTileSize` | input pin, where the engine's class has it | unconnected | no |
| `FirstPersonInterpolationAlpha` | input pin, where the engine's class has it | unconnected | no |

Node `UMaterialExpressionTransformPosition`; output `float3`.

A basis is a value of the engine's enum, written without its prefix — `Local`, `World`,
`TranslatedWorld`, `View`, `Camera`, `Instance`, and on the engines that have them `PeriodicWorld`
and `FirstPersonTranslatedWorld` — and, in a 1.x source, also with its prefix or in another case
(`DSH5278`). `AbsoluteWorld`, `CameraRelativeWorld`, `FirstPerson`, `Particle` and
`InstanceParticle` are `DSH5215` *(since 2.0.0)*, and so is a value the running engine's enum does
not have.

> [!WARNING]
> **The two optional pins follow the engine.** On an engine whose class lacks a pin, the argument is
> no pin of the node: in a 1.x source it is kept with the info
> [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291), and building the node fails with
> [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) *(since 2.0.0; 1.x dropped `PeriodicWorldTileSize`
> silently and refused `FirstPersonInterpolationAlpha` with an error of its own)*.

> [!NOTE]
> 1.x defaulted `Destination` to `World`. The front end writes no default *(since 2.0.0)*: a call
> without `Destination` gets the node's own, which is `Local` on UE 5.8. Write `Destination` out.

## Special-cased builtins

`UE.StaticSwitchParameter` and `UE.SceneTexture` are rewritten by the legacy front end into
`UE.Expression` calls before the binder sees them. `UE.CollectionParam` is not: in a `Graph` body only
the engine's name, `UE.CollectionParameter`, resolves.

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
| **`Name`** | `ParameterName` | a quoted string or a word, non-blank after trimming | — | **yes** |
| **`True`** | `A`, positional index 0 | value | — | **yes** |
| **`False`** | `B`, positional index 1 | value | — | **yes** |
| `Default` | `DefaultValue` | `true` / `false` | `false` | no |
| `Group` | — | a quoted string or a word | none | no |
| `Description` | — | a quoted string or a word | none | no |
| `SortPriority` | — | whole number | node default | no |

The call becomes `UE.Expression(Class = "StaticSwitchParameter", ParameterName = …, DefaultValue = …,
Group = …, Desc = …, SortPriority = …, A = <True>, B = <False>)`; any other argument is dropped with
`DSH5254`. Node `UMaterialExpressionStaticSwitchParameter`.

The output follows the branches: it is as wide as the wider numeric branch, or the Substrate or
`MaterialAttributes` value a branch carries. The two branches are not compared with each other
*(since 2.0.0)*; a texture in either is [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214).

The parameter is registered with an editor-only static-switch value on the generated material — or
material function — so it appears in the instance editor. Two calls with the same arguments are one
node.

> [!NOTE]
> `Group` and `Description` are written to the node's `Group` and `Desc` properties; a value that is
> neither a quoted string nor a word is [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224)
> *(since 2.0.0; 1.x discarded it without a diagnostic)*.

### UE.CollectionParam

*(since 1.2.3)*

```c
UE.CollectionParameter(Collection = Path(<root>, "<asset>"), Parameter = "<name>"
                       [, Group = "<text>"] [, SortPriority = <int>] [, Desc = "<text>"])
```

In a `Graph` body `UE.CollectionParam` is [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210)
*(since 2.0.0)*: the catalog knows the class by its engine name, `UE.CollectionParameter`, only. The
[declaration form](#properties-declaration-form) still takes both spellings.

| Argument | Aliases | Kind | Default | Required |
| :-- | :-- | :-- | :-- | :-- |
| **`Collection`** | `Asset` | `Path(…)` or an Unreal object path | — | **yes** |
| **`Parameter`** | `ParameterName` | a quoted string or a word | — | **yes** |
| `Group` | — | a quoted string or a word, where the engine's class has it | none | no |
| `SortPriority` | — | whole number, where the engine's class has it | node default | no |
| `Desc` | — | a quoted string or a word | none | no |

Node `UMaterialExpressionCollectionParameter`. The collection is loaded and the parameter looked up
when the node is built: a collection that does not resolve or load is
[`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213), a name that is neither a scalar nor a vector parameter
of it is [`DSH8254`](../diagnostics/DSH8xxx.md#dsh8254). A call without `Collection` or `Parameter` is
not checked *(since 2.0.0)*. `Description` is not an argument of this call — the node's description
property is `Desc` — and is [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213).

The engine does not type the node's output, so the value is as wide as the place it is read into
*(since 2.0.0; 1.x read the width off the loaded collection: `float4` for a vector parameter, `float1`
for a scalar)*. The [declaration form](#properties-declaration-form) declares it `float4` unless it
says otherwise.

> [!WARNING]
> `Group` and `SortPriority` are taken only where the running engine's class has them. On an engine
> whose class lacks them, `Group` is `DSH5213`, and `SortPriority` — a number — is kept with
> `DSH5291` and fails with `DSH8212` when the node is built *(since 2.0.0; 1.x dropped both silently
> below its version gate)*.

`Path(…)` grammar and its accepted roots: [`Path(…)`](../parameters/path.md).

### UE.SceneTexture

```c
UE.SceneTexture(Id = "<scene-texture-id>")
```

| Argument | Kind | Default | Required |
| :-- | :-- | :-- | :-- |
| **`Id`** | enum value | — | **yes** |

Pure sugar. The front end rewrites the call to

```c
UE.Expression(Class = "SceneTexture", SceneTextureId = <Id>)[0]
```

— output 0 of `UMaterialExpressionSceneTexture`, *Color*, a `float4`. The call must have **exactly
one** argument and it must be named `Id`; anything else, an output selector included, is
[`DSH5255`](../diagnostics/DSH5xxx.md#dsh5255). The node's other properties and its `Size` /
`InvSize` outputs need `UE.Expression(Class = "SceneTexture", …)`.

The `Id` is a value of `ESceneTextureId`, written without its prefix — `"PostProcessInput0"`. In a
1.x source the prefixed name (`"PPI_PostProcessInput0"`), the `ESceneTextureId::` scope and another
case are accepted with `DSH5278`; a display name, or a spelling with spaces in it
(`"ppi postprocessinput0"`), is `DSH5215` *(since 2.0.0)*. See
[enum values](ue-expression.md#enum-values).

## Properties declaration form

`UE.<Name>(…)` may also stand where a type token would in a [`Properties`](../language/properties.md)
section. It declares no parameter. The legacy front end keeps the text and, in every `Graph` body that
reads the property, declares a local of that name at the head of the body, initialised with the call —
`float2 UV = UE.TexCoord(Index = 0);` — one per such body, in property order. A body that does not
read the property gets no node. From there the call is an ordinary `UE.` call, bound against the
catalog *(since 2.0.0)*.

```c
Properties {
    UE.TexCoord(Index = 0) UV;
    UE.CollectionParam(Collection = Path(Game, "MPC_Weather"), Parameter = "Wind") Wind;
}
```

| Rule | Declaration form | Expression form |
| :-- | :-- | :-- |
| `UE` | any case | exact |
| Argument syntax | `Key = Value` pairs split at top-level commas; a value is a quoted string, a number, `true` / `false`, a bare word (read as a name: another variable, or an enum value), or else kept as text, as `Path(…)` is | full expressions, including nested calls |
| The 1.x argument filter (`DSH5254`) | not applied | applied to the 27 names |
| Unknown argument | [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213); a number is kept with `DSH5291` and fails with `DSH8212` when the node is built | dropped with `DSH5254` on the 27 names |
| Duplicate argument, or both spellings of one (`Index` and `CoordinateIndex`) | [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | same |
| A piece without `=`, an empty key or an empty value | skipped without a diagnostic | — |
| Inline default (`= …`) | [`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) | not applicable |
| Metadata `[ … ]` | read and not applied | not applicable |
| Renamed arguments | `Description` → `Desc`; on `CollectionParam` / `CollectionParameter` `Asset` → `Collection` and `Parameter` → `ParameterName`, and the call is `UE.CollectionParameter` | none |
| `OutputType` / `ResultType` | the type of the local, not passed on — see [Declared output width](#declared-output-width) | see [`OutputType`](output-type.md) |

The 1.x names, as this form takes them:

| `UE.Name` | Node class | Arguments | Differences from the expression form |
| :-- | :-- | :-- | :-- |
| `TexCoord` | `TextureCoordinate` | `Index` or `CoordinateIndex`, `UTiling`, `VTiling`, `UnMirrorU`, `UnMirrorV` | `CoordinateIndex` is taken; a negative index is not checked *(since 2.0.0)* |
| `Time` | `Time` | `Period`, `IgnorePause` | `Period` is not range-checked, and it does not turn on the period override — write `bOverride_Period = true` *(since 2.0.0)* |
| `Panner` | `Panner` | `Coordinate`, `Time`, `Speed` (pins: a word is another variable, a number a constant on the pin), `ConstCoordinate`, `SpeedX`, `SpeedY`, `FractionalPart` | `ConstCoordinate` is taken |
| `WorldPosition` | `WorldPosition` | `ShaderOffsets` — the property `WorldPositionShaderOffset` | `ShaderOffsets` is taken |
| `ObjectPositionWS` | `ObjectPositionWS` | `Origin` — the property `OriginType` | `Origin` is taken |
| `CameraVectorWS`, `ScreenPosition`, `VertexColor` | as named | none of their own | an argument is an unknown argument |
| `CollectionParam`, `CollectionParameter` | `CollectionParameter` | `Collection` / `Asset`, `Parameter` / `ParameterName`, and `Group`, `SortPriority`, `Description` where the class has them | |
| *(any other name)* | the class of that name | its pins and properties | needs no `OutputType` *(since 2.0.0)* |

`ShaderOffsets` takes the values of `EWorldPositionIncludedOffsets` without their prefix — `Default`,
`ExcludeAllShaderOffsets`, `CameraRelative`, `CameraRelativeNoOffsets` — and, in another case or with
the `WPT_` prefix, with `DSH5278`. `Origin` takes `Absolute` and `CameraRelative`. The 1.x spellings
that are no value of the enum (`IncludingShaderOffsets`, `Absolute` and `NoOffsets` for
`ShaderOffsets`, `World` for `Origin`, …) are `DSH5215` *(since 2.0.0)*.

### Declared output width

The type of the local comes from the first of these that applies:

1. an `OutputType` or `ResultType` argument naming a type — see [`OutputType`](output-type.md);
2. the name, compared ignoring case: `TexCoord`, `Panner` → `float2`; `Time` → `float`;
   `WorldPosition`, `CameraVectorWS`, `ObjectPositionWS`, `VertexNormalWS`, `VertexTangentWS` →
   `float3`;
3. every other name → `float4`: `ScreenPosition`, `VertexColor`, `CollectionParam` /
   `CollectionParameter`, and any name outside the list *(since 2.0.0; 1.x rejected such a name
   without an `OutputType`)*.

The call initialises the local like any initializer: a scalar output is spread across the local's
width, a wider one is cut down ([`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289)), and a narrower
vector — a `float3` node under a name outside the list — is
[`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228). An output the engine does not type, such as a
collection parameter's, takes the local's width. Give such a declaration an `OutputType`; a scalar
collection parameter is `OutputType = float1`.

A `const` in front of a `UE.*` declaration is read and ignored *(since 2.0.0; 1.x rejected it)*.

## Notes

- A `UE.*` call may be swizzled directly: `UE.TexCoord(Index = 0).x` is a postfix chain. On a node of
  channel outputs a swizzle that one output publishes is that output (`UE.VertexColor().a`). See
  [Swizzle](../graph/swizzle.md#swizzling-a-call-result).
- Two identical calls in one graph are **one** node, whichever builtin they are: the IR merges nodes
  with the same class, properties and inputs *(since 2.0.0; 1.x merged only the generic and
  `Substrate.*` calls)*. See [Node reuse](../graph/node-reuse.md).
- A property, local or parameter named `TexCoord` does not shadow `UE.TexCoord`; one named `UE` does —
  `UE.X(…)` is then a member call on that variable *(since 2.0.0)*. See
  [Name resolution](../graph/name-resolution.md).
- On the 27 names, `OutputType`, `ResultType` and `Class` are dropped with `DSH5254` like any argument
  the name does not read. `Output`, `OutputName` and `OutputIndex` select an output of the node, as on
  every call *(since 2.0.0)*.
- Inside a [`GraphFunction`](../language/graph-function.md), `UE.*` calls are lifted out of the HLSL
  body and become generated input pins on the emitted Custom node.

## Diagnostics

Every code is listed with its message and its full description on its page in
[Diagnostics](../diagnostics/index.md).

### Expression form

| Code | Raised when |
| :-- | :-- |
| [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) (warning) | an argument one of the 27 names does not read — unknown, misspelled or positional — is dropped |
| [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) (warning) | in a 1.x source, a builtin, argument or output name matches only when case is ignored |
| [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200) | the namespace is not spelled `UE` |
| [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) | the name after `UE.` is no class of the running engine — `UE.CollectionParam` among them |
| [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) | an argument that passed the filter names no pin or property of the class |
| [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) (info) | in a 1.x source, such an argument carries a value, and is kept for the built node to name as a pin |
| [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224) | a property is given something that is not a constant, or a text property something that is neither a quoted string nor a word |
| [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215) | an enum value — a basis, a scene texture id — is no value of the engine's enum |
| [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) (warning) | an enum value is written the 1.x way: with its prefix or scope, or in another case |
| [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) | a value does not fit the pin it feeds |
| [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | a pin or property is given twice |
| [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279) (warning) | a required pin — the `Input` of a transform — is left unconnected |
| [`DSH5255`](../diagnostics/DSH5xxx.md#dsh5255) | `UE.SceneTexture` has not exactly one argument, `Id` |
| [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) | the built node has no pin of a name kept with `DSH5291` |
| [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213) | the node could not take a property value; the message carries the reason (`DSH7132`–`DSH7143`) |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the node could not be created |

### `UE.StaticSwitchParameter`

| Code | Raised when |
| :-- | :-- |
| [`DSH5257`](../diagnostics/DSH5xxx.md#dsh5257) | neither `Name` nor `ParameterName` is given, or it is blank |
| [`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258) | a branch is missing, both branches are the same argument, or the call carries an output selector |
| [`DSH5263`](../diagnostics/DSH5xxx.md#dsh5263) | `Default` / `DefaultValue` is not `true` or `false` |
| [`DSH5264`](../diagnostics/DSH5xxx.md#dsh5264) | `SortPriority` is not a whole number |
| `DSH5254` (warning) | any other argument is dropped |
| `DSH5224` | `Group` or `Description` is neither a quoted string nor a word |
| `DSH5214` | a branch is a texture |

### `UE.CollectionParameter`

| Code | Raised when |
| :-- | :-- |
| `DSH5210` | the call is spelled `UE.CollectionParam` |
| `DSH5213` | `Description`, or `Group` on an engine whose class has none |
| `DSH5291` / `DSH8212` | `SortPriority` on an engine whose class has none |
| `DSH5224` | `Parameter` is neither a quoted string nor a word |
| `DSH8213` | the collection does not resolve, does not load, or is not a `MaterialParameterCollection` |
| `DSH8254` | the collection has no scalar or vector parameter of that name |

### Declaration form

| Code | Raised when |
| :-- | :-- |
| [`DSH3259`](../diagnostics/DSH3xxx.md#dsh3259) | a default follows the property name, or the argument list is not closed |
| [`DSH3250`](../diagnostics/DSH3xxx.md#dsh3250) | no property name follows the builtin |
| `DSH5210` | the name is no class of the running engine |
| `DSH5213`, `DSH5291` / `DSH8212` | an argument the class has no pin or property for |
| `DSH4215` | a pin or property is given twice |
| `DSH5215`, `DSH5278` | an enum value — `ShaderOffsets`, `Origin` — as in the expression form |
| `DSH5224` | a property value that is not a constant |
| `DSH8213`, `DSH8254` | as for `UE.CollectionParameter` |

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
TextureCoordinate  (CoordinateIndex 0)        <- UV0        (a local at the head of Graph)
Panner             (Coordinate, Time, SpeedX) <- uv
Time                                          <- Panner.Time
WorldPosition      (camera-relative)          <- wp
VertexNormalWS                                -> Transform
Transform          (World -> Tangent)         <- nrm
PerInstanceFadeAmount                         <- fade
VertexColor                                   <- vcol
```

## See also

- [Builtins](index.md) — the call surfaces and how a callee is resolved
- [`UE.Expression`](ue-expression.md) — every engine class by its name
- [`OutputType`](output-type.md) — what the argument still does in a 1.x source
- [Transform bases](transform.md) — the basis values
- [Substrate](substrate.md) — the sibling namespace
- [Math builtins](math.md) — `pow`, `fmod`, `min`, `max` and the rest
- [Properties](../language/properties.md) — the section grammar the declaration form lives in
- [Parameter nodes](../parameters/parameter-nodes.md) — the explicit `*Parameter` tokens
- [`Path(…)`](../parameters/path.md) — asset references for `UE.CollectionParameter`
- [Node reuse](../graph/node-reuse.md) — identical calls and nodes
- [Conversions](../graph/conversions.md) — widening and cutting down
- [`dsc migrate`](../tools/migrate.md) — the `.dss` spelling of each 1.x call
- [Diagnostics index](../diagnostics/index.md) — every code
