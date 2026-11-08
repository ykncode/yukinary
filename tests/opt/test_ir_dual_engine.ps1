# ============================================================
# tests/opt/test_ir_dual_engine.ps1
# FAZ 7.5: Cift Motorlu Kod Ureteci (Native Fast + Typed IR Emission) Dogrulama
# ============================================================

$ErrorActionPreference = "Stop"

$Compiler = "build\yukinary.exe"
if (-not (Test-Path -LiteralPath $Compiler)) {
    Write-Host "HATA: Compiler bulunamadi: $Compiler" -ForegroundColor Red
    exit 1
}

$workDir = "build\test-work\opt_ir"
if (-not (Test-Path -LiteralPath $workDir)) {
    New-Item -ItemType Directory -Path $workDir -Force | Out-Null
}

$testSrc = "tests\generics\01_identity.ykn"
$testExe = Join-Path $workDir "identity_native.exe"
$irDumpLog = Join-Path $workDir "identity_ir_dump.txt"

# 1. Native Fast Engine Testi
Write-Host "[1/3] Native Fast x86_64 Derleme Testi..." -ForegroundColor Cyan
& $Compiler $testSrc $testExe | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $testExe)) {
    Write-Host "FAIL: Native fast derleme basarisiz!" -ForegroundColor Red
    exit 1
}
$out1 = (& $testExe)
if ($out1 -notmatch "TEST 01 GENERICS: IDENTITY OK") {
    Write-Host "FAIL: Native calisma ciktisi hatali: $out1" -ForegroundColor Red
    exit 1
}
Write-Host "  PASS: Native Fast x86_64 calisti: $out1" -ForegroundColor Green

# 2. Typed IR E2E Lowering & Verifier Testi (--dump-ir)
Write-Host "[2/3] Typed IR Lowering ve Verifier Kontrolu (--dump-ir)..." -ForegroundColor Cyan
$irOut = & $Compiler --dump-ir $testSrc $testExe 2>&1
$irText = ($irOut -join "`n")
[System.IO.File]::WriteAllText($irDumpLog, $irText, [System.Text.Encoding]::UTF8)

if ($irText -notmatch "=== IR DUMP ===" -or $irText -notmatch "0 verifier hatasi") {
    Write-Host "FAIL: IR Lowering / Verifier basarisiz!" -ForegroundColor Red
    exit 1
}
Write-Host "  PASS: Typed IR Lowering ve 0 Verifier Hatasi dogrulandi." -ForegroundColor Green

# 3. Release Optimizasyon Modu Testi (--opt)
Write-Host "[3/3] Release Optimizasyon Modu Testi (-O / --opt)..." -ForegroundColor Cyan
$testOptExe = Join-Path $workDir "identity_opt.exe"
& $Compiler --opt $testSrc $testOptExe | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $testOptExe)) {
    Write-Host "FAIL: Release opt derleme basarisiz!" -ForegroundColor Red
    exit 1
}
$outOpt = (& $testOptExe)
if ($outOpt -notmatch "TEST 01 GENERICS: IDENTITY OK") {
    Write-Host "FAIL: Opt calisma ciktisi hatali: $outOpt" -ForegroundColor Red
    exit 1
}
Write-Host "  PASS: Release Opt calisti: $outOpt" -ForegroundColor Green

Write-Host "CIFT MOTORLU KOD URETECI (FAZ 7.5): BASARILI" -ForegroundColor Green
exit 0
