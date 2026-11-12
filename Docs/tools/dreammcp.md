# DreamMCP endpoint

> [DreamShader](../index.md) » [Tools](index.md) » **DreamMCP endpoint**

The HTTP endpoint a DreamMCP server reaches DreamShader through: an AI agent's `dream_shader` tool checks,
compiles and previews sources, reads their diagnostics, looks nodes up and decompiles assets, in the user's
editor when it is open and in a host process the server starts when it is not.

| | |
| :-- | :-- |
| Kind | editor service — `UE::DreamShader::Editor::MCP::FEndpoint`, in the `DreamShaderEditor` module (`Source/DreamShaderEditor/Private/MCP/`) |
| Protocol | DreamMCP protocol `1` |
| Manifest | `<Plugin>/Resources/DreamMCP.json` — the actions, their arguments as JSON Schema, the route, the host commandlet |
| Language references | `.skill/reference/dss.md`, `dsp.md`, `legacy.md`, named by the manifest's `reference` and packaged with the plugin (`Config/FilterPlugin.ini`). Each page stands alone: its links lead to the online manual, and an agent that cannot open them misses nothing it needs |
| Route | `http://127.0.0.1:<port>/dream/shader` |
| Port | `47800`, or `-DreamMCPPort=<n>`; a port that cannot be bound is skipped for the next, up to 16 |
| Bind address | `127.0.0.1` only — proven at startup, see [Loopback only](#loopback-only) |
| Authentication | the `X-Dream-Token` header on every request; the token is `DREAM_MCP_TOKEN` from the environment, else `-DreamMCPToken=<t>`, else random |
| Discovery | `<Project>/Saved/DreamMCP/shader.json`, written once the endpoint has answered a hello of its own, deleted when it stops |
| Hosts | the editor with `-DreamMCPHost -RenderOffscreen` (renders); `-run=DreamShaderHost -nullrhi -DreamMCPHost` (does not) |
| Module dependencies | `HTTPServer`, `Sockets` |
| Disabled by | `-NoDreamMCP`; in the user's editor also `-NoDreamShaderEditorBridge` and *Enable DreamMCP Endpoint* off |

The endpoint does nothing a source save, the [commandlet](commandlet.md) or the [preview](preview.md) does not
already do: every action forwards to the same compiler, renderer, node catalog and decompiler. What it adds is
one answer shape for all of them — an outcome, a reason code, diagnostics with codes and lines, the assets
written — so a server never has to read a log line.

## Which process serves it

| Process | Endpoint | `mode` | Renders |
| :-- | :-- | :-- | :-- |
| The user's editor, with the [bridge](bridge.md) running | yes, once the engine has initialized | `editor` | yes |
| A **render host**: the editor with `-DreamMCPHost -RenderOffscreen`, no `-run=` | yes | `host` | yes |
| A **headless host**: `-run=DreamShaderHost -nullrhi -DreamMCPHost` | yes, before the commandlet's `Main` | `host` | no |
| Any other commandlet — `-run=DreamShader compile`, a cook | no | — | — |
| An editor with `-NoDreamShaderEditorBridge` (an automation run) | no | — | — |

`-DreamMCPHost` is what makes a process a host, and every Dream plugin's endpoint honours it, so one host process
serves every Dream plugin, each under its own route on one shared port. A commandlet whose `-run=` is `Dream…Host`
counts as a host without the switch too. A commandlet never renders a preview scene, so the headless host reports
`render: false` whatever its switches; a server that needs a picture starts the render host.

In the user's editor, a compile through the endpoint is the editor's own compile: it goes through the bridge, so
the diagnostics store, the VSCode extension and the [Material Content Browser](material-browser.md) see it as they
see a save, and a ThinCustom material stays [Ephemeral](../generation/ephemeral.md). A host has no bridge and no
session to keep an Ephemeral material alive in, so `compile` there saves it, as the commandlet does.

## Starting a host

A DreamMCP server starts one of the two itself, with the token in the environment variable `DREAM_MCP_TOKEN`:

```text
render:    UnrealEditor-Cmd.exe <Project>.uproject [<map>] -DreamMCPHost -RenderOffscreen -unattended -nosplash -nosound -DreamMCPPort=<p>
headless:  UnrealEditor-Cmd.exe <Project>.uproject -run=DreamShaderHost -nullrhi -DreamMCPHost -unattended -nosplash -nopause -DreamMCPPort=<p>
```

| Switch | Effect |
| :-- | :-- |
| `-DreamMCPHost` | this process is a host: `mode: host`, `/shutdown` accepted, the parent process watched |
| `-DreamMCPPort=<n>` | the first port tried |
| `-DreamMCPToken=<t>` | the token when `DREAM_MCP_TOKEN` is not set in the environment; random when neither is. A server uses the environment, since a command line is visible to every process on the machine |
| `-DreamMCPParentPid=<pid>` | the process whose exit ends the host; by default the process that started it. `0` watches none |
| `-DreamMCPStartupTimeout=<s>` | a host whose engine loop has not turned within this many seconds of the plugin loading exits with code `3` instead of hanging on whatever holds it; `600` by default, `0` turns it off |
| `-NoDreamMCP` | no endpoint — a host then exits at once |
| `<map>` *(render host)* | the map the editor opens; a project whose default map is heavy starts faster on `/Engine/Maps/Entry` |

A host exits on `POST /dream/shader/shutdown` (a quarter of a second after answering), when the process that
started it goes away (checked every second), or on Ctrl+C, and takes its discovery file back as it goes.

The render host is an editor nobody sees, and it behaves like one:

| | |
| :-- | :-- |
| Window | none: no visible top-level window, no taskbar entry, never the foreground window |
| Dialogs | `-unattended` answers them; one that still held the startup trips `-DreamMCPStartupTimeout` |
| The editor's own parts | no bridge (no startup sweep, no file watcher, no preview WebSocket, no claim on the bridge's ownership), no browser tab, no menus |
| Settings | writes no config file (the config system's file operations are off), does not auto-save, and records the project it opened beside its discovery file rather than in the user's `ProjectEditorRecords.json` (UE 5.6+) |
| Idle | level viewports are not realtime, and the frame rate is capped at 15 while no job runs (an unattended editor is not throttled by the engine) |

## Discovery file

`<Project>/Saved/DreamMCP/shader.json`, written beside itself and moved into place, so it is never read half
written:

```json
{ "protocol": 1, "id": "shader", "pid": 23116, "port": 47811, "token": "…", "route": "/dream/shader",
  "ws": null, "mode": "host", "render": true, "engine": "5.8.2", "plugin": "2.2.0",
  "startedAt": "2026-11-12T10:00:00.000Z" }
```

| Rule | |
| :-- | :-- |
| When | only once the endpoint answers. Listening starts with the module, but nothing answers until the engine's main loop ticks the HTTP server — in an editor, seconds later, while it builds its UI. So the endpoint first asks its own `GET /dream/shader/hello`, with the token, over the socket from a worker thread, and writes the file on the first tick after a `200`; an unanswered hello is asked again half a second later. A file that exists names an endpoint that has answered |
| Starting | writes the file over whatever is there: the newest process for the project is the one to talk to. An editor opened while a host runs replaces the host's file, and the server moves to the editor once the host's current job is done |
| Stopping | deletes the file only while it still names this process, so a host that exits after an editor took over leaves the editor's file alone |

## Loopback only

The engine's `HTTPServer` binds wherever `[HTTPServer.Listeners]` in the project's Engine config says, and a
project may say `DefaultBindAddress=any` for a web interface of its own. The endpoint therefore puts
`(Port=<p>,BindAddress=127.0.0.1,ReuseAddressAndPort=false)` at the front of `ListenerOverrides` for the one bind
it makes and puts the section back right after, so nothing of it reaches a saved config.

It then proves the result: it opens a TCP connection to the port through every non-loopback IPv4 address the
machine has, and every one must be refused. A port that answers on one is not used:

```text
DreamMCP: port 47800 answers on 192.168.1.20:47800, not only on 127.0.0.1; it is not used. Check [HTTPServer.Listeners] in the project's Engine config.
```

That happens when another plugin bound the same port to every address earlier in the same process; the endpoint
moves on to the next port.

## Requests

| Method and path | Answers |
| :-- | :-- |
| `GET /dream/shader/hello` | `200` `{ protocol, id, plugin, engine, mode, render, busy, ws, pid, actions }` |
| `POST /dream/shader/call` | a short action: `200` and the [result](#the-result); a long one: `202 { id, job, state }` |
| `GET /dream/shader/jobs/<job>` | `200 { id, job, action, state, progress?, result? }` |
| `POST /dream/shader/jobs/<job>/cancel` | `200 { job, state, cancelled, pending?, summary }` |
| `POST /dream/shader/shutdown` | a host: `200` and exits; an editor: `409` `not-a-host` |

Every `POST` carries a body, `{}` when there is nothing to say: the engine's HTTP server answers a `POST` without
`Content-Length` with a `411` of its own, before the endpoint sees it.

`/call` takes `{ "id": "<caller's id>", "action": "<name>", "args": { … } }`. The arguments are checked
against the action's schema in the manifest before anything runs: an argument it does not declare, a wrong type,
a value outside its `enum`, `minimum` or `maximum`, or a missing `required` one is `400` `bad-args`, and the
summary names the argument. A second `/call` with the same `id` and action while the first job is still known
answers with that job instead of starting another.

Every refusal of the protocol itself has the result shape, with `outcome: "refused"`:

| Status | `reason` | When |
| :-- | :-- | :-- |
| `401` | `bad-token` | `X-Dream-Token` missing or wrong |
| `403` | `origin-not-allowed` | the request carries an `Origin` header — checked first, token or not |
| `400` | `bad-request` | the body is not a JSON object |
| `400` | `bad-args` | the arguments do not fit the action's schema |
| `404` | `unknown-action` | no such action; `data.actions` lists them |
| `404` | `unknown-route` / `unknown-job` | no such path / no such job in this process |
| `405` | `bad-method` | the wrong verb for the route |
| `409` | `not-a-host` | `/shutdown` sent to an editor |
| `202`, then the job's result | `render-unavailable` | a `needs: render` action on a process that cannot render: a long action is still a job, finished at once with the refusal |

### Jobs

A long action is a job. Requests and jobs both run on the game thread — the compiler creates objects — so the
request answers `202` at once and the job runs **one step per tick** afterwards: `hello` stays answerable while
it runs, and `busy` is `true` while any job is queued or running. One job runs at a time, in the order they came.

| `state` | Meaning |
| :-- | :-- |
| `queued` | waiting for the job ahead of it |
| `running` | `progress` is `{ current, total, message }`: `check` counts files, `preview` its stages (compile, shaders, warm-up, then one per frame); `message` names the stage |
| `done` | finished with a result: `ok`, `source-error` or `refused` |
| `failed` | finished with an `engine-error` result |
| `cancelled` | stopped on request; its result's `reason` is `cancelled` |

Cancelling a queued job is immediate. A running one stops at its next step — between two files of a
`check`, before a preview renders — and the answer says `pending: true`; a step already under way finishes first.
The last 64 finished jobs stay readable; a job does not survive the process.

## The result

```json
{
  "ok": false,
  "outcome": "source-error",
  "reason": null,
  "summary": "Checked DShader/MCPProbe/M_McpBroken.dss: 1 error(s), 0 warning(s).",
  "diagnostics": [
    { "code": "DSH4200", "severity": "error", "file": "DShader/MCPProbe/M_McpBroken.dss", "line": 4, "column": 23,
      "length": 13, "message": "'UndefinedTint' is not declared in this scope.", "stage": "bind",
      "doc": "https://shader.toolchain.64hz.cn/en/docs/plugin/diagnostics/dsh4xxx/#dsh4200" }
  ],
  "writes": [],
  "artifacts": [],
  "data": { "checked": 1, "errors": 1, "warnings": 0, "files": [ … ] },
  "next": [ "Fix the errors at the lines given, then check again." ]
}
```

| Field | |
| :-- | :-- |
| `outcome` | `ok`; `source-error` — the source is wrong, the diagnostics say where; `refused` — not the source's fault, `reason` says what; `engine-error` — something inside failed |
| `diagnostics[]` | `code`, `severity`, `file`, `line`, `column`, `message`; `length`, `hint` (the record's detail), `stage` and `doc` when known |
| `writes[]` | `{ asset, state }`: the package, and `created`, `saved` (it existed) or `memory` (an Ephemeral product) — read from the engine's own save events |
| `artifacts[]` | `{ kind, path, title }`: an `image` under `Saved/DreamMCP/artifacts/`, a decompiled `source` |

Every path is relative to the project directory, with forward slashes; no answer carries an absolute path.
Every sentence is invariant English whatever the editor's language — compiler messages go out through
`ToInvariantWireString`, as on the [bridge](bridge.md).

## Actions

| Action | Long | Writes | Arguments | Does |
| :-- | :-- | :-- | :-- | :-- |
| `check` | yes | no | `file`, or `all: true` | the pipeline to IR validation, as `dsc check`; nothing is built |
| `compile` | yes | assets | `file`; `force`, `shaders` | builds the source's assets; `shaders: true` takes `check -Shaders`'s route and maps HLSL errors to source lines (always saves) |
| `diagnostics` | no | no | `file` (optional) | what this process already holds: the editor's diagnostics store, else the last check or compile through the endpoint. Without `file`, the summary names each file with its counts, the files with errors first (ten at most, then "N more"); `data.files[]` has `errors`, `warnings`, `infos` for each. Only an error makes the outcome `source-error` |
| `lookup` | no | no | `name`; `limit` (1–200, default 20) | the [builtin node catalog](../builtins/ue-expression.md): an exact name, alias or `m.` attribute gives the entry as `export-catalog` writes it; anything else the names that contain the text |
| `preview` | yes | maybe | `file`; `mesh`, `width`, `height`, `yaw`, `pitch`, `force`, `time`, `frames`, `duration`, `tone` | compiles the material as the [preview](preview.md) does, waits for its shaders a tick at a time, renders a PNG to `Saved/DreamMCP/artifacts/<job>.png` and [measures it](#what-preview-measures); a shader error is a `source-error` with the image of the default material |
| `info` | no | no | — | plugin and engine versions, the source roots, the default backend, `mode`, `render`, whether Custom Pass and Substrate are compiled in |
| `decompile` | yes | a source file | `asset`; `out`, `readable`, `overwrite` | the [decompiler](decompiler.md), written as the commandlet writes it; refuses `file-exists` unless `overwrite` |

The refusal reasons the actions use: `file-not-found`, `not-a-source` (a `.dsh`, or not a DreamShader file),
`not-previewable`, `asset-not-found`, `not-decompilable`, `file-exists`, `asset-hand-edited` (the asset was
[edited by hand](../generation/divergence.md), `DSH8115` — `force` does not get past it), `strip-too-wide`
(`frames × width` over 8192 pixels), `cancelled`.
The engine errors: `compiler-unavailable`, `compile-failed`, `preview-compile-failed`, `render-failed`,
`artifact-not-written`, `source-not-written`.

### What `preview` measures

A model that cannot look at the image still has to know what it shows, so `preview` measures its pixels and says
the one fact that matters most in the first sentence of `summary`, with what to do about it in `next`. The
summary ends with the image's path.

Three renders go into the measurement, all with the same mesh, camera, time and tone: the material; the
background alone (the mesh drawn with nothing on it); the mesh's silhouette (unlit magenta, two-sided). The
background and the silhouette are rendered once per job, at the first frame's time — a World Position Offset that
moves the mesh is not followed. All three are drawn once and thrown away first: a material that has never been
drawn asks for its shaders when it is, and draws as the engine's default material until they are in.

| Term | Means |
| :-- | :-- |
| mesh pixel | the silhouette render differs from the background in some channel by more than **32** (of 255) |
| transparent | a mesh pixel whose every channel is within **4** of the background: the material lets the background through |
| foreground | a mesh pixel that is not transparent — what the material draws |
| near white | a foreground pixel with every channel at **240** or above |
| near black | a foreground pixel with every channel at **12** or below |
| luminance | Rec. 709 weights (0.2126, 0.7152, 0.0722) on the PNG's 0–1 values — the gamma-encoded image as it looks, not scene light |
| flat | luminance and every channel with a standard deviation of **0.012** or less |

`data.stats` (all frames together) and `data.frames[].stats` (each frame) hold:

| Field | |
| :-- | :-- |
| `coverage` | share of the image the mesh covers, 0–1 |
| `transparent` | share of the mesh that is transparent, 0–1 |
| `foreground` | share of the image that is foreground, 0–1 |
| `meanColor`, `meanColorName` | the foreground's mean, `#RRGGBB`, and a plain name for it (`orange`, `pinkish grey`, `brown`; see below) |
| `luminance` | `{ min, max, mean, stdDev }` of the foreground, 0–1 |
| `nearWhite`, `nearBlack` | shares of the foreground, 0–1 |
| `flat` | the foreground is one colour |

Colour names go by HSV value and saturation before hue: saturation under 0.12 is a grey (`white`, `light grey`,
`grey`, `dark grey`, `black`); under 0.4 a grey with a tint, which a hue name would overstate (`pinkish grey`,
`light bluish grey`, `warm grey`); above it a hue (`red`, `orange`, `dark blue`, `pale green`), with
orange and yellow under value 0.5 called `brown`.

The first rule that holds gives the summary's fact:

| Rule | Fact | `next` |
| :-- | :-- | :-- |
| coverage under 0.5% | the mesh is not in the picture | check `yaw`, `pitch`, `mesh`, a World Position Offset |
| transparent 95% or more | the material is invisible | check Opacity, OpacityMask and the blend mode |
| near black 90% or more | the material renders black | an Unlit material shows only EmissiveColor; a lit one needs BaseColor |
| near white 30% or more | the material is mostly white (`tone: "linear"`: clipped at 1) | bright values are pressed toward white; `tone: "linear"` reads the values |
| flat | one flat colour, with its `#RRGGBB` | check what should make it vary |
| otherwise | the mean colour and the luminance range, and the see-through share when it is 5% or more | — |

`tone` decides how colour reaches the image:

| `tone` | Exposure | Tone curve | Use it to |
| :-- | :-- | :-- | :-- |
| `editor` (default) | the project's, as the engine's thumbnail views use it: a thumbnail has no view history, so it is the exposure the engine fixes from the auto-exposure range | filmic, with bloom | see what the [preview](preview.md) shows the user |
| `fixed` | 1 (EV100 0), whatever the project's settings | filmic, with bloom | compare across projects with different exposure settings |
| `linear` | 1 (EV100 0) | none and no bloom — the tone mapper's gamma-only path, gamma 2.2 | read the material's output: emissive (1, 0, 0) is `#FF0000`, a value of 1 or more clips to white |

Measured with Unlit emissive greys on the plugin's test host, whose exposure settings are the defaults (there
`editor` and `fixed` draw the same):

| Emissive | 0.05 | 0.18 | 0.5 | 1 | 2 | 4 | (1, 0, 0) |
| :-- | --: | --: | --: | --: | --: | --: | :-- |
| `editor`, `fixed` | 54 | 117 | 178 | 208 | 227 | 240 | (227, 0, 6) |
| `linear` | 65 | 117 | 186 | 255 | 255 | 255 | (255, 0, 0) |

The filmic curve compresses: nothing clips, and bright colours drift toward white — that is what an
emissive-heavy preview looks like, and `data.toneNote` says so on every result.

`time` sets the material's Time (both clocks, with and without *Ignore Pause*) in seconds; default `0`, so a
preview of the same source is the same image every time. `frames` renders that many moments spread evenly
over `duration` seconds from `time` — frame *i* at `time + duration × i / frames` — side by side in one PNG, each
`width × height` (default 256 with `frames`, 512 without). `data.animation` is `{ frames, duration,
maxDifference, changes }`: `maxDifference` is the largest mean per-pixel difference from the first frame (0–255),
and `changes` is whether it is above 0.5. A strip whose frames are all the same says so in the summary.

## Example

```bash
T=$(jq -r .token Saved/DreamMCP/shader.json); P=$(jq -r .port Saved/DreamMCP/shader.json)
curl -s -H "X-Dream-Token: $T" http://127.0.0.1:$P/dream/shader/hello
curl -s -H "X-Dream-Token: $T" -d '{"id":"1","action":"check","args":{"file":"DShader/M_Rock.dss"}}' \
  http://127.0.0.1:$P/dream/shader/call
# {"id":"1","job":"3f9c0a1d2b-1","state":"queued"}
curl -s -H "X-Dream-Token: $T" http://127.0.0.1:$P/dream/shader/jobs/3f9c0a1d2b-1
```

## See also

- [Commandlet](commandlet.md) — the same verbs from a command line, one process per run
- [Bridge](bridge.md) — the editor service the endpoint compiles through, and its own two transports
- [Preview](preview.md) — the renderer behind `preview`
- [Diagnostics](../diagnostics/index.md) — every code a result can carry
- [Project settings](../settings/project.md) — *Enable DreamMCP Endpoint*
