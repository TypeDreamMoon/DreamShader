# DreamShaderVersionCompat.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderVersionCompat.h**

Twelve preprocessor macros that gate every engine-version-dependent behaviour in the plugin. This
page is the source of truth for every *(since UE 5.x)* marker in the manual.

Defined in header `DreamShaderVersionCompat.h`.

| | |
| :-- | :-- |
| Module | `DreamShader` (Runtime), included by all three modules |
| Include | `#include "DreamShaderVersionCompat.h"` |
| Contents | 12 macros. **No types, no functions, no namespace.** |
| Only dependency | `Runtime/Launch/Resources/Version.h` |
| Verified engines | UE `5.3` – `5.8` (Win64) |

## Synopsis

The version arithmetic every other macro is built on. The feature gates that use it are in
[Macros](#macros) below.

```cpp
#include "Runtime/Launch/Resources/Version.h"

#ifndef DREAMSHADER_UE_MAJOR
#define DREAMSHADER_UE_MAJOR ENGINE_MAJOR_VERSION
#endif

#ifndef DREAMSHADER_UE_MINOR
#define DREAMSHADER_UE_MINOR ENGINE_MINOR_VERSION
#endif

#ifndef DREAMSHADER_UE_PATCH
#define DREAMSHADER_UE_PATCH ENGINE_PATCH_VERSION
#endif

#ifndef DREAMSHADER_WITH_SUBSTRATE_BUILTINS
#define DREAMSHADER_WITH_SUBSTRATE_BUILTINS (DREAMSHADER_UE_MAJOR > 5 || (DREAMSHADER_UE_MAJOR == 5 && DREAMSHADER_UE_MINOR >= 4))
#endif

#define DREAMSHADER_UE_VERSION_AT_LEAST(MajorVersion, MinorVersion) \
    (DREAMSHADER_UE_MAJOR > (MajorVersion) || (DREAMSHADER_UE_MAJOR == (MajorVersion) && DREAMSHADER_UE_MINOR >= (MinorVersion)))

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)
#define DREAMSHADER_ALLOW_SHRINKING_NO EAllowShrinking::No
#else
#define DREAMSHADER_ALLOW_SHRINKING_NO false
#endif
```

## Macros

| Macro | Expands to | Overridable | Purpose |
| :-- | :-- | :-- | :-- |
| `DREAMSHADER_UE_MAJOR` | `ENGINE_MAJOR_VERSION` | **yes** — `#ifndef` guarded | The major version every other test is built on. |
| `DREAMSHADER_UE_MINOR` | `ENGINE_MINOR_VERSION` | **yes** — `#ifndef` guarded | The minor version. |
| `DREAMSHADER_UE_PATCH` | `ENGINE_PATCH_VERSION` | **yes** — `#ifndef` guarded | Declared for completeness. **Never referenced anywhere in the plugin.** |
| `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` | `(MAJOR > 5 \|\| (MAJOR == 5 && MINOR >= 4))` — i.e. **UE ≥ 5.4** | **yes** — `#ifndef` guarded | The single Substrate feature gate. |
| `DREAMSHADER_WITH_MOON_ENGINE` | `1` when the engine's `SceneTypes.h` declares `MP_MoonEncodedAttribute0`, `0` otherwise | **yes** — `#ifndef` guarded, and `DreamShader.Build.cs` sets it as a `PublicDefinition` from that probe | Moon Engine's extra material attributes. Not a version test: the probe reads the enumerator the guarded code needs, so it works for any fork that carries it. |
| `DREAMSHADER_UE_VERSION_AT_LEAST(Major, Minor)` | `(MAJOR > Major \|\| (MAJOR == Major && MINOR >= Minor))` | no — unconditional `#define` | The general "at least this engine" test. |
| `DREAMSHADER_ALLOW_SHRINKING_NO` | `EAllowShrinking::No` on UE ≥ 5.4, `false` below | no — selected by `#if` | Portability shim, described below. |
| `DREAMSHADER_POST_ENGINE_INIT_DELEGATE()` | `FCoreDelegates::GetOnPostEngineInit()` on UE ≥ 5.8, the `OnPostEngineInit` data member below | no — selected by `#if` | 5.8 added the accessor and deprecated the member, so naming either one directly breaks the other engine. |
| `DREAMSHADER_THUMBNAIL_PRIM_SHADERBALL` | `TPT_ShaderBall` on UE ≥ 5.8, `TPT_Sphere` below | no — selected by `#if` | `EThumbnailPrimType` stops at `TPT_Cylinder` before 5.8; the preview falls back to the sphere. |
| `DREAMSHADER_MATERIAL_AGGREGATE_HANDLES_SUBSTRATE` | **UE ≥ 5.8** | no — unconditional `#define` | Whether `MaterialValueTypeToMaterialAggregateAttributeType` has a case for `MCT_Substrate`. Below 5.8 it `checkf(false)`s, so the manifest exporter must not make the call for `UMaterialExpressionAggregate` at all. |
| `DREAMSHADER_WITH_SCALAR_PARAMETER_CONTROL_TYPE` | **UE ≥ 5.7** | no — unconditional `#define` | Whether `UMaterialExpressionScalarParameter` has `ControlType`, `Enumeration` and `EnumerationIndex`. |
| `DREAMSHADER_WITH_MATERIAL_PARAMETERS_HEADER` | **UE ≥ 5.7** | no — unconditional `#define` | Which header declares `FMaterialParameterInfo`: `Materials/MaterialParameters.h` from 5.7, `MaterialTypes.h` before it. |

The five `#ifndef`-guarded ones may be pre-defined by a build target — for example to compile the
Substrate paths out on a 5.4+ engine by defining `DREAMSHADER_WITH_SUBSTRATE_BUILTINS=0`, or to force
a version branch when testing. `DreamShader.Build.cs` uses that to set
`DREAMSHADER_WITH_MOON_ENGINE`, and is the only `PublicDefinitions` entry in the plugin; no module
sets `PrivateDefinitions` at all.

> [!NOTE]
> `DREAMSHADER_ALLOW_SHRINKING_NO` carries **no behavioural difference**. It exists solely because
> UE 5.4 changed the trailing `bool bAllowShrinking` parameter of `TArray::RemoveAt`, `TArray::Pop`,
> `FString::RightChopInline` and `FString::LeftChopInline` into an `EAllowShrinking` enum. Both
> spellings mean "do not shrink the allocation". It is used 37 times across the plugin and is not a
> tuning knob.

> [!NOTE]
> There are **no** raw `#if ENGINE_MAJOR_VERSION`, `ENGINE_MINOR_VERSION` or `UE_VERSION_NEWER_THAN`
> tests anywhere in the plugin outside this header. Every version-dependent behaviour goes through
> these macros, which is what makes the table below exhaustive.

## Complete version-gated behaviour

Every site in the plugin guarded by one of these macros, grouped by the version it requires.
Runtime substitutions in quoted messages are shown as `{Placeholder}`.

### UE ≥ 5.4 — `DREAMSHADER_UE_VERSION_AT_LEAST(5, 4)`

| Feature | On UE ≥ 5.4 | On UE 5.3 |
| :-- | :-- | :-- |
| `TArray` / `FString` shrink argument | `EAllowShrinking::No` | `false` |
| Generated Custom nodes | `UMaterialExpressionCustom::ShowCode = false` on every node the generator creates, so the HLSL body stays collapsed in the material editor | the property does not exist; not set, and generated Custom nodes show their code |
| Material reset before a rebuild | `bHasPixelAnimation` is cleared along with the other material flags | skipped |
| Decompiler material-flag export | `bHasPixelAnimation` is included in the emitted `Settings` flag list | excluded |

### UE ≥ 5.4 — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`

The Substrate gate is a separate macro so it can be overridden independently, but its default
threshold is the same 5.4.

| Feature | On UE ≥ 5.4 | On UE 5.3 |
| :-- | :-- | :-- |
| `Substrate.*` builtins | The builtin table is compiled in; an unrecognized name fails with `Unsupported Substrate builtin call '{Name}' in Graph.` | The table is absent; every `Substrate.*` call fails with `Substrate builtin call '{Name}' requires Unreal Engine 5.4 or newer.` |
| `MaterialExpressionSubstrate.h` | included by the generator, the workspace service and the three decompiler files | not included |
| `ShadingModel = "Substrate"` | accepted | error: `ShadingModel="Substrate" requires Unreal Engine 5.4 or newer.` |
| Shading-model alias catalogue | `MSM_Strata` is kept despite its `Hidden` metadata, and the `Substrate` and `Strata` aliases are added | `MSM_Strata` is skipped and neither alias is added |
| `Base.FrontMaterial` output binding | resolves to `MP_FrontMaterial` | the token is unknown, and the binding fails with `Base.FrontMaterial requires Unreal Engine 5.4 or newer.` |
| Material connection type for Substrate values | `MCT_Substrate` | `MCT_Strata` |
| `IsSubstrateMaterialTypeSupported()` | `true` | `false` |
| Decompiler type naming | `MCT_Substrate` prints as `"Substrate"` | `MCT_Strata` prints as `"Substrate"` |
| Decompiler shading-model export | `"Substrate"` is emitted for `MSM_Strata` | not emitted |
| Decompiler binding table | `MP_FrontMaterial` is included | excluded |
| `substrate-builtins.json` manifest | `supported: true` and a populated `builtins` array | `supported: false` with `unsupportedReason: "Substrate builtins require Unreal Engine 5.4 or newer."`, and an empty `builtins` array |
| Bridge manifest, shading models | `MSM_Strata` offered | `MSM_Strata` added to the excluded-shading-model set |

### UE ≥ 5.5

| Feature | On UE ≥ 5.5 | On UE 5.3 – 5.4 |
| :-- | :-- | :-- |
| Counting an expression's inputs | `Expression->CountInputs()` | `Expression->GetInputsView().Num()` |
| Resolving `UMaterialExpressionObjectPositionWS` | `StaticClass()` and `IsA<>` directly | `FindObject<UClass>(nullptr, "/Script/Engine.MaterialExpressionObjectPositionWS")` |
| Transform basis `periodicworld` | resolves to `TRANSFORMPOSSOURCE_PeriodicWorld` | unsupported — basis resolution fails |
| `UE.TransformPosition` argument `PeriodicWorldTileSize` | honoured | not handled |

### UE ≥ 5.6

| Feature | On UE ≥ 5.6 | On UE 5.3 – 5.5 |
| :-- | :-- | :-- |
| Package metadata access | `UPackage::GetMetaData()` returns `FMetaData&` | returns `UMetaData*`, null-checked before use |
| Expression pin value types | `GetInputValueType` / `GetOutputValueType` | `GetInputType` / `GetOutputType`, cast to `EMaterialValueType` |
| Rebuilding an expression's outputs | `Expression->RebuildOutputs()` | manual `Outputs.Reset()` followed by a rebuild |
| Resolving `UMaterialExpressionScreenPosition` | `StaticClass()` and `IsA<>` directly | `FindObject<UClass>(nullptr, "/Script/Engine.MaterialExpressionScreenPosition")` |
| Workspace-service pin types | `GetInputValueType` | the deprecated `GetInputType`, wrapped in `PRAGMA_DISABLE_DEPRECATION_WARNINGS` |
| `Shader(Root="Plugin.X")` | additionally requires `Plugin->IsMounted()`; otherwise `DreamShader Root '{Root}' references project plugin '{Plugin}', but the plugin content is not mounted.` | mount check skipped |
| `Path(Plugin.X, "…")` asset roots | same mount check; otherwise `Asset Path root '{Root}' references plugin '{Plugin}', but the plugin content is not mounted.` | mount check skipped |
| `UE.TransformPosition` argument `FirstPersonInterpolationAlpha` | honoured | error: `UE.TransformPosition FirstPersonInterpolationAlpha requires Unreal Engine 5.6 or newer.` |
| Transform bases `firstperson` / `firstpersontranslatedworld` | resolve to `TRANSFORMPOSSOURCE_FirstPersonTranslatedWorld` | unsupported |
| Decompiler `TextureSample` export | `GatherMode` is round-tripped | omitted |
| Unexported engine expression classes — `SceneDepth`, `SceneColor`, `ObjectRadius`, `ObjectBounds`, `PerInstanceRandom`, `PerInstanceFadeAmount` | `StaticClass()` directly, via `DREAMSHADER_ENGINE_EXPRESSION_CLASS` | `FindObject<UClass>(nullptr, "/Script/Engine.MaterialExpression<Name>")`. The `UE.*` builtins behave the same either way; they are only dropped from the table if the lookup fails |

> [!NOTE]
> That last row is a **link**-time gate, not a compile-time one. UE 5.6 changed UHT to emit
> `DECLARE_CLASS2` with an exported `Z_Construct_<Class>_NoRegister`, so `StaticClass()` resolves
> from a plugin even for a `UCLASS()` that carries neither `MinimalAPI` nor `ENGINE_API`. UE 5.5 and
> earlier emit `DECLARE_CLASS(..., NO_API)`: `GetPrivateStaticClass` never leaves `Engine.dll`, and
> naming `StaticClass()` compiles on every engine and then fails with `LNK2019`. Only a full
> [`RunUAT BuildPlugin`](../contributing/index.md#synopsis) sees it — an editor build against one
> engine never will.

### UE ≥ 5.7

| Feature | On UE ≥ 5.7 | On UE 5.3 – 5.6 |
| :-- | :-- | :-- |
| Material-resource diagnostics in the bridge | iterate every `EShaderPlatform` × `EMaterialQualityLevel` combination | a different, narrower path |
| Material-parameter-collection expressions | a fresh `ExpressionGUID` is assigned when the existing one is invalid | skipped |
| `UE.CollectionParam` / `UE.CollectionParameter` | `Group` and `SortPriority` metadata are honoured | ignored |
| Layer-blend function inputs | `UMaterialExpressionFunctionInput::BlendInputRelevance` is computed | not set |
| Node preview height in the layout pass | `Expression->ShouldShowPreview()` | `!bHidePreviewWindow && !bCollapsed`, the two flags 5.7 composed it from |
| Decompiler scalar-parameter metadata | `ControlType`, `Enumeration` and `EnumerationIndex` are exported | not exported — the properties do not exist. A source that carries them still parses; there is nothing to write them to |
| `FMaterialParameterInfo` include | `Materials/MaterialParameters.h` | `MaterialTypes.h`, which 5.7 keeps only as a deprecation stub |

## "Since UE 5.x" summary

| Version | Features that require it |
| :-- | :-- |
| **5.4** | Substrate — `Substrate.*` builtins, `ShadingModel="Substrate"`, `Base.FrontMaterial`, `MSM_Strata` aliases · collapsed Custom-node code (`ShowCode`) · `bHasPixelAnimation` reset and export · `EAllowShrinking` |
| **5.5** | `periodicworld` transform basis · `UE.TransformPosition(PeriodicWorldTileSize=…)` · `ObjectPositionWS` resolved directly · `CountInputs` |
| **5.6** | `firstperson` / `firstpersontranslatedworld` transform bases · `UE.TransformPosition(FirstPersonInterpolationAlpha=…)` · plugin-mount validation for `Root=` and `Path(...)` · `TextureSample.GatherMode` round-trip · `FMetaData&`, `GetInputValueType`, `RebuildOutputs`, `ScreenPosition` resolved directly · six unexported expression classes resolved by `StaticClass()` rather than by path (no behaviour difference) |
| **5.7** | `Group` / `SortPriority` on collection parameters · `BlendInputRelevance` on layer-blend inputs · MPC `ExpressionGUID` repair · per-platform × per-quality material-resource diagnostics · scalar-parameter `ControlType` / `Enumeration` / `EnumerationIndex` round-trip · node preview height from `ShouldShowPreview()` |

Everything not listed above works identically on every engine from 5.3 to 5.8.

## Notes

- **The gate is compile-time, not run-time.** The plugin binary built against UE 5.3 does not
  contain the Substrate code paths at all. Moving a project to a newer engine requires rebuilding
  the plugin to gain the newer behaviours.
- Many "older branch" paths are **errors with an explicit version message** rather than silent
  degradation. Every Substrate surface reachable from source — `ShadingModel="Substrate"`, a
  `Substrate` output or input type on a `Shader`/`Function`/`GraphFunction`, a `Substrate.*` builtin
  call, `UE.<Name>(OutputType="Substrate")`, `Base.FrontMaterial`, a Graph variable holding a
  Substrate value — reports a message ending `requires Unreal Engine 5.4 or newer.` on UE 5.3, as
  does the `substrate-builtins.json` manifest's `unsupportedReason`. The only 5.6 message of this
  kind is `UE.TransformPosition`'s `FirstPersonInterpolationAlpha` argument. Rows without a quoted
  message in the tables above degrade silently — the feature simply is not offered.
- `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` is evaluated as a value, so it must be written
  `#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS`, not `#ifdef`. It is always defined.
- `DREAMSHADER_UE_VERSION_AT_LEAST` is defined unconditionally, so pre-defining it in a build target
  produces a macro redefinition. Override the three version-number macros instead.
- Because `DreamShaderModule.h` includes this header, any translation unit that uses the plugin's
  path helpers also has the macros available.

## Example

Guarding project code that consumes a version-gated DreamShader behaviour:

```cpp
#include "DreamShaderVersionCompat.h"

void ConfigureShadingModel(FString& OutSettingValue)
{
#if DREAMSHADER_WITH_SUBSTRATE_BUILTINS
    OutSettingValue = TEXT("Substrate");
#else
    OutSettingValue = TEXT("DefaultLit");
#endif
}

void TrimTrailing(TArray<int32>& InOut)
{
    if (InOut.Num() > 0)
    {
        InOut.RemoveAt(InOut.Num() - 1, 1, DREAMSHADER_ALLOW_SHRINKING_NO);
    }
}

#if DREAMSHADER_UE_VERSION_AT_LEAST(5, 6)
// Plugin-rooted asset paths are mount-validated here; a packaged-but-unmounted plugin is an error.
#endif
```

The corresponding DreamShaderLang source, written so it compiles on 5.3 as well:

```c
Shader(Name="Materials/M_Portable")
{
    Settings { ShadingModel = "DefaultLit"; }   // "Substrate" would require UE 5.4+
    Outputs  { vec3 Color; Base.BaseColor = Color; }
    Graph    { Color = UE.TransformVector(Input = UE.VertexNormalWS(), Source = "World", Destination = "Tangent"); }
}
```

## See also

- [C++ API](index.md) — modules, headers, and linkage
- [`DreamShaderModule.h`](dreamshader-module.md) — the header that includes this one everywhere
- [`DreamShaderSettings.h`](settings.md) — the `Strata` alias gate in the shading-model catalogue
- [`Substrate.*`](../builtins/substrate.md) — the builtin family behind `DREAMSHADER_WITH_SUBSTRATE_BUILTINS`
- [Transform builtins](../builtins/transform.md) — the version-gated basis names
- [`UE.*` catalogue](../builtins/ue.md#uecollectionparam) — the collection-parameter builtin gated at 5.7
- [Material enums](../settings/material-enums.md) — where `Substrate` appears as a shading model
- [`Path(...)`](../parameters/path.md) — plugin roots and the 5.6 mount check
- [Asset paths](../generation/asset-paths.md) — `Root="Plugin.X"` and the 5.6 mount check
- [Decompiler](../tools/decompiler.md) — the version-gated round-trip properties
- [Diagnostics index](../diagnostics/index.md) — the three explicit version-requirement errors
