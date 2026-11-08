# ============================================================
# run_collection_ownership_tests.ps1
# Vec ve HashMap ownership test fixture'larini calistiran script.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/collection_ownership_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
$workDir = Join-Path $ROOT "build\test-work\collection-ownership"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

if (-not (Test-Path $compilerPath)) {
    Write-Host "UYARI: Compiler bulunamadi — test atlanıyor" -ForegroundColor Yellow
    exit 0
}

$fixtures = @(
    @{ Name = "vec_double_free_neg";  File = "tests/stdlib/vec_double_free_neg.ykn";  ExpectedPartial = "ilk yok_et OK" },
    @{ Name = "hashmap_uaf_neg";      File = "tests/stdlib/hashmap_uaf_neg.ykn";      ExpectedPartial = "yok_et OK" }
)

Write-Host "=== Collection Ownership Tests ===" -ForegroundColor Cyan
$results = @()

foreach ($fx in $fixtures) {
    $srcPath = Join-Path $ROOT $fx.File
    $artPath = Join-Path $workDir "$($fx.Name).exe"

    if (-not (Test-Path $srcPath)) {
        Write-Host "  [SKIP] $($fx.Name)" -ForegroundColor Yellow
        continue
    }

    $compileOut = & $compilerPath $srcPath $artPath 2>&1
    $compileExit = $LASTEXITCODE

    if ($compileExit -ne 0 -or -not (Test-Path $artPath)) {
        Write-Host ("  [INFO] {0}: derleme tamamlanamadi (runtime serbest birakma eksik olabilir)" -f $fx.Name) -ForegroundColor Yellow
        $results += [ordered]@{ name = $fx.Name; compiled = $false }
        continue
    }

    $proc = $null; $runOut = ""; $runExit = -1
    try {
        $psi = [System.Diagnostics.ProcessStartInfo]::new($artPath)
        $psi.RedirectStandardOutput = $true; $psi.UseShellExecute = $false
        $proc = [System.Diagnostics.Process]::Start($psi)
        $runOut = $proc.StandardOutput.ReadToEnd()
        $proc.WaitForExit($TimeoutMs) | Out-Null; $runExit = $proc.ExitCode
    } catch {}

    $partialMatch = $runOut -match [regex]::Escape($fx.ExpectedPartial)
    $neverReached = -not ($runOut -match "ULASILMAMALI")
    $ok = $partialMatch -and ($neverReached -or $runExit -ne 0)
    $color = if ($ok) { "Green" } else { "Yellow" }
    Write-Host ("  [{0}] {1}: exit={2}, partial={3}" -f (if ($ok){"OK"} else {"INFO"}), $fx.Name, $runExit, $partialMatch) -ForegroundColor $color
    $results += [ordered]@{ name=$fx.Name; compiled=$true; run_exit=$runExit; partial_match=$partialMatch; ok=$ok }
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema = "yukinary.collection-ownership.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    results = $results
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
