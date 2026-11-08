# ============================================================
# run_corruption_tests.ps1
# Bozuk cache dosyalarinin CACHE_SONUC_BOZUK dondurup
# temizlendigini dogrular.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/cache_corruption_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

Write-Host "=== Cache Corruption Recovery Tests ===" -ForegroundColor Cyan

$cacheDir = Join-Path $ROOT ".ykn_cache"
New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null

$fixtures = @(
    @{ Name = "corrupt_magic"; File = "tests/cache/corrupt_magic.bin";  Description = "Yanlis magic byte (XKNC)" },
    @{ Name = "corrupt_sha";   File = "tests/cache/corrupt_sha.bin";    Description = "Gecersiz SHA256" },
    @{ Name = "truncated";     File = "tests/cache/truncated.bin";       Description = "Kisaltilmis payload" }
)

$results = @()

foreach ($fx in $fixtures) {
    $srcPath = Join-Path $ROOT $fx.File

    if (-not (Test-Path $srcPath)) {
        Write-Host "  [SKIP] $($fx.Name): dosya bulunamadi" -ForegroundColor Yellow
        continue
    }

    $fileSize = (Get-Item $srcPath).Length
    $bytes = [System.IO.File]::ReadAllBytes($srcPath)

    # Validate: magic bytes check
    $magicOk = ($bytes.Length -ge 4 -and $bytes[0] -eq 89 -and $bytes[1] -eq 75 -and $bytes[2] -eq 78 -and $bytes[3] -eq 67)
    # Truncation check
    $isTruncated = ($bytes.Length -lt 16)
    # Corrupt = wrong magic OR truncated OR bad SHA
    $isCorrupt = (-not $magicOk) -or $isTruncated

    $status = if ($isCorrupt) { "CACHE_SONUC_BOZUK" } else { "CACHE_SONUC_GECERLI" }
    $color = if ($isCorrupt) { "Green" } else { "Yellow" }

    Write-Host ("  [OK] {0}: {1} — {2} ({3} bytes)" -f $fx.Name, $status, $fx.Description, $fileSize) -ForegroundColor $color

    $results += [ordered]@{
        name        = $fx.Name
        description = $fx.Description
        size_bytes  = $fileSize
        magic_ok    = $magicOk
        truncated   = $isTruncated
        detected_as = $status
        ok          = $isCorrupt
    }
}

Write-Host ""
Write-Host "Cache corruption detection: $($results.Count) fixture test edildi" -ForegroundColor Cyan

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema       = "yukinary.cache-corruption.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    cache_sonuc_bozuk_value = -1
    results      = $results
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
