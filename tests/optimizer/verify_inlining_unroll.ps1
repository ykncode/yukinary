[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [int]$CompileTimeoutMs = 120000,
    [int]$RuntimeTimeoutMs = 15000,
    [string]$OutputJson = "build/reports/optimizer-inlining-unroll-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Work = Join-Path $Root "build\test-work\optimizer-inlining-unroll"
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
        [int]$TimeoutMs
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join " ")
    $psi.WorkingDirectory = $Root
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    if (-not $process.Start()) { throw "Process could not start: $FilePath" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($TimeoutMs)
    if ($timedOut) {
        try { $process.Kill() } catch {}
        $process.WaitForExit()
    }
    [System.Threading.Tasks.Task]::WaitAll(@($stdoutTask, $stderrTask), 5000) | Out-Null
    [ordered]@{
        exit_code = if ($timedOut) { $null } else { $process.ExitCode }
        timed_out = $timedOut
        stdout = $stdoutTask.Result
        stderr = $stderrTask.Result
    }
}

function Invoke-OptimizerCase([string]$CompilerRelative, [string]$FixtureRelative, [string]$StageName) {
    $compilerPath = Root-Path $CompilerRelative
    $fixturePath = Root-Path $FixtureRelative
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) { throw "Compiler not found: $CompilerRelative" }
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) { throw "Fixture not found: $FixtureRelative" }
    $fixtureName = [IO.Path]::GetFileNameWithoutExtension($FixtureRelative)
    $artifactRelative = "build/test-work/optimizer-inlining-unroll/{0}-{1}.exe" -f $fixtureName, $StageName.ToLowerInvariant()
    $artifactPath = Root-Path $artifactRelative
    Remove-Item -LiteralPath $artifactPath -Force -ErrorAction SilentlyContinue

    # 1. Derleme
    $compileRun = Invoke-CapturedProcess -FilePath $compilerPath -Arguments @($fixturePath, $artifactPath) -TimeoutMs $CompileTimeoutMs
    $artifactExists = Test-Path -LiteralPath $artifactPath -PathType Leaf
    $compilePass = ($compileRun.exit_code -eq 0 -and -not $compileRun.timed_out -and $artifactExists)

    # 2. Çalıştırma
    $runExitCode = $null
    $runTimedOut = $false
    $runStdout = ""
    $runStderr = ""
    $runPass = $false
    if ($compilePass) {
        $execRun = Invoke-CapturedProcess -FilePath $artifactPath -Arguments @() -TimeoutMs $RuntimeTimeoutMs
        $runExitCode = $execRun.exit_code
        $runTimedOut = $execRun.timed_out
        $runStdout = $execRun.stdout
        $runStderr = $execRun.stderr
        $runPass = ($runExitCode -eq 0 -and -not $runTimedOut)
    }

    [ordered]@{
        stage = $StageName
        compiler = $CompilerRelative
        fixture = $FixtureRelative
        artifact = $artifactRelative
        compile_exit = $compileRun.exit_code
        artifact_exists = $artifactExists
        compile_pass = $compilePass
        run_exit_code = $runExitCode
        run_timed_out = $runTimedOut
        run_pass = $runPass
        pass = ($compilePass -and $runPass)
        stdout = $runStdout
        stderr = $runStderr
    }
}

$fixture = "tests/optimizer/test_inlining_and_unroll.ykn"
$stages = @(
    [ordered]@{ name = "Stage-0"; compiler = $CompilerStage0 },
    [ordered]@{ name = "Stage-3"; compiler = $CompilerStage3 }
)

$records = @()
foreach ($stage in $stages) {
    $records += ,(Invoke-OptimizerCase -CompilerRelative $stage.compiler -FixtureRelative $fixture -StageName $stage.name)
}

$verified = $true
foreach ($record in $records) {
    if (-not $record.pass) { $verified = $false }
}

$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.optimizer-inlining-unroll.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "Verification of function inlining (parameter substitution) and loop unrolling in YKN optimizer pipeline"
    fixture = $fixture
    records = @($records)
    case_count = $records.Count
    verified = $verified
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Optimizer inlining & unroll evidence: $outPath"
Write-Host "Cases=$($records.Count) Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
