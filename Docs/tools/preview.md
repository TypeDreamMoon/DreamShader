# Preview

> [DreamShader](../index.md) » [Tools](index.md) » **Preview**

The editor-side renderer that compiles a material source and renders its material to a PNG, either
once or as a stream of frames.

| | |
| :-- | :-- |
| Implemented in | the `DreamShaderEditor` module |
| Accepts | a source that builds a material or a material instance: `.dss`, `.dsi` or `.dsm` *(`.dss` and `.dsi` since 2.0.0)* |
| Produces | streamed frames over the WebSocket; PNG files under `<Project>/Saved/DreamShader/Bridge/Preview/` plus a `preview.json` manifest on the file/`png` paths |
| Transports | bridge request files (one-shot) · WebSocket on `127.0.0.1:17864` (one-shot or streaming) |
| Streaming since | `1.5.0`; raw RGBA8 frames and Graph breakpoints since `1.7.0` |

Rendering runs on the game thread's ticker. The streaming path exists precisely so that it does not
stall that thread.

> [!NOTE]
> Since `1.7.0` the streaming path defaults to **raw RGBA8 frames** (`encoding: "raw"`): the editor
> reads the render target back and sends the pixels as-is, with no PNG encode, and the client paints
> them straight onto a canvas. This is what makes smooth 30–60 FPS streaming affordable. The old
> JSON-metadata-plus-PNG path is still available as `encoding: "png"` for older clients. A session
> also **dedupes** identical frames and **backs off** to a low idle rate once the picture is static,
> so a settled preview is nearly free. See [Bridge » Framing](bridge.md#framing) for the wire format.

## Breakpoints (probe preview)

*Since `1.7.0`.* A breakpoint set on a line of the material's code — a `Graph` line in a `.dsm`, a
statement of the material's body in a `.dss` *(since 2.0.0)* — (F9 in the DreamShaderLang VS Code
extension) previews the **value bound at that line** on the mesh, instead of the finished material —
the text analogue of the Material Editor's right-click **Start Previewing Node**.

| Aspect | Behaviour |
| :-- | :-- |
| Selection | one probe at a time — the topmost enabled breakpoint in the active source |
| Line snapping | a breakpoint on a blank/comment line snaps forward to the next line that binds a value |
| What is shown | the bound value routed into an unlit Emissive preview (or MaterialAttributes / Substrate when the value is one of those); a texture object is sampled with default coordinates |
| Set before compile | remembered as *pending* and attaches on the next generation |
| After a recompile | re-resolved automatically; the line may move, and the client is told the new line |

