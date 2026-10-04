<#
.SYNOPSIS
    Builds the DreamShader test host and runs one preset of the automation suite in it.

.DESCRIPTION
    The host is the project this script finds four levels up (Plugins/DreamShader/Tools/Tests), or the one
    -Project names; see Tools/TestHost/README.md for how it is made. Never point this at a project whose
    editor is open: building links the plugin's DLLs, and a run needs an editor process of its own.

    Presets:

      Suite   every DreamShader test, -nullrhi. The tests that need a real RHI fail here and are reported
              as expected (see $ExpectedUnderNullRhi), not as failures.
      Fast    the Core-only layers and the Custom Pass logic tests, -nullrhi. Seconds, no assets.
      Rhi     the tests that need a renderer -- Custom Pass render and lifecycle, the HLSL slot registry and its
              pre-check, pixel parity, preview probes --
              with a real RHI (-RenderOffscreen, D3D12, no window).

    -WithoutCustomPass builds the plugin the way an engine before 5.8 builds it -- the Custom Pass asset types
    without the renderer half -- so one 5.8 machine tests both sides of DREAMSHADER_WITH_CUSTOM_PASS. The
    next build without it puts the renderer back; nothing else needs resetting. Before running, the script
    reads which of the two the binaries are and stops if that is not the one asked for (say, -NoBuild after
    a build of the other kind).

    The report lands in <host>/Saved/DreamShaderTests/<preset>-<stamp>/ (index.json from the engine, the
    editor log, and summary.txt). The exit code is the number of unexpected failures, capped at 100;
    a run that produced no report exits 101.

.EXAMPLE
    pwsh -NoProfile -File Tools\Tests\Invoke-DreamShaderTests.ps1 -Preset Fast

.EXAMPLE
    pwsh -NoProfile -File Tools\Tests\Invoke-DreamShaderTests.ps1 -Preset Rhi -NoBuild -Filter DreamShader.Pass.Render.Highlight

.EXAMPLE
    pwsh -NoProfile -File Tools\Tests\Invoke-DreamShaderTests.ps1 -Preset Fast -WithoutCustomPass
#>
[CmdletBinding()]
param(
    [ValidateSet('Suite', 'Fast', 'Rhi')]
    [string]$Preset = 'Suite',

    # Replaces the preset's filter (any test-name prefix; '+' joins several).
    [string]$Filter,

    [string]$Project = $env:DREAMSHADER_TEST_PROJECT,

    [string]$Engine,

    [switch]$NoBuild,

    # Builds with -ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS: DREAMSHADER_WITH_CUSTOM_PASS is 0 on any engine.
    [switch]$WithoutCustomPass,

    # Extra arguments for the editor, e.g. -DreamShaderUpdateGolden.
    [string[]]$ExtraArguments = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Tests that need what -nullrhi takes away. Under the Suite preset a failure of one of these is expected.
$ExpectedUnderNullRhi = @(
    'DreamShader.Preview.ProbePreview.RendersBinding'
)

# Tests that need content this host does not carry (the MoonToon project plugin's assets).
$ExpectedMissingContent = @(
    'DreamShader.Compiler2.Parity.Function'
)

$Presets = @{
    Suite = @{ Filter = 'DreamShader'; Rhi = $false }
    Fast  = @{ Filter = 'DreamShader.Lang+DreamShader.Lang2+DreamShader.Pass.Logic'; Rhi = $false }
    Rhi   = @{ Filter = 'DreamShader.Pass.Render+DreamShader.Pass.Lifecycle+DreamShader.Compiler2.Pipeline.Registry+DreamShader.Render+DreamShader.Preview'; Rhi = $true }
}

# ------------------------------------------------------------------------------------------ inputs
if ([string]::IsNullOrWhiteSpace($Project))
{
    $HostRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
    $Found = Get-ChildItem -Path $HostRoot -Filter '*.uproject' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $Found) { throw "No -Project given, DREAMSHADER_TEST_PROJECT is not set, and $HostRoot holds no .uproject." }
    $Project = $Found.FullName
}
$Project = [System.IO.Path]::GetFullPath($Project)
$ProjectDir = Split-Path $Project
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($Project)

if ([string]::IsNullOrWhiteSpace($Engine))
{
    $Association = (Get-Content $Project -Raw | ConvertFrom-Json).EngineAssociation
    $Key = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association"
    if (Test-Path $Key) { $Engine = (Get-ItemProperty $Key).InstalledDirectory }
    if ([string]::IsNullOrWhiteSpace($Engine)) { throw "No installed engine registered for '$Association'; pass -Engine." }
}

$Settings = $Presets[$Preset]
$RunFilter = if ([string]::IsNullOrWhiteSpace($Filter)) { $Settings.Filter } else { $Filter }

$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$ReportDir = Join-Path $ProjectDir "Saved\DreamShaderTests\$Preset-$Stamp"
$LogPath = Join-Path $ReportDir 'Editor.log'

Write-Host "DreamShader tests -- $Preset"
Write-Host "  project  $Project"
Write-Host "  engine   $Engine"
Write-Host "  filter   $RunFilter"
Write-Host "  report   $ReportDir"

# ------------------------------------------------------------------------------------------- build
if (-not $NoBuild)
{
    $BuildBat = Join-Path $Engine 'Engine\Build\BatchFiles\Build.bat'
    $BuildArguments = @("${ProjectName}Editor", 'Win64', 'Development', "-Project=$Project", '-WaitMutex', '-NoHotReloadFromIDE', '-NoEngineChanges')
    if ($WithoutCustomPass) { $BuildArguments += '-ProjectDefine:DREAMSHADER_FORCE_NO_CUSTOM_PASS' }
    $Started = Get-Date
    & $BuildBat @BuildArguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }
    Write-Host ("  built in {0:N0} s" -f ((Get-Date) - $Started).TotalSeconds)
}

