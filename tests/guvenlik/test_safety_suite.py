import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
COMPILER = ROOT / "app_full.exe"

def run():
    print("=== YUKINARY FAZ 23: GUVENLIK & BORROW-CHECKER TEST SUITI ===")

    # 1. Negative Test: Multiple mutable borrows (Aliasing)
    p1 = subprocess.run(
        [str(COMPILER), "tests/guvenlik/01_multiple_mutable_borrow_neg.ykn", "build/test_guv_01.exe"],
        cwd=ROOT, capture_output=True, text=True
    )
    if p1.returncode != 0 and "[E041]" in p1.stdout:
        print("  [x] 01_multiple_mutable_borrow_neg: PASS (E041 BORROW_CONFLICT yakalandi)")
    else:
        print("  [ ] 01_multiple_mutable_borrow_neg: FAIL", p1.stdout)
        return False

    # 2. Negative Test: Dangling local reference (Stack Escape)
    p2 = subprocess.run(
        [str(COMPILER), "tests/guvenlik/02_dangling_local_pointer_neg.ykn", "build/test_guv_02.exe"],
        cwd=ROOT, capture_output=True, text=True
    )
    if p2.returncode != 0 and "[E046]" in p2.stdout:
        print("  [x] 02_dangling_local_pointer_neg: PASS (E046 ERR_DANGLING_POINTER yakalandi)")
    else:
        print("  [ ] 02_dangling_local_pointer_neg: FAIL", p2.stdout)
        return False

    # 3. Negative Test: Use after move
    p3 = subprocess.run(
        [str(COMPILER), "tests/guvenlik/03_use_after_move_neg.ykn", "build/test_guv_03.exe"],
        cwd=ROOT, capture_output=True, text=True
    )
    if p3.returncode != 0 and "[E040]" in p3.stdout:
        print("  [x] 03_use_after_move_neg: PASS (E040 USE_AFTER_MOVE yakalandi)")
    else:
        print("  [ ] 03_use_after_move_neg: FAIL", p3.stdout)
        return False

    # 4. Positive Test: Smart Pointers (Box, Rc, Arc)
    p4_comp = subprocess.run(
        [str(COMPILER), "tests/guvenlik/04_smart_pointers_pos.ykn", "build/test_guv_04.exe"],
        cwd=ROOT, capture_output=True, text=True
    )
    if p4_comp.returncode != 0:
        print("  [ ] 04_smart_pointers_pos DERLEME HATA:", p4_comp.stdout)
        return False

    p4_run = subprocess.run(
        [str(ROOT / "build/test_guv_04.exe")],
        cwd=ROOT, capture_output=True, text=True
    )
    if p4_run.returncode == 0 and "SMART_POINTERS_PASS" in p4_run.stdout:
        print("  [x] 04_smart_pointers_pos: PASS (SMART_POINTERS_PASS)")
    else:
        print("  [ ] 04_smart_pointers_pos CALISMA HATA:", p4_run.stdout)
        return False

    print("=== TUM GUVENLIK TESTLERI BASARIYLA TAMAMLANDI ===")
    return True

if __name__ == "__main__":
    ok = run()
    sys.exit(0 if ok else 1)

