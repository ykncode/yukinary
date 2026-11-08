[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$OutputJson = "build/reports/reality-audit-core-native.json",
    [switch]$WindowsOnly
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $Root
$Source = Join-Path $Root "tests\tooling\reality_audit_core.ykn"
$CliSource = Join-Path $Root "tools\reality_audit_cli.ykn"
$CleanFixture = Join-Path $Root "tests\tooling\audit_clean_fixture.ykn"
$FlaggedFixture = Join-Path $Root "tests\tooling\audit_flagged_fixture.ykn"
$MissingFixture = Join-Path $Root "build\test-work\missing-audit-input.ykn"
$EmptyFixture = Join-Path $Root "build\test-work\empty-audit-input.ykn"
$OversizedFixture = Join-Path $Root "build\test-work\oversized-audit-input.ykn"
$MalformedFixture = Join-Path $Root "build\test-work\malformed-audit-input.ykn"
$NulFixture = Join-Path $Root "build\test-work\nul-audit-input.ykn"
$SpacedFixture = Join-Path $Root "build\test-work\audit input with spaces.ykn"
$EscapedFixture = Join-Path $Root "build\test-work\audit% path.ykn"
$Work = Join-Path $Root "build\test-work\reality-audit-core"
New-Item -ItemType Directory -Path $Work -Force | Out-Null
[IO.File]::WriteAllText($SpacedFixture, "isv fixture() -> tamsayi {`n    dondu 42;`n}`n", (New-Object System.Text.UTF8Encoding($false)))
[IO.File]::WriteAllText($EscapedFixture, ('degisken value: *u8 = "mock \" stub";' + "`n"), (New-Object System.Text.UTF8Encoding($false)))
[IO.File]::WriteAllBytes($EmptyFixture, [byte[]]@())
[IO.File]::WriteAllBytes($OversizedFixture, [byte[]]::new(1048576))
[IO.File]::WriteAllText($MalformedFixture, "/* unterminated mock comment", (New-Object System.Text.UTF8Encoding($false)))
[IO.File]::WriteAllBytes($NulFixture, [byte[]]@(97, 98, 99, 0, 109, 111, 99, 107))

function Invoke-Checked([string]$Path, [string[]]$Arguments) {
    & $Path @Arguments
    $code = $LASTEXITCODE
    if ($code -ne 0) { throw "Process failed ($code): $Path $($Arguments -join ' ')" }
    return $code
}
function Quote-Argument([string]$Value) { return '"' + $Value.Replace('"', '\\"') + '"' }
function Invoke-Captured([string]$Path, [string[]]$Arguments) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Path
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join " ")
    $psi.WorkingDirectory = $Root
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    if (-not $process.Start()) { throw "Could not start $Path" }
    $outTask = $process.StandardOutput.ReadToEndAsync()
    $errTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(30000)) { $process.Kill(); $process.WaitForExit(); throw "Timed out: $Path" }
    [System.Threading.Tasks.Task]::WaitAll(@($outTask, $errTask), 5000) | Out-Null
    return [ordered]@{ exit_code = $process.ExitCode; stdout = $outTask.Result; stderr = $errTask.Result }
}

