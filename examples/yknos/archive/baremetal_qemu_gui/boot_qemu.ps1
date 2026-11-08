# ============================================================
# YknOS Bare-Metal QEMU Sanal Makine Baslatici (PowerShell)
# ============================================================

$QemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$BinTarget = Join-Path $PSScriptRoot "yknos_baremetal.bin"

if (-not (Test-Path $BinTarget)) {
    Write-Host "[YknOS] Cekirdek ikilisi bulunamadi, derleniyor..." -ForegroundColor Yellow
    & "$PSScriptRoot\build_qemu.ps1"
}

if (-not (Test-Path $QemuPath)) {
    Write-Error "[HATA] QEMU yuklu degil: $QemuPath"
    exit 1
}

Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host "  YknOS QEMU Bare-Metal Sanal Makinesinde Baslatiliyor..." -ForegroundColor Cyan
Write-Host "  Mavi VGA 80x25 ekrani ayri bir pencerede acilacaktir." -ForegroundColor Green
Write-Host "  QEMU penceresine tiklayip klavyenizle komut yazabilirsiniz!" -ForegroundColor Yellow
Write-Host "  Seri port (COM1 0x3F8) gunlukleri bu terminale akacaktir." -ForegroundColor Cyan
Write-Host "============================================================`n" -ForegroundColor Cyan

& $QemuPath -kernel $BinTarget -serial stdio

