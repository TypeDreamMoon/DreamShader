#Requires -Version 7.0
<#
.SYNOPSIS
    Headless driver for the DreamShader commandlet.

.DESCRIPTION
    Wraps `UnrealEditor-Cmd.exe <project> -run=DreamShader …` so an agent can compile
    and decompile DreamShaderLang sources without opening the editor, and gets back a
    clean verdict instead of 200 lines of engine boot spam. `-Define` feeds the
    conditional-compilation preprocessor, so one source can be built for either side of
    an `#if` without editing anything.

    `dump-graph` is the third verb and a DEVELOPER TOOL rather than part of the normal
    build: it writes one canonical JSON per generated asset — node classes, reflected
    properties, connections and pin order, with every coordinate, colour and GUID left
    out — so two captures can be diffed. It exists to be the parity oracle for the 2.0
    compiler rewrite. It never writes an asset, which also means an asset that already
    exists on disk is dumped as it stands rather than rebuilt; compile the tree first
    (`./dsc.ps1 compile -All -Force`) when the sources have moved.

    `check`, `dump-ir`, `index` and `export-catalog` belong to the 2.0 pipeline; the
    first three take every compilable source -- `.dss`, `.dsi`, `.dsp`, `.dsm` and `.dsf`
    (a `.dsh` header is checked through the sources that include it). `check` runs the
    compiler as far as IR validation and
    writes no asset — it is the CI gate. `check -Shaders` goes further, and costs more:
    it BUILDS AND SAVES the assets (2.0 has no transient one) and compiles their
    shaders, so an HLSL mistake in a `@custom` body fails here rather than in somebody's
    editor a week later. `dump-ir` and `index` are language-service tools, and
    `export-catalog` publishes the builtin node catalog the extension binds `UE.*` against.
    `dump-layout` draws the graph layout of every product of a source as SVG -- every
    style unless `-Style` names one -- without building an asset: the way to look at
    Blocks, Source Bands and Layered before choosing a Graph Layout Style in the project
    settings.

    `fmt` rewrites 2.0 sources -- `.dss`, `.dsi`, `.dsp`, and a `.dsh` that has no 1.x
    declarations left -- in the printer's layout, in place. It refuses to write a file it cannot vouch
    for (the formatted text has to parse to the same declarations with every comment), it
    leaves a file that uses `#if` alone, and `-All` takes the project's own source root only:
    a plugin ships its sources as they are. `fmt -Check` writes nothing and fails when a file
    would change, which is the form for CI. `list-generated` names every asset the sources
    build without building any -- package names by default, project-relative files with
    `-ListAs Files`, a ready `.gitignore` block with `-ListAs GitIgnore`, everything with
    `-ListAs Json` -- which is what Docs/generation/source-control.md writes ignore rules and
    P4 typemaps from.

    `decompile` writes 2.0 text by default: a `.dss` for a material or a material
    function, a `.dsi` for a material instance (only the parameters that differ from
    the parent), a `.dsp` for a Custom Pass pipeline. `-Format Legacy`, or an `-Out`
    ending in `.dsm` / `.dsf`, writes the 1.x text instead; `-SourceFile` decompiles every asset one source builds into one
    file. `migrate` rewrites 1.x sources (`.dsm`, `.dsf`, `.dsh`) as `.dss` and moves
    the old files to Saved/DreamShader/Migrated; `-Check` verifies the rewrite and
    writes nothing.

    `pass-registry` lists the Custom Pass HLSL slots (`<source root>/.dreampass`) and what
    each one is -- Live, Reserved, SnapshotMissing, PipelineGone, PassGone or Unknown --
    writing nothing. `-Gc` frees the PipelineGone and PassGone slots. `-Rebuild` compiles
    every `.dsp` again, frees the garbage and rewrites the registry files from
    Registry.json; a Registry.json that does not parse (a merge conflict left in it) is
    moved aside first and every slot assigned afresh. The registry and its Slots folder
    are committed files: every editor and every cook builds the global shaders from them.

    On top of the raw commandlet it adds:
      * engine resolution from the .uproject's EngineAssociation (no hard-coded path),
      * project discovery by walking up from the source file or the working directory,
      * the DreamShader messages only, each WHOLE: a compile's report is one multi-line log
        record -- every `Generated` line, then `Warnings:` and the warnings, or the first error
        and then the rest -- and the engine prefixes only its first line, so the driver keeps
        records rather than lines, and drops the engine's end-of-run summary echo of them,
      * a report of every asset the run wrote, found by comparing the Content folders of the
        project and its plugins before and after the run, and classified against git where
        the file is in a repository, so a throw-away probe asset is told apart from a tracked
        asset the run just overwrote,
      * the Custom Pass registry files under `DShader/.dreampass` that the run changed,
      * `-CleanNew`, which deletes only the assets this run created.

    Exit code is the commandlet's own: 0 success, 1 failure.

