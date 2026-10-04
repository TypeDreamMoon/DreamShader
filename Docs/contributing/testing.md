# Testing

> [DreamShader](../index.md) » [Contributing](index.md) » **Testing**

The plugin's Unreal automation suite, and the on-disk fixture corpus that thirteen of its tests
enumerate at run time.

| | |
| :-- | :-- |
| Declared in | `Source/DreamShaderEditor/Private/Tests/` — 37 translation units, all inside `#if WITH_DEV_AUTOMATION_TESTS` |
| Kind | Unreal automation tests |
| Flags | `EAutomationTestFlags::EditorContext \| EAutomationTestFlags::EngineFilter` on every declaration; the Custom Pass render, lifecycle and slot-registry tests add `NonNullRHI` |
| Corpus root | `<Plugin>/Tests/Corpus` |
| Editor UI | *Tools ▸ Session Frontend ▸ Automation*, filtered on a test-name prefix |

## Synopsis

```powershell
& "<EngineDir>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" --% "<ProjectDir>\<Project>.uproject" -ExecCmds="Automation RunTests <filter>; Quit" -nullrhi -unattended -nopause -nosplash -log
```

`<filter>` is any prefix of a test name; `DreamShader` runs everything. `--%` is PowerShell's
stop-parsing token: everything after it is handed to the executable verbatim, so the quotes around
`-ExecCmds=` survive. The same line without `--%` is what you paste into `cmd.exe`.

