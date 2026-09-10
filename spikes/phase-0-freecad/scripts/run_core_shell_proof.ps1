$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$executionRoot = $root
if ($root -cmatch '[^\x00-\x7F]') {
    $executionRoot = (& cmd.exe /d /c "for %I in (`"$root`") do @echo %~sI").Trim()
    if (-not $executionRoot -or -not (Test-Path -LiteralPath $executionRoot)) {
        throw "FreeCAD core requires an ASCII-safe path, but no short path could be resolved for $root"
    }
}

$python = Join-Path $executionRoot '.tools\runtime\FreeCAD_1.1.3-Windows-x86_64-py311\bin\python.exe'
if (-not (Test-Path -LiteralPath $python)) {
    throw 'Pinned FreeCAD Python runtime not found. Run scripts\bootstrap_freecad.ps1 first.'
}

$output = Join-Path $executionRoot 'artifacts\core-shell-probe'
$reportPath = Join-Path $root 'artifacts\core-shell-probe\core-shell-report.json'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reportPath) | Out-Null
if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force
}

& $python (Join-Path $executionRoot 'scripts\prove_core_shell.py') --output $output
if ($LASTEXITCODE -ne 0) {
    if (Test-Path -LiteralPath $reportPath) {
        Get-Content -Raw -LiteralPath $reportPath | Write-Output
    }
    throw "Core-only shell proof failed with exit code $LASTEXITCODE"
}

$report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
if ($report.status -ne 'PASS') {
    throw "Core-only shell proof reported $($report.status)"
}
Write-Output "Core-only shell proof: PASS"
Write-Output "Report: $reportPath"
