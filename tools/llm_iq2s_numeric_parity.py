#!/usr/bin/env python3
"""Compare a real IQ2_S GGUF slice against the YKN scalar ABI.

The generated YKN fixture and the independent C++ oracle use the same four
real blocks from blk.0.ffn_down.weight and two deterministic Q16 activation
rows. This is intentionally a narrow scalar-ABI gate, not a full transformer
parity claim.
"""
from __future__ import annotations

import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path


IQ2S_BLOCK_BYTES = 82
K = 256
N = 2
M = 2


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def read_u32(data: bytes, pos: int) -> tuple[int, int]:
    return struct.unpack_from("<I", data, pos)[0], pos + 4


def read_u64(data: bytes, pos: int) -> tuple[int, int]:
    return struct.unpack_from("<Q", data, pos)[0], pos + 8


def read_string(data: bytes, pos: int) -> tuple[str, int]:
    length, pos = read_u64(data, pos)
    end = pos + length
    if end > len(data):
        raise ValueError("GGUF string sınırı aşıldı")
    return data[pos:end].decode("utf-8", "replace"), end


def skip_value(data: bytes, pos: int, value_type: int) -> int:
    scalar_sizes = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1}
    if value_type in scalar_sizes:
        return pos + scalar_sizes[value_type]
    if value_type == 8:
        length, pos = read_u64(data, pos)
        return pos + length
    if value_type == 9:
        element_type, pos = read_u32(data, pos)
        count, pos = read_u64(data, pos)
        for _ in range(count):
            pos = skip_value(data, pos, element_type)
        return pos
    raise ValueError(f"desteklenmeyen GGUF metadata tipi: {value_type}")


