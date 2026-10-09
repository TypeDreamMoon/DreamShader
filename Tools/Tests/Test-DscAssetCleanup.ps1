#requires -Version 7.0
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Run with pwsh -NoProfile -File Tools/Tests/Test-DscAssetCleanup.ps1.
# Load only the production helpers by AST: evaluating dsc.ps1 itself would launch the editor.
$pluginRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$driverPath = Join-Path $pluginRoot '.skill/dsc.ps1'
$driverText = Get-Content -LiteralPath $driverPath -Raw -Encoding UTF8
$driverTokens = $null
$driverErrors = $null
$driverAst = [Management.Automation.Language.Parser]::ParseInput($driverText, [ref]$driverTokens, [ref]$driverErrors)
if ($driverErrors.Count -ne 0) { throw "dsc.ps1 has parse errors: $($driverErrors -join '; ')" }
foreach ($helperName in @('Remove-EmptyParents', 'Get-ContentRoots', 'Get-FileSnapshot', 'Read-AssetWriteManifest', 'Remove-NewAssetIfOwned')) {
    $definitions = @($driverAst.FindAll({
        param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $helperName
    }, $true))
    if ($definitions.Count -ne 1) { throw "Expected exactly one dsc helper '$helperName'." }
    . ([scriptblock]::Create($definitions[0].Extent.Text))
}

$script:assertions = 0
function Assert-True {
    param([bool]$Condition, [string]$Message)
    $script:assertions++
    if (-not $Condition) { throw "Assertion failed: $Message" }
}

function Assert-Rejected {
    param([scriptblock]$Action, [string]$Message)
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-True $rejected $Message
}

function Write-TestAsset {
    param([string]$Path, [string]$Text = 'test asset')
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    Set-Content -LiteralPath $Path -Value $Text -Encoding UTF8
}

function Write-TestManifest {
    param([string]$Path, [string]$RunId, [object[]]$Files)
    @{ schema = 'dreamshader-saved-assets'; version = 1; runId = $RunId; files = @($Files) } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Path -Encoding UTF8
}

# Every fake project and file lives beneath this one newly created directory. Never reuse a path.
$testDirectory = [IO.Path]::GetFullPath($PSScriptRoot)
$scratchName = '.dsc-cleanup-test-' + [guid]::NewGuid().ToString('N')
$scratchPath = [IO.Path]::GetFullPath((Join-Path $testDirectory $scratchName))
if ([IO.Path]::GetDirectoryName($scratchPath) -cne $testDirectory -or (Test-Path -LiteralPath $scratchPath)) {
    throw "Refusing to use a non-unique or unexpected test directory: $scratchPath"
}
[IO.Directory]::CreateDirectory($scratchPath) | Out-Null

