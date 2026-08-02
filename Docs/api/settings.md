# DreamShaderSettings.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderSettings.h**

The project-settings object, the default-backend enumeration, and the name-to-engine-enum resolvers
that back every `ShadingModel`, `BlendMode` and `Domain` value spelling.

Defined in header `DreamShaderSettings.h`.

| | |
| :-- | :-- |
| Module | `DreamShader` (Runtime) |
| Include | `#include "DreamShaderSettings.h"` |
| Namespace | global scope |
| Export macro | `DREAMSHADER_API` on `UDreamShaderSettings` |
| Reflection | 1 `UENUM`, 1 `UCLASS` with 13 `UPROPERTY`s |
| Pulls in | `CoreMinimal.h`, `Engine/DeveloperSettings.h`, `Engine/EngineTypes.h`, `MaterialDomain.h` *(since 1.3.6)* |

## `EDreamShaderDefaultBackend`

```cpp
/** Backend used for source files that do not specify Settings = { Backend = "..." } themselves. */
UENUM()
enum class EDreamShaderDefaultBackend : uint8
{
    Graph,
    Instance,
    ThinCustom,
};
```

| Enumerator | Value | Meaning |
| :-- | :-- | :-- |
| `Graph` | `0` | Build a `UMaterial` node graph per material — the full DSL feature surface, one visible asset. |
| `Instance` | `1` | **Deprecated** alias for `ThinCustom` *(since 1.5.0)*. Kept for one deprecation window so existing configs and `Settings = { Backend = "Instance" }` sources keep working. The legacy graphless-host instance backend is retired. |
| `ThinCustom` | `2` | Build the graph on a hidden per-material base `UMaterial` and emit a lightweight material instance of it. **The default.** |

> [!WARNING]
> `Instance` resolves silently. A project configured with `DefaultBackend = Instance`, or a source
> file with `Backend = "Instance"`, produces exactly the same asset as `ThinCustom` with no warning
> and no log line. The only indication is the tooltip and this documentation.

### Resolution at generation time

| `Settings = { Backend = … }` | `DefaultBackend` | Resolved backend |
| :-- | :-- | :-- |
| *(absent)* | `Instance` | `ThinCustom` |
| *(absent)* | `ThinCustom` | `ThinCustom` |
| *(absent)* | `Graph` | `Graph` |
| `"Instance"` | *(any)* | `ThinCustom` |
| `"ThinCustom"` | *(any)* | `ThinCustom` |
| `"Graph"` | *(any)* | `Graph` |
| `""` (empty string) | *(any)* | `Graph` |
| anything else | *(any)* | error: `Unsupported Backend '{Value}'. Supported values: Graph, Instance, ThinCustom.` |

The `Backend` value is compared case-insensitively with surrounding quotes trimmed. See
[Backend](../settings/backend.md).

## `UDreamShaderSettings`

```cpp
UCLASS(Config=Engine, DefaultConfig, meta=(DisplayName="DreamShader"))
class DREAMSHADER_API UDreamShaderSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UDreamShaderSettings();

    virtual FName GetContainerName() const override;
    virtual FName GetCategoryName() const override;
    virtual FName GetSectionName() const override;
#if WITH_EDITOR
    virtual FText GetSectionText() const override;
    virtual FText GetSectionDescription() const override;
#endif

    bool TryResolveShadingModel(const FString& InName, EMaterialShadingModel& OutShadingModel) const;
    bool TryResolveBlendMode(const FString& InName, EBlendMode& OutBlendMode) const;
    bool TryResolveMaterialDomain(const FString& InName, EMaterialDomain& OutMaterialDomain) const;

    static FString NormalizeMappingKey(const FString& InName);
    static void BuildDefaultShadingModelMappings(TMap<FString, TEnumAsByte<EMaterialShadingModel>>& OutMappings);
    static void BuildDefaultBlendModeMappings(TMap<FString, TEnumAsByte<EBlendMode>>& OutMappings);
    static void BuildDefaultMaterialDomainMappings(TMap<FString, TEnumAsByte<EMaterialDomain>>& OutMappings);

    // 13 UPROPERTYs — see below.

private:
    static FString NormalizeShadingModelKey(const FString& InName);
    static FString NormalizeBlendModeKey(const FString& InName);
    static FString NormalizeMaterialDomainKey(const FString& InName);
};
```

