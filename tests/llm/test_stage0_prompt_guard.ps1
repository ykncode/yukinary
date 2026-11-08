<##
.SYNOPSIS
    Stage-0 #prompt sahte üretim regression testi.
.DESCRIPTION
    Seed compiler'da keyword tabanlı Fibonacci/faktöriyel gibi sabit cevapların
    artık derlenebilir YKN olarak enjekte edilmediğini doğrular. Bu test gerçek
    inference başarı testi değildir; inference yoksa açık hata verilmesini zorunlu
    kılar ve sahte başarıyı engeller.
#>
[CmdletBinding()]
param(
    [int]$TimeoutMs = 120000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$work = Join-Path $ROOT "build\test-work\stage0-prompt-guard"
New-Item -ItemType Directory -Path $work -Force | Out-Null
$compiler = Join-Path $work "stage0-prompt-guard.exe"
$source = Join-Path $work "keyword-prompt.ykn"
$artifact = Join-Path $work "keyword-prompt-output.exe"

$gpp = Get-Command g++ -ErrorAction SilentlyContinue
$gppPath = if ($null -ne $gpp) { $gpp.Source } else {
    $known = "C:\Users\naitr\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"
    if (Test-Path -LiteralPath $known) { $known } else { throw "g++ bulunamadı" }
}

& $gppPath -std=c++17 -O2 -pthread -static (Join-Path $ROOT "app_full.cpp") -lws2_32 -o $compiler
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $compiler)) {
    throw "Stage-0 seed derlenemedi"
}

@'
#prompt "fibonacci hesaplayan fonksiyon yaz"
isv ana(arguman_sayisi: tamsayi, argumanlar: **u8) -> tamsayi {
    dondu 0;
}
'@ | Set-Content -LiteralPath $source -Encoding UTF8
Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $compiler
$psi.Arguments = '"' + $source.Replace('"', '\"') + '" "' + $artifact.Replace('"', '\"') + '"'
$psi.WorkingDirectory = $ROOT
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$p = New-Object System.Diagnostics.Process
$p.StartInfo = $psi
[void]$p.Start()
$outTask = $p.StandardOutput.ReadToEndAsync()
$errTask = $p.StandardError.ReadToEndAsync()
$timedOut = -not $p.WaitForExit($TimeoutMs)
if ($timedOut) { try { $p.Kill() } catch {} }
$p.WaitForExit()
$stdout = if ($outTask.IsCompleted) { $outTask.Result } else { "" }
$stderr = if ($errTask.IsCompleted) { $errTask.Result } else { "" }

if ($timedOut) { throw "Stage-0 #prompt guard timeout verdi" }
if ($p.ExitCode -eq 0) { throw "#prompt sahte/başarılı kod üretmeye devam ediyor" }
if (-not $stdout.Contains("sahte kod uretilmeyecek")) {
    Write-Host $stdout
    Write-Host $stderr
    throw "#prompt için gerçek inference yok hatası gözlenmedi"
}
if (Test-Path -LiteralPath $artifact) { throw "Reddedilen prompt artifact üretti" }

# A model flag must not downgrade an invalid/unreadable model into a prompt
# success. This validates the selected-model failure boundary independently.
$invalidArtifact = Join-Path $work "invalid-model-output.exe"
Remove-Item -LiteralPath $invalidArtifact -Force -ErrorAction SilentlyContinue
$invalidModel = Join-Path $work "missing-model.gguf"
$psi.Arguments = '"--llm-model" "' + $invalidModel.Replace('"', '\"') + '" "' + $source.Replace('"', '\"') + '" "' + $invalidArtifact.Replace('"', '\"') + '"'
$p = New-Object System.Diagnostics.Process
$p.StartInfo = $psi
[void]$p.Start()
$invalidOutTask = $p.StandardOutput.ReadToEndAsync()
$invalidErrTask = $p.StandardError.ReadToEndAsync()
$invalidTimedOut = -not $p.WaitForExit($TimeoutMs)
if ($invalidTimedOut) { try { $p.Kill() } catch {} }
$p.WaitForExit()
$invalidStdout = if ($invalidOutTask.IsCompleted) { $invalidOutTask.Result } else { "" }
$invalidStderr = if ($invalidErrTask.IsCompleted) { $invalidErrTask.Result } else { "" }
if ($invalidTimedOut) { throw "Geçersiz GGUF model guard timeout verdi" }
if ($p.ExitCode -eq 0) {
    Write-Host $invalidStdout
    Write-Host $invalidStderr
    throw "Geçersiz --llm-model başarılı kabul edildi"
}
if (-not $invalidStdout.Contains("GGUF v3")) {
    Write-Host $invalidStdout
    Write-Host $invalidStderr
    throw "Geçersiz model için GGUF doğrulama diagnostici gözlenmedi"
}
if (Test-Path -LiteralPath $invalidArtifact) { throw "Geçersiz model artifact üretti" }
Write-Host "[PASS] STAGE0_PROMPT_GUARD_PASS"
exit 0
