#!/usr/bin/env bash
# ============================================================
# test_linux_elf_execution.sh
# Linux ELF64 CI: YKN derleyicisini derleyip ELF64 binary
# uretir ve gercek stdout/exit code dogrular.
# ============================================================
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORK="$ROOT/build/test-work/linux-elf-ci"
mkdir -p "$WORK"
REPORTS="$ROOT/build/reports"
mkdir -p "$REPORTS"

echo "=== Linux ELF64 Execution CI ==="

# g++ ile YKN derleyicisini derle
echo "Derleyici derleniyor (g++ -O2)..."
if ! g++ -O2 -std=c++17 -o "$WORK/stage0_linux" "$ROOT/app_full.cpp"; then
    echo "[FAIL] g++ build basarisiz"
    printf '%s\n' '{"schema":"yukinary.linux-elf.v1","passed":false,"reason":"compiler_build_failed"}' > "$REPORTS/linux_elf_ci_report.json"
    exit 1
fi

COMPILER="$WORK/stage0_linux"

# Hello world fixture derle (ELF64 hedefi)
SRC="$ROOT/tests/bootstrap/01_hello.ykn"
if [ ! -f "$SRC" ]; then
    echo "[FAIL] Test kaynagi bulunamadi: $SRC"
    printf '%s\n' '{"schema":"yukinary.linux-elf.v1","passed":false,"reason":"fixture_missing"}' > "$REPORTS/linux_elf_ci_report.json"
    exit 1
fi

OUT="$WORK/hello_linux.elf"
echo "ELF64 binary üretiliyor..."
if ! "$COMPILER" --target linux "$SRC" "$OUT" 2>&1; then
    echo "[FAIL] ELF64 derleme basarisiz"
    exit 1
fi
if [ ! -f "$OUT" ]; then
    echo "[FAIL] Compiler output artifact uretmedi"
    exit 1
fi

echo "  Binary boyutu: $(wc -c < "$OUT") byte"

# ELF magic kontrolu
MAGIC=$(xxd -p -l 4 "$OUT" | tr -d '\\n' | tr '[:upper:]' '[:lower:]')
if [ "$MAGIC" = "7f454c46" ]; then
    echo "  [OK] ELF magic gecerli"
else
    echo "  [FAIL] ELF magic gecersiz: $MAGIC"
    exit 1
fi

# Binary calistir (Linux native)
chmod +x "$OUT"
OUTPUT=$("$OUT" 2>&1)
EXIT_CODE=$?
echo "  Cikti: $OUTPUT"
echo "  Exit: $EXIT_CODE"

if [ "$EXIT_CODE" -ne 0 ]; then
    echo "  [FAIL] ELF64 native execution exit=$EXIT_CODE"
    PASSED=false
else
    echo "  [OK] Gercek ELF64 calistirma basarili"
    PASSED=true
fi

python3 - "$REPORTS/linux_elf_ci_report.json" "$PASSED" "$EXIT_CODE" "$OUTPUT" <<'PY'
import json
import sys
path, passed, exit_code, output = sys.argv[1:]
with open(path, "w", encoding="utf-8") as f:
    json.dump({
        "schema": "yukinary.linux-elf.v1",
        "passed": passed == "true",
        "exit_code": int(exit_code),
        "output": output,
        "native_execution": True,
    }, f, ensure_ascii=False)
PY

echo "Report: $REPORTS/linux_elf_ci_report.json"
if [ "$PASSED" = true ]; then exit 0; else exit 1; fi
