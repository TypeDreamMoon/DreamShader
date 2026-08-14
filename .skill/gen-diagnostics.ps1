<#
.SYNOPSIS
    Regenerates the machine-written half of Docs/diagnostics/.

.DESCRIPTION
    Scans the plugin sources for `FailWith(OutError, TEXT("DSHnnnn"), ...)` and writes one page per
    thousand-range, with a section per code holding its severity, message template and the place
    that raises it.

    Only the text between the `generated:begin`/`generated:end` markers is rewritten. Everything a
    human wrote under a code -- the Cause and Fix paragraphs that make the page worth reading -- is
    carried across untouched. That is the whole point of splitting it this way: the half that rots
    (which codes exist, what they say) is regenerated, and the half that does not (why it happens)
    is written once.

    DreamShader raises through two message dialects and both are matched here: the parser's
    LOCTEXT/FText::Format sites, whose text the localization gather also sees, and the generator's
    FString::Printf sites. The code is the same shape in both, which is the point of the migration.

    Re-run it after adding a diagnostic. `-Check` makes it a gate instead: exit 1 if the pages are
    out of date, which is how CI notices a new code that nobody documented.

.EXAMPLE
    ./gen-diagnostics.ps1

.EXAMPLE
    ./gen-diagnostics.ps1 -Check
#>
[CmdletBinding()]
param(
    # Report drift and exit non-zero instead of writing.
    [switch]$Check
)

$ErrorActionPreference = 'Stop'

$pluginRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $pluginRoot 'Source'
$docsRoot = Join-Path $pluginRoot 'Docs/diagnostics'

# ---------------------------------------------------------------- scan

# String keys, not integers: an OrderedDictionary indexed with an int looks up by *position*, so
# `$ranges[3]` would return the fourth entry and every page would be titled with its neighbour's
# subject. (Inherited straight from DreamFX's script, which learned it the hard way.)
$ranges = [ordered]@{
    '1' = 'Driver, source files and imports'
    '2' = 'Lexer and syntax'
    '3' = 'Sections and declarations'
    '4' = 'Graph statements and expressions'
    '5' = 'Builtins -- UE.*, math, Substrate'
    '6' = 'Functions and HLSL codegen'
    '7' = 'Properties, parameters and settings'
    '8' = 'Asset generation and saving'
    '9' = 'Tools, sync and internal invariants'
}

$found = @{}

foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Include '*.cpp', '*.h' -File -Recurse) {
    if ($file.FullName -match '\\Tests\\') { continue }
    $text = [System.IO.File]::ReadAllText($file.FullName)

    # After the code comes the message, in whichever dialect the site uses:
    #   LOCTEXT("Key", "text")  -- parser, gathered for localization
    #   TEXT("text")            -- generator's Printf format string
    # Both are the right thing to show, so take whichever appears first.
    $pattern = 'FailWith\(\s*\w+\s*,\s*TEXT\("(DSH\d{4})"\)(.{0,500}?)(?:LOCTEXT\(\s*"[A-Za-z0-9_]+"\s*,\s*"((?:[^"\\]|\\.)*)"\)|TEXT\("((?:[^"\\]|\\.)*)"\)|;)'
    foreach ($match in [regex]::Matches($text, $pattern, 'Singleline')) {
        $code = $match.Groups[1].Value
        $message = if ($match.Groups[3].Success) { $match.Groups[3].Value } else { $match.Groups[4].Value }

        if (-not $message) { $message = '(built at runtime)' }

        $line = ($text.Substring(0, $match.Index) -split "`n").Count
        $relative = [System.IO.Path]::GetRelativePath($pluginRoot, $file.FullName) -replace '\\', '/'

        if ($found.ContainsKey($code)) {
            # Same code raised from more than one place: keep them all, they are usually the same
            # condition reached by different routes and a reader chasing one wants to see the others.
            $found[$code].Sites += "$relative`:$line"
        }
        else {
            $found[$code] = [pscustomobject]@{
                Code     = $code
                Severity = 'error'
                Message  = $message
                Sites    = @("$relative`:$line")
            }
        }
    }
}

