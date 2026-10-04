# DreamShaderCompiler

> [DreamShader](../index.md) » [C++ API](index.md) » **DreamShaderCompiler**

The compiler's back half: the pipeline that drives a source from text to assets, the IR emitter, the
asset layer under it, the builtin catalog built from reflection, and the product index. The front
half — lexer to validated IR — is [`DreamShaderLang`](lang-module.md), which knows nothing about the
engine; this module is where a `UObject` is first touched.

| | |
| :-- | :-- |
| Module | `DreamShaderCompiler` (**Editor**, `Default` loading phase) |
| Public headers | 20 — the ones a caller meets are listed [below](#public-headers) |
| Namespace | `UE::DreamShader::Editor::Compiler` · `UE::DreamShader::Editor` (the generated-notice delegate) · global scope (module class) |
| Export macro | `DREAMSHADERCOMPILER_API` |
| Public dependencies | `Core`, `CoreUObject`, `DreamShader`, `DreamShaderLang`, `Engine` |
| Private dependencies | `AssetRegistry`, `AssetTools`, `DreamShaderPass`, `Json`, `MaterialEditor`, `Projects`, `RenderCore`, `RHI`, `TargetPlatform`, `UnrealEd` |
| Reflection | none |

> [!IMPORTANT]
> **Changed in 2.0.** Through 1.9.x this was a *Runtime* module holding nothing but the compile
> interface, and the compiler itself lived privately in `DreamShaderEditor`. The two swapped places:
> the interface moved **down** into the runtime module as
> [`DreamShaderCompilerInterface.h`](#the-interface-lives-in-dreamshader), and the compiler moved
> **out** of the editor module into this one. `DreamShaderCompilerInterfaces.h`,
> `FDreamShaderCompileService`, the editor's `FEditorCompileAdapter` and the 1.x `FMaterialGenerator`
> are gone.

## The interface lives in `DreamShader`

An Editor-type module is absent from game targets, so nothing outside the editor may depend on it at
build time. The request, the result and the interface therefore live in the **runtime** module, in a
header that needs nothing but `Core`:

```cpp
#include "DreamShaderCompilerInterface.h"   // module: DreamShader

namespace UE::DreamShader
{
    enum class EThinCustomPersistence : uint8 { Ephemeral, Materialized };

    struct DREAMSHADER_API FDreamShaderCompileRequest
    {
        FString SourceFilePath;
        bool bForce = false;
        EThinCustomPersistence ThinCustomPersistence = EThinCustomPersistence::Materialized;
    };

    struct DREAMSHADER_API FDreamShaderCompileResult
    {
        bool bSucceeded = false;
        FText Message;
        FString Code;
    };

    class DREAMSHADER_API IDreamShaderCompiler
    {
    public:
        virtual ~IDreamShaderCompiler() = default;
        virtual FDreamShaderCompileResult CompileAssets(const FDreamShaderCompileRequest& Request) = 0;
        virtual FDreamShaderCompileResult CompileMaterial(const FDreamShaderCompileRequest& Request) = 0;
    };

    DREAMSHADER_API IDreamShaderCompiler* GetDreamShaderCompiler();
    DREAMSHADER_API void RegisterDreamShaderCompiler(IDreamShaderCompiler* Compiler);
}
```

The namespace is `UE::DreamShader`, not `UE::DreamShader::Compiler`: inside
`UE::DreamShader::Editor` a bare `Compiler::` names `UE::DreamShader::Editor::Compiler`, so a nested
namespace here would be hidden from exactly the callers that use it most.

### `FDreamShaderCompileRequest`

| Member | Type | Default | Meaning |
| :-- | :-- | :-- | :-- |
| `SourceFilePath` | `FString` | `""` | A `.dss`, `.dsi`, `.dsp`, `.dsm` or `.dsf`; the compiler normalizes it, so a relative path is accepted. A `.dsh` header is not a compile unit. |
| `bForce` | `bool` | `false` | Rebuild even when a product's stamped build key says its asset is current. With `false`, such a product is left untouched and the call **succeeds** with a `Skipped …` line. |
| `ThinCustomPersistence` | `EThinCustomPersistence` | `Materialized` | Which state a ThinCustom product this compile touches should end in. **Ignored by the Graph, function and instance products**, which always save. |

> [!NOTE]
> `Ephemeral` is the editor's **normal** state for a ThinCustom product, not an exotic flag. The
> bridge and the preview renderer both ask for it; the commandlet, the cook path and the explicit
> *Materialize* action take the `Materialized` default, so a caller that does not name a state — a
> headless tool — writes assets to disk. See [Ephemeral materials](../generation/ephemeral.md).

### `FDreamShaderCompileResult`

| Member | Type | Meaning |
| :-- | :-- | :-- |
| `bSucceeded` | `bool` | Whether the compile completed. A skipped compile counts as success. |
| `Message` | `FText` | The report in the wire shape the bridge, the extensions and `dsc.ps1` parse — see [Result messages](#result-messages). A caller that writes it to a wire passes it through `ToInvariantWireString`. |
| `Code` | `FString` | The `DSHnnnn` code of the first error; empty on success. Carried beside `Message` so a caller that keys on codes does not parse it back out of the text. |

The structured diagnostics of a compile — every error, warning and info with its code, stage,
severity and span — are not in the result; ask
[`GetDreamShaderLastCompileDiagnostics`](#dreamshadercompilerserviceh) for them.

### `IDreamShaderCompiler`

| Method | Contract |
| :-- | :-- |
| `CompileAssets` | Every product of the source: its material and every exported function, layer and blend — or, for a `.dsi`, the one material instance (building a stale parent first); for a `.dsp` *(since 2.1.0)*, the pipeline with the render targets of its exported buffers and its HLSL slots (building any stale material its passes name first). |
| `CompileMaterial` | The source's material product only: the preview's route. |

### `GetDreamShaderCompiler()`

The registered compiler, **or null**. In an editor build the compiler module is loaded on demand when
nothing has registered yet — both modules are `Default`-phase, and start order within a phase is not
a contract. Null in a game target, where the module does not exist, and while the engine is shutting
down. **Every caller must handle null.** Game thread only whenever it may have to load the module.

`RegisterDreamShaderCompiler` is for the compiler module itself: its service from `StartupModule`,
`nullptr` from `ShutdownModule`. The registry does not own the object. It is exported, so another
module *can* register a replacement — there is no chaining and no arbitration, the last call wins.

## Public headers

| Header | What it declares |
| :-- | :-- |
| `DreamShaderCompilerService.h` | `FDreamShaderCompilerService` — the one implementation of `IDreamShaderCompiler`; `OnDreamShaderSourceGenerated`; `IsMemoryOnlyMaterial` / `MaterializeDreamShaderMaterial`; `GetDreamShaderLastCompileDiagnostics` |
| `DreamShaderCompilePipeline.h` | `IsDreamShaderLang2Source`, `CompileDreamShaderLang2File`, `RunDreamShaderLang2Pipeline` with its options and result, `ResolveDreamShaderSourceProducts`, `ResolveDreamShaderProductDestination`, `ResolveDreamShaderLegacyTextureTypes` |
| `DreamShaderIREmitter.h` | `FIREmitContext`, `EmitDreamShaderIRProduct` — one finished `IR::FIRProduct` in, one saved asset out; the source-span and decompile-hint writers |
| `DreamShaderBuiltinCatalog.h` | `GetDreamShaderBuiltinCatalog`, `InvalidateDreamShaderBuiltinCatalog`, `BuildBuiltinCatalogFromReflection` — the `IR::FBuiltinCatalog` the binder and the emitter share |
| `DreamShaderProductIndex.h` | `FDreamShaderProductIndex` — which source builds which asset, over the `.dss`, `.dsi` and `.dsp` files of every root; `ResolveInstanceParent`, `CollectInstanceDependents`, `FindInstanceParentSourceFile` for `.dsi` files; *(since 2.1.0)* the `.dsp` edges: `CollectPipelineDependents` (the `.dsp` sources to rebuild after a material source), `CollectPassBufferDependents` (the `.dss` sources that read a pipeline's buffers), `FindPipelineMaterialSourceFiles` and `FindPassBufferPipelineSourceFiles` (the dependency sort's two edges), and on the index `FindPipelines`, `FindPipelinesUsingSource`, `FindPassBufferReadersOfSource`. A `.dsp` record carries its `MaterialReferences` and `ShaderFiles`, a `.dss` record its `PassPipelineReferences` |
| `DreamShaderPassPipelines.h` *(since 2.1.0)* | the Custom Pass half as the editor's tools reach it: the HLSL slot registry, the slot pre-check, the shader files a `.dsp` compiles from — [below](#dreamshaderpasspipelinesh) |
| `DreamShaderInstanceSchema.h` · `DreamShaderInstanceSettings.h` | the parameter schema of a parent asset (`BuildParameterSchemaFromAsset`), and the `#pragma instance` keys (`ApplyInstanceSettings`, `ReadInstanceSettings`, `GetInstanceSettingKeys`) |
| `DreamShaderCompilerDiagnostics.h` · `DreamShaderDiagnosticRecord.h` | the wire line (`FormatLang2DiagnosticWireLine`), the diagnostics JSON, and the record the bridge's store files |
| `DreamShaderCompilerIncludes.h` | `FDreamShaderIncludeResolver` — `#include` resolution over the source roots and packages |
| `DreamShaderGeneratedAssets.h` · `DreamShaderGeneratedAssetDigest.h` | destination rules, source metadata, the ownership guard, the output digest behind [divergence detection](../generation/divergence.md) |
| `DreamShaderDependencyGraphService.h` · `DreamShaderSourceFileUtils.h` · `DreamShaderGraphDebugInfo.h` · `DreamShaderGenerationProgress.h` · `DreamShaderMaterialExpressionCompat.h` · `DreamShaderTextWireUtils.h` | what the editor module's tools share with the compiler: who includes whom, source enumeration, node ↔ source debug info, the progress heuristics (the shader-compile stall threshold, the Cancel seam), engine-version shims for expression APIs, culture-invariant wire strings |
| `DreamShaderCompilerModule.h` | `FDreamShaderCompilerModule`: registers the service on startup, unregisters it on shutdown |

Everything here is editor-only API. It is exported so `DreamShaderEditor` — the commandlet, the
bridge, the browser, the decompiler, the tests — can link it; a third-party editor module can too.

## `DreamShaderCompilerService.h`

```cpp
namespace UE::DreamShader::Editor
{
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDreamShaderSourceGenerated, const FString& /*SourceFilePath*/, bool /*bSucceeded*/);
    DREAMSHADERCOMPILER_API FOnDreamShaderSourceGenerated& OnDreamShaderSourceGenerated();
}

namespace UE::DreamShader::Editor::Compiler
{
    class DREAMSHADERCOMPILER_API FDreamShaderCompilerService final : public ::UE::DreamShader::IDreamShaderCompiler
    {
    public:
        static FDreamShaderCompilerService& Get();
        // CompileAssets / CompileMaterial
    };

    DREAMSHADERCOMPILER_API bool GetDreamShaderLastCompileDiagnostics(const FString& SourceFilePath, TArray<FLang2DiagnosticRecord>& OutRecords);
}
```

| | |
| :-- | :-- |
| The service | Picks the front end by extension, forwards the request's ThinCustom persistence to the pipeline, and words the result. Every compile route — the bridge's watcher, a commandlet, the Material Content Browser, a provenance action, a test — ends here. |
| `OnDreamShaderSourceGenerated` | Fired once per **outermost** compile of a source, after it succeeded or failed, with the normalized path. A compile nested inside another (a `.dsi` building its stale parent) does not fire. |
| `MaterializeDreamShaderMaterial` | Persists a memory-only ThinCustom product by compiling its source again with `bForce` and `Materialized`, then reloading it by object path. |
| `GetDreamShaderLastCompileDiagnostics` | Each source's most recent compile as structured records — code, stage, severity, span length — which the bridge files into its diagnostics store instead of re-parsing a result's text. |

## `DreamShaderCompilePipeline.h`

The pipeline is **preprocess → parse → bind → lower → passes → validate → emit**, and this header is
the same driver with its intermediate products handed back instead of dropped.

| Call | Use |
| :-- | :-- |
| `IsDreamShaderLang2Source(Path)` | True for every file the pipeline compiles on its own: `.dss`, `.dsi` and `.dsp` through the 2.0 front end, `.dsm` and `.dsf` through the legacy one. A `.dsh` answers false. (The name is older than that: once only `.dss` answered true.) |
| `CompileDreamShaderLang2File(Path, bForce, OutError)` | The service's `CompileAssets` with a `Materialized` request, spelled with an `FDreamShaderError` for callers that want the code and the text apart. |
| `RunDreamShaderLang2Pipeline(Path, Options, OutResult)` | The whole run. `Options.bEmitAssets = false` stops after IR validation — that is `dsc check`. The result owns the parsed module, the bound module and the IR, **in a load-bearing member order** (the bound module points into the parsed ones); it keeps whatever the run got as far as, so a language service gets an AST from a false return. *(Since 2.1.0)* it also owns, for a `.dsp`, the `PipelineReferences` the host resolved before the bind — declared before `Bound`, which may point into it — and records, for a `.dss`, the `PassPipelineReferences` of its `UE.DreamPassBuffer` nodes, as written. |
| `ResolveDreamShaderSourceProducts(Path, OutResult)` | Which assets a source builds, and under which build key, **without building them**: front end, binder and IR builder, then the emitter's own destination rules. Builds and saves nothing and opens no progress dialog — the Material Content Browser calls it for every source it lists. It does read the assets a source's cross-source references name, loading them when they are not loaded: a `.dsi`'s parent that no source builds and, *(since 2.1.0)*, the materials a `.dsp`'s passes name and the pipeline a `.dss` reads an exported buffer of. A `PassPipeline` product lists in `ExportTargetObjectPaths` the render target the emitter keeps beside the pipeline for each exported buffer, in buffer order — generated assets as much as the pipeline is, which is how `dsc list-generated` names them. |
| `ResolveDreamShaderProductDestination(Product, Path, …)` | Where one product would land if `Path` declared it. The decompiler asks this before it writes a file, to find out whether the text needs a `/// @name` to keep the asset where it is. |

## `DreamShaderPassPipelines.h`

*(since 2.1.0)* The Custom Pass half of the compiler as the editor's tools reach it: the HLSL slot registry
as [`dsc pass-registry`](../tools/commandlet.md#pass-registry) lists, collects and rebuilds it; the slot
pre-check as `check -Shaders` runs it on a `.dsp`; and the shader files a `.dsp` compiles from, which the
[bridge](../tools/bridge.md#custom-pass-shader-files) watches. Nothing here builds an asset. Game thread
only. Everything is in `UE::DreamShader::Editor::Compiler`.

| Declaration | |
| :-- | :-- |
| `EDreamPassSlotState` | `Live`, `Reserved`, `SnapshotMissing`, `PipelineGone`, `PassGone`, `Unknown` — what one slot is ([the listing](../tools/commandlet.md#the-listing)) |
| `LexDreamPassSlotState(State)` | the state's name. Deliberately not a `LexToString` overload: one declared in this namespace would hide the engine's from every unqualified call in it |
| `FDreamPassSlotReport` | one slot as `Registry.json` records it and as it was judged: `bCompute`, `Slot`, `Pipeline` (object path), `Pass`, `Source` (the `.dsp`, project-relative), `Shader` (project-relative), `Entry`, `Hash`, `Formats` (the shader formats its snapshot passed the pre-check for), `State`, and `bPipelineAssetExists` for a `PipelineGone` slot whose asset is still there |
| `FDreamPassRegistryReport` | `RegistryJsonPath`, `ComputeSlotCount`, `PixelSlotCount`, and `Slots` — compute slots first, then pixel ones, each in slot order |
| `DescribeDreamPassRegistry(bClassify, OutReport, OutError)` | reads `Registry.json` and judges every slot. With `bClassify` it runs the front end of every `.dsp` that owns a slot — nothing built or written — to tell `Live` from `PipelineGone`, `PassGone` and `Unknown`; without it a slot is `Live`, `Reserved` or `SnapshotMissing`. False, with `OutError`, when `Registry.json` does not parse |
| `CollectDreamPassRegistryGarbage(OutFreed, Diagnostics)` | `pass-registry -Gc`: frees every `PipelineGone` and `PassGone` slot — its entry, its registry section, its snapshot — deletes the slot directories nothing names, and recompiles the slot shaders in the editor. False with diagnostics when `Registry.json` does not parse or a file cannot be written |
| `ResetUnreadableDreamPassRegistry(OutMovedTo, Diagnostics)` | the first step of `-Rebuild`: moves a `Registry.json` that does not parse aside to `Registry.json.unreadable` and writes an empty registry. True without touching anything when the file parses; `OutMovedTo` is then empty |
| `RewriteDreamPassRegistryFiles(OutReserved, Diagnostics)` | the last step of `-Rebuild`: writes `RegistryCompute.ush` and `RegistryPixel.ush` from `Registry.json`, turning a slot whose snapshot files are missing into a reserved one, deletes the slot directories it does not name, and recompiles the slot shaders in the editor. `OutReserved` counts the slots it turned |
| `CheckDreamShaderPipelineSlots(SourceFile, Formats, Diagnostics, OutPassesChecked)` | `check -Shaders` on a `.dsp`: runs it to IR, stages each pipeline, plans its slots against the registry as it stands, and pre-checks every HLSL pass, changed or not, for `Formats` — empty: the formats a compile pre-checks for. Builds no asset and writes nothing. `OutPassesChecked` counts the HLSL passes. Below UE 5.8, `DSH8338` |
| `IsDreamShaderPassShaderFile(Path)` | `.usf` or `.ush`, case-insensitively |
| `CollectDreamShaderPipelineShaderFiles(PipelineSourceFile, OutShaderFiles)` | every file the HLSL passes of one `.dsp` compile from: each `Shader = "..."` file, everything it includes by a relative path — a file such an include names that does not exist yet as well — and the files behind the user virtual includes its snapshot keeps live. Absolute, normalized, each once |
| `FindDreamShaderPipelinesUsingShaderFile(ShaderFile, OutPipelineSourceFiles)` | the `.dsp` sources one of whose HLSL passes compiles from that file |
| `CollectDreamShaderPipelineShaderDirectories(OutDirectories)` | every directory holding a file some `.dsp` compiles from — what the bridge watches |

The registry itself — the files under `<DShader>/.dreampass/`, how a slot is planned, pre-checked and
committed — is private to the module; its paths and slot counts are public in the runtime module
([`DreamShaderPassModule.h`](pass-module.md#the-hlsl-slot-registry)).

## Thread and context requirements

- **Game thread, editor build only.** The emit half creates and modifies `UPackage`s, `UMaterial`s and
  `UMaterialInstanceConstant`s and runs the asset save path; binding reads the builtin catalog, which
  is built from reflection.
- Outside a commandlet a compile opens an `FScopedSlowTask` and shows a modal progress dialog after a
  short delay; Cancel is reported as `bCancelled`, distinct from a failure.
- The calls are **synchronous**. There is no async variant, no future, and no completion callback
  other than `OnDreamShaderSourceGenerated`.
- *(Since 2.1.0)* A `.dsp` compile writes outside its package as well: the HLSL slot registry and its
  snapshots under `<DShader>/.dreampass/`. In an editor that renders it then recompiles the slot shaders
  synchronously; a commandlet does not.

## Result messages

`FDreamShaderCompileResult::Message` keeps the 1.x wire shape on purpose: `.skill/dsc.ps1` greps the
`Generated` lines to list the assets a run wrote, and the bridge logs the message verbatim.

### Success

One line per product, in emit order, and after a built pipeline's line one per render target of its
exported buffers:

| Line | Condition |
| :-- | :-- |
| `Generated {Kind} {ObjectPath} from {File}.` | the product was built. `{Kind}` is `Material`, `MaterialFunction`, `MaterialLayer`, `MaterialLayerBlend`, `MaterialInstance` or *(since 2.1.0)* `PassPipeline` |
| `Generated RenderTarget {ObjectPath} from {File}.` | *(since 2.1.0)* the render target of one of a built pipeline's exported buffers, made or reused by that build and saved beside it, so that `dsc.ps1` counts it among the assets the run wrote |
| `Skipped {ObjectPath} from {File}; source hash is unchanged (build key {BuildKey}).` | `bForce == false` and the asset's stamped build key still matches |
| `Skipped {ObjectPath}; another editor owns this project's DreamShader bridge, and only that one writes generated assets to disk.` | a second editor on the same project |
| `Compiled {File}; it declares no material and no exported function, so no asset was written.` | a source with no product |
| `\nWarnings:\n` + one wire line per warning | appended when the run raised warnings: its own diagnostics, then what `RaiseGenerationWarning` collected |

### Failure

The first error as `<file>(<line>,<column>): DSHnnnn: <message>`, then every other diagnostic on its
own line. `Code` holds that first `DSHnnnn`. The codes are catalogued under
[Diagnostics](../diagnostics/index.md); the emitter's own are `DSH8200`–`DSH8289`, and Custom Pass
emission's — the pipeline, its render targets, the HLSL slots and their pre-check — `DSH8300`–`DSH8339`
*(since 2.1.0)*.

The build key behind the `Skipped` line covers the preprocessed text of the file **and of every
header**, the defines the preprocessor read, and — for a `.dsi` — the parent's object path; for a `.dsp`,
the materials its passes name with the build keys they were built under, the snapshot inputs of its shader
files, and the pass layer table; for a `.dss` that reads an exported buffer, that buffer's export. See
[Caching](../generation/caching.md#custom-pass-pipelines).

## Example

Requesting a compile without linking the compiler module — this is all a runtime or a third-party
editor module needs:

```csharp
// MyTooling.Build.cs
PrivateDependencyModuleNames.AddRange(new[] { "Core", "DreamShader" });
```

```cpp
#include "DreamShaderCompilerInterface.h"
#include "DreamShaderModule.h"

using namespace UE::DreamShader;

void CompileOne(const FString& InPath)
{
    IDreamShaderCompiler* Compiler = GetDreamShaderCompiler();
    if (!Compiler)
    {
        return; // a game target, or the engine is shutting down
    }

    FDreamShaderCompileRequest Request;
    Request.SourceFilePath = InPath;
    Request.ThinCustomPersistence = EThinCustomPersistence::Ephemeral;

    const FDreamShaderCompileResult Result = Compiler->CompileAssets(Request);
    UE_LOG(LogDreamShader, Display, TEXT("%s"), *Result.Message.ToString());
}
```

A typical successful message for a `.dss` that exports one function and one material:

```text
Generated MaterialFunction /Game/FX/MF_Tint.MF_Tint from I:/Project/DShader/FX/Glow.dss.
Generated Material /Game/FX/M_Glow.M_Glow from I:/Project/DShader/FX/Glow.dss.
```

Running the front half only, the way `dsc check` does:

```cpp
#include "DreamShaderCompilePipeline.h"      // module: DreamShaderCompiler (editor only)
#include "DreamShaderCompilerDiagnostics.h"  // FormatLang2DiagnosticWireLine

using namespace UE::DreamShader::Editor::Compiler;

bool Check(const FString& InPath)
{
    FDreamShaderLang2PipelineOptions Options;
    Options.bEmitAssets = false;

    FDreamShaderLang2PipelineResult Run;
    const bool bOk = RunDreamShaderLang2Pipeline(InPath, Options, Run);
    for (const UE::DreamShader::Lang::FLangDiagnostic& Diagnostic : Run.Diagnostics.GetDiagnostics())
    {
        UE_LOG(LogTemp, Display, TEXT("%s"), *FormatLang2DiagnosticWireLine(Diagnostic, Run.SourceFilePath));
    }
    return bOk;
}
```

## See also

- [C++ API](index.md) — modules, headers, linkage, and build dependencies
- [`DreamShaderLang`](lang-module.md) — the front end and the IR this module runs and emits
- [`DreamShaderModule.h`](dreamshader-module.md) — `NormalizeSourceFilePath` for building a request
- [Generation](../generation/index.md) — what an emitted asset looks like
- [Ephemeral materials](../generation/ephemeral.md) — what the two states mean in practice
- [Caching](../generation/caching.md) — the build key `bForce` bypasses
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the persisting caller
- [Editor bridge](../tools/bridge.md) — the memory-only caller and the JSON diagnostics
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
- [`DreamShaderPass`](pass-module.md) — the runtime module a `.dsp` compiles for
