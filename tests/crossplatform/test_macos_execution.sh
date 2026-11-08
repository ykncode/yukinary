#!/usr/bin/env bash
# ============================================================
# test_macos_execution.sh
# macOS Mach-O CI: YKN derleyicisini derleyip Mach-O binary
# uretir ve gercek stdout/exit code dogrular.
# ============================================================
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK="$ROOT/build/test-work/macos-macho-ci"
mkdir -p "$WORK"
REPORTS="$ROOT/build/reports"
mkdir -p "$REPORTS"

echo "=== macOS Mach-O Execution CI ==="

# Bu job yalnızca manuel planlı doğrulamada çalışır; hata atlanmaz.
echo "Runner: $(uname -a)"
echo "Architecture: $(uname -m)"
echo "Compiler: $(clang++ --version | sed -n '1p')"
case "$(uname -m)" in
  arm64|x86_64) ;;
  *) echo "[FAIL] Desteklenmeyen macOS runner mimarisi: $(uname -m)"; exit 1 ;;
esac

echo "Derleyici derleniyor (clang++ -O2)..."
clang++ -O2 -std=c++17 -o "$WORK/stage0_macos" "$ROOT/app_full.cpp"

COMPILER="$WORK/stage0_macos"

# Hello world fixture derle (Mach-O hedefi)
SRC="$ROOT/tests/bootstrap/01_hello.ykn"
if [ ! -f "$SRC" ]; then
    echo "[FAIL] Test kaynagi bulunamadi: $SRC"
    exit 1
fi

OUT="$WORK/hello_macos.macho"
echo "Mach-O binary üretiliyor..."
"$COMPILER" "$SRC" "$OUT" --target macos 2>&1
if [ ! -f "$OUT" ]; then
    echo "[FAIL] Compiler output artifact uretmedi"
    exit 1
fi

echo "  Binary boyutu: $(wc -c < "$OUT") byte"

# Mach-O magic kontrolu: thin 64-bit x86_64/ARM64 ve fat binary desteklenir.
MAGIC=$(xxd -p -l 4 "$OUT" | tr -d '\\n' | tr '[:upper:]' '[:lower:]')
case "$MAGIC" in
  feedfacf|cffaedfe|cafebabe|bebafeca)
    echo "  [OK] Mach-O magic dogrulandi: $MAGIC" ;;
  *)
    echo "  [FAIL] Mach-O magic gecersiz: $MAGIC"
    exit 1 ;;
esac
FILE_INFO=$(file "$OUT")
echo "  file: $FILE_INFO"
echo "$FILE_INFO" | grep -Eqi 'Mach-O|mach-o'

# Calistir (macOS native)
chmod +x "$OUT" 2>/dev/null || true
set +e
OUTPUT=$("$OUT" 2>&1)
EXIT_CODE=$?
set -e
echo "  Cikti: $OUTPUT"
echo "  Exit: $EXIT_CODE"
if [ "$EXIT_CODE" -ne 0 ]; then
    echo "  [FAIL] Mach-O native execution exit=$EXIT_CODE"
    PASSED=false
else
    echo "  [OK] Mach-O native execution basarili"
    PASSED=true
fi
python3 - "$REPORTS/macos_macho_ci_report.json" "$PASSED" "$EXIT_CODE" "$OUTPUT" "$(uname -m)" <<'PY'
import json
import sys
path, passed, exit_code, output, machine = sys.argv[1:]
with open(path, "w", encoding="utf-8") as f:
    json.dump({
        "schema": "yukinary.macos-macho.v2",
        "passed": passed == "true",
        "exit_code": int(exit_code),
        "output": output,
        "runner_machine": machine,
        "native_execution": True,
        "planned_manual_job": True,
    }, f, ensure_ascii=False)
PY
echo "Report: $REPORTS/macos_macho_ci_report.json"
if [ "$PASSED" = true ]; then exit 0; else exit 1; fi
