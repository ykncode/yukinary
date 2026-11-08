@echo off
rem ============================================================
rem YknOS Bare-Metal QEMU Cekirdek Derleme Betigi
rem ============================================================

set NASM_EXE="C:\Users\naitr\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\nasm.exe"

if not exist %NASM_EXE% (
    echo [HATA] NASM bulunamadi!
    exit /b 1
)

echo [YknOS] Bare-metal Multiboot1 cekirdek derleniyor...
%NASM_EXE% -f bin "%~dp0boot\yknos_baremetal.asm" -o "%~dp0yknos_baremetal.bin"

if %ERRORLEVEL% EQU 0 (
    echo [YknOS] Bare-metal cekirdek basariyla derlendi: %~dp0yknos_baremetal.bin
) else (
    echo [YknOS] Derleme basarisiz oldu! Kod: %ERRORLEVEL%
)
