$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$candidates = @(@(
    (Get-Command python -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
    (Join-Path $root '.tools\runtime\FreeCAD_1.1.3-Windows-x86_64-py311\bin\python.exe')
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) })

if (-not $candidates) {
    throw 'Python not found. Run scripts\bootstrap_freecad.ps1 or install Python 3.'
}

Push-Location $root
try {
    & $candidates[0] -m unittest discover -s tests -v
    if ($LASTEXITCODE -ne 0) {
        throw "Source boundary tests failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
