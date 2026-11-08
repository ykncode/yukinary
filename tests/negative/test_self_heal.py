import argparse
import subprocess
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_COMPILER = os.path.join(ROOT, "build", "bootstrap", "stage0.exe")
WORK_DIR = os.path.join(ROOT, "build", "test-work", "python-self-heal")


def test_self_healing(compiler=None):
    print("=" * 60)
    print("  YUKINARY FAZ 8.6 SELF-HEALING VALIDATION SUITE")
    print("=" * 60)

    compiler = compiler or DEFAULT_COMPILER
    compiler = os.path.abspath(compiler if os.path.isabs(compiler)
                               else os.path.join(ROOT, compiler))
    if not os.path.isfile(compiler):
        print(f"[FAIL] Compiler not found: {compiler}")
        return 1

    os.makedirs(WORK_DIR, exist_ok=True)
    out1 = os.path.join(WORK_DIR, "undefined-no-heal.exe")
    out2 = os.path.join(WORK_DIR, "undefined-self-heal.exe")
    out3 = os.path.join(WORK_DIR, "multi-self-heal.exe")
    out4 = os.path.join(WORK_DIR, "type-mismatch.exe")
    out5 = os.path.join(WORK_DIR, "invalid-return.exe")

    passed = 0
    total = 5

    # Case 1: 01_undefined_var WITHOUT self-heal -> MUST FAIL
    p1 = subprocess.run([compiler, os.path.join(ROOT, "tests", "negative", "01_undefined_var.ykn"), out1], capture_output=True, text=True)
    if p1.returncode != 0 and not os.path.exists(out1):
        print("[PASS] Undefined var without --self-heal: Correctly aborted")
        passed += 1
    else:
        print("[FAIL] Undefined var without --self-heal should have aborted!")
        if os.path.exists(out1):
            os.remove(out1)

    # Case 2: 01_undefined_var WITH --self-heal -> MUST PASS
    p2 = subprocess.run([compiler, "--self-heal", os.path.join(ROOT, "tests", "negative", "01_undefined_var.ykn"), out2], capture_output=True, text=True)
    if p2.returncode == 0 and os.path.exists(out2):
        r2 = subprocess.run([out2], capture_output=True, text=True)
        if r2.returncode == 0 and "[AI] Self-heal" in p2.stdout:
            print("[PASS] Undefined var with --self-heal: Successfully healed and executed")
            passed += 1
        else:
            print(f"[FAIL] Healed binary execution failed with code {r2.returncode}")
        os.remove(out2)
    else:
        print(f"[FAIL] Undefined var with --self-heal failed to compile: {p2.stdout}")

    # Case 3: 04_multi_undefined_var WITH --kendini-onar -> MUST PASS (2 heals)
    p3 = subprocess.run([compiler, "--kendini-onar", os.path.join(ROOT, "tests", "negative", "04_multi_undefined_var.ykn"), out3], capture_output=True, text=True)
    if p3.returncode == 0 and os.path.exists(out3):
        r3 = subprocess.run([out3], capture_output=True, text=True)
        if r3.returncode == 0 and "MULTI-SELF-HEAL-PASS" in r3.stdout:
            print("[PASS] Multi undefined var with --kendini-onar: Healed multi symbols and passed")
            passed += 1
        else:
            print(f"[FAIL] Multi-healed binary output mismatch: {r3.stdout}")
        os.remove(out3)
    else:
        print(f"[FAIL] Multi undefined var compilation failed: {p3.stdout}")

    # Case 4: 02_type_mismatch WITH --self-heal -> MUST STILL FAIL (Safety barrier)
    p4 = subprocess.run([compiler, "--self-heal", os.path.join(ROOT, "tests", "negative", "02_type_mismatch.ykn"), out4], capture_output=True, text=True)
    if p4.returncode != 0 and not os.path.exists(out4):
        print("[PASS] Type mismatch with --self-heal: Correctly rejected (unhealable safety barrier)")
        passed += 1
    else:
        print("[FAIL] Type mismatch should not be healed!")
        if os.path.exists(out4):
            os.remove(out4)

    # Case 5: 03_invalid_return WITH --self-heal -> MUST STILL FAIL (Safety barrier)
    if os.path.exists(out5): os.remove(out5)
    p5 = subprocess.run([compiler, "--self-heal", os.path.join(ROOT, "tests", "negative", "03_invalid_return.ykn"), out5], capture_output=True, text=True)
    if p5.returncode != 0 and not os.path.exists(out5):
        print("[PASS] Invalid return with --self-heal: Correctly rejected (unhealable safety barrier)")
        passed += 1
    else:
        print("[FAIL] Invalid return should not be healed!")
        if os.path.exists(out5):
            os.remove(out5)

    print("-" * 60)
    print(f"Results: {passed}/{total} tests passed")
    if passed == total:
        print("ALL SELF-HEALING TESTS PASSED! [OK]")
        return 0
    else:
        print("SOME SELF-HEALING TESTS FAILED!")
        return 1

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run YKN self-healing tests")
    parser.add_argument("--compiler", default=DEFAULT_COMPILER,
                        help="Compiler path, relative to the project root or absolute")
    args = parser.parse_args()
    sys.exit(test_self_healing(args.compiler))
