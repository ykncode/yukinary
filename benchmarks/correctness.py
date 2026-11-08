"""Aggregate oracle-based correctness runs; contains no performance timings."""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]


def load(path: Path, encoding: str = "utf-8") -> dict[str, Any]:
    return json.loads(path.read_text(encoding=encoding))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--canonical", type=Path, default=ROOT / "benchmarks/full-suite-results.json")
    parser.add_argument("--fix13", type=Path, default=ROOT / "benchmarks/fix13-results.json")
    parser.add_argument("--output", type=Path, default=ROOT / "benchmarks/correctness.json")
    args = parser.parse_args()

    canonical = load(args.canonical)
    fix13 = load(args.fix13, "utf-8-sig")
    cases: list[dict[str, Any]] = []
    for result in canonical["results"]:
        cases.append({"suite": result.get("Suite"), "fixture": result.get("File"),
                      "status": result.get("Status"), "expected_match": result.get("ExpectedMatch"),
                      "duration_ms": result.get("DurationMs"), "output": result.get("Output", "")})
    for result in fix13["results"]:
        cases.append({"suite": result.get("suite", "FIX-13"), "fixture": result.get("name"),
                      "status": result.get("status"), "expected_match": result.get("strict_oracle_passed"),
                      "duration_ms": None, "output": result.get("output", "")})

    summary: dict[str, dict[str, int]] = defaultdict(lambda: {"total": 0, "passed": 0, "failed": 0})
    for case in cases:
        row = summary[case["suite"]]
        row["total"] += 1
        if str(case["status"]).startswith(("PASS", "GECTI")):
            row["passed"] += 1
        else:
            row["failed"] += 1
    passed = sum(row["passed"] for row in summary.values())
    failed = sum(row["failed"] for row in summary.values())
    test_root = ROOT / "tests"
    fixture_count = sum(1 for _ in test_root.glob("*.ykn")) + sum(
        1 for path in test_root.iterdir()
        if path.is_dir() and path.name.lower() not in {"doc", "docs", "__pycache__"}
        for _ in path.rglob("*.ykn"))
    report = {
        "schema": "ykn.correctness.v1",
        "meaning": "only oracle correctness outcomes; performance metrics are excluded",
        "fixture_inventory_count": fixture_count,
        "executed_case_count": len(cases), "passed": passed, "failed": failed,
        "pass_rate_percent": round(passed * 100 / len(cases), 2) if cases else 0,
        "coverage_warning": "This report combines the repository's canonical 122-case suite and separate 34-case FIX-13 suite; it does not claim all fixture files were executed.",
        "sources": {"canonical": str(args.canonical), "fix13": str(args.fix13)},
        "suites": dict(summary), "failures": [case for case in cases if not str(case["status"]).startswith(("PASS", "GECTI"))],
        "cases": cases,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Correctness only: {passed}/{len(cases)} PASS, {failed} FAIL; fixtures inventoried={fixture_count}")
    print(f"Report: {args.output}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
