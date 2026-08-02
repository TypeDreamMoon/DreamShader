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
`.dsm` or `.dsf`. Every hand edit inside it is destroyed, with exactly one exception.

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

The whole-file parse, the `Settings` validation and the `Outputs` validation all run **before** the
target asset is created or cleared, so a source file that fails any of them leaves the previously
generated asset intact. A syntax error *inside* a `Graph` block is not one of those gates: the
statement parser runs after step 4, so such a failure leaves the asset emptied.

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
> instance is wiped on every regeneration** — scalar, vector, texture, static switch, and static
> component-mask alike. No diagnostic is emitted; the values are simply gone the next time the source
> is compiled.
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

## Ownership guard

DreamShader refuses to overwrite an asset it did not generate.

| | |
| :-- | :-- |
| Fires when | the target package exists **on disk** and the existing object carries **no** `DreamShader.SourceFile` metadata |
| Applies to | `Shader` under the `Graph` backend, and `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` |
| Result | generation fails; the existing asset is untouched |

| Message | Raised for |
| :-- | :-- |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your shader or move/delete the existing asset before regenerating.` | a material |
| `Asset '{ObjectPath}' already exists and was not generated by DreamShader. Rename your function or move/delete the existing asset before regenerating.` | a material function |

### Where the guard does not apply

> [!WARNING]
> There is **no ownership guard on the ThinCustom instance path**. Generating a `Shader` under the
> default backend onto a path that holds a hand-authored `UDreamShaderMaterialInstance` checks only
> the class, not the provenance metadata — and then rebuilds it, clearing its parameter overrides.
> A hand-authored asset of any *other* class at that path is still rejected, with
> `Asset '{ObjectPath}' already exists and is not a DreamShader instance material. Delete it (or
> remove Backend="Instance") before switching backends.`
>
> **Workaround:** do not hand-author `UDreamShaderMaterialInstance` assets. Create child instances as
> plain `UMaterialInstanceConstant`, which the guard's class check rejects outright.

The guard is also inert for packages that are not on disk: a memory-only asset has no saved package
to protect, so the check does not run.

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
  `RF_Transactional`, because undo/redo desynchronizes the shader map.
- The safest mental model: treat the `.dsm` / `.dsf` as the asset. Anything you want to persist
  belongs in the source.
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
| `In-memory material mode: '{PackageName}' already exists as a saved asset, which shadows in-memory regeneration. Delete the saved asset to make it fully in-memory.` | log warning; a saved asset shadows a memory-only rebuild |

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
- [In-memory materials](in-memory.md) — child instances, materializing, cleaning shadowing assets
- [Graph layout](graph-layout.md) — why node positions move, and how `Layout` pins them
- [Layout](../language/layout.md) — the `Comment(Name=…)` directive that names generated boxes
- [Shader settings](../settings/material.md) — every key that survives the property reset
- [Function settings](../settings/function.md) — the material-function fields reapplied each rebuild
- [ShaderFunction](../language/shader-function.md) — pin identity and why renaming an input breaks callers
- [Decompiler](../tools/decompiler.md) — capturing hand edits back into source
- [Asset paths](asset-paths.md) — the class-match rules behind the guard's sibling errors
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
