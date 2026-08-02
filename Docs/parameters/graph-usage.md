# Using parameters in Graph

> [DreamShader](../index.md) » [Parameters](index.md) » **Using parameters in Graph**

How a declared property is consumed inside a `Graph` block: as a value, as a swizzled value, or as a
call that wires the generated node's input pins.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, an `Outputs` binding, or an `Outputs` declaration default |
| Kind | expression form |
| Generates | nothing new — the property's node is created on first use and then reused |
| Since | `1.2.3` (`StaticSwitchParameter` calls); `1.4.1` (input-pin call form) |

## Synopsis

```c
<name>                                  // value read
<name> . <channels>                     // value read, then a swizzle
<name> ( <pin> = <expression> [ , <pin> = <expression> ] … )   // pin call form
```

The pin call form has no positional variant: every argument must be named.

## Value reads

A bare identifier is resolved in this order: an already-bound `Graph` variable, then a property, then
the literals `true` / `false`. Property lookup is **case-insensitive**, and inside a material function
the function's own `Properties` are searched before the enclosing `Shader`'s.

| Property kind | Node output the read targets |
| :-- | :-- |
| Any scalar property | output 0 — the named-output remap applies to vector-typed properties only |
| Vector, 1 component (`ChannelMaskParameter`) | output named `R`, else output 0 |
| Vector, 2 components | output named `RG`, else output 0 |
| Vector, 3 components | output named `RGB`, else output 0 |
| Vector, 4 components | output named `RGBA`, else output 0 |
| Any `const` property | output 0 — the named-output remap is skipped entirely |
| Any texture property | output 0; the value reports **0** components and is marked as a texture object |

The component count that drives this table is the one the declaration produced, so `vec2 P` reads `RG`
while `VectorParameter P` reads `RGBA`. `ChannelMaskParameter` reads as **1** component and
`CurveAtlasRowParameter` as **3**, despite generating four-channel-looking nodes.

Reads are lazy and cached:

- The node is created the first time the property is referenced, so **declaration order does not
  matter** for reads. A `Graph` may reference a property declared further down the block.
- Every reference to the same property returns the same node. A property used ten times generates one
  node. See [Node reuse](../graph/node-reuse.md).
- The one ordering constraint is a `UE.*` declaration whose argument references another property:
  that referent must exist in the same `Properties` list. A cycle fails with
  `Property '{Name}' has a recursive UE builtin dependency.`

A swizzle applies to the read value, not to the node: `Tint.rgb`, `Sample.a`, `UV.yx`. See
[Swizzle](../graph/swizzle.md).

## The pin call form

```c
vec4 S = BaseTex(Coordinates = UV);
vec4 M = Keep(Input = S);
```

The node is materialised exactly as a bare read would materialise it — same cache, so a later bare
reference to the same property shares the configured node — and then each named argument is matched
against the node's input pins. Matching trims and lower-cases both the argument name and the engine's
pin name; nothing else is stripped, so a pin name containing a space or punctuation can never be
matched by an argument identifier.

Argument values are ordinary expressions and must be numeric: passing a texture object,
a `MaterialAttributes` value or a `Substrate` value fails with
`Parameter '{Name}' input '{Arg}' must be a numeric value.`

### Eligible parameter types

Exactly ten tokens are routed to the pin-wiring evaluator:

```text
ChannelMaskParameter                  TextureSampleParameterCubeArray
StaticComponentMaskParameter          TextureSampleParameterVolume
TextureSampleParameter2D              TextureSampleParameterSubUV
TextureSampleParameter2DArray         RuntimeVirtualTextureSampleParameter
TextureSampleParameterCube            SparseVolumeTextureSampleParameter
```

