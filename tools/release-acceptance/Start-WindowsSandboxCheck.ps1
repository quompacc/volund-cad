# L08: checks the Windows release ZIP in Windows Sandbox, a clean Windows without Visual Studio and
# without a developer PATH. Stages the ZIP, the UI flow tests and two format fixtures into a
# folder, starts the sandbox minimized and waits until windows-sandbox-check.ps1 wrote DONE.
# Nothing runs on the host desktop (user rule): the UI flows run inside the sandbox.
#
#   tools\release-acceptance\Start-WindowsSandboxCheck.ps1 -Stage <empty folder>
#
# Results: <Stage>\out\results.txt, smoke and ui-flow-scale-* pictures, desktop-product.png.
param(
    [Parameter(Mandatory = $true)][string]$Stage,
    [string]$Build = (Join-Path $PSScriptRoot '..\..\build\windows-msvc-release'),
    [int]$TimeoutMinutes = 30
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$zip = Get-ChildItem $Build -Filter 'voelund-cad-*-windows-x64.zip' | Select-Object -First 1
if ($null -eq $zip) { throw "No release ZIP in $Build (cmake --build --preset windows-msvc-release; cpack)" }
New-Item -ItemType Directory -Force $Stage, "$Stage\tests", "$Stage\fixtures" | Out-Null
if (Test-Path "$Stage\out") { throw "$Stage\out exists; use an empty stage folder" }
Copy-Item $zip.FullName $Stage
foreach ($test in 'voelund_reference_part_flow_tests', 'voelund_reference_ring_flow_tests', 'voelund_ui_flow_tests') {
    Copy-Item (Join-Path $Build "tests\$test.exe") "$Stage\tests\"
}
Copy-Item (Join-Path $root 'tests\fixtures\documents\v09.volund'), (Join-Path $root 'tests\fixtures\documents\v16.volund') "$Stage\fixtures\"
# Windows PowerShell 5.1 in the sandbox reads scripts without BOM as ANSI: write it with BOM.
$script = Get-Content (Join-Path $PSScriptRoot 'windows-sandbox-check.ps1') -Raw -Encoding utf8
[IO.File]::WriteAllText("$Stage\run.ps1", $script, (New-Object Text.UTF8Encoding $true))
$stageFull = (Resolve-Path $Stage).Path
@"
<Configuration>
  <VGpu>Enable</VGpu>
  <Networking>Disable</Networking>
  <ClipboardRedirection>Disable</ClipboardRedirection>
  <MemoryInMB>8192</MemoryInMB>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>$stageFull</HostFolder>
      <SandboxFolder>C:\l08</SandboxFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <LogonCommand>
    <Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\l08\run.ps1</Command>
  </LogonCommand>
</Configuration>
"@ | Set-Content "$Stage\check.wsb" -Encoding utf8
Start-Process -FilePath "$env:SystemRoot\System32\WindowsSandbox.exe" -ArgumentList "`"$stageFull\check.wsb`"" -WindowStyle Minimized
$results = "$Stage\out\results.txt"
$deadline = (Get-Date).AddMinutes($TimeoutMinutes)
while ((Get-Date) -lt $deadline) {
    if ((Test-Path $results) -and (Select-String -Path $results -Pattern '^DONE' -Quiet)) {
        Get-Content $results
        return
    }
    Start-Sleep -Seconds 15
}
throw "The sandbox did not finish within $TimeoutMinutes minutes; see $results"
