#Requires -Version 7.5
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PlatformReportSupport.psm1')

function Need-Map($Value, [string]$Name) {
    Assert-Report ($Value -is [Collections.IDictionary] -and $Value.Count -gt 0) "Missing/non-object $Name"
}

function Need-Text($Value, [string]$Name) {
    Assert-Report ($Value -is [string] -and -not [string]::IsNullOrWhiteSpace($Value)) "Missing text: $Name"
}

function Test-ReportSource($Source, [string]$Base) {
    Need-Map $Source 'source'
    Assert-Report ($Source.commit -cmatch '^[0-9a-f]{40}$' -and $Source.dirty -is [bool]) 'Invalid source revision/dirty flag.'
    $path = Resolve-ReportArtifact $Source.snapshot $Base
    $snapshot = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json -AsHashtable -DateKind String
    Assert-Report (@($snapshot.files).Count -gt 0 -and @($snapshot.scope).Count -gt 0) 'Empty source inventory.'
    $paths = @($snapshot.files | ForEach-Object {
        Need-Text $_.path 'source path'
        Assert-Report ($_.sha256 -ceq 'deleted' -or $_.sha256 -cmatch '^[0-9a-f]{64}$') 'Invalid source hash.'
        $_.path
    })
    Assert-Report (@($paths | Sort-Object -Unique).Count -eq $paths.Count) 'Duplicate source path.'
    $canonical = ($snapshot.files | Sort-Object path | ForEach-Object { $_.path + ':' + $_.sha256 }) -join "`n"
    $digest = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($canonical))).ToLowerInvariant()
    Assert-Report ($snapshot.digest -ceq $digest -and $Source.before -ceq $digest) 'Source digest mismatch.'
    Assert-Report ($Source.after -cmatch '^[0-9a-f]{64}$') 'Missing source after digest.'
}

function Test-ReportResult($Result, [string]$Base) {
    Need-Map $Result 'result'
    Need-Text $Result.id 'result.id'
    Need-Text $Result.configurationId 'result.configurationId'
    Assert-Report ($Result.status -in @('passed','failed','skipped','blocked')) 'Invalid result status.'
    if ($Result.status -in @('skipped','blocked')) {
        Need-Text $Result.reason 'result.reason'
        return
    }
    Need-Text $Result.command 'result.command'
    Assert-Report ($Result.exitCode -is [int] -or $Result.exitCode -is [long]) 'Invalid exitCode.'
    Assert-Report (@($Result.artifacts).Count -gt 0) 'Result evidence missing.'
    foreach ($artifact in $Result.artifacts) { $null = Resolve-ReportArtifact $artifact $Base }
    if ($Result.status -eq 'passed') {
        Assert-Report ($Result.exitCode -eq 0) 'Passed result has nonzero exit code.'
        if ($Result.id -eq 'ctest') { Assert-ReportJUnit (Resolve-ReportArtifact $Result.junit $Base) }
    } else { Need-Text $Result.reason 'failed result reason' }
}

