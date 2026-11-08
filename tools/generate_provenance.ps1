# tools/generate_provenance.ps1
# Generates build/reports/platform-provenance.json for Release Runner Provenance

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path "$ScriptDir\..").Path
Set-Location $RootDir

$ReportDir = "$RootDir\build\reports"
if (-not (Test-Path $ReportDir)) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}

function Get-FileSha256($path) {
    if (Test-Path $path) {
        return (Get-FileHash -Algorithm SHA256 $path).Hash.ToLowerInvariant()
    }
    return $null
}

function Get-ToolVersion([string]$toolPath, [string]$arguments) {
    if ([string]::IsNullOrWhiteSpace($toolPath) -or -not (Test-Path $toolPath)) {
        return $null
    }
    try {
        return (& $toolPath $arguments 2>&1 | Select-Object -First 1).ToString().Trim()
    } catch {
        return $null
    }
}

function Resolve-GxxPath {
    $configured = $env:YKN_GXX
    if (-not [string]::IsNullOrWhiteSpace($configured) -and (Test-Path $configured)) {
        return (Resolve-Path $configured).Path
    }
    $fromPath = Get-Command g++ -ErrorAction SilentlyContinue
    if ($null -ne $fromPath -and (Test-Path $fromPath.Source)) {
        return (Resolve-Path $fromPath.Source).Path
    }
    $known = @(
        "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe",
        "$env:ProgramFiles\mingw-w64\mingw64\bin\g++.exe"
    )
    foreach ($candidate in $known) {
        if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    }
    return $null
}

function Get-ArtifactRecord([string]$relativePath) {
    $absolute = Join-Path $RootDir $relativePath
    $exists = Test-Path $absolute -PathType Leaf
    $length = $null
    if ($exists) { $length = (Get-Item $absolute).Length }
    return [ordered]@{
        path = $relativePath.Replace('\\', '/')
        exists = $exists
        bytes = $length
        sha256 = if ($exists) { Get-FileSha256 $absolute } else { $null }
    }
}

function Get-GitValue([string[]]$arguments) {
    try {
        $lines = @(& git @arguments 2>$null)
        if ($LASTEXITCODE -ne 0) { return $null }
        if ($arguments -contains "--porcelain") {
            return ($lines -join "`n")
        }
        $value = ($lines | Select-Object -First 1)
        if ($null -ne $value) { return $value.ToString().Trim() }
    } catch { }
    return $null
}

$Stage0Hash = Get-FileSha256 "$RootDir\build\bootstrap\stage0.exe"
$Stage1Hash = Get-FileSha256 "$RootDir\build\bootstrap\stage1.exe"
$Stage2Hash = Get-FileSha256 "$RootDir\build\bootstrap\stage2.exe"
$Stage3Hash = Get-FileSha256 "$RootDir\build\bootstrap\stage3.exe"
$YukinaryHash = Get-FileSha256 "$RootDir\yukinary.exe"
$GxxPath = Resolve-GxxPath
$GxxVersion = Get-ToolVersion $GxxPath "--version"
$GitRevision = Get-GitValue @("rev-parse", "HEAD")
$GitStatus = Get-GitValue @("status", "--porcelain")

$BootstrapMatch = ($Stage2Hash -ne $null -and $Stage2Hash -eq $Stage3Hash)
# The canonical parity artifact is the current final report. The older
# stage-parity.json is historical and must not feed release status.
$StageParityPath = Join-Path $ReportDir "stage-parity-final-current.json"
$StageParityData = $null
$StageParityGate = $false
if (Test-Path $StageParityPath) {
    try {
        $StageParityData = Get-Content $StageParityPath -Raw | ConvertFrom-Json
        $StageParityGate = ($StageParityData.schema -eq "yukinary.stage-parity-report.v1" -and
            $StageParityData.gate_pass -eq $true -and
            $StageParityData.compilers.stage0.failed -eq 0 -and
            $StageParityData.compilers.stage3.failed -eq 0 -and
            $StageParityData.observable_outcomes_same -eq $true)
    } catch { $StageParityGate = $false }
}

