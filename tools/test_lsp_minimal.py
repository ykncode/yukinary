import subprocess
import os
import time

def run_quick_test():
    base = os.path.dirname(os.path.abspath(__file__))
    exe = os.path.join(base, "out_lsp.exe")
    print(f"[TEST] Binary: {exe}")
    print(f"[TEST] Var mi: {os.path.exists(exe)}")
    print(f"[TEST] Boyut: {os.path.getsize(exe)} bytes")

    # ==== TEST 1: Basit calisma - argumansiz calistir (yardim yazdirir mi?)
    print("\n=== TEST 1: out_lsp.exe (argumansiz)")
    try:
        p = subprocess.Popen(
            [exe],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=0x08000000
        )
        stdout, stderr = p.communicate(timeout=5)
        print(f"  exit={p.returncode}")
        if stdout: print(f"  stdout ({len(stdout)}b): {stdout[:300]}")
        if stderr: print(f"  stderr ({len(stderr)}b): {stderr[:300]}")
    except subprocess.TimeoutExpired:
        p.kill()
        print("  TIMEOUT - 5 sn sonra olmedi, OLDURULDU")

    # ==== TEST 2: Kucuk bir YKN dosyasini derle (LSP degil normal mod)
    print("\n=== TEST 2: Normal derleme: out_lsp.exe test_lsp.ykn tmp_cikti.exe")
    test_ykn = os.path.join(base, "test_lsp.ykn")
    tmp_out = os.path.join(base, "tmp_cikti_test.exe")
    if os.path.exists(tmp_out):
        os.remove(tmp_out)
    try:
        p = subprocess.Popen(
            [exe, test_ykn, tmp_out],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=0x08000000
        )
        stdout, stderr = p.communicate(timeout=15)
        print(f"  exit={p.returncode}")
        if stdout: print(f"  stdout ({len(stdout)}b): {stdout[:500]}")
        if stderr: print(f"  stderr ({len(stderr)}b): {stderr[:500]}")
        if os.path.exists(tmp_out):
            print(f"  CIKTI URETILDI: {os.path.getsize(tmp_out)} bytes")
    except subprocess.TimeoutExpired:
        p.kill()
        print("  TIMEOUT - 15 sn sonra OLDURULDU")

if __name__ == "__main__":
    run_quick_test()
