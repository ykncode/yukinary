[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [int]$CompileTimeoutMs = 120000,
    [string]$OutputJson = "build/reports/future-generic-typechecker-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Work = Join-Path $Root "build\test-work\future-generic-typechecker"
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

function Invoke-FutureCase {
    param(
        [string]$CompilerRelative,
        [string]$FixtureRelative,
        [string]$StageName,
        [string]$ExpectedDiagnostic,
        [string]$ForbiddenDiagnostic,
        [bool]$ExpectArtifact,
        [int]$ExpectedRunExit = 0
    )
    $compilerPath = Root-Path $CompilerRelative
    $fixturePath = Root-Path $FixtureRelative
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) { throw "Compiler not found: $CompilerRelative" }
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) { throw "Fixture not found: $FixtureRelative" }

    $fixtureName = [IO.Path]::GetFileNameWithoutExtension($FixtureRelative)
    $stageSuffix = $StageName.ToLowerInvariant().Replace("-", "")
    $artifactRelative = "build/test-work/future-generic-typechecker/{0}-{1}.exe" -f $fixtureName, $stageSuffix
    $artifactPath = Root-Path $artifactRelative
    Remove-Item -LiteralPath $artifactPath -Force -ErrorAction SilentlyContinue

    $run = Invoke-CapturedProcess -FilePath $compilerPath -Arguments @($fixturePath, $artifactPath) -TimeoutMs $CompileTimeoutMs
    $output = ($run.stdout + "`n" + $run.stderr)
    $parserDiagnostic = ($output -match "PARSER ERROR|ERR_UNEXPECTED|ERR_INVALID_EXPRESSION|sozdizimi hatasi|Parser.*basarisiz")
    $expectedSeen = if ([string]::IsNullOrEmpty($ExpectedDiagnostic)) { $true } else { $output -match [regex]::Escape($ExpectedDiagnostic) }
    $forbiddenSeen = if ([string]::IsNullOrEmpty($ForbiddenDiagnostic)) { $false } else { $output -match [regex]::Escape($ForbiddenDiagnostic) }
    $artifactExists = Test-Path -LiteralPath $artifactPath -PathType Leaf
    $runExit = $null
    $executionPass = $true

    if ($ExpectArtifact -and $artifactExists) {
        $execution = Invoke-CapturedProcess -FilePath $artifactPath -Arguments @() -TimeoutMs 15000
        $runExit = $execution.exit_code
        $executionPass = (-not $execution.timed_out -and $execution.exit_code -eq $ExpectedRunExit)
    } elseif ($ExpectArtifact) {
        $executionPass = $false
    }

    [ordered]@{
        stage = $StageName
        compiler = $CompilerRelative
        compiler_sha256 = (Get-FileHash -LiteralPath $compilerPath -Algorithm SHA256).Hash.ToLowerInvariant()
        fixture = $FixtureRelative
        fixture_sha256 = (Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant()
        expected_diagnostic = $ExpectedDiagnostic
        forbidden_diagnostic = $ForbiddenDiagnostic
        expect_artifact = $ExpectArtifact
        artifact = $artifactRelative
        compile_exit_code = $run.exit_code
        compile_timed_out = $run.timed_out
        expected_seen = $expectedSeen
        forbidden_seen = $forbiddenSeen
        parser_diagnostic = $parserDiagnostic
        artifact_exists = $artifactExists
        execution_exit_code = $runExit
        execution_pass = $executionPass
        pass = (-not $run.timed_out -and $expectedSeen -and -not $forbiddenSeen -and -not $parserDiagnostic -and ($artifactExists -eq $ExpectArtifact) -and $executionPass)
        stdout = $run.stdout
        stderr = $run.stderr
    }
}

$cases = @(
    [ordered]@{ fixture = "tests/async/future_generic_source.ykn"; expected = ""; forbidden = "E044"; artifact = $true; run_exit = 1 },
    [ordered]@{ fixture = "tests/async/future_nested_generic_source.ykn"; expected = ""; forbidden = "E044"; artifact = $true; run_exit = 0 },
    [ordered]@{ fixture = "tests/async/future_generic_invalid_arity.ykn"; expected = "E045"; forbidden = "E043"; artifact = $false; run_exit = 0 },
    [ordered]@{ fixture = "tests/async/future_generic_unknown_payload.ykn"; expected = "E045"; forbidden = "E043"; artifact = $false; run_exit = 0 }
)
$stages = @(
    [ordered]@{ name = "Stage-0"; compiler = $CompilerStage0 },
    [ordered]@{ name = "Stage-3"; compiler = $CompilerStage3 }
)
$records = @()
foreach ($stage in $stages) {
    foreach ($case in $cases) {
        $records += ,(Invoke-FutureCase -CompilerRelative $stage.compiler -FixtureRelative $case.fixture -StageName $stage.name -ExpectedDiagnostic $case.expected -ForbiddenDiagnostic $case.forbidden -ExpectArtifact $case.artifact -ExpectedRunExit $case.run_exit)
    }
}
$verified = $true
foreach ($record in $records) {
    if (-not $record.pass) { $verified = $false }
}

$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.future-generic-typechecker-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "source-level Future<T>, nested Future<Future<T>>, explicit empty/multi-arity rejection, and unknown payload rejection"
    cases = @($cases | ForEach-Object { [ordered]@{ fixture = $_.fixture; expected = $_.expected; forbidden = $_.forbidden; expect_artifact = $_.artifact; sha256 = (Get-FileHash -LiteralPath (Root-Path $_.fixture) -Algorithm SHA256).Hash.ToLowerInvariant() } })
    records = @($records)
    case_count = $records.Count
    verified = $verified
} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Future generic typechecker evidence: $outPath"
Write-Host "Cases=$($records.Count) Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