`StaticSwitchParameter` has its own call form, described [below](#staticswitchparameter). Every other
property — every compact token and the remaining eleven `*Parameter` tokens — is not routed here at
all: a call on one of them falls through to the function-call dispatcher and fails as an unknown
function.

### Pin names by parameter type

Pin names come from the engine node at generation time, so what is reachable depends on the node
**and** on metadata applied before the call — metadata is applied when the node is created, which is
before any pin is wired.

| Parameter type | Callable | Always available | Available only under metadata | Exposed by the engine but unreachable |
| :-- | :-- | :-- | :-- | :-- |
| `ChannelMaskParameter` | yes | `Input` | — | — |
| `StaticComponentMaskParameter` | yes | `Input` | — | — |
| `TextureSampleParameter2D` | yes | `Coordinates` | `MipLevel` with `[MipValueMode="MipLevel"]`; `MipBias` with `[MipValueMode="MipBias"]` | the two derivative pins under `[MipValueMode="Derivative"]`, and the automatic-view-mip-bias pin |
| `TextureSampleParameter2DArray` | yes | `Coordinates` | as above | as above |
| `TextureSampleParameterCube` | yes | `Coordinates` | as above | as above |
| `TextureSampleParameterCubeArray` | yes | `Coordinates` | as above | as above |
| `TextureSampleParameterVolume` | yes | `Coordinates` | as above | as above |
| `TextureSampleParameterSubUV` | yes | `Coordinates` | as above | as above |
| `RuntimeVirtualTextureSampleParameter` | yes | `Coordinates` | — | the world-position pin and all four derivative / mip pins, which the engine renames according to the node's own settings |
| `SparseVolumeTextureSampleParameter` | yes | `Coordinates`, `TextureObject` | `MipLevel` with `[MipValueMode="MipLevel"]`; `MipBias` with `[MipValueMode="MipBias"]` | the two derivative pins under `[MipValueMode="Derivative"]` |
| `StaticSwitchParameter` | **separate form** | `True` / `A`, `False` / `B` | — | — |
| `ScalarParameter`, `StaticBoolParameter`, `VectorParameter`, `DoubleVectorParameter`, `CurveAtlasRowParameter`, `DynamicParameter`, `FontSampleParameter`, `SpriteTextureSampler`, `TextureObjectParameter`, `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | no | — | — | — |
| Every compact scalar, vector and texture token | no | — | — | — |

> [!WARNING]
> **Several engine pin names cannot be written as an argument.** A DreamShaderLang argument name is an
> identifier, and matching only trims and lower-cases, so any pin whose engine name contains a space or
> parentheses is unreachable. The names in question are `Apply View MipBias`, `DDX(UVs)`, `DDY(UVs)` on
> texture-sample nodes, and `World Position`, `Translated World Position`, `Mip Level`, `Mip Bias`,
> `DDX (UV)`, `DDX (World)`, `DDY (UV)`, `DDY (World)` on the runtime-virtual-texture node. There is no
> workaround through the call form; set the corresponding value with metadata
> (`[ConstCoordinate=…]`, `[ConstMipValue=…]`, `[AutomaticViewMipBias=…]`) or build the node explicitly
> with [`UE.Expression`](../builtins/ue-expression.md).

> [!WARNING]
> **`TextureObject` is not a pin on a texture-sample *parameter* node.** Unreal's
> `UMaterialExpressionTextureSampleParameter` constructor clears `bShowTextureInputPin`, so
> `Tex(TextureObject = SomeTexture)` fails with
> `Parameter 'Tex' (TextureSampleParameter2D) has no input pin named 'TextureObject'. Asset slots (Texture/Curve/Font/...) are set via [TextureObject=Path(...)] metadata, not call arguments.`
> `SparseVolumeTextureSampleParameter` is the exception — it does expose that pin.

### Types that look callable but are not

| Token | Why the call fails |
| :-- | :-- |
| `CurveAtlasRowParameter` | The node has an `InputTime` pin, but the token is not in the eligible list, so `C(InputTime = t)` is never routed to the pin evaluator and fails as an unknown function |
| `FontSampleParameter` | Not in the eligible list; bind `Font` and `FontTexturePage` with metadata |
| `SpriteTextureSampler` | Not in the eligible list; bind `Texture` with metadata |
| `TextureObjectParameter` | A texture object has no input pins; read it as a value and feed it to a sampler |
| `TextureCollectionParameter`, `SparseVolumeTextureObjectParameter` | Object parameters, same as above |
| `ScalarParameter`, `VectorParameter`, compact tokens | No input pins to wire |

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

Both branches are required. Rules on the two branch values:

| Rule | Message when violated |
| :-- | :-- |
| Neither branch may be a texture object | `StaticSwitchParameter '{Name}' cannot switch Texture object values.` |
| Neither branch may be a `Substrate` value | `StaticSwitchParameter '{Name}' cannot switch Substrate values.` |
| The branches may not mix `MaterialAttributes` with numeric | `StaticSwitchParameter '{Name}' cannot mix MaterialAttributes and numeric branches.` |
| Both branches must have the same component count | `StaticSwitchParameter '{Name}' branches must have the same component count, got {Left} and {Right}.` |

The result has the branches' component count. The node's `DefaultValue` is the declaration's default
(`true` or `false`; `false` when there is none), an `ExpressionGUID` is minted if the node has none,
and on a `UMaterial` the material's static-parameter set is updated to match.

> [!WARNING]
> A `StaticSwitchParameter` **cannot be read as a value**. A bare `UseDetail` — or `UseDetail.r` —
> fails with `Unknown Graph identifier 'UseDetail'.` because the property-value path refuses this
> token. It must be called.

## Other surfaces

- **`UE.*` properties** are read exactly like any other property: `UE.TexCoord(Index = 0) UV;` in
  `Properties`, then `UV` in `Graph`. Their arguments are resolved at node-creation time, not at read
  time. See [UE builtins](../builtins/ue.md).
- **The anonymous switch builtin.** `UE.StaticSwitchParameter(Name = …, Default = …, Group = …,
  Description = …, SortPriority = …)` synthesises a property on the fly and runs the same evaluator, so
  a static switch does not have to be declared in `Properties` first.
- **The ThinCustom / HLSL backend.** When the shader body is compiled into an Unreal `Custom` node
  rather than a node graph, every property whose identifier textually appears in the prepared code
  becomes a `Custom` node input named after the property, wired from the same preferred output
  (`R` / `RG` / `RGB` / `RGBA`) that a graph read would use. Properties whose names do not appear in
  the code are skipped entirely. See [Backend](../settings/backend.md) and
  [Generated HLSL](../generation/generated-hlsl.md).

## Notes

- Property nodes are laid out at X = -800 with a Y stride of 220; a `StaticSwitchParameter` node is
  placed at X = 520. Constants land at X = -1120 — both the ones created for inline call arguments and
  the `Constant` / `Constant2Vector` / `Constant3Vector` / `Constant4Vector` node a `const` scalar or
  vector property generates. See [Graph layout](../generation/graph-layout.md).
- A name that matches both a `Graph` variable and a property resolves to the variable — the variable
  binding is checked first. See [Name resolution](../graph/name-resolution.md).
- Wiring a pin mutates the shared node. Because the node is cached by property name, a property
  configured once with `Tex(Coordinates = UV)` keeps that connection for every later bare reference to
  `Tex` in the same graph.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this table.

| Message | Cause |
| :-- | :-- |
| `Unknown Graph identifier '{Name}'.` | the name is neither a `Graph` variable, nor a resolvable property, nor `true` / `false` — including any bare read of a `StaticSwitchParameter` |
| `Property '{Name}': {Inner}` | the property's node could not be created |
| `Property '{Name}' has a recursive UE builtin dependency.` | a `UE.*` property argument chain refers back to itself |
| `Could not resolve parameter '{Name}' for configuration.` | the call form could not look the property up |
| `Parameter '{Name}' did not produce an expression node.` | the property resolved but generated no node |
| `Parameter '{Name}' must be called with named arguments wiring its input pins (e.g. {Name}(Coordinates=...) or {Name}(Input=...)).` | a positional argument in the pin call form |
| `Parameter '{Name}' ({Token}) has no input pin named '{Arg}'. Asset slots (Texture/Curve/Font/...) are set via [{Arg}=Path(...)] metadata, not call arguments.` | the argument name matched no pin on the node as currently configured |
| `Parameter '{Name}' input '{Arg}': {Inner}` | the argument's own expression failed to evaluate |
| `Parameter '{Name}' input '{Arg}' must be a numeric value.` | a texture object, `MaterialAttributes` or `Substrate` value passed to a pin |
| `StaticSwitchParameter '{Name}' requires True=... and False=... inputs.` | one or both branches missing |
| `StaticSwitchParameter '{Name}' True input: {Inner}` | the true branch failed to evaluate |
| `StaticSwitchParameter '{Name}' False input: {Inner}` | the false branch failed to evaluate |
| `StaticSwitchParameter '{Name}' cannot switch Texture object values.` | a branch is a texture object |
| `StaticSwitchParameter '{Name}' cannot switch Substrate values.` | a branch is a `Substrate` value |
| `StaticSwitchParameter '{Name}' cannot mix MaterialAttributes and numeric branches.` | one branch is `MaterialAttributes`, the other is not |
| `StaticSwitchParameter '{Name}' branches must have the same component count, got {Left} and {Right}.` | mismatched branch widths |
| `StaticSwitchParameter '{Name}': {Inner}` | metadata application failed on the switch node |
| `Failed to create StaticSwitchParameter node '{Name}'.` | node construction failed |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

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
        StaticComponentMaskParameter Keep = float4(1, 1, 1, 0);

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
        vec4  Base   = BaseTex(Coordinates = UV);           // Coordinates pin
        vec4  High   = HeightTex(Coordinates = UV,
                                 MipLevel    = 2.0);        // MipLevel exists because of the metadata
        vec4  Masked = Keep(Input = Base);                  // Input pin, 4 components out
        float Chan   = Pick(Input = High);                  // Input pin, 1 component out

        vec3 Plain    = Base.rgb * Tint;                    // bare read + swizzle, reuses the node
        vec3 Detailed = Masked.rgb * Detail;

        Color = UseDetail(True = Detailed, False = Plain);
        Rough = Chan;
    }
}
```

Generated nodes:

```text
TextureSampleParameter2D  BaseTex     Coordinates <- TextureCoordinate UV
TextureSampleParameter2D  HeightTex   MipValueMode=TMVM_MipLevel, Coordinates <- UV, MipLevel <- Constant(2)
StaticComponentMaskParameter Keep     Input <- BaseTex
ChannelMaskParameter      Pick        Input <- HeightTex
TextureCoordinate         UV          CoordinateIndex=0   (one node, shared by both samplers)
VectorParameter           Tint        read through RGB
ScalarParameter           Detail      read through R
StaticSwitchParameter     UseDetail   True <- Multiply(Keep.rgb, Detail), False <- Multiply(BaseTex.rgb, Tint)
```

## See also

- [Parameters](index.md) — the hub and the decision table
- [Parameter node tokens](parameter-nodes.md) — which token generates which node, and its pins
- [Compact type tokens](compact-types.md) — the declared component counts this page reads from
- [Metadata block](metadata.md) — the keys that change which pins exist
- [SamplerType](sampler-type.md) — `MipValueMode` and the other texture-sample keys
- [Calls](../graph/calls.md) — the general call grammar and named-argument rules
- [Swizzle](../graph/swizzle.md) — `.rgb`, `.a` and channel masks on a read value
- [Conversions](../graph/conversions.md) — component-count compatibility
- [Node reuse](../graph/node-reuse.md) — why one property equals one node
- [Name resolution](../graph/name-resolution.md) — variable-before-property lookup order
- [MaterialAttributes](../graph/material-attributes.md) — the value kind a static switch may not mix
- [UE builtins](../builtins/ue.md) — `UE.*` properties and the anonymous static-switch builtin
- [Generated HLSL](../generation/generated-hlsl.md) — how properties reach a `Custom` node
