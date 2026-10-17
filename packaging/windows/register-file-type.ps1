<#
.SYNOPSIS
Registers .volund documents for the current user with this copy of Voelund CAD (L02).

.DESCRIPTION
Writes HKCU\Software\Classes\.volund and the class Voelund.Document with the command
"<this folder>\bin\voelund-cad.exe" "%1". No administrator rights, nothing outside the
current user's classes. unregister-file-type.ps1 removes it again.

.PARAMETER ClassesRoot
The registry key that holds the classes; tests pass a key of their own.
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$ClassesRoot = 'HKCU:\Software\Classes'
)
$ErrorActionPreference = 'Stop'

$product = Join-Path $PSScriptRoot 'bin\voelund-cad.exe'
if (-not (Test-Path -LiteralPath $product)) {
    throw "voelund-cad.exe not found beside this script: $product"
}
$product = (Resolve-Path -LiteralPath $product).ProviderPath
$class = 'Voelund.Document'

if ($PSCmdlet.ShouldProcess("$ClassesRoot\.volund", "Register $product")) {
    $extension = New-Item -Path "$ClassesRoot\.volund" -Force
    Set-ItemProperty -LiteralPath $extension.PSPath -Name '(default)' -Value $class
    $document = New-Item -Path "$ClassesRoot\$class" -Force
    Set-ItemProperty -LiteralPath $document.PSPath -Name '(default)' -Value ('V' + [char]0x00F6 + 'lund-Dokument')
    $icon = New-Item -Path "$ClassesRoot\$class\DefaultIcon" -Force
    Set-ItemProperty -LiteralPath $icon.PSPath -Name '(default)' -Value "`"$product`",0"
    $command = New-Item -Path "$ClassesRoot\$class\shell\open\command" -Force
    Set-ItemProperty -LiteralPath $command.PSPath -Name '(default)' -Value "`"$product`" `"%1`""
    Write-Output "Registered .volund for $env:USERNAME with $product"
}
