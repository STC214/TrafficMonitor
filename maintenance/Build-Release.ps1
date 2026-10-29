param([string]$ReleaseName = 'TrafficMonitor-1.87-win-x64-portable')
$ErrorActionPreference = 'Stop'
foreach ($script in @('Build-Recovery', 'Test-HardwareRecovery')) {
    & powershell.exe -NoProfile -File "$PSScriptRoot\$script.ps1"
    if ($LASTEXITCODE -ne 0) { throw "$script failed: exit=$LASTEXITCODE" }
}
$root = Split-Path $PSScriptRoot
$header = [IO.File]::ReadAllText("$root\TrafficMonitor\stdafx.h")
if ($header -notmatch '#define VERSION L"(\d+\.\d+(?:\.\d+)?)"') { throw 'Application version must contain only numeric components' }
$version = $Matches[1]
$expectedFileVersion = if (($version -split '\.').Count -eq 2) { "$version.0.0" } else { "$version.0" }
$fileVersion = (Get-Item -LiteralPath "$root\Bin\x64\Release\TrafficMonitor.exe").VersionInfo
if ($fileVersion.FileVersion -ne $expectedFileVersion -or $fileVersion.ProductVersion -ne $expectedFileVersion) {
    throw 'Application display version and EXE resource versions disagree'
}
$package = Join-Path $PSScriptRoot 'package'
New-Item -ItemType Directory -Path $package -Force | Out-Null
$manifest = foreach ($name in @('TrafficMonitor.exe', 'OpenHardwareMonitorApi.dll')) {
    Copy-Item -LiteralPath "$root\Bin\x64\Release\$name" -Destination (Join-Path $package $name) -Force
    [pscustomobject]@{Name=$name; Hash=(Get-FileHash -LiteralPath (Join-Path $package $name)).Hash}
}
$manifest | ConvertTo-Json | Out-File "$package\hashes.json" -Encoding utf8
& powershell.exe -NoProfile -File "$PSScriptRoot\Build-Portable.ps1" -ReleaseName $ReleaseName
if ($LASTEXITCODE -ne 0) { throw "Portable package failed: exit=$LASTEXITCODE" }
