[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$Manifest = "tools/ast_node_manifest.json",
    [string]$OutputJson = "build/reports/ast-node-coverage.json",
    [string]$StageParityReport = "build/reports/stage-parity-final-current.json",
    [string]$MutationScript = "tests/ast/verify_executable_mutations.ps1",
    [string]$MutationReport = "build/reports/ast-executable-mutation-native.json",
    [string]$AsyncExecutionScript = "tests/async/verify_async_execution.ps1",
    [string]$AsyncExecutionReport = "build/reports/async-coroutine-execution-native.json",
    [string]$AsyncTypecheckerScript = "tests/async/verify_async_typechecker.ps1",
    [string]$AsyncTypecheckerReport = "build/reports/async-typechecker-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Root-Path([string]$Relative) {
    return Join-Path $Root ($Relative -replace "/", "\")
}

$manifestPath = Root-Path $Manifest
$astPath = Root-Path "src/ast.ykn"
$codegenPath = Root-Path "src/codegen.ykn"
$seedPath = Root-Path "app_full.cpp"
$manifestObject = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$astText = Get-Content -LiteralPath $astPath -Raw
$codegenText = Get-Content -LiteralPath $codegenPath -Raw
$seedText = Get-Content -LiteralPath $seedPath -Raw

$declarations = @{}
foreach ($match in [regex]::Matches($astText, '(?m)^\s*degisken\s+(AST_[A-Z0-9_]+)\s*=\s*(\d+)\s*;')) {
    $declarations[$match.Groups[1].Value] = [int]$match.Groups[2].Value
}

$nodes = @($manifestObject.nodes)
$allowedStatuses = @("implemented", "compile-time-only", "unsupported", "intentional-no-op")
$errors = @()
$seenIds = @{}
$seenNames = @{}

foreach ($node in $nodes) {
    if ($seenIds.ContainsKey([int]$node.id)) { $errors += "duplicate id: $($node.id)" }
    if ($seenNames.ContainsKey([string]$node.name)) { $errors += "duplicate name: $($node.name)" }
    $seenIds[[int]$node.id] = $true
    $seenNames[[string]$node.name] = $true
    if (-not $declarations.ContainsKey([string]$node.name)) { $errors += "manifest node missing from src/ast.ykn: $($node.name)" }
    elseif ($declarations[[string]$node.name] -ne [int]$node.id) { $errors += "id mismatch: $($node.name) manifest=$($node.id) source=$($declarations[[string]$node.name])" }
    if ($allowedStatuses -notcontains [string]$node.status) { $errors += "invalid status: $($node.name) -> $($node.status)" }
    if ($node.status -ne "unsupported" -and [string]::IsNullOrWhiteSpace([string]$node.positive_evidence)) { $errors += "missing positive evidence: $($node.name)" }
    if ($node.status -eq "unsupported" -and [string]::IsNullOrWhiteSpace([string]$node.negative_evidence)) { $errors += "missing negative evidence: $($node.name)" }
    foreach ($evidenceProperty in @("positive_evidence", "negative_evidence")) {
        $evidence = [string]$node.$evidenceProperty
        if (-not [string]::IsNullOrWhiteSpace($evidence) -and -not (Test-Path -LiteralPath (Root-Path $evidence) -PathType Leaf)) {
            $errors += "evidence file missing: $($node.name) -> $evidence"
        }
    }
}

$expectedIds = 0..($declarations.Count - 1)
foreach ($expectedId in $expectedIds) {
    if (-not $seenIds.ContainsKey($expectedId)) { $errors += "manifest missing source id: $expectedId" }
}
if ($declarations.Count -ne $nodes.Count) { $errors += "manifest/source node count mismatch: manifest=$($nodes.Count) source=$($declarations.Count)" }

$referenceRecords = @()
foreach ($node in $nodes) {
    $referenceCount = ([regex]::Matches(($codegenText + "`n" + $seedText), "\b$([regex]::Escape([string]$node.name))\b")).Count
    $referenceRecords += [ordered]@{
        id = [int]$node.id
        name = [string]$node.name
        status = [string]$node.status
        codegen_or_seed_references = $referenceCount
        positive_evidence = [string]$node.positive_evidence
        negative_evidence = [string]$node.negative_evidence
    }
    if ($node.status -eq "implemented" -and $referenceCount -eq 0) { $errors += "implemented node has no codegen/seed reference: $($node.name)" }
}

$boolFixture = Root-Path "tests/ast/01_boolean_literals.ykn"
$boolRecords = @()
foreach ($compilerRelative in @($CompilerStage0, $CompilerStage3)) {
    $compilerPath = Root-Path $compilerRelative
    $stageLabel = if ($compilerRelative -match "stage3") { "Stage-3" } else { "Stage-0" }
    $artifactRelative = "build/test-work/ast-coverage-boolean-$($stageLabel.ToLowerInvariant()).exe"
    $artifactPath = Root-Path $artifactRelative
    Remove-Item -LiteralPath $artifactPath -Force -ErrorAction SilentlyContinue
    $compileOutput = @(& $compilerPath $boolFixture $artifactPath 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
    $compileExit = $LASTEXITCODE
    $artifactExists = Test-Path -LiteralPath $artifactPath -PathType Leaf
    $runExit = $null
    if ($compileExit -eq 0 -and $artifactExists) {
        & $artifactPath | Out-Null
        $runExit = $LASTEXITCODE
    }
    $boolRecords += [ordered]@{
        stage = $stageLabel
        compiler = $compilerRelative
        fixture = "tests/ast/01_boolean_literals.ykn"
        compile_exit = $compileExit
        artifact_exists = $artifactExists
        run_exit = $runExit
        pass = ($compileExit -eq 0 -and $artifactExists -and $runExit -eq 0)
        output = $compileOutput
    }
}

$unsupportedScript = Join-Path $Root "tests/codegen/verify_unsupported_ast.ps1"
& $unsupportedScript -CompilerStage0 $CompilerStage0 -CompilerStage3 $CompilerStage3 -OutputJson "build/reports/unsupported-ast-codegen-native.json" | Out-Null
$unsupportedExit = $LASTEXITCODE
$unsupportedReportPath = Root-Path "build/reports/unsupported-ast-codegen-native.json"
$unsupportedReport = Get-Content -LiteralPath $unsupportedReportPath -Raw | ConvertFrom-Json
$unsupportedVerified = ($unsupportedReport.PSObject.Properties.Name -contains "verified" -and [bool]$unsupportedReport.verified)
if ($unsupportedExit -ne 0 -or -not $unsupportedVerified) { $errors += "unsupported AST native gate failed" }

$asyncScriptPath = Root-Path $AsyncExecutionScript
$asyncEvidenceReport = $null
$asyncCaseCount = 0
$asyncVerified = $false
if (-not (Test-Path -LiteralPath $asyncScriptPath -PathType Leaf)) {
    $errors += "async execution script missing: $AsyncExecutionScript"
} else {
    & $asyncScriptPath -CompilerStage0 $CompilerStage0 -CompilerStage3 $CompilerStage3 -OutputJson $AsyncExecutionReport | Out-Null
    $asyncExit = $LASTEXITCODE
    $asyncReportPath = Root-Path $AsyncExecutionReport
    if (Test-Path -LiteralPath $asyncReportPath -PathType Leaf) {
        $asyncEvidenceReport = Get-Content -LiteralPath $asyncReportPath -Raw | ConvertFrom-Json
        $asyncProperties = @($asyncEvidenceReport.PSObject.Properties.Name)
        if ($asyncProperties -notcontains "case_count" -or $asyncProperties -notcontains "verified") {
            $errors += "async execution report schema is invalid"
        } else {
            $asyncCaseCount = [int]$asyncEvidenceReport.case_count
            $asyncVerified = [bool]$asyncEvidenceReport.verified
        }
        if ($asyncExit -ne 0 -or -not $asyncVerified) { $errors += "async coroutine native execution failed" }
    } else {
        $errors += "async execution report missing: $AsyncExecutionReport"
    }
}

$asyncTypecheckerScriptPath = Root-Path $AsyncTypecheckerScript
$asyncTypecheckerEvidenceReport = $null
$asyncTypecheckerCaseCount = 0
$asyncTypecheckerVerified = $false
if (-not (Test-Path -LiteralPath $asyncTypecheckerScriptPath -PathType Leaf)) {
    $errors += "async typechecker script missing: $AsyncTypecheckerScript"
} else {
    & $asyncTypecheckerScriptPath -CompilerStage0 $CompilerStage0 -CompilerStage3 $CompilerStage3 -OutputJson $AsyncTypecheckerReport | Out-Null
    $asyncTypecheckerExit = $LASTEXITCODE
    $asyncTypecheckerReportPath = Root-Path $AsyncTypecheckerReport
    if (Test-Path -LiteralPath $asyncTypecheckerReportPath -PathType Leaf) {
        $asyncTypecheckerEvidenceReport = Get-Content -LiteralPath $asyncTypecheckerReportPath -Raw | ConvertFrom-Json
        $asyncTypecheckerProperties = @($asyncTypecheckerEvidenceReport.PSObject.Properties.Name)
        if ($asyncTypecheckerProperties -notcontains "case_count" -or $asyncTypecheckerProperties -notcontains "verified") {
            $errors += "async typechecker report schema is invalid"
        } else {
            $asyncTypecheckerCaseCount = [int]$asyncTypecheckerEvidenceReport.case_count
            $asyncTypecheckerVerified = [bool]$asyncTypecheckerEvidenceReport.verified
        }
        if ($asyncTypecheckerExit -ne 0 -or -not $asyncTypecheckerVerified) { $errors += "async typechecker native gate failed" }
    } else {
        $errors += "async typechecker report missing: $AsyncTypecheckerReport"
    }
}

$mutationScriptPath = Root-Path $MutationScript
$mutationEvidenceReport = $null
$mutationCaseCount = 0
$mutationNodeCount = 0
$mutationVerified = $false
if (-not (Test-Path -LiteralPath $mutationScriptPath -PathType Leaf)) {
    $errors += "executable AST mutation script missing: $MutationScript"
    $mutationEvidenceReport = $null
} else {
    & $mutationScriptPath -CompilerStage0 $CompilerStage0 -CompilerStage3 $CompilerStage3 -Manifest $Manifest -OutputJson $MutationReport | Out-Null
    $mutationExit = $LASTEXITCODE
    $mutationReportPath = Root-Path $MutationReport
    if (Test-Path -LiteralPath $mutationReportPath -PathType Leaf) {
        $mutationEvidenceReport = Get-Content -LiteralPath $mutationReportPath -Raw | ConvertFrom-Json
        $mutationProperties = @($mutationEvidenceReport.PSObject.Properties.Name)
        if ($mutationProperties -notcontains "implemented_node_count" -or $mutationProperties -notcontains "case_count" -or $mutationProperties -notcontains "verified") {
            $errors += "executable AST mutation report schema is invalid: $($mutationProperties -join ',')"
            $mutationVerified = $false
        } else {
            $mutationVerified = [bool]$mutationEvidenceReport.verified
            $mutationCaseCount = [int]$mutationEvidenceReport.case_count
            $mutationNodeCount = [int]$mutationEvidenceReport.implemented_node_count
        }
        if ($mutationExit -ne 0 -or -not $mutationVerified) { $errors += "executable AST mutation native gate failed" }
        if ($mutationProperties -contains "implemented_node_count" -and [int]$mutationEvidenceReport.implemented_node_count -ne @($nodes | Where-Object { $_.status -eq "implemented" }).Count) {
            $errors += "executable AST mutation node count does not match manifest"
        }
    } else {
        $mutationEvidenceReport = $null
        $errors += "executable AST mutation report missing: $MutationReport"
    }
}
foreach ($boolRecord in $boolRecords) {
    if (-not $boolRecord.pass) { $errors += "boolean AST native evidence failed: $($boolRecord.stage)" }
}

$stageParityPath = Root-Path $StageParityReport
$stageParity = $null
if (Test-Path -LiteralPath $stageParityPath -PathType Leaf) {
    $stageParity = Get-Content -LiteralPath $stageParityPath -Raw | ConvertFrom-Json
    if ($stageParity.compilers.stage0.passed -ne $stageParity.compilers.stage0.total -or $stageParity.compilers.stage3.passed -ne $stageParity.compilers.stage3.total -or -not $stageParity.parity_pass) {
        $errors += "stage parity report is not a complete PASS"
    }
} else {
    $errors += "stage parity evidence missing: build/reports/stage-parity-bool-ast.json"
}

$mutationEvidence = $null
if ($null -ne $mutationEvidenceReport) {
    $mutationEvidence = [ordered]@{
        report = $MutationReport
        cases = $mutationCaseCount
        nodes = $mutationNodeCount
        verified = $mutationVerified
    }
}
$gatePass = ($errors.Count -eq 0)
$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path $outPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.ast-node-coverage.v2"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    source = [ordered]@{ ast = "src/ast.ykn"; codegen = "src/codegen.ykn"; seed = "app_full.cpp" }
    node_count = $nodes.Count
    status_counts = [ordered]@{
        implemented = @($nodes | Where-Object { $_.status -eq "implemented" }).Count
        compile_time_only = @($nodes | Where-Object { $_.status -eq "compile-time-only" }).Count
        unsupported = @($nodes | Where-Object { $_.status -eq "unsupported" }).Count
        intentional_no_op = @($nodes | Where-Object { $_.status -eq "intentional-no-op" }).Count
    }
    nodes = $referenceRecords
    boolean_native_evidence = $boolRecords
    unsupported_native_evidence = [ordered]@{ report = "build/reports/unsupported-ast-codegen-native.json"; cases = $unsupportedReport.records.Count; verified = $unsupportedVerified }
    async_coroutine_execution_evidence = [ordered]@{ report = $AsyncExecutionReport; cases = $asyncCaseCount; verified = $asyncVerified }
    async_typechecker_evidence = [ordered]@{ report = $AsyncTypecheckerReport; cases = $asyncTypecheckerCaseCount; verified = $asyncTypecheckerVerified }
    executable_mutation_evidence = $mutationEvidence
    stage_parity_evidence = $StageParityReport
    validation_errors = $errors
    gate_pass = $gatePass
} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "AST node coverage report: $outPath"
Write-Host "Nodes=$($nodes.Count) Errors=$($errors.Count) Gate=$gatePass"
if (-not $gatePass) {
    $errors | ForEach-Object { Write-Host ("  ERROR: " + $_) -ForegroundColor Red }
    exit 1
}
exit 0
