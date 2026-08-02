# ShaderFunction

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **ShaderFunction**

A top-level block that declares one reusable Unreal material function: its typed input and output
pins, its function-local parameter nodes, and the node graph that connects them.

| | |
| :-- | :-- |
| Declared in | `.dsm` and `.dsf` — a `.dsh` containing the text `ShaderFunction(` is rejected before parsing |
| Kind | top-level block |
| Generates | `UMaterialFunction` with `EMaterialFunctionUsage::Default` |
| Multiplicity | any number per parse unit |

## Synopsis

```c
ShaderFunction(Name = "<asset-path>" [, Root = "<root>"] [,])
{
    [Properties            [=] { <property-declaration> ; … }]
    [Inputs                [=] { <parameter-declaration> ; … }]
    { Outputs | Results }  [=] { <parameter-declaration> ; … }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

`Graph` and at least one output are required; every other section is optional. Sections may appear in
any order and may be repeated. The `=` between a section name and its `{ … }` block is optional sugar
*(since 1.5.0)*; a `;` after a section's closing `}` is optional.

The keyword `ShaderFunction` is matched **case-sensitively**; section names are matched
case-insensitively. See [Lexical elements](lexical.md#case-sensitivity).

## Header attributes

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | string | The asset's logical path. The last `/`-separated segment is the asset name; preceding segments become folders. |
| `Root` | no | string | The package root the folders hang off. Defaults to `/Game` when absent or empty. |

Attribute keys are matched case-insensitively (`name=` works). Values may be quoted or bare; a bare
value ends at the first `,` or `)`. A trailing comma before `)` is accepted. A duplicate key silently
overwrites the earlier one — there is no diagnostic.

Full `Name` / `Root` grammar, the accepted root spellings, and the resulting on-disk path are
specified in [Asset paths](../generation/asset-paths.md).

## Sections

| Section | Accepted | Repeat behaviour | Reference |
| :-- | :-- | :-- | :-- |
| `Properties` | yes — function-local parameter, `const` and `UE.*` nodes | appends | [Properties](properties.md) |
| `Inputs` | yes — `UMaterialExpressionFunctionInput` pins | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Outputs` | yes — `UMaterialExpressionFunctionOutput` pins | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Results` | yes — alias for `Outputs`, no warning | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Settings` | yes — four keys are honoured, everything else is ignored | merges; last key wins | [Function settings](../settings/function.md) |
| `Graph` | yes — **required** | overwrites the previous body | [Graph](../graph/index.md) |
| `Layout` | yes | **resets** — a second `Layout` discards the first | [Layout](layout.md) |
| `Code` | **no** — hard error | — | — |
| `Options` | no — unknown section | — | — |

The same body parser serves `ShaderFunction`, [`ShaderLayer` and
`ShaderLayerBlend`](shader-layer.md); the section table above is identical for all three.

## `Properties` versus `Inputs`

Both sections put something into the generated function, but they are different grammars producing
different nodes.

| | `Properties` | `Inputs` |
| :-- | :-- | :-- |
| Grammar | `[const] <type> <name> [= <default>] [[ … ]] ;` | `[opt] <type> <name> [= <default>] [[ … ]] ;` |
| Generates | a parameter node, a constant node, or a `UE.*` builtin node *inside* the function graph | a `UMaterialExpressionFunctionInput` pin on the function's interface |
| Visible to callers | as a material parameter on any material that uses the function | as a wired input pin |
| Name validation | non-empty only — not checked against the identifier rules | must match `[A-Za-z_][A-Za-z0-9_]*` |
| Type/name separator | any whitespace, tabs included | a literal space character only |

Property nodes are created **lazily, on first reference from `Graph`** *(since 1.3.2)*. A property
that the `Graph` never mentions produces no node in the generated asset.

Property names must be unique within the block and must not collide with an input name; both checks
are case-insensitive and share one diagnostic:
`{Kind} '{Function}' property '{Name}' conflicts with another property or input name.`