function Test-ReportMatrix($Report) {
    $expected = @{
        'windows-qt-opengl' = @('Windows','qt-opengl','windows', $null)
        'windows-native-diagnostic' = @('Windows','native','windows', $null)
        'freebsd-sway-control' = @('FreeBSD','qt-opengl','wayland','Sway')
        'freebsd-meridian' = @('FreeBSD','qt-opengl','wayland','Meridian')
        'linux-core' = @('Linux', $null, $null, $null)
    }
    Assert-Report ($Report.matrix -is [array] -and $Report.matrix.Count -eq 5) 'Matrix requires exactly five entries.'
    $ids = @($Report.matrix | ForEach-Object id)
    Assert-Report (@($ids | Sort-Object -Unique).Count -eq 5) 'Duplicate matrix ID.'
    foreach ($entry in $Report.matrix) {
        Need-Map $entry 'matrix entry'
        Assert-Report ($expected.ContainsKey($entry.id)) 'Unknown matrix target.'
        $values = $expected[$entry.id]
        Assert-Report ($entry.operatingSystem -ceq $values[0] -and $entry.backend -ceq $values[1] -and
            $entry.qpa -ceq $values[2] -and $entry.compositor -ceq $values[3]) 'Matrix identity mismatch.'
        Assert-Report ($entry.status -in @('passed','failed','skipped','blocked')) 'Invalid matrix status.'
        if ($entry.status -ne 'passed') { Need-Text $entry.reason 'matrix.reason' }
        if ($entry.id -ne $Report.configuration.id) {
            Assert-Report ($entry.status -in @('blocked','skipped') -and @($entry.resultIds).Count -eq 0) 'Unexecuted configuration claims results.'
            continue
        }
        Assert-Report ((@($entry.resultIds | Sort-Object) -join ',') -ceq 'build,ctest') 'Current matrix requires build and ctest.'
        $results = @($Report.results)
        $status = if (@($results | Where-Object status -eq 'failed').Count) { 'failed' }
            elseif (@($results | Where-Object status -ne 'passed').Count) { 'skipped' } else { 'passed' }
        if ($Report.source.before -cne $Report.source.after) { $status = 'failed' }
        if ($Report.runtime.status -eq 'failed') { $status = 'failed' }
        Assert-Report ($entry.status -ceq $status) 'Matrix contradicts execution results.'
    }
}

function Test-ReportRuntimeEvidence($Report, [string]$Base) {
    $runtime = $Report.runtime
    Need-Map $runtime 'runtime'
    if ($runtime.status -ne 'passed') {
        Assert-Report ($runtime.status -in @('failed','not-run')) 'Invalid runtime status.'
        Need-Text $runtime.reason 'runtime.reason'
        Assert-Report ($Report.capture.status -eq 'not-run') 'Unavailable runtime cannot claim capture review.'
        return
    }
    $directory = [IO.Path]::GetFullPath((Join-Path $Base $runtime.directory))
    Assert-Report (@($runtime.artifacts).Count -eq 6) 'Expected six runtime capture artifacts.'
    $expectedPaths = @()
    foreach ($app in 'voelund-cad','voelund-ui-preview') {
        foreach ($suffix in '.json','-widgets.png','-framebuffer.png') { $expectedPaths += Join-Path $directory "$app$suffix" }
    }
    $paths = @($runtime.artifacts | ForEach-Object { Resolve-ReportArtifact $_ $Base })
    Assert-Report (@($paths | Sort-Object -Unique).Count -eq 6 -and
        @($expectedPaths | Where-Object { $_ -notin $paths }).Count -eq 0) 'Runtime artifacts do not match named application captures.'
    $actual = Read-ReportRuntime $directory $Report.configuration.backend
    foreach ($app in 'voelund-cad','voelund-ui-preview') {
        foreach ($key in $actual[$app].Keys) {
            Assert-Report ($runtime.applications[$app][$key] -ceq $actual[$app][$key]) "Embedded runtime differs: $app/$key"
        }
    }
    Assert-Report ($Report.configuration.qpa -ceq $actual['voelund-cad'].qpa) 'QPA differs from runtime.'
    Assert-Report ($Report.capture.status -in @('captured-not-reviewed','visually-reviewed')) 'Invalid capture status.'
    if ($Report.capture.status -eq 'visually-reviewed') {
        Need-Text $Report.capture.review.note 'visual review note'
        Need-Text $Report.capture.review.reviewer 'visual reviewer'
        Assert-Report ($Report.capture.review.atUtc -match 'Z$') 'Missing UTC visual review time.'
        $reviewPaths = @($Report.capture.review.artifacts | ForEach-Object { Resolve-ReportArtifact $_ $Base })
        Assert-Report ($reviewPaths.Count -eq 4 -and @($paths | Where-Object { $_ -like '*.png' -and $_ -notin $reviewPaths }).Count -eq 0) 'Review must identify all four current PNGs.'
    }
}

