---
title: VSCode and Rider
description: The language extensions, the generated DreamShader.code-workspace, and the bridge artifacts the two sides exchange.
---

DreamShaderLang ships as **two independent products**: the Unreal plugin, and a language extension
for your editor. They never call each other directly — they exchange files under
`<Project>/Saved/DreamShader/Bridge/` and one loopback WebSocket. Knowing which side owns what saves
a lot of time when something does not work.

| Side | Owns |
| :-- | :-- |
| Unreal plugin | compilation, diagnostics, the bridge artifacts, the preview renderer, `DreamShader.code-workspace` |
| Editor extension | syntax highlighting, completion, hovers, navigation, the preview panel and its camera, package install/update, the package manifest and lock file |

## Which side implements what

| Surface | Implemented by | Notes |
| :-- | :-- | :-- |
| Menus, toolbar, context menus, the browser tab | plugin | [Editor Tools](editor-integration.md) |
| Auto-compile-on-save, the diagnostics store | plugin | writes three diagnostic sinks |
| Preview rendering (the PNG frames) | plugin | the renderer, the mesh set and the clamps |
| Preview camera control, pitch clamping, frame acknowledgement | **extension** | the plugin applies no pitch clamp of its own |
| Decompiler | plugin | [Decompiler](decompiler.md) |
| `.dsm` / `.dsh` / `.dsf` and `.dss` / `.dsi` / `.dsp` highlighting, completion, hovers | **extension** | fed by the manifests, the symbol index and the builtin catalog the plugin exports |
| `DreamShader.code-workspace` | plugin | rewritten on every *Open Dream Shader Workspace* |
| `dreamshader.package.json`, `dreamshader.lock.json`, install/update commands | **extension** | **no plugin C++ reads either file** — [Packages](packages.md) |
| `DShader/Packages` creation, import resolution, auto-compile exclusion | plugin | [Packages](packages.md) |

## The extensions

Neither extension ships inside the plugin. Install one separately.

