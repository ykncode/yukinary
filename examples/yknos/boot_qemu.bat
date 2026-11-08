@echo off
REM ============================================================
REM YknOS: AI-First Neural Linux Operating System (QEMU Boot)
REM Linux LTS Kernel 6.6 x86_64 + Pure YKN Userspace + Alpine Minirootfs
REM ============================================================

set QEMU="C:\Program Files\qemu\qemu-system-x86_64.exe"
if not exist %QEMU% set QEMU="qemu-system-x86_64"

echo ============================================================
echo   Booting YknOS 6.1 LTS in QEMU (Linux Kernel + YKN Userspace)
echo ============================================================

%QEMU% -kernel kernel\vmlinuz-lts -initrd kernel\yknos_initramfs.cpio.gz -m 512M -vga std -serial stdio -append "console=tty0 console=ttyS0,115200 init=/init quiet loglevel=3"

pause
