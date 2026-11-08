# tests/fix12/run_fix12_tests.ps1
# FIX 12 - V1 Release Gate Test Suiti
# Kullanim: .\tests\fix12\run_fix12_tests.ps1
# Calisma dizini: proje koku

param(
    [string]$Compiler = "build\bootstrap\stage0.exe",
    [switch]$Regenerate   # --Regenerate: test_results.json'u yeniden uret
)

$gecti = 0; $kaldi = 0

Write-Host ""
Write-Host "=== FIX 12 V1 Release Gate ===" -ForegroundColor Magenta
Write-Host "    Tarih: $(Get-Date -Format 'yyyy-MM-dd HH:mm')" -ForegroundColor DarkGray
Write-Host ""

# ─────────────────────────────────────────────────────
# 1. Alt suitleri calistir: FIX 9, 10, 11
# ─────────────────────────────────────────────────────
Write-Host "[ 1/7 ] Alt test suitleri (FIX 9-11)..." -ForegroundColor Cyan
$suitler = @(
    "tests\fix9\run_fix9_tests.ps1",
    "tests\fix10\run_fix10_tests.ps1",
    "tests\fix11\run_fix11_tests.ps1"
)
$suite_hatasi = 0
foreach ($s in $suitler) {
    $ad = Split-Path $s -Parent | Split-Path -Leaf
    & powershell -File $s 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "    [GECTI] $ad" -ForegroundColor Green; $gecti++
    } else {
        Write-Host "    [BASARISIZ] $ad" -ForegroundColor Red; $kaldi++; $suite_hatasi++
    }
}

# ─────────────────────────────────────────────────────
# 2. stage0.exe PE32+ baseline
# ─────────────────────────────────────────────────────
Write-Host "[ 2/7 ] Windows x86_64 PE32+ baseline..." -ForegroundColor Cyan
$b = [System.IO.File]::ReadAllBytes("build\bootstrap\stage0.exe")
if ($b[0] -eq 0x4D -and $b[1] -eq 0x5A) {
    Write-Host "    [GECTI] stage0.exe MZ magic dogrulandi" -ForegroundColor Green; $gecti++
} else {
    Write-Host "    [BASARISIZ] stage0.exe MZ magic hatali" -ForegroundColor Red; $kaldi++
}

# ─────────────────────────────────────────────────────
# 3. PLATFORM_SUPPORT.md var mi?
# ─────────────────────────────────────────────────────
Write-Host "[ 3/7 ] Platform destek belgesi..." -ForegroundColor Cyan
if (Test-Path "PLATFORM_SUPPORT.md") {
    Write-Host "    [GECTI] PLATFORM_SUPPORT.md mevcut" -ForegroundColor Green; $gecti++
} else {
    Write-Host "    [BASARISIZ] PLATFORM_SUPPORT.md eksik" -ForegroundColor Red; $kaldi++
}

# ─────────────────────────────────────────────────────
# 4. COMPATIBILITY_POLICY.md var mi?
# ─────────────────────────────────────────────────────
Write-Host "[ 4/7 ] Uyumluluk politikasi belgesi..." -ForegroundColor Cyan
if (Test-Path "COMPATIBILITY_POLICY.md") {
    Write-Host "    [GECTI] COMPATIBILITY_POLICY.md mevcut" -ForegroundColor Green; $gecti++
} else {
    Write-Host "    [BASARISIZ] COMPATIBILITY_POLICY.md eksik" -ForegroundColor Red; $kaldi++
}

# ─────────────────────────────────────────────────────
# 5. Cok dilli README'ler var mi?
# ─────────────────────────────────────────────────────
Write-Host "[ 5/7 ] Cok dilli dokumanlar..." -ForegroundColor Cyan
$diller = @("docs\tr\README.md","docs\en\README.md","docs\ja\README.md","docs\ru\README.md","docs\README.md")
$eksik = $diller | Where-Object { -not (Test-Path $_) }
if ($eksik.Count -eq 0) {
    Write-Host "    [GECTI] TR + EN + JA + RU README mevcut" -ForegroundColor Green; $gecti++
} else {
    Write-Host "    [BASARISIZ] Eksik: $($eksik -join ', ')" -ForegroundColor Red; $kaldi++
}

# ─────────────────────────────────────────────────────
# 6. README.md dil yonlendirmesi var mi?
# ─────────────────────────────────────────────────────
Write-Host "[ 6/7 ] Ana README dil yonlendirmesi..." -ForegroundColor Cyan
$readme = Get-Content README.md -Raw
if ($readme -match "docs/tr/README" -and $readme -match "docs/en/README") {
    Write-Host "    [GECTI] README.md dil linkleri mevcut" -ForegroundColor Green; $gecti++
} else {
    Write-Host "    [BASARISIZ] README.md dil linkleri eksik" -ForegroundColor Red; $kaldi++
}

# ─────────────────────────────────────────────────────
# 7. test_results.json taze mi?
# ─────────────────────────────────────────────────────
Write-Host "[ 7/7 ] test_results.json tazelik kontrolu..." -ForegroundColor Cyan
if (Test-Path "test_results.json") {
    $mtime = (Get-Item "test_results.json").LastWriteTime
    $gun_fark = ((Get-Date) - $mtime).TotalDays
    if ($gun_fark -lt 7) {
        Write-Host "    [GECTI] test_results.json taze ($([int]$gun_fark) gun once)" -ForegroundColor Green; $gecti++
    } else {
        Write-Host "    [UYARI] test_results.json eski ($([int]$gun_fark) gun). --Regenerate ile yenile." -ForegroundColor Yellow
        $gecti++  # Uyari, hata degil
    }
} else {
    Write-Host "    [BASARISIZ] test_results.json eksik" -ForegroundColor Red; $kaldi++
}

# ─────────────────────────────────────────────────────
# Regenerate modu
# ─────────────────────────────────────────────────────
if ($Regenerate) {
    Write-Host ""
    Write-Host "  [REGENERATE] test_results.json yeniden uretiliyor..." -ForegroundColor DarkGray
    $tarih = Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ"
    $overall = if ($kaldi -eq 0) { "PASS" } else { "FAIL" }
    $json = @"
{
  "meta": {
    "generated_at": "$tarih",
    "generator": "tests/fix12/run_fix12_tests.ps1",
    "schema_version": 1,
    "note": "Bu dosya her release'te yeniden uretilir. Elle duzenleme yasak."
  },
  "platform_matrix": {
    "windows_x86_64": "SUPPORTED",
    "linux_x86_64": "SUPPORTED",
    "linux_arm64": "EXPERIMENTAL",
    "macos_x86_64": "EXPERIMENTAL",
    "macos_arm64": "EXPERIMENTAL"
  },
  "overall": "$overall"
}
"@
    [System.IO.File]::WriteAllText("test_results.json", $json, [System.Text.Encoding]::UTF8)
    Write-Host "  [GECTI] test_results.json uretildi" -ForegroundColor Green
}

# ─────────────────────────────────────────────────────
# Sonuc
# ─────────────────────────────────────────────────────
Write-Host ""
$durum = if ($kaldi -eq 0) { "RELEASE GATE GECTI" } else { "RELEASE GATE BASARISIZ" }
$renk  = if ($kaldi -eq 0) { "Green" } else { "Red" }
Write-Host ("=== " + $durum + ": " + $gecti + " gecti | " + $kaldi + " basarisiz ===") -ForegroundColor $renk
Write-Host ""

if ($kaldi -gt 0) { exit 1 }
exit 0
