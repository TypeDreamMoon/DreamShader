# Settings

> [DreamShader](../index.md) » **Settings**

The section that carries `<key> = <value>;` pairs from a source block to the asset it generates.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf`, `.dsh` — inside `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` or `VirtualFunction` |
| Kind | section |
| Generates | nothing directly — it writes properties on the asset the enclosing block generates |

## Synopsis

```c
Settings [=] { <setting-statement> … }

<setting-statement> := <key> = <value> ;
<key>               := <segment> [ . <segment> ] …
<segment>           := <identifier> [ [<integer>] ]
<value>             := <bare-text> | "<text>"
```

The `=` between the section name and its `{ … }` block is optional sugar *(since 1.5.0)*. The `;`
after the last statement is optional. The `[<integer>]` in `<segment>` is **literal** DreamShaderLang
punctuation, not the optional-marker meta-bracket; the surrounding `[ … ]` is the meta-bracket.

Nested `.` paths and `[<integer>]` indices are only meaningful in a [`Shader`](material.md) block. In
every other block kind the key is looked up whole.

## Where a `Settings` block is accepted

| Block | Section keywords | Key handling | Reference |
| :-- | :-- | :-- | :-- |
| `Shader` | `Settings` | 6 special keys, then reflection onto `UMaterial`; an unknown key is a **hard error** | [Shader settings](material.md) |
| `ShaderFunction` | `Settings` | 4 recognized keys; every other key is **silently ignored** | [Function settings](function.md) |
| `ShaderLayer` | `Settings` | same 4 keys | [Function settings](function.md) |
| `ShaderLayerBlend` | `Settings` | same 4 keys | [Function settings](function.md) |
| `MaterialLayer` / `MaterialLayerBlend` *(deprecated in 1.3.0)* | `Settings` | same 4 keys | [ShaderLayer](../language/shader-layer.md) |
| `VirtualFunction` | `Settings` **or** `Options` | `Asset` (required) and `Description` | [Options](../language/options.md) |
| `Function` / `GraphFunction` | — | no `Settings` section exists; only `Inputs`/`Properties`, `Outputs`/`Results` and `Code`/`Graph` | [Function](../language/function.md) |

A block may contain **more than one** `Settings` section. They merge into one map; a key declared
twice keeps the **last** value, regardless of which section it came from.

## How a statement is parsed

The same routine parses every `Settings` (and `Options`) block, in this order:

| # | Step | Consequence |
| --: | :-- | :-- |
| 1 | `//` and `/* … */` comments are stripped from the whole block | comments may appear anywhere, including mid-statement |
| 2 | The block is split on `;` at parenthesis depth 0 **and** bracket depth 0, outside string literals | `Color = (R=1,G=0,B=0);` is one statement; empty statements are dropped, so a trailing `;` is optional |
| 3 | Each statement is split on the **first** `=` at parenthesis/bracket depth 0 outside a string | the inner `=` of `(R=1,G=0,B=0)` does not split the statement |
| 4 | The key is normalized — trimmed and lowercased | key matching is case-insensitive |
| 5 | The value is unquoted — if the trimmed text both starts and ends with `"`, the quotes are removed and `\` escapes are unescaped; otherwise the trimmed text is kept verbatim | quotes are **optional on every setting** |
| 6 | The pair is added to the block's map | duplicate keys overwrite, last wins |

Because of step 5, `TwoSided = true;` and `TwoSided = "true";` are identical, and
`Domain = Surface;` behaves exactly like `Domain = "Surface";`.

## Key normalization

Three different normalizations act on `Settings` data. Confusing them is the source of most surprises
on this page.

| Stage | Operations, in order | Applied to |
| :-- | :-- | :-- |
| Key storage and direct lookup | trim, lowercase | every key stored in the parsed map; every direct key probe (`Backend`, `ShadingModel`, `Description`, `ExposeToLibrary`, …) |
| Reflection lookup | trim, lowercase, delete every space, `_` and `-` | `Shader` path segments, the [alias table](material.md#alias-table), and the special-key test |
| Value lookup for enums | trim, lowercase, delete every space, `_` and `-` | `ShadingModel`, `BlendMode`, `Domain` values and every reflected enum-typed value (which additionally deletes `:`, `.` and `/`) |

Nothing else is stripped. In particular the storage normalization does **not** remove spaces,
underscores or hyphens — so a key spelled `Two_Sided` is stored as `two_sided` and only matches later
because the *reflection* lookup strips the underscore.

> [!WARNING]
> The six special `Shader` keys are matched by *direct* lookup (trim + lowercase) but skipped from the
> reflection loop by the *reflection* normalization (which also strips ` `, `_`, `-`). A spelling that
> differs from the canonical name only by spaces, underscores or hyphens therefore matches neither
> path and is **applied nowhere, with no diagnostic**: `Blend_Mode`, `blend mode`, `Shading-Model`,
> `Material_Domain`, `Back_end`. Write `BlendMode`, `RenderType`, `ShadingModel`, `MaterialDomain`,
> `Domain` and `Backend` without separators. See [Shader settings](material.md#special-keys).

## This is not a fixed key list

`Settings` in a `Shader` block is **not** a catalogue of supported keys. Exactly six keys are
hand-handled; every other key is resolved against the generated `UMaterial` by Unreal reflection.

```text
Settings key
   ├── one of  blendmode rendertype shadingmodel materialdomain domain backend
   │             └── hand-handled: value parsed through the project's alias maps
   └── anything else
                 └── alias table  →  UMaterial / UMaterialInterface / UObject property lookup
                                        (name, b-stripped name, or DisplayName)
                                     →  value parsed per the property's C++ type
