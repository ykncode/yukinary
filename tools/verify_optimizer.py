import json
import subprocess
import sys

print("=== OPTIMIZER VERIFICATION ===")
# 1. Run all optimizer test files directly with stage0.exe
tests = [
    ("tests/optimizer/02_advanced_opt.ykn", "ADVANCED-OPTIMIZER-PASS"),
    ("tests/optimizer/test_inlining_and_unroll.ykn", ""),
    ("tests/optimizer/03_inlining_and_unroll_deep.ykn", "INLINING-AND-UNROLLING-DEEP-PASS"),
    ("tests/optimizer/test_stage0_ssa_cfg.ykn", "STAGE0_SSA_CFG_OPTIMIZER: OK"),
]

all_ok = True
for src, expected in tests:
    exe = "build/test_verify_opt.exe"
    r1 = subprocess.run(["build/bootstrap/stage0.exe", src, exe], capture_output=True, text=True)
    if r1.returncode != 0:
        print(f"[FAIL COMPILATION] {src}: {r1.stderr}")
        all_ok = False
        continue
    r2 = subprocess.run([exe], capture_output=True, text=True)
    if r2.returncode != 0:
        print(f"[FAIL RUN EXIT {r2.returncode}] {src}")
        all_ok = False
        continue
    if expected and expected not in r2.stdout:
        print(f"[FAIL OUTPUT] {src}: expected '{expected}' not in '{r2.stdout.strip()}'")
        all_ok = False
        continue
    print(f"[PASS] {src} => OK (exit={r2.returncode})")

if all_ok:
    print("ALL OPTIMIZER TESTS VERIFIED SUCCESSFULLY!")
    sys.exit(0)
else:
    sys.exit(1)
