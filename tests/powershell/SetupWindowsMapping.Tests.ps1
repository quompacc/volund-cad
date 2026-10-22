# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Import-Module (Join-Path $projectRoot 'tools/SetupWindowsMapping.psm1') -Force

function Assert-Equal([object]$Actual, [object]$Expected, [string]$Case) {
    if ($Actual -cne $Expected) {
        throw "$Case`: expected '$Expected', got '$Actual'."
    }
}

$runRoot = Join-Path $env:TEMP ('voelund-mapping-' + [guid]::NewGuid().ToString('N'))
$unicodeName = 'Pr' + [char]0x00fc + 'fung ' + [char]0x6d4b + [char]0x8bd5
$firstRoot = Join-Path $runRoot $unicodeName
$secondRoot = Join-Path $runRoot 'second checkout'
New-Item -ItemType Directory -Force -Path $firstRoot, $secondRoot | Out-Null
$marker = 'voelund-cad-root-6e25701d-ec07-4e9f-95c4-390df153a07c'
Set-Content -NoNewline -LiteralPath (Join-Path $firstRoot '.voelund-project-root') $marker
Set-Content -NoNewline -LiteralPath (Join-Path $secondRoot '.voelund-project-root') $marker

Assert-Equal (Test-VoelundSamePath $firstRoot "$firstRoot\.") $true 'equivalent Unicode paths'
Assert-Equal (Test-VoelundSamePath $firstRoot $secondRoot) $false 'distinct checkouts'

function Test-MappingCase([hashtable]$Case) {
    $assert = ${function:Assert-Equal}
    $expectedRoot = Get-VoelundCanonicalPath $firstRoot
    $state = [pscustomobject]@{
        Kind = $Case.Kind
        Target = $Case.Target
        Creates = 0
        Inspections = 0
    }
    $inspect = {
        param($drive)
        & $assert $drive 'V' 'inspected drive'
        $state.Inspections++
        if ($Case.InspectionError) { throw 'inspection failed' }
        return [pscustomobject]@{ Kind = $state.Kind; Target = $state.Target }
    }.GetNewClosure()
    $create = {
        param($drive, $root)
        & $assert $drive 'V' 'created drive'
        & $assert $root $expectedRoot 'creation target'
        $state.Creates++
        if ($Case.ExitCode -eq 0) {
            $state.Kind = $Case.CreatedKind
            $state.Target = $Case.CreatedTarget
        }
        return [pscustomobject]@{ ExitCode = $Case.ExitCode; Detail = 'injected result' }
    }.GetNewClosure()
    $readMarker = {
        param($root)
        & $assert $root 'V:\' 'marker root'
        if ($Case.ContainsKey('Marker')) { return $Case.Marker }
        return (Get-Content -Raw -LiteralPath (Join-Path $state.Target '.voelund-project-root')).Trim()
    }.GetNewClosure()
    $failure = $null
    $result = $null
    try {
        $result = Initialize-VoelundWindowsMapping -RepositoryRoot $firstRoot `
            -Inspect $inspect -Create $create -ReadMarker $readMarker
    } catch {
        $failure = $_.Exception.Message
    }
    if ($Case.Error) {
        if (-not $failure -or $failure -notlike "*$($Case.Error)*") {
            throw "$($Case.Name): expected '$($Case.Error)', got '$failure'."
        }
    } else {
        if ($failure) { throw "$($Case.Name): $failure" }
        Assert-Equal $result 'V:\' $Case.Name
    }
    Assert-Equal $state.Creates $Case.Creates "$($Case.Name) creation count"
    if ($Case.Creates -eq 1 -and $Case.ExitCode -eq 0) {
        Assert-Equal $state.Inspections 2 "$($Case.Name) post-create verification"
    }
    Write-Host "$($Case.Name): passed"
}

$cases = @(
    @{ Name = 'existing Unicode target'; Kind = 'Subst'; Target = $firstRoot; Creates = 0 },
    @{ Name = 'other checkout with identical marker'; Kind = 'Subst'; Target = $secondRoot; Creates = 0; Error = 'not this repository' },
    @{ Name = 'wrong marker'; Kind = 'Subst'; Target = $firstRoot; Marker = 'wrong'; Creates = 0; Error = 'incorrect repository marker' },
    @{ Name = 'missing marker'; Kind = 'Subst'; Target = $firstRoot; Marker = $null; Creates = 0; Error = 'missing or incorrect repository marker' },
    @{ Name = 'occupied physical drive'; Kind = 'Occupied'; Target = '\Device\HarddiskVolume1'; Creates = 0; Error = 'not a verified subst mapping' },
    @{ Name = 'inspection failed'; Kind = 'Missing'; InspectionError = $true; Creates = 0; Error = 'inspection failed' },
    @{ Name = 'subst returned failure'; Kind = 'Missing'; ExitCode = 5; Creates = 1; Error = 'subst exit code 5' },
    @{ Name = 'created Unicode target'; Kind = 'Missing'; ExitCode = 0; CreatedKind = 'Subst'; CreatedTarget = $firstRoot; Creates = 1 },
    @{ Name = 'creation did not establish mapping'; Kind = 'Missing'; ExitCode = 0; CreatedKind = 'Missing'; Creates = 1; Error = 'not a verified subst mapping' },
    @{ Name = 'creation points to other checkout'; Kind = 'Missing'; ExitCode = 0; CreatedKind = 'Subst'; CreatedTarget = $secondRoot; Creates = 1; Error = 'not this repository' },
    @{ Name = 'created mapping has wrong marker'; Kind = 'Missing'; ExitCode = 0; CreatedKind = 'Subst'; CreatedTarget = $firstRoot; Marker = 'wrong'; Creates = 1; Error = 'incorrect repository marker' }
)
foreach ($case in $cases) {
    Test-MappingCase $case
}

Write-Host 'Setup-Windows mapping tests passed without changing any real drive mapping.'
