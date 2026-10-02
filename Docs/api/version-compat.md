# DreamShaderVersionCompat.h

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderVersionCompat.h**

Twelve preprocessor macros that gate every engine-version-dependent behaviour in the plugin — and,
outside the header, one module definition: the Custom Pass gate,
[`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass). This page is the source of truth
for every *(since UE 5.x)* marker in the manual.

Defined in header `DreamShaderVersionCompat.h`.

| | |
| :-- | :-- |
| Module | `DreamShader` (Runtime), included by `DreamShader` and `DreamShaderEditor`. `DreamShaderLang` cannot include it — it depends on `Core` alone and has no engine version to gate on |
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
| `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS` *(since 2.0.0)* | `1` when the engine's `EMaterialParameterType` has a `ParameterCollection` enumerator, `0` otherwise | **yes** — `#ifndef` guarded (falls back to UE ≥ 5.8), and `DreamShader.Build.cs` sets it as a `PublicDefinition` from that probe | Whether a material instance can override a parameter collection. Asked of the header that declares the enum, because a `case` label cannot ask the type. |

The six `#ifndef`-guarded ones may be pre-defined by a build target — for example to compile the
Substrate paths out on a 5.4+ engine by defining `DREAMSHADER_WITH_SUBSTRATE_BUILTINS=0`, or to force
a version branch when testing. `DreamShader.Build.cs` uses that to set
`DREAMSHADER_WITH_MOON_ENGINE` and `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS` from their
header probes. The plugin's only other definitions are `DREAMSHADER_WITH_CUSTOM_PASS`, a
`PublicDefinitions` entry of `DreamShaderPass.Build.cs` that this header does not define
([below](#dreamshader_with_custom_pass)), and `MOON_ENGINE=1`, a `PrivateDefinitions` entry that
`DreamShaderEditor.Build.cs` adds when the engine's `SceneTypes.h` declares
`MP_MoonEncodedAttribute0` and that no source file reads.

> [!NOTE]
> `DREAMSHADER_ALLOW_SHRINKING_NO` carries **no behavioural difference**. It exists solely because
> UE 5.4 changed the trailing `bool bAllowShrinking` parameter of `TArray::RemoveAt`, `TArray::Pop`,
> `FString::RightChopInline` and `FString::LeftChopInline` into an `EAllowShrinking` enum. Both
> spellings mean "do not shrink the allocation". It is used 37 times across the plugin and is not a
> tuning knob.

> [!NOTE]
> There are **no** raw `#if ENGINE_MAJOR_VERSION`, `ENGINE_MINOR_VERSION` or `UE_VERSION_NEWER_THAN`
> tests anywhere in the plugin outside this header. The one version test outside it is not C++:
> `DreamShaderPass.Build.cs` reads `Target.Version` to set
> [`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass). Every version-dependent behaviour
> goes through these macros or that definition, which is what makes the table below exhaustive.

> [!NOTE]
> *(since 2.0.0)* Where a **member** is younger than the oldest supported engine and nobody can say in which
> release it arrived, the code asks the type instead of a version: `if constexpr (requires { … })` inside a small
> template, with a `static_assert` of the same question on the engine where the member is known to exist, so a
> misspelling cannot read as "this engine does not have it". Those sites are listed under
> [Asked of the type](#asked-of-the-type).

## `DREAMSHADER_WITH_CUSTOM_PASS`

*(since 2.1.0)* The Custom Pass gate is not in this header. `DreamShaderPass.Build.cs` computes it
from the engine being built against and adds it to the module's `PublicDefinitions`:

```csharp
bool bForcedOff = Target.ProjectDefinitions.Any(Definition =>
    Definition == "DREAMSHADER_FORCE_NO_CUSTOM_PASS" || Definition == "DREAMSHADER_FORCE_NO_CUSTOM_PASS=1");
bool bWithCustomPass = !bForcedOff
    && (Target.Version.MajorVersion > 5 || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 8));
PublicDefinitions.Add("DREAMSHADER_WITH_CUSTOM_PASS=" + (bWithCustomPass ? "1" : "0"));
```

| | |
| :-- | :-- |
| Defined by | `DreamShaderPass.Build.cs` — **not** `DreamShaderVersionCompat.h`, which neither defines nor mentions it |
| Value | `1` on UE ≥ 5.8, `0` below or when forced off. Always defined where it is visible, so test it with `#if`, not `#ifdef` |
| Visible in | `DreamShaderPass` and the modules that depend on it: in the plugin, `DreamShaderCompiler` and `DreamShaderEditor`, which ask it rather than test the version again. `DreamShaderLang` and `DreamShader` do not depend on `DreamShaderPass` and do not have it |
| Overridable | down only, from the UBT command line: `-ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS` builds `0` on any engine, which is how a 5.8 machine compiles and tests the other side ([Testing](../contributing/testing.md#both-sides-of-the-custom-pass-gate)). Nothing makes it `1` below 5.8, and code cannot pre-empt it: the rules file sets it with no `#ifndef` guard |
| Also decides | the `Renderer/Internal` include path of `DreamShaderPass` (for `FPostProcessingInputs`), added at `1` only |

The module builds on every engine. Every reflected type in it is plain data, so pipelines, the
settings, the components and the volume load and save on any supported engine; below 5.8 nothing
renders them. What `0` compiles out, module by module, is under
[UE ≥ 5.8 — `DREAMSHADER_WITH_CUSTOM_PASS`](#ue--58--dreamshader_with_custom_pass).

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
| Reading one input pin | `Expression->GetInput(Index)` | `Expression->GetInput(Index)` — identical. `GetInputsView()` would serve too, but it is deprecated in favour of `FExpressionInputIterator` / `GetInput()` and emits C4996 on UE 5.8, so the array view is never built just to reach a single pin |
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

### Asked of the type

*(since 2.0.0)* No version number here: each row is decided by whether the engine's own type has the member --
or, *(since 2.0.1)*, whether the engine has the node's header at all (`__has_include`).

| Feature | Where the member exists | Where it does not |
| :-- | :-- | :-- |
| Reading a **Convert** node (Make / Break FloatN) back as the channels each output is put together from — `Materials/MaterialExpressionConvert.h`, the node of the 5.6 material editor *(since 2.0.1)* | written as swizzles and constructors | no graph can hold one |
| Reading a **Switch** node back as its branches — `Materials/MaterialExpressionSwitch.h` *(since 2.0.1)* | written as a chain of `==` branches, or as the one input an unwired `SwitchValue` picks | no graph can hold one |
| Re-applying a kept **double vector** override to a ThinCustom instance — `UMaterialInstanceConstant::SetDoubleVectorParameterValueEditorOnly` (absent through 5.6) | set | reported by name as a kind this build cannot express, like a parameter the source dropped |
| Re-applying a kept **static component mask** override — `SetStaticComponentMaskParameterValueEditorOnly` on the instance (absent through 5.6) | set | reported the same way |
| Reading an instance's usage-flags override — `FMaterialInstanceBasePropertyOverrides::bOverride_UsageFlags` (absent through 5.6) | named as unsupported by the instance decompiler when set | there is no such override to report |
| `TextureSample.GatherMode` when the graph importer decides whether a sample is the plain one (absent in 5.5) | compared with the default | nothing to compare |

### `DREAMSHADER_WITH_PARAMETER_COLLECTION_PARAMETERS`

*(since 2.0.0)*

| Feature | Where `EMaterialParameterType::ParameterCollection` exists | Where it does not (5.6 and earlier) |
| :-- | :-- | :-- |
| A `.dsi` override of a parameter-collection parameter | bound against the parent's schema and written to the instance | the kind is not in the parent's schema, so the override is refused as unknown; building one by hand answers `this engine has no parameter collection parameters.` |
| The instance decompiler | writes the override | never meets one |

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

### UE ≥ 5.8

| Feature | On UE ≥ 5.8 | On UE 5.3 – 5.7 |
| :-- | :-- | :-- |
| The builtin catalog's material attributes *(since 2.0.0)* | `FMaterialAttributeDefinitionMap::GetAttributeNameToIDList` | that list is private, so the attributes are reached by walking `EMaterialProperty` through the public `GetID` / `GetProperty` / `GetAttributeName`. The same attributes either way; only their order in the exported catalog differs |

### UE ≥ 5.8 — `DREAMSHADER_WITH_CUSTOM_PASS`

*(since 2.1.0)* Custom Pass as a whole. The language does not change: a `.dsp` parses and binds on
every engine, and so does a `.dss` — what changes is what the engine side can build and run.

| Module | Feature | On UE ≥ 5.8 | On UE 5.3 – 5.7 |
| :-- | :-- | :-- | :-- |
| `DreamShaderPass` | Module startup | maps `/DreamPassUser` to the project source root's `.dreampass/` folder, creating `Slots/` and the two registry `.ush` files when missing and dropping registry sections whose snapshot is missing; the mapping is skipped in a cooked build | logs `DreamShaderPass: Custom Pass needs Unreal Engine 5.8 or later; this build has the asset types only.` and does nothing else |
| `DreamShaderPass` | The renderer half — everything under `Private/Render/` | compiled in: the scene view extension (one per world, made by `UDreamPassSubsystem`), the slot global shaders `FDreamPassCS` / `FDreamPassPS`, the mesh pass shaders, buffers, exports, the buffer visualization and the `stat DreamPass` group | compiled out. The subsystem still keeps pipelines, activations, lists and layers, but no view extension exists, so no pass runs |
| `DreamShaderPass` | `DreamPass.Dump` | ends each world with the last frame's report: views with passes, passes run, passes skipped | the same listing without the frame report |
| `DreamShaderPass` | `r.DreamPass.Enable`, `r.DreamPass.DisablePipelines`, `r.DreamPass.Visualize` | registered | registered; with nothing rendering, there is nothing for them to change |
| `DreamShaderPass` | `UMaterialExpressionDreamPassBuffer` / `…Output` (`UE.DreamPassBuffer`, `UE.DreamPassOutput`) | compile as documented | the classes exist — UHT cannot gate a `UCLASS` — but `Compile` returns `Dream Pass Buffer needs Unreal Engine 5.8 or later.` / `Dream Pass Output needs Unreal Engine 5.8 or later.`, and most of their overrides — pin value types, the referenced texture, the shader tag, `IsAllowedIn` — are compiled out |
| `DreamShaderCompiler` | The builtin catalog | lists both nodes | leaves both out, so a `.dss` that calls one fails when it is bound: `DSH5300` `'UE.{Name}' needs Unreal Engine 5.8 or later and DreamShader's Custom Pass module (DreamShaderPass), and this engine's node catalog does not have it.` |
| `DreamShaderCompiler` | Building a `.dsp` | the pipeline and its export render targets are written | `DSH8300` `'{Name}' is a Custom Pass pipeline, which needs Unreal Engine 5.8 or later; this engine has the DreamShaderPass asset types but no runtime to run them, so nothing was built.` |
| `DreamShaderCompiler` | What a pass's material is checked against | domain, blendable location, `UserSceneTexture` inputs, `UE.DreamPassOutput` pins, usage flags, pre-exposure and translator settings | domain and blendable location only (`FPipelineReferences::bCustomPassAvailable` is false), with the info `DSH7360` saying the rest was not checked; the [build key](../generation/caching.md#custom-pass-pipelines) carries `CustomPass=0` |
| `DreamShaderCompiler` | HLSL slot pre-check | compiles each changed slot for the shader formats of the active feature levels and of the target platforms | `DSH8323` `HLSL slots need Unreal Engine 5.8 or later; this engine has no Custom Pass runtime to compile them for.` |
| `DreamShaderCompiler` | `dsc check -Shaders` on a `.dsp` | pre-checks its slots | `DSH8338` `HLSL slots need Unreal Engine 5.8 or later; this engine has no Custom Pass runtime to pre-check them for.` |
| `DreamShaderCompiler` | Hot reload of the slot shaders after a registry commit | in the editor, never in a commandlet | none |
| `DreamShaderEditor` | [Pipeline details panel](../tools/editor-integration.md#pass-pipeline-details-panel) | as documented | one more row in Pipeline Overview: `Custom Pass runs on Unreal Engine 5.8 and later. This engine loads and saves the pipeline, and runs none of it.` |
| `DreamShaderEditor` | [`pass-keys.json`](../tools/workspace.md#pass-keysjson) | `supported: true` | `supported: false` and `unsupportedReason: "Custom Pass pipelines run on Unreal Engine 5.8 and later."`; the keys, injection points and formats are written either way |
| `DreamShaderEditor` | The render automation tests (`DreamShaderPassRenderTests.cpp`) | compiled in | compiled out |

## "Since UE 5.x" summary

| Version | Features that require it |
| :-- | :-- |
| **5.4** | Substrate — `Substrate.*` builtins, `ShadingModel="Substrate"`, `Base.FrontMaterial`, `MSM_Strata` aliases · collapsed Custom-node code (`ShowCode`) · `bHasPixelAnimation` reset and export · `EAllowShrinking` |
| **5.5** | `periodicworld` transform basis · `UE.TransformPosition(PeriodicWorldTileSize=…)` · `ObjectPositionWS` resolved directly · `CountInputs` |
| **5.6** | `firstperson` / `firstpersontranslatedworld` transform bases · `UE.TransformPosition(FirstPersonInterpolationAlpha=…)` · plugin-mount validation for `Root=` and `Path(...)` · `TextureSample.GatherMode` round-trip · `FMetaData&`, `GetInputValueType`, `RebuildOutputs`, `ScreenPosition` resolved directly · six unexported expression classes resolved by `StaticClass()` rather than by path (no behaviour difference) |
| **5.7** | `Group` / `SortPriority` on collection parameters · `BlendInputRelevance` on layer-blend inputs · MPC `ExpressionGUID` repair · per-platform × per-quality material-resource diagnostics · scalar-parameter `ControlType` / `Enumeration` / `EnumerationIndex` round-trip · node preview height from `ShouldShowPreview()` |
| **5.8** | Custom Pass *(since 2.1.0)* — building and running `.dsp` pipelines, HLSL slots, the `UE.DreamPassBuffer` / `UE.DreamPassOutput` nodes ([`DREAMSHADER_WITH_CUSTOM_PASS`](#dreamshader_with_custom_pass)) |

Everything not listed above works identically on every engine from 5.3 to 5.8. A `.dsp` is the one
source that parses and binds everywhere and builds on 5.8 only.

## Notes

- **The gate is compile-time, not run-time.** The plugin binary built against UE 5.3 does not
  contain the Substrate code paths at all. Moving a project to a newer engine requires rebuilding
  the plugin to gain the newer behaviours.
- **`GetInputsView()` survives in exactly one place.** Epic deprecated it in favour of
  `FExpressionInputIterator` and `UMaterialExpression::GetInput()`, and naming it produces C4996 on
  UE 5.8 — a warning today, a compile error in whichever release removes it. The remaining call is
  the UE 5.3 – 5.4 branch of `GetDreamShaderExpressionInputCount`, where `CountInputs()` does not
  exist yet and the deprecation is not in force; every other site asks for a pin by index.
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
- [`DreamShaderPass`](pass-module.md) — the module whose rules file defines `DREAMSHADER_WITH_CUSTOM_PASS`
- [Custom Pass runtime](../runtime/index.md) — what runs on 5.8
- [Diagnostics index](../diagnostics/index.md) — the three explicit version-requirement errors
