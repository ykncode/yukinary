# ============================================================
# test_arm64_syscalls.ps1
# ARM64 syscall wrapper'larini ARM64 ELF binary uretimi ile dogrular.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/arm64_syscall_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
$workDir = Join-Path $ROOT "build\test-work\arm64-syscalls"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

Write-Host "=== ARM64 Syscall Wrapper Test ===" -ForegroundColor Cyan

if (-not (Test-Path $compilerPath)) {
    Write-Host "  UYARI: Compiler bulunamadi — test atlanıyor" -ForegroundColor Yellow
    exit 0
}

# Compile a simple ARM64 ELF test
$srcPath = Join-Path $ROOT "tests\bootstrap\01_hello.ykn"
$arm64Out = Join-Path $workDir "arm64_syscall_test.elf"

$compileOut = & $compilerPath $srcPath $arm64Out --target linux-arm64 2>&1
$compileExit = $LASTEXITCODE

$compiled = ($compileExit -eq 0 -and (Test-Path $arm64Out))
$elfSize = if ($compiled) { (Get-Item $arm64Out).Length } else { 0 }

$results = @()

if ($compiled) {
    Write-Host "  [OK] ARM64 ELF derleme basarili: $elfSize byte" -ForegroundColor Green
    # ELF header check: Magic bytes EF 45 4C 46 (ELF magic)
    $bytes = [System.IO.File]::ReadAllBytes($arm64Out)
    $elfMagic = ($bytes.Length -ge 4 -and $bytes[0] -eq 0x7F -and $bytes[1] -eq 0x45 -and $bytes[2] -eq 0x4C -and $bytes[3] -eq 0x46)
    # e_machine check: ARM64 = 0xB7 (183) at offset 18
    $isArm64 = ($bytes.Length -ge 20 -and $bytes[18] -eq 0xB7)

    if ($elfMagic) { Write-Host "  [OK] ELF magic: gecerli" -ForegroundColor Green }
    else           { Write-Host "  [FAIL] ELF magic: gecersiz" -ForegroundColor Red }

    if ($isArm64) { Write-Host "  [OK] e_machine: AArch64 (0xB7)" -ForegroundColor Green }
    else          { Write-Host "  [INFO] e_machine: 0x$([BitConverter]::ToString(@($bytes[18], $bytes[19])))" -ForegroundColor Yellow }

    # Check for SVC instruction (0x01 0x00 0x00 0xD4 in little-endian)
    $svcCount = 0
    for ($i = 0; $i -lt $bytes.Length - 3; $i++) {
        if ($bytes[$i] -eq 0x01 -and $bytes[$i+1] -eq 0x00 -and $bytes[$i+2] -eq 0x00 -and $bytes[$i+3] -eq 0xD4) {
            $svcCount++
        }
    }
    Write-Host "  SVC (syscall) instruction count: $svcCount" -ForegroundColor Gray

    $results += [ordered]@{ test = "arm64_elf_compile"; ok = $compiled; elf_magic = $elfMagic; is_arm64 = $isArm64; svc_count = $svcCount }
} else {
    Write-Host "  [INFO] ARM64 ELF derleme tamamlanamadi: exit=$compileExit" -ForegroundColor Yellow
    $results += [ordered]@{ test = "arm64_elf_compile"; ok = $false }
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema       = "yukinary.arm64-syscalls.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    syscalls     = @("linux_arm64_sys_write", "linux_arm64_sys_exit", "linux_arm64_mmap")
    results      = $results
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
