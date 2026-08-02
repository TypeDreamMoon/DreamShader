# Editor bridge

> [DreamShader](../index.md) » [Tools](index.md) » **Editor bridge**

The editor-side service that exposes DreamShader to an external editor through two transports — a
polled request-file directory and a loopback WebSocket server — and a set of generated artifacts
under `Saved/DreamShader/Bridge/`.

| | |
| :-- | :-- |
| Kind | editor service — `FDreamShaderEditorBridge`, owned by the `DreamShaderEditor` module |
| Root | `<Project>/Saved/DreamShader/Bridge/` |
| Transport A | request files — `Bridge/Requests/*.json`, polled every `0.1 s`, deleted on read |
| Transport B | WebSocket — `ws://127.0.0.1:17864`, loopback only |
| Module dependencies | `WebSocketNetworking`, `SQLiteCore` |
| Disabled by | `-NoDreamShaderEditorBridge`; never starts in a commandlet |

## Synopsis

```text
<Project>/Saved/DreamShader/Bridge/
├─ Requests/                   inbound  *.json, consumed and deleted
├─ diagnostics.json            outbound aggregated diagnostics
├─ diagnostics/                outbound sharded diagnostics
│  ├─ index.json
│  └─ <md5-of-source-path>.json
├─ bridge.db                   outbound SQLite (+ bridge.db-wal, bridge.db-shm)
├─ material-expressions.json   outbound reflected UMaterialExpression catalogue
├─ settings.json               outbound enum alias tables
├─ substrate-builtins.json     outbound Substrate builtin catalogue
├─ preview.json                outbound one-shot preview result
└─ Preview/                    outbound rendered PNGs
   └─ <stem>-<crc32>.png
```

## Lifecycle

Startup, in order. `Bridge`, `Bridge/Requests` and `Bridge/Preview` are created first.

| Step | Action |
| :-- | :-- |
| 1 | Create `Bridge/`, `Bridge/Requests/`, `Bridge/Preview/` |
| 2 | Delete `bridge.db`, `bridge.db-wal`, `bridge.db-shm` and the whole `diagnostics/` directory |
| 3 | Export `material-expressions.json`, `settings.json`, `substrate-builtins.json` (and their SQLite tables) |
| 4 | Scan and refresh `VirtualFunction` declarations |
| 5 | Register a post-engine-init handler that generates every source file in memory |
| 6 | Register a project-settings property watcher |
| 7 | Queue a full rescan and write `diagnostics.json` |
| 8 | Start the preview WebSocket server on port `17864` |
| 9 | Register a directory watcher on `<SourceDirectory>` (including directory changes) |
| 10 | Hook `UMaterial::OnMaterialCompilationFinished` |
| 11 | Register the Tools menu, toolbar and context-menu entries |
| 12 | Register ticker A — `0.1 s`: request-file polling and the debounce queue |
| 13 | Register ticker B — every frame: preview streaming |

Ticker B is registered separately, at a zero-second interval, because sharing ticker A would cap
every preview stream at 10 FPS regardless of the client's requested frame rate.

Shutdown removes every handle, shuts the WebSocket server down, clears the pending-file map and the
diagnostics store, deletes the bridge database again, and unregisters the tool menus unless the
engine is already exiting.

## Request files

| | |
| :-- | :-- |
| Directory | `<Project>/Saved/DreamShader/Bridge/Requests/` |
| Discovery | `*.json`, files only, **non-recursive** — subdirectories are never scanned |
| Poll interval | `0.1 s` |
| Consumption | every discovered file is **deleted** at the end of its loop iteration |

The filename is irrelevant; only the JSON contents matter. Use a unique name per request.

> [!WARNING]
> A request file is deleted unconditionally — after a successful dispatch, after a read failure,
> after a JSON parse failure, and after an unrecognized `action`. There is no reply file, no error
> file and no log line for a malformed request: it simply vanishes. Because the poller may open a
> file that is still being written, write the JSON to a temporary name elsewhere and **rename** it
> into `Requests/` so it appears atomically.

### Actions

`action` and `scope` are both matched case-insensitively.

