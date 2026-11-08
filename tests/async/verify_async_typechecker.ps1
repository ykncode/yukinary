[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [int]$CompileTimeoutMs = 120000,
    [string]$OutputJson = "build/reports/async-typechecker-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Work = Join-Path $Root "build\test-work\async-typechecker"
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

function Invoke-TypeCase(
    [string]$CompilerRelative,
    [string]$FixtureRelative,
    [string]$StageName,
    [string]$ExpectedDiagnostic,
    [string]$ForbiddenDiagnostic
) {
    $compilerPath = Root-Path $CompilerRelative
    $fixturePath = Root-Path $FixtureRelative
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) { throw "Compiler not found: $CompilerRelative" }
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) { throw "Fixture not found: $FixtureRelative" }
    $fixtureName = [IO.Path]::GetFileNameWithoutExtension($FixtureRelative)
    $artifactRelative = "build/test-work/async-typechecker/{0}-{1}.exe" -f $fixtureName, $StageName.ToLowerInvariant()
    $artifactPath = Root-Path $artifactRelative
    Remove-Item -LiteralPath $artifactPath -Force -ErrorAction SilentlyContinue
    $run = Invoke-CapturedProcess -FilePath $compilerPath -Arguments @($fixturePath, $artifactPath) -TimeoutMs $CompileTimeoutMs
    $output = ($run.stdout + "`n" + $run.stderr)
    $parserDiagnostic = ($output -match "PARSER ERROR|ERR_UNEXPECTED|ERR_INVALID_EXPRESSION|ERR_ASYNC_FUNCTION_KEYWORD_EXPECTED|sozdizimi hatasi|Parser.*basarisiz")
    $expectedSeen = ($output -match [regex]::Escape($ExpectedDiagnostic))
    $forbiddenSeen = ($output -match [regex]::Escape($ForbiddenDiagnostic))
    $artifactExists = Test-Path -LiteralPath $artifactPath -PathType Leaf
    [ordered]@{
        stage = $StageName
        compiler = $CompilerRelative
        compiler_sha256 = (Get-FileHash -LiteralPath $compilerPath -Algorithm SHA256).Hash.ToLowerInvariant()
        fixture = $FixtureRelative
        fixture_sha256 = (Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant()
        expected_diagnostic = $ExpectedDiagnostic
        forbidden_diagnostic = $ForbiddenDiagnostic
        artifact = $artifactRelative
        exit_code = $run.exit_code
        timed_out = $run.timed_out
        expected_seen = $expectedSeen
        forbidden_seen = $forbiddenSeen
        parser_diagnostic = $parserDiagnostic
        artifact_exists = $artifactExists
        pass = ($run.exit_code -ne 0 -and -not $run.timed_out -and $expectedSeen -and -not $forbiddenSeen -and -not $parserDiagnostic -and -not $artifactExists)
        stdout = $run.stdout
        stderr = $run.stderr
    }
}

$cases = @(
    [ordered]@{
        fixture = "tests/async/await_non_future_type_error.ykn"
        expected = "E044"
        forbidden = "E043"
    },
    [ordered]@{
        fixture = "tests/async/await_outside_async_type_error.ykn"
        expected = "E044"
        forbidden = "E043"
    }
)
$stages = @(
    [ordered]@{ name = "Stage-0"; compiler = $CompilerStage0 },
    [ordered]@{ name = "Stage-3"; compiler = $CompilerStage3 }
)
$records = @()
foreach ($stage in $stages) {
    foreach ($case in $cases) {
        $records += ,(Invoke-TypeCase -CompilerRelative $stage.compiler -FixtureRelative $case.fixture -StageName $stage.name -ExpectedDiagnostic $case.expected -ForbiddenDiagnostic $case.forbidden)
    }
}
$verified = $true
foreach ($record in $records) {
    if (-not $record.pass) { $verified = $false }
}

$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.async-typechecker-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "Future<T> payload validation, non-Future await rejection, and fail-closed async lowering boundary"
    cases = @($cases | ForEach-Object { [ordered]@{ fixture = $_.fixture; expected = $_.expected; forbidden = $_.forbidden; sha256 = (Get-FileHash -LiteralPath (Root-Path $_.fixture) -Algorithm SHA256).Hash.ToLowerInvariant() } })
    records = @($records)
    case_count = $records.Count
    verified = $verified
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Async typechecker evidence: $outPath"
Write-Host "Cases=$($records.Count) Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
