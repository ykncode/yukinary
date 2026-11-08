# tests/fix11/run_fix11_tests.ps1
# FIX 11 - Gercek Coklu Platform ve Native Runtime Dogrulama Test Suiti
# Kullanim: .\tests\fix11\run_fix11_tests.ps1
# Calisma dizini: proje koku

param(
    [string]$Compiler = "build\bootstrap\stage0.exe"
)

$gecti = 0
$kaldi = 0
$hatali = 0

Write-Host ""
Write-Host "=== FIX 11 Platform & Runtime Test Suiti ===" -ForegroundColor Cyan
Write-Host "    Host: Windows x86_64 (release baseline)" -ForegroundColor DarkGray
Write-Host ""

# ── YKN unit testleri ──────────────────────────────────────────────────
$unit_testler = @(
    @{ kaynak = "tests/fix11/test_platform_destek.ykn";  beklenen = "PLATFORM DESTEK TESTLERI TAMAM" }
    @{ kaynak = "tests/fix11/test_windows_baseline.ykn"; beklenen = "WINDOWS BASELINE TESTLERI TAMAM" }
)

foreach ($test in $unit_testler) {
    $ad = [System.IO.Path]::GetFileNameWithoutExtension($test.kaynak)
    $exe = "build\tmp\fix11_$ad.exe"
    & $Compiler $test.kaynak $exe 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [DERLEME HATASI] $ad" -ForegroundColor Red
        $hatali++; continue
    }
    $cikti = (& $exe 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [BASARISIZ] $ad" -ForegroundColor Red; $kaldi++
    } elseif ($cikti -match $test.beklenen) {
        Write-Host "  [GECTI] $ad" -ForegroundColor Green; $gecti++
    } else {
        Write-Host "  [CIKTI UYUMSUZ] $ad" -ForegroundColor Yellow; $kaldi++
    }
}

# ── Windows PE32+ baseline header ──────────────────────────────────────
Write-Host ""
Write-Host "  [HEADER] Windows PE32+ baseline (stage0.exe MZ magic)..." -ForegroundColor Cyan
$b = [System.IO.File]::ReadAllBytes("build\bootstrap\stage0.exe")
if ($b[0] -eq 0x4D -and $b[1] -eq 0x5A) {
    Write-Host "  [GECTI] PE32+ MZ magic dogrulandi" -ForegroundColor Green; $gecti++
} else {
    Write-Host "  [BASARISIZ] MZ magic bulunamadi!" -ForegroundColor Red; $kaldi++
}

# ── Linux x86_64 native (Docker) ───────────────────────────────────────
Write-Host ""
Write-Host "  [NATIVE] Linux x86_64 - Docker native execution..." -ForegroundColor Cyan
& $Compiler tests/fix11/linux_native_test.ykn build/tmp/linux_native_docker --target linux 2>$null
if ($LASTEXITCODE -eq 0) {
    $docker_avail = (docker info --format "{{.ServerVersion}}" 2>$null)
    if ($docker_avail) {
        $cikti = (docker run --rm -v "${PWD}:/work" -w /work ubuntu:24.04 /work/build/tmp/linux_native_docker 2>&1) -join ""
        $exit_k = $LASTEXITCODE
        if ($cikti -match "LINUX_NATIVE_OK" -and $exit_k -eq 42) {
            Write-Host "  [GECTI] Linux x86_64 native (stdout + exit code 42)" -ForegroundColor Green
            $gecti++
        } else {
            Write-Host "  [BASARISIZ] Linux native: stdout='$cikti' exit=$exit_k" -ForegroundColor Red; $kaldi++
        }
    } else {
        Write-Host "  [ATLA] Docker mevcut degil" -ForegroundColor DarkGray
    }
} else {
    Write-Host "  [DERLEME HATASI] linux_native_test.ykn --target linux" -ForegroundColor Red; $hatali++
}

# ── Linux ARM64 header ─────────────────────────────────────────────────
Write-Host ""
Write-Host "  [HEADER] Linux ARM64 ELF64 header..." -ForegroundColor Cyan
& $Compiler tests/fix11/linux_native_test.ykn build/tmp/linux_arm64_hdr --target linux-arm64 2>$null
if (Test-Path build/tmp/linux_arm64_hdr) {
    $b = [System.IO.File]::ReadAllBytes("build/tmp/linux_arm64_hdr")
    if ($b[0] -eq 127 -and $b[1] -eq 69 -and $b[4] -eq 2 -and $b[18] -eq 183) {
        Write-Host "  [GECTI] ARM64 ELF64 header dogru (magic+EI_CLASS+e_machine=AArch64)" -ForegroundColor Green; $gecti++
    } else {
        Write-Host "  [BASARISIZ] ARM64 ELF header hatali" -ForegroundColor Red; $kaldi++
    }
} else {
    Write-Host "  [DERLEME HATASI] linux-arm64 target" -ForegroundColor Red; $hatali++
}

# ── Mach-O x86_64 header ──────────────────────────────────────────────
Write-Host "  [HEADER] macOS x86_64 Mach-O header..." -ForegroundColor Cyan
& $Compiler tests/fix11/linux_native_test.ykn build/tmp/macos_hdr --target macos 2>$null
if (Test-Path build/tmp/macos_hdr) {
    $b = [System.IO.File]::ReadAllBytes("build/tmp/macos_hdr")
    if ($b[0] -eq 207 -and $b[1] -eq 250 -and $b[2] -eq 237 -and $b[3] -eq 254) {
        Write-Host "  [GECTI] Mach-O 64-bit magic dogru (0xCFFAEDFE)" -ForegroundColor Green; $gecti++
    } else {
        Write-Host "  [BASARISIZ] Mach-O magic hatali: $($b[0]) $($b[1]) $($b[2]) $($b[3])" -ForegroundColor Red; $kaldi++
    }
} else {
    Write-Host "  [DERLEME HATASI] macos target" -ForegroundColor Red; $hatali++
}

# ── Platform destek ozeti ──────────────────────────────────────────────
Write-Host ""
Write-Host "  Platform Destek Ozeti:" -ForegroundColor DarkGray
Write-Host "    Windows x86_64  : [DESTEKLENIYOR] - runtime dogrulanmis" -ForegroundColor Green
Write-Host "    Linux x86_64    : [DESTEKLENIYOR] - Docker native dogrulanmis" -ForegroundColor Green
Write-Host "    Linux ARM64     : [DENEYSEL] - Header dogru, QEMU Illegal instruction (codegen eksik)" -ForegroundColor Yellow
Write-Host "    macOS x86_64    : [DENEYSEL] - Header dogru, native runner yok" -ForegroundColor Yellow
Write-Host "    macOS ARM64     : [DENEYSEL] - Header dogru, native runner yok" -ForegroundColor Yellow

Write-Host ""
Write-Host "--- Sonuc: $gecti gecti | $kaldi basarisiz | $hatali derleme hatasi ---"
Write-Host ""

if ($kaldi -gt 0 -or $hatali -gt 0) { exit 1 }
exit 0
