# Caching

> [DreamShader](../index.md) » [Generation](index.md) » **Caching**

The source-hash short circuit: a stored fingerprint of the compiled text that lets an unchanged
source skip regeneration entirely.

| | |
| :-- | :-- |
| Kind | generation optimization |
| Hashed text | the **prepared** source — the file with every `import` recursively inlined |
| Algorithm | `FCrc::StrCrc32`, formatted `%08x` — eight lowercase hex digits |
| Stored in | the generated asset's package metadata, keyed by the asset object |
| Bypassed by | the `bForce` flag on the generation entry points |

## Synopsis

```text
prepared text  ->  CRC32  ->  "%08x"  ->  DreamShader.SourceHash   e.g. "9f2c41ab"
source path    ->  project-relative, forward slashes  ->  DreamShader.SourceFile
```

## What is hashed

The hash covers the text the parser actually sees, **after** import inlining — not the bytes of the
file on disk. That text is the concatenation of the file and its whole transitive `import` closure,
with each import line blanked out and each included file bracketed by
`// Begin DreamShader source: <path>` / `// End DreamShader source: <path>` markers.

| Change | Changes the hash of |
| :-- | :-- |
| edit `M_Foo.dsm` | `M_Foo.dsm` |
| edit `Common.dsh`, imported by `M_Foo.dsm` and `M_Bar.dsm` | both `M_Foo.dsm` and `M_Bar.dsm` |
| move the project to another directory | nothing — the stored path is project-relative |
| rename the source file | the stored path no longer matches, so nothing is skipped |
| reformat whitespace or edit a comment | the hash — the text is compared byte for byte, not semantically |

> [!NOTE]
> Editing a `.dsh` invalidates every dependent `.dsm` and `.dsf`, but a header never generates
> anything by itself — saving it fails with `DreamShader header '{File}' does not generate assets
> directly. Recompile dependent .dsm or .dsf files instead.` The dependents are only rebuilt when
> they are themselves compiled: on their own save, or through *Generate all in-memory materials*
> (editor startup and every change to the **Default Compiler Backend** setting), the Material
> Content Browser's Compile button, the [commandlet](../tools/commandlet.md), or a cook.

## Where the metadata lives

Two keys are written into the generated asset's **package metadata**, keyed by the asset object.

| Key | Value |
| :-- | :-- |
| `DreamShader.SourceFile` | the source path made relative to the project directory, with forward slashes. A source outside the project keeps its absolute path. |
| `DreamShader.SourceHash` | the eight-hex-digit CRC32. Written only when non-empty. |

Storing the *project-relative* path is deliberate: a checkout on another machine, or a moved project
directory, still recognizes its own generated assets instead of regenerating everything.

Which assets get stamped, and when:

| Asset | Stamped |
| :-- | :-- |
| `UDreamShaderMaterialInstance` (ThinCustom) | **always** — memory-only and persisted alike |
| the hidden `MB_DreamThinBase_*` base | persist mode only |
| `UMaterial` (Graph backend) | persist mode only |
| `UMaterialFunction` / layer / layer blend | persist mode only |

A ThinCustom instance additionally carries the source path and the hash as read-only `UPROPERTY`s —
`SourceFilePath` and `SourceHash`, category `DreamShader` — so they are visible in the details panel
without inspecting package metadata. `SourceFilePath` holds the **full normalized** source path, not
the project-relative form the metadata stores; `SourceHash` is the same eight-hex-digit value. See
[`UDreamShaderMaterialInstance`](../api/material-instance.md).

