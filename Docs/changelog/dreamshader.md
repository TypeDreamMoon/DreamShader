---
title: DreamShader Plugin
description: Release notes for the DreamShader Unreal plugin, newest first, taken from the plugin's own CHANGELOG.
---

Release notes for the Unreal plugin, newest first. Entries record what changes for you — the
language surface, the generator, the editor tooling and engine compatibility.

| | |
| :-- | :-- |
| Current release | **2.1.2** |
| Engines | Unreal Engine `5.3` through `5.8`, Win64; Custom Pass needs `5.8` |
| Source of record | the plugin's own `CHANGELOG.md`, shipped in the plugin folder |

## 2.1.2 - 2026-10-11

A release that corrects semantics: constant folding, the conditional operator and default arguments now
behave as the graph and HLSL do, so **every generated asset rebuilds once after the upgrade**. It also fixes
two ways a cook could abort, and the release archive finally carries the Chinese translation.

### Changed

- **Every generated asset is rebuilt once.** The build key's format tag is `DSK7` (`DSK4` in 2.1.1): the
  corrections below change what some sources generate, so nothing an earlier version wrote counts as current.
  No source needs touching. See [Regeneration](../generation/regeneration.md).
- **`round` rounds a halfway value to even, whatever its argument.** `round(0.5)` is 0 and `round(2.5)` is 2,
  as in HLSL. The native Round node agrees for a varying input, but its preshader rounds a uniform one up, as
  constant folding did — the same value rounded differently depending on where it came from. The builtin now
  lowers to a generated Custom node, which decompiles back to `round`; `UE.Round(Input = x)` keeps the native node.
  See [Math builtins](../builtins/math.md).
- **Constant `sin` and `cos` take cycles, as the graph nodes do.** The Sine and Cosine nodes keep their default
  `Period = 1`, but constant folding took radians, so `sin(0.25)` and `sin(T)` with `T = 0.25` disagreed. Both are
  1 now; an `@custom` body is HLSL and takes radians.
- **A graph cast keeps its value in constants too.** `(int)0.5` and `(bool)0.5` are 0.5, as they already were in
  the graph and through a variable; folding no longer truncates them, which could flip a constant condition.
  Use `trunc(...)` or `x != 0`.
- **Only the chosen arm of `c ? a : b` writes.** With a dynamic condition, both arms' assignments, `++` and
  `out` / `inout` writes reached the code after it, the false arm's last. Each arm now starts from the state after
  the condition and only the selected one takes effect, as with a constant condition or an `if`.
- **A default argument is evaluated in the callee.** `float F(float x, float y = x)` called as `F(7)` gives
  `y = 7`; it used to read the caller's parameter slot. Defaults that read each other resolve as they are needed,
  and a cycle is `DSH6224`. A call to an exported or `extern` material function leaves an omitted pin open for the
  asset's own default.
- **A Custom Pass buffer has one mip.** `Mips` above 1 was accepted, but only mip 0 was ever written and the rest
  were undefined; it is `DSH7308` now. Set existing pipelines' `Mips` to 1.
  See [Custom Pass pipelines](../language-v2/passes.md).
- **`dsc -CleanNew` deletes only what the commandlet saved.** It took every `.uasset` that appeared in Content
  during the run for its own, an editor's import as well. The commandlet now records each package it saves and its
  MD5, and the driver deletes a new file only when that record names it and its content is unchanged; without a
  valid record nothing is deleted and an otherwise successful run exits 1. Rebuild the plugin when updating the driver.

### Fixed

- **A plugin that ships its own `DShader` folder no longer fails a host project's cook.** An engine or
  marketplace plugin's sources that write `Root="Plugin.<Name>"` cannot generate in a project that merely
  installed it (`DSH8090`), and used to abort the whole cook. Such a failure is now logged as
  `[Cook] Not this project's to build (plugin '<Name>')` and the cook goes on — those assets ship with the plugin.
  The project's own sources, and those of plugins under `<Project>/Plugins`, still abort a cook they cannot generate.
  See [In-memory materials](../generation/ephemeral.md).
- **A cook on a clean workspace no longer depends on file order.** A material whose `VirtualFunction` names a
  function another source generates failed with `DSH8219` when its path sorted first, and the cook aborted. The cook
  and `compile -All` now share one batch: `.dsf` first, then passes in which a failure is retried while the pass
  before it built something else; what still fails once a pass builds nothing new is a real failure. Chains of
  functions and `.dss` exported functions work the same way. See [Commandlet](../tools/commandlet.md).
- **The release archive ships `Content/`, and with it the `zh-Hans` translation.** Every zip from 1.6.0 to 2.1.1
  lacked it, so an install showed English editor text only. Older installs can copy `Content/Localization/` in from
  the matching tag.
