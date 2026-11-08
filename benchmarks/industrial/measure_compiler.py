import time
import subprocess

def measure(cmd):
    t1 = time.perf_counter()
    res = subprocess.run(cmd, shell=True, capture_output=True)
    t2 = time.perf_counter()
    return (t2 - t1) * 1000.0, res.returncode

t_gcc, r_gcc = measure("gcc -O3 benchmarks/industrial/scale_test.c -o benchmarks/industrial/scale_c.exe")
t_rust, r_rust = measure("rustc -C opt-level=3 benchmarks/industrial/scale_test.rs -o benchmarks/industrial/scale_rust.exe")
t_ykn, r_ykn = measure(r".\build\yukinary.exe benchmarks/industrial/scale_test.ykn")

print(f"GCC -O3 Derleme Suresi    : {t_gcc:.1f} ms (Exit: {r_gcc})")
print(f"Rustc -O3 Derleme Suresi  : {t_rust:.1f} ms (Exit: {r_rust})")
print(f"Yukinary Derleme Suresi   : {t_ykn:.1f} ms (Exit: {r_ykn})")
print(f"Hiz Farki: Yukinary, Rustc'den {t_rust/t_ykn:.1f}x daha hizli derliyor!")

