#!/usr/bin/env bash
# ==============================================================================
# Yukinary Turnkey Setup & Installer for Linux & macOS
# ==============================================================================
set -e

echo ""
echo "========================================================"
echo "       Yukinary v1.0.0 Turnkey Setup (Linux / macOS)    "
echo "========================================================"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YKN_HOME="$HOME/.yukinary"
YKN_BIN="$YKN_HOME/bin"
YKN_STD="$YKN_HOME/std"

mkdir -p "$YKN_BIN"
mkdir -p "$YKN_STD"

# 1. Locate ykn binary
SRC_BIN=""
if [ -f "$SCRIPT_DIR/ykn" ]; then
    SRC_BIN="$SCRIPT_DIR/ykn"
elif [ -f "$SCRIPT_DIR/ykn_linux" ]; then
    SRC_BIN="$SCRIPT_DIR/ykn_linux"
elif [ -f "$SCRIPT_DIR/bin/ykn" ]; then
    SRC_BIN="$SCRIPT_DIR/bin/ykn"
fi

if [ -z "$SRC_BIN" ]; then
    echo "[ERROR] ykn binary not found in $SCRIPT_DIR"
    exit 1
fi

cp "$SRC_BIN" "$YKN_BIN/ykn"
chmod +x "$YKN_BIN/ykn"
echo "[+] Installed ykn binary to $YKN_BIN/ykn"

# 2. Copy standard library
SRC_STD=""
if [ -d "$SCRIPT_DIR/std" ]; then
    SRC_STD="$SCRIPT_DIR/std"
elif [ -d "$SCRIPT_DIR/../std" ]; then
    SRC_STD="$SCRIPT_DIR/../std"
fi

if [ -n "$SRC_STD" ]; then
    cp -r "$SRC_STD"/* "$YKN_STD/"
    echo "[+] Installed Yukinary Standard Library to $YKN_STD"
fi

# 3. Locate and install VS Code Extension if code CLI is present
SRC_VSIX=""
if [ -f "$SCRIPT_DIR/yukinary-lang-1.0.0.vsix" ]; then
    SRC_VSIX="$SCRIPT_DIR/yukinary-lang-1.0.0.vsix"
elif [ -f "$SCRIPT_DIR/vscode/yukinary-lang-1.0.0.vsix" ]; then
    SRC_VSIX="$SCRIPT_DIR/vscode/yukinary-lang-1.0.0.vsix"
elif [ -f "$SCRIPT_DIR/vscode-eklentisi/yukinary-lang-1.0.0.vsix" ]; then
    SRC_VSIX="$SCRIPT_DIR/vscode-eklentisi/yukinary-lang-1.0.0.vsix"
fi

if command -v code >/dev/null 2>&1 && [ -n "$SRC_VSIX" ]; then
    echo "[*] Visual Studio Code detected. Installing Yukinary extension..."
    code --install-extension "$SRC_VSIX" --force || true
    echo "[+] VS Code Extension (yukinary-lang-1.0.0) successfully installed!"
fi

# 4. Patch PATH in shell profiles
LINE_TO_ADD='export PATH="$HOME/.yukinary/bin:$PATH"'
for PROFILE in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
    if [ -f "$PROFILE" ]; then
        if ! grep -q '\.yukinary/bin' "$PROFILE"; then
            echo "$LINE_TO_ADD" >> "$PROFILE"
            echo "[+] Added ~/.yukinary/bin to $PROFILE"
        fi
    fi
done

echo ""
echo "========================================================"
echo "  YUKINARY HAS BEEN SUCCESSFULLY INSTALLED & CONFIGURED!"
echo "========================================================"
echo ""
echo "Restart your shell or run:"
echo "  export PATH=\"\$HOME/.yukinary/bin:\$PATH\""
echo ""
echo "Then test running:"
echo "  ykn --yardim"
echo "  ykn --version"
echo ""