$Provenance = [ordered]@{
    timestamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    host_environment = [ordered]@{
        os = [System.Environment]::OSVersion.VersionString
        platform = "Windows"
        architecture = "x86_64"
        powershell_version = $PSVersionTable.PSVersion.ToString()
        python_version = (python --version 2>&1).ToString().Trim()
        compiler_seed_path = $GxxPath
        compiler_seed = $GxxVersion
    }
    bootstrap_artifacts = [ordered]@{
        stage0_sha256 = $Stage0Hash
        stage1_sha256 = $Stage1Hash
        stage2_sha256 = $Stage2Hash
        stage3_sha256 = $Stage3Hash
        yukinary_sha256 = $YukinaryHash
        deterministic_match_stage2_stage3 = $BootstrapMatch
    }
    platform_support_tiers = [ordered]@{
        tier1_native_verified = @(
            [ordered]@{
                target = "windows-x86_64"
                format = "PE32+"
                execution = "Native Windows host; detailed gate in current-status.json"
                test_status = if ($StageParityGate) { "PASS ($($StageParityData.compilers.stage0.passed)/$($StageParityData.compilers.stage0.total) Stage-0; $($StageParityData.compilers.stage3.passed)/$($StageParityData.compilers.stage3.total) Stage-3; equal outcomes)" } else { "UNVERIFIED (current stage-parity evidence absent or failed)" }
            }
        )
        tier2_cross_compiled_validated = @(
            [ordered]@{
                target = "linux-x86_64"
                format = "ELF64"
                execution = "UNVERIFIED"
                test_status = "Durable native execution evidence required"
            },
            [ordered]@{
                target = "macos-x86_64"
                format = "Mach-O 64"
                execution = "UNVERIFIED"
                test_status = "Manual-gated future target; native execution evidence required"
            },
            [ordered]@{
                target = "linux-arm64"
                format = "ELF64 (AArch64)"
                execution = "UNVERIFIED"
                test_status = "Cross-compile/ABI evidence is not native execution"
            },
            [ordered]@{
                target = "macos-arm64"
                format = "Mach-O (AArch64)"
                execution = "PLANNED"
                test_status = "macOS/ARM64 future manual-gated target"
            }
        )
    }
    verification_suites = [ordered]@{
        stage_parity_matrix = if ($StageParityGate) { "$($StageParityData.compilers.stage0.passed)/$($StageParityData.compilers.stage0.total) Stage-0; $($StageParityData.compilers.stage3.passed)/$($StageParityData.compilers.stage3.total) Stage-3 PASS" } else { "UNVERIFIED" }
        fix13_null_safety = if ($StageParityGate -and $StageParityData.fix13.parity_pass) { "$($StageParityData.fix13.stage0.passed)/$($StageParityData.fix13.stage0.total) Stage-0; $($StageParityData.fix13.stage3.passed)/$($StageParityData.fix13.stage3.total) Stage-3 PASS" } else { "UNVERIFIED" }
        session_isolation = "UNVERIFIED (current manifest has no session-isolation artifact validation)"
        incremental_build_suite = "UNVERIFIED (current manifest has no incremental-suite artifact validation)"
        tooling_suite = "UNVERIFIED (current manifest has no tooling-suite artifact validation)"
        abi_regression_matrix = "UNVERIFIED (current manifest has no ABI-suite artifact validation)"
        crossplatform_suite = "UNVERIFIED (per-target native execution evidence is recorded separately)"
    }
}

$JsonContent = $Provenance | ConvertTo-Json -Depth 6
$OutputFile = "$ReportDir\platform-provenance.json"
Set-Content -Path $OutputFile -Value $JsonContent -Encoding UTF8

Write-Host "Provenance report generated: $OutputFile" -ForegroundColor Green

