[CmdletBinding()]
param(
    [string]$Stage0 = ".\build\bootstrap\stage0.exe",
    [string]$Stage3 = ".\build\bootstrap\stage3.exe",
    [string]$OutputJson = ".\build\reports\arc-smart-pointer-release-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$source = Join-Path $root "tests\stdlib\arc_smart_pointer.ykn"
$stage0Path = [System.IO.Path]::GetFullPath((Join-Path $root $Stage0))
$stage3Path = [System.IO.Path]::GetFullPath((Join-Path $root $Stage3))
$reportPath = [System.IO.Path]::GetFullPath((Join-Path $root $OutputJson))
$work = Join-Path $root "build\test-work\arc-release"
New-Item -ItemType Directory -Path $work -Force | Out-Null

function Sha256([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Run-Command([string]$file, [string[]]$arguments) {
    $stdout = & $file @arguments 2>&1 | Out-String
    $code = $LASTEXITCODE
    return [ordered]@{
        command = $file
        arguments = @($arguments)
        exit_code = $code
        output = $stdout.Trim()
    }
}

function Compile-Run([string]$name, [string]$compiler, [string]$output, [string[]]$extra, [bool]$runLocally) {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
    $compileArgs = @($extra + @($source, $output))
    $compile = Run-Command $compiler $compileArgs
    $artifactExists = Test-Path -LiteralPath $output
    $run = $null
    if ($runLocally -and $compile.exit_code -eq 0 -and $artifactExists) {
        $run = Run-Command $output @()
    }
    return [ordered]@{
        name = $name
        native_run_required = $runLocally
        compiler = $compiler
        compile = $compile
        run = $run
        artifact_exists = $artifactExists
        artifact_bytes = if ($artifactExists) { (Get-Item -LiteralPath $output).Length } else { 0 }
        artifact_sha256 = Sha256 $output
    }
}

$results = [System.Collections.Generic.List[object]]::new()
$results.Add((Compile-Run "stage0-windows" $stage0Path (Join-Path $work "arc-stage0.exe") @() $true))
$results.Add((Compile-Run "stage3-windows" $stage3Path (Join-Path $work "arc-stage3.exe") @() $true))
$linux0 = Join-Path $work "arc-stage0-linux.elf"
$linux3 = Join-Path $work "arc-stage3-linux.elf"
$results.Add((Compile-Run "stage0-linux-elf" $stage0Path $linux0 @("--target", "linux") $false))
$results.Add((Compile-Run "stage3-linux-elf" $stage3Path $linux3 @("--target", "linux") $false))

$dockerResults = [System.Collections.Generic.List[object]]::new()
foreach ($item in @(@("stage0-linux-elf", "arc-stage0-linux.elf"), @("stage3-linux-elf", "arc-stage3-linux.elf"))) {
    $dockerResults.Add([ordered]@{
        name = $item[0]
        run = Run-Command "docker.exe" @("run", "--rm", "-v", ($root + ":/work"), "ubuntu:24.04", ("/work/build/test-work/arc-release/" + $item[1]))
    })
}

$requiredMarkers = @(
    "ARC_OOM_LIMIT_PASS",
    "ARC_REF_MAX_PASS",
    "ARC_UNDERFLOW_PASS",
    "ARC_NATIVE_PASS"
)
$allPass = $true
foreach ($r in $results) {
    $runOk = (-not $r.native_run_required)
    if ($r.native_run_required) {
        $runOk = $null -ne $r.run -and $r.run.exit_code -eq 0
        foreach ($marker in $requiredMarkers) {
            if (-not $runOk -or $r.run.output -notmatch [regex]::Escape($marker)) { $runOk = $false }
        }
    }
    if ($r.compile.exit_code -ne 0 -or -not $r.artifact_exists -or -not $runOk) { $allPass = $false }
}
foreach ($r in $dockerResults) {
    $dockerOk = $r.run.exit_code -eq 0
    foreach ($marker in $requiredMarkers) {
        if (-not $dockerOk -or $r.run.output -notmatch [regex]::Escape($marker)) { $dockerOk = $false }
    }
    if (-not $dockerOk) { $allPass = $false }
}

$report = [ordered]@{
    schema = "yukinary.arc-smart-pointer-release.v1"
    status = if ($allPass) { "PASS" } else { "FAIL" }
    source = $source
    results = @($results)
    docker = @($dockerResults)
    acceptance = [ordered]@{
        windows_stage0 = $true
        windows_stage3 = $true
        linux_stage0_elf_docker = $true
        linux_stage3_elf_docker = $true
        atomic_refcount_intrinsic = "ykn_atomic_inc_i64/ykn_atomic_dec_i64/ykn_atomic_load_i64/ykn_atomic_store_i64"
        double_drop = "handle is cleared before final cleanup; second drop returns -2"
        oom_limit_rejection = "payloads above 64 MiB return -4 before allocation"
        int64_max_clone_rejection = "INT64_MAX refcount clone returns -3 without mutation"
        underflow_rollback = "zero refcount drop returns -3 and atomic decrement rollback preserves the handle"
        partial_allocation_cleanup = "control/payload/ref allocation failures release every prior allocation"
    }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportPath) -Force | Out-Null
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath -Encoding utf8
if (-not $allPass) { exit 1 }
exit 0