> [!NOTE]
> Name lookup inside a `ShaderFunction`'s `Graph` searches the function's own `Properties` first and
> then falls back to the `Properties` of the parse unit's top-level [`Shader`](shader.md) block, if
> the file has one. A name resolved through that fallback still creates its node inside the
> **function** asset, and the function then only compiles in files that declare that property. See
> [Name resolution](../graph/name-resolution.md).

`Properties` in a `ShaderFunction` is *not* the same thing as `Properties` inside a
[`VirtualFunction`](virtual-function.md), where the keyword is a synonym for `Inputs`.

## Parameter types

`Inputs`, `Outputs` and `Results` accept exactly these type tokens, matched case-insensitively.

| Token(s) | Function pin type | Components |
| :-- | :-- | :-- |
| `StaticBool`, `StaticBoolParameter` | `FunctionInput_StaticBool` | 1 |
| `MaterialAttributes` | `FunctionInput_MaterialAttributes` | — |
| `Substrate` *(since UE 5.4)* | `FunctionInput_Substrate` | — |
| `float`, `float1`, `half`, `half1`, `int`, `uint`, `bool` | `FunctionInput_Scalar` | 1 |
| `float2`, `half2`, `vec2`, `int2`, `uint2`, `bool2`, `ivec2`, `uvec2`, `bvec2` | `FunctionInput_Vector2` | 2 |
| `float3`, `half3`, `vec3`, `int3`, `uint3`, `bool3`, `ivec3`, `uvec3`, `bvec3` | `FunctionInput_Vector3` | 3 |
| `float4`, `half4`, `vec4`, `int4`, `uint4`, `bool4`, `ivec4`, `uvec4`, `bvec4` | `FunctionInput_Vector4` | 4 |
| `Texture2D`, `SamplerState` | `FunctionInput_Texture2D` | — |
| `TextureCube` | `FunctionInput_TextureCube` | — |
| `Texture2DArray` | `FunctionInput_Texture2DArray` | — |
| `Texture3D`, `VolumeTexture` | `FunctionInput_VolumeTexture` | — |

`MaterialAttributes` and `Substrate` are compared after removing every space, so `Material
Attributes` is also accepted. Any other token fails with
`{Kind} '{Function}' input '{Name}' uses unsupported type '{Type}'.` (or the `output` variant).

## Input defaults and `opt`

| Form | Effect |
| :-- | :-- |
| `<type> <name>;` | required input; the caller must supply it |
| `<type> <name> = <expr>;` | required input whose `Preview` graph is built from `<expr>` |
| `opt <type> <name>;` | optional input (`bUsePreviewValueAsDefault = true`) with no preview value |
| `opt <type> <name> = <expr>;` *(since 1.2.3)* | optional input; `<expr>` becomes the value used when the pin is unconnected |

`opt` — and only `opt` — is what marks the pin optional in Unreal. A default on a non-`opt` input
still builds the preview graph but leaves the pin required.

A default is written straight into `PreviewValue` when the declared type is a plain scalar or vector
**and** the text parses as a numeric literal (`<anything>(a[,b[,c[,d]]])`, at most four components, a
single component splatting to all four). Otherwise the text is compiled as a `Graph` expression and
connected to the input's `Preview` pin — which is how `opt Texture2D Tex = PreviewTex;` referencing a
generated `Properties` entry works *(since 1.2.6)*. A mismatch reports
`Input '{Name}' default expression '{Expression}' does not match declared type '{Type}'.`

> [!WARNING]
> `opt` is recognized only as the literal three letters followed by a **space character**. Separating
> `opt` from the type with a tab parses without a diagnostic and produces a *required* input whose
> type token is `opt` plus the tab plus the real type; generation then fails with
> `{Kind} '{Function}' input '{Name}' uses unsupported type '{Type}'.` The separator between the type
> and the name has the same restriction — a tab there yields
> `Invalid typed declaration '{Statement}'.` (`Properties` accepts any whitespace, which is why the
> two sections disagree.)

## Per-parameter metadata

The trailing `[ … ]` block is accepted on `Inputs`, `Outputs` and `Results` declarations. Only these
keys have an effect; unlike `Properties`, no other key is reflected onto the generated node.

