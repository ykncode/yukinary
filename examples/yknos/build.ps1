# ============================================================
# YknOS: Build Script (Compile YKN Userspace & Pack Rootfs)
# ============================================================

$ScriptDir = $PSScriptRoot
Push-Location $ScriptDir

try {
    $YknCompiler = "$ScriptDir\..\yukinary-language\ykn.exe"
    if (-not (Test-Path $YknCompiler)) {
        $YknCompiler = "ykn"
    }

    Write-Host "[YknOS] Compiling YKN Userspace components for Linux ELF64..." -ForegroundColor Cyan

    # 1. Compile ykn-init
    Write-Host "  -> Compiling /bin/ykn-init..." -ForegroundColor Gray
    & $YknCompiler --target linux "$ScriptDir\core\ykn_init.ykn" "$ScriptDir\rootfs\bin\ykn-init"

    # 2. Compile yknsh
    Write-Host "  -> Compiling /bin/yknsh..." -ForegroundColor Gray
    & $YknCompiler --target linux "$ScriptDir\core\ykn_shell.ykn" "$ScriptDir\rootfs\bin\yknsh"

    # 3. Compile yuki
    Write-Host "  -> Compiling /bin/yuki..." -ForegroundColor Gray
    & $YknCompiler --target linux "$ScriptDir\core\yuki_core.ykn" "$ScriptDir\rootfs\bin\yuki"

    # 4. Compile ykn-browser
    Write-Host "  -> Compiling /bin/ykn-browser..." -ForegroundColor Gray
    & $YknCompiler --target linux "$ScriptDir\core\ykn_browser.ykn" "$ScriptDir\rootfs\bin\ykn-browser"

    # 5. Pack Initramfs
    Write-Host "[YknOS] Packaging initramfs.cpio.gz..." -ForegroundColor Cyan
    python "$ScriptDir\build_initramfs.py"

    Write-Host "[YknOS] Build complete! Ready to boot with .\boot_qemu.ps1" -ForegroundColor Green
} finally {
    Pop-Location
}
