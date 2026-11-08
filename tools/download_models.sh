#!/bin/sh
# ==============================================================================
# Yukinary AI Model Downloader (Linux / macOS)
# ==============================================================================
# Usage:
#   sh tools/download_models.sh [bitnet | bonsai | all]
# ==============================================================================

set -e

MODEL="${1:-bitnet}"

download_model() {
    NAME="$1"
    DIR="$2"
    FILE="$3"
    URL="$4"

    echo ""
    echo "========================================================"
    echo "Model: $NAME"
    echo "Target: $DIR/$FILE"
    echo "========================================================"

    mkdir -p "$DIR"

    if [ -f "$DIR/$FILE" ]; then
        echo "[OK] Model file already exists on disk. Skipping download."
        return
    fi

    echo "Downloading from Hugging Face..."
    if command -v curl >/dev/null 2>&1; then
        curl -L --progress-bar -o "$DIR/$FILE" "$URL"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$DIR/$FILE" "$URL"
    else
        echo "Error: Neither curl nor wget found."
        exit 1
    fi

    echo "[SUCCESS] Saved to $DIR/$FILE"
    echo "Run with:"
    echo "  ./ykn main.ykn --llm-model $DIR/$FILE"
}

case "$MODEL" in
    bitnet)
        download_model "Microsoft BitNet b1.58 2B (Ternary GGUF)" \
            "models/bitnet-b1.58-2B-4T" "ggml-model-i2_s.gguf" \
            "https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf"
        ;;
    bonsai)
        download_model "Prism ML Bonsai 8B (Ultra-Low Bit GGUF)" \
            "models/bonsai-8b" "Bonsai-8B.gguf" \
            "https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf"
        ;;
    all)
        download_model "Microsoft BitNet b1.58 2B (Ternary GGUF)" \
            "models/bitnet-b1.58-2B-4T" "ggml-model-i2_s.gguf" \
            "https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf"
        download_model "Prism ML Bonsai 8B (Ultra-Low Bit GGUF)" \
            "models/bonsai-8b" "Bonsai-8B.gguf" \
            "https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf"
        ;;
    *)
        echo "Usage: sh tools/download_models.sh [bitnet | bonsai | all]"
        exit 1
        ;;
esac

