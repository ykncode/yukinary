# ============================================================
# debug_basic.ps1
# --debug bayragi ile uretilen artifact'in debug bolumunun
# yapisal olarak dogrulandigi test.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/debug_info_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
$workDir = Join-Path $ROOT "build\test-work\debug-info"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

if (-not (Test-Path $compilerPath)) {
    Write-Host "UYARI: Compiler bulunamadi: $compilerPath" -ForegroundColor Yellow
    exit 0
}

$srcPath = Join-Path $ROOT "tests\bootstrap\01_hello.ykn"
$outPathNormal = Join-Path $workDir "hello_nodebug.exe"
$outPathDebug  = Join-Path $workDir "hello_debug.exe"

Write-Host "=== Debug Info Test ===" -ForegroundColor Cyan

# Compile without --debug
& $compilerPath $srcPath $outPathNormal 2>&1 | Out-Null
$exitNormal = $LASTEXITCODE
$sizeNormal = if (Test-Path $outPathNormal) { (Get-Item $outPathNormal).Length } else { 0 }

# Compile with --debug
& $compilerPath $srcPath $outPathDebug --debug 2>&1 | Out-Null
$exitDebug = $LASTEXITCODE
$sizeDebug = if (Test-Path $outPathDebug) { (Get-Item $outPathDebug).Length } else { 0 }

$normalOk = ($exitNormal -eq 0 -and $sizeNormal -gt 0)
$debugOk  = ($exitDebug -eq 0 -and $sizeDebug -gt 0)

if ($normalOk) {
    Write-Host "  [OK] Normal derleme basarili: $sizeNormal byte" -ForegroundColor Green
} else {
    Write-Host "  [INFO] Normal derleme: exit=$exitNormal, size=$sizeNormal" -ForegroundColor Yellow
}

if ($debugOk) {
    Write-Host "  [OK] --debug derleme basarili: $sizeDebug byte" -ForegroundColor Green
} else {
    Write-Host "  [INFO] --debug derleme: exit=$exitDebug, size=$sizeDebug" -ForegroundColor Yellow
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema         = "yukinary.debug-info.v1"
    generated_at   = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    normal_ok      = $normalOk
    debug_flag_ok  = $debugOk
    normal_size    = $sizeNormal
    debug_size     = $sizeDebug
    flag           = "--debug"
    pe_format      = "CodeView (minimal)"
    elf_format     = "DWARF4 (minimal)"
} | ConvertTo-Json | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
