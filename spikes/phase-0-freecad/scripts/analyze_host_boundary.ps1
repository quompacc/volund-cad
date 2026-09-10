$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$runtime = Join-Path $root '.tools\runtime\FreeCAD_1.1.3-Windows-x86_64-py311'
if (-not (Test-Path -LiteralPath $runtime)) {
    throw "Pinned FreeCAD runtime not found at $runtime"
}

$outputDir = Join-Path $root 'artifacts\host-boundary'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$includeDirectory = Join-Path $runtime 'include'
$cmakeConfigs = @(
    Get-ChildItem -LiteralPath $runtime -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -in @('FreeCADConfig.cmake', 'freecad-config.cmake') } |
        ForEach-Object { $_.FullName.Substring($runtime.Length + 1) }
)
$freecadImportLibraries = @(
    Get-ChildItem -LiteralPath $runtime -Recurse -File -Filter 'FreeCAD*.lib' -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName.Substring($runtime.Length + 1) }
)
$runtimeLibraries = @(
    Get-ChildItem -LiteralPath (Join-Path $runtime 'bin') -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^FreeCAD.*\.(dll|pyd)$' } |
        Sort-Object Name |
        ForEach-Object { $_.Name }
)

$shellReportPath = Join-Path $root 'artifacts\shell-probe\shell-report.json'
$shellReport = if (Test-Path -LiteralPath $shellReportPath) {
    Get-Content -Raw -LiteralPath $shellReportPath | ConvertFrom-Json
} else {
    $null
}

$sdkAvailable = (Test-Path -LiteralPath $includeDirectory) -and
    ($cmakeConfigs.Count -gt 0) -and
    ($freecadImportLibraries.Count -gt 0)
$reparentingProven = $null -ne $shellReport -and
    $shellReport.status -eq 'CONDITIONAL' -and
    $shellReport.freecad_main_visible -eq $false -and
    $shellReport.shell_visible -eq $true

$report = [ordered]@{
    probe = 'freecad-host-boundary'
    status = if ($reparentingProven) { 'CONDITIONAL' } else { 'INCOMPLETE' }
    pinned_freecad_version = '1.1.3'
    package = [ordered]@{
        path = $runtime
        include_directory_present = Test-Path -LiteralPath $includeDirectory
        cmake_package_configs = $cmakeConfigs
        freecad_import_libraries = $freecadImportLibraries
        runtime_libraries = $runtimeLibraries
        development_sdk_available = $sdkAvailable
    }
    observed_python_gui_boundary = [ordered]@{
        freecad_executable_bootstraps_qapplication = $true
        classic_main_window_used_as_view_factory = $true
        native_view_reparenting_proven = $reparentingProven
        classic_main_window_hidden_after_reparenting = if ($null -ne $shellReport) {
            $shellReport.freecad_main_visible -eq $false
        } else {
            $false
        }
    }
    conclusion = if ($sdkAvailable) {
        'A main-window-free native host can be investigated with this package.'
    } else {
        'The official portable runtime is sufficient for Python embedding but not for compiling a supported custom C++ host. A source/development build is required to test removal of the FreeCAD main-window bootstrap.'
    }
    recommendation = 'CONDITIONAL GO: retain the reparenting code only as a spike boundary; make a source/development-build host experiment and Linux validation mandatory before product shell implementation.'
}

$reportPath = Join-Path $outputDir 'host-boundary-report.json'
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Output "Host-boundary report: $reportPath"
Write-Output "Status: $($report.status)"
Write-Output "Development SDK available: $sdkAvailable"
