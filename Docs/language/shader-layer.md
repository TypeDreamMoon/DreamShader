# ShaderLayer / ShaderLayerBlend

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **ShaderLayer / ShaderLayerBlend**

Two top-level blocks that declare Unreal's material-layer function assets: a layer, which produces one
`MaterialAttributes` value, and a layer blend, which combines two of them.

| | |
| :-- | :-- |
| Declared in | `.dsm` and `.dsf` — a `.dsh` containing `ShaderLayer(`, `ShaderLayerBlend(`, `MaterialLayer(` or `MaterialLayerBlend(` is rejected before parsing |
| Kind | top-level block |
| Generates | `UMaterialFunctionMaterialLayer` (`ShaderLayer`) / `UMaterialFunctionMaterialLayerBlend` (`ShaderLayerBlend`) |
| Multiplicity | any number per parse unit |
| Since | `1.3.0` |

## Synopsis

```c
ShaderLayer(Name = "<asset-path>" [, Root = "<root>"] [,])
{
    [Properties            [=] { <property-declaration> ; … }]
    [Inputs                [=] { MaterialAttributes <name> ; }]
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

```c
ShaderLayerBlend(Name = "<asset-path>" [, Root = "<root>"] [,])
{
    [Properties            [=] { <property-declaration> ; … }]
    Inputs                 [=] { MaterialAttributes <name> ; MaterialAttributes <name> ; }
    { Outputs | Results }  [=] { MaterialAttributes <name> ; }
    Graph                  [=] { <graph-statement> … }
    [Settings              [=] { <key> = <value> ; … }]
    [Layout                [=] { { Node( … ) | Comment( … ) } ; … }]
}
```

The `=` between a section name and its `{ … }` block is optional sugar *(since 1.5.0)*; a `;` after a
section's closing `}` is optional. Sections may appear in any order and may be repeated.

Both keywords are matched **case-sensitively**; section names are matched case-insensitively.
`ShaderLayerBlend` is attempted before `ShaderLayer`, and the keyword matcher requires a
non-alphanumeric character after the keyword, so `ShaderLayerBlend` never matches as `ShaderLayer`.

## Header attributes

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | string | The asset's logical path. The last `/`-separated segment is the asset name; preceding segments become folders. |
| `Root` | no | string | The package root the folders hang off. Defaults to `/Game` when absent or empty. |

Attribute keys are matched case-insensitively. Values may be quoted or bare; a bare value ends at the
first `,` or `)`. A trailing comma before `)` is accepted. A duplicate key silently overwrites the
earlier one. See [Asset paths](../generation/asset-paths.md).

## Sections

Both blocks share the [`ShaderFunction`](shader-function.md) body parser, so the section table is
identical.

| Section | Accepted | Repeat behaviour | Reference |
| :-- | :-- | :-- | :-- |
| `Properties` | yes — function-local parameter, `const` and `UE.*` nodes; this is where layer controls belong | appends | [Properties](properties.md) |
| `Inputs` | yes — arity-constrained, see below | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Outputs` | yes — exactly one `MaterialAttributes` output | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Results` | yes — alias for `Outputs`, no warning | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Settings` | yes — the same four keys a `ShaderFunction` honours | merges; last key wins | [Function settings](../settings/function.md) |
| `Graph` | yes — **required** | overwrites the previous body | [Graph](../graph/index.md) |
| `Layout` | yes | **resets** — a second `Layout` discards the first | [Layout](layout.md) |
| `Code` | **no** — hard error | — | — |
| `Options` | no — unknown section | — | — |

## Interface arity rules

These are the only rules that distinguish a layer or blend from a plain
[`ShaderFunction`](shader-function.md). They are evaluated at generation time, in the order listed.