$records = @()
$cliRecords = @()
$linuxRecords = @()
foreach ($stage in @(
    @{ Name = "Stage-0"; Compiler = (Join-Path $Root $CompilerStage0) },
    @{ Name = "Stage-3"; Compiler = (Join-Path $Root $CompilerStage3) }
)) {
    $exe = Join-Path $Work (($stage.Name -replace "[^A-Za-z0-9]", "").ToLowerInvariant() + ".exe")
    Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue
    Invoke-Checked $stage.Compiler @($Source, $exe) | Out-Null
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "$($stage.Name): compiler produced no executable" }
    Invoke-Checked $exe @() | Out-Null
    $cliExe = Join-Path $Work (($stage.Name -replace "[^A-Za-z0-9]", "").ToLowerInvariant() + "-cli.exe")
    Remove-Item -LiteralPath $cliExe -Force -ErrorAction SilentlyContinue
    Invoke-Checked $stage.Compiler @($CliSource, $cliExe) | Out-Null
    if (-not (Test-Path -LiteralPath $cliExe -PathType Leaf)) { throw "$($stage.Name): CLI compiler produced no executable" }
    $clean = Invoke-Captured $cliExe @($CleanFixture)
    $flagged = Invoke-Captured $cliExe @($FlaggedFixture)
    $missing = Invoke-Captured $cliExe @($MissingFixture)
    $empty = Invoke-Captured $cliExe @($EmptyFixture)
    $oversized = Invoke-Captured $cliExe @($OversizedFixture)
    $malformed = Invoke-Captured $cliExe @($MalformedFixture)
    $nulInput = Invoke-Captured $cliExe @($NulFixture)
    $usage = Invoke-Captured $cliExe @()
    $multi = Invoke-Captured $cliExe @($CleanFixture, $FlaggedFixture)
    $spaced = Invoke-Captured $cliExe @($SpacedFixture)
    $reportRun = Invoke-Captured $cliExe @("--report", $CleanFixture)
    $reportText = $reportRun.stdout
    $escapedReportRun = Invoke-Captured $cliExe @("--report", $EscapedFixture)
    $flaggedReportRun = Invoke-Captured $cliExe @("--report", $FlaggedFixture)
    $cliPass = ($clean.exit_code -eq 0 -and $clean.stdout.Contains("mock=absent stub=absent constant_success=absent fake_handle=absent") -and
        $clean.stdout.Contains("REALITY_AUDIT_NO_MARKERS") -and
        $flagged.exit_code -eq 1 -and
        $flagged.stdout.Contains("mock=present stub=absent constant_success=present fake_handle=absent") -and
        $flagged.stdout.Contains("REALITY_AUDIT_REVIEW_REQUIRED") -and
        $missing.exit_code -eq 3 -and $missing.stdout.Contains("REALITY_AUDIT_READ_ERROR") -and
        $empty.exit_code -eq 3 -and $empty.stdout.Contains("REALITY_AUDIT_READ_ERROR") -and
        $oversized.exit_code -eq 3 -and $oversized.stdout.Contains("REALITY_AUDIT_READ_ERROR") -and
        $malformed.exit_code -eq 4 -and $malformed.stdout.Contains("REALITY_AUDIT_SCAN_ERROR") -and
        $nulInput.exit_code -eq 3 -and $nulInput.stdout.Contains("REALITY_AUDIT_BINARY_INPUT_ERROR") -and
        $usage.exit_code -eq 2 -and $usage.stdout.Contains("REALITY_AUDIT_USAGE") -and
        $multi.exit_code -eq 1 -and $multi.stdout.Contains("audit_clean_fixture.ykn") -and
        $multi.stdout.Contains("audit_flagged_fixture.ykn") -and $multi.stdout.Contains("REALITY_AUDIT_REVIEW_REQUIRED") -and
        $spaced.exit_code -eq 0 -and $spaced.stdout.Contains("REALITY_AUDIT_NO_MARKERS") -and
        $reportRun.exit_code -eq 0 -and $reportText.Contains("YKN_REALITY_AUDIT_V1") -and
        $reportText.Contains("mock=absent") -and $reportText.Contains("VERDICT`tNO_MARKERS") -and
        $escapedReportRun.exit_code -eq 0 -and $escapedReportRun.stdout.Contains("audit%25 path.ykn") -and
        $flaggedReportRun.exit_code -eq 1 -and $flaggedReportRun.stdout.Contains("mock=present") -and
        $flaggedReportRun.stdout.Contains("VERDICT`tREVIEW_REQUIRED"))
    if (-not $cliPass) { throw "$($stage.Name): CLI clean/flagged/missing/usage contract failed" }
    $cliRecords += [ordered]@{
        stage = $stage.Name
        executable = $cliExe
        executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $cliExe).Hash.ToLowerInvariant()
        clean = $clean
        flagged = $flagged
        missing_input = $missing
        empty_input = $empty
        input_at_1mib_limit = $oversized
        malformed_source = $malformed
        embedded_nul = $nulInput
        no_arguments = $usage
        mixed_files = $multi
        spaced_path = $spaced
        report_stdout = $reportRun
        escaped_path_report_stdout = $escapedReportRun
        flagged_report_stdout = $flaggedReportRun
        verified = $cliPass
    }
    $records += [ordered]@{
        stage = $stage.Name
        compiler = $stage.Compiler
        compiler_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $stage.Compiler).Hash.ToLowerInvariant()
        executable = $exe
        executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash.ToLowerInvariant()
        compile_exit_code = 0
        run_exit_code = 0
    }
}

