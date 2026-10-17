# L02 test helper for PackageRuntimeTests.cmake: reads or removes a test copy of the .volund
# registration under -Root (a key of the test's own, never the user's classes).
param(
    [ValidateSet('read', 'left', 'clean')][string]$Action,
    [string]$Root
)
$ErrorActionPreference = 'Stop'
$classes = Join-Path $Root 'Classes'
switch ($Action) {
    'read' {
        (Get-ItemProperty -LiteralPath (Join-Path $classes 'Voelund.Document\shell\open\command')).'(default)'
        (Get-ItemProperty -LiteralPath (Join-Path $classes '.volund')).'(default)'
    }
    'left' {
        Test-Path -LiteralPath (Join-Path $classes 'Voelund.Document')
        Test-Path -LiteralPath (Join-Path $classes '.volund')
    }
    'clean' {
        if (Test-Path -LiteralPath $Root) { Remove-Item -LiteralPath $Root -Recurse -Force }
    }
}
