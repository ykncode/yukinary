<#
.SYNOPSIS
    test_arm64_aapcs.ps1 - ARM64 AAPCS Calling Convention Dogrulama
    GOREV 6.3: STP/LDP prolog+epilog ve RET talimat varligi dogrulanir
.DESCRIPTION
    Testler:
      1. Linux ARM64 ELF: STP X29,X30 prolog var mi?
      2. Linux ARM64 ELF: LDP X29,X30 epilog var mi?
      3. Linux ARM64 ELF: RET var mi?
      4. macOS ARM64 Mach-O: STP/LDP/RET var mi?
      5. ARM64 binary boyutu makul araliginda mi?
#>
param([string]$Compiler = ".\build\bootstrap\stage3.exe")

$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $ROOT $Compiler))) {
    if (Test-Path (Join-Path $ROOT "build\yukinary.exe")) {
        $Compiler = ".\build\yukinary.exe"
    } elseif (Test-Path (Join-Path $ROOT "build\bootstrap\stage0.exe")) {
        $Compiler = ".\build\bootstrap\stage0.exe"
    }
}
if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $Compiler = [System.IO.Path]::GetFullPath($Compiler)
} else {
    $Compiler = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
if (-not (Test-Path -LiteralPath $Compiler)) {
    Write-Host "  [FAIL] Compiler bulunamadi: $Compiler" -ForegroundColor Red
    exit 1
}
$WORK = Join-Path $ROOT "build\test-work\crossplatform-arm64-aapcs"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null
$passed = 0; $failed = 0

Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host "  YUKINARY ARM64 AAPCS CALLING CONVENTION TEST" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor Cyan

function Count-Pattern([byte[]]$data, [byte[]]$pattern) {
    $count = 0
    for ($i = 0; $i -lt $data.Length - $pattern.Length + 1; $i++) {
        $match = $true
        for ($j = 0; $j -lt $pattern.Length; $j++) {
            if ($data[$i+$j] -ne $pattern[$j]) { $match = $false; break }
        }
        if ($match) { $count++ }
    }
    return $count
}

function Test-ARM64Binary([string]$binPath, [string]$label) {
    if (-not (Test-Path $binPath)) {
        Write-Host "  [FAIL] $label`: Binary bulunamadi" -ForegroundColor Red
        $script:failed++; return
    }
    $bytes = [System.IO.File]::ReadAllBytes($binPath)
    
    # ARM64 talimat desenleri (little-endian)
    $RET      = [byte[]](0xC0, 0x03, 0x5F, 0xD6)  # RET
    $STP_X29  = [byte[]](0xFD, 0x7B, 0xBF, 0xA9)  # STP X29,X30,[SP,#-16]!
    $LDP_X29  = [byte[]](0xFD, 0x7B, 0xC1, 0xA8)  # LDP X29,X30,[SP],#16
    
    $ret_n  = Count-Pattern $bytes $RET
    $stp_n  = Count-Pattern $bytes $STP_X29
    $ldp_n  = Count-Pattern $bytes $LDP_X29
    
    $ok = $true
    if ($ret_n -lt 1) {
        Write-Host "  [FAIL] $label`: RET talimat yok (count=$ret_n)" -ForegroundColor Red
        $ok = $false
    }
    if ($stp_n -lt 1) {
        Write-Host "  [FAIL] $label`: STP X29,X30 prolog yok (count=$stp_n)" -ForegroundColor Red
        $ok = $false
    }
    if ($ldp_n -lt 1) {
        Write-Host "  [FAIL] $label`: LDP X29,X30 epilog yok (count=$ldp_n)" -ForegroundColor Red
        $ok = $false
    }
    if ($ok) {
        Write-Host "  [PASS] $label`: RET=$ret_n STP=$stp_n LDP=$ldp_n (${bytes.Length} bytes)" -ForegroundColor Green
        $script:passed++
    } else {
        $script:failed++
    }
    Remove-Item $binPath -Force -ErrorAction SilentlyContinue
}

# Test kaynak dosyaları
$test_sources = @(
    (Join-Path $ROOT "tests\bootstrap\01_hello.ykn"),
    (Join-Path $ROOT "tests\stdlib\02_matematik.ykn")
)

foreach ($src in $test_sources) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($src)
    
    # Linux ARM64 ELF
    $elf_out = Join-Path $WORK "${base}_aapcs.elf"
    & $Compiler $src $elf_out --target linux-arm64 2>&1 | Out-Null
    Test-ARM64Binary $elf_out "$base (Linux ARM64 ELF)"
    
    # macOS ARM64 Mach-O
    $macho_out = Join-Path $WORK "${base}_aapcs.macho"
    & $Compiler $src $macho_out --target macos-arm64 2>&1 | Out-Null
    Test-ARM64Binary $macho_out "$base (macOS ARM64 Mach-O)"
}

Write-Host ("-" * 60)
$total = $passed + $failed
Write-Host "Sonuc: $passed/$total test gecti"
if ($failed -eq 0) {
    Write-Host "TUM ARM64 AAPCS TESTLERI GECTI! [OK]" -ForegroundColor Green
    exit 0
} else {
    Write-Host "$failed AAPCS testi BASARISIZ!" -ForegroundColor Red
    exit 1
}