| # | Applies to | Rule | Diagnostic |
| :-- | :-- | :-- | :-- |
| 1 | both | at least one output must be declared | `{Kind} '{Function}' must declare at least one output.` |
| 2 | both | **exactly one** output, and its type must be `MaterialAttributes` | `{Kind} '{Function}' must declare exactly one MaterialAttributes output.` |
| 3 | `ShaderLayer` | **at most one** input, and every declared input must be `MaterialAttributes` | `ShaderLayer '{Function}' must declare at most one input, and it must be MaterialAttributes. Use Properties for layer controls.` |
| 4 | `ShaderLayerBlend` | **exactly two** inputs, both `MaterialAttributes` | `ShaderLayerBlend '{Function}' must declare exactly two inputs, both MaterialAttributes. Use Properties for blend controls.` |
| 5 | both | a `Graph` section is required | `{Kind} '{Function}' must provide a Graph block.` |

`{Kind}` renders as `ShaderLayer` or `ShaderLayerBlend`. Rule 3 permits a layer with **zero** inputs;
rule 4 does not permit a blend with one or three.

`MaterialAttributes` is compared after removing every space and ignoring case, so `Material
Attributes` also satisfies these rules. Any other type token in `Inputs` or `Outputs` fails these
rules rather than the type resolver.

> [!NOTE]
> Scalars, vectors and textures cannot be layer or blend inputs — the arity rules reject them. Expose
> them through `Properties` instead: they become parameter nodes inside the generated function and
> appear on the layer stack's parameter panel. The two diagnostics say so explicitly.

## Blend input relevance *(since UE 5.7)*

On UE 5.7 and newer, each `MaterialAttributes` input of a `ShaderLayerBlend` gets a
`BlendInputRelevance` derived from its declared name. The name is normalized by removing spaces, `_`
and `-`, then compared case-insensitively.

| Normalized input name | `BlendInputRelevance` |
| :-- | :-- |
| `Top`, `TopLayer` | `Top` |
| `Bottom`, `BottomLayer`, `Base`, `BaseLayer` | `Bottom` |
| the engine's `TopMaterialBlendInputName` (compared against the raw, un-normalized name) | `Top` |
| the engine's `BottomMaterialBlendInputName` (compared against the raw, un-normalized name) | `Bottom` |
| anything else — first `MaterialAttributes` input | `Bottom` |
| anything else — second `MaterialAttributes` input | `Top` |

Inputs of a `ShaderLayer`, and any non-`MaterialAttributes` input, resolve to `General`. On UE 5.3 –
5.6 the property does not exist and nothing is written.

## Generated asset

`Name` + `Root` resolve to a package path exactly as described in
[Asset paths](../generation/asset-paths.md).

| Block | UClass created | `EMaterialFunctionUsage` |
| :-- | :-- | :-- |
| `ShaderLayer` | `UMaterialFunctionMaterialLayer` | `MaterialLayer` |
| `ShaderLayerBlend` | `UMaterialFunctionMaterialLayerBlend` | `MaterialLayerBlend` |

When an asset already exists at the target path it is reused if it **is a** subclass of the expected
class; the exact-class rule that applies to [`ShaderFunction`](shader-function.md) is relaxed here.
Switching a block between `ShaderFunction` and `ShaderLayer` without moving or deleting the existing
asset therefore fails with
`Asset '{ObjectPath}' already exists as '{Class}', but {Kind} generation requires '{ExpectedClass}'. Delete or move the existing asset and regenerate it.`

