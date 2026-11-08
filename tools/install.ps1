# ==============================================================================
# Yukinary Global CLI Installer (Windows)
# ==============================================================================
# This script installs 'ykn' to %USERPROFILE%\.yukinary\bin and adds it to the
# User PATH environment variable. After running, you can simply type:
#   ykn --help
#   ykn convert math.py
#   ykn main.ykn
# from ANY terminal without needing 'ykn.exe' or '.\'.
# ==============================================================================

$ErrorActionPreference = "Stop"

$installDir = Join-Path $env:USERPROFILE ".yukinary\bin"
$repoRoot = Split-Path -Parent $PSScriptRoot

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "Installing Yukinary CLI to: $installDir" -ForegroundColor Green
Write-Host "========================================================" -ForegroundColor Cyan

if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
}

# 1. Copy ykn.exe binary
$srcExe = Join-Path $repoRoot "ykn.exe"
if (-not (Test-Path $srcExe)) {
    $srcExe = Join-Path $repoRoot "app_full.exe"
}

if (Test-Path $srcExe) {
    Copy-Item $srcExe (Join-Path $installDir "ykn.exe") -Force
    Write-Host "[OK] Copied ykn.exe binary." -ForegroundColor Green
} else {
    Write-Host "[ERROR] Could not find ykn.exe in repository root." -ForegroundColor Red
    exit 1
}

# 1b. Copy ykn_llm_simd.dll
$srcDll = Join-Path $repoRoot "ykn_llm_simd.dll"
if (Test-Path $srcDll) {
    Copy-Item $srcDll (Join-Path $installDir "ykn_llm_simd.dll") -Force
    Write-Host "[OK] Copied ykn_llm_simd.dll." -ForegroundColor Green
}

# 2. Create ykn.cmd shim for Command Prompt
$cmdShim = "@echo off`r`n`"%~dp0ykn.exe`" %*"
Set-Content -Path (Join-Path $installDir "ykn.cmd") -Value $cmdShim -Encoding ASCII
Write-Host "[OK] Created ykn.cmd shim." -ForegroundColor Green

# 3. Copy standard library (std/) alongside the compiler
$srcStd = Join-Path $repoRoot "std"
$targetStd = Join-Path $env:USERPROFILE ".yukinary\std"
if (Test-Path $srcStd) {
    Copy-Item $srcStd $targetStd -Recurse -Force
    Write-Host "[OK] Installed Yukinary Standard Library (std/)." -ForegroundColor Green
}

# 4. Patch User PATH environment variable (Prepended to avoid Windows 2048-char path truncation)
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathParts = $currentPath -split ";" | Where-Object { $_ -ne "" -and $_ -notlike "*yukinary*" }

$newPath = "$installDir;" + ($pathParts -join ";")
[Environment]::SetEnvironmentVariable("Path", $newPath, "User")
$env:Path = "$installDir;$env:Path"
Write-Host "[SUCCESS] Prepended $installDir to User PATH permanently!" -ForegroundColor Green

# 5. Also copy to WindowsApps as a bulletproof zero-path fallback
$winApps = Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps"
if (Test-Path $winApps) {
    Copy-Item (Join-Path $installDir "ykn.exe") (Join-Path $winApps "ykn.exe") -Force -ErrorAction SilentlyContinue
    Copy-Item (Join-Path $installDir "ykn.cmd") (Join-Path $winApps "ykn.cmd") -Force -ErrorAction SilentlyContinue
    if (Test-Path (Join-Path $installDir "ykn_llm_simd.dll")) {
        Copy-Item (Join-Path $installDir "ykn_llm_simd.dll") (Join-Path $winApps "ykn_llm_simd.dll") -Force -ErrorAction SilentlyContinue
    }
    Write-Host "[OK] Synced ykn to WindowsApps for instant execution." -ForegroundColor Green
}

Write-Host "`nInstallation complete! You can now run 'ykn' from any terminal:" -ForegroundColor Cyan
Write-Host "  ykn --yardim" -ForegroundColor White
Write-Host "  ykn convert test.py -o test.ykn" -ForegroundColor White
Write-Host "  ykn main.ykn" -ForegroundColor White

