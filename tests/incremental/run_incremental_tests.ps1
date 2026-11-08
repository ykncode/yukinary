# ============================================================
# tests/incremental/run_incremental_tests.ps1
# FAZ 6: Incremental Compilation & Build System Test Runner
# Tests all FAZ 6 components across Stage-0 and Stage-3
# ============================================================

$ErrorActionPreference = "Stop"

$tests = @(
    @{ File = "tests/incremental/01_sha256_vectors.ykn"; Expected = "SHA256_NIST_VECTORS_PASS" },
    @{ File = "tests/incremental/02_fingerprint_units.ykn"; Expected = "FAZ6_FINGERPRINT_UNITS_PASS" },
    @{ File = "tests/incremental/03_cache_invalidation.ykn"; Expected = "FAZ6_CACHE_INVALIDATION_PASS" },
    @{ File = "tests/incremental/04_cache_corruption.ykn"; Expected = "FAZ6_CACHE_CORRUPTION_RECOVERY_PASS" },
    @{ File = "tests/incremental/05_clean_vs_incremental_parity.ykn"; Expected = "FAZ6_CLEAN_VS_INCREMENTAL_PARITY_PASS" }
)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  YUKINARY FAZ 6: INCREMENTAL BUILD SUITE (STAGE 0 & 3)   " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$passed = 0
$total = $tests.Count * 2

foreach ($t in $tests) {
    $f = $t.File
    $exp = $t.Expected
    $base = [System.IO.Path]::GetFileNameWithoutExtension($f)

    # 1. Stage-0
    $out0 = "build/temp_${base}_s0.exe"
    Write-Host "[Stage-0] Compiling $f..." -NoNewline
    Remove-Item .ykn_cache* -ErrorAction Ignore
    $compile0 = & .\build\bootstrap\stage0.exe $f $out0 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host " COMPILE FAIL" -ForegroundColor Red
        Write-Host $compile0
        exit 1
    }
    $run0 = & .\$out0 2>&1
    if ($run0 -match $exp) {
        Write-Host " PASS" -ForegroundColor Green
        $passed++
    } else {
        Write-Host " RUN FAIL (Expected '$exp')" -ForegroundColor Red
        Write-Host $run0
        exit 1
    }

    # 2. Stage-3
    $out3 = "build/temp_${base}_s3.exe"
    Write-Host "[Stage-3] Compiling $f..." -NoNewline
    Remove-Item .ykn_cache* -ErrorAction Ignore
    $compile3 = & .\build\bootstrap\stage3.exe $f $out3 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host " COMPILE FAIL" -ForegroundColor Red
        Write-Host $compile3
        exit 1
    }
    $run3 = & .\$out3 2>&1
    if ($run3 -match $exp) {
        Write-Host " PASS" -ForegroundColor Green
        $passed++
    } else {
        Write-Host " RUN FAIL (Expected '$exp')" -ForegroundColor Red
        Write-Host $run3
        exit 1
    }
}

Remove-Item .ykn_cache* -ErrorAction Ignore
Remove-Item build/temp_*_s*.exe -ErrorAction Ignore

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  FAZ 6 RESULTS: $passed / $total PASSED (100% PARITY)   " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
