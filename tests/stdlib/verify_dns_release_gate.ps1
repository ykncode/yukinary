[CmdletBinding()]
param(
    [string]$CompilerStage0 = "build/bootstrap/stage0.exe",
    [string]$CompilerStage3 = "build/bootstrap/stage3.exe",
    [int]$CompileTimeoutMs = 120000,
    [int]$RuntimeTimeoutMs = 15000,
    [string]$OutputJson = "build/reports/dns-release-native.json"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Work = Join-Path $Root "build\test-work\dns-release"
New-Item -ItemType Directory -Path $Work -Force | Out-Null

function Root-Path([string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative)) { return [IO.Path]::GetFullPath($Relative) }
    return [IO.Path]::GetFullPath((Join-Path $Root ($Relative -replace "/", "\")))
}
function Quote-Argument([string]$Value) { return '"' + $Value.Replace('"', '\"') + '"' }
function Invoke-CapturedProcess {
    param([string]$FilePath,[string[]]$Arguments,[int]$TimeoutMs,[string]$WorkingDirectory=$Root)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join " ")
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    if (-not $p.Start()) { throw "Process could not start: $FilePath" }
    $outTask = $p.StandardOutput.ReadToEndAsync()
    $errTask = $p.StandardError.ReadToEndAsync()
    $timedOut = -not $p.WaitForExit($TimeoutMs)
    $killed = $false
    if ($timedOut) { try { $p.Kill() ; $killed = $true } catch { }; $p.WaitForExit() }
    [ordered]@{
        exit_code = if ($timedOut) { $null } else { $p.ExitCode }
        timed_out = $timedOut
        killed = $killed
        stdout = $outTask.Result
        stderr = $errTask.Result
    }
}

$records = New-Object System.Collections.Generic.List[object]
$stages = @(
    [ordered]@{ name = "Stage-0"; compiler = (Root-Path $CompilerStage0) },
    [ordered]@{ name = "Stage-3"; compiler = (Root-Path $CompilerStage3) }
)
$positiveSource = "tests/stdlib/dns_generated_probe.ykn"
$negativeSource = "tests/stdlib/dns_generated_negative_probe.ykn"
$linuxSource = "tests/stdlib/dns_generated_linux_probe.ykn"

foreach ($stage in $stages) {
    foreach ($item in @(
        [ordered]@{ name = "dns_positive"; source = $positiveSource; target = "windows"; extension = ".exe"; args = @() },
        [ordered]@{ name = "dns_negative"; source = $negativeSource; target = "windows"; extension = ".exe"; args = @() },
        [ordered]@{ name = "dns_linux_resolver"; source = $linuxSource; target = "linux"; extension = ".elf"; args = @("--target", "linux") }
    )) {
        $artifact = Join-Path $Work (("{0}-{1}{2}" -f $stage.name.ToLowerInvariant(), $item.name, $item.extension))
        Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue
        $compileArgs = @((Root-Path $item.source), $artifact) + @($item.args)
        $compile = Invoke-CapturedProcess -FilePath $stage.compiler -Arguments $compileArgs -TimeoutMs $CompileTimeoutMs
        $exists = Test-Path -LiteralPath $artifact -PathType Leaf
        $pass = ($compile.exit_code -eq 0 -and -not $compile.timed_out -and $exists)
        $records.Add([ordered]@{ kind="compile"; stage=$stage.name; case=$item.name; target=$item.target; source=$item.source; artifact=$artifact.Substring($Root.Length+1).Replace("\","/"); exit_code=$compile.exit_code; timed_out=$compile.timed_out; artifact_exists=$exists; pass=$pass; stderr=$compile.stderr; stdout=$compile.stdout })
    }
}

foreach ($stage in $stages) {
    $positive = Join-Path $Work (("{0}-dns_positive.exe" -f $stage.name.ToLowerInvariant()))
    $negative = Join-Path $Work (("{0}-dns_negative.exe" -f $stage.name.ToLowerInvariant()))
    $compileRows = @($records | Where-Object { $_.kind -eq "compile" -and $_.stage -eq $stage.name -and $_.target -eq "windows" })
    if (@($compileRows | Where-Object { -not $_.pass }).Count -gt 0) {
        $records.Add([ordered]@{ kind="runtime"; stage=$stage.name; target="windows"; case="dns_positive_negative"; pass=$false; skipped=$true; error="compile failed" })
        continue
    }
    $runnerArgs = @("-NoProfile","-ExecutionPolicy","Bypass","-File",(Root-Path "tests/stdlib/run_dns_native.ps1"),"-Client",$positive,"-NegativeClient",$negative)
    $runner = Invoke-CapturedProcess -FilePath "powershell.exe" -Arguments $runnerArgs -TimeoutMs $RuntimeTimeoutMs
    $records.Add([ordered]@{ kind="runtime"; stage=$stage.name; target="windows"; case="dns_positive_negative"; pass=($runner.exit_code -eq 0 -and -not $runner.timed_out); exit_code=$runner.exit_code; timed_out=$runner.timed_out; stdout=$runner.stdout; stderr=$runner.stderr; error=$runner.stderr })
}

$resolverProbeArgs = @("run","--rm","ubuntu:24.04","bash","-lc","awk '/^nameserver/{print `$2; exit}' /etc/resolv.conf")
$resolverProbe = Invoke-CapturedProcess -FilePath "docker" -Arguments $resolverProbeArgs -TimeoutMs 30000
$resolver = ($resolverProbe.stdout.Trim() -split "\r?\n")[0]
if ($resolverProbe.exit_code -ne 0 -or $resolver -notmatch '^\d+\.\d+\.\d+\.\d+$') {
    throw "Docker resolver discovery failed: $($resolverProbe.stdout) $($resolverProbe.stderr)"
}

foreach ($stage in $stages) {
    $artifact = Join-Path $Work (("{0}-dns_linux_resolver.elf" -f $stage.name.ToLowerInvariant()))
    $compileRow = @($records | Where-Object { $_.kind -eq "compile" -and $_.stage -eq $stage.name -and $_.case -eq "dns_linux_resolver" })[0]
    if (-not $compileRow.pass) {
        $records.Add([ordered]@{ kind="runtime"; stage=$stage.name; target="linux-docker-ubuntu-24.04"; case="dns_linux_resolver"; resolver=$resolver; pass=$false; skipped=$true; error="compile failed" })
        continue
    }
    $containerArtifact = "/work/" + $artifact.Substring($Root.Length+1).Replace("\","/")
    $command = "chmod +x '$containerArtifact'; '$containerArtifact' '$resolver'; rc=`$?; exit `$rc"
    $dockerArgs = @("run","--rm","-v",((Resolve-Path $Root).Path.Replace("\","/")+":/work"),"-w","/work","ubuntu:24.04","bash","-lc",$command)
    $run = Invoke-CapturedProcess -FilePath "docker" -Arguments $dockerArgs -TimeoutMs $RuntimeTimeoutMs
    $records.Add([ordered]@{ kind="runtime"; stage=$stage.name; target="linux-docker-ubuntu-24.04"; case="dns_linux_resolver"; resolver=$resolver; pass=($run.exit_code -eq 0 -and -not $run.timed_out); exit_code=$run.exit_code; timed_out=$run.timed_out; stdout=$run.stdout; stderr=$run.stderr; error=$run.stderr })
}

$failed = @($records | Where-Object { -not $_.pass })
$verified = ($failed.Count -eq 0)
$outPath = Root-Path $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $outPath) -Force | Out-Null
[ordered]@{
    schema = "yukinary.dns-release-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    scope = "Real YKN-generated Windows UDP wire acceptance and Docker Linux ELF DNS resolution using dynamically discovered /etc/resolv.conf nameserver."
    resolver = $resolver
    records = $records
    failed_cases = $failed.Count
    verified = $verified
} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outPath -Encoding UTF8
Write-Host "DNS release evidence: $outPath"
Write-Host "Records=$($records.Count) Failed=$($failed.Count) Resolver=$resolver Verified=$verified"
if (-not $verified) { $failed | ForEach-Object { Write-Host ("  FAIL: " + $_.stage + " " + $_.target + " " + $_.case + " " + $_.error) -ForegroundColor Red }; exit 1 }
exit 0
