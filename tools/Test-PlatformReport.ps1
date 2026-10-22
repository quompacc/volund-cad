#Requires -Version 7.5
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

[CmdletBinding()]
param([Parameter(Mandatory)][string]$ReportPath, [switch]$RequirePlatformAcceptance)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PlatformReportValidation.psm1') -Force
try {
    $report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json -AsHashtable -DateKind String
    Test-ReportContent $report (Split-Path -Parent ([IO.Path]::GetFullPath($ReportPath)))
    Write-Output 'structuralValidity=passed'
    $accepted = @($report.matrix | Where-Object status -ne 'passed').Count -eq 0
    Write-Output ('platformAcceptance=' + $(if ($accepted) { 'passed' } else { 'not-passed' }))
    if ($RequirePlatformAcceptance -and -not $accepted) { exit 2 }
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
