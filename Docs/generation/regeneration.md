# Regeneration

> [DreamShader](../index.md) » [Generation](index.md) » **Regeneration**

What happens to an already-generated asset when its source file is compiled again.

| | |
| :-- | :-- |
| Applies to | every generated `UMaterial`, `UDreamShaderMaterialInstance`, `UMaterialFunction`, `UMaterialFunctionMaterialLayer`, `UMaterialFunctionMaterialLayerBlend` |
| Triggered by | any compile that is not skipped by the source hash — see [Caching](caching.md) |
| Effect | the generated graph is torn down and rebuilt from source |

## Summary

**A generated asset is source-derived output, not a document.** Regeneration rebuilds it from the
`.dsm` or `.dsf`, and everything the table below marks as not surviving is destroyed by that rebuild.

> [!IMPORTANT]
> A rebuild that *would* destroy a hand edit does not happen. Since `1.8.0` an asset is fingerprinted
> at the end of every successful generation, and a rebuild that finds the fingerprint no longer
> matching stops and asks you to choose — see [Divergence](divergence.md). The table below therefore
> describes what a rebuild does to an asset that is **still untouched**, and what the **Revert to
> Source** action does to one that is not.

| Edit | Survives regeneration |
| :-- | :-- |
| a comment box whose text does **not** begin with `DreamShader: ` | **yes** |
| a comment box whose text begins with `DreamShader: ` | no — deleted |
| nodes you added by hand | no — deleted |
| node property tweaks on generated nodes | no — the node is deleted and recreated |
| node positions | no, unless pinned by a [`Layout`](../language/layout.md) section |
| material settings changed in the editor | no — every property in [Reset properties](#reset-properties) is restored to its default, then `Settings` is reapplied |
| parameter overrides on a generated ThinCustom instance | **no** — see the warning below |
| `FunctionInput` / `FunctionOutput` pin identities on a material function | **yes** *(since 1.3.2)* |
| named-reroute variable GUIDs | yes — regenerated only when invalid |

## Sequence

0. **Refuse if the asset is open in an asset editor**, then **refuse if it has diverged** — both
   before anything below runs, and before any of it is reversible. See
   [Open in an asset editor](#open-in-an-asset-editor) and [Divergence](divergence.md).
1. `Modify()` the target object.
2. **Clear the generated comments** — every `UMaterialExpressionComment` whose text starts with the
   literal `DreamShader: `.
3. **Null every material property input**, from the first to the last material-property slot.
4. **Delete every expression** in the graph.
5. **Reset the material to defaults** — see [Reset properties](#reset-properties). Material functions
   skip this step.
6. Apply `Settings`.
7. Rebuild: `Properties` nodes, the `Graph` body or the whole-surface `Custom` node, the `Outputs`
   bindings.
8. Lay out — [skipped in memory-only mode](graph-layout.md#when-layout-runs).
9. Recompile.

Step 4 has two strategies. Below 1200 expressions each node is deleted individually through the
material editing library, in up to 64 outer passes, reporting
`Deleting old Material node '{Name}'...`. At 1200 or more the whole expression collection is
un-rooted and marked as garbage in one pass. The material path also resets the material's editor
parameter cache; the material-function path does not.

**A rebuild is atomic.** Steps 1-9 either all take effect or none of them do: the old graph is
detached rather than destroyed, and a failure at any point puts it back — nodes, connections, render
state, material-function usage and pin GUIDs alike — before the compile returns. The asset a failed
compile leaves behind is the asset it started with.

That matters because not every failure is caught up front. The whole-file parse, the `Settings`
validation and the `Outputs` validation are gates that run **before** the asset is touched at all,
but a `Graph` block is compiled one statement at a time by the graph builder, which runs at step 7 —
after the teardown. Until `1.8.0` such a failure left the asset **emptied**, which was bad for a
material and much worse for a material function, whose call sites read their pins from the live
asset: one bad `.dsf` took every material that called it down with it, with no undo (generated assets
are deliberately not `RF_Transactional`).

> [!NOTE]
> The one thing a rollback does not restore is the `DreamShader: ` comment boxes, which are deleted
> at step 2 — before the snapshot, necessarily, since a snapshot taken first would capture references
> to comments that are already gone. They are regenerated decoration, and the next successful compile
> recreates them. Comments you wrote are never prefixed and are never deleted in the first place.

## What survives

Exactly one hand edit survives: a comment box whose text does not carry the DreamShader prefix.

| | |
| :-- | :-- |
| Prefix | `DreamShader: ` — the word, a colon, and a single trailing space |
| Comparison | **case-sensitive** |
| Effect | a comment whose text starts with the prefix is deleted before the rebuild; every other comment is left untouched |

`dreamshader: Notes`, `DREAMSHADER: Notes` and `DreamShader:Notes` (no space) all fail the prefix
test and therefore **survive**. This is the supported way to annotate a generated material by hand.

> [!NOTE]
> The corollary: renaming a generated box from `DreamShader: Sampling` to `Sampling` makes it
> permanent, and the next regeneration creates a *second* box named `DreamShader: Sampling` on top of
> it. To keep DreamShader's own boxes in sync, leave their text alone and change the `Comment(Name=…)`
> entry in the source [`Layout`](../language/layout.md) section instead.

Two identities are deliberately preserved so that existing call sites do not break:

- **Material function pins.** Before the graph is cleared, the `Id` GUID of every
  `UMaterialExpressionFunctionInput` and `UMaterialExpressionFunctionOutput` is cached by name and
  restored onto the newly created pin with the same name. A `MaterialFunctionCall` node elsewhere in
  the project keeps its wiring across a regeneration of the function *(since 1.3.2)*.
- **Named reroutes.** A declaration's variable GUID is regenerated only when the existing one is
  invalid.

Renaming an input or output in the source is therefore a breaking change for its call sites: the old
name's GUID has nothing to restore onto.

## Parameter overrides on a generated instance

> [!WARNING]
> Under the **ThinCustom** backend, regeneration calls `ClearParameterValuesEditorOnly()` on the
> emitted `UDreamShaderMaterialInstance`. **Every parameter override set by hand on a generated
> instance is wiped by a regeneration** — scalar, vector, texture, static switch, and static
> component-mask alike.
>
> Since `1.8.0` this no longer happens silently: an override is a hand edit like any other, so the
> instance reads as [diverged](divergence.md) and the rebuild is refused until you pick Revert, Adopt
> or Detach. The advice below is still the better habit, because it never reaches that point.
>
> **Workaround:** never tune a generated instance directly. Either
>
> - move the value into the source as a `Properties` default, so the generated instance carries it,
>   or
> - create a **child** `UMaterialInstanceConstant` parented to the generated instance and override
>   there. The child is a normal asset that regeneration never touches, and because the generated
>   instance owns the static permutation, the child shares its shader map at no extra compile cost.
>
> The Material Content Browser's instance-creation action produces exactly such a child, in
> `<parent directory>/<Instance Subfolder>` — see
> [In-memory materials](in-memory.md#materializing-to-disk).

## Open in an asset editor

A rebuild is refused outright while the asset is open in an asset editor:

```text
Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. An open editor works on its
own copy of the asset and writes that copy back when you press Apply or Save, which would silently
undo this rebuild. Close the editor and compile again.
```

| | |
| :-- | :-- |
| Fires when | `UAssetEditorSubsystem::FindEditorForAsset` reports an editor open on the target |
| Applies to | materials, material functions, layers, layer blends and generated ThinCustom instances |
| Since | `1.8.0` |

The reason is that **an asset editor does not edit the asset.** `FMaterialEditor` duplicates it into
a transient `UPreviewMaterial` and copies that duplicate back over the original on Apply or Save
(`UpdateOriginalMaterial`); the material instance editor writes back through a
`UMaterialEditorInstanceConstant` wrapper. An editor that stayed open across a rebuild is therefore
holding a *pre-rebuild* copy of the asset, and the next Apply reverts everything the rebuild did —
which then shows up as a [divergence](divergence.md) on the compile after that, a long way from the
cause.

> [!NOTE]
> Refusing is the only safe answer available here. Whether that copy has unapplied edits in it is
> `FMaterialEditor::bMaterialDirty`, which is private to the MaterialEditor module — so "close it if
> it is clean, refuse if it is not" is not a question this plugin can ask. Closing it blindly is
> worse: the engine's save prompt would appear in the middle of a compile-on-save, and would hang a
> headless build.

**The provenance actions are the exception.** *Revert to Source* and *Adopt Into Source* close the
editor themselves, do the work, and reopen it — because you just clicked them, quite possibly from
that editor's own toolbar, and a menu item that is permanently dead where it is most useful is not a
guard, it is a bug. The engine's save prompt may appear as part of that close; for Adopt it is
load-bearing rather than noise, since unapplied editor changes are not part of "this asset's current
contents" until the prompt is answered. Cancelling it cancels the whole action.

## Ownership guard

DreamShader refuses to overwrite an asset it did not generate.

| | |
| :-- | :-- |
| Fires when | the target package exists **on disk** and the existing object carries **no** `DreamShader.SourceFile` metadata |
| Applies to | every backend and every block kind *(the ThinCustom instance path since `1.8.0`)* |
| Result | generation fails; the existing asset is untouched |

| Message | Raised for |
| :-- | :-- |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | a material |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | a material function |

### Where the guard does not apply

The guard is inert for packages that are not on disk: a memory-only asset has no saved package to
protect, so the check does not run. That gap is covered from the other side —
[divergence](divergence.md) applies in memory too, because the source path is stamped there as well.

A wrong-class asset at the target path is refused by a class check instead, before the ownership
question is asked at all:

```text
Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or remove
Backend="Instance") before switching backends.
```

> [!NOTE]
> Before `1.8.0` the ThinCustom instance path checked only that class, not provenance — so generating
> onto a path holding a hand-authored `UDreamShaderMaterialInstance` adopted and rebuilt it, clearing
> its parameter overrides. Since the default backend is ThinCustom, that was the widest of the three
> paths and the only one without the check. It now runs the same guard as the others.

## Reset properties

Before the graph is rebuilt, a material's render state is restored to these values, in this order.
`Settings` is applied afterwards, so any key you declare wins; anything you do **not** declare
returns to the value below regardless of what the material editor last held.

| Property | Reset to |
| :-- | :-- |
| `BlendMode` | `BLEND_Opaque` |
| `MaterialDomain` | `MD_Surface` |
| shading model | `MSM_DefaultLit` |
| `TwoSided` | `false` |
| `OpacityMaskClipValue` | `0.3333` |
| `Wireframe` | `false` |
| `DitheredLODTransition` | `false` |
| `DitherOpacityMask` | `false` |
| `bAllowNegativeEmissiveColor` | `false` |
| `bCastDynamicShadowAsMasked` | `false` |
| `bCastRayTracedShadows` | `true` |
| `bEnableResponsiveAA` | `false` |
| `bScreenSpaceReflections` | `false` |
| `bContactShadows` | `false` |
| `bDisableDepthTest` | `false` |
| `bOutputTranslucentVelocity` | `false` |
| `bWriteOnlyAlpha` | `false` |
| `BlendableOutputAlpha` | `false` |
| `TranslucencyLightingMode` | `TLM_VolumetricNonDirectional` |
| `bTangentSpaceNormal` | `true` |
| `bAlwaysEvaluateWorldPositionOffset` | `false` |
| `bFullyRough` | `false` |
| `bIsSky` | `false` |
| `bIsThinSurface` | `false` |
| `MaterialDecalResponse` | `MDR_ColorNormalRoughness` |
| `bHasPixelAnimation` *(UE 5.4+)* | `false` |
| `NumCustomizedUVs` | `0` |

Material functions have no render state; their asset-level fields are reapplied instead:

| Source setting | Field | When absent |
| :-- | :-- | :-- |
| `Description` | `Description` | cleared |
| `UserExposedCaption` | `UserExposedCaption` | cleared |
| `ExposeToLibrary` | `bExposeToLibrary` | set to `false` |
| `LibraryCategories` | `LibraryCategoriesText` — comma-separated, entries trimmed, empties dropped | cleared |

The material-function usage is also re-stamped from the block kind on every regeneration.

## Notes

- **Regeneration is not undoable.** Generated material instances are deliberately not
  `RF_Transactional`, because undo/redo desynchronizes the shader map. This is why the
  [divergence](divergence.md) gate is a *pre*-check: once the graph is cleared there is nothing to
  come back to.
- The safest mental model: treat the `.dsm` / `.dsf` as the asset. Anything you want to persist
  belongs in the source — or, if you edited the asset instead, use **Adopt Into Source** to make the
  source say what the asset says.
- A regeneration that is skipped by the source hash does none of this — the asset is not touched at
  all. See [Caching](caching.md#when-regeneration-is-skipped).
- Deleting the generated asset and recompiling is always equivalent to a forced regeneration, except
  that the pin GUIDs of a material function are lost and its call sites break.
- The [decompiler](../tools/decompiler.md) is the way to capture hand edits: export the edited
  material back to `.dsm` / `.dsf`, then make that the source of truth.

## Diagnostics

Runtime substitutions are rendered as `{Placeholder}`.

| Message | Cause |
| :-- | :-- |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | ownership guard, material |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | ownership guard, material function |
| `Asset '{ObjectPath}' already exists and is not a Material.` | `Graph` backend, non-`UMaterial` at the path |
| `Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or remove Backend="Instance") before switching backends.` | ThinCustom backend, wrong class at the path |
| `Asset '{ObjectPath}' already exists and is not a MaterialFunction asset.` | function kind, wrong class at the path |
| `Asset '{ObjectPath}' already exists as '{ActualClass}', but {Kind} generation requires '{ExpectedClass}'. Delete or move the existing asset and regenerate it.` | function kind, wrong material-function subclass |
| `Generated DreamShader asset '{Path}' could not be saved.` | the package save failed after a successful rebuild |
| `Generated DreamShader asset packages could not be saved.` | the paired instance + base save failed |
| `'{ObjectPath}' exists as a saved asset, so it is rebuilt and saved on disk rather than in memory. Run Tools > DreamShader > Clean Persisted Generated Assets to make it memory-only.` | log; a memory-only compile landed on an asset with a file behind it, so it took the persisted path — see [In-memory materials](in-memory.md#when-the-asset-already-exists-on-disk) |
| `Asset '{ObjectPath}' has been edited by hand since DreamShader generated it from '{SourceFile}', so it was NOT rebuilt (rebuilding would destroy those edits). ...` | the [divergence](divergence.md) gate |
| `Asset '{ObjectPath}' is open in an asset editor, so it was NOT rebuilt. ...` | the [open-editor gate](#open-in-an-asset-editor) |

## Example

```c
Shader(Name="Docs/M_Regen")
{
    Properties {
        ScalarParameter Intensity = 2.0 [Group="Look"; SortPriority=10];
        VectorParameter Tint      = float4(1.0, 0.4, 0.1, 1.0) [Group="Look"];
    }
    Settings { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs  { vec3 Color; Base.EmissiveColor = Color; }
    Graph    { Color = Tint.rgb * Intensity; }
    Layout   { Node(Var="Color", X=-400, Y=0); }
}
```

Hand-edit the generated asset, then save the `.dsm` again:

```text
before regeneration                              after regeneration
-----------------------------------------------  --------------------------------------------
comment "DreamShader: Output: EmissiveColor"     recreated
comment "Reviewed 2026-07-30"                    KEPT — no DreamShader: prefix
extra Multiply node wired in by hand             deleted
Two Sided ticked in the material editor          reset to false (not declared in Settings)
Intensity override = 5.0 on the instance         cleared, back to the source default 2.0
Color node dragged to (900, 400)                 back to (-400, 0), pinned by Layout
```

## See also

- [Caching](caching.md) — when regeneration is skipped, and the provenance metadata the guard uses
- [In-memory materials](in-memory.md) — child instances, materializing, and when storage decides the mode
- [Graph layout](graph-layout.md) — why node positions move, and how `Layout` pins them
- [Layout](../language/layout.md) — the `Comment(Name=…)` directive that names generated boxes
- [Shader settings](../settings/material.md) — every key that survives the property reset
- [Function settings](../settings/function.md) — the material-function fields reapplied each rebuild
- [ShaderFunction](../language/shader-function.md) — pin identity and why renaming an input breaks callers
- [Divergence](divergence.md) — what happens when the asset no longer matches what was generated
- [Decompiler](../tools/decompiler.md) — capturing hand edits back into source
- [Asset paths](asset-paths.md) — the class-match rules behind the guard's sibling errors
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
