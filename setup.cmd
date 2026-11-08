@echo off
setlocal
cd /d "%~dp0"
echo Starting Yukinary Setup...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1"
echo.
pause