.EXAMPLE
    ./dsc.ps1 compile DShader/Materials/M_Foo.dsm -Force

.EXAMPLE
    ./dsc.ps1 compile -All

.EXAMPLE
    ./dsc.ps1 compile -All -Define USE_DETAIL=0,SHIP_BUILD -Force

.EXAMPLE
    ./dsc.ps1 decompile /Game/Materials/M_Steel -Out I:/Work/M_Steel.dss

.EXAMPLE
    ./dsc.ps1 decompile /Game/Materials/MI_Steel_Red

.EXAMPLE
    ./dsc.ps1 decompile /Game/Materials/M_Steel -Format Legacy -Out I:/Work/M_Steel.dsm

.EXAMPLE
    ./dsc.ps1 migrate DShader/Materials/M_Foo.dsm -Check

.EXAMPLE
    ./dsc.ps1 migrate -All

.EXAMPLE
    ./dsc.ps1 migrate -Root MyPlugin -Check

.EXAMPLE
    ./dsc.ps1 dump-graph -All -Out I:/Baseline/before

.EXAMPLE
    ./dsc.ps1 check -All

.EXAMPLE
    ./dsc.ps1 check DShader/Materials/M_Toon.dss -Shaders -Platform SM6,SM5 -Quality High

.EXAMPLE
    ./dsc.ps1 export-catalog

.EXAMPLE
    ./dsc.ps1 dump-layout DShader/Materials/M_Toon.dss -Out I:/Work/Layout

.EXAMPLE
    ./dsc.ps1 pass-registry

.EXAMPLE
    ./dsc.ps1 pass-registry -Gc

.EXAMPLE
    ./dsc.ps1 pass-registry -Rebuild

.NOTES
    Written for and verified against UE 5.8 + DreamShader 2.1.0 on Win64, PowerShell 7.
