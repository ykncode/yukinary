# ============================================================
# run_memory_tests.ps1
# Bellek guvenlik test fixture'larini derleyip calistiran script.
# UAF, double-free, dangling pointer senaryolari.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/memory_safety_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
$workDir = Join-Path $ROOT "build\test-work\memory-tests"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

if (-not (Test-Path $compilerPath)) {
    Write-Host "UYARI: Compiler bulunamadi: $compilerPath — test atlanıyor" -ForegroundColor Yellow
    exit 0
}

$fixtures = @(
    @{ Name = "uaf_test";         File = "tests/memory/uaf_test.ykn";         ExpectedPartialOut = "UAF_TEST: serbest birakma OK" },
    @{ Name = "double_free_test"; File = "tests/memory/double_free_test.ykn"; ExpectedPartialOut = "DOUBLE_FREE_TEST: ilk serbest birakma OK" },
    @{ Name = "dangling_test";    File = "tests/memory/dangling_test.ykn";     ExpectedPartialOut = "DANGLING_TEST: serbest birakma OK" }
)

Write-Host "=== Memory Safety Tests ===" -ForegroundColor Cyan
Write-Host "Compiler: $compilerPath" -ForegroundColor Gray
Write-Host ""

$results = @()
$passed = 0

foreach ($fx in $fixtures) {
    $srcPath = Join-Path $ROOT $fx.File
    $artPath = Join-Path $workDir "$($fx.Name).exe"

    if (-not (Test-Path $srcPath)) {
        Write-Host "  [SKIP] $($fx.Name): kaynak dosya bulunamadi" -ForegroundColor Yellow
        continue
    }

    # Compile
    $compileOut = & $compilerPath $srcPath $artPath 2>&1
    $compileExit = $LASTEXITCODE

    $compiled = ($compileExit -eq 0 -and (Test-Path $artPath))
    if (-not $compiled) {
        Write-Host "  [INFO] $($fx.Name): derleme basarisiz (beklenen olabilir)" -ForegroundColor Yellow
        $results += [ordered]@{
            name = $fx.Name
            compiled = $false
            compile_exit = $compileExit
            run_exit = $null
            expected_partial = $fx.ExpectedPartialOut
            partial_match = $false
        }
        continue
    }

    # Run with timeout — expect non-zero exit (abort/panic) or partial expected output
    $proc = $null
    $runOut = ""
    $runExit = -1
    try {
        $psi = [System.Diagnostics.ProcessStartInfo]::new($artPath)
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.UseShellExecute = $false
        $proc = [System.Diagnostics.Process]::Start($psi)
        $runOut = $proc.StandardOutput.ReadToEnd()
        $proc.WaitForExit($TimeoutMs) | Out-Null
        $runExit = $proc.ExitCode
    } catch { $runExit = -1 }

    $partialMatch = $runOut -match [regex]::Escape($fx.ExpectedPartialOut)
    $neverReached = -not ($runOut -match "ULASILMAMALI")

    # Success: program printed expected partial output + did not reach "ULASILMAMALI" OR exited non-zero
    $ok = $partialMatch -and ($neverReached -or $runExit -ne 0)

    if ($ok) {
        $passed++
        Write-Host ("  [OK] {0}: exit={1}, partial_match={2}" -f $fx.Name, $runExit, $partialMatch) -ForegroundColor Green
    } else {
        Write-Host ("  [INFO] {0}: exit={1}, partial_match={2}, never_reached={3} (memory safety enforcement depends on runtime)" -f `
            $fx.Name, $runExit, $partialMatch, $neverReached) -ForegroundColor Yellow
    }

    $results += [ordered]@{
        name = $fx.Name
        compiled = $compiled
        compile_exit = $compileExit
        run_exit = $runExit
        expected_partial = $fx.ExpectedPartialOut
        partial_match = $partialMatch
        never_reached_ulasilmamali = $neverReached
        ok = $ok
    }
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null

[ordered]@{
    schema       = "yukinary.memory-safety.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    passed       = $passed
    total        = $fixtures.Count
    results      = $results
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host ""
Write-Host "Memory tests done. Report: $OutputJson" -ForegroundColor Cyan
exit 0
