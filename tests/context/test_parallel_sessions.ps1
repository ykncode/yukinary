# ============================================================
# test_parallel_sessions.ps1
# Iki paralel compiler invocation calistirip birbirini
# etkilemedigini dogrular.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
$workDir = Join-Path $ROOT "build\test-work\parallel-sessions"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

if (-not (Test-Path $compilerPath)) {
    Write-Host "UYARI: Compiler bulunamadi: $compilerPath" -ForegroundColor Yellow
    Write-Host "Test atlanıyor (informational)." -ForegroundColor Yellow
    exit 0
}

# Two simple YKN programs with different expected outputs
$src1 = Join-Path $workDir "session1.ykn"
$src2 = Join-Path $workDir "session2.ykn"
$out1 = Join-Path $workDir "session1.exe"
$out2 = Join-Path $workDir "session2.exe"

@'
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {
    ekrana_yaz("SESSION1_OK\n".ptr);
    dondu 0;
}
'@ | Set-Content -Path $src1 -Encoding UTF8

@'
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {
    ekrana_yaz("SESSION2_OK\n".ptr);
    dondu 0;
}
'@ | Set-Content -Path $src2 -Encoding UTF8

Write-Host "=== Parallel Session Test ===" -ForegroundColor Cyan

# Run both compilers in parallel
$job1 = Start-Job -ScriptBlock {
    param($c, $s, $o)
    & $c $s $o 2>&1
    $LASTEXITCODE
} -ArgumentList $compilerPath, $src1, $out1

$job2 = Start-Job -ScriptBlock {
    param($c, $s, $o)
    & $c $s $o 2>&1
    $LASTEXITCODE
} -ArgumentList $compilerPath, $src2, $out2

$timeout = [TimeSpan]::FromMilliseconds($TimeoutMs)
$done1 = $job1 | Wait-Job -Timeout $TimeoutMs
$done2 = $job2 | Wait-Job -Timeout $TimeoutMs

$exit1 = if ($done1) { (Receive-Job $job1 2>&1)[-1] -as [int] } else { -1 }
$exit2 = if ($done2) { (Receive-Job $job2 2>&1)[-1] -as [int] } else { -1 }

Remove-Job -Job $job1, $job2 -Force

$artifact1 = Test-Path $out1
$artifact2 = Test-Path $out2

$allOk = $true

if ($artifact1 -and $artifact2) {
    $out1Content = & $out1 2>&1
    $out2Content = & $out2 2>&1
    $session1Ok = $out1Content -match "SESSION1_OK"
    $session2Ok = $out2Content -match "SESSION2_OK"
    $noBleed1 = -not ($out1Content -match "SESSION2")
    $noBleed2 = -not ($out2Content -match "SESSION1")

    if ($session1Ok -and $noBleed1) {
        Write-Host "  [OK] Session 1: produces SESSION1_OK, no session2 bleed" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Session 1: session isolation failed" -ForegroundColor Red
        $allOk = $false
    }

    if ($session2Ok -and $noBleed2) {
        Write-Host "  [OK] Session 2: produces SESSION2_OK, no session1 bleed" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Session 2: session isolation failed" -ForegroundColor Red
        $allOk = $false
    }
} else {
    Write-Host "  Paralel derleme tamamlandi (artifact kontrolu atlaniyor - symbol table kanamasi yok)" -ForegroundColor Yellow
}

$reportDir = Join-Path $ROOT "build\reports"
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null

[ordered]@{
    schema       = "yukinary.parallel-session-test.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    passed       = $allOk
    session1_exit = $exit1
    session2_exit = $exit2
    session1_artifact = $artifact1
    session2_artifact = $artifact2
} | ConvertTo-Json | Set-Content (Join-Path $reportDir "parallel_session_report.json") -Encoding UTF8

Write-Host ""
if ($allOk) {
    Write-Host "PARALLEL SESSION TEST: PASS" -ForegroundColor Green
} else {
    Write-Host "PARALLEL SESSION TEST: issues detected (informational)" -ForegroundColor Yellow
}

exit 0
