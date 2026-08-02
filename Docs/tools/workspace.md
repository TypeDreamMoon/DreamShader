# Workspace and editor extensions

> [DreamShader](../index.md) » [Tools](index.md) » **Workspace and editor extensions**

A generated VSCode workspace file that maps the three DreamShaderLang file extensions onto the
`dreamshaderlang` language id, and the editor command that writes it and launches an external editor
on it.

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

`<SourceDirectory>` is the *Source Directory* project setting, default `<Project>/DShader`. The
directory is created if it does not exist. The file is written UTF-8 without a BOM, pretty-printed by
Unreal's JSON writer (tab indentation):

```json
{
	"folders": [
		{
			"name": "DreamShader Source",
			"path": "."
		}
	],
	"settings": {
		"files.associations": {
			"*.dsm": "dreamshaderlang",
			"*.dsh": "dreamshaderlang",
			"*.dsf": "dreamshaderlang"
		}
	}
}
```

## Contents

Every key the writer emits. There are no others, and nothing is conditional.

| Key | Value | Purpose |
| :-- | :-- | :-- |
| `folders[0].name` | `DreamShader Source` | Display name of the single workspace folder. |
| `folders[0].path` | `.` | The folder containing the workspace file, i.e. `<SourceDirectory>` itself. |
| `settings["files.associations"]["*.dsm"]` | `dreamshaderlang` | Language id for material sources. |
| `settings["files.associations"]["*.dsh"]` | `dreamshaderlang` | Language id for headers. |
| `settings["files.associations"]["*.dsf"]` | `dreamshaderlang` | Language id for function sources *(since 1.3.5)*. |

> [!WARNING]
> The file is rewritten from scratch on **every** invocation of *Open Dream Shader Workspace*. The
> writer serializes a fixed object; it never reads, merges or preserves the existing file. Any
> `launch`, `tasks`, `extensions` or extra `settings` entries added by hand are destroyed the next
> time the command runs. Keep per-user workspace configuration in a different `.code-workspace` file
> or in `<SourceDirectory>/.vscode/settings.json`, neither of which this command touches.

## Open behaviour

The command performs four steps, in this order:

| Step | Action | On failure |
| :-- | :-- | :-- |
| 1 | Re-export `material-expressions.json` | logged, command continues |
| 2 | Re-export `settings.json` | logged, command continues |
| 3 | Re-export `substrate-builtins.json` | logged, command continues |
| 4 | Write `DreamShader.code-workspace` | toast + warning, command **aborts** |
| 5 | Launch an editor on the workspace file (fallback chain below) | toast + warning |

Steps 1–3 are the same three manifests the [bridge](bridge.md#manifests) writes at editor startup;
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
| *Open source* (Material Content Browser, Gen page) | file launcher | always `--reuse-window` |
| *OpenVirtualFunction* (asset context menu) | file launcher | always `--reuse-window`, positioned at the declaration's line and column |
| *Export DSM* / *Export DSF* post-export open | preferred-editor chain | always `--reuse-window` when VSCode is used |

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

Every artifact an extension reads or writes lives under `<Project>/Saved/DreamShader/Bridge/`, plus
the loopback WebSocket endpoint. The full schemas are on [Editor bridge](bridge.md).

| Artifact | Direction | Contents |
| :-- | :-- | :-- |
| `Requests/*.json` | extension → editor | Recompile, clean and one-shot preview commands; see [request files](bridge.md#request-files). |
| `diagnostics.json` | editor → extension | All current diagnostics, grouped by source file. |
| `diagnostics/index.json` + `diagnostics/<md5>.json` | editor → extension | The same data sharded per file, for incremental reads. |
| `bridge.db` | editor → extension | SQLite mirror of the diagnostics and the three manifests. |
| `material-expressions.json` | editor → extension | Reflected `UMaterialExpression` catalogue for `UE.Expression` completion *(since 1.2.10)*. |
| `settings.json` | editor → extension | `ShadingModel` / `BlendMode` / `MaterialDomain` alias tables. |
| `substrate-builtins.json` | editor → extension | `Substrate.*` builtin catalogue with snippets; `supported: false` below UE 5.4. |
| `preview.json` + `Preview/*.png` | editor → extension | Result manifest and image for a one-shot preview. |
| `ws://127.0.0.1:17864` | bidirectional | Streaming preview with orbit control; see [WebSocket protocol](bridge.md#websocket-server). |

## Example

Running *Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* on a default project touches:

```text
<Project>/DShader/DreamShader.code-workspace                    rewritten
<Project>/Saved/DreamShader/Bridge/material-expressions.json    rewritten
<Project>/Saved/DreamShader/Bridge/settings.json                rewritten
<Project>/Saved/DreamShader/Bridge/substrate-builtins.json      rewritten
<Project>/Saved/DreamShader/Bridge/bridge.db                    tables replaced
```

and then launches, for a `code.cmd` candidate with *Open In New Window* left at its default:

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd"  "C:/Projects/MyGame/DShader/DreamShader.code-workspace""
```

With *Open In New Window* turned off, the same line carries the extra flag:

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd" --reuse-window "C:/Projects/MyGame/DShader/DreamShader.code-workspace""
```

## See also

- [Editor bridge](bridge.md) — the artifacts, request files and WebSocket protocol an extension drives
- [Packages](packages.md) — `DShader/Packages`, and which half of the package system the extension owns
- [Editor integration](editor-integration.md) — the Tools menu and toolbar entries that invoke this command
- [Material Content Browser](material-browser.md) — the *Open source* action that uses the file launcher
- [VirtualFunction tools](virtual-function-tools.md) — *OpenVirtualFunction* and its line/column jump
- [Decompiler](decompiler.md) — *Export DSM* / *Export DSF* and the post-export open
- [Project settings](../settings/project.md) — `SourceDirectory` and `bOpenInNewWindow`
- [Source files](../language/source-files.md) — what `.dsm`, `.dsh` and `.dsf` may each contain
- [Release](../contributing/release.md) — how extension assets are attached to a plugin release
