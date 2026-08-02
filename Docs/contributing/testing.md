# Testing

> [DreamShader](../index.md) » [Contributing](index.md) » **Testing**

The plugin's Unreal automation suite, and the on-disk fixture corpus that two of its tests enumerate
at run time.

| | |
| :-- | :-- |
| Declared in | `Source/DreamShaderEditor/Private/Tests/` — five translation units, all inside `#if WITH_DEV_AUTOMATION_TESTS` |
| Kind | Unreal automation tests |
| Flags | `EAutomationTestFlags::EditorContext \| EAutomationTestFlags::EngineFilter` — identical on all 40 declarations |
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
| Test declarations (`IMPLEMENT_*_AUTOMATION_TEST`) | 40 |
| Simple declarations (one test each) | 38 |
| Complex declarations (data-driven runners) | 2 — `DreamShader.Lang.Parse`, `DreamShader.Gen.Material` |
| Corpus fixtures | 41 — 32 under `Parse/`, 9 under `Generate/` |
| `.expected.json` goldens | 10 — 2 under `Parse/`, 8 under `Generate/` |
| Individually runnable tests | **79** = 38 simple + 32 Parse sub-tests + 9 Generate sub-tests |

The two complex runners enumerate the corpus tree at run time, so the runnable-test count moves with
the fixture count and **no C++ changes when a fixture is added**.

## Test groups

| Prefix | Layer | Requirements |
| :-- | :-- | :-- |
| `DreamShader.Lang.Parse.*` | Data-driven parse corpus | fast; no asset I/O |
| `DreamShader.Lang.Diagnostics.*` | Pure diagnostic helpers | milliseconds; no editor state |
| `DreamShader.Lang.Import.*` | Pure import-specifier helpers | milliseconds; no editor state |
| `DreamShader.Lang.ParameterExpressions.*` | Parser `Properties` surface | fast; parser only |
| `DreamShader.Commandlet.Args.*` | Commandlet argument parsing | milliseconds; pure |
| `DreamShader.Commandlet.Compile.*` | Commandlet runner smoke test | slow; editor; writes a real `/Game` asset |
| `DreamShader.Compiler.Parser.*` | Generator front end, end to end | slow; editor |
| `DreamShader.Compiler.Generate.*` | Generator end to end | slow; editor |
| `DreamShader.Compiler.SourceHash.*` | The regeneration skip check | slow; editor |
| `DreamShader.Gen.Material.*` | Data-driven Generate corpus | slow; editor |
| `DreamShader.Gen.Graph.*` | Node-shape assertions on the generated graph | slow; editor |
| `DreamShader.Gen.Parameters.*` | Parameter-node creation and pin wiring | slow; editor |
| `DreamShader.Gen.Wiring.*` | Condition wiring in the generated graph | slow; editor |
| `DreamShader.Roundtrip.*` | Decompile → regenerate fidelity | slow; editor; two of them also need a real RHI |
| `DreamShader.Render.*` | Pixel parity | needs a real RHI |

The split the source records: the fast `DreamShader.Lang.*` layer gates pull requests, the slow
`DreamShader.Gen.*` layer runs nightly.

## Running headlessly

| Goal | Filter |
| :-- | :-- |
| Fast gate — parse corpus, pure helpers, parameter parsing | `DreamShader.Lang` |
| Commandlet argument helpers only | `DreamShader.Commandlet.Args` |
| Generate corpus | `DreamShader.Gen.Material` |
| Generator end-to-end tests | `DreamShader.Compiler` |
| Decompile round trips | `DreamShader.Roundtrip` |
| Everything | `DreamShader` |

### Command-line switches