| Metadata key (case-insensitive) | Effect on the input/output node |
| :-- | :-- |
| `Description`, `Desc`, `Tooltip` | writes `Description` |
| `SortPriority`, `Sort` | writes `SortPriority`; when absent the declaration index is used |
| `Group`, `Category` | parsed and retained, **not applied** — the engine's function input/output nodes have no group field |
| any other key | ignored |

Full metadata grammar, including the `Slider(min, max)` shorthand, is in
[Metadata](../parameters/metadata.md).

## `Outputs`

Every output must be assigned by the `Graph`; the assigned value's type must match the declaration.

| Rule | Diagnostic |
| :-- | :-- |
| at least one output must be declared | `{Kind} '{Function}' must declare at least one output.` |
| the `Graph` must assign each output | `{Kind} '{Function}' output '{Name}' was never assigned an expression.` |
| the assigned value must match the declared type | `{Kind} '{Function}' output '{Name}' does not match its declared type '{Type}'.` |

A `MaterialAttributes` output is pre-seeded with a `MakeMaterialAttributes` node so that member
writes such as `Attrs.BaseColor = …;` have a target. See
[MaterialAttributes](../graph/material-attributes.md).

> [!NOTE]
> An initializer on an output declaration (`vec3 OutColor = InColor;`) parses without a diagnostic
> and is then **discarded** — a function output is only ever driven by the `Graph`. This is unlike a
> [`Shader`](shader.md)'s `Outputs`, where an initializer is meaningful.

## Generated asset

`Name` + `Root` resolve to a package path exactly as described in
[Asset paths](../generation/asset-paths.md). The asset created there is a `UMaterialFunction` and its
`MaterialFunctionUsage` is set to `Default`.

When an asset already exists at that path it is reused only if its class is **exactly**
`UMaterialFunction`; a subclass such as `UMaterialFunctionMaterialLayer` is rejected. If the package
exists on disk and the object carries no DreamShader provenance metadata, generation refuses to touch
it. See [Regeneration](../generation/regeneration.md).

The four honoured `Settings` keys are applied to the asset; each one is **reset when the key is
absent**, so removing a key from the source removes it from the asset:

| Key | `UMaterialFunction` field | When absent |
| :-- | :-- | :-- |
| `Description` | `Description` | cleared |
| `UserExposedCaption` | `UserExposedCaption` | cleared |
| `ExposeToLibrary` | `bExposeToLibrary` | set to `false` |
| `LibraryCategories` | `LibraryCategoriesText` — comma-split, each entry trimmed, empty entries dropped | cleared |

Every other key in a `ShaderFunction`'s `Settings` map is parsed, stored and silently ignored — there
is no generic reflected-property path here, unlike [`Shader` settings](../settings/material.md). See
[Function settings](../settings/function.md).

### Input and output identity across regeneration

Before the old graph is cleared, the `Id` GUID of every `FunctionInput` and `FunctionOutput` is
cached by pin name and restored onto the newly created node *(since 1.3.2)*. Existing
`MaterialFunctionCall` nodes in hand-authored materials therefore keep their wiring across a
regeneration, **as long as the pin name is unchanged**. Renaming an input or output is equivalent to
deleting it and adding a new one: call sites lose that connection.

### Node placement

When [layout](../generation/graph-layout.md) is skipped — transient generation, or graphs at or above
1200 nodes — the construction-time positions are what remains:

| Node | X | First Y | Y step |
| :-- | :-- | :-- | :-- |
| `FunctionInput` | -800 | -260 | +180 |
| `FunctionOutput` | 900 | -120 | +180 |
| seeded `MakeMaterialAttributes` | 120 | 260 | +220 |

> [!WARNING]
> Regeneration clears the function graph. Node positions not pinned by [`Layout`](layout.md), added
> nodes, node property tweaks, and comment boxes whose text begins with `DreamShader: ` are
> destroyed. Only comment boxes that do **not** carry that prefix survive.

## Calling a ShaderFunction

Inside any `Graph` in the same parse unit — including the file's `Shader` block and other functions —
the callee name is matched case-insensitively against the full `Name` and against its last
`/`-separated segment. Only the leaf form is spellable in an expression, because `/` lexes as the
division operator.

