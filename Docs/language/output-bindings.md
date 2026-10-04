# Output bindings

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Output bindings**

The statements inside a `Shader`'s `Outputs` section that connect a graph value to a material
property input or to a pin on an explicitly created `UMaterialExpression`.

| | |
| :-- | :-- |
| Declared in | `.dsm` — inside a `Shader` block's `Outputs` section only |
| Kind | statement |
| Generates | a connection into `UMaterial`'s property inputs, or into a created output node |

The 2.0 spelling of a binding is an assignment to the material parameter, `m.EmissiveColor = Color;`
— see [DreamShaderLang 2.0](../language-v2/index.md); [`dsc migrate`](../tools/migrate.md) rewrites a
`.dsm` that way.

## Synopsis

```c
Outputs [=]
{
    <output-declaration>…
    <output-binding>…
}
```

```c
output-declaration := <type> <name> [ = <expression> ] ;

output-binding     := Base. <target> = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
                    | Expression( <key> = <value> [, <key> = <value> ]… ) { <pin-binding>… } [;]

pin-binding        := Pin[ <index> ] = <source> ;
```

`<source>` is an expression *(since 2.0.0)*: an output variable name, as 1.x wrote it, or anything a
`Graph` expression may be — `Base.FrontMaterial = Substrate.Layer(Coat, Body, 0.01);`. It is read after
the `Graph` has run, and it is held to the rules of a 1.x `Graph` expression (`DSH2200`–`DSH2222`). The
`[` and `]` around `<index>` are **literal**.

The section is read token by token, one statement at a time, by its first tokens:

