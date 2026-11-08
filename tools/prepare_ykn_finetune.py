#!/usr/bin/env python3
"""Convert YKN instruction JSONL into deterministic chat train/validation JSONL."""
from __future__ import annotations

import argparse
import json
import random
from pathlib import Path
from typing import Any

SYSTEM = (
    "Sen YKN dilinde uzman bir kod asistanisin. "
    "Yalnizca gecerli YKN kodu uret; Markdown veya aciklama ekleme. "
    "YKN kurallarini ve verilen kaynak ornegini koru."
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--validation-ratio", type=float, default=0.1)
    parser.add_argument("--seed", type=int, default=20260908)
    args = parser.parse_args()
    if not 0.0 < args.validation_ratio < 1.0:
        parser.error("--validation-ratio 0 ile 1 arasinda olmali")
    rows: list[dict[str, Any]] = []
    for line in args.input.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        item = json.loads(line)
        if not item.get("instruction") or not item.get("output"):
            continue
        rows.append({
            "messages": [
                {"role": "system", "content": SYSTEM},
                {"role": "user", "content": item["instruction"]},
                {"role": "assistant", "content": item["output"]},
            ],
            "source": item.get("source", {}),
            "language": "ykn",
        })
    random.Random(args.seed).shuffle(rows)
    validation_count = max(1, int(len(rows) * args.validation_ratio))
    validation = rows[:validation_count]
    train = rows[validation_count:]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    train_path = args.output_dir / "train.jsonl"
    validation_path = args.output_dir / "validation.jsonl"
    for path, values in ((train_path, train), (validation_path, validation)):
        with path.open("w", encoding="utf-8", newline="\n") as stream:
            for value in values:
                stream.write(json.dumps(value, ensure_ascii=False) + "\n")
    manifest = {
        "schema": "yukinary.ykn-finetune.v1",
        "seed": args.seed,
        "records": len(rows),
        "train_records": len(train),
        "validation_records": len(validation),
        "train": str(train_path),
        "validation": str(validation_path),
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(manifest, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
