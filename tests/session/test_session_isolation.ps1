# ============================================================
# test_session_isolation.ps1 - FAZ 2 Session Isolation & Determinism Tests
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe"
)

$ErrorActionPreference = "Stop"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $compilerPath = (Resolve-Path -LiteralPath $Compiler).Path
} else {
    $compilerPath = (Resolve-Path -LiteralPath (Join-Path $ROOT $Compiler)).Path
}

Write-Host "=== FAZ 2 COMPILER SESSION ISOLATION & PARALLEL DETERMINISM ===" -ForegroundColor Cyan
Write-Host "Compiler: $compilerPath`n" -ForegroundColor Yellow

$testWork = Join-Path $ROOT "build\test-work\session-tests"
if (Test-Path $testWork) { Remove-Item -LiteralPath $testWork -Recurse -Force }
New-Item -ItemType Directory -Path $testWork -Force | Out-Null

$passCount = 0
$totalTests = 0

function Run-Test([string]$Name, [scriptblock]$Action) {
    $global:totalTests++
    Write-Host -NoNewline "[TEST $global:totalTests] $Name ... "
    try {
        & $Action
        $global:passCount++
        Write-Host "PASS" -ForegroundColor Green
    } catch {
        Write-Host "FAIL: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Test 1: Symbol Table Isolation
Run-Test "Symbol Table Isolation: Session 1 symbols do not leak to Session 2" {
    $out1 = Join-Path $testWork "s1.exe"
    $f1 = Join-Path $PSScriptRoot "session1_symbols.ykn"
    $p1 = Start-Process -FilePath $compilerPath -ArgumentList @($f1, $out1) -NoNewWindow -Wait -PassThru
    if ($p1.ExitCode -ne 0) { throw "Session 1 compilation failed with code $($p1.ExitCode)" }
    if (-not (Test-Path $out1)) { throw "Session 1 executable was not generated" }

    $out2 = Join-Path $testWork "s2.exe"
    $f2 = Join-Path $PSScriptRoot "session2_leak_check.ykn"
    $p2 = Start-Process -FilePath $compilerPath -ArgumentList @($f2, $out2) -NoNewWindow -Wait -PassThru
    if ($p2.ExitCode -eq 0) { throw "Session 2 unexpectedly succeeded; symbol table leaked from Session 1!" }
    if (Test-Path $out2) { throw "Session 2 generated binary despite type error" }
}

# Test 2: Diagnostic Isolation after Failure
Run-Test "Diagnostic Isolation: Clean compilation immediately after failure" {
    $outClean = Join-Path $testWork "clean.exe"
    $fClean = Join-Path $PSScriptRoot "clean_program.ykn"
    $pClean = Start-Process -FilePath $compilerPath -ArgumentList @($fClean, $outClean) -NoNewWindow -Wait -PassThru
    if ($pClean.ExitCode -ne 0) { throw "Clean compilation failed with code $($pClean.ExitCode)" }
    if (-not (Test-Path $outClean)) { throw "Clean binary was not generated" }
}

# Test 3: Parallel Compilation Determinism
Run-Test "Parallel Compilation Determinism: Two concurrent compilations produce identical binaries" {
    $src = Join-Path $PSScriptRoot "session1_symbols.ykn"
    $outA = Join-Path $testWork "parallel_a.exe"
    $outB = Join-Path $testWork "parallel_b.exe"

    $psiA = New-Object System.Diagnostics.ProcessStartInfo
    $psiA.FileName = $compilerPath
    $psiA.Arguments = "`"$src`" `"$outA`""
    $psiA.UseShellExecute = $false
    $pA = [System.Diagnostics.Process]::Start($psiA)

    $psiB = New-Object System.Diagnostics.ProcessStartInfo
    $psiB.FileName = $compilerPath
    $psiB.Arguments = "`"$src`" `"$outB`""
    $psiB.UseShellExecute = $false
    $pB = [System.Diagnostics.Process]::Start($psiB)

    $pA.WaitForExit()
    $pB.WaitForExit()

    $exitA = $pA.ExitCode
    $exitB = $pB.ExitCode

    if ($exitA -ne 0 -or $exitB -ne 0) {
        throw "Parallel compilation failed: pA=$exitA, pB=$exitB"
    }
    if (-not (Test-Path $outA) -or -not (Test-Path $outB)) { throw "Output binaries missing" }

    $hashA = (Get-FileHash -LiteralPath $outA -Algorithm SHA256).Hash
    $hashB = (Get-FileHash -LiteralPath $outB -Algorithm SHA256).Hash
    if ($hashA -ne $hashB) { throw "Non-deterministic output: $hashA != $hashB" }
}

Write-Host "`nResult: $passCount / $totalTests tests passed." -ForegroundColor Yellow
if ($passCount -ne $totalTests) {
    exit 1
}
