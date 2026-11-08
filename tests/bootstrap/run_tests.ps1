# ============================================================
# run_tests.ps1 - Yukinary Regression Test Runner
# Usage: .\tests\bootstrap\run_tests.ps1
#        .\tests\bootstrap\run_tests.ps1 -Compiler .\build\bootstrap\stage0.exe
#        .\tests\bootstrap\run_tests.ps1 -Filter 03
#        .\tests\bootstrap\run_tests.ps1 -AllCompilers
# ============================================================
param(
    [string]$Compiler     = "",
    [string]$Filter       = "",
    [switch]$AllCompilers
)

$ErrorActionPreference = "SilentlyContinue"
$ROOT    = (Resolve-Path "$PSScriptRoot\..\..").Path
$TESTDIR = "$PSScriptRoot"
$WORK    = Join-Path $ROOT "build\test-work\bootstrap-regression"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null
$TMP_EXE = Join-Path $WORK "test-output.exe"
$TMP_OUT = Join-Path $WORK "test-stdout.txt"

$TOTAL  = 0
$PASSED = 0
$FAILED = 0
$SKIP   = 0

function Run-Test($compiler_path, $ykn_file, $expected_file) {
    $script:TOTAL++
    $name = [System.IO.Path]::GetFileNameWithoutExtension($ykn_file)

    if (-not (Test-Path $expected_file)) {
        Write-Host "  [--] $name  (no .expected, skipped)" -ForegroundColor DarkGray
        $script:SKIP++
        return
    }

    # Compile. The explicit build/test-work output is the only accepted artifact.
    Remove-Item -LiteralPath $TMP_EXE -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $TMP_OUT -Force -ErrorAction SilentlyContinue

    & $compiler_path $ykn_file $TMP_EXE 2>&1 | Out-Null

    $target_exe = ""
    if (Test-Path -LiteralPath $TMP_EXE) {
        $target_exe = $TMP_EXE
    }

    if ($LASTEXITCODE -ne 0 -or $target_exe -eq "") {
        Write-Host "  [!!] $name  COMPILE ERROR" -ForegroundColor Red
        $script:FAILED++
        return
    }

    # Run and capture stdout
    & $target_exe 2>&1 | Out-File -FilePath $TMP_OUT -Encoding utf8
    $actual   = (Get-Content $TMP_OUT   -Raw -Encoding utf8) -replace "`r`n", "`n"
    $expected = (Get-Content $expected_file -Raw -Encoding utf8) -replace "`r`n", "`n"

    Remove-Item $target_exe -Force -ErrorAction SilentlyContinue

    if ($actual -eq $expected) {
        Write-Host "  [OK] $name" -ForegroundColor Green
        $script:PASSED++
    } else {
        Write-Host "  [!!] $name  OUTPUT MISMATCH" -ForegroundColor Red
        Write-Host "       Expected: $($expected.Trim())" -ForegroundColor DarkYellow
        Write-Host "       Actual  : $($actual.Trim())"  -ForegroundColor DarkRed
        $script:FAILED++
    }
}

function Run-Suite($compiler_path) {
    $cname = Split-Path $compiler_path -Leaf
    Write-Host ""
    Write-Host "======================================" -ForegroundColor Cyan
    Write-Host "  Compiler: $cname" -ForegroundColor Cyan
    Write-Host "======================================" -ForegroundColor Cyan

    $ykn_files = Get-ChildItem "$TESTDIR\*.ykn" | Where-Object {
        $Filter -eq "" -or $_.BaseName -like "$Filter*"
    } | Sort-Object Name

    if ($ykn_files.Count -eq 0) {
        Write-Host "  [--] No matching test files found." -ForegroundColor DarkGray
        return
    }

    $pre_p = $PASSED; $pre_f = $FAILED; $pre_t = $TOTAL
    foreach ($ykn in $ykn_files) {
        $exp = [System.IO.Path]::ChangeExtension($ykn.FullName, ".expected")
        Run-Test $compiler_path $ykn.FullName $exp
    }
    $suite_p = $PASSED - $pre_p
    $suite_f = $FAILED - $pre_f
    $suite_t = $TOTAL  - $pre_t
    Write-Host "  --- $suite_p/$suite_t passed, $suite_f failed ---" -ForegroundColor White
}

function Resolve-CompilerPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $ROOT $Path))
}

# --- Select only manifest-generated compilers ---
$bootstrapRoot = Join-Path $ROOT "build\bootstrap"
if ($AllCompilers) {
    $compilers = @(
        (Join-Path $bootstrapRoot "stage0.exe"),
        (Join-Path $bootstrapRoot "stage1.exe"),
        (Join-Path $bootstrapRoot "stage2.exe"),
        (Join-Path $bootstrapRoot "stage3.exe")
    ) | Where-Object { Test-Path -LiteralPath $_ }
} elseif ($Compiler -ne "") {
    $compilers = @(Resolve-CompilerPath $Compiler)
} else {
    $compilers = @(
        (Join-Path $bootstrapRoot "stage0.exe"),
        (Join-Path $bootstrapRoot "stage1.exe"),
        (Join-Path $bootstrapRoot "stage2.exe"),
        (Join-Path $bootstrapRoot "stage3.exe")
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ($compilers.Count -eq 0) {
        Write-Host "[!!] No canonical compiler found. Run run_bootstrap.ps1 first." -ForegroundColor Red
        exit 1
    }
}

foreach ($c in $compilers) { Run-Suite $c }

# --- Summary ---
Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "  OVERALL SUMMARY" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "  Total   : $TOTAL"  -ForegroundColor White
Write-Host "  Passed  : $PASSED" -ForegroundColor Green
Write-Host "  Failed  : $FAILED" -ForegroundColor $(if ($FAILED -gt 0) {"Red"} else {"Gray"})
Write-Host "  Skipped : $SKIP"   -ForegroundColor DarkGray
Write-Host ""

if ($FAILED -gt 0) {
    Write-Host "REGRESSION DETECTED - $FAILED test(s) FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "ALL TESTS PASSED ($PASSED/$TOTAL)" -ForegroundColor Green
    exit 0
}
