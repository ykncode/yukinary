param(
    [Parameter(Mandatory = $true)][string]$Client,
    [Parameter(Mandatory = $true)][string]$NegativeClient
)

$ErrorActionPreference = 'Stop'

function Invoke-DnsFixture {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][int]$Port,
        [Parameter(Mandatory = $true)][bool]$Malformed
    )

    $udp = [System.Net.Sockets.UdpClient]::new()
    $endpoint = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Parse('127.0.0.1'), $Port)
    $udp.Client.Bind($endpoint)
    $resolved = (Resolve-Path $Executable).Path
    $process = Start-Process -FilePath $resolved -PassThru -WorkingDirectory (Split-Path $resolved)
    try {
        $receiveTask = $udp.ReceiveAsync()
        if (-not $receiveTask.Wait(5000)) {
            if (-not $process.HasExited) { $process.Kill() }
            throw "DNS client did not send a query within 5 seconds: $resolved"
        }
        $datagram = $receiveTask.GetAwaiter().GetResult()
        $query = $datagram.Buffer
        $remote = $datagram.RemoteEndPoint
        if ($query.Length -lt 12) { throw "DNS query too short: $($query.Length)" }

        $questionEnd = 12
        while ($questionEnd -lt $query.Length -and $query[$questionEnd] -ne 0) {
            $labelLength = [int]$query[$questionEnd]
            if ($labelLength -gt 63 -or $questionEnd + 1 + $labelLength -ge $query.Length) {
                throw "Malformed DNS question label"
            }
            $questionEnd += 1 + $labelLength
        }
        if ($questionEnd + 4 -ge $query.Length) { throw 'DNS question is truncated' }
        $questionEnd += 5

        if ($Malformed) {
            [byte[]]$response = @(0x42, 0x17, 0x81, 0x80)
        } else {
            $responseLength = $questionEnd + 16
            [byte[]]$response = [byte[]]::new($responseLength)
            [Array]::Copy($query, 0, $response, 0, $questionEnd)
            $response[2] = 0x81
            $response[3] = 0x80
            $response[6] = 0
            $response[7] = 1
            $answer = $questionEnd
            $response[$answer + 0] = 0xC0
            $response[$answer + 1] = 0x0C
            $response[$answer + 2] = 0
            $response[$answer + 3] = 1
            $response[$answer + 4] = 0
            $response[$answer + 5] = 1
            $response[$answer + 6] = 0
            $response[$answer + 7] = 0
            $response[$answer + 8] = 0
            $response[$answer + 9] = 60
            $response[$answer + 10] = 0
            $response[$answer + 11] = 4
            $response[$answer + 12] = 203
            $response[$answer + 13] = 0
            $response[$answer + 14] = 113
            $response[$answer + 15] = 7
        }
        [void]$udp.Send($response, $response.Length, $remote)
        if (-not $process.WaitForExit(5000)) {
            $process.Kill()
            throw "DNS client timed out: $resolved"
        }
        if ($process.ExitCode -ne 0) {
            throw "DNS client failed with exit code $($process.ExitCode): $resolved"
        }
    } finally {
        if (-not $process.HasExited) { $process.Kill() }
        $udp.Dispose()
    }
}

Invoke-DnsFixture -Executable $Client -Port 53535 -Malformed $false
Invoke-DnsFixture -Executable $NegativeClient -Port 53536 -Malformed $true
Write-Host 'DNS generated native acceptance: PASS (positive compression-pointer A + malformed/truncated fail-closed)'
