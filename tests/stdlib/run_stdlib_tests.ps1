<#
.SYNOPSIS
    run_stdlib_tests.ps1 - FAZ 5: Stdlib ve Koleksiyonlar Doğrulama Paketi
.DESCRIPTION
    F5.1 (Koleksiyonlar), F5.2 (Metin ve Unicode), F5.3 (OS ve sistem API'leri)
    ve F5.4 (Stable API/ABI) kapsamındaki tüm testleri verilen derleyici ile çalıştırır.
#>
param(
    [string]$Compiler = ".\build\bootstrap\stage3.exe",
    [string]$OutputJson = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $Compiler = [System.IO.Path]::GetFullPath($Compiler)
} else {
    $Compiler = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}

if (-not (Test-Path -LiteralPath $Compiler)) {
    Write-Host "HATA: Compiler bulunamadi: $Compiler" -ForegroundColor Red
    exit 1
}

$WORK = Join-Path $ROOT "build\test-work\stdlib-suite"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

$fixtures = @(
    @{ Name = "21_koleksiyon"; Path = "tests/stdlib/21_koleksiyon.ykn"; ExpectedOut = "Koleksiyon (Dinamik Dizi) testleri BASARILI!" },
    @{ Name = "30_faz5_stdlib_collections"; Path = "tests/stdlib/30_faz5_stdlib_collections.ykn"; ExpectedOut = "FAZ5_STDLIB_PASS" },
    @{ Name = "test_concat_minimal"; Path = "tests/test_concat_minimal.ykn"; ExpectedOut = "CONCAT-PASS" },
    @{ Name = "26_algebraic_enum"; Path = "tests/stdlib/26_algebraic_enum.ykn"; ExpectedOut = "Algebraic enum PASS" },
    @{ Name = "02_matematik"; Path = "tests/stdlib/02_matematik.ykn"; ExpectedOut = "std-matematik-PASS" }
)

Write-Host "`n=== FAZ 5: STDLIB VE PACKAGE ECOSYSTEM TEST PAKETI ===" -ForegroundColor Cyan
Write-Host "Compiler: $Compiler" -ForegroundColor Gray

$passed = 0
$failed = 0
$results = [System.Collections.Generic.List[object]]::new()

foreach ($f in $fixtures) {
    $srcPath = Join-Path $ROOT $f.Path
    $exePath = Join-Path $WORK ($f.Name + ".exe")
    Remove-Item -LiteralPath $exePath -Force -ErrorAction SilentlyContinue

    Write-Host -NoNewline ("  [{0}] derleniyor... " -f $f.Name)

    # 1. Derle
    $compileOut = & $Compiler $srcPath $exePath 2>&1 | Out-String
    $compileCode = $LASTEXITCODE

    if ($compileCode -ne 0 -or -not (Test-Path -LiteralPath $exePath)) {
        Write-Host "DERLEME HATASI" -ForegroundColor Red
        $failed++
        [void]$results.Add([PSCustomObject]@{
            Name = $f.Name
            Status = "COMPILE_FAIL"
            ExitCode = $compileCode
            Output = $compileOut
        })
        continue
    }

    # 2. Calistir
    $runOut = & $exePath 2>&1 | Out-String
    $runCode = $LASTEXITCODE

    $hasExpected = ($runOut -match [regex]::Escape($f.ExpectedOut))
    if ($runCode -eq 0 -and $hasExpected) {
        Write-Host "PASS" -ForegroundColor Green
        $passed++
        [void]$results.Add([PSCustomObject]@{
            Name = $f.Name
            Status = "PASS"
            ExitCode = $runCode
            Output = $runOut.Trim()
        })
    } else {
        Write-Host ("FAIL (exit=$runCode, expected '{0}')" -f $f.ExpectedOut) -ForegroundColor Red
        $failed++
        [void]$results.Add([PSCustomObject]@{
            Name = $f.Name
            Status = "RUN_FAIL"
            ExitCode = $runCode
            Output = $runOut.Trim()
        })
    }
}

Write-Host "`n------------------------------------------------------------" -ForegroundColor Cyan
Write-Host ("Sonuc: {0}/{1} test gecti ({2} basarisiz)" -f $passed, $fixtures.Count, $failed) -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })

if ($OutputJson -ne "") {
    $reportObj = [ordered]@{
        schema = "yukinary.stdlib-test-report.v1"
        compiler = $Compiler
        total = $fixtures.Count
        passed = $passed
        failed = $failed
        results = $results
    }
    $jsonPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $ROOT $OutputJson }
    New-Item -ItemType Directory -Path (Split-Path -Parent $jsonPath) -Force | Out-Null
    [System.IO.File]::WriteAllText($jsonPath, ($reportObj | ConvertTo-Json -Depth 5), (New-Object System.Text.UTF8Encoding($false)))
}

if ($failed -gt 0) {
    exit 1
}
exit 0
