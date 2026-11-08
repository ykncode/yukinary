"""Run performance tests only; correctness results live in correctness.py."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

try:
    import psutil
except ImportError:  # optional peak-RSS instrumentation
    psutil = None

ROOT = Path(__file__).resolve().parents[1]
YKN_CASES = {
    "optimizer": ROOT / "tests/optimizer/02_advanced_opt.ykn",
    "generics": ROOT / "tests/generics/03_multi_specialization.ykn",
    "arrays": ROOT / "examples/cli/11_matris_carpimi.ykn",
    "sorting": ROOT / "examples/cli/16_quicksort_hizli_sirala.ykn",
    "ownership": ROOT / "tests/ownership/borrow_conflict_pos.ykn",
    "vectorization": ROOT / "tests/optimizer/04_loop_vectorization_unroll.ykn",
    "https_tls": ROOT / "tests/ag/test_https_schannel.ykn",
    "smart_pointers": ROOT / "tests/guvenlik/04_smart_pointers_pos.ykn",
    "concurrency": ROOT / "tests/async/04_async_actor_channel_pos.ykn",
    "native_gui": ROOT / "tests/gui/test_gui_engine.ykn",
    "pkg_integrity": ROOT / "tests/paket/test_package_checksum.ykn",
    "profiler": ROOT / "tests/profiler/test_profiler_sampling.ykn",
    "ssa_gvn_slp": ROOT / "tests/optimizer/05_ssa_gvn_slp.ykn",
    "nll_safety": ROOT / "tests/guvenlik/05_nll_early_drop.ykn",
    "async_reactor": ROOT / "tests/ag/test_reactor_scale.ykn",
    "wasm_riscv": ROOT / "tests/cross_target/test_wasm_riscv_emit.ykn",
    "vulkan_ai": ROOT / "tests/ai/test_vulkan_inference.ykn",
    "autograd_train": ROOT / "tests/ai/test_autograd_training.ykn",
    "acid_btree_wal": ROOT / "tests/veritabani/test_acid_wal.ykn",
    "debug_symbols": ROOT / "tests/debug/test_debug_symbols.ykn",
    "traits_vtable": ROOT / "tests/generics/04_traits_vtable_comptime.ykn",
    "flash_moe": ROOT / "tests/ai/test_flash_attn_moe.ykn",
    "lora_ft": ROOT / "tests/ai/test_lora_finetuning.ykn",
    "hnsw_vec": ROOT / "tests/veritabani/test_hnsw_vector.ykn",
    "ws_http2": ROOT / "tests/ag/test_websocket_http2.ykn",
    "font_raster": ROOT / "tests/gui/test_font_render.ykn",
    "sql_engine": ROOT / "tests/veritabani/test_sql_engine.ykn",
    "cli_tooling": ROOT / "tests/paket/test_cli_tooling.ykn",
    "raft_rpc": ROOT / "tests/dagitik/test_raft_consensus.ykn",
    "df_csv": ROOT / "tests/veri/test_dataframe_csv.ykn",
    "ecs_physics": ROOT / "tests/ecs/test_ecs_engine.ykn",
    "window_ctx": ROOT / "tests/gui/test_window_context.ykn",
    "desktop_apps": ROOT / "tests/apps/test_desktop_apps.ykn",
    "ws_live": ROOT / "tests/ag/test_websocket_live.ykn",
    "sql_str_btree": ROOT / "tests/veritabani/test_sql_parser_btree.ykn",
    "font_atlas": ROOT / "tests/gui/test_font_atlas.ykn",
    "cli_fmt": ROOT / "tests/paket/test_cli_commands.ykn",
    "baremetal_os": ROOT / "tests/os/test_baremetal_kernel.ykn",
    "runtime": ROOT / "benchmarks/runtime.ykn",
}
WORKLOAD = ROOT / "benchmarks/rivals"
LOCAL_TOOLCHAINS = {
    "rustc": Path.home() / ".cargo/bin/rustc.exe",
    "go": Path.home() / "AppData/Local/Programs/go/bin/go.exe",
}

def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(args: list[str], timeout: int = 300) -> tuple[subprocess.CompletedProcess[str], float]:
    start = time.perf_counter()
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True,
                            errors="replace", timeout=timeout)
    return result, time.perf_counter() - start


def peak_rss(args: list[str], timeout: int = 300) -> int | None:
    if psutil is None:
        return None
    proc = subprocess.Popen(args, cwd=ROOT, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    watched = psutil.Process(proc.pid)
    peak = 0
    start = time.monotonic()
    while proc.poll() is None:
        try:
            peak = max(peak, watched.memory_info().rss)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass
        if time.monotonic() - start > timeout:
            proc.kill()
            proc.wait()
            raise TimeoutError(f"memory probe timed out: {args[0]}")
        time.sleep(0.002)
    return peak or None


def summarize(samples: list[float]) -> dict[str, Any]:
    ordered = sorted(samples)
    nearest_rank = ordered[max(0, __import__("math").ceil(0.95 * len(ordered)) - 1)]
    return {"samples_s": samples, "sample_count": len(samples),
            "median_s": statistics.median(ordered), "min_s": ordered[0],
            "max_s": ordered[-1], "stdev_s": statistics.stdev(ordered) if len(ordered) > 1 else 0.0,
            "p95_nearest_rank_s": nearest_rank,
            "p95_interpretation": "nearest-rank; highly uncertain with fewer than 20 measured samples"}


def validate_ykn_output(name: str, output: str) -> bool:
    if name == "runtime":
        return "RUNTIME-BENCH-PASS" in output
    if name == "optimizer":
        return "ADVANCED-OPTIMIZER-PASS" in output
    if name == "vectorization":
        return "LOOP-VECTORIZATION-PASS" in output
    if name == "https_tls":
        return "HTTPS-SCHANNEL-PASS" in output
    if name == "smart_pointers":
        return "SMART_POINTERS_PASS" in output
    if name == "concurrency":
        return "ASYNC_ACTOR_CHANNEL_PASS" in output
    if name == "native_gui":
        return "NATIVE_GUI_ENGINE_PASS" in output
    if name == "pkg_integrity":
        return "PACKAGE_INTEGRITY_PASS" in output
    if name == "profiler":
        return "PROFILER_SAMPLING_PASS" in output
    if name == "ssa_gvn_slp":
        return "SSA_GVN_SLP_PASS" in output
    if name == "nll_safety":
        return "NLL_EARLY_DROP_PASS" in output
    if name == "async_reactor":
        return "CROSS_PLATFORM_REACTOR_PASS" in output
    if name == "wasm_riscv":
        return "WASM_RISCV_EMIT_PASS" in output
    if name == "vulkan_ai":
        return "VULKAN_SPIRV_INFERENCE_PASS" in output
    if name == "autograd_train":
        return "AUTOGRAD_TENSOR_TRAIN_PASS" in output
    if name == "acid_btree_wal":
        return "ACID_WAL_BTREE_PASS" in output
    if name == "debug_symbols":
        return "DEBUG_SYMBOLS_PASS" in output
    if name == "traits_vtable":
        return "TRAITS_VTABLE_COMPTIME_PASS" in output
    if name == "flash_moe":
        return "FLASH_ATTN_MOE_PASS" in output
    if name == "lora_ft":
        return "LORA_FINETUNING_PASS" in output
    if name == "hnsw_vec":
        return "HNSW_VECTOR_DB_PASS" in output
    if name == "ws_http2":
        return "WEBSOCKET_HTTP2_PASS" in output
    if name == "font_raster":
        return "FONT_RASTERIZER_PASS" in output
    if name == "sql_engine":
        return "SQL_ENGINE_PASS" in output
    if name == "cli_tooling":
        return "CLI_TOOLING_PASS" in output
    if name == "raft_rpc":
        return "RAFT_CONSENSUS_RPC_PASS" in output
    if name == "df_csv":
        return "COLUMNAR_DATAFRAME_CSV_PASS" in output
    if name == "ecs_physics":
        return "ECS_SOA_PHYSICS_PASS" in output
    if name == "window_ctx":
        return "WINDOW_CONTEXT_PASS" in output
    if name == "desktop_apps":
        return "DESKTOP_APPS_ENGINE_PASS" in output
    if name == "ws_live":
        return "WEBSOCKET_HANDSHAKE_LIVE_PASS" in output
    if name == "sql_str_btree":
        return "SQL_TOKENIZER_BTREE_PASS" in output
    if name == "font_atlas":
        return "FONT_TEXTURE_ATLAS_PASS" in output
    if name == "cli_fmt":
        return "CLI_COMMANDS_LSP_FMT_PASS" in output
    if name == "baremetal_os":
        return "BAREMETAL_KERNEL_PASS" in output
    if name == "generics":
        return "TEST 03 GENERICS: MULTI-SPECIALIZATION OK" in output
    if name == "ownership":
        return "BORROW_CONFLICT_POS: OK" in output
    if name == "arrays":
        numbers = [int(token) for token in output.split() if token.lstrip("-+").isdigit()]
        return numbers[-9:] == [2, 4, 6, 8, 10, 12, 14, 16, 18]
    if name == "sorting":
        lines = [line for line in output.splitlines() if "Sirali Dizi" in line]
        if not lines:
            return False
        sorted_line_index = output.splitlines().index(lines[-1]) + 1
        if sorted_line_index >= len(output.splitlines()):
            return False
        numbers = [int(token) for token in output.splitlines()[sorted_line_index].split()
                   if token.lstrip("-+").isdigit()]
        return numbers == [5, 11, 12, 22, 25, 34, 64, 71, 88, 90]
    return False


def checked_json(result: subprocess.CompletedProcess[str]) -> bool:
    try:
        output = json.loads(result.stdout.strip().splitlines()[-1])
        return result.returncode == 0 and output == {
            "status": "PASS", "checksum": 1277871, "fibonacci": 102334155}
    except (json.JSONDecodeError, IndexError):
        return False


def run_competitor(name: str, command: list[str], exe: Path, build_command: list[str],
                   build_flags: list[str], repeats: int, warmups: int) -> dict[str, Any]:
    build_times: list[float] = []
    for index in range(warmups + repeats):
        built, elapsed = run(build_command)
        if built.returncode != 0 or not exe.is_file():
            return {"status": "BUILD_FAIL", "compiler": command[0],
                    "build_stdout": built.stdout[-1000:], "build_stderr": built.stderr[-1000:]}
        if index >= warmups:
            build_times.append(elapsed)
    timed: list[float] = []
    for index in range(warmups + repeats):
        result, elapsed = run([str(exe)])
        if not checked_json(result):
            return {"status": "WRONG_RESULT", "stdout": result.stdout[-1000:],
                    "stderr": result.stderr[-1000:], "exit_code": result.returncode}
        if index >= warmups:
            timed.append(elapsed)
    version, _ = run(command, timeout=20)
    return {"status": "PASS", "compiler": command[0],
            "compiler_version": (version.stdout or version.stderr).splitlines()[:1],
            "build_flags": build_flags, "build": summarize(build_times),
            "run": summarize(timed), "binary_bytes": exe.stat().st_size,
            "peak_rss_bytes": peak_rss([str(exe)])}


def _git_revision() -> str | None:
    try:
        result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                capture_output=True, text=True, timeout=5)
        return result.stdout.strip() if result.returncode == 0 else None
    except (OSError, subprocess.TimeoutExpired):
        return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="app_full.exe")
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--with-ai", action="store_true", help="Run CPU AI benchmark")
    parser.add_argument("--json", type=Path, default=ROOT / "benchmarks/performance.json")
    args = parser.parse_args()
    compiler = Path(args.compiler)
    if not compiler.is_absolute():
        compiler = ROOT / compiler
    if not compiler.is_file() or args.repeats < 3 or args.warmups < 1:
        parser.error("compiler must exist; repeats >= 3 and warmups >= 1")

    report: dict[str, Any] = {
        "schema": "ykn.performance.v2", "compiler": str(compiler),
        "compiler_sha256": sha256(compiler), "compiler_flags": [],
        "repeats": args.repeats, "warmups": args.warmups,
        "timer": "perf_counter; process startup included",
        "source_revision": _git_revision(),
        "source_hashes": {str(path.relative_to(ROOT)): sha256(path) for path in [*YKN_CASES.values(), ROOT / "benchmarks/runtime.ykn", *WORKLOAD.glob("workload.*"), ROOT / "benchmarks/rival_python.py"] if path.is_file()},
        "measurement_protocol": {"percentile": "nearest-rank P95; highly uncertain below 20 samples", "raw_samples": "stored per case", "peak_rss_sampling_interval_ms": 2, "peak_rss_limitation": "short-lived peaks may be missed"},
        "environment": {"os": platform.platform(), "machine": platform.machine(),
                        "processor": platform.processor(), "cpu_count": os.cpu_count(),
                        "python": sys.version, "peak_rss_probe": "psutil 2ms polling" if psutil else "unavailable"},
        "ykn": {}, "rivals": {}}

    with tempfile.TemporaryDirectory(prefix="ykn-bench-") as temporary:
        temp = Path(temporary)
        for name, source in YKN_CASES.items():
            exe = temp / f"{name}.exe"
            compile_times: list[float] = []
            run_times: list[float] = []
            failure = None
            for iteration in range(args.warmups + args.repeats):
                built, compile_elapsed = run([str(compiler), str(source), str(exe)])
                if built.returncode != 0 or not exe.is_file():
                    failure = {"stage": "compile", "exit_code": built.returncode,
                               "output": (built.stdout + built.stderr)[-2000:]}
                    break
                executed, elapsed = run([str(exe)])
                if executed.returncode != 0 or not validate_ykn_output(name, executed.stdout):
                    failure = {"stage": "run", "exit_code": executed.returncode,
                               "output": (executed.stdout + executed.stderr)[-2000:]}
                    break
                if iteration >= args.warmups:
                    compile_times.append(compile_elapsed)
                    run_times.append(elapsed)
            case: dict[str, Any] = {"source": str(source.relative_to(ROOT)),
                                    "source_sha256": sha256(source),
                                    "status": "FAIL" if failure else "PASS",
                                    "correctness_validation": "expected output marker plus exit code" if name != "runtime" else "checksum and Fibonacci assertions, then pass marker"}
            if failure:
                case["failure"] = failure
            else:
                case.update({"compile": summarize(compile_times), "run": summarize(run_times),
                             "binary_bytes": exe.stat().st_size,
                             "compile_peak_rss_bytes": peak_rss([str(compiler), str(source), str(exe)]),
                             "run_peak_rss_bytes": peak_rss([str(exe)])})
            report["ykn"][name] = case
            print(f"YKN {name:10} {case['status']}" + (f" compile={case['compile']['median_s']*1000:.2f}ms run={case['run']['median_s']*1000:.2f}ms size={case.get('binary_bytes','?')}B" if not failure else ""))

        rival_specs = [
            ("gcc-O0", "C", "gcc", ["-O0", str(WORKLOAD / "workload.c"), "-o"]),
            ("gcc-O3", "C", "gcc", ["-O3", str(WORKLOAD / "workload.c"), "-o"]),
            ("g++-O0", "C++", "g++", ["-O0", "-std=c++20", str(WORKLOAD / "workload.cpp"), "-o"]),
            ("g++-O3", "C++", "g++", ["-O3", "-std=c++20", str(WORKLOAD / "workload.cpp"), "-o"]),
            ("clang-O0", "C", "clang", ["-O0", str(WORKLOAD / "workload.c"), "-o"]),
            ("clang-O3", "C", "clang", ["-O3", str(WORKLOAD / "workload.c"), "-o"]),
            ("clang++-O0", "C++", "clang++", ["-O0", "-std=c++20", str(WORKLOAD / "workload.cpp"), "-o"]),
            ("clang++-O3", "C++", "clang++", ["-O3", "-std=c++20", str(WORKLOAD / "workload.cpp"), "-o"]),
            ("cl-Od", "C++", "cl", ["/Od"]),
            ("cl-O2", "C++", "cl", ["/O2"]),
            ("rustc-O0", "Rust", "rustc", ["-C", "opt-level=0", str(WORKLOAD / "workload.rs"), "-o"]),
            ("rustc-O3", "Rust", "rustc", ["-C", "opt-level=3", str(WORKLOAD / "workload.rs"), "-o"]),
            ("go-default", "Go", "go", []),
        ]
        for label, language, tool, flags in rival_specs:
            path = shutil.which(tool)
            if not path and tool in LOCAL_TOOLCHAINS and LOCAL_TOOLCHAINS[tool].is_file():
                path = str(LOCAL_TOOLCHAINS[tool])
            if not path:
                report["rivals"][label] = {"status": "UNAVAILABLE", "reason": f"{tool} not installed"}
                continue
            executable = temp / f"rival_{label}.exe"
            if tool in {"gcc", "g++", "clang", "clang++", "rustc"}:
                build = [path, *flags, str(executable)]
                version_args = [path, "--version"]
            elif tool == "cl":
                build = [path, "/nologo", *flags, str(WORKLOAD / "workload.cpp"), f"/Fe:{executable}"]
                version_args = [path]
            else:
                build = [path, "build", "-trimpath", "-o", str(executable), str(WORKLOAD / "workload.go")]
                version_args = [path, "version"]
                flags = ["go build", "default optimizations"]
            report["rivals"][label] = run_competitor(language, version_args, executable,
                                                        build, flags, args.repeats, args.warmups)

        # FAZ 22: Cross-Platform Conformance Test (ELF64 & Mach-O ARM64)
        cross_res = subprocess.run([sys.executable, str(ROOT / "tests/cross_target/test_cross_conformance.py")],
                                   cwd=ROOT, capture_output=True, text=True, errors="replace")
        if cross_res.returncode == 0 and "CROSS-PLATFORM-CONFORMANCE-PASS" in cross_res.stdout:
            report["cross_platform"] = {"status": "PASS", "targets": ["linux-x64", "macos-x64", "macos-arm64"]}
            print("CROSS_TARGET (Linux ELF64, macOS Mach-O x64/ARM64) PASS")
        else:
            report["cross_platform"] = {"status": "FAIL", "output": cross_res.stdout + cross_res.stderr}
            print(f"CROSS_TARGET FAIL: {cross_res.stdout + cross_res.stderr}")

        # FAZ 28: Normatif Conformance Suiti (56/56 Dil Standardi Dogrulamasi)
        conf_res = subprocess.run([sys.executable, str(ROOT / "tests/conformance/run_all_conformance.py")],
                                  cwd=ROOT, capture_output=True, text=True, errors="replace")
        if conf_res.returncode == 0 and "CONFORMANCE-SUITE-PASS" in conf_res.stdout:
            report["conformance_suite"] = {"status": "PASS", "tested_count": 56}
            print("CONFORMANCE_SUITE (56/56 Normative Spec & E2E Suites) PASS")
        else:
            report["conformance_suite"] = {"status": "FAIL", "output": conf_res.stdout + conf_res.stderr}
            print(f"CONFORMANCE_SUITE FAIL: {conf_res.stdout + conf_res.stderr}")

        # FAZ 28: 3-Asamali Bootstrap Fixpoint & Self-Host Dogrulamasi
        boot_res = subprocess.run([sys.executable, str(ROOT / "tests/bootstrap/test_bootstrap_fixpoint.py")],
                                  cwd=ROOT, capture_output=True, text=True, errors="replace")
        if boot_res.returncode == 0 and "BOOTSTRAP-FIXPOINT-PASS" in boot_res.stdout:
            report["bootstrap_fixpoint"] = {"status": "PASS", "stage1_selfhost": "VERIFIED"}
            print("BOOTSTRAP_FIXPOINT (Stage-0 -> Stage-1 Self-Host Parity) PASS")
        else:
            report["bootstrap_fixpoint"] = {"status": "FAIL", "output": boot_res.stdout + boot_res.stderr}
            print(f"BOOTSTRAP_FIXPOINT FAIL: {boot_res.stdout + boot_res.stderr}")

        python_times: list[float] = []
        python_command = [sys.executable, str(ROOT / "benchmarks/rival_python.py")]
        python_failure = None
        for index in range(args.warmups + args.repeats):
            result, elapsed = run(python_command)
            if not checked_json(result):
                python_failure = {"stdout": result.stdout[-1000:], "stderr": result.stderr[-1000:],
                                  "exit_code": result.returncode}
                break
            if index >= args.warmups:
                python_times.append(elapsed)
        if python_failure:
            report["rivals"]["Python"] = {"status": "WRONG_RESULT", **python_failure}
        else:
            report["rivals"]["Python"] = {
                "status": "PASS", "compiler_version": platform.python_version(),
                "build_flags": ["interpreted, CPython default"], "build": None,
                "run": summarize(python_times), "binary_bytes": None,
                "peak_rss_bytes": peak_rss(python_command),
                "process_startup_included": True}
            print(f"Python rival PASS run={statistics.median(python_times)*1000:.2f}ms")

    if args.with_ai:
        from benchmarks.bench_ai import run_ai_benchmark
        report['ai_inference'] = run_ai_benchmark()
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Performance report: {args.json}")
    return 1 if any(c["status"] != "PASS" for c in report["ykn"].values()) else 0


if __name__ == "__main__":
    raise SystemExit(main())
