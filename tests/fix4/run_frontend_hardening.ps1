<##
.SYNOPSIS
    FIX-4 lexer/parser hardening acceptance gate.
.DESCRIPTION
    Verifies that malformed source terminates with a structured diagnostic,
    that multiple parser errors are reported, and that block comments and
    escaped strings remain usable. Every case runs on Stage-0 and Stage-3.
#>
[CmdletBinding()]
param(
    [string]$CompilerA = ".\build\bootstrap\stage1.exe",
    [string]$CompilerB = ".\build\bootstrap\stage3.exe",
    [int]$TimeoutMs = 10000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$WORK = Join-Path $ROOT "build\test-work\fix4-hardening"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null
Remove-Item -LiteralPath (Join-Path $WORK "*") -Force -Recurse -ErrorAction SilentlyContinue

function Resolve-Compiler([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { $resolved = [System.IO.Path]::GetFullPath($Path) }
    else { $resolved = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Path)) }
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) { throw "Compiler not found: $resolved" }
    return $resolved
}

function Quote-Argument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

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
    try { [void]$process.Start() }
    catch { return [PSCustomObject]@{ ExitCode = -1; Output = $_.Exception.Message; TimedOut = $false } }
    $outTask = $process.StandardOutput.ReadToEndAsync()
    $errTask = $process.StandardError.ReadToEndAsync()
    $timedOut = $false
    if (-not $process.WaitForExit($Ms)) {
        $timedOut = $true
        try { $process.Kill() } catch {}
    }
    $process.WaitForExit()
    try { [System.Threading.Tasks.Task]::WaitAll(@($outTask, $errTask), 2000) | Out-Null } catch {}
    $stdout = if ($outTask.IsCompleted) { $outTask.Result } else { "" }
    $stderr = if ($errTask.IsCompleted) { $errTask.Result } else { "" }
    $exitCode = if ($timedOut) { -99 } else { $process.ExitCode }
    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = $stdout + $stderr
        TimedOut = $timedOut
    }
}

$cases = @(
    [PSCustomObject]@{
        Name = "invalid-character"; Mode = "reject"; Pattern = "ERR_INVALID_CHARACTER"
        Source = @'
 isv ana() -> tamsayi {
    degisken x: tamsayi = @;
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "unterminated-block-comment"; Mode = "reject"; Pattern = "ERR_UNTERMINATED_COMMENT"
        Source = @'
 isv ana() -> tamsayi {
    /* yorum kapanmadi
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "integer-overflow"; Mode = "reject"; Pattern = "ERR_INTEGER_OVERFLOW"
        Source = @'
 isv ana() -> tamsayi {
    degisken x: tamsayi = 92233720368547758070;
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "malformed-number"; Mode = "reject"; Pattern = "ERR_MALFORMED_NUMBER"
        Source = @'
 isv ana() -> tamsayi {
    degisken x: tamsayi = 0x;
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "multiple-parse-errors"; Mode = "multi"; Pattern = "PARSE_SYNTAX_ERROR"
        Source = @'
 isv ana() -> tamsayi {
    degisken x: tamsayi = ;
    degisken y: tamsayi = ;
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "generic-closer-recovery"; Mode = "reject"; Pattern = "PARSE_SYNTAX_ERROR"
        Source = @'
 isv kimlik<T, > (x: T) -> T {
    dondu x;
}
 isv ana() -> tamsayi {
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "struct-field-recovery"; Mode = "reject"; Pattern = "PARSE_SYNTAX_ERROR"
        Source = @'
 yapi Kutu {
    : tamsayi;
    deger: tamsayi;
}
 isv ana() -> tamsayi {
    dondu 0;
}
'@
    }
    [PSCustomObject]@{
        Name = "block-comment-and-escape"; Mode = "run"; Pattern = "FIX4-BLOCK-PASS"
        Source = @'
 isv ana() -> tamsayi {
    /* cok satirli
       yorum */
    ekrana_yaz("FIX4-BLOCK-PASS\n".ptr);
    dondu 0;
}
'@
    }
)

$compilers = @(
    [PSCustomObject]@{ Name = "Stage-0"; Path = Resolve-Compiler $CompilerA }
    [PSCustomObject]@{ Name = "Stage-3"; Path = Resolve-Compiler $CompilerB }
)

$passed = 0
$failed = 0
foreach ($case in $cases) {
    $casePass = $true
    foreach ($compiler in $compilers) {
        $sourcePath = Join-Path $WORK ($case.Name + ".ykn")
        $outputPath = Join-Path $WORK ($case.Name + "-" + $compiler.Name + ".exe")
        [System.IO.File]::WriteAllText($sourcePath, $case.Source, (New-Object System.Text.UTF8Encoding($false)))
        Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
        $compile = Invoke-ProcessWithTimeout -File $compiler.Path -Arguments @($sourcePath, $outputPath)
        $artifact = Test-Path -LiteralPath $outputPath -PathType Leaf
        $ok = $false
        if ($case.Mode -eq "reject") {
            $ok = (-not $compile.TimedOut -and $compile.ExitCode -ne 0 -and -not $artifact -and
                $compile.Output -match $case.Pattern)
        } elseif ($case.Mode -eq "multi") {
            $matches = [regex]::Matches($compile.Output, $case.Pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
            $ok = (-not $compile.TimedOut -and $compile.ExitCode -ne 0 -and -not $artifact -and $matches -ge 2)
        } else {
            $run = $null
            if (-not $compile.TimedOut -and $compile.ExitCode -eq 0 -and $artifact) {
                $run = Invoke-ProcessWithTimeout -File $outputPath -Arguments @()
            }
            $ok = ($null -ne $run -and -not $run.TimedOut -and $run.ExitCode -eq 0 -and
                $run.Output.Contains($case.Pattern))
        }
        if (-not $ok) {
            $casePass = $false
            Write-Host ("  {0}: FAIL exit={1}, artifact={2}, timeout={3}" -f
                $compiler.Name, $compile.ExitCode, $artifact, $compile.TimedOut) -ForegroundColor Red
        }
    }
    if ($casePass) {
        $passed++
        Write-Host ("[PASS] {0}" -f $case.Name) -ForegroundColor Green
    } else {
        $failed++
        Write-Host ("[FAIL] {0}" -f $case.Name) -ForegroundColor Red
    }
}

Write-Host ("FIX-4 frontend hardening: {0} PASS / {1} FAIL / {2} total" -f $passed, $failed, ($passed + $failed))
if ($failed -eq 0) {
    Write-Host "FIX-4 FRONTEND HARDENING PASS" -ForegroundColor Green
    exit 0
}
Write-Host "FIX-4 FRONTEND HARDENING FAIL" -ForegroundColor Red
exit 1