| Editor | Repository | Provides |
| :-- | :-- | :-- |
| VSCode | [TypeDreamMoon/dreamshader-language-support](https://github.com/TypeDreamMoon/dreamshader-language-support) | Syntax highlighting, snippets, completion, Go to Definition, Find References, Hover, Signature Help, local diagnostics, Unreal bridge diagnostics, package commands, quick templates, the material preview panel; currently 2.1.0, with `.dsp` support |
| JetBrains Rider | [tsdaer/dreamshader-language-support](https://github.com/tsdaer/dreamshader-language-support) | `.dsm` / `.dsf` / `.dsh` file types, grammar and PSI parsing, highlighting, completion, navigation, diagnostics, Unreal Bridge integration, semantic tokens, inlay hints, package tools |

| Capability | VSCode | Rider |
| :-- | :-- | :-- |
| File type recognition | yes | yes |
| Syntax highlighting | yes | yes |
| Parser model | parser-based language service | JetBrains PSI parser |
| Completion, hover, signature help | yes | yes |
| Go to definition, find references | yes | yes |
| Local diagnostics | yes | yes |
| Unreal bridge diagnostics | yes | yes |
| Inlay hints / semantic tokens | yes | yes |
| Material preview panel | yes | plugin-dependent |
| Package commands | yes | plugin-dependent |
| Authoring templates | yes | plugin-dependent |

The release workflow attaches the latest VSCode extension assets to each plugin GitHub release, so
the two versions stay roughly in step. Extension-side settings — a project-root override, a preview
frame rate, package store index URLs — are declared by the extension, not by `UDreamShaderSettings`,
and the extension's own repository is authoritative for them.

## The generated workspace

*Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* — and the toolbar button of the same
name — writes one file and launches an editor on it. (since 1.2.1)

| Aspect | Value |
| :-- | :-- |
| Path | `<SourceDirectory>/DreamShader.code-workspace` — `<Project>/DShader/` by default |
| Encoding | UTF-8 without BOM, pretty-printed by Unreal's JSON writer (tab indentation) |
| Platform | **Windows only** — discovery uses Windows environment variables, a `;`-separated `PATH`, `cmd.exe` and `notepad.exe` |

The source directory is created if it does not exist. The file looks like this:

```json
{
	"folders": [
		{
			"name": "DreamShader Source",
			"path": "."
		},
		{
			"name": "Plugin: MoonToon",
			"path": "../Plugins/MoonToon/DShader"
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
file lives in it), then one entry per plugin that ships a `DShader` folder, named `Plugin: <Name>` and
pointed at a path relative to the workspace file (an absolute one when there is no relative form — an
engine plugin on another drive). Every key the writer emits follows; there are no others, and only the
number of `folders` entries varies.

| Key | Value | Purpose |
| :-- | :-- | :-- |
| `folders[0].name` | `DreamShader Source` | display name of the project root's folder |
| `folders[0].path` | `.` | the folder holding the workspace file — `<SourceDirectory>` itself |
| `folders[n].name`, `folders[n].path` | `Plugin: <Name>`, a path relative to the workspace file | one per plugin source root, in root order |
| `settings["files.associations"]["*.dss"]` | `dreamshader-dss` | language id for 2.0 compilation units (since 2.1.0) |
| `settings["files.associations"]["*.dsi"]` | `dreamshader-dss` | language id for material instance sources (since 2.1.0) |
| `settings["files.associations"]["*.dsp"]` | `dreamshader-dsp` | language id for Custom Pass pipelines (since 2.1.0). Without it a `.dsp` is claimed by whatever else knows the extension — the Faust language and old Visual C++ projects use it too |
| `settings["files.associations"]["*.dsm"]` | `dreamshaderlang` | language id for material sources |
| `settings["files.associations"]["*.dsh"]` | `dreamshaderlang` | language id for headers |
| `settings["files.associations"]["*.dsf"]` | `dreamshaderlang` | language id for function sources (since 1.3.5) |

Three language ids, the ones the extension registers: `dreamshaderlang` for the 1.x sources and headers,
`dreamshader-dss` for 2.0 sources, `dreamshader-dsp` for pipelines (extension 2.1.0 and later). A workspace
association overrides the extension's own mapping of an extension to a language, so the two have to agree:
with `dreamshaderlang` for a `.dss`, VS Code would give it the 1.x grammar and language server. Settings
scoped to a language — `"[dreamshader-dss]": { ... }` — use the same ids.

**Warning:**

**The file is rewritten from scratch on every invocation.** The writer serializes a fixed object; it
never reads, merges or preserves what was there. Any `launch`, `tasks`, `extensions` or extra
`settings` entries you added by hand are destroyed the next time the command runs. Keep per-user
configuration in a *different* `.code-workspace` file, or in `<SourceDirectory>/.vscode/settings.json`
— the command touches neither.

### What the command does, in order

| Step | Action | On failure |
| :-- | :-- | :-- |
| 1 | Re-export `material-expressions.json` | logged, the command continues |
| 2 | Re-export `settings.json`, and `pass-keys.json` right after it (since 2.1.0) | logged, the command continues |
| 3 | Re-export `substrate-builtins.json` | logged, the command continues |
| 4 | Write `DreamShader.code-workspace` | toast + warning, the command **aborts** |
| 5 | Launch an editor on the workspace file | toast + warning |

Steps 1–3 rewrite the same three manifests the bridge writes at editor startup, so an extension
installed a minute ago sees current data without an editor restart. See
[Bridge artifacts](bridge.md).

### Launch fallback chain

The first mechanism that succeeds wins; the rest are not attempted.

| Order | Mechanism | Detail |
| :-- | :-- | :-- |
| 1 | VSCode | the first discovered executable that yields a valid process handle. `.cmd` / `.bat` candidates run through `%ComSpec%` (falling back to `C:/Windows/System32/cmd.exe`) with `/C`, hidden; `.exe` candidates are spawned directly |
| 2 | Shell default application | `LaunchFileInDefaultExternalApplication` with the `Edit` verb — whatever is registered for `.code-workspace` |
| 3 | Notepad | `%SystemRoot%\System32\notepad.exe` if it exists, otherwise bare `notepad.exe` |
| — | *(none)* | failure toast and a warning in the log |

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
| 9 | for each `;`-separated `PATH` entry, in `PATH` order: `code.cmd`, `code.exe`, `Code.exe`, `code-insiders.cmd`, `Code - Insiders.exe` |

**Note:**

There is **no setting that names a VSCode executable.** A non-standard install is reachable only by
putting it on `PATH`.

### Which launcher an action uses

| Aspect | Value |
| :-- | :-- |
| Setting | *Open In New Window* — `bOpenInNewWindow`, category *Editor* |
| Default | `true` |
| Effect | when **false**, ` --reuse-window` is appended to the VSCode command line. When true, no flag is passed and VSCode applies its own default |

**Warning:**

`bOpenInNewWindow` is consulted by the **workspace launcher only**. Every other DreamShader action
that opens a file in VSCode uses a separate launcher that always passes
`--reuse-window -g "<path>:<line>:<column>"`, regardless of the setting.

| Action | Launcher | Window behaviour |
| :-- | :-- | :-- |
| *Open Dream Shader Workspace (VSCode)*, menu and toolbar | workspace launcher | honours `bOpenInNewWindow` |
| *Open source* (Material Content Browser) | file launcher | always `--reuse-window` |
| *OpenVirtualFunction* (asset context menu) | file launcher | always `--reuse-window`, positioned at the declaration's line and column |
| *Export .dss* / *.dsi* / *Legacy .dsm* / *Legacy .dsf* post-export open | preferred-editor chain | always `--reuse-window` when VSCode is used |

The file launcher clamps line and column to `1` or greater, and its own fallback chain is
VSCode → shell default application (`Edit` verb) → Notepad, the same shape as the workspace chain.

### Diagnostics

Toast text and log text differ; both are listed. Runtime substitutions are written `{Placeholder}`.

| Toast | Log | Cause |
| :-- | :-- | :-- |
| `DreamShader failed to create workspace: {Error}` | Warning — `Failed to create DreamShader workspace: {Error}` | the workspace file could not be written; `{Error}` is one of the three writer errors below |
| `Opened DreamShader workspace in VSCode: {Path}` | Display — same text | a VSCode candidate launched |
| `Opened DreamShader workspace: {Path}` | Display — `Opened DreamShader workspace with the default editor: {Path}` | the shell default application launched |
| `Opened DreamShader workspace in Notepad: {Path}` | Display — same text | Notepad launched |
| `DreamShader could not open workspace: {Path}` | Warning — `Failed to open DreamShader workspace: {Path}` | every mechanism failed; the file was still written |

| Writer error | Cause |
| :-- | :-- |
| `DreamShader source directory is empty.` | the resolved source directory normalized to an empty string |
| `Failed to create DreamShader source directory '{Path}'.` | the source directory did not exist and could not be created |
| `Failed to write DreamShader workspace file '{Path}'.` | the file could not be saved — read-only, locked, out of space |

## What an extension consumes

Every artifact an extension reads or writes lives under `<Project>/Saved/DreamShader/Bridge/`, plus
the loopback WebSocket endpoint.

| Artifact | Direction | Contents |
| :-- | :-- | :-- |
| `Requests/*.json` | extension → editor | recompile, clean, one-shot preview and `reveal-node` commands |
| `diagnostics.json` | editor → extension | all current diagnostics, grouped by source file |
| `diagnostics/index.json` + `diagnostics/<md5>.json` | editor → extension | the same data sharded per file, for incremental reads |
| `bridge.db` | editor → extension | SQLite mirror of the diagnostics and the three manifests |
| `material-expressions.json` | editor → extension | reflected `UMaterialExpression` catalogue for [`UE.Expression`](../builtins/ue-expression.md) completion (since 1.2.10) |
| `settings.json` | editor → extension | `ShadingModel` / `BlendMode` / `MaterialDomain` alias tables |
| `substrate-builtins.json` | editor → extension | [`Substrate.*`](../builtins/substrate.md) catalogue with snippets; `supported: false` below UE 5.4 |
| `pass-keys.json` (since 2.1.0) | editor → extension | what a `.dsp` may say, for completion: injection points, pipeline / buffer / pass keys, value spellings, the project's pass layer names; `supported: false` below UE 5.8. No `bridge.db` table |
| `dreamshader-builtin-catalog.json` (since 2.0.0) | editor → extension | the binder's own node catalog — the source of `UE.` / `Substrate.` completion; `dsc export-catalog` writes it too |
| `../Index/<root>/<source>.index.json` (since 2.0.0) | editor → extension | each source's symbol index, refreshed after every compile, a failed one included |
| `preview.json` + `Preview/*.png` | editor → extension | result manifest and image for a one-shot preview |
| `status.json` | editor → extension | heartbeat and identity — **a missing file means "no editor is running"** (since 1.6.0) |
| `Responses/<requestId>.json` | editor → extension | the answer to a request that carried a `requestId` (since 1.6.0) |
| `owner.lock` | editor-internal | which editor owns this project's bridge (since 1.8.0) |
| `ws://127.0.0.1:17864` | bidirectional | streaming preview with orbit control and breakpoints |

**Warning:**

`bridge.db` is **write-only from the plugin's side** and is not durable state: it is deleted on bridge
startup *and* on shutdown, and every writer replaces its whole table inside a transaction. Nothing in
the plugin ever reads a row back. Treat it as a query-friendly mirror of the JSON, valid only while
the editor is running — never as a place to store client state.

### Is the editor alive?

(since 1.6.0)

A client's only way to guess whether an editor was running used to be looking for `bridge.db` and
hoping. The bridge now rewrites `status.json` every 2 s and **deletes it on shutdown** — so a missing
file means "not running", definitively, and a client can fall back immediately instead of waiting out
a liveness window.

| Field | Notes |
| :-- | :-- |
| `protocol`, `pid`, `project`, `projectDir`, `engineDir`, `pluginVersion` | identity |
| `busy` | true while a request is being served |
| `busyAction` | present only when busy |
| `lastResult` | present once something has completed |
| `heartbeatUtc` | ISO-8601 UTC |

**Warning:**

Read `busy` **before** judging the heartbeat's age. A compile blocks the game thread, so the heartbeat
stops while one runs — a client that only looks at the timestamp reads every real compile as a crash.
The remaining ambiguity is a hard crash, which leaves the file behind; the recorded `pid` is what
closes it.

**Note:**

Requests written while nobody was listening are **discarded**, not queued. The cutoff is the bridge's
own start time, so a request that was waiting on disk when an editor starts is deleted with a log line
rather than served late — by then whoever sent it has long since timed out. Check `status.json` before
writing a request.

### Request files

| Aspect | Value |
| :-- | :-- |
| Directory | `<Project>/Saved/DreamShader/Bridge/Requests/` |
| Discovery | `*.json`, files only, **non-recursive** |
| Poll interval | `0.1 s` |
| Consumption | every discovered file is **deleted** at the end of its loop iteration |

The filename is irrelevant; only the JSON contents matter. `action` and `scope` are both matched
case-insensitively.

| `action` | Required fields | Effect | Answered |
| :-- | :-- | :-- | :-- |
| `ping` (since 1.6.0) | — | nothing; confirms the bridge is serving | immediately |
| `recompile` | `scope: "all"` | rebuild the dependency graph and queue every source of every root — `.dsm`, `.dsf`, `.dss`, `.dsi`, `.dsp`, except a `.dsm` under a root's `Packages` — forced | immediately, as *queued* |
| `recompile` | `scope: "file"`, `sourceFile` | queue one file into the debounce queue, forced | **when that compile finishes** |
| `cleanGeneratedShaders` | — | delete the generated `*.ush` includes, then queue a full rescan | immediately |
| `previewMaterial` | `sourceFile` | render one preview synchronously and write `preview.json`; `sourceFile` is a `.dss`, `.dsi` or `.dsm` | immediately, with the render result |
| `reveal-node` (since 2.0.0) | `file`, `line` | find the node generated from that line, open its asset's Material Editor and select it | immediately, with the asset path and expression GUIDs |

A request carrying a `requestId` is answered in `Responses/<requestId>.json` (since 1.6.0), with
`protocol`, `requestId`, `ok`, `durationMs`, `message` and `diagnostics`. It is written atomically —
beside the target, then renamed — so appearing and being complete are the same event; stale responses
from a previous session are deleted at startup.

**Warning:**

`recompile` with `scope: "file"` is the only action whose response is **deferred**. The file goes into
the debounce queue and is compiled some ticks later, so answering at dispatch time would report
success before anything had been attempted. The id is parked and answered from the compile itself,
carrying that compile's diagnostics.

`scope: "all"` is the opposite and says so: it answers immediately with *queued*, because the batch
drains across ticks and the outcome is not knowable yet. **Do not read that as a result.**

**Note:**

A bad `scope`, a missing `sourceFile`, or an unrecognized `action` is now an **error response** rather
than a silent no-op — provided the request carried a `requestId`. Without one the behaviour is
unchanged: the file is deleted and nothing is said. That is what every request the shipped VSCode
extension sends looks like, and is why it keeps working unchanged.

A request is deleted **before** it is dispatched, so one that crashes the editor cannot be replayed on
every subsequent start. The one exception is a file that cannot be read at all: that is almost always
one still being written, so it is left for the next poll rather than thrown away. Writing the JSON to
a temporary name elsewhere and **renaming** it into `Requests/` is still the right way to make it
appear atomically.

A request may carry `protocol`. A **missing** field is read as the current version, not as a mismatch
— every request the shipped VSCode extension sends omits it, and rejecting those would break it on
every machine that has it installed. A field that is present *and* different is refused:

```text
Protocol 3 is not understood; this editor speaks 1. Update the DreamShaderLang extension or the
plugin so the two match.
```

Compilation through this path always leaves a ThinCustom product Ephemeral; a queued file compiles after the debounce window
(*Save Debounce Seconds*, clamped to `[0.05, 10.0]`, default `0.25`) plus up to `0.1 s` of poll
delay, and only if it still exists on disk.

### Diagnostics the extension reads

`diagnostics.json` has the shape
`{ "version": 1, "updatedAtUtc": "…", "files": [ { "path": "…", "diagnostics": [ … ] } ] }`. Optional
fields are omitted entirely when empty.

| Field | Presence | Value |
| :-- | :-- | :-- |
| `message` | always | the diagnostic text |
| `detail` | when non-empty | the raw underlying line |
| `stage` | when non-empty | a compile's record: `preprocess`, `parse`, `bind`, `ir`, `generate`, `tools` or `compile`, from its code's range; otherwise `generate`, `materialCompile` or `virtualFunctionSync` |
| `assetPath` | when non-empty | object path of the asset involved |
| `shaderPlatform`, `qualityLevel` | when non-empty | material-compile diagnostics only |
| `code` | when non-empty | a compile's record: its `DSHnnnn` code (since 2.0.0); otherwise `generate-error`, `material-compile` or `virtual-function-sync` |
| `line`, `column` | always | 1-based, defaulting to `1` |
| `severity` | always | `error`, `warning` or `info` (since 2.1.0) |
| `source` | always | `DreamShader Lang2`, `DreamShader`, `DreamShader Generate`, `DreamShader Material Compile` or `DreamShader VirtualFunction` |

**Note:**

A compile files its own records — each with its `DSHnnnn` code, severity and position: a failed compile all
of them, **a successful one its warnings and notes** (since 2.1.0) (before, a successful compile
cleared the file's diagnostics and its warnings were only in the log). A client that filters on severity
should treat a missing or unknown value as an error.

**Note:**

**Diagnostics belong to the source and the run that filed them** (since 2.1.1). There are three
kinds of run: the compile of a source, the shader compile of one material it generated, and the startup
VirtualFunction scan of a file. Compiling `A.dsm` replaces exactly what A's previous compile filed —
against A and against any header A imports — and leaves what `B.dsm` filed against the same header alone;
a successful compile's warnings also survive its material's shader compile. Before, compiling A wiped B's
error in a shared `.dsh` until B compiled again. Deleting a file removes everything it owns and everything
filed against it.

A compile's record carries its own line and column. For `generate-error`, locations are recovered from messages shaped `<path>(<line>,<column>): <message>`; a line with no
parseable location is reported at `1,1`. Material-compile diagnostics carry a display message of the
form `[{ShaderPlatform} / {QualityLevel}] {Message}`. Since UE 5.7 `shaderPlatform` carries a
shader-format name such as `PCD3D_SM6`; below 5.7 it carries a feature-level name such as `SM6`.

### Streaming preview

The preview panel connects to `ws://127.0.0.1:17864` and drives the plugin's renderer.

| Message | Direction | Purpose |
| :-- | :-- | :-- |
| `previewMaterial` | client → editor | start a preview session for a `.dss`, `.dsi` or `.dsm` (since 2.0.0). `encoding` selects `raw` or `png`; `force` controls regeneration (since 1.7.0) |
| `previewControl` | client → editor | adjust the active session — streaming, frame rate, camera, `width` / `height` / `mesh` (since 1.7.0) |
| `setProbe` / `clearProbe` | client → editor | attach or detach a breakpoint-style probe (since 1.7.0) |
| `previewResult` | editor → client | session start result, or a mid-stream error |
| `previewFrame` | editor → client | *(png encoding only)* metadata for the PNG that follows |
| `probeState` | editor → client | whether the probe is `attached`, `pending`, or `cleared` (since 1.7.0) |

Every **outbound** message is a WebSocket *binary* frame whose payload is a 4-byte little-endian
length, a 1-byte type tag (`1` = UTF-8 JSON, `2` = raw PNG, `3` = raw RGBA8 frame
(since 1.7.0)), then the payload. Inbound messages are plain UTF-8 JSON with no prefix and no
tag.

Streaming is both rate-limited and acknowledgement-gated: a new frame starts only when the client has
acknowledged the previous one **and** the frame interval has elapsed.

**Note:**

**The default encoding is `raw` since (since 1.7.0).** A tag-`3` frame is self-describing: a
24-byte header (size, flags, camera, resolved probe line) followed by the pixels, with no PNG encode
on the editor side, and the client paints it straight onto a canvas. That is what makes smooth
30–60 FPS affordable. The old `previewFrame`-plus-tagged-PNG pair is still available as
`encoding: "png"` for older clients, unchanged.

When nothing is moving, identical frames are dropped, and after a few in a row the render clock backs
off to a low idle rate until an edit, a camera/mesh change, or a probe change re-arms it.

**Warning:**

**Old behaviour (before `1.7.0`):** omitting `frameRate` from a `previewControl` silently dropped the
session to 2 FPS — the reader initialized it to `2.0` before looking for the field, so a control
message sent purely to acknowledge a frame or nudge the camera was enough to hit it.

Since `1.7.0`, **an omitted field keeps its current value** (`frameRate` included); only `requestId`
and `ackFrameIndex` are exempt. A client that must also work against older plugins can keep sending
`frameRate` on every `previewControl` — correct under both versions.

### Graph breakpoints

(since 1.7.0)

`setProbe` hands the editor a line number — a `Graph` line in a `.dsm`, a statement of the material's body in a `.dss` (since 2.0.0) — and the preview mesh then shows the **value bound
at that line** instead of the finished material — the text analogue of the Material Editor's
right-click *Start Previewing Node*. A breakpoint on a blank or comment line snaps forward to the next
line that binds a value; one set before the source has ever generated is remembered as *pending* and
attaches on the next compile; after a recompile the probe re-resolves automatically and `probeState`
tells the client the line it landed on. The line is also echoed per frame in the tag-`3` frame header.
The semantics are on [Editor Tools](editor-integration.md).

A session example, client side:

```text
→ {"type":"previewMaterial","sourceFile":"…/M_Sample.dsm","mesh":"shaderball",
   "width":512,"height":512,"requestId":"8f3c1b","stream":true,"frameRate":12,
   "encoding":"raw"}
← [len][1] {"type":"previewResult","requestId":"8f3c1b","status":"ready", … }
← [len][3] <24-byte header><RGBA8 pixels>
→ {"type":"previewControl","requestId":"8f3c1b","ackFrameIndex":0,"orbitYaw":-140.0}
← [len][3] <24-byte header><RGBA8 pixels>
→ {"type":"setProbe","requestId":"8f3c1b","line":31}
← [len][1] {"type":"probeState","requestId":"8f3c1b","state":"attached","line":31}
← [len][3] <24-byte header><RGBA8 pixels>      // now showing the value bound on line 31
```

The renderer's own limits — material and material-instance sources only (`.dss`, `.dsi`, `.dsm`), the size clamp, the mesh fallback, the missing pitch clamp —
are on [Editor Tools](editor-integration.md).

## DreamShader 2.0 sources

(since 2.0.0) `.dss` and `.dsi` files open as the language **DreamShader 2.0** (language id
`dreamshader-dss`, extension 2.0.0), with a grammar of their own, snippets, folding on `#pragma region` and
`#if`, and `///` that continues on Enter. `.dsp` files open as `dreamshader-dsp` (extension 2.1.0)
(since 2.1.0), completed from `pass-keys.json`, with a `pipeline` section in their symbol index — the
language itself is on [Custom Pass (.dsp)](../language-v2/passes.md).

The extension does not read 2.0 text for meaning. 2.0 has overloads, implicit conversions and a binder
that knows the engine's node classes; a second implementation of that in JavaScript would be wrong in
exactly the places that matter. So the editor asks the compiler:

| Feature | Answered from |
| :-- | :-- |
| Go to Definition · Find References · Hover · Outline · Signature Help · Rename | the **symbol index** the plugin writes after every compile: `Saved/DreamShader/Index/<root>/<file>.index.json` |
| completion and hover for `UE.` and `Substrate.` — node names, pins, properties, the Substrate sugar arguments | the **builtin catalog** the plugin exports once the editor has loaded: `Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json` |
| diagnostics | the bridge, as for 1.x |

Answers are as fresh as the last compile, and the plugin compiles on save. A position the text no longer
matches is dropped rather than shown, and a rename refuses and asks for a compile. The index lists what a
file declares at file scope, not the locals of a body, and records mentions by name — so only a name
that is unique at file scope is renamed.

**Reveal in Material Editor** sends the bridge's `reveal-node` request for the current line: the running
editor opens the owning asset's material editor and selects the node that line became. The other
direction is in the plugin — a generated node's context menu has *DreamShader ▸ Open Source Line*.

## VSCode commands

Names are the extension's, not the plugin's, and are surfaced under the `DreamShaderLang` group.

```text
DreamShaderLang: Recompile Current Source
DreamShaderLang: Recompile All Sources
DreamShaderLang: Clean Generated Shaders
DreamShaderLang: Show Bridge Panel
DreamShaderLang: Refresh Bridge Diagnostics
DreamShaderLang: Show Material Preview
DreamShaderLang: Install Package from GitHub
DreamShaderLang: Browse Package Store
DreamShaderLang: Update Installed Packages
DreamShaderLang: Remove Installed Package
DreamShaderLang: Open Packages Folder
DreamShaderLang: Add Package Store Index Source
DreamShaderLang: Remove Package Store Index Source
DreamShaderLang: Create Package Step by Step
DreamShaderLang: Create DreamShader Material
DreamShaderLang: Create DreamShader Function File
DreamShaderLang: Create DreamShader Header
DreamShaderLang: Create DreamShader Texture Sample
DreamShaderLang: Create DreamShader Noise Material
```

## VSCode settings

Extension-declared, so the extension repository is authoritative. Keep the workspace pointed at the
Unreal project root — or set `dreamshader.projectRoot` — so the extension can resolve `DShader`,
`DShader/Packages` and `Saved/DreamShader/Bridge`.

| Setting | Default | Use |
| :-- | :-- | :-- |
| `dreamshader.projectRoot` | *(auto)* | Unreal project root, when the workspace is not opened there |
| `dreamshader.previewWebSocketPort` | `17864` | the plugin's preview WebSocket port — it is fixed on the plugin side |
| `dreamshader.previewAutoRefreshDelayMs` | `1200` | delay before saving and refreshing after an edit |
| `dreamshader.previewTransport` | `websocket` | use the WebSocket, or force `file` bridge requests |
| `dreamshader.previewLiveFrameRate` | `2` | maximum streamed FPS; `0` disables continuous frames |
| `dreamshader.packageStoreIndexUrls` | *(default index)* | one or more package store index JSON URLs |
| `dreamshader.enableGitHubPackageSearch` | `true` | also search GitHub for the `dreamshader-package` topic |

```json
{
  "dreamshader.packageStoreIndexUrls": [
    "https://raw.githubusercontent.com/TypeDreamMoon/dreamshader-package-index/main/packages.json"
  ],
  "dreamshader.enableGitHubPackageSearch": true
}
```

## Notes

- The plugin's `previewWebSocketPort` counterpart is **not configurable**: the server always binds
  `127.0.0.1:17864`, and a connection from any other address is refused. Two editors on one machine
  cannot both serve previews — the second logs a listen warning and runs without streaming.
- Every diagnostic carries the line and column of the file its construct is in: one raised inside an
  imported header names the header (since 2.0.0) (a header is parsed on its own, no longer inlined
  into the file that imports it).
- A Material Content Browser source row shows only the **first** error per file; the inspector's
  *Diagnostics* section and the extension show all of them. See
  [Editor Tools](editor-integration.md#material-submenu).

## Example

Running *Tools ▸ DreamShader ▸ Open Dream Shader Workspace (VSCode)* on a default project touches:

```text
<Project>/DShader/DreamShader.code-workspace                    rewritten
<Project>/Saved/DreamShader/Bridge/material-expressions.json    rewritten
<Project>/Saved/DreamShader/Bridge/settings.json                rewritten
<Project>/Saved/DreamShader/Bridge/pass-keys.json               rewritten (since 2.1.0)
<Project>/Saved/DreamShader/Bridge/substrate-builtins.json      rewritten
<Project>/Saved/DreamShader/Bridge/bridge.db                    tables replaced
```

and then launches, for a `code.cmd` candidate with *Open In New Window* at its default:

```powershell
%ComSpec% /C ""C:/Users/<user>/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd"  "C:/Projects/MyGame/DShader/DreamShader.code-workspace""
```

Asking a running editor to recompile one file, without VSCode:

```powershell
$req = @{ action = "recompile"; scope = "file"; sourceFile = "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm" }
$dir = "C:\Projects\MyGame\Saved\DreamShader\Bridge\Requests"
$tmp = Join-Path $env:TEMP ("ds-" + [guid]::NewGuid() + ".json")
$req | ConvertTo-Json | Set-Content -Path $tmp -Encoding utf8
Move-Item $tmp (Join-Path $dir ([IO.Path]::GetFileName($tmp)))
```

## Where next

  - [Editor Tools](editor-integration.md) — The menu entry that writes the workspace, and the bridge artifacts.
  - [Packages](packages.md) — The half of the package system the extension owns.
  - [Commandlet](commandlet.md) — Compiling with no editor and no bridge at all.
  - [Installation](../getting-started/installation.md) — Installing the plugin and one of the extensions.
  - [Diagnostics](../diagnostics/index.md) — Every message the bridge can publish.
