<##
.SYNOPSIS
    YKNLLM YKNA truncation recovery acceptance gate.
.DESCRIPTION
    Generates a valid append-only audit file with the real YKN executable,
    truncates its final byte, then proves the reader rejects the corrupted
    prefix instead of reporting success.
#>
[CmdletBinding()]
param(
    [string]$Compiler = ".\build\bootstrap\stage3.exe",
    [int]$TimeoutMs = 180000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$compilerPath = if ([System.IO.Path]::IsPathRooted($Compiler)) {
    [System.IO.Path]::GetFullPath($Compiler)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
$work = Join-Path $ROOT "build\test-work\yknllm-audit-recovery"
New-Item -ItemType Directory -Path $work -Force | Out-Null
$source = Join-Path $ROOT "tests\llm\33_yknllm_audit_reader.ykn"
$artifact = Join-Path $work "audit-reader.exe"
$audit = Join-Path $ROOT "build\test-work\yknllm-audit-reader.bin"

if (-not (Test-Path -LiteralPath $compilerPath)) { throw "Compiler bulunamadı: $compilerPath" }
if (-not (Test-Path -LiteralPath $source)) { throw "Fixture bulunamadı: $source" }
Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $audit -Force -ErrorAction SilentlyContinue

& $compilerPath $source $artifact *> $null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $artifact)) {
    throw "Audit reader fixture derlenemedi"
}

$first = Start-Process -FilePath $artifact -WorkingDirectory $ROOT -Wait -PassThru -NoNewWindow
if ($first.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $audit)) {
    throw "Geçerli audit fixture oluşturulamadı"
}
$before = (Get-Item -LiteralPath $audit).Length
if ($before -lt 2) { throw "Audit dosyası beklenenden kısa" }

$stream = [System.IO.File]::Open($audit, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
try { $stream.SetLength($before - 1) } finally { $stream.Dispose() }

$secondStdout = Join-Path $work "truncated.stdout.log"
$secondStderr = Join-Path $work "truncated.stderr.log"
Remove-Item -LiteralPath $secondStdout, $secondStderr -Force -ErrorAction SilentlyContinue
$second = Start-Process -FilePath $artifact -WorkingDirectory $ROOT -Wait -PassThru -NoNewWindow `
    -RedirectStandardOutput $secondStdout -RedirectStandardError $secondStderr
if ($second.ExitCode -eq 0) {
    throw "Truncated YKNA dosyası başarılı kabul edildi"
}
Write-Host "[PASS] YKNLLM_AUDIT_RECOVERY_VALID_APPEND_PASS"
Write-Host "[PASS] YKNLLM_AUDIT_TRUNCATION_REJECT_PASS"
exit 0
