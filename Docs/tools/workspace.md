# Workspace and editor extensions

> [DreamShader](../index.md) » [Tools](index.md) » **Workspace and editor extensions**

A generated VSCode workspace file that maps the six DreamShaderLang file extensions onto the language
ids of the [VS Code extension](https://github.com/TypeDreamMoon/dreamshader-language-support), and the
editor command that writes it and launches an external editor on it.

| | |
| :-- | :-- |
| Kind | editor command + generated file |
| Writes | `<SourceDirectory>/DreamShader.code-workspace` |
| Command | *Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)*, and the toolbar button of the same name |
| Platform | Windows only — discovery uses Windows environment variables, `;`-separated `PATH`, `cmd.exe` and `notepad.exe` |
| Since | `1.2.1` |

## Synopsis

```text
<SourceDirectory>/DreamShader.code-workspace
```

`<SourceDirectory>` is the project's source directory: *Project Settings ▸ Dream Source ▸ Shader ▸
Source Directory*, default `<Project>/DreamSource/Shader`, or the legacy `<Project>/DShader` while only
that one exists — see [DreamSource layout](../settings/project.md#dreamsource-layout). The
directory is created if it does not exist. The file is written UTF-8 without a BOM, pretty-printed by
Unreal's JSON writer (tab indentation):

```json
{
	"folders": [
		{
			"name": "DreamShader Source",
			"path": "."
		},
		{
			"name": "Plugin: MoonToon",
			"path": "../../Plugins/MoonToon/DreamSource/Shader"
		}
	],
	"settings": {
		"files.associations": {
			"*.dss": "dreamshader-dss",
			"*.dsi": "dreamshader-dss",
			"*.dsp": "dreamshader-dsp",
			"*.dsm": "dreamshaderlang",
			"*.dsh": "dreamshaderlang",
			"*.dsf": "dreamshaderlang"
		}
	}
}
```

One `folders` entry per source root: the project root is always first and always `"."` (the workspace
file lives in it), then one entry per plugin that ships a `DreamSource/Shader` (or legacy `DShader`) folder, named `Plugin: <Name>`
and pointed at a path relative to the workspace file. A root that has no relative form — an engine
plugin on a different drive — gets its absolute path instead. See
[Source files](../language/source-files.md#source-roots).

## Contents

Every key the writer emits. There are no others; only the number of `folders` entries varies.

| Key | Value | Purpose |
| :-- | :-- | :-- |
| `folders[0].name` | `DreamShader Source` | Display name of the project root's folder. |
| `folders[0].path` | `.` | The folder containing the workspace file, i.e. `<SourceDirectory>` itself. |
| `folders[n].name`, `folders[n].path` | `Plugin: <Name>`, a path relative to the workspace file (absolute when there is none) | One per plugin source root, in root order. |
| `settings["files.associations"]["*.dss"]` | `dreamshader-dss` | Language id for 2.0 compilation units *(since 2.1.0)*. |
| `settings["files.associations"]["*.dsi"]` | `dreamshader-dss` | Language id for material instance sources *(since 2.1.0)*. |
| `settings["files.associations"]["*.dsp"]` | `dreamshader-dsp` | Language id for Custom Pass pipelines *(since 2.1.0)*. Without it a `.dsp` is claimed by whatever else knows the extension -- the Faust language and old Visual C++ projects use it too. |
| `settings["files.associations"]["*.dsm"]` | `dreamshaderlang` | Language id for material sources. |
| `settings["files.associations"]["*.dsh"]` | `dreamshaderlang` | Language id for headers. |
| `settings["files.associations"]["*.dsf"]` | `dreamshaderlang` | Language id for function sources *(since 1.3.5)*. |

Three language ids, the ones the extension registers: `dreamshaderlang` for the 1.x sources and headers,
`dreamshader-dss` for 2.0 sources, `dreamshader-dsp` for pipelines (extension 2.1.0 and later). A workspace
association overrides the extension's own mapping of an extension to a language, so the two have to agree: with
`dreamshaderlang` for a `.dss`, VS Code would give it the 1.x grammar and language server. Settings scoped to a
language -- `"[dreamshader-dss]": { ... }` -- use the same ids.

> [!WARNING]
> The file is rewritten from scratch on **every** invocation of *Open Dream Shader Workspace*. The
> writer serializes a fixed object; it never reads, merges or preserves the existing file. Any
> `launch`, `tasks`, `extensions` or extra `settings` entries added by hand are destroyed the next
> time the command runs. Keep per-user workspace configuration in a different `.code-workspace` file
> or in `<SourceDirectory>/.vscode/settings.json`, neither of which this command touches.

## Open behaviour

The command performs five steps, in this order:

| Step | Action | On failure |
| :-- | :-- | :-- |
| 1 | Rewrite the `material-expressions` document of `shader.db` | logged, command continues |
| 2 | Rewrite the `settings` document, and [`pass-keys`](#pass-keys) right after it *(since 2.1.0)* | logged, command continues |
| 3 | Rewrite the `substrate-builtins`, `preprocessor-defines` and `builtin-catalog` documents | logged, command continues |
| 4 | Write `DreamShader.code-workspace` | toast + warning, command **aborts** |
| 5 | Launch an editor on the workspace file (fallback chain below) | toast + warning |

Steps 1–3 are the same manifests the [bridge](bridge.md#documents) writes at editor startup;
opening the workspace refreshes them so an extension that has just been installed sees current data
without an editor restart.

### Launch fallback chain

The first mechanism that succeeds wins; the rest are not attempted.

| Order | Mechanism | Detail |
| :-- | :-- | :-- |
| 1 | VSCode | The first discovered executable that yields a valid process handle. `.cmd` / `.bat` candidates are run through `%ComSpec%` (falling back to `C:/Windows/System32/cmd.exe`) with `/C`, hidden; `.exe` candidates are spawned directly. |
| 2 | Shell default application | `FPlatformProcess::LaunchFileInDefaultExternalApplication` with the `Edit` verb — whatever is registered for `.code-workspace`. |
| 3 | Notepad | `%SystemRoot%\System32\notepad.exe` if it exists, otherwise bare `notepad.exe`. |
| — | *(none)* | Failure toast and a warning in the log. |

### VSCode executable discovery

Probed in this exact order. Only paths that exist as files are kept, and duplicates are dropped.

| Order | Candidate |
| :-- | :-- |
| 1 | `%LOCALAPPDATA%\Programs\Microsoft VS Code\Code.exe` |
| 2 | `%LOCALAPPDATA%\Programs\Microsoft VS Code\bin\code.cmd` |
| 3 | `%LOCALAPPDATA%\Programs\Microsoft VS Code Insiders\Code - Insiders.exe` |
| 4 | `%LOCALAPPDATA%\Programs\Microsoft VS Code Insiders\bin\code-insiders.cmd` |
| 5 | `%ProgramFiles%\Microsoft VS Code\Code.exe` |
| 6 | `%ProgramFiles%\Microsoft VS Code\bin\code.cmd` |
| 7 | `%ProgramFiles(x86)%\Microsoft VS Code\Code.exe` |
| 8 | `%ProgramFiles(x86)%\Microsoft VS Code\bin\code.cmd` |
| 9 | For each `;`-separated `PATH` entry, in `PATH` order: `code.cmd`, `code.exe`, `Code.exe`, `code-insiders.cmd`, `Code - Insiders.exe` |

There is no setting that names a VSCode executable. A non-standard install is reachable only by
putting it on `PATH`.

## The reuse-window setting

| | |
| :-- | :-- |
| Setting | *Open In New Window* — `bOpenInNewWindow`, category *Editor* |
| Default | `true` |
| Effect | When **false**, ` --reuse-window` is appended to the VSCode command line. When true, no flag is passed and VSCode applies its own default. |

> [!NOTE]
> `bOpenInNewWindow` is consulted by the workspace launcher **only**. Every other DreamShader action
> that opens a file in VSCode uses a separate launcher that always passes
> `--reuse-window -g "<path>:<line>:<column>"`, regardless of the setting. Turning *Open In New
> Window* on does not make *Open source*, *OpenVirtualFunction* or the post-export open use a new
> window.

| Action | Launcher | Window behaviour |
| :-- | :-- | :-- |
| *Open Dream Shader Workspace (VSCode)* (menu and toolbar) | workspace launcher | honours `bOpenInNewWindow` |
| *Open source* (Material Content Browser) | file launcher | always `--reuse-window` |
| *OpenVirtualFunction* (asset context menu) | file launcher | always `--reuse-window`, positioned at the declaration's line and column |
| *Export .dss* / *.dsi* / *Legacy .dsm* / *Legacy .dsf* post-export open | preferred-editor chain | always `--reuse-window` when VSCode is used |

The file launcher clamps line and column to `1` or greater. Its own fallback chain is VSCode → shell
default application (`Edit` verb) → Notepad, the same shape as the workspace chain.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}`. Toast text and log text differ; both are listed.

| Toast | Log | Cause |
| :-- | :-- | :-- |
| `DreamShader failed to create workspace: {Error}` | `Warning` — `Failed to create DreamShader workspace: {Error}` | the workspace file could not be written; `{Error}` is one of the three writer errors below |
| `Opened DreamShader workspace in VSCode: {Path}` | `Display` — same text | a VSCode candidate launched |
| `Opened DreamShader workspace: {Path}` | `Display` — `Opened DreamShader workspace with the default editor: {Path}` | the shell default application launched |
| `Opened DreamShader workspace in Notepad: {Path}` | `Display` — same text | Notepad launched |
| `DreamShader could not open workspace: {Path}` | `Warning` — `Failed to open DreamShader workspace: {Path}` | every mechanism in the chain failed; the file was still written |

Writer errors, substituted into `{Error}` above:

| Message | Cause |
| :-- | :-- |
| `DreamShader source directory is empty.` | the resolved source directory normalized to an empty string |
| `Failed to create DreamShader source directory '{Path}'.` | the source directory did not exist and could not be created |
| `Failed to write DreamShader workspace file '{Path}'.` | the file could not be saved (read-only, locked, out of space) |

All logging goes to the `LogDreamShader` category.

## Editor extensions

The language tooling lives in two repositories, **neither of which ships inside this plugin**. The
plugin contributes the workspace file, the file associations and the [bridge](bridge.md) artifacts;
everything below — grammars, completion, navigation, package commands — is implemented by the
extension.

| Editor | Repository | Provides |
| :-- | :-- | :-- |
| VSCode | <https://github.com/TypeDreamMoon/dreamshader-language-support> | Syntax highlighting, snippets, completion, Go to Definition, Find References, Hover, Signature Help, local diagnostics, Unreal bridge diagnostics, package commands, quick templates. |
| JetBrains Rider | <https://github.com/tsdaer/dreamshader-language-support> | `.dsm` / `.dsf` / `.dsh` file types, grammar and PSI parsing, highlighting, completion, navigation, diagnostics, Unreal Bridge integration, semantic tokens, inlay hints, package tools. |

Extension-side settings (for example a project-root override, a preview frame rate, or package store
index URLs) are declared by the extension, not by `UDreamShaderSettings`, and are documented in the
extension's own repository. The release workflow attaches the latest VSCode extension assets to each
plugin GitHub release; see [Release](../contributing/release.md).

### What an extension consumes

An extension reads `<Project>/Intermediate/DreamSource/shader.db`, writes request files under
`<Project>/Intermediate/DreamSource/Shader/Bridge/`, and talks to the loopback WebSocket endpoint. The
full schemas are on [Editor bridge](bridge.md).

| Artifact | Direction | Contents |
| :-- | :-- | :-- |
| `Shader/Bridge/Requests/*.json` | extension → editor | Recompile, clean and one-shot preview commands; see [request files](bridge.md#request-files). |
| `shader.db` `diagnostics` | editor → extension | All current diagnostics, one row per source file. |
| `shader.db` `documents` `material-expressions` | editor → extension | Reflected `UMaterialExpression` catalogue for `UE.Expression` completion *(since 1.2.10)*; also row by row in `material_expressions`. |
| `shader.db` `documents` `settings` | editor → extension | `ShadingModel` / `BlendMode` / `MaterialDomain` alias tables; also row by row in `settings_mappings`. |
| `shader.db` `documents` `substrate-builtins` | editor → extension | `Substrate.*` builtin catalogue with snippets; `supported: false` below UE 5.4; also row by row in `substrate_builtins`. |
| `shader.db` `documents` `pass-keys` | editor → extension | What a `.dsp` may say, for completion; `supported: false` below UE 5.8 — [below](#pass-keys) *(since 2.1.0)*. |
| `shader.db` `documents` `preprocessor-defines`, `builtin-catalog` | editor → extension | The define table `#if` reads; the binder's node catalog. |
| `shader.db` `source_index` | editor → extension | The [symbol index](bridge.md#symbol-index) of every compiled source. |
| `shader.db` `documents` `preview` + `Shader/Preview/*.png` | editor → extension | Result and image for a one-shot preview. |
| `ws://127.0.0.1:17864` | bidirectional | Streaming preview with orbit control; see [WebSocket protocol](bridge.md#websocket-server). |

> [!NOTE]
> *(DreamSource layout)* An extension from before it reads `Saved/DreamShader/Bridge/` and finds nothing new
> there; update it with the plugin. An extension that knows the layout falls back to the old folder when
> `shader.db` does not exist, so it still works against an older plugin.

### `pass-keys`

*(since 2.1.0)* The vocabulary of a [`.dsp`](../language-v2/passes.md), for an editor's completion: the
`pass-keys` document of `<Project>/Intermediate/DreamSource/shader.db` (before the DreamSource layout,
`Saved/DreamShader/Bridge/pass-keys.json`), schema `DreamShader.PassKeys`, version `1`, with a
`generatedAt` timestamp. It is written with the `settings` document — at bridge startup and on *Open Dream
Shader Workspace* — because the layer names it ends with are a project setting. It has no row table.

| Key | Contents |
| :-- | :-- |
| `supported` | `true` on UE 5.8 and later. Below, `false` with `unsupportedReason`: `Custom Pass pipelines run on Unreal Engine 5.8 and later.` — a `.dsp` still parses and binds there, and nothing runs it |
| `injectionPoints` | every injection point in the order a frame reaches it: `name`, `order`, `postProcess`, and `resolution` (`Render` or `Output`) |
| `pipelineKeys` | the `#pragma pipeline` keys: `Order`, `Injection`, `Views`, `Requires`, `Enabled` |
| `bufferFormats` · `bufferKeys` · `bufferResolutions` | `R8` … `Depth32`; `Scale`, `Size`, `Resolution`, `Clear`, `Mips`, `History`, `Export`; `Render`, `Output` |
| `passKinds` | `fullscreen`, `compute`, `mesh`, `clear`, `copy`, each with its own `keys` and a `detail` |
| `commonPassKeys` | `Injection` and `Enabled`, which every kind takes |
| `bindingStatements` | `read`, `write`, `param` |
| `filterKinds` | `Stencil`, `Layer`, `List`, each with the `snippet` an editor inserts (`Stencil(${1:1})`) |
| `meshModes` · `depthModes` · `cullModes` · `blendModes` · `nanitePolicies` · `meshUsages` · `viewFlags` · `requirementFlags` · `parameterTypes` | the spellings each of those values takes |
| `builtinBuffers` | `SceneColor`, `SceneDepth`, `CustomDepth`, `CustomStencil`, `GBufferA` … `GBufferF`, `Velocity`, `Translucency` |
| `weightParameter` | `DreamPassWeight` |
| `layerNames` | the project's pass layers, in bit order — the names `Layer(...)` takes |

Every key entry is `name`, `value` (the kind of value, which names the list it is completed from — `injection`
for `injectionPoints`, …), `default` (what leaving the key out means), `required` (true when there is no
default) and `detail`.

## Example

Running *Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* on a default project touches:

```text
<Project>/DreamSource/Shader/DreamShader.code-workspace          rewritten
<Project>/Intermediate/DreamSource/shader.db                     documents material-expressions, settings,
                                                                 pass-keys (since 2.1.0), substrate-builtins,
                                                                 preprocessor-defines, builtin-catalog rewritten;
                                                                 the three row tables replaced
```

and then launches, for a `code.cmd` candidate with *Open In New Window* left at its default:

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd"  "C:/Projects/MyGame/DreamSource/Shader/DreamShader.code-workspace""
```

With *Open In New Window* turned off, the same line carries the extra flag:

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd" --reuse-window "C:/Projects/MyGame/DreamSource/Shader/DreamShader.code-workspace""
```

## See also

- [Editor bridge](bridge.md) — the artifacts, request files and WebSocket protocol an extension drives
- [Packages](packages.md) — `<SourceDirectory>/Packages`, and which half of the package system the extension owns
- [Editor integration](editor-integration.md) — the Tools menu and toolbar entries that invoke this command
- [Material Content Browser](material-browser.md) — the *Open source* action that uses the file launcher
- [VirtualFunction tools](virtual-function-tools.md) — *OpenVirtualFunction* and its line/column jump
- [Decompiler](decompiler.md) — the *Export* entries and the post-export open
- [Project settings](../settings/project.md) — `SourceDirectory` and `bOpenInNewWindow`
- [Source files](../language/source-files.md) — what `.dsm`, `.dsh` and `.dsf` may each contain
- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md) — the language the `pass-keys` document describes
- [Release](../contributing/release.md) — how extension assets are attached to a plugin release