function Invoke-LinuxAudit([string]$Elf, [string[]]$InputPaths) {
    $elfRelative = $Elf.Substring($Root.Length + 1).Replace("\", "/")
    $containerElf = "/work/$elfRelative"
    $containerArgs = @($InputPaths | ForEach-Object {
        $value = [string]$_
        if ([IO.Path]::IsPathRooted($value)) { "'/work/" + $value.Substring($Root.Length + 1).Replace("\", "/") + "'" }
        elseif ($value.Contains("/") -or $value.Contains("\\")) { "'/work/" + $value.Replace("\\", "/") + "'" }
        else { "'" + $value.Replace("'", "'\\''") + "'" }
    })
    $command = "chmod +x '$containerElf' && '$containerElf' " + ($containerArgs -join " ")
    $dockerArgs = @("run", "--rm", "-v", ($Root.Replace("\", "/") + ":/work"), "-w", "/work", "ubuntu:24.04", "bash", "-lc", $command)
    return Invoke-Captured "docker" $dockerArgs
}

if (-not $WindowsOnly) {
foreach ($stage in @(
    @{ Name = "Stage-0"; Compiler = (Join-Path $Root $CompilerStage0) },
    @{ Name = "Stage-3"; Compiler = (Join-Path $Root $CompilerStage3) }
)) {
    $elf = Join-Path $Work (($stage.Name -replace "[^A-Za-z0-9]", "").ToLowerInvariant() + "-cli.elf")
    Remove-Item -LiteralPath $elf -Force -ErrorAction SilentlyContinue
    $compile = Invoke-Captured $stage.Compiler @($CliSource, $elf, "--target", "linux")
    if ($compile.exit_code -ne 0 -or -not (Test-Path -LiteralPath $elf -PathType Leaf)) {
        throw "$($stage.Name): Linux CLI ELF compilation failed: $($compile.stderr)"
    }
    $linuxCases = @(
        @{ name = "clean"; args = @($CleanFixture); exit = 0; marker = "REALITY_AUDIT_NO_MARKERS" },
        @{ name = "flagged"; args = @($FlaggedFixture); exit = 1; marker = "REALITY_AUDIT_REVIEW_REQUIRED" },
        @{ name = "missing"; args = @($MissingFixture); exit = 3; marker = "REALITY_AUDIT_READ_ERROR" },
        @{ name = "empty"; args = @($EmptyFixture); exit = 3; marker = "REALITY_AUDIT_READ_ERROR" },
        @{ name = "oversized"; args = @($OversizedFixture); exit = 3; marker = "REALITY_AUDIT_READ_ERROR" },
        @{ name = "malformed"; args = @($MalformedFixture); exit = 4; marker = "REALITY_AUDIT_SCAN_ERROR" },
        @{ name = "embedded_nul"; args = @($NulFixture); exit = 3; marker = "REALITY_AUDIT_BINARY_INPUT_ERROR" },
        @{ name = "usage"; args = @(); exit = 2; marker = "REALITY_AUDIT_USAGE" },
        @{ name = "mixed"; args = @($CleanFixture, $FlaggedFixture); exit = 1; marker = "REALITY_AUDIT_REVIEW_REQUIRED" },
        @{ name = "spaced"; args = @($SpacedFixture); exit = 0; marker = "REALITY_AUDIT_NO_MARKERS" },
        @{ name = "report"; args = @("--report", $CleanFixture); exit = 0; marker = "YKN_REALITY_AUDIT_V1" },
        @{ name = "report_escaped_path"; args = @("--report", $EscapedFixture); exit = 0; marker = "YKN_REALITY_AUDIT_V1" },
        @{ name = "report_flagged"; args = @("--report", $FlaggedFixture); exit = 1; marker = "YKN_REALITY_AUDIT_V1" }
    )
    foreach ($case in $linuxCases) {
        $run = Invoke-LinuxAudit $elf $case.args
        $pass = ($run.exit_code -eq $case.exit -and $run.stdout.Contains($case.marker))
        if ($case.name -eq "mixed") {
            $pass = ($pass -and $run.stdout.Contains("audit_clean_fixture.ykn") -and $run.stdout.Contains("audit_flagged_fixture.ykn"))
        }
        if ($case.name -eq "report") {
            $pass = ($pass -and $run.stdout.Contains("mock=absent") -and $run.stdout.Contains("VERDICT`tNO_MARKERS"))
        }
        if ($case.name -eq "report_escaped_path") {
            $pass = ($pass -and $run.stdout.Contains("audit%25 path.ykn") -and $run.stdout.Contains("mock=absent"))
        }
        if ($case.name -eq "report_flagged") {
            $pass = ($pass -and $run.stdout.Contains("mock=present") -and $run.stdout.Contains("VERDICT`tREVIEW_REQUIRED"))
        }
        if (-not $pass) { throw "$($stage.Name): Linux CLI case $($case.name) failed: $($run.stdout) $($run.stderr)" }
        $linuxRecords += [ordered]@{
            stage = $stage.Name
            case = $case.name
            expected_exit_code = $case.exit
            actual_exit_code = $run.exit_code
            stdout = $run.stdout
            stderr = $run.stderr
            verified = $pass
        }
    }
}
}

$output = Join-Path $Root $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force | Out-Null
$windowsVerified = ($records.Count -eq 2 -and $cliRecords.Count -eq 2 -and @($cliRecords | Where-Object { -not $_.verified }).Count -eq 0)
$linuxVerified = ($linuxRecords.Count -eq 26 -and @($linuxRecords | Where-Object { -not $_.verified }).Count -eq 0)
$report = [ordered]@{
    schema = "yukinary.reality-audit-core-native.v3"
    verified = ($windowsVerified -and $linuxVerified)
    windows_verified = $windowsVerified
    linux_verified = $linuxVerified
    linux_skipped = [bool]$WindowsOnly
    source = "tests/tooling/reality_audit_core.ykn"
    source_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Source).Hash.ToLowerInvariant()
    cli_source = "tools/reality_audit_cli.ykn"
    cli_source_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $CliSource).Hash.ToLowerInvariant()
    records = $records
    cli_acceptance = $cliRecords
    linux_docker_acceptance = $linuxRecords
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $output -Encoding UTF8
if (-not $windowsVerified) { throw "Reality audit Windows native acceptance failed" }
if (-not $WindowsOnly -and -not $linuxVerified) { throw "Reality audit Linux Docker native acceptance failed" }
$linuxText = if ($WindowsOnly) { "Linux Docker CLI=SKIPPED" } else { "Linux Docker CLI=$($linuxRecords.Count)/26 PASS" }
Write-Host "Reality audit YKN native gate: Windows core=$($records.Count)/2, Windows CLI=$($cliRecords.Count * 13)/26, $linuxText; report=$OutputJson"
