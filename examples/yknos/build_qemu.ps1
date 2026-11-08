# ============================================================
# YknOS Bare-Metal QEMU Cekirdek Derleme Betigi (PowerShell)
# ============================================================

$NasmPath = "C:\Users\naitr\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\nasm.exe"

if (-not (Test-Path $NasmPath)) {
    Write-Error "[HATA] NASM derleyicisi bulunamadi: $NasmPath"
    exit 1
}

$AsmSource = Join-Path $PSScriptRoot "boot\yknos_baremetal.asm"
$BinTarget = Join-Path $PSScriptRoot "yknos_baremetal.bin"

Write-Host "[YknOS] Bare-metal Multiboot1 cekirdek derleniyor: $AsmSource" -ForegroundColor Cyan
& $NasmPath -f bin $AsmSource -o $BinTarget

if ($LASTEXITCODE -eq 0) {
    Write-Host "[YknOS] Bare-metal ikili basariyla uretildi: $BinTarget" -ForegroundColor Green
} else {
    Write-Error "[YknOS] Derleme basarisiz oldu! Kod: $LASTEXITCODE"
}

