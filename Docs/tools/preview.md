# Preview

> [DreamShader](../index.md) » [Tools](index.md) » **Preview**

The editor-side renderer that compiles a `.dsm` in memory and renders its material to a PNG, either
once or as a stream of frames.

| | |
| :-- | :-- |
| Implemented in | the `DreamShaderEditor` module |
| Accepts | `.dsm` only |
| Produces | PNG files under `<Project>/Saved/DreamShader/Bridge/Preview/`, plus a `preview.json` manifest |
| Transports | bridge request files (one-shot) · WebSocket on `127.0.0.1:17864` (one-shot or streaming) |
| Streaming since | `1.5.0` |

Rendering runs on the game thread's ticker. The streaming path exists precisely so that it does not
stall that thread.

## The two paths

| | One-shot thumbnail | Streaming preview |
| :-- | :-- | :-- |
| Triggered by | a `previewMaterial` request file, or a `previewMaterial` WebSocket message with `stream` false | a `previewMaterial` WebSocket message with `stream` true (the default) |
| Readback | synchronous — flushes rendering commands, then blocks on `ReadPixels` | asynchronous — enqueues a GPU readback and polls it on a later tick |
| Waits for shader compilation | yes — finishes asset compilation, then all shader compilation, then flushes rendering commands | no |
| Camera control | request-file path: **no** — orbit fields are not read. WebSocket path: yes | yes, through `previewControl` |
| Result | one PNG plus `preview.json` | `preview.json`, then one tagged binary PNG per frame over the socket |
| Tick rate | the 0.1 s bridge ticker | every frame |

> [!NOTE]
> The streaming path does not wait for shader compilation. Early frames of a freshly edited material
> can show the previous or default shader until the compile lands; a later frame corrects it. The
> one-shot path always waits, which is why it is slower and can block the editor briefly.

