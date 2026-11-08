import os
import subprocess
import sys
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
STAGE0 = ROOT / "app_full.exe"
STAGE1 = ROOT / "build/bootstrap_stage1.exe"
STAGE2 = ROOT / "build/stage2.exe"
STAGE3 = ROOT / "build/stage3.exe"
BOOTSTRAP_DIR = ROOT / "tests/bootstrap"

def sha256_of_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def run_bootstrap_fixpoint():
    print("============================================================")
    print("YUKINARY FAZ 28: 3-ASAMALI BOOTSTRAP VE SELF-HOST FIXPOINT")
    print("============================================================")

    # 1. Asama: Stage-0 (C++ Bootstrap derleyicisi) -> Stage-1 (Yukinary ile derlenmis derleyici)
    print("\n[ADIM 1] Stage-0 ile Stage-1 derleniyor (src/main.ykn -> build/bootstrap_stage1.exe)...")
    res1 = subprocess.run([str(STAGE0), "src/main.ykn", str(STAGE1)], cwd=ROOT, capture_output=True, text=True)
    if res1.returncode != 0:
        print("HATA: Stage-1 derlemesi basarisiz!")
        print(res1.stderr or res1.stdout)
        return False
    
    stage1_hash = sha256_of_file(STAGE1)
    print(f"  [OK] Stage-1 basariyla uretildi. SHA-256: {stage1_hash}")

    # 2. Asama: Stage-1 ile tum normatif bootstrap test senaryolarinin dogrulanmasi (01..07)
    cases = [
        ("01_hello.ykn", "01_hello.expected"),
        ("02_arithmetic.ykn", "02_arithmetic.expected"),
        ("03_control_flow.ykn", "03_control_flow.expected"),
        ("04_pointers.ykn", "04_pointers.expected"),
        ("05_structs.ykn", "05_structs.expected"),
        ("06_arrays.ykn", "06_arrays.expected"),
        ("07_functions.ykn", "07_functions.expected"),
    ]

    print("\n[ADIM 2] Stage-1 ile bootstrap dogrulama suiti test ediliyor (01..07)...")
    passed_cases = 0
    for ykn_name, exp_name in cases:
        ykn_path = BOOTSTRAP_DIR / ykn_name
        exp_path = BOOTSTRAP_DIR / exp_name
        bin_path = ROOT / f"build/test_stage1_{Path(ykn_name).stem}.exe"

        c_res = subprocess.run([str(STAGE1), str(ykn_path), str(bin_path)], cwd=ROOT, capture_output=True, text=True)
        if c_res.returncode != 0:
            print(f"  [X] DERLEME HATASI ({ykn_name}): {c_res.stderr or c_res.stdout}")
            continue

        r_res = subprocess.run([str(bin_path)], cwd=ROOT, capture_output=True, text=True)
        with open(exp_path, "r", encoding="utf-8") as f:
            expected = f.read().strip()
        actual = r_res.stdout.strip()

        if actual == expected:
            passed_cases += 1
            print(f"  [+] {ykn_name}: PASS ({actual})")
        else:
            print(f"  [-] {ykn_name}: CIKTI UYUSMAZLIGI (Beklenen={expected!r}, Alinan={actual!r})")

    print(f"\nBootstrap Senaryo Sonuclari: {passed_cases}/{len(cases)} PASS")
    if passed_cases != len(cases):
        return False

    # 3. Asama: Stage-1 -> Stage-2 (Self-Hosting Derleyici Uretimi)
    print("\n[ADIM 3] Stage-1 ile Stage-2 derleniyor (src/main.ykn -> build/stage2.exe)...")
    res2 = subprocess.run([str(STAGE1), "src/main.ykn", str(STAGE2)], cwd=ROOT, capture_output=True, text=True)
    if res2.returncode != 0:
        print("HATA: Stage-2 derlemesi basarisiz!")
        print(res2.stderr or res2.stdout)
        return False
    stage2_hash = sha256_of_file(STAGE2)
    print(f"  [OK] Stage-2 basariyla uretildi. SHA-256: {stage2_hash}")

    # 4. Asama: Stage-2 -> Stage-3 (Fixpoint Adayi Uretimi)
    print("\n[ADIM 4] Stage-2 ile Stage-3 derleniyor (src/main.ykn -> build/stage3.exe)...")
    res3 = subprocess.run([str(STAGE2), "src/main.ykn", str(STAGE3)], cwd=ROOT, capture_output=True, text=True)
    if res3.returncode != 0:
        print("HATA: Stage-3 derlemesi basarisiz!")
        print(res3.stderr or res3.stdout)
        return False
    stage3_hash = sha256_of_file(STAGE3)
    print(f"  [OK] Stage-3 basariyla uretildi. SHA-256: {stage3_hash}")

    # 5. Asama: Deterministik Kriptografik Fixpoint Dogrulamasi
    print("\n[ADIM 5] Fixpoint & Reproducibility Degerlendirmesi:")
    print(f"  - Stage-0 -> Stage-1 Derleyici Pipeline: TAMAMLANDI")
    print(f"  - Stage-1 -> Stage-2 Self-Host Pipeline: TAMAMLANDI")
    print(f"  - Stage-2 -> Stage-3 Fixpoint Pipeline:  TAMAMLANDI")
    print(f"  - Stage-2 SHA-256: {stage2_hash}")
    print(f"  - Stage-3 SHA-256: {stage3_hash}")
    
    if stage2_hash != stage3_hash:
        print("HATA: Stage-2 ve Stage-3 hash degerleri uyusmuyor! Non-determinism tespit edildi.")
        return False

    print("  - Fixpoint Durumu: %100 BIT-FOR-BIT IDENTICAL (Tam Ozdes)")
    print("BOOTSTRAP-FIXPOINT-PASS: 3-Asamali bootstrap fixpoint ve self-host basariyla dogrulandi!")
    return True

if __name__ == "__main__":
    ok = run_bootstrap_fixpoint()
    sys.exit(0 if ok else 1)
