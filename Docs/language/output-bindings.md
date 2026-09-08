# Output bindings

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Output bindings**

The statements inside a `Shader`'s `Outputs` section that connect a graph variable to a material
property input or to a pin on an explicitly created `UMaterialExpression`.

| | |
| :-- | :-- |
| Declared in | `.dsm` — inside a `Shader` block's `Outputs` section only |
| Kind | statement |
| Generates | a connection into `UMaterial`'s property inputs, or into a created output node |

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

`<source>` is an output variable name, or the reserved name `return`. The `[` and `]` around
`<index>` are **literal**.

Statement classification is per statement, not per section:

| Statement shape | Classified as |
| :-- | :-- |
| no top-level `=` | bare output-variable declaration |
| top-level `=`, left side is a valid typed declaration | initialized output declaration *(since 1.3.4)* |
| top-level `=`, left side is **not** a valid typed declaration | output **binding** |
| a `{` at statement level, after `Expression( … )` | [block form](#block-form) *(since 1.9.0)* |
| a `{` at statement level, after anything else | error `DSH3133` |

`Expression( … ) { … }` is the **only** construct that opens a brace inside `Outputs`, exactly as
`Group("Name") { … }` is the only one inside [`Properties`](properties.md).

Declaration grammar is on [Inputs / Outputs / Results](inputs-outputs.md#outputs-in-a-shader).

## Binding target kinds

| Target text begins with | Kind | Meaning |
| :-- | :-- | :-- |
| `Base.` (case-insensitive) | material property | connects to one of `UMaterial`'s property inputs |
| `Expression` (case-insensitive) | expression input | creates a `UMaterialExpression` and connects to one of its input pins |
| anything else | — | `Output binding target '{Target}' must start with Base. for material outputs or Expression(...) for output nodes.` |

## `Base.<target>` catalogue

Target names are matched **case-insensitively**. Every accepted spelling is listed; there are no
wildcards and no "and others".

**46 spellings resolving to 31 distinct `EMaterialProperty` values** on UE 5.4 and newer.
On UE 5.3 `FrontMaterial` does not exist, leaving 45 spellings / 30 properties. Five further
spellings exist only in a custom engine fork — see [Fork-only targets](#fork-only-targets).

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
| `CustomizedUV0`, `CustomizedUVs0` | `MP_CustomizedUVs0` | Float2 |
| `CustomizedUV1`, `CustomizedUVs1` | `MP_CustomizedUVs1` | Float2 |
| `CustomizedUV2`, `CustomizedUVs2` | `MP_CustomizedUVs2` | Float2 |
| `CustomizedUV3`, `CustomizedUVs3` | `MP_CustomizedUVs3` | Float2 |
| `CustomizedUV4`, `CustomizedUVs4` | `MP_CustomizedUVs4` | Float2 |
| `CustomizedUV5`, `CustomizedUVs5` | `MP_CustomizedUVs5` | Float2 |
| `CustomizedUV6`, `CustomizedUVs6` | `MP_CustomizedUVs6` | Float2 |
| `CustomizedUV7`, `CustomizedUVs7` | `MP_CustomizedUVs7` | Float2 |
| `Anisotropy` | `MP_Anisotropy` | Float1 |
| `Tangent` | `MP_Tangent` | Float3 |

> [!NOTE]
> `ClearCoat` and `CustomData0` are the same material property, as are `ClearCoatRoughness` and
> `CustomData1`. Binding both spellings of a pair in one `Shader` writes the same input twice; the
> later statement wins, without a diagnostic.

An unrecognized name fails with `Unsupported material output '{Name}'.`

### Fork-only targets

These resolve only when the engine defines `MOON_ENGINE`. On a stock Unreal build they are
`Unsupported material output`.

| `Base.` spelling | `EMaterialProperty` | Value type |
| :-- | :-- | :-- |
| `MoonEncodedAttribute0`, `MooaEncodedAttribute0` | `MP_MoonEncodedAttribute0` | Float4 |
| `MoonEncodedAttribute1`, `MooaEncodedAttribute1` | `MP_MoonEncodedAttribute1` | Float4 |
| `MoonEncodedAttribute2`, `MooaEncodedAttribute2` | `MP_MoonEncodedAttribute2` | Float4 |
| `MoonEncodedAttribute3`, `MooaEncodedAttribute3` | `MP_MoonEncodedAttribute3` | Float4 |
| `MoonEncodedAttribute4`, `MooaEncodedAttribute4` | `MP_MoonEncodedAttribute4` | Float4 |

## Targets with side effects

Two `Base.` targets change the generated material's render state as a side effect of being bound.

### `Base.MaterialAttributes`

*(since 1.2.5)*

Binding `Base.MaterialAttributes` sets `UMaterial::bUseMaterialAttributes = true` — the
**Use Material Attributes** checkbox — so the material exposes the single attributes input instead of
the individual property inputs. The bound variable must carry a `MaterialAttributes` value:

```
{File}: Material output '{Name}' expects a MaterialAttributes value.
```

See [MaterialAttributes](../graph/material-attributes.md) for the value type, member writes and
reads.

### `Base.FrontMaterial`

*(since UE 5.4)*

Binding `Base.FrontMaterial`:

- **force-sets the shading model to Substrate** (`MSM_Strata`);
- requires that `Settings` either declares `ShadingModel="Substrate"` (or its accepted alias
  `ShadingModel="Strata"`) or declares no `ShadingModel` at all — anything else fails with
  `{File}: Base.FrontMaterial requires ShadingModel="Substrate" or no explicit ShadingModel setting.`;
- is **mutually exclusive** with `Base.MaterialAttributes` —
  `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.`;
- cannot be driven by the whole-surface `Custom`-node path —
  `{File}: Base.FrontMaterial expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes.`;
- does not exist on UE 5.3, where it reports
  `Base.FrontMaterial requires Unreal Engine 5.4 or newer.`

The bound variable must be a Substrate value, produced by the
[`Substrate.*` builtins](../builtins/substrate.md):

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
node — and connects the source variable to one of its input pins.

```c
Expression( Class = "<ExpressionClass>" [, <key> = <value> ]… ) . Pin[ <index> ] = <source> ;
```

### Requirements, in the order they are checked

| Requirement | Diagnostic when violated |
| :-- | :-- |
| the target text is non-empty | `Output binding target cannot be empty.` |
| the target begins with `Base.` or `Expression` | `Output binding target '{Target}' must start with Base. for material outputs or Expression(...) for output nodes.` |
| a balanced `( … )` is present | `Invalid output expression target '{Target}'.` |
| the text before `(` is exactly `Expression` | `Unsupported output target '{Target}'.` |
| the text after `)` starts with `.` | `Expression output target '{Target}' must select a pin with .Pin[index].` |
| that suffix is `Pin[` … `]` (`Pin[` matched case-insensitively) | `Expression output target '{Target}' must use .Pin[index] syntax.` |
| the index parses as an integer ≥ 0 | `Expression output target '{Target}' has an invalid pin index.` |
| every argument is `Key=Value` | `Expression output target argument '{Argument}' must use Key=Value syntax.` |
| no argument has an empty key or value | `Invalid expression output target argument '{Argument}'.` |
| no argument key is repeated | `Expression output target argument '{Key}' is declared more than once.` |
| a `Class` argument is present | `Expression output target '{Target}' must specify Class="...".` |

Argument keys are normalized (trimmed and lower-cased); values are unquoted and trimmed.

### Class resolution

A value containing `/` or `.` is loaded directly as an object path and accepted only if the loaded
class derives from `UMaterialExpression`. Otherwise a candidate list is built — `<Value>`, plus
`U<Value>`, `MaterialExpression<Value>` and `UMaterialExpression<Value>` when `<Value>` does not
already start with that prefix — and every non-abstract `UMaterialExpression` subclass is scanned for
a name equal to any candidate, ignoring case.

> [!IMPORTANT]
> The scan compares against the **reflected** class name, which carries no `U` prefix:
> `UMaterialExpressionThinTranslucentMaterialOutput` is reflected as
> `MaterialExpressionThinTranslucentMaterialOutput`. A specifier that begins with `U` therefore never
> matches. `Class="ThinTranslucentMaterialOutput"` and
> `Class="MaterialExpressionThinTranslucentMaterialOutput"` name the same class;
> `Class="UMaterialExpressionThinTranslucentMaterialOutput"` fails with
> `Output target '{Target}' could not resolve MaterialExpression class '{Class}'.`
> The same rule is documented in detail on [`UE.Expression`](../builtins/ue-expression.md).

### Other arguments

Every argument other than `Class` is written to the same-named reflected property on the created
node, using the same literal grammar as [declaration metadata](../parameters/metadata.md).

> [!WARNING]
> An argument that names an **input** property of the node (an `FExpressionInput`) is rejected rather
> than wired: `Output target '{Target}': inline input property '{Key}' is not supported yet. Bind through .Pin[index] instead.`
> Inputs are reachable only through the `.Pin[index]` suffix.

`Class` stays in the argument map but is skipped when reflected properties are written, so it never
produces a "not a property" error.

### Pin indices and node reuse

- `Pin[<index>]` is the zero-based index into the node's input list, in the order the engine declares
  them. `Expression(Class="ThinTranslucentMaterialOutput").Pin[0]` is `TransmittanceColor` and
  `.Pin[1]` is `SurfaceCoverage`.
- An index past the end fails with `Output target '{Target}' does not have Pin[{Index}].`
- Each pin may be bound **once**, and the check spans the statement and
  [block](#block-form) forms combined: `DSH3137` at parse time, `Output target pin '{Target}' is bound more than once.` at generation.
- Nodes are **de-duplicated by class plus sorted argument list**. Two bindings whose
  `Expression( … )` specification is identical share one node and bind different pins; a difference in
  any argument creates a second node.
- The source variable must have been **declared** in `Outputs`:
  `Output variable '{Name}' must declare an explicit type before binding to expression target '{Target}'.`
- Created output-target nodes are placed at graph X `1200`, Y starting `200`, stepping `+220`.

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

The block is **lowered by the parser** to one ordinary binding per `Pin[i]`, each carrying the
`Class` and argument list computed from the shared head. Every lowered binding therefore has a
byte-identical `Expression( … )` specification, which is exactly the
[reuse key](#pin-indices-and-node-reuse) — so a block of *N* pins always produces **one** node. The
argument list cannot drift between pins, because there is only one copy of it.

Nothing else changes: the block form and the statement form are indistinguishable downstream. They
share the same class resolution, the same reflected-argument writes, the same pin-index checks and
the same diagnostics.

### Rules

| Rule | Detail |
| :-- | :-- |
| what may open a brace | only `Expression( … )`. A `{` after a variable, a `Base.` target or an `Expression( … ).Pin[i]` target is `DSH3133` |
| head shape | `Expression(` … `)` and nothing after the closing `)` — the pin selector is written per statement inside the block |
| statements inside | only `Pin[ <index> ] = <source> ;` — `DSH3135` otherwise |
| comments and blank lines | allowed anywhere inside the block |
| empty block | `DSH3136` — a block that binds no pin creates nothing, so it is rejected rather than ignored |
| trailing `;` | optional after the closing `}`, as after `Group("Name") { … }` |
| repeats | a block may be written more than once for the same node, and may be mixed freely with statement-form bindings for it |

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

The "a pin may be bound once" rule is checked at **parse time** across both forms and across
repeated `Outputs` sections, keyed on class + sorted argument list + pin index. A second binding of
the same pin fails with `DSH3137`, naming the pin and both sources:

```text
Output target pin 'Expression(Class="ThinTranslucentMaterialOutput").Pin[0]' is bound more than once:
first to 'Transmittance', then to 'TransmittanceAgain'.
```

> [!NOTE]
> Two blocks whose argument lists differ — even by one argument — are two different nodes, and each
> of them has its own `Pin[0]`. That is not a double bind and is not diagnosed; it is the same rule
> that made the repetition dangerous in the statement form.

### Diagnostics

| Code | Message |
| :-- | :-- |
| `DSH3133` | `Unexpected brace block in Outputs near '{Head}'. Only Expression(Class="...") opens a brace block here; every other Outputs statement ends with ';'.` |
| `DSH3134` | `Unterminated Expression(...) block in Outputs after '{Head}'.` |
| `DSH3135` | `Invalid statement '{Statement}' inside an Expression(...) block. Only Pin[index] = <source>; is allowed there.` |
| `DSH3136` | `The Expression(...) block for '{Head}' binds no pin. Write at least one Pin[index] = <source>; inside it, or delete the block.` |
| `DSH3137` | `Output target pin '{Target}' is bound more than once: first to '{First}', then to '{Second}'. …` |

A head written across several lines is folded to one line in these messages and in every downstream
diagnostic, so `{Head}` and `{Target}` always read as a single line.

## The reserved name `return`

`return` may be used as a binding **source** but never as a declaration name.

| Rule | Diagnostic |
| :-- | :-- |
| `return` may not be declared | `Outputs declarations cannot use the reserved name 'return'.` |
| `return` may only bind to `Base.*` targets | `The reserved output name 'return' can only bind to Base material properties.` |
| all `Base.*` targets `return` binds to must agree in type | `The return value is bound to material properties with incompatible types.` |
| a `Shader` that has a `Graph` block may not bind `return` at all | `{File}: Graph blocks do not support binding Outputs to the reserved name 'return'.` |

## Validation rules

Applied after parsing, before any node is created.

| Rule | Diagnostic |
| :-- | :-- |
| a declared type must resolve | `Unsupported output type '{Type}' for '{Name}'.` |
| `Substrate` declarations need UE 5.4+ | `Output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` |
| the same name may not be declared twice with different types | `Output variable '{Name}' is declared with conflicting types.` |
| one variable bound to two `Base.*` targets of different types | `Output variable '{Name}' is bound to incompatible material properties.` |
| a declared type conflicting with the bound property's type | `Output variable '{Name}' is declared as '{Type}' but bound material property '{Property}' expects a different type.` |
| a variable bound to an `Expression(…)` target with no declaration | `Output variable '{Name}' must declare an explicit type before binding to expression target '{Target}'.` |
| the target name resolves | `Unsupported material output '{Name}'.` |

## Notes

- A `Shader` that declares no bindings parses with the warning
  `No Outputs block was provided. Generation requires explicit material property bindings.` and then
  fails generation with `{File}: Outputs block is required.` Bindings, not declarations, are what
  make a material.
- Bindings and declarations may be interleaved freely; a binding may reference a variable declared
  later in the same section.
- A repeated `Outputs` section appends to both lists.
- No `[ … ]` metadata block is accepted on any `Outputs` statement in a `Shader`.
- Each binding is routed through a generated `NamedReroute` pair, so the material root node stays
  readable in a large graph. Declarations are named `DS_<Sanitized>` with an index suffix. See
  [Graph layout](../generation/graph-layout.md).
- Binding to a property that the current shading model or blend mode does not use is not diagnosed —
  Unreal simply leaves the input unread. Check the material editor's greyed-out inputs.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Output binding target cannot be empty.` | the text left of `=` is empty |
| `Output binding target '{Target}' is empty.` | the target is `Base.` with nothing after it |
| `Output binding target '{Target}' must start with Base. for material outputs or Expression(...) for output nodes.` | unrecognized target prefix |
| `Invalid output binding '{Statement}'.` | the right side of the binding is empty |
| `Invalid output declaration initializer '{Statement}'.` | an initialized declaration with an empty right side |
| `Invalid output expression target '{Target}'.` | unbalanced parentheses in `Expression( … )` |
| `Unsupported output target '{Target}'.` | text before `(` is not exactly `Expression` |
| `Expression output target '{Target}' must select a pin with .Pin[index].` | nothing, or no `.`, after `)` |
| `Expression output target '{Target}' must use .Pin[index] syntax.` | the suffix after `.` is not `Pin[ … ]` |
| `Expression output target '{Target}' has an invalid pin index.` | the index is not an integer, or is negative |
| `Expression output target argument '{Argument}' must use Key=Value syntax.` | positional argument |
| `Invalid expression output target argument '{Argument}'.` | empty key or empty value |
| `Expression output target argument '{Key}' is declared more than once.` | duplicate argument key |
| `Expression output target '{Target}' must specify Class="...".` | no `Class` argument |
| `Invalid typed declaration '{Statement}'.` | a bare declaration that is not a valid typed declaration |
| `Unexpected brace block in Outputs near '{Head}'. …` | a `{` after anything but `Expression( … )` *(since 1.9.0)* |
| `Unterminated Expression(...) block in Outputs after '{Head}'.` | the block's closing `}` is missing *(since 1.9.0)* |
| `Invalid statement '{Statement}' inside an Expression(...) block. …` | a statement other than `Pin[index] = <source>;` inside a block *(since 1.9.0)* |
| `The Expression(...) block for '{Head}' binds no pin. …` | an empty block *(since 1.9.0)* |
| `Output target pin '{Target}' is bound more than once: first to '{First}', then to '{Second}'. …` | the same pin bound twice, in either form *(since 1.9.0)* |

### Validation

| Message | Cause |
| :-- | :-- |
| `Outputs declarations cannot use the reserved name 'return'.` | `return` used as a declaration name |
| `Unsupported output type '{Type}' for '{Name}'.` | declared type does not resolve |
| `Output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` declaration on UE 5.3 |
| `Output variable '{Name}' is declared with conflicting types.` | two declarations of the same name |
| `Unsupported material output '{Name}'.` | unknown `Base.` target |
| `Base.FrontMaterial requires Unreal Engine 5.4 or newer.` | `Base.FrontMaterial` on UE 5.3 |
| `The reserved output name 'return' can only bind to Base material properties.` | `return` bound to an `Expression(…)` target |
| `The return value is bound to material properties with incompatible types.` | `return` bound to two incompatible `Base.*` |
| `Output variable '{Name}' is bound to incompatible material properties.` | one variable bound to two `Base.*` of different types |
| `Output variable '{Name}' is declared as '{Type}' but bound material property '{Property}' expects a different type.` | declaration/target type conflict |
| `Output variable '{Name}' must declare an explicit type before binding to expression target '{Target}'.` | undeclared source on an `Expression(…)` binding |

### Generation time

| Message | Cause |
| :-- | :-- |
| `{File}: Outputs block is required.` | the `Shader` declared no bindings |
| `{File}: Base.FrontMaterial and Base.MaterialAttributes cannot be used by the same Shader.` | both bound |
| `{File}: Base.FrontMaterial requires ShadingModel="Substrate" or no explicit ShadingModel setting.` | conflicting explicit shading model |
| `{File}: Graph blocks do not support binding Outputs to the reserved name 'return'.` | `return` bound in a `Shader` with a `Graph` |
| `{File}: Graph output '{Name}' does not match its declared type.` | assigned value's type differs from the declaration |
| `{File}: Material output '{Name}' expects a MaterialAttributes value.` | non-attributes value bound to `Base.MaterialAttributes` |
| `{File}: Material output '{Name}' expects a Substrate value.` | non-Substrate value bound to a Substrate target |
| `{File}: Material output '{Name}' expects a numeric value, but got Substrate.` | Substrate value bound to a numeric target |
| `{File}: Failed to find material property '{Property}' while connecting Graph output '{Name}'.` | the property input could not be located on the material |
| `{File}: Failed to find material property '{Property}' while connecting '{Name}'.` | same, on the `Custom`-node path |
| `{File}: Base.FrontMaterial expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes.` | `Base.FrontMaterial` on the whole-surface `Custom` path |
| `{File}: Output '{Name}' is declared as Substrate and cannot be generated by a material Custom node. Use a Graph block and Substrate.* nodes.` | Substrate output on the `Custom` path |
| `{File}: Material output '{Name}' expects a Substrate value and cannot be driven by a material Custom node. Use a Graph block and Substrate.* nodes.` | same, target side |
| `Output target '{Target}' could not resolve MaterialExpression class '{Class}'.` | `Class=` names no known expression class |
| `Output target '{Target}' failed to create '{Class}'.` | node construction failed |
| `Output target '{Target}': '{Key}' is not a property on '{Class}'.` | unknown reflected argument |
| `Output target '{Target}': inline input property '{Key}' is not supported yet. Bind through .Pin[index] instead.` | an argument naming an input pin |
| `Output target '{Target}': {Detail}` | a reflected value could not be written |
| `Output target '{Target}' does not have Pin[{Index}].` | pin index out of range |
| `Output target pin '{Target}' is bound more than once.` | the same pin bound twice |
| `Invalid output source or target expression.` | the binding's source produced no node |

The complete cross-stage list is in the [diagnostics index](../diagnostics/index.md).

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
  MP_EmissiveColor  <- NamedReroute DS_Color_<n>  <- VectorParameter "Tint"  RGB output
  MP_Opacity        <- NamedReroute DS_Alpha_<n>  <- ScalarParameter "A"     R output
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
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
