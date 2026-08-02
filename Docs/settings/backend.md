# Backend

> [DreamShader](../index.md) » [Settings](index.md) » **Backend**

The `Shader` setting that selects how a source file is materialized: a visible `UMaterial` node
graph, or a thin material instance over a hidden base material.

| | |
| :-- | :-- |
| Declared in | `.dsm` — inside a `Shader` `Settings` block |
| Kind | special settings key |
| Generates | `UMaterial` (`Graph`) or `UDreamShaderMaterialInstance` + a hidden `UMaterial` base (`ThinCustom`) |
| Since | `1.5.0` in its current form |

## Synopsis

```c
Shader(Name = "<asset-path>")
{
    Settings [=] {
        Backend = { "Graph" | "ThinCustom" | "Instance" };
    }
}
```

The value is matched case-insensitively after trimming and quote-stripping; quotes are optional.
`Backend` is a special key and never reaches the reflection resolver.

## Accepted values

| Value | Resolves to | Produces |
| :-- | :-- | :-- |
| `Graph` | `Graph` | one `UMaterial` at the resolved asset path, with the node graph built directly on it |
| `ThinCustom` | `ThinCustom` | a `UDreamShaderMaterialInstance` at the resolved asset path, parented to a hidden `UMaterial` named `MB_DreamThinBase_…` — the suffix differs between in-memory and persist mode, see [`ThinCustom`](#thincustom-since-150) |
| `Instance` | `ThinCustom` | identical to `ThinCustom` *(deprecated in 1.5.0)* |
| *(empty string)* | `Graph` | as `Graph` — **not** the project default |
| *(key absent)* | the project's *Default Compiler Backend* | see [Precedence](#precedence) |
| anything else | — | hard error, `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` |

> [!WARNING]
> `Backend = "Instance"` is **deprecated since 1.5.0**. It is an alias for `ThinCustom`; the legacy
> graphless instance backend is retired and there is no runtime `Instance` backend left. The spelling
> is retained for one deprecation window so existing sources keep compiling, and it produces **no**
> diagnostic. Replace it with `Backend = "ThinCustom";` — or delete the key and let the project
> default apply.

> [!NOTE]
> `Backend = "";` resolves to **`Graph`**, not to the project default. Only *omitting* the key falls
> back to *Default Compiler Backend*. An empty value is otherwise indistinguishable from a typo, so
> prefer removing the whole statement.

## Precedence

| `Settings = { Backend }` | Project *Default Compiler Backend* | Resolved backend |
| :-- | :-- | :-- |
| absent | `ThinCustom` (the shipped default) | `ThinCustom` |
| absent | `Instance` | `ThinCustom` |
| absent | `Graph` | `Graph` |
| `"Graph"` | any | `Graph` |
| `"ThinCustom"` | any | `ThinCustom` |
| `"Instance"` | any | `ThinCustom` |
| `""` | any | `Graph` |

An explicit `Backend` always wins over the project setting. The project setting is described in
[Project settings](project.md#settings).

`Backend` is resolved **before** any material object exists and before the rest of `Settings` is
validated, so an unrecognized value fails the compile first and no other settings diagnostic is
reported for that file.

## What each backend produces

### `Graph`

| Aspect | Behaviour |
| :-- | :-- |
| Asset written | a `UMaterial` at the package path derived from `Name` and `Root` |
| Graph | built directly on the material, then laid out and recompiled |
| In-memory mode | the package is flagged newly-created and its dirty flag is cleared afterwards, so a Save All cannot silently persist it |
| Persist mode | the package is marked dirty, source metadata is stamped, and the package is saved |
| Reuse conflict | `Asset '{ObjectPath}' already exists and is not a Material.` |

### `ThinCustom` *(since 1.5.0)*

| Aspect | Behaviour |
| :-- | :-- |
| Asset written | a `UDreamShaderMaterialInstance` — a `UMaterialInstanceConstant` subclass — at the resolved asset path. **This instance is the addressable asset.** |
| Hidden base name, in-memory mode | `MB_DreamThinBase_<sanitized Name>` — the whole `Shader` `Name`, with every character outside `[A-Za-z0-9_]` replaced by `_`. `Name="Docs/M_Tint"` gives `MB_DreamThinBase_Docs_M_Tint` |
| Hidden base name, persist mode | `MB_DreamThinBase_<instance leaf name>` — the instance's own object name, with no path component. `Name="Docs/M_Tint"` gives `MB_DreamThinBase_M_Tint` |
| Base ownership, in-memory mode | owned by the transient package, flagged public, standalone and transient |
| Base ownership, persist mode | a **subobject of the instance**, so the pair shares one package and one `.uasset` |
| Graph and settings | built on the **base**; every `Settings` key lands there, not on the instance |
| Instance wiring | parent set to the base, parameter overrides cleared, source path and hash stamped, static permutation updated |
| Reuse conflict | `Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.` |

`UDreamShaderMaterialInstance` overrides two `UObject`/`UMaterialInterface` behaviours:

| Override | Rule |
| :-- | :-- |
| `HasOverridenBaseProperties()` | forced `true` when the parent is a `UMaterial` — that is, for the root instance over the hidden base; any other parent falls through to the stock implementation. A child instance parented to a DreamShader instance therefore **shares** the root's compiled shader map instead of compiling its own. |
| `IsAsset()` | `false` while the package is newly created **and** *Show In-Memory Materials In Content Browser* is off. Memory-only materials are hidden from the Content Browser, asset-registry enumeration and save pickers. The setting is read live, on every call. |

Both backends log a warning when in-memory generation is shadowed by an already-saved asset:
`In-memory material mode: '{Asset}' already exists as a saved asset, which shadows in-memory
regeneration. Delete the saved asset to make it fully in-memory.` See
[In-memory materials](../generation/in-memory.md).

## Changing the project default

Changing *Default Compiler Backend* while the editor is running regenerates every source file in
memory and logs `DreamShader default compiler backend changed; regenerating all source files in
memory.` If persisted generated assets exist, a notification points at the cleanup action:
`{Count} previously generated asset(s) are still saved on disk and shadow the in-memory materials.
Run Tools > DreamShader > Clean Persisted Generated Assets to remove them.`

Switching an individual material between backends leaves the previous asset behind. The reuse-conflict
errors above are what you hit next; delete the stale asset and regenerate.

## Notes

- The backend does not change the language surface. Both backends build a real node graph and accept
  the identical feature set; they differ only in which object carries the graph and which object is
  addressable.
- Under `ThinCustom`, reading `BlendMode` or the shading model off the generated instance shows the
  value **inherited** from the hidden base — that is where the settings were written.
- Neither backend writes a `.uasset` during ordinary editor work. Assets reach disk at cook time,
  through the [commandlet](../tools/commandlet.md), or through an explicit *Materialize* action. See
  [In-memory materials](../generation/in-memory.md).
- `Backend` is honoured only in a `Shader` block. In a `ShaderFunction` `Settings` block it is one of
  the keys that is [silently ignored](function.md).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}` throughout this page.

| Message | Cause |
| :-- | :-- |
| `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` | the value is not `Graph`, `ThinCustom`, `Instance` or empty |
| `Asset '{ObjectPath}' already exists and is not a Material.` | `Graph` backend, the target path holds another UClass |
| `Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.` | `ThinCustom` backend, the target path holds a non-DreamShader object |
| `Failed to create ThinCustom base material for '{Name}'.` | the hidden base could not be created |
| `Cannot create a persisted ThinCustom base without an instance for '{Name}'.` | persist mode reached base creation with no instance |
| `Failed to create ThinCustom base material for instance '{Name}'.` | the base subobject could not be created under the instance |

Informational messages:

| Message | Meaning |
| :-- | :-- |
| `Generated {Asset} from {File}.{Virtual}` | `Graph` backend success; `{Virtual}` is ` (virtual)` for in-memory generation and empty otherwise |
| `Generated DreamShader thin-custom material {Asset} from {File}.` | `ThinCustom` backend success |
| `Skipped {Asset} from {File}; source hash is unchanged.` | the [source-hash cache](../generation/caching.md) suppressed the rebuild |
| `In-memory material mode: '{Asset}' already exists as a saved asset, which shadows in-memory regeneration. Delete the saved asset to make it fully in-memory.` | a saved asset shadows the memory-only one |

## Example

```c
Shader(Name="Docs/M_ThinCustom", Root="Game")
{
    Properties {
        VectorParameter Tint = float4(0.2, 0.6, 1.0, 1.0) [Group="Look"];
    }

    Settings {
        Backend      = "ThinCustom";
        Domain       = "Surface";
        ShadingModel = "DefaultLit";
        BlendMode    = "Opaque";
        TwoSided     = true;
    }

    Outputs {
        vec3 Color;
        Base.BaseColor = Color;
    }

    Graph {
        Color = Tint.rgb;
    }
}
```

Generated assets:

```text
package     /Game/Docs/M_ThinCustom
asset       /Game/Docs/M_ThinCustom.M_ThinCustom      UDreamShaderMaterialInstance
subobject   MB_DreamThinBase_M_ThinCustom             UMaterial  (hidden base, same package)

Settings applied to:  MB_DreamThinBase_M_ThinCustom
  BlendMode    = BLEND_Opaque
  ShadingModel = MSM_DefaultLit
  TwoSided     = true
```

That listing is persist mode. In the editor's ordinary memory-only mode the instance is the same
object, but the base is a separate transient-package object named `MB_DreamThinBase_Docs_M_ThinCustom`
— the sanitized `Name`, not the leaf.

The same file with `Backend = "Graph";` instead produces a single `UMaterial` at
`/Game/Docs/M_ThinCustom` carrying the graph and the settings itself.

## See also

- [Settings](index.md) — the block grammar shared by every block kind
- [Shader settings](material.md) — the other special keys and the reflection resolver
- [Project settings](project.md) — *Default Compiler Backend* and the in-memory visibility toggle
- [In-memory materials](../generation/in-memory.md) — memory-only generation, the hidden base, materializing to disk
- [Generation pipeline](../generation/index.md) — where backend resolution sits in the pipeline
- [Asset paths](../generation/asset-paths.md) — how `Name` and `Root` become a package path
- [Caching](../generation/caching.md) — the source-hash skip
- [Material instance API](../api/material-instance.md) — `UDreamShaderMaterialInstance` in C++
- [Shader](../language/shader.md) — the enclosing block
- [Material Content Browser](../tools/material-browser.md) — materializing and inspecting generated assets
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
