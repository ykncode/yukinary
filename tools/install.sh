#!/bin/sh
# ==============================================================================
# Yukinary Global CLI Installer (Linux / macOS)
# ==============================================================================
# Installs 'ykn' to ~/.yukinary/bin and adds it to PATH in ~/.bashrc or ~/.zshrc.
# ==============================================================================

set -e

INSTALL_DIR="$HOME/.yukinary/bin"
STD_DIR="$HOME/.yukinary/std"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "========================================================"
echo "Installing Yukinary CLI to: $INSTALL_DIR"
echo "========================================================"

mkdir -p "$INSTALL_DIR"

if [ -f "$REPO_ROOT/dist/ykn" ]; then
    cp "$REPO_ROOT/dist/ykn" "$INSTALL_DIR/ykn"
elif [ -f "$REPO_ROOT/ykn" ]; then
    cp "$REPO_ROOT/ykn" "$INSTALL_DIR/ykn"
else
    echo "[ERROR] ykn Linux binary not found in dist/ or root."
    exit 1
fi

chmod +x "$INSTALL_DIR/ykn"
echo "[OK] Installed ykn binary."

if [ -d "$REPO_ROOT/std" ]; then
    rm -rf "$STD_DIR"
    cp -r "$REPO_ROOT/std" "$STD_DIR"
    echo "[OK] Installed Standard Library to $STD_DIR."
fi

# Add to PATH in shell profile
SHELL_RC=""
if [ -n "$ZSH_VERSION" ] || [ -f "$HOME/.zshrc" ]; then
    SHELL_RC="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    SHELL_RC="$HOME/.bashrc"
fi

if [ -n "$SHELL_RC" ]; then
    if ! grep -q '\.yukinary/bin' "$SHELL_RC"; then
        echo 'export PATH="$HOME/.yukinary/bin:$PATH"' >> "$SHELL_RC"
        echo "[SUCCESS] Added ~/.yukinary/bin to $SHELL_RC"
    else
        echo "[OK] ~/.yukinary/bin is already in $SHELL_RC"
    fi
fi

echo ""
echo "Installation complete! Restart your shell or run:"
echo "  export PATH=\"\$HOME/.yukinary/bin:\$PATH\""
echo "Then simply type:"
echo "  ykn --help"
echo "  ykn convert test.py"
echo "  ykn main.ykn"

