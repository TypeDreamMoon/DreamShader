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
`Backend` is a special key: it is read when the source is bound and never reaches the material or the
reflection resolver. The 2.0 spelling is `#pragma material(Backend = ThinCustom)`.

## Accepted values

| Value | Resolves to | Produces |
| :-- | :-- | :-- |
| `Graph` | `Graph` | one `UMaterial` at the resolved asset path, with the node graph built directly on it |
| `ThinCustom` | `ThinCustom` | a `UDreamShaderMaterialInstance` at the resolved asset path, parented to a hidden `UMaterial` named `MB_DreamThinBase_…` — the suffix differs between the Ephemeral and Materialized states, see [`ThinCustom`](#thincustom-since-150) |
| `Instance` | `ThinCustom` | identical to `ThinCustom`, with a [`DSH7204`](../diagnostics/DSH7xxx.md#dsh7204) warning *(since 2.0.0)* |
| *(empty string)* | — | [`DSH7201`](../diagnostics/DSH7xxx.md#dsh7201), an error *(since 2.0.0; 1.x read it as `Graph`)* |
| *(key absent)* | the project's *Default Compiler Backend* | see [Precedence](#precedence) |
| anything else | — | [`DSH7202`](../diagnostics/DSH7xxx.md#dsh7202), an error |

> [!WARNING]
> `Backend = "Instance"` is **deprecated since 1.5.0**. It is an alias for `ThinCustom`; the legacy
> graphless instance backend is retired and there is no runtime `Instance` backend left. The spelling
> still compiles, and since 2.0.0 it says so with a `DSH7204` warning. Replace it with
> `Backend = "ThinCustom";` — or delete the key and let the project default apply.

> [!NOTE]
> `Backend = "";` is an error *(since 2.0.0)*. 1.x read it as **`Graph`**, which silently overrode the
> project default with a value nobody wrote. Only *omitting* the key falls back to *Default Compiler
> Backend*; write `Graph` if that is what you mean.

## Precedence

| `Settings = { Backend }` | Project *Default Compiler Backend* | Resolved backend |
| :-- | :-- | :-- |
| absent | `ThinCustom` (the shipped default) | `ThinCustom` |
| absent | `Instance` | `ThinCustom` |
| absent | `Graph` | `Graph` |
| `"Graph"` | any | `Graph` |
| `"ThinCustom"` | any | `ThinCustom` |
| `"Instance"` | any | `ThinCustom` (`DSH7204`) |
| `""` | any | — (`DSH7201`) |

An explicit `Backend` always wins over the project setting. The project setting is described in
[Project settings](project.md#settings).

`Backend` is resolved when the source is bound, before any material object exists and before the rest
of `Settings` is applied. An unrecognized value is a binding error, so the compile stops there and no
settings diagnostic of the build is reported for that file.

## What each backend produces

### `Graph`

| Aspect | Behaviour |
| :-- | :-- |
| Asset written | a `UMaterial` at the package path derived from `Name` and `Root` |
| Graph | built directly on the material, then laid out and recompiled |
| Storage | a `Graph` material has no Ephemeral state: the editor that owns writing this project's generated assets marks the package dirty, stamps the source metadata and saves it on every build. Any other editor builds it in memory and clears its dirty flag, so a Save All cannot persist it |
| Reuse conflict | [`DSH8201`](../diagnostics/DSH8xxx.md#dsh8201), quoting [`DSH8102`](../diagnostics/DSH8xxx.md#dsh8102) (the path holds another class) or [`DSH8103`](../diagnostics/DSH8xxx.md#dsh8103) (a saved asset DreamShader did not generate) |

### `ThinCustom` *(since 1.5.0)*

| Aspect | Behaviour |
| :-- | :-- |
| Asset written | a `UDreamShaderMaterialInstance` — a `UMaterialInstanceConstant` subclass — at the resolved asset path. **This instance is the addressable asset.** |
| Hidden base name, the Ephemeral state | `MB_DreamThinBase_<sanitized package path>` *(since 2.0.0)* — the instance's whole package path, with every character outside `[A-Za-z0-9_]` replaced by `_` and runs of `_` collapsed. The package `/Game/Docs/M_Tint` gives `MB_DreamThinBase__Game_Docs_M_Tint`, so two instances that share a leaf name in different folders never share a base |
| Hidden base name, persist mode | `MB_DreamThinBase_<instance leaf name>` — the instance's own object name, with no path component. `Name="Docs/M_Tint"` gives `MB_DreamThinBase_M_Tint` |
| Base ownership, the Ephemeral state | owned by the transient package, flagged public, standalone and transient |
| Base ownership, persist mode | a **subobject of the instance**, so the pair shares one package and one `.uasset` |
| Graph and settings | built on the **base**; every `Settings` key lands there, not on the instance |
| Instance wiring | parent set to the base, parameter overrides cleared and the ones set on the instance restored by name, source path and hash stamped, static permutation updated |
| Reuse conflict | [`DSH8203`](../diagnostics/DSH8xxx.md#dsh8203), quoting [`DSH8106`](../diagnostics/DSH8xxx.md#dsh8106) (the path holds something that is not a DreamShader instance material) or [`DSH8107`](../diagnostics/DSH8xxx.md#dsh8107) (a saved asset DreamShader did not generate) |

`UDreamShaderMaterialInstance` overrides two `UObject`/`UMaterialInterface` behaviours:

| Override | Rule |
| :-- | :-- |
| `HasOverridenBaseProperties()` | forced `true` when the parent is a `UMaterial` — that is, for the root instance over the hidden base; any other parent falls through to the stock implementation. A child instance parented to a DreamShader instance therefore **shares** the root's compiled shader map instead of compiling its own. |
| `IsAsset()` | `false` while the package is newly created **and** *Show Ephemeral Materials* is off. Memory-only materials are hidden from the Content Browser, asset-registry enumeration and save pickers. The setting is read live, on every call. |

A build that finds a saved asset where an Ephemeral one was asked for logs a warning:
`'{Asset}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run
Tools > DreamShader > Make Ephemeral to make it Ephemeral again.` See
[Ephemeral materials](../generation/ephemeral.md).

## Changing the project default

Changing *Default Compiler Backend* while the editor is running rebuilds the project's sources and
logs `DreamShader setting '{Setting}' changed; regenerating all source files.` A source the change
does not affect is skipped by its build key. If persisted generated assets exist, a notification
points at the cleanup action: `{Count} previously generated asset(s) are still saved on disk and
shadow the Ephemeral materials. Run Tools > DreamShader > Make Ephemeral to remove them.`

Switching an individual material between backends leaves the previous asset behind. The reuse-conflict
errors above are what you hit next; delete the stale asset and regenerate.

## Notes

- The backend does not change the language surface. Both backends build the same node graph and
  accept the identical feature set; they differ only in which object carries the graph and which
  object is addressable.
- Under `ThinCustom`, reading `BlendMode` or the shading model off the generated instance shows the
  value **inherited** from the hidden base — that is where the settings were written.
- A `ThinCustom` product is Ephemeral during ordinary editor work: it reaches disk at cook time,
  through the [commandlet](../tools/commandlet.md), or through an explicit *Materialize* action. A
  `Graph` material is saved when it is built. See [Ephemeral materials](../generation/ephemeral.md).
- `Backend` is honoured only in a `Shader` block. In a `ShaderFunction` `Settings` block it is a
  [`DSH3263`](../diagnostics/DSH3xxx.md#dsh3263) warning and is ignored — see
  [Function settings](function.md).

## Diagnostics

| Code | Raised when |
| :-- | :-- |
| `DSH7201` | `Backend` has an empty value |
| `DSH7202` | the value is not `Graph`, `ThinCustom` or `Instance` |
| `DSH7204` | the value is `Instance` (warning; builds as `ThinCustom`) |
| `DSH8201` | `Graph` backend: the material could not be created or reused; the message quotes `DSH8102` or `DSH8103` |
| `DSH8203` | `ThinCustom` backend: the instance could not be created or reused; the message quotes `DSH8106` or `DSH8107` |
| [`DSH8230`](../diagnostics/DSH8xxx.md#dsh8230) | the hidden base could not be created |
| [`DSH8229`](../diagnostics/DSH8xxx.md#dsh8229) | the asset was built but could not be saved |

Informational output:

| Output | Meaning |
| :-- | :-- |
| `Generated {Kind} {Asset} from {File}.` | a product was built; `{Kind}` is `Material` for a `Shader` under either backend |
| `Skipped {Asset} from {File}; source hash is unchanged (build key {BuildKey}).` | the [source-hash cache](../generation/caching.md) suppressed the rebuild ([`DSH8237`](../diagnostics/DSH8xxx.md#dsh8237), info) |
| `'{Asset}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run Tools > DreamShader > Make Ephemeral to make it Ephemeral again.` | the product is Materialized, so storage decides |

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
object, but the base is a separate transient-package object named
`MB_DreamThinBase__Game_Docs_M_ThinCustom` — the sanitized package path, not the leaf.

The same file with `Backend = "Graph";` instead produces a single `UMaterial` at
`/Game/Docs/M_ThinCustom` carrying the graph and the settings itself.

## See also

- [Settings](index.md) — the block grammar shared by every block kind
- [Shader settings](material.md) — the other special keys and the reflection resolver
- [Project settings](project.md) — *Default Compiler Backend* and the Ephemeral visibility toggle
- [Ephemeral materials](../generation/ephemeral.md) — memory-only generation, the hidden base, materializing to disk
- [Generation pipeline](../generation/index.md) — where backend resolution sits in the pipeline
- [Asset paths](../generation/asset-paths.md) — how `Name` and `Root` become a package path
- [Caching](../generation/caching.md) — the source-hash skip
- [Material instance API](../api/material-instance.md) — `UDreamShaderMaterialInstance` in C++
- [Shader](../language/shader.md) — the enclosing block
- [Material Content Browser](../tools/material-browser.md) — materializing and inspecting generated assets
- [Diagnostics index](../diagnostics/index.md) — every code
