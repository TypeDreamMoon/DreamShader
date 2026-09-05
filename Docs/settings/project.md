# Project settings

> [DreamShader](../index.md) » [Settings](index.md) » **Project settings**

The project-wide configuration object: sixteen properties under *Project Settings ▸ DreamPlugin ▸
Dream Shader*, persisted to the project's `DefaultEngine.ini`.

| | |
| :-- | :-- |
| Declared in | C++ — `UDreamShaderSettings`, a `UDeveloperSettings` in the `DreamShader` runtime module |
| Kind | project settings object |
| Config file | `<Project>/Config/DefaultEngine.ini`, section `[/Script/DreamShader.DreamShaderSettings]` |

## Location

| Aspect | Value |
| :-- | :-- |
| Container | `Project` |
| Category | `DreamPlugin` |
| Section | `DreamShader` |
| Section title | **Dream Shader** |
| Section description | **Dream Shader Settings** |

The panel is at *Project Settings ▸ DreamPlugin ▸ Dream Shader* — **not** under *Plugins*. Edits made
in the panel, and the toggles the editor exposes elsewhere, are written straight back to
`DefaultEngine.ini`.

## Settings

Every configurable property, grouped by the category it appears under in the panel.

| Category | UI name | Config property | Type | Default | Effect |
| :-- | :-- | :-- | :-- | :-- | :-- |
| Mappings | Shading Model Mappings | `ShadingModelMappings` | `TMap<FString, EMaterialShadingModel>` | *empty* | Extra or overriding spellings for the `ShadingModel` setting value. Scanned before the built-in table. |
| Mappings | Blend Mode Mappings | `BlendModeMappings` | `TMap<FString, EBlendMode>` | *empty* | Extra or overriding spellings for `BlendMode` / `RenderType`. |
| Mappings | Material Domain Mappings | `MaterialDomainMappings` | `TMap<FString, EMaterialDomain>` | *empty* | Extra or overriding spellings for `MaterialDomain` / `Domain`. |
| Paths | Source Directory | `SourceDirectory` | `FDirectoryPath` | `DShader` | Root scanned for `.dsm` / `.dsf` / `.dsh` sources. Empty falls back to `DShader`; a relative path resolves against the project directory. `<Source>/Packages` is derived from it. |
| Paths | Generated Shader Directory | `GeneratedShaderDirectory` | `FDirectoryPath` | `Intermediate/DreamShader/GeneratedShaders` | Where the generated `.ush` include is written and the virtual shader directory is mapped. Empty falls back to the default. |
| Paths | **Scan Plugin Source Directories** | `bScanPluginSourceDirectories` | `bool` | `true` | When on, every enabled plugin that has a `DShader` folder contributes a source root of its own. Off leaves the project's *Source Directory* as the only root. |
| Compiler | **Preprocessor Defines** *(since 1.9.0)* | `PreprocessorDefines` | `TMap<FString, FString>` | *empty* | Names visible to [`#if`](../language/preprocessor.md) in every source the project compiles. Keys are **case-sensitive**; an empty value is a bare marker, true to `#if` and to `defined()`. A key beginning with `DS_` is dropped with a warning — that prefix is reserved for the builtins. Editing the map rebuilds the sources that read a changed name. |
| Compiler | **Default Compiler Backend** | `DefaultBackend` | `EDreamShaderDefaultBackend` | `ThinCustom` | Backend for a source file that does not set `Settings = { Backend = … }`. Changing it regenerates every source file in memory. |
| Compiler | **Show In-Memory Materials In Content Browser** | `bShowInMemoryMaterialsInContentBrowser` | `bool` | `false` | When off, memory-only DreamShader instances report themselves as non-assets and disappear from the Content Browser, asset-registry enumeration and save pickers. Read live, on every query. |
| Compiler | **Lay Out In-Memory Graphs** | `bLayoutInMemoryGraphs` | `bool` | `true` | When on, [graph layout](../generation/graph-layout.md) also runs for the memory-only materials an interactive compile produces, so the graph you see after a save matches the generated asset. Off, those graphs keep their construction coordinates — a single tall column. Graphs at or above the large-graph threshold skip layout either way. |
| Compiler | Auto Compile On Save | `bAutoCompileOnSave` | `bool` | `true` | When off, the source-directory watcher ignores file changes entirely. |
| Compiler | Save Debounce Seconds | `SaveDebounceSeconds` | `float` | `0.25` | Quiet period after a file change before compiling. Clamped to `[0.05, 10.0]`; the slider stops at `2.0`. Falls back to `0.25` when the settings object is unavailable. |
| Compiler | Verbose Logs | `bVerboseLogs` | `bool` | `false` | Adds `Display`-level logging of the dependent-file compile queue. |
| Decompiler | Export Decompiled Layout | `bExportDecompiledLayout` | `bool` | `true` | When on, a decompiled `.dsm` carries a `Layout = { … }` section reproducing node positions. |
| Editor | Open In New Window | `bOpenInNewWindow` | `bool` | `true` | When off, the VSCode launch command gets `--reuse-window`. |
| Editor | **Material Instance Subfolder** | `InstanceSubfolder` | `FString` | `Instances` | Subfolder, relative to the parent material's folder, where the Material Content Browser creates new instances. Empty creates them alongside the parent. The asset is named `MI_<ParentName>`, uniquified. |

