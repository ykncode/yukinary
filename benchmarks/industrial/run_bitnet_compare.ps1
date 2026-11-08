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

    $memMb = 0
    try {
        $memMb = [math]::Round($p.PeakWorkingSet64 / 1MB, 2)
    } catch {}

    return [PSCustomObject]@{
        Name = (Split-Path $FilePath -Leaf)
        ExitCode = $p.ExitCode
        ElapsedMs = $sw.ElapsedMilliseconds
        PeakMemMb = $memMb
        Output = $stdout.Trim()
    }
}

Write-Host "=== ENDUSTRIYEL BITNET AI KERNEL BENCHMARK ===" -ForegroundColor Cyan
Write-Host "Islem Hacmi: 100 Iterasyon x (64 x 512 x 512 x 2) = 3.35 Giga-Operations (GOP)`n" -ForegroundColor DarkGray

# 1. C (GCC -O3)
$rC = Measure-ProcessExecution "benchmarks\industrial\bitnet_c.exe"
Write-Host "1. C (GCC -O3)        : $($rC.ElapsedMs) ms | Peak RSS: $($rC.PeakMemMb) MB" -ForegroundColor Green
Write-Host "   $($rC.Output)" -ForegroundColor DarkGray

# 2. Rust (rustc opt-level=3)
$rRust = Measure-ProcessExecution "benchmarks\industrial\bitnet_rust.exe"
Write-Host "2. Rust (opt-level=3) : $($rRust.ElapsedMs) ms | Peak RSS: $($rRust.PeakMemMb) MB" -ForegroundColor Green
Write-Host "   $($rRust.Output)" -ForegroundColor DarkGray

# 3. Yukinary Native
$rYkn = Measure-ProcessExecution "benchmarks\industrial\bitnet_ykn.exe"
$yknGops = [math]::Round(3.3554432 / ($rYkn.ElapsedMs / 1000.0), 2)
Write-Host "3. Yukinary (Native)  : $($rYkn.ElapsedMs) ms | Throughput: $yknGops GOP/s | Peak RSS: $($rYkn.PeakMemMb) MB" -ForegroundColor Yellow
Write-Host "   $($rYkn.Output)" -ForegroundColor DarkGray

# 4. Python 3.13
$rPy = Measure-ProcessExecution "python.exe" @("benchmarks\industrial\bitnet_bench.py")
Write-Host "4. Python (3.13)      : $($rPy.ElapsedMs) ms | Peak RSS: $($rPy.PeakMemMb) MB" -ForegroundColor Magenta
Write-Host "   $($rPy.Output)" -ForegroundColor DarkGray

$outData = @{
    workload = "BitNet b1.58 Ternary Matrix Multiplication + RMSNorm"
    gops_total = 3.3554432
    results = @($rC, $rRust, $rYkn, $rPy)
}
$outData | ConvertTo-Json -Depth 4 | Set-Content "benchmarks\industrial\bitnet_results.json" -Encoding UTF8
