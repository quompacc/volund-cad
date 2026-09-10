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
    $env:VOELUND_FREECAD_CMD,
    (Join-Path $executionRoot '.tools\freecad\bin\FreeCADCmd.exe'),
    (Join-Path $executionRoot '.tools\runtime\FreeCAD_1.1.3-Windows-x86_64-py311\bin\FreeCADCmd.exe'),
    'C:\Program Files\FreeCAD 1.1\bin\FreeCADCmd.exe',
    'C:\Program Files\FreeCAD 1.0\bin\FreeCADCmd.exe',
    (Join-Path $env:LOCALAPPDATA 'Programs\FreeCAD 1.1\bin\FreeCADCmd.exe')
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) })

if (-not $candidates) {
    throw 'FreeCADCmd.exe not found. Set VOELUND_FREECAD_CMD or unpack FreeCAD into .tools\freecad.'
}

$freecadCmd = $candidates[0]
$configDir = Join-Path $executionRoot '.tools\config'
New-Item -ItemType Directory -Force -Path $configDir | Out-Null
$userCfg = Join-Path $configDir 'user.cfg'
$systemCfg = Join-Path $configDir 'system.cfg'

& $freecadCmd `
    -u $userCfg `
    -s $systemCfg `
    (Join-Path $executionRoot 'scripts\prove_backend.py') `
    --pass --output (Join-Path $executionRoot 'artifacts\backend-proof')
if ($LASTEXITCODE -ne 0) {
    throw "Backend proof failed with exit code $LASTEXITCODE"
}