Obtain it with `GetDefault<UDreamShaderSettings>()` — it is a `UDeveloperSettings`, so the class
default object *is* the live configured instance. There is no subsystem and no getter on the module.

### Panel location

All five overrides are inline and return constants.

| Override | Returns | Availability |
| :-- | :-- | :-- |
| `GetContainerName()` | `TEXT("Project")` | always |
| `GetCategoryName()` | `TEXT("DreamPlugin")` | always |
| `GetSectionName()` | `TEXT("DreamShader")` | always |
| `GetSectionText()` | `"Dream Shader"` | `WITH_EDITOR` only |
| `GetSectionDescription()` | `"Dream Shader Settings"` | `WITH_EDITOR` only |

The panel is therefore **Project Settings ▸ DreamPlugin ▸ Dream Shader** — not under *Plugins*. The
`UCLASS` specifiers `Config=Engine, DefaultConfig` persist every `Config` property to the project's
`Config/DefaultEngine.ini`, section `[/Script/DreamShader.DreamShaderSettings]`.

### Constructor

`UDreamShaderSettings::UDreamShaderSettings()` sets exactly two members:

```cpp
SourceDirectory.Path          = TEXT("DShader");
GeneratedShaderDirectory.Path = TEXT("Intermediate/DreamShader/GeneratedShaders");
```

> [!NOTE]
> **The three `*Mappings` maps start empty and stay empty** unless a user adds entries. The built-in
> alias catalogues are never materialized into them; they are rebuilt into a throwaway local map
> inside each `TryResolve*` call. An empty *Mappings* panel does not mean "no aliases are
> recognized".

### Properties

Every `UPROPERTY` is `Config, EditAnywhere`.

| Property | Type | Category | Default | Extra metadata |
| :-- | :-- | :-- | :-- | :-- |
| `ShadingModelMappings` | `TMap<FString, TEnumAsByte<EMaterialShadingModel>>` | `Mappings` | *empty* | — |
| `BlendModeMappings` | `TMap<FString, TEnumAsByte<EBlendMode>>` | `Mappings` | *empty* | — |
| `MaterialDomainMappings` | `TMap<FString, TEnumAsByte<EMaterialDomain>>` | `Mappings` | *empty* | — |
| `SourceDirectory` | `FDirectoryPath` | `Paths` | `DShader` | `RelativeToGameDir` |
| `GeneratedShaderDirectory` | `FDirectoryPath` | `Paths` | `Intermediate/DreamShader/GeneratedShaders` | `RelativeToGameDir` |
| `DefaultBackend` | `EDreamShaderDefaultBackend` | `Compiler` | `ThinCustom` | `DisplayName="Default Compiler Backend"`, `ToolTip` |
| `bShowInMemoryMaterialsInContentBrowser` | `bool` | `Compiler` | `false` | `DisplayName="Show In-Memory Materials In Content Browser"`, `ToolTip` |
| `bAutoCompileOnSave` | `bool` | `Compiler` | `true` | — |
| `SaveDebounceSeconds` | `float` | `Compiler` | `0.25f` | `ClampMin="0.05"`, `ClampMax="10.0"`, `UIMin="0.05"`, `UIMax="2.0"` |
| `bVerboseLogs` | `bool` | `Compiler` | `false` | — |
| `bExportDecompiledLayout` | `bool` | `Decompiler` | `true` | — |
| `bOpenInNewWindow` | `bool` | `Editor` | `true` | — |
| `InstanceSubfolder` | `FString` | `Editor` | `TEXT("Instances")` | `DisplayName="Material Instance Subfolder"`, `ToolTip` |

What each property *does* is on [Project settings](../settings/project.md); this page documents the
declaration.

> [!NOTE]
> There is deliberately **no in-memory on/off toggle**. The header records the reason: DreamShader
> always generates in the editor's memory — source files are the authoring surface, and the editor
> never writes per-material `.uasset` files — and materializes to disk during cooking.
> `DefaultBackend` is the single compiler knob. See [In-memory materials](../generation/in-memory.md).

## `NormalizeMappingKey`

