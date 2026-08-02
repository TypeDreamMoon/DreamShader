# Packages

> [DreamShader](../index.md) » [Tools](index.md) » **Packages**

`DShader/Packages` — the install root for reusable DreamShaderLang libraries, resolvable by `import`
and excluded from most of the plugin's source enumeration.

| | |
| :-- | :-- |
| Kind | directory convention |
| Path | `<SourceDirectory>/Packages` — default `<Project>/DShader/Packages` |
| Created by | the `DreamShader` runtime module at startup, unconditionally |
| Configurable | only indirectly, through the *Source Directory* project setting |
| Implemented by | the plugin: the import root and the enumeration exclusion. Everything else: the editor extensions |

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
            │  └─ Noise.dsh              importable
            └─ Examples/
               └─ M_NoisePreview.dsm     never compiled by the plugin
```

The directory name is hard-coded: the package root is always the literal subdirectory `Packages` of
the configured source directory. Changing *Source Directory* moves it; there is no separate package
directory setting.

## Division of labour

The Unreal plugin implements two things about packages, and nothing else. Everything under
"extension" is a convention defined and enforced by the [editor extensions](workspace.md#editor-extensions),
in their own repositories, and cannot be observed from the plugin.

| Aspect | Implemented by |
| :-- | :-- |
| Creating `<SourceDirectory>/Packages` at editor startup | **plugin** |
| `<SourceDirectory>/Packages` as the third `import` resolution root | **plugin** |
| Excluding package files from source enumeration | **plugin** |
| `dreamshader.package.json` — existence, fields, validation | extension |
| `dreamshader.lock.json` — existence, contents, updates | extension |
| Installing, updating, removing packages | extension |
| Package store, index sources, GitHub topic search | extension |
| Package scaffolding / "create package" flows | extension |
| Scoped (`@scope/name`) naming | extension |

> [!NOTE]
> No C++ in this plugin reads `dreamshader.package.json` or `dreamshader.lock.json`. A package is
> resolvable purely because its files exist under `<SourceDirectory>/Packages`; deleting the manifest
> does not break `import`, and adding one does not change how the plugin behaves. The manifest and
> lock file exist for the extensions' installer and store.

## Import resolution

An `import` specifier is normalized, then tried against three candidate roots in order. The first
candidate that both stays inside its own root and exists on disk wins. This resolver is shared by the
compiler's import inliner and by the editor's dependency scanner, so both agree.

### Specifier normalization

| Step | Rule |
| :-- | :-- |
| 1 | Leading and trailing whitespace trimmed |
| 2 | Every `\` replaced with `/` |
| 3 | **All** leading `./` sequences stripped (repeatedly, so `././X` becomes `X`) |
| 4 | If the result has no extension, `.dsh` is appended |

Step 4 means `import "@scope/pkg/Library/Noise"` and `import "@scope/pkg/Library/Noise.dsh"` are the
same import. An extensionless specifier can never resolve to a `.dsf`.

### Candidate roots

| Order | Candidate path | Root it must stay under |
| :-- | :-- | :-- |
| 1 | `<directory of the importing file>/<specifier>` | the importing file's import root — whichever of `<SourceDirectory>` or `<SourceDirectory>/Packages` contains it (longest match), or the file's own directory if neither does |
| 2 | `<SourceDirectory>/<specifier>` | `<SourceDirectory>` |
| 3 | `<SourceDirectory>/Packages/<specifier>` | `<SourceDirectory>/Packages` |

Because candidate 1 is rooted at `Packages` for a file that lives inside a package, a `.dsh` inside a
package can reach its siblings with a relative specifier but cannot climb out of the package tree
with `../`; the escape guard rejects the candidate and resolution falls through to roots 2 and 3.

A specifier that resolves under no root produces
`DreamShader import '{Specifier}' referenced from '{File}' could not be resolved.` The full `import`
grammar, cycle handling and line-number mapping are on [import](../language/import.md).

## Source-file enumeration

The plugin has two enumerators. They exclude different things, and that asymmetry decides which
package files are compiled.

| Enumerator | Extensions scanned | Excluded | Sorted |
| :-- | :-- | :-- | :-- |
| Full source enumeration | `.dsm`, `.dsh`, `.dsf` | **everything** under `<SourceDirectory>/Packages` | yes |
| Material source enumeration | `.dsm`, `.dsf` | only `.dsm` files under `<SourceDirectory>/Packages` | no |

The exclusion test is a path-prefix comparison against the package directory, case-insensitive, and
it matches the package directory itself as well as anything beneath it at any depth.

### Which enumerator each feature uses

| Feature | Enumerator | Sees package `.dsm` | Sees package `.dsf` / `.dsh` |
| :-- | :-- | :-- | :-- |
| Startup in-memory generation | full | no | no |
| [Commandlet](commandlet.md) `compile -All` | full | no | no |
| Cook-time materialization | full | no | no |
| Material Content Browser, Gen page list | full | no | no |
| VirtualFunction declaration sync | full | no | no |
| *Recompile DSM* / full rescan queue | material | no | **`.dsf` yes** |
| Dependency-graph rebuild | material | no | **`.dsf` yes** |

> [!WARNING]
> The `Packages` exclusion is complete for `.dsm` but only partial for `.dsf`. The auto-compile paths
> test for "a `.dsm` under `Packages`", which no `.dsf` can satisfy — so a `.dsf` inside a package is
> queued by *Recompile DSM*, is recompiled when the file watcher sees it saved, and participates in
> the dependency graph, while remaining invisible to `compile -All`, to cook, to the Gen page and to
> VirtualFunction sync. A package that ships `.dsf` function assets will therefore generate those
> assets in an interactive editor session and **not** generate them in a headless build. Ship library
> code as `.dsh` headers, or copy the `.dsf` out of `Packages` into the project's own source tree.

> [!WARNING]
> `Examples/**/*.dsm` inside a package is never compiled by any path, and never appears in the
> Material Content Browser's Gen page. To use an example, copy it out of `DShader/Packages` into
> `DShader/` (or any subdirectory of it that is not `Packages`).

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

- [import](../language/import.md) — the directive's grammar, cycle detection and line mapping
- [Source files](../language/source-files.md) — what `.dsm`, `.dsh` and `.dsf` may each contain
- [Workspace and editor extensions](workspace.md) — the extensions that own the installer and store
- [Commandlet](commandlet.md) — `compile -All`, which uses the full enumerator
- [Editor bridge](bridge.md) — the recompile requests that use the material enumerator
- [VirtualFunction tools](virtual-function-tools.md) — the sync pass package headers are excluded from
- [Material Content Browser](material-browser.md) — the Gen page listing package files are absent from
- [Project settings](../settings/project.md) — `SourceDirectory`, which the package root hangs off
- [Path](../parameters/path.md) — asset-path roots, a different resolver from import roots
