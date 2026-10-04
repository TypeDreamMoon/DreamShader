# Function settings

> [DreamShader](../index.md) » [Settings](index.md) » **Function settings**

The `Settings` section of a [`ShaderFunction`](../language/shader-function.md),
[`ShaderLayer` or `ShaderLayerBlend`](../language/shader-layer.md) block: four keys that describe the
generated `UMaterialFunction` to the Material Function Library.

| | |
| :-- | :-- |
| Declared in | `.dsm`, `.dsf` — inside `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` |
| Kind | section |
| Generates | property writes on the generated `UMaterialFunction`, `UMaterialFunctionMaterialLayer` or `UMaterialFunctionMaterialLayerBlend` |

## Synopsis

```c
ShaderFunction(Name = "<asset-path>")
{
    Settings [=] {
        [Description       = <text>;]
        [UserExposedCaption = <text>;]
        [ExposeToLibrary   = { true | false };]
        [LibraryCategories = "<category>[, <category>]…";]
    }
}
```

The same section is accepted, with the same four keys, in `ShaderLayer`, `ShaderLayerBlend` and their
deprecated `MaterialLayer` / `MaterialLayerBlend` spellings. In 2.0 the keys are the `/// @desc` and
`/// @library` directives of the exported function — see [The 2.0 language](../language-v2/index.md).

## Keys

Keys are matched ignoring case, so `description`, `Description` and `DESCRIPTION` are the same key.
Underscores are **not** folded here — `Expose_To_Library` is not `ExposeToLibrary`.