```cpp
static FString NormalizeMappingKey(const FString& InName);
```

Applies, in order: `TrimStartAndEndInline()`, `ToLowerInline()`, then removal of every `" "`, `"_"`
and `"-"`.

| Input | Result |
| :-- | :-- |
| `"Two Sided Foliage"` | `"twosidedfoliage"` |
| `"two-sided_foliage"` | `"twosidedfoliage"` |
| `"  TWOSIDEDFOLIAGE  "` | `"twosidedfoliage"` |
| `"Runtime Virtual Texture"` | `"runtimevirtualtexture"` |

Alias matching is therefore case-, space-, underscore- and hyphen-insensitive.

> [!WARNING]
> Do not confuse this with [`UE::DreamShader::NormalizeSettingKey`](types.md#normalizesettingkey),
> which only trims and lower-cases. `NormalizeMappingKey` applies to setting **values** (enum
> aliases); `NormalizeSettingKey` applies to setting **keys**. See the
> [normalizer comparison](dreamshader-module.md#the-four-normalizers-compared).

The three private forwarders `NormalizeShadingModelKey`, `NormalizeBlendModeKey` and
`NormalizeMaterialDomainKey` call `NormalizeMappingKey` and add nothing.

## The resolvers

```cpp
bool TryResolveShadingModel(const FString& InName, EMaterialShadingModel& OutShadingModel) const;
bool TryResolveBlendMode(const FString& InName, EBlendMode& OutBlendMode) const;
bool TryResolveMaterialDomain(const FString& InName, EMaterialDomain& OutMaterialDomain) const;
```

All three share one algorithm:

| Step | Action |
| :-- | :-- |
| 1 | Normalize `InName` with `NormalizeMappingKey`. |
| 2 | Linear-scan the **user-configured** map, normalizing each stored key on the fly. On a match, write `OutValue` and return `OutValue != <ENUM>_MAX`. |
| 3 | If nothing matched, build the default catalogue into a **fresh local map** and scan it identically. |
| 4 | Return `false`. `OutValue` is left untouched. |

| Aspect | Behaviour |
| :-- | :-- |
| Precedence | The user map always wins; the first normalized-key match returns immediately. |
| Disabling an alias | An entry mapped to `MSM_MAX` / `BLEND_MAX` / `MD_MAX` **overwrites `OutValue`** with that sentinel and returns `false`. It does not fall through to the built-in table. |
| Ambiguity | Two user entries whose keys normalize identically are resolved in `TMap` iteration order, which is unspecified. |
| `OutValue` on failure | Untouched by steps 1–4 unless the `MAX` sentinel case above applied. |
| Constness | `const`; safe on the CDO. |

> [!NOTE]
> Step 3 rebuilds the entire default catalogue — including a full `StaticEnum` walk with per-index
> metadata queries — on **every miss of the user map**. Because the user maps are empty by default,
> that is effectively every call. This is fine at generation frequency; do not call these in a
> per-node or per-frame loop.

## Building the default catalogues

```cpp
static void BuildDefaultShadingModelMappings(TMap<FString, TEnumAsByte<EMaterialShadingModel>>& OutMappings);
static void BuildDefaultBlendModeMappings(TMap<FString, TEnumAsByte<EBlendMode>>& OutMappings);
static void BuildDefaultMaterialDomainMappings(TMap<FString, TEnumAsByte<EMaterialDomain>>& OutMappings);
```

Each appends to `OutMappings` — it does **not** clear it — in two phases.

### Phase 1: reflected

Walk `StaticEnum<T>()` index by index. For each index:

| Step | Rule |
| :-- | :-- |
| 1 | Read the value and the name string. |
| 2 | Read the `Hidden` metadata — **only under `WITH_EDITORONLY_DATA`**. |
| 3 | Apply the per-enum skip predicate; `continue` if it returns `true`. |
| 4 | Build the alias: take the name, drop everything up to and including the last `:` (scoped-enum support), then `RemoveFromStart(<Prefix>, ESearchCase::CaseSensitive)`. |
| 5 | Add it, additively. |

> [!WARNING]
> `UEnum` metadata is editor-only. In non-editor and shipping builds `bHidden` is hard-coded to
> `false`, so nothing is dropped for being hidden and only the name- and value-based parts of the
> skip predicate apply. A shipping build can therefore recognize a few extra aliases the editor
> rejects. This is a deliberate fix: `UEnum::HasMetaData` does not compile in those configurations.

Phase 2 adds literal aliases through the same additive helper.

### Additive-only semantics

The helper skips an alias when it trims to empty, **or** when the map already contains that exact
`FString` key — an exact, case-sensitive, unnormalized key comparison. Because the reflected phase
runs first, an explicit alias that is character-identical to a reflected one is a silent no-op.

### `EMaterialShadingModel` — prefix `MSM_`

Skip predicate:

| Condition | Result |
| :-- | :-- |
| Name is `Strata` or `MSM_Strata`, or the value is `MSM_Strata` | Kept when `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` (UE ≥ 5.4); dropped otherwise. **This test runs first and short-circuits the `Hidden` check.** |
| `Hidden` metadata | skip |
| Name is `NUM`, `MSM_NUM`, `MAX`, `MSM_MAX`, `FromMaterialExpression` or `MSM_FromMaterialExpression` | skip |
| Value is `MSM_NUM`, `MSM_MAX` or `MSM_FromMaterialExpression` | skip |

Reflected aliases — 12, plus `Strata` on UE ≥ 5.4:

| | | | |
| :-- | :-- | :-- | :-- |
| `Unlit` | `DefaultLit` | `Subsurface` | `PreintegratedSkin` |
| `ClearCoat` | `SubsurfaceProfile` | `TwoSidedFoliage` | `Hair` |
| `Cloth` | `Eye` | `SingleLayerWater` | `ThinTranslucent` |
| `Strata` *(since UE 5.4)* | | | |

Explicit aliases — 8, plus 2 on UE ≥ 5.4:

| Alias | Value | Note |
| :-- | :-- | :-- |
| `Default Lit` | `MSM_DefaultLit` | |
| `Lit` | `MSM_DefaultLit` | |
| `Preintegrated Skin` | `MSM_PreintegratedSkin` | |
| `Clear Coat` | `MSM_ClearCoat` | |
| `Subsurface Profile` | `MSM_SubsurfaceProfile` | |
| `Two Sided Foliage` | `MSM_TwoSidedFoliage` | |
| `Single Layer Water` | `MSM_SingleLayerWater` | |
| `Thin Translucent` | `MSM_ThinTranslucent` | |
| `Substrate` | `MSM_Strata` | *(UE 5.4+ only)* |
| `Strata` | `MSM_Strata` | *(UE 5.4+ only)*; **no-op** — the reflected phase already added the identical key |

### `EBlendMode` — prefix `BLEND_`

Skip predicate: `Hidden` metadata, or the name is `MAX` or `BLEND_MAX`, or the value is `BLEND_MAX`.

Reflected aliases — 8:

| | | | |
| :-- | :-- | :-- | :-- |
| `Opaque` | `Masked` | `Translucent` | `Additive` |
| `Modulate` | `AlphaComposite` | `AlphaHoldout` | `TranslucentColoredTransmittance` |

`BLEND_TranslucentGreyTransmittance` and `BLEND_ColoredTransmittanceOnly` are engine value aliases
marked `UMETA(Hidden)` and are dropped by the `Hidden` test in editor builds.

Explicit aliases — 4:

| Alias | Value |
| :-- | :-- |
| `Cutout` | `BLEND_Masked` |
| `Transparent` | `BLEND_Translucent` |
| `PremultipliedAlpha` | `BLEND_AlphaComposite` |
| `Premultiplied` | `BLEND_AlphaComposite` |

### `EMaterialDomain` — prefix `MD_`

Skip predicate: `Hidden` metadata **unless the value is `MD_RuntimeVirtualTexture`**, or the name is
`MAX` or `MD_MAX`, or the value is `MD_MAX`. The exception is deliberate — the engine marks
`MD_RuntimeVirtualTexture` hidden, and DreamShader keeps it usable.

Reflected aliases — 7:

| | | | |
| :-- | :-- | :-- | :-- |
| `Surface` | `DeferredDecal` | `LightFunction` | `Volume` |
| `PostProcess` | `UI` | `RuntimeVirtualTexture` | |

Explicit aliases — 9 written, 8 effective:

| Alias | Value | Note |
| :-- | :-- | :-- |
| `DeferredDecal` | `MD_DeferredDecal` | **no-op** — the reflected key is identical |
| `Decal` | `MD_DeferredDecal` | |
| `Light Function` | `MD_LightFunction` | |
| `Post Process` | `MD_PostProcess` | |
| `UserInterface` | `MD_UI` | |
| `User Interface` | `MD_UI` | |
| `Runtime Virtual Texture` | `MD_RuntimeVirtualTexture` | |
| `VirtualTexture` | `MD_RuntimeVirtualTexture` | |
| `Virtual Texture` | `MD_RuntimeVirtualTexture` | |

> [!NOTE]
> The spaced aliases are **lookup-redundant**: `NormalizeMappingKey` strips spaces, so
> `Two Sided Foliage` and `TwoSidedFoliage` normalize to the same key that the reflected phase
> already covers. They exist as distinct map entries so that the Project Settings panel and the
> bridge manifest consumed by the editor extensions can offer human-readable spellings.

The user-facing catalogue, including which spellings are recommended, is on
[Material enums](../settings/material-enums.md).

## Notes

- The reflected phase enumerates the **engine's** enum, so the exact reflected set is whatever the
  engine version defines. The tables above list the result on a current engine; the only entry the
  plugin itself gates by version is `Strata`.
- `TEnumAsByte<T>` is used throughout because `TMap` value types in a `UPROPERTY` must be reflectable
  and these engine enums are not `enum class`.
- The class has no `PostEditChangeProperty` override. Reacting to a settings change is done by the
  editor module's own panel hooks, not by the settings object.
- `MaterialDomain.h` is included by this header *(since 1.3.6)*, so a translation unit that includes
  `DreamShaderSettings.h` gets `EMaterialDomain` without adding an include of its own.

## Example

Resolving a user-written spelling to an engine enum:

```cpp
#include "DreamShaderSettings.h"

bool ApplyShadingModelSetting(UMaterial* Material, const FString& InValue, FString& OutError)
{
    const UDreamShaderSettings* Settings = GetDefault<UDreamShaderSettings>();

    EMaterialShadingModel ShadingModel = MSM_DefaultLit;
    if (!Settings->TryResolveShadingModel(InValue, ShadingModel))
    {
        OutError = FString::Printf(TEXT("Unknown ShadingModel '%s'."), *InValue);
        return false;
    }

    Material->SetShadingModel(ShadingModel);
    return true;
}
```

All four of these calls succeed and select the same shading model:

```cpp
ApplyShadingModelSetting(M, TEXT("TwoSidedFoliage"),   Error);
ApplyShadingModelSetting(M, TEXT("Two Sided Foliage"), Error);
ApplyShadingModelSetting(M, TEXT("two-sided_foliage"), Error);
ApplyShadingModelSetting(M, TEXT("  TWOSIDEDFOLIAGE"), Error);
```

Enumerating the built-in catalogue without a settings object — this is how the workspace service
exports the alias manifest:

```cpp
TMap<FString, TEnumAsByte<EBlendMode>> Aliases;
UDreamShaderSettings::BuildDefaultBlendModeMappings(Aliases);   // 12 entries: 8 reflected + 4 explicit
```

## See also

- [Project settings](../settings/project.md) — the same object from the user's side, with effects
- [Material enums](../settings/material-enums.md) — the complete accepted value catalogue
- [Backend](../settings/backend.md) — how `DefaultBackend` interacts with a per-file `Backend`
- [`DreamShaderTypes.h`](types.md) — `NormalizeSettingKey`, the other normalizer
- [`DreamShaderModule.h`](dreamshader-module.md) — the directory helpers that consume the two path settings
- [`DreamShaderMaterialInstance.h`](material-instance.md) — the class whose `IsAsset()` reads `bShowInMemoryMaterialsInContentBrowser`
- [`DreamShaderVersionCompat.h`](version-compat.md) — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`, the `Strata` gate
- [Shader settings](../settings/material.md) — the `Settings` keys these resolvers serve
- [Workspace](../tools/workspace.md) — the exported alias manifest and `bridge.db`