The Dream Shader Gen page in the [Material Content Browser](material-browser.md#preview-pane) uses
neither path — its image is a plain 160×160 engine asset thumbnail of the already-generated material.

## Request fields

| Field | Type | Default | Notes |
| :-- | :-- | :-- | :-- |
| **`sourceFile`** | string | — | must be an existing `.dsm` |
| `width` | integer | `512` | clamped to `[64, 2048]` |
| `height` | integer | `512` | clamped to `[64, 2048]` |
| `mesh` | string | `sphere` | see [Meshes](#meshes) |
| `orbitYaw` | number | `-157.5` | degrees. WebSocket only |
| `orbitPitch` | number | `-11.25` | degrees. WebSocket only |
| `requestId` | string | *(absent)* | echoed back on every reply and frame |
| `stream` | boolean | `true` | WebSocket only. `false` renders one frame and stops |
| `frameRate` | number | `2.0` | WebSocket only. `<= 0` disables streaming; otherwise clamped to `[0.25, 60.0]` |

The size clamp is applied independently at three points — when a request file is read, when a
WebSocket message is parsed, and inside the render context — so no route can escape it.

The orbit defaults match Unreal's own scene-thumbnail defaults, so a request that omits both angles
renders from the same viewpoint as the Content Browser tile.

## Meshes

The mesh name is compared case-insensitively. Anything unrecognised, including an empty string,
falls back to the sphere.

| Value | Primitive |
| :-- | :-- |
| `plane` | plane |
| `cube` | cube |
| `cylinder` | cylinder |
| `shaderball` | shader ball |
| *(any other value, or absent)* | sphere |

> [!WARNING]
> An unknown mesh name is **accepted silently** and renders a sphere. There is no diagnostic. Check
> the `mesh` field echoed back in `preview.json` or in the `previewResult` reply to see what was
> actually used.

The shape and the two orbit angles are written onto the **material asset's own** scene thumbnail info
before each render — the very fields the native Material Editor's preview-shape button and
drag-to-orbit viewport write. One is created lazily if the material has none.

> [!NOTE]
> Because the settings live on the asset, a preview render changes the shape and camera the Content
> Browser tile and the Material Editor preview viewport use for that material.

## Camera

| Aspect | Behaviour |
| :-- | :-- |
| Yaw | free; no clamp on either side |
| Pitch | **no engine-side clamp**. The editor extension clamps to roughly ±89° before sending; a client that does not will happily flip the camera over |
| Missing angles in `previewControl` | keep the current angle, so a plain frame acknowledgement cannot snap the camera |

## Render settings

| Aspect | Value |
| :-- | :-- |
| Render target | transient, `PF_B8G8R8A8`, linear gamma not forced, cleared to `(0.025, 0.025, 0.03, 1.0)` |
| Reuse | the render target and the thumbnail scene persist across the frames of one streaming session, and are recreated only when the requested size changes |
| Show flags | game show flags, with **motion blur disabled** and **anti-aliasing disabled** |
| Screen percentage | fixed at 1.0 |
| Separate translucency | follows the thumbnail scene's own rule for the material |
| Alpha | forced to 255 before the PNG is encoded — previews are always opaque |
| UI-domain materials | forced onto a plane regardless of the requested mesh |

The scene's material interface is deliberately **not** cleared after a frame is submitted; clearing
it would race with render commands still in flight.

The material is always compiled **transiently**: an editor preview never writes an asset. The result
is typed as a material *interface* on purpose, because the default `ThinCustom` backend produces a
thin material instance rather than a `UMaterial`.

## Output

| Path | Contents |
| :-- | :-- |
| `<Project>/Saved/DreamShader/Bridge/Preview/` | rendered PNGs |
| `<Project>/Saved/DreamShader/Bridge/preview.json` | the result manifest |

The PNG file name is `<sanitized source stem>-<crc32 of the project-relative path>.png`, with the
CRC printed as eight lowercase hex digits. A source whose stem sanitizes to nothing produces
`DreamShaderPreview-<crc32>.png`.

`preview.json` fields:

| Field | Notes |
| :-- | :-- |
| `version` | `1` |
| `requestId` | present only when the request carried one |
| `status` | `ready` or `error` |
| `sourceFile` | the requested `.dsm` |
| `assetPath` | the generated material's object path |
| `imagePath` | the PNG written for this result |
| `mesh` | the mesh actually used, after the fallback |
| `message` | the success or failure text |
| `updatedAtUtc` | ISO 8601 UTC |

## Streaming preview

| Aspect | Value |
| :-- | :-- |
| Port | `17864` |
| Bind address | `127.0.0.1` only |
| Connection filter | the client IP must be `127.0.0.1` or `localhost`; anything else is refused |
| Frame format | a 4-byte length, a 1-byte type tag, then the payload. Tag `1` = JSON, tag `2` = binary |

Every message leaves as a raw WebSocket *binary* frame, so the opcode cannot distinguish JSON from
image bytes — hence the explicit type tag.

| Message | Direction | Purpose |
| :-- | :-- | :-- |
| `previewMaterial` | in | compile and render a source file. Also accepted as `action: "previewMaterial"`, compared case-insensitively |
| `previewControl` | in | change streaming state, frame rate or camera angles for the active session |
| `previewResult` | out | `status` `ready` or `error`, plus the result fields listed above |
| `previewFrame` | out | frame header: `type`, `requestId`, `sourceFile`, `assetPath`, `mesh`, `frameIndex`, `updatedAtUtc` — immediately followed by the PNG as a tagged binary message |

`previewControl` reads `requestId`, `stream`, `frameRate`, `orbitYaw`, `orbitPitch` and
`ackFrameIndex`. A `requestId` that does not match the active session is ignored.

### Frame pacing

Streaming is acknowledgement-gated as well as rate-limited. A new frame is started only when **both**
conditions hold: the client has acknowledged the previous frame, and the frame interval has elapsed.
The interval is `1 / frameRate` seconds, with `frameRate` clamped to `[0.25, 60.0]`.

Any error during streaming turns streaming off and sends an error `previewResult`.

> [!NOTE]
> The 60 FPS ceiling is only reachable because the preview runs on its own every-frame ticker.
> Sharing the bridge's 0.1 s request ticker would cap every session at 10 FPS no matter what
> `frameRate` asked for.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}`.

### Resolving the material

Checked in this order; the first failure is the reported message.

| Message | Cause |
| :-- | :-- |
| `DreamShader source '{File}' does not exist.` | empty path, or the file is missing |
| `DreamShader preview only supports .dsm material files: '{File}'.` | the path is a `.dsh` or `.dsf` |
| `Failed to read DreamShader source '{File}'.` | the file could not be read |
| `{File}: {ParserError}` | the source did not parse |
| `{File}: This file does not define a top-level Shader block.` | the parse produced no `Shader` |
| *(the compile result message)* | generation failed |
| `Generated material '{ObjectPath}' could not be loaded.` | generation succeeded but the object did not load |
| `Compiled preview material for {ObjectPath}.` | success; any compile message is appended |

`import` lines are stripped before parsing, exactly as on the Gen page.

### Rendering

| Message | Cause |
| :-- | :-- |
| `Failed to create preview render target.` | the render target could not be allocated |
| `Failed to access preview render target resource.` | the target has no RHI resource |
| `Failed to create preview view for '{ObjectPath}'.` | the scene view could not be built |
| `Preview material is not valid.` | the synchronous path was entered with no material |
| `Failed to read preview pixels for '{ObjectPath}'.` | the blocking read-back failed |
| `Failed to encode preview thumbnail for '{ObjectPath}'.` | PNG encoding failed (synchronous path) |
| `A preview readback is already in flight.` | a second asynchronous frame was started before the first completed |
| `Failed to lock preview readback buffer.` | the readback buffer could not be mapped |
| `Preview readback failed.` | the readback promise was not fulfilled |
| `Failed to encode preview thumbnail.` | PNG encoding failed (asynchronous path) |
| `Failed to write preview image '{Path}'.` | the PNG could not be written to disk |
| `Rendered preview for {ObjectPath}.` | success |

### Server

| Message | Severity | Cause |
| :-- | :-- | :-- |
| `DreamShader preview WebSocket server could not load WebSocketNetworking.` | Warning | the module is unavailable |
| `DreamShader preview WebSocket server could not be created.` | Warning | server creation failed |
| `DreamShader preview WebSocket server failed to listen on 127.0.0.1:{Port}.` | Warning | the port is taken or blocked |
| `DreamShader preview WebSocket server listening on 127.0.0.1:{Port}.` | Display | started |
| `DreamShader preview WebSocket client connected: {Client}` | Display | a client attached |
| `DreamShader preview WebSocket: {Message}` | Display / Error | per-request result |
| `DreamShader preview: {Message}` | Display / Error | per-request result, request-file path |

An unparsable inbound message is answered with a `previewResult` whose status is `error` and whose
message is `Invalid DreamShader preview request JSON.`

## Limits

- **`.dsm` only.** A `.dsf` or `.dsh` cannot be previewed at all; there is no function preview.
- Size is clamped to `[64, 2048]` in both dimensions.
- Motion blur, anti-aliasing and dynamic screen percentage are off, so a preview will not match a
  viewport capture pixel for pixel.
- There is no pitch clamp on the plugin side.
- The alpha channel is discarded — a translucent material previews against the fixed clear colour.
- The one-shot path blocks until every shader has compiled; on a cold shader cache that can take a
  long time.
- Only one asynchronous read-back may be outstanding; starting a second returns
  `A preview readback is already in flight.`
- The request-file transport ignores `orbitYaw` and `orbitPitch` entirely.

## Example

A one-shot render driven by a bridge request file. Drop this into
`<Project>/Saved/DreamShader/Bridge/Requests/` as any `.json` name; the poller processes and then
deletes it.

```json
{
  "action": "previewMaterial",
  "sourceFile": "I:/Project/DShader/Materials/M_Emissive.dsm",
  "mesh": "shaderball",
  "width": 512,
  "height": 512,
  "requestId": "preview-1"
}
```

Resulting `<Project>/Saved/DreamShader/Bridge/preview.json`:

```json
{
  "version": 1,
  "requestId": "preview-1",
  "status": "ready",
  "sourceFile": "I:/Project/DShader/Materials/M_Emissive.dsm",
  "assetPath": "/Game/Materials/M_Emissive.M_Emissive",
  "imagePath": "I:/Project/Saved/DreamShader/Bridge/Preview/M_Emissive-1a2b3c4d.png",
  "mesh": "shaderball",
  "message": "Rendered preview for /Game/Materials/M_Emissive.M_Emissive.",
  "updatedAtUtc": "2026-08-02T09:14:07Z"
}
```

## See also

- [Bridge](bridge.md) — the request-file protocol and the full WebSocket message schema
- [Material Content Browser](material-browser.md) — the Gen page's static thumbnail, which this is not
- [Editor integration](editor-integration.md) — `-NoDreamShaderEditorBridge`, which disables the server
- [In-memory materials](../generation/in-memory.md) — why a preview compile never writes an asset
- [Backend](../settings/backend.md) — why the previewed object may be an instance rather than a `UMaterial`
- [Source files](../language/source-files.md) — why only `.dsm` can produce a material
- [Workspace](workspace.md) — the editor extension that drives the streaming client
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