if ($found.Count -eq 0) {
    throw "No diagnostics found under '$sourceRoot'. Is the raise helper still FailWith(...)?"
}

Write-Host "Found $($found.Count) diagnostic code(s)." -ForegroundColor DarkGray

# ---------------------------------------------------------------- merge and emit

function Convert-ToCrlf {
    param([string]$Text)
    return ($Text -replace "`r`n", "`n") -replace "`n", "`r`n"
}

function Get-HandWrittenSections {
    param([string]$Path)

    # Maps code -> everything the human wrote under that code's heading, i.e. what follows the
    # generated:end marker up to the next heading.
    $sections = @{}
    if (-not (Test-Path -LiteralPath $Path)) { return $sections }

    $content = [System.IO.File]::ReadAllText($Path)
    $pattern = '<!-- generated:end (DSH\d{4}) -->\r?\n(.*?)(?=\r?\n## DSH|\Z)'
    foreach ($match in [regex]::Matches($content, $pattern, 'Singleline')) {
        # Trim both ends, not just the tail: the writer puts a blank line after the marker, so
        # keeping the leading newline would make each run add one more and the file would never
        # converge -- which turns -Check into a permanent false alarm.
        $sections[$match.Groups[1].Value] = $match.Groups[2].Value.Trim()
    }
    return $sections
}

$placeholder = @'
**Cause.** _Not written yet._

**Fix.** _Not written yet._
'@

if (-not $Check) {
    New-Item -ItemType Directory -Force -Path $docsRoot | Out-Null
}

$drift = @()
$indexRows = @()

foreach ($rangeKey in $ranges.Keys) {
    $codes = $found.Values |
        Where-Object { $_.Code.Substring(3, 1) -eq "$rangeKey" } |
        Sort-Object Code

    if (-not $codes) { continue }

    $pagePath = Join-Path $docsRoot "DSH$($rangeKey)xxx.md"
    $handWritten = Get-HandWrittenSections -Path $pagePath

    $builder = [System.Text.StringBuilder]::new()
    [void]$builder.AppendLine("# DSH$($rangeKey)xxx --- $($ranges[$rangeKey])")
    [void]$builder.AppendLine()
    [void]$builder.AppendLine('> The block between the generated markers is written by `.skill/gen-diagnostics.ps1`.')
    [void]$builder.AppendLine('> Everything below a marker is written by hand and survives a regeneration.')
    [void]$builder.AppendLine()

    foreach ($entry in $codes) {
        $prose = if ($handWritten.ContainsKey($entry.Code)) { $handWritten[$entry.Code] } else { $placeholder }
        if (-not $prose.Trim()) { $prose = $placeholder }

        [void]$builder.AppendLine("## $($entry.Code)")
        [void]$builder.AppendLine()
        [void]$builder.AppendLine("<!-- generated:begin $($entry.Code) -->")
        [void]$builder.AppendLine("**Severity** $($entry.Severity)")
        [void]$builder.AppendLine()
        [void]$builder.AppendLine('**Message**')
        [void]$builder.AppendLine()
        [void]$builder.AppendLine('```')
        [void]$builder.AppendLine($entry.Message)
        [void]$builder.AppendLine('```')
        [void]$builder.AppendLine()
        $tick = [char]0x60
        $sites = ($entry.Sites | Sort-Object -Unique | ForEach-Object { "$tick$_$tick" }) -join ', '
        [void]$builder.AppendLine("**Raised by** $sites")
        [void]$builder.AppendLine("<!-- generated:end $($entry.Code) -->")
        [void]$builder.AppendLine()
        [void]$builder.AppendLine($prose)
        [void]$builder.AppendLine()

        $undocumented = $prose -match 'Not written yet'
        $indexRows += [pscustomobject]@{
            Code         = $entry.Code
            Severity     = $entry.Severity
            Page         = "DSH$($rangeKey)xxx.md"
            Message      = $entry.Message
            Undocumented = $undocumented
        }
    }

    # AppendLine emits CRLF but the placeholder here-string and any carried-over prose may hold bare
    # LFs. Left mixed, the file's endings depend on which codes happen to be documented, and -Check
    # reports drift forever. Normalize once, at the boundary.
    $rendered = Convert-ToCrlf $builder.ToString()
    $existing = if (Test-Path -LiteralPath $pagePath) { [System.IO.File]::ReadAllText($pagePath) } else { '' }

    if ($rendered -ne $existing) {
        if ($Check) {
            $drift += "DSH$($rangeKey)xxx.md"
        }
        else {
            [System.IO.File]::WriteAllText($pagePath, $rendered)
            Write-Host "  wrote DSH$($rangeKey)xxx.md ($($codes.Count) code(s))" -ForegroundColor Gray
        }
    }
}

