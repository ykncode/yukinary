<##
.SYNOPSIS
    Stage-0 ve Stage-3 tam test matrisi parity kapisi.
.DESCRIPTION
    Iki canonical compiler'i ayni run_all_suites.ps1 fixture listesiyle calistirir.
    Sonuclari Suite/File anahtariyla karsilastirir; wrapper'in bildirdigi toplamlar
    yerine her fixture'in gercek oracle sonucunu kullanir.
#>
[CmdletBinding()]
param(
    [string]$CompilerA = ".\build\bootstrap\stage0.exe",
    [string]$CompilerB = ".\build\bootstrap\stage3.exe",
    [int]$TimeoutMs = 10000,
    [string]$OutputJson = "build\reports\stage-parity.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$RUNNER = Join-Path $PSScriptRoot "run_all_suites.ps1"
$WORK = Join-Path $ROOT "build\test-work\stage-parity"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $ROOT $Path))
}

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Parity report bulunamadi: $Path"
    }
    return ([System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path).Path, [System.Text.Encoding]::UTF8)) | ConvertFrom-Json
}

function Invoke-Matrix([string]$Label, [string]$CompilerPath) {
    $reportPath = Join-Path $WORK ("matrix-" + $Label + ".json")
    Remove-Item -LiteralPath $reportPath -Force -ErrorAction SilentlyContinue
    $args = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $RUNNER,
        "-Compiler", $CompilerPath,
        "-MinPassThreshold", "0",
        "-ThresholdMode", "Absolute",
        "-TimeoutMs", [string]$TimeoutMs,
        "-NoCoverage",
        "-OutputJson", $reportPath
    )
    Write-Host ("`n=== {0}: full fixture matrix ===" -f $Label) -ForegroundColor Cyan
    & powershell.exe @args | Out-Host
    $exitCode = $LASTEXITCODE
    $report = Read-Json $reportPath
    if ([string]$report.schema -ne "yukinary.test-report.v2") {
        throw ("{0}: beklenmeyen test report schema" -f $Label)
    }
    return [PSCustomObject]@{
        Label = $Label
        Compiler = $CompilerPath
        ExitCode = [int]$exitCode
        ReportPath = $reportPath
        Report = $report
    }
}

function Get-ResultMap($Report) {
    $map = @{}
    $duplicates = [System.Collections.Generic.List[string]]::new()
    foreach ($result in @($Report.results)) {
        $key = ("{0}|{1}" -f [string]$result.Suite, [string]$result.File)
        if ($map.ContainsKey($key)) { [void]$duplicates.Add($key) }
        $map[$key] = $result
    }
    return [PSCustomObject]@{ Map = $map; Duplicates = @($duplicates) }
}

function Get-ObservableResult($Result) {
    if ($null -eq $Result) { return $null }
    # Duration, compiler path ve debug metni parity karari icin goz ardi edilir.
    return [ordered]@{
        Status = [string]$Result.Status
        ExpectedMatch = if ($Result.PSObject.Properties.Name -contains "ExpectedMatch") { [bool]$Result.ExpectedMatch } else { $null }
        ActualExitCode = if ($Result.PSObject.Properties.Name -contains "ActualExitCode") { [int]$Result.ActualExitCode } else { $null }
        CompilerExitCode = if ($Result.PSObject.Properties.Name -contains "CompilerExitCode") { [int]$Result.CompilerExitCode } else { $null }
        StdoutMatch = if ($Result.PSObject.Properties.Name -contains "StdoutMatch") { [bool]$Result.StdoutMatch } else { $null }
        StderrMatch = if ($Result.PSObject.Properties.Name -contains "StderrMatch") { [bool]$Result.StderrMatch } else { $null }
        ActualStdoutSha256 = if ($Result.PSObject.Properties.Name -contains "ActualStdoutSha256") { [string]$Result.ActualStdoutSha256 } else { $null }
        ActualStderrSha256 = if ($Result.PSObject.Properties.Name -contains "ActualStderrSha256") { [string]$Result.ActualStderrSha256 } else { $null }
        DiagnosticContract = if ($Result.PSObject.Properties.Name -contains "DiagnosticContract") { [bool]$Result.DiagnosticContract } else { $null }
        DiagnosticCodeMatch = if ($Result.PSObject.Properties.Name -contains "DiagnosticCodeMatch") { [bool]$Result.DiagnosticCodeMatch } else { $null }
        LocationMatch = if ($Result.PSObject.Properties.Name -contains "LocationMatch") { [bool]$Result.LocationMatch } else { $null }
        ArtifactExists = if ($Result.PSObject.Properties.Name -contains "ArtifactExists") { [bool]$Result.ArtifactExists } else { $null }
        TimedOut = if ($Result.PSObject.Properties.Name -contains "TimedOut") { [bool]$Result.TimedOut } else { $null }
    }
}

