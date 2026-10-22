# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build/windows-msvc',
    [string]$Label,
    [string]$TestRegex,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
# Run setup and CTest within the same execution context. A mapping created by
# a different sandboxed tool invocation may not exist in this one.
$context = & (Join-Path $PSScriptRoot 'setup-windows.ps1') -SkipConfigure -PassThru |
    Where-Object { $_.PSTypeNames -contains 'Voelund.WindowsBuildContext' }
if ($null -eq $context) { throw 'Windows setup did not return a build context.' }
if (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $context.BuildRoot $BuildDirectory
}
if (-not (Test-Path -LiteralPath (Join-Path $BuildDirectory 'CTestTestfile.cmake') -PathType Leaf)) {
    throw "No configured CTest build at '$BuildDirectory'. Configure and build it first."
}

$testArguments = @('--test-dir', $BuildDirectory, '--output-on-failure', '--no-tests=error')
if ($Label) { $testArguments += @('-L', $Label) }
if ($TestRegex) { $testArguments += @('-R', $TestRegex) }
if ($ReportPath) { $testArguments += @('--output-junit', $ReportPath) }
& $context.CTest @testArguments
if ($LASTEXITCODE -ne 0) {
    throw "CTest failed with exit code $LASTEXITCODE."
}
