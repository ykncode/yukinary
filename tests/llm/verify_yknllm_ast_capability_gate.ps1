[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [string]$Fixture = "tests/llm/44_yknllm_ast_capability_gate.ykn",
    [string]$OutputJson = "build/reports/yknllm-ast-capability-gate-native.json"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$fixturePath = Join-Path $Root ($Fixture -replace "/", "\")
if (-not (Test-Path $fixturePath -PathType Leaf)) { throw "Fixture not found: $Fixture" }

function Get-Utf8Sha256([string]$value) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return [System.BitConverter]::ToString($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($value))).Replace("-", "").ToLowerInvariant()
    } finally { $sha.Dispose() }
}

function Invoke-NativeCompiler([string]$compilerRelative, [string]$artifactRelative) {
    $compilerPath = Join-Path $Root ($compilerRelative -replace "/", "\")
    $artifactPath = Join-Path $Root ($artifactRelative -replace "/", "\")
    if (-not (Test-Path $compilerPath -PathType Leaf)) { throw "Compiler not found: $compilerRelative" }
    Remove-Item $artifactPath -Force -ErrorAction SilentlyContinue
    $compileOutput = @(& $compilerPath $fixturePath $artifactPath 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
    $compileExit = $LASTEXITCODE
    $artifactExists = Test-Path $artifactPath -PathType Leaf
    $runOutput = ""
    $runExit = $null
    $marker = $false
    $artifactHash = $null
    if ($artifactExists) {
        $runOutput = @(& $artifactPath 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
        $runExit = $LASTEXITCODE
        $marker = $runOutput -match "YKNLLM_AST_CAPABILITY_GATE_PASS"
        $artifactHash = (Get-FileHash $artifactPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return [ordered]@{
        compiler = $compilerRelative
        artifact = $artifactRelative
        compile_exit_code = $compileExit
        artifact_exists = $artifactExists
        artifact_sha256 = $artifactHash
        run_exit_code = $runExit
        pass_marker = $marker
        compile_output_sha256 = Get-Utf8Sha256 $compileOutput
        run_output = $runOutput
    }
}

$stage0 = Invoke-NativeCompiler $CompilerStage0 "build/test-work/yknllm-ast-capability-gate-stage0.exe"
$stage3 = Invoke-NativeCompiler $CompilerStage3 "build/test-work/yknllm-ast-capability-gate-stage3.exe"
$verified = ($stage0.compile_exit_code -eq 0 -and $stage0.artifact_exists -and $stage0.run_exit_code -eq 0 -and $stage0.pass_marker -and
    $stage3.compile_exit_code -eq 0 -and $stage3.artifact_exists -and $stage3.run_exit_code -eq 0 -and $stage3.pass_marker)

$outputPath = Join-Path $Root ($OutputJson -replace "/", "\")
New-Item -ItemType Directory -Path (Split-Path $outputPath -Parent) -Force | Out-Null
[ordered]@{
    schema = "yukinary.yknllm-ast-capability-gate-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "compiler AST gate before prompt AST insertion; native Stage0/Stage3 execution"
    fixture = [ordered]@{ path = $Fixture; sha256 = (Get-FileHash $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant() }
    stage0 = $stage0
    stage3 = $stage3
    verified = $verified
} | ConvertTo-Json -Depth 8 | Set-Content $outputPath -Encoding UTF8

Write-Host "YKNLLM AST capability evidence: $outputPath"
Write-Host "Stage0=$($stage0.pass_marker) Stage3=$($stage3.pass_marker); Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
