#!/usr/bin/env python3
"""
tests/convert/test_convert_matrix.py
Yukinary (YKN) 20-Library × 5-Fallback Convert Matrix Test — FAZ 9.2 - 9.5

Acceptance Criteria:
  1. 20 libraries across 5 languages (C, Python, JS, C#, Ruby)
  2. Only verified native conversions may produce YKN source
  3. Unimplemented fallback modes must return E038, never placeholder code
  4. Threshold decision engine determinism
  5. E036 syntax diagnostic handling
  6. SHA-256 cache is used only for successful deterministic conversions
  7. Transparency report distinguishes verified, rejected, and pending backends
"""

import os
import sys
import json
import subprocess
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
sys.path.insert(0, ROOT_DIR)

from tools.convert_engine import ConvertEngine, parse_directive_line

FIXTURES = [
    # 4 C Libraries
    {"lang": "C", "name": "libmath.c", "mode": "HARICI_STATIK", "esik": 0.8, "content": "int add(int a, int b) { return a + b; }\nint mul(int a, int b) { return a * b; }"},
    {"lang": "C", "name": "libio.c", "mode": "HARICI_STATIK", "esik": 0.9, "content": "int write_data(int a, int b) { fopen(\"test.dat\", \"w\"); return 0; }"},
    {"lang": "C", "name": "libdynamic.c", "mode": "HARICI_DINAMIK", "esik": 0.9, "content": "int sys_call(int a, int b) { system(\"ls\"); return 0; }"},
    {"lang": "C", "name": "libvector.c", "mode": "HARICI_STATIK", "esik": 0.5, "sadece": ["vec_add"], "content": "int vec_add(int a, int b) { return a + b; }\nint vec_sub(int a, int b) { return a - b; }"},

    # 4 Python Libraries
    {"lang": "PY", "name": "pystats.py", "mode": "PYTHON_EMBED", "esik": 0.8, "content": "def mean(a, b):\n    return (a + b) / 2\ndef variance(a, b):\n    return (a - b) ** 2"},
    {"lang": "PY", "name": "pynetwork.py", "mode": "PYTHON_EMBED", "esik": 0.9, "content": "def connect(host, port):\n    eval(\"socket.connect()\")\n    return 0"},
    {"lang": "PY", "name": "pystring.py", "mode": "PYTHON_EMBED", "esik": 0.7, "content": "def str_len(s, flag):\n    return len(s)"},
    {"lang": "PY", "name": "pyjson.py", "mode": "PYTHON_EMBED", "esik": 0.95, "content": "def parse_json(raw, mode):\n    exec(\"import json; json.loads(raw)\")\n    return 1"},

    # 4 JavaScript Libraries
    {"lang": "JS", "name": "jsformat.js", "mode": "JS_RPC_IPC", "esik": 0.8, "content": "function formatNumber(a, b) { return a + b; }\nfunction formatHex(a, b) { return a - b; }"},
    {"lang": "JS", "name": "jsipc.js", "mode": "JS_RPC_IPC", "esik": 0.95, "content": "function sendIpc(channel, msg) { eval(\"process.send(msg)\"); return 0; }"},
    {"lang": "JS", "name": "jsalgo.js", "mode": "JS_RPC_IPC", "esik": 0.7, "content": "function binarySearch(a, b) { return a + b; }"},
    {"lang": "JS", "name": "jsnetwork.js", "mode": "JS_RPC_IPC", "esik": 0.99, "content": "function wsConnect(url, opts) { eval(\"new WebSocket(url)\"); return 0; }"},

    # 4 C# Libraries
    {"lang": "CS", "name": "csmath.cs", "mode": "SHELL_SYSTEM", "esik": 0.8, "content": "public static int FastAdd(int a, int b) { return a + b; }\npublic static int FastMul(int a, int b) { return a * b; }"},
    {"lang": "CS", "name": "cssys.cs", "mode": "SHELL_SYSTEM", "esik": 0.9, "content": "public static int SpawnProcess(int a, int b) { system(\"cmd\"); return 0; }"},
    {"lang": "CS", "name": "csstruct.cs", "mode": "SHELL_SYSTEM", "esik": 0.6, "content": "public static int DotProduct(int a, int b) { return a * b; }"},
    {"lang": "CS", "name": "cscli.cs", "mode": "SHELL_SYSTEM", "esik": 0.99, "content": "public static int ExecTool(int a, int b) { system(\"run.exe\"); return 1; }"},

    # 4 Ruby Libraries
    {"lang": "RB", "name": "rbmath.rb", "mode": "HARICI_DINAMIK", "esik": 0.8, "content": "def calc_sum(a, b)\n  a + b\nend\ndef calc_diff(a, b)\n  a - b\nend"},
    {"lang": "RB", "name": "rbshell.rb", "mode": "SHELL_SYSTEM", "esik": 0.9, "content": "def run_cmd(a, b)\n  system(\"sh\")\n  0\nend"},
    {"lang": "RB", "name": "rbfilter.rb", "mode": "HARICI_DINAMIK", "esik": 0.7, "content": "def filter_range(a, b)\n  a + b\nend"},
    {"lang": "RB", "name": "rbdyn.rb", "mode": "HARICI_DINAMIK", "esik": 0.95, "content": "def load_plugin(a, b)\n  eval(\"require 'plugin'\")\n  1\nend"}
]

