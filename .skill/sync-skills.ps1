<#
.SYNOPSIS
    Publish .skill/ into a .claude/skills/ directory so Claude Code auto-loads it.

.DESCRIPTION
    Claude Code discovers skills from `.claude/skills/`, searching the directory tree at and
    *above* where the agent is working. `.skill/` is not on that path, so the skills have to be
    published into one that is.

    A symlink or junction is not enough: each SKILL.md links to `../../Docs/…`, which resolves
    correctly from `.skill/<skill>/` and not from `.claude/skills/<skill>/`. This script copies the
    tree and rewrites those two path families to whatever is correct at the destination:

        ](../../Docs/…                        -> the real relative path to <plugin>/Docs
        pwsh -File Plugins/DreamShader/.skill -> the real relative path to the driver

    `.skill/` stays the source of truth. Re-run after editing it; `-Check` reports drift and exits
    1, which makes it usable as a pre-commit gate.

.EXAMPLE
    ./sync-skills.ps1
    Publish to the host project's .claude/skills (the nearest .uproject above the plugin).

.EXAMPLE
    ./sync-skills.ps1 -Check
    Report whether the published copy is stale. Exit 1 if it is.

.EXAMPLE
    ./sync-skills.ps1 -Target I:/Other/Project/.claude/skills
#>
[CmdletBinding()]
param(
    # The .claude/skills directory to publish into. Defaults to the host project's, falling back
    # to the plugin's own when the plugin repo is checked out standalone.
    [string]$Target,

    # Compare instead of writing. Exit 1 when the published copy differs from what would be written.
    [switch]$Check,

    # Remove published dream-shader-* / reference directories that this run did not write.
    [switch]$Prune
)

$ErrorActionPreference = 'Stop'

$skillRoot = $PSScriptRoot
$pluginRoot = Split-Path -Parent $skillRoot
$docsDir = Join-Path $pluginRoot 'Docs'
$driverPath = Join-Path $skillRoot 'dsc.ps1'

function ConvertTo-RelativePosix {
    # GetRelativePath is lexical, so both arguments must already be resolved.
    param([string]$From, [string]$To)
    return ([System.IO.Path]::GetRelativePath($From, $To)) -replace '\\', '/'
}

# ---------------------------------------------------------------- destination

if (-not $Target) {
    # The host project is the nearest directory above the plugin holding a .uproject.
    $projectRoot = $null
    $dir = Split-Path -Parent $pluginRoot
    while ($dir) {
        if (@(Get-ChildItem -LiteralPath $dir -Filter '*.uproject' -File -ErrorAction SilentlyContinue).Count -gt 0) {
            $projectRoot = $dir
            break
        }
        $dir = Split-Path -Parent $dir
    }
    if (-not $projectRoot) { $projectRoot = $pluginRoot }   # standalone plugin checkout
    $Target = Join-Path $projectRoot '.claude/skills'
}

# The directory the code blocks assume you are standing in: the parent of .claude.
$workingRoot = Split-Path -Parent (Split-Path -Parent $Target)

if (-not $Check) {
    New-Item -ItemType Directory -Path $Target -Force | Out-Null
}
elseif (-not (Test-Path -LiteralPath $Target)) {
    Write-Host "not published: $Target does not exist" -ForegroundColor Yellow
    exit 1
}

$targetFull = (Resolve-Path -LiteralPath $Target).Path
$workingFull = if (Test-Path -LiteralPath $workingRoot) { (Resolve-Path -LiteralPath $workingRoot).Path } else { $workingRoot }

# Every published skill sits one level under $Target, so one prefix serves them all.
$docsPrefix = ConvertTo-RelativePosix -From (Join-Path $targetFull 'any-skill') -To $docsDir
$driverRel = ConvertTo-RelativePosix -From $workingFull -To $driverPath

$marker = '<!-- Published from Plugins/DreamShader/.skill by sync-skills.ps1. Edit the source, not this copy. -->'

function Get-PublishedText {
    param([string]$Path)

    $text = Get-Content -LiteralPath $Path -Raw

    # `](../../Docs/x.md)` is written for .skill/<skill>/; retarget it at the real Docs tree.
    $text = $text -replace '\]\(\.\./\.\./Docs/', "]($docsPrefix/"

    # The driver invocation is written relative to the host project root.
    $text = $text.Replace('Plugins/DreamShader/.skill/dsc.ps1', $driverRel)

    # Mark the copy, after the frontmatter so the `---` block stays first.
    if ($text -match '(?s)^(---\r?\n.*?\r?\n---\r?\n)') {
        $frontmatter = $Matches[1]
        $text = $frontmatter + $marker + [Environment]::NewLine + $text.Substring($frontmatter.Length)
    }
    else {
        $text = $marker + [Environment]::NewLine + $text
    }

    return $text
}

# ---------------------------------------------------------------- publish

$sources = @(Get-ChildItem -LiteralPath $skillRoot -Directory | Where-Object { $_.Name -like 'dream-shader-*' -or $_.Name -eq 'reference' })
if ($sources.Count -eq 0) { throw "No dream-shader-* directories under '$skillRoot'." }

Write-Host "source:  $skillRoot" -ForegroundColor DarkGray
Write-Host "target:  $targetFull" -ForegroundColor DarkGray
Write-Host "Docs ->  $docsPrefix" -ForegroundColor DarkGray
Write-Host "driver ->$driverRel" -ForegroundColor DarkGray
Write-Host ''

$drift = @()
$written = 0
$publishedNames = @()

foreach ($source in $sources) {
    $publishedNames += $source.Name
    $destDir = Join-Path $targetFull $source.Name

    foreach ($file in Get-ChildItem -LiteralPath $source.FullName -File -Recurse) {
        $relative = $file.FullName.Substring($source.FullName.Length).TrimStart('\', '/')
        $dest = Join-Path $destDir $relative
        $wanted = if ($file.Extension -eq '.md') { Get-PublishedText -Path $file.FullName } else { Get-Content -LiteralPath $file.FullName -Raw }

        $current = if (Test-Path -LiteralPath $dest) { Get-Content -LiteralPath $dest -Raw } else { $null }

        if ($current -eq $wanted) {
            Write-Host "  = $($source.Name)/$relative" -ForegroundColor DarkGray
            continue
        }

        $drift += "$($source.Name)/$relative"

        if ($Check) {
            $state = if ($null -eq $current) { 'missing' } else { 'stale' }
            Write-Host "  ! $($source.Name)/$relative  [$state]" -ForegroundColor Yellow
            continue
        }

        New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
        Set-Content -LiteralPath $dest -Value $wanted -NoNewline -Encoding utf8
        Write-Host "  + $($source.Name)/$relative" -ForegroundColor Green
        $written++
    }
}

if ($Prune -and -not $Check) {
    foreach ($stale in Get-ChildItem -LiteralPath $targetFull -Directory) {
        if ($stale.Name -notin $publishedNames -and ($stale.Name -like 'dream-shader-*' -or $stale.Name -eq 'reference')) {
            Remove-Item -LiteralPath $stale.FullName -Recurse -Force
            Write-Host "  - $($stale.Name)  [pruned]" -ForegroundColor Yellow
        }
    }
}

Write-Host ''
if ($Check) {
    if ($drift.Count -eq 0) {
        Write-Host "sync-skills: up to date" -ForegroundColor Green
        exit 0
    }
    Write-Host "sync-skills: $($drift.Count) file(s) out of date — run sync-skills.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "sync-skills: published $($sources.Count) director$(if ($sources.Count -eq 1) { 'y' } else { 'ies' }), $written file(s) written" -ForegroundColor Green
exit 0
