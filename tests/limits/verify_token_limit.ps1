[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$OutputJson = "build/reports/token-limit-native.json",
    [int]$DeclarationCount = 40000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$work = Join-Path $Root "build/test-work/limits"
New-Item -ItemType Directory -Path $work -Force | Out-Null
$fixture = Join-Path $work "token-limit-generated.ykn"

$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine("isv ana(argc: tamsayi, argv: **u8) -> tamsayi {")
for ($i = 0; $i -lt $DeclarationCount; $i++) {
    [void]$builder.AppendLine(("    degisken limit_v_{0}: tamsayi = 0;" -f $i))
}
[void]$builder.AppendLine("    dondu 0;")
[void]$builder.AppendLine("}")
[IO.File]::WriteAllText($fixture, $builder.ToString(), (New-Object Text.UTF8Encoding($false)))

function Run-Case([string]$CompilerRelative, [string]$Stage) {
    $compiler = Join-Path $Root ($CompilerRelative -replace "/", "\")
    $artifactRelative = "build/test-work/limits/token-limit-{0}.exe" -f $Stage.ToLowerInvariant()
    $artifact = Join-Path $Root ($artifactRelative -replace "/", "\")
    Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue
    $output = @(& $compiler $fixture $artifact 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
    $exitCode = $LASTEXITCODE
    $artifactExists = Test-Path -LiteralPath $artifact -PathType Leaf
    $diagnostic = ($output -match "E013|ERR_TOKEN_LIMIT|Token kapasitesi asildi")
    [ordered]@{
        stage = $Stage
        compiler = $CompilerRelative
        fixture = "build/test-work/limits/token-limit-generated.ykn"
        generated_declarations = $DeclarationCount
        exit_code = $exitCode
        artifact_exists = $artifactExists
        diagnostic = $diagnostic
        pass = ($exitCode -ne 0 -and -not $artifactExists -and $diagnostic)
        output = $output
    }
}

$records = @(
    (Run-Case $CompilerStage0 "Stage-0")
    (Run-Case $CompilerStage3 "Stage-3")
)
$verified = $true
foreach ($record in $records) {
    if (-not $record.pass) { $verified = $false }
}

$outPath = Join-Path $Root ($OutputJson -replace "/", "\")
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.token-limit-native-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    token_capacity_under_test = 262144
    fixture_sha256 = (Get-FileHash $fixture -Algorithm SHA256).Hash.ToLowerInvariant()
    records = $records
    verified = $verified
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Token limit evidence: $outPath"
Write-Host "Cases=$($records.Count) Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
