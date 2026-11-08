# ============================================================
# YknOS: AI-First Neural Linux Operating System (QEMU Boot)
# Linux LTS Kernel 6.6 x86_64 + Pure YKN Userspace + Alpine Minirootfs
# ============================================================

$ScriptDir = $PSScriptRoot
Push-Location $ScriptDir

try {
    $QemuBin = "C:\Program Files\qemu\qemu-system-x86_64.exe"
    if (-not (Test-Path $QemuBin)) {
        $QemuBin = "qemu-system-x86_64"
    }

    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "  Booting YknOS 6.1 LTS (Linux Kernel + Pure YKN Userspace)  " -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Cyan

    & $QemuBin `
        -kernel "kernel\vmlinuz-lts" `
        -initrd "kernel\yknos_initramfs.cpio.gz" `
        -m 512M `
        -vga std `
        -serial stdio `
        -append "console=tty0 console=ttyS0,115200 init=/init quiet loglevel=3"
} finally {
    Pop-Location
}
