#!/usr/bin/env python3
"""Independent GPT-2 byte-level BPE oracle for supported GGUF vocabularies."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from gguf_model import read_gguf_profile

# GPT-2's regex over the ASCII subset used by the YKN chat acceptance fixture.
_PRETOKENIZE = re.compile(
    r"'s|'t|'re|'ve|'m|'ll|'d| ?[A-Za-z]+| ?[0-9]+| ?[^A-Za-z0-9\s]+|\s+(?!\S)|\s+"
)


def byte_encoder() -> dict[int, str]:
    visible = list(range(ord("!"), ord("~") + 1))
    visible += list(range(161, 173))
    visible += list(range(174, 256))
    codepoints = visible[:]
    extra = 0
    for byte in range(256):
        if byte not in visible:
            visible.append(byte)
            codepoints.append(256 + extra)
            extra += 1
    return {byte: chr(codepoint) for byte, codepoint in zip(visible, codepoints)}


def encode(text: str, tokens: list[str], merges: list[str]) -> list[int]:
    if len(set(tokens)) != len(tokens):
        raise ValueError("GGUF tokenizer vocab contains duplicate token strings")
    vocab = {token: index for index, token in enumerate(tokens)}
    ranks: dict[tuple[str, str], int] = {}
    for rank, merge in enumerate(merges):
        pair = merge.split(" ", 1)
        if len(pair) != 2:
            raise ValueError(f"malformed GGUF merge entry at rank {rank}")
        ranks[(pair[0], pair[1])] = rank
    encoder = byte_encoder()
    ids: list[int] = []
    specials = sorted(
        (token for token in tokens if token.startswith("<|") and token.endswith("|>")),
        key=len,
        reverse=True,
    )

    def encode_plain(segment: str) -> None:
        pieces = _PRETOKENIZE.findall(segment)
        if "".join(pieces) != segment:
            raise ValueError("GPT-2 pretokenizer failed to cover the full input")
        for piece in pieces:
            symbols = [encoder[value] for value in piece.encode("utf-8")]
            while len(symbols) > 1:
                best_index = -1
                best_rank: int | None = None
                for index in range(len(symbols) - 1):
                    rank = ranks.get((symbols[index], symbols[index + 1]))
                    if rank is not None and (best_rank is None or rank < best_rank):
                        best_rank = rank
                        best_index = index
                if best_index < 0:
                    break
                symbols[best_index:best_index + 2] = [symbols[best_index] + symbols[best_index + 1]]
            for symbol in symbols:
                try:
                    ids.append(vocab[symbol])
                except KeyError as exc:
                    raise ValueError(f"GPT-2 fallback token is missing from GGUF vocab: {symbol!r}") from exc

    offset = 0
    while offset < len(text):
        special = next((token for token in specials if text.startswith(token, offset)), None)
        if special is not None:
            ids.append(vocab[special])
            offset += len(special)
            continue
        next_special = min(
            (position for token in specials if (position := text.find(token, offset)) >= 0),
            default=len(text),
        )
        encode_plain(text[offset:next_special])
        offset = next_special
    return ids


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("model", type=Path)
    parser.add_argument("--text", required=True)
    args = parser.parse_args()
    profile = read_gguf_profile(args.model)
    if profile.architecture not in {"bitnet-b1.58", "qwen3"}:
        parser.error(f"unsupported GPT-2 BPE GGUF architecture: {profile.architecture!r}")
    metadata = profile.metadata
    tokens = metadata.get("tokenizer.ggml.tokens")
    merges = metadata.get("tokenizer.ggml.merges")
    if not isinstance(tokens, list) or not all(isinstance(x, str) for x in tokens):
        parser.error("GGUF tokenizer.ggml.tokens is absent or invalid")
    if not isinstance(merges, list) or not all(isinstance(x, str) for x in merges):
        parser.error("GGUF tokenizer.ggml.merges is absent or invalid")
    ids = encode(args.text, tokens, merges)
    result = {
        "architecture": profile.architecture,
        "text": args.text,
        "token_count": len(ids),
        "token_ids": ids,
        "bos_token_id": metadata.get("tokenizer.ggml.bos_token_id"),
        "eos_token_id": metadata.get("tokenizer.ggml.eos_token_id"),
    }
    print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
