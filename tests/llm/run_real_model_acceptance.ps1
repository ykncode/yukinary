<##
.SYNOPSIS
    Gerçek GGUF model session kabul testi.
.DESCRIPTION
    BitNet ve Bonsai GGUF modelleri ile --llm-model yolunu doğrular.
    Bu test metadata/session oluşturma ve üretilen binary çalıştırmasını kapsar;
    tam numeric transformer parity testi değildir.
#>
[CmdletBinding()]
param(
    [string]$Compiler = ".\build\bootstrap\stage3.exe",
    [int]$TimeoutMs = 180000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$compilerPath = if ([System.IO.Path]::IsPathRooted($Compiler)) {
    [System.IO.Path]::GetFullPath($Compiler)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
$sourcePath = Join-Path $ROOT "tests\mini_hello.ykn"
$work = Join-Path $ROOT "build\test-work\llm-real-model"
New-Item -ItemType Directory -Path $work -Force | Out-Null

if (-not (Test-Path -LiteralPath $compilerPath)) { throw "Compiler bulunamadı: $compilerPath" }
if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Smoke kaynak bulunamadı: $sourcePath" }

function Quote-ProcessArg([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-CheckedProcess([string]$File, [string[]]$Arguments, [string]$Name) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $File
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-ProcessArg ([string]$_) }) -join " ")
    $psi.WorkingDirectory = $ROOT
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = New-Object System.Text.UTF8Encoding($false)
    $psi.StandardErrorEncoding = New-Object System.Text.UTF8Encoding($false)
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()
    $stdoutTask = $p.StandardOutput.ReadToEndAsync()
    $stderrTask = $p.StandardError.ReadToEndAsync()
    $timedOut = -not $p.WaitForExit($TimeoutMs)
    if ($timedOut) { try { $p.Kill() } catch {} }
    $p.WaitForExit()
    try { [System.Threading.Tasks.Task]::WaitAll(@($stdoutTask, $stderrTask), 5000) | Out-Null } catch {}
    return [PSCustomObject]@{
        Name = $Name
        ExitCode = if ($timedOut) { -99 } else { $p.ExitCode }
        TimedOut = $timedOut
        Stdout = if ($stdoutTask.IsCompleted) { $stdoutTask.Result } else { "" }
        Stderr = if ($stderrTask.IsCompleted) { $stderrTask.Result } else { "" }
    }
}

$cases = @(
    @{ Name = "bitnet"; Model = "models\bitnet-b1.58-2B-4T\ggml-model-i2_s.gguf" },
    @{ Name = "bonsai-qwen"; Model = "models\bonsai-8b\Bonsai-8B.gguf" }
)
$failed = 0
foreach ($case in $cases) {
    $modelPath = Join-Path $ROOT $case.Model
    $outputPath = Join-Path $work ($case.Name + ".exe")
    if (-not (Test-Path -LiteralPath $modelPath)) {
        Write-Host ("[SKIP] {0}: model bulunamadı: {1}" -f $case.Name, $modelPath) -ForegroundColor Yellow
        continue
    }
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
    $result = Invoke-CheckedProcess $compilerPath @(
        "--llm-model", $modelPath, $sourcePath, $outputPath
    ) ("compile-" + $case.Name)
    $accepted = ($result.ExitCode -eq 0 -and -not $result.TimedOut -and
        (Test-Path -LiteralPath $outputPath) -and
        $result.Stdout.Contains("LLM modeli yuklendi") -and
        $result.Stdout.Contains("Derleme tamamlandi"))
    if (-not $accepted) {
        $failed++
        Write-Host ("[FAIL] {0}: session başlatma/derleme başarısız (exit={1})" -f $case.Name, $result.ExitCode) -ForegroundColor Red
        Write-Host $result.Stdout
        Write-Host $result.Stderr
        continue
    }
    $run = Invoke-CheckedProcess $outputPath @() ("run-" + $case.Name)
    $ran = ($run.ExitCode -eq 0 -and -not $run.TimedOut -and $run.Stdout.Trim() -eq "merhaba")
    if (-not $ran) {
        $failed++
        Write-Host ("[FAIL] {0}: üretilen binary çalışmadı (exit={1})" -f $case.Name, $run.ExitCode) -ForegroundColor Red
        Write-Host $run.Stdout
        Write-Host $run.Stderr
        continue
    }
    Write-Host ("[PASS] {0}: GGUF session + unrelated hello binary smoke (NOT inference)" -f $case.Name) -ForegroundColor Green
}

if ($failed -gt 0) {
    Write-Host ("REAL MODEL ACCEPTANCE FAIL: {0} vaka başarısız." -f $failed) -ForegroundColor Red
    exit 1
}
Write-Host "GGUF SESSION SMOKE PASS only: no model token generation or inference was tested." -ForegroundColor Yellow
exit 0
