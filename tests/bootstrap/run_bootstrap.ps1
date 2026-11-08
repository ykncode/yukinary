# ============================================================
# run_bootstrap.ps1 - Yukinary canonical self-host pipeline wrapper
#
# Usage:
#   .\tests\bootstrap\run_bootstrap.ps1
#   .\tests\bootstrap\run_bootstrap.ps1 -SkipBuild
#   .\tests\bootstrap\run_bootstrap.ps1 -KeepArtifacts -Verbose
#
# The implementation lives in tools/build_bootstrap.ps1 so CI and local
# development use exactly the same manifest-driven pipeline.
# ============================================================

param(
    [switch]$SkipBuild,
    [switch]$KeepArtifacts,
    [switch]$Verbose,
    [string]$Cxx = "",
    [ValidateSet("O0", "O1", "O2", "O3", "Os")]
    [string]$Optimization = "O2"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$builder = Join-Path $root "tools\build_bootstrap.ps1"

if (-not (Test-Path -LiteralPath $builder)) {
    Write-Error ("Bootstrap builder not found: " + $builder)
    exit 1
}

$builderArgs = @("-Optimization", $Optimization)
if (-not [string]::IsNullOrWhiteSpace($Cxx)) {
    $builderArgs += @("-Cxx", $Cxx)
}
if ($SkipBuild) {
    $builderArgs += "-SkipBootstrapBuild"
}
if ($KeepArtifacts) {
    $builderArgs += "-KeepArtifacts"
}
if ($Verbose) {
    $builderArgs += "-VerboseOutput"
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $builder @builderArgs
$exitCode = $LASTEXITCODE
exit $exitCode
