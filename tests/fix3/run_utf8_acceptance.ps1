<#
FIX-3 UTF-8 acceptance contract.
Valid sources must compile/run identically on Stage-0 and Stage-3.
Malformed UTF-8 must be rejected before parsing with E007 and no artifact.
#>
[CmdletBinding()]
param(
    [string]$CompilerA = ".\build\bootstrap\stage0.exe",
    [string]$CompilerB = ".\build\bootstrap\stage3.exe",
    [int]$TimeoutMs = 10000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$WORK = Join-Path $ROOT "build\test-work\fix3-utf8"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null
Remove-Item -LiteralPath (Join-Path $WORK "*") -Force -Recurse -ErrorAction SilentlyContinue

function Resolve-Compiler([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { $resolved = [System.IO.Path]::GetFullPath($Path) }
    else { $resolved = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Path)) }
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) { throw "Compiler not found: $resolved" }
    return $resolved
}

function Quote-Argument([string]$Value) { return '"' + $Value.Replace('"', '\"') + '"' }

function Invoke-ProcessWithTimeout {
    param([string]$File, [string[]]$Arguments = @(), [int]$Ms = $TimeoutMs)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $File
    $psi.Arguments = ($Arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join " "
    $psi.WorkingDirectory = $ROOT
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $psi.StandardOutputEncoding = $utf8
    $psi.StandardErrorEncoding = $utf8
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($Ms)
    if ($timedOut) { try { $process.Kill() } catch {} }
    $process.WaitForExit()
    try { [System.Threading.Tasks.Task]::WaitAll(@($stdoutTask, $stderrTask), 2000) | Out-Null } catch {}
    return [PSCustomObject]@{
        ExitCode = if ($timedOut) { -99 } else { $process.ExitCode }
        Stdout = if ($stdoutTask.IsCompleted) { $stdoutTask.Result } else { "" }
        Stderr = if ($stderrTask.IsCompleted) { $stderrTask.Result } else { "" }
        TimedOut = $timedOut
    }
}

function Invoke-Case($Case, $Compiler) {
    $sourcePath = Join-Path $WORK ($Case.Name + ".ykn")
    $outputPath = Join-Path $WORK ($Case.Name + "-" + $Compiler.Name + ".exe")
    [System.IO.File]::WriteAllBytes($sourcePath, [byte[]]$Case.Bytes)
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
    $compile = Invoke-ProcessWithTimeout -File $Compiler.Path -Arguments @($sourcePath, $outputPath)
    $artifact = Test-Path -LiteralPath $outputPath -PathType Leaf
    $combined = [string]($compile.Stdout + $compile.Stderr)
    $ok = $false
    $actual = "reject"
    if ($Case.Mode -eq "run") {
        if (-not $compile.TimedOut -and $compile.ExitCode -eq 0 -and $artifact) {
            $run = Invoke-ProcessWithTimeout -File $outputPath
            $actual = "run-fail"
            $ok = -not $run.TimedOut -and $run.ExitCode -eq 0 -and ([string]$run.Stdout).Contains($Case.Marker)
            if ($ok) { $actual = "run" }
        } else { $actual = "compile-fail" }
    } else {
        $ok = -not $compile.TimedOut -and $compile.ExitCode -ne 0 -and -not $artifact -and $combined.Contains("E007") -and $combined.Contains("ERR_INVALID_UTF8")
    }
    return [PSCustomObject]@{ Name = $Case.Name; Compiler = $Compiler.Name; Passed = $ok; ExitCode = $compile.ExitCode; Artifact = $artifact; Actual = $actual; Output = $combined }
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$unicodeIdentifier = "sonu" + [char]0x00E7
$unicodeSource = @"
isv ana() -> tamsayi {
    degisken $unicodeIdentifier`: tamsayi = 41;
    ekrana_yaz("FIX3-UNICODE-PASS`n".ptr);
    dondu 0;
}
"@
$japaneseIf = ([char]0x3082).ToString() + ([char]0x3057).ToString()
$japaneseThen = ([char]0x306A).ToString() + ([char]0x3089).ToString()
$japaneseSource = 'isv ana() -> tamsayi {' + "`n" +
    '    degisken a: tamsayi = 1;' + "`n" +
    '    ' + $japaneseIf + ' a == 1 ' + $japaneseThen + ' {' + "`n" +
    '        ekrana_yaz("FIX3-I18N-PASS`n".ptr);' + "`n" +
    '    }' + "`n" +
    '    dondu 0;' + "`n" +
    '}' + "`n"
$bomSource = ([char]0xFEFF).ToString() + "isv ana() -> tamsayi {`n    ekrana_yaz(`"FIX3-BOM-PASS`n`".ptr);`n    dondu 0;`n}`n"
$prefix = $utf8.GetBytes("isv ana() -> tamsayi {`n    # invalid: ")
$suffix = $utf8.GetBytes("`n    dondu 0;`n}`n")

function Bytes-With-Invalid([byte[]]$Invalid) {
    $bytes = New-Object byte[] ($prefix.Length + $Invalid.Length + $suffix.Length)
    [Array]::Copy($prefix, 0, $bytes, 0, $prefix.Length)
    [Array]::Copy($Invalid, 0, $bytes, $prefix.Length, $Invalid.Length)
    [Array]::Copy($suffix, 0, $bytes, $prefix.Length + $Invalid.Length, $suffix.Length)
    return $bytes
}

$cases = @(
    [PSCustomObject]@{ Name = "unicode-identifier"; Mode = "run"; Marker = "FIX3-UNICODE-PASS"; Bytes = $utf8.GetBytes($unicodeSource) },
    [PSCustomObject]@{ Name = "japanese-keyword"; Mode = "run"; Marker = "FIX3-I18N-PASS"; Bytes = $utf8.GetBytes($japaneseSource) },
    [PSCustomObject]@{ Name = "utf8-bom"; Mode = "run"; Marker = "FIX3-BOM-PASS"; Bytes = $utf8.GetBytes($bomSource) },
    [PSCustomObject]@{ Name = "invalid-continuation"; Mode = "reject"; Bytes = (Bytes-With-Invalid ([byte[]](0x80))) },
    [PSCustomObject]@{ Name = "utf8-overlong"; Mode = "reject"; Bytes = (Bytes-With-Invalid ([byte[]](0xC0, 0xAF))) },
    [PSCustomObject]@{ Name = "utf8-surrogate"; Mode = "reject"; Bytes = (Bytes-With-Invalid ([byte[]](0xED, 0xA0, 0x80))) },
    [PSCustomObject]@{ Name = "utf8-out-of-range"; Mode = "reject"; Bytes = (Bytes-With-Invalid ([byte[]](0xF4, 0x90, 0x80, 0x80))) }
)
$compilers = @(
    [PSCustomObject]@{ Name = "Stage-0"; Path = Resolve-Compiler $CompilerA },
    [PSCustomObject]@{ Name = "Stage-3"; Path = Resolve-Compiler $CompilerB }
)
$passed = 0; $failed = 0
foreach ($case in $cases) {
    $results = @($compilers | ForEach-Object { Invoke-Case $case $_ })
    $casePass = $true
    foreach ($result in $results) { if (-not $result.Passed) { $casePass = $false } }
    if ($casePass -and $case.Mode -eq "run") {
        if (@($results | ForEach-Object Actual | Select-Object -Unique).Count -ne 1) { $casePass = $false }
    }
    if ($casePass) { $passed++; Write-Host "[PASS] $($case.Name)" -ForegroundColor Green }
    else {
        $failed++; Write-Host "[FAIL] $($case.Name)" -ForegroundColor Red
        $results | ForEach-Object { Write-Host "  $($_.Compiler): exit=$($_.ExitCode), artifact=$($_.Artifact), actual=$($_.Actual)" }
    }
}
Write-Host "FIX-3 UTF-8 acceptance: $passed PASS / $failed FAIL / $($passed + $failed) total"
if ($failed -ne 0) { exit 1 }
exit 0
