param(
    [ValidateRange(2, 20)]
    [int]$Runs = 5
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $root 'artifacts\shell-stress'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$sourceReport = Join-Path $root 'artifacts\shell-probe\shell-report.json'
$results = @()

for ($run = 1; $run -le $Runs; $run++) {
    Write-Output "Shell proof run $run of $Runs"
    & (Join-Path $PSScriptRoot 'run_shell_proof.ps1')
    $report = Get-Content -Raw -LiteralPath $sourceReport | ConvertFrom-Json
    $runPath = Join-Path $outputDir ("run-{0:d2}.json" -f $run)
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $runPath -Encoding utf8
    $results += $report
    Start-Sleep -Milliseconds 500
}

$passingRuns = @($results | Where-Object { $_.status -eq 'CONDITIONAL' })
$startupValues = @($results | ForEach-Object { [double]$_.launcher_to_report_ms })
$clickValues = @($results | ForEach-Object { [double]$_.interaction.click_latency_ms })
$zoomValues = @($results | ForEach-Object { [double]$_.interaction.zoom_event_median_ms })
$orbitValues = @($results | ForEach-Object { [double]$_.interaction.orbit_latency_ms })
$panValues = @($results | ForEach-Object { [double]$_.interaction.pan_latency_ms })

function Get-Median([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    $middle = [Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 0) {
        return ($sorted[$middle - 1] + $sorted[$middle]) / 2
    }
    return $sorted[$middle]
}

$summary = [ordered]@{
    probe = 'voelund-shell-repeatability'
    status = if ($passingRuns.Count -eq $Runs) { 'PASS' } else { 'FAIL' }
    runs = $Runs
    passing_runs = $passingRuns.Count
    all_interactions_passed = @($results | Where-Object {
        -not ($_.interaction.preselection.pass -and
            $_.interaction.picking_pass -and
            $_.interaction.zoom_pass -and
            $_.interaction.orbit_pass -and
            $_.interaction.pan_pass)
    }).Count -eq 0
    latency_ms = [ordered]@{
        launcher_to_report_median = [Math]::Round((Get-Median $startupValues), 3)
        launcher_to_report_max = [Math]::Round(($startupValues | Measure-Object -Maximum).Maximum, 3)
        picking_median = [Math]::Round((Get-Median $clickValues), 3)
        zoom_event_median = [Math]::Round((Get-Median $zoomValues), 3)
        orbit_sequence_median = [Math]::Round((Get-Median $orbitValues), 3)
        pan_sequence_median = [Math]::Round((Get-Median $panValues), 3)
    }
}

$summaryPath = Join-Path $outputDir 'summary.json'
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Output "Stress summary: $summaryPath"
Write-Output "Status: $($summary.status) ($($summary.passing_runs)/$Runs runs)"
if ($summary.status -ne 'PASS') {
    throw 'Shell repeatability proof failed'
}