| Statement begins with | Read as |
| :-- | :-- |
| two words, `<type> <name>` | output declaration, with or without an initializer *(initializer since 1.3.4)* |
| `Base` `.` `<name>` `=` | material-property binding |
| `Expression` `(` … `)` `.` | statement form of an expression binding |
| `Expression` `(` … `)` `{` | [block form](#block-form) *(since 1.9.0)* |
| anything else, including a `{` after anything but `Expression( … )` | [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) — or [`DSH3267`](../diagnostics/DSH3xxx.md#dsh3267) when it began as an `Expression( … )` target |

`Expression( … ) { … }` is the **only** construct that opens a brace inside `Outputs`, exactly as
`Group("Name") { … }` is the only one inside [`Properties`](properties.md).

A declaration becomes a local of the material's entry, ahead of the `Graph`; one without an
initializer starts at zero, as in 1.x (a texture or `Substrate` output has no zero and must be assigned
by the `Graph`). A binding becomes an assignment to the material after the `Graph`. Declaration grammar
is on [Inputs / Outputs / Results](inputs-outputs.md#outputs-in-a-shader).

## Binding target kinds

| Target text begins with | Kind | Meaning |
| :-- | :-- | :-- |
| `Base.` (case-insensitive) | material property | connects to one of `UMaterial`'s property inputs |
| `Expression(` (case-insensitive) | expression input | creates a `UMaterialExpression` and connects to one of its input pins |
| anything else | — | [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) |

## `Base.<target>` catalogue

*(since 2.0.0)* The names are the engine's own: every attribute of the engine's material attribute
table that has an `EMaterialProperty`, under the name the engine gives it, plus the 1.x aliases below.
A name is matched **exactly**; in a 1.x file a name that matches only ignoring case still resolves,
with the warning [`DSH5276`](../diagnostics/DSH5xxx.md#dsh5276) (legacy rule L19). A name that matches
nothing is [`DSH5200`](../diagnostics/DSH5xxx.md#dsh5200).

The spellings 1.x documented, with the property and value type each resolves to; the engine's table
may hold more names, and those resolve too:

| `Base.` spelling(s) | `EMaterialProperty` | Value type |
| :-- | :-- | :-- |
| `BaseColor` | `MP_BaseColor` | Float3 |
| `MaterialAttributes`, `Attributes` | `MP_MaterialAttributes` | MaterialAttributes |
| `FrontMaterial` *(since UE 5.4)* | `MP_FrontMaterial` | Substrate |
| `EmissiveColor`, `Emissive` | `MP_EmissiveColor` | Float3 |
| `Opacity` | `MP_Opacity` | Float1 |
| `OpacityMask` | `MP_OpacityMask` | Float1 |
| `Metallic` | `MP_Metallic` | Float1 |
| `Specular` | `MP_Specular` | Float1 |
| `Roughness` | `MP_Roughness` | Float1 |
| `Normal` | `MP_Normal` | Float3 |
| `AmbientOcclusion`, `AO` | `MP_AmbientOcclusion` | Float1 |
| `Refraction` | `MP_Refraction` | Float3 |
| `WorldPositionOffset`, `WPO` | `MP_WorldPositionOffset` | Float3 |
| `PixelDepthOffset`, `PDO` | `MP_PixelDepthOffset` | Float1 |
| `SubsurfaceColor` | `MP_SubsurfaceColor` | Float3 |
| `ClearCoat` | `MP_CustomData0` | Float1 |
| `ClearCoatRoughness` | `MP_CustomData1` | Float1 |
| `CustomData0` | `MP_CustomData0` | Float1 |
| `CustomData1` | `MP_CustomData1` | Float1 |
| `DiffuseColor` | `MP_DiffuseColor` | Float3 |
| `SpecularColor` | `MP_SpecularColor` | Float3 |
| `SurfaceThickness` | `MP_SurfaceThickness` | Float1 |
| `Displacement` | `MP_Displacement` | Float1 |
| `CustomizedUV0` … `CustomizedUV7` | `MP_CustomizedUVs0` … `MP_CustomizedUVs7` | Float2 |
| `Anisotropy` | `MP_Anisotropy` | Float1 |
| `Tangent` | `MP_Tangent` | Float3 |

> [!WARNING]
> 1.x also accepted `CustomizedUVs0` … `CustomizedUVs7`. The engine names those attributes
> `CustomizedUV0` … `CustomizedUV7`, so the `s` spellings are `DSH5200` *(since 2.0.0)*; write
> `CustomizedUV<n>`.

> [!NOTE]
> `ClearCoat` and `CustomData0` are the same material property, as are `ClearCoatRoughness` and
> `CustomData1`. Binding both spellings of a pair in one `Shader` writes the same input twice; the
> later statement wins, without a diagnostic.

### Fork-only targets

These attributes exist only in the Moon engine fork; on a stock Unreal build they are `DSH5200`. The
`Mooa…` aliases are the fork's pre-rename spellings.

| `Base.` spelling | `EMaterialProperty` | Value type |
| :-- | :-- | :-- |
| `MoonEncodedAttribute0`, `MooaEncodedAttribute0` | `MP_MoonEncodedAttribute0` | Float4 |
| `MoonEncodedAttribute1`, `MooaEncodedAttribute1` | `MP_MoonEncodedAttribute1` | Float4 |
| `MoonEncodedAttribute2`, `MooaEncodedAttribute2` | `MP_MoonEncodedAttribute2` | Float4 |
| `MoonEncodedAttribute3`, `MooaEncodedAttribute3` | `MP_MoonEncodedAttribute3` | Float4 |
| `MoonEncodedAttribute4`, `MooaEncodedAttribute4` | `MP_MoonEncodedAttribute4` | Float4 |

## Targets with side effects

### `Base.MaterialAttributes`

*(since 1.2.5)*

Binding `Base.MaterialAttributes` sets `UMaterial::bUseMaterialAttributes = true` — the
**Use Material Attributes** checkbox — so the material exposes the single attributes input instead of
the individual property inputs. The bound value must be a `MaterialAttributes` value; anything else is
[`DSH4228`](../diagnostics/DSH4xxx.md#dsh4228).

See [MaterialAttributes](../graph/material-attributes.md) for the value type, member writes and
reads.

### `Base.FrontMaterial`

*(since UE 5.4)*

The bound value must be a Substrate value, produced by the [`Substrate.*` builtins](../builtins/substrate.md)
or the [Substrate sugar](../language-v2/substrate.md); anything else is `DSH4228`.

*(since 2.0.0)* `FrontMaterial` is an ordinary attribute: the 1.x generator's extra rules for it — forcing
the shading model to Substrate, refusing an explicit `ShadingModel` other than `"Substrate"`, refusing
it beside `Base.MaterialAttributes`, and the whole-surface `Custom`-node path — are not applied. Write
`ShadingModel = "Substrate";` in `Settings` where the material needs it (on UE 5.3 that setting is
[`DSH7128`](../diagnostics/DSH7xxx.md#dsh7128)). How a material written against the legacy attributes is
read in a Substrate project is the `Substrate =` setting — see
[Substrate sugar](../language-v2/substrate.md#one-source-two-kinds-of-project--substrate-).

```c
Outputs = {
    Substrate Surface;
    Base.FrontMaterial = Surface;
}
Graph = {
    Surface = Substrate.Unlit(EmissiveColor = Color);
}
```

## The `Expression( … ).Pin[i]` target

The non-`Base` binding form creates an arbitrary `UMaterialExpression` — in practice a *custom output*
node — and connects the source to one of its input pins.

```c
Expression( Class = "<ExpressionClass>" [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
```

Both forms of the target become one `UE.Expression(Class = "…", …)` node call in the material's entry,
so a target is bound by the same rules as a [`UE.Expression`](../builtins/ue-expression.md) call in a
`Graph`.

### Requirements, in the order they are checked

| Requirement | Code when violated |
| :-- | :-- |
| the head is `Expression(` `<key> = <value>` [`,` …] `)`, each value non-empty | [`DSH3267`](../diagnostics/DSH3xxx.md#dsh3267) |
| no key is written twice (compared ignoring case) | `DSH3267` |
| a `Class` argument is present | `DSH3267` |
| after `)` comes `.Pin[<index>] = <source>` or a `{ … }` block, the index a whole-number literal | `DSH3267` |
| something follows `=` | [`DSH3270`](../diagnostics/DSH3xxx.md#dsh3270) |
| the class is one the engine has | [`DSH5212`](../diagnostics/DSH5xxx.md#dsh5212) |
| the index names an input pin of the class | [`DSH5285`](../diagnostics/DSH5xxx.md#dsh5285) |
| every other argument names a pin or a property of the class | [`DSH5213`](../diagnostics/DSH5xxx.md#dsh5213) |

`Class` is matched ignoring case. The reuse key below lower-cases every other key and unquotes every
value; the node call carries each argument as written.

### Class resolution

*(since 2.0.0)* The class is looked up in the engine's expression catalog, by exact name, in this
order: the short name (`ThinTranslucentMaterialOutput`), the class name
(`MaterialExpressionThinTranslucentMaterialOutput`), the class path
(`/Script/Engine.MaterialExpressionThinTranslucentMaterialOutput`), the short name with
`MaterialExpression` put in front, and the C++ name with its `U`
(`UMaterialExpressionThinTranslucentMaterialOutput`), which 1.x refused. In a 1.x file a name that
matches one class only ignoring case resolves too, with the warning `DSH5276`. Anything else is
`DSH5212`. The same rule is documented on [`UE.Expression`](../builtins/ue-expression.md).

### Other arguments

Every argument other than `Class` is matched against the class: an input pin first, then a reflected
property. A value is read as a number, `true` / `false`, a quoted string, or a bare word, which names a
value of the graph or, for an enumerated property, an enumerator.

*(since 2.0.0)* An argument that names an **input** pin connects that pin, as `Pin[<index>]` would;
1.x refused it. Connecting one pin both ways is [`DSH4215`](../diagnostics/DSH4xxx.md#dsh4215). A name
that is neither a pin nor a property is `DSH5213` — except in a 1.x file for a graph value, which is
connected by that name once the node exists ([`DSH5291`](../diagnostics/DSH5xxx.md#dsh5291), info;
[`DSH8212`](../diagnostics/DSH8xxx.md#dsh8212) when the built node has no such pin). A value the
property cannot take is [`DSH8213`](../diagnostics/DSH8xxx.md#dsh8213).

### Pin indices and node reuse

- `Pin[<index>]` is the zero-based index into the node's input list, in the order the engine declares
  them. `Expression(Class="ThinTranslucentMaterialOutput").Pin[0]` is `TransmittanceColor` and
  `.Pin[1]` is `SurfaceCoverage`. An index past the end is `DSH5285`.
- Each pin of a node may be bound **once**, across the statement and [block](#block-form) forms:
  [`DSH3268`](../diagnostics/DSH3xxx.md#dsh3268).
- Nodes are **de-duplicated by class plus sorted argument list**, within one `Outputs` section. Two
  bindings whose `Expression( … )` specification is identical share one node and bind different pins;
  a difference in any argument creates a second node, and so does the same head in a second `Outputs`
  section.
- A source must be a value of the graph: a name nothing declares is
  [`DSH4200`](../diagnostics/DSH4xxx.md#dsh4200), and a value the pin cannot take is
  [`DSH5214`](../diagnostics/DSH5xxx.md#dsh5214). A required pin left unconnected is the warning
  [`DSH5279`](../diagnostics/DSH5xxx.md#dsh5279).
- Where the nodes land is the [graph layout](../generation/graph-layout.md)'s business.

Pin order for the custom outputs this form is most often used for:

| `Class` | `Pin[i]`, in order |
| :-- | :-- |
| `ThinTranslucentMaterialOutput` | `0` TransmittanceColor, `1` SurfaceCoverage |
| `ClearCoatNormalCustomOutput` | `0` Input |
| `RuntimeVirtualTextureOutput` | `0` BaseColor, `1` Specular, `2` Roughness, `3` Normal, `4` WorldHeight, `5` Opacity, `6` Mask, `7` Displacement, `8` Mask4 |

Any other `UMaterialExpressionCustomOutput` subclass works the same way; its pin order is the order
the engine declares the inputs in, which the material editor shows top to bottom on the node. A
worked `RuntimeVirtualTextureOutput` file is [example 15](../examples/index.md#15-runtime-virtual-texture-sampling-and-writing).

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;
    float3 CoatNormal;

    // Both statements reuse one ThinTranslucentMaterialOutput node.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[0] = Transmittance;
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;

    // A different class → a second node.
    Expression(Class="ClearCoatNormalCustomOutput").Pin[0] = CoatNormal;
}
```

## Block form

*(since 1.9.0)*

A terminal node with many pins has to repeat its whole `Expression( … )` specification once per pin
in the statement form, and because the specification *is* the de-duplication key, **one forgotten
argument silently splits the node in two**. The block form writes the specification once:

```c
Expression( <key> = <value> [, <key> = <value> ]… )
{
    Pin[ <index> ] = <source> ;
    …
}
```

```c
Outputs = {
    float _PhaseG_Out;
    float _PhaseG2_Out;
    float _PhaseBlend_Out;

    Expression(Class="VolumetricAdvancedMaterialOutput",
        PerSamplePhaseEvaluation="false",
        MultiScatteringApproximationOctaveCount=0,
        bGroundContribution="false")
    {
        Pin[0] = _PhaseG_Out;
        Pin[1] = _PhaseG2_Out;
        Pin[2] = _PhaseBlend_Out;
    }
}
```

### The one-node guarantee

The block and the statement form read into the same thing: one node call per reuse key, with every
pin bound under that key as one of its arguments. A block's pins all share the head's class and
argument list, which is exactly the [reuse key](#pin-indices-and-node-reuse) — so a block of *N* pins
always produces **one** node. The argument list cannot drift between pins, because there is only one
copy of it.

Nothing else changes: the block form and the statement form are indistinguishable downstream. They
share the same class resolution, the same argument checks, the same pin-index checks and the same
diagnostics.

### Rules

| Rule | Detail |
| :-- | :-- |
| what may open a brace | only `Expression( … )`. A `{` after a variable or a `Base.` target is `DSH3266`; after an `Expression( … ).Pin[i]` target it is `DSH3267` |
| head shape | `Expression(` … `)` and nothing after the closing `)` — the pin selector is written per statement inside the block |
| statements inside | only `Pin[ <index> ] = <source> ;` — `DSH3267` otherwise |
| comments and blank lines | allowed anywhere inside the block |
| empty block | [`DSH3269`](../diagnostics/DSH3xxx.md#dsh3269) — a block that binds no pin creates nothing, so it is rejected rather than ignored |
| unclosed block | the parse runs on to the end of the file: [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) |
| trailing `;` | optional after the closing `}`, as after `Group("Name") { … }` |
| repeats | a block may be written more than once for the same node, and may be mixed freely with statement-form bindings for it, within one `Outputs` section |

### Mixing the two forms

A block and a loose statement that agree on class and argument list address the **same** node, so
they fill different pins of it:

```c
Outputs = {
    float3 Transmittance;
    float  Coverage;

    Expression(Class="ThinTranslucentMaterialOutput")
    {
        Pin[0] = Transmittance;
    }

    // Same specification → same node → this fills its other pin.
    Expression(Class="ThinTranslucentMaterialOutput").Pin[1] = Coverage;
}
```

### Each pin once

The "a pin may be bound once" rule is checked while the section is read, across both forms, keyed
on class + sorted argument list + pin index. A second binding of the same pin is `DSH3268`, reported
at the second binding.

> [!NOTE]
> Two blocks whose argument lists differ — even by one argument — are two different nodes, and each
> of them has its own `Pin[0]`. That is not a double bind and is not diagnosed; it is the same rule
> that made the repetition dangerous in the statement form.

### Diagnostics

The 1.x parser's codes for this form, `DSH3133`–`DSH3137`, are no longer raised *(since 2.0.0)*:

| 1.x code | Raised now |
| :-- | :-- |
| `DSH3133` — a brace after anything but `Expression( … )` | `DSH3266`, or `DSH3267` after an `Expression( … ).Pin[i]` target |
| `DSH3134` — an unterminated block | `DSH2150` |
| `DSH3135` — a statement other than `Pin[i] = <source>;` in a block | `DSH3267` |
| `DSH3136` — an empty block | `DSH3269` |
| `DSH3137` — a pin bound twice | `DSH3268` |

## The reserved name `return`

*(since 2.0.0)* `return` is a keyword in every file. It cannot name an output declaration
(`DSH3266`) and is not a binding source: `Base.EmissiveColor = return;` is
[`DSH2151`](../diagnostics/DSH2xxx.md#dsh2151). 1.x reserved the name for its whole-surface
`Custom`-node path, which the compiler no longer has.

## Validation rules

| Condition | Code |
| :-- | :-- |
| a declared type that is not a type | [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201) |
| a name declared twice in `Outputs` | [`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220) |
| a source name that nothing declares | `DSH4200` |
| a `Base.` name the material has not | `DSH5200` |
| a value the material pin cannot take; a wider vector is not narrowed into a material pin, not even in a 1.x file | `DSH4228` |
| a value an `Expression( … )` pin cannot take | `DSH5214` |

A `Graph` declaration of a name `Outputs` declares is read as an assignment to that output, which is
what 1.x made of it.

## Notes

- A `Shader` with no `Outputs` section builds with the warning
  [`DSH2256`](../diagnostics/DSH2xxx.md#dsh2256); one that has neither a `Graph` nor anything in
  `Outputs` is [`DSH2255`](../diagnostics/DSH2xxx.md#dsh2255).
- Bindings and declarations may be interleaved freely; a binding may reference a variable declared
  later in the same section.
- A repeated `Outputs` section appends to both lists.
- No `[ … ]` metadata block is accepted on any `Outputs` statement in a `Shader`.
- Each binding is routed through a generated `NamedReroute` pair named `DS_<pin>_<n>` after the
  `Base.` name as written, so the material root node stays readable in a large graph. See
  [Graph layout](../generation/graph-layout.md).
- Binding to a property that the current shading model or blend mode does not use is not diagnosed —
  Unreal simply leaves the input unread. Check the material editor's greyed-out inputs. A property the
  material has no input for at all is [`DSH8217`](../diagnostics/DSH8xxx.md#dsh8217).

## Diagnostics

Every diagnostic carries the line and column of the statement it is about.

### Reading the section

| Code | Raised when |
| :-- | :-- |
| `DSH3266` | the section is not opened with `{`; a statement is neither a declaration nor a binding; a declaration does not end with `;` |
| `DSH3267` | an `Expression( … )` target is malformed: its head, its pin selector, a missing `Class`, a repeated key, a statement other than `Pin[i] = <source>;` in a block |
| `DSH3268` | a pin of one node is bound twice |
| `DSH3269` | an `Expression( … ) { }` block binds no pin |
| `DSH3270` | an `=` has nothing after it |
| `DSH2150` | the file ends inside the section or inside a block |
| `DSH2151` | a source is not an expression, such as `return` |
| [`DSH2200`](../diagnostics/DSH2xxx.md#dsh2200)–[`DSH2222`](../diagnostics/DSH2xxx.md#dsh2222) | a source or an initializer uses what a 1.x `Graph` expression does not have |

### Binding

| Code | Raised when |
| :-- | :-- |
| `DSH4201` | a declared type is not a type |
| `DSH4220` | a name is declared twice |
| `DSH4200` | a source names nothing declared |
| `DSH4228` | a value the material pin cannot take |
| `DSH5200` | an unknown `Base.` name |
| `DSH5276` | warning: a `Base.` name, a class or an argument matched only ignoring case |
| `DSH5212` | `Class=` names no expression class of this engine |
| `DSH5285` | `Pin[i]` is past the node's last input |
| `DSH5213` | an argument names neither a pin nor a property of the class |
| `DSH5291` | info: in a 1.x file, an unknown argument carrying a graph value is connected by name later |
| `DSH4215` | one pin connected twice in one node call |
| `DSH5214` | a value an `Expression( … )` pin cannot take |
| `DSH5279` | warning: a required pin of the node is left unconnected |

### Building the asset

| Code | Raised when |
| :-- | :-- |
| `DSH8217` | the material has no input for the bound property |
| [`DSH8211`](../diagnostics/DSH8xxx.md#dsh8211) | the expression class is in the catalog but not in the running engine |
| `DSH8212` | a pin connected by name does not exist on the built node |
| `DSH8213` | a reflected argument's value cannot be written to the node |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | a node could not be created |

The complete list is in the [diagnostics index](../diagnostics/index.md).

## Example

```c
Shader(Name="DreamShaderTests/Corpus/M_Outputs")
{
    Properties = {
        vec3  Tint = vec3(0.4, 0.8, 1.0);
        float A    = 0.75;
    }

    Settings = {
        Domain       = "Surface";
        ShadingModel = "Unlit";
    }

    Outputs = {
        vec3  Color;
        float Alpha;

        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph = {
        Color = Tint;
        Alpha = A;
    }
}
```

Resulting connections:

```text
UMaterial /Game/DreamShaderTests/Corpus/M_Outputs
  MP_EmissiveColor  <- NamedReroute DS_EmissiveColor_<n>  <- VectorParameter "Tint"  RGB output
  MP_Opacity        <- NamedReroute DS_Opacity_<n>        <- ScalarParameter "A"     R output
  bUseMaterialAttributes = false
```

## See also

- [Shader](shader.md) — the block that owns `Outputs`
- [Inputs / Outputs / Results](inputs-outputs.md) — the declaration side of `Outputs`
- [MaterialAttributes](../graph/material-attributes.md) — the attributes value type and member writes
- [Substrate builtins](../builtins/substrate.md) — producing a value for `Base.FrontMaterial`
- [`UE.Expression`](../builtins/ue-expression.md) — the same `Class=` resolution used in `Properties` and `Graph`
- [Metadata](../parameters/metadata.md) — the literal grammar shared by `Expression( … )` arguments
- [Material settings](../settings/material.md) — `ShadingModel`, `BlendMode` and the reflected keys
- [Shading model / blend mode values](../settings/material-enums.md) — every accepted enum spelling
- [Graph](../graph/index.md) — where output variables are assigned
- [Graph layout](../generation/graph-layout.md) — the reroute pairs and `Material Output` grouping
- [DreamShaderLang 2.0](../language-v2/index.md) — `m.<Attribute> = …`, the `.dss` spelling
- [Diagnostics index](../diagnostics/index.md) — every code
