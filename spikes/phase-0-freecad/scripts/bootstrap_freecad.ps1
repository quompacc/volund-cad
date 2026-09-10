$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$version = '1.1.3'
$archiveName = 'FreeCAD_1.1.3-Windows-x86_64-py311.7z'
$downloadUrl = "https://github.com/FreeCAD/FreeCAD/releases/download/$version/$archiveName"
$expectedSha256 = '9c6959dc9c4dba64dd818a62447e3dfedb4221d776fb044b239d462f150bcec4'
$archivePath = Join-Path $root ".tools\downloads\$archiveName"
$runtimePath = Join-Path $root ".tools\runtime\FreeCAD_1.1.3-Windows-x86_64-py311"

if (Test-Path -LiteralPath (Join-Path $runtimePath 'bin\FreeCADCmd.exe')) {
    Write-Output "FreeCAD $version is already available at $runtimePath"
    return
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $archivePath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $runtimePath) | Out-Null

if (-not (Test-Path -LiteralPath $archivePath)) {
    curl.exe -L --fail --output $archivePath $downloadUrl
    if ($LASTEXITCODE -ne 0) {
        throw "FreeCAD download failed with exit code $LASTEXITCODE"
    }
}

$actualSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash.ToLowerInvariant()
if ($actualSha256 -ne $expectedSha256) {
    throw "FreeCAD archive checksum mismatch. Expected $expectedSha256, got $actualSha256"
}

$sevenZip = @(
    (Get-Command 7z.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source),
    'C:\Program Files\7-Zip\7z.exe'
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
if (-not $sevenZip) {
    throw '7-Zip is required to unpack the portable FreeCAD build.'
}

& $sevenZip x $archivePath "-o$(Join-Path $root '.tools\runtime')" -y
if ($LASTEXITCODE -ne 0) {
    throw "FreeCAD extraction failed with exit code $LASTEXITCODE"
}

Write-Output "FreeCAD $version is ready at $runtimePath"
