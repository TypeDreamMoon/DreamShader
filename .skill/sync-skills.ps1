#Requires -Version 7.0
<#
.SYNOPSIS
    Publish .skill/ into a .claude/skills/ directory so Claude Code auto-loads it.

.DESCRIPTION
    Claude Code discovers skills from `.claude/skills/`, searching the directory tree at and
    *above* where the agent is working. `.skill/` is not on that path, so the skills have to be
    published into one that is.

    A symlink or junction is not enough: each SKILL.md links to `../../Docs/…`, which resolves
    correctly from `.skill/<skill>/` and not from `.claude/skills/<skill>/`. This script copies the
    tree and rewrites three path families to whatever is correct at the destination:

        ](../../Docs/…         -> the real relative path to <plugin>/Docs
        ](../reference/…       -> ](../dream-shader-reference/…, where reference/ is published
        Plugins/DreamShader/   -> the plugin's real path from the project root (the driver
                                  invocation `pwsh -File Plugins/DreamShader/.skill/dsc.ps1`
                                  among them), so a plugin under Plugins/<Group>/ works too

    `reference/` is published as `dream-shader-reference/`, because every plugin that ships skills
    this way has a `reference/` of its own, and one `-Prune` must never delete another's.

    `.skill/` stays the source of truth. Re-run after editing it; `-Check` reports drift and exits
    1, which makes it usable as a pre-commit gate.

.EXAMPLE
    ./sync-skills.ps1
    Publish to the host project's .claude/skills (the nearest .uproject above the plugin).

.EXAMPLE
    ./sync-skills.ps1 -Check
    Report whether the published copy is stale. Exit 1 if it is.

.EXAMPLE
    ./sync-skills.ps1 -Target I:/Other/Project/.claude/skills -Prune
#>
[CmdletBinding()]
param(
    # The .claude/skills directory to publish into, absolute or relative to the working directory.
    # Defaults to the host project's, falling back to the plugin's own when the plugin repo is
    # checked out standalone.
    [string]$Target,

    # Compare instead of writing. Exit 1 when the published copy differs from what would be written,
    # including a published file whose source is gone.
    [switch]$Check,

    # Remove what this run did not write: dream-shader-* directories whose skill is gone, files in a
    # published directory whose source is gone, and the reference/ files an older version of this
    # script published.
    [switch]$Prune
)

$ErrorActionPreference = 'Stop'

$skillRoot = $PSScriptRoot
$pluginRoot = Split-Path -Parent $skillRoot
$docsDir = Join-Path $pluginRoot 'Docs'

function ConvertTo-RelativePosix {
    # GetRelativePath is lexical, so both arguments must already be resolved.
    param([string]$From, [string]$To)
    return ([System.IO.Path]::GetRelativePath($From, $To)) -replace '\\', '/'
}

# ---------------------------------------------------------------- destination

if ($Target) {
    $Target = [System.IO.Path]::GetFullPath($Target, (Get-Location).Path)
}
else {
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
$pluginRel = ConvertTo-RelativePosix -From $workingFull -To $pluginRoot

$referenceName = 'dream-shader-reference'
$marker = "<!-- Published from $pluginRel/.skill by sync-skills.ps1. Edit the source, not this copy. -->"
$markerPattern = 'Published from \S*DreamShader/\.skill by sync-skills\.ps1'

function Get-PublishedText {
    param([string]$Path)

    $text = Get-Content -LiteralPath $Path -Raw
    if ($null -eq $text) { $text = '' }
    $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }

    # `](../../Docs/x.md)` is written for .skill/<skill>/; retarget it at the real Docs tree.
    $text = $text -replace '\]\(\.\./\.\./Docs/', "]($docsPrefix/"

    # reference/ is published under a name of its own.
    $text = $text -replace '\]\(\.\./reference/', "](../$referenceName/"

    # Paths are written for a plugin at Plugins/DreamShader/; the driver invocation is one of them.
    $text = $text.Replace('Plugins/DreamShader/', "$pluginRel/")

    # Mark the copy, after the frontmatter so the `---` block stays first.
    if ($text -match '(?s)^(---\r?\n.*?\r?\n---\r?\n)') {
        $frontmatter = $Matches[1]
        $text = $frontmatter + $marker + $newline + $text.Substring($frontmatter.Length)
    }
    else {
        $text = $marker + $newline + $text
    }

    return $text
}

# ---------------------------------------------------------------- publish

$sources = @(Get-ChildItem -LiteralPath $skillRoot -Directory | Where-Object { $_.Name -like 'dream-shader-*' -or $_.Name -eq 'reference' })
if ($sources.Count -eq 0) { throw "No dream-shader-* directories under '$skillRoot'." }

Write-Host "source:  $skillRoot" -ForegroundColor DarkGray
Write-Host "target:  $targetFull" -ForegroundColor DarkGray
Write-Host "Docs ->  $docsPrefix" -ForegroundColor DarkGray
Write-Host "plugin ->$pluginRel" -ForegroundColor DarkGray
Write-Host ''

