<##
.SYNOPSIS
    Native SIMD DLL -> generated PE acceptance gate.
.DESCRIPTION
    Rebuilds the current native LLM DLL, compiles both generated-PE fixtures,
    runs them beside the DLL, and compares exact stdout. This intentionally
    does not trust the generated process exit code alone: the fixtures report
    their own PASS marker because the hand-emitted PE entrypoint can return 0
    even when an assertion inside the fixture fails.
#>
[CmdletBinding()]
param(
    [string]$Compiler = ".\build\bootstrap\stage0.exe",
    [string]$Cxx = "g++",
    [switch]$Avx2
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$work = Join-Path $ROOT "build\test-work\llm-simd-pe"
New-Item -ItemType Directory -Path $work -Force | Out-Null

$compilerPath = if ([System.IO.Path]::IsPathRooted($Compiler)) {
    [System.IO.Path]::GetFullPath($Compiler)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
$dllPath = Join-Path $work "ykn_llm_simd.dll"
$source = Join-Path $ROOT "libykn\llm_simd.cpp"
$gridGenerator = Join-Path $ROOT "tools\generate_iq2s_grid_inc.py"
$gridInclude = Join-Path $ROOT "libykn\llm_iq2s_grid.inc"

if (-not (Test-Path -LiteralPath $compilerPath)) { throw "Compiler bulunamadı: $compilerPath" }
if (-not (Test-Path -LiteralPath $source)) { throw "SIMD kaynağı bulunamadı: $source" }

& python $gridGenerator
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $gridInclude)) {
    throw "IQ2_S grid include üretilemedi."
}

$nativeArgs = @("-std=c++17", "-O2", "-shared")
if ($Avx2) { $nativeArgs += "-mavx2" }
$nativeArgs += @($source, "-o", $dllPath)
& $Cxx @nativeArgs
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $dllPath)) {
    throw "ykn_llm_simd.dll derlenemedi."
}

$cases = @(
    @{ Name = "20_simd_pe"; Expected = "DOT=262144 DOT-PASS BACKEND-PASS" },
    @{ Name = "21_iq2s_simd_pe"; Expected = "RESULT=524288 IQ2S-PE-PASS" }
    @{ Name = "22_q1_simd_pe"; Expected = "Q1-PE-PASS" },
    @{ Name = "23_f16_simd_pe"; Expected = "F16-PE-PASS" }
)
$failed = 0
foreach ($case in $cases) {
    $input = Join-Path $ROOT ("tests\llm\" + $case.Name + ".ykn")
    $output = Join-Path $work ($case.Name + ".exe")
    if (-not (Test-Path -LiteralPath $input)) { throw "Fixture bulunamadı: $input" }
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue

    & $compilerPath $input $output *> $null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $output)) {
        Write-Host ("[FAIL] {0}: compile" -f $case.Name) -ForegroundColor Red
        $failed++
        continue
    }

    $actual = (& $output 2>&1 | Out-String).Trim()
    if ($actual -eq $case.Expected) {
        Write-Host ("[PASS] {0}: {1}" -f $case.Name, $actual) -ForegroundColor Green
    } else {
        Write-Host ("[FAIL] {0}: expected '{1}', actual '{2}'" -f $case.Name, $case.Expected, $actual) -ForegroundColor Red
        $failed++
    }
}

if ($failed -ne 0) {
    Write-Host ("SIMD PE ACCEPTANCE FAIL: {0} vaka başarısız." -f $failed) -ForegroundColor Red
    exit 1
}
Write-Host "SIMD PE ACCEPTANCE PASS: native dot, IQ2_S, Q1_0 ve F16 ABI generated PE içinde doğrulandı." -ForegroundColor Green
exit 0