```c
// ShaderFunction(Name="Functions/F_Tint") declared elsewhere in the parse unit:
vec3 Tinted = F_Tint(BaseColor, Tint);          // single-output call as a value  (since 1.5.0)
F_Tint(BaseColor, Tint, OutColor, OutLuma);     // statement call: inputs, then one target per output
```

Argument rules, `default` arguments and the named-argument form are specified in
[Calls](../graph/calls.md).

## Notes

- **`ShaderFunction` may share a file with anything except a second `Shader`.** A `.dsm` or `.dsf`
  may declare any number of `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`, `VirtualFunction`,
  `Function`, `GraphFunction` and `Namespace` blocks; one compile generates all of them.
- **The file-kind restriction is a substring scan, not a parse.** A `.dsh` is rejected if its text
  contains `ShaderFunction(` anywhere, including inside a comment or a string literal. Note that
  `ShaderFunction(` does not contain the substring `Shader(`, which is what lets a `.dsf` hold
  function blocks. See [Source files](source-files.md).
- A file that declares only functions still produces a compile message; no `Shader` block is needed.
- Regeneration is skipped silently when the source hash is unchanged **and** the asset's
  `MaterialFunctionUsage` already matches the block kind. Unlike a material, a skipped function emits
  no `Skipped …` message. See [Caching](../generation/caching.md).
- `Layout` is accepted and applies to the function's own graph.
- The `Code` section is rejected outright. Its diagnostic still claims `Function Code = { ... } is
  still supported`; no reachable grammar accepts `Code`.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section. `{Kind}` is always one of
`ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` — the deprecated `MaterialLayer*` spellings never
appear in a diagnostic.

### Parse time

| Message | Cause |
| :-- | :-- |
| `ShaderFunction(Name="...") is required.` | the header has no `Name` attribute |
| `Unknown material function section '{Section}'.` | a section other than `Properties`, `Inputs`, `Outputs`, `Results`, `Settings`, `Graph`, `Layout`, `Code` |
| `ShaderFunction, ShaderLayer, and ShaderLayerBlend graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` | a `Code` section was used |
| `Invalid typed declaration '{Statement}'.` | an `Inputs`/`Outputs`/`Results` statement with no space between type and name, an empty left side, or a name that is not an identifier |
| `Expected '{' near index {Index}.` | the body block is missing |
| `Unterminated block.` | the body `{` is never closed |
| `Expected ',' or ')' near index {Index}.` | malformed attribute list |

### Generation time

| Message | Cause |
| :-- | :-- |
| `{Kind} '{Function}' must declare at least one output.` | no `Outputs` / `Results` entry |
| `{Kind} '{Function}' must provide a Graph block.` | no `Graph` section |
| `{Kind} '{Function}' property '{Name}' conflicts with another property or input name.` | duplicate property name, or a property that shadows an input |
| `{Kind} '{Function}' input '{Name}' uses unsupported type '{Type}'.` | type token not in the parameter-type table |
| `{Kind} '{Function}' input '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` input on UE 5.3 |
| `{Kind} '{Function}' failed to create input '{Name}'.` | the `FunctionInput` node could not be created |
| `{Kind} '{Function}' failed to resolve generated input '{Name}'.` | the created input could not be looked up again while building the graph |
| `{Kind} '{Function}' input '{Name}': {Detail}` | the input's preview default failed to build |
| `Input '{Name}' default expression '{Expression}' does not match declared type '{Type}'.` | preview default of the wrong type |
| `{Kind} '{Function}' output '{Name}' was never assigned an expression.` | the `Graph` never writes the output |
| `{Kind} '{Function}' output '{Name}' does not match its declared type '{Type}'.` | assigned value has the wrong component count or kind |
| `{Kind} '{Function}' output '{Name}' uses unsupported type '{Type}'.` | type token not in the parameter-type table |
| `{Kind} '{Function}' output '{Name}' uses Substrate, which requires Unreal Engine 5.4 or newer.` | `Substrate` output on UE 5.3 |
| `{Kind} '{Function}' output '{Name}': {Detail}` | the output's `MakeMaterialAttributes` seed failed |
| `{Kind} '{Function}' failed to create output '{Name}'.` | the `FunctionOutput` node could not be created |
| `{Kind} '{Function}': ExposeToLibrary must be true or false.` | `Settings = { ExposeToLibrary = … }` is not a boolean literal |
| `{Kind} '{Function}': {Detail}` | the `Graph` body failed to parse; `{Detail}` is the graph-expression error, prefixed with the real file, line and column |
| `Property '{Name}' has a recursive UE builtin dependency.` | a `UE.*` property whose arguments reference it, directly or through another property |
| `Asset '{ObjectPath}' already exists as '{Class}', but ShaderFunction generation requires 'MaterialFunction'. Delete or move the existing asset and regenerate it.` | the target path holds a `UMaterialFunction` subclass or another UClass |
| `Asset '{ObjectPath}' already exists and is not a MaterialFunction asset.` | the target path holds an unrelated object |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | ownership guard: a saved asset at the target path lacks DreamShader provenance metadata |
| `Failed to create package '{PackageName}'.` | the package could not be created |
| `Failed to create material function '{ObjectPath}'.` | the asset object could not be created |
| `DreamShader asset name must resolve to a non-empty asset path.` | `Name` is empty after trimming and slash-stripping |

