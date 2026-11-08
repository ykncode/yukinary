<#
.SYNOPSIS
    test_linux_elf.ps1 — Yukinary Linux x86-64 ELF64 binary doğrulama suite (Saf PowerShell)
.DESCRIPTION
    Python bağımlılığı olmadan ELF64 başlık, program header ve _start thunk doğrulaması yapar.
    Checks:
      1. ELF64 Header (Magic, Class, Endian, Machine=0x3E, ET_EXEC)
      2. Program Header (PT_LOAD, p_vaddr=0x400000, PF_R|PF_W|PF_X)
      3. Linux _start Thunk ABI (argc/argv, CALL ana, exit syscall 60)
      4. Syscall emission (0x0F 0x05 varlığı)
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
    Join-Path $ROOT "build\test-work\crossplatform-linux-elf"
} else {
    Join-Path $WorkRoot "crossplatform-linux-elf"
}
New-Item -ItemType Directory -Path $WORK -Force | Out-Null

$TestCases = @(
    (Join-Path $ROOT "tests\bootstrap\01_hello.ykn"),
    (Join-Path $ROOT "tests\bootstrap\02_arithmetic.ykn"),
    (Join-Path $ROOT "tests\bootstrap\05_structs.ykn"),
    (Join-Path $ROOT "tests\stdlib\05_dosya.ykn")
)

$passed = 0
$total  = $TestCases.Count

Write-Host ("=" * 60) -ForegroundColor Cyan
Write-Host "  YUKINARY CROSS-PLATFORM LINUX ELF64 VALIDATION SUITE" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor Cyan

function Read-U16LE([byte[]]$data, [int]$offset) {
    return [int]$data[$offset] + ([int]$data[$offset+1] -shl 8)
}
function Read-U32LE([byte[]]$data, [int]$offset) {
    return [uint32](([uint32]$data[$offset]) -bor (([uint32]$data[$offset+1]) -shl 8) -bor (([uint32]$data[$offset+2]) -shl 16) -bor (([uint32]$data[$offset+3]) -shl 24))
}
function Read-U64LE([byte[]]$data, [int]$offset) {
    $lo = Read-U32LE $data $offset
    $hi = Read-U32LE $data ($offset+4)
    return [uint64]$lo + ([uint64]$hi -shl 32)
}

