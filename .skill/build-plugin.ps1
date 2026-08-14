<#
.SYNOPSIS
    Standalone `RunUAT BuildPlugin` matrix for DreamShader, one engine per run.

.DESCRIPTION
    Builds the plugin on its own -- no host project target -- against every engine root passed
    in, and reports one PASS/FAIL line per engine. This is the check that catches an
    engine-version gate that compiles on the engine you happen to have open and nowhere else;
    a project build only ever exercises one engine.

    Each engine gets its own `-Package` directory, so the runs never share Intermediate state
    and the plugin's own Binaries/ and Intermediate/ are left untouched.

    A build's full UAT log is kept under <Package>/Logs even when the build passes. The console
    shows only the compiler diagnostics and UAT's own step lines unless -Raw is passed.

    Exit code is the number of engines that failed, so 0 means the whole matrix is green.

.EXAMPLE
    ./build-plugin.ps1 I:\UnrealEngine\UE_5.3, I:\UnrealEngine\UE_5.4

.EXAMPLE
    ./build-plugin.ps1 -Engine (Get-ChildItem I:\UnrealEngine\UE_5.* -Directory).FullName -StopOnFailure

.NOTES
    Sequential on purpose: two UAT runs on one machine contend for the same compiler processes
    and the timings stop meaning anything.

    Read the file paths in a failure before blaming the plugin. UE 5.3 and 5.4 do not compile
    under a recent MSVC at all -- ConcurrentLinearAllocator.h raises C4668 on an unguarded
    __has_feature and UBT builds with /we4668 -- so those two fail inside *engine* headers with no
    plugin file named, and Epic's own plugins fail there identically. BuildPlugin has no toolchain
    switch; the toolchain lives in BuildConfiguration.xml, and no installed toolchain avoids it.
    See Docs/contributing/index.md, "Engine versions".
#>
[CmdletBinding()]
param(
    # Engine roots -- the directory holding Engine\Build\BatchFiles\RunUAT.bat.
    [Parameter(Mandatory, Position = 0)]
    [string[]]$Engine,

    # The .uplugin. Defaults to the one at or above this script.
    [string]$Plugin,

    # Staging root. Each engine builds into <Package>\<EngineLabel>. Must not overlap the
    # plugin tree -- UAT stages a clean copy there and refuses to nest it inside the source.
    [string]$Package,

    # Passed straight to BuildPlugin, in UAT's own syntax: '+'-separated, not commas.
    [string]$TargetPlatforms = 'Win64',

    # Drop -Rocket. Only useful when a build must see engine source it would otherwise be
    # told to treat as an installed build.
    [switch]$NoRocket,

    # Extra arguments appended to every UAT invocation, e.g. -StrictIncludes.
    [string[]]$ExtraArguments,

    # Stop at the first failing engine instead of finishing the matrix.
    [switch]$StopOnFailure,

    # Keep the staged plugin output. Without it only the log survives -- a green BuildPlugin
    # is ~300 MB per engine and the binaries are not the point of this script.
    [switch]$KeepOutput,

    # Echo the whole UAT log instead of just diagnostics and step lines.
    [switch]$Raw
)

$ErrorActionPreference = 'Stop'

# A note for whoever chases this next: on a localized Windows the prose half of a cl.exe diagnostic
# arrives mangled. It is not fixable from here. UnrealBuildTool reads cl.exe's pipe with the wrong
# encoding and hands this script text that is already wrong -- run cl.exe straight from PowerShell
# and the same message comes back perfectly -- and VSLANG=1033 does not help either unless the
# English compiler resource pack (bin\Hostx64\x64\1033) is installed, which it usually is not.
# The machine-readable half survives: file, line, `error C4668`, and that is what the report keys on.

# ---------------------------------------------------------------- inputs

function Resolve-PluginFile {
    param([string]$Explicit)

    if ($Explicit) {
        if (-not (Test-Path -LiteralPath $Explicit)) { throw "No .uplugin at '$Explicit'." }
        return (Resolve-Path -LiteralPath $Explicit).Path
    }

    $dir = $PSScriptRoot
    while ($dir) {
        $found = @(Get-ChildItem -LiteralPath $dir -Filter '*.uplugin' -File -ErrorAction SilentlyContinue)
        if ($found.Count -gt 0) { return $found[0].FullName }
        $dir = Split-Path -Parent $dir
    }

    throw "Could not find a .uplugin at or above '$PSScriptRoot'. Pass -Plugin."
}

