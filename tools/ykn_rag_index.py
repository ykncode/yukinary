#!/usr/bin/env python3
"""Build a small, deterministic retrieval index for YKN source context.

The index is intentionally dependency-free. It extracts declaration-sized
windows from src/, std/, and tests/ and ranks them by query-token overlap.
Runtime integration can consume the generated JSON without changing the
compiler's source-of-truth files.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

DECL_RE = re.compile(r"^\s*(?:isv|yapi|enum|#)\b", re.IGNORECASE)
TOKEN_RE = re.compile(r"[\wÇĞİÖŞÜçğıöşü]+", re.UNICODE)
KEYWORDS = {"bir", "birkaç", "ile", "için", "yaz", "oluştur", "hesapla", "fonksiyon", "fonksiyonu"}


def tokens(text: str) -> set[str]:
    return {x.casefold() for x in TOKEN_RE.findall(text) if len(x) > 1 and x.casefold() not in KEYWORDS}


def collect(root: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for folder in (root / "src", root / "std", root / "tests"):
        if not folder.exists():
            continue
        for path in sorted(folder.rglob("*.ykn")):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            i = 0
            while i < len(lines):
                if not DECL_RE.match(lines[i]):
                    i += 1
                    continue
                start = i
                depth = 0
                seen_brace = False
                while i < len(lines):
                    line = lines[i]
                    depth += line.count("{") - line.count("}")
                    seen_brace = seen_brace or "{" in line
                    i += 1
                    if seen_brace and depth <= 0:
                        break
                    if i - start >= 80:
                        break
                snippet = "\n".join(lines[start:i]).strip()
                if snippet:
                    records.append({
                        "path": path.relative_to(root).as_posix(),
                        "line": start + 1,
                        "text": snippet,
                        "tokens": sorted(tokens(snippet)),
                    })
            # Avoid indexing comments or non-declaration fragments as records.
    return records


def rank(records: list[dict[str, Any]], query: str, limit: int) -> list[dict[str, Any]]:
    wanted = tokens(query)
    scored: list[tuple[int, str, int, dict[str, Any]]] = []
    for record in records:
        overlap = len(wanted.intersection(record["tokens"]))
        # Prefer stdlib and tests as executable API examples on ties.
        bonus = 1 if record["path"].startswith(("std/", "tests/")) else 0
        scored.append((overlap * 10 + bonus, record["path"], record["line"], record))
    scored.sort(key=lambda item: (-item[0], item[1], item[2]))
    return [item[3] for item in scored[: max(1, limit)] if item[0] > 0] or [item[3] for item in scored[: max(1, limit)]]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--query", required=True)
    parser.add_argument("--limit", type=int, default=6)
    parser.add_argument("--out-json", type=Path, default=None)
    parser.add_argument("--out-context", type=Path, default=None)
    args = parser.parse_args()
    root = args.root.resolve()
    records = collect(root)
    selected = rank(records, args.query, args.limit)
    result = {
        "schema": "yukinary.ykn-rag.v1",
        "query": args.query,
        "record_count": len(records),
        "results": selected,
    }
    if args.out_json:
        args.out_json.parent.mkdir(parents=True, exist_ok=True)
        args.out_json.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if args.out_context:
        args.out_context.parent.mkdir(parents=True, exist_ok=True)
        chunks = [f"YKN kaynak context'i: {args.query}", "Yalnızca geçerli YKN üret."]
        for item in selected:
            chunks.append(f"\n# kaynak: {item['path']}:{item['line']}\n{item['text']}")
        args.out_context.write_text("\n".join(chunks) + "\n", encoding="utf-8")
    print(json.dumps({"records": len(records), "selected": len(selected)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
