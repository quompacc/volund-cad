# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# Loaded by PlatformReport.Tests.ps1; uses its unique workspace directory.
$executionRoot = Join-Path $work 'execution'
$fakeBuild = Join-Path $executionRoot 'build'
New-Item -ItemType Directory -Path (Join-Path $fakeBuild 'apps'), (Join-Path $fakeBuild 'tests') -Force | Out-Null
'fixture' | Set-Content -LiteralPath (Join-Path $fakeBuild 'apps/voelund-cad.exe')
'fixture' | Set-Content -LiteralPath (Join-Path $fakeBuild 'tests/voelund_kernel_tests.exe')
'VOELUND_VIEWPORT_BACKEND:STRING=qt-opengl' | Set-Content -LiteralPath (Join-Path $fakeBuild 'CMakeCache.txt')
$developer = Join-Path $executionRoot 'developer.cmd'
$cmake = Join-Path $executionRoot 'cmake.cmd'
$ctest = Join-Path $executionRoot 'ctest.cmd'
"@echo off`r`nexit /b 0" | Set-Content -LiteralPath $developer
"@echo off`r`nexit /b 0" | Set-Content -LiteralPath $cmake
"@echo off`r`necho expected test failure`r`nexit /b 7" | Set-Content -LiteralPath $ctest
$context = @{ DeveloperShell = $developer; CMake = $cmake; CTest = $ctest }
$priorCapture = [Environment]::GetEnvironmentVariable('VOELUND_SMOKE_ARTIFACT_DIR', 'Process')
try {
    $env:VOELUND_SMOKE_ARTIFACT_DIR = 'fixture-parent-value'
    $run = Join-Path $executionRoot 'failed-test'
    New-Item -ItemType Directory -Path $run | Out-Null
    $result = Invoke-PlatformExecution $context $fakeBuild $run 'windows-qt-opengl' $false
    if ($result.results[1].status -ne 'failed' -or $result.results[1].exitCode -ne 7) { throw 'CTest failure lost.' }
    if ($env:VOELUND_SMOKE_ARTIFACT_DIR -ne 'fixture-parent-value') { throw 'Capture environment not restored.' }
    $run = Join-Path $executionRoot 'skip-test'
    New-Item -ItemType Directory -Path $run | Out-Null
    $result = Invoke-PlatformExecution $context $fakeBuild $run 'windows-qt-opengl' $true
    if ($result.results[1].status -ne 'skipped' -or (Test-Path (Join-Path $run 'captures'))) { throw 'Skip executed or adopted captures.' }
    "@echo off`r`necho expected build failure`r`nexit /b 9" | Set-Content -LiteralPath $cmake
    $run = Join-Path $executionRoot 'failed-build'
    New-Item -ItemType Directory -Path $run | Out-Null
    $result = Invoke-PlatformExecution $context $fakeBuild $run 'windows-qt-opengl' $false
    if ($result.results[0].exitCode -ne 9 -or $result.results[1].status -ne 'skipped') { throw 'Failed build ran tests.' }
} finally { [Environment]::SetEnvironmentVariable('VOELUND_SMOKE_ARTIFACT_DIR', $priorCapture, 'Process') }
# The public entry rejects reused destinations before touching setup or historical artifacts.
$generator = Join-Path $root 'tools/New-PlatformReport.ps1'
$existing = Join-Path $executionRoot 'existing.json'
'historical-report' | Set-Content -LiteralPath $existing
$originalHash = Get-ReportHash $existing
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $generator -Configuration windows-qt-opengl -BuildDirectory absent -ArtifactDirectory $executionRoot -RunId reused -ReportPath $existing *> $null
if ($LASTEXITCODE -ne 1 -or (Get-ReportHash $existing) -ne $originalHash) { throw 'Existing report overwritten.' }
New-Item -ItemType Directory -Path (Join-Path $executionRoot 'reused') | Out-Null
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $generator -Configuration windows-qt-opengl -BuildDirectory absent -ArtifactDirectory $executionRoot -RunId reused -ReportPath (Join-Path $executionRoot 'new.json') *> $null
if ($LASTEXITCODE -ne 1 -or (Test-Path (Join-Path $executionRoot 'new.json'))) { throw 'Existing run reused.' }
Write-Output 'Execution regressions passed: test failure, environment restoration, skip, build failure, existing report and existing run.'
