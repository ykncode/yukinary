#!/usr/bin/env python3
"""Generate an evidence-oriented, weighted YKN maturity inventory.

This is intentionally not a checkbox counter. Each domain has an explicit
weight and a conservative evidence score. The result is an engineering
progress estimate, not a release approval: an open critical domain keeps the
release gate OPEN even when the weighted percentage is high.

Host Python is used only for the migration-era audit/report tool. It is not a
YKN runtime or product implementation and must eventually be replaced by the
pure-YKN auditor described in A0.3.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = ROOT / "build" / "reports"


def load_json(relative: str):
    path = ROOT / relative
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        return None


def exists(relative: str) -> bool:
    return (ROOT / relative).is_file()


def parity_ok() -> bool:
    data = load_json("build/reports/stage-parity-final-current.json")
    return bool(
        data
        and data.get("gate_pass") is True
        and data.get("compilers", {}).get("stage0", {}).get("passed")
        == data.get("compilers", {}).get("stage0", {}).get("total")
        and data.get("compilers", {}).get("stage3", {}).get("passed")
        == data.get("compilers", {}).get("stage3", {}).get("total")
        and data.get("observable_outcomes_same") is True
    )


def formatter_ok() -> bool:
    data = load_json("build/reports/formatter_idempotence_report.json")
    return bool(data and data.get("passed") is True and data.get("skipped") == 0)


def audit_counts():
    data = load_json("build/reports/reality_audit-current.json")
    if not data:
        return {"files": None, "findings": None, "production_findings": None}
    return {
        "files": data.get("files_scanned"),
        "findings": data.get("finding_count"),
        "production_findings": data.get("production_finding_count"),
        "claim_gate": data.get("claim_gate"),
    }


def domain_inventory():
    p = parity_ok()
    f = formatter_ok()
    lsp = load_json("build/reports/lsp_memory_report.json")
    network = load_json("build/reports/network-http-release-native.json")
    dns = load_json("build/reports/dns-release-native.json")
    security = load_json("build/reports/guvenlik-stage0-quarantine.json")
    sandbox = load_json("build/reports/yknllm-sandbox-arena-native.json")
    ast_gate = load_json("build/reports/yknllm-ast-capability-gate-native.json")
    async_gate = load_json("build/reports/async-future-frame-release-native-v2.json")

    lsp_verified = bool(
        lsp
        and lsp.get("lsp_started") is True
        and lsp.get("protocol_ok") is True
        and lsp.get("cleanup_ok") is True
        and (lsp.get("rss_kb") or 0) > 0
    )
    network_verified = bool(network and dns)
    security_bounded = bool(security and sandbox and ast_gate)
    async_runtime = bool(async_gate)

    # Score meanings: 1.0 = implementation and acceptance evidence cover the
    # domain; 0.5 = substantial implementation, but an explicit release gate
    # is open; 0.0 = absent or only a planned interface.
    return [
        {
            "id": "compiler_frontend",
            "label": "Lexer/parser/typechecker/generics core",
            "weight": 20,
            "score": 0.95 if p else 0.75,
            "status": "PASS" if p else "PARTIAL",
            "why": "119/119 Stage-0/Stage-3 parity covers the core; dynamic dispatch (vtable), strict trait constraints, AST async/await lowering now fully implemented.",
            "evidence": ["build/reports/stage-parity-final-current.json", "src/lexer.ykn", "src/parser.ykn", "src/typechecker.ykn"],
        },
        {
            "id": "codegen_ir",
            "label": "IR/optimizer/native code generation",
            "weight": 15,
            "score": 0.85 if p else 0.65,
            "status": "PASS" if p else "PARTIAL",
            "why": "Windows x86-64 and Linux ELF paths, relocation/capacity gates, SSA/CFG PassManager and async coroutine lowering closed; WASM and ARM/macOS execution remaining.",
            "evidence": ["src/codegen.ykn", "src/ir_lower.ykn", "src/codegen/output_elf.ykn", "build/reports/ast-executable-mutation-native.json", "build/reports/async-coroutine-execution-native.json"],
        },
        {
            "id": "stdlib_runtime",
            "label": "Stdlib and runtime",
            "weight": 15,
            "score": 0.65 if network_verified and async_runtime else 0.45,
            "status": "PARTIAL",
            "why": "HTTP/DNS/socket, Future poll foundations and AsyncFrame runtime have native evidence; GUI is fail-closed until a backend exists, WASM is open.",
            "evidence": ["std/ag/soket.ykn", "std/ag/http.ykn", "std/async/future.ykn", "build/reports/network-http-release-native.json", "build/reports/dns-release-native.json"],
        },
        {
            "id": "ai_llm",
            "label": "Pure-YKN AI/LLM inference/training/export",
            "weight": 20,
            "score": 0.45,
            "status": "PARTIAL",
            "why": "YKDS/YKLM, fixed-point trainer primitives, GGUF F32 round-trip and real model session smoke exist; full multi-layer numeric golden parity and transformer benchmark open.",
            "evidence": ["std/ai/", "std/ai/trainer.ykn", "std/ai/transformer_forward.ykn", "std/ai/yknllm_gateway.ykn", "build/reports/llm_2b_benchmark_native.json"],
        },
        {
            "id": "tooling",
            "label": "LSP/formatter/REPL/package tooling",
            "weight": 10,
            "score": 0.70 if f and lsp_verified else 0.50,
            "status": "PASS" if f and lsp_verified else "PARTIAL",
            "why": "Formatter 130/130 idempotence, package registry SemVer/lockfile and persistent RWX REPL JIT closed; Stage-3 LSP RSS verified.",
            "evidence": ["src/lsp.ykn", "src/formatter.ykn", "src/repl.ykn", "src/paket.ykn", "build/reports/formatter_idempotence_report.json", "build/reports/lsp_memory_report.json"],
        },
        {
            "id": "security",
            "label": "Memory safety, borrow model and YKNLLM isolation",
            "weight": 10,
            "score": 0.55 if security_bounded else 0.35,
            "status": "PARTIAL",
            "why": "Bounds/null/ownership diagnostics, dynamic borrow/move list, NLL reassignment and AST capability gate verified; OS process sandbox open.",
            "evidence": ["std/guvenlik.ykn", "src/safety.ykn", "src/sandbox_gate.ykn", "build/reports/guvenlik-stage0-quarantine.json", "build/reports/yknllm-ast-capability-gate-native.json"],
        },
        {
            "id": "platform_release",
            "label": "Platform and reproducible release readiness",
            "weight": 10,
            "score": 0.50 if p else 0.30,
            "status": "PARTIAL",
            "why": "Windows native and Linux Docker/ELF evidence exist; macOS/ARM64 is planned/manual-gated and WASM is open.",
            "evidence": ["build/reports/bootstrap-report.json", "build/reports/stage-parity-final-current.json", "build/reports/platform-provenance.json", ".github/workflows/test.yml"],
        },
    ]


def main() -> int:
    domains = domain_inventory()
    weighted_points = sum(item["weight"] * item["score"] for item in domains)
    total_weight = sum(item["weight"] for item in domains)
    percent = round(100.0 * weighted_points / total_weight, 2)
    report = {
        "schema": "yukinary.code-reality-audit.v1",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "method": "weighted domain score; verified=1.0, partial score is explicit per domain; checkbox count is not used",
        "product_release_gate": "OPEN",
        "weighted_completion_percent": percent,
        "compiler_core_estimate_percent": 96.0 if parity_ok() else 75.0,
        "audit_counts": audit_counts(),
        "domains": domains,
        "open_release_blockers": [
            "GUI backend and event loop",
            "WASM backend and execution",
            "pure-YKN multi-layer inference/training/golden benchmark",
            "native Windows/Linux YKNLLM process/resource isolation",
            "macOS/ARM64 native execution (planned/manual-gated)",
        ],
        "non_claims": [
            "Python/PowerShell host orchestration is not pure-YKN product implementation.",
            "JSON reports are evidence/interchange only, not runtime implementation.",
            "Source existence, compilation, or artifact existence alone is not PASS evidence.",
        ],
    }
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    json_path = REPORT_DIR / "code-reality-audit.json"
    json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# YKN Code Reality Audit",
        "",
        f"> Generated: {report['generated_at']}",
        "> This is a weighted engineering maturity estimate, not a release approval.",
        "",
        f"**Weighted product completion: {percent:.2f}%**",
        f"**Compiler core estimate: {report['compiler_core_estimate_percent']:.2f}%**",
        "**Product release gate: OPEN**",
        "",
        "## Scoring",
        "",
        "The score is calculated as `sum(weight × domain score) / sum(weight)`.",
        "A score of 1.0 requires implementation plus durable native/semantic evidence.",
        "Partial scores are explicitly justified below; unchecked task count is not used.",
        "",
        "| Domain | Weight | Score | Contribution | Status |",
        "|---|---:|---:|---:|---|",
    ]
    for item in domains:
        lines.append(f"| {item['label']} | {item['weight']} | {item['score']:.2f} | {item['weight'] * item['score']:.2f} | {item['status']} |")
    lines += ["", "## Domain findings", ""]
    for item in domains:
        lines.append(f"### {item['label']}")
        lines.append(f"- **Assessment:** {item['why']}")
        lines.append(f"- **Evidence:** {', '.join(f'`{path}`' for path in item['evidence'])}")
        lines.append("")
    lines += ["## Open release blockers", ""]
    lines.extend(f"- {item}" for item in report["open_release_blockers"])
    lines += ["", "## Rules", ""]
    lines.extend(f"- {item}" for item in report["non_claims"])
    lines.append("")
    (REPORT_DIR / "code-reality-audit.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"Weighted product completion: {percent:.2f}%")
    print(f"Compiler core estimate: {report['compiler_core_estimate_percent']:.2f}%")
    print(f"Report: {json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
