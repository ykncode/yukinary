[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string[]]$Fixtures = @(
        "tests/codegen/04_unsupported_ast_directive.ykn",
        "tests/codegen/05_unsupported_py_lib.ykn",
        "tests/codegen/06_unsupported_rs_lib.ykn",
        "tests/codegen/07_unsupported_convert.ykn"
    ),
    [string]$OutputJson = "build/reports/unsupported-ast-codegen-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Invoke-Rejection([string]$CompilerRelative, [string]$FixtureRelative, [string]$StageName) {
    $compilerPath = Join-Path $Root ($CompilerRelative -replace "/", "\")
    $fixturePath = Join-Path $Root ($FixtureRelative -replace "/", "\")
    $fixtureName = [IO.Path]::GetFileNameWithoutExtension($FixtureRelative)
    $artifactRelative = "build/test-work/unsupported-ast-{0}-{1}.exe" -f $fixtureName, $StageName.ToLowerInvariant()
    $artifactPath = Join-Path $Root ($artifactRelative -replace "/", "\")
    if (-not (Test-Path $compilerPath -PathType Leaf)) { throw "Compiler not found: $CompilerRelative" }
    if (-not (Test-Path $fixturePath -PathType Leaf)) { throw "Fixture not found: $FixtureRelative" }
    Remove-Item $artifactPath -Force -ErrorAction SilentlyContinue
    $output = @(& $compilerPath $fixturePath $artifactPath 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
    $exitCode = $LASTEXITCODE
    $artifactExists = Test-Path $artifactPath -PathType Leaf
    [ordered]@{
        stage = $StageName
        compiler = $CompilerRelative
        fixture = $FixtureRelative
        artifact = $artifactRelative
        exit_code = $exitCode
        rejected = ($exitCode -ne 0)
        artifact_exists = $artifactExists
        diagnostic = ($output -match "E043|ERR_UNSUPPORTED_AST_NODE")
        output = $output
    }
}

$records = @()
foreach ($fixture in $Fixtures) {
    $records += Invoke-Rejection $CompilerStage0 $fixture "Stage-0"
    $records += Invoke-Rejection $CompilerStage3 $fixture "Stage-3"
}
$verified = $true
foreach ($record in $records) {
    if (-not ($record.rejected -and $record.diagnostic -and -not $record.artifact_exists)) {
        $verified = $false
    }
}

$outPath = Join-Path $Root ($OutputJson -replace "/", "\")
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
$fixtureRecords = @()
foreach ($fixture in $Fixtures) {
    $fixturePath = Join-Path $Root ($fixture -replace "/", "\")
    $fixtureRecords += [ordered]@{
        path = $fixture
        sha256 = (Get-FileHash $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
[ordered]@{
    schema = "yukinary.unsupported-ast-codegen-evidence.v2"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "unsupported codegen AST nodes must fail closed; no artifact"
    fixtures = $fixtureRecords
    records = $records
    verified = $verified
} | ConvertTo-Json -Depth 10 | Set-Content $outPath -Encoding UTF8

Write-Host "Unsupported AST codegen evidence: $outPath"
Write-Host "Cases=$($records.Count) Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