def run_matrix_tests(compiler_path: str, report_path: str):
    print("=" * 60)
    print("  YUKINARY MULTI-LANGUAGE CONVERT MATRIX (FAZ 9.2 - 9.5)")
    print("=" * 60)
    print(f"Compiler : {compiler_path}")
    
    work_dir = os.path.join(ROOT_DIR, "build", "test-work", "convert_fixtures")
    cache_dir = os.path.join(ROOT_DIR, ".ykn_cache", "convert")
    os.makedirs(work_dir, exist_ok=True)
    os.makedirs(cache_dir, exist_ok=True)

    # 1. Write sample library files
    print("\n[Step 1] Preparing 20 multi-language library fixtures...")
    for fix in FIXTURES:
        p = os.path.join(work_dir, fix["name"])
        with open(p, "w", encoding="utf-8") as f:
            f.write(fix["content"])
    print(f"  Created {len(FIXTURES)} fixture files in {work_dir}")

    # 2. Test E036 Syntax Diagnostics
    print("\n[Step 2] Testing E036 Syntax Diagnostic Rejection...")
    invalid_cases = [
        '#C-convert esik=0.8;', # missing kaynak
        '#PY-convert kaynak="test.py", esik=2.5;', # esik > 1.0
        '#JS-convert kaynak="test.js", mod="INVALID_MODE";', # invalid fallback mode
    ]
    diag_pass = 0
    for idx, inv in enumerate(invalid_cases):
        d = parse_directive_line(inv, idx + 1)
        if d and not d.is_valid and "E036" in d.error_message:
            diag_pass += 1
            print(f"  [PASS] Expected diagnostic: {d.error_message}")
        else:
            print(f"  [FAIL] Did not reject: {inv}")
    if diag_pass == len(invalid_cases):
        print("  E036 Diagnostic Tests: 3/3 PASS [OK]")
    else:
        print("  E036 Diagnostic Tests FAILED!")
        sys.exit(1)

    # 3. First Pass: Conversion & Verification
    print("\n[Step 3] Running First-Pass Conversion for 20 Libraries...")
    engine = ConvertEngine(cache_dir=cache_dir)
    results = []
    compile_success_count = 0
    expected_rejection_count = 0
    unexpected_result_count = 0

    for idx, fix in enumerate(FIXTURES):
        lang = fix["lang"]
        name = fix["name"]
        mode = fix["mode"]
        esik = fix["esik"]
        sadece = fix.get("sadece", [])

        sadece_str = f', sadece={json.dumps(sadece)}' if sadece else ''
        directive_line = f'#{lang}-convert kaynak="{name}", esik={esik}, mod="{mode}"{sadece_str};'

        directive = parse_directive_line(directive_line, idx + 1)
        if directive is None:
            raise AssertionError(f"generated directive could not be parsed: {directive_line}")
        success, generated_ykn, meta = engine.process_directive(directive, source_dir=work_dir)

        expected_native = mode == "HARICI_STATIK" and lang == "C" and name in {"libmath.c", "libvector.c"}
        if not success:
            expected_unsupported = not expected_native
            if expected_unsupported and meta.get("status") == "backend_not_implemented":
                expected_rejection_count += 1
                print(f"  [PASS] {name:<16} ({lang:>2}) -> E038 explicit unsupported; no placeholder emitted")
            else:
                unexpected_result_count += 1
                print(f"  [FAIL] Unexpected conversion rejection for {name}: {generated_ykn}")
            results.append({
                "fixture": name,
                "language": lang,
                "configured_mode": mode,
                "chosen_mode": meta.get("chosen_mode", "UNSUPPORTED"),
                "status": meta.get("status", "error"),
                "expected_native": expected_native,
                "compiled_ok": False,
                "placeholder_emitted": False,
                "cache_hit_pass1": meta.get("cache_hit", False)
            })
            continue

        if not expected_native:
            unexpected_result_count += 1
            print(f"  [FAIL] Unsupported backend unexpectedly emitted code for {name}")
            continue

        # Replace the inert generated entry point with a real call into every
        # converted fixture; a successful compile alone does not prove semantics.
        entry_stub = "islev ana() -> tamsayi {\n    dondu 0;\n}"
        if name == "libmath.c":
            runner_body = (
                "    eger add(19, 23) != 42 ise { dondu 1; }\n"
                "    eger mul(19, 23) != 437 ise { dondu 2; }\n"
                "    dondu 42;"
            )
        else:
            runner_body = "    dondu vec_add(19, 23);"
        executable_source = generated_ykn.replace(
            entry_stub,
            f"islev ana() -> tamsayi {{\n{runner_body}\n}}",
        )
        if executable_source == generated_ykn:
            unexpected_result_count += 1
            print(f"  [FAIL] {name:<16} ({lang:>2}) generated entry point contract changed")
            continue

        ykn_out_path = os.path.join(work_dir, f"{name}.ykn")
        exe_out_path = os.path.join(work_dir, f"{name}.exe")
        with open(ykn_out_path, "w", encoding="utf-8") as f:
            f.write(executable_source)

        cmd = [compiler_path, ykn_out_path, exe_out_path]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=30)
        compiled_ok = (res.returncode == 0) and ("HATA" not in res.stdout) and os.path.exists(exe_out_path)
        execution = None
        if compiled_ok:
            execution = subprocess.run([exe_out_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
        execution_ok = execution is not None and execution.returncode == 42

        if compiled_ok and execution_ok:
            compile_success_count += 1
            print(f"  [PASS] {name:<16} ({lang:>2}) -> native execution returned 42")
        else:
            unexpected_result_count += 1
            detail = f"compile={res.returncode}, execution={None if execution is None else execution.returncode}"
            print(f"  [FAIL] {name:<16} ({lang:>2}) verified conversion failed: {detail}")

        results.append({
            "fixture": name,
            "language": lang,
            "configured_mode": mode,
            "chosen_mode": meta["chosen_mode"],
            "success_ratio": meta["success_ratio"],
            "threshold": esik,
            "compiled_ok": compiled_ok,
            "execution_ok": execution_ok,
            "execution_exit_code": None if execution is None else execution.returncode,
            "placeholder_emitted": False,
            "cache_hit_pass1": meta["cache_hit"]
        })

    # 4. Second Pass: Cache Hit Verification
    print("\n[Step 4] Running Second-Pass Cache Verification (100% Cache Hit Target)...")
    cache_hits = 0
    rejected_again = 0
    for idx, fix in enumerate(FIXTURES):
        lang = fix["lang"]
        name = fix["name"]
        mode = fix["mode"]
        esik = fix["esik"]
        sadece = fix.get("sadece", [])
        sadece_str = f', sadece={json.dumps(sadece)}' if sadece else ''
        directive_line = f'#{lang}-convert kaynak="{name}", esik={esik}, mod="{mode}"{sadece_str};'

        directive = parse_directive_line(directive_line, idx + 1)
        if directive is None:
            raise AssertionError(f"generated directive could not be parsed: {directive_line}")
        success, generated_ykn, meta = engine.process_directive(directive, source_dir=work_dir)
        if meta.get("cache_hit"):
            cache_hits += 1
        elif not success and meta.get("status") == "backend_not_implemented":
            rejected_again += 1

    print(f"  Cache Hits (verified native only): {cache_hits}/2")
    print(f"  Explicit unsupported rejections: {rejected_again}/{len(FIXTURES) - 2}")

    # 5. Summary & Report
    print("\n" + "-" * 60)
    print("CONVERT MATRIX RESULTS SUMMARY:")
    print("-" * 60)
    print(f"Total Libraries Tested : {len(FIXTURES)} (4 C, 4 Python, 4 JS, 4 C#, 4 Ruby)")
    print(f"Verified native compilation: {compile_success_count}/2")
    print(f"Expected backend rejections: {expected_rejection_count}/{len(FIXTURES) - 2}")
    print(f"Unexpected results         : {unexpected_result_count}")
    print("Pending fallback modes    : HARICI_DINAMIK, PYTHON_EMBED, JS_RPC_IPC, SHELL_SYSTEM")
    print("-" * 60)

    report = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "compiler": compiler_path,
        "total_libraries": len(FIXTURES),
        "verified_native_compiled": compile_success_count,
        "expected_backend_rejections": expected_rejection_count,
        "unexpected_results": unexpected_result_count,
        "cache_hits_pass2": cache_hits,
        "rejected_again_pass2": rejected_again,
        "e036_diagnostics_verified": True,
        "fallback_modes_verified": [],
        "fallback_modes_pending": [
            "HARICI_DINAMIK", "PYTHON_EMBED", "JS_RPC_IPC", "SHELL_SYSTEM"
        ],
        "results": results
    }

    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    print(f"Saved Convert Matrix report: {report_path}")

    if compile_success_count != 2:
        print("\nFAIL: Verified native conversion count is not 2")
        sys.exit(1)
    if expected_rejection_count != len(FIXTURES) - 2 or rejected_again != len(FIXTURES) - 2:
        print("\nFAIL: Unsupported backends did not reject deterministically")
        sys.exit(1)
    if unexpected_result_count != 0 or cache_hits != 2:
        print("\nFAIL: Conversion matrix produced an unexpected result or cache state")
        sys.exit(1)

    print("\nCONVERT MATRIX HONEST-GATE PASSED: no mock or placeholder emitted [OK]")
    return 0

if __name__ == "__main__":
    compiler = os.path.join(ROOT_DIR, "build", "bootstrap", "stage3.exe")
    report = os.path.join(ROOT_DIR, "build", "reports", "convert_matrix_report.json")
    if len(sys.argv) > 1:
        compiler = sys.argv[1]
    sys.exit(run_matrix_tests(compiler, report))
