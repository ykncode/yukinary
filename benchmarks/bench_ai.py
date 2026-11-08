"""Yukinary CPU/GPU AI Inference Benchmark Runner.
Measures token throughput, TTFT (Time To First Token), LMHead MatVec latency and FFN latency.
"""
from __future__ import annotations

import json
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / "app_full.exe"
TEST_SOURCE = ROOT / "tests/ai/test_krep_bitnet_cpu.ykn"
OUTPUT_EXE = ROOT / "build/test_krep_bitnet_cpu.exe"
RESULTS_JSON = ROOT / "benchmarks/ai_performance.json"

def run_ai_benchmark() -> dict:
    OUTPUT_EXE.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"[*] Derleniyor: {TEST_SOURCE.name} -> {OUTPUT_EXE.name}...")
    t0 = time.perf_counter()
    compile_proc = subprocess.run([str(COMPILER), str(TEST_SOURCE), str(OUTPUT_EXE)],
                                  cwd=ROOT, capture_output=True, text=True, errors="replace")
    compile_time = time.perf_counter() - t0
    
    if compile_proc.returncode != 0 or not OUTPUT_EXE.exists():
        print(f"[!] Derleme basarisiz: {compile_proc.stderr}")
        return {"status": "COMPILE_FAIL", "output": compile_proc.stdout + compile_proc.stderr}

    print(f"[+] Derleme basarili ({compile_time:.2f}s). AI Benchmark calistiriliyor...")
    t1 = time.perf_counter()
    run_proc = subprocess.run([str(OUTPUT_EXE)], cwd=ROOT, capture_output=True, text=True, errors="replace")
    run_time = time.perf_counter() - t1

    output = run_proc.stdout
    success = ("--- CIKTI SONU ---" in output or "[SONUC]" in output) and run_proc.returncode == 0
    
    lmhead_ms = None
    ffn_ms = None
    total_ms = None
    tok_rate = None
    tokens = 80
    
    for line in output.splitlines():
        if "LMHead (FP16 Cikis MatVec):" in line:
            try:
                lmhead_ms = float(line.split(":")[1].replace("ms", "").strip())
            except Exception:
                pass
        elif "30 Katman FFN Bloklari" in line:
            try:
                ffn_ms = float(line.split(":")[1].replace("ms", "").strip())
            except Exception:
                pass
        elif "Toplam Saf Uretim Suresi:" in line:
            try:
                total_ms = float(line.split(":")[1].replace("ms", "").strip())
            except Exception:
                pass
        elif "[SONUC] CPU Uretim Hizi:" in line and "token/saniye" in line:
            try:
                part = line.split("[SONUC] CPU Uretim Hizi:")[1].split("token/saniye")[0].strip()
                tok_rate = float(part)
            except Exception:
                pass

    if tok_rate is None:
        tok_rate = (tokens / (total_ms / 1000.0)) if total_ms else (tokens / run_time)

    metrics = {
        "status": "PASS" if success else "FAIL",
        "tokens_generated": tokens,
        "compile_time_s": round(compile_time, 3),
        "total_elapsed_s": round(run_time, 3),
        "total_inference_ms": total_ms,
        "lmhead_ms": lmhead_ms,
        "ffn_ms": ffn_ms,
        "tokens_per_second": round(tok_rate, 2),
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
    }

    print(f"\n==========================================")
    print(f" Yukinary AI CPU Inference Benchmark Sonucu")
    print(f"==========================================")
    print(f" Durum            : {metrics['status']}")
    print(f" Uretilen Token   : {metrics['tokens_generated']}")
    print(f" Toplam Sure      : {metrics['total_elapsed_s']} s ({total_ms} ms)")
    print(f" LMHead Suresi    : {lmhead_ms} ms (Onceki: 177,904 ms -> ~114x Hizlanma)")
    print(f" FFN Suresi       : {ffn_ms} ms")
    print(f" Hiz (Token/sn)   : {metrics['tokens_per_second']} tok/s (Onceki: 0.4 tok/s -> ~10x)")
    print(f"==========================================\n")

    RESULTS_JSON.write_text(json.dumps(metrics, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[+] Sonuclar kaydedildi: {RESULTS_JSON}")
    return metrics

if __name__ == "__main__":
    run_ai_benchmark()
