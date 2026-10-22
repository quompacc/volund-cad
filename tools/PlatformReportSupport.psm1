#Requires -Version 7.5
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

Set-StrictMode -Version Latest

function Assert-Report($Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-ReportHash([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-SourceSnapshot([string]$Root) {
    $scope = @('src', 'include', 'apps', 'tests', 'tools', 'cmake', 'third_party',
        'CMakeLists.txt', 'CMakePresets.json', 'CMakeUserPresets.json', 'vcpkg.json',
        'vcpkg-configuration.json', '.clang-tidy', '.clang-format')
    $paths = @(& git -C $Root -c core.quotepath=false ls-files --cached --others --exclude-standard -- @scope)
    Assert-Report ($LASTEXITCODE -eq 0 -and $paths.Count -gt 0) 'Cannot inventory build sources.'
    # User presets may be ignored but still affect configuration.
    if (Test-Path -LiteralPath (Join-Path $Root 'CMakeUserPresets.json')) { $paths += 'CMakeUserPresets.json' }
    $records = @($paths | Sort-Object -Unique | ForEach-Object {
        $full = Join-Path $Root $_
        @{ path = $_; sha256 = if (Test-Path -LiteralPath $full -PathType Leaf) { Get-ReportHash $full } else { 'deleted' } }
    })
    $canonical = ($records | ForEach-Object { $_.path + ':' + $_.sha256 }) -join "`n"
    $digest = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($canonical))).ToLowerInvariant()
    @{ scope = $scope; files = $records; digest = $digest }
}

function New-ReportArtifact([string]$Path, [string]$Base) {
    @{ path = [IO.Path]::GetRelativePath($Base, [IO.Path]::GetFullPath($Path)); sha256 = Get-ReportHash $Path }
}

function Resolve-ReportArtifact($Artifact, [string]$Base) {
    Assert-Report ($Artifact -is [Collections.IDictionary]) 'Artifact must be an object.'
    Assert-Report ($Artifact.path -is [string] -and $Artifact.path.Length -gt 0) 'Artifact path is missing.'
    Assert-Report ($Artifact.sha256 -cmatch '^[0-9a-f]{64}$') 'Artifact hash is invalid.'
    $path = [IO.Path]::GetFullPath((Join-Path $Base $Artifact.path))
    Assert-Report (Test-Path -LiteralPath $path -PathType Leaf) "Missing artifact: $path"
    Assert-Report ((Get-ReportHash $path) -ceq $Artifact.sha256) "Changed artifact: $path"
    $path
}

function Read-ReportRuntime([string]$Directory, [string]$Backend) {
    $values = @{}
    foreach ($app in 'voelund-cad') {
        $path = Join-Path $Directory "$app.json"
        $data = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json -AsHashtable -DateKind String
        Assert-Report ($data.backend -ceq $Backend -and $data.qpa -ceq 'windows') "Unexpected runtime backend/QPA: $app"
        foreach ($key in 'qt','occt','gl','vendor','renderer','os') {
            Assert-Report ($data[$key] -is [string] -and $data[$key].Length -gt 0) "Missing runtime $key in $app"
        }
        foreach ($key in 'dpi','dpr','width','height') {
            Assert-Report ($data[$key] -is [ValueType] -and $data[$key] -gt 0) "Invalid runtime $key in $app"
        }
        foreach ($suffix in 'widgets','framebuffer') {
            $png = Join-Path $Directory "$app-$suffix.png"
            $bytes = [IO.File]::ReadAllBytes($png)
            Assert-Report ($bytes.Length -gt 8 -and [Convert]::ToHexString($bytes[0..7]) -eq '89504E470D0A1A0A') "Invalid PNG: $png"
        }
        $values[$app] = $data
    }
    $values
}

function Assert-ReportJUnit([string]$Path) {
    $settings = [Xml.XmlReaderSettings]::new()
    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
    $reader = [Xml.XmlReader]::Create($Path, $settings)
    $xml = [Xml.XmlDocument]::new()
    try { $xml.Load($reader) } finally { $reader.Dispose() }
    $suite = $xml.DocumentElement
    Assert-Report ($suite.LocalName -eq 'testsuite') 'Expected CTest JUnit testsuite.'
    $cases = @($suite.SelectNodes('testcase'))
    Assert-Report ($cases.Count -gt 0 -and [int]$suite.GetAttribute('tests') -eq $cases.Count) 'JUnit test count mismatch/empty suite.'
    Assert-Report ($suite.GetAttribute('failures') -eq '0' -and $suite.GetAttribute('disabled') -eq '0') 'JUnit contains failures or disabled tests.'
    foreach ($case in $cases) {
        Assert-Report ($case.GetAttribute('status') -eq 'run' -and $case.SelectNodes('failure|error|skipped').Count -eq 0) 'JUnit contains an unsuccessful case.'
    }
    foreach ($name in 'desktop.viewport_startup','desktop.viewport_startup.uninitialized',
        'architecture.platform_report','architecture.source_limits') {
        Assert-Report (@($cases | Where-Object { $_.GetAttribute('name') -ceq $name }).Count -eq 1) "Missing/duplicate required test: $name"
    }
}

Export-ModuleMember -Function *-Report*, Get-SourceSnapshot
