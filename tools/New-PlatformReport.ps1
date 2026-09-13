#Requires -Version 7.5
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('windows-qt-opengl','windows-native-diagnostic')][string]$Configuration,
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$ReportPath,
    [Parameter(Mandatory)][string]$ArtifactDirectory,
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId = ('p01-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [guid]::NewGuid().ToString('N')),
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PlatformReportSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PlatformReportExecution.psm1') -Force
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$reportFile = [IO.Path]::GetFullPath($ReportPath)
$base = Split-Path -Parent $reportFile
$run = Join-Path ([IO.Path]::GetFullPath($ArtifactDirectory)) $RunId
Assert-Report (-not (Test-Path -LiteralPath $reportFile)) 'Report already exists; preserve historical evidence.'
Assert-Report (-not (Test-Path -LiteralPath $run)) 'Run directory already exists; choose a new RunId.'
$context = & (Join-Path $PSScriptRoot 'setup-windows.ps1') -SkipConfigure -PassThru |
    Where-Object { $_.PSTypeNames -contains 'Voelund.WindowsBuildContext' }
Assert-Report ($null -ne $context) 'Missing Windows build context.'
$build = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $context.BuildRoot $BuildDirectory }
Assert-Report (Test-Path -LiteralPath (Join-Path $build 'CTestTestfile.cmake')) 'Configure the build first.'
$cache = Read-PlatformCache $build
$backend = if ($Configuration -eq 'windows-qt-opengl') { 'qt-opengl' } else { 'native' }
Assert-Report ($cache['VOELUND_VIEWPORT_BACKEND'] -ceq $backend) 'Build cache backend mismatch.'
New-Item -ItemType Directory -Path $run -Force | Out-Null
New-Item -ItemType Directory -Path $base -Force | Out-Null
$before = Get-SourceSnapshot $root
$snapshotPath = Join-Path $run 'source.json'
$before | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $snapshotPath
$commit = (& git -C $root rev-parse HEAD).Trim()
Assert-Report ($LASTEXITCODE -eq 0) 'Cannot read git revision.'
$dirty = @(& git -C $root status --porcelain).Count -gt 0
$packages = @{}
$plan = Get-Content -LiteralPath (Join-Path $root 'docs/implementation-plan/packets.json') -Raw | ConvertFrom-Json
foreach ($packet in $plan.packets) { $packages[$packet.id] = $packet.status }
$execution = Invoke-PlatformExecution $context $build $run $Configuration ([bool]$SkipTests)
$after = Get-SourceSnapshot $root
$runtime = @{ status = 'not-run'; reason = 'Tests were skipped or build failed.'; artifacts = @() }
$capture = @{ status = 'not-run'; method = 'QWidget::grab and separate GL framebuffer; no compositor proof.' }
if ($execution.results[1].status -eq 'passed') {
    try {
        $directory = Join-Path $run 'captures'
        $applications = Read-ReportRuntime $directory $backend
        $artifacts = @()
        foreach ($app in 'voelund-cad','voelund-ui-preview') {
            foreach ($suffix in '.json','-widgets.png','-framebuffer.png') {
                $artifacts += New-ReportArtifact (Join-Path $directory "$app$suffix") $base
            }
        }
        $runtime = @{ status = 'passed'; reason = $null; directory = [IO.Path]::GetRelativePath($base, $directory); applications = $applications; artifacts = $artifacts }
        $capture.status = 'captured-not-reviewed'
    } catch {
        $runtime = @{ status = 'failed'; reason = $_.Exception.Message; artifacts = @() }
        $execution.results[1].status = 'failed'
        $execution.results[1].reason = 'Runtime evidence failed: ' + $_.Exception.Message
    }
}
$compiler = $cache['CMAKE_CXX_COMPILER']
$compilerVersion = (& $compiler 2>&1 | Select-Object -First 1).ToString()
$status = if (@($execution.results | Where-Object status -eq 'failed').Count) { 'failed' }
    elseif ($SkipTests) { 'skipped' } else { 'passed' }
if ($before.digest -cne $after.digest) { $status = 'failed' }
$reason = if ($status -eq 'passed') { $null } else { 'See build/test/runtime results and source before/after digest.' }
$report = @{
    schemaVersion = 2; runId = $RunId; generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    source = @{ commit = $commit; dirty = $dirty; snapshot = New-ReportArtifact $snapshotPath $base; before = $before.digest; after = $after.digest }
    configuration = @{
        id = $Configuration; platform = 'Windows'; osVersion = [Environment]::OSVersion.VersionString
        backend = $backend; qpa = if ($runtime.status -eq 'passed') { $runtime.applications['voelund-cad'].qpa } else { $null }
        buildPreset = if ($backend -eq 'native') { 'windows-msvc-native-diagnostic' } else { 'windows-msvc' }
        compiler = @{ path = $compiler; version = $compilerVersion }
        compositorReason = 'Desktop compositor identity not observed by this internal Qt capture.'
        displayScope = 'Per-application logical DPI, DPR and framebuffer dimensions are in runtime.applications. Physical monitor identity and OS scaling are not observed; DPR is not a physical-display proof.'
    }
    packageStatus = $packages; results = $execution.results; buildEvidence = $execution.buildEvidence
    matrix = New-PlatformMatrix $Configuration $status $reason
    runtime = $runtime; capture = $capture
}
# Convert artifact paths created against the run directory to report-relative paths.
foreach ($result in $report.results) {
    foreach ($artifact in $result.artifacts) { $artifact.path = [IO.Path]::GetRelativePath($base, (Join-Path $run $artifact.path)) }
    if ($result.ContainsKey('junit')) { $result.junit.path = [IO.Path]::GetRelativePath($base, (Join-Path $run $result.junit.path)) }
}
foreach ($artifact in $report.buildEvidence) { $artifact.path = [IO.Path]::GetRelativePath($base, (Join-Path $run $artifact.path)) }
$report | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $reportFile
Import-Module (Join-Path $PSScriptRoot 'PlatformReportValidation.psm1') -Force
try { Test-ReportContent $report $base } catch {
    [Console]::Error.WriteLine('Report validation failed: ' + $_.Exception.Message)
    exit 1
}
Write-Output "Wrote $reportFile; execution=$status"
if ($status -eq 'failed') { exit 1 }
exit 0
