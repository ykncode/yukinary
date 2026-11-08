[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$OutputJson = "build/reports/oom-contract-native.json",
    [int]$TimeoutMs = 60000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Work = Join-Path $Root "build/test-work/limits/oom-contract"
New-Item -ItemType Directory -Path $Work -Force | Out-Null

function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding($false)))
}

$fixture = Join-Path $Work "oom-fixture.ykn"
Write-Utf8 $fixture "isv ana() -> tamsayi { dondu 0; }`n"
$fixtureHash = (Get-FileHash $fixture -Algorithm SHA256).Hash.ToLowerInvariant()
$records = @()

$stages = @(
    @{ Name = "Stage-0"; Exe = (Join-Path $Root ($CompilerStage0 -replace "/", "\")); FirstSuccess = 25 },
    @{ Name = "Stage-3"; Exe = (Join-Path $Root ($CompilerStage3 -replace "/", "\")); FirstSuccess = 88 }
)

foreach ($stage in $stages) {
    if (-not (Test-Path -LiteralPath $stage.Exe -PathType Leaf)) {
        throw "Compiler executable not found: $($stage.Exe)"
    }

    $firstSuccess = -1
    for ($failAfter = 0; $failAfter -le [int]$stage.FirstSuccess; $failAfter++) {
        $prefix = ("{0}-{1}" -f $stage.Name.ToLowerInvariant(), $failAfter)
        $artifact = Join-Path $Work ("oom-{0}.exe" -f $prefix)
        $stdoutPath = Join-Path $Work ("{0}.stdout" -f $prefix)
        $stderrPath = Join-Path $Work ("{0}.stderr" -f $prefix)
        Remove-Item -LiteralPath $artifact, $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

        $timer = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $stage.Exe `
            -ArgumentList @("--fail-alloc-after", [string]$failAfter, $fixture, $artifact) `
            -WorkingDirectory $Root `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath `
            -Wait `
            -PassThru
        if (-not $process.WaitForExit($TimeoutMs)) {
            $process.Kill()
            throw "$($stage.Name) fail_after=$failAfter OOM contract timed out"
        }
        $timer.Stop()
        $process.Refresh()

        $output = ""
        if (Test-Path -LiteralPath $stdoutPath) { $output += [IO.File]::ReadAllText($stdoutPath) }
        if (Test-Path -LiteralPath $stderrPath) { $output += "`n" + [IO.File]::ReadAllText($stderrPath) }
        $exists = Test-Path -LiteralPath $artifact -PathType Leaf
        $diagnostic = ($output -match "E014") -and ($output -match "ERR_OOM")
        $crash = $output -match "access violation|segmentation fault|stack overflow|0xC0000005|STATUS_ACCESS_VIOLATION"
        $isExpectedFailure = ($failAfter -lt [int]$stage.FirstSuccess)
        $compilePass = if ($isExpectedFailure) {
            ($process.ExitCode -ne 0 -and -not $exists -and $diagnostic -and -not $crash)
        } else {
            ($process.ExitCode -eq 0 -and $exists -and -not $crash)
        }
        $runExit = $null
        $runPass = $true
        if (-not $isExpectedFailure -and $compilePass) {
            $run = Start-Process -FilePath $artifact -WorkingDirectory $Root -Wait -PassThru -NoNewWindow
            $run.Refresh()
            $runExit = $run.ExitCode
            $runPass = ($runExit -eq 0)
            $firstSuccess = $failAfter
        }
        $pass = ($compilePass -and $runPass)

        $records += [ordered]@{
            case = if ($isExpectedFailure) { "allocation-failure" } else { "allocation-boundary-success" }
            stage = $stage.Name
            fixture = (Resolve-Path -Relative $fixture).TrimStart(".\") -replace "\\", "/"
            fixture_sha256 = $fixtureHash
            fail_after = $failAfter
            expected_first_success = [int]$stage.FirstSuccess
            exit_code = $process.ExitCode
            run_exit_code = $runExit
            elapsed_ms = $timer.ElapsedMilliseconds
            artifact_exists = $exists
            diagnostic = $diagnostic
            crash_signature = $crash
            pass = $pass
            output = $output
        }
        if (-not $pass) {
            throw ("FAIL {0} fail_after={1}: compile_exit={2} run_exit={3} artifact={4} diagnostic={5} crash={6}`n{7}" -f $stage.Name, $failAfter, $process.ExitCode, $runExit, $exists, $diagnostic, $crash, $output)
        }
    }
    if ($firstSuccess -ne [int]$stage.FirstSuccess) {
        throw "$($stage.Name) first successful allocation boundary changed: expected=$($stage.FirstSuccess) actual=$firstSuccess"
    }
    Write-Host ("PASS {0}: fail_after=0..{1} fail-closed + boundary executable" -f $stage.Name, $stage.FirstSuccess) -ForegroundColor Green
}

$verified = $true
foreach ($record in $records) { if (-not $record.pass) { $verified = $false } }
$outPath = Join-Path $Root ($OutputJson -replace "/", "\")
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.oom-contract-native-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "full deterministic allocation-failure matrix for the minimal compiler pipeline"
    first_success_boundary = [ordered]@{ stage0 = 25; stage3 = 88 }
    records = $records
    verified = $verified
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "OOM contract evidence: $outPath"
Write-Host ("Cases={0} Verified={1}" -f $records.Count, $verified)
if (-not $verified) { exit 1 }
exit 0
