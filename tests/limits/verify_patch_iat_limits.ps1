[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$OutputJson = "build/reports/patch-iat-limits-native.json",
    [int]$TimeoutMs = 180000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Work = Join-Path $Root "build/test-work/limits/patch-iat"
New-Item -ItemType Directory -Path $Work -Force | Out-Null

function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding($false)))
}

function New-PatchFixture([int]$HelperCount, [int]$CallsPerHelper, [int]$MainCalls) {
    $b = New-Object Text.StringBuilder
    [void]$b.AppendLine("# Gerçek codegen patch tablosu kabul fixture'ı.")
    [void]$b.AppendLine("# Her çağrı AST_ISLEV_CAGRISI -> E8 rel32 patch kaydı üretir.")
    [void]$b.AppendLine("isv hedef_yama() -> tamsayi { dondu 0; }")
    for ($i = 0; $i -lt $HelperCount; $i++) {
        [void]$b.AppendLine(("isv yardim_yama_{0}() -> tamsayi {{" -f $i))
        for ($j = 0; $j -lt $CallsPerHelper; $j++) {
            [void]$b.AppendLine("    hedef_yama();")
        }
        [void]$b.AppendLine("    dondu 0;")
        [void]$b.AppendLine("}")
    }
    [void]$b.AppendLine("isv ana() -> tamsayi {")
    for ($j = 0; $j -lt $MainCalls; $j++) {
        [void]$b.AppendLine("    hedef_yama();")
    }
    [void]$b.AppendLine("    dondu 0;")
    [void]$b.AppendLine("}")
    return $b.ToString()
}

function New-IatFixture([int]$HelperCount, [int]$CallsPerHelper, [int]$MainCalls) {
    $b = New-Object Text.StringBuilder
    [void]$b.AppendLine("# Gerçek Win32 IAT kapasite fixture'ı; socket_close -> Winsock IAT lowering kullanılır.")
    [void]$b.AppendLine("isv yukinary_sys_socket_close(sock: tamsayi) -> tamsayi { dondu 0; }")
    for ($i = 0; $i -lt $HelperCount; $i++) {
        [void]$b.AppendLine(("isv yardim_iat_{0}() -> tamsayi {{" -f $i))
        for ($j = 0; $j -lt $CallsPerHelper; $j++) {
            [void]$b.AppendLine("    yukinary_sys_socket_close(0);")
        }
        [void]$b.AppendLine("    dondu 0;")
        [void]$b.AppendLine("}")
    }
    # Sınır çağrıları çalıştırılmayan ayrı fonksiyonda tutulur; acceptance
    # derleyici patch/IAT kayıtlarını ve üretilen PE'yi ölçer, fixture runtime'
    # içinde 16K gereksiz allocation yapmaz.
    [void]$b.AppendLine("isv kuyruk_iat() -> tamsayi {")
    for ($j = 0; $j -lt $MainCalls; $j++) {
        [void]$b.AppendLine("    yukinary_sys_socket_close(0);")
    }
    [void]$b.AppendLine("    dondu 0;")
    [void]$b.AppendLine("}")
    [void]$b.AppendLine("isv ana() -> tamsayi { dondu 0; }")
    return $b.ToString()
}

