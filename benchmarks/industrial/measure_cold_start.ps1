$iters = 10

function Get-AvgColdStart([string]$Target, [string[]]$Args = @()) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    for ($i = 0; $i -lt $iters; $i++) {
        $p = Start-Process -FilePath $Target -ArgumentList $Args -NoNewWindow -Wait -PassThru
    }
    $sw.Stop()
    return [math]::Round($sw.ElapsedMilliseconds / $iters, 2)
}

$yknExe = (Resolve-Path "build\test-work\stage1-selfhost\stage1-selfhost.exe").Path
$cExe = (Resolve-Path "benchmarks\industrial\systems_c.exe").Path
$rustExe = (Resolve-Path "benchmarks\industrial\systems_rust.exe").Path
$pyExe = (Get-Command "python.exe").Source

$yknTime = Get-AvgColdStart $yknExe
$cTime = Get-AvgColdStart $cExe
$rustTime = Get-AvgColdStart $rustExe
$pyTime = Get-AvgColdStart $pyExe @("-c", "pass")

Write-Host "Yukinary Cold Start : $yknTime ms"
Write-Host "C Cold Start        : $cTime ms"
Write-Host "Rust Cold Start     : $rustTime ms"
Write-Host "Python Cold Start   : $pyTime ms"

