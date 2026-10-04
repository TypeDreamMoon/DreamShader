# Using parameters in Graph

> [DreamShader](../index.md) » [Parameters](index.md) » **Using parameters in Graph**

How a declared property is consumed inside a `Graph` block: as a value, as a swizzled value, or as a
call that wires the generated node's input pins.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding, or an `Outputs` declaration default |
| Kind | expression form |
| Generates | a declaration's one node; for a token expanded at its uses, one node per distinct use |
| Since | `1.2.3` (`StaticSwitchParameter` calls); `1.4.1` (input-pin call form) |

## Synopsis

```c
<name>                                  // value read
<name> . <channels>                     // value read, then a swizzle
<name> ( <pin> = <expression> [ , <pin> = <expression> ] … )   // pin call form
```

The pin call form has no positional variant: every argument must be named
([`DSH5259`](../diagnostics/DSH5xxx.md#dsh5259)).

## Value reads

A property is one of two things *(since 2.0.0)* — see
[Parameter node tokens](parameter-nodes.md#the-22-tokens):

- a **declaration** (every compact token, `ScalarParameter`, `StaticBoolParameter`, `VectorParameter`,
  `TextureObjectParameter`): a file-scope `uniform` or `static const`, one node however often it is
  read;
- a token **expanded at each use** (`StaticSwitchParameter`, the two masks, the texture-sample
  tokens): every read or call builds the node there, and uses that are written the same way share one
  node.

A property name is looked up ignoring case, as in 1.x; a read of a declaration that matches only in
case is [`DSH5275`](../diagnostics/DSH5xxx.md#dsh5275). A `Graph` variable of the same name hides a
declaration for the rest of the body ([`DSH4251`](../diagnostics/DSH4xxx.md#dsh4251), warning).

| Property kind | What a read is |
| :-- | :-- |
| A scalar declaration | the `ScalarParameter`'s value |
| A 2- or 3-component vector declaration | the `VectorParameter`'s `RG` / `RGB` output |
| A 4-component vector declaration, `VectorParameter` | the `RGBA` output |
| A `const` declaration | the constant node's whole value |
| A texture declaration | a texture object |
| `ChannelMaskParameter` | its one output, **1** component |
| `StaticComponentMaskParameter` | its output, as wide as its input |
| A texture-sample token (15–20) | the sample's `RGBA` output, 4 components |
| `RuntimeVirtualTextureSampleParameter`, `SparseVolumeTextureSampleParameter` | a node with several outputs |
| `StaticSwitchParameter` | not a value — see [below](#staticswitchparameter) |

The component count that drives this table is the one the declaration produced, so `vec2 P` reads `RG`
while `VectorParameter P` reads `RGBA`.

- **Declaration order does not matter** for reads. A `Graph` may reference a property declared further
  down the block: the `Graph` is read after every other section.
- A declaration is declared at **file scope** *(since 2.0.0)*: any block of the file can read it.
  A token expanded at its uses belongs to its own block.
- A property nothing reads builds no node ([`DSH4390`](../diagnostics/DSH4xxx.md#dsh4390), info).
- A `UE.*` property is a local declared at the head of each body that reads it, in `Properties` order.
  An argument naming another `UE.*` property needs that one earlier in the list and read by the same
  body; otherwise the name is not declared ([`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)).

A swizzle applies to the read value, not to the node: `Tint.rgb`, `Sample.a`, `UV.yx`. See
[Swizzle](../graph/swizzle.md).

## The pin call form

```c
vec4 S = BaseTex(Coordinates = UV);
vec4 M = Keep(Input = S);
```

The call builds the property's node at this use, with the default and metadata of its declaration, and
wires each named argument to an input pin. *(since 2.0.0)* The node belongs to the call: two calls with
the same arguments share one node, but a bare read of the same property elsewhere is a node of its
own, without the wiring — 1.x cached one node per property and kept the first call's connections for
every later read.

Each argument name is matched against the class's input pins as the engine catalog lists them, then
against its properties; a match only ignoring case is [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276).
In a 1.x source, a name the catalog does not list but whose value is a number is taken for a pin named
after the node's settings ([`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291), info): the emitter connects
it to the live node's pin of that display name — or of the display name with every character an
identifier cannot hold turned into `_` — or refuses it ([`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212)).
Any other unknown name is [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213).

Argument values are ordinary expressions, converted to the pin's type; one that does not convert is
[`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214). An output selector (`Output = …`, `OutputIndex = …`) on
the call is [`DSH5265`](../diagnostics/DSH5xxx.md#dsh5265).

### Eligible parameter types

The call form is for the tokens expanded at their uses:

```text
ChannelMaskParameter                  TextureSampleParameterCubeArray
StaticComponentMaskParameter          TextureSampleParameterVolume
TextureSampleParameter2D              TextureSampleParameterSubUV
TextureSampleParameter2DArray         RuntimeVirtualTextureSampleParameter
TextureSampleParameterCube            SparseVolumeTextureSampleParameter
```

`StaticSwitchParameter` has its own call form, described [below](#staticswitchparameter). A call on a
declaration — every compact token, `ScalarParameter`, `StaticBoolParameter`, `VectorParameter`,
`TextureObjectParameter` — is [`DSH4208`](../diagnostics/DSH4xxx.md#dsh4208): it names no function.

### Pin names by parameter type

The catalog lists a pin under the engine's C++ name for it, with the display name in identifier form
as an alias where the two differ. *(since 2.0.0)* Every pin is reachable that way — 1.x matched display
names only, and those with a space or parentheses could not be written.

| Parameter type | Pins the catalog lists | Display names a 1.x call can still use (`DSH5291`) |
| :-- | :-- | :-- |
| `ChannelMaskParameter` | `Input` | — |
| `StaticComponentMaskParameter` | `Input` | — |
| `TextureSampleParameter2D` … `TextureSampleParameterSubUV` | `Coordinates`, `TextureObject`, `MipValue`, `CoordinatesDX`, `CoordinatesDY`, `AutomaticViewMipBiasValue` (alias `Apply_View_MipBias`) | `MipLevel` under `[MipValueMode="MipLevel"]`, `MipBias` under `[MipValueMode="MipBias"]`, `DDX_UVs_` / `DDY_UVs_` under `[MipValueMode="Derivative"]` |
| `RuntimeVirtualTextureSampleParameter` | `Coordinates`, `WorldPosition` (alias `World_Position`), `MipValue`, `DDX`, `DDY` | the names the node shows for its settings |
| `SparseVolumeTextureSampleParameter` | `Coordinates`, `TextureObject`, `MipValue`, `CoordinatesDX`, `CoordinatesDY` | — |
| `StaticSwitchParameter` | **separate form**: `A` (alias `True`), `B` (alias `False`) | — |
| Every declaration | no call | — |

> [!WARNING]
> **Leave `TextureObject` unwired on a texture-sample *parameter* node.** Unreal's
> `UMaterialExpressionTextureSampleParameter` constructor clears `bShowTextureInputPin`, and a loaded
> asset drops any connection to the hidden pin. Set the asset with the default or with
> `[Texture=Path(…)]`. `SparseVolumeTextureSampleParameter` is the exception — it does expose that pin.

### Types that look callable but are not

| Token | Why the call fails |
| :-- | :-- |
| `CurveAtlasRowParameter`, `FontSampleParameter`, `SpriteTextureSampler`, `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | The token has no 2.0 form at all ([`DSH3253`](../diagnostics/DSH3xxx.md#dsh3253)) |
| `TextureObjectParameter`, compact texture tokens | A texture object has no input pins; read it as a value and feed it to a sampler (a call is `DSH4208`) |
| `ScalarParameter`, `VectorParameter`, `StaticBoolParameter`, compact tokens | No input pins to wire (a call is `DSH4208`) |

## StaticSwitchParameter

```c
vec3 C = UseDetail(True = DetailColor, False = BaseColor);
vec3 D = UseDetail(A = DetailColor, B = BaseColor);
vec3 E = UseDetail(DetailColor, BaseColor);          // positional, in that order
```

| Argument | Accepted spellings |
| :-- | :-- |
| true branch | `True=`, else `A=`, else the first positional argument |
| false branch | `False=`, else `B=`, else the second positional argument |

Both branches are required ([`DSH5258`](../diagnostics/DSH5xxx.md#dsh5258)); any other argument is
dropped with a [`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254) warning, and an output selector is
`DSH5265`.

The branches are wired to the node's `A` and `B` pins, which take any width. The result is as wide as
the wider branch, or the material or Substrate value a branch carries. *(since 2.0.0)* The 1.x checks
on the branches — no texture object, no `Substrate` or `MaterialAttributes` mixed with a number, equal
component counts — are not made as such: a branch the pin cannot take is `DSH5214`.

The node's `DefaultValue` is the declaration's default (`true` or `false`; the node's own `false` when
there is none), and the switch is registered as a static parameter of the material.

> [!WARNING]
> A `StaticSwitchParameter` **cannot be read as a value**. A bare `UseDetail` — or `UseDetail.r` — is
> no declared name ([`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200)). It must be called.

## Other surfaces

- **`UE.*` properties** are read like any other property: `UE.TexCoord(Index = 0) UV;` in
  `Properties`, then `UV` in `Graph`. Each becomes a local at the head of the body that reads it, built
  once there. See [UE builtins](../builtins/ue.md).
- **The anonymous switch builtin.** `UE.StaticSwitchParameter(Name = …, Default = …, Group = …,
  Description = …, SortPriority = …)` builds the same node in place, so a static switch does not have
  to be declared in `Properties` first. `ParameterName` may stand for `Name` and `DefaultValue` for
  `Default`; the branches are given as above. No name is
  [`DSH5257`](../diagnostics/DSH5xxx.md#dsh5257), a default that is not `true` / `false`
  [`DSH5263`](../diagnostics/DSH5xxx.md#dsh5263), a non-integer `SortPriority`
  [`DSH5264`](../diagnostics/DSH5xxx.md#dsh5264); any other argument is dropped with `DSH5254`.
- **The ThinCustom backend** builds the same node graph as `Graph`, on the hidden base material, so a
  property reaches it exactly as described above. See [Backend](../settings/backend.md).

## Notes

- Node positions come from the [graph layout](../generation/graph-layout.md) the project selects
  (*Graph Layout Style*), not from fixed coordinates *(since 2.0.0)*.
- A name that matches both a `Graph` variable and a declaration resolves to the variable, with
  `DSH4251`. See [Name resolution](../graph/name-resolution.md).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH4200` | a bare read of a `StaticSwitchParameter`, or a name that is no variable and no property |
| `DSH4208` | a call on a property that is a declaration |
| `DSH5259` | a positional argument in the pin call form |
| `DSH5265` | `Output` / `OutputIndex` on a parameter or static-switch call |
| `DSH5213` | an argument that names neither a pin nor a property of the class |
| `DSH5291` | an argument that names a pin the catalog does not list (info; connected on the live node) |
| `DSH8212` | the live node has no pin of that name either |
| `DSH5276` | an argument that matches a pin or property only ignoring case (warning) |
| `DSH5214` | an argument the pin's type cannot take |
| `DSH5258` | a static switch called without both branches |
| `DSH5254` | an argument a static switch does not read (warning; dropped) |
| `DSH5257` / `DSH5263` / `DSH5264` | `UE.StaticSwitchParameter` without a name / with a non-boolean default / with a non-integer sort priority |
| `DSH4251` | a `Graph` variable hides a declaration (warning) |

The complete list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="Docs/M_GraphUsage")
{
    Properties = {
        TextureSampleParameter2D BaseTex = Path(Game, "Textures/T_White");

        TextureSampleParameter2D HeightTex = Path(Game, "Textures/T_Height") [
            MipValueMode = "MipLevel"
        ];

        ChannelMaskParameter         Pick = float4(1, 0, 0, 0);
        StaticComponentMaskParameter Keep [DefaultR = true; DefaultG = true; DefaultB = true];

        StaticSwitchParameter UseDetail = true;

        vec3  Tint   = vec3(1.0, 0.8, 0.6);
        float Detail = 0.25;

        UE.TexCoord(Index = 0) UV;
    }

    Settings = { Domain = "Surface"; ShadingModel = "DefaultLit"; BlendMode = "Opaque"; }

    Outputs = {
        vec3  Color;
        float Rough;

        Base.BaseColor = Color;
        Base.Roughness = Rough;
    }

    Graph = {
        vec4  Albedo = BaseTex(Coordinates = UV);           // Coordinates pin
        vec4  High   = HeightTex(Coordinates = UV,
                                 MipValue    = 2.0);        // the mip level, as MipValueMode says
        vec4  Masked = Keep(Input = Albedo);                // Input pin, 4 components out
        float Chan   = Pick(Input = High);                  // Input pin, 1 component out

        vec3 Plain    = Albedo.rgb * Tint;                  // a variable read + swizzle
        vec3 Detailed = Masked.rgb * Detail;

        Color = UseDetail(True = Detailed, False = Plain);
        Rough = Chan;
    }
}
```

Generated nodes:

```text
TextureSampleParameter2D  BaseTex     Coordinates <- TextureCoordinate UV
TextureSampleParameter2D  HeightTex   MipValueMode=TMVM_MipLevel, Coordinates <- UV, MipValue <- Constant(2)
StaticComponentMaskParameter Keep     DefaultR/G/B=true, Input <- BaseTex
ChannelMaskParameter      Pick        Input <- HeightTex
TextureCoordinate         UV          CoordinateIndex=0   (one node, shared by both samplers)
VectorParameter           Tint        read through RGB
ScalarParameter           Detail
StaticSwitchParameter     UseDetail   True <- Multiply(Keep.rgb, Detail), False <- Multiply(BaseTex.rgb, Tint)
```

`MipLevel = 2.0`, the pin's display name under this `MipValueMode`, reaches the same pin with
`DSH5291`. *(since 2.0.0)* A `Graph` variable may not be called `Base` in a `Shader`: that is the
material the `Outputs` bind, and a local of that name is
[`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220).

## See also

- [Parameters](index.md) — the hub and the decision table
- [Parameter node tokens](parameter-nodes.md) — which token generates which node, and its pins
- [Compact type tokens](compact-types.md) — the declared component counts this page reads from
- [Metadata block](metadata.md) — the keys that change which pins exist
- [SamplerType](sampler-type.md) — `MipValueMode` and the other texture-sample keys
- [Calls](../graph/calls.md) — the general call grammar and named-argument rules
- [Swizzle](../graph/swizzle.md) — `.rgb`, `.a` and channel masks on a read value
- [Conversions](../graph/conversions.md) — component-count compatibility
- [Node reuse](../graph/node-reuse.md) — when two uses share one node
- [Name resolution](../graph/name-resolution.md) — variable-before-property lookup order
- [MaterialAttributes](../graph/material-attributes.md) — the value kind a branch may carry
- [UE builtins](../builtins/ue.md) — `UE.*` properties and the anonymous static-switch builtin
- [Backend](../settings/backend.md) — what the two backends build
