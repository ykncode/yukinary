"""
tests/ffi/test_ffi.py — Faz 9 FFI test koşucusu
#c-lib direktifi, extern isv prototipi ve Win32 IAT çağrı testleri
"""
import argparse
import subprocess
import sys
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DEFAULT_COMPILER = os.path.join(ROOT, 'build', 'bootstrap', 'stage0.exe')
COMPILER = DEFAULT_COMPILER
TESTS_DIR = os.path.dirname(__file__)
WORK_DIR = os.path.join(ROOT, 'build', 'test-work', 'python-ffi')

PASS = 0
FAIL = 0

def run(label, src_file, expected_stdout=None, expected_exit=0):
    global PASS, FAIL
    src = os.path.join(TESTS_DIR, src_file)
    os.makedirs(WORK_DIR, exist_ok=True)
    out_exe = os.path.join(WORK_DIR, 'ffi-test.exe')
    if os.path.exists(out_exe):
        os.unlink(out_exe)

    try:
        cr = subprocess.run([COMPILER, src, out_exe],
                            capture_output=True, text=True, timeout=15)

        if cr.returncode != 0 or not os.path.exists(out_exe):
            print(f"  [FAIL] {label} — derleme hatasi:\n{cr.stdout}{cr.stderr}")
            FAIL += 1
            return

        xr = subprocess.run([out_exe], capture_output=True, text=True, timeout=10)
        ok = True
        if expected_exit is not None and xr.returncode != expected_exit:
            print(f"  [FAIL] {label} — cikis kodu {xr.returncode} (beklenen {expected_exit})")
            ok = False
        if expected_stdout and expected_stdout not in (xr.stdout + xr.stderr):
            print(f"  [FAIL] {label} — beklenen '{expected_stdout}' bulunamadi")
            print(f"         stdout={repr(xr.stdout[:120])}")
            ok = False
        if ok:
            print(f"  [PASS] {label}")
            PASS += 1
        else:
            FAIL += 1
    finally:
        try: os.unlink(out_exe)
        except: pass

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Run YKN FFI tests")
    parser.add_argument("--compiler", default=DEFAULT_COMPILER,
                        help="Compiler path, relative to the project root or absolute")
    args = parser.parse_args()
    COMPILER = os.path.abspath(args.compiler if os.path.isabs(args.compiler)
                               else os.path.join(ROOT, args.compiler))
    if not os.path.isfile(COMPILER):
        print(f"Compiler not found: {COMPILER}")
        sys.exit(1)
    print("=== Faz 9 FFI Test Süiti ===")
    # Test 1: #c-lib direktifi sözdizimi (body'li isv)
    run("01 #c-lib direktif + normal isv",
        "01_extern_syntax.ykn",
        expected_stdout="FFI SYNTAX OK")

    # Test 2: extern isv WriteFile/GetStdHandle IAT çağrısı
    run("02 extern isv + kernel32 WriteFile IAT",
        "02_kernel32_writefile.ykn",
        expected_stdout="FFI WriteFile OK")

    # Test 3: isv prototip (;) sözdizimi derlemesi
    run("03 isv prototip (noktalı virgül) sözdizimi",
        "01_extern_syntax.ykn",
        expected_stdout="FFI SYNTAX OK")

    print(f"\nSonuc: {PASS} PASS / {FAIL} FAIL / {PASS+FAIL} toplam")
    sys.exit(0 if FAIL == 0 else 1)
