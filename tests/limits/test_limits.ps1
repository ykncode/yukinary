<#
.SYNOPSIS
    FAZ 3.1 Compiler Limitleri ve Dynamic Growth Test Suite
.DESCRIPTION
    Stage-0 ve Stage-3 compiler'larinda:
    1. Macro recursion depth / budget asimi (E015/E016)
    2. Parser recursion depth (E015)
    3. Buyuk AST dugum ve parametre arenasi dinamik buyume
    4. Struct kapasite genislemesi (300 struct > 256 eski sinir)
    5. Sembol tablosu ve yerel degisken buyumesi
    parity ile dogrulanir.
#>
[CmdletBinding()]
param(
    [string]$Compiler0 = "build/bootstrap/stage0.exe",
    [string]$Compiler3 = "build/bootstrap/stage3.exe",
    [string]$OutDir = "build/test-work/limits"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$COMP0 = Join-Path $ROOT $Compiler0
$COMP3 = Join-Path $ROOT $Compiler3
$WORK = Join-Path $ROOT $OutDir

New-Item -ItemType Directory -Path $WORK -Force | Out-Null

$passes = 0
$total = 0

function Run-Limit-Case([string]$Name, [string]$FixtureRel, [string]$ExpectedError, [bool]$ShouldSucceed) {
    $script:total++
    Write-Host ("`n[LIMIT TEST {0}] {1}" -f $script:total, $Name) -ForegroundColor Cyan
    $fixturePath = Join-Path $ROOT $FixtureRel

    foreach ($comp in @(@{Name="Stage-0"; Exe=$COMP0}, @{Name="Stage-3"; Exe=$COMP3})) {
        $outExe = Join-Path $WORK ("{0}_{1}.exe" -f $Name, $comp.Name)
        Remove-Item -LiteralPath $outExe -Force -ErrorAction SilentlyContinue

        $combined = & $comp.Exe $fixturePath $outExe 2>&1 | Out-String
        $exitCode = $LASTEXITCODE

        if ($ShouldSucceed) {
            if ($exitCode -ne 0) {
                Write-Host ("  [-] {0} FAILED to compile (exit code {1}):`n{2}" -f $comp.Name, $exitCode, $combined) -ForegroundColor Red
                return $false
            }
            if (-not (Test-Path -LiteralPath $outExe)) {
                Write-Host ("  [-] {0} produced no executable" -f $comp.Name) -ForegroundColor Red
                return $false
            }
            $runOut = & $outExe 2>&1 | Out-String
            $runExit = $LASTEXITCODE
            if ($runExit -ne 0) {
                Write-Host ("  [-] {0} binary exited with code {1}" -f $comp.Name, $runExit) -ForegroundColor Red
                return $false
            }
        } else {
            if ($exitCode -eq 0) {
                Write-Host ("  [-] {0} unexpectedly SUCCEEDED to compile" -f $comp.Name) -ForegroundColor Red
                return $false
            }
            if ($combined -notmatch [regex]::Escape($ExpectedError)) {
                Write-Host ("  [-] {0} missing expected diagnostic: '{1}' in output:`n{2}" -f $comp.Name, $ExpectedError, $combined) -ForegroundColor Red
                return $false
            }
        }
        Write-Host ("  [+] {0}: OK" -f $comp.Name) -ForegroundColor Green
    }

    $script:passes++
    return $true
}

Write-Host "=== YUKINARY FAZ 3.1 LIMITS TEST SUITE ===" -ForegroundColor Magenta

Run-Limit-Case "01_macro_recursion" "tests/limits/fixtures/01_macro_recursion.ykn" "ERR_RECURSION_DEPTH" $false
Run-Limit-Case "02_deep_expression" "tests/limits/fixtures/02_deep_expression.ykn" "ERR_RECURSION_DEPTH" $false
Run-Limit-Case "03_large_ast_growth" "tests/limits/fixtures/03_large_ast_growth.ykn" "" $true
Run-Limit-Case "04_struct_capacity" "tests/limits/fixtures/04_struct_capacity.ykn" "" $true
Run-Limit-Case "05_many_symbols" "tests/limits/fixtures/05_many_symbols.ykn" "" $true

Write-Host ("`n==========================================") -ForegroundColor Magenta
Write-Host ("Limits Test Sonucu: {0} / {1} passed" -f $passes, $total) -ForegroundColor $(if ($passes -eq $total) { "Green" } else { "Red" })

if ($passes -ne $total) {
    exit 1
}
exit 0
