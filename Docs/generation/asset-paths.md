# Asset paths

> [DreamShader](../index.md) » [Generation](index.md) » **Asset paths**

The rule that turns a block's `Name` and `Root` header attributes into an Unreal package path, an
object path, and a file on disk.

| | |
| :-- | :-- |
| Applies to | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` — one resolver, identical for all four |
| Kind | header attributes |
| Since | `Root="Plugin.X"` — `1.2.0`; the plugin-source-root default — `1.6.0` |

## Synopsis

```c
Shader        (Name = "<name-path>" [, Root = "<root>"]) { … }
ShaderFunction(Name = "<name-path>" [, Root = "<root>"]) { … }

<name-path> := [<folder> /] … <leaf>
<root>      := "" | Game | Plugin.<PluginName> | Plugins.<PluginName>
             | Plugin / <PluginName> | Plugins / <PluginName>
             | / <MountRoot> [/ <folder>] …
             | <folder> [/ <folder>] …
```

`Name` is **required**: a block without one, or with one that is empty, is
[`DSH2242`](../diagnostics/DSH2xxx.md#dsh2242). `Root` is optional and defaults to the empty string.
Attribute keys are matched case-insensitively, so `name=` and `ROOT=` both resolve.

*(since 2.0.0)* A 1.x block keeps exactly this destination (legacy rule L10): its `Name=` is never
placed under the source file's folder, as the name of a `.dss` product is. A destination that does not
resolve fails the product with [`DSH8200`](../diagnostics/DSH8xxx.md#dsh8200), whose message quotes the
asset layer's code and text — see [Diagnostics](#diagnostics).

## Resolution order

1. **`Root` → root package path.** Trim; `\` → `/`; remember whether the string began with `/`; strip
   leading and trailing `/`; split on `/`. An empty result at any point yields `/Game`.
2. **`Name` → folders + leaf.** Trim; `\` → `/`; strip all leading and trailing `/`; split on `/`.
   The **last** segment is the asset name; every preceding segment is a folder appended to the root.
3. Every segment passes through `ObjectTools::SanitizeObjectName`. Empty segments are skipped
   silently.
4. `PackageName = <root>/<folders…>/<Leaf>` and `ObjectPath = <PackageName>.<Leaf>`.
5. `PackageName` is validated with `FPackageName::IsValidObjectPath`.

## `Root=` dispatch

Dispatch is on the **first** segment, compared case-insensitively.

| First segment | Extra condition | Root package | Folders taken from segment |
| :-- | :-- | :-- | :-- |
| *(absent, empty, `/`, or all-whitespace)* | — | `/Game` | — |
| `Game` | — | `/Game` | 1 |
| `Plugin.<Name>` | — | the plugin's mount point | 1 |
| `Plugins.<Name>` | — | the plugin's mount point | 1 |
| `Plugin` | a second segment exists | the plugin named by segment 1 | 2 |
| `Plugins` | a second segment exists | the plugin named by segment 1 | 2 |
| anything else | the `Root` string began with `/` | `/<segment 0>`, verbatim | 1 |
| anything else | no leading `/` | `/Game`, and segment 0 becomes a folder | 0 |

### Branch — omitted or empty

| `Root` | `Name` | Package | Object path |
| :-- | :-- | :-- | :-- |
| *(omitted)* | `M_Flat` | `/Game/M_Flat` | `/Game/M_Flat.M_Flat` |
| `""` | `Materials/M_Flat` | `/Game/Materials/M_Flat` | `/Game/Materials/M_Flat.M_Flat` |
| `"/"` | `M_Flat` | `/Game/M_Flat` | `/Game/M_Flat.M_Flat` |

#### The plugin source root default

The table above is the **project root's** answer. A source file that lives under a plugin source
root — `<Plugin>/DShader`, see [Source files](../language/source-files.md#source-roots) — defaults
instead to that plugin's mount point, exactly as if it had written `Root="Plugin.<PluginName>"`.

| Source file | `Root` | `Name` | Package |
| :-- | :-- | :-- | :-- |
| `<Project>/DShader/M.dsm` | *(omitted)* | `M_Flat` | `/Game/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | *(omitted)* | `M_Flat` | `/MoonToon/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | `"/"` | `M_Flat` | `/Game/M_Flat` |
| `Plugins/MoonToon/DShader/M.dsm` | `Game/Legacy` | `M_Flat` | `/Game/Legacy/M_Flat` |

Only an **absent or whitespace-only** `Root` is defaulted, so `Root="/"` (or any explicit spelling)
is how a plugin-root file opts back into `/Game`. The default is applied per block: a file may leave
its `Shader` block to the default and pin one `ShaderFunction` to `/Game`.

> [!NOTE]
> The default is skipped, and the block falls back to `/Game`, when the owning plugin cannot host
> generated content — it is not a project plugin under `<Project>/Plugins`, is disabled, declares no
> `CanContainContent`, has no `Content` directory, or is unmounted. *(since 2.0.0)* The fallback is
> silent: nothing is logged. An unresolvable inferred `Root` is never emitted as an error, because
> the author never wrote one.

The default is applied in the one place a product becomes an asset path, so the compile, product
resolution (the Material Content Browser, `dsc list-generated`) and the preview renderer all name the
same asset.

### Branch — `Game`

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Game` | `M_Flat` | `/Game/M_Flat` |
| `/Game` | `Materials/M_Flat` | `/Game/Materials/M_Flat` |
| `Game/Materials` | `M_Flat` | `/Game/Materials/M_Flat` |
| `game/materials` | `M_Flat` | `/Game/Materials/M_Flat` |

