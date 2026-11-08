<#
.SYNOPSIS
    FIX-2 lexer/parser boundary acceptance contract.
.DESCRIPTION
    Runs deterministic malformed-input and nesting cases through Stage-0 and
    Stage-3. Reject cases must terminate with a diagnostic and no artifact;
    the deep valid case must compile and execute; malformed UTF-8 is currently
    a no-crash/parity case because validation policy belongs to FIX-3.
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
$WORK = Join-Path $ROOT "build\test-work\fix2-boundaries"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null
Remove-Item -LiteralPath (Join-Path $WORK "*") -Force -Recurse -ErrorAction SilentlyContinue

function Resolve-Compiler([string]$Path) {
    $resolved = if ([System.IO.Path]::IsPathRooted($Path)) {
        [System.IO.Path]::GetFullPath($Path)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $ROOT $Path))
    }
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "Compiler not found: $resolved"
    }
    return $resolved
}

function Quote-Argument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-ProcessWithTimeout {
    param(
        [string]$File,
        [string[]]$Arguments = @(),
        [int]$Ms = $TimeoutMs
    )

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
    try {
        [void]$process.Start()
    } catch {
        return [PSCustomObject]@{
            ExitCode = -1; Stdout = ""; Stderr = $_.Exception.Message; TimedOut = $false
        }
    }

    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = $false
    if (-not $process.WaitForExit($Ms)) {
        $timedOut = $true
        try { $process.Kill() } catch {}
    }
    $process.WaitForExit()
    try {
        [System.Threading.Tasks.Task]::WaitAll(@($stdoutTask, $stderrTask), 2000) | Out-Null
    } catch {}

    return [PSCustomObject]@{
        ExitCode = if ($timedOut) { -99 } else { $process.ExitCode }
        Stdout = if ($stdoutTask.IsCompleted) { $stdoutTask.Result } else { "" }
        Stderr = if ($stderrTask.IsCompleted) { $stderrTask.Result } else { "" }
        TimedOut = $timedOut
    }
}

function Write-Bytes([string]$Path, [byte[]]$Bytes) {
    [System.IO.File]::WriteAllBytes($Path, $Bytes)
}

function Utf8([string]$Text) {
    return [System.Text.Encoding]::UTF8.GetBytes($Text)
}

$prefix = Utf8 "isv ana() -> tamsayi {`n    # malformed utf8: "
$suffix = Utf8 "`n    dondu 0;`n}`n"
$invalidUtf8 = New-Object byte[] ($prefix.Length + 2 + $suffix.Length)
[Array]::Copy($prefix, 0, $invalidUtf8, 0, $prefix.Length)
$invalidUtf8[$prefix.Length] = 0xC3
$invalidUtf8[$prefix.Length + 1] = 0x28
[Array]::Copy($suffix, 0, $invalidUtf8, $prefix.Length + 2, $suffix.Length)

$nested = ('(' * 16) -join ""
$closing = (')' * 16) -join ""
$deepSource = "isv ana() -> tamsayi {`n    degisken x: tamsayi = ${nested}1${closing};`n    eger x == 1 ise { ekrana_yaz(`"FIX2-DEEP-PASS\n`".ptr); }`n    dondu 0;`n}`n"

