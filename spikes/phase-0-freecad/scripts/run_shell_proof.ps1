$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$executionRoot = $root
if ($root -cmatch '[^\x00-\x7F]') {
    $executionRoot = (& cmd.exe /d /c "for %I in (`"$root`") do @echo %~sI").Trim()
    if (-not $executionRoot -or -not (Test-Path -LiteralPath $executionRoot)) {
        throw "FreeCAD requires an ASCII-safe path, but no short path could be resolved for $root"
    }
}

$candidates = @(@(
    $env:VOELUND_FREECAD_GUI,
    (Join-Path $executionRoot '.tools\freecad\FreeCAD.exe'),
    (Join-Path $executionRoot '.tools\runtime\FreeCAD_1.1.3-Windows-x86_64-py311\FreeCAD.exe'),
    'C:\Program Files\FreeCAD 1.1\bin\FreeCAD.exe',
    'C:\Program Files\FreeCAD 1.0\bin\FreeCAD.exe'
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) })

if (-not $candidates) {
    throw 'FreeCAD.exe not found. Set VOELUND_FREECAD_GUI or unpack FreeCAD into .tools\freecad.'
}

$configDir = Join-Path $executionRoot '.tools\config'
New-Item -ItemType Directory -Force -Path $configDir | Out-Null
$reportPath = Join-Path $root 'artifacts\shell-probe\shell-report.json'
if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force
}

$freecadGui = $candidates[0]
$startupTimer = [System.Diagnostics.Stopwatch]::StartNew()
& $freecadGui `
    -u (Join-Path $configDir 'user.cfg') `
    -s (Join-Path $configDir 'system.cfg') `
    (Join-Path $executionRoot 'scripts\prove_shell.py') `
    --pass --output (Join-Path $executionRoot 'artifacts\shell-probe')

$deadline = [DateTime]::UtcNow.AddSeconds(30)
while (-not (Test-Path -LiteralPath $reportPath) -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 100
}
if (-not (Test-Path -LiteralPath $reportPath)) {
    throw 'Shell proof did not create a report'
}
$report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
$startupTimer.Stop()
$report | Add-Member -NotePropertyName launcher_to_report_ms -NotePropertyValue ([Math]::Round($startupTimer.Elapsed.TotalMilliseconds, 2)) -Force
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath -Encoding utf8
if ($report.status -notin @('PASS', 'CONDITIONAL')) {
    throw "Shell proof reported $($report.status): $($report.error.message)"
}
