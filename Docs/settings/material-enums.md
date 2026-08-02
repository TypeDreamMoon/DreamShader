# Material enums

> [DreamShader](../index.md) » [Settings](index.md) » **Material enums**

The complete set of value spellings accepted by the `ShadingModel`, `BlendMode` / `RenderType` and
`MaterialDomain` / `Domain` keys of a [`Shader`](../language/shader.md) `Settings` block.

| | |
| :-- | :-- |
| Declared in | `.dsm` — as the value of a `Shader` `Settings` key |
| Kind | value catalogue |
| Generates | `UMaterial::BlendMode`, the material's shading model, `UMaterial::MaterialDomain` |

## Synopsis

```c
Settings [=] {
    [ShadingModel   = <shading-model>;]
    [{ BlendMode | RenderType }     = <blend-mode>;]
    [{ MaterialDomain | Domain }    = <domain>;]
}
```

## How a value is matched

1. The value is normalized: trimmed, lowercased, then every space, `_` and `-` is deleted.
2. The project's mapping map for that key is scanned first. **The first normalized-key match wins and
   returns immediately** — a project entry shadows the built-in spelling and there is no fall-through.
3. Only if no project entry matched is the built-in table built and scanned the same way.
4. A match whose value is the enum's `_MAX` sentinel counts as **failure**, so a project entry
   pointing at `MSM_MAX` / `BLEND_MAX` / `MD_MAX` makes that spelling stop resolving.

Because of step 1, `"Default Lit"`, `"DefaultLit"`, `"default_lit"` and `"DEFAULT-LIT"` are the same
alias. The tables below list the raw spellings the plugin registers; every separator variant of each
is equally valid.

The built-in tables are built in two phases: a **reflected** phase that walks the engine enum and
strips the enumerator prefix, then an **explicit** phase that adds hand-written spellings. The
explicit phase never overwrites a reflected entry with the identical key, so a few explicit rows are
no-ops kept for clarity.

## ShadingModel

Reflected from `EMaterialShadingModel`, prefix `MSM_` stripped.

| Alias | Source | Engine enumerator |
| :-- | :-- | :-- |
| `Unlit` | reflected | `MSM_Unlit` |
| `DefaultLit` | reflected | `MSM_DefaultLit` |
| `Subsurface` | reflected | `MSM_Subsurface` |
| `PreintegratedSkin` | reflected | `MSM_PreintegratedSkin` |
| `ClearCoat` | reflected | `MSM_ClearCoat` |
| `SubsurfaceProfile` | reflected | `MSM_SubsurfaceProfile` |
| `TwoSidedFoliage` | reflected | `MSM_TwoSidedFoliage` |
| `Hair` | reflected | `MSM_Hair` |
| `Cloth` | reflected | `MSM_Cloth` |
| `Eye` | reflected | `MSM_Eye` |
| `SingleLayerWater` | reflected | `MSM_SingleLayerWater` |
| `ThinTranslucent` | reflected | `MSM_ThinTranslucent` |
| `Strata` | reflected *(since UE 5.4)* | `MSM_Strata` |
| `Default Lit` | explicit alias | `MSM_DefaultLit` |
| `Lit` | explicit alias | `MSM_DefaultLit` |
| `Preintegrated Skin` | explicit alias | `MSM_PreintegratedSkin` |
| `Clear Coat` | explicit alias | `MSM_ClearCoat` |
| `Subsurface Profile` | explicit alias | `MSM_SubsurfaceProfile` |
| `Two Sided Foliage` | explicit alias | `MSM_TwoSidedFoliage` |
| `Single Layer Water` | explicit alias | `MSM_SingleLayerWater` |
| `Thin Translucent` | explicit alias | `MSM_ThinTranslucent` |
| `Substrate` | explicit alias *(since UE 5.4)* | `MSM_Strata` |
| `Strata` | explicit alias *(since UE 5.4)* — no-op, the reflected phase already registered this key | `MSM_Strata` |

**15 distinct normalized aliases** on UE 5.4 – 5.8; **13** on UE 5.3, which has neither `strata` nor
`substrate`.

Excluded: `MSM_NUM`, `MSM_MAX` and — deliberately — **`MSM_FromMaterialExpression`**. Writing
`ShadingModel = "FromMaterialExpression";` is an error, not a way to drive the shading model from the
graph.

