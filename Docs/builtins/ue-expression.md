# UE.Expression

> [DreamShader](../index.md) » [Builtins](index.md) » **UE.Expression**

The reflected node call: creates any `UMaterialExpression` class of the builtin catalog by name and
fills its input pins and properties from the arguments.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, or a `Properties { … }` section (see [Declaration form](#declaration-form)) |
| Kind | builtin |
| Generates | one instance of the resolved `UMaterialExpression` class |

## Synopsis

```c
UE.Expression( Class = <class-specifier>
               [, { OutputType | ResultType } = <type-token> ]
               [, { { Output | OutputName } = <text> | OutputIndex = <int> } ]
               [, <arg-name> = <expression> ] … )

UE.<ClassName>( [ <arg-name> = <expression> ] … )
```

The two forms are one path through the binder. A call names its class with `Class`, or else with the
name after `UE.`, so `UE.Sine(…)` and `UE.Expression(Class = "Sine", …)` are identical; a `Class`
argument on another name wins over the name. `Expression` names no class, which is why the first form
needs `Class` — without it the call is [`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218).

`OutputType` is no longer required *(since 2.0.0)*: the catalog knows what every class makes. See
[OutputType](#outputtype).

Arguments are named. A positional argument binds only on the classes the catalog gives an argument
order — `TextureCoordinate`, `Constant`, `Constant2Vector`, `Constant3Vector`, `Constant4Vector`,
`Transform`, `TransformPosition`, `Panner`, `ComponentMask`, `Time`, and the five Substrate
composition nodes — and is [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220) elsewhere,
[`DSH5221`](../diagnostics/DSH5xxx.md#dsh5221) past the end of the order *(since 2.0.0)*. On the 27
1.x names of the [`UE.*` catalogue](ue.md#catalogue) the legacy front end drops a positional argument
first ([`DSH5254`](../diagnostics/DSH5xxx.md#dsh5254)), except `Input` of the two transforms.

Text arguments may be quoted or bare: `Class = Sine` and `Class = "Sine"` are the same, as are
`OutputType = float3` and `OutputType = "float3"`.

## Order of resolution

A call goes through the stages of the compiler. Each stage reports all of its errors, and a stage with
errors stops the pipeline before the next one *(since 2.0.0; 1.x stopped at the first failing step)*.

| # | Stage | What happens to the call | Codes |
| :-- | :-- | :-- | :-- |
| 1 | legacy front end | `Output` / `OutputIndex` become a selection on the call; `OutputType` / `ResultType` are taken off it (and kept on a Custom call); the 1.x shorthands are rewritten | `DSH5250`–`DSH5259`, `DSH5261`, `DSH5263`–`DSH5265` |
| 2 | binder — the class | `Class`, or the name, is looked up in the catalog | `DSH5216`, `DSH5217`, `DSH5218`, `DSH5212`, `DSH5210`, `DSH5294`, `DSH5300`, `DSH5276` |
| 3 | binder — the arguments | each is bound to a pin, a property, a Custom input, a Substrate argument, or kept for the built node | `DSH5220`, `DSH5221`, `DSH5285`, `DSH4215`, `DSH5214`, `DSH5224`, `DSH5215`, `DSH5278`, `DSH5284`, `DSH5288`, `DSH5291`, `DSH5213` |
| 4 | binder — the call | required pins; the type of the value | `DSH5279` (`DSH5219` in a `.dss`), `DSH4231` |
| 5 | IR builder | property values become literals | `DSH4373` |
| 6 | emitter | the node is created, its properties written, its pins connected | `DSH8211`, `DSH8214`, `DSH8213`, `DSH8212`, `DSH8254` |

## Class resolution

`Class` takes a quoted string, a bare word or a dotted name
([`DSH5217`](../diagnostics/DSH5xxx.md#dsh5217) otherwise). The specifier is trimmed and looked up in
the catalog, in this order:

| # | Matches |
| :-- | :-- |
| 1 | the catalog's short name — `Sine` |
| 2 | the class name — `MaterialExpressionSine` |
| 3 | the class path — `/Script/Engine.MaterialExpressionSine` |
| 4 | `MaterialExpression` + the specifier, and the C++ name with its `U` — `UMaterialExpressionSine` |
| 5 | an alias the catalog carries — `TexCoord`, `Lerp`, `Mask`, `FunctionCall`, … |
| 6 | in a 1.x source only: a unique match of rows 1–3 ignoring case, with [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) |

Nothing matched is [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212). Rows 1–5 are case-sensitive.

> [!IMPORTANT]
> `UMaterialExpressionSine` resolves *(since 2.0.0; 1.x compared against the reflected name and never
> matched a `U`-prefixed spelling)*. `USine` does not.

> [!NOTE]
> The catalog lists the classes loaded when it is built, abstract and deprecated ones left out. An
> expression class living in a plugin module the editor has not loaded is not in it.

On the `Substrate.*` path the name is the class, and `Class=` is
[`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216).

## OutputType

`OutputType` — or its alias `ResultType` — is **not** required *(since 2.0.0)*. The legacy front end
takes it off the call, and the type of the value comes from the catalog. It still matters in two
places:

- on a `Custom` node it is the node's output type — see [Custom nodes](#custom-nodes);
- on a node with one numeric output, a numeric type token is the width the value is typed with, as in
  1.x, and the node is wired whole.

Everything else it was — a texture, `MaterialAttributes` or `Substrate` token, a misspelled token — is
dropped without a diagnostic. The token tables are on the [`OutputType`](output-type.md) page.

## Argument dispatch

These names are not dispatched:

| Name | Purpose |
| :-- | :-- |
| `Class` | class specifier; matched with exactly this spelling |
| `OutputType`, `ResultType` | declared output type — taken off by the front end, any case |
| `Output`, `OutputName` | output selector by name — any case |
| `OutputIndex` | output selector by index — any case |

Every other argument is resolved against the class's catalog entry in this order. The first match
wins.

| # | Test | Result |
| :-- | :-- | :-- |
| a | the name is an input pin of the class, or one of its aliases | the value is connected to that pin |
| b | the name is a property of the class, or one of its aliases | the value is written into the node |
| c | in a 1.x source: a pin, then a property, whose name or alias is the only one to match ignoring case | as `a` or `b`, with `DSH5276` |
| d | the class is `Custom` | a new input of the node, named as written — see [Custom nodes](#custom-nodes) |
| e | the node is a Substrate BSDF and the name a [virtual argument](../language-v2/substrate.md#legacy-parameters-on-a-slab) | the input of a conversion node in front of the pins |
| f | in a 1.x source: `DefaultValue` on a class that has none | dropped with [`DSH5288`](../diagnostics/DSH5xxx.md#dsh5288) |
| g | in a 1.x source, or on a class whose pins depend on its properties: the value is a number, a texture, a material, a Substrate value or a node | kept with [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) and connected by that name when the node is built ([`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) if the node has no such pin) |
| h | — | [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213), with a "did you mean" when a spelling is close |

An input-pin name beats a property of the same name; a real pin or property name beats an alias.
The same pin or property twice is [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215).

The aliases are:

| Alias | Example |
| :-- | :-- |
| a bool property's name without its leading `b`, where a capital follows the `b` *(since 2.0.0)* | `FractionalPart` → `bFractionalPart` |
| a property's display name, with its spaces removed or turned into `_` | `ShaderOffsets`, `Shader_Offsets` → `WorldPositionShaderOffset` |
| a pin's display name in identifier form | `True`, `False` → `StaticSwitch.A`, `StaticSwitch.B` |
| the 1.x spellings with no rule behind them | `Index` (`TextureCoordinate.CoordinateIndex`), `Origin` (`ObjectPositionWS.OriginType`), `Source` / `Destination` (`Transform`, `TransformPosition`), `Asset` / `Parameter` (`CollectionParameter`), `UV` (`TextureSample.Coordinates`) |

The catalog lists the input pins and the properties that are editable and neither deprecated nor
transient; 1.x reached any `UPROPERTY`, editable or not *(since 2.0.0)*. Argument names are otherwise
exact: no separator is stripped.

### Input pins

An argument that resolves to a pin is an expression and is connected. Its type is checked against the
pin's as the catalog types it:

| Pin | Value | Result |
| :-- | :-- | :-- |
| `float1`–`float4` | the same width, or a scalar (spread across the pin) | connected |
| `float1`–`float4` | a narrower vector | connected as it is — what the node does with fewer components is the node's business |
| `float1`–`float4` | a wider vector | in a 1.x source connected whole, with the info [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289); otherwise [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) |
| a pin the engine does not type | any number, a material, a Substrate value | connected as it is |
| `MaterialAttributes`, Substrate, texture | a value of another kind | `DSH5214` |

### Literal properties

Any other matched name is a property, written into the node when it is built. The binder checks the
value; the emitter writes it with the 1.x literal writer, and a value it cannot write is
[`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213), whose message carries the writer's own code.

| Property type | The binder accepts | The emitter reads it as | Code inside `DSH8213` |
| :-- | :-- | :-- | :-- |
| `FBoolProperty` | a constant | `true` / `false`, case-insensitive | `DSH7132` |
| `FIntProperty` | a constant | a decimal integer | `DSH7133` |
| `FUInt32Property` | a constant | an integer in `0 … 4294967295` | `DSH7134` |
| `FFloatProperty` | a constant | a number | `DSH7135` |
| `FDoubleProperty` | a constant | a number | `DSH7136` |
| `FStrProperty` | a quoted string, a word or a dotted name | the trimmed text, verbatim — **always succeeds** | — |
| `FNameProperty` | the same | the trimmed text as an `FName` — **always succeeds** | — |
| `FObjectPropertyBase` | the same | see [Object properties](#object-properties) | `DSH7137`–`DSH7139` |
| `FEnumProperty`, `FByteProperty` with an enum | see [Enum values](#enum-values) | the enum value | `DSH7140`, `DSH7141` |
| `FByteProperty` without an enum | a constant | an integer in `0 … 255` | `DSH7142` |
| anything else — structs, arrays | a quoted string or a word | Unreal's own import text, e.g. `"(R=1,G=0,B=0,A=1)"` | `DSH7143` |

A value the binder does not accept is [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224): a computed
value for a numeric property, or a number for a text property.

> [!WARNING]
> `FStrProperty` and `FNameProperty` never fail. Whatever text the argument carries — including an
> unresolvable path — is stored verbatim. Neither a diagnostic nor a fallback value is produced.

### Enum values

The binder matches an enum value against the values the catalog lists for the property — hidden
values left out, each written without its prefix (`PostProcessInput0` for `PPI_PostProcessInput0`).

| Written | In a `.dss` | In a 1.x source |
| :-- | :-- | :-- |
| `PostProcessInput0` | accepted | accepted |
| `postprocessinput0` | [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215), with a "did you mean" | accepted, with [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) |
| `PPI_PostProcessInput0`, `ESceneTextureId::PPI_PostProcessInput0` | `DSH5215` | accepted, with `DSH5278` |
| a display name that differs from the value's name | `DSH5215` | `DSH5215` *(since 2.0.0)* |

The 1.x match (rule L12) ignores case, spaces, tabs and the characters `_`, `-`, `:`, `.` and `/`,
and tries the value whole, after an `Enum::` scope, and after its own prefix. The node is written with
the catalog's spelling. [`UE.SceneTexture`](ue.md#uescenetexture) and the transform bases rely on
this.

### Object properties

An object property takes `Path(<root>, "<asset>")`, a quoted object path `"/Game/…"`, or
`Class'/Game/…'`. A `Path(…)` argument is carried as text and resolved when the node is built. See
[`Path(…)`](../parameters/path.md).

| Situation | Result |
| :-- | :-- |
| The value is neither a quoted string, a word nor a `Path(…)` | `DSH5224` |
| The text begins with `Path(` or `/`, or ends in `'`, and does not resolve | `DSH8213` with the resolver's own message |
| The text is no asset reference | `DSH8213` (`DSH7137`) |
| The asset fails to load, and the property is a `UTexture` named `Texture` or `TextureObject` | the property is set to **null** and this counts as **success** |
| The asset fails to load, any other property | `DSH8213` (`DSH7138`) |
| The asset loads but is the wrong class | `DSH8213` (`DSH7139`) |

> [!WARNING]
> The null-on-failure rule is silent. `UE.Expression(Class = "TextureSample",
> Texture = Path(Game, "Missing/T_Nope"))` compiles with an unassigned texture instead of reporting
> the missing asset. Only the two property names `Texture` and `TextureObject` on `UTexture`-typed
> properties behave this way.

## Selecting an output

A node with several outputs is read through one of two mutually exclusive selectors, which the front
end turns into a selection on the call.

| Argument | Aliases | Kind | Becomes |
| :-- | :-- | :-- | :-- |
| `Output` | `OutputName` | a quoted name or a word | `.Name` on the call |
| `OutputIndex` | — | whole number ≥ 0 | `[k]` on the call |

| Mistake | Code |
| :-- | :-- |
| both selectors | [`DSH5252`](../diagnostics/DSH5xxx.md#dsh5252) |
| `OutputIndex` not a whole number of zero or more | [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250) |
| `Output` neither a quoted name nor a word | [`DSH5251`](../diagnostics/DSH5xxx.md#dsh5251) |
| a selector on a constructor | [`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253) |
| a name no output of the class has | [`DSH5201`](../diagnostics/DSH5xxx.md#dsh5201) |
| an index past the last output | [`DSH5282`](../diagnostics/DSH5xxx.md#dsh5282) |

The name is compared with the outputs the catalog lists, exactly; in a 1.x source a name that matches
only ignoring case is accepted with `DSH5276`. It may contain spaces (`Output = "Second Roughness"`).
An unnamed masked output of a node with several outputs is listed under the channels its mask keeps
(`RGB`, `R`, `A`); see [output names](output-type.md#output-mask-pseudo-names).

Using neither selector, the call is the node: see
[Result type and component count](#result-type-and-component-count).

## Custom nodes

`UE.Expression(Class = "Custom", …)` — `MaterialExpressionCustom` too — makes a node whose pins are the
call's (rule L4):

| Aspect | Behaviour |
| :-- | :-- |
| `OutputType` | the node's output type: `float`, `float1`, `half`, `half1` → Float1; `float2`, `vec2`, `half2` → Float2; `float3`, `vec3`, `half3` → Float3; `float4`, `vec4`, `half4` → Float4; `MaterialAttributes` → MaterialAttributes. Compared ignoring case and spaces; anything else is [`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261). Without one, the class's own output type |
| `AdditionalOutputs` | the node's further outputs, in the engine's import text; each is then selected by its name, and output 0 is called `return` |
| An argument that is no pin or property | a new input, named exactly as written; a value that is no number, bool or texture is [`DSH5284`](../diagnostics/DSH5xxx.md#dsh5284), the same name twice `DSH4215` |
| The fresh node | its `Inputs` and `AdditionalOutputs` arrays are cleared before the call's are written |
| An output selector | names one of the declared outputs: another name is `DSH5201`, an index past them `DSH5282` *(since 2.0.0; 1.x made placeholder outputs `Output1`, `Output2`, … up to the one asked for)* |
| Node reuse | two identical Custom calls are one node, as any two identical calls are *(since 2.0.0)* |

## Result type and component count

The value of a call, by what the catalog says of the class:

| # | The class | The value |
| :-- | :-- | :-- |
| 1 | has no output (a custom-output class) | none — the call is a statement; used as a value it is [`DSH4231`](../diagnostics/DSH4xxx.md#dsh4231) |
| 2 | is `Custom` | the type `OutputType` gives; with `AdditionalOutputs`, a node whose outputs are named |
| 3 | has several outputs | a node: name one, or use it where its first output — or the whole value its channel outputs make up — fits exactly. In a 1.x source a first output of no fixed width is read as that output ([`DSH5287`](../diagnostics/DSH5xxx.md#dsh5287)), and so is the first output of any node passed straight to a pin *(2.0.0 – 2.1.0: `DSH5201`)*; otherwise `DSH5201` |
| 4 | has one output of a known type | that type |
| 5 | has one output the engine does not type | as wide as the widest number on a pin the engine does not type either — or the Substrate or material value such a pin carries — else as wide as the place it is read into |

In a 1.x source, a numeric `OutputType` on a class of row 4 or 5 types the value instead, as above.

An output's type is its mask's width when it has a mask, else the engine's value type; where the
engine says only "a float", a table of known widths decides — 1.x's own, consulted for a class with
one output, and a few rows the catalog adds:

| Width | Classes |
| :-- | :-- |
| 2 | `TextureCoordinate`, `Panner`, `Rotator`, `SceneTexelSize`; `SceneTexture` outputs `Size` and `InvSize` |
| 3 | `ObjectPositionWS`, `CameraVectorWS`, `VertexNormalWS`, `VertexTangentWS`, `Transform`, `TransformPosition`, `SkyAtmosphereLightDirection`, `PixelNormalWS`, `CrossProduct`, `ObjectBounds`, `CameraPositionWS`, `ReflectionVectorWS` |
| 1 | `PixelDepth`, `TwoSidedSign`, `Arctangent2Fast`, `Length`, `MaterialXLuminance`, `Time`, `SceneDepth`, `ObjectRadius`, `PerInstanceRandom`, `PerInstanceFadeAmount` |

A `MaterialAttributes` result is a [`material`](../graph/material-attributes.md) value.

## Side effects on the material

Two classes are registered as material parameters when they are built, so they appear in the
material instance editor:

| Class | Registration |
| :-- | :-- |
| `UMaterialExpressionStaticSwitchParameter` with a parameter name | an editor-only static switch value on the material or material function |
| `UMaterialExpressionStaticComponentMaskParameter` with a parameter name | an editor-only static component-mask value, from the node's four default channels |

Both are given a fresh expression GUID when theirs is invalid.

## Node reuse

The IR merges two nodes of the same class with the same properties and the same inputs into one, and
everything that read the second reads the first *(since 2.0.0)*. The comparison is of the nodes, not
of the calls' spelling: argument order, an alias or a case-only difference play no part, and a
`Custom` node merges like any other. A statement — a node with no output — never merges. See
[Node reuse](../graph/node-reuse.md).

## SampleTexture2D

```c
SampleTexture2D(<texture-object>, <uv>)
```

A reserved two-argument form, resolved by the legacy front end before user properties and functions
and matched **case-sensitively**. It rewrites to

```c
UE.Expression(Class = "TextureSample", TextureObject = <arg0>, Coordinates = <arg1>)
```

and the call stands for the whole sample: `float4 t = SampleTexture2D(T, uv)` is the `RGBA` output,
`float3 c = …` the `RGB` output, `.a` the `A` output. Both arguments are positional and both are
required; any other shape — a named argument, a third argument, an output selector — is
[`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256).

A property declared with a texture-sample parameter token (`TextureSampleParameter2D` and its kin) is
called with its pins instead — `BaseTex(Coordinates = uv)`. See
[Parameters in Graph](../parameters/graph-usage.md).

## Declaration form

A `UE.<Name>(…)` — `UE.Expression(Class = …)` included — may stand as a property type inside
[`Properties`](../language/properties.md). It is the [declaration form](ue.md#properties-declaration-form)
of any `UE.` call: a local at the head of each `Graph` body that reads the property, bound like the
`Graph` form. What differs from 1.x *(since 2.0.0)*:

| Aspect | Now | 1.x |
| :-- | :-- | :-- |
| `OutputType` / `ResultType` | optional; the type of the local — see [Declared output width](ue.md#declared-output-width) | required, from a reduced token set |
| Input-pin names | matched, as in `Graph` | not matched — properties only |
| Input values | a quoted string, a number, `true` / `false`, or a word (another variable); anything else, a vector literal `float3(…)` among them, is kept as text, which a pin refuses ([`DSH4202`](../diagnostics/DSH4xxx.md#dsh4202)) | a declared property, a scalar, or a 2–4 component vector literal |
| `ParameterName` | only as written | set from the property name when the class has one |
| Metadata `[ … ]` | read and not applied | applied to the node |
| `Output` / `OutputIndex` | ordinary arguments: `DSH5213`, or `DSH5291` and `DSH8212` | select the output |

## Notes

- Property names are **flat**. Dotted paths and `[index]` selectors are not accepted here; those exist
  only in the [`Settings`](../settings/material.md) section, which is a different resolver.
- A required pin left unconnected is a warning — [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279) in a
  1.x source — and the engine reports it when the material compiles if the node needs the pin.
- A typo in a name and a typo in `Class` are two codes: `UE.Sinee()` is
  [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210), `UE.Expression(Class = "Sinee")` is `DSH5212`.
- A call may be swizzled like any other expression:
  `UE.Expression(Class = "VertexColor").rgb`. See
  [Swizzle](../graph/swizzle.md#swizzling-a-call-result).
- `dsc export-catalog` writes out every class with its pins, properties, aliases and outputs — the
  practical way to discover argument names. The editor also exports a manifest to
  `Saved/DreamShader/Bridge/material-expressions.json`; see [Bridge](../tools/bridge.md).

## Diagnostics

Every code is listed with its message and its full description on its page in
[Diagnostics](../diagnostics/index.md).

| Code | Raised when |
| :-- | :-- |
| [`DSH5218`](../diagnostics/DSH5xxx.md#dsh5218) | `UE.Expression` without `Class` |
| [`DSH5217`](../diagnostics/DSH5xxx.md#dsh5217) | `Class` is not a quoted string, a word or a dotted name |
| [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212) | `Class` matches no class of the catalog |
| [`DSH5210`](../diagnostics/DSH5xxx.md#dsh5210) | the name after `UE.` matches no class of the catalog |
| [`DSH5216`](../diagnostics/DSH5xxx.md#dsh5216) | `Class` on a `Substrate.*` call |
| [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) (warning) | in a 1.x source, a class, pin, property or output matches only ignoring case |
| [`DSH5220`](../diagnostics/DSH5xxx.md#dsh5220) / [`DSH5221`](../diagnostics/DSH5xxx.md#dsh5221) | a positional argument on a class with no argument order, or past its end |
| [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) | an argument names no pin or property |
| [`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291) (info) | an argument naming no pin is kept for the built node to name |
| [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215) | a pin, property or Custom input is given twice |
| [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214) | a value does not fit its pin |
| [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289) (info) | in a 1.x source, a wider vector is connected whole |
| [`DSH5224`](../diagnostics/DSH5xxx.md#dsh5224) | a property value is not a literal or constant of the kind the property takes |
| [`DSH5215`](../diagnostics/DSH5xxx.md#dsh5215) | an enum value is not a value of the enum |
| [`DSH5278`](../diagnostics/DSH5xxx.md#dsh5278) (warning) | in a 1.x source, an enum value is spelled the 1.x way |
| [`DSH5284`](../diagnostics/DSH5xxx.md#dsh5284) | a Custom input is given something a Custom pin cannot carry |
| [`DSH5261`](../diagnostics/DSH5xxx.md#dsh5261) | a Custom `OutputType` is not one of the accepted tokens |
| [`DSH5288`](../diagnostics/DSH5xxx.md#dsh5288) (warning) | in a 1.x source, `DefaultValue` on a class that has none is dropped |
| [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279) (warning) | in a 1.x source, a required pin is left unconnected |
| [`DSH5250`](../diagnostics/DSH5xxx.md#dsh5250)–[`DSH5253`](../diagnostics/DSH5xxx.md#dsh5253), `DSH5201`, `DSH5282` | an output selector — see [Selecting an output](#selecting-an-output) |
| [`DSH4231`](../diagnostics/DSH4xxx.md#dsh4231) | a node with no output is used as a value |
| [`DSH4373`](../diagnostics/DSH4xxx.md#dsh4373) | a property value is computed at run time |
| [`DSH5256`](../diagnostics/DSH5xxx.md#dsh5256) | `SampleTexture2D` is not called with exactly two positional arguments |
| [`DSH8211`](../diagnostics/DSH8xxx.md#dsh8211) | the engine building the asset has no such class |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | the node could not be created |
| [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213) | a property could not be written; the message carries `DSH7131`–`DSH7144` or the asset resolver's code |
| [`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) | the built node has no pin of a name kept with `DSH5291` |

## Example

```c
Shader(Name="Docs/M_Generic")
{
    Properties {
        Texture2D BaseTex = Path(Game, "Textures/T_Noise");
        vec3      Dimmed  = vec3(0.2, 0.2, 0.2);
    }

    Settings { ShadingModel = "Unlit"; }

    Outputs {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph {
        // Class defaults to the function name.
        float pulse = UE.Sine(Input = UE.Time());

        // Explicit Class, an enum property by its value's name, then output 0 swizzled.
        float3 scene = UE.Expression(Class = "SceneTexture",
                                     SceneTextureId = "PostProcessInput0").Color.rgb;

        // An object property through Path(...), and a pin by its pin name.
        float4 tex = UE.Expression(Class = "TextureSample",
                                   Texture      = Path(Game, "Textures/T_Noise"),
                                   Coordinates  = UE.TexCoord(Index = 0));

        // Pins by their display names (True, False for A, B), and a bool property.
        float3 sel = UE.Expression(Class = "StaticSwitch",
                                   True = Dimmed, False = tex.rgb, DefaultValue = true);

        Color = (scene + sel) * pulse;
    }
}
```

Generated nodes:

```text
Time                                  -> Sine                       (pulse)
SceneTexture (PPI_PostProcessInput0)  -> Color, mask .rgb           (scene)
TextureCoordinate (Index 0)           -> TextureSample.Coordinates
TextureSample (Texture = T_Noise)                                   (tex)
Constant3Vector (0.2, 0.2, 0.2)       -> StaticSwitch.True
StaticSwitch  (DefaultValue = true)                                 (sel)
Add, Multiply                                                       (Color)
```

## See also

- [Builtins](index.md) — the call surfaces and the resolution order
- [`UE.*` catalogue](ue.md) — the 1.x names and their argument lists
- [`OutputType`](output-type.md) — what the argument still does, and the output names
- [Substrate](substrate.md) — the sibling namespace that shares this path
- [Substrate sugar](../language-v2/substrate.md) — virtual arguments and operators
- [`Path(…)`](../parameters/path.md) — asset-reference syntax for object properties
- [Parameters in Graph](../parameters/graph-usage.md) — the parameter pin-call form
- [Material attributes](../graph/material-attributes.md) — the `material` value kind
- [Conversions](../graph/conversions.md) — widths and how values fit
- [Node reuse](../graph/node-reuse.md) — identical nodes
- [Graph functions](../language/graph-function.md) — `UE.*` calls hoisted into Custom-node pins
- [Bridge](../tools/bridge.md) — the exported reflected expression manifest
- [Diagnostics index](../diagnostics/index.md) — every code
