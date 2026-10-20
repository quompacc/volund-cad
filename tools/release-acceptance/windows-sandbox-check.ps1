# L08: runs inside Windows Sandbox (clean Windows, no Visual Studio, no dev PATH); started by
# Start-WindowsSandboxCheck.ps1. C:\l08 is the mapped host folder: package ZIP, test
# executables, fixtures; results go to C:\l08\out.
# Smart App Control is on in a fresh Windows 11 and blocks the unsigned program; that is
# recorded first, then switched off inside the disposable sandbox only (user decision
# 2026-10-20) so the product itself can be checked. Signing belongs to L10.
$ErrorActionPreference = 'Continue'
$root = 'C:\l08'
$out = Join-Path $root 'out'
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out 'results.txt'
function Note([string]$text) { Add-Content -Path $log -Value $text -Encoding utf8 }
Set-Content -Path $log -Value "L08 sandbox run $(Get-Date -Format o)" -Encoding utf8
Note "OS: $((Get-CimInstance Win32_OperatingSystem).Caption) $((Get-CimInstance Win32_OperatingSystem).Version)"
Note "GPU: $((Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name + ' ' + $_.DriverVersion }) -join '; ')"
Note "PATH: $env:Path"
Note "VC runtime in System32: $(Test-Path C:\Windows\System32\vcruntime140.dll) debug: $(Test-Path C:\Windows\System32\vcruntime140d.dll) ucrtbased: $(Test-Path C:\Windows\System32\ucrtbased.dll)"

# Unpack into a folder with umlaut and space, as a user would.
$zip = Get-ChildItem $root -Filter 'voelund-cad-*-windows-x64.zip' | Select-Object -First 1
$target = Join-Path $env:USERPROFILE 'Desktop\Völund Prüfung'
Expand-Archive -Path $zip.FullName -DestinationPath $target -Force
$package = Get-ChildItem $target -Directory | Select-Object -First 1
$bin = Join-Path $package.FullName 'bin'
$exe = Join-Path $bin 'voelund-cad.exe'
Note "Package: $($zip.Name) -> $($package.FullName)"
Note "bin files: $((Get-ChildItem $bin -File).Count); exe exists: $(Test-Path $exe)"

function Run([string]$name, [string]$file, [string[]]$arguments = @(), [hashtable]$environment = @{}) {
    $saved = @{}
    foreach ($key in $environment.Keys) { $saved[$key] = [Environment]::GetEnvironmentVariable($key); [Environment]::SetEnvironmentVariable($key, $environment[$key]) }
    $stdout = Join-Path $out "$name.out.txt"; $stderr = Join-Path $out "$name.err.txt"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        $info = New-Object Diagnostics.ProcessStartInfo
        $info.FileName = $file
        $info.Arguments = ($arguments -join ' ')
        $info.UseShellExecute = $false
        $info.RedirectStandardOutput = $true
        $info.RedirectStandardError = $true
        $info.WorkingDirectory = $out
        $process = [Diagnostics.Process]::Start($info)
        $outTask = $process.StandardOutput.ReadToEndAsync()
        $errTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(600000)) { $process.Kill(); Note "$name : TIMEOUT" }
        $process.WaitForExit()
        [IO.File]::WriteAllText($stdout, $outTask.Result)
        [IO.File]::WriteAllText($stderr, $errTask.Result)
        Note ("{0} : exit {1} (0x{1:X8}) ({2:n1} s)" -f $name, $process.ExitCode, $watch.Elapsed.TotalSeconds)
    } catch {
        Note "$name : START FAILED $($_.Exception.Message)"
    }
    foreach ($key in $environment.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key]) }
}

# Unicode documents next to the package.
$docs = Join-Path $env:USERPROFILE 'Documents\Prüf Ordner äöü'
New-Item -ItemType Directory -Force $docs | Out-Null
Copy-Item (Join-Path $root 'fixtures\v09.volund') (Join-Path $docs 'Teil mit Ä.volund')
Copy-Item (Join-Path $root 'fixtures\v16.volund') (Join-Path $docs 'Älteres Teil.volund')

function Policy([string]$when) {
    Note "[$when] SAC state: $((Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -ErrorAction SilentlyContinue).VerifiedAndReputablePolicyState)"
    Note "[$when] CiTool policies: $((CiTool.exe --list-policies 2>&1 | Select-String 'Friendly Name|Is Currently Enforced' | ForEach-Object { $_.Line.Trim() }) -join ' | ')"
}
Policy 'before'
Run 'version-blocked' $exe @('--version')
Get-WinEvent -LogName 'Microsoft-Windows-CodeIntegrity/Operational' -MaxEvents 6 -ErrorAction SilentlyContinue |
    ForEach-Object { $m = $_.Message -replace '\s+', ' '; Note "CI EVENT $($_.Id) $($_.TimeCreated): $($m.Substring(0, [Math]::Min(400, $m.Length)))" }
