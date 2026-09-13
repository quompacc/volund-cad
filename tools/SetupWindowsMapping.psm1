Set-StrictMode -Version Latest

function Get-VoelundCanonicalPath {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path)
}

function Get-VoelundDosDeviceTarget {
    [CmdletBinding()]
    param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$DriveLetter)

    if (-not ('VoelundDosDevice' -as [type])) {
        Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class VoelundDosDevice {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern uint QueryDosDevice(string deviceName, StringBuilder targetPath, int targetPathLength);
}
'@
    }
    $target = New-Object System.Text.StringBuilder 32768
    $length = [VoelundDosDevice]::QueryDosDevice("$DriveLetter`:", $target, $target.Capacity)
    if ($length -eq 0) {
        $nativeError = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        if ($nativeError -eq 2) {
            return $null
        }
        throw [System.ComponentModel.Win32Exception]::new($nativeError)
    }
    $value = $target.ToString().Split([char]0)[0]
    if ($value.StartsWith('\??\')) {
        return $value.Substring(4)
    }
    return $value
}

function Test-VoelundSamePath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Left,
        [Parameter(Mandatory)][string]$Right
    )

    return [string]::Equals(
        (Get-VoelundCanonicalPath $Left).TrimEnd([char[]]@('\', '/')),
        (Get-VoelundCanonicalPath $Right).TrimEnd([char[]]@('\', '/')),
        [System.StringComparison]::OrdinalIgnoreCase
    )
}

function Get-VoelundDriveState {
    param([string]$DriveLetter)

    $target = Get-VoelundDosDeviceTarget -DriveLetter $DriveLetter
    if ($null -eq $target -and -not (Test-Path -LiteralPath "$DriveLetter`:\")) {
        return [pscustomobject]@{ Kind = 'Missing'; Target = $null }
    }
    if ($null -ne $target -and $target -match '^[A-Za-z]:\\') {
        return [pscustomobject]@{ Kind = 'Subst'; Target = $target }
    }
    return [pscustomobject]@{ Kind = 'Occupied'; Target = $target }
}

function New-VoelundSubstMapping {
    param([string]$DriveLetter, [string]$RepositoryRoot)

    $output = @(& "$env:SystemRoot/System32/subst.exe" "$DriveLetter`:" $RepositoryRoot 2>&1)
    $exitCode = $LASTEXITCODE
    return [pscustomobject]@{ ExitCode = $exitCode; Detail = ($output -join ' ') }
}

function Initialize-VoelundWindowsMapping {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$RepositoryRoot,
        [ValidatePattern('^[A-Za-z]$')][string]$DriveLetter = 'V',
        [scriptblock]$Inspect = { param($drive) Get-VoelundDriveState $drive },
        [scriptblock]$Create = { param($drive, $root) New-VoelundSubstMapping $drive $root },
        [scriptblock]$ReadMarker = {
            param($root)
            $path = Join-Path $root '.voelund-project-root'
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
            return (Get-Content -Raw -LiteralPath $path).Trim()
        }
    )

    $repository = Get-VoelundCanonicalPath $RepositoryRoot
    $buildRoot = "$DriveLetter`:\"
    $state = & $Inspect $DriveLetter
    if ($state.Kind -eq 'Missing') {
        $creation = & $Create $DriveLetter $repository
        if ($creation.ExitCode -ne 0) {
            throw "Creating $DriveLetter`: failed (subst exit code $($creation.ExitCode)): $($creation.Detail)"
        }
        $state = & $Inspect $DriveLetter
    }
    if ($state.Kind -ne 'Subst') {
        throw "$DriveLetter`: is not a verified subst mapping (state: $($state.Kind))."
    }
    if (-not (Test-VoelundSamePath -Left $state.Target -Right $repository)) {
        throw "$DriveLetter`: is mapped to '$($state.Target)', not this repository."
    }
    $expected = 'voelund-cad-root-6e25701d-ec07-4e9f-95c4-390df153a07c'
    if ((& $ReadMarker $buildRoot) -cne $expected) {
        throw "$DriveLetter`: has a missing or incorrect repository marker."
    }
    return $buildRoot
}

Export-ModuleMember -Function Get-VoelundCanonicalPath, Get-VoelundDosDeviceTarget, Test-VoelundSamePath, Initialize-VoelundWindowsMapping