The mechanism reuses the engine's own preview material machinery: a transient `UPreviewMaterial`
that shares the generated material's expression collection and has the probed node wired into it,
recompiled through `FMaterialUpdateContext`. `UPreviewMaterial`'s restricted `ShouldCache` keeps that
recompile to a handful of shaders. The protocol side is `setProbe` / `clearProbe` / `probeState` —
see [Bridge » Message types](bridge.md#message-types).

## The two paths

| | One-shot thumbnail | Streaming preview |
| :-- | :-- | :-- |
| Triggered by | a `previewMaterial` request file, or a `previewMaterial` WebSocket message with `stream` false | a `previewMaterial` WebSocket message with `stream` true (the default) |
| Readback | file/PNG: synchronous; WebSocket raw: asynchronous GPU readback | asynchronous — enqueues a GPU readback and polls it on a later tick |
| Waits for shader compilation | yes; WebSocket raw waits across ticks without blocking the editor | no |
| Camera control | request-file path: **no** — orbit fields are not read. WebSocket path: yes | yes, through `previewControl` |
| Result | file/PNG: one PNG plus `preview.json`; WebSocket raw: exactly one RGBA8 frame | one raw RGBA8 frame per tick over the socket (`encoding: "raw"`), or the legacy `preview.json` + tagged PNG pair (`encoding: "png"`) |
| Tick rate | the 0.1 s bridge ticker | every frame |

> [!NOTE]
> The streaming path does not wait for shader compilation. Early frames of a freshly edited material
> can show the previous or default shader until the compile lands; a later frame corrects it. The
> one-shot path waits for the shader to be ready. File/PNG requests can block the editor briefly;
> raw requests deliver their one frame asynchronously, including when `frameRate <= 0`.

Frames retain the camera, probe, encoding and dimensions used when rendering began. Controls changed
during GPU readback apply to the next frame. Starting another preview request discards any pending
frame from the previous request, so it cannot be sent under the new request ID.

The inspector of the [Material Content Browser](material-browser.md#inspector) renders through this
renderer too: a 224×224 preview, asynchronous, re-rendered on every compile. The 1.x *Dream Shader
Gen* page and its static asset thumbnail are gone.

## Request fields

| Field | Type | Default | Notes |
| :-- | :-- | :-- | :-- |
| **`sourceFile`** | string | — | must be an existing `.dss`, `.dsi` or `.dsm` |
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
| Reuse | the render target and thumbnail scene persist across frames; changing size recreates the render target and GPU readback buffer |
| Show flags | game show flags, with **motion blur disabled** and **anti-aliasing disabled** |
| Screen percentage | fixed at 1.0 |
| Separate translucency | follows the thumbnail scene's own rule for the material |
| Alpha | forced to 255 before the PNG is encoded — previews are always opaque |
| UI-domain materials | forced onto a plane regardless of the requested mesh |
| Projection aspect | **always 1:1**, never derived from the requested size |

GPU readiness is polled on the render thread after that frame's copy command, so reusing a readback
buffer cannot mistake the previous frame's completed fence for the current one. The game thread
checks only the asynchronous result. Disconnecting during a capture releases the pending result
safely, and an invalid render-target texture reports an error instead of leaving a capture pending.

> [!WARNING]
> **Request a square `width`/`height`.** The preview renders through `FThumbnailPreviewScene`, whose
> projection matrix is built as `FReversedZPerspectiveMatrix(halfFov, 1, 1, near)` — the aspect ratio
> is hardcoded and is *not* taken from the view rect. A non-square render target therefore does not
> show more of the scene; it stretches the sphere into an ellipse. There is no diagnostic. Ask for a
> square frame and letterbox it on the client side if the surface it fills is not square.

The scene's material interface is deliberately **not** cleared after a frame is submitted; clearing
it would race with render commands still in flight.

The source is compiled as an interactive compile is: a `ThinCustom` product **Ephemeral**, so the
preview never writes it to disk; a `Graph`-backend material and a `.dsi` instance are ordinary assets,
saved as any compile saves them *(since 2.0.0)*. An unchanged source is skipped by the build key
unless the request forces it. The result is typed as a material *interface* on purpose, because the
default `ThinCustom` backend produces a thin material instance rather than a `UMaterial`, and a `.dsi`
a material instance.

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
| Frame format | a 4-byte length, a 1-byte type tag, then the payload. Tag `1` = JSON, tag `2` = PNG, tag `3` = raw RGBA8 frame |

Every message leaves as a raw WebSocket *binary* frame, so the opcode cannot distinguish JSON from
image bytes — hence the explicit type tag. The full framing, the tag-`3` raw-frame header, and the
frame-flag bits are documented in [Bridge » Framing](bridge.md#framing).

| Message | Direction | Purpose |
| :-- | :-- | :-- |
| `previewMaterial` | in | compile and render a source file. Also accepted as `action: "previewMaterial"`, compared case-insensitively. `encoding` selects `raw` or `png`; `force` controls regeneration |
| `previewControl` | in | change streaming state, frame rate, camera angles, viewport size or mesh for the active session |
| `setProbe` / `clearProbe` | in | attach or detach a breakpoint-style probe (see [Breakpoints](#breakpoints-probe-preview)) |
| `previewResult` | out | `status` `ready` or `error`, plus the result fields listed above |
| `previewFrame` | out | *(png encoding only)* frame metadata, immediately followed by the PNG as a tagged binary message |
| `probeState` | out | whether the probe is `attached`, `pending`, or `cleared` |

`previewControl` reads `requestId`, `stream`, `frameRate`, `orbitYaw`, `orbitPitch`, `width`,
`height`, `mesh` and `ackFrameIndex`; every field except `requestId`/`ackFrameIndex` keeps its
current value when omitted. A `requestId` that does not match the active session is ignored.

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
| `DreamShader preview renders a material or a material instance, so it takes a .dss, .dsi or .dsm source: '{File}'.` | the path is a `.dsf`, `.dsh`, `.dsp` or anything else *(since 2.0.0)* |
| `<file>(<line>,<col>): DSHnnnn: <message>` | resolving what the source builds failed; the first error, as a compile reports it |
| `{File}: the source could not be resolved to the assets it builds.` | the same, with no error to show |
| `{File}: this file builds no material or material instance.` | e.g. a `.dss` of exported functions only |
| `The DreamShader compiler module is not available, so '{File}' could not be compiled for preview.` | the `DreamShaderCompiler` module is not loaded |
| *(the compile result message)* | the compile failed — its diagnostics, see [Commandlet » Result messages](commandlet.md#result-messages) |
| `Generated material '{ObjectPath}' could not be loaded.` | the compile succeeded but the object did not load |
| `Compiled preview material for {ObjectPath}.` | success; any compile message is appended |

The source is resolved by the compile's own front half — preprocessor, front end, binder, `import`
headers parsed on their own — so a preview and a compile cannot disagree about which asset a file
builds. The 1.x preview stripped `import` lines and ran the 1.x parser *(before 2.0.0)*.

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

- **Material sources only** — `.dss`, `.dsi`, `.dsm`. A `.dsf`, `.dsh` or `.dsp` cannot be previewed
  at all; there is no function preview.
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
- [Material Content Browser](material-browser.md) — the inspector's live preview, drawn by this renderer
- [Editor integration](editor-integration.md) — `-NoDreamShaderEditorBridge`, which disables the server
- [Ephemeral materials](../generation/ephemeral.md) — why a preview compile does not write a ThinCustom material
- [Backend](../settings/backend.md) — why the previewed object may be an instance rather than a `UMaterial`
- [Source files](../language/source-files.md) — which sources build a material
- [Workspace](workspace.md) — the editor extension that drives the streaming client
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
