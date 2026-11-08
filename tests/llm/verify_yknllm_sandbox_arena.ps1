[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/test-work/stage0-sandbox.exe",
    [string]$CompilerStage3 = "build/test-work/stage3-writecount.exe",
    [string]$Fixture = "tests/llm/42_yknllm_sandbox_arena.ykn",
    [string]$BoundaryFixture = "tests/llm/43_yknllm_sandbox_boundary.ykn",
    [string]$OutputJson = "build/reports/yknllm-sandbox-arena-native.json"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$fixturePath = Join-Path $Root ($Fixture -replace "/", "\")
$boundaryFixturePath = Join-Path $Root ($BoundaryFixture -replace "/", "\")
if (-not (Test-Path $fixturePath -PathType Leaf)) { throw "Fixture not found: $Fixture" }
if (-not (Test-Path $boundaryFixturePath -PathType Leaf)) { throw "Boundary fixture not found: $BoundaryFixture" }

function Invoke-NativeCompiler([string]$compilerRelative, [string]$artifactRelative, [string]$fixtureRelative, [string]$passMarker) {
    $fixturePathLocal = Join-Path $Root ($fixtureRelative -replace "/", "\")
    $compilerPath = Join-Path $Root ($compilerRelative -replace "/", "\")
    $artifactPath = Join-Path $Root ($artifactRelative -replace "/", "\")
    if (-not (Test-Path $compilerPath -PathType Leaf)) { throw "Compiler not found: $compilerRelative" }
    if (Test-Path $artifactPath) { Remove-Item -Force $artifactPath }

    $compileOutput = @(& $compilerPath $fixturePathLocal $artifactPath 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
    $compileExit = $LASTEXITCODE
    $artifactExists = Test-Path $artifactPath -PathType Leaf
    $runOutput = ""
    $runExit = $null
    $marker = $false
    $artifactHash = $null
    if ($artifactExists) {
        $runOutput = @(& $artifactPath 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
        $runExit = $LASTEXITCODE
        $marker = $runOutput -match $passMarker
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
        compile_output_sha256 = [System.BitConverter]::ToString(
            ([System.Security.Cryptography.SHA256]::Create()).ComputeHash(
                [System.Text.Encoding]::UTF8.GetBytes($compileOutput)
            )
        ).Replace("-", "").ToLowerInvariant()
        run_output = $runOutput
    }
}

$stage0 = Invoke-NativeCompiler $CompilerStage0 "build/test-work/yknllm-sandbox-arena-stage0.exe" $Fixture "YKNLLM_SANDBOX_ARENA_PASS"
$stage3 = Invoke-NativeCompiler $CompilerStage3 "build/test-work/yknllm-sandbox-arena-stage3.exe" $Fixture "YKNLLM_SANDBOX_ARENA_PASS"
$boundary0 = Invoke-NativeCompiler $CompilerStage0 "build/test-work/yknllm-sandbox-boundary-stage0.exe" $BoundaryFixture "YKNLLM_SANDBOX_BOUNDARY_PASS"
$boundary3 = Invoke-NativeCompiler $CompilerStage3 "build/test-work/yknllm-sandbox-boundary-stage3.exe" $BoundaryFixture "YKNLLM_SANDBOX_BOUNDARY_PASS"
$verified = ($stage0.compile_exit_code -eq 0 -and $stage0.artifact_exists -and
    $stage0.run_exit_code -eq 0 -and $stage0.pass_marker -and
    $stage3.compile_exit_code -eq 0 -and $stage3.artifact_exists -and
    $stage3.run_exit_code -eq 0 -and $stage3.pass_marker -and
    $boundary0.compile_exit_code -eq 0 -and $boundary0.artifact_exists -and
    $boundary0.run_exit_code -eq 0 -and $boundary0.pass_marker -and
    $boundary3.compile_exit_code -eq 0 -and $boundary3.artifact_exists -and
    $boundary3.run_exit_code -eq 0 -and $boundary3.pass_marker)

$reportDirectory = Join-Path $Root (Split-Path ($OutputJson -replace "/", "\") -Parent)
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
[ordered]@{
    schema = "yukinary.yknllm-sandbox-arena-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "pure-YKN bounded arena and fail-closed capability policy; not OS process isolation"
    fixture = [ordered]@{
        path = $Fixture
        sha256 = (Get-FileHash $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    boundary_fixture = [ordered]@{
        path = $BoundaryFixture
        sha256 = (Get-FileHash $boundaryFixturePath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    stage0 = $stage0
    stage3 = $stage3
    boundary_stage0 = $boundary0
    boundary_stage3 = $boundary3
    verified = $verified
} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $Root ($OutputJson -replace "/", "\")) -Encoding UTF8

Write-Host "YKNLLM sandbox arena evidence: $(Join-Path $Root $OutputJson)"
Write-Host "Arena Stage0=$($stage0.pass_marker) Stage3=$($stage3.pass_marker); Boundary Stage0=$($boundary0.pass_marker) Stage3=$($boundary3.pass_marker); Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