| `action` | Required fields | Effect |
| :-- | :-- | :-- |
| `recompile` | `scope: "all"` | Rebuild the dependency graph and queue every project `.dsm` / `.dsf` for compilation |
| `recompile` | `scope: "file"`, `sourceFile` | Queue one file into the debounce queue |
| `cleanGeneratedShaders` | — | Delete the generated `*.ush` includes, then queue a full rescan |
| `previewMaterial` | `sourceFile` | Render one preview synchronously and write `preview.json` |

> [!NOTE]
> `recompile` with any other `scope`, or with no `scope`, is a silent no-op. `recompile` with
> `scope: "file"` and an empty or missing `sourceFile` is a silent no-op. An unrecognized `action` is
> a silent no-op. In every case the file is still deleted. There are exactly four action names; there
> is no command to save assets, delete assets, or query object state over this transport.

### `recompile`

| Field | Type | Default | Notes |
| :-- | :-- | :-- | :-- |
| `action` | string | — | `"recompile"` |
| `scope` | string | — | `"all"` or `"file"` |
| `sourceFile` | string | — | required and non-empty when `scope` is `"file"`; absolute path recommended |

`scope: "all"` logs `DreamShader queued a full .dsm/.dsf recompile scan.` A queued file is compiled
after the debounce window — *Save Debounce Seconds*, clamped to `[0.05, 10.0]`, default `0.25` — and
only if it still exists on disk. Effective latency is the debounce plus up to `0.1 s` of poll delay.
Compilation through this path is always **transient**: the editor generates in memory.

### `cleanGeneratedShaders`

| Field | Type | Notes |
| :-- | :-- | :-- |
| `action` | string | `"cleanGeneratedShaders"` |

Deletes only `*.ush` files, recursively, one at a time — the directory itself is never removed — then
queues a full rescan. It refuses to run when *Generated Shader Directory* has been pointed outside
the project's `Intermediate` directory:

```text
DreamShader refused to clean generated shaders: '{Directory}' is not inside the project Intermediate
directory. Point DreamShaderSettings.GeneratedShaderDirectory back under Intermediate/ before cleaning.
```

### `previewMaterial`

