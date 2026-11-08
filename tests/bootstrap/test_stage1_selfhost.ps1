# ============================================================
# test_stage1_selfhost.ps1
# Stage-1 self-host compiler entry-point acceptance test
# ============================================================

param(
    [string]$Compiler = ".\build\bootstrap\stage1.exe"
)

$ErrorActionPreference = "Stop"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$WORK = Join-Path $ROOT "build\test-work\stage1-selfhost"
$OUTPUT = Join-Path $WORK "stage1-selfhost.exe"

New-Item -ItemType Directory -Path $WORK -Force | Out-Null

if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $compilerPath = [System.IO.Path]::GetFullPath($Compiler)
} else {
    $compilerPath = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  TEST STAGE-1 SELF-HOST ana() ENTRY POINT" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

if ([string]::IsNullOrEmpty($compilerPath) -or -not (Test-Path -LiteralPath $compilerPath)) {
    Write-Host "[FAIL] Stage-1 compiler bulunamadi: $compilerPath" -ForegroundColor Red
    exit 1
}

Push-Location $ROOT
try {
    Remove-Item -LiteralPath $OUTPUT -Force -ErrorAction SilentlyContinue

    Write-Host ("[>>] " + $compilerPath + " tests\mini_hello.ykn " + $OUTPUT) -ForegroundColor Yellow
    $compOut = & $compilerPath "tests\mini_hello.ykn" $OUTPUT 2>&1
    $compCode = $LASTEXITCODE


    if ($compCode -ne 0 -or (-not (Test-Path -LiteralPath $OUTPUT))) {
        Write-Host "[FAIL] mini_hello.ykn derlenemedi! Exit: $compCode" -ForegroundColor Red
        Write-Host ($compOut -join "`n")
        exit 1
    }

    Write-Host "[>>] Uretilen mini program calistiriliyor..." -ForegroundColor Yellow
    $runOut = & $OUTPUT 2>&1
    $runCode = $LASTEXITCODE
    $runText = ($runOut -join "`n").Trim()

    Write-Host ("Cikti: '" + $runText + "' (Exit: " + $runCode + ")") -ForegroundColor Gray

    if ($runCode -eq 0 -and $runText.Contains("merhaba") -and (-not $runText.Contains("Kullanim:"))) {
        Write-Host "[PASS] Stage-1 Self-Host ana() testi BASARILI!" -ForegroundColor Green
        exit 0
    }

    Write-Host ("[FAIL] Beklenen: 'merhaba', Alinan: '" + $runText + "' (Exit: " + $runCode + ")") -ForegroundColor Red
    exit 1
} finally {
    Remove-Item -LiteralPath $OUTPUT -Force -ErrorAction SilentlyContinue
    Pop-Location
}
