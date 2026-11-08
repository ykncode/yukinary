[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [int]$CompileTimeoutMs = 120000,
    [int]$RuntimeTimeoutMs = 15000,
    [string]$OutputJson = "build/reports/async-future-release-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Work = Join-Path $Root "build\test-work\async-future-release"
New-Item -ItemType Directory -Path $Work -Force | Out-Null

function Root-Path([string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative)) { return [IO.Path]::GetFullPath($Relative) }
    return [IO.Path]::GetFullPath((Join-Path $Root ($Relative -replace "/", "\")))
}

function Quote-Argument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-CapturedProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [int]$TimeoutMs,
        [string]$WorkingDirectory = $Root
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join " ")
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    if (-not $p.Start()) { throw "Process could not start: $FilePath" }
    $outTask = $p.StandardOutput.ReadToEndAsync()
    $errTask = $p.StandardError.ReadToEndAsync()
    $timedOut = -not $p.WaitForExit($TimeoutMs)
    if ($timedOut) {
        try { $p.Kill() } catch {}
        $p.WaitForExit()
    }
    [System.Threading.Tasks.Task]::WaitAll(@($outTask, $errTask), 5000) | Out-Null
    [ordered]@{
        exit_code = if ($timedOut) { $null } else { $p.ExitCode }
        timed_out = $timedOut
        stdout = $outTask.Result
        stderr = $errTask.Result
    }
}

$source = Root-Path "tests/stdlib/async_future_runtime.ykn"
$stages = @(
    [ordered]@{ name = "Stage-0"; compiler = (Root-Path $CompilerStage0) },
    [ordered]@{ name = "Stage-3"; compiler = (Root-Path $CompilerStage3) }
)
$records = New-Object System.Collections.Generic.List[object]

foreach ($stage in $stages) {
    foreach ($target in @("windows", "linux")) {
        $extension = if ($target -eq "windows") { ".exe" } else { ".elf" }
        $artifact = Join-Path $Work (("{0}-async-future{1}" -f $stage.name.ToLowerInvariant(), $extension))
        Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue
        if ($target -eq "linux") {
            $compileArgs = @("--target", "linux", $source, $artifact)
        } else {
            $compileArgs = @($source, $artifact)
        }
        $compile = Invoke-CapturedProcess -FilePath $stage.compiler -Arguments $compileArgs -TimeoutMs $CompileTimeoutMs
        $exists = Test-Path -LiteralPath $artifact -PathType Leaf
        $pass = ($compile.exit_code -eq 0 -and -not $compile.timed_out -and $exists)
        $records.Add([ordered]@{
            kind = "compile"; stage = $stage.name; target = $target; source = "tests/stdlib/async_future_runtime.ykn"
            artifact = $artifact.Substring($Root.Length + 1).Replace("\", "/")
            exit_code = $compile.exit_code; timed_out = $compile.timed_out; artifact_exists = $exists; pass = $pass
            stdout = $compile.stdout; stderr = $compile.stderr
        })
    }
}

foreach ($stage in $stages) {
    foreach ($target in @("windows", "linux")) {
        $extension = if ($target -eq "windows") { ".exe" } else { ".elf" }
        $artifact = Join-Path $Work (("{0}-async-future{1}" -f $stage.name.ToLowerInvariant(), $extension))
        $compileRow = @($records | Where-Object { $_.kind -eq "compile" -and $_.stage -eq $stage.name -and $_.target -eq $target })[0]
        if (-not $compileRow.pass) {
            $records.Add([ordered]@{ kind = "runtime"; stage = $stage.name; target = $target; pass = $false; skipped = $true; error = "compile failed" })
            continue
        }
        if ($target -eq "windows") {
            $run = Invoke-CapturedProcess -FilePath $artifact -Arguments @() -TimeoutMs $RuntimeTimeoutMs
        } else {
            $dockerRoot = $Root.Replace("\", "/")
            $containerArtifact = "/work/" + $artifact.Substring($Root.Length + 1).Replace("\", "/")
            $command = "chmod +x '$containerArtifact'; exec '$containerArtifact'"
            $dockerArgs = @("run", "--rm", "--platform", "linux/amd64", "-v", ($dockerRoot + ":/work"), "-w", "/work", "ubuntu:24.04", "bash", "-lc", $command)
            $run = Invoke-CapturedProcess -FilePath "docker" -Arguments $dockerArgs -TimeoutMs $RuntimeTimeoutMs
        }
        $records.Add([ordered]@{
            kind = "runtime"; stage = $stage.name; target = if ($target -eq "linux") { "linux-docker-ubuntu-24.04" } else { "windows-native" }
            expected_exit_code = 0; exit_code = $run.exit_code; timed_out = $run.timed_out
            pass = ($run.exit_code -eq 0 -and -not $run.timed_out); stdout = $run.stdout; stderr = $run.stderr
        })
    }
}

$failed = @($records | Where-Object { -not $_.pass })
$verified = ($failed.Count -eq 0)
$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $outPath) -Force | Out-Null
[ordered]@{
    schema = "yukinary.async-future-release-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "Real saf-YKN callback Future poll contract and bounded cooperative scheduler on Windows PE and Linux x86_64 ELF in Docker Ubuntu 24.04."
    source = "tests/stdlib/async_future_runtime.ykn"
    records = $records
    failed_cases = $failed.Count
    verified = $verified
} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outPath -Encoding UTF8
Write-Host "Async/Future release evidence: $outPath"
Write-Host "Records=$($records.Count) Failed=$($failed.Count) Verified=$verified"
if (-not $verified) {
    $failed | ForEach-Object {
        $detail = if ($_.PSObject.Properties.Name -contains "error") { $_.error } else { "compile_or_runtime_verification_failed" }
        Write-Host ("  FAIL: " + $_.stage + " " + $_.target + " " + $detail) -ForegroundColor Red
    }
    exit 1
}
exit 0
