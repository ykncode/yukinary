# tests/run_faz9_suites.ps1
# FAZ 9  LLM, Self-Healing ve Convert Ekosistemi Dogrulama Paketi

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path "$ScriptDir\..").Path
Set-Location $RootDir

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "FAZ 9 - LLM, Self-Healing ve Convert Ekosistemi Test Paketi" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$Stage3 = "$RootDir\build\bootstrap\stage3.exe"
if (-not (Test-Path $Stage3)) {
    Write-Error "HATA: $Stage3 bulunamadi!"
    exit 1
}

Write-Host "[1/4] Synthetic Error Dataset Generator (Deterministic Seed=42)..." -ForegroundColor Yellow
python tools\self_healing_dataset_generator.py 42
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[2/4] Mini Model Training & GGUF Quantization Pipeline..." -ForegroundColor Yellow
python tools\train_mini_model_pipeline.py
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[3/4] Self-Healing Decision Engine Acceptance Suite (120 Held-Out Tests)..." -ForegroundColor Yellow
python tests\self_healing\test_self_healing_suite.py $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[4/6] 20-Library x 5-Fallback Convert Matrix & Cache Suite..." -ForegroundColor Yellow
python tests\convert\test_convert_matrix.py $Stage3
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[5/7] Real Dynamic/Process Backend Suite..." -ForegroundColor Yellow
python tests\convert\test_dynamic_backend.py
if ($LASTEXITCODE -ne 0) { exit 1 }
python tests\convert\test_process_backends.py
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[6/7] Configured Convert Directive Execution Contract..." -ForegroundColor Yellow
python tests\convert\test_directive_execution.py
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[7/8] YKNLLM Gateway Compiler-Sandbox Contract..." -ForegroundColor Yellow
python tests\llm\test_yknllm_gateway.py
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[8/8] User-selected GGUF Model Profile Contract..." -ForegroundColor Yellow
python tests\llm\test_gguf_model_selection.py
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "============================================================" -ForegroundColor Green
Write-Host "FAZ 9 HONEST GATE TAMAMLANDI; dogrulanmamis fallback backend'leri acik" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
