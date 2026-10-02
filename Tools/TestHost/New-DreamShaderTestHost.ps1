<#
.SYNOPSIS
    Creates or checks the DreamShader test host: a minimal project whose Plugins/DreamShader is a git
    worktree of this repository.

.DESCRIPTION
    Safe to run again at any time. In order:

      1. Checks before touching anything: git is on PATH, -RepoPath is a git repository, -Root is not
         inside that repository's working tree, -Root does not already hold another project, and the
         engine the template's .uproject asks for exists.
      2. The worktree. If <Root>/Plugins/DreamShader exists it must be a worktree of the same repository
         (same git common directory); it is reported and never removed, reset or switched. If it does not
         exist it is created with `git worktree add`: on -Branch when that branch exists, as a new branch
         -Branch at -StartPoint when it does not, or detached at -Branch with -Detach.
      3. The template. Missing files are written; matching files are left alone (line endings and a
         byte-order mark do not count as differences); a file that differs is listed with its first
         differing line and kept, unless -Force, which keeps the old one as <name>.bak-<timestamp>.
      4. <Root>/DShader, the host's DreamShader source root, is created.
      5. <Root>/.dreamshader-testhost.json records what the host was made from.
      6. Prints the commands that build the host and run the suite.

    Exit codes: 0 the host matches the template; 1 a check failed (nothing after the failing step was
    done); 2 the host is usable but does not match the template (files kept without -Force, or -WhatIf
    found work to do).

    Creating the worktree is the one thing written outside -Root: git records every worktree in the
    repository's .git/worktrees directory.

.EXAMPLE
    pwsh -NoProfile -File Tools\TestHost\New-DreamShaderTestHost.ps1 -Root 'C:\Users\me\Unreal Projects\58\DreamShaderTestHost' -Branch feat/custom-pass -WhatIf
