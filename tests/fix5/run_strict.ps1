<#!
FIX-5 strict typechecker Stage-1/Stage-3 parity gate.
Her negatif fixture beklenen diagnostic kodu ve konumuyla, pozitif fixture
ise iki compiler'da artifact üretimiyle doğrulanır.
#>
[CmdletBinding()]
param(
    [string]$CompilerA = "./build/bootstrap/stage1.exe",
    [string]$CompilerB = "./build/bootstrap/stage3.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Work = Join-Path $Root "build/test-work/fix5"
New-Item -ItemType Directory -Path $Work -Force | Out-Null

function Resolve-PathFromRoot([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
}

function Invoke-Compiler([string]$Compiler, [string]$Source, [string]$Output) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = Resolve-PathFromRoot $Compiler
    $psi.Arguments = '"' + (Resolve-PathFromRoot $Source) + '" "' + (Resolve-PathFromRoot $Output) + '"'
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
    $out = $p.StandardOutput.ReadToEnd()
    $err = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    return [PSCustomObject]@{ ExitCode = [int]$p.ExitCode; Text = $out + $err }
}

$negative = @(
    [PSCustomObject]@{ File="tests/fix5/01_wrong_argument.ykn"; Code="TC_TYPE_MISMATCH"; Line=5; Column=16 },
    [PSCustomObject]@{ File="tests/fix5/02_unknown_type.ykn"; Code="TC_UNDEFINED_SYMBOL"; Line=2; Column=14 },
    [PSCustomObject]@{ File="tests/fix5/03_pointer_depth.ykn"; Code="TC_INVALID_CONVERSION"; Line=3; Column=25 },
    [PSCustomObject]@{ File="tests/fix5/04_trait_violation.ykn"; Code="TC_TRAIT_VIOLATION"; Line=0; Column=0 },
    [PSCustomObject]@{ File="tests/fix5/05_recursive_long.ykn"; Code="TC_RECURSIVE_TYPE"; Line=1; Column=6 },
    [PSCustomObject]@{ File="tests/fix5/06_nominal_struct.ykn"; Code="TC_NOMINAL_MISMATCH"; Line=9; Column=27 }
)
$positive = @(
    [PSCustomObject]@{ File="tests/fix5/07_alias_valid.ykn"; Run=$false },
    [PSCustomObject]@{ File="tests/fix5/08_result_question_valid.ykn"; Run=$false },
    [PSCustomObject]@{ File="tests/fix5/09_imported_globals.ykn"; Run=$true }
)

$failures = 0
$compilerList = @(
    [PSCustomObject]@{ Name="Stage-1"; Path=$CompilerA },
    [PSCustomObject]@{ Name="Stage-3"; Path=$CompilerB }
)

foreach ($compiler in $compilerList) {
    $compilerPath = Resolve-PathFromRoot $compiler.Path
    if (-not (Test-Path -LiteralPath $compilerPath)) {
        Write-Host "[FAIL] $($compiler.Name) compiler bulunamadı: $compilerPath" -ForegroundColor Red
        $failures++
        continue
    }
    foreach ($case in $negative) {
        $outPath = Join-Path $Work ($compiler.Name + "-" + [System.IO.Path]::GetFileNameWithoutExtension($case.File) + ".exe")
        Remove-Item -LiteralPath $outPath -Force -ErrorAction SilentlyContinue
        $result = Invoke-Compiler $compiler.Path $case.File $outPath
        $codeMatch = $result.Text -match [regex]::Escape($case.Code)
        $locationMatch = $true
        if ($case.Line -gt 0) {
            $locationMatch = $result.Text -match (":{0}:{1}(?:[^0-9]|$)" -f $case.Line, $case.Column)
        }
        $artifactMatch = -not (Test-Path -LiteralPath $outPath)
        if ($result.ExitCode -eq 1 -and $codeMatch -and $locationMatch -and $artifactMatch) {
            Write-Host ("[PASS] {0} {1} ({2})" -f $compiler.Name, [System.IO.Path]::GetFileName($case.File), $case.Code) -ForegroundColor Green
        } else {
            Write-Host ("[FAIL] {0} {1}: exit={2}, code={3}, location={4}, artifact={5}" -f $compiler.Name, [System.IO.Path]::GetFileName($case.File), $result.ExitCode, $codeMatch, $locationMatch, (-not $artifactMatch)) -ForegroundColor Red
            $failures++
        }
        Remove-Item -LiteralPath $outPath -Force -ErrorAction SilentlyContinue
    }
    foreach ($source in $positive) {
        $outPath = Join-Path $Work ($compiler.Name + "-" + [System.IO.Path]::GetFileNameWithoutExtension($source.File) + ".exe")
        Remove-Item -LiteralPath $outPath -Force -ErrorAction SilentlyContinue
        $result = Invoke-Compiler $compiler.Path $source.File $outPath
        $artifactMatch = Test-Path -LiteralPath $outPath
        $runMatch = $true
        $runCode = 0
        if ($source.Run -and $artifactMatch) {
            & $outPath | Out-Null
            $runCode = [int]$LASTEXITCODE
            $runMatch = ($runCode -eq 0)
        }
        if ($result.ExitCode -eq 0 -and $artifactMatch -and $runMatch) {
            Write-Host ("[PASS] {0} {1} (valid{2})" -f $compiler.Name, [System.IO.Path]::GetFileName($source.File), $(if ($source.Run) { ", runtime" } else { "" })) -ForegroundColor Green
        } else {
            Write-Host ("[FAIL] {0} {1}: compile_exit={2}, artifact={3}, runtime_exit={4}" -f $compiler.Name, [System.IO.Path]::GetFileName($source.File), $result.ExitCode, $artifactMatch, $runCode) -ForegroundColor Red
            $failures++
        }
        Remove-Item -LiteralPath $outPath -Force -ErrorAction SilentlyContinue
    }
}

if ($failures -ne 0) {
    Write-Host ("FIX-5 STRICT FAIL: {0} case(s) failed" -f $failures) -ForegroundColor Red
    exit 1
}
Write-Host "FIX-5 STRICT PASS: Stage-1 ve Stage-3 negatif/pozitif parity doğrulandı." -ForegroundColor Green
exit 0