# ABI Manifest Hash
$abiManifestPath = Join-Path $RootDir "docs\abi_manifest.json"
if (Test-Path $abiManifestPath) {
    $abiHash = (Get-FileHash -Path $abiManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "ABI Manifest SHA-256: $abiHash" -ForegroundColor Cyan
}

# ============================================================
# A0.2 current-status manifest
# Bu manifest release iddialarini otomatik PASS saymaz. Her kanit ya
# mevcut artifact/test raporuna dayanir ya da UNVERIFIED olarak kalir.
# ============================================================
$selfHealingReportPath = Join-Path $RootDir "build\reports\self_healing_report.json"
$datasetReportPath = Join-Path $RootDir "build\datasets\self_healing.report"
$selfHealingGate = $false
$datasetSelfValidation = $false
$linuxNativeGate = $false
$sandboxArenaGate = $false
$astCapabilityGate = $false
$lspMemoryGate = $false
$unsupportedAstCodegenGate = $false
$linuxEvidencePath = Join-Path $RootDir "build\reports\dataset-native-execution.json"
$sandboxArenaEvidencePath = Join-Path $RootDir "build\reports\yknllm-sandbox-arena-native.json"
$astCapabilityEvidencePath = Join-Path $RootDir "build\reports\yknllm-ast-capability-gate-native.json"
$lspMemoryEvidencePath = Join-Path $RootDir "build\reports\lsp_memory_report.json"
$unsupportedAstCodegenEvidencePath = Join-Path $RootDir "build\reports\unsupported-ast-codegen-native.json"
$selfHealingData = $null
if (Test-Path $selfHealingReportPath) {
    try {
        $selfHealingData = Get-Content $selfHealingReportPath -Raw | ConvertFrom-Json
        $selfHealingGate = ($selfHealingData.total_cases -eq 120 -and
            $selfHealingData.overall_accuracy_percent -ge 99.0 -and
            $selfHealingData.safety_barrier_violations -eq 0 -and
            $selfHealingData.p95_latency_ms -lt 500.0)
    } catch { $selfHealingGate = $false }
}
if (Test-Path $datasetReportPath) {
    $datasetReportText = Get-Content $datasetReportPath -Raw
    $datasetSelfValidation = ($datasetReportText -match "records=60" -and
        $datasetReportText -match "batch_records=60" -and
        $datasetReportText -match "split\.heldout=5")
}
if (Test-Path $linuxEvidencePath) {
    try {
        $linuxEvidence = Get-Content $linuxEvidencePath -Raw | ConvertFrom-Json
        $linuxNativeGate = ($linuxEvidence.verified -eq $true -and
            $linuxEvidence.exit_code -eq 0 -and
            $linuxEvidence.generator_pass_marker -eq $true -and
            $linuxEvidence.acceptance_pass_marker -eq $true)
    } catch { $linuxNativeGate = $false }
}
if (Test-Path $sandboxArenaEvidencePath) {
    try {
        $sandboxArenaEvidence = Get-Content $sandboxArenaEvidencePath -Raw | ConvertFrom-Json
        $sandboxArenaGate = ($sandboxArenaEvidence.verified -eq $true -and
            $sandboxArenaEvidence.stage0.compile_exit_code -eq 0 -and
            $sandboxArenaEvidence.stage0.run_exit_code -eq 0 -and
            $sandboxArenaEvidence.stage0.pass_marker -eq $true -and
            $sandboxArenaEvidence.stage3.compile_exit_code -eq 0 -and
            $sandboxArenaEvidence.stage3.run_exit_code -eq 0 -and
            $sandboxArenaEvidence.stage3.pass_marker -eq $true)
    } catch { $sandboxArenaGate = $false }
}
if (Test-Path $astCapabilityEvidencePath) {
    try {
        $astCapabilityEvidence = Get-Content $astCapabilityEvidencePath -Raw | ConvertFrom-Json
        $astCapabilityGate = ($astCapabilityEvidence.verified -eq $true -and
            $astCapabilityEvidence.stage0.compile_exit_code -eq 0 -and
            $astCapabilityEvidence.stage0.run_exit_code -eq 0 -and
            $astCapabilityEvidence.stage0.pass_marker -eq $true -and
            $astCapabilityEvidence.stage3.compile_exit_code -eq 0 -and
            $astCapabilityEvidence.stage3.run_exit_code -eq 0 -and
            $astCapabilityEvidence.stage3.pass_marker -eq $true)
    } catch { $astCapabilityGate = $false }
}
if (Test-Path $lspMemoryEvidencePath) {
    try {
        $lspMemoryEvidence = Get-Content $lspMemoryEvidencePath -Raw | ConvertFrom-Json
        $lspMemoryGate = ($lspMemoryEvidence.schema -eq "yukinary.lsp-memory.v2" -and
            $lspMemoryEvidence.lsp_started -eq $true -and
            $lspMemoryEvidence.initialize_response -eq $true -and
            $lspMemoryEvidence.shutdown_response -eq $true -and
            $lspMemoryEvidence.protocol_ok -eq $true -and
            $lspMemoryEvidence.cleanup_ok -eq $true -and
            $lspMemoryEvidence.rss_kb -gt 0 -and
            $lspMemoryEvidence.within_limit -eq $true)
    } catch { $lspMemoryGate = $false }
}
if (Test-Path $unsupportedAstCodegenEvidencePath) {
    try {
        $unsupportedAstCodegenEvidence = Get-Content $unsupportedAstCodegenEvidencePath -Raw | ConvertFrom-Json
        $records = @($unsupportedAstCodegenEvidence.records)
        $stage0Records = @($records | Where-Object { $_.stage -eq "Stage-0" })
        $stage3Records = @($records | Where-Object { $_.stage -eq "Stage-3" })
        $allRejected = ($records.Count -eq 8 -and @($records | Where-Object {
            $_.exit_code -eq 0 -or $_.rejected -ne $true -or
            $_.diagnostic -ne $true -or $_.artifact_exists -ne $false
        }).Count -eq 0)
        $unsupportedAstCodegenGate = ($unsupportedAstCodegenEvidence.schema -eq "yukinary.unsupported-ast-codegen-evidence.v2" -and
            $unsupportedAstCodegenEvidence.verified -eq $true -and
            $stage0Records.Count -eq 4 -and $stage3Records.Count -eq 4 -and $allRejected)
    } catch { $unsupportedAstCodegenGate = $false }
}

$sourceFiles = @(
    "app_full.cpp",
    "tools/dataset_generator.ykn",
    "std/ai/dataset.ykn",
    "std/ai/dataset_batch.ykn",
    "std/ai/dataset_manifest.ykn",
    "tests/llm/41_dataset_generator_acceptance.ykn",
    "std/ai/yknllm_sandbox.ykn",
    "src/diagnostics.ykn",
    "src/codegen.ykn",
    "src/main.ykn",
    "tests/llm/42_yknllm_sandbox_arena.ykn",
    "tests/llm/43_yknllm_sandbox_boundary.ykn",
    "tests/llm/verify_yknllm_sandbox_arena.ps1",
    "src/sandbox_gate.ykn",
    "tests/llm/44_yknllm_ast_capability_gate.ykn",
    "tests/llm/verify_yknllm_ast_capability_gate.ps1",
    "tests/codegen/04_unsupported_ast_directive.ykn",
    "tests/codegen/verify_unsupported_ast.ps1",
    "tools/measure_lsp_memory.ps1",
    "tests/self_healing/test_self_healing_suite.py"
) | Select-Object -Unique | ForEach-Object { Get-ArtifactRecord $_ }

$testEvidence = @(
    [ordered]@{
        name = "stage0-stage3-full-fixture-parity"
        report = "build/reports/stage-parity-final-current.json"
        report_sha256 = Get-FileSha256 $StageParityPath
        verified = $StageParityGate
        stage0_passed = if ($StageParityData) { $StageParityData.compilers.stage0.passed } else { $null }
        stage0_total = if ($StageParityData) { $StageParityData.compilers.stage0.total } else { $null }
        stage3_passed = if ($StageParityData) { $StageParityData.compilers.stage3.passed } else { $null }
        stage3_total = if ($StageParityData) { $StageParityData.compilers.stage3.total } else { $null }
        note = "Current report is schema-checked; both stages must pass and observable outcomes must match"
    },
    [ordered]@{
        name = "self-healing-production-gate"
        report = "build/reports/self_healing_report.json"
        report_sha256 = Get-FileSha256 $selfHealingReportPath
        verified = $selfHealingGate
        note = "120 cases, accuracy >=99%, safety=0, P95 <500ms"
    },
    [ordered]@{
        name = "saf-ykn-dataset-self-validation"
        report = "build/datasets/self_healing.report"
        report_sha256 = Get-FileSha256 $datasetReportPath
        verified = $datasetSelfValidation
        note = "YKDS/YKLM writer-reader-validator and bounded batch self-validation"
    },
    [ordered]@{
        name = "saf-ykn-yknllm-bounded-sandbox-arena"
        report = "build/reports/yknllm-sandbox-arena-native.json"
        report_sha256 = Get-FileSha256 $sandboxArenaEvidencePath
        verified = $sandboxArenaGate
        note = "Stage0 and Stage3 native bounded output arena plus fail-closed capability policy; not OS process isolation"
    },
    [ordered]@{
        name = "saf-ykn-yknllm-ast-capability-gate"
        report = "build/reports/yknllm-ast-capability-gate-native.json"
        report_sha256 = Get-FileSha256 $astCapabilityEvidencePath
        verified = $astCapabilityGate
        note = "Stage0 and Stage3 native AST gate rejects FFI, asm, escape imports, external declarations and process/network symbols"
    },
    [ordered]@{
        name = "stage3-lsp-protocol-memory"
        report = "build/reports/lsp_memory_report.json"
        report_sha256 = Get-FileSha256 $lspMemoryEvidencePath
        verified = $lspMemoryGate
        note = "Stage3 native LSP initialize/shutdown framing, RSS, limit and cleanup; Stage0 parity still open"
    },
    [ordered]@{
        name = "unsupported-ast-codegen-rejection"
        report = "build/reports/unsupported-ast-codegen-native.json"
        report_sha256 = Get-FileSha256 $unsupportedAstCodegenEvidencePath
        verified = $unsupportedAstCodegenGate
        note = "Stage0 and Stage3 reject unsupported AST codegen directives with E043 and produce no artifact"
    }
)

# Native execution kaniti, command log'u veya CI provenance olmadan artifact
# varligindan turetilmez. Bu alan bilinçli olarak OPEN kalabilir.
$nativeExecutionEvidence = @(
    [ordered]@{
        target = "windows-x86_64-self-healing"
        status = if ($selfHealingGate) { "VERIFIED" } else { "UNVERIFIED" }
        evidence = @("build/reports/self_healing_report.json")
        note = "Only the Stage3 120-case self-healing acceptance is proven here; this is not a platform-wide support claim"
    },
    [ordered]@{
        target = "linux-x86_64-docker"
        status = if ($linuxNativeGate) { "VERIFIED" } else { "UNVERIFIED" }
        evidence = @("build/reports/dataset-native-execution.json")
        note = "Docker Ubuntu 24.04 generator and acceptance ELF execution"
    },
    [ordered]@{ target = "macos-arm64"; status = "PLANNED"; evidence = @(); note = "Manual-gated future target" }
)

$codeRealityPath = Join-Path $ReportDir "code-reality-audit.json"
$codeRealityData = $null
if (Test-Path $codeRealityPath) {
    try { $codeRealityData = Get-Content $codeRealityPath -Raw | ConvertFrom-Json } catch { $codeRealityData = $null }
}

$currentStatus = [ordered]@{
    schema = "yukinary.current-status.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    release_gate = "OPEN"
    release_gate_reason = "All native execution claims require durable command or CI evidence; macOS/ARM64 is planned"
    source = [ordered]@{
        revision = $GitRevision
        working_tree_first_status = $GitStatus
        clean = if ($null -eq $GitRevision -or $null -eq $GitStatus) { $null } else { [string]::IsNullOrWhiteSpace($GitStatus) }
        source_files = $sourceFiles
    }
    toolchain = [ordered]@{
        powershell = $PSVersionTable.PSVersion.ToString()
        python = (python --version 2>&1).ToString().Trim()
        gxx_path = $GxxPath
        gxx_version = $GxxVersion
    }
    artifacts = @(
        Get-ArtifactRecord "build/test-work/stage0-native-dataset.exe"
        Get-ArtifactRecord "build/test-work/stage3-writecount.exe"
        Get-ArtifactRecord "build/datasets/self_healing.ykds"
        Get-ArtifactRecord "build/datasets/self_healing.yklm"
        Get-ArtifactRecord "build/datasets/self_healing.report"
        Get-ArtifactRecord "build/reports/self_healing_report.json"
        Get-ArtifactRecord "build/reports/dataset-native-execution.json"
        Get-ArtifactRecord "build/reports/yknllm-sandbox-arena-native.json"
        Get-ArtifactRecord "build/reports/yknllm-ast-capability-gate-native.json"
        Get-ArtifactRecord "build/reports/lsp_memory_report.json"
        Get-ArtifactRecord "build/reports/unsupported-ast-codegen-native.json"
    )
    test_evidence = $testEvidence
    native_execution = $nativeExecutionEvidence
    code_reality_audit = [ordered]@{
        report = "build/reports/code-reality-audit.json"
        report_sha256 = Get-FileSha256 $codeRealityPath
        weighted_completion_percent = if ($codeRealityData) { $codeRealityData.weighted_completion_percent } else { $null }
        compiler_core_estimate_percent = if ($codeRealityData) { $codeRealityData.compiler_core_estimate_percent } else { $null }
        production_finding_count = if ($codeRealityData) { $codeRealityData.audit_counts.production_findings } else { $null }
        release_gate = "OPEN"
        note = "Weighted engineering estimate; never a release approval"
    }
    unverified_product_areas = @(
        "real multi-layer transformer parameter training/backward/export",
        "pure-YKN product gateway and native process/resource sandbox isolation",
        "macOS/ARM64 native execution"
    )
}
$currentStatusPath = Join-Path $ReportDir "current-status.json"
$currentStatus | ConvertTo-Json -Depth 8 | Set-Content -Path $currentStatusPath -Encoding UTF8
Write-Host "Current status manifest generated: $currentStatusPath" -ForegroundColor Yellow