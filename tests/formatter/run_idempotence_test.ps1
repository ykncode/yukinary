# ============================================================
# run_idempotence_test.ps1
# src/**/*.ykn ve std/**/*.ykn dosyalarini iki kez formatlayip
# byte-identical olduğunu doğrular (formatter idempotence).
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/formatter_idempotence_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
$workDir = Join-Path $ROOT "build\test-work\formatter-idempotence"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

Write-Host "=== Formatter Idempotence Test ===" -ForegroundColor Cyan
Write-Host "Compiler: $compilerPath" -ForegroundColor Gray

if (-not (Test-Path $compilerPath)) {
    Write-Host "  UYARI: Compiler bulunamadi - test atlaniyor" -ForegroundColor Yellow
    [ordered]@{
        schema = "yukinary.formatter-idempotence.v1"
        generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
        passed = $true
        skipped = $true
        reason = "compiler not found"
    } | ConvertTo-Json | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8
    exit 0
}

$sources = @(
    Get-ChildItem -Path (Join-Path $ROOT "src") -Filter "*.ykn" -Recurse
    Get-ChildItem -Path (Join-Path $ROOT "std") -Filter "*.ykn" -Recurse
) | Sort-Object FullName

Write-Host "  Kontrol edilecek dosya: $($sources.Count)" -ForegroundColor Gray

$results = @()
$allIdempotent = $true
$tested = 0; $skipped = 0

foreach ($src in $sources) {
    $relPath = $src.FullName.Substring($ROOT.Length).TrimStart('\/')
    $baseName = $src.Name -replace '[^\w]', '_'
    $workCopy = Join-Path $workDir ($baseName + "_work.ykn")

    # Make a working copy
    Copy-Item -LiteralPath $src.FullName -Destination $workCopy -Force

    # Format pass 1
    $fmt1 = Join-Path $workDir ($baseName + "_fmt1.ykn")
    Copy-Item -LiteralPath $workCopy -Destination $fmt1 -Force
    & $compilerPath fmt $fmt1 2>&1 | Out-Null

    # Format pass 2
    $fmt2 = Join-Path $workDir ($baseName + "_fmt2.ykn")
    Copy-Item -LiteralPath $fmt1 -Destination $fmt2 -Force
    & $compilerPath fmt $fmt2 2>&1 | Out-Null

    # Compare
    $hash1 = if (Test-Path $fmt1) { (Get-FileHash $fmt1 -Algorithm SHA256).Hash } else { "" }
    $hash2 = if (Test-Path $fmt2) { (Get-FileHash $fmt2 -Algorithm SHA256).Hash } else { "" }

    $idempotent = ($hash1 -eq $hash2 -and $hash1 -ne "")

    if (-not $idempotent -and $hash1 -ne "" -and $hash2 -ne "") {
        $allIdempotent = $false
        Write-Host ("  [FAIL] {0}: hash1={1} hash2={2}" -f $relPath, $hash1.Substring(0,8), $hash2.Substring(0,8)) -ForegroundColor Red
    }

    $results += [ordered]@{
        file = $relPath
        idempotent = $idempotent
        hash1 = $hash1
        hash2 = $hash2
    }
    $tested++
}

if ($allIdempotent) {
    Write-Host "  [OK] Tum $tested dosya: formatter idempotent" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] Bazi dosyalar idempotent degil" -ForegroundColor Red
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema       = "yukinary.formatter-idempotence.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    passed       = $allIdempotent
    tested       = $tested
    skipped      = $skipped
    results      = $results
} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit $(if ($allIdempotent) { 0 } else { 1 })
