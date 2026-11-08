[CmdletBinding()]
param(
    [string]$ServerPost = "build/http-post-server.exe",
    [string]$ClientPost = "build/http-post-client.exe",
    [string]$ServerChunk = "build/http-chunk-server.exe",
    [string]$ClientChunk = "build/http-chunk-client.exe"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path

function Resolve-RootPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
    return Join-Path $root $Path
}

function Invoke-LoopbackCase([string]$Name, [string]$ServerPath, [string]$ClientPath) {
    $server = $null
    try {
        $server = Start-Process -FilePath (Resolve-RootPath $ServerPath) -WorkingDirectory $root -PassThru -NoNewWindow
        Start-Sleep -Milliseconds 250
        if ($server.HasExited) {
            throw "$Name server exited before client: $($server.ExitCode)"
        }
        $client = Start-Process -FilePath (Resolve-RootPath $ClientPath) -WorkingDirectory $root -Wait -PassThru -NoNewWindow
        if ($client.ExitCode -ne 0) {
            $server.Refresh()
            $serverCode = if ($server.HasExited) { $server.ExitCode } else { "running" }
            throw "$Name client exit=$($client.ExitCode), server exit=$serverCode"
        }
        Write-Host "$Name PASS (client exit=0)" -ForegroundColor Green
    } finally {
        if ($null -ne $server -and -not $server.HasExited) {
            Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

Invoke-LoopbackCase "POST" $ServerPost $ClientPost
Invoke-LoopbackCase "CHUNKED" $ServerChunk $ClientChunk
Write-Host "HTTP POST/chunked native acceptance PASS" -ForegroundColor Green
