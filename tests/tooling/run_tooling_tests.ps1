# tests/tooling/run_tooling_tests.ps1
# FAZ 7 - Tooling Urunlestirme dogrulama paketi

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path "$ScriptDir\..\..").Path
Set-Location $RootDir

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "FAZ 7 - Tooling Urunlestirme Test Paketi" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$Stage3 = "$RootDir\build\bootstrap\stage3.exe"
$Yukinary = "$RootDir\yukinary.exe"

if (-not (Test-Path $Stage3)) {
    Write-Error "HATA: $Stage3 bulunamadi!"
    exit 1
}
if (-not (Test-Path $Yukinary)) {
    Write-Error "HATA: $Yukinary bulunamadi!"
    exit 1
}

Write-Host "[1/4] Formatter Idempotence Testi (F7.2)..." -ForegroundColor Yellow
python tests\formatter\01_idempotent.py $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[2/4] REPL Bridge Testi (F7.3)..." -ForegroundColor Yellow
python tests\repl\01_commands.py $Yukinary
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[3/4] Package Manager Testi (F7.4)..." -ForegroundColor Yellow
python tests\package\01_package_manager.py $Yukinary
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[4/4] LSP Protocol Testi (F7.1)..." -ForegroundColor Yellow
python tests\lsp\01_protocol.py $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "============================================================" -ForegroundColor Green
Write-Host "FAZ 7 TOOLING TESTLERI BASARIYLA TAMAMLANDI! PASS" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
