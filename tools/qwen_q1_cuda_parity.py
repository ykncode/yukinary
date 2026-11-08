#!/usr/bin/env python3
"""Compare the real Bonsai Q1_0 weight row on YKN CPU and CUDA kernels."""
from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from bitnet_i2s_parity import find_tensor


def trunc_div(numerator: int, denominator: int) -> int:
    return numerator // denominator if numerator >= 0 else -((-numerator) // denominator)


def half_to_q16(lo: int, hi: int) -> int:
    bits = lo | (hi << 8)
    sign = bits >> 15
    exponent = (bits >> 10) & 31
    mantissa = bits & 1023
    if exponent == 0:
        value = mantissa >> 8
    elif exponent == 31:
        value = 0
    else:
        value = (1024 + mantissa) << 6
        shift = exponent - 15
        if shift > 0:
            value <<= shift
        elif shift < 0:
            value >>= -shift
    return -value if sign else value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path)
    parser.add_argument("--compiler", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    model = args.model or root / "models/bonsai-8b/Bonsai-8B.gguf"
    compiler = args.compiler or root / "build/cuda-stage0-rebuilt.exe"
    tensor_name = "blk.0.attn_q.weight"
    with model.open("rb") as stream:
        data_start, (dims, tensor_type, offset) = find_tensor(stream, tensor_name)
        if tensor_type != 41 or len(dims) < 2 or dims[0] % 128:
            raise ValueError(f"expected Q1_0 tensor, got type={tensor_type}, dims={dims}")
        K = dims[0]
        row_bytes = (K // 128) * 18
        stream.seek(data_start + offset)
        weights = list(stream.read(row_bytes))
    if len(weights) != row_bytes:
        raise ValueError("short read from real Q1_0 tensor row")
    activation = [(((i * 13) % 9) - 4) * 65536 for i in range(K)]
    expected = 0
    for i, x in enumerate(activation):
        block = (i // 128) * 18
        weight = half_to_q16(weights[block], weights[block + 1])
        bits = weights[block + 2 + (i % 128) // 8]
        sign = (bits >> (i % 8)) & 1
        weight = weight if sign else -weight
        expected += trunc_div(x * weight, 65536)

    weight_init = "\n".join(f"    weights[{i}] = {v};" for i, v in enumerate(weights))
    activation_init = "\n".join(f"    activation[{i}] = {v};" for i, v in enumerate(activation))
    source_text = f'''ice_aktar "../../../std/ai/bitnet_backend.ykn";
ice_aktar "../../../std/ai/cuda_backend.ykn";
harici isv ekrana_yaz(mesaj: *u8);
isv print_int(value: tamsayi) {{
    degisken digits: [32]u8; degisken n: tamsayi = value; degisken count: tamsayi = 0;
    eger n == 0 {{ ekrana_yaz("0".ptr); dondu; }}
    eger n < 0 {{ ekrana_yaz("-".ptr); n = 0 - n; }}
    dongu n > 0 {{ digits[count] = (n % 10) + 48; count = count + 1; n = n / 10; }}
    dongu count > 0 {{ count = count - 1; degisken c: [2]u8; c[0] = digits[count]; c[1] = 0; ekrana_yaz(c); }}
}}
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {{
    degisken weights: [{row_bytes}]u8;
{weight_init}
    degisken activation: [{K}]tamsayi;
{activation_init}
    degisken cpu: [1]tamsayi; degisken gpu: [1]tamsayi;
    eger llm_q1_0_matris_carp(weights, activation, cpu, 1, {K}, 1) != 1 {{ dondu 2; }}
    eger ykn_cuda_backend_start() != 1 {{ ekrana_yaz("CUDA_START_FAIL\\n".ptr); dondu 3; }}
    eger ykn_cuda_q1_matvec(weights, activation, gpu, {K}, 1, {row_bytes}) != 1 {{ ekrana_yaz("CUDA_Q1_FAIL\\n".ptr); dondu 4; }}
    ekrana_yaz("CPU:"); print_int(cpu[0]); ekrana_yaz("\\nGPU:"); print_int(gpu[0]); ekrana_yaz("\\n");
    ykn_cuda_backend_stop(); dondu 0;
}}
'''
    work = root / "build/test-work/qwen-q1-cuda-parity"
    work.mkdir(parents=True, exist_ok=True)
    source = work / "real_q1_cuda_parity.ykn"
    binary = work / "real_q1_cuda_parity.exe"
    source.write_text(source_text, encoding="utf-8")
    built = subprocess.run([str(compiler), str(source), str(binary)], cwd=root, capture_output=True, text=True)
    if built.returncode:
        raise RuntimeError(f"YKN compile failed ({built.returncode})\n{built.stdout}\n{built.stderr}")
    ran = subprocess.run([str(binary)], cwd=root, capture_output=True, text=True)
    if ran.returncode:
        raise RuntimeError(f"CUDA parity fixture failed ({ran.returncode})\n{ran.stdout}\n{ran.stderr}")
    values = {}
    for line in ran.stdout.splitlines():
        if line.startswith("CPU:"):
            values["cpu"] = int(line[4:])
        elif line.startswith("GPU:"):
            values["gpu"] = int(line[4:])
    print(f"tensor={tensor_name} dims={dims} type={tensor_type} row_bytes={row_bytes}")
    print(f"python_expected={expected} ykn_cpu={values.get('cpu')} ykn_cuda={values.get('gpu')}")
    if values.get("cpu") != expected or values.get("gpu") != expected:
        raise SystemExit("QWEN REAL Q1_0 CPU/CUDA PARITY FAIL")
    print("QWEN REAL Q1_0 CPU/CUDA PARITY PASS")


if __name__ == "__main__":
    main()
