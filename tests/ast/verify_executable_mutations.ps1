[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$Manifest = "tools/ast_node_manifest.json",
    [int]$TimeoutMs = 20000,
    [string]$OutputJson = "build/reports/ast-executable-mutation-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Root-Path([string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative)) { return [IO.Path]::GetFullPath($Relative) }
    return [IO.Path]::GetFullPath((Join-Path $Root ($Relative -replace "/", "\")))
}

function Quote-ProcessArgument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-CompilerMutation {
    param(
        [string]$CompilerRelative,
        [string]$FixtureRelative,
        [int]$NodeId,
        [string]$NodeName,
        [string]$Stage
    )

    $compilerPath = Root-Path $CompilerRelative
    $fixturePath = Root-Path $FixtureRelative
    $artifactRelative = "build/test-work/ast-mutation/{0}-{1}.exe" -f $NodeName.ToLowerInvariant(), $Stage.ToLowerInvariant()
    $artifactPath = Root-Path $artifactRelative
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) { throw "Compiler not found: $CompilerRelative" }
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) { throw "Fixture not found: $FixtureRelative" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $artifactPath) -Force | Out-Null
    Remove-Item -LiteralPath $artifactPath -Force -ErrorAction SilentlyContinue

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $compilerPath
    $psi.Arguments = "{0} {1} --test-mutate-ast-node {2}" -f (Quote-ProcessArgument $fixturePath), (Quote-ProcessArgument $artifactPath), $NodeId
    $psi.WorkingDirectory = $Root
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    $started = $process.Start()
    if (-not $started) { throw "Could not start compiler: $CompilerRelative" }

    $timedOut = $false
    $killed = $false
    if (-not $process.WaitForExit($TimeoutMs)) {
        $timedOut = $true
        try { $process.Kill() ; $killed = $true } catch { }
        $process.WaitForExit()
    }
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $exitCode = if ($timedOut) { $null } else { $process.ExitCode }
    $combined = ($stdout + "`n" + $stderr).Trim()
    $artifactExists = Test-Path -LiteralPath $artifactPath -PathType Leaf
    $mutationApplied = $combined -match "\[AST-MUTATION\]"
    $diagnostic = $combined -match "E043|ERR_UNSUPPORTED_AST_NODE"
    $rejected = ($null -ne $exitCode -and $exitCode -ne 0)
    $pass = (-not $timedOut -and -not $killed -and $rejected -and $mutationApplied -and $diagnostic -and -not $artifactExists)

    [ordered]@{
        stage = $Stage
        compiler = $CompilerRelative
        node_id = $NodeId
        node_name = $NodeName
        fixture = $FixtureRelative
        fixture_sha256 = (Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant()
        artifact = $artifactRelative
        exit_code = $exitCode
        timed_out = $timedOut
        process_killed = $killed
        rejected = $rejected
        mutation_applied = $mutationApplied
        diagnostic = $diagnostic
        artifact_exists = $artifactExists
        pass = $pass
        output = $combined
    }
}

$manifestObject = Get-Content -LiteralPath (Root-Path $Manifest) -Raw | ConvertFrom-Json
$implemented = @($manifestObject.nodes | Where-Object { [string]$_.status -eq "implemented" })
if ($implemented.Count -eq 0) { throw "Manifest has no implemented nodes" }

$records = @()
foreach ($node in $implemented) {
    $records += Invoke-CompilerMutation $CompilerStage0 ([string]$node.positive_evidence) ([int]$node.id) ([string]$node.name) "Stage-0"
    $records += Invoke-CompilerMutation $CompilerStage3 ([string]$node.positive_evidence) ([int]$node.id) ([string]$node.name) "Stage-3"
}

$failed = @($records | Where-Object { -not $_.pass })
$verified = ($failed.Count -eq 0)
$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $outPath) -Force | Out-Null
[ordered]@{
    schema = "yukinary.ast-executable-mutation-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "Every implemented AST node is mutated at the real ast_kod_uret entry into an unknown codegen tag; Stage-0/Stage-3 must reject with E043 and produce no artifact."
    mutation_tag = 255
    implemented_node_count = $implemented.Count
    case_count = $records.Count
    timeout_ms = $TimeoutMs
    records = $records
    failed_cases = $failed.Count
    verified = $verified
} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "AST executable mutation evidence: $outPath"
Write-Host "Nodes=$($implemented.Count) Cases=$($records.Count) Failed=$($failed.Count) Verified=$verified"
if (-not $verified) {
    $failed | ForEach-Object { Write-Host ("  FAIL: " + $_.stage + " " + $_.node_name + " -> " + $_.output) -ForegroundColor Red }
    exit 1
}
exit 0