| Key | Sets | Value grammar | Value when the key is absent |
| :-- | :-- | :-- | :-- |
| **`Description`** | `UMaterialFunction::Description` | free text | cleared to the empty string |
| **`UserExposedCaption`** | nothing *(since 2.0.0)*: [`DSH3264`](../diagnostics/DSH3xxx.md#dsh3264) warns that it is not applied, and the asset's caption is left as it is | free text | — |
| **`ExposeToLibrary`** | `UMaterialFunction::bExposeToLibrary`, together with `LibraryCategories` | `true` / `false`, case-insensitive; any other value is a [`DSH3265`](../diagnostics/DSH3xxx.md#dsh3265) warning and the setting is ignored | `false` |
| **`LibraryCategories`** | `UMaterialFunction::LibraryCategoriesText`, together with `ExposeToLibrary` | a comma-separated list; each entry is trimmed and empty entries are dropped | the category list is cleared |

*(since 2.0.0)* `ExposeToLibrary` and `LibraryCategories` act as one setting, the 2.0
`/// @library <categories>`: the function is exposed, under those categories, when `ExposeToLibrary`
is `true` **and** `LibraryCategories` is not empty. `ExposeToLibrary = true;` alone leaves the function
out of the library, and `LibraryCategories` without `ExposeToLibrary = true;` is dropped.

`LibraryCategories` clears the list before parsing, so the setting always replaces the asset's
categories rather than appending to them. `LibraryCategories = "A,,  B ,";` yields exactly `A` and
`B`.

Quotes are optional on every value, as everywhere in a `Settings` block:
`Description = Tint helper;` and `Description = "Tint helper";` are identical.

> [!NOTE]
> **Every key here is reset when the key is absent.** Omitting `ExposeToLibrary` sets it to `false`;
> omitting `Description` empties it. A property edited by hand on the generated asset is discarded on
> the next regeneration — except the caption, which 2.0 does not write at all. See
> [Regeneration](../generation/regeneration.md).

> [!WARNING]
> **Any other key is ignored**, with a [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263) warning
> *(since 2.0.0; 1.x said nothing)*. Only these four names are looked up: `Backend`, `Domain`,
> `ShadingModel`, `BlendMode`, `TwoSided` and every other [`Shader` setting](material.md) do
> **nothing** in a `ShaderFunction` block, and a misspelling of one of the four — `Descriptions`,
> `Expose_To_Library`, `LibraryCategory` — is the same warning.

> [!WARNING]
> The [decompiler](../tools/decompiler.md) does not emit a `Settings` block when exporting a
> `UMaterialFunction` to `.dsf`. `Description`, `UserExposedCaption`, `ExposeToLibrary` and
> `LibraryCategories` are lost on that round trip and must be re-added by hand.

## Related validation

These checks are the diagnostics a `Settings` mistake is most often confused with.

| Rule | Code |
| :-- | :-- |
| A material function produces at least one output | [`DSH4315`](../diagnostics/DSH4xxx.md#dsh4315) |
| A `ShaderLayer` or `ShaderLayerBlend` has a `MaterialAttributes` output | [`DSH3278`](../diagnostics/DSH3xxx.md#dsh3278) |
| A `ShaderLayer` has that material as its only input and output | [`DSH6204`](../diagnostics/DSH6xxx.md#dsh6204) |
| A `ShaderLayerBlend` has, besides the material output, inputs only — at least one of them `MaterialAttributes` | [`DSH6205`](../diagnostics/DSH6xxx.md#dsh6205) |
| The body section is `Graph`, not `Code` | [`DSH2246`](../diagnostics/DSH2xxx.md#dsh2246) |
| Two properties of the file do not share a name | [`DSH4210`](../diagnostics/DSH4xxx.md#dsh4210) |

The generated asset's usage follows the block kind: `ShaderFunction` produces an ordinary material
function, `ShaderLayer` a material-layer function, `ShaderLayerBlend` a layer-blend function.

## Notes

- A [`VirtualFunction`](../language/virtual-function.md) block also accepts a section named
  `Settings`, but it is a synonym for `Options` and takes an entirely different key set — `Asset` and
  `Description`. See [Options](../language/options.md).
- [`Function`](../language/function.md) and [`GraphFunction`](../language/graph-function.md) blocks
  have no `Settings` section at all; their only sections are `Inputs` / `Properties`,
  `Outputs` / `Results` and `Code` / `Graph`.
- Multiple `Settings` sections in one block merge, last key wins, with a
  [`DSH3262`](../diagnostics/DSH3xxx.md#dsh3262) warning, exactly as in a `Shader` block. See
  [Settings](index.md#how-a-statement-is-parsed).
- `ExposeToLibrary = true;` with a `LibraryCategories` is what makes the function appear in the
  Material palette's function library; the categories decide where in that palette it sits.

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH3265` | the `ExposeToLibrary` value is not a boolean literal (warning; the setting is ignored) |
| `DSH3264` | `UserExposedCaption` is set (warning; not applied) |
| `DSH3263` | a key other than the four (warning; ignored) |
| `DSH3262` | a key written twice (warning; the later value wins) |
| `DSH4315` | the block produces no output |
| `DSH3278` | a layer or blend has no `MaterialAttributes` output |
| `DSH6204` / `DSH6205` | a layer's / a blend's parameters do not fit its kind |
| [`DSH2245`](../diagnostics/DSH2xxx.md#dsh2245) | a section name other than the recognized ones |

`Description` and `LibraryCategories` have no failure mode: any text is accepted.

## Example

```c
ShaderFunction(Name="Functions/F_Tint", Root="Game")
{
    Inputs = {
        vec3 InColor;
        vec3 InTint;
    }

    Outputs = {
        vec3 OutColor;
    }

    Settings = {
        Description        = "Multiplies a colour by a tint.";
        UserExposedCaption = "Tint";
        ExposeToLibrary    = true;
        LibraryCategories  = "DreamShader, Color";
    }

    Graph = {
        OutColor = InColor * InTint;
    }
}
```

Generated asset:

```text
package  /Game/Functions/F_Tint                     UMaterialFunction
  Description           = "Multiplies a colour by a tint."
  bExposeToLibrary      = true
  LibraryCategoriesText = ["DreamShader", "Color"]
```

`UserExposedCaption` is reported with `DSH3264` and not applied.

## See also

- [Settings](index.md) — the block grammar and key normalization shared by every block kind
- [Shader settings](material.md) — the very different key surface of a `Shader` block
- [ShaderFunction](../language/shader-function.md) — the enclosing block
- [ShaderLayer / ShaderLayerBlend](../language/shader-layer.md) — the layer block kinds and their arity rules
- [VirtualFunction](../language/virtual-function.md) — declaring an existing `UMaterialFunction`
- [Options](../language/options.md) — the `VirtualFunction` key set
- [Inputs / Outputs / Results](../language/inputs-outputs.md) — the parameter sections a function declares
- [Decompiler](../tools/decompiler.md) — the material-function round-trip gap
- [Regeneration](../generation/regeneration.md) — what a rebuild resets
- [Diagnostics index](../diagnostics/index.md) — every code
