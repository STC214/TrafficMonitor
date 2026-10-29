param([string]$ReleaseName = 'TrafficMonitor-1.87-win-x64-portable', [switch]$VerifyExisting)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
if ($ReleaseName -notmatch '^[A-Za-z0-9._-]+$') { throw 'Release name must be a file name, not a path' }
$release = Join-Path $root "releases\$ReleaseName"
$zip = "$release.zip"
if (!$VerifyExisting -and ((Test-Path -LiteralPath $release) -or (Test-Path -LiteralPath $zip))) { throw 'Release already exists; choose a new release name' }
$app = Join-Path $release 'TrafficMonitor'
if (!$VerifyExisting) {
New-Item -ItemType Directory -Path $app -Force | Out-Null
foreach ($name in @('TrafficMonitor.exe', 'OpenHardwareMonitorApi.dll')) {
    Copy-Item -LiteralPath "$PSScriptRoot\package\$name" -Destination $app
}
Copy-Item -LiteralPath "$root\OpenHardwareMonitorApi\LibreHardwareMonitorLib.dll" -Destination $app
foreach ($name in @('skins', 'language')) {
    Copy-Item -LiteralPath "$root\TrafficMonitor\$name" -Destination $app -Recurse
}
foreach ($name in @('LICENSE', 'LICENSE_CN')) { Copy-Item -LiteralPath (Join-Path $root $name) -Destination $app }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools are required' }
$redistVersion = Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Redist\MSVC') -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!$redistVersion) { throw 'Visual C++ redistributable files are required' }
$redist = Join-Path $redistVersion.FullName 'x64'
foreach ($folder in @('Microsoft.VC143.CRT', 'Microsoft.VC143.MFC')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $redist $folder) -Filter '*.dll') {
        Copy-Item -LiteralPath $file.FullName -Destination $app
    }
}
[IO.File]::WriteAllText("$app\global_cfg.ini", "[config]`r`nportable_mode = true`r`n", [Text.UTF8Encoding]::new($true))
[IO.File]::WriteAllText("$app\config.ini", "[general]`r`ncheck_update_when_start = false`r`nmonitor_time_span = 1000`r`n[config]`r`nshow_notify_icon = true`r`nhide_main_window = false`r`nshow_task_bar_wnd = true`r`n[connection]`r`nauto_select = true`r`nselect_all = false`r`n", [Text.UTF8Encoding]::new($true))
$readme = @'
TrafficMonitor 1.87 - Windows x64 portable

Extract the entire TrafficMonitor folder to a writable local directory.
Run TrafficMonitor.exe and approve its normal administrator request.
Windows 10/11 x64 and .NET Framework 4.7.2 or newer are required.
The native Visual C++/MFC runtime DLLs are bundled app-locally.

This is a clean portable package: no personal logs, IP addresses, saved
adapter selections or traffic history are included. Config/history are
stored beside the executable (portable_mode=true). Do not extract over
an existing installation unless its configuration/history are backed up.

Driver-related acquisition failures: restart after 3 consecutive failures;
wait for clean acquisition-thread shutdown and parent exit, then 5 seconds;
limit 3 restarts per 10-minute window, retry after the window expires.
Native/managed error details are recorded in error.log. A permanently hung
hardware API call is not treated as a returned acquisition error.

Original project license: LICENSE / LICENSE_CN.
This build includes the upstream LibreHardwareMonitorLib.dll unchanged.

Taskbar fixes: exact left edge when widget avoidance is disabled, refreshed
layout after setting/widget changes, correct secondary-monitor coordinates,
safe missing-window fallbacks and retries after failed window moves.
'@
[IO.File]::WriteAllText("$app\PORTABLE-README.txt", $readme, [Text.UTF8Encoding]::new($true))
$manifest = @(foreach ($file in Get-ChildItem -LiteralPath $app -File -Recurse) {
    $relative = $file.FullName.Substring($app.Length + 1).Replace('\', '/')
    [pscustomobject]@{Path=$relative; SHA256=(Get-FileHash -LiteralPath $file.FullName).Hash; Length=$file.Length}
})
$manifest | ConvertTo-Json -Depth 3 | Out-File "$app\SHA256-MANIFEST.json" -Encoding utf8
} else {
    $manifest = Get-Content -LiteralPath "$app\SHA256-MANIFEST.json" -Raw | ConvertFrom-Json
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (!(Test-Path -LiteralPath $zip)) {
$writer = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem -LiteralPath $app -File -Recurse) {
        $relative = $file.FullName.Substring($app.Length + 1).Replace('\', '/')
        $item = $writer.CreateEntry('TrafficMonitor/' + $relative, [IO.Compression.CompressionLevel]::Optimal)
        $input = [IO.File]::OpenRead($file.FullName)
        $output = $item.Open()
        try { $input.CopyTo($output) } finally { $input.Dispose(); $output.Dispose() }
    }
} finally { $writer.Dispose() }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $files = @($archive.Entries | Where-Object { $_.Name.Length -gt 0 })
    if ($files.Count -ne ($manifest.Count + 1)) {
        $missing = @($manifest | Where-Object { !$archive.GetEntry('TrafficMonitor/' + $_.Path) })
        throw "Archive entry count mismatch: files=$($files.Count), manifest=$($manifest.Count); missing=$($missing.Path -join ',')"
    }
    foreach ($entry in $manifest) {
        $item = $archive.GetEntry('TrafficMonitor/' + $entry.Path)
        if (!$item) { throw "Missing ZIP entry: $($entry.Path)" }
        $stream = $item.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
        finally { $stream.Dispose(); $sha.Dispose() }
        if ($hash -ne $entry.SHA256 -or $item.Length -ne $entry.Length) { throw "ZIP hash mismatch: $($entry.Path)" }
    }
    if ($files.FullName -match '(^|/)(error\.log|history_traffic\.dat|TrafficMonitor\.sys)$') { throw 'Personal/runtime state in ZIP' }
} finally { $archive.Dispose() }
$extracted = "$release-verify-$(Get-Date -Format 'yyyyMMdd-HHmmss-fff')"
Expand-Archive -LiteralPath $zip -DestinationPath $extracted
foreach ($entry in $manifest) {
    if ((Get-FileHash -LiteralPath (Join-Path "$extracted\TrafficMonitor" $entry.Path)).Hash -ne $entry.SHA256) {
        throw "Extracted hash mismatch: $($entry.Path)"
    }
}
$hash = (Get-FileHash -LiteralPath $zip).Hash
"$hash  $([IO.Path]::GetFileName($zip))" | Out-File "$zip.sha256" -Encoding ascii
[pscustomobject]@{Zip=$zip; SHA256=$hash; Entries=$manifest.Count+1; Extracted="$extracted\TrafficMonitor"} |
    ConvertTo-Json | Out-File "$PSScriptRoot\evidence\portable-verification.json" -Encoding utf8
Write-Output "PASS: ZIP entries and extracted files hash-verified; $($manifest.Count + 1) files; $zip"
