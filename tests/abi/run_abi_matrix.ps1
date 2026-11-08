# tests/abi/run_abi_matrix.ps1
# Windows x86_64 ABI & Memory Layout Regression Matrix

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path "$ScriptDir\..\..").Path
Set-Location $RootDir

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "YUKINARY ABI & MEMORY LAYOUT REGRESSION MATRIX" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$Stage3 = "$RootDir\build\bootstrap\stage3.exe"
$WorkDir = "$RootDir\build\test-work\abi"
if (-not (Test-Path $WorkDir)) {
    New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
}

$TestFiles = @(
    @{ Name = "Pointer & Endianness"; Path = "tests\abi\test_pointer_endianness.ykn"; PassStr = "ABI_POINTER_ENDIANNESS_PASS" },
    @{ Name = "Calling Convention";  Path = "tests\abi\02_abi_calling_convention.ykn"; PassStr = "ABI_CALLING_CONVENTION_PASS" },
    @{ Name = "Struct Alignment";     Path = "tests\abi\03_struct_alignment.ykn"; PassStr = "ABI_STRUCT_ALIGNMENT_PASS" },
    @{ Name = "Enum Discriminant";    Path = "tests\abi\04_enum_discriminant.ykn"; PassStr = "ABI_ENUM_DISCRIMINANT_PASS" },
    @{ Name = "Array Offset";         Path = "tests\abi\05_array_offset.ykn"; PassStr = "ABI_ARRAY_OFFSET_PASS" }
)

$Total = $TestFiles.Count
$Passed = 0

foreach ($t in $TestFiles) {
    $ExeName = "$WorkDir\" + [System.IO.Path]::GetFileNameWithoutExtension($t.Path) + ".exe"
    Write-Host "Compiling $($t.Name)..." -NoNewline
    & $Stage3 $t.Path $ExeName | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host " [FAIL - COMPILE ERROR]" -ForegroundColor Red
        continue
    }

    $ProcOutput = & $ExeName 2>&1
    if ($LASTEXITCODE -eq 0 -and $ProcOutput -match $t.PassStr) {
        Write-Host " [PASS]" -ForegroundColor Green
        $Passed++
    } else {
        Write-Host " [FAIL - RUN ERROR]" -ForegroundColor Red
        Write-Host $ProcOutput
    }
}

Write-Host "------------------------------------------------------------" -ForegroundColor Cyan
if ($Passed -eq $Total) {
    Write-Host "Sonuc: $Passed / $Total test gecti." -ForegroundColor Green
    Write-Host "TUM ABI REGRESSION MATRIX TESTLERI GECTI! [OK]" -ForegroundColor Green
    exit 0
} else {
    Write-Host "Sonuc: $Passed / $Total test gecti." -ForegroundColor Red
    Write-Error "HATA: Bazi ABI testleri basarisiz oldu!"
    exit 1
}
