<#
.SYNOPSIS
    test_arm64_elf.ps1 - Yukinary ARM64 (Linux ELF64 + macOS Mach-O) binary dogrulama suite (Saf PowerShell)
.DESCRIPTION
    Python bagimliligi olmadan ARM64 binary dogrulamasi.
    Checks:
      1. Linux ARM64 ELF64 Header (EM_AARCH64=0xB7)
      2. Linux ARM64 _start thunk (LDR X0, ADD X1, BL ana, MOVZ X8 #93, SVC #0)
      3. macOS ARM64 Mach-O Header (CPU_TYPE_ARM64=0x0100000C)
      4. macOS ARM64 _start thunk (BL ana, MOVZ X16 #1, SVC #0x80)
#>
param(
    [string]$Compiler = ".\build\bootstrap\stage0.exe",
    [string]$WorkRoot = ""
)

$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $Compiler = [System.IO.Path]::GetFullPath($Compiler)
} else {
    $Compiler = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
if (-not (Test-Path -LiteralPath $Compiler)) {
    Write-Host "  [FAIL] Compiler bulunamadi: $Compiler" -ForegroundColor Red
    exit 1
}
$WORK = if ([string]::IsNullOrWhiteSpace($WorkRoot)) {
    Join-Path $ROOT "build\test-work\crossplatform-arm64"
} else {
    Join-Path $WorkRoot "crossplatform-arm64"
}
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

$TestCases = @(
    (Join-Path $ROOT "tests\bootstrap\01_hello.ykn"),
    (Join-Path $ROOT "tests\bootstrap\02_arithmetic.ykn")
)

$passed = 0
$total  = $TestCases.Count * 2  # Her dosya icin hem Linux hem macOS ARM64

Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host "  YUKINARY CROSS-PLATFORM ARM64 VALIDATION SUITE" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor Cyan

function Read-U16LE([byte[]]$data, [int]$offset) {
    return [BitConverter]::ToUInt16($data, $offset)
}
function Read-U32LE([byte[]]$data, [int]$offset) {
    return [BitConverter]::ToUInt32($data, $offset)
}
function Read-U64LE([byte[]]$data, [int]$offset) {
    return [BitConverter]::ToUInt64($data, $offset)
}

function Test-LinuxARM64([string]$elfPath) {
    if (-not (Test-Path $elfPath)) {
        Write-Host "  [FAIL] Dosya bulunamadi: $elfPath" -ForegroundColor Red
        return $false
    }
    [byte[]]$data = [System.IO.File]::ReadAllBytes($elfPath)
    $size = $data.Count

    if ($size -lt 120) {
        Write-Host "  [FAIL] Dosya cok kucuk" -ForegroundColor Red
        return $false
    }

    # ELF Magic
    if ($data[0] -ne 0x7F -or $data[1] -ne 0x45 -or $data[2] -ne 0x4C -or $data[3] -ne 0x46) {
        Write-Host "  [FAIL] Gecersiz ELF magic" -ForegroundColor Red
        return $false
    }

    $e_type    = Read-U16LE $data 16
    $e_machine = Read-U16LE $data 18
    $e_entry   = Read-U64LE $data 24

    if ($e_type -ne 2) {
        Write-Host "  [FAIL] ET_EXEC(2) beklendi, alindi: $e_type" -ForegroundColor Red
        return $false
    }
    if ($e_machine -ne 0xB7) {
        Write-Host ("  [FAIL] EM_AARCH64(0xB7) beklendi, alindi: 0x{0:X}" -f $e_machine) -ForegroundColor Red
        return $false
    }

    $entryOffset = [int]($e_entry - 0x400000)
    if ($entryOffset -lt 0 -or ($entryOffset + 20) -gt $size) {
        Write-Host "  [FAIL] Giris noktasi dosya siniri disinda" -ForegroundColor Red
        return $false
    }

    # LDR X0, [SP]: E0 03 40 F9
    if ($data[$entryOffset] -ne 0xE0 -or $data[$entryOffset+1] -ne 0x03 -or $data[$entryOffset+2] -ne 0x40 -or $data[$entryOffset+3] -ne 0xF9) {
        Write-Host "  [FAIL] Linux ARM64 _start thunk prolog uyusmadi (LDR X0)" -ForegroundColor Red
        return $false
    }
    # ADD X1, SP, #8: E1 23 00 91
    if ($data[$entryOffset+4] -ne 0xE1 -or $data[$entryOffset+5] -ne 0x23 -or $data[$entryOffset+6] -ne 0x00 -or $data[$entryOffset+7] -ne 0x91) {
        Write-Host "  [FAIL] Linux ARM64 _start thunk prolog uyusmadi (ADD X1)" -ForegroundColor Red
        return $false
    }
    # MOVZ X8, #93: A8 0B 80 D2
    if ($data[$entryOffset+12] -ne 0xA8 -or $data[$entryOffset+13] -ne 0x0B -or $data[$entryOffset+14] -ne 0x80 -or $data[$entryOffset+15] -ne 0xD2) {
        Write-Host "  [FAIL] Linux ARM64 exit syscall (MOVZ X8,93) uyusmadi" -ForegroundColor Red
        return $false
    }
    # SVC #0: 01 00 00 D4
    if ($data[$entryOffset+16] -ne 0x01 -or $data[$entryOffset+17] -ne 0x00 -or $data[$entryOffset+18] -ne 0x00 -or $data[$entryOffset+19] -ne 0xD4) {
        Write-Host "  [FAIL] Linux ARM64 SVC #0 uyusmadi" -ForegroundColor Red
        return $false
    }

    $name = Split-Path $elfPath -Leaf
    Write-Host ("  [PASS] {0}: Gecerli Linux ARM64 ELF (giris=0x{1:X}, EM_AARCH64=0xB7)" -f $name, $e_entry) -ForegroundColor Green
    return $true
}

function Test-MacOSARM64([string]$machoPath) {
    if (-not (Test-Path $machoPath)) {
        Write-Host "  [FAIL] Dosya bulunamadi: $machoPath" -ForegroundColor Red
        return $false
    }
    [byte[]]$data = [System.IO.File]::ReadAllBytes($machoPath)
    $size = $data.Count

    if ($size -lt 320) {
        Write-Host "  [FAIL] Dosya cok kucuk ($size bayt)" -ForegroundColor Red
        return $false
    }

    # Mach-O 64 magic (CF FA ED FE)
    if ($data[0] -ne 0xCF -or $data[1] -ne 0xFA -or $data[2] -ne 0xED -or $data[3] -ne 0xFE) {
        Write-Host "  [FAIL] Gecersiz Mach-O 64 magic" -ForegroundColor Red
        return $false
    }

    $cputype = Read-U32LE $data 4
    if ($cputype -ne 0x0100000C) {
        Write-Host ("  [FAIL] CPU_TYPE_ARM64(0x0100000C) beklendi, alindi: 0x{0:X}" -f $cputype) -ForegroundColor Red
        return $false
    }

    $ncmds = Read-U32LE $data 16
    $pos = 32
    $entryOffset = -1
    for ($c = 0; $c -lt $ncmds; $c++) {
        if ($pos + 8 -gt $size) { break }
        $cmd = Read-U32LE $data $pos
        $cmdsize = Read-U32LE $data ($pos + 4)
        if ($cmd -eq 0x80000028L) { # LC_MAIN
            $entryOffset = [int](Read-U64LE $data ($pos + 8))
        } elseif ($cmd -eq 0x05) { # LC_UNIXTHREAD
            $entry_pc = Read-U64LE $data ($pos + 16 + 256)
            $entryOffset = [int]($entry_pc - 0x100000000)
        }
        $pos += $cmdsize
    }

    if ($entryOffset -lt 0 -or ($entryOffset + 20) -gt $size) {
        Write-Host "  [FAIL] Giris noktasi dosya siniri disinda: $entryOffset" -ForegroundColor Red
        return $false
    }

    # Modern thunk check (CMP X0, #0 = 1F 00 00 F1) or legacy (LDR X0, [SP] = E0 03 40 F9)
    $isModern = ($data[$entryOffset] -eq 0x1F -and $data[$entryOffset+1] -eq 0x00 -and $data[$entryOffset+2] -eq 0x00 -and $data[$entryOffset+3] -eq 0xF1)
    $isLegacy = ($data[$entryOffset] -eq 0xE0 -and $data[$entryOffset+1] -eq 0x03 -and $data[$entryOffset+2] -eq 0x40 -and $data[$entryOffset+3] -eq 0xF9)

    if (-not ($isModern -or $isLegacy)) {
        Write-Host "  [FAIL] macOS ARM64 _start thunk prolog uyusmadi" -ForegroundColor Red
        return $false
    }

    $exitOffset = if ($isModern) { $entryOffset + 20 } else { $entryOffset + 12 }
    # MOVZ X16, #1: 30 00 80 D2
    if ($data[$exitOffset] -ne 0x30 -or $data[$exitOffset+1] -ne 0x00 -or $data[$exitOffset+2] -ne 0x80 -or $data[$exitOffset+3] -ne 0xD2) {
        Write-Host "  [FAIL] macOS ARM64 exit syscall (MOVZ X16,1) uyusmadi" -ForegroundColor Red
        return $false
    }
    # SVC #0x80: 01 10 00 D4
    if ($data[$exitOffset+4] -ne 0x01 -or $data[$exitOffset+5] -ne 0x10 -or $data[$exitOffset+6] -ne 0x00 -or $data[$exitOffset+7] -ne 0xD4) {
        Write-Host "  [FAIL] macOS ARM64 SVC #0x80 uyusmadi" -ForegroundColor Red
        return $false
    }

    $name = Split-Path $machoPath -Leaf
    Write-Host ("  [PASS] {0}: Gecerli macOS ARM64 Mach-O (giris=0x{1:X}, CPU_TYPE_ARM64=0x0100000C)" -f $name, $entryOffset) -ForegroundColor Green
    return $true
}

foreach ($ykn in $TestCases) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($ykn)

    # Linux ARM64
    $outElf = Join-Path $WORK "${name}_arm64.elf"
    Remove-Item $outElf -Force -ErrorAction SilentlyContinue
    & $Compiler "--target" "linux-arm64" $ykn $outElf 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0 -and (Test-Path $outElf)) {
        if (Test-LinuxARM64 $outElf) { $passed++ }
    } else {
        Write-Host "  [FAIL] Linux ARM64 derleme hatasi: $ykn" -ForegroundColor Red
    }
    Remove-Item $outElf -Force -ErrorAction SilentlyContinue

    # macOS ARM64
    $outMacho = Join-Path $WORK "${name}_arm64.macho"
    Remove-Item $outMacho -Force -ErrorAction SilentlyContinue
    & $Compiler "--target" "macos-arm64" $ykn $outMacho 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0 -and (Test-Path $outMacho)) {
        if (Test-MacOSARM64 $outMacho) { $passed++ }
    } else {
        Write-Host "  [FAIL] macOS ARM64 derleme hatasi: $ykn" -ForegroundColor Red
    }
    Remove-Item $outMacho -Force -ErrorAction SilentlyContinue
}

Write-Host ("-" * 60)
Write-Host "Sonuc: $passed/$total test gecti"
if ($passed -eq $total) {
    Write-Host "TUM ARM64 CROSS-PLATFORM TESTLERI GECTI! [OK]" -ForegroundColor Green
    exit 0
} else {
    Write-Host "BAZI TESTLER BASARISIZ." -ForegroundColor Red
    exit 1
}
