param([switch]$Baseline)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$output = Join-Path $PSScriptRoot 'evidence'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$workspace = $root
$label = 'modified'
if ($Baseline) {
    $label = 'baseline'
    $workspace = Join-Path $PSScriptRoot 'baseline-project'
    New-Item -ItemType Directory -Path $workspace -Force | Out-Null
    foreach ($item in @('TrafficMonitor', 'OpenHardwareMonitorApi', 'PluginDemo', 'include', 'TrafficMonitor.sln', 'LICENSE', 'LICENSE_CN')) {
        if (!(Test-Path -LiteralPath (Join-Path $workspace $item))) {
            Copy-Item -LiteralPath (Join-Path $root $item) -Destination $workspace -Recurse
        }
    }
    foreach ($name in @('TrafficMonitor.cpp', 'TrafficMonitorDlg.cpp', 'TrafficMonitorDlg.h', 'crashtool.cpp')) {
        Copy-Item -LiteralPath "$PSScriptRoot\baseline\$name" -Destination "$workspace\TrafficMonitor\$name"
    }
    Copy-Item -LiteralPath "$PSScriptRoot\baseline\OpenHardwareMonitorImp.cpp" -Destination "$workspace\OpenHardwareMonitorApi\OpenHardwareMonitorImp.cpp"
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if (!$vs) { throw 'Visual Studio with MSBuild is required' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$command = "_CL_=/utf-8 MSBuild.exe TrafficMonitor.sln /t:TrafficMonitor:Rebuild /p:Configuration=Release /p:Platform=x64 /m /nr:false /nologo /v:minimal"
"Directory=$workspace`nCommand=$command" | Out-File "$output\$label-command.txt" -Encoding utf8
Push-Location $workspace
$previousCl = $env:_CL_
try {
    $env:_CL_ = "$previousCl /utf-8"
    & $msbuild TrafficMonitor.sln /t:TrafficMonitor:Rebuild /p:Configuration=Release /p:Platform=x64 /m /nr:false /nologo /v:minimal *> "$output\$label-build.txt"
    $result = $LASTEXITCODE
    "ExitCode=$result" | Add-Content "$output\$label-build.txt"
    Get-Content "$output\$label-build.txt" -Tail 18
    exit $result
} finally { $env:_CL_ = $previousCl; Pop-Location }
