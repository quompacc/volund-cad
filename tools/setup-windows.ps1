[CmdletBinding()]
param(
    [switch]$SkipConfigure,
    [switch]$PassThru,
    [ValidateSet('windows-msvc', 'windows-msvc-native-diagnostic')]
    [string]$Preset = 'windows-msvc',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]*$')]
    [string]$BuildName
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$buildRoot = $repositoryRoot
Import-Module (Join-Path $PSScriptRoot 'SetupWindowsMapping.psm1') -Force

if ($repositoryRoot -notmatch '^[\x00-\x7F]+$') {
    $buildRoot = Initialize-VoelundWindowsMapping -RepositoryRoot $repositoryRoot
}

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer (vswhere.exe) was not found.'
}

$visualStudioRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $visualStudioRoot) {
    throw 'A Visual Studio C++ toolchain was not found.'
}

$vcpkgRoot = Join-Path $buildRoot '.tools\vcpkg'
if (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot 'vcpkg.exe'))) {
    New-Item -ItemType Directory -Force -Path (Join-Path $buildRoot '.tools') | Out-Null
    & git clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkgRoot
    if ($LASTEXITCODE -ne 0) { throw "vcpkg clone failed with exit code $LASTEXITCODE." }
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed with exit code $LASTEXITCODE." }
}

$qtRoot = Join-Path $buildRoot '.tools\qt'
$qtConfig = Join-Path $qtRoot 'lib\cmake\Qt6\Qt6Config.cmake'
if (-not (Test-Path -LiteralPath $qtConfig)) {
    $qtArchive = '6.11.1-0-202605090529qtbase-Windows-Windows_11_24H2-MSVC2022-Windows-Windows_11_24H2-X86_64.7z'
    $qtBaseUrl = 'https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6111/qt6_6111_msvc2022_64/qt.qt6.6111.win64_msvc2022_64'
    $downloadDirectory = Join-Path $buildRoot '.tools\downloads\qt'
    $archivePath = Join-Path $downloadDirectory $qtArchive
    New-Item -ItemType Directory -Force -Path $downloadDirectory, $qtRoot | Out-Null
    Invoke-WebRequest -Uri "$qtBaseUrl/$qtArchive" -OutFile $archivePath
    $actualHash = (Get-FileHash -Algorithm SHA1 -LiteralPath $archivePath).Hash.ToLowerInvariant()
    if ($actualHash -ne '6f554628540ab947d48294e208cc3caae7f023d2') {
        throw "Qt archive hash mismatch: $actualHash"
    }
    $sevenZip = 'C:\Program Files\7-Zip\7z.exe'
    if (-not (Test-Path -LiteralPath $sevenZip)) {
        throw '7-Zip is required to extract the official Qt archive.'
    }
    & $sevenZip x $archivePath "-o$qtRoot" -y
    if ($LASTEXITCODE -ne 0) {
        throw "Qt extraction failed with exit code $LASTEXITCODE."
    }
}

$developerShell = Join-Path $visualStudioRoot 'VC\Auxiliary\Build\vcvars64.bat'
$cmake = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$context = [pscustomobject]@{
    PSTypeName = 'Voelund.WindowsBuildContext'
    BuildRoot = $buildRoot
    CMake = $cmake
    CTest = Join-Path (Split-Path $cmake) 'ctest.exe'
    DeveloperShell = $developerShell
}
if ($SkipConfigure) {
    if ($PassThru) { $context }
    return
}
$command = "`"$developerShell`" >nul && `"$cmake`" --preset $Preset"
if ($BuildName) { $command += " -B `"build/$BuildName`"" }
Push-Location $buildRoot
try {
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE."
    }
} finally {
    Pop-Location
}
if ($PassThru) { $context }
