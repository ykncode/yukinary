$ErrorActionPreference = "Stop"

function Measure-ProcessExecution {
    param([string]$FilePath, [string[]]$Arguments = @())
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    if (Test-Path $FilePath) {
        $psi.FileName = (Resolve-Path $FilePath).Path
    } else {
        $psi.FileName = (Get-Command $FilePath).Source
    }
    $psi.Arguments = $Arguments -join " "
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $p = [System.Diagnostics.Process]::Start($psi)
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    $sw.Stop()

    return [PSCustomObject]@{
        Name = (Split-Path $FilePath -Leaf)
        ExitCode = $p.ExitCode
        ElapsedMs = $sw.ElapsedMilliseconds
        Output = $stdout.Trim()
    }
}

Write-Host "=== ENDUSTRIYEL SISTEMLER & BELLEK AGIR YUK BENCHMARK ===" -ForegroundColor Cyan
Write-Host "Islem: 100,000 Dugum Tahsisi & Arama (50k Search) & Range Sum (100k Records)`n" -ForegroundColor DarkGray

# 1. C (GCC -O3)
$rC = Measure-ProcessExecution "benchmarks\industrial\systems_c.exe"
Write-Host "1. C (GCC -O3)        : $($rC.ElapsedMs) ms" -ForegroundColor Green
Write-Host "   $($rC.Output)" -ForegroundColor DarkGray

# 2. Rust (opt-level=3)
$rRust = Measure-ProcessExecution "benchmarks\industrial\systems_rust.exe"
Write-Host "2. Rust (opt-level=3) : $($rRust.ElapsedMs) ms" -ForegroundColor Green
Write-Host "   $($rRust.Output)" -ForegroundColor DarkGray

# 3. Yukinary Native
$rYkn = Measure-ProcessExecution "benchmarks\industrial\systems_ykn.exe"
Write-Host "3. Yukinary (Native)  : $($rYkn.ElapsedMs) ms" -ForegroundColor Yellow
Write-Host "   $($rYkn.Output)" -ForegroundColor DarkGray

# 4. Python 3.13
$rPy = Measure-ProcessExecution "python.exe" @("benchmarks\industrial\systems_workload.py")
Write-Host "4. Python (3.13)      : $($rPy.ElapsedMs) ms" -ForegroundColor Magenta
Write-Host "   $($rPy.Output)" -ForegroundColor DarkGray

