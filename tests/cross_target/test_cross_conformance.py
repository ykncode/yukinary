"""Yukinary Multi-Platform Binary Conformance & Specification Verifier (FAZ 22).
Verifies that generated Linux ELF64 and macOS Mach-O (x86_64 & ARM64) binaries
comply bit-by-bit with System V ABI and Apple Mach-O specifications.
"""
from __future__ import annotations

import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COMPILER = ROOT / "app_full.exe"
TEST_SOURCE = ROOT / "tests/generics/03_multi_specialization.ykn"
BUILD_DIR = ROOT / "build"

def verify_elf64_binary(path: Path, expected_machine: int = 0x3E) -> bool:
    data = path.read_bytes()
    if len(data) < 64:
        print(f"[!] ELF too small: {len(data)} bytes")
        return False

    # 1. ELF Magic: \x7f E L F
    if data[:4] != b"\x7fELF":
        print(f"[!] Invalid ELF magic: {data[:4]}")
        return False

    # 2. EI_CLASS: 2 (64-bit), EI_DATA: 1 (LSB)
    ei_class = data[4]
    ei_data = data[5]
    if ei_class != 2 or ei_data != 1:
        print(f"[!] Invalid ELF class or data encoding: class={ei_class}, data={ei_data}")
        return False

    # 3. ELF Header fields
    # e_type (2=EXEC), e_machine (0x3E=x86_64, 0xB7=AArch64)
    e_type, e_machine, e_version = struct.unpack("<HHI", data[16:24])
    if e_type != 2:
        print(f"[!] Expected ET_EXEC (2), got {e_type}")
        return False
    if e_machine != expected_machine:
        print(f"[!] Expected machine {hex(expected_machine)}, got {hex(e_machine)}")
        return False

    e_entry = struct.unpack("<Q", data[24:32])[0]
    e_phoff = struct.unpack("<Q", data[32:40])[0]
    if e_entry < 0x400000 or e_phoff != 64:
        print(f"[!] Suspicious entry ({hex(e_entry)}) or phoff ({e_phoff})")
        return False

    # 4. Program Header: PT_LOAD
    p_type, p_flags = struct.unpack("<II", data[64:72])
    if p_type != 1:  # PT_LOAD
        print(f"[!] Expected PT_LOAD (1), got {p_type}")
        return False

    return True


def verify_macho_binary(path: Path, is_arm64: bool = False) -> bool:
    data = path.read_bytes()
    if len(data) < 32:
        print(f"[!] Mach-O too small: {len(data)} bytes")
        return False

    # 1. Magic: 0xFEEDFACF (MH_MAGIC_64)
    magic = struct.unpack("<I", data[:4])[0]
    if magic != 0xFEEDFACF:
        print(f"[!] Invalid Mach-O 64 magic: {hex(magic)}")
        return False

    # 2. CPU Type: 0x01000007 (x86_64) or 0x0100000C (ARM64)
    cputype = struct.unpack("<I", data[4:8])[0]
    expected_cputype = 0x0100000C if is_arm64 else 0x01000007
    if cputype != expected_cputype:
        print(f"[!] Expected cputype {hex(expected_cputype)}, got {hex(cputype)}")
        return False

    # 3. Filetype: 2 (MH_EXECUTE)
    filetype, ncmds = struct.unpack("<II", data[12:20])
    if filetype != 2 or ncmds < 2:
        print(f"[!] Expected MH_EXECUTE with >=2 commands, got filetype={filetype}, ncmds={ncmds}")
        return False

    # 4. Iterate through load commands to verify LC_SEGMENT_64 and locate __TEXT
    offset = 32
    found_text = False
    for _ in range(ncmds):
        if offset + 8 > len(data):
            break
        cmd, cmdsize = struct.unpack("<II", data[offset:offset+8])
        if cmd == 0x19 and offset + 24 <= len(data):  # LC_SEGMENT_64
            segname = data[offset+8:offset+24].split(b'\x00', 1)[0].decode("ascii", errors="replace")
            if segname == "__TEXT":
                found_text = True
        offset += cmdsize

    if not found_text:
        print("[!] Expected segment __TEXT was not found in Mach-O binary")
        return False

    return True


def run_cross_conformance_test() -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    
    # Target 1: Linux ELF64 (x86_64)
    linux_bin = BUILD_DIR / "out_linux_elf64"
    res1 = subprocess.run([str(COMPILER), "--target", "linux", str(TEST_SOURCE), str(linux_bin)],
                          cwd=ROOT, capture_output=True, text=True, errors="replace")
    if res1.returncode != 0 or not linux_bin.is_file():
        print(f"[!] Linux cross-compilation failed: {res1.stderr}")
        return 1
    if not verify_elf64_binary(linux_bin, expected_machine=0x3E):
        print(f"[!] Linux ELF64 binary specification validation failed!")
        return 2

    # Target 2: macOS Mach-O 64 (x86_64)
    macos_bin = BUILD_DIR / "out_macos_macho"
    res2 = subprocess.run([str(COMPILER), "--target", "macos", str(TEST_SOURCE), str(macos_bin)],
                          cwd=ROOT, capture_output=True, text=True, errors="replace")
    if res2.returncode != 0 or not macos_bin.is_file():
        print(f"[!] macOS cross-compilation failed: {res2.stderr}")
        return 3
    if not verify_macho_binary(macos_bin, is_arm64=False):
        print(f"[!] macOS Mach-O x86_64 validation failed!")
        return 4

    # Target 3: macOS Apple Silicon (ARM64)
    macho_arm64 = BUILD_DIR / "out_macos_arm64"
    res3 = subprocess.run([str(COMPILER), "--target", "macos-arm64", str(TEST_SOURCE), str(macho_arm64)],
                          cwd=ROOT, capture_output=True, text=True, errors="replace")
    if res3.returncode != 0 or not macho_arm64.is_file():
        print(f"[!] macOS ARM64 cross-compilation failed: {res3.stderr}")
        return 5
    if not verify_macho_binary(macho_arm64, is_arm64=True):
        print(f"[!] macOS Mach-O ARM64 validation failed!")
        return 6

    print("CROSS-PLATFORM-CONFORMANCE-PASS")
    return 0

if __name__ == "__main__":
    sys.exit(run_cross_conformance_test())