- **A generated material instance is not a hand edit after it is reloaded.** UE fills in each parameter override's
  `ExpressionGUID` on load, and the output digest counted it, so a `.dsi` instance read as `Diverged` after its first
  reload, its next compile was refused and the cook stopped on it. The digest leaves the GUID out now and every other
  field still counts; the format stays `DSD4`. Thanks to [@Yu2erer](https://github.com/Yu2erer) —
  PR [#38](https://github.com/TypeDreamMoon/DreamShader/pull/38).
- **Thirty findings of a review of 2.1.1** (PR [#49](https://github.com/TypeDreamMoon/DreamShader/pull/49), each
  with a regression test): a chained swizzle write such as `v.zyx.x = 1` writes the component it names; an outer
  return still reachable after a nested `if` whose branches all return is kept; saving an included `.dss` rebuilds
  what includes it; compiling an instance refreshes its stale ancestors; `TextureCollection`, `double4`, and Cube,
  Volume and Array textures survive instances, decompile and Adopt; a second editor on the project no longer
  rewrites the owning editor's bridge files; a pooled Custom Pass material instance no longer carries a previous
  pass's parameters, a Nanite stencil filter's `AND` is not an `OR`, a `Depth32` buffer clears to its configured
  value; enlarging the preview window no longer crashes D3D12.
- The plugin builds on its own (`RunUAT BuildPlugin`) on UE `5.5`, `5.6` and `5.8`.

## 2.1.1 - 2026-10-04

The 1.x language's documentation and behaviour line up with the 2.0 compiler that actually builds 1.x
sources, plus a set of 1.x text-compatibility fixes.

- **The 1.x pages describe the compiler that builds 1.x sources.** Since 2.0.0 a `.dsm`, `.dsf` or `.dsh` is read
  by the legacy front end and built by the 2.0 compiler, yet the language, graph, builtin, parameter, settings,
  generation and tool pages still quoted the deleted 1.x generator's messages and behaviour. Each page now names the
  `DSHnnnn` code that reports a condition and says what happens today; [Diagnostics](../diagnostics/index.md) says where
  diagnostics appear and how to read one, and maps the common 1.x messages to their codes.
- **A build key no longer depends on where the project is checked out.** The digest named sources and headers by
  absolute path, so a teammate's or a CI agent's checkout rebuilt and re-saved every generated asset. It names them
  project-relative now. The key format is `DSK4`, so every asset rebuilds once after upgrading.
- **Recompiling one material leaves another's diagnostics for a shared header alone.** A record belongs to the
  source whose run filed it and to that run; a successful compile's warnings also survive its material's shader
  compile.
- **A commandlet flag written with a value is read.** `-Force=true`, `-Check=1` or `-DryRun=yes` read as absent —
  `migrate -Check=true` migrated the file. Now `true` / `1` / `yes` / `on` are on, `false` / `0` / `no` / `off` are
  off, and any other value, an empty `-Force=` included, is `DSH9110`, after which the command does nothing and exits
  `1`. An error that belongs to no file is no longer printed with a made-up location.
- **1.x text compatibility:** `Function SelfContained` (and its old spelling `Inline`) embeds the functions it
  calls again; `UE.CollectionParam` works again, the transform spaces only 1.x had (`AbsoluteWorld` and the rest)
  are written as the engine's names with the warning `DSH5278`, and `UE.TransformPosition` without a `Destination`
  goes to `World`; a comma before the `)` of a block's attribute list and an exponent in a `Properties` default
  (`1e3`) read again; `7 / 2` in a 1.x `Graph` is 3.5 again (integer division only when both sides are integer
  constructor calls); a node with several outputs passed straight to a pin is its first output (`DSH5287`).
- **A `.dsm` builds its `Shader` and the function and layer blocks beside it**; the material calls them by name and
  the function assets are built first. A `Shader` block in a `.dsf` is `DSH2259`.
- **The agent skills write 2.0 sources**, and there is a sixth, `dream-shader-migrate`.

## 2.1.0 - 2026-10-04

- **Custom Pass: render passes of your own, written as `.dsp` pipelines (UE 5.8).** A `.dsp` file declares named
  buffers and an ordered list of passes, each at an injection point of the renderer — from `BeginView` before
  anything is drawn, through `AfterBasePass`, `AfterOpaque` and `BeforePostProcess`, to the points inside the
  post-process chain and `EndOfView` — and compiles to a `UDreamPassPipeline` asset, which decompiles back to `.dsp`.
  A pass is a fullscreen Post Process material (written in a `.dss`, reading the pass's buffers through
  `UE.UserSceneTexture`), fullscreen or compute HLSL written in the `.dsp` or in a `.usf` of your project, a mesh pass
  that draws the primitives a stencil value, a pass layer or a named list selects, or a clear or a copy. Buffers can
  keep their history across frames, and an exported buffer is a render target that ordinary materials
  (`UE.DreamPassBuffer`), Blueprints, UMG and Niagara read. Pipelines are activated by the project settings,
  `ADreamPassVolume`, `UDreamPassComponent` or the Blueprint / C++ API, which blend their parameter overrides as
  post-process volumes do. See [Custom Pass pipelines](../language-v2/passes.md),
  [Runtime](../language-v2/passes.md) and the [example](../examples/custom-pass.md).
- **A pass's HLSL can be written in its `.dsp`** (`hlsl { }`): whole functions, or just the entry's statements with
  the function written for you; the file-level `hlsl { }` holds code every such pass shares. A pre-check error is
  reported at its line of the `.dsp`. See [HLSL passes](../builtins/hlsl-library.md).
- **The generated VS Code workspace knows the 2.0 sources**: `.dss` and `.dsi` map to `dreamshader-dss`, `.dsp` to
  `dreamshader-dsp` (extension 2.1.0).
- **Fixed:** a slider range on a parameter that has none (`Slider` on a 1.x `vec3` property, `@slider` on a
  `float3` or `bool` uniform) is one warning, `DSH7233`, at the range, and the range is dropped; a successful
  compile's warnings reach `diagnostics.json`, so the extension shows them.

## 2.0.2 - 2026-10-02

- **The decompiler writes every Substrate node by name.** A Transmittance-To-MFP, a Coverage Weight, an Add or a
  Horizontal Mixing came back as sugar (`Slab * w`, `+`, `lerp`) — graph-exact, but a search of the text for the node
  the graph shows found nothing. They are now the calls they are, such as `Substrate.Weight(A = ..., Weight = ...)`;
  the sugar appears only in the readable form (`-Readable`). Sources written with the sugar compile as before.
- **Fixed:** a Switch whose cases are named decompiles as the Switch itself (one with an unnamed case is still the
  chain of branches, and `DSH9073` says to name the cases); an unwired pin's constant is written in the pin's place;
  the Landscape layer blend, grass output and physical-material output decompile with their pins
  (`Layer_Grass = ...`, `Height_Rock = ...`).

## 2.0.1 - 2026-09-27

- **Fixed decompiler gaps:** a Convert (Make / Break FloatN) is written back as the channels it computes
  (`DXY.xy`, `float3(Gain, 0.25, 0.75)`); a Switch as its branches; a material that reads a VertexInterpolator
  decompiles (`UE.VertexInterpolator(Input = ...)`); a SetMaterialAttributes pin the catalog does not know is
  named (`DSH9072`).
- **`Substrate.ShadingModels` has its `ShadingModel` pin**, so a computed shading model under `Substrate = Bridge`
  is wired to it.

## 2.0.0 - 2026-09-21

The 2.0 line: a compiler rewrite rather than a feature release. A language front end that depends on
nothing but Core, a graph IR between the parser and the material graph, the 1.x syntax carried as a
second front end of the same compiler, and the HLSL-shaped `.dss` language beside it. Everything since
`1.8.0` — conditional compilation and graph breakpoints from `1.9.x` included — is in it.

- **[DreamShaderLang 2.0](../language-v2/index.md)** — `.dss` sources: `uniform`s, `///` directives,
  `#pragma material(...)`, one asset per `export`, real expressions and control flow, helpers that inline,
  `/// @custom` HLSL bodies, `extern` prototypes for assets that already exist.
- **The Material Content Browser makes `.dss` sources.** *New ▾* has a DreamShaderLang 2.0 section —
  **Material (.dss)**, **Material function (.dss)**, **Instance (.dsi)** — ahead of the three 1.x
  templates; the name typed is the `export`'s, and so the asset's, and the folder decides where under
  `/Game` it lands.
- **One compiler, two front ends.** A `.dsm` / `.dsf` is read by a legacy front end into the same tree
  and built by the same emitter; measured against frozen 1.x graphs of four real source roots, every
  source rebuilds the same graph node for node. The 1.x generator and the 1.x parser are gone.
- **[Material instances as source](../language-v2/instances.md)** — `.dsi`, checked against the
  parent's real parameters.
- **[`dsc migrate`](../tools/migrate.md)** — 1.x to `.dss`, proving before it writes that no
  comment is lost and the same graph comes out.
- **A decompiler that writes 2.0 text**, by running the compiler backwards; what the language cannot
  say is named at the head of the file, never dropped silently.
- **[Substrate sugar](../language-v2/substrate.md)** — `A + B`, `A * w`, `lerp(A, B, t)`; legacy
  parameters on a slab with the conversion node added for you; run-time branches as `Substrate.Select`;
  values built member by member; `#pragma material(Substrate = Legacy | Bridge | Native)`.
- **Graph layouts that read the source**, as the project setting **Graph Layout Style**. `Blocks`, the
  default, is a page of boxes in source order — one per `#pragma region` or run of statements, each a small
  left-to-right drawing — with **no wire between two boxes**: a value another box reads gets a named
  reroute declaration (`DS_<variable>`) beside it and a usage in every box that reads it, and a constant
  is written again where it is read. The other two insert nothing: `SourceBands` is one band per
  statement, `Layered` the whole graph in layers. `dsc dump-layout` draws all three for any source and
  builds nothing, to look before choosing. The 1.x layout is now called `Classic` and can still be
  chosen. An existing graph takes the new arrangement the next time its source is rebuilt; node positions
  are not part of the divergence digest, so nothing reads as hand-edited.
- **Tooling** — `check` (and `check -Shaders`, which maps HLSL errors back to source lines), `fmt`,
  `list-generated` and a page on [source control](../generation/source-control.md), `dump-ir`,
  `index`, `export-catalog`; node ↔ source line navigation both ways; the symbol index and the node
  catalog written for the language service.
- **Graph materials and material functions always save.** Only a ThinCustom material still has a
  memory-only state.
- **Fixed: a generated layer, blend or attribute function read as hand-edited in an editor of another
  language.** The [divergence](../generation/divergence.md) digest named inputs by the engine's
  `GetInputName`, and `BreakMaterialAttributes`, `GetMaterialAttributes` and `SetMaterialAttributes`
  answer that with **translated text** — a digest stamped by an English editor did not match what a
  Chinese editor computed, so a team that uses both saw each other's assets as `Diverged` and was
  refused the rebuild. Those inputs are named by the attribute itself now, and the digest also covers
  which attributes a `GetMaterialAttributes` publishes. The format moves from `DSD3` to `DSD4`, so every
  generated asset reads as `Unstamped` once after the upgrade and is restamped by its next rebuild.
  `dump-graph` names the pins of those three nodes the same way.
- **Fixed: a layer blend could not write `FrontMaterial`.** A blend's result starts empty, so its
  writes become a `MakeMaterialAttributes`, which has no `FrontMaterial` pin (`DSH8212`). What Make has
  no pin for is set by a `SetMaterialAttributes` on top of it.

## 1.8.0 - 2026-08-21

The release about **not losing things**. Generation used to tear an asset down unconditionally: a
hand-edited asset was overwritten silently, a failed rebuild left the asset empty, and an asset left
open in the material editor had the rebuild quietly reverted by the next Apply. This version closes
all of those — an output fingerprint, an atomic rebuild, an open-editor gate, dependency-ordered
batches, and a build key that finally covers everything deciding what a source compiles into.

### Divergence — three ways out

- Every successful generation stamps a **`DreamShader.OutputDigest`** fingerprint of what the asset
  actually holds, and the next rebuild compares it **before** anything is cleared. A mismatch
  **fails the compile** with the asset exactly as you left it — not cleared, not half-built. See
  [Regeneration](../generation/regeneration.md).
- The digest covers **exactly what a rebuild would destroy**: nodes, connections, node properties,
  the reset-property set, a function's asset-level fields, and a ThinCustom instance's parameter
  overrides. It deliberately excludes what a rebuild never touches — node positions, comment boxes
  you wrote, pin GUIDs, and properties outside the reset list. Blocking on a property that was never
  in danger only refuses a rebuild for nothing.
- All three ways out are on the asset's right-click menu under **DreamShader**: **Revert to Source**
  (discard the edits and rebuild), **Adopt Into Source** (rewrite the `.dsm` / `.dsf` from the asset,
  backing the old one up to `<source>.bak`), and **Detach From DreamShader** (keep the asset, stop
  managing it).
- `-Force` does **not** get past it. `bForce` answers "is the source hash stale", and the editor
  asserts it for every file in its own startup sweep — honouring it would have left the gate dead in
  the mode the editor spends all its time in. Only Revert overrides a divergence, because only a
  person can make that call.
- Memory-only assets are stamped too, with the digest and the source **path** (but not the source
  hash, which would switch the [skip](../generation/index.md) on). Without the path an
  in-memory asset classifies as foreign and the gate never fires — dead in the editor's default mode.

### A rebuild is atomic

- The steps either all take effect or none of them do. The old graph is now **detached** rather than
  destroyed, and destroyed only once the rebuild has fully succeeded; a failure restores the asset
  from a serialized snapshot — render state, material-function usage, node graph, connections and
  `FunctionInput` / `FunctionOutput` pin GUIDs all come back, so existing call sites stay wired.
- This was necessary because **not every failure can be caught up front**: the whole-file parse, the
  `Settings` validation and the `Outputs` validation all run before the asset is touched, but a
  `Graph` block is compiled one statement at a time by the graph builder, which runs *after* the
  teardown. Before `1.8.0` such a failure left the asset **emptied** — bad for a material, much worse
  for a material function, whose call sites read their pins from the live asset: one bad `.dsf` took
  every material that called it down with it, with no undo (generated assets are deliberately not
  `RF_Transactional`).
- Two things fall out of it. The teardown's two deletion strategies collapsed into one — the
  node-by-node path existed to break inbound links, which is pointless when the graph leaves as a
  unit — so the former 1200-expression threshold is gone and every rebuild takes the fast path. And
  `DependentFunctionExpressionCandidates`, the second serialized node list whose stale entries used to
  arm a null-deref for the rest of the session after a failed function compile, is now reset at
  commit rather than at teardown.

### A rebuild is refused while the asset is open in an asset editor

- An asset editor **does not edit the asset**: `FMaterialEditor` duplicates it into a transient
  `UPreviewMaterial` and copies that duplicate back over the original on Apply or Save; the material
  instance editor writes back through a `UMaterialEditorInstanceConstant` wrapper. An editor left open
  across a rebuild is therefore holding a *pre-rebuild* copy, and the next Apply silently reverts
  everything the rebuild did — surfacing later as a divergence report on the compile after that, a
  long way from the cause.
- Compiles now stop with `Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. …`
  and say to close it.
- **Revert to Source** and **Adopt Into Source** are the exception: they close the editor themselves,
  act, and reopen it. Both are offered on that editor's own toolbar, and a menu item that is
  permanently dead where it is most useful is not a guard, it is a bug.

### The build key replaces the plain source hash

- `DreamShader.SourceHash` is now a **build key**: prepared source text, the **Default Compiler
  Backend**, the three mapping tables, the plugin version plus a hand-bumped format tag, and the
  engine version. It used to hash the source text alone, so anything *else* that changes the output
  left every already generated asset looking current.
- The forced full sweep that was bolted onto the backend setting is therefore gone: each affected
  asset fails the skip check on its own, and one the setting does **not** affect is still skipped
  instead of being needlessly rebuilt.
- Changing the key's composition invalidates every existing stamp — one rebuild per asset, once,
  which is the intended effect.

### A batch of source files compiles in dependency order

- A `.dsm` that calls a `ShaderFunction` binds its call node against the **live** `UMaterialFunction`
  asset — `SetMaterialFunction` reads the pins off the object, not off the source — so compiling the
  caller first bound it against the *previous* version of that function's interface. Renaming a
  function input and saving both files was enough to hit it, and which one won depended on the
  iteration order of a `TMap`.
- Both drain points — the watcher's pending-file batch and the whole-project sweep — now topologically
  sort by the import graph. Only edges inside the batch are honoured; a cycle is left for the import
  loader to reject with its own diagnostic.

### One editor owns the bridge for a project

- The bridge directory is per **project**, not per process: one `Requests` folder, one `status.json`,
  one heartbeat. Two editors open on the same project therefore both polled the same queue and both
  overwrote `status.json` with their own pid.
- Ownership is now a lock file, `Bridge/owner.lock`, carrying the owning pid and a heartbeat: an owner
  is believed while its process is alive **and** its heartbeat is under 30s old. Both tests are needed
  — the pid test alone hands the bridge over whenever the owner is mid-compile (a compile blocks the
  game thread), and the heartbeat test alone leaves it unowned after a hard crash. It is released on
  shutdown.
- A non-owning editor still compiles its own in-memory materials, but does not consume requests, write
  `status.json`, or **write generated assets to disk**. That last part follows directly from storage
  deciding how a rebuild persists: without it, two editors would both `SavePackage` the same file, and
  a save that loses that race is not a merge — it is a corrupted package or a dead editor. The
  commandlet is unaffected. See [Editor Tools](../tools/editor-integration.md).

### An asset that exists on disk is rebuilt on disk

- **Storage decides how a rebuild persists, not the compile that asked for it.** The editor asks for a
  memory-only compile every time, and when that landed on a package which already existed, generation
  rebuilt it in place and then cleared the dirty flag — producing an object that matched neither the
  file on disk nor anything that would ever be written, and that reported itself clean. A Save All
  could persist a state nobody chose, and the version visible in the editor disappeared on restart.
  The only signal was one log warning.
- `IsGeneratedAssetPersisted` now decides: an asset with a file behind it takes the persisted path —
  stamped, saved, dirty flag never faked — and one without stays memory-only. There is exactly one
  answer to "what is this asset" again. See [In-memory Materials](../generation/ephemeral.md).
- The startup sweep stopped forcing as part of this, and had to. Forcing was free while every in-memory
  asset regenerated regardless of its source hash; it stopped being free the moment a disk-backed asset
  started being *saved*, because every editor launch would then rewrite every persisted generated
  asset. Changing the **Default Compiler Backend** still forces, because the hash cannot see it.

### Fixed

- **A `Domain="Volume"` material carries `bUsedWithVolumetricCloud`, and decompiling one no longer
  turns the flag back on.** The flag is now derived from the domain, and an explicit
  `bUsedWithVolumetricCloud = "false";` still wins — a Volume material that only feeds volumetric fog
  can decline the cloud shader permutations. It is written through `UMaterial::SetUsageByFlag` (UE 5.8
  deprecates every `bUsedWith*` field), and the assignment moved out of the `Domain` branch to read the
  domain back off the material, because `ResetMaterialToDefaults` does not clear usage flags. The
  decompiler goes through the same `GetDefaultUsedWithVolumetricCloud`, so `UMaterial` → `.dsm` →
  `UMaterial` is an identity again. See [Material Settings](../settings/material.md). Reported in
  [#27](https://github.com/TypeDreamMoon/DreamShader/pull/27).
- **A swizzle survives the material editor.** A component selection like `CustomStencil.r` was written
  as an inline mask on the connection (`OutputIndex=0` plus `Mask/MaskR`), which the HLSL translator
  honours but the material graph editor cannot draw: pins correspond to an expression's `Outputs`
  entries, and no pin means "output 0, red channel only". `UMaterialGraph::GetValidOutputIndex`
  distrusts `OutputIndex 0` whenever a mask is present, finds no matching pin, and falls back to the
  node's **last** output — which the first Apply/Save then writes back. On a `SceneTexture` node
  (`Color / Size / InvSize`) that turned `CustomStencil.r` into `InvSize`, silently, with no error
  anywhere: the stencil comparison failed for every pixel and the effect the function gated simply
  stopped appearing. Any generated asset touched through the material editor was exposed, and
  duplicating one first did not help.
  Component selections are now emitted in a form the graph can round-trip. An inline mask is kept only
  where the editor resolves it back to the pin it already names; where the expression publishes the
  same value through several masked outputs (`TextureSample`'s `RGB/R/G/B/A/RGBA`, `VertexColor`, …)
  the matching output is named directly; otherwise a real `ComponentMask` node is emitted. Mixed output
  sets like `SceneTexture`'s are never retargeted, because there the outputs are different values
  rather than views of one. The build key tag moves to `DSK2`, so every generated asset is rebuilt once
  into the new form.
- **`UE.CollectionParam(...) Name;` in `Properties` is as wide as the collection parameter.** The
  parser records the declaration form as a scalar (it cannot open the collection), and the generator
  used that width as-is, so a vector MPC parameter passed to a `float4` input was widened by three
  `AppendVector` splats and the material failed with `Can't append float4 to float4`. The width now
  comes from the loaded collection (vector → 4, scalar → 1); an explicit `OutputType=` still wins. See
  [Properties](../language/properties.md).
- **A render target is accepted where a texture of its dimension is expected.** The dimension check
  compared asset classes; it now reads `UTexture::GetMaterialType()` — the same answer the material
  compiler gives on a texture-object pin — so `UTextureRenderTargetVolume`, `UTextureRenderTarget2D`,
  `UTextureRenderTargetCube` and `UTextureRenderTarget2DArray` pass for their dimension, and anything
  else a texture-object pin would take does too.
- **Cook-generated assets are registered with the AssetRegistry, so they reach the package.** A cook
  request is resolved through `IAssetRegistry::DoesPackageExistOnDisk`, which consults only the
  registry's in-memory state and has no filesystem fallback; generation runs on post-engine-init, after
  the registry enumerated the content directories, so a freshly written package was invisible to that
  lookup — the material simply was not in the pak, and `LoadObject` failed at runtime in a Shipping
  build, with no error anywhere. Generation now records what its saves actually wrote and hands the
  filenames to `IAssetRegistry::ScanModifiedAssetFiles` before the commandlet's `Main` collects the
  initial requests. Reported in [#26](https://github.com/TypeDreamMoon/DreamShader/issues/26).
- **The ThinCustom instance path runs the ownership guard.** It checked only the class of whatever sat
  at the target path, not its provenance, so generating onto a hand-authored
  `UDreamShaderMaterialInstance` adopted it and then cleared its parameter overrides. Since ThinCustom
  is the default backend this was the widest of the three creation paths and the only one without the
  check.
- **`DreamShaderGraphDecompilerHelpers.h` compiles on its own** — it declared functions taking
  `EMaterialDomain` and `EBlendMode` without including either enum's header, which a unity build only
  ever had through a neighbouring translation unit.

### Added

- **`#include` at the top of a `Function` body is hoisted to file scope.** A `Function` block's HLSL
  used to be emitted verbatim between the braces of the generated `DreamShaderFn_*` definition, which
  made any `#include` land *inside* a function — fine for macro-only headers, a compile error for
  anything that defines a function. Leading `#include "…"` / `#include <…>` directives (only whitespace
  and comments before them) are now stripped from the body and emitted once, in first-seen order, right
  after the guard of the generated `.ush`; a `SelfContained` embed or a `GraphFunction` body puts them
  on the Custom node's `IncludeFilePaths` instead, ahead of the generated include. A directive after
  the first statement keeps the old behaviour. This is what lets a header shared between C++ and HLSL
  be consumed from a `.dsf` without copying its functions into DreamShaderLang. See
  [Functions](../language/functions.md).

## 1.7.1 - 2026-08-16

### Fixed

- **Closing the editor crashed after using a Graph breakpoint.** An access violation reading `0x3b0` in
  `UMaterial::GetExpressionInputDescription`, from the probe preview's own destructor, reached through
  `FDreamShaderPreviewWebSocketServer::Shutdown` while modules were unloading.
  Modules unload from inside the exit path: `EngineExit()` raises the exit request, `FEngineLoop::Exit()`
  runs the purge, and only then calls `UnloadModulesAtShutdown()` — so a module tearing down its own
  state at that point is doing it *after* the objects it points at are gone. `TStrongObjectPtr` keeps
  the preview material out of the garbage collector's reachable-set sweep, but it does not exempt it
  from the exit purge: the pointer stayed non-null while the object behind it did not, which is why the
  existing null checks passed and the dereference still faulted.
  What it faulted on is worth naming, because nothing about the call site suggests it —
  `UMaterial::GetExpressionInputDescription` and `GetExpressionCollection` both dereference
  `GetEditorOnlyData()` without checking it, so reaching for a material's inputs or expressions after
  the purge is an unconditional null-plus-offset read rather than a recoverable failure. The teardown is
  now skipped once the engine is exiting, which is correct and not merely safe: releasing the shared
  expression collection exists so the preview material cannot touch the graph material's nodes *later*,
  and at exit there is no later. The ordinary path, where a client disconnects while the editor keeps
  running, is unchanged.

## 1.7.0 - 2026-08-16

### Added

- **Graph breakpoints in the live preview — "Start Previewing Node" for a text source.** Set a
  breakpoint on a `Graph` line and the preview mesh shows the value bound at that line instead of the
  finished material. This is the same idea as right-clicking a node in the Material Editor and choosing
  *Start Previewing Node*, except the "node" is a line of DreamShaderLang.
  It reuses the engine's own machinery: the generator publishes a per-source **debug table** mapping
  every `(line, name)` binding to the exact `UMaterialExpression` / output / channel-mask it produced,
  and a transient `UPreviewMaterial` — sharing the generated material's expression collection, with the
  probed node wired into its emissive (or `MaterialAttributes` / `FrontMaterial` for those value kinds,
  and a default-coordinate sample for a texture object) — is recompiled through
  `FMaterialUpdateContext`. `UPreviewMaterial::ShouldCache` keeps that recompile to a handful of
  shaders, exactly as the node-thumbnail path does.
  A breakpoint on a blank or comment line snaps forward to the next line that binds a value; one set
  before the source has ever generated is remembered and attaches on the next compile; after a recompile
  the probe re-resolves automatically and the client is told the line it landed on. Wire protocol:
  `setProbe` / `clearProbe` / `probeState`. See [Editor Tools](../tools/editor-integration.md).

### Changed

- **The streaming preview now sends raw RGBA8 frames instead of a PNG per frame.** A new
  `encoding: "raw"` session reads the render target back and streams the pixels as one self-describing
  binary frame (a 24-byte header — size, flags, camera, resolved probe line — then the pixels), with no
  PNG encode on the editor side. A browser/webview client paints them straight onto a canvas. This is
  most of what a smooth 30–60 FPS stream costs; PNG-encoding a 512² frame per tick was tens of
  milliseconds of game-thread time. The legacy `encoding: "png"` path is unchanged for older clients.
- **Streaming got cheaper when nothing is moving.** Identical frames are dropped, and after a few in a
  row the render clock backs off to a low idle rate until an edit, a camera/mesh change, or a probe
  change re-arms it (a frame rendered while shaders are still compiling never backs off). A
  `previewControl` that omits a field — including `frameRate` — now keeps the current value rather than
  resetting to 2 FPS, and it can now carry `width` / `height` / `mesh` so the client can drive the
  render size and shape without a full re-request. A `force` flag on `previewMaterial` distinguishes an
  explicit refresh (regenerate) from a camera/mesh re-request (reuse the last generation).

## 1.6.0 - 2026-08-15

### Language — ten more math builtins

- `Graph` gains `step`, `smoothstep`, `length`, `cross`, `asin`, `acos`, `atan`, `atan2`, `reflect` and
  `refract`. Every one of them had a node — or, for `reflect` and `refract`, a four-line definition —
  and none of them had a spelling, so the only way to write `step(0.5, x)` was
  `UE.Expression(Class="Step", OutputType="float1", Y=0.5, X=x)`, and the failure when you wrote it the
  HLSL way was `Unknown Graph function 'step'` wrapped inside whichever call the argument belonged to.
  The builtin surface goes from 19 spellings to 29. See [Math Builtins](../builtins/math.md).
- Three of the new names do not wire to a pin called `Input`, which is the whole reason the mapping is
  worth stating: `Step` and `Arctangent2` name their pins `Y` and `X`, and `SmoothStep` names them
  `Min` / `Max` / `Value`. Argument order stays HLSL's in every case, so `step(edge, x)` wires argument
  1 to `Y` and argument 2 to `X` and still means `x >= edge`.
- `length` and `cross` join `dot` as the builtins with a fixed return width — 1 and 3 components,
  authoritative.
- `reflect` and `refract` are the **first builtins that are not one node**. Unreal has no expression for
  either, so they are lowered to the arithmetic HLSL defines them as: four nodes for
  `i - 2 * dot(i, n) * n`, and fourteen for refraction including the `If` that returns zero under total
  internal reflection. Both are exact and both are expensive; where the surrounding code is already
  HLSL, a `Function` body still does it in one node.

**Warning:**

**This widens a reserved namespace.** These ten names now shadow user code silently, the same way the
original nineteen do — a `Function`, property or `ShaderFunction` named `length`, `step` or `cross`
becomes unreachable from a `Graph` block with no diagnostic. Existing sources that declare one need
renaming.

Not added, and not addable: matrices. The Unreal material graph has **no matrix value type at all**, so
`mul(M, v)` has no `Graph` spelling regardless of what the DSL does —
`UE.Expression(Class="Transform"/"TransformPosition", …)` covers space conversions and a `Function` HLSL
body covers the rest. Still absent but reachable through `UE.Expression`, each having a node:
exponential, logarithmic, `tan`, `sign`, `round`, `trunc` and `distance`. The decompiler is unchanged,
so the new nodes export as generic `UE.Expression(Class="…", …)` calls rather than round-tripping to the
builtin spelling — the same asymmetry `Fmod` already has. `reflect` and `refract` cannot round-trip at
all, since they leave behind ordinary arithmetic nodes with nothing marking their origin.

### Diagnostics — `DSHnnnn` codes

- Until now the only thing identifying a DreamShader failure was its **English wording**. All 120 parser
  raise sites now carry a code instead: `DSH1xxx` path resolution, `DSH2xxx` lexer and syntax, `DSH3xxx`
  sections and declarations, `DSH7xxx` properties, parameters and settings. The code rides on
  `FDreamShaderTextError` alongside the `FText`. See [Diagnostics](../diagnostics/index.md).
- **Nothing on the wire changed** — `FDreamShaderDiagnosticRecord::Code` and the `"code"` field in
  `diagnostics.json` already existed.
- The generator does not raise codes yet: 112 codes are documented, while the full catalogue of all 659
  messages, grouped by stage, stays authoritative until the generator's remaining ~561 raise sites are
  tagged too.

### Localization — the editor UI speaks your language; the wire format does not

- Editor-facing text — the *DreamShader Gen* page, the settings section, slow-task progress, parser and
  decompiler diagnostics — moved from `FString` literals to `LOCTEXT` / `NSLOCTEXT`, and the diagnostic
  records that carry it are `FText`. Simplified Chinese ships with it:
  `Content/Localization/DreamShader/zh-Hans/DreamShader.locres`, loaded through the
  `LocalizationTargets` entry in `DreamShader.uplugin` with an `Editor` loading policy. `FString`
  overloads are kept alongside every converted signature. Thanks to
  [@youli42](https://github.com/youli42) — PR [#24](https://github.com/TypeDreamMoon/DreamShader/pull/24).
- **`diagnostics.json`, `diagnostics/*.json`, `bridge.db` and the preview WebSocket stay English in every
  editor culture.** The VSCode and Rider extensions parse those, so a translated editor must not change
  a byte of them. `FTextInspector::GetSourceString` is only half an answer: it hands back the source
  string for a plain `LOCTEXT`, but for an `FText::Format` result it returns the *localized* substituted
  display, so one message site adopting `FText::Format` would have leaked translated text onto the wire.
  `ToInvariantWireString` instead replays an `FText`'s historic format data — the source pattern, every
  argument rendered in the invariant culture, nested formats recursively, number grouping off so `12345`
  never becomes `12,345` — and every JSON / SQLite / WebSocket write goes through it.

### Plugin source roots

- Every enabled plugin that ships a `DShader` folder now contributes its own source root, so a plugin
  can carry the `.dsm` / `.dsf` / `.dsh` files that build its materials instead of parking them in the
  project's tree. `Root="Plugin.<Name>"` has been able to *write* assets into a plugin since `1.2.0`;
  the source half was missing. Discovery, the dependency graph and generate-all pick plugin roots up
  with no further configuration. See [Project Layout](../getting-started/project-layout.md).
- *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Paths ▸ **Scan Plugin Source Directories***
  (`bScanPluginSourceDirectories`, default on) turns the plugin scan off.
- **A plugin-root file defaults to its own plugin's mount point.** A `.dsm` under
  `Plugins/MoonToon/DShader` with no `Root=` attribute now generates into `/MoonToon`, not `/Game` —
  source and asset stay in the plugin that ships them. Only an absent or whitespace-only `Root` is
  defaulted, so `Root="/"` opts back into `/Game`.
- **Imports never cross roots.** A file resolves its imports against its own root and that root's
  `Packages` folder only. Two plugins shipping the same relative path can no longer shadow one another,
  and disabling a plugin cannot silently change what another root's import means.
- **Root-qualified imports** — `import "Plugin.MoonToon:Shared/Toon.dsh";` — are the one way to cross a
  root deliberately. The qualifier is `Project`, `Plugin.<Name>` or `Plugins.<Name>` (`/` spells the
  same as `.`), matched case-insensitively. The `:` is load-bearing: `Plugin.MoonToon/Shared/Common.dsh`
  could not be told apart from a relative path through a folder of that name. See
  [Imports](../language/import.md).
- The source-directory watcher registers one watch per root, `DreamShader.code-workspace` lists one
  `folders` entry per root, and the *DreamShader Gen* page labels a plugin-root file with its root name
  in the row subtitle — typing a plugin's name filters to everything it ships.

### Editor and generation

- **Graph layout runs on in-memory materials.** Interactive compiles are memory-only, and the placement
  pass used to be skipped for them outright — so the graph you saw after a save was not a badly laid out
  graph, it was an *unlaid out* one: the tall single column of construction coordinates, wires strung
  across it. *Project Settings ▸ … ▸ Compiler ▸ **Lay Out In-Memory Graphs***
  (`bLayoutInMemoryGraphs`, default on) turns it back off. See
  [Project Settings](../settings/project.md).
- **Automatic layout places nodes at their real size.** Every node used to be assumed `320 × 150` and
  spaced on a fixed `420 × 220` grid, so nothing that draws taller than 220 fitted — a `TextureSample`'s
  preview thumbnail alone is 106, a `Custom` node grows a row per pin — and those nodes overlapped their
  neighbours and pushed out through the comment box meant to contain them. Sizes are now estimated from
  what the node widget actually assembles.
- **Long edges get lanes, and chains come out straight.** An edge spanning several ranks now reserves a
  dummy slot in each column it crosses. Crossing reduction only ever compares neighbours one rank apart,
  so without lanes it could not see those edges at all — which is what let a graph read as a ball of wire
  even though each column, taken on its own, was tidy. Placement then runs four straightening passes,
  each repairing the column with an isotonic regression.
- **Blocks pack into columns instead of one stack.** A material with several connected outputs used to
  become a ribbon tens of thousands of units tall. Blocks are now packed into columns against a height
  budget of `max(2400, sqrt(totalArea / 1.6))`.
- **The *DreamShader Gen* page reads diagnostics from the bridge**, not by re-parsing the
  `diagnostics.json` the bridge had just written. One consequence worth knowing: those records live for
  the editor session, so the page no longer shows errors left over from a *previous* session before you
  compile again.

### Fixed

- **The plugin builds again on UE 5.5 and 5.6.** Five UE 5.7 APIs had been used without a gate, so the
  plugin compiled only on the engine it was written against. All five now route through
  `DreamShaderVersionCompat.h`: `Materials/MaterialParameters.h` (5.7 and later),
  `UMaterialExpression::ShouldShowPreview()` (5.7), `UMaterialExpressionScalarParameter::ControlType` and
  its two neighbours (5.7), `UMaterialExpressionCustomOutput::GetInputValueType` (5.6), and six engine
  expression classes — `SceneDepth`, `SceneColor`, `ObjectRadius`, `ObjectBounds`, `PerInstanceRandom`,
  `PerInstanceFadeAmount` — that are `UCLASS()` with no export macro and whose `StaticClass()` does not
  resolve from a plugin before 5.6. That last one is an `LNK2019`, not a compile error: it passes every
  compile-time check and only a full `RunUAT BuildPlugin` sees it. Below 5.6 the class is looked up by
  script path instead, so the `UE.*` builtins behave the same.
- Two missing includes that unity builds had been papering over (`UObject/Package.h`,
  `Engine/EngineTypes.h`), which only a non-unity compile exposes.

### Tools

- **The editor bridge answers now, and says whether it is alive.** It had an inbound half and nothing
  else: a request produced no reply, no error file and no log line — a malformed one simply vanished —
  and a client's only way to guess whether an editor was running at all was to look for `bridge.db` and
  hope. The bridge now publishes `Bridge/status.json` (protocol, pid, project, plugin version, `busy` /
  `busyAction`, `lastResult`, heartbeat), rewritten every 2 s and **deleted on shutdown** so a missing
  file means "not running" definitively rather than "timed out", and answers any request carrying a
  `requestId` in `Bridge/Responses/<requestId>.json` with `ok`, `durationMs`, `message` and this
  compile's diagnostics. A `ping` action was added; bad scopes, missing `sourceFile` and unknown actions
  are now error responses instead of silent no-ops. See [Editor Tools](../tools/editor-integration.md).
- **`.skill/build-plugin.ps1`** — `RunUAT BuildPlugin` across a list of engine roots, one `PASS` / `FAIL`
  line each, exit code = the number that failed. This is what caught the five ungated newer-engine APIs
  above; an editor build against one engine cannot, and neither can any compile-time check, because one
  of the five is a link error.
- **`Tools/Localization/localization_lint.ps1`** — checks `LOCTEXT_NAMESPACE` define/undef pairing,
  rejects it in headers, and flags literals that gather cannot see, with an `I18N-EXEMPT` escape for text
  that is deliberately not display text.

## 1.5.1 - 2026-08-02

Documentation and tooling only. No plugin code changed, so a project on `1.5.0` needs no migration.

### Added

- `.skill/` — an agent skill set for DreamShaderLang, in the Claude Code `SKILL.md` format:
  `dream-shader-create` (a description becomes a `.dsm` that provably compiles),
  `dream-shader-optimize` (decompiler output becomes a source a human would write),
  `dream-shader-decompile`, `dream-shader-verify` and `dream-shader-diagnose`.
- `.skill/dsc.ps1` — a headless driver around `-run=DreamShader`. It resolves the engine from the
  `.uproject`'s `EngineAssociation`, finds the project by walking up, de-duplicates the doubled `LogInit`
  echo of every `LogDreamShader` line, and classifies each asset the run wrote against git. `-CleanNew`
  then deletes exactly the untracked ones and prunes the emptied folders.
- `.skill/sync-skills.ps1` — publishes the tree into `.claude/skills`, rewriting the relative `Docs/`
  links and the driver path against the destination. `-Check` exits `1` on drift.

### Changed

- Both READMEs restructured around the reference manual. The sections that had become abridged copies of
  `Docs/` pages — Properties, Graph, MaterialAttributes, Substrate, Material Layers, VirtualFunction,
  Configuration, Release — now link to the page that owns them, which is also the page that gets
  maintained. 459 → 279 lines, and the two languages are kept structurally identical.

### Fixed

- The release archive now ships `Shaders/`, `README.zh-CN.md` and `.skill/`. Up to `1.5.0` the packaging
  step copied a seven-item allowlist and skipped anything missing **silently**, so an archive install had
  no `Shaders/DreamShaderBuiltins.ush` for the `/Plugin/DreamShader` virtual shader directory to resolve
  against.

## 1.5.0 - 2026-08-02

The release that unifies compilation. The two old backends collapse into one **ThinCustom** path,
generated materials become memory-only in the editor, and a new **Material Content Browser** tab is
where you look at them. The language picks up optional section `=`, `Group()` scopes and
`Slider()`.

### Language (DreamShaderLang 1.5)

- Section `=` is now **optional**: write `Properties { … }`, `Settings { … }` and `Graph { … }`
  without the assignment. Both spellings stay valid — see
  [Sections](../language/sections.md).
- `Properties Group("Name") { … }` scopes a group onto every parameter it contains. Groups nest and
  compose, so `Group("Outer") { Group("Inner") { … } }` yields `Outer|Inner` — see
  [Metadata and Groups](../parameters/metadata.md).
- `Slider(min, max)` shorthand sets a scalar parameter's UI range. It expands to the reflected
  `SliderMin` / `SliderMax` properties and is the one metadata entry written without an `=`.
- Asset paths can follow `=` directly, and bare quoted paths are accepted — see
  [Asset References](../parameters/path.md).
- Single-output functions can be used as return values (`x = Fn(…)`), and `Graph` builtins now match
  the `Function` path: `fract`, `mod` and `fmod` — see [Math Builtins](../builtins/math.md).
- Live preview streaming keeps the editor and language-server previews in sync while you type.
- `true` / `false` are graph literals and materialize as `StaticBool` nodes, so an
  `opt StaticBool X = false` input default generates the Preview-pin node Unreal requires — it
  ignores `PreviewValue` for static-bool inputs.
- `StaticBool` resolves as a one-component type at call sites.
- Texture parameter types whose token carries no dimension — `TextureObjectParameter` — take their
  dimension from the assigned default asset, so a `Texture2DArray`, `TextureCube` or `VolumeTexture`
  default is accepted. Explicit tokens such as `Texture2D` and `Texture2DArray` still validate
  strictly.

### Backend — one unified compilation path

- The `Graph` and the experimental `Instance` backends are collapsed into a single **ThinCustom**
  path: DreamShaderLang compiles to a real node graph on a hidden base `UMaterial`, wrapped by a
  thin `UDreamShaderMaterialInstance`. The engine compiles and enumerates the material natively, so
  Substrate, static switches, virtual textures, MaterialAttributes and cook correctness all come
  from the real graph. See [Backend](../settings/backend.md).
- Bit-identical SM6 render parity with the previous `Graph` backend, verified across Unlit,
  textured, and DefaultLit MaterialAttributes cases.
- The hidden base is a **subobject of the instance** — one asset, one package, invisible in the
  Content Browser, with no separate `MB_DreamThinBase_*` sibling and no cross-package parent import
  to lose at cook. See [In-memory Materials](../generation/ephemeral.md).
- A single **Default Compiler Backend** project setting replaces the old In-Memory toggle. See
  [Project Settings](../settings/project.md).

**Deprecated since 1.5.0.** Use `ThinCustom` instead.

`Backend = "Instance"` and `DefaultBackend = Instance` are retained as **aliases** for
`ThinCustom`. The legacy graphless instance backend is retired; there is no runtime `Instance`
backend left. The spelling is kept for one deprecation window so existing sources keep compiling,
and it produces no diagnostic.

### Editor — Material Content Browser

- New **DreamShader Material Content Browser** tab under *Tools ▸ DreamShader*, with two pages:
  **Project** browses, filters and inspects every material and material instance under `/Game`,
  including the full inheritance chain; **Dream Shader Gen** lists the sources with live preview,
  search, filters, compile-all, and load-time error surfacing. See
  [Editor Tools](../tools/editor-integration.md).
- Create material instances from any material through a folder picker, and **materialize**
  in-memory (preview-only) materials to disk on demand.
- Content Browser context-menu actions, and a toggle to show or hide DreamShader's memory-only
  materials.

### Decompiler

- Faithful round-trip for Substrate materials and renamed graph channels. The exporter derives
  channel swizzles from the **write mask** rather than the channel name, so recompiled materials
  match the source bit-for-bit. See [Decompiler](../tools/decompiler.md).

Decompiling a hand-authored material and generating it back could fail on graph shapes Unreal
itself accepts. Found on LGUI's `LexUI_ImageAndFont`, `LexUI_RectBlock` and `MF_LexUI_SDF_Font`.

- Switch-style nodes — `StaticSwitch`, `FeatureLevelSwitch`, `QualitySwitch`, `ShadingPathSwitch`,
  `VertexInterpolator` and friends — report no output value type, so the "assume float4" fallback
  oversized them and everything downstream. An `AppendVector` fed by a float3 material-function
  output was emitted as `float5(...)`; appends are now clamped to a float4, with a warning when a
  count still disagrees.
- `VertexColor` is emitted as float4 so the alpha pin's swizzle is valid. It used to be typed from
  the RGB pin and produce `.a` on a three-component value.
- An input's own channel mask now **replaces** the connected pin's mask instead of stacking on it,
  so `.rgb.a` no longer appears when a graph wires the RGB pin but masks alpha.
- A `StaticBool` function input keeps the `StaticBool` type token. `bool` declares a scalar pin, so
  the input used to come back as a float and reject every static-bool value passed to it.
- Comment, `#Region` and description text carrying newlines or tabs is escaped, so a multi-line
  comment no longer splits the directive across lines.
- A Custom node's additional outputs are declared on **every** emission of that node, and reading
  one no longer rewrites the node's own return type. Previously the emission that did not select the
  extra output produced a node without it, and the code body assigning to it failed at
  shader-compile time with `use of undeclared identifier` — long after generation reported success.

### Fixed

- Cook: assets are materialized on the cook **director** only, and a generation error now fails the
  cook instead of shipping a stale asset.
- Generation refuses to overwrite assets DreamShader did not generate, and pre-validates graph
  syntax before clearing the target material.
- Generated-include identity hashes the **project-relative** source path; stored source paths are
  project-relative and no longer carry a generated-at timestamp.
- Runtime builds: guarded the editor-only `UEnum::HasMetaData` call so non-editor and Shipping
  (store) builds compile (#12).
- Bridge: adopted `FCoreDelegates::GetOnPostEngineInit` for UE 5.8, and constrained *Clean Generated
  Shaders* to `Intermediate` with per-file deletes.

### Compatibility

- Unreal Engine `5.3` through `5.8` (Win64).

## Source snapshot - 2026-06-10

Landed in the source tree between `1.4.0` and `1.5.0`.

### Automation baseline

- Added a DreamShader automation-test baseline covering minimal material parsing, minimal material
  generation, `.dsf` plus `import` generation, Substrate material generation, source-hash skip, and
  a commandlet single-source compile smoke test.
- The tests write temporary `.dsm` / `.dsf` / `.dsh` files under `DShader/Tests/Automation`,
  generate temporary assets under `/Game/DreamShaderTests/Automation/…`, and clean up after the run.
- `source hash is unchanged` is the stable assertion text for an unchanged source file skipping a
  duplicate generation.
- Added the explicit bridge kill switch `-NoDreamShaderEditorBridge` for automation runs, so bridge
  startup scans stay out of the test log.

## 1.4.0 - 2026-06-06

### Compatibility

- Added Unreal Engine `5.3` through `5.7` compatibility coverage.
- Verified single-plugin `RunUAT BuildPlugin` builds for UE `5.3`, `5.4`, `5.5`, `5.6` and `5.7` on
  Win64.
- UE `5.7` remains the active development target; UE `5.3` and `5.4` may need the MSVC `14.38`
  toolchain on Windows.

### Substrate

- Completed the Substrate generation path: `Substrate` graph values, the `Base.FrontMaterial`
  output binding, and the `Substrate.*` wrappers. Substrate itself requires **UE 5.4 or newer** with
  Substrate enabled in the project — see [Substrate Nodes](../builtins/substrate.md).
- The generator, the decompiler, type propagation and the editor manifest all recognize the
  Substrate type and its wrappers.

### Material Preview

- The editor bridge can resolve a `.dsm` source, generate a preview material, and write
  `Saved/DreamShader/Bridge/preview.json` plus `Preview/*.png`.
- Added a local WebSocket preview service, listening on `127.0.0.1:17864` by default, which feeds
  results and continuous frames to the VSCode preview panel.

## 1.3.9 - 2026-05-29

### Maintenance

- Plugin version metadata updated to `1.3.9`.
- README, release notes and documentation references synchronized with the current language
  capabilities.

## 1.3.8 - 2026-05-25

### Texture Support

- Added `VolumeTexture` property parsing, code generation and default-texture handling.
- Texture object subtypes are preserved during code generation, so `Texture2D`, `Texture2DArray`
  and `VolumeTexture` inputs reach the generated HLSL with the correct Unreal texture type.

### Plugin Cleanup

- Removed built-in shader library path support from the project settings and the documentation.

## 1.3.7 - 2026-05-18

### Decompiler

- Generic `UE.Expression(...)` decompilation exports reflected literal properties, so unsupported
  `MaterialExpression` nodes retain more editable state.
- `TextureSampleParameter2D` nodes with connected graph inputs — UV coordinates, for instance — are
  exported as graph expressions instead of plain `Properties` declarations.
- Fixed decompilation of `MaterialExpressionCustom` nodes with dynamic named inputs and custom
  output-type metadata.

### Performance

- Improved import performance for very large decompiled materials: less per-node package dirtying,
  throttled progress-text updates, and automatic layout skipped on large generated graphs.

## 1.3.6 - 2026-05-12

### Build Fixes

- `DreamShaderSettings.h` includes `MaterialDomain.h` explicitly, so projects that include the
  settings header directly resolve `EMaterialDomain` reliably.

## 1.3.5 - 2026-05-11

### ShaderFunction Calls

- `Graph` accepts statement-style multi-output `ShaderFunction` and `VirtualFunction` calls:
  positional inputs first, output target variables after.

### Dream Shader Function Files

- Added `.dsf` Dream Shader Function files for reusable generated `ShaderFunction` assets.
- `.dsm` and `.dsf` files can import `.dsf` files, so generated functions are reusable across
  DreamShader sources.
- Added `.dsf` source discovery, dependency tracking, and the VSCode workspace file association.

### Decompiler

- Added Content Browser export actions: `UMaterial` to `.dsm`, `UMaterialFunction` to `.dsf`.
- Decompiled files are written under `DShader/Decompiled/Materials` or
  `DShader/Decompiled/Functions` with unique file names.
- Common constants, parameters, arithmetic nodes, swizzles, texture samples, Custom nodes and
  MaterialFunction calls are exported as DreamShader graph text; less common reflected nodes fall
  back to `UE.Expression(...)`.

## 1.3.4 - 2026-05-11

### Output Initializers

- `Outputs` accepts initialized declarations such as `vec3 Color = Tint;`.
- A `Shader` block can use initialized output declarations with an empty `Graph = {}` block.

## 1.3.3 - 2026-05-11

### Graph Swizzles

- Fixed vector property component counts, so `vec2` / `vec3` properties bind through `RG` / `RGB`
  instead of always using `RGBA`.
- Fixed non-sequential swizzles such as `.gbr` by generating explicit `ComponentMask` and
  `AppendVector` nodes.

## 1.3.2 - 2026-05-11

### Material Function Generation

- Plugin version metadata updated to `1.3.2`.
- Generated `ShaderFunction` input and output IDs are preserved across regeneration, so existing
  `MaterialFunctionCall` nodes in ordinary Unreal materials keep their connections.
- The Graph and Custom/HLSL generation paths skip unused generated property nodes.
- Improved generated node placement, and avoided Unreal's full automatic layout pass on
  DreamShader-generated material graphs.
- Fixed a crash when regenerating an opened material function asset whose expressions were still
  rooted by the editor.

## 1.3.1 - 2026-05-09

### Function Calls

- Single-output `Function` and `GraphFunction` calls can be used as value expressions, for example
  `Color = Texture::Sample2DRGB(BaseTex, UV0);` — see [Calls](../graph/calls.md).
- Multi-output `Function` and `GraphFunction` calls still require explicit out variables, for
  example `Texture::Sample2D(BaseTex, UV0, Color, Alpha);`.

### Graph Functions

- Added top-level and namespaced `GraphFunction` blocks for reusable HLSL Custom-node logic.
- `GraphFunction` remains HLSL, but `UE.*` calls inside its body are converted into material nodes
  and passed into the Custom node as generated inputs.
- Added GraphFunction argument validation, recursive-call detection, and explicit out-variable
  writeback.

## 1.3.0 - 2026-05-08

### Shader Layer Functions

- Added top-level `ShaderLayer(Name="...", Root="...")` and `ShaderLayerBlend(Name="...", Root="...")`
  blocks.
- Generated layer assets use Unreal's native `UMaterialFunctionMaterialLayer` /
  `UMaterialFunctionMaterialLayerBlend` classes.
- `ShaderLayer` / `ShaderLayerBlend` reuse the existing `Properties`, `Inputs`, `Outputs`,
  `Settings` and `Graph` sections.
- Added validation that a Shader Layer block outputs exactly one `MaterialAttributes` value, and
  that a Shader Layer Blend block declares at least two `MaterialAttributes` inputs.
- Vector parameter properties keep their RGBA output available in `Graph`, so `.a` / `.w` reads
  alpha and assignments to lower component counts use the leading channels automatically.

**Deprecated since 1.3.0.** Use `ShaderLayer` instead.

`MaterialLayer` and `MaterialLayerBlend` remain compatibility aliases and emit warnings. New source
should use `ShaderLayer` and `ShaderLayerBlend`.

## 1.2.10 - 2026-05-08

### VSCode MaterialExpression Manifest

- Added editor-side export of reflected `UMaterialExpression` metadata to
  `Saved/DreamShader/Bridge/material-expressions.json`.
- The manifest is refreshed on editor bridge startup and when the DreamShader VSCode workspace is
  opened.
- Exported metadata covers expression class names, editable reflected properties, expression
  inputs, output pins, and inferred DreamShader `OutputType` hints.
- The release workflow downloads the latest `dreamshader-language-support` GitHub Release assets and
  attaches them to DreamShader releases.

## 1.2.8 - 2026-05-05

### Project Settings and Editor Entry Points

- Plugin version metadata updated to `1.2.8`.
- The `DreamShader.uplugin` documentation link now points at `https://lang.64hz.cn/`, and the
  support link at the GitHub home page.
- The Unreal Project Settings category moved from `Plugins` to **`DreamPlugin`**, with the section
  displayed as **Dream Shader** and described as `Dream Shader Settings` — see
  [Project Settings](../settings/project.md).
- Added the `OpenInNewWindow` setting under the `Editor` category. On by default, opening the
  DreamShader VSCode workspace from Unreal opens a new window; off, `--reuse-window` is appended and
  an existing VSCode window is reused.

## 1.2.7 - 2026-05-05

### Unreal 5.7 Compatibility

- Plugin version metadata updated to `1.2.7`.
- The generator picks up the header dependencies for `UMaterialExpressionFunctionInput` /
  `UMaterialExpressionFunctionOutput`, matching the newer Unreal material-function node build
  environment.
- The Moon Engine specific `MooaEncodedAttribute0` through `MooaEncodedAttribute4` outputs are now
  wrapped in `MOON_ENGINE` conditional compilation, so ordinary engine builds no longer depend on
  those custom material attributes.
- This release does not change `.dsm` / `.dsh` syntax; it is output targets and engine
  compatibility.

## 1.2.6 - 2026-04-30

### ShaderFunction Properties

- Added a `Properties` section to `ShaderFunction`, declaring property and helper nodes local to
  the generated material function.
- Added `const` property declarations for scalar, vector and texture helper nodes that are not
  externally adjustable parameters.
- `ShaderFunction` `Inputs` preview defaults can reference the same function's `Properties`, for
  example `opt Texture2D BaseColorTex = PreviewTex;`.

## 1.2.5 - 2026-04-30

### Material Attributes

- Added `MaterialAttributes` as a graph value type for `Shader`, `ShaderFunction` and
  `VirtualFunction` signatures.
- Added struct-like member writes such as `Attrs.BaseColor = Color;` and
  `Attrs.Roughness = Roughness;`.
- Added `Base.MaterialAttributes = Attrs;` output binding, which enables *Use Material Attributes*
  on the generated material automatically.
- `MaterialAttributes` values can be returned from generated or virtual material functions and
  passed through `Graph` assignments.

## 1.2.4 - 2026-04-30

### Parameter Reflection

- The documented comma-style metadata suffix was replaced with a semicolon-separated trailing
  reflection block on declarations.
- Parameter reflection blocks can set any reflected `UMaterialExpression` property exposed by the
  generated parameter node.
- The basic `float`, vector and texture shorthand declarations use the same reflection path as
  explicit parameter node declarations.
- Texture sample parameters can configure reflected properties such as `SamplerType`,
  `SamplerSource`, `MipValueMode`, `AutomaticViewMipBias`, `ConstCoordinate` and `ConstMipValue`.

## 1.2.3 - 2026-04-29

### Parameters

- Added declaration metadata `[Group="...", SortPriority=32, Description="..."]` for material
  `Properties` and for function input/output declarations.
- Added explicit parameter node declarations: `ScalarParameter`, `VectorParameter`,
  `TextureObjectParameter`, the texture sample parameter family, `StaticBoolParameter` and
  `StaticSwitchParameter` — see [Property Types](../language/properties.md).
- Added `StaticSwitchParameter` graph calls, for example
  `UseDetail(True=detailColor, False=baseColor)`.
- Added `UE.CollectionParam(Collection=Path(...), Parameter="...")` for Material Parameter
  Collection reads.

### Function Defaults

- Added `opt` inputs for `ShaderFunction` and `VirtualFunction`.
- Added the `default` call argument for optional material function inputs, preserving Unreal
  FunctionInput preview defaults.
- Generated `ShaderFunction` assets write input and output descriptions and sort priorities to the
  FunctionInput / FunctionOutput nodes.
- VirtualFunction copy, create and sync emit optional inputs, preview defaults and pin metadata
  when available.

## 1.2.2 - 2026-04-29

### VirtualFunction Workflow

- `CreateVirtualFunction` reuses the existing declaration for the selected material function
  instead of creating duplicate `.dsh` files.
- When a matching declaration already exists, the material function `DreamShader` menu shows
  `OpenVirtualFunction` and `Copy Virtual Function Reference` instead of the create and
  copy-definition actions.
- `OpenVirtualFunction` opens the existing declaration in VSCode and jumps to the declaration
  location where possible.
- Added startup validation and refresh for `VirtualFunction` declarations under `DShader`,
  reporting missing source `UMaterialFunction` assets and updating changed signatures.

### Import Compatibility

- `import "File.dsh"` works with or without a trailing semicolon in the Unreal generator import
  pass.

## 1.2.1 - 2026-04-29

### Editor Workflow

- Replaced the single material function toolbar action with a `DreamShader` dropdown menu.
- Added `CopyVirtualFunction`, `CreateVirtualFunction` and `CopyVirtualFunctionCall` to the material
  function editor toolbar and the material function asset context menu.
- `CreateVirtualFunction` writes a `.dsh` declaration under the configured
  `DShader/VirtualFunctions` directory and opens it in the default external editor.
- `CopyVirtualFunctionCall` copies a ready-to-paste `Graph` call using the generated input names and
  the first output.
- Added *Open Dream Shader Workspace (VSCode)* to the editor Tools menu and the DreamShader toolbar
  section. It writes `DShader/DreamShader.code-workspace`, opens it in VSCode when available, and
  falls back to the default editor or Notepad.

### Release

- Added a GitHub Actions release workflow that packages the plugin source and publishes a GitHub
  Release from version tags or manual workflow dispatch.

## 1.2.0 - 2026-04-28

### VirtualFunction

- Added `VirtualFunction(Name="...")` declarations for existing Unreal `UMaterialFunction` assets.
- `VirtualFunction` calls work from `Graph` like `ShaderFunction` calls, without generating or
  overwriting the referenced asset.
- `Options.Asset` supports `Path(Game, "...")`, `Path(Engine, "...")`,
  `Path(Plugin.PluginName, "...")` / `Path(Plugins.PluginName, "...")`, and full Unreal object
  paths.
- Added material function context-menu and Material Editor toolbar actions that copy a complete
  `VirtualFunction` declaration with inputs, outputs and options.

### Asset Roots

- `Root="Plugin.PluginName"` maps to the project plugin content root, physically saving generated
  assets under `[Project]/Plugins/PluginName/Content`.
- `Plugins.PluginName` and `Plugins/PluginName` remain compatibility spellings.

### Tooling

- The VSCode extension picked up `VirtualFunction` highlighting, completion, hover, snippets,
  signature help and diagnostics.
- Project content plugin names are completed inside `Path(Plugins.)` as well.

## 1.1.4 - 2026-04-28

### Asset Generation

- `Root="Plugins.PluginName"` / `Root="Plugins/PluginName"` are supported as compatibility
  spellings, resolving identically to `Root="Plugin.PluginName"`.
- This avoids `Plugins.MoonToon` being treated as an ordinary `/Game/Plugins_MoonToon`
  subdirectory — see [Asset Paths](../generation/asset-paths.md).

## 1.1.3 - 2026-04-28

### Asset Generation

- Clarified that `Root="Plugin.PluginName"` targets an **enabled project content plugin** only.
- `Plugin.MoonToon` resolves to the UE package root `/MoonToon`, physically saved under
  `[Project]/Plugins/MoonToon/Content`.
- The generator validates that the target plugin sits in the project `Plugins` directory, is
  enabled, can contain content, and has a `Content` directory.

## 1.1.2 - 2026-04-28

### Language and Generation

- Added the `Root` top-level attribute to `Shader` and `ShaderFunction`, selecting the generated
  asset's root path.
- `Root` defaults to `Game` and also accepts `Plugin.PluginName`, saving the generated `UMaterial`
  or `UMaterialFunction` into an enabled project content plugin root.
- When a `ShaderFunction` is called from `Graph`, the generated `UMaterialFunction` asset is
  resolved and loaded through that function's own `Root`.

### Documentation

- Updated the asset generation notes in the plugin README, the language reference and the site.

## 1.1.1 - 2026-04-26

### Fixed

- Fixed a `Function` call with several `out` parameters in `Graph`, where the Custom node's
  trailing output pins received no value.
- The first `out` of a multi-output `Function` still comes back through the Custom node's main
  `return`; the second and later `out` parameters now generate explicit assignments writing the
  `__ds_*_outN` temporaries back to the matching Additional Output, for example
  `Output_ToonA = __ds_xxx_out1;`.

### Compatibility

- A patch release. No `.dsm` / `.dsh` changes are required.

## 1.1.0 - 2026-04-22

### Language and Graph

- Renamed the `Code = { ... }` block in materials and material functions to `Graph = { ... }`,
  separating the graph DSL from the HLSL helper layer.
- Added basic `if` / `else` support to `Graph` — see [Control Flow](../graph/if.md).
- Improved output expression handling: an output binding records source text rather than only a
  variable name, allowing more flexible parsing of graph outputs.
- Improved type conversion and generic `UE.*` MaterialExpression argument handling, reducing
  friction when patching nodes in by hand.

### Function and Generation

- Added `Function SelfContained` and `Function Inline`, embedding helper dependencies in the Custom
  node so generated materials can be handed to projects without the DreamShader plugin installed.
- An ordinary `Function` still generates a `.ush` helper include, referenced from the Custom node.
- Added a cleanup workflow for generated shader files, reducing leftover historical includes.

### Packages and Workflow

- Added the Package system, distributing and installing reusable `.dsh` shared libraries through
  GitHub — see [Packages](../tools/packages.md).
- Added the import dependency graph: saving a shared `.dsh` finds the affected `.dsm` files and
  refreshes them incrementally. Later versions extend this to `.dsf` dependencies.
- Cached the config directory, cutting the cost of repeatedly parsing project settings.

### Diagnostics and Compatibility

- Diagnostics carry stage, platform and quality context, separating parse, generation and material
  compile problems — see [Diagnostics](../diagnostics/index.md).
- Removed the default mapping for the Toon shading model, so generated results do not
  unexpectedly depend on a project-custom shading model.
- Updated the README, language reference and example documentation to match.

## 1.0.0 - 2026-04-21

### Initial Version

- The `.dsm` / `.dsh` file model: Unreal materials and shared helpers described as text sources.
- `Shader(Name="...")` generates a `UMaterial`; `ShaderFunction(Name="...")` generates a
  `UMaterialFunction`.
- `Properties`, `Settings`, `Outputs` and the early graph-building syntax.
- HLSL-style `Function` helpers, generating Custom node code.
- Baseline documentation and examples, as the first public DreamShaderLang release.
