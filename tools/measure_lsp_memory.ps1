# ============================================================
# measure_lsp_memory.ps1
# LSP surecinin RSS/bellek kullanımını ölçer.
# LSP başlatır, birkaç istek gönderir, bellek raporlar.
# 500MB sınırı kontrolü yapar.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 10000,
    [string]$OutputJson = "build/reports/lsp_memory_report.json",
    [int]$MaxMemoryMB = 500
)

$ErrorActionPreference = "Stop"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
Write-Host "=== LSP Memory Measurement ===" -ForegroundColor Cyan
Write-Host "Compiler: $compilerPath" -ForegroundColor Gray
Write-Host "Max allowed: $MaxMemoryMB MB" -ForegroundColor Gray

$lspProc = $null
$rssKb = 0
$rssOk = $false
$lspStarted = $false
$initializeResponse = $false
$shutdownResponse = $false
$protocolOk = $false
$cleanupOk = $false
$measurementStatus = "not_started"

function Send-LspFrame([System.Diagnostics.Process]$Process, [string]$Json) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Json)
    $header = "Content-Length: $($bytes.Length)`r`nContent-Type: application/vscode-jsonrpc; charset=utf-8`r`n`r`n"
    $Process.StandardInput.Write($header)
    $Process.StandardInput.Write($Json)
    $Process.StandardInput.Flush()
}

function Read-LspFrame([System.Diagnostics.Process]$Process, [int]$Timeout) {
    $headers = @{}
    while ($true) {
        $lineTask = $Process.StandardOutput.ReadLineAsync()
        if (-not $lineTask.Wait($Timeout)) { throw "LSP header timeout" }
        $line = $lineTask.Result
        if ($null -eq $line) { throw "LSP stdout closed before response" }
        if ($line -eq "") { break }
        $separator = $line.IndexOf(":")
        if ($separator -le 0) { throw "Malformed LSP header: $line" }
        $name = $line.Substring(0, $separator).Trim().ToLowerInvariant()
        $value = $line.Substring($separator + 1).Trim()
        $headers[$name] = $value
    }
    if (-not $headers.ContainsKey("content-length")) { throw "LSP response missing Content-Length" }
    $length = 0
    if (-not [int]::TryParse($headers["content-length"], [ref]$length) -or $length -lt 0 -or $length -gt 16777216) {
        throw "Invalid LSP Content-Length"
    }
    $chars = New-Object char[] $length
    $read = 0
    while ($read -lt $length) {
        $readTask = $Process.StandardOutput.ReadAsync($chars, $read, $length - $read)
        if (-not $readTask.Wait($Timeout)) { throw "LSP body timeout" }
        $count = $readTask.Result
        if ($count -le 0) { throw "LSP stdout closed in response body" }
        $read += $count
    }
    return (-join $chars)
}

if (Test-Path $compilerPath) {
    try {
        # Start LSP process
        $psi = [System.Diagnostics.ProcessStartInfo]::new($compilerPath)
        $psi.Arguments = "--lsp"
        $psi.UseShellExecute = $false
        $psi.RedirectStandardInput = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
        $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
        $lspProc = [System.Diagnostics.Process]::Start($psi)
        $lspStarted = $true

        # LSP initialize response is the protocol-level startup proof.
        Start-Sleep -Milliseconds 100
        $initReq = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"capabilities":{},"rootUri":null}}'
        Send-LspFrame $lspProc $initReq
        $initBody = Read-LspFrame $lspProc $TimeoutMs
        $initJson = $initBody | ConvertFrom-Json
        $initializeResponse = ($initJson.jsonrpc -eq "2.0" -and $initJson.id -eq 1 -and $null -ne $initJson.result)
        if (-not $initializeResponse) { throw "LSP initialize response invalid" }
        Send-LspFrame $lspProc '{"jsonrpc":"2.0","method":"initialized","params":{}}'

        # Measure RSS only after a valid initialize response.
        if (-not $lspProc.HasExited -and $initializeResponse) {
            $rssKb = $lspProc.WorkingSet64 / 1024
            $rssMb = [Math]::Round($rssKb / 1024, 2)
            Write-Host "  RSS: $rssMb MB ($rssKb KB)" -ForegroundColor Cyan
            $measurementStatus = "measured"
            $rssOk = ($rssMb -gt 0 -and $rssMb -lt $MaxMemoryMB)
            if ($rssOk) {
                Write-Host "  [OK] RSS $rssMb MB < $MaxMemoryMB MB limit" -ForegroundColor Green
            } else {
                Write-Host "  [FAIL] RSS $rssMb MB >= $MaxMemoryMB MB limit" -ForegroundColor Red
            }
        }

        # Shutdown must also receive a valid response; then send exit and wait.
        $shutdownReq = '{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'
        Send-LspFrame $lspProc $shutdownReq
        $shutdownBody = Read-LspFrame $lspProc $TimeoutMs
        $shutdownJson = $shutdownBody | ConvertFrom-Json
        $hasShutdownResult = ($shutdownJson.PSObject.Properties.Name -contains "result")
        $hasShutdownError = ($shutdownJson.PSObject.Properties.Name -contains "error")
        $shutdownResponse = ($shutdownJson.jsonrpc -eq "2.0" -and $shutdownJson.id -eq 2 -and $hasShutdownResult -and -not $hasShutdownError)
        if (-not $shutdownResponse) { throw "LSP shutdown response invalid" }
        Send-LspFrame $lspProc '{"jsonrpc":"2.0","method":"exit","params":null}'
        $cleanupOk = $lspProc.WaitForExit([Math]::Min($TimeoutMs, 2000))
        if (-not $cleanupOk) { throw "LSP process did not exit after shutdown/exit" }
        $protocolOk = ($initializeResponse -and $shutdownResponse -and $cleanupOk)
    } catch {
        Write-Host "  [FAIL] LSP protocol measurement: $($_.Exception.Message)" -ForegroundColor Red
        $measurementStatus = "protocol_failure"
    } finally {
        if ($lspProc -and -not $lspProc.HasExited) {
            try { $lspProc.Kill() } catch { }
            try { $lspProc.WaitForExit(2000) } catch { }
        }
        if ($lspProc) { $cleanupOk = $lspProc.HasExited }
    }
} else {
    Write-Host "  [FAIL] Compiler bulunamadi - LSP olcumu yapilmadi" -ForegroundColor Red
    $measurementStatus = "compiler_missing"
}

$rssMb = if ($rssKb -gt 0) { [Math]::Round($rssKb / 1024, 2) } else { 0 }

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
$report = [ordered]@{
    schema           = "yukinary.lsp-memory.v2"
    generated_at     = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    lsp_started      = $lspStarted
    initialize_response = $initializeResponse
    shutdown_response = $shutdownResponse
    protocol_ok      = $protocolOk
    cleanup_ok       = $cleanupOk
    rss_kb           = $rssKb
    rss_mb           = $rssMb
    max_allowed_mb   = $MaxMemoryMB
    within_limit     = $rssOk
    limit_check      = "500MB"
    measurement_status = $measurementStatus
    notes            = "PASS requires started process, valid initialize/shutdown JSON-RPC responses, RSS > 0, limit compliance and cleanup"
}
$report | ConvertTo-Json | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8
Write-Host "Report: $OutputJson" -ForegroundColor Cyan
if ($lspStarted -and $protocolOk -and $cleanupOk -and $rssKb -gt 0 -and $rssOk) { exit 0 } else { exit 1 }
