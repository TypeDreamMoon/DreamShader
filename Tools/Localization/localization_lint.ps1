<#
.SYNOPSIS
    DreamShader localization lint + baseline inventory.

.DESCRIPTION
    Scans the plugin for compile-time localization text and for runtime text that is
    invisible to Unreal's Localization Dashboard gather step.

    Step-1 mode covers the localizable editor UI surface plus DreamShaderSettings.h.
    Deferred diagnostics live under MaterialAssetGeneration/ and Decompiler/ and are
    only linted when -IncludeDeferred is supplied.

    The script also reports a deterministic inventory of LOCTEXT/NSLOCTEXT entries so
    the dashboard gather count can be compared against a checked-in baseline.

.NOTES
    PowerShell 7+ only. No external modules required.
#>

[CmdletBinding()]
param(
    [switch]$IncludeDeferred,
    [switch]$Json,
    [switch]$EmitBaseline
)

$ErrorActionPreference = 'Stop'

function Get-PluginRoot {
    $loc = Split-Path -Parent $PSScriptRoot
    $root = Split-Path -Parent $loc
    return (Resolve-Path -LiteralPath $root).Path
}

function Get-NormalizedPath {
    param([string]$Path)
    return $Path.Replace('\', '/')
}

function Get-Category {
    param([string]$RelativePath)

    $p = Get-NormalizedPath $RelativePath

    if ($p.Contains('Source/DreamShaderEditor/Private/Bridge/') -or $p.Contains('Source/DreamShaderEditor/Private/Tests/')) {
        return 'Excluded'
    }

    if ($p.Contains('Source/DreamShaderEditor/Private/MaterialAssetGeneration/') -or $p.Contains('Source/DreamShaderEditor/Private/Decompiler/')) {
        return 'Deferred'
    }

    if (($p.StartsWith('Source/DreamShaderEditor/Private/UI/', [StringComparison]::OrdinalIgnoreCase) -and $p.EndsWith('.cpp', [StringComparison]::OrdinalIgnoreCase)) -or
        $p -eq 'Source/DreamShader/Public/DreamShaderSettings.h') {
        return 'Scope'
    }

    if ($p.StartsWith('Source/DreamShaderEditor/Private/Workspace/', [StringComparison]::OrdinalIgnoreCase) -or
        $p.StartsWith('Source/DreamShaderEditor/Private/VirtualFunction/', [StringComparison]::OrdinalIgnoreCase) -or
        $p.StartsWith('Source/DreamShaderEditor/Private/Commandlet/', [StringComparison]::OrdinalIgnoreCase) -or
        $p.StartsWith('Source/DreamShaderEditor/Private/DependencyGraph/', [StringComparison]::OrdinalIgnoreCase) -or
        $p.StartsWith('Source/DreamShaderEditor/Private/Preview/', [StringComparison]::OrdinalIgnoreCase) -or
        $p.StartsWith('Source/DreamShader/Private/Parser/', [StringComparison]::OrdinalIgnoreCase)) {
        return 'Allowlisted'
    }

    return 'Allowlisted'
}

function Get-RelativeSourcePath {
    param(
        [string]$PluginRoot,
        [string]$FullPath
    )

    $rel = [System.IO.Path]::GetRelativePath($PluginRoot, $FullPath)
    return ($rel -replace '\\', '/')
}

function Get-LineNumber {
    param(
        [string]$Text,
        [int]$Index
    )

    if ($Index -lt 0) { return 1 }
    $prefix = $Text.Substring(0, [Math]::Min($Index, $Text.Length))
    return 1 + ([regex]::Matches($prefix, "`n").Count)
}

function Get-LineSpanText {
    param(
        [string]$Text,
        [int]$Index
    )

    $start = $Text.LastIndexOf("`n", [Math]::Min($Index, $Text.Length - 1))
    if ($start -lt 0) { $start = 0 } else { $start += 1 }

    $end = $Text.IndexOf("`n", [Math]::Min($Index, [Math]::Max(0, $Text.Length - 1)))
    if ($end -lt 0) { $end = $Text.Length }

    return $Text.Substring($start, $end - $start)
}

function Convert-CStringLiteral {
    param([string]$Text)

    if ($null -eq $Text) { return $Text }

    $sb = [System.Text.StringBuilder]::new()
    for ($i = 0; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -ne '\\' -or $i -eq $Text.Length - 1) {
            [void]$sb.Append($ch)
            continue
        }

        $i++
        switch ($Text[$i]) {
            '\\' { [void]$sb.Append('\\') }
            '"' { [void]$sb.Append('"') }
            'n' { [void]$sb.Append("`n") }
            'r' { [void]$sb.Append("`r") }
            't' { [void]$sb.Append("`t") }
            default {
                [void]$sb.Append('\\')
                [void]$sb.Append($Text[$i])
            }
        }
    }

    return $sb.ToString()
}

