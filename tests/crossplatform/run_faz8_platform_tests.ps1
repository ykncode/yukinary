# tests/crossplatform/run_faz8_platform_tests.ps1
# FAZ 8  Gercek Platform ve Native Runtime Dogrulama Paketi

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path "$ScriptDir\..\..").Path
Set-Location $RootDir

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "FAZ 8  Gercek Platform ve Native Runtime Dogrulama Paketi" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$Stage3 = "$RootDir\build\bootstrap\stage3.exe"
if (-not (Test-Path $Stage3)) {
    Write-Error "HATA: $Stage3 bulunamadi!"
    exit 1
}

Write-Host "[1/6] Windows x86_64 ABI Regression Matrix..." -ForegroundColor Yellow
powershell -NoProfile -ExecutionPolicy Bypass -File tests\abi\run_abi_matrix.ps1
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[2/6] Linux ELF64 Cross-Platform Binary Suite..." -ForegroundColor Yellow
python tests\crossplatform\test_linux_elf.py --compiler $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[3/6] macOS Mach-O 64 Cross-Platform Binary Suite..." -ForegroundColor Yellow
python tests\crossplatform\test_macos_macho.py --compiler $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[4/6] ARM64 ELF / Mach-O Binary Suite..." -ForegroundColor Yellow
python tests\crossplatform\test_arm64_elf.py --compiler $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[5/6] ARM64 AAPCS Calling Convention Suite..." -ForegroundColor Yellow
powershell -NoProfile -ExecutionPolicy Bypass -File tests\crossplatform\test_arm64_aapcs.ps1 -Compiler $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[6/6] Release Runner Provenance Report Generation..." -ForegroundColor Yellow
powershell -NoProfile -ExecutionPolicy Bypass -File tools\generate_provenance.ps1
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "============================================================" -ForegroundColor Green
Write-Host "FAZ 8 PLATFORM DOGRULAMA TESTLERI BASARIYLA TAMAMLANDI! PASS" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
