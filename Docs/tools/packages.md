# Packages

> [DreamShader](../index.md) » [Tools](index.md) » **Packages**

`DShader/Packages` — the install root for reusable DreamShaderLang libraries, resolvable by `import`
in a 1.x file and by `#include` in a `.dss`. A package is somebody else's source: the plugin compiles
it only through what includes it, except what the package asks to have built — its
[assets](#package-assets) *(after 2.1.2)*: material functions for the Material Editor, generated into the
package's own asset folder. Every source root has one: the project's, and `<Plugin>/DShader/Packages`
for a plugin that ships a [source root](../language/source-files.md#source-roots).

| | |
| :-- | :-- |
| Kind | directory convention |
| Path | `<SourceDirectory>/Packages` — default `<Project>/DShader/Packages` |
| Created by | the `DreamShader` runtime module at startup, unconditionally |
| Configurable | only indirectly, through the *Source Directory* project setting |
| Implemented by | the plugin: the import root, the enumeration rules and the package assets. Everything else: the editor extensions |

## Synopsis

```text
<Project>/
└─ DShader/                              <SourceDirectory>
   ├─ Materials/                         ordinary sources — compiled, listed, synced
   │  └─ M_Sample.dsm
   ├─ dreamshader.lock.json              written by the extension
   └─ Packages/                          <SourceDirectory>/Packages
      └─ @scope/
         └─ package-name/
            ├─ dreamshader.package.json  read by the extension
            ├─ README.md
            ├─ LICENSE
            ├─ Library/
            │  └─ Noise.dsh              importable; an asset header when it asks for functions
            ├─ Assets/                   (after 2.1.2) built like the project's own sources
            │  └─ MF_Noise.dss           -> /Game/DreamShaderPackages/typedreammoon/package-name/MF_Noise
            └─ Examples/
               └─ M_NoisePreview.dsm     never compiled by the plugin
```

The directory name is hard-coded: the package root is always the literal subdirectory `Packages` of
the configured source directory. Changing *Source Directory* moves it; there is no separate package
directory setting.

## Division of labour

The Unreal plugin implements three things about packages, and nothing else. Everything under
"extension" is a convention defined and enforced by the [editor extensions](workspace.md#editor-extensions),
in their own repositories, and cannot be observed from the plugin.

| Aspect | Implemented by |
| :-- | :-- |
| Creating `<SourceDirectory>/Packages` at editor startup | **plugin** |
| `<SourceDirectory>/Packages` as the third `import` / `#include` resolution root | **plugin** |
| Excluding package files from source enumeration, apart from a package's assets | **plugin** |
| [Package assets](#package-assets): the `Assets` folder, asset headers, the package asset root | **plugin** |
| `dreamshader.package.json` — existence, fields, validation | extension |
| `dreamshader.lock.json` — existence, contents, updates | extension |
| Installing, updating, removing packages | extension |
| Package store, index sources, GitHub topic search | extension |
| Package scaffolding / "create package" flows | extension |
| Scoped (`@scope/name`) naming | extension |

> [!NOTE]
> No C++ in this plugin reads `dreamshader.package.json` or `dreamshader.lock.json`. A package is
> resolvable purely because its files exist under `<SourceDirectory>/Packages`; deleting the manifest
> does not break an `import`, and adding one does not change how the plugin behaves. The manifest and
> lock file exist for the extensions' installer and store.

## Import resolution

An `import` specifier — or the path of a `.dss` `#include` — is normalized, then tried against three
candidate roots in order. The first candidate that both stays inside its own root and exists on disk
wins. This resolver is shared by the compiler's include resolver and by the editor's dependency
scanner, so both agree. *(since 2.0.0)* The compiler reads the header it finds on its own and declares
its names into the including file; nothing is pasted in as text.

### Specifier normalization

| Step | Rule |
| :-- | :-- |
| 1 | Leading and trailing whitespace trimmed |
| 2 | Every `\` replaced with `/` |
| 3 | **All** leading `./` sequences stripped (repeatedly, so `././X` becomes `X`) |
| 4 | If the result has no extension, `.dsh` is appended |

Step 4 means `import "@scope/pkg/Library/Noise"` and `import "@scope/pkg/Library/Noise.dsh"` are the
same import. Only a header can be imported *(since 2.0.0)*: an `import` that names a `.dsf` or a
`.dsm` is [`DSH2252`](../diagnostics/DSH2xxx.md#dsh2252), and an include that resolves to anything but a
`.dsh` (or a `.dss`) is [`DSH8295`](../diagnostics/DSH8xxx.md#dsh8295).

### Candidate roots

| Order | Candidate path | Root it must stay under |
| :-- | :-- | :-- |
| 1 | `<directory of the importing file>/<specifier>` | the importing file's import root — whichever source or packages directory, of any root, contains it (longest match), or the file's own directory if none does |
| 2 | `<owning root's source directory>/<specifier>` | that source directory |
| 3 | `<owning root's source directory>/Packages/<specifier>` | that packages directory |

The owning root is the source root the importing file sits under — `<SourceDirectory>` for a project
file — so a plugin's sources resolve against the plugin's own `Packages`.

Because candidate 1 is rooted at `Packages` for a file that lives inside a package, a `.dsh` inside a
package can reach its siblings with a relative specifier but cannot climb out of the package tree
with `../`; the escape guard rejects the candidate and resolution falls through to roots 2 and 3.

A specifier that resolves under no root is [`DSH8292`](../diagnostics/DSH8xxx.md#dsh8292). The full
`import` grammar, how a header is read and cycle handling are on [import](../language/import.md).

## Source-file enumeration

Everything under a root's `Packages` tree is left out of every build path, with two exceptions — the
files a package asks to have built *(after 2.1.2)*:

| Package file | Built | Where its assets go |
| :-- | :-- | :-- |
| a `.dss`, `.dsf` or `.dsi` under the package's `Assets` folder | yes, like a source of the project | the package's asset root, plus the file's folder under `Assets` |
| a `.dsh` that asks for material functions (an [asset header](#asset-headers)), not under `Examples` | yes, as the wrappers of those functions | the package's asset root, at the name each function asks for |
| any other `.dsh` | only through what includes it | — |
| anything else: a `.dsm`, a `.dsp`, an `Examples` file, a source outside `Assets` | **never** | — |

The test is a path-prefix comparison, case-insensitive. A *package* is the folder under `Packages`
that holds a file: `Packages/@scope/name` for a scoped package, `Packages/name` otherwise; a file
directly in `Packages` (or directly in an `@scope` folder) belongs to none and is never built. An asset
header is recognised by its text: it holds `/// @generate`, `#pragma generate` or `Generate =`.

What a feature does with the package tree:

| Feature | Package `Assets` sources and asset headers | Every other package file |
| :-- | :-- | :-- |
| Startup Ephemeral generation | built | left out |
| [Commandlet](commandlet.md) `compile`, `dump-graph`, `check`, `dump-ir`, `index`, `list-generated` with `-All` | built / read | left out |
| Cook-time materialization | built | left out |
| *Recompile DSM* / full rescan queue, the dependency graph | built | left out |
| File watcher, on save | the file is rebuilt, and what includes it | a `.dsh` rebuilds what includes it; anything else is ignored |
| Material Content Browser source list | listed | left out |
| VirtualFunction declaration sync, asset rename sync, `migrate -All`, `fmt -All`, `pass-registry -All` | left out: these rewrite sources, and a package's files are its installer's | left out |

> [!NOTE]
> Up to 2.1.2 the rule was different and inconsistent: a `.dsf`, `.dss`, `.dsi` or `.dsp` anywhere in a
> package was compiled by the interactive paths (*Recompile DSM*, the file watcher, the dependency graph)
> and by none of the batch ones (startup, `compile -All`, the cook), so such a package built its assets in
> an editor session and not in a headless build. Now only what is under `Assets` is built, by every path;
> move a package's buildable sources there.

> [!WARNING]
> `Examples/**` inside a package is never compiled by any path, and never appears in the Material
> Content Browser. To use an example, copy it out of `DShader/Packages` into `DShader/` (or any
> subdirectory of it that is not `Packages`).

## Package assets

*(after 2.1.2)* A package can ship material functions that the Material Editor uses like any other: an
artist drops `MF_BL_TexNoise3D` into a graph, and nobody writes DreamShader for it. The plugin builds
them in the project the package is installed in, into a folder of the package's own, and rebuilds them
when the package changes. A source that *calls* a package function — `BL::TexNoise3D(...)` in a `.dss`
or a 1.x `Graph` — still inlines it, as before; the generated function is a second way in, not a
replacement.

### The asset root

Everything a package builds goes under its **asset root** unless the source names a `Root` of its own:

| Package | Asset root |
| :-- | :-- |
| `<Project>/DShader/Packages/@typedreammoon/dreamshader-blender` | `/Game/DreamShaderPackages/typedreammoon/dreamshader-blender` |
| `<Project>/DShader/Packages/dream-noise` | `/Game/DreamShaderPackages/dream-noise` |
| `<Plugin>/DShader/Packages/@scope/name` | `/<Plugin>/DreamShaderPackages/scope/name` |

The `@` is dropped and any character a package path cannot hold becomes `_`. A source under `Assets`
adds its folder under `Assets` (`Assets/Noise/MF_X.dss` → `<asset root>/Noise/MF_X`); a 1.x `Name=` with
no `Root=` is relative to the asset root; an explicit `Root` wins. Nothing a package builds lands in
the project's own folders, so removing a package leaves the project's assets alone.

### The `Assets` folder

Any source the language has can sit under `Assets` and is built like the project's own — a `.dss`
exporting a function, a 1.x `.dsf` with a `ShaderFunction`, even a `.dss` material or a `.dsi` instance.
It includes the package's headers by relative path:

```hlsl
// Packages/@typedreammoon/dreamshader-blender/Assets/MF_BL_TexNoise3D.dss
#include "../Library/Noise.dsh"

/// @library Blender|Noise
export void MF_BL_TexNoise3D(float3 co, float scale, float detail, float roughness,
                             float lacunarity, float distortion, float normalize01,
                             out float fac, out float3 color)
{
    BL::TexNoise3D(co, scale, detail, roughness, lacunarity, distortion, normalize01, fac, color);
}
```

```c
// Packages/@typedreammoon/dreamshader-blender/Assets/MF_BL_Checker.dsf
import "../Library/Checker.dsh";

ShaderFunction(Name = "MF_BL_Checker")
{
    Inputs  = { vec3 co; float scale = 5.0; }
    Outputs = { float fac; }
    Graph   = { BL::Checker(co, scale, fac); }
}
```

A `.dsp` under `Assets` is not built: Custom Pass pipelines are not package assets.

### Asset headers

Writing a wrapper per function is tedious for a library of fifty. Instead, a header can mark the
functions it wants generated, and the plugin writes the wrappers itself:

| | One function | Every function below / in the block |
| :-- | :-- | :-- |
| 2.0 | `/// @generate MF_Circle` on the function | `#pragma generate(Functions = "MF_{Name}")` |
| 1.x | `Function(Generate = "MF_Circle") Circle(...)`, also on `GraphFunction` | `Namespace(Name = "BL", Generate = "MF_BL_{Name}")` |
| Category in the Material Editor's function library | `/// @library Cat\|Sub` · `Library = "Cat\|Sub"` | the same key on the pragma or the `Namespace` |
| Description | `/// @desc ...` · `Description = "..."` | `Desc = "..."` on the pragma · `Description = "..."` on the `Namespace` |
| Opting one function out | `/// @generate none` | `Function(Generate = "")` |

```c
// Packages/@typedreammoon/dreamshader-blender/Library/Noise.dsh — 1.x
Namespace(Name = "BL", Generate = "MF_BL_{Name}", Library = "Blender|Noise")
{
    Function TexNoise3D(in vec3 co, in float scale, ..., out float fac, out vec3 color) { ... }
    Function(Generate = "") NoiseHelper(in float x, out float y) { ... }   // not generated
}
```

```hlsl
// Packages/@scope/shapes/Library/Shapes.dsh — 2.0
/// @generate Shapes/MF_Circle
/// @library Shapes
float Circle(float2 uv, float radius = 0.5) { return step(length(uv - 0.5), radius); }

#pragma generate(Functions = "MF_{Name}", Library = "Shapes")
float Ring(float2 uv) { return Circle(uv, 0.4) - Circle(uv, 0.3); }
/// @generate none
float Helper(float x) { return x; }
```

- `{Name}` is the function's own name — a namespace member's without the namespace. It is the only
  placeholder ([`DSH6332`](../diagnostics/DSH6xxx.md#dsh6332) in a 1.x header,
  [`DSH8273`](../diagnostics/DSH8xxx.md#dsh8273) in a pragma).
- A name may hold folders: `Shapes/MF_Circle` is `<asset root>/Shapes/MF_Circle`.
- A pragma reaches the helper functions after it — no `export`, no `extern`, a body — until the next
  `#pragma generate(...)`; `#pragma generate()` ends it.
- The material function has an input per `in` parameter (its default, if any, is the input's preview
  value), an output per `out` parameter, and a return value's output when the function returns one. A
  `/// @static` or `/// @param` on the function carries over.
- What cannot be one is [`DSH8274`](../diagnostics/DSH8xxx.md#dsh8274): an `export` or `extern` function, a
  function with no body, an array parameter. Two functions asking for the same asset is
  [`DSH8275`](../diagnostics/DSH8xxx.md#dsh8275). An unknown key of the pragma is
  [`DSH8276`](../diagnostics/DSH8xxx.md#dsh8276), of a 1.x function header
  [`DSH6331`](../diagnostics/DSH6xxx.md#dsh6331) — warnings.
- The annotations mean something in a package's header only. Anywhere else — a project header, a
  `.dsm` — `@generate` is [`DSH7234`](../diagnostics/DSH7xxx.md#dsh7234) and nothing is generated; the
  project writes an `export` function or a `ShaderFunction` instead.

The header is compiled as a `.dss` the plugin writes in memory — an `#include` of the header and one
`export` wrapper per function — so everything after that is the ordinary compile of an exported
function. The assets name the **header** as their source: *Revert to Source*, the build key, the cook
and the file watcher all come back to it.

### Updating, hand edits and removal

| | What happens |
| :-- | :-- |
| The package is updated | Every source the update changed rebuilds its assets on its build key, as a project source does |
| Someone edited a generated function in the Material Editor | The [hand-edit guard](../generation/divergence.md) refuses to rebuild it until *Revert to Source*; *Adopt Into Source* is refused, [`DSH8133`](../diagnostics/DSH8xxx.md#dsh8133) — the package's files are its installer's, and an update would overwrite what was adopted |
| A function stops being generated, or the package is removed | Its asset stays. `list-generated -All` names every asset under a package asset root that no source builds any more, [`DSH9111`](../diagnostics/DSH9xxx.md#dsh9111); delete it once nothing uses it |

Package `.dsh` headers are fully importable but are never scanned for `VirtualFunction` declarations,
so a `VirtualFunction` block shipped in a package is never validated or refreshed against its
`UMaterialFunction` asset. See [VirtualFunction tools](virtual-function-tools.md).

## `dreamshader.package.json` — extension convention

Placed at the root of a package. **Not read by the plugin.** The fields below are the convention the
VSCode extension's installer and store use; the extension repository is authoritative.

| Field | Type | Purpose |
| :-- | :-- | :-- |
| `name` | string | Package identity; plain `name` or scoped `@scope/name`. |
| `version` | string | Package version; SemVer recommended. |
| `displayName` | string | Human-readable name shown in the store. |
| `description` | string | Short description shown in the store. |
| `author` | string | Author attribution. |
| `repository` | string | Git URL; used to update an installed package. |
| `license` | string | SPDX identifier. |
| `dreamshader.language` | string | Language identity, `DreamShaderLang`. |
| `dreamshader.version` | string | DreamShaderLang version range the package targets. |
| `dreamshader.entry` | string | Recommended entry header, for documentation and store display. |
| `keywords` | string[] | Store search terms. |

A package repository is discoverable in the store by carrying the GitHub topic `dreamshader-package`.

## `dreamshader.lock.json` — extension convention

| | |
| :-- | :-- |
| Path | `<SourceDirectory>/dreamshader.lock.json` |
| Written by | the extension, on install and update |
| Read by | the extension |
| Records | package name, version, repository, commit, install path |

**Neither read nor written by the plugin.** Deleting it changes nothing about compilation; it exists
so a team can see which package revisions are checked in.

## Extension commands

Command-palette entries provided by the VSCode extension. Install and update require a working `git`
on `PATH`. Names are the extension's, not the plugin's.

| Command |
| :-- |
| `DreamShaderLang: Install Package from GitHub` |
| `DreamShaderLang: Browse Package Store` |
| `DreamShaderLang: Update Installed Packages` |
| `DreamShaderLang: Remove Installed Package` |
| `DreamShaderLang: Open Packages Folder` |
| `DreamShaderLang: Add Package Store Index Source` |
| `DreamShaderLang: Remove Package Store Index Source` |
| `DreamShaderLang: Create Package Step by Step` |

Install accepts either a `owner/repo` shorthand or a full `https://github.com/owner/repo` URL.

## Notes

- The package directory is created at module startup even when it is empty and even in a commandlet
  process, alongside the source directory and the generated-shader directory.
- Package files are ordinary files. Nothing prevents a package `.dsh` from being edited in place; the
  extension's update command overwrites it.
- Because the exclusion is a path test, a symbolic link or junction that points a non-package path at
  package content is treated as whatever its resolved path is.
- The plugin never downloads anything. Installation is entirely the extension's `git` work.

## Example

A package installed at `DShader/Packages/@typedreammoon/dream-noise/`, consumed by a project material:

```c
import "@typedreammoon/dream-noise/Library/Noise.dsh";

Shader(Name="Materials/M_Noise")
{
    Properties = {
        float Scale = 4.0 [Slider(0.1, 32)];
    }

    Outputs = {
        vec3 Color;
        Base.EmissiveColor = Color;
    }

    Graph = {
        vec2  UV = UE.TexCoord(Index = 0);
        float N  = FBM(UV * Scale);
        Color = vec3(N, N, N);
    }
}
```

Resolution of the specifier:

```text
specifier   @typedreammoon/dream-noise/Library/Noise.dsh
candidate 1 <Project>/DShader/Materials/@typedreammoon/dream-noise/Library/Noise.dsh   missing
candidate 2 <Project>/DShader/@typedreammoon/dream-noise/Library/Noise.dsh             missing
candidate 3 <Project>/DShader/Packages/@typedreammoon/dream-noise/Library/Noise.dsh    resolved
```

Dropping the extension resolves identically:

```c
import "@typedreammoon/dream-noise/Library/Noise";
```

## See also

- [import](../language/import.md) — the directive's grammar, how a header is read, cycle detection
- [Source files](../language/source-files.md) — what `.dsm`, `.dsh` and `.dsf` may each contain
- [Workspace and editor extensions](workspace.md) — the extensions that own the installer and store
- [Commandlet](commandlet.md) — `compile -All`, which uses the full enumerator
- [Editor bridge](bridge.md) — the recompile requests that use the material enumerator
- [VirtualFunction tools](virtual-function-tools.md) — the sync pass package headers are excluded from
- [Material Content Browser](material-browser.md) — the source list package files are absent from
- [Project settings](../settings/project.md) — `SourceDirectory`, which the package root hangs off
- [Path](../parameters/path.md) — asset-path roots, a different resolver from import roots
