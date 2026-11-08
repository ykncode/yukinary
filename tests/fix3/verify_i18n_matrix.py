"""Validate FIX-3's language/category acceptance coverage contract."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REGISTRY_PATH = ROOT / "tools" / "keyword_registry.json"
MATRIX_PATH = ROOT / "tests" / "fix3" / "i18n_matrix.json"


def main() -> int:
    registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    matrix = json.loads(MATRIX_PATH.read_text(encoding="utf-8"))
    registry_names = {
        item["name"] for item in registry["aliases"]
    }
    required_categories = set(matrix["categories"])
    errors: list[str] = []

    for language, entry in matrix["languages"].items():
        fixture = ROOT / entry["fixture"]
        if not fixture.is_file():
            errors.append(f"{language}: fixture bulunamadı: {entry['fixture']}")
            continue
        fixture_text = fixture.read_text(encoding="utf-8")
        language_names = {
            item["name"]
            for item in registry["aliases"]
            if item["language"] == language
        }
        if not (language_names & set(fixture_text.split())):
            # Punctuation can be adjacent to an alias, so use substring as a
            # second check after the cheap token check.
            if not any(name in fixture_text for name in language_names):
                errors.append(f"{language}: fixture registry aliası kullanmıyor")

        categories = set(entry.get("categories", {}))
        missing = required_categories - categories
        if missing:
            errors.append(f"{language}: kategori eksik: {', '.join(sorted(missing))}")
        for category, relative_path in entry.get("categories", {}).items():
            if not (ROOT / relative_path).is_file():
                errors.append(f"{language}/{category}: fixture bulunamadı: {relative_path}")

    if errors:
        for error in errors:
            print(f"I18N MATRIX FAIL: {error}")
        return 1

    print(
        f"I18N MATRIX PASS: {len(matrix['languages'])} languages x "
        f"{len(required_categories)} categories; shared semantic fixtures validated"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
