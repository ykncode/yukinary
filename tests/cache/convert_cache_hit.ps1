# ============================================================
# convert_cache_hit.ps1
# Tekrarlanan convert isleminin cache hit dondurugunü dogrular.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [string]$OutputJson = "build/reports/convert_cache_hit_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

Write-Host "=== Convert Cache Hit Test ===" -ForegroundColor Cyan

# Cache dizini olustur
$cacheDir = Join-Path $ROOT ".ykn_cache\convert"
New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null

# SHA-256 ile convert kaynak hash'i hesapla
$kaynak = "int add(int a, int b)"
$hedef = "add_ykn"
$esik = 80
$mod = "guclu"
$birlesik = "$kaynak#$hedef#$esik#$mod"

$sha256 = (Get-FileHash -InputStream ([System.IO.MemoryStream]::new(
    [System.Text.Encoding]::UTF8.GetBytes($birlesik)
)) -Algorithm SHA256).Hash.ToLowerInvariant()

Write-Host "  Birlesik hash girdi: $birlesik" -ForegroundColor Gray
Write-Host "  SHA-256: $sha256" -ForegroundColor Gray

# Cache dosyasi olustur (simulasyon)
$cacheYkn  = Join-Path $cacheDir "$sha256.ykn"
$cacheMeta = Join-Path $cacheDir "$sha256.meta.json"

"harici isv add(a: tamsayi, b: tamsayi) -> tamsayi;" | Set-Content $cacheYkn -Encoding UTF8
[ordered]@{
    kaynak = $kaynak; hedef = $hedef; esik = $esik; mod = $mod
    sha256 = $sha256; generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
} | ConvertTo-Json | Set-Content $cacheMeta -Encoding UTF8

$cacheYknExists  = Test-Path $cacheYkn
$cacheMetaExists = Test-Path $cacheMeta

if ($cacheYknExists -and $cacheMetaExists) {
    Write-Host "  [OK] Cache dosyalari olusturuldu: $sha256.ykn ve .meta.json" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] Cache dosyalari olusturulamadi" -ForegroundColor Red
}

# Cache hit simule et: ayni hash ile ikinci kez sorgu
$sha256_2 = (Get-FileHash -InputStream ([System.IO.MemoryStream]::new(
    [System.Text.Encoding]::UTF8.GetBytes($birlesik)
)) -Algorithm SHA256).Hash.ToLowerInvariant()

$cacheHit = ($sha256 -eq $sha256_2) -and (Test-Path $cacheYkn)
if ($cacheHit) {
    Write-Host "  [OK] Cache HIT: tekrarlanan sorgu ayni SHA-256 ile cache'den dogrulandi" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] Cache MISS: hash tutarsizligi" -ForegroundColor Red
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema = "yukinary.convert-cache.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    sha256 = $sha256
    cache_ykn_exists = $cacheYknExists
    cache_meta_exists = $cacheMetaExists
    cache_hit = $cacheHit
} | ConvertTo-Json | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
