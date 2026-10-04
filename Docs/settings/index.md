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

<setting-statement> := <name> = <value> ;
<name>              := <identifier>
<value>             := <tokens up to the next ;> | "<text>"
```

The `=` between the section name and its `{ … }` block is optional sugar *(since 1.5.0)*. The `;`
after the last statement is optional.

*(since 2.0.0)* A name is **one identifier**. 1.x also read a nested path (`Lightmass.DiffuseBoost`)
and a fixed-array index (`PhysicalMaterialMap[2]`) as a key of a `Shader` block; the legacy front end
stops at the `.` or the `[` and reports [`DSH3261`](../diagnostics/DSH3xxx.md#dsh3261).

A `Shader`'s `Settings` is the 2.0 `#pragma material(…)` — see [The 2.0 language](../language-v2/index.md).

## Where a `Settings` block is accepted

| Block | Section keywords | Key handling | Reference |
| :-- | :-- | :-- | :-- |
| `Shader` | `Settings` | `Backend` and `Substrate` are read by the compiler; `BlendMode` / `RenderType`, `ShadingModel`, `MaterialDomain` / `Domain` are hand-handled; every other key is written onto `UMaterial` by reflection — an unknown key fails the build ([`DSH8215`](../diagnostics/DSH8xxx.md#dsh8215)) | [Shader settings](material.md) |
| `ShaderFunction` | `Settings` | 4 recognized keys; every other key is a [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263) warning and is ignored | [Function settings](function.md) |
| `ShaderLayer` | `Settings` | same 4 keys | [Function settings](function.md) |
| `ShaderLayerBlend` | `Settings` | same 4 keys | [Function settings](function.md) |
| `MaterialLayer` / `MaterialLayerBlend` *(deprecated in 1.3.0; [`DSH2251`](../diagnostics/DSH2xxx.md#dsh2251))* | `Settings` | same 4 keys | [ShaderLayer](../language/shader-layer.md) |
| `VirtualFunction` | `Settings` **or** `Options` | `Asset` (required, [`DSH6312`](../diagnostics/DSH6xxx.md#dsh6312)) and `Description`; any other key is ignored | [Options](../language/options.md) |
| `Function` / `GraphFunction` | — | no `Settings` section exists; only `Inputs`/`Properties`, `Outputs`/`Results` and `Code`/`Graph` | [Function](../language/function.md) |

A block may contain **more than one** `Settings` section. They merge into one list; a key written
twice keeps the **last** value, regardless of which section it came from, and the repeat is a
[`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) warning *(since 2.0.0)*.

## How a statement is parsed

*(since 2.0.0)* The legacy front end reads a `Settings` section from the same tokens as the rest of the
file:

| # | Step | Consequence |
| --: | :-- | :-- |
| 1 | `//` and `/* … */` comments are trivia | comments may stand between any two tokens, including mid-statement |
| 2 | A statement is a name, `=`, and a value; the value runs to the next `;` at depth zero of `( )`, `[ ]` and `{ }`, or to the `}` that closes the section | `Color = (R=1,G=0,B=0);` is one statement; empty statements are skipped, so a trailing `;` is optional |
| 3 | The name is one identifier | `Foo.Bar = 1;` and `Foo[2] = 1;` are `DSH3261`, as are a statement with no `=` and an `=` with no value |
| 4 | A value that is one string literal gives its contents, its escapes read by the lexer; any other value is its text as written | quotes are **optional on every setting** |
| 5 | A name written twice, compared ignoring case, keeps the later value | `DSH3262` |

Because of step 4, `TwoSided = true;` and `TwoSided = "true";` are identical, and
`Domain = Surface;` behaves exactly like `Domain = "Surface";`.

## Key normalization

Several normalizations act on `Settings` data. Confusing them is the source of most surprises on this
page.

| Stage | Operations, in order | Applied to |
| :-- | :-- | :-- |
| Duplicate check, `Backend`, `Substrate` | case-insensitive comparison | every key of the block |
| Key storage and direct lookup | trim, lowercase | every key that reaches the material; every direct key probe (`BlendMode`, `ShadingModel`, `Domain`, …) |
| Reflection lookup | trim, lowercase, delete every space, `_` and `-` | property names, the [alias table](material.md#alias-table), and the special-key test |
| Value lookup for enums | trim, lowercase, delete every space, `_` and `-` | `ShadingModel`, `BlendMode`, `Domain` values and every reflected enum-typed value (which additionally deletes `:`, `.` and `/`) |

Nothing else is stripped. A key spelled `Two_Sided` is stored as `two_sided` and only matches later
because the *reflection* lookup strips the underscore.

> [!WARNING]
> The special `Shader` keys are matched by *direct* lookup (trim + lowercase) but skipped from the
> reflection pass by the *reflection* normalization (which also strips `_`). A spelling that differs
> from the canonical name only by underscores therefore matches neither and is **applied nowhere, with
> no diagnostic**: `Blend_Mode`, `Render_Type`, `Shading_Model`, `Material_Domain`, `Back_end`. Write
> `BlendMode`, `RenderType`, `ShadingModel`, `MaterialDomain`, `Domain` and `Backend` without
> separators. (A space or a hyphen cannot stand in a name at all since 2.0.0.) See
> [Shader settings](material.md#special-keys).

## This is not a fixed key list

`Settings` in a `Shader` block is **not** a catalogue of supported keys. A handful of keys are
hand-handled; every other key is resolved against the generated `UMaterial` by Unreal reflection.

```text
Settings key
   ├── Backend, Substrate
   │      └── read by the compiler when the source is bound; never reach the material
   ├── blendmode rendertype shadingmodel materialdomain domain
   │      └── hand-handled: value parsed through the project's alias maps
   └── anything else
          └── alias table  →  UMaterial / UMaterialInterface / UObject property lookup
                                 (name, b-stripped name, or DisplayName)
                              →  value parsed per the property's C++ type
```

Two consequences a reference reader must internalise:

- Any `UPROPERTY` on `UMaterial` or one of its bases is reachable by its name. Nested struct members
  and fixed-array elements are not reachable from a 1.x key *(since 2.0.0)*; a fixed-size array named
  without an index is [`DSH7121`](../diagnostics/DSH7xxx.md#dsh7121). No table in this manual can be
  complete for that surface, because it is the engine's property set, not the plugin's.
- Which enum *values* are accepted likewise comes from engine reflection, so a modified engine build
  contributes its own spellings automatically. See [Material enums](material-enums.md).

Material-function blocks are the opposite: exactly four keys are read and every other key is a
`DSH3263` warning. See [Function settings](function.md).

## Pages

| Page | Covers |
| :-- | :-- |
| [Shader settings](material.md) | the special keys, the alias table, the reflection resolver, value grammar per property type, and what every generated material is reset to |
| [Material enums](material-enums.md) | every accepted `ShadingModel`, `BlendMode` and `Domain` spelling |
| [Backend](backend.md) | `Graph` vs `ThinCustom`, the old `Instance` spelling, precedence against the project default |
| [Function settings](function.md) | `Settings` inside `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` |
| [Project settings](project.md) | *Project Settings ▸ DreamPlugin ▸ Dream Shader* — all seventeen config properties, including *Preprocessor Defines* |

## Diagnostics

### Parse time

| Code | Raised when |
| :-- | :-- |
| `DSH3261` | no `{` after `Settings`; a statement that does not start with a name; a name with no `=` after it (which is what a `.` or `[` in a key gives); an `=` with no value |
| `DSH3262` | a name written twice in the block, compared ignoring case (warning; the later value wins) |
| `DSH3263` | a function block's key it does not read (warning; ignored) |
| [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | a section name inside `Shader`, a function block or a `VirtualFunction` that the block does not have |
| [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) | a `Code` section in a `Shader` or function block, which takes `Graph` |

Diagnostics raised while *applying* the settings are listed on [Shader settings](material.md#diagnostics),
[Backend](backend.md#diagnostics) and [Function settings](function.md#diagnostics).

## Example

```c
Shader(Name="Docs/M_SettingsGrammar")
{
    Properties { vec3 Tint = vec3(1.0, 0.5, 0.25); }

    // Two Settings sections merge into one list.
    Settings {
        Domain       = Surface;        // quotes are optional
        ShadingModel = "Unlit";
        BlendMode    = "Opaque";
    }

    Settings = {
        BlendMode           = "Translucent";   // last write wins over "Opaque" (DSH3262)
        TwoSided            = true;            // reflected onto UMaterial::TwoSided
        OpacityMaskClipValue = 0.5;            // reflected, float-typed
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

Effective settings, keys as the emitter stores them:

```text
domain                  -> Surface
shadingmodel            -> Unlit
blendmode               -> Translucent
twosided                -> true
opacitymaskclipvalue    -> 0.5
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
- [Diagnostics index](../diagnostics/index.md) — every code