```

Two consequences a reference reader must internalise:

- Any `UPROPERTY` on `UMaterial` or one of its bases is reachable, including nested struct paths
  (`Lightmass.DiffuseBoost`) and fixed-array indices (`PhysicalMaterialMap[2]`). No table in this
  manual can be complete for that surface, because it is the engine's property set, not the plugin's.
- Which enum *values* are accepted likewise comes from engine reflection, so a modified engine build
  contributes its own spellings automatically. See [Material enums](material-enums.md).

Material-function blocks are the opposite: exactly four keys are read and everything else is dropped
in silence. See [Function settings](function.md).

## Pages

| Page | Covers |
| :-- | :-- |
| [Shader settings](material.md) | the special keys, the alias table, the reflection resolver, value grammar per property type, and what every generated material is reset to |
| [Material enums](material-enums.md) | every accepted `ShadingModel`, `BlendMode` and `Domain` spelling |
| [Backend](backend.md) | `Graph` vs `ThinCustom`, the deprecated `Instance` alias, precedence against the project default |
| [Function settings](function.md) | `Settings` inside `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` |
| [Project settings](project.md) | *Project Settings ▸ DreamPlugin ▸ Dream Shader* — all 13 config properties |

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this page.

### Parse time

| Message | Cause |
| :-- | :-- |
| `Invalid setting declaration '{Statement}'.` | a statement with no `=` at parenthesis/bracket depth 0 |
| `Invalid empty setting key in '{Statement}'.` | the text before `=` is empty after trimming |
| `Unknown shader section '{Section}'.` | a section name inside `Shader { … }` that is not `Properties`, `Settings`, `Outputs`, `Graph`, `Layout` or `Code` |
| `Unknown material function section '{Section}'.` | an unrecognized section inside `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` |
| `Unknown VirtualFunction section '{Section}'.` | an unrecognized section inside `VirtualFunction` |

Errors raised while *applying* the settings are listed on [Shader settings](material.md#diagnostics),
[Backend](backend.md#diagnostics) and [Function settings](function.md#diagnostics).

## Example

```c
Shader(Name="Docs/M_SettingsGrammar")
{
    Properties { vec3 Tint = vec3(1.0, 0.5, 0.25); }

    // Two Settings sections merge into one map.
    Settings {
        Domain       = Surface;        // quotes are optional
        ShadingModel = "Unlit";
        BlendMode    = "Opaque";
    }

    Settings = {
        BlendMode           = "Translucent";   // last write wins over "Opaque"
        TwoSided            = true;            // reflected onto UMaterial::TwoSided
        OpacityMaskClipValue = 0.5;            // reflected, float-typed
        Lightmass.DiffuseBoost = 1.25;         // nested struct path
    }

    Outputs {
        vec3  Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph {
        Color = Tint;
        Alpha = 0.5;
    }
}
```

Effective map after parsing:

```text
domain                  -> Surface
shadingmodel            -> Unlit
blendmode               -> Translucent
twosided                -> true
opacitymaskclipvalue    -> 0.5
lightmass.diffuseboost  -> 1.25
```

## See also

- [Shader settings](material.md) — the full `Shader` key surface
- [Material enums](material-enums.md) — every `ShadingModel`, `BlendMode` and `Domain` spelling
- [Backend](backend.md) — choosing the materialization strategy
- [Function settings](function.md) — the four material-function keys
- [Project settings](project.md) — the project-wide defaults and mapping maps
- [Shader](../language/shader.md) — the block a material `Settings` section lives in
- [ShaderFunction](../language/shader-function.md) — the material-function block
- [VirtualFunction](../language/virtual-function.md) — `Options` / `Settings` on an existing asset
- [Options](../language/options.md) — the `VirtualFunction` `Options` keys
- [Lexical elements](../language/lexical.md) — comments, string literals and escapes
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
