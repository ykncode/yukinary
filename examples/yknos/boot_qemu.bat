@echo off
rem ============================================================
rem YknOS Bare-Metal QEMU Sanal Makine Baslatici
rem ============================================================

set QEMU_EXE="C:\Program Files\qemu\qemu-system-x86_64.exe"
set BIN_FILE="%~dp0yknos_baremetal.bin"

if not exist %BIN_FILE% (
    echo [YknOS] Cekirdek ikilisi bulunamadi, derleniyor...
    call "%~dp0build_qemu.bat"
)

if not exist %QEMU_EXE% (
    echo [HATA] QEMU bulunamadi: %QEMU_EXE%
    exit /b 1
)

echo.
echo ============================================================
echo   YknOS QEMU Sanal Makinesinde Baslatiliyor...
echo   Ekranda YknOS VGA Konsolu acilacak.
echo   Seri port (COM1) ciktilari bu terminale akacaktir.
echo ============================================================
echo.

%QEMU_EXE% -kernel %BIN_FILE% -serial stdio
