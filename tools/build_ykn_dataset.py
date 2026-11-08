#!/usr/bin/env python3
"""Create dependency-free YKN instruction data from source declarations.

The output is JSONL and intentionally keeps source provenance. It is suitable
for RAG evaluation now and for later LoRA/QLoRA conversion without pretending
that generated examples are hand-curated gold data.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

DECL = re.compile(r"^\s*isv\s+([A-Za-zÇĞİÖŞÜçğıöşü_][\wÇĞİÖŞÜçğıöşü]*)\s*\(")
TYPE_DECL = re.compile(r"^\s*yapi\s+([A-Za-zÇĞİÖŞÜçğıöşü_][\wÇĞİÖŞÜçğıöşü]*)")


def declarations(root: Path):
    for folder in (root / "src", root / "std", root / "tests"):
        if not folder.exists():
            continue
        for path in sorted(folder.rglob("*.ykn")):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            i = 0
            while i < len(lines):
                match = DECL.match(lines[i]) or TYPE_DECL.match(lines[i])
                if not match:
                    i += 1
                    continue
                start = i
                depth = 0
                opened = False
                while i < len(lines):
                    line = lines[i]
                    depth += line.count("{") - line.count("}")
                    opened = opened or "{" in line
                    i += 1
                    if opened and depth <= 0:
                        break
                    if i - start >= 80:
                        break
                text = "\n".join(lines[start:i]).strip()
                if text:
                    yield path.relative_to(root).as_posix(), start + 1, match.group(1), text


def instruction(name: str, text: str) -> str:
    if text.lstrip().startswith("yapi"):
        return f"YKN dilinde {name} adli struct tanimini kullanarak kod yaz."
    if any(word in name.casefold() for word in ("oku", "yaz", "dosya", "file")):
        return f"YKN dilinde {name} fonksiyonunun kullanimini goster."
    return f"YKN dilinde {name} fonksiyonunu kullanan gecerli bir ornek yaz."


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-records", type=int, default=10000)
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    seen: set[tuple[str, str]] = set()
    with output.open("w", encoding="utf-8", newline="\n") as stream:
        for path, line, name, text in declarations(root):
            key = (name, text)
            if key in seen:
                continue
            seen.add(key)
            record = {
                "instruction": instruction(name, text),
                "input": "",
                "output": text,
                "language": "ykn",
                "source": {"path": path, "line": line},
            }
            stream.write(json.dumps(record, ensure_ascii=False) + "\n")
            count += 1
            if count >= args.max_records:
                break
    print(json.dumps({"records": count, "output": str(output)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
