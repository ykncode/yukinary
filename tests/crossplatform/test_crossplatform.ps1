param(
    [string]$Compiler = ".\build\bootstrap\stage0.exe"
)

$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([System.IO.Path]::IsPathRooted($Compiler)) {
    $Compiler = [System.IO.Path]::GetFullPath($Compiler)
} else {
    $Compiler = [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
if (-not (Test-Path -LiteralPath $Compiler)) {
    Write-Host "[FAIL] Compiler bulunamadi: $Compiler" -ForegroundColor Red
    exit 1
}
$WORK = Join-Path $ROOT "build\test-work\crossplatform-combined"
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

$passed = 0
$total = 12

function Read-BinaryFile($filePath) {
    return [System.IO.File]::ReadAllBytes($filePath)
}

function Validate-Elf64([byte[]]$bytes, $name) {
    if ($bytes.Length -lt 120) {
        Write-Host "[FAIL] ${name}: File too small ($($bytes.Length) bytes)" -ForegroundColor Red
        return $false
    }
    # ELF Magic: 0x7F 'E' 'L' 'F'
    if ($bytes[0] -ne 0x7F -or $bytes[1] -ne 0x45 -or $bytes[2] -ne 0x4C -or $bytes[3] -ne 0x46) {
        Write-Host "[FAIL] ${name}: Invalid ELF Magic" -ForegroundColor Red
        return $false
    }
    # Class: 2 (64-bit), Data: 1 (Little Endian)
    if ($bytes[4] -ne 2 -or $bytes[5] -ne 1) {
        Write-Host "[FAIL] ${name}: Invalid ELF Class or Endianness" -ForegroundColor Red
        return $false
    }
    # e_type = 2 (ET_EXEC) at offset 16
    $e_type = [BitConverter]::ToUInt16($bytes, 16)
    $e_machine = [BitConverter]::ToUInt16($bytes, 18)
    $e_entry = [BitConverter]::ToUInt64($bytes, 24)
    if ($e_type -ne 2) {
        Write-Host "[FAIL] ${name}: Expected ET_EXEC (2), got $e_type" -ForegroundColor Red
        return $false
    }
    if ($e_machine -ne 0x3E) { # EM_X86_64
        Write-Host "[FAIL] ${name}: Expected EM_X86_64 (0x3E), got $e_machine" -ForegroundColor Red
        return $false
    }
    # Syscall count (0x0F 0x05)
    $sysCount = 0
    for ($i = 0; $i -lt $bytes.Length - 1; $i++) {
        if ($bytes[$i] -eq 0x0F -and $bytes[$i+1] -eq 0x05) {
            $sysCount++
        }
    }
    if ($sysCount -eq 0) {
        Write-Host "[FAIL] ${name}: No syscalls (0x0F 0x05) found" -ForegroundColor Red
        return $false
    }
    Write-Host "[PASS] ${name}: Valid ELF64 (size=$($bytes.Length)B, entry=0x$($e_entry.ToString('X')), syscalls=$sysCount)" -ForegroundColor Green
    return $true
}

function Validate-Macho64([byte[]]$bytes, $name) {
    if ($bytes.Length -lt 368) {
        Write-Host "[FAIL] ${name}: File too small ($($bytes.Length) bytes)" -ForegroundColor Red
        return $false
    }
    $magic = [BitConverter]::ToUInt32($bytes, 0)
    $cputype = [BitConverter]::ToUInt32($bytes, 4)
    $filetype = [BitConverter]::ToUInt32($bytes, 12)
    $expected_magic = [BitConverter]::ToUInt32([byte[]](0xCF, 0xFA, 0xED, 0xFE), 0)
    if ($magic -ne $expected_magic) {
        Write-Host "[FAIL] ${name}: Invalid Mach-O magic 0x$($magic.ToString('X'))" -ForegroundColor Red
        return $false
    }
    if ($cputype -ne 0x01000007) { # CPU_TYPE_X86_64
        Write-Host "[FAIL] ${name}: Expected CPU_TYPE_X86_64, got 0x$($cputype.ToString('X'))" -ForegroundColor Red
        return $false
    }
    if ($filetype -ne 2) { # MH_EXECUTE
        Write-Host "[FAIL] ${name}: Expected MH_EXECUTE, got $filetype" -ForegroundColor Red
        return $false
    }
    # Syscalls
    $sysCount = 0
    for ($i = 0; $i -lt $bytes.Length - 1; $i++) {
        if ($bytes[$i] -eq 0x0F -and $bytes[$i+1] -eq 0x05) {
            $sysCount++
        }
    }
    # RIP entry from LC_UNIXTHREAD (at offset 32 + 152 + 16 + 128 = 328)
    $entryRip = [BitConverter]::ToUInt64($bytes, 328)
    Write-Host "[PASS] ${name}: Valid Mach-O 64 (size=$($bytes.Length)B, entry=0x$($entryRip.ToString('X')), syscalls=$sysCount)" -ForegroundColor Green
    return $true
}

function Validate-Arm64Elf([byte[]]$bytes, $name) {
    if ($bytes.Length -lt 120) {
        Write-Host "[FAIL] ${name}: File too small" -ForegroundColor Red
        return $false
    }
    if ($bytes[0] -ne 0x7F -or $bytes[1] -ne 0x45 -or $bytes[2] -ne 0x4C -or $bytes[3] -ne 0x46) {
        Write-Host "[FAIL] ${name}: Invalid ELF magic" -ForegroundColor Red
        return $false
    }
    $e_machine = [BitConverter]::ToUInt16($bytes, 18)
    $e_entry = [BitConverter]::ToUInt64($bytes, 24)
    if ($e_machine -ne 0xB7) { # EM_AARCH64 (183)
        Write-Host "[FAIL] ${name}: Expected EM_AARCH64 (0xB7), got 0x$($e_machine.ToString('X'))" -ForegroundColor Red
        return $false
    }
    Write-Host "[PASS] ${name}: Valid Linux ARM64 ELF (entry=0x$($e_entry.ToString('X')), EM_AARCH64=0xB7)" -ForegroundColor Green
    return $true
}

function Validate-Arm64Macho([byte[]]$bytes, $name) {
    if ($bytes.Length -lt 472) {
        Write-Host "[FAIL] ${name}: File too small" -ForegroundColor Red
        return $false
    }
    $magic = [BitConverter]::ToUInt32($bytes, 0)
    $cputype = [BitConverter]::ToUInt32($bytes, 4)
    $expected_magic = [BitConverter]::ToUInt32([byte[]](0xCF, 0xFA, 0xED, 0xFE), 0)
    if ($magic -ne $expected_magic -or $cputype -ne 0x0100000C) {
        Write-Host "[FAIL] ${name}: Invalid ARM64 Mach-O header" -ForegroundColor Red
        return $false
    }
    # PC entry from ARM_THREAD_STATE64 (at offset 32 + 152 + 16 + 256 = 456)
    $entryPc = [BitConverter]::ToUInt64($bytes, 456)
    Write-Host "[PASS] ${name}: Valid macOS ARM64 Mach-O (entry=0x$($entryPc.ToString('X')), CPU_TYPE_ARM64=0x0100000C)" -ForegroundColor Green
    return $true
}

# 1. Linux ELF64 Test Suite
Write-Host "============================================================"
Write-Host "  YUKINARY CROSS-PLATFORM LINUX ELF64 VALIDATION SUITE"
Write-Host "============================================================"
$elfCases = @(
    (Join-Path $ROOT "tests\bootstrap\01_hello.ykn"),
    (Join-Path $ROOT "tests\bootstrap\02_arithmetic.ykn"),
    (Join-Path $ROOT "tests\bootstrap\05_structs.ykn"),
    (Join-Path $ROOT "tests\stdlib\05_dosya.ykn")
)
foreach ($c in $elfCases) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($c)
    $outElf = Join-Path $WORK "${base}.elf"
    Remove-Item $outElf -Force -ErrorAction SilentlyContinue
    $cOut = & $Compiler "--target" "linux" $c $outElf 2>&1
    if (Test-Path $outElf) {
        $bytes = Read-BinaryFile $outElf
        if (Validate-Elf64 $bytes "temp_$base.elf") { $passed++ }
        Remove-Item $outElf -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "[FAIL] Compilation failed for ${c}: $($cOut -join ' ')" -ForegroundColor Red
    }
}

# 2. macOS Mach-O 64 Test Suite
Write-Host "`n============================================================"
Write-Host "  YUKINARY CROSS-PLATFORM MACOS MACH-O 64 VALIDATION SUITE"
Write-Host "============================================================"
foreach ($c in $elfCases) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($c)
    $outMacho = Join-Path $WORK "${base}.macho"
    Remove-Item $outMacho -Force -ErrorAction SilentlyContinue
    $cOut = & $Compiler "--target" "macos" $c $outMacho 2>&1
    if (Test-Path $outMacho) {
        $bytes = Read-BinaryFile $outMacho
        if (Validate-Macho64 $bytes "temp_$base.macho") { $passed++ }
        Remove-Item $outMacho -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "[FAIL] Compilation failed for ${c}: $($cOut -join ' ')" -ForegroundColor Red
    }
}