# ------------------------------------------------------------------------------------- custom pass
# Which side of DREAMSHADER_WITH_CUSTOM_PASS the binaries about to run are on, read from the definitions UBT wrote
# for DreamShaderPass rather than assumed from this run's switches: -NoBuild runs whatever the last build left.
$CustomPassBuilt = $null
$Uplugin = Get-ChildItem -Path (Join-Path $ProjectDir 'Plugins') -Filter 'DreamShader.uplugin' -Recurse -Depth 3 -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($Uplugin)
{
    $Header = @('Intermediate\Build\Win64\x64\UnrealEditor\Development', 'Intermediate\Build\Win64\UnrealEditor\Development') |
        ForEach-Object { Join-Path $Uplugin.DirectoryName "$_\DreamShaderPass\Definitions.DreamShaderPass.h" } |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($Header)
    {
        $Match = Select-String -Path $Header -Pattern '^#define DREAMSHADER_WITH_CUSTOM_PASS (\d)' | Select-Object -First 1
        if ($Match) { $CustomPassBuilt = $Match.Matches[0].Groups[1].Value -eq '1' }
    }
}
$Version = Get-Content (Join-Path $Engine 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
$EngineHasCustomPass = $Version.MajorVersion -gt 5 -or ($Version.MajorVersion -eq 5 -and $Version.MinorVersion -ge 8)

if ($WithoutCustomPass -and $CustomPassBuilt -eq $true)
{
    throw 'DreamShaderPass was built with the Custom Pass renderer, not -WithoutCustomPass; run without -NoBuild.'
}
if (-not $WithoutCustomPass -and $CustomPassBuilt -eq $false -and $EngineHasCustomPass)
{
    throw 'DreamShaderPass was built -WithoutCustomPass; run without -NoBuild to build the Custom Pass renderer back in.'
}
$CustomPassState = if ($null -eq $CustomPassBuilt) { 'unknown' } elseif ($CustomPassBuilt) { 'compiled in' } else { 'compiled out' }
Write-Host "  custom pass  $CustomPassState"

# --------------------------------------------------------------------------------------------- run
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
$EditorCmd = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Arguments = @(
    "`"$Project`"",
    "-ExecCmds=`"Automation RunTests $RunFilter; Quit`"",
    '-unattended', '-nopause', '-nosplash', '-NoDreamShaderEditorBridge',
    "-ReportExportPath=`"$ReportDir`"",
    "-abslog=`"$LogPath`""
)
if ($Settings.Rhi) { $Arguments += @('-RenderOffscreen', '-d3d12') } else { $Arguments += '-nullrhi' }
$Arguments += $ExtraArguments

$Started = Get-Date
$Process = Start-Process -FilePath $EditorCmd -ArgumentList $Arguments -NoNewWindow -PassThru -Wait
Write-Host ("  ran in {0:N0} s, editor exit {1}" -f ((Get-Date) - $Started).TotalSeconds, $Process.ExitCode)

# ------------------------------------------------------------------------------------------ report
$IndexPath = Join-Path $ReportDir 'index.json'
if (-not (Test-Path $IndexPath))
{
    Write-Host "No index.json -- the editor did not finish the run. See $LogPath." -ForegroundColor Red
    exit 101
}

# The engine writes index.json with a byte-order mark and, on some versions, trailing commas.
$IndexText = [System.IO.File]::ReadAllText($IndexPath).TrimStart([char]0xFEFF)
$IndexText = [System.Text.RegularExpressions.Regex]::Replace($IndexText, ',\s*([\]}])', '$1')
$Index = $IndexText | ConvertFrom-Json

$Expected = @()
if (-not $Settings.Rhi) { $Expected += $ExpectedUnderNullRhi }
$Expected += $ExpectedMissingContent

$Passed = 0
$Unexpected = New-Object System.Collections.Generic.List[string]
$ExpectedFailed = New-Object System.Collections.Generic.List[string]
$Lines = New-Object System.Collections.Generic.List[string]

foreach ($Test in $Index.tests)
{
    $Name = $Test.fullTestPath
    if ($Test.state -eq 'Success') { $Passed++; continue }

    $Errors = @($Test.entries | Where-Object { $_.event.type -eq 'Error' } | Select-Object -First 3 | ForEach-Object { $_.event.message })
    if ($Expected | Where-Object { $Name -like "$_*" })
    {
        $ExpectedFailed.Add($Name)
        continue
    }

    $Unexpected.Add($Name)
    $Lines.Add("FAIL $Name")
    foreach ($Message in $Errors) { $Lines.Add("     $Message") }
}

$Lines.Insert(0, ("{0}: {1} passed, {2} failed, {3} expected failures" -f $Preset, $Passed, $Unexpected.Count, $ExpectedFailed.Count))
if ($CustomPassBuilt -eq $false) { $Lines.Insert(1, 'custom pass compiled out') }
foreach ($Name in $ExpectedFailed) { $Lines.Add("expected: $Name") }

$Lines | Set-Content -Path (Join-Path $ReportDir 'summary.txt') -Encoding utf8
$Lines | ForEach-Object { Write-Host $_ }

exit [Math]::Min($Unexpected.Count, 100)
