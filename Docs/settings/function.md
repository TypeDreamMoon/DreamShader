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
deprecated `MaterialLayer` / `MaterialLayerBlend` spellings.

## Keys

Keys are matched after trimming and lowercasing, so `description`, `Description` and `DESCRIPTION` are
the same key. Spaces, underscores and hyphens are **not** folded here — `Expose_To_Library` is not
`ExposeToLibrary`.

| Key | Sets | Value grammar | Value when the key is absent |
| :-- | :-- | :-- | :-- |
| **`Description`** | `UMaterialFunction::Description` | free text | cleared to the empty string |
| **`UserExposedCaption`** | `UMaterialFunction::UserExposedCaption` | free text | cleared to the empty string |
| **`ExposeToLibrary`** | `UMaterialFunction::bExposeToLibrary` | `true` / `false`, case-insensitive | `false` |
| **`LibraryCategories`** | `UMaterialFunction::LibraryCategoriesText` | a comma-separated list; each entry is trimmed and empty entries are dropped | the category list is cleared |

`LibraryCategories` clears the list before parsing, so the setting always replaces the asset's
categories rather than appending to them. `LibraryCategories = "A,,  B ,";` yields exactly `A` and
`B`.

Quotes are optional on every value, as everywhere in a `Settings` block:
`Description = Tint helper;` and `Description = "Tint helper";` are identical.

> [!NOTE]
> **Every key here is reset when the key is absent.** Omitting `ExposeToLibrary` sets it to `false`;
> omitting `Description` empties it. A property edited by hand on the generated asset is discarded on
> the next regeneration. There is no "leave it alone" state — see
> [Regeneration](../generation/regeneration.md).

> [!WARNING]
> **Any other key is ignored, silently.** There is no validation pass over a material function's
> `Settings` map: only these four names are looked up, and everything else is dropped with no error
> and no warning. In particular `Backend`, `Domain`, `ShadingModel`, `BlendMode`, `TwoSided` and
> every other [`Shader` setting](material.md) do **nothing** in a `ShaderFunction` block. Misspelling
> one of the four keys — `Descriptions`, `Expose_To_Library`, `LibraryCategory` — is equally silent.

> [!WARNING]
> The [decompiler](../tools/decompiler.md) does not emit a `Settings` block when exporting a
> `UMaterialFunction` to `.dsf`. `Description`, `UserExposedCaption`, `ExposeToLibrary` and
> `LibraryCategories` are lost on that round trip and must be re-added by hand.

## Related validation

These checks run in the same pass and are the diagnostics a `Settings` mistake is most often confused
with. `{Kind}` is `ShaderFunction`, `ShaderLayer` or `ShaderLayerBlend` depending on the block.

| Rule | Applies to |
| :-- | :-- |
| At least one output must be declared | all three block kinds |
| A `Graph` (or legacy `Code`) body must be present | all three block kinds |
| Property and input names must be unique, ignoring case | all three block kinds |
| Exactly one `MaterialAttributes` output | `ShaderLayer`, `ShaderLayerBlend` |
| At most one `MaterialAttributes` input | `ShaderLayer` |
| Exactly two `MaterialAttributes` inputs | `ShaderLayerBlend` |

The generated asset's usage follows the block kind: `ShaderFunction` produces an ordinary material
function, `ShaderLayer` a material-layer function, `ShaderLayerBlend` a layer-blend function.

## Notes

- A [`VirtualFunction`](../language/virtual-function.md) block also accepts a section named
  `Settings`, but it is a synonym for `Options` and takes an entirely different key set — `Asset` and
  `Description`. See [Options](../language/options.md).
- [`Function`](../language/function.md) and [`GraphFunction`](../language/graph-function.md) blocks
  have no `Settings` section at all; their only sections are `Inputs` / `Properties`,
  `Outputs` / `Results` and `Code` / `Graph`.
- Multiple `Settings` sections in one block merge into a single map, last key wins, exactly as in a
  `Shader` block. See [Settings](index.md#how-a-statement-is-parsed).
- `ExposeToLibrary = true;` is what makes the function appear in the Material palette's function
  library; `LibraryCategories` decides where in that palette it sits.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}`.

| Message | Cause |
| :-- | :-- |
| `{Kind} '{Name}': ExposeToLibrary must be true or false.` | the `ExposeToLibrary` value is not a boolean literal |
| `{Kind} '{Name}' must declare at least one output.` | the block declares no `Outputs` |
| `{Kind} '{Name}' must provide a Graph block.` | the block has no graph body |
| `{Kind} '{Name}' property '{Property}' conflicts with another property or input name.` | a `Properties` entry collides with another property or an input, ignoring case |
| `Unknown material function section '{Section}'.` | a section name other than the recognized ones |

`Description`, `UserExposedCaption` and `LibraryCategories` have no failure mode: any text is
accepted.

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
  UserExposedCaption    = "Tint"
  bExposeToLibrary      = true
  LibraryCategoriesText = ["DreamShader", "Color"]
```

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
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
