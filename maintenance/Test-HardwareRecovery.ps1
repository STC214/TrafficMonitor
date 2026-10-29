$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools are required' }
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if ($env:VSCMD_VER -eq $null) { throw 'Developer shell initialization failed' }
$output = Join-Path $PSScriptRoot 'evidence'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$exe = Join-Path $output 'Test-HardwareRecovery.exe'
$marker = "$exe.child-ok"
if (Test-Path -LiteralPath $marker) { Move-Item -LiteralPath $marker -Destination "$marker.previous" -Force }
& cl.exe /nologo /EHsc /std:c++17 /W4 "$PSScriptRoot\Test-HardwareRecovery.cpp" "/Fe:$exe" "/Fo:$output\Test-HardwareRecovery.obj" *> "$output\policy-compile.txt"
if ($LASTEXITCODE -ne 0) { Get-Content "$output\policy-compile.txt"; throw "Compile exit=$LASTEXITCODE" }
& $exe *> "$output\policy-tests.txt"
$result = $LASTEXITCODE
"ExitCode=$result" | Add-Content "$output\policy-tests.txt"
if ($result -ne 0) { throw "Tests exit=$result" }
$childLine = Get-Content -LiteralPath "$output\policy-tests.txt" | Where-Object { $_ -match '^ChildPID=(\d+)$' }
if (!$childLine -or $childLine -notmatch '^ChildPID=(\d+)$') { throw 'Child PID missing' }
$childProcess = [Diagnostics.Process]::GetProcessById([int]$Matches[1])
$childHandle = $childProcess.Handle # Retain a query handle before the child exits.
if ($childHandle -eq [IntPtr]::Zero) { throw 'Child process handle missing' }
if (!$childProcess.WaitForExit(20000)) { throw 'Replacement child timed out' }
if ($childProcess.ExitCode -ne 0) { throw "Replacement child exit=$($childProcess.ExitCode)" }
"ChildExitCode=$($childProcess.ExitCode)" | Add-Content "$output\policy-tests.txt"
for ($i=0; $i -lt 150 -and !(Test-Path -LiteralPath $marker); $i++) { Start-Sleep -Milliseconds 100 }
if (!(Test-Path -LiteralPath $marker)) { throw 'Replacement child did not finish' }
Get-Content -LiteralPath $marker | Add-Content "$output\policy-tests.txt"
Get-Content -LiteralPath "$output\policy-tests.txt"
