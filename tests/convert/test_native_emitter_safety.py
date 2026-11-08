#!/usr/bin/env python3
"""Reject threshold-qualified but incomplete native conversions."""

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    sys.path.insert(0, str(ROOT))
    from tools.convert_engine import ConvertEngine, parse_directive_line

    with tempfile.TemporaryDirectory(prefix="ykn-convert-partial-") as temp:
        root = Path(temp)
        source = root / "partial.c"
        source.write_text(
            "int add(int a, int b) { return a + b; }\n"
            "int unsupported(int a, int b, int c) { return a + b + c; }\n",
            encoding="utf-8",
        )
        directive = parse_directive_line(
            '#C-convert kaynak="partial.c", mod="HARICI_STATIK", esik=0.5;', 1
        )
        if directive is None or not directive.is_valid:
            raise AssertionError("fixture directive did not parse")

        engine = ConvertEngine(cache_dir=str(root / "cache"))
        success, generated, record = engine.process_directive(directive, str(root))
        if success:
            raise AssertionError("partial module was incorrectly emitted")
        if generated.count("E038 [CONVERT_PARTIAL_UNSUPPORTED]") != 1:
            raise AssertionError(f"unexpected diagnostic: {generated}")
        if record.get("status") != "partial_native_unsupported":
            raise AssertionError(f"unexpected conversion status: {record}")
        if record.get("functions_found") != 2 or record.get("functions_convertible") != 1:
            raise AssertionError(f"inaccurate conversion accounting: {record}")
        if record.get("cache_hit") or list((root / "cache").glob("*.ykn")):
            raise AssertionError("rejected conversion was cached as successful")

    print("CONVERT_PARTIAL_NATIVE_REJECTION_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