#>
[CmdletBinding()]
param(
    # compile  — build one source file, or every project source with -All
    # decompile — export an existing UMaterial / UMaterialFunction / material instance back to source
    # dump-graph — write a canonical JSON fingerprint of the graph each source generates
    # check — 2.0 pipeline: compile a source as far as IR validation, writing no asset
    # dump-ir — 2.0 pipeline: write the lowered IR of a source as text (and JSON with -Json)
    # dump-layout — 2.0 pipeline: draw the graph layout of a source's products as SVG, building nothing
    # index — 2.0 pipeline: write the symbol index a language service reads
    # export-catalog — 2.0 pipeline: write the builtin node catalog as JSON
    # migrate — rewrite 1.x sources (.dsm, .dsf, .dsh) as .dss
    # fmt — rewrite 2.0 sources in the printer's layout (-Check: report, write nothing)
    # list-generated — name every asset the sources build, for a .gitignore or a P4 typemap
    # pass-registry — list the Custom Pass HLSL slots (-Gc: free the dead ones, -Rebuild: rebuild the registry)
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet('compile', 'decompile', 'dump-graph', 'check', 'dump-ir', 'dump-layout', 'index', 'export-catalog', 'migrate', 'fmt', 'list-generated', 'pass-registry')]
    [string]$Command,

    # compile / dump-graph / check / dump-ir / index: path to a compilable source -- .dss, .dsi,
    # .dsp, .dsm or .dsf (absolute, or relative to DShader/ then the project).
    # migrate: a 1.x source, .dsm, .dsf or .dsh.
    # decompile: an object path such as /Game/Materials/M_Steel (a material instance writes a .dsi).
    [Parameter(Position = 1)]
    [string]$Target,

    # Compile every project source instead of one file. .dsf files are built before .dsm.
    # dump-graph resolves -All / -Source through the same code, so the two verbs always
    # agree on which sources a project has.
    [switch]$All,

    # Bypass the source-hash skip. Without it an unchanged source logs "Skipped …".
    # Accepted by dump-graph for symmetry and ignored there: a dump always regenerates,
    # because dumping a graph that was skipped would dump whatever was already in memory.
    [switch]$Force,

    # Preprocessor defines for `#if` / `#elif`, as NAME=VALUE or a bare NAME marker
    # (`defined(NAME)` is true for a marker; arithmetic reads it as 1). Comma separated,
    # without spaces:
    #   -Define SUBSTRATE_PATH=1,USE_DETAIL=0,SHIP_BUILD
    # Under `pwsh -File` that arrives as ONE string, which the driver splits on commas; in a
    # PowerShell session `-Define A=1, B` is an array and works too. Each item becomes its own
    # `-Define=` on the commandlet line, which is what the commandlet needs: it re-tokenizes
    # the raw command line specifically so repeated `-Define=` survive.
    #
    # These outrank the project settings table and any C++ registration, but never the
    # read-only DS_ built-ins — a DS_ name here is dropped with a warning rather than
    # applied, because those describe the compiling process and lying about them produces
    # a graph the engine cannot compile at all.
    #
    # Quoting follows the rest of this script: the item is interpolated into a single
    # argument and PowerShell quotes it if it contains spaces, which is exactly what
    # `-Source=` already relies on for paths. A value with an `=` in it survives too — the
    # commandlet splits the payload on the FIRST `=` only.
    [string[]]$Define,

    # decompile: write the source here instead of <SourceDirectory>/Decompiled/…. Under the
    # default -Format Auto the extension decides: .dsm / .dsf is the 1.x text, anything else 2.0.
    # migrate: write the .dss files under this directory, mirroring the source tree, instead of
    # beside each source.
    # dump-graph: the root of the dump tree, instead of <Project>/Saved/DreamShader/GraphBaseline.
    # dump-ir: instead of <Project>/Saved/DreamShader/IR. index: instead of …/Index.
    # fmt: write the formatted copies under this directory instead of over the sources.
    # list-generated: write the list to this file instead of to the log.
    # export-catalog: the file, instead of <Project>/Saved/DreamShader/Bridge/dreamshader-builtin-catalog.json.
    [string]$Out,

    # check: after the graph is validated, build and SAVE the assets and compile their
    # shaders. The saving is not optional -- 2.0 has no transient asset -- so pass this
    # only where `compile` would be welcome too. It is the only thing that catches an
    # HLSL error in a `@custom` body without an editor. Slow: it runs the real compiler.
    [switch]$Shaders,

    # check -Shaders: which shader platforms to compile for, comma separated — SM6, SM5,
    # ES3_1, or a shader format name such as PCD3D_SM6. Default: the host's own.
    [string]$Platform,

    # check -Shaders: which material quality levels, comma separated — Low, Medium, High,
    # Epic. Default: the project's current scalability level, which is what an editor shows.
    [string]$Quality,

    # check -Shaders: seconds to wait per material before calling the compile a hang.
    # Default 120. A timeout is reported as an error, never waited out forever.
    [int]$Timeout,

    # check: also write the diagnostics as JSON here (schema dreamshader-diagnostics,
    # version 1, plus the optional `length` field). One file per source under -All.
    # decompile: the decompile's diagnostics, in the same schema.
    [string]$DiagnosticsOut,

    # decompile: Dss (a .dss, a .dsi for a material instance, a .dsp for a pipeline), Legacy (the
    # 1.x .dsm / .dsf text), or Auto (by the -Out extension; the default).
    [ValidateSet('Dss', 'Legacy', 'Auto')]
    [string]$Format,

    # decompile: keep the asset's own object path, writing `/// @name` when the output file
    # would derive another one.
    [switch]$KeepAssetPath,

    # decompile: prefer HLSL operators over class-exact `UE.*` calls. The graph may gain
    # Constant nodes; the shader does not change.
    [switch]$Readable,

    # decompile: decompile every asset this source builds into one file, instead of naming one asset.
    [string]$SourceFile,

    # migrate: verify the rewrite (re-parse, compare the IR) and write nothing.
    # fmt: write nothing, and fail when a file is not in the formatter's layout.
    [switch]$Check,

    # list-generated: Packages (long package names, the default), Files (paths relative to the
    # project directory), GitIgnore (the same paths as an anchored .gitignore block), or Json
    # (every field: source, kind, backend, package, file, onDisk, persistent).
    [ValidateSet('Packages', 'Files', 'GitIgnore', 'Json')]
    [string]$ListAs,

    # list-generated: also list a ThinCustom material that is memory-only right now. It has no
    # file, so it is left out of a list that ignore rules are written from.
    [switch]$IncludeEphemeral,

    # migrate: report what would be written and write nothing -- the same as -Check.
    [switch]$DryRun,

    # migrate: delete the 1.x sources instead of moving them to Saved/DreamShader/Migrated.
    [switch]$NoBackup,

    # migrate: every 1.x source of ONE source root, by its display name or its plugin's name
    # (`-Root MyPlugin`). `-All` takes the writable roots only, which leaves a plugin's root out.
    [string]$Root,

    # dump-ir: write the JSON form beside the text one. dump-layout: the coordinates as JSON beside each SVG.
    [switch]$Json,

    # dump-layout: Blocks, SourceBands, Layered, or All (the default) for one picture of each.
    [ValidateSet('Blocks', 'SourceBands', 'Layered', 'All')]
    [string]$Style,

    # Keep -nullrhi on for `check -Shaders`, where it is dropped by default — see the note below.
    [switch]$NullRhi,

    # pass-registry: free the slots of pipelines no .dsp builds any more and of passes a
    # pipeline no longer runs in HLSL. A slot whose .dsp does not compile is kept.
    [switch]$Gc,

    # pass-registry: compile every .dsp again, free the garbage and rewrite the registry
    # files from Registry.json (one that does not parse is moved aside and every slot
    # assigned afresh). Writes assets, as compile does.
    [switch]$Rebuild,

    # The .uproject. Defaults to the nearest one at or above the target / working directory.
    [string]$Project,

    # Engine root (the directory containing Engine/Binaries). Defaults to the association lookup.
    [string]$Engine,

    # Delete the assets this run created: .uasset files under a Content folder of the project
    # or one of its plugins that were not there before the run. An asset that existed before is
    # never touched — when git tracks it and the run changed it, it is reported with the command
    # that restores it.
    [switch]$CleanNew,

    # Echo the full commandlet output instead of just the DreamShader lines.
    [switch]$Raw
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- project & engine

