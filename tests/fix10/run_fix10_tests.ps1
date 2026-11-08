# tests/fix10/run_fix10_tests.ps1
# FIX 10 - LSP, Formatter, REPL ve Paket Yoneticisi Test Suiti
# Kullanim: .\tests\fix10\run_fix10_tests.ps1
# Calisma dizini: proje koku

param(
    [string]$Compiler = "build\bootstrap\stage3.exe"
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $Compiler)) {
    if (Test-Path "build\yukinary.exe") {
        $Compiler = "build\yukinary.exe"
    } elseif (Test-Path "build\bootstrap\stage0.exe") {
        $Compiler = "build\bootstrap\stage0.exe"
    }
}
$gecti = 0
$kaldi = 0
$hatali = 0

$testler = @(
    @{ kaynak = "tests/fix10/test_repl.ykn";                    beklenen = "REPL TESTLERI TAMAM" }
    @{ kaynak = "tests/fix10/test_formatter_idempotence.ykn";   beklenen = "FORMATTER IDEMPOTENCE TAMAM" }
    @{ kaynak = "tests/fix10/test_paket.ykn";                   beklenen = "PAKET TESTLERI TAMAM" }
)

Write-Host ""
Write-Host "=== FIX 10 LSP/Formatter/REPL/Paket Test Suiti ===" -ForegroundColor Cyan
Write-Host ""

foreach ($test in $testler) {
    $ad = [System.IO.Path]::GetFileNameWithoutExtension($test.kaynak)
    $cikti_exe = "build\tmp\fix10_$ad.exe"

    $derleme = & $Compiler $test.kaynak $cikti_exe 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [DERLEME HATASI] $ad" -ForegroundColor Red
        $hatali++
        continue
    }

    $gercek = (& $cikti_exe 2>&1) | Where-Object { $_ -match $test.beklenen } | Select-Object -First 1
    $exit_k = $LASTEXITCODE

    if ($exit_k -ne 0) {
        Write-Host "  [BASARISIZ] $ad  (exit=$exit_k)" -ForegroundColor Red
        $kaldi++
    } elseif ($gercek) {
        Write-Host "  [GECTI] $ad" -ForegroundColor Green
        $gecti++
    } else {
        Write-Host "  [CIKTI UYUMSUZ] $ad" -ForegroundColor Yellow
        Write-Host "    Beklenen: $($test.beklenen)"
        $kaldi++
    }
}

# --- Formatter idempotence CLI roundtrip ---
Write-Host ""
Write-Host "  [ROUNDTRIP] Formatter idempotence (CLI)..." -ForegroundColor Cyan
$fmt1 = "build/tmp/fmt1.ykn"
$fmt2 = "build/tmp/fmt2.ykn"

New-Item -ItemType Directory -Force -Path "build/tmp" | Out-Null
@"
degisken x: tamsayi = 5;
isv topla(a: tamsayi, b: tamsayi) -> tamsayi {
    dondu a + b;
}
"@ | Set-Content $fmt1 -Encoding UTF8

# Canonical CLI is in-place: `fmt kaynak.ykn`, not `--fmt input output`.
& $Compiler fmt $fmt1 2>&1 | Out-Null
$fmt1_exit = $LASTEXITCODE
if ($fmt1_exit -ne 0) {
    Write-Host "  [BASARISIZ] Formatter CLI fmt (exit=$fmt1_exit)" -ForegroundColor Red
    $kaldi++
} else {
    Copy-Item $fmt1 $fmt2 -Force
    & $Compiler fmt $fmt2 2>&1 | Out-Null
    $fmt2_exit = $LASTEXITCODE
    if ($fmt2_exit -ne 0) {
        Write-Host "  [BASARISIZ] Formatter ikinci tur (exit=$fmt2_exit)" -ForegroundColor Red
        $kaldi++
    } else {
        $h1 = (Get-FileHash $fmt1 -Algorithm SHA256).Hash
        $h2 = (Get-FileHash $fmt2 -Algorithm SHA256).Hash
        if ($h1 -eq $h2) {
            & $Compiler fmt --check $fmt2 2>&1 | Out-Null
            $check_exit = $LASTEXITCODE
            if ($check_exit -eq 0) {
                Write-Host "  [GECTI] Formatter idempotence + fmt --check" -ForegroundColor Green
                $gecti++
            } else {
                Write-Host "  [BASARISIZ] Formatter --check (exit=$check_exit)" -ForegroundColor Red
                $kaldi++
            }
        } else {
            Write-Host "  [CIKTI UYUMSUZ] Formatter ikinci tur idempotent degil" -ForegroundColor Yellow
            $kaldi++
        }
    }
}

Write-Host ""
Write-Host "--- Sonuc: $gecti/$($testler.Count + 1) gecti | $kaldi basarisiz | $hatali derleme hatasi ---"
Write-Host ""

if ($kaldi -gt 0 -or $hatali -gt 0) { exit 1 }
exit 0
