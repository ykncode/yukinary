#!/usr/bin/env python3
"""Small dependency-free GGUF metadata/tensor inspector for parity fixtures."""
from __future__ import annotations
import argparse
import struct
from pathlib import Path

SCALARS = {
    0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2),
    4: ("I", 4), 5: ("i", 4), 6: ("f", 4), 7: ("?", 1),
    10: ("Q", 8), 11: ("q", 8), 12: ("d", 8),
}

def read(path: Path):
    data = path.read_bytes()
    pos = 0
    def take(n: int) -> bytes:
        nonlocal pos
        if n < 0 or pos + n > len(data):
            raise ValueError(f"truncated GGUF at {pos}, need {n} bytes")
        out = data[pos:pos+n]
        pos += n
        return out
    def u32() -> int: return struct.unpack("<I", take(4))[0]
    def u64() -> int: return struct.unpack("<Q", take(8))[0]
    def string() -> str: return take(u64()).decode("utf-8", "replace")
    def value(kind: int):
        if kind == 8:
            return string()
        if kind == 9:
            elem_kind = u32()
            return [value(elem_kind) for _ in range(u64())]
        if kind in SCALARS:
            fmt, _ = SCALARS[kind]
            return struct.unpack("<" + fmt, take(struct.calcsize(fmt)))[0]
        raise ValueError(f"unsupported GGUF value type {kind}")

    if take(4) != b"GGUF":
        raise ValueError("not a GGUF file")
    version = u32(); tensor_count = u64(); meta_count = u64()
    meta = {}
    for index in range(meta_count):
        key = string()
        kind = u32()
        try:
            meta[key] = value(kind)
        except Exception as exc:
            raise ValueError(f"metadata[{index}] key={key!r} kind={kind} at {pos}: {exc}") from exc
    infos = []
    for _ in range(tensor_count):
        name = string(); ndim = u32()
        dims = tuple(u64() for _ in range(ndim))
        kind = u32(); offset = u64()
        infos.append((name, dims, kind, offset))
    alignment = int(meta.get(" general.alignment", meta.get("general.alignment", 32)))
    data_start = (pos + alignment - 1) // alignment * alignment
    return data, version, meta, infos, data_start

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", type=Path)
    ap.add_argument("--tensors", action="store_true")
    ap.add_argument("--all-metadata", action="store_true")
    args = ap.parse_args()
    data, version, meta, infos, data_start = read(args.path)
    print(f"file={args.path} bytes={len(data)} version={version} tensors={len(infos)} metadata={len(meta)} data_start={data_start}")
    wanted = (
        "general.architecture", "general.name", "general.file_type", "general.alignment",
        "block_count", "embedding_length", "feed_forward_length", "attention.head_count",
        "attention.head_count_kv", "attention.key_length", "attention.value_length",
        "rope.dimension_count", "rope.freq_base", "context_length", "tokenizer.ggml.model",
        "tokenizer.ggml.tokens", "tokenizer.ggml.merges", "tokenizer.ggml.bos_token_id",
        "tokenizer.ggml.eos_token_id", "expert_count", "expert_used_count",
    )
    for key in wanted:
        for actual, val in meta.items():
            if actual == key or actual.endswith("." + key):
                if isinstance(val, list):
                    print(f"meta.{actual}=list[{len(val)}]")
                else:
                    print(f"meta.{actual}={val!r}")
                break
    if args.all_metadata:
        for key, val in meta.items():
            if isinstance(val, list):
                print(f"meta.{key}=list[{len(val)}]")
            else:
                print(f"meta.{key}={val!r}")
    if args.tensors:
        for name, dims, kind, offset in infos:
            print(f"tensor name={name!r} dims={dims} type={kind} rel_offset={offset} abs_offset={data_start + offset}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