Everything else about generation — the four honoured `Settings` keys, lazy property-node creation,
input/output `Id` preservation across regeneration, construction-time node positions, the source-hash
skip, and what a regeneration destroys — is identical to
[`ShaderFunction`](shader-function.md#generated-asset).

The single `MaterialAttributes` output is pre-seeded with a `MakeMaterialAttributes` node, which is
what makes member writes such as `Attrs.BaseColor = …;` legal in the `Graph`. See
[MaterialAttributes](../graph/material-attributes.md).

## Deprecated spellings

> [!WARNING]
> `MaterialLayer(...)` and `MaterialLayerBlend(...)` are **deprecated since 1.3.0**. Both still parse
> and generate exactly the same assets as their modern spellings, and both push a parse warning that
> is appended to the compile message:
>
> - `MaterialLayer is deprecated; use ShaderLayer instead.`
> - `MaterialLayerBlend is deprecated; use ShaderLayerBlend instead.`
>
> The warnings do not fail the parse. Use `ShaderLayer` / `ShaderLayerBlend` in new code.

| Deprecated spelling | Replacement | Behaves as |
| :-- | :-- | :-- |
| `MaterialLayer(Name = …)` | `ShaderLayer(Name = …)` | identical: same sections, same arity rules, same asset |
| `MaterialLayerBlend(Name = …)` | `ShaderLayerBlend(Name = …)` | identical |

Two consequences of the aliasing:

- A missing `Name` reports the spelling the author actually typed:
  `MaterialLayer(Name="...") is required.`
- Every other diagnostic reports the **modern** kind name. A `MaterialLayer` block with two inputs
  fails with `ShaderLayer '{Function}' must declare at most one input, …`, never `MaterialLayer …`.

## Notes

- **Layer blocks may share a file with anything except a second `Shader`.** One compile of a `.dsm`
  or `.dsf` generates every layer, blend, `ShaderFunction`, `VirtualFunction`, `Function`,
  `GraphFunction` and `Namespace` block it declares, plus the `Shader` if there is one.
- **The file-kind restriction is a substring scan, not a parse.** All four spellings —
  `ShaderLayer(`, `ShaderLayerBlend(`, `MaterialLayer(`, `MaterialLayerBlend(` — are on the `.dsh`
  reject list, including occurrences inside comments and string literals. A `.dsf` only rejects
  `Shader(`, so layer blocks are welcome there. See [Source files](source-files.md).
- A layer or blend can be called like any other material function from a `Graph` in the same parse
  unit, but the usual consumer is Unreal's material layer stack on a material or material instance.
- The `Code` section is rejected outright; use `Graph`.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this section. `{Kind}` is always
`ShaderLayer` or `ShaderLayerBlend` — the deprecated spellings never appear in a diagnostic.

### Parse time

| Message | Cause |
| :-- | :-- |
| `ShaderLayer(Name="...") is required.` | a `ShaderLayer` header with no `Name` attribute |
| `ShaderLayerBlend(Name="...") is required.` | a `ShaderLayerBlend` header with no `Name` attribute |
| `MaterialLayer(Name="...") is required.` | the deprecated spelling with no `Name` attribute |
| `MaterialLayerBlend(Name="...") is required.` | the deprecated spelling with no `Name` attribute |
| `Unknown material function section '{Section}'.` | a section other than `Properties`, `Inputs`, `Outputs`, `Results`, `Settings`, `Graph`, `Layout`, `Code` |
| `ShaderFunction, ShaderLayer, and ShaderLayerBlend graph sections now use Graph = { ... }. Function Code = { ... } is still supported.` | a `Code` section was used |
| `Invalid typed declaration '{Statement}'.` | an `Inputs`/`Outputs`/`Results` statement with no space between type and name, an empty left side, or a name that is not an identifier |
| `Expected '{' near index {Index}.` | the body block is missing |
| `Unterminated block.` | the body `{` is never closed |

### Parse-time warnings

| Message | Cause |
| :-- | :-- |
| `MaterialLayer is deprecated; use ShaderLayer instead.` | the deprecated block spelling |
| `MaterialLayerBlend is deprecated; use ShaderLayerBlend instead.` | the deprecated block spelling |

### Generation time

| Message | Cause |
| :-- | :-- |
| `{Kind} '{Function}' must declare at least one output.` | no `Outputs` / `Results` entry |
| `{Kind} '{Function}' must declare exactly one MaterialAttributes output.` | more than one output, or an output that is not `MaterialAttributes` |
| `ShaderLayer '{Function}' must declare at most one input, and it must be MaterialAttributes. Use Properties for layer controls.` | two or more inputs, or any non-`MaterialAttributes` input |
| `ShaderLayerBlend '{Function}' must declare exactly two inputs, both MaterialAttributes. Use Properties for blend controls.` | an input count other than two, or any non-`MaterialAttributes` input |
| `{Kind} '{Function}' must provide a Graph block.` | no `Graph` section |
| `{Kind} '{Function}' property '{Name}' conflicts with another property or input name.` | duplicate property name, or a property that shadows an input |
| `{Kind} '{Function}' output '{Name}' was never assigned an expression.` | the `Graph` never writes the output |
| `{Kind} '{Function}' output '{Name}' does not match its declared type '{Type}'.` | the assigned value is not a `MaterialAttributes` value |
| `{Kind} '{Function}' output '{Name}': {Detail}` | the output's `MakeMaterialAttributes` seed failed |
| `{Kind} '{Function}' failed to create input '{Name}'.` | the `FunctionInput` node could not be created |
| `{Kind} '{Function}' failed to create output '{Name}'.` | the `FunctionOutput` node could not be created |
| `{Kind} '{Function}': ExposeToLibrary must be true or false.` | `Settings = { ExposeToLibrary = … }` is not a boolean literal |
| `Asset '{ObjectPath}' already exists as '{Class}', but {Kind} generation requires '{ExpectedClass}'. Delete or move the existing asset and regenerate it.` | the target path holds an asset of an incompatible class |
| `Asset '{ObjectPath}' already exists and is not a MaterialFunction asset.` | the target path holds an unrelated object |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | ownership guard: a saved asset at the target path lacks DreamShader provenance metadata |
| `Failed to create material function '{ObjectPath}'.` | the asset object could not be created |

On success the compile message contains `Generated {Kind} {AssetPath} from {SourceFile}.`

The complete cross-stage list lives in the [diagnostics index](../diagnostics/index.md).

## Example

```c
ShaderLayer(Name="Layers/L_Rust", Root="Game")
{
    Properties = {
        Group("Rust") {
            vec3  RustColor = vec3(0.35, 0.13, 0.05) [Description="Base colour of the rust"];
            float RustRough = 0.85                   [Slider(0, 1)];
        }
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = RustColor;
        Attrs.Roughness = RustRough;
        Attrs.Metallic  = 0.0;
    }
}

ShaderLayerBlend(Name="Layers/LB_Wear", Root="Game")
{
    Properties = {
        float WearAmount = 0.5 [Slider(0, 1); Description="0 keeps Bottom, 1 keeps Top"];
    }

    Inputs = {
        MaterialAttributes Bottom;
        MaterialAttributes Top;
    }

    Outputs = {
        MaterialAttributes Attrs;
    }

    Graph = {
        Attrs.BaseColor = lerp(Bottom.BaseColor, Top.BaseColor, WearAmount);
        Attrs.Roughness = lerp(Bottom.Roughness, Top.Roughness, WearAmount);
        Attrs.Normal    = lerp(Bottom.Normal,    Top.Normal,    WearAmount);
    }
}
```

Generated assets:

```text
package     /Game/Layers/L_Rust
object path /Game/Layers/L_Rust.L_Rust
class       UMaterialFunctionMaterialLayer        (usage: MaterialLayer)

package     /Game/Layers/LB_Wear
object path /Game/Layers/LB_Wear.LB_Wear
class       UMaterialFunctionMaterialLayerBlend   (usage: MaterialLayerBlend)
            in  Bottom  MaterialAttributes   (BlendInputRelevance: Bottom, UE 5.7+)
            in  Top     MaterialAttributes   (BlendInputRelevance: Top,    UE 5.7+)
            out Attrs   MaterialAttributes
parameters  WearAmount  ScalarParameter, slider 0..1
```

## See also

- [ShaderFunction](shader-function.md) — the plain `UMaterialFunction` block these two specialize
- [Shader](shader.md) — the `UMaterial`-producing top-level block
- [VirtualFunction](virtual-function.md) — declaring an existing `UMaterialFunction` instead of generating one
- [Source files](source-files.md) — which block kinds each of `.dsm` / `.dsh` / `.dsf` may contain
- [Keywords](keywords.md) — the complete keyword index, including deprecated spellings
- [Properties](properties.md) — the `Properties` section grammar, `Group(…)` scopes and `const`
- [Inputs / Outputs / Results](inputs-outputs.md) — the typed-parameter grammar in full
- [MaterialAttributes](../graph/material-attributes.md) — the value type these blocks are built around
- [Function settings](../settings/function.md) — the keys a material-function `Settings` honours
- [Asset paths](../generation/asset-paths.md) — `Name=` + `Root=` → package path
- [Regeneration](../generation/regeneration.md) — what survives a rebuild and what does not
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
