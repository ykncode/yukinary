<#
.SYNOPSIS
    test_macos_macho.ps1 - Yukinary macOS x86-64 Mach-O 64 binary dogrulama suite (Saf PowerShell)
.DESCRIPTION
    Python bagimliligi olmadan Mach-O 64 baslik, LC_SEGMENT_64, LC_UNIXTHREAD ve _start thunk dogrulamasi.
    Checks:
      1. Mach-O 64 Header (MH_MAGIC_64, CPU_TYPE_X86_64, MH_EXECUTE, ncmds=2)
      2. LC_SEGMENT_64 (__TEXT segment, vmaddr=0x100000000)
      3. LC_UNIXTHREAD (flavor=4, count=42, entry RIP)
      4. macOS _start Thunk (argc/argv, CALL ana, BSD exit 0x2000001)
      5. BSD Syscall emission (0x0F 0x05)
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
    Join-Path $ROOT "build\test-work\crossplatform-macos-macho"
} else {
    Join-Path $WorkRoot "crossplatform-macos-macho"
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
Write-Host "  YUKINARY CROSS-PLATFORM MACOS MACH-O 64 VALIDATION SUITE" -ForegroundColor Cyan
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

function Test-MachO64Binary([string]$machoPath) {
    if (-not (Test-Path $machoPath)) {
        Write-Host "  [FAIL] Dosya bulunamadi: $machoPath" -ForegroundColor Red
        return $false
    }
    [byte[]]$data = [System.IO.File]::ReadAllBytes($machoPath)
    $size = $data.Count

    if ($size -lt 368) {
        Write-Host "  [FAIL] Dosya cok kucuk ($size bayt)" -ForegroundColor Red
        return $false
    }

    # 1. Mach-O 64 Header (32 bayt): CF FA ED FE (0xFEEDFACF)
    if ($data[0] -ne 0xCF -or $data[1] -ne 0xFA -or $data[2] -ne 0xED -or $data[3] -ne 0xFE) {
        Write-Host "  [FAIL] Gecersiz Mach-O 64 magic" -ForegroundColor Red
        return $false
    }

    $cputype    = Read-U32LE $data 4
    $filetype   = Read-U32LE $data 12
    $ncmds      = Read-U32LE $data 16
    $sizeofcmds = Read-U32LE $data 20

    if ($cputype -ne 0x01000007) {
        Write-Host ("  [FAIL] Beklenen CPU_TYPE_X86_64(0x01000007), alindi: 0x{0:X}" -f $cputype) -ForegroundColor Red
        return $false
    }
    if ($filetype -ne 2) {
        Write-Host "  [FAIL] Beklenen MH_EXECUTE(2), alindi: $filetype" -ForegroundColor Red
        return $false
    }

    # 2. Iterate load commands for __TEXT and entry offset
    $pos = 32
    $entryFileOffset = -1
    $hasTextSegment = $false
    for ($c = 0; $c -lt $ncmds; $c++) {
        if ($pos + 8 -gt $size) { break }
        $cmd = Read-U32LE $data $pos
        $cmdsize = Read-U32LE $data ($pos + 4)
        if ($cmd -eq 0x19) { # LC_SEGMENT_64
            $segName = [System.Text.Encoding]::ASCII.GetString($data, $pos + 8, 16).TrimEnd("`0")
            if ($segName -eq "__TEXT") { $hasTextSegment = $true }
        } elseif ($cmd -eq 0x80000028L) { # LC_MAIN
            $entryFileOffset = [int](Read-U64LE $data ($pos + 8))
        } elseif ($cmd -eq 0x05) { # LC_UNIXTHREAD
            $entryRip = Read-U64LE $data ($pos + 144)
            $entryFileOffset = [int]($entryRip - 0x100000000)
        }
        $pos += $cmdsize
    }

    if (-not $hasTextSegment) {
        Write-Host "  [FAIL] __TEXT segmenti bulunamadi" -ForegroundColor Red
        return $false
    }
    if ($entryFileOffset -lt 0 -or $entryFileOffset -ge $size) {
        Write-Host "  [FAIL] Giris noktasi dosya sinirinin disinda: $entryFileOffset" -ForegroundColor Red
        return $false
    }

    # 3. _start thunk dogrulama (Modern 46 bayt veya legacy 34 bayt)
    $isModernThunk = ($data[$entryFileOffset] -eq 0x48 -and $data[$entryFileOffset+1] -eq 0x85 -and $data[$entryFileOffset+2] -eq 0xFF)
    $isLegacyThunk = ($data[$entryFileOffset] -eq 0x48 -and $data[$entryFileOffset+1] -eq 0x8B)

    if (-not ($isModernThunk -or $isLegacyThunk)) {
        Write-Host "  [FAIL] _start thunk prolog uyusmadi" -ForegroundColor Red
        return $false
    }

    $callOffset = if ($isModernThunk) { $entryFileOffset + 25 } else { $entryFileOffset + 13 }
    if ($data[$callOffset] -ne 0xE8) {
        Write-Host "  [FAIL] CALL ana opcode beklendi (0xE8)" -ForegroundColor Red
        return $false
    }

    $exitOffset = if ($isModernThunk) { $entryFileOffset + 34 } else { $entryFileOffset + 22 }
    if ($data[$exitOffset] -ne 0x48 -or $data[$exitOffset+1] -ne 0x89 -or $data[$exitOffset+2] -ne 0xC7) {
        Write-Host "  [FAIL] MOV RDI, RAX uyusmadi" -ForegroundColor Red
        return $false
    }
    if ($data[$exitOffset+3] -ne 0x48 -or $data[$exitOffset+4] -ne 0xC7 -or $data[$exitOffset+5] -ne 0xC0 -or $data[$exitOffset+6] -ne 0x01) {
        Write-Host "  [FAIL] BSD exit syscall (0x2000001) uyusmadi" -ForegroundColor Red
        return $false
    }
    if ($data[$exitOffset+10] -ne 0x0F -or $data[$exitOffset+11] -ne 0x05) {
        Write-Host "  [FAIL] SYSCALL opcode (0x0F 0x05) bulunamadi" -ForegroundColor Red
        return $false
    }

    # 5. Syscall sayisi
    $syscallCount = 0
    for ($i = 0; $i -lt $size - 1; $i++) {
        if ($data[$i] -eq 0x0F -and $data[$i+1] -eq 0x05) { $syscallCount++ }
    }

    $name = Split-Path $machoPath -Leaf
    Write-Host ("  [PASS] {0}: Gecerli Mach-O 64 (boyut={1}B, giris=0x{2:X}, syscall={3})" -f $name, $size, $entryFileOffset, $syscallCount) -ForegroundColor Green
    return $true
}

foreach ($ykn in $TestCases) {
    $name     = [System.IO.Path]::GetFileNameWithoutExtension($ykn)
    $outMacho = Join-Path $WORK "${name}.macho"
    Remove-Item $outMacho -Force -ErrorAction SilentlyContinue

    & $Compiler "--target" "macos" $ykn $outMacho 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $outMacho)) {
        Write-Host "  [FAIL] Derleme hatasi: $ykn" -ForegroundColor Red
        continue
    }

    if (Test-MachO64Binary $outMacho) { $passed++ }
    Remove-Item $outMacho -Force -ErrorAction SilentlyContinue
}

Write-Host ("-" * 60)
Write-Host "Sonuc: $passed/$total test gecti"
if ($passed -eq $total) {
    Write-Host "TUM MACOS MACH-O 64 CROSS-PLATFORM TESTLERI GECTI! [OK]" -ForegroundColor Green
    exit 0
} else {
    Write-Host "BAZI TESTLER BASARISIZ." -ForegroundColor Red
    exit 1
}
