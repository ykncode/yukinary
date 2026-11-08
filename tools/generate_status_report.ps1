<#
.SYNOPSIS
    Generates a release/status report exclusively from JSON test artifacts.
#>
[CmdletBinding()]
param(
    [string]$ParityJson = "build/reports/stage-parity-final-current.json",
    [string]$BootstrapJson = "build/reports/bootstrap-report.json",
    [string]$Fix13Stage0Json = "build/reports/fix13_results_stage0.json",
    [string]$Fix13Stage3Json = "build/reports/fix13_results_stage3.json",
    [string]$FullStage0Json = "build/test-work/stage-parity/matrix-stage0.json",
    [string]$Output = "build/reports/status-report.md"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

function Read-Artifact([string]$Path) {
    $full = if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $root $Path }
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Artifact bulunamadi: $full" }
    return [pscustomobject]@{ Path = $full; Data = (Get-Content -LiteralPath $full -Raw | ConvertFrom-Json) }
}

$parity = Read-Artifact $ParityJson
$bootstrap = Read-Artifact $BootstrapJson
$fix0 = Read-Artifact $Fix13Stage0Json
$fix3 = Read-Artifact $Fix13Stage3Json
$full0 = Read-Artifact $FullStage0Json
$out = if ([IO.Path]::IsPathRooted($Output)) { $Output } else { Join-Path $root $Output }

$p = $parity.Data
$b = $bootstrap.Data
$f0 = $fix0.Data
$f3 = $fix3.Data
$generated = Get-Date -Format "yyyy-MM-ddTHH:mm:ssK"

$lines = @(
    "# Yukinary Generated Status Report",
    "",
    "> Generated: $generated",
    "> Source: JSON artifacts only; no test count is hardcoded.",
    "",
    "## Gate Summary",
    "",
    "| Gate | Result | Evidence |",
    "|---|---|---|",
    ("| Bootstrap | {0} | `{1}` |" -f ($(if ([string]$b.status -eq "PASS") { "PASS" } else { "FAIL" }), $bootstrap.Path)),
    ("| Full parity | {0} | Stage-0 {1}/{2}, Stage-3 {3}/{4} |" -f ($(if ($p.gate_pass) { "PASS" } else { "FAIL" }), $p.compilers.stage0.passed, $p.compilers.stage0.total, $p.compilers.stage3.passed, $p.compilers.stage3.total)),
    ("| Fixture list parity | {0} | observable outcomes: {1} |" -f $p.fixture_list_same, $p.observable_outcomes_same),
    ("| FIX-13 parity | {0} | Stage-0 {1}/{2}, Stage-3 {3}/{4} |" -f $p.fix13.parity_pass, $p.fix13.stage0.passed, $p.fix13.stage0.total, $p.fix13.stage3.passed, $p.fix13.stage3.total),
    "",
    "## Provenance",
    "",
    "| Report | Path | Compiler SHA-256 | Source revision SHA-256 |",
    "|---|---|---|---|",
    ("| Stage-0 full suite | `{0}` | `{1}` | `{2}` |" -f $FullStage0Json, $full0.Data.compiler_sha256, $full0.Data.source_revision_sha256),
    ("| FIX-13 Stage-0 | `{0}` | `{1}` | `{2}` |" -f $Fix13Stage0Json, $f0.compiler_sha256, $f0.source_revision_sha256),
    ("| FIX-13 Stage-3 | `{0}` | `{1}` | `{2}` |" -f $Fix13Stage3Json, $f3.compiler_sha256, $f3.source_revision_sha256),
    "",
    "## Remaining Release Work",
    "",
    "- Product release remains OPEN: GUI, WASM, registry, formal borrow/lifetime, dynamic dispatch, complete async lowering, full YKNLLM inference/training and native process isolation are not closed.",
    "- macOS/ARM64 remains PLANNED/manual-gated; artifact generation is not native execution evidence.",
    "- Weighted code-reality audit is separate from core parity: see build/reports/code-reality-audit.md.",
    "- Historical reports are never used as current release evidence.",
    ""
)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $out) | Out-Null
[IO.File]::WriteAllLines($out, $lines, (New-Object Text.UTF8Encoding($false)))
Write-Host "Generated status report: $out"
