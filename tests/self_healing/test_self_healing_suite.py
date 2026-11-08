#!/usr/bin/env python3
"""
tests/self_healing/test_self_healing_suite.py
Yukinary (YKN) Self-Healing Acceptance Suite — FAZ 9.1

Tests 120 held-out error variants across 6 categories:
  - SYNTAX
  - UNDEFINED_SYMBOL
  - TYPE_MISMATCH
  - NULL_SAFETY
  - INDEX_ACCESS
  - RETURN_TYPE

Verifies:
  1. High repair accuracy (> 85% on held-out test suite)
  2. Safety barrier: 100% of accepted repairs compile with 0 errors
  3. P50/P95 latency measurement
  4. Model independence (works with both Stage-3 and Stage-0)
  5. Structured report generation in build/reports/self_healing_report.json
"""

import os
import sys
import json
import time
from collections import defaultdict

# Add tools to sys.path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
sys.path.insert(0, os.path.join(ROOT_DIR, "tools"))

from self_healing_engine import SelfHealingEngine

def run_suite(compiler_path: str, dataset_path: str, report_path: str):
    print("=" * 60)
    print("  YUKINARY SELF-HEALING ACCEPTANCE SUITE (FAZ 9.1)")
    print("=" * 60)
    print(f"Compiler : {compiler_path}")
    print(f"Dataset  : {dataset_path}")
    
    if not os.path.exists(dataset_path):
        raise FileNotFoundError(f"Dataset not found: {dataset_path}")
    if not os.path.exists(compiler_path):
        raise FileNotFoundError(f"Compiler not found: {compiler_path}")

    records = []
    with open(dataset_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                records.append(json.loads(line))

    engine = SelfHealingEngine(compiler_path=compiler_path)

    category_stats = defaultdict(lambda: {"total": 0, "repaired": 0, "latencies": []})
    round_stats = defaultdict(int)
    latencies = []
    safety_violations = 0

    print(f"\nEvaluating {len(records)} held-out error cases...")
    for idx, r in enumerate(records):
        cat = r["category"]
        corrupted = r["corrupted_code"]
        err_line = r.get("error_line", 1)

        result = engine.repair(corrupted, error_line=err_line)
        lat = result["elapsed_ms"]
        latencies.append(lat)
        category_stats[cat]["total"] += 1
        category_stats[cat]["latencies"].append(lat)

        if result["success"]:
            # Verify Safety Barrier: double check that repaired code compiles cleanly
            valid, _ = engine.test_compile(result["repaired_code"])
            if not valid:
                safety_violations += 1
                print(f"[SAFETY VIOLATION] Test {r['id']} claimed success but failed compilation!")
            else:
                category_stats[cat]["repaired"] += 1
                round_stats[result["round"]] += 1
        else:
            round_stats[-1] += 1

        if (idx + 1) % 20 == 0:
            print(f"  Processed {idx + 1}/{len(records)} cases...", flush=True)

    # Calculate percentiles
    latencies.sort()
    p50 = latencies[len(latencies) // 2] if latencies else 0.0
    p95_idx = int(len(latencies) * 0.95)
    p95 = latencies[min(p95_idx, len(latencies) - 1)] if latencies else 0.0

    total_tests = len(records)
    total_repaired = sum(c["repaired"] for c in category_stats.values())
    accuracy = (total_repaired / total_tests) * 100.0 if total_tests > 0 else 0.0

    print("\n" + "-" * 60)
    print("RESULTS BY CATEGORY:")
    print("-" * 60)
    for cat in sorted(category_stats.keys()):
        stats = category_stats[cat]
        cat_acc = (stats["repaired"] / stats["total"]) * 100.0 if stats["total"] > 0 else 0.0
        print(f"  {cat:<18}: {stats['repaired']:>3}/{stats['total']:>3} repaired ({cat_acc:>5.1f}%)")

    print("-" * 60)
    print(f"Overall Accuracy       : {total_repaired}/{total_tests} ({accuracy:.1f}%)")
    print(f"Safety Violations      : {safety_violations} (Must be 0)")
    print(f"P50 Latency            : {p50:.2f} ms")
    print(f"P95 Latency            : {p95:.2f} ms")
    print(f"Rounds Distribution    : Round 1={round_stats[1]}, Round 2={round_stats[2]}, Round 3={round_stats[3]}, Rejected={round_stats[-1]}")
    print("-" * 60)

    report = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "compiler": compiler_path,
        "total_cases": total_tests,
        "total_repaired": total_repaired,
        "overall_accuracy_percent": round(accuracy, 2),
        "safety_barrier_violations": safety_violations,
        "safety_barrier_verified": (safety_violations == 0),
        "p50_latency_ms": round(p50, 2),
        "p95_latency_ms": round(p95, 2),
        "round_distribution": dict(round_stats),
        "category_results": {
            cat: {
                "total": stats["total"],
                "repaired": stats["repaired"],
                "accuracy_percent": round((stats["repaired"] / stats["total"]) * 100.0, 2)
            }
            for cat, stats in category_stats.items()
        }
    }

    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    print(f"Saved telemetry report: {report_path}")

    # Production acceptance gate: the task contract is stricter than the
    # historical smoke threshold. A report is not PASS unless the complete
    # held-out matrix, safety barrier and latency target all hold.
    if total_tests != 120:
        print(f"\nFAIL: Expected 120 held-out cases, got {total_tests}!")
        sys.exit(1)
    if safety_violations != 0:
        print("\nFAIL: Safety barrier violated!")
        sys.exit(1)
    if accuracy < 99.0:
        print(f"\nFAIL: Accuracy {accuracy:.1f}% below production threshold 99%!")
        sys.exit(1)
    if p95 >= 500.0:
        print(f"\nFAIL: P95 latency {p95:.2f} ms exceeds 500 ms target!")
        sys.exit(1)

    print("\nALL SELF-HEALING ACCEPTANCE TESTS PASSED! [OK]")
    return 0

if __name__ == "__main__":
    compiler = os.path.join(ROOT_DIR, "build", "bootstrap", "stage3.exe")
    dataset = os.path.join(ROOT_DIR, "build", "datasets", "self_healing_heldout_test.jsonl")
    report = os.path.join(ROOT_DIR, "build", "reports", "self_healing_report.json")
    
    if len(sys.argv) > 1:
        compiler = sys.argv[1]
        
    sys.exit(run_suite(compiler, dataset, report))
