@echo off
rem ============================================================
rem YknOS Canli Demo Calistirici (Windows CMD)
rem ============================================================

cd /d "%~dp0..\.."

if not exist "examples\yknos\yknos.exe" (
    echo [YknOS] Calistirilabilir ikili bulunamadi, once derleniyor...
    call "examples\yknos\build.bat"
)

echo.
echo [YknOS] Canli Isletim Sistemi Calisma Zamani Baslatiliyor...
echo.
"examples\yknos\yknos.exe"

