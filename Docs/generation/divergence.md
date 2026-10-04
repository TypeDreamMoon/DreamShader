# Divergence

> [DreamShader](../index.md) » [Generation](index.md) » **Divergence**

What happens when a generated asset has been edited by hand: how DreamShader notices, why it stops,
and the three ways to resolve it.

| | |
| :-- | :-- |
| Applies to | every generated `UMaterial`, `UDreamShaderMaterialInstance`, `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend`; a `UDreamPassPipeline` *(since 2.1.0)* — see [A Custom Pass pipeline](#a-custom-pass-pipeline) |
| Stored in | the generated asset's package metadata, key `DreamShader.OutputDigest` |
| Checked | immediately before a rebuild clears the asset, and never after |
| Since | `1.8.0` |

## The problem it solves

The source hash ([Caching](caching.md)) fingerprints the source a compile reads. It says nothing
about the asset that compile produced. So an asset you edited by hand still looked untouched: the
next time its `.dsm` moved, [regeneration](regeneration.md) tore the graph down and rebuilt it, and
the edit was gone with no diagnostic and no undo.

The **output digest** is the other half: a fingerprint of what the asset actually holds, written at
the end of every successful generation. Comparing it against the asset before a rebuild answers the
question the source hash cannot — *has somebody been working in here?*

## States

`DreamShader.SourceFile` decides ownership; the digest decides whether the contents are still ours.

| State | Meaning | Effect on a rebuild |
| :-- | :-- | :-- |
| **Foreign** | no `DreamShader.SourceFile` — DreamShader never generated this | the [ownership guard](regeneration.md#ownership-guard) refuses; nothing is touched |
| **Unstamped** | ours, but carrying no comparable digest | rebuilt normally, and restamped |
| **Generated** | ours, and the contents match the stamp | rebuilt normally |
| **Tweaked** *(since 1.9.0)* | ours, the contents match, and the ThinCustom instance carries parameter overrides | rebuilt normally, and the overrides are [put back](#parameter-overrides-on-a-generated-thincustom-instance) |
| **Diverged** | ours, and the contents no longer match | **refused** — see below |

`Unstamped` covers two cases, both benign: an asset generated before digests existed, and one whose
stamp carries a different schema tag. The tag is the digest format version, the engine version, and a
fingerprint of the reflected layout of every expression class the asset uses
(`DSD4-5.8-1a2b3c4d`), because the property set the digest walks is the engine's. Without the
version, upgrading the engine would re-fingerprint every asset in the project at once and report the
whole library as hand-edited; without the layout fingerprint, a source-built engine that adds a pin
to one expression class between two sessions did the same to every asset using that class *(since
1.9.0)*.

The classes the fingerprint covers are the ones the generator placed, and they are recorded next to
the stamp in a second metadata key, `DreamShader.OutputDigestClasses`. The check recomputes the tag
over *that* list rather than over the graph as it stands, so a node you add by hand — usually a class
the generated graph never had — is compared and reads `Diverged`, instead of moving the tag and
slipping through as a schema change. A stamp written without the list falls back to the current
graph's classes and is rewritten with the list on its next rebuild.

## What counts as a hand edit

The rule is **the digest covers exactly what a rebuild would destroy**. A property a rebuild never
touches survives it untouched, so blocking on that property would refuse a rebuild in order to
protect something that was never in danger.

| Change | Divergence |
| :-- | :-- |
| a node added, deleted, or rewired | **yes** |
| a property changed on a generated node (including a parameter's default value) | **yes** |
| which attributes a `GetMaterialAttributes` publishes, or a `SetMaterialAttributes` sets | **yes** *(Get: since 2.0.0)* |
| a material property input (an `Outputs` binding) rewired | **yes** |
| any property in [Reset properties](regeneration.md#reset-properties) | **yes** |
| a material function's `Description`, `UserExposedCaption`, `ExposeToLibrary`, `LibraryCategories`, usage | **yes** |
| the instance's parent, or the hidden base material's graph | **yes** |
| a parameter override on a generated ThinCustom instance | no — `Tweaked` instead, and the rebuild [restores it](#parameter-overrides-on-a-generated-thincustom-instance) *(since 1.9.0)* |
| a node dragged to a new position | no |
| a comment box added by hand (any text — see [regeneration](regeneration.md#what-survives)) | no |
| a node's comment bubble, collapsed state, preview visibility, `Desc` | no |
| a named reroute's display colour | no |
| a material property outside the reset list — preview mesh, thumbnail, physical material | no |
| pin `Id` GUIDs and named-reroute variable GUIDs | no — these are identity, deliberately carried across a rebuild |

> [!NOTE]
> A named reroute seeds its display colour from its own object path name, which changes every time
> the node is recreated. It is excluded for that reason as much as for being cosmetic: leaving it in
> made two rebuilds of one unchanged source disagree with each other.

> [!NOTE]
> The digest does not depend on the language the editor runs in *(since 2.0.0)*. It names a node's
> inputs, and three engine nodes name theirs with translated text -- `BreakMaterialAttributes`, and
> `GetMaterialAttributes` / `SetMaterialAttributes`, whose pins show the attributes' display names --
> so through 1.9 a layer stamped by an English editor read as `Diverged` in a Chinese one. Those
> inputs are named by the attribute itself now. The format tag moved from `DSD3` to `DSD4` with
> that, so every asset reads as `Unstamped` once after the upgrade.

## Parameter overrides on a generated ThinCustom instance

*(since 1.9.0)*

Under the [ThinCustom backend](../language/shader.md#generated-asset) the graph lives on a hidden base
`UMaterial` and the generated asset is a thin `UDreamShaderMaterialInstance` of it — the surface you
tune. Until `1.9.0` an override on that instance was digested like any other content, so **dragging
one slider refused every future rebuild of the `.dsm`**: the plugin's most ordinary usage judged a
violation, and the only way out was Revert, which discarded the very values you had just set.

It is now its own state. `Tweaked` says "generated, and tuned"; the gate answers only to `Diverged`,
so it does not stop anything.

| Step | What happens |
| :-- | :-- |
| Before the rebuild | every override the instance itself carries is read off it — scalar, vector, texture, font, static switch, static component mask, and every other kind this engine version has |
| During | the base's graph is torn down and rebuilt, and `ClearParameterValuesEditorOnly` empties the instance, exactly as before |
| After | each captured value is written back under the **same name and kind**, then the static permutation is updated so the shader map matches the restored switches |
| A name the rebuild no longer declares | dropped, and named in one `DSH8155` log line listing every dropped parameter |

Restoration is by name, because a name is the only thing that survives a graph the generator tore
down and rebuilt from scratch — pin ids and node identities do not. So renaming a parameter in the
source loses its override, and that is the source's decision rather than something the plugin can
second-guess.

> [!NOTE]
> Only the **success** path restores. A rebuild that fails rolls the base's graph back and never
> reaches the clear, so the instance comes out of a failed compile with its overrides untouched.

> [!WARNING]
> [Adopt Into Source](#adopt-into-source) still refuses an instance carrying overrides, and that has
> not changed: the [decompiler](../tools/decompiler.md) writes the base's graph, which has nowhere to
> put an instance-level override, so adopting would write a source describing everything except the
> edit you actually made. Move the values into the source as `Properties` defaults — or override them
> on a child material instance of your own — and then Adopt.

## What a refusal looks like

Generation fails with the asset exactly as you left it — not cleared, not half-built. The compile
reports [`DSH8207`](../diagnostics/DSH8xxx.md#dsh8207), whose message carries the asset layer's
[`DSH8115`](../diagnostics/DSH8xxx.md#dsh8115) text:

```text
Asset '/Game/Materials/M_Emissive.M_Emissive' was edited by hand since DreamShader generated it from
'DShader/Materials/M_Emissive.dsm', so it was NOT rebuilt (rebuilding would destroy those edits).
Use the notification, or right-click the asset > DreamShader, and choose one: Revert to Source,
Adopt Into Source, or Detach From DreamShader.
```

> [!NOTE]
> The message was four sentences before `1.9.0` and explained each of the three answers in the log.
> It is two now, and the explanations moved to this page and into the tooltips on the notification
> described below — the old text was long enough that the part saying what to do came after the part
> people stopped reading.

The check runs **before** anything mutates the asset. For a material function that ordering is the
whole point: the gate fires before the usage is restamped, not merely before the graph teardown.

This gate and [atomic rebuild](regeneration.md#sequence) answer two different questions, and you want
both. The gate stops a rebuild that *would have succeeded* from overwriting work you did by hand;
atomicity stops a rebuild that *fails* from leaving the asset in pieces. Neither covers the other's
case.

> [!IMPORTANT]
> `-Force` does **not** get past this. `bForce` only means "ignore the source hash", and *Recompile
> DSM*, the Material Content Browser's Compile, *Materialize* and the cook all set it — honouring it
> here would leave the gate dead exactly when a whole project is rebuilt. Only the **Revert** action
> overrides a divergence, because only a person can make that call.

An unforced compile of an unchanged source is skipped by the source hash before the gate is asked, so
hand-editing an asset and leaving its source alone reports nothing until the source changes or a
forced compile reaches the asset — until then nothing is in danger.

## What you see *(since 1.9.0)*

A refusal also raises a notification in the bottom-right corner of the editor, carrying the answers
themselves. It names the asset and the source, and it does not time out — this is a question, and a
question that fades away is one you never answered.

| Button | Does | Same as |
| :-- | :-- | :-- |
| **Revert to Source** | discards the edits and rebuilds | [Revert to Source](#revert-to-source) |
| **Adopt Into Source** | rewrites the source from the asset | [Adopt Into Source](#adopt-into-source) |
| **Detach** | stops managing the asset | [Detach From DreamShader](#detach-from-dreamshader) |
| **Show Ephemeral Materials** | reveals the asset in the Content Browser — only when it is a hidden [Ephemeral](ephemeral.md) instance | [the Tools menu toggle](../tools/editor-integration.md#show-ephemeral-materials) |
| **Dismiss** | closes the notification and changes nothing | — |

The first three are the same code the right-click menu runs, so each still confirms with a dialog,
still closes and reopens any open asset editor, and still recompiles afterwards. All of them except
*Show Ephemeral Materials* close the notification: that one exists so you can go and **look** at an
asset you could not see, and taking the three answers away at that moment would leave you with a
visible tile and nothing to do with it.

The fourth button is the reason the notification exists at all. In the editor's default
[the Ephemeral state](ephemeral.md) a generated ThinCustom instance has no tile in the Content Browser
unless *Show Ephemeral Materials* is on — so "right-click the asset" named something that was not on
screen, and the three answers were unreachable for exactly the assets most likely to diverge.

### How often it appears

| Rule | Effect |
| :-- | :-- |
| One notification per asset per **round** | a source that reports the same asset twice, or two sources that reach it, still produce one |
| No second notification while the first is still on screen | saving a broken source repeatedly does not stack toasts |
| More than **5** assets in one round | the rest collapse into a single summary carrying **Open Material Browser** |
| A new round | an asset that is still diverged is reported again |

A *round* is one batch of work as you asked for it: one save (with everything its debounce window
swept up), or one whole-project scan behind *Recompile DSM* / *Clean Generated Shaders*, or one
compile asked for on its own from the [Material Content Browser](../tools/material-browser.md) or the
bridge. The compiles inside a batch belong to the batch's round rather than each starting one.

So a project with fifty hand-edited assets produces at most six notifications for a full recompile,
not fifty. The summary is raised at the **end** of the round, because only then is the total known.

> [!NOTE]
> No notification is ever raised headlessly. The commandlet, a cook, and any run with `-unattended`
> get the log line and the [diagnostics](../diagnostics/index.md) exactly as before; there is nothing
> to click and nobody to click it. Closing the editor retires any notification still waiting.

The log line, the VSCode problems panel and the Material Content Browser's per-source status are
unchanged and remain the durable record — the notification is the part that is in front of you at
the moment it happens, not the part you go looking for afterwards.

## The three ways out

All three live on the asset's right-click menu, under **DreamShader**, for materials, material
functions, layers, layer blends, and generated ThinCustom instances alike — and, since `1.9.0`, on
the [notification](#what-you-see-since-190) a refusal raises.

### Revert to Source

Discards the hand edits and rebuilds from the source. The source file is not modified. This is the
"the source was right after all" answer.

Reverting a **saved** asset rebuilds and saves it; reverting a memory-only one rebuilds it in memory.
Doing it the other way round would leave the edits on disk and report success, and the next session
would read the same divergence straight back off the package.

### Adopt Into Source

Rewrites the `.dsm` / `.dsf` from the asset's current contents, so the hand edits become the source
of truth, then recompiles so the two agree again. This is the "the asset was right" answer.

| | |
| :-- | :-- |
| Backup | the previous source is copied to `<source>.bak` before anything is written |
| Refused when | the source file declares more than one asset |
| Refused when | the source contains [preprocessor directives](../language/preprocessor.md) — `DSH8149` *(since 1.9.0)* |
| Replaces | the file's own form — hand-written comments, `import` directives and formatting become the decompiler's output |

Both refusals exist for the same reason: adopting rewrites the whole file from one asset, and
anything the file said that the asset does not hold is gone.

The multi-asset refusal matters because the [decompiler](../tools/decompiler.md) emits one block, not
a translation unit, so adopting one asset out of a file that declares several would silently delete
the others.

The directive refusal matters because a generated asset holds only the **post-cut** result — the
branch that was taken, with no record that a branch ever existed — so rewriting the source from it
would erase every `#if` in the file and freeze whichever configuration happened to build last. The
gate fires on *any* directive, taken or not, and a file whose only directive is a `#define` is
refused too; `#Region` is not a preprocessor directive and does not trip it.

Use **Export DSM** and merge by hand into the right branch in either case.

> [!NOTE]
> The [VirtualFunction startup sync](../tools/virtual-function-tools.md#startup-sync-service) refuses
> conditional sources as well, with `DSH9001`. It is not one of the three actions on this page — it
> runs unattended at editor start rather than from the menu — but it is the other writer of source
> files, and it splices by byte offset, which line-count conservation does not preserve.

### Detach From DreamShader

Drops every `DreamShader.*` stamp. The asset keeps its contents and becomes an ordinary hand-authored
asset that DreamShader will never rebuild. This is the "stop managing this" answer.

Afterwards the asset is `Foreign`, so compiling the source that used to own it fails with the
ownership guard until you rename or move one of the two. Save the asset to keep the change — the
detach only edits it in memory.

## A Custom Pass pipeline

*(since 2.1.0)* A `UDreamPassPipeline` has no graph, and is meant to be tuned by hand: its
[details panel](../tools/editor-integration.md#pass-pipeline-details-panel) edits it, and the next frame
runs the edit. It is guarded like every other generated asset, and Adopt is how the tuning reaches its
`.dsp`.

| | |
| :-- | :-- |
| Digest | every property of the asset but its two source stamps (`SourceFilePath`, `SourceHash`), each whole, as exported text: the order, the default injection point, the views, the requirements, the enabled parameter, the parameters, the buffers, the passes with their slots, and the export render targets. The schema tag fingerprints the pipeline class and every struct its content reaches, so a plugin update that adds a field reads as `Unstamped` once rather than as a hand edit |
| A hand edit | any change in the details panel — a value, a pass added, moved or removed — reads `Diverged`. The panel's **Source** row shows the state as it changes |
| The gate | the next compile of the changed `.dsp` is refused, with the [notification](#what-you-see-since-190) and its answers. Nothing is written: not the asset, not the slot registry, not a snapshot, not a render target |
| The answers | **Revert to Source**, **Adopt Into Source** and **Detach** on the notification, and in the [Material Content Browser](../tools/material-browser.md#pipelines) — its *Provenance* section and its context menu; the first two are also buttons on the details panel. A pipeline has no Content Browser context-menu entry |

### Adopting into a `.dsp`

A `.dsp` is never reprinted: Adopt splices the asset's values into the file, as it does for a `.dsi`.

1. The file is read and checked as a compile would, stopping before anything is built.
2. The asset is [decompiled](../tools/decompiler.md#pipelines-dsp) into the payload a `.dsp` binds to.
3. Every value the asset still holds unchanged takes the file's own spelling back: a material or a shader
   that resolves to the same asset or file keeps the reference as written, a number equal at the precision
   the asset keeps (a `float`) keeps the literal or the expression it was folded from — a `static const`
   included — a flag set with the same bits keeps its written form, and a key written although it states
   the default stays written. Parameters, buffers and passes are matched by name, bindings by position.
4. Only the declarations and keys whose values differ are rewritten, so `//` comments and the order of the
   file survive. When anything changes, the file is copied to `<file>.bak` first.
5. The pipeline is rebuilt from the file, past the divergence gate, and saved.

When the file already states every value, nothing is written and the pipeline is only rebuilt.

| Refused when | Code |
| :-- | :-- |
| the file uses [preprocessor directives](../language/preprocessor.md) — a splice addresses the file's bytes, and the parser read the preprocessed text | `DSH8149` |
| the source the asset is stamped with is not a `.dsp` | `DSH9227` |
| the asset built from a `.dsp` is not a pass pipeline | `DSH9226` |
| the file does not check — a material it names no longer resolves, a shader file is gone — so there is nothing to check a rewrite against | the file's own diagnostics |
| the file builds no pipeline any more | `DSH9228` |
| the asset holds what a `.dsp` cannot state: a fullscreen pass with neither or both of a material and a shader, layer bits the layer table has no name for, no view at all, a mesh pass that checks no usage flag, a texture constant in a `param`, an own depth without its buffer | `DSH9211`, `DSH9212`, `DSH9215`, `DSH9216`, `DSH9217`, `DSH9219`, `DSH9220` |
| the source ships with a plugin | — the action is disabled |

Like every Adopt, the editor's asks first, and closes and then reopens an asset editor open on the
pipeline.

## Notes

- The digest is stamped for memory-only assets too, along with the source path and hash. Without
  the path the asset reads as `Foreign` and the gate never fires — which would have left it dead in
  the editor's default the Ephemeral state.
- A digest is only ever compared against one carrying the same schema tag. Changing the tag is the
  supported way to retire a digest format without flagging the world. `1.9.0` changed it — taking the
  instance's overrides out of the digest changed what the digest is made of — so every asset stamped
  by an earlier version reads as `Unstamped` once: rebuilt normally, and restamped. Nothing is
  refused by it, and a hand edit made *before* that first rebuild is not protected by the gate.
- The gate protects the asset, not the source. Nothing here writes to a `.dsm` except the Adopt
  action, which asks first and backs up.

## See also

- [Regeneration](regeneration.md) — what a rebuild destroys, and the ownership guard
- [Caching](caching.md) — the source hash, and why it is not this
- [Ephemeral materials](ephemeral.md) — the mode most of these assets live in
- [Decompiler](../tools/decompiler.md) — what the Adopt action writes
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
