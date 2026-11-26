#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the knowledge files an agent reads first: the diagnostics JSON and the cheat sheet.

.DESCRIPTION
    Writes three things, none of them by hand:

    Docs/diagnostics/codes.json
        Every live DSHnnnn as data: code, severity, stage, title (the message), meaning and fix (the
        page's Cause and Fix paragraphs) and doc (the code's anchor on the documentation site). It is
        read from Docs/diagnostics/DSH?xxx.md, which .skill/gen-diagnostics.ps1 keeps in step with the
        source, so the pages stay the one truth and this is a projection of them. DreamMCP lists it as
        a reference document and the documentation site publishes the same file.

    Docs/cheatsheet.md
        One page of syntax and common forms, expanded from .skill/cheatsheet/cheatsheet.src.md. The
        source is ordinary Markdown with three directives, each on a line of its own:

          <!-- block: <path> #<heading anchor> [N] -->   the Nth fenced block (default 1) under that
                                                          heading of a page in the repository
          <!-- names: <path> #<heading anchor> -->        the first column of the first table under
                                                          that heading, as one line of `names`
          <!-- codes: DSHnnnn DSHnnnn ... -->              a table of those codes and their messages,
                                                          read from the diagnostics pages

    Tests/Corpus/Compile/CheatSheet/*.dss
        Every fenced block of the expanded page whose first line names a source file,
        `// DreamSource/Shader/<path>.dss` (or the legacy `// DShader/<path>.dss`), is also written here. DreamShader.Compiler2.Corpus.CheatSheet.* compiles
        each one, and a fixture with no golden asserts that the compile succeeds -- so an example on the
        cheat sheet that stops building fails the suite.

    Run it after changing the cheat sheet source, a page it quotes, or a diagnostic (after
    gen-diagnostics.ps1). -Check writes nothing and exits 1 when any of the three is out of date; CI runs
    it that way.

.EXAMPLE
    ./gen-knowledge.ps1

.EXAMPLE
    ./gen-knowledge.ps1 -Check
#>
[CmdletBinding()]
param(
    # Report drift and exit non-zero instead of writing.
    [switch]$Check
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- what differs per plugin

$plugin = 'DreamShader'
$prefix = 'DSH'
$siteRoot = 'https://shader.toolchain.64hz.cn/en/docs/'
# Where the documentation site publishes each diagnostics page (docs-sync/config.json of the site
# repository). The site's validator checks every `doc` link of the published JSON against its routes.
$pageRoute = { param($pageName) 'plugin/diagnostics/' + [System.IO.Path]::GetFileNameWithoutExtension($pageName).ToLowerInvariant() }
$fixtureHeader = '^//\s*((?:DreamSource/Shader|DShader)/\S+\.dss)\b'
$fixtureDir = 'Tests/Corpus/Compile/CheatSheet'
$fixtureExtension = '.dss'
# A fixture is the block as printed; DreamShader's corpus runner places it under a scratch package itself.
$toFixture = { param($text, $sourcePath) $text }

# ---------------------------------------------------------------- shared

$pluginRoot = Split-Path -Parent $PSScriptRoot
$docsRoot = Join-Path $pluginRoot 'Docs'
$diagnosticsRoot = Join-Path $docsRoot 'diagnostics'
$cheatSource = Join-Path $PSScriptRoot 'cheatsheet/cheatsheet.src.md'

function Convert-ToLf([string]$Text) { $Text -replace "`r`n", "`n" }

function Read-Text([string]$Path) { Convert-ToLf ([System.IO.File]::ReadAllText($Path)) }

$drift = [System.Collections.Generic.List[string]]::new()

function Write-Output-File {
    <#
        Compares with line endings normalized and writes in the ending the existing file has (LF for a new
        one), so a checkout with either ending is not drift forever.
    #>
    param([string]$Path, [string]$Text)
    $label = [System.IO.Path]::GetRelativePath($pluginRoot, $Path) -replace '\\', '/'
    $lf = Convert-ToLf $Text
    $existing = if (Test-Path -LiteralPath $Path) { [System.IO.File]::ReadAllText($Path) } else { $null }
    if ($null -ne $existing -and (Convert-ToLf $existing) -ceq $lf) { return }
    $drift.Add($label)
    if ($Check) { return }
    $eol = if ($null -ne $existing -and $existing.Contains("`r`n")) { "`r`n" } else { "`n" }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [System.IO.File]::WriteAllText($Path, ($lf -replace "`n", $eol), [System.Text.UTF8Encoding]::new($false))
    Write-Host "  wrote $label" -ForegroundColor Gray
}

function ConvertTo-Prose([string]$Text) {
    # Unwrap the hard-wrapped Markdown of a page into one line per paragraph. Fenced blocks and list
    # items keep their line breaks: joining them would change what they say.
    $out = [System.Collections.Generic.List[string]]::new()
    $paragraph = [System.Collections.Generic.List[string]]::new()
    $inFence = $false
    foreach ($line in ($Text.Trim() -split "`n")) {
        if ($line -match '^\s*```') {
            if ($paragraph.Count) { $out.Add(($paragraph -join ' ')); $paragraph.Clear() }
            $inFence = -not $inFence
            $out.Add($line)
            continue
        }
        if ($inFence) { $out.Add($line); continue }
        if (-not $line.Trim()) {
            if ($paragraph.Count) { $out.Add(($paragraph -join ' ')); $paragraph.Clear() }
            if ($out.Count -and $out[$out.Count - 1] -ne '') { $out.Add('') }
            continue
        }
        if ($line -match '^\s*(?:[-*+]|\d+\.|\|)\s') {
            if ($paragraph.Count) { $out.Add(($paragraph -join ' ')); $paragraph.Clear() }
            $out.Add($line.TrimEnd())
            continue
        }
        if ($paragraph.Count -eq 0 -and $out.Count -and $out[$out.Count - 1] -match '^\s*(?:[-*+]|\d+\.)\s' -and $line -match '^\s+') {
            # A list item's continuation line.
            $out[$out.Count - 1] = $out[$out.Count - 1] + ' ' + $line.Trim()
            continue
        }
        $paragraph.Add($line.Trim())
    }
    if ($paragraph.Count) { $out.Add(($paragraph -join ' ')) }
    return ($out -join "`n").Trim()
}

# ---------------------------------------------------------------- diagnostics

$codePattern = "$prefix\d{4}"
$pages = Get-ChildItem -LiteralPath $diagnosticsRoot -Filter "${prefix}?xxx.md" -File | Sort-Object Name
if (-not $pages) { throw "No ${prefix}?xxx.md pages under Docs/diagnostics. Run gen-diagnostics.ps1 first." }

$stages = [System.Collections.Generic.List[object]]::new()
$entries = @{}

foreach ($page in $pages) {
    $text = Read-Text $page.FullName
    if ($text -notmatch "(?m)^# ($prefix\dxxx) --- (.+)$") { throw "$($page.Name): no '# ${prefix}Nxxx --- <stage>' title." }
    $range = $Matches[1]
    $stage = $Matches[2].Trim()
    $route = & $pageRoute $page.Name
    $stages.Add([ordered]@{ range = $range; title = $stage; doc = "$siteRoot$route/" })

    # Only the live part of the page: the retired block keeps prose for codes nothing raises.
    $live = ($text -split "(?m)^## Retired codes\s*$")[0]
    $sections = [regex]::Matches($live, "(?ms)^## ($codePattern)\s*\n(.*?)(?=^## |\z)")
    foreach ($section in $sections) {
        $code = $section.Groups[1].Value
        $body = $section.Groups[2].Value
        if ($body -notmatch "(?s)<!-- generated:begin $code -->(.*?)<!-- generated:end $code -->(.*)") {
            throw "$($page.Name): $code has no generated block."
        }
        $generated = $Matches[1]
        $prose = $Matches[2]
        $severity = if ($generated -match '\*\*Severity\*\* (\w+)') { $Matches[1] } else { throw "$($page.Name): $code has no severity." }
        $message = if ($generated -match '(?s)\*\*Message\*\*\s*\n\s*```\s*\n(.*?)\n```') { $Matches[1].Trim() } else { '' }

        # The hand-written half is a run of bold-led paragraphs: **Cause.** ... **Fix.** ... and now and
        # then **Note.** or **Recovery.**, which belong with the fix an author applies.
        $parts = [ordered]@{}
        $current = $null
        foreach ($chunk in [regex]::Split($prose.Trim(), '(?m)^(?=\*\*[A-Z][A-Za-z ]*\.\*\*)')) {
            if ($chunk -match '^\*\*([A-Z][A-Za-z ]*)\.\*\*\s*(.*)$' -or $chunk -match '(?s)^\*\*([A-Z][A-Za-z ]*)\.\*\*\s*(.*)$') {
                $current = $Matches[1]
                $parts[$current] = $Matches[2]
            }
            elseif ($current -and $chunk.Trim()) { $parts[$current] += "`n" + $chunk }
        }
        $meaning = if ($parts.Contains('Cause')) { ConvertTo-Prose $parts['Cause'] } else { $null }
        $fixParts = foreach ($key in $parts.Keys) { if ($key -ne 'Cause') { if ($key -eq 'Fix') { ConvertTo-Prose $parts[$key] } else { "**$key.** " + (ConvertTo-Prose $parts[$key]) } } }
        $fix = if ($fixParts) { ($fixParts -join "`n`n") } else { $null }
        if ($meaning -match 'Not written yet') { $meaning = $null }
        if ($fix -match '^_?Not written yet') { $fix = $null }
        # A message assembled at run time has no template to show; the first sentence of the cause
        # names the problem instead, so a title is never a placeholder.
        if ($message -eq '(built at runtime)' -and $meaning) {
            $message = ($meaning -split '(?<=\.)\s', 2)[0]
        }

        $entries[$code] = [ordered]@{
            code     = $code
            severity = $severity
            stage    = $stage
            title    = $message
            meaning  = $meaning
            fix      = $fix
            doc      = "$siteRoot$route/#$($code.ToLowerInvariant())"
        }
    }
}

$document = [ordered]@{
    schema      = 1
    plugin      = $plugin
    prefix      = $prefix
    description = "Every $prefix diagnostic $plugin can report. title is the message as printed, with %s / {0} where the source fills in a value; meaning and fix are the Cause and Fix of the code's documentation page; doc is that page on the documentation site."
    generatedBy = '.skill/gen-knowledge.ps1 from Docs/diagnostics/' + $prefix + '?xxx.md'
    stages      = $stages
    codes       = @($entries.Keys | Sort-Object | ForEach-Object { $entries[$_] })
}
$json = ($document | ConvertTo-Json -Depth 6) + "`n"
Write-Output-File -Path (Join-Path $diagnosticsRoot 'codes.json') -Text $json
Write-Host "Found $($entries.Count) diagnostic code(s) on $($pages.Count) page(s)." -ForegroundColor DarkGray

# ---------------------------------------------------------------- cheat sheet

function Get-Anchor([string]$Heading) {
    # GitHub's and the site's slug: lower case, punctuation dropped, spaces to hyphens.
    $text = $Heading.Trim().ToLowerInvariant() -replace '`', ''
    $text = [regex]::Replace($text, '[^\p{L}\p{Nd}\s_-]', '')
    return ($text -replace '\s', '-')
}

function Get-Section([string]$RelativePath, [string]$Anchor) {
    $path = Join-Path $pluginRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "cheat sheet: '$RelativePath' does not exist." }
    $lines = (Read-Text $path) -split "`n"
    $start = -1; $level = 0; $inFence = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*```') { $inFence = -not $inFence; continue }
        if ($inFence) { continue }
        if ($lines[$i] -match '^(#{1,6})\s+(.*?)\s*$') {
            if ($start -ge 0 -and $Matches[1].Length -le $level) { return ($lines[$start..($i - 1)] -join "`n") }
            if ($start -lt 0 -and (Get-Anchor $Matches[2]) -eq $Anchor) { $start = $i + 1; $level = $Matches[1].Length }
        }
    }
    if ($start -lt 0) { throw "cheat sheet: '$RelativePath' has no heading #$Anchor." }
    return ($lines[$start..($lines.Count - 1)] -join "`n")
}

function Get-Fences([string]$Text) {
    [regex]::Matches($Text, '(?ms)^```[^\n]*\n.*?^```[ \t]*$') | ForEach-Object { $_.Value }
}

$messages = @{}
foreach ($entry in $entries.Values) { $messages[$entry.code] = $entry }

$expanded = foreach ($line in ((Read-Text $cheatSource) -split "`n")) {
    if ($line -match '^<!--\s*block:\s*(\S+)\s+#(\S+)(?:\s+(\d+))?\s*-->$') {
        $n = if ($Matches[3]) { [int]$Matches[3] } else { 1 }
        $fences = @(Get-Fences (Get-Section $Matches[1] $Matches[2]))
        if ($fences.Count -lt $n) { throw "cheat sheet: $($Matches[1]) #$($Matches[2]) has $($fences.Count) block(s), not $n." }
        $fences[$n - 1]
    }
    elseif ($line -match '^<!--\s*names:\s*(\S+)\s+#(\S+)\s*-->$') {
        $section = Get-Section $Matches[1] $Matches[2]
        $names = foreach ($row in ($section -split "`n")) {
            if ($row -match '^\|\s*`([^`]+)`') { $Matches[1] }
        }
        if (-not $names) { throw "cheat sheet: $($Matches[1]) #$($Matches[2]) has no table of names." }
        (@($names | Select-Object -Unique) | ForEach-Object { "``$_``" }) -join ' '
    }
    elseif ($line -match '^<!--\s*codes:\s*(.+?)\s*-->$') {
        '| Code | Severity | Message |'
        '| :-- | :-- | :-- |'
        foreach ($code in ($Matches[1] -split '\s+')) {
            if (-not $messages.ContainsKey($code)) { throw "cheat sheet: $code is not a live diagnostic." }
            $m = $messages[$code]
            "| [$code]($($m.doc)) | $($m.severity) | $(($m.title -replace '\|', '\|')) |"
        }
    }
    elseif ($line -match '^<!--\s*(block|names|codes):') { throw "cheat sheet: malformed directive: $line" }
    else { $line }
}

$banner = '<!-- Generated by .skill/gen-knowledge.ps1 from .skill/cheatsheet/cheatsheet.src.md. Edit that file, not this one. -->'
# The banner goes last: the page has to open with its title, which the documentation site lifts out.
$cheatText = (($expanded -join "`n").TrimEnd()) + "`n`n" + $banner + "`n"
Write-Output-File -Path (Join-Path $docsRoot 'cheatsheet.md') -Text $cheatText

# ---------------------------------------------------------------- fixtures

$fixtures = @{}
foreach ($fence in (Get-Fences $cheatText)) {
    $body = ($fence -split "`n" | Select-Object -Skip 1 | Select-Object -SkipLast 1) -join "`n"
    if ($body -match $fixtureHeader) {
        $sourcePath = $Matches[1]
        $name = [System.IO.Path]::GetFileName($sourcePath)
        if ($fixtures.ContainsKey($name)) { throw "cheat sheet: two blocks are both '$name'." }
        $fixtures[$name] = (& $toFixture $body $sourcePath).TrimEnd() + "`n"
    }
}
$fixtureRoot = Join-Path $pluginRoot $fixtureDir
foreach ($name in $fixtures.Keys) {
    Write-Output-File -Path (Join-Path $fixtureRoot $name) -Text $fixtures[$name]
}
if (Test-Path -LiteralPath $fixtureRoot) {
    foreach ($stale in Get-ChildItem -LiteralPath $fixtureRoot -Filter "*$fixtureExtension" -File) {
        if (-not $fixtures.ContainsKey($stale.Name)) {
            $drift.Add("$fixtureDir/$($stale.Name) (no longer on the cheat sheet)")
            if (-not $Check) { Remove-Item -LiteralPath $stale.FullName; Write-Host "  removed $fixtureDir/$($stale.Name)" -ForegroundColor Gray }
        }
    }
}
Write-Host "Cheat sheet: $($fixtures.Count) compiled example(s)." -ForegroundColor DarkGray

# ---------------------------------------------------------------- verdict

if ($Check) {
    if ($drift.Count) {
        Write-Host ''
        Write-Host "knowledge files are out of date: $($drift -join ', ')" -ForegroundColor Red
        Write-Host 'Run .skill/gen-knowledge.ps1 and commit the result.' -ForegroundColor Red
        exit 1
    }
    Write-Host 'knowledge files are up to date.' -ForegroundColor Green
}
exit 0