$drift = @()
$written = 0
$publishedNames = @()

foreach ($source in $sources) {
    $publishedName = if ($source.Name -eq 'reference') { $referenceName } else { $source.Name }
    $publishedNames += $publishedName
    $destDir = Join-Path $targetFull $publishedName
    $wantedFiles = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

    foreach ($file in Get-ChildItem -LiteralPath $source.FullName -File -Recurse) {
        $relative = $file.FullName.Substring($source.FullName.Length).TrimStart('\', '/')
        $dest = Join-Path $destDir $relative
        [void]$wantedFiles.Add($dest)

        if ($file.Extension -eq '.md') {
            $wanted = Get-PublishedText -Path $file.FullName
            $current = if (Test-Path -LiteralPath $dest) { Get-Content -LiteralPath $dest -Raw } else { $null }
            $same = $current -ceq $wanted
        }
        else {
            # Anything else is copied byte for byte -- an image must not go through a text read.
            $same = (Test-Path -LiteralPath $dest) -and
                    ((Get-FileHash -LiteralPath $dest).Hash -eq (Get-FileHash -LiteralPath $file.FullName).Hash)
            $current = if (Test-Path -LiteralPath $dest) { 'present' } else { $null }
        }

        if ($same) {
            Write-Host "  = $publishedName/$relative" -ForegroundColor DarkGray
            continue
        }

        $drift += "$publishedName/$relative"

        if ($Check) {
            $state = if ($null -eq $current) { 'missing' } else { 'stale' }
            Write-Host "  ! $publishedName/$relative  [$state]" -ForegroundColor Yellow
            continue
        }

        New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
        if ($file.Extension -eq '.md') {
            Set-Content -LiteralPath $dest -Value $wanted -NoNewline -Encoding utf8NoBOM
        }
        else {
            Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
        }
        Write-Host "  + $publishedName/$relative" -ForegroundColor Green
        $written++
    }

    # A published file whose source is gone: drift for -Check, removed by -Prune.
    if (Test-Path -LiteralPath $destDir) {
        foreach ($orphan in Get-ChildItem -LiteralPath $destDir -File -Recurse) {
            if ($wantedFiles.Contains($orphan.FullName)) { continue }
            $relative = "$publishedName/" + ($orphan.FullName.Substring($destDir.Length).TrimStart('\', '/') -replace '\\', '/')
            $drift += $relative
            if ($Prune -and -not $Check) {
                Remove-Item -LiteralPath $orphan.FullName -Force
                Write-Host "  - $relative  [pruned]" -ForegroundColor Yellow
            }
            else {
                Write-Host "  ! $relative  [no source; -Prune removes it]" -ForegroundColor Yellow
            }
        }
    }
}

# Directories of skills that no longer exist.
foreach ($stale in Get-ChildItem -LiteralPath $targetFull -Directory) {
    if ($stale.Name -like 'dream-shader-*' -and $stale.Name -notin $publishedNames) {
        $drift += "$($stale.Name)/"
        if ($Prune -and -not $Check) {
            Remove-Item -LiteralPath $stale.FullName -Recurse -Force
            Write-Host "  - $($stale.Name)/  [pruned]" -ForegroundColor Yellow
        }
        else {
            Write-Host "  ! $($stale.Name)/  [no source; -Prune removes it]" -ForegroundColor Yellow
        }
    }
}

# An older version of this script published reference/ under that name. Only files that carry this
# plugin's marker are ours to remove; another plugin's reference/ files stay.
$legacyReference = Join-Path $targetFull 'reference'
if (Test-Path -LiteralPath $legacyReference) {
    foreach ($old in Get-ChildItem -LiteralPath $legacyReference -File -Filter '*.md') {
        if ((Get-Content -LiteralPath $old.FullName -Raw) -notmatch $markerPattern) { continue }
        $drift += "reference/$($old.Name)"
        if ($Prune -and -not $Check) {
            Remove-Item -LiteralPath $old.FullName -Force
            Write-Host "  - reference/$($old.Name)  [pruned; now under $referenceName/]" -ForegroundColor Yellow
        }
        else {
            Write-Host "  ! reference/$($old.Name)  [published by an older sync-skills; -Prune removes it]" -ForegroundColor Yellow
        }
    }
    if (-not $Check -and @(Get-ChildItem -LiteralPath $legacyReference -Force).Count -eq 0) {
        Remove-Item -LiteralPath $legacyReference -Force
    }
}

Write-Host ''
if ($Check) {
    if ($drift.Count -eq 0) {
        Write-Host "sync-skills: up to date" -ForegroundColor Green
        exit 0
    }
    Write-Host "sync-skills: $($drift.Count) file(s) out of date — run sync-skills.ps1 (-Prune for the ones without a source)" -ForegroundColor Yellow
    exit 1
}

Write-Host "sync-skills: published $($sources.Count) director$(if ($sources.Count -eq 1) { 'y' } else { 'ies' }), $written file(s) written" -ForegroundColor Green
exit 0