# ---------------------------------------------------------------- index

$index = [System.Text.StringBuilder]::new()
[void]$index.AppendLine('# DreamShader diagnostics')
[void]$index.AppendLine()
[void]$index.AppendLine('Every `DSHnnnn` DreamShader can emit. The leading digit is the stage that raises it:')
[void]$index.AppendLine()
foreach ($rangeKey in $ranges.Keys) {
    # A range with no tagged site yet has no page, so do not link to one -- the migration lands
    # stage by stage and a dead link reads as a lost file rather than as work still to do.
    $hasPage = @($indexRows | Where-Object { $_.Code.Substring(3, 1) -eq "$rangeKey" }).Count -gt 0
    if ($hasPage) {
        [void]$index.AppendLine("- **DSH$($rangeKey)xxx** --- $($ranges[$rangeKey]) --- [DSH$($rangeKey)xxx.md](DSH$($rangeKey)xxx.md)")
    }
    else {
        [void]$index.AppendLine("- **DSH$($rangeKey)xxx** --- $($ranges[$rangeKey]) --- _no tagged site yet; see [index.md](index.md)_")
    }
}
[void]$index.AppendLine()
[void]$index.AppendLine('Generated by `.skill/gen-diagnostics.ps1`; run it after adding a code.')
[void]$index.AppendLine()
[void]$index.AppendLine('Messages not yet reachable by a code are still catalogued verbatim in [index.md](index.md),')
[void]$index.AppendLine('which stays authoritative until every raise site is tagged.')
[void]$index.AppendLine()
[void]$index.AppendLine('| Code | Severity | Message |')
[void]$index.AppendLine('| --- | --- | --- |')
foreach ($row in $indexRows | Sort-Object Code) {
    $short = $row.Message
    if ($short.Length -gt 110) { $short = $short.Substring(0, 107) + '...' }
    $short = $short -replace '\|', '\|'
    [void]$index.AppendLine("| [$($row.Code)]($($row.Page)#$($row.Code.ToLowerInvariant())) | $($row.Severity) | $short |")
}
[void]$index.AppendLine()

$indexPath = Join-Path $docsRoot 'README.md'
$renderedIndex = Convert-ToCrlf $index.ToString()
$existingIndex = if (Test-Path -LiteralPath $indexPath) { [System.IO.File]::ReadAllText($indexPath) } else { '' }

if ($renderedIndex -ne $existingIndex) {
    if ($Check) { $drift += 'README.md' }
    else {
        [System.IO.File]::WriteAllText($indexPath, $renderedIndex)
        Write-Host '  wrote README.md' -ForegroundColor Gray
    }
}

# ---------------------------------------------------------------- verdict

$missing = @($indexRows | Where-Object { $_.Undocumented })
if ($missing.Count -gt 0) {
    Write-Host "$($missing.Count) code(s) still have no Cause/Fix prose:" -ForegroundColor Yellow
    Write-Host "  $(($missing.Code | Sort-Object) -join ', ')" -ForegroundColor DarkYellow
}

if ($Check) {
    if ($drift.Count -gt 0) {
        Write-Host ''
        Write-Host "diagnostics docs are out of date: $($drift -join ', ')" -ForegroundColor Red
        Write-Host 'Run .skill/gen-diagnostics.ps1 and commit the result.' -ForegroundColor Red
        exit 1
    }
    Write-Host 'diagnostics docs are up to date.' -ForegroundColor Green
}

exit 0