function Expand-EngineList {
    <#
        `pwsh -File` hands every argument through as one literal string, so an array written
        at a call site arrives as "'I:\UE_5.3','I:\UE_5.4'" -- quotes and all. Splitting and
        unquoting here means -Engine takes a real array, a comma-separated string, or the
        mangled middle ground, and all three mean the same thing.
    #>
    param([string[]]$Entries)

    $expanded = foreach ($entry in $Entries) {
        foreach ($part in ($entry -split '[,;]')) {
            $trimmed = $part.Trim().Trim('"', "'").TrimEnd('\', '/')
            if ($trimmed) { $trimmed }
        }
    }

    return @($expanded)
}

function Get-EngineLabel {
    <#
        "UE_5.3" from the engine root, falling back to the Build.version numbers when the
        directory is named something else. Used for the package subdirectory and the report.
    #>
    param([string]$EngineRoot)

    $versionFile = Join-Path $EngineRoot 'Engine\Build\Build.version'
    if (Test-Path -LiteralPath $versionFile) {
        $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
        return "UE_$($version.MajorVersion).$($version.MinorVersion)"
    }

    return (Split-Path -Leaf $EngineRoot)
}

function Get-EngineVersion {
    param([string]$EngineRoot)

    $versionFile = Join-Path $EngineRoot 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $versionFile)) { return 'unknown' }

    $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
    return "$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
}

$Engine = Expand-EngineList -Entries $Engine
if ($Engine.Count -eq 0) { throw '-Engine resolved to no engine roots.' }

$pluginFile = Resolve-PluginFile -Explicit $Plugin
$pluginDir = Split-Path -Parent $pluginFile
$pluginName = [System.IO.Path]::GetFileNameWithoutExtension($pluginFile)

if (-not $Package) {
    $Package = Join-Path ([System.IO.Path]::GetTempPath()) "${pluginName}BuildPlugin"
}
$Package = [System.IO.Path]::GetFullPath($Package)

# UAT stages into -Package and refuses a directory inside the plugin it is copying.
if ($Package.StartsWith($pluginDir, [StringComparison]::OrdinalIgnoreCase)) {
    throw "-Package '$Package' is inside the plugin tree. Pick a directory outside '$pluginDir'."
}

$logDir = Join-Path $Package 'Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null

$descriptor = Get-Content -LiteralPath $pluginFile -Raw | ConvertFrom-Json

Write-Host ''
Write-Host "$pluginName $($descriptor.VersionName) -- BuildPlugin matrix" -ForegroundColor Cyan
Write-Host "  plugin   $pluginFile" -ForegroundColor DarkGray
Write-Host "  package  $Package" -ForegroundColor DarkGray
Write-Host "  platforms $TargetPlatforms" -ForegroundColor DarkGray

# ---------------------------------------------------------------- run

# UAT is loud. These are the lines worth seeing live; everything else is in the log file.
$interesting = '(?i)(\berror\b|\bwarning\b|\bfatal\b|^\s*BUILD (FAILED|SUCCESSFUL)|AutomationTool exiting|Running UnrealBuildTool|Compiling |Took \d)'

# A diagnostic worth reprinting in the failure summary. Deliberately narrower than the live filter:
# anything looser matches UAT's own prose and, memorably, a unity-build file list that happened to
# contain Error.cpp.
$diagnostic = '(?i)((fatal )?error\s+(C|LNK|MSB|D)\d+|:\s*error\s*:|^\s*ERROR:|UnrealHeaderTool failed)'

$results = [System.Collections.Generic.List[object]]::new()

