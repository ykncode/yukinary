#!/usr/bin/env python3
"""Real GGUF selection/profile and gateway propagation test."""

from __future__ import annotations

import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.gguf_model import GGUFModelError, read_gguf_profile
from tools.yknllm_gateway import ExternalLLMGateway, GatewayProtocolError, TypedIntent, YKNLLMResponse


def gguf_string(value: str) -> bytes:
    encoded = value.encode("utf-8")
    return struct.pack("<Q", len(encoded)) + encoded


def build_model(path: Path) -> None:
    metadata = [
        ("general.architecture", 8, "bitnet"),
        ("general.alignment", 4, 32),
        ("bitnet.embedding_length", 4, 4),
        ("bitnet.block_count", 4, 2),
        ("bitnet.attention.head_count", 4, 1),
        ("bitnet.attention.head_count_kv", 4, 1),
        ("bitnet.context_length", 4, 16),
    ]
    payload = bytearray()
    payload += b"GGUF"
    payload += struct.pack("<IQQ", 3, 1, len(metadata))
    for key, value_type, value in metadata:
        payload += gguf_string(key)
        payload += struct.pack("<I", value_type)
        if value_type == 8:
            payload += gguf_string(value)
        elif value_type == 4:
            payload += struct.pack("<I", value)
        else:
            raise AssertionError(value_type)
    payload += gguf_string("token_embd.weight")
    payload += struct.pack("<I", 2)
    payload += struct.pack("<QQ", 4, 4)
    payload += struct.pack("<IQ", 0, 0)
    payload += b"\0" * ((32 - len(payload) % 32) % 32)
    payload += b"\0" * 64
    path.write_bytes(payload)


def main() -> int:
    compiler = ROOT / "build" / "agent-stage0.exe"
    if not compiler.is_file():
        raise RuntimeError(f"compiler not found: {compiler}")
    with tempfile.TemporaryDirectory(prefix="ykn-gguf-selection-") as temp:
        root = Path(temp)
        model = root / "bitnet-user-selected.gguf"
        build_model(model)
        profile = read_gguf_profile(model)
        if profile.architecture != "bitnet" or profile.block_count != 2:
            return 1
        if profile.tensor_count != 1 or profile.quantization != "F32":
            return 2

        real_model = ROOT / "models" / "bitnet-b1.58-2B-4T" / "ggml-model-i2_s.gguf"
        if real_model.is_file():
            real_profile = read_gguf_profile(real_model)
            if (real_profile.architecture != "bitnet-b1.58" or
                    real_profile.quantization != "F16+F32+IQ2_S" or
                    real_profile.block_count != 30 or real_profile.tensor_count != 332):
                return 7

        audit = root / "audit.jsonl"
        gateway = ExternalLLMGateway(str(compiler), str(audit), str(model))

        def adapter(request):
            selected = request["model"]
            if request["role"] != "YKN_CODE_ONLY" or not selected["read_only"]:
                raise AssertionError("model role/capability contract was not enforced")
            if selected["profile"]["architecture"] != "bitnet":
                raise AssertionError("selected GGUF profile was not propagated")
            return YKNLLMResponse("YKN_CODE", "isv ana() -> tamsayi { dondu 0; }\n", "user-gguf-v1", 0.95)

        record = gateway.handle(TypedIntent("ana fonksiyonu yaz"), adapter)
        if not record.accepted or record.selected_gguf != str(model.resolve()):
            return 3
        if record.gguf_architecture != "bitnet" or record.gguf_quantization != "F32":
            return 4

        bad = root / "bad.gguf"
        bad.write_bytes(b"not-a-gguf")
        try:
            gateway.set_model(str(bad))
            return 5
        except GatewayProtocolError:
            pass
        try:
            read_gguf_profile(str(bad))
            return 6
        except GGUFModelError:
            pass

    print("GGUF_MODEL_SELECTION_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