# User decision 2026-10-20, test run in the disposable sandbox only: Smart App Control off,
# policies refreshed without restart. The host stays untouched.
reg.exe add 'HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy' /v VerifiedAndReputablePolicyState /t REG_DWORD /d 0 /f | Out-Null
CiTool.exe --refresh --json 2>&1 | Out-Null
Start-Sleep -Seconds 3
Policy 'after switching SAC off'

$smoke = Join-Path $out 'smoke'
New-Item -ItemType Directory -Force $smoke | Out-Null
Run 'version' $exe @('--version')
Run 'smoke-start' $exe @('--smoke-test') @{ VOELUND_SMOKE_ARTIFACT_DIR = $smoke; QT_DEBUG_PLUGINS = '1' }
Run 'smoke-open-unicode' $exe @('--smoke-test-open', "`"$docs\Teil mit Ä.volund`"") @{ VOELUND_SMOKE_ARTIFACT_DIR = $smoke }
Run 'check-unicode' $exe @('--check-document', "`"$docs\Teil mit Ä.volund`"")
Run 'resave-old' $exe @('--check-document', "`"$docs\Älteres Teil.volund`"", '--resave', "`"$docs\Älteres Teil neu.volund`"")
Run 'check-resaved' $exe @('--check-document', "`"$docs\Älteres Teil neu.volund`"")

# File type per user (HKCU), then the registered command.
Run 'register' 'powershell.exe' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$($package.FullName)\register-file-type.ps1`"")
Note "Association: $((Get-ItemProperty 'HKCU:\Software\Classes\.volund' -ErrorAction SilentlyContinue).'(default)') -> $((Get-ItemProperty 'HKCU:\Software\Classes\Voelund.Document\shell\open\command' -ErrorAction SilentlyContinue).'(default)')"
Note "cmd assoc: $(cmd /c assoc .volund 2>&1)"

# The UI flows through the real shell, run from the package's bin folder with its DLLs.
$tests = Join-Path $root 'tests'
foreach ($test in Get-ChildItem $tests -Filter '*.exe') { Copy-Item $test.FullName $bin }
$flows = @('voelund_reference_part_flow_tests', 'voelund_reference_ring_flow_tests', 'voelund_ui_flow_tests')
foreach ($flow in $flows) {
    if (Test-Path (Join-Path $bin "$flow.exe")) { Run $flow (Join-Path $bin "$flow.exe") }
}
foreach ($scale in @('1', '1.5', '2')) {
    $dir = Join-Path $out "ui-flow-scale-$scale"
    New-Item -ItemType Directory -Force $dir | Out-Null
    Run "ui-flow-captures-$scale" (Join-Path $bin 'voelund_ui_flow_tests.exe') @() @{ VOELUND_UI_FLOW_ARTIFACT_DIR = $dir; QT_SCALE_FACTOR = $scale }
}

# The installed program on screen, captured from the desktop (compositor view, not a grab).
$shot = Join-Path $out 'desktop-product.png'
$process = Start-Process -FilePath $exe -ArgumentList "`"$docs\Teil mit Ä.volund`"" -PassThru
Start-Sleep -Seconds 12
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
# Windows PowerShell is not DPI aware: without this the capture is a scaled-down crop.
Add-Type -Namespace L08 -Name Dpi -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();'
[void][L08.Dpi]::SetProcessDPIAware()
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$bitmap.Save($shot, [System.Drawing.Imaging.ImageFormat]::Png)
Note "Desktop capture $($bounds.Width)x$($bounds.Height): $shot; process alive: $(-not $process.HasExited)"
if (-not $process.HasExited) {
    $process.Refresh()
    Note "Window title: '$($process.MainWindowTitle)' handle $($process.MainWindowHandle) responding $($process.Responding)"
    Note "Modules: $(($process.Modules | ForEach-Object { $_.ModuleName }) -join ', ')"
}
Get-WinEvent -FilterHashtable @{ LogName = 'Application'; Level = 1, 2; StartTime = (Get-Date).AddHours(-1) } -ErrorAction SilentlyContinue |
    Select-Object -First 15 | ForEach-Object { Note "EVENT $($_.TimeCreated) $($_.ProviderName): $($_.Message -replace '\s+', ' ')" }
if (-not $process.HasExited) { $process.Kill() }

Run 'unregister' 'powershell.exe' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$($package.FullName)\unregister-file-type.ps1`"")
Note "After unregister: $(Test-Path 'HKCU:\Software\Classes\.volund')"
Note 'DONE'
Stop-Computer -Force
