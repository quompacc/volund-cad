function Get-FixtureRuntimeArtifacts([string]$Work) {
    foreach ($app in 'voelund-cad','voelund-ui-preview') {
        foreach ($suffix in '.json','-widgets.png','-framebuffer.png') {
            New-ReportArtifact (Join-Path $Work "captures/$app$suffix") $Work
        }
    }
}

function New-PlatformReportFixture([string]$Work) {
    $captures = Join-Path $Work 'captures'
    New-Item -ItemType Directory -Path $captures | Out-Null
    $runtime = @{ backend = 'qt-opengl'; qpa = 'windows'; qt = '6.11.1'; occt = '8.0.1';
        gl = 'fixture'; vendor = 'fixture'; renderer = 'fixture'; os = 'fixture'; dpi = 96; dpr = 1; width = 1; height = 1 }
    $png = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Wl6VAAAAABJRU5ErkJggg==')
    foreach ($app in 'voelund-cad','voelund-ui-preview') {
        $runtime | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $captures "$app.json")
        foreach ($suffix in 'widgets','framebuffer') { [IO.File]::WriteAllBytes((Join-Path $captures "$app-$suffix.png"), $png) }
    }
    'fixture source' | Set-Content -LiteralPath (Join-Path $Work 'source.cpp')
    $sourceHash = Get-ReportHash (Join-Path $Work 'source.cpp')
    $canonical = 'src/source.cpp:' + $sourceHash
    $digest = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($canonical))).ToLowerInvariant()
    @{ files = @(@{ path = 'src/source.cpp'; sha256 = $sourceHash }); scope = @('src'); digest = $digest } |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Work 'source.json')
    $names = @('desktop.viewport_startup','desktop.ui_preview_startup','desktop.viewport_startup.uninitialized',
        'desktop.ui_preview_startup.uninitialized','architecture.platform_report','architecture.source_limits')
    $xml = '<testsuite name="Win32-ninja" tests="6" failures="0" disabled="0">' + (($names | ForEach-Object { '<testcase name="' + $_ + '" status="run" />' }) -join '') + '</testsuite>'
    $xml | Set-Content -LiteralPath (Join-Path $Work 'ctest.xml')
    'fixture log' | Set-Content -LiteralPath (Join-Path $Work 'run.log')
    $log = New-ReportArtifact (Join-Path $Work 'run.log') $Work
    $junit = New-ReportArtifact (Join-Path $Work 'ctest.xml') $Work
    'fixture build' | Set-Content -LiteralPath (Join-Path $Work 'build.log')
    'VOELUND_VIEWPORT_BACKEND:STRING=qt-opengl' | Set-Content -LiteralPath (Join-Path $Work 'CMakeCache.txt')
    @('voelund-cad.exe','voelund-ui-preview.exe','voelund_kernel_tests.exe' | ForEach-Object {
        @{ path = $_; sha256 = 'a' * 64 }
    }) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Work 'binaries.json')
    @{
        schemaVersion = 2; runId = 'fixture'; generatedAtUtc = '2026-09-13T00:00:00Z'
        source = @{ commit = 'a' * 40; dirty = $true; before = $digest; after = $digest; snapshot = New-ReportArtifact (Join-Path $Work 'source.json') $Work }
        configuration = @{ id = 'windows-qt-opengl'; platform = 'Windows'; backend = 'qt-opengl'; qpa = 'windows';
            osVersion = 'fixture'; buildPreset = 'windows-msvc'; compiler = @{ path = 'fixture'; version = 'fixture' };
            compositorReason = 'fixture unavailable'; displayScope = 'fixture pixels' }
        packageStatus = @{ P01 = 'review'; P02 = 'planned'; P03 = 'planned'; P04 = 'planned' }
        matrix = @(New-PlatformMatrix 'windows-qt-opengl' 'passed' $null)
        results = @(
            @{ id = 'build'; configurationId = 'windows-qt-opengl'; status = 'passed'; command = 'fixture build'; exitCode = 0; reason = $null; artifacts = @($log) },
            @{ id = 'ctest'; configurationId = 'windows-qt-opengl'; status = 'passed'; command = 'fixture ctest'; exitCode = 0; reason = $null; artifacts = @($log,$junit); junit = $junit })
        buildEvidence = @('build.log','CMakeCache.txt','binaries.json' | ForEach-Object { New-ReportArtifact (Join-Path $Work $_) $Work })
        runtime = @{ status = 'passed'; directory = 'captures'; reason = $null; applications = Read-ReportRuntime $captures 'qt-opengl'; artifacts = @(Get-FixtureRuntimeArtifacts $Work) }
        capture = @{ status = 'captured-not-reviewed'; method = 'synthetic fixture PNGs' }
    }
}
