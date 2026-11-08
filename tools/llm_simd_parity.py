#!/usr/bin/env python3
"""Validate the optional native LLM SIMD kernels against integer references.

This is intentionally separate from the PE linker gate: it proves that the
native DLL/object has the same Q16 contract as the scalar reference before the
compiler starts importing it into generated binaries.
"""
from __future__ import annotations

import argparse
import ctypes
import platform
import subprocess
import sys
from pathlib import Path

from llm_iq2s_numeric_parity import IQ2S_BLOCK_BYTES, K, M, N, load_real_blocks, oracle, parse_grid


def trunc_zero(value: int, divisor: int) -> int:
    return value // divisor if value >= 0 else -((-value) // divisor)


def scalar_dot(a: list[int], b: list[int]) -> int:
    return trunc_zero(sum(x * y for x, y in zip(a, b)), 65536)


def scalar_silu(value: int) -> int:
    ax = abs(value)
    denominator = 65536 + ax
    sigmoid = 32768 + trunc_zero(value * 65536, 2 * denominator)
    result = trunc_zero(value * sigmoid, 65536)
    return max(-(1 << 31), min((1 << 31) - 1, result))


def run(args: argparse.Namespace) -> int:
    root = Path(__file__).resolve().parents[1]
    source = root / "libykn" / "llm_simd.cpp"
    work = root / "build" / "test-work" / "llm-simd-parity"
    work.mkdir(parents=True, exist_ok=True)
    if platform.system().lower().startswith("win"):
        native = work / "ykn_llm_simd.dll"
        command = ["g++", "-std=c++17", "-O2", "-shared", str(source), "-o", str(native)]
    else:
        native = work / "libykn_llm_simd.so"
        command = ["g++", "-std=c++17", "-O2", "-fPIC", "-shared", str(source), "-o", str(native)]
    if args.avx2:
        command.insert(1, "-mavx2")
    generated = subprocess.run(
        [sys.executable, str(root / "tools" / "generate_iq2s_grid_inc.py")],
        cwd=root, capture_output=True, text=True,
    )
    if generated.returncode != 0:
        raise RuntimeError("IQ2_S grid include üretimi başarısız:\n" + generated.stdout + generated.stderr)
    result = subprocess.run(command, cwd=root, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("SIMD native derlemesi başarısız:\n" + result.stdout + result.stderr)

    lib = ctypes.CDLL(str(native))
    lib.ykn_llm_simd_backend.restype = ctypes.c_int
    # The YKN ABI stores every `tamsayi` array element in 8 bytes.
    lib.ykn_llm_dot_q16.argtypes = [ctypes.POINTER(ctypes.c_int64), ctypes.POINTER(ctypes.c_int64), ctypes.c_size_t]
    lib.ykn_llm_dot_q16.restype = ctypes.c_int64
    lib.ykn_llm_silu_q16.argtypes = [ctypes.POINTER(ctypes.c_int64), ctypes.POINTER(ctypes.c_int64), ctypes.c_size_t]
    lib.ykn_llm_silu_q16.restype = None
    lib.ykn_llm_iq2s_dot_q16.argtypes = [
        ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_int64),
        ctypes.c_size_t, ctypes.c_size_t,
    ]
    lib.ykn_llm_iq2s_dot_q16.restype = ctypes.c_int64

    # Odd and non-vector-aligned length exercises the SIMD tail path too.
    values_a = [((i * 7919) % 131071) - 65535 for i in range(args.length)]
    values_b = [((i * 3571 + 17) % 131071) - 65535 for i in range(args.length)]
    array_a = (ctypes.c_int64 * args.length)(*values_a)
    array_b = (ctypes.c_int64 * args.length)(*values_b)
    actual_dot = int(lib.ykn_llm_dot_q16(array_a, array_b, args.length))
    expected_dot = scalar_dot(values_a, values_b)
    if actual_dot != expected_dot:
        raise RuntimeError(f"dot parity başarısız: expected={expected_dot} actual={actual_dot}")

    silu_input_values = [-262144, -131072, -65536, -1, 0, 1, 65536, 131072, 262144, 2147483647]
    silu_input = (ctypes.c_int64 * len(silu_input_values))(*silu_input_values)
    silu_output = (ctypes.c_int64 * len(silu_input_values))()
    lib.ykn_llm_silu_q16(silu_input, silu_output, len(silu_input_values))
    actual_silu = list(silu_output)
    expected_silu = [scalar_silu(value) for value in silu_input_values]
    if actual_silu != expected_silu:
        raise RuntimeError(f"SiLU parity başarısız: expected={expected_silu} actual={actual_silu}")

    # Real GGUF IQ2_S fused row parity uses the same bounded slice as the
    # scalar ABI test, including the tensor row stride in row_offset.
    model = args.model or root / "models" / "bitnet-b1.58-2B-4T" / "ggml-model-i2_s.gguf"
    blocks, activations, offsets = load_real_blocks(model)
    expected_iq2s = oracle(blocks, activations, parse_grid(root))
    # Materialize the two bounded rows at the real GGUF row stride so the
    # row_offset ABI is tested as it is used by the transformer loader.
    stride = offsets[2]
    strided_blocks = bytearray(stride * N)
    for column in range(N):
        start = column * IQ2S_BLOCK_BYTES
        strided_blocks[column * stride:column * stride + IQ2S_BLOCK_BYTES] = blocks[start:start + IQ2S_BLOCK_BYTES]
    block_array = (ctypes.c_uint8 * len(strided_blocks))(*strided_blocks)
    activation_array = (ctypes.c_int64 * len(activations))(*activations)
    actual_iq2s = []
    for row in range(M):
        for column in range(N):
            actual_iq2s.append(int(lib.ykn_llm_iq2s_dot_q16(
                block_array, ctypes.cast(ctypes.byref(activation_array, row * K * 8), ctypes.POINTER(ctypes.c_int64)),
                K, column * stride,
            )))
    if actual_iq2s != expected_iq2s:
        raise RuntimeError(f"IQ2_S fused parity başarısız: expected={expected_iq2s} actual={actual_iq2s}")

    backend = int(lib.ykn_llm_simd_backend())
    if backend not in (0, 1, 2, 3):
        raise RuntimeError(f"geçersiz native backend kodu: {backend}")
    print(f"backend={backend} dot_length={args.length} silu_cases={len(silu_input_values)} iq2s={actual_iq2s}")
    print("LLM SIMD PARITY PASS: native dot/SiLU/IQ2_S Q16 sözleşmesi scalar oracle ile eşleşti.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, default=None)
    parser.add_argument("--length", type=int, default=257)
    parser.add_argument("--avx2", action="store_true", help="AVX2 kernelini derle ve çalıştır")
    args = parser.parse_args()
    if args.length < 1:
        parser.error("--length pozitif olmalı")
    try:
        return run(args)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"LLM SIMD PARITY FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
