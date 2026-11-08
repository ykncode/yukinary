<#
.SYNOPSIS
    test_ffi.ps1 - Yukinary Faz 9 FFI test suite (Saf PowerShell)
.DESCRIPTION
    Python bagimliligi olmadan #c-lib direktifi, extern isv prototipi ve Win32 IAT cagri testleri.
    Tests:
      1. #c-lib direktif + normal isv - FFI SYNTAX OK beklenir
      2. extern isv + kernel32 WriteFile IAT - FFI WriteFile OK beklenir
      3. isv prototip (noktali virgul) sozdizimi - FFI SYNTAX OK beklenir
#>
param(
    [string]$Compiler = ".\build\bootstrap\stage0.exe"
)

$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $Compiler = [System.IO.Path]::GetFullPath($Compiler)
} else {
    $Compiler = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
if (-not (Test-Path -LiteralPath $Compiler)) {
    Write-Host "  [FAIL] Compiler bulunamadi: $Compiler" -ForegroundColor Red
    exit 1
}
$TestsDir = $PSScriptRoot
$WORK = Join-Path $ROOT "build\test-work\ffi"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

$pass = 0
$fail = 0

Write-Host "=== Faz 9 FFI Test Suiti ===" -ForegroundColor Cyan

function Invoke-FFITest {
    param(
        [string]$label,
        [string]$srcFile,
        [string]$expectedStdout = "",
        [int]$expectedExit = 0
    )
    $src    = Join-Path $TestsDir $srcFile
    $outExe = Join-Path $WORK "ffi-test.exe"

    Remove-Item -LiteralPath $outExe -Force -ErrorAction SilentlyContinue

    $cOut = & $Compiler $src $outExe 2>&1
    $cCode = $LASTEXITCODE


    if ($cCode -ne 0 -or -not (Test-Path $outExe)) {
        Write-Host "  [FAIL] $label -- derleme hatasi: $($cOut -join ' ')" -ForegroundColor Red
        $script:fail++
        return
    }

    $rOut = & $outExe 2>&1
    $rCode = $LASTEXITCODE
    $runStr = ($rOut -join "`n").Trim()

    $ok = $true
    if ($rCode -ne $expectedExit) {
        Write-Host "  [FAIL] $label -- cikis kodu $rCode (beklenen $expectedExit)" -ForegroundColor Red
        $ok = $false
    }
    if ($expectedStdout -ne "" -and -not $runStr.Contains($expectedStdout)) {
        Write-Host "  [FAIL] $label -- beklenen '$expectedStdout' bulunamadi" -ForegroundColor Red
        $ok = $false
    }

    if ($ok) {
        Write-Host "  [PASS] $label" -ForegroundColor Green
        $script:pass++
    } else {
        $script:fail++
    }

    Remove-Item -LiteralPath $outExe -Force -ErrorAction SilentlyContinue
}

Invoke-FFITest -label "01 #c-lib direktif + normal isv" -srcFile "01_extern_syntax.ykn" -expectedStdout "FFI SYNTAX OK"
Invoke-FFITest -label "02 extern isv + kernel32 WriteFile IAT" -srcFile "02_kernel32_writefile.ykn" -expectedStdout "FFI WriteFile OK"
Invoke-FFITest -label "03 isv prototip (noktali virgul) sozdizimi" -srcFile "01_extern_syntax.ykn" -expectedStdout "FFI SYNTAX OK"

Write-Host "`nSonuc: $pass PASS / $fail FAIL / $($pass+$fail) toplam"
if ($fail -eq 0) { exit 0 } else { exit 1 }