#>
[CmdletBinding()]
param(
    # The host directory. Defaults to $env:DREAMSHADER_TEST_HOST.
    [string]$Root = $env:DREAMSHADER_TEST_HOST,

    # Any working tree of the DreamShader repository. Defaults to the one holding this script.
    [string]$RepoPath,

    # What the worktree must have, or is created with.
    [string]$Branch = 'main',

    # Where a branch that does not exist yet starts.
    [string]$StartPoint = 'HEAD',

    # Follow -Branch with a detached HEAD: needed when the branch is checked out in another worktree,
    # since git checks a branch out in one worktree at a time.
    [switch]$Detach,

    # The engine root. Defaults to the installed engine the template's .uproject associates with.
    [string]$EngineRoot,

    # Replace template files that differ.
    [switch]$Force,

    # Report only.
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$TemplateDirectory = Join-Path $PSScriptRoot 'Template'
$TemplateVersion = 1
$Mismatch = $false

function Fail([string]$Message)
{
    Write-Host "FAILED: $Message" -ForegroundColor Red
    exit 1
}

function Step([string]$Message)
{
    Write-Host "== $Message" -ForegroundColor Cyan
}

function Get-NormalizedText([string]$Path)
{
    $Text = [System.IO.File]::ReadAllText($Path)
    if ($Text.Length -gt 0 -and $Text[0] -eq [char]0xFEFF) { $Text = $Text.Substring(1) }
    return $Text.Replace("`r`n", "`n")
}

function Get-FullPath([string]$Path)
{
    return [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Test-PathUnder([string]$Path, [string]$Directory)
{
    $P = (Get-FullPath $Path) + '\'
    $D = (Get-FullPath $Directory) + '\'
    return $P.StartsWith($D, [System.StringComparison]::OrdinalIgnoreCase)
}

# ------------------------------------------------------------------------------------------ 1. checks
Step 'Checks'

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail 'git is not on PATH.' }
if ([string]::IsNullOrWhiteSpace($Root)) { Fail 'No -Root given and DREAMSHADER_TEST_HOST is not set.' }
$Root = Get-FullPath $Root

if ([string]::IsNullOrWhiteSpace($RepoPath)) { $RepoPath = $PSScriptRoot }
$RepoTop = (& git -C $RepoPath rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or -not $RepoTop) { Fail "$RepoPath is not inside a git repository." }
$RepoTop = Get-FullPath $RepoTop
$CommonDir = Get-FullPath (& git -C $RepoTop rev-parse --path-format=absolute --git-common-dir)

if (Test-PathUnder $Root $RepoTop) { Fail "-Root ($Root) is inside the repository's working tree ($RepoTop)." }

$TemplateProject = Get-ChildItem -Path $TemplateDirectory -Filter '*.uproject' | Select-Object -First 1
if (-not $TemplateProject) { Fail "No .uproject in $TemplateDirectory." }
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($TemplateProject.Name)

if (Test-Path $Root)
{
    $OtherProjects = @(Get-ChildItem -Path $Root -Filter '*.uproject' -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne $TemplateProject.Name })
    if ($OtherProjects.Count -gt 0) { Fail "$Root already holds another project ($($OtherProjects[0].Name))." }
}

if ([string]::IsNullOrWhiteSpace($EngineRoot))
{
    $Association = (Get-Content $TemplateProject.FullName -Raw | ConvertFrom-Json).EngineAssociation
    $Key = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association"
    if (Test-Path $Key) { $EngineRoot = (Get-ItemProperty $Key).InstalledDirectory }
    if ([string]::IsNullOrWhiteSpace($EngineRoot)) { Fail "No installed engine registered for '$Association'; pass -EngineRoot." }
}
$EngineRoot = Get-FullPath $EngineRoot
if (-not (Test-Path (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'))) { Fail "$EngineRoot is not an engine root." }

Write-Host "  root      $Root"
Write-Host "  repo      $RepoTop"
Write-Host "  engine    $EngineRoot"

# ---------------------------------------------------------------------------------------- 2. worktree
Step 'Worktree'

$WorktreePath = Join-Path $Root 'Plugins\DreamShader'
if (Test-Path $WorktreePath)
{
    $WorktreeCommon = (& git -C $WorktreePath rev-parse --path-format=absolute --git-common-dir 2>$null)
    if ($LASTEXITCODE -ne 0 -or -not $WorktreeCommon) { Fail "$WorktreePath exists but is not a git checkout." }
    if ((Get-FullPath $WorktreeCommon) -ne $CommonDir) { Fail "$WorktreePath is a checkout of another repository ($WorktreeCommon)." }
    $WorktreeTop = Get-FullPath (& git -C $WorktreePath rev-parse --show-toplevel)
    if ($WorktreeTop -eq $RepoTop -and -not (Test-PathUnder $RepoTop $Root)) { Fail "$WorktreePath reaches the repository's own working tree through a link." }
    $Head = (& git -C $WorktreePath rev-parse --abbrev-ref HEAD)
    $Commit = (& git -C $WorktreePath rev-parse --short HEAD)
    if (-not $Detach -and $Head -ne $Branch)
    {
        Write-Host "  kept: the worktree has '$Head' ($Commit), not '$Branch'. It is never switched by this script." -ForegroundColor Yellow
        $Mismatch = $true
    }
    else
    {
        Write-Host "  ok: $WorktreePath on $Head ($Commit)"
    }
}
elseif ($WhatIf)
{
    Write-Host "  would create the worktree $WorktreePath ($Branch)"
    $Mismatch = $true
}
else
{
    $BranchExists = $false
    & git -C $RepoTop show-ref --verify --quiet "refs/heads/$Branch"
    if ($LASTEXITCODE -eq 0) { $BranchExists = $true }

    if ($Detach)           { & git -C $RepoTop worktree add --detach $WorktreePath $Branch }
    elseif ($BranchExists) { & git -C $RepoTop worktree add $WorktreePath $Branch }
    else                   { & git -C $RepoTop worktree add -b $Branch $WorktreePath $StartPoint }
    if ($LASTEXITCODE -ne 0) { Fail 'git worktree add failed.' }
    Write-Host "  created: $WorktreePath"
}

# ---------------------------------------------------------------------------------------- 3. template
Step 'Template'

$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
foreach ($File in Get-ChildItem -Path $TemplateDirectory -Recurse -File)
{
    $Relative = [System.IO.Path]::GetRelativePath($TemplateDirectory, $File.FullName)
    $Target = Join-Path $Root $Relative

    if (-not (Test-Path $Target))
    {
        if ($WhatIf) { Write-Host "  would write $Relative"; $Mismatch = $true; continue }
        New-Item -ItemType Directory -Force -Path (Split-Path $Target) | Out-Null
        Copy-Item $File.FullName $Target
        Write-Host "  wrote $Relative"
        continue
    }

    $Want = Get-NormalizedText $File.FullName
    $Have = Get-NormalizedText $Target
    if ($Want -eq $Have) { continue }

    $WantLines = $Want -split "`n"
    $HaveLines = $Have -split "`n"
    $Line = 0
    while ($Line -lt [Math]::Min($WantLines.Count, $HaveLines.Count) -and $WantLines[$Line] -eq $HaveLines[$Line]) { $Line++ }

    if ($Force -and -not $WhatIf)
    {
        Copy-Item $Target "$Target.bak-$Stamp"
        Copy-Item $File.FullName $Target -Force
        Write-Host "  replaced $Relative (old one kept as .bak-$Stamp)"
    }
    else
    {
        Write-Host "  differs: $Relative, first at line $($Line + 1)" -ForegroundColor Yellow
        $Mismatch = $true
    }
}

# ------------------------------------------------------------------------------- 4. the source root
$SourceRoot = Join-Path $Root 'DShader'
if (-not (Test-Path $SourceRoot))
{
    if ($WhatIf) { Write-Host '  would create DShader/'; $Mismatch = $true }
    else { New-Item -ItemType Directory -Force -Path $SourceRoot | Out-Null; Write-Host '  created DShader/' }
}

# ------------------------------------------------------------------------------------- 5. the record
$RecordPath = Join-Path $Root '.dreamshader-testhost.json'
if (-not $WhatIf -and (Test-Path $WorktreePath))
{
    $Record = [ordered]@{
        templateVersion = $TemplateVersion
        repository      = $RepoTop
        branch          = (& git -C $WorktreePath rev-parse --abbrev-ref HEAD)
        engine          = $EngineRoot
    }
    $Json = ($Record | ConvertTo-Json) + "`n"
    if (-not (Test-Path $RecordPath) -or (Get-NormalizedText $RecordPath) -ne $Json.Replace("`r`n", "`n"))
    {
        [System.IO.File]::WriteAllText($RecordPath, $Json)
        Write-Host '  wrote .dreamshader-testhost.json'
    }
}

# ---------------------------------------------------------------------------------- 6. what to do next
Step 'Next'
$Project = Join-Path $Root "$ProjectName.uproject"
Write-Host "  build: & '$EngineRoot\Engine\Build\BatchFiles\Build.bat' ${ProjectName}Editor Win64 Development `"-Project=$Project`" -WaitMutex -NoHotReloadFromIDE -NoEngineChanges"
Write-Host "  test:  pwsh -NoProfile -File '$WorktreePath\Tools\Tests\Invoke-DreamShaderTests.ps1' -Project '$Project' -Preset Suite"

if ($Mismatch) { exit 2 }
exit 0