foreach ($engineRoot in $Engine) {
    $engineRoot = $engineRoot.TrimEnd('\', '/')
    $label = Get-EngineLabel -EngineRoot $engineRoot
    $uat = Join-Path $engineRoot 'Engine\Build\BatchFiles\RunUAT.bat'

    Write-Host ''
    Write-Host "=== $label ($(Get-EngineVersion -EngineRoot $engineRoot)) -- $engineRoot ===" -ForegroundColor Cyan

    if (-not (Test-Path -LiteralPath $uat)) {
        Write-Host "  RunUAT.bat not found at '$uat'." -ForegroundColor Red
        $results.Add([PSCustomObject]@{
                Engine      = $label
                Version     = '-'
                Status      = 'MISSING'
                Seconds     = 0
                Diagnostics = @("RunUAT.bat not found at '$uat'.")
                Log         = ''
            })
        if ($StopOnFailure) { break }
        continue
    }

    $outputDir = Join-Path $Package $label
    $logFile = Join-Path $logDir "$label.log"
    Remove-Item -LiteralPath $outputDir -Recurse -Force -ErrorAction SilentlyContinue

    $uatArguments = @(
        'BuildPlugin',
        "-Plugin=$pluginFile",
        "-Package=$outputDir",
        "-TargetPlatforms=$TargetPlatforms"
    )
    if (-not $NoRocket) { $uatArguments += '-Rocket' }
    if ($ExtraArguments) { $uatArguments += $ExtraArguments }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $captured = [System.Collections.Generic.List[string]]::new()

    # 2>&1 folds UAT's stderr into the same stream; ForEach-Object flattens the ErrorRecords
    # that would otherwise make $ErrorActionPreference='Stop' abort the whole matrix.
    & $uat @uatArguments 2>&1 | ForEach-Object {
        $line = "$_"
        $captured.Add($line)
        if ($Raw) { Write-Host $line }
        elseif ($line -match $interesting) { Write-Host "  $line" -ForegroundColor DarkGray }
    }

    $exit = $LASTEXITCODE
    $stopwatch.Stop()

    Set-Content -LiteralPath $logFile -Value $captured -Encoding UTF8

    $diagnostics = @($captured | Where-Object { $_ -match $diagnostic } | Select-Object -Unique)

    $status = if ($exit -eq 0) { 'PASS' } else { "FAIL($exit)" }
    $colour = if ($exit -eq 0) { 'Green' } else { 'Red' }
    Write-Host "  $status in $([int]$stopwatch.Elapsed.TotalSeconds)s -- log: $logFile" -ForegroundColor $colour

    $results.Add([PSCustomObject]@{
            Engine      = $label
            Version     = (Get-EngineVersion -EngineRoot $engineRoot)
            Status      = $status
            Seconds     = [int]$stopwatch.Elapsed.TotalSeconds
            Diagnostics = $diagnostics
            Log         = $logFile
        })

    if (-not $KeepOutput) {
        Remove-Item -LiteralPath $outputDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    if ($exit -ne 0 -and $StopOnFailure) { break }
}

# ---------------------------------------------------------------- report

Write-Host ''
Write-Host '=== Summary ===' -ForegroundColor Cyan
$results | Format-Table -AutoSize -Property Engine, Version, Status, Seconds

$failed = @($results | Where-Object { $_.Status -ne 'PASS' })

foreach ($failure in $failed) {
    Write-Host ''
    Write-Host "$($failure.Engine) diagnostics:" -ForegroundColor Red
    if ($failure.Diagnostics.Count -eq 0) {
        Write-Host "  (none matched; read $($failure.Log))" -ForegroundColor Yellow
    }
    else {
        # More than a screenful is a cascade, not new information -- the log has the rest.
        foreach ($line in $failure.Diagnostics | Select-Object -First 20) {
            Write-Host "  $line" -ForegroundColor Red
        }
        if ($failure.Diagnostics.Count -gt 20) {
            Write-Host "  ... $($failure.Diagnostics.Count - 20) more in $($failure.Log)" -ForegroundColor DarkGray
        }
    }
}

Write-Host ''
if ($failed.Count -eq 0) {
    Write-Host "build-plugin: OK -- $($results.Count)/$($results.Count) engines green" -ForegroundColor Green
}
else {
    Write-Host "build-plugin: FAILED on $($failed.Count) of $($results.Count) engines" -ForegroundColor Red
}

exit $failed.Count