> [!NOTE]
> `DreamShader.SourceFile` doubles as the **ownership marker**. Its presence is what tells
> DreamShader that an asset is its own to overwrite, and what *Clean Persisted Generated Assets*
> filters on. See [Regeneration](regeneration.md#ownership-guard).

## When regeneration is skipped

The short circuit fires only when **all** of the following hold:

| # | Condition |
| :-- | :-- |
| 1 | the generation call did not set `bForce` |
| 2 | the asset exists and the newly computed hash is non-empty |
| 3 | the stored `DreamShader.SourceFile` is present and non-empty |
| 4 | the stored source path equals the project-relative path of the source being compiled, **ignoring case** |
| 5 | the stored `DreamShader.SourceHash` equals the new hash, **case-sensitively** |

Per asset kind:

| Asset | Skip point | Extra condition | Message |
| :-- | :-- | :-- | :-- |
| ThinCustom material | after the instance is created or reused, **before** the hidden base is created | — | `Skipped {AssetPath} from {File}; source hash is unchanged.` |
| `Graph`-backend material | after the material is created or reused | — | `Skipped {AssetPath} from {File}; source hash is unchanged.` |
| Material function | after the function asset is created or reused | the asset's material-function usage must already match the one the block requires | *silent* — the asset path is returned with no message |

Placing the ThinCustom check before the base is created is what makes a skip cheap: no base
material, no ownership check, no graph teardown.

> [!NOTE]
> A material function whose usage does not match — a `ShaderLayer` block whose asset is still marked
> `Default`, for instance — is regenerated even when the hash matches, and the usage is corrected.

## Forcing regeneration

| Path | Force |
| :-- | :-- |
| Auto-compile on save | **no** — the hash short circuit is active |
| *Generate all in-memory materials* (startup, backend-setting change) | yes |
| Material Content Browser Compile / thumbnail refresh | yes |
| Live preview render | yes |
| *Materialize*, and child-instance creation | yes |
| Cook | yes |
| Commandlet `-run=DreamShader` | only with [`-Force`](../tools/commandlet.md#compile--generate); otherwise it reports `Skipped {AssetPath} from {SourceFile}; source hash is unchanged.` |

There is no way to clear the stored hash from the source language. To force a rebuild without a
force-capable entry point, either change the source text (any change, including whitespace), or
delete the generated asset.

## Notes

- The hash is a CRC32, not a cryptographic digest. It detects edits; it is not a security or
  integrity mechanism.
- The generated `.ush` helper include is **not** covered by this short circuit. It is rewritten on
  every compile of a unit that declares `Function` blocks, and its file name embeds a hash of the
  *source path*, not of the source text. See [Generated HLSL](generated-hlsl.md).
- The comparison is per asset. One source file that declares a material and three functions stores
  the same hash on four assets, and each is skipped independently.
- On UE 5.6 and newer the package metadata is accessed through the engine's value-typed metadata
  API; earlier engines use the object-typed one. The stored keys and values are identical.
- Nothing writes a generation timestamp. `DreamShader.SourceFile` and `DreamShader.SourceHash` are
  the only two keys DreamShader ever sets.

## Diagnostics

Runtime substitutions are rendered as `{Placeholder}`.

| Message | Cause |
| :-- | :-- |
| `Skipped {AssetPath} from {File}; source hash is unchanged.` | the short circuit fired for a material |
| `DreamShader header '{File}' does not generate assets directly. Recompile dependent .dsm or .dsf files instead.` | a `.dsh` was compiled directly |

## Example

```c
// DShader/Common.dsh
Function float Remap01(in float value) { return saturate(value * 0.5 + 0.5); }
```

```c
// DShader/M_Ramp.dsm
import "Common.dsh";

Shader(Name="Materials/M_Ramp")
{
    Properties { ScalarParameter Input = 0.25; }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { float R = Remap01(Input); Color = vec3(R, R, R); }
}
```

Observed sequence:

```text
save M_Ramp.dsm      Generated DreamShader thin-custom material /Game/Materials/M_Ramp from ...M_Ramp.dsm.
save M_Ramp.dsm      Skipped /Game/Materials/M_Ramp from ...M_Ramp.dsm; source hash is unchanged.
edit Common.dsh      (saving the header itself generates nothing)
save M_Ramp.dsm      Generated DreamShader thin-custom material /Game/Materials/M_Ramp from ...M_Ramp.dsm.
```

Metadata on the generated instance:

```text
DreamShader.SourceFile   DShader/M_Ramp.dsm
DreamShader.SourceHash   9f2c41ab
```

## See also

- [Generation](index.md) — where the hash is computed in the pipeline
- [import](../language/import.md) — how the prepared text is assembled
- [Regeneration](regeneration.md) — the ownership guard built on `DreamShader.SourceFile`
- [In-memory materials](in-memory.md) — which assets are stamped in which mode
- [`UDreamShaderMaterialInstance`](../api/material-instance.md) — `SourceFilePath` and `SourceHash`
- [Generated HLSL](generated-hlsl.md) — the include's separate, path-based hash
- [Commandlet](../tools/commandlet.md) — headless compiles and forcing
- [Project settings](../settings/project.md) — auto-compile and debounce
