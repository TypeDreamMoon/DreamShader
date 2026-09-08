# Commandlet

> [DreamShader](../index.md) » [Tools](index.md) » **Commandlet**

`-run=DreamShader` — a headless entry point that compiles DreamShader sources into persistent assets
and decompiles existing material assets back into source files.

| | |
| :-- | :-- |
| Kind | `UCommandlet` subclass — `UDreamShaderCommandlet`, in the `DreamShaderEditor` module |
| Invocation | `-run=DreamShader` (equivalently `-run=DreamShaderCommandlet`) |
| Commands | `compile`, `generate`, `decompile`, `export`, `dump-graph` *(since 1.9.0)* |
| Exit codes | `0` success, `1` failure |
| Log category | `LogDreamShader` |

## Synopsis

```text
UnrealEditor-Cmd.exe <project>.uproject -run=DreamShader <command> [<option>…]

<command> ::= { compile | generate | decompile | export | dump-graph }

-run=DreamShader { compile | generate } { -Source=<path> | -File=<path> | -All } [-Force]
                                        [-Define=<NAME>[=<value>]]…
-run=DreamShader { decompile | export } -Asset=<object-path> [{ -Out | -Output }=<path>]
-run=DreamShader dump-graph { -Source=<path> | -File=<path> | -All } [{ -Out | -Output }=<dir>]
                                        [-Define=<NAME>[=<value>]]…
```

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
| `decompile` | — | Export a `UMaterial` / `UMaterialFunction` graph to a source file |
| `export` | `decompile` | Identical; alternate spelling |
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
| 1 | Recursively collect `*.dsm`, `*.dsh`, `*.dsf` under `<SourceDirectory>` |
| 2 | Drop **everything** under `<SourceDirectory>/Packages` |
| 3 | Drop `.dsh` headers — they generate no assets and are inlined by their dependents |
| 4 | Sort: `.dsf` function files first (rank 0), then `.dsm` materials (rank 1); ties broken by case-insensitive path comparison |

