[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [int]$CompileTimeoutMs = 30000,
    [int]$RuntimeTimeoutMs = 15000,
    [string]$OutputJson = "build/reports/network-http-release-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$WorkRelative = "build/test-work/network-http-release"
$Work = Join-Path $Root ($WorkRelative -replace "/", "\")
New-Item -ItemType Directory -Path $Work -Force | Out-Null

function Root-Path([string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative)) { return [IO.Path]::GetFullPath($Relative) }
    return [IO.Path]::GetFullPath((Join-Path $Root ($Relative -replace "/", "\")))
}

function Quote-Argument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-CapturedProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [int]$TimeoutMs,
        [string]$WorkingDirectory = $Root
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join " ")
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    $started = $process.Start()
    if (-not $started) { throw "Process could not start: $FilePath" }
    # Read asynchronously before waiting. Stage-3 emits enough diagnostics to
    # fill a redirected pipe; waiting first would deadlock until the timeout.
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($TimeoutMs)
    $killed = $false
    if ($timedOut) {
        try { $process.Kill(); $killed = $true } catch { }
        $process.WaitForExit()
    }
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    [ordered]@{
        exit_code = if ($timedOut) { $null } else { $process.ExitCode }
        timed_out = $timedOut
        killed = $killed
        stdout = $stdout
        stderr = $stderr
    }
}

function New-Case([string]$Name, [string]$Server, [string]$Client) {
    [ordered]@{ name = $Name; server = $Server; client = $Client }
}

$cases = @(
    (New-Case "http_get_content_length" "tests/stdlib/http_native_server.ykn" "tests/stdlib/http_native_client.ykn"),
    (New-Case "http_get_partial_reads" "tests/stdlib/http_native_split_server.ykn" "tests/stdlib/http_native_split_client.ykn"),
    (New-Case "http_post_content_length" "tests/stdlib/http_native_post_server.ykn" "tests/stdlib/http_native_post_client.ykn"),
    (New-Case "http_chunked" "tests/stdlib/http_native_chunk_server.ykn" "tests/stdlib/http_native_chunk_client.ykn"),
    (New-Case "socket_timeout" "tests/stdlib/socket_timeout_probe_server.ykn" "tests/stdlib/socket_timeout_probe_client.ykn"),
    (New-Case "socket_nonblocking" "tests/stdlib/socket_nonblocking_probe_server.ykn" "tests/stdlib/socket_nonblocking_probe_client.ykn")
)

$compilerMatrix = @(
    [ordered]@{ stage = "Stage-0"; relative = $CompilerStage0; path = (Root-Path $CompilerStage0) },
    [ordered]@{ stage = "Stage-3"; relative = $CompilerStage3; path = (Root-Path $CompilerStage3) }
)
$targetMatrix = @(
    [ordered]@{ target = "windows"; flag = @(); extension = ".exe" },
    [ordered]@{ target = "linux"; flag = @("--target", "linux"); extension = ".elf" }
)

$records = New-Object System.Collections.Generic.List[object]
$artifacts = @{}
$compileFailures = 0

foreach ($compiler in $compilerMatrix) {
    if (-not (Test-Path -LiteralPath $compiler.path -PathType Leaf)) { throw "Compiler not found: $($compiler.relative)" }
    foreach ($target in $targetMatrix) {
        $targetDir = Join-Path $Work (("{0}-{1}" -f $compiler.stage.ToLowerInvariant(), $target.target))
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        foreach ($case in $cases) {
            $serverName = "{0}-server{1}" -f $case.name, $target.extension
            $clientName = "{0}-client{1}" -f $case.name, $target.extension
            $serverPath = Join-Path $targetDir $serverName
            $clientPath = Join-Path $targetDir $clientName
            $caseKey = "{0}|{1}|{2}" -f $compiler.stage, $target.target, $case.name
            $artifacts[$caseKey] = [ordered]@{ server = $serverPath; client = $clientPath }
            foreach ($role in @("server", "client")) {
                $sourceRelative = [string]$case[$role]
                $outputPath = if ($role -eq "server") { $serverPath } else { $clientPath }
                Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
                $compileArgs = @((Root-Path $sourceRelative), $outputPath) + @($target.flag)
                $result = Invoke-CapturedProcess $compiler.path $compileArgs $CompileTimeoutMs
                $artifactExists = Test-Path -LiteralPath $outputPath -PathType Leaf
                $pass = ($result.exit_code -eq 0 -and -not $result.timed_out -and $artifactExists)
                if (-not $pass) { $compileFailures++ }
                $records.Add([ordered]@{
                    kind = "compile"
                    stage = $compiler.stage
                    target = $target.target
                    case = $case.name
                    role = $role
                    source = $sourceRelative
                    artifact = $outputPath.Substring($Root.Length + 1).Replace("\", "/")
                    exit_code = $result.exit_code
                    timed_out = $result.timed_out
                    artifact_exists = $artifactExists
                    pass = $pass
                    stdout = $result.stdout
                    stderr = $result.stderr
                })
            }
        }
    }
}

function Invoke-WindowsPair([string]$Stage, [string]$CaseName, [string]$ServerPath, [string]$ClientPath) {
    $server = $null
    $client = $null
    $clientExit = $null
    $timedOut = $false
    $errorText = ""
    try {
        # Use Process directly so ExitCode is read from a refreshed process
        # handle. Start-Process can expose a stale/null ExitCode object here.
        $serverInfo = New-Object System.Diagnostics.ProcessStartInfo
        $serverInfo.FileName = $ServerPath
        $serverInfo.WorkingDirectory = $Root
        $serverInfo.UseShellExecute = $false
        $serverInfo.CreateNoWindow = $true
        $server = New-Object System.Diagnostics.Process
        $server.StartInfo = $serverInfo
        if (-not $server.Start()) { throw "server could not start" }
        Start-Sleep -Milliseconds 250
        $server.Refresh()
        if ($server.HasExited) { throw "server exited early: $($server.ExitCode)" }

        $clientInfo = New-Object System.Diagnostics.ProcessStartInfo
        $clientInfo.FileName = $ClientPath
        $clientInfo.WorkingDirectory = $Root
        $clientInfo.UseShellExecute = $false
        $clientInfo.CreateNoWindow = $true
        $client = New-Object System.Diagnostics.Process
        $client.StartInfo = $clientInfo
        if (-not $client.Start()) { throw "client could not start" }
        if (-not $client.WaitForExit($RuntimeTimeoutMs)) {
            $timedOut = $true
            try { $client.Kill() } catch { }
            $client.WaitForExit()
        }
        if (-not $timedOut) {
            $client.Refresh()
            $clientExit = [int]$client.ExitCode
        }
    } catch {
        $errorText = $_.Exception.Message
    } finally {
        if ($null -ne $client) {
            try { $client.Refresh(); if (-not $client.HasExited) { $client.Kill(); $client.WaitForExit() } } catch { }
            $client.Dispose()
        }
        if ($null -ne $server) {
            try { $server.Refresh(); if (-not $server.HasExited) { $server.Kill(); $server.WaitForExit() } } catch { }
            $server.Dispose()
        }
    }
    [ordered]@{
        kind = "runtime"
        stage = $Stage
        target = "windows"
        case = $CaseName
        client_exit = $clientExit
        timed_out = $timedOut
        error = $errorText
        pass = (-not $timedOut -and $clientExit -eq 0 -and [string]::IsNullOrEmpty($errorText))
    }
}

function Invoke-LinuxPair([string]$Stage, [string]$CaseName, [string]$ServerPath, [string]$ClientPath) {
    # Artifacts live below the mounted project root; use their root-relative
    # paths instead of only their basenames.
    $serverRelative = $ServerPath.Substring($Root.Length + 1).Replace("\", "/")
    $clientRelative = $ClientPath.Substring($Root.Length + 1).Replace("\", "/")
    $server = "/work/" + $serverRelative
    $client = "/work/" + $clientRelative
    $command = "chmod +x '$server' '$client'; '$server' & server_pid=`$!; sleep 0.25; '$client'; client_rc=`$?; kill `$server_pid 2>/dev/null || true; wait `$server_pid 2>/dev/null || true; exit `$client_rc"
    $dockerArgs = @("run", "--rm", "-v", ("{0}:/work" -f $Root), "-w", "/work", "ubuntu:24.04", "bash", "-lc", $command)
    $result = Invoke-CapturedProcess "docker" $dockerArgs $RuntimeTimeoutMs
    [ordered]@{
        kind = "runtime"
        stage = $Stage
        target = "linux"
        case = $CaseName
        client_exit = $result.exit_code
        timed_out = $result.timed_out
        error = $result.stderr
        pass = ($result.exit_code -eq 0 -and -not $result.timed_out)
        stdout = $result.stdout
        stderr = $result.stderr
    }
}

$runtimeFailures = 0
foreach ($compiler in $compilerMatrix) {
    foreach ($target in $targetMatrix) {
        foreach ($case in $cases) {
            $caseKey = "{0}|{1}|{2}" -f $compiler.stage, $target.target, $case.name
            $paths = $artifacts[$caseKey]
            $compileRows = @($records | Where-Object { $_.kind -eq "compile" -and $_.stage -eq $compiler.stage -and $_.target -eq $target.target -and $_.case -eq $case.name })
            if (@($compileRows | Where-Object { -not $_.pass }).Count -gt 0) {
                $runtimeRow = [ordered]@{ kind = "runtime"; stage = $compiler.stage; target = $target.target; case = $case.name; skipped = $true; pass = $false; error = "compile failed" }
            } elseif ($target.target -eq "windows") {
                $runtimeRow = Invoke-WindowsPair $compiler.stage $case.name $paths.server $paths.client
            } else {
                $runtimeRow = Invoke-LinuxPair $compiler.stage $case.name $paths.server $paths.client
            }
            if (-not $runtimeRow.pass) { $runtimeFailures++ }
            $records.Add($runtimeRow)
        }
    }
}

$failed = @($records | Where-Object { -not $_.pass })
$verified = ($compileFailures -eq 0 -and $runtimeFailures -eq 0 -and $failed.Count -eq 0)
$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $outPath) -Force | Out-Null
[ordered]@{
    schema = "yukinary.network-http-release-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "Real Stage-0/Stage-3 YKN-generated Windows PE and Linux ELF TCP loopback acceptance for GET, partial reads, POST, chunked, timeout and nonblocking socket paths."
    compiler_matrix = @($compilerMatrix | ForEach-Object { [ordered]@{ stage = $_.stage; compiler = $_.relative } })
    targets = @("windows", "linux-docker-ubuntu-24.04")
    cases = @($cases | ForEach-Object { [ordered]@{ name = $_.name; server = $_.server; client = $_.client } })
    compile_timeout_ms = $CompileTimeoutMs
    runtime_timeout_ms = $RuntimeTimeoutMs
    compile_case_count = @($records | Where-Object { $_.kind -eq "compile" }).Count
    runtime_case_count = @($records | Where-Object { $_.kind -eq "runtime" }).Count
    compile_failures = $compileFailures
    runtime_failures = $runtimeFailures
    failed_cases = $failed.Count
    records = $records
    verified = $verified
} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outPath -Encoding UTF8

Write-Host "Network/HTTP release evidence: $outPath"
Write-Host "Compile=$(@($records | Where-Object { $_.kind -eq 'compile' }).Count) Runtime=$(@($records | Where-Object { $_.kind -eq 'runtime' }).Count) Failed=$($failed.Count) Verified=$verified"
if (-not $verified) {
    $failed | ForEach-Object {
        $errorText = if ($_.PSObject.Properties.Name -contains "error") {
            [string]$_.error
        } elseif ($_.PSObject.Properties.Name -contains "stderr") {
            [string]$_.stderr
        } elseif ($_.PSObject.Properties.Name -contains "stdout") {
            [string]$_.stdout
        } else {
            "no diagnostic field"
        }
        Write-Host ("  FAIL: " + $_.stage + " " + $_.target + " " + $_.case + " " + $errorText) -ForegroundColor Red
    }
    exit 1
}
exit 0
