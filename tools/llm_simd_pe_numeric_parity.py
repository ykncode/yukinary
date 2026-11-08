#!/usr/bin/env python3
"""Validate the native IQ2_S kernel through a generated PE using real GGUF data.

The direct ctypes gate is useful on hosts that can load the MinGW DLL. This
companion gate deliberately exercises the production path instead: YKN arrays
use their native 8-byte `tamsayi` layout, the generated PE imports
`ykn_llm_simd.dll`, and the four outputs are compared with the independent
Python IQ2_S oracle.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from llm_iq2s_numeric_parity import (
    IQ2S_BLOCK_BYTES,
    K,
    M,
    N,
    load_real_blocks,
    oracle,
    parse_grid,
)


def ykn_number_printer() -> str:
    return r'''
isv sayiyi_yazdir(sayi: tamsayi) {
    degisken buf: [48]u8;
    degisken n: tamsayi = sayi;
    degisken i: tamsayi = 0;
    eger n == 0 ise { buf[0] = 48; buf[1] = 0; ekrana_yaz(buf); dondu; }
    eger n < 0 ise { ekrana_yaz("-".ptr); n = 0 - n; }
    dongu n > 0 { buf[i] = (n % 10) + 48; n = n / 10; i = i + 1; }
    dongu i > 0 { i = i - 1; degisken one: [2]u8; one[0] = buf[i]; one[1] = 0; ekrana_yaz(one); }
}
'''


def ykn_source(blocks: list[int], activations: list[int], stride: int) -> str:
    # Only the first two bounded rows are materialized. The second block ends
    # at `stride + 82`; reserving a full stride for every column would needlessly
    # push the generated function over the current fixed stack frame.

    packed_size = stride + IQ2S_BLOCK_BYTES

    def row_function(row: int) -> str:
        block_assignments: list[str] = []
        for column in range(N):
            source_start = column * IQ2S_BLOCK_BYTES
            target_start = column * stride
            for index in range(IQ2S_BLOCK_BYTES):
                block_assignments.append(
                    f"    packed[{target_start + index}] = {blocks[source_start + index]};"
                )
        activation_assignments = [
            f"    activation[{index}] = {activations[row * K + index]};"
            for index in range(K)
        ]
        return """isv iq2s_satir_{row}() -> tamsayi {{
    # Keep each native row call below the current 8 KiB YKN frame limit.
    degisken packed: [{packed_size}]u8;
    degisken activation: [256]tamsayi;
{blocks}
{activations}
    degisken left: tamsayi = ykn_llm_iq2s_dot_q16(packed, activation, 256, 0);
    degisken right: tamsayi = ykn_llm_iq2s_dot_q16(packed, activation, 256, {offset});
    sayiyi_yazdir(left);
    ekrana_yaz(" ".ptr);
    sayiyi_yazdir(right);
    dondu 0;
}}
""".format(
            row=row,
            packed_size=packed_size,
            blocks="\n".join(block_assignments),
            activations="\n".join(activation_assignments),
            offset=stride,
        )

    rows = "\n".join(row_function(row) for row in range(M))
    return """# Real GGUF -> native IQ2_S generated PE parity fixture
harici isv ykn_llm_iq2s_dot_q16(packed: *u8, activation: *tamsayi, k: tamsayi, row_offset: tamsayi) -> tamsayi;

{printer}
{rows}

isv ana(arguman_sayisi: tamsayi, argumanlar: **u8) -> tamsayi {{
    ekrana_yaz("RESULTS=".ptr);
    iq2s_satir_0();
    ekrana_yaz(" ".ptr);
    iq2s_satir_1();
    ekrana_yaz("\\n".ptr);
    dondu 0;
}}
""".format(
        printer=ykn_number_printer(),
        rows=rows,
    )


def run_checked(command: list[str], cwd: Path, label: str) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{label} başarısız (exit={result.returncode})\n"
            + result.stdout
            + result.stderr
        )
    return result.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, default=None)
    parser.add_argument("--compiler", type=Path, default=None)
    parser.add_argument("--avx2", action="store_true")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    model = args.model or root / "models" / "bitnet-b1.58-2B-4T" / "ggml-model-i2_s.gguf"
    compiler = args.compiler or root / "build" / "bootstrap" / "stage0.exe"
    work = root / "build" / "test-work" / "iq2s-simd-pe-numeric"
    work.mkdir(parents=True, exist_ok=True)

    blocks, activations, offsets = load_real_blocks(model)
    expected = oracle(blocks, activations, parse_grid(root))
    stride = offsets[2]
    source = work / "real_iq2s_simd_pe.ykn"
    executable = work / "real_iq2s_simd_pe.exe"
    dll = work / "ykn_llm_simd.dll"
    source.write_text(ykn_source(blocks, activations, stride), encoding="utf-8")

    grid_generator = root / "tools" / "generate_iq2s_grid_inc.py"
    run_checked([sys.executable, str(grid_generator)], root, "IQ2_S grid include üretimi")
    cxx_args = ["g++", "-std=c++17", "-O2", "-shared"]
    if args.avx2:
        cxx_args.append("-mavx2")
    cxx_args.extend([str(root / "libykn" / "llm_simd.cpp"), "-o", str(dll)])
    run_checked(cxx_args, root, "native IQ2_S DLL derlemesi")
    run_checked([str(compiler), str(source), str(executable)], root, "generated PE derlemesi")
    actual_text = run_checked([str(executable)], root, "generated PE çalıştırması")
    prefix = "RESULTS="
    if not actual_text.startswith(prefix):
        raise RuntimeError(f"beklenmeyen PE çıktısı: {actual_text!r}")
    actual = [int(value) for value in actual_text[len(prefix):].split()]
    if actual != expected:
        raise RuntimeError(f"native PE IQ2_S parity başarısız: expected={expected} actual={actual}")

    backend = "avx2" if args.avx2 else "portable"
    print(f"backend={backend} row_bytes={stride} expected={expected} actual={actual}")
    print("LLM SIMD PE NUMERIC PARITY PASS: gerçek GGUF IQ2_S slice native generated PE ABI ile eşleşti.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"LLM SIMD PE NUMERIC PARITY FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
