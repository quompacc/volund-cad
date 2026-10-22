#Requires -Version 7.5
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

Import-Module (Join-Path $PSScriptRoot 'PlatformReportSupport.psm1')

function Read-PlatformCache([string]$Build) {
    $cache = @{}
    foreach ($line in Get-Content -LiteralPath (Join-Path $Build 'CMakeCache.txt')) {
        if ($line -match '^([^#/:][^:]*):[^=]+=(.*)$') { $cache[$Matches[1]] = $Matches[2] }
    }
    $cache
}

function New-PlatformMatrix([string]$Configuration, [string]$Status, $Reason) {
    foreach ($row in @(
        @('windows-qt-opengl','Windows','qt-opengl','windows',$null),
        @('windows-native-diagnostic','Windows','native','windows',$null),
        @('freebsd-sway-control','FreeBSD','qt-opengl','wayland','Sway'),
        @('freebsd-meridian','FreeBSD','qt-opengl','wayland','Meridian'),
        @('linux-core','Linux',$null,$null,$null))) {
        $selected = $row[0] -ceq $Configuration
        $windows = $row[1] -eq 'Windows'
        @{
            id = $row[0]; operatingSystem = $row[1]; backend = $row[2]; qpa = $row[3]; compositor = $row[4]
            status = if ($selected) { $Status } elseif ($windows) { 'skipped' } else { 'blocked' }
            reason = if ($selected) { $Reason } elseif ($windows) { 'Separate configuration report required.' }
                else { 'No documented authorized target host in this workspace inventory; not executed.' }
            resultIds = @(if ($selected) { 'build'; 'ctest' })
        }
    }
}

function Invoke-PlatformExecution($Context, [string]$Build, [string]$Run, [string]$Configuration, [bool]$Skip) {
    foreach ($path in $Context.DeveloperShell, $Context.CMake, $Build) {
        Assert-Report ($path -notmatch '[%"!&|<>^\r\n]') 'Unsafe shell path.'
    }
    $buildLog = Join-Path $Run 'build.log'
    $command = '"' + $Context.DeveloperShell + '" >nul && "' + $Context.CMake + '" --build "' + $Build + '" --parallel 6'
    & cmd.exe /d /s /c $command *> $buildLog
    $buildExit = $LASTEXITCODE
    $buildResult = @{
        id = 'build'; configurationId = $Configuration; command = $command; exitCode = $buildExit
        status = if ($buildExit -eq 0) { 'passed' } else { 'failed' }
        reason = if ($buildExit -eq 0) { $null } else { 'Build failed; see build.log.' }
        artifacts = @(New-ReportArtifact $buildLog $Run)
    }
    $ctest = @{ id = 'ctest'; configurationId = $Configuration; status = 'skipped';
        reason = 'Tests explicitly skipped or build failed.'; command = $null; exitCode = $null; artifacts = @() }
    $evidence = @()
    if ($buildExit -eq 0) {
        Copy-Item -LiteralPath (Join-Path $Build 'CMakeCache.txt') -Destination (Join-Path $Run 'CMakeCache.txt')
        $binaries = @(Get-ChildItem -LiteralPath (Join-Path $Build 'apps'), (Join-Path $Build 'tests') -Filter '*.exe' -Recurse -File |
            Sort-Object FullName | ForEach-Object { @{ path = $_.FullName; sha256 = Get-ReportHash $_.FullName } })
        Assert-Report ($binaries.Count -ge 2) 'No built applications/tests found.'
        $binaries | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Run 'binaries.json')
        $evidence = @('build.log','CMakeCache.txt','binaries.json' | ForEach-Object { New-ReportArtifact (Join-Path $Run $_) $Run })
    }
    if ($buildExit -eq 0 -and -not $Skip) {
        $oldCapture = [Environment]::GetEnvironmentVariable('VOELUND_SMOKE_ARTIFACT_DIR', 'Process')
        try {
            $env:VOELUND_SMOKE_ARTIFACT_DIR = Join-Path $Run 'captures'
            $junit = Join-Path $Run 'ctest.xml'
            $log = Join-Path $Run 'ctest.log'
            $ctest.command = "& '$($Context.CTest)' --test-dir '$Build' --output-on-failure --no-tests=error --output-junit '$junit'"
            & $Context.CTest --test-dir $Build --output-on-failure --no-tests=error --output-junit $junit *> $log
            $ctest.exitCode = $LASTEXITCODE
            $ctest.status = if ($LASTEXITCODE -eq 0) { 'passed' } else { 'failed' }
            $ctest.reason = if ($ctest.status -eq 'passed') { $null } else { 'CTest failed; see ctest.log.' }
            $ctest.artifacts = @(New-ReportArtifact $log $Run)
            if (Test-Path -LiteralPath $junit) {
                $ctest.junit = New-ReportArtifact $junit $Run
                $ctest.artifacts += New-ReportArtifact $junit $Run
            }
        } finally { [Environment]::SetEnvironmentVariable('VOELUND_SMOKE_ARTIFACT_DIR', $oldCapture, 'Process') }
    }
    if ($buildExit -eq 0) {
        foreach ($binary in $binaries) {
            if (-not (Test-Path -LiteralPath $binary.path) -or (Get-ReportHash $binary.path) -cne $binary.sha256) {
                $ctest.status = 'failed'; $ctest.exitCode = 1
                $ctest.command = 'Verify binary hashes after test execution'
                $ctest.reason = 'Built binary changed during the run.'
                $ctest.artifacts = @(New-ReportArtifact $buildLog $Run)
            }
        }
    }
    @{ results = @($buildResult, $ctest); buildEvidence = $evidence }
}

Export-ModuleMember -Function Read-PlatformCache, New-PlatformMatrix, Invoke-PlatformExecution
