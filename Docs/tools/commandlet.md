# Commandlet

> [DreamShader](../index.md) » [Tools](index.md) » **Commandlet**

`-run=DreamShader` — a headless entry point that compiles DreamShader sources into persistent assets
and decompiles existing material assets back into source files.

| | |
| :-- | :-- |
| Kind | `UCommandlet` subclass — `UDreamShaderCommandlet`, in the `DreamShaderEditor` module |
| Invocation | `-run=DreamShader` (equivalently `-run=DreamShaderCommandlet`) |
| Commands | `compile`, `generate`, `decompile`, `export`, `dump-graph` *(since 1.9.0)*; `migrate`, `check`, `dump-ir`, `dump-layout`, `index`, `export-catalog`, `fmt`, `list-generated` *(2.0)*; `pass-registry` *(2.1.0)* |
| Exit codes | `0` success, `1` failure |
| Log category | `LogDreamShader` |

## Synopsis

```text
UnrealEditor-Cmd.exe <project>.uproject -run=DreamShader <command> [<option>…]

<command> ::= { compile | generate | decompile | export | migrate | dump-graph
              | check | dump-ir | dump-layout | index | export-catalog | fmt | list-generated
              | pass-registry }

-run=DreamShader { compile | generate } { -Source=<path> | -File=<path> | -All } [-Force]
                                        [-Define=<NAME>[=<value>]]…
-run=DreamShader { decompile | export } { -Asset=<object-path> | -SourceFile=<path> }
                                        [{ -Out | -Output }=<path>] [-Format={ Dss | Legacy | Auto }]
                                        [-KeepAssetPath] [-Readable] [-DiagnosticsOut=<file>]
-run=DreamShader migrate { -Source=<path> | -All | -Root=<source root> }
                                        [-Check] [-DryRun] [-Out=<dir>] [-NoBackup]
-run=DreamShader dump-graph { -Source=<path> | -File=<path> | -All } [{ -Out | -Output }=<dir>]
                                        [-Define=<NAME>[=<value>]]…
-run=DreamShader check { -Source=<path> | -All } [-Shaders] [-Platform=<list>] [-Quality=<list>]
                                        [-Timeout=<seconds>] [-DiagnosticsOut=<file>]
-run=DreamShader dump-ir { -Source=<path> | -All } [-Out=<dir>] [-Json]
-run=DreamShader dump-layout { -Source=<path> | -All } [-Style={ Blocks | SourceBands | Layered | All }]
                                        [-Out=<dir>] [-Json]
-run=DreamShader index { -Source=<path> | -All } [-Out=<dir>]
-run=DreamShader export-catalog [-Out=<file>]
-run=DreamShader fmt { -Source=<path> | -All } [-Check] [-Out=<dir>]
-run=DreamShader list-generated { -Source=<path> | -All } [-As={ Packages | Files | GitIgnore | Json }]
                                        [-Out=<file>] [-IncludeEphemeral]
-run=DreamShader pass-registry [-Gc | -Rebuild]
```

