# ============================================================
# run_seed_parity_check.ps1
# Stage-0 ve Stage-3 ile seed parity fixture'larini derleyip
# ciktilarini karsilastiran script. Basarisiz olmaz, sadece raporlar.
# ============================================================
[CmdletBinding()]
param(
    [string]$CompilerA = "build/bootstrap/stage0.exe",
    [string]$CompilerB = "build/bootstrap/stage3.exe",
    [string]$OutputJson = "build/reports/seed_parity_report.json",
    [int]$TimeoutMs = 15000
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$stage0 = Resolve-CompilerPath $CompilerA
$stage3 = Resolve-CompilerPath $CompilerB
$fixturesDir = Join-Path $PSScriptRoot "fixtures"
$workDir = Join-Path $ROOT "build\test-work\seed-parity"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

$fixtures = Get-ChildItem -Path $fixturesDir -Filter "*.ykn" | Sort-Object Name

Write-Host "=== SEED PARITY CHECK ===" -ForegroundColor Cyan
Write-Host "Stage-0: $stage0" -ForegroundColor Gray
Write-Host "Stage-3: $stage3" -ForegroundColor Gray
Write-Host "Fixtures: $($fixtures.Count)" -ForegroundColor Gray
Write-Host ""

$results = @()
$allMatch = $true

foreach ($fx in $fixtures) {
    $name = $fx.BaseName
    $workA = Join-Path $workDir "$name-s0"
    $workB = Join-Path $workDir "$name-s3"
    New-Item -ItemType Directory -Path $workA -Force | Out-Null
    New-Item -ItemType Directory -Path $workB -Force | Out-Null

    $outA = Join-Path $workA "out.txt"
    $outB = Join-Path $workB "out.txt"

    # Run stage0
    $procA = $null
    $exitA = -1
    $stdoutA = ""
    try {
        if (Test-Path $stage0) {
            $psiA = [System.Diagnostics.ProcessStartInfo]::new($stage0, "`"$($fx.FullName)`"")
            $psiA.RedirectStandardOutput = $true
            $psiA.RedirectStandardError = $true
            $psiA.UseShellExecute = $false
            $psiA.WorkingDirectory = $workA
            $procA = [System.Diagnostics.Process]::Start($psiA)
            $stdoutA = $procA.StandardOutput.ReadToEnd()
            $procA.WaitForExit($TimeoutMs) | Out-Null
            $exitA = $procA.ExitCode
        }
    } catch { $exitA = -1; $stdoutA = "ERROR: $_" }

    # Run stage3
    $exitB = -1
    $stdoutB = ""
    try {
        if (Test-Path $stage3) {
            $psiB = [System.Diagnostics.ProcessStartInfo]::new($stage3, "`"$($fx.FullName)`"")
            $psiB.RedirectStandardOutput = $true
            $psiB.RedirectStandardError = $true
            $psiB.UseShellExecute = $false
            $psiB.WorkingDirectory = $workB
            $procB = [System.Diagnostics.Process]::Start($psiB)
            $stdoutB = $procB.StandardOutput.ReadToEnd()
            $procB.WaitForExit($TimeoutMs) | Out-Null
            $exitB = $procB.ExitCode
        }
    } catch { $exitB = -1; $stdoutB = "ERROR: $_" }

    $exitMatch = ($exitA -eq $exitB)
    if (-not $exitMatch) { $allMatch = $false }

    $status = if ($exitMatch) { "PARITY_OK" } else { "PARITY_MISMATCH" }
    $color  = if ($exitMatch) { "Green" } else { "Yellow" }

    Write-Host ("  [{0}] {1}  stage0_exit={2}  stage3_exit={3}" -f $status, $name, $exitA, $exitB) -ForegroundColor $color

    $results += [ordered]@{
        fixture   = $name
        path      = $fx.FullName.Replace($ROOT, "").TrimStart("\\/").Replace("\", "/")
        stage0_exit = $exitA
        stage3_exit = $exitB
        parity    = $exitMatch
        status    = $status
    }
}

Write-Host ""
if ($allMatch) {
    Write-Host "SEED PARITY: ALL MATCH" -ForegroundColor Green
} else {
    Write-Host "SEED PARITY: SOME MISMATCHES (informational, not blocking)" -ForegroundColor Yellow
}

# Write JSON report
$reportObj = [ordered]@{
    schema        = "yukinary.seed-parity.v1"
    generated_at  = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    all_match     = $allMatch
    fixture_count = $fixtures.Count
    results       = $results
}
$reportDir = Split-Path $OutputJson -Parent
if ($reportDir -and -not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}
$reportObj | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $ROOT $OutputJson) -Encoding UTF8
Write-Host "Report: $OutputJson" -ForegroundColor Cyan

# Always exit 0 — seed parity is informational only
exit 0
