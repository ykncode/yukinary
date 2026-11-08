<#
.SYNOPSIS
    run_ir_tests.ps1 - FAZ 4: Typed IR, Codegen ve ABI Doğrulama Paketi
.DESCRIPTION
    F4.1 (Typed IR), F4.2 (ABI ve layout) ve F4.3 (Codegen güvenliği)
    kapsamındaki tüm test fixture'larını verilen derleyici ile çalıştırır.
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

$WORK = Join-Path $ROOT "build\test-work\ir-suite"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

$fixtures = @(
    @{ Name = "01_ir_verifier_temel"; Path = "tests/ir/01_ir_verifier_temel.ykn"; ExpectedOut = "IR_VERIFIER_PASS" },
    @{ Name = "02_ir_dump"; Path = "tests/ir/02_ir_dump.ykn"; ExpectedOut = "IR_DUMP_PASS" },
    @{ Name = "03_ir_parity"; Path = "tests/ir/03_ir_parity.ykn"; ExpectedOut = "IR_PARITY_PASS" },
    @{ Name = "04_ir_verifier_negatif"; Path = "tests/ir/04_ir_verifier_negatif.ykn"; ExpectedOut = "IR_VERIFIER_NEGATIF_PASS" },
    @{ Name = "05_mir_to_ir"; Path = "tests/ir/05_mir_to_ir.ykn"; ExpectedOut = "MIR_TO_IR_PASS" },
    @{ Name = "06_ir_relocation"; Path = "tests/ir/06_ir_relocation.ykn"; ExpectedOut = "IR_RELOCATION_PASS" },
    @{ Name = "abi_pointer_endianness"; Path = "tests/abi/test_pointer_endianness.ykn"; ExpectedOut = "ABI_POINTER_ENDIANNESS_PASS" },
    @{ Name = "01_regalloc_yogun"; Path = "tests/codegen/01_regalloc_yogun.ykn"; ExpectedOut = "REGALLOC-PASS" },
    @{ Name = "02_large_struct_offset"; Path = "tests/codegen/02_large_struct_offset.ykn"; ExpectedOut = "LARGE_STRUCT_OFFSET_PASS" },
    @{ Name = "03_unary_expressions"; Path = "tests/codegen/03_unary_expressions.ykn"; ExpectedOut = "UNARY_EXPRESSIONS_PASS" },
    @{ Name = "07_float_ir_test"; Path = "tests/ir/07_float_ir_test.ykn"; ExpectedOut = "FLOAT_IR_PASS" },
    @{ Name = "08_hir_islev_test"; Path = "tests/ir/08_hir_islev_test.ykn"; ExpectedOut = "HIR_ISLEV_PASS" },
    @{ Name = "09_mir_cfg_branch"; Path = "tests/ir/09_mir_cfg_branch.ykn"; ExpectedOut = "MIR_CFG_PASS" },
    @{ Name = "10_pipeline_and_features"; Path = "tests/ir/10_pipeline_and_features.ykn"; ExpectedOut = "TUM_TESTLER_BASARILI" }
)

Write-Host "`n=== FAZ 4: TYPED IR, CODEGEN VE ABI TEST PAKETI ===" -ForegroundColor Cyan
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
        schema = "yukinary.ir-test-report.v1"
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
