# UE.Expression

> [DreamShader](../index.md) » [Builtins](index.md) » **UE.Expression**

The generic reflected call: creates any non-abstract `UMaterialExpression` subclass by name and fills
its input pins and `UPROPERTY`s from named arguments.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside a `Graph { … }` body, or a `Properties { … }` section (see [Declaration form](#declaration-form)) |
| Kind | builtin |
| Generates | one instance of the resolved `UMaterialExpression` subclass |

## Synopsis

```c
UE.Expression( Class = <class-specifier> , { OutputType | ResultType } = <type-token>
               [, { { Output | OutputName } = <text> | OutputIndex = <int> } ]
               [, <arg-name> = <expression> ] … )

UE.<ClassName>( { OutputType | ResultType } = <type-token> [, <arg-name> = <expression> ] … )
```

The two forms are one implementation. `Class` defaults to the function name, so `UE.Sine(…)` and
`UE.Expression(Class = "Sine", …)` are identical. The name `Expression` is not special except that it
never resolves to a class, which is what makes `Class=` mandatory in the first form.

Every argument must be named. A positional argument fails with
`Generic {Namespace}.{Function} calls require named arguments.`

Text arguments may be quoted or bare: `Class = Sine` and `Class = "Sine"` are the same, as are
`OutputType = float3` and `OutputType = "float3"`.

## Order of resolution

The call is processed in this order. The order is observable, because the first failing step is the
only error reported.

| # | Step | Failure |
| :-- | :-- | :-- |
| 1 | Reject positional arguments | `Generic {Namespace}.{Function} calls require named arguments.` |
| 2 | `Substrate.*` descriptor lookup and engine-version gate | see [Substrate](substrate.md) |
| 3 | Resolve `OutputType` / `ResultType` | `Unsupported UE builtin call …`, `… OutputType must be a literal value.`, `… OutputType '{Token}' is not supported.` |
| 4 | Resolve `Class` | `… Class must be a literal value.`, `UE.Expression requires Class="MaterialExpressionName".`, `… could not resolve MaterialExpression class '{Class}'.` |
| 5 | Reject `Output`/`OutputName` together with `OutputIndex` | `… cannot use OutputName/Output together with OutputIndex.` |
| 6 | Node-reuse lookup | — |
| 7 | Create the node | `UE.{Function} failed to create '{Class}'.` |
| 8 | `UMaterialExpressionCustom` setup | `… OutputType="Substrate" is not supported by UMaterialExpressionCustom.`, `… OutputType '{Token}' is not a valid Custom node output type.` |
| 9 | Dispatch each remaining argument to a pin, a property, or a Custom input | `… '{Argument}' is not a property on '{Class}'.` and the input/property messages |
| 10 | Synthesize Custom-node outputs | `… OutputName must be a non-empty literal value.` |
| 11 | Resolve the output index | `… OutputIndex is out of range …`, `… output '{Name}' was not found …`, `… has no material outputs.` |
| 12 | Register static-switch / static-component-mask parameters on the material | — |
| 13 | Derive the result type and component count | `{Namespace}.{Function} output is not a Substrate value.` |
| 14 | Assemble the result and write the reuse cache | — |

> [!NOTE]
> `UE.Expression()` with no arguments at all reports the **`OutputType`** error, not the missing
> `Class`, because step 3 runs before step 4.

## Class resolution

`Class` must be a literal (quoted string, bare identifier, or dotted name). The specifier is trimmed
and then resolved as follows.

| # | Rule |
| :-- | :-- |
| 1 | Empty after trimming → unresolved |
| 2 | If the text contains `/` or `.`, it is loaded directly as an object path and accepted only if the loaded class derives from `UMaterialExpression` — `Class = "/Script/Engine.MaterialExpressionSine"` |
| 3 | Otherwise a candidate-name list is built (below) |
| 4 | Every loaded `UClass` is scanned; the first one that derives from `UMaterialExpression`, is **not** abstract, and whose name equals any candidate ignoring case, wins |

Candidate names, for a specifier `S`:

| Candidate | Added when |
| :-- | :-- |
| `S` | always |
| `U` + `S` | `S` does not start with `U` |
| `MaterialExpression` + `S` | `S` does not start with `MaterialExpression` |
| `UMaterialExpression` + `S` | `S` does not start with `UMaterialExpression` |

Both the `StartsWith` guards and the final comparison are case-insensitive.

> [!IMPORTANT]
> **The scan compares against the *reflected* class name, which carries no `U` prefix.**
> `UMaterialExpressionSine`'s reflected name is `MaterialExpressionSine`, so a specifier that already
> begins with `U` — `USine`, `UMaterialExpressionSine` — never matches, and the `U`-prefixed candidate
> rows above are unreachable in practice. The spellings that do resolve to `UMaterialExpressionSine`
> are `Sine`, `sine`, `MaterialExpressionSine` (any casing) and the object path
> `/Script/Engine.MaterialExpressionSine`.

> [!NOTE]
> Only **loaded** classes are scanned. An expression class living in a plugin module that the editor
> has not loaded will not be found, and the call fails with
> `UE.{Function} could not resolve MaterialExpression class '{Class}'.` Abstract classes are skipped
> even when the name matches exactly.

On the `Substrate.*` path the class is fixed by the builtin descriptor and `Class=` is rejected
outright.

## OutputType

`OutputType` — or its alias `ResultType`, checked second — is **required** on this path. Omitting it
produces

```text
Unsupported UE builtin call '{Function}' in Graph. For generic MaterialExpression calls, add OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture/Substrate".
```

The complete accepted token set is larger than that hint suggests and is tabulated in
[`OutputType`](output-type.md).

> [!IMPORTANT]
> `OutputType` is **advisory only for the classes the generator recognizes**. Substrate,
> `MaterialAttributes` and texture outputs are re-derived from the node's real output value type, and
> so are the classes in the [known-width table](#result-type-and-component-count) — which is why
> `UE.Expression(Class = "WorldPosition", OutputType = "float1")` still yields a 3-component value.
> For every other class the declared token *is* the width, whatever the node's real output type is.
> On a Custom node the declared `OutputType` is authoritative in a stronger sense: it is written to the
> node and decides the HLSL return type.

## Argument dispatch

These six argument names are reserved and never dispatched:

| Reserved name | Purpose |
| :-- | :-- |
| `Class` | class specifier |
| `OutputType` | declared output type |
| `ResultType` | alias of `OutputType` |
| `Output` | output selector by name |
| `OutputName` | alias of `Output` |
| `OutputIndex` | output selector by index |

Every other argument is resolved against the created node in this order. The first match wins.

| # | Test | Result |
| :-- | :-- | :-- |
| a | The normalized argument name equals the normalized name of one of the node's **input pins** | connect an expression to that pin |
| b | The normalized argument name equals the normalized name of a reflected `FProperty` on the class or any super | pin path if the property is an expression-input struct, otherwise the literal path |
| c | The node is a `UMaterialExpressionCustom` | a new Custom input pin named exactly as written |
| d | — | `UE.{Function}: '{Argument}' is not a property on '{Class}'.` |

> [!IMPORTANT]
> **An input-pin name beats a reflected property of the same name.** Several engine expressions carry
> both — a pin `Input` and a `UPROPERTY` of a related name — and the pin always wins. To reach a
> property that is shadowed by a pin name, there is no alternative spelling; use the property's real
> `UPROPERTY` name, which is what the second pass matches.

Property matching runs in two passes:

| Pass | Matches | Note |
| :-- | :-- | :-- |
| 1 | any `FProperty` whose name equals the argument after trimming and lower-casing | **not** restricted to editable properties — private and non-`EditAnywhere` `UPROPERTY`s are reachable |
| 2 | `FBoolProperty` only: the property name, lower-cased, with a leading `b` removed | this is how `FractionalPart` reaches `bFractionalPart` |

> [!WARNING]
> Pass 2 lower-cases **before** stripping, and strips a leading `b` from *any* bool property. A bool
> property named `bTangent` is therefore also reachable as `Tangent`, and a bool property named
> `BaseColor` is also reachable as `aseColor`. Only bool properties are affected. When two properties
> collide under these rules, pass 1 wins because it runs first.

### Input pins

An argument that resolves to a pin — by pin name, or by a property whose type is an expression-input
struct (`FExpressionInput` or a struct named `MaterialAttributesInput`) — has its value evaluated as a
`Graph` expression and connected. Any channel mask carried by the value is transferred to the
connection.

The pin's declared value type is then checked against the value:

| Pin type | Value | Result |
| :-- | :-- | :-- |
| Substrate | non-Substrate | `{Namespace}.{Function} input '{Pin}' expects a Substrate value.` |
| `MaterialAttributes` | non-attributes | `{Namespace}.{Function} input '{Pin}' expects a MaterialAttributes value.` |
| numeric | Substrate | `{Namespace}.{Function} input '{Pin}' does not accept Substrate values.` |
| numeric | `MaterialAttributes` | `{Namespace}.{Function} input '{Pin}' does not accept MaterialAttributes values.` |
| otherwise | any | connected |

No width check is applied at a pin: connecting a `float4` to a scalar pin is the engine's problem, not
DreamShader's.

### Literal properties

Any other matched property is written from a literal. Object-typed properties are read as asset
references; everything else is read as literal text.

| Property type | Accepted value syntax | Message on failure |
| :-- | :-- | :-- |
| `FBoolProperty` | `true` / `false`, case-insensitive | `'{Value}' is not a valid boolean value for '{Property}'.` |
| `FIntProperty` | decimal integer | `'{Value}' is not a valid integer value for '{Property}'.` |
| `FUInt32Property` | integer in `0 … 4294967295` | `'{Value}' is not a valid unsigned integer value for '{Property}'.` |
| `FFloatProperty` | any numeric literal | `'{Value}' is not a valid numeric value for '{Property}'.` |
| `FDoubleProperty` | any numeric literal | `'{Value}' is not a valid numeric value for '{Property}'.` |
| `FStrProperty` | the trimmed text, verbatim — **always succeeds** | — |
| `FNameProperty` | the trimmed text as an `FName` — **always succeeds** | — |
| `FObjectPropertyBase` | `Path(…)` or an absolute Unreal object path | see [Object properties](#object-properties) |
| `FEnumProperty` | see [Enum values](#enum-values) | `'{Value}' is not a valid enum value for '{Property}'.` |
| `FByteProperty` backed by an enum | see [Enum values](#enum-values) | `'{Value}' is not a valid enum value for '{Property}'.` |
| `FByteProperty` with no enum | integer in `0 … 255` | `'{Value}' is not a valid byte value for '{Property}'.` |
| anything else — structs, arrays, sets, maps | Unreal's own import text, e.g. `(R=1,G=0,B=0,A=1)`, `(X=1,Y=2,Z=3)` | `Property '{Property}' on '{Class}' is not a supported literal type yet.` |

All of these are wrapped as `UE.{Function} property '{Property}': {Message}`.

> [!WARNING]
> `FStrProperty` and `FNameProperty` never fail. Whatever text the argument carries — including a
> misspelled enum name or an unresolvable path — is stored verbatim. Neither a diagnostic nor a
> fallback value is produced.

### Enum values

An enum value is matched against every non-hidden entry of the enum in four spellings. Both sides are
lower-cased and the characters ` `, `_`, `-`, `:`, `.` and `/` are removed before comparison.

| # | Spelling compared |
| :-- | :-- |
| 1 | the entry's short name — `PPI_PostProcessInput0` |
| 2 | the entry's fully qualified name — `ESceneTextureId::PPI_PostProcessInput0` |
| 3 | the entry's display name |
| 4 | the short name with everything up to and including the first `_` removed — `PostProcessInput0` |

Rule 4 is what makes prefix-less spellings work, and what
[`UE.SceneTexture`](ue.md#uescenetexture) relies on.

### Object properties

An object-typed property takes `Path(<root>, "<asset>")`, `Path("/Game/…")` or a bare absolute object
path. See [`Path(…)`](../parameters/path.md).

| Situation | Result |
| :-- | :-- |
| The argument is neither a literal nor a `Path(…)` call | `UE.{Function} property '{Property}' must use Path(...) or an Unreal object path.` |
| The text begins with `Path(` or `/` but does not resolve | the resolver's own message is reported |
| The text is a literal that is not an asset reference and begins with neither | `Object property '{Property}' expects Path(...) or an absolute Unreal object path.` |
| The asset fails to load, and the property is a `UTexture` named `Texture` or `TextureObject` | the property is set to **null** and this counts as **success** |
| The asset fails to load, any other property | `Failed to load asset '{Path}' for '{Property}'.` |
| The asset loads but is the wrong class | `Asset '{Path}' is not compatible with '{Property}'. Expected '{Class}'.` |

> [!WARNING]
> The null-on-failure rule is silent. `UE.Expression(Class = "TextureSample", OutputType = "float4",
> Texture = Path(Game, "Missing/T_Nope"))` compiles with an unassigned texture instead of reporting
> the missing asset. Only the two property names `Texture` and `TextureObject` on `UTexture`-typed
> properties behave this way.

## Selecting an output

A node with several outputs is read through one of two mutually exclusive selectors.

| Argument | Aliases | Kind | Semantics |
| :-- | :-- | :-- | :-- |
| `Output` | `OutputName` | literal text | resolved by name, then by mask pseudo-name |
| `OutputIndex` | — | integer ≥ 0 | a direct index into the node's outputs |

Using both is an error. Using neither selects output 0.

Name resolution walks the outputs in order:

| Output | Matched by |
| :-- | :-- |
| named output | its name, compared as an `FName` — case-insensitive |
| unnamed output | one of the mask pseudo-names `RG`, `RGB`, `RGBA`, `R`, `G`, `B`, `A`, tested in that order against the output's channel mask |

The specifier is trimmed; an empty specifier selects output 0. The mask pseudo-names are listed in
full on the [`OutputType`](output-type.md#output-mask-pseudo-names) page.

## Custom nodes

`UMaterialExpressionCustom` is the one class with dedicated handling.

| Aspect | Behaviour |
| :-- | :-- |
| `OutputType = "Substrate"` | rejected — `UE.{Function} OutputType="Substrate" is not supported by UMaterialExpressionCustom.` |
| Other `OutputType` values | must map to a Custom output type; `Texture2D`, `SamplerState`, `TextureCube`, `Texture2DArray`, `Texture3D`, `VolumeTexture`, `StaticBool` and `StaticBoolParameter` are valid `OutputType`s in general but **not** here |
| Declared `OutputType` | authoritative — written to the node |
| Unmatched arguments | become new Custom input pins named exactly as written; a Substrate value is rejected with `UE.{Function} Custom input '{Name}' does not accept Substrate values.` |
| Fresh node | its `Inputs` and `AdditionalOutputs` arrays are cleared before the arguments are applied |
| `OutputName` | must be a non-empty literal, and implies a request for additional output index 1 |
| Missing additional outputs | placeholders named `Output1`, `Output2`, … are synthesized up to the requested index, then the node's outputs are rebuilt |
| Result width | taken from the node's **actual** output value type, so secondary outputs are sized correctly |
| Node reuse | **disabled** — every Custom call creates its own node |

## Result type and component count

The result is derived from the resolved output's real value type, in this order.

| # | Condition | Result |
| :-- | :-- | :-- |
| 1 | `OutputType = "Substrate"` but the actual output is not Substrate | `{Namespace}.{Function} output is not a Substrate value.` |
| 2 | Actual output is Substrate | 0 components, Substrate value, authoritative |
| 3 | Actual output is `MaterialAttributes` | 0 components, attributes value, authoritative |
| 4 | Actual output is any texture type | texture object, authoritative |
| 5 | A `Substrate.*` utility builtin | width from the output's value type |
| 6 | A Custom node | width from the output's value type |
| 7 | `TextureCoordinate`, `Panner` or `Rotator` | 2 components |
| 8 | Otherwise | a hard-coded known-width table, else the declared `OutputType` |

The known-width table:

| Width | Classes |
| :-- | :-- |
| 2 | `TextureCoordinate`, `Panner`, `ScreenPosition`, `Rotator`, `SceneTexelSize` |
| 3 | `WorldPosition`, `ObjectPositionWS`, `CameraVectorWS`, `VertexNormalWS`, `VertexTangentWS`, `Transform`, `TransformPosition`, `SkyAtmosphereLightDirection`, `PixelNormalWS`, `CrossProduct` |
| 1 | `PixelDepth`, `TwoSidedSign`, `Arctangent2Fast`, `Length`, `MaterialXLuminance` |

Five classes then override the width from the values actually bound to their inputs:

| Class | Width |
| :-- | :-- |
| `Saturate` | the width of the bound `Input` |
| `StaticSwitchParameter` | the larger of the bound `True` and `False` |
| `If` | the largest of the bound `AGreaterThanB`, `AEqualsB` and `ALessThanB` |
| `StaticComponentMaskParameter` | the number of `DefaultR`/`DefaultG`/`DefaultB`/`DefaultA` set, minimum 1 |
| `CurveAtlasRowParameter` | the number of channels in the selected output's mask, minimum 1 |

A result with 0 components that is neither a texture nor a Substrate value is a
[`MaterialAttributes`](../graph/material-attributes.md) value.

## Side effects on the material

Two classes are registered as material parameters when created through this path, so they appear in
the material instance editor:

| Class | Registration |
| :-- | :-- |
| `UMaterialExpressionStaticSwitchParameter` with a parameter name | an editor-only static switch value on the material or material function |
| `UMaterialExpressionStaticComponentMaskParameter` | an editor-only static component-mask value, from the node's four default channels |

Both are given a fresh expression GUID when theirs is invalid.

## Node reuse

Two cache keys are built per call:

| Key | Contents |
| :-- | :-- |
| expression key | every non-reserved argument, plus the resolved class name and the normalized `OutputType` text |
| output key | the expression key plus the output selector (`OutputName=…`, `OutputIndex=…`, or `OutputIndex=0`) |

An output-key hit returns the previous result immediately. An expression-key hit reuses the **node**
and re-resolves only the output. Literal text in a key is normalized by collapsing runs of spaces and
normalizing line endings. `UMaterialExpressionCustom` subclasses never participate. See
[Node reuse](../graph/node-reuse.md).

> [!NOTE]
> When a node is reused, its input and property arguments are **not re-applied**. This is not
> observable through the language — the arguments are part of the key, so a reused node was built from
> the same arguments — but it does mean a single node ends up with several call sites in the source.

## SampleTexture2D

```c
SampleTexture2D(<texture-object>, <uv>)
```

A reserved two-argument form, resolved before user properties and functions and matched
**case-sensitively**. It rewrites to

```c
UE.Expression(Class = "TextureSample", OutputType = "float4",
              TextureObject = <arg0>, Coordinates = <arg1>)
```

Both arguments are positional and both are required:
`SampleTexture2D expects exactly two positional arguments: (textureObject, uv).`

For sampling a declared texture *parameter*, prefer the parameter's own pin-call form —
`BaseTex(Coordinates = uv)` — which reuses the parameter node instead of creating a plain
`TextureSample`. See [Parameters in Graph](../parameters/graph-usage.md).

## Declaration form

A generic `UE.<Name>(…)` may also stand as a property type inside
[`Properties`](../language/properties.md), where any name outside the
[declaration-form catalogue](ue.md#properties-declaration-form) is created by the same reflection
machinery. The rules differ from the `Graph` form:

| Aspect | Declaration form | Graph form |
| :-- | :-- | :-- |
| `OutputType` / `ResultType` | required, from a [reduced token set](output-type.md) | required, from the full set |
| `Class` | defaults to the builtin name, overridable | same |
| Input-pin-name matching | **not performed** — only reflected properties are matched | performed first |
| Unmatched argument | a Custom input on a Custom node, otherwise `'{Argument}' is not a property on '{Class}'.` | same |
| Input values | a previously declared property, a scalar literal, or a 2–4 component vector literal | any `Graph` expression |
| `ParameterName` | **auto-set from the property name** when the class exposes one and the author did not | never set |
| Output selection | `Output` / `OutputName` / `OutputIndex`, Custom placeholders synthesized the same way | same |
| Metadata `[ … ]` | applied to the node after creation | not applicable |
| Node canvas X | -800 | 520 |

Additional messages from this surface:

| Message | Cause |
| :-- | :-- |
| `Unsupported vector literal '{Value}'.` | an input literal that is not 2–4 numeric components |
| `Failed to create a scalar constant expression.` | the constant node for a scalar input literal could not be created |
| `Failed to create a float{N} constant expression.` | the constant node for a vector input literal could not be created |
| `'{Value}' is not a valid property reference or literal input.` | an input value that is neither a declared property nor a literal |
| `'{Class}' does not expose a ParameterName property.` | `ParameterName` metadata on a class with no such property |
| `OutputIndex is out of range for '{Class}'.` | the selected index does not exist |

## Notes

- Property names are **flat**. Dotted paths and `[index]` selectors are not accepted here; those exist
  only in the [`Settings`](../settings/material.md) section, which is a different resolver.
- There is no way to leave a required engine pin unconnected on purpose and no way to disconnect one —
  an argument either connects a value or is absent.
- Because `Class` defaults to the function name, a typo in a class name produces
  `could not resolve MaterialExpression class` rather than "unknown builtin"; the two spellings
  `UE.Sinee(OutputType="float1")` and `UE.Expression(Class="Sinee", OutputType="float1")` report the
  same thing.
- A generic call may be swizzled like any other expression:
  `UE.Expression(Class = "VertexColor", OutputType = "float4").rgb`. See
  [Swizzle](../graph/swizzle.md#swizzling-a-call-result).
- The reflected expression manifest the editor exports to
  `Saved/DreamShader/Bridge/material-expressions.json` lists every resolvable class with its pins and
  properties, which is the practical way to discover argument names. See
  [Bridge](../tools/bridge.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section; the compiler emits the
substituted text. `{Function}` preserves the author's spelling and casing; `{Namespace}` is literally
`UE` or `Substrate`.

| Message | Cause |
| :-- | :-- |
| `Generic {Namespace}.{Function} calls require named arguments.` | a positional argument on the generic path |
| `Unsupported UE builtin call '{Function}' in Graph. For generic MaterialExpression calls, add OutputType="float1/2/3/4/Texture2D/TextureCube/Texture2DArray/VolumeTexture/Substrate".` | no `OutputType` and no `ResultType` |
| `UE.{Function} OutputType must be a literal value.` | the value is an expression, not a literal |
| `UE.{Function} OutputType="Substrate" requires Unreal Engine 5.4 or newer.` | `Substrate` on UE 5.3 |
| `UE.{Function} OutputType '{Token}' is not supported.` | the token is not in the [`OutputType` table](output-type.md) |
| `UE.{Function} Class must be a literal value.` | the class specifier is an expression |
| `UE.Expression requires Class="MaterialExpressionName".` | the function name is literally `Expression` and no `Class` was given |
| `UE.{Function} could not resolve MaterialExpression class '{Class}'.` | no loaded, non-abstract `UMaterialExpression` subclass matched |
| `Substrate.{Function} uses a fixed MaterialExpression class and does not accept Class.` | `Class=` on a `Substrate.*` call |
| `Substrate.{Function} resolved to non-Substrate class '{Class}'.` | the descriptor's class is not a Substrate BSDF or utility class |
| `UE.{Function} cannot use OutputName/Output together with OutputIndex.` | both selectors given |
| `UE.{Function} failed to create '{Class}'.` | node creation returned nothing |
| `UE.{Function} OutputType="Substrate" is not supported by UMaterialExpressionCustom.` | Substrate output on a Custom node |
| `UE.{Function} OutputType '{Token}' is not a valid Custom node output type.` | a texture or static-bool `OutputType` on a Custom node |
| `UE.{Function}: '{Argument}' is not a property on '{Class}'.` | the argument matched no pin and no property |
| `UE.{Function} input '{Pin}': {Message}` | the value bound to a pin failed to evaluate |
| `UE.{Function} Custom input '{Name}' does not accept Substrate values.` | a Substrate value passed to a Custom input |
| `UE.{Function} failed to bind input '{Pin}'.` | the connection could not be made |
| `{Namespace}.{Function} input '{Pin}' expects a Substrate value.` | non-Substrate value on a Substrate pin |
| `{Namespace}.{Function} input '{Pin}' expects a MaterialAttributes value.` | non-attributes value on a `MaterialAttributes` pin |
| `{Namespace}.{Function} input '{Pin}' does not accept Substrate values.` | Substrate value on a numeric pin |
| `{Namespace}.{Function} input '{Pin}' does not accept MaterialAttributes values.` | attributes value on a numeric pin |
| `UE.{Function} property '{Property}' must use Path(...) or an Unreal object path.` | an object property given a value that is neither a literal nor a `Path(…)` call |
| `Object property '{Property}' expects Path(...) or an absolute Unreal object path.` | an object property given a literal that is not an asset reference |
| `UE.{Function} property '{Property}' must use a literal value.` | a non-literal value for a literal property |
| `UE.{Function} property '{Property}': {Message}` | the literal could not be converted — the inner messages are in [Literal properties](#literal-properties) |
| `UE.{Function} OutputName must be a non-empty literal value.` | empty `OutputName` on a Custom node |
| `UE.{Function} OutputIndex is out of range for '{Class}'.` | negative, non-integer, or beyond the node's output count |
| `UE.{Function} OutputName must be a literal value.` | a non-literal output selector |
| `UE.{Function} output '{Name}' was not found on '{Class}'.` | no named output and no mask pseudo-name matched |
| `UE.{Function} created '{Class}', but it has no material outputs.` | the node exposes no outputs |
| `{Namespace}.{Function} output is not a Substrate value.` | `OutputType="Substrate"` on a node whose output is not Substrate |
| `Invalid reflected property target.` | defensive guard in the literal writer |
| `SampleTexture2D expects exactly two positional arguments: (textureObject, uv).` | wrong argument count or named arguments |

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

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
        float pulse = UE.Sine(OutputType = "float1", Input = UE.Time());

        // Explicit Class, an enum property by prefix-less name, a bool by its b-stripped alias.
        float3 scene = UE.Expression(Class = "SceneTexture", OutputType = "float4",
                                     SceneTextureId = "PostProcessInput0").rgb;

        // An object property through Path(...), and a pin by its pin name.
        float4 tex = UE.Expression(Class = "TextureSample", OutputType = "float4",
                                   Texture      = Path(Game, "Textures/T_Noise"),
                                   Coordinates  = UE.TexCoord(Index = 0));

        // Selecting a named output.
        float3 sel = UE.Expression(Class = "StaticSwitch", OutputType = "float3",
                                   True = Dimmed, False = tex.rgb, Value = true);

        Color = (scene + sel) * pulse;
    }
}
```

Generated nodes:

```text
Time                                  -> Sine                       (pulse)
SceneTexture (PPI_PostProcessInput0)  -> mask .rgb                  (scene)
TextureCoordinate (Index 0)           -> TextureSample.Coordinates
TextureSample (Texture = T_Noise)                                   (tex)
Constant3Vector (0.2, 0.2, 0.2)       -> StaticSwitch.True
StaticSwitch  (Value = true)                                        (sel)
Add, Multiply                                                       (Color)
```

## See also

- [Builtins](index.md) — the five call surfaces and the dispatch order
- [`UE.*` catalogue](ue.md) — the registered builtins this page is the fallback for
- [`OutputType`](output-type.md) — every accepted token, per surface, and the mask pseudo-names
- [Substrate](substrate.md) — the sibling namespace that shares this code path
- [`Path(…)`](../parameters/path.md) — asset-reference syntax for object properties
- [Parameters in Graph](../parameters/graph-usage.md) — the parameter pin-call form
- [Material attributes](../graph/material-attributes.md) — the 0-component value kind
- [Conversions](../graph/conversions.md) — authoritative component counts
- [Node reuse](../graph/node-reuse.md) — the two cache keys in context
- [Graph functions](../language/graph-function.md) — `UE.*` calls hoisted into Custom-node pins
- [Bridge](../tools/bridge.md) — the exported reflected expression manifest
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
