# ============================================================
# run_stage_regression.ps1 - compatibility wrapper
#
# The canonical bootstrap implementation is tools/build_bootstrap.ps1.
# This historical entry point is kept so old commands do not create a
# second, divergent Stage-0/1/2/3 pipeline.
# ============================================================

param(
    [switch]$KeepArtifacts
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$builder = Join-Path $root "tools\build_bootstrap.ps1"

# Preserve the old --keep-work spelling used by previous automation.
$keep = $KeepArtifacts -or ($args -contains "--keep-work")
$builderArgs = @()
if ($keep) {
    $builderArgs += "-KeepArtifacts"
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $builder @builderArgs
$exitCode = $LASTEXITCODE
exit $exitCode