function Resolve-Uproject {
    param([string]$Explicit, [string]$StartAt)

    if ($Explicit) {
        if (-not (Test-Path -LiteralPath $Explicit)) { throw "No .uproject at '$Explicit'." }
        return (Resolve-Path -LiteralPath $Explicit).Path
    }

    # Walk up from the target file first, then from the working directory. A source file
    # under DShader/ sits inside the project, so this finds the right one even when the
    # driver is invoked from somewhere else entirely.
    $roots = @()
    if ($StartAt -and (Test-Path -LiteralPath $StartAt)) {
        $item = Get-Item -LiteralPath $StartAt
        $roots += if ($item.PSIsContainer) { $item.FullName } else { $item.DirectoryName }
    }
    $roots += (Get-Location).Path

    foreach ($root in $roots) {
        $dir = $root
        while ($dir) {
            $found = @(Get-ChildItem -LiteralPath $dir -Filter '*.uproject' -File -ErrorAction SilentlyContinue)
            if ($found.Count -gt 0) { return $found[0].FullName }
            $dir = Split-Path -Parent $dir
        }
    }

    throw "Could not find a .uproject at or above '$($roots -join "', '")'. Pass -Project."
}

function Resolve-EngineRoot {
    param([string]$Explicit, [string]$UprojectPath)

    if ($Explicit) { return $Explicit.TrimEnd('\', '/') }
    if ($env:UE_ENGINE_ROOT) { return $env:UE_ENGINE_ROOT.TrimEnd('\', '/') }

    $association = (Get-Content -LiteralPath $UprojectPath -Raw | ConvertFrom-Json).EngineAssociation
    if (-not $association) { throw "The .uproject has no EngineAssociation. Pass -Engine." }

    # Source builds register a GUID -> path pair here.
    $builds = Get-ItemProperty 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
    if ($builds -and $builds.PSObject.Properties.Name -contains $association) {
        return $builds.$association.TrimEnd('\', '/')
    }

    # Launcher installs register a version number instead ("5.8"). If neither key has it, pass
    # -Engine or set UE_ENGINE_ROOT.
    foreach ($hive in 'HKLM:', 'HKCU:') {
        $key = "$hive\SOFTWARE\EpicGames\Unreal Engine\$association"
        $installed = (Get-ItemProperty $key -ErrorAction SilentlyContinue).InstalledDirectory
        if ($installed) { return $installed.TrimEnd('\', '/') }
    }

    throw "EngineAssociation '$association' is not registered. Pass -Engine or set UE_ENGINE_ROOT."
}

function Remove-EmptyParents {
    <#
        Deleting a generated .uasset leaves its folders behind, and an empty folder under
        Content/ still shows up in the Content Browser. Walk up to Content/ (exclusive),
        removing directories while they are empty.
    #>
    param([string]$StartDir, [string]$StopAtDir)

    $dir = $StartDir
    while ($dir -and $dir.Length -gt $StopAtDir.Length -and $dir.StartsWith($StopAtDir, [StringComparison]::OrdinalIgnoreCase)) {
        if (@(Get-ChildItem -LiteralPath $dir -Force -ErrorAction SilentlyContinue).Count -gt 0) { break }
        Remove-Item -LiteralPath $dir -Force
        $dir = Split-Path -Parent $dir
    }
}

function Resolve-OutPath {
    # A relative path means the engine's Binaries folder to the commandlet; to the person typing it, the
    # directory the driver was started in. The file does not have to exist yet.
    param([string]$Path)
    return ($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path) -replace '\\', '/')
}

function Resolve-SourceArg {
    # An existing relative path is made absolute so the commandlet's own DShader-then-project resolution never
    # has to guess.
    param([string]$Path)
    $resolved = if (Test-Path -LiteralPath $Path) { (Resolve-Path -LiteralPath $Path).Path } else { $Path }
    return ($resolved -replace '\\', '/')
}

function Get-ContentRoots {
    <#
        Where a run can write a package: the project's Content/ and the Content/ of every plugin under Plugins/
        (a plugin's mount point is its descriptor's name). Depth-limited, because Plugins/ also holds every
        plugin's Intermediate/ and Binaries/.
    #>
    param([string]$ProjectDir)

    $roots = [ordered]@{}
    $game = Join-Path $ProjectDir 'Content'
    if (Test-Path -LiteralPath $game) { $roots['Game'] = $game }
    $plugins = Join-Path $ProjectDir 'Plugins'
    if (Test-Path -LiteralPath $plugins) {
        foreach ($descriptor in Get-ChildItem -LiteralPath $plugins -Filter '*.uplugin' -File -Recurse -Depth 3 -ErrorAction SilentlyContinue) {
            $content = Join-Path $descriptor.DirectoryName 'Content'
            if ((Test-Path -LiteralPath $content) -and -not $roots.Contains($descriptor.BaseName)) {
                $roots[$descriptor.BaseName] = $content
            }
        }
    }
    return $roots
}

function Get-FileSnapshot {
    # full path -> last write time, for every file under the directories that match the filter.
    param([string[]]$Directories, [string]$Filter = '*')

    $map = @{}
    foreach ($dir in $Directories) {
        if (-not (Test-Path -LiteralPath $dir)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $dir -Filter $Filter -File -Recurse -Force -ErrorAction SilentlyContinue) {
            $map[$file.FullName] = $file.LastWriteTimeUtc.Ticks
        }
    }
    return $map
}

function Compare-FileSnapshot {
    param([hashtable]$Before, [hashtable]$After)

    $new = @($After.Keys | Where-Object { -not $Before.ContainsKey($_) } | Sort-Object)
    $changed = @($After.Keys | Where-Object { $Before.ContainsKey($_) -and $Before[$_] -ne $After[$_] } | Sort-Object)
    $removed = @($Before.Keys | Where-Object { -not $After.ContainsKey($_) } | Sort-Object)
    return [pscustomobject]@{ New = $new; Changed = $changed; Removed = $removed }
}

function Get-GitState {
    <#
        The file's state in the repository that holds it -- asked of the file's own directory, so an asset of a
        plugin that is a repository of its own is answered by that one. $null when git is missing or the file is
        in no work tree (this script must not depend on the project being a repository).
    #>
    param([string]$File)

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { return $null }
    $dir = Split-Path -Parent $File
    $top = & git -C $dir rev-parse --show-toplevel 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $top) { return $null }
    $status = & git -C $dir status --porcelain -- (Split-Path -Leaf $File) 2>$null
    $state = if (-not $status) { 'tracked, unchanged' }
             elseif ("$status" -match '^\?\?') { 'untracked' }
             elseif ("$status" -match '^!!') { 'ignored' }
             else { 'TRACKED AND MODIFIED' }
    return [pscustomobject]@{ Top = ($top -replace '/', '\'); State = $state }
}

function Get-LogRecords {
    <#
        UE writes `<Category>: [<Verbosity>: ]` in front of the FIRST line of a message only, and a DreamShader
        compile report is one message of many lines: the `Generated` lines of every product, then `Warnings:` and
        the warnings, or the first error and then every other diagnostic. So the log is read as records -- a line
        that starts with a category opens one, every other line continues it -- and the LogDreamShader records are
        kept whole. A diagnostic's own `DSHnnnn: ` is not a category, and neither is a drive letter (`C:/`).

        At the end of a run the engine repeats the run's warnings and errors under `LogInit` (its "Warning/Error
        Summary"); a record whose first line was already seen is that echo, and is dropped.
    #>
    param([string[]]$Lines)

    $recordStart = '^(?:\[[^\]]*\])*(?!DSH\d{4}:)[A-Za-z_][A-Za-z0-9_]*: '
    $records = [System.Collections.Generic.List[object]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new()
    $current = $null

    foreach ($line in $Lines) {
        if ($line -eq '' -or $line -match $recordStart) {
            if ($current -and $seen.Add($current[0])) { $records.Add($current) }
            $current = $null
            if ($line -match 'LogDreamShader:') {
                $current = [System.Collections.Generic.List[string]]::new()
                $current.Add(($line -replace '^.*?(LogDreamShader:)', '$1'))
            }
        }
        elseif ($current) {
            $current.Add($line)
        }
    }
    if ($current -and $seen.Add($current[0])) { $records.Add($current) }
    return , $records
}

# ---------------------------------------------------------------- run

$uproject = Resolve-Uproject -Explicit $Project -StartAt $(if ($SourceFile) { $SourceFile } else { $Target })
$projectDir = Split-Path -Parent $uproject
$engineRoot = Resolve-EngineRoot -Explicit $Engine -UprojectPath $uproject
$editorCmd = Join-Path $engineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'

if (-not (Test-Path -LiteralPath $editorCmd)) {
    throw "UnrealEditor-Cmd.exe not found at '$editorCmd'."
}

$commandletArgs = @($uproject, "-run=DreamShader", $Command)

switch ($Command) {
    'compile' {
        if ($All) {
            $commandletArgs += '-All'
        }
        elseif ($Target) {
            $commandletArgs += "-Source=$(Resolve-SourceArg $Target)"
        }
        else {
            throw "compile needs a source file (.dss, .dsi, .dsp, .dsm or .dsf) or -All."
        }
        if ($Force) { $commandletArgs += '-Force' }
    }
    'decompile' {
        if (-not $Target -and -not $SourceFile) {
            throw "decompile needs an asset object path, e.g. /Game/Materials/M_Steel, or -SourceFile."
        }
        if ($Target) { $commandletArgs += "-Asset=$Target" }
        if ($SourceFile) {
            $commandletArgs += "-SourceFile=$(Resolve-SourceArg $SourceFile)"
        }
        if ($Out) { $commandletArgs += "-Out=$(Resolve-OutPath $Out)" }
        if ($Format) { $commandletArgs += "-Format=$Format" }
        if ($KeepAssetPath) { $commandletArgs += '-KeepAssetPath' }
        if ($Readable) { $commandletArgs += '-Readable' }
        if ($DiagnosticsOut) { $commandletArgs += "-DiagnosticsOut=$(Resolve-OutPath $DiagnosticsOut)" }
    }
    'dump-graph' {
        # Deliberately the same source selection as compile, down to the -Force pass-through
        # (which the commandlet accepts and ignores): a baseline that covered a different set
        # of sources than the compiler does would report a missing dump as a difference.
        if ($All) {
            $commandletArgs += '-All'
        }
        elseif ($Target) {
            $commandletArgs += "-Source=$(Resolve-SourceArg $Target)"
        }
        else {
            throw "dump-graph needs a source file or -All."
        }
        if ($Force) { $commandletArgs += '-Force' }
        if ($Out) { $commandletArgs += "-Out=$(Resolve-OutPath $Out)" }
    }
    { $_ -in @('check', 'dump-ir', 'dump-layout', 'index') } {
        # The 2.0 verbs share one source selection, deliberately: `check` and `dump-ir` that
        # disagreed about which files a project has would report a missing dump as a
        # difference, which is the same trap dump-graph documents.
        if ($All) {
            $commandletArgs += '-All'
        }
        elseif ($Target) {
            $commandletArgs += "-Source=$(Resolve-SourceArg $Target)"
        }
        else {
            throw "$Command needs a source file (.dss, .dsi, .dsp, .dsm or .dsf) or -All."
        }
        if ($Out) { $commandletArgs += "-Out=$(Resolve-OutPath $Out)" }
        if ($Force) { $commandletArgs += '-Force' }
        if ($Json) { $commandletArgs += '-Json' }
        if ($Style) { $commandletArgs += "-Style=$Style" }
        if ($Shaders) { $commandletArgs += '-Shaders' }
        if ($Platform) { $commandletArgs += "-Platform=$Platform" }
        if ($Quality) { $commandletArgs += "-Quality=$Quality" }
        if ($Timeout) { $commandletArgs += "-Timeout=$Timeout" }
        if ($DiagnosticsOut) { $commandletArgs += "-DiagnosticsOut=$(Resolve-OutPath $DiagnosticsOut)" }
    }
    'export-catalog' {
        # No source: the catalog is a property of the engine and the loaded plugins, not of
        # any one file.
        if ($Out) { $commandletArgs += "-Out=$(Resolve-OutPath $Out)" }
    }
    'pass-registry' {
        # No source either: the registry is the project's. -Rebuild includes the collection,
        # so -Gc beside it adds nothing.
        if ($Rebuild) { $commandletArgs += '-Rebuild' }
        elseif ($Gc) { $commandletArgs += '-Gc' }
    }
    { $_ -in @('fmt', 'list-generated') } {
        if ($All) {
            $commandletArgs += '-All'
        }
        elseif ($Target) {
            $commandletArgs += "-Source=$(Resolve-SourceArg $Target)"
        }
        else {
            throw "$Command needs a source file or -All."
        }
        if ($Out) { $commandletArgs += "-Out=$(Resolve-OutPath $Out)" }
        if ($Check) { $commandletArgs += '-Check' }
        if ($ListAs) { $commandletArgs += "-As=$ListAs" }
        if ($IncludeEphemeral) { $commandletArgs += '-IncludeEphemeral' }
    }
    'migrate' {
        # The commandlet orders a -All set headers first and migrates a header only when every
        # file that includes it is migrated too.
        if ($Root) {
            $commandletArgs += "-Root=$Root"
        }
        elseif ($All) {
            $commandletArgs += '-All'
        }
        elseif ($Target) {
            $commandletArgs += "-Source=$(Resolve-SourceArg $Target)"
        }
        else {
            throw "migrate needs a 1.x source file (.dsm, .dsf or .dsh), -All, or -Root <name>."
        }
        if ($Check) { $commandletArgs += '-Check' }
        if ($DryRun) { $commandletArgs += '-DryRun' }
        if ($NoBackup) { $commandletArgs += '-NoBackup' }
        if ($Out) { $commandletArgs += "-Out=$(Resolve-OutPath $Out)" }
    }
}

# One `-Define=` per item, never one joined switch: the commandlet installs the whole set at
# once and repeated switches are how it receives more than one. Emitted outside the switch
# above so it is not the compile branch's private feature — the commandlet reads them before
# it dispatches, so any command that grows a source-reading path gets them for free.
foreach ($item in @($Define | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
    $commandletArgs += "-Define=$item"
}

# -nullrhi keeps the run off the GPU, and `check -Shaders` drops it by default.
#
# Dropping it is not enough to get the RENDERING shader maps, though: in a commandlet
# FApp::CanEverRender() is false unless -AllowCommandletRendering is passed as well, which this
# driver does not pass. So `check -Shaders` drives the cook-target shader compilers, reports a
# timeout as an error, and takes the error text from the engine log. Pass -NullRhi to keep
# -nullrhi on regardless.
$useNullRhi = (-not ($Command -eq 'check' -and $Shaders)) -or $NullRhi
$commandletArgs += @('-unattended', '-nopause', '-nosplash', '-stdout', '-NoLogTimes')
if ($useNullRhi) { $commandletArgs += '-nullrhi' }

Write-Host "dsc: $Command  project=$(Split-Path -Leaf $uproject)  engine=$engineRoot" -ForegroundColor DarkGray

# What the run can write. `check` writes assets only with -Shaders, and `pass-registry` only with -Rebuild;
# a compile of a .dsp, and both of those, can also rewrite the committed Custom Pass registry files.
$writesAssets = ($Command -eq 'compile') -or ($Command -eq 'check' -and $Shaders) -or ($Command -eq 'pass-registry' -and $Rebuild)
$touchesRegistry = $writesAssets -or ($Command -eq 'pass-registry' -and $Gc)
$contentRoots = if ($writesAssets) { Get-ContentRoots -ProjectDir $projectDir } else { [ordered]@{} }
$assetsBefore = if ($writesAssets) { Get-FileSnapshot -Directories @($contentRoots.Values) -Filter '*.uasset' } else { @{} }
$registryDir = Join-Path $projectDir 'DShader/.dreampass'
$registryBefore = if ($touchesRegistry) { Get-FileSnapshot -Directories @($registryDir) } else { @{} }

$output = & $editorCmd @commandletArgs 2>&1 | ForEach-Object { "$_" }
$exit = $LASTEXITCODE

if ($Raw) { $output | ForEach-Object { Write-Host $_ } }

$records = Get-LogRecords -Lines $output
$dreamLines = @($records | ForEach-Object { $_ })

# ---------------------------------------------------------------- report

if (-not $Raw) {
    foreach ($record in $records) {
        $isError = $record[0] -match '^LogDreamShader:\s*Error:'
        $colour = if ($isError) { 'Red' } elseif ($record[0] -match '^LogDreamShader:\s*Warning:') { 'Yellow' } else { 'Green' }
        $inWarnings = $false
        foreach ($line in $record) {
            if ($line -eq 'Warnings:') { $inWarnings = $true }
            Write-Host $line -ForegroundColor $(if ($inWarnings -and -not $isError) { 'Yellow' } else { $colour })
        }
    }
}

if ($writesAssets) {
    $assetsAfter = Get-FileSnapshot -Directories @($contentRoots.Values) -Filter '*.uasset'
    $diff = Compare-FileSnapshot -Before $assetsBefore -After $assetsAfter

    if ($diff.New.Count -gt 0 -or $diff.Changed.Count -gt 0) {
        Write-Host ''
        Write-Host "Assets written to disk by this run:" -ForegroundColor DarkGray
    }
    foreach ($full in $diff.New) {
        $relative = [IO.Path]::GetRelativePath($projectDir, $full) -replace '\\', '/'
        Write-Host "  $relative  [NEW]" -ForegroundColor Yellow
        if ($CleanNew) {
            Remove-Item -LiteralPath $full -Force
            $stopAt = @($contentRoots.Values | Where-Object { $full.StartsWith($_ + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) })[0]
            if ($stopAt) { Remove-EmptyParents -StartDir (Split-Path -Parent $full) -StopAtDir $stopAt }
            Write-Host "    deleted (-CleanNew)" -ForegroundColor DarkGray
        }
    }
    foreach ($full in $diff.Changed) {
        $relative = [IO.Path]::GetRelativePath($projectDir, $full) -replace '\\', '/'
        $git = Get-GitState -File $full
        $state = if (-not $git) { 'rewritten; it existed before the run' } else { $git.State }
        Write-Host "  $relative  [$state]" -ForegroundColor $(if ($state -eq 'TRACKED AND MODIFIED') { 'Red' } else { 'DarkGray' })
        if ($state -eq 'TRACKED AND MODIFIED') {
            Write-Host "    restore with: git -C `"$($git.Top)`" checkout -- `"$([IO.Path]::GetRelativePath($git.Top, $full) -replace '\\', '/')`"" -ForegroundColor Red
        }
    }
    if ($diff.New.Count -gt 0 -and -not $CleanNew) {
        Write-Host "  (pass -CleanNew to delete the NEW ones — a leftover ThinCustom file shadows the editor's Ephemeral product)" -ForegroundColor DarkGray
    }

    # A product the log names that no Content folder of the project shows: an engine mount, or a save that
    # did not happen. `Generated <Kind> <ObjectPath> from <Source>.`
    $found = @($diff.New + $diff.Changed)
    foreach ($line in $dreamLines) {
        if ($line -match "Generated [A-Za-z]+ '?(/[^ '`"]+?)'? from ") {
            $objectPath = $Matches[1]
            $package = ($objectPath -split '\.')[0]
            $mount = ($package.TrimStart('/') -split '/')[0]
            $root = $contentRoots[$mount]
            if (-not $root) {
                Write-Host "  $objectPath  (mount '/$mount/' is not the project or one of its plugins — locate it by hand)" -ForegroundColor Yellow
                continue
            }
            $file = Join-Path $root (($package.Substring($mount.Length + 2) -replace '/', '\') + '.uasset')
            if ($found -notcontains $file -and -not (Test-Path -LiteralPath $file)) {
                Write-Host "  $objectPath  (logged as generated, but $([IO.Path]::GetRelativePath($projectDir, $file)) is not on disk)" -ForegroundColor Yellow
            }
        }
    }
}

if ($touchesRegistry) {
    $registryDiff = Compare-FileSnapshot -Before $registryBefore -After (Get-FileSnapshot -Directories @($registryDir))
    $touched = @(
        $registryDiff.New | ForEach-Object { "  $([IO.Path]::GetRelativePath($projectDir, $_))  [new]" }
        $registryDiff.Changed | ForEach-Object { "  $([IO.Path]::GetRelativePath($projectDir, $_))  [changed]" }
        $registryDiff.Removed | ForEach-Object { "  $([IO.Path]::GetRelativePath($projectDir, $_))  [removed]" })
    if ($touched.Count -gt 0) {
        Write-Host ''
        Write-Host "Custom Pass registry files this run changed — sources: commit them with the .dsp:" -ForegroundColor DarkGray
        $touched | ForEach-Object { Write-Host $_ -ForegroundColor Cyan }
    }
}

# `migrate` names every source it wrote and where the old file went, one line per file:
#   Migrated '<source>' to '<output>' (backup '<backup>').    or    ... (no backup).
#   Checked '<source>': would write '<output>'.                     (-Check / -DryRun)
if ($Command -eq 'migrate') {
    $migrated = @()
    $checked = @()
    foreach ($line in $dreamLines) {
        if ($line -match "Migrated '([^']+)' to '([^']+)' \((?:backup '([^']+)'|no backup)\)\.") {
            $migrated += [pscustomobject]@{ Source = $Matches[1]; Output = $Matches[2]; Backup = $Matches[3] }
        }
        elseif ($line -match "Checked '([^']+)': would write '([^']+)'\.") {
            $checked += [pscustomobject]@{ Source = $Matches[1]; Output = $Matches[2] }
        }
    }

    if ($migrated.Count -gt 0) {
        Write-Host ''
        Write-Host "Sources written by this run:" -ForegroundColor DarkGray
        foreach ($entry in $migrated) {
            Write-Host "  $($entry.Output)" -ForegroundColor Yellow
            if ($entry.Backup) {
                Write-Host "    backup of $($entry.Source): $($entry.Backup)" -ForegroundColor DarkGray
            }
            else {
                Write-Host "    $($entry.Source) was deleted (-NoBackup)" -ForegroundColor DarkGray
            }
        }
    }
    if ($checked.Count -gt 0) {
        Write-Host ''
        Write-Host "Checked, nothing written:" -ForegroundColor DarkGray
        foreach ($entry in $checked) {
            Write-Host "  $($entry.Source) -> $($entry.Output)" -ForegroundColor DarkGray
        }
    }
}

Write-Host ''
if ($exit -eq 0) {
    Write-Host "dsc: OK (exit 0)" -ForegroundColor Green
}
else {
    Write-Host "dsc: FAILED (exit $exit)" -ForegroundColor Red
    if ($dreamLines.Count -eq 0) {
        Write-Host "No LogDreamShader output — re-run with -Raw to see the engine log." -ForegroundColor Yellow
    }
}

exit $exit
