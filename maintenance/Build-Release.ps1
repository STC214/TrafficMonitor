param([string]$ReleaseName = 'TrafficMonitor-1.86-recovery.3-win-x64-portable')
$ErrorActionPreference = 'Stop'
foreach ($script in @('Build-Recovery', 'Test-HardwareRecovery')) {
    & powershell.exe -NoProfile -File "$PSScriptRoot\$script.ps1"
    if ($LASTEXITCODE -ne 0) { throw "$script failed: exit=$LASTEXITCODE" }
}
$root = Split-Path $PSScriptRoot
$package = Join-Path $PSScriptRoot 'package'
New-Item -ItemType Directory -Path $package -Force | Out-Null
$manifest = foreach ($name in @('TrafficMonitor.exe', 'OpenHardwareMonitorApi.dll')) {
    Copy-Item -LiteralPath "$root\Bin\x64\Release\$name" -Destination (Join-Path $package $name) -Force
    [pscustomobject]@{Name=$name; Hash=(Get-FileHash -LiteralPath (Join-Path $package $name)).Hash}
}
$manifest | ConvertTo-Json | Out-File "$package\hashes.json" -Encoding utf8
& powershell.exe -NoProfile -File "$PSScriptRoot\Build-Portable.ps1" -ReleaseName $ReleaseName
if ($LASTEXITCODE -ne 0) { throw "Portable package failed: exit=$LASTEXITCODE" }