function Test-ELF64Binary([string]$elfPath) {
    if (-not (Test-Path $elfPath)) {
        Write-Host "  [FAIL] Dosya bulunamadi: $elfPath" -ForegroundColor Red
        return $false
    }
    [byte[]]$data = [System.IO.File]::ReadAllBytes($elfPath)
    $size = $data.Count

    if ($size -lt 120) {
        Write-Host "  [FAIL] Dosya cok kucuk ($size bayt)" -ForegroundColor Red
        return $false
    }

    # 1. ELF Magic
    if ($data[0] -ne 0x7F -or $data[1] -ne 0x45 -or $data[2] -ne 0x4C -or $data[3] -ne 0x46) {
        Write-Host "  [FAIL] Gecersiz ELF magic" -ForegroundColor Red
        return $false
    }
    if ($data[4] -ne 2 -or $data[5] -ne 1) {
        Write-Host "  [FAIL] Beklenen 64-bit Little Endian ELF, class=$($data[4]) data=$($data[5])" -ForegroundColor Red
        return $false
    }

    # ELF Header fields
    $e_type    = Read-U16LE $data 16
    $e_machine = Read-U16LE $data 18
    $e_entry   = Read-U64LE $data 24
    $e_phoff   = Read-U64LE $data 32
    $e_phentsize = Read-U16LE $data 54
    $e_phnum   = Read-U16LE $data 56

    if ($e_type -ne 2) {
        Write-Host "  [FAIL] Beklenen ET_EXEC(2), alindi: $e_type" -ForegroundColor Red
        return $false
    }
    if ($e_machine -ne 0x3E) {
        Write-Host "  [FAIL] Beklenen EM_X86_64(0x3E), alindi: 0x{0:X}" -f $e_machine -ForegroundColor Red
        return $false
    }
    if ($e_phoff -ne 64) {
        Write-Host "  [FAIL] e_phoff=64 beklendi, alindi: $e_phoff" -ForegroundColor Red
        return $false
    }
    if ($e_phentsize -ne 56 -or ($e_phnum -ne 1 -and $e_phnum -ne 2)) {
        Write-Host "  [FAIL] 1 veya 2 program header (56 bayt) beklendi, alindi: $e_phnum adet $e_phentsize bayt" -ForegroundColor Red
        return $false
    }

    # 2. Program Header
    $p_type  = Read-U32LE $data 64
    $p_flags = Read-U32LE $data 68
    $p_vaddr = Read-U64LE $data 80

    if ($p_type -ne 1) {
        Write-Host "  [FAIL] Beklenen PT_LOAD(1), alindi: $p_type" -ForegroundColor Red
        return $false
    }
    if ($p_flags -ne 7) {
        Write-Host "  [FAIL] Beklenen PF_R|PF_W|PF_X(7), alindi: $p_flags" -ForegroundColor Red
        return $false
    }
    if ($p_vaddr -ne 0x400000) {
        Write-Host "  [FAIL] p_vaddr=0x400000 beklendi, alindi: 0x{0:X}" -f $p_vaddr -ForegroundColor Red
        return $false
    }

    # 3. _start thunk dogrulama
    $entryFileOffset = [int]($e_entry - 0x400000)
    if ($entryFileOffset -lt 0 -or ($entryFileOffset + 34) -gt $size) {
        Write-Host "  [FAIL] Giris noktasi dosya sinirlarinin disinda: offset=$entryFileOffset" -ForegroundColor Red
        return $false
    }

    # MOV RCX,[RSP]: 48 8B 0C 24
    if ($data[$entryFileOffset] -ne 0x48 -or $data[$entryFileOffset+1] -ne 0x8B -or $data[$entryFileOffset+2] -ne 0x0C -or $data[$entryFileOffset+3] -ne 0x24) {
        Write-Host "  [FAIL] _start thunk prolog uyusmadi (argc MOV)" -ForegroundColor Red
        return $false
    }
    # LEA RDX,[RSP+8]: 48 8D 54 24 08
    if ($data[$entryFileOffset+4] -ne 0x48 -or $data[$entryFileOffset+5] -ne 0x8D -or $data[$entryFileOffset+6] -ne 0x54 -or $data[$entryFileOffset+7] -ne 0x24 -or $data[$entryFileOffset+8] -ne 0x08) {
        Write-Host "  [FAIL] _start thunk prolog uyusmadi (argv LEA)" -ForegroundColor Red
        return $false
    }
    # CALL ana: E8 ...
    if ($data[$entryFileOffset+13] -ne 0xE8) {
        Write-Host "  [FAIL] CALL ana opcode beklendi (0xE8)" -ForegroundColor Red
        return $false
    }
    # MOV RAX,60 (SYS_exit): 48 C7 C0 3C 00 00 00
    if ($data[$entryFileOffset+25] -ne 0x48 -or $data[$entryFileOffset+26] -ne 0xC7 -or $data[$entryFileOffset+27] -ne 0xC0 -or $data[$entryFileOffset+28] -ne 0x3C) {
        Write-Host "  [FAIL] exit syscall (SYS_exit=60) uyusmadi" -ForegroundColor Red
        return $false
    }
    # SYSCALL: 0F 05
    if ($data[$entryFileOffset+32] -ne 0x0F -or $data[$entryFileOffset+33] -ne 0x05) {
        Write-Host "  [FAIL] SYSCALL opcode (0x0F 0x05) bulunamadi" -ForegroundColor Red
        return $false
    }

    # 4. Syscall sayisi
    $syscallCount = 0
    for ($i = 0; $i -lt $size - 1; $i++) {
        if ($data[$i] -eq 0x0F -and $data[$i+1] -eq 0x05) { $syscallCount++ }
    }
    if ($syscallCount -eq 0) {
        Write-Host "  [FAIL] Syscall (0x0F 0x05) bulunamadi" -ForegroundColor Red
        return $false
    }

    $name = Split-Path $elfPath -Leaf
    Write-Host ("  [PASS] {0}: Gecerli ELF64 (boyut={1}B, giris=0x{2:X}, syscall={3})" -f $name, $size, $e_entry, $syscallCount) -ForegroundColor Green
    return $true
}

foreach ($ykn in $TestCases) {
    $name   = [System.IO.Path]::GetFileNameWithoutExtension($ykn)
    $outElf = Join-Path $WORK "${name}.elf"
    Remove-Item $outElf -Force -ErrorAction SilentlyContinue

    $cr = & $Compiler "--target" "linux" $ykn $outElf 2>&1
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $outElf)) {
        Write-Host "  [FAIL] Derleme hatasi: $ykn" -ForegroundColor Red
        continue
    }

    if (Test-ELF64Binary $outElf) { $passed++ }
    Remove-Item $outElf -Force -ErrorAction SilentlyContinue
}

Write-Host ("-" * 60)
Write-Host "Sonuc: $passed/$total test gecti"
if ($passed -eq $total) {
    Write-Host "TUM LINUX ELF64 CROSS-PLATFORM TESTLERI GECTI! [OK]" -ForegroundColor Green
    exit 0
} else {
    Write-Host "BAZI TESTLER BASARISIZ." -ForegroundColor Red
    exit 1
}
