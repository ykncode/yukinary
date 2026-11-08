import hashlib
import time

data_size = 16 * 1024 * 1024
buf = bytearray((i * 31 + 17) & 0xFF for i in range(data_size))

t1 = time.perf_counter()
h = hashlib.sha256(buf).hexdigest()
t2 = time.perf_counter()

elapsed_ms = (t2 - t1) * 1000.0
mb_per_sec = 16.0 / (elapsed_ms / 1000.0)

print("SHA-256 Python (hashlib / OpenSSL C wrapper) Sonucu:")
print(f"Hash: {h}")
print(f"Boyut: 16 MB | Sure: {elapsed_ms:.2f} ms | Throughput: {mb_per_sec:.2f} MB/s")

