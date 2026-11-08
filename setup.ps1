<#
.SYNOPSIS
    Yukinary Turnkey Setup & Installer for Windows
.DESCRIPTION
    Installs Yukinary Compiler, Standard Library, LSP Server, and VS Code
    Extension into %USERPROFILE%\.yukinary, automatically configuring User PATH
    and system shims for instant 'ykn' CLI access.
#>

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "       Yukinary v1.0.0 Turnkey Setup (Windows)          " -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

$baseDir = $PSScriptRoot
if (-not $baseDir) {
    $baseDir = (Get-Location).Path
}

$userDir = $env:USERPROFILE
$yknHome = Join-Path $userDir ".yukinary"
$yknBin  = Join-Path $yknHome "bin"
$yknStd  = Join-Path $yknHome "std"

# 1. Create target directories
if (-not (Test-Path $yknBin)) {
    New-Item -ItemType Directory -Force -Path $yknBin | Out-Null
}
if (-not (Test-Path $yknStd)) {
    New-Item -ItemType Directory -Force -Path $yknStd | Out-Null
}

# Stop running ykn background processes before updating binaries
Get-Process "ykn*", "ykn_lsp*" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

# 2. Locate ykn.exe binary
$candidateExes = @(
    (Join-Path $baseDir "ykn.exe"),
    (Join-Path $baseDir "bin\ykn.exe"),
    (Join-Path $baseDir "app_full.exe")
)

$srcExe = $null
foreach ($c in $candidateExes) {
    if (Test-Path $c) {
        $srcExe = $c
        break
    }
}

if (-not $srcExe) {
    Write-Host "[ERROR] ykn.exe binary not found in $baseDir" -ForegroundColor Red
    exit 1
}

function Install-FileSafe($src, $dst) {
    if (-not (Test-Path $src)) { return }
    try {
        Copy-Item $src $dst -Force -ErrorAction Stop
    } catch {
        $oldFile = "$dst.old"
        if (Test-Path $oldFile) {
            Remove-Item $oldFile -Force -ErrorAction SilentlyContinue
        }
        Move-Item $dst $oldFile -Force -ErrorAction SilentlyContinue
        Copy-Item $src $dst -Force -ErrorAction SilentlyContinue
        Remove-Item $oldFile -Force -ErrorAction SilentlyContinue
    }
}

Install-FileSafe $srcExe (Join-Path $yknBin "ykn.exe")
Install-FileSafe $srcExe (Join-Path $yknBin "ykn_lsp.exe")
Write-Host "[+] Installed ykn.exe and ykn_lsp.exe to $yknBin" -ForegroundColor Green

# 2b. Locate and install ykn_llm_simd.dll (AVX2 SIMD acceleration runtime for Windows)
$candidateDlls = @(
    (Join-Path $baseDir "ykn_llm_simd.dll"),
    (Join-Path $baseDir "bin\ykn_llm_simd.dll"),
    (Join-Path $baseDir "build\ykn_llm_simd.dll")
)
$srcDll = $null
foreach ($d in $candidateDlls) {
    if (Test-Path $d) {
        $srcDll = $d
        break
    }
}
if ($srcDll) {
    Install-FileSafe $srcDll (Join-Path $yknBin "ykn_llm_simd.dll")
    Write-Host "[+] Installed ykn_llm_simd.dll to $yknBin" -ForegroundColor Green
} else {
    Write-Host "[!] Note: ykn_llm_simd.dll not found in $baseDir" -ForegroundColor Yellow
}

