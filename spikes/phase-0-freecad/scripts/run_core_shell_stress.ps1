param(
    [ValidateRange(2, 20)]
    [int]$Runs = 5
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $root 'artifacts\core-shell-stress'
$sourceReport = Join-Path $root 'artifacts\core-shell-probe\core-shell-report.json'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$reports = @()

for ($run = 1; $run -le $Runs; $run++) {
    Write-Output "Core-only shell run $run of $Runs"
    & (Join-Path $PSScriptRoot 'run_core_shell_proof.ps1')
    $report = Get-Content -Raw -LiteralPath $sourceReport | ConvertFrom-Json
    $runPath = Join-Path $outputDir ("run-{0:d2}.json" -f $run)
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $runPath -Encoding utf8
    $reports += $report
}

function Get-Median([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    $middle = [Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 0) {
        return ($sorted[$middle - 1] + $sorted[$middle]) / 2
    }
    return $sorted[$middle]
}

$summary = [ordered]@{
    probe = 'voelund-core-only-repeatability'
    status = if (@($reports | Where-Object { $_.status -ne 'PASS' }).Count -eq 0) { 'PASS' } else { 'FAIL' }
    runs = $Runs
    passing_runs = @($reports | Where-Object { $_.status -eq 'PASS' }).Count
    freecad_gui_loaded_in_any_run = @($reports | Where-Object {
        $_.freecad_gui_python_module_loaded -or $_.freecad_gui_dll_loaded
    }).Count -gt 0
    classic_main_window_created_in_any_run = @($reports | Where-Object {
        $_.classic_freecad_main_window_created
    }).Count -gt 0
    startup_to_report_median_ms = [Math]::Round((Get-Median @(
        $reports | ForEach-Object { [double]$_.startup_to_report_ms }
    )), 3)
}

$summaryPath = Join-Path $outputDir 'summary.json'
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Output "Core-only stress: $($summary.status) ($($summary.passing_runs)/$Runs runs)"
Write-Output "Summary: $summaryPath"
if ($summary.status -ne 'PASS') {
    throw 'Core-only shell repeatability proof failed'
}
