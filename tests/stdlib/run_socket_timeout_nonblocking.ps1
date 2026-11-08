[CmdletBinding()]
param(
    [string]$TimeoutServer = "build/socket-timeout-server.exe",
    [string]$TimeoutClient = "build/socket-timeout-client.exe",
    [string]$NonblockingServer = "build/socket-nonblocking-server.exe",
    [string]$NonblockingClient = "build/socket-nonblocking-client.exe"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
function Resolve-RootPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
    return Join-Path $root $Path
}
function Invoke-Probe([string]$Name, [string]$ServerPath, [string]$ClientPath) {
    $server = $null
    try {
        $server = Start-Process -FilePath (Resolve-RootPath $ServerPath) -WorkingDirectory $root -PassThru -NoNewWindow
        Start-Sleep -Milliseconds 250
        if ($server.HasExited) { throw "$Name server exited before client: $($server.ExitCode)" }
        $started = [DateTime]::UtcNow
        $client = Start-Process -FilePath (Resolve-RootPath $ClientPath) -WorkingDirectory $root -Wait -PassThru -NoNewWindow
        $elapsed = ([DateTime]::UtcNow - $started).TotalMilliseconds
        if ($client.ExitCode -ne 0) { throw "$Name client exit=$($client.ExitCode)" }
        Write-Host "$Name PASS (client exit=0, elapsed_ms=$([int]$elapsed))" -ForegroundColor Green
    } finally {
        if ($null -ne $server -and -not $server.HasExited) { Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue }
    }
}
Invoke-Probe "TIMEOUT" $TimeoutServer $TimeoutClient
Invoke-Probe "NONBLOCKING" $NonblockingServer $NonblockingClient
Write-Host "Socket timeout/nonblocking native acceptance PASS" -ForegroundColor Green
