<#
.SYNOPSIS
Removes the current user's .volund registration of this copy of Voelund CAD (L02).

.DESCRIPTION
Deletes HKCU\Software\Classes\Voelund.Document and the .volund link only when they point to
the voelund-cad.exe beside this script, so another copy's registration is left alone.

.PARAMETER ClassesRoot
The registry key that holds the classes; tests pass a key of their own.
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$ClassesRoot = 'HKCU:\Software\Classes'
)
$ErrorActionPreference = 'Stop'

$product = Join-Path $PSScriptRoot 'bin\voelund-cad.exe'
$expected = "`"$((Resolve-Path -LiteralPath $product).ProviderPath)`" `"%1`""
$class = 'Voelund.Document'
$commandKey = "$ClassesRoot\$class\shell\open\command"

if (-not (Test-Path -LiteralPath $commandKey)) {
    Write-Output 'No .volund registration found'
    return
}
$current = (Get-ItemProperty -LiteralPath $commandKey).'(default)'
if ($current -ne $expected) {
    Write-Output "The .volund registration belongs to another copy ($current); left unchanged"
    return
}
if ($PSCmdlet.ShouldProcess("$ClassesRoot\.volund", 'Unregister')) {
    Remove-Item -LiteralPath "$ClassesRoot\$class" -Recurse -Force
    $extension = "$ClassesRoot\.volund"
    if ((Test-Path -LiteralPath $extension) -and
        (Get-ItemProperty -LiteralPath $extension).'(default)' -eq $class) {
        Remove-Item -LiteralPath $extension -Recurse -Force
    }
    Write-Output 'Removed the .volund registration'
}