# 3. ARM64 ELF & Mach-O Test Suite
Write-Host "`n============================================================"
Write-Host "  YUKINARY CROSS-PLATFORM ARM64 VALIDATION SUITE"
Write-Host "============================================================"
$armCases = @(
    (Join-Path $ROOT "tests\bootstrap\01_hello.ykn"),
    (Join-Path $ROOT "tests\bootstrap\02_arithmetic.ykn")
)
foreach ($c in $armCases) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($c)
    
    # ARM64 ELF
    $outArmElf = Join-Path $WORK "${base}_arm64.elf"
    Remove-Item $outArmElf -Force -ErrorAction SilentlyContinue
    $null = & $Compiler "--target" "linux-arm64" $c $outArmElf 2>&1
    if (Test-Path $outArmElf) {
        $bytes = Read-BinaryFile $outArmElf
        if (Validate-Arm64Elf $bytes "temp_${base}_arm64.elf") { $passed++ }
        Remove-Item $outArmElf -Force -ErrorAction SilentlyContinue
    }

    # ARM64 Mach-O
    $outArmMacho = Join-Path $WORK "${base}_arm64.macho"
    Remove-Item $outArmMacho -Force -ErrorAction SilentlyContinue
    $null = & $Compiler "--target" "macos-arm64" $c $outArmMacho 2>&1
    if (Test-Path $outArmMacho) {
        $bytes = Read-BinaryFile $outArmMacho
        if (Validate-Arm64Macho $bytes "temp_${base}_arm64.macho") { $passed++ }
        Remove-Item $outArmMacho -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "------------------------------------------------------------"
Write-Host "Results: $passed/$total tests passed"
if ($passed -eq $total) {
    Write-Host "ALL CROSS-PLATFORM TESTS PASSED! [OK]" -ForegroundColor Green
    exit 0
} else {
    Write-Host "SOME CROSS-PLATFORM TESTS FAILED." -ForegroundColor Red
    exit 1
}
