# ShaderLayer / ShaderLayerBlend

> [DreamShader](../index.md) » [DreamShaderLang](index.md) » **ShaderLayer / ShaderLayerBlend**

Two top-level blocks that declare Unreal's material-layer function assets: a layer, which produces one
`MaterialAttributes` value, and a layer blend, which combines two of them.

| | |
| :-- | :-- |
| Declared in | `.dsf`, or a `.dsm` without a `Shader` — beside a `Shader` either one is [`DSH6201`](../diagnostics/DSH6xxx.md#dsh6201), in a `.dsh` [`DSH2249`](../diagnostics/DSH2xxx.md#dsh2249) (see [Source files](source-files.md#how-the-restriction-is-enforced)) |
| Kind | top-level block |
| Generates | `UMaterialFunctionMaterialLayer` (`ShaderLayer`) / `UMaterialFunctionMaterialLayerBlend` (`ShaderLayerBlend`) |
| Multiplicity | any number per file |
| Since | `1.3.0` |

The legacy front end reads a `ShaderLayer` as a 2.0 `/// @layer export void L(inout material m)` and a
`ShaderLayerBlend` as `/// @layerblend export void B(material …, inout material Result)`; see
[the 2.0 language](../language-v2/index.md) and [`dsc migrate`](../tools/migrate.md).

## Synopsis

```c
ShaderLayer(Name = "<asset-path>" [, Root = "<root>"])
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
ShaderLayerBlend(Name = "<asset-path>" [, Root = "<root>"])
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

Both keywords are matched **case-sensitively**, as whole words; section names are matched
case-insensitively.

## Header attributes

| Attribute | Required | Value | Effect |
| :-- | :-- | :-- | :-- |
| **`Name`** | yes | string | The asset's logical path. The last `/`-separated segment is the asset name; preceding segments become folders. |
| `Root` | no | string | The package root the folders hang off. Defaults to `/Game` when absent or empty. |

Attribute keys are matched case-insensitively. Values may be quoted or bare; a bare value runs to the
next `,` or `)` outside parentheses. A key written twice is a warning
([`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244)) and the later value wins. *(since 2.0.0)* A trailing
comma before `)` is [`DSH2243`](../diagnostics/DSH2xxx.md#dsh2243). See
[Asset paths](../generation/asset-paths.md).

## Sections

Both blocks share the [`ShaderFunction`](shader-function.md#sections) body parser, so the section
table is identical.

| Section | Accepted | Repeat behaviour | Reference |
| :-- | :-- | :-- | :-- |
| `Properties` | yes — function-local parameter, `const` and `UE.*` nodes; this is where layer controls belong | appends | [Properties](properties.md) |
| `Inputs` | yes — constrained, see below | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Outputs` | yes — one `MaterialAttributes` output | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Results` | yes — alias for `Outputs`, no warning | appends | [Inputs / Outputs / Results](inputs-outputs.md) |
| `Settings` | yes — the keys a `ShaderFunction` honours | merges; a key written twice is a warning ([`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262)) | [Function settings](../settings/function.md) |
| `Graph` | yes | the later one wins, with a warning ([`DSH2258`](../diagnostics/DSH2xxx.md#dsh2258)) | [Graph](../graph/index.md) |
| `Layout` | yes | the later one replaces the earlier, with `DSH2258` | [Layout](layout.md) |
| `Code` | **no** — [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | — | — |
| anything else, `Options` included | **no** — [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | — | — |

## Interface rules

These are the rules that distinguish a layer or blend from a plain
[`ShaderFunction`](shader-function.md). *(since 2.0.0)* They are the 2.0 signature rules of `@layer`
and `@layerblend`, checked when the file is bound.

| # | Applies to | Rule | Code |
| :-- | :-- | :-- | :-- |
| 1 | both | a `MaterialAttributes` output is declared | [`DSH3278`](../diagnostics/DSH3xxx.md#dsh3278) |
| 2 | `ShaderLayer` | the material is all there is: any input that is not `MaterialAttributes`, and any output besides the material, breaks the signature | [`DSH6204`](../diagnostics/DSH6xxx.md#dsh6204) |
| 3 | `ShaderLayerBlend` | at least one `MaterialAttributes` input, and no output besides the material | [`DSH6205`](../diagnostics/DSH6xxx.md#dsh6205) |

A layer's material flows through it: *(since 2.0.0)* the layer always has exactly one
`MaterialAttributes` input, named after its output — also when the 1.x block declared no input at
all. A 1.x `MaterialAttributes` input with another name becomes that input, and the pin changes its
name with a warning ([`DSH3277`](../diagnostics/DSH3xxx.md#dsh3277)); the body still reads it under
its 1.x name. The input is optional only when the 1.x input was declared `opt`.

A blend's output starts empty, and each of its inputs is a pin of its own. The binder takes any number
of `MaterialAttributes` inputs from one up; the engine's layer stack blends exactly two.

`MaterialAttributes` is one word, matched ignoring case; `Material Attributes` with a space is
[`DSH3271`](../diagnostics/DSH3xxx.md#dsh3271) *(since 2.0.0)*. A missing `Graph` is not reported.

> [!NOTE]
> Scalars, vectors and textures cannot be layer inputs — rule 2 rejects them. Expose them through
> `Properties` instead: they become parameter nodes inside the generated function and appear on the
> layer stack's parameter panel. Do the same for blend controls.

## Blend input relevance *(since UE 5.7)*

On UE 5.7 and newer, each `MaterialAttributes` input of a `ShaderLayerBlend` gets a
`BlendInputRelevance` derived from its declared name. The name is normalized by removing spaces, `_`
and `-`, then compared case-insensitively.

| Normalized input name | `BlendInputRelevance` |
| :-- | :-- |
| `Top`, `TopLayer` | `Top` |
| `Bottom`, `BottomLayer`, `Base`, `BaseLayer` | `Bottom` |
| anything else — the first `MaterialAttributes` input | `Bottom` |
| anything else — a later `MaterialAttributes` input | `Top` |

Inputs of a `ShaderLayer`, and any non-`MaterialAttributes` input, get no relevance and keep the
engine's `General`. On UE 5.3 – 5.6 the property does not exist and nothing is written.

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
asset therefore fails with [`DSH8110`](../diagnostics/DSH8xxx.md#dsh8110).

Everything else about generation — the honoured `Settings` keys, lazy property-node creation,
input/output `Id` preservation across regeneration, the source-hash skip, and what stops a rebuild —
is identical to [`ShaderFunction`](shader-function.md#generated-asset).

A blend's `MaterialAttributes` output starts as an empty material. A layer's starts as the material
that came in through its input — or empty, when the 1.x input had another name (see
[Interface rules](#interface-rules)). Either way member writes such as `Attrs.BaseColor = …;` are
legal in the `Graph`. See [MaterialAttributes](../graph/material-attributes.md).

## Deprecated spellings

> [!WARNING]
> `MaterialLayer(...)` and `MaterialLayerBlend(...)` are **deprecated since 1.3.0**. Both still parse
> and generate exactly the same assets as their modern spellings, and each is a warning
> ([`DSH2251`](../diagnostics/DSH2xxx.md#dsh2251)). Use `ShaderLayer` / `ShaderLayerBlend` in new code.

| Deprecated spelling | Replacement | Behaves as |
| :-- | :-- | :-- |
| `MaterialLayer(Name = …)` | `ShaderLayer(Name = …)` | identical: same sections, same rules, same asset |
| `MaterialLayerBlend(Name = …)` | `ShaderLayerBlend(Name = …)` | identical |

A missing `Name` ([`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242)) names the spelling the author
actually typed.

## Notes

- **A file makes a material or function assets, not both** *(since 2.0.0)*. A layer or blend beside a
  `Shader` is `DSH6201`. Without a `Shader`, one compile of a `.dsm` or `.dsf` generates every layer,
  blend and `ShaderFunction` it declares. See [Source files](source-files.md).
- **The file-kind restriction is a parse, decided per block** *(since 2.0.0)*. A `.dsh` holding any of
  the four spellings is `DSH2249`; a comment or a string that mentions `ShaderLayer(` is fine.
- The consumer of a layer or blend is Unreal's material layer stack on a material or material
  instance. *(since 2.0.0)* Neither can be called from a `Graph`:
  [`DSH6208`](../diagnostics/DSH6xxx.md#dsh6208).
- The `Code` section is rejected outright (`DSH2246`); use `Graph`.

## Diagnostics

Each code carries the line and column of the construct; the code's page has the message. The
parse-time codes of the shared body parser — attributes, sections, parameter statements, metadata,
`Settings` — are listed on [`ShaderFunction` § Diagnostics](shader-function.md#diagnostics).

| Code | Raised when |
| :-- | :-- |
| `DSH2251` | `MaterialLayer` or `MaterialLayerBlend` (warning) |
| `DSH2242` | no `Name`, or an empty one |
| `DSH3278` | no `MaterialAttributes` output |
| `DSH3277` | the layer's `MaterialAttributes` input is renamed after the output (warning) |
| `DSH6204` | a `ShaderLayer` with any other input or output |
| `DSH6205` | a `ShaderLayerBlend` without a `MaterialAttributes` input, or with another output |
| `DSH6201` | the file also has a `Shader` |
| `DSH6208` | a `Graph` calls a layer or blend |
| `DSH8110` | an asset of an incompatible class is at the target path |

The emit-time codes for the asset itself — [`DSH8111`](../diagnostics/DSH8xxx.md#dsh8111),
[`DSH8112`](../diagnostics/DSH8xxx.md#dsh8112), [`DSH8114`](../diagnostics/DSH8xxx.md#dsh8114),
[`DSH8206`](../diagnostics/DSH8xxx.md#dsh8206), [`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207) — are
a `ShaderFunction`'s.

On success the compile output has a `Generated <Kind> <AssetPath> from <SourceFile>.` line, `<Kind>`
being `MaterialLayer` or `MaterialLayerBlend`.

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
            in  Attrs   MaterialAttributes   (the material the layer is applied to; required, no `opt` input in the source)
            out Attrs   MaterialAttributes

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
- [Diagnostics index](../diagnostics/index.md) — every code, by stage