function New-Violation {
    param(
        [string]$Rule,
        [string]$Severity,
        [string]$File,
        [int]$Line,
        [string]$Message
    )

    [pscustomobject]@{
        rule = $Rule
        severity = $Severity
        file = $File
        line = $Line
        message = $Message
    }
}

function Get-TextMatches {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Kind
    )

    $options = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $regex = [System.Text.RegularExpressions.Regex]::new($Pattern, $options)
    $matches = $regex.Matches($Text)
    foreach ($m in $matches) {
        [pscustomobject]@{
            kind = $Kind
            index = $m.Index
            length = $m.Length
            match = $m
        }
    }
}

function Get-FileAnalysis {
    param(
        [string]$PluginRoot,
        [System.IO.FileInfo]$File,
        [switch]$IncludeDeferred
    )

    $relative = Get-RelativeSourcePath -PluginRoot $PluginRoot -FullPath $File.FullName
    $category = Get-Category -RelativePath $relative
    $text = [System.IO.File]::ReadAllText($File.FullName)
    $ext = $File.Extension.ToLowerInvariant()
    $isHeader = $ext -eq '.h'
    $isCpp = $ext -eq '.cpp'

    $analysis = [ordered]@{
        relative = $relative
        fullPath = $File.FullName
        category = $category
        enabledForLint = $false
        namespaceDefine = $null
        hasNamespaceUndef = $false
        loctextCount = 0
        inventoryEntries = @()
        deferredLiteralCalls = 0
        r1R2Violations = @()
        r4R5R6Violations = @()
        warnings = @()
    }

    $lintable = $false
    if ($category -eq 'Scope') { $lintable = $true }
    elseif ($category -eq 'Deferred' -and $IncludeDeferred) { $lintable = $true }
    $analysis.enabledForLint = $lintable

    $defineLineNumber = $null
    $lineIndex = 0
    foreach ($line in ($text -split "`r?`n")) {
        $lineIndex++
        $trimmed = $line.TrimStart([char]0xFEFF)
        if ($trimmed -match '^[ \t]*#define[ \t]+LOCTEXT_NAMESPACE[ \t]+"(?<ns>(?:\\.|[^"\\])*)"[ \t]*(?://.*)?$') {
            $analysis.namespaceDefine = Convert-CStringLiteral $Matches['ns']
            $defineLineNumber = $lineIndex
            if ($isHeader) {
                $analysis.r4R5R6Violations += New-Violation -Rule 'R4' -Severity 'error' -File $File.FullName -Line $defineLineNumber -Message 'LOCTEXT_NAMESPACE must not be defined in headers; use NSLOCTEXT instead.'
            }
            break
        }
    }

    if ($category -ne 'Excluded' -and $isCpp -and $text -match 'LOCTEXT\(' -and -not $analysis.namespaceDefine) {
        $line = Get-LineNumber -Text $text -Index ([regex]::Match($text, 'LOCTEXT\(').Index)
        $analysis.r4R5R6Violations += New-Violation -Rule 'R5' -Severity 'error' -File $File.FullName -Line $line -Message 'LOCTEXT is used in a .cpp file without a #define LOCTEXT_NAMESPACE.'
    }

    if ($defineLineNumber) {
        $undefFound = $false
        $lineIndex = 0
        foreach ($line in ($text -split "`r?`n")) {
            $lineIndex++
            if ($lineIndex -le $defineLineNumber) { continue }
            if ($line.TrimStart([char]0xFEFF) -match '^[ \t]*#undef[ \t]+LOCTEXT_NAMESPACE[ \t]*(?://.*)?$') {
                $undefFound = $true
                break
            }
        }
        if (-not $undefFound) {
            $analysis.r4R5R6Violations += New-Violation -Rule 'R6' -Severity 'warning' -File $File.FullName -Line $defineLineNumber -Message 'LOCTEXT_NAMESPACE is defined but not undefined at the end of the file.'
        }
    }

    # The LOCTEXT/NSLOCTEXT inventory runs for EVERY file, not just the linted ones. The engine's
    # GatherText commandlet scans the whole module -- it knows nothing about this script's Scope /
    # Deferred / Allowlisted / Excluded split -- so an inventory that skipped the unlinted files
    # would report a gather count no real gather ever produces, and the baseline comparison would
    # be checking a number against itself. The R1/R2 literal rules below stay scoped.
    $lineIndex = 0
    foreach ($line in ($text -split "`r?`n")) {
        $lineIndex++
        if ($line -match 'NSLOCTEXT\(\s*"(?<ns>(?:\\.|[^"\\])*)"\s*,\s*"(?<key>(?:\\.|[^"\\])*)"\s*,\s*"(?<text>(?:\\.|[^"\\])*)"\s*\)') {
            $analysis.loctextCount++
            $analysis.inventoryEntries += [pscustomobject]@{
                namespace = Convert-CStringLiteral $Matches['ns']
                key = Convert-CStringLiteral $Matches['key']
                text = Convert-CStringLiteral $Matches['text']
                file = $File.FullName
                line = $lineIndex
            }
        }
        if ($line -match 'LOCTEXT\(\s*"(?<key>(?:\\.|[^"\\])*)"\s*,\s*"(?<text>(?:\\.|[^"\\])*)"\s*\)') {
            if ($lintable -and [string]::IsNullOrWhiteSpace($analysis.namespaceDefine)) {
                $analysis.r4R5R6Violations += New-Violation -Rule 'R5' -Severity 'error' -File $File.FullName -Line $lineIndex -Message 'LOCTEXT found without an active LOCTEXT_NAMESPACE define.'
            }
            $analysis.loctextCount++
            $analysis.inventoryEntries += [pscustomobject]@{
                namespace = $analysis.namespaceDefine
                key = Convert-CStringLiteral $Matches['key']
                text = Convert-CStringLiteral $Matches['text']
                file = $File.FullName
                line = $lineIndex
            }
        }
    }

    if (-not $lintable) {
        return [pscustomobject]$analysis
    }

    $fromTextPatterns = @(
        @{ rule = 'R1'; pattern = 'FText::From(?:String|Name)\(\s*TEXT\(\s*"(?<text>(?:\\.|[^"\\])*)"\s*\)\s*\)'; message = 'FText::FromString/FText::FromName with a string literal is invisible to gather; use LOCTEXT or NSLOCTEXT.' },
        @{ rule = 'R2'; pattern = 'FString::Printf\(\s*TEXT\(\s*"(?<fmt>(?:\\.|[^"\\])*)"\s*\)'; message = 'FString::Printf with a literal format string is invisible to gather in UI scope; use FText::Format or add // I18N-EXEMPT if intentionally non-display.' }
    )

    foreach ($spec in $fromTextPatterns) {
        $regex = [regex]::new($spec.pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
        foreach ($m in $regex.Matches($text)) {
            $line = Get-LineNumber -Text $text -Index $m.Index
            $segment = Get-LineSpanText -Text $text -Index $m.Index
            if ($segment -match 'I18N-EXEMPT') { continue }

            if ($spec.rule -eq 'R2') {
                $analysis.deferredLiteralCalls++
            }

            $literal = if ($spec.rule -eq 'R1') { Convert-CStringLiteral $m.Groups['text'].Value } else { Convert-CStringLiteral $m.Groups['fmt'].Value }
            $analysis.r1R2Violations += New-Violation -Rule $spec.rule -Severity 'error' -File $File.FullName -Line $line -Message ($spec.message + " Literal: `"$literal`".")
        }
    }

    return [pscustomobject]$analysis
}

function Merge-UniqueInventory {
    param([object[]]$Entries)

    $seen = @{}
    $unique = New-Object System.Collections.Generic.List[object]
    $warnings = New-Object System.Collections.Generic.List[object]

    foreach ($e in $Entries) {
        if (-not $e.namespace) { continue }
        $sep = [char]1
        $key = "$($e.namespace)$sep$($e.key)$sep$($e.text)"
        if ($seen.ContainsKey($key)) { continue }

        $composite = "$($e.namespace)$sep$($e.key)"
        if ($seen.ContainsKey($composite)) {
            $prev = $seen[$composite]
            if ($prev.text -ne $e.text) {
                throw "Duplicate LOCTEXT/NSLOCTEXT key collision in namespace '$($e.namespace)' for key '$($e.key)': '$($prev.text)' vs '$($e.text)'."
            }

            $warnings.Add([pscustomobject]@{
                namespace = $e.namespace
                key = $e.key
                text = $e.text
                file = $e.file
                line = $e.line
                message = 'Duplicate same-text LOCTEXT/NSLOCTEXT entry (warning only).'
            })
            $seen[$key] = $true
            continue
        }

        $seen[$composite] = $e
        $seen[$key] = $true
        $unique.Add([pscustomobject]@{
            namespace = $e.namespace
            key = $e.key
            text = $e.text
            file = $e.file
            line = $e.line
        })
    }

    return [pscustomobject]@{
        unique = $unique
        warnings = $warnings
    }
}

function New-BaselineMarkdown {
    param(
        [object[]]$Inventory,
        [int]$ExpectedCount,
        [int]$DeferredFileCount,
        [int]$DeferredLiteralCallCount
    )

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine('# DreamShader localization baseline')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('This inventory covers compile-time LOCTEXT/NSLOCTEXT entries that the Localization Dashboard gather step can see.')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('It spans EVERY source file, not just the ones this script lints. GatherText scans the whole')
    [void]$sb.AppendLine('module and knows nothing about the Scope / Deferred / Allowlisted / Excluded split, so an')
    [void]$sb.AppendLine('inventory narrower than the module would report a count no real gather ever produces. That')
    [void]$sb.AppendLine('includes the automation tests: their LOCTEXT entries (namespace `DreamShaderTests`) are')
    [void]$sb.AppendLine('gathered like any other, so they are listed here rather than quietly dropped.')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('`-IncludeDeferred` widens which files the R1/R2 literal rules run on; it does not change this count.')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('## Expected gather count')
    [void]$sb.AppendLine("$ExpectedCount")
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('## Inventory')
    [void]$sb.AppendLine('| Namespace | Key | Source text |')
    [void]$sb.AppendLine('| --- | --- | --- |')

    foreach ($entry in $Inventory | Sort-Object namespace, key, text) {
        $text = $entry.text -replace '\|', '\|'
        $ns = $entry.namespace -replace '\|', '\|'
        $key = $entry.key -replace '\|', '\|'
        [void]$sb.AppendLine("| $ns | $key | $text |")
    }

    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('## Deferred diagnostics inventory')
    [void]$sb.AppendLine("Deferred files: $DeferredFileCount")
    [void]$sb.AppendLine("Runtime FText::FromString/FText::FromName / FString::Printf literal call sites in deferred diagnostics: $DeferredLiteralCallCount")
    [void]$sb.AppendLine('Use -IncludeDeferred to lint MaterialAssetGeneration/ and Decompiler/ in the next phase.')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('## Auto-gathered metadata')
    [void]$sb.AppendLine('Unreal will add any auto-gathered UPROPERTY metadata (for example DisplayName and ToolTip) on top of this compile-time baseline. This file intentionally counts only LOCTEXT/NSLOCTEXT entries.')

    return $sb.ToString()
}

function Format-HumanReport {
    param(
        [object]$Summary,
        [object[]]$Violations,
        [object[]]$Warnings
    )

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine("Localization lint for $($Summary.pluginRoot)")
    [void]$sb.AppendLine("Scope: $($Summary.scopeCount) file(s), deferred: $($Summary.deferredCount) file(s), excluded: $($Summary.excludedCount) file(s), allowlisted: $($Summary.allowlistedCount) file(s)")
    [void]$sb.AppendLine("Gather inventory: $($Summary.gatherCount) unique LOCTEXT/NSLOCTEXT entries")
    [void]$sb.AppendLine("Deferred diagnostics: $($Summary.deferredFileCount) file(s), $($Summary.deferredLiteralCount) runtime literal call site(s)")

    foreach ($v in $Violations) {
        [void]$sb.AppendLine("$($v.file):$($v.line) [$($v.rule)] $($v.message)")
    }

    foreach ($w in $Warnings) {
        [void]$sb.AppendLine("$($w.file):$($w.line) [WARN:$($w.rule)] $($w.message)")
    }

    [void]$sb.AppendLine("Summary: $($Summary.errorCount) error(s), $($Summary.warningCount) warning(s)")
    [void]$sb.AppendLine("Expected gather count: $($Summary.gatherCount)")
    return $sb.ToString()
}

$pluginRoot = Get-PluginRoot
$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $pluginRoot 'Source') -Recurse -File -Include *.h, *.cpp | Sort-Object FullName

$analyses = New-Object System.Collections.Generic.List[object]
foreach ($file in $sourceFiles) {
    $analysis = Get-FileAnalysis -PluginRoot $pluginRoot -File $file -IncludeDeferred:$IncludeDeferred
    $analyses.Add($analysis)
}

$scopeFiles = @($analyses | Where-Object { $_.category -eq 'Scope' })
$deferredFiles = @($analyses | Where-Object { $_.category -eq 'Deferred' })
$excludedFiles = @($analyses | Where-Object { $_.category -eq 'Excluded' })
$allowlistedFiles = @($analyses | Where-Object { $_.category -eq 'Allowlisted' })

# Every file contributes to the inventory -- see the note in Get-FileAnalysis. The gather count
# must match what the engine's GatherText would find, which is the whole module.
$inventoryEntries = @()
foreach ($a in $analyses) {
    $inventoryEntries += @($a.inventoryEntries)
}

$inventory = Merge-UniqueInventory -Entries $inventoryEntries

$violations = New-Object System.Collections.Generic.List[object]
$warnings = New-Object System.Collections.Generic.List[object]

foreach ($a in $analyses) {
    if ($a.enabledForLint) {
        foreach ($v in $a.r1R2Violations) { $violations.Add($v) }
    }

    foreach ($v in $a.r4R5R6Violations) {
        if ($v.rule -eq 'R6') { $warnings.Add($v) } else { $violations.Add($v) }
    }
}

foreach ($w in $inventory.warnings) {
    $warnings.Add((New-Violation -Rule 'R3' -Severity 'warning' -File $w.file -Line $w.line -Message $w.message))
}

$deferredLiteralCount = 0
foreach ($a in $deferredFiles) {
    $deferredLiteralCount += $a.deferredLiteralCalls
}

$summary = [pscustomobject]@{
    pluginRoot = $pluginRoot
    scopeCount = $scopeFiles.Count
    deferredCount = $deferredFiles.Count
    excludedCount = $excludedFiles.Count
    allowlistedCount = $allowlistedFiles.Count
    gatherCount = $inventory.unique.Count
    deferredFileCount = $deferredFiles.Count
    deferredLiteralCount = $deferredLiteralCount
    errorCount = $violations.Count
    warningCount = $warnings.Count
}

if ($EmitBaseline) {
    New-BaselineMarkdown -Inventory $inventory.unique -ExpectedCount $summary.gatherCount -DeferredFileCount $summary.deferredFileCount -DeferredLiteralCallCount $summary.deferredLiteralCount
    exit 0
}

if ($Json) {
    [pscustomobject]@{
        includeDeferred = [bool]$IncludeDeferred
        summary = $summary
        violations = @($violations | Sort-Object file, line, rule)
        warnings = @($warnings | Sort-Object file, line, rule)
        inventory = @($inventory.unique | Sort-Object namespace, key, text)
    } | ConvertTo-Json -Depth 8
} else {
    Format-HumanReport -Summary $summary -Violations (@($violations | Sort-Object file, line, rule)) -Warnings (@($warnings | Sort-Object file, line, rule))
}

if ($violations.Count -gt 0) { exit 1 }
if ($warnings.Count -gt 0) { exit 0 }
exit 0
