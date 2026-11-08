param(
    [string]$Compiler = ".\build\bootstrap\stage0.exe"
)

$ErrorActionPreference = "Stop"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$WORK = Join-Path $ROOT "build\test-work\self-heal"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $compilerPath = $Compiler
} else {
    $compilerPath = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
if (-not (Test-Path -LiteralPath $compilerPath)) {
    Write-Host "Compiler not found: $compilerPath" -ForegroundColor Red
    exit 1
}

Write-Host "============================================================"
Write-Host "  YUKINARY FAZ 8.6 SELF-HEALING VALIDATION SUITE (PowerShell)"
Write-Host "============================================================"
Write-Host "  Compiler: $compilerPath"

$passed = 0
$total = 5

function Remove-TestArtifacts([string[]]$Paths) {
    Remove-Item -LiteralPath $Paths -Force -ErrorAction SilentlyContinue
}

function Run-WithTimeout($exePath, [int]$timeoutMs = 10000) {
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo.FileName = $exePath
    $p.StartInfo.WorkingDirectory = $ROOT
    $p.StartInfo.UseShellExecute = $false
    $p.StartInfo.RedirectStandardOutput = $true
    $p.StartInfo.RedirectStandardError = $true
    $p.StartInfo.CreateNoWindow = $true
    try {
        $null = $p.Start()
    } catch {
        return [PSCustomObject]@{ ExitCode = -1; Output = $_.Exception.Message; TimedOut = $false }
    }
    if ($p.WaitForExit($timeoutMs)) {
        $out = $p.StandardOutput.ReadToEnd() + $p.StandardError.ReadToEnd()
        return [PSCustomObject]@{ ExitCode = $p.ExitCode; Output = $out; TimedOut = $false }
    }
    try { $p.Kill() } catch {}
    return [PSCustomObject]@{ ExitCode = -1; Output = "TIMEOUT"; TimedOut = $true }
}

# Case 1: undefined variable without self-heal -> MUST FAIL
$out1 = Join-Path $WORK "undefined-no-heal.exe"
Remove-TestArtifacts @($out1)
$c1Out = & $compilerPath "tests\negative\01_undefined_var.ykn" $out1 2>&1
$c1Code = $LASTEXITCODE
if ($c1Code -ne 0 -and -not (Test-Path -LiteralPath $out1)) {
    Write-Host "[PASS] Undefined var without --self-heal: Correctly aborted" -ForegroundColor Green
    $passed++
} else {
    Write-Host "[FAIL] Undefined var without --self-heal should have aborted!" -ForegroundColor Red
}
Remove-TestArtifacts @($out1)

# Case 2: undefined variable with self-heal -> MUST PASS
$out2 = Join-Path $WORK "undefined-self-heal.exe"
Remove-TestArtifacts @($out2)
$c2Out = & $compilerPath "--self-heal" "tests\negative\01_undefined_var.ykn" $out2 2>&1
$c2Code = $LASTEXITCODE
if ($c2Code -eq 0 -and (Test-Path -LiteralPath $out2)) {
    $res = Run-WithTimeout $out2
    if ($res.ExitCode -eq 0 -and ($c2Out -join " ").Contains("[AI] Self-heal")) {
        Write-Host "[PASS] Undefined var with --self-heal: Successfully healed and executed" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "[FAIL] Healed binary execution failed with code $($res.ExitCode)" -ForegroundColor Red
    }
} else {
    Write-Host "[FAIL] Undefined var with --self-heal failed to compile: $($c2Out -join ' ')" -ForegroundColor Red
}
Remove-TestArtifacts @($out2)

# Case 3: multiple undefined variables with --kendini-onar -> MUST PASS
$out3 = Join-Path $WORK "multi-self-heal.exe"
Remove-TestArtifacts @($out3)
$c3Out = & $compilerPath "--kendini-onar" "tests\negative\04_multi_undefined_var.ykn" $out3 2>&1
$c3Code = $LASTEXITCODE
if ($c3Code -eq 0 -and (Test-Path -LiteralPath $out3)) {
    $res3 = Run-WithTimeout $out3
    if ($res3.ExitCode -eq 0 -and $res3.Output.Contains("MULTI-SELF-HEAL-PASS")) {
        Write-Host "[PASS] Multi undefined var with --kendini-onar: Healed multi symbols and passed" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "[FAIL] Multi-healed binary output mismatch: $($res3.Output)" -ForegroundColor Red
    }
} else {
    Write-Host "[FAIL] Multi undefined var compilation failed: $($c3Out -join ' ')" -ForegroundColor Red
}
Remove-TestArtifacts @($out3)

# Case 4: type mismatch with self-heal -> MUST STILL FAIL
$out4 = Join-Path $WORK "type-mismatch.exe"
Remove-TestArtifacts @($out4)
$c4Out = & $compilerPath "--self-heal" "tests\negative\02_type_mismatch.ykn" $out4 2>&1
$c4Code = $LASTEXITCODE
if ($c4Code -ne 0 -and -not (Test-Path -LiteralPath $out4)) {
    Write-Host "[PASS] Type mismatch with --self-heal: Correctly rejected (unhealable safety barrier)" -ForegroundColor Green
    $passed++
} else {
    Write-Host "[FAIL] Type mismatch should not be healed!" -ForegroundColor Red
}
Remove-TestArtifacts @($out4)

# Case 5: invalid return with self-heal -> MUST STILL FAIL
$out5 = Join-Path $WORK "invalid-return.exe"
Remove-TestArtifacts @($out5)
$c5Out = & $compilerPath "--self-heal" "tests\negative\03_invalid_return.ykn" $out5 2>&1
$c5Code = $LASTEXITCODE
if ($c5Code -ne 0 -and -not (Test-Path -LiteralPath $out5)) {
    Write-Host "[PASS] Invalid return with --self-heal: Correctly rejected (unhealable safety barrier)" -ForegroundColor Green
    $passed++
} else {
    Write-Host "[FAIL] Invalid return should not be healed!" -ForegroundColor Red
}
Remove-TestArtifacts @($out5)

Write-Host "------------------------------------------------------------"
Write-Host "Results: $passed/$total tests passed"
if ($passed -eq $total) {
    Write-Host "ALL SELF-HEALING TESTS PASSED! [OK]" -ForegroundColor Green
    exit 0
}
Write-Host "SOME SELF-HEALING TESTS FAILED!" -ForegroundColor Red
exit 1