| Switch | Effect on the suite |
| :-- | :-- |
| `-nullrhi` | No rendering device. `DreamShader.Render.ThinCustomVsGraphParity` and `DreamShader.Roundtrip.MTestToonRenderParity` self-skip. Every other test still runs. |
| `-DreamShaderUpdateGolden` | Both corpus runners **rewrite** each `.expected.json` from the actual result instead of asserting it. See [Regenerating goldens](#regenerating-goldens). |
| `-NoDreamShaderEditorBridge` | Skips creating the editor bridge and the Material Content Browser: no directory watcher, no in-memory generation pass at startup, no WebSocket listener on `127.0.0.1:17864`. Useful when a run must not compete with the bridge for the same sources. |
| `-unattended -nopause -nosplash` | Standard headless flags; no modal dialogs, no splash, no keypress on exit. |
| `-log` / `-stdout` | Route the log to the console. |

> [!WARNING]
> **A skipped test reports success.** Every skip path in the suite calls `AddInfo(...)` and then
> `return true`. A `-nullrhi` run therefore shows `DreamShader.Render.ThinCustomVsGraphParity` and
> `DreamShader.Roundtrip.MTestToonRenderParity` green **without having compared a single pixel**, and
> a run on UE 5.3 shows `DreamShader.Compiler.Generate.SubstrateMaterial` green without having
> generated a Substrate material. To prove render parity, run without `-nullrhi` and read the info
> lines in the log.

### Skip conditions

Every condition under which a test returns success without asserting anything.

| Test | Condition | Message |
| :-- | :-- | :-- |
| `DreamShader.Compiler.Generate.SubstrateMaterial` | `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` is 0 — UE < 5.4 | `DreamShader Substrate builtins are not available for this Unreal Engine version; skipping generation test.` |
| `DreamShader.Render.ThinCustomVsGraphParity` | `GUsingNullRHI` or `!FApp::CanEverRender()` | `Skipping ThinCustom-vs-Graph render parity: no usable RHI (-nullrhi). Run without -nullrhi for the full pixel comparison.` |
| `DreamShader.Roundtrip.MTestToonRenderParity` | `GUsingNullRHI` or `!FApp::CanEverRender()` | `Skipping M_Test_Toon round-trip render parity: no usable RHI (run without -nullrhi).` |
| `DreamShader.Roundtrip.MTestToonRenderParity` | the project asset it round-trips is absent | `Skipping M_Test_Toon round-trip: '{ObjectPath}' is not present in this project.` |
| `DreamShader.Gen.Material.*` | the fixture's extension is `.dsh` | `[{SourcePath}] is a .dsh header; nothing to generate (skipped).` |

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
> The Generate corpus goldens encode **Graph-backend** semantics, so the Generate runner pins
> `DefaultBackend = Graph` for the duration of each case. The project default is `ThinCustom`. A
> fixture written against ThinCustom-specific behaviour will not behave as expected in the corpus.

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

### `DreamShaderPureFunctionTests.cpp` — 5 declarations

No editor, world or asset dependency; these run in milliseconds.

| Test | Covers |
| :-- | :-- |
| `DreamShader.Lang.Diagnostics.ParseErrorLocation` | `TryParseErrorLocation`, including clamping line/column to `>= 1` and rejecting non-numeric coordinates |
| `DreamShader.Lang.Diagnostics.BuildGenerateDiagnostics` | `BuildGenerateErrorDiagnostics` line splitting |
| `DreamShader.Lang.Import.ExtractImportPath` | `TryExtractImportPathFromLine`: quoting rules, comment rejection, trailing-junk rejection |
| `DreamShader.Lang.Import.NormalizeSpecifier` | `NormalizeImportSpecifier`: extensionless specifiers gain `.dsh`, backslashes and leading `./` are stripped |
| `DreamShader.Commandlet.Args.SplitAndGet` | Commandlet key/value normalization and the `Params → Switches → Tokens` search order |

### Corpus runners — 2 declarations

| Test base name | Macro | Corpus subtree |
| :-- | :-- | :-- |
| `DreamShader.Lang.Parse` | `IMPLEMENT_COMPLEX_AUTOMATION_TEST` | `Tests/Corpus/Parse` |
| `DreamShader.Gen.Material` | `IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST` with a quiet base | `Tests/Corpus/Generate` |

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

```text
<Plugin>/Tests/Corpus/
  README.md
  Parse/                      -> DreamShader.Lang.Parse.*
    Builtins/                 B_  UE.* and Substrate.* call surfaces
    Graph/                    G_  Graph statements and expressions
    Lexical/                  L_  comments, strings, block balance
    Sections/                 S_  Properties / Settings / Outputs / Inputs
    TopLevel/                 T_  Shader / ShaderFunction / Namespace / VirtualFunction
    Types/                    Ty_ type tokens and aliases
  Generate/                   -> DreamShader.Gen.Material.*
    Material/                 M_  end-to-end generation cases
```

Discovery is recursive over `*.dsm`, `*.dsf` and `*.dsh` under the layer directory, then sorted.

> [!WARNING]
> `Tests/` is **not** in the [release archive](release.md#archive-contents). A plugin installed from
> a release zip has no corpus, so `DreamShader.Lang.Parse` and `DreamShader.Gen.Material` enumerate
> zero sub-tests and the suite silently shrinks from 79 runnable tests to 38. Run the corpus from a
> repository checkout.

### Parse fixtures (32)

| Directory | Fixture | Golden |
| :-- | :-- | :-- |
| `Parse/Builtins` | `B_Substrate.dsm` | — |
| `Parse/Builtins` | `B_UEBuiltins.dsm` | — |
| `Parse/Graph` | `G_Arithmetic.dsm` | — |
| `Parse/Graph` | `G_BraceInit.dsm` | — |
| `Parse/Graph` | `G_IfElse.dsm` | — |
| `Parse/Graph` | `G_MaterialAttributes.dsm` | — |
| `Parse/Graph` | `G_MathBuiltins.dsm` | — |
| `Parse/Graph` | `G_SwizzleReorder.dsm` | — |
| `Parse/Lexical` | `L_Comments.dsm` | — |
| `Parse/Lexical` | `L_UnterminatedBlock.bad.dsm` | `L_UnterminatedBlock.bad.expected.json` |
| `Parse/Lexical` | `L_UnterminatedString.bad.dsm` | — (implicit "must fail") |
| `Parse/Sections` | `S_ExplicitParams.dsm` | — |
| `Parse/Sections` | `S_NoEqualsSections.dsm` | — |
| `Parse/Sections` | `S_OptInputs.dsf` | — |
| `Parse/Sections` | `S_OutputsBinding.dsm` | — |
| `Parse/Sections` | `S_PropertiesConst.dsm` | — |
| `Parse/Sections` | `S_SettingsExtended.dsm` | — |
| `Parse/Sections` | `S_ShaderFunctionNoEquals.dsf` | — |
| `Parse/TopLevel` | `T_FunctionNoOut.bad.dsh` | — (implicit "must fail") |
| `Parse/TopLevel` | `T_FunctionReturnType.dsh` | — |
| `Parse/TopLevel` | `T_FunctionReturnVoid.bad.dsh` | — (implicit "must fail") |
| `Parse/TopLevel` | `T_FunctionSelfContained.dsh` | — |
| `Parse/TopLevel` | `T_GraphFunction.dsh` | — |
| `Parse/TopLevel` | `T_MinimalShader.dsm` | `T_MinimalShader.expected.json` |
| `Parse/TopLevel` | `T_Namespace.dsh` | — |
| `Parse/TopLevel` | `T_ShaderFunction.dsf` | — |
| `Parse/TopLevel` | `T_ShaderLayer.dsf` | — |
| `Parse/TopLevel` | `T_ShaderLayerBlend.dsf` | — |
| `Parse/TopLevel` | `T_ShaderNoName.bad.dsm` | — (implicit "must fail") |
| `Parse/TopLevel` | `T_TwoShaders.bad.dsm` | — (implicit "must fail") |
| `Parse/TopLevel` | `T_VirtualFunction.dsh` | — |
| `Parse/Types` | `Ty_VectorAliases.dsm` | — |

### Generate fixtures (9)

| Fixture | Golden `outcome` | Golden `messageContains` |
| :-- | :-- | :-- |
| `M_GraphBuiltins.dsm` | `ok` | — |
| `M_IfBranchParamRead.dsm` | `ok` | — |
| `M_IfBranchTypeMismatch.dsm` | `error` | `["inconsistent types"]` |
| `M_IntegerDivide.dsm` | `error` | `["Integer division is not supported"]` |
| `M_MaterialAttributeRead.dsm` | `ok` | — |
| `M_ScalarSwizzle.dsm` | `error` | `["Swizzle"]` |
| `M_Suffix.dsm` | `ok` | — |
| `M_Surface.dsm` | *no golden* — implicit "must succeed" | — |
| `M_Truncate.dsm` | `error` | `["matching vector sizes"]` |

Some `Generate/` fixtures deliberately encode a **known generator defect**: they assert today's wrong
message, and turn red — demanding a golden update — when the defect is fixed.

### Naming convention

| Convention | Rule |
| :-- | :-- |
| Layer prefix | `L_` lexical, `T_` top-level, `S_` sections, `Ty_` types, `G_` graph, `B_` builtins, `M_` generate/material |
| Negative case | the filename **contains `.bad.`**, matched case-insensitively. With no golden, the default expectation flips to "parse or generation FAILS" |
| Golden name | only the last extension is stripped, so `X.bad.dsm` pairs with `X.bad.expected.json` and `X.dsm` with `X.expected.json` |
| Golden presence | optional — absent means the defaults apply: positive ⇒ must succeed, `.bad.` ⇒ must fail |
| Sub-test name | the path relative to the layer directory with the extension stripped and `/` and `\` replaced by `.` |

Resulting full test names:

```text
DreamShader.Lang.Parse.TopLevel.T_MinimalShader
DreamShader.Lang.Parse.Lexical.L_UnterminatedBlock.bad
DreamShader.Gen.Material.Material.M_Surface
```

> [!NOTE]
> The Generate layer's segment is doubled — `DreamShader.Gen.Material` is the runner's own name and
> `Material` is the subdirectory under `Corpus/Generate/`. Filtering on `DreamShader.Gen.Material`
> still selects all of them.

### Entry point per extension

| Extension | Parse layer | Generate layer |
| :-- | :-- | :-- |
| `.dsm` | `FTextShaderParser::Parse` | `FMaterialGenerator::GenerateMaterialFromFile(path, msg, bForce=true, bTransient=true)` |
| `.dsf` | `FTextShaderParser::Parse` | `FMaterialGenerator::GenerateAssetsFromFile(path, msg, bForce=true, bTransient=true)` |
| `.dsh` | `FTextShaderParser::Parse` | skipped with an info message |

The Generate layer always runs transient, so no `/Game` package is written and no cleanup is needed.

## `.expected.json`

Every field is opt-in; an absent field asserts nothing. Fields under `definition` are read by the
Parse layer only — the Generate runner ignores the whole `definition` object.

| Field | Type | Layer | Meaning |
| :-- | :-- | :-- | :-- |
| `entryPoint` | string | — | **Informational only.** Never read by the decoder. The golden writers emit `"parse"` or `"generate"` |
| `outcome` | string | both | `"error"`, compared case-insensitively, expects failure; **any other value, including `"ok"`, expects success**. Overrides the `.bad.` filename default |
| `errorContains` | string[] | both | Every substring must appear in the diagnostic, compared case-insensitively. The Parse layer asserts it only on the failure path; the Generate layer asserts it either way |
| `warningsContain` | string[] | Parse | Each substring must be found in some entry of `Definition.Warnings`, case-insensitively |
| `messageContains` | string[] | Generate | Folded into `errorContains`; asserted against the generator's message whether the outcome is `ok` or `error` |
| `definition.name` | string | Parse | Exact `FTextShaderDefinition::Name` |
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

1. Drop a `.dsm`, `.dsf` or `.dsh` into `Tests/Corpus/Parse/<Layer>/` or
   `Tests/Corpus/Generate/Material/`. Use the layer prefix; add `.bad.` to the stem for a negative
   case.
2. Optionally add `<same stem>.expected.json` with any subset of the fields above. A positive case
   with no golden still asserts "must succeed"; a `.bad.` case with no golden asserts "must fail".
3. **Write no C++ and do not recompile.** Both runners enumerate the tree on the next run.
4. Run the layer once to see the new sub-test appear:
   `-ExecCmds="Automation RunTests DreamShader.Lang.Parse; Quit"`.
5. To bootstrap a golden from the actual result, add `-DreamShaderUpdateGolden` and run again, then
   review the written JSON by hand before committing.

### Regenerating goldens

With `-DreamShaderUpdateGolden` on the command line, each runner writes the golden instead of
asserting it, logging `Updated golden '{Path}'.` or, on a write failure, `Failed to write golden '{Path}'.`

What the writers emit:

| Layer | Emitted fields |
| :-- | :-- |
| Parse | `entryPoint: "parse"`, `outcome`, then either `errorContains: [<the error>]` or a `definition` object with `name` (omitted when empty), `outputDeclarations`, `outputs`, `materialFunctions`, `materialFunction0Kind` (only when at least one exists), `virtualFunctions`, `codeNotEmpty` and `settings` (only when non-empty); plus a top-level `warningsContain` when the parse produced warnings |
| Generate | `entryPoint: "generate"`, `outcome`, `messageContains: [<the message>]` |

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
| `[{Case}] parse should FAIL` | Parse | the fixture parsed successfully but was expected to fail |
| `[{Case}] parse should SUCCEED but failed: {Error}` | Parse | the fixture failed to parse but was expected to succeed |
| `[{Case}] error contains '{Substring}' (actual: {Error})` | Parse | an `errorContains` entry was not found |
| `[{Case}] warnings contain '{Substring}'` | Parse | a `warningsContain` entry matched no warning |
| `[{Case}] name` | Parse | `definition.name` mismatch |
| `[{Case}] outputDeclarations` | Parse | `definition.outputDeclarations` mismatch |
| `[{Case}] outputs` | Parse | `definition.outputs` mismatch |
| `[{Case}] materialFunctions` | Parse | `definition.materialFunctions` mismatch |
| `[{Case}] materialFunction0Kind` | Parse | `definition.materialFunction0Kind` mismatch |
| `[{Case}] virtualFunctions` | Parse | `definition.virtualFunctions` mismatch |
| `[{Case}] codeNotEmpty == {Value}` | Parse | `definition.codeNotEmpty` mismatch |
| `[{Case}] setting '{Key}' present` | Parse | a key from `definition.settings` is absent |
| `[{Case}] setting '{Key}'` | Parse | that key's value differs |
| `[{Case}] generation should FAIL (msg: {Message})` | Generate | generation succeeded but was expected to fail |
| `[{Case}] generation should SUCCEED but failed: {Message}` | Generate | generation failed but was expected to succeed |
| `[{Case}] message contains '{Substring}' (actual: {Message})` | Generate | a `messageContains` entry was not found |
| `Cannot read corpus source '{Path}'.` | both | the fixture file could not be read |
| `Cannot read golden '{Path}'.` | both | the `.expected.json` exists but could not be read |
| `Malformed golden '{Path}': invalid JSON` | both | the golden is not valid JSON |
| `Updated golden '{Path}'.` | both | `-DreamShaderUpdateGolden` wrote the golden |
| `Failed to write golden '{Path}'.` | both | `-DreamShaderUpdateGolden` could not write it |
| `DreamShader parse corpus test invoked without a source path.` | Parse runner | the sub-test was launched with an empty command parameter |
| `DreamShader generate corpus test invoked without a source path.` | Generate runner | the sub-test was launched with an empty command parameter |
| `Failed to write DreamShader automation source file '{Path}'.` | harness | a test could not write its temporary source |

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

It ships **without** a golden, so the `.bad.` stem alone asserts "the parse must fail" — the message
is not checked. Adding `T_TwoShaders.bad.expected.json` alongside it would also pin the wording:

```json
{
	"entryPoint": "parse",
	"outcome": "error",
	"errorContains": ["Only one top-level Shader block is currently supported."]
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
- [Project settings](../settings/project.md) — `DefaultBackend`, which the Generate runner pins to `Graph`
- [Parser API](../api/parser.md) — `FTextShaderParser::Parse`, the Parse layer's entry point
- [Diagnostics index](../diagnostics/index.md) — the messages fixtures assert against
- [Version compatibility](../api/version-compat.md) — `DREAMSHADER_WITH_SUBSTRATE_BUILTINS` and the UE 5.4 skip
</content>