$a = Invoke-Matrix "stage0" (Resolve-ProjectPath $CompilerA)
$b = Invoke-Matrix "stage3" (Resolve-ProjectPath $CompilerB)

function Invoke-Fix13Matrix([string]$Label, [string]$CompilerPath) {
    $reportPath = Join-Path $WORK ("fix13-" + $Label + ".json")
    Remove-Item -LiteralPath $reportPath -Force -ErrorAction SilentlyContinue
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ROOT "tests/fix13/run_fix13_tests.ps1") `
        -Compiler $CompilerPath -OutputJson $reportPath | Out-Host
    $exitCode = $LASTEXITCODE
    if (-not (Test-Path -LiteralPath $reportPath)) { throw "FIX-13 parity raporu olusturulamadi: $Label" }
    $report = Read-Json $reportPath
    return [PSCustomObject]@{ Label=$Label; Compiler=$CompilerPath; ExitCode=[int]$exitCode; ReportPath=$reportPath; Report=$report }
}

function Get-Fix13ResultMap($Report) {
    $map = @{}
    foreach ($result in @($Report.results)) { $map[[string]$result.name] = $result }
    return $map
}

$fix13A = Invoke-Fix13Matrix "stage0" (Resolve-ProjectPath $CompilerA)
$fix13B = Invoke-Fix13Matrix "stage3" (Resolve-ProjectPath $CompilerB)
$fix13AMap = Get-Fix13ResultMap $fix13A.Report
$fix13BMap = Get-Fix13ResultMap $fix13B.Report
$fix13Keys = @($fix13AMap.Keys + $fix13BMap.Keys | Sort-Object -Unique)
$fix13Differences = [System.Collections.Generic.List[object]]::new()
foreach ($key in $fix13Keys) {
    $hasA = $fix13AMap.ContainsKey($key); $hasB = $fix13BMap.ContainsKey($key)
    if (-not $hasA -or -not $hasB) {
        [void]$fix13Differences.Add([PSCustomObject]@{ key=$key; kind="fixture-list"; stage0_present=$hasA; stage3_present=$hasB }); continue
    }
    $left = $fix13AMap[$key]; $right = $fix13BMap[$key]
    $leftJson = [ordered]@{ status=$left.status; strict_oracle_passed=[bool]$left.strict_oracle_passed; compiler_exit_code=[int]$left.compiler_exit_code; artifact_exists=[bool]$left.artifact_exists; diagnostic_code=[string]$left.diagnostic_code; error_class=[string]$left.error_class } | ConvertTo-Json -Compress
    $rightJson = [ordered]@{ status=$right.status; strict_oracle_passed=[bool]$right.strict_oracle_passed; compiler_exit_code=[int]$right.compiler_exit_code; artifact_exists=[bool]$right.artifact_exists; diagnostic_code=[string]$right.diagnostic_code; error_class=[string]$right.error_class } | ConvertTo-Json -Compress
    if ($leftJson -cne $rightJson) { [void]$fix13Differences.Add([PSCustomObject]@{ key=$key; kind="observable-result"; stage0=$left; stage3=$right }) }
}
$fix13FixtureListSame = ((@($fix13AMap.Keys | Sort-Object) -join "`n") -ceq (@($fix13BMap.Keys | Sort-Object) -join "`n"))
$fix13OutcomesSame = ($fix13Differences.Count -eq 0)
$fix13BothPassed = ([int]$fix13A.Report.failed -eq 0 -and [int]$fix13B.Report.failed -eq 0)
$fix13ParityPass = ($fix13FixtureListSame -and $fix13OutcomesSame -and $fix13BothPassed)

Write-Host "`n=== FAZ 2 SESSION ISOLATION PARITY ===" -ForegroundColor Cyan
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ROOT "tests/session/test_session_isolation.ps1") -Compiler (Resolve-ProjectPath $CompilerA) | Out-Host
$sessionExitA = $LASTEXITCODE
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ROOT "tests/session/test_session_isolation.ps1") -Compiler (Resolve-ProjectPath $CompilerB) | Out-Host
$sessionExitB = $LASTEXITCODE
$sessionParityPass = ($sessionExitA -eq 0 -and $sessionExitB -eq 0)

$aMap = Get-ResultMap $a.Report
$bMap = Get-ResultMap $b.Report
$keys = @($aMap.Map.Keys + $bMap.Map.Keys | Sort-Object -Unique)
$differences = [System.Collections.Generic.List[object]]::new()

