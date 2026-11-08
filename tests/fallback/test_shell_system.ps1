[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [string]$OutputJson = "build/reports/fallback_test_shell_system_report.json"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$CompilerPath = Join-Path $Root $Compiler
$Source = Join-Path $Root "tests/fallback/shell_system_acceptance.ykn"
$Work = Join-Path $Root "build/test-work/shell-system-acceptance"
$Executable = Join-Path $Work "shell-system-acceptance.exe"
$ReportPath = Join-Path $Root $OutputJson

if (-not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
    throw "Compiler executable not found: $CompilerPath"
}
New-Item -ItemType Directory -Path $Work -Force | Out-Null
Remove-Item -LiteralPath $Executable -Force -ErrorAction SilentlyContinue

& $CompilerPath $Source $Executable
$CompileExitCode = $LASTEXITCODE
if ($CompileExitCode -ne 0 -or -not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "YKN shell acceptance compilation failed (exit=$CompileExitCode)"
}

$StartInfo = New-Object System.Diagnostics.ProcessStartInfo
$StartInfo.FileName = $Executable
$StartInfo.WorkingDirectory = $Root
$StartInfo.UseShellExecute = $false
$StartInfo.CreateNoWindow = $true
$StartInfo.RedirectStandardOutput = $true
$StartInfo.RedirectStandardError = $true
$Process = New-Object System.Diagnostics.Process
$Process.StartInfo = $StartInfo
if (-not $Process.Start()) { throw "Could not start native shell acceptance executable" }
$StdoutTask = $Process.StandardOutput.ReadToEndAsync()
$StderrTask = $Process.StandardError.ReadToEndAsync()
if (-not $Process.WaitForExit(35000)) {
    $Process.Kill()
    $Process.WaitForExit()
    throw "Native shell acceptance timed out"
}
[System.Threading.Tasks.Task]::WaitAll(@($StdoutTask, $StderrTask), 5000) | Out-Null
$Run = [ordered]@{
    compile_exit_code = $CompileExitCode
    exit_code = $Process.ExitCode
    stdout = $StdoutTask.Result
    stderr = $StderrTask.Result
    executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Executable).Hash.ToLowerInvariant()
}
$Report = [ordered]@{
    schema = "yukinary.shell-system-native-acceptance.v1"
    verified = ($Run.exit_code -eq 0 -and $Run.stdout.Contains("YKN_SHELL_NATIVE_PASS"))
    target = "windows-x86_64"
    source = "tests/fallback/shell_system_acceptance.ykn"
    compiler = $CompilerPath
    run = $Run
}
New-Item -ItemType Directory -Path (Split-Path -Parent $ReportPath) -Force | Out-Null
$Report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
if (-not $Report.verified) {
    throw "Native shell acceptance failed: exit=$($Run.exit_code), stderr=$($Run.stderr)"
}
Write-Host "Shell system native acceptance PASS; report=$OutputJson"
