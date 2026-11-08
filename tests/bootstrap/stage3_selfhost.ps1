# ============================================================
# stage3_selfhost.ps1 - compatibility wrapper
#
# Stage-3 generation and determinism checks now live in the single
# manifest-driven pipeline at tools/build_bootstrap.ps1.
# ============================================================

param(
    [switch]$KeepArtifacts
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$builder = Join-Path $root "tools\build_bootstrap.ps1"

$builderArgs = @()
if ($KeepArtifacts) {
    $builderArgs += "-KeepArtifacts"
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $builder @builderArgs
$exitCode = $LASTEXITCODE
exit $exitCode
