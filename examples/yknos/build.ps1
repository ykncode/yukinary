# ============================================================
# YknOS Derleme Betigi (PowerShell)
# ============================================================

$RootPath = Resolve-Path "$PSScriptRoot\..\.."
Push-Location $RootPath

try {
    Write-Host "[YknOS] Yukinary derleyicisi ile derleniyor..." -ForegroundColor Cyan
    & ".\ykn.exe" "examples\yknos\yknos_main.ykn" "examples\yknos\yknos.exe"

    if ($LASTEXITCODE -eq 0) {
        Write-Host "[YknOS] Derleme basariyla tamamlandi: examples\yknos\yknos.exe" -ForegroundColor Green
    } else {
        Write-Error "[YknOS] Derleme hatasi! Cikis kodu: $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