On success the compile message contains `Generated {Kind} {AssetPath} from {SourceFile}.`

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
ShaderFunction(Name="Functions/F_Tint", Root="Game")
{
    Properties = {
        Group("Tint") {
            float Boost = 1.0 [Slider(0, 4); Description="Extra gain applied after tinting"];
        }
        const float Epsilon = 0.001;
    }

    Inputs = {
        vec3  InColor;
        vec3  InTint                [Description="Multiplied with InColor"];
        opt float Strength = 1.0    [Description="Blend amount"; SortPriority=10];
    }

    Outputs = {
        vec3  OutColor              [Description="Tinted colour"];
        float OutLuma;
    }

    Settings = {
        Description        = "Tint helper";
        UserExposedCaption = "Tint";
        ExposeToLibrary    = true;
        LibraryCategories  = "DreamShader, Color";
    }

    Graph = {
        vec3 Tinted = InColor * InTint * Boost;
        OutColor    = lerp(InColor, Tinted, Strength);
        OutLuma     = dot(OutColor, vec3(0.2126, 0.7152, 0.0722)) + Epsilon;
    }
}
```

Generated asset:

```text
package     /Game/Functions/F_Tint
object path /Game/Functions/F_Tint.F_Tint
class       UMaterialFunction   (usage: Default)
on disk     <Project>/Content/Functions/F_Tint.uasset      (persist mode only)

interface   in  InColor   Vector3
            in  InTint    Vector3
            in  Strength  Scalar     (optional, preview 1.0)
            out OutColor  Vector3
            out OutLuma   Scalar
parameters  Boost         ScalarParameter, group "Tint", slider 0..4
```

## See also

- [Source files](source-files.md) — which block kinds each of `.dsm` / `.dsh` / `.dsf` may contain
- [Shader](shader.md) — the `UMaterial`-producing top-level block
- [ShaderLayer / ShaderLayerBlend](shader-layer.md) — the material-layer variants of this block
- [VirtualFunction](virtual-function.md) — declaring an existing `UMaterialFunction` instead of generating one
- [Properties](properties.md) — the `Properties` section grammar, `Group(…)` scopes and `const`
- [Inputs / Outputs / Results](inputs-outputs.md) — the typed-parameter grammar in full
- [Metadata](../parameters/metadata.md) — the `[ … ]` block, `Slider(…)` and reflected passthrough
- [Types](types.md) — the complete type-token catalogue and per-context validity matrix
- [Layout](layout.md) — `Node` / `Comment` placement directives and `#Region`
- [Graph](../graph/index.md) — the statement/expression language inside `Graph`
- [Calls](../graph/calls.md) — calling functions from a `Graph`, argument forms, `default`
- [Function settings](../settings/function.md) — the keys a material-function `Settings` honours
- [Asset paths](../generation/asset-paths.md) — `Name=` + `Root=` → package path
- [Regeneration](../generation/regeneration.md) — what survives a rebuild and what does not
- [Caching](../generation/caching.md) — the source-hash skip
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
