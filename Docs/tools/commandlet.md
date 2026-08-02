# Commandlet

> [DreamShader](../index.md) » [Tools](index.md) » **Commandlet**

`-run=DreamShader` — a headless entry point that compiles DreamShader sources into persistent assets
and decompiles existing material assets back into source files.

| | |
| :-- | :-- |
| Kind | `UCommandlet` subclass — `UDreamShaderCommandlet`, in the `DreamShaderEditor` module |
| Invocation | `-run=DreamShader` (equivalently `-run=DreamShaderCommandlet`) |
| Commands | `compile`, `generate`, `decompile`, `export` |
| Exit codes | `0` success, `1` failure |
| Log category | `LogDreamShader` |

## Synopsis

```text
UnrealEditor-Cmd.exe <project>.uproject -run=DreamShader <command> [<option>…]

<command> ::= { compile | generate | decompile | export }

-run=DreamShader { compile | generate } { -Source=<path> | -File=<path> | -All } [-Force]
-run=DreamShader { decompile | export } -Asset=<object-path> [{ -Out | -Output }=<path>]
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

The usage banner, verbatim:

```text
Usage:
  -run=DreamShader compile -Source="C:/Project/DShader/File.dsm" [-Force]
  -run=DreamShader compile -All [-Force]
  -run=DreamShader decompile -Asset="/Game/Path/Asset.Asset" [-Out="C:/Project/DShader/Decompiled/File.dsm"]
Supported asset types: Material -> .dsm, MaterialFunction -> .dsf.
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

Console output of a successful two-file `-All` run:

```text
LogDreamShader: Display: Generated ShaderFunction /Game/Functions/MF_Noise from C:/Projects/MyGame/DShader/Functions/MF_Noise.dsf.
LogDreamShader: Display: Generated DreamShader thin-custom material /Game/Materials/M_Sample from C:/Projects/MyGame/DShader/Materials/M_Sample.dsm.
```

## See also

- [Editor bridge](bridge.md) — everything the commandlet deliberately does not start
- [In-memory materials](../generation/in-memory.md) — persistent versus transient generation
- [Caching](../generation/caching.md) — the source-hash skip `-Force` bypasses
- [Asset paths](../generation/asset-paths.md) — how `Name=` and `Root=` become the package path
- [Decompiler](decompiler.md) — the export the `decompile` command drives
- [Packages](packages.md) — why `DShader/Packages` is skipped by `-All`
- [Source files](../language/source-files.md) — `.dsm` / `.dsf` / `.dsh` roles
- [Project settings](../settings/project.md) — `SourceDirectory`, which path resolution depends on
- [Testing](../contributing/testing.md) — running the automation suite headlessly
- [Diagnostics index](../diagnostics/index.md) — every message, by stage