function Test-ReportContent($Report, [string]$Base) {
    Need-Map $Report 'report'
    Assert-Report ($Report.schemaVersion -eq 2) 'Expected schemaVersion 2; historical v1 reports are not current evidence.'
    Need-Text $Report.runId 'runId'
    $date = [DateTimeOffset]::MinValue
    Assert-Report ($Report.generatedAtUtc -match '(Z|\+00:00)$' -and [DateTimeOffset]::TryParse($Report.generatedAtUtc, [ref]$date)) 'Invalid UTC timestamp.'
    Test-ReportSource $Report.source $Base
    Need-Map $Report.configuration 'configuration'
    $config = $Report.configuration
    Assert-Report ($config.id -in @('windows-qt-opengl','windows-native-diagnostic')) 'This report schema currently records Windows executions only.'
    $backend = if ($config.id -eq 'windows-qt-opengl') { 'qt-opengl' } else { 'native' }
    Assert-Report ($config.platform -ceq 'Windows' -and $config.backend -ceq $backend) 'Configuration backend mismatch.'
    Need-Text $config.osVersion 'osVersion'
    Need-Text $config.buildPreset 'buildPreset'
    Need-Text $config.compiler.path 'compiler.path'
    Need-Text $config.compiler.version 'compiler.version'
    Need-Text $config.compositorReason 'compositorReason'
    Need-Text $config.displayScope 'displayScope'
    Need-Map $Report.packageStatus 'packageStatus'
    foreach ($id in 'P01','P02','P03','P04') { Need-Text $Report.packageStatus[$id] "packageStatus.$id" }
    Assert-Report ($Report.results -is [array] -and $Report.results.Count -eq 2) 'Expected build and ctest results.'
    Assert-Report ((@($Report.results.id | Sort-Object) -join ',') -ceq 'build,ctest') 'Invalid result IDs.'
    foreach ($result in $Report.results) {
        Assert-Report ($result.configurationId -ceq $config.id) 'Result configuration mismatch.'
        Test-ReportResult $result $Base
    }
    $build = $Report.results | Where-Object id -eq 'build'
    if ($build.status -eq 'passed') {
        Test-ReportBuildEvidence $Report $Base
    }
    Need-Map $Report.capture 'capture'
    Need-Text $Report.capture.method 'capture.method'
    Test-ReportRuntimeEvidence $Report $Base
    $ctest = $Report.results | Where-Object id -eq 'ctest'
    if ($ctest.status -eq 'passed') { Assert-Report ($Report.runtime.status -eq 'passed') 'Passing tests require current runtime evidence.' }
    Test-ReportMatrix $Report
}

function Test-ReportBuildEvidence($Report, [string]$Base) {
    Assert-Report (@($Report.buildEvidence).Count -eq 3) 'Build binary/configuration evidence missing.'
    $files = @{}
    foreach ($artifact in $Report.buildEvidence) {
        $path = Resolve-ReportArtifact $artifact $Base
        $files[[IO.Path]::GetFileName($path)] = $path
    }
    foreach ($name in 'build.log','CMakeCache.txt','binaries.json') {
        Assert-Report ($files.ContainsKey($name)) "Build evidence missing $name"
    }
    $cache = Get-Content -LiteralPath $files['CMakeCache.txt'] -Raw
    $backend = [regex]::Match($cache, '(?m)^VOELUND_VIEWPORT_BACKEND:[^=]+=(.+)\r?$').Groups[1].Value.Trim()
    Assert-Report ($backend -ceq $Report.configuration.backend) 'Recorded cache backend mismatch.'
    $binaries = @(Get-Content -LiteralPath $files['binaries.json'] -Raw | ConvertFrom-Json -AsHashtable -DateKind String)
    foreach ($binary in $binaries) {
        Need-Text $binary.path 'binary.path'
        Assert-Report ($binary.sha256 -cmatch '^[0-9a-f]{64}$') 'Invalid binary digest.'
    }
    foreach ($name in 'voelund-cad.exe','voelund-ui-preview.exe','voelund_kernel_tests.exe') {
        Assert-Report (@($binaries | Where-Object { [IO.Path]::GetFileName($_.path) -ceq $name }).Count -eq 1) "Missing built binary identity: $name"
    }
}

Export-ModuleMember -Function Test-ReportContent
