[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$OutputJson = "build/reports/capacity-limits-native.json",
    [int]$TimeoutMs = 60000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Work = Join-Path $Root "build/test-work/limits/capacity"
New-Item -ItemType Directory -Path $Work -Force | Out-Null

function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding($false)))
}

function New-LocalFixture([int]$Count) {
    $b = New-Object Text.StringBuilder
    [void]$b.AppendLine("isv ana() -> tamsayi {")
    for ($i = 0; $i -lt $Count; $i++) { [void]$b.AppendLine(("    degisken local_{0}: tamsayi = 0;" -f $i)) }
    [void]$b.AppendLine("    dondu 0;")
    [void]$b.AppendLine("}")
    return $b.ToString()
}

function New-GlobalFixture([int]$Count) {
    $b = New-Object Text.StringBuilder
    for ($i = 0; $i -lt $Count; $i++) { [void]$b.AppendLine(("degisken global_{0}: tamsayi = 0;" -f $i)) }
    [void]$b.AppendLine("isv ana() -> tamsayi { dondu 0; }")
    return $b.ToString()
}

function New-FunctionFixture([int]$Count) {
    $b = New-Object Text.StringBuilder
    for ($i = 0; $i -lt $Count; $i++) { [void]$b.AppendLine(("isv function_{0}() -> tamsayi {{ dondu 0; }}" -f $i)) }
    [void]$b.AppendLine("isv ana() -> tamsayi { dondu 0; }")
    return $b.ToString()
}

function New-FieldFixture([int]$Count) {
    $b = New-Object Text.StringBuilder
    [void]$b.AppendLine("yapi TooManyFields {")
    for ($i = 0; $i -lt $Count; $i++) { [void]$b.AppendLine(("    field_{0}: tamsayi;" -f $i)) }
    [void]$b.AppendLine("}")
    [void]$b.AppendLine("isv ana() -> tamsayi { dondu 0; }")
    return $b.ToString()
}

function New-NominalFixture([int]$Count) {
    $b = New-Object Text.StringBuilder
    for ($i = 0; $i -lt $Count; $i++) {
        [void]$b.AppendLine(("yapi Nominal_{0} {{ value_{0}: tamsayi; }}" -f $i))
    }
    [void]$b.AppendLine("isv ana() -> tamsayi { dondu 0; }")
    return $b.ToString()
}

function New-SymbolFixture([int]$Count) {
    $b = New-Object Text.StringBuilder
    for ($i = 0; $i -lt $Count; $i++) {
        [void]$b.AppendLine(("degisken symbol_{0}: tamsayi = 0;" -f $i))
    }
    [void]$b.AppendLine("isv ana() -> tamsayi { dondu 0; }")
    return $b.ToString()
}

$cases = @(
    @{ Name = "local-table"; File = "local-table-limit.ykn"; Text = (New-LocalFixture 4097); Expected = "ERR_LOCAL_LIMIT" },
    @{ Name = "global-table"; File = "global-table-limit.ykn"; Text = (New-GlobalFixture 1025); Expected = "ERR_GLOBAL_LIMIT|ERR_GLOBAL_INIT_LIMIT" },
    @{ Name = "function-table"; File = "function-table-limit.ykn"; Text = (New-FunctionFixture 4097); Expected = "ERR_FUNCTION_LIMIT" },
    @{ Name = "struct-field-table"; File = "struct-field-limit.ykn"; Text = (New-FieldFixture 33); Expected = "ERR_STRUCT_FIELD_LIMIT" },
    @{ Name = "nominal-type-table"; File = "nominal-type-table-limit.ykn"; Text = (New-NominalFixture 257); Expected = "ERR_NOMINAL_TYPE_LIMIT" },
    @{ Name = "typechecker-symbol-arena"; File = "typechecker-symbol-arena-limit.ykn"; Text = (New-SymbolFixture 16385); Expected = "ERR_SYMBOL_ARENA_LIMIT" }
)

$records = @()
foreach ($case in $cases) {
    $fixture = Join-Path $Work $case.File
    Write-Utf8 $fixture $case.Text
    $fixtureHash = (Get-FileHash $fixture -Algorithm SHA256).Hash.ToLowerInvariant()
    foreach ($stage in @(
        @{ Name = "Stage-0"; Exe = (Join-Path $Root ($CompilerStage0 -replace "/", "\")) },
        @{ Name = "Stage-3"; Exe = (Join-Path $Root ($CompilerStage3 -replace "/", "\")) }
    )) {
        $artifact = Join-Path $Work ("{0}-{1}.exe" -f $case.Name, $stage.Name.ToLowerInvariant())
        Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $output = @(& $stage.Exe $fixture $artifact 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
        $exitCode = $LASTEXITCODE
        $timer.Stop()
        $exists = Test-Path -LiteralPath $artifact -PathType Leaf
        $diagnostic = $output -match $case.Expected
        $crash = $output -match "access violation|segmentation fault|stack overflow|0xC0000005|STATUS_ACCESS_VIOLATION"
        $pass = ($exitCode -ne 0 -and -not $exists -and $diagnostic -and -not $crash)
        $records += [ordered]@{
            case = $case.Name
            stage = $stage.Name
            fixture = (Resolve-Path -Relative $fixture).TrimStart(".\") -replace "\\", "/"
            fixture_sha256 = $fixtureHash
            exit_code = $exitCode
            elapsed_ms = $timer.ElapsedMilliseconds
            artifact_exists = $exists
            diagnostic = $diagnostic
            crash_signature = $crash
            pass = $pass
            output = $output
        }
        if (-not $pass) {
            Write-Host ("FAIL {0}/{1}: exit={2} artifact={3} diagnostic={4} crash={5}`n{6}" -f $case.Name, $stage.Name, $exitCode, $exists, $diagnostic, $crash, $output) -ForegroundColor Red
        } else {
            Write-Host ("PASS {0}/{1}" -f $case.Name, $stage.Name) -ForegroundColor Green
        }
    }
}

$verified = $true
foreach ($record in $records) { if (-not $record.pass) { $verified = $false } }
$outPath = Join-Path $Root ($OutputJson -replace "/", "\")
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.capacity-limits-native-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    capacities = [ordered]@{ local = 4096; global = 1024; function = 4096; struct_fields_per_type = 32; patches = 16384; iat_patches = 16384 }
    records = $records
    verified = $verified
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Capacity limit evidence: $outPath"
Write-Host ("Cases={0} Verified={1}" -f $records.Count, $verified)
if (-not $verified) { exit 1 }
exit 0
