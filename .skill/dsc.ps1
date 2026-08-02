<#
.SYNOPSIS
    Headless driver for the DreamShader commandlet.

.DESCRIPTION
    Wraps `UnrealEditor-Cmd.exe <project> -run=DreamShader …` so an agent can compile
    and decompile DreamShaderLang sources without opening the editor, and gets back a
    clean verdict instead of 200 lines of engine boot spam.

    On top of the raw commandlet it adds:
      * engine resolution from the .uproject's EngineAssociation (no hard-coded path),
      * project discovery by walking up from the source file or the working directory,
      * de-duplication of the doubled `LogInit: Display: LogDreamShader:` echo,
      * a report of every asset the run wrote, classified against git so you can tell a
        throw-away probe asset from a tracked asset the run just overwrote,
      * `-CleanNew`, which deletes only the assets this run created and were untracked.

    Exit code is the commandlet's own: 0 success, 1 failure.

.EXAMPLE
    ./dsc.ps1 compile DShader/Materials/M_Foo.dsm -Force

.EXAMPLE
    ./dsc.ps1 compile -All

.EXAMPLE
    ./dsc.ps1 decompile /Game/Materials/M_Steel -Out I:/Work/M_Steel.dsm

.NOTES
    Written for and verified against UE 5.8 (source build) + DreamShader 1.5.1 on Win64.
#>
[CmdletBinding()]
param(
    # compile  — build one source file, or every project source with -All
    # decompile — export an existing UMaterial / UMaterialFunction back to source
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet('compile', 'decompile')]
    [string]$Command,

    # compile: path to a .dsm/.dsf (absolute, or relative to DShader/ then the project).
    # decompile: an object path such as /Game/Materials/M_Steel.
    [Parameter(Position = 1)]
    [string]$Target,

    # Compile every project source instead of one file. .dsf files are built before .dsm.
    [switch]$All,

    # Bypass the source-hash skip. Without it an unchanged source logs "Skipped …".
    [switch]$Force,

    # decompile only: write here instead of <SourceDirectory>/Decompiled/….
    [string]$Out,

    # The .uproject. Defaults to the nearest one at or above the target / working directory.
    [string]$Project,

    # Engine root (the directory containing Engine/Binaries). Defaults to the association lookup.
    [string]$Engine,

    # Delete the assets this run created that git reports as untracked. Assets that were
    # already tracked are never touched — they are reported instead.
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

    # Source builds register a GUID -> path pair here. This is the verified path on this machine.
    $builds = Get-ItemProperty 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
    if ($builds -and $builds.PSObject.Properties.Name -contains $association) {
        return $builds.$association.TrimEnd('\', '/')
    }

    # Launcher installs register a version number instead. Not exercised on this machine —
    # if it misses, pass -Engine or set UE_ENGINE_ROOT.
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

# ---------------------------------------------------------------- run

$uproject = Resolve-Uproject -Explicit $Project -StartAt $Target
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
            # An existing relative path is made absolute here so the commandlet's own
            # DShader-then-project resolution never has to guess.
            $resolved = if (Test-Path -LiteralPath $Target) { (Resolve-Path -LiteralPath $Target).Path } else { $Target }
            $commandletArgs += "-Source=$($resolved -replace '\\', '/')"
        }
        else {
            throw "compile needs a source file or -All."
        }
        if ($Force) { $commandletArgs += '-Force' }
    }
    'decompile' {
        if (-not $Target) { throw "decompile needs an asset object path, e.g. /Game/Materials/M_Steel." }
        $commandletArgs += "-Asset=$Target"
        if ($Out) { $commandletArgs += "-Out=$($Out -replace '\\', '/')" }
    }
}

# -nullrhi keeps the run off the GPU. Drop it only when something needs real shader
# compilation, e.g. reading back material compile errors.
$commandletArgs += @('-unattended', '-nopause', '-nullrhi', '-nosplash', '-stdout', '-NoLogTimes')

Write-Host "dsc: $Command  project=$(Split-Path -Leaf $uproject)  engine=$engineRoot" -ForegroundColor DarkGray

$output = & $editorCmd @commandletArgs 2>&1 | ForEach-Object { "$_" }
$exit = $LASTEXITCODE

if ($Raw) { $output | ForEach-Object { Write-Host $_ } }

# The commandlet logs each DreamShader line twice: once raw, once re-emitted through
# LogInit. Strip the wrapper prefix, then keep first occurrences.
$dreamLines = $output |
    Where-Object { $_ -match 'LogDreamShader:' } |
    ForEach-Object { $_ -replace '^.*?(LogDreamShader:)', '$1' } |
    Select-Object -Unique

# ---------------------------------------------------------------- report

if (-not $Raw) {
    foreach ($line in $dreamLines) {
        $colour = if ($line -match 'LogDreamShader:\s*Error:') { 'Red' }
                  elseif ($line -match 'LogDreamShader:\s*Warning:') { 'Yellow' }
                  else { 'Green' }
        Write-Host $line -ForegroundColor $colour
    }
}

# Every "Generated … <path> from …" message names an asset the run persisted to disk.
# The commandlet always writes real .uasset files, even though the interactive editor
# generates the same material in memory only.
$generated = @()
foreach ($line in $dreamLines) {
    if ($line -match 'Generated (?:DreamShader thin-custom material |DreamShader helper include |[A-Za-z]+ )?''?(/[^ ''"]+)''? from ') {
        $generated += $Matches[1]
    }
}
$generated = @($generated | Select-Object -Unique)

if ($generated.Count -gt 0) {
    Write-Host ''
    Write-Host "Assets written to disk by this run:" -ForegroundColor DarkGray

    foreach ($objectPath in $generated) {
        # /Game/Foo/M_Bar.M_Bar -> Content/Foo/M_Bar.uasset
        $packagePath = ($objectPath -split '\.')[0]
        if ($packagePath -notmatch '^/Game/') {
            Write-Host "  $objectPath  (outside /Game — locate and clean by hand)" -ForegroundColor Yellow
            continue
        }

        $relative = "Content/" + $packagePath.Substring('/Game/'.Length) + '.uasset'
        $full = Join-Path $projectDir $relative

        if (-not (Test-Path -LiteralPath $full)) {
            Write-Host "  $objectPath  -> $relative (not on disk; in-memory only)" -ForegroundColor DarkGray
            continue
        }

        $status = & git -C $projectDir status --porcelain -- $relative 2>$null
        $state = if (-not $status) { 'tracked, unchanged' }
                 elseif ($status -match '^\?\?') { 'NEW (untracked)' }
                 else { 'TRACKED AND MODIFIED' }

        Write-Host "  $relative  [$state]" -ForegroundColor $(
            if ($state -eq 'NEW (untracked)') { 'Yellow' }
            elseif ($state -eq 'TRACKED AND MODIFIED') { 'Red' }
            else { 'DarkGray' })

        if ($state -eq 'TRACKED AND MODIFIED') {
            Write-Host "    restore with: git -C `"$projectDir`" checkout -- `"$relative`"" -ForegroundColor Red
        }
        if ($state -eq 'NEW (untracked)' -and $CleanNew) {
            Remove-Item -LiteralPath $full -Force
            Remove-EmptyParents -StartDir (Split-Path -Parent $full) -StopAtDir (Join-Path $projectDir 'Content')
            Write-Host "    deleted (-CleanNew)" -ForegroundColor DarkGray
        }
    }

    if (-not $CleanNew) {
        Write-Host "  (pass -CleanNew to delete the untracked ones — they shadow the editor's in-memory materials)" -ForegroundColor DarkGray
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
