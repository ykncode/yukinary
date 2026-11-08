[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [int]$CompileTimeoutMs = 120000,
    [string]$OutputJson = "build/reports/async-syntax-fail-closed-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Work = Join-Path $Root "build\test-work\async-syntax-fail-closed"
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

function Invoke-AsyncSyntaxCase([string]$CompilerRelative, [string]$FixtureRelative, [string]$StageName) {
    $compilerPath = Root-Path $CompilerRelative
    $fixturePath = Root-Path $FixtureRelative
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) { throw "Compiler not found: $CompilerRelative" }
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) { throw "Fixture not found: $FixtureRelative" }
    $fixtureName = [IO.Path]::GetFileNameWithoutExtension($FixtureRelative)
    $artifactRelative = "build/test-work/async-syntax-fail-closed/{0}-{1}.exe" -f $fixtureName, $StageName.ToLowerInvariant()
    $artifactPath = Root-Path $artifactRelative
    Remove-Item -LiteralPath $artifactPath -Force -ErrorAction SilentlyContinue
    $run = Invoke-CapturedProcess -FilePath $compilerPath -Arguments @($fixturePath, $artifactPath) -TimeoutMs $CompileTimeoutMs
    $output = ($run.stdout + "`n" + $run.stderr)
    $parserDiagnostic = ($output -match "PARSER ERROR|ERR_UNEXPECTED|ERR_INVALID_EXPRESSION|ERR_ASYNC_FUNCTION_KEYWORD_EXPECTED|sozdizimi hatasi|Parser.*basarisiz")
    $codegenDiagnostic = ($output -match "E043|ERR_UNSUPPORTED_AST_NODE")
    $artifactExists = Test-Path -LiteralPath $artifactPath -PathType Leaf
    [ordered]@{
        stage = $StageName
        compiler = $CompilerRelative
        compiler_sha256 = (Get-FileHash -LiteralPath $compilerPath -Algorithm SHA256).Hash.ToLowerInvariant()
        fixture = $FixtureRelative
        fixture_sha256 = (Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant()
        artifact = $artifactRelative
        exit_code = $run.exit_code
        timed_out = $run.timed_out
        parser_diagnostic = $parserDiagnostic
        codegen_diagnostic = $codegenDiagnostic
        artifact_exists = $artifactExists
        pass = ($run.exit_code -ne 0 -and -not $run.timed_out -and $codegenDiagnostic -and -not $parserDiagnostic -and -not $artifactExists)
        stdout = $run.stdout
        stderr = $run.stderr
    }
}

$fixtures = @(
    "tests/async/async_decl_not_lowered.ykn",
    "tests/async/async_syntax_not_lowered.ykn"
)
$stages = @(
    [ordered]@{ name = "Stage-0"; compiler = $CompilerStage0 },
    [ordered]@{ name = "Stage-3"; compiler = $CompilerStage3 }
)
$records = @()
foreach ($stage in $stages) {
    foreach ($fixture in $fixtures) {
        $records += ,(Invoke-AsyncSyntaxCase -CompilerRelative $stage.compiler -FixtureRelative $fixture -StageName $stage.name)
    }
}
$verified = $true
foreach ($record in $records) {
    if (-not $record.pass) { $verified = $false }
}

$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.async-syntax-fail-closed.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "async/await syntax is recognized by lexer/parser but cannot emit an artifact before real Future<T> lowering"
    fixtures = @($fixtures | ForEach-Object {
        $fixturePath = Root-Path $_
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    records = @($records)
    case_count = $records.Count
    verified = $verified
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Async syntax fail-closed evidence: $outPath"
Write-Host "Cases=$($records.Count) Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