Step 4 is a guarantee, not an accident: function assets referenced by a material must exist before
that material is generated, and the two-rank sort provides that within a single run. Step 2 has a
sharp edge for `.dsf` files that live in packages; see [Packages](packages.md#source-file-enumeration).

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
or that *is* a `.dsh` header, logs `DreamShader compile requires a .dsm or .dsf file: {Path}`, marks
the whole run failed, and the loop **continues** with the remaining files. One bad file therefore
does not prevent the rest from compiling, but the process still exits `1`.

### Result messages

Each file's compile result is logged verbatim — at `Display` on success, at `Error` on failure. When
one file produces several assets, the messages are joined with newlines.

| Message | Outcome |
| :-- | :-- |
| `Generated {Kind} {AssetPath} from {SourceFile}.` | a `ShaderFunction` / `ShaderLayer` / `ShaderLayerBlend` asset generated; `{Kind}` is the block keyword |
| `Generated {AssetPath} from {SourceFile}.` | material generated (Graph backend) |
| `Generated DreamShader thin-custom material {AssetPath} from {SourceFile}.` | material generated (ThinCustom backend) |
| `Skipped {AssetPath} from {SourceFile}; source hash is unchanged.` | hash match — pass `-Force` to regenerate |
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
| **`-Asset=<object-path>`** | — | string | **yes** | — | The asset to decompile |
| `-Out=<path>` | `-Output=<path>` | string | no | computed | Destination file |

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
| anything else | — | error |

Path segments are sanitized: control characters and `< > : " / \ | ? *` become `_`; an empty folder
segment becomes `Folder<N>` and an empty asset segment becomes `Asset<N>`. Output is written UTF-8
without a BOM. `-Out` bypasses the computed destination entirely — the directory is created if
needed. Full decompiler behaviour is on [Decompiler](decompiler.md).

## `dump-graph` *(since 1.9.0)*

A **developer tool**, not part of a normal build. It generates each source the way `compile` does and
then, instead of saving an asset, writes one canonical JSON file describing the graph that generation
produced: node classes, their reflected properties, every connection, and pin order.

The reason it exists is [parity](../contributing/testing.md#graph-baseline). A JSON capture taken
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
| `kind` | `Material` / `MaterialFunction` / `MaterialLayer` / `MaterialLayerBlend` / `ThinCustomInstance` |
| `backend` | `Graph`, or `ThinCustom` for an instance material |
| `nodes` | Every expression in the graph, in canonical order (below) |
| `properties` | *(materials)* connected material property → `{ from, output, mask }`, keyed by `EMaterialProperty` enumerator name |
| `settings` | The curated `Settings` set — `Domain`, `ShadingModel`, `BlendMode`, the usage/render bools and `MaterialDecalResponse` for a material; `Usage`, `Description`, `bExposeToLibrary`, `LibraryCategories` for a function |
| `inputs` / `outputs` | *(functions)* name, type, sort priority, description, optional flag, preview value |
| `instance` | *(ThinCustom)* `parent.kind` and `parameters` — the overrides, static switches and static component masks |

For a ThinCustom instance the `nodes`, `properties` and `settings` describe the **hidden base
material** the instance parents to, which is where the graph actually lives. The base's own object
path is deliberately absent: it is a transient object in the editor and a subobject of the instance on
disk, so the string differs between two captures of an identical graph.

Each node is `{ id, class, props, inputs }`, plus `reroute` on a named-reroute usage. `id` is `n0`,
`n1`, … — positional, never an engine identifier. `class` is the expression class's full path name.
`inputs` lists only **connected** pins, in pin order, as `{ index, name, from, output, mask }`, where
`mask` is an `"rgba"`-style string or `null`. A named-reroute usage links to its declaration **by
name**, because the name is the declaration's identity in the source while its GUID is reissued on
every rebuild.

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

## Argument syntax

Shared by every command. Pinned by the automation test `DreamShader.Commandlet.Args.SplitAndGet`.

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

A flag may be written bare or with a value. The value is lowercased before matching.

| Written as | Result |
| :-- | :-- |
| `-Force` | on |
| `-Force=` | on *(empty value)* |
| `-Force=1` | on |
| `-Force=true` | on |
| `-Force=yes` | on |
| `-Force=on` | on |
| `-Force=0` | off |
| `-Force=false` | off |
| `-Force=no` | off |
| `-Force=off` | off |
| `-Force=<anything else>` | **on** |

> [!WARNING]
> An unrecognized boolean value evaluates to **on**, not off and not an error. `-Force=banana`,
> `-Force=disable` and `-All=never` all enable the flag. There is no diagnostic. Use the literals in
> the table above.

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

## Notes

- **The editor bridge never runs inside a commandlet.** The editor module returns from startup as
  soon as it detects a commandlet process, so there is no source-directory watcher, no
  auto-compile-on-save, no WebSocket server on port 17864, no `diagnostics.json` writer, no
  `bridge.db`, and no menu registration. The only exception is the cook commandlet, which installs a
  post-engine-init hook. See [Editor bridge](bridge.md).
- **The commandlet writes real packages.** Compilation runs with the transient flag off, so
  `/Game/...` `.uasset` files are created and saved on disk. The interactive editor does the
  opposite: every compile there is memory-only. This is the intended way to materialize a whole
  project's sources in CI. See [In-memory materials](../generation/in-memory.md).
- Because assets are persisted, a commandlet run can leave assets on disk that shadow the editor's
  in-memory materials. *Tools ▸ DreamShader ▸ Clean Persisted Generated Assets* removes them.
- Cooking is a separate commandlet. On the cook **director** only (a process whose `-run=` contains
  `Cook` and that does not carry `-cookworker`), DreamShader materializes every project source as a
  persistent asset before the cook proper. A generation failure there is `Fatal` and aborts the cook:
  `DreamShader cook generation failed for {Count} source file(s); aborting the cook. See the [Cook] Failed entries above.`
- `-Force` bypasses the source-hash skip only; it does not delete anything. See
  [Caching](../generation/caching.md).
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
| `DreamShader compile requires a .dsm or .dsf file: {Path}` | Error | the file is not a DreamShader source, or is a `.dsh` header |
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
| `DreamShader dump-graph requires a .dsm or .dsf file: {Path}` | Error | the file is not a DreamShader source, or is a `.dsh` header |
| `DreamShader failed to dump '{Path}': {Error}` | Error | generation or the dump failed; see [`DSH9030`–`DSH9034`](../diagnostics/DSH9xxx.md) |
| `Dumped {Kind} {AssetPath} ({N} nodes) to {File}.` | Display | one asset dumped; ` (read from disk; not regenerated)` is appended when the asset already had a file |
| `DreamShader dump-graph wrote {N} graph dump(s) from {M} source file(s) to {Directory}.` | Display | end-of-run summary |
| `{N} of them already exist on disk, so dump-graph read them as they stand instead of rebuilding them (it never writes an asset). Run 'compile -All -Force' first if those sources have changed since.` | **Warning** | see [It never writes an asset](#it-never-writes-an-asset) |

The usage banner, verbatim:

```text
Usage:
  -run=DreamShader compile -Source="C:/Project/DShader/File.dsm" [-Force] [-Define=NAME=VALUE ...]
  -run=DreamShader compile -All [-Force] [-Define=NAME=VALUE ...]
  -run=DreamShader decompile -Asset="/Game/Path/Asset.Asset" [-Out="C:/Project/DShader/Decompiled/File.dsm"]
  -run=DreamShader dump-graph { -Source="C:/Project/DShader/File.dsm" | -All } [-Out="C:/Project/Saved/DreamShader/GraphBaseline"]
Supported asset types: Material -> .dsm, MaterialFunction -> .dsf.
dump-graph is a developer tool: it writes one canonical JSON per generated asset and
never writes an asset itself. Compile the tree first if its sources have changed.
-Define (short form -D) may be repeated; -Define=NAME with no value is a bare marker that
defined(NAME) sees. Names starting with DS_ are reserved for the built-in constants.
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

Console output of a successful two-file `-All` run:

```text
LogDreamShader: Display: Generated ShaderFunction /Game/Functions/MF_Noise from C:/Projects/MyGame/DShader/Functions/MF_Noise.dsf.
LogDreamShader: Display: Generated DreamShader thin-custom material /Game/Materials/M_Sample from C:/Projects/MyGame/DShader/Materials/M_Sample.dsm.
```

## See also

- [Editor bridge](bridge.md) — everything the commandlet deliberately does not start
- [Testing](../contributing/testing.md#graph-baseline) — the graph baseline `dump-graph` produces
- [Divergence](../generation/divergence.md) — the output digest whose "what counts as content" rule the dump shares
- [In-memory materials](../generation/in-memory.md) — persistent versus transient generation
- [Caching](../generation/caching.md) — the source-hash skip `-Force` bypasses
- [Preprocessor](../language/preprocessor.md) — what `-Define` feeds, and the other four define tiers
- [Asset paths](../generation/asset-paths.md) — how `Name=` and `Root=` become the package path
- [Decompiler](decompiler.md) — the export the `decompile` command drives
- [Packages](packages.md) — why `DShader/Packages` is skipped by `-All`
- [Source files](../language/source-files.md) — `.dsm` / `.dsf` / `.dsh` roles
- [Project settings](../settings/project.md) — `SourceDirectory`, which path resolution depends on
- [Testing](../contributing/testing.md) — running the automation suite headlessly
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
