$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vsDevCmd = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDevCmd)) { throw "VsDevCmd not found: $vsDevCmd" }
$outDir = Join-Path $root 'build\bootstrap'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$source = Join-Path $root 'app_full.cpp'
$output = Join-Path $outDir 'stage0-msvc.exe'
$line = 'call "' + $vsDevCmd + '" -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++17 /O2 /EHsc "' + $source + '" /Fe:"' + $output + '" /link ws2_32.lib'
$p = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/d', '/s', '/c', ('"' + $line + '"')) -WorkingDirectory $root -Wait -PassThru -NoNewWindow
if ($p.ExitCode -ne 0) { throw "MSVC bootstrap compile failed with exit code $($p.ExitCode)" }
if (-not (Test-Path -LiteralPath $output)) { throw 'MSVC did not create Stage-0 artifact' }
Get-FileHash -Algorithm SHA256 -LiteralPath $output | Format-List
Write-Output "STAGE0=$output"
