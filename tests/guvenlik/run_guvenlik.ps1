param(
    [string]$Compiler = ".\build\bootstrap\stage0.exe",
    [string]$OutputJson = ""
)
$pass = 0
$fail = 0
$script:guvenlikResults = New-Object System.Collections.Generic.List[object]

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projRoot  = Split-Path -Parent (Split-Path -Parent $scriptDir)
if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $Compiler = [System.IO.Path]::GetFullPath($Compiler)
} else {
    $Compiler = [System.IO.Path]::GetFullPath((Join-Path $projRoot $Compiler))
}
$work = Join-Path $projRoot "build\test-work\guvenlik"
New-Item -ItemType Directory -Path $work -Force | Out-Null

if (-not (Test-Path -LiteralPath $Compiler)) {
    Write-Host "  FAIL: Compiler bulunamadi: $Compiler" -ForegroundColor Red
    exit 1
}

function Invoke-GuvenlikTest([string]$src, [string]$exp) {
    $name = Split-Path $src -Leaf
    $tmp = Join-Path $work (([System.IO.Path]::GetFileNameWithoutExtension($src)) + ".exe")
    Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    $cOut = & $Compiler $src $tmp 2>&1
    $cCode = $LASTEXITCODE
    if ($cCode -ne 0 -or !(Test-Path -LiteralPath $tmp)) {
        $script:guvenlikResults.Add([ordered]@{
            file = $name; status = "BUILD_FAIL"; compiler_exit_code = $cCode
            runtime_exit_code = $null; expected = $exp; output = ($cOut -join " ").Trim()
            artifact_exists = Test-Path -LiteralPath $tmp
        })
        Write-Host "  FAIL(Build): $name" -ForegroundColor Red
        return $false
    }
    $rOut = & $tmp 2>&1
    $rCode = $LASTEXITCODE
    Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    $o = ($rOut -join " ").Trim()
    $matched = ($rCode -eq 0 -and $o -match $exp)
    $script:guvenlikResults.Add([ordered]@{
        file = $name; status = if ($matched) { "PASS" } else { "FAIL" }
        compiler_exit_code = $cCode; runtime_exit_code = $rCode; expected = $exp
        output = $o; artifact_exists = $true
    })
    if ($matched) {
        Write-Host "  PASS: $name => $o" -ForegroundColor Green
        return $true
    }
    Write-Host "  FAIL: $name => EXIT=$rCode OUT='$o'" -ForegroundColor Red
    return $false
}

Write-Host "=== FAZ 14 BELLEK GUVENLIGI TEST SUITE ===" -ForegroundColor Cyan

$tests = @(
    @{ File = Join-Path $scriptDir "01_bolge_temel.ykn";          Expected = "FAZ14-BOLGE-PASS" },
    @{ File = Join-Path $scriptDir "02_affine_move.ykn";           Expected = "FAZ14-AFFINE-PASS" },
    @{ File = Join-Path $scriptDir "03_guvenli_sinir_ve_null.ykn"; Expected = "FAZ14-BOUNDS-NULL-PASS" },
    @{ File = Join-Path $scriptDir "08_bounds_check.ykn";         Expected = "FAZ14-BOUNDS-CHECK-PASS" },
    @{ File = Join-Path $scriptDir "09_double_free.ykn";          Expected = "FAZ14-DOUBLE-FREE-PASS" },
    @{ File = Join-Path $scriptDir "10_dangling_pointer.ykn";      Expected = "FAZ14-DANGLING-PASS" },
    @{ File = Join-Path $scriptDir "11_uaf_runtime.ykn";          Expected = "FAZ14-UAF-PASS" },
    @{ File = Join-Path $scriptDir "12_asan_redzone.ykn";         Expected = "FAZ14-ASAN-REDZONE-PASS" }
)

foreach ($t in $tests) {
    $r = Invoke-GuvenlikTest $t.File $t.Expected
    if ($r) { $pass++ } else { $fail++ }
}

Write-Host ""
Write-Host "  Guvenlik: $pass/$($pass+$fail) PASS" -ForegroundColor Cyan

if (-not [string]::IsNullOrWhiteSpace($OutputJson)) {
    $reportPath = $OutputJson
    if (-not [System.IO.Path]::IsPathRooted($reportPath)) {
        $reportPath = Join-Path $projRoot $reportPath
    }
    $reportDir = Split-Path -Parent $reportPath
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
    $report = [ordered]@{}
    $report["schema"] = "yukinary.guvenlik-report.v1"
    $report["generated_at"] = (Get-Date).ToUniversalTime().ToString("o")
    $report["compiler"] = [string]$Compiler
    $report["passed"] = [int]$pass
    $report["failed"] = [int]$fail
    $report["total"] = [int]($pass + $fail)
    $report["results"] = @($script:guvenlikResults.ToArray())
    [System.IO.File]::WriteAllText(
        $reportPath,
        ($report | ConvertTo-Json -Depth 10),
        (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "  Rapor: $reportPath"
}

if ($fail -eq 0) {
    Write-Host "  TUM GUVENLIK TESTLERI GECTI!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "  $fail TEST BASARISIZ!" -ForegroundColor Red
    exit 1
}