def load_real_blocks(model: Path) -> tuple[list[int], list[int], list[int]]:
    data = model.read_bytes()
    if data[:4] != b"GGUF":
        raise ValueError("GGUF magic bulunamadı")
    version, tensor_count, metadata_count = struct.unpack_from("<IQQ", data, 4)
    if version not in (2, 3):
        raise ValueError(f"desteklenmeyen GGUF sürümü: {version}")
    pos = 24
    for _ in range(metadata_count):
        _, pos = read_string(data, pos)
        value_type, pos = read_u32(data, pos)
        pos = skip_value(data, pos, value_type)
    tensors: dict[str, tuple[list[int], int, int]] = {}
    for _ in range(tensor_count):
        name, pos = read_string(data, pos)
        ndim, pos = read_u32(data, pos)
        dims = []
        for _ in range(ndim):
            dim, pos = read_u64(data, pos)
            dims.append(dim)
        ggml_type, pos = read_u32(data, pos)
        offset, pos = read_u64(data, pos)
        tensors[name] = (dims, ggml_type, offset)
    target = tensors.get("blk.0.ffn_down.weight")
    if target is None:
        raise ValueError("blk.0.ffn_down.weight bulunamadı")
    dims, ggml_type, relative_offset = target
    if ggml_type != 22:
        raise ValueError(
            f"This oracle supports upstream GGML IQ2_S type 22, not type {ggml_type}. "
            "Microsoft BitNet I2_S type 36 must be checked with bitnet_i2s_parity.py."
        )
    if len(dims) < 2 or dims[0] < K or dims[1] < N:
        raise ValueError(f"beklenmeyen tensor boyutu: {dims}")
    tensor_data_offset = align_up(pos, 32)
    full_row_bytes = ((int(dims[0]) + 255) // 256) * IQ2S_BLOCK_BYTES
    start = tensor_data_offset + relative_offset
    # Use the first 256 input weights from two real output rows. The YKN
    # fixture intentionally models this bounded slice while preserving the
    # GGUF row stride of the full [K,N] tensor.
    blocks = []
    for column in range(N):
        row_start = start + column * full_row_bytes
        row = data[row_start:row_start + IQ2S_BLOCK_BYTES]
        if len(row) != IQ2S_BLOCK_BYTES:
            raise ValueError("gerçek IQ2_S blokları dosyadan eksik okundu")
        blocks.extend(row)
    row_bytes = full_row_bytes
    # Exercise signed, zero and positive Q16 activation paths without overflow.
    activations = []
    for row in range(M):
        for index in range(K):
            value = ((index * 13 + row * 17) % 9) - 4
            activations.append(value * 65536)
    return blocks, activations, [tensor_data_offset, relative_offset, row_bytes]


def parse_grid(root: Path) -> list[list[int]]:
    text = (root / "std" / "ai" / "iq2s_grid.ykn").read_text(encoding="utf-8")
    alt = [0] * 1024
    upper = [0] * 1024
    pattern = re.compile(
        r"IQ2S_GRID_(ALT|UST)\[(\d+)\]\s*=\s*(\d+)\s*;"
    )
    for kind, index_text, value_text in pattern.findall(text):
        index = int(index_text)
        value = int(value_text)
        if kind == "ALT":
            alt[index] = value
        else:
            upper[index] = value
    if any(value == 0 for value in alt) or any(value == 0 for value in upper):
        raise ValueError("IQ2S grid tablosu eksik")
    grid = []
    for index in range(1024):
        words = (alt[index], upper[index])
        lanes = []
        for word in words:
            lanes.extend((word >> shift) & 0xFF for shift in (0, 8, 16, 24))
        grid.append(lanes)
    return grid


def f16_q16(lo: int, hi: int) -> int:
    bits = lo + hi * 256
    sign = bits // 32768
    exponent = (bits // 1024) % 32
    mantissa = bits % 1024
    if exponent in (0, 31):
        return 0
    result = (1024 + mantissa) * 64
    difference = exponent - 15
    if difference > 0:
        result *= 2 ** difference
    elif difference < 0:
        result //= 2 ** (-difference)
    return -result if sign else result


def grid_value(grid: list[list[int]], index: int, lane: int) -> int:
    return grid[index][lane]


def dequant(block: list[int], element: int, grid: list[list[int]]) -> int:
    group = element // 32
    group_inner = element % 32
    lane_group = group_inner // 8
    lane = group_inner % 8
    q = block[2 + group * 4 + lane_group]
    high = block[66 + group]
    grid_high = (high >> (2 * lane_group)) & 3
    grid_index = q + grid_high * 256
    signs = block[34 + group * 4 + lane_group]
    nibble = (block[74 + group] >> ((group_inner // 16) * 4)) & 15
    scale = (f16_q16(block[0], block[1]) * (1 + 2 * nibble)) // 8
    result = (scale * grid_value(grid, grid_index, lane)) // 32
    if (signs // (2 ** lane)) % 2 == 1:
        result = -result
    return result


def oracle(blocks: list[int], activations: list[int], grid: list[list[int]]) -> list[int]:
    outputs = []
    for row in range(M):
        for column in range(N):
            total = 0
            block = blocks[column * IQ2S_BLOCK_BYTES:(column + 1) * IQ2S_BLOCK_BYTES]
            for index in range(K):
                total += (activations[row * K + index] * dequant(block, index, grid)) // 65536
            outputs.append(total)
    return outputs


def ykn_source(blocks: list[int], activations: list[int]) -> str:
    block_assignments = "\n".join(f"    blok[{i}] = {value};" for i, value in enumerate(blocks))
    activation_assignments = "\n".join(
        f"    aktivasyon[{i}] = {value};" for i, value in enumerate(activations)
    )
    return f'''# IQ2_S generated numeric parity fixture
isv ekrana_yaz(mesaj: *u8) {{}}
isv sayiyi_yazdir(sayi: tamsayi) {{
    degisken buf: [32]u8;
    degisken n: tamsayi = sayi;
    degisken i: tamsayi = 0;
    eger n == 0 ise {{ buf[0] = 48; buf[1] = 0; ekrana_yaz(buf); dondu; }}
    eger n < 0 ise {{ ekrana_yaz("-".ptr); n = 0 - n; }}
    dongu n > 0 {{ buf[i] = (n % 10) + 48; n = n / 10; i = i + 1; }}
    dongu i > 0 {{ i = i - 1; degisken one: [2]u8; one[0] = buf[i]; one[1] = 0; ekrana_yaz(one); }}
}}
ice_aktar "../../../std/ai/iq2s_grid.ykn";
ice_aktar "../../../std/ai/bitnet_iq2s.ykn";
isv ana(arguman_sayisi: tamsayi, argumanlar: **u8) -> tamsayi {{
    degisken blok: [{len(blocks)}]u8;
{block_assignments}
    degisken aktivasyon: [{len(activations)}]tamsayi;
{activation_assignments}
    degisken cikti: [{M * N}]tamsayi;
    degisken kontrol: [6]tamsayi;
    kontrol[0] = 0; kontrol[1] = 31; kontrol[2] = 32; kontrol[3] = 63; kontrol[4] = 127; kontrol[5] = 255;
    degisken ci: tamsayi = 0;
    dongu ci < 6 {{
        degisken ei: tamsayi = kontrol[ci];
        eger bitnet_iq2s_eleman_q16(blok, ei) != bitnet_iq2s_eleman_q16_at(blok, 0, ei) veya
           bitnet_iq2s_eleman_q16(blok + 82, ei) != bitnet_iq2s_eleman_q16_at(blok, 82, ei) ise {{
            ekrana_yaz("PARITY-POINTER-FAIL\\n".ptr); dondu 3;
        }}
        ci = ci + 1;
    }}
    eger bitnet_iq2s_paketli_matris_carp(blok, aktivasyon, cikti, {M}, {K}, {N}) == 0 ise {{
        ekrana_yaz("PARITY-ABI-FAIL\\n".ptr); dondu 2;
    }}
    degisken i: tamsayi = 0;
    dongu i < {M * N} {{ sayiyi_yazdir(cikti[i]); eger i + 1 < {M * N} ise {{ ekrana_yaz(" ".ptr); }} i = i + 1; }}
    ekrana_yaz("\\n".ptr);
    dondu 0;
}}
'''


def cpp_source(blocks: list[int], activations: list[int], grid: list[list[int]]) -> str:
    def values(items: list[int]) -> str:
        return ", ".join(str(item) for item in items)

    grid_flat = [value for row in grid for value in row]
    return f'''#include <cstdint>
#include <iostream>
#include <vector>
static constexpr int K = {K};
static constexpr int N = {N};
static constexpr int M = {M};
static const uint8_t weights[] = {{{values(blocks)}}};
static const int32_t activations[] = {{{values(activations)}}};
static const uint8_t grid[] = {{{values(grid_flat)}}};
static int f16_q16(uint8_t lo, uint8_t hi) {{
    int bits = lo + hi * 256, sign = bits / 32768, exponent = (bits / 1024) % 32, mantissa = bits % 1024;
    if (exponent == 0 || exponent == 31) return 0;
    int result = (1024 + mantissa) * 64, difference = exponent - 15;
    if (difference > 0) result *= (1 << difference);
    else if (difference < 0) result /= (1 << (-difference));
    return sign ? -result : result;
}}
static int64_t dequant(const uint8_t* block, int element) {{
    int group = element / 32, inner = element % 32, lane_group = inner / 8, lane = inner % 8;
    int q = block[2 + group * 4 + lane_group], high = block[66 + group];
    int index = q + ((high * (1 << (8 - 2 * lane_group))) % 1024);
    int signs = block[34 + group * 4 + lane_group];
    int nibble = (block[74 + group] / (1 << (inner / 16))) % 16;
    int64_t scale = (static_cast<int64_t>(f16_q16(block[0], block[1])) * (1 + 2 * nibble)) / 8;
    int64_t result = (scale * grid[index * 8 + lane]) / 32;
    return ((signs / (1 << lane)) % 2) ? -result : result;
}}
int main() {{
    for (int row = 0; row < M; ++row) {{
        for (int column = 0; column < N; ++column) {{
            int64_t total = 0;
            const uint8_t* block = weights + column * 82;
            for (int index = 0; index < K; ++index)
                total += (static_cast<int64_t>(activations[row * K + index]) * dequant(block, index)) / 65536;
            std::cout << total << ((row == M - 1 && column == N - 1) ? '\\n' : ' ');
        }}
    }}
}}
'''


def run_checked(command: list[str], cwd: Path, label: str) -> str:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        raise RuntimeError(f"{label} başarısız (exit={result.returncode})\n{result.stdout}\n{result.stderr}")
    return result.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, default=None)
    parser.add_argument("--compiler", type=Path, default=None)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    model = args.model or root / "models" / "bitnet-b1.58-2B-4T" / "ggml-model-i2_s.gguf"
    compiler = args.compiler or root / "build" / "bootstrap" / "stage3.exe"
    work = root / "build" / "test-work" / "iq2s-numeric-parity"
    work.mkdir(parents=True, exist_ok=True)
    blocks, activations, offsets = load_real_blocks(model)
    grid = parse_grid(root)
    expected = oracle(blocks, activations, grid)
    ykn_path = work / "real_iq2s_parity.ykn"
    cpp_path = work / "real_iq2s_oracle.cpp"
    cpp_exe = work / "real_iq2s_oracle.exe"
    ykn_exe = work / "real_iq2s_parity.exe"
    ykn_path.write_text(ykn_source(blocks, activations), encoding="utf-8")
    cpp_path.write_text(cpp_source(blocks, activations, grid), encoding="utf-8")
    if sys.platform == "win32":
        vs_dev_cmd = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat")
        if not vs_dev_cmd.is_file():
            raise RuntimeError(f"MSVC VsDevCmd bulunamadı: {vs_dev_cmd}")
        compile_script = work / "compile_iq2s_oracle.cmd"
        compile_script.write_text(
            f'@call "{vs_dev_cmd}" -arch=x64 -host_arch=x64 >nul\n'
            '@if errorlevel 1 exit /b %errorlevel%\n'
            f'@cl.exe /nologo /std:c++17 /O2 /EHsc "{cpp_path}" /Fe:"{cpp_exe}"\n',
            encoding="utf-8",
        )
        compiled = subprocess.run(
            ["cmd.exe", "/d", "/s", "/c", str(compile_script)],
            cwd=root, capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        if compiled.returncode != 0:
            raise RuntimeError(
                f"MSVC IQ2_S oracle derlemesi başarısız (exit={compiled.returncode})\n"
                f"{compiled.stdout}\n{compiled.stderr}"
            )
    else:
        run_checked(["g++", "-std=c++17", "-O2", str(cpp_path), "-o", str(cpp_exe)], root, "C++ IQ2_S oracle derlemesi")
    cpp_output = run_checked([str(cpp_exe)], root, "C++ IQ2_S oracle")
    run_checked([str(compiler), str(ykn_path), str(ykn_exe)], root, "YKN IQ2_S parity fixture derlemesi")
    ykn_output = run_checked([str(ykn_exe)], root, "YKN IQ2_S parity fixture")
    expected_text = " ".join(str(value) for value in expected)
    print(f"tensor_data_offset={offsets[0]} relative_tensor_offset={offsets[1]} row_bytes={offsets[2]}")
    print(f"expected={expected_text}")
    print(f"cpp={cpp_output}")
    print(f"ykn={ykn_output}")
    if cpp_output != expected_text or ykn_output != expected_text:
        print("IQ2S NUMERIC PARITY FAIL", file=sys.stderr)
        return 1
    print("IQ2S NUMERIC PARITY PASS: gerçek GGUF blokları C++ oracle ve YKN scalar ABI ile eşleşti.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
