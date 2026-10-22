#Requires -Version 7.5
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
Import-Module (Join-Path $root 'tools/PlatformReportSupport.psm1') -Force
Import-Module (Join-Path $root 'tools/PlatformReportValidation.psm1') -Force
Import-Module (Join-Path $root 'tools/PlatformReportExecution.psm1') -Force
$work = Join-Path $root ('build/platform-report-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
# The fixtures live only as long as the test; 235 leftover folders filled build/ (2026-10-14).
try {
. (Join-Path $PSScriptRoot 'PlatformReportFixtures.ps1')
$valid = New-PlatformReportFixture $work
$script:count = 0
function Check([string]$Name, [scriptblock]$Change, [bool]$ShouldPass = $false) {
    $candidate = $valid | ConvertTo-Json -Depth 40 | ConvertFrom-Json -AsHashtable -DateKind String
    & $Change $candidate | Out-Null
    $passed = $true
    try { Test-ReportContent $candidate $work } catch { $passed = $false; $failure = $_.Exception.Message }
    if ($passed -ne $ShouldPass) { throw "$Name expected valid=$ShouldPass; actual=$passed; $failure" }
    $script:count++
}
Check 'valid blocked matrix' {} $true
Check 'empty matrix' { param($x) $x.matrix = @() }
Check 'null source' { param($x) $x.source = $null }
Check 'empty source' { param($x) $x.source = @{} }
Check 'empty results' { param($x) $x.results = @() }
Check 'empty capture' { param($x) $x.capture = @{} }
Check 'missing runId' { param($x) $x.Remove('runId') }
Check 'bad UTC' { param($x) $x.generatedAtUtc = 'yesterday' }
Check 'bad status' { param($x) $x.matrix[0].status = 'green' }
Check 'missing blocked reason' { param($x) $x.matrix[4].reason = '' }
Check 'duplicate matrix' { param($x) $x.matrix[4] = $x.matrix[0] }
Check 'extra matrix' { param($x) $x.matrix += $x.matrix[0] }
Check 'missing evidence' { param($x) $x.results[1].artifacts = @() }
Check 'nonexistent artifact' { param($x) $x.results[1].artifacts[0].path = 'absent.log' }
Check 'changed artifact' { param($x) $x.results[1].artifacts[0].sha256 = 'a' * 64 }
Check 'failed tests passed matrix' { param($x) $x.results[1].status = 'failed'; $x.results[1].exitCode = 1; $x.results[1].reason = 'fixture failure' }
Check 'foreign platform passed' { param($x) $x.matrix[3].status = 'passed' }
Check 'backend mismatch' { param($x) $x.configuration.backend = 'native' }
Check 'QPA mismatch' { param($x) $x.configuration.qpa = 'offscreen' }
Check 'null compiler version' { param($x) $x.configuration.compiler.version = $null }
Check 'source changed during run' { param($x) $x.source.after = 'a' * 64 }
Check 'passed without runtime' { param($x) $x.runtime.status = 'not-run'; $x.runtime.reason = 'none'; $x.capture.status = 'not-run' }
Check 'review without images' { param($x) $x.capture.status = 'visually-reviewed'; $x.capture.review = @{} }
$goodXml = Get-Content -LiteralPath (Join-Path $work 'ctest.xml') -Raw
'<testsuite tests="0" failures="0" disabled="0" />' | Set-Content -LiteralPath (Join-Path $work 'empty.xml')
Check 'empty JUnit' { param($x) $x.results[1].junit = New-ReportArtifact (Join-Path $work 'empty.xml') $work }
$goodXml.Replace('architecture.source_limits', 'unrelated') | Set-Content -LiteralPath (Join-Path $work 'missing-case.xml')
Check 'missing required JUnit case' { param($x) $x.results[1].junit = New-ReportArtifact (Join-Path $work 'missing-case.xml') $work }
$goodXml.Replace('status="run"', 'status="notrun"') | Set-Content -LiteralPath (Join-Path $work 'skipped.xml')
Check 'skipped JUnit case' { param($x) $x.results[1].junit = New-ReportArtifact (Join-Path $work 'skipped.xml') $work }
$valid.runtime.applications['voelund-cad'].backend = 'native'
Check 'embedded runtime mismatch' {}
$valid.runtime.applications['voelund-cad'].backend = 'qt-opengl'
# Unrelated JSON must never be read as a runtime diagnostic.
'{"backend":"native"}' | Set-Content -LiteralPath (Join-Path $work 'captures/aaa-unrelated.json')
Check 'unrelated JSON ignored' {} $true
$runtimePath = Join-Path $work 'captures/voelund-cad.json'
$originalRuntime = Get-Content -LiteralPath $runtimePath -Raw
$originalRuntime.Replace('qt-opengl', 'native') | Set-Content -LiteralPath $runtimePath
Check 'actual runtime mismatch' { param($x) $x.runtime.artifacts = @(Get-FixtureRuntimeArtifacts $work) }
$originalRuntime | Set-Content -LiteralPath $runtimePath -NoNewline
# SkipTests must describe no new runtime, even if unrelated images already exist elsewhere.
Check 'honest skipped tests' { param($x)
    $x.results[1].status = 'skipped'; $x.results[1].reason = 'Explicit skip'; $x.results[1].exitCode = $null
    $x.runtime = @{ status = 'not-run'; reason = 'Explicit skip'; artifacts = @() }; $x.capture.status = 'not-run'
    $x.matrix[0].status = 'skipped'; $x.matrix[0].reason = 'Explicit skip'
} $true
$validPath = Join-Path $work 'valid.json'
$valid | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $validPath
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root 'tools/Test-PlatformReport.ps1') -ReportPath $validPath -RequirePlatformAcceptance *> (Join-Path $work 'cli.log')
if ($LASTEXITCODE -ne 2) { Get-Content (Join-Path $work 'cli.log'); throw 'Acceptance exit must be 2 for blocked platforms.' }
Write-Output "Platform report regressions passed: $script:count cases plus blocked acceptance exit code."
. (Join-Path $PSScriptRoot 'PlatformReportExecution.Tests.ps1')
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
