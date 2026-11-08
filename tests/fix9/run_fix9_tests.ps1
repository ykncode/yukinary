# tests/fix9/run_fix9_tests.ps1
# FIX 9 - Stdlib ve OS Soyutlama Katmani Test Suiti
# Kullanim: .\tests\fix9\run_fix9_tests.ps1
# Calisma dizini: proje kokundan calistirilmalidir.

param(
    [string]$Compiler = "build\bootstrap\stage0.exe",
    [string]$Platform = "windows"
)

$ErrorActionPreference = "Stop"
$scriptkoku = Split-Path -Parent $MyInvocation.MyCommand.Path
$projekok = (Get-Item "$scriptkoku\..\..")

$gecti = 0
$kaldi = 0
$hatali = 0

$testler = @(
    @{ kaynak = "tests/fix9/test_bellek.ykn";        cikti_beklenen = "BELLEK TESTLERI TAMAM" }
    @{ kaynak = "tests/fix9/test_sonuc.ykn";          cikti_beklenen = "SONUC VE ENUM TESTLERI TAMAM" }
    @{ kaynak = "tests/fix9/test_os_soyutlama.ykn";   cikti_beklenen = "OS SOYUTLAMA TESTLERI TAMAM" }
    @{ kaynak = "tests/fix9/test_iterator.ykn";        cikti_beklenen = "ITERATOR TESTLERI TAMAM" }
)

Write-Host ""
Write-Host "=== FIX 9 Stdlib Test Suiti (Platform: $Platform) ===" -ForegroundColor Cyan
Write-Host ""

foreach ($test in $testler) {
    $ad = [System.IO.Path]::GetFileNameWithoutExtension($test.kaynak)
    $cikti_exe = "build\tmp\fix9_$ad.exe"

    # Derle
    $derleme = & $Compiler $test.kaynak $cikti_exe 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [DERLEME HATASI] $ad" -ForegroundColor Red
        Write-Host "    $derleme"
        $hatali++
        continue
    }

    # Calistir ve ciktisini yakala
    $gercek_cikti = (& $cikti_exe 2>&1) -join "`n"
    $exit_kodu = $LASTEXITCODE
    $beklenen = $test.cikti_beklenen

    if ($exit_kodu -ne 0) {
        Write-Host "  [BASARISIZ] $ad  (exit=$exit_kodu)" -ForegroundColor Red
        Write-Host "    Beklenen : $beklenen"
        Write-Host "    Gercek   : $gercek_cikti"
        $kaldi++
    } elseif ($gercek_cikti.Trim() -eq $beklenen.Trim()) {
        Write-Host "  [GECTI] $ad" -ForegroundColor Green
        $gecti++
    } else {
        Write-Host "  [CIKTI UYUMSUZ] $ad" -ForegroundColor Yellow
        Write-Host "    Beklenen : $beklenen"
        Write-Host "    Gercek   : $gercek_cikti"
        $kaldi++
    }
}

Write-Host ""
Write-Host "--- Sonuc: $gecti/$($testler.Count) gecti | $kaldi basarisiz | $hatali derleme hatasi ---"
Write-Host ""

if ($kaldi -gt 0 -or $hatali -gt 0) {
    exit 1
}
exit 0