| Field | Type | Default | Constraint |
| :-- | :-- | :-- | :-- |
| `action` | string | — | `"previewMaterial"` |
| `sourceFile` | string | `""` | must exist and be a `.dsm` |
| `mesh` | string | `""` → `sphere` | see [Meshes](#meshes) |
| `width` | number | `512` | rounded, clamped to `[64, 2048]` |
| `height` | number | `512` | rounded, clamped to `[64, 2048]` |
| `requestId` | string | *(absent)* | echoed into `preview.json` when non-empty |

> [!NOTE]
> The request-file variant does **not** read `orbitYaw`, `orbitPitch`, `stream` or `frameRate`. Those
> fields exist only on the WebSocket transport. A file request always renders one frame at the
> default camera angles.

The result is written to `preview.json` with status `ready` or `error`, and logged as
`DreamShader preview: {Message}` at `Display` on success or `Error` on failure.

## WebSocket server

| | |
| :-- | :-- |
| Endpoint | `ws://127.0.0.1:17864` |
| Bind address | `127.0.0.1` — never `0.0.0.0` |
| Port | `17864`, fixed; there is no project setting for it |
| Connection filter | a connection is accepted only when the client IP is exactly `127.0.0.1` or `localhost`; anything else is refused |
| Tick | every editor frame |
| Purpose | streaming material preview with live orbit control |

Startup and connection logging:

| Message | Severity |
| :-- | :-- |
| `DreamShader preview WebSocket server could not load WebSocketNetworking.` | Warning |
| `DreamShader preview WebSocket server could not be created.` | Warning |
| `DreamShader preview WebSocket server failed to listen on 127.0.0.1:{Port}.` | Warning |
| `DreamShader preview WebSocket server listening on 127.0.0.1:{Port}.` | Display |
| `DreamShader preview WebSocket client connected: {Endpoint}` | Display |
| `DreamShader preview WebSocket: {Message}` | Display on success, Error on failure |

A failure to listen — most often the port already being in use by another editor instance — is a
warning, not an error: the rest of the bridge continues without streaming preview.

### Framing

Every **outbound** message, JSON metadata and raw PNG alike, is sent as a WebSocket *binary* frame
whose payload is:

```text
+--------------------+----------+------------------+
| length (uint32 LE) | tag (u8) | payload bytes …  |
+--------------------+----------+------------------+
```

| Field | Width | Value |
| :-- | :-- | :-- |
| length | 4 bytes, unsigned, little-endian | `1 + payload byte count` — the tag byte is included |
| tag | 1 byte | `1` = UTF-8 JSON, `2` = raw PNG bytes |
| payload | *length − 1* bytes | the JSON text or the image |

The tag exists because the transport always writes binary frames, so the WebSocket opcode cannot
distinguish a JSON message from an image. Images are sent as raw bytes — there is no Base64 anywhere
in this protocol.

**Correlation rule:** a JSON message is always sent first and the matching binary message immediately
after, on the same connection. A client pairs them by arrival order.

**Inbound** messages are plain UTF-8 JSON with no length prefix and no tag byte.

### Message types

| Direction | `type` | Purpose |
| :-- | :-- | :-- |
| client → editor | `previewMaterial` | Start a preview session for a `.dsm` |
| client → editor | `previewControl` | Adjust an existing session |
| editor → client | `previewResult` | Session start result, or a mid-stream error |
| editor → client | `previewFrame` | Metadata for the PNG that follows |

Dispatch reads both `type` and `action`, case-insensitively: a message is treated as
`previewMaterial` when **either** field equals `previewMaterial`; it is treated as `previewControl`
only when `type` equals `previewControl`. Anything else is silently ignored.

Unparsable JSON gets an immediate reply:

```json
{"type":"previewResult","status":"error","message":"Invalid DreamShader preview request JSON.","updatedAtUtc":"<ISO-8601>"}
```

### `previewMaterial` — client → editor

| Field | Type | Default | Constraint / effect |
| :-- | :-- | :-- | :-- |
| `type` or `action` | string | — | must equal `previewMaterial` |
| `sourceFile` | string | `""` | must exist and be a `.dsm` |
| `mesh` | string | `""` → `sphere` | see [Meshes](#meshes) |
| `width` | number | `512` | rounded, clamped to `[64, 2048]` |
| `height` | number | `512` | rounded, clamped to `[64, 2048]` |
| `orbitYaw` | number, degrees | `-157.5` | camera yaw; matches `USceneThumbnailInfo`'s own default |
| `orbitPitch` | number, degrees | `-11.25` | camera pitch; no engine-side clamp |
| `requestId` | string | *(absent)* | echoed on every message of this session |
| `frameRate` | number, FPS | `2.0` | `<= 0` disables streaming; otherwise clamped to `[0.25, 60.0]` and inverted into a frame interval |
| `stream` | bool | `true` | streaming additionally requires a positive frame interval |

On success the editor resolves and compiles the material transiently, saves a PNG under
`Bridge/Preview/`, renders the first frame, writes `preview.json`, sends a `previewResult`, sends the
first frame's PNG bytes, and installs a per-connection session. On failure the connection's session
is removed.

### `previewControl` — client → editor

Ignored entirely when the connection has no session. When both the message's `requestId` and the
session's `requestId` are non-empty they must match, otherwise the message is ignored.

| Field | Type | When absent | Effect |
| :-- | :-- | :-- | :-- |
| `requestId` | string | no guard applied | session guard |
| `stream` | bool | keeps the current value | enable or disable streaming |
| `frameRate` | number | **resets to `2.0`** | new frame interval; `<= 0` stops streaming, otherwise clamped to `[0.25, 60.0]` |
| `orbitYaw` | number | keeps the current angle | drag-to-rotate |
| `orbitPitch` | number | keeps the current angle | drag-to-rotate |
| `ackFrameIndex` | number | no ack | acknowledges a delivered frame and releases the flow-control gate |

> [!WARNING]
> `frameRate` does **not** mean "keep the current rate" when omitted — the reader initializes it to
> `2.0` before looking for the field, so a `previewControl` sent purely to acknowledge a frame or to
> nudge the camera silently resets the session to 2 FPS. Orbit angles behave the opposite way and are
> preserved. Send the current `frameRate` on every `previewControl` message.

### `previewResult` — editor → client

| Field | Type | Notes |
| :-- | :-- | :-- |
| `type` | string | always `previewResult` |
| `requestId` | string | **omitted when empty** |
| `status` | string | `ready` or `error` |
| `sourceFile` | string | normalized absolute source path |
| `assetPath` | string | resolved object path |
| `imagePath` | string | absolute PNG path — **only on the initial result**; the mid-stream error variants omit it |
| `mesh` | string | resolved mesh name |
| `message` | string | human-readable text |
| `updatedAtUtc` | string | ISO-8601 UTC |

A `previewResult` with `status: "error"` sent mid-stream also turns streaming off; the session stops
delivering frames until a new `previewMaterial` or a `previewControl` re-enables it.

### `previewFrame` — editor → client

Always followed immediately by a tag-`2` binary message carrying the PNG.

| Field | Type | Notes |
| :-- | :-- | :-- |
| `type` | string | always `previewFrame` |
| `requestId` | string | omitted when empty |
| `sourceFile` | string | normalized absolute source path |
| `assetPath` | string | resolved object path |
| `mesh` | string | resolved mesh name |
| `frameIndex` | number | monotonically increasing, starting at `0` |
| `updatedAtUtc` | string | ISO-8601 UTC |

### Streaming state machine

Per connection, evaluated once per editor frame.

| State field | Default | Purpose |
| :-- | :-- | :-- |
| `RequestId`, `SourceFilePath`, `AssetPath`, `Mesh` | `""` | session identity |
| `OrbitYaw` / `OrbitPitch` | `-157.5` / `-11.25` | camera angles, degrees |
| `Width` / `Height` | `512` / `512` | render size |
| `FrameIntervalSeconds` | `0.5` (2 FPS) | rate limit |
| `FrameIndex` | `0` | outbound counter |
| `LastAckFrameIndex` | `-1` | last index the client acknowledged |
| `bFrameInFlight` | `false` | flow-control gate |
| `bStreaming` | `false` | master switch |

Each tick:

1. Do nothing when not streaming, when the material is invalid, when there is no render context, or
   when the frame interval is not positive.
2. If a GPU readback is in flight, poll it without blocking. Ready → send `previewFrame` plus the PNG
   and raise the in-flight gate. Error → send an error `previewResult` and stop streaming. Neither →
   return and retry next tick.
3. Otherwise start a new frame only when the previous frame has been acknowledged **and** the frame
   interval has elapsed.
4. A failure to start a frame sends an error `previewResult` and stops streaming.

Effective frame rate is bounded by the 60 FPS clamp, by the client's acknowledgements, and by the
editor's real tick rate.

### Meshes

Compared case-insensitively. Anything unrecognized, including the empty string, falls back to
`sphere`, and the fallback name is what the result messages report.

| Value | Preview primitive |
| :-- | :-- |
| `plane` | plane |
| `cube` | cube |
| `cylinder` | cylinder |
| `shaderball` | shader ball |
| `sphere` | sphere |
| *(anything else)* | sphere |

Mesh and orbit are applied by writing the **material asset's own** thumbnail info, the same fields
the native Material Editor's preview-shape button and drag-to-orbit viewport write. A UI-domain
material is always drawn on a plane regardless of the requested shape. Full renderer behaviour and
its limits are on [Preview](preview.md).

## Artifacts

Everything the bridge writes, and who reads it.

| Path | Direction | Read back by the plugin? |
| :-- | :-- | :-- |
| `Requests/*.json` | inbound | consumed and deleted |
| `diagnostics.json` | outbound | yes — the Material Content Browser's Gen page reads it directly |
| `diagnostics/index.json`, `diagnostics/<md5>.json` | outbound | no |
| `bridge.db` | outbound | **no** |
| `material-expressions.json` | outbound | no |
| `settings.json` | outbound | no |
| `substrate-builtins.json` | outbound | no |
| `preview.json` | outbound | no |
| `Preview/<stem>-<crc32>.png` | outbound | no |
| `<SourceDirectory>/DreamShader.code-workspace` | outbound | no — see [Workspace](workspace.md) |

### `diagnostics.json`

```json
{ "version": 1, "updatedAtUtc": "<ISO-8601>", "files": [ { "path": "…", "diagnostics": [ … ] } ] }
```

Diagnostic object fields. Optional fields are omitted entirely when empty.

| Field | Type | Presence | Value |
| :-- | :-- | :-- | :-- |
| `message` | string | always | the diagnostic text |
| `detail` | string | when non-empty | the raw underlying line |
| `stage` | string | when non-empty | `generate`, `materialCompile` or `virtualFunctionSync` |
| `assetPath` | string | when non-empty | object path of the asset involved |
| `shaderPlatform` | string | when non-empty | material-compile diagnostics only |
| `qualityLevel` | string | when non-empty | material-compile diagnostics only |
| `code` | string | when non-empty | `generate-error`, `material-compile` or `virtual-function-sync` |
| `line` | number | always | 1-based, defaults to `1` |
| `column` | number | always | 1-based, defaults to `1` |
| `severity` | string | always | `error` |
| `source` | string | always | `DreamShader`, `DreamShader Generate`, `DreamShader Material Compile` or `DreamShader VirtualFunction` |

> [!NOTE]
> `severity` is always the literal `error`. The plugin never emits a warning, information or hint
> diagnostic through this file — parse warnings are appended to compile messages instead. A client
> that filters on severity should treat a missing or unknown value as an error.

Stage, code and source always travel together:

| `stage` | `code` | `source` | Produced by |
| :-- | :-- | :-- | :-- |
| `generate` | `generate-error` | `DreamShader Generate` | a failed compile of a source file |
| `materialCompile` | `material-compile` | `DreamShader Material Compile` | shader-compile errors on a generated material |
| `virtualFunctionSync` | `virtual-function-sync` | `DreamShader VirtualFunction` | the startup `VirtualFunction` declaration scan |

Locations are recovered from messages of the form `<path>(<line>,<column>): <message>`. Line and
column must both be numeric and are clamped to `1` or greater; a line with no parseable location is
reported at `1,1` with the leading `"<source path>: "` prefix stripped.

Material-compile diagnostics carry a display message of the form
`[{ShaderPlatform} / {QualityLevel}] {Message}` and are deduplicated on source, platform, quality,
message, line and column.

> [!NOTE]
> Since UE 5.7 the `shaderPlatform` field carries a shader-format name, for example `PCD3D_SM6`,
> because the enumeration walks every shader platform. Below UE 5.7 it carries a feature-level name,
> for example `SM6`.

### `diagnostics/`

The same data sharded one file per source, so a client can re-read only what changed.

`index.json`:

```json
{ "version": 1, "updatedAtUtc": "<ISO-8601>", "files": [ { "path": "…", "file": "<md5>.json", "count": 2 } ] }
```

| Field | Type | Notes |
| :-- | :-- | :-- |
| `path` | string | normalized source path |
| `file` | string | sibling file name — the MD5 of the normalized path plus `.json` |
| `count` | number | number of diagnostics in that shard |

Each `<md5>.json` holds the same `files[]` entry shape as `diagnostics.json`. Stale shards — any
`*.json` other than `index.json` whose name is no longer an active key — are deleted on every write.
All three sinks (`diagnostics.json`, `diagnostics/`, the database) are rewritten together.

### `bridge.db`

SQLite, opened read-write-create with `PRAGMA journal_mode=WAL` and `PRAGMA synchronous=NORMAL`, so
`bridge.db-wal` and `bridge.db-shm` appear alongside it.

```sql
CREATE TABLE IF NOT EXISTS meta(
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS settings_mappings(
    kind TEXT NOT NULL, alias TEXT NOT NULL, normalized_alias TEXT NOT NULL,
    value INTEGER NOT NULL, name TEXT, display_name TEXT, source TEXT NOT NULL,
    PRIMARY KEY(kind, normalized_alias));

CREATE TABLE IF NOT EXISTS material_expressions(
    name TEXT PRIMARY KEY, class_name TEXT NOT NULL, path_name TEXT NOT NULL,
    default_output_type TEXT NOT NULL, json TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS substrate_builtins(
    name TEXT PRIMARY KEY, qualified_name TEXT NOT NULL, class_name TEXT NOT NULL,
    output_type TEXT NOT NULL, is_substrate_output INTEGER NOT NULL,
    detail TEXT, example TEXT, snippet TEXT, json TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS diagnostics(
    path TEXT PRIMARY KEY, json TEXT NOT NULL, updated_at_utc TEXT NOT NULL);

CREATE INDEX IF NOT EXISTS idx_settings_mappings_kind_alias
    ON settings_mappings(kind, alias);
```

| Table | Contents |
| :-- | :-- |
| `meta` | Generation timestamps, one row per producer |
| `settings_mappings` | Every `ShadingModel` / `BlendMode` / `MaterialDomain` alias, with its enum value and whether it came from the project settings or the built-in table |
| `material_expressions` | One row per reflected `UMaterialExpression`, with the full manifest entry in `json` |
| `substrate_builtins` | One row per `Substrate.*` builtin, with the full manifest entry in `json` |
| `diagnostics` | One row per source file, with that file's diagnostics array in `json` |

`meta` keys: `settings.generatedAt`, `materialExpressions.generatedAt`,
`substrateBuiltins.generatedAt`, `diagnostics.updatedAt`.

> [!WARNING]
> The database is **write-only from the plugin's side** and is not durable state. It is deleted on
> bridge startup *and* on bridge shutdown, and every writer replaces its whole table inside a
> transaction (`DELETE FROM` then insert) rather than updating rows. Nothing in the plugin ever reads
> a row back. Do not store client state in it; treat it as a query-friendly mirror of the JSON
> artifacts, valid only while the editor is running.

Failure to open the database is a warning — `Failed to open DreamShader bridge database: {Path}`, or
`Failed to open DreamShader bridge database for diagnostics: {Path}` — and the JSON sinks are still
written. The substitution is the database path, not a reason.

### Manifests

All three are written at bridge startup and again whenever the workspace is opened. Every one carries
a `generatedAt` ISO-8601 UTC timestamp.

| File | `schema` | `version` | Payload |
| :-- | :-- | :-- | :-- |
| `settings.json` | `DreamShader.Settings` | `1` | `mappings: { ShadingModel[], BlendMode[], MaterialDomain[] }` |
| `material-expressions.json` | `DreamShader.MaterialExpressions` | `1` | `expressions[]` |
| `substrate-builtins.json` | `DreamShader.SubstrateBuiltins` | `1` | `builtins[]`, `supported`, and `unsupportedReason` when unsupported |

| Manifest | Entry fields |
| :-- | :-- |
| `settings.json` | `alias`, `value` (number), `name`, `displayName`, `source` (`user` or `builtin`) |
| `material-expressions.json` | `name`, `className`, `pathName`, `defaultOutputType`, `properties[]` (`name`, `type`, `isInput`), `inputs[]`, `outputs[]` (`index`, `name`, `componentCount`, `outputType`) |
| `substrate-builtins.json` | `name`, `qualifiedName`, `className`, `outputType`, `isSubstrateOutput`, `detail`, `example`, `snippet`, `parameters[]` (`qualifier` — always `in` —, `type`, `name`, optional `placeholder`) |

User-defined alias entries are emitted before built-in ones, and a built-in alias whose normalized
form a user alias already claimed is dropped. Aliases are sorted within each kind. Material
expressions are sorted by `name`.

> [!NOTE]
> Below UE 5.4 `substrate-builtins.json` carries an empty `builtins` array, `"supported": false` and
> `"unsupportedReason": "Substrate builtins require Unreal Engine 5.4 or newer."`, and the
> `MSM_Strata` shading model is excluded from `settings.json`.

Logging: `Wrote DreamShader MaterialExpression manifest: {Path}` /
`Failed to write DreamShader MaterialExpression manifest: {Path}`,
`Wrote DreamShader settings manifest: {Path}` /
`Failed to write DreamShader settings manifest: {Path}`, and
`Wrote DreamShader Substrate builtin manifest: {Path}` /
`Failed to write DreamShader Substrate builtin manifest: {Path}`. Unreadable settings produce
`Failed to read DreamShader settings for Bridge manifest.` and no file.

### `preview.json`

Rewritten by every one-shot preview, whichever transport requested it.

| Field | Type | Notes |
| :-- | :-- | :-- |
| `version` | number | always `1` |
| `requestId` | string | present only when non-empty |
| `status` | string | `ready` or `error` |
| `sourceFile` | string | normalized absolute source path |
| `assetPath` | string | resolved object path |
| `imagePath` | string | absolute PNG path |
| `mesh` | string | resolved mesh name |
| `message` | string | success or error text |
| `updatedAtUtc` | string | ISO-8601 UTC |

PNG names are `<sanitized source stem>-<crc32 of the project-relative path, 8 hex digits>.png` under
`Bridge/Preview/`; a source whose stem sanitizes to nothing becomes `DreamShaderPreview`. The same
source therefore always maps to the same file name, and each render overwrites it.

## Kill switch

| | |
| :-- | :-- |
| Switch | `-NoDreamShaderEditorBridge` |
| Form | a bare command-line parameter; no value |

| Effect | Detail |
| :-- | :-- |
| Bridge | not created — no request polling, no WebSocket server, no directory watcher, no diagnostics writer, no manifests, no `bridge.db` |
| Startup in-memory generation | does not run |
| Material Content Browser | tab and menu entries are not registered |
| Tools menu, toolbar, context menus | not registered |
| Cook hook | unaffected — it is installed on a different code path |

The bridge is also never created in a commandlet process, with or without the switch; see
[Commandlet](commandlet.md). Use the switch for automation runs in a normal editor process — for
example `-ExecCmds="Automation RunTests …"` — that must not have a file watcher, a generation pass or
a listening socket.

## Example

Ask a running editor to recompile one file, from PowerShell, writing the request atomically:

```powershell
$req = @{ action = "recompile"; scope = "file"; sourceFile = "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm" }
$dir = "C:\Projects\MyGame\Saved\DreamShader\Bridge\Requests"
$tmp = Join-Path $env:TEMP ("ds-" + [guid]::NewGuid() + ".json")
$req | ConvertTo-Json | Set-Content -Path $tmp -Encoding utf8
Move-Item $tmp (Join-Path $dir ([IO.Path]::GetFileName($tmp)))
```

Request a one-shot 512×512 preview on a cube and read the result:

```json
{
  "action": "previewMaterial",
  "sourceFile": "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm",
  "mesh": "cube",
  "width": 512,
  "height": 512,
  "requestId": "8f3c1b"
}
```

`Saved/DreamShader/Bridge/preview.json` afterwards:

```json
{
  "version": 1,
  "requestId": "8f3c1b",
  "status": "ready",
  "sourceFile": "C:/Projects/MyGame/DShader/Materials/M_Sample.dsm",
  "assetPath": "/Game/Materials/M_Sample.M_Sample",
  "imagePath": "C:/Projects/MyGame/Saved/DreamShader/Bridge/Preview/M_Sample-3fa91c07.png",
  "mesh": "cube",
  "message": "Rendered preview for /Game/Materials/M_Sample.M_Sample.",
  "updatedAtUtc": "2026-08-02T09:14:22Z"
}
```

A streaming session over the WebSocket, client side, in order:

```text
→ {"type":"previewMaterial","sourceFile":"…/M_Sample.dsm","mesh":"shaderball",
   "width":512,"height":512,"requestId":"8f3c1b","stream":true,"frameRate":12}
← [len][1] {"type":"previewResult","requestId":"8f3c1b","status":"ready", … "imagePath":"…"}
← [len][2] <PNG bytes>
← [len][1] {"type":"previewFrame","requestId":"8f3c1b","frameIndex":0, …}
← [len][2] <PNG bytes>
→ {"type":"previewControl","requestId":"8f3c1b","ackFrameIndex":0,"frameRate":12,"orbitYaw":-140.0}
← [len][1] {"type":"previewFrame","requestId":"8f3c1b","frameIndex":1, …}
← [len][2] <PNG bytes>
```

Note the `frameRate` repeated on the control message: omitting it would drop the session to 2 FPS.

## See also

- [Workspace and editor extensions](workspace.md) — the workspace file and the extensions that drive this protocol
- [Commandlet](commandlet.md) — the headless path, where the bridge never starts
- [Preview](preview.md) — the renderer behind `previewMaterial`, and its limits
- [Material Content Browser](material-browser.md) — the Gen page, which reads `diagnostics.json`
- [Editor integration](editor-integration.md) — the menu commands the request actions mirror
- [VirtualFunction tools](virtual-function-tools.md) — the sync pass that produces `virtualFunctionSync` diagnostics
- [Packages](packages.md) — which files a full rescan actually queues
- [Project settings](../settings/project.md) — `SaveDebounceSeconds`, `bAutoCompileOnSave`, `GeneratedShaderDirectory`
- [Material enums](../settings/material-enums.md) — the alias tables exported to `settings.json`
- [Substrate builtins](../builtins/substrate.md) — the catalogue exported to `substrate-builtins.json`
- [UE.Expression](../builtins/ue-expression.md) — the reflection surface `material-expressions.json` describes
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
