import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
COMPILER = ROOT / "app_full.exe"

def test_directories():
    return [
        ROOT / "tests/optimizer",
        ROOT / "tests/generics",
        ROOT / "tests/ownership",
        ROOT / "tests/guvenlik",
        ROOT / "tests/async",
        ROOT / "tests/ag",
        ROOT / "tests/gui",
        ROOT / "tests/paket",
        ROOT / "tests/profiler",
        ROOT / "tests/veritabani",
        ROOT / "tests/debug",
    ]

def run_suite():
    print("============================================================")
    print("YUKINARY FAZ 28: NORMATIF DIL CONFORMANCE TEST SUITI")
    print("============================================================")

    total_tests = 0
    passed_tests = 0
    failed_tests = 0

    # 1. Tum .ykn testlerini tara ve derle
    test_files = []
    for d in test_directories():
        if d.exists():
            for f in d.glob("*.ykn"):
                test_files.append(f)

    print(f"Toplam tespit edilen normatif test dosyasi: {len(test_files)}")

    for tf in test_files:
        is_neg = any(k in tf.name for k in ["_neg", "error", "invalid", "unknown"])
        out_bin = ROOT / f"build/test_conf_{tf.stem}.exe"
        cmd = [str(COMPILER), str(tf), str(out_bin)]
        res = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, errors="replace")

        total_tests += 1
        if is_neg:
            # Negatif testler derlenmemeli (returncode != 0)
            if res.returncode != 0:
                passed_tests += 1
            else:
                failed_tests += 1
                print(f"  [X] NEGATIF TEST BASARISIZ (Derlendi ama hata bekleniyordu): {tf.name}")
        else:
            # Pozitif testler derlenmeli
            if res.returncode == 0:
                # Calistir ve kontrol et
                run_res = subprocess.run([str(out_bin)], cwd=ROOT, capture_output=True, text=True, errors="replace")
                expected_ret = 1 if tf.name in ("future_generic_source.ykn", "async_syntax_not_lowered.ykn") else 0
                if run_res.returncode == expected_ret:
                    passed_tests += 1
                else:
                    failed_tests += 1
                    print(f"  [X] CALISMA HATASI: {tf.name} (exit={run_res.returncode})")
            else:
                failed_tests += 1
                print(f"  [X] DERLEME HATASI: {tf.name}")

    # 2. Cross target conformance testi
    cross_script = ROOT / "tests/cross_target/test_cross_conformance.py"
    if cross_script.exists():
        total_tests += 1
        c_res = subprocess.run([sys.executable, str(cross_script)], cwd=ROOT, capture_output=True, text=True)
        if c_res.returncode == 0:
            passed_tests += 1
        else:
            failed_tests += 1
            print("  [X] CROSS TARGET TESTI BASARISIZ")

    # 3. Guvenlik suiti
    sec_script = ROOT / "tests/guvenlik/test_safety_suite.py"
    if sec_script.exists():
        total_tests += 1
        s_res = subprocess.run([sys.executable, str(sec_script)], cwd=ROOT, capture_output=True, text=True)
        if s_res.returncode == 0:
            passed_tests += 1
        else:
            failed_tests += 1
            print("  [X] GUVENLIK TESTI BASARISIZ")

    # 4. Bootstrap Fixpoint suiti
    boot_script = ROOT / "tests/bootstrap/test_bootstrap_fixpoint.py"
    if boot_script.exists():
        total_tests += 1
        b_res = subprocess.run([sys.executable, str(boot_script)], cwd=ROOT, capture_output=True, text=True)
        if b_res.returncode == 0:
            passed_tests += 1
        else:
            failed_tests += 1
            print("  [X] BOOTSTRAP FIXPOINT TESTI BASARISIZ")

    print(f"\nSONUC: Toplam: {total_tests} | PASS: {passed_tests} | FAIL: {failed_tests}")
    success = (failed_tests == 0)
    if success:
        print("CONFORMANCE-SUITE-PASS: Tum normatif dil standartlari %100 dogrulandi!")
    return success

if __name__ == "__main__":
    ok = run_suite()
    sys.exit(0 if ok else 1)
