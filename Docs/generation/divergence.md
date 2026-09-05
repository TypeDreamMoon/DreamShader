# Divergence

> [DreamShader](../index.md) » [Generation](index.md) » **Divergence**

What happens when a generated asset has been edited by hand: how DreamShader notices, why it stops,
and the three ways to resolve it.

| | |
| :-- | :-- |
| Applies to | every generated `UMaterial`, `UDreamShaderMaterialInstance`, `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend` |
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
| **Diverged** | ours, and the contents no longer match | **refused** — see below |

`Unstamped` covers two cases, both benign: an asset generated before digests existed, and one whose
stamp carries a different schema tag. The tag is the digest format version plus the engine version
(`DSD1-5.8`), because the property set the digest walks is the engine's. Without it, upgrading the
engine would re-fingerprint every asset in the project at once and report the whole library as
hand-edited.

## What counts as a hand edit

The rule is **the digest covers exactly what a rebuild would destroy**. A property a rebuild never
touches survives it untouched, so blocking on that property would refuse a rebuild in order to
protect something that was never in danger.

| Change | Divergence |
| :-- | :-- |
| a node added, deleted, or rewired | **yes** |
| a property changed on a generated node (including a parameter's default value) | **yes** |
| a material property input (an `Outputs` binding) rewired | **yes** |
| any property in [Reset properties](regeneration.md#reset-properties) | **yes** |
| a material function's `Description`, `UserExposedCaption`, `ExposeToLibrary`, `LibraryCategories`, usage | **yes** |
| a parameter override on a generated ThinCustom instance | **yes** |
| the instance's parent, or the hidden base material's graph | **yes** |
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

## What a refusal looks like

Generation fails with the asset exactly as you left it — not cleared, not half-built:

```text
Asset '/Game/Materials/M_Emissive.M_Emissive' has been edited by hand since DreamShader generated it
from 'DShader/Materials/M_Emissive.dsm', so it was NOT rebuilt (rebuilding would destroy those
edits). Right-click the asset > DreamShader and choose one: 'Revert to Source' discards the edits and
rebuilds, 'Adopt Into Source' rewrites 'DShader/Materials/M_Emissive.dsm' from the edited asset,
'Detach From DreamShader' hands the asset over to you and stops managing it.
```

The check runs **before** anything mutates the asset. For a material function that ordering is the
whole point: the gate fires before the usage is restamped, not merely before the graph teardown.

This gate and [atomic rebuild](regeneration.md#sequence) answer two different questions, and you want
both. The gate stops a rebuild that *would have succeeded* from overwriting work you did by hand;
atomicity stops a rebuild that *fails* from leaving the asset in pieces. Neither covers the other's
case.

> [!IMPORTANT]
> `-Force` does **not** get past this. `bForce` answers "is the source hash stale", and the editor
> asserts it for every file in its own startup sweep — honouring it here would have left the gate
> dead in the mode the editor spends all its time in. Only the **Revert** action overrides a
> divergence, because only a person can make that call.

A rebuild is only ever attempted when the source moved. An unchanged source is skipped by the source
hash long before the gate, so hand-editing an asset and leaving its source alone reports nothing at
all — nothing is in danger.

## The three ways out

All three live on the asset's right-click menu, under **DreamShader**, for materials, material
functions, layers, layer blends, and generated ThinCustom instances alike.

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

## Notes

- The digest is stamped for memory-only assets too, along with the source path and hash. Without
  the path the asset reads as `Foreign` and the gate never fires — which would have left it dead in
  the editor's default in-memory mode.
- A digest is only ever compared against one carrying the same schema tag. Changing the tag is the
  supported way to retire a digest format without flagging the world.
- The gate protects the asset, not the source. Nothing here writes to a `.dsm` except the Adopt
  action, which asks first and backs up.

## See also

- [Regeneration](regeneration.md) — what a rebuild destroys, and the ownership guard
- [Caching](caching.md) — the source hash, and why it is not this
- [In-memory materials](in-memory.md) — the mode most of these assets live in
- [Decompiler](../tools/decompiler.md) — what the Adopt action writes
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