function Invoke-Compiler([string]$StageName, [string]$Compiler, [hashtable]$Case) {
    $fixture = Join-Path $Work $Case.File
    $artifact = Join-Path $Work ("{0}-{1}.exe" -f $Case.Name, $StageName.ToLowerInvariant())
    $stdoutPath = Join-Path $Work ("{0}-{1}.stdout" -f $Case.Name, $StageName.ToLowerInvariant())
    $stderrPath = Join-Path $Work ("{0}-{1}.stderr" -f $Case.Name, $StageName.ToLowerInvariant())
    Remove-Item -LiteralPath $artifact, $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $Compiler `
        -ArgumentList @($fixture, $artifact) `
        -WorkingDirectory $Root `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -Wait `
        -PassThru
    $timer.Stop()
    $process.Refresh()
    $runExit = $null
    $runPass = $true
    $output = ""
    if (Test-Path -LiteralPath $stdoutPath) { $output += [IO.File]::ReadAllText($stdoutPath) }
    if (Test-Path -LiteralPath $stderrPath) { $output += "`n" + [IO.File]::ReadAllText($stderrPath) }
    $exists = Test-Path -LiteralPath $artifact -PathType Leaf
    $patchDiagnostic = $output -match "ERR_PATCH_LIMIT"
    $iatDiagnostic = $output -match "ERR_IAT_LIMIT"
    $irDiagnostic = $output -match "ERR_IR_LIMIT"
    $crash = $output -match "access violation|segmentation fault|stack overflow|0xC0000005|STATUS_ACCESS_VIOLATION"
    if ($Case.Expected -eq "success" -and $process.ExitCode -eq 0 -and $exists) {
        $run = Start-Process -FilePath $artifact -WorkingDirectory $Root -Wait -PassThru -NoNewWindow
        $run.Refresh()
        $runExit = $run.ExitCode
        $runPass = ($runExit -eq 0)
    }
    $pass = if ($Case.Expected -eq "success") {
        ($process.ExitCode -eq 0 -and $exists -and $runPass -and -not $crash)
    } else {
        ($process.ExitCode -ne 0 -and -not $exists -and (($Case.Expected -eq "patch" -and $patchDiagnostic) -or ($Case.Expected -eq "iat" -and $iatDiagnostic)) -and -not $crash)
    }
    $intendedUserRecords = ($Case.HelperCount * $Case.CallsPerHelper) + $Case.MainCalls
    $startupIatRecords = if ($Case.Kind -eq "iat") { 2 } else { 0 }
    return [ordered]@{
        case = $Case.Name
        stage = $StageName
        fixture = (Resolve-Path -Relative $fixture).TrimStart(".\") -replace "\\", "/"
        fixture_sha256 = (Get-FileHash $fixture -Algorithm SHA256).Hash.ToLowerInvariant()
        helper_count = $Case.HelperCount
        calls_per_helper = $Case.CallsPerHelper
        main_calls = $Case.MainCalls
        intended_user_records = $intendedUserRecords
        startup_iat_records = $startupIatRecords
        intended_total_records = $intendedUserRecords + $startupIatRecords
        expected = $Case.Expected
        exit_code = $process.ExitCode
        elapsed_ms = $timer.ElapsedMilliseconds
        artifact_exists = $exists
        run_exit_code = $runExit
        run_pass = $runPass
        patch_diagnostic = $patchDiagnostic
        iat_diagnostic = $iatDiagnostic
        ir_diagnostic = $irDiagnostic
        crash_signature = $crash
        pass = $pass
        output = $output
    }
}

$cases = @(
    @{ Name = "patch-small-400"; File = "patch-small-400.ykn"; Kind = "patch"; HelperCount = 100; CallsPerHelper = 4; MainCalls = 0; Expected = "success" },
    @{ Name = "patch-16383"; File = "patch-16383.ykn"; Kind = "patch"; HelperCount = 4094; CallsPerHelper = 4; MainCalls = 7; Expected = "success" },
    @{ Name = "patch-16384"; File = "patch-16384.ykn"; Kind = "patch"; HelperCount = 4094; CallsPerHelper = 4; MainCalls = 8; Expected = "success" },
    @{ Name = "patch-16385"; File = "patch-16385.ykn"; Kind = "patch"; HelperCount = 4094; CallsPerHelper = 4; MainCalls = 9; Expected = "patch" },
    @{ Name = "iat-small-400"; File = "iat-small-400.ykn"; Kind = "iat"; HelperCount = 100; CallsPerHelper = 4; MainCalls = 0; Expected = "success" },
    @{ Name = "iat-16381"; File = "iat-16381.ykn"; Kind = "iat"; HelperCount = 1023; CallsPerHelper = 16; MainCalls = 13; Expected = "success" },
    @{ Name = "iat-16382"; File = "iat-16382.ykn"; Kind = "iat"; HelperCount = 1023; CallsPerHelper = 16; MainCalls = 14; Expected = "success" },
    @{ Name = "iat-16383"; File = "iat-16383.ykn"; Kind = "iat"; HelperCount = 1023; CallsPerHelper = 16; MainCalls = 15; Expected = "iat" },
        @{ Name = "iat-16384"; File = "iat-16384.ykn"; Kind = "iat"; HelperCount = 1023; CallsPerHelper = 16; MainCalls = 16; Expected = "iat" }
)

$stages = @(
    @{ Name = "Stage-0"; Exe = (Join-Path $Root ($CompilerStage0 -replace "/", "\")) },
    @{ Name = "Stage-3"; Exe = (Join-Path $Root ($CompilerStage3 -replace "/", "\")) }
)
foreach ($stage in $stages) {
    if (-not (Test-Path -LiteralPath $stage.Exe -PathType Leaf)) { throw "Compiler executable not found: $($stage.Exe)" }
}

$records = @()
foreach ($case in $cases) {
    $fixtureText = if ($case.Kind -eq "iat") {
        New-IatFixture $case.HelperCount $case.CallsPerHelper $case.MainCalls
    } else {
        New-PatchFixture $case.HelperCount $case.CallsPerHelper $case.MainCalls
    }
    Write-Utf8 (Join-Path $Work $case.File) $fixtureText
    foreach ($stage in $stages) {
        $record = Invoke-Compiler $stage.Name $stage.Exe $case
        $records += $record
        if ($record.pass) {
            Write-Host ("PASS {0}/{1} total_records={2}" -f $case.Name, $stage.Name, $record.intended_total_records) -ForegroundColor Green
        } else {
            Write-Host ("FAIL {0}/{1} total_records={2} exit={3} artifact={4} patch={5} iat={6} ir={7}`n{8}" -f $case.Name, $stage.Name, $record.intended_total_records, $record.exit_code, $record.artifact_exists, $record.patch_diagnostic, $record.iat_diagnostic, $record.ir_diagnostic, $record.output) -ForegroundColor Red
        }
    }
}

$verified = $true
foreach ($record in $records) { if (-not $record.pass) { $verified = $false } }
$outPath = Join-Path $Root ($OutputJson -replace "/", "\")
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.patch-iat-limits-native-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    platform = "windows-x86_64"
    patch_capacity = 16384
    iat_capacity = 16384
    cases = $cases
    records = $records
    verified = $verified
} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Patch/IAT limit evidence: $outPath"
Write-Host ("Cases={0} Verified={1}" -f $records.Count, $verified)
if (-not $verified) { exit 1 }
exit 0