On UE 5.3, `ShadingModel = "Substrate";` and `ShadingModel = "Strata";` are rejected with a dedicated
message rather than the generic "unsupported" one. See [Diagnostics](#diagnostics) and
[Substrate builtins](../builtins/substrate.md).

## BlendMode

Reflected from `EBlendMode`, prefix `BLEND_` stripped. The key may also be spelled `RenderType`.

| Alias | Source | Engine enumerator |
| :-- | :-- | :-- |
| `Opaque` | reflected | `BLEND_Opaque` |
| `Masked` | reflected | `BLEND_Masked` |
| `Translucent` | reflected | `BLEND_Translucent` |
| `Additive` | reflected | `BLEND_Additive` |
| `Modulate` | reflected | `BLEND_Modulate` |
| `AlphaComposite` | reflected | `BLEND_AlphaComposite` |
| `AlphaHoldout` | reflected | `BLEND_AlphaHoldout` |
| `TranslucentColoredTransmittance` | reflected | `BLEND_TranslucentColoredTransmittance` |
| `Cutout` | explicit alias | `BLEND_Masked` |
| `Transparent` | explicit alias | `BLEND_Translucent` |
| `PremultipliedAlpha` | explicit alias | `BLEND_AlphaComposite` |
| `Premultiplied` | explicit alias | `BLEND_AlphaComposite` |

**12 distinct normalized aliases** on a stock UE 5.4 – 5.8 build. `TranslucentColoredTransmittance` is
a Substrate-only enumerator, so the count is engine-version dependent.

Excluded: `BLEND_MAX`, and the two engine enumerators marked hidden —
`BLEND_TranslucentGreyTransmittance` and `BLEND_ColoredTransmittanceOnly`. Both are aliases of values
already in the table, so nothing is unreachable.

> [!NOTE]
> `TranslucentColoredTransmittance` does not survive a decompile. The
> [decompiler](../tools/decompiler.md) writes `BLEND_TranslucentColoredTransmittance` back out as
> `"Translucent"`, so a `UMaterial` → `.dsm` → `UMaterial` round trip downgrades the blend mode.
> Re-add the setting by hand after decompiling such a material.

## Domain

Reflected from `EMaterialDomain`, prefix `MD_` stripped. The key may also be spelled
`MaterialDomain`, which wins if both are present.

| Alias | Source | Engine enumerator |
| :-- | :-- | :-- |
| `Surface` | reflected | `MD_Surface` |
| `DeferredDecal` | reflected | `MD_DeferredDecal` |
| `LightFunction` | reflected | `MD_LightFunction` |
| `Volume` | reflected | `MD_Volume` |
| `PostProcess` | reflected | `MD_PostProcess` |
| `UI` | reflected | `MD_UI` |
| `RuntimeVirtualTexture` | reflected — hidden in the engine, kept on purpose | `MD_RuntimeVirtualTexture` |
| `DeferredDecal` | explicit alias — no-op, the reflected phase already registered this key | `MD_DeferredDecal` |
| `Decal` | explicit alias | `MD_DeferredDecal` |
| `Light Function` | explicit alias | `MD_LightFunction` |
| `Post Process` | explicit alias | `MD_PostProcess` |
| `UserInterface` | explicit alias | `MD_UI` |
| `User Interface` | explicit alias | `MD_UI` |
| `Runtime Virtual Texture` | explicit alias | `MD_RuntimeVirtualTexture` |
| `VirtualTexture` | explicit alias | `MD_RuntimeVirtualTexture` |
| `Virtual Texture` | explicit alias | `MD_RuntimeVirtualTexture` |

**10 distinct normalized aliases**, stable across UE 5.3 – 5.8.

Excluded: `MD_MAX`. Every other hidden enumerator is skipped, with `MD_RuntimeVirtualTexture` as the
single deliberate exception — the engine marks it hidden, the plugin keeps it.

## Adding project-specific spellings

Three `TMap<FString, …>` properties on the project settings extend the tables:

| Setting | Value type | Extends |
| :-- | :-- | :-- |
| `ShadingModelMappings` | `EMaterialShadingModel` | `ShadingModel` |
| `BlendModeMappings` | `EBlendMode` | `BlendMode` / `RenderType` |
| `MaterialDomainMappings` | `EMaterialDomain` | `MaterialDomain` / `Domain` |

They live under *Project Settings ▸ DreamPlugin ▸ Dream Shader ▸ Mappings* and are **empty by
default** — the built-in tables above are never materialized into them. See
[Project settings](project.md).

| Entry | Effect |
| :-- | :-- |
| A key that normalizes to an existing built-in alias | **shadows** the built-in; the project value is used |
| A brand-new key | adds an accepted spelling |
| Any key mapped to `MSM_MAX` / `BLEND_MAX` / `MD_MAX` | makes that spelling **fail to resolve**; it does not fall through to the built-in table |

Keys are compared after normalization, so casing, spaces, underscores and hyphens in the entry do not
matter.

> [!WARNING]
> Two project entries whose keys normalize to the same string are scanned in unspecified hash-map
> order — which one wins is undefined. Do not add both `My Model` and `my_model`.

## Notes

- The reflected phase walks whatever the compiled engine's enum contains. **A modified engine build
  contributes its own values automatically**: an engine that adds `MSM_Toon` makes
  `ShadingModel = "Toon";` work with no plugin change and no project mapping. The tables above
  describe a stock UE 5.3 – 5.8 build.
- Enumerator membership comes from the compiled engine, so it is engine-version dependent. The values
  the plugin itself gates are `Strata` / `Substrate`, which it registers only on UE 5.4 and newer.
- Values carrying `Hidden` metadata are filtered out only in builds that keep editor-only data.
  Material generation is editor-only, so this is the configuration that applies in practice.
- `Saved/DreamShader/Bridge/settings.json` exports these tables for editor completion. It
  de-duplicates by normalized alias, so it shows `"Default Lit"` but not `"DefaultLit"`. **Both still
  compile** — the manifest is a completion surface, not the acceptance set. See
  [Workspace](../tools/workspace.md) and [Bridge](../tools/bridge.md).
- Omitting a key does not leave the previous value in place: a regenerated material is reset to
  `Opaque` / `DefaultLit` / `Surface` first. See [Shader settings](material.md#defaults--what-an-omitted-setting-resets-to).

## Diagnostics

Runtime substitutions are shown as `{Placeholder}`.

| Message | Cause |
| :-- | :-- |
| `Unsupported ShadingModel '{Value}'.` | the value matched no project mapping and no built-in alias, or matched an entry mapped to `MSM_MAX` |
| `ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` | the value trims and case-folds to `Substrate` or `Strata` on UE 5.3 |
| `Unsupported BlendMode/RenderType '{Value}'.` | no match for the `BlendMode` / `RenderType` value |
| `Unsupported MaterialDomain '{Value}'.` | no match for the `MaterialDomain` / `Domain` value |

All three are raised during validation, before anything is written to the material.

## Example

```c
Shader(Name="Docs/M_EnumSpellings")
{
    Properties { vec3 Tint = vec3(0.9, 0.2, 0.1); }

    Settings {
        Domain       = "User Interface";   // -> MD_UI
        ShadingModel = "Lit";              // -> MSM_DefaultLit
        RenderType   = "Premultiplied";    // -> BLEND_AlphaComposite
    }

    Outputs {
        vec3  Color;
        float Alpha;
        Base.EmissiveColor = Color;
        Base.Opacity       = Alpha;
    }

    Graph {
        Color = Tint;
        Alpha = 0.75;
    }
}
```

Resulting material state:

```text
MaterialDomain = MD_UI
ShadingModel   = MSM_DefaultLit
BlendMode      = BLEND_AlphaComposite
```

## See also

- [Shader settings](material.md) — the special keys these values belong to
- [Settings](index.md) — block grammar and the three normalization rules
- [Project settings](project.md) — `ShadingModelMappings`, `BlendModeMappings`, `MaterialDomainMappings`
- [Backend](backend.md) — the remaining special key
- [Substrate builtins](../builtins/substrate.md) — the UE 5.4+ Substrate surface
- [Output bindings](../language/output-bindings.md) — `Base.FrontMaterial` and the forced Substrate model
- [Decompiler](../tools/decompiler.md) — the canonical spellings written back out
- [Workspace](../tools/workspace.md) — the exported `settings.json` completion manifest
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