$cases = @(
    [PSCustomObject]@{
        Name = "unterminated-string"; Mode = "reject"
        Bytes = Utf8 @'
isv ana() -> tamsayi {
    ekrana_yaz("unterminated);
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "missing-brace"; Mode = "reject"
        Bytes = Utf8 @'
isv ana() -> tamsayi {
    degisken x: tamsayi = 1;
    dondu x;
'@
    }
    [PSCustomObject]@{
        Name = "malformed-generic"; Mode = "reject"
        Bytes = Utf8 @'
isv kimlik<T>(x: T) -> T {
    dondu x;
}

isv ana() -> tamsayi {
    degisken x: tamsayi = kimlik<tamsayi(1);
    dondu x;
}
'@
    }
    [PSCustomObject]@{
        Name = "malformed-utf8-no-crash"; Mode = "parity"
        Bytes = $invalidUtf8
    }
    [PSCustomObject]@{
        Name = "deep-expression"; Mode = "run"
        Bytes = Utf8 $deepSource
    }
)

$compilers = @(
    [PSCustomObject]@{ Name = "Stage-0"; Path = Resolve-Compiler $CompilerA }
    [PSCustomObject]@{ Name = "Stage-3"; Path = Resolve-Compiler $CompilerB }
)

$passed = 0
$failed = 0

foreach ($case in $cases) {
    $caseResults = [System.Collections.Generic.List[object]]::new()
    foreach ($compiler in $compilers) {
        $sourcePath = Join-Path $WORK ($case.Name + ".ykn")
        $outputPath = Join-Path $WORK ($case.Name + "-" + $compiler.Name + ".exe")
        Write-Bytes $sourcePath $case.Bytes
        Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue

        $compile = Invoke-ProcessWithTimeout -File $compiler.Path -Arguments @($sourcePath, $outputPath)
        $artifact = Test-Path -LiteralPath $outputPath -PathType Leaf
        $combined = [string]($compile.Stdout + $compile.Stderr)
        $ok = $false
        $actual = "reject"

        if ($case.Mode -eq "reject") {
            $ok = (-not $compile.TimedOut -and $compile.ExitCode -ne 0 -and
                -not $artifact -and $combined -match "HATA")
        } elseif ($case.Mode -eq "parity") {
            $ok = (-not $compile.TimedOut -and
                (($compile.ExitCode -eq 0) -eq $artifact))
            $actual = if ($compile.ExitCode -eq 0 -and $artifact) { "accept" } else { "reject" }
        } elseif ($case.Mode -eq "run") {
            $actual = "compile-fail"
            if (-not $compile.TimedOut -and $compile.ExitCode -eq 0 -and $artifact) {
                $run = Invoke-ProcessWithTimeout -File $outputPath -Arguments @()
                $actual = "run-fail"
                $ok = (-not $run.TimedOut -and $run.ExitCode -eq 0 -and
                    ([string]$run.Stdout).Contains("FIX2-DEEP-PASS"))
                if ($ok) { $actual = "run" }
            }
        }

        [void]$caseResults.Add([PSCustomObject]@{
            Compiler = $compiler.Name; Passed = $ok; ExitCode = $compile.ExitCode
            Artifact = $artifact; TimedOut = $compile.TimedOut; Actual = $actual
        })
    }

    $casePass = $true
    foreach ($result in $caseResults) {
        if (-not $result.Passed) { $casePass = $false }
    }
    if ($case.Mode -eq "parity" -and $caseResults.Count -eq 2) {
        if ($caseResults[0].Actual -ne $caseResults[1].Actual) { $casePass = $false }
    }

    if ($casePass) {
        $passed++
        Write-Host ("[PASS] {0}" -f $case.Name) -ForegroundColor Green
    } else {
        $failed++
        Write-Host ("[FAIL] {0}" -f $case.Name) -ForegroundColor Red
        foreach ($result in $caseResults) {
            Write-Host ("  {0}: exit={1}, artifact={2}, timeout={3}, actual={4}" -f
                $result.Compiler, $result.ExitCode, $result.Artifact, $result.TimedOut, $result.Actual)
        }
    }
}

Write-Host ("Frontend boundary contracts: {0} PASS / {1} FAIL / {2} total" -f $passed, $failed, ($passed + $failed))
if ($failed -eq 0) {
    Write-Host "FIX-2 FRONTEND BOUNDARY CONTRACTS PASS" -ForegroundColor Green
    exit 0
}
Write-Host "FIX-2 FRONTEND BOUNDARY CONTRACTS FAIL" -ForegroundColor Red
exit 1