# 3. Create ykn.cmd shim for instant cmd.exe execution
$cmdShim = "@echo off`r`n`"%~dp0ykn.exe`" %*"
Set-Content -Path (Join-Path $yknBin "ykn.cmd") -Value $cmdShim -Encoding ASCII
Write-Host "[+] Created ykn.cmd shim" -ForegroundColor Green

# 4. Copy Standard Library (std/)
$candidateStd = @(
    (Join-Path $baseDir "std"),
    (Join-Path $baseDir "..\std")
)
$srcStd = $null
foreach ($s in $candidateStd) {
    if (Test-Path $s) {
        $srcStd = $s
        break
    }
}

if ($srcStd) {
    Copy-Item -Path "$srcStd\*" -Destination $yknStd -Recurse -Force
    Write-Host "[+] Installed Yukinary Standard Library to $yknStd" -ForegroundColor Green
} else {
    Write-Host "[!] Warning: std/ directory not found nearby. Standard library skipped." -ForegroundColor Yellow
}

# 5. Locate and install VS Code Extension (.vsix)
$candidateVsix = @(
    (Join-Path $baseDir "yukinary-lang-1.0.0.vsix"),
    (Join-Path $baseDir "vscode\yukinary-lang-1.0.0.vsix"),
    (Join-Path $baseDir "vscode-eklentisi\yukinary-lang-1.0.0.vsix")
)
$srcVsix = $null
foreach ($v in $candidateVsix) {
    if (Test-Path $v) {
        $srcVsix = $v
        break
    }
}

$codeCli = Get-Command "code" -ErrorAction SilentlyContinue
if ($codeCli -and $srcVsix) {
    Write-Host "[*] Visual Studio Code detected. Installing Yukinary extension..." -ForegroundColor Cyan
    try {
        & code --install-extension $srcVsix --force | Out-Null
        Write-Host "[+] VS Code Extension (yukinary-lang-1.0.0) successfully installed!" -ForegroundColor Green
    } catch {
        Write-Host "[!] Note: Could not auto-install VS Code extension: $_" -ForegroundColor Yellow
    }
} elseif ($srcVsix) {
    Write-Host "[*] VS Code extension package ready at: $srcVsix" -ForegroundColor Gray
    Write-Host "    (Run 'code --install-extension $srcVsix' to install anytime)" -ForegroundColor Gray
}

# 6. Configure User PATH environment variable
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
$cleanParts = $currentPath -split ";" | Where-Object { $_ -ne "" -and $_ -notlike "*\.yukinary\bin*" }
$newPath = "$yknBin;" + ($cleanParts -join ";")
[Environment]::SetEnvironmentVariable("Path", $newPath, "User")
$env:Path = "$yknBin;$env:Path"

# 7. Sync to WindowsApps (Guaranteed fallback for Windows 10/11)
$winApps = Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps"
if (Test-Path $winApps) {
    Install-FileSafe (Join-Path $yknBin "ykn.exe") (Join-Path $winApps "ykn.exe")
    Install-FileSafe (Join-Path $yknBin "ykn.cmd") (Join-Path $winApps "ykn.cmd")
    if (Test-Path (Join-Path $yknBin "ykn_llm_simd.dll")) {
        Install-FileSafe (Join-Path $yknBin "ykn_llm_simd.dll") (Join-Path $winApps "ykn_llm_simd.dll")
    }
}

# 8. Broadcast WM_SETTINGCHANGE to notify running shells
try {
    Add-Type -Namespace Win32 -Name NativeMethods -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll", SetLastError = true, CharSet = System.Runtime.InteropServices.CharSet.Auto)]
public static extern System.IntPtr SendMessageTimeout(
    System.IntPtr hWnd,
    uint Msg,
    System.UIntPtr wParam,
    string lParam,
    uint fuFlags,
    uint uTimeout,
    out System.UIntPtr lpdwResult);
"@ -ErrorAction SilentlyContinue | Out-Null
    $result = [System.UIntPtr]::Zero
    [Win32.NativeMethods]::SendMessageTimeout([System.IntPtr]0xffff, 0x001A, [System.UIntPtr]::Zero, "Environment", 2, 5000, [ref]$result) | Out-Null
} catch {
    # Non-critical fallback
}

Write-Host ""
Write-Host "========================================================" -ForegroundColor Green
Write-Host "  YUKINARY HAS BEEN SUCCESSFULLY INSTALLED & CONFIGURED!" -ForegroundColor Green
Write-Host "========================================================" -ForegroundColor Green
Write-Host ""
Write-Host "You can now run 'ykn' directly from ANY terminal (PowerShell, CMD, Git Bash):" -ForegroundColor Cyan
Write-Host "  ykn --yardim            (Goster yardim mesaji)" -ForegroundColor White
Write-Host "  ykn --version           (Surum kontrolu)" -ForegroundColor White
Write-Host "  ykn dosya.ykn           (Derle ve calistir)" -ForegroundColor White
Write-Host "  ykn convert kod.py      (Python -> Yukinary AI donusumu)" -ForegroundColor White
Write-Host ""

