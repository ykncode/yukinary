#!/usr/bin/env python3
"""Independent, bounded GGUF v3 model profile reader.

This module reads model metadata and tensor descriptors only. It does not
pretend to run inference and it never converts a malformed file into a valid
model. The selected path is user-controlled (for example a BitNet GGUF under
models/) and is validated before an LLM provider receives it.
"""

from __future__ import annotations

import math
import os
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any, BinaryIO, Dict, List


class GGUFModelError(ValueError):
    """Raised for an unreadable, malformed, unsupported, or unsafe GGUF."""


GGUF_MAGIC = b"GGUF"
GGUF_VERSION = 3
_MAX_STRING = 1 << 20
_MAX_METADATA = 1_000_000
_MAX_TENSORS = 1_000_000
_MAX_ARRAY = 1 << 24

_VALUE_TYPES = {
    0: "uint8",
    1: "int8",
    2: "uint16",
    3: "int16",
    4: "uint32",
    5: "int32",
    6: "float32",
    7: "bool",
    8: "string",
    9: "array",
    10: "uint64",
    11: "int64",
    12: "float64",
}


@dataclass(frozen=True)
class GGUFTensorProfile:
    name: str
    dimensions: tuple[int, ...]
    ggml_type: int
    offset: int


@dataclass(frozen=True)
class GGUFModelProfile:
    path: str
    file_size: int
    version: int
    tensor_count: int
    metadata_count: int
    data_offset: int
    alignment: int
    architecture: str
    quantization: str
    embedding_length: int
    block_count: int
    head_count: int
    kv_head_count: int
    feed_forward_length: int
    context_length: int
    metadata: Dict[str, Any]
    tensors: tuple[GGUFTensorProfile, ...]

    def public(self) -> Dict[str, Any]:
        """Return provider-safe model facts; tensor bytes are never exposed."""
        return {
            "path": self.path,
            "file_size": self.file_size,
            "version": self.version,
            "tensor_count": self.tensor_count,
            "metadata_count": self.metadata_count,
            "data_offset": self.data_offset,
            "alignment": self.alignment,
            "architecture": self.architecture,
            "quantization": self.quantization,
            "embedding_length": self.embedding_length,
            "block_count": self.block_count,
            "head_count": self.head_count,
            "kv_head_count": self.kv_head_count,
            "feed_forward_length": self.feed_forward_length,
            "context_length": self.context_length,
        }


class _Reader:
    def __init__(self, stream: BinaryIO, file_size: int):
        self.stream = stream
        self.file_size = file_size

    def read(self, size: int) -> bytes:
        if size < 0 or size > self.file_size:
            raise GGUFModelError("GGUF read size is outside the file bounds")
        data = self.stream.read(size)
        if len(data) != size:
            raise GGUFModelError("GGUF ended before a complete value was read")
        return data

    def u8(self) -> int:
        return self.read(1)[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.read(4))[0]

    def u64(self) -> int:
        return struct.unpack("<Q", self.read(8))[0]

    def i8(self) -> int:
        return struct.unpack("<b", self.read(1))[0]

    def i16(self) -> int:
        return struct.unpack("<h", self.read(2))[0]

    def i32(self) -> int:
        return struct.unpack("<i", self.read(4))[0]

    def i64(self) -> int:
        return struct.unpack("<q", self.read(8))[0]

    def f32(self) -> float:
        return struct.unpack("<f", self.read(4))[0]

    def f64(self) -> float:
        return struct.unpack("<d", self.read(8))[0]

    def string(self) -> str:
        length = self.u64()
        if length > _MAX_STRING:
            raise GGUFModelError(f"GGUF string exceeds {_MAX_STRING} bytes")
        try:
            return self.read(length).decode("utf-8")
        except UnicodeDecodeError as exc:
            raise GGUFModelError("GGUF contains invalid UTF-8 metadata") from exc

    def value(self, value_type: int) -> Any:
        if value_type == 0:
            return self.u8()
        if value_type == 1:
            return self.i8()
        if value_type == 2:
            return struct.unpack("<H", self.read(2))[0]
        if value_type == 3:
            return self.i16()
        if value_type == 4:
            return self.u32()
        if value_type == 5:
            return self.i32()
        if value_type == 6:
            return self.f32()
        if value_type == 7:
            value = self.u8()
            if value not in (0, 1):
                raise GGUFModelError("GGUF bool metadata is not 0 or 1")
            return bool(value)
        if value_type == 8:
            return self.string()
        if value_type == 9:
            element_type = self.u32()
            count = self.u64()
            if element_type not in _VALUE_TYPES or element_type == 9:
                raise GGUFModelError("GGUF array element type is invalid")
            if count > _MAX_ARRAY:
                raise GGUFModelError("GGUF metadata array is too large")
            return [self.value(element_type) for _ in range(count)]
        if value_type == 10:
            return self.u64()
        if value_type == 11:
            return self.i64()
        if value_type == 12:
            value = self.f64()
            if not math.isfinite(value):
                raise GGUFModelError("GGUF float metadata must be finite")
            return value
        raise GGUFModelError(f"unsupported GGUF metadata type: {value_type}")


def _nested_int(metadata: Dict[str, Any], architecture: str, suffix: str) -> int:
    value = metadata.get(f"{architecture}.{suffix}")
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int) and value >= 0:
        return value
    return 0


