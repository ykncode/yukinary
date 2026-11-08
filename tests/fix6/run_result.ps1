<#
FIX-6 canonical Result/Option Stage-1/Stage-3 gate.
Pozitif fixture'lar artifact + runtime exit kodunu; negatif fixture artifact
uretilmemesini ve TC_TYPE_MISMATCH sonucu vermesini doğrular.
#>
[CmdletBinding()]
param(
    [string]$CompilerA = "./build/bootstrap/stage1.exe",
    [string]$CompilerB = "./build/bootstrap/stage3.exe"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Work = Join-Path $Root "build/test-work/fix6-result"
New-Item -ItemType Directory -Path $Work -Force | Out-Null

$positive = @(
    @{ File="tests/fix6/09_result_question.ykn"; Exit=42 },
    @{ File="tests/fix6/10_result_question_error.ykn"; Exit=7 },
    @{ File="tests/fix6/12_result_alias.ykn"; Exit=42 },
    @{ File="tests/fix6/13_option_canonical.ykn"; Exit=42 },
    @{ File="tests/fix6/14_result_option_together.ykn"; Exit=42 }
)
$negative = @(
    @{ File="tests/fix6/11_result_question_invalid_operand.ykn" }
)

function Resolve-Project([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
}

function Invoke-Compiler([string]$Compiler, [string]$Source, [string]$Output) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = Resolve-Project $Compiler
    $psi.Arguments = '"' + (Resolve-Project $Source) + '" "' + (Resolve-Project $Output) + '"'
    $psi.WorkingDirectory = $Root
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = New-Object System.Text.UTF8Encoding($false)
    $psi.StandardErrorEncoding = New-Object System.Text.UTF8Encoding($false)
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    if (-not $p.Start()) { throw "Compiler başlatılamadı: $Compiler" }
    $text = $p.StandardOutput.ReadToEnd() + $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    return [PSCustomObject]@{ ExitCode=[int]$p.ExitCode; Text=$text }
}

$failures = 0
$compilers = @(
    @{ Name="Stage-1"; Path=$CompilerA },
    @{ Name="Stage-3"; Path=$CompilerB }
)

foreach ($compiler in $compilers) {
    foreach ($case in $positive) {
        $output = Join-Path $Work ($compiler.Name + "-" + [System.IO.Path]::GetFileNameWithoutExtension($case.File) + ".exe")
        Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
        $result = Invoke-Compiler $compiler.Path $case.File $output
        $artifact = Test-Path -LiteralPath $output
        $runtime = -1
        if ($artifact) {
            & $output | Out-Null
            $runtime = [int]$LASTEXITCODE
        }
        if ($result.ExitCode -eq 0 -and $artifact -and $runtime -eq [int]$case.Exit) {
            Write-Host ("[PASS] {0} {1} -> {2}" -f $compiler.Name, $case.File, $runtime) -ForegroundColor Green
        } else {
            Write-Host ("[FAIL] {0} {1}: compile={2}, artifact={3}, runtime={4}" -f $compiler.Name, $case.File, $result.ExitCode, $artifact, $runtime) -ForegroundColor Red
            $failures++
        }
        Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
    }

    foreach ($case in $negative) {
        $output = Join-Path $Work ($compiler.Name + "-invalid-question.exe")
        Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
        $result = Invoke-Compiler $compiler.Path $case.File $output
        $artifact = Test-Path -LiteralPath $output
        if ($result.ExitCode -eq 1 -and -not $artifact -and $result.Text -match "TC_TYPE_MISMATCH") {
            Write-Host ("[PASS] {0} {1} -> invalid operand rejected" -f $compiler.Name, $case.File) -ForegroundColor Green
        } else {
            Write-Host ("[FAIL] {0} {1}: compile={2}, artifact={3}, diagnostic={4}" -f $compiler.Name, $case.File, $result.ExitCode, $artifact, ($result.Text -match "TC_TYPE_MISMATCH")) -ForegroundColor Red
            $failures++
        }
        Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
    }
}

if ($failures -ne 0) {
    Write-Host ("FIX-6 RESULT FAIL: {0} case(s) failed" -f $failures) -ForegroundColor Red
    exit 1
}
Write-Host "FIX-6 RESULT PASS: canonical Result/Option ABI Stage-1/Stage-3 parity doğrulandı." -ForegroundColor Green
exit 0
