# Inputs, Outputs and Results

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **Inputs / Outputs / Results**

The sections that declare a block's typed parameters — the input and output pins of a generated
`UMaterialFunction`, or the declared output variables of a `Shader`.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — inside `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction`; `Outputs` also inside `Shader` |
| Kind | section |
| Generates | `UMaterialExpressionFunctionInput` / `UMaterialExpressionFunctionOutput` (material-function blocks); nothing directly for a `Shader` |

## Synopsis

```c
Inputs  [=] { <parameter-declaration>… }
Outputs [=] { <parameter-declaration>… }
Results [=] { <parameter-declaration>… }
```

```c
parameter-declaration := [ opt ] <type> <name> [ = <default-expression> ] [ [ <metadata> ] ] ;
```

The innermost `[ … ]` pair around `<metadata>` is **literal DreamShaderLang punctuation**; the outer
pair is the meta-syntax for "optional".

Inside a `Shader`, `Outputs` uses a different grammar that also carries binding statements:

```c
Outputs [=]
{
    <output-declaration>…
    <output-binding>…
}
```

```c
output-declaration := <type> <name> [ = <expression> ] ;
```

Output bindings are specified on [Output bindings](output-bindings.md).

## Section keywords per block

| Block | `Inputs` | `Outputs` | `Results` | `Properties` |
| :-- | :-- | :-- | :-- | :-- |
| `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` | typed parameters | typed parameters | **alias for `Outputs`** | parameter nodes — see [Properties](properties.md) |
| `VirtualFunction` | typed parameters | typed parameters | **alias for `Outputs`** | **alias for `Inputs`** |
| `Shader` | unknown section | declarations + bindings (own grammar) | unknown section | parameter nodes |

`Results` is a pure synonym: it appends into the same array as `Outputs`. Section names are matched
case-insensitively, may appear in any order, and a repeated section **appends** to the previous one.
The `=` before the `{ … }` block is optional *(since 1.5.0)*.

> [!WARNING]
> Inside a `VirtualFunction`, `Properties` is an **alias for `Inputs`** and therefore gets the
> grammar on this page, not the [`Properties`](properties.md) declaration grammar. Reading
> `Properties` as "parameter nodes" there is the single most common source of confusion in the
> section grammar.

## Declaration members

