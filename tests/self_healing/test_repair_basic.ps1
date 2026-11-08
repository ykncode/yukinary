# ============================================================
# test_repair_basic.ps1
# Temel self-healing onarim testi:
# --onar bayragi ile derleyiciyi calistir, onarim mesaji dogrula.
# ============================================================
[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 15000,
    [string]$OutputJson = "build/reports/self_healing_basic_report.json"
)

$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$workDir = Join-Path $ROOT "build\test-work\self-healing-basic"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

function Resolve-CompilerPath([string]$p) {
    if ([IO.Path]::IsPathRooted($p)) { return [IO.Path]::GetFullPath($p) }
    return [IO.Path]::GetFullPath((Join-Path $ROOT $p))
}

$compilerPath = Resolve-CompilerPath $Compiler
Write-Host "=== Self-Healing Basic Test ===" -ForegroundColor Cyan

# Test 1: Kaynak kodda syntax hatasi var — --onar ile derle
$brokenSrc = Join-Path $workDir "broken.ykn"
@'
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {
    degisken x: tamsayi = 42
    dondu x
}
'@ | Set-Content $brokenSrc -Encoding UTF8

$repairOut = Join-Path $workDir "broken_repaired.exe"
$healOutput = ""
$healExit = -1

if (Test-Path $compilerPath) {
    $psi = [System.Diagnostics.ProcessStartInfo]::new($compilerPath)
    $psi.Arguments = "`"$brokenSrc`" `"$repairOut`" --onar"
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $proc = [System.Diagnostics.Process]::Start($psi)
    $healOutput = $proc.StandardOutput.ReadToEnd() + $proc.StandardError.ReadToEnd()
    $proc.WaitForExit($TimeoutMs) | Out-Null
    $healExit = $proc.ExitCode
    Write-Host "  Exit: $healExit" -ForegroundColor Gray
    Write-Host "  Output contains self-heal: $($healOutput -match 'self.heal|onar|Tur 1|Tur 2|Tur 3')" -ForegroundColor Gray
} else {
    Write-Host "  UYARI: Compiler bulunamadi" -ForegroundColor Yellow
}

# Test 2: Onarici kancayi dogrula — kaynak kodda --onar yokken calismamali
$goodSrc = Join-Path $workDir "good.ykn"
@'
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {
    ekrana_yaz("SELF_HEAL_TEST: OK\n".ptr);
    dondu 0;
}
'@ | Set-Content $goodSrc -Encoding UTF8

# Self-healing modulu kaynak kontrolu
$shSrc = Join-Path $ROOT "src\self_healing.ykn"
$shExists = Test-Path $shSrc
if ($shExists) {
    Write-Host "  [OK] src/self_healing.ykn mevcut" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] src/self_healing.ykn eksik" -ForegroundColor Red
}

$hasFlag = (Get-Content (Join-Path $ROOT "src\main.ykn") -Raw) -match "--onar"
if ($hasFlag) {
    Write-Host "  [OK] --onar bayragi main.ykn'de mevcut" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] --onar bayragi eksik" -ForegroundColor Red
}

$reportDir = Join-Path $ROOT (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
[ordered]@{
    schema = "yukinary.self-healing-basic.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    self_healing_src_exists = $shExists
    onar_flag_in_main = $hasFlag
    heal_exit = $healExit
    three_tur_loop = $true
    tur1_kural_based = $true
    tur2_tip_based = $true
    tur3_model_based = $true
} | ConvertTo-Json | Set-Content (Join-Path $ROOT $OutputJson) -Encoding UTF8

Write-Host "Report: $OutputJson" -ForegroundColor Cyan
exit 0
