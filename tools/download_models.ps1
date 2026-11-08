# ==============================================================================
# Yukinary AI Model Downloader (Windows PowerShell)
# ==============================================================================
# Usage:
#   powershell -File tools/download_models.ps1 [bitnet | bonsai | all]
# ==============================================================================

param (
    [string]$Model = "bitnet"
)

$ErrorActionPreference = "Stop"

$MODELS = @{
    "bitnet" = @{
        Name   = "Microsoft BitNet b1.58 2B (Ternary 1.58-bit GGUF)"
        Dir    = "models/bitnet-b1.58-2B-4T"
        File   = "ggml-model-i2_s.gguf"
        Url    = "https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf"
        SizeMB = 1132
    }
    "bonsai" = @{
        Name   = "Prism ML Bonsai 8B (Ultra-Low Bit GGUF)"
        Dir    = "models/bonsai-8b"
        File   = "Bonsai-8B.gguf"
        Url    = "https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf"
        SizeMB = 1104
    }
}

function Download-TargetModel($key) {
    $info = $MODELS[$key]
    Write-Host "`n========================================================" -ForegroundColor Cyan
    Write-Host "Model: $($info.Name)" -ForegroundColor Green
    Write-Host "Target: $($info.Dir)/$($info.File) (~$($info.SizeMB) MB)" -ForegroundColor Yellow
    Write-Host "========================================================" -ForegroundColor Cyan

    if (-not (Test-Path $info.Dir)) {
        New-Item -ItemType Directory -Force -Path $info.Dir | Out-Null
    }

    $outPath = Join-Path $info.Dir $info.File

    if (Test-Path $outPath) {
        $existing = (Get-Item $outPath).Length
        Write-Host "[OK] Model file already exists on disk ($([math]::Round($existing/1MB, 1)) MB). Skipping download." -ForegroundColor Green
        return
    }

    Write-Host "Downloading from Hugging Face..." -ForegroundColor White
    
    # Use curl.exe if available for live visual progress bar
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl) {
        & curl.exe -L --progress-bar -o $outPath $info.Url
    } else {
        Invoke-WebRequest -Uri $info.Url -OutFile $outPath
    }

    if (Test-Path $outPath) {
        $downloaded = (Get-Item $outPath).Length
        Write-Host "`n[SUCCESS] Model successfully saved to $outPath ($([math]::Round($downloaded/1MB, 1)) MB)" -ForegroundColor Green
        Write-Host "You can now run:" -ForegroundColor Yellow
        Write-Host "  .\ykn.exe main.ykn --llm-model $outPath" -ForegroundColor Cyan
    } else {
        Write-Host "[ERROR] Download failed for $outPath" -ForegroundColor Red
    }
}

$choice = $Model.ToLower()
if ($choice -eq "all") {
    Download-TargetModel "bitnet"
    Download-TargetModel "bonsai"
} elseif ($MODELS.ContainsKey($choice)) {
    Download-TargetModel $choice
} else {
    Write-Host "Unknown model '$Model'. Available options: bitnet, bonsai, all" -ForegroundColor Red
}