Rows in **bold** carry an explicit `DisplayName`; the others show the name Unreal derives from the
property identifier, which drops a leading `b` from booleans.

### Enumerators of `EDreamShaderDefaultBackend`

| Enumerator | Meaning |
| :-- | :-- |
| `Graph` | Build a visible `UMaterial` node graph per material. |
| `Instance` | **Deprecated** alias for `ThinCustom` *(since 1.5.0)*. |
| `ThinCustom` | Build the graph on a hidden per-material base and emit a lightweight material instance of it. **The default.** |

Full behaviour, including how a per-file `Backend` setting overrides this, is on
[Backend](backend.md#precedence).

## Verbatim tooltips

*Scan Plugin Source Directories*:

> When enabled, every enabled plugin that ships a DShader folder contributes its own source root, so
> a plugin can carry the .dsm/.dsf/.dsh files that build its materials. Plugin roots are discovered
> and compiled but never rewritten by the editor -- only the project's own source directory is
> writable. Imports never cross roots: a file resolves its imports against its own root and that
> root's Packages folder.

*Default Compiler Backend*:

> How DreamShader materializes a source file that does not specify `Settings = { Backend = "..." }`.
> ThinCustom (the default) builds the material graph on a hidden per-material base and emits a
> lightweight, memory-only material instance of it -- full feature surface, no visible per-material
> asset. Graph builds a visible UMaterial node graph. Instance is a deprecated alias for ThinCustom.

*Show In-Memory Materials In Content Browser*:

> When enabled, the memory-only DreamShader materials appear in the Content Browser like unsaved
> assets. Disabled by default: the source files are the intended authoring surface, and hiding the
> materials also prevents accidental Save actions from materializing them to disk.

*Material Instance Subfolder*:

> Subfolder, relative to the parent material's folder, where the Material Content Browser creates
> new material instances. Leave empty to create them alongside the parent.

*Preprocessor Defines*:

> Preprocessor defines every .dsm/.dsf/.dsh source compiles with -- what its #if / #elif conditions
> read. The name answers defined(); a value that parses as an integer compares as a number, anything
> else as a string; an empty value still counts as defined; a name absent from this table evaluates to
> 0, as in C. Conditions are evaluated at GENERATION time and the losing branch is cut before the
> parser sees it, so a condition can select a Domain, a ShadingModel or a whole Outputs block -- none
> of which a StaticSwitch can reach, because they describe what the material IS rather than what it
> computes. Editing this table rebuilds only the generated assets whose sources actually read a name
> that changed. Names are case-sensitive and must match [A-Za-z_][A-Za-z0-9_]*. The DS_ prefix is
> reserved for the read-only builtins; an entry using it is dropped with a log warning rather than
> failing the compile, since a settings mistake has no source line to report against.

The builtin names this tooltip alludes to are `DS_ENGINE_MAJOR`, `DS_ENGINE_MINOR`,
`DS_ENGINE_PATCH`, `DS_PLATFORM`, `DS_PLUGIN_VERSION` and `DS_SUBSTRATE`; they are listed out in
[the preprocessor page](../language/preprocessor.md). The tooltip names them only by their prefix
because UHT caps a metadata string at 1024 characters, and spelling all six out went over it.

## The mapping maps

The three `Mappings` entries extend the value spellings accepted by the three enum settings. They are
**empty by default**: the built-in alias tables are rebuilt on demand during resolution and are never
materialized into these maps, so an empty panel does not mean "no aliases".

| Behaviour | Detail |
| :-- | :-- |
| Precedence | the project map is scanned first; the first normalized-key match returns immediately |
| Shadowing | an entry whose key normalizes to a built-in alias replaces it |
| Disabling | an entry mapped to `MSM_MAX` / `BLEND_MAX` / `MD_MAX` makes that spelling fail to resolve, and does not fall through to the built-in |
| Key matching | trimmed, lowercased, and stripped of spaces, `_` and `-` |
| Ambiguity | two entries whose keys normalize identically are resolved in unspecified order |

The complete built-in tables are on [Material enums](material-enums.md).

## Other places these settings are surfaced

| Setting | Also reachable from |
| :-- | :-- |
| `bShowInMemoryMaterialsInContentBrowser` | *Tools ▸ DreamShader ▸ Show In-Memory Materials*, and the Project page of the [Material Content Browser](../tools/material-browser.md). Both write the ini and re-broadcast asset creation/removal for every memory-only instance. |
| `DefaultBackend` | Changing it in the panel triggers an immediate in-memory regeneration of every source file, plus a notification when persisted generated assets shadow the result. |
| `SourceDirectory`, `GeneratedShaderDirectory` | Consumed by the module's directory helpers; see [Generated HLSL](../generation/generated-hlsl.md) and [Packages](../tools/packages.md). |
| `PreprocessorDefines` | One of five tiers that make up the define table a compile sees, and the lowest-precedence one that a person edits. C++ registration and providers outrank it, and `-Define=` on the [commandlet](../tools/commandlet.md) outranks those; only the builtin `DS_` names outrank everything. See [Preprocessor ▸ Where defines come from](../language/preprocessor.md#where-defines-come-from). |

## Notes

- The class is `Config=Engine, DefaultConfig`, so values live in the **project's**
  `Config/DefaultEngine.ini`, not in a per-user file. They are shared by everyone who checks the
  project out.
- There is **no in-memory on/off toggle**. The editor always generates in memory — source files are
  the authoring surface — and materialization to disk happens at cook, through the
  [commandlet](../tools/commandlet.md), or through an explicit action. *Default Compiler Backend*
  replaced the old In-Memory toggle in 1.5.0.
- `GeneratedShaderDirectory` is only consulted while the virtual shader directory is unmapped. If a
  mapping already exists for this project, **the existing mapping wins over the setting** until the
  editor restarts.
- The built-in shader library path setting was **removed in 1.3.8**; no replacement exists.
- The C++ surface of this object — the three `TryResolve*` methods, `NormalizeMappingKey` and the
  three `BuildDefault*Mappings` helpers — is documented in [Settings API](../api/settings.md).

## Example

`<Project>/Config/DefaultEngine.ini`, showing every scalar property at its default value:

```ini
[/Script/DreamShader.DreamShaderSettings]
SourceDirectory=(Path="DShader")
GeneratedShaderDirectory=(Path="Intermediate/DreamShader/GeneratedShaders")
DefaultBackend=ThinCustom
bShowInMemoryMaterialsInContentBrowser=False
bAutoCompileOnSave=True
SaveDebounceSeconds=0.250000
bVerboseLogs=False
bExportDecompiledLayout=True
bOpenInNewWindow=True
InstanceSubfolder=Instances
```

The ini keys are the raw C++ identifiers, including the `b` prefix on booleans — `bAutoCompileOnSave`,
not `AutoCompileOnSave`. The three mapping maps and *Preprocessor Defines* are container properties;
edit them in the Project Settings panel and let the editor write them back. A hand-written entry uses
the `+Key=((…))` form:

```ini
+PreprocessorDefines=(("MOONTOON_LEGACY_TOON", "1"))
```

## See also

- [Settings](index.md) — the per-file `Settings` section this page's defaults interact with
- [Preprocessor](../language/preprocessor.md) — what *Preprocessor Defines* feeds, and the four other tiers
- [Backend](backend.md) — how *Default Compiler Backend* is overridden per file
- [Material enums](material-enums.md) — the built-in tables the mapping maps extend
- [Shader settings](material.md) — the reflected `UMaterial` property surface
- [Settings API](../api/settings.md) — `UDreamShaderSettings` in C++
- [Material instance API](../api/material-instance.md) — the `IsAsset()` behaviour driven by the visibility toggle
- [In-memory materials](../generation/in-memory.md) — what "memory-only" means in practice
- [Generated HLSL](../generation/generated-hlsl.md) — what `GeneratedShaderDirectory` receives
- [Material Content Browser](../tools/material-browser.md) — the Project page and the instance factory
- [Decompiler](../tools/decompiler.md) — the layout-export toggle
- [Workspace](../tools/workspace.md) — VSCode launch and the exported settings manifest
- [Editor integration](../tools/editor-integration.md) — the Tools menu entries
