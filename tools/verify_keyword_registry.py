"""Validate the single reviewable YKN keyword alias registry.

The compiler deliberately keeps its seed compiler self-contained. The runtime
registry is owned by lexer.ykn; keywords.ykn is metadata-only so the flattened
self-host source does not create a second runtime table. This check validates
that single table and the generated C++ seed without making bootstrap depend
on a Python code generator at runtime.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "tools" / "keyword_registry.json"
SOURCE_FILES = (ROOT / "src" / "lexer.ykn",)
METADATA_FILE = ROOT / "src" / "keywords.ykn"
APP = ROOT / "app_full.cpp"


def string_literals(text: str):
    return re.findall(r'\"((?:\\.|[^\"\\])*)\"', text)


def has_raw_non_ascii_literal(text: str) -> bool:
    return any(any(ord(ch) > 127 for ch in value) for value in string_literals(text))


def extract_function(text: str, signature: str) -> str:
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise ValueError(f"kapanmayan C++ fonksiyonu: {signature}")


def main() -> int:
    data = json.loads(REGISTRY.read_text(encoding="utf-8"))
    aliases = data.get("aliases", [])
    names = [item["name"] for item in aliases]
    errors: list[str] = []

    if len(names) != len(set(names)):
        errors.append("registry içinde duplicate alias var")
    languages = set(data.get("languages", []))
    for item in aliases:
        if item.get("language") not in languages:
            errors.append(f"geçersiz dil: {item.get('language')!r}")
        if not item.get("token", "").startswith("TIP_"):
            errors.append(f"geçersiz token: {item.get('token')!r}")

    source_text = [path.read_text(encoding="utf-8") for path in SOURCE_FILES]
    for name in names:
        missing = [str(path.relative_to(ROOT)) for path, text in zip(SOURCE_FILES, source_text) if name not in text]
        if missing:
            errors.append(f"alias {name!r} eksik: {', '.join(missing)}")

    metadata_text = METADATA_FILE.read_text(encoding="utf-8")
    if "tools/keyword_registry.json" not in metadata_text:
        errors.append("keywords.ykn metadata registry JSON kaynağını belirtmiyor")
    if "kelime_tipini_bul" in metadata_text:
        errors.append("keywords.ykn runtime resolver tanımı içeriyor; duplicate tablo oluşur")

    app_text = APP.read_text(encoding="utf-8", errors="replace")
    keyword_body = extract_function(app_text, "long long kelime_tipini_bul(char* adres")
    if has_raw_non_ascii_literal(keyword_body):
        errors.append("app_full.cpp keyword fonksiyonunda raw non-ASCII literal kaldı")

    type_body = extract_function(app_text, "long long tip_adini_coz(char* ad")
    # Type aliases are required to use the same escaped-byte policy as keywords.
    if has_raw_non_ascii_literal(type_body):
        errors.append("app_full.cpp type alias fonksiyonunda raw non-ASCII literal kaldı")

    entry_start = app_text.index("// Resolve multilingual entry points")
    entry_end = app_text.index("// Emit startup thunk", entry_start)
    if has_raw_non_ascii_literal(app_text[entry_start:entry_end]):
        errors.append("app_full.cpp entry-point çözümlemesinde raw non-ASCII literal kaldı")

    if errors:
        for error in errors:
            print(f"KEYWORD REGISTRY FAIL: {error}")
        return 1

    by_language = {language: 0 for language in sorted(languages)}
    for item in aliases:
        by_language[item["language"]] += 1
    counts = ", ".join(f"{language}={count}" for language, count in by_language.items())
    print(f"KEYWORD REGISTRY PASS: {len(names)} aliases; {counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
