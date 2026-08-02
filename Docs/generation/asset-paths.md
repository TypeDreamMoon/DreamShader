# Asset paths

> [DreamShader](../index.md) » [Generation](index.md) » **Asset paths**

The rule that turns a block's `Name` and `Root` header attributes into an Unreal package path, an
object path, and a file on disk.

| | |
| :-- | :-- |
| Applies to | `Shader`, `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend` — one resolver, identical for all four |
| Kind | header attributes |
| Since | `Root="Plugin.X"` — `1.2.0` |

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

`Name` is **required**; a missing one fails with `Shader(Name="...") is required.` (or
`{Kind}(Name="...") is required.` for the function blocks). `Root` is optional and defaults to the
empty string. Attribute keys are matched case-insensitively, so `name=` and `ROOT=` both resolve.

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

The first segment must survive `SanitizeObjectName` unchanged, otherwise
`DreamShader Root '{Root}' has an invalid package root.` The mount point itself is not checked for
existence here — an unmounted root fails later, at the `IsValidObjectPath` gate.

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
each has its own message.

| # | Requirement | Message when it fails |
| :-- | :-- | :-- |
| 1 | The plugin name is non-empty and unchanged by `SanitizeObjectName` | `DreamShader Root '{Root}' has an invalid plugin name.` |
| 2 | A plugin with that name is known to the plugin manager | `DreamShader Root '{Root}' references project plugin '{Plugin}', but no enabled plugin with that name was found.` |
| 3 | It is a **project** plugin, and its base directory is under the project's `Plugins` directory | `DreamShader Root '{Root}' must reference a project plugin under '{PluginsDir}'.` |
| 4 | It is enabled | `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin is not enabled.` |
| 5 | It can contain content | `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin cannot contain content.` |
| 6 | Its `Content` directory exists on disk | `DreamShader Root '{Root}' references project plugin '{Plugin}', but its Content directory does not exist: '{ContentDir}'.` |
| 7 | Its content is mounted *(UE 5.6+ only)* | `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin content is not mounted.` |

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

## On-disk mapping

| Package root | On-disk directory |
| :-- | :-- |
| `/Game/…` | `<Project>/Content/…` |
| `/<PluginName>/…` | `<Project>/Plugins/<PluginName>/Content/…` |
| `/<MountRoot>/…` | wherever that mount is registered |

The file is `<directory>/<Leaf>.uasset`. It is written only in persist mode — see
[In-memory materials](in-memory.md).

## Notes

- **`Name` may contain folders; `Root` is only a prefix.** `Name="A/B/C"` under `Root="Game"` yields
  `/Game/A/B/C`, and the asset is named `C`.
- **A duplicate attribute key silently overwrites the earlier one.** `Shader(Name="A", Name="B")`
  resolves to `B` with no diagnostic.
- Every segment is sanitized independently. Characters `SanitizeObjectName` rejects are replaced, so
  `Name="My Mat"` resolves to a leaf named `My_Mat` without a diagnostic.
- Empty segments are dropped: `Name="A//B"` is `A/B`, and `Root="Game//Sub"` is `/Game/Sub`.
- The resolver is also what the Content Browser status column and the *Materialize* action use, so a
  path that fails here also shows as unresolvable in the [Material Content
  Browser](../tools/material-browser.md).

## Diagnostics

Runtime substitutions are rendered as `{Placeholder}` throughout this page.

| Message | Cause |
| :-- | :-- |
| `Shader(Name="...") is required.` | a `Shader` header with no `Name` |
| `{Kind}(Name="...") is required.` | a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` header with no `Name` |
| `DreamShader asset name must resolve to a non-empty asset path.` | `Name` is empty after trimming and slash-stripping |
| `DreamShader asset name '{Name}' produced an invalid asset name.` | the leaf segment is empty after sanitization |
| `DreamShader asset name '{Name}' contains an invalid folder segment.` | a non-empty folder segment of `Name` sanitized away to nothing |
| `DreamShader asset path '{Path}' is not a valid Unreal object path.` | the assembled path failed `IsValidObjectPath` and the engine reported no reason of its own |
| `DreamShader Root '{Root}' contains an invalid folder segment.` | a non-empty folder segment of `Root` sanitized away to nothing |
| `DreamShader Root '{Root}' has an invalid plugin name.` | plugin name empty, or altered by sanitization |
| `DreamShader Root '{Root}' has an invalid package root.` | explicit mount root altered by sanitization |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but no enabled plugin with that name was found.` | unknown plugin |
| `DreamShader Root '{Root}' must reference a project plugin under '{PluginsDir}'.` | engine plugin, or a plugin outside `<Project>/Plugins` |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin is not enabled.` | disabled plugin |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin cannot contain content.` | code-only plugin |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but its Content directory does not exist: '{ContentDir}'.` | missing `Content` folder |
| `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin content is not mounted.` | unmounted plugin *(UE 5.6+)* |
| `Failed to create package '{Package}'.` | package creation failed |
| `Asset '{ObjectPath}' already exists and is not a Material.` | `Graph` backend, wrong class at the path |
| `Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.` | ThinCustom backend, wrong class at the path |
| `Asset '{ObjectPath}' already exists and is not a MaterialFunction asset.` | function kind, wrong class at the path |
| `Asset '{ObjectPath}' already exists as '{ActualClass}', but {Kind} generation requires '{ExpectedClass}'. Delete or move the existing asset and regenerate it.` | function kind, wrong material-function subclass |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | ownership guard on a material — see [Regeneration](regeneration.md#ownership-guard) |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | ownership guard on a material function |

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
- [In-memory materials](in-memory.md) — when the `.uasset` is actually written
- [Regeneration](regeneration.md) — the ownership guard and where it does not apply
- [Caching](caching.md) — the provenance metadata that marks an asset as DreamShader-generated
- [Path(Root, "…")](../parameters/path.md) — the *other* path grammar, for referencing existing assets
- [Backend](../settings/backend.md) — which class a `Shader` produces
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