try {
    $projectPath = Join-Path $scratchPath 'Project With Spaces'
    [IO.Directory]::CreateDirectory($projectPath) | Out-Null
    $pluginDescriptor = Join-Path $projectPath 'Plugins/TestPlugin/TestPlugin.uplugin'
    Write-TestAsset -Path $pluginDescriptor -Text '{"CanContainContent":true}'

    $roots = Get-ContentRoots -ProjectDir $projectPath
    $gameRoot = Join-Path $projectPath 'Content'
    $pluginContent = Join-Path $projectPath 'Plugins/TestPlugin/Content'
    Assert-True ($roots['Game'] -eq $gameRoot) 'Game root is recorded before Content exists'
    Assert-True ($roots['TestPlugin'] -eq $pluginContent) 'plugin root is recorded before Content exists'
    Assert-True (-not (Test-Path -LiteralPath $gameRoot)) 'test begins without a Game Content directory'
    Assert-True (-not (Test-Path -LiteralPath $pluginContent)) 'test begins without a plugin Content directory'
    $emptyBefore = Get-FileSnapshot -Directories @($roots.Values) -Filter '*.uasset'
    Assert-True ($emptyBefore.Count -eq 0) 'snapshot of missing Content roots is empty'

    $runId = [guid]::NewGuid().ToString('N')
    $manifestPath = Join-Path $scratchPath 'asset-writes.json'
    $latePluginAsset = Join-Path $pluginContent 'Generated/Late.uasset'
    Write-TestAsset $latePluginAsset
    Write-TestManifest -Path $manifestPath -RunId $runId -Files @(
        @{ path = $latePluginAsset; md5 = (Get-FileHash -LiteralPath $latePluginAsset -Algorithm MD5).Hash }
    )
    $written = Read-AssetWriteManifest -Path $manifestPath -RunId $runId
    Assert-True (Remove-NewAssetIfOwned -Path $latePluginAsset -Before $emptyBefore -Written $written -ContentRoots @($roots.Values)) 'owned asset in a newly created Content root is removed'
    Assert-True (-not (Test-Path -LiteralPath $latePluginAsset)) 'new plugin asset no longer exists'
    Assert-True (Test-Path -LiteralPath $pluginContent) 'cleanup keeps the plugin Content root itself'

    $preexisting = Join-Path $gameRoot 'Existing.uasset'
    Write-TestAsset -Path $preexisting -Text 'original data'
    $before = Get-FileSnapshot -Directories @($roots.Values) -Filter '*.uasset'
    Assert-True ($before.ContainsKey($preexisting)) 'preexisting asset is in the real snapshot'

    $ownedNew = Join-Path $gameRoot 'Generated/Nested/Owned.uasset'
    $unrelatedNew = Join-Path $gameRoot 'ConcurrentEditor.uasset'
    $changedAfterSave = Join-Path $gameRoot 'ChangedAfterSave.uasset'
    $outside = Join-Path $scratchPath 'Outside.uasset'
    $sibling = Join-Path $projectPath 'ContentSibling/Keep.uasset'
    $nonAsset = Join-Path $gameRoot 'Keep.uexp'
    foreach ($path in @($ownedNew, $unrelatedNew, $changedAfterSave, $outside, $sibling, $nonAsset)) {
        Write-TestAsset -Path $path -Text 'saved data'
    }
    # The commandlet may overwrite an existing file; that never makes it eligible for -CleanNew.
    Write-TestAsset -Path $preexisting -Text 'saved replacement'
    $entries = @(
        foreach ($path in @($ownedNew, $preexisting, $changedAfterSave, $outside, $sibling, $nonAsset)) {
            @{ path = $path; md5 = (Get-FileHash -LiteralPath $path -Algorithm MD5).Hash }
        }
    )
    Write-TestManifest -Path $manifestPath -RunId $runId -Files $entries
    $written = Read-AssetWriteManifest -Path $manifestPath -RunId $runId
    Write-TestAsset -Path $changedAfterSave -Text 'later editor changes'

    Assert-True (Remove-NewAssetIfOwned -Path $ownedNew -Before $before -Written $written -ContentRoots @($roots.Values)) 'new owned file with its saved hash is removed'
    Assert-True (-not (Test-Path -LiteralPath $ownedNew)) 'owned new file was deleted'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'Generated'))) 'only newly empty parent directories were removed'
    Assert-True (Test-Path -LiteralPath $gameRoot) 'Game Content root survives cleanup'

    foreach ($case in @(
        @{ Path = $unrelatedNew; Reason = 'concurrent unrelated new asset' },
        @{ Path = $preexisting; Reason = 'preexisting asset saved by the commandlet' },
        @{ Path = $changedAfterSave; Reason = 'asset changed after the recorded save' },
        @{ Path = $outside; Reason = 'owned path outside Content roots' },
        @{ Path = $sibling; Reason = 'ContentSibling is outside the root boundary' },
        @{ Path = $nonAsset; Reason = 'non-uasset file' }
    )) {
        Assert-True (-not (Remove-NewAssetIfOwned -Path $case.Path -Before $before -Written $written -ContentRoots @($roots.Values))) "$($case.Reason) is ineligible"
        Assert-True (Test-Path -LiteralPath $case.Path) "$($case.Reason) remains on disk"
    }

    $written[$changedAfterSave] = 'not-an-md5'
    Assert-True (-not (Remove-NewAssetIfOwned -Path $changedAfterSave -Before $before -Written $written -ContentRoots @($roots.Values))) 'invalid saved hash disables deletion'
    Assert-Rejected { Read-AssetWriteManifest -Path $manifestPath -RunId ([guid]::NewGuid().ToString('N')) } 'stale run nonce is rejected'
    Assert-Rejected { Read-AssetWriteManifest -Path (Join-Path $scratchPath 'missing.json') -RunId $runId } 'missing manifest is rejected'

    Set-Content -LiteralPath $manifestPath -Value '{broken json' -Encoding UTF8
    Assert-Rejected { Read-AssetWriteManifest -Path $manifestPath -RunId $runId } 'invalid JSON is rejected'
    @{ schema = 'different-schema'; version = 1; runId = $runId; files = @() } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Assert-Rejected { Read-AssetWriteManifest -Path $manifestPath -RunId $runId } 'wrong schema is rejected'
    @{ schema = 'dreamshader-saved-assets'; version = 2; runId = $runId; files = @() } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Assert-Rejected { Read-AssetWriteManifest -Path $manifestPath -RunId $runId } 'unknown version is rejected'
    @{ schema = 'dreamshader-saved-assets'; version = 1; runId = $runId } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Assert-Rejected { Read-AssetWriteManifest -Path $manifestPath -RunId $runId } 'missing files field is rejected'
    Write-TestManifest -Path $manifestPath -RunId $runId -Files @(@{ path = 'Content/Relative.uasset'; md5 = '0' * 32 })
    Assert-Rejected { Read-AssetWriteManifest -Path $manifestPath -RunId $runId } 'relative manifest path is rejected'
    Write-TestManifest -Path $manifestPath -RunId $runId -Files @()
    Assert-True ((Read-AssetWriteManifest -Path $manifestPath -RunId $runId).Count -eq 0) 'empty valid manifest is accepted without attributing any files'

    Write-Host "PASS: dsc asset cleanup ($script:assertions assertions; no engine required)."
}
finally {
    # Read back and verify the exact directory before the only recursive deletion in this test.
    $resolvedScratch = (Resolve-Path -LiteralPath $scratchPath).Path
    if ($resolvedScratch -cne $scratchPath -or [IO.Path]::GetDirectoryName($resolvedScratch) -cne $testDirectory -or [IO.Path]::GetFileName($resolvedScratch) -cne $scratchName) {
        throw "Refusing to clean an unexpected test directory: $resolvedScratch"
    }
    $links = @(
        Get-Item -LiteralPath $resolvedScratch -Force
        Get-ChildItem -LiteralPath $resolvedScratch -Force -Recurse
    ) | Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 }
    if (@($links).Count -ne 0) { throw "Refusing recursive cleanup through an unexpected link in $resolvedScratch" }
    Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
}
