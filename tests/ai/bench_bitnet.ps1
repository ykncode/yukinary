Write-Host "Derleniyor..."
.\app_full.exe tests\ai\test_krep_bitnet_cuda.ykn | Out-Null
Write-Host "Calistiriliyor..."
$sw = [System.Diagnostics.Stopwatch]::StartNew()
.\out.exe
$sw.Stop()
$elapsedSec = $sw.ElapsedMilliseconds / 1000.0
Write-Host "=========================================="
Write-Host "Total Elapsed Time : $elapsedSec s ($($sw.ElapsedMilliseconds) ms)"
$tps = 80.0 / $elapsedSec
Write-Host "Token / sec        : $tps tok/s"
Write-Host "=========================================="
