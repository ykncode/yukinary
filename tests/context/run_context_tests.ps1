# ============================================================
# run_context_tests.ps1
# CompilerContext mimarisi test calistiricisi
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "./app_full.exe"
)

$ErrorActionPreference = "Stop"
Write-Host "=== YUKINARY COMPILER CONTEXT TEST SUITE ===" -ForegroundColor Cyan

$testSrc = Join-Path $PSScriptRoot "test_compiler_context_isolation.ykn"
$testExe = Join-Path $PSScriptRoot "test_compiler_context_isolation.exe"

if (Test-Path $testExe) {
    Remove-Item $testExe -Force
}

Write-Host "1. Derleniyor: $testSrc -> $testExe" -ForegroundColor Yellow
& $Compiler $testSrc $testExe 2>&1 | Out-Null

if (-not (Test-Path $testExe)) {
    Write-Host "[FAIL] Derleme basarisiz oldu!" -ForegroundColor Red
    exit 1
}

Write-Host "2. Calistiriliyor: $testExe" -ForegroundColor Yellow
$output = & $testExe 2>&1
$output | ForEach-Object { Write-Host "   $_" }

if ($output -match "COMPILER_CONTEXT_ISOLATION_PASS") {
    Write-Host "=== SONUC: PASS (CompilerContext Tam Izolasyon Dogrulandi) ===" -ForegroundColor Green
    Remove-Item $testExe -Force
    exit 0
} else {
    Write-Host "=== SONUC: FAIL ===" -ForegroundColor Red
    exit 1
}
