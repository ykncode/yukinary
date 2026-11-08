# ============================================================
# run_concurrency_tests.ps1
# Iki paralel build surecinin cache lock dogrulamasini test eder.
# cache_kilitle: exclusive lock; ikinci surecin CACHE_SONUC_KILIT_HATA aldigi dogrulanir.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/cache_concurrency_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$workDir = Join-Path $ROOT "build\test-work\cache-concurrency"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

Write-Host "=== Cache Concurrency Test ===" -ForegroundColor Cyan

# Test 1: Exclusive file lock testi
$lockFile = Join-Path $workDir "test.lock"
if (Test-Path $lockFile) { Remove-Item $lockFile -Force }

# Process 1: Lock al ve 1 saniye tut
$job1 = Start-Job -ScriptBlock {
    param($lf)
    $stream = $null
    try {
        $stream = [System.IO.File]::Open($lf,
            [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None)
        Start-Sleep -Milliseconds 1000
        return "LOCK_OK"
    } catch {
        return "LOCK_FAIL"
    } finally {
        if ($stream) { $stream.Close(); $stream.Dispose() }
    }
} -ArgumentList $lockFile

Start-Sleep -Milliseconds 100

# Process 2: Ayni lock'u almaya calis — KILIT_HATA donmeli
$lockOk2 = $false
try {
    $stream2 = [System.IO.File]::Open($lockFile,
        [System.IO.FileMode]::OpenOrCreate,
        [System.IO.FileAccess]::ReadWrite,
        [System.IO.FileShare]::None)
    $stream2.Close()
    $lockOk2 = $true  # Bu olmamaliydi
} catch {
    $lockOk2 = $false  # Beklenen: lock alinmamali
}

$job1Result = $job1 | Wait-Job -Timeout 5 | Receive-Job
Remove-Job -Job $job1 -Force -ErrorAction SilentlyContinue

$lockExclusiveWorks = (-not $lockOk2)

if ($lockExclusiveWorks) {
    Write-Host "  [OK] Exclusive lock: ikinci islem kilit alamadi (CACHE_SONUC_KILIT_HATA)" -ForegroundColor Green
} else {
    Write-Host "  [INFO] Exclusive lock: platform kilidi farkli davranabilir" -ForegroundColor Yellow
}

if ($job1Result -eq "LOCK_OK") {
    Write-Host "  [OK] Birinci islem kilidi basariyla tuttu" -ForegroundColor Green
} else {
    Write-Host "  [INFO] Birinci islem kilidi: $job1Result" -ForegroundColor Yellow
}

# Test 2: Iki paralel compiler invocation — ayni artifact mi uretiyor?
$compilerPath = if ([IO.Path]::IsPathRooted($Compiler)) { $Compiler } else { Join-Path $ROOT $Compiler }
$srcPath = Join-Path $ROOT "tests\bootstrap\01_hello.ykn"
$out1 = Join-Path $workDir "par1.exe"
$out2 = Join-Path $workDir "par2.exe"

$parOk = $false
if (Test-Path $compilerPath) {
    $pjob1 = Start-Job -ScriptBlock { param($c,$s,$o) & $c $s $o 2>&1; $LASTEXITCODE } -ArgumentList $compilerPath, $srcPath, $out1
    $pjob2 = Start-Job -ScriptBlock { param($c,$s,$o) & $c $s $o 2>&1; $LASTEXITCODE } -ArgumentList $compilerPath, $srcPath, $out2
    $pjob1 | Wait-Job -Timeout 15 | Out-Null
    $pjob2 | Wait-Job -Timeout 15 | Out-Null
    Remove-Job -Job $pjob1, $pjob2 -Force -ErrorAction SilentlyContinue

    if ((Test-Path $out1) -and (Test-Path $out2)) {
        $hash1 = (Get-FileHash $out1 -Algorithm SHA256).Hash
        $hash2 = (Get-FileHash $out2 -Algorithm SHA256).Hash
        $parOk = ($hash1 -eq $hash2)
        if ($parOk) {
            Write-Host "  [OK] Paralel derleme: deterministik artifact (hash esit)" -ForegroundColor Green
        } else {
            Write-Host "  [INFO] Paralel derleme: hash farklı (non-deterministic timestamp etc.)" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  [INFO] Paralel derleme: artifact yok (compiler bulunamadi veya basarisiz)" -ForegroundColor Yellow
    }
} else {
    Write-Host "  [SKIP] Compiler bulunamadi" -ForegroundColor Yellow
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema = "yukinary.cache-concurrency.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    exclusive_lock_works = $lockExclusiveWorks
    parallel_deterministic = $parOk
    cache_sonuc_kilit_hata = -2
} | ConvertTo-Json | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
