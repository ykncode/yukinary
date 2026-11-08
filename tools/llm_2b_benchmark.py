#!/usr/bin/env python3
"""Measure the real GGUF compiler/session and #prompt pipeline.

This is a wall-clock acceptance benchmark, not a claim of full model-quality
parity. It separates model session/load, prompt forward attempt, safe rejection,
and generated-binary execution so a slow or stalled stage is visible.
"""
from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import re
import sys
import time
from pathlib import Path
from typing import Any


MODEL_DEFAULT = "models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf"
SESSION_SOURCE_DEFAULT = "tests/mini_hello.ykn"
PROMPT_SOURCE_DEFAULT = "tests/llm/11_prompt_cli_integration.ykn"


def run_process(command: list[str], root: Path, timeout: float) -> dict[str, Any]:
    started = time.perf_counter()
    stdout = ""
    stderr = ""
    try:
        completed = subprocess.run(
            command,
            cwd=root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
        )
        timed_out = False
    except subprocess.TimeoutExpired as error:
        completed = None
        timed_out = True
        stdout = error.stdout or ""
        stderr = error.stderr or ""
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    if completed is not None:
        stdout = completed.stdout
        stderr = completed.stderr
        exit_code = completed.returncode
    else:
        exit_code = -124
    return {
        "command": command,
        "elapsed_ms": round(elapsed_ms, 2),
        "exit_code": exit_code,
        "timed_out": timed_out,
        "stdout": stdout,
        "stderr": stderr,
    }


def measure_case(
    root: Path,
    compiler: Path,
    model: Path,
    source: Path,
    output: Path,
    max_tokens: int,
    rag_context: Path | None,
    timeout: float,
    pass_max_tokens: bool,
) -> dict[str, Any]:
    output.unlink(missing_ok=True)
    command = [str(compiler), "--llm-model", str(model)]
    if rag_context is not None:
        command.extend(["--llm-rag-context", str(rag_context)])
    if pass_max_tokens:
        command.extend(["--llm-max-tokens", str(max_tokens)])
    command.extend([str(source), str(output)])
    compile_result = run_process(command, root, timeout)
    text = compile_result["stdout"] + compile_result["stderr"]
    artifact = output.exists()
    metric_match = re.search(r"\[AI-METRIC\](.*)", text)
    metrics: dict[str, Any] = {}
    if metric_match:
        for key, value in re.findall(r"([a-z_]+)=(-?\d+)", metric_match.group(1)):
            metrics[key] = int(value)
        decode_ns = metrics.get("decode_ns", 0)
        output_tokens = metrics.get("output_tokens", 0)
        metrics["decode_ms_per_token"] = (
            round(decode_ns / 1_000_000.0 / output_tokens, 4)
            if decode_ns > 0 and output_tokens > 0 else None
        )
        metrics["decode_tokens_per_sec"] = (
            round(output_tokens * 1_000_000_000.0 / decode_ns, 4)
            if decode_ns > 0 and output_tokens > 0 else None
        )
    result: dict[str, Any] = {
        "source": str(source),
        "artifact": artifact,
        "compile": {
            "elapsed_ms": compile_result["elapsed_ms"],
            "exit_code": compile_result["exit_code"],
            "timed_out": compile_result["timed_out"],
        },
        "model_loaded": (
            "LLM modeli yuklendi" in text
            or "GGUF v3 modeli" in text
            or "GGUF v3 modeli do" in text
        ),
        "forward_attempted": "[AI] #prompt cozumleniyor" in text,
        "safe_prompt_rejection": "[AI] #prompt reddedildi" in text,
        "metrics": metrics,
        "output_excerpt": text[-4000:],
    }
    if artifact and compile_result["exit_code"] == 0:
        run_result = run_process([str(output)], root, timeout)
        result["binary_run"] = {
            "elapsed_ms": run_result["elapsed_ms"],
            "exit_code": run_result["exit_code"],
            "timed_out": run_result["timed_out"],
            "stdout": run_result["stdout"],
            "stderr": run_result["stderr"],
        }
    else:
        result["binary_run"] = None
    output.unlink(missing_ok=True)
    return result