Optional switches: `-nullrhi`, `-DreamShaderUpdateGolden`, `-NoDreamShaderEditorBridge`. See
[Command-line switches](#command-line-switches).

## Test counts

| Quantity | Value |
| :-- | :-- |
| Test declarations (`IMPLEMENT_*_AUTOMATION_TEST`) | 277 |
| Simple declarations (one test each) | 264 |
| Complex declarations (data-driven runners) | 13 — see [Corpus runners](#corpus-runners--13-declarations) |
| Corpus fixtures | 513 sources under `Tests/Corpus/` |
| `.expected.json` goldens | 508 |
| Individually runnable tests | **945** in a `DreamShader` run (2026-09-21) |

The complex runners enumerate the corpus tree at run time, so the runnable-test count moves with the
fixture count and **no C++ changes when a fixture is added**. `DreamShader.Lang2.RoundtripIR` sweeps
fixtures that belong to other layers, which is why the runnable count is not "simple + fixtures".

## Test groups

| Prefix | Layer | Requirements |
| :-- | :-- | :-- |
| `DreamShader.Lang.Parse.*` | Data-driven parse corpus | fast; no asset I/O |
| `DreamShader.Lang.Diagnostics.*` | Pure diagnostic helpers | milliseconds; no editor state |
| `DreamShader.Lang.Import.*` | Pure import-specifier helpers | milliseconds; no editor state |
| `DreamShader.Lang.ParameterExpressions.*` | The 1.x `Properties` surface, through the legacy front end | fast; parser only |
| `DreamShader.Commandlet.Args.*` | Commandlet argument parsing | milliseconds; pure |
| `DreamShader.Commandlet.Compile.*` | Commandlet runner smoke test | slow; editor; writes a real `/Game` asset |
| `DreamShader.Compiler.Parser.*` | 1.x sources through the compiler, end to end | slow; editor |
| `DreamShader.Compiler.Generate.*` | 1.x sources through the compiler, end to end | slow; editor |
| `DreamShader.Compiler.SourceHash.*` | The regeneration skip check | slow; editor |
| `DreamShader.Lang2.{Lexer,Expressions,Statements,Declarations,Printer,Trivia}.*` | The 2.0 front end, unit by unit | milliseconds; `Core` only |
| `DreamShader.Lang2.{Binder,IR,IRCompare,InstanceSource,Migrate,LegacyRules}.*` | Binder, IR, the comparator, `.dsi` text, the migrator | fast; hand-made catalog, no assets |
| `DreamShader.Lang2.{IRLayout,SubstrateSugar,Format}.*` | The three IR graph layouts, the Substrate sugar (each spelling against the nodes it stands for, and read back by the decompiler), `dsc fmt`'s core | fast; hand-made catalog, no assets |
| `DreamShader.Lang2.Corpus*.*` | Data-driven text layers: `Lang/`, `IR/`, `Legacy/Parse`, `Legacy/IR`, `Decompile/`, `Migrate/` | fast; no asset I/O |
| `DreamShader.Lang2.RoundtripIR.*` | Every fixture that lowers: IR → text → IR is equivalent | fast; no asset I/O |
| `DreamShader.Compiler2.{Smoke,Instance,Decompile,Migrate,Provenance,Parity}.*` | The 2.0 pipeline, instances, the decompile service, `dsc migrate`, Adopt, graph parity against 1.x captures | slow; editor |
| `DreamShader.Compiler2.Corpus*.*` | Data-driven compile layers: `Compile/`, `Legacy/Compile`, `Instance/` | slow; editor |
| `DreamShader.Compiler2.Roundtrip.*` | `.dss` → asset → decompile → asset, both dumps equal | slow; editor |
| `DreamShader.Compiler.{Divergence,Atomic,Tweaked,Persistence,BuildKey,Order}.*` | Hand-edit detection, atomic rebuild, Ephemeral/Materialized, the build key | slow; editor |
| `DreamShader.Browser.*` · `DreamShader.AssetRenameSync.*` · `DreamShader.Preview.*` | Material Content Browser model, asset-rename rewriting, preview probes (`Preview.ProbePreview.RendersBinding` needs a real RHI) | mixed |
| `DreamShader.Gen.Graph.*` | Node-shape assertions on the generated graph | slow; editor |
| `DreamShader.Gen.Parameters.*` | Parameter-node creation and pin wiring | slow; editor |
| `DreamShader.Gen.Wiring.*` | Condition wiring in the generated graph | slow; editor |
| `DreamShader.Gen.Layout.*` | Geometry of the placed graph | slow; editor |
| `DreamShader.DumpGraph.*` *(since 1.9.0)* | The canonical graph dump — see [Graph baseline](#graph-baseline-since-190) | slow; editor |
| `DreamShader.Roundtrip.*` | Decompile → regenerate fidelity | slow; editor; two of them also need a real RHI |
| `DreamShader.Render.*` | Pixel parity | needs a real RHI |
| `DreamShader.Lang2.Pipeline.*` | `.dsp`: parse, print, `fmt`, bind, every rule's code, compare, Adopt's rewrite, the pass nodes in a `.dss` | fast; `Core` only, hand-made engine facts |
| `DreamShader.Lang2.Corpus.Pipeline.*` · `DreamShader.Lang2.CorpusIR.Pipeline.*` | The `.dsp` fixtures of `Tests/Corpus/Lang/Pipeline` and `Tests/Corpus/IR/Pipeline` | fast; no asset I/O |
| `DreamShader.Compiler2.Pipeline.*` | `.dsp` to a `UDreamPassPipeline`: fields, render targets, dependencies, round trip; the slot registry and its pre-check | slow; editor; `Registry` and `RegistryPrecheck` need a real RHI and back up `<DShader>/.dreampass` first |
| `DreamShader.Pass.Logic.*` | The Custom Pass runtime without rendering: spellings, parameter blending, slot packing, the asset's checks, which pipelines apply to a view, sources | fast |
| `DreamShader.Pass.Render.*` · `DreamShader.Pass.Lifecycle.*` | Custom Pass passes rendered into a scene capture and read back pixel by pixel; edits, removals and world teardown mid-frame | `NonNullRHI`: skipped under `-nullrhi`, run in the RHI gate below |

The split the source records: the fast `DreamShader.Lang.*` and `DreamShader.Lang2.*` layers gate
pull requests, the slow `DreamShader.Compiler*` and `DreamShader.Gen.*` layers run nightly. The whole
suite takes about a minute and a half headlessly.

> [!WARNING]
> **Close the editor first.** A run compiles sources and writes assets under `/Game/DreamShaderTests`;
> with an editor open on the same project the two fight over the bridge's write ownership and over
> package files.

## Running headlessly

| Goal | Filter |
| :-- | :-- |
| Fast gate — 1.x parse-equivalence corpus, pure helpers, the preprocessor | `DreamShader.Lang.` |
| Fast gate — the 2.0 front end, binder, IR, decompile and migrate text layers | `DreamShader.Lang2` |
| Commandlet argument helpers only | `DreamShader.Commandlet.Args` |
| 1.x sources compiled end to end (the former Generate corpus) | `DreamShader.Compiler2.CorpusLegacy` |
| The 2.0 pipeline end to end | `DreamShader.Compiler2` |
| 1.x-era end-to-end tests | `DreamShader.Compiler.` |
| Decompile round trips | `DreamShader.Roundtrip` |
| Graph dump determinism | `DreamShader.DumpGraph` |
| Custom Pass, no renderer | `DreamShader.Lang2.Pipeline+DreamShader.Compiler2.Pipeline+DreamShader.Pass.Logic` |
| Custom Pass, rendered (UE 5.8, real RHI — see below) | `DreamShader.Pass.Render+DreamShader.Pass.Lifecycle+DreamShader.Compiler2.Pipeline.Registry` |
| Everything | `DreamShader` |

The Custom Pass render tests need a rendering device but no window: run them as a gate of their own,
without `-nullrhi`, with `-RenderOffscreen -d3d12`:

```powershell
& "<EngineDir>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" --% "<ProjectDir>\<Project>.uproject" -ExecCmds="Automation RunTests DreamShader.Pass.Render+DreamShader.Pass.Lifecycle; Quit" -RenderOffscreen -d3d12 -unattended -nopause -nosplash -NoDreamShaderEditorBridge -log
```

`Tools/Tests/Invoke-DreamShaderTests.ps1` runs both gates on the standalone host project
(`Tools/TestHost`): the `Suite` preset under `-nullrhi`, the `Rhi` preset offscreen.

### Both sides of the Custom Pass gate

Below UE 5.8, [`DREAMSHADER_WITH_CUSTOM_PASS`](../api/version-compat.md#dreamshader_with_custom_pass)
is `0` and the plugin builds without the renderer half of Custom Pass. A 5.8 machine builds that side
too when UBT is given `-ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS`, which the test script's
`-WithoutCustomPass` passes:

```powershell
pwsh -NoProfile -File Tools\Tests\Invoke-DreamShaderTests.ps1 -Preset Suite -WithoutCustomPass -Filter DreamShader.Lang2.Pipeline+DreamShader.Compiler2.Pipeline+DreamShader.Pass.Logic
```

At `0` the pipeline tests that build assets are compiled out and
`DreamShader.Compiler2.Pipeline.NeedsCustomPass` is compiled in: a `.dsp` build reports `DSH8300` and
writes nothing. The render and lifecycle tests are compiled out with the renderer.

The next build without the switch puts the renderer back by itself: a changed UBT command line
invalidates UBT's makefile, so the rules are evaluated again. An environment variable read by a
rules file would not be seen — UBT keeps the rules it evaluated last until a rules file changes, and
on 5.8 it loads its makefile even under `-NoUBTMakefiles`. Before running, the script reads which side
the binaries are on from the definitions UBT wrote for `DreamShaderPass`, and stops when it is not the
one asked for, as after `-NoBuild` following a build of the other kind.

### Command-line switches

| Switch | Effect on the suite |
| :-- | :-- |
| `-nullrhi` | No rendering device. `DreamShader.Render.ThinCustomVsGraphParity` and `DreamShader.Roundtrip.MTestToonRenderParity` self-skip; the `NonNullRHI` tests (Custom Pass render, lifecycle, slot registry) are not run at all. Every other test still runs. |
| `-DreamShaderUpdateGolden` | Every corpus runner **rewrites** each `.expected.json` from the actual result instead of asserting it. See [Regenerating goldens](#regenerating-goldens). |
| `-NoDreamShaderEditorBridge` | Skips creating the editor bridge and the Material Content Browser: no directory watcher, no Ephemeral generation pass at startup, no WebSocket listener on `127.0.0.1:17864`. Useful when a run must not compete with the bridge for the same sources. |
| `-unattended -nopause -nosplash` | Standard headless flags; no modal dialogs, no splash, no keypress on exit. |
| `-log` / `-stdout` | Route the log to the console. |

> [!WARNING]
> **A skipped test reports success.** Every skip path in the suite calls `AddInfo(...)` and then
> `return true`. A `-nullrhi` run therefore shows `DreamShader.Render.ThinCustomVsGraphParity` and
> `DreamShader.Roundtrip.MTestToonRenderParity` green **without having compared a single pixel**, and
> a run on UE 5.3 shows `DreamShader.Compiler.Generate.SubstrateMaterial` green without having
> compiled a Substrate material. To prove render parity, run without `-nullrhi` and read the info
> lines in the log.

### Skip conditions

Every condition under which a test returns success without asserting anything.

| Test | Condition | Message |
| :-- | :-- | :-- |
| `DreamShader.Compiler.Generate.SubstrateMaterial` | `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` is 0 — UE < 5.4 | `DreamShader Substrate builtins are not available for this Unreal Engine version; skipping the compile test.` |
| `DreamShader.Render.ThinCustomVsGraphParity` | `GUsingNullRHI` or `!FApp::CanEverRender()` | `Skipping ThinCustom-vs-Graph render parity: no usable RHI (-nullrhi). Run without -nullrhi for the full pixel comparison.` |
| `DreamShader.Roundtrip.MTestToonRenderParity` | `GUsingNullRHI` or `!FApp::CanEverRender()` | `Skipping M_Test_Toon round-trip render parity: no usable RHI (run without -nullrhi).` |
| `DreamShader.Roundtrip.MTestToonRenderParity` | the project asset it round-trips is absent | `Skipping M_Test_Toon round-trip: '{ObjectPath}' is not present in this project.` |

Runtime substitutions are shown as `{Placeholder}` in every message table on this page.

## Shared harness

The editor-level tests all use the same scaffolding.

| Helper | Behaviour |
| :-- | :-- |
| `MakeUniqueTestAssetName(Prefix)` | `<Prefix>_<GUID digits>` — no two runs collide. |
| `GetAutomationSourceDirectory()` | `<SourceDirectory>/Tests/Automation`, i.e. `DShader/Tests/Automation` by default. |
| `WriteAutomationSourceFile(...)` | Writes the test's `.dsm` / `.dsf` there as UTF-8 without BOM. Failure message: `Failed to write DreamShader automation source file '{Path}'.` |
| `MakeAutomationObjectPath(Name)` | `/Game/DreamShaderTests/Automation/<Name>.<Name>` |
| `FScopedDreamShaderAutomationArtifacts` | RAII cleanup: deletes every registered asset with `ObjectTools::DeleteObjectsUnchecked`, then every registered source file. |
| `AddExpectedNewAssetProbeWarnings(...)` | Suppresses the `SkipPackage: <pkg>` and object-path probe warnings, registered with occurrence count `-1` (suppress if present, do not require) because newer engines do not always emit them. |
| `AddExpectedAutomationCleanupWarnings(...)` | Suppresses `package was marked as deleted in editor, but has been modified on disk`. |
| `FDreamShaderQuietAutomationTestBase` | Base whose `SuppressLogErrors()` / `SuppressLogWarnings()` return `true`; needed because graph auto-layout trips a benign engine `SlowTask` ensure. |
| `FScopedDreamShaderGraphBackendPin` | RAII: forces `UDreamShaderSettings::DefaultBackend = Graph` for the scope and restores the previous value. |

> [!NOTE]
> The `Compile/` corpus goldens encode **Graph-backend** semantics, so that runner pins
> `DefaultBackend = Graph` for the duration of each case. `Legacy/Compile/` is the exception: its
> goldens were seeded from the 1.x captures of 2026-09-15 and pin **ThinCustom**, the project default.

## Test index

Every declaration, by translation unit.

### `DreamShaderAutomationTests.cpp` — 30 declarations

Unless the notes say otherwise, each of these writes a source file into `DShader/Tests/Automation`,
generates from it, asserts against the generated asset, and deletes both on the way out.

| Test | Macro | Notes |
| :-- | :-- | :-- |
| `DreamShader.Compiler.Parser.MinimalMaterial` | SIMPLE | — |
| `DreamShader.Compiler.Generate.MinimalMaterial` | SIMPLE | — |
| `DreamShader.Compiler.Generate.DsfWithImport` | SIMPLE | — |
| `DreamShader.Compiler.Generate.SubstrateMaterial` | SIMPLE | Skips below UE 5.4 |
| `DreamShader.Compiler.SourceHash.SkipUnchangedMaterial` | SIMPLE | Asserts the second compile of an unchanged source is skipped |
| `DreamShader.Commandlet.Compile.SingleSourceSmoke` | SIMPLE | Runs the commandlet compile path, which persists a real `/Game` asset |
| `DreamShader.Gen.Wiring.TruthyCondition` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Roundtrip.MaterialDecompiles` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Roundtrip.SubstrateMaterialRegenerates` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Roundtrip.SwitchTypedAppendRegenerates` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Roundtrip.CustomAdditionalOutputRegenerates` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Roundtrip.StaticBoolFunctionInputRegenerates` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Gen.Graph.ArithmeticNodes` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Gen.Graph.MathBuiltinNodes` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Gen.Parameters.NodeCreation` | CUSTOM_SIMPLE, quiet base | Declares parameters with and without inline defaults and asserts the matching `UMaterialExpression*Parameter` nodes are created |
| `DreamShader.Gen.Parameters.OtherNodeCreation` | CUSTOM_SIMPLE, quiet base | Same axis for the parameter types beyond Scalar / Vector / Dynamic. Texture-object and asset-required types are out of scope |
| `DreamShader.Gen.Parameters.InputWiring` | CUSTOM_SIMPLE, quiet base | The `Param(InputPin = expr)` call form; asserts the named pins end up connected |
| `DreamShader.Gen.Layout.NoOverlap` | CUSTOM_SIMPLE, quiet base | Generates a four-output material and asserts no two placed nodes overlap and no node hangs out of its comment box, measuring each node with the same `EstimateMaterialNodeSize` the placement used. The fixture needs balanced trees (rank is distance to the sink, so a chain leaves one node per column with nothing to collide) and a pair of eight-argument `Function` call sites — ordinary math nodes default to `bCollapsed`, draw no preview and stand ~124 tall, which clears any plausible fixed row pitch; the `Custom` node a call site generates is ~416 |
| `DreamShader.Compiler.Generate.InstanceAlias` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomBackend` | SIMPLE | — |
| `DreamShader.Render.ThinCustomVsGraphParity` | CUSTOM_SIMPLE, quiet base | **Needs a real RHI.** See [Render parity](#render-parity) |
| `DreamShader.Roundtrip.MTestToonRenderParity` | CUSTOM_SIMPLE, quiet base | **Needs a real RHI** and a project-specific asset; skips when either is missing |
| `DreamShader.Roundtrip.RenamedChannelUsesMask` | CUSTOM_SIMPLE, quiet base | — |
| `DreamShader.Compiler.Generate.ThinCustomTexture` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomUI` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomPostProcess` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomSceneReads` | SIMPLE | — |
| `DreamShader.Compiler.Generate.ThinCustomMaterialAttributes` | SIMPLE | — |
| `DreamShader.Compiler.Generate.InstanceAliasStateReads` | SIMPLE | — |
| `DreamShader.Compiler.Generate.InstanceAliasImportedFunction` | SIMPLE | — |
| `DreamShader.Compiler.Generate.InstanceAliasBaseOverrides` | SIMPLE | — |

### `DreamShaderParameterTests.cpp` — 3 declarations

| Test | Covers |
| :-- | :-- |
| `DreamShader.Lang.ParameterExpressions.ParseAll` | 24 declarations covering 23 distinct parameter-node keywords; `ScalarParameter`, `VectorParameter` and `TextureObjectParameter` appear twice, once with an inline default and once without |
| `DreamShader.Lang.ParameterExpressions.GroupScope` | `Group("X") { … }` stamps the group; loose properties stay ungrouped and unsorted; auto `SortPriority` is a global counter with step 10 from 0; an explicit `SortPriority` wins and consumes no counter slot; `Slider(0, 1)` expands to exactly two reflected properties |
| `DreamShader.Lang.ParameterExpressions.NestedGroupScope` | Nested groups compose with `\|` — `Group("Surface") { Group("SS") { … } }` yields `Surface\|SS`; a literal `Group("Manual\|Literal")` passes through unchanged |

### `DreamShaderPureFunctionTests.cpp` — 8 declarations

No editor, world or asset dependency; these run in milliseconds.

| Test | Covers |
| :-- | :-- |
| `DreamShader.Lang.Diagnostics.ParseErrorLocation` | `TryParseErrorLocation`, including clamping line/column to `>= 1` and rejecting non-numeric coordinates |
| `DreamShader.Lang.Diagnostics.BuildGenerateDiagnostics` | `BuildGenerateErrorDiagnostics` line splitting |
| `DreamShader.Lang.Diagnostics.TextWireUtils` | The diagnostics wire text is the same under the `en-US` and `zh-Hans` editor cultures |
| `DreamShader.Lang.Diagnostics.NoFile` | A diagnostic of no file, reported with an empty path, carries no location: no made-up file in its record or its compile error |
| `DreamShader.Lang.Import.ExtractImportPath` | `TryExtractImportPathFromLine`: quoting rules, comment rejection, trailing-junk rejection |
| `DreamShader.Lang.Import.NormalizeSpecifier` | `NormalizeImportSpecifier`: extensionless specifiers gain `.dsh`, backslashes and leading `./` are stripped |
| `DreamShader.Commandlet.Args.SplitAndGet` | Commandlet key/value normalization and the `Params → Switches → Tokens` search order, on lists from `UCommandlet::ParseCommandLine` |
| `DreamShader.Commandlet.Args.FlagValues` | `HasCommandletFlag`: the [Boolean flags](../tools/commandlet.md#boolean-flags) table row by row, `DSH9110` for any other value |

### Corpus runners — 13 declarations

| Test base name | Corpus subtree |
| :-- | :-- |
| `DreamShader.Lang.Parse` | `Parse/` — the 1.x parse-equivalence set, read by the legacy front end |
| `DreamShader.Lang2.Corpus` | `Lang/` |
| `DreamShader.Lang2.CorpusIR` | `IR/` (its `Instances/*.dsi` included) |
| `DreamShader.Lang2.CorpusLegacyParse` | `Legacy/Parse/` |
| `DreamShader.Lang2.CorpusLegacyIR` | `Legacy/IR/` |
| `DreamShader.Lang2.CorpusDecompile` | `Decompile/` |
| `DreamShader.Lang2.CorpusMigrate` | `Migrate/` |
| `DreamShader.Lang2.RoundtripIR` | sweeps `IR/`, `Lang/Examples/` and `Compile/` without a text golden |
| `DreamShader.Lang2.Trivia.CommentInvariant` | every fixture that parses, in both languages, one sub-test per corpus directory: no comment is lost through parse → print |
| `DreamShader.Compiler2.Corpus` | `Compile/` |
| `DreamShader.Compiler2.CorpusLegacy` | `Legacy/Compile/` — the former `Generate/` fixtures |
| `DreamShader.Compiler2.CorpusInstance` | `Instance/` |
| `DreamShader.Compiler2.Roundtrip.Corpus` | `Roundtrip/` |

The four `Compiler2` runners are `IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST` with a quiet base (graph
auto-layout trips a benign engine ensure); the rest are `IMPLEMENT_COMPLEX_AUTOMATION_TEST`.

## Graph baseline *(since 1.9.0)*

The automation suite says a compile still *succeeds*. It does not say the compiler still produces the
**same graph** — and for a compiler rewrite that is the only question. The
[`dump-graph`](../tools/commandlet.md#dump-graph-since-190) commandlet verb closes that gap: it writes
one canonical JSON per generated asset (node classes, reflected properties, connections, pin order;
no coordinates, colours or GUIDs), and two captures are compared with an ordinary text diff.

Capture a baseline for a whole project. The compile step is not optional — `dump-graph` never writes
an asset, so an asset already on disk is dumped as it stands rather than rebuilt:

```powershell
& $UnrealEditorCmd $Project -run=DreamShader compile -All -Force -unattended -nopause -nullrhi -nosplash -stdout -log
& $UnrealEditorCmd $Project -run=DreamShader dump-graph -All -Out="I:/Baseline/before" -unattended -nopause -nullrhi -nosplash -stdout -log
```

Then, after the change:

```powershell
git diff --no-index -- I:/Baseline/before I:/Baseline/after
```

Every hunk is a behavioural difference. `./dsc.ps1 dump-graph -All -Out …` is the shorter spelling of
the same two commands.

Three automation tests guard the dump itself, and the first is the one that could not be replaced by
reading the code — it generates one source **twice** and asserts the two JSON files are byte-identical,
which is the only way to see a leaked GUID, a path-name-seeded node colour, or an object-name suffix:

| Test | Asserts |
| :-- | :-- |
| `DreamShader.DumpGraph.Determinism` | Two full generations of one source dump identical bytes; LF endings; one trailing newline |
| `DreamShader.DumpGraph.ExcludesEditorState` | No `MaterialExpressionGuid`, `EditorX/Y`, `NodeColor`, `Desc`, `bCollapsed`, `VariableGuid`… key survives — and the behavioural keys do |
| `DreamShader.DumpGraph.NodeCount` | The tiny fixture dumps three nodes, every expression in the graph appears exactly once, and ids run `n0`…`n(N-1)` |

All three pin the project's `DefaultBackend` to `Graph` with `FScopedDreamShaderGraphBackendPin`, and
clean up their source file, their asset and their scratch output tree.

## Render parity

Both pixel tests render two materials and compare them frame to frame.
`ThinCustomVsGraphParity` builds twins from the same body, one per backend (`Graph` and
`ThinCustom`); `MTestToonRenderParity` compares a project material against the material regenerated
from its own decompiled source (`Original` versus `RoundTripTwin`).

| Criterion | Value |
| :-- | :-- |
| Render size | 256 × 256 |
| Mesh | `sphere` |
| Orbit yaw / pitch | `-157.5` / `-11.25` |
| Per-channel tolerance | `2` |
| Allowed offending pixels | `PixelCount / 1000`, i.e. 0.1 % |
| Content sentinel — ThinCustom parity | more than 5 % of pixels must be green-dominant (`G > R + 30` and `G > B + 30`) |
| Content sentinel — `MTestToonRenderParity` | more than 5 % of pixels must differ from the clear colour `(44, 44, 48)` by more than 8 |
| Warm-up | one throwaway render per twin, then `FAssetCompilingManager::FinishCompilationForObjects`, `GShaderCompilingManager->FinishAllCompilation()` and `FlushRenderingCommands()` |
| Failure artifacts | `<Project>/Saved/DreamShaderTests/Parity_<Case>_<Backend>.png` for `ThinCustomVsGraphParity`, `<Project>/Saved/DreamShaderTests/MTestToon_<Label>.png` for `MTestToonRenderParity` |

Parity cases — ThinCustom and Graph twins of the same body, differing only in `Backend`:

| Case | Exercises |
| :-- | :-- |
| `FlatParams` | `ScalarParameter` / `VectorParameter` by-name binding on an Unlit, Opaque material |
| `UvTexture` | `TextureObjectParameter` plus `UE.TexCoord(Index=0)` and `SampleTexture2D` — the interpolator and texture path |
| `LitAttributes` | A `MaterialAttributes` output on a `DefaultLit` material — the MakeMaterialAttributes path |

## The corpus

`Tests/Corpus` lives under the **plugin**, not under the project's `DShader` tree, so it travels with
the plugin and is never picked up by ordinary source discovery.
[`Tests/Corpus/README.md`](../../Tests/Corpus/README.md) is the reference for the tree, every golden
schema and every runner; this section is the map.

| Layer | Sources | What a case does | Needs |
| :-- | --: | :-- | :-- |
| `Parse/` | 36 | a 1.x text is accepted or refused by the legacy front end exactly as 1.x did | `Core` |
| `Lang/` | 72 | one `ParseDreamShaderLang`, structural counts, print → parse → print is byte-identical | `Core` |
| `IR/` | 151 | bind → build → passes → validate; the golden is the IR dump | `Core`, hand-made catalog |
| `Legacy/Parse/` | 117 | 1.x text → legacy front end (with trivia) → the 2.0 text it prints, which has to parse as 2.0 | `Core` |
| `Legacy/IR/` | 49 | one fixture per documented 1.x rule (L2–L26), and the Substrate sugar as 1.x spells it | `Core`, hand-made catalog |
| `Decompile/` | 25 | `.dss` → IR → AST → text, and that text lowers to an equivalent IR | `Core`, hand-made catalog |
| `Migrate/` | 20 | 1.x text → migrator → 2.0 text: no comment lost, it builds, the IR is equivalent | `Core`, hand-made catalog |
| `Compile/` | 12 | the whole pipeline into assets; the golden holds the graph dump | editor |
| `Legacy/Compile/` | 10 | the same for 1.x sources | editor |
| `Instance/` | 10 | a `.dsi` with the sibling `.dss` its `Parent` names | editor |
| `Roundtrip/` | 11 | `.dss` → asset → decompile service → text → asset; both dumps equal | editor |
| `Parity/` | — | goldens only: 1.x graph captures the `Lang/Examples` sources have to reproduce | editor |

Discovery is recursive over the layer's extensions, then sorted.

> [!WARNING]
> `Tests/` is **not** in the [release archive](release.md#archive-contents). A plugin installed from
> a release zip has no corpus, so the thirteen runners enumerate zero sub-tests and the suite silently
> shrinks to its simple declarations. Run the corpus from a repository checkout.

### Naming convention

| Convention | Rule |
| :-- | :-- |
| Layer prefix | a short prefix per directory (`L_` lexical, `T_` top-level, `S_` sections, `Ty_` types, `E_` expressions, `D_` declarations or decompile, `Rf_` reflected, `Cu_` custom, `M_` material, `I_` instance, `H_` header …) |
| Negative case | the filename **contains `.bad.`**, matched case-insensitively. With no golden, the default expectation flips to "this FAILS" |
| Golden name | only the last extension is stripped, so `X.bad.dsm` pairs with `X.bad.expected.json` and `X.dsm` with `X.expected.json` |
| Golden presence | optional — absent means the defaults apply: positive ⇒ must succeed, `.bad.` ⇒ must fail |
| Sub-test name | the path relative to the layer directory with the extension stripped and `/` and `\` replaced by `.` |

Resulting full test names:

```text
DreamShader.Lang.Parse.TopLevel.T_MinimalShader
DreamShader.Lang2.CorpusIR.Reflected.Rf_LateBoundPin
DreamShader.Compiler2.CorpusLegacy.Material.M_Surface
```

### Entry point per layer

| Layer | Entry point |
| :-- | :-- |
| `Parse/`, `Lang/` | `ParseDreamShaderLang` — `Auto` picks the legacy front end for `.dsm` / `.dsf`, and per declaration in a `.dsh` |
| `IR/`, `Legacy/IR/` | `BindDreamShaderLang` → `BuildDreamShaderIR` → `RunDreamShaderIRPasses` → `ValidateDreamShaderIR` |
| `Legacy/Parse/` | `ParseDreamShaderLang` with `bKeepTrivia` → `PrintDreamShaderLang`, and the printed text parsed again as 2.0 |
| `Decompile/` | `RaiseDreamShaderIR` → `BuildDreamShaderAstFromIR` → print → lower again → `AreDreamShaderIRModulesEquivalent` |
| `Migrate/` | legacy parse + bind → `MigrateDreamShaderLegacyModule` → print → comments counted → built as 2.0 → IR equivalence |
| `Compile/`, `Legacy/Compile/`, `Instance/`, `Parity/` | `CompileDreamShaderTestAssets` — the test façade over the compiler service's `CompileAssets` — then `dump-graph`'s dump of every asset |
| `Roundtrip/` | compile → `RunDreamShaderDecompileRequest` over the whole source → compile the text under another scratch root → compare the two dumps |

The compile layers copy each fixture into a scratch source root and build into
`/Game/DreamShaderTests/...`, and clean both up.

## `.expected.json`

Every field is opt-in; an absent field asserts nothing. The table below is the **`Parse/` layer's**
golden (`"entryPoint": "parse"`): since the 1.x parser retired in 2.0, its `definition` fields are
read out of the legacy front end's AST and `FLegacyMigrationInfo` instead of an
`FTextShaderDefinition`, under the same names. Every other layer has its own shape — `lang`, `ir`,
`legacy`, `legacy-ir`, `decompile`, `migrate`, `compile`, `roundtrip` — documented in
[`Tests/Corpus/README.md`](../../Tests/Corpus/README.md). Two conventions they share:
`errorContains` / `warningsContain` / `infosContain` hold `DSHnnnn` codes rather than message text,
and a `...Pending: true` flag (`textPending`, `irPending`, `graphPending`) skips the byte comparison
of a golden nobody has reviewed yet — removing the flag is the reviewer's act, never the writer's.

| Field | Type | Layer | Meaning |
| :-- | :-- | :-- | :-- |
| `entryPoint` | string | — | **Informational only** in a `Parse/` golden: its decoder never reads it. The `Parse/` writer emits `"parse"` |
| `outcome` | string | all | `"error"`, compared case-insensitively, expects failure; **any other value, including `"ok"`, expects success**. Overrides the `.bad.` filename default |
| `errorContains` | string[] | all | Checked when failure is expected: every substring must appear in some error diagnostic, compared case-insensitively — in practice the `DSHnnnn` code. `messageContains`, the 1.x Generate layer's spelling, is read into the same list |
| `warningsContain` | string[] | all | Each substring must be found in some warning diagnostic, case-insensitively |
| `definition.name` | string | Parse | The `Name=` of the first product block |
| `definition.settings` | object | Parse | String → string. Asserted through `TryGetSetting`: **key case-insensitive, value exact**; every listed key must be present |
| `definition.outputDeclarations` | number | Parse | Count of output declarations |
| `definition.outputs` | number | Parse | Count of output bindings |
| `definition.materialFunctions` | number | Parse | Count of `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` blocks |
| `definition.materialFunction0Kind` | string | Parse | One of `ShaderFunction`, `ShaderLayer`, `ShaderLayerBlend`. Checked only when at least one material function exists |
| `definition.virtualFunctions` | number | Parse | Count of `VirtualFunction` blocks |
| `definition.codeNotEmpty` | bool | Parse | Whether `Definition.Code` is non-empty |

> [!WARNING]
> `outcome` is compared only against `"error"` (case-insensitively, so `"Error"` and `"ERROR"` are
> fine). A typo — `"fail"`, `"error "` with a
> trailing space, `"errors"` — makes the golden expect **success**, and a fixture that was meant to
> pin a failure silently starts asserting the opposite. Use `"ok"` or `"error"` and nothing else.

## Adding a fixture

1. Drop a source into the layer's directory — `Tests/Corpus/IR/<Area>/X.dss`,
   `Tests/Corpus/Legacy/Compile/Material/M_X.dsm`, … Use the layer prefix; add `.bad.` to the stem
   for a negative case.
2. Optionally add `<same stem>.expected.json` with any subset of the fields above. A positive case
   with no golden still asserts "must succeed"; a `.bad.` case with no golden asserts "must fail".
3. **Write no C++ and do not recompile.** Every runner enumerates its tree on the next run.
4. Run the layer once to see the new sub-test appear:
   `-ExecCmds="Automation RunTests DreamShader.Lang.Parse; Quit"`.
5. To bootstrap a golden from the actual result, add `-DreamShaderUpdateGolden` and run again, then
   review the written JSON by hand before committing. A new text, IR or graph golden is written with
   its `...Pending` flag set; delete the flag once you have read the golden line by line.

### Regenerating goldens

With `-DreamShaderUpdateGolden` on the command line, each runner writes the golden instead of
asserting it, logging `Updated golden '{Path}'.` or, on a write failure, `Failed to write golden '{Path}'.`

What the `Parse/` writer emits: `entryPoint: "parse"`, `outcome`, then either
`errorContains: [<the codes>]` or a `definition` object with `name` (omitted when empty),
`outputDeclarations`, `outputs`, `materialFunctions`, `materialFunction0Kind` (only when at least one
exists), `virtualFunctions`, `codeNotEmpty` and `settings` (only when non-empty); plus a top-level
`warningsContain` when the parse produced warnings. The other layers' writers are described with
their schemas in the corpus README.

Output is pretty-printed JSON.

> [!WARNING]
> `-DreamShaderUpdateGolden` rewrites goldens from whatever the code currently does. Running it
> after an unreviewed change accepts a regression as the new baseline. Diff every touched
> `.expected.json` before committing, and never put the switch in a CI job.

## Diagnostics

Assertion and infrastructure messages the runners emit. Runtime substitutions are shown as
`{Placeholder}`; `{Case}` is the fixture's **absolute source path**, not its sub-test name.

| Message | Layer | Cause |
| :-- | :-- | :-- |
| `[{Case}] the legacy front end should REFUSE this source` | Parse | the fixture was accepted but was expected to fail *(since 2.0.0)* |
| `[{Case}] the legacy front end should ACCEPT this source but reported: {Error}` | Parse | the fixture was refused but was expected to succeed *(since 2.0.0)* |
| `[{Case}] an error contains '{Substring}' (actual: {Error})` | Parse | an `errorContains` entry was not found |
| `[{Case}] a warning contains '{Substring}' (actual: {Warnings})` | Parse | a `warningsContain` entry matched no warning |
| `[{Case}] name == '{Expected}' (actual '{Actual}')` | Parse | `definition.name` mismatch |
| `[{Case}] outputDeclarations` | Parse | `definition.outputDeclarations` mismatch |
| `[{Case}] outputs` | Parse | `definition.outputs` mismatch |
| `[{Case}] materialFunctions` | Parse | `definition.materialFunctions` mismatch |
| `[{Case}] materialFunction0Kind` | Parse | `definition.materialFunction0Kind` mismatch |
| `[{Case}] virtualFunctions` | Parse | `definition.virtualFunctions` mismatch |
| `[{Case}] codeNotEmpty == {Value}` | Parse | `definition.codeNotEmpty` mismatch |
| `[{Case}] setting '{Key}' present` | Parse | a key from `definition.settings` is absent |
| `[{Case}] setting '{Key}' == '{Expected}' (actual '{Actual}')` | Parse | that key's value differs |
| `Cannot read corpus source '{Path}'.` | all | the fixture file could not be read |
| `Cannot read golden '{Path}'.` | all | the `.expected.json` exists but could not be read |
| `Malformed golden '{Path}': {Reason}` | all | the golden is not valid JSON (`invalid JSON`) or does not decode |
| `Updated golden '{Path}'.` | all | `-DreamShaderUpdateGolden` wrote the golden |
| `Failed to write golden '{Path}'.` | all | `-DreamShaderUpdateGolden` could not write it |
| `DreamShader {layer} corpus test invoked without a source path.` | every runner | the sub-test was launched with an empty command parameter; `{layer}` is `parse`, `lang`, `IR`, `legacy parse`, `legacy IR`, `legacy compile`, `compile`, `instance`, `decompile`, `migrate` or `roundtrip` (`DreamShader RoundtripIR test …` for that runner) |
| `Failed to write DreamShader automation source file '{Path}'.` | harness | a test could not write its temporary source |

The 1.x Generate runner and its `generation should …` messages are gone *(since 2.0.0)*: its fixtures
are `Legacy/Compile/`, compiled through the 2.0 pipeline. The other layers' assertion messages are in
their runners and in [`Tests/Corpus/README.md`](../../Tests/Corpus/README.md).

## Example

The shipped negative fixture that pins the "one `Shader` per parse unit" rule.
`Tests/Corpus/Parse/TopLevel/T_TwoShaders.bad.dsm`:

```c
Shader(Name="DreamShaderTests/Corpus/M_First")
{
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = { Color = vec3(1.0, 0.0, 0.0); }
}

Shader(Name="DreamShaderTests/Corpus/M_Second")
{
    Settings = { Domain = "UI"; ShadingModel = "Unlit"; }
    Outputs = { vec3 Color; Base.EmissiveColor = Color; }
    Graph = { Color = vec3(0.0, 1.0, 0.0); }
}
```

The `.bad.` stem alone would assert "the parse must fail". Its golden,
`T_TwoShaders.bad.expected.json`, also pins the code the legacy front end raises for a second `Shader`
block, [`DSH2250`](../diagnostics/DSH2xxx.md#dsh2250):

```json
{
	"entryPoint": "parse",
	"outcome": "error",
	"errorContains": ["DSH2250"]
}
```

Run it:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" --% "D:\Work\MyProject\MyProject.uproject" -ExecCmds="Automation RunTests DreamShader.Lang.Parse.TopLevel; Quit" -nullrhi -unattended -nopause -nosplash -log
```

The new sub-test appears as:

```text
DreamShader.Lang.Parse.TopLevel.T_TwoShaders.bad
```

## See also

- [Contributing](index.md) — building the plugin and the source-tree layout
- [Release](release.md) — why `Tests/` is absent from the release archive
- [Commandlet](../tools/commandlet.md) — `-run=DreamShader`, the other headless entry point
- [Editor bridge](../tools/bridge.md) — what `-NoDreamShaderEditorBridge` turns off
- [Preview](../tools/preview.md) — the renderer the parity tests drive
- [Backend](../settings/backend.md) — `Graph` vs `ThinCustom`, the axis the parity cases compare
- [Project settings](../settings/project.md) — `DefaultBackend`, which the `Compile/` runner pins to `Graph`
- [`DreamShaderLang`](../api/lang-module.md) — `ParseDreamShaderLang` and every other entry point the text layers call
- [Diagnostics index](../diagnostics/index.md) — the messages fixtures assert against
- [Version compatibility](../api/version-compat.md) — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` and the UE 5.4 skip
</content>
