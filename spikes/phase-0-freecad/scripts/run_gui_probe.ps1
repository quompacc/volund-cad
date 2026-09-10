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
$reportPath = Join-Path $root 'artifacts\gui-probe\gui-capabilities.json'
if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force
}

$freecadGui = $candidates[0]
& $freecadGui `
    -u (Join-Path $configDir 'user.cfg') `
    -s (Join-Path $configDir 'system.cfg') `
    (Join-Path $executionRoot 'scripts\probe_gui.py') `
    --pass --output (Join-Path $executionRoot 'artifacts\gui-probe')

if ($LASTEXITCODE -ne 0) {
    throw "GUI capability probe failed with exit code $LASTEXITCODE"
}

$deadline = [DateTime]::UtcNow.AddSeconds(20)
while (-not (Test-Path -LiteralPath $reportPath) -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 100
}
if (-not (Test-Path -LiteralPath $reportPath)) {
    throw 'GUI capability probe did not create a report'
}
$report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
if ($report.status -ne 'PASS') {
    throw "GUI capability probe reported $($report.status): $($report.error.message)"
}