def summarize(cases: list[dict[str, Any]]) -> dict[str, Any]:
    timings = [case["compile"]["elapsed_ms"] for case in cases if not case["compile"]["timed_out"]]
    metric_cases = [case["metrics"] for case in cases if case.get("metrics")]
    def metric_median(name: str) -> float | None:
        values = [item[name] for item in metric_cases if isinstance(item.get(name), (int, float))]
        return round(statistics.median(values), 4) if values else None
    return {
        "runs": len(cases),
        "compile_ms_min": round(min(timings), 2) if timings else None,
        "compile_ms_median": round(statistics.median(timings), 2) if timings else None,
        "compile_ms_max": round(max(timings), 2) if timings else None,
        "model_loaded_runs": sum(1 for case in cases if case["model_loaded"]),
        "forward_attempt_runs": sum(1 for case in cases if case["forward_attempted"]),
        "safe_rejection_runs": sum(1 for case in cases if case["safe_prompt_rejection"]),
        "artifact_runs": sum(1 for case in cases if case["artifact"]),
        "tokenize_ms_median": (round(metric_median("tokenize_ns") / 1_000_000.0, 4)
                                if metric_median("tokenize_ns") is not None else None),
        "prefill_ms_median": (round(metric_median("prefill_ns") / 1_000_000.0, 4)
                               if metric_median("prefill_ns") is not None else None),
        "decode_ms_median": (round(metric_median("decode_ns") / 1_000_000.0, 4)
                              if metric_median("decode_ns") is not None else None),
        "total_inference_ms_median": (round(metric_median("total_ns") / 1_000_000.0, 4)
                                       if metric_median("total_ns") is not None else None),
        "decode_ms_per_token_median": metric_median("decode_ms_per_token"),
        "decode_tokens_per_sec_median": metric_median("decode_tokens_per_sec"),
        "peak_rss_bytes_median": metric_median("peak_rss"),
        "input_tokens_median": metric_median("input_tokens"),
        "output_tokens_median": metric_median("output_tokens"),
        "native_simd_runs": sum(1 for item in metric_cases if item.get("native_simd") == 1),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, default=None)

    parser.add_argument("--model", type=Path, default=None)
    parser.add_argument("--session-source", type=Path, default=None)
    parser.add_argument("--prompt-source", type=Path, default=None)
    parser.add_argument("--rag-context", type=Path, default=None)
    parser.add_argument("--max-tokens", type=int, default=1)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--prompt", action="store_true", help="also benchmark the real #prompt case")
    parser.add_argument(
        "--require-positive-prompt", action="store_true",
        help="#prompt case must produce and run a binary; safe rejection fails",
    )
    parser.add_argument(
        "--pass-max-tokens",
        action="store_true",
        help="compiler destekliyorsa --llm-max-tokens bayrağını da geçir",
    )
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()
    if not 1 <= args.max_tokens <= 64:
        parser.error("--max-tokens 1..64 aralığında olmalı")
    if args.repeats < 1:
        parser.error("--repeats pozitif olmalı")

    root = Path(__file__).resolve().parents[1]
    compiler = args.compiler or root / "build" / "final_llm_compiler.exe"
    model = args.model or root / MODEL_DEFAULT
    session_source = args.session_source or root / SESSION_SOURCE_DEFAULT
    prompt_source = args.prompt_source or root / PROMPT_SOURCE_DEFAULT
    if not compiler.exists():
        raise FileNotFoundError(f"compiler bulunamadı: {compiler}")
    if not model.exists():
        raise FileNotFoundError(f"model bulunamadı: {model}")
    if not session_source.exists():
        raise FileNotFoundError(f"session source bulunamadı: {session_source}")
    if args.prompt and not prompt_source.exists():
        raise FileNotFoundError(f"prompt source bulunamadı: {prompt_source}")
    rag_context = args.rag_context.resolve() if args.rag_context else None
    if rag_context is not None and not rag_context.exists():
        raise FileNotFoundError(f"RAG context bulunamadı: {rag_context}")

    work = root / "build" / "test-work" / "llm-2b-benchmark"
    work.mkdir(parents=True, exist_ok=True)
    cases: dict[str, list[dict[str, Any]]] = {"session": []}
    for index in range(args.repeats):
        cases["session"].append(
            measure_case(
                root,
                compiler,
                model,
                session_source,
                work / f"session-{index}.exe",
                args.max_tokens,
                rag_context,
                args.timeout,
                args.pass_max_tokens,
            )
        )
    if args.prompt:
        cases["prompt"] = []
        for index in range(args.repeats):
            cases["prompt"].append(
                measure_case(
                    root,
                    compiler,
                    model,
                    prompt_source,
                    work / f"prompt-{index}.exe",
                    args.max_tokens,
                    rag_context,
                    args.timeout,
                    args.pass_max_tokens,
                )
            )

    report: dict[str, Any] = {
        "schema": "yukinary.llm-2b-benchmark.v1",
        "compiler": str(compiler),
        "model": str(model),
        "model_bytes": model.stat().st_size,
        "max_tokens": args.max_tokens,
        "timeout_seconds": args.timeout,
        "cases": {name: {"summary": summarize(values), "runs": values} for name, values in cases.items()},
    }
    output = args.output or root / "build" / "reports" / "llm_2b_benchmark.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    failures: list[str] = []
    session_runs = cases.get("session", [])
    for index, case in enumerate(session_runs):
        if not case["model_loaded"]:
            failures.append(f"session[{index}]: model yüklenmedi")
        if case["compile"]["exit_code"] != 0 or not case["artifact"]:
            failures.append(f"session[{index}]: compiler artifact üretmedi")
    if "prompt" in cases:
        for index, case in enumerate(cases["prompt"]):
            if not case["model_loaded"]:
                failures.append(f"prompt[{index}]: model yüklenmedi")
            if not case["forward_attempted"]:
                failures.append(f"prompt[{index}]: #prompt forward aşamasına ulaşmadı")
            if args.require_positive_prompt and (case["compile"]["exit_code"] != 0 or
                                                   not case["artifact"] or
                                                   case["safe_prompt_rejection"] or
                                                   not case.get("binary_run") or
                                                   case["binary_run"]["exit_code"] != 0):
                failures.append(f"prompt[{index}]: pozitif artifact/binary acceptance başarısız")

    for name, values in cases.items():
        summary = summarize(values)
        print(
            f"{name}: runs={summary['runs']} "
            f"compile_ms={summary['compile_ms_min']}/{summary['compile_ms_median']}/{summary['compile_ms_max']} "
            f"model_loaded={summary['model_loaded_runs']} "
            f"forward={summary['forward_attempt_runs']} "
            f"safe_rejection={summary['safe_rejection_runs']} "
            f"artifact={summary['artifact_runs']} "
            f"prefill_ms={summary['prefill_ms_median']} "
            f"decode_ms={summary['decode_ms_median']} "
            f"tok_s={summary['decode_tokens_per_sec_median']} "
            f"peak_rss={summary['peak_rss_bytes_median']}"
        )
    print(f"LLM 2B BENCHMARK REPORT: {output}")
    if failures:
        print("LLM 2B BENCHMARK FAIL: " + "; ".join(failures), file=sys.stderr)
        return 1
    print("LLM 2B BENCHMARK PASS: model session and requested pipeline stages measured.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"LLM 2B BENCHMARK FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