| Member | Required | Form | Description |
| :-- | :-- | :-- | :-- |
| `opt` | no | keyword | Marks the input optional in Unreal (`bUsePreviewValueAsDefault`). *(since 1.2.3)* |
| **`<type>`** | yes | token | See [Accepted types](#accepted-types). |
| **`<name>`** | yes | identifier | Must match `[A-Za-z_][A-Za-z0-9_]*`. Becomes the pin name. |
| `= <default-expression>` | no | literal or graph expression | Preview value / preview graph. Meaningless on outputs — see [Notes](#notes). |
| `[ <metadata> ]` | no | block | Description and sort priority. See [Per-parameter metadata](#per-parameter-metadata). |

### Parse order

| Step | Operation |
| :-- | :-- |
| 1 | A trailing `[ … ]` metadata block is peeled off the end |
| 2 | A leading `opt` is consumed |
| 3 | The remainder is split at the first `=` outside `()`, `[]` and `"…"` |
| 4 | The left side is split at the **last literal space**, giving type and name |

### `opt`

`opt` is matched case-insensitively and is recognized in exactly two shapes: the statement begins with
`opt` followed by a **literal space**, or the whole statement is exactly `opt`.

> [!WARNING]
> A tab between `opt` and the type token does **not** match. `opt<TAB>float Strength;` parses as a
> declaration whose type token is `opt\tfloat`, which then fails at generation with
> `{Kind} '{Function}' input '{Name}' uses unsupported type '{Type}'.`

`opt` is what makes a pin optional. A default value on a **non-`opt`** input still builds a preview
graph, but the pin stays required.

### The whitespace rule differs from `Properties`

| | `Properties` | `Inputs` / `Outputs` / `Results` |
| :-- | :-- | :-- |
| Type/name split point | last **top-level whitespace** (parenthesis-aware) | last **literal space** |
| Tab as the separator | accepted | **not** accepted |
| Type token may contain `( … )` | yes — `UE.TexCoord(Index = 0) UV` works | no |
| Name validated as an identifier | no | **yes** |

A tab used as the type/name separator in `Inputs` produces
`Invalid typed declaration '{Statement}'.` only when the result leaves an empty side; otherwise the
tab ends up inside the type token and the failure surfaces later as an unsupported-type error.

> [!NOTE]
> `in` and `out` are **not** qualifiers in these sections. They exist only on the HLSL
> [`Function`](function.md) / [`GraphFunction`](graph-function.md) signature form, which is a
> different grammar. Writing `Inputs = { in float X; }` splits into type `in float` and name `X`, and
> generation then reports `uses unsupported type 'in float'`.

## Accepted types

Resolved at generation. All tokens are matched case-insensitively; the scalar/vector resolver and the
`MaterialAttributes` / `Substrate` predicates additionally strip **all spaces** from the token, so
`Material Attributes` is accepted.

| Tokens | `EFunctionInputType` | Components |
| :-- | :-- | :-- |
| `float` `float1` `half` `half1` `int` `uint` `bool` | `FunctionInput_Scalar` | 1 |
| `float2` `half2` `vec2` `int2` `uint2` `bool2` `ivec2` `uvec2` `bvec2` | `FunctionInput_Vector2` | 2 |
| `float3` `half3` `vec3` `int3` `uint3` `bool3` `ivec3` `uvec3` `bvec3` | `FunctionInput_Vector3` | 3 |
| `float4` `half4` `vec4` `int4` `uint4` `bool4` `ivec4` `uvec4` `bvec4` | `FunctionInput_Vector4` | 4 |
| `StaticBool` `StaticBoolParameter` | `FunctionInput_StaticBool` | 1 *(unreleased — previously not a one-component type at call sites)* |
| `MaterialAttributes` | `FunctionInput_MaterialAttributes` | — |
| `Substrate` | `FunctionInput_Substrate` | — *(requires UE 5.4)* |
| `Texture2D` `SamplerState` | `FunctionInput_Texture2D` | — |
| `TextureCube` | `FunctionInput_TextureCube` | — |
| `Texture2DArray` | `FunctionInput_Texture2DArray` | — |
| `Texture3D` `VolumeTexture` | `FunctionInput_VolumeTexture` | — |

Anything else fails with `{Kind} '{Function}' input '{Name}' uses unsupported type '{Type}'.` (or the
matching `output` message). `{Kind}` is always one of `ShaderFunction`, `ShaderLayer`,
`ShaderLayerBlend` — the deprecated `MaterialLayer*` spellings never appear in a diagnostic.

> [!NOTE]
> Because the scalar/vector resolver strips spaces after the type/name split, `float 4 Colour;`
> resolves to `float4`. This is a consequence of the two-stage split, not a documented spelling.

`Substrate` is gated on the engine build: on UE 5.3 it fails with
`{Kind} '{Function}' input '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.`

## Input defaults

A default on an input drives Unreal's *Preview* pin, and — when `opt` is present — the pin's default
value.

| Case | Behaviour |
| :-- | :-- |
| Type is a plain scalar/vector **and** the default parses as a numeric literal | written straight into `PreviewValue` |
| Anything else | evaluated as a **graph expression** and connected to the input's `Preview` pin |

The graph-expression path is what lets a preview default reference a node the block itself generates,
including a `const` or parameter node declared in [`Properties`](properties.md) *(since 1.2.6)*:

```c
ShaderFunction(Name="Functions/F_Sample")
{
    Properties = {
        const Texture2D PreviewTex = Path(Engine, "EngineResources/DefaultTexture");
    }
    Inputs = {
        opt Texture2D Tex = PreviewTex;      // preview graph, not a literal
        opt float     Mix = 0.5;             // literal → PreviewValue
    }
    Outputs = { vec4 OutColor; }
    Graph   = { OutColor = Tex(Coordinates = UE.TexCoord(Index = 0)) * Mix; }
}
```

The literal grammar accepted by the fast path is `name(a[, b[, c[, d]]])` with at most four parts; a
single part splats to all four components. A default whose evaluated type does not match the declared
type fails with:

```
Input '{Name}' default expression '{Expression}' does not match declared type '{Type}'.
```

> [!NOTE]
> *(unreleased)* `true` and `false` are graph literals that materialize as `StaticBool` nodes, so
> `opt StaticBool Flag = false;` generates the Preview-pin node Unreal requires — the engine ignores
> `PreviewValue` for static-bool inputs.

## Output defaults

> [!WARNING]
> `= <expression>` on an entry of a material function's `Outputs` / `Results` is **parsed and then
> ignored**. The only place an output's "has a default" flag is read is a Substrate guard. Write the
> value in the `Graph` instead. This does not apply to a `Shader`'s `Outputs`, where an initializer is
> meaningful *(since 1.3.4)*.

Every declared output must be assigned somewhere in `Graph`, and the assigned value's type must match:

| Message | Cause |
| :-- | :-- |
| `{Kind} '{Function}' output '{Name}' was never assigned an expression.` | nothing in `Graph` writes it |
| `{Kind} '{Function}' output '{Name}' does not match its declared type '{Type}'.` | type mismatch |

A block must declare at least one output: `{Kind} '{Function}' must declare at least one output.`

## Per-parameter metadata

The trailing `[ … ]` block on an input or output is parsed by the same code as
[`Properties` metadata](../parameters/metadata.md), but only two keys are consumed:

| Metadata key | Aliases | Effect |
| :-- | :-- | :-- |
| `Description` | `Desc`, `Tooltip` | `UMaterialExpressionFunctionInput` / `…FunctionOutput` `Description` |
| `SortPriority` | `Sort` | `…::SortPriority`. **When absent, the value is the declaration index**, not `32`. |
| `Group` | `Category` | parsed and retained, but **never applied** — the engine's function input/output nodes have no group field |
| *any other key* | — | **silently ignored** — the reflected-property pass is not run for function inputs and outputs |

> [!WARNING]
> Metadata keys other than the three above are accepted by the parser and dropped without a
> diagnostic. `[SamplerType="LinearColor"]` on an `Inputs` entry does nothing; that key only has an
> effect on a [`Properties`](properties.md) declaration.

Function inputs are created at graph X `-800`, Y starting `-260`, stepping `+180`. Function outputs
are created at X `900`, Y starting `-120`, stepping `+180`. Their pin GUIDs are cached by name across
regeneration, which is why regenerating a `ShaderFunction` does not break existing call sites.

## `MaterialAttributes` outputs

Each `MaterialAttributes`-typed output is pre-seeded with a
`UMaterialExpressionMakeMaterialAttributes` node so member writes such as `Attrs.BaseColor = …` have
a target. Failure to create it reports
`Failed to create a MakeMaterialAttributes node for '{Name}'.` The same seeding happens for a
`Shader`'s `MaterialAttributes` output declarations. See
[MaterialAttributes](../graph/material-attributes.md).

## Layer and blend arity

`ShaderLayer` and `ShaderLayerBlend` constrain these sections further:

| Kind | Inputs | Outputs |
| :-- | :-- | :-- |
| `ShaderFunction` | unconstrained | ≥ 1 |
| `ShaderLayer` | at most 1, and it must be `MaterialAttributes` | exactly one, `MaterialAttributes` |
| `ShaderLayerBlend` | exactly 2, both `MaterialAttributes` | exactly one, `MaterialAttributes` |

Layer controls belong in [`Properties`](properties.md), not in `Inputs`. Full rules — including
`EBlendInputRelevance` name matching *(since UE 5.7)* — are on
[ShaderLayer / ShaderLayerBlend](shader-layer.md).

## `Outputs` in a `Shader`

A `Shader`'s `Outputs` section fills **two** lists from one body: output-variable declarations and
output bindings. Each `;`-separated statement is classified independently:

| Statement shape | Classified as |
| :-- | :-- |
| `<type> <name> ;` | output-variable declaration |
| `<type> <name> = <expr> ;` — left side is a valid typed declaration | **initialized** output declaration *(since 1.3.4)* |
| `<target> = <source> ;` — left side is **not** a valid typed declaration | output binding |
| no top-level `=` at all | bare output-variable declaration |

The declaration grammar is the typed-declaration form above, without `opt` and without metadata.

> [!WARNING]
> No `[ … ]` metadata block is accepted on a `Shader` `Outputs` statement. The metadata parser is
> never invoked there, so a bracketed block is left inside the statement text and produces an
> `Invalid typed declaration` or `Invalid output binding` error.

Additional `Shader`-only rules:

- The name `return` is reserved. It may not be used as a declaration, and as a binding source it may
  only bind to `Base.*` targets — and never in a `Shader` that has a `Graph` block.
- An **initialized** output declaration satisfies the "shader has a body" rule, so a `Shader` whose
  outputs are all initialized may use an empty `Graph = { }`.
- A `Shader` with no bindings parses with a warning
  (`No Outputs block was provided. Generation requires explicit material property bindings.`) and then
  fails generation with `{File}: Outputs block is required.`

The complete binding catalogue, the validation rules and every binding diagnostic are on
[Output bindings](output-bindings.md).

## Notes

- Section bodies are comment-stripped before statements are split.
- An empty right-hand side after `=` is **not** diagnosed at parse time in the typed-parameter
  grammar; it becomes an empty default-expression text and fails later.
- `Options` and `Settings` are the `VirtualFunction` counterparts to these sections; see
  [Options](options.md).
- A `VirtualFunction` declares an existing asset and never generates pins itself — its `Inputs` /
  `Outputs` describe the asset's interface so `Graph` calls can be type-checked. See
  [VirtualFunction](virtual-function.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section. `{Kind}` is
`ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend`.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Invalid typed declaration '{Statement}'.` | no literal space between type and name, an empty type or name, or a name that is not an identifier |
| `Invalid output declaration initializer '{Statement}'.` | `Shader` `Outputs`: initialized declaration with an empty right side |
| `Invalid output binding '{Statement}'.` | `Shader` `Outputs`: binding with an empty right side |
| `Metadata must follow a declaration.` | the statement is nothing but a `[ … ]` block |
| `Unknown material function section '{Section}'.` | a section name other than `Properties`, `Inputs`, `Outputs`, `Results`, `Settings`, `Graph`, `Layout`, `Code` |
| `Unknown VirtualFunction section '{Section}'.` | a section name other than `Properties`, `Inputs`, `Outputs`, `Results`, `Options`, `Settings` — `Graph` and `Code` are recognized and rejected with their own message |
| `VirtualFunction '{Name}' must declare at least one output.` | no `Outputs` / `Results` entries |

### Generation time

| Message | Cause |
| :-- | :-- |
| `{Kind} '{Function}' must declare at least one output.` | empty `Outputs` |
| `{Kind} '{Function}' input '{Name}' uses unsupported type '{Type}'.` | type token not in the accepted table |
| `{Kind} '{Function}' output '{Name}' uses unsupported type '{Type}'.` | same, for an output |
| `{Kind} '{Function}' input '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` input on UE 5.3 |
| `{Kind} '{Function}' output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` output on UE 5.3 |
| `{Kind} '{Function}' output '{Name}' was never assigned an expression.` | the `Graph` never writes the output |
| `{Kind} '{Function}' output '{Name}' does not match its declared type '{Type}'.` | assigned value has the wrong type |
| `{Kind} '{Function}' property '{Name}' conflicts with another property or input name.` | a `Properties` name collides with an input name |
| `{Kind} '{Function}' failed to create input '{Name}'.` | node construction failed |
| `{Kind} '{Function}' failed to create output '{Name}'.` | node construction failed |
| `{Kind} '{Function}' failed to resolve generated input '{Name}'.` | the generated input node could not be looked up |
| `{Kind} '{Function}' input '{Name}': {Detail}` | preview-default or metadata failure on that input |
| `{Kind} '{Function}' output '{Name}': {Detail}` | metadata failure on that output |
| `Input '{Name}' default expression '{Expression}' does not match declared type '{Type}'.` | preview default type mismatch |
| `Failed to create a MakeMaterialAttributes node for '{Name}'.` | `MaterialAttributes` output seeding failed |
| `Output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Shader` output declared `Substrate` on UE 5.3 |
| `Unsupported output type '{Type}' for '{Name}'.` | `Shader` output declaration type not resolvable |
| `Output variable '{Name}' is declared with conflicting types.` | the same `Shader` output name declared twice with different types |
| `Outputs declarations cannot use the reserved name 'return'.` | a `Shader` output declaration named `return` |

The complete cross-stage list is in the [diagnostics index](../diagnostics/index.md).

## Example

```c
ShaderFunction(Name="Functions/F_Opt")
{
    Inputs = {
        vec3 InColor;
        opt float Strength = 1.0 [
            Description="Preview strength";
            SortPriority=10;
        ];
    }

    Results = {                       // alias for Outputs
        vec3 OutColor [Description="Tinted result"];
    }

    Settings = {
        Description     = "Strength helper";
        ExposeToLibrary = true;
    }

    Graph = {
        OutColor = InColor * Strength;
    }
}
```

Generated asset interface:

```text
UMaterialFunction  /Game/Functions/F_Opt.F_Opt
  FunctionInput   InColor    Vector3   required
  FunctionInput   Strength   Scalar    optional, PreviewValue = (1,1,1,1), SortPriority 10
  FunctionOutput  OutColor   Vector3   SortPriority 0
```

## See also

- [Properties](properties.md) — the parameter/const declaration section
- [ShaderFunction](shader-function.md) — the block these sections describe
- [ShaderLayer / ShaderLayerBlend](shader-layer.md) — arity constraints on these sections
- [VirtualFunction](virtual-function.md) — where `Properties` aliases `Inputs`
- [Options](options.md) — the `VirtualFunction` `Asset` section
- [Output bindings](output-bindings.md) — the `Base.*` and `Expression(…).Pin[i]` binding catalogue
- [Shader](shader.md) — the block whose `Outputs` carry bindings
- [Types](types.md) — the full type-token catalogue and per-context validity matrix
- [Metadata](../parameters/metadata.md) — the `[ … ]` block grammar
- [Calls](../graph/calls.md) — calling a `ShaderFunction` or `VirtualFunction` from `Graph`
- [MaterialAttributes](../graph/material-attributes.md) — member writes and Substrate interop
- [Function settings](../settings/function.md) — the four keys a function's `Settings` honours
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