def _quantization(tensors: List[GGUFTensorProfile]) -> str:
    if not tensors:
        return "UNKNOWN"
    type_names = {
        0: "F32", 1: "F16", 2: "Q4_0", 3: "Q4_1", 6: "Q5_0",
        7: "Q5_1", 8: "Q8_0", 10: "Q2_K", 11: "Q3_K", 12: "Q4_K",
        13: "Q5_K", 14: "Q6_K", 15: "Q8_K", 16: "IQ2_XXS", 17: "IQ2_XS",
        18: "IQ3_XXS", 19: "IQ1_S", 20: "IQ4_NL", 21: "IQ3_S",
        22: "IQ2_S_LEGACY", 23: "IQ4_XS", 24: "I8", 25: "I16", 26: "I32",
        27: "I64", 28: "F64", 29: "IQ1_M", 30: "BF16", 34: "IQ2_S_LEGACY",
        36: "IQ2_S", 41: "Q1_0",
    }
    names = {type_names.get(t.ggml_type, f"TYPE_{t.ggml_type}") for t in tensors}
    return next(iter(names)) if len(names) == 1 else "+".join(sorted(names))


def read_gguf_profile(path: str | os.PathLike[str]) -> GGUFModelProfile:
    resolved = Path(path).expanduser().resolve()
    if not resolved.is_file():
        raise GGUFModelError(f"GGUF model does not exist: {resolved}")
    file_size = resolved.stat().st_size
    if file_size < 24:
        raise GGUFModelError("GGUF file is shorter than its fixed header")

    with resolved.open("rb") as stream:
        reader = _Reader(stream, file_size)
        if reader.read(4) != GGUF_MAGIC:
            raise GGUFModelError("GGUF magic is invalid")
        version = reader.u32()
        if version != GGUF_VERSION:
            raise GGUFModelError(f"unsupported GGUF version: {version}")
        tensor_count = reader.u64()
        metadata_count = reader.u64()
        if tensor_count > _MAX_TENSORS or metadata_count > _MAX_METADATA:
            raise GGUFModelError("GGUF counts exceed safety limits")

        metadata: Dict[str, Any] = {}
        for _ in range(metadata_count):
            key = reader.string()
            if key in metadata:
                raise GGUFModelError(f"duplicate GGUF metadata key: {key}")
            value_type = reader.u32()
            metadata[key] = reader.value(value_type)

        tensors: List[GGUFTensorProfile] = []
        for _ in range(tensor_count):
            name = reader.string()
            dimensions_count = reader.u32()
            if dimensions_count > 16:
                raise GGUFModelError("GGUF tensor has too many dimensions")
            dimensions = tuple(reader.u64() for _ in range(dimensions_count))
            if any(d == 0 or d > (1 << 32) for d in dimensions):
                raise GGUFModelError(f"invalid dimensions for tensor {name}")
            ggml_type = reader.u32()
            offset = reader.u64()
            tensors.append(GGUFTensorProfile(name, dimensions, ggml_type, offset))

        architecture_value = metadata.get("general.architecture", "")
        architecture = architecture_value if isinstance(architecture_value, str) else ""
        if not architecture or any(
            not (character.isalnum() or character in "._-") for character in architecture
        ):
            raise GGUFModelError("GGUF general.architecture is missing or invalid")
        alignment_value = metadata.get("general.alignment", 32)
        if not isinstance(alignment_value, int) or alignment_value <= 0 or alignment_value > (1 << 20):
            raise GGUFModelError("GGUF alignment is invalid")
        data_offset = (stream.tell() + alignment_value - 1) // alignment_value * alignment_value
        if data_offset > file_size:
            raise GGUFModelError("GGUF tensor data offset exceeds file size")
        for tensor in tensors:
            if tensor.offset > file_size - data_offset:
                raise GGUFModelError(f"tensor offset exceeds file size: {tensor.name}")

    return GGUFModelProfile(
        path=str(resolved),
        file_size=file_size,
        version=version,
        tensor_count=tensor_count,
        metadata_count=metadata_count,
        data_offset=data_offset,
        alignment=alignment_value,
        architecture=architecture,
        quantization=_quantization(tensors),
        embedding_length=_nested_int(metadata, architecture, "embedding_length"),
        block_count=_nested_int(metadata, architecture, "block_count"),
        head_count=_nested_int(metadata, architecture, "attention.head_count"),
        kv_head_count=_nested_int(metadata, architecture, "attention.head_count_kv"),
        feed_forward_length=_nested_int(metadata, architecture, "feed_forward_length"),
        context_length=_nested_int(metadata, architecture, "context_length"),
        metadata=metadata,
        tensors=tuple(tensors),
    )


__all__ = ["GGUFModelError", "GGUFModelProfile", "GGUFTensorProfile", "read_gguf_profile"]


if __name__ == "__main__":
    import argparse
    import json

    parser = argparse.ArgumentParser(description="Validate and inspect a GGUF v3 model profile")
    parser.add_argument("model", help="user-selected GGUF model path")
    args = parser.parse_args()
    try:
        profile = read_gguf_profile(args.model)
    except GGUFModelError as exc:
        parser.error(str(exc))
    print(json.dumps(profile.public(), ensure_ascii=False, indent=2, sort_keys=True))
