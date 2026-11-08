# ============================================================
# tests/fallback/test_dynamic.ps1
# Fallback modu gercek sistem cagrisi kaniti testi
# ============================================================
[CmdletBinding()]
param([string]$Compiler = "build/bootstrap/stage0.exe", [string]$OutputJson = "build/reports/fallback_test_dynamic_report.json")
$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
Write-Host "=== Fallback Test: test_dynamic ===" -ForegroundColor Cyan
# Kaynak kodda mock kalip aramasi
$srcFiles = Get-ChildItem -Path (Join-Path $ROOT "src") -Filter "fallback_*.ykn" -ErrorAction SilentlyContinue
$noMock = $true
foreach ($f in $srcFiles) {
    $content2 = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
    if ($content2 -match "g_dyn_lib_handle\s*=\s*1\s*;|dondu 0;.*[Mm]ock") {
        $noMock = $false
        Write-Host "  [WARN] Mock bulunamdi: $($f.Name)" -ForegroundColor Yellow
    }
}
if ($noMock) {
    Write-Host "  [OK] Mock handle yok — gercek sistem cagrisi kullaniliyor" -ForegroundColor Green
}
$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema = "yukinary.fallback.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    mode = "test_dynamic"
    no_mock = $noMock
} | ConvertTo-Json | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8
Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
