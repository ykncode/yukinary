import time
import subprocess

def cold(cmd):
    ts = []
    for _ in range(5):
        t1 = time.perf_counter()
        subprocess.run(cmd, shell=True, capture_output=True)
        t2 = time.perf_counter()
        ts.append((t2 - t1) * 1000.0)
    return min(ts)

print(f"YKN Cold Start    : {cold('cikti.exe'):.2f} ms")
print(f"C Cold Start      : {cold('benchmarks/industrial/http_c.exe'):.2f} ms")
print(f"Rust Cold Start   : {cold('benchmarks/industrial/http_rust.exe'):.2f} ms")
print(f"Python Cold Start : {cold('python -c pass'):.2f} ms")

