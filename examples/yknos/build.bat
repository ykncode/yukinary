@echo off
rem ============================================================
rem YknOS Derleme Betigi (Windows CMD)
rem ============================================================

echo [YknOS] Yukinary derleyicisi cagiriliyor...
cd /d "%~dp0..\.."

.\ykn.exe examples\yknos\yknos_main.ykn examples\yknos\yknos.exe

if %ERRORLEVEL% EQU 0 (
    echo [YknOS] Derleme basarili: examples\yknos\yknos.exe
) else (
    echo [YknOS] Derleme basarisiz oldu! Kod: %ERRORLEVEL%
)