foreach ($key in $keys) {
    $hasA = $aMap.Map.ContainsKey($key)
    $hasB = $bMap.Map.ContainsKey($key)
    if (-not $hasA -or -not $hasB) {
        [void]$differences.Add([PSCustomObject]@{
            key=$key; kind="fixture-list"; stage0_present=$hasA; stage3_present=$hasB
        })
        continue
    }
    $left = Get-ObservableResult $aMap.Map[$key]
    $right = Get-ObservableResult $bMap.Map[$key]
    $leftJson = $left | ConvertTo-Json -Depth 10 -Compress
    $rightJson = $right | ConvertTo-Json -Depth 10 -Compress
    if ($leftJson -cne $rightJson) {
        [void]$differences.Add([PSCustomObject]@{
            key=$key; kind="observable-result"; stage0=$left; stage3=$right
        })
    }
}

$stage0Keys = @($aMap.Map.Keys | Sort-Object)
$stage3Keys = @($bMap.Map.Keys | Sort-Object)
$fixtureListSame = ($aMap.Duplicates.Count -eq 0 -and $bMap.Duplicates.Count -eq 0 -and
    (($stage0Keys -join "`n") -ceq ($stage3Keys -join "`n")))
$reportsConsistent = ([bool]$a.Report.report_consistent -and [bool]$b.Report.report_consistent -and
    [int]$a.Report.result_count -eq [int]$a.Report.total_tests -and
    [int]$b.Report.result_count -eq [int]$b.Report.total_tests)
$outcomeSame = ($differences.Count -eq 0)
$bothPassed = ([int]$a.Report.total_failed -eq 0 -and [int]$b.Report.total_failed -eq 0)
$parityPass = ($fixtureListSame -and $reportsConsistent -and $outcomeSame)
$gatePass = ($parityPass -and $bothPassed -and $fix13ParityPass -and $sessionParityPass)

$outPath = Resolve-ProjectPath $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $outPath) -Force | Out-Null
$report = [ordered]@{
    schema="yukinary.stage-parity-report.v1"
    generated_at=(Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    fixture_source="tests/run_all_suites.ps1"
    compilers=[ordered]@{
        stage0=[ordered]@{ path=$a.Compiler; exit_code=$a.ExitCode; report=$a.ReportPath; passed=[int]$a.Report.total_passed; failed=[int]$a.Report.total_failed; total=[int]$a.Report.total_tests; report_consistent=[bool]$a.Report.report_consistent }
        stage3=[ordered]@{ path=$b.Compiler; exit_code=$b.ExitCode; report=$b.ReportPath; passed=[int]$b.Report.total_passed; failed=[int]$b.Report.total_failed; total=[int]$b.Report.total_tests; report_consistent=[bool]$b.Report.report_consistent }
    }
    fixture_list_same=$fixtureListSame
    reports_consistent=$reportsConsistent
    observable_outcomes_same=$outcomeSame
    both_compilers_all_tests_passed=$bothPassed
    parity_pass=$parityPass
    gate_pass=$gatePass
    duplicate_stage0_keys=@($aMap.Duplicates)
    duplicate_stage3_keys=@($bMap.Duplicates)
    difference_count=$differences.Count
    differences=@($differences)
    fix13=[ordered]@{
        stage0=[ordered]@{ path=$fix13A.Compiler; exit_code=$fix13A.ExitCode; report=$fix13A.ReportPath; passed=[int]$fix13A.Report.passed; failed=[int]$fix13A.Report.failed; total=[int]$fix13A.Report.total }
        stage3=[ordered]@{ path=$fix13B.Compiler; exit_code=$fix13B.ExitCode; report=$fix13B.ReportPath; passed=[int]$fix13B.Report.passed; failed=[int]$fix13B.Report.failed; total=[int]$fix13B.Report.total }
        fixture_list_same=$fix13FixtureListSame
        observable_outcomes_same=$fix13OutcomesSame
        parity_pass=$fix13ParityPass
        difference_count=$fix13Differences.Count
        differences=@($fix13Differences)
    }
    session_isolation=[ordered]@{
        stage0_exit=$sessionExitA
        stage3_exit=$sessionExitB
        parity_pass=$sessionParityPass
    }
}
[System.IO.File]::WriteAllText($outPath, ($report | ConvertTo-Json -Depth 20), (New-Object System.Text.UTF8Encoding($false)))

Write-Host "`n=== STAGE PARITY SUMMARY ===" -ForegroundColor Cyan
Write-Host ("Stage-0: {0}/{1} passed" -f $a.Report.total_passed, $a.Report.total_tests)
Write-Host ("Stage-3: {0}/{1} passed" -f $b.Report.total_passed, $b.Report.total_tests)
Write-Host ("Fixture list same       : {0}" -f $fixtureListSame)
Write-Host ("Observable outcomes same: {0}" -f $outcomeSame)
Write-Host ("Parity report           : {0}" -f $outPath)
if (-not $gatePass) {
    Write-Host "STAGE PARITY FAIL - farklar veya compiler test failure'lari var." -ForegroundColor Red
    exit 1
}
Write-Host "STAGE PARITY PASS - iki compiler ayni fixture matrisini tamamen gecti." -ForegroundColor Green
exit 0
