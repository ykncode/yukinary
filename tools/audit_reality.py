#!/usr/bin/env python3
"""Evidence-oriented scan for mock/stub/placeholder claims.

This is a detector, not a proof of correctness. It reports source evidence and
never turns a suspicious production match into PASS automatically.
"""

from __future__ import annotations

import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path

PATTERNS = {
    "mock": re.compile(r"\bmock\b|sahte|placeholder|dummy", re.I),
    "stub": re.compile(r"\bstub\b|iskelet|TODO|FIXME", re.I),
    # A guard such as `eger ptr == 0 ise { dondu 0; }` is not a stub.
    # Constant success is classified only by function_body_findings below.
    "constant_success": re.compile(r"dondu\s+0\s*;|return\s+0\s*;"),
    "fake_handle": re.compile(r"(?:g_\w*handle|handle)\s*=\s*1\s*;", re.I),
}
ANNOTATION = re.compile(r"reality-audit:\s*allow\s+class=([a-z_,]+)\s+reason=(.{8,})$", re.I)
SEMANTIC_CLASSES = {"empty_body", "constant_success_body"}
FUNCTION_START = re.compile(
    r"^[ \t]*(?:(?:pub|inline|static|extern|unsafe)[ \t]+)*(?:isv|fn|void|int|bool|long|float|double|auto|[A-Za-z_][\w:*&<>]*)[ \t]+([A-Za-z_]\w*)[ \t]*\([^;{}]*\)[ \t]*(?:->[^\n{]*?)?[ \t]*\{", 
    re.M,
)
DEFAULT_ROOTS = ("src", "std", "tools", "tests")
DEFAULT_EXTENSIONS = {".ykn", ".py", ".ps1", ".cpp", ".c", ".h"}


def function_body_findings(relative: str, lines: list[str]) -> list[dict]:
    """Find suspicious YKN/C-style function bodies; these are review signals, not proof."""
    source = "\n".join(lines)
    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.S)
    source = re.sub(r"//[^\n]*|#[^\n]*", "", source)
    results = []
    for match in FUNCTION_START.finditer(source):
        opening = match.end() - 1
        depth = 1
        closing = opening + 1
        while closing < len(source) and depth:
            if source[closing] == "{":
                depth += 1
            elif source[closing] == "}":
                depth -= 1
            closing += 1
        if depth:
            continue
        body = source[opening + 1:closing - 1].strip()
        line_number = source.count("\n", 0, match.start()) + 1
        name = match.group(1)
        if not body:
            results.append({"path": relative, "line": line_number, "class": "empty_body", "symbol": name, "text": "empty function body"})
        elif re.fullmatch(r"(?:dondu|return)\s+0\s*;", body):
            results.append({"path": relative, "line": line_number, "class": "constant_success_body", "symbol": name, "text": body})
    return results


def mask_non_code(text: str) -> str:
    """Mask comments and quoted strings while preserving line positions."""
    out = list(text)
    i = 0
    line_comment = False
    block_comment = False
    string_literal = False
    escaped = False
    while i < len(text):
        c = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if line_comment:
            if c == "\n":
                line_comment = False
            elif c != "\r":
                out[i] = " "
        elif block_comment:
            if c == "*" and nxt == "/":
                out[i] = " "
                out[i + 1] = " "
                i += 1
                block_comment = False
            elif c not in "\r\n":
                out[i] = " "
        elif string_literal:
            if c == "\n":
                # Unterminated strings are left for the syntax-aware review.
                string_literal = False
            elif c not in "\r":
                out[i] = " "
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == '"':
                string_literal = False
        elif c == "#":
            out[i] = " "
            line_comment = True
        elif c == "/" and nxt == "*":
            out[i] = " "
            out[i + 1] = " "
            i += 1
            block_comment = True
        elif c == '"':
            out[i] = " "
            string_literal = True
        elif c == "/" and nxt == "/":
            out[i] = " "
            out[i + 1] = " "
            i += 1
            line_comment = True
        i += 1
    return "".join(out)


def scan(root: Path, roots: tuple[str, ...]) -> dict:
    findings = []
    suppressions = []
    files_scanned = 0
    for relative_root in roots:
        directory = root / relative_root
        if not directory.exists():
            continue
        for path in directory.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in DEFAULT_EXTENSIONS:
                continue
            files_scanned += 1
            try:
                source_text = path.read_text(encoding="utf-8", errors="replace")
                lines = source_text.splitlines()
                code_lines = mask_non_code(source_text).splitlines()
            except OSError as exc:
                findings.append({"path": str(path.relative_to(root)), "line": 0, "class": "read_error", "text": str(exc)})
                continue
            relative = str(path.relative_to(root)).replace("\\", "/")
            for item in function_body_findings(relative, lines):
                source_line = lines[item["line"] - 1]
                annotation = ANNOTATION.search(source_line)
                allowed = {part.strip().lower() for part in annotation.group(1).split(",")} if annotation else set()
                reason = annotation.group(2).strip() if annotation else ""
                unknown = allowed - (set(PATTERNS) | SEMANTIC_CLASSES)
                if unknown:
                    findings.append({"path": relative, "line": item["line"], "class": "invalid_annotation", "text": f"unknown classes: {','.join(sorted(unknown))}"})
                    allowed.clear()
                if item["class"] in allowed and reason:
                    suppressions.append({**item, "reason": reason})
                else:
                    findings.append(item)
            for number, line in enumerate(code_lines, start=1):
                # Annotations and evidence text are reported from the original line;
                # matching itself is performed on code with comments/strings masked.
                original_line = lines[number - 1] if number <= len(lines) else line
                annotation = ANNOTATION.search(original_line)
                allowed = set()
                reason = ""
                if annotation:
                    allowed = {part.strip().lower() for part in annotation.group(1).split(",")}
                    reason = annotation.group(2).strip()
                    unknown = allowed - (set(PATTERNS) | SEMANTIC_CLASSES)
                    if unknown:
                        findings.append({"path": relative, "line": number, "class": "invalid_annotation", "text": f"unknown classes: {','.join(sorted(unknown))}"})
                        allowed.clear()
                for category, pattern in PATTERNS.items():
                    if pattern.search(line):
                        # Constant-success guards are intentionally classified only
                        # at function-body level, never for every defensive branch.
                        if category == "constant_success":
                            continue
                        item = {"path": relative, "line": number, "class": category, "text": original_line.strip()[:300]}
                        if category in allowed and reason:
                            suppressions.append({**item, "reason": reason})
                        else:
                            findings.append(item)
    production = [item for item in findings if item["path"].startswith(("src/", "std/"))]
    return {
        "schema": "yukinary.reality-audit.v1",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "roots": list(roots),
        "files_scanned": files_scanned,
        "finding_count": len(findings),
        "production_finding_count": len(production),
        "suppression_count": len(suppressions),
        "claim_gate": "REVIEW_REQUIRED" if production else "NO_UNSUPPRESSED_PRODUCTION_FINDINGS",
        "findings": findings,
        "suppressions": suppressions,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--output", default="build/reports/reality_audit.json")
    parser.add_argument("--strict", action="store_true", help="return non-zero while production findings need review")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    report = scan(root, DEFAULT_ROOTS)
    output = root / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Reality audit: {report['files_scanned']} files, {report['finding_count']} findings")
    print(f"Claim gate: {report['claim_gate']}")
    return 1 if args.strict and report["claim_gate"] == "REVIEW_REQUIRED" else 0


if __name__ == "__main__":
    raise SystemExit(main())
