#Requires -Version 7.5
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ReportPath,
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$Reviewer,
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$Note
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PlatformReportValidation.psm1') -Force
$report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json -AsHashtable -DateKind String
$base = Split-Path -Parent ([IO.Path]::GetFullPath($ReportPath))
Test-ReportContent $report $base
if ($report.runtime.status -ne 'passed') { throw 'No current runtime captures to review.' }
if ($report.capture.status -eq 'visually-reviewed') { throw 'Review already recorded; preserve it.' }
# Explicit attestation only, after the caller actually inspected all four images.
$report.capture.status = 'visually-reviewed'
$report.capture.review = @{
    reviewer = $Reviewer; note = $Note; atUtc = [DateTime]::UtcNow.ToString('o')
    artifacts = @($report.runtime.artifacts | Where-Object { $_.path -like '*.png' })
}
Test-ReportContent $report $base
$report | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $ReportPath