The comparison is case-insensitive, but the emitted root is always the literal `/Game`.

### Branch — `Plugin.<Name>` / `Plugins.<Name>`

The dotted form names the plugin in a single segment; everything after it is folders.

| `Root` | `Name` | Package | On disk |
| :-- | :-- | :-- | :-- |
| `Plugin.MoonToon` | `Mat/Test` | `/MoonToon/Mat/Test` | `<Project>/Plugins/MoonToon/Content/Mat/Test.uasset` |
| `Plugins.MoonToon` | `Test` | `/MoonToon/Test` | `<Project>/Plugins/MoonToon/Content/Test.uasset` |
| `Plugin.MoonToon/Shared` | `Test` | `/MoonToon/Shared/Test` | `<Project>/Plugins/MoonToon/Content/Shared/Test.uasset` |

### Branch — `Plugin/<Name>` / `Plugins/<Name>`

The slash form spends two segments on the plugin reference.

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Plugin/MoonToon` | `Test` | `/MoonToon/Test` |
| `Plugins/MoonToon` | `Mat/Test` | `/MoonToon/Mat/Test` |
| `Plugins/MoonToon/Shared` | `Test` | `/MoonToon/Shared/Test` |

> [!WARNING]
> `Root="Plugin"` and `Root="Plugins"` with **no** second segment do not name a plugin. They fall
> through to the last dispatch row and are treated as ordinary folder names, producing
> `/Game/Plugin` and `/Game/Plugins`. No diagnostic is emitted.

### Branch — explicit mount root

A `Root` that begins with `/` and is not one of the forms above is taken verbatim as a mount point.

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `/MyMount` | `Test` | `/MyMount/Test` |
| `/MyMount/Sub` | `Test` | `/MyMount/Sub/Test` |
| `/Engine` | `Test` | `/Engine/Test` |

The first segment must survive `SanitizeObjectName` unchanged, otherwise the destination fails with
[`DSH8097`](../diagnostics/DSH8xxx.md#dsh8097). The mount point itself is not checked for existence
here — an unmounted root fails later, at the `IsValidObjectPath` gate.

### Branch — bare relative path

Any other `Root` without a leading `/` becomes folders under `/Game`.

| `Root` | `Name` | Package |
| :-- | :-- | :-- |
| `Foo` | `Test` | `/Game/Foo/Test` |
| `Foo/Bar` | `Test` | `/Game/Foo/Bar/Test` |
| `Foo/Bar` | `Deep/Test` | `/Game/Foo/Bar/Deep/Test` |

The difference between this branch and the previous one is exactly the leading slash:
`Root="Foo/Bar"` is `/Game/Foo/Bar`, `Root="/Foo/Bar"` is `/Foo/Bar`.

## Plugin-root requirements

Both plugin forms resolve through the same validator. Every gate below must pass, in this order;
each has its own code, which the product's `DSH8200` quotes.

| # | Requirement | Code when it fails |
| :-- | :-- | :-- |
| 1 | The plugin name is non-empty and unchanged by `SanitizeObjectName` | [`DSH8095`](../diagnostics/DSH8xxx.md#dsh8095) (`Plugin.X`), [`DSH8096`](../diagnostics/DSH8xxx.md#dsh8096) (`Plugin/X`) |
| 2 | A plugin with that name is known to the plugin manager | [`DSH8089`](../diagnostics/DSH8xxx.md#dsh8089) |
| 3 | It is a **project** plugin, and its base directory is under the project's `Plugins` directory | [`DSH8090`](../diagnostics/DSH8xxx.md#dsh8090) |
| 4 | It is enabled | [`DSH8091`](../diagnostics/DSH8xxx.md#dsh8091) |
| 5 | It can contain content | [`DSH8092`](../diagnostics/DSH8xxx.md#dsh8092) |
| 6 | Its `Content` directory exists on disk | [`DSH8093`](../diagnostics/DSH8xxx.md#dsh8093) |
| 7 | Its content is mounted *(UE 5.6+ only)* | [`DSH8094`](../diagnostics/DSH8xxx.md#dsh8094) |

Gate 7 does not exist on UE 5.3 – 5.5; on those engines an unmounted plugin is caught later by the
object-path validation instead.

The root package path is the plugin's own mounted asset path — normalized to `\` → `/`, no trailing
slash, a forced leading slash. If that degenerates to empty or `/`, `/<PluginName>` is used.

> [!NOTE]
> Engine plugins and marketplace plugins installed under the engine directory are rejected by gate 3
> even when they are enabled and mounted. Only plugins physically under `<Project>/Plugins` are
> accepted. To write into an engine-side mount, use the explicit mount-root branch
> (`Root="/SomeMount"`), which skips the plugin validator entirely.

## Per asset kind

The path resolution is identical for all four block kinds. What differs is the class created and,
for the function kinds, the material-function usage stamped on it.

| Source block | Asset class | Material function usage |
| :-- | :-- | :-- |
| `Shader` — `Graph` backend | `UMaterial` | — |
| `Shader` — `ThinCustom` backend *(since 1.5.0)* | `UDreamShaderMaterialInstance` plus a hidden `UMaterial` subobject | — |
| `ShaderFunction` | `UMaterialFunction` | `Default` |
| `ShaderLayer` *(since 1.3.0)* | `UMaterialFunctionMaterialLayer` | `MaterialLayer` |
| `ShaderLayerBlend` *(since 1.3.0)* | `UMaterialFunctionMaterialLayerBlend` | `MaterialLayerBlend` |

When an asset already exists at the resolved path, the class must match:

| Kind | Match rule |
| :-- | :-- |
| `ShaderFunction` | **exact** class match — a `UMaterialFunctionMaterialLayer` at that path is rejected |
| `ShaderLayer`, `ShaderLayerBlend` | `IsA` the expected class |
| `Shader` — `Graph` | the existing object must be a `UMaterial` |
| `Shader` — `ThinCustom` | the existing object must be a `UDreamShaderMaterialInstance` |

## Custom Pass pipelines

*(since 2.1.0)* A [`.dsp`](../language-v2/passes.md) has no `Name=`, no `Root=` and no `/// @name`. Its one
product, a `UDreamPassPipeline`, is named after the file and placed by the 2.0 default: the file's folder
relative to its [source root](../language/source-files.md#source-roots), under `/Game` — or under the
plugin's mount point for a file under a plugin's source root, by the same
[plugin source root default](#the-plugin-source-root-default) and with the same fallback to `/Game`. The
render target of every exported buffer lands in the pipeline's folder, named
`<Pipeline>_<Buffer>` through `SanitizeObjectName`.

| Source file | Pipeline | Render target of an exported buffer `Mask` |
| :-- | :-- | :-- |
| `<Project>/DShader/Passes/CP_Highlight.dsp` | `/Game/Passes/CP_Highlight` | `/Game/Passes/CP_Highlight_Mask` |
| `<Project>/DShader/CP_Wind.dsp` | `/Game/CP_Wind` | `/Game/CP_Wind_Mask` |
| `Plugins/MoonToon/DShader/Passes/CP_Toon.dsp` | `/MoonToon/Passes/CP_Toon` | `/MoonToon/Passes/CP_Toon_Mask` |

| At the path | Refused when |
| :-- | :-- |
| the pipeline's | the object there is not a `UDreamPassPipeline` (`DSH8302`), or it is a saved asset DreamShader did not generate (`DSH8303`) |
| a render target's | the object there is not a `UTextureRenderTarget2D`, or it is a saved render target DreamShader did not make (`DSH8313`) |

The only way to choose where a pipeline lands is where the `.dsp` lives, which is also why a decompiled
`.dsp` cannot keep its asset's path from elsewhere (`DSH9224`). Moving or renaming the file builds a new
pipeline at the new path; the old one, and its render targets, are left where they are, and its HLSL slots
stay taken until [`dsc pass-registry -Gc`](../tools/commandlet.md#pass-registry) collects them.

## On-disk mapping

| Package root | On-disk directory |
| :-- | :-- |
| `/Game/…` | `<Project>/Content/…` |
| `/<PluginName>/…` | `<Project>/Plugins/<PluginName>/Content/…` |
| `/<MountRoot>/…` | wherever that mount is registered |

The file is `<directory>/<Leaf>.uasset`. A `Graph`-backend material and every function, layer and
blend asset are saved by every build that may write; a ThinCustom product only once it is
Materialized — see [Ephemeral materials](ephemeral.md).

## Notes

- **`Name` may contain folders; `Root` is only a prefix.** `Name="A/B/C"` under `Root="Game"` yields
  `/Game/A/B/C`, and the asset is named `C`.
- **A duplicate attribute key overwrites the earlier one.** `Shader(Name="A", Name="B")` resolves to
  `B`, with the warning [`DSH2244`](../diagnostics/DSH2xxx.md#dsh2244) *(since 2.0.0; silent through
  1.x)*.
- Every segment is sanitized independently. Characters `SanitizeObjectName` rejects are replaced, so
  `Name="My Mat"` resolves to a leaf named `My_Mat` without a diagnostic.
- Empty segments are dropped: `Name="A//B"` is `A/B`, and `Root="Game//Sub"` is `/Game/Sub`.
- The resolver is also what the Content Browser status column and the *Materialize* action use, so a
  path that fails here also shows as unresolvable in the [Material Content
  Browser](../tools/material-browser.md).

## Diagnostics

The asset layer's codes reach the compile result inside the emitter's: a destination that does not
resolve is [`DSH8200`](../diagnostics/DSH8xxx.md#dsh8200), an asset that cannot be created or reused
is `DSH8201` (`Graph` material), `DSH8203` (ThinCustom instance) or `DSH8202` (function, layer,
blend), and each message quotes the code below.

| Code | Raised when |
| :-- | :-- |
| `DSH2242` | a `Shader` / `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` header has no `Name`, or an empty one |
| `DSH2244` | *(warning)* an attribute key is written twice; the later value wins |
| [`DSH8098`](../diagnostics/DSH8xxx.md#dsh8098), `DSH8099` | `Name` is empty after trimming and slash-stripping |
| [`DSH8100`](../diagnostics/DSH8xxx.md#dsh8100) | the leaf segment is empty after sanitization |
| [`DSH8088`](../diagnostics/DSH8xxx.md#dsh8088) | a non-empty folder segment of `Name` or of `Root` sanitizes away to nothing |
| — | the assembled path fails `IsValidObjectPath`; `DSH8200` quotes the engine's reason, or says the path is not a valid object path, with no code of its own |
| `DSH8095`, `DSH8096` | plugin name empty, or altered by sanitization |
| `DSH8097` | explicit mount root altered by sanitization |
| `DSH8089`–`DSH8094` | a plugin gate — see [Plugin-root requirements](#plugin-root-requirements) |
| [`DSH8104`](../diagnostics/DSH8xxx.md#dsh8104), `DSH8108`, `DSH8113` | package creation failed (material, instance, function) |
| [`DSH8105`](../diagnostics/DSH8xxx.md#dsh8105), `DSH8109`, `DSH8114` | object creation failed (material, instance, function) |
| [`DSH8102`](../diagnostics/DSH8xxx.md#dsh8102) | `Graph` backend, wrong class at the path |
| [`DSH8106`](../diagnostics/DSH8xxx.md#dsh8106) | ThinCustom backend, wrong class at the path |
| [`DSH8111`](../diagnostics/DSH8xxx.md#dsh8111) | function kind, an object that is not a material function at the path |
| [`DSH8110`](../diagnostics/DSH8xxx.md#dsh8110) | function kind, wrong material-function subclass |
| [`DSH8103`](../diagnostics/DSH8xxx.md#dsh8103), [`DSH8107`](../diagnostics/DSH8xxx.md#dsh8107) | ownership guard on a material (`Graph`, ThinCustom) — see [Regeneration](regeneration.md#ownership-guard) |
| [`DSH8112`](../diagnostics/DSH8xxx.md#dsh8112) | ownership guard on a material function |

## Example

```c
Shader(Name="Mat/Test", Root="Plugin.MoonToon")
{
    Properties { vec3 Tint = vec3(1.0, 0.2, 0.2); }
    Settings   { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs    { vec3 Color; Base.EmissiveColor = Color; }
    Graph      { Color = Tint; }
}
```

Resolved destination:

```text
Root      "Plugin.MoonToon"  ->  /MoonToon                       (plugin mount point)
Name      "Mat/Test"         ->  folders "Mat", leaf "Test"
package                          /MoonToon/Mat/Test
object path                      /MoonToon/Mat/Test.Test
on disk                          <Project>/Plugins/MoonToon/Content/Mat/Test.uasset
```

## See also

- [Shader](../language/shader.md) — the header attributes as part of the block grammar
- [ShaderFunction](../language/shader-function.md) — the same attributes on a function block
- [ShaderLayer / ShaderLayerBlend](../language/shader-layer.md) — the layer asset kinds
- [Ephemeral materials](ephemeral.md) — when the `.uasset` is actually written
- [Regeneration](regeneration.md) — the ownership guard and where it does not apply
- [Caching](caching.md) — the provenance metadata that marks an asset as DreamShader-generated
- [Path(Root, "…")](../parameters/path.md) — the *other* path grammar, for referencing existing assets
- [Backend](../settings/backend.md) — which class a `Shader` produces
- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md) — a source whose asset is named after its file
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
