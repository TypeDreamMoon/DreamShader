<#
.SYNOPSIS
    Builds the Custom Pass demo level in the DreamShader test host and, with -Shots, renders it in a game window.

.DESCRIPTION
    Seven pipelines on one level, each activated the way a project would: the four of Docs/examples/custom-pass.md
    (highlight outline, X-ray, UI backdrop, wind field) and three more (a scanner pulse and an old CRT in HLSL, and a
    stencil-selected mesh pass that draws with each object's own material). See README.md beside this script.

      1. Sources/ is copied into <host>/DShader/PassDemo.
      2. `-run=DreamShader compile -All` builds every source of the host: the demo's pipelines, materials and
         instances, and the HLSL pre-check of its slots. Unchanged sources are skipped.
      3. BuildLevel.py builds /Game/PassDemo/L_PassDemo in a Python commandlet.
      4. With -Shots, the level runs in -game with TakeShots.py, which takes one screenshot per pipeline on its own
         and one of all of them into <host>/Saved/PassDemo/Shots. CP_WindField is activated from the project
         settings there, by -ini on the command line, with the wind blowing sideways so the grass visibly bends.

    The host must be built (Tools/Tests/Invoke-DreamShaderTests.ps1 builds it) and no editor may have it open. The
    pass layers Highlight and XRay come from the template's Config/DefaultEngine.ini. Logs land in
    <host>/Saved/PassDemo. The exit code is 0 when every step succeeded.

.EXAMPLE
    pwsh -NoProfile -File Tools\TestHost\PassDemo\Build-PassDemo.ps1 -Shots
#>
[CmdletBinding()]
param(
    [string]$Project = $env:DREAMSHADER_TEST_PROJECT,

    [string]$Engine,

    # Also run the level in a game window and take the screenshots.
    [switch]$Shots,

    # How long the game run may take before it is stopped.
    [int]$TimeoutMinutes = 20
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ------------------------------------------------------------------------------------------ inputs
if ([string]::IsNullOrWhiteSpace($Project))
{
    $HostRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..\..'))
    $Found = Get-ChildItem -Path $HostRoot -Filter '*.uproject' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $Found) { throw "No -Project given, DREAMSHADER_TEST_PROJECT is not set, and $HostRoot holds no .uproject." }
    $Project = $Found.FullName
}
$Project = [System.IO.Path]::GetFullPath($Project)
$ProjectDir = Split-Path $Project

if ([string]::IsNullOrWhiteSpace($Engine))
{
    $Association = (Get-Content $Project -Raw | ConvertFrom-Json).EngineAssociation
    $Key = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association"
    if (Test-Path $Key) { $Engine = (Get-ItemProperty $Key).InstalledDirectory }
    if ([string]::IsNullOrWhiteSpace($Engine)) { throw "No installed engine registered for '$Association'; pass -Engine." }
}
$EditorCmd = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'

$OutDir = Join-Path $ProjectDir 'Saved\PassDemo'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Invoke-Commandlet([string]$Name, [string[]]$Arguments)
{
    $Log = Join-Path $OutDir "$Name.log"
    $All = @("`"$Project`"") + $Arguments + @('-unattended', '-nopause', '-nullrhi', '-nosplash', '-NoDreamShaderEditorBridge', "`"-abslog=$Log`"")
    $Started = Get-Date
    $Process = Start-Process -FilePath $EditorCmd -ArgumentList $All -WindowStyle Hidden -PassThru -Wait
    Write-Host ("  {0}: exit {1} in {2:N0} s ({3})" -f $Name, $Process.ExitCode, ((Get-Date) - $Started).TotalSeconds, $Log)
    if ($Process.ExitCode -ne 0) { throw "$Name failed (exit $($Process.ExitCode)); see $Log." }
}

Write-Host 'DreamShader Custom Pass demo'
Write-Host "  project  $Project"
Write-Host "  engine   $Engine"

# ------------------------------------------------------------------------------------------ sources
$SourceDir = Join-Path $ProjectDir 'DShader\PassDemo'
New-Item -ItemType Directory -Force -Path $SourceDir | Out-Null
Copy-Item -Path (Join-Path $PSScriptRoot 'Sources\*') -Destination $SourceDir -Force
Write-Host "  sources  $SourceDir"

# ------------------------------------------------------------------------------------------ compile
Invoke-Commandlet 'Compile' @('-run=DreamShader', 'compile', '-All')

# ------------------------------------------------------------------------------------------ level
$LevelScript = (Join-Path $PSScriptRoot 'BuildLevel.py').Replace('\', '/')
Invoke-Commandlet 'BuildLevel' @('-run=pythonscript', "`"-script=$LevelScript`"")

if (-not $Shots)
{
    Write-Host '  level    /Game/PassDemo/L_PassDemo (open it in the editor, or run again with -Shots)'
    exit 0
}

# ------------------------------------------------------------------------------------------ shots
$ShotDir = Join-Path $OutDir 'Shots'
$ShotScript = (Join-Path $PSScriptRoot 'TakeShots.py').Replace('\', '/')
$GameLog = Join-Path $OutDir 'Game.log'

# CP_WindField from the project settings, with the wind blowing along +Y: across the view, so the grass leans.
$WindSettings = '-ini:Engine:[/Script/DreamShaderPass.DreamPassSettings]:GlobalPipelines=(Pipeline=/Game/PassDemo/CP_WindField.CP_WindField,' +
    'Overrides=((Name=WindDirection,Value=(Type=Float2,Vector=(X=0.0,Y=1.0,Z=0.0,W=0.0))),(Name=Gust,Value=(Type=Float,Vector=(X=1.5,Y=0.0,Z=0.0,W=0.0)))))'
$GameArguments = @("`"$Project`"", '/Game/PassDemo/L_PassDemo', '-game', '-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-unattended',
    '-nosound', "`"-abslog=$GameLog`"", "-ExecCmds=`"py $ShotScript`"", $WindSettings)

$env:DREAMSHADER_PASSDEMO_SHOTS = $ShotDir
$Started = Get-Date
$Process = Start-Process -FilePath $Editor -ArgumentList $GameArguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60 * 1000))
{
    Stop-Process -Id $Process.Id -Force
    throw "The game run took longer than $TimeoutMinutes minutes and was stopped; see $GameLog."
}
Write-Host ("  shots    exit {0} in {1:N0} s ({2})" -f $Process.ExitCode, ((Get-Date) - $Started).TotalSeconds, $ShotDir)

$Summary = Join-Path $ShotDir 'TakeShots.txt'
if (-not (Test-Path $Summary)) { throw "The driver wrote no summary; see $GameLog." }
$Lines = Get-Content $Summary
$Lines | ForEach-Object { Write-Host "    $_" }
if (-not ($Lines -contains 'done')) { throw "The driver did not finish; see $GameLog." }
exit 0
