#requires -Version 7.0
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Run with pwsh -NoProfile -File Tools/Tests/Test-DscInfo.ps1.
# `dsc.ps1 info` starts no engine, so the driver itself is run, against fake projects and fake engine roots.
$pluginRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$driverPath = Join-Path $pluginRoot '.skill/dsc.ps1'
$pluginVersion = (Get-Content -LiteralPath (Join-Path $pluginRoot 'DreamShader.uplugin') -Raw | ConvertFrom-Json).VersionName
$keys = @('plugin.version', 'plugin.commit', 'engine.version', 'engine.kind', 'os', 'locale', 'shell')

$script:assertions = 0
function Assert-True {
    param([bool]$Condition, [string]$Message)
    $script:assertions++
    if (-not $Condition) { throw "Assertion failed: $Message" }
}

function Write-TestFile {
    param([string]$Path, [string]$Text)
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    Set-Content -LiteralPath $Path -Value $Text -Encoding UTF8
}

function Invoke-Info {
    # The driver in a process of its own, as a reporter runs it. Returns the exit code, the lines and the values.
    param([string[]]$Arguments)
    $lines = @(& pwsh -NoProfile -File $driverPath info @Arguments 2>&1 | ForEach-Object { "$_" })
    $values = @{}
    foreach ($line in $lines) {
        if ($line -match '^([a-z.]+)\s+: (.*)$') { $values[$Matches[1]] = $Matches[2] }
    }
    return [pscustomobject]@{ Exit = $LASTEXITCODE; Lines = $lines; Values = $values }
}

function Assert-Report {
    param([pscustomobject]$Report, [string]$Case, [string[]]$Forbidden)
    Assert-True ($Report.Exit -eq 0) "$Case`: exits 0"
    Assert-True ($Report.Lines.Count -eq 8 -and $Report.Lines[0] -ceq 'DreamShader environment') "$Case`: a title and seven fields"
    for ($i = 0; $i -lt $keys.Count; $i++) {
        Assert-True ($Report.Lines[$i + 1] -match "^$([regex]::Escape($keys[$i]))\s+: \S") "$Case`: field $($i + 1) is $($keys[$i])"
    }
    Assert-True ($Report.Values['plugin.version'] -ceq $pluginVersion) "$Case`: plugin.version is the descriptor's VersionName"
    Assert-True ($Report.Values['shell'] -match '^pwsh \d+\.\d+') "$Case`: shell is the pwsh version"
    foreach ($line in $Report.Lines) {
        Assert-True ($line -notmatch '[A-Za-z]:[\\/]' -and $line -notmatch '\\\\') "$Case`: no absolute path in '$line'"
        foreach ($text in $Forbidden) {
            Assert-True (-not $line.Contains($text, [StringComparison]::OrdinalIgnoreCase)) "$Case`: '$line' does not name '$text'"
        }
    }
}

# Every fake project and engine lives beneath this one newly created directory. Never reuse a path.
$testDirectory = [IO.Path]::GetFullPath($PSScriptRoot)
$scratchName = '.dsc-info-test-' + [guid]::NewGuid().ToString('N')
$scratchPath = [IO.Path]::GetFullPath((Join-Path $testDirectory $scratchName))
if ([IO.Path]::GetDirectoryName($scratchPath) -cne $testDirectory -or (Test-Path -LiteralPath $scratchPath)) {
    throw "Refusing to use a non-unique or unexpected test directory: $scratchPath"
}
[IO.Directory]::CreateDirectory($scratchPath) | Out-Null