`compile`, `dump-graph`, `check`, `dump-ir`, `dump-layout`, `index` and `list-generated` take **every
compilable source**: `.dss`, `.dsi`, `.dsp`, `.dsm` and `.dsf`. There is one compiler; a 1.x source is
read by the legacy front end and a `.dsh` header is compiled through the sources that include it. `fmt`
takes 2.0 text — `.dss`, `.dsi`, `.dsp`, and a `.dsh` that has no 1.x declarations left. What each verb
does with a [Custom Pass pipeline](../language-v2/passes.md) is in [`.dsp` sources](#dsp-sources).

Commandlet flags declared by the class: `IsClient = false`, `IsEditor = true`, `IsServer = false`,
`LogToConsole = true`.

## Commands

The command name is the first **bare** (non-`-`) argument. If there is no bare argument, a
`Command=<name>` parameter is consulted instead. Matching is case-insensitive; surrounding whitespace
is trimmed.

| Spelling | Equivalent to | Effect |
| :-- | :-- | :-- |
| `compile` | — | Compile one source file or every project source into assets |
| `generate` | `compile` | Identical; alternate spelling |
| `decompile` | — | Export a material, function, layer, blend or material instance to a source file: 2.0 text by default (`.dss`, or `.dsi` for an instance), 1.x text with `-Format=Legacy` or an `-Out` ending in `.dsm` / `.dsf` — see [Decompiler](decompiler.md). A Custom Pass pipeline is written as a `.dsp`, and has no 1.x text *(2.1.0)* |
| `export` | `decompile` | Identical; alternate spelling |
| `migrate` *(2.0)* | — | Rewrite 1.x sources as `.dss`, proving each rewrite first — see [Migrate](migrate.md) |
| `check` *(2.0)* | — | Compile as far as IR validation and write no asset; `-Shaders` builds the products and reports HLSL errors against source lines |
| `dump-ir` *(2.0)* | — | Write the lowered graph IR of a source as text, and as JSON with `-Json` |
| `dump-layout` *(2.0)* | — | Draw the 2.0 graph layout of every product of a source as SVG, building nothing — see [`dump-layout`](#dump-layout) |
| `index` *(2.0)* | — | Write the symbol index a language service reads. The editor writes the same file after every compile. |
| `export-catalog` *(2.0)* | — | Write the builtin node catalog as JSON, so tools can bind `UE.*` without an editor. The editor writes the same file once it has loaded. |
| `fmt` *(2.0)* | `format` | Rewrite 2.0 sources in the printer's layout; `-Check` reports and writes nothing — see [`fmt`](#fmt) |
| `list-generated` *(2.0)* | — | Name every asset the sources build, building none — see [`list-generated`](#list-generated) |
| `pass-registry` *(2.1.0)* | `passregistry` | List the Custom Pass HLSL slots and what each one is; `-Gc` frees the dead ones, `-Rebuild` rebuilds the registry — see [`pass-registry`](#pass-registry) |
| `dump-graph` *(since 1.9.0)* | — | Write a canonical JSON fingerprint of the graph each source generates |
| `dumpgraph` | `dump-graph` | Identical; the hyphen is optional |

Anything else fails with `Unknown DreamShader command '{Command}'.` followed by the usage banner, and
exits `1`.

> [!WARNING]
> The first bare token is taken as the command name **unconditionally**, before any of it is
> validated. Writing an option without its leading dash *first* — `-run=DreamShader Source=X compile`
> — consumes `Source=X` as the command name and produces `Unknown DreamShader command 'Source=X'.`
> Keep the command as the first bare argument.

## `compile` / `generate`

| Option | Aliases | Type | Required | Default | Meaning |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **`-Source=<path>`** | `-File=<path>` | string | one of `-Source` / `-File` / `-All` | — | Compile exactly one source file |
| **`-All`** | — | flag | one of `-Source` / `-File` / `-All` | off | Compile every project DreamShader source |
| `-Force` | — | flag | no | off | Bypass the source-hash skip and regenerate unconditionally |
| `-Define=<NAME>[=<value>]` *(since 1.9.0)* | `-D=<NAME>[=<value>]` | string, repeatable | no | — | Contribute one [preprocessor define](../language/preprocessor.md) to this run |

Precedence: `-Source` is looked up first, then `-File`; `-All` is consulted only when neither yielded
a value. `-Source` and `-All` together silently compiles only the one file. With none of the three,
the usage banner is logged at `Error` and the run exits `1`.

### `-Source` path resolution

Tried in this order; the first that applies wins.

| Order | Condition | Result |
| :-- | :-- | :-- |
| 1 | value is empty, or is an **absolute** path | normalized as given |
| 2 | `<SourceDirectory>/<value>` **exists** | that path |
| 3 | `<ProjectDir>/<value>` **exists** | that path |
| 4 | otherwise | normalized as given — which then fails the extension guard or the compile |

`<SourceDirectory>` is the *Source Directory* project setting, default `<Project>/DShader`. A
relative value is therefore resolved against the DreamShader source tree first and the project
directory second.

### `-All` discovery and ordering

| Step | Rule |
| :-- | :-- |
| 1 | Recursively collect `*.dsm`, `*.dsh`, `*.dsf`, `*.dss`, `*.dsi` and `*.dsp` under every [source root](../language/source-files.md#source-roots) — `<SourceDirectory>`, and the `DShader` folder of every plugin that has one |
| 2 | Drop **everything** under each root's `Packages` folder |
| 3 | Drop `.dsh` headers — they generate no assets and are inlined by their dependents |
| 4 | Sort: `.dsf` function files first (rank 0), then every other kind (rank 1); ties broken by case-insensitive path comparison |

Step 4 is a guarantee, not an accident: function assets referenced by a material must exist before
that material is generated, and the two-rank sort provides that within a single run. Step 2 has a
sharp edge for `.dsf` files that live in packages; see [Packages](packages.md#source-file-enumeration).
A `.dsp` needs no rank of its own *(2.1.0)*: a material one of its passes names that is missing or older
than its source is compiled first by the `.dsp`'s own compile.

### `-Define` *(since 1.9.0)*

Each occurrence contributes one entry to the [define table](../language/preprocessor.md#where-defines-come-from)
this run compiles against. The option is repeatable — later occurrences add names rather than
replacing the set.

| Written as | Defines |
| :-- | :-- |
| `-Define=FOO=1` | `FOO` to `1` |
| `-Define=PROFILE=cinematic` | `PROFILE` to the string `cinematic` |
| `-Define="NOTE=two words"` | `NOTE` to `two words` — quote the whole argument, not just the value |
| `-Define=CI_BUILD` | `CI_BUILD` to an **empty** value: a bare marker, for which `defined(CI_BUILD)` is `1` and `#if CI_BUILD` is true |
| `-D=FOO=1` | the same as `-Define=FOO=1` — `-D` is the short spelling |

The name is everything up to the **first** `=` in the option's value, so a value may itself contain
`=`. Names are case-sensitive.

> [!WARNING]
> `-Define` is one of the few options that must be read off the **raw** command line rather than
> through the commandlet's usual parameter map, and the reason is a trap in the engine rather than a
> preference. `UCommandlet::ParseCommandLine`'s four-argument overload does not put `-Foo=Bar` into
> both lists: it *removes* it from `Switches` and moves it into `Params`. So scanning `Switches`
> finds no `-Define` at all, and `Params` is keyed by option name, which collapses
> `-Define=A=1 -Define=B=2` into a single entry. Either route silently drops every define but one.

The command line is the highest-precedence define tier, so it overrides *Preprocessor Defines* in the
project settings and anything a module registered in C++ — with one exception: a name beginning with
`DS_` is reserved for the builtins and is dropped with a warning, because a run that lies about the
engine version or about Substrate does not produce a different material, it produces a graph the
engine cannot compile at all.

> [!NOTE]
> Defines are part of the [build key](../generation/caching.md#it-is-a-build-key-not-just-a-source-hash),
> so a run with a different `-Define` set rebuilds the sources that read a changed name **without**
> `-Force`, and still skips the ones that do not read it. That is what makes a per-configuration CI
> sweep affordable: one run per define set, each rebuilding only what its own switches touch.

### Per-file guard

Each file in the compile list is checked before compiling: a path that is not a DreamShader source,
or that *is* a `.dsh` header, logs `DreamShader compile requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}`,
marks the whole run failed, and the loop **continues** with the remaining files. One bad file therefore
does not prevent the rest from compiling, but the process still exits `1`.

### Result messages

Each file's compile result is logged verbatim — at `Display` on success, at `Error` on failure. When
one file produces several assets, the messages are joined with newlines.

| Message | Outcome |
| :-- | :-- |
| `Generated {Kind} {AssetPath} from {SourceFile}.` | a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` asset generated; `{Kind}` is the block keyword |
| `Generated {AssetPath} from {SourceFile}.` | material generated (Graph backend) |
| `Generated DreamShader thin-custom material {AssetPath} from {SourceFile}.` | material generated (ThinCustom backend) |
| `Generated PassPipeline {ObjectPath} from {SourceFile}.` | a `.dsp` built *(2.1.0)*. A line for each render target of its exported buffers follows |
| `Generated RenderTarget {ObjectPath} from {SourceFile}.` | the render target of an exported buffer, made or reused by its pipeline's build and saved with it *(2.1.0)* |
| `Skipped {AssetPath} from {SourceFile}; source hash is unchanged (build key {BuildKey}).` | hash match — pass `-Force` to regenerate |
| `Generated DreamShader helper include '{Path}' from {SourceFile}.` | the file produced only a generated `.ush` |
| `DreamShader file '{Path}' contains VirtualFunction declarations only; no assets were generated.` | success, nothing to write |
| `DreamShader file '{Path}' contains GraphFunction declarations only; no assets were generated.` | success, nothing to write |
| `DreamShader file '{Path}' did not contain any material, ShaderFunction, ShaderLayer, or ShaderLayerBlend assets to generate.` | **failure** |
| `DreamShader header '{Path}' does not generate assets directly. Recompile dependent .dsm or .dsf files instead.` | **failure** — a `.dsh` reached the generator |
| `{Path}: .dsf files cannot define top-level Shader blocks.` | **failure** |

A message ending in ` (virtual)` indicates a transient asset; the commandlet never produces those.

## `decompile` / `export`

| Option | Aliases | Type | Required | Default | Meaning |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **`-Asset=<object-path>`** | — | string | one of the two | — | The asset to decompile |
| **`-SourceFile=<path>`** | — | string | one of the two | — | Decompile every asset that source builds into one file |
| `-Out=<path>` | `-Output=<path>` | string | no | computed | Destination file |
| `-Format=<Dss\|Legacy\|Auto>` | — | enum | no | `Auto` | `Auto` lets the extension of `-Out` decide: `.dsm` / `.dsf` is 1.x text, anything else 2.0. A format that contradicts the extension is `DSH9085`. |
| `-KeepAssetPath` | — | flag | no | off | Write `/// @name` with the asset's own path when the output file would otherwise name another |
| `-Readable` | — | flag | no | off | Prefer HLSL sugar over class-exact node calls (a rebuilt graph may then differ in node classes), and read the [Substrate sugar](../language-v2/substrate.md#decompiling) back (`A * w`, `Transmittance = …`) |
| `-DiagnosticsOut=<file>` | — | string | no | — | The decompile's diagnostics as JSON (schema `dreamshader-diagnostics`) |

### Asset path normalization

| Step | Rule |
| :-- | :-- |
| 1 | Every `\` becomes `/` |
| 2 | If the path starts with `/` and contains **no** `.`, the short name is appended: `/Game/Path/Asset` → `/Game/Path/Asset.Asset` |

The normalized path is loaded first. If that fails **and** normalization actually changed the string,
the raw (quote-stripped) input is retried unchanged.

### Supported asset classes

| Class | Emits | Default destination |
| :-- | :-- | :-- |
| `UMaterial` | `.dsm` | `<SourceDirectory>/Decompiled/Materials/<package path>.dsm` |
| `UMaterialFunction` | `.dsf` | `<SourceDirectory>/Decompiled/Functions/<package path>.dsf` |
| `UMaterialFunctionMaterialLayer` | `.dsf` | `<SourceDirectory>/Decompiled/Layers/<package path>.dsf` |
| `UMaterialFunctionMaterialLayerBlend` | `.dsf` | `<SourceDirectory>/Decompiled/LayerBlends/<package path>.dsf` |
| `UDreamPassPipeline` *(2.1.0)* | `.dsp` — 2.0 text only: `-Format=Legacy`, or an `-Out` ending in `.dsm` / `.dsf`, is refused, and an `-Out` with any extension but `.dsp` is [`DSH9210`](../diagnostics/DSH9xxx.md) | `<SourceDirectory>/Decompiled/Pipelines/<package path>.dsp` |
| anything else | — | error |

Path segments are sanitized: control characters and `< > : " / \ | ? *` become `_`; an empty folder
segment becomes `Folder<N>` and an empty asset segment becomes `Asset<N>`. Output is written UTF-8
without a BOM. `-Out` bypasses the computed destination entirely — the directory is created if
needed. Full decompiler behaviour is on [Decompiler](decompiler.md).

## `dump-graph` *(since 1.9.0)*

A **developer tool**, not part of a normal build. It generates each source the way `compile` does and
then, instead of saving an asset, writes one canonical JSON file describing the graph that generation
produced: node classes, their reflected properties, every connection, and pin order.

The reason it exists is [parity](../contributing/testing.md#graph-baseline-since-190). A JSON capture taken
before a compiler change is the only ground truth a rewritten compiler can be held to, source file by
source file — the machine-readable version of "decompile both and diff the text", without the
decompiler's own opinions in the middle. Two captures are compared with an ordinary text diff.

| Option | Aliases | Type | Required | Default | Meaning |
| :-- | :-- | :-- | :-- | :-- | :-- |
| **`-Source=<path>`** | `-File=<path>` | string | one of `-Source` / `-File` / `-All` | — | Dump exactly one source file |
| **`-All`** | — | flag | one of `-Source` / `-File` / `-All` | off | Dump every project DreamShader source |
| `-Out=<dir>` | `-Output=<dir>` | string | no | `<Project>/Saved/DreamShader/GraphBaseline` | Root of the dump tree |
| `-Force` | — | flag | no | off | Accepted and **ignored**; a dump always regenerates |
| `-Define=<NAME>[=<value>]` | `-D=<NAME>[=<value>]` | string, repeatable | no | — | Same define tier as `compile`; a dump is per define set |

`-Source` resolution, `-All` discovery and the `.dsf`-before-`.dsm` ordering are literally the same
code as [`compile`](#-all-discovery-and-ordering). That is deliberate: a baseline that covered a
different set of sources than the compiler does would report a missing file as a difference.

### It never writes an asset

A baseline capture over a project must not be a rebuild-and-save of every generated `.uasset` just to
read them back. `dump-graph` therefore runs with the generator's write ownership switched off for the
whole sweep, which makes generation refuse — *before* the old graph is torn down — any asset that
would persist.

> [!WARNING]
> The consequence is that an asset which **already exists on disk is dumped as it stands, not
> regenerated**. On a compiled tree those are the same graph. On a stale one they are not, silently.
> Run `compile -All -Force` first whenever the sources may have moved since the assets were written.
> The run counts these and says so:
> `{N} of them already exist on disk, so dump-graph read them as they stand instead of rebuilding them…`

Sources whose asset has no file behind it — the whole test corpus, and anything not yet compiled —
are generated in memory and dumped from that, so nothing on disk is touched either way.

### Output layout

```text
<Out>/<root>/<source path relative to that root>.<asset leaf>.graph.json
```

`<root>` is the [source root](../language/source-files.md) that owns the file — `Project`, or the
owning plugin's name; `DreamShaderPlugin` for the plugin's own test corpus, and `External` for a file
under none of them (whose relative path degrades to its file name). The source's extension is kept in
the middle of the name on purpose: `M_Foo.dsm` and `M_Foo.dsf` would otherwise collide. Path
separators inside the relative path stay separators, so the dump tree mirrors the source tree; the
characters a file name cannot hold become `_`.

One file per **asset**, not per source: a `.dsf` declaring three `ShaderFunction` blocks writes three.

### What the JSON contains

| Key | Meaning |
| :-- | :-- |
| `schema` | Dump format version — currently `1`. Two captures are only comparable at the same number |
| `plugin` | The plugin's `VersionName` |
| `source` | `{ root, path }` — the source root's name and the path relative to it |
| `asset` | The generated asset's object path |
| `kind` | `Material` / `MaterialFunction` / `MaterialLayer` / `MaterialLayerBlend` / `ThinCustomInstance` / `MaterialInstance` / `PassPipeline` *(2.1.0)* |
| `backend` | `Graph`, or `ThinCustom` for an instance material; absent for a material instance and a pass pipeline |
| `nodes` | Every expression in the graph, in canonical order (below) |
| `properties` | *(materials)* connected material property → `{ from, output, mask }`, keyed by `EMaterialProperty` enumerator name |
| `settings` | The curated `Settings` set — `Domain`, `ShadingModel`, `BlendMode`, the usage/render bools and `MaterialDecalResponse` for a material; `Usage`, `Description`, `bExposeToLibrary`, `LibraryCategories` for a function |
| `inputs` / `outputs` | *(functions)* name, type, sort priority, description, optional flag, preview value |
| `instance` | *(ThinCustom)* `parent.kind` and `parameters` — the overrides, static switches and static component masks |

For a ThinCustom instance the `nodes`, `properties` and `settings` describe the **hidden base
material** the instance parents to, which is where the graph actually lives. The base's own object
path is deliberately absent: it is a transient object in the editor and a subobject of the instance on
disk, so the string differs between two captures of an identical graph.

### A pipeline's dump

*(since 2.1.0)* A `.dsp` product has no graph, so its dump is the `UDreamPassPipeline` as the runtime
reads it, every enumeration spelled the way the `.dsp` spells it. The run's line says `nodes` but counts
the passes. The render targets of its exported buffers get no file of their own.

| Key | Meaning |
| :-- | :-- |
| `pipeline` | `order`, `defaultInjection`, `views`, `requires`, `enabled` (the bool parameter, or `null`) |
| `parameters` | in declaration order: `name`, `default` (`type` and `value`), `group`, `description`, `slider` (`[min, max]` or `null`), `sortPriority` |
| `buffers` | in declaration order: `name`, `format`, `resolution`, `size` for a fixed-size buffer or `scale` otherwise, `clear` (four floats, or `null` for `Clear = None`), `mips`, `history`, `export`, `exportTarget` (the render target's object path, or `null`), `description` |
| `passes` | in declaration order — execution order within an injection point: `name`, `kind`, `injection`, `enabled`, `reads` / `writes` (`slot`, `buffer`, `previous`), `params` (`target`, `source`, and the constant, or the parameter with `multiplier` and `offset`), `description`, and `settings` — only the keys of the pass's own kind, slot numbers included |
| `slots` | every HLSL pass again: `pass`, `kind` (`compute` or `pixel`), `slot` — the number a rebuild of another pipeline can move |

The write guard covers the slot registry too: a pipeline with no file behind it is built in memory with
the slots the registry already records for it (`-1` where it records none), and neither the registry
nor a snapshot is written.

Each node is `{ id, class, props, inputs }`, plus `reroute` on a named-reroute usage. `id` is `n0`,
`n1`, … — positional, never an engine identifier. `class` is the expression class's full path name.
`inputs` lists only **connected** pins, in pin order, as `{ index, name, from, output, mask }`, where
`mask` is an `"rgba"`-style string or `null`. A named-reroute usage links to its declaration **by
name**, because the name is the declaration's identity in the source while its GUID is reissued on
every rebuild.

An input's `name` is the engine's own, with one exception *(since 2.0.0)*: `BreakMaterialAttributes`,
`GetMaterialAttributes` and `SetMaterialAttributes` name their inputs with translated text, so theirs
are written as `MaterialAttributes` and the attribute's own name (`BaseColor`, `FrontMaterial`) -- a
capture must not depend on the language of the editor that took it. For the same reason a Get and a
Set carry their attribute lists in `props` as names, `AttributeGetTypes` and `AttributeSetTypes`,
although every other `FGuid` is left out: for these two nodes the list is the behaviour.

### The exclusion list

Determinism beats completeness: a fingerprint that changes between two runs of the *same* compiler
cannot say anything about two *different* ones. Anything that is not a property of the graph is
therefore left out rather than normalized.

| Excluded | Why |
| :-- | :-- |
| The object name, and the `_12` numeric suffix the engine appends to it | Assigned when a node is recreated; node identity is positional instead |
| `MaterialExpressionGuid`, `VariableGuid`, `DeclarationGuid` — every `FGuid` | Reissued on every regeneration |
| `MaterialExpressionEditorX` / `EditorY` | Node coordinates; regeneration reassigns them from the `Layout` section anyway |
| `NodeColor` and every other colour | A named reroute seeds its colour from its own **path name**, so a recreated node repaints itself |
| `Desc`, `bCollapsed`, `bCommentBubbleVisible`, `bHidePreviewWindow`, `bShowOutputNameOnPin`, `bRealtimePreview` | Presentation, not behaviour |
| `GraphNode`, `Material`, `Function`, `SubgraphExpression` | Transient, or back-pointers into the owner |
| `FExpressionInput` properties | Their exported text is the connected object's path; connections are recorded structurally by node id instead |

`props` is otherwise every reflected `UPROPERTY` the plugin already counts as *content* — the same
rule the [divergence digest](../generation/divergence.md) uses, so the two cannot disagree about what
a hand edit is. Parameter name, group and sort priority, sampler type, texture object path, static
switch default, `Custom` `Code` / `OutputType` / `AdditionalOutputs` / `IncludeFilePaths`, the
function-call target path, `ComponentMask` channel flags, constants and `Panner`/`Time` settings are
all in. Values are written whether or not they equal the class default — omitting a value because it
happened to match a CDO would change the dump's shape when an engine default moved.

Floats are formatted `%.9g`; a non-finite value degrades to a string, since JSON cannot spell one.

### Node order

`nodes` is a **post-order depth-first walk from the sinks**, so a node is numbered only after
everything feeding it is, and `from` always names an earlier entry.

| Step | Rule |
| :-- | :-- |
| 1 | Sinks. For a material: every connected material property input, in `EMaterialProperty` enumerator order (for a ThinCustom instance, the hidden base's). For a function: its `FunctionOutput` expressions in `SortPriority` order, declaration order breaking ties |
| 2 | From each sink, depth-first over that node's **inputs in pin order**; a named-reroute usage follows its declaration as if it were an input |
| 3 | Everything unreached — dead nodes, `CustomOutput` sinks, unused function inputs — sorted by class path, then by the node's own `props`, then by array position |

The engine's own `GetExpressions()` order is creation order: a property of the compiler that built the
graph rather than of the graph, and exactly what two compilers cannot be expected to agree on. Step 3's
final tiebreak is the one rule two compilers need not match; it exists so that a single capture is
reproducible.

### Diffing two captures

```powershell
git diff --no-index -- I:/Baseline/before I:/Baseline/after
```

`--no-index` diffs two directory trees that are not in a repository, and the layout is designed for
it: same tree shape, sorted keys, one value per line, LF endings. A file present on one side only is a
source whose asset set changed. Every other hunk is a behavioural difference, and needs either a fix
or an entry in the parity log.

Output is UTF-8 without a BOM, two-space indents, keys sorted case-sensitively at every level, LF line
endings and a trailing newline.

## `dump-layout`

*(since 2.0.0)* Draws where a layout computed on the IR would put the nodes of every product of a source,
as one SVG per product and style, without building an asset. It is how to look at **Blocks**, **Source
Bands** and **Layered** before choosing a [Graph Layout Style](../generation/graph-layout.md#layout-styles).
For `Blocks` the picture shows the boxes and the named reroutes between them: a declaration beside the
value, a usage in every box that reads it, and no wire from one box to another.

| Option | Default | Meaning |
| :-- | :-- | :-- |
| `-Source=<path>` / `-All` | one is required | which sources |
| `-Style=` | `All` | `Blocks`, `SourceBands`, `Layered`, or `All` for one picture of each. `Classic` is the 1.x layout, which works on the finished graph and cannot be drawn from the IR ([`DSH9041`](../diagnostics/DSH9xxx.md)). |
| `-Out=<dir>` | `<Project>/Saved/DreamShader/Layout` | root of the output tree, laid out `<root name>/<path relative to the root>` like `dump-ir`'s |
| `-Json` | off | also write the coordinates — positions, sizes, columns, bands, comment boxes, the count of long edges, and for `Blocks` the boxes, the reroutes between them and the reroutes in front of the outputs — beside each SVG |

Files are named `<source file>.<Product>.<Style>.layout.svg` (and `.json`). Node sizes are estimated
from the IR — a node's pins and its title — so the picture shows the arrangement, not the exact
footprint a live node has. A file that cannot be written is [`DSH9040`](../diagnostics/DSH9xxx.md).

## `fmt`

*(since 2.0.0)* The formatter is the language's own printer over a parse that kept its comments and
blank lines; there is no second opinion about layout anywhere. `fmt` rewrites each file in place.

| Option | Meaning |
| :-- | :-- |
| `-Source=<path>` | one file — any 2.0 source, in any source root |
| `-All` | every `.dss`, `.dsi` and `.dsh` under the **writable** source roots — the project's own; a plugin ships its sources as they are, and naming one of its files is how to format it anyway |
| `-Check` | write nothing; every file that would change is [`DSH9046`](../diagnostics/DSH9xxx.md) and the run fails — the form for CI |
| `-Out=<dir>` | write the formatted copies under this directory, mirroring the source tree, instead of over the sources |

What it guarantees, per file, before it writes a byte: the formatted text **parses**, to the **same
declarations**, with **every comment** the file had, and formatting it **again** changes nothing. A
file that fails any of those is left alone and reported as [`DSH9044`](../diagnostics/DSH9xxx.md) — a
fault of the formatter, never of the file. The file's own line terminator is kept, so a format is
never a whole-file diff.

Two kinds of file are left as they are, and neither is an error:

| File | Why | Code |
| :-- | :-- | :-- |
| one with 1.x declarations — a `.dsm`, a `.dsf`, a `.dsh` that still has `Function` blocks | what the printer writes for those is 2.0 text, and turning 1.x into 2.0 is [`migrate`](migrate.md), which proves far more before it writes | `DSH9042` (info) |
| one that uses `#if` outside a `/// @custom` body | the parser reads preprocessed text and `fmt` reads the file as it is on disk; reprinting one side of an `#if` would delete the other | `DSH9043` (info) |

The run ends with one line: `DreamShader fmt: 3 rewritten, 41 already formatted, 2 left alone, 0 failed
of 46 file(s). RESULT=OK`.

## `list-generated`

*(since 2.0.0)* Names every asset the sources build — front end, binder and destination rules, and
nothing after them: no asset is built, loaded or saved. It is what ignore rules and P4 typemaps are
written from; [Source control](../generation/source-control.md) has the recipes.

| Option | Default | Meaning |
| :-- | :-- | :-- |
| `-Source=<path>` / `-All` | one is required | which sources; `-All` covers plugin source roots too |
| `-As=` | `Packages` | `Packages` — long package names; `Files` — paths relative to the project directory; `GitIgnore` — the same paths anchored with `/`, under a header comment; `Json` — every field (schema `dreamshader-generated-assets`, version 1) |
| `-Out=<file>` | the log | write the list here. A script should always pass it: the log is for reading. |
| `-IncludeEphemeral` | off | also list a ThinCustom material that is memory-only right now; it has no file, so it is left out of a list ignore rules are written from |

A `.dsp` contributes two kinds of row *(2.1.0)*: its pipeline (kind `PassPipeline`) and the render target
of every exported buffer of a float or normalized format (kind `PassExportTarget`, `<Pipeline>_<Buffer>`
in the pipeline's folder). The slot registry and snapshots under `<DShader>/.dreampass/` are sources, not
generated assets, and are not listed — see [Source control](../generation/source-control.md#notes).

A source that does not compile still contributes the products that were established before the
failure, and the run ends `RESULT=FAILED` — a list is never silently short. Assets outside the project
directory (an engine-level plugin's content) have no project-relative path; `Files` and `GitIgnore`
say how many were left out ([`DSH9049`](../diagnostics/DSH9xxx.md)). An unknown `-As` is `DSH9048`; a
list that cannot be written is `DSH9047`.

## `.dsp` sources

*(since 2.1.0)* A [Custom Pass pipeline](../language-v2/passes.md) is a compilable source like the others,
with one product — a `UDreamPassPipeline` — and no graph. What each verb does with one:

| Verb | On a `.dsp` |
| :-- | :-- |
| `compile` | Builds the pipeline and the render target of every exported buffer, and gives each HLSL pass its slot: planned, [pre-checked](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine), its snapshot and the registry written under `<DShader>/.dreampass/` (see [`pass-registry`](#pass-registry)). A material one of its passes names that is missing or older than its source is compiled first. A commandlet recompiles no shader; the next editor start compiles the changed registry. Below UE 5.8 the compile is refused, `DSH8300` |
| `check` | Reads every material and `.usf` the passes name and makes every check the binder makes. Compiles nothing first, builds nothing, and compiles no HLSL |
| `check -Shaders` | Builds and saves the pipeline as `compile` does, then pre-checks every HLSL pass in its slot — see [Shader check of a pipeline](#shader-check-of-a-pipeline) |
| `dump-ir` | The product as text: the pipeline's keys, one line per parameter and buffer, and per pass its keys, its kind's settings and its bindings, as the binder settled them — `(inferred)` marks a buffer resolution the source did not write, `(from the shader)` a group size read off `[numthreads]`. `-Json` adds the JSON form |
| `dump-layout` | Writes nothing for it: a pipeline has no graph to lay out. Not an error |
| `index` | The symbol index, with a `pipeline` section: the `#pragma pipeline` values as bound, the uniforms, the buffers and the passes with their bindings and spans, and the injection-point and format spellings an editor completes from |
| `fmt` | The printer's layout, with the same guarantees as for a `.dss`; `-All` includes the `.dsp` files of the writable source roots |
| `list-generated` | The pipeline and the render targets of its exported buffers — see [`list-generated`](#list-generated) |
| `dump-graph` | One JSON for the pipeline — see [A pipeline's dump](#a-pipelines-dump) |
| `decompile` | The way back: `-Asset` naming a pipeline, or `-SourceFile` naming a `.dsp`, writes a `.dsp` — see [Decompiler](decompiler.md#pipelines-dsp) |

### Shader check of a pipeline

`check -Shaders` on a `.dsp` compiles no material: a pipeline has none, and the materials its passes name
are checked by the sources that build them. What can fail to compile is an HLSL pass, and that is
pre-checked in its global shader slot instead — the pre-check a compile runs, with three differences:

- **Every HLSL pass, changed or not.** A compile pre-checks only the passes whose snapshot changed. The
  check plans the slots against the registry as it stands — a pass that has a slot keeps it, a new one is
  checked in the slot it would take — and commits nothing of that plan.
- **The `-Platform` formats**, when the run names any: `SM6` is `PCD3D_SM6`, `SM5` `PCD3D_SM5`, `ES3_1`
  `PCD3D_ES3_1`, `VULKAN_SM6`, `VULKAN_SM5` and `METAL_SM5` the `SF_` formats, and any other token is
  taken as a shader format name. A token that names no shader format this machine can compile — a
  target-platform name such as `Windows` among them — is not pre-checked, with
  [`DSH8324`](../diagnostics/DSH8xxx.md) saying so. Without `-Platform`, the
  formats a compile pre-checks for: those of the active feature levels and every format the active target
  platforms target, with [`DSH8324`](../diagnostics/DSH8xxx.md) for one this machine has no compiler for.
  `-Quality` and `-Timeout` do not apply to slots.
- **Its errors count as shader errors** in the summary line. A compile error is `DSH8322`, at the line of
  the `.usf` that caused it; a `.dsp` with no HLSL pass reports `DSH8339` (info) and passes.

## `pass-registry`

*(since 2.1.0)* The Custom Pass HLSL slot registry from the command line. Every HLSL pass of every `.dsp`
— a `compute` pass, or a `fullscreen` pass with `Shader =` — runs in one **slot** of the global shaders
`FDreamPassCS` and `FDreamPassPS`, and the files that map slots to passes are committed with the sources
([HLSL passes](../runtime/hlsl.md#how-the-hlsl-gets-into-the-engine)):

```text
<DShader>/.dreampass/
├─ Registry.json          the record the other files are written from
├─ RegistryCompute.ush    one section per compute slot; what FDreamPassCS includes
├─ RegistryPixel.ush      the same for FDreamPassPS
└─ Slots/C00/, P03/, …    one snapshot per slot that has one
```

There is one registry per project, under the project's *Source Directory*; a plugin's `.dsp` takes its
slots in it like any other. `Registry.json` (schema `dreamshader-pass-registry`, version `1`) holds two
arrays, `compute` and `pixel`, with one entry per slot: `slot`, `pipeline` (the object path), `pass`,
`kind` (`compute` or `fullscreen`), `source` (the `.dsp`, project-relative), `shader` (project-relative),
`entry`, `hash`, `formats`, `files` (the snapshot's, relative to the slot directory) and `section` (the
text of the slot in its registry file).

Compiles keep the registry current on their own. The verb is for what a compile cannot see: a `.dsp`
that was deleted or renamed, a merge that broke `Registry.json`, a checkout that lost snapshot files.

| Option | Effect |
| :-- | :-- |
| *(none)* | Lists every slot and what it is. Writes nothing |
| `-Gc` | Frees every `PipelineGone` and `PassGone` slot |
| `-Rebuild` | Moves an unreadable `Registry.json` aside, compiles every `.dsp` again, frees the garbage and rewrites the registry files. `-Gc` beside it adds nothing |

The options follow the [argument rules](#argument-syntax): `-Gc`, `--gc` and `-Gc=true` are one switch,
and `-Rebuild` wins when both are given. The `dsc.ps1` switches are `-Gc` and `-Rebuild`; from a
PowerShell prompt, `./dsc.ps1 pass-registry --gc` is not the switch — PowerShell binds `--gc` to the
script's positional target, which this verb ignores, and the run only lists.

### The listing

```text
LogDreamShader: Display: DreamShader pass-registry: C:/Projects/MyGame/DShader/.dreampass/Registry.json
LogDreamShader: Display:   C00  Live             /Game/Passes/CP_Highlight.CP_Highlight  pass 'Blur'  DShader/Passes/BoxBlur.usf : BlurCS  [PCD3D_SM5, PCD3D_SM6]
LogDreamShader: Display:   C01  PipelineGone     /Game/Passes/CP_Old.CP_Old  pass 'Simulate'  DShader/Passes/Old.usf : MainCS  [PCD3D_SM6]
LogDreamShader: Display: DreamShader pass-registry: 2 of 32 compute slot(s), 0 of 16 pixel slot(s) taken; 1 to collect with -Gc. RESULT=OK
```

One line per slot, the compute table first, each in slot order:

| Column | |
| :-- | :-- |
| `C07` / `P03` | the table — compute or pixel — and the slot number: the slot directory's name |
| state | below |
| pipeline | the `UDreamPassPipeline`'s object path |
| `pass '<Name>'` | the pass |
| `<shader> : <entry>` | the shader file, project-relative, and the entry; `-` for one the record does not have |
| `[<formats>]` | the shader formats the snapshot passed its pre-check for. A format the project targets since is one the snapshot was never checked for |

| State | Meaning | `-Gc` |
| :-- | :-- | :-- |
| `Live` | its pipeline's `.dsp` runs that pass in HLSL in that table, and the snapshot is on disk | keeps it |
| `Reserved` | recorded without a snapshot that passed a pre-check: the slot compiles to the empty stub, and its pass does nothing until its `.dsp` compiles again | keeps it |
| `SnapshotMissing` | the snapshot's files are not on disk — a `Slots` folder that was not committed. The next compile of its `.dsp` writes them; until then a process that loads the DreamShaderPass module takes the slot's section out of the registry file at startup, logged as an error, so the global shaders still compile. `DSH9208` | keeps it |
| `PipelineGone` | no `.dsp` under the source roots builds its pipeline any more. When the pipeline asset still exists, `DSH9201` warns that its pass still points at the slot: once the slot is freed and given to another pass, that pass's shader is what the old asset would run | **frees it** |
| `PassGone` | its pipeline's `.dsp` compiles, and no longer runs that pass in HLSL in that table | **frees it** |
| `Unknown` | its pipeline's `.dsp` does not compile far enough to tell. `DSH9209` | keeps it |

To tell `Live` from `PipelineGone`, `PassGone` and `Unknown`, the listing runs the front end of every
`.dsp` that owns a slot; nothing is built or written. A slot is garbage only on positive evidence — no
source builds its pipeline, or its pipeline's source compiles and has no such pass — so a source that
does not compile keeps every slot it has.

### `-Gc`

Frees every `PipelineGone` and `PassGone` slot — its entry in `Registry.json`, its section in the registry
file, its snapshot — and deletes every slot directory nothing names. Each freed slot is an info,
`DSH9202`, after the listing's warnings. Nothing is recompiled in the commandlet; the next editor start
compiles the changed registry.

```text
LogDreamShader: Display: DreamShader pass-registry -Gc: 1 slot(s) freed. RESULT=OK
```

Run it after deleting or renaming a `.dsp`: its slots stay taken until they are collected, and a compile
that finds every slot of a table taken (`DSH8316`) names this verb.

### `-Rebuild`

| Step | |
| :-- | :-- |
| 1 | A `Registry.json` that does not parse — a merge conflict left in it is the usual reason — is moved aside to `Registry.json.unreadable` (`DSH9203`), and an empty registry is written so the compiles below assign every slot afresh. One that parses is left as it is |
| 2 | Every `.dsp` under the source roots, `Packages` folders excluded, is compiled in path order, forced, as `compile -Force` would: each plans its slots against the registry and writes the snapshots it is missing. One that fails is `DSH9204` and keeps the slots it had; with no `.dsp` at all the step is `DSH9206` (info) |
| 3 | The garbage is collected, as `-Gc` does, without its missing-snapshot warnings |
| 4 | `RegistryCompute.ush` and `RegistryPixel.ush` are written from `Registry.json` as it stands. A slot whose snapshot files are still missing becomes reserved (`DSH8337`), because a registry that includes a missing file fails the global shader compile. Every slot directory nothing names is deleted |
| 5 | For every `.dsp` that failed, each HLSL pass of its existing pipeline asset is checked against the registry: a slot the registry now gives to another pass, or leaves free, is `DSH9205` — until its source compiles, that pass runs whatever the slot holds |

```text
LogDreamShader: Display: DreamShader pass-registry -Rebuild: 4 .dsp compiled, 0 failed, 1 slot(s) freed, 0 slot(s) reserved for a missing snapshot. RESULT=OK
```

It builds and saves the pipelines, as `compile` does. Run it after a merge conflict in `Registry.json` —
a compile refuses to write over a registry that does not parse (`DSH8315`), and so does `-Gc`
(`DSH8335`) — after a checkout that lost snapshot files, or whenever the listing and the sources
disagree. Commit the whole `.dreampass/` folder with the result.

A registry file that cannot be written is `DSH8326`, and one that cannot be moved aside `DSH8336`; a file
that is read-only because it is not checked out is the usual reason for both.

## Argument syntax

Shared by every command. Pinned by the automation tests `DreamShader.Commandlet.Args.SplitAndGet` and
`DreamShader.Commandlet.Args.FlagValues`.

| Rule | Behaviour |
| :-- | :-- |
| Key normalization | trim, then strip **all** leading `-`, then trim again. `-Source`, `--Source` and `Source` are the same key |
| Name matching | case-insensitive — `-source`, `-SOURCE`, `-Source` are equivalent |
| Value normalization | trim, then strip one layer of surrounding quotes, then trim again |
| Assignment split | on the **first** `=`; a value may therefore contain `=` |
| Dashless assignment | bare `Key=Value` (no leading dash) is accepted wherever `-Key=Value` is |
| Search order | the parsed parameter map, then the switch list, then the bare-token list; the first match wins |
| Empty value | `-Source=` resolves to an empty string and is treated as **absent** |
| Missing key | absent |

### Boolean flags

A flag may be written bare or with a value. The value is matched case-insensitively, after the
value normalization above.

| Written as | Result |
| :-- | :-- |
| `-Force` | on |
| `-Force=1`, `-Force=true`, `-Force=yes`, `-Force=on` | on |
| `-Force=0`, `-Force=false`, `-Force=no`, `-Force=off` | off |
| *(not written)* | off |
| `-Force=<anything else>`, an empty `-Force=` included | **error** [`DSH9110`](../diagnostics/DSH9xxx.md#dsh9110) |

An error is not a guess at what was meant. The command logs `DSH9110`, does nothing at all and
exits `1`: `migrate -Check=banana` neither checks nor writes, and `-All=never` selects nothing.
Every command reads all of its flags before it touches a file.

The engine's parser moves every `-Name=value` out of the switch list into the parameter map before
the command sees it, so a flag is looked up in the same order as a value: the map, then the
switches, then the bare tokens (a dashless `Force=true` is a token). A flag a command does not take
is ignored, like any other unknown switch: `dump-graph -Force=banana` runs, because `dump-graph`
never reads `-Force`.

> [!NOTE]
> In 2.1.0 and earlier a flag written with any value was read as **absent**: the lookup searched
> the switch list only, where the engine never leaves `-Name=value`. `-Force=true` did not force,
> and `migrate -Source=X -Check=true` wrote the migration. This page used to say the opposite —
> that an unrecognized value meant on.

## Exit codes

| Code | Condition |
| :-- | :-- |
| `0` | the selected command reported success |
| `0` | `compile -All` resolved an **empty** source list — logged `Warning`, treated as success |
| `1` | no command token and no `Command=` value |
| `1` | unknown command name |
| `1` | `compile` with none of `-Source` / `-File` / `-All` |
| `1` | any per-file guard failure or compile failure during `compile` |
| `1` | `decompile` without `-Asset`, or a load / decompile / write failure |
| `0` | `dump-graph -All` resolved an **empty** source list — logged `Warning`, treated as success |
| `1` | `dump-graph` with none of `-Source` / `-File` / `-All` |
| `1` | any per-file guard failure, generation failure or dump-write failure during `dump-graph` |
| `0` | `pass-registry` listed the registry — whatever the listing found; its warnings do not fail the run *(2.1.0)* |
| `1` | `pass-registry` could not read `Registry.json` (`DSH9200`) |
| `1` | `pass-registry -Gc` could not read `Registry.json` (`DSH8335`) or write a registry file (`DSH8326`) |
| `1` | `pass-registry -Rebuild`: the unreadable `Registry.json` could not be moved aside (`DSH8336`), a `.dsp` did not compile (`DSH9204`), or collecting or rewriting failed |
| `1` | any command: a flag it reads has a value that is neither on nor off (`DSH9110`); nothing was run — see [Boolean flags](#boolean-flags) |

## Notes

- **The editor bridge never runs inside a commandlet.** The editor module returns from startup as
  soon as it detects a commandlet process, so there is no source-directory watcher, no
  auto-compile-on-save, no WebSocket server on port 17864, no `diagnostics.json` writer, no
  `bridge.db`, and no menu registration. The only exception is the cook commandlet, which installs a
  post-engine-init hook. See [Editor bridge](bridge.md).
- **The commandlet writes real packages.** Compilation runs with the transient flag off, so
  `/Game/...` `.uasset` files are created and saved on disk. The interactive editor does the
  opposite: every compile there is memory-only. This is the intended way to materialize a whole
  project's sources in CI. See [Ephemeral materials](../generation/ephemeral.md).
- Because assets are persisted, a commandlet run can leave assets on disk that shadow the editor's
  Ephemeral materials. *Tools ▸ DreamShader ▸ Make Ephemeral* removes them.
- Cooking is a separate commandlet. On the cook **director** only (a process whose `-run=` contains
  `Cook` and that does not carry `-cookworker`), DreamShader materializes every project source as a
  persistent asset before the cook proper. A generation failure there is `Fatal` and aborts the cook:
  `DreamShader cook generation failed for {Count} source file(s); aborting the cook. See the [Cook] Failed entries above.`
- `-Force` bypasses the source-hash skip only; it does not delete anything. See
  [Caching](../generation/caching.md).
- **A `.dsp` compile writes outside `Content/`** *(2.1.0)*: the HLSL slot registry and its snapshots under
  `<DShader>/.dreampass/`, which are sources to commit — see [Source control](../generation/source-control.md#notes).
  A commandlet writes them and recompiles no shader; an editor recompiles the slot shaders as soon as they
  change.
- Adding `-NoDreamShaderEditorBridge` to a non-commandlet automation run (for example
  `-ExecCmds="Automation RunTests …"`) suppresses the bridge there too.

## Diagnostics

Runtime substitutions are shown as `{Placeholder}`. All messages go to `LogDreamShader`.

| Message | Severity | Cause |
| :-- | :-- | :-- |
| *(the usage banner)* | Error | no command token and no `Command=` value |
| `Unknown DreamShader command '{Command}'.` + the usage banner | Error | command is not `compile` / `generate` / `decompile` / `export` |
| *(the usage banner)* | Error | `compile` with neither `-Source` / `-File` nor `-All` |
| `DreamShader commandlet found no source files to compile.` | **Warning** | the resolved source list is empty; the run still exits `0` |
| `DreamShader compile requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}` | Error | the file is not a DreamShader source, or is a `.dsh` header |
| *(the compile result message)* | Display / Error | per-file outcome; see [Result messages](#result-messages) |
| *(the usage banner)* | Error | `decompile` without `-Asset` |
| `DreamShader could not load asset '{AssetPath}'.` | Error | the asset failed to load under both the normalized and the raw path |
| `DreamShader failed to decompile '{LoadPath}': {Error}` | Error | the decompiler reported failure |
| `DreamShader decompile supports Material and MaterialFunction assets only: {AssetPath}` | Error | unsupported asset class (surfaced through the message above) |
| `Decompile did not produce source text.` | Error | decompiler failed with no error text |
| `DreamShader failed to resolve an output file path.` | Error | the destination path resolved empty |
| `DreamShader failed to create output directory '{Directory}'.` | Error | the destination directory could not be created |
| `DreamShader failed to write decompiled source '{Path}'.` | Error | the file could not be written |
| `DreamShader decompiled '{LoadPath}' to '{OutputPath}'.` | Display | success |
| `DreamShader commandlet found no source files to dump.` | **Warning** | `dump-graph` resolved an empty source list; the run still exits `0` |
| `DreamShader dump-graph requires a .dss, .dsi, .dsp, .dsm or .dsf file: {Path}` | Error | the file is not a DreamShader source, or is a `.dsh` header |
| `DreamShader failed to dump '{Path}': {Error}` | Error | generation or the dump failed; see [`DSH9030`–`DSH9034`](../diagnostics/DSH9xxx.md) |
| `Dumped {Kind} {AssetPath} ({N} nodes) to {File}.` | Display | one asset dumped; ` (read from disk; not regenerated)` is appended when the asset already had a file |
| `DreamShader dump-graph wrote {N} graph dump(s) from {M} source file(s) to {Directory}.` | Display | end-of-run summary |
| `{N} of them already exist on disk, so dump-graph read them as they stand instead of rebuilding them (it never writes an asset). Run 'compile -All -Force' first if those sources have changed since.` | **Warning** | see [It never writes an asset](#it-never-writes-an-asset) |
| `DreamShader pass-registry: {Registry.json path}` | Display | the head of a listing *(2.1.0)* |
| `DreamShader pass-registry: {N} of {M} compute slot(s), {N} of {M} pixel slot(s) taken; {N} to collect with -Gc. RESULT=OK` | Display | the end of a listing |
| `DreamShader pass-registry: the registry could not be read. RESULT=FAILED` | Error | after `DSH9200` |
| `DreamShader pass-registry -Gc: {N} slot(s) freed. RESULT={OK\|FAILED}` | Display / Error | the end of `-Gc` |
| `DreamShader pass-registry -Rebuild: {N} .dsp compiled, {N} failed, {N} slot(s) freed, {N} slot(s) reserved for a missing snapshot. RESULT={OK\|FAILED}` | Display / Error | the end of `-Rebuild` |
| `DreamShader pass-registry -Rebuild: the unreadable registry could not be reset. RESULT=FAILED` | Error | after `DSH8336` |
| `DSH9110: '-{Flag}={Value}' is neither on nor off. A flag takes true, 1, yes or on, or false, 0, no or off, and '-{Flag}' alone is on; the command did nothing.` | Error | any command: a flag it reads has another value, an empty one included; see [Boolean flags](#boolean-flags) |
| `DreamShader {verb}: a flag's value is neither on nor off, so nothing was run. RESULT=FAILED` | Error | after `DSH9110`, from `check`, `dump-ir`, `dump-layout`, `index`, `fmt`, `list-generated` and `pass-registry` |

The usage banner, verbatim:

```text
Usage:
  -run=DreamShader compile -Source="C:/Project/DShader/File.dsm" [-Force] [-Define=NAME=VALUE ...]
  -run=DreamShader compile -All [-Force] [-Define=NAME=VALUE ...]
  -run=DreamShader decompile { -Asset="/Game/Path/Asset.Asset" | -SourceFile="C:/Project/DShader/File.dss" } [-Out=<file>] [-Format=Dss|Legacy|Auto] [-KeepAssetPath] [-Readable] [-DiagnosticsOut=<file>]
  -run=DreamShader migrate { -Source="C:/Project/DShader/File.dsm" | -All | -Root=<source root> } [-Check] [-DryRun] [-Out=<dir>] [-NoBackup]
  -run=DreamShader dump-graph { -Source="C:/Project/DShader/File.dsm" | -All } [-Out="C:/Project/Saved/DreamShader/GraphBaseline"]
  -run=DreamShader check { -Source="C:/Project/DShader/File.dss" | -All } [-Shaders] [-Platform=SM6,SM5] [-Quality=High] [-Timeout=120] [-DiagnosticsOut=<file>]
  -run=DreamShader dump-ir { -Source="C:/Project/DShader/File.dss" | -All } [-Out=<dir>] [-Json]
  -run=DreamShader dump-layout { -Source="C:/Project/DShader/File.dss" | -All } [-Style=Blocks|SourceBands|Layered|All] [-Out=<dir>] [-Json]
  -run=DreamShader index { -Source="C:/Project/DShader/File.dss" | -All } [-Out=<dir>]
  -run=DreamShader export-catalog [-Out=<file>]
  -run=DreamShader fmt { -Source="C:/Project/DShader/File.dss" | -All } [-Check] [-Out=<dir>]
  -run=DreamShader list-generated { -Source="C:/Project/DShader/File.dss" | -All } [-As=Packages|Files|GitIgnore|Json] [-Out=<file>] [-IncludeEphemeral]
  -run=DreamShader pass-registry [-Gc | -Rebuild]
decompile writes 2.0 text by default -- a .dss for a Material or MaterialFunction, a .dsi for a
MaterialInstanceConstant, a .dsp for a DreamPassPipeline; -Format=Legacy, or an -Out ending in .dsm
or .dsf, writes the 1.x text (a DreamPassPipeline has none).
-SourceFile decompiles every asset that source builds into one file; -KeepAssetPath keeps each asset's own path.
migrate rewrites 1.x sources (.dsm, .dsf, .dsh) as .dss; -Check verifies the rewrite and writes nothing.
-All takes the writable source roots; -Root names one root, a plugin's included, by its name or its plugin's.
fmt rewrites 2.0 sources in the printer's layout (-All: the writable roots); -Check writes nothing and fails
when a file would change. list-generated names every asset the sources build, building nothing.
compile, dump-graph, check, dump-ir and index take any compilable source -- .dss, .dsi,
.dsp, .dsm or .dsf; a .dsh header is compiled through the sources that include it.
pass-registry lists the Custom Pass HLSL slots and what each is (Live, Reserved, SnapshotMissing,
PipelineGone, PassGone, Unknown), writing nothing; -Gc frees the PipelineGone and PassGone ones;
-Rebuild compiles every .dsp again, frees the garbage and rewrites the registry files from
Registry.json -- a Registry.json that does not parse is moved aside and every slot assigned afresh.
check writes no asset at all; -Shaders is the exception -- a shader
compile needs a real material, so it builds and saves the products the way compile
does, then reports HLSL errors as stage: shader.
dump-graph is a developer tool: it writes one canonical JSON per generated asset and
never writes an asset itself. Compile the tree first if its sources have changed.
-Define (short form -D) may be repeated; -Define=NAME with no value is a bare marker that
defined(NAME) sees. Names starting with DS_ are reserved for the built-in constants.
A flag is written bare (-Force) or with true/false, 1/0, yes/no or on/off (-Force=false);
any other value is an error (DSH9110), and the command does nothing.
```

## Example

Compile a single source, bypassing the hash skip:

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader compile -Source="C:/Projects/MyGame/DShader/Materials/M_Sample.dsm" -Force `
  -unattended -nopause -nosplash -stdout -log
```

Compile every project source — `.dsf` first, then `.dsm` — as a CI gate:

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader compile -All -Force `
  -unattended -nopause -nosplash -stdout -log
```

Compile the whole tree twice, once per branch of a `#if`, as the CI gate that keeps an inactive
branch from rotting — an untaken branch is never parsed, so nothing else checks it:

```powershell
foreach ($Legacy in 0, 1) {
    & $UnrealEditorCmd $Project `
      -run=DreamShader compile -All -Force `
      -Define=MOONTOON_LEGACY_TOON=$Legacy `
      -unattended -nopause -nosplash -stdout -log
    if ($LASTEXITCODE -ne 0) { throw "DreamShader failed with MOONTOON_LEGACY_TOON=$Legacy" }
}
```

Decompile an existing material to a chosen path:

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Projects\MyGame\MyGame.uproject" `
  -run=DreamShader decompile -Asset="/Game/Materials/M_Existing" `
  -Out="C:/Projects/MyGame/DShader/Decompiled/Materials/M_Existing.dsm" `
  -unattended -nopause -nosplash -stdout -log
```

A relative `-Source` is resolved against `DShader/` first:

```powershell
-run=DreamShader compile -Source="Materials/M_Sample.dsm"
```

Capture a graph baseline for the whole project, then compare it against a later one — the two-step
shape matters, because `dump-graph` will not rebuild an asset that already exists on disk:

```powershell
& $UnrealEditorCmd $Project -run=DreamShader compile -All -Force `
  -unattended -nopause -nosplash -stdout -log
& $UnrealEditorCmd $Project -run=DreamShader dump-graph -All -Out="I:/Baseline/before" `
  -unattended -nopause -nosplash -stdout -log

# …change the compiler, rebuild, capture again…

git diff --no-index -- I:/Baseline/before I:/Baseline/after
```

After a merge that conflicted in `DShader/.dreampass/Registry.json`, rebuild the slot registry from the
sources, then list it:

```powershell
& $UnrealEditorCmd $Project -run=DreamShader pass-registry -Rebuild `
  -unattended -nopause -nosplash -stdout -log
& $UnrealEditorCmd $Project -run=DreamShader pass-registry `
  -unattended -nopause -nosplash -stdout -log
```

Console output of a successful two-file `-All` run:

```text
LogDreamShader: Display: Generated ShaderFunction /Game/Functions/MF_Noise from C:/Projects/MyGame/DShader/Functions/MF_Noise.dsf.
LogDreamShader: Display: Generated DreamShader thin-custom material /Game/Materials/M_Sample from C:/Projects/MyGame/DShader/Materials/M_Sample.dsm.
```

## See also

- [Editor bridge](bridge.md) — everything the commandlet deliberately does not start
- [Testing](../contributing/testing.md#graph-baseline-since-190) — the graph baseline `dump-graph` produces
- [Divergence](../generation/divergence.md) — the output digest whose "what counts as content" rule the dump shares
- [Ephemeral materials](../generation/ephemeral.md) — persistent versus transient generation
- [Caching](../generation/caching.md) — the source-hash skip `-Force` bypasses
- [Preprocessor](../language/preprocessor.md) — what `-Define` feeds, and the other four define tiers
- [Asset paths](../generation/asset-paths.md) — how `Name=` and `Root=` become the package path
- [Decompiler](decompiler.md) — the export the `decompile` command drives
- [Packages](packages.md) — why `DShader/Packages` is skipped by `-All`
- [Source files](../language/source-files.md) — `.dsm` / `.dsf` / `.dsh` roles
- [Custom Pass pipelines — `.dsp`](../language-v2/passes.md) — the source `pass-registry` keeps the slots of
- [HLSL passes](../runtime/hlsl.md) — slots, snapshots and the pre-check
- [Project settings](../settings/project.md) — `SourceDirectory`, which path resolution depends on
- [Testing](../contributing/testing.md) — running the automation suite headlessly
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
