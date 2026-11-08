# ============================================================
# YknOS Canli Demo Calistirici (PowerShell)
# ============================================================

$RootPath = Resolve-Path "$PSScriptRoot\..\.."
$ExePath = Join-Path $PSScriptRoot "yknos.exe"

Push-Location $RootPath

try {
    if (-not (Test-Path $ExePath)) {
        Write-Host "[YknOS] Calistirilabilir dosya bulunamadi, derleniyor..." -ForegroundColor Yellow
        & "$PSScriptRoot\build.ps1"
    }

    Write-Host "`n[YknOS] Canli Isletim Sistemi Calisma Zamani Baslatiliyor...`n" -ForegroundColor Cyan
    & $ExePath
} finally {
    Pop-Location
}

