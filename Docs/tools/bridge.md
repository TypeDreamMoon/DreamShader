# Editor bridge

> [DreamShader](../index.md) » [Tools](index.md) » **Editor bridge**

The editor-side service that exposes DreamShader to an external editor through two transports — a
polled request-file directory and a loopback WebSocket server — and a set of generated artifacts
under `Saved/DreamShader/Bridge/`.

| | |
| :-- | :-- |
| Kind | editor service — `FDreamShaderEditorBridge`, owned by the `DreamShaderEditor` module |
| Root | `<Project>/Saved/DreamShader/Bridge/` |
| Transport A | request files — `Bridge/Requests/*.json`, polled every `0.1 s`, deleted before dispatch, answered in `Bridge/Responses/<requestId>.json` |
| Transport B | WebSocket — `ws://127.0.0.1:17864`, loopback only |
| Liveness | `Bridge/status.json`, rewritten every `2 s` while idle, **deleted on shutdown** |
| Protocol | `1` — sent as `protocol` on a request, echoed on every response and on the status file |
| Module dependencies | `WebSocketNetworking`, `SQLiteCore` |
| Disabled by | `-NoDreamShaderEditorBridge`; never starts in a commandlet |

## Synopsis

```text
<Project>/Saved/DreamShader/Bridge/
├─ Requests/                   inbound  *.json, consumed and deleted
├─ Responses/                  outbound <requestId>.json, one per request that carried an id
├─ status.json                 outbound liveness heartbeat; absent means "not running"
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
| 1 | Create `Bridge/`, `Bridge/Requests/`, `Bridge/Responses/`, `Bridge/Preview/` |
| 1a | Delete every stale `Responses/*.json`, stamp the listening-since time, publish `status.json` |
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

Shutdown **deletes `status.json` first**, then removes every handle, shuts the WebSocket server
down, clears the pending-file map and the diagnostics store, deletes the bridge database again, and
unregisters the tool menus unless the engine is already exiting.

## One editor owns the bridge

The bridge directory is per **project**, not per process: one `Requests` folder, one
`status.json`, one heartbeat. Two editors open on the same project therefore both polled the same
queue — whichever got there first consumed the file, the other read a half-deleted one, and both
overwrote `status.json` with their own pid, so a client could not tell which editor was about to
answer it.

Since `1.8.0` exactly one process serves the bridge, and says so in a lock file:

| | |
| :-- | :-- |
| File | `<Project>/Saved/DreamShader/Bridge/owner.lock` |
| Contents | `pid` and an ISO-8601 `heartbeat`, refreshed on the ordinary heartbeat |
| Believed while | the owning process is still running **and** its heartbeat is under 30s old |
| Released | on editor shutdown, so the next editor takes over on its next heartbeat rather than after the stale window |

Both conditions are needed. The pid test alone would hand the bridge to a second editor every time
the first was mid-compile, since a compile blocks the game thread and stops the heartbeat; the
heartbeat test alone would leave the bridge unowned for a stale window after a hard crash.

A non-owning editor still compiles its own in-memory materials — those are per-process and nobody
else's business — but it does not consume requests, does not write `status.json`, and **does not
write generated assets to disk**. That last one matters because
[storage decides how a rebuild persists](../generation/in-memory.md#when-the-asset-already-exists-on-disk):
without it, two editors would both `SavePackage` the same file, and a save that loses that race is
not a merge — it is a corrupted package or a dead editor. Such a compile reports:

```text
Skipped {AssetPath}; another editor owns this project's DreamShader bridge, and only that one
writes generated assets to disk.
```

The commandlet is unaffected: it has no bridge, no lock file and no second process to negotiate
with, and writing these assets is its entire job.

## Request files

| | |
| :-- | :-- |
| Directory | `<Project>/Saved/DreamShader/Bridge/Requests/` |
| Discovery | `*.json`, files only, **non-recursive** — subdirectories are never scanned |
| Poll interval | `0.1 s` |
| Consumption | every discovered file is **deleted** at the end of its loop iteration |

The filename is irrelevant; only the JSON contents matter. Use a unique name per request.

A request is deleted **before** it is dispatched, not after, so a request that crashes the editor
cannot be replayed on every subsequent start. The one exception is a file that cannot be read at
all: that is almost always one still being written, so it is left alone for the next poll rather
than thrown away.

Because the poller may still open a file mid-write, write the JSON to a temporary name elsewhere
and **rename** it into `Requests/` so it appears atomically.

> [!NOTE]
> A request carrying a non-empty `requestId` is answered in `Responses/<requestId>.json` — including
> the failures: unparseable JSON, a protocol mismatch, an unrecognized `action`, and a `recompile`
> whose source file vanished before the debounce window elapsed. A request **without** an id gets no
> response, which is what every request the shipped VSCode extension sends looks like, and is why
> that extension keeps working unchanged.

> [!WARNING]
> Requests written while no editor was listening are **discarded**, not queued. The cutoff is the
> bridge's own start time, so a request that was waiting on disk when an editor starts is deleted
> with a log line rather than served — by then whoever sent it has long since timed out, and serving
> it would mean a compile pass nobody asked for. Check `status.json` before writing a request.

### Actions

`action` and `scope` are both matched case-insensitively.

| `action` | Required fields | Effect | Answered |
| :-- | :-- | :-- | :-- |
| `ping` | — | Nothing. Confirms the bridge is serving | immediately |
| `recompile` | `scope: "all"` | Rebuild the dependency graph and queue every project `.dsm` / `.dsf` for compilation, **forced** past the source-hash skip | immediately, as **queued** — see below |
| `recompile` | `scope: "file"`, `sourceFile` | Queue one file into the debounce queue, **forced** past the source-hash skip | **when that compile finishes** |
| `cleanGeneratedShaders` | — | Delete the generated `*.ush` includes, then queue a full rescan | immediately |
| `previewMaterial` | `sourceFile` | Render one preview synchronously and write `preview.json` | immediately, with the render result |

> [!IMPORTANT]
> `recompile` with `scope: "file"` is the only action whose response is **deferred**. The file goes
> into the debounce queue and is compiled some ticks later, so answering at dispatch time would
> report success before anything had been attempted. The id is parked and answered from the compile
> itself, carrying that compile's diagnostics. Two clients asking for the same file both get an
> answer.
>
> `scope: "all"` is the opposite and says so: it answers immediately with *queued*, because the batch
> drains across ticks and the outcome is not knowable yet. Do not read that as a result.

> [!NOTE]
> A bad `scope`, a missing `sourceFile`, or an unrecognized `action` is now an **error response**
> rather than a silent no-op — provided the request carried a `requestId`. Without one the behaviour
> is unchanged: the file is deleted and nothing is said.

### Protocol

A request may carry `protocol`. A **missing** field is read as this version, not as a mismatch —
every request the shipped VSCode extension sends omits it, and rejecting those would break it on
every machine that has it installed. A field that is present *and* different is refused:

```text
Protocol 3 is not understood; this editor speaks 1. Update the DreamShaderLang extension or the
plugin so the two match.
```

### `Responses/<requestId>.json`

Written atomically — beside the target, then renamed — so appearing and being complete are the same
event. Stale responses from a previous session are deleted at startup: an answer nobody is waiting
for any more is worse than none, because a client that reconnects would act on it.

| Field | Type | Notes |
| :-- | :-- | :-- |
| `protocol` | number | always `1` |
| `requestId` | string | echoes the request |
| `ok` | bool | |
| `durationMs` | number | |
| `message` | string | invariant, never the localised display text |
| `diagnostics` | array | `file`, `line`, `column`, `severity`, and `code` / `stage` / `assetPath` when non-empty |

Each diagnostic keeps its own file: a `.dsm` that pulls in a broken `.dsh` fails at the `.dsh`'s
position, and attributing it to the `.dsm` would send the author to the wrong file.

### `status.json`

The heartbeat, rewritten every `2 s` while idle and **deleted on shutdown** — so a missing file
means "not running", definitively, and a client can fall back immediately instead of waiting out a
liveness window.

| Field | Type | Notes |
| :-- | :-- | :-- |
| `protocol`, `pid`, `project`, `projectDir`, `engineDir`, `pluginVersion` | | identity |
| `busy` | bool | true while a request is being served |
| `busyAction` | string | present only when busy |
| `lastResult` | string | present once something has completed |
| `heartbeatUtc` | string | ISO-8601 UTC |

> [!WARNING]
> Read `busy` **before** judging the heartbeat's age. A compile blocks the game thread, so the
> heartbeat stops while one runs — a client that only looks at the timestamp reads every real
> compile as a crash. The remaining ambiguity is a hard crash, which leaves the file behind; the
> recorded `pid` is what closes it.

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
| tag | 1 byte | `1` = UTF-8 JSON, `2` = raw PNG bytes, `3` = raw frame (header + RGBA8) |
| payload | *length − 1* bytes | the JSON text, the image, or the raw-frame header+pixels |

The tag exists because the transport always writes binary frames, so the WebSocket opcode cannot
distinguish a JSON message from an image. Images are sent as raw bytes — there is no Base64 anywhere
in this protocol.

A session chooses its frame encoding with the `encoding` field of `previewMaterial`:

| `encoding` | Frame delivery | Since |
| :-- | :-- | :-- |
| `"raw"` | one self-describing tag-`3` message per frame (header + RGBA8), no JSON metadata | `1.7.0` |
| `"png"` *(default when absent)* | a tag-`1` `previewFrame` JSON message immediately followed by a tag-`2` PNG | `1.5.0` |

`raw` is the efficient path a browser/webview client wants — the pixels go straight onto a canvas
with `putImageData`, with no PNG encode on the editor side and no decode on the client side. `png`
is kept for clients that predate `raw`.

**Correlation rule (png only):** the JSON `previewFrame` is always sent first and the matching PNG
binary message immediately after, on the same connection. A client pairs them by arrival order. A
`raw` frame is a single message and needs no pairing.

#### Raw frame header (tag `3`)

The tag-`3` payload is a 24-byte little-endian header followed by `width × height × 4` bytes of
RGBA8 pixels (alpha is always `255`).

| Offset | Field | Type | Notes |
| :-- | :-- | :-- | :-- |
| 0 | `version` | u8 | `1` |
| 1 | `format` | u8 | `1` = RGBA8 |
| 2 | `width` | u16 | pixels |
| 4 | `height` | u16 | pixels |
| 6 | `flags` | u16 | frame-flag bits, see below |
| 8 | `frameIndex` | u32 | monotonic, starts at `0`; echo it back in `ackFrameIndex` |
| 12 | `orbitYaw` | f32 | the yaw the frame was rendered at |
| 16 | `orbitPitch` | f32 | the pitch the frame was rendered at |
| 20 | `probeLine` | u32 | the source line the active probe resolved to, or `0` |

Frame-flag bits:

| Bit | Name | Meaning |
| :-- | :-- | :-- |
| `1 << 0` | Compiling | the shader map is still building; the pixels are the fallback material |
| `1 << 1` | ProbeActive | a breakpoint is attached and the frame shows that value, not the material |
| `1 << 2` | ProbePending | a breakpoint was requested but could not attach yet |
| `1 << 3` | Keyframe | the frame was sent because something changed, not because the clock ticked |

**Inbound** messages are plain UTF-8 JSON with no length prefix and no tag byte.

### Message types

| Direction | `type` | Purpose |
| :-- | :-- | :-- |
| client → editor | `previewMaterial` | Start a preview session for a `.dsm` |
| client → editor | `previewControl` | Adjust an existing session |
| client → editor | `setProbe` | Attach a breakpoint-style probe to a Graph line |
| client → editor | `clearProbe` | Detach the active probe |
| editor → client | `previewResult` | Session start result, or a mid-stream error |
| editor → client | `previewFrame` | Metadata for the PNG that follows *(png encoding only)* |
| editor → client | `probeState` | Whether the probe is attached, pending, or cleared |

Dispatch reads both `type` and `action`, case-insensitively: a message is treated as
`previewMaterial` when **either** field equals `previewMaterial`. `previewControl`, `setProbe` and
`clearProbe` are matched on `type` only. Anything else is silently ignored.

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
| `encoding` | string | `"png"` | `"raw"` for tag-`3` RGBA8 frames; `"png"` for the legacy JSON+PNG pair |
| `force` | bool | `false` | `true` regenerates the material even when the source hash is unchanged (the explicit **Refresh** action); `false` reuses the last generation, so a re-request for a camera/mesh change or a save the bridge already picked up costs no regeneration |

On success the editor resolves and compiles the material transiently, sends a `previewResult`, sends
a `probeState`, and installs a per-connection session that streams frames from the next tick. The
`png` encoding additionally saves a first-frame PNG under `Bridge/Preview/`, writes `preview.json`,
and sends that PNG immediately; the `raw` encoding streams its first frame from the tick loop like
every other one. On failure the connection's session is removed.

### `previewControl` — client → editor

Ignored entirely when the connection has no session. When both the message's `requestId` and the
session's `requestId` are non-empty they must match, otherwise the message is ignored.

| Field | Type | When absent | Effect |
| :-- | :-- | :-- | :-- |
| `requestId` | string | no guard applied | session guard |
| `stream` | bool | keeps the current value | enable or disable streaming |
| `frameRate` | number | keeps the current rate | new frame interval; `<= 0` stops streaming, otherwise clamped to `[0.25, 60.0]` |
| `orbitYaw` | number | keeps the current angle | drag-to-rotate |
| `orbitPitch` | number | keeps the current angle | drag-to-rotate |
| `width` / `height` | number | keeps the current size | resize the render target; clamped to `[64, 2048]` |
| `mesh` | string | keeps the current mesh | change the preview primitive |
| `ackFrameIndex` | number | no ack | acknowledges a delivered frame and releases the flow-control gate |

> [!NOTE]
> Every field except `requestId`/`ackFrameIndex` keeps its current value when omitted *(since
> `1.7.0`)*, so a bare frame acknowledgement or camera nudge no longer resets the rate. A missing
> `ackFrameIndex` on a streaming session is still treated as "the client is alive", releasing the
> flow-control gate.

### `setProbe` — client → editor

Attaches a probe (the text-source analogue of the Material Editor's **Start Previewing Node**): the
value bound at a Graph line replaces the material on the preview mesh. Ignored when the connection
has no session.

| Field | Type | Default | Effect |
| :-- | :-- | :-- | :-- |
| `type` | string | — | must equal `setProbe` |
| `line` | number | `0` | 1-based line in the session's source file; the nearest binding at or after it is chosen |
| `name` | string | `""` | disambiguates when several bindings share the line |

The reply is a `probeState`. A probe set before the source has ever generated is remembered and
attaches on the next generation rather than failing.

### `clearProbe` — client → editor

Detaches the active probe; the mesh returns to the whole material. Replies with a `probeState` whose
`status` is `cleared`.

### `probeState` — editor → client

Sent after any `setProbe`/`clearProbe`, after a `previewMaterial` re-resolves the probe, and whenever
a regeneration moves or drops it.

| Field | Type | Notes |
| :-- | :-- | :-- |
| `type` | string | always `probeState` |
| `requestId` | string | omitted when empty |
| `sourceFile` | string | normalized absolute source path |
| `status` | string | `attached`, `pending`, or `cleared` |
| `line` / `column` | number | present when `attached`; the line the probe resolved to |
| `name` | string | present when `attached`; the bound variable |
| `componentCount` | number | present when `attached`; the value's channel count |
| `message` | string | present when `pending`; why it has not attached yet |

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

Only sent for the `png` encoding, always followed immediately by a tag-`2` binary message carrying
the PNG. A `raw`-encoding session sends the tag-`3` frame instead and never sends `previewFrame`.

| Field | Type | Notes |
| :-- | :-- | :-- |
| `type` | string | always `previewFrame` |
| `requestId` | string | omitted when empty |
| `sourceFile` | string | normalized absolute source path |
| `assetPath` | string | resolved object path |
| `mesh` | string | resolved mesh name |
| `frameIndex` | number | monotonically increasing, starting at `0` |
| `flags` | number | frame-flag bits (same set as the raw-frame header) |
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
2. If a GPU readback is in flight, poll it without blocking. Ready → deliver the frame (a tag-`3`
   raw frame, or a `previewFrame`+PNG pair) and raise the in-flight gate. Error → send an error
   `previewResult` and stop streaming. Neither → return and retry next tick.
3. Otherwise start a new frame only when the previous frame has been acknowledged **and** the frame
   interval has elapsed.
4. A failure to start a frame sends an error `previewResult` and stops streaming.

Two efficiency behaviours sit on top of the loop *(since `1.7.0`)*:

- **Dedupe.** A frame whose pixels are identical to the last one sent is dropped, so a static
  material costs no bandwidth once it has settled.
- **Idle back-off.** After a few identical frames the render clock relaxes to ~2 FPS until something
  changes (an edit's recompile, a camera/mesh change, a probe change). A frame rendered while the
  shader map is still compiling never backs off — the compile landing is the change being waited
  for. Any change re-arms the full rate and forces the next frame through the dedupe as a keyframe.

A frame whose acknowledgement never arrives (a hidden tab, a lost message) is not waited on forever;
after a two-second timeout the next frame is started regardless. Effective frame rate is bounded by
the 60 FPS clamp, by the client's acknowledgements, by the idle back-off, and by the editor's real
tick rate.

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
| `Responses/<requestId>.json` | outbound | no — the client deletes its own |
| `status.json` | outbound | no |
| `owner.lock` | internal | yes — every editor reads it to find out whether it is the one serving this project |
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