try {
    $project = Join-Path $scratchPath 'Project With Spaces/InfoTest.uproject'
    Write-TestFile -Path $project -Text '{"FileVersion":3,"EngineAssociation":"{00000000-0000-0000-0000-000000000000}"}'
    $buildVersion = '{"MajorVersion":5,"MinorVersion":8,"PatchVersion":1,"Changelist":0}'

    $installed = Join-Path $scratchPath 'Installed Engine'
    Write-TestFile -Path (Join-Path $installed 'Engine/Build/Build.version') -Text $buildVersion
    Write-TestFile -Path (Join-Path $installed 'Engine/Build/InstalledBuild.txt') -Text 'InstalledBuild'

    $source = Join-Path $scratchPath 'Source Engine'
    Write-TestFile -Path (Join-Path $source 'Engine/Build/Build.version') -Text $buildVersion
    [IO.Directory]::CreateDirectory((Join-Path $source 'Engine/Source/Runtime')) | Out-Null

    $started = [Diagnostics.Stopwatch]::StartNew()
    $report = Invoke-Info @('-Project', $project, '-Engine', $installed)
    $elapsed = $started.Elapsed.TotalSeconds
    Assert-Report $report 'installed engine' @($scratchName, 'Installed Engine', 'Project With Spaces')
    Assert-True ($report.Values['engine.version'] -ceq '5.8.1') 'installed engine: engine.version from Build.version'
    Assert-True ($report.Values['engine.kind'] -ceq 'launcher') 'installed engine: InstalledBuild.txt means launcher'
    # Generous for a cold pwsh start on a loaded machine; the driver itself does no engine work.
    Assert-True ($elapsed -lt 15) "installed engine: returns without starting an engine ($([int]$elapsed) s)"

    $report = Invoke-Info @('-Project', $project, '-Engine', $source)
    Assert-Report $report 'source engine' @($scratchName, 'Source Engine')
    Assert-True ($report.Values['engine.kind'] -ceq 'source') 'source engine: a Source tree without InstalledBuild.txt means source'

    # An engine that is not there, and a project that is not there: every engine field is unknown, and it still exits 0.
    $report = Invoke-Info @('-Project', (Join-Path $scratchPath 'Missing/Missing.uproject'), '-Engine', (Join-Path $scratchPath 'No Engine'))
    Assert-Report $report 'missing engine' @($scratchName, 'No Engine', 'Missing')
    Assert-True ($report.Values['engine.version'] -ceq 'unknown') 'missing engine: engine.version is unknown'
    Assert-True ($report.Values['engine.kind'] -ceq 'unknown') 'missing engine: engine.kind is unknown'

    # An association no registry knows, and no -Engine: unknown, not an error.
    $savedRoot = $env:UE_ENGINE_ROOT
    try {
        $env:UE_ENGINE_ROOT = $null
        $report = Invoke-Info @('-Project', $project)
        Assert-Report $report 'unregistered association' @($scratchName)
        Assert-True ($report.Values['engine.version'] -ceq 'unknown') 'unregistered association: engine.version is unknown'
    }
    finally {
        $env:UE_ENGINE_ROOT = $savedRoot
    }

    # A launcher engine with no registry key: only the launcher's LauncherInstalled.dat lists it. The driver
    # runs with ProgramData pointed at a fake manifest, and the association is a version no registry has.
    # A plugin artifact named after the same version comes first, and is not the engine.
    $launcherProject = Join-Path $scratchPath 'Launcher Project/LauncherTest.uproject'
    Write-TestFile -Path $launcherProject -Text '{"FileVersion":3,"EngineAssociation":"0.0"}'
    $fakeProgramData = Join-Path $scratchPath 'ProgramData'
    $manifest = [ordered]@{
        InstallationList = @(
            [ordered]@{ InstallLocation = $source; AppName = 'FabPlugin_0.0' }
            [ordered]@{ InstallLocation = $installed; AppName = 'UE_0.0' }
            [ordered]@{ InstallLocation = (Join-Path $scratchPath 'Gone Engine'); AppName = 'UE_0.1' }
        )
    }
    Write-TestFile -Path (Join-Path $fakeProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat') -Text ($manifest | ConvertTo-Json -Depth 4)
    $savedRoot = $env:UE_ENGINE_ROOT
    $savedProgramData = $env:ProgramData
    try {
        $env:UE_ENGINE_ROOT = $null
        $env:ProgramData = $fakeProgramData
        $report = Invoke-Info @('-Project', $launcherProject)
        Assert-Report $report 'launcher manifest' @($scratchName, 'Installed Engine', 'Launcher Project')
        Assert-True ($report.Values['engine.version'] -ceq '5.8.1') 'launcher manifest: the engine is found by its UE_ name'
        Assert-True ($report.Values['engine.kind'] -ceq 'launcher') 'launcher manifest: not the plugin artifact listed before it'

        # An engine the manifest lists whose directory is gone resolves to nothing.
        Write-TestFile -Path $launcherProject -Text '{"FileVersion":3,"EngineAssociation":"0.1"}'
        $report = Invoke-Info @('-Project', $launcherProject)
        Assert-Report $report 'launcher manifest, engine gone' @($scratchName, 'Gone Engine')
        Assert-True ($report.Values['engine.version'] -ceq 'unknown') 'launcher manifest, engine gone: engine.version is unknown'
    }
    finally {
        $env:UE_ENGINE_ROOT = $savedRoot
        $env:ProgramData = $savedProgramData
    }

    Write-Host "PASS: dsc info ($script:assertions assertions; no engine required)."
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
