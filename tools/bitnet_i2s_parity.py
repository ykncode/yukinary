#!/usr/bin/env python3
"""Compare YKN's Microsoft I2_S kernel with a separate Python oracle on real GGUF rows."""
from __future__ import annotations
import argparse
import struct
import subprocess
from pathlib import Path

SCALAR_SIZE = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}

def read_string(f):
    n, = struct.unpack('<Q', f.read(8))
    data = f.read(n)
    if len(data) != n:
        raise ValueError('short GGUF string')
    return data.decode('utf-8', 'replace')

def skip_value(f, typ):
    if typ in SCALAR_SIZE:
        f.seek(SCALAR_SIZE[typ], 1)
    elif typ == 8:
        f.seek(struct.unpack('<Q', f.read(8))[0], 1)
    elif typ == 9:
        subtype, = struct.unpack('<I', f.read(4))
        count, = struct.unpack('<Q', f.read(8))
        if subtype == 8:
            for _ in range(count):
                f.seek(struct.unpack('<Q', f.read(8))[0], 1)
        elif subtype in SCALAR_SIZE:
            f.seek(count * SCALAR_SIZE[subtype], 1)
        else:
            raise ValueError(f'unsupported GGUF array subtype {subtype}')
    else:
        raise ValueError(f'unsupported GGUF metadata type {typ}')

def find_tensor(f, target):
    if f.read(4) != b'GGUF':
        raise ValueError('not a GGUF file')
    version, tensor_count, metadata_count = struct.unpack('<IQQ', f.read(20))
    if version not in (2, 3):
        raise ValueError(f'unsupported GGUF version {version}')
    for _ in range(metadata_count):
        read_string(f)
        typ, = struct.unpack('<I', f.read(4))
        skip_value(f, typ)
    found = None
    for _ in range(tensor_count):
        name = read_string(f)
        ndim, = struct.unpack('<I', f.read(4))
        dims = list(struct.unpack('<' + 'Q' * ndim, f.read(8 * ndim)))
        typ, = struct.unpack('<I', f.read(4))
        offset, = struct.unpack('<Q', f.read(8))
        if name == target:
            found = (dims, typ, offset)
    data_start = (f.tell() + 31) & ~31
    if found is None:
        raise ValueError(f'missing tensor {target}')
    return data_start, found

def q16_scale(scale_bytes):
    bits, = struct.unpack('<I', scale_bytes[:4])
    sign = bits >> 31
    exponent = (bits >> 23) & 0xff
    mantissa = bits & 0x7fffff
    if exponent in (0, 255):
        return 0
    value = (0x800000 + mantissa) * (2 ** (exponent - 134))
    value = int(value)
    return -value if sign else value

