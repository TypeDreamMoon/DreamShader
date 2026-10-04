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
pair is the meta-syntax for "optional". The `;` may be left off the last declaration before `}`.

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
| `Shader` | unknown section ([`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245)) | declarations + bindings (own grammar) | unknown section (`DSH2245`) | parameter nodes |

`Results` is a pure synonym: it appends into the same list as `Outputs`. Section names are matched
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
| **`<type>`** | yes | one word | See [Accepted types](#accepted-types). |
| **`<name>`** | yes | identifier | Becomes the pin name. |
| `= <default-expression>` | no | expression | The input's default, which also makes it optional *(since 2.0.0)*. Meaningless on outputs — see [Output defaults](#output-defaults). |
| `[ <metadata> ]` | no | block | Description and sort order. See [Per-parameter metadata](#per-parameter-metadata). |

### Parse order

*(since 2.0.0)* The section is read as words, one declaration at a time:

| Step | Operation |
| :-- | :-- |
| 1 | A leading `opt` (any case) is taken when two more words follow it |
| 2 | The next two words are the type and the name |
| 3 | An `=` starts the default, which runs to the `;` or to a `[` |
| 4 | A `[ … ]` block is the metadata |
| 5 | A `;` (or the section's `}`) ends the declaration |

Anything else is [`DSH3271`](../diagnostics/DSH3xxx.md#dsh3271), and the parser resumes at the next
`;`. A default with nothing after the `=` is `DSH3271` too.

### `opt`

`opt` is matched case-insensitively. It is what marks a pin optional, and *(since 2.0.0)* a default
does too (legacy rule L7): `<type> <name> = <expr>;` is an optional input, where 1.x left it required
with only a preview value. `opt` on an output means nothing and is a warning
([`DSH3272`](../diagnostics/DSH3xxx.md#dsh3272)).

### Whitespace

*(since 2.0.0)* Whitespace only separates words, in these sections as in [`Properties`](properties.md):
a tab between `opt`, the type and the name is as good as a space. What 1.x split at the last literal
space — so that a tab made the type token `opt\tfloat` — no longer exists. The type and the name are
one word each: `Material Attributes X;` or `float 4 Colour;` is `DSH3271`.

> [!NOTE]
> `in` and `out` are **not** qualifiers in these sections. They exist only on the HLSL
> [`Function`](function.md) / [`GraphFunction`](graph-function.md) signature form, which is a
> different grammar. Writing `Inputs = { in float X; }` is `DSH3271`: `in` is a keyword, not a type.

## Accepted types

Resolved when the file is bound. Tokens are matched case-insensitively.

| Tokens | `EFunctionInputType` | Components |
| :-- | :-- | :-- |
| `float` `float1` `half` `half1` `int` `uint` `bool` | `FunctionInput_Scalar` | 1 |
| `float2` `half2` `vec2` `int2` `uint2` `bool2` `ivec2` `uvec2` `bvec2` | `FunctionInput_Vector2` | 2 |
| `float3` `half3` `vec3` `int3` `uint3` `bool3` `ivec3` `uvec3` `bvec3` | `FunctionInput_Vector3` | 3 |
| `float4` `half4` `vec4` `int4` `uint4` `bool4` `ivec4` `uvec4` `bvec4` | `FunctionInput_Vector4` | 4 |
| `StaticBool` `StaticBoolParameter` | `FunctionInput_StaticBool` on an input; read as `bool` on an output | 1 *(since 1.6.0 — previously not a one-component type at call sites)* |
| `MaterialAttributes` | `FunctionInput_MaterialAttributes` | — |
| `Substrate` | `FunctionInput_Substrate` | — |
| `Texture2D` | `FunctionInput_Texture2D` | — |
| `TextureCube` | `FunctionInput_TextureCube` | — |
| `Texture2DArray` | `FunctionInput_Texture2DArray` | — |
| `Texture3D` `VolumeTexture` | `FunctionInput_VolumeTexture` | — |

A word that names no type is [`DSH4201`](../diagnostics/DSH4xxx.md#dsh4201). `SamplerState` is a
type, but no function pin carries a sampler: [`DSH4364`](../diagnostics/DSH4xxx.md#dsh4364)
*(since 2.0.0; 1.x made it a `Texture2D` input)*. The pin type is looked up in the running engine's
own list; one the engine does not have is [`DSH8228`](../diagnostics/DSH8xxx.md#dsh8228).

Two inputs or outputs of one name are [`DSH4214`](../diagnostics/DSH4xxx.md#dsh4214).

## Input defaults

A default on an input is the value the pin takes when the caller leaves it unconnected; it makes the
input optional *(since 2.0.0)*.

| Case | Behaviour |
| :-- | :-- |
| The default folds to a constant | written into `PreviewValue` |
| Anything else | built as a **graph expression** and connected to the input's `Preview` pin |

The graph-expression path is what lets a default reference a node the block itself generates,
including a `const` or parameter node declared in [`Properties`](properties.md) *(since 1.2.6)*:

```c
ShaderFunction(Name="Functions/F_Sample")
{
    Properties = {
        const Texture2D PreviewTex = Path(Engine, "EngineResources/DefaultTexture");
    }
    Inputs = {
        opt Texture2D Tex = PreviewTex;      // preview graph, not a literal
        opt float     Mix = 0.5;             // constant → PreviewValue
    }
    Outputs = { vec4 OutColor; }
    Graph   = { OutColor = Tex(Coordinates = UE.TexCoord(Index = 0)) * Mix; }
}
```

A default is converted to the input's type as an assignment would be: a value that does not fit is
[`DSH4226`](../diagnostics/DSH4xxx.md#dsh4226), and a wider vector is cut down to its leading
components with [`DSH5289`](../diagnostics/DSH5xxx.md#dsh5289).

> [!NOTE]
> *(since 1.6.0)* `true` and `false` are graph literals. A `StaticBool` input's default becomes a
> `StaticBool` node on the `Preview` pin — the engine ignores `PreviewValue` for static-bool inputs —
> so `opt StaticBool Flag = false;` generates the node Unreal requires.

## Output defaults

> [!WARNING]
> `= <expression>` on an entry of a material function's `Outputs` / `Results` is **dropped**, with a
> warning ([`DSH3273`](../diagnostics/DSH3xxx.md#dsh3273)). Write the value in the `Graph` instead.
> This does not apply to a `Shader`'s `Outputs`, where an initializer is meaningful *(since 1.3.4)*.

Every declared output other than a first one named `Result` must be assigned somewhere in `Graph`
([`DSH6211`](../diagnostics/DSH6xxx.md#dsh6211)); a first `Result` is the function's return value and
starts at zero. An assigned value that does not fit is `DSH4226` (a wider vector: `DSH5289`).

A material function must declare at least one output
([`DSH4315`](../diagnostics/DSH4xxx.md#dsh4315)); a `VirtualFunction` must too
([`DSH6313`](../diagnostics/DSH6xxx.md#dsh6313)).

## Per-parameter metadata

The trailing `[ … ]` block on an input or output is read by the same code as
[`Properties` metadata](../parameters/metadata.md), but only these keys are consumed:

| Metadata key | Aliases | Effect |
| :-- | :-- | :-- |
| `Description` | `Desc`, `Tooltip` | `UMaterialExpressionFunctionInput` / `…FunctionOutput` `Description` |
| `SortPriority` | `Sort` | orders the section's pins: they are stably sorted by it, a pin without one counting as its declaration index, and the asset gets dense priorities `0, 1, 2, …` in that order *(since 2.0.0)*. Not a whole number: [`DSH3258`](../diagnostics/DSH3xxx.md#dsh3258). |
| `Group` | `Category` | ignored — the engine's function input/output nodes have no group field |
| *any other key* | — | **silently ignored** — the reflected-property pass is not run for function inputs and outputs |

> [!WARNING]
> Metadata keys other than the ones above are accepted by the parser and dropped without a
> diagnostic. `[SamplerType="LinearColor"]` on an `Inputs` entry does nothing; that key only has an
> effect on a [`Properties`](properties.md) declaration.

A malformed block — no `]`, a key without `=` or value — is
[`DSH3255`](../diagnostics/DSH3xxx.md#dsh3255); a key written twice is
[`DSH3256`](../diagnostics/DSH3xxx.md#dsh3256).

Pin GUIDs are cached by name across regeneration, which is why regenerating a `ShaderFunction` does
not break existing call sites.

## `MaterialAttributes` outputs

A `MaterialAttributes` output starts as an empty material, so member writes such as
`Attrs.BaseColor = …` have a target. A layer's output starts as the material that came in instead;
see [ShaderLayer / ShaderLayerBlend](shader-layer.md#interface-rules). The same holds for a `Shader`'s
`MaterialAttributes` output declarations. See [MaterialAttributes](../graph/material-attributes.md).

## Layer and blend rules

`ShaderLayer` and `ShaderLayerBlend` constrain these sections further:

| Kind | Inputs | Outputs |
| :-- | :-- | :-- |
| `ShaderFunction` | unconstrained | ≥ 1 |
| `ShaderLayer` | none, or one `MaterialAttributes` — the layer's material input either way | exactly one, `MaterialAttributes` |
| `ShaderLayerBlend` | at least one `MaterialAttributes` | exactly one, `MaterialAttributes` |

Layer controls belong in [`Properties`](properties.md), not in `Inputs`. Full rules and their codes —
including `BlendInputRelevance` name matching *(since UE 5.7)* — are on
[ShaderLayer / ShaderLayerBlend](shader-layer.md).

## `Outputs` in a `Shader`

A `Shader`'s `Outputs` section fills **two** lists from one body: output-variable declarations and
output bindings. Each statement is classified by its first words:

| Statement shape | Classified as |
| :-- | :-- |
| `<type> <name> ;` | output-variable declaration; it starts at zero, as in 1.x |
| `<type> <name> = <expr> ;` | **initialized** output declaration *(since 1.3.4)* |
| `Base.<target> = <source> ;`, `Expression( … ).Pin[<i>] = <source> ;`, `Expression( … ) { … }` | output binding |
| anything else | [`DSH3266`](../diagnostics/DSH3xxx.md#dsh3266) |

The declaration grammar is the typed-declaration form above, without `opt` and without metadata. A
`[ … ]` block on a `Shader` `Outputs` statement is `DSH3266`. An `=` with nothing after it is
[`DSH3270`](../diagnostics/DSH3xxx.md#dsh3270).

`Shader`-only rules:

- `return` is a keyword *(since 2.0.0)*: it cannot name an output declaration (`DSH3266`) and is not a
  binding source. See [Output bindings](output-bindings.md#the-reserved-name-return).
- An output name declared twice is [`DSH4220`](../diagnostics/DSH4xxx.md#dsh4220); a type that is
  not a type is `DSH4201`.
- When a `Shader` needs a `Graph`, and what a `Shader` with no `Outputs` section does, is on
  [Shader](shader.md#the-outputs--graph-relationship).

The complete binding catalogue, the validation rules and every binding diagnostic are on
[Output bindings](output-bindings.md).

## Notes

- Comments are skipped wherever whitespace may stand.
- `Options` and `Settings` are the `VirtualFunction` counterparts to these sections; see
  [Options](options.md).
- A `VirtualFunction` declares an existing asset and never generates pins itself — its `Inputs` /
  `Outputs` describe the asset's interface so `Graph` calls can be type-checked. See
  [VirtualFunction](virtual-function.md).

## Diagnostics

Each code carries the line and column of the declaration; the code's page has the message.

### Parse time

| Code | Raised when |
| :-- | :-- |
| `DSH3271` | the section is not opened with `{`; a declaration is not `[opt] <type> <name> [= <default>] [[ … ]] ;`; a default is empty |
| `DSH3272` | `opt` on an output (warning; ignored) |
| `DSH3273` | a default on an output (warning; dropped) |
| `DSH3255`, `DSH3256`, [`DSH3257`](../diagnostics/DSH3xxx.md#dsh3257), `DSH3258` | a malformed metadata block, a key written twice, a malformed `Slider(…)`, a `SortPriority` that is not a whole number |
| `DSH2245` | a section name the block does not have |
| [`DSH2247`](../diagnostics/DSH2xxx.md#dsh2247) | `Graph` or `Code` in a `VirtualFunction` |
| `DSH6313` | a `VirtualFunction` with no `Outputs` / `Results` entry |
| `DSH3266`, `DSH3270` | `Shader` `Outputs`: a statement of no known shape / an `=` with nothing after it |
| [`DSH2150`](../diagnostics/DSH2xxx.md#dsh2150) | the file ends inside the section |

### Binding and IR

| Code | Raised when |
| :-- | :-- |
| `DSH4201` | a type token names no type |
| `DSH4214` | two parameters share a name |
| `DSH4220` | `Shader` `Outputs`: a name declared twice |
| `DSH4226`, `DSH5289` | a default or an assigned value does not fit / a wider vector is cut down (info) |
| `DSH6211` | the `Graph` never assigns an output |
| `DSH4315` | a material function without outputs |
| `DSH4364` | a type no function pin carries |

### Emit time

| Code | Raised when |
| :-- | :-- |
| `DSH8228` | the engine has no function input type of that name |
| [`DSH8214`](../diagnostics/DSH8xxx.md#dsh8214) | a `FunctionInput` / `FunctionOutput` node could not be created |

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
    }

    Graph = {
        OutColor = InColor * Strength;
    }
}
```

Generated asset interface:

```text
UMaterialFunction  /Game/Functions/F_Opt.F_Opt
  FunctionInput   InColor    Vector3   required, SortPriority 0
  FunctionInput   Strength   Scalar    optional, PreviewValue 1.0, SortPriority 1
  FunctionOutput  OutColor   Vector3   SortPriority 0
```

## See also

- [Properties](properties.md) — the parameter/const declaration section
- [ShaderFunction](shader-function.md) — the block these sections describe
- [ShaderLayer / ShaderLayerBlend](shader-layer.md) — the constraints on these sections
- [VirtualFunction](virtual-function.md) — where `Properties` aliases `Inputs`
- [Options](options.md) — the `VirtualFunction` `Asset` section
- [Output bindings](output-bindings.md) — the `Base.*` and `Expression(…).Pin[i]` binding catalogue
- [Shader](shader.md) — the block whose `Outputs` carry bindings
- [Types](types.md) — the full type-token catalogue and per-context validity matrix
- [Metadata](../parameters/metadata.md) — the `[ … ]` block grammar
- [Calls](../graph/calls.md) — calling a `ShaderFunction` or `VirtualFunction` from `Graph`
- [MaterialAttributes](../graph/material-attributes.md) — member writes and Substrate interop
- [Function settings](../settings/function.md) — the keys a function's `Settings` honours
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
