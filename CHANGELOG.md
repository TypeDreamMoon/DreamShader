# DreamShader ChangeLog

## 1.8.0 - 2026-08-21

### Fixed

- **A `Domain="Volume"` material carries `bUsedWithVolumetricCloud`, and decompiling one no longer
  turns the flag back on.** Assigning a generated volume material to a Volumetric Cloud component
  warned that the material was not flagged for it, and the only cure was ticking the box by hand in
  the material editor, which the next generation overwrote. `ApplySettings` now derives the flag
  from the domain, so a Volume-domain material gets it without the source having to say so, and an
  explicit `bUsedWithVolumetricCloud = "false";` still wins — a Volume material that only feeds
  volumetric fog can decline the cloud shader permutations. Two things had to follow from that.
  First, on UE 5.8 the flag is written through
  `UMaterial::SetUsageByFlag(MATUSAGE_VolumetricCloud, ...)` rather than by assigning the member:
  5.8 deprecates every `bUsedWith*` field in favour of the accessors, so the direct write compiled
  with a C4996 that would become a hard error the release Epic removes them in. Which spelling is
  the portable one flips at exactly that version, in both directions at once — before 5.8
  `SetUsageByFlag` is *private* on `UMaterial` and the fields are plain public `UPROPERTY`s — so the
  write is gated on `DREAMSHADER_UE_VERSION_AT_LEAST(5, 8)` and each branch is the only one that
  compiles warning-free on its own engines. Second, the assignment moved out of the `Domain`
  branch and reads the domain back off the material, because `ResetMaterialToDefaults` does not
  clear usage flags: a material regenerated from `Volume` to some other domain used to keep a stale
  flag, and a source that omits `Domain` keeps the domain it already has. Reported in
  [#27](https://github.com/TypeDreamMoon/DreamShader/pull/27).

- **Decompile no longer drops `bUsedWithVolumetricCloud = "false"` from a Volume-domain material.**
  `AppendBoolMaterialSettingIfDifferent` decides whether a setting is worth writing by comparing it
  against the `UMaterial` class default, and for this one flag that baseline is now wrong: the
  generator derives it from the domain, so on a Volume material the value that needs no line is
  `true`, not `false`. A fog-only Volume material therefore decompiled to a source with no mention
  of the flag, and regenerating that source switched it on — `UMaterial` → `.dsm` → `UMaterial` was
  not an identity. Both directions now go through `GetDefaultUsedWithVolumetricCloud`, the single
  place that says what the domain implies, and the helper takes an explicit baseline for settings
  whose default is derived rather than constant.

- **`DreamShaderGraphDecompilerHelpers.h` compiles on its own.** It declares
  `GetMaterialDomainText(EMaterialDomain)` and `GetBlendModeText(EBlendMode)` without including
  either enum's header; a unity build only ever had them through a neighbouring translation unit, so
  the gap stayed invisible until a non-unity compile of that one file (`-SingleFile`) failed with
  four syntax errors per declaration. It now includes `MaterialDomain.h` and `Engine/EngineTypes.h`,
  as `DreamShaderMaterialGeneratorPrivate.h` already did for the same reason.

- **A swizzle survives the material editor.** A component selection like `CustomStencil.r` was written
  as an inline mask on the connection (`OutputIndex=0` plus `Mask/MaskR`), which the HLSL translator
  honours but the material graph editor cannot draw: pins correspond to an expression's `Outputs`
  entries, and no pin means "output 0, red channel only". `UMaterialGraph::GetValidOutputIndex`
  distrusts `OutputIndex 0` whenever a mask is present — that combination used to mean a pre-`OutputIndex`
  legacy connection — looks for a pin whose mask matches, finds none, and falls back to the node's
  **last** output. The wire is then drawn from that pin, and the first Apply/Save writes it back through
  `UMaterialExpression::ConnectExpression`, which also replaces the mask with the pin's own. On a
  `SceneTexture` node (`Color / Size / InvSize`) that turned `CustomStencil.r` into `InvSize`, silently,
  with no error anywhere: the stencil comparison then failed for every pixel and the effect the function
  gated simply stopped appearing. Any generated asset touched through the material editor was exposed,
  and duplicating one first did not help — the copy is corrupted the same way on its first save.

  Component selections are now emitted in a form the graph can round-trip. An inline mask is kept only
  where the editor resolves it back to the pin it already names; where the expression publishes the same
  value through several masked outputs (`TextureSample`'s `RGB/R/G/B/A/RGBA`, `VertexColor`, …) the
  matching output is named directly; otherwise a real `ComponentMask` node is emitted. Mixed output sets
  like `SceneTexture`'s are never retargeted, because there the outputs are different values rather than
  views of one. `ConnectCodeValueToInput` now `ensure`s the invariant, so a future masking shortcut says
  so instead of shipping a graph that decays on contact. Covered by
  `DreamShader.Compiler.Generate.GraphStableComponentMasks`. The build key tag moves to `DSK2`, so every
  generated asset is rebuilt once into the new form.

- **The regeneration skip key covers everything that decides what a source compiles into.** It hashed
  the prepared source text and nothing else, so anything *else* that changes the output left every
  already generated asset looking current: switching the **Default Compiler Backend** — which decides
  whether a `Shader` block becomes a `UMaterial` or a thin instance — retargeting a shading-model,
  blend-mode or domain mapping, or upgrading the plugin or the engine. The backend case was papered
  over with a forced full sweep bolted onto that one setting; the others were not covered at all.

  `DreamShader.SourceHash` is now a build key: prepared source text, default backend, the three
  mapping tables, the plugin version plus a hand-bumped format tag, and the engine version. The forced
  sweep is gone — each affected asset fails the skip check on its own, and one the setting does not
  affect is still skipped instead of being needlessly rebuilt. Imports need nothing extra, since the
  hashed text already has them inlined. Changing the key's composition invalidates every existing
  stamp: one rebuild per asset, once, which is the intended effect. Covered by
  `DreamShader.Compiler.BuildKey.CoversCompileContext`.

- **A batch of source files compiles in dependency order.** A `.dsm` that calls a `ShaderFunction`
  binds its call node against the live `UMaterialFunction` asset — `SetMaterialFunction` reads the pins
  off the object, not off the source — so compiling the caller before the callee bound it against the
  *previous* version of that function's interface. Renaming a function input and saving both files was
  enough to hit it, and which one won depended on the iteration order of a `TMap`. Both drain points
  (the watcher's pending-file batch and the whole-project sweep) now topologically sort by the import
  graph; only edges inside the batch are honoured, and a cycle is left for the import loader to reject
  with its own diagnostic. Covered by `DreamShader.Compiler.Order.*`.

- **One editor owns the bridge for a project.** The bridge directory is per-project — one `Requests`
  folder, one `status.json`, one heartbeat — so two editors open on the same project both polled the
  same queue (whichever got there first consumed the file, the other read a half-deleted one) and both
  overwrote `status.json` with their own pid, leaving a client unable to tell which editor was about
  to answer it. Ownership is now a lock file, `Bridge/owner.lock`, carrying the owning pid and a
  heartbeat: an owner is believed while its process is alive **and** its heartbeat is under 30s old
  (the pid test alone hands the bridge over whenever the owner is mid-compile, since a compile blocks
  the game thread; the heartbeat test alone leaves it unowned after a hard crash). It is released on
  shutdown so the next editor takes over on its next heartbeat.

  A non-owning editor still compiles its own in-memory materials, but does not consume requests, write
  `status.json`, or **write generated assets to disk**. That last part is a direct consequence of
  storage deciding how a rebuild persists: without it, two editors would both `SavePackage` the same
  file, and a save that loses that race is not a merge — it is a corrupted package or a dead editor.
  The commandlet is unaffected; it has no bridge and writing these assets is its whole job. The
  deferral is covered by `DreamShader.Compiler.Persistence.NonWriteOwnerLeavesDiskAssetAlone`; the
  lock itself needs two processes and is not automated.

- **A rebuild is refused while the asset is open in an asset editor.** An asset editor does not edit
  the asset: `FMaterialEditor` duplicates it into a transient `UPreviewMaterial` and copies that
  duplicate back over the original on Apply or Save, and the material instance editor writes back
  through a `UMaterialEditorInstanceConstant` wrapper. An editor left open across a rebuild was
  therefore holding a *pre-rebuild* copy, and the next Apply silently reverted everything the rebuild
  had done — surfacing later as a divergence report on the compile after that, a long way from the
  cause. Compiles now stop with `Asset '{ObjectPath}' is open in an asset editor, so it was NOT
  rebuilt. …`, and say to close it.

  Refusing is the only safe answer available: whether the editor's copy has unapplied edits in it is
  `FMaterialEditor::bMaterialDirty`, which is private to the MaterialEditor module, so "close it if
  it is clean" is not a question the plugin can ask — and closing it blindly would pop the engine's
  save prompt in the middle of a compile-on-save, or hang a headless build.

  **Revert to Source** and **Adopt Into Source** are the exception: they close the editor themselves,
  act, and reopen it, because both are offered on that editor's own toolbar and a permanently dead
  menu item there would be a bug rather than a guard. For Adopt the close must come first and the
  engine's save prompt is load-bearing rather than noise — unapplied editor changes are not part of
  "this asset's current contents" until it is answered. Cancelling it cancels the action. Covered by
  `DreamShader.Compiler.OpenEditor.*`.

- **An asset that exists on disk is rebuilt on disk, not in memory over the top of its own file.**
  The editor asks for a memory-only compile every time, and when that landed on a package which
  already existed, generation loaded the saved asset, rebuilt it in place, and then cleared the dirty
  flag. The result was an object that matched neither the file on disk nor anything that would ever
  be written, and that reported itself clean: a Save All could persist a state nobody chose, and the
  version visible in the editor disappeared on restart. The only signal was one log warning.

  Storage now decides how a rebuild persists, not the compile that asked for it
  (`IsGeneratedAssetPersisted`): an asset with a file behind it takes the persisted path — stamped,
  saved, dirty flag never faked — and one without stays memory-only. There is exactly one answer to
  "what is this asset" again. Covered by `DreamShader.Compiler.Persistence.*`.

  The startup sweep stopped forcing as part of this, and had to. Forcing was free while every
  in-memory asset regenerated regardless of its source hash; it stopped being free the moment a
  disk-backed asset started being *saved*, because every editor launch would then rewrite every
  persisted generated asset — disk churn, source-control noise and a slow startup, for rebuilds the
  source hash had already ruled out. Changing the **Default Compiler Backend** still forces, because
  the hash covers the source text and cannot see that setting.

- **A failed rebuild no longer empties the asset.** Regeneration cleared the graph and then built
  into the emptied asset, so any failure after the teardown left it empty — and the failures that can
  still happen at that point are ordinary ones, because a `Graph` block is compiled one statement at
  a time by the graph builder, long after the clear. An emptied material was bad; an emptied material
  *function* was much worse, since its call sites read their pins from the live asset, so one bad
  `.dsf` took every material that called it down with it. There was no undo either: generated assets
  are deliberately not `RF_Transactional`, because undo/redo desynchronizes the shader map.

  Teardown now *detaches* the old graph instead of destroying it (`FDreamShaderGraphRollback`), and
  destroys it only once the rebuild has fully succeeded. A failure restores the asset from a
  serialized snapshot taken before the teardown — render state, material-function usage, node graph,
  connections and `FunctionInput`/`FunctionOutput` pin GUIDs all come back, so existing call sites
  stay wired. Two details make it work: the snapshot is taken with delta serialization **off** (a
  property equal to the class default is otherwise skipped, and the restore would leave whatever the
  failed build had set it to), and it covers the asset's **editor-only data object** as well as the
  asset — since UE 5.1 the expression collection and the material property inputs live on a separate
  `UMaterialEditorOnlyData` / `UMaterialFunctionEditorOnlyData` UObject, so snapshotting the material
  alone captures nothing but a pointer to it. Covered by `DreamShader.Compiler.Atomic.*`.

  Two things fall out of it. The teardown's two deletion strategies collapsed into one: the
  node-by-node path existed to break inbound links, which is `O(n^2)` across a graph and pointless
  when the graph leaves as a unit, so the former 1200-expression threshold is gone and every rebuild
  takes the fast path. And `DependentFunctionExpressionCandidates` — the second, serialized node list
  whose stale entries used to arm a null-deref crash for the rest of the session after a failed
  function compile — is now reset at commit rather than at teardown, because a failed compile no
  longer empties the function at all.

- **The ThinCustom instance path runs the ownership guard.** It checked only the class of whatever
  sat at the target path, not its provenance, so generating onto a hand-authored
  `UDreamShaderMaterialInstance` adopted it and then cleared its parameter overrides. Since
  ThinCustom is the default backend this was the widest of the three creation paths and the only one
  without the check; it now refuses with the same message the material and function paths use. The
  gap was documented as a known warning in `Docs/generation/regeneration.md`, which is updated.

- **Cook-generated assets are registered with the AssetRegistry, so they reach the package.** The
  cook hook wrote every source file out as a persistent `.uasset` and then relied on the engine
  finding it, but a cook request is resolved through `IAssetRegistry::DoesPackageExistOnDisk`, which
  consults only the registry's in-memory state and has no filesystem fallback. Generation runs on
  post-engine-init, i.e. after the registry enumerated the content directories, so a freshly written
  package was invisible to that lookup: `DirectoriesToAlwaysCook` scanned the file off disk and then
  the request was dropped again, with no error anywhere — the material simply was not in the pak, and
  `LoadObject` failed at runtime in a Shipping build. `GenerateAllAssetsForCook` now records what its
  saves actually wrote (`UPackage::PackageSavedWithContextEvent`) and hands the filenames to
  `IAssetRegistry::ScanModifiedAssetFiles` before the commandlet's `Main` collects the initial
  requests, logging `DreamShader cook: registered {Count} generated package(s) with the
  AssetRegistry.` Reported in [#26](https://github.com/TypeDreamMoon/DreamShader/issues/26) against a
  `Domain="UI"` material; nothing about the fix is domain-specific.

- **`UE.CollectionParam(...) Name;` in `Properties` is as wide as the collection parameter.** The
  parser records the declaration form as a scalar (it cannot open the collection), and the generator
  used that width as-is, so a vector MPC parameter passed to a `float4` input was widened by rule 9 —
  three `AppendVector` splats — and the material failed to compile with `Can't append float4 to
  float4`. The generator now reads the width from the loaded collection when it creates the node
  (vector → 4, scalar → 1); an explicit `OutputType=` on the declaration still wins. Found while
  feeding DreamWind's five MPC vectors into a `Function`.

- **A render target is accepted where a texture of its dimension is expected.** `const VolumeTexture
  V = Path(Plugin.DreamWind, "RT_DreamWindVolume");` failed with `Const texture property 'V' expects
  VolumeTexture but '...' is a 'TextureRenderTargetVolume'`, because the dimension check compared
  asset classes (`UVolumeTexture`, `UTextureCube`, `UTexture2DArray`) instead of dimensions. It now
  reads `UTexture::GetMaterialType()` — the same answer the material compiler gives on a
  texture-object pin — so `UTextureRenderTargetVolume`, `UTextureRenderTarget2D`,
  `UTextureRenderTargetCube` and `UTextureRenderTarget2DArray` pass for their dimension, and anything
  else a texture-object pin would take (e.g. `UTexture2DDynamic`) does too. Compact texture tokens
  and `TextureObjectParameter`, `const` or not.

### Added

- **A Shader's Graph block can drive material properties directly, and the Outputs block became
  optional.** Writing `Base.BaseColor = ...` inside `Graph` does what the same line did inside
  `Outputs`, so a material that only routes the standard attributes no longer restates every one of
  them twice — once as a typed declaration, once as a binding of a variable to the attribute of the
  same name. `Outputs` is unchanged and still required for what only it can express: renaming a
  value on its way out, and `Expression(Class="...").Pin[N]` custom-output targets, which have no
  declared type to infer and must keep their declaration. Four things are refused rather than
  resolved: one property driven from both places (the block is order-free, the statement is not, so
  neither is the obvious winner), a Graph variable named `Base` (one identifier would mean a local
  on the right of `=` and an output on the left), reading `Base.X` back, and a material that ends up
  driving nothing at all. Repeating a write is allowed and the last one wins, like any other Graph
  assignment.

  `Base` is now reserved inside a Shader's Graph, which is a source-breaking change for a material
  that used it as a variable name; a material function is unaffected and keeps the name available.

- **A value that does not fit the material property it drives is refused where it is written.** The
  existing check compared the *declared* type of an output against its property, so it only ran for
  a binding whose source was a declared variable — an expression-valued binding, and now a Graph
  write, had nothing to check. The component count of the value that was actually built is now
  compared against the property instead, which covers every route to a material input and is a
  stronger question than what someone declared. Widening still happens (a scalar assigned to
  `BaseColor` splats); only a value too wide for its property is an error, and only when its
  component count is known rather than inferred.

- **A hand-edited generated asset is no longer rebuilt over.** Regeneration used to tear the graph
  down unconditionally, so editing a generated material and then touching its `.dsm` destroyed the
  edit with no diagnostic and no undo — regeneration is deliberately not transactional. Every
  successful generation now stamps `DreamShader.OutputDigest`, a fingerprint of what the asset
  actually holds, and the next rebuild compares it against the asset *before* anything is cleared. A
  mismatch fails the compile with a message naming three actions, all on the asset's right-click
  menu under **DreamShader**: **Revert to Source** (discard the edits and rebuild), **Adopt Into
  Source** (rewrite the `.dsm`/`.dsf` from the asset, backing the old one up to `<source>.bak`), and
  **Detach From DreamShader** (keep the asset, stop managing it). The digest covers exactly what a
  rebuild would destroy — nodes, connections, node properties, the reset-property set, a function's
  asset-level fields, and a ThinCustom instance's parameter overrides — and deliberately excludes
  what one would not: node positions, user comment boxes, pin GUIDs, and properties outside the reset
  list. See `Docs/generation/divergence.md`. Seven tests under
  `DreamShader.Compiler.Divergence.*`.

  Two details worth knowing. `-Force` does **not** override a divergence: `bForce` answers "is the
  source hash stale", and the editor asserts it for every file in its own startup sweep, so honouring
  it would have left the gate dead in the mode the editor spends all its time in — only the Revert
  action overrides one. And the source *path* (not the hash) is now stamped on memory-only assets
  too, because without it an in-memory asset classifies as foreign and the gate never fires; stamping
  the path alone cannot switch the source-hash skip on, which needs a matching hash as well.


- **`#include` at the top of a `Function` body is hoisted to file scope.** A `Function` block's
  HLSL used to be emitted verbatim between the braces of the generated `DreamShaderFn_*` definition,
  which made any `#include` land *inside* a function — fine for macro-only headers, a compile error
  for anything that defines a function. Leading `#include "…"` / `#include <…>` directives (only
  whitespace and comments before them) are now stripped from the body and emitted once, in first-seen
  order, right after the guard of the generated `.ush`; a `SelfContained` embed or a `GraphFunction`
  body puts them on the Custom node's `IncludeFilePaths` instead, ahead of the generated include.
  A directive after the first statement keeps the old behaviour. This is what lets a header shared
  between C++ and HLSL — DreamWind's `/Plugin/DreamWind/Shared/DreamWindShared.h` — be consumed from
  a `.dsf` without copying its functions into DreamShaderLang.
  `FTextShaderFunctionDefinition::IncludePaths` carries the paths; `PrepareCustomNodeCode` gained an
  `OutEmbeddedIncludePaths` parameter. Covered by `DreamShader.Compiler.Parser.FunctionIncludeHoist`
  and `DreamShader.Compiler.Generate.FunctionIncludeHoist`.

## 1.7.1 - 2026-08-16

### Fixed

- **Closing the editor crashed after using a Graph breakpoint.** An access violation reading
  `0x3b0` in `UMaterial::GetExpressionInputDescription`, from the probe preview's own destructor,
  reached through `FDreamShaderPreviewWebSocketServer::Shutdown` while modules were unloading.

  Modules unload from inside the exit path: `EngineExit()` raises the exit request, `FEngineLoop::Exit()`
  runs the purge, and only then calls `UnloadModulesAtShutdown()`. So a module tearing down its own
  state at that point is doing it *after* the objects it points at are gone. `TStrongObjectPtr` keeps
  the preview material out of the garbage collector's reachable-set sweep, but it does not exempt it
  from the exit purge — the pointer stayed non-null while the object behind it did not, which is why
  the existing null checks passed and the dereference still faulted.

  What it faulted on is worth naming, because nothing about the call site suggests it:
  `UMaterial::GetExpressionInputDescription` and `GetExpressionCollection` both dereference
  `GetEditorOnlyData()` without checking it, so reaching for a material's inputs or expressions after
  the purge is an unconditional null-plus-offset read rather than a recoverable failure.

  The teardown is now skipped once the engine is exiting. Skipping is correct and not merely safe:
  releasing the shared expression collection exists so the preview material cannot touch the graph
  material's nodes *later*, and at exit there is no later — both are being destroyed anyway. The
  ordinary path, where a client disconnects while the editor keeps running, is unchanged and still
  releases everything.

## 1.7.0 - 2026-08-16

### Added

- **Graph breakpoints in the live preview — "Start Previewing Node" for a text source.** Set a
  breakpoint on a `Graph` line and the preview mesh shows the value bound at that line instead of the
  finished material. This is the same idea as right-clicking a node in the Material Editor and
  choosing *Start Previewing Node*, except the "node" is a line of DreamShaderLang. It reuses the
  engine's own machinery to get there: the generator now publishes a per-source **debug table**
  mapping every `(line, name)` binding to the exact `UMaterialExpression` / output / channel-mask it
  produced, and a transient `UPreviewMaterial` — sharing the generated material's expression
  collection, with the probed node wired into its emissive (or `MaterialAttributes` / `FrontMaterial`
  for those value kinds, and a default-coordinate sample for a texture object) — is recompiled through
  `FMaterialUpdateContext`. `UPreviewMaterial::ShouldCache` keeps that recompile to a handful of
  shaders, exactly as the node-thumbnail path does.

  A breakpoint on a blank or comment line snaps forward to the next line that binds a value; a
  breakpoint set before the source has ever generated is remembered and attaches on the next compile;
  after a recompile the probe re-resolves automatically and the client is told the line it landed on.
  Wire protocol: `setProbe` / `clearProbe` / `probeState` — see [Docs/tools/preview.md](Docs/tools/preview.md).

### Changed

- **The streaming preview now sends raw RGBA8 frames instead of a PNG per frame.** A new
  `encoding: "raw"` session reads the render target back and streams the pixels as one self-describing
  binary frame (a 24-byte header — size, flags, camera, resolved probe line — then the pixels), with
  no PNG encode on the editor side. A browser/webview client paints them straight onto a canvas. This
  is most of what a smooth 30–60 FPS stream costs; PNG-encoding a 512² frame per tick was tens of
  milliseconds of game-thread time. The legacy `encoding: "png"` path (JSON `previewFrame` + tagged
  PNG) is unchanged for older clients.

- **Streaming got cheaper when nothing is moving.** Identical frames are dropped, and after a few in a
  row the render clock backs off to a low idle rate until an edit, a camera/mesh change, or a probe
  change re-arms it (a frame rendered while shaders are still compiling never backs off). A
  `previewControl` that omits a field — including `frameRate` — now keeps the current value rather than
  resetting to 2 FPS, and it can now carry `width` / `height` / `mesh` so the client can drive the
  render size and shape without a full re-request. A `force` flag on `previewMaterial` distinguishes an
  explicit refresh (regenerate) from a camera/mesh re-request (reuse the last generation).

## 1.6.0 - 2026-08-15

### Added

- **Ten more math builtins in `Graph`: `step` `smoothstep` `length` `cross` `asin` `acos` `atan`
  `atan2` `reflect` `refract`.** Every one of them had a node — or, for `reflect` and `refract`, a
  four-line definition — and none of them had a spelling, so the only way to write
  `step(0.5, x)` was `UE.Expression(Class="Step", OutputType="float1", Y=0.5, X=x)`, and the failure
  when you wrote it the HLSL way was `Unknown Graph function 'step'` wrapped inside whichever call
  the argument belonged to. The builtin surface goes from 19 spellings to 29.

  Three of the new names do not wire to a pin called `Input`, which is the whole reason the mapping
  is worth stating: `Step` and `Arctangent2` name their pins `Y` and `X`, and `SmoothStep` names them
  `Min`/`Max`/`Value`. Argument order stays HLSL's in every case, so `step(edge, x)` wires argument 1
  to `Y` and argument 2 to `X` and still means `x >= edge`. `length` and `cross` join `dot` as the
  builtins with a fixed return width — 1 and 3 components, authoritative — matching what those same
  classes already reported when reached through `UE.Expression`.

  `reflect` and `refract` are the first builtins that are not one node. Unreal has no expression for
  either, so they are lowered to the arithmetic HLSL defines them as: four nodes for
  `i - 2 * dot(i, n) * n`, and fourteen for refraction including the `If` that returns zero under
  total internal reflection. Both are exact and both are expensive; where the surrounding code is
  already HLSL, a `Function` body still does it in one node.

  **This widens a reserved namespace.** These ten names now shadow user code silently, the same way
  the original nineteen do — a `Function`, property or `ShaderFunction` named `length`, `step` or
  `cross` becomes unreachable from a `Graph` block with no diagnostic. Existing sources that declare
  one need renaming.

  Not added, and not addable: matrices. The Unreal material graph has no matrix value type at all,
  so `mul(M, v)` has no `Graph` spelling regardless of what the DSL does —
  `UE.Expression(Class="Transform"/"TransformPosition", …)` covers space conversions and a `Function`
  HLSL body covers the rest. Still absent but reachable through `UE.Expression`, each having a node:
  exponential, logarithmic, `tan`, `sign`, `round`, `trunc` and `distance`.

  The decompiler is unchanged, so the new nodes export as generic `UE.Expression(Class="…", …)` calls
  rather than round-tripping to the builtin spelling — the same asymmetry `Fmod` already has.
  `reflect` and `refract` cannot round-trip at all, since they leave behind ordinary arithmetic nodes
  with nothing marking their origin.

- **The editor bridge answers now, and says whether it is alive.** It had an inbound half and
  nothing else: a request produced no reply, no error file and no log line — a malformed one simply
  vanished — and a client's only way to guess whether an editor was running at all was to look for
  `bridge.db` and hope. The bridge now publishes `Bridge/status.json` (protocol, pid, project,
  plugin version, `busy` / `busyAction`, `lastResult`, heartbeat), rewritten every 2 s and **deleted
  on shutdown** so a missing file means "not running" definitively rather than "timed out", and
  answers any request carrying a `requestId` in `Bridge/Responses/<requestId>.json` with `ok`,
  `durationMs`, `message` and this compile's diagnostics. `recompile` with `scope: "file"` is
  answered **when the compile finishes**, not at dispatch, since the file goes through the debounce
  queue — including the case where the source vanished before the window elapsed. `scope: "all"`
  answers immediately as *queued*, because the batch drains across ticks and the outcome is not
  knowable yet. A `ping` action was added. Bad scopes, missing `sourceFile` and unknown actions are
  now error responses instead of silent no-ops.

  Three deliberate compatibility choices, all of which keep the shipped VSCode extension working
  unchanged: a **missing** `protocol` field is read as the current version rather than a mismatch
  (every request the extension sends omits it); a request with **no** `requestId` gets no response,
  as before; and an unreadable request file is now **left for the next poll** instead of deleted,
  because the extension writes requests in place rather than renaming them in, so a half-written
  file is a real state and deleting it threw the request away.

  Two behaviours borrowed from DreamFX's bridge, which had them right: requests are deleted
  **before** dispatch so one that crashes the editor cannot be replayed on every start, and requests
  written while nobody was listening are discarded rather than served late.

- **Parse failures now carry a `DSHnnnn` code, so the message text stops being the contract.**
  Until now the only thing identifying a DreamShader failure was its English wording — which is why
  the generator's message sites carry `I18N-EXEMPT` markers: translate the text and you break the
  tests, the corpus expectations and the diagnose skill that match on it. All 120 parser raise sites
  now name a code instead (`DSH1xxx` path resolution, `DSH2xxx` lexer and syntax, `DSH3xxx` sections
  and declarations, `DSH7xxx` properties, parameters and settings), carried on
  `FDreamShaderTextError` alongside the `FText`. `FTextShaderParser::Parse` gains a coded overload
  and keeps the `FText`/`FString` ones, so callers migrate when they have a use for the code.
  `Docs/diagnostics/` gains a page per range, generated from the raise sites by
  `.skill/gen-diagnostics.ps1` with the Cause/Fix prose written by hand and preserved across
  regenerations; `-Check` gates it. Nothing on the wire changed —
  `FDreamShaderDiagnosticRecord::Code` and the `"code"` field in `diagnostics.json` already existed.
- **The editor UI and the diagnostics pipeline speak your language; the wire format does not.**
  Editor-facing text — the *DreamShader Gen* page, the settings section, slow-task progress, parser
  and decompiler diagnostics — moved from `FString` literals to `LOCTEXT`/`NSLOCTEXT`, and the
  diagnostic records that carry it (`FDreamShaderCompileResult::Message`,
  `FDreamShaderDiagnosticRecord::Message`/`Detail`, `FTextShaderParser::Parse`) are `FText`.
  Simplified Chinese ships with it: `Content/Localization/DreamShader/zh-Hans/DreamShader.locres`,
  loaded through the `LocalizationTargets` entry in `DreamShader.uplugin` with an `Editor` loading
  policy. `FString` overloads are kept alongside every converted signature, so nothing outside the
  plugin has to move at once. Thanks to [@youli42](https://github.com/youli42) — PR
  [#24](https://github.com/TypeDreamMoon/DreamShader/pull/24).
- **`diagnostics.json`, `diagnostics/*.json`, `bridge.db` and the preview WebSocket stay English in
  every editor culture.** The VSCode and Rider extensions parse those, so a translated editor must
  not change a byte of them. `FTextInspector::GetSourceString` is only half an answer: it hands back
  the source string for a plain `LOCTEXT`, but for an `FText::Format` result it returns the
  *localized* substituted display, so one message site adopting `FText::Format` would have leaked
  translated text onto the wire. `ToInvariantWireString` (`DreamShaderTextWireUtils.h`) instead
  replays an `FText`'s historic format data — the source pattern, every argument rendered in the
  invariant culture, nested formats recursively, number grouping off so `12345` never becomes
  `12,345` — and every JSON/SQLite/WebSocket write goes through it.
  `DreamShader.Lang.Diagnostics.TextWireUtils` asserts the output is identical under `en-US` and
  `zh-Hans`.
- **`Tools/Localization/localization_lint.ps1`** — checks `LOCTEXT_NAMESPACE` define/undef pairing,
  rejects it in headers, and flags literals that gather cannot see (`FText::FromString(TEXT(...))`,
  `FString::Printf(TEXT(...))`) with an `I18N-EXEMPT` escape for text that is deliberately not
  display text. `-EmitBaseline` writes `Tools/Localization/BASELINE.md`, whose gather count (342)
  is the regression check.

- **Graph layout runs on in-memory materials.** Interactive compiles are memory-only, and the
  placement pass used to be skipped for them outright — so the graph you saw after a save was not a
  badly laid out graph, it was an *unlaid out* one: the tall single column of construction
  coordinates, wires strung across it. Layout now runs there too, so a save shows what the
  generated asset will look like. *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Compiler ▸ **Lay
  Out In-Memory Graphs*** (`bLayoutInMemoryGraphs`, default on) turns it back off. The in-memory
  path runs the pass quiet — one slow-task frame per block rather than a formatted progress string
  per node, which at these sizes costs more than the placement.

- **Plugin source roots.** Every enabled plugin that ships a `DShader` folder now contributes its
  own source root, so a plugin can carry the `.dsm`/`.dsf`/`.dsh` files that build its materials
  instead of parking them in the project's tree. `Root="Plugin.<Name>"` has been able to *write*
  assets into a plugin since `1.2.0`; the source half was missing. Discovery, the dependency graph
  and generate-all pick plugin roots up with no further configuration.
- *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Paths ▸ **Scan Plugin Source Directories***
  (`bScanPluginSourceDirectories`, default on) turns the plugin scan off.
- **A plugin-root file defaults to its own plugin's mount point.** A `.dsm` under
  `Plugins/MoonToon/DShader` with no `Root=` attribute now generates into `/MoonToon`, not `/Game` —
  source and asset stay in the plugin that ships them. Only an absent or whitespace-only `Root` is
  defaulted, so `Root="/"` opts back into `/Game`, and the default is applied per block. Skipped
  with a `Warning` when the plugin cannot host content, in which case the block falls back to
  `/Game` as before. Applied at generation, at the *DreamShader Gen* page's target column and at the
  preview renderer alike, so all three name the same asset.
- The source-directory watcher registers one watch per root, so *Auto Compile On Save* fires for a
  plugin's sources the same way it does for the project's.
- `DreamShader.code-workspace` lists one `folders` entry per root — the project as `"."`, then
  `Plugin: <Name>` for each plugin root, relative to the workspace file (absolute when the root is
  on another drive). The file stays at `<SourceDirectory>/DreamShader.code-workspace`, and the
  project folder keeps its name and `"."` path, so VSCode's per-folder settings survive.
- The *DreamShader Gen* page labels a plugin-root file with its root name in the row subtitle, and
  the search box matches root names — typing a plugin's name filters to everything it ships.

- **`.skill/build-plugin.ps1`** — `RunUAT BuildPlugin` across a list of engine roots, one `PASS` /
  `FAIL` line each, exit code = the number that failed. Every engine builds into its own `-Package`
  directory, so the runs share no state and the plugin's `Binaries/` and `Intermediate/` are left
  alone. This is what caught the five ungated newer-engine APIs below; an editor build against one
  engine cannot, and neither can any compile-time check, because one of the five is a link error.

### Changed

- **The *DreamShader Gen* page reads diagnostics from the bridge, not from `diagnostics.json`.** The
  page used to re-parse the file the bridge had just written in order to colour its rows; it now
  asks the bridge for the records it already holds. One consequence worth knowing: those records
  live for the editor session, so the page no longer shows errors left over from a *previous*
  session before you compile again. Everything produced in the current session — including the
  startup and auto-compile passes — shows exactly as before.

- **Automatic layout places nodes at their real size.** Every node used to be assumed `320 × 150`
  and spaced on a fixed `420 × 220` grid. Nothing that draws taller than 220 fitted — a
  `TextureSample`'s preview thumbnail alone is 106, a `Custom` node grows a row per pin — so those
  nodes overlapped their neighbours and pushed out through the comment box that was meant to
  contain them. Sizes are now estimated from what the node widget actually assembles (title bar,
  one row per pin, the expression preview when `ShouldShowPreview()`), and columns, rows, comment
  boxes and the output-usage column are all measured against that. Nodes are right-aligned within
  their column so a column's outputs line up.
- **Long edges get lanes, and chains come out straight.** An edge spanning several ranks now
  reserves a dummy slot in each column it crosses. Crossing reduction only ever compares neighbours
  one rank apart, so without lanes it could not see those edges at all — which is what let a graph
  read as a ball of wire even though each column, taken on its own, was tidy. Placement then runs
  four straightening passes, each pulling a node toward its neighbours' average centre and
  repairing the column with an isotonic regression: the closest set of centres that still keeps the
  column's order and its gaps. A chain lands on one horizontal line instead of a staircase.
- **Blocks pack into columns instead of one stack.** A material with several connected outputs used
  to become a ribbon tens of thousands of units tall, legible only fully zoomed out. Blocks are now
  laid out around their own origins and packed into columns against a height budget of
  `max(2400, sqrt(totalArea / 1.6))`, which keeps the finished graph roughly landscape whatever the
  block count. Too few blocks to fill a column reproduces the old top-to-bottom stack. Comment
  boxes are created after packing, so a box can no longer be left behind by the nodes it wraps.
- `DreamShader.Gen.Layout.NoOverlap` covers the three above: it generates a four-output material and
  asserts no two placed nodes overlap and no node hangs out of its comment box, measuring each node
  with the same estimate the placement used.
- **Imports never cross roots.** A file resolves its imports against its own root and that root's
  `Packages` folder only. Two plugins shipping the same relative path can no longer shadow one
  another, and disabling a plugin cannot silently change what another root's import means. Files
  under the project root — every file that existed before this release — resolve exactly as they
  did. A file outside every root (a test fixture, a commandlet `-Source` pointing elsewhere) still
  falls back to the project root.
- VirtualFunction sync skips files under a plugin root. Only the project root is writable: a plugin
  ships its definitions as authored, and the editor no longer rewrites them.
- A plugin `DShader` folder that overlaps an existing root — which happens when *Source Directory*
  is pointed at a plugin folder or at something containing one — is ignored with a warning, rather
  than handing the same file to two owners.

### Fixed

- **The plugin builds again on UE 5.5 and 5.6.** Five UE 5.7 APIs had been used without a gate, so
  the plugin compiled only on the engine it was written against. All five now route through
  `DreamShaderVersionCompat.h` like everything else version-dependent:
  - `Materials/MaterialParameters.h` is 5.7 and later; `MaterialTypes.h` declares
    `FMaterialParameterInfo` before it, and is a deprecation stub after.
  - `UMaterialExpression::ShouldShowPreview()` is 5.7; the layout pass falls back to
    `!bHidePreviewWindow && !bCollapsed`, which is what 5.7 composed it from — same answer, no
    behaviour change.
  - `UMaterialExpressionScalarParameter::ControlType` / `Enumeration` / `EnumerationIndex` are 5.7.
    The decompiler exports them only there; below 5.7 there is no such property to read or write.
  - `UMaterialExpressionCustomOutput::GetInputValueType` is 5.6, and the decompiler was calling it
    directly instead of through the shim the generator already had for exactly this.
  - Six engine expression classes — `SceneDepth`, `SceneColor`, `ObjectRadius`, `ObjectBounds`,
    `PerInstanceRandom`, `PerInstanceFadeAmount` — are `UCLASS()` with no export macro, and before
    5.6 their `StaticClass()` does not resolve from a plugin at all. This one is a `LNK2019`, not a
    compile error: it passes every compile-time check and only a full `RunUAT BuildPlugin` sees it.
    Below 5.6 the class is looked up by script path instead, so the `UE.*` builtins behave the same;
    they are dropped from the builtin table only if that lookup fails.

- `DreamShaderMaterialGenerator.cpp` did not include `UObject/Package.h` despite calling
  `GetPackage()->SetDirtyFlag()`, `HasAnyPackageFlags()` and deducing `FindObject`/`NewObject`
  against a `GetTransientPackage()` outer. Same shape as the `Engine/EngineTypes.h` omission below:
  unity builds pulled `UPackage` in through a neighbouring translation unit, so it only failed —
  with `error C2027` — under the adaptive non-unity compile UBT gives whatever file you are editing.
- `DreamShaderMaterialGeneratorPrivate.h` did not include `Engine/EngineTypes.h` despite declaring
  functions that take `EBlendMode` and `EMaterialShadingModel`. Unity builds pulled the enums in
  through a neighbouring translation unit; any non-unity compile of a file including it — which is
  what UBT's adaptive non-unity does to whatever you are currently editing — failed with
  `error C2061`.

- **Root-qualified imports** — `import "Plugin.MoonToon:Shared/Toon.dsh";` — the one way to cross a
  root deliberately. The qualifier is `Project`, `Plugin.<Name>` or `Plugins.<Name>` (`/` spells the
  same as `.`), matched case-insensitively, using the vocabulary `Root=` already has. A qualified
  specifier tries only the target root's source and packages directories, both containment-checked;
  the importing file's own root is not consulted.

  The `:` is load-bearing: `Plugin.MoonToon/Shared/Common.dsh` could not be told apart from a
  relative path through a folder of that name, which would put the resolver back to guessing by scan
  order. Text before a `:` that does not match a qualifier shape is not treated as one, so
  `import "C:/Shared/Common.dsh"` keeps failing exactly as it did.

  New diagnostic for a qualifier that parses but names no live root:
  `DreamShader import '{Specifier}' referenced from '{Path}' names source root '{Qualifier}', which
  is not a DreamShader source root.`

### Known gaps

Follow-up work, not regressions:

- The root list is cached and only rebuilt when the *Source Directory* setting or the scan toggle
  changes, or when *DreamShader Gen ▸ Refresh* is pressed — which is what picks up a `DShader`
  folder you just created, or a plugin mounted mid-session. The **watcher** is still registered once
  at startup, so *Auto Compile On Save* for a root that appeared mid-session starts working on the
  next editor start.

- **The `zh-Hans` locres is a hand-gathered snapshot with no gather config behind it.** There is no
  `Config/Localization/DreamShader.ini`, so the Localization Dashboard cannot re-gather or recompile
  the target — a new `LOCTEXT` will simply fall back to English until someone regenerates the
  `.locres` by hand. `Tools/Localization/BASELINE.md` is what catches the drift in the meantime:
  its count moving means the translation is behind. Wiring up a gather config, and a CI step that
  runs it, is the fix.
- **Only the parser raises `DSHnnnn` codes so far; the generator does not.** `Docs/diagnostics/`
  covers 112 codes, but `Docs/diagnostics/index.md` remains the authoritative catalogue of all 659
  messages until the generator's ~561 sites are tagged too — they still report through
  `FString& OutError` with no code, and reach the store as the generic `generate-error`. The recipe
  is settled and mechanical (tag by message key, `FailWith` as a statement, `WrapError` to keep the
  inner code), but it is roughly four times the volume of the parser and touches 229 signatures.
  `README.md` marks the untagged ranges rather than linking to pages that do not exist yet.
- **The preview renderer's error channel is not localized.** `FDreamShaderPreviewRenderContext`
  reports failures through `FString& OutError`, and those strings reach `preview.json` and the
  preview WebSocket, which must stay English for the VSCode and Rider extensions. Collapsing an
  `FText` into that `FString` would localize the wire, so every one of those sites is deliberately
  an `I18N-EXEMPT` literal. Localizing them means converting the whole error path to `FText` first.

## 1.5.1 - 2026-08-02

Documentation and tooling only. No plugin code changed, so a project on `1.5.0` needs no
migration.

### Added

- `.skill/` — an agent skill set for DreamShaderLang, in the Claude Code `SKILL.md` format:
  `dream-shader-create` (a description becomes a `.dsm` that provably compiles),
  `dream-shader-optimize` (decompiler output becomes a source a human would write),
  `dream-shader-decompile`, `dream-shader-verify` and `dream-shader-diagnose`.
- `.skill/dsc.ps1` — a headless driver around `-run=DreamShader`. It resolves the engine from the
  `.uproject`'s `EngineAssociation`, finds the project by walking up, de-duplicates the doubled
  `LogInit` echo of every `LogDreamShader` line, and classifies each asset the run wrote against
  git. `-CleanNew` then deletes exactly the untracked ones and prunes the emptied folders, so a
  verification run no longer leaves `.uasset` files behind that shadow in-memory generation.
- `.skill/sync-skills.ps1` — publishes the tree into `.claude/skills`, rewriting the relative
  `Docs/` links and the driver path against the destination. `-Check` exits `1` on drift.
- `.skill/reference/dreamshaderlang.md` — the grammar subset an author needs, including the traps
  that only surface at compile time: the 19 reserved math builtins that shadow user code silently,
  the whole-identifier GLSL rewrite inside `Function` bodies, and the absent matrix types.

### Changed

- Both READMEs restructured around the reference manual. The sections that had become abridged
  copies of `Docs/` pages — Properties, Graph, MaterialAttributes, Substrate, Material Layers,
  VirtualFunction, Configuration, Release — now link to the page that owns them, which is also the
  page that gets maintained. The minimal material leads instead of sitting 130 lines down, and
  in-memory generation is stated up front. 459 → 279 lines, and the two languages are kept
  structurally identical.
- Version references across `Docs/` updated to `1.5.1`. `(since 1.5.0)` feature markers are
  unchanged — they record when a feature landed, not what the current version is.

### Fixed

- The release archive now ships `Shaders/`, `README.zh-CN.md` and `.skill/`. Up to `1.5.0` the
  packaging step copied a seven-item allowlist and skipped anything missing **silently**, so an
  archive install had no `Shaders/DreamShaderBuiltins.ush` for the `/Plugin/DreamShader` virtual
  shader directory to resolve against, and `README.md`'s link to the Chinese readme dangled. A
  missing item now also emits a `::warning::` on the run summary instead of vanishing.

## 1.5.0 - 2026-08-02

### Language (DreamShaderLang 1.5)

- Section `=` is now optional: write `Properties { ... }`, `Settings { ... }`, and `Graph { ... }` without the assignment.
- `Properties Group("Name") { ... }` scopes a group onto every parameter it contains; groups nest and compose.
- `Slider(min, max)` shorthand sets a scalar parameter's UI range; asset paths can follow `=` directly and bare quoted paths are accepted.
- Single-output functions can be used as return values (`x = Fn(...)`), and `Graph` builtins now match the `Function` path (`fract`, `mod` / `fmod`).
- Live preview streaming keeps the editor and language-server previews in sync while you type.
- `true` / `false` are graph literals and materialize as `StaticBool` nodes, so an
  `opt StaticBool X = false` input default generates the Preview-pin node Unreal requires (it
  ignores `PreviewValue` for static-bool inputs).
- `StaticBool` resolves as a one-component type at call sites.
- Texture parameter types whose token carries no dimension (`TextureObjectParameter`) take their
  dimension from the assigned default asset, so a `Texture2DArray` / `TextureCube` / `VolumeTexture`
  default is accepted. Explicit tokens (`Texture2D`, `Texture2DArray`, …) still validate strictly.

### Backend — one unified compilation path

- The Graph and (experimental) Instance backends are collapsed into a single **ThinCustom** path: DreamShaderLang compiles to a real node graph on a hidden base `UMaterial`, wrapped by a thin `UDreamShaderMaterialInstance`. The engine compiles and enumerates the material natively, so Substrate, static switches, virtual textures, MaterialAttributes, and cook correctness all come from the real graph.
- Bit-identical SM6 render parity with the previous Graph backend, verified across Unlit, textured, and DefaultLit MaterialAttributes cases.
- The hidden base is a subobject of the instance — one asset, one package, invisible in the Content Browser, with no separate `MB_DreamThinBase_*` sibling and no cross-package parent import to lose at cook.
- `Backend="Instance"` and `DefaultBackend=Instance` are retained as aliases for ThinCustom; a single **Default Compiler Backend** setting replaces the old In-Memory toggle.

### Editor — Material Content Browser

- New **DreamShader Material Content Browser** tab (`Tools > DreamShader`) with two pages: **Project** (browse, filter, and inspect every material / material instance under `/Game`, with the full inheritance chain) and **Dream Shader Gen** (source list, live preview, search, filters, compile-all, and load-time error surfacing).
- Create material instances from any material through a folder picker, and materialize in-memory (preview-only) materials to disk on demand.
- Content Browser context-menu actions and a toggle to show or hide DreamShader's memory-only materials.

### Decompiler

- Faithful round-trip for Substrate materials and renamed graph channels: the exporter now derives channel swizzles from the write mask rather than the channel name, so recompiled materials match the source bit-for-bit.

Decompiling a hand-authored material and generating it back could fail on graph shapes that Unreal
accepts (found on LGUI's `LexUI_ImageAndFont` / `LexUI_RectBlock` / `MF_LexUI_SDF_Font`):

- Switch-style nodes (`StaticSwitch`, `FeatureLevelSwitch`, `QualitySwitch`, `ShadingPathSwitch`,
  `VertexInterpolator`, …) report no output value type, so the "assume float4" fallback oversized
  them and everything downstream. An `AppendVector` fed by a float3 material-function output was
  emitted as `float5(...)`; appends are now clamped to a float4 with a warning if a count still
  disagrees.
- `VertexColor` is emitted as float4 so the alpha pin's swizzle is valid — it used to be typed from
  the RGB pin and produce `.a` on a three-component value.
- An input's own channel mask now replaces the connected pin's mask instead of stacking on it
  (`.rgb.a` no longer appears when a graph wires the RGB pin but masks alpha).
- A `StaticBool` function input keeps the `StaticBool` type token; `bool` declares a scalar pin, so
  the input used to come back as a float and reject every static-bool value passed to it.
- Comment / `#Region` / description text carrying newlines or tabs is escaped, so a multi-line
  comment no longer splits the directive across lines.
- A Custom node's additional outputs are declared on every emission of that node, and reading one no
  longer rewrites the node's own return type. Previously the emission that did not select the extra
  output produced a node without it, and the code body assigning to it failed at shader-compile time
  with "use of undeclared identifier" — long after generation reported success.

### Fixed

- Cook: assets are materialized on the cook director only, and a generation error now fails the cook instead of shipping a stale asset.
- Generation refuses to overwrite assets DreamShader did not generate, and pre-validates graph syntax before clearing the target material.
- Generated-include identity hashes the project-relative source path; stored source paths are project-relative and no longer carry a generated-at timestamp.
- Runtime builds: guarded the editor-only `UEnum::HasMetaData` call so non-editor / Shipping (store) builds compile (#12).
- Bridge: adopted `FCoreDelegates::GetOnPostEngineInit` for UE 5.8, and constrained "Clean Generated Shaders" to `Intermediate` with per-file deletes.

### Compatibility

- Added Unreal Engine `5.8` support. The supported range is now `5.3` through `5.8` (Win64).

## 1.4.1 - 2026-07-01

### Added

- Parameter input pins can be wired from the `Graph` with a call form, e.g. `Mask(Input = ...)` and `Tex(Coordinates = ...)`, for channel/component-mask and texture-sample parameters.
- `Docs/ParameterReference.md`: per-type declaration + metadata reference covering asset slots (`[Prop = Path(...)]`) and input pins for every parameter type.

### Fixed

- Decompiled multi-output Custom nodes emitted both `Output=` and `OutputIndex=` and failed to regenerate; the decompiler now emits a single output selector.
- `DynamicParameter`, `CurveAtlasRowParameter` inline defaults, and the texture-sample parameter family failed to generate or compile; they now produce valid nodes (texture samples seed a default texture).
- Import directives no longer shift diagnostic line/column numbers in multi-file sources.

### Changed

- De-duplicated internal JSON/SQLite editor helpers to remove a unity-build symbol-collision risk.

## 1.4.0 - 2026-06-06

### Compatibility

- Added Unreal Engine `5.3` through `5.7` compatibility coverage.
- Verified single-plugin `RunUAT BuildPlugin` builds for UE `5.3`, `5.4`, `5.5`, `5.6`, and `5.7` on Win64.

## 1.3.9 - 2026-05-29

### Maintenance

- Updated plugin version metadata and documentation references.

## 1.3.8 - 2026-05-25

### Texture Support

- Added `VolumeTexture` property parsing, code generation, and default texture handling.
- Preserved texture object subtypes during code generation so `Texture2D`, `Texture2DArray`, and `VolumeTexture` inputs are passed to generated HLSL with the correct Unreal texture type.

### Plugin Cleanup

- Removed built-in shader library path support from project settings and documentation.

## 1.3.7 - 2026-05-18

### Decompiler

- Added reflected literal property export for generic `UE.Expression(...)` decompilation so unsupported `MaterialExpression` nodes retain more editable state.
- Preserved connected `TextureSampleParameter2D` graph inputs such as UV coordinates by exporting connected sample parameters as graph expressions instead of plain `Properties` declarations.
- Fixed decompiled `MaterialExpressionCustom` imports with dynamic named inputs and custom output type metadata.

### Performance

- Improved import performance for very large decompiled materials by reducing per-node package dirtying, throttling progress text updates, and skipping automatic layout on large generated graphs.

## 1.3.6 - 2026-05-12

### Build Fixes

- Added an explicit `MaterialDomain.h` include to `DreamShaderSettings.h` so projects that include the settings header directly can resolve `EMaterialDomain` reliably.

## 1.3.5 - 2026-05-11

### ShaderFunction Calls

- Added statement-style multi-output `ShaderFunction` and `VirtualFunction` calls in `Graph`, using positional inputs followed by output target variables.

### Dream Shader Function Files

- Added `.dsf` Dream Shader Function files for reusable generated `ShaderFunction` assets.
- Allowed `.dsm` and `.dsf` files to import `.dsf` files so generated functions can be reused across DreamShader sources.
- Added `.dsf` source discovery, dependency tracking, and VSCode workspace file association.

### Decompiler

- Added Content Browser export actions for `UMaterial` -> `.dsm` and `UMaterialFunction` -> `.dsf`.
- Decompiled files are written under `DShader/Decompiled/Materials` or `DShader/Decompiled/Functions` with unique file names.
- Common constants, parameters, arithmetic nodes, swizzles, texture samples, Custom nodes, and MaterialFunction calls are exported to DreamShader graph text; less common reflected nodes fall back to `UE.Expression(...)`.

## 1.3.4 - 2026-05-11

### Output Initializers

- Added support for initialized output declarations such as `vec3 Color = Tint;` inside `Outputs`.
- Allowed `Shader` blocks to use initialized output declarations with an empty `Graph = {}` block.

## 1.3.3 - 2026-05-11

### Graph Swizzles

- Fixed vector property component counts so declared `vec2` / `vec3` properties bind through `RG` / `RGB` instead of always using `RGBA`.
- Fixed non-sequential swizzles such as `.gbr` by generating explicit `ComponentMask` and `AppendVector` nodes.

## 1.3.2 - 2026-05-11

### Material Function Generation

- Preserved generated `ShaderFunction` input and output IDs across regeneration so existing `MaterialFunctionCall` nodes in regular Unreal materials keep their connections.
- Skipped unused generated property nodes in Graph and Custom/HLSL generation paths.
- Improved generated node placement and avoided Unreal's full automatic layout pass for DreamShader-generated material graphs.
- Fixed a crash when regenerating opened material function assets whose expressions were still rooted by the editor.

## 1.3.1 - 2026-05-09

### Function Calls

- Single-output `Function` and `GraphFunction` calls can now be used as value expressions, for example `Color = Texture::Sample2DRGB(BaseTex, UV0);`.
- Multi-output `Function` and `GraphFunction` calls still require explicit out variables, for example `Texture::Sample2D(BaseTex, UV0, Color, Alpha);`.

### Graph Functions

- Added top-level and namespaced `GraphFunction` blocks for reusable HLSL Custom-node logic.
- `GraphFunction` remains HLSL, but `UE.*` calls inside its body are converted into material nodes and passed into the Custom node as generated inputs.
- Added GraphFunction argument validation, recursive call detection, and explicit out-variable writeback.

## 1.3.0 - 2026-05-08

### Shader Layer Functions

- Added top-level `ShaderLayer(Name="...", Root="...")` and `ShaderLayerBlend(Name="...", Root="...")` blocks.
- Generated layer assets now use Unreal's native `UMaterialFunctionMaterialLayer` / `UMaterialFunctionMaterialLayerBlend` classes.
- `MaterialLayer` / `MaterialLayerBlend` remain compatibility aliases and emit warnings; new source should use `ShaderLayer` / `ShaderLayerBlend`.
- `ShaderLayer` / `ShaderLayerBlend` reuse the existing `Properties`, `Inputs`, `Outputs`, `Settings`, and `Graph` sections.
- Added validation that Shader Layer blocks output exactly one `MaterialAttributes` value, and Shader Layer Blend blocks declare at least two `MaterialAttributes` inputs.
- Vector parameter properties now keep their RGBA output available in Graph, so `.a` / `.w` can read alpha and assignments to lower component counts automatically use leading channels.

## 1.2.10 - 2026-05-08

### VSCode MaterialExpression Manifest

- Added editor-side export of reflected `UMaterialExpression` metadata to `Saved/DreamShader/Bridge/material-expressions.json`.
- The manifest is refreshed on editor bridge startup and when opening the DreamShader VSCode workspace.
- Exported metadata includes expression class names, editable reflected properties, expression inputs, output pins, and inferred DreamShader `OutputType` hints.
- Release workflow now downloads the latest `dreamshader-language-support` GitHub Release assets and attaches them to DreamShader releases.

## 1.2.6 - 2026-04-30

### ShaderFunction Properties

- Added `ShaderFunction` `Properties` as material-function-local property nodes.
- Added `const` property declarations for scalar, vector, and texture helper nodes that are not externally adjustable parameters.
- `ShaderFunction` `Inputs` preview defaults can now reference generated `Properties`, including texture object previews such as `opt Texture2D BaseColorTex = Tex;`.

## 1.2.5 - 2026-04-30

### Material Attributes

- Added `MaterialAttributes` as a graph value type for `Shader`, `ShaderFunction`, and `VirtualFunction` signatures.
- Added struct-like member writes such as `Attrs.BaseColor = Color;` and `Attrs.Roughness = Roughness;`.
- Added `Base.MaterialAttributes = Attrs;` output binding support and automatic `Use Material Attributes` enablement on generated materials.
- MaterialAttributes values can be returned from generated or virtual Material Functions and passed through Graph assignments.

## 1.2.4 - 2026-04-30

### Parameter Reflection

- Replaced the documented comma-style metadata suffix with a semicolon-based trailing reflection block for declarations.
- Parameter reflection blocks can now set any reflected `UMaterialExpression` property exposed by the generated parameter node.
- Basic `float` / vector / texture property shorthand declarations use the same reflection path as explicit parameter node declarations.
- Texture sample parameters can now configure reflected properties such as `SamplerType`, `SamplerSource`, `MipValueMode`, `AutomaticViewMipBias`, `ConstCoordinate`, and `ConstMipValue`.

## 1.2.3 - 2026-04-29

### Parameters

- Added declaration metadata `[Group="...", SortPriority=32, Description="..."]` for material `Properties` and function input/output declarations.
- Added explicit Parameter node declarations including `ScalarParameter`, `VectorParameter`, `TextureObjectParameter`, texture sample parameter nodes, `StaticBoolParameter`, and `StaticSwitchParameter`.
- Added `StaticSwitchParameter` graph calls, for example `UseDetail(True=detailColor, False=baseColor)`.
- Added `UE.CollectionParam(Collection=Path(...), Parameter="...")` for Material Parameter Collection reads.

### Function Defaults

- Added `opt` inputs for `ShaderFunction` and `VirtualFunction`.
- Added `default` call arguments for optional material function inputs, preserving Unreal FunctionInput preview defaults.
- Generated `ShaderFunction` assets now write input/output descriptions and sort priorities to FunctionInput / FunctionOutput nodes.
- VirtualFunction copy/create/sync now emits optional inputs, preview defaults, and pin metadata when available.

## 1.2.2 - 2026-04-29

### VirtualFunction Workflow

- `CreateVirtualFunction` now reuses the existing declaration for the selected Material Function instead of creating duplicate `.dsh` files.
- When a matching declaration already exists, the Material Function `DreamShader` menu shows `OpenVirtualFunction` and `Copy Virtual Function Reference` instead of the create/copy-definition actions.
- `OpenVirtualFunction` opens the existing declaration in VSCode and jumps to the declaration location when possible.
- Added startup validation and refresh for `VirtualFunction` declarations under `DShader`, reporting missing source `UMaterialFunction` assets and updating changed signatures.

### Import Compatibility

- `import "File.dsh"` now works with or without a trailing semicolon in the Unreal generator import pass.

## 1.2.1 - 2026-04-29

### Editor Workflow

- Replaced the single Material Function toolbar action with a `DreamShader` dropdown menu.
- Added `CopyVirtualFunction`, `CreateVirtualFunction`, and `CopyVirtualFunctionCall` actions to the Material Function editor toolbar and Material Function asset context menu.
- `CreateVirtualFunction` writes a `.dsh` declaration file under the configured `DShader/VirtualFunctions` directory and opens it in the default external editor.
- `CopyVirtualFunctionCall` copies a ready-to-paste Graph call using the generated input names and first output.
- Added `Open Dream Shader Workspace (VSCode)` to the editor Tools menu and DreamShader toolbar section. It writes `DShader/DreamShader.code-workspace`, opens it in VSCode when available, and falls back to the default editor or Notepad.

### Release

- Added a GitHub Actions release workflow that packages the plugin source and publishes a GitHub Release from version tags or manual workflow dispatch.

## 1.2.0 - 2026-04-28

### VirtualFunction

- Added `VirtualFunction(Name="...")` declarations for existing Unreal `UMaterialFunction` assets.
- `VirtualFunction` calls can be used from `Graph` like `ShaderFunction` calls, without generating or overwriting the referenced asset.
- `Options.Asset` supports `Path(Game, "...")`, `Path(Engine, "...")`, `Path(Plugin.PluginName, "...")` / `Path(Plugins.PluginName, "...")`, and full Unreal object paths.
- Added Material Function context-menu and Material Editor toolbar actions that copy a complete `VirtualFunction` declaration with inputs, outputs, and options.

### Asset Roots

- Kept `Root="Plugin.PluginName"` mapped to the project plugin content root, physically saving generated assets under `[Project]/Plugins/PluginName/Content`.
- `Plugins.PluginName` and `Plugins/PluginName` remain compatibility spellings.

### Tooling

- Updated the VSCode extension language service for `VirtualFunction`, plugin path completion inside `Path(Plugins.)`, snippets, hover text, signatures, and diagnostics.
- Updated plugin documentation for DreamShader `1.2.0`.