def decode_code(data, element):
    group = (element % 128) // 32
    byte_index = (element // 128) * 32 + element % 32
    raw = (data[byte_index] >> (6 - 2 * group)) & 3
    if raw == 0:
        return -1
    if raw == 1:
        return 0
    if raw == 2:
        return 1
    raise ValueError(f'invalid ternary code at {element}')

def make_source(weights, activations, result_count, use_cuda=False):
    w = '\n'.join(f'    weights[{i}] = {v};' for i, v in enumerate(weights))
    x = '\n'.join(f'    activation[{i}] = {v};' for i, v in enumerate(activations))
    return f'''isv ekrana_yaz(mesaj: *u8) {{}}
ice_aktar "../../../std/ai/bitnet_backend.ykn";
{'ice_aktar "../../../std/ai/cuda_backend.ykn";' if use_cuda else ''}
isv print_int(value: tamsayi) {{
    degisken digits: [32]u8; degisken n: tamsayi = value; degisken count: tamsayi = 0;
    eger n == 0 {{ ekrana_yaz("0".ptr); dondu; }}
    eger n < 0 {{ ekrana_yaz("-".ptr); n = 0 - n; }}
    dongu n > 0 {{ digits[count] = (n % 10) + 48; count = count + 1; n = n / 10; }}
    dongu count > 0 {{ count = count - 1; degisken c: [2]u8; c[0] = digits[count]; c[1] = 0; ekrana_yaz(c); }}
}}
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {{
    degisken weights: [{len(weights)}]u8;
{w}
    degisken activation: [{len(activations)}]tamsayi;
{x}
    degisken output: [{result_count}]tamsayi;
    eger bitnet_i2s_paketli_matris_carp(weights, activation, output, 2, 128, 2) != 1 {{ dondu 2; }}
    ekrana_yaz("CPU:");
    degisken i: tamsayi = 0;
    dongu i < {result_count} {{ eger i > 0 {{ ekrana_yaz(" ".ptr); }} print_int(output[i]); i = i + 1; }}
    ekrana_yaz("\\n".ptr);
    {'degisken gpu_output: [2]tamsayi; if ykn_cuda_backend_start() != 1 { ekrana_yaz("CUDA_START_FAIL\\n".ptr); dondu 3; } if ykn_cuda_i2s_matvec(weights, activation, gpu_output, 128, 2, ' + str(len(weights)) + ') != 1 { ekrana_yaz("CUDA_I2S_FAIL\\n".ptr); dondu 4; } ekrana_yaz("GPU:"); i = 0; dongu i < 2 { eger i > 0 { ekrana_yaz(" ".ptr); } print_int(gpu_output[i]); i = i + 1; } ekrana_yaz("\\n".ptr); ykn_cuda_backend_stop();' if use_cuda else ''}
    dondu 0;
}}
'''

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', type=Path)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--cuda', action='store_true', help='compare the real GGUF row with CUDA on the selected GPU')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    model = args.model or root / 'models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf'
    compiler = args.compiler or root / 'buildagent-stage0.exe'
    target_name = 'blk.0.ffn_down.weight'
    K, N, M = 128, 2, 2
    with model.open('rb') as f:
        data_start, (dims, typ, relative_offset) = find_tensor(f, target_name)
        if typ != 36 or len(dims) < 2 or dims[0] % 128 or dims[0] < K or dims[1] < N:
            raise ValueError(f'unexpected I2_S tensor layout: type={typ}, dims={dims}')
        total = dims[0] * dims[1]
        byte_count = total // 4 + 32
        row_bytes = dims[0] // 4
        tensor_start = data_start + relative_offset
        f.seek(tensor_start)
        rows = [f.read(K // 4) for _ in range(N)]
        f.seek(tensor_start + total // 4)
        trailer = f.read(32)
        if any(len(row) != K // 4 for row in rows) or len(trailer) != 32:
            raise ValueError('short read in verified GGUF I2_S tensor')
    scale = q16_scale(trailer)
    weights = [b for row in rows for b in row] + list(trailer)
    activations = [((((i * 13 + r * 17) % 9) - 4) * 65536) for r in range(M) for i in range(K)]
    expected = []
    expected_cpu_i8s = []
    expected_cuda = []
    for r in range(M):
        row_activations = activations[r * K:(r + 1) * K]
        amax = max((abs(value) for value in row_activations), default=0)
        quantized_dequantized = []
        for value in row_activations:
            magnitude = abs(value)
            q = (magnitude * 127 + amax // 2) // amax if amax else 0
            q = min(q, 127)
            effective = (q * amax + 63) // 127 if amax else 0
            quantized_dequantized.append(-effective if value < 0 else effective)
        for c in range(N):
            total_q16 = 0
            total_cpu_i8s = 0
            total_cuda_q32 = 0
            row = rows[c]
            for k in range(K):
                code = decode_code(row, k)
                product = row_activations[k] * scale
                scaled = abs(product) // 65536
                if product < 0:
                    scaled = -scaled
                total_q16 += scaled * code
                quantized_product = quantized_dequantized[k] * scale
                quantized_scaled = abs(quantized_product) // 65536
                if quantized_product < 0:
                    quantized_scaled = -quantized_scaled
                total_cpu_i8s += quantized_scaled * code
                total_cuda_q32 += quantized_product * code
            expected.append(total_q16)
            expected_cpu_i8s.append(total_cpu_i8s)
            if total_cuda_q32 < 0:
                expected_cuda.append(-((-total_cuda_q32) // 65536))
            else:
                expected_cuda.append(total_cuda_q32 // 65536)
    work = root / 'build/test-work/bitnet-i2s-parity'
    work.mkdir(parents=True, exist_ok=True)
    source = work / 'real_i2s_parity.ykn'
    binary = work / 'real_i2s_parity.exe'
    source.write_text(make_source(weights, activations, M * N, args.cuda), encoding='utf-8')
    compile_result = subprocess.run([str(compiler), str(source), str(binary)], cwd=root, capture_output=True, text=True)
    if compile_result.returncode:
        raise RuntimeError(f'YKN compile failed ({compile_result.returncode})\n{compile_result.stdout}\n{compile_result.stderr}')
    run_result = subprocess.run([str(binary)], cwd=root, capture_output=True, text=True)
    if run_result.returncode:
        raise RuntimeError(f'YKN fixture failed ({run_result.returncode})\n{run_result.stdout}\n{run_result.stderr}')
    lines = run_result.stdout.splitlines()
    cpu_line = next((line for line in lines if line.startswith('CPU:')), None)
    gpu_line = next((line for line in lines if line.startswith('GPU:')), None)
    if cpu_line is None or (args.cuda and gpu_line is None):
        raise RuntimeError(f'parity fixture omitted a result marker: {run_result.stdout!r}')
    actual = [int(x) for x in cpu_line.removeprefix('CPU:').split()]
    gpu_actual = [int(x) for x in gpu_line.removeprefix('GPU:').split()] if gpu_line else None
    print(f'tensor={target_name} dimensions={dims} type={typ} data_offset={tensor_start} row_bytes={row_bytes} payload_bytes={byte_count}')
    print(f'scale_q16={scale}')
    print('expected_raw_q16=' + ' '.join(map(str, expected)))
    print('expected_i8s=' + ' '.join(map(str, expected_cpu_i8s)))
    print('ykn_cpu=' + ' '.join(map(str, actual)))
    if gpu_actual is not None:
        print('expected_cuda=' + ' '.join(map(str, expected_cuda[:2])))
        print('ykn_cuda=' + ' '.join(map(str, gpu_actual)))
        if gpu_actual != expected_cuda[:2]:
            raise SystemExit('BITNET I2_S QUANTIZED CUDA PARITY FAIL')
        print('BITNET I2_S QUANTIZED CUDA PARITY PASS')
    if actual != expected_cpu_i8s:
        raise SystemExit('BITNET I2_S QUANTIZED CPU PARITY FAIL')
    print('BITNET I2_S QUANTIZED CPU PARITY PASS')

if __name__ == '__main__':
    main()
